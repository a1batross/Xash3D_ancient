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
// sv_user.c -- server code for moving users

#include "engine.h"
#include "server.h"

extern cvar_t *sv_maxspeed;
extern cvar_t *sv_accelerate;
extern cvar_t *sv_wateraccelerate;
extern cvar_t *sv_friction;
extern cvar_t *sv_rollangle;
extern cvar_t *sv_rollspeed;

prvm_edict_t	*sv_player;
static bool	onground;
static usercmd_t	cmd;
static vec3_t	wishdir, forward, right, up;
static float	wishspeed;

/*
============================================================

USER STRINGCMD EXECUTION

sv_client and sv_player will be valid.
============================================================
*/

/*
==================
SV_BeginDemoServer
==================
*/
void SV_BeginDemoserver (void)
{
	char		name[MAX_OSPATH];

	sprintf (name, "demos/%s", sv.name);
	sv.demofile = FS_Open(name, "rb" );
	if (!sv.demofile)
		Com_Error (ERR_DROP, "Couldn't open %s\n", name);
}

/*
================
SV_New_f

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_New_f (void)
{
	char		*gamedir;
	int		playernum;
	prvm_edict_t		*ent;

	if (sv_client->state != cs_connected)
	{
		Msg ("New not valid -- already spawned\n");
		return;
	}

	// demo servers just dump the file message
	if (sv.state == ss_demo)
	{
		SV_BeginDemoserver ();
		return;
	}

	// serverdata needs to go over for all types of servers
	// to make sure the protocol is right, and to set the gamedir
	gamedir = Cvar_VariableString ("gamedir");

	// send the serverdata
	MSG_WriteByte (&sv_client->netchan.message, svc_serverdata);
	MSG_WriteLong (&sv_client->netchan.message, PROTOCOL_VERSION);
	MSG_WriteLong (&sv_client->netchan.message, svs.spawncount);
	MSG_WriteByte (&sv_client->netchan.message, sv.attractloop);
	MSG_WriteString (&sv_client->netchan.message, gamedir);

	if (sv.state == ss_cinematic || sv.state == ss_pic)
		playernum = -1;
	else playernum = sv_client - svs.clients;
	MSG_WriteShort (&sv_client->netchan.message, playernum);

	// send full levelname
	MSG_WriteString (&sv_client->netchan.message, sv.configstrings[CS_NAME]);

	// game server
	if (sv.state == ss_game)
	{
		// set up the entity for the client
		ent = PRVM_EDICT_NUM(playernum+1);
		ent->priv.sv->state.number = playernum+1;
		sv_client->edict = ent;
		memset (&sv_client->lastcmd, 0, sizeof(sv_client->lastcmd));

		// begin fetching configstrings
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd configstrings %i 0\n",svs.spawncount) );
	}

}

/*
==================
SV_Configstrings_f
==================
*/
void SV_Configstrings_f (void)
{
	int			start;

	if (sv_client->state != cs_connected)
	{
		Msg ("configstrings not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Msg ("SV_Configstrings_f from different level\n");
		SV_New_f ();
		return;
	}
	
	start = atoi(Cmd_Argv(2));

	// write a packet full of data

	while ( sv_client->netchan.message.cursize < MAX_MSGLEN/2 
		&& start < MAX_CONFIGSTRINGS)
	{
		if (sv.configstrings[start][0])
		{
			MSG_WriteByte (&sv_client->netchan.message, svc_configstring);
			MSG_WriteShort (&sv_client->netchan.message, start);
			MSG_WriteString (&sv_client->netchan.message, sv.configstrings[start]);
		}
		start++;
	}

	// send next command

	if (start == MAX_CONFIGSTRINGS)
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd baselines %i 0\n",svs.spawncount) );
	}
	else
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd configstrings %i %i\n",svs.spawncount, start) );
	}
}

