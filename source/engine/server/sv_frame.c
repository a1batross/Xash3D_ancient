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
static byte *clientphs;	// FatPHS

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
void SV_UpdateEntityState( const edict_t *ent, bool baseline )
{
	sv_client_t	*client = SV_ClientFromEdict( ent, true );

	if( !ent->pvServerData->s.classname )
		ent->pvServerData->s.classname = SV_ClassIndex( STRING( ent->v.classname ));

	if( client )
	{
		SV_SetIdealPitch( client );

		if( ent->v.fixangle )
		{
			MSG_WriteByte( &sv.multicast, svc_setangle );
			MSG_WriteAngle32( &sv.multicast, ent->v.angles[0] );
			MSG_WriteAngle32( &sv.multicast, ent->v.angles[1] );
			MSG_WriteAngle32( &sv.multicast, 0 );
			MSG_DirectSend( MSG_ONE, vec3_origin, client->edict );
		}

		if( client->modelindex )
		{
			 // apply custom model if set
			((edict_t *)ent)->v.modelindex = client->modelindex;
		}
	}

	svgame.dllFuncs.pfnUpdateEntityState( &ent->pvServerData->s, (edict_t *)ent, baseline );

	if( client )
	{
		client->edict->v.fixangle = false;
	}
	// always keep an actual
	ent->pvServerData->s.number = ent->serialnumber;
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
			MSG_WriteDeltaEntity( oldent, newent, msg, false, ( newent->number <= sv_maxclients->integer ));
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

static void SV_AddEntitiesToPacket( edict_t *pViewEnt, edict_t *pClient, client_frame_t *frame, sv_ents_t *ents, bool portal )
{
	edict_t		*ent;
	vec3_t		origin;
	byte		*pset;
	int		leafnum, fullvis = false;
	int		e, clientcluster, clientarea;
	bool		force = false;

	// during an error shutdown message we may need to transmit
	// the shutdown message after the server has shutdown, so
	// specfically check for it
	if( !sv.state ) return;

	e = svgame.dllFuncs.pfnSetupVisibility( pViewEnt, pClient, portal, origin );
	if( e == 0 ) return; // invalid viewent ?
	if( e == -1 ) fullvis = true;

	clientpvs = CM_FatPVS( origin, portal );
	leafnum = CM_PointLeafnum( origin );
	clientarea = CM_LeafArea( leafnum );
	clientcluster = CM_LeafCluster( leafnum );
	clientphs = CM_FatPHS( clientcluster, portal );

	// calculate the visible areas
	if( fullvis )
	{
		if( !portal ) Mem_Set( frame->areabits, 0xFF, sizeof( frame->areabits ));
		frame->areabits_size = sizeof( frame->areabits );
	}
	else frame->areabits_size = CM_WriteAreaBits( frame->areabits, clientarea, portal );

	for( e = 1; e < svgame.globals->numEntities; e++ )
	{
		ent = EDICT_NUM( e );
		if( ent->free ) continue;

		if( ent->serialnumber != e )
		{
			// this should never happens
			MsgDev( D_NOTE, "fixing ent->serialnumber from %i to %i\n", ent->serialnumber, e );
			ent->serialnumber = e;
		}

		// don't double add an entity through portals (already added)
		if( ent->pvServerData->framenum == sv.net_framenum )
			continue;

		if( fullvis ) force = true;
		else force = false; // clear forceflag

		// NOTE: always add himslef to list
		if( !portal && ( ent == pClient ))
			force = true;

		if( ent->v.flags & FL_PHS_FILTER )
			pset = clientphs;
		else pset = clientpvs;

		if( !force )
		{
			// run custom user filter
			if( !svgame.dllFuncs.pfnAddToFullPack( pViewEnt, pClient, ent, sv.hostflags, clientarea, pset ))
				continue;
		}

		SV_AddEntToSnapshot( ent->pvServerData, ent, ents ); // add it
		if( fullvis ) continue; // portal ents will be added anywhere, ignore recursion

		// if its a portal entity, add everything visible from its camera position
		if( !portal && (ent->pvServerData->s.ed_type == ED_PORTAL || ent->pvServerData->s.ed_type == ED_SKYPORTAL))
			SV_AddEntitiesToPacket( ent, NULL, frame, ents, true );
	}
}

static void SV_EmitEvents( sv_client_t *cl, client_frame_t *frame, sizebuf_t *msg )
{
	int		i, ev;
	event_state_t	*es;
	event_info_t	*info;
	int		ev_count = 0;
	int		c;

	es = &cl->events;

	// count events
	for( ev = 0; ev < MAX_EVENT_QUEUE; ev++ )
	{
		info = &es->ei[ev];
		if( info->index == 0 )
			continue;
		ev_count++;
	}

	// nothing to send
	if( !ev_count ) return;

	if( ev_count >= MAX_EVENT_QUEUE )
		ev_count = MAX_EVENT_QUEUE - 1;

	MSG_WriteByte( msg, svc_event );	// create message
	MSG_WriteByte( msg, ev_count );	// Up to MAX_EVENT_QUEUE events

	for( i = c = 0 ; i < MAX_EVENT_QUEUE; i++ )
	{
		info = &es->ei[i];

		if( info->index == 0 )
			continue;

		// only send if there's room
		if ( c < ev_count )
			SV_PlaybackEvent( msg, info );

		info->index = 0;
		c++;
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
			MsgDev( D_WARN, "%s: ^7delta request from out of date entities.\n", cl->name );
			oldframe = NULL;
			lastframe = 0;
		}
	}

	// refresh physinfo if needs
	if( cl->physinfo_modified )
	{
		cl->physinfo_modified = false;
		MSG_WriteByte( msg, svc_physinfo );
		MSG_WriteString( msg, cl->physinfo );
	}

	// delta encode the events
	SV_EmitEvents( cl, frame, msg );

	MSG_WriteByte( msg, svc_frame );
	MSG_WriteLong( msg, sv.framenum );
	MSG_WriteLong( msg, sv.time );		// send a servertime each frame
	MSG_WriteLong( msg, sv.frametime );
	MSG_WriteLong( msg, lastframe );		// what we are delta'ing from
	MSG_WriteByte( msg, cl->surpressCount );	// rate dropped packets
	cl->surpressCount = 0;

	// send over the areabits
	MSG_WriteByte( msg, frame->areabits_size );	// never more than 255 bytes
	MSG_WriteData( msg, frame->areabits, frame->areabits_size );

	// delta encode the entities
	SV_EmitPacketEntities( oldframe, frame, msg );

	// just send an client index
	// it's safe, because NUM_FOR_EDICT always equal ed->serialnumber,
	// thats shared across network
	MSG_WriteByte( msg, svc_playerinfo );
	MSG_WriteByte( msg, frame->index );
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
	edict_t		*ent;
	edict_t		*clent;
	edict_t		*viewent;	// may be NULL
	client_frame_t	*frame;
	entity_state_t	*state;
	sv_ents_t		frame_ents;
	int		i;

	clent = cl->edict;
	viewent = cl->pViewEntity;
	sv.net_framenum++;

	// this is the frame we are creating
	frame = &cl->frames[sv.framenum & UPDATE_MASK];
	frame->senttime = svs.realtime; // save it for ping calc later

	// clear everything in this snapshot
	frame_ents.num_entities = c_fullsend = 0;
	Mem_Set( frame->areabits, 0, sizeof( frame->areabits ));
	if( !clent->pvServerData->client ) return; // not in game yet

	// grab the current player index
	frame->index = NUM_FOR_EDICT( clent );

	// add all the entities directly visible to the eye, which
	// may include portal entities that merge other viewpoints
	SV_AddEntitiesToPacket( viewent, clent, frame, &frame_ents, false );

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

	// copy the accumulated reliable datagram
	// for this client out to the message
	// it is necessary for this to be after the WriteEntities
	// so that entity references will be current
	if( cl->reliable.overflowed ) MsgDev( D_ERROR, "reliable datagram overflowed for %s\n", cl->name );
	else MSG_WriteData( &msg, cl->reliable.data, cl->reliable.cursize );
	MSG_Clear( &cl->reliable );

	if( msg.overflowed )
	{	
		// must have room left for the packet header
		MsgDev( D_WARN, "msg overflowed for %s\n", cl->name );
		MSG_Clear( &msg );
	}

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

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages( void )
{
	sv_client_t	*cl;
	int		i;

	if( sv.state == ss_dead )
		return;

	// send a message to each connected client
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( !cl->state ) continue;
			
		if( !cl->edict || (cl->edict->v.flags & (FL_FAKECLIENT|FL_SPECTATOR)))
			continue;

		// update any userinfo packets that have changed
		if( cl->sendinfo )
		{
			cl->sendinfo = false;
			SV_FullClientUpdate( cl, &sv.multicast );
		}
                    
		if( cl->sendmovevars )
		{
			cl->sendmovevars = false;
			SV_UpdatePhysinfo( cl, &sv.multicast );
                    }

		// if the reliable message overflowed, drop the client
		if( cl->netchan.message.overflowed )
		{
			MSG_Clear( &cl->netchan.message );
			MSG_Clear( &cl->reliable );
			MSG_Clear( &cl->datagram );
			SV_BroadcastPrintf( PRINT_HIGH, "%s overflowed\n", cl->name );
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