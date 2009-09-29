//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_physics.c - server physic
//=======================================================================

#include "common.h"
#include "server.h"
#include "matrix_lib.h"
#include "const.h"

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
		if( passedict->v.flags & FL_MONSTER )
			return MASK_MONSTERSOLID;
		else if( passedict->v.flags & FL_CLIENT )
			return MASK_PLAYERSOLID;
		else if( passedict->v.solid == SOLID_TRIGGER )
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
		cliptrace.ent = EDICT_NUM( 0 );
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
	if( passedict == EDICT_NUM( 0 )) passedict = NULL;
	// figure out whether this is a point trace for comparisons
	pointtrace = VectorCompare(clipmins, clipmaxs);
	// precalculate passedict's owner edict pointer for comparisons
	if( passedict && passedict->v.owner )
		traceowner = passedict->v.owner;
	else traceowner = NULL;

	// clip to entities
	// because this uses SV_AreaEdicts, we know all entity boxes overlap
	// the clip region, so we can skip culling checks in the loop below
	numtouchedicts = SV_AreaEdicts( clipboxmins, clipboxmaxs, touchedicts, GI->max_edicts, AREA_SOLID );
	if( numtouchedicts > GI->max_edicts )
	{
		// this never happens
		MsgDev( D_WARN, "SV_AreaEdicts returned %i edicts, max was %i\n", numtouchedicts, GI->max_edicts );
		numtouchedicts = GI->max_edicts;
	}
	for( i = 0; i < numtouchedicts; i++ )
	{
		touch = touchedicts[i];
		if( touch->v.solid < SOLID_BBOX ) continue;
		if( type == MOVE_NOMONSTERS && touch->v.solid != SOLID_BSP )
			continue;

		if( passedict )
		{
			// don't clip against self
			if( passedict == touch ) continue;
			// don't clip owned entities against owner
			if( traceowner == touch ) continue;
			// don't clip owner against owned entities
			if( passedict == touch->v.owner ) continue;
			// don't clip points against points (they can't collide)
			if( pointtrace && VectorCompare( touch->v.mins, touch->v.maxs ) && (type != MOVE_MISSILE || !(touch->v.flags & FL_MONSTER)))
				continue;
		}

		bodycontents = CONTENTS_BODY;

		// might interact, so do an exact clip
		model = NULL;
		if( touch->v.solid == SOLID_BSP || type == MOVE_HITMODEL )
		{
			uint modelindex = (uint)touch->v.modelindex;
			// if the modelindex is 0, it shouldn't be SOLID_BSP!
			if( modelindex > 0 && modelindex < MAX_MODELS )
				model = sv.models[touch->v.modelindex];
		}
		if( model ) Matrix4x4_CreateFromEntity( matrix, touch->v.origin[0], touch->v.origin[1], touch->v.origin[2], touch->v.angles[0], touch->v.angles[1], touch->v.angles[2], 1 );
		else Matrix4x4_CreateTranslate( matrix, touch->v.origin[0], touch->v.origin[1], touch->v.origin[2] );
		Matrix4x4_Invert_Simple( imatrix, matrix );
		if((int)touch->v.flags & FL_MONSTER)
			pe->ClipToGenericEntity(&trace, model, touch->v.mins, touch->v.maxs, bodycontents, matrix, imatrix, clipstart, clipmins2, clipmaxs2, clipend, contentsmask );
		else pe->ClipToGenericEntity(&trace, model, touch->v.mins, touch->v.maxs, bodycontents, matrix, imatrix, clipstart, clipmins, clipmaxs, clipend, contentsmask );

		pe->CombineTraces( &cliptrace, &trace, touch, touch->v.solid == SOLID_BSP );
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
	numtouchedicts = SV_AreaEdicts( point, point, touchedicts, GI->max_edicts, AREA_SOLID );
	if( numtouchedicts > GI->max_edicts )
	{
		// this never happens
		MsgDev( D_WARN, "SV_AreaEdicts returned %i edicts, max was %i\n", numtouchedicts, GI->max_edicts );
		numtouchedicts = GI->max_edicts;
	}
	for( i = 0; i < numtouchedicts; i++ )
	{
		touch = touchedicts[i];

		// we only care about SOLID_BSP for pointcontents
		if( touch->v.solid != SOLID_BSP ) continue;

		// might interact, so do an exact clip
		modelindex = (uint)touch->v.modelindex;
		if( modelindex >= MAX_MODELS ) continue;
		model = sv.models[(int)touch->v.modelindex];
		if( !model || !model->PointContents ) continue;
		Matrix4x4_CreateFromEntity( matrix, touch->v.origin[0], touch->v.origin[1], touch->v.origin[2], touch->v.angles[0], touch->v.angles[1], touch->v.angles[2], 1 );
		Matrix4x4_Invert_Simple( imatrix, matrix );
		Matrix4x4_VectorTransform( imatrix, point, transformed);
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

	VectorAdd( ent->v.origin, offset, org );
	trace = SV_Trace( org, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NOMONSTERS, ent, CONTENTS_SOLID );
	if( trace.startcontents & CONTENTS_SOLID )
		return true;
	// if the trace found a better position for the entity, move it there
	if( VectorDistance2( trace.endpos, ent->v.origin ) >= 0.0001 )
		VectorCopy( trace.endpos, ent->v.origin );
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
	check = EDICT_NUM( 1 );
	for( e = 1; e < svgame.globals->numEntities; e++, check++ )
	{
		if( check->free ) continue;
		switch( check->v.movetype )
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
		if(IS_NAN(ent->v.velocity[i]))
		{
			MsgDev( D_INFO, "Got a NaN velocity on entity #%i (%s)\n", NUM_FOR_EDICT( ent ), STRING( ent->v.classname ));
			ent->v.velocity[i] = 0;
		}
		if (IS_NAN(ent->v.origin[i]))
		{
			MsgDev( D_INFO, "Got a NaN origin on entity #%i (%s)\n", NUM_FOR_EDICT( ent ), STRING( ent->v.classname ));
			ent->v.origin[i] = 0;
		}
	}

	// LordHavoc: max velocity fix, inspired by Maddes's source fixes, but this is faster
	wishspeed = DotProduct( ent->v.velocity, ent->v.velocity );
	if( wishspeed > sv_maxvelocity->value * sv_maxvelocity->value )
	{
		wishspeed = sv_maxvelocity->value / sqrt(wishspeed);
		ent->v.velocity[0] *= wishspeed;
		ent->v.velocity[1] *= wishspeed;
		ent->v.velocity[2] *= wishspeed;
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
	float	thinktime;

	thinktime = ent->v.nextthink;
	if( thinktime <= 0 || thinktime > (sv.time * 0.001f) + svgame.globals->frametime )
		return true;

	if( thinktime < (sv.time * 0.001f))	// don't let things stay in the past.
		thinktime = sv.time * 0.001f;	// it is possible to start that way
					// by a trigger with a local time.
	svgame.globals->time = thinktime;
	ent->v.nextthink = 0;
	svgame.dllFuncs.pfnThink( ent );

	return !ent->free;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact( edict_t *e1, trace_t *trace )
{
	edict_t *e2 = trace->ent;

	SV_CopyTraceToGlobal( trace );

	svgame.globals->time = sv.time * 0.001f;
	if( !e1->free && !e2->free && e1->v.solid != SOLID_NOT )
	{
		svgame.dllFuncs.pfnTouch( e1, e2 );
	}

	if( !e1->free && !e2->free && e2->v.solid != SOLID_NOT )
	{
		VectorCopy( e2->v.origin, svgame.globals->trace_endpos );
		VectorNegate( trace->plane.normal, svgame.globals->trace_plane_normal );
		svgame.globals->trace_plane_dist = -trace->plane.dist;
		svgame.globals->trace_ent = e1;
		svgame.dllFuncs.pfnTouch( e2, e1 );
	}
}

/*
============
SV_TouchTriggers

called by player or monster
============
*/
void SV_TouchTriggers( edict_t *ent )
{
	int	i, num;
	edict_t	*touch[MAX_EDICTS];

	// dead things don't activate triggers!
	if(!(ent->v.flags & FL_CLIENT) && (ent->v.health <= 0))
		return;

	num = SV_AreaEdicts( ent->v.absmin, ent->v.absmax, touch, GI->max_edicts, AREA_TRIGGERS );

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for( i = 0; i < num; i++ )
	{
		if( touch[i]->free ) continue;
		svgame.globals->time = sv.time * 0.001f;
		svgame.dllFuncs.pfnTouch( touch[i], ent );
	}
}

/*
============
SV_ClampMove

clamp the move to 1/8 units, so the position will
be accurate for client side prediction
============
*/
void SV_ClampAngle( vec3_t angle )
{
	angle[0] -= 360.0 * floor(angle[0] * (1.0 / 360.0));
	angle[1] -= 360.0 * floor(angle[1] * (1.0 / 360.0));
	angle[2] -= 360.0 * floor(angle[2] * (1.0 / 360.0));
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
	VectorCopy( ent->v.velocity, original_velocity );
	VectorCopy( ent->v.velocity, primal_velocity );
	numplanes = 0;
	time_left = time;

	for( bumpcount = 0; bumpcount < MAX_CLIP_PLANES; bumpcount++ )
	{
		if(!ent->v.velocity[0] && !ent->v.velocity[1] && !ent->v.velocity[2])
			break;

		VectorMA( ent->v.origin, time_left, ent->v.velocity, end );
		trace = SV_Trace( ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent, contentsmask );

		// break if it moved the entire distance
		if( trace.fraction == 1 )
		{
			VectorCopy( trace.endpos, ent->v.origin );
			break;
		}

		if( !trace.ent )
		{
			MsgDev( D_WARN, "SV_FlyMove: trace.ent == NULL\n" );
			trace.ent = EDICT_NUM( 0 );
		}

		impact = !(ent->v.flags & FL_ONGROUND) || ent->v.groundentity != trace.ent;

		if( trace.plane.normal[2] )
		{
			if( trace.plane.normal[2] > 0.7 )
			{
				// floor
				blocked |= 1;
				ent->v.flags |= FL_ONGROUND;
				ent->v.groundentity = trace.ent;
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
			VectorCopy( trace.endpos, ent->v.origin );
			VectorCopy( ent->v.velocity, original_velocity );
			numplanes = 0;
		}

		// run the impact function
		if( impact )
		{
			SV_Impact( ent, &trace );

			// break if removed by the impact function
			if( ent->free ) break;
		}

		time_left *= 1 - trace.fraction;

		// clipped to another plane
		if( numplanes >= MAX_CLIP_PLANES )
		{
			// this shouldn't really happen
			VectorClear( ent->v.velocity );
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
			VectorCopy(new_velocity, ent->v.velocity);
		}
		else
		{
			// go along the crease
			if( numplanes != 2 )
			{
				VectorClear( ent->v.velocity );
				blocked = 7;
				break;
			}
			CrossProduct( planes[0], planes[1], dir );
			VectorNormalize( dir );
			d = DotProduct( dir, ent->v.velocity );
			VectorScale( dir, d, ent->v.velocity );
		}

		// if current velocity is against the original velocity,
		// stop dead to avoid tiny occilations in sloping corners
		if( DotProduct(ent->v.velocity, primal_velocity ) <= 0 )
		{
			VectorClear( ent->v.velocity );
			break;
		}
	}

	// this came from QW and allows you to get out of water more easily
	if( ent->v.flags & FL_WATERJUMP )
		VectorCopy( primal_velocity, ent->v.velocity );

	return blocked;
}

/*
============
SV_AddGravity

============
*/
void SV_AddGravity( edict_t *ent )
{
	if( ent->pvServerData->stuck )
	{
		VectorClear( ent->v.velocity );
		return;
	}
	if( ent->v.gravity ) // gravity modifier
		ent->v.velocity[2] -= sv_gravity->value * ent->v.gravity * svgame.globals->frametime;
	else ent->v.velocity[2] -= sv_gravity->value * svgame.globals->frametime;
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

	VectorAdd( ent->v.origin, push, end );

	if( ent->v.solid == SOLID_TRIGGER || ent->v.solid == SOLID_NOT )
		type = MOVE_NOMONSTERS; // only clip against bmodels
	else type = MOVE_NORMAL;

	trace = SV_Trace( ent->v.origin, ent->v.mins, ent->v.maxs, end, type, ent, SV_ContentsMask( ent ));
	if( trace.startstuck && failonstartstuck )
		return trace;

	VectorCopy( trace.endpos, ent->v.origin );
	SV_LinkEdict( ent );

	if( ent->v.solid >= SOLID_TRIGGER && ent->v.solid < SOLID_BOX && trace.ent && (!(ent->v.flags & FL_ONGROUND) || ent->v.groundentity != trace.ent ))
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

	if (!pusher->v.velocity[0] && !pusher->v.velocity[1] && !pusher->v.velocity[2] && !pusher->v.avelocity[0] && !pusher->v.avelocity[1] && !pusher->v.avelocity[2])
	{
		pusher->v.ltime += movetime;
		return;
	}

	switch((int) pusher->v.solid )
	{
	case SOLID_BSP:
	case SOLID_BBOX:
		break;
	case SOLID_NOT:
	case SOLID_TRIGGER:
		VectorMA (pusher->v.origin, movetime, pusher->v.velocity, pusher->v.origin);
		VectorMA (pusher->v.angles, movetime, pusher->v.avelocity, pusher->v.angles);
		SV_ClampAngle( pusher->v.angles );
		pusher->v.ltime += movetime;
		SV_LinkEdict( pusher );
		return;
	default:
		MsgDev( D_WARN, "SV_PushMove: entity #%i, unrecognized solid type %f\n", NUM_FOR_EDICT( pusher ), pusher->v.solid );
		return;
	}
	index = (int)pusher->v.modelindex;
	if( index < 1 || index >= MAX_MODELS )
	{
		MsgDev( D_WARN, "SV_PushMove: entity #%i has an invalid modelindex %f\n", NUM_FOR_EDICT( pusher ), pusher->v.modelindex );
		return;
	}
	pushermodel = sv.models[index];

	rotated = VectorLength2(pusher->v.angles) + VectorLength2(pusher->v.avelocity) > 0;

	movetime2 = movetime;
	VectorScale( pusher->v.velocity, movetime2, move1 );
	VectorScale( pusher->v.avelocity, movetime2, moveangle );
	if( moveangle[0] || moveangle[2] )
	{
		for( i = 0; i < 3; i++ )
		{
			if( move1[i] > 0 )
			{
				mins[i] = pushermodel->rotatedmins[i] + pusher->v.origin[i] - 1;
				maxs[i] = pushermodel->rotatedmaxs[i] + move1[i] + pusher->v.origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->rotatedmins[i] + move1[i] + pusher->v.origin[i] - 1;
				maxs[i] = pushermodel->rotatedmaxs[i] + pusher->v.origin[i] + 1;
			}
		}
	}
	else if( moveangle[1] )
	{
		for( i = 0; i < 3; i++ )
		{
			if( move1[i] > 0 )
			{
				mins[i] = pushermodel->yawmins[i] + pusher->v.origin[i] - 1;
				maxs[i] = pushermodel->yawmaxs[i] + move1[i] + pusher->v.origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->yawmins[i] + move1[i] + pusher->v.origin[i] - 1;
				maxs[i] = pushermodel->yawmaxs[i] + pusher->v.origin[i] + 1;
			}
		}
	}
	else
	{
		for( i = 0; i < 3; i++ )
		{
			if( move1[i] > 0 )
			{
				mins[i] = pushermodel->normalmins[i] + pusher->v.origin[i] - 1;
				maxs[i] = pushermodel->normalmaxs[i] + move1[i] + pusher->v.origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->normalmins[i] + move1[i] + pusher->v.origin[i] - 1;
				maxs[i] = pushermodel->normalmaxs[i] + pusher->v.origin[i] + 1;
			}
		}
	}

	VectorNegate( moveangle, a );
	AngleVectorsFLU( a, forward, left, up );

	VectorCopy( pusher->v.origin, pushorig );
	VectorCopy( pusher->v.angles, pushang );
	pushltime = pusher->v.ltime;

	// move the pusher to its final position
	VectorMA( pusher->v.origin, movetime, pusher->v.velocity, pusher->v.origin );
	VectorMA( pusher->v.angles, movetime, pusher->v.avelocity, pusher->v.angles );
	pusher->v.ltime += movetime;
	SV_LinkEdict( pusher );

	pushermodel = NULL;
	if( pusher->v.modelindex >= 1 && pusher->v.modelindex < MAX_MODELS )
		pushermodel = sv.models[(int)pusher->v.modelindex];
	Matrix4x4_CreateFromEntity( pusherfinalmatrix, pusher->v.origin[0], pusher->v.origin[1], pusher->v.origin[2], pusher->v.angles[0], pusher->v.angles[1], pusher->v.angles[2], 1 );
	Matrix4x4_Invert_Simple( pusherfinalimatrix, pusherfinalmatrix );

	savesolid = pusher->v.solid;

	// see if any solid entities are inside the final position
	num_moved = 0;

	numcheckentities = SV_AreaEdicts( mins, maxs, checkentities, MAX_EDICTS, AREA_SOLID );
	for( e = 0; e < numcheckentities; e++ )
	{
		edict_t	*check = checkentities[e];
		
		switch( check->v.movetype )
		{
		case MOVETYPE_NONE:
		case MOVETYPE_PUSH:
		case MOVETYPE_FOLLOW:
		case MOVETYPE_NOCLIP:
			continue;
		}

		// tell any MOVETYPE_STEP entity that it may need to check for water transitions
		check->pvServerData->forceupdate = true;
		checkcontents = SV_ContentsMask( check );

		// if the entity is standing on the pusher, it will definitely be moved
		// if the entity is not standing on the pusher, but is in the pusher's
		// final position, move it
		if(!( check->v.flags & FL_ONGROUND) || check->v.groundentity != pusher )
		{
			pe->ClipToGenericEntity( &trace, pushermodel, pusher->v.mins, pusher->v.maxs, CONTENTS_BODY, pusherfinalmatrix, pusherfinalimatrix, check->v.origin, check->v.mins, check->v.maxs, check->v.origin, checkcontents );
			if( !trace.startsolid ) continue;
		}

		if( rotated )
		{
			vec3_t org2;
			VectorSubtract( check->v.origin, pusher->v.origin, org );
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

		VectorCopy (check->v.origin, check->pvServerData->moved_origin );
		VectorCopy (check->v.angles, check->pvServerData->moved_angles );
		moved_edicts[num_moved++] = NUM_FOR_EDICT( check );

		// try moving the contacted entity
		pusher->v.solid = SOLID_NOT;
		trace = SV_PushEntity( check, move, true );
		// FIXME: turn players specially
		check->v.angles[1] += trace.fraction * moveangle[1];
		pusher->v.solid = savesolid; // was SOLID_BSP

		// this trace.fraction < 1 check causes items to fall off of pushers
		// if they pass under or through a wall
		// the groundentity check causes items to fall off of ledges
		if( check->v.movetype != MOVETYPE_WALK && (trace.fraction < 1 || check->v.groundentity != pusher ))
			check->v.flags &= ~FL_ONGROUND;

		// if it is still inside the pusher, block
		pe->ClipToGenericEntity( &trace, pushermodel, pusher->v.mins, pusher->v.maxs, CONTENTS_BODY, pusherfinalmatrix, pusherfinalimatrix, check->v.origin, check->v.mins, check->v.maxs, check->v.origin, checkcontents );
		if (trace.startsolid)
		{
			// try moving the contacted entity a tiny bit further to account for precision errors
			vec3_t move2;
			pusher->v.solid = SOLID_NOT;
			VectorScale( move, 1.1f, move2 );
			VectorCopy( check->pvServerData->moved_origin, check->v.origin );
			VectorCopy( check->pvServerData->moved_angles, check->v.angles );
			SV_PushEntity( check, move2, true );
			pusher->v.solid = savesolid;
			pe->ClipToGenericEntity( &trace, pushermodel, pusher->v.mins, pusher->v.maxs, CONTENTS_BODY, pusherfinalmatrix, pusherfinalimatrix, check->v.origin, check->v.mins, check->v.maxs, check->v.origin, checkcontents );
			if( trace.startsolid )
			{
				// try moving the contacted entity a tiny bit less to account for precision errors
				pusher->v.solid = SOLID_NOT;
				VectorScale( move, 0.9, move2 );
				VectorCopy( check->pvServerData->moved_origin, check->v.origin );
				VectorCopy( check->pvServerData->moved_angles, check->v.angles );
				SV_PushEntity( check, move2, true );
				pusher->v.solid = savesolid;
				pe->ClipToGenericEntity( &trace, pushermodel, pusher->v.mins, pusher->v.maxs, CONTENTS_BODY, pusherfinalmatrix, pusherfinalimatrix, check->v.origin, check->v.mins, check->v.maxs, check->v.origin, checkcontents );
				if( trace.startsolid )
				{
					// still inside pusher, so it's really blocked

					// fail the move
					if( check->v.mins[0] == check->v.maxs[0] ) continue;
					if( check->v.solid == SOLID_NOT || check->v.solid == SOLID_TRIGGER )
					{
						// corpse
						check->v.mins[0] = check->v.mins[1] = 0;
						VectorCopy( check->v.mins, check->v.maxs );
						continue;
					}

					VectorCopy( pushorig, pusher->v.origin );
					VectorCopy( pushang, pusher->v.angles );
					pusher->v.ltime = pushltime;
					SV_LinkEdict( pusher );

					// move back any entities we already moved
					for( i = 0; i < num_moved; i++ )
					{
						edict_t *ed = EDICT_NUM( moved_edicts[i] );
						VectorCopy( ed->pvServerData->moved_origin, ed->v.origin );
						VectorCopy( ed->pvServerData->moved_angles, ed->v.angles );
						SV_LinkEdict( ed );
					}

					// if the pusher has a "blocked" function, call it, otherwise just stay in place until the obstacle is gone
					svgame.dllFuncs.pfnBlocked( pusher, check );
					break;
				}
			}
		}
	}
	SV_ClampAngle( pusher->v.angles );
}

/*
================
SV_Physics_Pusher

================
*/
void SV_Physics_Pusher( edict_t *ent )
{
	float	thinktime, oldltime, movetime;

	oldltime = ent->v.ltime;
	thinktime = ent->v.nextthink;

	if( thinktime < ent->v.ltime + svgame.globals->frametime )
	{
		movetime = thinktime - ent->v.ltime;
		if( movetime < 0 ) movetime = 0;
	}
	else movetime = svgame.globals->frametime;

	// advances ent->v.ltime if not blocked
	if( movetime ) SV_PushMove( ent, movetime );
	if( thinktime > oldltime && thinktime <= ent->v.ltime )
	{
		ent->v.nextthink = 0;
		svgame.globals->time = sv.time * 0.001f;
		svgame.dllFuncs.pfnThink( ent );
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

	VectorClear( offset );
	if( ent->v.flags & FL_DUCKING )
	{
		offset[0] += 1;
		offset[1] += 1;
		offset[2] += 1;
	}
	if( !SV_TestEntityPosition( ent, offset ))
	{
		VectorCopy( ent->v.origin, ent->v.oldorigin );
		ent->pvServerData->stuck = false;
		return;
	}

	for( i = 0; i < (int)(sizeof(unstickoffsets) / sizeof(unstickoffsets[0])); i += 3 )
	{
		if(!SV_TestEntityPosition( ent, unstickoffsets + i))
		{
			MsgDev( D_NOTE, "Unstuck player with offset %g %g %g.\n", unstickoffsets[i+0], unstickoffsets[i+1], unstickoffsets[i+2]);
			ent->pvServerData->stuck = false;
			SV_LinkEdict( ent );
			return;
		}
	}

	VectorSubtract( ent->v.oldorigin, ent->v.origin, offset );
	if(!SV_TestEntityPosition( ent, offset ))
	{
		MsgDev( D_NOTE, "Unstuck player by restoring oldorigin.\n" );
		ent->pvServerData->stuck = false;
		SV_LinkEdict( ent );
		return;
	}

	if( !ent->pvServerData->stuck )
		MsgDev( D_ERROR, "Stuck player\n" );	// fire once
	ent->pvServerData->stuck = true;
}

bool SV_UnstickEntity( edict_t *ent )
{
	int i;

	// if not stuck in a bmodel, just return
	if( !SV_TestEntityPosition( ent, vec3_origin ))
	{
		ent->pvServerData->stuck = false;
		return true;
	}

	for( i = 0; i < (int)(sizeof(unstickoffsets) / sizeof(unstickoffsets[0])); i += 3 )
	{
		if(!SV_TestEntityPosition( ent, unstickoffsets + i ))
		{
			MsgDev( D_NOTE, "Unstuck entity \"%s\" with offset %g %g %g.\n", STRING( ent->v.classname ), unstickoffsets[i+0], unstickoffsets[i+1], unstickoffsets[i+2] );
			ent->pvServerData->stuck = false;
			SV_LinkEdict( ent );
			return true;
		}
	}

	if( !ent->pvServerData->stuck )
		MsgDev( D_ERROR, "Stuck entity \"%s\".\n", STRING( ent->v.classname ));
	ent->pvServerData->stuck = true;
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

	point[0] = ent->v.origin[0];
	point[1] = ent->v.origin[1];
	point[2] = ent->v.origin[2] + ent->v.mins[2] + 1;

	ent->v.waterlevel = 0;
	ent->v.watertype = CONTENTS_NONE;
	cont = SV_PointContents( point );

	if( cont & (MASK_WATER))
	{
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
		point[2] = ent->v.origin[2] + (ent->v.mins[2] + ent->v.maxs[2]) * 0.5;
		if( SV_PointContents( point ) & MASK_WATER )
		{
			ent->v.waterlevel = 2;
			point[2] = ent->v.origin[2] + ent->v.view_ofs[2];
			if( SV_PointContents( point ) & MASK_WATER )
			{
				ent->v.waterlevel = 3;
			}
		}
	}
	return ent->v.waterlevel > 1;
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

	AngleVectors( ent->v.viewangles, forward, NULL, NULL );
	if(( d = DotProduct( stepnormal, forward ) + 0.5 ) < 0 )
	{
		// cut the tangential velocity
		i = DotProduct( stepnormal, ent->v.velocity );
		VectorScale( stepnormal, i, into );
		VectorSubtract( ent->v.velocity, into, side );
		ent->v.velocity[0] = side[0] * (1 + d);
		ent->v.velocity[1] = side[1] * (1 + d);
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
	int	contentsmask;
	int	clip, oldonground, originalmove_clip, originalmove_flags;
	vec3_t	upmove, downmove, start_origin, start_velocity, stepnormal;
	vec3_t	originalmove_origin, originalmove_velocity;
	edict_t	*originalmove_groundentity;
	trace_t	downtrace;

	// if frametime is 0 (due to client sending the same timestamp twice), don't move
	if( svgame.globals->frametime <= 0 ) return;

	contentsmask = SV_ContentsMask( ent );
 	SV_CheckVelocity( ent );

	// do a regular slide move unless it looks like you ran into a step
	oldonground = (ent->v.flags & FL_ONGROUND);

	VectorCopy( ent->v.origin, start_origin );
	VectorCopy( ent->v.velocity, start_velocity );
	clip = SV_FlyMove( ent, svgame.globals->frametime, NULL, contentsmask );

	// if the move did not hit the ground at any point, we're not on ground
	if(!(clip & 1)) ent->v.flags &= ~FL_ONGROUND;

	SV_CheckVelocity( ent );

	VectorCopy( ent->v.origin, originalmove_origin );
	VectorCopy( ent->v.velocity, originalmove_velocity );
	originalmove_clip = clip;
	originalmove_flags = ent->v.flags;
	originalmove_groundentity = ent->v.groundentity;

	if( ent->v.flags & FL_WATERJUMP )
		return;

	// if move didn't block on a step, return
	if( clip & 2 )
	{
		// if move was not trying to move into the step, return
		if(fabs(start_velocity[0]) < 0.03125 && fabs(start_velocity[1]) < 0.03125)
			return;

		if( ent->v.movetype != MOVETYPE_FLY )
		{
			// return if gibbed by a trigger
			if( ent->v.movetype != MOVETYPE_WALK )
				return;

			// only step up while jumping if that is enabled
			if( !oldonground && ent->v.waterlevel == 0 )
				return;
		}

		// try moving up and forward to go up a step
		// back to start pos
		VectorCopy( start_origin, ent->v.origin );
		VectorCopy( start_velocity, ent->v.velocity );

		// move up
		VectorClear( upmove );
		upmove[2] = sv_stepheight->value;

		SV_PushEntity( ent, upmove, false ); // FIXME: don't link?

		// move forward
		ent->v.velocity[2] = 0;
		clip = SV_FlyMove( ent, svgame.globals->frametime, stepnormal, contentsmask );
		ent->v.velocity[2] += start_velocity[2];

		SV_CheckVelocity( ent );

		// check for stuckness, possibly due to the limited precision of floats
		// in the clipping hulls
		if( clip && fabs(originalmove_origin[1] - ent->v.origin[1]) < 0.03125 && fabs(originalmove_origin[0] - ent->v.origin[0]) < 0.03125 )
		{
			// stepping up didn't make any progress, revert to original move
			VectorCopy( originalmove_origin, ent->v.origin );
			VectorCopy( originalmove_velocity, ent->v.velocity );
			ent->v.flags = originalmove_flags;
			ent->v.groundentity = originalmove_groundentity;
			return;
		}

		// extra friction based on view angle
		if( clip & 2 ) SV_WallFriction( ent, stepnormal );
	}
	// don't do the down move if stepdown is disabled, moving upward, not in water, or the move started offground or ended onground
	else if( ent->v.waterlevel >= 3 || start_velocity[2] >= (1.0 / 32.0) || !oldonground || (ent->v.flags & FL_ONGROUND))
		return;

	// move down
	VectorClear( downmove );
	downmove[2] = -sv_stepheight->value + start_velocity[2] * svgame.globals->frametime;
	downtrace = SV_PushEntity( ent, downmove, false ); // FIXME: don't link?

	if( downtrace.fraction < 1 && downtrace.plane.normal[2] > 0.7 )
	{
		// this has been disabled so that you can't jump when you are stepping
		// up while already jumping (also known as the Quake2 double jump bug)
#if 0
		// LordHavoc: disabled this check so you can walk on monsters/players
		//if (ent->v.solid == SOLID_BSP)
		{
			//Con_Printf("onground\n");
			ent->v.flags |= FL_ONGROUND;
			ent->v.groundentity = downtrace.ent;
		}
#endif
	}
	else
	{
		// if the push down didn't end up on good ground, use the move without
		// the step up.  This happens near wall / slope combinations, and can
		// cause the player to hop up higher on a slope too steep to climb
		VectorCopy( originalmove_origin, ent->v.origin );
		VectorCopy( originalmove_velocity, ent->v.velocity );
		ent->v.flags = originalmove_flags;
		ent->v.groundentity = originalmove_groundentity;
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

	e = ent->v.aiment;
	if( !e || e->free ) return;

	if(VectorCompare( e->v.angles, ent->v.punchangle ))
	{
		// quick case for no rotation
		VectorAdd( e->v.origin, ent->v.view_ofs, ent->v.origin );
	}
	else
	{
		angles[0] = -ent->v.punchangle[0];
		angles[1] =  ent->v.punchangle[1];
		angles[2] =  ent->v.punchangle[2];
		AngleVectors( angles, vf, vr, vu );
		v[0] = ent->v.view_ofs[0] * vf[0] + ent->v.view_ofs[1] * vr[0] + ent->v.view_ofs[2] * vu[0];
		v[1] = ent->v.view_ofs[0] * vf[1] + ent->v.view_ofs[1] * vr[1] + ent->v.view_ofs[2] * vu[1];
		v[2] = ent->v.view_ofs[0] * vf[2] + ent->v.view_ofs[1] * vr[2] + ent->v.view_ofs[2] * vu[2];
		angles[0] = -e->v.angles[0];
		angles[1] =  e->v.angles[1];
		angles[2] =  e->v.angles[2];
		AngleVectors( angles, vf, vr, vu );
		ent->v.origin[0] = v[0] * vf[0] + v[1] * vf[1] + v[2] * vf[2] + e->v.origin[0];
		ent->v.origin[1] = v[0] * vr[0] + v[1] * vr[1] + v[2] * vr[2] + e->v.origin[1];
		ent->v.origin[2] = v[0] * vu[0] + v[1] * vu[1] + v[2] * vu[2] + e->v.origin[2];
	}
	VectorAdd( e->v.angles, ent->v.viewangles, ent->v.angles );
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
	int	cont = SV_PointContents( ent->v.origin );

	if( !ent->v.watertype )
	{
		// just spawned here
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
		return;
	}

	// check if the entity crossed into or out of water
	if( ent->v.watertype & MASK_WATER ) 
	{
		//Msg( "water splash!\n" );
		//SV_StartSound (ent, 0, "", 255, 1);
	}

	if( cont <= CONTENTS_WATER )
	{
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
	}
	else
	{
		ent->v.watertype = 0;
		ent->v.waterlevel = 0;
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
	if( ent->v.flags & FL_ONGROUND )
	{
		if( ent->v.velocity[2] >= (1.0 / 32.0))
		{
			// don't stick to ground if onground and moving upward
			ent->v.flags &= FL_ONGROUND;
		}
		else if( !ent->v.groundentity )
		{
			// we can trust FL_ONGROUND if groundentity is world because it never moves
			return;
		}
		else if( ent->pvServerData->suspended && ent->v.groundentity->free )
		{
			// if ent was supported by a brush model on previous frame,
			// and groundentity is now freed, set groundentity to 0 (world)
			// which leaves it suspended in the air
			ent->v.groundentity = EDICT_NUM( 0 );
			return;
		}
	}
	ent->pvServerData->suspended = false;

	SV_CheckVelocity( ent );

	// add gravity
	if( ent->v.movetype == MOVETYPE_TOSS || ent->v.movetype == MOVETYPE_BOUNCE )
		SV_AddGravity( ent );

	// move angles
	VectorMA( ent->v.angles, svgame.globals->frametime, ent->v.avelocity, ent->v.angles );

	// move origin
	VectorScale( ent->v.velocity, svgame.globals->frametime, move );
	trace = SV_PushEntity( ent, move, true );
	if( ent->free ) return;

	if( trace.startstuck )
	{
		// try to unstick the entity
		SV_UnstickEntity( ent );
		trace = SV_PushEntity( ent, move, false );
		if( ent->free )
			return;
	}

	if( trace.fraction < 1 )
	{
		if( ent->v.movetype == MOVETYPE_BOUNCE )
		{
			float	d;
			SV_ClipVelocity( ent->v.velocity, trace.plane.normal, ent->v.velocity, 1.5 );
			d = DotProduct( trace.plane.normal, ent->v.velocity );
			if( trace.plane.normal[2] > 0.7 && fabs(d) < 60 )
			{
				ent->v.flags |= FL_ONGROUND;
				ent->v.groundentity = trace.ent;
				VectorClear( ent->v.velocity );
				VectorClear( ent->v.avelocity );
			}
			else ent->v.flags &= ~FL_ONGROUND;
		}
		else
		{
			SV_ClipVelocity( ent->v.velocity, trace.plane.normal, ent->v.velocity, 1.0 );
			if( trace.plane.normal[2] > 0.7 )
			{
				ent->v.flags |= FL_ONGROUND;
				ent->v.groundentity = trace.ent;
				if( trace.ent && trace.ent->v.solid == SOLID_BSP ) 
					ent->pvServerData->suspended = true;
				VectorClear( ent->v.velocity );
				VectorClear( ent->v.avelocity );
			}
			else ent->v.flags &= ~FL_ONGROUND;
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
	int flags = ent->v.flags;

	if(!(flags & (FL_FLY|FL_SWIM)))
	{
		if( ent->v.flags & FL_ONGROUND )
		{
			// freefall if onground and moving upward
			// freefall if not standing on a world surface (it may be a lift or trap door)
			if(ent->v.velocity[2] >= (1.0 / 32.0) || ent->v.groundentity)
			{
				ent->v.flags &= ~FL_ONGROUND;
				SV_AddGravity( ent );
				SV_CheckVelocity( ent );
				SV_FlyMove( ent, svgame.globals->frametime, NULL, SV_ContentsMask( ent ));
				SV_LinkEdict( ent );
				ent->pvServerData->forceupdate = true;
			}
		}
		else
		{
			SV_AddGravity( ent );
			SV_CheckVelocity( ent );
			SV_FlyMove( ent, svgame.globals->frametime, NULL, SV_ContentsMask( ent ));
			SV_LinkEdict( ent );

			// just hit ground
			ent->pvServerData->forceupdate = true;
		}
	}

	// regular thinking
	if(!SV_RunThink( ent )) return;

	if( ent->pvServerData->forceupdate || !VectorCompare( ent->v.origin, ent->pvServerData->water_origin))
	{
		ent->pvServerData->forceupdate = false;
		VectorCopy( ent->v.origin, ent->pvServerData->water_origin );
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

	VectorScale( ent->v.movedir, ent->v.speed, v );
	VectorScale( v, 0.1f, move );

	for( i = 0; i < sv_maxclients->integer; i++ )
	{
		player = EDICT_NUM(i) + 1;
		if( player->free ) continue;
		if( !player->v.groundentity ) continue;
		if( player->v.groundentity != ent )
			continue;

		// Look below player; make sure he's on a conveyor
		VectorCopy( player->v.origin, point );
		point[2] += 1;
		VectorCopy( point, end );
		end[2] -= 256;

		tr = SV_Trace( point, player->v.mins, player->v.maxs, end, MOVE_NORMAL, player, MASK_SOLID );
		// tr.ent HAS to be conveyor, but just in case we screwed something up:
		if( tr.ent == ent )
		{
			if( tr.plane.normal[2] > 0 )
			{
				v[2] = ent->v.speed * com.sqrt( 1.0 - tr.plane.normal[2]*tr.plane.normal[2]) / tr.plane.normal[2];
				if(DotProduct( ent->v.movedir, tr.plane.normal) > 0)
					v[2] = -v[2]; // then we're moving down
				move[2] = v[2] * 0.1f;
			}
			VectorAdd( player->v.origin, move, end );
			tr = SV_Trace( player->v.origin, player->v.mins, player->v.maxs, end, MOVE_NORMAL, player, player->pvServerData->clipmask );
			VectorCopy( tr.endpos, player->v.origin );
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
	if( SV_RunThink( ent ))
	{
		SV_CheckWater( ent );	
		VectorMA( ent->v.angles, svgame.globals->frametime, ent->v.avelocity, ent->v.angles );
		VectorMA( ent->v.origin, svgame.globals->frametime, ent->v.velocity, ent->v.origin );
	}
	SV_LinkEdict( ent );
}


/*
=============
SV_PhysicsNone

Non moving objects can only think
=============
*/
void SV_Physics_None( edict_t *ent )
{
	if( ent->v.nextthink > 0 && ent->v.nextthink <= (sv.time * 0.001f) + svgame.globals->frametime )
		SV_RunThink( ent );
}


//============================================================================

static void SV_Physics_Entity( edict_t *ent )
{
	switch( ent->v.movetype )
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
			if(!SV_CheckWater( ent ) && !( ent->v.flags & FL_WATERJUMP ))
				SV_AddGravity( ent );
			SV_CheckStuck( ent );
			SV_WalkMove( ent );
			SV_LinkEdict( ent );
		}
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_FLY:
		if( SV_RunThink( ent )) SV_Physics_Toss( ent );
		break;
	case MOVETYPE_CONVEYOR:
		SV_Physics_Conveyor( ent );
		break;
	default:
		svgame.dllFuncs.pfnFrame( ent );
		break;
	}
}

void SV_Physics_ClientEntity( edict_t *ent )
{
	sv_client_t *client = ent->pvServerData->client;

	if( !client ) return;//Host_Error( "SV_Physics_ClientEntity: tired to apply physic to a non-client entity\n" );

	// don't do physics on disconnected clients, FrikBot relies on this
	if( client->state != cs_spawned )
	{
		memset( &client->lastcmd, 0, sizeof( client->lastcmd ));
		return;
	}

	// don't run physics here if running asynchronously
	if( client->skipframes <= 0 ) SV_ClientThink( client, &client->lastcmd );

	// make sure the velocity is sane (not a NaN)
	SV_CheckVelocity( ent );
	if( DotProduct( ent->v.velocity, ent->v.velocity ) < 0.0001f )
		VectorClear( ent->v.velocity );

	// call standard client pre-think
	svgame.globals->time = sv.time * 0.001f;
	svgame.dllFuncs.pfnPlayerPreThink( ent );
	SV_CheckVelocity( ent );

	switch( ent->v.movetype )
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
		VectorMA( ent->v.origin, svgame.globals->frametime, ent->v.velocity, ent->v.origin );
		VectorMA( ent->v.angles, svgame.globals->frametime, ent->v.avelocity, ent->v.angles );
		break;
	case MOVETYPE_STEP:
		SV_Physics_Step( ent );
		break;
	case MOVETYPE_WALK:
		SV_RunThink( ent );
		// don't run physics here if running asynchronously
		if( client->skipframes <= 0 )
		{
			if(!SV_CheckWater( ent ) && !( ent->v.flags & FL_WATERJUMP) )
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
		MsgDev( D_ERROR, "SV_Physics_ClientEntity: bad movetype %i\n", ent->v.movetype );
		break;
	}

	// decrement the countdown variable used to decide when to go back to synchronous physics
	if( client->skipframes > 0 ) client->skipframes--;

	SV_CheckVelocity( ent );
	SV_LinkEdict( ent );
	SV_CheckVelocity( ent );

	if( ent->v.movetype != MOVETYPE_NOCLIP )
		SV_TouchTriggers( ent );

	// call standard player post-think
	svgame.globals->time = sv.time * 0.001f;
	svgame.dllFuncs.pfnPlayerPostThink( ent );
}

void SV_Physics_ClientMove( sv_client_t *client, usercmd_t *cmd )
{
	edict_t	*ent = client->edict;

	// call player physics, this needs the proper frametime
	svgame.globals->frametime = sv.frametime * 0.001f;
	SV_ClientThink( client, cmd );

	// call standard client pre-think, with frametime = 0
	svgame.globals->time = cmd->servertime * 0.001f;
	svgame.globals->frametime = 0;
	svgame.dllFuncs.pfnPlayerPreThink( ent );
	svgame.globals->frametime = cmd->msec * 0.001f;

	if( !sv_physics->integer )
	{
		// make sure the velocity is sane (not a NaN)
		SV_CheckVelocity( ent );
		if( DotProduct(ent->v.velocity, ent->v.velocity) < 0.0001 )
			VectorClear( ent->v.velocity );
                   		
		switch( ent->v.movetype )
		{
		case MOVETYPE_WALK:
			// perform MOVETYPE_WALK behavior
			if(!SV_CheckWater( ent ) && !( ent->v.flags & FL_WATERJUMP ))
				SV_AddGravity( ent );
			SV_CheckStuck( ent );
			SV_WalkMove( ent );
			break;
		case MOVETYPE_NOCLIP:
			SV_CheckWater( ent );
			VectorMA( ent->v.origin, svgame.globals->frametime, ent->v.velocity, ent->v.origin );
			VectorMA( ent->v.angles, svgame.globals->frametime, ent->v.avelocity, ent->v.angles );
			break;
		}
		SV_CheckVelocity( ent );
		SV_LinkEdict( ent );
		SV_CheckVelocity( ent );
          }
          else SV_LinkEdict( ent );

	if( ent->v.movetype != MOVETYPE_NOCLIP )
		SV_TouchTriggers( ent );

	// call standard player post-think, with frametime = 0
	svgame.globals->time = sv.time * 0.001f;
	svgame.globals->frametime = 0;
	svgame.dllFuncs.pfnPlayerPostThink( ent );
	svgame.globals->frametime = sv.frametime * 0.001f;
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
	svgame.globals->time = sv.time * 0.001f;
	svgame.globals->frametime = sv.frametime * 0.001f;
	svgame.dllFuncs.pfnStartFrame();

	// treat each object in turn
	for( i = 1; i < svgame.globals->numEntities; i++ )
	{
		ent = EDICT_NUM( i );
		if( ent->free ) continue;

		if( ent->pvServerData->s.ed_type != ED_PORTAL )
			VectorCopy( ent->v.origin, ent->v.oldorigin );
		if( i <= sv_maxclients->integer );// SV_Physics_ClientEntity( ent );
		else if( !sv_playersonly->integer ) SV_Physics_Entity( ent );
	}

	// let everything in the world think and move
	pe->Frame( svgame.globals->frametime );

	svgame.globals->time = sv.time * 0.001f;

	// at end of frame kill all entities which supposed to it 
	for( i = svgame.globals->maxClients + 1; i < svgame.globals->numEntities; i++ )
	{
		ent = EDICT_NUM( i );
		if( ent->free ) continue;

		if( ent->v.flags & FL_KILLME )
			SV_FreeEdict( EDICT_NUM( i ));
	}

	svgame.dllFuncs.pfnEndFrame();

	// decrement svgame.globals->numEntities if the highest number entities died
	for( ; EDICT_NUM( svgame.globals->numEntities - 1)->free; svgame.globals->numEntities-- );

	if( !sv_playersonly->integer ) sv.time += sv.frametime;
}