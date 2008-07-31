//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_physics.c - server physic
//=======================================================================

#include "common.h"
#include "server.h"
#include "matrixlib.h"

/*
pushmove objects do not obey gravity, and do not interact with each other or trigger fields, but block normal movement and push normal objects when they move.

onground is set for toss objects when they come to a complete rest.  it is set for steping or walking objects

doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
corpses are SOLID_NOT and MOVETYPE_TOSS
crates are SOLID_BBOX and MOVETYPE_TOSS
walking monsters are SOLID_BBOX and MOVETYPE_STEP
flying/floating monsters are SOLID_BBOX and MOVETYPE_FLY

solid_edge items only clip against bsp models.
*/

#define MOVE_EPSILON	0.01
#define MAX_CLIP_PLANES	32

/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/
int SV_ContentsMask( const edict_t *passedict )
{
	if( passedict )
	{
		if((int)passedict->progs.sv->flags & FL_MONSTER)
			return MASK_MONSTERSOLID;
		else if((int)passedict->progs.sv->flags & FL_CLIENT)
			return MASK_PLAYERSOLID;
		else if( passedict->progs.sv->solid == SOLID_TRIGGER )
			return CONTENTS_SOLID|CONTENTS_BODY;
		return MASK_SOLID;
	}
	return MASK_SOLID;
}

/*
==================
SV_Trace
==================
*/
trace_t SV_Trace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int type, edict_t *passedict, int contentsmask )
{
	vec3_t		hullmins, hullmaxs;
	int		i, bodycontents;
	int		passedictprog;
	bool		pointtrace;
	edict_t		*traceowner, *touch;
	trace_t		trace;
	vec3_t		clipboxmins, clipboxmaxs;// bounding box of entire move area
	vec3_t		clipmins, clipmaxs;// size of the moving object
	vec3_t		clipmins2, clipmaxs2;// size when clipping against monsters
	vec3_t		clipstart, clipend;// start and end origin of move
	trace_t		cliptrace;// trace results
	matrix4x4		matrix, imatrix;// matrices to transform into/out of other entity's space
	cmodel_t		*model;// model of other entity
	int		numtouchedicts;// list of entities to test for collisions
	edict_t		*touchedicts[MAX_EDICTS];

	VectorCopy( start, clipstart );
	VectorCopy( end, clipend );
	VectorCopy( mins, clipmins );
	VectorCopy( maxs, clipmaxs );
	VectorCopy( mins, clipmins2 );
	VectorCopy( maxs, clipmaxs2 );

	// clip to world
	pe->ClipToWorld( &cliptrace, sv.worldmodel, clipstart, clipmins, clipmaxs, clipend, contentsmask );
	cliptrace.startstuck = cliptrace.startsolid;
	if( cliptrace.startsolid || cliptrace.fraction < 1 )
		cliptrace.ent = prog->edicts;
	if( type == MOVE_WORLDONLY )
		return cliptrace;

	if( type == MOVE_MISSILE )
	{
		// LordHavoc: modified this, was = -15, now -= 15
		for( i = 0; i < 3; i++ )
		{
			clipmins2[i] -= 15;
			clipmaxs2[i] += 15;
		}
	}

	VectorCopy( clipmins, hullmins );
	VectorCopy( clipmaxs, hullmaxs );

	// create the bounding box of the entire move
	for( i = 0; i < 3; i++ )
	{
		clipboxmins[i] = min(clipstart[i], cliptrace.endpos[i]) + min(hullmins[i], clipmins2[i]) - 1;
		clipboxmaxs[i] = max(clipstart[i], cliptrace.endpos[i]) + max(hullmaxs[i], clipmaxs2[i]) + 1;
	}

	// if the passedict is world, make it NULL (to avoid two checks each time)
	if( passedict == prog->edicts ) passedict = NULL;
	// precalculate prog value for passedict for comparisons
	passedictprog = PRVM_EDICT_TO_PROG(passedict);
	// figure out whether this is a point trace for comparisons
	pointtrace = VectorCompare(clipmins, clipmaxs);
	// precalculate passedict's owner edict pointer for comparisons
	traceowner = passedict ? PRVM_PROG_TO_EDICT(passedict->progs.sv->owner) : 0;

	// clip to entities
	// because this uses World_EntitiestoBox, we know all entity boxes overlap
	// the clip region, so we can skip culling checks in the loop below
	numtouchedicts = SV_AreaEdicts( clipboxmins, clipboxmaxs, touchedicts, host.max_edicts );
	if( numtouchedicts > host.max_edicts )
	{
		// this never happens
		MsgDev( D_WARN, "SV_AreaEdicts returned %i edicts, max was %i\n", numtouchedicts, host.max_edicts );
		numtouchedicts = host.max_edicts;
	}
	for( i = 0; i < numtouchedicts; i++ )
	{
		touch = touchedicts[i];
		if( touch->progs.sv->solid < SOLID_BBOX ) continue;
		if( type == MOVE_NOMONSTERS && touch->progs.sv->solid != SOLID_BSP )
			continue;

		if( passedict )
		{
			// don't clip against self
			if( passedict == touch ) continue;
			// don't clip owned entities against owner
			if( traceowner == touch ) continue;
			// don't clip owner against owned entities
			if( passedictprog == touch->progs.sv->owner ) continue;
			// don't clip points against points (they can't collide)
			if( pointtrace && VectorCompare( touch->progs.sv->mins, touch->progs.sv->maxs ) && (type != MOVE_MISSILE || !((int)touch->progs.sv->flags & FL_MONSTER)))
				continue;
		}

		bodycontents = CONTENTS_BODY;

		// might interact, so do an exact clip
		model = NULL;
		if((int)touch->progs.sv->solid == SOLID_BSP || type == MOVE_HITMODEL )
		{
			uint modelindex = (uint)touch->progs.sv->modelindex;
			// if the modelindex is 0, it shouldn't be SOLID_BSP!
			if( modelindex > 0 && modelindex < MAX_MODELS )
				model = sv.models[(int)touch->progs.sv->modelindex];
		}
		if( model ) Matrix4x4_CreateFromEntity( matrix, touch->progs.sv->origin[0], touch->progs.sv->origin[1], touch->progs.sv->origin[2], touch->progs.sv->angles[0], touch->progs.sv->angles[1], touch->progs.sv->angles[2], 1 );
		else Matrix4x4_CreateTranslate( matrix, touch->progs.sv->origin[0], touch->progs.sv->origin[1], touch->progs.sv->origin[2] );
		Matrix4x4_Invert_Simple( imatrix, matrix );
		if((int)touch->progs.sv->flags & FL_MONSTER)
			pe->ClipToGenericEntity(&trace, model, touch->progs.sv->mins, touch->progs.sv->maxs, bodycontents, matrix, imatrix, clipstart, clipmins2, clipmaxs2, clipend, contentsmask );
		else pe->ClipToGenericEntity(&trace, model, touch->progs.sv->mins, touch->progs.sv->maxs, bodycontents, matrix, imatrix, clipstart, clipmins, clipmaxs, clipend, contentsmask );

		pe->CombineTraces( &cliptrace, &trace, touch, touch->progs.sv->solid == SOLID_BSP );
	}
	
	return cliptrace;
}

