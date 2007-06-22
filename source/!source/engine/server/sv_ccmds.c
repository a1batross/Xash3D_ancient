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
	if (!dedicated->value)
	{
		Com_Printf ("Only dedicated servers use masters.\n");
		return;
	}

	// make sure the server is listed public
	Cvar_Set ("public", "1");

	for (i=1 ; i<MAX_MASTERS ; i++)
		memset (&master_adr[i], 0, sizeof(master_adr[i]));

	slot = 1;		// slot 0 will always contain the id master
	for (i=1 ; i<Cmd_Argc() ; i++)
	{
		if (slot == MAX_MASTERS)
			break;

		if (!NET_StringToAdr (Cmd_Argv(i), &master_adr[i]))
		{
			Com_Printf ("Bad address: %s\n", Cmd_Argv(i));
			continue;
		}
		if (master_adr[slot].port == 0)
			master_adr[slot].port = BigShort (PORT_MASTER);

		Com_Printf ("Master server at %s\n", NET_AdrToString (master_adr[slot]));

		Com_Printf ("Sending a ping.\n");

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
		if (idnum < 0 || idnum >= maxclients->value)
		{
			Com_Printf ("Bad client slot: %i\n", idnum);
			return false;
		}

		sv_client = &svs.clients[idnum];
		sv_player = sv_client->edict;
		if (!sv_client->state)
		{
			Com_Printf ("Client %i is not active\n", idnum);
			return false;
		}
		return true;
	}

	// check for a name match
	for (i=0,cl=svs.clients ; i<maxclients->value; i++,cl++)
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

	Com_Printf ("Userid %s is not on the server\n", s);
	return false;
}


/*
===============================================================================

SAVEGAME FILES

===============================================================================
*/

/*
====================
Log_Timestamp
====================
*/
const char* CurTime( void )
{
	static char timestamp [128];
	time_t crt_time;
	const struct tm *crt_tm;
	char timestring [64];

	// Build the time stamp (ex: "Apr2007-03(23.31.55)");
	time (&crt_time);
	crt_tm = localtime (&crt_time);
	strftime (timestring, sizeof (timestring), "%b%Y-%d(%H.%M.%S)", crt_tm);
          strcpy( timestamp, timestring );

	return timestamp;
}

/*
==============
SV_WriteLevelFile

mode: 0 - regular save, 1 - autosave, 2 - quicksave
==============
*/

#define REGULAR	0
#define AUTOSAVE	1
#define QUICK	2

char* LF( int mode )
{
	static char path[MAX_SYSPATH];

	switch(mode)
	{
	case REGULAR:
		Com_sprintf (path, sizeof(path), "save/%s/%s.bin", sv.name, CurTime());	 
		break;
	case AUTOSAVE:
		Com_sprintf (path, sizeof(path), "save/%s/auto.bin", sv.name );
		break;
	case QUICK:
		Com_sprintf (path, sizeof(path), "save/quick.bin" );
		break;
	}

	return path;
}

char* SF( int mode )
{
	static char path[MAX_SYSPATH];

	switch(mode)
	{
	case REGULAR:
		Com_sprintf (path, sizeof(path), "save/%s/%s.sav", sv.name, CurTime());	 
		break;
	case AUTOSAVE:
		Com_sprintf (path, sizeof(path), "save/%s/auto.sav", sv.name );
		break;
	case QUICK:
		Com_sprintf (path, sizeof(path), "save/quick.sav" );
		break;
	}

	return path;
}

/*
==============
SV_WriteLevelFile

==============
*/
void SV_WriteLevelFile (void)
{
	char	name[MAX_OSPATH];
	file_t	*f;

	Com_DPrintf("SV_WriteLevelFile()\n");
          
	f = FS_Open(LF( QUICK ), "wb");
	if (!f)
	{
		Com_Printf ("Failed to open %s\n", name);
		return;
	}
	FS_Write (f, sv.configstrings, sizeof(sv.configstrings));
	CM_WritePortalState (f);
	ge->WriteLevel(f);

	FS_Close (f);
}

