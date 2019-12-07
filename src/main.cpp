#include <X11/Xlib.h>

#include <thread>
#include <mutex>
#include <vector>

#include <unistd.h>

#include "wlserver.h"
#include "steamcompmgr.h"

#include "main.hpp"
#include "main.h"
#include "drm.hpp"

#include <waffle.h>

int ac;
char **av;

struct waffle_display *dpy;
struct waffle_window *window;
struct waffle_context *ctx;

int g_nNestedWidth = 1280;
int g_nNestedHeight = 720;
int g_nNestedRefresh = 60;

int g_nOutputWidth = 1280;
int g_nOutputHeight = 720;

bool g_bIsNested = false;

int BIsNested()
{
	return g_bIsNested == true;
}

int main(int argc, char **argv)
{
	int o;
	ac = argc;
	av = argv;
	
	bool bSleepAtStartup = false;
	
	while ((o = getopt (argc, argv, ":w:h:r:s")) != -1)
	{
		switch (o) {
			case 'w':
				g_nNestedWidth = atoi( optarg );
				break;
			case 'h':
				g_nNestedHeight = atoi( optarg );
				break;
			case 'r':
				g_nNestedRefresh = atoi( optarg );
				break;
			case 's':
				bSleepAtStartup = true;
				break;
			default:
				break;
		}
	}
	
	if ( bSleepAtStartup == true )
	{
	 	sleep( 2 );
	}
	
	XInitThreads();
	
	initOutput();

	wlserver_init(argc, argv, g_bIsNested == true );
	
	register_signal();
	
	wlserver_run();
}

void steamCompMgrThreadRun(void)
{
	if ( g_bIsNested == true )
	{
		waffle_make_current(dpy, window, ctx);
	}

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
	if ( getenv("DISPLAY") != NULL )
	{
		g_bIsNested = true;
	}

	if ( g_bIsNested == true )
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
		
		const int32_t window_width = g_nOutputWidth;
		const int32_t window_height = g_nOutputHeight;
		
		waffle_init(init_attrs);
		dpy = waffle_display_connect(NULL);
		
		config = waffle_config_choose(dpy, config_attrs);
		window = waffle_window_create(config, window_width, window_height);
		ctx = waffle_context_create(config, NULL);
		
		waffle_window_show(window);
	}
	else
	{
		init_drm( &g_DRM, nullptr, nullptr, 0 );
	}
}

std::mutex g_ResListLock;
std::vector<ResListEntry_t> g_vecResListEntries;

void wayland_PushSurface(struct wlr_surface *surf, struct wlr_dmabuf_attributes *attribs)
{
	{
		std::lock_guard<std::mutex> lock(g_ResListLock);
		
		ResListEntry_t newEntry = {
			.surf = surf,
			.attribs = *attribs
		};
		g_vecResListEntries.push_back( newEntry );
	}
	
	nudge_steamcompmgr();
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
