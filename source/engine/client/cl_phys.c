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
			return CONTENTS_SOLID|CONTENTS_BODY;
		return MASK_SOLID;
	}
	return MASK_SOLID;
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
	if( cliptrace.startsolid || cliptrace.fraction < 1 )
		cliptrace.ent = (edict_t *)EDICT_NUM( 0 );
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
	if( passedict == EDICT_NUM( 0 )) passedict = NULL;
	// figure out whether this is a point trace for comparisons
	pointtrace = VectorCompare( clipmins, clipmaxs );
	// precalculate passedict's owner edict pointer for comparisons
	if( passedict && passedict->v.owner )
		traceowner = passedict->v.owner;
	else traceowner = NULL;

	// clip to entities
	// because this uses World_EntitiestoBox, we know all entity boxes overlap
	// the clip region, so we can skip culling checks in the loop below
	numtouchedicts = 0;// FIXME: CL_AreaEdicts( clipboxmins, clipboxmaxs, touchedicts, host.max_edicts );
	if( numtouchedicts > host.max_edicts )
	{
		// this never happens
		MsgDev( D_WARN, "CL_AreaEdicts returned %i edicts, max was %i\n", numtouchedicts, host.max_edicts );
		numtouchedicts = host.max_edicts;
	}
	for( i = 0; i < numtouchedicts; i++ )
	{
		touch = touchedicts[i];
		if( touch->v.solid < SOLID_BBOX ) continue;
		if( type == MOVE_NOMONSTERS && touch->v.solid != SOLID_BSP )
			continue;

		if( passedict )
		{
			// don't clip against self
			if( passedict == touch ) continue;
			// don't clip owned entities against owner
			if( traceowner == touch ) continue;
			// don't clip owner against owned entities
			if( passedict == touch->v.owner ) continue;
			// don't clip points against points (they can't collide)
			if( pointtrace && VectorCompare( touch->v.mins, touch->v.maxs ) && (type != MOVE_MISSILE || !(touch->v.flags & FL_MONSTER)))
				continue;
		}

		bodycontents = CONTENTS_BODY;

		// might interact, so do an exact clip
		model = NULL;
		if( touch->v.solid == SOLID_BSP || type == MOVE_HITMODEL )
		{
			uint modelindex = (uint)touch->v.modelindex;
			// if the modelindex is 0, it shouldn't be SOLID_BSP!
			if( modelindex > 0 && modelindex < MAX_MODELS )
				model = cl.models[touch->v.modelindex];
		}
		if( model ) Matrix4x4_CreateFromEntity( matrix, touch->v.origin[0], touch->v.origin[1], touch->v.origin[2], touch->v.angles[0], touch->v.angles[1], touch->v.angles[2], 1 );
		else Matrix4x4_CreateTranslate( matrix, touch->v.origin[0], touch->v.origin[1], touch->v.origin[2] );
		Matrix4x4_Invert_Simple( imatrix, matrix );
		if( touch->v.flags & FL_MONSTER )
			pe->ClipToGenericEntity(&trace, model, touch->v.mins, touch->v.maxs, bodycontents, matrix, imatrix, clipstart, clipmins2, clipmaxs2, clipend, contentsmask );
		else pe->ClipToGenericEntity(&trace, model, touch->v.mins, touch->v.maxs, bodycontents, matrix, imatrix, clipstart, clipmins, clipmaxs, clipend, contentsmask );
		pe->CombineTraces( &cliptrace, &trace, (edict_t *)touch, touch->v.solid == SOLID_BSP );
	}
	return cliptrace;
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
	ent->v.watertype = CONTENTS_NONE;
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

int CL_PointContents( const vec3_t point )
{
	// get world supercontents at this point
	if( cl.worldmodel && cl.worldmodel->PointContents )
		return cl.worldmodel->PointContents( point, cl.worldmodel );
	return 0;
}

bool CL_AmbientLevel( const vec3_t point, float *volumes )
{
	// get world supercontents at this point
	if( cl.worldmodel && cl.worldmodel->AmbientLevel )
		return cl.worldmodel->AmbientLevel( point, volumes, cl.worldmodel );
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
	entvars_t		pmove;
	usercmd_t		*cmd;
	int		i;
	float		step;
	float		oldz;

	if( cls.state != ca_active ) return;
	if( cl_paused->value ) return;

	pmove = EDICT_NUM( cl.playernum + 1 )->v;

	if( !cl_predict->value || cl.frame.ps.ed_flags & ESF_NO_PREDICTION )
	{	
		// just set angles
		for( i = 0; i < 3; i++ )
			cl.predicted_angles[i] = cl.viewangles[i] + SHORT2ANGLE( cl.frame.ps.delta_angles[i] );
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

		pe->PlayerMove( &pmove, cmd, NULL, true );

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
