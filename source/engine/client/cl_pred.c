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
#include "matrixlib.h"
#include "const.h"

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
==================
CL_Trace
==================
*/
trace_t CL_Trace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int type, edict_t *passedict, int contentsmask )
{
	vec3_t		hullmins, hullmaxs;
	int		i, bodycontents;
	int		passedictprog;
	bool		pointtrace;
	edict_t		*traceowner, *touch;
	trace_t		trace;
	vec3_t		clipboxmins, clipboxmaxs;	// bounding box of entire move area
	vec3_t		clipmins, clipmaxs;		// size of the moving object
	vec3_t		clipmins2, clipmaxs2;	// size when clipping against monsters
	vec3_t		clipstart, clipend;		// start and end origin of move
	trace_t		cliptrace;		// trace results
	matrix4x4		matrix, imatrix;		// matrices to transform into/out of other entity's space
	cmodel_t		*model;			// model of other entity
	int		numtouchedicts = 0;		// list of entities to test for collisions
	edict_t		*touchedicts[MAX_EDICTS];

	VectorCopy( start, clipstart );
	VectorCopy( end, clipend );
	VectorCopy( mins, clipmins );
	VectorCopy( maxs, clipmaxs );
	VectorCopy( mins, clipmins2 );
	VectorCopy( maxs, clipmaxs2 );

	// clip to world
	pe->ClipToWorld( &cliptrace, cl.worldmodel, clipstart, clipmins, clipmaxs, clipend, contentsmask );
	cliptrace.startstuck = cliptrace.startsolid;
	if( cliptrace.startsolid || cliptrace.fraction < 1 ) cliptrace.ent = prog ? prog->edicts : NULL;
	if( type == MOVE_WORLDONLY ) return cliptrace;

	if( type == MOVE_MISSILE )
	{
		for( i = 0; i < 3; i++ )
		{
			clipmins2[i] -= 15;
			clipmaxs2[i] += 15;
		}
	}

	// get adjusted box for bmodel collisions if the world is q1bsp or hlbsp
	VectorCopy( clipmins, hullmins );
	VectorCopy( clipmaxs, hullmaxs );

	// create the bounding box of the entire move
	for( i = 0; i < 3; i++ )
	{
		clipboxmins[i] = min(clipstart[i], cliptrace.endpos[i]) + min(hullmins[i], clipmins2[i]) - 1;
		clipboxmaxs[i] = max(clipstart[i], cliptrace.endpos[i]) + max(hullmaxs[i], clipmaxs2[i]) + 1;
	}

	// if the passedict is world, make it NULL (to avoid two checks each time)
	// this checks prog because this function is often called without a CSQC
	// VM context
	if( prog == NULL || passedict == prog->edicts ) passedict = NULL;
	// precalculate prog value for passedict for comparisons
	passedictprog = prog != NULL ? PRVM_EDICT_TO_PROG(passedict) : 0;
	// figure out whether this is a point trace for comparisons
	pointtrace = VectorCompare( clipmins, clipmaxs );
	// precalculate passedict's owner edict pointer for comparisons
	traceowner = passedict ? PRVM_PROG_TO_EDICT( passedict->progs.cl->owner ) : NULL;

	// clip to entities
	// because this uses World_EntitiestoBox, we know all entity boxes overlap
	// the clip region, so we can skip culling checks in the loop below
	// note: if prog is NULL then there won't be any linked entities
	numtouchedicts = 0;//CL_AreaEdicts( clipboxmins, clipboxmaxs, touchedicts, host.max_edicts );
	if( numtouchedicts > host.max_edicts )
	{
		// this never happens
		MsgDev( D_WARN, "CL_AreaEdicts returned %i edicts, max was %i\n", numtouchedicts, host.max_edicts );
		numtouchedicts = host.max_edicts;
	}
	for( i = 0; i < numtouchedicts; i++ )
	{
		touch = touchedicts[i];

		if( touch->progs.cl->solid < SOLID_BBOX ) continue;
		if( type == MOVE_NOMONSTERS && touch->progs.cl->solid != SOLID_BSP )
			continue;

		if( passedict )
		{
			// don't clip against self
			if( passedict == touch ) continue;
			// don't clip owned entities against owner
			if( traceowner == touch ) continue;
			// don't clip owner against owned entities
			if( passedictprog == touch->progs.cl->owner ) continue;
			// don't clip points against points (they can't collide)
			if( pointtrace && VectorCompare( touch->progs.cl->mins, touch->progs.cl->maxs) && (type != MOVE_MISSILE || !((int)touch->progs.cl->flags & FL_MONSTER)))
				continue;
		}

		bodycontents = CONTENTS_BODY;

		// might interact, so do an exact clip
		model = NULL;
		if((int)touch->progs.cl->solid == SOLID_BSP || type == MOVE_HITMODEL )
		{
			uint modelindex = (uint)touch->progs.cl->modelindex;
			// if the modelindex is 0, it shouldn't be SOLID_BSP!
			if( modelindex > 0 && modelindex < MAX_MODELS )
				model = cl.models[(int)touch->progs.cl->modelindex];
		}
		if( model ) Matrix4x4_CreateFromEntity( matrix, touch->progs.cl->origin[0], touch->progs.cl->origin[1], touch->progs.cl->origin[2], touch->progs.cl->angles[0], touch->progs.cl->angles[1], touch->progs.cl->angles[2], 1 );
		else Matrix4x4_CreateTranslate( matrix, touch->progs.cl->origin[0], touch->progs.cl->origin[1], touch->progs.cl->origin[2] );
		Matrix4x4_Invert_Simple( imatrix, matrix );
		if((int)touch->progs.cl->flags & FL_MONSTER)
			pe->ClipToGenericEntity(&trace, model, touch->progs.cl->mins, touch->progs.cl->maxs, bodycontents, matrix, imatrix, clipstart, clipmins2, clipmaxs2, clipend, contentsmask );
		else pe->ClipToGenericEntity(&trace, model, touch->progs.cl->mins, touch->progs.cl->maxs, bodycontents, matrix, imatrix, clipstart, clipmins, clipmaxs, clipend, contentsmask );
		pe->CombineTraces(&cliptrace, &trace, touch, touch->progs.cl->solid == SOLID_BSP );
	}
	return cliptrace;
}


/*
====================
CL_ClipMoveToEntities

====================
*/
void CL_ClipMoveToEntities ( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, trace_t *tr )
{
/*
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
}
*/
}


/*
================
CL_PMTrace
================
*/
void CL_PMTrace( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, trace_t *tr )
{
	*tr = CL_Trace( start, mins, maxs, end, MOVE_NORMAL, NULL, CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_BODY );
}

int CL_PointContents( vec3_t point )
{
	// get world supercontents at this point
	if( cl.worldmodel && cl.worldmodel->PointContents )
		return cl.worldmodel->PointContents( point, cl.worldmodel );
	return 0;
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
	pm.pointcontents = CL_PointContents;
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
			cl.predicted_step = step;
			cl.predicted_step_time = cls.realtime - cls.frametime * 500;
		}
	}

	// copy results out for rendering
	VectorCopy( pm.ps.origin, cl.predicted_origin );
	VectorCopy(pm.ps.viewangles, cl.predicted_angles);
}
