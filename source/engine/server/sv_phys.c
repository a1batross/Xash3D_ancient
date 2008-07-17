//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_physics.c - server physic
//=======================================================================

#include "common.h"
#include "server.h"

#define STEPSIZE		18

/*
============
SV_ClampMove

clamp the move to 1/8 units, so the position will
be accurate for client side prediction
============
*/
void SV_ClampCoord( vec3_t coord )
{
	coord[0] -= SV_COORD_FRAC * floor(coord[0] * CL_COORD_FRAC);
	coord[1] -= SV_COORD_FRAC * floor(coord[1] * CL_COORD_FRAC);
	coord[2] -= SV_COORD_FRAC * floor(coord[2] * CL_COORD_FRAC);
}

void SV_ClampAngle( vec3_t angle )
{
	angle[0] -= SV_ANGLE_FRAC * floor(angle[0] * CL_ANGLE_FRAC);
	angle[1] -= SV_ANGLE_FRAC * floor(angle[1] * CL_ANGLE_FRAC);
	angle[2] -= SV_ANGLE_FRAC * floor(angle[2] * CL_ANGLE_FRAC);
}

/*
==================
ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
void SV_ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	int	i;
	float	backoff;

	backoff = -DotProduct (in, normal) * overbounce;
	VectorMA(in, backoff, normal, out);

	for (i = 0; i < 3; i++)
	{
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}
}

/*
=============
SV_CheckBottom

Returns false if any part of the bottom of the entity is off an edge that
is not a staircase.

=============
*/
int c_yes, c_no;

bool SV_CheckBottom (edict_t *ent)
{
	vec3_t		mins, maxs, start, stop;
	trace_t		trace;
	int		x, y;
	float		mid, bottom;
	
	VectorAdd (ent->progs.sv->origin, ent->progs.sv->mins, mins);
	VectorAdd (ent->progs.sv->origin, ent->progs.sv->maxs, maxs);

	// if all of the points under the corners are solid world, don't bother
	// with the tougher checks
	// the corners must be within 16 of the midpoint
	start[2] = mins[2] - 1;

	for (x = 0; x <= 1; x++)
	{
		for (y = 0; y <= 1; y++)
		{
			start[0] = x ? maxs[0] : mins[0];
			start[1] = y ? maxs[1] : mins[1];
			if (SV_PointContents(start, ent ) != CONTENTS_SOLID)
				goto realcheck;
		}
	}

	c_yes++;
	return true; // we got out easy

realcheck:
	c_no++;

	// check it for real...
	start[2] = mins[2];
	
	// the midpoint must be within 16 of the bottom
	start[0] = stop[0] = (mins[0] + maxs[0])*0.5;
	start[1] = stop[1] = (mins[1] + maxs[1])*0.5;
	stop[2] = start[2] - 2*STEPSIZE;
	trace = SV_Trace (start, vec3_origin, vec3_origin, stop, ent, MASK_MONSTERSOLID);

	if (trace.fraction == 1.0)
		return false;
	mid = bottom = trace.endpos[2];
	
	// the corners must be within 16 of the midpoint	
	for (x = 0; x <= 1; x++)
	{
		for ( y = 0; y <= 1; y++)
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];
			
			trace = SV_Trace (start, vec3_origin, vec3_origin, stop, ent, MASK_MONSTERSOLID);
			
			if (trace.fraction != 1.0 && trace.endpos[2] > bottom)
				bottom = trace.endpos[2];
			if (trace.fraction == 1.0 || mid - trace.endpos[2] > STEPSIZE)
				return false;
		}
	}

	c_yes++;
	return true;
}

/*
======================
SV_FixCheckBottom

======================
*/
void SV_FixCheckBottom (edict_t *ent)
{
	ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags | AI_PARTIALONGROUND;
}


void SV_CheckVelocity (edict_t *ent)
{
	int	i;
	float	wishspeed;

	// bound velocity
	for (i = 0; i < 3; i++)
	{
		if (IS_NAN(ent->progs.sv->velocity[i]))
		{
			MsgDev( D_WARN, "Got a NaN velocity on %s\n", PRVM_GetString(ent->progs.sv->classname));
			ent->progs.sv->velocity[i] = 0;
		}
		if (IS_NAN(ent->progs.sv->origin[i]))
		{
			MsgDev( D_WARN, "Got a NaN origin on %s\n", PRVM_GetString(ent->progs.sv->classname));
			ent->progs.sv->origin[i] = 0;
		}
	}

	// LordHavoc: max velocity fix, inspired by Maddes's source fixes, but this is faster
	wishspeed = DotProduct(ent->progs.sv->velocity, ent->progs.sv->velocity);
	if (wishspeed > sv_maxvelocity->value * sv_maxvelocity->value)
	{
		wishspeed = sv_maxvelocity->value / sqrt(wishspeed);
		ent->progs.sv->velocity[0] *= wishspeed;
		ent->progs.sv->velocity[1] *= wishspeed;
		ent->progs.sv->velocity[2] *= wishspeed;
	}
}

