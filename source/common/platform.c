//=======================================================================
//			Copyright XashXT Group 2007 ©
//			platform.c - game common dll
//=======================================================================

#include "platform.h"
#include "utils.h"
#include "bsplib.h"
#include "mdllib.h"

dll_info_t vprogs_dll = { "vprogs.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(vprogs_exp_t) };
vprogs_exp_t *PRVM;
stdlib_api_t com;

#define	MAX_SEARCHMASK	256
string	searchmask[MAX_SEARCHMASK];
int	num_searchmask = 0;
string	gs_searchmask;
string	gs_gamedir;
byte	*basepool;
byte	*zonepool;
byte	*error_bmp;
size_t	error_bmp_size;
byte	*checkermate_dds;
size_t	checkermate_dds_size;
static	double start, end;
uint	app_name = HOST_OFFLINE;

void ClrMask( void )
{
	num_searchmask = 0;
	Mem_Set( searchmask, 0,  MAX_STRING * MAX_SEARCHMASK ); 
}

void AddMask( const char *mask )
{
	if( num_searchmask >= MAX_SEARCHMASK )
	{
		MsgDev( D_WARN, "AddMask: searchlist is full\n" );
		return;
	}
	com.strncpy( searchmask[num_searchmask], mask, MAX_STRING );
	num_searchmask++;
}

/*
==================
CommonInit

platform.dll needs for some setup operations
so do it manually
==================
*/
void InitPlatform ( int argc, char **argv )
{
	byte	bspflags = 0, qccflags = 0, roqflags = 0;
	string	source, gamedir;
	launch_t	CreateVprogs;

	basepool = Mem_AllocPool( "Temp" );
	// blamk image for missed resources
	error_bmp = FS_LoadInternal( "blank.bmp", &error_bmp_size );

	// for custom cmdline parsing
	com_argc = argc;
	com_argv = argv;
	app_name = g_Instance;

	switch( app_name )
	{
	case HOST_BSPLIB:
		if(!FS_GetParmFromCmdLine("-game", gamedir ))
			com.strncpy(gamedir, Cvar_VariableString( "fs_defaultdir" ), sizeof(gamedir));
		if(!FS_GetParmFromCmdLine("+map", source ))
			com.strncpy(source, "newmap", sizeof(source));
		if(FS_CheckParm("-vis")) bspflags |= BSP_ONLYVIS;
		if(FS_CheckParm("-rad")) bspflags |= BSP_ONLYRAD;
		if(FS_CheckParm("-full")) bspflags |= BSP_FULLCOMPILE;
		if(FS_CheckParm("-onlyents")) bspflags |= BSP_ONLYENTS;
		if(FS_CheckParm("-notjunc")) notjunc = true;

		// famous q1 "notexture" image: purple-black checkerboard
		checkermate_dds = FS_LoadInternal( "checkerboard.dds", &checkermate_dds_size );

		// initialize ImageLibrary
		start = Sys_DoubleTime();
		Image_Init( NULL, IL_ALLOW_OVERWRITE|IL_IGNORE_MIPS );
		PrepareBSPModel( gamedir, source, bspflags );
		break;
	case HOST_QCCLIB:
		Sys_LoadLibrary( &vprogs_dll ); // load qcclib
		CreateVprogs = (void *)vprogs_dll.main;
		PRVM = CreateVprogs( &com, NULL ); // second interface not allowed

		PRVM->Init( argc, argv );

		if(!FS_GetParmFromCmdLine("-dir", gamedir ))
			com.strncpy(gamedir, ".", sizeof(gamedir));
		if(!FS_GetParmFromCmdLine("+src", source ))
			com.strncpy(source, "progs.src", sizeof(source));

		start = Sys_DoubleTime();
		PRVM->PrepareDAT( gamedir, source );	
		break;
	case HOST_SPRITE:
	case HOST_STUDIO:
	case HOST_WADLIB:
	case HOST_RIPPER:
		FS_InitRootDir(".");

		// initialize ImageLibrary
		Image_Init( NULL, IL_KEEP_8BIT );
		start = Sys_DoubleTime();
		Msg( "\n\n" ); // tabulation
		break;
	case HOST_OFFLINE:
		break;
	}
}

void RunPlatform( void )
{
	search_t	*search;
	bool	(*CompileMod)( byte *mempool, const char *name, byte parms ) = NULL;
	cvar_t	*fs_defaultdir = Cvar_Get( "fs_defaultdir", "tmpQuArK", CVAR_SYSTEMINFO, NULL );
	byte	parms = 0; // future expansion
	int	i, j, numCompiledMods = 0;
	string	errorstring;

	// directory to extract
	com.strncpy( gs_gamedir, fs_defaultdir->string, sizeof( gs_gamedir ));
	Mem_Set( errorstring, 0, MAX_STRING ); 
	ClrMask();

	switch( app_name )
	{
	case HOST_SPRITE: 
		CompileMod = CompileSpriteModel;
		AddMask( "*.qc" );
		break;
	case HOST_STUDIO:
		CompileMod = CompileStudioModel;
		AddMask( "*.qc" );
		break;
	case HOST_BSPLIB: 
		AddMask( "*.map" );
		CompileBSPModel(); 
		break;
	case HOST_WADLIB:
		CompileMod = CompileWad3Archive;
		AddMask( "*.qc" );
		break;
	case HOST_RIPPER:
		CompileMod = ConvertResource;
		Conv_RunSearch();
		break;
	case HOST_QCCLIB: 
		AddMask( "*.src" );
		PRVM->CompileDAT(); 
		break;
	case HOST_OFFLINE:
		break;
	}
	if( !CompileMod ) goto elapced_time; // jump to shutdown

	// using custom mask
	if(FS_GetParmFromCmdLine( "-file", gs_searchmask ))
	{
		ClrMask(); // clear all previous masks
		AddMask( gs_searchmask ); // custom mask
	}
	zonepool = Mem_AllocPool( "compiler" );
	Msg( "Converting ...\n\n" );

	// search by mask		
	for( i = 0; i < num_searchmask; i++ )
	{
		// skip blank mask
		if(!com.strlen( searchmask[i] )) continue;
		search = FS_Search( searchmask[i], true );
		if( !search ) continue; // try next mask

		for( j = 0; j < search->numfilenames; j++ )
		{
			if(CompileMod( zonepool, search->filenames[j], parms ))
				numCompiledMods++;
		}
		Mem_Free( search );
	}
	if( numCompiledMods == 0 ) 
	{
		if( !num_searchmask ) com.strncpy( errorstring, "files", MAX_STRING );
		for( j = 0; j < num_searchmask; j++ ) 
		{
			if(!com.strlen( searchmask[j] )) continue;
			com.strncat( errorstring, va("%s ", searchmask[j]), MAX_STRING );
		}
		Sys_Break( "no %s found in this folder!\n", errorstring );
	}
elapced_time:
	end = Sys_DoubleTime();
	Msg( "%5.3f seconds elapsed\n", end - start );
	if( numCompiledMods > 1) Msg("total %d files proceed\n", numCompiledMods );
}

void FreePlatform ( void )
{
	if( app_name == HOST_QCCLIB )
	{
		PRVM->Free();
		Sys_FreeLibrary( &vprogs_dll ); // free qcclib
	}
	else if( app_name == HOST_RIPPER )
	{
		// finalize qc-script
		Skin_FinalizeScript();
	}

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