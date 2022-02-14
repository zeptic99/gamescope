// Try to figure out when vblank is and notify steamcompmgr to render some time before it

#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <condition_variable>

#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include "gpuvis_trace_utils.h"

#include "vblankmanager.hpp"
#include "steamcompmgr.hpp"
#include "wlserver.hpp"
#include "main.hpp"

static int g_vblankPipe[2];

std::atomic<uint64_t> g_lastVblank;

// 3ms by default -- a good starting value.
const uint64_t g_uStartingDrawTime = 3'000'000;

// This is the last time a draw took.
std::atomic<uint64_t> g_uVblankDrawTimeNS = { g_uStartingDrawTime };

// 1.3ms by default. (g_uDefaultMinVBlankTime)
// This accounts for some time we cannot account for (which (I think) is the drm_commit -> triggering the pageflip)
// It would be nice to make this lower if we can find a way to track that effectively
// Perhaps the missing time is spent elsewhere, but given we track from the pipe write
// to after the return from `drm_commit` -- I am very doubtful.
uint64_t g_uMinVblankTime = g_uDefaultMinVBlankTime;

// Tuneable
// 0.3ms by default. (g_uDefaultVBlankRedZone)
// This is the leeway we always apply to our buffer.
uint64_t g_uVblankDrawBufferRedZoneNS = g_uDefaultVBlankRedZone;

// Tuneable
// 93% by default. (g_uVBlankRateOfDecayPercentage)
// The rate of decay (as a percentage) of the rolling average -> current draw time
uint64_t g_uVBlankRateOfDecayPercentage = g_uDefaultVBlankRateOfDecayPercentage;

const uint64_t g_uVBlankRateOfDecayMax = 1000;

static std::atomic<uint64_t> g_uRollingMaxDrawTime = { g_uStartingDrawTime };

//#define VBLANK_DEBUG

void vblankThreadRun( void )
{
	pthread_setname_np( pthread_self(), "gamescope-vblk" );

	// Start off our average with our starting draw time.
	uint64_t rollingMaxDrawTime = g_uStartingDrawTime;

	const uint64_t range = g_uVBlankRateOfDecayMax;
	while ( true )
	{
		const uint64_t alpha = g_uVBlankRateOfDecayPercentage;
		const int refresh = g_nNestedRefresh ? g_nNestedRefresh : g_nOutputRefresh;

		const uint64_t nsecInterval = 1'000'000'000ul / refresh;
		const uint64_t drawTime = g_uVblankDrawTimeNS;

		// This is a rolling average when drawTime < rollingMaxDrawTime,
		// and a a max when drawTime > rollingMaxDrawTime.
		// This allows us to deal with spikes in the draw buffer time very easily.
		// eg. if we suddenly spike up (eg. because of test commits taking a stupid long time),
		// we will then be able to deal with spikes in the long term, even if several commits after
		// we get back into a good state and then regress again.

		// If we go over half of our deadzone, be more defensive about things.
		if ( int64_t(drawTime) - int64_t(g_uVblankDrawBufferRedZoneNS / 2) > int64_t(rollingMaxDrawTime) )
			rollingMaxDrawTime = drawTime;
		else
			rollingMaxDrawTime = ( ( alpha * rollingMaxDrawTime ) + ( range - alpha ) * drawTime ) / range;

		// If we need to offset for our draw more than half of our vblank, something is very wrong.
		// Clamp our max time to half of the vblank if we can.
		rollingMaxDrawTime = std::min( rollingMaxDrawTime, nsecInterval - g_uVblankDrawBufferRedZoneNS );

		g_uRollingMaxDrawTime = rollingMaxDrawTime;

		uint64_t offset = rollingMaxDrawTime + g_uVblankDrawBufferRedZoneNS;

#ifdef VBLANK_DEBUG
		// Debug stuff for logging missed vblanks
		static uint64_t vblankIdx = 0;
		static uint64_t lastDrawTime = g_uVblankDrawTimeNS;
		static uint64_t lastOffset = g_uVblankDrawTimeNS + g_uVblankDrawBufferRedZoneNS;

		if ( vblankIdx++ % 300 == 0 || drawTime > lastOffset )
		{
			if ( drawTime > lastOffset )
				fprintf( stderr, " !! missed vblank " );

			fprintf( stderr, "redZone: %.2fms decayRate: %lu%% - rollingMaxDrawTime: %.2fms lastDrawTime: %.2fms lastOffset: %.2fms - drawTime: %.2fms offset: %.2fms\n",
				g_uVblankDrawBufferRedZoneNS / 1'000'000.0,
				g_uVBlankRateOfDecayPercentage,
				rollingMaxDrawTime / 1'000'000.0,
				lastDrawTime / 1'000'000.0,
				lastOffset / 1'000'000.0,
				drawTime / 1'000'000.0,
				offset / 1'000'000.0 );
		}

		lastDrawTime = drawTime;
		lastOffset = offset;
#endif

		uint64_t lastVblank = g_lastVblank - offset;

		uint64_t now = get_time_in_nanos();
		uint64_t targetPoint = lastVblank + nsecInterval;
		while ( targetPoint < now )
			targetPoint += nsecInterval;

		sleep_until_nanos( targetPoint );

		// give the time of vblank to steamcompmgr
		uint64_t vblanktime = get_time_in_nanos();

		ssize_t ret = write( g_vblankPipe[ 1 ], &vblanktime, sizeof( vblanktime ) );
		if ( ret <= 0 )
		{
			perror( "vblankmanager: write failed" );
		}
		else
		{
			gpuvis_trace_printf( "sent vblank" );
		}
		
		// Get on the other side of it now
		sleep_for_nanos( offset + 1'000'000 );
	}
}