/*
============
SV_WallFriction

============
*/
void SV_WallFriction (edict_t *ent, float *stepnormal)
{
	float	d, i;
	vec3_t	forward, into, side;

	AngleVectors (ent->progs.sv->v_angle, forward, NULL, NULL);
	if ((d = DotProduct (stepnormal, forward) + 0.5) < 0)
	{
		// cut the tangential velocity
		i = DotProduct (stepnormal, ent->progs.sv->velocity);
		VectorScale (stepnormal, i, into);
		VectorSubtract (ent->progs.sv->velocity, into, side);
		ent->progs.sv->velocity[0] = side[0] * (1 + d);
		ent->progs.sv->velocity[1] = side[1] * (1 + d);
	}
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact (edict_t *e1, trace_t *trace)
{
	edict_t		*e2 = (edict_t *)trace->ent;

	PRVM_PUSH_GLOBALS;

	prog->globals.sv->time = sv.time;
	if (!e1->priv.sv->free && !e2->priv.sv->free && e1->progs.sv->touch && e1->progs.sv->solid != SOLID_NOT)
	{
		prog->globals.sv->pev = PRVM_EDICT_TO_PROG(e1);
		prog->globals.sv->other = PRVM_EDICT_TO_PROG(e2);
		prog->globals.sv->time = sv.time;
		prog->globals.sv->trace_allsolid = trace->allsolid;
		prog->globals.sv->trace_startsolid = trace->startsolid;
		prog->globals.sv->trace_fraction = trace->fraction;
		prog->globals.sv->trace_contents = trace->contents;
		VectorCopy (trace->endpos, prog->globals.sv->trace_endpos);
		VectorCopy (trace->plane.normal, prog->globals.sv->trace_plane_normal);
		prog->globals.sv->trace_plane_dist =  trace->plane.dist;
		if (trace->ent) prog->globals.sv->trace_ent = PRVM_EDICT_TO_PROG(trace->ent);
		else prog->globals.sv->trace_ent = PRVM_EDICT_TO_PROG(prog->edicts);
		PRVM_ExecuteProgram( e1->progs.sv->touch, "pev->touch" );
	}
	if (!e1->priv.sv->free && !e2->priv.sv->free && e2->progs.sv->touch && e2->progs.sv->solid != SOLID_NOT)
	{
		prog->globals.sv->pev = PRVM_EDICT_TO_PROG(e2);
		prog->globals.sv->other = PRVM_EDICT_TO_PROG(e1);
		prog->globals.sv->time = sv.time;
		prog->globals.sv->trace_allsolid = false;
		prog->globals.sv->trace_startsolid = false;
		prog->globals.sv->trace_fraction = 1;
		prog->globals.sv->trace_contents = trace->contents;
		VectorCopy (e2->progs.sv->origin, prog->globals.sv->trace_endpos);
		VectorSet (prog->globals.sv->trace_plane_normal, 0, 0, 1);
		prog->globals.sv->trace_plane_dist = 0;
		prog->globals.sv->trace_ent = PRVM_EDICT_TO_PROG(e1);
		PRVM_ExecuteProgram (e2->progs.sv->touch, "pev->touch");
	}

	PRVM_POP_GLOBALS;
}

/*
============
SV_TestEntityPosition

============
*/
edict_t *SV_TestEntityPosition (edict_t *ent)
{
	trace_t		trace;
	int		mask;

	if (ent->priv.sv->clipmask) mask = ent->priv.sv->clipmask;
	else mask = MASK_SOLID;

	if(ent->progs.sv->solid == SOLID_BSP)
	{
		vec3_t	org, mins, maxs;
		VectorAdd(ent->progs.sv->origin, ent->progs.sv->origin_offset, org);
		VectorSubtract(ent->progs.sv->mins, ent->progs.sv->origin_offset, mins);
		VectorSubtract(ent->progs.sv->maxs, ent->progs.sv->origin_offset, maxs);
		trace = SV_Trace (org, mins, maxs, org, ent, mask);
	}
	else trace = SV_Trace (ent->progs.sv->origin, ent->progs.sv->mins, ent->progs.sv->maxs, ent->progs.sv->origin, ent, mask);

	if (trace.startsolid)
	{
		// Lazarus - work around for players/monsters standing on dead monsters causing
		// those monsters to gib when rotating brush models are in the vicinity
		if ( ((int)ent->progs.sv->flags & FL_DEADMONSTER) && (trace.ent->priv.sv->client || ((int)trace.ent->progs.sv->flags & FL_MONSTER)))
			return NULL;

		// Lazarus - return a bit more useful info than simply "g_edicts"
		if(trace.ent) return trace.ent;
		else return prog->edicts;
	}
	
	return NULL;
}

/*
=============
SV_CheckStuck

This is a big hack to try and fix the rare case of getting stuck in the world
clipping hull.
=============
*/
static void SV_CheckStuck (edict_t *ent)
{
	int i, j, z;
	vec3_t org;

	if (!SV_TestEntityPosition(ent))
	{
		VectorCopy (ent->progs.sv->origin, ent->progs.sv->old_origin);
		return;
	}

	VectorCopy (ent->progs.sv->origin, org);
	VectorCopy (ent->progs.sv->old_origin, ent->progs.sv->origin);
	if (!SV_TestEntityPosition(ent))
	{
		MsgDev( D_INFO, "Unstuck player entity %i (classname \"%s\") by restoring old_origin.\n", (int)PRVM_EDICT_TO_PROG(ent), PRVM_GetString(ent->progs.sv->classname));
		SV_LinkEdict (ent);
		return;
	}

	for (z=-1 ; z< 18 ; z++)
		for (i=-1 ; i <= 1 ; i++)
			for (j=-1 ; j <= 1 ; j++)
			{
				ent->progs.sv->origin[0] = org[0] + i;
				ent->progs.sv->origin[1] = org[1] + j;
				ent->progs.sv->origin[2] = org[2] + z;
				if (!SV_TestEntityPosition(ent))
				{
					MsgDev( D_INFO, "Unstuck player entity %i (classname \"%s\") with offset %f %f %f.\n", (int)PRVM_EDICT_TO_PROG(ent), PRVM_GetString(ent->progs.sv->classname), (float)i, (float)j, (float)z);
					SV_LinkEdict (ent);
					return;
				}
			}

	VectorCopy (org, ent->progs.sv->origin);
	MsgDev( D_ERROR, "Stuck player entity %i (classname \"%s\").\n", (int)PRVM_EDICT_TO_PROG(ent), PRVM_GetString(ent->progs.sv->classname));
}

static void SV_UnstickEntity (edict_t *ent)
{
	int	x, y, z;
	vec3_t	org;

	// if not stuck in a bmodel, just return
	if (!SV_TestEntityPosition(ent))
		return;

	VectorCopy (ent->progs.sv->origin, org);

	for (z = -1; z < 18; z += 6)
	{
		for (x = -1; x <= 1; x++)
		{
			for (y = -1; y <= 1; y++)
			{
				ent->progs.sv->origin[0] = org[0] + x;
				ent->progs.sv->origin[1] = org[1] + y;
				ent->progs.sv->origin[2] = org[2] + z;
				if (!SV_TestEntityPosition(ent))
				{
					MsgDev(D_INFO, "Unstuck entity %i (classname \"%s\") with offset %f %f %f.\n", (int)PRVM_EDICT_TO_PROG(ent), PRVM_GetString(ent->progs.sv->classname), (float)x, (float)y, (float)z);
					SV_LinkEdict (ent);
					return;
				}
			}
		}
	}

	VectorCopy (org, ent->progs.sv->origin);
	MsgDev(D_INFO, "Stuck entity %i (classname \"%s\").\n", (int)PRVM_EDICT_TO_PROG(ent), PRVM_GetString(ent->progs.sv->classname));
}

/*
=============
SV_CheckWater
=============
*/
bool SV_CheckWater (edict_t *ent)
{
	int cont;
	vec3_t point;

	point[0] = ent->progs.sv->origin[0];
	point[1] = ent->progs.sv->origin[1];
	point[2] = ent->progs.sv->origin[2] + ent->progs.sv->mins[2] + 1;

	ent->progs.sv->waterlevel = 0;
	ent->progs.sv->watertype = CONTENTS_NONE;
	cont = SV_PointContents( point, ent );
	if (cont & (MASK_WATER))
	{
		ent->progs.sv->watertype = cont;
		ent->progs.sv->waterlevel = 1;
		point[2] = ent->progs.sv->origin[2] + (ent->progs.sv->mins[2] + ent->progs.sv->maxs[2])*0.5;
		if (SV_PointContents(point, ent ) & (MASK_WATER))
		{
			ent->progs.sv->waterlevel = 2;
			point[2] = ent->progs.sv->origin[2] + ent->progs.sv->view_ofs[2];
			if (SV_PointContents(point, ent ) & (MASK_WATER))
				ent->progs.sv->waterlevel = 3;
		}
	}
	return ent->progs.sv->waterlevel > 1;
}


/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
If stepnormal is not NULL, the plane normal of any vertical wall hit will be stored
============
*/
#define MAX_CLIP_PLANES 32
int SV_FlyMove (edict_t *ent, float time, float *stepnormal)
{
	int	blocked = 0, bumpcount;
	int	i, j, mask, impact, numplanes = 0;
	float	d, time_left;
	vec3_t	dir, end, planes[MAX_CLIP_PLANES], primal_velocity, original_velocity, new_velocity;
	trace_t	trace;

	VectorCopy(ent->progs.sv->velocity, original_velocity);
	VectorCopy(ent->progs.sv->velocity, primal_velocity);

	//setup trace mask
	if ((int)ent->progs.sv->flags & FL_MONSTER) mask = MASK_MONSTERSOLID;
	else if(ent->progs.sv->movetype == MOVETYPE_PUSHABLE) mask = MASK_MONSTERSOLID | MASK_PLAYERSOLID;
	else if(ent->priv.sv->clipmask) mask = ent->priv.sv->clipmask;
	else mask = MASK_SOLID;

	numplanes = 0;
	time_left = time;
	for (bumpcount = 0;bumpcount < MAX_CLIP_PLANES;bumpcount++)
	{
		if (!ent->progs.sv->velocity[0] && !ent->progs.sv->velocity[1] && !ent->progs.sv->velocity[2])
			break;

		VectorMA(ent->progs.sv->origin, time_left, ent->progs.sv->velocity, end);
		trace = SV_Trace (ent->progs.sv->origin, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, mask);

		// break if it moved the entire distance
		if (trace.fraction == 1)
		{
			VectorCopy(trace.endpos, ent->progs.sv->origin);
			break;
		}

		if (!trace.ent)
		{
			Msg("SV_FlyMove: !trace.ent");
			trace.ent = prog->edicts;
		}

		if (((int) ent->progs.sv->aiflags & AI_ONGROUND) && ent->progs.sv->groundentity == PRVM_EDICT_TO_PROG(trace.ent))
			impact = false;
		else
		{
			ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags & ~AI_ONGROUND;
			impact = true;
		}

		if (trace.plane.normal[2])
		{
			if (trace.plane.normal[2] > 0.7)
			{
				// floor
				blocked |= 1;
				ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags | AI_ONGROUND;
				ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
			}
		}
		else
		{
			// step
			blocked |= 2;
			// save the trace for player extrafriction
			if (stepnormal) VectorCopy(trace.plane.normal, stepnormal);
		}

		if (trace.fraction >= 0.001)
		{
			// actually covered some distance
			VectorCopy(trace.endpos, ent->progs.sv->origin);
			VectorCopy(ent->progs.sv->velocity, original_velocity);
			numplanes = 0;
		}

		// run the impact function
		if (impact)
		{
			SV_Impact(ent, &trace);
			// break if removed by the impact function
			if (ent->priv.sv->free)
				break;
		}

		time_left *= 1 - trace.fraction;

		// clipped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{
			// this shouldn't really happen
			VectorClear(ent->progs.sv->velocity);
			blocked = 3;
			break;
		}

		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for (i = 0; i < numplanes; i++)
		{
			SV_ClipVelocity(original_velocity, planes[i], new_velocity, 1);
			for (j = 0; j < numplanes; j++)
			{
				if (j != i) // not ok
				{
					if (DotProduct(new_velocity, planes[j]) < 0)
						break;
				}
			}
			if (j == numplanes) break;
		}

		if (i != numplanes)
		{
			// go along this plane
			VectorCopy(new_velocity, ent->progs.sv->velocity);
		}
		else
		{
			// go along the crease
			if (numplanes != 2)
			{
				VectorClear(ent->progs.sv->velocity);
				blocked = 7;
				break;
			}
			CrossProduct(planes[0], planes[1], dir);
			// LordHavoc: thanks to taniwha of QuakeForge for pointing out this fix for slowed falling in corners
			VectorNormalize(dir);
			d = DotProduct(dir, ent->progs.sv->velocity);
			VectorScale(dir, d, ent->progs.sv->velocity);
		}

		// if current velocity is against the original velocity,
		// stop dead to avoid tiny occilations in sloping corners
		if (DotProduct(ent->progs.sv->velocity, primal_velocity) <= 0)
		{
			VectorClear(ent->progs.sv->velocity);
			break;
		}
	}

	// LordHavoc: this came from QW and allows you to get out of water more easily
	if((int)ent->progs.sv->aiflags & AI_WATERJUMP)
		VectorCopy(primal_velocity, ent->progs.sv->velocity);
	return blocked;
}

