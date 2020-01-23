#ifndef C_SIDE
extern "C" {
#endif

extern uint32_t currentOutputWidth;
extern uint32_t currentOutputHeight;

unsigned int get_time_in_milliseconds(void);

int steamcompmgr_main(int argc, char **argv);

#ifndef C_SIDE
}

extern std::mutex wayland_commit_lock;
extern std::vector<ResListEntry_t> wayland_commit_queue;

#endif