/*
==============
SV_ReadLevelFile

==============
*/
void SV_ReadLevelFile (void)
{
	char	name[MAX_OSPATH];
	file_t	*f;

	Com_DPrintf("SV_ReadLevelFile()\n");

	f = FS_Open(LF( QUICK ), "rb");
	if (!f)
	{
		Com_Printf ("Failed to open %s\n", name);
		return;
	}
	FS_Read (f, sv.configstrings, sizeof(sv.configstrings));
	CM_ReadPortalState (f);
	ge->ReadLevel(f);

	FS_Close (f);
}

/*
==============
SV_WriteServerFile

==============
*/
void SV_WriteServerFile (bool autosave)
{
	file_t	*f;
	cvar_t	*var;
	char	name[MAX_OSPATH], string[128];
	char	comment[32];
	time_t	aclock;
	struct tm	*newtime;
	int	mode;

	Com_DPrintf("SV_WriteServerFile(%s)\n", autosave ? "true" : "false");

	if(autosave)mode = AUTOSAVE;
	else mode = QUICK;
	
	f = FS_Open (SF(mode), "wb");
	if (!f)
	{
		Com_Printf ("Couldn't write %s\n", name);
		return;
	}

	// write the comment field
	memset (comment, 0, sizeof(comment));

	if (!autosave)
	{
		time (&aclock);
		newtime = localtime (&aclock);
		Com_sprintf (comment, sizeof(comment), "%s", CurTime());
		strncat (comment, sv.configstrings[CS_NAME], sizeof(comment)-1-strlen(comment) );
	}
	else
	{	// autosaved
		Com_sprintf (comment, sizeof(comment), "ENTERING %s", sv.configstrings[CS_NAME]);
	}

	FS_Write (f, comment, sizeof(comment));

	// write the mapcmd
	FS_Write (f, svs.mapcmd, sizeof(svs.mapcmd));

	// write all CVAR_LATCH cvars
	// these will be things like coop, skill, deathmatch, etc
	for (var = cvar_vars; var; var = var->next)
	{
		if (!(var->flags & CVAR_LATCH))
			continue;
		if (strlen(var->name) >= sizeof(name)-1 || strlen(var->string) >= sizeof(string)-1)
		{
			Com_Printf ("Cvar too long: %s = %s\n", var->name, var->string);
			continue;
		}

		memset (name, 0, sizeof(name));
		memset (string, 0, sizeof(string));
		strcpy (name, var->name);
		strcpy (string, var->string);
		FS_Write (f, name, sizeof(name));
		FS_Write (f, string, sizeof(string));
	}
          FS_Write(f, "§", 1 );//end of cvars

	ge->WriteGame (f, autosave);
	FS_Close (f);
}

/*
==============
SV_ReadServerFile

==============
*/
void SV_ReadServerFile (void)
{
	file_t	*f;
	char	name[MAX_OSPATH], string[128];
	char	comment[32];
	char	mapcmd[MAX_TOKEN_CHARS];

	Com_DPrintf("SV_ReadServerFile()\n");

	f = FS_Open (SF(QUICK), "rb");
	if (!f)
	{
		Com_Printf ("Couldn't read %s\n", name);
		return;
	}
	// read the comment field
	FS_Read (f, comment, sizeof(comment));

	// read the mapcmd
	FS_Read (f, mapcmd, sizeof(mapcmd));

	// read all CVAR_LATCH cvars
	// these will be things like coop, skill, deathmatch, etc
	while (1)
	{
		if (!FS_Read (f, name, sizeof(name)))
			break;
		if(name[0] == '§')
		{
			int curpos = FS_Tell(f);
                              curpos -= sizeof(name);
			curpos++;//skip § character
			FS_Seek(f, curpos, SEEK_SET);
			break;
		}
		FS_Read (f, string, sizeof(string));
		Com_DPrintf ("Set %s = %s\n", name, string);
		Cvar_ForceSet (name, string);
	}

	// start a new game fresh with new cvars
	SV_InitGame ();

	strcpy (svs.mapcmd, mapcmd);

	ge->ReadGame (f);
	FS_Close (f);
}


