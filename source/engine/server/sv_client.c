//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_client.c - client interactions
//=======================================================================

#include "common.h"
#include "const.h"
#include "server.h"

#define MAX_FORWARD		6

typedef struct ucmd_s
{
	const char	*name;
	void		(*func)( sv_client_t *cl );
} ucmd_t;

static vec3_t wishdir, forward, right, up;
static float wishspeed;
static bool onground;

/*
=================
SV_GetChallenge

Returns a challenge number that can be used
in a subsequent client_connect command.
We do this to prevent denial of service attacks that
flood the server with invalid connection IPs.  With a
challenge, they must give a valid IP address.
=================
*/
void SV_GetChallenge( netadr_t from )
{
	int	i, oldest = 0;
	int	oldestTime;

	oldestTime = 0x7fffffff;
	// see if we already have a challenge for this ip
	for (i = 0; i < MAX_CHALLENGES; i++ )
	{
		if( !svs.challenges[i].connected && NET_CompareAdr( from, svs.challenges[i].adr ))
			break;
		if( svs.challenges[i].time < oldestTime )
		{
			oldestTime = svs.challenges[i].time;
			oldest = i;
		}
	}

	if( i == MAX_CHALLENGES )
	{
		// this is the first time this client has asked for a challenge
		svs.challenges[oldest].challenge = ((rand()<<16) ^ rand()) ^ svs.realtime;
		svs.challenges[oldest].adr = from;
		svs.challenges[oldest].time = svs.realtime;
		svs.challenges[oldest].connected = false;
		i = oldest;
	}

	// send it back
	Netchan_OutOfBandPrint( NS_SERVER, svs.challenges[i].adr, "challenge %i", svs.challenges[i].challenge );
}

/*
==================
SV_DirectConnect

A connection request that did not come from the master
==================
*/
void SV_DirectConnect( netadr_t from )
{
	char		userinfo[MAX_INFO_STRING];
	sv_client_t	temp, *cl, *newcl;
	edict_t		*ent;
	int		i, edictnum;
	int		version;
	int		qport, count = 0;
	int		challenge;

	version = com.atoi( Cmd_Argv( 1 ));
	if( version != PROTOCOL_VERSION )
	{
		Netchan_OutOfBandPrint( NS_SERVER, from, "print\nServer uses protocol version %i.\n", PROTOCOL_VERSION );
		MsgDev( D_ERROR, "SV_DirectConnect: rejected connect from version %i\n", version );
		return;
	}

	qport = com.atoi( Cmd_Argv( 2 ));
	challenge = com.atoi( Cmd_Argv( 3 ));
	com.strncpy( userinfo, Cmd_Argv( 4 ), sizeof( userinfo ) - 1);
	userinfo[sizeof(userinfo) - 1] = 0;

	// quick reject
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( cl->state == cs_free ) continue;
		if( NET_CompareBaseAdr(from, cl->netchan.remote_address) && (cl->netchan.qport == qport || from.port == cl->netchan.remote_address.port))
		{
			if(!NET_IsLocalAddress( from ) && (svs.realtime - cl->lastconnect) < sv_reconnect_limit->value * 1000 )
			{
				MsgDev( D_INFO, "%s:reconnect rejected : too soon\n", NET_AdrToString( from ));
				return;
			}
			break;
		}
	}
		
	// see if the challenge is valid (LAN clients don't need to challenge)
	if( !NET_IsLocalAddress( from ))
	{
		for( i = 0; i < MAX_CHALLENGES; i++ )
		{
			if( NET_CompareAdr( from, svs.challenges[i].adr ))
			{
				if( challenge == svs.challenges[i].challenge )
					break; // valid challenge
			}
		}
		if( i == MAX_CHALLENGES )
		{
			Netchan_OutOfBandPrint( NS_SERVER, from, "print\nNo or bad challenge for address.\n" );
			return;
		}
		// force the IP key/value pair so the game can filter based on ip
		Info_SetValueForKey( userinfo, "ip", NET_AdrToString( from ));
		svs.challenges[i].connected = true;
		MsgDev( D_INFO, "Client %i connecting with challenge %p\n", i, challenge );
	}
	else
	{
		// force the "ip" info key to "localhost"
		Info_SetValueForKey( userinfo, "ip", "127.0.0.1" );
		Info_SetValueForKey( userinfo, "name", SI->username ); // can be overwrited later
	}

	newcl = &temp;
	Mem_Set( newcl, 0, sizeof( sv_client_t ));

	// if there is already a slot for this ip, reuse it
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( cl->state == cs_free ) continue;
		if( NET_CompareBaseAdr( from, cl->netchan.remote_address ) && (cl->netchan.qport == qport || from.port == cl->netchan.remote_address.port ))
		{
			MsgDev( D_INFO, "%s:reconnect\n", NET_AdrToString( from ));
			newcl = cl;
			goto gotnewcl;
		}
	}

	// find a client slot
	newcl = NULL;
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)
	{
		if( cl->state == cs_free )
		{
			newcl = cl;
			break;
		}
	}
	if( !newcl )
	{
		Netchan_OutOfBandPrint( NS_SERVER, from, "print\nServer is full.\n" );
		MsgDev( D_INFO, "SV_DirectConnect: rejected a connection.\n");
		return;
	}

gotnewcl:	
	// build a new connection
	// accept the new client
	// this is the only place a sv_client_t is ever initialized
	*newcl = temp;
	sv_client = newcl;
	edictnum = (newcl - svs.clients) + 1;

	ent = EDICT_NUM( edictnum );
	ent->pvServerData->client = newcl;
	newcl->edict = ent;
	newcl->challenge = challenge; // save challenge for checksumming

	// get the game a chance to reject this connection or modify the userinfo
	if(!(SV_ClientConnect( ent, userinfo )))
	{
		if(*Info_ValueForKey( userinfo, "rejmsg" )) 
			Netchan_OutOfBandPrint( NS_SERVER, from, "print\n%s\nConnection refused.\n", Info_ValueForKey( userinfo, "rejmsg" ));
		else Netchan_OutOfBandPrint( NS_SERVER, from, "print\nConnection refused.\n" );
		MsgDev( D_ERROR, "SV_DirectConnect: game rejected a connection.\n");
		return;
	}

	// parse some info from the info strings
	SV_UserinfoChanged( newcl, userinfo );

	// send the connect packet to the client
	Netchan_OutOfBandPrint( NS_SERVER, from, "client_connect" );

	Netchan_Setup( NS_SERVER, &newcl->netchan, from, qport );
	MSG_Init( &newcl->reliable, newcl->reliable_buf, sizeof( newcl->reliable_buf ));	// reliable buf
	MSG_Init( &newcl->datagram, newcl->datagram_buf, sizeof( newcl->datagram_buf ));	// datagram buf

	newcl->state = cs_connected;
	newcl->lastmessage = svs.realtime;
	newcl->lastconnect = svs.realtime;

	// if this was the first client on the server, or the last client
	// the server can hold, send a heartbeat to the master.
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
		if( cl->state >= cs_connected ) count++;
	if( count == 1 || count == sv_maxclients->integer )
		svs.last_heartbeat = MAX_HEARTBEAT;
}

