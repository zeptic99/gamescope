// For the nested case, reads input from the SDL window and send to wayland

#pragma once

#ifndef C_SIDE
extern "C" {
#endif

bool inputsdl_init( void );

#ifndef C_SIDE
}
#endif
