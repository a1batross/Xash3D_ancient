//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        sv_cmds.c - server console commands
//=======================================================================

#include "common.h"
#include "server.h"

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
	if(!dedicated->value)
	{
		Msg("Only dedicated servers use masters.\n");
		return;
	}

	// make sure the server is listed public
	Cvar_Set ("public", "1");

	for (i = 1; i < MAX_MASTERS; i++)
	{ 
		memset(&master_adr[i], 0, sizeof(master_adr[i]));
	}

	// slot 0 will always contain the id master
	for (i = 1, slot = 1; i < Cmd_Argc(); i++)
	{
		if (slot == MAX_MASTERS) break;
		if (!NET_StringToAdr (Cmd_Argv(i), &master_adr[i]))
		{
			Msg ("Bad address: %s\n", Cmd_Argv(i));
			continue;
		}

		if(!master_adr[slot].port) master_adr[slot].port = BigShort (PORT_MASTER);
		Msg ("Master server at %s\n", NET_AdrToString (master_adr[slot]));
		Msg ("Sending a ping.\n");
		Netchan_OutOfBandPrint (NS_SERVER, master_adr[slot], "ping");
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
	client_state_t	*cl;
	int		i, idnum;

	if(Cmd_Argc() < 2) return false;

	s = Cmd_Argv(1);

	// numeric values are just slot numbers
	if (s[0] >= '0' && s[0] <= '9')
	{
		idnum = atoi(Cmd_Argv(1));
		if (idnum < 0 || idnum >= maxclients->integer)
		{
			Msg("Bad client slot: %i\n", idnum);
			return false;
		}
		sv_client = &svs.clients[idnum];
		sv_player = sv_client->edict;
		if (!sv_client->state)
		{
			Msg("Client %i is not active\n", idnum);
			return false;
		}
		return true;
	}

	// check for a name match
	for (i = 0, cl = svs.clients; i < maxclients->integer; i++, cl++)
	{
		if (!cl->state) continue;
		if (!strcmp(cl->name, s))
		{
			sv_client = cl;
			sv_player = sv_client->edict;
			return true;
		}
	}
	Msg ("Userid %s is not on the server\n", s);
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
	char	filename[MAX_QPATH];

	if(Cmd_Argc() != 2)
	{
		Msg("Usage: map <filename>\n");
		return;
	}

	com.snprintf( filename, MAX_QPATH, "%s.bsp", Cmd_Argv(1));
	if(!FS_FileExists(va("maps/%s", filename )))
	{
		Msg("Can't loading %s\n", filename );
		return;
	}

	SV_InitGame(); // reset previous state

	SV_BroadcastCommand("changing\n");
	SV_SendClientMessages();
	SV_SpawnServer( filename, NULL, ss_active );
	SV_BroadcastCommand ("reconnect\n");

	// archive server state
	com.strncpy (svs.mapcmd, filename, sizeof(svs.mapcmd) - 1);
}

/*
==================
SV_Movie_f

Playing a Darkplaces video with specified name
==================
*/
void SV_Movie_f( void )
{
	char	filename[MAX_QPATH];

	if(Cmd_Argc() != 2)
	{
		Msg("Usage: movie <filename>\n");
		return;
	}

	com.snprintf( filename, MAX_QPATH, "%s.roq", Cmd_Argv(1));
	if(!FS_FileExists(va("video/%s", filename )))
	{
		Msg("Can't loading %s\n", filename );
		return;
	}

	SV_InitGame();
	SV_BroadcastCommand( "changing\n" );
	SV_SpawnServer( filename, NULL, ss_cinematic );
	SV_BroadcastCommand( "reconnect\n" );
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
	char	filename[MAX_QPATH];

	if(Cmd_Argc() != 2)
	{
		Msg ("Usage: load <filename>\n");
		return;
	}

	com.snprintf( filename, MAX_QPATH, "%s.bin", Cmd_Argv(1));
	if(!FS_FileExists(va("save/%s", filename )))
	{
		Msg("Can't loading %s\n", filename );
		return;
	}

	SV_ReadSaveFile( filename );
	SV_BroadcastCommand( "changing\n" );
	SV_SpawnServer( svs.mapcmd, filename, ss_active );
	SV_BroadcastCommand( "reconnect\n" );
}

