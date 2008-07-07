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
#include "mathlib.h"

/*

packet header (quake2)
-------------
31	sequence
1	does this message contain a reliable payload
31	acknowledge sequence
1	acknowledge receipt of even/odd message
16	qport

packet header (quake3)
-------------
4	outgoing sequence.  high bit will be set if this is a fragmented message
[2	qport (only for client to server)]
[2	fragment start byte]
[2	fragment length. if < FRAGMENT_SIZE, this is the last fragment]

if the sequence number is -1, the packet should be handled as an out-of-band
message instead of as part of a netcon.

All fragments will have the same sequence numbers.

The qport field is a workaround for bad address translating routers that
sometimes remap the client's source port on a packet during gameplay.

If the base part of the net address matches and the qport matches, then the
channel matches even if the IP port differs.  The IP port should be updated
to the new value before sending out any replies.
*/

#define MAX_PACKETLEN		1400		// max size of a network packet
#define FRAGMENT_SIZE		(MAX_PACKETLEN - 100)
#define PACKET_HEADER		10		// two ints and a short
#define MAX_LOOPBACK		16
#define FRAGMENT_BIT		(1<<31)

cvar_t *showpackets;
cvar_t *showdrop;
cvar_t *qport;

static char *netsrcString[2] =
{
"client",
"server"
};

typedef struct
{
	byte	data[MAX_PACKETLEN];
	int	datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t	msgs[MAX_LOOPBACK];
	int	get, send;
} loopback_t;

loopback_t	loopbacks[2];
netadr_t		net_from;
sizebuf_t		net_message;
byte		net_message_buffer[MAX_MSGLEN];

/*
===============
Netchan_Init

===============
*/
void Netchan_Init( void )
{
	// pick a port value that should be nice and random
	int port = RANDOM_LONG( 1, 65535 );

	showpackets = Cvar_Get ("net_showpackets", "0", CVAR_TEMP, "show network packets" );
	showdrop = Cvar_Get ("net_showdrop", "0", CVAR_TEMP, "show packets that are dropped" );
	qport = Cvar_Get ("net_qport", va("%i", port), CVAR_INIT, "current quake netport" );
}

/*
==============
Netchan_Setup

called to open a channel to a remote system
==============
*/
void Netchan_Setup( netsrc_t sock, netchan_t *chan, netadr_t adr, int qport )
{
	memset( chan, 0, sizeof(*chan));
	
	chan->sock = sock;
	chan->remote_address = adr;
	chan->qport = qport;
	chan->incoming_sequence = 0;
	chan->outgoing_sequence = 1;
}

/*
=================
Netchan_TransmitNextFragment

Send one fragment of the current message
=================
*/
void Netchan_TransmitNextFragment( netchan_t *chan )
{
	sizebuf_t		send;
	byte		send_buf[MAX_PACKETLEN];
	int		fragment_length;

	// write the packet header
	SZ_Init( &send, send_buf, sizeof(send_buf));
	MSG_WriteLong( &send, chan->outgoing_sequence | FRAGMENT_BIT );

	// send the qport if we are a client
	if( chan->sock == NS_CLIENT )
	{
		MSG_WriteShort( &send, qport->integer );
	}

	// copy the reliable message to the packet first
	fragment_length = FRAGMENT_SIZE;
	if( chan->unsent_fragment_start + fragment_length > chan->unsent_length )
	{
		fragment_length = chan->unsent_length - chan->unsent_fragment_start;
	}

	MSG_WriteShort( &send, chan->unsent_fragment_start );
	MSG_WriteShort( &send, fragment_length );
	MSG_WriteData( &send, chan->unsent_buffer + chan->unsent_fragment_start, fragment_length );

	// send the datagram
	NET_SendPacket( chan->sock, send.cursize, send.data, chan->remote_address );

	if( showpackets->integer )
		MsgDev( D_INFO, "%s send %4i : s=%i fragment=%i,%i\n", netsrcString[chan->sock], send.cursize, chan->outgoing_sequence, chan->unsent_fragment_start, fragment_length );

	chan->unsent_fragment_start += fragment_length;
	if ( chan->unsent_fragment_start == chan->unsent_length && fragment_length != FRAGMENT_SIZE )
	{
		chan->outgoing_sequence++;
		chan->unsent_fragments = false;
	}
}

