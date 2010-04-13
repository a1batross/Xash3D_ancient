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
	MSG_WriteByte( &cl->netchan.message, level );
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
		MSG_WriteByte( &cl->netchan.message, level );
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
	MSG_Send( MSG_ALL, NULL, NULL );
}

/*
====================
SV_SetMaster_f

Specify a list of master servers
====================
*/
void SV_SetMaster_f( void )
{
	int	i, slot;

	// only dedicated servers send heartbeats
	if( host.type != HOST_DEDICATED )
	{
		Msg( "Only dedicated servers use masters.\n" );
		return;
	}

	// make sure the server is listed public
	Cvar_Set( "public", "1" );

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

	if( sv_maxclients->integer == 1 )
	{
		// sepcial case for singleplayer
		sv_client = svs.clients;
		return true;
	}

	if( Cmd_Argc() < 2 ) return false;

	s = Cmd_Argv( 1 );

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
	char	*spawn_entity;
	string	mapname;
	int	flags;

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: map <mapname>\n" );
		return;
	}

	// hold mapname to other place
	com.strncpy( mapname, Cmd_Argv( 1 ), sizeof( mapname ));
	
	// determine spawn entity classname
	if( Cvar_VariableInteger( "deathmatch" ))
		spawn_entity = GI->dm_entity;
	else if( Cvar_VariableInteger( "coop" ))
		spawn_entity = GI->coop_entity;
	else if( Cvar_VariableInteger( "teamplay" ))
		spawn_entity = GI->team_entity;
	else spawn_entity = GI->sp_entity;

	flags = SV_MapIsValid( mapname, spawn_entity, NULL );

	if(!( flags & MAP_IS_EXIST ))
	{
		Msg( "SV_NewMap: map %s doesn't exist\n", mapname );
		return;
	}

	if(!( flags & MAP_HAS_SPAWNPOINT ))
	{
		Msg( "SV_NewMap: map %s doesn't have a valid spawnpoint\n", mapname );
		return;
	}

	sv.loadgame = false; // set right state
	SV_ClearSaveDir ();	// delete all temporary *.hl files

	SV_DeactivateServer();
	SV_SpawnServer( mapname, NULL );
	SV_LevelInit( mapname, NULL, NULL, false );
	SV_ActivateServer ();
}

void SV_Newgame_f( void )
{
	if( Cmd_Argc() != 1 )
	{
		Msg( "Usage: newgame\n" );
		return;
	}

	Host_NewGame( GI->startmap, false );
}

void SV_Endgame_f( void )
{
	Host_EndGame( "The End" );
}

/*
==============
SV_Load_f

==============
*/
void SV_Load_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: load <savename>\n" );
		return;
	}
	SV_LoadGame( Cmd_Argv( 1 ));
}

/*
==============
SV_QuickLoad_f

==============
*/
void SV_QuickLoad_f( void )
{
	Cbuf_ExecuteText( EXEC_APPEND, "echo Quick Loading...; wait; load quick" );
}

/*
==============
SV_Save_f

==============
*/
void SV_Save_f( void )
{
	const char *name;

	switch( Cmd_Argc() )
	{
	case 1: name = "new"; break;
	case 2: name = Cmd_Argv( 1 ); break;
	default:
		Msg( "Usage: save <savename>\n" );
		return;
	}

	SV_SaveGame( name );
}

/*
==============
SV_QuickSave_f

==============
*/
void SV_QuickSave_f( void )
{
	Cbuf_ExecuteText( EXEC_APPEND, "echo Quick Saving...; wait; save quick" );
}

/*
==============
SV_DeleteSave_f

==============
*/
void SV_DeleteSave_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: delsave <name>\n" );
		return;
	}

	// delete save and saveshot
	FS_Delete( va( "save/%s.sav", Cmd_Argv( 1 )));
	FS_Delete( va( "save/%s.%s", Cmd_Argv( 1 ), SI->savshot_ext ));
}

/*
==============
SV_AutoSave_f

==============
*/
void SV_AutoSave_f( void )
{
	SV_SaveGame( "autosave" );
}

