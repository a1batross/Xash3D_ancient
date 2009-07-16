//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_frame.c - server world snapshot
//=======================================================================

#include "common.h"
#include "server.h"
#include "const.h"

#define MAX_VISIBLE_PACKET		1024
typedef struct
{
	int	num_entities;
	int	entities[MAX_VISIBLE_PACKET];	
} sv_ents_t;

static byte *clientpvs;	// FatPVS
static byte *clientphs;	// ClientPHS

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

Copy entvars into entity state

=============================================================================
*/
void SV_UpdateEntityState( edict_t *ent, bool baseline )
{
	// copy progs values to state
	ent->pvServerData->s.number = ent->serialnumber;

	if( !ent->pvServerData->s.classname )
		ent->pvServerData->s.classname = SV_ClassIndex( STRING( ent->v.classname ));

	svgame.dllFuncs.pfnUpdateEntityState( &ent->pvServerData->s, ent, baseline );

	switch( ent->pvServerData->s.ed_type )
	{
	case ED_MOVER:
	case ED_BSPBRUSH:
	case ED_PORTAL:
		ent->pvServerData->s.skin = DirToByte( ent->v.movedir );
		break;
	}
}

/*
===============
SV_AddEntToSnapshot
===============
*/
static void SV_AddEntToSnapshot( sv_priv_t *svent, edict_t *ent, sv_ents_t *ents )
{
	// if we have already added this entity to this snapshot, don't add again
	if( svent->framenum == sv.net_framenum ) return;
	svent->framenum = sv.net_framenum;

	// if we are full, silently discard entities
	if( ents->num_entities == MAX_VISIBLE_PACKET )
	{
		MsgDev( D_ERROR, "too many entities in visible packet list\n" );
		return;
	}

	SV_UpdateEntityState( ent, false ); // copy entity state from progs
	ents->entities[ents->num_entities] = ent->serialnumber;
	ents->num_entities++;
	c_fullsend++; // debug counter
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
			MSG_WriteDeltaEntity( oldent, newent, msg, false,
			(newent->ed_type == ED_CLIENT) || (newent->ed_type == ED_PORTAL));
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
			// remove from message
			MSG_WriteDeltaEntity( oldent, NULL, msg, false, false );
			oldindex++;
			continue;
		}
	}
	MSG_WriteBits( msg, 0, "svc_packetentities", NET_WORD ); // end of packetentities
}

