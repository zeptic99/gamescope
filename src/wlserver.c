#define _POSIX_C_SOURCE 200112L
#define _GNU_SOURCE

#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h> 
#include <string.h> 
#include <sys/epoll.h>

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/libinput.h>
#include <wlr/interfaces/wlr_pointer.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>

#define C_SIDE

#include "wlserver.h"
#include "main.hpp"

#include "gpuvis_trace_utils.h"

struct wlserver_t wlserver;

Display *g_XWLDpy;

static bool run = true;

void sig_handler(int signal)
{
	wlr_log(WLR_DEBUG, "Received kill signal. Terminating!");
	run = false;
}

void nudge_steamcompmgr(void)
{
	static bool bHasNestedDisplay = false;
	static XEvent XWLExposeEvent = {};
	
	if ( bHasNestedDisplay == false )
	{
		g_XWLDpy = XOpenDisplay( wlserver.wlr.xwayland->display_name );
		
		XWLExposeEvent.xclient.type = ClientMessage;
		XWLExposeEvent.xclient.window = DefaultRootWindow( g_XWLDpy );
		XWLExposeEvent.xclient.format = 32;

		bHasNestedDisplay = true;
	}
	
	XSendEvent( g_XWLDpy , DefaultRootWindow( g_XWLDpy ), True, SubstructureRedirectMask, &XWLExposeEvent);
	XFlush( g_XWLDpy );
}

static void xwayland_surface_role_commit(struct wlr_surface *wlr_surface) {
	assert(wlr_surface->role == &xwayland_surface_role);
	
	struct wlr_texture *tex = wlr_surface_get_texture( wlr_surface );
	
	struct wlr_dmabuf_attributes dmabuf_attribs = {};
	bool result = False;
	result = wlr_texture_to_dmabuf( tex, &dmabuf_attribs );
	
	assert( result == true );
	if ( result == false || dmabuf_attribs.fd[0] == -1 )
	{
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		
		wlr_surface_send_frame_done( wlr_surface, &now );
		return;
	}
	
	gpuvis_trace_printf( "xwayland_surface_role_commit wlr_surface %p\n", wlr_surface );
	
	wayland_PushSurface( wlr_surface, &dmabuf_attribs );
}

static void xwayland_surface_role_precommit(struct wlr_surface *wlr_surface) {
	assert(wlr_surface->role == &xwayland_surface_role);
	struct wlr_xwayland_surface *surface = wlr_surface->role_data;
	if (surface == NULL) {
		return;
	}
}

const struct wlr_surface_role xwayland_surface_role = {
	.name = "wlr_xwayland_surface",
	.commit = xwayland_surface_role_commit,
	.precommit = xwayland_surface_role_precommit,
};

static void xwayland_ready(struct wl_listener *listener, void *data)
{
	startSteamCompMgr();
}

struct wl_listener xwayland_ready_listener = { .notify = xwayland_ready };

int wlserver_init(int argc, char **argv, bool bIsNested) {
	bool bIsDRM = bIsNested == false;
	
	wlr_log_init(WLR_DEBUG, NULL);
	wlserver.wl_display = wl_display_create();

	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);
	
	wlserver.wlr.session = ( bIsDRM == True ) ? wlr_session_create(wlserver.wl_display) : NULL;
	
	wlserver.wl_event_loop = wl_display_get_event_loop(wlserver.wl_display);
	wlserver.wl_event_loop_fd = wl_event_loop_get_fd( wlserver.wl_event_loop );
	
	wlserver.wlr.backend = wlr_multi_backend_create(wlserver.wl_display);
	
	assert(wlserver.wl_display && wlserver.wl_event_loop && wlserver.wlr.backend);
	assert( !bIsDRM || wlserver.wlr.session );

	struct wlr_backend* headless_backend = wlr_headless_backend_create(wlserver.wl_display, NULL);
	if (headless_backend == NULL) {
		wlr_log(WLR_ERROR, "could not start headless_backend");
		return 1;
	}
	wlr_multi_backend_add(wlserver.wlr.backend, headless_backend);
	
	wlserver.wlr.output = wlr_headless_add_output( headless_backend, g_nNestedWidth, g_nNestedHeight );
	wlr_output_set_custom_mode( wlserver.wlr.output, g_nNestedWidth, g_nNestedHeight, g_nNestedRefresh * 1000 );
	
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
// 	else
// 	{
// 		wlserver.wlr.keyboard_dev = wlr_headless_add_input_device( headless_backend, WLR_INPUT_DEVICE_KEYBOARD );
// 		wlserver.wlr.pointer_dev = wlr_headless_add_input_device( headless_backend, WLR_INPUT_DEVICE_POINTER );
// 	}
	
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
	
	wlr_seat_set_capabilities( wlserver.wlr.seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD );
