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

	VectorAdd (ent->progs.sv->origin, ent->progs.sv->mins, mins);
	VectorAdd (ent->progs.sv->origin, ent->progs.sv->maxs, maxs);

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
	VectorCopy (ent->progs.sv->origin, oldorg);
	VectorAdd (ent->progs.sv->origin, move, neworg);

	// flying monsters don't step up
	if((int)ent->progs.sv->aiflags & (AI_SWIM|AI_FLY))
	{
		// try one move with vertical motion, then one without
		for( i = 0; i < 2; i++ )
		{
			VectorAdd( ent->progs.sv->origin, move, neworg );
			if( noenemy ) enemy = prog->edicts;
			else
			{
				enemy = PRVM_PROG_TO_EDICT( ent->progs.sv->enemy );
				if( i == 0 && enemy != prog->edicts )
				{
					dz = ent->progs.sv->origin[2] - PRVM_PROG_TO_EDICT(ent->progs.sv->enemy)->progs.sv->origin[2];
					if( dz > 40 ) neworg[2] -= 8;
					if( dz < 30 ) neworg[2] += 8;
				}
			}
			trace = SV_Trace( ent->progs.sv->origin, ent->progs.sv->mins, ent->progs.sv->maxs, neworg, MOVE_NORMAL, ent, SV_ContentsMask(ent));

			if( trace.fraction == 1 )
			{
				VectorCopy( trace.endpos, traceendpos );
				if(((int)ent->progs.sv->aiflags & AI_SWIM) && !(SV_PointContents(traceendpos) & MASK_WATER))
					return false; // swim monster left water

				VectorCopy( traceendpos, ent->progs.sv->origin );
				if( relink ) SV_LinkEdict( ent );
				return true;
			}
			if( enemy == prog->edicts ) break;
		}
		return false;
	}

	// push down from a step height above the wished position
	neworg[2] += sv_stepheight->value;
	VectorCopy( neworg, end );
	end[2] -= sv_stepheight->value * 2;

	trace = SV_Trace( neworg, ent->progs.sv->mins, ent->progs.sv->maxs, end, MOVE_NORMAL, ent, SV_ContentsMask(ent));

	if( trace.startsolid )
	{
		neworg[2] -= sv_stepheight->value;
		trace = SV_Trace( neworg, ent->progs.sv->mins, ent->progs.sv->maxs, end, MOVE_NORMAL, ent, SV_ContentsMask(ent));
		if( trace.startsolid ) return false;
	}
	if( trace.fraction == 1 )
	{
		// if monster had the ground pulled out, go ahead and fall
		if((int)ent->progs.sv->aiflags & AI_PARTIALONGROUND )
		{
			VectorAdd( ent->progs.sv->origin, move, ent->progs.sv->origin );
			if (relink) SV_LinkEdict( ent );
			ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags & ~AI_ONGROUND;
			return true;
		}
		return false; // walked off an edge
	}

	// check point traces down for dangling corners
	VectorCopy( trace.endpos, ent->progs.sv->origin );

	if(!SV_CheckBottom( ent ))
	{
		if((int)ent->progs.sv->aiflags & AI_PARTIALONGROUND )
		{	
			// entity had floor mostly pulled out from underneath it
			// and is trying to correct
			if( relink ) SV_LinkEdict( ent );
			return true;
		}
		VectorCopy( oldorg, ent->progs.sv->origin );
		return false;
	}

	if((int)ent->progs.sv->aiflags & AI_PARTIALONGROUND )
		ent->progs.sv->flags = (int)ent->progs.sv->flags & ~AI_PARTIALONGROUND;

	ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG( trace.ent );

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

	ent->progs.sv->ideal_yaw = yaw;
	current = anglemod( ent->progs.sv->angles[1] );
	ent->progs.sv->angles[1] = SV_AngleMod( ent->progs.sv->ideal_yaw, current, ent->progs.sv->yaw_speed );

	yaw = yaw * M_PI*2 / 360;
	move[0] = cos(yaw)*dist;
	move[1] = sin(yaw)*dist;
	move[2] = 0;

	VectorCopy( ent->progs.sv->origin, oldorigin );
	if(SV_movestep( ent, move, false, false, false ))
	{
		delta = ent->progs.sv->angles[YAW] - ent->progs.sv->ideal_yaw;
		if( delta > 45 && delta < 315 )
		{		
			// not turned far enough, so don't take the step
			VectorCopy( oldorigin, ent->progs.sv->origin );
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
	ent->progs.sv->flags = (int)ent->progs.sv->aiflags | AI_PARTIALONGROUND;
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

	olddir = anglemod((int)(actor->progs.sv->ideal_yaw/45) * 45);
	turnaround = anglemod( olddir - 180 );

	deltax = enemy->progs.sv->origin[0] - actor->progs.sv->origin[0];
	deltay = enemy->progs.sv->origin[1] - actor->progs.sv->origin[1];
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

	actor->progs.sv->ideal_yaw = olddir; // can't move

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
		if( goal->progs.sv->absmin[i] > ent->progs.sv->absmax[i] + dist )
			return false;
		if( goal->progs.sv->absmax[i] < ent->progs.sv->absmin[i] - dist )
			return false;
	}
	return true;
}

/*
======================
SV_MoveToGoal

======================
*/
void SV_MoveToGoal( void )
{
	edict_t		*ent, *goal;
	float		dist;

	if(!VM_ValidateArgs( "movetogoal", 1 ))
		return;

	ent = PRVM_PROG_TO_EDICT( prog->globals.sv->pev );
	goal = PRVM_PROG_TO_EDICT( ent->progs.sv->goalentity );
	dist = PRVM_G_FLOAT( OFS_PARM0 );

	if(!((int)ent->progs.sv->aiflags & (AI_ONGROUND|AI_FLY|AI_SWIM)))
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	// if the next step hits the enemy, return immediately
	if(PRVM_PROG_TO_EDICT(ent->progs.sv->enemy) != prog->edicts &&  SV_CloseEnough( ent, goal, dist))
		return;

	// bump around...
	if(( rand() & 3) == 1 || !SV_StepDirection( ent, ent->progs.sv->ideal_yaw, dist ))
	{
		SV_NewChaseDir( ent, goal, dist );
	}
}


