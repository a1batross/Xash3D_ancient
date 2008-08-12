//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        cl_frame.c - client world snapshot
//=======================================================================

#include "common.h"
#include "client.h"

/*
=========================================================================

FRAME PARSING

=========================================================================
*/
void CL_UpdateEntityFileds( edict_t *ent )
{
	// copy state to progs
	ent->progs.cl->classname = cl.edict_classnames[ent->priv.cl->current.classname];
	ent->progs.cl->modelindex = ent->priv.cl->current.model.index;
	ent->progs.cl->soundindex = ent->priv.cl->current.soundindex;
	ent->progs.cl->model = PRVM_SetEngineString( cl.configstrings[CS_MODELS+ent->priv.cl->current.model.index] ); 
	VectorCopy( ent->priv.cl->current.origin, ent->progs.cl->origin );
	VectorCopy( ent->priv.cl->current.angles, ent->progs.cl->angles );
}

/*
==================
CL_DeltaEntity

Parses deltas from the given base and adds the resulting entity
to the current frame
==================
*/
void CL_DeltaEntity( sizebuf_t *msg, frame_t *frame, int newnum, entity_state_t *old, bool unchanged )
{
	edict_t		*ent;
	entity_state_t	*state;

	ent = PRVM_EDICT_NUM( newnum );
	state = &cl_parse_entities[cl.parse_entities & (MAX_PARSE_ENTITIES-1)];

	if( unchanged )
	{
		*state = *old;
	}
	else
	{
		MSG_ReadDeltaEntity( msg, old, state, newnum );
	}

	if( state->number == -1 ) return; // entity was delta removed

	cl.parse_entities++;
	frame->num_entities++;

	// some data changes will force no lerping
	if( state->model.index != ent->priv.cl->current.model.index || state->pmodel.index != ent->priv.cl->current.pmodel.index || state->model.body != ent->priv.cl->current.model.body
		|| state->model.sequence != ent->priv.cl->current.model.sequence || abs(state->origin[0] - ent->priv.cl->current.origin[0]) > 512
		|| abs(state->origin[1] - ent->priv.cl->current.origin[1]) > 512 || abs(state->origin[2] - ent->priv.cl->current.origin[2]) > 512 )
	{
		ent->priv.cl->serverframe = -99;
	}

	if( ent->priv.cl->serverframe != cl.frame.serverframe - 1 )
	{	
		// duplicate the current state so lerping doesn't hurt anything
		ent->priv.cl->prev = *state;
		VectorCopy (state->old_origin, ent->priv.cl->prev.origin);
	}
	else
	{	// shuffle the last state to previous
		ent->priv.cl->prev = ent->priv.cl->current;
	}

	ent->priv.cl->serverframe = cl.frame.serverframe;
	ent->priv.cl->current = *state;

	// update prvm fields
	CL_UpdateEntityFileds( ent );
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
void CL_ParsePacketEntities( sizebuf_t *msg, frame_t *oldframe, frame_t *newframe )
{
	int		newnum;
	entity_state_t	*oldstate;
	int		oldindex, oldnum;

	newframe->parse_entities = cl.parse_entities;
	newframe->num_entities = 0;

	// delta from the entities present in oldframe
	oldindex = 0;
	oldstate = NULL;
	if( !oldframe ) oldnum = MAX_ENTNUMBER;
	else
	{
		if( oldindex >= oldframe->num_entities )
		{
			oldnum = MAX_ENTNUMBER;
		}
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}

	while( 1 )
	{
		// read the entity index number
		newnum = MSG_ReadShort( msg );
		if( !newnum ) break; // end of packet entities

		if( msg->readcount > msg->cursize )
			Host_Error("CL_ParsePacketEntities: end of message[%d > %d]\n", msg->readcount, msg->cursize );

		while( oldnum < newnum )
		{	
			// one or more entities from the old packet are unchanged
			CL_DeltaEntity( msg, newframe, oldnum, oldstate, true );
			
			oldindex++;

			if( oldindex >= oldframe->num_entities )
			{
				oldnum = MAX_ENTNUMBER;
			}
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
		}
		if( oldnum == newnum )
		{	
			// delta from previous state
			CL_DeltaEntity( msg, newframe, newnum, oldstate, false );
			oldindex++;

			if( oldindex >= oldframe->num_entities )
			{
				oldnum = MAX_ENTNUMBER;
			}
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if( oldnum > newnum )
		{	
			// delta from baseline
			edict_t *ent = PRVM_EDICT_NUM( newnum );
			CL_DeltaEntity( msg, newframe, newnum, &ent->priv.cl->baseline, false );
			continue;
		}

	}

	// any remaining entities in the old frame are copied over
	while( oldnum != MAX_ENTNUMBER )
	{	
		// one or more entities from the old packet are unchanged
		CL_DeltaEntity( msg, newframe, oldnum, oldstate, true );
		oldindex++;

		if( oldindex >= oldframe->num_entities )
		{
			oldnum = MAX_ENTNUMBER;
		}
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}
}

/*
================
CL_ParseFrame
================
*/
void CL_ParseFrame( sizebuf_t *msg )
{
	int     		cmd, len;
	frame_t		*old;
          
	memset( &cl.frame, 0, sizeof(cl.frame));
	cl.frame.serverframe = MSG_ReadLong( msg );
	cl.frame.deltaframe = MSG_ReadLong( msg );
	cl.frame.servertime = cl.frame.serverframe * Host_FrameTime();
	cl.surpressCount = MSG_ReadByte( msg );

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message 
	if( cl.frame.deltaframe <= 0 )
	{
		cl.frame.valid = true;	// uncompressed frame
		cls.demowaiting = false;	// we can start recording now
		old = NULL;
	}
	else
	{
		old = &cl.frames[cl.frame.deltaframe & UPDATE_MASK];
		if( !old->valid )
		{	
			// should never happen
			MsgDev( D_INFO, "delta from invalid frame (not supposed to happen!).\n" );
		}
		if( old->serverframe != cl.frame.deltaframe )
		{	
			// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			MsgDev( D_INFO, "Delta frame too old.\n" );
		}
		else if( cl.parse_entities - old->parse_entities > MAX_PARSE_ENTITIES - 128 )
		{
			MsgDev( D_INFO, "delta parse_entities too old.\n" );
		}
		else cl.frame.valid = true;	// valid delta parse
	}

	// clamp time 
	if( cl.time > cl.frame.servertime ) cl.time = cl.frame.servertime;
	else if( cl.time < cl.frame.servertime - Host_FrameTime())
		cl.time = cl.frame.servertime - Host_FrameTime();

	// read areabits
	len = MSG_ReadByte( msg );
	MSG_ReadData( msg, &cl.frame.areabits, len );

#if 0
	// read clientindex
	cmd = MSG_ReadByte( msg );
	if( cmd != svc_playerinfo ) Host_Error( "CL_ParseFrame: not clientindex\n" );
	idx = MSG_ReadByte( msg );
	clent = PRVM_EDICT_NUM( idx ); // get client
	if((idx-1) != cl.playernum ) Host_Error("CL_ParseFrame: invalid playernum (%d should be %d)\n", idx-1, cl.playernum );
#else
	// read playerinfo
	cmd = MSG_ReadByte( msg );
	if( cmd != svc_playerinfo ) Host_Error( "CL_ParseFrame: not playerinfo\n" );
	if( old ) MSG_ReadDeltaPlayerstate( msg, &old->ps, &cl.frame.ps );
	else MSG_ReadDeltaPlayerstate( msg, NULL, &cl.frame.ps );
#endif
	// read packet entities
	cmd = MSG_ReadByte( msg );
	if( cmd != svc_packetentities ) Host_Error("CL_ParseFrame: not packetentities[%d]\n", cmd );
	CL_ParsePacketEntities( msg, old, &cl.frame );
#if 0
	// now we can reading delta player state
	if( old ) cl.frame.ps = MSG_ParseDeltaPlayer( &old->ps, &clent->priv.cl->current );
	else cl.frame.ps = MSG_ParseDeltaPlayer( NULL, &clent->priv.cl->current );
#endif
	// HACKHACK
	if( cls.state == ca_cinematic || cls.demoplayback )
		cl.frame.ps.pm_type = PM_FREEZE; // demo or movie playback

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.serverframe & UPDATE_MASK] = cl.frame;

	if( cl.frame.valid )
	{
		if( cls.state != ca_active )
		{
			cls.state = ca_active;
			cl.force_refdef = true;
			// getting a valid frame message ends the connection process
			VectorCopy( cl.frame.ps.origin, cl.predicted_origin );
			VectorCopy( cl.frame.ps.viewangles, cl.predicted_angles );
		}
		CL_CheckPredictionError();
	}
}

/*
==========================================================================

INTERPOLATE BETWEEN FRAMES TO GET RENDERING PARMS

==========================================================================
*/
/*
===============
CL_AddPacketEntities

===============
*/
void CL_AddPacketEntities( frame_t *frame )
{
	entity_state_t	*s1;
	edict_t		*ent;
	int		pnum;

	for( pnum = 0; pnum < frame->num_entities; pnum++ )
	{
		s1 = &cl_parse_entities[(frame->parse_entities + pnum)&(MAX_PARSE_ENTITIES-1)];
		ent = PRVM_EDICT_NUM( s1->number );
		re->AddRefEntity( &cl.refdef, &ent->priv.cl->current, &ent->priv.cl->prev, cl.lerpfrac );
	}
}



/*
==============
CL_AddViewWeapon
==============
*/
void CL_AddViewWeapon( entity_state_t *ps )
{
	edict_t		*view;	// view model

	// allow the gun to be completely removed
	if( !cl_gun->value ) return;

	// don't draw gun if in wide angle view
	if( ps->fov > 135 ) return;

	view = PRVM_EDICT_NUM( ps->aiment );
	VectorCopy( cl.refdef.vieworg, view->priv.cl->current.origin );
	VectorCopy( cl.refdef.viewangles, view->priv.cl->current.angles );
	VectorCopy( cl.refdef.vieworg, view->priv.cl->prev.origin );
	VectorCopy( cl.refdef.viewangles, view->priv.cl->prev.angles );
	re->AddRefEntity( &cl.refdef, &view->priv.cl->current, &view->priv.cl->prev, cl.lerpfrac );
}


/*
===============
CL_CalcViewValues

Sets cl.refdef view values
===============
*/
void CL_CalcViewValues( void )
{
	int		i;
	float		lerp, backlerp;
	frame_t		*oldframe;
	entity_state_t	*ps, *ops;

	// clamp time
	if( cl.time > cl.frame.servertime )
	{
		if( cl_showclamp->value )
			Msg ("high clamp %i\n", cl.time - cl.frame.servertime);
		cl.time = cl.frame.servertime;
		cl.lerpfrac = 1.0f;
	}
	else if (cl.time < cl.frame.servertime - Host_FrameTime())
	{
		if (cl_showclamp->value)
			Msg( "low clamp %i\n", cl.frame.servertime - Host_FrameTime() - cl.time);
		cl.time = cl.frame.servertime - Host_FrameTime();
		cl.lerpfrac = 0.0f;
	}
	else cl.lerpfrac = 1.0 - (cl.frame.servertime - cl.time) * 0.01f;

	// find the previous frame to interpolate from
	ps = &cl.frame.ps;
	i = (cl.frame.serverframe - 1) & UPDATE_MASK;
	oldframe = &cl.frames[i];
	if( oldframe->serverframe != cl.frame.serverframe-1 || !oldframe->valid )
		oldframe = &cl.frame; // previous frame was dropped or invalid
	ops = &oldframe->ps;

	// see if the player entity was teleported this frame
	if( ps->pm_flags & PMF_TIME_TELEPORT )
		ops = ps;	// don't interpolate
	lerp = cl.lerpfrac;

	// calculate the origin
	if((cl_predict->value) && !(cl.frame.ps.pm_flags & PMF_NO_PREDICTION) && !cls.demoplayback )
	{	
		// use predicted values
		int	delta;

		backlerp = 1.0 - lerp;
		for( i = 0; i < 3; i++ )
		{
			cl.refdef.vieworg[i] = cl.predicted_origin[i] + ops->viewoffset[i] 
				+ cl.lerpfrac * (ps->viewoffset[i] - ops->viewoffset[i]) - backlerp * cl.prediction_error[i];
		}

		// smooth out stair climbing
		delta = cls.realtime - cl.predicted_step_time;
		if( delta < Host_FrameTime()) cl.refdef.vieworg[2] -= cl.predicted_step * (Host_FrameTime() - delta) * 0.01f;
	}
	else
	{
		// just use interpolated values
		for( i = 0; i < 3; i++ )
			cl.refdef.vieworg[i] = LerpView( ops->origin[i], ps->origin[i], ops->viewoffset[i], ps->viewoffset[i], lerp );
	}

	// if not running a demo or on a locked frame, add the local angle movement
	if( cls.demoplayback )
	{
		for (i = 0; i < 3; i++)
			cl.refdef.viewangles[i] = LerpAngle( ops->viewangles[i], ps->viewangles[i], lerp );
	}
	else
	{	
		// in-game use predicted values
		for (i = 0; i < 3; i++) cl.refdef.viewangles[i] = cl.predicted_angles[i];
	}

	for( i = 0; i < 3; i++ )
		cl.refdef.viewangles[i] += LerpAngle( ops->punch_angles[i], ps->punch_angles[i], lerp );

	AngleVectors( cl.refdef.viewangles, cl.v_forward, cl.v_right, cl.v_up );

	// interpolate field of view
	cl.refdef.fov_x = ops->fov + lerp * ( ps->fov - ops->fov );

	// add the weapon
	CL_AddViewWeapon( ps );
}

/*
===============
CL_AddEntities

Emits all entities, particles, and lights to the refresh
===============
*/
void CL_AddEntities( void )
{
	if( cls.state != ca_active )
		return;

	CL_CalcViewValues();
	CL_AddPacketEntities( &cl.frame );
	CL_AddParticles();
	CL_AddDLights();
	CL_AddLightStyles();
}

//
// sound engine implementation
//

void CL_GetEntitySoundSpatialization( int entnum, vec3_t origin, vec3_t velocity )
{
	edict_t		*ent;
	cmodel_t		*cmodel;
	vec3_t		midPoint;

	if( entnum < 0 || entnum >= host.max_edicts )
	{
		MsgDev( D_ERROR, "CL_GetEntitySoundSpatialization: invalid entnum %d\n", entnum );
		VectorCopy( vec3_origin, origin );
		VectorCopy( vec3_origin, velocity );
		return;
	}

	ent = PRVM_EDICT_NUM( entnum );

	// calculate origin
	origin[0] = ent->priv.cl->prev.origin[0] + (ent->priv.cl->current.origin[0] - ent->priv.cl->prev.origin[0]) * cl.lerpfrac;
	origin[1] = ent->priv.cl->prev.origin[1] + (ent->priv.cl->current.origin[1] - ent->priv.cl->prev.origin[1]) * cl.lerpfrac;
	origin[2] = ent->priv.cl->prev.origin[2] + (ent->priv.cl->current.origin[2] - ent->priv.cl->prev.origin[2]) * cl.lerpfrac;

	// calculate velocity
	VectorSubtract( ent->priv.cl->current.origin, ent->priv.cl->prev.origin, velocity);
	VectorScale(velocity, 10, velocity);

	// if a brush model, offset the origin
	if( VectorIsNull( origin ))
	{
		cmodel = cl.models[ent->priv.cl->current.model.index];
		if(!cmodel) return;
		VectorAverage( cmodel->mins, cmodel->maxs, midPoint );
		VectorAdd( origin, midPoint, origin );
	}
}

/*
=================
S_AddLoopingSounds

Entities with a sound field will generate looping sounds that are
automatically started and stopped as the entities are sent to the
client
=================
*/
void CL_AddLoopingSounds( void )
{
	entity_state_t	*ent;
	int		num, i;

	if( cls.state != ca_active ) return;
	if( cl_paused->integer ) return;
	if( !cl.audio_prepped ) return;

	for( i = 0; i < cl.frame.num_entities; i++ )
	{
		num = (cl.frame.parse_entities + i)&(MAX_PARSE_ENTITIES-1);
		ent = &cl_parse_entities[num];
		if(!ent->soundindex) continue;
		se->AddLoopingSound( ent->number, cl.sound_precache[ent->soundindex], 1.0f, ATTN_IDLE );
	}
}