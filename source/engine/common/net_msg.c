//=======================================================================
//			Copyright XashXT Group 2007 ©
//			net_msg.c - network messages
//=======================================================================

#include "common.h"
#include "mathlib.h"
#include "builtin.h"

// if (int)f == f and (int)f + (1<<(FLOAT_INT_BITS-1) ) < (1<<FLOAT_INT_BITS)
// the float will be sent with FLOAT_INT_BITS, otherwise all 32 bits will be sent
#define FLOAT_INT_BITS	13
#define FLOAT_INT_BIAS	(1<<(FLOAT_INT_BITS-1))

// add a bit to the output file (buffered)
static void sz_putbit( sizebuf_t *msg, char bit )
{
	if((msg->bitpos & 7) == 0) msg->data[(msg->bitpos>>3)] = 0;
	msg->data[(msg->bitpos>>3)] |= bit<<(msg->bitpos & 7);
	msg->bitpos++;
}

// receive one bit from the input file (buffered)
static int sz_getbit( sizebuf_t *msg )
{
	int	t;

	t = (msg->data[(msg->bitpos>>3)] >> (msg->bitpos & 7)) & 0x1;
	msg->bitpos++;

	return t;
}

/*
=======================
   SZ BUFFER
=======================
*/
void MSG_Init( sizebuf_t *buf, byte *data, size_t length )
{
	memset( buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
}

void *SZ_GetSpace( sizebuf_t *msg, int length )
{
	void	*data;
	
	if( msg->cursize + length > msg->maxsize )
	{
		if( length > msg->maxsize )
			Host_Error("SZ_GetSpace: length[%i] > buffer maxsize [%i]\n", length, msg->maxsize );
		MsgDev( D_WARN, "SZ_GetSpace: overflow [cursize %d maxsize %d]\n", msg->cursize + length, msg->maxsize );
		MSG_Clear( msg ); 
		msg->overflowed = true;
	}
	data = msg->data + msg->cursize;
	msg->cursize += length;
	msg->bitpos = msg->cursize<<3;	

	return data;
}

void MSG_Bitstream( sizebuf_t *msg, bool state )
{
}

void MSG_Clear( sizebuf_t *buf )
{
	buf->bitpos = 0;
	buf->cursize = 0;
	buf->overflowed = false;
}

/*
=============================================================================

bit functions
  
=============================================================================
*/
void _MSG_WriteBits( sizebuf_t *msg, int value, int net_type, const char *filename, int fileline )
{
	byte	*buf;
	short	*sp;
	int	*ip;

	// check range first
	if( value < NWDesc[net_type].min_range || value > NWDesc[net_type].max_range )
		MsgDev( D_WARN, "MSG_Write%s: range error %d (called at %s:%i)\n", NWDesc[net_type].name, value, filename, fileline );

	// this isn't an exact overflow check, but close enough
	if( msg->maxsize - msg->cursize < 4 )
	{
		MsgDev( D_ERROR, "MSG_WriteBits: sizebuf overflowed\n" );
		msg->overflowed = true;
		return;
	}

	switch( net_type )
	{
	case NET_CHAR:
	case NET_BYTE:
		buf = SZ_GetSpace( msg, 1 );
		buf[0] = value;
		break;
	case NET_SHORT:
	case NET_WORD:
		buf = SZ_GetSpace( msg, 2 );
		buf[0] = value & 0xff;
		buf[1] = value>>8;
		break;
	case NET_LONG:
	case NET_FLOAT:
		buf = SZ_GetSpace( msg, 4 );
		buf[0] = (value>>0 ) & 0xff;
		buf[1] = (value>>8 ) & 0xff;
		buf[2] = (value>>16) & 0xff;
		buf[3] = (value>>24);
		break;
	case NET_ANGLE:
		sp = (short *)&msg->data[msg->cursize];
		*sp = LittleShort(ANGLE2SHORT( value ));
		msg->cursize += 2;
		msg->bitpos += 16;
		break;		
	case NET_COORD:
		ip = ( int *)&msg->data[msg->cursize];
		*ip = LittleLong(value * SV_COORD_FRAC);
		msg->cursize += 4;
		msg->bitpos += 32;
		break;
	case NET_BIT:
		sz_putbit( msg, (value & 1));
		msg->cursize = msg->bitpos>>3;
		if( msg->bitpos & 7 ) msg->cursize++;
		break;
	default:
		Host_Error( "MSG_WriteBits: bad net.type ( called %s:%i)\n", filename, fileline );			
		break;
	}
}

int _MSG_ReadBits( sizebuf_t *msg, int net_type, const char *filename, int fileline )
{
	int	value = 0;
	short	*sp;
	int	*ip;

	switch( net_type )
	{
	case NET_CHAR:
		value = (signed char)msg->data[msg->readcount];
		msg->readcount += 1;
		msg->bitpos += 8;
		break;
	case NET_BYTE:
		value = (byte)msg->data[msg->readcount];
		msg->readcount += 1;
		msg->bitpos += 8;
		break;
	case NET_WORD:
	case NET_SHORT:
		value = (short)BuffLittleShort(msg->data + msg->readcount);
		msg->readcount += 2;
		msg->bitpos += 16;
		break;
	case NET_LONG:
	case NET_FLOAT:
		value = BuffLittleLong(msg->data + msg->readcount);
		msg->readcount += 4;
		msg->bitpos += 32;
		break;
	case NET_ANGLE:
		sp = (short *)&msg->data[msg->readcount];
		value = LittleShort(SHORT2ANGLE( *sp ));
		msg->readcount += 2;
		msg->bitpos += 16;
		break;		
	case NET_COORD:
		ip = (int *)&msg->data[msg->readcount];
		value = LittleLong(*ip * CL_COORD_FRAC);
		msg->readcount += 4;
		msg->bitpos += 32;
		break;
	case NET_BIT:
		value = sz_getbit( msg );
		msg->readcount = msg->bitpos>>3;
		if( msg->bitpos & 7 ) msg->readcount++;
		break;
	default:
		Host_Error( "MSG_ReadBits: bad net.type ( called %s:%i)\n", filename, fileline );			
		break;
	}

	// end of message
	if( msg->readcount > msg->cursize )
		return -1;

	return value;
}

/*
==============================================================================

			MESSAGE IO FUNCTIONS
	       Handles byte ordering and avoids alignment errors
==============================================================================
*/
/*
=======================
   writing functions
=======================
*/
void _MSG_WriteFloat( sizebuf_t *sb, float f, const char *filename, int fileline )
{
	union { float f; int l; } dat;
	dat.f = f;
	MSG_WriteBits( sb, dat.l, NET_FLOAT );
}

void _MSG_WriteData( sizebuf_t *buf, const void *data, size_t length, const char *filename, int fileline )
{
	int	i;

	for( i = 0; i < length; i++ )
		_MSG_WriteBits( buf, ((byte *)data)[i], NET_BYTE, filename, fileline );
}

void _MSG_WriteString( sizebuf_t *sb, const char *s, const char *filename, int fileline )
{
	if( !s ) _MSG_WriteData( sb, "", 1, filename, fileline );
	else
	{
		int	l, i;
		char	string[MAX_STRING_CHARS];
                    
		l = com.strlen( s ) + 1;		
		if( l >= MAX_STRING_CHARS )
		{
			MsgDev( D_ERROR, "MSG_WriteString: exceeds MAX_STRING_CHARS (called at %s:%i\n", filename, fileline );
			_MSG_WriteData( sb, "", 1, filename, fileline );
			return;
		}
		com.strncpy( string, s, sizeof( string ));

		// get rid of 0xff chars, because old clients don't like them
		for( i = 0; i < l; i++ )
		{
			if(((byte *)string)[i] > 127 )
				string[i] = '.';
		}
		_MSG_WriteData( sb, string, l, filename, fileline );
	}
}

void MSG_Print( sizebuf_t *msg, char *data )
{
	int		len;
	
	len = com.strlen( data ) + 1;
	if( msg->cursize )
	{
		if( msg->data[msg->cursize - 1] ) Mem_Copy ((byte *)SZ_GetSpace(msg, len),data,len); //no trailing 0
		else Mem_Copy ((byte *)SZ_GetSpace( msg, len - 1) - 1, data, len ); // write over trailing 0
	}
	else Mem_Copy((byte *)SZ_GetSpace(msg, len), data, len );
}

void _MSG_WritePos16(sizebuf_t *sb, int pos[3], const char *filename, int fileline)
{
	_MSG_WriteBits(sb, pos[0], NET_COORD, filename, fileline );
	_MSG_WriteBits(sb, pos[1], NET_COORD, filename, fileline );
	_MSG_WriteBits(sb, pos[2], NET_COORD, filename, fileline );
}

void _MSG_WritePos32(sizebuf_t *sb, vec3_t pos, const char *filename, int fileline)
{
	_MSG_WriteBits( sb, pos[0], NET_FLOAT, filename, fileline );
	_MSG_WriteBits( sb, pos[1], NET_FLOAT, filename, fileline );
	_MSG_WriteBits( sb, pos[2], NET_FLOAT, filename, fileline );
}

/*
=======================
   reading functions
=======================
*/
void MSG_BeginReading( sizebuf_t *msg )
{
	msg->readcount = 0;
	msg->errorcount = 0;
	msg->bitpos = 0;
}

void MSG_EndReading( sizebuf_t *msg )
{
	if(!msg->errorcount) return;
	MsgDev( D_ERROR, "MSG_EndReading: received with errors\n");
}

float MSG_ReadFloat( sizebuf_t *msg )
{
	union { float f; int l; } dat;
	dat.l = MSG_ReadBits( msg, NET_FLOAT );
	return dat.f;	
}

char *MSG_ReadString( sizebuf_t *msg )
{
	static char	string[MAX_STRING_CHARS];
	int		l = 0, c;
	
	do
	{
		// use MSG_ReadByte so -1 is out of bounds
		c = MSG_ReadByte( msg );
		if( c == -1 || c == '\0' )
			break;

		// translate all fmt spec to avoid crash bugs
		if( c == '%' ) c = '.';
		// don't allow higher ascii values
		if( c > 127 ) c = '.';

		string[l] = c;
		l++;
	} while( l < sizeof(string) - 1 );
	string[l] = 0; // terminator
	
	return string;
}

char *MSG_ReadStringLine( sizebuf_t *msg )
{
	static char	string[MAX_STRING_CHARS];
	int		l = 0, c;
	
	do
	{
		// use MSG_ReadByte so -1 is out of bounds
		c = MSG_ReadByte( msg );
		if( c == -1 || c == '\0' || c == '\n' )
			break;

		// translate all fmt spec to avoid crash bugs
		if( c == '%' ) c = '.';

		string[l] = c;
		l++;
	} while( l < sizeof(string) - 1 );
	string[l] = 0; // terminator
	
	return string;
}

void MSG_ReadData( sizebuf_t *msg, void *data, size_t len )
{
	int		i;

	for( i = 0; i < len; i++ )
		((byte *)data)[i] = MSG_ReadByte( msg );
}

void MSG_ReadPos32( sizebuf_t *msg_read, vec3_t pos )
{
	pos[0] = MSG_ReadBits(msg_read, NET_FLOAT );
	pos[1] = MSG_ReadBits(msg_read, NET_FLOAT );
	pos[2] = MSG_ReadBits(msg_read, NET_FLOAT );
}

void MSG_ReadPos16( sizebuf_t *msg_read, int pos[3] )
{
	pos[0] = MSG_ReadBits(msg_read, NET_COORD );
	pos[1] = MSG_ReadBits(msg_read, NET_COORD );
	pos[2] = MSG_ReadBits(msg_read, NET_COORD );
}

/*
=============================================================================

delta functions
  
=============================================================================
*/
/*
=====================
MSG_WriteDeltaUsercmd
=====================
*/
void _MSG_WriteDeltaUsercmd( sizebuf_t *msg, usercmd_t *from, usercmd_t *to, const char *filename, const int fileline )
{
	int		num_fields;
	net_field_t	*field;
	int		*fromF, *toF;
	int		i, flags = 0;
	
	num_fields = sizeof(cmd_fields) / sizeof(cmd_fields[0]);
	MSG_WriteByte( msg, to->msec ); // send msec always

	// integer limit is reached (this should never happen)
	if( num_fields > 31 ) return;

	// compare fields
	for( i = 0, field = cmd_fields; i < num_fields; i++, field++ )
	{
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );
		if( *fromF != *toF ) flags |= 1<<i;
	}
	if( flags == 0 )
	{
		// nothing at all changed
		MSG_WriteLong( msg, INVALID_DELTA ); // no delta info
		return;
	}		

	MSG_WriteLong( msg, flags );	// send flags who indicates changes
	for( i = 0, field = cmd_fields; i < num_fields; i++, field++ )
	{
		toF = (int *)((byte *)to + field->offset );
		if( flags & 1<<i ) MSG_WriteBits( msg, *toF, field->bits );
	}
}

