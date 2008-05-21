#include "engine.h"
#include "server.h"

edict_t	*pm_passent;

// pmove doesn't need to know about passent and contentmask
trace_t PM_trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	if (pm_passent->progs.sv->health > 0)
		return SV_Trace (start, mins, maxs, end, pm_passent, MASK_PLAYERSOLID);
	return SV_Trace (start, mins, maxs, end, pm_passent, MASK_DEADSOLID);
}

int PM_pointcontents( vec3_t point )
{
	return SV_PointContents( point, pm_passent );
}

/*
===========
PutClientInServer

Called when a player connects to a server or respawns in
a deathmatch.
============
*/
void SV_PutClientInServer (edict_t *ent)
{
	int		index;
	gclient_t		*client;
	int		i;
          
	index = PRVM_NUM_FOR_EDICT(ent) - 1;
	client = ent->priv.sv->client;

	prog->globals.sv->time = sv.time;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG( ent );

	if(sv.loadgame)
	{
	}
	else PRVM_ExecuteProgram (prog->globals.sv->PutClientInServer, "QC function PutClientInServer is missing");

	ent->priv.sv->client = svs.gclients + index;
	ent->priv.sv->free = false;
	(int)ent->progs.sv->flags &= ~FL_DEADMONSTER;
 
	// clear playerstate values
	memset (&ent->priv.sv->client->ps, 0, sizeof(client->ps));

	// info_player_start
	VectorCopy(ent->progs.sv->origin, client->ps.origin);  

	client->ps.fov = 90;
	client->ps.fov = bound(1, client->ps.fov, 160);
	client->ps.vmodel.index = SV_ModelIndex(PRVM_GetString(ent->progs.sv->weaponmodel));

	if(sv.loadgame)
	{
	}
	else
	{
		ent->progs.sv->frame = 0;
		ent->progs.sv->origin[2] += 1; // make sure off ground
	}

	VectorCopy(ent->progs.sv->origin, ent->progs.sv->old_origin);

	// set the delta angle
	for (i = 0; i < 3; i++)
	{
		client->ps.delta_angles[i] = ANGLE2SHORT(ent->progs.sv->angles[i]);
	}

	if(sv.loadgame)
	{
	}
	else
	{
		ent->progs.sv->angles[PITCH] = 0;
		ent->progs.sv->angles[ROLL] = 0;
	}

	VectorCopy(ent->progs.sv->angles, client->ps.viewangles);
	SV_LinkEdict(ent);
}

/*
============
SV_TouchTriggers

============
*/
void SV_TouchTriggers (edict_t *ent)
{
	int		i, num;
	edict_t		*touch[MAX_EDICTS], *hit;

	// dead things don't activate triggers!
	if ((ent->priv.sv->client || ((int)ent->progs.sv->flags & FL_MONSTER)) && (ent->progs.sv->health <= 0))
		return;

	num = SV_AreaEdicts(ent->progs.sv->absmin, ent->progs.sv->absmax, touch, MAX_EDICTS );

	PRVM_PUSH_GLOBALS;

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for (i = 0; i < num; i++)
	{
		hit = touch[i];
		if (hit->priv.sv->free) continue;

		prog->globals.sv->pev = PRVM_EDICT_TO_PROG(hit);
		prog->globals.sv->other = PRVM_EDICT_TO_PROG(ent);
		prog->globals.sv->time = sv.time;
		if( hit->progs.sv->touch )
		{
			PRVM_ExecuteProgram (hit->progs.sv->touch, "QC function pev->touch is missing\n");
		}
	}

	// restore state
	PRVM_POP_GLOBALS;
}

static edict_t	*current_player;
static gclient_t	*current_client;
static vec3_t	forward, right, up;
float		xyspeed, bobmove, bobfracsin;	// sin(bobfrac*M_PI)
int		bobcycle;			// odd cycles are right foot going forward


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
	
	value = 2;

	if (side < 200) side = side * value / 200;
	else side = value;
	
	return side*sign;
	
}