/*
==================
SV_FakeConnect

A connection request that came from the game module
==================
*/
edict_t *SV_FakeConnect( const char *netname )
{
	int		i, edictnum;
	char		userinfo[MAX_INFO_STRING];
	sv_client_t	temp, *cl, *newcl;
	edict_t		*ent;

	if( !netname ) netname = "";
	userinfo[0] = '\0';

	// setup fake client name
	Info_SetValueForKey( userinfo, "name", netname );

	// force the IP key/value pair so the game can filter based on ip
	Info_SetValueForKey( userinfo, "ip", "127.0.0.1" );

	// find a client slot
	newcl = &temp;
	Mem_Set( newcl, 0, sizeof( sv_client_t ));
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( cl->state == cs_free )
		{
			newcl = cl;
			break;
		}
	}
	if( !newcl )
	{
		MsgDev( D_INFO, "SV_DirectConnect: rejected a connection.\n");
		return NULL;
	}

	// build a new connection
	// accept the new client
	// this is the only place a sv_client_t is ever initialized
	*newcl = temp;
	sv_client = newcl;
	edictnum = (newcl - svs.clients) + 1;

	ent = EDICT_NUM( edictnum );
	ent->pvServerData->client = newcl;
	newcl->edict = ent;
	newcl->challenge = -1;		// fake challenge
	ent->v.flags |= FL_FAKECLIENT;	// mark it as fakeclient

	// get the game a chance to reject this connection or modify the userinfo
	if( !SV_ClientConnect( ent, userinfo ))
	{
		MsgDev( D_ERROR, "SV_DirectConnect: game rejected a connection.\n" );
		return NULL;
	}

	// parse some info from the info strings
	SV_UserinfoChanged( newcl, userinfo );

	newcl->state = cs_spawned;
	newcl->lastmessage = svs.realtime;	// don't timeout
	newcl->lastconnect = svs.realtime;

	return ent;
}

/*
=====================
SV_ClientCconnect

QC code can rejected a connection for some reasons
e.g. ipban
=====================
*/
bool SV_ClientConnect( edict_t *ent, char *userinfo )
{
	bool result = true;

	// make sure we start with known default
	if( !sv.loadgame ) ent->v.flags = 0;

	MsgDev( D_NOTE, "SV_ClientConnect()\n" );
	svgame.globals->time = sv.time * 0.001f;
	result = svgame.dllFuncs.pfnClientConnect( ent, userinfo );

	return result;
}

/*
=====================
SV_DropClient

Called when the player is totally leaving the server, either willingly
or unwillingly.  This is NOT called if the entire server is quiting
or crashing.
=====================
*/
void SV_DropClient( sv_client_t *drop )
{
	int	i;
	
	if( drop->state == cs_zombie ) return;	// already dropped

	// add the disconnect
	if(!( drop->edict && (drop->edict->v.flags & FL_FAKECLIENT )))
		MSG_WriteByte( &drop->netchan.message, svc_disconnect );

	// let the game known about client state
	svgame.globals->time = sv.time * 0.001f;

	if( drop->edict->v.flags & FL_SPECTATOR )
		svgame.dllFuncs.pfnSpectatorDisconnect( drop->edict );
	else svgame.dllFuncs.pfnClientDisconnect( drop->edict );

	SV_FreeEdict( drop->edict );
	if( drop->download ) drop->download = NULL;

	drop->state = cs_zombie; // become free in a few seconds
	drop->name[0] = 0;

	// if this was the last client on the server, send a heartbeat
	// to the master so it is known the server is empty
	// send a heartbeat now so the master will get up to date info
	// if there is already a slot for this ip, reuse it
	for( i = 0; i < sv_maxclients->integer; i++ )
	{
		if( svs.clients[i].state >= cs_connected )
			break;
	}
	if( i == sv_maxclients->integer ) svs.last_heartbeat = MAX_HEARTBEAT;
}

/*
==============================================================================

SVC COMMAND REDIRECT

==============================================================================
*/
void SV_BeginRedirect( netadr_t adr, int target, char *buffer, int buffersize, void (*flush))
{
	if( !target || !buffer || !buffersize || !flush )
		return;

	host.rd.target = target;
	host.rd.buffer = buffer;
	host.rd.buffersize = buffersize;
	host.rd.flush = flush;
	host.rd.address = adr;
	host.rd.buffer[0] = 0;
}

void SV_FlushRedirect( netadr_t adr, int dest, char *buf )
{
	if( sv_client->edict && (sv_client->edict->v.flags & FL_FAKECLIENT))
		return;

	switch( dest )
	{
	case RD_PACKET:
		Netchan_OutOfBandPrint( NS_SERVER, adr, "print\n%s", buf );
		break;
	case RD_CLIENT:
		if( !sv_client ) return; // client not set
		MSG_WriteByte( &sv_client->netchan.message, svc_print );
		MSG_WriteByte( &sv_client->netchan.message, PRINT_HIGH );
		MSG_WriteString( &sv_client->netchan.message, buf );
		break;
	case RD_NONE:
		MsgDev( D_ERROR, "SV_FlushRedirect: %s: invalid destination\n", NET_AdrToString( adr ));
		break;
	}
}

void SV_EndRedirect( void )
{
	host.rd.flush( host.rd.address, host.rd.target, host.rd.buffer );

	host.rd.target = 0;
	host.rd.buffer = NULL;
	host.rd.buffersize = 0;
	host.rd.flush = NULL;
}