int vblank_init( void )
{
	if ( pipe2( g_vblankPipe, O_CLOEXEC | O_NONBLOCK ) != 0 )
	{
		perror( "vblankmanager: pipe failed" );
		return -1;
	}
	
	g_lastVblank = get_time_in_nanos();

	std::thread vblankThread( vblankThreadRun );
	vblankThread.detach();

	return g_vblankPipe[ 0 ];
}

void vblank_mark_possible_vblank( uint64_t nanos )
{
	g_lastVblank = nanos;
}

// fps limit manager

static std::mutex g_TargetFPSMutex;
static std::condition_variable g_TargetFPSCondition;
static int g_nFpsLimitTargetFPS = 0;

bool steamcompmgr_fpslimit_release_commit( int consecutive_missed_frame_count );
void steamcompmgr_fpslimit_release_all();
void steamcompmgr_send_frame_done_to_focus_window();

// Dump some stats.
#define FPS_LIMIT_DEBUG

// 1.80ms for the app's deadzone to account for varying GPU clocks, other variances, etc
uint64_t g_uFPSLimiterRedZoneNS = 1'800'000;

// 1.0ms as the minimum time we consider a 'frame' for scheduling purposes.
// If the app is running at 1000s of FPS, its probably going to vary a lot.
// so best to keep this stable at some minimum.
uint64_t g_uMinFPSLimiter = 1'000'000;

bool g_bFPSLimitThreadRun = true;

extern bool g_bLowLatency;

uint64_t g_uFPSLimitLastFullFrameTime = 0;
uint64_t g_uFPSLimitDoneToDoneTime = 0;

