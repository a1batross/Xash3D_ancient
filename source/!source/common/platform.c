//=======================================================================
//			Copyright XashXT Group 2007 ©
//			platform.c - game common dll
//=======================================================================

#include "platform.h"
#include "utils.h"
#include "bsplib.h"
#include "mdllib.h"
#include "qcclib.h"
#include "roqlib.h"

bool host_debug = false;
stdlib_api_t std;
byte *basepool;
byte *zonepool;

bool InitPlatform ( int argc, char **argv )
{
	basepool = Mem_AllocPool( "Temp" );
	zonepool = Mem_AllocPool( "Zone" );
	imagepool = Mem_AllocPool( "ImageLib Pool" );

	com_argc = argc;
	com_argv = argv;

	if(FS_CheckParm("-debug"))
		host_debug = true;

	return true;
}

void ClosePlatform ( void )
{
	Mem_FreePool( &imagepool);
	Mem_FreePool( &basepool );
	Mem_FreePool( &zonepool );
}

common_exp_t DLLEXPORT *CreateAPI ( stdlib_api_t *input )
{
	static common_exp_t		Com;

	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but first argument will be 0
	// and always make exception, run simply check for avoid it
	if(input) std = *input;

	// generic functions
	Com.api_size = sizeof(common_exp_t);

	Com.Init = InitPlatform;
	Com.Shutdown = ClosePlatform;

	Com.LoadImage = FS_LoadImage;
	Com.FreeImage = FS_FreeImage;
	Com.SaveImage = FS_SaveImage;	

	// get interfaces
	Com.Compile = Comp_GetAPI();
	Com.Info = Info_GetAPI();
	
	return &Com;
}