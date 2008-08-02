//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_utils.c - server vm utils
//=======================================================================

#include "common.h"
#include "server.h"

netadr_t	master_adr[MAX_MASTERS];	// address of group servers

cvar_t	*sv_paused;
cvar_t	*sv_fps;
cvar_t	*sv_enforcetime;
cvar_t	*sv_fatpvs;
cvar_t	*timeout;				// seconds without any message
cvar_t	*zombietime;			// seconds to sink messages after disconnect

cvar_t	*rcon_password;			// password for remote server commands

cvar_t	*allow_download;
cvar_t	*sv_airaccelerate;
cvar_t	*sv_maxvelocity;
cvar_t	*sv_gravity;
cvar_t	*sv_stepheight;
cvar_t	*sv_noreload;			// don't reload level state when reentering
cvar_t	*sv_playersonly;
cvar_t	*sv_rollangle;
cvar_t	*sv_rollspeed;
cvar_t	*sv_maxspeed;
cvar_t	*sv_accelerate;
cvar_t	*sv_friction;
cvar_t	*hostname;
cvar_t	*public_server; // should heartbeats be sent

cvar_t	*sv_reconnect_limit;// minimum seconds between connect messages

void Master_Shutdown (void);

//============================================================================

/*
===================
SV_CalcPings

Updates the cl->ping variables
===================
*/
void SV_CalcPings( void )
{
	int		i, j;
	sv_client_t	*cl;
	int		total, count;

	// clamp fps counter
	for( i = 0; i < Host_MaxClients(); i++ )
	{
		cl = &svs.clients[i];
		if( cl->state != cs_spawned ) continue;

		total = count = 0;
		for( j = 0; j < UPDATE_BACKUP; j++ )
		{
			if( cl->frames[j].latency > 0 )
			{
				count++;
				total += cl->frames[j].latency;
			}
		}
		if( !count ) cl->ping = 0;
		else cl->ping = total / count;

		// let the game dll know about the ping
		cl->edict->priv.sv->client->ping = cl->ping;
	}
}

/*
=================
SV_PacketEvent
=================
*/
void SV_PacketEvent( netadr_t from, sizebuf_t *msg )
{
	int		i;
	sv_client_t	*cl;
	int		qport;

	if( !svs.initialized ) return;

	SV_VM_Begin();

	// check for connectionless packet (0xffffffff) first
	if( msg->cursize >= 4 && *(int *)msg->data == -1 )
	{
		SV_ConnectionlessPacket( from, msg );
		return;
	}

	// read the qport out of the message so we can fix up
	// stupid address translating routers
	MSG_BeginReading( msg );
	MSG_ReadLong( msg );	// sequence number
	MSG_ReadLong( msg );	// sequence number
	qport = (int)MSG_ReadShort( msg ) & 0xffff;

	// check for packets from connected clients
	for( i = 0, cl = svs.clients; i < Host_MaxClients(); i++, cl++ )
	{
		if( cl->state == cs_free ) continue;
		if( !NET_CompareBaseAdr( from, cl->netchan.remote_address )) continue;
		if( cl->netchan.qport != qport ) continue;
		if( cl->netchan.remote_address.port != from.port )
		{
			MsgDev( D_INFO, "SV_ReadPackets: fixing up a translated port\n");
			cl->netchan.remote_address.port = from.port;
		}
		if( Netchan_Process( &cl->netchan, msg ))
		{	
			// this is a valid, sequenced packet, so process it
			if( cl->state != cs_zombie )
			{
				cl->lastmessage = svs.realtime; // don't timeout
				SV_ExecuteClientMessage( cl, msg );
			}
		}
		break;
	}
	if( i != Host_MaxClients()) return;
	SV_VM_End();
}

/*
==================
SV_CheckTimeouts

If a packet has not been received from a client for timeout->value
seconds, drop the conneciton.  Server frames are used instead of
realtime to avoid dropping the local client while debugging.

When a client is normally dropped, the sv_client_t goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
void SV_CheckTimeouts( void )
{
	int		i;
	sv_client_t	*cl;
	float		droppoint;
	float		zombiepoint;

	droppoint = svs.realtime - 1000 * timeout->value;
	zombiepoint = svs.realtime - 1000 * zombietime->value;

	if( host_frametime->modified )
		Cvar_SetValue( "host_frametime", bound( 0.01f, host_frametime->value, 0.1f ));

	for( i = 0, cl = svs.clients; i < Host_MaxClients(); i++, cl++ )
	{
		// message times may be wrong across a changelevel
		if( cl->lastmessage > svs.realtime ) cl->lastmessage = svs.realtime;
		if( cl->state == cs_zombie && cl->lastmessage < zombiepoint )
		{
			cl->state = cs_free; // can now be reused
			continue;
		}
		if(( cl->state == cs_connected || cl->state == cs_spawned) && cl->lastmessage < droppoint )
		{
			SV_BroadcastPrintf( PRINT_CONSOLE, "%s timed out\n", cl->name );
			SV_DropClient( cl ); 
			cl->state = cs_free; // don't bother with zombie state
		}
	}
}


/*
=================
SV_RunGameFrame
=================
*/
void SV_RunGameFrame (void)
{
	if( sv_paused->integer && Host_MaxClients() == 1 )
		return;

	if( sv.state != ss_active ) return;

	// we always need to bump framenum, even if we
	// don't run the world, otherwise the delta
	// compression can get confused when a client
	// has the "current" frame
	sv.framenum++;
	sv.frametime = Host_FrameTime() * 0.001f;
	//sv.time = sv.framenum * sv.frametime;

	SV_Physics();

	// never get more than one tic behind
	if( sv.time * 1000 < svs.realtime )
	{
		Msg ("sv highclamp\n");
		svs.realtime = sv.time * 1000;
	}
}

