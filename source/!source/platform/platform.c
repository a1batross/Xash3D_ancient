//=======================================================================
//			Copyright XashXT Group 2007 ©
//			platform.c - game platform dll
//=======================================================================

#include "platform.h"
#include "baseutils.h"
#include "bsplib.h"
#include "mdllib.h"
#include "qcclib.h"

bool host_debug = false;

gameinfo_t Plat_GameInfo( void )
{
	return GI;
}

bool InitPlatform ( int argc, char **argv )
{
	MsgDev(D_INFO, "------- Loading bin/platform.dll [%g] -------\n", PLATFORM_VERSION );

	InitMemory();
	Plat_InitCPU();
	Plat_LinkDlls();

	ThreadSetDefault();
	FS_Init( argc, argv );

	if(FS_CheckParm("-debug"))
		host_debug = true;

	return true;
}

void ClosePlatform ( void )
{
	FS_Shutdown();
	FreeMemory();
}

platform_exp_t DLLEXPORT *CreateAPI ( stdinout_api_t *input )
{
	static platform_exp_t pi;

	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but first argument will be 0
	// and always make exception, run simply check for avoid it
	if(input) std = *input;

	//generic functions
	pi.apiversion = PLATFORM_API_VERSION;
	pi.api_size = sizeof(platform_exp_t);

	pi.Init = InitPlatform;
	pi.Shutdown = ClosePlatform;

	//get interfaces
	pi.Fs = FS_GetAPI();
	pi.VFs = VFS_GetAPI();
	pi.Mem = Mem_GetAPI();
	pi.Script = Sc_GetAPI();
	pi.Compile = Comp_GetAPI();
	pi.Info = Info_GetAPI();

	pi.InitRootDir = FS_InitRootDir;
	pi.LoadGameInfo = FS_LoadGameInfo;
	pi.AddGameHierarchy = FS_AddGameHierarchy;

	//timer
	pi.DoubleTime = Plat_DoubleTime;
	pi.GameInfo = Plat_GameInfo;
	
	return &pi;
}