/*
============
SV_AddGravity

============
*/
void SV_AddGravity (edict_t *ent)
{
	ent->progs.sv->velocity[2] -= ent->progs.sv->gravity * sv_gravity->value * sv.frametime;
}

/*
=============
SV_RunThink

Runs thinking code for this frame if necessary
=============
*/
bool SV_RunThink (edict_t *ent)
{
	float thinktime;

	thinktime = ent->progs.sv->nextthink;
	if (thinktime <= 0 || thinktime > sv.time + sv.frametime)
		return true;

	// don't let things stay in the past.
	// it is possible to start that way by a trigger with a local time.
	if (thinktime < sv.time) thinktime = sv.time;

	ent->progs.sv->nextthink = 0; //reset thinktime
	prog->globals.sv->time = thinktime;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(ent);
	prog->globals.sv->other = PRVM_EDICT_TO_PROG(prog->edicts);
	PRVM_ExecuteProgram (ent->progs.sv->think, "pev->think");

	return !ent->priv.sv->free;
}

/*
=============
SV_MoveStep

Called by monster program code.
The move will be adjusted for slopes and stairs, but if the move isn't
possible, no move is done, false is returned, and
prog->globals.sv->trace_normal is set to the normal of the blocking wall
=============
*/
bool SV_MoveStep (edict_t *ent, vec3_t move, bool relink)
{
	float		dz;
	vec3_t		oldorg, neworg, end, traceendpos;
	trace_t		trace;
	int		i;
	edict_t		*enemy;

	// try the move
	VectorCopy (ent->progs.sv->origin, oldorg);
	VectorAdd (ent->progs.sv->origin, move, neworg);

	// flying monsters don't step up
	if ((int)ent->progs.sv->aiflags & (AI_SWIM | AI_FLY) )
	{
		// try one move with vertical motion, then one without
		for (i = 0; i < 2; i++)
		{
			VectorAdd (ent->progs.sv->origin, move, neworg);
			enemy = PRVM_PROG_TO_EDICT(ent->progs.sv->enemy);
			if (i == 0 && enemy != prog->edicts)
			{
				dz = ent->progs.sv->origin[2] - PRVM_PROG_TO_EDICT(ent->progs.sv->enemy)->progs.sv->origin[2];
				if (dz > 40) neworg[2] -= SV_COORD_FRAC;
				if (dz < 30) neworg[2] += SV_COORD_FRAC;
			}
			trace = SV_Trace(ent->progs.sv->origin, ent->progs.sv->mins, ent->progs.sv->maxs, neworg, ent, MASK_SOLID);

			if (trace.fraction == 1)
			{
				VectorCopy(trace.endpos, traceendpos);
				if (((int)ent->progs.sv->aiflags & AI_SWIM) && !(SV_PointContents(traceendpos, ent ) & MASK_WATER))
					return false; // swim monster left water

				VectorCopy (traceendpos, ent->progs.sv->origin);
				if (relink) SV_LinkEdict (ent);
				return true;
			}
			if (enemy == prog->edicts) break;
		}
		return false;
	}

	// push down from a step height above the wished position
	neworg[2] += STEPSIZE;
	VectorCopy (neworg, end);
	end[2] -= STEPSIZE * 2;

	trace = SV_Trace(neworg, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, MASK_SOLID );

	if (trace.startsolid)
	{
		neworg[2] -= STEPSIZE;
		trace = SV_Trace(neworg, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, MASK_SOLID);
		if (trace.startsolid) return false;
	}
	if (trace.fraction == 1)
	{
		// if monster had the ground pulled out, go ahead and fall
		if ((int)ent->progs.sv->aiflags & AI_PARTIALONGROUND )
		{
			VectorAdd (ent->progs.sv->origin, move, ent->progs.sv->origin);
			if (relink) SV_LinkEdict (ent);
			ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags & ~AI_ONGROUND;
			return true;
		}
		return false; // walked off an edge
	}

	// check point traces down for dangling corners
	VectorCopy (trace.endpos, ent->progs.sv->origin);

	if (!SV_CheckBottom (ent))
	{
		if ((int)ent->progs.sv->aiflags & AI_PARTIALONGROUND )
		{	
			// entity had floor mostly pulled out from underneath it
			// and is trying to correct
			if (relink) SV_LinkEdict(ent);
			return true;
		}
		VectorCopy (oldorg, ent->progs.sv->origin);
		return false;
	}

	if ( (int)ent->progs.sv->aiflags & AI_PARTIALONGROUND )
		ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags & ~AI_PARTIALONGROUND;

	ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);

	// the move is ok
	if (relink) SV_LinkEdict(ent);
	return true;
}


