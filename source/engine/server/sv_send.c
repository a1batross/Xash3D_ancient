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
// sv_main.c -- server main program

#include "common.h"
#include "server.h"

#define HEADER_RATE_BYTES		48 // include our header, IP header, and some overhead

/*
=============================================================================

Msg redirection

=============================================================================
*/

char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

void SV_FlushRedirect( int sv_redirected, char *outputbuf )
{
	if (sv_redirected == RD_PACKET)
	{
		Netchan_OutOfBandPrint (NS_SERVER, svs.redirect_address, "print\n%s", outputbuf);
	}
	else if (sv_redirected == RD_CLIENT)
	{
		MSG_WriteByte (&sv_client->netmsg, svc_print);
		MSG_WriteByte (&sv_client->netmsg, PRINT_CONSOLE );
		MSG_WriteString (&sv_client->netmsg, outputbuf);
	}
}

/*
====================
SV_RateMsec

Return the number of msec a given size message is supposed
to take to clear, based on the current rate
====================
*/

static int SV_RateMsec( sv_client_t *client, int msg_size )
{
	int	rate, rate_msec;

	// individual messages will never be larger than fragment size
	if( msg_size > 1500 ) msg_size = 1500;

	rate = client->rate;
	rate_msec = (msg_size + HEADER_RATE_BYTES) * 1000 / rate;

	return rate_msec;
}

/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/


/*
=================
SV_ClientPrintf

Sends text across to be displayed if the level passes
=================
*/
void SV_ClientPrintf (sv_client_t *cl, int level, char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	
	if (level < cl->messagelevel)
		return;
	
	va_start (argptr,fmt);
	com.vsprintf (string, fmt,argptr);
	va_end (argptr);
	
	MSG_WriteByte (&cl->netmsg, svc_print);
	MSG_WriteByte (&cl->netmsg, level);
	MSG_WriteString (&cl->netmsg, string);
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf (int level, char *fmt, ...)
{
	va_list		argptr;
	char		string[2048];
	sv_client_t	*cl;
	int			i;

	va_start (argptr,fmt);
	com.vsprintf (string, fmt,argptr);
	va_end (argptr);
	
	// echo to console
	if( host.type == HOST_DEDICATED )
	{
		char	echo[1024];
		int	i;
		
		// mask off high bits
		for (i = 0; i < 1023 && string[i]; i++)
			echo[i] = string[i] & 127;
		echo[i] = 0;
		Msg ("%s", echo );
	}

	for (i = 0, cl = svs.clients; i < maxclients->value; i++, cl++)
	{
		if (level < cl->messagelevel) continue;
		if (cl->state != cs_spawned) continue;

		MSG_WriteByte (&cl->netmsg, svc_print);
		MSG_WriteByte (&cl->netmsg, level);
		MSG_WriteString (&cl->netmsg, string);
	}
}

/*
=================
SV_BroadcastCommand

Sends text to all active clients
=================
*/
void SV_BroadcastCommand (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	
	if (!sv.state) return;
	va_start (argptr,fmt);
	com.vsprintf (string, fmt,argptr);
	va_end (argptr);

	MSG_Begin(svc_stufftext);
	MSG_WriteString (&sv.multicast, string);
	MSG_Send(MSG_ALL_R, NULL, NULL);
}


/*
=================
SV_Send

Sends the contents of sv.multicast to a subset of the clients,
then clears sv.multicast.

MULTICAST_ONE	send to one client (ent can't be NULL)
MULTICAST_ALL	same as broadcast (origin can be NULL)
MULTICAST_PVS	send to clients potentially visible from org
MULTICAST_PHS	send to clients potentially hearable from org
=================
*/
void _MSG_Send (msgtype_t to, vec3_t origin, edict_t *ent, const char *filename, int fileline)
{
	byte		*mask = NULL;
	int		leafnum = 0, cluster = 0;
	int		area1 = 0, area2 = 0;
	int		j, numclients = maxclients->value;
	sv_client_t	*client, *current = svs.clients;
	bool		reliable = false;

	/*if(origin == NULL || ent == NULL) 
	{
		if(to == MSG_ONE || to == MSG_ONE_R)
			Msg("MSG_Send: ent == NULL (called at %s:%i)\n", filename, fileline);
		else Msg("MSG_Send: origin == NULL (called at %s:%i)\n", filename, fileline);
		return;
	}*/
	
	switch (to)
	{
	case MSG_ALL_R:
		reliable = true;	// intentional fallthrough
	case MSG_ALL:
		// nothing to sort	
		break;
	case MSG_PHS_R:
		reliable = true;	// intentional fallthrough
	case MSG_PHS:
		if(origin == NULL) return;
		leafnum = pe->PointLeafnum (origin);
		cluster = pe->LeafCluster (leafnum);
		mask = pe->ClusterPHS (cluster);
		area1 = pe->LeafArea (leafnum);
		break;
	case MSG_PVS_R:
		reliable = true;	// intentional fallthrough
	case MSG_PVS:
		if(origin == NULL) return;
		leafnum = pe->PointLeafnum (origin);
		cluster = pe->LeafCluster (leafnum);
		mask = pe->ClusterPVS (cluster);
		area1 = pe->LeafArea (leafnum);
		break;
	case MSG_ONE_R:
		reliable = true;	// intentional fallthrough
	case MSG_ONE:
		if(ent == NULL) return;
		j = PRVM_NUM_FOR_EDICT(ent);
		if (j < 1 || j > numclients) return;
		current = svs.clients + (j - 1);
		numclients = 1; // send to one
		break;
	default:
		MsgDev( D_ERROR, "MSG_Send: bad destination: %i (called at %s:%i)\n", to, filename, fileline);
		return;
	}

	// send the data to all relevent clients (or once only)
	for (j = 0, client = current; j < numclients; j++, client++)
	{
		if (client->state == cs_free || client->state == cs_zombie) continue;
		if (client->state != cs_spawned && !reliable) continue;

		if (mask)
		{
			area2 = pe->LeafArea (leafnum);
			cluster = pe->LeafCluster (leafnum);
			leafnum = pe->PointLeafnum (client->edict->progs.sv->origin);
			if (!pe->AreasConnected (area1, area2)) continue;
			if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7))))) continue;
		}

		if (reliable) _SZ_Write (&client->netmsg, sv.multicast.data, sv.multicast.cursize, filename, fileline);
		else _SZ_Write (&client->netmsg, sv.multicast.data, sv.multicast.cursize, filename, fileline);
	}
	SZ_Clear (&sv.multicast);
}

