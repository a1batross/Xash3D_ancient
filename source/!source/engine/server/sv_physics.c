//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_physics.c - server physic
//=======================================================================

#include "engine.h"
#include "server.h"

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
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
trace_t SV_MovePushEntity (edict_t *ent, vec3_t push, bool failonbmodelstartsolid)
{
	trace_t trace;
	vec3_t end;

	VectorAdd (ent->progs.sv->origin, push, end);

	trace = SV_Trace(ent->progs.sv->origin, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, MASK_SOLID );
	
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
	int		numcheckentities;
	static edict_t	*checkentities[MAX_EDICTS];
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
		MsgWarn("SV_MovePush: %s, have invalid solid type %g\n", PRVM_ED_Info(pusher), pusher->progs.sv->solid);
		return;
	}

	index = (int)pusher->progs.sv->modelindex;
	if (index < 1 || index >= MAX_MODELS)
	{
		MsgWarn("SV_MovePush: %s, has an invalid modelindex %g\n", PRVM_ED_Info(pusher), pusher->progs.sv->modelindex);
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

	numcheckentities = 0;//SV_EntitiesInBox(mins, maxs, MAX_EDICTS, checkentities);
	for (e = 0;e < numcheckentities; e++)
	{
		edict_t *check = checkentities[e];
		if (check->progs.sv->movetype == MOVETYPE_NONE || check->progs.sv->movetype == MOVETYPE_PUSH || check->progs.sv->movetype == MOVETYPE_FOLLOW || check->progs.sv->movetype == MOVETYPE_NOCLIP)
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
		else
			VectorCopy (move1, move);

		VectorCopy (check->progs.sv->origin, check->progs.sv->post_origin);
		VectorCopy (check->progs.sv->angles, check->progs.sv->post_angles);
		sv.moved_edicts[num_moved++] = check;

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

					// move back any entities we already moved
					for (i = 0; i < num_moved; i++)
					{
						edict_t *ed = sv.moved_edicts[i];
						VectorCopy (ed->progs.sv->post_origin, ed->progs.sv->origin);
						VectorCopy (ed->progs.sv->post_angles, ed->progs.sv->angles);
						SV_LinkEdict (ed);
					}

					// if the pusher has a "blocked" function, call it, otherwise just stay in place until the obstacle is gone
					if (pusher->progs.sv->blocked)
					{
						prog->globals.server->pev = PRVM_EDICT_TO_PROG(pusher);
						prog->globals.server->other = PRVM_EDICT_TO_PROG(check);
						PRVM_ExecuteProgram (pusher->progs.sv->blocked, "QC function self.blocked is missing");
					}
					break;
				}
			}
		}
	}
	SV_ClampAngle( pusher->progs.sv->angles );
}

/*
================
SV_MovetypePush

================
*/
void SV_MovetypePush (edict_t *ent)
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
		prog->globals.server->time = sv.time;
		prog->globals.server->pev = PRVM_EDICT_TO_PROG(ent);
		prog->globals.server->other = PRVM_EDICT_TO_PROG(prog->edicts);
		PRVM_ExecuteProgram (ent->progs.sv->think, "QC function pev->think is missing");
	}
}