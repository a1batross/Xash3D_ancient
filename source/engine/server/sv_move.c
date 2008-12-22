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

	VectorAdd (ent->v.origin, ent->v.mins, mins);
	VectorAdd (ent->v.origin, ent->v.maxs, maxs);

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
			if(!(SV_PointContents( start ) & (CONTENTS_SOLID | CONTENTS_BODY)))
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
	trace = SV_Trace( start, vec3_origin, vec3_origin, stop, MOVE_NOMONSTERS, ent, SV_ContentsMask(ent));

	if( trace.fraction == 1.0 )
		return false;
	mid = bottom = trace.endpos[2];

	// the corners must be within 16 of the midpoint
	for( x = 0; x <= 1; x++ )
	{
		for( y = 0; y <= 1; y++ )
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];

			trace = SV_Trace( start, vec3_origin, vec3_origin, stop, MOVE_NOMONSTERS, ent, SV_ContentsMask(ent));

			if( trace.fraction != 1.0 && trace.endpos[2] > bottom )
				bottom = trace.endpos[2];
			if( trace.fraction == 1.0 || mid - trace.endpos[2] > sv_stepheight->value )
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
	float		dz;
	vec3_t		oldorg, neworg, end, traceendpos;
	edict_t		*enemy;
	trace_t		trace;
	int		i;

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
			trace = SV_Trace( ent->v.origin, ent->v.mins, ent->v.maxs, neworg, MOVE_NORMAL, ent, SV_ContentsMask(ent));

			if( trace.fraction == 1 )
			{
				VectorCopy( trace.endpos, traceendpos );
				if((ent->v.flags & FL_SWIM) && !(SV_PointContents(traceendpos) & MASK_WATER))
					return false; // swim monster left water

				VectorCopy( traceendpos, ent->v.origin );
				if( relink ) SV_LinkEdict( ent );
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

	trace = SV_Trace( neworg, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent, SV_ContentsMask(ent));

	if( trace.startsolid )
	{
		neworg[2] -= sv_stepheight->value;
		trace = SV_Trace( neworg, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent, SV_ContentsMask(ent));
		if( trace.startsolid ) return false;
	}
	if( trace.fraction == 1 )
	{
		// if monster had the ground pulled out, go ahead and fall
		if( ent->v.flags & FL_PARTIALONGROUND )
		{
			VectorAdd( ent->v.origin, move, ent->v.origin );
			if (relink) SV_LinkEdict( ent );
			ent->v.flags &= ~FL_ONGROUND;
			return true;
		}
		return false; // walked off an edge
	}

	// check point traces down for dangling corners
	VectorCopy( trace.endpos, ent->v.origin );

	if(!SV_CheckBottom( ent ))
	{
		if( ent->v.flags & FL_PARTIALONGROUND )
		{	
			// entity had floor mostly pulled out from underneath it
			// and is trying to correct
			if( relink ) SV_LinkEdict( ent );
			return true;
		}
		VectorCopy( oldorg, ent->v.origin );
		return false;
	}

	if( ent->v.flags & FL_PARTIALONGROUND )
		ent->v.flags &= ~FL_PARTIALONGROUND;

	ent->v.groundentity = trace.ent;

	// the move is ok
	if( relink ) SV_LinkEdict( ent );
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
	float	delta, current;

	ent->v.ideal_yaw = yaw;
	current = anglemod( ent->v.angles[1] );
	ent->v.angles[1] = SV_AngleMod( ent->v.ideal_yaw, current, ent->v.yaw_speed );

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
		SV_LinkEdict( ent );
		return true;
	}
	SV_LinkEdict( ent );

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

	olddir = anglemod((int)(actor->v.ideal_yaw/45) * 45);
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
	if(((rand()&3) & 1) || fabs(deltay) > fabs(deltax))
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


// g-cont: my stupid callbacks
cmodel_t *SV_GetModelPtr( edict_t *ent )
{
	return pe->RegisterModel( sv.configstrings[CS_MODELS + (int)ent->v.modelindex] );
}

