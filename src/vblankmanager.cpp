// Try to figure out when vblank is and notify steamcompmgr to render some time before it

#include <thread>
#include <vector>
#include <chrono>

#include "X11/Xlib.h"
#include "assert.h"

#include "vblankmanager.hpp"
#include "steamcompmgr.hpp"
#include "wlserver.h"
#include "main.hpp"

static Display *g_nestedDpy;
static XEvent repaintMsg;

void vblankThreadRun( void )
{
	while ( true )
	{
		int usec = 1.0 / g_nOutputRefresh * 1000.0 * 1000.0;
		std::chrono::system_clock::time_point timePoint =
		std::chrono::system_clock::now() + std::chrono::microseconds( usec );
		
		std::this_thread::sleep_until( timePoint );
		
		XSendEvent( g_nestedDpy , DefaultRootWindow( g_nestedDpy ), True, SubstructureRedirectMask, &repaintMsg);
		XFlush( g_nestedDpy );
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

	std::thread vblankThread( vblankThreadRun );
	vblankThread.detach();
}

void vblank_mark_possible_vblank( void )
{
}
