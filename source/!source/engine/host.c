//=======================================================================
//			Copyright XashXT Group 2007 ©
//			host.c - dedicated and shared host
//=======================================================================

#include <setjmp.h>
#include <windows.h>
#include <dsound.h>
#include "engine.h"
#include "progsvm.h"

platform_exp_t	*pi;	// fundamental callbacks
host_parm_t	host;	// host parms

byte	*zonepool;
int	ActiveApp;

// host params

void Key_Init (void);
void SCR_EndLoadingPlaque (void);

HINSTANCE	global_hInstance;
HINSTANCE	platform_dll;

cvar_t	*timescale;
cvar_t	*fixedtime;
cvar_t	*showtrace;

void Host_InitPlatform( char *funcname, int argc, char **argv )
{
	stdinout_api_t	pistd;
	platform_t	CreatePlat;         

	//make callbacks
	pistd.printf = Msg;
	pistd.dprintf = MsgDev;
	pistd.wprintf = MsgWarn;
	pistd.error = Sys_Error;
	
	if (( platform_dll = LoadLibrary( "bin/platform.dll" )) == 0 )
		Sys_Error( "Couldn't load platform.dll\n" );

	if (( CreatePlat = (void *)GetProcAddress( platform_dll, "CreateAPI" ) ) == 0 )
		Sys_Error("CreateInstance: %s has no valid entry point\n", "platform.dll" );

	pi = CreatePlat( pistd );
	if(pi->apiversion != PLATFORM_API_VERSION)
		Sys_Error("mismatch version (%i should be %i)\n", pi->apiversion, PLATFORM_API_VERSION);

	if(pi->api_size != sizeof(platform_exp_t))
		Sys_Error("mismatch interface size (%i should be %i)\n", pi->api_size, sizeof(platform_exp_t));
	
	//initialize our platform :)
	pi->Init( argc, argv );

	//TODO: init basedir here
	pi->LoadGameInfo("gameinfo.txt");
	zonepool = Mem_AllocPool("Zone Engine");
}

void Host_FreePlatform( void )
{
	if(platform_dll)
	{
		Mem_FreePool( &zonepool );
		pi->Shutdown();
		FreeLibrary( platform_dll );
	}
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
	Host_InitPlatform( funcname, argc, argv );

	MsgDev(D_INFO, "------- Loading bin/engine.dll   [%g] -------\n", ENGINE_VERSION );
	
	Cbuf_Init();
	Cmd_Init();
	Cvar_Init();
	Key_Init();
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
	Cmd_AddCommand ("error", Com_Error_f);

	host_speeds = Cvar_Get ("host_speeds", "0", 0);
	timescale = Cvar_Get ("timescale", "1", 0);
	fixedtime = Cvar_Get ("fixedtime", "0", 0);
	showtrace = Cvar_Get ("showtrace", "0", 0);

	s = va("%4.2f %s %s %s", VERSION, "x86", __DATE__, BUILDSTRING);
	Cvar_Get ("version", s, CVAR_SERVERINFO|CVAR_NOSET);

	if (host.type == HOST_DEDICATED) 
	{
		Cmd_AddCommand ("quit", Com_Quit);
	}

	Sys_Init();
          
	NET_Init ();
	Netchan_Init ();
          
	SV_Init();
	CL_Init();

	// add + commands from command line
	if (!Cbuf_AddLateCommands())
	{
		// if the user didn't give any commands, run default action
		if(host.type == HOST_NORMAL) Cbuf_AddText ("d1\n");
		else Cbuf_AddText ("dedicated_start\n");
		Cbuf_Execute ();
	}
	else
	{	// the user asked for something explicit
		// so drop the loading plaque
		SCR_EndLoadingPlaque ();
	}
}

/*
=================
Host_Frame
=================
*/
void Host_Frame (double time)
{
	char	*s;

	if (setjmp (abortframe) ) return; // an ERR_DROP was thrown

	if (showtrace->value)
	{
		extern	int c_traces, c_brush_traces;
		extern	int	c_pointcontents;

		Msg ("%4i traces  %4i points\n", c_traces, c_pointcontents);
		c_traces = 0;
		c_brush_traces = 0;
		c_pointcontents = 0;
	}

	do
	{
		s = Sys_ConsoleInput ();
		if(s) Cbuf_AddText (va("%s\n",s));
	} while (s);
	Cbuf_Execute ();
	
	SV_Frame (time);
	CL_Frame (time);

	host.framecount++;
}

/*
=================
Host_Main
=================
*/
void Host_Main( void )
{
	MSG		msg;
	static double	oldtime, newtime;

	host.state = HOST_FRAME;
	oldtime = Sys_DoubleTime(); //first call

	// main window message loop
	while (host.type != HOST_OFFLINE)
	{
		// if at a full screen console, don't update unless needed
		if (host.type == HOST_DEDICATED )
		{
			Sleep( 1 );
		}
                    else if(host.state == HOST_SLEEP) Sleep( 100 ); 
		
		while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage (&msg, NULL, 0, 0)) Com_Quit ();
			host.sv_timer = msg.time;
			TranslateMessage (&msg);
   			DispatchMessage (&msg);
		}

		do
		{
			newtime = Sys_DoubleTime();
			host.realtime = newtime - oldtime;

		} while (host.realtime < 0.001);

		Host_Frame (host.realtime);
		oldtime = newtime;
	}
}


/*
=================
Host_Shutdown
=================
*/
void Host_Free (void)
{
	host.state = HOST_SHUTDOWN;
	
	SV_Shutdown ("Server shutdown\n", false);
	CL_Shutdown ();
	Host_FreePlatform();
}