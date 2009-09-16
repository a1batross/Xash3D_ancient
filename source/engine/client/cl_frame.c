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
		ent->pvClientData->msgnum = -1;
	}

	if( ent->pvClientData->msgnum != cl.frame.msgnum - 1 )
	{	
		// duplicate the current state so lerping doesn't hurt anything
		ent->pvClientData->prev = *state;
	}
	else
	{	// shuffle the last state to previous
		ent->pvClientData->prev = ent->pvClientData->current;
	}

	ent->pvClientData->msgnum = cl.frame.msgnum;
	ent->pvClientData->current = *state;

	// update edict fields
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
static float CL_LerpPoint1( void )
{
	float	f, frac;

	f = cl.time - cl.oldtime;

	if( !f || SV_Active())
	{
		return 1.0f;
	}

	if( f > 0.1 )
	{	
		// dropped packet, or start of demo
//		cl.mtime[1] = cl.mtime[0] - 0.1f;
		f = 0.1f;
	}

	frac = (cl.time - cl.oldtime) / f;
	if( frac < 0 )
	{
		if( frac < -0.01f )
		{
//			cl.time = cl.mtime[1];
		}
		frac = 0.0f;
	}
	else if( frac > 1.0f )
	{
		if( frac > 1.01f )
		{
//			cl.time = cl.mtime[0];
		}
		frac = 1.0f;
	}
	return frac;
}

static float CL_LerpPoint2( void )
{
	float	lerpfrac;

	// clamp time
	if( cl.time > cl.frame.servertime )
	{
		if( cl_showclamp->integer ) Msg( "cl highclamp\n" );
//		cl.time = cl.frame.servertime;
		lerpfrac = 1.0f;
	}
	else if( cl.time < cl.frame.servertime - cl.frametime )
	{
		if( cl_showclamp->integer ) Msg( "cl lowclamp\n" );
//		cl.time = cl.frame.servertime - cl.frametime;
		lerpfrac = 0.0f;
	}
	else lerpfrac = 1.0f - (cl.frame.servertime - cl.time) / cl.frametime;

	if( cl_paused->integer )
		lerpfrac = 1.0f;

	return lerpfrac;
}

/*
==================
CL_FirstSnapshot
==================
*/
void CL_FirstSnapshot( void )
{
	cls.state = ca_active;

	// set the timedelta so we are exactly on this first frame
	cl.time_delta = cl.frame.servertime - cls.realtime;
	cl.oldtime = cl.frame.servertime;

	// getting a valid frame message ends the connection process
	VectorCopy( cl.frame.ps.origin, cl.predicted_origin );
	VectorCopy( cl.frame.ps.viewangles, cl.predicted_angles );
}

void CL_AdjustTimeDelta( void )
{
	float	newDelta;
	float	deltaDelta;

	cl.has_newframe = false;

	// the delta never drifts when replaying a demo
	if( cls.demoplayback ) return;

	newDelta = cl.frame.servertime - cls.realtime;
	deltaDelta = fabs( newDelta - cl.time_delta );

	if( deltaDelta > 0.5f )
	{
		cl.time_delta = newDelta;
		cl.oldtime = cl.frame.servertime;	// FIXME: is this a problem for cgame?
		cl.time = cl.frame.servertime;
		Msg( "<RESET> " );
	}
	else if( deltaDelta > 0.1f )
	{
		// fast adjust, cut the difference in half
		cl.time_delta = ( cl.time_delta + newDelta ) / 2;
	}
	else
	{
		// slow drift adjust, only move 1 or 2 msec

		// if any of the frames between this and the previous snapshot
		// had to be extrapolated, nudge our sense of time back a little
		// the granularity of +1 / -2 is too high for timescale modified frametimes
		if( cl.frame_extrapolate )
		{
			cl.frame_extrapolate = false;
			cl.time_delta -= 0.002f;
		}
		else
		{
			// otherwise, move our sense of time forward to minimize total latency
			cl.time_delta += 0.001f;
		}
	}
}

