//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_move.c - monsters movement
//=======================================================================

#include "common.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "server.h"
#include "const.h"

/*
=============
SV_CheckBottom

Returns false if any part of the bottom of the entity is off an edge that
is not a staircase.

=============
*/
bool SV_CheckBottom( edict_t *ent, int iMode )
{
	vec3_t	mins, maxs, start, stop;
	bool	realcheck = false;
	float	mid, bottom;
	trace_t	trace;
	int	x, y;

	VectorAdd( ent->v.origin, ent->v.mins, mins );
	VectorAdd( ent->v.origin, ent->v.maxs, maxs );

	// if all of the points under the corners are solid world, don't bother
	// with the tougher checks
	// the corners must be within 16 of the midpoint
	start[2] = mins[2] - 1;
	for( x = 0; x <= 1; x++ )
	{
		for( y = 0; y <= 1; y++ )
		{
			start[0] = x ? maxs[0] : mins[0];
			start[1] = y ? maxs[1] : mins[1];
			if( SV_BaseContents( start, ent ) & MASK_SOLID )
			{
				realcheck = true;
				break;
			}
		}
		if( realcheck )
			break;
	}

	if( !realcheck )
		return true; // we got out easy

	// check it for real...
	start[2] = mins[2] + svgame.movevars.stepsize;

	// the midpoint must be within 16 of the bottom
	start[0] = stop[0] = (mins[0] + maxs[0]) * 0.5f;
	start[1] = stop[1] = (mins[1] + maxs[1]) * 0.5f;
	stop[2] = start[2] - 2 * svgame.movevars.stepsize;

	if( iMode == WALKMOVE_WORLDONLY )
		trace = SV_Move( start, vec3_origin, vec3_origin, stop, MOVE_WORLDONLY, ent );
	else trace = SV_Move( start, vec3_origin, vec3_origin, stop, MOVE_NORMAL|FTRACE_SIMPLEBOX, ent );

	if( trace.flFraction == 1.0f )
		return false;
	mid = bottom = trace.vecEndPos[2];

	// the corners must be within 16 of the midpoint
	for( x = 0; x <= 1; x++ )
	{
		for( y = 0; y <= 1; y++ )
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];

			if( iMode == WALKMOVE_WORLDONLY )
				trace = SV_Move( start, vec3_origin, vec3_origin, stop, MOVE_WORLDONLY, ent );
			else trace = SV_Move( start, vec3_origin, vec3_origin, stop, MOVE_NORMAL|FTRACE_SIMPLEBOX, ent );

			if( trace.flFraction != 1.0f && trace.vecEndPos[2] > bottom )
				bottom = trace.vecEndPos[2];
			if( trace.flFraction == 1.0f || mid - trace.vecEndPos[2] > svgame.movevars.stepsize )
				return false;
		}
	}
	return true;
}

/*
=============
SV_VecToYaw

converts dir to yaw
=============
*/
float SV_VecToYaw( const vec3_t src )
{
	float	yaw;

	if( src[1] == 0 && src[0] == 0 )
	{
		yaw = 0;
	}
	else
	{
		yaw = (int)( com.atan2( src[1], src[0] ) * 180 / M_PI );
		if( yaw < 0 ) yaw += 360;
	}
	return yaw;
}

