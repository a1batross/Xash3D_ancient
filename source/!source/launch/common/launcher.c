//=======================================================================
//			Copyright XashXT Group 2007 ©
//			launcher.c - main engine launcher
//=======================================================================

#include "launch.h"

const char *show_credits = "\n\n\n\n\tCopyright XashXT Group 2007 ©\n\t          All Rights Reserved\n\n\t           Visit www.xash.ru\n";
common_exp_t	*Com;	// callback to utilities
launch_exp_t	*Host;	// callback to mainframe 

stdlib_api_t *Get_StdAPI( void )
{
	static stdlib_api_t std;

	// base events
	std.printf = Sys_Msg;
	std.dprintf = Sys_MsgDev;
	std.wprintf = Sys_MsgWarn;
	std.error = Sys_Error;
	std.exit = Sys_Exit;
	std.print = Sys_Print;
	std.input = Sys_Input;
	std.sleep = Sys_Sleep;
	std.clipboard = Sys_GetClipboardData;

	// multi-thread system
	std.create_thread = Sys_RunThreadsOnIndividual;
	std.thread_lock = Sys_ThreadLock;
	std.thread_unlock = Sys_ThreadUnlock;
	std.get_numthreads = Sys_GetNumThreads;

	// crclib.c funcs
	std.crc_init = CRC_Init;
	std.crc_block = CRC_Block;
	std.crc_process = CRC_ProcessByte;
	std.crc_sequence = CRC_BlockSequence;

	// memlib.c
	std.memcpy = _mem_copy;
	std.memset = _mem_set;
	std.realloc = _mem_realloc;
	std.move = _mem_move;
	std.malloc = _mem_alloc;
	std.free = _mem_free;
	std.mallocpool = _mem_allocpool;
	std.freepool = _mem_freepool;
	std.clearpool = _mem_emptypool;
	std.memcheck = _mem_check;

	std.InitRootDir = FS_InitRootDir;		// init custom rootdir 
	std.LoadGameInfo = FS_LoadGameInfo;		// gate game info from script file
	std.AddGameHierarchy = FS_AddGameHierarchy;	// add base directory in search list

	std.Fs = FS_GetAPI();
	std.VFs = VFS_GetAPI();

	// timelib.c funcs
	std.time_stamp = com_timestamp;
	std.gettime = Sys_DoubleTime;
	std.GameInfo = &GI;

	std.Script = Sc_GetAPI();

	std.strnupr = com_strnupr;
	std.strnlwr = com_strnlwr;
	std.strupr = com_strupr;
	std.strlwr = com_strlwr;
	std.strlen = com_strlen;
	std.cstrlen = com_cstrlen;
	std.toupper = com_toupper;
	std.tolower = com_tolower;
	std.strncat = com_strncat;
	std.strcat = com_strcat;
	std.strncpy = com_strncpy;
	std.strcpy = com_strcpy;
	std.stralloc = com_stralloc;
	std.atoi = com_atoi;
	std.atof = com_atof;
	std.atov = com_atov;
	std.strchr = com_strchr;
	std.strrchr = com_strrchr;
	std.strnicmp = com_strnicmp;
	std.stricmp = com_stricmp;
	std.strncmp = com_strncmp;
	std.strcmp = com_strcmp;
	std.stristr = com_stristr;
	std.strstr = com_stristr;		// FIXME
	std.strpack = com_strpack;
	std.strunpack = com_strunpack;
	std.vsprintf = com_vsprintf;
	std.sprintf = com_sprintf;
	std.va = va;
	std.vsnprintf = com_vsnprintf;
	std.snprintf = com_snprintf;

	std.LoadLibrary = Sys_LoadLibrary;
	std.FreeLibrary = Sys_FreeLibrary;
	std.GetProcAddress = Sys_GetProcAddress;

	return &std;
}