/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
trace_t SV_MovePushEntity (edict_t *ent, vec3_t push, bool failonbmodelstartsolid)
{
	trace_t	trace;
	vec3_t	end;
	int	mask;

	VectorAdd (ent->progs.sv->origin, push, end);

	if (ent->progs.sv->solid == SOLID_TRIGGER || ent->progs.sv->solid == SOLID_NOT)
		mask = MASK_MONSTERSOLID; // only clip against bmodels
	else mask = MASK_SOLID;

	trace = SV_Trace(ent->progs.sv->origin, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, mask );
	
	if (trace.startstuck && failonbmodelstartsolid) return trace;
	VectorCopy (trace.endpos, ent->progs.sv->origin);
	SV_LinkEdict (ent);

	if (ent->progs.sv->solid >= SOLID_TRIGGER && trace.ent && (!((int)ent->progs.sv->aiflags & AI_ONGROUND) || ent->progs.sv->groundentity != PRVM_EDICT_TO_PROG(trace.ent)))
		SV_Impact (ent, &trace);
	return trace;
}

/*
============
SV_MovePush

============
*/
void SV_MovePush(edict_t *pusher, float movetime)
{
	int		i, e, index;
	float		savesolid, movetime2, pushltime;
	vec3_t		mins, maxs, move, move1, moveangle, pushorig, pushang, a, forward, left, up, org;
	int		num_moved;
	cmodel_t		*pushermodel;
	trace_t		trace;

	if (VectorIsNull(pusher->progs.sv->velocity) && VectorIsNull(pusher->progs.sv->avelocity))   
	{
		pusher->progs.sv->ltime += movetime;
		return;
	}

	switch ((int)pusher->progs.sv->solid)
	{
	case SOLID_BSP:
	case SOLID_BBOX:
		break;
	case SOLID_NOT:
	case SOLID_TRIGGER:
		// basic moving for triggers
		VectorMA (pusher->progs.sv->origin, movetime, pusher->progs.sv->velocity, pusher->progs.sv->origin);
		VectorMA (pusher->progs.sv->angles, movetime, pusher->progs.sv->avelocity, pusher->progs.sv->angles);
		SV_ClampAngle( pusher->progs.sv->angles ); 
		pusher->progs.sv->ltime += movetime;
		SV_LinkEdict(pusher);
		return;
	default:
		MsgDev( D_ERROR, "SV_MovePush:"); 
		PRVM_ED_Print(pusher);
		MsgDev( D_ERROR, ", have invalid solid type %g\n", pusher->progs.sv->solid );
		return;
	}

	index = (int)pusher->progs.sv->modelindex;
	if (index < 1 || index >= MAX_MODELS)
	{
		MsgDev( D_ERROR, "SV_MovePush:"); 
		PRVM_ED_Print(pusher);
		MsgDev( D_ERROR, ", has an invalid modelindex %g\n", pusher->progs.sv->modelindex );
		return;
	}
	pushermodel = sv.models[index];

	movetime2 = movetime;
	VectorScale(pusher->progs.sv->velocity, movetime2, move1);
	VectorScale(pusher->progs.sv->avelocity, movetime2, moveangle);

	SV_ClampCoord( move1 );

	// find the bounding box
	for (i = 0; i < 3; i++)
	{
		mins[i] = pusher->progs.sv->absmin[i] + move1[i];
		maxs[i] = pusher->progs.sv->absmax[i] + move1[i];
	}
          
          //SV_CalcBBox(pusher, mins, maxs );
	
	VectorNegate (moveangle, a);
	AngleVectorsFLU(a, forward, left, up);	//stupid quake bug

	VectorCopy (pusher->progs.sv->origin, pushorig);
	VectorCopy (pusher->progs.sv->angles, pushang);
	pushltime = pusher->progs.sv->ltime;

	// move the pusher to its final position
	VectorMA (pusher->progs.sv->origin, movetime, pusher->progs.sv->velocity, pusher->progs.sv->origin);
	VectorMA (pusher->progs.sv->angles, movetime, pusher->progs.sv->avelocity, pusher->progs.sv->angles);

	pusher->progs.sv->ltime += movetime;
	SV_LinkEdict (pusher);

	savesolid = pusher->progs.sv->solid;

	// see if any solid entities are inside the final position
	num_moved = 0;

	for (e = 0; e < prog->num_edicts; e++)
	{
		edict_t *check = PRVM_PROG_TO_EDICT(e);
		if (check->progs.sv->movetype == MOVETYPE_NONE
		|| check->progs.sv->movetype == MOVETYPE_PUSH
		|| check->progs.sv->movetype == MOVETYPE_FOLLOW
		|| check->progs.sv->movetype == MOVETYPE_NOCLIP
		|| check->progs.sv->movetype == MOVETYPE_PHYSIC)
			continue;

		// if the entity is standing on the pusher, it will definitely be moved
		if (((int)check->progs.sv->aiflags & AI_ONGROUND) && PRVM_PROG_TO_EDICT(check->progs.sv->groundentity) == pusher)
		{
			// remove the onground flag for non-players
			if (check->progs.sv->movetype != MOVETYPE_WALK)
				check->progs.sv->flags = (int)check->progs.sv->aiflags & ~AI_ONGROUND;
		}
		else
		{
			// if the entity is not inside the pusher's final position, leave it alone
			if (!SV_ClipMoveToEntity(pusher, check->progs.sv->origin, check->progs.sv->mins, check->progs.sv->maxs, check->progs.sv->origin, MASK_SOLID).startsolid)
				continue;
		}

		if (forward[0] != 1 || left[1] != 1) // quick way to check if any rotation is used
		{
			vec3_t org2;
			VectorSubtract (check->progs.sv->origin, pusher->progs.sv->origin, org);
			org2[0] = DotProduct (org, forward);
			org2[1] = DotProduct (org, left);
			org2[2] = DotProduct (org, up);
			VectorSubtract (org2, org, move);
			VectorAdd (move, move1, move);
		}
		else VectorCopy (move1, move);

		VectorCopy (check->progs.sv->origin, check->progs.sv->post_origin);
		VectorCopy (check->progs.sv->angles, check->progs.sv->post_angles);

		// try moving the contacted entity
		pusher->progs.sv->solid = SOLID_NOT;
		trace = SV_MovePushEntity (check, move, true);

		// FIXME: turn players specially
		check->progs.sv->angles[1] += trace.fraction * moveangle[1];
		pusher->progs.sv->solid = savesolid; // was SOLID_BSP

		// if it is still inside the pusher, block
		if (SV_ClipMoveToEntity(pusher, check->progs.sv->origin, check->progs.sv->mins, check->progs.sv->maxs, check->progs.sv->origin, MASK_SOLID).startsolid)
		{
			// try moving the contacted entity a tiny bit further to account for precision errors
			vec3_t move2;
			pusher->progs.sv->solid = SOLID_NOT;
			VectorScale(move, 1.1, move2);
			VectorCopy (check->progs.sv->post_origin, check->progs.sv->origin);
			VectorCopy (check->progs.sv->post_angles, check->progs.sv->angles);
			SV_MovePushEntity (check, move2, true);
			pusher->progs.sv->solid = savesolid;

			if (SV_ClipMoveToEntity(pusher, check->progs.sv->origin, check->progs.sv->mins, check->progs.sv->maxs, check->progs.sv->origin, MASK_SOLID).startsolid)
			{
				// try moving the contacted entity a tiny bit less to account for precision errors
				pusher->progs.sv->solid = SOLID_NOT;
				VectorScale(move, 0.9, move2);
				VectorCopy (check->progs.sv->post_origin, check->progs.sv->origin);
				VectorCopy (check->progs.sv->post_angles, check->progs.sv->angles);
				SV_MovePushEntity (check, move2, true);
				pusher->progs.sv->solid = savesolid;

				if (SV_ClipMoveToEntity(pusher, check->progs.sv->origin, check->progs.sv->mins, check->progs.sv->maxs, check->progs.sv->origin, MASK_SOLID).startsolid)
				{
					// still inside pusher, so it's really blocked
					// fail the move
					if (check->progs.sv->mins[0] == check->progs.sv->maxs[0])
						continue;
					if (check->progs.sv->solid == SOLID_NOT || check->progs.sv->solid == SOLID_TRIGGER)
					{
						// corpse
						check->progs.sv->mins[0] = check->progs.sv->mins[1] = 0;
						VectorCopy (check->progs.sv->mins, check->progs.sv->maxs);
						continue;
					}

					VectorCopy (pushorig, pusher->progs.sv->origin);
					VectorCopy (pushang, pusher->progs.sv->angles);
					pusher->progs.sv->ltime = pushltime;
					SV_LinkEdict (pusher);

					// if the pusher has a "blocked" function, call it, otherwise just stay in place until the obstacle is gone
					if (pusher->progs.sv->blocked)
					{
						prog->globals.sv->pev = PRVM_EDICT_TO_PROG(pusher);
						prog->globals.sv->other = PRVM_EDICT_TO_PROG(check);
						PRVM_ExecuteProgram (pusher->progs.sv->blocked, "pev->blocked");
					}
					break;
				}
			}
		}
	}
	SV_ClampAngle( pusher->progs.sv->angles );
}