//============================================================================
/*
======================
SV_WalkMove

======================
*/
bool SV_WalkMove( edict_t *ent, vec3_t move, int iMode )
{
	trace_t	trace;
	vec3_t	oldorg, neworg, end;
	edict_t	*groundent = NULL;
	bool	relink;

	if( iMode == WALKMOVE_NORMAL )
		relink = true;
	else relink = false;

	// try the move
	VectorCopy( ent->v.origin, oldorg );
   
	// flying pawns don't step up
	if( ent->v.flags & ( FL_SWIM|FL_FLY ))
	{
		VectorAdd( oldorg, move, neworg );

		if( iMode == WALKMOVE_WORLDONLY )
			trace = SV_Move( oldorg, ent->v.mins, ent->v.maxs, neworg, MOVE_WORLDONLY, ent );
		else trace = SV_Move( oldorg, ent->v.mins, ent->v.maxs, neworg, MOVE_NORMAL|FTRACE_SIMPLEBOX, ent );

		if( trace.flFraction == 1.0f )
		{
			if(( ent->v.flags & FL_SWIM ) && !( SV_BaseContents(trace.vecEndPos, ent) & MASK_WATER ))
				return false; // swimming pawn left water

			VectorCopy( trace.vecEndPos, ent->v.origin );

			if( !VectorCompare( ent->v.origin, oldorg ))
				SV_LinkEdict( ent, relink );

			return true;
		}
		return false;
	}

	VectorAdd( oldorg, move, neworg );

	// push down from a step height above the wished position
	neworg[2] += svgame.movevars.stepsize;
	VectorCopy( neworg, end );
	end[2] -= svgame.movevars.stepsize * 2;

	if( iMode == WALKMOVE_WORLDONLY )
		trace = SV_Move( neworg, ent->v.mins, ent->v.maxs, end, MOVE_WORLDONLY, ent );
	else trace = SV_Move( neworg, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL|FTRACE_SIMPLEBOX, ent );
    
	if( trace.fAllSolid )
	{
		Msg( "WalkMove: all solid\n" );
		return false;
	}
	if( trace.fStartSolid )
	{
		neworg[2] -= svgame.movevars.stepsize;

		if( iMode == WALKMOVE_WORLDONLY )
			trace = SV_Move( neworg, ent->v.mins, ent->v.maxs, end, MOVE_WORLDONLY, ent );
		else trace = SV_Move( neworg, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL|FTRACE_SIMPLEBOX, ent );

		if( trace.fAllSolid || trace.fStartSolid )
		{
			Msg( "WalkMove: all solid || start solid\n" );
			return false;
		}
	}

	if( trace.flFraction == 1.0f )
	{
		// if monster had the ground pulled out, go ahead and fall
		if( ent->v.flags & FL_PARTIALGROUND )
		{
			VectorAdd( ent->v.origin, move, ent->v.origin );

			if( !VectorCompare( ent->v.origin, oldorg ))
				SV_LinkEdict( ent, relink );

			ent->v.flags &= ~FL_ONGROUND;
			return true;
		}
		Msg( "WalkMove: walked off and edge\n" );
		return false; // walked off an edge
	}

	// check point traces down for dangling corners
	VectorCopy( trace.vecEndPos, ent->v.origin );
	groundent = trace.pHit;

	// check our pos
	if( iMode == WALKMOVE_WORLDONLY )
		trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_WORLDONLY, ent );
	else trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NORMAL|FTRACE_SIMPLEBOX, ent );

	if( trace.fStartSolid )
	{
		VectorCopy( oldorg, ent->v.origin );
		Msg( "WalkMove: start solid\n" );
		return false;
	}

	if( !SV_CheckBottom( ent, iMode ))
	{
		if( ent->v.flags & FL_PARTIALGROUND )
		{    
			// actor had floor mostly pulled out from underneath it
			// and is trying to correct
			if( !VectorCompare( ent->v.origin, oldorg ))
				SV_LinkEdict( ent, relink );
			Msg( "WalkMove: partialground - ok\n" );
			return true;
		}

		ent->v.flags |= FL_PARTIALGROUND;
		VectorCopy( oldorg, ent->v.origin );
		Msg( "WalkMove: restore old pos\n" );
		return false;
	}

	if( ent->v.flags & FL_PARTIALGROUND )
		ent->v.flags &= ~FL_PARTIALGROUND;

	ent->v.groundentity = groundent;

	// the move is ok
	if( !VectorCompare( ent->v.origin, oldorg ))
		SV_LinkEdict( ent, relink );
	return true;
}

/*
======================
SV_StepDirection

Turns to the movement direction, and walks the current distance if
facing it.
======================
*/
bool SV_StepDirection( edict_t *ent, float yaw, float dist, int iMode )
{
	vec3_t	move, oldorigin;
	float	delta;

	yaw = yaw * M_PI*2 / 360;
	VectorSet( move, com.cos( yaw ) * dist, com.sin( yaw ) * dist, 0.0f );
	VectorCopy( ent->v.origin, oldorigin );

	if( SV_WalkMove( ent, move, WALKMOVE_NORMAL ))
	{
		if( iMode != MOVE_STRAFE )
		{
			delta = ent->v.angles[YAW] - ent->v.ideal_yaw;
			if( delta > 45 && delta < 315 )
			{		
				// not turned far enough, so don't take the step
				VectorCopy( oldorigin, ent->v.origin );
			}
		}
		SV_LinkEdict( ent, false );
		return true;
	}

	SV_LinkEdict( ent, false );
	return false;
}

/*
======================
SV_MoveToOrigin

Turns to the movement direction, and walks the current distance if
facing it.
======================
*/  
void SV_MoveToOrigin( edict_t *ed, const vec3_t goal, float dist, int iMode )
{
	float	yaw, distToGoal;
	vec3_t	vecDist;

	if( iMode == MOVE_STRAFE )
	{
		vec3_t	delta;
		
		VectorSubtract( goal, ed->v.origin, delta );
		VectorNormalizeFast( delta );
		yaw = SV_VecToYaw( delta );
	}
	else
	{
		yaw = ed->v.ideal_yaw;
	}


	VectorSubtract( ed->v.origin, goal, vecDist );
	distToGoal = com.sqrt( vecDist[0] * vecDist[0] + vecDist[1] * vecDist[1] );
	if( dist > distToGoal ) dist = distToGoal;

	SV_StepDirection( ed, yaw, dist, iMode );
}