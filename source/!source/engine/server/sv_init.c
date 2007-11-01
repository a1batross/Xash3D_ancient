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

server_static_t	svs;	// persistant server info
server_t		sv;	// local server

/*
================
SV_FindIndex

================
*/
int SV_FindIndex (const char *name, int start, int end, bool create)
{
	int		i = 0;
	
	if (!name || !name[0]) return 0;

	for (i = 1; i < end && sv.configstrings[start+i][0]; i++)
		if(!strcmp(sv.configstrings[start+i], name))
			return i;
	if(!create) return 0;

	if (i == end) 
	{
		MsgWarn ("SV_FindIndex: %d out of range [%d - %d]\n", start, end );
		return 0;
	}

	// register new resource
	strncpy (sv.configstrings[start+i], name, sizeof(sv.configstrings[i]));

	if (sv.state != ss_loading)
	{	
		// send the update to everyone
		SZ_Clear (&sv.multicast);
		MSG_Begin(svc_configstring);
		MSG_WriteShort (&sv.multicast, start + i);
		MSG_WriteString (&sv.multicast, (char *)name);
		MSG_Send(MSG_ALL_R, vec3_origin, NULL );
	}
	return i;
}


int SV_ModelIndex (const char *name)
{
	return SV_FindIndex (name, CS_MODELS, MAX_MODELS, true);
}

int SV_SoundIndex (const char *name)
{
	return SV_FindIndex (name, CS_SOUNDS, MAX_SOUNDS, true);
}

int SV_ImageIndex (const char *name)
{
	return SV_FindIndex (name, CS_IMAGES, MAX_IMAGES, true);
}

int SV_DecalIndex (const char *name)
{
	return SV_FindIndex (name, CS_DECALS, MAX_DECALS, true);
}

/*
================
SV_CreateBaseline

Entity baselines are used to compress the update messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
void SV_CreateBaseline (void)
{
	edict_t			*svent;
	int			entnum;	

	for (entnum = 1; entnum < prog->num_edicts ; entnum++)
	{
		svent = PRVM_EDICT_NUM(entnum);
		if (svent->priv.sv->free) continue;
		if (!svent->progs.sv->modelindex && !svent->progs.sv->noise3 && !svent->progs.sv->effects)
			continue;
		svent->priv.sv->serialnumber = entnum;

		//
		// take current state as baseline
		//
		VectorCopy (svent->progs.sv->origin, svent->progs.sv->old_origin);
		SV_UpdateEntityState( svent );

		sv.baselines[entnum] = svent->priv.sv->s;
	}
}


/*
=================
SV_CheckForSavegame
=================
*/
void SV_CheckForSavegame (char *savename )
{
	char		name[MAX_SYSPATH];

	if (sv_noreload->value) return;
	if (Cvar_VariableValue ("deathmatch")) return;
	if (!savename) return;

	sprintf (name, "save/%s.bin", savename );
	if(!FS_FileExists(name))
	{
		Msg("can't find %s\n", savename );
		return;
	}
	SV_ClearWorld ();

	// get configstrings and areaportals
	SV_ReadLevelFile ( savename );
}


/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.