/*
===============
SV_StatusString

Builds the string that is sent as heartbeats and status replies
===============
*/
char *SV_StatusString( void )
{
	char		player[1024];
	static char	status[MAX_MSGLEN - 16];
	int		i;
	sv_client_t	*cl;
	int		statusLength;
	int		playerLength;

	com.strcpy( status, Cvar_Serverinfo());
	com.strcat( status, "\n" );
	statusLength = com.strlen(status);

	for( i = 0; i < sv_maxclients->integer; i++ )
	{
		cl = &svs.clients[i];
		if( cl->state == cs_connected || cl->state == cs_spawned )
		{
			com.sprintf( player, "%i %i \"%s\"\n", (int)cl->edict->v.frags, cl->ping, cl->name );
			playerLength = com.strlen(player);
			if( statusLength + playerLength >= sizeof(status))
				break; // can't hold any more
			com.strcpy( status + statusLength, player );
			statusLength += playerLength;
		}
	}
	return status;
}

/*
================
SV_Status

Responds with all the info that qplug or qspy can see
================
*/
void SV_Status( netadr_t from )
{
	Netchan_OutOfBandPrint( NS_SERVER, from, "print\n%s", SV_StatusString());
}

/*
================
SV_Ack

================
*/
void SV_Ack( netadr_t from )
{
	Msg( "ping %s\n", NET_AdrToString( from ));
}

/*
================
SV_Info

Responds with short info for broadcast scans
The second parameter should be the current protocol version number.
================
*/
void SV_Info( netadr_t from )
{
	char	string[64];
	int	i, count = 0;
	int	version;

	// ignore in single player
	if( sv_maxclients->integer == 1 ) return;

	version = com.atoi(Cmd_Argv( 1 ));

	if( version != PROTOCOL_VERSION )
	{
		com.sprintf( string, "%s: wrong version\n", hostname->string, sizeof( string ));
	}
	else
	{
		for( i = 0; i < sv_maxclients->integer; i++ )
			if( svs.clients[i].state >= cs_connected )
				count++;
		com.sprintf( string, "%16s %8s %2i/%2i\n", hostname->string, sv.name, count, sv_maxclients->integer );
	}
	Netchan_OutOfBandPrint( NS_SERVER, from, "info\n%s", string );
}

/*
================
SV_Ping

Just responds with an acknowledgement
================
*/
void SV_Ping( netadr_t from )
{
	Netchan_OutOfBandPrint( NS_SERVER, from, "ack" );
}

bool Rcon_Validate( void )
{
	if( !com.strlen( rcon_password->string ))
		return false;
	if( !com.strcmp( Cmd_Argv( 1 ), rcon_password->string ))
		return false;
	return true;
}

/*
===============
SV_RemoteCommand

A client issued an rcon command.
Shift down the remaining args
Redirect all printfs
===============
*/
void SV_RemoteCommand( netadr_t from, sizebuf_t *msg )
{
	char		remaining[1024];
	static char	outputbuf[MAX_MSGLEN - 16];
	int		i;

	if(!Rcon_Validate()) MsgDev(D_INFO, "Bad rcon from %s:\n%s\n", NET_AdrToString( from ), msg->data + 4 );
	else MsgDev( D_INFO, "Rcon from %s:\n%s\n", NET_AdrToString( from ), msg->data + 4 );
	SV_BeginRedirect( from, RD_PACKET, outputbuf, MAX_MSGLEN - 16, SV_FlushRedirect );

	if( !Rcon_Validate( ))
	{
		MsgDev( D_WARN, "Bad rcon_password.\n" );
	}
	else
	{
		remaining[0] = 0;
		for( i = 2; i < Cmd_Argc(); i++ )
		{
			com.strcat( remaining, Cmd_Argv( i ));
			com.strcat( remaining, " " );
		}
		Cmd_ExecuteString( remaining );
	}
	SV_EndRedirect();
}

/*
===========
PutClientInServer

Called when a player connects to a server or respawns in
a deathmatch.
============
*/
void SV_PutClientInServer( edict_t *ent )
{
	int		index;
	sv_client_t	*client;

	index = NUM_FOR_EDICT( ent ) - 1;
	client = ent->pvServerData->client;

	svgame.globals->time = sv.time * 0.001f;
	ent->pvServerData->s.ed_type = ED_CLIENT; // init edict type
	ent->free = false;

	if( !sv.changelevel && !sv.loadgame )
	{	
		if( ent->v.flags & FL_SPECTATOR )
		{
			// setup spectatormaxspeed and refresh physinfo
			SV_SetClientMaxspeed( client, svgame.movevars.spectatormaxspeed );

			svgame.dllFuncs.pfnSpectatorConnect( ent );
		}
		else
		{
			// copy signon buffer
			MSG_WriteData( &client->reliable, sv.signon.data, sv.signon.cursize );

			// setup maxspeed and refresh physinfo
			SV_SetClientMaxspeed( client, svgame.movevars.maxspeed );
	
			// fisrt entering
			svgame.dllFuncs.pfnClientPutInServer( ent );

			ent->v.view_ofs[2] = GI->viewheight[0];
			ent->v.viewangles[ROLL] = 0;	// cut off any camera rolling
			ent->v.origin[2] -= GI->client_mins[2][2]; // FIXME: make sure off ground
		}
	}
	else
	{
	}

	client->pViewEntity = NULL; // reset pViewEntity

	if( !( ent->v.flags & FL_FAKECLIENT ))
	{
		MSG_WriteByte( &client->netchan.message, svc_setview );
		MSG_WriteWord( &client->netchan.message, NUM_FOR_EDICT( client->edict ));
		MSG_Send( MSG_ONE, NULL, client->edict );
	}

	Mem_EmptyPool( svgame.temppool ); // all tempstrings can be frees now

	// clear any temp states
	sv.changelevel = false;
	sv.loadgame = false;

	MsgDev( D_INFO, "level loaded at %g sec\n", (Sys_Milliseconds() - svs.timestart) * 0.001f );
}

/*
==================
SV_TogglePause
==================
*/
void SV_TogglePause( const char *msg )
{
	sv.paused ^= 1;

	if( msg ) SV_BroadcastPrintf( PRINT_HIGH, "%s", msg );

	// send notification to all clients
	MSG_Begin( svc_setpause );
	MSG_WriteByte( &sv.multicast, sv.paused );
	MSG_Send( MSG_ALL, vec3_origin, NULL );
}

