//=======================================================================
//			Copyright XashXT Group 2007 ©
//			platform.c - game platform dll
//=======================================================================

#include "platform.h"
#include "baseutils.h"
#include "bsplib.h"

gameinfo_t Plat_GameInfo( void )
{
	return GI;
}

void InitPlatform ( void )
{
	InitMemory();
	Plat_InitCPU();
	ThreadSetDefault();
	FS_Init();

	/*
	if(FS_GetParmFromCmdLine("-bounce", parm ))
			numbounce = atoi(parm);

	if(FS_GetParmFromCmdLine("-ambient", parm ))
		ambient = atof(parm) * 128;
	*/
}

void ClosePlatform ( void )
{
	FS_Shutdown();
	FreeMemory();
}

platform_api_t DLLEXPORT *CreateAPI ( stdio_api_t *input )
{
	static platform_api_t pi;

	std = *input;

	//generic functions
	pi.apiversion = PLATFORM_API_VERSION;
	pi.api_size = sizeof(platform_api_t);

	pi.Init = InitPlatform;
	pi.Shutdown = ClosePlatform;

	//get interfaces
	pi.Fs = FS_GetAPI();
	pi.VFs = VFS_GetAPI();
	pi.Mem = Mem_GetAPI();
	pi.Script = Sc_GetAPI();
	pi.Compile = Comp_GetAPI();

	pi.InitRootDir = FS_InitRootDir;
	pi.LoadGameInfo = FS_LoadGameInfo;
	pi.AddGameHierarchy = FS_AddGameHierarchy;

	//timer
	pi.DoubleTime = Plat_DoubleTime;
	pi.GameInfo = Plat_GameInfo;
	
	return &pi;
}