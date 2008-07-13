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

#include "common.h"
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
		if(!com.strcmp(sv.configstrings[start+i], name))
			return i;
	if(!create) return 0;

	if (i == end) 
	{
		MsgDev( D_WARN, "SV_FindIndex: %d out of range [%d - %d]\n", start, end );
		return 0;
	}

	// register new resource
	strncpy (sv.configstrings[start+i], name, sizeof(sv.configstrings[i]));

	if (sv.state != ss_loading)
	{	
		// send the update to everyone
		MSG_Clear( &sv.multicast );
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
		if (!svent->progs.sv->modelindex && !svent->priv.sv->s.soundindex && !svent->progs.sv->effects)
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
	sv.loadgame = true; // predicting state

	if(sv_noreload->value) sv.loadgame = false;
	if(Cvar_VariableValue("deathmatch")) sv.loadgame = false;
	if(!savename) sv.loadgame = false;
	if(!FS_FileExists(va("save/%s", savename )))
		sv.loadgame = false;
}


/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.

================
*/
void SV_SpawnServer (char *server, char *savename, sv_state_t serverstate )
{
	uint	i, checksum;

	if( serverstate == ss_cinematic ) Cvar_Set ("paused", "0");

	Msg("SpawnServer [%s]\n", server );

	svs.spawncount++; // any partially connected client will be restarted
	sv.state = ss_dead;
	Host_SetServerState(sv.state);

	// wipe the entire per-level structure
	memset (&sv, 0, sizeof(sv));
	svs.realtime = 0;

	// save name for levels that don't set message
	strcpy (sv.configstrings[CS_NAME], server);
	if( Cvar_VariableValue ("deathmatch") )
		com.sprintf( sv.configstrings[CS_AIRACCEL], "%g", sv_airaccelerate->value );
	else com.strcpy( sv.configstrings[CS_AIRACCEL], "0" );

	MSG_Init(&sv.multicast, sv.multicast_buf, sizeof(sv.multicast_buf));
	com.strcpy( sv.name, server );

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
	
	strcpy(sv.name, server);
	FS_FileBase(server, sv.configstrings[CS_NAME]);

	if (serverstate != ss_active)
	{
		sv.models[1] = pe->BeginRegistration( "", false, &checksum); // no real map
	}
	else
	{
		com.sprintf(sv.configstrings[CS_MODELS+1], "maps/%s", server);
		sv.models[1] = pe->BeginRegistration(sv.configstrings[CS_MODELS+1], false, &checksum);
	}
	com.sprintf(sv.configstrings[CS_MAPCHECKSUM], "%i", checksum);

	// clear physics interaction links
	SV_ClearWorld();

	for (i = 1; i < pe->NumBmodels(); i++)
	{
		com.sprintf( sv.configstrings[CS_MODELS+1+i], "*%i", i );
		sv.models[i+1] = pe->RegisterModel(sv.configstrings[CS_MODELS+1+i] );
	}

	//
	// spawn the rest of the entities on the map
	//	

	// precache and static commands can be issued during
	// map initialization
	sv.state = ss_loading;
	Host_SetServerState( sv.state );

	// check for a savegame
	SV_CheckForSavegame( savename );

	if( serverstate == ss_active )
	{
		// ignore ents for cinematic servers
		if(sv.loadgame) SV_ReadLevelFile( savename );
		else SV_SpawnEntities ( sv.name, pe->GetEntityString());
	}        

	// run two frames to allow everything to settle
	SV_RunFrame();
	SV_RunFrame();

	// all precaches are complete
	sv.state = serverstate;
	Host_SetServerState (sv.state);

	// create a baseline for more efficient communications
	SV_CreateBaseline ();

	// set serverinfo variable
	Cvar_FullSet("mapname", sv.name, CVAR_SERVERINFO | CVAR_INIT);
	pe->EndRegistration(); // free unused models
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
	
	if( svs.initialized )
	{
		// cause any connected clients to reconnect
		com.strncpy( host.finalmsg, "Server restarted\n", MAX_STRING );
		SV_Shutdown( true );
	}
	else
	{
		// make sure the client is down
		CL_Drop();
	}

	svs.initialized = true;

	if (Cvar_VariableValue ("coop") && Cvar_VariableValue ("deathmatch"))
	{
		Msg("Deathmatch and Coop both set, disabling Coop\n");
		Cvar_FullSet ("coop", "0",  CVAR_SERVERINFO | CVAR_LATCH);
	}

	// dedicated servers are can't be single player and are usually DM
	// so unless they explicity set coop, force it to deathmatch
	if( host.type == HOST_DEDICATED )
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
	svs.clients = Z_Malloc (sizeof(sv_client_t)*maxclients->value);
	svs.num_client_entities = maxclients->value*UPDATE_BACKUP*64;
	svs.client_entities = Z_Malloc (sizeof(entity_state_t)*svs.num_client_entities);

	// heartbeats will always be sent to the id master
	svs.last_heartbeat = MAX_HEARTBEAT; // send immediately
	com.sprintf(idmaster, "192.246.40.37:%i", PORT_MASTER);
	NET_StringToAdr (idmaster, &master_adr[0]);

	// init game
	SV_InitServerProgs();

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