/*
==================
SV_Baselines_f
==================
*/
void SV_Baselines_f (void)
{
	int		start;
	entity_state_t	nullstate;
	entity_state_t	*base;

	if (sv_client->state != cs_connected)
	{
		Msg ("baselines not valid -- already spawned\n");
		return;
	}
	
	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Msg ("SV_Baselines_f from different level\n");
		SV_New_f ();
		return;
	}
	
	start = atoi(Cmd_Argv(2));

	memset (&nullstate, 0, sizeof(nullstate));

	// write a packet full of data

	while ( sv_client->netchan.message.cursize <  MAX_MSGLEN/2
		&& start < MAX_EDICTS)
	{
		base = &sv.baselines[start];
		if (base->modelindex || base->sound || base->effects)
		{
			MSG_WriteByte (&sv_client->netchan.message, svc_spawnbaseline);
			MSG_WriteDeltaEntity (&nullstate, base, &sv_client->netchan.message, true, true);
		}
		start++;
	}

	// send next command

	if (start == MAX_EDICTS)
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("precache %i\n", svs.spawncount) );
	}
	else
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd baselines %i %i\n",svs.spawncount, start) );
	}
}

/*
==================
SV_Begin_f
==================
*/
void SV_Begin_f (void)
{
	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Msg ("SV_Begin_f from different level\n");
		SV_New_f ();
		return;
	}

	sv_client->state = cs_spawned;
	
	// call the game begin function
//ge->ClientBegin (sv_player);
	Cbuf_InsertFromDefer ();
}

//=============================================================================

/*
==================
SV_NextDownload_f
==================
*/
void SV_NextDownload_f (void)
{
	int		r;
	int		percent;
	int		size;

	if (!sv_client->download)
		return;

	r = sv_client->downloadsize - sv_client->downloadcount;
	if (r > 1024)
		r = 1024;

	MSG_WriteByte (&sv_client->netchan.message, svc_download);
	MSG_WriteShort (&sv_client->netchan.message, r);

	sv_client->downloadcount += r;
	size = sv_client->downloadsize;
	if (!size)
		size = 1;
	percent = sv_client->downloadcount*100/size;
	MSG_WriteByte (&sv_client->netchan.message, percent);
	SZ_Write (&sv_client->netchan.message,
		sv_client->download + sv_client->downloadcount - r, r);

	if (sv_client->downloadcount != sv_client->downloadsize)
		return;

	sv_client->download = NULL;
}

/*
==================
SV_BeginDownload_f
==================
*/
void SV_BeginDownload_f(void)
{
	char	*name;
	extern	cvar_t *allow_download;
	extern	cvar_t *allow_download_players;
	extern	cvar_t *allow_download_models;
	extern	cvar_t *allow_download_sounds;
	extern	cvar_t *allow_download_maps;
	int offset = 0;

	name = Cmd_Argv(1);

	if (Cmd_Argc() > 2)
		offset = atoi(Cmd_Argv(2)); // downloaded offset

	// hacked by zoid to allow more conrol over download
	// first off, no .. or global allow check
	if (strstr (name, "..") || !allow_download->value
		// leading dot is no good
		|| *name == '.' 
		// leading slash bad as well, must be in subdir
		|| *name == '/'
		// next up, skin check
		|| (strncmp(name, "players/", 6) == 0 && !allow_download_players->value)
		// now models
		|| (strncmp(name, "models/", 6) == 0 && !allow_download_models->value)
		// now sounds
		|| (strncmp(name, "sound/", 6) == 0 && !allow_download_sounds->value)
		// now maps (note special case for maps, must not be in pak)
		|| (strncmp(name, "maps/", 6) == 0 && !allow_download_maps->value)
		// MUST be in a subdirectory	
		|| !strstr (name, "/") )	
	{	// don't allow anything with .. path
		MSG_WriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);
		return;
	}

          sv_client->download = FS_LoadFile (name, &sv_client->downloadsize);
	sv_client->downloadcount = offset;

	if (offset > sv_client->downloadsize)
		sv_client->downloadcount = sv_client->downloadsize;

	if (!sv_client->download)
	{
		MsgWarn("SV_BeginDownload_f: couldn't download %s to %s\n", name, sv_client->name);
		if (sv_client->download)
		{
			sv_client->download = NULL;
		}

		MSG_WriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);
		return;
	}

	SV_NextDownload_f ();
	MsgDev ("Downloading %s to %s\n", name, sv_client->name);
}



