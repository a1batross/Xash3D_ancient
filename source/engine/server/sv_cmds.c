//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        sv_cmds.c - server console commands
//=======================================================================

#include "common.h"
#include "server.h"
#include "byteorder.h"

sv_client_t *sv_client; // current client

/*
=================
SV_ClientPrintf

Sends text across to be displayed if the level passes
=================
*/
void SV_ClientPrintf( sv_client_t *cl, int level, char *fmt, ... )
{
	va_list	argptr;
	char	string[MAX_SYSPATH];


	if( level < cl->messagelevel )
		return;
	if( cl->edict && (cl->edict->v.flags & FL_FAKECLIENT ))
		return;
	
	va_start( argptr, fmt );
	com.vsprintf( string, fmt, argptr );
	va_end( argptr );
	
	MSG_WriteByte( &cl->netchan.message, svc_print );
	MSG_WriteString( &cl->netchan.message, string );
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf( int level, char *fmt, ... )
{
	char		string[MAX_SYSPATH];
	va_list		argptr;
	sv_client_t	*cl;
	int		i;

	va_start( argptr, fmt );
	com.vsprintf( string, fmt, argptr );
	va_end( argptr );
	
	// echo to console
	if( host.type == HOST_DEDICATED ) Msg( "%s", string );
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( level < cl->messagelevel ) continue;
		if( cl->state != cs_spawned ) continue;
		if( cl->edict && (cl->edict->v.flags & FL_FAKECLIENT ))
			continue;
		MSG_WriteByte( &cl->netchan.message, svc_print );
		MSG_WriteString( &cl->netchan.message, string );
	}
}

/*
=================
SV_BroadcastCommand

Sends text to all active clients
=================
*/
void SV_BroadcastCommand( char *fmt, ... )
{
	va_list	argptr;
	char	string[MAX_SYSPATH];
	
	if( !sv.state ) return;
	va_start( argptr, fmt );
	com.vsprintf( string, fmt, argptr );
	va_end( argptr );

	MSG_Begin( svc_stufftext );
	MSG_WriteString( &sv.multicast, string );
	MSG_Send( MSG_ALL_R, NULL, NULL );
}

/*
====================
SV_SetMaster_f

Specify a list of master servers
====================
*/
void SV_SetMaster_f( void )
{
	int		i, slot;

	// only dedicated servers send heartbeats
	if( host.type != HOST_DEDICATED )
	{
		Msg("Only dedicated servers use masters.\n");
		return;
	}

	// make sure the server is listed public
	Cvar_Set ("public", "1");

	for( i = 1; i < MAX_MASTERS; i++ )
	{ 
		Mem_Set( &master_adr[i], 0, sizeof( master_adr[i] ));
	}

	// slot 0 will always contain the id master
	for( i = 1, slot = 1; i < Cmd_Argc(); i++ )
	{
		if( slot == MAX_MASTERS ) break;
		if(!NET_StringToAdr(Cmd_Argv(i), &master_adr[i]))
		{
			Msg( "Bad address: %s\n", Cmd_Argv(i));
			continue;
		}

		if(!master_adr[slot].port) master_adr[slot].port = BigShort (PORT_MASTER);
		Msg( "Master server at %s\n", NET_AdrToString (master_adr[slot]));
		Msg( "Sending a ping.\n");
		Netchan_OutOfBandPrint( NS_SERVER, master_adr[slot], "ping" );
		slot++;
	}
	svs.last_heartbeat = MAX_HEARTBEAT;
}

