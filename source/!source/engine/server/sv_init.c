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

server_static_t	svs;				// persistant server info
server_t		sv;					// local server

#define REQFIELDS (sizeof(reqfields) / sizeof(prvm_fieldvars_t))

prvm_fieldvars_t reqfields[] = 
{
	{0,	2,	"modelindex"},
	{1,	3,	"absmin"},
	{1,	2,	"absmin_x"},
	{2,	2,	"absmin_y"},
	{3,	2,	"absmin_z"},
	{4,	3,	"absmax"},
	{7,	2,	"ltime"},
	{8,	2,	"movetype"},
	{9,	2,	"solid"},
	{10,	3,	"origin"},
	{13,	3,	"oldorigin"},
	{16,	3,	"velocity"},
	{19,	3,	"angles"},
	{22,	3,	"avelocity"},
	{25,	3,	"punchangle"},
	{28,	1,	"classname"},
	{29,	1,	"model"},
	{30,	2,	"frame"},
	{31,	2,	"skin"},
	{32,	2,	"body"},
	{33,	2,	"effects"},
	{34,	2,	"sequence"},
	{35,	2,	"renderfx"},
	{36,	3,	"mins"},
	{39,	3,	"maxs"},
	{42,	3,	"size"},
	{45,	6,	"touch"},
	{46,	6,	"use"},
	{47,	6,	"think"},
	{48,	6,	"blocked"},
	{49,	2,	"nextthink"},
	{50,	4,	"groundentity"},
	{51,	2,	"health"},
	{52,	2,	"frags"},
	{53,	2,	"weapon"},
	{54,	1,	"weaponmodel"},
	{55,	2,	"weaponframe"},
	{56,	2,	"currentammo"},
	{57,	2,	"ammo_shells"},
	{58,	2,	"ammo_nails"},
	{59,	2,	"ammo_rockets"},
	{60,	2,	"ammo_cells"},
	{61,	2,	"items"},
	{62,	2,	"takedamage"},
	{63,	4,	"chain"},
	{64,	2,	"deadflag"},
	{65,	3,	"view_ofs"},
	{68,	2,	"button0"},
	{69,	2,	"button1"},
	{70,	2,	"button2"},
	{71,	2,	"impulse"},
	{72,	2,	"fixangle"},
	{73,	3,	"v_angle"},
	{76,	2,	"idealpitch"},
	{77,	1,	"netname"},
	{78,	4,	"enemy"},
	{79,	2,	"flags"},
	{80,	2,	"colormap"},
	{81,	2,	"team"},
	{82,	2,	"max_health"},
	{83,	2,	"teleport_time"},
	{84,	2,	"armortype"},
	{85,	2,	"armorvalue"},
	{86,	2,	"waterlevel"},
	{87,	2,	"watertype"},
	{88,	2,	"ideal_yaw"},
	{89,	2,	"yaw_speed"},
	{90,	4,	"aiment"},
	{91,	4,	"goalentity"},
	{92,	2,	"spawnflags"},
	{93,	1,	"target"},
	{94,	1,	"targetname"},
	{95,	2,	"dmg_take"},
	{96,	2,	"dmg_save"},
	{97,	4,	"dmg_inflictor"},
	{98,	4,	"owner"},
	{99,	3,	"movedir"},
	{102,	1,	"message"},
	{103,	2,	"sounds"},
	{104,	1,	"noise"},
	{105,	1,	"noise1"},
	{106,	1,	"noise2"},
	{107,	1,	"noise3"}
};

