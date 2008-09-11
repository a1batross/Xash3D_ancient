//=======================================================================
//			Copyright XashXT Group 2008 �
//		        sv_frame.c - server world snapshot
//=======================================================================

#include "common.h"
#include "server.h"

#define MAX_VISIBLE_PACKET		1024
typedef struct
{
	int	num_entities;
	int	entities[MAX_VISIBLE_PACKET];	
} sv_ents_t;

int	c_fullsend;

/*
=======================
SV_EntityNumbers
=======================
*/
static int SV_EntityNumbers( const void *a, const void *b )
{
	int	*ea, *eb;

	ea = (int *)a;
	eb = (int *)b;

	if( *ea == *eb )
		Host_Error( "SV_EntityNumbers: duplicated entity\n" );
	if( *ea < *eb ) return -1;
	return 1;
}

/*
=============================================================================

Copy PRVM values into entity state

=============================================================================
*/
void SV_UpdateEntityState( edict_t *ent )
{
	edict_t	*client;

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
	ent->priv.sv->s.model.gaitsequence = (int)ent->progs.sv->gaitsequence;// player sequence, that will be playing on client
	ent->priv.sv->s.model.sequence = (byte)ent->progs.sv->sequence;	// studio model sequence
	ent->priv.sv->s.effects = (uint)ent->progs.sv->effects;		// shared client and render flags
	ent->priv.sv->s.renderfx = (int)ent->progs.sv->renderfx;		// renderer flags
	ent->priv.sv->s.renderamt = ent->progs.sv->renderamt;		// alpha value
	ent->priv.sv->s.model.framerate = ent->progs.sv->framerate;
	ent->priv.sv->s.model.animtime = (int)(1000.0 * ent->progs.sv->animtime) * 0.001; // sequence time
	ent->priv.sv->s.aiment = ent->progs.sv->aiment;			// viewmodel parent

	if( ent->priv.sv->s.ed_type == ED_VIEWMODEL )
	{
		// copy v_model state from client to viemodel entity
		client = PRVM_EDICT_NUM( ent->progs.sv->aiment );

		// update both arrays, because viewmodel are hidden for qc-coders
		ent->priv.sv->s.model.index = ent->progs.sv->modelindex = SV_ModelIndex(PRVM_GetString(client->progs.sv->v_model));
		ent->priv.sv->s.model.frame = ent->progs.sv->frame = client->progs.sv->v_frame;
		ent->priv.sv->s.model.body = ent->progs.sv->body = client->progs.sv->v_body;
		ent->priv.sv->s.model.skin = ent->progs.sv->skin = client->progs.sv->v_skin;
		ent->priv.sv->s.model.sequence = ent->progs.sv->sequence = client->progs.sv->v_sequence;
		VectorCopy( ent->progs.sv->origin, ent->priv.sv->s.old_origin );
		ent->priv.sv->s.model.colormap = ent->progs.sv->colormap = client->progs.sv->colormap;
	}
}

