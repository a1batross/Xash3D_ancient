//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_physics.c - server physic
//=======================================================================

#include "common.h"
#include "server.h"
#include "matrix_lib.h"
#include "const.h"

/*

pushmove objects do not obey gravity, and do not interact with each other or trigger fields,
but block normal movement and push normal objects when they move.

onground is set for toss objects when they come to a complete rest.  it is set for steping or walking objects

doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
corpses are SOLID_NOT and MOVETYPE_TOSS
crates are SOLID_BBOX and MOVETYPE_TOSS
walking monsters are SOLID_BBOX and MOVETYPE_STEP
flying/floating monsters are SOLID_BBOX and MOVETYPE_FLY

solid_edge items only clip against bsp models.
*/
#define DIST_EPSILON	(0.03125)		// 1/32 epsilon to keep floating point happy
#define MOVE_EPSILON	0.01
#define MAX_CLIP_PLANES	32

typedef struct
{
	edict_t	*ent;
	vec3_t	origin;
	vec3_t	angles;
} sv_pushed_t;

sv_pushed_t	sv_pushed[MAX_EDICTS];
sv_pushed_t	*sv_pushlist;

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
static bool SV_TestEntityPosition( edict_t *ent, const vec3_t offset )
{
	vec3_t	org;
	trace_t	trace;

	VectorAdd( ent->v.origin, offset, org );
	trace = SV_Move( org, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NOMONSTERS, ent );
	if( trace.fStartSolid ) return true;

	// if the trace found a better position for the entity, move it there
	if( VectorDistance2( trace.vecEndPos, ent->v.origin ) >= 0.0001f )
		VectorCopy( trace.vecEndPos, ent->v.origin );
	return false;
}

