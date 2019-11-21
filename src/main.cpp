#include <X11/Xlib.h>

#include <thread>
#include <mutex>
#include <vector>

#include "rootston.h"
#include "steamcompmgr.h"

#include "main.hpp"
#include "main.h"

#include <waffle.h>

int ac;
char **av;

struct waffle_display *dpy;
struct waffle_window *window;
struct waffle_context *ctx;

Display *XWLDpy;

int main(int argc, char **argv)
{
	ac = argc;
	av = argv;
	
	XInitThreads();
	
	initOutput();

	rootston_init(argc, argv);
	
	register_signal();
	
	rootston_run();
}

void steamCompMgrThreadRun(void)
{
	waffle_make_current(dpy, window, ctx);

	steamcompmgr_main( ac, av );
}

void startSteamCompMgr(void)
{
	std::thread steamCompMgrThread( steamCompMgrThreadRun );
	steamCompMgrThread.detach();
	
	return;
}

void initOutput(void)
{
	struct waffle_config *config;
	
	const int32_t init_attrs[] = {
		WAFFLE_PLATFORM, WAFFLE_PLATFORM_X11_EGL,
		0,
	};
	
	const int32_t config_attrs[] = {
		WAFFLE_CONTEXT_API,         WAFFLE_CONTEXT_OPENGL,
		
		WAFFLE_RED_SIZE,            8,
		WAFFLE_BLUE_SIZE,           8,
		WAFFLE_GREEN_SIZE,          8,
		
		0,
	};
	
	const int32_t window_width = 1280;
	const int32_t window_height = 720;
	
	waffle_init(init_attrs);
	dpy = waffle_display_connect(NULL);
	
	config = waffle_config_choose(dpy, config_attrs);
	window = waffle_window_create(config, window_width, window_height);
	ctx = waffle_context_create(config, NULL);
	
	waffle_window_show(window);
}

std::mutex g_ResListLock;
std::vector<ResListEntry_t> g_vecResListEntries;

void wayland_PushSurface(struct wlr_surface *surf, struct wlr_dmabuf_attributes *attribs)
{
	std::lock_guard<std::mutex> lock(g_ResListLock);
	
	ResListEntry_t newEntry = {
		.surf = surf,
		.attribs = *attribs
	};
	g_vecResListEntries.push_back( newEntry );
	
	static bool bHasNestedDisplay = false;
	
	if ( bHasNestedDisplay == false )
	{
		// This should open the nested XWayland display as our environment changed during Xwayland init
		XWLDpy = XOpenDisplay( nullptr );
		
		bHasNestedDisplay = true;
	}
	
	static XEvent XWLExposeEvent = {
		.xexpose {
			.type = Expose,
			.window = DefaultRootWindow( XWLDpy ),
			.width = 1,
			.height = 1
		}
	};
	
	XSendEvent( XWLDpy , DefaultRootWindow( XWLDpy ), True, ExposureMask, &XWLExposeEvent);
}

int steamCompMgr_PullSurface( struct ResListEntry_t *pResEntry )
{
	std::lock_guard<std::mutex> lock(g_ResListLock);
	
	if ( g_vecResListEntries.size() > 0 )
	{
		*pResEntry = g_vecResListEntries[0];
		g_vecResListEntries.erase(g_vecResListEntries.begin());
		
		return 1;
	}
	
	return 0;
}
