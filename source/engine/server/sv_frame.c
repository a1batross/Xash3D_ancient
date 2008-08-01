//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_frame.c - server world snapshot
//=======================================================================

#include "common.h"
#include "server.h"

static byte fatpvs[MAX_MAP_LEAFS/8];

/*
============
SV_FatPVS

The client will interpolate the view position,
so we can't use a single PVS point
===========
*/
void SV_FatPVS( vec3_t org )
{
	int	leafs[64];
	int	i, j, count;
	int	longs;
	byte	*src;
	vec3_t	mins, maxs;

	for( i = 0; i < 3; i++ )
	{
		mins[i] = org[i] - 8;
		maxs[i] = org[i] + 8;
	}

	count = pe->BoxLeafnums( mins, maxs, leafs, 64, NULL );
	if( count < 1 ) Host_Error( "SV_FatPVS: invalid leafcount\n" );
	longs = (pe->NumClusters() + 31)>>5;

	// convert leafs to clusters
	for( i = 0; i < count; i++) leafs[i] = pe->LeafCluster( leafs[i] );

	Mem_Copy( fatpvs, pe->ClusterPVS( leafs[0] ), longs<<2 );

	// or in all the other leaf bits
	for( i = 1; i < count; i++ )
	{
		for( j = 0; j < i; j++ )
		{
			if( leafs[i] == leafs[j] )
				break;
		}
		if( j != i ) continue; // already have the cluster we want
		src = pe->ClusterPVS( leafs[i] );
		for( j = 0; j < longs; j++ ) ((long *)fatpvs)[j] |= ((long *)src)[j];
	}
}


