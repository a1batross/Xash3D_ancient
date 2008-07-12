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
#include "server.h"

/*
=============================================================================

Copy PRVM values into entity state

=============================================================================
*/
void SV_UpdateEntityState( edict_t *ent)
{
	// copy progs values to state
	ent->priv.sv->s.number = ent->priv.sv->serialnumber;
	ent->priv.sv->s.solid = ent->priv.sv->solid;

	VectorCopy (ent->progs.sv->origin, ent->priv.sv->s.origin);
	VectorCopy (ent->progs.sv->angles, ent->priv.sv->s.angles);
	VectorCopy (ent->progs.sv->old_origin, ent->priv.sv->s.old_origin);
	
	ent->priv.sv->s.modelindex = (int)ent->progs.sv->modelindex;

	if( ent->priv.sv->client )
	{
		// attached weaponmodel
		// FIXME: let any entity send weaponmodel
		ent->priv.sv->s.weaponmodel = ent->priv.sv->client->ps.pmodel.index;
	}

	ent->priv.sv->s.skin = (short)ent->progs.sv->skin;	// studio model skin
	ent->priv.sv->s.body = (byte)ent->progs.sv->body;		// studio model submodel 
	ent->priv.sv->s.frame = ent->progs.sv->frame;		// any model current frame
	ent->priv.sv->s.sequence = (byte)ent->progs.sv->sequence;	// studio model sequence
	ent->priv.sv->s.effects = (uint)ent->progs.sv->effects;	// shared client and render flags
	ent->priv.sv->s.renderfx = (int)ent->progs.sv->renderfx;	// renderer flags
	ent->priv.sv->s.alpha = ent->progs.sv->alpha;		// alpha value
	ent->priv.sv->s.animtime = ent->progs.sv->animtime;	// auto-animating time
}

/*
=============================================================================

Encode a client frame onto the network channel

=============================================================================
*/
/*
=============
SV_EmitPacketEntities

Writes a delta update of an entity_state_t list to the message->
=============
*/
void SV_EmitPacketEntities (client_frame_t *from, client_frame_t *to, sizebuf_t *msg)
{
	entity_state_t	*oldent, *newent;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		from_num_entities;
	int		bits;

	MSG_WriteByte (msg, svc_packetentities);

	if (!from) from_num_entities = 0;
	else from_num_entities = from->num_entities;

	newindex = 0;
	oldindex = 0;
	while( newindex < to->num_entities || oldindex < from_num_entities )
	{
		if( newindex >= to->num_entities )
		{
			newnum = MAX_ENTNUMBER;
		}
		else
		{
			newent = &svs.client_entities[(to->first_entity+newindex)%svs.num_client_entities];
			newnum = newent->number;
		}

		if( oldindex >= from_num_entities )
		{
			oldnum = MAX_ENTNUMBER;
		}
		else
		{
			oldent = &svs.client_entities[(from->first_entity+oldindex)%svs.num_client_entities];
			oldnum = oldent->number;
		}

		if (newnum == oldnum)
		{	
			// delta update from old position
			// because the force parm is false, this will not result
			// in any bytes being emited if the entity has not changed at all
			// note that players are always 'newentities', this updates their oldorigin always
			// and prevents warping
			MSG_WriteDeltaEntity( oldent, newent, msg, false, newent->number <= maxclients->value );
			oldindex++;
			newindex++;
			continue;
		}

		if( newnum < oldnum )
		{	// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity (&sv.baselines[newnum], newent, msg, true, true);
			newindex++;
			continue;
		}

		if( newnum > oldnum )
		{	
			// the old entity isn't present in the new message
			bits = U_REMOVE;

			MSG_WriteByte( msg,	bits & 255 );
			if( bits & 0x0000ff00 ) MSG_WriteByte( msg, (bits>>8) & 255 );

			if (bits & U_NUMBER16)
			{
				MSG_WriteShort (msg, oldnum);
			}
			else
			{
				MSG_WriteByte (msg, oldnum);
			}
			oldindex++;
			continue;
		}
	}
	MSG_WriteShort( msg, 0 ); // end of packetentities
}