/*
============================================================

CLIENT COMMAND EXECUTION

============================================================
*/
/*
================
SV_New_f

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_New_f( sv_client_t *cl )
{
	int	playernum;
	edict_t	*ent;

	if( cl->state != cs_connected )
	{
		MsgDev( D_INFO, "new is not valid from the console\n" );
		return;
	}

	playernum = cl - svs.clients;

	// send the serverdata
	MSG_WriteByte( &cl->netchan.message, svc_serverdata );
	MSG_WriteLong( &cl->netchan.message, PROTOCOL_VERSION);
	MSG_WriteLong( &cl->netchan.message, svs.spawncount );
	MSG_WriteShort( &cl->netchan.message, playernum );
	MSG_WriteString( &cl->netchan.message, sv.configstrings[CS_NAME] );
	MSG_WriteString( &cl->netchan.message, STRING( EDICT_NUM( 0 )->v.message ));	// Map Message

	// game server
	if( sv.state == ss_active )
	{
		// set up the entity for the client
		ent = EDICT_NUM( playernum + 1 );
		ent->serialnumber = playernum + 1;
		cl->edict = ent;
		Mem_Set( &cl->lastcmd, 0, sizeof( cl->lastcmd ));

		// begin fetching configstrings
		MSG_WriteByte( &cl->netchan.message, svc_stufftext );
		MSG_WriteString( &cl->netchan.message, va( "cmd configstrings %i %i\n", svs.spawncount, 0 ));
	}
}

/*
==================
SV_Configstrings_f
==================
*/
void SV_Configstrings_f( sv_client_t *cl )
{
	int	start;
	string	cs;

	if( cl->state != cs_connected )
	{
		MsgDev( D_INFO, "configstrings is not valid from the console\n" );
		return;
	}

	// handle the case of a level changing while a client was connecting
	if( com.atoi( Cmd_Argv( 1 )) != svs.spawncount )
	{
		MsgDev( D_INFO, "configstrings from different level\n" );
		SV_New_f( cl );
		return;
	}
	
	start = com.atoi( Cmd_Argv( 2 ));

	// write a packet full of data
	while( cl->netchan.message.cursize < (MAX_MSGLEN / 2) && start < MAX_CONFIGSTRINGS )
	{
		if( sv.configstrings[start][0])
		{
			MSG_WriteByte( &cl->netchan.message, svc_configstring );
			MSG_WriteShort( &cl->netchan.message, start );
			MSG_WriteString( &cl->netchan.message, sv.configstrings[start] );
		}
		start++;
	}
	if( start == MAX_CONFIGSTRINGS ) com.snprintf( cs, MAX_STRING, "cmd baselines %i %i\n", svs.spawncount, 0 );
	else com.snprintf( cs, MAX_STRING, "cmd configstrings %i %i\n", svs.spawncount, start );

	// send next command
	MSG_WriteByte( &cl->netchan.message, svc_stufftext );
	MSG_WriteString( &cl->netchan.message, cs );
}

/*
==================
SV_Baselines_f
==================
*/
void SV_Baselines_f( sv_client_t *cl )
{
	int		start;
	entity_state_t	*base, nullstate;
	string		baseline;

	if( cl->state != cs_connected )
	{
		MsgDev( D_INFO, "baselines is not valid from the console\n" );
		return;
	}
	
	// handle the case of a level changing while a client was connecting
	if( com.atoi( Cmd_Argv( 1 )) != svs.spawncount )
	{
		MsgDev( D_INFO, "baselines from different level\n" );
		SV_New_f( cl );
		return;
	}
	
	start = com.atoi( Cmd_Argv( 2 ));

	Mem_Set( &nullstate, 0, sizeof( nullstate ));

	// write a packet full of data
	while( cl->netchan.message.cursize < MAX_MSGLEN / 2 && start < GI->max_edicts )
	{
		base = &svs.baselines[start];
		if( base->modelindex || base->soundindex || base->effects )
		{
			MSG_WriteByte( &cl->netchan.message, svc_spawnbaseline );
			MSG_WriteDeltaEntity( &nullstate, base, &cl->netchan.message, true, true );
		}
		start++;
	}

	if( start == GI->max_edicts ) com.snprintf( baseline, MAX_STRING, "precache %i\n", svs.spawncount );
	else com.snprintf( baseline, MAX_STRING, "cmd baselines %i %i\n", svs.spawncount, start );

	// send next command
	MSG_WriteByte( &cl->netchan.message, svc_stufftext );
	MSG_WriteString( &cl->netchan.message, baseline );
}

/*
==================
SV_Begin_f
==================
*/
void SV_Begin_f( sv_client_t *cl )
{
	// handle the case of a level changing while a client was connecting
	if( com.atoi( Cmd_Argv( 1 )) != svs.spawncount )
	{
		Msg( "begin from different level\n" );
		SV_New_f( cl );
		return;
	}

	cl->state = cs_spawned;
	SV_PutClientInServer( cl->edict );

	// if we are paused, tell the client
	if( sv.paused )
	{
		MSG_Begin( svc_setpause );
		MSG_WriteByte( &sv.multicast, sv.paused );
		MSG_Send( MSG_ONE, vec3_origin, cl->edict );
		SV_ClientPrintf( cl, PRINT_HIGH, "Server is paused.\n" );
	}
}

/*
==================
SV_NextDownload_f
==================
*/
void SV_NextDownload_f( sv_client_t *cl )
{
	int	percent;
	int	r, size;

	if( !cl->download ) return;

	r = cl->downloadsize - cl->downloadcount;
	if( r > 1024 ) r = 1024;

	MSG_WriteByte( &cl->netchan.message, svc_download );
	MSG_WriteShort( &cl->netchan.message, r );

	cl->downloadcount += r;
	size = cl->downloadsize;
	if( !size ) size = 1;
	percent = cl->downloadcount * 100 / size;
	MSG_WriteByte( &cl->netchan.message, percent );
	MSG_WriteData( &cl->netchan.message, cl->download + cl->downloadcount - r, r );
	if( cl->downloadcount == cl->downloadsize ) cl->download = NULL;
}

