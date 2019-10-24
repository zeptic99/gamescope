#include <thread>

#include "rootston.h"
#include "steamcompmgr.h"

#include "main.hpp"
#include "main.h"

int ac;
char **av;

int main(int argc, char **argv)
{
	ac = argc;
	av = argv;

	rootston_init(argc, argv);
	
	register_signal();
	
	rootston_run();
}

void steamCompMgrThreadRun(void)
{
	steamcompmgr_main( ac, av );
}

void startSteamCompMgr(void)
{
	std::thread steamCompMgrThread( steamCompMgrThreadRun );
	steamCompMgrThread.detach();
	
	return;
}