/*
==================
CommonInit

platform.dll needs for some setup operations
so do it manually
==================
*/
void CommonInit ( char *funcname, int argc, char **argv )
{
	byte bspflags = 0, qccflags = 0, roqflags = 0;
	char source[64], gamedir[64];

	Com->Init( argc, argv );

	switch(sys.app_name)
	{
	case BSPLIB:
		if(!FS_GetParmFromCmdLine("-game", gamedir ))
			com_strncpy(gamedir, "xash", sizeof(gamedir));
		if(!FS_GetParmFromCmdLine("+map", source ))
			com_strncpy(source, "newmap", sizeof(source));
		if(FS_CheckParm("-vis")) bspflags |= BSP_ONLYVIS;
		if(FS_CheckParm("-rad")) bspflags |= BSP_ONLYRAD;
		if(FS_CheckParm("-full")) bspflags |= BSP_FULLCOMPILE;
		if(FS_CheckParm("-onlyents")) bspflags |= BSP_ONLYENTS;

		Com->Compile.PrepareBSP( gamedir, source, bspflags );
		break;
	case QCCLIB:
		if(!FS_GetParmFromCmdLine("-dir", gamedir ))
			com_strncpy(gamedir, ".", sizeof(gamedir));
		if(!FS_GetParmFromCmdLine("+src", source ))
			com_strncpy(source, "progs.src", sizeof(source));
		if(FS_CheckParm("-progdefs")) qccflags |= QCC_PROGDEFS;
		if(FS_CheckParm("/O0")) qccflags |= QCC_OPT_LEVEL_0;
		if(FS_CheckParm("/O1")) qccflags |= QCC_OPT_LEVEL_1;
		if(FS_CheckParm("/O2")) qccflags |= QCC_OPT_LEVEL_2;
		if(FS_CheckParm("/O3")) qccflags |= QCC_OPT_LEVEL_3;

		sys.start = Sys_DoubleTime();
		Com->Compile.PrepareDAT( gamedir, source, qccflags );	
		break;
	case ROQLIB:
		if(!FS_GetParmFromCmdLine("-dir", gamedir ))
			com_strncpy(gamedir, ".", sizeof(gamedir));
		if(!FS_GetParmFromCmdLine("+src", source ))
			com_strncpy(source, "makefile.qc", sizeof(source));

		sys.start = Sys_DoubleTime();
		Com->Compile.PrepareROQ( gamedir, source, roqflags );	
		break;
	case IMGLIB:
	case SPRITE:
	case STUDIO:
		FS_InitRootDir(".");
		sys.start = Sys_DoubleTime();
		break;
	case DEFAULT:
		break;
	}
}