/*
==================
SV_BeginDownload_f
==================
*/
void SV_BeginDownload_f( sv_client_t *cl )
{
	char	*name;
	int	offset = 0;

	name = Cmd_Argv( 1 );
	if(Cmd_Argc() > 2 ) offset = com.atoi(Cmd_Argv(2)); // continue download from
	cl->download = FS_LoadFile( name, &cl->downloadsize );
	cl->downloadcount = offset;
	if( offset > cl->downloadsize ) cl->downloadcount = cl->downloadsize;

	if( !allow_download->integer || !cl->download )
	{
		MsgDev( D_ERROR, "SV_BeginDownload_f: couldn't download %s to %s\n", name, cl->name );
		if( cl->download ) Mem_Free( cl->download );
		MSG_WriteByte( &cl->netchan.message, svc_download );
		MSG_WriteShort( &cl->netchan.message, -1 );
		MSG_WriteByte( &cl->netchan.message, 0 );
		cl->download = NULL;
		return;
	}

	SV_NextDownload_f( cl );
	MsgDev( D_INFO, "Downloading %s to %s\n", name, cl->name );
}

/*
=================
SV_Disconnect_f

The client is going to disconnect, so remove the connection immediately
=================
*/
void SV_Disconnect_f( sv_client_t *cl )
{
	SV_DropClient( cl );	
}

/*
==================
SV_ShowServerinfo_f

Dumps the serverinfo info string
==================
*/
void SV_ShowServerinfo_f( sv_client_t *cl )
{
	Info_Print( Cvar_Serverinfo());
}

/*
==================
SV_Pause_f
==================
*/
void SV_Pause_f( sv_client_t *cl )
{
	string	message;

	if( !sv_pausable->integer )
	{
		SV_ClientPrintf( cl, PRINT_HIGH, "Pause not allowed.\n" );
		return;
	}

	if( cl->edict->v.flags & FL_SPECTATOR )
	{
		SV_ClientPrintf( cl, PRINT_HIGH, "Spectators can not pause.\n" );
		return;
	}

	if( !sv.paused ) com.snprintf( message, MAX_STRING, "%s paused the game\n", cl->name );
	else com.snprintf( message, MAX_STRING, "%s unpaused the game\n", cl->name );

	SV_TogglePause( message );
}


/*
=================
SV_UserinfoChanged

Pull specific info from a newly changed userinfo string
into a more C freindly form.
=================
*/
void SV_UserinfoChanged( sv_client_t *cl, const char *userinfo )
{
	char	*val;
	int	i;

	if( !userinfo || !userinfo[0] ) return; // ignored

	com.strncpy( cl->userinfo, userinfo, sizeof( cl->userinfo ));

	// name for C code (make colored string)
	com.snprintf( cl->name, sizeof( cl->name ), "^2%s^7", Info_ValueForKey( cl->userinfo, "name" ));

	// rate command
	val = Info_ValueForKey( cl->userinfo, "rate" );
	if( com.strlen( val ))
	{
		i = com.atoi( val );
		cl->rate = i;
		cl->rate = bound ( 100, cl->rate, 15000 );
	}
	else cl->rate = 5000;

	// msg command
	val = Info_ValueForKey( cl->userinfo, "msg" );
	if( com.strlen( val ))
		cl->messagelevel = com.atoi( val );

	// call prog code to allow overrides
	svgame.globals->time = sv.time * 0.001f;
	svgame.globals->frametime = sv.frametime * 0.001f;
	svgame.dllFuncs.pfnClientUserInfoChanged( cl->edict, cl->userinfo );
}

/*
==================
SV_UpdateUserinfo_f
==================
*/
static void SV_UpdateUserinfo_f( sv_client_t *cl )
{
	SV_UserinfoChanged( cl, Cmd_Argv( 1 ));
}

ucmd_t ucmds[] =
{
{ "new", SV_New_f },
{ "begin", SV_Begin_f },
{ "pause", SV_Pause_f },
{ "baselines", SV_Baselines_f },
{ "info", SV_ShowServerinfo_f },
{ "nextdl", SV_NextDownload_f },
{ "disconnect", SV_Disconnect_f },
{ "download", SV_BeginDownload_f },
{ "userinfo", SV_UpdateUserinfo_f },
{ "configstrings", SV_Configstrings_f },
{ NULL, NULL }
};

/*
==================
SV_ExecuteUserCommand
==================
*/
void SV_ExecuteClientCommand( sv_client_t *cl, char *s )
{
	ucmd_t	*u;

	Cmd_TokenizeString( s );
	for( u = ucmds; u->name; u++ )
	{
		if( !com.strcmp( Cmd_Argv( 0 ), u->name ))
		{
			MsgDev( D_NOTE, "ucmd->%s()\n", u->name );
			u->func( cl );
			break;
		}
	}

	if( !u->name && sv.state == ss_active )
	{
		// custom client commands
		svgame.globals->time = sv.time * 0.001f;
		svgame.globals->frametime = sv.frametime * 0.001f;
		svgame.dllFuncs.pfnClientCommand( cl->edict );
	}
}

/*
=================
MSG_Begin

Misc helper function
=================
*/

void _MSG_Begin( int dest, const char *filename, int fileline )
{
	_MSG_WriteBits( &sv.multicast, dest, "MSG_Begin", NET_BYTE, filename, fileline );
}

