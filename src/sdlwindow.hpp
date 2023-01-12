// For the nested case, manages SDL window for input/output

#pragma once

#include <SDL.h>
#include <SDL_vulkan.h>

bool sdlwindow_init( void );

void sdlwindow_update( void );
void sdlwindow_title( const char* title );

// called from other threads with interesting things have happened with clients that might warrant updating the nested window
void sdlwindow_visible( bool bVisible );
void sdlwindow_grab( bool bGrab );
void sdlwindow_cursor(void* pixels, uint32_t width, uint32_t height, uint32_t xhot, uint32_t yhot);

extern SDL_Window *g_SDLWindow;