//============================================================================


/*
=================
SV_Disconnect_f

The client is going to disconnect, so remove the connection immediately
=================
*/
void SV_Disconnect_f (void)
{
//	SV_EndRedirect ();
	SV_DropClient (sv_client);	
}


/*
==================
SV_ShowServerinfo_f

Dumps the serverinfo info string
==================
*/
void SV_ShowServerinfo_f (void)
{
	Info_Print (Cvar_Serverinfo());
}


void SV_Nextserver (void)
{
	char	*v;

	//ZOID, ss_pic can be nextserver'd in coop mode
	if (sv.state == ss_game || (sv.state == ss_pic && !Cvar_VariableValue("coop")))
		return;		// can't nextserver while playing a normal game

	svs.spawncount++;	// make sure another doesn't sneak in
	v = Cvar_VariableString ("nextserver");
	if (!v[0])
		Cbuf_AddText ("killserver\n");
	else
	{
		Cbuf_AddText (v);
		Cbuf_AddText ("\n");
	}
	Cvar_Set ("nextserver","");
}

/*
==================
SV_Nextserver_f

A cinematic has completed or been aborted by a client, so move
to the next server,
==================
*/
void SV_Nextserver_f (void)
{
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		MsgWarn("SV_Nextserver_f: loading wrong level, from %s\n", sv_client->name);
		return; // leftover from last server
	}
	SV_Nextserver ();
}

typedef struct
{
	char	*name;
	void	(*func) (void);
} ucmd_t;

ucmd_t ucmds[] =
{
	// auto issued
	{"new", SV_New_f},
	{"configstrings", SV_Configstrings_f},
	{"baselines", SV_Baselines_f},
	{"begin", SV_Begin_f},
	{"nextserver", SV_Nextserver_f},
	{"disconnect", SV_Disconnect_f},
	{"info", SV_ShowServerinfo_f},
	{"download", SV_BeginDownload_f},
	{"nextdl", SV_NextDownload_f},
	{NULL, NULL}
};

/*
==================
SV_ExecuteUserCommand
==================
*/
void SV_ExecuteUserCommand (char *s)
{
	ucmd_t	*u;
	
	Cmd_TokenizeString (s, true);
	sv_player = sv_client->edict;

	for (u = ucmds; u->name; u++)
	{
		if (!strcmp (Cmd_Argv(0), u->name) )
		{
			u->func();
			break;
		}
	}

//if (!u->name && sv.state == ss_game)
//ge->ClientCommand (sv_player);
}

/*
===========================================================================

USER CMD EXECUTION

===========================================================================
*/
void SV_ClientRun (client_t *cl, usercmd_t *curcmd)
{
	sv_client = cl;
          cmd = *curcmd;
 
	cl->commandMsec -= curcmd->msec;

	if (cl->commandMsec < 0 && sv_enforcetime->value )
	{
		MsgWarn("SV_ClientThink: commandMsec underflow from %s\n", cl->name);
		return;
	}
	//SV_ClientThink();
}

/*
===============
SV_CalcRoll

===============
*/
float SV_CalcRoll (vec3_t angles, vec3_t velocity)
{
	float	sign;
	float	side;
	float	value;
	
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);
	
	value = sv_rollangle->value;

	if (side < sv_rollspeed->value)
		side = side * value / sv_rollspeed->value;
	else side = value;
	
	return side*sign;
	
}