/*
=====================
MSG_ReadDeltaUsercmd
=====================
*/
void MSG_ReadDeltaUsercmd( sizebuf_t *msg, usercmd_t *from, usercmd_t *to )
{
	net_field_t	*field;
	int		num_fields;
	int		msec, i, flags;
	int		*cmd, *fromF, *toF;

	num_fields = sizeof(cmd_fields) / sizeof(cmd_fields[0]);
	msec = MSG_ReadByte( msg ); // always catch time
	*to = *from;

	cmd = (int *)&msg->data[msg->readcount]; // check for special cmds
	if( *cmd == INVALID_DELTA )
	{
		MSG_ReadLong( msg );
		to->msec = msec;
		return;
	}

	to->msec = msec;
	flags = MSG_ReadLong( msg );

	for( i = 0, field = cmd_fields; i < num_fields; i++, field++ )
	{
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );

		if( flags & 1<<i )
			*toF = MSG_ReadBits( msg, field->bits );
		else *toF = *fromF;	// no change
	}
}

/*
=============================================================================

entity_state_t communication
  
=============================================================================
*/
/*
==================
MSG_WriteDeltaEntity

Writes part of a packetentities message, including the entity number.
Can delta from either a baseline or a previous packet_entity
If to is NULL, a remove entity update will be sent
If force is not set, then nothing at all will be generated if the entity is
identical, under the assumption that the in-order delta code will catch it.
==================
*/
void _MSG_WriteDeltaEntity( entity_state_t *from, entity_state_t *to, sizebuf_t *msg, bool force, bool newentity, const char *filename, int fileline ) 
{
	net_field_t	*field;
	int		*fromF, *toF;
	int		i, j, flags;
	int		num_fields;
	int		total = 0;
	bool		write_header = false;
	
	num_fields = sizeof(ent_fields) / sizeof(ent_fields[0]);

	if( to == NULL )
	{
		if( from == NULL ) return;
		// a NULL to is a delta remove message
		MSG_WriteBits( msg, from->number, NET_WORD );
		MSG_WriteBits( msg, -99, NET_LONG );
		return;
	}

	if( to->number < 0 || to->number >= MAX_EDICTS )
		Host_Error( "MSG_WriteDeltaEntity: Bad entity number: %i (called at %s:%i)\n", to->number, filename, fileline );

	for( i = 0; i < num_fields; i += 31 )
	{
		flags = 0;
		// compare fields
		for( j = i, field = &ent_fields[i]; j < num_fields; j++, field++ )
		{
			fromF = (int *)((byte *)from + field->offset );
			toF = (int *)((byte *)to + field->offset );
			if( *fromF != *toF || (newentity && field->force)) flags |= 1<<(j & 31);
		}

		// NOTE: entity must have changes from one at first of 32 fields
		// otherwise it will ignore updates
		if( flags == 0 && total == 0 && force == 0 )
			return; // nothing at all changed
		if(!write_header)
		{
			// entity number
			MSG_WriteBits( msg, to->number, NET_WORD );
			write_header = true;
		}
		MSG_WriteLong( msg, flags );	// send flags who indicates changes

		for( j = i, field = &ent_fields[i]; j < num_fields; j++, field++ )
		{
			toF = (int *)((byte *)to + field->offset );
			if( flags & 1<<(j & 31)) MSG_WriteBits( msg, *toF, field->bits );
		}
		total += flags;
	}
}

