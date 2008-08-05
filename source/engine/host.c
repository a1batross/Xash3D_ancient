//=======================================================================
//			Copyright XashXT Group 2007 ©
//			host.c - dedicated and shared host
//=======================================================================

#include "common.h"
#include "input.h"
#include "server.h"
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
dll_info_t vsound_dll = { "vsound.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(vsound_exp_t) };

cvar_t	*timescale;
cvar_t	*host_serverstate;
cvar_t	*host_frametime;
cvar_t	*host_cheats;
cvar_t	*host_maxfps;
cvar_t	*host_maxclients;
cvar_t	*host_registered;

// these cvars will be duplicated on each client across network
int Host_FrameTime( void ) { return (int)(bound( 10, Cvar_VariableValue( "host_frametime" ) * 1000, 100 )); }
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
		memset( &pe, 0, sizeof(pe));
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
	ri.StudioEvent = CL_StudioEvent;
	ri.ShowCollision = pe->DrawCollision;
	ri.WndProc = IN_WndProc;
          
	Sys_LoadLibrary( &render_dll );

	if( render_dll.link )
	{
		CreateRender = (void *)render_dll.main;
		re = CreateRender( &newcom, &ri );

		if(re->Init()) result = true;
	} 

	// video system not started, run dedicated server
	if( !result ) Sys_NewInstance( va("#%s", GI->title), "Host_InitRender: fallback to dedicated mode\n" ); 
}

