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
void CL_UpdateEntityFields( edict_t *ent )
{
	// these fields user can overwrite if need
	ent->v.model = MAKE_STRING( cl.configstrings[CS_MODELS+ent->pvClientData->current.modelindex] );
	VectorCopy( ent->pvClientData->prev.angles, ent->v.oldangles );
	if( ent->pvClientData->current.aiment )
		ent->v.aiment = EDICT_NUM( ent->pvClientData->current.aiment );
	else ent->v.aiment = NULL;

	clgame.dllFuncs.pfnUpdateEntityVars( ent, &cl.refdef, &ent->pvClientData->current );

	// always keep an actual (users can't replace this)
	ent->serialnumber = ent->pvClientData->current.number;
	ent->v.classname = cl.edict_classnames[ent->pvClientData->current.classname];
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

	ent = EDICT_NUM( newnum );
	state = &cl_parse_entities[cl.parse_entities & (MAX_PARSE_ENTITIES-1)];

	if( unchanged ) *state = *old;
	else MSG_ReadDeltaEntity( msg, old, state, newnum );

	if( state->number == -1 )
	{
		// NOTE: not real removed, just out of view, use ent->free as marker
		ent->free = true;
		return; // entity was delta removed
	}
	cl.parse_entities++;
	frame->num_entities++;

	// some data changes will force no lerping
	if( state->ed_flags & ESF_NODELTA )
	{
		ent->pvClientData->serverframe = -99;
	}

	if( ent->pvClientData->serverframe != cl.frame.serverframe - 1 )
	{	
		// duplicate the current state so lerping doesn't hurt anything
		ent->pvClientData->prev = *state;
	}
	else
	{	// shuffle the last state to previous
		ent->pvClientData->prev = ent->pvClientData->current;
	}

	ent->pvClientData->serverframe = cl.frame.serverframe;
	ent->pvClientData->current = *state;

	// update prvm fields
	CL_UpdateEntityFields( ent );
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

		if( msg->error )
			Host_Error("CL_ParsePacketEntities: end of message[%d > %d]\n", msg->readcount, msg->cursize );

		while( newnum >= clgame.numEntities ) CL_AllocEdict();

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
			// delta from baseline ?
			edict_t *ent = EDICT_NUM( newnum );
			if( ent->free ) CL_InitEdict( ent ); // FIXME: get rid of this
			CL_DeltaEntity( msg, newframe, newnum, &ent->pvClientData->baseline, false );
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

	for( ; EDICT_NUM( clgame.numEntities - 1 )->free; clgame.numEntities-- );
}

/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
static float CL_LerpPoint( void )
{
	float	f, frac;

	f = cl.mtime[0] - cl.mtime[1];

	if( !f )
	{
		cl.time = cl.mtime[0];		
		return 1.0f;
	}

	if( f > 0.1 )
	{	
		// dropped packet, or start of demo
		cl.mtime[1] = cl.mtime[0] - 0.1f;
		f = 0.1f;
	}

	frac = (cl.time - cl.mtime[1]) / f;
	if( frac < 0 )
	{
		if( frac < -0.01f )
		{
			cl.time = cl.mtime[1];
		}
		frac = 0;
	}
	else if( frac > 1.0f )
	{
		if( frac > 1.01f )
		{
			cl.time = cl.mtime[0];
		}
		frac = 1.0f;
	}
	return frac;
}