/*
==================
MSG_ReadDeltaEntity

The entity number has already been read from the message, which
is how the from state is identified.
                             
If the delta removes the entity, entityState_t->number will be set to MAX_EDICTS-1

Can go from either a baseline or a previous packet_entity
==================
*/
void MSG_ReadDeltaEntity( sizebuf_t *msg, entity_state_t *from, entity_state_t *to, int number )
{
	net_field_t	*field;
	int		i, flags;
	int		num_fields;
	int		*cmd, *fromF, *toF;

	if( number < 0 || number >= MAX_EDICTS )
		Host_Error( "MSG_ReadDeltaEntity: bad delta entity number: %i\n", number );

	*to = *from;
	VectorCopy( from->origin, to->old_origin );
	to->number = number;

	num_fields = sizeof(ent_fields) / sizeof(ent_fields[0]);
	cmd = (int *)&msg->data[msg->readcount]; // check for special cmds

	if( *cmd == -99 )
	{
		// check for a remove
		MSG_ReadLong( msg );
		memset( to, 0, sizeof(*to));	
		to->number = -1;
		return;
	}

	to->number = number;
	for( i = 0, field = ent_fields; i < num_fields; i++, field++ )
	{
		// get flags of next packet if LONG out of range
		if(( i & 31 ) == 0 ) flags = MSG_ReadLong( msg );
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );
		
		if( flags & 1<<(i & 31))
			*toF = MSG_ReadBits( msg, field->bits );
		else *toF = *fromF;	// no change
	}
}

