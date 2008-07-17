//=======================================================================
//			Copyright XashXT Group 2007 ©
//			engine.c - engine entry base
//=======================================================================

#include "common.h"

/*
=================
Base Entry Point
=================
*/
launch_exp_t DLLEXPORT *CreateAPI( stdlib_api_t *input, void *unused )
{
         	static launch_exp_t Host;

	com = *input;
	Host.api_size = sizeof(launch_exp_t);

	Host.Init = Host_Init;
	Host.Main = Host_Main;
	Host.Free = Host_Free;
	Host.CPrint = Host_Print;
	Host.MSG_Init = MSG_Init;

	return &Host;
}