/*
=================
SV_Send

Sends the contents of sv.multicast to a subset of the clients,
then clears sv.multicast.

MULTICAST_ONE	send to one client (ent can't be NULL)
MULTICAST_ALL	same as broadcast (origin can be NULL)
MULTICAST_PVS	send to clients potentially visible from org
MULTICAST_PHS	send to clients potentially hearable from org
=================
*/
void _MSG_Send( int dest, const vec3_t origin, const edict_t *ent, const char *filename, int fileline )
{
	byte		*mask = NULL;
	int		leafnum = 0, cluster = 0;
	int		area1 = 0, area2 = 0;
	int		j, numclients = sv_maxclients->integer;
	sv_client_t	*cl, *current = svs.clients;
	bool		reliable = false;
	bool		specproxy = false;

	switch( dest )
	{
	case MSG_INIT:
		if( sv.state == ss_loading )
		{
			// copy signon buffer
			MSG_WriteData( &sv.signon, sv.multicast.data, sv.multicast.cursize );
			MSG_Clear( &sv.multicast );
			return;
		}
		// intentional fallthrough (in-game MSG_INIT it's a MSG_ALL reliable)
	case MSG_ALL:
		reliable = true;
		// intentional fallthrough
	case MSG_BROADCAST:
		// nothing to sort	
		break;
	case MSG_PAS_R:
		reliable = true;
		// intentional fallthrough
	case MSG_PAS:
		if( origin == NULL ) return;
		leafnum = CM_PointLeafnum( origin );
		cluster = CM_LeafCluster( leafnum );
		mask = CM_ClusterPHS( cluster );
		area1 = CM_LeafArea( leafnum );
		break;
	case MSG_PVS_R:
		reliable = true;
		// intentional fallthrough
	case MSG_PVS:
		if( origin == NULL ) return;
		leafnum = CM_PointLeafnum( origin );
		cluster = CM_LeafCluster( leafnum );
		mask = CM_ClusterPVS( cluster );
		area1 = CM_LeafArea( leafnum );
		break;
	case MSG_ONE:
		reliable = true;
		// intentional fallthrough
	case MSG_ONE_UNRELIABLE:
		if( ent == NULL ) return;
		j = NUM_FOR_EDICT( ent );
		if( j < 1 || j > numclients ) return;
		current = svs.clients + (j - 1);
		numclients = 1; // send to one
		break;
	case MSG_SPEC:
		specproxy = reliable = true;
		break;
	default:
		Host_Error( "MSG_Send: bad dest: %i (called at %s:%i)\n", dest, filename, fileline );
		return;
	}

	// send the data to all relevent clients (or once only)
	for( j = 0, cl = current; j < numclients; j++, cl++ )
	{
		if( cl->state == cs_free || cl->state == cs_zombie )
			continue;
		if( cl->state != cs_spawned && !reliable )
			continue;

		if( specproxy && !( cl->edict->v.flags & FL_PROXY ))
			continue;

		if( cl->edict && ( cl->edict->v.flags & FL_FAKECLIENT ))
			continue;

		if( mask )
		{
			area2 = CM_LeafArea( leafnum );
			cluster = CM_LeafCluster( leafnum );
			leafnum = CM_PointLeafnum( cl->edict->v.origin );
			if(!CM_AreasConnected( area1, area2 )) continue;
			if( mask && (!(mask[cluster>>3] & (1<<(cluster & 7)))))
				continue;
		}

		if( reliable ) MSG_WriteData( &cl->reliable, sv.multicast.data, sv.multicast.cursize );
		else MSG_WriteData( &cl->datagram, sv.multicast.data, sv.multicast.cursize );
	}
	MSG_Clear( &sv.multicast );
}

/*
=================
SV_ConnectionlessPacket

A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
=================
*/
void SV_ConnectionlessPacket( netadr_t from, sizebuf_t *msg )
{
	char	*s;
	char	*c;

	MSG_BeginReading( msg );
	MSG_ReadLong( msg );// skip the -1 marker

	s = MSG_ReadStringLine( msg );
	Cmd_TokenizeString( s );

	c = Cmd_Argv( 0 );
	MsgDev( D_INFO, "SV_ConnectionlessPacket: %s : %s\n", NET_AdrToString( from ), c );

	if( !com.strcmp( c, "ping" )) SV_Ping( from );
	else if( !com.strcmp( c, "ack" )) SV_Ack( from );
	else if( !com.strcmp( c, "status" )) SV_Status( from );
	else if( !com.strcmp( c, "info" )) SV_Info( from );
	else if( !com.strcmp( c, "getchallenge" )) SV_GetChallenge( from );
	else if( !com.strcmp( c, "connect" )) SV_DirectConnect( from );
	else if( !com.strcmp( c, "rcon" )) SV_RemoteCommand( from, msg );
	else MsgDev( D_ERROR, "bad connectionless packet from %s:\n%s\n", NET_AdrToString( from ), s );
}

/*
===============
SV_SetIdealPitch
===============
*/
void SV_SetIdealPitch( sv_client_t *cl )
{
	float		angleval, sinval, cosval;
	trace_t		tr;
	vec3_t		top, bottom;
	float		z[MAX_FORWARD];
	int		i, j;
	int		step, dir, steps;
	edict_t		*ent = cl->edict;

	if( !( ent->v.flags & FL_ONGROUND ))
		return;
		
	angleval = ent->v.angles[YAW] * M_PI * 2 / 360;
	com.sincos( angleval, &sinval, &cosval );

	for( i = 0; i < MAX_FORWARD; i++ )
	{
		top[0] = ent->v.origin[0] + cosval * (i + 3) * 12;
		top[1] = ent->v.origin[1] + sinval * (i + 3) * 12;
		top[2] = ent->v.origin[2] + ent->v.view_ofs[2];
		
		bottom[0] = top[0];
		bottom[1] = top[1];
		bottom[2] = top[2] - 160;
		
		tr = SV_Move( top, vec3_origin, vec3_origin, bottom, MOVE_NOMONSTERS, ent );
		if( tr.fAllSolid )
			return;	// looking at a wall, leave ideal the way is was

		if( tr.flFraction == 1.0f )
			return;	// near a dropoff
		
		z[i] = top[2] + tr.flFraction * (bottom[2] - top[2]);
	}
	
	dir = 0;
	steps = 0;
	for( j = 1; j < i; j++ )
	{
		step = z[j] - z[j-1];
		if( step > -ON_EPSILON && step < ON_EPSILON )
			continue;

		if( dir && ( step-dir > ON_EPSILON || step-dir < -ON_EPSILON ))
			return; // mixed changes

		steps++;	
		dir = step;
	}
	
	if( !dir )
	{
		ent->v.ideal_pitch = 0;
		return;
	}
	
	if( steps < 2 ) return;
	ent->v.ideal_pitch = -dir * sv_idealpitchscale->value;
}

/*
==================
SV_UserFriction

==================
*/
void SV_UserFriction( sv_client_t *cl )
{
	float	speed, newspeed;
	float	*origin, *vel;
	float	control, friction;
	vec3_t	start, stop;
	trace_t	trace;

	vel = cl->edict->v.velocity;
	origin = cl->edict->v.origin;

	speed = com.sqrt( vel[0] * vel[0] + vel[1] * vel[1] );
	if( !speed ) return;

	// if the leading edge is over a dropoff, increase friction
	start[0] = stop[0] = origin[0] + vel[0] / speed * 16;
	start[1] = stop[1] = origin[1] + vel[1] / speed * 16;
	start[2] = origin[2] + cl->edict->v.mins[2];
	stop[2] = start[2] - 34;

	trace = SV_Move( start, vec3_origin, vec3_origin, stop, MOVE_NOMONSTERS, cl->edict );

	if( trace.flFraction == 1.0 )
		friction = sv_friction->value * sv_edgefriction->value;
	else friction = sv_friction->value;

	// apply friction
	control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
	newspeed = speed - svgame.globals->frametime * control * friction;

	if( newspeed < 0 ) newspeed = 0;
	else newspeed /= speed;
	VectorScale( vel, newspeed, vel );
}

