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
bool SV_CheckBottom( edict_t *ent )
{
	vec3_t	mins, maxs, start, stop;
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
			if( SV_PointContents( start ) != CONTENTS_SOLID )
				goto realcheck;
		}
	}
	return true; // we got out easy

realcheck:
	// check it for real...
	start[2] = mins[2];

	// the midpoint must be within 16 of the bottom
	start[0] = stop[0] = (mins[0] + maxs[0]) * 0.5f;
	start[1] = stop[1] = (mins[1] + maxs[1]) * 0.5f;
	stop[2] = start[2] - 2 * sv_stepheight->value;
	trace = SV_Move( start, vec3_origin, vec3_origin, stop, MOVE_NOMONSTERS, ent );

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

			trace = SV_Move( start, vec3_origin, vec3_origin, stop, MOVE_NOMONSTERS, ent );

			if( trace.flFraction != 1.0f && trace.vecEndPos[2] > bottom )
				bottom = trace.vecEndPos[2];
			if( trace.flFraction == 1.0f || mid - trace.vecEndPos[2] > sv_stepheight->value )
				return false;
		}
	}
	return true;
}


/*
=============
SV_movestep

Called by monster program code.
The move will be adjusted for slopes and stairs, but if the move isn't
possible, no move is done and false is returned
=============
*/
bool SV_movestep( edict_t *ent, vec3_t move, bool relink, bool noenemy, bool settrace )
{
	float	dz;
	vec3_t	oldorg, neworg;
	vec3_t	end, endpos;
	edict_t	*enemy;
	trace_t	trace;
	int	i;

	// try the move
	VectorCopy (ent->v.origin, oldorg);
	VectorAdd (ent->v.origin, move, neworg);

	// flying monsters don't step up
	if( ent->v.flags & (FL_SWIM|FL_FLY))
	{
		// try one move with vertical motion, then one without
		for( i = 0; i < 2; i++ )
		{
			VectorAdd( ent->v.origin, move, neworg );
			if( noenemy ) enemy = EDICT_NUM( 0 );
			else
			{
				enemy = ent->v.enemy;
				if( i == 0 && enemy != EDICT_NUM( 0 ))
				{
					dz = ent->v.origin[2] - ent->v.enemy->v.origin[2];
					if( dz > 40 ) neworg[2] -= 8;
					if( dz < 30 ) neworg[2] += 8;
				}
			}
			trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, neworg, MOVE_NORMAL, ent );

			if( trace.flFraction == 1.0f )
			{
				VectorCopy( trace.vecEndPos, endpos );
				if( ent->v.flags & FL_SWIM && SV_PointContents( endpos ) == CONTENTS_EMPTY )
					return false; // swim monster left water

				VectorCopy( endpos, ent->v.origin );
				if( relink ) SV_LinkEdict( ent, true );
				return true;
			}
			if( enemy == EDICT_NUM( 0 )) break;
		}
		return false;
	}

	// push down from a step height above the wished position
	neworg[2] += sv_stepheight->value;
	VectorCopy( neworg, end );
	end[2] -= sv_stepheight->value * 2;

	trace = SV_Move( neworg, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent );

	if( trace.fAllSolid )
		return false;

	if( trace.fStartSolid )
	{
		neworg[2] -= sv_stepheight->value;
		trace = SV_Move( neworg, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent );
		if( trace.fAllSolid || trace.fStartSolid )
			return false;
	}
	if( trace.flFraction == 1.0f )
	{
		// if monster had the ground pulled out, go ahead and fall
		if( ent->v.flags & FL_PARTIALONGROUND )
		{
			VectorAdd( ent->v.origin, move, ent->v.origin );
			if( relink ) SV_LinkEdict( ent, true );
			ent->v.flags &= ~FL_ONGROUND;
			return true;
		}
		return false; // walked off an edge
	}

	// check point traces down for dangling corners
	VectorCopy( trace.vecEndPos, ent->v.origin );

	if(!SV_CheckBottom( ent ))
	{
		if( ent->v.flags & FL_PARTIALONGROUND )
		{	
			// entity had floor mostly pulled out from underneath it
			// and is trying to correct
			if( relink ) SV_LinkEdict( ent, true );
			return true;
		}
		VectorCopy( oldorg, ent->v.origin );
		return false;
	}

	if( ent->v.flags & FL_PARTIALONGROUND )
		ent->v.flags &= ~FL_PARTIALONGROUND;

	ent->v.groundentity = trace.pHit;

	// the move is ok
	if( relink ) SV_LinkEdict( ent, true );
	return true;
}


//============================================================================

