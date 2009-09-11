/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cl_main.c  -- client main loop

#include "common.h"
#include "client.h"
#include "byteorder.h"

cvar_t	*freelook;
cvar_t	*rcon_client_password;
cvar_t	*rcon_address;

cvar_t	*cl_footsteps;
cvar_t	*cl_timeout;
cvar_t	*cl_predict;
cvar_t	*cl_showfps;
cvar_t	*cl_maxfps;

cvar_t	*cl_particles;
cvar_t	*cl_particlelod;

cvar_t	*cl_shownet;
cvar_t	*cl_showmiss;
cvar_t	*cl_showclamp;
cvar_t	*cl_mouselook;
cvar_t	*cl_paused;

cvar_t	*lookspring;
cvar_t	*lookstrafe;

cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;

cvar_t	*cl_lightlevel;

//
// userinfo
//
cvar_t	*info_password;
cvar_t	*info_spectator;
cvar_t	*name;
cvar_t	*rate;
cvar_t	*fov;

client_static_t	cls;
client_t		cl;
clgame_static_t	clgame;

entity_state_t	cl_parse_entities[MAX_PARSE_ENTITIES];

extern	cvar_t *allow_download;
//======================================================================

//======================================================================

/*
=======================================================================

CLIENT RELIABLE COMMAND COMMUNICATION

=======================================================================
*/
/*
===================
Cmd_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
/*
==================
CL_ForwardToServer_f
==================
*/
void CL_ForwardToServer_f( void )
{
	char	*cmd;

	if( cls.demoplayback || ( cls.state != ca_connected && cls.state != ca_active ))
		return; // not connected

	cmd = Cmd_Argv( 0 );
	if( *cmd == '-' || *cmd == '+' )
	{
		Msg( "Unknown command \"%s\"\n", cmd );
		return;
	}	

	// don't forward the first argument
	if( Cmd_Argc() > 1 )
	{
		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		MSG_Print( &cls.netchan.message, Cmd_Args());
	}
}


/*
==================
CL_Pause_f
==================
*/
void CL_Pause_f (void)
{
	if(!Host_ServerState() && !cls.demoplayback )
		return; // but allow pause in demos

	// never pause in multiplayer
	if(com.atoi(cl.configstrings[CS_MAXCLIENTS]) > 1 )
	{
		Cvar_SetValue( "paused", 0 );
		return;
	}
	Cvar_SetValue( "paused", !cl_paused->value );
}

/*
==================
CL_Quit_f
==================
*/
void CL_Quit_f (void)
{
	CL_Disconnect();
	Sys_Quit();
}

/*
================
CL_Drop

Called after an Host_Error was thrown
================
*/
void CL_Drop (void)
{
	if( cls.state == ca_uninitialized ) return;
	if( cls.state == ca_disconnected ) return;

	CL_Disconnect();
}