/*
==============
SV_CalcGunOffset
==============
*/
void SV_CalcGunOffset (edict_t *ent)
{
	int		i;
	float		delta;

	// gun angles from bobbing
	ent->priv.sv->client->ps.vmodel.angles[ROLL] = xyspeed * bobfracsin * 0.005;
	ent->priv.sv->client->ps.vmodel.angles[YAW] = xyspeed * bobfracsin * 0.01;
	if (bobcycle & 1)
	{
		ent->priv.sv->client->ps.vmodel.angles[ROLL] = -ent->priv.sv->client->ps.vmodel.angles[ROLL];
		ent->priv.sv->client->ps.vmodel.angles[YAW] = -ent->priv.sv->client->ps.vmodel.angles[YAW];
	}

	ent->priv.sv->client->ps.vmodel.angles[PITCH] = xyspeed * bobfracsin * 0.005;
	ent->priv.sv->client->ps.viewoffset[2] = ent->priv.sv->client->ps.viewheight;

	// gun angles from delta movement
	for (i = 0; i < 3; i++)
	{
		delta = ent->priv.sv->client->ps.oldviewangles[i] - ent->priv.sv->client->ps.viewangles[i];
		if (delta > 180) delta -= 360;
		if (delta < -180) delta += 360;
		if (delta > 45) delta = 45;
		if (delta < -45) delta = -45;
		if (i == YAW) ent->priv.sv->client->ps.vmodel.angles[ROLL] += 0.1*delta;
		ent->priv.sv->client->ps.vmodel.angles[i] += 0.2 * delta;
	}

	// gun height
	VectorClear (ent->priv.sv->client->ps.vmodel.offset);

	// gun_x / gun_y / gun_z are development tools
	for (i = 0; i < 3; i++)
	{
		ent->priv.sv->client->ps.vmodel.offset[i] += forward[i];
		ent->priv.sv->client->ps.vmodel.offset[i] += right[i];
		ent->priv.sv->client->ps.vmodel.offset[i] += up[i];
	}
}


void SV_CalcViewOffset (edict_t *ent)
{
	float		*angles;
	float		bob;
	float		delta;
	vec3_t		v;

	// base angles
	angles = ent->priv.sv->client->ps.kick_angles;

	VectorCopy(ent->progs.sv->punchangle, angles);
	// add angles based on velocity
	delta = DotProduct (ent->progs.sv->velocity, forward);
	angles[PITCH] += delta * 0.002;
		
	delta = DotProduct (ent->progs.sv->velocity, right);
	angles[ROLL] += delta * 0.005;

	// add angles based on bob
	delta = bobfracsin * 0.002 * xyspeed;
	if (ent->priv.sv->client->ps.pm_flags & PMF_DUCKED)
		delta *= 6; // crouching
	angles[PITCH] += delta;
	delta = bobfracsin * 0.002 * xyspeed;

	if (ent->priv.sv->client->ps.pm_flags & PMF_DUCKED)
		delta *= 6; // crouching
	if (bobcycle & 1) delta = -delta;
	angles[ROLL] += delta;


	// base origin
	VectorClear (v);

	// add view height
	v[2] += 22;

	// add bob height
	bob = bobfracsin * xyspeed * 0.005;
	if (bob > 6) bob = 6;
	v[2] += bob;


	v[0] = bound(-14, v[0], 14);
	v[1] = bound(-14, v[1], 14);
	v[2] = bound(-22, v[0], 30);

	VectorCopy (v, ent->priv.sv->client->ps.viewoffset);
}

void SV_SetStats (edict_t *ent)
{
	ent->priv.sv->client->ps.stats[STAT_HEALTH_ICON] = SV_ImageIndex("hud/i_health");
	ent->priv.sv->client->ps.stats[STAT_HEALTH] = ent->progs.sv->health;
}