/*
============================================================================

player_state_t communication

============================================================================
*/
/*
=============
MSG_WriteDeltaPlayerstate

=============
*/
void MSG_WriteDeltaPlayerstate( player_state_t *from, player_state_t *to, sizebuf_t *msg )
{
	player_state_t	dummy;
	int		statsbits;
	int		num_fields;
	int		i, c;
	net_field_t	*field;
	int		*fromF, *toF;
	float		fullFloat;
	int		trunc, lc = 0;

	if( !from )
	{
		from = &dummy;
		memset( &dummy, 0, sizeof(dummy));
	}

	c = msg->cursize;
	num_fields = sizeof(ps_fields) / sizeof(ps_fields[0]);

	// all fields should be 32 bits to avoid any compiler packing issues
	// the "number" field is not part of the field list
	// if this assert fails, someone added a field to the entity_state_t
	// struct without updating the message fields
	if( num_fields + 1 != sizeof(*from ) / 4 ) Sys_Error( "ps_fields s&3\n" ); 

	for( i = 0, field = ps_fields; i < num_fields; i++, field++ )
	{
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );
		if( *fromF != *toF ) lc = i + 1;
	}

	MSG_WriteByte( msg, lc );			// # of changes

	for( i = 0, field = ps_fields; i < lc; i++, field++ )
	{
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );

		if( *fromF == *toF )
		{
			MSG_WriteBits( msg, 0, 1 );	// no change
			continue;
		}

		MSG_WriteBits( msg, 1, 1 );		// changed

		if( field->bits == 0 )
		{
			// float
			fullFloat = *(float *)toF;
			trunc = (int)fullFloat;

			if( trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 && trunc + FLOAT_INT_BIAS < (1<<FLOAT_INT_BITS))
			{
				// send as small integer
				MSG_WriteBits( msg, 0, 1 );
				MSG_WriteBits( msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS );
			}
			else
			{
				// send as full floating point value
				MSG_WriteBits( msg, 1, 1 );
				MSG_WriteBits( msg, *toF, 32 );
			}
		}
		else
		{
			// integer
			MSG_WriteBits( msg, *toF, field->bits );
		}
	}
	c = msg->cursize - c;


	// send the arrays
	statsbits = 0;
	for( i = 0; i < MAX_STATS; i++ )
	{
		if( to->stats[i] != from->stats[i] )
			statsbits |= 1<<i;
	}
	if( !statsbits )
	{
		MSG_WriteBits( msg, 0, 1 );	// no change
		return;
	}
	MSG_WriteBits( msg, 1, 1 );		// changed

	if( statsbits )
	{
		MSG_WriteBits( msg, 1, 1 );	// changed
		MSG_WriteShort( msg, statsbits );
		for( i = 0; i < MAX_STATS; i++ )
		{
			if( statsbits & (1<<i))
				MSG_WriteShort( msg, to->stats[i]);
		}
	}
	else MSG_WriteBits( msg, 0, 1 );	// no change
}


