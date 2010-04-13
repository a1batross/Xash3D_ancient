//=======================================================================
//			Copyright XashXT Group 2009 ©
//			cl_main.c - client main loop
//=======================================================================

#include "common.h"
#include "client.h"
#include "byteorder.h"

cvar_t	*rcon_client_password;
cvar_t	*rcon_address;

cvar_t	*cl_timeout;
cvar_t	*cl_predict;
cvar_t	*cl_showfps;
cvar_t	*cl_maxfps;
cvar_t	*cl_nodelta;
cvar_t	*cl_crosshair;
cvar_t	*cl_shownet;
cvar_t	*cl_showmiss;
cvar_t	*cl_mouselook;
cvar_t	*userinfo;

//
// userinfo
//
cvar_t	*info_password;
cvar_t	*info_spectator;
cvar_t	*name;
cvar_t	*model;
cvar_t	*topcolor;
cvar_t	*bottomcolor;
cvar_t	*rate;
cvar_t	*cl_lw;

client_t		cl;
client_static_t	cls;
clgame_static_t	clgame;

//======================================================================
bool CL_Active( void )
{
	return ( cls.state == ca_active );
}

//======================================================================
bool CL_IsInGame( void )
{
	if( host.type == HOST_DEDICATED ) return true;	// always active for dedicated servers
	if( CL_GetMaxClients() > 1 ) return true;	// always active for multiplayer
	return ( cls.key_dest == key_game );		// active if not menu or console
}

void CL_ForceVid( void )
{
	cl.video_prepped = false;
	host.state = HOST_RESTART;
}