void ClientEndServerFrame (edict_t *ent)
{
	float		bobtime;

	current_player = ent;
	current_client = ent->priv.sv->client;
	
	//
	// If the origin or velocity have changed since ClientThink(),
	// update the pmove values.  This will happen when the client
	// is pushed by a bmodel or kicked by an explosion.
	// 
	// If it wasn't updated here, the view position would lag a frame
	// behind the body position when pushed -- "sinking into plats"
	//
	VectorCopy(ent->progs.sv->origin, current_client->ps.origin ); 
	VectorCopy(ent->progs.sv->velocity, current_client->ps.velocity ); 
	AngleVectors (ent->priv.sv->client->ps.viewangles, forward, right, up);

	//
	// set model angles from view angles so other things in
	// the world can tell which direction you are looking
	//
	if (ent->priv.sv->client->ps.viewangles[PITCH] > 180) 
		ent->progs.sv->angles[PITCH] = (-360 + ent->priv.sv->client->ps.viewangles[PITCH])/3;
	else ent->progs.sv->angles[PITCH] = ent->priv.sv->client->ps.viewangles[PITCH]/3;

	ent->progs.sv->angles[YAW] = ent->priv.sv->client->ps.viewangles[YAW];
	ent->progs.sv->angles[ROLL] = 0;
	ent->progs.sv->angles[ROLL] = SV_CalcRoll (ent->progs.sv->angles, ent->progs.sv->velocity)*4;
	ent->priv.sv->client->ps.pm_time = ent->progs.sv->teleport_time * 1000; //in msecs

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	xyspeed = sqrt(ent->progs.sv->velocity[0] * ent->progs.sv->velocity[0] + ent->progs.sv->velocity[1] * ent->progs.sv->velocity[1]);

	if (xyspeed < 5)
	{
		bobmove = 0;
		current_client->ps.bobtime = 0;	// start at beginning of cycle again
	}
	else if (ent->progs.sv->groundentity)
	{	
		// so bobbing only cycles when on ground
		if (xyspeed > 210) bobmove = 0.25;
		else if (xyspeed > 100) bobmove = 0.125;
		else bobmove = 0.0625;
	}
	
	bobtime = (current_client->ps.bobtime += bobmove);

	if (current_client->ps.pm_flags & PMF_DUCKED)
		bobtime *= 4;

	bobcycle = (int)bobtime;
	bobfracsin = fabs(sin(bobtime*M_PI));

	// determine the view offsets
	SV_CalcViewOffset (ent);

	// determine the gun offsets
	SV_CalcGunOffset (ent);

	SV_SetStats( ent );

	// if the scoreboard is up, update it
	if (!(sv.framenum & 31))
	{
	}
}

/*
=================
ClientEndServerFrames
=================
*/
void ClientEndServerFrames (void)
{
	int		i;

	// calc the player views now that all pushing
	// and damage has been added
	for (i = 0; i < maxclients->value; i++)
	{
		if(svs.clients[i].state != cs_spawned) continue;
		ClientEndServerFrame (svs.clients[i].edict);
	}
}

/*
================
SV_RunFrame

Advances the world by 0.1 seconds
================
*/
void SV_RunFrame( void )
{
	int		i;
	edict_t		*ent;

	// let the progs know that a new frame has started
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.sv->other = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.sv->time = sv.time;
	prog->globals.sv->frametime = sv.frametime;
	PRVM_ExecuteProgram (prog->globals.sv->StartFrame, "QC function StartFrame is missing");

	for (i = 1; i < prog->num_edicts; i++ )
	{
		ent = PRVM_EDICT_NUM(i);
		if (ent->priv.sv->free) continue;

		VectorCopy (ent->progs.sv->origin, ent->progs.sv->old_origin);

		// don't apply phys on clients
		if (i > 0 && i <= maxclients->value) continue;
		SV_Physics( ent );
	}

	// build the playerstate_t structures for all players
	ClientEndServerFrames ();

	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.sv->other = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.sv->time = sv.time;
	PRVM_ExecuteProgram (prog->globals.sv->EndFrame, "QC function EndFrame is missing");

	// decrement prog->num_edicts if the highest number entities died
	for ( ;PRVM_EDICT_NUM(prog->num_edicts - 1)->priv.sv->free; prog->num_edicts-- );
}