// 	wlr_seat_set_keyboard( wlserver.wlr.seat, wlserver.wlr.keyboard_dev );

	wlr_xwayland_set_seat(wlserver.wlr.xwayland, wlserver.wlr.seat);
	
	wl_signal_add(&wlserver.wlr.xwayland->events.ready, &xwayland_ready_listener);

	return 0;
}

pthread_mutex_t waylock = PTHREAD_MUTEX_INITIALIZER;

void wlserver_lock(void)
{
	pthread_mutex_lock(&waylock);
}

void wlserver_unlock(void)
{
	wl_display_flush_clients(wlserver.wl_display);
	pthread_mutex_unlock(&waylock);
}

int wlserver_run(void)
{
	int epoll_fd = epoll_create( 1 );
	struct epoll_event ev;
	struct epoll_event events[128];
	int n;
	
	ev.events = EPOLLIN;
	
	if ( epoll_fd == -1 ||
		epoll_ctl( epoll_fd, EPOLL_CTL_ADD, wlserver.wl_event_loop_fd, &ev ) == -1 )
	{
		return 1;
	}

	while ( run )
	{
		n = epoll_wait( epoll_fd, events, 128, -1 );
		if ( n == -1 )
		{
			break;
		}
		
		// We have wayland stuff to do, do it while locked
		wlserver_lock();
		
		for ( int i = 0; i < n; i++ )
		{
			wl_display_flush_clients(wlserver.wl_display);
			int ret = wl_event_loop_dispatch(wlserver.wl_event_loop, 0);
			if (ret < 0) {
				wlserver_unlock();
				break;
			}
		}

		wlserver_unlock();
	}

	// We need to shutdown Xwayland before disconnecting all clients, otherwise
	// wlroots will restart it automatically.
	wlr_xwayland_destroy(wlserver.wlr.xwayland);
	wl_display_destroy_clients(wlserver.wl_display);
	wl_display_destroy(wlserver.wl_display);
	return 0;
}

void wlserver_mousefocus( struct wlr_surface *wlrsurface )
{
	wlr_seat_pointer_notify_enter( wlserver.wlr.seat, wlrsurface, 0.5, 0.5 );
}

void wlserver_mousemotion( int x, int y, uint32_t time )
{
	wlr_seat_pointer_notify_motion( wlserver.wlr.seat, time, x, y );
	wlr_seat_pointer_notify_frame( wlserver.wlr.seat );
}

void wlserver_mousebutton( int button, bool press, uint32_t time )
{
	wlr_seat_pointer_notify_button( wlserver.wlr.seat, time, button, press ? WLR_BUTTON_PRESSED : WLR_BUTTON_RELEASED );
	wlr_seat_pointer_notify_frame( wlserver.wlr.seat );
}

void wlserver_mousewheel( int x, int y, uint32_t time )
{
	if ( x != 0 )
	{
		wlr_seat_pointer_notify_axis( wlserver.wlr.seat, time, WLR_AXIS_ORIENTATION_HORIZONTAL, x, x, WLR_AXIS_SOURCE_WHEEL );
	}
	if ( y != 0 )
	{
		wlr_seat_pointer_notify_axis( wlserver.wlr.seat, time, WLR_AXIS_ORIENTATION_VERTICAL, y, y, WLR_AXIS_SOURCE_WHEEL );
	}
	wlr_seat_pointer_notify_frame( wlserver.wlr.seat );
}
