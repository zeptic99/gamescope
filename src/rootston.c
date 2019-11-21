#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/libinput.h>
#include <wlr/config.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>
#include "config.h"
#include "server.h"

struct roots_server server = { 0 };

struct wlr_input_device *keyboard;
struct wlr_input_device *pointer;

int rootston_init(int argc, char **argv) {
	bool bIsDRM = False;
	
	if ( getenv("DISPLAY") == NULL )
	{
		bIsDRM = True;
	}
	
	wlr_log_init(WLR_DEBUG, NULL);
	server.config = roots_config_create_from_args(argc, argv);
	server.wl_display = wl_display_create();
	
	server.wlr_session = ( bIsDRM == True ) ? wlr_session_create(server.wl_display) : NULL;
	
	server.wl_event_loop = wl_display_get_event_loop(server.wl_display);	
	
	server.backend = wlr_multi_backend_create(server.wl_display);
	
	assert(server.config && server.wl_display && server.wl_event_loop && server.backend);
	assert( !bIsDRM || server.wlr_session );

	struct wlr_backend* headless_backend = wlr_headless_backend_create(server.wl_display, NULL);
	if (headless_backend == NULL) {
		wlr_log(WLR_ERROR, "could not start headless_backend");
		return 1;
	}
	wlr_multi_backend_add(server.backend, headless_backend);
	
	server.wlr_output = wlr_headless_add_output( headless_backend, 1280, 720 );
	
	wlr_output_create_global( server.wlr_output );
	
	if ( bIsDRM == True )
	{
		struct wlr_backend *libinput_backend = wlr_libinput_backend_create(server.wl_display, server.wlr_session);
		if (libinput_backend == NULL) {
			wlr_log(WLR_ERROR, "could not start libinput_backend");
			return 1;
		}
		wlr_multi_backend_add(server.backend, libinput_backend);
	}
	else
	{
		keyboard = wlr_headless_add_input_device( headless_backend, WLR_INPUT_DEVICE_KEYBOARD );
		pointer = wlr_headless_add_input_device( headless_backend, WLR_INPUT_DEVICE_POINTER );
	}
	
	server.renderer = wlr_backend_get_renderer(server.backend);
	
	assert(server.renderer);

	wlr_renderer_init_wl_display(server.renderer, server.wl_display);

	server.compositor = wlr_compositor_create(server.wl_display, server.renderer);
	
	server.xwayland = wlr_xwayland_create(server.wl_display, server.compositor, False);
	
	setenv("DISPLAY", server.xwayland->display_name, true);

	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		wlr_log_errno(WLR_ERROR, "Unable to open wayland socket");
		wlr_backend_destroy(server.backend);
		return 1;
	}

	wlr_log(WLR_INFO, "Running compositor on wayland display '%s'", socket);
	setenv("_WAYLAND_DISPLAY", socket, true);

	if (!wlr_backend_start(server.backend)) {
		wlr_log(WLR_ERROR, "Failed to start backend");
		wlr_backend_destroy(server.backend);
		wl_display_destroy(server.wl_display);
		return 1;
	}

	setenv("WAYLAND_DISPLAY", socket, true);

	server.seat = wlr_seat_create(server.wl_display, ROOTS_CONFIG_DEFAULT_SEAT_NAME);
	wlr_xwayland_set_seat(server.xwayland, server.seat);

	if (server.config->startup_cmd != NULL) {
		const char *cmd = server.config->startup_cmd;
		pid_t pid = fork();
		if (pid < 0) {
			wlr_log(WLR_ERROR, "cannot execute binding command: fork() failed");
		} else if (pid == 0) {
			execl("/bin/sh", "/bin/sh", "-c", cmd, (void *)NULL);
		}
	}

	return 0;
}

int rootston_run(void)
{
	wl_display_run(server.wl_display);
#if WLR_HAS_XWAYLAND
	// We need to shutdown Xwayland before disconnecting all clients, otherwise
	// wlroots will restart it automatically.
	wlr_xwayland_destroy(server.xwayland);
#endif
	wl_display_destroy_clients(server.wl_display);
	wl_display_destroy(server.wl_display);
	return 0;
}
