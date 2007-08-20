//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cl_game.c - client.dll link system
//=======================================================================

#include <windows.h>
#include "client.h"

HINSTANCE	cl_library;

void CL_InitGameProgs (void)
{
	void	*cl;

	//find client.dll
	cl = Sys_LoadGame("ClientAPI", cl_library, NULL);

	if(cl) Msg("client.dll found and loaded\n");
	else Msg("client.dll not loaded\n"); 
}