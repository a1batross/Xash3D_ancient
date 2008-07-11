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

if the sequence number is -1, the packet should be handled as an out-of-band
message instead of as part of a netcon.

All fragments will have the same sequence numbers.

The qport field is a workaround for bad address translating routers that
sometimes remap the client's source port on a packet during gameplay.

If the base part of the net address matches and the qport matches, then the
channel matches even if the IP port differs.  The IP port should be updated
to the new value before sending out any replies.
*/

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
	chan->last_received = Sys_Milliseconds();

	SZ_Init( &chan->message, chan->message_buf, sizeof(chan->message_buf));
}

/*
===============
Netchan_CanReliable

Returns true if the last reliable message has acked
================
*/
bool Netchan_CanReliable( netchan_t *chan )
{
	if( chan->reliable_length )
		return false; // waiting for ack
	return true;
}


bool Netchan_NeedReliable( netchan_t *chan )
{
	bool	send_reliable;

	// if the remote side dropped the last reliable message, resend it
	send_reliable = false;

	if (chan->incoming_acknowledged > chan->last_reliable_sequence
	&& chan->incoming_reliable_acknowledged != chan->reliable_sequence)
		send_reliable = true;

	// if the reliable transmit buffer is empty, copy the current message out
	if( !chan->reliable_length && chan->message.cursize )
	{
		send_reliable = true;
	}

	return send_reliable;
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
	byte	send_buf[MAX_MSGLEN];
	bool	send_reliable;
	uint	w1, w2;

	// check for message overflow
	if (chan->message.overflowed)
	{
		Host_Error( "%s:Outgoing message overflow\n", NET_AdrToString (chan->remote_address));
		return;
	}

	send_reliable = Netchan_NeedReliable (chan);

	if (!chan->reliable_length && chan->message.cursize)
	{
		memcpy (chan->reliable_buf, chan->message_buf, chan->message.cursize);
		chan->reliable_length = chan->message.cursize;
		chan->message.cursize = 0;
		chan->reliable_sequence ^= 1;
	}


	// write the packet header
	SZ_Init (&send, send_buf, sizeof(send_buf));

	w1 = ( chan->outgoing_sequence & ~(1<<31) ) | (send_reliable<<31);
	w2 = ( chan->incoming_sequence & ~(1<<31) ) | (chan->incoming_reliable_sequence<<31);

	chan->outgoing_sequence++;
	chan->last_sent = Sys_Milliseconds();

	MSG_WriteLong (&send, w1);
	MSG_WriteLong (&send, w2);

	// send the qport if we are a client
	if (chan->sock == NS_CLIENT) MSG_WriteWord (&send, qport->value);

	// copy the reliable message to the packet first
	if (send_reliable)
	{
		SZ_Write (&send, chan->reliable_buf, chan->reliable_length);
		chan->last_reliable_sequence = chan->outgoing_sequence;
	}
	
	// add the unreliable part if space is available
	if (send.maxsize - send.cursize >= length) 
		SZ_Write (&send, data, length);
	else MsgDev( D_WARN, "Netchan_Transmit: dumped unreliable\n");

	// send the datagram
	NET_SendPacket (chan->sock, send.cursize, send.data, chan->remote_address);

	if (showpackets->value)
	{
		if (send_reliable)
			Msg ("send %4i : s=%i reliable=%i ack=%i rack=%i\n"
				, send.cursize
				, chan->outgoing_sequence - 1
				, chan->reliable_sequence
				, chan->incoming_sequence
				, chan->incoming_reliable_sequence);
		else
			Msg ("send %4i : s=%i ack=%i rack=%i\n"
				, send.cursize
				, chan->outgoing_sequence - 1
				, chan->incoming_sequence
				, chan->incoming_reliable_sequence);
	}
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
	uint	sequence, sequence_ack;
	uint	reliable_ack, reliable_message;
	int	qport;

	// get sequence numbers		
	MSG_BeginReading (msg);
	sequence = MSG_ReadLong (msg);
	sequence_ack = MSG_ReadLong (msg);

	// read the qport if we are a server
	if (chan->sock == NS_SERVER)
		qport = MSG_ReadShort (msg);

	reliable_message = sequence >> 31;
	reliable_ack = sequence_ack >> 31;

	sequence &= ~(1<<31);
	sequence_ack &= ~(1<<31);	

	if (showpackets->value)
	{
		if (reliable_message)
			Msg ("recv %4i : s=%i reliable=%i ack=%i rack=%i\n"
				, msg->cursize
				, sequence
				, chan->incoming_reliable_sequence ^ 1
				, sequence_ack
				, reliable_ack);
		else
			Msg ("recv %4i : s=%i ack=%i rack=%i\n"
				, msg->cursize
				, sequence
				, sequence_ack
				, reliable_ack);
	}

	// discard stale or duplicated packets
	if (sequence <= chan->incoming_sequence)
	{
		if (showdrop->value)
			Msg ("%s:Out of order packet %i at %i\n"
				, NET_AdrToString (chan->remote_address)
				,  sequence
				, chan->incoming_sequence);
		return false;
	}

	// dropped packets don't keep the message from being used
	chan->dropped = sequence - (chan->incoming_sequence+1);
	if (chan->dropped > 0)
	{
		if (showdrop->value)
			Msg ("%s:Dropped %i packets at %i\n"
			, NET_AdrToString (chan->remote_address)
			, chan->dropped
			, sequence);
	}

	//
	// if the current outgoing reliable message has been acknowledged
	// clear the buffer to make way for the next
	//
	if (reliable_ack == chan->reliable_sequence)
		chan->reliable_length = 0;	// it has been received
	
	// if this message contains a reliable message, bump incoming_reliable_sequence 
	chan->incoming_sequence = sequence;
	chan->incoming_acknowledged = sequence_ack;
	chan->incoming_reliable_acknowledged = reliable_ack;
	if (reliable_message)
	{
		chan->incoming_reliable_sequence ^= 1;
	}

	// the message can now be read from the current message pointer
	chan->last_received = Sys_Milliseconds();
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
	MSG_WriteLong( &send, -1 );	// -1 sequence means out of band
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

		NETCHAN TRANSMIT\RECEIVED UTILS

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

bool NET_GetPacket( netsrc_t sock, netadr_t *from, sizebuf_t *msg )
{
	if( NET_GetLoopPacket( sock, from, msg ))
		return true;

	return Sys_RecvPacket( from, msg );	
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