bool SV_ClientConnect (edict_t *ent, char *userinfo)
{
	// they can connect
	ent->priv.sv->client = svs.gclients + PRVM_NUM_FOR_EDICT(ent) - 1;
	ent->progs.sv->flags = 0; // make sure we start with known default
	ent->progs.sv->health = 100;

	MsgDev(D_NOTE, "SV_ClientConnect()\n");
	prog->globals.sv->time = sv.time;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram (prog->globals.sv->ClientConnect, "QC function ClientConnect is missing");

	return true;
}

void SV_ClientUserinfoChanged (edict_t *ent, char *userinfo)
{
	char	*s;
	int	playernum;
          
	// check for malformed or illegal info strings
	if (!Info_Validate(userinfo))
	{
		strcpy (userinfo, "\\name\\badinfo\\skin\\male/grunt");
	}

	// set skin
	s = Info_ValueForKey (userinfo, "skin");

	playernum = PRVM_NUM_FOR_EDICT(ent);

	// combine name and skin into a configstring
	SV_ConfigString (CS_PLAYERSKINS + playernum, va("%s\\%s", Info_ValueForKey (userinfo, "name"), Info_ValueForKey (userinfo, "skin")));

	ent->priv.sv->client->ps.fov = bound(1, atoi(Info_ValueForKey(userinfo, "fov")), 160);
}

/*
===========
SV_ClientBegin

called when a client has finished connecting, and is ready
to be placed into the game.  This will happen every level load.
============
*/
void SV_ClientBegin (edict_t *ent)
{
	int		i;

	ent->priv.sv->client = svs.gclients + PRVM_NUM_FOR_EDICT(ent) - 1;

	// if there is already a body waiting for us (a loadgame), just
	// take it, otherwise spawn one from scratch
	if (ent->priv.sv->free)
	{
		// the client has cleared the client side viewangles upon
		// connecting to the server, which is different than the
		// state when the game is saved, so we need to compensate
		// with deltaangles
		for (i = 0; i < 3; i++)
			ent->priv.sv->client->ps.delta_angles[i] = ANGLE2SHORT(ent->priv.sv->client->ps.viewangles[i]);
	}
	else
	{
		// a spawn point will completely reinitialize the entity
		// except for the persistant data that was initialized at
		// ClientConnect() time
		SV_InitEdict (ent);
		SV_PutClientInServer (ent);
	}

	// make sure all view stuff is valid
	ClientEndServerFrame (ent);
}

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame.
==============
*/
void ClientThink (edict_t *ent, usercmd_t *ucmd)
{
	gclient_t		*client;
	edict_t		*other;
	pmove_t		pm;
	vec3_t		view;
	vec3_t		oldorigin, oldvelocity;
	int		i, j;

	client = ent->priv.sv->client;

	// call standard client pre-think
	prog->globals.sv->time = sv.time;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram (prog->globals.sv->PlayerPreThink, "QC function PlayerPreThink is missing");

	VectorCopy(ent->progs.sv->origin, oldorigin);
	VectorCopy(ent->progs.sv->velocity, oldvelocity);

	ent->priv.sv->client->ps.pm_flags &= ~PMF_NO_PREDICTION;

	VectorCopy(ent->progs.sv->origin, view);

	pm_passent = ent;

	// set up for pmove
	memset (&pm, 0, sizeof(pm));

	if (ent->progs.sv->movetype == MOVETYPE_NOCLIP) client->ps.pm_type = PM_SPECTATOR;
	else client->ps.pm_type = PM_NORMAL;
	client->ps.gravity = sv_gravity->value;

	if(ent->progs.sv->teleport_time) client->ps.pm_flags |= PMF_TIME_TELEPORT; 
	else client->ps.pm_flags &= ~PMF_TIME_TELEPORT; 

	pm.ps = client->ps;

	VectorCopy(ent->progs.sv->origin, pm.ps.origin );
	VectorCopy(ent->progs.sv->velocity, pm.ps.velocity );

	pm.cmd = *ucmd;

	pm.trace = PM_trace; // adds default parms
	pm.pointcontents = PM_pointcontents;

	// perform a pmove
	Pmove (&pm);

	// save results of pmove
	client->ps = pm.ps;

	VectorCopy(pm.ps.origin, ent->progs.sv->origin);
	VectorCopy(pm.ps.velocity, ent->progs.sv->velocity);
	VectorCopy(pm.mins, ent->progs.sv->mins);
	VectorCopy(pm.maxs, ent->progs.sv->maxs);
	VectorCopy(pm.ps.viewangles, client->ps.viewangles);

	SV_LinkEdict(ent);

	if (ent->progs.sv->movetype != MOVETYPE_NOCLIP)
		SV_TouchTriggers (ent);

	PRVM_PUSH_GLOBALS;
	for (i = 0; i < pm.numtouch; i++)
	{
		other = pm.touchents[i];
		for (j = 0; j < i; j++)
		{
			if (pm.touchents[j] == other)
				break;
		}
		if (j != i) continue; // duplicated
		if (!other->progs.sv->touch) continue;
		
		prog->globals.sv->pev = PRVM_EDICT_TO_PROG(other);
		prog->globals.sv->other = PRVM_EDICT_TO_PROG(ent);
		prog->globals.sv->time = sv.time;
		if( other->progs.sv->touch )
		{
			PRVM_ExecuteProgram (other->progs.sv->touch, "QC function pev->touch is missing\n");
		}
	}
	PRVM_POP_GLOBALS;

	// call standard player post-think
	prog->globals.sv->time = sv.time;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram (prog->globals.sv->PlayerPostThink, "QC function PlayerPostThink is missing");
}