/*
=============
SV_PhysicsFollow

Entities that are "stuck" to another entity
=============
*/
void SV_PhysicsFollow (edict_t *ent)
{
	vec3_t	vf, vr, vu, angles, v;
	edict_t	*e;

	// regular thinking
	if (!SV_RunThink (ent)) return;

	// LordHavoc: implemented rotation on MOVETYPE_FOLLOW objects
	e = PRVM_PROG_TO_EDICT(ent->progs.sv->aiment);
	if (VectorCompare(e->progs.sv->angles, ent->progs.sv->punchangle))
	{
		// quick case for no rotation
		VectorAdd(e->progs.sv->origin, ent->progs.sv->view_ofs, ent->progs.sv->origin);
	}
	else
	{
		angles[0] = -ent->progs.sv->punchangle[0];
		angles[1] =  ent->progs.sv->punchangle[1];
		angles[2] =  ent->progs.sv->punchangle[2];
		AngleVectors (angles, vf, vr, vu);
		v[0] = ent->progs.sv->view_ofs[0] * vf[0] + ent->progs.sv->view_ofs[1] * vr[0] + ent->progs.sv->view_ofs[2] * vu[0];
		v[1] = ent->progs.sv->view_ofs[0] * vf[1] + ent->progs.sv->view_ofs[1] * vr[1] + ent->progs.sv->view_ofs[2] * vu[1];
		v[2] = ent->progs.sv->view_ofs[0] * vf[2] + ent->progs.sv->view_ofs[1] * vr[2] + ent->progs.sv->view_ofs[2] * vu[2];
		angles[0] = -e->progs.sv->angles[0];
		angles[1] =  e->progs.sv->angles[1];
		angles[2] =  e->progs.sv->angles[2];
		AngleVectors (angles, vf, vr, vu);
		ent->progs.sv->origin[0] = v[0] * vf[0] + v[1] * vf[1] + v[2] * vf[2] + e->progs.sv->origin[0];
		ent->progs.sv->origin[1] = v[0] * vr[0] + v[1] * vr[1] + v[2] * vr[2] + e->progs.sv->origin[1];
		ent->progs.sv->origin[2] = v[0] * vu[0] + v[1] * vu[1] + v[2] * vu[2] + e->progs.sv->origin[2];
	}
	VectorAdd (e->progs.sv->angles, ent->progs.sv->v_angle, ent->progs.sv->angles);
	SV_LinkEdict (ent);
}

