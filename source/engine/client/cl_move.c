//=======================================================================
//			Copyright XashXT Group 2008 ©
//		   cl_physics.c - client physic and prediction
//=======================================================================

#include "common.h"
#include "client.h"
#include "matrix_lib.h"
#include "const.h"
#include "pm_defs.h"

/*
===================
CL_CheckPredictionError
===================
*/
void CL_CheckPredictionError( void )
{
	int	frame;
	vec3_t	delta;
	edict_t	*player;

	if( !cl_predict->integer ) return;

	player = CL_GetLocalPlayer();

	// calculate the last usercmd_t we sent that the server has processed
	frame = cls.netchan.incoming_acknowledged;
	frame &= CMD_MASK;

	// compare what the server returned with what we had predicted it to be
	VectorSubtract( player->v.origin, cl.predicted_origins[frame], delta );

	if( player->pvClientData->current.ed_flags & ESF_NO_PREDICTION )
	{	
		// a teleport or something
		VectorClear( cl.prediction_error );
	}
	else
	{
		if( cl_showmiss->integer && ( delta[0] || delta[1] || delta[2] ))
			Msg( "prediction miss on %i: %g\n", cl.frame.serverframe, delta[0] + delta[1] + delta[2]);

		VectorCopy( player->v.origin, cl.predicted_origins[frame] );

		// save for error itnerpolation
		VectorCopy( delta, cl.prediction_error );
	}
}

/*
=================
CL_PredictMovement

Sets cl.predicted_origin and cl.predicted_angles
=================
*/
void CL_PredictMovement( void )
{
	int		ack, current;
	int		frame;
	int		oldframe;
	usercmd_t		*cmd;
	edict_t		*player, *viewent;
	int		i;
	float		step;
	float		oldz;

	if( cls.state != ca_active ) return;
	if( cl.refdef.paused ) return;

	player = CL_GetLocalPlayer ();
	viewent = CL_GetEdictByIndex( cl.refdef.viewentity );

	if( cls.demoplayback )
	{
		// interpolate server values
		for( i = 0; viewent && i < 3; i++ )
			cl.refdef.cl_viewangles[i] = viewent->v.viewangles[i];
	}

	// unpredicted pure angled values converted into axis
	AngleVectors( cl.refdef.cl_viewangles, cl.refdef.forward, cl.refdef.right, cl.refdef.up );

	if( !cl_predict->value || player->pvClientData->current.ed_flags & ESF_NO_PREDICTION )
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

	frame = 0;

	// run frames
	while( ++ack < current )
	{
		frame = ack & CMD_MASK;
		cmd = &cl.cmds[frame];

		// call pfnPM_Move here

		// save for debug checking
		VectorCopy( clgame.pmove->origin, cl.predicted_origins[frame] );
	}

	oldframe = (ack- 2 ) & CMD_MASK;

	if( player->v.flags & FL_ONGROUND )
	{
		oldz = cl.predicted_origins[oldframe][2];
		step = clgame.pmove->origin[2] - oldz;

		if( step > 63 && step < 160 )
		{
			cl.predicted_step = step;
			cl.predicted_step_time = cls.realtime - cls.frametime * 500;
		}
	}

	// copy results out for rendering
	VectorCopy( clgame.pmove->origin, cl.predicted_origin );
	VectorCopy( clgame.pmove->angles, cl.predicted_angles );
}