/*
================
SV_CheckAllEnts
================
*/
void SV_CheckAllEnts( void )
{
	int	i;
	edict_t	*e;

	if( !sv_check_errors->integer ) return;

	// see if any solid entities are inside the final position
	for( i = svgame.globals->maxClients + 1; i < svgame.globals->numEntities; i++ )
	{
		e = EDICT_NUM( i );
		if( e->free ) continue;

		switch( e->v.movetype )
		{
		case MOVETYPE_PUSH:
		case MOVETYPE_NONE:
		case MOVETYPE_FOLLOW:
		case MOVETYPE_NOCLIP:
			continue;
		default: break;
		}

		if( SV_TestEntityPosition( e, vec3_origin ))
			MsgDev( D_ERROR, "SV_CheckEntity: %s in invalid position\n", SV_ClassName( e ));
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
	float	maxvel;

	maxvel = fabs( svgame.movevars.maxvelocity );

	// bound velocity
	for( i = 0; i < 3; i++ )
	{
		if( IS_NAN( ent->v.velocity[i] ))
		{
			MsgDev( D_INFO, "Got a NaN velocity on %s\n", STRING( ent->v.classname ));
			ent->v.velocity[i] = 0.0f;
		}
		if( IS_NAN( ent->v.origin[i] ))
		{
			MsgDev( D_INFO, "Got a NaN origin on %s\n", STRING( ent->v.classname ));
			ent->v.origin[i] = 0.0f;
		}
	}

	if( VectorLength( ent->v.velocity ) > maxvel )
		VectorScale( ent->v.velocity, maxvel / VectorLength( ent->v.velocity ), ent->v.velocity );
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

	// don't let things stay in the past.
	// it is possible to start that way
	// by a trigger with a local time.
	if( thinktime < (sv.time * 0.001f))
		thinktime = (sv.time * 0.001f);

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
bool SV_Impact( edict_t *e1, trace_t *trace )
{
	edict_t	*e2 = trace->pHit;
	vec3_t	org;

	SV_CopyTraceToGlobal( trace );
	VectorCopy( e1->v.origin, org );
	svgame.globals->time = (sv.time * 0.001f);

	if( !e1->free && !e2->free && e1->v.solid != SOLID_NOT )
	{
		svgame.dllFuncs.pfnTouch( e1, e2 );
          }

	if( !e1->free && !e2->free && e2->v.solid != SOLID_NOT )
	{
		VectorCopy( e2->v.origin, svgame.globals->trace_endpos );
		VectorNegate( trace->vecPlaneNormal, svgame.globals->trace_plane_normal );
		svgame.globals->trace_plane_dist = -trace->flPlaneDist;
		svgame.globals->trace_ent = e1;
		svgame.dllFuncs.pfnTouch( e2, e1 );
	}
	return VectorCompare( e1->v.origin, org );
}

/*
============
SV_ClampOrigin

clamp the move to 1/32 units, so the position will
be accurate for client side prediction
============
*/
void SV_ClampOrigin( vec3_t origin )
{
	origin[0] -= 32.0f * floor( origin[0] * (1.0f / 32.0f));
	origin[1] -= 32.0f * floor( origin[1] * (1.0f / 32.0f));
	origin[2] -= 32.0f * floor( origin[2] * (1.0f / 32.0f));
}

/*
============
SV_ClampAngles

clamp the angles to 1/360 degrees, so the position will
be accurate for client side prediction
============
*/
void SV_ClampAngles( vec3_t angles )
{
	angles[0] -= 360.0f * floor( angles[0] * ( 1.0f / 360.0f ));
	angles[1] -= 360.0f * floor( angles[1] * ( 1.0f / 360.0f ));
	angles[2] -= 360.0f * floor( angles[2] * ( 1.0f / 360.0f ));
}

/*
==================
SV_ClipVelocity

Slide off of the impacting object
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
			out[i] = 0.0f;
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
if stepnormal is not NULL, the plane normal of any vertical wall hit will be stored
============
*/
int SV_FlyMove( edict_t *ent, float time, trace_t *steptrace )
{
	int		i, j, numplanes, bumpcount, blocked;
	vec3_t		dir, end, planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity, original_velocity, new_velocity;
	float		d, time_left;
	trace_t		trace;

	blocked = 0;
	VectorCopy( ent->v.velocity, original_velocity );
	VectorCopy( ent->v.velocity, primal_velocity );
	numplanes = 0;

	time_left = time;

	for( bumpcount = 0; bumpcount < MAX_CLIP_PLANES - 1; bumpcount++ )
	{
		if( VectorIsNull( ent->v.velocity ))
			break;

		VectorMA( ent->v.origin, time_left, ent->v.velocity, end );
		trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent );

		if( trace.fAllSolid )
		{	
			// entity is trapped in another solid
			VectorClear( ent->v.velocity );
			return 3;
		}

		if( trace.flFraction > 0.0f )
		{	
			// actually covered some distance
			VectorCopy( trace.vecEndPos, ent->v.origin );
			VectorCopy( ent->v.velocity, original_velocity );
			numplanes = 0;
		}

		if( trace.flFraction == 1.0f )
			 break; // moved the entire distance

		if( !trace.pHit )
		{
			MsgDev( D_ERROR, "SV_FlyMove: trace.pHit == NULL\n" );
			trace.pHit = EDICT_NUM( 0 ); // world
		}

		if( trace.vecPlaneNormal[2] > 0.7f )
		{
			blocked |= 1; // floor
			if( trace.pHit->v.solid == SOLID_BSP )
			{
				ent->v.flags |= FL_ONGROUND;
				ent->v.groundentity = trace.pHit;
			}
		}

		if( trace.vecPlaneNormal[2] == 0.0f )
		{
			blocked |= 2; // step
			if( steptrace ) *steptrace = trace; // save for player extrafriction
		}

		// run the impact function
		SV_Impact( ent, &trace );

		// break if removed by the impact function
		if( ent->free ) break;

		time_left -= time_left - trace.flFraction;

		// clipped to another plane
		if( numplanes >= MAX_CLIP_PLANES )
		{
			// this shouldn't really happen
			VectorClear( ent->v.velocity );
			return 3;
		}

		VectorCopy( trace.vecPlaneNormal, planes[numplanes] );
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for( i = 0; i < numplanes; i++ )
		{
			SV_ClipVelocity( original_velocity, planes[i], new_velocity, 1.0f );
			for( j = 0; j < numplanes; j++ )
			{
				if( j != i )
				{
					if( DotProduct( new_velocity, planes[j] ) < 0.0f )
						break; // not ok
				}
			}
			if( j == numplanes ) break;
		}

		if( i != numplanes )
		{
			// go along this plane
			VectorCopy( new_velocity, ent->v.velocity );
		}
		else
		{
			// go along the crease
			if( numplanes != 2 )
			{
				VectorClear( ent->v.velocity );
				return 7;
			}

			CrossProduct( planes[0], planes[1], dir );
			d = DotProduct( dir, ent->v.velocity );
			VectorScale( dir, d, ent->v.velocity );
		}

		// if current velocity is against the original velocity,
		// stop dead to avoid tiny occilations in sloping corners
		if( DotProduct( ent->v.velocity, primal_velocity ) <= 0.0f )
		{
			VectorClear( ent->v.velocity );
			return blocked;
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
static trace_t SV_PushEntity( edict_t *ent, vec3_t push )
{
	trace_t	trace;
	vec3_t	start, end;

	VectorCopy( ent->v.origin, start );
	VectorAdd( start, push, end );
retry:
	if( ent->v.movetype == MOVETYPE_FLYMISSILE )
		trace = SV_Move( start, ent->v.mins, ent->v.maxs, end, MOVE_MISSILE, ent );
	else if( ent->v.solid == SOLID_TRIGGER || ent->v.solid == SOLID_NOT )
		trace = SV_Move( start, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);	// only clip against bmodels
	else trace = SV_Move( start, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);

	VectorCopy( trace.vecEndPos, ent->v.origin );
	SV_LinkEdict( ent, false );

	if( trace.flFraction != 1.0f )
	{
		SV_Impact( ent, &trace );

		// if the pushed entity went away and the pusher is still there
		if( trace.pHit->free && !ent->free )
		{
			// move the pusher back and try again
			VectorCopy( start, ent->v.origin );
			SV_LinkEdict( ent, false );
			goto retry;
		}
	}
	if( !ent->free ) SV_TouchLinks( ent, sv_areanodes );

	return trace;
}

/*
============
SV_PushAngularMove

============
*/
bool SV_PushAngularMove( edict_t *pusher, vec3_t move, vec3_t amove )
{
	edict_t		*check;
	vec3_t		mins, maxs;
	int		oldsolid;
	int		i, e, block;
	vec3_t		org, org2, move2;
	vec3_t		forward, right, up;
	sv_pushed_t	*p;

	// init pushlist
	sv_pushlist = sv_pushed;

	// find the bounding box
	for( i = 0; i < 3; i++ )
	{
		mins[i] = pusher->v.absmin[i] + move[i];
		maxs[i] = pusher->v.absmax[i] + move[i];
	}

	// we need this for pushing things later
	VectorNegate( amove, org );
	AngleVectors( org, forward, right, up );

	// save the pusher's original position
	sv_pushlist->ent = pusher;
	VectorCopy( pusher->v.origin, sv_pushlist->origin );
	VectorCopy( pusher->v.angles, sv_pushlist->angles );
	sv_pushlist++;

	// move the pusher to it's final position
	VectorAdd( pusher->v.origin, move, pusher->v.origin );
	VectorAdd( pusher->v.angles, amove, pusher->v.angles );
	SV_LinkEdict( pusher, false );

	// see if any solid entities are inside the final position
	for( e = 1; e < svgame.globals->numEntities; e++ )
	{
		check = EDICT_NUM( e );

		if( check->free ) continue;

		switch( check->v.movetype )
		{
		case MOVETYPE_PUSH:
		case MOVETYPE_NONE:
		case MOVETYPE_NOCLIP:
			continue;
		}

		oldsolid = pusher->v.solid;
		pusher->v.solid = SOLID_NOT;
		block = SV_TestEntityPosition( check, vec3_origin );
		pusher->v.solid = oldsolid;
		if( block ) continue;

		// if the entity is standing on the pusher, it will definitely be moved
		if(!( check->v.flags & FL_ONGROUND && check->v.groundentity == pusher ))
		{
			// see if the ent needs to be tested
			if( !BoundsIntersect( check->v.absmin, check->v.absmax, mins, maxs ))
				continue;
		}

		if(( pusher->v.movetype == MOVETYPE_PUSH ) || ( check->v.groundentity == pusher ))
		{
			// move this entity
			sv_pushlist->ent = check;
			VectorCopy (check->v.origin, sv_pushlist->origin);
			VectorCopy (check->v.angles, sv_pushlist->angles);
			sv_pushlist++;

			// try moving the contacted entity
			VectorAdd( check->v.origin, move, check->v.origin );
			VectorAdd( check->v.angles, amove, check->v.angles );

			// figure movement due to the pusher's amove
			VectorSubtract( check->v.origin, pusher->v.origin, org );
			org2[0] = DotProduct( org, forward );
			org2[1] = -DotProduct( org, right );
			org2[2] = DotProduct( org, up );
			VectorSubtract( org2, org, move2 );
			VectorAdd( check->v.origin, move2, check->v.origin );

			check->v.flags &= ~FL_ONGROUND;

			// may have pushed them off an edge
			if( check->v.groundentity != pusher )
				check->v.groundentity = NULL;

			block = SV_TestEntityPosition( check, vec3_origin );
			if( !block )
			{	
				// pushed ok
				SV_LinkEdict( check, false );
				continue;
			}

			// if it is ok to leave in the old position, do it
			// this is only relevent for riding entities, not pushed
			// FIXME: this doesn't acount for rotation

			VectorSubtract( check->v.origin, move, check->v.origin );
			block = SV_TestEntityPosition( check, vec3_origin );

			if( !block )
			{
				sv_pushlist--;
				continue;
			}
		}

		// if it is sitting on top. Do not block.
		if( check->v.mins[0] == check->v.maxs[0] )
		{
			SV_LinkEdict( check, false );
			continue;
		}

		svgame.dllFuncs.pfnBlocked( pusher, check );

		// move back any entities we already moved
		// go backwards, so if the same entity was pushed
		// twice, it goes back to the original position
		for( p = sv_pushlist - 1; p >= sv_pushed; p-- )
		{
			VectorCopy( p->origin, p->ent->v.origin );
			VectorCopy( p->angles, p->ent->v.angles );
			SV_LinkEdict( p->ent, false );
		}
		return false;
	}

	// see if anything we moved has touched a trigger
	for( p = sv_pushlist - 1; p >= sv_pushed; p-- )
		SV_TouchLinks( p->ent, sv_areanodes );

	return true;
}

/*
============
SV_PushLinearMove

============
*/
bool SV_PushLinearMove( edict_t *pusher, vec3_t move )
{
	edict_t		*check;
	vec3_t		mins, maxs;
	vec3_t		pushorig;
	int		i, e, block;
	int		oldsolid;
	sv_pushed_t	*p;

	// init pushlist
	sv_pushlist = sv_pushed;

	for( i = 0; i < 3; i++ )
	{
		mins[i] = pusher->v.absmin[i] + move[i];
		maxs[i] = pusher->v.absmax[i] + move[i];
	}

	VectorCopy( pusher->v.origin, pushorig );

	// move the pusher to it's final position
	VectorAdd( pusher->v.origin, move, pusher->v.origin );
	SV_LinkEdict( pusher, false );

	// see if any solid entities are inside the final position
	for( e = 1; e < svgame.globals->numEntities; e++ )
	{
		check = EDICT_NUM( e );
		if( check->free ) continue;

		switch( check->v.movetype )
		{
		case MOVETYPE_PUSH:
		case MOVETYPE_NONE:
		case MOVETYPE_NOCLIP:
		case MOVETYPE_FOLLOW:
			continue;
		}

		oldsolid = pusher->v.solid;
		pusher->v.solid = SOLID_NOT;
		block = SV_TestEntityPosition( check, vec3_origin );
		pusher->v.solid = oldsolid;
		if( block ) continue;

		// if the entity is standing on the pusher, it will definately be moved
		if(!( check->v.flags & FL_ONGROUND && check->v.groundentity == pusher ))
		{
			if( !BoundsIntersect( check->v.absmin, check->v.absmax, mins, maxs ))
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if( !SV_TestEntityPosition( check, vec3_origin ))
				continue;
		}

		sv_pushlist->ent = check;
		VectorCopy( check->v.origin, sv_pushlist->origin );
		sv_pushlist++;

		// try moving the contacted entity
		VectorAdd( check->v.origin, move, check->v.origin );
		block = SV_TestEntityPosition( check, vec3_origin );

		if( !block )
		{	
			// pushed ok
			SV_LinkEdict( check, false );
			continue;
		}

		// if it is ok to leave in the old position, do it
		VectorSubtract( check->v.origin, move, check->v.origin );
		block = SV_TestEntityPosition( check, vec3_origin );

		if( !block )
		{
			// if leaving it where it was, allow it to drop to the floor again
			// (useful for plats that move downward)
			check->v.flags &= ~FL_ONGROUND;

			sv_pushlist--;
			continue;
		}

		// if it is still inside the pusher, block
		if( check->v.mins[0] == check->v.maxs[0] )
		{
			SV_LinkEdict( check, false );
			continue;
		}

		if( check->v.solid == SOLID_NOT || check->v.solid == SOLID_TRIGGER )
		{	
			// corpse
			check->v.mins[0] = check->v.mins[1] = 0;
			VectorCopy( check->v.mins, check->v.maxs );
			SV_LinkEdict( check, false );
			continue;
		}

		VectorCopy( pushorig, pusher->v.origin );
		SV_LinkEdict( pusher, false );

		// if the pusher has a "blocked" function, call it
		// otherwise, just stay in place until the obstacle is gone
		svgame.dllFuncs.pfnBlocked( pusher, check );

		// move back any entities we already moved
		for( p = sv_pushlist - 1; p >= sv_pushed; p-- )
		{
			VectorCopy( p->origin, p->ent->v.origin );
			VectorCopy( p->angles, p->ent->v.angles );
			SV_LinkEdict( p->ent, false );
		}
		return false;
	}
	return true;
}

/*
============
SV_PushMove

============
*/
void SV_PushMove( edict_t *pusher, float movetime )
{
	int	i, result;
	vec3_t	lmove;
	vec3_t	amove;

	if( VectorIsNull( pusher->v.velocity ) && VectorIsNull( pusher->v.avelocity ))
	{
		// advance movetime
		pusher->v.ltime += movetime;
		return;
	}

	for( i = 0; i < 3; i++ )
	{
		lmove[i] = pusher->v.velocity[i] * movetime;
		amove[i] = pusher->v.avelocity[i] * movetime;
	}

	if( VectorIsNull( amove ))
		result = SV_PushLinearMove( pusher, lmove );
	else result = SV_PushAngularMove( pusher, lmove, amove );

	if( result ) pusher->v.ltime += movetime;
}

/*
================
SV_Physics_Pusher

================
*/
void SV_Physics_Pusher( edict_t *ent )
{
	float	thinktime, oldltime;
	vec3_t	oldorg, lmove;
	vec3_t	oldang, amove;
	float	l, movetime;

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
		VectorCopy( ent->v.origin, oldorg );
		VectorCopy( ent->v.angles, oldang );

		ent->v.nextthink = 0;
		svgame.globals->time = (sv.time * 0.001f);
		svgame.dllFuncs.pfnThink( ent );
		if( ent->free ) return;

		VectorSubtract( ent->v.origin, oldorg, lmove );
		VectorSubtract( ent->v.angles, oldang, amove );

		l = VectorLength( lmove ) + VectorLength( amove );
		if( l > (1.0 / 64 ))
		{
			VectorCopy( oldorg, ent->v.origin );

			if( VectorIsNull( amove ))
				SV_PushLinearMove( ent, lmove );
			else SV_PushAngularMove( ent, lmove, amove );
		}
	}
	else if( ent->v.flags & FL_ALWAYSTHINK )
	{
		ent->v.nextthink = 0;
		svgame.globals->time = (sv.time * 0.001f);
		svgame.dllFuncs.pfnThink( ent );
		if( ent->free ) return;
	}
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
	if( !SV_RunThink( ent ))
		return;

	e = ent->v.aiment;
	if( !e || e->free ) return;

	if( VectorCompare( e->v.angles, ent->v.punchangle ))
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
	SV_LinkEdict( ent, true );
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
	if( !SV_RunThink( ent )) return;

	SV_CheckWater( ent );	
	VectorMA( ent->v.angles, svgame.globals->frametime, ent->v.avelocity, ent->v.angles );
	VectorMA( ent->v.origin, svgame.globals->frametime, ent->v.velocity, ent->v.origin );

	SV_LinkEdict( ent, false );	// nocip ents never touch triggers
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
	if( ent->v.watertype <= CONTENTS_WATER ) 
	{
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
	}
	else
	{
		ent->v.watertype = CONTENTS_EMPTY;
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
	float	backoff;

	// check for conveyor
	if( ent->v.groundentity && ent->v.groundentity->v.flags & FL_CONVEYOR )
		VectorScale( ent->v.groundentity->v.movedir, ent->v.groundentity->v.speed, ent->v.basevelocity );
	else VectorClear( ent->v.basevelocity );

	SV_CheckVelocity( ent );
	SV_CheckWater( ent );

	// regular thinking
	if( !SV_RunThink( ent )) return;

	// if onground, return without moving
	if( ent->v.flags & FL_ONGROUND )
		return;

	SV_CheckVelocity( ent );

	// add gravity
	switch( ent->v.movetype )
	{
	case MOVETYPE_FLY:
	case MOVETYPE_FLYMISSILE:
	case MOVETYPE_BOUNCEMISSILE:
		break;
	default:
		SV_AddGravity( ent );
		break;
	}

	// move angles
	VectorMA( ent->v.angles, svgame.globals->frametime, ent->v.avelocity, ent->v.angles );

	// move origin
	VectorAdd( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );
	VectorScale( ent->v.velocity, svgame.globals->frametime, move );

	trace = SV_PushEntity( ent, move );
	if( ent->free ) return;
	VectorSubtract( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );

	if( trace.flFraction < 1.0f )
	{
		if( ent->v.movetype == MOVETYPE_BOUNCE )
			backoff = 1.5f;
		else if( ent->v.movetype == MOVETYPE_BOUNCEMISSILE )
			backoff = 2.0f;
		else backoff = 1.0f;

		SV_ClipVelocity( ent->v.velocity, trace.vecPlaneNormal, ent->v.velocity, backoff );

		// stop if on ground
		if( trace.vecPlaneNormal[2] > 0.7f && ( ent->v.movetype != MOVETYPE_BOUNCEMISSILE ))
		{		
			if( ent->v.velocity[2] < 60.0f || ent->v.movetype != MOVETYPE_BOUNCE )
			{
				ent->v.flags |= FL_ONGROUND;
				ent->v.groundentity = trace.pHit;
				VectorClear( ent->v.velocity );
				VectorClear( ent->v.avelocity );
			}
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
void SV_AddRotationalFriction( edict_t *ent )
{
	int	i;
	float	adjustment;

	VectorMA( ent->v.angles, svgame.globals->frametime, ent->v.avelocity, ent->v.angles );
	adjustment = svgame.globals->frametime * sv_stopspeed->value * sv_friction->value;

	for( i = 0; i < 3; i++ )
	{
		if( ent->v.avelocity[i] > 0.0f )
		{
			ent->v.avelocity[i] -= adjustment;
			if( ent->v.avelocity[i] < 0.0f )
				ent->v.avelocity[i] = 0.0f;
		}
		else
		{
			ent->v.avelocity[i] += adjustment;
			if( ent->v.avelocity[i] > 0.0f )
				ent->v.avelocity[i] = 0.0f;
		}
	}
}

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
	bool		wasonground;
	bool		inwater;
	float		*vel;
	float		speed, newspeed, control;
	float		friction;

	// check for conveyor
	if( ent->v.groundentity && ent->v.groundentity->v.flags & FL_CONVEYOR )
		VectorScale( ent->v.groundentity->v.movedir, ent->v.groundentity->v.speed, ent->v.basevelocity );
	else VectorClear( ent->v.basevelocity );

	SV_CheckVelocity( ent );

	wasonground = ent->v.flags & FL_ONGROUND;

	if( !VectorIsNull( ent->v.avelocity ))
		SV_AddRotationalFriction( ent );

	// add gravity except:
	// flying monsters
	// swimming monsters who are in the water
	inwater = SV_CheckWater( ent );

	if( !wasonground )
	{
		if( !( ent->v.flags & FL_FLY ))
		{
			if(!( ent->v.flags & FL_SWIM && ent->v.waterlevel > 0 ))
				if( !inwater ) SV_AddGravity( ent );
		}
	}

	if( !VectorIsNull( ent->v.velocity ) || !VectorIsNull( ent->v.basevelocity ))
	{
		vec3_t	mins, maxs, point;
		int	x, y;

		ent->v.flags &= ~FL_ONGROUND;

		// apply friction
		// let dead monsters who aren't completely onground slide
		if( wasonground )
		{
			if( !( ent->v.health <= 0.0f && !SV_CheckBottom( ent, WALKMOVE_NORMAL )))
			{
				vel = ent->v.velocity;
				speed = com.sqrt( vel[0] * vel[0] + vel[1] * vel[1] );

				if( speed )
				{
					friction = sv_friction->value;

					control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
					newspeed = speed - svgame.globals->frametime * control * friction;

					if( newspeed < 0 ) newspeed = 0;
					newspeed /= speed;

					vel[0] = vel[0] * newspeed;
					vel[1] = vel[1] * newspeed;
				}
			}
                    }

		VectorAdd( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );
		SV_FlyMove( ent, svgame.globals->frametime, NULL );
		VectorSubtract (ent->v.velocity, ent->v.basevelocity, ent->v.velocity);

		// determine if it's on solid ground at all
		VectorAdd( ent->v.origin, ent->v.mins, mins );
		VectorAdd( ent->v.origin, ent->v.maxs, maxs );

		point[2] = mins[2] - 1;
		for( x = 0; x <= 1; x++ )
		{
			for( y = 0; y <= 1; y++ )
			{
				point[0] = x ? maxs[0] : mins[0];
				point[1] = y ? maxs[1] : mins[1];
			
				if( SV_PointContents( point ) == CONTENTS_SOLID )
				{
					ent->v.flags |= FL_ONGROUND;
					break;
				}
			}

		}
		SV_LinkEdict( ent, true );
	}

	// regular thinking
	SV_RunThink ( ent );
	SV_CheckWaterTransition( ent );
}

/*
=============
SV_CheckStuck

This is a big hack to try and fix the rare case of getting stuck in the world
clipping hull.
=============
*/
void SV_CheckStuck( edict_t *ent )
{
	int	x, y, z;
	vec3_t	org;

	if( !SV_TestEntityPosition( ent, vec3_origin ))
	{
		ent->pvServerData->stuck = false;
		VectorCopy( ent->v.origin, ent->v.oldorigin );
		return;
	}

	VectorCopy( ent->v.origin, org );
	VectorCopy( ent->v.oldorigin, ent->v.origin );

	if( !SV_TestEntityPosition( ent, vec3_origin ))
	{
		MsgDev( D_INFO, "Unstuck player.\n" );
		SV_LinkEdict( ent, true );
		return;
	}

	for( z = 0; z < sv_stepheight->value; z++ )
	{
		for( x = -1; x <= 1; x++ )
		{
			for( y = -1; y <= 1; y++ )
			{
				ent->v.origin[0] = org[0] + x;
				ent->v.origin[1] = org[1] + y;
				ent->v.origin[2] = org[2] + z;

				if( !SV_TestEntityPosition( ent, vec3_origin ))
				{
					MsgDev( D_INFO, "Unstuck player.\n" );
					ent->pvServerData->stuck = false;
					SV_LinkEdict( ent, true );
					return;
				}
			}
		}
	}

	VectorCopy( org, ent->v.origin );
	if( !ent->pvServerData->stuck )
		MsgDev( D_ERROR, "Stuck player\n" );	// fire once
	ent->pvServerData->stuck = true;
}

void SV_UnstickEntity( edict_t *ent )
{
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
	ent->v.watertype = CONTENTS_EMPTY;
	cont = SV_PointContents( point );

	if( cont <= CONTENTS_WATER )
	{
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
		point[2] = ent->v.origin[2] + (ent->v.mins[2] + ent->v.maxs[2]) * 0.5f;

		if( SV_PointContents( point ) <= CONTENTS_WATER )
		{
			ent->v.waterlevel = 2;
			point[2] = ent->v.origin[2] + ent->v.view_ofs[2];

			if(SV_PointContents( point ) <= CONTENTS_WATER )
				ent->v.waterlevel = 3;
		}
		if( cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN )
		{
			static vec3_t current_table[] =
			{
				{ 1,  0, 0 },
				{ 0,  1, 0 },
				{-1,  0, 0 },
				{ 0, -1, 0 },
				{ 0,  0, 1 },
				{ 0,  0, -1}
			};
			float	speed = 150.0f * ent->v.waterlevel / 3.0f;
			float	*dir = current_table[CONTENTS_CURRENT_0 - cont];
			VectorMA( ent->v.basevelocity, speed, dir, ent->v.basevelocity );
		}
	}
	return ent->v.waterlevel > 1;
}

/*
============
SV_WallFriction

============
*/
void SV_WallFriction( edict_t *ent, trace_t *trace )
{
	vec3_t	forward;
	vec3_t	into, side;
	float	d, i;

	AngleVectors( ent->v.viewangles, forward, NULL, NULL );
	d = DotProduct( trace->vecPlaneNormal, forward );

	d += 0.5f;
	if( d >= 0.0f || IS_NAN( d ))
		return;

	// cut the tangential velocity
	i = DotProduct( trace->vecPlaneNormal, ent->v.velocity );
	VectorScale( trace->vecPlaneNormal, i, into );
	VectorSubtract( ent->v.velocity, into, side );

	ent->v.velocity[0] = side[0] * (1 + d);
	ent->v.velocity[1] = side[1] * (1 + d);
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
int SV_TryUnstick( edict_t *ent, vec3_t oldvel )
{
	vec3_t	dir, oldorg;
	trace_t	steptrace;
	int	i, clip;

	VectorCopy( ent->v.origin, oldorg );
	VectorClear( dir );

	for( i = 0; i < 8; i++ )
	{
		// try pushing a little in an axial direction
		switch( i )
		{
			case 0: dir[0] =  2; dir[1] =  0; break;
			case 1: dir[0] =  0; dir[1] =  2; break;
			case 2: dir[0] = -2; dir[1] =  0; break;
			case 3: dir[0] =  0; dir[1] = -2; break;
			case 4: dir[0] =  2; dir[1] =  2; break;
			case 5: dir[0] = -2; dir[1] =  2; break;
			case 6: dir[0] =  2; dir[1] = -2; break;
			case 7: dir[0] = -2; dir[1] = -2; break;
		}

		SV_PushEntity( ent, dir );

		// retry the original move
		ent->v.velocity[0] = oldvel[0];
		ent->v.velocity[1] = oldvel[1];
		ent->v.velocity[2] = 0.0f;
		clip = SV_FlyMove( ent, 0.1f, &steptrace );

		if( fabs( oldorg[1] - ent->v.origin[1] ) > 4 || fabs( oldorg[0] - ent->v.origin[0] ) > 4 )
		{
			MsgDev( D_INFO, "Unstuck %s.\n", STRING( ent->v.classname ));
			ent->pvServerData->stuck = false;
			return clip;
		}

		// go back to the original pos and try again
		VectorCopy( oldorg, ent->v.origin );
	}

	VectorClear( ent->v.velocity );
	if( !ent->pvServerData->stuck )
		MsgDev( D_ERROR, "Stuck %s.\n", STRING( ent->v.classname )); // fire once
	ent->pvServerData->stuck = true;

	return 7;	// still not moving
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
	VectorScale( v, svgame.globals->frametime, move );

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

		tr = SV_Move( point, player->v.mins, player->v.maxs, end, MOVE_NORMAL, player );
		// tr.pHit HAS to be conveyor, but just in case we screwed something up:
		if( tr.pHit == ent )
		{
			if( tr.vecPlaneNormal[2] > 0 )
			{
				v[2] = ent->v.speed * com.sqrt( 1.0f - tr.vecPlaneNormal[2] * tr.vecPlaneNormal[2] ) / tr.vecPlaneNormal[2];
				if(DotProduct( ent->v.movedir, tr.vecPlaneNormal) > 0.0f )
					v[2] = -v[2]; // then we're moving down
				move[2] = v[2] * svgame.globals->frametime;
			}
			VectorAdd( player->v.origin, move, end );
			tr = SV_Move( player->v.origin, player->v.mins, player->v.maxs, end, MOVE_NORMAL, player );
			VectorCopy( tr.vecEndPos, player->v.origin );
			SV_LinkEdict( player, false );
		}
	}
}

/*
=============
SV_PhysicsNone

Non moving objects can only think
=============
*/
void SV_Physics_None( edict_t *ent )
{
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
		SV_Physics_None( ent );
		break;
	case MOVETYPE_FOLLOW:
		SV_Physics_Follow( ent );
		break;
	case MOVETYPE_NOCLIP:
		SV_Physics_Noclip( ent );
		break;
	case MOVETYPE_STEP:
	case MOVETYPE_PUSHSTEP:
		SV_Physics_Step( ent );
		break;
	case MOVETYPE_FLY:
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_FLYMISSILE:
	case MOVETYPE_BOUNCEMISSILE:
		SV_Physics_Toss( ent );
		break;
	case MOVETYPE_CONVEYOR:
		SV_Physics_Conveyor( ent );
		break;
	default:
		svgame.dllFuncs.pfnPhysicsEntity( ent );
		break;
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
	svgame.globals->time = sv.time * 0.001f;
	svgame.globals->frametime = sv.frametime * 0.001f;
	svgame.dllFuncs.pfnStartFrame();

	SV_CheckAllEnts ();

	// treat each object in turn
	for( i = 0; i < svgame.globals->numEntities; i++ )
	{
		ent = EDICT_NUM( i );
		if( ent->free ) continue;

		if( svgame.globals->force_retouch )
			SV_LinkEdict( ent, true ); // force retouch even for stationary

		if(!( ent->v.flags & FL_BASEVELOCITY ) && !VectorIsNull( ent->v.basevelocity ))
		{
			// Apply momentum (add in half of the previous frame of velocity first)
			VectorMA( ent->v.velocity, 1.0f + (svgame.globals->frametime * 0.5f), ent->v.basevelocity, ent->v.velocity );
			VectorClear( ent->v.basevelocity );
		}
		ent->v.flags &= ~FL_BASEVELOCITY;

		if( i > 0 && i <= sv_maxclients->integer )
			continue;	// clients are directly runs from packets
		else if( !sv_playersonly->integer )
			SV_Physics_Entity( ent );
	}

	// let everything in the world think and move
	CM_Frame( svgame.globals->frametime );

	svgame.globals->time = sv.time * 0.001f;

	// at end of frame kill all entities which supposed to it 
	for( i = svgame.globals->maxClients + 1; i < svgame.globals->numEntities; i++ )
	{
		ent = EDICT_NUM( i );
		if( ent->free ) continue;

		if( ent->v.flags & FL_KILLME )
			SV_FreeEdict( ent );
	}

	if( svgame.globals->force_retouch ) svgame.globals->force_retouch--;

	// decrement svgame.globals->numEntities if the highest number entities died
	for( ; EDICT_NUM( svgame.globals->numEntities - 1)->free; svgame.globals->numEntities-- );

	svgame.dllFuncs.pfnEndFrame();

	if( !sv_playersonly->integer ) sv.time += sv.frametime;
}