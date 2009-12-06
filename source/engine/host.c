//=======================================================================
//			Copyright XashXT Group 2007 ©
//			host.c - dedicated and shared host
//=======================================================================

#include "common.h"
#include "input.h"

#define MAX_RENDERS		8	// max libraries to keep tracking
#define MAX_SYSEVENTS	1024	// system events

physic_exp_t	*pe;
render_exp_t	*re;
vsound_exp_t	*se;
host_parm_t	host;	// host parms
stdlib_api_t	com, newcom;

char		*buildstring = __TIME__ " " __DATE__;
string		video_dlls[MAX_RENDERS];
string		audio_dlls[MAX_RENDERS];
string		cphys_dlls[MAX_RENDERS];
int		num_video_dlls;
int		num_audio_dlls;
int		num_cphys_dlls;

dll_info_t render_dll = { "", NULL, "CreateAPI", NULL, NULL, 0, sizeof(render_exp_t), sizeof(stdlib_api_t) };
dll_info_t vsound_dll = { "", NULL, "CreateAPI", NULL, NULL, 0, sizeof(vsound_exp_t), sizeof(stdlib_api_t) };
dll_info_t physic_dll = { "", NULL, "CreateAPI", NULL, NULL, 0, sizeof(physic_exp_t), sizeof(stdlib_api_t) };

cvar_t	*timescale;
cvar_t	*host_serverstate;
cvar_t	*host_cheats;
cvar_t	*host_maxfps;
cvar_t	*host_minfps;
cvar_t	*host_framerate;
cvar_t	*host_registered;
cvar_t	*host_video;
cvar_t	*host_audio;
cvar_t	*host_cphys;

// these cvars will be duplicated on each client across network
int Host_ServerState( void ) { return Cvar_VariableInteger( "host_serverstate" ); }

int Host_CompareFileTime( long ft1, long ft2 )
{
	if( ft1 < ft2 )
	{
		return -1;
	}
	else if( ft1 > ft2 )
	{
		return 1;
	}
	return 0;
}

void Host_ShutdownServer( void )
{
	if( !SV_Active()) return;
	com.strncpy( host.finalmsg, "Server was killed\n", MAX_STRING );
	SV_Shutdown( false );
}

/*
================
Host_EndGame
================
*/
void Host_EndGame( const char *message, ... )
{
	va_list		argptr;
	static char	string[MAX_MSGLEN];
	
	va_start( argptr, message );
	vsprintf( string, message, argptr );
	va_end( argptr );

	MsgDev( D_INFO, "Host_EndGame: %s\n", string );
	
	if( SV_Active())
	{
		com.strncpy( host.finalmsg, "Host_EndGame\n", MAX_STRING );
		SV_Shutdown( false );
	}
	
	if( host.type == HOST_DEDICATED )
		Sys_Break( "Host_EndGame: %s\n", string ); // dedicated servers exit
	
	if( CL_NextDemo( ));
	else CL_Disconnect();

	Host_AbortCurrentFrame ();
}

static void Host_DrawDebug( cmdraw_t callback )
{
	if( pe ) pe->DrawCollision( callback );
}

void Host_FreePhysic( void )
{
	if( physic_dll.link )
	{
		pe->Shutdown();
		Mem_Set( &pe, 0, sizeof( pe ));
	}
	Sys_FreeLibrary( &physic_dll );
}

bool Host_InitPhysic( void )
{
	static physic_imp_t	pi;
	launch_t		CreatePhysic;  
	bool		result = false;
	
	// phys callback
	pi.api_size = sizeof( physic_imp_t );

	Sys_LoadLibrary( host_cphys->string, &physic_dll );

	if( physic_dll.link )
	{
		CreatePhysic = (void *)physic_dll.main;
		pe = CreatePhysic( &newcom, &pi );

		if( pe->Init( )) result = true;
	} 

	// video system not started, shutdown refresh subsystem
	if( !result ) Host_FreePhysic();

	return result;
}

void Host_FreeRender( void )
{
	if( render_dll.link )
	{
		SCR_Shutdown ();
		re->Shutdown( true );
		Mem_Set( &re, 0, sizeof( re ));
	}
	Sys_FreeLibrary( &render_dll );
}

