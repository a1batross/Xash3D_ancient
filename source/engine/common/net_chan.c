//=======================================================================
//			Copyright XashXT Group 2008 ©
//			net_chan.c - network channel
//=======================================================================

#include "common.h"
#include "mathlib.h"
#include "byteorder.h"

/*
packet header ( size in bits )
-------------
31	sequence
1	does this message contain a reliable payload
31	acknowledge sequence
1	acknowledge receipt of even/odd message
16	qport

The remote connection never knows if it missed a reliable message, the
local side detects that it has been dropped by seeing a sequence acknowledge
higher thatn the last reliable sequence, but without the correct evon/odd
bit for the reliable set.

If the sender notices that a reliable message has been dropped, it will be
retransmitted.  It will not be retransmitted again until a message after
the retransmit has been acknowledged and the reliable still failed to get there.

if the sequence number is -1, the packet should be handled without a netcon

The reliable message can be added to at any time by doing
MSG_Write* (&netchan->message, <data>).

If the message buffer is overflowed, either by a single message, or by
multiple frames worth piling up while the last reliable transmit goes
unacknowledged, the netchan signals a fatal error.

Reliable messages are always placed first in a packet, then the unreliable
message is included if there is sufficient room.

To the receiver, there is no distinction between the reliable and unreliable
parts of the message, they are just processed out as a single larger message.

Illogical packet sequence numbers cause the packet to be dropped, but do
not kill the connection.  This, combined with the tight window of valid
reliable acknowledgement numbers provides protection against malicious
address spoofing.


The qport field is a workaround for bad address translating routers that
sometimes remap the client's source port on a packet during gameplay.

If the base part of the net address matches and the qport matches, then the
channel matches even if the IP port differs.  The IP port should be updated
to the new value before sending out any replies.


If there is no information that needs to be transfered on a given frame,
such as during the connection stage while waiting for the client to load,
then a packet only needs to be delivered if there is something in the
unacknowledged reliable
*/
cvar_t	*net_showpackets;
cvar_t	*net_showdrop;
cvar_t	*net_qport;

netadr_t	net_from;
sizebuf_t	net_message;
byte	net_message_buffer[MAX_MSGLEN];

/*
===============
Netchan_Init
===============
*/
void Netchan_Init( void )
{
	int		port;
	
	// pick a port value that should be nice and random
	port = RANDOM_LONG( 1, 65535 );

	net_showpackets = Cvar_Get ("net_showpackets", "0", CVAR_TEMP, "show network packets" );
	net_showdrop = Cvar_Get ("net_showdrop", "0", CVAR_TEMP, "show packets that are dropped" );
	net_qport = Cvar_Get ("net_qport", va("%i", port), CVAR_INIT, "current quake netport" );
}

/*
==============
Netchan_Setup

called to open a channel to a remote system
==============
*/
void Netchan_Setup( netsrc_t sock, netchan_t *chan, netadr_t adr, int qport )
{
	Mem_Set( chan, 0, sizeof( *chan ));
	
	chan->sock = sock;
	chan->remote_address = adr;
	chan->qport = qport;
	chan->last_received = Sys_Milliseconds();
	chan->incoming_sequence = 0;
	chan->outgoing_sequence = 1;
	chan->compress = true;

	MSG_Init( &chan->message, chan->message_buf, sizeof( chan->message_buf ));
}

/*
===============
Netchan_OutOfBand

Sends an out-of-band datagram
================
*/
void Netchan_OutOfBand( int net_socket, netadr_t adr, int length, byte *data )
{
	sizebuf_t	send;
	byte	send_buf[MAX_MSGLEN];

	// write the packet header
	MSG_Init( &send, send_buf, sizeof( send_buf ));
	
	MSG_WriteLong( &send, -1 );	// -1 sequence means out of band
	MSG_WriteData( &send, data, length );

	// send the datagram
	NET_SendPacket( net_socket, send.cursize, send.data, adr );
}

/*
===============
Netchan_OutOfBandPrint

Sends a text message in an out-of-band datagram
================
*/
void Netchan_OutOfBandPrint( int net_socket, netadr_t adr, char *format, ... )
{
	va_list	argptr;
	char	string[MAX_MSGLEN];

	va_start( argptr, format );
	com.vsprintf( string, format, argptr );
	va_end( argptr );

	Netchan_OutOfBand( net_socket, adr, com.strlen( string ), string );
}

bool Netchan_NeedReliable( netchan_t *chan )
{
	bool	send_reliable = false;

	// if the remote side dropped the last reliable message, resend it
	send_reliable = false;

	if( chan->incoming_acknowledged > chan->last_reliable_sequence && chan->incoming_reliable_acknowledged != chan->reliable_sequence )
		send_reliable = true;

	// if the reliable transmit buffer is empty, copy the current message out
	if( !chan->reliable_length && chan->message.cursize )
		send_reliable = true;

	if( !chan->reliable_length && chan->message.cursize )
	{
		Mem_Copy( chan->reliable_buf, chan->message_buf, chan->message.cursize );
		chan->reliable_length = chan->message.cursize;
		chan->message.cursize = 0;
		chan->reliable_sequence ^= 1;
	}

	return send_reliable;
}

