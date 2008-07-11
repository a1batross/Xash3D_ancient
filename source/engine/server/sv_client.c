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
SVC_GetChallenge

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
SVC_DirectConnect

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
		MsgDev( D_ERROR, "SVC_DirectConnect: rejected connect from version %i\n", version );
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
		MsgDev( D_INFO, "SVC_DirectConnect: rejected a connection.\n");
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
		MsgDev( D_ERROR, "SVC_DirectConnect: game rejected a connection.\n");
		return;
	}

	// parse some info from the info strings
	com.strncpy( newcl->userinfo, userinfo, sizeof(newcl->userinfo) - 1);
	SV_UserinfoChanged( newcl );

	// send the connect packet to the client
	Netchan_OutOfBandPrint( NS_SERVER, from, "client_connect" );

	Netchan_Setup( NS_SERVER, &newcl->netchan, from, qport );
	SZ_Init( &newcl->datagram, newcl->datagram_buf, sizeof(newcl->datagram_buf));
	
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
	
	if( drop->state == cs_zombie )
		return;		// already dropped

	// add the disconnect
	MSG_WriteByte( &drop->netchan.message, svc_disconnect );

	// let the game known about client state
	prog->globals.sv->time = sv.time;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG( drop->edict );
	PRVM_ExecuteProgram( prog->globals.sv->ClientDisconnect, "QC function ClientDisconnect is missing\n" );
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
		PRVM_ExecuteProgram( prog->globals.sv->PutClientInServer, "QC function PutClientInServer is missing\n" );
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
	client->ps.cmd_time = ( sv.time * 1000 ) - 100;
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
	while( cl->netchan.message.cursize < MAX_MSGLEN / 2 && start < MAX_CONFIGSTRINGS )
	{
		if( sv.configstrings[start][0])
		{
			MSG_WriteByte( &cl->netchan.message, svc_configstring );
			MSG_WriteShort( &cl->netchan.message, start );
			MSG_WriteString( &cl->netchan.message, sv.configstrings[start] );
		}
		start++;
	}

	if( start == MAX_CONFIGSTRINGS ) com.snprintf( cs, MAX_STRING, "cmd baselines %i 0\n", svs.spawncount );
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
	if( com.atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		MsgDev( D_INFO, "baselines from different level\n" );
		SV_New_f( cl );
		return;
	}
	
	start = com.atoi(Cmd_Argv(2));

	memset( &nullstate, 0, sizeof(nullstate));

	// write a packet full of data
	while( cl->netchan.message.cursize < MAX_MSGLEN / 2 && start < MAX_EDICTS )
	{
		base = &sv.baselines[start];
		if( base->modelindex || base->soundindex || base->effects )
		{
			MSG_WriteByte( &cl->netchan.message, svc_spawnbaseline );
			MSG_WriteDeltaEntity( &nullstate, base, &cl->netchan.message, true, true );
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
	SZ_Write( &cl->netchan.message, cl->download + cl->downloadcount - r, r );
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
	// this is set in SVC_DirectConnect ( directly on the server, not transmitted ),
	// may be lost when client updates it's userinfo the banning code relies on this being consistently present
	if(!Info_ValueForKey (cl->userinfo, "ip"))
	{
		//Com_DPrintf("Maintain IP in userinfo for '%s'\n", cl->name);
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
	PRVM_ExecuteProgram( prog->globals.sv->ClientUserInfoChanged, "QC function ClientUserInfoChanged is missing\n" );
}

ucmd_t ucmds[] =
{
	{"new", SV_New_f},
	{"begin", SV_Begin_f},
	{"baselines", SV_Baselines_f},
	{"info", SV_ShowServerinfo_f},
	{"disconnect", SV_Disconnect_f},
	{"download", SV_BeginDownload_f},
	{"nextdl", SV_NextDownload_f},
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
		PRVM_ExecuteProgram( prog->globals.sv->ClientCommand, "QC function ClientCommand is missing\n" );
	}
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
	int	i, cmdcount, latency;
	int	checksumIndex, lastframe;
	int	checksum, calculatedChecksum;
	usercmd_t	cmds[PACKET_BACKUP];
	usercmd_t	*cmd, *oldcmd;
	usercmd_t	nullcmd;

	checksumIndex = msg->readcount;
	checksum = MSG_ReadByte( msg );
	lastframe = MSG_ReadLong( msg );
	if( lastframe != cl->lastframe )
	{
		cl->lastframe = lastframe;
		if( cl->lastframe > 0 )
		{
			latency = svs.realtime - cl->frames[cl->lastframe & UPDATE_MASK].msg_sent;
			cl->frame_latency[cl->lastframe&(LATENCY_COUNTS-1)] = latency;
		}
	}
	cmdcount = MSG_ReadByte( msg );

	// invalid command count
	if( cmdcount < 1 || cmdcount > PACKET_BACKUP )
		return;

	memset( &nullcmd, 0, sizeof(nullcmd) );
	oldcmd = &nullcmd;
	for( i = 0; i < cmdcount; i++ )
	{
		cmd = &cmds[i];
		MSG_ReadDeltaUsercmd( msg, oldcmd, cmd );
		oldcmd = cmd;
	}

	// save time for ping calculation
	if( cl->lastframe > 0 ) cl->frames[cl->lastframe & UPDATE_MASK].msg_acked = svs.realtime;
	if( cl->state != cs_spawned )
	{
		cl->lastframe = -1;
		return;
	}

	// if the checksum fails, ignore the rest of the packet
	calculatedChecksum = CRC_Sequence( msg->data + checksumIndex + 1, msg->readcount - checksumIndex - 1, cl->netchan.incoming_sequence );

	if( calculatedChecksum != checksum )
	{
		MsgDev( D_ERROR, "SV_UserMove: failed checksum for %s (%d != %d)\n", cl->name, calculatedChecksum, checksum );
		return;
	}

	// usually, the first couple commands will be duplicates
	// of ones we have previously received, but the servertimes
	// in the commands will cause them to be immediately discarded
	for( i =  0; i < cmdcount; i++ )
	{
		// if this is a cmd from before a map_restart ignore it
		if( cmds[i].servertime > cmds[cmdcount - 1].servertime )
			continue;
		if( cmds[i].servertime <= cl->lastcmd.servertime )
			continue;
		SV_ClientThink( cl, &cmds[i] );
	}
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
	
	// read optional clientCommand strings
	while( cl->state != cs_zombie )
	{
		c = MSG_ReadByte( msg );
		if( c == clc_eof ) break;
		if( c == -1 ) break;

		switch( c )
		{
		case clc_nop:
			break;
		case clc_userinfo:
			Msg("clc_userinfo: %s\n", cl->userinfo );
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