bool Host_InitRender( void )
{
	static render_imp_t	ri;
	launch_t		CreateRender;
	bool		result = false;
	
	ri.api_size = sizeof( render_imp_t );

          // studio callbacks
	ri.UpdateScreen = SCR_UpdateScreen;
	ri.StudioEvent = CL_StudioEvent;
	ri.StudioFxTransform = CL_StudioFxTransform;
	ri.ShowCollision = Host_DrawDebug;
	ri.GetAttachment = CL_GetAttachment;
	ri.SetAttachment = CL_SetAttachment;
	ri.GetClientEdict = CL_GetEdictByIndex;
	ri.GetPrevFrame = CL_GetPrevFrame;
	ri.GetMouthOpen = CL_GetMouthOpen;
	ri.GetLocalPlayer = CL_GetLocalPlayer;
	ri.GetMaxClients = CL_GetMaxClients;
	ri.GetLerpFrac = CL_GetLerpFrac;
	ri.RoQ_ReadChunk = CIN_ReadChunk;
	ri.RoQ_ReadNextFrame = CIN_ReadNextFrame;
	ri.WndProc = IN_WndProc;          

	Sys_LoadLibrary( host_video->string, &render_dll );

	if( render_dll.link )
	{
		CreateRender = (void *)render_dll.main;
		re = CreateRender( &newcom, &ri );

		if( re->Init( true )) result = true;
	} 

	// video system not started, shutdown refresh subsystem
	if( !result ) Host_FreeRender();

	return result;
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

bool Host_InitSound( void )
{
	static vsound_imp_t	si;
	launch_t		CreateSound;  
	bool		result = false;

	// phys callback
	si.api_size = sizeof( vsound_imp_t );
	si.GetSoundSpatialization = CL_GetEntitySoundSpatialization;
	si.PointContents = CL_PointContents;
	si.GetClientEdict = CL_GetEdictByIndex;
	si.AddLoopingSounds = CL_AddLoopingSounds;
	si.GetServerTime = CL_GetServerTime;

	Sys_LoadLibrary( host_audio->string, &vsound_dll );

	if( vsound_dll.link )
	{
		CreateSound = (void *)vsound_dll.main;
		se = CreateSound( &newcom, &si );
	
		if( se->Init( host.hWnd )) result = true;
	}

	// audio system not started, shutdown sound subsystem
	if( !result ) Host_FreeSound();

	return result;
}

void Host_CheckRestart( void )
{
	int	num_changes;

	if( host_cphys->modified )
	{
		// host.state = HOST_RESTART;
		S_StopAllSounds();		// don't let them loop during the restart

		SV_ForceMod();
		CL_ForceVid();
	}
	else return;

	num_changes = 0;

	// restart or change renderer
	while( host_cphys->modified )
	{
		host_cphys->modified = false;

		Host_FreePhysic();			// release physic.dll
		if( !Host_InitPhysic( ))		// load it again
		{
			if( num_changes > num_cphys_dlls )
			{
				MsgDev( D_ERROR, "couldn't initialize physic system\n" );
				return;
			}
			if( !com.strcmp( cphys_dlls[num_changes], host_cphys->string ))
				num_changes++; // already trying - failed
			Cvar_FullSet( "host_cphys", cphys_dlls[num_changes], CVAR_SYSTEMINFO );
			num_changes++;
		}
	}
}

void Host_CheckChanges( void )
{
	int	num_changes;

	if( host_video->modified || host_audio->modified )
	{
		S_StopAllSounds();	// don't let them loop during the restart

		if( host_video->modified ) CL_ForceVid();
		if( host_audio->modified ) CL_ForceSnd();
	}
	else return;

	num_changes = 0;

	// restart or change renderer
	while( host_video->modified )
	{
		host_video->modified = false;

		Host_FreeRender();			// release render.dll
		if( !Host_InitRender( ))		// load it again
		{
			if( num_changes > num_video_dlls )
			{
				Sys_NewInstance( va("#%s", GI->gamefolder ), "fallback to dedicated mode\n" );
				return;
			}
			if( !com.strcmp( video_dlls[num_changes], host_video->string ))
				num_changes++; // already trying - failed
			Cvar_FullSet( "host_video", video_dlls[num_changes], CVAR_SYSTEMINFO );
			num_changes++;
		}
		else SCR_Init ();
	}

	num_changes = 0;

	// restart or change sound engine
	while( host_audio->modified )
	{
		host_audio->modified = false;

		Host_FreeSound();			// release sound.dll
		if( !Host_InitSound( ))		// load it again
		{
			if( num_changes > num_audio_dlls )
			{
				MsgDev( D_ERROR, "couldn't initialize sound system\n" );
				return;
			}
			if( !com.strcmp( audio_dlls[num_changes], host_audio->string ))
				num_changes++; // already trying - failed
			Cvar_FullSet( "host_audio", audio_dlls[num_changes], CVAR_SYSTEMINFO );
			num_changes++;
		}
	}
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
	host_video->modified = true;
}

/*
=================
Host_SndRestart_f

Restart the audio subsystem
=================
*/
void Host_SndRestart_f( void )
{
	host_audio->modified = true;
}

/*
=================
Host_PhyRestart_f

Restart the physic subsystem
=================
*/
void Host_PhysRestart_f( void )
{
	host_cphys->modified = true;
}

void Host_ChangeGame_f( void )
{
	int	i;

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: game <directory>\n" );
		return;
	}

	// validate gamedir
	for( i = 0; i < SI->numgames; i++ )
	{
		if( !com.stricmp( SI->games[i]->gamefolder, Cmd_Argv( 1 )))
			break;
	}

	if( i == SI->numgames ) Msg( "%s not exist\n", Cmd_Argv( 1 ));
	else if( !com.stricmp( GI->gamefolder, Cmd_Argv( 1 )))
		Msg( "%s already active\n", Cmd_Argv( 1 ));	
	else Sys_NewInstance( Cmd_Argv( 1 ), "Host_ChangeGame\n" );
}