/*
===========
SV_ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.
============
*/
void SV_ClientDisconnect (edict_t *ent)
{
	int	playernum;

	if (!ent->priv.sv->client) return;

	SV_UnlinkEdict(ent);
	ent->progs.sv->modelindex = 0;
	ent->progs.sv->solid = SOLID_NOT;
	ent->priv.sv->free = true;
	ent->progs.sv->classname = PRVM_SetEngineString("disconnected");

	playernum = PRVM_NUM_FOR_EDICT(ent) - 1;
	SV_ConfigString (CS_PLAYERSKINS + playernum, "");

}

/*
==================
SV_StartParticle

Make sure the event gets sent to all clients
==================
*/
void SV_StartParticle (vec3_t org, vec3_t dir, int color, int count)
{
	MsgWarn("SV_StartParticle: implement me\n");
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
		n = PRVM_NUM_FOR_EDICT(ent);
		if (n < 1 || n > maxclients->value)
			Host_Error("cprintf to a non-client\n");
	}

	va_start (argptr,fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);

	if (ent) SV_ClientPrintf (svs.clients+(n-1), level, "%s", msg);
	else Msg ("%s", msg);
}


/*
==================
Cmd_Say_f
==================
*/
void Cmd_Say_f (edict_t *ent, bool team, bool arg0)
{
	int		j;
	edict_t		*other;
	char		*p;
	char		text[2048];

	if (Cmd_Argc () < 2 && !arg0) return;

	sprintf (text, "%s: ", "all");

	if (arg0)
	{
		strcat (text, Cmd_Argv(0));
		strcat (text, " ");
		strcat (text, Cmd_Args());
	}
	else
	{
		p = Cmd_Args();

		if (*p == '"')
		{
			p++;
			p[strlen(p)-1] = 0;
		}
		strcat(text, p);
	}

	// don't let text be too long for malicious reasons
	if (strlen(text) > 150) text[150] = 0;

	strcat(text, "\n");

	if (dedicated->value) 
		PF_cprintf(NULL, PRINT_CHAT, "%s", text);

	for (j = 1; j <= maxclients->value; j++)
	{
		other = PRVM_EDICT_NUM(j);
		if (other->priv.sv->free) continue;
		if (!other->priv.sv->client) continue;
		PF_cprintf(other, PRINT_CHAT, "%s", text);
	}
}