/*
===============
Netchan_Transmit

Sends a message to a connection, fragmenting if necessary
A 0 length will still generate a packet.
================
*/
void Netchan_Transmit( netchan_t *chan, int length, const byte *data )
{
	sizebuf_t	send;
	byte	send_buf[MAX_PACKETLEN];

	if( length > MAX_MSGLEN ) Host_Error( "Netchan_Transmit: %i > MAX_MSGLEN\n", length );
	chan->unsent_fragment_start = 0;

	// fragment large reliable messages
	if( length >= FRAGMENT_SIZE )
	{
		chan->unsent_fragments = true;
		chan->unsent_length = length;
		Mem_Copy( chan->unsent_buffer, data, length );
		// only send the first fragment now
		Netchan_TransmitNextFragment( chan );
		return;
	}

	// write the packet header
	SZ_Init( &send, send_buf, sizeof(send_buf));
	MSG_WriteLong( &send, chan->outgoing_sequence );
	chan->outgoing_sequence++;

	// send the qport if we are a client
	if( chan->sock == NS_CLIENT )
		MSG_WriteShort( &send, qport->integer );

	MSG_WriteData( &send, data, length );
	NET_SendPacket( chan->sock, send.cursize, send.data, chan->remote_address ); // send the datagram

	if( showpackets->integer )
		MsgDev( D_INFO, "%s send %4i : s=%i ack=%i\n", netsrcString[chan->sock], send.cursize, chan->outgoing_sequence - 1, chan->incoming_sequence );
}

/*
=================
Netchan_Process

Returns qfalse if the message should not be processed due to being
out of order or a fragment.

Msg must be large enough to hold MAX_MSGLEN, because if this is the
final fragment of a multi-part message, the entire thing will be
copied out.
=================
*/
bool Netchan_Process( netchan_t *chan, sizebuf_t *msg )
{
	int	sequence;
	int	qport;
	int	fragment_start = 0, fragment_length = 0;
	bool	fragmented = false;

	// get sequence numbers		
	MSG_BeginReading( msg );
	sequence = MSG_ReadLong( msg );

	// check for fragment information
	if( sequence & FRAGMENT_BIT )
	{
		sequence &= ~FRAGMENT_BIT;
		fragmented = true;
	}

	// read the qport if we are a server
	if( chan->sock == NS_SERVER )
		qport = MSG_ReadShort( msg );

	// read the fragment information
	if( fragmented )
	{
		fragment_start = MSG_ReadShort( msg );
		fragment_length = MSG_ReadShort( msg );
	}

	if( showpackets->integer )
	{
		if( fragmented )
			MsgDev( D_INFO, "%s recv %4i : s=%i fragment=%i,%i\n", netsrcString[chan->sock], msg->cursize, sequence, fragment_start, fragment_length );
		else MsgDev( D_INFO, "%s recv %4i : s=%i\n", netsrcString[chan->sock], msg->cursize, sequence );
	}

	// discard out of order or duplicated packets
	if( sequence <= chan->incoming_sequence )
	{
		if( showdrop->integer || showpackets->integer )
			MsgDev( D_INFO, "%s:Out of order packet %i at %i\n", NET_AdrToString(chan->remote_address),  sequence, chan->incoming_sequence );
		return false;
	}

	// dropped packets don't keep the message from being used
	chan->dropped = sequence - (chan->incoming_sequence + 1);
	if( chan->dropped > 0 )
	{
		if( showdrop->integer || showpackets->integer )
			MsgDev( D_INFO, "%s:Dropped %i packets at %i\n", NET_AdrToString( chan->remote_address ), chan->dropped, sequence );
	}
	

	// if this is the final framgent of a reliable message, bump incoming_reliable_sequence 
	if( fragmented )
	{
		if( sequence != chan->fragment_sequence )
		{
			chan->fragment_sequence = sequence;
			chan->fragment_length = 0;
		}
		// if we missed a fragment, dump the message
		if( fragment_start != chan->fragment_length )
		{
			if( showdrop->integer || showpackets->integer )
				MsgDev( D_INFO, "%s:Dropped a message fragment\n", NET_AdrToString( chan->remote_address ), sequence);
			return false;
		}

		// copy the fragment to the fragment buffer
		if( fragment_length < 0 || msg->readcount + fragment_length > msg->cursize || chan->fragment_length + fragment_length > sizeof( chan->fragment_buffer ))
		{
			if( showdrop->integer || showpackets->integer )
				MsgDev( D_INFO, "%s:illegal fragment length\n", NET_AdrToString (chan->remote_address ));
			return false;
		}

		Mem_Copy( chan->fragment_buffer + chan->fragment_length, msg->data + msg->readcount, fragment_length );
		chan->fragment_length += fragment_length;

		// if this wasn't the last fragment, don't process anything
		if( fragment_length == FRAGMENT_SIZE )
			return false;

		if( chan->fragment_length > msg->maxsize )
		{
			MsgDev( D_INFO, "%s:fragmentLength %i > msg->maxsize\n", NET_AdrToString (chan->remote_address ), chan->fragment_length );
			return false;
		}

		// copy the full message over the partial fragment
		// make sure the sequence number is still there
		*(int *)msg->data = LittleLong( sequence );

		Mem_Copy( msg->data + 4, chan->fragment_buffer, chan->fragment_length );
		msg->cursize = chan->fragment_length + 4;
		chan->fragment_length = 0;
		msg->readcount = 4;	// past the sequence number
		msg->bit = 32;	// past the sequence number

		// clients were not acking fragmented messages
		chan->incoming_sequence = sequence;
		return true;
	}
	// the message can now be read from the current message pointer
	chan->incoming_sequence = sequence;

	return true;
}