/*
==============
SV_Save_f

==============
*/
void SV_Save_f( void )
{
	char	filename[MAX_QPATH];

	if(Cmd_Argc() != 2)
	{
		Msg ("Usage: savegame <directory>\n");
		return;
	}

	com.snprintf( filename, MAX_QPATH, "%s.bin", Cmd_Argv(1));
	SV_WriteSaveFile( filename );
}

/*
==================
SV_ChangeLevel_f

Saves the state of the map just being exited and goes to a new map.
==================
*/
void SV_ChangeLevel_f( void )
{
	char	filename[MAX_QPATH];
	int	c = Cmd_Argc();

	if( c != 2 && c != 3 )
	{
		Msg ("Usage: changelevel <map> [landmark]\n");
		return;
	}

	com.snprintf( filename, MAX_QPATH, "%s.bsp", Cmd_Argv(1));
	if(!FS_FileExists(va("maps/%s", filename )))
	{
		Msg("Can't loading %s\n", filename );
		return;
	}

	if(sv.state == ss_active)
	{
		bool		*savedFree;
		client_state_t	*cl;
		int		i;
	
		// clear all the client free flags before saving so that
		// when the level is re-entered, the clients will spawn
		// at spawn points instead of occupying body shells
		savedFree = Z_Malloc(maxclients->integer * sizeof(bool));
		for (i = 0, cl = svs.clients; i < maxclients->integer; i++, cl++)
		{
			savedFree[i] = cl->edict->priv.sv->free;
			cl->edict->priv.sv->free = true;
		}
		SV_WriteSaveFile( "save0.bin" ); // autosave
		// we must restore these for clients to transfer over correctly
		for (i = 0, cl = svs.clients; i < maxclients->integer; i++, cl++)
			cl->edict->priv.sv->free = savedFree[i];
		Mem_Free(savedFree);
	}

	SV_InitGame(); // reset previous state
	SV_BroadcastCommand("changing\n");
	SV_SendClientMessages();
	SV_SpawnServer( filename, NULL, ss_active );
	SV_BroadcastCommand ("reconnect\n");

	// archive server state
	com.strncpy (svs.mapcmd, filename, sizeof(svs.mapcmd) - 1);
}

/*
==================
SV_Restart_f

restarts current level
==================
*/
void SV_Restart_f( void )
{
	char	filename[MAX_QPATH];
	
	if(sv.state != ss_active) return;

	strncpy( filename, svs.mapcmd, MAX_QPATH );
	FS_StripExtension( filename );

	// just sending console command
	Cbuf_AddText(va("map %s\n", filename ));
}

/*
==================
SV_Kick_f

Kick a user off of the server
==================
*/
void SV_Kick_f( void )
{
	if(Cmd_Argc() != 2)
	{
		Msg ("Usage: kick <userid>\n");
		return;
	}

	if(!svs.clients)
	{
		Msg("^3no server running.\n");
		return;
	}
	if(!SV_SetPlayer()) return;

	SV_BroadcastPrintf (PRINT_CONSOLE, "%s was kicked\n", sv_client->name);
	SV_ClientPrintf(sv_client, PRINT_CONSOLE, "You were kicked from the game\n");
	SV_DropClient(sv_client);
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
	client_state_t	*cl;

	if(!svs.clients)
	{
		Msg ("^3no server running.\n");
		return;
	}

	Msg("map: %s\n", sv.name);
	Msg("num score ping    name            lastmsg address               port \n");
	Msg("--- ----- ------- --------------- ------- --------------------- ------\n");

	for(i = 0, cl = svs.clients; i < maxclients->integer; i++, cl++)
	{
		int	j, l, ping;
		char	*s;

		if (!cl->state) continue;

		Msg("%3i ", i);
		Msg("%5i ", cl->edict->priv.sv->client->ps.stats[STAT_FRAGS]);

		if (cl->state == cs_connected) Msg("Connect");
		else if (cl->state == cs_zombie) Msg ("Zombie ");
		else
		{
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Msg("%7i ", ping);
		}

		Msg("%s", cl->name );
		l = 16 - com.strlen(cl->name);
		for (j = 0; j < l; j++) Msg (" ");
		Msg ("%7i ", svs.realtime - cl->lastmessage );
		s = NET_AdrToString ( cl->netchan.remote_address);
		Msg ("%s", s);
		l = 22 - strlen(s);
		for (j = 0; j < l; j++) Msg (" ");
		Msg("%5i", cl->netchan.qport);
		Msg("\n");
	}
	Msg ("\n");
}

