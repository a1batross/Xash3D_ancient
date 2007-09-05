//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_phys.c - internal physic engine
//=======================================================================

#include "engine.h"
#include "server.h"

// phys cvars
extern cvar_t	*sv_maxvelocity;
extern cvar_t	*sv_gravity;

void SV_CheckVelocity (prvm_edict_t *ent)
{
	int	i;
	float	wishspeed;

	// bound velocity
	for (i = 0; i < 3; i++)
	{
		if (IS_NAN(ent->fields.sv->velocity[i]))
		{
			Msg("Got a NaN velocity on %s\n", PRVM_GetString(ent->fields.sv->classname));
			ent->fields.sv->velocity[i] = 0;
		}
		if (IS_NAN(ent->fields.sv->origin[i]))
		{
			Msg("Got a NaN origin on %s\n", PRVM_GetString(ent->fields.sv->classname));
			ent->fields.sv->origin[i] = 0;
		}
	}

	// LordHavoc: max velocity fix, inspired by Maddes's source fixes, but this is faster
	wishspeed = DotProduct(ent->fields.sv->velocity, ent->fields.sv->velocity);
	if (wishspeed > sv_maxvelocity->value * sv_maxvelocity->value)
	{
		wishspeed = sv_maxvelocity->value / sqrt(wishspeed);
		ent->fields.sv->velocity[0] *= wishspeed;
		ent->fields.sv->velocity[1] *= wishspeed;
		ent->fields.sv->velocity[2] *= wishspeed;
	}
}

trace_t SV_TraceToss (prvm_edict_t *tossent, prvm_edict_t *ignore)
{
	int		i;
	float		gravity = 1.0;
	vec3_t		move, end;
	vec3_t		original_origin;
	vec3_t		original_velocity;
	vec3_t		original_angles;
	vec3_t		original_avelocity;
	trace_t		trace;

	VectorCopy(tossent->fields.sv->origin, original_origin );
	VectorCopy(tossent->fields.sv->velocity, original_velocity );
	VectorCopy(tossent->fields.sv->angles, original_angles );
	VectorCopy(tossent->fields.sv->avelocity, original_avelocity);

	gravity *= sv_gravity->value * 0.05;

	for (i = 0; i < 200; i++) // LordHavoc: sanity check; never trace more than 10 seconds
	{
		SV_CheckVelocity (tossent);
		tossent->fields.sv->velocity[2] -= gravity;
		VectorMA (tossent->fields.sv->angles, 0.05, tossent->fields.sv->avelocity, tossent->fields.sv->angles);
		VectorScale (tossent->fields.sv->velocity, 0.05, move);
		VectorAdd (tossent->fields.sv->origin, move, end);
		trace = SV_Trace(tossent->fields.sv->origin, tossent->fields.sv->mins, tossent->fields.sv->maxs, end, tossent, MASK_SOLID );
		VectorCopy (trace.endpos, tossent->fields.sv->origin);

		if (trace.fraction < 1) break;
	}

	VectorCopy(original_origin, tossent->fields.sv->origin );
	VectorCopy(original_velocity, tossent->fields.sv->velocity );
	VectorCopy(original_angles, tossent->fields.sv->angles );
	VectorCopy(original_avelocity, tossent->fields.sv->avelocity);

	return trace;
}

/*
============
SV_TestEntityPosition

returns true if the entity is in solid currently
============
*/
int SV_TestEntityPosition (prvm_edict_t *ent)
{
	trace_t trace = SV_Trace(ent->fields.sv->origin, ent->fields.sv->mins, ent->fields.sv->maxs, ent->fields.sv->origin, ent, MASK_SOLID);

	if (trace.contents & MASK_SOLID)
		return true;
	return false;
}

