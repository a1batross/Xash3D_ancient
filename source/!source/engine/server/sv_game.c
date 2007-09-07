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
// sv_game.c -- interface to the game dll

#include <windows.h>
#include "engine.h"
#include "server.h"

game_export_t	*ge;
HINSTANCE		sv_library;
game_locals_t	game;

/*
===============
PF_dprintf

Debug print to server console
===============
*/
void PF_dprintf (char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	
	va_start (argptr,fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);

	Msg ("%s", msg);
}


/*
===============
PF_cprintf

Print to a single client
===============
*/
void PF_cprintf (edict_t *ent, int level, char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	int			n;

	if (ent)
	{
		n = NUM_FOR_EDICT(ent);
		if (n < 1 || n > maxclients->value)
			Com_Error (ERR_DROP, "cprintf to a non-client");
	}

	va_start (argptr,fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);

	if (ent)
		SV_ClientPrintf (svs.clients+(n-1), level, "%s", msg);
	else
		Msg ("%s", msg);
}


/*
===============
PF_centerprintf

centerprint to a single client
===============
*/
void PF_centerprintf (edict_t *ent, char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	int			n;
	
	n = NUM_FOR_EDICT(ent);
	if (n < 1 || n > maxclients->value)
		return;	// Com_Error (ERR_DROP, "centerprintf to a non-client");

	va_start (argptr,fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);

	MSG_Begin( svc_centerprint );
	MSG_WriteString (&sv.multicast, msg);
	MSG_Send(MSG_ONE_R, NULL, ent );
}


/*
===============
PF_error

Abort the server with a game error
===============
*/
void PF_error (char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	
	va_start (argptr,fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);

	Com_Error (ERR_DROP, "Game Error: %s", msg);
}


/*
=================
PF_setmodel

Also sets mins and maxs for inline bmodels
=================
*/
void PF_setmodel (edict_t *ent, char *name)
{
	int		i;
	cmodel_t		*mod;
	
	if (!name) Com_Error (ERR_DROP, "PF_setmodel: NULL");

	i = SV_ModelIndex (name);
		
//	ent->model = name;
	ent->s.modelindex = i;

	mod = CM_LoadModel (name);

	if(mod)	// hull setup
	{
		VectorCopy (mod->mins, ent->mins);
		VectorCopy (mod->maxs, ent->maxs);
		SV_LinkEdict (ent);
	}
}

/*
===============
PF_Configstring

===============
*/
void PF_Configstring (int index, char *val)
{
	if (index < 0 || index >= MAX_CONFIGSTRINGS)
		Com_Error (ERR_DROP, "configstring: bad index %i value %s\n", index, val);

	if (!val) val = "";

	// change the string in sv
	strcpy (sv.configstrings[index], val);

	if (sv.state != ss_loading)
	{
		// send the update to everyone
		SZ_Clear (&sv.multicast);
		MSG_Begin(svc_configstring);
		MSG_WriteShort (&sv.multicast, index);
		MSG_WriteString (&sv.multicast, val);
		MSG_Send(MSG_ALL_R, vec3_origin, NULL );
	}
}

/*
=================
PF_inPVS

Also checks portalareas so that doors block sight
=================
*/
bool PF_inPVS (vec3_t p1, vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return false;
	if (!CM_AreasConnected (area1, area2))
		return false;		// a door blocks sight
	return true;
}


/*
=================
PF_inPHS

Also checks portalareas so that doors block sound
=================
*/
bool PF_inPHS (vec3_t p1, vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPHS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return false;		// more than one bounce away
	if (!CM_AreasConnected (area1, area2))
		return false;		// a door blocks hearing

	return true;
}

void PF_StartSound (edict_t *entity, int channel, int sound_num, float volume, float attenuation, float timeofs)
{
	if (!entity) return;
	SV_StartSound (NULL, entity, channel, sound_num, volume, attenuation, timeofs);
}

//==============================================

/*
===============
SV_ShutdownGameProgs

Called when either the entire server is being killed, or
it is changing to a different game directory.
===============
*/
void SV_ShutdownGameProgs (void)
{
	if (!ge) return;

	Msg("==== ShutdownGame ====\n");

	//free main memory pools
	Mem_FreePool(&sv.mempool);

	Mem_Free( ge );
	ge = NULL;
}

/*
===============
SV_InitGameProgs

Init the game subsystem for a new map
===============
*/
void SCR_DebugGraph (float value, int color);

void SV_InitGameProgs (void)
{
	// unload anything we have now
	if (ge) SV_ShutdownGameProgs ();

	sv.mempool = Mem_AllocPool("Zone Game");
	ge = Mem_Alloc(sv.mempool, sizeof(game_export_t));

	// initialize all entities for this game
	game.maxentities = 1024;

	ge->edict_size = sizeof(edict_t);
	ge->edicts =  (edict_t *)Mem_Alloc(sv.mempool, game.maxentities * ge->edict_size);
	ge->max_edicts = game.maxentities;

	// initialize all clients for this game
	game.maxclients = maxclients->value;
	game.clients = (gclient_t *)Mem_Alloc(sv.mempool, game.maxclients * sizeof(gclient_t));
	ge->num_edicts = game.maxclients + 1; //reserve for world
}

