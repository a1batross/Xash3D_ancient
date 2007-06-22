//=======================================================================
//			Copyright XashXT Group 2007 ©
//			launcher.c - main engine launcher
//=======================================================================

#include "launcher.h"
#include <math.h>

static int app_name;
bool hooked_out = false;
bool show_always = true;

int com_argc;
char *com_argv[MAX_NUM_ARGVS];
char progname[32];
HINSTANCE	linked_dll;

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
7. "qcclib" - QuakeC compiler\decompiler utility (not implemented yet)
8. "credits" - display credits of engine developers

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
	}
	else if(!strcmp(progname, "host_dedicated"))
	{
		app_name = HOST_DEDICATED;
		console_read_only = false;
	}
	else if(!strcmp(progname, "host_editor"))
	{
		app_name = HOST_EDITOR;
		console_read_only = true;
		//don't show console as default
		if(!debug_mode) show_always = false;
	}
	else if(!strcmp(progname, "bsplib"))
	{
		app_name = BSPLIB;
	}
	else if(!strcmp(progname, "sprite"))
	{
		app_name = SPRITE;
	}
	else if(!strcmp(progname, "studio"))
	{
		app_name = STUDIO;
	}
	else if(!strcmp(progname, "qcclib"))
	{
		app_name = QCCLIB;
	}
	else if(!strcmp(progname, "credits")) //easter egg
	{
		app_name = CREDITS;
	}
	else app_name = DEFAULT;
}


/*
==================
Find needed library, setup and run it
==================
*/
void CreateInstance( void )
{
	system_api_t  sysapi;//import

	//export
	platform_t	CreatePLAT;
	platform_api_t	pi;

	host_t		CreateHOST;
	host_api_t	hi;          

	editor_t		CreateEDIT;
	edit_api_t	ei;
	
	//setup sysfuncs
	sysapi.sys_msg = Msg;
	sysapi.sys_dev = MsgDev;
	sysapi.sys_err = Sys_Error;
	sysapi.sys_exit = Sys_Exit;
	sysapi.sys_print = Sys_Print;
	sysapi.sys_input = Sys_Input;

	switch(app_name)
	{
	case HOST_SHARED:
	case HOST_DEDICATED:
		if (( linked_dll = LoadLibrary( "bin/engine.dll" )) == 0 )
			Sys_Error("couldn't load engine.dll\n");
		if ((CreateHOST = (void *)GetProcAddress( linked_dll, "CreateAPI" ) ) == 0 )
			Sys_Error("unable to find entry point\n");
		//set callback
		hi = CreateHOST( sysapi );

		Host_Init = hi.host_init;
		Host_Main = hi.host_main;
		Host_Free = hi.host_free;
		break;
	case HOST_EDITOR:
		if (( linked_dll = LoadLibrary( "bin/editor.dll" )) == 0 )
			Sys_Error("couldn't load editor.dll\n");
		if ((CreateEDIT = (void *)GetProcAddress( linked_dll, "CreateAPI" ) ) == 0 )
			Sys_Error("unable to find entry point\n");
		//set callback
		ei = CreateEDIT( sysapi );

		Host_Init = ei.editor_init;
		Host_Main = ei.editor_main;
		Host_Free = ei.editor_free;
		break;
	case BSPLIB:
	case SPRITE:
	case STUDIO:
	case QCCLIB:
		if (( linked_dll = LoadLibrary( "bin/platform.dll" )) == 0 )
			Sys_Error("couldn't load platform.dll\n");
		if ((CreatePLAT = (void *)GetProcAddress( linked_dll, "CreateAPI" ) ) == 0 )
			Sys_Error("unable to find entry point\n");
		//set callback
		pi = CreatePLAT( sysapi );

		Host_Init = pi.plat_init;
		Host_Main = pi.plat_main;
		Host_Free = pi.plat_free;
		break;
	case CREDITS:
		//blank
		break;
	case DEFAULT:
		Sys_Error("unsupported instance\n");		
		break;
	}

	//that's all right, mr. freeman
	Host_Init( progname, com_argc, com_argv );//init our host now!
	MsgDev("\"%s\" initialized\n", progname );

	//hide console if needed
	if(debug_mode)
	{
		switch(app_name)
		{
			case HOST_SHARED:
			case HOST_EDITOR:
				Sys_ShowConsole( false );
				break;
		}
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

void Sys_LastError( void )
{
	//Sys_Error( GetLastError() );
}


void API_SetConsole( void )
{
	if( hooked_out && app_name > HOST_EDITOR)
	{
		Sys_Print = printf;
	}
          else
          {
		Sys_InitConsole = Sys_CreateConsoleW;
		Sys_FreeConsole = Sys_DestroyConsoleW;
          	Sys_ShowConsole = Sys_ShowConsoleW;
		Sys_Print = Sys_PrintW;
		Sys_Input = Sys_InputW;
	}

	Sys_Error = Sys_ErrorW;
	//unexpected_handler = Sys_LastError;

	Msg = Sys_MsgW;
	MsgDev = Sys_MsgDevW;
}


void InitLauncher( char *funcname )
{
	HANDLE hStdout;
	
	API_Reset();//filled std api
	
	//get current hInstance first
	base_hInstance = (HINSTANCE)GetModuleHandle( NULL );

	//check for hooked out
	hStdout = GetStdHandle (STD_OUTPUT_HANDLE);

	if(CheckParm ("-debug")) debug_mode = true;
	if(abs((short)hStdout) < 100) hooked_out = false;
	else hooked_out = true;
          
	//init launcher
	LookupInstance( funcname );
	HOST_MakeStubs();//make sure what all functions are filled
	API_SetConsole(); //initialize system console
	Sys_InitConsole();

	UpdateEnvironmentVariables();

	MsgDev("launcher.dll version %g\n", LAUNCHER_VERSION );
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