/*
======================
SV_StepDirection

Turns to the movement direction, and walks the current distance if
facing it.

======================
*/
bool SV_StepDirection( edict_t *ent, float yaw, float dist )
{
	vec3_t	move, oldorigin;
	float	delta;

	ent->v.ideal_yaw = yaw;
	ent->v.angles[1] = SV_AngleMod( ent->v.ideal_yaw, anglemod( ent->v.angles[1] ), ent->v.yaw_speed );

	yaw = yaw * M_PI*2 / 360;
	move[0] = cos(yaw)*dist;
	move[1] = sin(yaw)*dist;
	move[2] = 0;

	VectorCopy( ent->v.origin, oldorigin );
	if(SV_movestep( ent, move, false, false, false ))
	{
		delta = ent->v.angles[YAW] - ent->v.ideal_yaw;
		if( delta > 45 && delta < 315 )
		{		
			// not turned far enough, so don't take the step
			VectorCopy( oldorigin, ent->v.origin );
		}
		SV_LinkEdict( ent, true );
		return true;
	}
	SV_LinkEdict( ent, true );

	return false;
}

/*
======================
SV_FixCheckBottom

======================
*/
void SV_FixCheckBottom( edict_t *ent )
{
	ent->v.flags |= FL_PARTIALONGROUND;
}

/*
================
SV_NewChaseDir

================
*/
void SV_NewChaseDir( edict_t *actor, edict_t *enemy, float dist )
{
	float		deltax, deltay;
	float		d[3], tdir, olddir, turnaround;

	olddir = anglemod((int)(actor->v.ideal_yaw / 45 ) * 45 );
	turnaround = anglemod( olddir - 180 );

	deltax = enemy->v.origin[0] - actor->v.origin[0];
	deltay = enemy->v.origin[1] - actor->v.origin[1];
	if( deltax > 10 ) d[1]= 0;
	else if( deltax < -10 ) d[1] = 180;
	else d[1] = -1;
	if( deltay < -10 ) d[2] = 270;
	else if( deltay > 10 ) d[2] = 90;
	else d[2] = -1;

	// try direct route
	if( d[1] != -1 && d[2] != -1 )
	{
		if( d[1] == 0 ) tdir = d[2] == 90 ? 45 : 315;
		else tdir = d[2] == 90 ? 135 : 215;

		if( tdir != turnaround && SV_StepDirection( actor, tdir, dist ))
			return;
	}

	// try other directions
	if((( rand()&3 ) & 1 ) || fabs(deltay) > fabs( deltax ))
	{
		tdir = d[1];
		d[1] = d[2];
		d[2] = tdir;
	}

	if( d[1] != -1 && d[1] != turnaround && SV_StepDirection( actor, d[1], dist ))
		return;

	if( d[2] != -1 && d[2] != turnaround && SV_StepDirection( actor, d[2], dist ))
		return;

	// there is no direct path to the player, so pick another direction
	if( olddir != -1 && SV_StepDirection( actor, olddir, dist ))
		return;

 	// randomly determine direction of search
	if( rand() & 1 )
	{
		for( tdir = 0; tdir <= 315; tdir += 45 )
			if( tdir != turnaround && SV_StepDirection( actor, tdir, dist ))
				return;
	}
	else
	{
		for( tdir = 315; tdir >= 0; tdir -= 45 )
			if( tdir != turnaround && SV_StepDirection( actor, tdir, dist ))
				return;
	}

	if( turnaround != -1 && SV_StepDirection( actor, turnaround, dist ))
			return;

	actor->v.ideal_yaw = olddir; // can't move

	// if a bridge was pulled out from underneath a monster, it may not have
	// a valid standing position at all
	if(!SV_CheckBottom( actor )) SV_FixCheckBottom( actor );
}

/*
======================
SV_CloseEnough

======================
*/
bool SV_CloseEnough( edict_t *ent, edict_t *goal, float dist )
{
	int	i;

	for( i = 0; i < 3; i++ )
	{
		if( goal->v.absmin[i] > ent->v.absmax[i] + dist )
			return false;
		if( goal->v.absmax[i] < ent->v.absmin[i] - dist )
			return false;
	}
	return true;
}

/*
======================
SV_MoveToGoal

======================
*/
bool SV_MoveToGoal( edict_t *ent, edict_t *goal, float dist )
{
	if(!(ent->v.flags & (FL_FLY|FL_SWIM|FL_ONGROUND)))
	{
		return false;
	}

	// if the next step hits the enemy, return immediately
	if( ent->v.enemy != EDICT_NUM( 0 ) && SV_CloseEnough( ent, goal, dist ))
		return false;

	// bump around...
	if(( rand() & 3) == 1 || !SV_StepDirection( ent, ent->v.ideal_yaw, dist ))
	{
		SV_NewChaseDir( ent, goal, dist );
	}
	return true;
}

/*
==============
SV_ClientMove

grab user cmd from player state
send it to transform callback
==============
*/
void SV_PlayerMove( edict_t *player )
{
	sv_client_t	*client;
	usercmd_t		*cmd;

	client = player->pvServerData->client;
	cmd = &client->lastcmd;

	// call PM_PlayerMove here
}