/*
================
SV_FindIndex

================
*/
int SV_FindIndex (const char *name, int start, int end, bool create)
{
	int		i;
	
	if (!name || !name[0]) return 0;

	for (i = 1; i < end && sv.configstrings[start + i][0]; i++)
		if(!strcmp(sv.configstrings[start + i], name))
			return i;
	if (!create) return 0;

	if (i == end) 
	{
		MsgWarn ("SV_FindIndex: %d out range [%d - %d]\n", start, end );
		return 0;
	}

	// register new resource
	strncpy (sv.configstrings[start + i], name, sizeof(sv.configstrings[i]));

	if (sv.state != ss_loading)
	{	
		// send the update to everyone
		SZ_Clear (&sv.multicast);
		MSG_Begin(svc_configstring);
		MSG_WriteShort (&sv.multicast, start+i);
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
	prvm_edict_t			*svent;
	int				entnum;	

	for (entnum = 1; entnum < prog->num_edicts; entnum++)
	{
		svent = PRVM_EDICT_NUM(entnum);

		if (!svent->priv.sv->free) continue;
		if (!(int)svent->fields.sv->modelindex && !(int)svent->priv.sv->state.sound && !(int)svent->fields.sv->effects)
			continue;

		svent->priv.sv->state.number = entnum;

		if (entnum > host.maxclients && !(int)svent->fields.sv->modelindex)
			continue;

		// create entity baseline
		VectorCopy (svent->fields.sv->origin, svent->priv.sv->state.origin);
		VectorCopy (svent->priv.sv->state.origin, svent->priv.sv->state.old_origin);
		VectorCopy (svent->fields.sv->angles, svent->priv.sv->state.angles);
		svent->priv.sv->state.frame = (int)svent->fields.sv->frame;
		svent->priv.sv->state.skin = (int)svent->fields.sv->skin;
		svent->priv.sv->state.body = (int)svent->fields.sv->body;
		svent->priv.sv->state.sequence = (int)svent->fields.sv->sequence;
		svent->priv.sv->state.effects = (int)svent->fields.sv->effects;
		svent->priv.sv->state.renderfx = (int)svent->fields.sv->renderfx;
		svent->priv.sv->state.solid = (int)svent->fields.sv->solid;

		if (entnum > 0 && entnum <= host.maxclients)
		{
			svent->priv.sv->state.modelindex = SV_ModelIndex("progs/player.mdl");
		}
		else
		{
			svent->priv.sv->state.modelindex = (int)svent->fields.sv->modelindex;
		}

		// take current state as baseline
		sv.baselines[entnum] = svent->priv.sv->state;
	}
}

/*
================
SV_SaveSpawnparms

Grabs the current state of each client for saving across the
transition to another level
================
*/
void SV_SaveSpawnparms (void)
{
	int		i, j;

	svs.serverflags = (int)prog->globals.server->serverflags;

	for (i = 0, sv_client = svs.clients; i < host.maxclients; i++, sv_client++)
	{
		if (sv_client->state != cs_spawned)
			continue;

		// call the progs to get default spawn parms for the new client
		prog->globals.server->self = PRVM_EDICT_TO_PROG(sv_client->edict);
		PRVM_ExecuteProgram (prog->globals.server->SetChangeParms, "QC function SetChangeParms is missing");
		for (j = 0; j < NUM_SPAWN_PARMS; j++)
			sv_client->spawn_parms[j] = prog->globals.server->parm[j];
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
	uint		i, checksum;
	prvm_edict_t	*ent;

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

	SV_VM_Setup();

	SZ_Init (&sv.multicast, sv.multicast_buf, sizeof(sv.multicast_buf));

	strcpy (sv.name, server);
	strcpy (sv.configstrings[CS_NAME], server);

	SV_VM_Begin();

	// leave slots at start for clients only
	for (i = 0; i < host.maxclients; i++)
	{
		// needs to reconnect
		if (svs.clients[i].state > cs_connected)
			svs.clients[i].state = cs_connected;
		svs.clients[i].lastframe = -1;
	}

	sv.state = ss_loading;

	if (serverstate != ss_game)
	{
		sv.models[1] = CM_LoadMap ("", false, &checksum);	// no real map
	}
	else
	{
		strcpy (sv.configstrings[CS_MODELS+1], server);
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

	ent = PRVM_EDICT_NUM(0);
	memset (ent->fields.sv, 0, prog->progs->entityfields * 4);
	ent->priv.sv->free = false;
	ent->fields.sv->model = PRVM_SetEngineString(sv.configstrings[CS_MODELS]);
	ent->fields.sv->modelindex = 1; // world model
	ent->fields.sv->solid = SOLID_BSP;
	ent->fields.sv->movetype = MOVETYPE_PUSH;
          
	prog->globals.server->mapname = PRVM_SetEngineString(sv.name);
	Com_SetServerState (sv.state);

	// spawn the rest of the entities on the map
	sv.moved_edicts = (prvm_edict_t **)PRVM_Alloc(prog->max_edicts * sizeof(prvm_edict_t *));
	*prog->time = sv.time = 1.0;

	// serverflags are for cross level information (sigils)
	prog->globals.server->serverflags = svs.serverflags;

	// we need to reset the spawned flag on all connected clients here so that
	// their thinks don't run during startup (before PutClientInServer)
	// we also need to set up the client entities now
	// and we need to set the ->edict pointers to point into the progs edicts
	for (i = 0, sv_client = svs.clients; i < host.maxclients; i++, sv_client++)
	{
		sv_client->state = cs_connected;
		sv_client->edict = PRVM_EDICT_NUM(i + 1);
		sv_client->edict->priv.sv->client = Z_Malloc(sizeof(player_state_t));
		ent->priv.sv->state.number = i + 1;
		memset (&sv_client->lastcmd, 0, sizeof(sv_client->lastcmd));
		PRVM_ED_ClearEdict(sv_client->edict);
	}
	
	PRVM_ED_LoadFromFile (CM_EntityString());

	// all precaches are complete
	sv.state = serverstate;
	Com_SetServerState (sv.state);

	// run two frames to allow everything to settle
	for (i = 0; i < 2; i++)
	{
		sv.frametime = 0.1f;
		SV_Physics();
	}

	// create a baseline for more efficient communications
	SV_CreateBaseline ();

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
	//int		i;
	//prvm_edict_t	*ent;
	char	idmaster[32];

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
	if (host.type == HOST_DEDICATED)
	{
		if (!Cvar_VariableValue ("coop")) Cvar_FullSet ("deathmatch", "1",  CVAR_SERVERINFO | CVAR_LATCH);
	}

	// init clients
	if (Cvar_VariableValue ("deathmatch"))
	{
		if (host.maxclients <= 1) host.maxclients = 8;
		else if (host.maxclients > MAX_CLIENTS) host.maxclients = MAX_CLIENTS;
	}
	else if (Cvar_VariableValue ("coop"))
	{
		if (host.maxclients <= 1 || host.maxclients > 4) host.maxclients = 4;
	}
	else	// non-deathmatch, non-coop is one player
	{
		host.maxclients = 1;
	}

	svs.spawncount = rand();
	svs.clients = Z_Malloc (sizeof(client_t) * host.maxclients);
	svs.num_client_entities = host.maxclients * UPDATE_BACKUP * 64;
	svs.client_entities = Z_Malloc(sizeof(entity_state_t) * svs.num_client_entities);

	// init network stuff
	NET_Config ( (host.maxclients > 1) );

	// heartbeats will always be sent to the id master
	svs.last_heartbeat = -99999;		// send immediately
	sprintf(idmaster, "192.246.40.37:%i", PORT_MASTER);
	NET_StringToAdr (idmaster, &master_adr[0]);
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
	if (!strcmp(ext, "cin"))
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
	else if (!strcmp(ext, "pcx"))
	{
		SCR_BeginLoadingPlaque (); // for local system
		SV_BroadcastCommand ("changing\n");
		SV_SpawnServer (level, spawnpoint, NULL, ss_pic, attractloop, loadgame);
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
	prvm_edict_t	*ent;

	PRVM_Free( sv.moved_edicts );
	sv.moved_edicts = (prvm_edict_t **)PRVM_Alloc(prog->max_edicts * sizeof(prvm_edict_t *));

	// links don't survive the transition, so unlink everything
	for (i = 0, ent = prog->edicts;i < prog->max_edicts;i++, ent++)
	{
		if (!ent->priv.sv->free) SV_UnlinkEdict(prog->edicts + i);
		memset(&ent->priv.sv->clusternums, 0, sizeof(ent->priv.sv->clusternums));
	}
	SV_ClearWorld();
}

void SV_VM_EndIncreaseEdicts(void)
{
	int		i;
	prvm_edict_t	*ent;

	for (i = 0, ent = prog->edicts;i < prog->max_edicts;i++, ent++)
	{
		// link every entity except world
		if (!ent->priv.sv->free) SV_LinkEdict(ent);
	}
}

void SV_VM_InitEdict(prvm_edict_t *e)
{
	int num = PRVM_NUM_FOR_EDICT(e) - 1;
	e->priv.sv->move = false; // don't move on first frame

	// set here additional player effects: model, skin, etc...
}

void SV_VM_FreeEdict(prvm_edict_t *ed)
{
	SV_UnlinkEdict (ed);		// unlink from world bsp

	ed->fields.sv->model = 0;
	ed->fields.sv->takedamage = 0;
	ed->fields.sv->modelindex = 0;
	ed->fields.sv->colormap = 0;
	ed->fields.sv->skin = 0;
	ed->fields.sv->frame = 0;
	VectorClear(ed->fields.sv->origin);
	VectorClear(ed->fields.sv->angles);
	ed->fields.sv->nextthink = -1;
	ed->fields.sv->solid = 0;
}

void SV_VM_CountEdicts( void )
{
	int		i;
	prvm_edict_t	*ent;
	int		active = 0, models = 0, solid = 0, step = 0;

	for (i = 0; i < prog->num_edicts; i++)
	{
		ent = PRVM_EDICT_NUM(i);
		if (ent->priv.sv->free)
			continue;
		active++;
		if (ent->fields.sv->solid) solid++;
		if (ent->fields.sv->model) models++;
		if (ent->fields.sv->movetype == MOVETYPE_STEP) step++;
	}

	Msg("num_edicts:%3i\n", prog->num_edicts);
	Msg("active    :%3i\n", active);
	Msg("view      :%3i\n", models);
	Msg("touch     :%3i\n", solid);
	Msg("step      :%3i\n", step);
}

bool SV_VM_LoadEdict(prvm_edict_t *ent)
{
	int current_skill = (int)Cvar_VariableValue ("skill");

	// remove things from different skill levels or deathmatch
	if(Cvar_VariableValue ("deathmatch"))
	{
		if (((int)ent->fields.sv->spawnflags & SPAWNFLAG_NOT_DEATHMATCH))
		{
			return false;
		}
	}
	else if ((current_skill <= 0 && ((int)ent->fields.sv->spawnflags & SPAWNFLAG_NOT_EASY  )) || (current_skill == 1 && ((int)ent->fields.sv->spawnflags & SPAWNFLAG_NOT_MEDIUM)) || (current_skill >= 2 && ((int)ent->fields.sv->spawnflags & SPAWNFLAG_NOT_HARD  )))
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
	prog->reserved_edicts = host.maxclients;
	prog->edictprivate_size = sizeof(server_edict_t);
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