/*
==================
SV_SetPlayer

Sets sv_client and sv_player to the player with idnum Cmd_Argv(1)
==================
*/
bool SV_SetPlayer( void )
{
	char		*s;
	sv_client_t	*cl;
	int		i, idnum;

	if(Cmd_Argc() < 2) return false;

	s = Cmd_Argv(1);

	// numeric values are just slot numbers
	if( s[0] >= '0' && s[0] <= '9' )
	{
		idnum = com.atoi( Cmd_Argv( 1 ));
		if( idnum < 0 || idnum >= sv_maxclients->integer )
		{
			Msg( "Bad client slot: %i\n", idnum );
			return false;
		}
		sv_client = &svs.clients[idnum];
		if( !sv_client->state )
		{
			Msg( "Client %i is not active\n", idnum );
			return false;
		}
		return true;
	}

	// check for a name match
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( !cl->state ) continue;
		if( !com.strcmp( cl->name, s ))
		{
			sv_client = cl;
			return true;
		}
	}
	Msg( "Userid %s is not on the server\n", s );
	return false;
}

/*
==================
SV_Map_f

Goes directly to a given map without any savegame archiving.
For development work
==================
*/
void SV_Map_f( void )
{
	string	filename;

	if( Cmd_Argc() != 2 )
	{
		Msg("Usage: map <filename>\n");
		return;
	}

	com.snprintf( filename, MAX_STRING, "%s.bsp", Cmd_Argv(1));
	if( !FS_FileExists( va("maps/%s", filename )))
	{
		Msg( "Can't loading %s\n", filename );
		return;
	}

	SV_InitGame(); // reset previous state

	SV_BroadcastCommand( "changing\n" );
	SV_SendClientMessages();
	SV_SpawnServer( filename, NULL );
	SV_BroadcastCommand( "reconnect\n" );

	// archive server state
	com.strncpy( svs.mapname, filename, sizeof( svs.mapname ) - 1 );
}

void SV_Newgame_f( void )
{
	// FIXME: do some clear operations
	// FIXME: parse newgame script
	Cbuf_ExecuteText(EXEC_APPEND, va("map %s\n", GI->startmap ));
}

/*
==============
SV_Load_f

==============
*/
void SV_Load_f( void )
{
	string	filename;

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: load <filename>\n" );
		return;
	}

	com.snprintf( filename, MAX_STRING, "%s.bin", Cmd_Argv( 1 ));
	if(!FS_FileExists( va( "save/%s", filename )))
	{
		Msg("Can't loading %s\n", filename );
		return;
	}

	SV_ReadSaveFile( filename );
	SV_BroadcastCommand( "changing\n" );
	SV_SendClientMessages();
	SV_SpawnServer( svs.mapname, filename );
	SV_BroadcastCommand( "reconnect\n" );
}

/*
==============
SV_Save_f

==============
*/
void SV_Save_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg ("Usage: save <name>\n");
		return;
	}
	SV_WriteSaveFile( Cmd_Argv( 1 ), false );
}

/*
==============
SV_Delete_f

==============
*/
void SV_Delete_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: delete <name>\n" );
		return;
	}

	// delete save and saveshot
	FS_Delete( va( "%s/save/%s.bin", GI->gamedir, Cmd_Argv( 1 )));
	FS_Delete( va( "%s/save/%s.jpg", GI->gamedir, Cmd_Argv( 1 )));
}

/*
==============
SV_AutoSave_f

==============
*/
void SV_AutoSave_f( void )
{
	SV_WriteSaveFile( "autosave", true );
}

/*
==================
SV_ChangeLevel_f

Saves the state of the map just being exited and goes to a new map.
==================
*/
void SV_ChangeLevel_f( void )
{
	string	filename;
	int	c = Cmd_Argc();

	if( c != 2 && c != 3 )
	{
		Msg ("Usage: changelevel <map> [landmark]\n");
		return;
	}

	com.snprintf( filename, MAX_STRING, "%s.bsp", Cmd_Argv(1));
	if(!FS_FileExists(va("maps/%s", filename )))
	{
		Msg("Can't loading %s\n", filename );
		return;
	}

	if( sv.state == ss_active )
	{
		bool		*savedFree;
		sv_client_t	*cl;
		int		i;
	
		// clear all the client free flags before saving so that
		// when the level is re-entered, the clients will spawn
		// at spawn points instead of occupying body shells
		savedFree = Z_Malloc( sv_maxclients->integer * sizeof( bool ));
		for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
		{
			savedFree[i] = cl->edict->free;
			cl->edict->free = true;
		}
		SV_WriteSaveFile( "autosave", true );
		// we must restore these for clients to transfer over correctly
		for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
			cl->edict->free = savedFree[i];
		Mem_Free( savedFree );
	}

	SV_InitGame(); // reset previous state
	SV_BroadcastCommand("changing\n");
	SV_SendClientMessages();
	SV_SpawnServer( filename, NULL );
	SV_BroadcastCommand ("reconnect\n");

	// archive server state
	com.strncpy( svs.mapname, filename, sizeof( svs.mapname ) - 1 );
}