/*
=============================================================================

Copy PRVM values into entity state

=============================================================================
*/
void SV_UpdateEntityState( edict_t *ent )
{
	// copy progs values to state
	ent->priv.sv->s.number = ent->priv.sv->serialnumber;
	ent->priv.sv->s.solid = ent->priv.sv->solid;

	VectorCopy (ent->progs.sv->origin, ent->priv.sv->s.origin);
	VectorCopy (ent->progs.sv->angles, ent->priv.sv->s.angles);
	VectorCopy (ent->progs.sv->old_origin, ent->priv.sv->s.old_origin);
	ent->priv.sv->s.model.index = (int)ent->progs.sv->modelindex;
	ent->priv.sv->s.health = ent->progs.sv->health;
	ent->priv.sv->s.model.skin = (short)ent->progs.sv->skin;		// studio model skin
	ent->priv.sv->s.model.body = (byte)ent->progs.sv->body;		// studio model submodel 
	ent->priv.sv->s.model.frame = ent->progs.sv->frame;		// any model current frame
	ent->priv.sv->s.model.sequence = (byte)ent->progs.sv->sequence;	// studio model sequence
	ent->priv.sv->s.effects = (uint)ent->progs.sv->effects;		// shared client and render flags
	ent->priv.sv->s.renderfx = (int)ent->progs.sv->renderfx;		// renderer flags
	ent->priv.sv->s.renderamt = ent->progs.sv->alpha;			// alpha value
	ent->priv.sv->s.model.animtime = ent->progs.sv->animtime;		// auto-animating time

	// copy viewmodel info
	ent->priv.sv->s.vmodel.frame = ent->progs.sv->v_frame;
	ent->priv.sv->s.vmodel.body = ent->progs.sv->v_body;
	ent->priv.sv->s.vmodel.skin = ent->progs.sv->v_skin;
	ent->priv.sv->s.vmodel.sequence = ent->progs.sv->v_sequence;
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
void SV_EmitPacketEntities( client_frame_t *from, client_frame_t *to, sizebuf_t *msg )
{
	entity_state_t	*oldent, *newent;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		from_num_entities;

	MSG_WriteByte( msg, svc_packetentities );

	if( !from ) from_num_entities = 0;
	else from_num_entities = from->num_entities;

	newindex = 0;
	oldindex = 0;
	while( newindex < to->num_entities || oldindex < from_num_entities )
	{
		if( newindex >= to->num_entities ) newnum = MAX_ENTNUMBER;
		else
		{
			newent = &svs.client_entities[(to->first_entity+newindex)%svs.num_client_entities];
			newnum = newent->number;
		}

		if( oldindex >= from_num_entities ) oldnum = MAX_ENTNUMBER;
		else
		{
			oldent = &svs.client_entities[(from->first_entity+oldindex)%svs.num_client_entities];
			oldnum = oldent->number;
		}

		if( newnum == oldnum )
		{	
			// delta update from old position
			// because the force parm is false, this will not result
			// in any bytes being emited if the entity has not changed at all
			// note that players are always 'newentities', this updates their oldorigin always
			// and prevents warping
			MSG_WriteDeltaEntity( oldent, newent, msg, false, newent->number <= Host_MaxClients());
			oldindex++;
			newindex++;
			continue;
		}
		if( newnum < oldnum )
		{	
			// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity( &svs.baselines[newnum], newent, msg, true, true );
			newindex++;
			continue;
		}
		if( newnum > oldnum )
		{	
			MSG_WriteDeltaEntity( oldent, NULL, msg, true, true );
			oldindex++;
			continue;
		}
	}
	MSG_WriteBits( msg, 0, NET_WORD ); // end of packetentities
}

/*
==================
SV_WriteFrameToClient
==================
*/
void SV_WriteFrameToClient( sv_client_t *cl, sizebuf_t *msg )
{
	client_frame_t	*frame, *oldframe;
	int		lastframe;

	// this is the frame we are creating
	frame = &cl->frames[sv.framenum & UPDATE_MASK];

	if( cl->lastframe <= 0 )
	{	
		// client is asking for a retransmit
		oldframe = NULL;
		lastframe = -1;
	}
	else if( sv.framenum - cl->lastframe >= (UPDATE_BACKUP - 3))
	{
		// client hasn't gotten a good message through in a long time
		oldframe = NULL;
		lastframe = -1;
	}
	else
	{	// we have a valid message to delta from
		oldframe = &cl->frames[cl->lastframe & UPDATE_MASK];
		lastframe = cl->lastframe;
	}

	MSG_WriteByte( msg, svc_frame );
	MSG_WriteLong( msg, sv.framenum );
	MSG_WriteLong( msg, lastframe );		// what we are delta'ing from
	MSG_WriteByte( msg, cl->surpressCount );	// rate dropped packets
	cl->surpressCount = 0;

	// send over the areabits
	MSG_WriteByte( msg, frame->areabytes );
	MSG_WriteData( msg, frame->areabits, frame->areabytes );

	// delta encode the playerstate
	MSG_WriteDeltaPlayerstate( &oldframe->ps, &frame->ps, msg );

	// delta encode the entities
	SV_EmitPacketEntities( oldframe, frame, msg );
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
void SV_BuildClientFrame( sv_client_t *cl )
{
	int		i, e, l;
	vec3_t		org;
	edict_t		*ent;
	edict_t		*clent;
	client_frame_t	*frame;
	entity_state_t	*state;
	int		clientarea;
	int		clientcluster;
	int		leafnum;
	byte		*clientpvs;
	byte		*clientphs;
	byte		*bitvector;

	clent = cl->edict;
	if( !clent->priv.sv->client )
		return; // not in game yet

	// this is the frame we are creating
	frame = &cl->frames[sv.framenum & UPDATE_MASK];

	frame->msg_sent = svs.realtime; // save it for ping calc later

	// find the client's PVS
	VectorCopy( clent->priv.sv->s.origin, org ); 
	VectorAdd( org, clent->priv.sv->s.viewoffset, org );  

	// calculate fat pvs
	if( sv_fatpvs->integer ) SV_FatPVS( org );

	leafnum = pe->PointLeafnum( org );
	clientarea = pe->LeafArea( leafnum );
	clientcluster = pe->LeafCluster( leafnum );

	// calculate the visible areas
	frame->areabytes = pe->WriteAreaBits( frame->areabits, clientarea );

	// grab the current player state
	frame->ps = clent->priv.sv->s;

	clientpvs = pe->ClusterPVS( clientcluster );
	clientphs = pe->ClusterPHS( clientcluster );

	// build up the list of visible entities
	frame->num_entities = 0;
	frame->first_entity = svs.next_client_entities;

	for( e = 1; e < prog->num_edicts; e++ )
	{
		ent = PRVM_EDICT_NUM(e);

		// ignore ents without visible models unless they have an effect
		if( !ent->progs.sv->modelindex && !ent->progs.sv->effects && !ent->priv.sv->s.soundindex )
			continue;

		// ignore if not touching a PV leaf
		if( ent != clent )
		{
			// check area
			if( !pe->AreasConnected( clientarea, ent->priv.sv->areanum ))
			{	
				// doors can legally straddle two areas, so
				// we may need to check another one
				int areanum2 = ent->priv.sv->areanum2; 
				if( !areanum2 || !pe->AreasConnected( clientarea, areanum2 ))
					continue;	// blocked by a door
			}

			// FIXME: if an ent has a model and a sound, but isn't
			// in the PVS, only the PHS, clear the model
			if( ent->priv.sv->s.soundindex ) bitvector = clientphs;
			else if( sv_fatpvs->integer ) bitvector = fatpvs;
			else bitvector = clientpvs;

			// check individual leafs
			if( !ent->priv.sv->num_clusters ) continue;
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
			MsgDev( D_WARN, "SV_BuildClientFrame: invalid number %d\n", ent->priv.sv->serialnumber );
			ent->priv.sv->serialnumber = e; // ptr to current entity such as entnumber
		}

                    SV_UpdateEntityState( ent );
		*state = ent->priv.sv->s;

		// don't mark players missiles as solid
		if( PRVM_PROG_TO_EDICT( ent->progs.sv->owner) == cl->edict )
			state->solid = 0;

		svs.next_client_entities++;
		frame->num_entities++;
	}
}

/*
===============================================================================

FRAME UPDATES

===============================================================================
*/
/*
=======================
SV_SendClientDatagram
=======================
*/
bool SV_SendClientDatagram( sv_client_t *cl )
{
	byte		msg_buf[MAX_MSGLEN];
	sizebuf_t		msg;

	SV_BuildClientFrame( cl );

	MSG_Init( &msg, msg_buf, sizeof(msg_buf));

	// send over all the relevant entity_state_t
	// and the player state
	SV_WriteFrameToClient( cl, &msg );

	// copy the accumulated multicast datagram
	// for this client out to the message
	// it is necessary for this to be after the WriteEntities
	// so that entity references will be current
	if( cl->datagram.overflowed ) MsgDev( D_WARN, "datagram overflowed for %s\n", cl->name );
	else MSG_WriteData( &msg, cl->datagram.data, cl->datagram.cursize );
	MSG_Clear( &cl->datagram );

	if( msg.overflowed )
	{	
		// must have room left for the packet header
		MsgDev( D_WARN, "msg overflowed for %s\n", cl->name );
		MSG_Clear( &msg );
	}
	// send the datagram
	Netchan_Transmit( &cl->netchan, msg.cursize, msg.data );

	// record the size for rate estimation
	// record information about the message
	cl->frames[cl->netchan.outgoing_sequence & UPDATE_MASK].msg_size = msg.cursize;
	cl->frames[cl->netchan.outgoing_sequence & UPDATE_MASK].msg_sent = svs.realtime;

	return true;
}

/*
=======================
SV_RateDrop

Returns true if the client is over its current
bandwidth estimation and should not be sent another packet
=======================
*/
bool SV_RateDrop( sv_client_t *cl )
{
	int	i, total = 0;

	// never drop over the loopback
	if( NET_IsLocalAddress( cl->netchan.remote_address ))
		return false;
	
	if( NET_IsLANAddress( cl->netchan.remote_address ))
		return false;

	for( i = 0; i < UPDATE_BACKUP; i++ )
		total += cl->frames[i].msg_size;

	if( total > cl->rate )
	{
		cl->surpressCount++;
		cl->frames[cl->netchan.outgoing_sequence & UPDATE_MASK].msg_size = 0;
		return true;
	}
	return false;
}

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages( void )
{
	sv_client_t	*cl;
	int		i;

	// send a message to each connected client
	for( i = 0, cl = svs.clients; i < Host_MaxClients(); i++, cl++ )
	{
		if( !cl->state ) continue;

		// if the reliable message overflowed, drop the client
		if( cl->netchan.message.overflowed )
		{
			MSG_Clear( &cl->netchan.message );
			MSG_Clear( &cl->datagram );
			SV_BroadcastPrintf (PRINT_CONSOLE, "%s overflowed\n", cl->name );
			SV_DropClient( cl );
		}

		if( sv.state == ss_cinematic )
		{
			Netchan_Transmit( &cl->netchan, 0, NULL );
		}
		else if( cl->state == cs_spawned )
		{
			// don't overrun bandwidth
			if( SV_RateDrop( cl )) continue;
			SV_SendClientDatagram( cl );
		}
		else
		{
			// just update reliable if needed
			if( cl->netchan.message.cursize || svs.realtime - cl->netchan.last_sent > 1000 )
				Netchan_Transmit( &cl->netchan, 0, NULL );
		}
	}
}