/*
==================
SV_ChangeLevel_f

Saves the state of the map just being exited and goes to a new map.
==================
*/
void SV_ChangeLevel_f( void )
{
	char	*spawn_entity, *mapname;
	int	flags, c = Cmd_Argc();

	mapname = Cmd_Argv( 1 );

	// determine spawn entity classname
	if( Cvar_VariableInteger( "deathmatch" ))
		spawn_entity = GI->dm_entity;
	else if( Cvar_VariableInteger( "coop" ))
		spawn_entity = GI->coop_entity;
	else if( Cvar_VariableInteger( "teamplay" ))
		spawn_entity = GI->team_entity;
	else spawn_entity = GI->sp_entity;

	flags = SV_MapIsValid( mapname, spawn_entity, Cmd_Argv( 2 ));

	if(!( flags & MAP_IS_EXIST ))
	{
		Msg( "SV_ChangeLevel: map %s doesn't exist\n", mapname );
		return;
	}

	if( c == 3 && !( flags & MAP_HAS_LANDMARK ))
	{
		// NOTE: we find valid map but specified landmark it's doesn't exist
		// run simple changelevel like in q1, throw warning
		MsgDev( D_INFO, "SV_ChangeLevel: map %s is exist but doesn't contain\n", mapname );
		MsgDev( D_INFO, "landmark with name %s. Run classic quake changelevel\n", Cmd_Argv( 2 ));
		c = 2; // reduce args
	}

	if( c == 3 && !com.stricmp( sv.name, Cmd_Argv( 1 )))
	{
		MsgDev( D_INFO, "SV_ChangeLevel: can't changelevel with same map. Ignored.\n" );
		return;	
	}

	if( c == 2 && !( flags & MAP_HAS_SPAWNPOINT ))
	{
		MsgDev( D_INFO, "SV_ChangeLevel: map %s doesn't have a valid spawnpoint. Ignored.\n", mapname );
		return;	
	}

	if( sv.state != ss_active )
	{
		// just load map
		Cbuf_AddText( va( "map %s\n", mapname ));
		return;
	}

	switch( c )
	{
	case 2: SV_ChangeLevel( false, Cmd_Argv( 1 ), NULL ); break;
	case 3: SV_ChangeLevel( true, Cmd_Argv( 1 ), Cmd_Argv( 2 )); break;
	default: Msg( "Usage: changelevel <map> [landmark]\n" ); break;
	}
}

/*
==================
SV_Restart_f

restarts current level
==================
*/
void SV_Restart_f( void )
{
	if( sv.state != ss_active ) return;

	// just sending console command
	Cbuf_AddText( va( "map %s\n", sv.name ));
}