void Host_Minimize_f( void )
{
	if( host.hWnd ) ShowWindow( host.hWnd, SW_MINIMIZE );
}

/*
=================
Host_InitEvents
=================
*/
void Host_InitEvents( void )
{
	Mem_Set( host.events, 0, sizeof( host.events ));
	host.events_head = 0;
	host.events_tail = 0;
}

/*
=================
Host_PushEvent
=================
*/
void Host_PushEvent( sys_event_t *event )
{
	sys_event_t	*ev;
	static bool	overflow = false;

	ev = &host.events[host.events_head & (MAX_SYSEVENTS-1)];

	if( host.events_head - host.events_tail >= MAX_SYSEVENTS )
	{
		if( !overflow )
		{
			MsgDev( D_WARN, "Host_PushEvent overflow\n" );
			overflow = true;
		}
		if( ev->data ) Mem_Free( ev->data );
		host.events_tail++;
	}
	else overflow = false;

	*ev = *event;
	host.events_head++;
}

/*
=================
Host_GetEvent
=================
*/
sys_event_t Host_GetEvent( void )
{
	if( host.events_head > host.events_tail )
	{
		host.events_tail++;
		return host.events[(host.events_tail - 1) & (MAX_SYSEVENTS-1)];
	}
	return Sys_GetEvent();
}

