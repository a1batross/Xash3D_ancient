//=======================================================================
//			Copyright XashXT Group 2007 ©
//			host.c - dedicated and shared host
//=======================================================================

#include <setjmp.h>
#include <windows.h>
#include <dsound.h>
#include "engine.h"

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
dll_info_t physic_dll = { "physic.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(physic_exp_t) };

cvar_t	*timescale;
cvar_t	*fixedtime;

stdlib_api_t Host_GetStdio( bool crash_on_error )
{
	static stdlib_api_t		io;

	io = std;

	// overload some funcs
	io.print = Com_Print;
	io.printf = Com_Printf;
	io.dprintf = Com_DPrintf;
	io.wprintf = Com_DWarnf;

	if(crash_on_error) io.error = Sys_Error;
	else io.error = Host_Error;

	return io;
}

void Host_InitCommon( uint funcname, int argc, char **argv )
{
	// TODO: init basedir here
	FS_LoadGameInfo("gameinfo.txt");
	zonepool = Mem_AllocPool("Zone Engine");
}

void Host_FreeCommon( void )
{
	Mem_FreePool( &zonepool );
}

void Host_InitPhysic( void )
{
	static physic_imp_t		pi;
	launch_t			CreatePhys;  
	stdlib_api_t		io = Host_GetStdio( false );

	// phys callback
	pi.api_size = sizeof(physic_imp_t);
	pi.Transform = SV_Transform;

	Sys_LoadLibrary( &physic_dll );

	CreatePhys = (void *)physic_dll.main;
	Phys = CreatePhys( &io, &pi );
	
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
void Host_Init (uint funcname, int argc, char **argv)
{
	char	*s;

	host.state = HOST_INIT;	//initialzation started

	global_hInstance = (HINSTANCE)GetModuleHandle( NULL );
	host.type = funcname;

	srand(time(NULL)); // init random generator

	Host_InitCommon( funcname, argc, argv ); // loading common.dll
	Cmd_Init( argc, argv );
	Cvar_Init();
	Key_Init();
	PRVM_Init();

	// get default configuration
#if 1
	Cbuf_AddText("exec keys.rc\n");
	Cbuf_AddText("exec vars.rc\n");
#else
	Cbuf_AddText("exec default.cfg\n");
	Cbuf_AddText("exec config.cfg\n");
#endif
	Cbuf_Execute();

	// init commands and vars
	Cmd_AddCommand ("error", Host_Error_f);

	host_speeds = Cvar_Get ("host_speeds", "0", 0);
	host_frametime = Cvar_Get ("host_frametime", "0.01", 0);
	timescale = Cvar_Get ("timescale", "1", 0);
	fixedtime = Cvar_Get ("fixedtime", "0", 0);
	if(host.type == HOST_DEDICATED) dedicated = Cvar_Get ("dedicated", "1", CVAR_INIT);
	else dedicated = Cvar_Get ("dedicated", "0", CVAR_INIT);

	s = va("Xash %g (%s)", XASH_VERSION, BUILDSTRING);
	Cvar_Get ("version", s, CVAR_SERVERINFO|CVAR_INIT);

	if (dedicated->value) Cmd_AddCommand ("quit", Sys_Quit);
       
	NET_Init();
	Netchan_Init();
	Host_InitPhysic();
       
	SV_Init();
	CL_Init();

	Cbuf_AddText("exec init.rc\n");
	Cbuf_Execute();

	// if stuffcmds wasn't run, then init.rc is probably missing, use default
	if(!host.stuffcmdsrun) Cbuf_ExecuteText( EXEC_NOW, "stuffcmds\n" );

	SCR_EndLoadingPlaque ();
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
	Cbuf_Execute();

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