/*
===================
MSG_ReadDeltaPlayerstate
===================
*/
void MSG_ReadDeltaPlayerstate( sizebuf_t *msg, player_state_t *from, player_state_t *to )
{
	int		i, lc;
	int		bits;
	net_field_t	*field;
	int		num_fields;
	int		startBit, endBit;
	int		*fromF, *toF;
	int		print;
	int		trunc;
	player_state_t	dummy;

	if( !from )
	{
		from = &dummy;
		memset( &dummy, 0, sizeof( dummy ));
	}
	*to = *from;

	if( msg->bitpos == 0 ) startBit = msg->readcount * 8 - MAXEDICT_BITS;
	else startBit = ( msg->readcount - 1 ) * 8 + msg->bitpos - MAXEDICT_BITS;

	// shownet 2/3 will interleave with other printed info, -2 will
	// just print the delta records
	if( cl_shownet->integer >= 2 || cl_shownet->integer == -2 )
	{
		print = 1;
		MsgDev( D_INFO, "%3i: playerstate ", msg->readcount );
	}
	else print = 0;

	num_fields = sizeof(ps_fields) / sizeof(ps_fields[0] );
	lc = MSG_ReadByte( msg );

	for( i = 0, field = ps_fields; i < lc; i++, field++ )
	{
		fromF = (int *)( (byte *)from + field->offset );
		toF = (int *)( (byte *)to + field->offset );

		if(!MSG_ReadBits( msg, 1 ))
		{
			// no change
			*toF = *fromF;
		}
		else
		{
			if( field->bits == 0 )
			{
				// float
				if( MSG_ReadBits( msg, 1 ) == 0 )
				{
					// integral float
					trunc = MSG_ReadBits( msg, FLOAT_INT_BITS );
					// bias to allow equal parts positive and negative
					trunc -= FLOAT_INT_BIAS;
					*(float *)toF = trunc; 
					if( print ) MsgDev( D_INFO, "%s:%i ", field->name, trunc );
				}
				else
				{
					// full floating point value
					*toF = MSG_ReadBits( msg, 32 );
					if( print ) MsgDev( D_INFO, "%s:%f ", field->name, *(float *)toF );
				}
			}
			else
			{
				// integer
				*toF = MSG_ReadBits( msg, field->bits );
				if( print ) MsgDev( D_INFO, "%s:%i ", field->name, *toF );
			}
		}
	}
	for( i = lc, field = &ps_fields[lc]; i < num_fields; i++, field++ )
	{
		fromF = (int *)( (byte *)from + field->offset );
		toF = (int *)( (byte *)to + field->offset );
		// no change
		*toF = *fromF;
	}


	// read the arrays
	if( MSG_ReadBits( msg, 1 ))
	{
		// parse stats
		if( MSG_ReadBits( msg, 1 ))
		{
			bits = MSG_ReadShort( msg );
			for( i = 0; i < MAX_STATS; i++ )
			{
				if( bits & (1<<i))
					to->stats[i] = MSG_ReadShort( msg );
			}
		}
	}

	if( print )
	{
		if( msg->bitpos == 0 ) endBit = msg->readcount * 8 - MAXEDICT_BITS;
		else endBit = ( msg->readcount - 1 ) * 8 + msg->bitpos - MAXEDICT_BITS;
		MsgDev( D_INFO, " (%i bits)\n", endBit - startBit );
	}
}