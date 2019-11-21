#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>
#include "wlserver.h"

struct wlserver_t wlserver;

int rootston_init(int argc, char **argv) {
	bool bIsDRM = False;
	
	if ( getenv("DISPLAY") == NULL )
	{
		bIsDRM = True;
	}
	
	wlr_log_init(WLR_DEBUG, NULL);
	wlserver.wl_display = wl_display_create();
	
	wlserver.wlr.session = ( bIsDRM == True ) ? wlr_session_create(wlserver.wl_display) : NULL;
	
	wlserver.wl_event_loop = wl_display_get_event_loop(wlserver.wl_display);	
	
	wlserver.wlr.backend = wlr_multi_backend_create(wlserver.wl_display);
	
	assert(wlserver.wl_display && wlserver.wl_event_loop && wlserver.wlr.backend);
	assert( !bIsDRM || wlserver.wlr.session );

	struct wlr_backend* headless_backend = wlr_headless_backend_create(wlserver.wl_display, NULL);
	if (headless_backend == NULL) {
		wlr_log(WLR_ERROR, "could not start headless_backend");
		return 1;
	}
	wlr_multi_backend_add(wlserver.wlr.backend, headless_backend);
	
	wlserver.wlr.output = wlr_headless_add_output( headless_backend, 1280, 720 );
	
	wlr_output_create_global( wlserver.wlr.output );
	
	if ( bIsDRM == True )
	{
		struct wlr_backend *libinput_backend = wlr_libinput_backend_create(wlserver.wl_display, wlserver.wlr.session);
		if (libinput_backend == NULL) {
			wlr_log(WLR_ERROR, "could not start libinput_backend");
			return 1;
		}
		wlr_multi_backend_add(wlserver.wlr.backend, libinput_backend);
	}
	else
	{
		wlserver.wlr.keyboard = wlr_headless_add_input_device( headless_backend, WLR_INPUT_DEVICE_KEYBOARD );
		wlserver.wlr.pointer = wlr_headless_add_input_device( headless_backend, WLR_INPUT_DEVICE_POINTER );
	}
	
	wlserver.wlr.renderer = wlr_backend_get_renderer(wlserver.wlr.backend);
	
	assert(wlserver.wlr.renderer);

	wlr_renderer_init_wl_display(wlserver.wlr.renderer, wlserver.wl_display);

	wlserver.wlr.compositor = wlr_compositor_create(wlserver.wl_display, wlserver.wlr.renderer);
	
	wlserver.wlr.xwayland = wlr_xwayland_create(wlserver.wl_display, wlserver.wlr.compositor, False);
	
	setenv("DISPLAY", wlserver.wlr.xwayland->display_name, true);

	const char *socket = wl_display_add_socket_auto(wlserver.wl_display);
	if (!socket) {
		wlr_log_errno(WLR_ERROR, "Unable to open wayland socket");
		wlr_backend_destroy(wlserver.wlr.backend);
		return 1;
	}

	wlr_log(WLR_INFO, "Running compositor on wayland display '%s'", socket);
	setenv("_WAYLAND_DISPLAY", socket, true);

	if (!wlr_backend_start(wlserver.wlr.backend)) {
		wlr_log(WLR_ERROR, "Failed to start backend");
		wlr_backend_destroy(wlserver.wlr.backend);
		wl_display_destroy(wlserver.wl_display);
		return 1;
	}

	setenv("WAYLAND_DISPLAY", socket, true);

	wlserver.wlr.seat = wlr_seat_create(wlserver.wl_display, "seat0");
	wlr_xwayland_set_seat(wlserver.wlr.xwayland, wlserver.wlr.seat);

	return 0;
}

int rootston_run(void)
{
	wl_display_run(wlserver.wl_display);
	// We need to shutdown Xwayland before disconnecting all clients, otherwise
	// wlroots will restart it automatically.
	wlr_xwayland_destroy(wlserver.wlr.xwayland);
	wl_display_destroy_clients(wlserver.wl_display);
	wl_display_destroy(wlserver.wl_display);
	return 0;
}
