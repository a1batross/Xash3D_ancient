#include "common.h"
#include "server.h"

edict_t	*pm_passent;

// pmove doesn't need to know about passent and contentmask
void PM_trace( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, trace_t *tr )
{
	if( pm_passent->progs.sv->health > 0 )
		*tr = SV_Trace (start, mins, maxs, end, MOVE_NORMAL, pm_passent, MASK_PLAYERSOLID );
	*tr = SV_Trace (start, mins, maxs, end, MOVE_NORMAL, pm_passent, MASK_DEADSOLID );
}

int PM_pointcontents( vec3_t point )
{
	return SV_PointContents( point );
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

	num = SV_AreaEdicts(ent->progs.sv->absmin, ent->progs.sv->absmax, touch, host.max_edicts );

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
			PRVM_ExecuteProgram (hit->progs.sv->touch, "pev->touch");
		}
	}

	// restore state
	PRVM_POP_GLOBALS;
}

static edict_t	*current_player;
static sv_client_t	*current_client;
static vec3_t	forward, right, up;
float		xyspeed, bobmove, bobfracsin;	// sin(bobfrac*M_PI)
int		bobcycle;			// odd cycles are right foot going forward


/*
===============
SV_CalcRoll

===============
*/
static float SV_CalcRoll (vec3_t angles, vec3_t velocity)
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

void SV_CalcViewOffset (edict_t *ent)
{
	float		*angles;
	float		bob;
	float		delta;
	vec3_t		v;

	// base angles
	angles = ent->priv.sv->client->ps.punch_angles;

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

	VectorCopy( ent->progs.sv->view_ofs, v ); // base origin
	if( ent->priv.sv->client->ps.pm_flags & PMF_DUCKED )
		v[2] = -2;

	// add bob height
	bob = bobfracsin * xyspeed * 0.005;
	if( bob > 6 ) bob = 6;
	v[2] += bob;

	v[0] = bound( -14, v[0], 14 );
	v[1] = bound( -14, v[1], 14 );
	v[2] = bound( -22, v[2], 48 );
	VectorCopy( v, ent->priv.sv->client->ps.viewoffset );
}

void ClientEndServerFrame (edict_t *ent)
{
	float	bobtime = 0;

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
	VectorScale(ent->progs.sv->origin, SV_COORD_FRAC, current_client->ps.origin ); 
	VectorScale(ent->progs.sv->velocity, SV_COORD_FRAC, current_client->ps.velocity ); 
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

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	xyspeed = sqrt(ent->progs.sv->velocity[0] * ent->progs.sv->velocity[0] + ent->progs.sv->velocity[1] * ent->progs.sv->velocity[1]);

	if( xyspeed < 5 ) bobmove = 0;
	else if (ent->progs.sv->groundentity)
	{	
		// so bobbing only cycles when on ground
		if (xyspeed > 210) bobmove = 0.25;
		else if (xyspeed > 100) bobmove = 0.125;
		else bobmove = 0.0625;
	}
	
	bobtime += bobmove;

	if (current_client->ps.pm_flags & PMF_DUCKED)
		bobtime *= 4;

	bobcycle = (int)bobtime;
	bobfracsin = fabs(sin(bobtime*M_PI));

	// determine the view offsets
	SV_CalcViewOffset (ent);
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
	;
	//ClientEndServerFrames();
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
	sv_client_t	*client;
	edict_t		*other;
	pmove_t		pm;
	vec3_t		view;
	vec3_t		oldorigin, oldvelocity;
	int		i, j;

	client = ent->priv.sv->client;

	// call standard client pre-think
	prog->globals.sv->time = sv.time;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram (prog->globals.sv->PlayerPreThink, "PlayerPreThink");

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

	if(ent->progs.sv->teleport_time) 
	{
		// next frame will be cleared teleport flag
		client->ps.pm_flags |= PMF_TIME_TELEPORT; 
		ent->progs.sv->teleport_time = 0;
	}
	else client->ps.pm_flags &= ~PMF_TIME_TELEPORT; 

	pm.ps = client->ps;
	memcpy( &client->lastcmd, ucmd, sizeof(usercmd_t));//IMPORTANT!!!

	VectorScale(ent->progs.sv->origin, SV_COORD_FRAC, pm.ps.origin );
	VectorScale(ent->progs.sv->velocity, SV_COORD_FRAC, pm.ps.velocity );

	pm.cmd = *ucmd;

	pm.trace = PM_trace; // adds default parms
	pm.pointcontents = PM_pointcontents;

	// perform a pmove
	pe->PlayerMove( &pm, false );

	// save results of pmove
	client->ps = pm.ps;

	VectorScale(pm.ps.origin, CL_COORD_FRAC, ent->progs.sv->origin);
	VectorScale(pm.ps.velocity, CL_COORD_FRAC, ent->progs.sv->velocity);
	VectorCopy(pm.mins, ent->progs.sv->mins);
	VectorCopy(pm.maxs, ent->progs.sv->maxs);
	VectorCopy(pm.ps.viewangles, ent->progs.sv->v_angle);
	VectorCopy(pm.ps.viewangles, ent->progs.sv->angles);
	if( pm.groundentity )
		ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG( pm.groundentity );
	else ent->progs.sv->groundentity = 0;

	// copy viewmodel info
	client->ps.vmodel.frame = ent->progs.sv->v_frame;
	client->ps.vmodel.body = ent->progs.sv->v_body;
	client->ps.vmodel.skin = ent->progs.sv->v_skin;
	client->ps.vmodel.sequence = ent->progs.sv->v_sequence;

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
			PRVM_ExecuteProgram (other->progs.sv->touch, "pev->touch");
		}
	}
	PRVM_POP_GLOBALS;

	// call standard player post-think
	prog->globals.sv->time = sv.time;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram (prog->globals.sv->PlayerPostThink, "PlayerPostThink");
}

/*
==================
SV_StartParticle

Make sure the event gets sent to all clients
==================
*/
void SV_StartParticle( vec3_t org, vec3_t dir, int color, int count )
{
	MsgDev( D_ERROR, "SV_StartParticle: implement me\n");
}