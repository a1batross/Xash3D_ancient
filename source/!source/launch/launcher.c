//=======================================================================
//			Copyright XashXT Group 2007 ©
//			launcher.c - main engine launcher
//=======================================================================

#include "launcher.h"

static int app_name;
bool hooked_out = false;
bool log_active = false;
bool show_always = true;
bool about_mode = false;
bool sys_error = false;
char caption[64];

const char *show_credits = "\n\n\n\n\tCopyright XashXT Group 2007 ©\n\t          All Rights Reserved\n\n\t           Visit www.xash.ru\n";

dll_info_t common_dll = { "common.dll", NULL, "CreateAPI", NULL, NULL, true, COMMON_API_VERSION, sizeof(common_exp_t) };
dll_info_t engine_dll = { "engine.dll", NULL, "CreateAPI", NULL, NULL, true, LAUNCH_API_VERSION, sizeof(launch_exp_t) };
dll_info_t editor_dll = { "editor.dll", NULL, "CreateAPI", NULL, NULL, true, LAUNCH_API_VERSION, sizeof(launch_exp_t) };
dll_info_t *linked_dll; // generic hinstance

//app name
typedef enum 
{
	DEFAULT =	0,	// host_init( funcname *arg ) same much as:
	HOST_SHARED,	// "host_shared"
	HOST_DEDICATED,	// "host_dedicated"
	HOST_EDITOR,	// "host_editor"
	BSPLIB,		// "bsplib"
	IMGLIB,		// "imglib"
	QCCLIB,		// "qcclib"
	SPRITE,		// "sprite"
	STUDIO,		// "studio"
	CREDITS,		// misc
};

int		com_argc;
char		*com_argv[MAX_NUM_ARGVS];
char		progname[32];	// limit of funcname
common_exp_t	*Com;		// callback to utilities
launch_exp_t	*Host;		// callback to mainframe 
static double	start, end;
byte		*mempool;		// generic mempoolptr


/*
==================
Parse program name to launch and determine work style

NOTE: at this day we have seven instnaces

1. "host_shared" - normal game launch
2. "host_dedicated" - dedicated server
3. "host_editor" - resource editor
4. "bsplib" - three BSP compilers in one
5. "qcclib" - quake c complier
5. "sprite" - sprite creator (requires qc. script)
6. "studio" - Half-Life style models creatror (requires qc. script) 
7. "credits" - display credits of engine developers

This list will be expnaded in future
==================
*/
void LookupInstance( const char *funcname )
{
	//memeber name
	strncpy( progname, funcname, sizeof(progname));

	//lookup all instances
	if(!strcmp(progname, "host_shared"))
	{
		app_name = HOST_SHARED;
		console_read_only = true;
		//don't show console as default
		if(!debug_mode) show_always = false;
		linked_dll = &engine_dll;	// pointer to engine.dll info
		strcpy(log_path, "engine.log" ); // xash3d root directory
		strcpy(caption, va("Xash3D ver.%g", CalcEngineVersion()));
	}
	else if(!strcmp(progname, "host_dedicated"))
	{
		app_name = HOST_DEDICATED;
		console_read_only = false;
		linked_dll = &engine_dll;	// pointer to engine.dll info
		strcpy(log_path, "engine.log" ); // xash3d root directory
		strcpy(caption, va("Xash3D Dedicated Server ver.%g", CalcEngineVersion()));
	}
	else if(!strcmp(progname, "host_editor"))
	{
		app_name = HOST_EDITOR;
		console_read_only = true;
		//don't show console as default
		if(!debug_mode) show_always = false;
		linked_dll = &editor_dll;	// pointer to editor.dll info
		strcpy(log_path, "editor.log" ); // xash3d root directory
		strcpy(caption, va("Xash3D Editor ver.%g", CalcEditorVersion()));
	}
	else if(!strcmp(progname, "bsplib"))
	{
		app_name = BSPLIB;
		linked_dll = &common_dll;	// pointer to common.dll info
		strcpy(log_path, "bsplib.log" ); // xash3d root directory
		strcpy(caption, "Xash3D BSP Compiler");
	}
	else if(!strcmp(progname, "imglib"))
	{
		app_name = IMGLIB;
		linked_dll = &common_dll;	// pointer to common.dll info
		sprintf(log_path, "%s/convert.log", sys_rootdir ); // same as .exe file
		strcpy(caption, "Xash3D Image Converter");
	}
	else if(!strcmp(progname, "qcclib"))
	{
		app_name = QCCLIB;
		linked_dll = &common_dll;	// pointer to common.dll info
		sprintf(log_path, "%s/compile.log", sys_rootdir ); // same as .exe file
		strcpy(caption, "Xash3D QuakeC Compiler");
	}
	else if(!strcmp(progname, "sprite"))
	{
		app_name = SPRITE;
		linked_dll = &common_dll;	// pointer to common.dll info
		sprintf(log_path, "%s/spritegen.log", sys_rootdir ); // same as .exe file
		strcpy(caption, "Xash3D Sprite Compiler");
	}
	else if(!strcmp(progname, "studio"))
	{
		app_name = STUDIO;
		linked_dll = &common_dll;	// pointer to common.dll info
		sprintf(log_path, "%s/studiomdl.log", sys_rootdir ); // same as .exe file
		strcpy(caption, "Xash3D Studio Models Compiler");
	}
	else if(!strcmp(progname, "credits")) //easter egg
	{
		app_name = CREDITS;
		linked_dll = NULL;	// no need to loading library
		log_active = dev_mode = debug_mode = 0; //clear all dbg states
		strcpy(caption, "About");
		about_mode = true;
	}
	else 
	{
		app_name = DEFAULT;
	}
}