int SV_PointContents( const vec3_t point )
{
	int		i, contents = 0;
	edict_t		*touch;
	vec3_t		transformed;
	matrix4x4		matrix, imatrix;	// matrices to transform into/out of other entity's space
	cmodel_t		*model;		// model of other entity
	uint		modelindex;
	int		numtouchedicts;		// list of entities to test for collisions
	edict_t		*touchedicts[MAX_EDICTS];

	// get world supercontents at this point
	if( sv.worldmodel && sv.worldmodel->PointContents )
		contents = sv.worldmodel->PointContents( point, sv.worldmodel );

	// get list of entities at this point
	numtouchedicts = SV_AreaEdicts( point, point, touchedicts, host.max_edicts );
	if( numtouchedicts > host.max_edicts )
	{
		// this never happens
		MsgDev( D_WARN, "SV_AreaEdicts returned %i edicts, max was %i\n", numtouchedicts, host.max_edicts );
		numtouchedicts = host.max_edicts;
	}
	for( i = 0; i < numtouchedicts; i++ )
	{
		touch = touchedicts[i];

		// we only care about SOLID_BSP for pointcontents
		if( touch->progs.sv->solid != SOLID_BSP ) continue;

		// might interact, so do an exact clip
		modelindex = (uint)touch->progs.sv->modelindex;
		if( modelindex >= MAX_MODELS ) continue;
		model = sv.models[(int)touch->progs.sv->modelindex];
		if( !model || !model->PointContents ) continue;
		Matrix4x4_CreateFromEntity( matrix, touch->progs.sv->origin[0], touch->progs.sv->origin[1], touch->progs.sv->origin[2], touch->progs.sv->angles[0], touch->progs.sv->angles[1], touch->progs.sv->angles[2], 1 );
		Matrix4x4_Invert_Simple( imatrix, matrix );
		Matrix4x4_Transform( imatrix, point, transformed);
		contents |= model->PointContents( transformed, model );
	}

	return contents;
}

/*
===============================================================================

Utility functions

===============================================================================
*/
/*
============
SV_TestEntityPosition

returns true if the entity is in solid currently
============
*/
static int SV_TestEntityPosition( edict_t *ent, vec3_t offset )
{
	vec3_t	org;
	trace_t	trace;

	VectorAdd( ent->progs.sv->origin, offset, org );
	trace = SV_Trace( org, ent->progs.sv->mins, ent->progs.sv->maxs, ent->progs.sv->origin, MOVE_NOMONSTERS, ent, CONTENTS_SOLID );
	if( trace.startcontents & CONTENTS_SOLID )
		return true;
	// if the trace found a better position for the entity, move it there
	if( VectorDistance2( trace.endpos, ent->progs.sv->origin ) >= 0.0001 )
		VectorCopy( trace.endpos, ent->progs.sv->origin );
	return false;
}

/*
================
SV_CheckAllEnts
================
*/
void SV_CheckAllEnts( void )
{
	int	e;
	edict_t	*check;

	// see if any solid entities are inside the final position
	check = PRVM_NEXT_EDICT( prog->edicts );
	for( e = 1; e < prog->num_edicts; e++, check = PRVM_NEXT_EDICT(check))
	{
		if( check->priv.sv->free ) continue;
		switch((int)check->progs.sv->movetype )
		{
		case MOVETYPE_PUSH:
		case MOVETYPE_NONE:
		case MOVETYPE_FOLLOW:
		case MOVETYPE_NOCLIP:
			continue;
		default: break;
		}

		if(SV_TestEntityPosition( check, vec3_origin ))
			MsgDev( D_INFO, "entity in invalid position\n" );
	}
}

/*
================
SV_CheckVelocity
================
*/
void SV_CheckVelocity( edict_t *ent )
{
	int	i;
	float	wishspeed;

	// bound velocity
	for( i = 0; i < 3; i++ )
	{
		if(IS_NAN(ent->progs.sv->velocity[i]))
		{
			MsgDev( D_INFO, "Got a NaN velocity on entity #%i (%s)\n", PRVM_NUM_FOR_EDICT(ent), PRVM_GetString(ent->progs.sv->classname));
			ent->progs.sv->velocity[i] = 0;
		}
		if (IS_NAN(ent->progs.sv->origin[i]))
		{
			MsgDev( D_INFO, "Got a NaN origin on entity #%i (%s)\n", PRVM_NUM_FOR_EDICT(ent), PRVM_GetString(ent->progs.sv->classname));
			ent->progs.sv->origin[i] = 0;
		}
	}

	// LordHavoc: max velocity fix, inspired by Maddes's source fixes, but this is faster
	wishspeed = DotProduct( ent->progs.sv->velocity, ent->progs.sv->velocity );
	if( wishspeed > sv_maxvelocity->value * sv_maxvelocity->value )
	{
		wishspeed = sv_maxvelocity->value / sqrt(wishspeed);
		ent->progs.sv->velocity[0] *= wishspeed;
		ent->progs.sv->velocity[1] *= wishspeed;
		ent->progs.sv->velocity[2] *= wishspeed;
	}
}