/*
=============
SV_WritePlayerstateToClient

=============
*/
void SV_WritePlayerstateToClient (client_frame_t *from, client_frame_t *to, sizebuf_t *msg)
{
	int				i;
	int				pflags = 0;
	player_state_t			*ps, *ops;
	player_state_t			dummy;
	int				statbits;

	ps = &to->ps;
	if (!from)
	{
		memset (&dummy, 0, sizeof(dummy));
		ops = &dummy;
	}
	else ops = &from->ps;

	// determine what needs to be sent
	if (ps->pm_type != ops->pm_type) pflags |= PS_M_TYPE;
	if(!VectorICompare(ps->origin, ops->origin)) pflags |= PS_M_ORIGIN;
	if(!VectorICompare(ps->velocity, ops->velocity)) pflags |= PS_M_VELOCITY;
	if (ps->pm_time != ops->pm_time) pflags |= PS_M_TIME;
	if (ps->pm_flags != ops->pm_flags) pflags |= PS_M_FLAGS;
	if (ps->gravity != ops->gravity) pflags |= PS_M_GRAVITY;
	if(!VectorCompare(ps->delta_angles, ops->delta_angles)) pflags |= PS_M_DELTA_ANGLES;
	if(!VectorCompare(ps->viewoffset, ops->viewoffset)) pflags |= PS_VIEWOFFSET;
	if(!VectorCompare(ps->viewangles, ops->viewangles)) pflags |= PS_VIEWANGLES;
	if(!VectorCompare(ps->kick_angles, ops->kick_angles)) pflags |= PS_KICKANGLES;
		
	if (ps->blend[0] != ops->blend[0] || ps->blend[1] != ops->blend[1] || ps->blend[2] != ops->blend[2] || ps->blend[3] != ops->blend[3] )
		pflags |= PS_BLEND;

	if (ps->fov != ops->fov) pflags |= PS_FOV;
	if (ps->effects != ops->effects) pflags |= PS_RDFLAGS;
	if (ps->vmodel.frame != ops->vmodel.frame) pflags |= PS_WEAPONFRAME;
	if (ps->vmodel.sequence != ops->vmodel.sequence) pflags |= PS_WEAPONSEQUENCE;
	//if (!VectorCompare(ps->vmodel.offset, ops->vmodel.offset)) pflags |= PS_WEAPONOFFSET;
	//if (!VectorCompare(ps->vmodel.angles, ops->vmodel.angles)) pflags |= PS_WEAPONANGLES;
	if (ps->vmodel.body != ops->vmodel.body) pflags |= PS_WEAPONBODY;
	if (ps->vmodel.skin != ops->vmodel.skin) pflags |= PS_WEAPONSKIN;

	pflags |= PS_WEAPONINDEX;

	// write it
	MSG_WriteByte (msg, svc_playerinfo);
	MSG_WriteLong (msg, pflags);

	// write the pmove_state_t
	if (pflags & PS_M_TYPE) MSG_WriteByte (msg, ps->pm_type);
	if (pflags & PS_M_ORIGIN) MSG_WritePos16(msg, ps->origin);
	if (pflags & PS_M_VELOCITY) MSG_WritePos16(msg, ps->velocity);
	if (pflags & PS_M_TIME) MSG_WriteByte (msg, ps->pm_time);
	if (pflags & PS_M_FLAGS) MSG_WriteByte (msg, ps->pm_flags);
	if (pflags & PS_M_GRAVITY) MSG_WriteShort (msg, ps->gravity);
	if (pflags & PS_M_DELTA_ANGLES) MSG_WritePos32(msg, ps->delta_angles);

	//
	// write the rest of the player_state_t
	//
	if (pflags & PS_VIEWOFFSET)
	{
		MSG_WriteChar (msg, ps->viewoffset[0] * 4);
		MSG_WriteChar (msg, ps->viewoffset[1] * 4);
		MSG_WriteChar (msg, ps->viewoffset[2] * 4);
	}

	if (pflags & PS_VIEWANGLES)
	{
		MSG_WriteAngle32 (msg, ps->viewangles[0]);
		MSG_WriteAngle32 (msg, ps->viewangles[1]);
		MSG_WriteAngle32 (msg, ps->viewangles[2]);
	}

	if (pflags & PS_KICKANGLES)
	{
		MSG_WriteChar (msg, ps->kick_angles[0] * 4);
		MSG_WriteChar (msg, ps->kick_angles[1] * 4);
		MSG_WriteChar (msg, ps->kick_angles[2] * 4);
	}

	if (pflags & PS_WEAPONINDEX)
	{
		MSG_WriteByte (msg, ps->vmodel.index);
	}

	if (pflags & PS_WEAPONFRAME)
	{
		MSG_WriteFloat( msg, ps->vmodel.frame );
	}

	if (pflags & PS_WEAPONOFFSET)
	{
		MSG_WriteChar (msg, ps->vmodel.offset[0]*4);
		MSG_WriteChar (msg, ps->vmodel.offset[1]*4);
		MSG_WriteChar (msg, ps->vmodel.offset[2]*4);
	}

	if (pflags & PS_WEAPONANGLES)
	{
		MSG_WriteChar (msg, ps->vmodel.angles[0]*4);
		MSG_WriteChar (msg, ps->vmodel.angles[1]*4);
		MSG_WriteChar (msg, ps->vmodel.angles[2]*4);
	} 

	if (pflags & PS_WEAPONSEQUENCE)
	{
		MSG_WriteByte (msg, ps->vmodel.sequence);
	}

	if (pflags & PS_WEAPONBODY)
	{
		MSG_WriteByte (msg, ps->vmodel.body);
	}
	
	if (pflags & PS_WEAPONSKIN)
	{
		MSG_WriteByte (msg, ps->vmodel.skin);
	}

	if (pflags & PS_BLEND)
	{
		MSG_WriteByte (msg, ps->blend[0]*255);
		MSG_WriteByte (msg, ps->blend[1]*255);
		MSG_WriteByte (msg, ps->blend[2]*255);
		MSG_WriteByte (msg, ps->blend[3]*255);
	}
	if (pflags & PS_FOV)
		MSG_WriteByte (msg, ps->fov);
	if (pflags & PS_RDFLAGS)
		MSG_WriteByte (msg, ps->effects);

	// send stats
	statbits = 0;
	for( i = 0; i < MAX_STATS; i++ )
		if( ps->stats[i] != ops->stats[i] )
			statbits |= 1<<i;
	MSG_WriteLong( msg, statbits );
	for (i=0 ; i<MAX_STATS ; i++)
		if (statbits & (1<<i) )
			MSG_WriteShort (msg, ps->stats[i]);
}