/*
==================
SV_Frame

==================
*/
void SV_Frame( dword time )
{
	// if server is not active, do nothing
	if( !svs.initialized ) return;
	svs.realtime += time;
	svs.timeleft += sv_paused->integer ? 0 : time;

	// keep the random time dependent
	rand ();

	// check timeouts
	SV_CheckTimeouts ();

	// move autonomous things around if enough time has passed
	if( svs.realtime < (sv.time * 1000))
	{
		// never let the time get too far off
		if((sv.time * 1000) - svs.realtime > Host_FrameTime())
		{
			Msg ("sv lowclamp\n");
			svs.realtime = (sv.time * 1000 ) - Host_FrameTime();
		}

		NET_Sleep((sv.time*1000) - svs.realtime);
		return;
	}

	// setup the VM frame
	SV_VM_Begin();

	// update ping based on the last known frame from all clients
	SV_CalcPings ();

	// let everything in the world think and move
	SV_RunGameFrame ();

	// send messages back to the clients that had packets read this frame
	SV_SendClientMessages ();

	// send a heartbeat to the master if needed
	Master_Heartbeat ();

	// end the server VM frame
	SV_VM_End();
}

//============================================================================

/*
================
Master_Heartbeat

Send a message to the master every few minutes to
let it know we are alive, and log information
================
*/
#define	HEARTBEAT_SECONDS	300.0f
void Master_Heartbeat (void)
{
	char		*string;
	int			i;

	if( host.type != HOST_DEDICATED )
		return;		// only dedicated servers send heartbeats

	// pgm post3.19 change, cvar pointer not validated before dereferencing
	if (!public_server || !public_server->value)
		return;	// a private dedicated game

	// check for time wraparound
	if (svs.last_heartbeat > svs.realtime) svs.last_heartbeat = svs.realtime;

	if( svs.realtime - svs.last_heartbeat < HEARTBEAT_SECONDS * 1000 )
		return;		// not time to send yet

	svs.last_heartbeat = svs.realtime;

	// send the same string that we would give for a status OOB command
	string = SV_StatusString();

	// send to group master
	for (i = 0; i < MAX_MASTERS; i++)
	{
		if (master_adr[i].port)
		{
			Msg ("Sending heartbeat to %s\n", NET_AdrToString (master_adr[i]));
			Netchan_OutOfBandPrint (NS_SERVER, master_adr[i], "heartbeat\n%s", string);
		}
	}
}

/*
=================
Master_Shutdown

Informs all masters that this server is going down
=================
*/
void Master_Shutdown (void)
{
	int			i;

	if( host.type != HOST_DEDICATED )
		return;		// only dedicated servers send heartbeats

	// pgm post3.19 change, cvar pointer not validated before dereferencing
	if (!public_server || !public_server->value)
		return;		// a private dedicated game

	// send to group master
	for(i = 0; i < MAX_MASTERS; i++)
	{
		if (master_adr[i].port)
		{
			if( i ) Msg ("Sending heartbeat to %s\n", NET_AdrToString (master_adr[i]));
			Netchan_OutOfBandPrint( NS_SERVER, master_adr[i], "shutdown" );
		}
	}
}

//============================================================================