stdlib_api_t *Get_StdAPI( void )
{
	static stdlib_api_t std;

	// setup sysfuncs
	std.printf = Msg;
	std.dprintf = MsgDev;
	std.wprintf = MsgWarn;
	std.error = Sys_Error;
	std.exit = Sys_Exit;
	std.print = Sys_Print;
	std.input = Sys_Input;
	std.sleep = Sys_Sleep;

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
	byte bspflags = 0, qccflags = 0;
	char source[64], gamedir[64];

	Com->Init( argc, argv );

	switch(app_name)
	{
	case BSPLIB:
		if(!GetParmFromCmdLine("-game", gamedir ))
			strncpy(gamedir, "xash", sizeof(gamedir));
		if(!GetParmFromCmdLine("+map", source ))
			strncpy(source, "newmap", sizeof(source));
		if(CheckParm("-vis")) bspflags |= BSP_ONLYVIS;
		if(CheckParm("-rad")) bspflags |= BSP_ONLYRAD;
		if(CheckParm("-full")) bspflags |= BSP_FULLCOMPILE;
		if(CheckParm("-onlyents")) bspflags |= BSP_ONLYENTS;

		Com->Compile.PrepareBSP( gamedir, source, bspflags );
		break;
	case QCCLIB:
		if(!GetParmFromCmdLine("+dat", source ))
			strncpy(source, "progs", sizeof(source));
		if(CheckParm("-progdefs")) qccflags |= QCC_PROGDEFS;
		if(CheckParm("/O0")) qccflags |= QCC_OPT_LEVEL_0;
		if(CheckParm("/O1")) qccflags |= QCC_OPT_LEVEL_1;
		if(CheckParm("/O2")) qccflags |= QCC_OPT_LEVEL_2;
		if(CheckParm("/O2")) qccflags |= QCC_OPT_LEVEL_3;

		Com->Compile.PrepareDAT( gamedir, source, qccflags );	
		break;
	case IMGLIB:
	case SPRITE:
	case STUDIO:
		Com->InitRootDir(".");
		start = Com->DoubleTime();
		break;
	case DEFAULT:
		break;
	}
}

void CommonMain ( void )
{
	search_t	*search;
	char filename[MAX_QPATH], typemod[16], searchmask[16];
	bool (*CompileMod)( byte *mempool, const char *name, byte parms ) = NULL;
	byte parms = 0; // future expansion
	int i, numCompiledMods = 0;

	switch(app_name)
	{
	case SPRITE: 
		CompileMod = Com->Compile.Sprite;
		strcpy(typemod, "sprites" );
		strcpy(searchmask, "*.qc" );
		break;
	case STUDIO:
		CompileMod = Com->Compile.Studio;
		strcpy(typemod, "models" );
		strcpy(searchmask, "*.qc" );
		break;
	case IMGLIB:
		CompileMod = Com->Compile.Image; 
		strcpy(typemod, "images" );
		strcpy(searchmask, "*.pcx" );
		break;		
	case BSPLIB: 
		Com->Compile.BSP(); 
		strcpy(typemod, "maps" );
		strcpy(searchmask, "*.map" );
		break;
	case QCCLIB: 
		Com->Compile.DAT(); 
		strcpy(typemod, "progs" );
		strcpy(searchmask, "*.src" );
		break;
	case DEFAULT:
		strcpy(typemod, "things" );
		strcpy(searchmask, "*.*" );
		break;
	}
	if(!CompileMod) return;//back to shutdown

	mempool = Mem_AllocPool("compiler");
	if(!GetParmFromCmdLine("-file", filename ))
	{
		//search for all .ac files in folder		
		search = Com->Fs.Search(searchmask, true );
		if(!search) Sys_Error("no %s found in this folder!\n", searchmask );

		for( i = 0; i < search->numfilenames; i++ )
		{
			if(CompileMod( mempool, search->filenames[i], parms ))
				numCompiledMods++;
		}
	}
	else CompileMod( mempool, filename, parms );

	end = Com->DoubleTime();
	Msg ("%5.1f seconds elapsed\n", end - start);
	if(numCompiledMods > 1) Msg("total %d %s compiled\n", numCompiledMods, typemod );
}

