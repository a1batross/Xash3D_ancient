//=======================================================================
//			Copyright XashXT Group 2007 ©
//			host.c - dedicated and shared host
//=======================================================================

#include <setjmp.h>
#include <windows.h>
#include <dsound.h>
#include "engine.h"

common_exp_t    	*Com;	// fundamental callbacks
physic_exp_t	*Phys;
host_parm_t	host;	// host parms
stdlib_api_t	std;

byte	*zonepool;
int	ActiveApp;
bool	Minimized;
char	*buildstring = __TIME__ " " __DATE__;

void Key_Init (void);
void SCR_EndLoadingPlaque (void);

HINSTANCE	global_hInstance;
dll_info_t common_dll = { "common.dll", NULL, "CreateAPI", NULL, NULL, true, COMMON_API_VERSION, sizeof(common_exp_t) };
dll_info_t physic_dll = { "physic.dll", NULL, "CreateAPI", NULL, NULL, true, PHYSIC_API_VERSION, sizeof(physic_exp_t) };

cvar_t	*timescale;
cvar_t	*fixedtime;

stdlib_api_t Host_GetStdio( bool crash_on_error )
{
	static stdlib_api_t		io;

	io.api_size = sizeof(stdlib_api_t); 

	io.print = Con_Print;
	io.printf = Con_Printf;
	io.dprintf = Con_DPrintf;
	io.wprintf = Con_DWarnf;
	io.exit = Sys_Quit;
	io.input = Sys_ConsoleInput;
	io.sleep = Sys_Sleep;

	if(crash_on_error) io.error = Sys_Error;
	else io.error = Host_Error;

	io.LoadLibrary = Sys_LoadLibrary;
	io.FreeLibrary = Sys_FreeLibrary;
	io.GetProcAddress = std.GetProcAddress;

	return io;
}

void Host_InitCommon( char *funcname, int argc, char **argv )
{
	common_t		CreateCom;         
	stdlib_api_t	io = Host_GetStdio( true );

	Sys_LoadLibrary( &common_dll );

	CreateCom = (void *)common_dll.main;
	Com = CreateCom( &io );
	
	Com->Init( argc, argv );

	// TODO: init basedir here
	Com->LoadGameInfo("gameinfo.txt");
	zonepool = Mem_AllocPool("Zone Engine");
}

void Host_FreeCommon( void )
{
	if(common_dll.link)
	{
		Mem_FreePool( &zonepool );
		Com->Shutdown();
	}
	Sys_FreeLibrary( &common_dll );
}

void Host_InitPhysic( void )
{
	static physic_imp_t		pi;
	physic_t			CreatePhys;  

	pi.Fs = Com->Fs;
	pi.VFs = Com->VFs;
	pi.Mem = Com->Mem;
	pi.Script = Com->Script;
	pi.Compile = Com->Compile;
	pi.Stdio = Host_GetStdio( false );

	// phys callback
	pi.Transform = SV_Transform;

	Sys_LoadLibrary( &physic_dll );

	CreatePhys = (void *)physic_dll.main;
	Phys = CreatePhys( &pi );
	
	Phys->Init();
}

void Host_FreePhysic( void )
{
	if(physic_dll.link)
	{
		Phys->Shutdown();
	}
	Sys_FreeLibrary( &physic_dll );
}

/*
================
Host_AbortCurrentFrame

aborts the current host frame and goes on with the next one
================
*/
void Host_AbortCurrentFrame( void )
{
	longjmp(host.abortframe, 1);
}

