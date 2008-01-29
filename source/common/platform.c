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

dll_info_t imglib_dll = { "imglib.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(imglib_exp_t) };
bool host_debug = false;
imglib_exp_t *Image;
stdlib_api_t com;
byte *basepool;
byte *zonepool;
static double start, end;
uint app_name = 0;

/*
==================
CommonInit

platform.dll needs for some setup operations
so do it manually
==================
*/
void InitPlatform ( uint funcname, int argc, char **argv )
{
	byte	bspflags = 0, qccflags = 0, roqflags = 0;
	char	source[64], gamedir[64];
	launch_t	CreateImglib;

	basepool = Mem_AllocPool( "Temp" );

	// for custom cmdline parsing
	com_argc = argc;
	com_argv = argv;
	app_name = funcname;

	Sys_LoadLibrary( &imglib_dll ); // load imagelib
	CreateImglib = (void *)imglib_dll.main;
	Image = CreateImglib( &com, NULL ); // second interface not allowed

	if(FS_CheckParm("-debug")) host_debug = true;

	switch( funcname )
	{
	case COMP_BSPLIB:
		if(!FS_GetParmFromCmdLine("-game", gamedir ))
			strncpy(gamedir, "xash", sizeof(gamedir));
		if(!FS_GetParmFromCmdLine("+map", source ))
			strncpy(source, "newmap", sizeof(source));
		if(FS_CheckParm("-vis")) bspflags |= BSP_ONLYVIS;
		if(FS_CheckParm("-rad")) bspflags |= BSP_ONLYRAD;
		if(FS_CheckParm("-full")) bspflags |= BSP_FULLCOMPILE;
		if(FS_CheckParm("-onlyents")) bspflags |= BSP_ONLYENTS;

		PrepareBSPModel( gamedir, source, bspflags );
		break;
	case COMP_QCCLIB:
		if(!FS_GetParmFromCmdLine("-dir", gamedir ))
			strncpy(gamedir, ".", sizeof(gamedir));
		if(!FS_GetParmFromCmdLine("+src", source ))
			strncpy(source, "progs.src", sizeof(source));
		if(FS_CheckParm("-progdefs")) qccflags |= QCC_PROGDEFS;
		if(FS_CheckParm("/O0")) qccflags |= QCC_OPT_LEVEL_0;
		if(FS_CheckParm("/O1")) qccflags |= QCC_OPT_LEVEL_1;
		if(FS_CheckParm("/O2")) qccflags |= QCC_OPT_LEVEL_2;
		if(FS_CheckParm("/O3")) qccflags |= QCC_OPT_LEVEL_3;

		start = Sys_DoubleTime();
		PrepareDATProgs( gamedir, source, qccflags );	
		break;
	case COMP_ROQLIB:
	case COMP_SPRITE:
	case COMP_STUDIO:
	case COMP_WADLIB:
		FS_InitRootDir(".");
		start = Sys_DoubleTime();
		break;
	case HOST_OFFLINE:
		break;
	}

	Image->Init( funcname ); // initialize image support
}

void RunPlatform ( void )
{
	search_t	*search;
	bool	(*CompileMod)( byte *mempool, const char *name, byte parms ) = NULL;
	char	filename[MAX_QPATH], typemod[16], searchmask[8][16], errorstring[256];
	byte	parms = 0; // future expansion
	int	i, j, numCompiledMods = 0;

	memset( searchmask, 0, 8 * 16 ); 
	memset( errorstring, 0, 256 ); 

	switch(app_name)
	{
	case COMP_SPRITE: 
		CompileMod = CompileSpriteModel;
		strcpy(typemod, "sprites" );
		strcpy(searchmask[0], "*.qc" );
		break;
	case COMP_STUDIO:
		CompileMod = CompileStudioModel;
		strcpy(typemod, "models" );
		strcpy(searchmask[0], "*.qc" );
		break;
	case COMP_BSPLIB: 
		strcpy(typemod, "maps" );
		strcpy(searchmask[0], "*.map" );
		CompileBSPModel(); 
		break;
	case COMP_WADLIB:
		CompileMod = CompileWad3Archive;
		strcpy(typemod, "wads" );
		strcpy(searchmask[0], "*.qc" );
		break;
	case COMP_QCCLIB: 
		strcpy(typemod, "progs" );
		strcpy(searchmask[0], "*.src" );
		CompileDATProgs(); 
		break;
	case COMP_ROQLIB:
		CompileMod = CompileROQVideo;
		strcpy(typemod, "videos" );
		strcpy(searchmask[0], "*.qc" );
		break;
	case HOST_OFFLINE:
		strcpy(typemod, "things" );
		strcpy(searchmask[0], "*.*" );
		break;
	}
	if(!CompileMod) goto elapced_time; // jump to shutdown

	zonepool = Mem_AllocPool("compiler");
	if(!FS_GetParmFromCmdLine("-file", filename ))
	{
		// search by mask		
		for( i = 0; i < 8; i++)
		{
			// skip blank mask
			if(!com.strlen(searchmask[i])) continue;
			search = FS_Search( searchmask[i], true );
			if(!search) continue; // try next mask

			for( j = 0; j < search->numfilenames; j++ )
			{
				if(CompileMod( zonepool, search->filenames[j], parms ))
					numCompiledMods++;
			}
		}
		if(numCompiledMods == 0) 
		{
			for(j = 0; j < 8; j++) 
			{
				if(!strlen(searchmask[j])) continue;
				strcat(errorstring, va("%s ", searchmask[j]));
			}
			Sys_Break("no %sfound in this folder!\n", errorstring );
		}
	}
	else CompileMod( zonepool, filename, parms );

elapced_time:
	end = Sys_DoubleTime();
	Msg ("%5.3f seconds elapsed\n", end - start);
	if(numCompiledMods > 1) Msg("total %d %s compiled\n", numCompiledMods, typemod );
}

void FreePlatform ( void )
{
	Image->Free();
	Sys_FreeLibrary( &imglib_dll ); // free imagelib

	Mem_Check(); // check for leaks
	Mem_FreePool( &basepool );
	Mem_FreePool( &zonepool );
}

launch_exp_t DLLEXPORT *CreateAPI( stdlib_api_t *input, void *unused )
{
	static launch_exp_t		Com;

	com = *input;

	// generic functions
	Com.api_size = sizeof(launch_exp_t);

	Com.Init = InitPlatform;
	Com.Main = RunPlatform;
	Com.Free = FreePlatform;

	return &Com;
}