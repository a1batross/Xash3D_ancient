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
#define MAX_CLIP_PLANES	5

typedef struct
{
	edict_t	*ent;
	vec3_t	origin;
	vec3_t	angles;
} pushed_t;

pushed_t	sv_pushed[MAX_EDICTS];

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
bool SV_TestEntityPosition( edict_t *ent, const vec3_t offset )
{
	vec3_t	org;
	trace_t	trace;

	VectorAdd( ent->v.origin, offset, org );

	if( ent->v.flags & ( FL_CLIENT|FL_FAKECLIENT ))
	{
		// player can crouch, noclip etc
		if( SV_TestPlayerPosition( org, ent, NULL ))
			return true;
		return false;
	}

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

		if( !SV_IsValidEdict( e )) continue;

		switch( e->v.movetype )
		{
		case MOVETYPE_PUSH:
		case MOVETYPE_NONE:
		case MOVETYPE_FOLLOW:
		case MOVETYPE_NOCLIP:
			continue;
		default:
			break;
		}

		if( SV_TestEntityPosition( e, vec3_origin ))
			MsgDev( D_INFO, "Stuck entity %s\n", SV_ClassName( e ));
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
================
SV_UpdateBaseVelocity
================
*/
void SV_UpdateBaseVelocity( edict_t *ent )
{
	if( ent->v.flags & FL_ONGROUND )
	{
		edict_t	*groundentity = ent->v.groundentity;

		if( SV_IsValidEdict( groundentity ))
		{
			// On conveyor belt that's moving?
			if( groundentity->v.flags & FL_CONVEYOR )
			{
				vec3_t	new_basevel;

				VectorScale( groundentity->v.movedir, groundentity->v.speed, new_basevel );
				if( ent->v.flags & FL_BASEVELOCITY )
					VectorAdd( new_basevel, ent->v.basevelocity, new_basevel );

				ent->v.flags |= FL_BASEVELOCITY;
				VectorCopy( new_basevel, ent->v.basevelocity );
			}
		}
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
	if( thinktime <= 0.0f || thinktime > ( sv.time * 0.001f ) + ( sv.frametime * 0.001f ))
		return true;
		
	if( thinktime < ( sv.time * 0.001f ))
		thinktime = ( sv.time * 0.001f );	// don't let things stay in the past.
						// it is possible to start that way
						// by a trigger with a local time.
	ent->v.nextthink = 0;
	svgame.globals->time = thinktime;
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

	// custom user filter
	if( !svgame.dllFuncs.pfnShouldCollide( e1, e2 ))
		return false;

	SV_CopyTraceToGlobal( trace );
	VectorCopy( e1->v.origin, org );
	svgame.globals->time = (sv.time * 0.001f);

	if( !e1->free && !e2->free && e1->v.solid != SOLID_NOT )
	{
		svgame.dllFuncs.pfnTouch( e1, e2 );
          }

	if( !e1->free && !e2->free && e2->v.solid != SOLID_NOT )
	{
		// inverse plane and normal for second contacted edict
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
SV_TransformedBBox

Returns the actual bounding box of a bmodel. This is a big improvement over
what quake normally does with rotating bmodels - quake sets absmin, absmax to a cube
that will completely contain the bmodel at *any* rotation on *any* axis, whether
the bmodel can actually rotate to that angle or not. This leads to a lot of
false block tests in SV_MovePush if another bmodel is in the vicinity.
============
*/
void SV_TransformedBBox( edict_t *ent, vec3_t mins, vec3_t maxs )
{
	vec3_t	forward, left, up, f1, l1, u1;
	int	i, j, k, j2, k4;
	vec3_t	p[8];

	for( k = 0; k < 2; k++ )
	{
		k4 = k * 4;
		if( k ) p[k4][2] = ent->v.maxs[2];
		else p[k4][2] = ent->v.mins[2];

		p[k4+1][2] = p[k4][2];
		p[k4+2][2] = p[k4][2];
		p[k4+3][2] = p[k4][2];

		for( j = 0; j < 2; j++ )
		{
			j2 = j * 2;
			if( j ) p[j2+k4][1] = ent->v.maxs[1];
			else p[j2+k4][1] = ent->v.mins[1];
			p[j2+k4+1][1] = p[j2+k4][1];

			for( i = 0; i < 2; i++ )
			{
				if( i ) p[i+j2+k4][0] = ent->v.maxs[0];
				else p[i+j2+k4][0] = ent->v.mins[0];
			}
		}
	}

	AngleVectorsFLU( ent->v.angles, forward, left, up );

	for( i = 0; i < 8; i++ )
	{
		VectorScale( forward, p[i][0], f1 );
		VectorScale( left, -p[i][1], l1 );
		VectorScale( up, p[i][2], u1 );
		VectorAdd( ent->v.origin, f1, p[i] );
		VectorAdd( p[i], l1, p[i] );
		VectorAdd( p[i], u1, p[i] );
	}

	VectorCopy( p[0], mins );
	VectorCopy( p[0], maxs );

	for( i = 1; i < 8; i++ )
	{
		mins[0] = min( mins[0], p[i][0] );
		mins[1] = min( mins[1], p[i][1] );
		mins[2] = min( mins[2], p[i][2] );
		maxs[0] = max( maxs[0], p[i][0] );
		maxs[1] = max( maxs[1], p[i][1] );
		maxs[2] = max( maxs[2], p[i][2] );
	}
}

/*
=============
SV_AngularMove

may use friction for smooth stopping
=============
*/
void SV_AngularMove( edict_t *ent, float frametime, float friction )
{
	int	i;
	float	adjustment;

	VectorMA( ent->v.angles, frametime, ent->v.avelocity, ent->v.angles );
	if( friction == 0.0f ) return;
	adjustment = frametime * (sv_stopspeed->value / 10) * sv_friction->value * fabs( friction );

	for( i = 0; i < 3; i++ )
	{
		if( ent->v.avelocity[i] > 0.0f )
		{
			ent->v.avelocity[i] -= adjustment;
			if( ent->v.avelocity[i] < 0.0f ) ent->v.avelocity[i] = 0.0f;
		}
		else
		{
			ent->v.avelocity[i] += adjustment;
			if( ent->v.avelocity[i] > 0.0f ) ent->v.avelocity[i] = 0.0f;
		}
	}
}

/*
=============
SV_LinearMove

use friction for smooth stopping
=============
*/
void SV_LinearMove( edict_t *ent, float frametime, float friction )
{
	int	i;
	float	adjustment;

	VectorMA( ent->v.origin, frametime, ent->v.velocity, ent->v.origin );
	if( friction == 0.0f ) return;
	adjustment = frametime * (sv_stopspeed->value / 10) * sv_friction->value * fabs( friction );

	for( i = 0; i < 3; i++ )
	{
		if( ent->v.velocity[i] > 0.0f )
		{
			ent->v.velocity[i] -= adjustment;
			if( ent->v.velocity[i] < 0.0f ) ent->v.velocity[i] = 0.0f;
		}
		else
		{
			ent->v.velocity[i] += adjustment;
			if( ent->v.velocity[i] > 0.0f ) ent->v.velocity[i] = 0.0f;
		}
	}
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

	point[0] = (ent->v.absmin[0] + ent->v.absmax[0]) * 0.5f;
	point[1] = (ent->v.absmin[1] + ent->v.absmax[1]) * 0.5f;
	point[2] = ent->v.absmin[2] + 1;

	ent->v.waterlevel = 0;
	ent->v.watertype = CONTENTS_EMPTY;
	cont = SV_PointContents( point );

	if( cont <= CONTENTS_WATER )
	{
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
			
		// point sized entities are always fully submerged
		if( ent->v.absmin[2] == ent->v.absmax[2] )
		{
			ent->v.waterlevel = 3;
		}
		else
		{
			// check the exact center of the box
			point[2] = (ent->v.absmin[2] + ent->v.absmax[2]) * 0.5f;

			if( SV_PointContents( point ) <= CONTENTS_WATER )
			{
				ent->v.waterlevel = 2;
				point[2] = ent->v.origin[2] + ent->v.view_ofs[2];

				// now check where the eyes are...
				if( SV_PointContents( point ) <= CONTENTS_WATER )
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

				float speed = ent->v.waterlevel * 50.0f;
				float *dir = current_table[CONTENTS_CURRENT_0 - cont];

				VectorMA( ent->v.basevelocity, speed, dir, ent->v.basevelocity );
			}
		}
	}

	return ent->v.waterlevel > 1;
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
	origin[0] -= 32.0f * floor( origin[0] * ( 1.0f / 32.0f ));
	origin[1] -= 32.0f * floor( origin[1] * ( 1.0f / 32.0f ));
	origin[2] -= 32.0f * floor( origin[2] * ( 1.0f / 32.0f ));
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
===============================================================================

	FLYING MOVEMENT CODE

===============================================================================
*/
/*
============
SV_TryMove

The basic solid body movement clip that slides along multiple planes
*steptrace - if not NULL, the trace results of any vertical wall hit will be stored
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
============
*/
int SV_TryMove( edict_t *ent, float time, trace_t *steptrace )
{
	int		i, j, numplanes, bumpcount, blocked;
	vec3_t		dir, end, planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity, original_velocity, new_velocity;
	float		d, time_left, allFraction;
	trace_t		trace;

	blocked = 0;
	VectorCopy( ent->v.velocity, original_velocity );
	VectorCopy( ent->v.velocity, primal_velocity );
	numplanes = 0;

	allFraction = 0;
	time_left = time;

	for( bumpcount = 0; bumpcount < MAX_CLIP_PLANES - 1; bumpcount++ )
	{
		if( VectorIsNull( ent->v.velocity ))
			break;

		VectorMA( ent->v.origin, time_left, ent->v.velocity, end );
		trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent );

		allFraction += trace.flFraction;


		if( trace.fAllSolid )
		{	
			// entity is trapped in another solid
			VectorClear( ent->v.velocity );
			return 4;
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
			MsgDev( D_WARN, "SV_TryMove: trace.pHit == NULL\n" );
			return 4;
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

		time_left -= time_left * trace.flFraction;

		// clipped to another plane
		if( numplanes >= MAX_CLIP_PLANES )
		{
			// this shouldn't really happen
			VectorClear( ent->v.velocity );
			break;
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
			if( j == numplanes )
				break;
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
				break;
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
			break;
		}
	}

	if( allFraction == 0 )
		VectorClear( ent->v.velocity );

	return blocked;
}

/*
============
SV_AddGravity

============
*/
void SV_AddGravity( edict_t *ent )
{
	if( ent->v.gravity ) // gravity modifier
		ent->v.velocity[2] -= sv_gravity->value * ent->v.gravity * svgame.globals->frametime;
	else ent->v.velocity[2] -= sv_gravity->value * svgame.globals->frametime;
}

void SV_AddHalfGravity( edict_t *ent, float timestep )
{
	float	ent_gravity;

	if( ent->v.gravity )
		ent_gravity = ent->v.gravity;
	else ent_gravity = 1.0f;

	// Add 1/2 of the total gravitational effects over this timestep
	ent->v.velocity[2] -= ( 0.5f * ent_gravity * sv_gravity->value * timestep );
	ent->v.velocity[2] += ent->v.basevelocity[2] * svgame.globals->frametime;
	ent->v.basevelocity[2] = 0.0f;
	
	// bound velocity
	SV_CheckVelocity( ent );
}

/*
===============================================================================

PUSHMOVE

===============================================================================
*/
/*
============
PM_PushEntity

Does not change the entities velocity at all
============
*/
trace_t SV_PushEntity( edict_t *ent, const vec3_t lpush, const vec3_t apush, int *blocked )
{
	trace_t		trace;
	sv_client_t	*cl;
	int		type;
	vec3_t		end;

	VectorAdd( ent->v.origin, lpush, end );

	if( ent->v.movetype == MOVETYPE_FLYMISSILE )
		type = MOVE_MISSILE;
	else if( ent->v.solid == SOLID_TRIGGER || ent->v.solid == SOLID_NOT )
		type = MOVE_NOMONSTERS; // only clip against bmodels
	else type = MOVE_NORMAL;

	trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, end, type|FMOVE_SIMPLEBOX, ent );
	VectorCopy( trace.vecEndPos, ent->v.origin );
	SV_LinkEdict( ent, true );

	if( apush[YAW] && ent->v.flags & FL_CLIENT && ( cl = SV_ClientFromEdict( ent, true )) != NULL )
	{
		// Because we can run multiple ticks per server frame,
		// accumulate a total offset here instead of straight
		// setting it. The engine will reset anglechange to 0
		// when the message is actually sent to the client
		cl->anglechangetotal += apush[1];
		cl->anglechangefinal = apush[1];
		ent->v.fixangle = 2;
	}

	ent->v.angles[YAW] += trace.flFraction * apush[YAW];

	if( blocked ) *blocked = !VectorCompare( ent->v.origin, end ); // can't move full distance

	// so we can run impact function afterwards.
	if( trace.pHit ) SV_Impact( ent, &trace );

	return trace;
}

/*
============
SV_Push

Objects need to be moved back on a failed push,
otherwise riders would continue to slide.
============
*/
void SV_PushAngles( edict_t *pusher, float movetime )
{
	int	i, e, block;
	edict_t	*check;
	vec3_t	mins, maxs, move, amove;
	float	oldsolid;
	pushed_t	*p, *pushed_p;
	vec3_t	org, org2, move2, forward, right, up;

	if( VectorIsNull( pusher->v.velocity ) && VectorIsNull( pusher->v.avelocity ))
	{
		// advances ltime when stop too for thinking
		pusher->v.ltime += movetime;
		return;
	}

	for( i = 0; i < 3; i++ )
	{
		move[i] = pusher->v.velocity[i] * movetime;
		amove[i] = pusher->v.avelocity[i] * movetime;
		mins[i] = pusher->v.absmin[i] + move[i];
		maxs[i] = pusher->v.absmax[i] + move[i];
	}

	pushed_p = sv_pushed;

	// getting real bbox size
	SV_TransformedBBox( pusher, mins, maxs );

	// we need this for pushing things later
	VectorNegate( amove, org );
	AngleVectors( org, forward, right, up );

	// save the pusher's original position
	pushed_p->ent = pusher;
	VectorCopy( pusher->v.origin, pushed_p->origin );
	VectorCopy( pusher->v.angles, pushed_p->angles );
	pushed_p++;

	// move the pusher to it's final position
	VectorAdd( pusher->v.origin, move, pusher->v.origin );
	VectorAdd( pusher->v.angles, amove, pusher->v.angles );
	pusher->v.ltime += movetime;
	SV_LinkEdict( pusher, false );

	// see if any solid entities are inside the final position
	for( e = 1; e < svgame.globals->numEntities; e++ )
	{
		check = EDICT_NUM( e );
		if ( check->free ) continue;

		// filter movetypes to collide with
		switch( check->v.movetype )
		{
		case MOVETYPE_NONE:
		case MOVETYPE_PUSH:
		case MOVETYPE_FOLLOW:
		case MOVETYPE_NOCLIP:
		case MOVETYPE_COMPOUND:
			continue;
		default: break;
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
			if( check->v.absmin[0] >= maxs[0]
			 || check->v.absmin[1] >= maxs[1]
			 || check->v.absmin[2] >= maxs[2]
			 || check->v.absmax[0] <= mins[0]
			 || check->v.absmax[1] <= mins[1]
			 || check->v.absmax[2] <= mins[2] )
				continue;
		}

		if(( pusher->v.movetype == MOVETYPE_PUSH ) || ( check->v.groundentity == pusher ))
		{
			// move this entity
			pushed_p->ent = check;
			VectorCopy( check->v.origin, pushed_p->origin );
			VectorCopy( check->v.angles, pushed_p->angles );
			pushed_p++;

			// try moving the contacted entity
			VectorAdd( check->v.origin, move, check->v.origin );
			VectorAdd( check->v.angles, amove, check->v.angles);

			if( check->v.flags & FL_CLIENT )
			{
				sv_client_t	*cl;

				if(( cl = SV_ClientFromEdict( check, true )) != NULL )
				{
					// Because we can run multiple ticks per server frame,
					// accumulate a total offset here instead of straight
					// setting it. The engine will reset anglechange to 0
					// when the message is actually sent to the client
					cl->anglechangetotal += amove[1];
					cl->anglechangefinal = amove[1];
					check->v.fixangle = 2;
				}
			}

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
				check->v.groundentity = 0;

			block = SV_TestEntityPosition( check, vec3_origin );
			if( !block )
			{	
				// pushed ok
				SV_LinkEdict( check, false );
				// impact?
				continue;
			}

			// if it is ok to leave in the old position, do it
			// this is only relevent for riding entities, not pushed
			// FIXME: this doesn't acount for rotation
			VectorSubtract( check->v.origin, move, check->v.origin );
			block = SV_TestEntityPosition( check, vec3_origin );
			if( !block )
			{
				pushed_p--;
				continue;
			}
		}

		// if it is sitting on top. Do not block.
		if( check->v.mins[0] == check->v.maxs[0] )
		{
			SV_LinkEdict( check, false );
			continue;
		}

		svgame.globals->time = (sv.time * 0.001f);
		svgame.dllFuncs.pfnBlocked( pusher, check );

		// move back any entities we already moved
		// go backwards, so if the same entity was pushed
		// twice, it goes back to the original position
		for( p = pushed_p - 1; p >= sv_pushed; p-- )
		{
			VectorCopy( p->origin, p->ent->v.origin );
			VectorCopy( p->angles, p->ent->v.angles );
			SV_LinkEdict( p->ent, false );
		}
		return;
	}

	// FIXME: is there a better way to handle this?
	// see if anything we moved has touched a trigger
	for( p = pushed_p - 1; p >= sv_pushed; p-- )
		SV_TouchLinks( p->ent, sv_areanodes );
}

/*
============
SV_PushMove

============
*/
void SV_PushMove( edict_t *pusher, float movetime )
{
	int	i, e, oldsolid, num_moved;
	vec3_t	a, lmove, amove, org, org2, move2;
	vec3_t	forward, right, up, pushorg, pushang;
	edict_t	*check, *pusherowner, *moved_edict[MAX_EDICTS];
	bool	rotated, blocked;
	vec3_t	mins, maxs;
	float	pushltime;
	trace_t	trace;

	switch( pusher->v.solid )
	{
	case SOLID_BSP:
	case SOLID_BBOX:
	case SOLID_SLIDEBOX:
		break;
	case SOLID_NOT:
	case SOLID_TRIGGER:
		SV_LinearMove( pusher, movetime, pusher->v.friction );
		SV_AngularMove( pusher, movetime, pusher->v.friction );
		pusher->v.ltime += movetime;
		SV_LinkEdict( pusher, false );
		return;
	default:
		Host_Error( "SV_PushMove: invalid solid type %i\n", pusher->v.solid );
	}

	if( VectorIsNull( pusher->v.velocity ) && VectorIsNull( pusher->v.avelocity ))
	{
		// advances ltime when stop too for thinking
		pusher->v.ltime += movetime;
		return;
	}

	for( i = 0; i < 3; i++ )
	{
		lmove[i] = pusher->v.velocity[i] * movetime;
		amove[i] = pusher->v.avelocity[i] * movetime;
		mins[i] = pusher->v.absmin[i] + lmove[i];
		maxs[i] = pusher->v.absmax[i] + lmove[i];
	}

	pusherowner = pusher->v.owner;
	rotated = !VectorIsNull( amove );

	VectorNegate( amove, a );
	AngleVectors( a, forward, right, up );

	// save pusher parms
	VectorCopy( pusher->v.origin, pushorg );
	VectorCopy( pusher->v.angles, pushang );
	pushltime = pusher->v.ltime;

	// getting real bbox size
	SV_TransformedBBox( pusher, mins, maxs );

	// move the pusher to it's final position
	SV_LinearMove( pusher, movetime, pusher->v.friction );
	SV_AngularMove( pusher, movetime, pusher->v.friction );
	pusher->v.ltime += movetime;
	SV_LinkEdict( pusher, false );

	num_moved = 0;
	oldsolid = pusher->v.solid;

	// see if any solid entities are inside the final position
	for( e = 1; e < svgame.globals->numEntities; e++ )
	{
		check = EDICT_NUM( e );
		if( !SV_IsValidEdict( check )) continue;

		// filter movetypes to collide with
		switch( check->v.movetype )
		{
		case MOVETYPE_NONE:
		case MOVETYPE_PUSH:
		case MOVETYPE_FOLLOW:
		case MOVETYPE_NOCLIP:
		case MOVETYPE_COMPOUND:
			continue;
		default: break;
		}

		if( check->v.owner == pusher )
			continue;
		if( pusherowner == check )
			continue;

		// if the entity is standing on the pusher, it will definitely be moved
		if( !(( check->v.flags & FL_ONGROUND ) && check->v.groundentity == pusher ))
		{
			// see if the ent needs to be tested
			if( check->v.absmin[0] >= maxs[0]
			 || check->v.absmin[1] >= maxs[1]
			 || check->v.absmin[2] >= maxs[2]
			 || check->v.absmax[0] <= mins[0]
			 || check->v.absmax[1] <= mins[1]
			 || check->v.absmax[2] <= mins[2] )
				continue;

			pusher->v.solid = SOLID_NOT;
			trace = SV_ClipMoveToEntity( pusher, check->v.origin, check->v.mins, check->v.maxs, check->v.origin, MASK_SOLID, FMOVE_SIMPLEBOX );
			pusher->v.solid = oldsolid; // was SOLID_BSP
			if( !trace.fStartSolid ) continue; // not touched
		}

		VectorCopy( check->v.origin, check->pvServerData->moved_origin );
		VectorCopy( check->v.angles, check->pvServerData->moved_angles );
		moved_edict[num_moved++] = check;

		// try moving the contacted entity
		pusher->v.solid = SOLID_NOT;
		trace = SV_PushEntity( check, lmove, amove, &blocked );
		pusher->v.solid = oldsolid; // was SOLID_BSP

		if( check->v.groundentity == pusher )
		{
			blocked = false;	// probably groundents can't block pusher
			check->v.flags |= FL_ONGROUND;
		}		

		if( rotated && !blocked )
		{
			// figure movement due to the pusher's amove
			VectorSubtract( check->v.origin, pusher->v.origin, org );
			org2[0] = DotProduct( org, forward );
			org2[1] = -DotProduct( org, right );
			org2[2] = DotProduct( org, up );
			VectorSubtract( org2, org, move2 );

			pusher->v.solid = SOLID_NOT;
			trace = SV_PushEntity( check, move2, vec3_origin, &blocked );
			pusher->v.solid = oldsolid; // was SOLID_BSP
		}

		// remove the onground flag for non-players
		if( check->v.movetype != MOVETYPE_WALK && ( trace.flFraction < 1.0f || check->v.groundentity != pusher ))
			check->v.flags &= ~FL_ONGROUND;

		// if it is still inside the pusher, block
		if( blocked || SV_TestEntityPosition( check, vec3_origin ))
		{
			// fail the move
			if( check->v.mins[0] == check->v.maxs[0] )
				continue;

			if( check->v.solid == SOLID_NOT || check->v.solid == SOLID_TRIGGER || check->v.deadflag == DEAD_DEAD )
			{
				// corpse
				check->v.mins[0] = check->v.mins[1] = 0;
				VectorCopy( check->v.mins, check->v.maxs );
				continue;
			}

			// restore oldpos
			VectorCopy( pushorg, pusher->v.origin );
			VectorCopy( pushang, pusher->v.angles );
			pusher->v.ltime = pushltime;
			SV_LinkEdict( pusher, false );

			// move back any entities we already moved
			for( i = 0; i < num_moved; i++ )
			{
				edict_t *ed = moved_edict[i];
				sv_client_t *cl;
	
				if( ed->v.flags & FL_CLIENT && ( cl = SV_ClientFromEdict( ed, true )) != NULL )
				{
					cl->anglechangetotal = cl->anglechangefinal = 0.0f;
					ed->v.fixangle = 0;	
				}

				VectorCopy( ed->pvServerData->moved_origin, ed->v.origin );
				VectorCopy( ed->pvServerData->moved_angles, ed->v.angles );
				SV_LinkEdict( ed, false );
			}

			// call the pusher "blocked" function
			svgame.globals->time = (sv.time * 0.001f);
			Msg( "%s is blocked by %s\n", SV_ClassName( pusher ), SV_ClassName( check ));
			svgame.dllFuncs.pfnBlocked( pusher, check );
			return;
		}

	}
}

/*
================
SV_Physics_Pusher

================
*/
void SV_Physics_Pusher( edict_t *ent )
{
	float	thinktime;
	float	oldltime;
	float	movetime;

	oldltime = ent->v.ltime;
	thinktime = ent->v.nextthink;

	if( thinktime < ent->v.ltime + ( sv.frametime * 0.001f ))
	{
		movetime = thinktime - ent->v.ltime;
		if( movetime < 0.0f ) movetime = 0.0f;
	}
	else movetime = (sv.frametime * 0.001f);

	if( movetime )
	{
		SV_PushMove( ent, movetime ); // advances ent->v.ltime if not blocked
	}

	if( thinktime > oldltime && thinktime <= ent->v.ltime )
	{
		ent->v.nextthink = 0.0f;
		svgame.globals->time = (sv.time * 0.001f);
		svgame.dllFuncs.pfnThink( ent );
		if( ent->free ) return;
	}
	else if( ent->v.flags & FL_ALWAYSTHINK )
	{
		ent->v.nextthink = 0;
		svgame.globals->time = (sv.time * 0.001f);
		svgame.dllFuncs.pfnThink( ent );
	}
}

//============================================================================

/*
=============
SV_Physics_Compound

Entities that are "stuck" to another entity
assume oldorigin as originoffset and oldangles as angle difference
=============
*/
void SV_Physics_Compound( edict_t *ent )
{
	vec3_t		vf, vr, vu, angles;
	edict_t		*parent;

	// regular thinking
	if( !SV_RunThink( ent )) return;

	parent = ent->v.aiment;
	if( !SV_IsValidEdict( parent )) return;

	// force to hold current values as offsets
	if( ent->v.effects & EF_NOINTERP )
	{
		VectorSubtract( ent->v.origin, parent->v.origin, ent->v.oldorigin );
		VectorSubtract( ent->v.angles, parent->v.angles, ent->v.oldangles );
		ent->v.effects &= ~EF_NOINTERP;	// done
	}

	if( VectorCompare( parent->v.angles, ent->v.oldangles ))
	{
		// quick case for no rotation
		VectorAdd( parent->v.origin, ent->v.oldorigin, ent->v.origin );
	}
	else
	{
		vec3_t	org, org2, move;

		VectorAdd( parent->v.origin, ent->v.oldorigin, ent->v.origin );

		VectorSubtract( ent->v.origin, parent->v.origin, org );
		VectorNegate( parent->v.angles, angles );
		AngleVectors( angles, vf, vr, vu );
		org2[0] = DotProduct( ent->v.oldorigin, vf );
		org2[1] = -DotProduct( ent->v.oldorigin, vr );
		org2[2] = DotProduct( ent->v.oldorigin, vu );		
		VectorSubtract( org2, ent->v.oldorigin, move );
		VectorAdd( ent->v.origin, move, ent->v.origin );
	}

	VectorAdd( parent->v.angles, ent->v.oldangles, ent->v.angles );
	SV_LinkEdict( ent, false );
}

/*
=============
SV_Physics_Follow

just copy angles and origin of parent
=============
*/
void SV_Physics_Follow( edict_t *ent )
{
	edict_t	*parent;

	// regular thinking
	if( !SV_RunThink( ent )) return;

	parent = ent->v.aiment;
	if( !SV_IsValidEdict( parent )) return;

	VectorCopy( parent->v.origin, ent->v.origin );
	VectorCopy( parent->v.angles, ent->v.angles );

	SV_LinkEdict( ent, false );	// nocip ents never touch triggers
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

	VectorMA( ent->v.origin, svgame.globals->frametime, ent->v.velocity,  ent->v.origin );
	VectorMA( ent->v.angles, svgame.globals->frametime, ent->v.avelocity, ent->v.angles );

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

	// regular thinking
	if( !SV_RunThink( ent )) return;

	SV_CheckWaterTransition( ent );
	SV_CheckWater( ent );

	if( ent->v.velocity[2] > DIST_EPSILON )
	{
		ent->v.flags &= ~FL_ONGROUND;
		ent->v.groundentity = NULL;
          }

	// if on ground and not moving, return.
	if( ent->v.flags & FL_ONGROUND && SV_IsValidEdict( ent->v.groundentity ))
	{
		if( VectorIsNull( ent->v.basevelocity ) && VectorIsNull( ent->v.velocity ))
			return;
	}

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

	// move angles (with friction)
	switch( ent->v.movetype )
	{
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		SV_AngularMove( ent, svgame.globals->frametime, ent->v.friction );
		break;         
	default:
		SV_AngularMove( ent, svgame.globals->frametime, 0.0f );
		break;
	}

	// move origin
	// Base velocity is not properly accounted for since this entity will move again
	// after the bounce without taking it into account
	VectorAdd( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );

	SV_CheckVelocity( ent );
	VectorScale( ent->v.velocity, svgame.globals->frametime, move );
	VectorSubtract( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );

	trace = SV_PushEntity( ent, move, vec3_origin, NULL );
	if( ent->free ) return;

	SV_CheckVelocity( ent );

	if( trace.fAllSolid )
	{	
		// entity is trapped in another solid
		ent->v.flags |= FL_ONGROUND;
		ent->v.groundentity = trace.pHit;
		VectorClear( ent->v.velocity );
		return;
	}
	
	if( trace.flFraction == 1.0f )
	{
		SV_CheckWater( ent );
		return;
	}

	if( ent->v.movetype == MOVETYPE_BOUNCE )
		backoff = 2.0f - ent->v.friction;
	else if( ent->v.movetype == MOVETYPE_BOUNCEMISSILE )
		backoff = 2.0f;
	else backoff = 1.0f;

	SV_ClipVelocity( ent->v.velocity, trace.vecPlaneNormal, ent->v.velocity, backoff );

	// stop if on ground
	if( trace.vecPlaneNormal[2] > 0.7f )
	{		
		float	vel;

		if( ent->v.velocity[2] < sv_gravity->value * svgame.globals->frametime )
		{
			// we're rolling on the ground, add static friction.
			ent->v.groundentity = trace.pHit;
			ent->v.flags |= FL_ONGROUND;
			ent->v.velocity[2] = 0.0f;
		}

		vel = DotProduct( ent->v.velocity, ent->v.velocity );

		if( vel < 900 || ( ent->v.movetype != MOVETYPE_BOUNCE && ent->v.movetype != MOVETYPE_BOUNCEMISSILE ))
		{
			ent->v.flags |= FL_ONGROUND;
			ent->v.groundentity = trace.pHit;
			VectorClear( ent->v.velocity ); // avelocity clearing in server.dll
		}
		else
		{
			VectorScale( ent->v.velocity, (1.0f - trace.flFraction) * svgame.globals->frametime * 0.9f, move );
			trace = SV_PushEntity( ent, move, vec3_origin, NULL );
			if( ent->free ) return;
		}
	}
	
	// check for in water
	SV_CheckWater( ent );
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
	bool	wasonground;
	bool	inwater;
	bool	hitsound = false;
	bool	isfalling = false;
	trace_t	trace;

	SV_CheckVelocity( ent );

	wasonground = (ent->v.flags & FL_ONGROUND) ? true : false;

	// add gravity except:
	// flying monsters
	// swimming monsters who are in the water
	inwater = SV_CheckWater( ent );

	if( !wasonground )
	{
		if( !( ent->v.flags & FL_FLY ))
		{
			if(!( ent->v.flags & (FL_SWIM|FL_FLOAT) && ent->v.waterlevel > 0 ))
			{
				if( ent->v.velocity[2] < ( sv_gravity->value * -svgame.globals->frametime ))
				{
					hitsound = true;
				}

				if( !inwater )
				{
					SV_AddHalfGravity( ent, svgame.globals->frametime );
					isfalling = true;
				}
			}
			else if( ent->v.waterlevel > 0 )
			{
				if( ent->v.waterlevel > 1 )
				{
					VectorScale( ent->v.velocity, 0.9f, ent->v.velocity );
					ent->v.velocity[2] += (ent->v.skin * svgame.globals->frametime);
				}
				else if( ent->v.waterlevel == 1 )
				{
					if( ent->v.velocity[2] > 0.0f )
						ent->v.velocity[2] = svgame.globals->frametime;
					ent->v.velocity[2] -= (ent->v.skin * svgame.globals->frametime);
				}
			}
		}
	}

	if( !VectorIsNull( ent->v.velocity ) || !VectorIsNull( ent->v.basevelocity ))
	{
		vec3_t	mins, maxs, point;
		float	friction = sv_friction->value;
		int	x, y;

		if( ent->v.friction != 0.0f )
			friction *= ent->v.friction;

		ent->v.flags &= ~FL_ONGROUND;

		// apply friction
		// let dead monsters who aren't completely onground slide
		if( wasonground )
		{
			float	speed, newspeed;
			float	control;
	
			speed = VectorLength( ent->v.velocity );	

			if( speed )
			{
				control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
				newspeed = speed - svgame.globals->frametime * control * friction;

				if( newspeed < 0.0f )
					newspeed = 0.0f;
				newspeed /= speed;

				ent->v.velocity[0] *= newspeed;
				ent->v.velocity[1] *= newspeed;
			}
		}

		VectorAdd( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );

		SV_AngularMove( ent, svgame.globals->frametime, friction );

		SV_CheckVelocity( ent );
		SV_TryMove( ent, svgame.globals->frametime, NULL );
		SV_CheckVelocity( ent );

		VectorSubtract( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );
		SV_CheckVelocity( ent );

		// determine if it's on solid ground at all
		VectorAdd( ent->v.origin, ent->v.mins, mins );
		VectorAdd( ent->v.origin, ent->v.maxs, maxs );

		point[2] = mins[2] - 1;
		for( x = 0; x <= 1; x++ )
		{
			if( ent->v.flags & FL_ONGROUND )
				break;

			for( y = 0; y <= 1; y++ )
			{
				point[0] = x ? maxs[0] : mins[0];
				point[1] = y ? maxs[1] : mins[1];

				trace = SV_Move( point, vec3_origin, vec3_origin, point, MOVE_NORMAL, ent );			

				if( trace.fStartSolid )
				{
					ent->v.flags |= FL_ONGROUND;
					ent->v.groundentity = trace.pHit;
					break;
				}
			}

		}
		SV_LinkEdict( ent, true );
	}

	if(!( ent->v.flags & FL_ONGROUND) && isfalling )
	{
		SV_AddHalfGravity( ent, svgame.globals->frametime );
	}

	if( !SV_RunThink( ent )) return;

	SV_CheckWaterTransition( ent );
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

	for( i = 0; i < svgame.globals->maxClients; i++ )
	{
		player = EDICT_NUM( i + 1 );
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
	SV_UpdateBaseVelocity( ent );

	if(!( ent->v.flags & FL_BASEVELOCITY ) && !VectorIsNull( ent->v.basevelocity ))
	{
		// Apply momentum (add in half of the previous frame of velocity first)
		VectorMA( ent->v.velocity, 1.0f + (svgame.globals->frametime * 0.5f), ent->v.basevelocity, ent->v.velocity );
		VectorClear( ent->v.basevelocity );
	}
	ent->v.flags &= ~FL_BASEVELOCITY;

	// user dll can override movement type
	if( svgame.dllFuncs.pfnPhysicsEntity( ent ))
		return;

	switch( ent->v.movetype )
	{
	case MOVETYPE_NONE:
		SV_Physics_None( ent );
		break;
	case MOVETYPE_NOCLIP:
		SV_Physics_Noclip( ent );
		break;
	case MOVETYPE_FOLLOW:
		SV_Physics_Follow( ent );
		break;
	case MOVETYPE_COMPOUND:
		SV_Physics_Compound( ent );
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
	case MOVETYPE_PUSH:
		SV_Physics_Pusher( ent );
		break;
	case MOVETYPE_CONVEYOR:
		SV_Physics_Conveyor( ent );
		break;
	case MOVETYPE_WALK:
		Host_Error( "SV_Physics: bad movetype %i\n", ent->v.movetype );
		break;
	}
}

void SV_FreeOldEntities( void )
{
	edict_t	*ent;
	int	i;

	// at end of frame kill all entities which supposed to it 
	for( i = svgame.globals->maxClients + 1; i < svgame.globals->numEntities; i++ )
	{
		ent = EDICT_NUM( i );
		if( ent->free ) continue;

		if( ent->v.flags & FL_KILLME )
			SV_FreeEdict( ent );
	}

	// decrement svgame.globals->numEntities if the highest number entities died
	for( ; EDICT_NUM( svgame.globals->numEntities - 1)->free; svgame.globals->numEntities-- );
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
	for( i = 1; ( svgame.globals->force_retouch > 0 ) && i < svgame.globals->numEntities; i++ )
	{
		ent = EDICT_NUM( i );
		if( !SV_IsValidEdict( ent )) continue;

		// force retouch even for stationary
		SV_LinkEdict( ent, true );
	}

	// treat each object in turn
	if( !( sv.hostflags & SVF_PLAYERSONLY ))
	{
		for( i = svgame.globals->maxClients + 1; i < svgame.globals->numEntities; i++ )
		{
			ent = EDICT_NUM( i );
			if( !SV_IsValidEdict( ent )) continue;

			SV_Physics_Entity( ent );
		}
	}

	// let everything in the world think and move
	CM_Frame( svgame.globals->frametime );

	// at end of frame kill all entities which supposed to it 
	SV_FreeOldEntities();

	if( svgame.globals->force_retouch > 0 )
		svgame.globals->force_retouch = max( 0, svgame.globals->force_retouch - 1 );

	svgame.dllFuncs.pfnEndFrame();

	if( !( sv.hostflags & SVF_PLAYERSONLY )) sv.time += sv.frametime;
}