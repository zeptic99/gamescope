

#define C_SIDE

#include "main.hpp"
#include "wlserver.h"


static void xwayland_ready(struct wl_listener *listener,
										 void *data) {
	startSteamCompMgr();
}

struct wl_listener xwayland_ready_listener = { .notify = xwayland_ready };

void register_signal(void)
{
	wl_signal_add(&wlserver.wlr.xwayland->events.ready, &xwayland_ready_listener);
}