//=========================================================




/*
==================
SV_DemoMap_f

Puts the server in demo mode on a specific map/cinematic
==================
*/
void SV_DemoMap_f (void)
{
	SV_Map (true, Cmd_Argv(1), false );
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
		Com_Printf ("USAGE: gamemap <map>\n");
		return;
	}

	Com_DPrintf("SV_GameMap(%s)\n", Cmd_Argv(1));

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
			savedInuse = Z_Malloc(maxclients->value * sizeof(bool));
			for (i=0,cl=svs.clients ; i<maxclients->value; i++,cl++)
			{
				savedInuse[i] = cl->edict->inuse;
				cl->edict->inuse = false;
			}

			SV_WriteLevelFile ();

			// we must restore these for clients to transfer over correctly
			for (i=0,cl=svs.clients ; i<maxclients->value; i++,cl++)
				cl->edict->inuse = savedInuse[i];
			Z_Free (savedInuse);
		}
	}

	// start up the next map
	SV_Map (false, Cmd_Argv(1), false );

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
	char	*map;
	char	expanded[MAX_QPATH];

	// if not a pcx, demo, or cinematic, check to make sure the level exists
	map = Cmd_Argv(1);
	if (!strstr (map, "."))
	{
		Com_sprintf (expanded, sizeof(expanded), "maps/%s.bsp", map);
		if (!FS_LoadFile (expanded, NULL))
		{
			Com_Printf ("Can't find %s\n", expanded);
			return;
		}
	}

	sv.state = ss_dead;		// don't save current level when changing
	SV_GameMap_f ();
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
	char	*dir;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("USAGE: loadgame <directory>\n");
		return;
	}

	Com_Printf ("Loading game...\n");

	dir = Cmd_Argv(1);
	if (strstr (dir, "..") || strstr (dir, "/") || strstr (dir, "\\") )
	{
		Com_Printf ("Bad savedir.\n");
	}

	SV_ReadServerFile ();

	// go to the map
	sv.state = ss_dead;		// don't save current level when changing
	SV_Map (false, svs.mapcmd, true);
}