/*
===============
Netchan_OutOfBand

Sends a data message in an out-of-band datagram (only used for "connect")
================
*/
void Netchan_OutOfBand( netsrc_t sock, netadr_t adr, int length, byte *data )
{
	sizebuf_t	send;
	byte	send_buf[MAX_MSGLEN];

	// write the packet header
	SZ_Init( &send, send_buf, sizeof(send_buf));
	MSG_WriteLong( &send, 0xffffffff );	// -1 sequence means out of band
	SZ_Write( &send, data, length );

	// send the datagram
	NET_SendPacket( sock, send.cursize, send.data, adr );
}

/*
===============
Netchan_OutOfBandPrint

Sends a text message in an out-of-band datagram
================
*/
void Netchan_OutOfBandPrint( netsrc_t sock, netadr_t adr, const char *format, ... )
{
	va_list		argptr;
	static char	string[MAX_MSGLEN - 4];
	
	va_start( argptr, format );
	com.vsprintf( string, format, argptr );
	va_end( argptr );

	Netchan_OutOfBand( sock, adr, com.strlen(string), (byte *)string );
}

/*
=============================================================================

		NET MISC HELPER FUNCTIONS

=============================================================================
*/
/*
===================
NET_CompareBaseAdr

Compares without the port
===================
*/
bool NET_CompareBaseAdr( netadr_t a, netadr_t b )
{
	if( a.type != b.type )
		return false;
	if( a.type == NA_LOOPBACK )
		return true;

	if( a.type == NA_IP )
	{
		if(!memcmp(a.ip, b.ip, 4 ))
			return true;
		return false;
	}
	MsgDev( D_ERROR, "NET_CompareBaseAdr: bad address type\n" );
	return false;
}

bool NET_CompareAdr( netadr_t a, netadr_t b )
{
	if( a.type != b.type )
		return false;
	if( a.type == NA_LOOPBACK )
		return true;

	if( a.type == NA_IP )
	{
		if((memcmp(a.ip, b.ip, 4 ) == 0) && a.port == b.port)
			return true;
		return false;
	}
	MsgDev( D_ERROR, "NET_CompareAdr: bad address type\n" );
	return false;
}

bool NET_IsLocalAddress( netadr_t adr )
{
	return adr.type == NA_LOOPBACK;
}

/*
=============================================================================

LOOPBACK BUFFERS FOR LOCAL PLAYER

=============================================================================
*/
bool NET_GetLoopPacket( netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message )
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock];

	if( loop->send - loop->get > MAX_LOOPBACK )
		loop->get = loop->send - MAX_LOOPBACK;

	if( loop->get >= loop->send )
		return false;

	i = loop->get & (MAX_LOOPBACK-1);
	loop->get++;

	Mem_Copy( net_message->data, loop->msgs[i].data, loop->msgs[i].datalen );
	net_message->cursize = loop->msgs[i].datalen;
	memset( net_from, 0, sizeof(*net_from));
	net_from->type = NA_LOOPBACK;
	return true;

}

void NET_SendLoopPacket( netsrc_t sock, int length, const void *data, netadr_t to )
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock^1];

	i = loop->send & (MAX_LOOPBACK-1);
	loop->send++;

	Mem_Copy( loop->msgs[i].data, data, length );
	loop->msgs[i].datalen = length;
}

/*
=============================================================================

		NETCHAN TRANSMIT UTILS

=============================================================================
*/
void NET_SendPacket( netsrc_t sock, int length, const void *data, netadr_t to )
{
	// sequenced packets are shown in netchan, so just show oob
	if( showpackets->integer && *(int *)data == -1 )
		MsgDev( D_INFO, "send packet %4i\n", length);

	if( to.type == NA_LOOPBACK )
	{
		NET_SendLoopPacket( sock, length, data, to );
		return;
	}

	if( to.type == NA_NONE ) return;
	Sys_SendPacket( length, data, to );
}

/*
=============
NET_StringToAdr

Traps "localhost" for loopback, passes everything else to system
=============
*/
bool NET_StringToAdr( const char *s, netadr_t *a )
{
	bool	r;
	char	*port, base[MAX_STRING_CHARS];

	if(!com.strcmp( s, "localhost" ))
	{
		memset( a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return true;
	}

	// look for a port number
	com.strncpy( base, s, sizeof( base ));
	port = com.strstr( base, ":" );
	if( port )
	{
		*port = 0;
		port++;
	}

	r = Sys_StringToAdr( base, a );

	if( !r )
	{
		a->type = NA_NONE;
		return false;
	}

	// inet_addr returns this if out of range
	if( a->ip[0] == 255 && a->ip[1] == 255 && a->ip[2] == 255 && a->ip[3] == 255 )
	{
		a->type = NA_NONE;
		return false;
	}

	if( port ) a->port = BigShort((short)com.atoi( port ));
	else a->port = BigShort( PORT_SERVER );

	return true;
}