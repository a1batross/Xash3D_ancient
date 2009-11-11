//=======================================================================
//			Copyright XashXT Group 2008 ©
//		   cl_physics.c - client physic and prediction
//=======================================================================

#include "common.h"
#include "client.h"
#include "matrix_lib.h"
#include "const.h"

/*
===================
CL_CheckPredictionError
===================
*/
void CL_CheckPredictionError( void )
{
	int	frame;
	int	delta[3];
	int	len;

	if( !cl_predict->integer ) return;

	// calculate the last usercmd_t we sent that the server has processed
	frame = cls.netchan.incoming_acknowledged;
	frame &= (CMD_BACKUP-1);

	// compare what the server returned with what we had predicted it to be
	VectorSubtract (cl.frame.ps.origin, cl.predicted_origins[frame], delta);

	// save the prediction error for interpolation
	len = abs(delta[0]) + abs(delta[1]) + abs(delta[2]);
	if( len > 640 ) // 80 world units
	{	// a teleport or something
		VectorClear (cl.prediction_error);
	}
	else
	{
		if (cl_showmiss->value && (delta[0] || delta[1] || delta[2]))
			Msg ("prediction miss on %i: %i\n", cl.frame.serverframe, delta[0] + delta[1] + delta[2]);

		VectorCopy (cl.frame.ps.origin, cl.predicted_origins[frame]);

		// save for error itnerpolation
		VectorCopy( delta, cl.prediction_error );
	}
}

/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/
int CL_ContentsMask( const edict_t *passedict )
{
	if( passedict )
	{
		if( passedict->v.flags & FL_MONSTER )
			return MASK_MONSTERSOLID;
		else if( passedict->v.flags & FL_CLIENT )
			return MASK_PLAYERSOLID;
		else if( passedict->v.solid == SOLID_TRIGGER )
			return BASECONT_SOLID|BASECONT_BODY;
		return MASK_SOLID;
	}
	return MASK_SOLID;
}

/*
================
CL_CheckVelocity
================
*/
void CL_CheckVelocity( edict_t *ent )
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
	if( wishspeed > ( clgame.movevars.maxvelocity * clgame.movevars.maxvelocity ))
	{
		wishspeed = clgame.movevars.maxvelocity / com.sqrt( wishspeed );
		ent->v.velocity[0] *= wishspeed;
		ent->v.velocity[1] *= wishspeed;
		ent->v.velocity[2] *= wishspeed;
	}
}

/*
=============
CL_CheckWater
=============
*/
bool CL_CheckWater( edict_t *ent )
{
	int	cont;
	vec3_t	point;

	point[0] = ent->v.origin[0];
	point[1] = ent->v.origin[1];
	point[2] = ent->v.origin[2] + ent->v.mins[2] + 1;

	ent->v.waterlevel = 0;
	ent->v.watertype = BASECONT_NONE;
	cont = CL_PointContents( point );

	if( cont & (MASK_WATER))
	{
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
		point[2] = ent->v.origin[2] + (ent->v.mins[2] + ent->v.maxs[2]) * 0.5;
		if( CL_PointContents( point ) & MASK_WATER )
		{
			ent->v.waterlevel = 2;
			point[2] = ent->v.origin[2] + ent->v.view_ofs[2];
			if( CL_PointContents( point ) & MASK_WATER )
			{
				ent->v.waterlevel = 3;
			}
		}
	}
	return ent->v.waterlevel > 1;
}

/*
=================
CL_PredictMovement

Sets cl.predicted_origin and cl.predicted_angles
=================
*/
void CL_PredictMovement (void)
{
	int		ack, current;
	int		frame;
	int		oldframe;
	entity_state_t	pmove;
	usercmd_t		*cmd;
	int		i;
	float		step;
	float		oldz;

	if( cls.state != ca_active ) return;
	if( cl.refdef.paused ) return;

	pmove = EDICT_NUM( cl.playernum + 1 )->pvClientData->current;

	if( cls.demoplayback )
	{
		// interpolate server values
		for( i = 0; i < 3; i++ )
			cl.refdef.cl_viewangles[i] = EDICT_NUM( cl.refdef.viewentity )->v.viewangles[i];
	}

	// unpredicted pure angled values converted into axis
	AngleVectors( cl.refdef.cl_viewangles, cl.refdef.forward, cl.refdef.right, cl.refdef.up );

	if( !cl_predict->value || cl.frame.ps.ed_flags & ESF_NO_PREDICTION )
	{	
		// just set angles
		for( i = 0; i < 3; i++ )
			cl.predicted_angles[i] = cl.refdef.cl_viewangles[i];
		return;
	}

	ack = cls.netchan.incoming_acknowledged;
	current = cls.netchan.outgoing_sequence;

	// if we are too far out of date, just freeze
	if( current - ack >= CMD_BACKUP )
	{
		if( cl_showmiss->value )
			Msg( "exceeded CMD_BACKUP\n" );
		return;	
	}

//	SCR_DebugGraph (current - ack - 1, COLOR_0);

	frame = 0;

	// run frames
	while( ++ack < current )
	{
		frame = ack & (CMD_BACKUP-1);
		cmd = &cl.cmds[frame];

		// call PM_PlayerMove here

		// save for debug checking
		VectorCopy( pmove.origin, cl.predicted_origins[frame] );
	}

	oldframe = (ack-2) & (CMD_BACKUP-1);

	if( pmove.flags & FL_ONGROUND )
	{
		oldz = cl.predicted_origins[oldframe][2];
		step = pmove.origin[2] - oldz;
		if( step > 63 && step < 160 )
		{
			cl.predicted_step = step;
			cl.predicted_step_time = cls.realtime - cls.frametime * 500;
		}
	}

	// copy results out for rendering
	VectorCopy( pmove.origin, cl.predicted_origin );
	VectorCopy( pmove.viewangles, cl.predicted_angles );
}
