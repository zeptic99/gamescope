#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/config.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/util/log.h>
#include <wlr/xwayland.h>
#include "wlserver.h"
#include "xwayland.h"

#define C_SIDE
#include "main.hpp"

static void xwayland_surface_role_commit(struct wlr_surface *wlr_surface) {
	assert(wlr_surface->role == &xwayland_surface_role);
	
	struct wlr_texture *tex = wlr_surface_get_texture( wlr_surface );
	
	struct wlr_dmabuf_attributes dmabuf_attribs = {};
	bool result = False;
	result = wlr_texture_to_dmabuf( tex, &dmabuf_attribs );
	
	if (result == False)
	{
		//
	}
	
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