/*
================
CL_ParseFrame
================
*/
void CL_ParseFrame( sizebuf_t *msg )
{
	int     		cmd, len, idx;
	edict_t		*clent;
          
	Mem_Set( &cl.frame, 0, sizeof( cl.frame ));

	cl.frame.serverframe = MSG_ReadLong( msg );
	cl.serverframetime = MSG_ReadFloat( msg );
	cl.frame.deltaframe = MSG_ReadLong( msg );
	cl.surpressCount = MSG_ReadByte( msg );
	cl.frame.servertime = cl.mtime[0];	// same as servertime

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message 
	if( cl.frame.deltaframe <= 0 )
	{
		cl.frame.valid = true;	// uncompressed frame
		cls.demowaiting = false;	// we can start recording now
		cl.oldframe = NULL;
	}
	else
	{
		cl.oldframe = &cl.frames[cl.frame.deltaframe & UPDATE_MASK];
		if( !cl.oldframe->valid )
		{	
			// should never happen
			MsgDev( D_INFO, "delta from invalid frame (not supposed to happen!).\n" );
		}
		if( cl.oldframe->serverframe != cl.frame.deltaframe )
		{	
			// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			MsgDev( D_INFO, "Delta frame too old.\n" );
		}
		else if( cl.parse_entities - cl.oldframe->parse_entities > MAX_PARSE_ENTITIES - 128 )
		{
			MsgDev( D_INFO, "delta parse_entities too old.\n" );
		}
		else cl.frame.valid = true;	// valid delta parse
	}

	cl.time = bound( cl.frame.servertime - cl.serverframetime, cl.time, cl.frame.servertime );

	// read areabits
	len = MSG_ReadByte( msg );
	MSG_ReadData( msg, &cl.frame.areabits, len );

	// read clientindex
	cmd = MSG_ReadByte( msg );
	if( cmd != svc_playerinfo ) Host_Error( "CL_ParseFrame: not clientindex\n" );
	idx = MSG_ReadByte( msg );
	clent = EDICT_NUM( idx ); // get client
	if(( idx - 1 ) != cl.playernum )
		Host_Error("CL_ParseFrame: invalid playernum (%d should be %d)\n", idx-1, cl.playernum );

	// read packet entities
	cmd = MSG_ReadByte( msg );
	if( cmd != svc_packetentities ) Host_Error("CL_ParseFrame: not packetentities[%d]\n", cmd );
	CL_ParsePacketEntities( msg, cl.oldframe, &cl.frame );

	// now we can reading delta player state
	if( cl.oldframe ) cl.frame.ps = MSG_ParseDeltaPlayer( &cl.oldframe->ps, &clent->pvClientData->current );
	else cl.frame.ps = MSG_ParseDeltaPlayer( NULL, &clent->pvClientData->current );

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.serverframe & UPDATE_MASK] = cl.frame;

	if( cl.frame.valid )
	{
		if( cls.state != ca_active )
		{
			cls.state = ca_active;
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

	if( cl_paused->integer )
		cl.refdef.lerpfrac = 1.0f;
	else cl.refdef.lerpfrac = 1.0 - (cl.frame.servertime - cl.time) / (float)cl.serverframetime;
//	Msg( "cl.refdef.lerpfrac %g\n", cl.refdef.lerpfrac );

	for( pnum = 0; pnum < frame->num_entities; pnum++ )
	{
		s1 = &cl_parse_entities[(frame->parse_entities + pnum)&(MAX_PARSE_ENTITIES-1)];
		ent = EDICT_NUM( s1->number );

		if( ent->free ) continue;
		if( re->AddRefEntity( ent, s1->ed_type, cl.refdef.lerpfrac ))
		{
			if( s1->ed_type == ED_PORTAL && !VectorCompare( ent->v.origin, ent->v.oldorigin ))
				cl.render_flags |= RDF_PORTALINVIEW;
		}
		// NOTE: skyportal entity never added to rendering
		if( s1->ed_type == ED_SKYPORTAL ) cl.render_flags |= RDF_SKYPORTALINVIEW;
	}

	if( cl.oldframe && !memcmp( cl.oldframe->areabits, cl.frame.areabits, sizeof( cl.frame.areabits )))
		cl.render_flags |= RDF_OLDAREABITS;
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

	cl.render_flags = 0;

	CL_AddPacketEntities( &cl.frame );
	clgame.dllFuncs.pfnCreateEntities();

	CL_AddParticles();
	CL_AddDLights();
	CL_AddLightStyles();
	CL_AddDecals();

	// perfomance test
	CL_TestEntities();
	CL_TestLights();
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

	ent = EDICT_NUM( entnum );

	// calculate origin
	origin[0] = ent->pvClientData->prev.origin[0] + (ent->pvClientData->current.origin[0] - ent->pvClientData->prev.origin[0]) * cl.refdef.lerpfrac;
	origin[1] = ent->pvClientData->prev.origin[1] + (ent->pvClientData->current.origin[1] - ent->pvClientData->prev.origin[1]) * cl.refdef.lerpfrac;
	origin[2] = ent->pvClientData->prev.origin[2] + (ent->pvClientData->current.origin[2] - ent->pvClientData->prev.origin[2]) * cl.refdef.lerpfrac;

	// calculate velocity
	VectorSubtract( ent->pvClientData->current.origin, ent->pvClientData->prev.origin, velocity);
	VectorScale(velocity, 10, velocity);

	// if a brush model, offset the origin
	if( VectorIsNull( origin ))
	{
		cmodel = cl.models[ent->pvClientData->current.modelindex];
		if( !cmodel ) return;
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

		switch( ent->ed_type )
		{
		case ED_MOVER:
		case ED_AMBIENT:
		case ED_NORMAL: break;
		default: continue;
		}

		if( !ent->soundindex ) continue;
		S_AddLoopingSound( ent->number, cl.sound_precache[ent->soundindex], 1.0f, ATTN_IDLE );
	}
}