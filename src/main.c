#include "server.h"

#define C_SIDE

#include "main.hpp"

static void xwayland_ready(struct wl_listener *listener,
										 void *data) {
	startSteamCompMgr();
}

struct wl_listener xwayland_ready_listener = { .notify = xwayland_ready };

void register_signal(void)
{
	wl_signal_add(&server.desktop->xwayland->events.ready, &xwayland_ready_listener);
}