/*
==============
SV_Accelerate
==============
*/
void SV_Accelerate( sv_client_t *cl )
{
	int	i;
	float	addspeed;
	float	accelspeed, currentspeed;

	currentspeed = DotProduct( cl->edict->v.velocity, wishdir );
	addspeed = wishspeed - currentspeed;
	if( addspeed <= 0 ) return;
	accelspeed = sv_accelerate->value * svgame.globals->frametime * wishspeed;
	if( accelspeed > addspeed ) accelspeed = addspeed;

	for( i = 0; i < 3; i++ )
		cl->edict->v.velocity[i] += accelspeed * wishdir[i];
}

/*
==============
SV_AirAccelerate
==============
*/
void SV_AirAccelerate( sv_client_t *cl, vec3_t wishveloc )
{
	int	i;
	float	addspeed, accel;
	float	wishspd, accelspeed, currentspeed;

	accel = sv_airaccelerate->value ? sv_airaccelerate->value : 1.0f;
	wishspd = VectorNormalizeLength( wishveloc );
	if( wishspd > 30 ) wishspd = 30;
	currentspeed = DotProduct( cl->edict->v.velocity, wishveloc );
	addspeed = wishspd - currentspeed;
	if( addspeed <= 0 ) return;
	accelspeed = accel * wishspeed * svgame.globals->frametime;
	if( accelspeed > addspeed ) accelspeed = addspeed;

	for( i = 0; i < 3; i++ )
		cl->edict->v.velocity[i] += accelspeed * wishveloc[i];
}

void SV_DropPunchAngle( sv_client_t *cl )
{
	float	len;

	len = VectorNormalizeLength( cl->edict->v.punchangle );

	len -= 10 * svgame.globals->frametime;
	if( len < 0 ) len = 0;
	VectorScale( cl->edict->v.punchangle, len, cl->edict->v.punchangle );
}

/*
===================
SV_WaterMove

===================
*/
void SV_WaterMove( sv_client_t *cl, usercmd_t *cmd )
{
	int	i;
	vec3_t	wishvel;
	float	speed, newspeed;
	float	wishspeed, addspeed;
	float	accelspeed;

	// user intentions
	AngleVectors( cl->edict->v.viewangles, forward, right, up );

	for( i = 0; i < 3; i++ )
		wishvel[i] = forward[i] * cmd->forwardmove + right[i] * cmd->sidemove;

	if( !cmd->forwardmove && !cmd->sidemove && !cmd->upmove )
		wishvel[2] -= 60; // drift towards bottom
	else wishvel[2] += cmd->upmove;

	wishspeed = VectorLength( wishvel );
	if( wishspeed > sv_maxspeed->value )
	{
		VectorScale( wishvel, (sv_maxspeed->value / wishspeed), wishvel );
		wishspeed = sv_maxspeed->value;
	}
	wishspeed *= 0.7;

	// water friction
	speed = VectorLength( cl->edict->v.velocity );
	if( speed )
	{
		newspeed = speed - svgame.globals->frametime * speed * sv_friction->value;
		if( newspeed < 0 ) newspeed = 0;
		VectorScale( cl->edict->v.velocity, (newspeed / speed), cl->edict->v.velocity );
	}
	else newspeed = 0;

	// water acceleration
	if( !wishspeed ) return;

	addspeed = wishspeed - newspeed;
	if( addspeed <= 0 ) return;

	VectorNormalize( wishvel );
	accelspeed = sv_accelerate->value * wishspeed * svgame.globals->frametime;
	if( accelspeed > addspeed ) accelspeed = addspeed;

	for( i = 0; i < 3; i++ )
		cl->edict->v.velocity[i] += accelspeed * wishvel[i];
}

void SV_WaterJump( sv_client_t *cl )
{
	if( svgame.globals->time > cl->edict->v.teleport_time || !cl->edict->v.waterlevel )
	{
		cl->edict->v.flags &= ~FL_WATERJUMP;
		cl->edict->v.teleport_time = 0;
	}
	cl->edict->v.velocity[0] = cl->edict->v.movedir[0];
	cl->edict->v.velocity[1] = cl->edict->v.movedir[1];
}

/*
===================
SV_AirMove

===================
*/
void SV_AirMove( sv_client_t *cl, usercmd_t *cmd )
{
	int	i;
	vec3_t	wishvel;
	float	fmove, smove;

	wishvel[2] = 0;
	wishvel[0] = -cl->edict->v.angles[0];
	wishvel[1] = cl->edict->v.angles[1];
	AngleVectors( wishvel, forward, right, up );

	fmove = cmd->forwardmove;
	smove = cmd->sidemove;

	// HACKHACK to not let you back into teleporter
	if( svgame.globals->time < cl->edict->v.teleport_time && fmove < 0 )
		fmove = 0;

	for( i = 0; i < 3; i++ )
		wishvel[i] = forward[i] * fmove + right[i] * smove;

	if( cl->edict->v.movetype != MOVETYPE_WALK )
		wishvel[2] += cmd->upmove;
	else wishvel[2] = 0;
		
	wishspeed = VectorNormalizeLength2( wishvel, wishdir );
	if( wishspeed > sv_maxspeed->value )
	{
		VectorScale( wishvel, (sv_maxspeed->value / wishspeed), wishvel );
		wishspeed = sv_maxspeed->value;
	}

	if( cl->edict->v.movetype == MOVETYPE_NOCLIP )
	{
		// noclip
		VectorCopy( wishvel, cl->edict->v.velocity );
	}
	else if( onground )
	{
		SV_UserFriction( cl );
		SV_Accelerate( cl );
	}
	else
	{
		// not on ground, so little effect on velocity
		SV_AirAccelerate( cl, wishvel );
	}
}