/*
================
CL_SetClientTime

set right client time and calc lerp value
================
*/
void CL_SetClientTime( void )
{
	float	tn;

	// getting a valid frame message ends the connection process
	if( cls.state != ca_active )
	{
		if( cls.state != ca_connected ) return;

		if( cls.demoplayback )
		{
			CL_ReadDemoMessage();
		}
		if( cl.has_newframe )
		{
			cl.has_newframe = false;
			CL_FirstSnapshot();
		}
		if( cls.state != ca_active ) return;
	}

	// if we have gotten to this point, cl.snap is guaranteed to be valid
	Com_Assert( !cl.frame.valid );

	// allow pause in single player
	if( SV_Active() && cl_paused->integer ) return;

	// cl_timeNudge is a user adjustable cvar that allows more
	// or less latency to be added in the interest of better 
	// smoothness or better responsiveness.
	tn = cl_timenudge->integer * 0.001f;
	if( tn < -0.03f ) tn = -0.03f;
	else if( tn > 0.03f ) tn = 0.03f;

	cl.time = cls.realtime + cl.time_delta - tn;

	cl.frametime = cl.time - cl.oldtime;
	if( cl.frametime < 0 ) cl.frametime = 0;

	// guarantee that time will never flow backwards, even if
	// serverTimeDelta made an adjustment or cl_timeNudge was changed
	if( cl.time < cl.oldtime ) cl.time = cl.oldtime;
	cl.oldtime = cl.time;

	// note if we are almost past the latest frame (without timeNudge),
	// so we will try and adjust back a bit when the next snapshot arrives
	if( cls.realtime + cl.time_delta >= cl.frame.servertime - 0.005f )
		cl.frame_extrapolate = true;

	// if we have gotten new snapshots, drift serverTimeDelta
	// don't do this every frame, or a period of packet loss would
	// make a huge adjustment
	if( cl.has_newframe ) CL_AdjustTimeDelta();

#if 1
	cl.refdef.lerpfrac = CL_LerpPoint1();
#else
	// set cl.refdef.lerpfrac
	if( cl.oldframe )
	{
		float	delta;

		delta = (cl.frame.servertime - cl.oldframe->servertime);
		if( delta == 0.0f ) cl.refdef.lerpfrac = 0.0f;
		else cl.refdef.lerpfrac = (cl.time - cl.oldframe->servertime) / delta;
	}
	else cl.refdef.lerpfrac = 0.0f;
#endif
}

/*
================
CL_ParseFrame
================
*/
void CL_ParseFrame( sizebuf_t *msg )
{
	int     		cmd, len, idx;
	int		delta_num;
	int		old_msgnum;
	int		packet_num;
	edict_t		*clent;
          
	Mem_Set( &cl.frame, 0, sizeof( cl.frame ));

	cl.frame.msgnum = cls.netchan.incoming_sequence;
	cl.frame.servertime = MSG_ReadFloat( msg );
	delta_num = MSG_ReadByte( msg );

	if( !delta_num ) cl.frame.deltaframe = -1;
	else cl.frame.deltaframe = cl.frame.msgnum - delta_num;

	// If the frame is delta compressed from data that we no longer
	// have available, we must suck up the rest of the frame,
	// but not use it, then ask for a non-compressed message 
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
		if( cl.oldframe->msgnum != cl.frame.deltaframe )
		{	
			// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			MsgDev( D_INFO, "delta frame too old.\n" );
		}
		else if( cl.parse_entities - cl.oldframe->parse_entities > MAX_PARSE_ENTITIES - 128 )
		{
			MsgDev( D_INFO, "delta parse_entities too old.\n" );
		}
		else cl.frame.valid = true;	// valid delta parse
	}

	// read areabits
	len = MSG_ReadByte( msg );
	MSG_ReadData( msg, &cl.frame.areabits, len );

	if( cl.oldframe && !memcmp( cl.oldframe->areabits, cl.frame.areabits, sizeof( cl.frame.areabits )))
		cl.render_flags |= RDF_OLDAREABITS;
	else cl.render_flags &= ~RDF_OLDAREABITS;

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

	// if not valid, dump the entire thing now that it has been properly read
	if( !cl.frame.valid ) return;

	// clear the valid flags of any snapshots between the last
	// received and this one, so if there was a dropped packet
	// it won't look like something valid to delta from next
	// time we wrap around in the buffer
	old_msgnum = cl.frame.msgnum + 1;
	cl.frame.ping = 999;

	if( cl.frame.msgnum - old_msgnum >= UPDATE_BACKUP )
		old_msgnum = cl.frame.msgnum - UPDATE_MASK;
	for( ; old_msgnum < cl.frame.msgnum; old_msgnum++ )
		cl.frames[old_msgnum & UPDATE_MASK].valid = false;

	// calculate ping time
	for ( idx = 0; idx < UPDATE_BACKUP; idx++ )
	{
		packet_num = ( cls.netchan.outgoing_sequence - 1 - idx ) & UPDATE_MASK;
		if( cl.frame.ps.cmdtime >= cl.outframes[packet_num].servertime )
		{
			cl.frame.ping = cls.realtime - cl.outframes[packet_num].realtime;
			break;
		}
	}

	if( cl_shownet->integer == 3 )
		Msg( "   snapshot:%i  delta:%i  ping:%i\n", cl.frame.msgnum, cl.frame.deltaframe, cl.frame.ping );

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.msgnum & UPDATE_MASK] = cl.frame;
	cl.has_newframe = true;

	CL_CheckPredictionError();
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