/*
===============
Netchan_Transmit

tries to send an unreliable message to a connection, and handles the
transmition / retransmition of the reliable messages.

A 0 length will still generate a packet and deal with the reliable messages.
================
*/
void Netchan_Transmit( netchan_t *chan, int length, byte *data )
{
	sizebuf_t		send;
	static bool	overflow = false;
	byte		send_buf[MAX_MSGLEN];
	bool		send_reliable;
	size_t		size1, size2;
	uint		w1, w2;

	// check for message overflow
	if( chan->message.overflowed )
	{
		chan->fatal_error = true;
		MsgDev( D_ERROR, "%s:outgoing message overflow\n", NET_AdrToString( chan->remote_address ));
		return;
	}
 
	send_reliable = Netchan_NeedReliable( chan );

	// write the packet header
	MSG_Init( &send, send_buf, sizeof(send_buf));

	w1 = (chan->outgoing_sequence & ~(1<<31)) | (send_reliable<<31);
	w2 = (chan->incoming_sequence & ~(1<<31)) | (chan->incoming_reliable_sequence<<31);

	chan->outgoing_sequence++;
	chan->last_sent = Sys_Milliseconds();

	MSG_WriteLong( &send, w1 );
	MSG_WriteLong( &send, w2 );

	// send the qport if we are a client
	if( chan->sock == NS_CLIENT ) MSG_WriteWord( &send, Cvar_VariableValue( "net_qport" ));

	// copy the reliable message to the packet first
	if( send_reliable )
	{
		MSG_WriteData( &send, chan->reliable_buf, chan->reliable_length );
		chan->last_reliable_sequence = chan->outgoing_sequence;
	}

	// add the unreliable part if space is available
	if( send.maxsize - send.cursize >= length )
	{
		MSG_WriteData( &send, data, length );
		overflow = false;
	}
	else
	{
		// don't flood with multiple messages
		if( !overflow ) MsgDev( D_WARN, "Netchan_Transmit: unreliable msg overflow\n" );
		overflow = true;
	}

	size1 = send.cursize;
	if( chan->compress ) Huff_CompressPacket( &send, (chan->sock == NS_CLIENT) ? 10 : 8 );
	size2 = send.cursize;

	// send the datagram
	NET_SendPacket( chan->sock, send.cursize, send.data, chan->remote_address );

	if( net_showpackets->integer )
	{
		const char *s1, *s2;

		if( chan->sock == NS_CLIENT ) MsgDev( D_INFO, "CL " );
		else if( chan->sock == NS_SERVER ) MsgDev( D_INFO, "SV " );

		s1 = memprint( size1 );	// uncompressed size
		s2 = memprint( size2 );	// compressed size

		MsgDev( D_INFO, "Netchan_Transmit: %s[%s] : %sreliable\n", s1, s2, send_reliable ? "" : "un" );
	}
}

/*
=================
Netchan_Process

called when the current net message is from remote_address
modifies net message so that it points to the packet payload
=================
*/
bool Netchan_Process( netchan_t *chan, sizebuf_t *msg )
{
	uint	sequence, sequence_ack;
	uint	reliable_ack, recv_reliable;
	size_t	size1, size2;
	int	qport;

	// get sequence numbers		
	MSG_BeginReading( msg );
	sequence = MSG_ReadLong( msg );
	sequence_ack = MSG_ReadLong( msg );

	// read the qport if we are a server
	if( chan->sock == NS_SERVER )
		qport = MSG_ReadShort( msg );

	recv_reliable = sequence>>31;
	reliable_ack = sequence_ack>>31;

	sequence &= ~(1<<31);
	sequence_ack &= ~(1<<31);	

	// discard stale or duplicated packets
	if( sequence <= chan->incoming_sequence )
	{
		if( net_showdrop->value )
			MsgDev( D_WARN, "%s:Out of order packet\n", NET_AdrToString( chan->remote_address ));
		return false;
	}

	// dropped packets don't keep the message from being used
	chan->dropped = sequence - (chan->incoming_sequence + 1);
	if( chan->dropped > 0 )
	{
		chan->drop_count += 1;
		if( net_showdrop->value )
			MsgDev( D_WARN, "%s:dropped %i packets\n", NET_AdrToString( chan->remote_address ), chan->dropped );
	}

	// if the current outgoing reliable message has been acknowledged
	// clear the buffer to make way for the next
	if( reliable_ack == chan->reliable_sequence )
		chan->reliable_length = 0; // it has been received
	
	// if this message contains a reliable message, bump incoming_reliable_sequence 
	chan->incoming_sequence = sequence;
	chan->incoming_acknowledged = sequence_ack;
	chan->incoming_reliable_acknowledged = reliable_ack;
	if( recv_reliable ) chan->incoming_reliable_sequence ^= 1;
	size1 = msg->cursize;
	if( chan->compress ) Huff_DecompressPacket( msg, ( chan->sock == NS_SERVER) ? 10 : 8 );
	size2 = msg->cursize;

	if( net_showpackets->integer )
	{
		const char *s1, *s2;

		if( chan->sock == NS_CLIENT ) MsgDev( D_INFO, "CL " );
		else if( chan->sock == NS_SERVER ) MsgDev( D_INFO, "SV " );

		s1 = memprint( size2 );	// compressed size
		s2 = memprint( size1 );	// uncompressed size

		MsgDev( D_INFO, "Netchan_Process: %s[%s] : %sreliable\n", s1, s2, recv_reliable ? "" : "un" );
	}
	chan->good_count += 1;

	// the message can now be read from the current message pointer
	chan->last_received = Sys_Milliseconds();

	return true;
}