/*
=============
SV_RunThink

Runs thinking code if time.  There is some play in the exact time the think
function will be called, because it is called before any movement is done
in a frame.  Not used for pushmove objects, because they must be exact.
Returns false if the entity removed itself.
=============
*/
bool SV_RunThink( edict_t *ent )
{
	int	iterations;

	// don't let things stay in the past.
	// it is possible to start that way by a trigger with a local time.
	if( ent->progs.sv->nextthink <= 0 || ent->progs.sv->nextthink > sv.time + sv.frametime )
		return true;

	for( iterations = 0; iterations < 128 && !ent->priv.sv->free; iterations++ )
	{
		prog->globals.sv->time = max( sv.time, ent->progs.sv->nextthink );
		ent->progs.sv->nextthink = 0;
		prog->globals.sv->pev = PRVM_EDICT_TO_PROG(ent);
		prog->globals.sv->other = PRVM_EDICT_TO_PROG(prog->edicts);
		PRVM_ExecuteProgram( ent->progs.sv->think, "pev->think" );
		// mods often set nextthink to time to cause a think every frame,
		// we don't want to loop in that case, so exit if the new nextthink is
		// <= the time the qc was told, also exit if it is past the end of the
		// frame
		if( ent->progs.sv->nextthink <= prog->globals.sv->time || ent->progs.sv->nextthink > sv.time + sv.frametime )
			break;
	}
	return !ent->priv.sv->free;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact( edict_t *e1, trace_t *trace )
{
	edict_t *e2 = (edict_t *)trace->ent;

	PRVM_PUSH_GLOBALS;
	VM_SetTraceGlobals( trace );

	prog->globals.sv->time = sv.time;
	if( !e1->priv.sv->free && !e2->priv.sv->free && e1->progs.sv->touch && e1->progs.sv->solid != SOLID_NOT )
	{
		prog->globals.sv->pev = PRVM_EDICT_TO_PROG(e1);
		prog->globals.sv->other = PRVM_EDICT_TO_PROG(e2);
		PRVM_ExecuteProgram( e1->progs.sv->touch, "pev->touch" );
	}

	if( !e1->priv.sv->free && !e2->priv.sv->free && e2->progs.sv->touch && e2->progs.sv->solid != SOLID_NOT )
	{
		prog->globals.sv->pev = PRVM_EDICT_TO_PROG(e2);
		prog->globals.sv->other = PRVM_EDICT_TO_PROG(e1);
		VectorCopy( e2->progs.sv->origin, prog->globals.sv->trace_endpos );
		VectorNegate( trace->plane.normal, prog->globals.sv->trace_plane_normal );
		prog->globals.sv->trace_plane_dist = -trace->plane.dist;
		prog->globals.sv->trace_ent = PRVM_EDICT_TO_PROG(e1);
		PRVM_ExecuteProgram( e2->progs.sv->touch, "pev->touch" );
	}
	PRVM_POP_GLOBALS;
}

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
SV_ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
void SV_ClipVelocity( vec3_t in, vec3_t normal, vec3_t out, float overbounce )
{
	int i;
	float backoff;

	backoff = -DotProduct (in, normal) * overbounce;
	VectorMA( in, backoff, normal, out );

	for( i = 0; i < 3; i++ )
	{
		if( out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON )
			out[i] = 0;
	}
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
int SV_FlyMove( edict_t *ent, float time, float *stepnormal, int contentsmask )
{
	int	blocked = 0, bumpcount;
	int	i, j, impact, numplanes;
	float	d, time_left;
	vec3_t	dir, end, planes[MAX_CLIP_PLANES], primal_velocity, original_velocity, new_velocity;
	trace_t	trace;

	if( time <= 0 ) return 0;
	VectorCopy(ent->progs.sv->velocity, original_velocity);
	VectorCopy(ent->progs.sv->velocity, primal_velocity);
	numplanes = 0;
	time_left = time;

	for( bumpcount = 0; bumpcount < MAX_CLIP_PLANES; bumpcount++ )
	{
		if(!ent->progs.sv->velocity[0] && !ent->progs.sv->velocity[1] && !ent->progs.sv->velocity[2])
			break;

		VectorMA( ent->progs.sv->origin, time_left, ent->progs.sv->velocity, end );
		trace = SV_Trace( ent->progs.sv->origin, ent->progs.sv->mins, ent->progs.sv->maxs, end, MOVE_NORMAL, ent, contentsmask );

		// break if it moved the entire distance
		if( trace.fraction == 1 )
		{
			VectorCopy( trace.endpos, ent->progs.sv->origin );
			break;
		}

		if( !trace.ent )
		{
			MsgDev( D_WARN, "SV_FlyMove: trace.ent == NULL\n" );
			trace.ent = prog->edicts;
		}

		impact = !((int)ent->progs.sv->aiflags & AI_ONGROUND) || ent->progs.sv->groundentity != PRVM_EDICT_TO_PROG(trace.ent);

		if( trace.plane.normal[2] )
		{
			if( trace.plane.normal[2] > 0.7 )
			{
				// floor
				blocked |= 1;
				ent->progs.sv->aiflags = (int)ent->progs.sv->flags | AI_ONGROUND;
				ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
			}
		}
		else
		{
			// step
			blocked |= 2;
			// save the trace for player extrafriction
			if( stepnormal ) VectorCopy( trace.plane.normal, stepnormal );
		}

		if( trace.fraction >= 0.001 )
		{
			// actually covered some distance
			VectorCopy( trace.endpos, ent->progs.sv->origin );
			VectorCopy( ent->progs.sv->velocity, original_velocity );
			numplanes = 0;
		}

		// run the impact function
		if( impact )
		{
			SV_Impact( ent, &trace );

			// break if removed by the impact function
			if( ent->priv.sv->free ) break;
		}

		time_left *= 1 - trace.fraction;

		// clipped to another plane
		if( numplanes >= MAX_CLIP_PLANES )
		{
			// this shouldn't really happen
			VectorClear( ent->progs.sv->velocity );
			blocked = 3;
			break;
		}

		VectorCopy( trace.plane.normal, planes[numplanes] );
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for( i = 0; i < numplanes; i++ )
		{
			SV_ClipVelocity( original_velocity, planes[i], new_velocity, 1 );
			for( j = 0; j < numplanes; j++ )
			{
				if( j != i )
				{
					// not ok
					if(DotProduct( new_velocity, planes[j] ) < 0 )
						break;
				}
			}
			if( j == numplanes ) break;
		}
		if( i != numplanes )
		{
			// go along this plane
			VectorCopy(new_velocity, ent->progs.sv->velocity);
		}
		else
		{
			// go along the crease
			if( numplanes != 2 )
			{
				VectorClear( ent->progs.sv->velocity );
				blocked = 7;
				break;
			}
			CrossProduct( planes[0], planes[1], dir );
			VectorNormalize( dir );
			d = DotProduct( dir, ent->progs.sv->velocity );
			VectorScale( dir, d, ent->progs.sv->velocity );
		}

		// if current velocity is against the original velocity,
		// stop dead to avoid tiny occilations in sloping corners
		if( DotProduct(ent->progs.sv->velocity, primal_velocity ) <= 0 )
		{
			VectorClear( ent->progs.sv->velocity );
			break;
		}
	}

	// this came from QW and allows you to get out of water more easily
	if(((int)ent->progs.sv->aiflags & AI_WATERJUMP))
		VectorCopy( primal_velocity, ent->progs.sv->velocity );
	return blocked;
}

/*
============
SV_AddGravity

============
*/
void SV_AddGravity( edict_t *ent )
{
	ent->progs.sv->velocity[2] -= ent->progs.sv->gravity * sv_gravity->value * sv.frametime;
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
static trace_t SV_PushEntity( edict_t *ent, vec3_t push, bool failonstartstuck )
{
	int	type;
	trace_t	trace;
	vec3_t	end;

	VectorAdd( ent->progs.sv->origin, push, end );

	if( ent->progs.sv->solid == SOLID_TRIGGER || ent->progs.sv->solid == SOLID_NOT )
		type = MOVE_NOMONSTERS; // only clip against bmodels
	else type = MOVE_NORMAL;

	trace = SV_Trace( ent->progs.sv->origin, ent->progs.sv->mins, ent->progs.sv->maxs, end, type, ent, SV_ContentsMask( ent ));
	if( trace.startstuck && failonstartstuck )
		return trace;

	VectorCopy( trace.endpos, ent->progs.sv->origin );
	SV_LinkEdict( ent );

	if( ent->progs.sv->solid >= SOLID_TRIGGER && ent->progs.sv->solid < SOLID_BOX && trace.ent && (!((int)ent->progs.sv->aiflags & AI_ONGROUND) || ent->progs.sv->groundentity != PRVM_EDICT_TO_PROG(trace.ent)))
		SV_Impact( ent, &trace );
	return trace;
}

/*
============
SV_PushMove

============
*/
void SV_PushMove( edict_t *pusher, float movetime )
{
	int		i, e, index;
	int		checkcontents;
	bool		rotated;
	float		savesolid, movetime2, pushltime;
	vec3_t		mins, maxs, move, move1, moveangle;
	vec3_t		pushorig, pushang, a, forward, left, up, org;
	int		num_moved, numcheckentities;
	static edict_t	*checkentities[MAX_EDICTS];
	cmodel_t		*pushermodel;
	trace_t		trace;
	matrix4x4		pusherfinalmatrix, pusherfinalimatrix;
	word		moved_edicts[MAX_EDICTS];

	if (!pusher->progs.sv->velocity[0] && !pusher->progs.sv->velocity[1] && !pusher->progs.sv->velocity[2] && !pusher->progs.sv->avelocity[0] && !pusher->progs.sv->avelocity[1] && !pusher->progs.sv->avelocity[2])
	{
		pusher->progs.sv->ltime += movetime;
		return;
	}

	switch((int) pusher->progs.sv->solid )
	{
	case SOLID_BSP:
	case SOLID_BBOX:
		break;
	case SOLID_NOT:
	case SOLID_TRIGGER:
		VectorMA (pusher->progs.sv->origin, movetime, pusher->progs.sv->velocity, pusher->progs.sv->origin);
		VectorMA (pusher->progs.sv->angles, movetime, pusher->progs.sv->avelocity, pusher->progs.sv->angles);
		SV_ClampAngle( pusher->progs.sv->angles );
		pusher->progs.sv->ltime += movetime;
		SV_LinkEdict( pusher );
		return;
	default:
		MsgDev( D_WARN, "SV_PushMove: entity #%i, unrecognized solid type %f\n", PRVM_NUM_FOR_EDICT(pusher), pusher->progs.sv->solid );
		return;
	}
	index = (int)pusher->progs.sv->modelindex;
	if( index < 1 || index >= MAX_MODELS )
	{
		MsgDev( D_WARN, "SV_PushMove: entity #%i has an invalid modelindex %f\n", PRVM_NUM_FOR_EDICT(pusher), pusher->progs.sv->modelindex );
		return;
	}
	pushermodel = sv.models[index];

	rotated = VectorLength2(pusher->progs.sv->angles) + VectorLength2(pusher->progs.sv->avelocity) > 0;

	movetime2 = movetime;
	VectorScale( pusher->progs.sv->velocity, movetime2, move1 );
	VectorScale( pusher->progs.sv->avelocity, movetime2, moveangle );
	if( moveangle[0] || moveangle[2] )
	{
		for( i = 0; i < 3; i++ )
		{
			if( move1[i] > 0 )
			{
				mins[i] = pushermodel->rotatedmins[i] + pusher->progs.sv->origin[i] - 1;
				maxs[i] = pushermodel->rotatedmaxs[i] + move1[i] + pusher->progs.sv->origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->rotatedmins[i] + move1[i] + pusher->progs.sv->origin[i] - 1;
				maxs[i] = pushermodel->rotatedmaxs[i] + pusher->progs.sv->origin[i] + 1;
			}
		}
	}
	else if( moveangle[1] )
	{
		for( i = 0; i < 3; i++ )
		{
			if( move1[i] > 0 )
			{
				mins[i] = pushermodel->yawmins[i] + pusher->progs.sv->origin[i] - 1;
				maxs[i] = pushermodel->yawmaxs[i] + move1[i] + pusher->progs.sv->origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->yawmins[i] + move1[i] + pusher->progs.sv->origin[i] - 1;
				maxs[i] = pushermodel->yawmaxs[i] + pusher->progs.sv->origin[i] + 1;
			}
		}
	}
	else
	{
		for( i = 0; i < 3; i++ )
		{
			if( move1[i] > 0 )
			{
				mins[i] = pushermodel->normalmins[i] + pusher->progs.sv->origin[i] - 1;
				maxs[i] = pushermodel->normalmaxs[i] + move1[i] + pusher->progs.sv->origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->normalmins[i] + move1[i] + pusher->progs.sv->origin[i] - 1;
				maxs[i] = pushermodel->normalmaxs[i] + pusher->progs.sv->origin[i] + 1;
			}
		}
	}

	VectorNegate( moveangle, a );
	AngleVectorsFLU( a, forward, left, up );

	VectorCopy( pusher->progs.sv->origin, pushorig );
	VectorCopy( pusher->progs.sv->angles, pushang );
	pushltime = pusher->progs.sv->ltime;

	// move the pusher to its final position
	VectorMA( pusher->progs.sv->origin, movetime, pusher->progs.sv->velocity, pusher->progs.sv->origin );
	VectorMA( pusher->progs.sv->angles, movetime, pusher->progs.sv->avelocity, pusher->progs.sv->angles );
	pusher->progs.sv->ltime += movetime;
	SV_LinkEdict( pusher );

	pushermodel = NULL;
	if( pusher->progs.sv->modelindex >= 1 && pusher->progs.sv->modelindex < MAX_MODELS )
		pushermodel = sv.models[(int)pusher->progs.sv->modelindex];
	Matrix4x4_CreateFromEntity( pusherfinalmatrix, pusher->progs.sv->origin[0], pusher->progs.sv->origin[1], pusher->progs.sv->origin[2], pusher->progs.sv->angles[0], pusher->progs.sv->angles[1], pusher->progs.sv->angles[2], 1 );
	Matrix4x4_Invert_Simple( pusherfinalimatrix, pusherfinalmatrix );

	savesolid = pusher->progs.sv->solid;

	// see if any solid entities are inside the final position
	num_moved = 0;

	numcheckentities = SV_AreaEdicts( mins, maxs, checkentities, MAX_EDICTS );
	for( e = 0; e < numcheckentities; e++ )
	{
		edict_t	*check = checkentities[e];
		
		switch((int)check->progs.sv->movetype)
		{
		case MOVETYPE_NONE:
		case MOVETYPE_PUSH:
		case MOVETYPE_FOLLOW:
		case MOVETYPE_NOCLIP:
			continue;
		}

		// tell any MOVETYPE_STEP entity that it may need to check for water transitions
		check->priv.sv->forceupdate = true;
		checkcontents = SV_ContentsMask( check );

		// if the entity is standing on the pusher, it will definitely be moved
		// if the entity is not standing on the pusher, but is in the pusher's
		// final position, move it
		if(!((int)check->progs.sv->aiflags & AI_ONGROUND) || PRVM_PROG_TO_EDICT(check->progs.sv->groundentity) != pusher )
		{
			pe->ClipToGenericEntity( &trace, pushermodel, pusher->progs.sv->mins, pusher->progs.sv->maxs, CONTENTS_BODY, pusherfinalmatrix, pusherfinalimatrix, check->progs.sv->origin, check->progs.sv->mins, check->progs.sv->maxs, check->progs.sv->origin, checkcontents );
			if( !trace.startsolid ) continue;
		}

		if( rotated )
		{
			vec3_t org2;
			VectorSubtract( check->progs.sv->origin, pusher->progs.sv->origin, org );
			org2[0] = DotProduct( org, forward );
			org2[1] = DotProduct( org, left );
			org2[2] = DotProduct( org, up );
			VectorSubtract( org2, org, move );
			VectorAdd( move, move1, move );
		}
		else
		{
			VectorCopy( move1, move );
		}
		//Msg("- pushing %f %f %f\n", move[0], move[1], move[2]);

		VectorCopy (check->progs.sv->origin, check->priv.sv->moved_origin );
		VectorCopy (check->progs.sv->angles, check->priv.sv->moved_angles );
		moved_edicts[num_moved++] = PRVM_NUM_FOR_EDICT(check);

		// try moving the contacted entity
		pusher->progs.sv->solid = SOLID_NOT;
		trace = SV_PushEntity( check, move, true );
		// FIXME: turn players specially
		check->progs.sv->angles[1] += trace.fraction * moveangle[1];
		pusher->progs.sv->solid = savesolid; // was SOLID_BSP

		// this trace.fraction < 1 check causes items to fall off of pushers
		// if they pass under or through a wall
		// the groundentity check causes items to fall off of ledges
		if( check->progs.sv->movetype != MOVETYPE_WALK && (trace.fraction < 1 || PRVM_PROG_TO_EDICT(check->progs.sv->groundentity) != pusher))
			check->progs.sv->aiflags = (int)check->progs.sv->aiflags & ~AI_ONGROUND;

		// if it is still inside the pusher, block
		pe->ClipToGenericEntity( &trace, pushermodel, pusher->progs.sv->mins, pusher->progs.sv->maxs, CONTENTS_BODY, pusherfinalmatrix, pusherfinalimatrix, check->progs.sv->origin, check->progs.sv->mins, check->progs.sv->maxs, check->progs.sv->origin, checkcontents );
		if (trace.startsolid)
		{
			// try moving the contacted entity a tiny bit further to account for precision errors
			vec3_t move2;
			pusher->progs.sv->solid = SOLID_NOT;
			VectorScale( move, 1.1f, move2 );
			VectorCopy( check->priv.sv->moved_origin, check->progs.sv->origin );
			VectorCopy( check->priv.sv->moved_angles, check->progs.sv->angles );
			SV_PushEntity( check, move2, true );
			pusher->progs.sv->solid = savesolid;
			pe->ClipToGenericEntity( &trace, pushermodel, pusher->progs.sv->mins, pusher->progs.sv->maxs, CONTENTS_BODY, pusherfinalmatrix, pusherfinalimatrix, check->progs.sv->origin, check->progs.sv->mins, check->progs.sv->maxs, check->progs.sv->origin, checkcontents );
			if( trace.startsolid )
			{
				// try moving the contacted entity a tiny bit less to account for precision errors
				pusher->progs.sv->solid = SOLID_NOT;
				VectorScale( move, 0.9, move2 );
				VectorCopy( check->priv.sv->moved_origin, check->progs.sv->origin );
				VectorCopy( check->priv.sv->moved_angles, check->progs.sv->angles );
				SV_PushEntity( check, move2, true );
				pusher->progs.sv->solid = savesolid;
				pe->ClipToGenericEntity( &trace, pushermodel, pusher->progs.sv->mins, pusher->progs.sv->maxs, CONTENTS_BODY, pusherfinalmatrix, pusherfinalimatrix, check->progs.sv->origin, check->progs.sv->mins, check->progs.sv->maxs, check->progs.sv->origin, checkcontents );
				if( trace.startsolid )
				{
					// still inside pusher, so it's really blocked

					// fail the move
					if( check->progs.sv->mins[0] == check->progs.sv->maxs[0] ) continue;
					if( check->progs.sv->solid == SOLID_NOT || check->progs.sv->solid == SOLID_TRIGGER )
					{
						// corpse
						check->progs.sv->mins[0] = check->progs.sv->mins[1] = 0;
						VectorCopy( check->progs.sv->mins, check->progs.sv->maxs );
						continue;
					}

					VectorCopy( pushorig, pusher->progs.sv->origin );
					VectorCopy( pushang, pusher->progs.sv->angles );
					pusher->progs.sv->ltime = pushltime;
					SV_LinkEdict( pusher );

					// move back any entities we already moved
					for( i = 0; i < num_moved; i++ )
					{
						edict_t *ed = PRVM_EDICT_NUM( moved_edicts[i] );
						VectorCopy( ed->priv.sv->moved_origin, ed->progs.sv->origin );
						VectorCopy( ed->priv.sv->moved_angles, ed->progs.sv->angles );
						SV_LinkEdict( ed );
					}

					// if the pusher has a "blocked" function, call it, otherwise just stay in place until the obstacle is gone
					if( pusher->progs.sv->blocked )
					{
						prog->globals.sv->pev = PRVM_EDICT_TO_PROG( pusher );
						prog->globals.sv->other = PRVM_EDICT_TO_PROG( check );
						PRVM_ExecuteProgram( pusher->progs.sv->blocked, "pev->blocked" );
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
SV_Physics_Pusher

================
*/
void SV_Physics_Pusher( edict_t *ent )
{
	float	thinktime, oldltime, movetime;

	oldltime = ent->progs.sv->ltime;
	thinktime = ent->progs.sv->nextthink;

	if( thinktime < ent->progs.sv->ltime + sv.frametime)
	{
		movetime = thinktime - ent->progs.sv->ltime;
		if( movetime < 0 ) movetime = 0;
	}
	else movetime = sv.frametime;

	// advances ent->progs.sv->ltime if not blocked
	if( movetime ) SV_PushMove( ent, movetime );
	if( thinktime > oldltime && thinktime <= ent->progs.sv->ltime )
	{
		ent->progs.sv->nextthink = 0;
		prog->globals.sv->time = sv.time;
		prog->globals.sv->pev = PRVM_EDICT_TO_PROG(ent);
		prog->globals.sv->other = PRVM_EDICT_TO_PROG(prog->edicts);
		PRVM_ExecuteProgram( ent->progs.sv->think, "pev->think" );
	}
}

/*
===============================================================================

CLIENT MOVEMENT

===============================================================================
*/
static float unstickoffsets[] =
{
-1, 0, 0, 1, 0, 0, 0, -1, 0, 0,  1, 0, -1, -1, 0, 1, -1, 0, -1, 1,
0, 1, 1, 0, 0, 0, -1, 0, 0, 1, 0, 0, -2, 0, 0, 2, 0, 0, -3, 0, 0, 3,
0, 0, -4, 0, 0, 4, 0, 0, -5, 0, 0, 5, 0, 0, -6, 0, 0, 6, 0, 0, -7,
0, 0, 7, 0, 0, -8, 0, 0, 8, 0, 0, -9, 0, 0, 9, 0, 0, -10, 0, 0, 10,
0, 0, -11, 0, 0, 11, 0, 0, -12, 0, 0, 12, 0, 0, -13, 0, 0, 13, 0, 0,
-14, 0, 0, 14, 0, 0, -15, 0, 0, 15, 0, 0, -16, 0, 0, 16, 0, 0, -17,
0, 0, 17,
};

/*
=============
SV_CheckStuck

This is a big hack to try and fix the rare case of getting stuck in the world
clipping hull.
=============
*/
void SV_CheckStuck( edict_t *ent )
{
	int	i;
	vec3_t	offset;

	if(!SV_TestEntityPosition( ent, vec3_origin ))
	{
		VectorCopy( ent->progs.sv->origin, ent->progs.sv->old_origin );
		return;
	}

	for( i = 0; i < (int)(sizeof(unstickoffsets) / sizeof(unstickoffsets[0])); i += 3 )
	{
		if(!SV_TestEntityPosition( ent, unstickoffsets + i))
		{
			MsgDev( D_INFO, "Unstuck player with offset %g %g %g.\n", unstickoffsets[i+0], unstickoffsets[i+1], unstickoffsets[i+2]);
			SV_LinkEdict( ent );
			return;
		}
	}

	VectorSubtract( ent->progs.sv->old_origin, ent->progs.sv->origin, offset );
	if(!SV_TestEntityPosition( ent, offset ))
	{
		MsgDev( D_INFO, "Unstuck player by restoring oldorigin.\n" );
		SV_LinkEdict( ent );
		return;
	}
	MsgDev( D_INFO, "Stuck player\n" );
}

bool SV_UnstickEntity( edict_t *ent )
{
	int i;

	// if not stuck in a bmodel, just return
	if(!SV_TestEntityPosition( ent, vec3_origin ))
		return true;

	for( i = 0; i < (int)(sizeof(unstickoffsets) / sizeof(unstickoffsets[0])); i += 3 )
	{
		if(!SV_TestEntityPosition( ent, unstickoffsets + i ))
		{
			MsgDev( D_INFO, "Unstuck entity \"%s\" with offset %g %g %g.\n", PRVM_GetString(ent->progs.sv->classname), unstickoffsets[i+0], unstickoffsets[i+1], unstickoffsets[i+2]);
			SV_LinkEdict( ent );
			return true;
		}
	}

	MsgDev( D_INFO, "Stuck entity \"%s\".\n", PRVM_GetString(ent->progs.sv->classname));
	return false;
}

/*
=============
SV_CheckWater
=============
*/
bool SV_CheckWater( edict_t *ent )
{
	int	cont;
	vec3_t	point;

	point[0] = ent->progs.sv->origin[0];
	point[1] = ent->progs.sv->origin[1];
	point[2] = ent->progs.sv->origin[2] + ent->progs.sv->mins[2] + 1;

	ent->progs.sv->waterlevel = 0;
	ent->progs.sv->watertype = CONTENTS_NONE;
	cont = SV_PointContents( point );
	if( cont & (MASK_WATER))
	{
		ent->progs.sv->watertype = cont;
		ent->progs.sv->waterlevel = 1;
		point[2] = ent->progs.sv->origin[2] + (ent->progs.sv->mins[2] + ent->progs.sv->maxs[2])*0.5;
		if(SV_PointContents( point ) & (MASK_WATER))
		{
			ent->progs.sv->waterlevel = 2;
			point[2] = ent->progs.sv->origin[2] + ent->progs.sv->view_ofs[2];
			if(SV_PointContents(point) & (MASK_WATER))
				ent->progs.sv->waterlevel = 3;
		}
	}
	return ent->progs.sv->waterlevel > 1;
}

/*
============
SV_WallFriction

============
*/
void SV_WallFriction( edict_t *ent, float *stepnormal )
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
=====================
SV_WalkMove

Only used by players
======================
*/
void SV_WalkMove( edict_t *ent )
{
	int	clip, oldonground, originalmove_clip, originalmove_flags;
	int	originalmove_groundentity, contentsmask;
	vec3_t	upmove, downmove, start_origin, start_velocity, stepnormal;
	vec3_t	originalmove_origin, originalmove_velocity;
	trace_t	downtrace;

	// if frametime is 0 (due to client sending the same timestamp twice), don't move
	if( sv.frametime <= 0 ) return;

	contentsmask = SV_ContentsMask( ent );

	SV_CheckVelocity( ent );

	// do a regular slide move unless it looks like you ran into a step
	oldonground = (int)ent->progs.sv->aiflags & AI_ONGROUND;

	VectorCopy( ent->progs.sv->origin, start_origin );
	VectorCopy( ent->progs.sv->velocity, start_velocity );

	clip = SV_FlyMove( ent, sv.frametime, NULL, contentsmask );

	// if the move did not hit the ground at any point, we're not on ground
	if(!(clip & 1)) ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags & ~AI_ONGROUND;

	SV_CheckVelocity( ent );

	VectorCopy( ent->progs.sv->origin, originalmove_origin );
	VectorCopy( ent->progs.sv->velocity, originalmove_velocity );
	originalmove_clip = clip;
	originalmove_flags = (int)ent->progs.sv->flags;
	originalmove_groundentity = ent->progs.sv->groundentity;

	if((int)ent->progs.sv->aiflags & AI_WATERJUMP)
		return;

	// if move didn't block on a step, return
	if( clip & 2 )
	{
		// if move was not trying to move into the step, return
		if(fabs(start_velocity[0]) < 0.03125 && fabs(start_velocity[1]) < 0.03125)
			return;

		if( ent->progs.sv->movetype != MOVETYPE_FLY )
		{
			// return if gibbed by a trigger
			if( ent->progs.sv->movetype != MOVETYPE_WALK )
				return;

			// only step up while jumping if that is enabled
			if( !oldonground && ent->progs.sv->waterlevel == 0 )
				return;
		}

		// try moving up and forward to go up a step
		// back to start pos
		VectorCopy( start_origin, ent->progs.sv->origin );
		VectorCopy( start_velocity, ent->progs.sv->velocity );

		// move up
		VectorClear( upmove );
		upmove[2] = sv_stepheight->value;

		SV_PushEntity( ent, upmove, false ); // FIXME: don't link?

		// move forward
		ent->progs.sv->velocity[2] = 0;
		clip = SV_FlyMove( ent, sv.frametime, stepnormal, contentsmask );
		ent->progs.sv->velocity[2] += start_velocity[2];

		SV_CheckVelocity( ent );

		// check for stuckness, possibly due to the limited precision of floats
		// in the clipping hulls
		if( clip && fabs(originalmove_origin[1] - ent->progs.sv->origin[1]) < 0.03125 && fabs(originalmove_origin[0] - ent->progs.sv->origin[0]) < 0.03125 )
		{
			// stepping up didn't make any progress, revert to original move
			VectorCopy( originalmove_origin, ent->progs.sv->origin );
			VectorCopy( originalmove_velocity, ent->progs.sv->velocity );
			ent->progs.sv->flags = originalmove_flags;
			ent->progs.sv->groundentity = originalmove_groundentity;
			return;
		}

		// extra friction based on view angle
		if( clip & 2 ) SV_WallFriction( ent, stepnormal );
	}
	// don't do the down move if stepdown is disabled, moving upward, not in water, or the move started offground or ended onground
	else if( ent->progs.sv->waterlevel >= 3 || start_velocity[2] >= (1.0 / 32.0) || !oldonground || ((int)ent->progs.sv->aiflags & AI_ONGROUND))
		return;

	// move down
	VectorClear( downmove );
	downmove[2] = -sv_stepheight->value + start_velocity[2] * sv.frametime;
	downtrace = SV_PushEntity( ent, downmove, false ); // FIXME: don't link?

	if( downtrace.fraction < 1 && downtrace.plane.normal[2] > 0.7 )
	{
		// this has been disabled so that you can't jump when you are stepping
		// up while already jumping (also known as the Quake2 double jump bug)
#if 0
		// LordHavoc: disabled this check so you can walk on monsters/players
		//if (ent->progs.sv->solid == SOLID_BSP)
		{
			//Con_Printf("onground\n");
			ent->progs.sv->flags =	(int)ent->progs.sv->flags | FL_ONGROUND;
			ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(downtrace.ent);
		}
#endif
	}
	else
	{
		// if the push down didn't end up on good ground, use the move without
		// the step up.  This happens near wall / slope combinations, and can
		// cause the player to hop up higher on a slope too steep to climb
		VectorCopy( originalmove_origin, ent->progs.sv->origin );
		VectorCopy( originalmove_velocity, ent->progs.sv->velocity );
		ent->progs.sv->flags = originalmove_flags;
		ent->progs.sv->groundentity = originalmove_groundentity;
	}
	SV_CheckVelocity( ent );
}

//============================================================================

/*
=============
SV_Physics_Follow

Entities that are "stuck" to another entity
=============
*/
void SV_Physics_Follow( edict_t *ent )
{
	vec3_t		vf, vr, vu, angles, v;
	edict_t		*e;

	// regular thinking
	if(!SV_RunThink( ent )) return;

	e = PRVM_PROG_TO_EDICT( ent->progs.sv->aiment );
	if(VectorCompare( e->progs.sv->angles, ent->progs.sv->punchangle ))
	{
		// quick case for no rotation
		VectorAdd( e->progs.sv->origin, ent->progs.sv->view_ofs, ent->progs.sv->origin );
	}
	else
	{
		angles[0] = -ent->progs.sv->punchangle[0];
		angles[1] =  ent->progs.sv->punchangle[1];
		angles[2] =  ent->progs.sv->punchangle[2];
		AngleVectors( angles, vf, vr, vu );
		v[0] = ent->progs.sv->view_ofs[0] * vf[0] + ent->progs.sv->view_ofs[1] * vr[0] + ent->progs.sv->view_ofs[2] * vu[0];
		v[1] = ent->progs.sv->view_ofs[0] * vf[1] + ent->progs.sv->view_ofs[1] * vr[1] + ent->progs.sv->view_ofs[2] * vu[1];
		v[2] = ent->progs.sv->view_ofs[0] * vf[2] + ent->progs.sv->view_ofs[1] * vr[2] + ent->progs.sv->view_ofs[2] * vu[2];
		angles[0] = -e->progs.sv->angles[0];
		angles[1] =  e->progs.sv->angles[1];
		angles[2] =  e->progs.sv->angles[2];
		AngleVectors( angles, vf, vr, vu );
		ent->progs.sv->origin[0] = v[0] * vf[0] + v[1] * vf[1] + v[2] * vf[2] + e->progs.sv->origin[0];
		ent->progs.sv->origin[1] = v[0] * vr[0] + v[1] * vr[1] + v[2] * vr[2] + e->progs.sv->origin[1];
		ent->progs.sv->origin[2] = v[0] * vu[0] + v[1] * vu[1] + v[2] * vu[2] + e->progs.sv->origin[2];
	}
	VectorAdd( e->progs.sv->angles, ent->progs.sv->v_angle, ent->progs.sv->angles );
	SV_LinkEdict( ent );
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
void SV_CheckWaterTransition( edict_t *ent )
{
	int	cont = SV_PointContents( ent->progs.sv->origin );

	if( !ent->progs.sv->watertype )
	{
		// just spawned here
		ent->progs.sv->watertype = cont;
		ent->progs.sv->waterlevel = 1;
		return;
	}

	// check if the entity crossed into or out of water
	if((int)ent->progs.sv->watertype & MASK_WATER ) 
	{
		Msg( "water splash!\n" );
		//SV_StartSound (ent, 0, "", 255, 1);
	}

	if( cont <= CONTENTS_WATER )
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
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_Physics_Toss( edict_t *ent )
{
	trace_t	trace;
	vec3_t	move;

	// if onground, return without moving
	if((int)ent->progs.sv->aiflags & AI_ONGROUND)
	{
		if( ent->progs.sv->velocity[2] >= (1.0 / 32.0))
		{
			// don't stick to ground if onground and moving upward
			ent->progs.sv->aiflags -= AI_ONGROUND;
		}
		else if( !ent->progs.sv->groundentity )
		{
			// we can trust AI_ONGROUND if groundentity is world because it never moves
			return;
		}
		else if( ent->priv.sv->suspended && PRVM_PROG_TO_EDICT(ent->progs.sv->groundentity)->priv.sv->free )
		{
			// if ent was supported by a brush model on previous frame,
			// and groundentity is now freed, set groundentity to 0 (world)
			// which leaves it suspended in the air
			ent->progs.sv->groundentity = 0;
			return;
		}
	}
	ent->priv.sv->suspended = false;

	SV_CheckVelocity( ent );

	// add gravity
	if( ent->progs.sv->movetype == MOVETYPE_TOSS || ent->progs.sv->movetype == MOVETYPE_BOUNCE )
		SV_AddGravity( ent );

	// move angles
	VectorMA( ent->progs.sv->angles, sv.frametime, ent->progs.sv->avelocity, ent->progs.sv->angles );

	// move origin
	VectorScale( ent->progs.sv->velocity, sv.frametime, move );
	trace = SV_PushEntity( ent, move, true );
	if( ent->priv.sv->free ) return;

	if( trace.startstuck )
	{
		// try to unstick the entity
		SV_UnstickEntity( ent );
		trace = SV_PushEntity( ent, move, false );
		if( ent->priv.sv->free )
			return;
	}

	if( trace.fraction < 1 )
	{
		if( ent->progs.sv->movetype == MOVETYPE_BOUNCE )
		{
			float	d;
			SV_ClipVelocity( ent->progs.sv->velocity, trace.plane.normal, ent->progs.sv->velocity, 1.5 );
			d = DotProduct( trace.plane.normal, ent->progs.sv->velocity );
			if( trace.plane.normal[2] > 0.7 && fabs(d) < 60 )
			{
				ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags | AI_ONGROUND;
				ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
				VectorClear( ent->progs.sv->velocity );
				VectorClear( ent->progs.sv->avelocity );
			}
			else ent->progs.sv->flags = (int)ent->progs.sv->aiflags & ~AI_ONGROUND;
		}
		else
		{
			SV_ClipVelocity( ent->progs.sv->velocity, trace.plane.normal, ent->progs.sv->velocity, 1.0 );
			if( trace.plane.normal[2] > 0.7 )
			{
				ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags | AI_ONGROUND;
				ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
				if( trace.ent->progs.sv->solid == SOLID_BSP ) 
					ent->priv.sv->suspended = true;
				VectorClear( ent->progs.sv->velocity );
				VectorClear( ent->progs.sv->avelocity );
			}
			else ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags & ~AI_ONGROUND;
		}
	}

	// check for in water
	SV_CheckWaterTransition( ent );
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
void SV_Physics_Step( edict_t *ent )
{
	int flags = (int)ent->progs.sv->aiflags;

	if(!(flags & (AI_FLY | AI_SWIM)))
	{
		if( flags & AI_ONGROUND )
		{
			// freefall if onground and moving upward
			// freefall if not standing on a world surface (it may be a lift or trap door)
			if(ent->progs.sv->velocity[2] >= (1.0 / 32.0) || ent->progs.sv->groundentity)
			{
				ent->progs.sv->aiflags -= AI_ONGROUND;
				SV_AddGravity( ent );
				SV_CheckVelocity( ent );
				SV_FlyMove( ent, sv.frametime, NULL, SV_ContentsMask( ent ));
				SV_LinkEdict( ent );
				ent->priv.sv->forceupdate = true;
			}
		}
		else
		{
			// freefall if not onground
			int hitsound = ent->progs.sv->velocity[2] < sv_gravity->value * -0.1;

			SV_AddGravity( ent );
			SV_CheckVelocity( ent );
			SV_FlyMove( ent, sv.frametime, NULL, SV_ContentsMask( ent ));
			SV_LinkEdict( ent );

			// just hit ground
			if( hitsound && (int)ent->progs.sv->aiflags & AI_ONGROUND )
			{
				Msg("Landing crash\n");
				//SV_StartSound(ent, 0, sv_sound_land.string, 255, 1);
			}
			ent->priv.sv->forceupdate = true;
		}
	}

	// regular thinking
	if(!SV_RunThink( ent )) return;

	if( ent->priv.sv->forceupdate || !VectorCompare( ent->progs.sv->origin, ent->priv.sv->water_origin))
	{
		ent->priv.sv->forceupdate = false;
		VectorCopy( ent->progs.sv->origin, ent->priv.sv->water_origin );
		SV_CheckWaterTransition( ent );
	}
}

/*
====================
SV_Physics_Conveyor

REAL simple - all we do is check for player riders and adjust their position.
Only gotcha here is we have to make sure we don't end up embedding player in
*another* object that's being moved by the conveyor.

====================
*/
void SV_Physics_Conveyor( edict_t *ent )
{
	edict_t		*player;
	int		i;
	trace_t		tr;
	vec3_t		v, move;
	vec3_t		point, end;

	VectorScale( ent->progs.sv->movedir, ent->progs.sv->speed, v );
	VectorScale( v, 0.1f, move );

	for( i = 0; i < maxclients->integer; i++ )
	{
		player = PRVM_EDICT_NUM(i) + 1;
		if( player->priv.sv->free ) continue;
		if( !player->progs.sv->groundentity ) continue;
		if( player->progs.sv->groundentity != PRVM_EDICT_TO_PROG(ent))
			continue;

		// Look below player; make sure he's on a conveyor
		VectorCopy( player->progs.sv->origin, point );
		point[2] += 1;
		VectorCopy( point, end );
		end[2] -= 256;

		tr = SV_Trace( point, player->progs.sv->mins, player->progs.sv->maxs, end, MOVE_NORMAL, player, MASK_SOLID );
		// tr.ent HAS to be conveyor, but just in case we screwed something up:
		if( tr.ent == ent )
		{
			if( tr.plane.normal[2] > 0 )
			{
				v[2] = ent->progs.sv->speed * sqrt(1.0 - tr.plane.normal[2]*tr.plane.normal[2]) / tr.plane.normal[2];
				if(DotProduct( ent->progs.sv->movedir, tr.plane.normal) > 0)
					v[2] = -v[2]; // then we're moving down
				move[2] = v[2] * 0.1f;
			}
			VectorAdd( player->progs.sv->origin, move, end );
			tr = SV_Trace( player->progs.sv->origin, player->progs.sv->mins, player->progs.sv->maxs, end, MOVE_NORMAL, player, player->priv.sv->clipmask );
			VectorCopy( tr.endpos, player->progs.sv->origin );
			SV_LinkEdict( player );
		}
	}
}

/*
=============
SV_PhysicsNoclip

A moving object that doesn't obey physics
=============
*/
void SV_Physics_Noclip( edict_t *ent )
{
	// regular thinking
	if(SV_RunThink( ent ))
	{
		SV_CheckWater( ent );	
		VectorMA( ent->progs.sv->angles, sv.frametime, ent->progs.sv->avelocity, ent->progs.sv->angles );
		VectorMA( ent->progs.sv->origin, sv.frametime, ent->progs.sv->velocity, ent->progs.sv->origin );
	}
	SV_LinkEdict(ent);
}


/*
=============
SV_PhysicsNone

Non moving objects can only think
=============
*/
void SV_Physics_None( edict_t *ent )
{
	if (ent->progs.sv->nextthink > 0 && ent->progs.sv->nextthink <= sv.time + sv.frametime)
		SV_RunThink (ent);
}


//============================================================================

static void SV_Physics_Entity( edict_t *ent )
{
	switch((int)ent->progs.sv->movetype)
	{
	case MOVETYPE_PUSH:
		SV_Physics_Pusher( ent );
		break;
	case MOVETYPE_NONE:
	case MOVETYPE_PHYSIC:
		SV_Physics_None( ent );
		break;
	case MOVETYPE_FOLLOW:
		SV_Physics_Follow( ent );
		break;
	case MOVETYPE_NOCLIP:
		SV_Physics_Noclip( ent );
		break;
	case MOVETYPE_STEP:
		SV_Physics_Step( ent );
		break;
	case MOVETYPE_WALK:
		if(SV_RunThink( ent ))
		{
			if(!SV_CheckWater( ent ) && !((int)ent->progs.sv->aiflags & AI_WATERJUMP ))
				SV_AddGravity( ent );
			SV_CheckStuck( ent );
			SV_WalkMove( ent );
			SV_LinkEdict( ent );
		}
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_FLY:
		if(SV_RunThink( ent )) SV_Physics_Toss( ent );
		break;
	case MOVETYPE_CONVEYOR:
		SV_Physics_Conveyor( ent );
		break;
	default:
		MsgDev( D_ERROR, "SV_Physics: bad movetype %i\n", (int)ent->progs.sv->movetype );
		break;
	}
}

void SV_Physics_ClientEntity( edict_t *ent )
{
	sv_client_t *client = ent->priv.sv->client;

	if( !client ) return;//Host_Error( "SV_Physics_ClientEntity: tired to apply physic to a non-client entity\n" );

	// don't do physics on disconnected clients, FrikBot relies on this
	if( client->state != cs_spawned )
	{
		memset( &client->cmd, 0, sizeof(client->cmd));
		return;
	}

	// don't run physics here if running asynchronously
	if( client->skipframes <= 0 ) SV_ClientThink( client, &client->cmd );

	// make sure the velocity is sane (not a NaN)
	SV_CheckVelocity( ent );
	if( DotProduct( ent->progs.sv->velocity, ent->progs.sv->velocity ) < 0.0001 )
		VectorClear( ent->progs.sv->velocity );

	// call standard client pre-think
	prog->globals.sv->time = sv.time;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG( ent );
	PRVM_ExecuteProgram (prog->globals.sv->PlayerPreThink, "PlayerPreThink" );
	SV_CheckVelocity( ent );

	switch((int)ent->progs.sv->movetype )
	{
	case MOVETYPE_PUSH:
		SV_Physics_Pusher( ent );
		break;
	case MOVETYPE_NONE:
	case MOVETYPE_PHYSIC:
		SV_Physics_None( ent );
		break;
	case MOVETYPE_FOLLOW:
		SV_Physics_Follow( ent );
		break;
	case MOVETYPE_NOCLIP:
		SV_RunThink( ent );
		SV_CheckWater( ent );
		VectorMA( ent->progs.sv->origin, sv.frametime, ent->progs.sv->velocity, ent->progs.sv->origin );
		VectorMA( ent->progs.sv->angles, sv.frametime, ent->progs.sv->avelocity, ent->progs.sv->angles );
		break;
	case MOVETYPE_STEP:
		SV_Physics_Step( ent );
		break;
	case MOVETYPE_WALK:
		SV_RunThink( ent );
		// don't run physics here if running asynchronously
		if( client->skipframes <= 0 )
		{
			if(!SV_CheckWater( ent ) && !((int)ent->progs.sv->aiflags & AI_WATERJUMP) )
				SV_AddGravity (ent);
			SV_CheckStuck (ent);
			SV_WalkMove (ent);
		}
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		// regular thinking
		SV_RunThink( ent );
		SV_Physics_Toss( ent );
		break;
	case MOVETYPE_FLY:
		SV_RunThink( ent );
		SV_CheckWater( ent );
		SV_WalkMove( ent );
		break;
	default:
		MsgDev( D_ERROR, "SV_Physics_ClientEntity: bad movetype %i\n", (int)ent->progs.sv->movetype);
		break;
	}

	// decrement the countdown variable used to decide when to go back to synchronous physics
	if( client->skipframes > 0 ) client->skipframes--;

	SV_CheckVelocity( ent );
	SV_LinkEdict( ent );
	SV_CheckVelocity( ent );

	// call standard player post-think
	prog->globals.sv->time = sv.time;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram( prog->globals.sv->PlayerPostThink, "PlayerPostThink" );

	if( ent->progs.sv->fixangle )
	{
		// angle fixing was requested by physics code...
		// so store the current angles for later use
		VectorCopy( ent->progs.sv->angles, client->fix_angles );
		ent->progs.sv->fixangle = false;// and clear fixangle for the next frame
		client->fixangle = true;
	}
}

void SV_Physics_ClientMove( sv_client_t *client, usercmd_t *cmd )
{
	edict_t	*ent = client->edict;

	// call player physics, this needs the proper frametime
	prog->globals.sv->frametime = sv.frametime;
	SV_ClientThink( client, cmd );

	// call standard client pre-think, with frametime = 0
	prog->globals.sv->time = sv.time;
	prog->globals.sv->frametime = 0;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram( prog->globals.sv->PlayerPreThink, "PlayerPreThink" );
	prog->globals.sv->frametime = sv.frametime;

	// make sure the velocity is sane (not a NaN)
	SV_CheckVelocity( ent );
	if(DotProduct(ent->progs.sv->velocity, ent->progs.sv->velocity) < 0.0001)
		VectorClear( ent->progs.sv->velocity );

	// perform MOVETYPE_WALK behavior
	if(!SV_CheckWater (ent) && !((int)ent->progs.sv->aiflags & AI_WATERJUMP))
		SV_AddGravity( ent );
	SV_CheckStuck( ent );
	SV_WalkMove( ent );
	SV_CheckVelocity( ent );
	SV_LinkEdict( ent );
	SV_CheckVelocity( ent );

	// call standard player post-think, with frametime = 0
	prog->globals.sv->time = sv.time;
	prog->globals.sv->frametime = 0;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG( ent );
	PRVM_ExecuteProgram( prog->globals.sv->PlayerPostThink, "PlayerPostThink" );
	prog->globals.sv->frametime = sv.frametime;

	if( ent->progs.sv->fixangle )
	{
		// angle fixing was requested by physics code...
		// so store the current angles for later use
		VectorCopy( ent->progs.sv->angles, client->fix_angles );
		ent->progs.sv->fixangle = false;// and clear fixangle for the next frame
		client->fixangle = true;
	}
}

/*
================
SV_Physics

================
*/
void SV_Physics( void )
{
	int    	i;
	edict_t	*ent;

	// let the progs know that a new frame has started
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.sv->other = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.sv->time = sv.time;
	prog->globals.sv->frametime = sv.frametime;
	PRVM_ExecuteProgram( prog->globals.sv->StartFrame, "StartFrame" );

	// treat each object in turn
	for( i = 1; i < prog->num_edicts; i++ )
	{
		ent = PRVM_EDICT_NUM(i);
		if( ent->priv.sv->free ) continue;

		VectorCopy( ent->progs.sv->origin, ent->progs.sv->old_origin );
		if(i <= maxclients->value) SV_Physics_ClientEntity( ent );
		else if(!sv_playersonly->integer)SV_Physics_Entity( ent );
	}

	// FIXME: calc frametime
	for( i = 0; i < 6; i++ )
	{
		if( sv_playersonly->integer )
			continue;
		pe->Frame( sv.frametime * i );
	}

	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.sv->other = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.sv->time = sv.time;
	PRVM_ExecuteProgram( prog->globals.sv->EndFrame, "EndFrame" );

	// decrement prog->num_edicts if the highest number entities died
	for( ;PRVM_EDICT_NUM(prog->num_edicts - 1)->priv.sv->free; prog->num_edicts-- );

	if( !sv_playersonly->integer ) sv.time += sv.frametime;
}