// g-cont: my stupid callbacks
cmodel_t *SV_GetModelPtr( edict_t *ent )
{
	return pe->RegisterModel( sv.configstrings[CS_MODELS + (int)ent->progs.sv->modelindex] );
}

float *SV_GetModelVerts( sv_edict_t *ed, int *numvertices )
{
	cmodel_t	*cmod;
	edict_t	*ent;

	ent = PRVM_EDICT_NUM(ed->serialnumber);
	cmod = pe->RegisterModel( sv.configstrings[CS_MODELS + (int)ent->progs.sv->modelindex] );
	if( cmod )
	{
		int i = (int)ent->progs.sv->body;
		i = bound( 0, i, cmod->numbodies ); // make sure what body exist
		
		if( cmod->col[i] )
		{
			*numvertices = cmod->col[i]->numverts;
			return (float *)cmod->col[i]->verts;
		}
	}
	return NULL;
}

void SV_Transform( sv_edict_t *ed, const vec3_t origin, const matrix3x3 matrix )
{
	edict_t	*edict;
	vec3_t	angles;

	if( !ed ) return;
	edict = PRVM_EDICT_NUM( ed->serialnumber );

	Matrix3x3_Transpose( edict->progs.sv->m_pmatrix, matrix );
#if 0
	edict->progs.sv->m_pmatrix[0][0] = matrix[0][0];
	edict->progs.sv->m_pmatrix[0][1] = matrix[0][2];
	edict->progs.sv->m_pmatrix[0][2] = matrix[0][1];
	edict->progs.sv->m_pmatrix[1][0] = matrix[1][0];
	edict->progs.sv->m_pmatrix[1][1] = matrix[1][2];
	edict->progs.sv->m_pmatrix[1][2] = matrix[1][1];
	edict->progs.sv->m_pmatrix[2][0] = matrix[2][0];
	edict->progs.sv->m_pmatrix[2][1] = matrix[2][2];
	edict->progs.sv->m_pmatrix[2][2] = matrix[2][1];

	Matrix3x3_ConcatRotate( edict->progs.sv->m_pmatrix, -90, 1, 0, 0 );
	Matrix3x3_ConcatRotate( edict->progs.sv->m_pmatrix, 180, 0, 1, 0 );
	Matrix3x3_ConcatRotate( edict->progs.sv->m_pmatrix, 90, 0, 0, 1 );
	Matrix3x3_ToAngles( edict->progs.sv->m_pmatrix, angles );
#endif
	VectorCopy( origin, edict->progs.sv->origin );

	MatrixAngles( edict->progs.sv->m_pmatrix, angles );
	edict->progs.sv->angles[0] = angles[0];
	edict->progs.sv->angles[1] = angles[1];
	edict->progs.sv->angles[2] = angles[2];

	// refresh force and torque
	pe->GetForce( ed->physbody, edict->progs.sv->velocity, edict->progs.sv->avelocity, edict->progs.sv->force, edict->progs.sv->torque );
	pe->GetMassCentre( ed->physbody, edict->progs.sv->m_pcentre );
}

/*
==============
SV_ClientMove

grab user cmd from player state
send it to transform callback
==============
*/
void SV_PlayerMove( sv_edict_t *ed )
{
	pmove_t		pm;
	sv_client_t	*client;
	edict_t		*player;

	client = ed->client;
	player = PRVM_PROG_TO_EDICT( ed->serialnumber );
	memset( &pm, 0, sizeof(pm) );

	if( player->progs.sv->movetype == MOVETYPE_NOCLIP )
		player->priv.sv->s.pm_type = PM_SPECTATOR;
	else player->priv.sv->s.pm_type = PM_NORMAL;
	player->priv.sv->s.gravity = sv_gravity->value;

	if( player->progs.sv->teleport_time )
		player->priv.sv->s.pm_flags |= PMF_TIME_TELEPORT; 
	else player->priv.sv->s.pm_flags &= ~PMF_TIME_TELEPORT; 

	pm.ps = player->priv.sv->s;
	pm.cmd = client->lastcmd;
	pm.body = ed->physbody;	// member body ptr
	
	VectorCopy( player->progs.sv->origin, pm.ps.origin );
	VectorCopy( player->progs.sv->velocity, pm.ps.velocity );

	pe->PlayerMove( &pm, false );	// server move

	// save results of pmove
	player->priv.sv->s = pm.ps;

	VectorCopy(pm.ps.origin, player->progs.sv->origin);
	VectorCopy(pm.ps.velocity, player->progs.sv->velocity);
	VectorCopy(pm.mins, player->progs.sv->mins);
	VectorCopy(pm.maxs, player->progs.sv->maxs);
	VectorCopy(pm.ps.viewangles, player->priv.sv->s.viewangles );
}

void SV_PlaySound( sv_edict_t *ed, float volume, const char *sample )
{
	float	vol = bound( 0.0f, volume/255.0f, 255.0f );
	int	sound_idx = SV_SoundIndex( sample );
	edict_t	*ent;
	if( !ed ) ed = prog->edicts->priv.sv;
	ent = PRVM_PROG_TO_EDICT( ed->serialnumber );

	//SV_StartSound( ent->progs.sv->origin, ent, CHAN_BODY, sound_idx, vol, 1.0f, 0 );
}