================
*/
void SV_SpawnServer (char *server, char *spawnpoint, char *savename, server_state_t serverstate, bool attractloop, bool loadgame)
{
	uint	i, checksum;

	if (attractloop) Cvar_Set ("paused", "0");

	Msg("------- Server Initialization -------\n");
	MsgDev (D_INFO, "SpawnServer: %s\n", server);
	if (sv.demofile) FS_Close (sv.demofile);

	svs.spawncount++; // any partially connected client will be restarted
	sv.state = ss_dead;
	Com_SetServerState (sv.state);

	// wipe the entire per-level structure
	memset (&sv, 0, sizeof(sv));
	svs.realtime = 0;
	sv.loadgame = loadgame;
	sv.attractloop = attractloop;

	// save name for levels that don't set message
	strcpy (sv.configstrings[CS_NAME], server);
	if (Cvar_VariableValue ("deathmatch"))
	{
		sprintf(sv.configstrings[CS_AIRACCEL], "%g", sv_airaccelerate->value);
		pm_airaccelerate = sv_airaccelerate->value;
	}
	else
	{
		strcpy(sv.configstrings[CS_AIRACCEL], "0");
		pm_airaccelerate = 0;
	}

	SZ_Init (&sv.multicast, sv.multicast_buf, sizeof(sv.multicast_buf));
	strcpy (sv.name, server);

	SV_VM_Begin();

	// leave slots at start for clients only
	for (i = 0; i < maxclients->value; i++)
	{
		// needs to reconnect
		if (svs.clients[i].state > cs_connected)
			svs.clients[i].state = cs_connected;
		svs.clients[i].lastframe = -1;
	}

	sv.time = 1.0f;
	
	strcpy (sv.name, server);
	strcpy (sv.configstrings[CS_NAME], server);

	if (serverstate != ss_game)
	{
		sv.models[1] = CM_LoadMap ("", false, &checksum);	// no real map
	}
	else
	{
		sprintf (sv.configstrings[CS_MODELS+1], "maps/%s.bsp", server);
		sv.models[1] = CM_LoadMap (sv.configstrings[CS_MODELS+1], false, &checksum);
	}
	sprintf (sv.configstrings[CS_MAPCHECKSUM],"%i", checksum);

	// clear physics interaction links
	SV_ClearWorld ();

	for (i = 1; i < CM_NumInlineModels(); i++)
	{
		sprintf(sv.configstrings[CS_MODELS+1+i], "*%i", i);
		sv.models[i+1] = CM_InlineModel (sv.configstrings[CS_MODELS+1+i]);
	}

	//
	// spawn the rest of the entities on the map
	//	

	// precache and static commands can be issued during
	// map initialization
	sv.state = ss_loading;
	Com_SetServerState (sv.state);

	// load and spawn all other entities
	SV_SpawnEntities ( sv.name, CM_EntityString(), spawnpoint );

	// run two frames to allow everything to settle
	SV_RunFrame ();
	SV_RunFrame ();

	// all precaches are complete
	sv.state = serverstate;
	Com_SetServerState (sv.state);

	// create a baseline for more efficient communications
	SV_CreateBaseline ();

	// check for a savegame
	SV_CheckForSavegame ( savename );

	// set serverinfo variable
	Cvar_FullSet ("mapname", sv.name, CVAR_SERVERINFO | CVAR_NOSET);

	Msg ("-------------------------------------\n");
	SV_VM_End();
}