/*
===============
SV_CalcRoll

Used by view and sv_user
===============
*/
float SV_CalcRoll( vec3_t angles, vec3_t velocity )
{
	vec3_t	right;
	float	sign, side, value;

	AngleVectors( angles, NULL, right, NULL );
	side = DotProduct( velocity, right );
	sign = side < 0 ? -1 : 1;
	side = fabs( side );

	value = sv_rollangle->value;

	if( side < sv_rollspeed->value )
		side = side * value / sv_rollspeed->value;
	else side = value;

	return side*sign;

}

/*
==================
SV_ClientThink

Also called by bot code
==================
*/
void SV_ClientThink( sv_client_t *cl, usercmd_t *cmd )
{
	vec3_t	viewangles;

	if( sv.paused ) return; // paused

	cl->commandMsec -= cmd->msec;

	if( cl->commandMsec < 0 && sv_enforcetime->integer )
	{
		MsgDev( D_INFO, "SV_ClientThink: commandMsec underflow from %s\n", cl->name );
		return;
	}

	// make sure the velocity is sane (not a NaN)
	SV_CheckVelocity( cl->edict );

	if( cl->edict->v.movetype == MOVETYPE_NONE )
		return;

	onground = (cl->edict->v.flags & FL_ONGROUND);

	SV_DropPunchAngle( cl );

	// if dead, behave differently
	if( cl->edict->v.health <= 0 )
		return;

	// show 1/3 the pitch angle and all the roll angle
	VectorAdd( cl->edict->v.viewangles, cl->edict->v.punchangle, viewangles );
	cl->edict->v.viewangles[ROLL] = SV_CalcRoll( viewangles, cl->edict->v.velocity ) * 4;

	if( !cl->edict->v.fixangle )
	{
		cl->edict->v.angles[PITCH] = -viewangles[PITCH] / 3;
		cl->edict->v.angles[YAW] = viewangles[YAW];
	}

	// waterjump
	if( cl->edict->v.flags & FL_WATERJUMP )
	{
		SV_WaterJump( cl );
		SV_CheckVelocity( cl->edict );
		return;
	}

	// walk
	if(( cl->edict->v.waterlevel >= 2 ) && ( cl->edict->v.movetype != MOVETYPE_NOCLIP ))
	{
		SV_WaterMove( cl, cmd );
		SV_CheckVelocity( cl->edict );
		return;
	}

	SV_AirMove( cl, cmd );
	SV_CheckVelocity( cl->edict );
}

/*
==================
SV_ReadClientMove

The message usually contains all the movement commands 
that were in the last three packets, so that the information
in dropped packets can be recovered.

On very fast clients, there may be multiple usercmd packed into
each of the backup packets.
==================
*/
static void SV_ReadClientMove( sv_client_t *cl, sizebuf_t *msg )
{
	int	checksum1, checksum2;
	int	key, lastframe, net_drop;
	usercmd_t	oldest, oldcmd, newcmd, nulcmd;

	key = msg->readcount;
	checksum1 = MSG_ReadByte( msg );
	cl->packet_loss = MSG_ReadByte( msg );
	lastframe = MSG_ReadLong( msg );

	if( lastframe != cl->lastframe )
	{
		cl->lastframe = lastframe;
		if( cl->lastframe > 0 )
		{
			client_frame_t *frame = &cl->frames[cl->lastframe & UPDATE_MASK];
			frame->latency = svs.realtime - frame->senttime;
		}
	}

	cl->packet_loss = SV_CalcPacketLoss( cl );

	Mem_Set( &nulcmd, 0, sizeof( nulcmd ));
	MSG_ReadDeltaUsercmd( msg, &nulcmd, &oldest );
	MSG_ReadDeltaUsercmd( msg, &oldest, &oldcmd );
	MSG_ReadDeltaUsercmd( msg, &oldcmd, &newcmd );

	if( cl->state != cs_spawned )
	{
		cl->lastframe = -1;
		return;
	}

	// if the checksum fails, ignore the rest of the packet
	checksum2 = CRC_Sequence( msg->data + key + 1, msg->readcount - key - 1, cl->netchan.incoming_sequence );
	if( checksum2 != checksum1 )
	{
		MsgDev( D_ERROR, "SV_UserMove: failed command checksum for %s (%d != %d)\n", cl->name, checksum2, checksum1 );
		return;
	}

	if( !sv.paused )
	{
		SV_PreRunCmd( cl, &newcmd );	// get random_seed from newcmd

		net_drop = cl->netchan.dropped;

		if( net_drop < 20 )
		{
			while( net_drop > 2 )
			{
				SV_RunCmd( cl, &cl->lastcmd );
				net_drop--;
			}

			if( net_drop > 1 ) SV_RunCmd( cl, &oldest );
			if( net_drop > 0 ) SV_RunCmd( cl, &oldcmd );

		}
		SV_RunCmd( cl, &newcmd );
		SV_PostRunCmd( cl );
	}

	cl->lastcmd = newcmd;
	cl->lastcmd.buttons = 0; // avoid multiple fires on lag
}

/*
===================
SV_ExecuteClientMessage

Parse a client packet
===================
*/
void SV_ExecuteClientMessage( sv_client_t *cl, sizebuf_t *msg )
{
	int	c, stringCmdCount = 0;
	bool	move_issued = false;
	char	*s;

	// make sure the reply sequence number matches the incoming sequence number 
	if( cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence )
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;

	// read optional clientCommand strings
	while( cl->state != cs_zombie )
	{
		c = MSG_ReadByte( msg );
		if( c == -1 ) break;

		if( msg->error )
		{
			MsgDev( D_ERROR, "SV_ReadClientMessage: clc_bad\n" );
			SV_DropClient( cl );
			return;
		}	

		switch( c )
		{
		case clc_nop:
			break;
		case clc_userinfo:
			SV_UserinfoChanged( cl, MSG_ReadString( msg ));
			break;
		case clc_move:
			if( move_issued ) return; // someone is trying to cheat...
			move_issued = true;
			SV_ReadClientMove( cl, msg );
			break;
		case clc_stringcmd:	
			s = MSG_ReadString( msg );
			// malicious users may try using too many string commands
			if( ++stringCmdCount < 8 ) SV_ExecuteClientCommand( cl, s );
			if( cl->state == cs_zombie ) return; // disconnect command
			break;
		default:
			MsgDev( D_ERROR, "SV_ReadClientMessage: clc_bad\n" );
			SV_DropClient( cl );
			return;
		}
	}
}