/*
==================
SV_WriteFrameToClient
==================
*/
void SV_WriteFrameToClient (sv_client_t *client, sizebuf_t *msg)
{
	client_frame_t		*frame, *oldframe;
	int					lastframe;

	//Msg ("%i -> %i\n", client->lastframe, sv.framenum);
	// this is the frame we are creating
	frame = &client->frames[sv.framenum & UPDATE_MASK];

	if (client->lastframe <= 0)
	{	// client is asking for a retransmit
		oldframe = NULL;
		lastframe = -1;
	}
	else if (sv.framenum - client->lastframe >= (UPDATE_BACKUP - 3) )
	{	// client hasn't gotten a good message through in a long time
//		Msg ("%s: Delta request from out-of-date packet.\n", client->name);
		oldframe = NULL;
		lastframe = -1;
	}
	else
	{	// we have a valid message to delta from
		oldframe = &client->frames[client->lastframe & UPDATE_MASK];
		lastframe = client->lastframe;
	}

	MSG_WriteByte (msg, svc_frame);
	MSG_WriteLong (msg, sv.framenum);
	MSG_WriteLong (msg, lastframe);	// what we are delta'ing from
	MSG_WriteByte (msg, client->surpressCount);	// rate dropped packets
	client->surpressCount = 0;

	// send over the areabits
	MSG_WriteByte (msg, frame->areabytes);
	SZ_Write (msg, frame->areabits, frame->areabytes);

	// delta encode the playerstate
	SV_WritePlayerstateToClient (oldframe, frame, msg);

	// delta encode the entities
	SV_EmitPacketEntities (oldframe, frame, msg);
}


