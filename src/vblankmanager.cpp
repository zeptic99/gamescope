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
#include "drm.hpp"

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

		// The redzone is relative to 60Hz, scale it by our
		// target refresh so we don't miss submitting for vblank in DRM.
		// (This fixes 4K@30Hz screens)
		const uint64_t nsecToSec = 1'000'000'000ul;
		const drm_screen_type screen_type = drm_get_screen_type();
		const uint64_t redZone = screen_type == DRM_SCREEN_TYPE_INTERNAL
			? g_uVblankDrawBufferRedZoneNS
			: ( g_uVblankDrawBufferRedZoneNS * 60 * nsecToSec ) / ( refresh * nsecToSec );

		// This is a rolling average when drawTime < rollingMaxDrawTime,
		// and a a max when drawTime > rollingMaxDrawTime.
		// This allows us to deal with spikes in the draw buffer time very easily.
		// eg. if we suddenly spike up (eg. because of test commits taking a stupid long time),
		// we will then be able to deal with spikes in the long term, even if several commits after
		// we get back into a good state and then regress again.

		// If we go over half of our deadzone, be more defensive about things.
		if ( int64_t(drawTime) - int64_t(redZone / 2) > int64_t(rollingMaxDrawTime) )
			rollingMaxDrawTime = drawTime;
		else
			rollingMaxDrawTime = ( ( alpha * rollingMaxDrawTime ) + ( range - alpha ) * drawTime ) / range;

		// If we need to offset for our draw more than half of our vblank, something is very wrong.
		// Clamp our max time to half of the vblank if we can.
		rollingMaxDrawTime = std::min( rollingMaxDrawTime, nsecInterval - redZone );

		g_uRollingMaxDrawTime = rollingMaxDrawTime;

		uint64_t offset = rollingMaxDrawTime + redZone;

#ifdef VBLANK_DEBUG
		// Debug stuff for logging missed vblanks
		static uint64_t vblankIdx = 0;
		static uint64_t lastDrawTime = g_uVblankDrawTimeNS;
		static uint64_t lastOffset = g_uVblankDrawTimeNS + redZone;

		if ( vblankIdx++ % 300 == 0 || drawTime > lastOffset )
		{
			if ( drawTime > lastOffset )
				fprintf( stderr, " !! missed vblank " );

			fprintf( stderr, "redZone: %.2fms decayRate: %lu%% - rollingMaxDrawTime: %.2fms lastDrawTime: %.2fms lastOffset: %.2fms - drawTime: %.2fms offset: %.2fms\n",
				redZone / 1'000'000.0,
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
