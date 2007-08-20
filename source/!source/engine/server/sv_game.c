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
HINSTANCE sv_library;

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



void PF_Begin( int dest )	{ MSG_Begin( dest ); }
void PF_WriteChar (int c)	{ MSG_WriteChar (&sv.multicast, c);	}
void PF_WriteByte (int c)	{ MSG_WriteByte (&sv.multicast, c);	}
void PF_WriteShort (int c)	{ MSG_WriteShort (&sv.multicast, c);	}
void PF_WriteWord (int c)	{ MSG_WriteWord (&sv.multicast, c);	}
void PF_WriteLong (int c)	{ MSG_WriteLong (&sv.multicast, c);	}
void PF_WriteFloat (float f)	{ MSG_WriteFloat (&sv.multicast, f);	}
void PF_WriteString (char *s) { MSG_WriteString (&sv.multicast, s);	}
void PF_WriteCoord(vec3_t pos){ MSG_WritePos (&sv.multicast, pos);	}
void PF_WriteDir (vec3_t dir) { MSG_WriteDir (&sv.multicast, dir);	}
void PF_WriteAngle (float f)	{ MSG_WriteAngle (&sv.multicast, f);	}
void PF_Send( msgtype_t to, vec3_t org, edict_t *ent ) { MSG_Send( to, org, ent ); }

message_write_t Msg_GetAPI( void )
{
	static message_write_t	Msg;

	Msg.api_size = sizeof(message_write_t);

	Msg.Begin = PF_Begin;
	Msg.WriteChar = PF_WriteChar;
	Msg.WriteByte = PF_WriteByte;
	Msg.WriteWord = PF_WriteWord;
	Msg.WriteShort = PF_WriteShort;
	Msg.WriteLong = PF_WriteLong;
	Msg.WriteFloat = PF_WriteFloat;
	Msg.WriteString = PF_WriteString;
	Msg.WriteCoord = PF_WriteCoord;
	Msg.WriteDir = PF_WriteDir;
	Msg.WriteAngle = PF_WriteAngle;
	Msg.Send = PF_Send;

	return Msg;
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
	ge->Shutdown ();
	Sys_UnloadGame ( sv_library );
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
	game_import_t	import;

	// unload anything we have now
	if (ge) SV_ShutdownGameProgs ();

	// load a new game dll
	import.Fs = pi->Fs;
	import.VFs = pi->VFs;
	import.Mem = pi->Mem;
	import.Script = pi->Script;
	import.Compile = pi->Compile;
	import.Info = pi->Info;
	import.Msg = Msg_GetAPI();

	import.GameInfo = pi->GameInfo;

	import.bprintf = SV_BroadcastPrintf;
	import.dprintf = PF_dprintf;
	import.cprintf = PF_cprintf;
	import.centerprintf = PF_centerprintf;
	import.error = PF_error;

	import.linkentity = SV_LinkEdict;
	import.unlinkentity = SV_UnlinkEdict;
	import.BoxEdicts = SV_AreaEdicts;
	import.trace = SV_Trace;
	import.pointcontents = SV_PointContents;
	import.setmodel = PF_setmodel;
	import.inPVS = PF_inPVS;
	import.inPHS = PF_inPHS;
	import.Pmove = Pmove;

	import.getmodelhdr = SV_GetModelPtr;

	import.modelindex = SV_ModelIndex;
	import.soundindex = SV_SoundIndex;
	import.imageindex = SV_ImageIndex;

	import.configstring = PF_Configstring;
	import.sound = PF_StartSound;
	import.positioned_sound = SV_StartSound;

	import.cvar = Cvar_Get;
	import.cvar_set = Cvar_Set;
	import.cvar_forceset = Cvar_ForceSet;

	import.argc = Cmd_Argc;
	import.argv = Cmd_Argv;
	import.args = Cmd_Args;
	import.AddCommandString = Cbuf_AddText;

	import.DebugGraph = SCR_DebugGraph;
	import.SetAreaPortalState = CM_SetAreaPortalState;
	import.AreasConnected = CM_AreasConnected;

	//find server.dll
	ge = (game_export_t *)Sys_LoadGame("ServerAPI", sv_library, &import);
	
	if (!ge) Com_Error (ERR_DROP, "failed to load game DLL");
	if (ge->apiversion != GAME_API_VERSION)
		Com_Error (ERR_DROP, "game is version %i, not %i", ge->apiversion, GAME_API_VERSION);
	ge->Init ();
}

