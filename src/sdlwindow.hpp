// For the nested case, manages SDL window for input/output

#pragma once

#include <SDL.h>
#include <SDL_vulkan.h>

bool sdlwindow_init( void );

// called from other threads with interesting things have happened with clients that might warrant updating the nested window
void sdlwindow_update( void );

extern SDL_Window *g_SDLWindow;
