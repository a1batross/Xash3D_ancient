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

#include "engine.h"
#include "server.h"
#include "savefile.h"

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

/*
====================
SV_SetMaster_f

Specify a list of master servers
====================
*/
void SV_SetMaster_f (void)
{
	int		i, slot;

	// only dedicated servers send heartbeats
	if (host.type == HOST_NORMAL)
	{
		Msg ("Only dedicated servers use masters.\n");
		return;
	}

	// make sure the server is listed public
	Cvar_Set ("public", "1");

	for (i = 1; i < MAX_MASTERS; i++) memset (&master_adr[i], 0, sizeof(master_adr[i]));

	slot = 1;	// slot 0 will always contain the id master

	for (i = 1; i < Cmd_Argc(); i++)
	{
		if (slot == MAX_MASTERS)
			break;

		if (!NET_StringToAdr (Cmd_Argv(i), &master_adr[i]))
		{
			Msg ("Bad address: %s\n", Cmd_Argv(i));
			continue;
		}
		if (master_adr[slot].port == 0)
			master_adr[slot].port = BigShort (PORT_MASTER);

		Msg ("Master server at %s\n", NET_AdrToString (master_adr[slot]));

		Msg ("Sending a ping.\n");

		Netchan_OutOfBandPrint (NS_SERVER, master_adr[slot], "ping");

		slot++;
	}

	svs.last_heartbeat = -9999999;
}



