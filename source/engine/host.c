//=======================================================================
//			Copyright XashXT Group 2007 ©
//			host.c - dedicated and shared host
//=======================================================================

#include "common.h"
#include "input.h"
#include "client.h"

#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ))

physic_exp_t	*pe;
render_exp_t	*re;
vprogs_exp_t	*vm;
vsound_exp_t	*se;
host_parm_t	host;	// host parms
stdlib_api_t	com, newcom;

byte	*zonepool;
char	*buildstring = __TIME__ " " __DATE__;

dll_info_t physic_dll = { "physic.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(physic_exp_t) };
dll_info_t render_dll = { "render.dll", NULL, "CreateAPI", NULL, NULL, false, sizeof(render_exp_t) };
dll_info_t vprogs_dll = { "vprogs.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(vprogs_exp_t) };
dll_info_t vsound_dll = { "vsound.dll", NULL, "CreateAPI", NULL, NULL, false, sizeof(vsound_exp_t) };

cvar_t	*timescale;
cvar_t	*host_serverstate;
cvar_t	*host_cheats;
cvar_t	*host_maxfps;
cvar_t	*host_minfps;
cvar_t	*host_ticrate;
cvar_t	*host_framerate;
cvar_t	*host_maxclients;
cvar_t	*host_registered;

// these cvars will be duplicated on each client across network
int Host_ServerState( void ) { return (int)Cvar_VariableValue( "host_serverstate" ); }
int Host_MaxClients( void ) { return (int)bound( 1, Cvar_VariableValue( "host_maxclients" ), 255 ); }

void Host_InitPhysic( void )
{
	static physic_imp_t		pi;
	launch_t			CreatePhysic;  

	// phys callback
	pi.api_size = sizeof(physic_imp_t);
	pi.Transform = SV_Transform;
	pi.PlaySound = SV_PlaySound;
	pi.ClientMove = SV_PlayerMove;
	pi.GetModelVerts = SV_GetModelVerts;

	Sys_LoadLibrary( &physic_dll );

	CreatePhysic = (void *)physic_dll.main;
	pe = CreatePhysic( &newcom, &pi );
	
	pe->Init();
}

void Host_FreePhysic( void )
{
	if(physic_dll.link)
	{
		pe->Shutdown();
		Mem_Set( &pe, 0, sizeof(pe));
	}
	Sys_FreeLibrary( &physic_dll );
}

void Host_InitRender( void )
{
	static render_imp_t		ri;
	launch_t			CreateRender;
	bool			result = false;
	
	ri.api_size = sizeof(render_imp_t);

          // studio callbacks
	ri.UpdateScreen = SCR_UpdateScreen;
	ri.StudioEvent = CL_StudioEvent;
	ri.AddDecal = CL_AddDecal;
	ri.ShowCollision = pe->DrawCollision;
	ri.GetClientEdict = CL_GetEdictByIndex;
	ri.GetLocalPlayer = CL_GetLocalPlayer;
	ri.GetMaxClients = CL_GetMaxClients;
	ri.Trace = CL_RenderTrace;
	ri.WndProc = IN_WndProc;
          
	Sys_LoadLibrary( &render_dll );

	if( render_dll.link )
	{
		CreateRender = (void *)render_dll.main;
		re = CreateRender( &newcom, &ri );

		if( re->Init( true )) result = true;
	} 

	// video system not started, run dedicated server
	if( !result ) Sys_NewInstance( va("#%s", GI->gamedir ), "Host_InitRender: fallback to dedicated mode\n" ); 
}

void Host_FreeRender( void )
{
	if( render_dll.link )
	{
		re->Shutdown( true );
		Mem_Set( &re, 0, sizeof( re ));
	}
	Sys_FreeLibrary( &render_dll );
}

void Host_InitVprogs( int argc, char **argv )
{
	launch_t		CreateVprogs;  

	Sys_LoadLibrary( &vprogs_dll );

	CreateVprogs = (void *)vprogs_dll.main;
	vm = CreateVprogs( &newcom, NULL ); // second interface not allowed
	
	vm->Init( argc, argv );
}

void Host_FreeVprogs( void )
{
	if( vprogs_dll.link )
	{
		vm->Free();
		Mem_Set( &vm, 0, sizeof(vm));
	}
	Sys_FreeLibrary( &vprogs_dll );
}

void Host_FreeSound( void )
{
	if( vsound_dll.link )
	{
		se->Shutdown();
		Mem_Set( &se, 0, sizeof( se ));
	}
	Sys_FreeLibrary( &vsound_dll );
}

void Host_InitSound( void )
{
	static vsound_imp_t		si;
	launch_t			CreateSound;  
	bool			result = false;

	// phys callback
	si.api_size = sizeof(vsound_imp_t);
	si.GetSoundSpatialization = CL_GetEntitySoundSpatialization;
	si.PointContents = CL_PointContents;
	si.AmbientLevel = CL_AmbientLevel;
	si.AddLoopingSounds = CL_AddLoopingSounds;

	Sys_LoadLibrary( &vsound_dll );

	if( vsound_dll.link )
	{
		CreateSound = (void *)vsound_dll.main;
		se = CreateSound( &newcom, &si );
	
		if( se->Init( host.hWnd )) result = true;
	}

	// audio system not started, shutdown sound subsystem
	if( !result ) Host_FreeSound();
}

/*
================
Host_AbortCurrentFrame

aborts the current host frame and goes on with the next one
================
*/
void Host_AbortCurrentFrame( void )
{
	longjmp( host.abortframe, 1 );
}

/*
==================
Host_SetServerState
==================
*/
void Host_SetServerState( int state )
{
	Cvar_SetValue( "host_serverstate", state );
}

/*
=================
Host_VidRestart_f

Restart the video subsystem
=================
*/
void Host_VidRestart_f( void )
{
	host.state = HOST_RESTART;
	S_StopAllSounds();		// don't let them loop during the restart
	cl.video_prepped = false;

	Host_FreeRender();		// release render.dll
	Host_InitRender();		// load it again

	SCR_RegisterShaders();	// reload 2d-shaders
}

/*
=================
Host_SndRestart_f

Restart the audio subsystem
=================
*/
void Host_SndRestart_f( void )
{
	host.state = HOST_RESTART;
	S_StopAllSounds();		// don't let them loop during the restart
	cl.audio_prepped = false;
	
	Host_FreeSound();		// release vsound.dll
	Host_InitSound();		// load it again
}

void Host_ChangeGame_f( void )
{
	int	i;

	if( Cmd_Argc() != 2 )
	{
		Msg("Usage: game <directory>\n");
		return;
	}

	// validate gamedir
	for( i = 0; i < GI->numgamedirs; i++ )
	{
		if(!com.stricmp(GI->gamedirs[i], Cmd_Argv(1)))
			break;
	}

	if( i == GI->numgamedirs ) Msg( "%s not exist\n", Cmd_Argv(1));
	else if(!com.stricmp(GI->gamedir, Cmd_Argv(1)))
		Msg( "%s already active\n", Cmd_Argv(1));	
	else Sys_NewInstance( Cmd_Argv(1), "Host_ChangeGame\n" );
}

void Host_Minimize_f( void )
{
	if( host.hWnd ) ShowWindow( host.hWnd, SW_MINIMIZE );
}

/*
============
VID_Init
============
*/
void VID_Init( void )
{
	scr_width = Cvar_Get("width", "640", 0, "screen width" );
	scr_height = Cvar_Get("height", "480", 0, "screen height" );

	Cmd_AddCommand( "minimize", Host_Minimize_f, "minimize main window to tray" );
	Cmd_AddCommand( "vid_restart", Host_VidRestart_f, "restarts video system" );
	Cmd_AddCommand( "snd_restart", Host_SndRestart_f, "restarts audio system" );
	Cmd_AddCommand( "game", Host_ChangeGame_f, "change game" );

	Host_InitRender();
	Host_InitSound();
}

/*
=================
Host_EventLoop

Returns last event time
=================
*/
void Host_EventLoop( void )
{
	sys_event_t	ev;

	while( 1 )
	{
		ev = Sys_GetEvent();
		switch( ev.type )
		{
		case SE_NONE:
			// end of events
			return;
		case SE_KEY:
			Key_Event( ev.value[0], ev.value[1] );
			break;
		case SE_CHAR:
			CL_CharEvent( ev.value[0] );
			break;
		case SE_MOUSE:
			CL_MouseEvent( ev.value[0], ev.value[1] );
			break;
		case SE_CONSOLE:
			Cbuf_AddText( va( "%s\n", ev.data ));
			break;
		default:
			Host_Error( "Host_EventLoop: bad event type %i", ev.type );
			break;
		}
		if( ev.data ) Mem_Free( ev.data );
	}
}

/*
===================
Host_FilterTime

Returns false if the time is too short to run a frame
===================
*/
bool Host_FilterTime( double time )
{
	double timecap, timeleft;

	host.realtime += time;

	if( timescale->value < 0.0f )
		Cvar_SetValue( "timescale", 0.0f );
	if( host_minfps->integer < 10 )
		Cvar_SetValue( "host_minfps", 10.0f );
	if( host_maxfps->integer < host_minfps->integer )
		Cvar_SetValue( "host_maxfps", host_minfps->integer );

	// check if framerate is too high
	// default to sys_ticrate (server framerate - presumably low) unless we have a good reason to run faster
	timecap = host_ticrate->value;
	if( host.state == HOST_FRAME )
		timecap = 1.0 / host_maxfps->integer;

	timeleft = host.oldrealtime + timecap - host.realtime;
	if( timeleft > 0 )
	{
		// don't totally hog the CPU
		if( timeleft >= 0.02 )
			Sys_Sleep( 1 );
		return false;
	}

	// LordHavoc: copy into host_realframetime as well
	host.realframetime = host.frametime = host.realtime - host.oldrealtime;
	host.oldrealtime = host.realtime;

	if( host_framerate->value > 0 )
	{
		host.frametime = host_framerate->value;
	}
	else
	{
		// don't allow really short frames
		if( host.frametime > (1.0 / host_minfps->value ))
			host.frametime = (1.0 / host_minfps->value);
	}

	host.frametime = bound( 0, host.frametime * timescale->value, 0.1f );

	return true;
}

/*
=================
Host_Frame
=================
*/
void Host_Frame( double time )
{
	if( setjmp( host.abortframe ))
		return;

	rand(); // keep the random time dependent

	// decide the simulation time
	if( !Host_FilterTime( time ))
		return;

	Host_EventLoop ();	// process all system events
	Cbuf_Execute ();	// execure commands

	SV_Frame (); // server frame
	CL_Frame (); // client frame
	VM_Frame (); // vprogs frame

	host.framecount++;
}
/*
================
Host_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void Host_Print( const char *txt )
{
	if( host.rd.target )
	{
		if((com.strlen (txt) + com.strlen(host.rd.buffer)) > (host.rd.buffersize - 1))
		{
			if( host.rd.flush )
			{
				host.rd.flush( host.rd.address, host.rd.target, host.rd.buffer );
				*host.rd.buffer = 0;
			}
		}
		com.strcat( host.rd.buffer, txt );
		return;
	}
	Con_Print( txt ); // echo to client console
}

/*
=================
Host_Error
=================
*/
void Host_Error( const char *error, ... )
{
	static char	hosterror1[MAX_MSGLEN];
	static char	hosterror2[MAX_MSGLEN];
	static bool	recursive = false;
	va_list		argptr;

	va_start( argptr, error );
	com.vsprintf( hosterror1, error, argptr );
	va_end( argptr );

	if( host.framecount < 3 || host.state == HOST_SHUTDOWN )
	{
		Msg( "Host_InitError: " );
		com.error( hosterror1 );
	}
	else if( host.framecount == host.errorframe )
	{
		com.error( "Host_MultiError: %s", hosterror2 );
		return;
	}
	else Msg( "Host_Error: %s", hosterror1 );

	if( recursive )
	{ 
		Msg( "Host_RecursiveError: %s", hosterror2 );
		com.error( va( "%s", hosterror1 ));
		return; // don't multiple executes
	}

	recursive = true;
	com.strncpy( hosterror2, hosterror1, MAX_MSGLEN );
	host.errorframe = host.framecount; // to avoid multply calls per frame
	com.sprintf( host.finalmsg, "Server crashed: %s\n", hosterror1 );

	SV_Shutdown( false );
	CL_Drop(); // drop clients

	recursive = false;
	Host_AbortCurrentFrame();
	host.state = HOST_ERROR;
}

void Host_Error_f( void )
{
	if( Cmd_Argc() == 1 ) Sys_Break( "\n" );
	else Host_Error( "%s\n", Cmd_Argv( 1 ));
}

/*
=================
Host_Crash_f
=================
*/
static void Host_Crash_f (void)
{
	*(int *)0 = 0xffffffff;
}

void Host_InitCommon( int argc, char **argv )
{
	char	dev_level[4];

	newcom = com;

	// overload some funcs
	newcom.error = Host_Error;

	// check developer mode
	if( FS_GetParmFromCmdLine( "-dev", dev_level ))
		host.developer = com.atoi( dev_level );

	// TODO: init basedir here
	FS_LoadGameInfo( "gameinfo.txt" );
	zonepool = Mem_AllocPool("Zone Engine");

	IN_Init();
}

void Host_FreeCommon( void )
{
	IN_Shutdown();
	Mem_FreePool( &zonepool );
}

/*
=================
Host_Init
=================
*/
void Host_Init( int argc, char **argv)
{
	char	*s;

	host.state = HOST_INIT;	// initialzation started
	host.type = g_Instance;

	Host_InitCommon( argc, argv );
	Key_Init();

	// get default configuration
	Cbuf_AddText( "exec keys.rc\n" );
	Cbuf_AddText( "exec vars.rc\n" );
	Cbuf_Execute();

	// init commands and vars
	if( host.developer )
	{
		Cmd_AddCommand ("error", Host_Error_f, "just throw a fatal error to test shutdown procedures" );
		Cmd_AddCommand ("crash", Host_Crash_f, "a way to force a bus error for development reasons");
          }

	host_cheats = Cvar_Get( "sv_cheats", "1", CVAR_SYSTEMINFO, "allow cheat variables to enable" );
	host_minfps = Cvar_Get( "host_minfps", "10", CVAR_ARCHIVE, "host fps lower limit" );
	host_maxfps = Cvar_Get( "host_maxfps", "100", CVAR_ARCHIVE, "host fps upper limit" );
	host_ticrate = Cvar_Get( "sys_ticrate", "0.0138889", CVAR_SYSTEMINFO, "how long a server frame is in seconds" );
	host_framerate = Cvar_Get( "host_framerate", "0", 0, "locks frame timing to this value in seconds" );  
	host_maxclients = Cvar_Get("host_maxclients", "1", CVAR_SERVERINFO|CVAR_LATCH, "server maxplayers limit" );
	host_serverstate = Cvar_Get("host_serverstate", "0", CVAR_SERVERINFO, "displays current server state" );
	host_registered = Cvar_Get( "registered", "1", CVAR_SYSTEMINFO, "indicate shareware version of game" );
	timescale = Cvar_Get( "timescale", "1.0", 0, "slow-mo timescale" );

	s = va("^1Xash %g ^3%s", GI->version, buildstring );
	Cvar_Get( "version", s, CVAR_SERVERINFO|CVAR_INIT, "engine current version" );

	NET_Init();
	Netchan_Init();
	Host_InitPhysic();
	Host_InitVprogs( argc, argv );

	// per level user limit
	host.max_edicts = bound( 8, Cvar_VariableValue( "host_maxedicts" ), MAX_EDICTS - 1 );

	SV_Init();
	CL_Init();

	if( host.type == HOST_DEDICATED ) Cmd_AddCommand ("quit", Sys_Quit, "quit the game" );
	host.errorframe = 0;
}

/*
=================
Host_Main
=================
*/
void Host_Main( void )
{
	static double oldtime, newtime;

	oldtime = Sys_DoubleTime();

	// main window message loop
	while( host.type != HOST_OFFLINE )
	{
		IN_Frame();

		newtime = Sys_DoubleTime ();
		Host_Frame( newtime - oldtime );
		oldtime = newtime;
	}
}


/*
=================
Host_Shutdown
=================
*/
void Host_Free( void )
{
	host.state = HOST_SHUTDOWN;	// prepare host to normal shutdown
	com.strncpy( host.finalmsg, "Server shutdown\n", MAX_STRING );

	SV_Shutdown( false );
	CL_Shutdown();
	Host_FreeRender();
	NET_Shutdown();
	Host_FreeVprogs();
	Host_FreePhysic();
	Host_FreeCommon();
}