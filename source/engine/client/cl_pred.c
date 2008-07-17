/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "common.h"
#include "client.h"

/*
===================
CL_CheckPredictionError
===================
*/
void CL_CheckPredictionError (void)
{
	int		frame;
	int		delta[3];
	int		len;

	if (!cl_predict->value || (cl.frame.ps.pm_flags & PMF_NO_PREDICTION))
		return;

	// calculate the last usercmd_t we sent that the server has processed
	frame = cls.netchan.incoming_acknowledged;
	frame &= (CMD_BACKUP-1);

	// compare what the server returned with what we had predicted it to be
	VectorSubtract (cl.frame.ps.origin, cl.predicted_origins[frame], delta);

	// save the prediction error for interpolation
	len = abs(delta[0]) + abs(delta[1]) + abs(delta[2]);
	if( len > 640 * SV_COORD_FRAC ) // 80 world units
	{	// a teleport or something
		VectorClear (cl.prediction_error);
	}
	else
	{
		if (cl_showmiss->value && (delta[0] || delta[1] || delta[2]))
			Msg ("prediction miss on %i: %i\n", cl.frame.serverframe, delta[0] + delta[1] + delta[2]);

		VectorCopy (cl.frame.ps.origin, cl.predicted_origins[frame]);

		// save for error itnerpolation
		VectorScale( delta, CL_COORD_FRAC, cl.prediction_error );
	}
}


/*
====================
CL_ClipMoveToEntities

====================
*/
void CL_ClipMoveToEntities ( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, trace_t *tr )
{
	int		i, x, zd, zu;
	trace_t		trace;
	float		*angles;
	entity_state_t	*ent;
	int		num;
	cmodel_t		*cmodel;
	vec3_t		bmins, bmaxs;

	for( i = 0; i < cl.frame.num_entities; i++ )
	{
		num = (cl.frame.parse_entities + i)&(MAX_PARSE_ENTITIES-1);
		ent = &cl_parse_entities[num];

		if(!ent->solid) continue;
		if(ent->number == cl.playernum + 1) continue;

		if( ent->solid == SOLID_BMODEL )
		{	
			// special value for bmodel
			cmodel = cl.model_clip[ent->modelindex];
			if(!cmodel) continue;
			angles = ent->angles;
		}
		else
		{	// encoded bbox
			x =  (ent->solid & 255);
			zd = ((ent->solid>>8) & 255);
			zu = ((ent->solid>>16) & 255) - 32;

			bmins[0] = bmins[1] = -x;
			bmaxs[0] = bmaxs[1] = x;
			bmins[2] = -zd;
			bmaxs[2] = zu;
			angles = ent->angles;
		}
		if( tr->allsolid ) return;

		trace = pe->TransformedBoxTrace( start, end, mins, maxs, cmodel, MASK_PLAYERSOLID, ent->origin, angles, false );

		if( trace.allsolid || trace.startsolid || trace.fraction < tr->fraction )
		{
			trace.ent = (edict_t *)ent;
		 	if (tr->startsolid)
			{
				*tr = trace;
				tr->startsolid = true;
			}
			else *tr = trace;
		}
		else if (trace.startsolid) tr->startsolid = true;
	}
}


/*
================
CL_PMTrace
================
*/
trace_t CL_PMTrace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	trace_t	t;

	// check against world
	t = pe->BoxTrace( start, end, mins, maxs, NULL, MASK_PLAYERSOLID, true );
	if (t.fraction < 1.0) t.ent = (edict_t *)1;

	// check all other solid models
	CL_ClipMoveToEntities( start, mins, maxs, end, &t );

	return t;
}

int CL_PMpointcontents( vec3_t point )
{
	int		i;
	entity_state_t	*ent;
	int		num;
	cmodel_t		*cmodel;
	int		contents;

	contents = pe->PointContents( point, NULL );

	for( i = 0; i < cl.frame.num_entities; i++ )
	{
		num = (cl.frame.parse_entities + i)&(MAX_PARSE_ENTITIES-1);
		ent = &cl_parse_entities[num];

		if (ent->solid != SOLID_BMODEL) // special value for bmodel
			continue;

		cmodel = cl.model_clip[ent->modelindex];
		if (!cmodel) continue;

		contents |= pe->TransformedPointContents( point, cmodel, ent->origin, ent->angles );
	}
	return contents;
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
	usercmd_t		*cmd;
	pmove_t		pm;
	int		i;
	float		step;
	float		oldz;

	if(cls.state != ca_active) return;
	if(cl_paused->value) return;

	if (!cl_predict->value || (cl.frame.ps.pm_flags & PMF_NO_PREDICTION))
	{	
		// just set angles
		for (i = 0; i < 3; i++)
		{
			cl.predicted_angles[i] = cl.viewangles[i] + SHORT2ANGLE(cl.frame.ps.delta_angles[i]);
		}
		return;
	}

	ack = cls.netchan.incoming_acknowledged;
	current = cls.netchan.outgoing_sequence;

	// if we are too far out of date, just freeze
	if (current - ack >= CMD_BACKUP)
	{
		if (cl_showmiss->value)
			Msg ("exceeded CMD_BACKUP\n");
		return;	
	}

	// copy current state to pmove
	memset (&pm, 0, sizeof(pm));
	pm.trace = CL_PMTrace;
	pm.pointcontents = CL_PMpointcontents;
	pm.ps = cl.frame.ps;

//	SCR_DebugGraph (current - ack - 1, COLOR_0);

	frame = 0;

	// run frames
	while (++ack < current)
	{
		frame = ack & (CMD_BACKUP-1);
		cmd = &cl.cmds[frame];

		pm.cmd = *cmd;
		pe->PlayerMove( &pm, true );

		// save for debug checking
		VectorCopy (pm.ps.origin, cl.predicted_origins[frame]);
	}

	oldframe = (ack-2) & (CMD_BACKUP-1);

	if( pm.ps.pm_flags & PMF_ON_GROUND )
	{
		oldz = cl.predicted_origins[oldframe][2];
		step = pm.ps.origin[2] - oldz;
		if( step > 63 && step < 160 )
		{
			cl.predicted_step = step * CL_COORD_FRAC;
			cl.predicted_step_time = cls.realtime - cls.frametime * 500;
		}
	}

	// copy results out for rendering
	VectorScale( pm.ps.origin, CL_COORD_FRAC, cl.predicted_origin );
	VectorCopy(pm.ps.viewangles, cl.predicted_angles);
}
