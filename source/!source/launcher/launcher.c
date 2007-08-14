//=======================================================================
//			Copyright XashXT Group 2007 ©
//			launcher.c - main engine launcher
//=======================================================================

#include "launcher.h"
#include <math.h>

static int app_name;
bool hooked_out = false;
bool log_active = false;
bool show_always = true;
char dllname[64];

//app name
typedef enum 
{
	DEFAULT =	0,	// host_init( funcname *arg ) same much as:
	HOST_SHARED,	// "host_shared"
	HOST_DEDICATED,	// "host_dedicated"
	HOST_EDITOR,	// "host_editor"
	HOST_COMPILERS,	// marker
	BSPLIB,		// "bsplib"
	SPRITE,		// "sprite"
	STUDIO,		// "studio"
	CREDITS,		// misc
};

int com_argc;
char *com_argv[MAX_NUM_ARGVS];
char progname[32];
HINSTANCE	linked_dll;
platform_exp_t	*pi; //callback to utilities
static double start, end;
byte *mempool; //generic mempoolptr

/*
==================
Parse program name to launch and determine work style

NOTE: at this day we have seven instnaces

1. "host_shared" - normal game launch
2. "host_dedicated" - dedicated server
3. "host_editor" - resource editor
4. "bsplib" - three BSP compilers in one
5. "sprite" - sprite creator (requires qc. script)
6. "studio" - Half-Life style models creatror (requires qc. script) 
7. "credits" - display credits of engine developers

This list will be expnaded in future
==================
*/
void LookupInstance( const char *funcname )
{
	//memeber name
	strcpy( progname, funcname );

	//lookup all instances
	if(!strcmp(progname, "host_shared"))
	{
		app_name = HOST_SHARED;
		console_read_only = true;
		//don't show console as default
		if(!debug_mode) show_always = false;
		strcpy(dllname, "bin/engine.dll" );
	}
	else if(!strcmp(progname, "host_dedicated"))
	{
		app_name = HOST_DEDICATED;
		console_read_only = false;
		strcpy(dllname, "bin/engine.dll" );
	}
	else if(!strcmp(progname, "host_editor"))
	{
		app_name = HOST_EDITOR;
		console_read_only = true;
		//don't show console as default
		if(!debug_mode) show_always = false;
		strcpy(dllname, "bin/editor.dll" );
	}
	else if(!strcmp(progname, "bsplib"))
	{
		app_name = BSPLIB;
		strcpy(dllname, "bin/platform.dll" );
	}
	else if(!strcmp(progname, "sprite"))
	{
		app_name = SPRITE;
		strcpy(dllname, "bin/platform.dll" );
	}
	else if(!strcmp(progname, "studio"))
	{
		app_name = STUDIO;
		strcpy(dllname, "bin/platform.dll" );
	}
	else if(!strcmp(progname, "credits")) //easter egg
	{
		app_name = CREDITS;
	}
	else app_name = DEFAULT;
}

/*
==================
PlatformInit

platform.dll needs for some setup operations
so do it manually
==================
*/
void PlatformInit ( char *funcname, int argc, char **argv )
{
	byte bspflags = 0;
	char mapname[64], gamedir[64];

	if(pi->apiversion != PLATFORM_API_VERSION)
		Sys_Error("mismatch version (%i should be %i)\n", pi->apiversion, PLATFORM_API_VERSION);
	if(pi->api_size != sizeof(platform_exp_t))
		Sys_Error("mismatch interface size (%i should be %i)\n", pi->api_size, sizeof(platform_exp_t));		

	pi->Init( argc, argv );

	if(!GetParmFromCmdLine("-game", gamedir ))
		strncpy(gamedir, "xash", sizeof(gamedir));
	if(!GetParmFromCmdLine("+map", mapname ))
		strncpy(mapname, "newmap", sizeof(mapname));
		
	if(CheckParm("-vis")) bspflags |= BSP_ONLYVIS;
	if(CheckParm("-rad")) bspflags |= BSP_ONLYRAD;
	if(CheckParm("-full")) bspflags |= BSP_FULLCOMPILE;
	if(CheckParm("-onlyents")) bspflags |= BSP_ONLYENTS;

	switch(app_name)
	{
	case BSPLIB:
	          // this does nothing
		pi->Compile.PrepareBSP( gamedir, mapname, bspflags );
		break;
	case SPRITE:
		pi->InitRootDir(".");
		start = pi->DoubleTime();
		break;
	case STUDIO:
		pi->InitRootDir(".");
		start = pi->DoubleTime();
		break;
	case DEFAULT:
		break;
	}
}