/*
==================
SV_UserFriction

==================
*/
void SV_UserFriction (void)
{
	float speed, newspeed, control, friction;
	vec3_t start, stop;
	trace_t trace;

	speed = sqrt(sv_client->edict->fields.sv->velocity[0]*sv_client->edict->fields.sv->velocity[0]+sv_client->edict->fields.sv->velocity[1]*sv_client->edict->fields.sv->velocity[1]);
	if (!speed) return;

	// if the leading edge is over a dropoff, increase friction
	start[0] = stop[0] = sv_client->edict->fields.sv->origin[0] + sv_client->edict->fields.sv->velocity[0]/speed*16;
	start[1] = stop[1] = sv_client->edict->fields.sv->origin[1] + sv_client->edict->fields.sv->velocity[1]/speed*16;
	start[2] = sv_client->edict->fields.sv->origin[2] + sv_client->edict->fields.sv->mins[2];
	stop[2] = start[2] - 34;

	trace = SV_Trace (start, vec3_origin, vec3_origin, stop, sv_client->edict, MASK_SOLID );

	if (trace.fraction == 1.0) friction = sv_friction->value * 2;
	else friction = sv_friction->value;

	// apply friction

	control = max(speed, 100);
	newspeed = speed - sv.frametime * control * friction;

	if (newspeed < 0) newspeed = 0;
	else newspeed /= speed;

	VectorScale(sv_client->edict->fields.sv->velocity, newspeed, sv_client->edict->fields.sv->velocity);
}

/*
==============
SV_Accelerate
==============
*/
void SV_Accelerate (void)
{
	int i;
	float addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct (sv_client->edict->fields.sv->velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0) return;
	accelspeed = sv_accelerate->value * sv.frametime * wishspeed;

	if (accelspeed > addspeed) accelspeed = addspeed;

	for (i = 0; i < 3; i++) sv_client->edict->fields.sv->velocity[i] += accelspeed * wishdir[i];
}

void SV_AirAccelerate (vec3_t wishveloc)
{
	int i;
	float addspeed, wishspd, accelspeed, currentspeed;

	wishspd = VectorNormalize (wishveloc);
	if (wishspd > sv_maxspeed->value / 10) wishspd = sv_maxspeed->value / 10;
	currentspeed = DotProduct (sv_client->edict->fields.sv->velocity, wishveloc);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0) return;
	accelspeed = (sv_airaccelerate->value < 0 ? sv_accelerate->value : sv_airaccelerate->value) * wishspeed * sv.frametime;
	if (accelspeed > addspeed) accelspeed = addspeed;

	for (i = 0; i < 3; i++) sv_client->edict->fields.sv->velocity[i] += accelspeed*wishveloc[i];
}

void DropPunchAngle (void)
{
	float len;

	len = VectorNormalize(sv_client->edict->fields.sv->punchangle);

	len -= 10 * sv.frametime;
	if (len < 0) len = 0;
	VectorScale (sv_client->edict->fields.sv->punchangle, len, sv_client->edict->fields.sv->punchangle);
}

/*
===================
SV_AirMove

===================
*/
void SV_AirMove (void)
{
	int	i;
	vec3_t	wishvel;
	float	fmove, smove, temp;

	wishvel[0] = wishvel[2] = 0;
	wishvel[1] = sv_client->edict->fields.sv->angles[1];
	AngleVectorsRight (wishvel, forward, right, up);

	fmove = cmd.forwardmove;
	smove = cmd.sidemove;

	// hack to not let you back into teleporter
	if (sv.time < sv_client->edict->fields.sv->teleport_time && fmove < 0)
		fmove = 0;

	for (i=0 ; i<3 ; i++)
		wishvel[i] = forward[i]*fmove + right[i]*smove;

	if ((int)sv_client->edict->fields.sv->movetype != MOVETYPE_WALK)
		wishvel[2] += cmd.upmove;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);
	if (wishspeed > sv_maxspeed->value)
	{
		temp = sv_maxspeed->value/wishspeed;
		VectorScale (wishvel, temp, wishvel);
		wishspeed = sv_maxspeed->value;
	}

	if (sv_client->edict->fields.sv->movetype == MOVETYPE_NOCLIP)
	{
		// noclip
		VectorCopy (wishvel, sv_client->edict->fields.sv->velocity);
	}
	else if (onground && (!(sv_client->edict->fields.sv->button2)))
	{
		SV_UserFriction ();
		SV_Accelerate ();
	}
	else
	{
		// not on ground, so little effect on velocity
		SV_AirAccelerate (wishvel);
	}
}