/*
================
SV_PhysicsPush

================
*/
void SV_PhysicsPush(edict_t *ent)
{
	float thinktime, oldltime, movetime;

	oldltime = ent->progs.sv->ltime;

	thinktime = ent->progs.sv->nextthink;
	if (thinktime < ent->progs.sv->ltime + sv.frametime)
	{
		movetime = thinktime - ent->progs.sv->ltime;
		if (movetime < 0) movetime = 0;
	}
	else movetime = sv.frametime;
		
	// advances ent->progs.sv->ltime if not blocked
	if (movetime) SV_MovePush(ent, movetime);

	if (thinktime > oldltime && thinktime <= ent->progs.sv->ltime)
	{
		ent->progs.sv->nextthink = 0;
		prog->globals.sv->time = sv.time;
		prog->globals.sv->pev = PRVM_EDICT_TO_PROG(ent);
		prog->globals.sv->other = PRVM_EDICT_TO_PROG(prog->edicts);
		PRVM_ExecuteProgram (ent->progs.sv->think, "pev->think");
	}
}

/*
==============================================================================

TOSS / BOUNCE

==============================================================================
*/

/*
=============
SV_CheckWaterTransition

=============
*/
void SV_CheckWaterTransition (edict_t *ent)
{
	int	cont = SV_PointContents(ent->progs.sv->origin, ent );

	if (!ent->progs.sv->watertype)
	{
		// just spawned here
		ent->progs.sv->watertype = cont;
		ent->progs.sv->waterlevel = 1;
		return;
	}

	// check if the entity crossed into or out of water
	if ((int)ent->progs.sv->watertype & MASK_WATER) 
	{
		Msg("water splash!\n");
		//SV_StartSound (ent, 0, "", 255, 1);
	}

	if (cont & MASK_WATER)
	{
		ent->progs.sv->watertype = cont;
		ent->progs.sv->waterlevel = 1;
	}
	else
	{
		ent->progs.sv->watertype = CONTENTS_NONE;
		ent->progs.sv->waterlevel = 0;
	}
}

