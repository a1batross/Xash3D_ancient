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

#include "engine.h"
#include "server.h"
#include "collision.h"

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
	ent->priv.sv->s.event = ent->priv.sv->event;

	VectorCopy (ent->progs.sv->origin, ent->priv.sv->s.origin);
	VectorCopy (ent->progs.sv->angles, ent->priv.sv->s.angles);
	VectorCopy (ent->progs.sv->old_origin, ent->priv.sv->s.old_origin);
	
	ent->priv.sv->s.modelindex = (int)ent->progs.sv->modelindex;
	ent->priv.sv->s.weaponmodel = 0; // attached weaponmodel

	ent->priv.sv->s.skin = (short)ent->progs.sv->skin;	// studio model skin
	ent->priv.sv->s.body = (byte)ent->progs.sv->body;		// studio model submodel 
	ent->priv.sv->s.frame = (short)ent->progs.sv->frame;	// any model current frame
	ent->priv.sv->s.sequence = (byte)ent->progs.sv->sequence;	// studio model sequence
	ent->priv.sv->s.effects = (uint)ent->progs.sv->effects;	// shared client and render flags
	ent->priv.sv->s.renderfx = (int)ent->progs.sv->renderfx;	// renderer flags
	ent->priv.sv->s.alpha = ent->progs.sv->alpha;		// alpha value
	ent->priv.sv->s.soundindex = SV_SoundIndex(PRVM_GetString(ent->progs.sv->noise3));
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
	while (newindex < to->num_entities || oldindex < from_num_entities)
	{
		if (newindex >= to->num_entities)
			newnum = 9999;
		else
		{
			newent = &svs.client_entities[(to->first_entity+newindex)%svs.num_client_entities];
			newnum = newent->number;
		}

		if (oldindex >= from_num_entities)
			oldnum = 9999;
		else
		{
			oldent = &svs.client_entities[(from->first_entity+oldindex)%svs.num_client_entities];
			oldnum = oldent->number;
		}

		if (newnum == oldnum)
		{	// delta update from old position
			// because the force parm is false, this will not result
			// in any bytes being emited if the entity has not changed at all
			// note that players are always 'newentities', this updates their oldorigin always
			// and prevents warping
			MSG_WriteDeltaEntity (oldent, newent, msg, false, newent->number <= maxclients->value);
			oldindex++;
			newindex++;
			continue;
		}

		if (newnum < oldnum)
		{	// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity (&sv.baselines[newnum], newent, msg, true, true);
			newindex++;
			continue;
		}

		if (newnum > oldnum)
		{	
			// the old entity isn't present in the new message
			bits = U_REMOVE;
			if (oldnum >= 256) bits |= U_NUMBER16 | U_MOREBITS1;

			MSG_WriteByte (msg,	bits&255 );
			if (bits & 0x0000ff00) MSG_WriteByte (msg, (bits>>8)&255 );

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
	MSG_WriteShort (msg, 0);	// end of packetentities
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
	if (ps->pmove.pm_type != ops->pmove.pm_type) pflags |= PS_M_TYPE;
	if(!VectorCompare(ps->pmove.origin, ops->pmove.origin)) pflags |= PS_M_ORIGIN;
	if(!VectorCompare(ps->pmove.velocity, ops->pmove.velocity)) pflags |= PS_M_VELOCITY;
	if (ps->pmove.pm_time != ops->pmove.pm_time) pflags |= PS_M_TIME;
	if (ps->pmove.pm_flags != ops->pmove.pm_flags) pflags |= PS_M_FLAGS;
	if (ps->pmove.gravity != ops->pmove.gravity) pflags |= PS_M_GRAVITY;
	if(!VectorCompare(ps->pmove.delta_angles, ops->pmove.delta_angles)) pflags |= PS_M_DELTA_ANGLES;
	if(!VectorCompare(ps->viewoffset, ops->viewoffset)) pflags |= PS_VIEWOFFSET;
	if(!VectorCompare(ps->viewangles, ops->viewangles)) pflags |= PS_VIEWANGLES;
	if(!VectorCompare(ps->kick_angles, ops->kick_angles)) pflags |= PS_KICKANGLES;
		
	if (ps->blend[0] != ops->blend[0] || ps->blend[1] != ops->blend[1] || ps->blend[2] != ops->blend[2] || ps->blend[3] != ops->blend[3] )
		pflags |= PS_BLEND;

	if (ps->fov != ops->fov) pflags |= PS_FOV;
	if (ps->rdflags != ops->rdflags) pflags |= PS_RDFLAGS;
	if (ps->gunframe != ops->gunframe) pflags |= PS_WEAPONFRAME;
	if (ps->sequence != ops->sequence) pflags |= PS_WEAPONSEQUENCE;
	if (ps->gunbody != ops->gunbody) pflags |= PS_WEAPONBODY;
	if (ps->gunskin != ops->gunskin) pflags |= PS_WEAPONSKIN;

	pflags |= PS_WEAPONINDEX;

	// write it
	MSG_WriteByte (msg, svc_playerinfo);
	MSG_WriteLong (msg, pflags);

	// write the pmove_state_t
	if (pflags & PS_M_TYPE) MSG_WriteByte (msg, ps->pmove.pm_type);

	if (pflags & PS_M_ORIGIN) MSG_WritePos32(msg, ps->pmove.origin);
	if (pflags & PS_M_VELOCITY) MSG_WritePos32(msg, ps->pmove.velocity);
	if (pflags & PS_M_TIME) MSG_WriteByte (msg, ps->pmove.pm_time);
	if (pflags & PS_M_FLAGS) MSG_WriteByte (msg, ps->pmove.pm_flags);
	if (pflags & PS_M_GRAVITY) MSG_WriteShort (msg, ps->pmove.gravity);
	if (pflags & PS_M_DELTA_ANGLES) MSG_WritePos32(msg, ps->pmove.delta_angles);

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
		MSG_WriteByte (msg, ps->gunindex);
	}

	if (pflags & PS_WEAPONFRAME)
	{
		MSG_WriteByte (msg, ps->gunframe);
		MSG_WriteChar (msg, ps->gunoffset[0]*4);
		MSG_WriteChar (msg, ps->gunoffset[1]*4);
		MSG_WriteChar (msg, ps->gunoffset[2]*4);
		MSG_WriteChar (msg, ps->gunangles[0]*4);
		MSG_WriteChar (msg, ps->gunangles[1]*4);
		MSG_WriteChar (msg, ps->gunangles[2]*4);
	}

	if (pflags & PS_WEAPONSEQUENCE)
	{
		MSG_WriteByte (msg, ps->sequence);
	}

	if (pflags & PS_WEAPONBODY)
	{
		MSG_WriteByte (msg, ps->gunbody);
	}
	
	if (pflags & PS_WEAPONSKIN)
	{
		MSG_WriteByte (msg, ps->gunskin);
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
		MSG_WriteByte (msg, ps->rdflags);

	// send stats
	statbits = 0;
	for (i=0 ; i<MAX_STATS ; i++)
		if (ps->stats[i] != ops->stats[i])
			statbits |= 1<<i;
	MSG_WriteLong (msg, statbits);
	for (i=0 ; i<MAX_STATS ; i++)
		if (statbits & (1<<i) )
			MSG_WriteShort (msg, ps->stats[i]);
}


/*
==================
SV_WriteFrameToClient
==================
*/
void SV_WriteFrameToClient (client_state_t *client, sizebuf_t *msg)
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

byte		fatpvs[MAX_MAP_LEAFS/8];	// 32767 is MAX_MAP_LEAFS

/*
============
SV_FatPVS

The client will interpolate the view position,
so we can't use a single PVS point
===========
*/
void SV_FatPVS (vec3_t org)
{
	int		leafs[64];
	int		i, j, count;
	int		longs;
	byte		*src;
	vec3_t		mins, maxs;

	for (i = 0; i < 3; i++)
	{
		mins[i] = org[i] - SV_COORD_FRAC;
		maxs[i] = org[i] + SV_COORD_FRAC;
	}

	count = pe->BoxLeafnums (mins, maxs, leafs, 64, NULL);
	if (count < 1) Host_Error("SV_FatPVS: count < 1\n");
	longs = (pe->NumClusters()+31)>>5;

	// convert leafs to clusters
	for (i = 0; i < count; i++) leafs[i] = pe->LeafCluster(leafs[i]);

	Mem_Copy(fatpvs, pe->ClusterPVS(leafs[0]), longs<<2);

	// or in all the other leaf bits
	for (i = 1; i < count; i++)
	{
		for (j = 0; j < i; j++)
		{
			if (leafs[i] == leafs[j])
				break;
		}
		if (j != i) continue; // already have the cluster we want
		src = pe->ClusterPVS(leafs[i]);
		for (j = 0; j < longs; j++) ((long *)fatpvs)[j] |= ((long *)src)[j];
	}
}


/*
=============
SV_BuildClientFrame

Decides which entities are going to be visible to the client, and
copies off the playerstat and areabits.
=============
*/
void SV_BuildClientFrame (client_state_t *client)
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
	byte		*clientphs;
	byte		*bitvector;

	clent = client->edict;
	if (!clent->priv.sv->client) return;// not in game yet

	// this is the frame we are creating
	frame = &client->frames[sv.framenum & UPDATE_MASK];

	frame->senttime = svs.realtime; // save it for ping calc later

	// find the client's PVS
	VectorScale( clent->priv.sv->client->ps.pmove.origin, CL_COORD_FRAC, org ); 
	VectorAdd( org, clent->priv.sv->client->ps.viewoffset, org );  

	leafnum = pe->PointLeafnum (org);
	clientarea = pe->LeafArea (leafnum);
	clientcluster = pe->LeafCluster (leafnum);

	// calculate the visible areas
	frame->areabytes = pe->WriteAreaBits (frame->areabits, clientarea);

	// grab the current player_state_t
	frame->ps = clent->priv.sv->client->ps;

	SV_FatPVS (org);
	clientphs = pe->ClusterPHS (clientcluster);

	// build up the list of visible entities
	frame->num_entities = 0;
	frame->first_entity = svs.next_client_entities;

	c_fullsend = 0;

	for (e = 1; e < prog->num_edicts; e++)
	{
		ent = PRVM_EDICT_NUM(e);

		// ignore ents without visible models unless they have an effect
		if (!ent->progs.sv->modelindex && !ent->progs.sv->effects && !ent->progs.sv->noise3 && !ent->priv.sv->event)
			continue;

		// ignore if not touching a PV leaf
		if (ent != clent)
		{
			// check area
			if (!pe->AreasConnected (clientarea, ent->priv.sv->areanum))
			{	
				// doors can legally straddle two areas, so
				// we may need to check another one
				if (!ent->priv.sv->areanum2 || !pe->AreasConnected (clientarea, ent->priv.sv->areanum2))
					continue;	// blocked by a door
			}

			// beams just check one point for PHS
			if ((int)ent->progs.sv->renderfx & RF_BEAM)
			{
				l = ent->priv.sv->clusternums[0];
				if ( !(clientphs[l >> 3] & (1 << (l&7) )) )
					continue;
			}
			else
			{
				// FIXME: if an ent has a model and a sound, but isn't
				// in the PVS, only the PHS, clear the model
				if (ent->progs.sv->noise3)
				{
					bitvector = fatpvs;	//clientphs;
				}
				else bitvector = fatpvs;

				// check individual leafs
				/*if( !ent->priv.sv->num_clusters )
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
					if( svEnt->lastCluster )
					{
						for( ; l <= svEnt->lastCluster; l++ )
						{
							if( bitvector[l>>3] & (1<<(l&7)))
								break;
						}
						if ( l == svEnt->lastCluster )
							continue;	// not visible
					}
					else continue;
				}*/

				if (ent->priv.sv->num_clusters == -1)
				{	
				// too many leafs for individual check, go by headnode
				//if(!pe->HeadnodeVisible (ent->priv.sv->headnode, bitvector))
				//	continue;
					c_fullsend++;
				}
				else
				{	// check individual leafs
					for (i=0 ; i < ent->priv.sv->num_clusters ; i++)
					{
						l = ent->priv.sv->clusternums[i];
						if (bitvector[l >> 3] & (1 << (l&7) ))
							break;
					}
					if (i == ent->priv.sv->num_clusters)
						continue;		// not visible
				}

				if (!ent->progs.sv->modelindex)
				{	
					// don't send sounds if they will be attenuated away
					vec3_t	delta;
					float	len;

					VectorSubtract (org, ent->progs.sv->origin, delta);
					len = VectorLength (delta);
					if (len > 400) continue;
				}
			}
		}

		// add it to the circular client_entities array
		state = &svs.client_entities[svs.next_client_entities % svs.num_client_entities];
		if (ent->priv.sv->serialnumber != e)
		{
			MsgWarn ("SV_BuildClientFrame: invalid ent->priv.sv->serialnumber %d\n", ent->priv.sv->serialnumber );
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


/*
==================
SV_RecordDemoMessage

Save everything in the world out without deltas.
Used for recording footage for merged or assembled demos
==================
*/
void SV_RecordDemoMessage (void)
{
	int		e;
	edict_t		*ent;
	entity_state_t	nostate;
	sizebuf_t	buf;
	byte		buf_data[32768];
	int		len;

	if (!svs.demofile) return;

	memset (&nostate, 0, sizeof(nostate));
	SZ_Init (&buf, buf_data, sizeof(buf_data));

	// write a frame message that doesn't contain a player_state_t
	MSG_WriteByte (&buf, svc_frame);
	MSG_WriteLong (&buf, sv.framenum);
	MSG_WriteByte (&buf, svc_packetentities);

	e = 1;
	ent = PRVM_EDICT_NUM(e);
	while (e < prog->num_edicts) 
	{
		// ignore ents without visible models unless they have an effect
		if (!ent->priv.sv->free && ent->priv.sv->serialnumber && (ent->priv.sv->s.modelindex || ent->progs.sv->effects || ent->progs.sv->noise3 || ent->priv.sv->event))
		{
			SV_UpdateEntityState( ent );
			MSG_WriteDeltaEntity (&nostate, &ent->priv.sv->s, &buf, false, true);
		}
		e++;
		ent = PRVM_EDICT_NUM(e);
	}

	MSG_WriteShort (&buf, 0);		// end of packetentities

	// now add the accumulated multicast information
	SZ_Write (&buf, svs.demo_multicast.data, svs.demo_multicast.cursize);
	SZ_Clear (&svs.demo_multicast);

	// now write the entire message to the file, prefixed by the length
	len = LittleLong (buf.cursize);
	FS_Write (svs.demofile, &len, 4);
	FS_Write (svs.demofile, buf.data, buf.cursize);
}

