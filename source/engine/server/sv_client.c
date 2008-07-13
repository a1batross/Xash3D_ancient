//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_client.c - client interactions
//=======================================================================

#include "common.h"
#include "server.h"

typedef struct ucmd_s
{
	const char	*name;
	void		(*func)( sv_client_t *cl );
} ucmd_t;

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

	version = com.atoi(Cmd_Argv(1));
	if( version != PROTOCOL_VERSION )
	{
		Netchan_OutOfBandPrint( NS_SERVER, from, "print\nServer uses protocol version %i.\n", PROTOCOL_VERSION );
		MsgDev( D_ERROR, "SV_DirectConnect: rejected connect from version %i\n", version );
		return;
	}

	qport = com.atoi(Cmd_Argv(2));
	challenge = com.atoi(Cmd_Argv(3));
	com.strncpy( userinfo, Cmd_Argv( 4 ), sizeof(userinfo) - 1);
	userinfo[sizeof(userinfo) - 1] = 0;

	// quick reject
	for( i = 0, cl = svs.clients; i < maxclients->integer; i++, cl++ )
	{
		if( cl->state == cs_free ) continue;
		if( NET_CompareBaseAdr(from, cl->netchan.remote_address) && (cl->netchan.qport == qport || from.port == cl->netchan.remote_address.port))
		{
			if(( svs.realtime - cl->lastconnect ) < (sv_reconnect_limit->integer * 1000 ))
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
		Info_SetValueForKey( userinfo, "ip", "localhost" );
		Info_SetValueForKey( userinfo, "name", GI->username ); // can be overwrited later
	}

	newcl = &temp;
	memset( newcl, 0, sizeof(sv_client_t));

	// if there is already a slot for this ip, reuse it
	for( i = 0, cl = svs.clients; i < maxclients->integer; i++, cl++ )
	{
		if( cl->state == cs_free ) continue;
		if( NET_CompareBaseAdr(from, cl->netchan.remote_address) && (cl->netchan.qport == qport || from.port == cl->netchan.remote_address.port))
		{
			MsgDev( D_INFO, "%s:reconnect\n", NET_AdrToString( from ));
			newcl = cl;
			goto gotnewcl;
		}
	}

	// find a client slot
	newcl = NULL;
	for( i = 0, cl = svs.clients; i < maxclients->value; i++, cl++)
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

	ent = PRVM_EDICT_NUM( edictnum );
	ent->priv.sv->client = newcl;
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
	com.strncpy( newcl->userinfo, userinfo, sizeof(newcl->userinfo) - 1);
	SV_UserinfoChanged( newcl );

	// send the connect packet to the client
	Netchan_OutOfBandPrint( NS_SERVER, from, "client_connect" );

	Netchan_Setup( NS_SERVER, &newcl->netchan, from, qport );
	MSG_Init( &newcl->datagram, newcl->datagram_buf, sizeof(newcl->datagram_buf));
	
	newcl->state = cs_connected;
	newcl->lastmessage = svs.realtime;
	newcl->lastconnect = svs.realtime;

	// if this was the first client on the server, or the last client
	// the server can hold, send a heartbeat to the master.
	for( i = 0, cl = svs.clients; i < maxclients->integer; i++, cl++ )
		if( cl->state >= cs_connected ) count++;
	if( count == 1 || count == maxclients->integer )
		svs.last_heartbeat = MAX_HEARTBEAT;
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
	MSG_WriteByte( &drop->netchan.message, svc_disconnect );

	// let the game known about client state
	prog->globals.sv->time = sv.time;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG( drop->edict );
	PRVM_ExecuteProgram( prog->globals.sv->ClientDisconnect, "ClientDisconnect" );
	SV_FreeEdict( drop->edict );
	if( drop->download ) drop->download = NULL;

	drop->state = cs_zombie; // become free in a few seconds
	drop->name[0] = 0;

	// if this was the last client on the server, send a heartbeat
	// to the master so it is known the server is empty
	// send a heartbeat now so the master will get up to date info
	// if there is already a slot for this ip, reuse it
	for( i = 0; i < maxclients->integer; i++ )
	{
		if( svs.clients[i].state >= cs_connected )
			break;
	}
	if( i == maxclients->integer ) svs.last_heartbeat = MAX_HEARTBEAT;
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
	switch( dest )
	{
	case RD_PACKET:
		Netchan_OutOfBandPrint( NS_SERVER, adr, "print\n%s", buf );
		break;
	case RD_CLIENT:
		if( !sv_client ) return; // client not set
		MSG_WriteByte( &sv_client->netchan.message, svc_print );
		MSG_WriteByte( &sv_client->netchan.message, PRINT_CONSOLE );
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
	statusLength = strlen(status);

	for( i = 0; i < maxclients->value; i++ )
	{
		cl = &svs.clients[i];
		if( cl->state == cs_connected || cl->state == cs_spawned )
		{
			com.sprintf( player, "%i %i \"%s\"\n", cl->edict->priv.sv->client->ps.stats[STAT_FRAGS], cl->ping, cl->name );
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
	Msg( "Ping acknowledge from %s\n", NET_AdrToString( from ));
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
	if( maxclients->value == 1 )
		return;

	version = com.atoi(Cmd_Argv( 1 ));

	if( version != PROTOCOL_VERSION )
		com.sprintf( string, "%s: wrong version\n", hostname->string, sizeof(string));
	else
	{
		for( i = 0; i < maxclients->value; i++ )
			if( svs.clients[i].state >= cs_connected )
				count++;
		com.sprintf( string, "%16s %8s %2i/%2i\n", hostname->string, sv.name, count, (int)maxclients->value );
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
	if(!com.strlen( rcon_password->string ))
		return false;
	if(!com.strcmp( Cmd_Argv(1), rcon_password->string ))
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

	if(!Rcon_Validate()) MsgDev( D_WARN, "Bad rcon_password.\n" );
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
	int		i;
          
	index = PRVM_NUM_FOR_EDICT( ent ) - 1;
	client = ent->priv.sv->client;

	prog->globals.sv->time = sv.time;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG( ent );

	ent->priv.sv->free = false;
	(int)ent->progs.sv->flags &= ~FL_DEADMONSTER;

	if( !sv.loadgame )
	{	
		// fisrt entering
		PRVM_ExecuteProgram( prog->globals.sv->PutClientInServer, "PutClientInServer" );
		ent->progs.sv->v_angle[ROLL] = 0;	// cut off any camera rolling
		ent->progs.sv->origin[2] += 1;	// make sure off ground
		VectorCopy( ent->progs.sv->origin, ent->progs.sv->old_origin );
	}
 
	// setup playerstate
	memset( &client->ps, 0, sizeof(client->ps));

	client->ps.fov = 90;
	client->ps.fov = bound(1, client->ps.fov, 160);
	client->ps.vmodel.index = SV_ModelIndex(PRVM_GetString(ent->progs.sv->v_model));
	client->ps.pmodel.index = SV_ModelIndex(PRVM_GetString(ent->progs.sv->p_model));
	VectorScale( ent->progs.sv->origin, SV_COORD_FRAC, client->ps.origin );
	VectorCopy( ent->progs.sv->v_angle, client->ps.viewangles );
	for( i = 0; i < 3; i++ ) client->ps.delta_angles[i] = ANGLE2SHORT(ent->progs.sv->v_angle[i]);

	SV_LinkEdict( ent ); // m_pmatrix calculated here, so we need call this before pe->CreatePlayer
	ent->priv.sv->physbody = pe->CreatePlayer( ent->priv.sv, SV_GetModelPtr( ent ), ent->progs.sv->m_pmatrix );
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

	if(sv.state == ss_cinematic) playernum = -1;
	else playernum = sv_client - svs.clients;
	MSG_WriteShort( &cl->netchan.message, playernum );
	MSG_WriteString( &cl->netchan.message, sv.configstrings[CS_NAME] );

	// game server
	if( sv.state == ss_active )
	{
		// set up the entity for the client
		ent = PRVM_EDICT_NUM( playernum + 1 );
		ent->priv.sv->serialnumber = playernum + 1;
		cl->edict = ent;
		memset( &cl->lastcmd, 0, sizeof(cl->lastcmd));

		// begin fetching configstrings
		MSG_WriteByte( &cl->netchan.message, svc_stufftext );
		MSG_WriteString( &cl->netchan.message, va("cmd configstrings %i 0\n",svs.spawncount) );
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
	if( com.atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		MsgDev( D_INFO, "configstrings from different level\n" );
		SV_New_f( cl );
		return;
	}
	
	start = com.atoi(Cmd_Argv(2));

	// write a packet full of data
	while( cl->netchan.message.cursize < MAX_DATAGRAM/2 && start < MAX_CONFIGSTRINGS )
	{
		if( sv.configstrings[start][0])
		{
			MSG_WriteByte( &cl->netchan.message, svc_configstring );
			MSG_WriteShort( &cl->netchan.message, start );
			MSG_WriteString( &cl->netchan.message, sv.configstrings[start] );
		}
		start++;
	}
	if( start == MAX_CONFIGSTRINGS )com.snprintf( cs, MAX_STRING, "cmd baselines %i 0\n", svs.spawncount );
	else com.snprintf( cs, MAX_STRING, "cmd configstrings %i %i\n", svs.spawncount, start );

	Msg("SV->svc_stufftext: %s\n", cs );	 
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
	if( com.atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		MsgDev( D_INFO, "baselines from different level\n" );
		SV_New_f( cl );
		return;
	}
	
	start = com.atoi(Cmd_Argv(2));

	memset( &nullstate, 0, sizeof(nullstate));

	// write a packet full of data
	while( cl->netchan.message.cursize < MAX_DATAGRAM/2 && start < MAX_EDICTS )
	{
		base = &sv.baselines[start];
		if( base->modelindex || base->soundindex || base->effects )
		{
			MSG_WriteByte( &cl->netchan.message, svc_spawnbaseline );
			MSG_WriteDeltaEntity( &nullstate, base, &cl->netchan.message, true );
		}
		start++;
	}

	if( start == MAX_EDICTS ) com.snprintf( baseline, MAX_STRING, "precache %i\n", svs.spawncount );
	else com.snprintf( baseline, MAX_STRING, "cmd baselines %i %i\n",svs.spawncount, start );

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
	if( com.atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Msg( "begin from different level\n" );
		SV_New_f( cl );
		return;
	}
	cl->state = cs_spawned;
	SV_ClientBegin( cl->edict );
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
		if( cl->download ) cl->download = NULL;
		MSG_WriteByte( &cl->netchan.message, svc_download );
		MSG_WriteShort( &cl->netchan.message, -1 );
		MSG_WriteByte( &cl->netchan.message, 0 );
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
	Info_Print(Cvar_Serverinfo());
}

/*
=================
SV_UserinfoChanged

Pull specific info from a newly changed userinfo string
into a more C freindly form.
=================
*/
void SV_UserinfoChanged( sv_client_t *cl )
{
	char	*val;
	int	i;

	// name for C code (make colored string)
	com.snprintf( cl->name, sizeof(cl->name), "^2%s", Info_ValueForKey( cl->userinfo, "name"));

	// if the client is on the same subnet as the server and we aren't running an
	// internet public server, assume they don't need a rate choke
	if(NET_IsLANAddress( cl->netchan.remote_address ))
	{
		// lans should not rate limit
		cl->rate = 99999;
	}
	else
	{
		val = Info_ValueForKey( cl->userinfo, "rate" );
		if( com.strlen( val ))
		{
			i = com.atoi( val );
			cl->rate = bound( 1000, i, 90000 );
		}
		else cl->rate = 3000;
	}

	// msg command
	val = Info_ValueForKey( cl->userinfo, "msg" );
	if( com.strlen( val )) cl->messagelevel = com.atoi( val );

	// maintain the IP information
	// this is set in SV_DirectConnect ( directly on the server, not transmitted ),
	// may be lost when client updates it's userinfo the banning code relies on this being consistently present
	if(!Info_ValueForKey (cl->userinfo, "ip"))
	{
		if( !NET_IsLocalAddress(cl->netchan.remote_address ))
			Info_SetValueForKey(cl->userinfo, "ip", NET_AdrToString(cl->netchan.remote_address));
		else Info_SetValueForKey( cl->userinfo, "ip", "localhost" );
	}
}

/*
==================
SV_UpdateUserinfo_f
==================
*/
static void SV_UpdateUserinfo_f( sv_client_t *cl )
{
	com.strncpy( cl->userinfo, Cmd_Argv(1), sizeof(cl->userinfo));

	SV_UserinfoChanged( cl );

	// call prog code to allow overrides
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG( cl->edict );
	prog->globals.sv->time = sv.time;
	prog->globals.sv->frametime = sv.frametime;
	PRVM_G_INT(OFS_PARM0) = PRVM_SetEngineString( cl->userinfo );
	PRVM_ExecuteProgram( prog->globals.sv->ClientUserInfoChanged, "ClientUserInfoChanged" );
}

ucmd_t ucmds[] =
{
	{"new", SV_New_f},
	{"begin", SV_Begin_f},
	{"baselines", SV_Baselines_f},
	{"info", SV_ShowServerinfo_f},
	{"nextdl", SV_NextDownload_f},
	{"disconnect", SV_Disconnect_f},
	{"download", SV_BeginDownload_f},
	{"userinfo", SV_UpdateUserinfo_f},
	{"configstrings", SV_Configstrings_f},
	{NULL, NULL}
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
		if(!com.strcmp(Cmd_Argv(0), u->name))
		{
			Msg("ucmd->%s\n", u->name );
			u->func( cl );
			break;
		}
	}
	if( !u->name && sv.state == ss_active )
	{
		// custom client commands
		prog->globals.sv->pev = PRVM_EDICT_TO_PROG( cl->edict );
		prog->globals.sv->time = sv.time;
		prog->globals.sv->frametime = sv.frametime;
		PRVM_ExecuteProgram( prog->globals.sv->ClientCommand, "ClientCommand" );
	}
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
	MsgDev( D_INFO, "SV_ConnectionlessPacket: %s : %s\n", NET_AdrToString(from), c);

	if (!strcmp(c, "ping")) SV_Ping( from );
	else if (!strcmp(c, "ack")) SV_Ack( from );
	else if (!strcmp(c,"status")) SV_Status( from );
	else if (!strcmp(c,"info")) SV_Info( from );
	else if (!strcmp(c,"getchallenge")) SV_GetChallenge( from );
	else if (!strcmp(c,"connect")) SV_DirectConnect( from );
	else if (!strcmp(c, "rcon")) SV_RemoteCommand( from, msg );
	else MsgDev( D_ERROR, "bad connectionless packet from %s:\n%s\n", NET_AdrToString( from ), s );
}

/*
==================
SV_ClientThink

Also called by bot code
==================
*/
void SV_ClientThink( sv_client_t *cl, usercmd_t *cmd )
{
	cl->lastcmd = *cmd;

	// may have been kicked during the last usercmd
	if( sv_paused->integer ) return;
	ClientThink( cl->edict, cmd );
}

/*
==================
SV_UserMove

The message usually contains all the movement commands 
that were in the last three packets, so that the information
in dropped packets can be recovered.

On very fast clients, there may be multiple usercmd packed into
each of the backup packets.
==================
*/
static void SV_UserMove( sv_client_t *cl, sizebuf_t *msg )
{
	usercmd_t	nullcmd;
	usercmd_t	oldest, oldcmd, newcmd;
	int	latency, net_drop;
	int	checksumIndex, lastframe;
	int	checksum, calculatedChecksum;

	checksumIndex = msg->readcount;
	checksum = MSG_ReadByte( msg );
	lastframe = MSG_ReadLong( msg );
	if( lastframe != cl->lastframe )
	{
		cl->lastframe = lastframe;
		if (cl->lastframe > 0)
		{
			latency = svs.realtime - cl->frames[cl->lastframe & UPDATE_MASK].msg_sent;
			cl->frames[cl->lastframe & UPDATE_MASK].latency = latency;
		}
	}

	memset( &nullcmd, 0, sizeof(nullcmd));
	MSG_ReadDeltaUsercmd( msg, &nullcmd, &oldest);
	MSG_ReadDeltaUsercmd( msg, &oldest, &oldcmd );
	MSG_ReadDeltaUsercmd( msg, &oldcmd, &newcmd );

	if( cl->state != cs_spawned )
	{
		cl->lastframe = -1;
		return;
	}

	// if the checksum fails, ignore the rest of the packet
	//calculatedChecksum = CRC_Sequence( msg->data + checksumIndex + 1, msg->readcount - checksumIndex - 1, cl->netchan.incoming_sequence );
//FIXME
	/*if( calculatedChecksum != checksum )
	{
		MsgDev( D_ERROR, "SV_UserMove: failed command checksum for %s (%d != %d)\n", cl->name, calculatedChecksum, checksum );
		return;
	}*/

	if( !sv_paused->value )
	{
		net_drop = cl->netchan.dropped;
		if( net_drop < 20 )
		{
			while( net_drop > 2 )
			{
				SV_ClientThink( cl, &cl->lastcmd );
				net_drop--;
			}
			if( net_drop > 1 ) SV_ClientThink( cl, &oldest );
			if( net_drop > 0 ) SV_ClientThink( cl, &oldcmd );
		}
		SV_ClientThink( cl, &newcmd );
	}
	cl->lastcmd = newcmd;
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

	MSG_UseHuffman( msg, true );
	
	// read optional clientCommand strings
	while( cl->state != cs_zombie )
	{
		c = MSG_ReadByte( msg );
		if( c == -1 ) break;

		switch( c )
		{
		case clc_nop:
			break;
		case clc_userinfo:
			SV_UpdateUserinfo_f( cl );
			break;
		case clc_move:
			if( move_issued ) return; // someone is trying to cheat...
			move_issued = true;
			SV_UserMove( cl, msg );
			break;
		case clc_stringcmd:	
			s = MSG_ReadString( msg );
			// malicious users may try using too many string commands
			if( ++stringCmdCount < 8 ) SV_ExecuteClientCommand( cl, s );
			if( cl->state == cs_zombie ) return; // disconnect command
			break;
		default:
			MsgDev( D_ERROR, "SV_ReadClientMessage: unknown command char %d\n", c );
			SV_DropClient( cl );
			return;
		}
	}
}