void PlatformMain ( void )
{
	search_t	*search;
	char qcfilename[64], typemod[16];
	int i, numCompiledMods = 0;
	bool (*CompileMod)( byte *mempool, const char *name, byte parms ) = NULL;
	byte parms = 0; //future expansion

	switch(app_name)
	{
	case SPRITE: 
		CompileMod = pi->Compile.Sprite;
		strcpy(typemod, "sprites" );
		break;
	case STUDIO:
		CompileMod = pi->Compile.Studio;
		strcpy(typemod, "models" );
		break;
	case BSPLIB: 
		pi->Compile.BSP(); 
		strcpy(typemod, "maps" );
		break;
	case DEFAULT:
		strcpy(typemod, "things" );
		break;
	}

	if(!CompileMod) return;//back to shutdown

	mempool = Mem_AllocPool("compiler");
	if(!GetParmFromCmdLine("-qcfile", qcfilename ))
	{
		//search for all .ac files in folder		
		search = pi->Fs.Search("*.qc", true );
		if(!search) Sys_Error("no qcfiles found in this folder!\n");

		for( i = 0; i < search->numfilenames; i++ )
		{
			if(CompileMod( mempool, search->filenames[i], parms ))
				numCompiledMods++;
		}
	}
	else CompileMod( mempool, qcfilename, parms );

	end = pi->DoubleTime();
	Msg ("%5.1f seconds elapsed\n", end - start);
	if(numCompiledMods > 1) Msg("total %d %s compiled\n", numCompiledMods, typemod );
}

void PlatformShutdown ( void )
{
	Mem_Check(); //check for leaks
	Mem_FreePool( &mempool );
	pi->Shutdown;
}


/*
==================
Find needed library, setup and run it
==================
*/
void CreateInstance( void )
{
	
	stdinout_api_t  std;//import

	//export
	platform_exp_t	*(*CreatePLAT)( stdinout_api_t *);

	launcher_t	CreateHost;
	launcher_exp_t	Host;          
	
	//setup sysfuncs
	std.printf = Msg;
	std.dprintf = MsgDev;
	std.error = Sys_Error;
	std.exit = Sys_Exit;
	std.print = Sys_Print;
	std.input = Sys_Input;

	switch(app_name)
	{
	case HOST_SHARED:
	case HOST_DEDICATED:
	case HOST_EDITOR:		
		if (( linked_dll = LoadLibrary( dllname )) == 0 )
			Sys_Error("CreateInstance: couldn't load %s\n", dllname );
		if ((CreateHost = (void *)GetProcAddress( linked_dll, "CreateAPI" ) ) == 0 )
			Sys_Error("CreateInstance: %s has no valid entry point\n", dllname );

		//set callback
		Host = CreateHost( std );
		Host_Init = Host.Init;
		Host_Main = Host.Main;
		Host_Free = Host.Free;
		break;
	case BSPLIB:
	case SPRITE:
	case STUDIO:
		if (( linked_dll = LoadLibrary( "bin/platform.dll" )) == 0 )
			Sys_Error("CreateInstance: couldn't load bin/platform.dll\n");
		if ((CreatePLAT = (void *)GetProcAddress( linked_dll, "CreateAPI" )) == 0 )
			Sys_Error("CreateInstance: bin/platform.dll has no valid entry point\n");
		//set callback
		pi = CreatePLAT( &std );

		Host_Init = PlatformInit;
		Host_Main = PlatformMain;
		Host_Free = PlatformShutdown;
		break;
	case CREDITS:
		//blank
		break;
	case DEFAULT:
		Sys_Error("CreateInstance: unsupported instance\n");		
		break;
	}

	//that's all right, mr. freeman
	Host_Init( progname, com_argc, com_argv );//init our host now!

	//hide console if needed
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
	MsgDev = NullVarArgs;
}

void API_SetConsole( void )
{
	if( !hooked_out && app_name < HOST_COMPILERS)
	{
		Sys_InitConsole = Sys_CreateConsoleW;
		Sys_FreeConsole = Sys_DestroyConsoleW;
          	Sys_ShowConsole = Sys_ShowConsoleW;
		Sys_Print = Sys_PrintW;
		Sys_Input = Sys_InputW;
	}
	else Sys_Print = printf;

	Msg = Sys_MsgW;
	MsgDev = Sys_MsgDevW;
	Sys_Error = Sys_ErrorW;
}


void InitLauncher( char *funcname )
{
	HANDLE hStdout;
	
	API_Reset();//filled stdinout api
	
	//get current hInstance first
	base_hInstance = (HINSTANCE)GetModuleHandle( NULL );

	//check for hooked out
	hStdout = GetStdHandle (STD_OUTPUT_HANDLE);

	if(CheckParm ("-debug")) debug_mode = true;
	if(CheckParm ("-log")) log_active = true;
	if(abs((short)hStdout) < 100) hooked_out = false;
	else hooked_out = true;
          
	//init launcher
	LookupInstance( funcname );
	HOST_MakeStubs();//make sure what all functions are filled
	API_SetConsole(); //initialize system console
	Sys_InitConsole();

	// set working directory
	UpdateEnvironmentVariables();

	// first text message into console or log
	Msg("\n------- Loading bin/launcher.dll [%g] -------\n\n", LAUNCHER_VERSION );
	CreateInstance();

	//NOTE: host will working in loop mode and never returned
	//control without reason
	Host_Main();//ok, starting host
	Sys_Exit();//normal quit from appilcation
}

/*
=================
Base Entry Point
=================
*/

DLLEXPORT int CreateAPI( char *funcname, LPSTR lpCmdLine )
{
	//parse and copy args into local array
	ParseCommandLine( lpCmdLine );
	InitLauncher( funcname );

	return 0;
}