void fpslimitThreadRun( void )
{
	pthread_setname_np( pthread_self(), "gamescope-fps" );

	uint64_t lastCommitReleased = get_time_in_nanos();
	const uint64_t range = g_uVBlankRateOfDecayMax;
	uint64_t rollingMaxFrameTime = g_uStartingDrawTime;
	uint64_t vblank = 0;
	int consecutive_missed_frame_count = 0;
	bool last_frame_was_late = false;
	g_uFPSLimitLastFullFrameTime = get_time_in_nanos();
	uint64_t lastFullFrameTime = 0;
	uint64_t donetodonetime = 0;
	bool isLatent = false;
	while ( true )
	{
		int nTargetFPS;
		uint64_t targetInterval;
		bool no_frame = false;

		{
			std::unique_lock<std::mutex> lock( g_TargetFPSMutex );

			if ( !g_bFPSLimitThreadRun )
				return;

			nTargetFPS = g_nFpsLimitTargetFPS;
			if ( nTargetFPS == 0 )
			{
				g_TargetFPSCondition.wait(lock);
			}
			else
			{
				targetInterval = 1'000'000'000ul / nTargetFPS;
				auto wait_time = std::chrono::nanoseconds(int64_t(lastCommitReleased + targetInterval) - get_time_in_nanos());
				if ( wait_time > std::chrono::nanoseconds(0) )
				{
					no_frame = g_TargetFPSCondition.wait_for(lock, std::chrono::nanoseconds(wait_time)) == std::cv_status::timeout;
				}
				else
				{
					no_frame = true;
				}
			}
			nTargetFPS = g_nFpsLimitTargetFPS;
			lastFullFrameTime = g_uFPSLimitLastFullFrameTime;
			donetodonetime = g_uFPSLimitDoneToDoneTime;
		}

		const int refresh = g_nNestedRefresh ? g_nNestedRefresh : g_nOutputRefresh;
		const uint64_t vblankInterval = 1'000'000'000ul / refresh;

		// If the last frame was late, and this isn't a late frame
		// ignore it, as this is that late frame.
		if ( !last_frame_was_late || no_frame )
		{
			if ( no_frame )
				consecutive_missed_frame_count++;
			else
				consecutive_missed_frame_count = 0;

			if ( nTargetFPS )
			{
				targetInterval = 1'000'000'000ul / nTargetFPS;

				// Check if we are unaligned or not, as to whether
				// we call frame callbacks from this thread instead of steamcompmgr based
				// on vblank count.
				bool useFrameCallbacks = fpslimit_use_frame_callbacks_for_focus_window( nTargetFPS, 0 );

				uint64_t t0 = lastCommitReleased;
				uint64_t t1 = lastFullFrameTime;
			
				// Not the actual frame time of the game
				// this is the time of the amount of work a 'frame' has done.
				uint64_t frameTime = t1 - t0;
				// If we didn't get a frame, set our frame time as the target interval.
				if ( no_frame || !frameTime )
				{
		#ifdef FPS_LIMIT_DEBUG
					fprintf( stderr, "no frame\n" );
		#endif
					frameTime = targetInterval;
				}

				// Currently,
				// Only affect rolling max frame time by 0.07%
				// Tends to be much more varied than the vblank timings.
				// Try to be much more defensive about it.
				//
				// Do we want something better here? Right now, because this moves around all the time
				// sometimes we can see judder in the mangoapp frametime graph when gpu clocks are changing around
				// in the downtime when we aren't rendering as it measures done->done time,
				// rather than present->present time, and done->done time changes as we move buffers around.
				// Maybe we want to tweak this alpha value to like 99.something% or change this rolling max to something even more defensive
				// to keep a more consistent latency. However, I also cannot feel this judder given how small it is, so maybe it doesn't matter?
				// We can tune this later by tweaking alpha + range anyway...
				// If we go over half of our deadzone, be more defensive about things.
				if ( int64_t(frameTime) - int64_t(g_uFPSLimiterRedZoneNS * 2 / 3) > int64_t(rollingMaxFrameTime) )
					rollingMaxFrameTime = frameTime;
				else
				{
					const uint64_t alphaUp = 980;
					const uint64_t alphaDown = 993;
					const uint64_t alpha = frameTime > rollingMaxFrameTime ? alphaUp : alphaDown;
					rollingMaxFrameTime = ( ( alpha * rollingMaxFrameTime ) + ( range - alpha ) * frameTime ) / range;
				}

				rollingMaxFrameTime = std::min( rollingMaxFrameTime, targetInterval + targetInterval / 2 );

				int64_t targetPoint;
				int64_t sleepyTime = targetInterval;
				uint64_t rollingMaxDrawTime = g_uRollingMaxDrawTime.load();
				uint64_t latency = 0;

				if ( refresh % nTargetFPS == 0 )
				{
					// Take the min of it to the target interval - the fps limiter redzone
					// so that we don't go over the target interval - expected vblank time
					sleepyTime -= std::max( rollingMaxFrameTime, g_uMinFPSLimiter );
					sleepyTime -= int64_t(g_uFPSLimiterRedZoneNS);
					// Don't roll back before current vblank
					// based on varying frame time otherwise we can become divergent
					// if these value change how we do not expect and get stuck in a feedback loop.
					const int64_t min_sleepy_time = 0;//-int64_t(targetInterval) / 2;
					if ( !g_bLowLatency )
					{
						sleepyTime = min_sleepy_time;
					}
					sleepyTime = std::max<int64_t>( sleepyTime, min_sleepy_time );
					sleepyTime -= int64_t(std::max<uint64_t>(rollingMaxDrawTime, g_uDefaultMinVBlankTime));
					sleepyTime -= int64_t(g_uVblankDrawBufferRedZoneNS);

					uint64_t last_vblank = vblank;

					vblank = ( ( t1 / targetInterval ) * targetInterval ) + ( g_lastVblank.load() % vblankInterval );
					// Make sure we are on the other side of the last vblank.
					while ( vblank < last_vblank + targetInterval / 2 + 1'000'000 )
						vblank += targetInterval;

					targetPoint = int64_t(vblank) + sleepyTime;
					latency = -(sleepyTime - int64_t(targetInterval));
					if ( isLatent )
						latency = 0; // invalid info, record as 0.
				}
				else
				{
					sleepyTime -= int64_t(frameTime);
					targetPoint = int64_t(t1) + sleepyTime;
					latency = uint64_t(~0ull);
				}

				if ( !no_frame )
				{
					mangoapp_update( isLatent ? donetodonetime : targetInterval, frameTime, latency );
				}

		#ifdef FPS_LIMIT_DEBUG
				fprintf( stderr, "Sleeping from %lu to %ld (%ld - %.2fms) to reach %d fps - rollingMaxDrawTime: %.2fms vblank: %lu sleepytime: %.2fms rollingMaxFrameTime: %.2fms frametime: %.2fms\n", t1, targetPoint, targetPoint - int64_t(t1), (targetPoint - int64_t(t1)) / 1'000'000.0, nTargetFPS, rollingMaxDrawTime / 1'000'000.0, vblank, sleepyTime  / 1'000'000.0, rollingMaxFrameTime / 1'000'000.0, frameTime  / 1'000'000.0 );
		#endif


				sleep_until_nanos( targetPoint );
				lastCommitReleased = get_time_in_nanos();
				isLatent = steamcompmgr_fpslimit_release_commit( consecutive_missed_frame_count );

				// If we aren't vblank aligned, nudge ourselves to process done commits now.
				if ( !useFrameCallbacks )
				{
					steamcompmgr_send_frame_done_to_focus_window();
					nudge_steamcompmgr();
				}
			}
		}
		else if ( last_frame_was_late && !no_frame )
		{
			if ( nTargetFPS )
			{
				mangoapp_update( donetodonetime, donetodonetime, ( refresh % nTargetFPS == 0 ) ? 0 : uint64_t(~0ull) );
			}
		}

		last_frame_was_late = no_frame;
	}
}