/*
=================
MSG_Begin

Misc helper function
=================
*/

void _MSG_Begin( int dest, const char *filename, int fileline )
{
	_MSG_WriteByte( &sv.multicast, dest, filename, fileline );
}

/*  
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

If cahnnel & 8, the sound will be sent to everyone, not just
things in the PHS.

FIXME: if entity isn't in PHS, they must be forced to be sent or
have the origin explicitly sent.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

Timeofs can range from 0.0 to 0.1 to cause sounds to be started
later in the frame than they normally would.

If origin is NULL, the origin is determined from the entity origin
or the midpoint of the entity box for bmodels.
==================
*/  
void SV_StartSound (vec3_t origin, edict_t *entity, int channel, int soundindex, float volume, float attenuation, float timeofs)
{       
	int		i, ent, flags, sendchan;
	vec3_t		origin_v;
	bool		use_phs;

	if (volume < 0 || volume > 1.0)
	{
		MsgDev( D_WARN, "SV_StartSound: volume = %f\n", volume);
		volume = bound(0, volume, 1.0);
	}
	if (attenuation < 0 || attenuation > 4)
	{
		MsgDev( D_WARN, "SV_StartSound: attenuation = %f\n", attenuation);
		attenuation = bound(0, volume, 4);
	}
	if (timeofs < 0 || timeofs > 0.255)
	{
		MsgDev( D_WARN, "SV_StartSound: timeofs = %f\n", timeofs);
		timeofs = bound(0, timeofs, 0.255 );
	}
	ent = PRVM_NUM_FOR_EDICT(entity);

	if (channel & CHAN_NO_PHS_ADD) // no PHS flag
	{
		use_phs = false;
		channel &= 7;
	}
	else use_phs = true;
	sendchan = (ent<<3) | (channel&7);

	flags = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOL) flags |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTN) flags |= SND_ATTENUATION;

	// the client doesn't know that bmodels have weird origins
	// the origin can also be explicitly set
	if (entity->progs.sv->solid == SOLID_BSP || !VectorIsNull(origin))
		flags |= SND_POS;

	// always send the entity number for channel overrides
	flags |= SND_ENT;

	if (timeofs) flags |= SND_OFFSET;

	// use the entity origin unless it is a bmodel or explicitly specified
	if (!origin)
	{
		origin = origin_v;
		if (entity->progs.sv->solid == SOLID_BSP)
		{
			for (i = 0; i < 3; i++)
			{
				origin_v[i] = entity->progs.sv->origin[i]+0.5*(entity->progs.sv->mins[i]+entity->progs.sv->maxs[i]);
			}
		}
		else
		{
			VectorCopy (entity->progs.sv->origin, origin_v);
		}
	}

	MSG_Begin(svc_sound);
	MSG_WriteByte (&sv.multicast, flags);
	MSG_WriteByte (&sv.multicast, soundindex);

	if (flags & SND_VOLUME) MSG_WriteByte(&sv.multicast, volume*255);
	if (flags & SND_ATTENUATION) MSG_WriteByte(&sv.multicast, attenuation*64);
	if (flags & SND_OFFSET) MSG_WriteByte(&sv.multicast, timeofs*1000);
	if (flags & SND_ENT) MSG_WriteShort(&sv.multicast, sendchan);
	if (flags & SND_POS) MSG_WritePos32(&sv.multicast, origin);

	if (channel & CHAN_RELIABLE)
	{
		if(use_phs) 
		{
			MSG_Send (MSG_PHS_R, origin, NULL);
		}
		else
		{
			MSG_Send(MSG_ALL_R, origin, NULL );
		}
	}
	else
	{
		if(use_phs)
		{
			MSG_Send (MSG_PHS, origin, NULL);
		}
		else
		{
			MSG_Send (MSG_ALL, origin, NULL);
		}
	}
}           