/*
===============
SV_Init

Only called at xash.exe startup, not for each game
===============
*/
void SV_Init (void)
{
	SV_InitOperatorCommands();

	sv_fatpvs = Cvar_Get( "sv_fatpvs", "1",  CVAR_ARCHIVE, "using fat pvs intstead of normal client pvs" );

	rcon_password = Cvar_Get( "rcon_password", "", 0, "remote connect password" );
	Cvar_Get ("skill", "1", 0, "game skill level" );
	Cvar_Get ("deathmatch", "0", CVAR_LATCH, "displays deathmatch state" );
	Cvar_Get ("coop", "0", CVAR_LATCH, "displays cooperative state" );
	Cvar_Get ("dmflags", "0", CVAR_SERVERINFO, "setup deathmatch flags" );
	Cvar_Get ("fraglimit", "0", CVAR_SERVERINFO, "multiplayer fraglimit" );
	Cvar_Get ("timelimit", "0", CVAR_SERVERINFO, "multiplayer timelimit" );
	Cvar_Get ("protocol", va("%i", PROTOCOL_VERSION), CVAR_SERVERINFO|CVAR_INIT, "displays server protocol version" );

	sv_fps = Cvar_Get( "sv_fps", "60", CVAR_ARCHIVE, "running physics engine at" );
	sv_stepheight = Cvar_Get( "sv_stepheight", "18", CVAR_ARCHIVE|CVAR_LATCH, "how high you can step up" );
	sv_playersonly = Cvar_Get( "playersonly", "0", 0, "freezes time, except for players" );
	hostname = Cvar_Get ("sv_hostname", "unnamed", CVAR_SERVERINFO | CVAR_ARCHIVE, "host name" );
	timeout = Cvar_Get ("timeout", "125", 0, "connection timeout" );
	zombietime = Cvar_Get ("zombietime", "2", 0, "timeout for clients-zombie (who died but not respawned)" );
	sv_paused = Cvar_Get ("paused", "0", 0, "server pause" );
	sv_enforcetime = Cvar_Get ("sv_enforcetime", "0", 0, "client enforce time" );
	allow_download = Cvar_Get ("allow_download", "1", CVAR_ARCHIVE, "allow download resources" );
	sv_noreload = Cvar_Get("sv_noreload", "0", 0, "ignore savepoints for singleplayer" );
	sv_rollangle = Cvar_Get("sv_rollangle", "2", 0, "how much to tilt the view when strafing" );
	sv_rollspeed = Cvar_Get("sv_rollspeed", "200", 0, "how much strafing is necessary to tilt the view" );
	sv_airaccelerate = Cvar_Get("sv_airaccelerate", "0", CVAR_LATCH, "player accellerate in air" );
	sv_maxvelocity = Cvar_Get("sv_maxvelocity", "2000", 0, "max world velocity" );
          sv_gravity = Cvar_Get("sv_gravity", "800", 0, "world gravity" );
	sv_maxspeed = Cvar_Get("sv_maxspeed", "320", 0, "maximum speed a player can accelerate to when on ground (can be exceeded by tricks)");
	sv_accelerate = Cvar_Get( "sv_accelerate", "10", 0, "rate at which a player accelerates to sv_maxspeed" );
	sv_friction = Cvar_Get( "sv_friction", "4", 0, "how fast you slow down" );
	
	public_server = Cvar_Get ("public", "0", 0, "change server type from private to public" );

	sv_reconnect_limit = Cvar_Get ("sv_reconnect_limit", "3", CVAR_ARCHIVE, "max reconnect attempts" );
}

/*
==================
SV_FinalMessage

Used by SV_Shutdown to send a final message to all
connected clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
void SV_FinalMessage (char *message, bool reconnect)
{
	sv_client_t	*cl;
	byte		msg_buf[MAX_MSGLEN];
	sizebuf_t		msg;
	int		i;
	
	MSG_Init( &msg, msg_buf, sizeof(msg_buf));
	MSG_WriteByte( &msg, svc_print );
	MSG_WriteByte( &msg, PRINT_CONSOLE );
	MSG_WriteString( &msg, message );

	if( reconnect )
	{
		MSG_WriteByte( &msg, svc_reconnect );
	}
	else
	{
		MSG_WriteByte( &msg, svc_disconnect );
	}

	// send it twice
	// stagger the packets to crutch operating system limited buffers
	for( i = 0, cl = svs.clients; i < Host_MaxClients(); i++, cl++ )
		if( cl->state >= cs_connected )
			Netchan_Transmit( &cl->netchan, msg.cursize, msg.data );

	for( i = 0, cl = svs.clients; i < Host_MaxClients(); i++, cl++ )
		if( cl->state >= cs_connected )
			Netchan_Transmit( &cl->netchan, msg.cursize, msg.data );
}



/*
================
SV_Shutdown

Called when each game quits,
before Sys_Quit or Sys_Error
================
*/
void SV_Shutdown( bool reconnect )
{
	// already freed
	if(host.state == HOST_ERROR) return;

	MsgDev(D_NOTE, "SV_Shutdown: %s\n", host.finalmsg );
	if (svs.clients) SV_FinalMessage( host.finalmsg, reconnect);

	Master_Shutdown();
	SV_FreeServerProgs();

	// free current level
	memset (&sv, 0, sizeof(sv));
	Host_SetServerState (sv.state);

	// free server static data
	if( svs.clients ) Mem_Free( svs.clients );
	if( svs.baselines ) Mem_Free( svs.baselines );
	if( svs.client_entities ) Mem_Free( svs.client_entities );
	memset( &svs, 0, sizeof(svs));
}