void CL_ForceSnd( void )
{
	cl.audio_prepped = false;
}

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

	if( cls.demoplayback )
	{
		if( !com.stricmp( Cmd_Argv( 1 ), "pause" ))
			cl.refdef.paused ^= 1;
		return;
	}

	if( cls.state != ca_connected && cls.state != ca_active )
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
===================
Cmd_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void Cmd_ForwardToServer( void )
{
	char	*cmd;

	if( cls.demoplayback )
	{
		if( !com.stricmp( Cmd_Argv( 1 ), "pause" ))
			cl.refdef.paused ^= 1;
		return;
	}

	cmd = Cmd_Argv( 0 );
	if( cls.state <= ca_connected || *cmd == '-' || *cmd == '+' )
	{
		Msg( "Unknown command \"%s\"\n", cmd );
		return;
	}

	MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
	MSG_Print( &cls.netchan.message, cmd );

	if( Cmd_Argc() > 1 )
	{
		MSG_Print( &cls.netchan.message, " " );
		MSG_Print( &cls.netchan.message, Cmd_Args( ));
	}
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
void CL_Drop( void )
{
	if( cls.state == ca_uninitialized )
		return;
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

	if( !NET_StringToAdr( cls.servername, &adr ))
	{
		MsgDev( D_INFO, "CL_SendConnectPacket: bad server address\n");
		cls.connect_time = 0;
		return;
	}
	if( adr.port == 0 ) adr.port = BigShort( PORT_SERVER );
	port = Cvar_VariableValue( "net_qport" );

	userinfo->modified = false;
	Netchan_OutOfBandPrint( NS_CLIENT, adr, "connect %i %i %i \"%s\"\n", PROTOCOL_VERSION, port, cls.challenge, Cvar_Userinfo( ));
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend( void )
{
	netadr_t	adr;

	// if the local server is running and we aren't then connect
	if( cls.state == ca_disconnected && SV_Active( ))
	{
		cls.state = ca_connecting;
		com.strncpy( cls.servername, "localhost", sizeof( cls.servername ));
		// we don't need a challenge on the localhost
		CL_SendConnectPacket();
		return;
	}

	// resend if we haven't gotten a reply yet
	if( cls.demoplayback || cls.state != ca_connecting )
		return;

	if(( cls.realtime - cls.connect_time ) < 3000 )
		return;

	if( !NET_StringToAdr( cls.servername, &adr ))
	{
		MsgDev( D_ERROR, "CL_CheckForResend: bad server address\n" );
		cls.state = ca_disconnected;
		return;
	}

	if( adr.port == 0 ) adr.port = BigShort( PORT_SERVER );
	cls.connect_time = cls.realtime; // for retransmit requests

	MsgDev( D_NOTE, "Connecting to %s...\n", cls.servername );
	Netchan_OutOfBandPrint( NS_CLIENT, adr, "getchallenge\n" );
}


/*
================
CL_Connect_f

================
*/
void CL_Connect_f( void )
{
	char	*server;

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: connect <server>\n" );
		return;	
	}
	
	if( Host_ServerState())
	{	
		// if running a local server, kill it and reissue
		com.strncpy( host.finalmsg, "Server quit\n", MAX_STRING );
		SV_Shutdown( false );
	}

	server = Cmd_Argv( 1 );
	NET_Config( true ); // allow remote
	
	Msg( "server %s\n", server );
	CL_Disconnect();

	cls.state = ca_connecting;
	com.strncpy( cls.servername, server, sizeof( cls.servername ));
	cls.connect_time = MAX_HEARTBEAT; // CL_CheckForResend() will fire immediately
}


/*
=====================
CL_Rcon_f

Send the rest of the command line over as
an unconnected command.
=====================
*/
void CL_Rcon_f( void )
{
	char	message[1024];
	netadr_t	to;
	int	i;

	if( !rcon_client_password->string )
	{
		Msg( "You must set 'rcon_password' before\n" "issuing an rcon command.\n" );
		return;
	}

	message[0] = (char)255;
	message[1] = (char)255;
	message[2] = (char)255;
	message[3] = (char)255;
	message[4] = 0;

	NET_Config( true );	// allow remote

	com.strcat( message, "rcon " );
	com.strcat( message, rcon_client_password->string );
	com.strcat( message, " " );

	for( i = 1; i < Cmd_Argc(); i++ )
	{
		com.strcat( message, Cmd_Argv( i ));
		com.strcat( message, " " );
	}

	if( cls.state >= ca_connected )
	{
		to = cls.netchan.remote_address;
	}
	else
	{
		if( !com.strlen( rcon_address->string ))
		{
			Msg( "You must either be connected,\n" "or set the 'rcon_address' cvar\n" "to issue rcon commands\n" );
			return;
		}

		NET_StringToAdr( rcon_address->string, &to );
		if( to.port == 0 ) to.port = BigShort( PORT_SERVER );
	}
	
	NET_SendPacket( NS_CLIENT, com.strlen( message ) + 1, message, to );
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
	CL_FreeEdicts ();

	if( clgame.hInstance ) clgame.dllFuncs.pfnReset();

	// wipe the entire cl structure
	Mem_Set( &cl, 0, sizeof( cl ));
	MSG_Clear( &cls.netchan.message );

	Cvar_SetValue( "scr_download", 0.0f );
	Cvar_SetValue( "scr_loading", 0.0f );
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
	CL_Stop_f();

	// send a disconnect message to the server
	final[0] = clc_stringcmd;
	com.strcpy((char *)final+1, "disconnect" );
	Netchan_Transmit( &cls.netchan, com.strlen( final ), final );
	Netchan_Transmit( &cls.netchan, com.strlen( final ), final );
	Netchan_Transmit( &cls.netchan, com.strlen( final ), final );

	CL_ClearState ();

	// stop download
	if( cls.download )
	{
		FS_Close( cls.download );
		cls.download = NULL;
	}

	cls.state = ca_disconnected;

	// back to menu if developer mode set to "player" or "mapper"
	if( host.developer > 2 ) return;
	UI_SetActiveMenu( UI_MAINMENU );
}

void CL_Disconnect_f( void )
{
	Host_Error( "Disconnected from server\n" );
}

void CL_Crashed_f( void )
{
	byte	final[32];

	// already freed
	if( host.state == HOST_CRASHED ) return;
	if( host.type != HOST_NORMAL ) return;
	if( !cls.initialized ) return;

	host.state = HOST_CRASHED;

	CL_Stop_f(); // stop any demos

	// send a disconnect message to the server
	final[0] = clc_stringcmd;
	com.strcpy((char *)final+1, "disconnect" );
	Netchan_Transmit( &cls.netchan, com.strlen( final ), final );
	Netchan_Transmit( &cls.netchan, com.strlen( final ), final );
	Netchan_Transmit( &cls.netchan, com.strlen( final ), final );

	// stop any downloads
	if( cls.download ) FS_Close( cls.download );

	Host_WriteConfig();	// write config
	if( re ) re->RestoreGamma();
}

/*
=================
CL_LocalServers_f
=================
*/
void CL_LocalServers_f( void )
{
	netadr_t	adr;

	MsgDev( D_INFO, "Scanning for servers on the local network area...\n" );
	NET_Config( true ); // allow remote
	
	// send a broadcast packet
	adr.type = NA_BROADCAST;
	adr.port = BigShort( PORT_SERVER );

	Netchan_OutOfBandPrint( NS_CLIENT, adr, "info %i", PROTOCOL_VERSION );
}

/*
====================
CL_Packet_f

packet <destination> <contents>

Contents allows \n escape character
====================
*/
void CL_Packet_f( void )
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

	NET_Config( true ); // allow remote

	if( !NET_StringToAdr( Cmd_Argv( 1 ), &adr ))
	{
		Msg( "Bad address\n" );
		return;
	}

	if( !adr.port ) adr.port = BigShort( PORT_SERVER );

	in = Cmd_Argv( 2 );
	out = send + 4;
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
CL_Reconnect_f

The server is changing levels
=================
*/
void CL_Reconnect_f( void )
{
	// if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if( cls.download ) return;

	S_StopAllSounds ();

	// disable plaque draw on change map
	cls.drawplaque = false;
	Cmd_ExecuteString( "hud_changelevel\n" );

	if( cls.demoplayback ) return;

	if( cls.state == ca_connected )
	{
		cls.demonum = -1;	// not in the demo loop now
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
			cls.connect_time = cls.realtime - 1500;
		}
		else cls.connect_time = MAX_HEARTBEAT; // fire immediately

		cls.demonum = -1;	// not in the demo loop now
		cls.state = ca_connecting;
		Msg( "reconnecting...\n" );
	}
}

