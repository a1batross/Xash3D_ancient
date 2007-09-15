//=======================================================================
//			Copyright XashXT Group 2007 ©
//			platform.c - game common dll
//=======================================================================

#include "platform.h"
#include "utils.h"
#include "bsplib.h"
#include "mdllib.h"
#include "qcclib.h"

bool host_debug = false;
stdlib_api_t std;

gameinfo_t Plat_GameInfo( void )
{
	return GI;
}

bool InitPlatform ( int argc, char **argv )
{
	MsgDev(D_INFO, "------- Loading bin/common.dll [%g] -------\n", COMMON_VERSION );

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

common_exp_t DLLEXPORT *CreateAPI ( stdlib_api_t *input )
{
	static common_exp_t		Com;

	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but first argument will be 0
	// and always make exception, run simply check for avoid it
	if(input) std = *input;

	//generic functions
	Com.apiversion = COMMON_API_VERSION;
	Com.api_size = sizeof(common_exp_t);

	Com.Init = InitPlatform;
	Com.Shutdown = ClosePlatform;

	//get interfaces
	Com.Fs = FS_GetAPI();
	Com.VFs = VFS_GetAPI();
	Com.Mem = Mem_GetAPI();
	Com.Script = Sc_GetAPI();
	Com.Compile = Comp_GetAPI();
	Com.Info = Info_GetAPI();

	Com.InitRootDir = FS_InitRootDir;
	Com.LoadGameInfo = FS_LoadGameInfo;
	Com.AddGameHierarchy = FS_AddGameHierarchy;

	//timer
	Com.DoubleTime = Plat_DoubleTime;
	Com.GameInfo = Plat_GameInfo;
	
	return &Com;
}