/*
=============
SV_PhysicsToss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_PhysicsToss (edict_t *ent)
{
	trace_t trace;
	vec3_t move;

	SV_CheckVelocity (ent);

	// add gravity
	if (ent->progs.sv->movetype == MOVETYPE_TOSS || ent->progs.sv->movetype == MOVETYPE_BOUNCE)
		SV_AddGravity (ent);

	// move angles
	VectorMA (ent->progs.sv->angles, sv.frametime, ent->progs.sv->avelocity, ent->progs.sv->angles);

	// move origin
	VectorScale (ent->progs.sv->velocity, sv.frametime, move);
	trace = SV_MovePushEntity (ent, move, true);
	if (ent->priv.sv->free) return;

	if (trace.startstuck)
	{
		// try to unstick the entity
		SV_UnstickEntity(ent);
		trace = SV_MovePushEntity (ent, move, false);
		if (ent->priv.sv->free) return;
	}

	if (trace.fraction < 1)
	{
		if (ent->progs.sv->movetype == MOVETYPE_BOUNCE)
		{
			float d;
			SV_ClipVelocity (ent->progs.sv->velocity, trace.plane.normal, ent->progs.sv->velocity, 1.5);
			d = DotProduct(trace.plane.normal, ent->progs.sv->velocity);
			if (trace.plane.normal[2] > 0.7 && fabs(d) < 60)
			{
				ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags | AI_ONGROUND;
				ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
				VectorClear (ent->progs.sv->velocity);
				VectorClear (ent->progs.sv->avelocity);
			}
			else ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags & ~AI_ONGROUND;
		}
		else
		{
			SV_ClipVelocity (ent->progs.sv->velocity, trace.plane.normal, ent->progs.sv->velocity, 1.0);
			if (trace.plane.normal[2] > 0.7)
			{
				ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags | AI_ONGROUND;
				ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
				VectorClear (ent->progs.sv->velocity);
				VectorClear (ent->progs.sv->avelocity);
			}
			else ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags & ~AI_ONGROUND;
		}
	}

	// check for in water
	SV_CheckWaterTransition (ent);
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/

/*
=============
SV_PhysicsStep

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
=============
*/
void SV_PhysicsStep (edict_t *ent)
{
	int aiflags = (int)ent->progs.sv->aiflags;

	// don't fall at all if fly/swim
	if (!(aiflags & (AI_FLY | AI_SWIM)))
	{
		if (aiflags & AI_ONGROUND)
		{
			// freefall if onground and moving upward
			// freefall if not standing on a world surface (it may be a lift or trap door)
			if (ent->progs.sv->groundentity)
			{
				ent->progs.sv->aiflags -= AI_ONGROUND;
				SV_AddGravity(ent);
				SV_CheckVelocity(ent);
				SV_FlyMove(ent, sv.frametime, NULL);
				SV_LinkEdict(ent);
			}
		}
		else
		{
			// freefall if not onground
			int hitsound = ent->progs.sv->velocity[2] < sv_gravity->value * -0.1;

			SV_AddGravity(ent);
			SV_CheckVelocity(ent);
			SV_FlyMove(ent, sv.frametime, NULL);
			SV_LinkEdict(ent);

			// just hit ground
			if (hitsound && (int)ent->progs.sv->aiflags & AI_ONGROUND)
			{
				Msg("hit on ground\n");
				//SV_StartSound(ent, 0, sv_sound_land.string, 255, 1);
			}
		}
	}

	// regular thinking
	SV_RunThink(ent);
	SV_CheckWaterTransition(ent);
}

/*
=====================
SV_WalkMove

Only used by players
======================
*/
void SV_WalkMove (edict_t *ent)
{
	int	clip, oldonground, originalmove_clip, originalmove_flags, originalmove_groundentity;
	vec3_t	upmove, downmove, start_origin, start_velocity, stepnormal, originalmove_origin, originalmove_velocity;
	trace_t	downtrace;

	SV_CheckVelocity(ent);

	// do a regular slide move unless it looks like you ran into a step
	oldonground = (int)ent->progs.sv->aiflags & AI_ONGROUND;
	ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags & ~AI_ONGROUND;

	VectorCopy (ent->progs.sv->origin, start_origin);
	VectorCopy (ent->progs.sv->velocity, start_velocity);

	clip = SV_FlyMove (ent, sv.frametime, NULL);
	SV_CheckVelocity(ent);

	VectorCopy(ent->progs.sv->origin, originalmove_origin);
	VectorCopy(ent->progs.sv->velocity, originalmove_velocity);
	originalmove_clip = clip;
	originalmove_flags = (int)ent->progs.sv->aiflags;
	originalmove_groundentity = ent->progs.sv->groundentity;

	if ((int)ent->progs.sv->aiflags & AI_WATERJUMP) return;

	// if move didn't block on a step, return
	if (clip & 2)
	{
		// if move was not trying to move into the step, return
		if (fabs(start_velocity[0]) < CL_COORD_FRAC && fabs(start_velocity[1]) < CL_COORD_FRAC)
			return;

		if (ent->progs.sv->movetype != MOVETYPE_FLY)
		{
			// return if gibbed by a trigger
			if (ent->progs.sv->movetype != MOVETYPE_WALK)
				return;

			// only step up while jumping if that is enabled
			if (!oldonground && ent->progs.sv->waterlevel == 0)
				return;
		}

		// try moving up and forward to go up a step
		// back to start pos
		VectorCopy (start_origin, ent->progs.sv->origin);
		VectorCopy (start_velocity, ent->progs.sv->velocity);

		// move up
		VectorClear (upmove);
		upmove[2] = STEPSIZE;
		// FIXME: don't link?
		SV_MovePushEntity(ent, upmove, false);

		// move forward
		ent->progs.sv->velocity[2] = 0;
		clip = SV_FlyMove (ent, sv.frametime, stepnormal);
		ent->progs.sv->velocity[2] += start_velocity[2];

		SV_CheckVelocity(ent);

		// check for stuckness, possibly due to the limited precision of floats
		// in the clipping hulls
		if (clip && fabs(originalmove_origin[1] - ent->progs.sv->origin[1]) < CL_COORD_FRAC && fabs(originalmove_origin[0] - ent->progs.sv->origin[0]) < CL_COORD_FRAC)
		{
			//Msg("wall\n");
			// stepping up didn't make any progress, revert to original move
			VectorCopy(originalmove_origin, ent->progs.sv->origin);
			VectorCopy(originalmove_velocity, ent->progs.sv->velocity);
			//clip = originalmove_clip;
			ent->progs.sv->aiflags = originalmove_flags;
			ent->progs.sv->groundentity = originalmove_groundentity;
			// now try to unstick if needed
			//clip = SV_TryUnstick (ent, oldvel);
			return;
		}

		//Msg("step - ");

		// extra friction based on view angle
		if (clip & 2) SV_WallFriction (ent, stepnormal);
	}
	// skip out if stepdown is enabled, moving downward, not in water, and the move started onground and ended offground
	else if (ent->progs.sv->waterlevel < 2 && start_velocity[2] < CL_COORD_FRAC && oldonground && !((int)ent->progs.sv->aiflags & AI_ONGROUND))
		return;

	// move down
	VectorClear (downmove);
	downmove[2] = -STEPSIZE + start_velocity[2]*sv.frametime;
	// FIXME: don't link?
	downtrace = SV_MovePushEntity (ent, downmove, false);

	if (downtrace.fraction < 1 && downtrace.plane.normal[2] > 0.7)
	{
		// this has been disabled so that you can't jump when you are stepping
		// up while already jumping (also known as the Quake2 stair jump bug)
	}
	else
	{
		//Msgf("slope\n");
		// if the push down didn't end up on good ground, use the move without
		// the step up.  This happens near wall / slope combinations, and can
		// cause the player to hop up higher on a slope too steep to climb
		VectorCopy(originalmove_origin, ent->progs.sv->origin);
		VectorCopy(originalmove_velocity, ent->progs.sv->velocity);
		//clip = originalmove_clip;
		ent->progs.sv->aiflags = originalmove_flags;
		ent->progs.sv->groundentity = originalmove_groundentity;
	}

	SV_CheckVelocity(ent);
}