float *SV_GetModelVerts( edict_t *ent, int *numvertices )
{
	cmodel_t	*cmod;

	cmod = pe->RegisterModel( sv.configstrings[CS_MODELS + (int)ent->v.modelindex] );
	if( cmod )
	{
		int i = (int)ent->v.body;
		i = bound( 0, i, cmod->numbodies ); // make sure what body exist
		
		if( cmod->col[i] )
		{
			*numvertices = cmod->col[i]->numverts;
			return (float *)cmod->col[i]->verts;
		}
	}
	return NULL;
}

void SV_Transform( edict_t *edict, const vec3_t origin, const matrix3x3 matrix )
{
	vec3_t	angles;

	if( !edict ) return;

	Matrix3x3_Transpose( edict->v.m_pmatrix, matrix );
#if 0
	edict->v.m_pmatrix[0][0] = matrix[0][0];
	edict->v.m_pmatrix[0][1] = matrix[0][2];
	edict->v.m_pmatrix[0][2] = matrix[0][1];
	edict->v.m_pmatrix[1][0] = matrix[1][0];
	edict->v.m_pmatrix[1][1] = matrix[1][2];
	edict->v.m_pmatrix[1][2] = matrix[1][1];
	edict->v.m_pmatrix[2][0] = matrix[2][0];
	edict->v.m_pmatrix[2][1] = matrix[2][2];
	edict->v.m_pmatrix[2][2] = matrix[2][1];

	Matrix3x3_ConcatRotate( edict->v.m_pmatrix, -90, 1, 0, 0 );
	Matrix3x3_ConcatRotate( edict->v.m_pmatrix, 180, 0, 1, 0 );
	Matrix3x3_ConcatRotate( edict->v.m_pmatrix, 90, 0, 0, 1 );
	Matrix3x3_ToAngles( edict->v.m_pmatrix, angles );
#endif
	VectorCopy( origin, edict->v.origin );

	MatrixAngles( edict->v.m_pmatrix, angles );
	edict->v.angles[0] = angles[0];
	edict->v.angles[1] = angles[1];
	edict->v.angles[2] = angles[2];

	// refresh force and torque
	pe->GetForce( edict->pvEngineData->physbody, edict->v.velocity, edict->v.avelocity, edict->v.force, edict->v.torque );
	pe->GetMassCentre( edict->pvEngineData->physbody, edict->v.m_pcentre );
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
	pmove_t		pm;
	sv_client_t	*client;

	client = player->pvEngineData->client;
	memset( &pm, 0, sizeof(pm) );

	if( player->v.movetype == MOVETYPE_NOCLIP )
		player->pvEngineData->s.pm_type = PM_SPECTATOR;
	else player->pvEngineData->s.pm_type = PM_NORMAL;
	player->pvEngineData->s.gravity = sv_gravity->value;

	if( player->v.teleport_time )
		player->pvEngineData->s.pm_flags |= PMF_TIME_TELEPORT; 
	else player->pvEngineData->s.pm_flags &= ~PMF_TIME_TELEPORT; 

	pm.ps = player->pvEngineData->s;
	pm.cmd = client->lastcmd;
	pm.body = player->pvEngineData->physbody;	// member body ptr
	
	VectorCopy( player->v.origin, pm.ps.origin );
	VectorCopy( player->v.velocity, pm.ps.velocity );

	pe->PlayerMove( &pm, false );	// server move

	// save results of pmove
	player->pvEngineData->s = pm.ps;

	VectorCopy(pm.ps.origin, player->v.origin);
	VectorCopy(pm.ps.velocity, player->v.velocity);
	VectorCopy(pm.mins, player->v.mins);
	VectorCopy(pm.maxs, player->v.maxs);
	VectorCopy(pm.ps.viewangles, player->pvEngineData->s.viewangles );
}

void SV_PlaySound( edict_t *ed, float volume, float pitch, const char *sample )
{
	// FIXME: send sound
}