// For the nested case, reads input from the SDL window and send to wayland

#include <thread>
#include <mutex>

#include <signal.h>
#include <linux/input-event-codes.h>

#include <SDL.h>

#include "inputsdl.hpp"
#include "wlserver.h"

std::mutex g_SDLInitLock;

static inline int SDLButtonToLinuxButton( int SDLButton )
{
	switch ( SDLButton )
	{
		case SDL_BUTTON_LEFT: return BTN_LEFT;
		case SDL_BUTTON_MIDDLE: return BTN_MIDDLE;
		case SDL_BUTTON_RIGHT: return BTN_RIGHT;
		case SDL_BUTTON_X1: return BTN_FORWARD;
		case SDL_BUTTON_X2: return BTN_BACK;
		default: return 0;
	}
}

void inputSDLThreadRun( void )
{
	// :/
	signal(SIGUSR1, SIG_IGN);

	SDL_Event event;
	
	SDL_Init( SDL_INIT_VIDEO | SDL_INIT_EVENTS );
	
	g_SDLInitLock.unlock();
	
	while( SDL_WaitEvent( &event ) )
	{
		switch( event.type )
		{
			case SDL_MOUSEMOTION:
				wlserver_lock();
				wlserver_mousemotion( event.motion.x, event.motion.y, event.motion.timestamp );
				wlserver_unlock();
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				wlserver_lock();
				wlserver_mousebutton( SDLButtonToLinuxButton( event.button.button ),
									  event.button.state == SDL_PRESSED,
									  event.button.timestamp );
				wlserver_unlock();
				break;
			case SDL_MOUSEWHEEL:
				wlserver_lock();
				wlserver_mousewheel( -event.wheel.x, -event.wheel.y, event.wheel.timestamp );
				wlserver_unlock();
				break;
			default:
				break;
		}
		

	}
}

bool inputsdl_init( void )
{
	g_SDLInitLock.lock();

	std::thread inputSDLThread( inputSDLThreadRun );
	inputSDLThread.detach();
	
	// When this returns SDL_Init should be over
	g_SDLInitLock.lock();

	return true;
}
