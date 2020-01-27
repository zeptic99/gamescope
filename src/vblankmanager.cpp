// Try to figure out when vblank is and notify steamcompmgr to render some time before it

#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

#include "X11/Xlib.h"
#include "assert.h"

#include "gpuvis_trace_utils.h"

#include "vblankmanager.hpp"
#include "steamcompmgr.hpp"
#include "wlserver.h"
#include "main.hpp"

static Display *g_nestedDpy;
static XEvent repaintMsg;

std::mutex g_vblankLock;
std::chrono::time_point< std::chrono::system_clock > g_lastVblank;

float g_flVblankDrawBufferMS = 5.0;

void vblankThreadRun( void )
{
	while ( true )
	{
		std::chrono::time_point< std::chrono::system_clock > lastVblank;
		int usecInterval = 1.0 / g_nOutputRefresh * 1000.0 * 1000.0;
		
		{
			std::unique_lock<std::mutex> lock( g_vblankLock );
			lastVblank = g_lastVblank;
		}
		
		lastVblank -= std::chrono::microseconds( (int)(g_flVblankDrawBufferMS * 1000) );

		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		std::chrono::system_clock::time_point targetPoint = lastVblank + std::chrono::microseconds( usecInterval );
		
		while ( targetPoint < now )
		{
			targetPoint += std::chrono::microseconds( usecInterval );
		}
		
		std::this_thread::sleep_until( targetPoint );
		
		XSendEvent( g_nestedDpy , DefaultRootWindow( g_nestedDpy ), True, SubstructureRedirectMask, &repaintMsg);
		XFlush( g_nestedDpy );
		
		gpuvis_trace_printf( "sent vblank\n" );
		
		// Get on the other side of it now
		std::this_thread::sleep_for( std::chrono::microseconds( (int)((g_flVblankDrawBufferMS + 1.0) * 1000) ) );
	}
}

void vblank_init( void )
{
	g_nestedDpy = XOpenDisplay( wlserver_get_nested_display() );
	assert( g_nestedDpy != nullptr );
		
	repaintMsg.xclient.type = ClientMessage;
	repaintMsg.xclient.window = DefaultRootWindow( g_nestedDpy );
	repaintMsg.xclient.format = 32;
	repaintMsg.xclient.data.l[0] = 24;
	repaintMsg.xclient.data.l[1] = 8;
	
	g_lastVblank = std::chrono::system_clock::now();

	std::thread vblankThread( vblankThreadRun );
	vblankThread.detach();
}

void vblank_mark_possible_vblank( void )
{
	std::unique_lock<std::mutex> lock( g_vblankLock );

	g_lastVblank = std::chrono::system_clock::now();
}