/*
=================
Host_EventLoop

Returns last event time
=================
*/
int Host_EventLoop( void )
{
	sys_event_t	ev;

	while( 1 )
	{
		ev = Sys_GetEvent();
		switch( ev.type )
		{
		case SE_NONE:
			return ev.time;
		case SE_KEY:
			Key_Event( ev.value[0], ev.value[1], ev.time );
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
	return 0;	// never reached
}

/*
================
Host_Milliseconds

Can be used for profiling, but will be journaled accurately
================
*/
int Host_Milliseconds( void )
{
	sys_event_t	ev;

	// get events and push them until we get a null event with the current time
	do {
		ev = Sys_GetEvent();
		if( ev.type != SE_NONE )
			Host_PushEvent( &ev );
	} while( ev.type != SE_NONE );
	
	return ev.time;
}

/*
================
Host_ModifyTime
================
*/
int Host_ModifyTime( int msec )
{
	int	clamp_time;

	// modify time for debugging values
	if( host_framerate->value ) msec = host_framerate->value * 1000;
	else if( timescale->value ) msec *= timescale->value;
	if( msec < 1 && timescale->value ) msec = 1;

	if( host.type == HOST_DEDICATED )
	{
		// dedicated servers don't want to clamp for a much longer
		// period, because it would mess up all the client's views of time.
		if( msec > 500 ) MsgDev( D_WARN, "Host_ModifyTime: %i msec frame time\n", msec );
		clamp_time = 5000;
	}
	else if( SV_Active( ))
	{
		// for local single player gaming
		// we may want to clamp the time to prevent players from
		// flying off edges when something hitches.
		clamp_time = 200;
	}
	else
	{
		// clients of remote servers do not want to clamp time, because
		// it would skew their view of the server's time temporarily
		clamp_time = 5000;
	}

	if( msec > clamp_time ) msec = clamp_time;
	return msec;
}

/*
=================
Host_Frame
=================
*/
void Host_Frame( void )
{
	int		time, min_time;
	static int	last_time;

	if( setjmp( host.abortframe ))
		return;

	rand(); // keep the random time dependent

	// we may want to spin here if things are going too fast
	if( host.type != HOST_DEDICATED && host_maxfps->integer > 0 )
		min_time = 1000 / host_maxfps->integer;
	else min_time = 1;

	do {
		host.frametime = Host_EventLoop();
		if( last_time > host.frametime )
			last_time = host.frametime;
		time = host.frametime - last_time;
	} while( time < min_time );
	Cbuf_Execute();

	last_time = host.frametime;
	time = Host_ModifyTime( time );

	SV_Frame ( time ); // server frame
	CL_Frame ( time ); // client frame

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
		if(( com.strlen( txt ) + com.strlen( host.rd.buffer )) > ( host.rd.buffersize - 1 ))
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
static void Host_Crash_f( void )
{
	*(int *)0 = 0xffffffff;
}

void Host_InitCommon( const int argc, const char **argv )
{
	char		dev_level[4];
	dll_info_t	check_vid, check_snd, check_cms;
	search_t		*dlls;
	int		i;

	newcom = com;

	// overload some funcs
	newcom.error = Host_Error;

	// check developer mode
	if( FS_GetParmFromCmdLine( "-dev", dev_level ))
		host.developer = com.atoi( dev_level );

	Host_InitEvents();

	FS_LoadGameInfo( NULL );
	Image_Init( GI->texmode, -1 );

	host.mempool = Mem_AllocPool( "Zone Engine" );

	IN_Init();

	// initialize audio\video multi-dlls system
	num_video_dlls = num_audio_dlls = 0;
	host_video = Cvar_Get( "host_video", "vid_gl.dll", CVAR_SYSTEMINFO, "name of video rendering library" );
	host_audio = Cvar_Get( "host_audio", "snd_al.dll", CVAR_SYSTEMINFO, "name of sound rendering library" );
	host_cphys = Cvar_Get( "host_cphys", "cms_qf.dll", CVAR_SYSTEMINFO, "name of physic colision library" );

	// make sure what global copy has no changed with any dll checking
	Mem_Copy( &check_vid, &render_dll, sizeof( dll_info_t ));
	Mem_Copy( &check_snd, &vsound_dll, sizeof( dll_info_t ));
	Mem_Copy( &check_cms, &physic_dll, sizeof( dll_info_t ));

	// checking dlls don't invoke crash!
	check_vid.crash = false;
	check_snd.crash = false;
	check_cms.crash = false;

	dlls = FS_Search( "*.dll", true );

	// couldn't find any dlls, render is missing (but i'm don't know how laucnher find engine :)
	// probably this should never happen
	if( !dlls ) Sys_NewInstance( "©", "" );

	for( i = 0; i < dlls->numfilenames; i++ )
	{
		if(!com.strnicmp( "vid_", dlls->filenames[i], 4 ))
		{
			// make sure what found library is valid
			if( Sys_LoadLibrary( dlls->filenames[i], &check_vid ))
			{
				MsgDev( D_NOTE, "VideoLibrary[%i]: %s\n", num_video_dlls, dlls->filenames[i] );
				com.strncpy( video_dlls[num_video_dlls], dlls->filenames[i], MAX_STRING );
				Sys_FreeLibrary( &check_vid );
				num_video_dlls++;
			}
		}
		else if(!com.strnicmp( "snd_", dlls->filenames[i], 4 ))
		{
			// make sure what found library is valid
			if( Sys_LoadLibrary( dlls->filenames[i], &check_snd ))
			{
				MsgDev( D_NOTE, "AudioLibrary[%i]: %s\n", num_audio_dlls, dlls->filenames[i] );
				com.strncpy( audio_dlls[num_audio_dlls], dlls->filenames[i], MAX_STRING );
				Sys_FreeLibrary( &check_snd );
				num_audio_dlls++;
			}
		}
		else if(!com.strnicmp( "cms_", dlls->filenames[i], 4 ))
		{
			// make sure what found library is valid
			if( Sys_LoadLibrary( dlls->filenames[i], &check_cms ))
			{
				MsgDev( D_NOTE, "PhysicLibrary[%i]: %s\n", num_cphys_dlls, dlls->filenames[i] );
				com.strncpy( cphys_dlls[num_cphys_dlls], dlls->filenames[i], MAX_STRING );
				Sys_FreeLibrary( &check_cms );
				num_cphys_dlls++;
			}
		}
	}
	Mem_Free( dlls );
}

void Host_FreeCommon( void )
{
	IN_Shutdown();
	Mem_FreePool( &host.mempool );
}

/*
=================
Host_Init
=================
*/
void Host_Init( const int argc, const char **argv )
{
	char	*s;

	host.state = HOST_INIT;	// initialzation started
	host.type = g_Instance();

	Host_InitCommon( argc, argv );
	Key_Init();

	// get user configuration 
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
	host_framerate = Cvar_Get( "host_framerate", "0", 0, "locks frame timing to this value in seconds" );  
	host_serverstate = Cvar_Get( "host_serverstate", "0", CVAR_SERVERINFO, "displays current server state" );
	host_registered = Cvar_Get( "registered", "1", CVAR_SYSTEMINFO, "indicate shareware version of game" );
	timescale = Cvar_Get( "timescale", "1.0", 0, "slow-mo timescale" );

	s = va("^1Xash %g ^3%s", GI->version, buildstring );
	Cvar_Get( "version", s, CVAR_SERVERINFO|CVAR_INIT, "engine current version" );

	NET_Init();
	Netchan_Init();

	SV_Init();
	CL_Init();

	Host_WriteDefaultConfig ();

	if( host.type == HOST_DEDICATED )
	{
		Cmd_AddCommand( "quit", Sys_Quit, "quit the game" );
		Cmd_AddCommand( "exit", Sys_Quit, "quit the game" );
	}
	else
	{
		Cmd_AddCommand( "minimize", Host_Minimize_f, "minimize main window to tray" );
		Cmd_AddCommand( "vid_restart", Host_VidRestart_f, "restarts video system" );
		Cmd_AddCommand( "snd_restart", Host_SndRestart_f, "restarts audio system" );
	}

	Cmd_AddCommand( "cmap_restart", Host_PhysRestart_f, "restarts physic system" );
	Cmd_AddCommand( "game", Host_ChangeGame_f, "change game" );	// allow to change game from the console
	host.frametime = Host_Milliseconds();
	host.errorframe = 0;
}

/*
=================
Host_Main
=================
*/
void Host_Main( void )
{
	// main window message loop
	while( host.type != HOST_OFFLINE )
	{
		IN_Frame();
		Host_Frame();
	}
}


/*
=================
Host_Shutdown
=================
*/
void Host_Free( void )
{
	if( host.state == HOST_SHUTDOWN )
		return;

	host.state = HOST_SHUTDOWN;	// prepare host to normal shutdown
	com.strncpy( host.finalmsg, "Server shutdown\n", MAX_STRING );

	SV_Shutdown( false );
	CL_Shutdown();
	Host_FreeRender();
	NET_Shutdown();
	Host_FreePhysic();
	Host_FreeCommon();
}

/*
=================
Engine entry point
=================
*/
launch_exp_t DLLEXPORT *CreateAPI( stdlib_api_t *input, void *unused )
{
         	static launch_exp_t Host;

	com = *input;
	Host.api_size = sizeof( launch_exp_t );
	Host.com_size = sizeof( stdlib_api_t );

	Host.Init = Host_Init;
	Host.Main = Host_Main;
	Host.Free = Host_Free;
	Host.CPrint = Host_Print;

	return &Host;
}