//  Wayland stuff

#pragma once

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/xwayland.h>

struct wlserver_t {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;

	struct {
		struct wlr_backend *backend;
		struct wlr_renderer *renderer;
		struct wlr_compositor *compositor;
		struct wlr_xwayland *xwayland;
		struct wlr_session *session;	
		struct wlr_seat *seat;
		struct wlr_output *output;
		
		// Only for nested
		struct wlr_input_device *keyboard;
		struct wlr_input_device *pointer;
	} wlr;
};

extern struct wlserver_t wlserver;