static void SV_AddEntitiesToPacket( vec3_t origin, client_frame_t *frame, sv_ents_t *ents, bool portal )
{
	int		l, e, i;
	edict_t		*ent;
	sv_priv_t		*svent;
	int		leafnum;
	byte		*clientphs;
	byte		*bitvector;
	int		clientarea;
	int		clientcluster;
	bool		force = false;

	// during an error shutdown message we may need to transmit
	// the shutdown message after the server has shutdown, so
	// specfically check for it
	if( !sv.state ) return;

	clientpvs = pe->FatPVS( origin, portal );

	leafnum = pe->PointLeafnum( origin );
	clientarea = pe->LeafArea( leafnum );
	clientcluster = pe->LeafCluster( leafnum );

	// calculate the visible areas
	frame->areabits_size = pe->WriteAreaBits( frame->areabits, clientarea, portal );
	clientphs = pe->FatPHS( clientcluster, portal );

	for( e = 1; e < svgame.globals->numEntities; e++ )
	{
		ent = EDICT_NUM( e );
		if( ent->free ) continue;
		force = false; // clear forceflag

		// completely ignore dormant entity
		if( ent->v.flags & FL_DORMANT )
			continue;

		// NOTE: client index on client expected that entity will be valid
		if( ent->pvServerData->s.ed_type == ED_CLIENT )
			force = true;

		if( ent->pvServerData->s.ed_type == ED_SKYPORTAL )
			force = true;

		// never send entities that aren't linked in
		if( !ent->pvServerData->linked && !force ) continue;

		if( ent->serialnumber != e )
		{
			MsgDev( D_WARN, "fixing ent->pvServerData->serialnumber\n");
			ent->serialnumber = e;
		}

		svent = ent->pvServerData;

		// quick reject by type
		switch( svent->s.ed_type )
		{
		case ED_MOVER:
		case ED_NORMAL:
		case ED_PORTAL:
		case ED_MONSTER:
		case ED_AMBIENT:
		case ED_BSPBRUSH:
		case ED_RIGIDBODY: break;
		default: if( !force ) continue;
		}

		// don't double add an entity through portals
		if( svent->framenum == sv.net_framenum ) continue;

		if( !force )
		{
			// ignore if not touching a PV leaf check area
			if( !pe->AreasConnected( clientarea, ent->pvServerData->areanum ))
			{
				// doors can legally straddle two areas, so
				// we may need to check another one
				if( !pe->AreasConnected( clientarea, ent->pvServerData->areanum2 ))
					continue;	// blocked by a door
			}
		}

		if( svent->s.ed_type == ED_AMBIENT || svent->s.ed_type == ED_PORTAL )
			bitvector = clientphs;
		else bitvector = clientpvs;

		if( !force )
		{ 
			// check individual leafs
			if( !svent->num_clusters ) continue;
			for( i = l = 0; i < svent->num_clusters && !force; i++ )
			{
				l = svent->clusternums[i];
				if( bitvector[l>>3] & (1<<(l & 7)))
					break;
			}

			// if we haven't found it to be visible,
			// check overflow clusters that coudln't be stored
			if( i == svent->num_clusters )
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
		}

		if( ent->pvServerData->s.ed_type == ED_AMBIENT )
		{	
			vec3_t	delta, entorigin;
			float	len;

			// don't send sounds if they will be attenuated away
			if( VectorIsNull( ent->v.origin ))
				VectorAverage( ent->v.mins, ent->v.maxs, entorigin );
			else VectorCopy( ent->v.origin, entorigin );

			VectorSubtract( origin, entorigin, delta );	
			len = VectorLength( delta );
			if( len > 400 ) continue;
		}

		// add it
		SV_AddEntToSnapshot( svent, ent, ents );

		// if its a portal entity, add everything visible from its camera position
		if( svent->s.ed_type == ED_PORTAL || svent->s.ed_type == ED_SKYPORTAL )
		{
			// don't merge pvs for mirrors
			if( !VectorCompare( svent->s.origin, svent->s.oldorigin ))
				SV_AddEntitiesToPacket( svent->s.oldorigin, frame, ents, true );
		}
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

	MSG_WriteByte( msg, svc_time );
	MSG_WriteFloat( msg, sv.time );		// send a servertime before each frame

	MSG_WriteByte( msg, svc_frame );
	MSG_WriteLong( msg, sv.framenum );
	MSG_WriteFloat( msg, sv.frametime );
	MSG_WriteLong( msg, lastframe );		// what we are delta'ing from
	MSG_WriteByte( msg, cl->surpressCount );	// rate dropped packets
	cl->surpressCount = 0;

	// send over the areabits
	MSG_WriteByte( msg, frame->areabits_size );	// never more than 255 bytes
	MSG_WriteData( msg, frame->areabits, frame->areabits_size );

	// just send an client index
	// it's safe, because NUM_FOR_EDICT always equal ed->serialnumber,
	// thats shared across network
	MSG_WriteByte( msg, svc_playerinfo );
	MSG_WriteByte( msg, frame->index );

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
	frame->senttime = svs.realtime; // save it for ping calc later

	// clear everything in this snapshot
	frame_ents.num_entities = c_fullsend = 0;
	Mem_Set( frame->areabits, 0, sizeof( frame->areabits ));
	if( !clent->pvServerData->client ) return; // not in game yet

	// find the client's PVS
	VectorCopy( clent->pvServerData->s.origin, org ); 
	VectorAdd( org, clent->pvServerData->s.viewoffset, org );  

	// grab the current player index
	frame->index = NUM_FOR_EDICT( clent );

	// add all the entities directly visible to the eye, which
	// may include portal entities that merge other viewpoints
	SV_AddEntitiesToPacket( org, frame, &frame_ents, false );

	// if there were portals visible, there may be out of order entities
	// in the list which will need to be resorted for the delta compression
	// to work correctly.  This also catches the error condition
	// of an entity being included twice.
	qsort( frame_ents.entities, frame_ents.num_entities, sizeof( frame_ents.entities[0] ), SV_EntityNumbers );

	// copy the entity states out
	frame->num_entities = 0;
	frame->first_entity = svs.next_client_entities;

	for( i = 0; i < frame_ents.num_entities; i++ )
	{
		ent = EDICT_NUM( frame_ents.entities[i] );

		// add it to the circular client_entities array
		state = &svs.client_entities[svs.next_client_entities % svs.num_client_entities];
		*state = ent->pvServerData->s;
		svs.next_client_entities++;

		// this should never hit, map should always be restarted first in SV_Frame
		if( svs.next_client_entities >= 0x7FFFFFFE )
			Host_Error( "svs.next_client_entities wrapped (sv.time limit is out)\n" );
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
	cl->message_size[sv.framenum % RATE_MESSAGES] = msg.cursize;

	return true;
}

/*
=======================
SV_RateDrop

Returns true if the client is over its current
bandwidth estimation and should not be sent another packet
=======================
*/
#if 0
bool SV_RateDrop( sv_client_t *cl )
{
	int	i, total = 0;

	// never drop over the loopback
	if( NET_IsLocalAddress( cl->netchan.remote_address ))
		return false;

	for( i = 0; i < RATE_MESSAGES; i++ )
		total += cl->message_size[i];

	if( total > cl->rate )
	{
		cl->surpressCount++;
		cl->message_size[sv.framenum % RATE_MESSAGES] = 0;
		return true;
	}
	return false;
}
#else
bool SV_RateDrop( sv_client_t *cl )
{
	// never drop over the loopback
	if( NET_IsLocalAddress( cl->netchan.remote_address ))
		return false;

	// only send messages if the client has sent one
	// and the bandwidth is not supressed
	if( !cl->send_message ) return true;

	cl->send_message = false;	// try putting this after choke?
	if( !sv_paused->integer && !Netchan_CanPacket( &cl->netchan ))
	{
		cl->surpressCount++;
		return true;	// bandwidth choke
	}
	return false;
}
#endif

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
		else cl->send_message = true;
			
		if( cl->edict && (cl->edict->v.flags & FL_FAKECLIENT))
			continue;

		// if the reliable message overflowed, drop the client
		if( cl->netchan.message.overflowed )
		{
			MSG_Clear( &cl->netchan.message );
			MSG_Clear( &cl->datagram );
			SV_BroadcastPrintf( PRINT_HIGH, "%s overflowed\n", cl->name );
			SV_DropClient( cl );
			cl->send_message = true;
			cl->netchan.cleartime = 0;	// don't supress this message
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
			if( cl->netchan.message.cursize || host.realtime - cl->netchan.last_sent > 1.0 )
				Netchan_Transmit( &cl->netchan, 0, NULL );
		}
	}
}