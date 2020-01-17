#ifndef C_SIDE
extern "C" {
#endif
	
unsigned int get_time_in_milliseconds(void);

int steamcompmgr_main(int argc, char **argv);

#ifndef C_SIDE
}
#endif