/*
==================
SV_ConSay_f
==================
*/
void SV_ConSay_f( void )
{
	char		*p, text[MAX_SYSPATH];
	client_state_t	*client;
	int		i;

	if(Cmd_Argc() < 2) return;

	strncpy(text, "console: ", MAX_SYSPATH );
	p = Cmd_Args();

	if(*p == '"')
	{
		p++;
		p[com.strlen(p) - 1] = 0;
	}
	com.strncat(text, p, MAX_SYSPATH );

	for (i = 0, client = svs.clients; i < maxclients->integer; i++, client++)
	{
		if (client->state != cs_spawned) continue;
		SV_ClientPrintf(client, PRINT_CHAT, "%s\n", text );
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
	if(!svs.initialized) return;
	com.strncpy( host.finalmsg, "Server was killed\n", MAX_STRING );
	SV_Shutdown( false );
	NET_Config( false );// close network sockets
}

/*
==================
SV_InitOperatorCommands
==================
*/
void SV_InitOperatorCommands( void )
{
	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f, "send a heartbeat to the master server" );
	Cmd_AddCommand ("kick", SV_Kick_f, "kick a player off the server by number or name" );
	Cmd_AddCommand ("status", SV_Status_f, "print server status information" );
	Cmd_AddCommand ("serverinfo", SV_ServerInfo_f, "print server settings" );
	Cmd_AddCommand ("clientinfo", SV_ClientInfo_f, "print user infostring (player num required)" );

	Cmd_AddCommand("map", SV_Map_f, "start new level" );
	Cmd_AddCommand("newgame", SV_Newgame_f, "begin new game" );
	Cmd_AddCommand("movie", SV_Movie_f, "playing video file" );
	Cmd_AddCommand("changelevel", SV_ChangeLevel_f, "changing level" );
	Cmd_AddCommand("restart", SV_Restart_f, "restarting current level" );
	Cmd_AddCommand("sectorlist", SV_SectorList_f, "display pvs sectors" );

	if( dedicated->value ) 
	{
		Cmd_AddCommand ("say", SV_ConSay_f, "send a chat message to everyone on the server" );
		Cmd_AddCommand("setmaster", SV_SetMaster_f, "set ip address for dedicated server" );
	}

	Cmd_AddCommand ("save", SV_Save_f, "save the game to a file");
	Cmd_AddCommand ("load", SV_Load_f, "load a saved game file" );
	Cmd_AddCommand ("killserver", SV_KillServer_f, "shutdown current server" );
}

void SV_KillOperatorCommands( void )
{
	Cmd_RemoveCommand("heartbeat");
	Cmd_RemoveCommand("kick");
	Cmd_RemoveCommand("status");
	Cmd_RemoveCommand("serverinfo");
	Cmd_RemoveCommand("clientinfo");

	Cmd_RemoveCommand("map");
	Cmd_RemoveCommand("movie");
	Cmd_RemoveCommand("newgame");
	Cmd_RemoveCommand("changelevel");
	Cmd_RemoveCommand("restart");
	Cmd_RemoveCommand("sectorlist");

	if( dedicated->integer ) 
	{
		Cmd_RemoveCommand("say");
		Cmd_RemoveCommand("setmaster");
	}

	Cmd_RemoveCommand("save");
	Cmd_RemoveCommand("load");
	Cmd_RemoveCommand("killserver");
}