/*
==============
SV_Savegame_f

==============
*/
void SV_Savegame_f (void)
{
	char	*dir;

	if (sv.state != ss_game)
	{
		Com_Printf ("You must be in a game to save.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("USAGE: savegame <directory>\n");
		return;
	}

	if (Cvar_VariableValue("deathmatch"))
	{
		Com_Printf ("Can't savegame in a deathmatch\n");
		return;
	}

	if (!strcmp (Cmd_Argv(1), "current"))
	{
		Com_Printf ("Can't save to 'current'\n");
		return;
	}

	if (maxclients->value == 1 && svs.clients[0].edict->client->ps.stats[STAT_HEALTH] <= 0)
	{
		Com_Printf ("\nCan't savegame while dead!\n");
		return;
	}

	dir = Cmd_Argv(1);
	if (strstr (dir, "..") || strstr (dir, "/") || strstr (dir, "\\") )
	{
		Com_Printf ("Bad savedir.\n");
	}

	Com_Printf ("Saving game...\n");

	// archive current level, including all client edicts.
	// when the level is reloaded, they will be shells awaiting
	// a connecting client
	SV_WriteLevelFile ();

	// save server state
	SV_WriteServerFile (false);

	Com_Printf ("Done.\n");
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
		Com_Printf ("No server running.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("Usage: kick <userid>\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

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
		Com_Printf ("No server running.\n");
		return;
	}
	Com_Printf ("map              : %s\n", sv.name);

	Com_Printf ("num score ping name            lastmsg address               qport \n");
	Com_Printf ("--- ----- ---- --------------- ------- --------------------- ------\n");
	for (i=0,cl=svs.clients ; i<maxclients->value; i++,cl++)
	{
		if (!cl->state)
			continue;
		Com_Printf ("%3i ", i);
		Com_Printf ("%5i ", cl->edict->client->ps.stats[STAT_FRAGS]);

		if (cl->state == cs_connected)
			Com_Printf ("CNCT ");
		else if (cl->state == cs_zombie)
			Com_Printf ("ZMBI ");
		else
		{
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Printf ("%4i ", ping);
		}

		Com_Printf ("%s", cl->name);
		l = 16 - strlen(cl->name);
		for (j=0 ; j<l ; j++)
			Com_Printf (" ");

		Com_Printf ("%7i ", svs.realtime - cl->lastmessage );

		s = NET_AdrToString ( cl->netchan.remote_address);
		Com_Printf ("%s", s);
		l = 22 - strlen(s);
		for (j=0 ; j<l ; j++)
			Com_Printf (" ");
		
		Com_Printf ("%5i", cl->netchan.qport);

		Com_Printf ("\n");
	}
	Com_Printf ("\n");
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

	for (j = 0, client = svs.clients; j < maxclients->value; j++, client++)
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
	Com_Printf ("Server info settings:\n");
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
		Com_Printf ("Usage: info <userid>\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	Com_Printf ("userinfo\n");
	Com_Printf ("--------\n");
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
		Com_Printf ("serverrecord <demoname>\n");
		return;
	}

	if (svs.demofile)
	{
		Com_Printf ("Already recording.\n");
		return;
	}

	if (sv.state != ss_game)
	{
		Com_Printf ("You must be in a level to record.\n");
		return;
	}

	//
	// open the demo file
	//
	Com_sprintf (name, sizeof(name), "demos/%s.dm2", Cmd_Argv(1));

	Com_Printf ("recording to %s.\n", name);
	svs.demofile = FS_Open (name, "wb");
	if (!svs.demofile)
	{
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	// setup a buffer to catch all multicasts
	SZ_Init (&svs.demo_multicast, svs.demo_multicast_buf, sizeof(svs.demo_multicast_buf));

	//
	// write a single giant fake message with all the startup info
	//
	SZ_Init (&buf, buf_data, sizeof(buf_data));

	//
	// serverdata needs to go over for all types of servers
	// to make sure the protocol is right, and to set the gamedir
	//
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
		if (sv.configstrings[i][0])
		{
			MSG_WriteByte (&buf, svc_configstring);
			MSG_WriteShort (&buf, i);
			MSG_WriteString (&buf, sv.configstrings[i]);
		}

	// write it to the demo file
	Com_DPrintf ("signon message length: %i\n", buf.cursize);
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
		Com_Printf ("Not doing a serverrecord.\n");
		return;
	}
	FS_Close (svs.demofile);
	svs.demofile = NULL;
	Com_Printf ("Recording completed.\n");
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
	NET_Config ( false );	// close network sockets
}

/*
===============
SV_ServerCommand_f

Let the game dll handle a command
===============
*/
void SV_ServerCommand_f (void)
{
	if (!ge)
	{
		Com_Printf ("No game loaded.\n");
		return;
	}

	ge->ServerCommand();
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

	if ( dedicated->value )
		Cmd_AddCommand ("say", SV_ConSay_f);

	Cmd_AddCommand ("serverrecord", SV_ServerRecord_f);
	Cmd_AddCommand ("serverstop", SV_ServerStop_f);

	Cmd_AddCommand ("save", SV_Savegame_f);
	Cmd_AddCommand ("load", SV_Loadgame_f);

	Cmd_AddCommand ("killserver", SV_KillServer_f);

	Cmd_AddCommand ("sv", SV_ServerCommand_f);
}

