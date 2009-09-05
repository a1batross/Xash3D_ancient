//=======================================================================
//			Copyright XashXT Group 2007 ©
//			platform.c - tools common dll
//=======================================================================

#include "xtools.h"
#include "utils.h"
#include "mdllib.h"
#include "vprogs_api.h"
#include "xtools.h"
#include "engine_api.h"
#include "mathlib.h"

dll_info_t vprogs_dll = { "vprogs.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(vprogs_exp_t) };
vprogs_exp_t *PRVM;
stdlib_api_t com;
char  **com_argv;

#define MAX_SEARCHMASK	256

string	searchmask[MAX_SEARCHMASK];
int	num_searchmask = 0;
string	gs_searchmask;
string	gs_gamedir;
byte	*basepool;
byte	*zonepool;
byte	*error_bmp;
size_t	error_bmp_size;
static	double start, end;
uint	app_name = HOST_OFFLINE;
bool	enable_log = false;
file_t	*bsplog = NULL;

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
void InitCommon( const int argc, const char **argv )
{
	int	imageflags = 0;
	launch_t	CreateVprogs;

	basepool = Mem_AllocPool( "Common Pool" );
	app_name = g_Instance;
	enable_log = false;

	switch( app_name )
	{
	case HOST_BSPLIB:
		if( !FS_GetParmFromCmdLine( "-game", gs_basedir ))
			com.strncpy( gs_basedir, Cvar_VariableString( "fs_defaultdir" ), sizeof( gs_basedir ));
		if( !FS_GetParmFromCmdLine( "+map", gs_filename ))
			com.strncpy( gs_filename, "newmap", sizeof( gs_filename ));
		// initialize ImageLibrary
		start = Sys_DoubleTime();
		PrepareBSPModel( (int)argc, (char **)argv );
		break;
	case HOST_QCCLIB:
		Sys_LoadLibrary( &vprogs_dll );	// load qcclib
		CreateVprogs = (void *)vprogs_dll.main;
		PRVM = CreateVprogs( &com, NULL );	// second interface not allowed

		PRVM->Init( argc, argv );

		if( !FS_GetParmFromCmdLine( "-dir", gs_basedir ))
			com.strncpy( gs_basedir, ".", sizeof( gs_basedir ));
		if( !FS_GetParmFromCmdLine( "+src", gs_filename ))
			com.strncpy( gs_filename, "progs.src", sizeof( gs_filename ));

		start = Sys_DoubleTime();
		PRVM->PrepareDAT( gs_basedir, gs_filename );	
		break;
	case HOST_XIMAGE:
		imageflags |= (IL_USE_LERPING|IL_ALLOW_OVERWRITE|IL_IGNORE_MIPS);
		com_argv = (char **)argv;
		if( !FS_GetParmFromCmdLine( "-to", gs_filename ))
			gs_filename[0] = '\0'; // will be set later
	case HOST_SPRITE:
	case HOST_STUDIO:
	case HOST_WADLIB:
		imageflags |= IL_KEEP_8BIT;
		Image_Init( NULL, imageflags );
	case HOST_RIPPER:
		// blamk image for missed resources
		error_bmp = FS_LoadInternal( "blank.bmp", &error_bmp_size );
		FS_InitRootDir(".");

		start = Sys_DoubleTime();
		Msg( "\n\n" ); // tabulation
		break;
	case HOST_OFFLINE:
		break;
	}
}

void CommonMain( void )
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
	case HOST_XIMAGE:
		CompileMod = ConvertImages;
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
	if( FS_GetParmFromCmdLine( "-file", gs_searchmask ))
	{
		ClrMask(); // clear all previous masks
		AddMask( gs_searchmask ); // custom mask

		if( app_name == HOST_XIMAGE && !gs_filename[0] )
		{
			const char	*ext = FS_FileExtension( gs_searchmask );
                              
			if( !com.strcmp( ext, "" ) || !com.strcmp( ext, "*" )); // blend mode
			else com.strncpy( gs_filename, ext, sizeof( gs_filename ));
		}
	}
	zonepool = Mem_AllocPool( "Zone Pool" );
	Msg( "Converting ...\n\n" );

	// search by mask		
	for( i = 0; i < num_searchmask; i++ )
	{
		// skip blank mask
		if( !com.strlen( searchmask[i] )) continue;
		search = FS_Search( searchmask[i], true );
		if( !search ) continue; // try next mask

		for( j = 0; j < search->numfilenames; j++ )
		{
			if( CompileMod( zonepool, search->filenames[j], parms ))
				numCompiledMods++;
		}
		Mem_Free( search );
	}
	if( numCompiledMods == 0 ) 
	{
		if( !num_searchmask ) com.strncpy( errorstring, "files", MAX_STRING );
		for( j = 0; j < num_searchmask; j++ ) 
		{
			if( !com.strlen( searchmask[j] )) continue;
			com.strncat( errorstring, va("%s ", searchmask[j]), MAX_STRING );
		}
		Sys_Break( "no %s found in this folder!\n", errorstring );
	}
elapced_time:
	end = Sys_DoubleTime();
	Msg( "%5.3f seconds elapsed\n", end - start );
	if( numCompiledMods > 1 ) Msg( "total %d files proceed\n", numCompiledMods );
}

void FreeCommon( void )
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
	else if( app_name == HOST_BSPLIB )
	{
		Bsp_Shutdown();
		if( bsplog ) FS_Close( bsplog );
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

	Com.Init = InitCommon;
	Com.Main = CommonMain;
	Com.Free = FreeCommon;
	Com.CPrint = Bsp_PrintLog;

	return &Com;
}