void SV_Reload_f( void )
{
	const char	*save;
	string		loadname;
	
	if( sv.state != ss_active )
		return;

	save = SV_GetLatestSave();
	if( save )
	{
		FS_FileBase( save, loadname );
		Cbuf_AddText( va( "load %s\n", loadname ));
	}
	else Cbuf_AddText( "newgame\n" ); // begin new game
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
==================
SV_Kill_f
==================
*/
void SV_Kill_f( void )
{
	if( !SV_SetPlayer()) return;
	if( sv_client->edict->v.health <= 0.0f )
	{
		SV_ClientPrintf( sv_client, PRINT_HIGH, "Can't suicide -- allready dead!\n");
		return;
	}

	svgame.dllFuncs.pfnClientKill( sv_client->edict );	
}

/*
==================
SV_EntPatch_f
==================
*/
void SV_EntPatch_f( void )
{
	const char	*mapname;

	if( Cmd_Argc() < 2 )
	{
		if( sv.state != ss_dead )
		{
			mapname = sv.name;
		}
		else
		{
			Msg( "Usage: entpatch <mapname>\n" );
			return;
		}
	}
	else mapname = Cmd_Argv( 2 );

	SV_WriteEntityPatch( mapname );
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
void SV_Heartbeat_f( void )
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
	Msg( "Server info settings:\n" );
	Info_Print( Cvar_Serverinfo( ));
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
void SV_KillServer_f( void )
{
	if( !svs.initialized ) return;
	com.strncpy( host.finalmsg, "Server was killed\n", MAX_STRING );
	SV_Shutdown( false );
	NET_Config ( false ); // close network sockets
}

/*
===============
SV_PlayersOnly_f

disable plhysics but players
===============
*/
void SV_PlayersOnly_f( void )
{
	sv.hostflags = sv.hostflags ^ SVF_PLAYERSONLY;
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
	Cmd_AddCommand( "kill", SV_Kill_f, "die instantly" );
	Cmd_AddCommand( "status", SV_Status_f, "print server status information" );
	Cmd_AddCommand( "serverinfo", SV_ServerInfo_f, "print server settings" );
	Cmd_AddCommand( "clientinfo", SV_ClientInfo_f, "print user infostring (player num required)" );

	Cmd_AddCommand( "map", SV_Map_f, "start new level" );
	Cmd_AddCommand( "devmap", SV_Map_f, "start new level" );
	Cmd_AddCommand( "newgame", SV_Newgame_f, "begin new game" );
	Cmd_AddCommand( "endgame", SV_Endgame_f, "end current game" );
	Cmd_AddCommand( "changelevel", SV_ChangeLevel_f, "changing level" );
	Cmd_AddCommand( "restart", SV_Restart_f, "restarting current level" );
	Cmd_AddCommand( "reload", SV_Reload_f, "continue from latest save or restart level" );
	Cmd_AddCommand( "entpatch", SV_EntPatch_f, "write entity patch to allow external editing" );

	if( host.type == HOST_DEDICATED )
	{
		Cmd_AddCommand( "say", SV_ConSay_f, "send a chat message to everyone on the server" );
		Cmd_AddCommand( "setmaster", SV_SetMaster_f, "set ip address for dedicated server" );
	}

	Cmd_AddCommand( "save", SV_Save_f, "save the game to a file" );
	Cmd_AddCommand( "load", SV_Load_f, "load a saved game file" );
	Cmd_AddCommand( "savequick", SV_QuickSave_f, "save the game to the quicksave" );
	Cmd_AddCommand( "loadquick", SV_QuickLoad_f, "load a quick-saved game file" );
	Cmd_AddCommand( "killsave", SV_DeleteSave_f, "delete a saved game file and saveshot" );
	Cmd_AddCommand( "autosave", SV_AutoSave_f, "save the game to 'autosave' file" );
	Cmd_AddCommand( "killserver", SV_KillServer_f, "shutdown current server" );
	Cmd_AddCommand( "playersonly", SV_PlayersOnly_f, "freezes time, except for players" );
}

void SV_KillOperatorCommands( void )
{
	Cmd_RemoveCommand( "heartbeat" );
	Cmd_RemoveCommand( "kick" );
	Cmd_RemoveCommand( "kill" );
	Cmd_RemoveCommand( "status" );
	Cmd_RemoveCommand( "serverinfo" );
	Cmd_RemoveCommand( "clientinfo" );
	Cmd_RemoveCommand( "playersonly" );

	Cmd_RemoveCommand( "map" );
	Cmd_RemoveCommand( "movie" );
	Cmd_RemoveCommand( "newgame" );
	Cmd_RemoveCommand( "endgame" );
	Cmd_RemoveCommand( "changelevel" );
	Cmd_RemoveCommand( "restart" );
	Cmd_RemoveCommand( "reload" );
	Cmd_RemoveCommand( "entpatch" );

	if( host.type == HOST_DEDICATED )
	{
		Cmd_RemoveCommand( "say" );
		Cmd_RemoveCommand( "setmaster" );
	}

	Cmd_RemoveCommand( "save" );
	Cmd_RemoveCommand( "load" );
	Cmd_RemoveCommand( "killsave" );
	Cmd_RemoveCommand( "autosave" );
	Cmd_RemoveCommand( "killserver" );
}