/*
=============
SV_EntityThink

Runs thinking code if time.  There is some play in the exact time the think
function will be called, because it is called before any movement is done
in a frame.  Not used for pushmove objects, because they must be exact.
Returns false if the entity removed itself.
=============
*/
bool SV_EntityThink (prvm_edict_t *ent)
{
	float thinktime;

	thinktime = ent->fields.sv->nextthink;
	if (thinktime <= 0 || thinktime > sv.time + sv.frametime)
		return true;

	// don't let things stay in the past.
	// it is possible to start that way by a trigger with a local time.
	if (thinktime < sv.time) thinktime = sv.time;

	// reset nextthink, it will be restored in qc-code
	ent->fields.sv->nextthink = 0;
	prog->globals.server->time = thinktime;
	prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
	prog->globals.server->other = PRVM_EDICT_TO_PROG(prog->edicts);
	PRVM_ExecuteProgram (ent->fields.sv->think, "QC function self.think is missing");
	return !ent->priv.sv->free;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact (prvm_edict_t *e1, trace_t *trace)
{
	int		old_self, old_other;
	prvm_edict_t	*e2 = (prvm_edict_t *)trace->ent;

	old_self = prog->globals.server->self;
	old_other = prog->globals.server->other;

	prog->globals.server->time = sv.time;
	if (!e1->priv.sv->free && !e2->priv.sv->free && e1->fields.sv->touch && e1->fields.sv->solid != SOLID_NOT)
	{
		prog->globals.server->self = PRVM_EDICT_TO_PROG(e1);
		prog->globals.server->other = PRVM_EDICT_TO_PROG(e2);
		prog->globals.server->trace_allsolid = trace->allsolid;
		prog->globals.server->trace_startsolid = trace->startsolid;
		prog->globals.server->trace_fraction = trace->fraction;
		prog->globals.server->trace_inwater = (trace->contents & MASK_WATER) ? true : false;
		prog->globals.server->trace_inopen = (trace->contents & MASK_SHOT) ? true : false;
		VectorCopy (trace->endpos, prog->globals.server->trace_endpos);
		VectorCopy (trace->plane.normal, prog->globals.server->trace_plane_normal);
		prog->globals.server->trace_plane_dist =  trace->plane.dist;
		if (trace->ent) prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(trace->ent);
		else prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(prog->edicts);
		PRVM_ExecuteProgram (e1->fields.sv->touch, "QC function self.touch is missing");
	}
	if (!e1->priv.sv->free && !e2->priv.sv->free && e2->fields.sv->touch && e2->fields.sv->solid != SOLID_NOT)
	{
		prog->globals.server->self = PRVM_EDICT_TO_PROG(e2);
		prog->globals.server->other = PRVM_EDICT_TO_PROG(e1);
		prog->globals.server->trace_allsolid = false;
		prog->globals.server->trace_startsolid = false;
		prog->globals.server->trace_fraction = 1;
		prog->globals.server->trace_inwater = false;
		prog->globals.server->trace_inopen = true;
		VectorCopy (e2->fields.sv->origin, prog->globals.server->trace_endpos);
		VectorSet (prog->globals.server->trace_plane_normal, 0, 0, 1);
		prog->globals.server->trace_plane_dist = 0;
		prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(e1);
		PRVM_ExecuteProgram (e2->fields.sv->touch, "QC function self.touch is missing");
	}
	prog->globals.server->self = old_self;
	prog->globals.server->other = old_other;
}


/*
==================
ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
void ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	int i;
	float backoff;

	backoff = -DotProduct (in, normal) * overbounce;
	VectorMA(in, backoff, normal, out);

	for (i = 0;i < 3;i++)
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
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

int SV_FlyMove (prvm_edict_t *ent, float time, float *stepnormal)
{
	int	blocked = 0, bumpcount;
	int	i, j, impact, numplanes;
	float	d, time_left;
	vec3_t	dir, end, planes[MAX_CLIP_PLANES], primal_velocity, original_velocity, new_velocity;
	trace_t	trace;

	VectorCopy(ent->fields.sv->velocity, original_velocity);
	VectorCopy(ent->fields.sv->velocity, primal_velocity);
	numplanes = 0;
	time_left = time;

	for (bumpcount = 0; bumpcount < MAX_CLIP_PLANES; bumpcount++)
	{
		if (!ent->fields.sv->velocity[0] && !ent->fields.sv->velocity[1] && !ent->fields.sv->velocity[2])
			break;

		VectorMA(ent->fields.sv->origin, time_left, ent->fields.sv->velocity, end);
		trace = SV_Trace(ent->fields.sv->origin, ent->fields.sv->mins, ent->fields.sv->maxs, end, ent, MASK_SOLID);

		// break if it moved the entire distance
		if (trace.fraction == 1)
		{
			VectorCopy(trace.endpos, ent->fields.sv->origin);
			break;
		}

		if (!trace.ent)
		{
			MsgWarn("SV_FlyMove: !trace.ent");
			trace.ent = prog->edicts;
		}

		if (((int) ent->fields.sv->flags & FL_ONGROUND) && ent->fields.sv->groundentity == PRVM_EDICT_TO_PROG(trace.ent))
		{
			impact = false;
		}
		else
		{
			ent->fields.sv->flags = (int)ent->fields.sv->flags & ~FL_ONGROUND;
			impact = true;
		}

		if (trace.plane.normal[2])
		{
			if (trace.plane.normal[2] > 0.7)
			{
				blocked |= 1; // floor
				ent->fields.sv->flags = (int)ent->fields.sv->flags | FL_ONGROUND;
				ent->fields.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
			}
		}
		else
		{
			blocked |= 2; // step
			// save the trace for player extrafriction
			if (stepnormal) VectorCopy(trace.plane.normal, stepnormal);
		}

		if (trace.fraction >= 0.001)
		{
			// actually covered some distance
			VectorCopy(trace.endpos, ent->fields.sv->origin);
			VectorCopy(ent->fields.sv->velocity, original_velocity);
			numplanes = 0;
		}

		// run the impact function
		if (impact)
		{
			SV_Impact(ent, &trace);

			// break if removed by the impact function
			if (ent->priv.sv->free) break;
		}

		time_left *= 1 - trace.fraction;

		// clipped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{
			// this shouldn't really happen
			VectorClear(ent->fields.sv->velocity);
			blocked = 3;
			break;
		}

		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for (i = 0;i < numplanes;i++)
		{
			ClipVelocity(original_velocity, planes[i], new_velocity, 1);
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
			VectorCopy(new_velocity, ent->fields.sv->velocity);
		}
		else
		{
			// go along the crease
			if (numplanes != 2)
			{
				VectorClear(ent->fields.sv->velocity);
				blocked = 7;
				break;
			}

			CrossProduct(planes[0], planes[1], dir);
			VectorNormalize(dir);
			d = DotProduct(dir, ent->fields.sv->velocity);
			VectorScale(dir, d, ent->fields.sv->velocity);
		}

		// if current velocity is against the original velocity,
		// stop dead to avoid tiny occilations in sloping corners
		if (DotProduct(ent->fields.sv->velocity, primal_velocity) <= 0)
		{
			VectorClear(ent->fields.sv->velocity);
			break;
		}
	}

	if ((int)ent->fields.sv->flags & FL_WATERJUMP) //from QuakeWorld
		VectorCopy(primal_velocity, ent->fields.sv->velocity);
	return blocked;
}

/*
============
SV_AddGravity

============
*/
void SV_AddGravity (prvm_edict_t *ent)
{
	float ent_gravity = 1.0;

	ent->fields.sv->velocity[2] -= ent_gravity * sv_gravity->value * sv.frametime;
}

/*
===============================================================================

PUSHMOVE

===============================================================================
*/
/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
trace_t SV_PushEntity (prvm_edict_t *ent, vec3_t push, bool failonbmodelstartsolid)
{
	trace_t trace;
	vec3_t end;

	VectorAdd (ent->fields.sv->origin, push, end);

	trace = SV_Trace(ent->fields.sv->origin, ent->fields.sv->mins, ent->fields.sv->maxs, end, ent, MASK_SOLID );
	
	if (trace.startstuck && failonbmodelstartsolid) return trace;
	VectorCopy (trace.endpos, ent->fields.sv->origin);
	SV_LinkEdict (ent);

	if (ent->fields.sv->solid >= SOLID_TRIGGER && trace.ent && (!((int)ent->fields.sv->flags & FL_ONGROUND) || ent->fields.sv->groundentity != PRVM_EDICT_TO_PROG(trace.ent)))
		SV_Impact (ent, &trace);
	return trace;
}

/*
============
SV_PushMove

============
*/
void SV_PushMove (prvm_edict_t *pusher, float movetime)
{
	int		i, e, index;
	float		savesolid, movetime2, pushltime;
	vec3_t		mins, maxs, move, move1, moveangle, pushorig, pushang, a, forward, left, up, org;
	int		num_moved;
	int		numcheckentities;
	static prvm_edict_t *checkentities[MAX_EDICTS];
	cmodel_t		*pushermodel;
	trace_t		trace;

	if (VectorIsNull(pusher->fields.sv->velocity) && VectorIsNull(pusher->fields.sv->avelocity))   
	{
		pusher->fields.sv->ltime += movetime;
		return;
	}

	switch ((int)pusher->fields.sv->solid)
	{
	// LordHavoc: valid pusher types
	case SOLID_BSP:
	case SOLID_BBOX:
		break;
	// LordHavoc: no collisions
	case SOLID_NOT:
	case SOLID_TRIGGER:
		VectorMA (pusher->fields.sv->origin, movetime, pusher->fields.sv->velocity, pusher->fields.sv->origin);
		VectorMA (pusher->fields.sv->angles, movetime, pusher->fields.sv->avelocity, pusher->fields.sv->angles);
		pusher->fields.sv->angles[0] -= 360.0 * floor(pusher->fields.sv->angles[0] * (1.0 / 360.0));
		pusher->fields.sv->angles[1] -= 360.0 * floor(pusher->fields.sv->angles[1] * (1.0 / 360.0));
		pusher->fields.sv->angles[2] -= 360.0 * floor(pusher->fields.sv->angles[2] * (1.0 / 360.0));
		pusher->fields.sv->ltime += movetime;
		SV_LinkEdict (pusher);
		return;
	default:
		MsgWarn("SV_PushMove: entity #%i, unrecognized solid type %f\n", PRVM_NUM_FOR_EDICT(pusher), pusher->fields.sv->solid);
		return;
	}

	index = (int) pusher->fields.sv->modelindex;
	if (index < 1 || index >= MAX_MODELS)
	{
		MsgWarn("SV_PushMove: entity #%i has an invalid modelindex %f\n", PRVM_NUM_FOR_EDICT(pusher), pusher->fields.sv->modelindex);
		return;
	}
	pushermodel = sv.models[index];

	movetime2 = movetime;
	VectorScale(pusher->fields.sv->velocity, movetime2, move1);
	VectorScale(pusher->fields.sv->avelocity, movetime2, moveangle);
	if (moveangle[0] || moveangle[2])
	{
		for (i = 0; i < 3; i++)
		{
			if (move1[i] > 0)
			{
				mins[i] = pushermodel->rotatedmins[i] + pusher->fields.sv->origin[i] - 1;
				maxs[i] = pushermodel->rotatedmaxs[i] + move1[i] + pusher->fields.sv->origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->rotatedmins[i] + move1[i] + pusher->fields.sv->origin[i] - 1;
				maxs[i] = pushermodel->rotatedmaxs[i] + pusher->fields.sv->origin[i] + 1;
			}
		}
	}
	else if (moveangle[1])
	{
		for (i = 0; i < 3; i++)
		{
			if (move1[i] > 0)
			{
				mins[i] = pushermodel->yawmins[i] + pusher->fields.sv->origin[i] - 1;
				maxs[i] = pushermodel->yawmaxs[i] + move1[i] + pusher->fields.sv->origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->yawmins[i] + move1[i] + pusher->fields.sv->origin[i] - 1;
				maxs[i] = pushermodel->yawmaxs[i] + pusher->fields.sv->origin[i] + 1;
			}
		}
	}
	else
	{
		for (i = 0; i < 3; i++)
		{
			if (move1[i] > 0)
			{
				mins[i] = pushermodel->normalmins[i] + pusher->fields.sv->origin[i] - 1;
				maxs[i] = pushermodel->normalmaxs[i] + move1[i] + pusher->fields.sv->origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->normalmins[i] + move1[i] + pusher->fields.sv->origin[i] - 1;
				maxs[i] = pushermodel->normalmaxs[i] + pusher->fields.sv->origin[i] + 1;
			}
		}
	}

	VectorNegate (moveangle, a);
	AngleVectorsLeft(a, forward, left, up);

	VectorCopy (pusher->fields.sv->origin, pushorig);
	VectorCopy (pusher->fields.sv->angles, pushang);
	pushltime = pusher->fields.sv->ltime;

	// move the pusher to its final position
	VectorMA (pusher->fields.sv->origin, movetime, pusher->fields.sv->velocity, pusher->fields.sv->origin);
	VectorMA (pusher->fields.sv->angles, movetime, pusher->fields.sv->avelocity, pusher->fields.sv->angles);
	pusher->fields.sv->ltime += movetime;
	SV_LinkEdict (pusher);

	savesolid = pusher->fields.sv->solid;

	// see if any solid entities are inside the final position
	num_moved = 0;

	numcheckentities = 0;//SV_EntitiesInBox(mins, maxs, MAX_EDICTS, checkentities);
	for (e = 0;e < numcheckentities; e++)
	{
		prvm_edict_t *check = checkentities[e];
		if (check->fields.sv->movetype == MOVETYPE_NONE || check->fields.sv->movetype == MOVETYPE_PUSH || check->fields.sv->movetype == MOVETYPE_FOLLOW || check->fields.sv->movetype == MOVETYPE_NOCLIP)
			continue;

		// if the entity is standing on the pusher, it will definitely be moved
		if (((int)check->fields.sv->flags & FL_ONGROUND) && PRVM_PROG_TO_EDICT(check->fields.sv->groundentity) == pusher)
		{
			// remove the onground flag for non-players
			if (check->fields.sv->movetype != MOVETYPE_WALK)
				check->fields.sv->flags = (int)check->fields.sv->flags & ~FL_ONGROUND;
		}
		else
		{
			// if the entity is not inside the pusher's final position, leave it alone
			if (!SV_ClipMoveToEntity(pusher, check->fields.sv->origin, check->fields.sv->mins, check->fields.sv->maxs, check->fields.sv->origin, MASK_SOLID).startsolid)
				continue;
		}

		if (forward[0] != 1 || left[1] != 1) // quick way to check if any rotation is used
		{
			vec3_t org2;
			VectorSubtract (check->fields.sv->origin, pusher->fields.sv->origin, org);
			org2[0] = DotProduct (org, forward);
			org2[1] = DotProduct (org, left);
			org2[2] = DotProduct (org, up);
			VectorSubtract (org2, org, move);
			VectorAdd (move, move1, move);
		}
		else
			VectorCopy (move1, move);

		VectorCopy (check->fields.sv->origin, check->priv.sv->moved_from);
		VectorCopy (check->fields.sv->angles, check->priv.sv->moved_fromangles);
		sv.moved_edicts[num_moved++] = check;

		// try moving the contacted entity
		pusher->fields.sv->solid = SOLID_NOT;
		trace = SV_PushEntity (check, move, true);

		// FIXME: turn players specially
		check->fields.sv->angles[1] += trace.fraction * moveangle[1];
		pusher->fields.sv->solid = savesolid; // was SOLID_BSP

		// if it is still inside the pusher, block
		if (SV_ClipMoveToEntity(pusher, check->fields.sv->origin, check->fields.sv->mins, check->fields.sv->maxs, check->fields.sv->origin, MASK_SOLID).startsolid)
		{
			// try moving the contacted entity a tiny bit further to account for precision errors
			vec3_t move2;
			pusher->fields.sv->solid = SOLID_NOT;
			VectorScale(move, 1.1, move2);
			VectorCopy (check->priv.sv->moved_from, check->fields.sv->origin);
			VectorCopy (check->priv.sv->moved_fromangles, check->fields.sv->angles);
			SV_PushEntity (check, move2, true);
			pusher->fields.sv->solid = savesolid;
			if (SV_ClipMoveToEntity(pusher, check->fields.sv->origin, check->fields.sv->mins, check->fields.sv->maxs, check->fields.sv->origin, MASK_SOLID).startsolid)
			{
				// try moving the contacted entity a tiny bit less to account for precision errors
				pusher->fields.sv->solid = SOLID_NOT;
				VectorScale(move, 0.9, move2);
				VectorCopy (check->priv.sv->moved_from, check->fields.sv->origin);
				VectorCopy (check->priv.sv->moved_fromangles, check->fields.sv->angles);
				SV_PushEntity (check, move2, true);
				pusher->fields.sv->solid = savesolid;
				if (SV_ClipMoveToEntity(pusher, check->fields.sv->origin, check->fields.sv->mins, check->fields.sv->maxs, check->fields.sv->origin, MASK_SOLID).startsolid)
				{
					// still inside pusher, so it's really blocked

					// fail the move
					if (check->fields.sv->mins[0] == check->fields.sv->maxs[0])
						continue;
					if (check->fields.sv->solid == SOLID_NOT || check->fields.sv->solid == SOLID_TRIGGER)
					{
						// corpse
						check->fields.sv->mins[0] = check->fields.sv->mins[1] = 0;
						VectorCopy (check->fields.sv->mins, check->fields.sv->maxs);
						continue;
					}

					VectorCopy (pushorig, pusher->fields.sv->origin);
					VectorCopy (pushang, pusher->fields.sv->angles);
					pusher->fields.sv->ltime = pushltime;
					SV_LinkEdict (pusher);

					// move back any entities we already moved
					for (i = 0;i < num_moved;i++)
					{
						prvm_edict_t *ed = sv.moved_edicts[i];
						VectorCopy (ed->priv.sv->moved_from, ed->fields.sv->origin);
						VectorCopy (ed->priv.sv->moved_fromangles, ed->fields.sv->angles);
						SV_LinkEdict (ed);
					}

					// if the pusher has a "blocked" function, call it, otherwise just stay in place until the obstacle is gone
					if (pusher->fields.sv->blocked)
					{
						prog->globals.server->self = PRVM_EDICT_TO_PROG(pusher);
						prog->globals.server->other = PRVM_EDICT_TO_PROG(check);
						PRVM_ExecuteProgram (pusher->fields.sv->blocked, "QC function self.blocked is missing");
					}
					break;
				}
			}
		}
	}
	pusher->fields.sv->angles[0] -= 360.0 * floor(pusher->fields.sv->angles[0] * (1.0 / 360.0));
	pusher->fields.sv->angles[1] -= 360.0 * floor(pusher->fields.sv->angles[1] * (1.0 / 360.0));
	pusher->fields.sv->angles[2] -= 360.0 * floor(pusher->fields.sv->angles[2] * (1.0 / 360.0));
}

/*
================
SV_PhysicsPusher

================
*/
void SV_PhysicsPusher (prvm_edict_t *ent)
{
	float thinktime, oldltime, movetime;

	oldltime = ent->fields.sv->ltime;

	thinktime = ent->fields.sv->nextthink;
	if (thinktime < ent->fields.sv->ltime + sv.frametime)
	{
		movetime = thinktime - ent->fields.sv->ltime;
		if (movetime < 0) movetime = 0;
	}
	else movetime = sv.frametime;

	if (movetime)
	{
		// advances ent->fields.sv->ltime if not blocked
		SV_PushMove(ent, movetime);
	}

	if (thinktime > oldltime && thinktime <= ent->fields.sv->ltime)
	{
		// called QC think function		
		ent->fields.sv->nextthink = 0;
		prog->globals.server->time = sv.time;
		prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
		prog->globals.server->other = PRVM_EDICT_TO_PROG(prog->edicts);
		PRVM_ExecuteProgram (ent->fields.sv->think, "QC function self.think is missing");
	}
}

/*
===============================================================================

CLIENT MOVEMENT

===============================================================================
*/
/*
=============
SV_CheckStuck

This is a big hack to try and fix the rare case of getting stuck in the world
clipping hull.
=============
*/
void SV_CheckStuck (prvm_edict_t *ent)
{
	int	x, y, z;
	vec3_t	org;

	if (!SV_TestEntityPosition(ent))
	{
		VectorCopy (ent->fields.sv->origin, ent->fields.sv->oldorigin);
		return;
	}

	VectorCopy (ent->fields.sv->origin, org);
	VectorCopy (ent->fields.sv->oldorigin, ent->fields.sv->origin);

	if(!SV_TestEntityPosition(ent))
	{
		Msg("Unstuck player entity %i (classname \"%s\") by restoring oldorigin.\n", (int)PRVM_EDICT_TO_PROG(ent), PRVM_GetString(ent->fields.sv->classname));
		SV_LinkEdict (ent);
		return;
	}

	for (z = -1; z < 18; z++)
	{
		for (x = -1; x <= 1; x++)
		{
			for (y =-1; y <= 1; y++)
			{
				ent->fields.sv->origin[0] = org[0] + x;
				ent->fields.sv->origin[1] = org[1] + y;
				ent->fields.sv->origin[2] = org[2] + z;
				if (!SV_TestEntityPosition(ent))
				{
					Msg("Unstuck player entity %i (classname \"%s\") with offset %f %f %f.\n", (int)PRVM_EDICT_TO_PROG(ent), PRVM_GetString(ent->fields.sv->classname), (float)x, (float)y, (float)z);
					SV_LinkEdict (ent);
					return;
				}
			}
		}
	}

	VectorCopy (org, ent->fields.sv->origin);
	Msg("Stuck player %i (classname \"%s\").\n", (int)PRVM_EDICT_TO_PROG(ent), PRVM_GetString(ent->fields.sv->classname));
}

void SV_UnstickEntity (prvm_edict_t *ent)
{
	int	x, y, z;
	vec3_t	org;

	// if not stuck in a bmodel, just return
	if (!SV_TestEntityPosition(ent))
		return;

	VectorCopy (ent->fields.sv->origin, org);

	for (z = -1; z < 18; z += 6)
	{
		for (x = -1; x <= 1; x++)
		{
			for (y =-1; y <= 1; y++)
			{
				ent->fields.sv->origin[0] = org[0] + x;
				ent->fields.sv->origin[1] = org[1] + y;
				ent->fields.sv->origin[2] = org[2] + z;
				if (!SV_TestEntityPosition(ent))
				{
					Msg("Unstuck entity %i (classname \"%s\") with offset %f %f %f.\n", (int)PRVM_EDICT_TO_PROG(ent), PRVM_GetString(ent->fields.sv->classname), (float)x, (float)y, (float)z);
					SV_LinkEdict(ent);
					return;
				}
			}
		}
	}

	VectorCopy (org, ent->fields.sv->origin);
	Msg("Stuck entity %i (classname \"%s\").\n", (int)PRVM_EDICT_TO_PROG(ent), PRVM_GetString(ent->fields.sv->classname));
}

/*
=============
SV_CheckWater
=============
*/
bool SV_CheckWater (prvm_edict_t *ent)
{
	int	cont;
	vec3_t	point;

	point[0] = ent->fields.sv->origin[0];
	point[1] = ent->fields.sv->origin[1];
	point[2] = ent->fields.sv->origin[2] + ent->fields.sv->mins[2] + 1;

	ent->fields.sv->waterlevel = 0;
	ent->fields.sv->watertype = CONTENTS_NONE;
	cont = SV_PointContents(point);

	if (cont & MASK_WATER)
	{
		ent->fields.sv->watertype = cont;
		ent->fields.sv->waterlevel = 1;
		point[2] = ent->fields.sv->origin[2] + (ent->fields.sv->mins[2] + ent->fields.sv->maxs[2]) * 0.5;

		if(SV_PointContents(point) & MASK_WATER)
		{
			ent->fields.sv->waterlevel = 2;
			point[2] = ent->fields.sv->origin[2] + ent->fields.sv->view_ofs[2];
			if (SV_PointContents(point) & MASK_WATER) ent->fields.sv->waterlevel = 3;
		}
	}

	return ent->fields.sv->waterlevel > 1;
}

/*
============
SV_WallFriction

============
*/
void SV_WallFriction (prvm_edict_t *ent, float *stepnormal)
{
	float	d, i;
	vec3_t	forward, into, side;

	AngleVectorsRight(ent->fields.sv->v_angle, forward, NULL, NULL);

	if ((d = DotProduct (stepnormal, forward) + 0.5) < 0)
	{
		// cut the tangential velocity
		i = DotProduct (stepnormal, ent->fields.sv->velocity);
		VectorScale (stepnormal, i, into);
		VectorSubtract (ent->fields.sv->velocity, into, side);
		ent->fields.sv->velocity[0] = side[0] * (1 + d);
		ent->fields.sv->velocity[1] = side[1] * (1 + d);
	}
}

/*
=====================
SV_TryUnstick

Player has come to a dead stop, possibly due to the problem with limited
float precision at some angle joins in the BSP hull.

Try fixing by pushing one pixel in each direction.

This is a hack, but in the interest of good gameplay...
======================
*/
int SV_TryUnstick (prvm_edict_t *ent, vec3_t oldvel)
{
	int i, clip;
	vec3_t oldorg, dir;

	VectorCopy (ent->fields.sv->origin, oldorg);
	VectorClear (dir);

	for (i = 0; i < 8; i++)
	{
		// try pushing a little in an axial direction
		switch (i)
		{
			case 0: dir[0] = 2; dir[1] = 0; break;
			case 1: dir[0] = 0; dir[1] = 2; break;
			case 2: dir[0] = -2; dir[1] = 0; break;
			case 3: dir[0] = 0; dir[1] = -2; break;
			case 4: dir[0] = 2; dir[1] = 2; break;
			case 5: dir[0] = -2; dir[1] = 2; break;
			case 6: dir[0] = 2; dir[1] = -2; break;
			case 7: dir[0] = -2; dir[1] = -2; break;
		}

		SV_PushEntity (ent, dir, false);

		// retry the original move
		ent->fields.sv->velocity[0] = oldvel[0];
		ent->fields.sv->velocity[1] = oldvel[1];
		ent->fields.sv->velocity[2] = 0;
		clip = SV_FlyMove (ent, 0.1, NULL);

		if (fabs(oldorg[1] - ent->fields.sv->origin[1]) > 4 || fabs(oldorg[0] - ent->fields.sv->origin[0]) > 4)
		{
			Msg("SV_TryUnstick: success.\n");
			return clip;
		}
		// go back to the original pos and try again
		VectorCopy (oldorg, ent->fields.sv->origin);
	}

	// still not moving
	VectorClear (ent->fields.sv->velocity);
	Msg("SV_TryUnstick: failure.\n");
	return 7;
}

/*
=====================
SV_WalkMove

Only used by players
======================
*/
void SV_WalkMove(prvm_edict_t *ent)
{
	int clip, oldonground, originalmove_clip, originalmove_flags, originalmove_groundentity;
	vec3_t upmove, downmove, start_origin, start_velocity, stepnormal, originalmove_origin, originalmove_velocity;
	trace_t downtrace;

	SV_CheckVelocity(ent);

	// do a regular slide move unless it looks like you ran into a step
	oldonground = (int)ent->fields.sv->flags & FL_ONGROUND;
	ent->fields.sv->flags = (int)ent->fields.sv->flags & ~FL_ONGROUND;

	VectorCopy (ent->fields.sv->origin, start_origin);
	VectorCopy (ent->fields.sv->velocity, start_velocity);

	clip = SV_FlyMove (ent, sv.frametime, NULL);

	SV_CheckVelocity(ent);

	VectorCopy(ent->fields.sv->origin, originalmove_origin);
	VectorCopy(ent->fields.sv->velocity, originalmove_velocity);
	originalmove_clip = clip;
	originalmove_flags = (int)ent->fields.sv->flags;
	originalmove_groundentity = ent->fields.sv->groundentity;

	if ((int)ent->fields.sv->flags & FL_WATERJUMP)
		return;

	// if move didn't block on a step, return
	if (clip & 2)
	{
		// if move was not trying to move into the step, return
		if (fabs(start_velocity[0]) < 0.03125 && fabs(start_velocity[1]) < 0.03125)
			return;

		if (ent->fields.sv->movetype != MOVETYPE_FLY)
		{
			// return if gibbed by a trigger
			if (ent->fields.sv->movetype != MOVETYPE_WALK)
				return;
		}

		// try moving up and forward to go up a step
		// back to start pos
		VectorCopy (start_origin, ent->fields.sv->origin);
		VectorCopy (start_velocity, ent->fields.sv->velocity);

		// move up
		VectorClear (upmove);
		upmove[2] = 18;

		// FIXME: don't link?
		SV_PushEntity(ent, upmove, false);

		// move forward
		ent->fields.sv->velocity[2] = 0;
		clip = SV_FlyMove (ent, sv.frametime, stepnormal);
		ent->fields.sv->velocity[2] += start_velocity[2];

		SV_CheckVelocity(ent);

		// check for stuckness, possibly due to the limited precision of floats
		// in the clipping hulls
		if (clip && fabs(originalmove_origin[1] - ent->fields.sv->origin[1]) < 0.03125 && fabs(originalmove_origin[0] - ent->fields.sv->origin[0]) < 0.03125)
		{
			Msg("wall\n");
			// stepping up didn't make any progress, revert to original move
			VectorCopy(originalmove_origin, ent->fields.sv->origin);
			VectorCopy(originalmove_velocity, ent->fields.sv->velocity);
			ent->fields.sv->flags = originalmove_flags;
			ent->fields.sv->groundentity = originalmove_groundentity;
			return;
		}

		//Msg("step - ");

		// extra friction based on view angle
		if (clip & 2) SV_WallFriction (ent, stepnormal);
	}
	// skip out if stepdown is enabled, moving downward, not in water, and the move started onground and ended offground
	else if (ent->fields.sv->waterlevel < 2 && start_velocity[2] < (1.0 / 32.0) && oldonground && !((int)ent->fields.sv->flags & FL_ONGROUND))
		return;

	// move down
	VectorClear (downmove);
	downmove[2] = -18 + start_velocity[2] * sv.frametime;

	// FIXME: don't link?
	downtrace = SV_PushEntity (ent, downmove, false);

	if (downtrace.fraction < 1 && downtrace.plane.normal[2] > 0.7)
	{
		// this has been disabled so that you can't jump when you are stepping
		// up while already jumping (also known as the Quake2 stair jump bug)
	}
	else
	{
		Msg("slope\n");

		// if the push down didn't end up on good ground, use the move without
		// the step up.  This happens near wall / slope combinations, and can
		// cause the player to hop up higher on a slope too steep to climb
		VectorCopy(originalmove_origin, ent->fields.sv->origin);
		VectorCopy(originalmove_velocity, ent->fields.sv->velocity);
		ent->fields.sv->flags = originalmove_flags;
		ent->fields.sv->groundentity = originalmove_groundentity;
	}

	SV_CheckVelocity(ent);
}

/*
=============
SV_Physics_Follow

Entities that are "stuck" to another entity
=============
*/
void SV_PhysicsFollow (prvm_edict_t *ent)
{
	vec3_t		vf, vr, vu, angles, v;
	prvm_edict_t	*e;

	// regular thinking
	if (!SV_EntityThink (ent)) return;

	e = PRVM_PROG_TO_EDICT(ent->fields.sv->aiment);
	if (VectorCompare(e->fields.sv->angles, ent->fields.sv->punchangle ))  
	{
		// quick case for no rotation
		VectorAdd(e->fields.sv->origin, ent->fields.sv->view_ofs, ent->fields.sv->origin);
	}
	else
	{
		angles[0] = -ent->fields.sv->punchangle[0];
		angles[1] =  ent->fields.sv->punchangle[1];
		angles[2] =  ent->fields.sv->punchangle[2];

		AngleVectorsRight(angles, vf, vr, vu);

		v[0] = ent->fields.sv->view_ofs[0] * vf[0] + ent->fields.sv->view_ofs[1] * vr[0] + ent->fields.sv->view_ofs[2] * vu[0];
		v[1] = ent->fields.sv->view_ofs[0] * vf[1] + ent->fields.sv->view_ofs[1] * vr[1] + ent->fields.sv->view_ofs[2] * vu[1];
		v[2] = ent->fields.sv->view_ofs[0] * vf[2] + ent->fields.sv->view_ofs[1] * vr[2] + ent->fields.sv->view_ofs[2] * vu[2];
		angles[0] = -e->fields.sv->angles[0];	// stupid quake legacy
		angles[1] =  e->fields.sv->angles[1];
		angles[2] =  e->fields.sv->angles[2];

		AngleVectorsRight(angles, vf, vr, vu);

		ent->fields.sv->origin[0] = v[0] * vf[0] + v[1] * vf[1] + v[2] * vf[2] + e->fields.sv->origin[0];
		ent->fields.sv->origin[1] = v[0] * vr[0] + v[1] * vr[1] + v[2] * vr[2] + e->fields.sv->origin[1];
		ent->fields.sv->origin[2] = v[0] * vu[0] + v[1] * vu[1] + v[2] * vu[2] + e->fields.sv->origin[2];
	}
	VectorAdd (e->fields.sv->angles, ent->fields.sv->v_angle, ent->fields.sv->angles);
	SV_LinkEdict (ent);
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
void SV_CheckWaterTransition (prvm_edict_t *ent)
{
	int cont = SV_PointContents(ent->fields.sv->origin);

	if (!ent->fields.sv->watertype)
	{
		// just spawned here
		ent->fields.sv->watertype = cont;
		ent->fields.sv->waterlevel = 1;
		return;
	}

	if (cont & MASK_WATER)
	{
		ent->fields.sv->watertype = cont;
		ent->fields.sv->waterlevel = 1;
	}
	else
	{
		ent->fields.sv->watertype = CONTENTS_NONE;
		ent->fields.sv->waterlevel = 0;
	}
}


/*
=============
SV_PhysicsToss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_PhysicsToss (prvm_edict_t *ent)
{
	trace_t	trace;
	vec3_t	move;

	// if onground, return without moving
	if ((int)ent->fields.sv->flags & FL_ONGROUND)
	{
		if (ent->fields.sv->velocity[2] >= (1.0 / 32.0))
		{
			// don't stick to ground if onground and moving upward
			ent->fields.sv->flags -= FL_ONGROUND;
		}
		else if (!ent->fields.sv->groundentity)
		{
			// we can trust FL_ONGROUND if groundentity is world because it never moves
			return;
		}
	}
	SV_CheckVelocity (ent);

	// add gravity
	if (ent->fields.sv->movetype == MOVETYPE_TOSS || ent->fields.sv->movetype == MOVETYPE_BOUNCE)
		SV_AddGravity (ent);

	// move angles
	VectorMA (ent->fields.sv->angles, sv.frametime, ent->fields.sv->avelocity, ent->fields.sv->angles);

	// move origin
	VectorScale (ent->fields.sv->velocity, sv.frametime, move);
	trace = SV_PushEntity (ent, move, true);
	if (ent->priv.sv->free) return;

	if (trace.startstuck)
	{
		// try to unstick the entity
		SV_UnstickEntity(ent);
		trace = SV_PushEntity (ent, move, false);
		if (ent->priv.sv->free) return;
	}

	if (trace.fraction < 1)
	{
		if (ent->fields.sv->movetype == MOVETYPE_BOUNCE)
		{
			float d;
			ClipVelocity (ent->fields.sv->velocity, trace.plane.normal, ent->fields.sv->velocity, 1.5);
	
			d = DotProduct(trace.plane.normal, ent->fields.sv->velocity);
			if (trace.plane.normal[2] > 0.7 && fabs(d) < 60)
			{
				ent->fields.sv->flags = (int)ent->fields.sv->flags | FL_ONGROUND;
				ent->fields.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
				VectorClear (ent->fields.sv->velocity);
				VectorClear (ent->fields.sv->avelocity);
			}
			else ent->fields.sv->flags = (int)ent->fields.sv->flags & ~FL_ONGROUND;
		}
		else
		{
			ClipVelocity (ent->fields.sv->velocity, trace.plane.normal, ent->fields.sv->velocity, 1.0);
			if (trace.plane.normal[2] > 0.7)
			{
				ent->fields.sv->flags = (int)ent->fields.sv->flags | FL_ONGROUND;
				ent->fields.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
				VectorClear (ent->fields.sv->velocity);
				VectorClear (ent->fields.sv->avelocity);
			}
			else ent->fields.sv->flags = (int)ent->fields.sv->flags & ~FL_ONGROUND;
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
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
=============
*/
void SV_PhysicsStep (prvm_edict_t *ent)
{
	int flags = (int)ent->fields.sv->flags;

	// don't fall at all if fly/swim
	if (!(flags & (FL_FLY | FL_SWIM)))
	{
		if (flags & FL_ONGROUND)
		{
			// freefall if onground and moving upward
			// freefall if not standing on a world surface (it may be a lift or trap door)
			if (ent->fields.sv->velocity[2] >= (1.0 / 32.0) || ent->fields.sv->groundentity)
			{
				ent->fields.sv->flags -= FL_ONGROUND;
				SV_AddGravity(ent);
				SV_CheckVelocity(ent);
				SV_FlyMove(ent, sv.frametime, NULL);
				SV_LinkEdict(ent);
			}
		}
		else
		{
			// freefall if not onground
			int hitsound = ent->fields.sv->velocity[2] < sv_gravity->value * -0.1;

			SV_AddGravity(ent);
			SV_CheckVelocity(ent);
			SV_FlyMove(ent, sv.frametime, NULL);
			SV_LinkEdict(ent);
		}
	}

	// regular thinking
	SV_EntityThink(ent);
	SV_CheckWaterTransition(ent);
}

void SV_PhysicsClient(prvm_edict_t *ent)
{
	int	i;

	/*for (i = 0; i < 3; i++)
	{
		ent->priv.sv->client->pmove.origin[i] = ent->priv.sv->state.origin[i];
		ent->priv.sv->client->pmove.velocity[i] = ent->fields.sv->velocity[i];
	}*/
	return;

	SV_ApplyClientMove();
	SV_CheckVelocity(ent); // make sure the velocity is sane (not a NaN)
	SV_ClientThink();
	SV_CheckVelocity(ent); // make sure the velocity is sane (not a NaN)

	// call standard client pre-think
	prog->globals.server->time = sv.time;
	prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram (prog->globals.server->PlayerPreThink, "QC function PlayerPreThink is missing");
	SV_CheckVelocity (ent);

	switch ((int)ent->fields.sv->movetype)
	{
	case MOVETYPE_PUSH:
		SV_PhysicsPusher(ent);
		break;
	case MOVETYPE_NONE:
		if (ent->fields.sv->nextthink > 0 && ent->fields.sv->nextthink <= sv.time + sv.frametime)
			SV_EntityThink(ent);
		break;
	case MOVETYPE_FOLLOW:
		SV_PhysicsFollow(ent);
		break;
	case MOVETYPE_NOCLIP:
		if (SV_EntityThink(ent))
		{
			SV_CheckWater(ent);
			VectorMA(ent->fields.sv->origin, sv.frametime, ent->fields.sv->velocity, ent->fields.sv->origin);
			VectorMA(ent->fields.sv->angles, sv.frametime, ent->fields.sv->avelocity, ent->fields.sv->angles);
		}
		break;
	case MOVETYPE_STEP:
		SV_PhysicsStep(ent);
		break;
	case MOVETYPE_WALK:
		if (SV_EntityThink(ent))
		{
			if(!SV_CheckWater(ent) && ! ((int)ent->fields.sv->flags & FL_WATERJUMP))
				SV_AddGravity (ent);
			SV_CheckStuck (ent);
			SV_WalkMove (ent);
		}
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		if (SV_EntityThink (ent))
			SV_PhysicsToss (ent);
		break;
	case MOVETYPE_FLY:
		if(SV_EntityThink (ent))
		{
			SV_CheckWater(ent);
			SV_WalkMove(ent);
		}
		break;
	default:
		MsgWarn("SV_PhysicsClient: bad movetype %i\n", (int)ent->fields.sv->movetype);
		break;
	}

	SV_CheckVelocity (ent);
	SV_LinkEdict(ent);
	SV_CheckVelocity(ent);

	// call standard player post-think
	prog->globals.server->time = sv.time;
	prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram (prog->globals.server->PlayerPostThink, "QC function PlayerPostThink is missing");
}

void SV_PhysicsEntity(prvm_edict_t *ent)
{
	// don't run a move on newly spawned projectiles as it messes up movement
	// interpolation and rocket trails
	bool runmove = ent->priv.sv->move;
	ent->priv.sv->move = true;

	switch ((int) ent->fields.sv->movetype)
	{
	case MOVETYPE_PUSH:
		SV_PhysicsPusher (ent);
		break;
	case MOVETYPE_NONE:
		if (ent->fields.sv->nextthink > 0 && ent->fields.sv->nextthink <= sv.time + sv.frametime)
			SV_EntityThink (ent);
		break;
	case MOVETYPE_FOLLOW:
		SV_PhysicsFollow (ent);
		break;
	case MOVETYPE_NOCLIP:
		if (SV_EntityThink(ent))
		{
			SV_CheckWater(ent);
			VectorMA(ent->fields.sv->origin, sv.frametime, ent->fields.sv->velocity, ent->fields.sv->origin);
			VectorMA(ent->fields.sv->angles, sv.frametime, ent->fields.sv->avelocity, ent->fields.sv->angles);
		}
		SV_LinkEdict(ent);
		break;
	case MOVETYPE_STEP:
		SV_PhysicsStep(ent);
		break;
	case MOVETYPE_WALK:
		if(SV_EntityThink(ent))
		{
			if (!SV_CheckWater (ent) && !((int)ent->fields.sv->flags & FL_WATERJUMP))
				SV_AddGravity (ent);
			SV_CheckStuck (ent);
			SV_WalkMove (ent);
			SV_LinkEdict (ent);
		}
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_FLY:
		if (SV_EntityThink (ent) && runmove)
			SV_PhysicsToss (ent);
		break;
	default:
		MsgWarn("SV_Physics: bad movetype %i\n", (int)ent->fields.sv->movetype);
		break;
	}
}

void SV_Physics (void)
{
	int i;
	prvm_edict_t *ent;

	// we always need to bump framenum, even if we
	// don't run the world, otherwise the delta
	// compression can get confused when a client
	// has the "current" frame
	sv.framenum++;
	sv.frametime = sv.framenum * 0.1;

	if(sv_paused->value && host.maxclients == 1) return;

	// let the progs know that a new frame has started
	prog->globals.server->self = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.server->other = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.server->time = sv.time;
	prog->globals.server->frametime = sv.frametime;
	PRVM_ExecuteProgram (prog->globals.server->StartFrame, "QC function StartFrame is missing");

	// run physics on the client entities
	for (i = 1, ent = PRVM_EDICT_NUM(i), sv_client = svs.clients; i <= host.maxclients; i++, ent = PRVM_NEXT_EDICT(ent), sv_client++)
	{
		if (!ent->priv.sv->free)
		{
			// don't do physics on disconnected clients, FrikBot relies on this
			if (sv_client->state != cs_spawned)
				memset(&sv_client->lastcmd, 0, sizeof(sv_client->lastcmd));
			else SV_PhysicsClient(ent);
		}
	}
	
	for (;i < prog->num_edicts; i++, ent = PRVM_NEXT_EDICT(ent))
	{
		if (!ent->priv.sv->free) SV_PhysicsEntity(ent);
	}

	prog->globals.server->self = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.server->other = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.server->time = sv.time;
	PRVM_ExecuteProgram (prog->globals.server->EndFrame, "QC function EndFrame is missing");

	// decrement prog->num_edicts if the highest number entities died
	for ( ;PRVM_EDICT_NUM(prog->num_edicts - 1)->priv.sv->free; prog->num_edicts-- );

	sv.time += sv.frametime;
}