void Host_FreeRender( void )
{
	if(render_dll.link)
	{
		re->Shutdown();
		memset( &re, 0, sizeof(re));
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
	if(vprogs_dll.link)
	{
		vm->Free();
		memset( &vm, 0, sizeof(vm));
	}
	Sys_FreeLibrary( &vprogs_dll );
}

void Host_InitSound( void )
{
	static vsound_imp_t		si;
	launch_t			CreateSound;  

	// phys callback
	si.api_size = sizeof(vsound_imp_t);
	si.GetSoundSpatialization = CL_GetEntitySoundSpatialization;
	si.PointContents = CL_PMpointcontents;
	si.AddLoopingSounds = CL_AddLoopingSounds;

	Sys_LoadLibrary( &vsound_dll );

	CreateSound = (void *)vsound_dll.main;
	se = CreateSound( &newcom, &si );
	
	se->Init( host.hWnd );
}

void Host_FreeSound( void )
{
	if(vsound_dll.link)
	{
		se->Shutdown();
		memset( &se, 0, sizeof(se));
	}
	Sys_FreeLibrary( &vsound_dll );
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
	cl.force_refdef = true;	// can't use a paused refdef
	S_StopAllSounds();		// don't let them loop during the restart
	cl.video_prepped = false;

	Host_FreeRender();		// release render.dll
	Host_InitRender();		// load it again
}

/*
=================
Host_SndRestart_f

Restart the audio subsystem
=================
*/
void Host_SndRestart_f( void )
{
	cl.force_refdef = true;	// can't use a paused refdef
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

/*
============
VID_Init
============
*/
void VID_Init( void )
{
	scr_width = Cvar_Get("width", "640", 0, "screen width" );
	scr_height = Cvar_Get("height", "480", 0, "screen height" );

	Cmd_AddCommand( "vid_restart", Host_VidRestart_f, "restarts video system" );
	Cmd_AddCommand( "snd_restart", Host_SndRestart_f, "restarts audio system" );
	Cmd_AddCommand( "game", Host_ChangeGame_f, "change game" );

	Host_InitRender();
	Host_InitSound();
}

/*
=================
Host_InitEvents
=================
*/
void Host_InitEvents( void )
{
	memset( host.events, 0, sizeof(host.events));
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

	ev = &host.events[host.events_head & (MAX_EVENTS-1)];

	if( host.events_head - host.events_tail >= MAX_EVENTS )
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
		return host.events[(host.events_tail - 1)&(MAX_EVENTS-1)];
	}
	return Sys_GetEvent();
}

/*
=================
Host_EventLoop

Returns last event time
=================
*/
dword Host_EventLoop( void )
{
	sys_event_t	ev;
	netadr_t		ev_from;
	byte		bufData[MAX_MSGLEN];
	sizebuf_t		buf;

	MSG_Init( &buf, bufData, sizeof( bufData ));

	while( 1 )
	{
		ev = Host_GetEvent();
		switch ( ev.type )
		{
		case SE_NONE:
			// manually send packet events for the loopback channel
			while( NET_GetLoopPacket( NS_CLIENT, &ev_from, &buf ))
			{
				CL_PacketEvent( ev_from, &buf );
			}
			while( NET_GetLoopPacket( NS_SERVER, &ev_from, &buf ))
			{
				SV_PacketEvent( ev_from, &buf );
			}
			return ev.time;
		case SE_KEY:
			Key_Event( ev.value[0], ev.value[1], ev.time );
			break;
		case SE_CHAR:
			CL_CharEvent( ev.value[0] );
			break;
		case SE_MOUSE:
			CL_MouseEvent( ev.value[0], ev.value[1], ev.time );
			break;
		case SE_CONSOLE:
			Cbuf_AddText(va( "%s\n", ev.data ));
			break;
		case SE_PACKET:
			ev_from = *(netadr_t *)ev.data;
			buf.cursize = ev.length - sizeof( ev_from );

			// we must copy the contents of the message out, because
			// the event buffers are only large enough to hold the
			// exact payload, but channel messages need to be large
			// enough to hold fragment reassembly
			if((uint)buf.cursize > buf.maxsize )
			{
				MsgDev( D_WARN, "Host_EventLoop: oversize packet\n");
				continue;
			}
			Mem_Copy( buf.data, (byte *)((netadr_t *)ev.data + 1), buf.cursize );
			if ( svs.initialized ) SV_PacketEvent( ev_from, &buf );
			else CL_PacketEvent( ev_from, &buf );
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
Host_ModifyMsec
================
*/
int Host_ModifyTime( int msec )
{
	int	clamp_time;

	// modify time for debugging values
	if( timescale->value ) msec *= timescale->value;
	if( msec < 1 && timescale->value ) msec = 1;

	if( host.type == HOST_DEDICATED )
	{
		// dedicated servers don't want to clamp for a much longer
		// period, because it would mess up all the client's views of time.
		if( msec > 500 ) MsgDev( D_WARN, "Host_ModifyTimer: %i msec frame time\n", msec );
		clamp_time = 5000;
	}
	else 
	{
		// for local single player gaming
		// we may want to clamp the time to prevent players from
		// flying off edges when something hitches.
		clamp_time = 200;
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
	dword		time, min_time;
	static int	last_time;

	if( setjmp(host.abortframe))
		return;

	rand(); // keep the random time dependent
	// we may want to spin here if things are going too fast
	if( host.type != HOST_DEDICATED && host_maxfps->integer > 0 )
		min_time = 1000 / host_maxfps->integer;
	else min_time = 1;

	do {
		host.frametime[0] = Host_EventLoop();
		if( last_time > host.frametime[0] )
			last_time = host.frametime[0];
		time = host.frametime[0] - last_time;
	} while( time < min_time );
	Cbuf_Execute();

	last_time = host.frametime[0];
	time = Host_ModifyTime( time );

	SV_Frame( time );	// server frame
	CL_Frame( time );	// client frame
	VM_Frame( time );	// vprogs frame

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
		com.error( hosterror1 );
	else Msg( "Host_Error: %s", hosterror1 );

	if( recursive )
	{ 
		Msg("Host_RecursiveError: %s", hosterror2 );
		com.error( hosterror1 );
		return; // don't multiple executes
	}

	recursive = true;
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
	if(FS_GetParmFromCmdLine("-dev", dev_level ))
		host.developer = com.atoi(dev_level);

	Host_InitEvents();

	// TODO: init basedir here
	FS_LoadGameInfo("gameinfo.txt");
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

	Host_InitCommon( argc, argv ); // loading common.dll
	Key_Init();

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
	if( host.developer )
	{
		Cmd_AddCommand ("error", Host_Error_f, "just throw a fatal error to test shutdown procedures" );
		Cmd_AddCommand ("crash", Host_Crash_f, "a way to force a bus error for development reasons");
          }

	host_cheats = Cvar_Get( "host_cheats", "1", CVAR_SYSTEMINFO, "allow cheat variables to enable" );          
	host_maxfps = Cvar_Get( "host_maxfps", "100", CVAR_ARCHIVE, "host fps upper limit" );
	host_frametime = Cvar_Get("host_frametime", "0.1", CVAR_SERVERINFO, "host frametime (only for test!)" );
	host_maxclients = Cvar_Get("host_maxclients", "1", CVAR_SERVERINFO|CVAR_LATCH, "server maxplayers limit" );
	host_serverstate = Cvar_Get("host_serverstate", "0", CVAR_SERVERINFO, "displays current server state" );
	host_registered = Cvar_Get( "registered", "1", CVAR_SYSTEMINFO, "indicate shareware version of game" );
	timescale = Cvar_Get ("timescale", "1", 0, "physics world timescale" );

	s = va("^1Xash %g ^3%s", GI->version, buildstring );
	Cvar_Get( "version", s, CVAR_SERVERINFO|CVAR_INIT, "engine current version" );

	NET_Init();
	Netchan_Init();
	Host_InitPhysic();
	Host_InitVprogs( argc, argv );

	// per level user limit
	host.max_edicts = bound( 8, Cvar_VariableValue("prvm_maxedicts"), MAX_EDICTS - 1 );

	SV_Init();
	CL_Init();

	if( host.type == HOST_DEDICATED ) Cmd_AddCommand ("quit", Sys_Quit, "quit the game" );
	host.frametime[0] = Host_Milliseconds();
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