void fpslimit_init( void )
{
	std::thread fpslimitThread( fpslimitThreadRun );
	fpslimitThread.detach();
}

void fpslimit_shutdown( void )
{
	{
		std::unique_lock<std::mutex> lock(g_TargetFPSMutex);
		g_bFPSLimitThreadRun = false;
	}

	g_TargetFPSCondition.notify_all();
}

void fpslimit_mark_frame( uint64_t frametime )
{
	uint64_t now = get_time_in_nanos();
	{
		std::unique_lock<std::mutex> lock(g_TargetFPSMutex);
		g_uFPSLimitLastFullFrameTime = now;
		g_uFPSLimitDoneToDoneTime = frametime;
	}
	g_TargetFPSCondition.notify_all();
}

bool fpslimit_use_frame_callbacks_for_focus_window( int nTargetFPS, int nVBlankCount ) 
{
	// Avoids a race incase the surface changes
	// We don't use this anymore since we force no-fifo
	return true;
#if 0
	if ( !nTargetFPS )
		return true;

	const int refresh = g_nNestedRefresh ? g_nNestedRefresh : g_nOutputRefresh;
	if ( refresh % nTargetFPS == 0 )
	{
		// Aligned, limit based on vblank count.
		return nVBlankCount % ( refresh / nTargetFPS );
	}
	else
	{
		// Unaligned from VBlank, never use frame callbacks on SteamCompMgr thread.
		// call them from fpslimit
		return false;
	}
#endif
}

// Called from steamcompmgr thread
void fpslimit_set_target( int nTargetFPS )
{
	{
		std::unique_lock<std::mutex> lock(g_TargetFPSMutex);
		g_nFpsLimitTargetFPS = nTargetFPS;
	}

	g_TargetFPSCondition.notify_all();
}
