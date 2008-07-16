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
// cl_ents.c -- entity parsing and management

#include "common.h"
#include "client.h"

/*
=========================================================================

FRAME PARSING

=========================================================================
*/

float LerpAngle (float a2, float a1, float frac)
{
	if (a1 - a2 > 180) a1 -= 360;
	if (a1 - a2 < -180) a1 += 360;
	return a2 + frac * (a1 - a2);
}

float LerpView(float org1, float org2, float ofs1, float ofs2, float frac)
{
	return org1 + ofs1 + frac * (org2 + ofs2 - (org1 + ofs1));
}

float LerpPoint( float oldpoint, float curpoint, float frac )
{
	return oldpoint + frac * (curpoint - oldpoint);
}

/*
=================
CL_ParseEntityBits

Returns the entity number and the header bits
=================
*/
int CL_ParseEntityBits( sizebuf_t *msg, uint *bits )
{
	uint	b, total;
	int	number;

	total = MSG_ReadByte (msg);
	if (total & U_MOREBITS1)
	{
		b = MSG_ReadByte (msg);
		total |= b<<8;
	}
	if (total & U_MOREBITS2)
	{
		b = MSG_ReadByte (msg);
		total |= b<<16;
	}
	if (total & U_MOREBITS3)
	{
		b = MSG_ReadByte (msg);
		total |= b<<24;
	}

	if( total & U_NUMBER16 ) number = MSG_ReadShort( msg );
	else number = MSG_ReadByte( msg );
	*bits = total;

	return number;
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
	if( state->modelindex != ent->priv.cl->current.modelindex || state->weaponmodel != ent->priv.cl->current.weaponmodel || state->body != ent->priv.cl->current.body
		|| state->sequence != ent->priv.cl->current.sequence || abs(state->origin[0] - ent->priv.cl->current.origin[0]) > 512
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
		if(!newnum) break; // end of packet entities

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
===================
CL_ParsePlayerstate
===================
*/
void CL_ParsePlayerstate( sizebuf_t *msg, frame_t *oldframe, frame_t *newframe )
{
	int		flags;
	player_state_t	*state;
	int		i;
	int		statbits;

	state = &newframe->ps;

	// clear to old value before delta parsing
	if (oldframe) *state = oldframe->ps;
	else memset (state, 0, sizeof(*state));

	flags = MSG_ReadLong(msg);//four bytes

	// parse the pmove_state_t
	if (flags & PS_M_ORIGIN) MSG_ReadPos16(msg, state->origin ); 
	if (flags & PS_M_VELOCITY) MSG_ReadPos16(msg, state->velocity ); 
	if (flags & PS_M_TIME) state->pm_time = MSG_ReadByte (msg);
	if (flags & PS_M_FLAGS) state->pm_flags = MSG_ReadByte (msg);
	if (flags & PS_M_GRAVITY) state->gravity = MSG_ReadShort (msg);
	if (flags & PS_M_DELTA_ANGLES) MSG_ReadPos32(msg, state->delta_angles ); 

	if( cls.state == ca_cinematic || cls.demoplayback )
		state->pm_type = PM_FREEZE; // demo or movie playback

	// parse the rest of the player_state_t
	if (flags & PS_VIEWOFFSET)
	{
		state->viewoffset[0] = MSG_ReadChar (msg) * 0.25;
		state->viewoffset[1] = MSG_ReadChar (msg) * 0.25;
		state->viewoffset[2] = MSG_ReadChar (msg) * 0.25;
	}

	if (flags & PS_VIEWANGLES)
	{
		state->viewangles[0] = MSG_ReadBits( msg, NET_FLOAT );
		state->viewangles[1] = MSG_ReadBits( msg, NET_FLOAT );
		state->viewangles[2] = MSG_ReadBits( msg, NET_FLOAT );
	}

	if (flags & PS_KICKANGLES)
	{
		state->kick_angles[0] = MSG_ReadChar (msg) * 0.25;
		state->kick_angles[1] = MSG_ReadChar (msg) * 0.25;
		state->kick_angles[2] = MSG_ReadChar (msg) * 0.25;
	}

	if (flags & PS_WEAPONINDEX)
	{
		state->vmodel.index = MSG_ReadByte (msg);
	}

	if (flags & PS_WEAPONFRAME)
	{
		state->vmodel.frame = MSG_ReadFloat (msg);
	}


	if (flags & PS_WEAPONOFFSET)
	{
		state->vmodel.offset[0] = MSG_ReadChar (msg)*0.25;
		state->vmodel.offset[1] = MSG_ReadChar (msg)*0.25;
		state->vmodel.offset[2] = MSG_ReadChar (msg)*0.25;
	}

	if (flags & PS_WEAPONANGLES)
	{
		state->vmodel.angles[0] = MSG_ReadChar (msg)*0.25;
		state->vmodel.angles[1] = MSG_ReadChar (msg)*0.25;
		state->vmodel.angles[2] = MSG_ReadChar (msg)*0.25;
	}

	if (flags & PS_WEAPONSEQUENCE)
	{
		state->vmodel.sequence = MSG_ReadByte (msg);
	}

	if (flags & PS_WEAPONBODY)
	{
		state->vmodel.body = MSG_ReadByte (msg);
	}
	
	if (flags & PS_WEAPONSKIN)
	{
		state->vmodel.skin = MSG_ReadByte (msg);
	}

	if (flags & PS_BLEND)
	{
		state->blend[0] = MSG_ReadByte (msg) / 255.0;
		state->blend[1] = MSG_ReadByte (msg) / 255.0;
		state->blend[2] = MSG_ReadByte (msg) / 255.0;
		state->blend[3] = MSG_ReadByte (msg) / 255.0;
	}

	if (flags & PS_FOV)
		state->fov = MSG_ReadByte (msg);

	if (flags & PS_RDFLAGS)
		state->effects = MSG_ReadByte (msg);

	// parse stats
	statbits = MSG_ReadLong (msg);
	for( i = 0; i < MAX_STATS; i++ )
		if(statbits & (1<<i)) state->stats[i] = MSG_ReadShort( msg );
}

/*
================
CL_ParseFrame
================
*/
void CL_ParseFrame( sizebuf_t *msg )
{
	int     		cmd;
	int     		len;
	frame_t 		*old;
          
	memset( &cl.frame, 0, sizeof(cl.frame));
	cl.frame.serverframe = MSG_ReadLong (msg);
	cl.frame.deltaframe = MSG_ReadLong (msg);
	cl.frame.servertime = cl.frame.serverframe * HOST_FRAMETIME;
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
	else if( cl.time < cl.frame.servertime - HOST_FRAMETIME )
		cl.time = cl.frame.servertime - HOST_FRAMETIME;

	// read areabits
	len = MSG_ReadByte( msg );
	MSG_ReadData( msg, &cl.frame.areabits, len );

	// read playerinfo
	cmd = MSG_ReadByte( msg );
	if( cmd != svc_playerinfo ) Host_Error( "CL_ParseFrame: not playerinfo\n" );
	CL_ParsePlayerstate( msg, old, &cl.frame );

	// read packet entities
	cmd = MSG_ReadByte (msg);
	if( cmd != svc_packetentities ) Host_Error("CL_ParseFrame: not packetentities\n" );
	CL_ParsePacketEntities( msg, old, &cl.frame );

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.serverframe & UPDATE_MASK] = cl.frame;

	if( cl.frame.valid )
	{
		if( cls.state != ca_active )
		{
			cls.state = ca_active;
			cl.force_refdef = true;
			// getting a valid frame message ends the connection process
			VectorScale( cl.frame.ps.origin, CL_COORD_FRAC, cl.predicted_origin );
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
	entity_t		refent;
	entity_state_t	*s1;
	float		autorotate;
	int		i, pnum;
	edict_t		*ent;
	int		autoanim;
	uint		effects, renderfx;

	// bonus items rotate at a fixed rate
	autorotate = anglemod(cl.time/10);

	// brush models can auto animate their frames
	autoanim = 2 * cl.time/1000;

	memset( &refent, 0, sizeof(refent));

	for( pnum = 0; pnum < frame->num_entities; pnum++ )
	{
		s1 = &cl_parse_entities[(frame->parse_entities+pnum)&(MAX_PARSE_ENTITIES-1)];

		ent = PRVM_EDICT_NUM( s1->number );

		effects = s1->effects;
		renderfx = s1->renderfx;
		refent.frame = s1->frame;

		// copy state to progs
		ent->progs.cl->modelindex = ent->priv.cl->current.modelindex;
		ent->progs.cl->soundindex = ent->priv.cl->current.soundindex;
//ent->progs.cl->model = PRVM_SetEngineString( cl.configstrings[CS_MODELS+ent->priv.cl->current.modelindex] ); 

		// copy state to render
		refent.prev.frame = ent->priv.cl->prev.frame;
		refent.backlerp = 1.0 - cl.lerpfrac;
		refent.alpha = s1->alpha;
		refent.body = s1->body;
		refent.sequence = s1->sequence;		
		refent.animtime = s1->animtime;

		// setup latchedvars
		refent.prev.animtime = ent->priv.cl->prev.animtime;
		VectorCopy( ent->priv.cl->prev.origin, refent.prev.origin );
		VectorCopy( ent->priv.cl->prev.angles, refent.prev.angles );
		refent.prev.sequence = ent->priv.cl->prev.sequence;
		refent.prev.frame = ent->priv.cl->prev.frame;
		//refent.prev.sequencetime;
		
		// interpolate origin
		for( i = 0; i < 3; i++ )
		{
			refent.origin[i] = LerpPoint( ent->priv.cl->prev.origin[i], ent->priv.cl->current.origin[i], cl.lerpfrac );
			refent.oldorigin[i] = refent.origin[i];
		}

		// set skin
		refent.skin = s1->skin;
		refent.model = cl.model_draw[s1->modelindex];
		refent.weaponmodel = cl.model_draw[s1->weaponmodel];
		refent.flags = renderfx;

		// calculate angles
		if( effects & EF_ROTATE )
		{	
			// some bonus items auto-rotate
			refent.angles[0] = 0;
			refent.angles[1] = autorotate;
			refent.angles[2] = 0;
		}
		else
		{	
			// interpolate angles
			for( i = 0; i < 3; i++ )
			{
				refent.angles[i] = LerpAngle( ent->priv.cl->prev.angles[i], ent->priv.cl->current.angles[i], cl.lerpfrac );
			}
		}

		if( s1->number == cl.playernum + 1 )
		{
			refent.flags |= RF_PLAYERMODEL;	// only draw from mirrors

			// just only for test
			refent.controller[0] = 90.0;
			refent.controller[1] = 90.0;
			refent.controller[2] = 180.0;
			refent.controller[3] = 180.0;
			refent.sequence = 0;
			refent.frame = 0;
		}

		// if set to invisible, skip
		if( !s1->modelindex ) continue;

		// add to refresh list
		V_AddEntity( &refent );
	}
}



/*
==============
CL_AddViewWeapon
==============
*/
void CL_AddViewWeapon( player_state_t *ps, player_state_t *ops )
{
	entity_t	gun;	// view model
	int	i;

	// allow the gun to be completely removed
	if( !cl_gun->value ) return;

	// don't draw gun if in wide angle view
	if (ps->fov > 135) return;

	memset (&gun, 0, sizeof(gun));

	if (gun_model) gun.model = gun_model; // development tool
	else gun.model = cl.model_draw[ps->vmodel.index];
	if (!gun.model) return;

	// set up gun position
	for( i = 0; i < 3; i++ )
	{
		gun.origin[i] = gun.oldorigin[i] = cl.refdef.vieworg[i] + ops->vmodel.offset[i] + (ps->vmodel.offset[i] - ops->vmodel.offset[i]) * cl.lerpfrac;
		gun.angles[i] = cl.refdef.viewangles[i] + LerpAngle (ops->vmodel.angles[i], ps->vmodel.angles[i], cl.lerpfrac);
	}

	gun.frame = ps->vmodel.frame;
	if( gun.frame == 0 ) gun.prev.frame = 0; // just changed weapons, don't lerp from old
	else gun.prev.frame = ops->vmodel.frame;

	gun.body = ps->vmodel.body;
	gun.skin = ps->vmodel.skin;
	gun.sequence = ps->vmodel.sequence;
          
	gun.flags = RF_MINLIGHT | RF_DEPTHHACK | RF_VIEWMODEL;
	gun.backlerp = 1.0 - cl.lerpfrac;
	VectorCopy ( gun.origin, gun.oldorigin ); // don't lerp at all
	V_AddEntity( &gun );
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
	player_state_t	*ps, *ops;

	// clamp time
	if( cl.time > cl.frame.servertime )
	{
		if( cl_showclamp->value )
			Msg ("high clamp %i\n", cl.time - cl.frame.servertime);
		cl.time = cl.frame.servertime;
		cl.lerpfrac = 1.0f;
	}
	else if (cl.time < cl.frame.servertime - HOST_FRAMETIME )
	{
		if (cl_showclamp->value)
			Msg ("low clamp %i\n", cl.frame.servertime - host_frametime->value - cl.time);
		cl.time = cl.frame.servertime - HOST_FRAMETIME;
		cl.lerpfrac = 0.0f;
	}
	else
	{
		cl.lerpfrac = 1.0 - (cl.frame.servertime - cl.time) * 0.01f;
		//Msg("cl.lerpfrac %g\n", cl.lerpfrac );
	}

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
		if( delta < HOST_FRAMETIME ) cl.refdef.vieworg[2] -= cl.predicted_step * (HOST_FRAMETIME - delta) * 0.01f;
	}
	else
	{
		// just use interpolated values
		for( i = 0; i < 3; i++ )
			cl.refdef.vieworg[i] = LerpView( ops->origin[i] * CL_COORD_FRAC, ps->origin[i] * CL_COORD_FRAC, ops->viewoffset[i], ps->viewoffset[i], lerp );
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
		cl.refdef.viewangles[i] += LerpAngle( ops->kick_angles[i], ps->kick_angles[i], lerp );

	AngleVectors( cl.refdef.viewangles, cl.v_forward, cl.v_right, cl.v_up );

	// interpolate field of view
	cl.refdef.fov_x = ops->fov + lerp * ( ps->fov - ops->fov );

	// don't interpolate blend color
	for( i = 0; i < 4; i++ )
		cl.refdef.blend[i] = ps->blend[i];

	// add the weapon
	CL_AddViewWeapon( ps, ops );
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

	if( entnum < 0 || entnum >= MAX_EDICTS )
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
		cmodel = cl.model_clip[ent->priv.cl->current.modelindex];
		if(!cmodel) return;
		VectorAverage(cmodel->mins, cmodel->maxs, midPoint);
		VectorAdd(origin, midPoint, origin);
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
	//if( !cl.sound_prepped ) return; //FIXME: enable

	for( i = 0; i < cl.frame.num_entities; i++ )
	{
		num = (cl.frame.parse_entities + i)&(MAX_PARSE_ENTITIES-1);
		ent = &cl_parse_entities[num];
		if(!ent->soundindex) continue;
		se->AddLoopingSound( ent->number, cl.sound_precache[ent->soundindex], 1.0f, ATTN_IDLE );
	}
}