void SV_AmbientSound( edict_t *entity, int soundindex, float volume, float attenuation )
{
	vec3_t		origin_v;
	int		i;

	if(entity->progs.sv->solid == SOLID_BSP)
	{
		for(i = 0; i < 3; i++)
		{
			origin_v[i] = entity->progs.sv->origin[i]+0.5*(entity->progs.sv->mins[i]+entity->progs.sv->maxs[i]);
		}
	}
	else
	{
		VectorCopy( entity->progs.sv->origin, origin_v );
	}

	if( sv.state != ss_loading )
	{
		MSG_Begin( svc_ambientsound );
			MSG_WriteWord(&sv.multicast, entity->priv.sv->serialnumber);
			MSG_WriteWord(&sv.multicast, soundindex);
			MSG_WritePos32(&sv.multicast, origin_v );
		MSG_Send (MSG_ALL_R, origin_v, NULL);
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
void SV_SendClientDatagram (sv_client_t *client )
{
	int		rate_msec;

	SV_BuildClientFrame( client );

	// send over all the relevant entity_state_t
	// and the player_state_t
	SV_WriteFrameToClient( client, &client->netmsg );

	if( client->netmsg.overflowed )
	{	
		// must have room left for the packet header
		MsgDev( D_WARN, "msg overflowed for %s\n", client->name);
		SZ_Clear( &client->netmsg );
	}

	// record information about the message
	client->frames[client->netchan.outgoing_sequence & PACKET_MASK].msg_size = client->netmsg.cursize;
	client->frames[client->netchan.outgoing_sequence & PACKET_MASK].msg_sent = svs.realtime;
	client->frames[client->netchan.outgoing_sequence & PACKET_MASK].msg_acked = -1;

	// send the datagram
	Netchan_Transmit( &client->netchan, client->netmsg.cursize, client->netmsg.data );
		
	if( NET_IsLocalAddress( client->netchan.remote_address ) || NET_IsLANAddress( client->netchan.remote_address ))
	{
		client->nextmsgtime = svs.realtime - 1;
		return;
	}

	// normal rate / snapshotMsec calculation
	rate_msec = SV_RateMsec( client, client->netmsg.cursize );
	client->nextmsgtime = svs.realtime + rate_msec;

	// don't pile up empty snapshots while connecting
	if( client->state != cs_spawned )
	{
		if( client->nextmsgtime < svs.realtime + 1000 )
			client->nextmsgtime = svs.realtime + 1000;
	}
	SZ_Clear( &client->netmsg );
}

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages( void )
{
	int		i;
	sv_client_t	*cl;

	// send a message to each connected client
	for( i = 0, cl = svs.clients; i < maxclients->integer; i++, cl++ )
	{
		if( !cl->state ) continue;
		if( svs.realtime < cl->nextmsgtime ) continue; // not time yet

		if( cl->state != cs_spawned )
		{
			if( cl->netmsg.cursize || svs.realtime - cl->lastmessage > 1000 )
			{
				Msg("SV_WritePacket: %s\n", cl->netmsg.data );
				Netchan_Transmit( &cl->netchan, cl->netmsg.cursize, cl->netmsg.data );
				SZ_Clear( &cl->netmsg );
				continue;
			}
		}

		if( cl->netchan.unsent_fragments )
		{
			// send additional message fragments if the last message was too large to send at once
			int packet_size = cl->netchan.unsent_length - cl->netchan.unsent_fragment_start;
			cl->nextmsgtime = svs.realtime + SV_RateMsec( cl, packet_size );
			Netchan_TransmitNextFragment( &cl->netchan );
			continue;
		}

		// generate and send a new message
		SV_SendClientDatagram( cl );
	}
}