/*
===============
SV_AddEntToSnapshot
===============
*/
static void SV_AddEntToSnapshot( sv_edict_t *svent, edict_t *ent, sv_ents_t *ents )
{
	// if we have already added this entity to this snapshot, don't add again
	if( svent->framenum == sv.net_framenum ) return;
	svent->framenum = sv.net_framenum;

	// if we are full, silently discard entities
	if( ents->num_entities == MAX_VISIBLE_PACKET ) return;

	SV_UpdateEntityState( ent ); // copy entity state from progs
	ents->entities[ents->num_entities] = svent->serialnumber;
	ents->num_entities++;
	c_fullsend++;		// debug counter
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

	newent = NULL;
	oldent = NULL;
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
			MSG_WriteDeltaEntity( oldent, newent, msg, false, (newent->ed_type == ED_CLIENT));
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

static void SV_AddEntitiesToPacket( vec3_t origin, client_frame_t *frame, sv_ents_t *ents, bool portal )
{
	int		l, e, i;
	edict_t		*ent;
	sv_edict_t	*svent;
	int		leafnum;
	byte		*clientpvs;
	byte		*bitvector;
	int		clientarea, clientcluster;
	bool		force = false;

	// during an error shutdown message we may need to transmit
	// the shutdown message after the server has shutdown, so
	// specfically check for it
	if( !sv.state ) return;

	leafnum = pe->PointLeafnum( origin );
	clientarea = pe->LeafArea( leafnum );
	clientcluster = pe->LeafCluster( leafnum );

	// calculate the visible areas
	frame->areabits_size = pe->WriteAreaBits( frame->areabits, clientarea );
	clientpvs = pe->ClusterPVS( clientcluster );

	for( e = 0; e < prog->num_edicts; e++ )
	{
		ent = PRVM_EDICT_NUM( e );
		force = false; // clear forceflag

		// send viewmodel entity always
		// NOTE: never apply LinkEdict to viewmodel entity, because
		// we wan't see it in list of entities returned with SV_AreaEdicts
		if( ent->priv.sv->s.ed_type == ED_VIEWMODEL )
			force = true;

		// NOTE: client index on client expected that entity will be valid
		if( sv_newprotocol->integer && ent->priv.sv->s.ed_type == ED_CLIENT )
			force = true;
		
		// never send entities that aren't linked in
		if( !ent->priv.sv->linked && !force ) continue;

		if( ent->priv.sv->serialnumber != e )
		{
			MsgDev( D_WARN, "fixing ent->priv.sv->serialnumber\n");
			ent->priv.sv->serialnumber = e;
		}

		svent = ent->priv.sv;

		// don't double add an entity through portals
		if( svent->framenum == sv.net_framenum ) continue;

		// ignore if not touching a PV leaf check area
		if( !pe->AreasConnected( clientarea, ent->priv.sv->areanum ))
		{
			// doors can legally straddle two areas, so
			// we may need to check another one
			if( !pe->AreasConnected( clientarea, ent->priv.sv->areanum2 ))
				continue;	// blocked by a door
		}
		bitvector = clientpvs;

		// check individual leafs
		if( !svent->num_clusters && !force ) continue;
		for( i = l = 0; i < svent->num_clusters && !force; i++ )
		{
			l = svent->clusternums[i];
			if( bitvector[l>>3] & (1<<(l & 7)))
				break;
		}

		// if we haven't found it to be visible,
		// check overflow clusters that coudln't be stored
		if( !force && i == svent->num_clusters )
		{
			if( svent->lastcluster )
			{
				for( ; l <= svent->lastcluster; l++ )
				{
					if( bitvector[l>>3] & (1<<(l & 7)))
						break;
				}
				if( l == svent->lastcluster )
					continue;	// not visible
			}
			else continue;
		}

		if( ent->priv.sv->s.ed_type == ED_AMBIENT )
		{	
			// don't send sounds if they will be attenuated away
			vec3_t	delta, entorigin;
			float	len;

			if(VectorIsNull( ent->progs.sv->origin ))
			{
				VectorAverage( ent->progs.sv->mins, ent->progs.sv->maxs, entorigin );
			}
			else
			{
				VectorCopy( ent->progs.sv->origin, entorigin );
			}

			VectorSubtract( origin, entorigin, delta );	
			len = VectorLength( delta );
			if( len > 400 ) continue;
		}

		// add it
		SV_AddEntToSnapshot( svent, ent, ents );

		// if its a portal entity, add everything visible from its camera position
		if( svent->s.ed_type == ED_PORTAL )
			SV_AddEntitiesToPacket( svent->s.infotarget, frame, ents, true );
	}
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

		// the snapshot's entities may still have rolled off the buffer, though
		if( oldframe->first_entity <= svs.next_client_entities - svs.num_client_entities )
		{
			MsgDev( D_WARN, "%s: delta request from out of date entities.\n", cl->name );
			oldframe = NULL;
			lastframe = 0;
		}
	}

	MSG_WriteByte( msg, svc_frame );
	MSG_WriteLong( msg, sv.framenum );
	MSG_WriteLong( msg, lastframe );		// what we are delta'ing from
	MSG_WriteByte( msg, cl->surpressCount );	// rate dropped packets
	cl->surpressCount = 0;

	// send over the areabits
	MSG_WriteByte( msg, frame->areabits_size );
	MSG_WriteData( msg, frame->areabits, frame->areabits_size );

	// just send an client index
	// it's safe, because PRVM_NUM_FOR_EDICT always equal ed->serialnumber,
	// thats shared across network
	if( sv_newprotocol->integer )
	{
		MSG_WriteByte( msg, svc_playerinfo );
		MSG_WriteByte( msg, frame->index );
	}
	else
	{
		// delta encode the playerstate
		MSG_WriteDeltaPlayerstate( &oldframe->ps, &frame->ps, msg );
	}

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
	vec3_t		org;
	edict_t		*ent;
	edict_t		*clent;
	client_frame_t	*frame;
	entity_state_t	*state;
	sv_ents_t		frame_ents;
	int		i;

	clent = cl->edict;
	sv.net_framenum++;

	// this is the frame we are creating
	frame = &cl->frames[sv.framenum & UPDATE_MASK];
	frame->msg_sent = svs.realtime; // save it for ping calc later

	// clear everything in this snapshot
	frame_ents.num_entities = c_fullsend = 0;
	memset( frame->areabits, 0, sizeof( frame->areabits ));
	if( !clent->priv.sv->client ) return; // not in game yet

	// find the client's PVS
	VectorCopy( clent->priv.sv->s.origin, org ); 
	VectorAdd( org, clent->priv.sv->s.viewoffset, org );  

	if( sv_newprotocol->integer )
	{
		// grab the current player index
		frame->index = PRVM_NUM_FOR_EDICT( clent );
	}
	else
	{
		// grab the current player state
		cl->edict->priv.sv->framenum = sv.net_framenum;
		frame->ps = clent->priv.sv->s;
	}

	// add all the entities directly visible to the eye, which
	// may include portal entities that merge other viewpoints
	SV_AddEntitiesToPacket( org, frame, &frame_ents, false );

	// if there were portals visible, there may be out of order entities
	// in the list which will need to be resorted for the delta compression
	// to work correctly.  This also catches the error condition
	// of an entity being included twice.
	qsort( frame_ents.entities, frame_ents.num_entities, sizeof( frame_ents.entities[0] ), SV_EntityNumbers );

	// now that all viewpoint's areabits have been OR'd together, invert
	// all of them to make it a mask vector, which is what the renderer wants
	for( i = 0; i < MAX_MAP_AREA_BYTES / 4; i++ )
		((int *)frame->areabits)[i] = ((int *)frame->areabits)[i] ^ -1;

	// copy the entity states out
	frame->num_entities = 0;
	frame->first_entity = svs.next_client_entities;

	for( i = 0; i < frame_ents.num_entities; i++ )
	{
		ent = PRVM_EDICT_NUM( frame_ents.entities[i] );

		// add it to the circular client_entities array
		state = &svs.client_entities[svs.next_client_entities % svs.num_client_entities];
		*state = ent->priv.sv->s;
		svs.next_client_entities++;

		// this should never hit, map should always be restarted first in SV_Frame
		if( svs.next_client_entities >= 0x7FFFFFFE )
			Host_Error( "svs.next_client_entities wrapped (sv.time integer limit is out)\n" );
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

	MSG_Init( &msg, msg_buf, sizeof( msg_buf ));

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
			SV_BroadcastPrintf( PRINT_CONSOLE, "%s overflowed\n", cl->name );
			SV_DropClient( cl );
		}

		if( cl->state == cs_spawned )
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