/*
=================
CL_ParseStatusMessage

Handle a reply from a info
=================
*/
void CL_ParseStatusMessage( netadr_t from, sizebuf_t *msg )
{
	char	*s;

	s = MSG_ReadString( msg );
	UI_AddServerToList( from, s );
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

	MsgDev( D_LOAD, "CL_PrepSound: %s\n", cl.configstrings[CS_NAME] );		
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
	string	name, mapname;
	int	i, mdlcount;
	int	map_checksum; // dummy

	if( !cl.configstrings[CS_MODELS+1][0] )
		return; // no map loaded

	Cvar_SetValue( "scr_loading", 0.0f ); // reset progress bar
	MsgDev( D_LOAD, "CL_PrepVideo: %s\n", cl.configstrings[CS_NAME] );

	// let the render dll load the map
	clgame.globals->mapname = MAKE_STRING( cl.configstrings[CS_NAME] );
	com.strncpy( mapname, cl.configstrings[CS_MODELS+1], MAX_STRING ); 
	CM_BeginRegistration( mapname, true, &map_checksum );
	re->BeginRegistration( mapname, CM_VisData()); // load map
	SCR_RegisterShaders(); // update with new sequence
	SCR_UpdateScreen();

	// clear physics interaction links
	CL_ClearWorld ();

	for( i = 0, mdlcount = 0; i < MAX_MODELS && cl.configstrings[CS_MODELS+1+i][0]; i++ )
		mdlcount++; // total num models

	for( i = 0; i < MAX_MODELS && cl.configstrings[CS_MODELS+1+i][0]; i++ )
	{
		com.strncpy( name, cl.configstrings[CS_MODELS+1+i], MAX_STRING );
		re->RegisterModel( name, i+1 );
		CM_RegisterModel( name, i+1 );
		Cvar_SetValue( "scr_loading", scr_loading->value + 45.0f / mdlcount );
		SCR_UpdateScreen();
	}

	for( i = 0; i < MAX_DECALS && cl.configstrings[CS_DECALS+1+i][0]; i++ )
	{
		com.strncpy( name, cl.configstrings[CS_DECALS+1+i], MAX_STRING );
		cl.decal_shaders[i+1] = re->RegisterShader( name, SHADER_GENERIC );
	}

	// setup sky and free unneeded stuff
	re->EndRegistration( cl.configstrings[CS_SKYNAME] );
	CM_EndRegistration (); // free unused models
	Cvar_SetValue( "scr_loading", 100.0f );	// all done
	
	if( host.developer <= 2 ) Con_ClearNotify(); // clear any lines of console text
	SCR_UpdateScreen();
	cl.video_prepped = true;
	cl.force_refdef = true;
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
	c = Cmd_Argv( 0 );

	MsgDev( D_NOTE, "CL_ConnectionlessPacket: %s : %s\n", NET_AdrToString( from ), c );

	// server connection
	if( !com.strcmp( c, "client_connect" ))
	{
		if( cls.state == ca_connected )
		{
			MsgDev( D_INFO, "dup connect received. ignored\n");
			return;
		}
		Netchan_Setup( NS_CLIENT, &cls.netchan, from, Cvar_VariableValue( "net_qport" ));
		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		MSG_Print( &cls.netchan.message, "new" );
		cls.state = ca_connected;
		UI_SetActiveMenu( UI_CLOSEMENU );
	}
	else if( !com.strcmp( c, "info" ))
	{
		// server responding to a status broadcast
		CL_ParseStatusMessage( from, msg );
	}
	else if( !com.strcmp( c, "cmd" ))
	{
		// remote command from gui front end
		if(!NET_IsLocalAddress( from ))
		{
			Msg( "Command packet from remote host.  Ignored.\n" );
			return;
		}

		ShowWindow( host.hWnd, SW_RESTORE );
		SetForegroundWindow ( host.hWnd );
		s = MSG_ReadString( msg );
		Cbuf_AddText( s );
		Cbuf_AddText( "\n" );
	}
	else if( !com.strcmp( c, "print" ))
	{
		// print command from somewhere
		s = MSG_ReadString( msg );
		Msg( s );
	}
	else if( !com.strcmp( c, "ping" ))
	{
		// ping from somewhere
		Netchan_OutOfBandPrint( NS_CLIENT, from, "ack" );
	}
	else if( !com.strcmp( c, "challenge" ))
	{
		// challenge from the server we are connecting to
		cls.challenge = com.atoi( Cmd_Argv( 1 ));
		CL_SendConnectPacket();
		return;
	}
	else if( !com.strcmp( c, "echo" ))
	{
		// echo request from server
		Netchan_OutOfBandPrint( NS_CLIENT, from, "%s", Cmd_Argv( 1 ));
	}
	else if( !com.strcmp( c, "disconnect" ))
	{
		// a disconnect message from the server, which will happen if the server
		// dropped the connection but it is still getting packets from us
		CL_Disconnect();
	}
	else MsgDev( D_ERROR, "bad connectionless packet from %s:\n%s\n", NET_AdrToString( from ), s );
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

	cl.time = bound( cl.frame.servertime - cl.serverframetime, cl.time, cl.frame.servertime );

	if( cl.refdef.paused ) cl.lerpFrac = 1.0f;
	else cl.lerpFrac = 1.0 - (cl.frame.servertime - cl.time) / (float)cl.serverframetime;

	// singleplayer never has connection timeout
	if( NET_IsLocalAddress( cls.netchan.remote_address ))
		return;
          
	// check timeout
	if( cls.state >= ca_connected && !cls.demoplayback )
	{
		if( cls.realtime - cls.netchan.last_received > (cl_timeout->value * 1000))
		{
			if( ++cl.timeoutcount > 5 ) // timeoutcount saves debugger
			{
				Msg( "\nServer connection timed out.\n" );
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
void CL_Userinfo_f( void )
{
	Msg( "User info settings:\n" );
	Info_Print( Cvar_Userinfo( ));
}

/*
==============
CL_Physinfo_f
==============
*/
void CL_Physinfo_f( void )
{
	Msg( "Phys info settings:\n" );
	Info_Print( cl.physinfo );
}

int precache_check; // for autodownload of precache items
int precache_spawncount;

// ENV_CNT is map load, ENV_CNT+1 is first cubemap
#define ENV_CNT		MAX_CONFIGSTRINGS
#define TEXTURE_CNT		(ENV_CNT+1)

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
			if( cl.configstrings[precache_check][0] == '*' || cl.configstrings[precache_check][0] == '#' )
			{
				precache_check++; // ignore bsp models or built-in models
				continue;
			}
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
			if( cl.configstrings[precache_check][0] == '*' || cl.configstrings[precache_check][0] == '#' )
			{
				precache_check++;
				continue;
			}
			com.sprintf( fn, "sound/%s", cl.configstrings[precache_check++]);
			if(!CL_CheckOrDownloadFile( fn )) return; // started a download
		}
		precache_check = CS_GENERICS;
	}

	if( precache_check >= CS_GENERICS && precache_check < CS_GENERICS+MAX_GENERICS )
	{
		if( precache_check == CS_GENERICS ) precache_check++; // zero is blank
		while( precache_check < CS_GENERICS+MAX_GENERICS && cl.configstrings[precache_check][0] )
		{
			// generic recources - pakfiles, wadfiles etc
			if( cl.configstrings[precache_check][0] == '#' )
			{
				precache_check++;
				continue;
			}
			com.sprintf( fn, "%s", cl.configstrings[precache_check++]);
			if(!CL_CheckOrDownloadFile( fn )) return; // started a download
		}
		precache_check = ENV_CNT;
	}

	if( precache_check == ENV_CNT )
	{
		precache_check = ENV_CNT + 1;

		CM_BeginRegistration( cl.configstrings[CS_MODELS+1], true, &map_checksum );

		if( com.atoi( cl.configstrings[CS_MAPCHECKSUM] ) == 0xBAD )
		{
			if( cl.refdef.demoplayback )
				MsgDev( D_INFO, "Playing demo without physics collision\n" );
			else MsgDev( D_WARN, "Server %s doesn't have physics\n", cls.servername );
		}
		else if( map_checksum == 0xBAD && !pe )
		{
			if( !cls.demoplayback )
				MsgDev( D_WARN, "Local client doesn't have physics\n" );
		}
		else if( map_checksum != com.atoi( cl.configstrings[CS_MAPCHECKSUM] ))
		{
			Host_Error( "Local map version differs from server: %i != '%s'\n", map_checksum, cl.configstrings[CS_MAPCHECKSUM] );
			return;
		}
	}

	if( precache_check > ENV_CNT && precache_check < TEXTURE_CNT )
	{
		if( allow_download->value )
		{
			com.sprintf( fn, "env/%s.dds", cl.configstrings[CS_SKYNAME] ); // cubemap pack
			if( !CL_CheckOrDownloadFile( fn )) return; // started a download
		}
		precache_check = TEXTURE_CNT;
	}

	// skip textures: use generic for downloading packs
	if( precache_check == TEXTURE_CNT )
		precache_check = TEXTURE_CNT + 999;

	CL_PrepSound();
	CL_PrepVideo();
	CL_SortUserMessages();

	if( cls.demoplayback ) return; // not really connected
	MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
	MSG_Print( &cls.netchan.message, va( "begin %i\n", precache_spawncount ));
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
	precache_spawncount = com.atoi( Cmd_Argv( 1 ));
	CL_RequestNextDownload();
}

/*
=================
CL_Escape_f

Escape to menu from game
=================
*/
void CL_Escape_f( void )
{
	if( cls.key_dest == key_menu )
		return;

	if( cls.state == ca_cinematic )
		SCR_StopCinematic();
	else UI_SetActiveMenu( UI_MAINMENU );
	cls.key_dest = key_menu;
}

/*
=================
CL_InitLocal
=================
*/
void CL_InitLocal( void )
{
	cls.state = ca_disconnected;
	cls.drawplaque = true;

	// register our variables
	cl_predict = Cvar_Get( "cl_predict", "1", CVAR_ARCHIVE, "disables client movement prediction" );
	cl_maxfps = Cvar_Get( "cl_maxfps", "1000", 0, "maximum client fps" );
	cl_crosshair = Cvar_Get( "crosshair", "1", CVAR_ARCHIVE|CVAR_USERINFO, "show weapon chrosshair" );
	cl_nodelta = Cvar_Get ("cl_nodelta", "0", 0, "disable delta-compression for usercommnds" );

	cl_shownet = Cvar_Get( "cl_shownet", "0", 0, "client show network packets" );
	cl_showmiss = Cvar_Get( "cl_showmiss", "0", CVAR_ARCHIVE, "client show prediction errors" );
	cl_timeout = Cvar_Get( "cl_timeout", "120", 0, "connect timeout (in-seconds)" );

	rcon_client_password = Cvar_Get( "rcon_password", "", 0, "remote control client password" );
	rcon_address = Cvar_Get( "rcon_address", "", 0, "remote control address" );

	// userinfo
	info_password = Cvar_Get( "password", "", CVAR_USERINFO, "player password" );
	info_spectator = Cvar_Get( "spectator", "0", CVAR_USERINFO, "spectator mode" );
	name = Cvar_Get( "name", SI->username, CVAR_USERINFO | CVAR_ARCHIVE, "player name" );
	model = Cvar_Get( "model", "player", CVAR_USERINFO | CVAR_ARCHIVE, "player model ('player' it's a single player model)" );
	topcolor = Cvar_Get( "topcolor", "0", CVAR_USERINFO | CVAR_ARCHIVE, "player top color" );
	bottomcolor = Cvar_Get( "bottomcolor", "0", CVAR_USERINFO | CVAR_ARCHIVE, "player bottom color" );
	rate = Cvar_Get( "rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE, "player network rate" );	// FIXME
	userinfo = Cvar_Get( "@userinfo", "0", CVAR_READ_ONLY, "" ); // use ->modified value only
	cl_showfps = Cvar_Get( "cl_showfps", "1", CVAR_ARCHIVE, "show client fps" );
	cl_lw = Cvar_Get( "cl_lw", "1", CVAR_ARCHIVE|CVAR_USERINFO, "enable client weapon predicting" );

	// register our commands
	Cmd_AddCommand ("cmd", CL_ForwardToServer_f, "send a console commandline to the server" );
	Cmd_AddCommand ("pause", NULL, "pause the game (if the server allows pausing)" );
	Cmd_AddCommand ("localservers", CL_LocalServers_f, "collect info about local servers" );

	Cmd_AddCommand ("@crashed",  CL_Crashed_f, "" );	// internal system command
	Cmd_AddCommand ("userinfo", CL_Userinfo_f, "print current client userinfo" );
	Cmd_AddCommand ("physinfo", CL_Physinfo_f, "print current client physinfo" );
	Cmd_AddCommand ("disconnect", CL_Disconnect_f, "disconnect from server" );
	Cmd_AddCommand ("record", CL_Record_f, "record a demo" );
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f, "playing a demo" );
	Cmd_AddCommand ("killdemo", CL_DeleteDemo_f, "delete a specified demo file and demoshot" );
	Cmd_AddCommand ("startdemos", CL_StartDemos_f, "start playing back the selected demos sequentially" );
	Cmd_AddCommand ("demos", CL_Demos_f, "restart looping demos defined by the last startdemos command" );
	Cmd_AddCommand ("movie", CL_PlayVideo_f, "playing a movie" );
	Cmd_AddCommand ("stop", CL_Stop_f, "stop playing or recording a demo" );
	Cmd_AddCommand ("info", NULL, "collect info about local servers with specified protocol" );
	Cmd_AddCommand ("escape", CL_Escape_f, "escape from game to menu" );
	
	Cmd_AddCommand ("quit", CL_Quit_f, "quit from game" );
	Cmd_AddCommand ("exit", CL_Quit_f, "quit from game" );

	Cmd_AddCommand ("screenshot", CL_ScreenShot_f, "takes a screenshot of the next rendered frame" );
	Cmd_AddCommand ("envshot", CL_EnvShot_f, "takes a six-sides cubemap shot with specified name" );
	Cmd_AddCommand ("skyshot", CL_SkyShot_f, "takes a six-sides envmap (skybox) shot with specified name" );
	Cmd_AddCommand ("levelshot", CL_LevelShot_f, "same as \"screenshot\", used for create plaque images" );
	Cmd_AddCommand ("saveshot", CL_SaveShot_f, "used for create save previews with LoadGame menu" );
	Cmd_AddCommand ("demoshot", CL_DemoShot_f, "used for create demo previews with PlayDemo menu" );

	Cmd_AddCommand ("connect", CL_Connect_f, "connect to a server by hostname" );
	Cmd_AddCommand ("reconnect", CL_Reconnect_f, "reconnect to current level" );

	Cmd_AddCommand ("rcon", CL_Rcon_f, "sends a command to the server console (rcon_password and rcon_address required)" );

	// this is dangerous to leave in
// 	Cmd_AddCommand ("packet", CL_Packet_f, "send a packet with custom contents" );

	Cmd_AddCommand ("precache", CL_Precache_f, "precache specified resource (by index)" );
	Cmd_AddCommand ("download", CL_Download_f, "download specified resource (by name)" );
}

//============================================================================
/*
==================
CL_ApplyAddAngle

==================
*/
void CL_ApplyAddAngle( void )
{
	float	frametime = (cl.serverframetime * 0.001f);
	float	amove = 0.0f;
	int	i;

	for( i = 0; i < CMD_MASK; i++ )
	{
		add_angle_t	*add = &cl.addangle[i];
		float		f, remainder = fabs( add->yawdelta - add->accum );
		float		amount_to_add;

		if( remainder <= 0.0001f )
			continue;	// this angle expired

		// apply some of the delta
		f = frametime;
		f = bound( 0.0f, f, 1.0f );

		amount_to_add = f * add->yawdelta;

		if( add->yawdelta > 0.0f )
		{
			amount_to_add = min( amount_to_add, remainder );
		}
		else
		{
			amount_to_add = max( amount_to_add, -remainder );
		}

		add->accum += amount_to_add;
		amove += amount_to_add;
	}

	cl.refdef.cl_viewangles[1] += amove;
}

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
void CL_Frame( int time )
{
	if( host.type == HOST_DEDICATED )
		return;

	// decide the simulation time
	cl.time += time;		// can be merged by cl.frame.servertime 
	cls.realtime += time;
	cls.frametime = time * 0.001f;
	if( cls.frametime > 0.2f ) cls.frametime = 0.2f;

	// if in the debugger last frame, don't timeout
	if( time > 5000 ) cls.netchan.last_received = Sys_Milliseconds();

	// fetch results from server
	CL_ReadPackets();

	// send a new command message to the server
	CL_SendCommand();

	// predict all unacknowledged movements
	CL_PredictMovement();

	// apply accumulated angles
	CL_ApplyAddAngle();

	Host_CheckChanges();

	// allow sound and video DLL change
	if( cls.state == ca_active )
	{
		if( !cl.video_prepped ) CL_PrepVideo();
		if( !cl.audio_prepped ) CL_PrepSound();
	}

	// update the screen
	SCR_UpdateScreen ();

	// update audio
	S_Update( &cl.refdef );

	// advance local effects for next frame
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

	Con_Init();	

	MSG_Init( &net_message, net_message_buffer, sizeof( net_message_buffer ));

	Host_CheckChanges ();

	if( !CL_LoadProgs( "client.dll" ))
		Host_Error( "CL_InitGame: can't initialize client.dll\n" );

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
	SCR_Shutdown();
	cls.initialized = false;
}