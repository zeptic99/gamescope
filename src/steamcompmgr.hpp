#ifndef C_SIDE
extern "C" {
#endif

#include <stdint.h>
	
extern uint32_t currentOutputWidth;
extern uint32_t currentOutputHeight;

unsigned int get_time_in_milliseconds(void);

int steamcompmgr_main(int argc, char **argv);

#ifndef C_SIDE
}

#include <mutex>
#include <vector>

#include <wlr/render/dmabuf.h>

struct ResListEntry_t {
	struct wlr_surface *surf;
	struct wlr_dmabuf_attributes attribs;
};

extern std::mutex wayland_commit_lock;
extern std::vector<ResListEntry_t> wayland_commit_queue;

#endif