void CommonMain ( void )
{
	search_t	*search;
	bool	(*CompileMod)( byte *mempool, const char *name, byte parms ) = NULL;
	char	filename[MAX_QPATH], typemod[16], searchmask[8][16], errorstring[256];
	byte	parms = 0; // future expansion
	int	i, j, numCompiledMods = 0;

	Mem_Set( searchmask, 0, 8 * 16 ); 
	Mem_Set( errorstring, 0, 256 ); 

	switch(sys.app_name)
	{
	case SPRITE: 
		CompileMod = Com->Compile.Sprite;
		com_strcpy(typemod, "sprites" );
		com_strcpy(searchmask[0], "*.qc" );
		break;
	case STUDIO:
		CompileMod = Com->Compile.Studio;
		com_strcpy(typemod, "models" );
		com_strcpy(searchmask[0], "*.qc" );
		break;
	case IMGLIB:
		CompileMod = Com->Compile.Image; 
		com_strcpy(typemod, "images" );
		com_strcpy(searchmask[0], "*.pcx" );	// quake2 menu images
		com_strcpy(searchmask[1], "*.wal" );	// quake2 textures
		com_strcpy(searchmask[2], "*.lmp" );	// quake1 menu images
		com_strcpy(searchmask[3], "*.mip" );	// quake1 textures
		Msg("Processing images ...\n\n");
		break;		
	case BSPLIB: 
		com_strcpy(typemod, "maps" );
		com_strcpy(searchmask[0], "*.map" );
		Com->Compile.BSP(); 
		break;
	case QCCLIB: 
		com_strcpy(typemod, "progs" );
		com_strcpy(searchmask[0], "*.src" );
		com_strcpy(searchmask[1], "*.qc" );	// no longer used
		Com->Compile.DAT(); 
		break;
	case ROQLIB:
		com_strcpy(typemod, "videos" );
		com_strcpy(searchmask[0], "*.qc" );
		Com->Compile.ROQ(); 
		break;
	case DEFAULT:
		com_strcpy(typemod, "things" );
		com_strcpy(searchmask[0], "*.*" );
		break;
	}
	if(!CompileMod) goto elapced_time;//back to shutdown

	sys.zonepool = Mem_AllocPool("compiler");
	if(!FS_GetParmFromCmdLine("-file", filename ))
	{
		//search by mask		
		for( i = 0; i < 8; i++)
		{
			// skip blank mask
			if(!com_strlen(searchmask[i])) continue;
			search = FS_Search( searchmask[i], true );
			if(!search) continue; // try next mask

			for( j = 0; j < search->numfilenames; j++ )
			{
				if(CompileMod( sys.zonepool, search->filenames[j], parms ))
					numCompiledMods++;
			}
		}
		if(numCompiledMods == 0) 
		{
			for(j = 0; j < 8; j++) com_strcat(errorstring, searchmask[j]);
			Sys_Error("no %s found in this folder!\n", errorstring );
		}
	}
	else CompileMod( sys.zonepool, filename, parms );

elapced_time:
	sys.end = Sys_DoubleTime();
	Msg ("%5.3f seconds elapsed\n", sys.end - sys.start);
	if(numCompiledMods > 1) Msg("total %d %s compiled\n", numCompiledMods, typemod );
}

void CommonShutdown ( void )
{
	Mem_Check(); //check for leaks
	Mem_FreePool( &sys.zonepool );
	Com->Shutdown();
}


/*
==================
Find needed library, setup and run it
==================
*/
void CreateInstance( void )
{
	// export
	common_t		CreateCom;
	launch_t		CreateHost;
          
	// first text message into console or log 
	MsgDev(D_INFO, "Sys_LoadLibrary: Loading launch.dll - ok\n" );
	Sys_LoadLibrary( sys.linked_dll ); // loading library if need

	switch(sys.app_name)
	{
	case HOST_SHARED:
	case HOST_DEDICATED:
	case HOST_EDITOR:		
		CreateHost = (void *)sys.linked_dll->main;

		// set callback
		Host = CreateHost( Get_StdAPI());
		sys.Init = Host->Init;
		sys.Main = Host->Main;
		sys.Free = Host->Free;
		break;
	case BSPLIB:
	case QCCLIB:
	case ROQLIB:
	case IMGLIB:
	case SPRITE:
	case STUDIO:
		CreateCom = (void *)sys.linked_dll->main;
		
		// set callback
		Com = CreateCom(Get_StdAPI());
		sys.Init = CommonInit;
		sys.Main = CommonMain;
		sys.Free = CommonShutdown;
		break;
	case CREDITS:
		Sys_Print( show_credits );
		Sys_WaitForQuit();
		Sys_Exit();
		break;
	case HOST_INSTALL:
		// FS_UpdateEnvironmentVariables() is done, quit now
		Sys_Exit();
		break;
	case DEFAULT:
		Sys_Error("CreateInstance: unsupported instance\n");		
		break;
	}

	// init our host now!
	sys.Init( sys.progname, fs_argc, fs_argv );

	// hide console if needed
	switch(sys.app_name)
	{
		case HOST_SHARED:
		case HOST_EDITOR:
			Con_ShowConsole( false );
			break;
	}
}

/*
=================
Base Entry Point
=================
*/
DLLEXPORT int CreateAPI( char *funcname )
{
	// memeber name
	com_strncpy( sys.progname, funcname, sizeof(sys.progname));

	Sys_Init();
	CreateInstance();
	sys.Main(); // ok, starting host
	Sys_Exit(); // normal quit from appilcation
                                                       
	return 0;
}