/*
===================
SV_WaterMove

===================
*/
void SV_WaterMove (void)
{
	int	i;
	vec3_t	wishvel;
	float	speed, newspeed, wishspeed, addspeed, accelspeed, temp;

	// user intentions
	AngleVectorsRight (sv_client->edict->fields.sv->v_angle, forward, right, up);

	for (i = 0; i < 3; i++)
		wishvel[i] = forward[i]*cmd.forwardmove + right[i]*cmd.sidemove;

	if (!cmd.forwardmove && !cmd.sidemove && !cmd.upmove)
		wishvel[2] -= 60;		// drift towards bottom
	else
		wishvel[2] += cmd.upmove;

	wishspeed = VectorLength(wishvel);
	if (wishspeed > sv_maxspeed->value)
	{
		temp = sv_maxspeed->value/wishspeed;
		VectorScale (wishvel, temp, wishvel);
		wishspeed = sv_maxspeed->value;
	}
	wishspeed *= 0.7;

	// water friction
	speed = VectorLength(sv_client->edict->fields.sv->velocity);
	if (speed)
	{
		newspeed = speed - sv.frametime * speed * -1;
		if (newspeed < 0)
			newspeed = 0;
		temp = newspeed/speed;
		VectorScale(sv_client->edict->fields.sv->velocity, temp, sv_client->edict->fields.sv->velocity);
	}
	else
		newspeed = 0;

	// water acceleration
	if (!wishspeed)
		return;

	addspeed = wishspeed - newspeed;
	if (addspeed <= 0)
		return;

	VectorNormalize (wishvel);
	accelspeed = (sv_wateraccelerate->value < 0 ? sv_accelerate->value : sv_wateraccelerate->value) * wishspeed * sv.frametime;
	if (accelspeed > addspeed) accelspeed = addspeed;

	for (i = 0; i < 3; i++) sv_client->edict->fields.sv->velocity[i] += accelspeed * wishvel[i];
}

void SV_WaterJump (void)
{
	if (sv.time > sv_client->edict->fields.sv->teleport_time || !sv_client->edict->fields.sv->waterlevel)
	{
		sv_client->edict->fields.sv->flags = (int)sv_client->edict->fields.sv->flags & ~FL_WATERJUMP;
		sv_client->edict->fields.sv->teleport_time = 0;
	}
	sv_client->edict->fields.sv->velocity[0] = sv_client->edict->fields.sv->movedir[0];
	sv_client->edict->fields.sv->velocity[1] = sv_client->edict->fields.sv->movedir[1];
}


/*
===================
SV_ClientThink

the move fields specify an intended velocity in pix/sec
the angle fields specify an exact angular motion in degrees
===================
*/
void SV_ClientThink (void)
{
	vec3_t v_angle;

	if (sv_client->edict->fields.sv->movetype == MOVETYPE_NONE)
		return;

	onground = (int)sv_client->edict->fields.sv->flags & FL_ONGROUND;

	DropPunchAngle ();

	// if dead, behave differently
	if (sv_client->edict->fields.sv->health <= 0) return;

	cmd = sv_client->lastcmd;
	sv_client->commandMsec -= cmd.msec;

	// angles
	// show 1/3 the pitch angle and all the roll angle
	VectorAdd (sv_client->edict->fields.sv->v_angle, sv_client->edict->fields.sv->punchangle, v_angle);
	sv_client->edict->fields.sv->angles[ROLL] = SV_CalcRoll (sv_client->edict->fields.sv->angles, sv_client->edict->fields.sv->velocity)*4;
	if (!sv_client->edict->fields.sv->fixangle)
	{
		sv_client->edict->fields.sv->angles[PITCH] = -v_angle[PITCH]/3;
		sv_client->edict->fields.sv->angles[YAW] = v_angle[YAW];
	}

	if ( (int)sv_client->edict->fields.sv->flags & FL_WATERJUMP )
	{
		SV_WaterJump ();
		return;
	}

	// walk
	if ((sv_client->edict->fields.sv->waterlevel >= 2) && (sv_client->edict->fields.sv->movetype != MOVETYPE_NOCLIP))
	{
		SV_WaterMove();
		return;
	}

	SV_AirMove();
}