/*
=======================
CL_SendConnectPacket

We have gotten a challenge from the server, so try and
connect.
======================
*/
void CL_SendConnectPacket (void)
{
	netadr_t	adr;
	int		port;

	if(!NET_StringToAdr (cls.servername, &adr))
	{
		MsgDev( D_INFO, "CL_SendConnectPacket: bad server address\n");
		cls.connect_time = 0;
		return;
	}
	if( adr.port == 0 ) adr.port = BigShort( PORT_SERVER );
	port = Cvar_VariableValue( "net_qport" );

	userinfo_modified = false;
	Netchan_OutOfBandPrint(NS_CLIENT, adr, "connect %i %i %i \"%s\"\n", PROTOCOL_VERSION, port, cls.challenge, Cvar_Userinfo());
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend (void)
{
	netadr_t	adr;

	// if the local server is running and we aren't
	// then connect
	if (cls.state == ca_disconnected && Host_ServerState())
	{
		cls.state = ca_connecting;
		com.strncpy (cls.servername, "localhost", sizeof(cls.servername)-1);
		// we don't need a challenge on the localhost
		CL_SendConnectPacket ();
		return;
	}

	// resend if we haven't gotten a reply yet
	if( cls.demoplayback || cls.state != ca_connecting )
		return;
	if(( host.realtime - cls.connect_time ) < 3.0f ) return;

	if( !NET_StringToAdr( cls.servername, &adr ))
	{
		MsgDev(D_INFO, "CL_CheckForResend: bad server address\n");
		cls.state = ca_disconnected;
		return;
	}
	if( adr.port == 0 ) adr.port = BigShort( PORT_SERVER );
	cls.connect_time = host.realtime; // for retransmit requests
	Msg( "Connecting to %s...\n", cls.servername );
	Netchan_OutOfBandPrint( NS_CLIENT, adr, "getchallenge\n" );
}


/*
================
CL_Connect_f

================
*/
void CL_Connect_f (void)
{
	char	*server;

	if (Cmd_Argc() != 2)
	{
		Msg ("usage: connect <server>\n");
		return;	
	}
	
	if(Host_ServerState())
	{	
		// if running a local server, kill it and reissue
		com.strncpy( host.finalmsg, "Server quit\n", MAX_STRING );
		SV_Shutdown( false );
	}
	else CL_Disconnect();

	server = Cmd_Argv (1);

	CL_Disconnect();

	cls.state = ca_connecting;
	com.strncpy (cls.servername, server, sizeof(cls.servername)-1);
	cls.connect_time = MAX_HEARTBEAT; // CL_CheckForResend() will fire immediately
}


/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
void CL_Rcon_f (void)
{
	char	message[1024];
	int		i;
	netadr_t	to;

	if (!rcon_client_password->string)
	{
		Msg ("You must set 'rcon_password' before\n"
					"issuing an rcon command.\n");
		return;
	}

	message[0] = (char)255;
	message[1] = (char)255;
	message[2] = (char)255;
	message[3] = (char)255;
	message[4] = 0;

	com.strcat (message, "rcon ");

	com.strcat (message, rcon_client_password->string);
	com.strcat (message, " ");

	for (i=1 ; i<Cmd_Argc() ; i++)
	{
		com.strcat (message, Cmd_Argv(i));
		com.strcat (message, " ");
	}

	if (cls.state >= ca_connected)
		to = cls.netchan.remote_address;
	else
	{
		if (!com.strlen(rcon_address->string))
		{
			Msg ("You must either be connected,\n"
						"or set the 'rcon_address' cvar\n"
						"to issue rcon commands\n");

			return;
		}
		NET_StringToAdr (rcon_address->string, &to);
		if (to.port == 0)
			to.port = BigShort (PORT_SERVER);
	}
	
	NET_SendPacket (NS_CLIENT, com.strlen(message)+1, message, to);
}


/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState( void )
{
	S_StopAllSounds ();
	CL_ClearEffects ();
	CL_FreeEdicts();

	if( clgame.hInstance ) clgame.dllFuncs.pfnReset();

	// wipe the entire cl structure
	Mem_Set( &cl, 0, sizeof( cl ));
	MSG_Clear( &cls.netchan.message );

	Cvar_SetValue( "scr_download", 0.0f );
	Cvar_SetValue( "scr_loading", 0.0f );

	UI_SetActiveMenu( UI_CLOSEMENU );
}

/*
=====================
CL_Disconnect

Goes from a connected state to full screen console state
Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect( void )
{
	byte	final[32];

	if( cls.state == ca_disconnected )
		return;

	cls.connect_time = 0;
	SCR_StopCinematic();

	CL_Stop_f();

	// send a disconnect message to the server
	final[0] = clc_stringcmd;
	com.strcpy ((char *)final+1, "disconnect");
	Netchan_Transmit(&cls.netchan, com.strlen(final), final);
	Netchan_Transmit(&cls.netchan, com.strlen(final), final);
	Netchan_Transmit(&cls.netchan, com.strlen(final), final);

	CL_ClearState ();

	// stop download
	if (cls.download)
	{
		FS_Close(cls.download);
		cls.download = NULL;
	}

	cls.state = ca_disconnected;
}

void CL_Disconnect_f( void )
{
	Host_Error( "Disconnected from server\n" );
}


/*
====================
CL_Packet_f

packet <destination> <contents>

Contents allows \n escape character
====================
*/
void CL_Packet_f (void)
{
	char	send[2048];
	int		i, l;
	char	*in, *out;
	netadr_t	adr;

	if (Cmd_Argc() != 3)
	{
		Msg ("packet <destination> <contents>\n");
		return;
	}

	if (!NET_StringToAdr (Cmd_Argv(1), &adr))
	{
		Msg ("Bad address\n");
		return;
	}
	if (!adr.port)
		adr.port = BigShort (PORT_SERVER);

	in = Cmd_Argv(2);
	out = send+4;
	send[0] = send[1] = send[2] = send[3] = (char)0xff;

	l = com.strlen (in);
	for (i=0 ; i<l ; i++)
	{
		if (in[i] == '\\' && in[i+1] == 'n')
		{
			*out++ = '\n';
			i++;
		}
		else
			*out++ = in[i];
	}
	*out = 0;

	NET_SendPacket (NS_CLIENT, out-send, send, adr);
}

/*
=================
CL_Changing_f

Just sent as a hint to the client that they should
drop to full console
=================
*/
void CL_Changing_f( void )
{
	//ZOID
	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if( cls.download ) return;

	S_StopAllSounds();
	cls.state = ca_connected;	// not active anymore, but not disconnected
	Msg("\nChanging map...\n");
}


/*
=================
CL_Reconnect_f

The server is changing levels
=================
*/
void CL_Reconnect_f (void)
{
	//ZOID
	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if( cls.download ) return;

	S_StopAllSounds ();
	if( cls.state == ca_connected )
	{
		Msg ("reconnecting...\n");
		cls.state = ca_connected;
		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		MSG_Print( &cls.netchan.message, "new" );
		return;
	}

	if( *cls.servername )
	{
		if( cls.state >= ca_connected )
		{
			CL_Disconnect();
			cls.connect_time = host.realtime - 1.5;
		}
		else cls.connect_time = MAX_HEARTBEAT; // fire immediately

		cls.state = ca_connecting;
		Msg ("reconnecting...\n");
	}
}

/*
===================
CL_StatusLocal_f
===================
*/
void CL_GetServerList_f( void )
{
	netadr_t	adr;

	// send a broadcast packet
	MsgDev( D_INFO, "status pinging broadcast...\n" );
	cls.pingtime = host.realtime;

	adr.type = NA_BROADCAST;
	adr.port = BigShort( PORT_SERVER );
	Netchan_OutOfBandPrint( NS_CLIENT, adr, "status" );
}

/*
=============
CL_FreeServerInfo
=============
*/
static void CL_FreeServerInfo( serverinfo_t *server )
{
	if( server->mapname ) Mem_Free( server->mapname );
	if( server->hostname ) Mem_Free( server->hostname );
	if( server->shortname ) Mem_Free( server->shortname );
	if( server->gamename ) Mem_Free( server->gamename );
	if( server->netaddress ) Mem_Free( server->netaddress );
	if( server->playerstr ) Mem_Free( server->playerstr );
	if( server->pingstring ) Mem_Free( server->pingstring );
	memset( server, 0, sizeof(serverinfo_t));
}

/*
=============
CL_FreeServerList
=============
*/
static void CL_FreeServerList_f( void )
{
	int		i;

	for( i = 0; i < cls.numservers; i++ )
		CL_FreeServerInfo( &cls.serverlist[i] );
	cls.numservers = 0;
}

/*
=============
CL_DupeCheckServerList

Checks for duplicates and returns true if there is one...
Since status has higher priority than info, if there is already an instance and
it's not status, and the current one is status, the old one is removed.
=============
*/
static bool CL_DupeCheckServerList( char *adr, bool status )
{
	int	i;

	for( i = 0; i < cls.numservers; i++ )
	{
		if(!cls.serverlist[i].netaddress && !cls.serverlist[i].hostname )
		{
			CL_FreeServerInfo( &cls.serverlist[i] );
			continue;
		}
		if( cls.serverlist[i].netaddress && !com.strcmp( cls.serverlist[i].netaddress, adr ))
		{
			if( cls.serverlist[i].statusPacket && status )
			{
				return true;
			}
			else if( status )
			{
				CL_FreeServerInfo( &cls.serverlist[i] );
				return false;
			}
		}
	}
	return false;
}

/*
=============
CL_ParseServerStatus

Parses a status packet from a server
FIXME: check against a list of sent status requests so it's not attempting to parse things it shouldn't
=============
*/
bool CL_ParseServerStatus( char *adr, char *info )
{
	serverinfo_t	*server;
	char		*token;
	char		shortName[32];

	if( !info || !info[0] )return false;
	if( !adr || !adr[0] ) return false;
	if( !com.strchr( info, '\\')) return false;

	if( cls.numservers >= MAX_SERVERS )
		return true;
	if( CL_DupeCheckServerList( adr, true ))
		return true;

	server = &cls.serverlist[cls.numservers];
	CL_FreeServerInfo( server );
	cls.numservers++;

	// add net address
	server->netaddress = copystring( adr );
	server->mapname = copystring(Info_ValueForKey( info, "mapname"));
	server->maxplayers = com.atoi(Info_ValueForKey( info, "maxclients"));
	server->gamename = copystring(Info_ValueForKey( info, "gamename"));
	server->hostname = copystring(Info_ValueForKey( info, "hostname"));
	if( server->hostname )
	{
		com.strncpy( shortName, server->hostname, sizeof(shortName));
		server->shortname = copystring( shortName );
	}

	// Check the player count
	server->numplayers = com.atoi(Info_ValueForKey( info, "curplayers"));
	if( server->numplayers <= 0 )
	{
		server->numplayers = 0;

		token = strtok( info, "\n" );
		if( token )
		{
			token = strtok( NULL, "\n" );
			while( token )
			{
				server->numplayers++;
				token = strtok( NULL, "\n" );
			}
		}
	}

	// check if it's valid
	if( !server->mapname[0] && !server->maxplayers && !server->gamename[0] && !server->hostname[0] )
	{
		CL_FreeServerInfo( server );
		return false;
	}

	server->playerstr = copystring( va("%i/%i", server->numplayers, server->maxplayers ));

	// add the ping
	server->ping = host.realtime - cls.pingtime;
	server->pingstring = copystring( va( "%ims", server->ping ));
	server->statusPacket = true;

	// print information
	MsgDev( D_NOTE, "%s %s ", server->hostname, server->mapname );
	MsgDev( D_NOTE, "%i/%i %ims\n", server->numplayers, server->maxplayers, server->ping );

	return true;
}

/*
=================
CL_ParseStatusMessage

Handle a reply from a ping
=================
*/
void CL_ParseStatusMessage( netadr_t from, sizebuf_t *msg )
{
	char	*s;

	s = MSG_ReadString( msg );

	Msg ("%s\n", s);
	CL_ParseServerStatus( NET_AdrToString(from), s );
}

//===================================================================

/*
======================
CL_PrepSound

Call before entering a new level, or after changing dlls
======================
*/
void CL_PrepSound( void )
{
	int	i, sndcount;
		
	for( i = 0, sndcount = 0; i < MAX_SOUNDS && cl.configstrings[CS_SOUNDS+i+1][0]; i++ )
		sndcount++; // total num sounds

	S_BeginRegistration();
	for( i = 0; i < MAX_SOUNDS && cl.configstrings[CS_SOUNDS+1+i][0]; i++ )
	{
		cl.sound_precache[i+1] = S_RegisterSound( cl.configstrings[CS_SOUNDS+1+i] );
		Cvar_SetValue( "scr_loading", scr_loading->value + 5.0f / sndcount );
		SCR_UpdateScreen();
	}

	S_EndRegistration();
	CL_RunBackgroundTrack();

	cl.audio_prepped = true;
}

/*
=================
CL_PrepVideo

Call before entering a new level, or after changing dlls
=================
*/
void CL_PrepVideo( void )
{
	string		name, mapname;
	int		i, mdlcount;

	if( !cl.configstrings[CS_MODELS+1][0] )
		return; // no map loaded

	Cvar_SetValue( "scr_loading", 0.0f ); // reset progress bar
	Msg( "CL_PrepRefresh: %s\n", cl.configstrings[CS_NAME] );
	// let the render dll load the map
	FS_FileBase( cl.configstrings[CS_MODELS+1], mapname ); 
	re->BeginRegistration( mapname, pe->VisData()); // load map
	SCR_RegisterShaders(); // update with new sequence
	SCR_UpdateScreen();

	for( i = 0, mdlcount = 0; i < MAX_MODELS && cl.configstrings[CS_MODELS+1+i][0]; i++ )
		mdlcount++; // total num models

	for( i = 0; i < MAX_MODELS && cl.configstrings[CS_MODELS+1+i][0]; i++ )
	{
		com.strncpy( name, cl.configstrings[CS_MODELS+1+i], MAX_STRING );
		re->RegisterModel( name, i+1 );
		cl.models[i+1] = pe->RegisterModel( name );
		Cvar_SetValue("scr_loading", scr_loading->value + 45.0f / mdlcount );
		SCR_UpdateScreen();
	}

	for( i = 0; i < MAX_DECALS && cl.configstrings[CS_DECALS+1+i][0]; i++ )
	{
		com.strncpy( name, cl.configstrings[CS_DECALS+1+i], MAX_STRING );
		cl.decal_shaders[i+1] = re->RegisterShader( name, SHADER_GENERIC );
	}

	// setup sky and free unneeded stuff
	re->EndRegistration( cl.configstrings[CS_SKYNAME] );
	Cvar_SetValue( "scr_loading", 100.0f );	// all done
	
	Con_ClearNotify();			// clear any lines of console text
	SCR_UpdateScreen();
	cl.video_prepped = true;
}

/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
void CL_ConnectionlessPacket( netadr_t from, sizebuf_t *msg )
{
	char	*s, *c;
	
	MSG_BeginReading( msg );
	MSG_ReadLong( msg ); // skip the -1

	s = MSG_ReadStringLine( msg );

	Cmd_TokenizeString( s );
	c = Cmd_Argv(0);

	MsgDev( D_INFO, "CL_ConnectionlessPacket: %s : %s\n", NET_AdrToString( from ), c );

	// server connection
	if( !com.strcmp( c, "client_connect" ))
	{
		if( cls.state == ca_connected )
		{
			Msg ("Dup connect received.  Ignored.\n");
			return;
		}
		Netchan_Setup( NS_CLIENT, &cls.netchan, from, Cvar_VariableValue( "net_qport" ));
		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		MSG_Print( &cls.netchan.message, "new" );
		cls.state = ca_connected;
		UI_SetActiveMenu( UI_CLOSEMENU );
		return;
	}

	// server responding to a status broadcast
	if( !com.strcmp( c, "info" ))
	{
		CL_ParseStatusMessage( from, msg );
		return;
	}

	// remote command from gui front end
	if( !com.strcmp( c, "cmd" ))
	{
		if(!NET_IsLocalAddress( from ))
		{
			Msg( "Command packet from remote host.  Ignored.\n" );
			return;
		}
		ShowWindow( host.hWnd, SW_RESTORE );
		SetForegroundWindow ( host.hWnd );
		s = MSG_ReadString( msg );
		Cbuf_AddText(s);
		Cbuf_AddText("\n");
		return;
	}
	// print command from somewhere
	if( !com.strcmp( c, "print" ))
	{
		// print command from somewhere
		s = MSG_ReadString( msg );
		if(!CL_ParseServerStatus( NET_AdrToString( from ), s ))
			Msg( s );
		return;
	}

	// ping from somewhere
	if( !com.strcmp(c, "ping" ))
	{
		Netchan_OutOfBandPrint( NS_CLIENT, from, "ack" );
		return;
	}

	// challenge from the server we are connecting to
	if( !com.strcmp( c, "challenge" ))
	{
		cls.challenge = com.atoi(Cmd_Argv(1));
		CL_SendConnectPacket();
		return;
	}

	// echo request from server
	if( !com.strcmp( c, "echo" ))
	{
		Netchan_OutOfBandPrint( NS_CLIENT, from, "%s", Cmd_Argv(1) );
		return;
	}

	// a disconnect message from the server, which will happen if the server
	// dropped the connection but it is still getting packets from us
	if( !com.strcmp( c, "disconnect" ))
	{
		CL_Disconnect();
		return;
	}

	Msg( "Unknown command.\n" );
}

/*
=================
CL_ReadNetMessage
=================
*/
void CL_ReadNetMessage( void )
{
	while( NET_GetPacket( NS_CLIENT, &net_from, &net_message ))
	{
		if( host.type == HOST_DEDICATED || cls.demoplayback )
			return;

		if( net_message.cursize >= 4 && *(int *)net_message.data == -1 )
		{
			CL_ConnectionlessPacket( net_from, &net_message );
			return;
		}

		// can't be a valid sequenced packet	
		if( cls.state < ca_connected ) return;

		if( net_message.cursize < 8 )
		{
			MsgDev( D_WARN, "%s: runt packet\n", NET_AdrToString( net_from ));
			return;
		}

		// packet from server
		if( !NET_CompareAdr( net_from, cls.netchan.remote_address ))
		{
			MsgDev( D_WARN, "CL_ReadPackets: %s:sequenced packet without connection\n", NET_AdrToString( net_from ));
			return;
		}

		if( Netchan_Process( &cls.netchan, &net_message ))
		{
			// the header is different lengths for reliable and unreliable messages
			int headerBytes = net_message.readcount;

			CL_ParseServerMessage( &net_message );

			// we don't know if it is ok to save a demo message until
			// after we have parsed the frame
			if( cls.demorecording && !cls.demowaiting )
				CL_WriteDemoMessage( &net_message, headerBytes );
		}
	}
}

void CL_ReadPackets( void )
{
	if( cls.demoplayback )
		CL_ReadDemoMessage();
	else CL_ReadNetMessage();

	// singleplayer never has connection timeout
	if( NET_IsLocalAddress( cls.netchan.remote_address ))
		return;
          
	// check timeout
	if( cls.state >= ca_connected && !cls.demoplayback )
	{
		if( host.realtime - cls.netchan.last_received > cl_timeout->value )
		{
			if( ++cl.timeoutcount > 5 ) // timeoutcount saves debugger
			{
				Msg ("\nServer connection timed out.\n");
				CL_Disconnect();
				return;
			}
		}
	}
	else cl.timeoutcount = 0;
	
}


//=============================================================================
/*
==============
CL_Userinfo_f
==============
*/
void CL_Userinfo_f (void)
{
	Msg( "User info settings:\n" );
	Info_Print( Cvar_Userinfo());
}

int precache_check; // for autodownload of precache items
int precache_spawncount;
int precache_tex;

// ENV_CNT is map load, ENV_CNT+1 is first cubemap
#define ENV_CNT		MAX_CONFIGSTRINGS
#define TEXTURE_CNT		(ENV_CNT+13)

static const char *env_suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

void CL_RequestNextDownload( void )
{
	string		fn;
	uint		map_checksum;	// for detecting cheater maps

	if( cls.state != ca_connected )
		return;

	if( !allow_download->value && precache_check < ENV_CNT )
		precache_check = ENV_CNT;

	if( precache_check == CS_MODELS )
	{
		// confirm map
		precache_check = CS_MODELS+2; // 0 isn't used
		if(!CL_CheckOrDownloadFile( cl.configstrings[CS_MODELS+1] ))
			return; // started a download map
	}
	if( precache_check >= CS_MODELS && precache_check < CS_MODELS+MAX_MODELS )
	{
		while( precache_check < CS_MODELS+MAX_MODELS && cl.configstrings[precache_check][0])
		{
			com.sprintf( fn, "%s", cl.configstrings[precache_check++]);
			if(!CL_CheckOrDownloadFile( fn )) return; // started a download
		}
		precache_check = CS_SOUNDS;
	}
	if( precache_check >= CS_SOUNDS && precache_check < CS_SOUNDS+MAX_SOUNDS )
	{ 
		if( precache_check == CS_SOUNDS ) precache_check++; // zero is blank
		while( precache_check < CS_SOUNDS+MAX_SOUNDS && cl.configstrings[precache_check][0])
		{
			// sound pathes from model events
			if( cl.configstrings[precache_check][0] == '*' )
			{
				precache_check++;
				continue;
			}
			com.sprintf( fn, "sound/%s", cl.configstrings[precache_check++]);
			if(!CL_CheckOrDownloadFile( fn )) return; // started a download
		}
		precache_check = ENV_CNT;
	}
	if( precache_check == ENV_CNT )
	{
		precache_check = ENV_CNT + 1;

		cl.worldmodel = pe->BeginRegistration( cl.configstrings[CS_MODELS+1], true, &map_checksum );
		if( map_checksum != com.atoi(cl.configstrings[CS_MAPCHECKSUM]))
		{
			Host_Error("Local map version differs from server: %i != '%s'\n", map_checksum, cl.configstrings[CS_MAPCHECKSUM]);
			return;
		}
	}
	if( precache_check > ENV_CNT && precache_check < TEXTURE_CNT )
	{
		if( allow_download->value )
		{
			while( precache_check < TEXTURE_CNT )
			{
				int n = precache_check++ - ENV_CNT - 1;
				if( n & 1 ) com.sprintf( fn, "env/%s.dds", cl.configstrings[CS_SKYNAME] ); // cubemap pack
				else com.sprintf( fn, "env/%s%s.tga", cl.configstrings[CS_SKYNAME], env_suf[n/2] );
				if(!CL_CheckOrDownloadFile( fn )) return; // started a download
			}
		}
		precache_check = TEXTURE_CNT;
	}

	if( precache_check == TEXTURE_CNT )
	{
		precache_check = TEXTURE_CNT+1;
		precache_tex = 0;
	}

	// confirm existance of textures, download any that don't exist
	if( precache_check == TEXTURE_CNT + 1 )
	{
		if( allow_download->value )
		{
			while( precache_tex < pe->NumTextures())
			{
				com.sprintf( fn, "textures/%s.tga", pe->GetTextureName( precache_tex++ ));
				if( !CL_CheckOrDownloadFile( fn )) return; // started a download
			}
		}
		precache_check = TEXTURE_CNT + 999;
	}

	CL_PrepSound();
	CL_PrepVideo();
	CL_SortUserMessages();

	if( cls.demoplayback ) return; // not really connected
	MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
	MSG_Print( &cls.netchan.message, va("begin %i\n", precache_spawncount ));
}

/*
=================
CL_Precache_f

The server will send this command right
before allowing the client into the server
=================
*/
void CL_Precache_f( void )
{
	precache_check = CS_MODELS;
	precache_spawncount = com.atoi(Cmd_Argv(1));
	CL_RequestNextDownload();
}


/*
=================
CL_InitLocal
=================
*/
void CL_InitLocal( void )
{
	cls.state = ca_disconnected;

	CL_InitInput();

	// register our variables
	cl_footsteps = Cvar_Get ("cl_footsteps", "1", 0, "disables player footsteps" );
	cl_predict = Cvar_Get ("cl_predict", "1", CVAR_ARCHIVE, "disables client movement prediction" );
	cl_maxfps = Cvar_Get ("cl_maxfps", "1000", 0, "maximum client fps" );
	cl_particles = Cvar_Get( "cl_particles", "1", CVAR_ARCHIVE, "disables particle effects" );
	cl_particlelod = Cvar_Get( "cl_lod_particle", "0", CVAR_ARCHIVE, "enables particle LOD (1, 2, 3)" );

	cl_upspeed = Cvar_Get ("cl_upspeed", "200", 0, "client upspeed limit" );
	cl_forwardspeed = Cvar_Get ("cl_forwardspeed", "200", 0, "client forward speed limit" );
	cl_sidespeed = Cvar_Get ("cl_sidespeed", "200", 0, "client side-speed limit" );
	cl_yawspeed = Cvar_Get ("cl_yawspeed", "140", 0, "client yaw speed" );
	cl_pitchspeed = Cvar_Get ("cl_pitchspeed", "150", 0, "client pitch speed" );
	cl_anglespeedkey = Cvar_Get ("cl_anglespeedkey", "1.5", 0, "client anglespeed" );

	cl_run = Cvar_Get ("cl_run", "0", CVAR_ARCHIVE, "keep client for always run mode" );
	cl_mouselook = Cvar_Get( "cl_mouselook", "1", CVAR_ARCHIVE, "enables mouse look" );
	lookspring = Cvar_Get ("lookspring", "0", CVAR_ARCHIVE, "allow look spring" );
	lookstrafe = Cvar_Get ("lookstrafe", "0", CVAR_ARCHIVE, "allow look strafe" );

	m_pitch = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE, "mouse pitch value" );
	m_yaw = Cvar_Get ("m_yaw", "0.022", 0, "mouse yaw value" );
	m_forward = Cvar_Get ("m_forward", "1", 0, "mouse forward speed" );
	m_side = Cvar_Get ("m_side", "1", 0, "mouse side speed" );

	cl_shownet = Cvar_Get ("cl_shownet", "0", 0, "client show network packets" );
	cl_showmiss = Cvar_Get ("cl_showmiss", "0", 0, "client show network errors" );
	cl_showclamp = Cvar_Get ("cl_showclamp", "0", CVAR_ARCHIVE, "show client clamping" );
	cl_timeout = Cvar_Get ("cl_timeout", "120", 0, "connect timeout (in-seconds)" );

	rcon_client_password = Cvar_Get ("rcon_password", "", 0, "remote control client password" );
	rcon_address = Cvar_Get ("rcon_address", "", 0, "remote control address" );

	cl_lightlevel = Cvar_Get ("r_lightlevel", "0", 0, "no description" );

	//
	// userinfo
	//
	info_password = Cvar_Get ("password", "", CVAR_USERINFO, "player password" );
	info_spectator = Cvar_Get ("spectator", "0", CVAR_USERINFO, "spectator mode" );
	name = Cvar_Get ("name", "unnamed", CVAR_USERINFO | CVAR_ARCHIVE, "player name" );
	rate = Cvar_Get ("rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE, "player network rate" );	// FIXME
	fov = Cvar_Get ("fov", "90", CVAR_USERINFO | CVAR_ARCHIVE, "client fov" );
	cl_showfps = Cvar_Get ("cl_showfps", "1", CVAR_ARCHIVE, "show client fps" );

	// register our commands
	Cmd_AddCommand ("cmd", CL_ForwardToServer_f, "send a console commandline to the server" );
	Cmd_AddCommand ("pause", CL_Pause_f, "pause the game (if the server allows pausing)" );
	Cmd_AddCommand ("getserverlist", CL_GetServerList_f, "get info about local servers" );
	Cmd_AddCommand ("freeserverlist", CL_FreeServerList_f, "clear info about local servers" );

	Cmd_AddCommand ("userinfo", CL_Userinfo_f, "print current client userinfo" );
	Cmd_AddCommand ("changing", CL_Changing_f, "sent by server to tell client to wait for level change" );
	Cmd_AddCommand ("disconnect", CL_Disconnect_f, "disconnect from server" );
	Cmd_AddCommand ("record", CL_Record_f, "record a demo" );
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f, "playing a demo" );
	Cmd_AddCommand ("movie", CL_PlayVideo_f, "playing a movie" );
	Cmd_AddCommand ("stop", CL_Stop_f, "stop playing or recording a demo" );

	Cmd_AddCommand ("quit", CL_Quit_f, "quit from game" );
	Cmd_AddCommand ("exit", CL_Quit_f, "quit from game" );

	Cmd_AddCommand ("screenshot", CL_ScreenShot_f, "takes a screenshot of the next rendered frame" );
	Cmd_AddCommand ("envshot", CL_EnvShot_f, "takes a six-sides cubemap shot with specified name" );
	Cmd_AddCommand ("skyshot", CL_SkyShot_f, "takes a six-sides envmap (skybox) shot with specified name" );
	Cmd_AddCommand ("levelshot", CL_LevelShot_f, "same as \"screenshot\", used for create plaque images" );
	Cmd_AddCommand ("saveshot", CL_SaveShot_f, "used for create save previews with LoadGame menu" );

	Cmd_AddCommand ("connect", CL_Connect_f, "connect to a server by hostname" );
	Cmd_AddCommand ("reconnect", CL_Reconnect_f, "reconnect to current level" );

	Cmd_AddCommand ("rcon", CL_Rcon_f, "sends a command to the server console (rcon_password and rcon_address required)" );

	// this is dangerous to leave in
// 	Cmd_AddCommand ("packet", CL_Packet_f, "send a packet with custom contents" );

	Cmd_AddCommand ("precache", CL_Precache_f, "precache specified resource (by index)" );
	Cmd_AddCommand ("download", CL_Download_f, "download specified resource (by name)" );

	CL_InitServerCommands ();

	UI_SetActiveMenu( UI_MAINMENU );
}