/*
====================
SV_Physics_Conveyor

REAL simple - all we do is check for player riders and adjust their position.
Only gotcha here is we have to make sure we don't end up embedding player in
*another* object that's being moved by the conveyor.

====================
*/
void SV_Physics_Conveyor(edict_t *ent)
{
	edict_t		*player;
	int		i;
	trace_t		tr;
	vec3_t		v, move;
	vec3_t		point, end;

	VectorScale(ent->progs.sv->movedir,ent->progs.sv->speed,v);
	VectorScale(v, 0.1f, move);

	for(i = 0; i < maxclients->value; i++)
	{
		player = PRVM_EDICT_NUM(i) + 1;
		if(player->priv.sv->free) continue;
		if(!player->progs.sv->groundentity) continue;
		if(player->progs.sv->groundentity != PRVM_EDICT_TO_PROG(ent)) continue;

		// Look below player; make sure he's on a conveyor
		VectorCopy(player->progs.sv->origin,point);
		point[2] += 1;
		VectorCopy(point,end);
		end[2] -= 256;

		tr = SV_Trace (point, player->progs.sv->mins, player->progs.sv->maxs, end, player, MASK_SOLID);
		// tr.ent HAS to be conveyor, but just in case we screwed something up:
		if(tr.ent == ent)
		{
			if(tr.plane.normal[2] > 0)
			{
				v[2] = ent->progs.sv->speed * sqrt(1.0 - tr.plane.normal[2]*tr.plane.normal[2]) / tr.plane.normal[2];
				if(DotProduct(ent->progs.sv->movedir, tr.plane.normal) > 0)
				{
					// then we're moving down
					v[2] = -v[2];
				}
				move[2] = v[2] * 0.1f;
			}
			VectorAdd(player->progs.sv->origin,move,end);
			tr = SV_Trace(player->progs.sv->origin,player->progs.sv->mins,player->progs.sv->maxs,end,player,player->priv.sv->clipmask);
			VectorCopy(tr.endpos,player->progs.sv->origin);
			SV_LinkEdict(player);
		}
	}
}

/*
=============
SV_PhysicsNoclip

A moving object that doesn't obey physics
=============
*/
void SV_PhysicsNoclip(edict_t *ent)
{
	// regular thinking
	if (!SV_RunThink (ent)) return;
	
	VectorMA (ent->progs.sv->angles, sv.frametime, ent->progs.sv->avelocity, ent->progs.sv->angles);
	VectorMA (ent->progs.sv->origin, sv.frametime, ent->progs.sv->velocity, ent->progs.sv->origin);

	SV_LinkEdict(ent);
}


/*
=============
SV_PhysicsNone

Non moving objects can only think
=============
*/
void SV_PhysicsNone (edict_t *ent)
{
	if (ent->progs.sv->nextthink > 0 && ent->progs.sv->nextthink <= sv.time + sv.frametime)
		SV_RunThink (ent);
}

void SV_Physics( edict_t *ent )
{
	switch ((int)ent->progs.sv->movetype)
	{
	case MOVETYPE_NONE:
	case MOVETYPE_PHYSIC:
		SV_PhysicsNone(ent);
		break;
	case MOVETYPE_PUSH:
		SV_PhysicsPush(ent);
		break;
	case MOVETYPE_NOCLIP:
		SV_PhysicsNoclip(ent);
		break;
	case MOVETYPE_FOLLOW:
		SV_PhysicsFollow(ent);
		break;
	case MOVETYPE_STEP:
		SV_PhysicsStep (ent);
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_FLY:
		SV_PhysicsToss(ent);
		break;
	case MOVETYPE_WALK:
		if(SV_RunThink( ent ))
		{
			if (!SV_CheckWater(ent) && ! ((int)ent->progs.sv->aiflags & AI_WATERJUMP) )
				SV_AddGravity (ent);
			SV_CheckStuck( ent );
			SV_WalkMove( ent );
			SV_LinkEdict( ent );
		}
		break;
	case MOVETYPE_CONVEYOR:
		SV_Physics_Conveyor(ent);
		break;
	default:
		PRVM_ERROR ("SV_Physics: bad movetype %i", (int)ent->progs.sv->movetype);			
	}
}