/*
=============================================================================

Build a client frame structure

=============================================================================
*/
/*
=============
SV_BuildClientFrame

Decides which entities are going to be visible to the client, and
copies off the playerstat and areabits.
=============
*/
void SV_BuildClientFrame( sv_client_t *client )
{
	int		e, i;
	vec3_t		org;
	edict_t		*ent;
	edict_t		*clent;
	client_frame_t	*frame;
	entity_state_t	*state;
	int		l;
	int		clientarea, clientcluster;
	int		leafnum;
	int		c_fullsend;
	byte		*clientpvs;
	byte		*clientphs;
	byte		*bitvector;

	clent = client->edict;
	if (!clent->priv.sv->client) return;// not in game yet

	// this is the frame we are creating
	frame = &client->frames[sv.framenum & UPDATE_MASK];

	frame->senttime = svs.realtime; // save it for ping calc later

	// find the client's PVS
	VectorScale( clent->priv.sv->client->ps.origin, CL_COORD_FRAC, org ); 
	VectorAdd( org, clent->priv.sv->client->ps.viewoffset, org );  

	leafnum = pe->PointLeafnum (org);
	clientarea = pe->LeafArea (leafnum);
	clientcluster = pe->LeafCluster (leafnum);

	// calculate the visible areas
	frame->areabytes = pe->WriteAreaBits (frame->areabits, clientarea);

	// grab the current player_state_t
	frame->ps = clent->priv.sv->client->ps;

	clientpvs = pe->ClusterPVS( clientcluster );
	clientphs = pe->ClusterPHS( clientcluster );

	// build up the list of visible entities
	frame->num_entities = 0;
	frame->first_entity = svs.next_client_entities;

	c_fullsend = 0;

	for (e = 1; e < prog->num_edicts; e++)
	{
		ent = PRVM_EDICT_NUM(e);

		// ignore ents without visible models unless they have an effect
		if( !ent->progs.sv->modelindex && !ent->progs.sv->effects && !ent->priv.sv->s.soundindex )
			continue;

		// ignore if not touching a PV leaf
		if( ent != clent )
		{
			// check area
			if (!pe->AreasConnected (clientarea, ent->priv.sv->areanum))
			{	
				// doors can legally straddle two areas, so
				// we may need to check another one
				if (!ent->priv.sv->areanum2 || !pe->AreasConnected (clientarea, ent->priv.sv->areanum2))
					continue;	// blocked by a door
			}

			// FIXME: if an ent has a model and a sound, but isn't
			// in the PVS, only the PHS, clear the model
			if (ent->priv.sv->s.soundindex ) bitvector = clientphs;
			else bitvector = clientpvs;

			// check individual leafs
			if( !ent->priv.sv->num_clusters )
			{
				continue;
			}
			for( i = 0, l = 0; i < ent->priv.sv->num_clusters; i++ )
			{
				l = ent->priv.sv->clusternums[i];
				if( bitvector[l>>3] & (1<<(l&7)))
					break;
			}
			// if we haven't found it to be visible,
			// check overflow clusters that coudln't be stored
			if( i == ent->priv.sv->num_clusters )
			{
				if( ent->priv.sv->lastcluster )
				{
					for( ; l <= ent->priv.sv->lastcluster; l++ )
					{
						if( bitvector[l>>3] & (1<<(l&7)))
							break;
					}
					if( l == ent->priv.sv->lastcluster )
						continue;	// not visible
				}
				else continue;
			}
			if( !ent->progs.sv->modelindex )
			{	
				// don't send sounds if they will be attenuated away
				vec3_t	delta, entorigin;
				float	len;

				if(VectorIsNull( ent->progs.sv->origin ))
				{
					VectorAdd( ent->progs.sv->mins, ent->progs.sv->maxs, entorigin );
					VectorScale( entorigin, 0.5, entorigin );
				}
				else
				{
					VectorCopy( ent->progs.sv->origin, entorigin );
				}

				VectorSubtract( org, entorigin, delta );	
				len = VectorLength( delta );
				if( len > 400 ) continue;
			}
		}

		// add it to the circular client_entities array
		state = &svs.client_entities[svs.next_client_entities % svs.num_client_entities];
		if (ent->priv.sv->serialnumber != e)
		{
			MsgDev( D_WARN, "SV_BuildClientFrame: invalid ent->priv.sv->serialnumber %d\n", ent->priv.sv->serialnumber );
			ent->priv.sv->serialnumber = e; // ptr to current entity such as entnumber
		}

                    SV_UpdateEntityState( ent );
		*state = ent->priv.sv->s;

		// don't mark players missiles as solid
		if (PRVM_PROG_TO_EDICT(ent->progs.sv->owner) == client->edict) state->solid = 0;

		svs.next_client_entities++;
		frame->num_entities++;
	}
}