void SV_ApplyClientMove (void)
{
	usercmd_t *move = &sv_client->lastcmd;

	if (!move->msec) return;

	// set the edict fields
	sv_client->edict->fields.sv->button0 = move->buttons & 1;
	sv_client->edict->fields.sv->button2 = (move->buttons & 2)>>1;
	if (move->impulse) sv_client->edict->fields.sv->impulse = move->impulse;

	// only send the impulse to qc once
	move->impulse = 0;
	VectorCopy(sv_client->edict->priv.sv->client->viewangles, sv_client->edict->fields.sv->v_angle);
}

/*
===================
SV_ExecuteClientMessage

The current net_message is parsed for the given client
===================
*/
#define	MAX_STRINGCMDS	8

void SV_ExecuteClientMessage (client_t *cl)
{
	int		c;
	char		*s;

	usercmd_t		nullcmd;
	usercmd_t		oldest, oldcmd, newcmd;
	int		net_drop;
	int		stringCmdCount;
	int		checksum, calculatedChecksum;
	int		checksumIndex;
	bool	move_issued;
	int		lastframe;

	sv_client = cl;
	sv_player = sv_client->edict;

	// only allow one move command
	move_issued = false;
	stringCmdCount = 0;

	while (1)
	{
		if (net_message.readcount > net_message.cursize)
		{
			MsgWarn("SV_ReadClientMessage: bad read\n");
			SV_DropClient (cl);
			return;
		}	

		c = MSG_ReadByte (&net_message);
		if (c == -1) break;
				
		switch (c)
		{
		default:
			MsgWarn("SV_ReadClientMessage: unknown command char\n");
			SV_DropClient (cl);
			return;
					
		case clc_nop:
			break;

		case clc_userinfo:
			strncpy (cl->userinfo, MSG_ReadString (&net_message), sizeof(cl->userinfo)-1);
			SV_UserinfoChanged (cl);
			break;

		case clc_move:
			if (move_issued) return; // someone is trying to cheat...

			move_issued = true;
			checksumIndex = net_message.readcount;
			checksum = MSG_ReadByte (&net_message);
			lastframe = MSG_ReadLong (&net_message);
			if (lastframe != cl->lastframe)
			{
				cl->lastframe = lastframe;
				if (cl->lastframe > 0)
				{
					cl->frame_latency[cl->lastframe&(LATENCY_COUNTS-1)] = svs.realtime - cl->frames[cl->lastframe & UPDATE_MASK].senttime;
				}
			}

			memset (&nullcmd, 0, sizeof(nullcmd));
			MSG_ReadDeltaUsercmd (&net_message, &nullcmd, &oldest);
			MSG_ReadDeltaUsercmd (&net_message, &oldest, &oldcmd);
			MSG_ReadDeltaUsercmd (&net_message, &oldcmd, &newcmd);

			if ( cl->state != cs_spawned )
			{
				cl->lastframe = -1;
				break;
			}

			// if the checksum fails, ignore the rest of the packet
			calculatedChecksum = COM_BlockSequenceCRCByte(net_message.data + checksumIndex + 1, net_message.readcount - checksumIndex - 1, cl->netchan.incoming_sequence);

			if (calculatedChecksum != checksum)
			{
				MsgWarn("SV_ExecuteClientMessage: failed command checksum for %s (%d != %d)/%d\n", cl->name, calculatedChecksum, checksum,  cl->netchan.incoming_sequence);
				return;
			}

			if (!sv_paused->value)
			{
				net_drop = cl->netchan.dropped;
				if (net_drop < 20)
				{
					while (net_drop > 2)
					{
						SV_ClientRun (cl, &cl->lastcmd);
						net_drop--;
					}
					if (net_drop > 1) SV_ClientRun (cl, &oldest);
					if (net_drop > 0) SV_ClientRun (cl, &oldcmd);
				}
				SV_ClientRun (cl, &newcmd);
			}
			cl->lastcmd = newcmd;
			break;

		case clc_stringcmd:	
			s = MSG_ReadString (&net_message);

			// malicious users may try using too many string commands
			if (++stringCmdCount < MAX_STRINGCMDS) SV_ExecuteUserCommand (s);
			if (cl->state == cs_zombie) return; // disconnect command
			break;
		}
	}
}