/*
==================
SV_Restart_f

restarts current level
==================
*/
void SV_Restart_f( void )
{
	string	filename;
	
	if( sv.state != ss_active ) return;
	com.strncpy( filename, svs.mapname, MAX_STRING );
	FS_StripExtension( filename );

	// just sending console command
	Cbuf_AddText( va( "map %s\n", filename ));
}

/*
==================
SV_Kick_f

Kick a user off of the server
==================
*/
void SV_Kick_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: kick <userid>\n" );
		return;
	}

	if( !svs.clients )
	{
		Msg( "^3no server running.\n" );
		return;
	}
	if( !SV_SetPlayer()) return;

	SV_BroadcastPrintf( PRINT_HIGH, "%s was kicked\n", sv_client->name );
	SV_ClientPrintf( sv_client, PRINT_HIGH, "You were kicked from the game\n" );
	SV_DropClient( sv_client );
	sv_client->lastmessage = svs.realtime; // min case there is a funny zombie
}

/*
================
SV_Status_f
================
*/
void SV_Status_f( void )
{
	int		i;
	sv_client_t	*cl;

	if( !svs.clients )
	{
		Msg ( "^3no server running.\n" );
		return;
	}

	Msg( "map: %s\n", sv.name );
	Msg( "num score ping    name            lastmsg address               port \n" );
	Msg( "--- ----- ------- --------------- ------- --------------------- ------\n" );

	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		int	j, l, ping;
		char	*s;

		if( !cl->state ) continue;

		Msg( "%3i ", i );
		Msg( "%5i ", (int)cl->edict->v.frags );

		if( cl->state == cs_connected ) Msg( "Connect" );
		else if( cl->state == cs_zombie ) Msg( "Zombie " );
		else
		{
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Msg( "%7i ", ping );
		}

		Msg( "%s", cl->name );
		l = 24 - com.strlen( cl->name );
		for( j = 0; j < l; j++ ) Msg( " " );
		Msg( "%g ", (svs.realtime - cl->lastmessage) * 0.001f );
		s = NET_AdrToString( cl->netchan.remote_address );
		Msg( "%s", s );
		l = 22 - com.strlen( s );
		for( j = 0; j < l; j++ ) Msg( " " );
		Msg( "%5i", cl->netchan.qport );
		Msg( "\n" );
	}
	Msg( "\n" );
}

/*
==================
SV_ConSay_f
==================
*/
void SV_ConSay_f( void )
{
	char		*p, text[MAX_SYSPATH];
	sv_client_t	*client;
	int		i;

	if(Cmd_Argc() < 2) return;

	com.strncpy( text, "console: ", MAX_SYSPATH );
	p = Cmd_Args();

	if( *p == '"' )
	{
		p++;
		p[com.strlen(p) - 1] = 0;
	}
	com.strncat( text, p, MAX_SYSPATH );

	for( i = 0, client = svs.clients; i < sv_maxclients->integer; i++, client++ )
	{
		if( client->state != cs_spawned ) continue;
		SV_ClientPrintf( client, PRINT_CHAT, "%s\n", text );
	}
}