//============================================================================

/*
==================
CL_SendCommand

==================
*/
void CL_SendCommand( void )
{
	// send intentions now
	CL_SendCmd ();

	// resend a connection request if necessary
	CL_CheckForResend ();
}

/*
==================
CL_Frame

==================
*/
void CL_Frame( double time )
{
	if( host.type == HOST_DEDICATED )
		return;

	// decide the simulation time
	cl.oldtime = cl.time;
	cl.time += time;		// can be merged by cl.frame.servertime 
	cls.realtime += time;
	cls.frametime = time;

	if( cls.frametime > (1.0f / 5)) cls.frametime = (1.0f / 5);

	// if in the debugger last frame, don't timeout
	if( time > 5.0f ) cls.netchan.last_received = Sys_DoubleTime ();

	// fetch results from server
	CL_ReadPackets();

	// send a new command message to the server
	CL_SendCommand();

	// predict all unacknowledged movements
	CL_PredictMovement ();

	// allow rendering DLL change
	if( cls.state == ca_active )
	{
		if( !cl.video_prepped ) CL_PrepVideo();
		if( !cl.audio_prepped ) CL_PrepSound();
	}

	// update the screen
	SCR_UpdateScreen();

	// update audio
	S_Update( cl.playernum + 1, cl.refdef.vieworg, vec3_origin, cl.refdef.forward, cl.refdef.up );

	// advance local effects for next frame
	CL_RunDLights ();
	CL_RunLightStyles ();

	SCR_RunCinematic();
	Con_RunConsole();

	cls.framecount++;
}


//============================================================================

/*
====================
CL_Init
====================
*/
void CL_Init( void )
{
	if( host.type == HOST_DEDICATED )
		return; // nothing running on the client

	// all archived variables will now be loaded
	cl_paused = Cvar_Get( "paused", "0", 0, "game paused" );

	Con_Init();	
	VID_Init();

	if( !CL_LoadProgs( "client" ))
		Host_Error( "CL_InitGame: can't initialize client.dll\n" );

	MSG_Init( &net_message, net_message_buffer, sizeof( net_message_buffer ));

	UI_Init();
	SCR_Init();
	CL_InitLocal();
	cls.initialized = true;
}


/*
===============
CL_Shutdown

FIXME: this is a callback from Sys_Quit and Host_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void CL_Shutdown( void )
{
	// already freed
	if( host.state == HOST_ERROR ) return;
	if( host.type == HOST_DEDICATED ) return;
	if( !cls.initialized ) return;

	Host_WriteConfig(); 
	CL_UnloadProgs();
	UI_Shutdown();
	S_Shutdown();
	SCR_Shutdown();
	CL_ShutdownInput();
	cls.initialized = false;
}