/*
==============
SV_InitGame

A brand new game has been started
==============
*/
void SV_InitGame (void)
{
	char		i, idmaster[32];
	edict_t		*ent;
	
	if (svs.initialized)
	{
		// cause any connected clients to reconnect
		SV_Shutdown ("Server restarted\n", true);
	}
	else
	{
		// make sure the client is down
		CL_Drop ();
		SCR_BeginLoadingPlaque ();
	}
          
	// get any latched variable changes (maxclients, etc)
	Cvar_GetLatchedVars ();

	svs.initialized = true;

	if (Cvar_VariableValue ("coop") && Cvar_VariableValue ("deathmatch"))
	{
		Msg("Deathmatch and Coop both set, disabling Coop\n");
		Cvar_FullSet ("coop", "0",  CVAR_SERVERINFO | CVAR_LATCH);
	}

	// dedicated servers are can't be single player and are usually DM
	// so unless they explicity set coop, force it to deathmatch
	if (dedicated->value)
	{
		if (!Cvar_VariableValue ("coop"))
			Cvar_FullSet ("deathmatch", "1",  CVAR_SERVERINFO | CVAR_LATCH);
	}

	// init clients
	if (Cvar_VariableValue ("deathmatch"))
	{
		if (maxclients->value <= 1)
			Cvar_FullSet ("maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH);
		else if (maxclients->value > MAX_CLIENTS)
			Cvar_FullSet ("maxclients", va("%i", MAX_CLIENTS), CVAR_SERVERINFO | CVAR_LATCH);
	}
	else if (Cvar_VariableValue ("coop"))
	{
		if (maxclients->value <= 1 || maxclients->value > 4)
			Cvar_FullSet ("maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
	}
	else	// non-deathmatch, non-coop is one player
	{
		Cvar_FullSet ("maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH);
	}

	svs.spawncount = rand();
	svs.clients = Z_Malloc (sizeof(client_t)*maxclients->value);
	svs.num_client_entities = maxclients->value*UPDATE_BACKUP*64;
	svs.client_entities = Z_Malloc (sizeof(entity_state_t)*svs.num_client_entities);
	svs.gclients = Z_Malloc(sizeof(gclient_t)*maxclients->value);

	// init network stuff
	NET_Config ( (maxclients->value > 1) );

	// heartbeats will always be sent to the id master
	svs.last_heartbeat = -99999.0f; // send immediately
	sprintf(idmaster, "192.246.40.37:%i", PORT_MASTER);
	NET_StringToAdr (idmaster, &master_adr[0]);

	// init game
	SV_InitGameProgs();

	SV_VM_Begin();

	for (i = 0; i < maxclients->value; i++)
	{
		ent = PRVM_EDICT_NUM(i + 1);
		ent->priv.sv->serialnumber = i + 1;
		svs.clients[i].edict = ent;
		memset (&svs.clients[i].lastcmd, 0, sizeof(svs.clients[i].lastcmd));
	}

	SV_VM_End();
}


/*
======================
SV_Map

  the full syntax is:

  map [*]<map>$<startspot>+<nextserver>

command from the console or progs.
Map can also be a.cin, .pcx, or .dm2 file
Nextserver is used to allow a cinematic to play, then proceed to
another level:

	map tram.cin+jail_e3
======================
*/
void SV_Map (bool attractloop, char *levelstring, char *savename, bool loadgame)
{
	char	*ch;
	int	l;
	char	level[MAX_QPATH], spawnpoint[MAX_QPATH];
	const char *ext = FS_FileExtension(levelstring);

	sv.loadgame = loadgame;
	sv.attractloop = attractloop;

	if (sv.state == ss_dead && !sv.loadgame) SV_InitGame ();// the game is just starting

	strcpy (level, levelstring);

	// if there is a + in the map, set nextserver to the remainder
	ch = strstr(level, "+");
	if (ch)
	{
		*ch = 0;
		Cvar_Set ("nextserver", va("gamemap \"%s\"", ch + 1));
	}
	else Cvar_Set ("nextserver", "");

	//ZOID special hack for end game screen in coop mode
	if (Cvar_VariableValue ("coop") && !strcasecmp(level, "victory.pcx"))
		Cvar_Set ("nextserver", "gamemap \"*base1\"");

	// if there is a $, use the remainder as a spawnpoint
	ch = strstr(level, "$");
	if (ch)
	{
		*ch = 0;
		strcpy (spawnpoint, ch + 1);
	}
	else spawnpoint[0] = 0;
          
	// skip the end-of-unit flag if necessary
	if (level[0] == '*') strcpy (level, level+1);

	l = strlen(level);
	if (!strcmp(ext, "roq"))
	{
		SCR_BeginLoadingPlaque (); // for local system
		SV_BroadcastCommand ("changing\n");
		SV_SpawnServer (level, spawnpoint, NULL, ss_cinematic, attractloop, loadgame);
	}
	else if (!strcmp(ext, "dm2"))
	{
		SCR_BeginLoadingPlaque (); // for local system
		SV_BroadcastCommand ("changing\n");
		SV_SpawnServer (level, spawnpoint, NULL, ss_demo, attractloop, loadgame);
	}
	else
	{
		SCR_BeginLoadingPlaque (); // for local system
		SV_BroadcastCommand ("changing\n");
		SV_SendClientMessages ();
		SV_SpawnServer (level, spawnpoint, savename, ss_game, attractloop, loadgame);
		Cbuf_CopyToDefer ();
	}
	SV_BroadcastCommand ("reconnect\n");
}

void SV_VM_BeginIncreaseEdicts(void)
{
	int		i;
	edict_t		*ent;

	PRVM_Free( sv.moved_edicts );
	sv.moved_edicts = (edict_t **)PRVM_Alloc(prog->max_edicts * sizeof(edict_t *));

	// links don't survive the transition, so unlink everything
	for (i = 0, ent = prog->edicts; i < prog->max_edicts; i++, ent++)
	{
		if (!ent->priv.sv->free) SV_UnlinkEdict(prog->edicts + i); //free old entity
		memset(&ent->priv.sv->clusternums, 0, sizeof(ent->priv.sv->clusternums));
	}
	SV_ClearWorld();
}

void SV_VM_EndIncreaseEdicts(void)
{
	int		i;
	edict_t		*ent;

	for (i = 0, ent = prog->edicts; i < prog->max_edicts; i++, ent++)
	{
		// link every entity except world
		if (!ent->priv.sv->free) SV_LinkEdict(ent);
	}
}

void SV_VM_InitEdict(edict_t *e)
{
	SV_InitEdict( e );
}

void SV_VM_FreeEdict(edict_t *e)
{
	SV_UnlinkEdict(e);	// unlink from world bsp
	SV_FreeEdict( e );
}

void SV_VM_CountEdicts( void )
{
	int		i;
	edict_t	*ent;
	int		active = 0, models = 0, solid = 0, step = 0;

	for (i = 0; i < prog->num_edicts; i++)
	{
		ent = PRVM_EDICT_NUM(i);
		if (ent->priv.sv->free)
			continue;
		active++;
		if (ent->progs.sv->solid) solid++;
		if (ent->progs.sv->model) models++;
		if (ent->progs.sv->movetype == MOVETYPE_STEP) step++;
	}

	Msg("num_edicts:%3i\n", prog->num_edicts);
	Msg("active    :%3i\n", active);
	Msg("view      :%3i\n", models);
	Msg("touch     :%3i\n", solid);
	Msg("step      :%3i\n", step);
}

bool SV_VM_LoadEdict(edict_t *ent)
{
	int current_skill = (int)Cvar_VariableValue ("skill");

	// remove things from different skill levels or deathmatch
	if(Cvar_VariableValue ("deathmatch"))
	{
		if (((int)ent->progs.sv->spawnflags & SPAWNFLAG_NOT_DEATHMATCH))
		{
			return false;
		}
	}
	else if ((current_skill <= 0 && ((int)ent->progs.sv->spawnflags & SPAWNFLAG_NOT_EASY  )) || (current_skill == 1 && ((int)ent->progs.sv->spawnflags & SPAWNFLAG_NOT_MEDIUM)) || (current_skill >= 2 && ((int)ent->progs.sv->spawnflags & SPAWNFLAG_NOT_HARD  )))
	{
		return false;
	}
	return true;
}

void SV_VM_Setup( void )
{
	PRVM_Begin;
	PRVM_InitProg( PRVM_SERVERPROG );
        
	// allocate the mempools
	// TODO: move the magic numbers/constants into #defines [9/13/2006 Black]
	prog->progs_mempool = Mem_AllocPool("Server Progs" );
	prog->builtins = vm_sv_builtins;
	prog->numbuiltins = vm_sv_numbuiltins;
	prog->max_edicts = 512;
	prog->limit_edicts = MAX_EDICTS;
	prog->reserved_edicts = maxclients->value;
	prog->edictprivate_size = sizeof(sv_edict_t);
	prog->name = "server";
	prog->extensionstring = "";
	prog->loadintoworld = true;

	prog->begin_increase_edicts = SV_VM_BeginIncreaseEdicts;
	prog->end_increase_edicts = SV_VM_EndIncreaseEdicts;
	prog->init_edict = SV_VM_InitEdict;
	prog->free_edict = SV_VM_FreeEdict;
	prog->count_edicts = SV_VM_CountEdicts;
	prog->load_edict = SV_VM_LoadEdict;
	prog->init_cmd = VM_Cmd_Init;
	prog->reset_cmd = VM_Cmd_Reset;
	prog->error_cmd = VM_Error;

	// TODO: add a requiredfuncs list (ask LH if this is necessary at all)
	PRVM_LoadProgs( "server.dat", 0, NULL, REQFIELDS, reqfields );
	PRVM_End;
}

void SV_VM_Begin(void)
{
	PRVM_Begin;
	PRVM_SetProg( PRVM_SERVERPROG );

	*prog->time = sv.time;
}

void SV_VM_End(void)
{
	PRVM_End;
}