/*
==================
SV_Heartbeat_f
==================
*/
void SV_Heartbeat_f (void)
{
	svs.last_heartbeat = MAX_HEARTBEAT;
}

/*
===========
SV_ServerInfo_f

Examine serverinfo string
===========
*/
void SV_ServerInfo_f( void )
{
	Msg("Server info settings:\n");
	Info_Print( Cvar_Serverinfo());
}

/*
===========
SV_ClientInfo_f

Examine all a users info strings
===========
*/
void SV_ClientInfo_f( void )
{
	if(Cmd_Argc() != 2)
	{
		Msg("Usage: clientinfo <userid>\n" );
		return;
	}

	if(!SV_SetPlayer()) return;
	Msg("userinfo\n");
	Msg("--------\n");
	Info_Print( sv_client->userinfo );

}

/*
===============
SV_KillServer_f

Kick everyone off, possibly in preparation for a new game
===============
*/
void SV_KillServer_f (void)
{
	if( !svs.initialized ) return;
	com.strncpy( host.finalmsg, "Server was killed\n", MAX_STRING );
	SV_Shutdown( false );
}

/*
==================
SV_InitOperatorCommands
==================
*/
void SV_InitOperatorCommands( void )
{
	Cmd_AddCommand( "heartbeat", SV_Heartbeat_f, "send a heartbeat to the master server" );
	Cmd_AddCommand( "kick", SV_Kick_f, "kick a player off the server by number or name" );
	Cmd_AddCommand( "status", SV_Status_f, "print server status information" );
	Cmd_AddCommand( "serverinfo", SV_ServerInfo_f, "print server settings" );
	Cmd_AddCommand( "clientinfo", SV_ClientInfo_f, "print user infostring (player num required)" );

	Cmd_AddCommand( "map", SV_Map_f, "start new level" );
	Cmd_AddCommand( "devmap", SV_Map_f, "start new level" );
	Cmd_AddCommand( "newgame", SV_Newgame_f, "begin new game" );
	Cmd_AddCommand( "changelevel", SV_ChangeLevel_f, "changing level" );
	Cmd_AddCommand( "restart", SV_Restart_f, "restarting current level" );

	if( host.type == HOST_DEDICATED )
	{
		Cmd_AddCommand( "say", SV_ConSay_f, "send a chat message to everyone on the server" );
		Cmd_AddCommand( "setmaster", SV_SetMaster_f, "set ip address for dedicated server" );
	}

	Cmd_AddCommand( "save", SV_Save_f, "save the game to a file" );
	Cmd_AddCommand( "load", SV_Load_f, "load a saved game file" );
	Cmd_AddCommand( "delete", SV_Delete_f, "delete a saved game file and saveshot" );
	Cmd_AddCommand( "autosave", SV_AutoSave_f, "save the game to 'autosave' file" );
	Cmd_AddCommand( "killserver", SV_KillServer_f, "shutdown current server" );
}

void SV_KillOperatorCommands( void )
{
	Cmd_RemoveCommand( "heartbeat" );
	Cmd_RemoveCommand( "kick" );
	Cmd_RemoveCommand( "status" );
	Cmd_RemoveCommand( "serverinfo" );
	Cmd_RemoveCommand( "clientinfo" );

	Cmd_RemoveCommand( "map" );
	Cmd_RemoveCommand( "movie" );
	Cmd_RemoveCommand( "newgame" );
	Cmd_RemoveCommand( "changelevel" );
	Cmd_RemoveCommand( "restart" );
	Cmd_RemoveCommand( "sectorlist" );

	if( host.type == HOST_DEDICATED )
	{
		Cmd_RemoveCommand( "say" );
		Cmd_RemoveCommand( "setmaster" );
	}

	Cmd_RemoveCommand( "save" );
	Cmd_RemoveCommand( "load" );
	Cmd_RemoveCommand( "delete" );
	Cmd_RemoveCommand( "autosave" );
	Cmd_RemoveCommand( "killserver" );
}