/*
=================
Host_Init
=================
*/
void Host_Init (char *funcname, int argc, char **argv)
{
	char	*s;

	host.state = HOST_INIT;	//initialzation started

	global_hInstance = (HINSTANCE)GetModuleHandle( NULL );

	if(!strcmp(funcname, "host_dedicated")) host.type = HOST_DEDICATED;
	else if(!strcmp(funcname, "host_shared")) host.type = HOST_NORMAL;
	else host.type = HOST_OFFLINE; // launcher can loading engine for some reasons

	COM_InitArgv (argc, argv); // init host.debug & host.developer here
	Host_InitCommon( funcname, argc, argv );

	Cbuf_Init ();
	Cmd_Init ();
	Cvar_Init ();
	Key_Init ();
	PRVM_Init();

	// we need to add the early commands twice, because
	// a basedir or cddir needs to be set before execing
	// config files, but we want other parms to override
	// the settings of the config files
	Cbuf_AddEarlyCommands (false);
	Cbuf_Execute ();

	Cbuf_AddText ("exec default.cfg\n");
	Cbuf_AddText ("exec config.cfg\n");
	Cbuf_AddEarlyCommands (true);
	Cbuf_Execute ();

	// init commands and vars
	Cmd_AddCommand ("error", Host_Error_f);

	host_speeds = Cvar_Get ("host_speeds", "0", 0);
	host_frametime = Cvar_Get ("host_frametime", "0.01", 0);
	timescale = Cvar_Get ("timescale", "1", 0);
	fixedtime = Cvar_Get ("fixedtime", "0", 0);
	if(host.type == HOST_DEDICATED) dedicated = Cvar_Get ("dedicated", "1", CVAR_NOSET);
	else dedicated = Cvar_Get ("dedicated", "0", CVAR_NOSET);

	s = va("%4.2f %s %s %s", VERSION, "x86", __DATE__, BUILDSTRING);
	Cvar_Get ("version", s, CVAR_SERVERINFO|CVAR_NOSET);

	if (dedicated->value) Cmd_AddCommand ("quit", Sys_Quit);
        
	NET_Init();
	Netchan_Init();
	Host_InitPhysic();
          
	SV_Init();
	CL_Init();

	// add + commands from command line
	if (!Cbuf_AddLateCommands ())
	{
		// if the user didn't give any commands, run default action
		if (!dedicated->value) Cbuf_AddText ("d1\n");
		else Cbuf_AddText ("dedicated_start\n");
		Cbuf_Execute ();
	}
	else
	{	// the user asked for something explicit
		// so drop the loading plaque
		SCR_EndLoadingPlaque ();
	}
	Sys_DoubleTime(); // initialize timer
}

/*
=================
Host_Frame
=================
*/
void Host_Frame (double time)
{
	char		*s;
	static double	time_before, time_between, time_after;

	if (setjmp(host.abortframe)) return;

	rand(); // keep the random time dependent

	// get new key events
	Sys_SendKeyEvents();

	do
	{
		s = Sys_ConsoleInput ();
		if(s) Cbuf_AddText (va("%s\n",s));
	} while (s);
	Cbuf_Execute ();

	// if at a full screen console, don't update unless needed
	if (Minimized || host.type == HOST_DEDICATED )
	{
		Sys_Sleep (1);
	}

	if (host_speeds->value) time_before = Sys_DoubleTime();

	SV_Frame (time);

	if (host_speeds->value) time_between = Sys_DoubleTime();		

	CL_Frame (time);

	if (host_speeds->value) time_after = Sys_DoubleTime();		
	if (host_speeds->value)
	{
		double all, sv, gm, cl, rf;

		all = time_after - time_before;
		sv = time_between - time_before;
		cl = time_after - time_between;
		gm = time_after_game - time_before_game;
		rf = time_after_ref - time_before_ref;
		sv -= gm;
		cl -= rf;
		Msg ("all:%.3f sv:%.3f gm:%.3f cl:%.3f rf:%.3f\n", all, sv, gm, cl, rf);
	}	
	host.framecount++;
}

/*
=================
Host_Main
=================
*/
void Host_Main( void )
{
	static double	time, oldtime, newtime;

	oldtime = host.realtime;

	// main window message loop
	while (host.type != HOST_OFFLINE)
	{
		oldtime = newtime;
		newtime = Sys_DoubleTime();
		time = newtime - oldtime;

		Host_Frame (time); // engine frame
	}
	host.state = HOST_SHUTDOWN;
}


/*
=================
Host_Shutdown
=================
*/
void Host_Free (void)
{
	SV_Shutdown ("Server shutdown\n", false);
	CL_Shutdown ();
	NET_Shutdown();
	Host_FreePhysic();
	Host_FreeCommon();
}

/*
=================
Host_Error
=================
*/
void Host_Error( const char *error, ... )
{
	static char	hosterror1[MAX_INPUTLINE];
	static char	hosterror2[MAX_INPUTLINE];
	static bool	recursive = false;
	va_list		argptr;

	va_start( argptr, error );
	vsprintf( hosterror1, error, argptr );
	va_end( argptr );

	if (host.framecount < 3 || host.state == HOST_SHUTDOWN)
		Sys_Error ("%s", hosterror1 );
	else Msg("Host_Error: %s", hosterror1);

	if(recursive)
	{ 
		Msg("Host_Error: recursive %s", hosterror2);
		Sys_Error ("%s", hosterror1);
	}
	recursive = true;
	strncpy(hosterror2, hosterror1, sizeof(hosterror2));

	SV_Shutdown (va("Server crashed: %s", hosterror1), false);
	CL_Drop(); // drop clients

	recursive = false;
	Host_AbortCurrentFrame();
	host.state = HOST_ERROR;
}

void Host_Error_f( void )
{
	Host_Error( "%s\n", Cmd_Argv(1));
}