void CommonShutdown ( void )
{
	Mem_Check(); //check for leaks
	Mem_FreePool( &mempool );
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
	MsgDev(D_INFO, "Sys_LoadLibrary: Loading launch.dll [%d] - ok\n", INIT32_API_VERSION );

	Sys_LoadLibrary( linked_dll ); // loading library if need

	switch(app_name)
	{
	case HOST_SHARED:
	case HOST_DEDICATED:
	case HOST_EDITOR:		
		CreateHost = (void *)linked_dll->main;

		// set callback
		Host = CreateHost( Get_StdAPI());
		Host_Init = Host->Init;
		Host_Main = Host->Main;
		Host_Free = Host->Free;
		break;
	case BSPLIB:
	case QCCLIB:
	case IMGLIB:
	case SPRITE:
	case STUDIO:
		CreateCom = (void *)linked_dll->main;
		
		// set callback
		Com = CreateCom(Get_StdAPI());
		Host_Init = CommonInit;
		Host_Main = CommonMain;
		Host_Free = CommonShutdown;
		break;
	case CREDITS:
		Sys_Print( show_credits );
		Sys_WaitForQuit();
		Sys_Exit();
		break;
	case DEFAULT:
		Sys_Error("CreateInstance: unsupported instance\n");		
		break;
	}

	// that's all right, mr. freeman
	Host_Init( progname, com_argc, com_argv );// init our host now!

	// hide console if needed
	switch(app_name)
	{
		case HOST_SHARED:
		case HOST_EDITOR:
			Sys_ShowConsole( false );
			break;
	}
}

void HOST_MakeStubs( void )
{
	Host_Init = NullInit;
	Host_Main = NullVoid;
	Host_Free = NullVoid;
}

void API_Reset( void )
{
	Sys_InitConsole = NullVoid;
	Sys_FreeConsole = NullVoid;
          Sys_ShowConsole = NullVoidWithArg;
	
	Sys_Input = NullChar;
	Sys_Error = NullVarArgs;

	Msg = NullVarArgs;
	MsgDev = NullVarArgs2;
	MsgWarn = NullVarArgs;
}

void API_SetConsole( void )
{
	if( hooked_out && app_name > HOST_EDITOR)
	{
		Sys_Print = Sys_PrintA;
		Sys_InitConsole = Sys_InitLog;
		Sys_FreeConsole = Sys_CloseLog;
	}
          else
          {
		Sys_InitConsole = Sys_CreateConsoleW;
		Sys_FreeConsole = Sys_DestroyConsoleW;
          	Sys_ShowConsole = Sys_ShowConsoleW;
		Sys_Print = Sys_PrintW;
		Sys_Input = Sys_InputW;
	}

	Msg = Sys_MsgW;
	MsgDev = Sys_MsgDevW;
	MsgWarn = Sys_MsgWarnW;
	Sys_Error = Sys_ErrorW;
}

/*
=================
Base Entry Point
=================
*/
DLLEXPORT int CreateAPI( char *funcname )
{
	HANDLE		hStdout;
	OSVERSIONINFO	vinfo;
	MEMORYSTATUS	lpBuffer;
	char		dev_level[4];

	lpBuffer.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus (&lpBuffer);

	// parse and copy args into local array
	ParseCommandLine(GetCommandLine());
	
	API_Reset();// fill stdlib api
	
	// get current hInstance first
	base_hInstance = (HINSTANCE)GetModuleHandle( NULL );
	vinfo.dwOSVersionInfoSize = sizeof(vinfo);
	hStdout = GetStdHandle (STD_OUTPUT_HANDLE); // check for hooked out

	if(CheckParm ("-debug")) debug_mode = true;
	if(CheckParm ("-log")) log_active = true;

	// ugly hack to get pipeline state, but it works
	if(abs((short)hStdout) < 100) hooked_out = false;
	else hooked_out = true;
	if(GetParmFromCmdLine("-dev", dev_level ))
		dev_mode = atoi(dev_level);

	UpdateEnvironmentVariables(); // set working directory
          
	// init launcher
	LookupInstance( funcname );
	HOST_MakeStubs();//make sure what all functions are filled
	API_SetConsole(); //initialize system console
	Sys_InitConsole();

	if(!GetVersionEx (&vinfo)) Sys_Error ("InitLauncher: Couldn't get OS info\n");
	if(vinfo.dwMajorVersion < 4) Sys_Error ("InitLauncher: Win%d is not supported\n", vinfo.dwMajorVersion );

	CreateInstance();

	// NOTE: host will working in loop mode and never returned
	// control without reason
	Host_Main(); // ok, starting host
	Sys_Exit(); // normal quit from appilcation

	return 0;
}