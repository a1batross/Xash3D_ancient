//=======================================================================
//			Copyright XashXT Group 2007 ©
//			host.c - dedicated and shared host
//=======================================================================

#include <setjmp.h>
#include <windows.h>
#include <dsound.h>
#include "engine.h"

platform_exp_t    	*pi;	// fundamental callbacks
host_parm_t	host;	// host parms

byte	*zonepool;
int	ActiveApp;
bool	Minimized;

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
	
	Cbuf_Init ();
	Cmd_Init ();
	Cvar_Init ();
	Key_Init ();

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
	log_stats = Cvar_Get ("log_stats", "0", 0);
	developer = Cvar_Get ("developer", "0", 0);
	timescale = Cvar_Get ("timescale", "1", 0);
	fixedtime = Cvar_Get ("fixedtime", "0", 0);
	showtrace = Cvar_Get ("showtrace", "0", 0);
	if(host.type == HOST_DEDICATED) dedicated = Cvar_Get ("dedicated", "1", CVAR_NOSET);
	else dedicated = Cvar_Get ("dedicated", "0", CVAR_NOSET);

	s = va("%4.2f %s %s %s", VERSION, "x86", __DATE__, BUILDSTRING);
	Cvar_Get ("version", s, CVAR_SERVERINFO|CVAR_NOSET);

	if (dedicated->value) Cmd_AddCommand ("quit", Com_Quit);

	Sys_Init();
          
	NET_Init ();
	Netchan_Init ();
          
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
	char	*s;
	static double	time_before, time_between, time_after;

	if (setjmp (abortframe) ) return; // an ERR_DROP was thrown

	if ( log_stats->modified )
	{
		log_stats->modified = false;
		if ( log_stats->value )
		{
			if ( log_stats_file )
			{
				FS_Close( log_stats_file );
				log_stats_file = 0;
			}
			log_stats_file = FS_Open( "stats.log", "w" );
			if ( log_stats_file )
				FS_Printf( log_stats_file, "entities,dlights,parts,frame time\n" );
		}
		else
		{
			if ( log_stats_file )
			{
				FS_Close( log_stats_file );
				log_stats_file = 0;
			}
		}
	}

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
		if (s) Cbuf_AddText (va("%s\n",s));
	} while (s);
	Cbuf_Execute ();

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
	MSG		msg;
	static double	time, oldtime, newtime;

	oldtime = host.realtime;

	// main window message loop
	while (1)
	{
		// if at a full screen console, don't update unless needed
		if (Minimized || (dedicated && dedicated->value) )
		{
			Sleep (1);
		}

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
			time = newtime - oldtime;

		} while (time < 0.001);

		Host_Frame (time); // engine frame
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
	SV_Shutdown ("Server shutdown\n", false);
	CL_Shutdown ();
	Host_FreePlatform();
}