/*
==================
HelpComputer

Draw help computer.
==================
*/
void SV_HelpComputer (edict_t *ent)
{
	char	string[1024];
	char	*sk = "medium";

	sprintf (string, "xv 32 yv 8 picn help "	// background
		"xv 202 yv 12 string2 \"%s\" "	// skill
		"xv 0 yv 24 cstring2 \"%s\" "		// level name
		"xv 0 yv 54 cstring2 \"%s\" "		// help 1
		"xv 0 yv 110 cstring2 \"%s\" "	// help 2
		"xv 50 yv 164 string2 \" kills     goals    secrets\" "
		"xv 50 yv 172 string2 \"%3i/%3i     %i/%i       %i/%i\" ", 
		sk,
		sv.name,
		"",
		"",
		0, 0, 
		0, 0,
		0, 0);

	MSG_Begin (svc_layout);
		MSG_WriteString (&sv.multicast, string);
	MSG_Send (MSG_ONE_R, NULL, ent );
}

/*
==================
Cmd_Help_f

Display the current help message
==================
*/
void Cmd_Help_f (edict_t *ent)
{
	SV_HelpComputer (ent);
}

/*
=================
SV_ClientCommand
=================
*/
void SV_ClientCommand (edict_t *ent)
{
	char	*cmd;
	char	*parm;

	if (!ent->priv.sv->client) return; // not fully in game yet

	cmd = Cmd_Argv(0);

	if(Cmd_Argc() < 2) parm = NULL;
	else parm = Cmd_Argv(1);

	if (strcasecmp (cmd, "say") == 0)
	{
		Cmd_Say_f (ent, false, false);
		return;
	}
	if (strcasecmp (cmd, "say_team") == 0)
	{
		Cmd_Say_f (ent, true, false);
		return;
	}
	if (strcasecmp (cmd, "help") == 0)
	{
		Cmd_Help_f (ent);
		return;
	}
}

void SV_Transform( sv_edict_t *ed, matrix4x3 transform )
{
	edict_t	*edict;
	vec3_t	origin, angles;
	matrix4x4	objmatrix;

	if(!ed) return;
	edict = PRVM_EDICT_NUM( ed->serialnumber );

	// save matrix (fourth value will be reset on save\load)
	VectorCopy( transform[0], edict->progs.sv->m_pmatrix[0] );
	VectorCopy( transform[1], edict->progs.sv->m_pmatrix[1] );
	VectorCopy( transform[2], edict->progs.sv->m_pmatrix[2] );
	VectorCopy( transform[3], edict->progs.sv->m_pmatrix[3] );

	MatrixLoadIdentity( objmatrix );
	VectorCopy( transform[0], objmatrix[0] );
	VectorCopy( transform[1], objmatrix[1] );
	VectorCopy( transform[2], objmatrix[2] );
	VectorCopy( transform[3], objmatrix[3] );
	MatrixAngles( objmatrix, origin, angles );

	VectorCopy( origin, edict->progs.sv->origin );
	VectorCopy( angles, edict->progs.sv->angles );

	// refresh force and torque
	pe->GetForce( ed->physbody, edict->progs.sv->velocity, edict->progs.sv->avelocity, edict->progs.sv->force, edict->progs.sv->torque );
	pe->GetMassCentre( ed->physbody, edict->progs.sv->m_pcentre );
}  