/*
==================
SV_SetPlayer

Sets sv_client and sv_player to the player with idnum Cmd_Argv(1)
==================
*/
bool SV_SetPlayer (void)
{
	client_t	*cl;
	int			i;
	int			idnum;
	char		*s;

	if (Cmd_Argc() < 2)
		return false;

	s = Cmd_Argv(1);

	// numeric values are just slot numbers
	if (s[0] >= '0' && s[0] <= '9')
	{
		idnum = atoi(Cmd_Argv(1));
		if (idnum < 0 || idnum >= host.maxclients)
		{
			Msg ("Bad client slot: %i\n", idnum);
			return false;
		}

		sv_client = &svs.clients[idnum];
		sv_player = sv_client->edict;
		if (!sv_client->state)
		{
			Msg ("Client %i is not active\n", idnum);
			return false;
		}
		return true;
	}

	// check for a name match
	for (i=0,cl=svs.clients ; i < host.maxclients; i++,cl++)
	{
		if (!cl->state)
			continue;
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
SV_DemoMap_f

Puts the server in demo mode on a specific map/cinematic
==================
*/
void SV_DemoMap_f (void)
{
	SV_Map (true, Cmd_Argv(1), NULL, false );
}

/*
==================
SV_GameMap_f

Saves the state of the map just being exited and goes to a new map.

If the initial character of the map string is '*', the next map is
in a new unit, so the current savegame directory is cleared of
map files.

Example:

*inter.cin+jail

Clears the archived maps, plays the inter.cin cinematic, then
goes to map jail.bsp.
==================
*/
void SV_GameMap_f (void)
{
	char		*map;
	int			i;
	client_t	*cl;
	bool	*savedInuse;

	if (Cmd_Argc() != 2)
	{
		Msg ("USAGE: gamemap <map>\n");
		return;
	}

	// check for clearing the current savegame
	map = Cmd_Argv(1);
	if (map[0] != '*')
	{
		// save the map just exited
		if (sv.state == ss_game)
		{
			// clear all the client inuse flags before saving so that
			// when the level is re-entered, the clients will spawn
			// at spawn points instead of occupying body shells
			savedInuse = Z_Malloc(host.maxclients * sizeof(bool));
			for (i = 0, cl = svs.clients; i < host.maxclients; i++, cl++)
			{
				savedInuse[i] = cl->edict->priv.sv->free;
				cl->edict->priv.sv->free = true;
			}

			SV_WriteSaveFile( "save0" ); //autosave

			// we must restore these for clients to transfer over correctly
			for (i = 0, cl = svs.clients; i < host.maxclients; i++, cl++)
				cl->edict->priv.sv->free = savedInuse[i];
			Z_Free (savedInuse);
		}
	}

	// start up the next map
	SV_Map (false, Cmd_Argv(1), NULL, false );

	// archive server state
	strncpy (svs.mapcmd, Cmd_Argv(1), sizeof(svs.mapcmd)-1);
}

/*
==================
SV_Map_f

Goes directly to a given map without any savegame archiving.
For development work
==================
*/
void SV_Map_f (void)
{
	char	level_path[MAX_QPATH];

	sprintf(level_path, "maps/%s", Cmd_Argv(1));
	FS_DefaultExtension(level_path, ".bsp" ); 

	if (FS_FileExists(level_path))
	{
		sv.state = ss_dead;	// don't save current level when changing

		SV_InitGame ();
		Cvar_Set ("nextserver", ""); //reset demoloop

		SCR_BeginLoadingPlaque (); // for local system
		SV_BroadcastCommand ("changing\n");

		SV_SendClientMessages ();
		SV_SpawnServer (level_path, NULL, NULL, ss_game, false, false);
		Cbuf_CopyToDefer ();
//FIXME//SV_BroadcastCommand ("reconnect\n");
		strncpy (svs.mapcmd, Cmd_Argv(1), sizeof(svs.mapcmd) - 1); // archive server state
	}
	else Msg ("Can't loading %s\n", level_path);
}

/*
=====================================================================

  SAVEGAMES

=====================================================================
*/


/*
==============
SV_Loadgame_f

==============
*/
void SV_Loadgame_f (void)
{
	if (Cmd_Argc() != 2)
	{
		Msg ("USAGE: loadgame <directory>\n");
		return;
	}

	Msg("Loading game... %s\n", Cmd_Argv(1));
	SV_ReadSaveFile( Cmd_Argv(1) );

	// go to the map
	sv.state = ss_dead;		// don't save current level when changing
	SV_Map (false, svs.mapcmd, Cmd_Argv(1), true);
}



/*
==============
SV_Savegame_f

==============
*/
void SV_Savegame_f (void)
{
	if (sv.state != ss_game)
	{
		Msg ("You must be in a game to save.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Msg ("USAGE: savegame <directory>\n");
		return;
	}

	if (Cvar_VariableValue("deathmatch"))
	{
		Msg ("Can't savegame in a deathmatch\n");
		return;
	}

	if (!strcmp (Cmd_Argv(1), "current"))
	{
		Msg ("Can't save to 'current'\n");
		return;
	}

	if (host.maxclients == 1 && svs.clients[0].edict->priv.sv->client->stats[STAT_HEALTH] <= 0)
	{
		Msg ("\nCan't savegame while dead!\n");
		return;
	}

	// archive current level, including all client edicts.
	// when the level is reloaded, they will be shells awaiting
	// a connecting client
	SV_WriteSaveFile( Cmd_Argv(1) );

	Msg ("Done.\n");
}

//===============================================================

/*
==================
SV_Kick_f

Kick a user off of the server
==================
*/
void SV_Kick_f (void)
{
	if (!svs.initialized)
	{
		Msg ("No server running.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Msg ("Usage: kick <userid>\n");
		return;
	}

	if (!SV_SetPlayer ()) return;

	SV_BroadcastPrintf (PRINT_HIGH, "%s was kicked\n", sv_client->name);
	// print directly, because the dropped client won't get the
	// SV_BroadcastPrintf message
	SV_ClientPrintf (sv_client, PRINT_HIGH, "You were kicked from the game\n");
	SV_DropClient (sv_client);
	sv_client->lastmessage = svs.realtime;	// min case there is a funny zombie
}


/*
================
SV_Status_f
================
*/
void SV_Status_f (void)
{
	int			i, j, l;
	client_t	*cl;
	char		*s;
	int			ping;
	if (!svs.clients)
	{
		Msg ("No server running.\n");
		return;
	}
	Msg ("map              : %s\n", sv.name);

	Msg ("num score ping name            lastmsg address               qport \n");
	Msg ("--- ----- ---- --------------- ------- --------------------- ------\n");
	for (i = 0, cl = svs.clients; i < host.maxclients; i++, cl++)
	{
		if (!cl->state) continue;
		Msg ("%3i ", i);
		Msg ("%5i ", cl->edict->priv.sv->client->stats[STAT_FRAGS]);

		if (cl->state == cs_connected)
			Msg ("CNCT ");
		else if (cl->state == cs_zombie)
			Msg ("ZMBI ");
		else
		{
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Msg ("%4i ", ping);
		}

		Msg ("%s", cl->name);
		l = 16 - strlen(cl->name);
		for (j=0 ; j<l ; j++)
			Msg (" ");

		Msg ("%g ", svs.realtime - cl->lastmessage );

		s = NET_AdrToString ( cl->netchan.remote_address);
		Msg ("%s", s);
		l = 22 - strlen(s);
		for (j=0 ; j<l ; j++)
			Msg (" ");
		
		Msg ("%5i", cl->netchan.qport);

		Msg ("\n");
	}
	Msg ("\n");
}

/*
==================
SV_ConSay_f
==================
*/
void SV_ConSay_f(void)
{
	client_t *client;
	int		j;
	char	*p;
	char	text[1024];

	if (Cmd_Argc () < 2)
		return;

	strcpy (text, "console: ");
	p = Cmd_Args();

	if (*p == '"')
	{
		p++;
		p[strlen(p)-1] = 0;
	}

	strcat(text, p);

	for (j = 0, client = svs.clients; j < host.maxclients; j++, client++)
	{
		if (client->state != cs_spawned)
			continue;
		SV_ClientPrintf(client, PRINT_CHAT, "%s\n", text);
	}
}


/*
==================
SV_Heartbeat_f
==================
*/
void SV_Heartbeat_f (void)
{
	svs.last_heartbeat = -9999999;
}


/*
===========
SV_Serverinfo_f

  Examine or change the serverinfo string
===========
*/
void SV_Serverinfo_f (void)
{
	Msg ("Server info settings:\n");
	Info_Print (Cvar_Serverinfo());
}


/*
===========
SV_DumpUser_f

Examine all a users info strings
===========
*/
void SV_DumpUser_f (void)
{
	if (Cmd_Argc() != 2)
	{
		Msg ("Usage: info <userid>\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	Msg ("userinfo\n");
	Msg ("--------\n");
	Info_Print (sv_client->userinfo);

}


/*
==============
SV_ServerRecord_f

Begins server demo recording.  Every entity and every message will be
recorded, but no playerinfo will be stored.  Primarily for demo merging.
==============
*/
void SV_ServerRecord_f (void)
{
	char	name[MAX_OSPATH];
	char	buf_data[32768];
	sizebuf_t	buf;
	int		len;
	int		i;

	if (Cmd_Argc() != 2)
	{
		Msg ("serverrecord <demoname>\n");
		return;
	}

	if (svs.demofile)
	{
		Msg ("Already recording.\n");
		return;
	}

	if (sv.state != ss_game)
	{
		Msg ("You must be in a level to record.\n");
		return;
	}

	// open the demo file
	sprintf (name, "demos/%s.dm2", Cmd_Argv(1));

	Msg ("recording to %s.\n", name);
	svs.demofile = FS_Open (name, "wb");
	if (!svs.demofile)
	{
		Msg ("ERROR: couldn't open.\n");
		return;
	}

	// setup a buffer to catch all multicasts
	SZ_Init (&svs.demo_multicast, svs.demo_multicast_buf, sizeof(svs.demo_multicast_buf));

	// write a single giant fake message with all the startup info
	SZ_Init (&buf, buf_data, sizeof(buf_data));

	// serverdata needs to go over for all types of servers
	// to make sure the protocol is right, and to set the gamedir

	// send the serverdata
	MSG_WriteByte (&buf, svc_serverdata);
	MSG_WriteLong (&buf, PROTOCOL_VERSION);
	MSG_WriteLong (&buf, svs.spawncount);
	// 2 means server demo
	MSG_WriteByte (&buf, 2);	// demos are always attract loops
	MSG_WriteString (&buf, Cvar_VariableString ("gamedir"));
	MSG_WriteShort (&buf, -1);
	// send full levelname
	MSG_WriteString (&buf, sv.configstrings[CS_NAME]);

	for (i=0 ; i<MAX_CONFIGSTRINGS ; i++)
	{
		if (sv.configstrings[i][0])
		{
			MSG_WriteByte (&buf, svc_configstring);
			MSG_WriteShort (&buf, i);
			MSG_WriteString (&buf, sv.configstrings[i]);
		}
	}

	// write it to the demo file
	len = LittleLong (buf.cursize);
	FS_Write (svs.demofile, &len, 4);
	FS_Write (svs.demofile, buf.data, buf.cursize);

	// the rest of the demo file will be individual frames
}


/*
==============
SV_ServerStop_f

Ends server demo recording
==============
*/
void SV_ServerStop_f (void)
{
	if (!svs.demofile)
	{
		Msg ("Not doing a record.\n");
		return;
	}
	FS_Close (svs.demofile);
	svs.demofile = NULL;
	Msg ("Completed demo.\n");
}


/*
===============
SV_KillServer_f

Kick everyone off, possibly in preparation for a new game

===============
*/
void SV_KillServer_f (void)
{
	if (!svs.initialized) return;
	SV_Shutdown ("Server was killed.\n", false);
	NET_Config ( false ); // close network sockets
}

//===========================================================

/*
==================
SV_InitOperatorCommands
==================
*/
void SV_InitOperatorCommands (void)
{
	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);
	Cmd_AddCommand ("kick", SV_Kick_f);
	Cmd_AddCommand ("status", SV_Status_f);
	Cmd_AddCommand ("serverinfo", SV_Serverinfo_f);
	Cmd_AddCommand ("dumpuser", SV_DumpUser_f);

	Cmd_AddCommand ("map", SV_Map_f);
	Cmd_AddCommand ("demomap", SV_DemoMap_f);
	Cmd_AddCommand ("gamemap", SV_GameMap_f);
	Cmd_AddCommand ("setmaster", SV_SetMaster_f);

	if (host.type == HOST_DEDICATED) 
	{
		Cmd_AddCommand ("say", SV_ConSay_f);
	}

	Cmd_AddCommand ("serverrecord", SV_ServerRecord_f);
	Cmd_AddCommand ("serverstop", SV_ServerStop_f);
	Cmd_AddCommand ("save", SV_Savegame_f);
	Cmd_AddCommand ("load", SV_Loadgame_f);
	Cmd_AddCommand ("killserver", SV_KillServer_f);
}

