//=======================================================================
//			Copyright XashXT Group 2007 �
//			net_msg.c - network messages
//=======================================================================

#include "common.h"
#include "byteorder.h"
#include "mathlib.h"

static net_field_t ent_fields[] =
{
{ ES_FIELD(ed_type),		NET_BYTE,	 true	},
{ ES_FIELD(classname),		NET_WORD,  true	},
{ ES_FIELD(soundindex),		NET_WORD,	 false	},	// 512 sounds ( OpenAL software limit is 255 )
{ ES_FIELD(origin[0]),		NET_FLOAT, false	},
{ ES_FIELD(origin[1]),		NET_FLOAT, false	},
{ ES_FIELD(origin[2]),		NET_FLOAT, false	},
{ ES_FIELD(angles[0]),		NET_FLOAT, false	},
{ ES_FIELD(angles[1]),		NET_FLOAT, false	},
{ ES_FIELD(angles[2]),		NET_FLOAT, false	},
{ ES_FIELD(velocity[0]),		NET_FLOAT, false	},
{ ES_FIELD(velocity[1]),		NET_FLOAT, false	},
{ ES_FIELD(velocity[2]),		NET_FLOAT, false	},
{ ES_FIELD(old_origin[0]),		NET_FLOAT, true	},	// send always
{ ES_FIELD(old_origin[1]),		NET_FLOAT, true	},
{ ES_FIELD(old_origin[2]),		NET_FLOAT, true	},
{ ES_FIELD(infotarget[0]),		NET_FLOAT, false	},	// beam endpoint, portal camera pos, etc
{ ES_FIELD(infotarget[1]),		NET_FLOAT, false	},
{ ES_FIELD(infotarget[2]),		NET_FLOAT, false	},
{ ES_FIELD(model.index),		NET_WORD,	 false	},	// 4096 models
{ ES_FIELD(model.colormap),		NET_WORD,	 false	},	// encoded as two shorts for top and bottom color
{ ES_FIELD(model.scale),		NET_COLOR, false	},	// 0-255 values
{ ES_FIELD(model.frame),		NET_FLOAT, false	},	// interpolate value
{ ES_FIELD(model.animtime),		NET_FLOAT, false	},	// auto-animating time
{ ES_FIELD(model.framerate),		NET_FLOAT, false	},	// custom framerate
{ ES_FIELD(model.sequence),		NET_WORD,	 false	},	// 1024 sequences
{ ES_FIELD(model.gaitsequence),	NET_WORD,	 false	},	// 1024 gaitsequences
{ ES_FIELD(model.skin),		NET_BYTE,	 false	},	// 255 skins
{ ES_FIELD(model.body),		NET_BYTE,	 false	},	// 255 bodies
{ ES_FIELD(pmodel.index),		NET_WORD,  false	},	// 4096 models 
{ ES_FIELD(pmodel.colormap),		NET_LONG,  false	},	// 4096 models 
{ ES_FIELD(pmodel.sequence),		NET_WORD,  false	},	// 1024 sequences
{ ES_FIELD(pmodel.frame),		NET_FLOAT, false	},	// interpolate value
{ ES_FIELD(pmodel.body),		NET_BYTE,  false	},	// 255 bodies
{ ES_FIELD(pmodel.skin),		NET_BYTE,  false	},	// 255 skins
{ ES_FIELD(model.blending[0]),	NET_COLOR, false	},	// animation blending
{ ES_FIELD(model.blending[1]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[2]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[3]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[4]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[5]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[6]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[7]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[8]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[9]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[0]),	NET_COLOR, false	},	// bone controllers #
{ ES_FIELD(model.controller[1]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[2]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[3]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[4]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[5]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[6]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[7]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[8]),	NET_COLOR, false	},
{ ES_FIELD(solidtype),		NET_BYTE,	 false	},
{ ES_FIELD(movetype),		NET_BYTE,	 false	},        // send flags (second 4 bytes)
{ ES_FIELD(gravity),		NET_SHORT, false	},	// gravity multiplier
{ ES_FIELD(aiment),			NET_WORD,	 false	},	// entity index
{ ES_FIELD(solid),			NET_LONG,	 false	},	// encoded mins/maxs
{ ES_FIELD(model.blending[10]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[11]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[12]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[13]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[14]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[15]),	NET_COLOR, false	},
{ ES_FIELD(mins[0]),		NET_FLOAT, false	},
{ ES_FIELD(mins[1]),		NET_FLOAT, false	},
{ ES_FIELD(mins[2]),		NET_FLOAT, false	},
{ ES_FIELD(maxs[0]),		NET_FLOAT, false	},
{ ES_FIELD(maxs[1]),		NET_FLOAT, false	},
{ ES_FIELD(maxs[2]),		NET_FLOAT, false	},	
{ ES_FIELD(effects),		NET_LONG,	 false	},	// effect flags
{ ES_FIELD(renderfx),		NET_LONG,	 false	},	// renderfx flags
{ ES_FIELD(renderamt),		NET_COLOR, false	},	// alpha amount
{ ES_FIELD(rendercolor[0]),		NET_COLOR, false	},	// animation blending
{ ES_FIELD(rendercolor[1]),		NET_COLOR, false	},
{ ES_FIELD(rendercolor[2]),		NET_COLOR, false	},
{ ES_FIELD(rendermode),		NET_BYTE,  false	},	// render mode (legacy stuff)
{ ES_FIELD(model.controller[9]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[10]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[11]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[12]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[13]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[14]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[15]),	NET_COLOR, false	},
{ ES_FIELD(pm_type),		NET_BYTE,  false	},	// 16 player movetypes allowed
{ ES_FIELD(pm_flags),		NET_WORD,  false	},	// 16 movetype flags allowed
{ ES_FIELD(pm_time),		NET_BYTE,  false	},	// each unit 8 msec
{ ES_FIELD(delta_angles[0]),		NET_FLOAT, false	},
{ ES_FIELD(delta_angles[1]),		NET_FLOAT, false	},
{ ES_FIELD(delta_angles[2]),		NET_FLOAT, false	},
{ ES_FIELD(punch_angles[0]),		NET_SCALE, false	},
{ ES_FIELD(punch_angles[1]),		NET_SCALE, false	},
{ ES_FIELD(punch_angles[2]),		NET_SCALE, false	},
{ ES_FIELD(viewangles[0]),		NET_FLOAT, false	},	// for fixed views
{ ES_FIELD(viewangles[1]),		NET_FLOAT, false	},
{ ES_FIELD(viewangles[2]),		NET_FLOAT, false	},
{ ES_FIELD(viewoffset[0]),		NET_SCALE, false	},
{ ES_FIELD(viewoffset[1]),		NET_SCALE, false	},
{ ES_FIELD(viewoffset[2]),		NET_SCALE, false	},
{ ES_FIELD(maxspeed),		NET_WORD,  false	},	// send flags (third 4 bytes )
{ ES_FIELD(fov),			NET_FLOAT, false	},	// client horizontal field of view
{ ES_FIELD(health),			NET_FLOAT, false	},	// client health
{ NULL },							// terminator
};

// probably usercmd_t never reached 32 field integer limit (in theory of course)
static net_field_t cmd_fields[] =
{
{ CM_FIELD(msec),		NET_BYTE,  true	},
{ CM_FIELD(angles[0]),	NET_WORD,  false	},
{ CM_FIELD(angles[1]),	NET_WORD,  false	},
{ CM_FIELD(angles[2]),	NET_WORD,  false	},
{ CM_FIELD(forwardmove),	NET_SHORT, false	},
{ CM_FIELD(sidemove),	NET_SHORT, false	},
{ CM_FIELD(upmove),		NET_SHORT, false	},
{ CM_FIELD(buttons),	NET_BYTE,  false	},
{ CM_FIELD(impulse),	NET_BYTE,  false	},
{ CM_FIELD(lightlevel),	NET_BYTE,  false	},
{ NULL },
};

/*
=============================================================================

SZ BUFFER (io functions)
  
=============================================================================
*/
/*
=======================
MSG_Init

init new buffer
=======================
*/
void MSG_Init( sizebuf_t *buf, byte *data, size_t length )
{
	memset( buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
	Huff_Init();
}

/*
=======================
MSG_GetSpace

get some space for write 
=======================
*/
void *MSG_GetSpace( sizebuf_t *msg, size_t length )
{
	void	*data;
	
	if( msg->cursize + length > msg->maxsize )
	{
		if( length > msg->maxsize )
			Host_Error("MSG_GetSpace: length[%i] > buffer maxsize [%i]\n", length, msg->maxsize );
		MsgDev( D_WARN, "MSG_GetSpace: overflow\n", msg->cursize + length, msg->maxsize );
		MSG_Clear( msg ); 
		msg->overflowed = true;
	}
	data = msg->data + msg->cursize;
	msg->cursize += length;

	return data;
}

/*
=======================
MSG_Print

used for write sv.forward cmds
=======================
*/
void MSG_Print( sizebuf_t *msg, const char *data )
{
	size_t	length = com.strlen(data) + 1;

	if( msg->cursize )
	{
		if(msg->data[msg->cursize - 1]) Mem_Copy((byte *)MSG_GetSpace( msg, length ), data, length );
		else Mem_Copy((byte *)MSG_GetSpace( msg, length - 1) - 1, data, length ); // write over trailing 0
	}
	else Mem_Copy((byte *)MSG_GetSpace( msg, length ), data, length );
}

/*
=======================
MSG_WriteData

used for swap buffers
=======================
*/
void _MSG_WriteData( sizebuf_t *buf, const void *data, size_t length, const char *filename, int fileline )
{
	Mem_Copy( MSG_GetSpace(buf, length), (void *)data, length );	
}

/*
=======================
MSG_Clear

for clearing overflowed buffer
=======================
*/
void MSG_Clear( sizebuf_t *buf )
{
	buf->cursize = 0;
	buf->overflowed = false;
}

void MSG_BeginReading( sizebuf_t *msg )
{
	msg->readcount = 0;
}

/*
=======================
MSG_WriteBits

write # of bytes
=======================
*/
void _MSG_WriteBits( sizebuf_t *msg, int value, int net_type, const char *filename, const int fileline )
{
	union { long l; float f; } dat;
	byte *buf;

	if((NWDesc[net_type].min_range + NWDesc[net_type].max_range) != 0 )
	{
		// check range first
		if( value < NWDesc[net_type].min_range || value > NWDesc[net_type].max_range )
			MsgDev( D_WARN, "MSG_Write%s: range error %i should be in range(%i %i)(called at %s:%i)\n", NWDesc[net_type].name, value, NWDesc[net_type].min_range, NWDesc[net_type].max_range, filename, fileline );
          }
	// this isn't an exact overflow check, but close enough
	if( msg->maxsize - msg->cursize < 4 )
	{
		MsgDev( D_ERROR, "MSG_WriteBits: sizebuf overflowed\n" );
		msg->overflowed = true;
		return;
	}

	switch( net_type )
	{
	case NET_SCALE:
		dat.l = value;
		value = dat.f * 4;	
		buf = MSG_GetSpace( msg, 1 );
		buf[0] = value;
		break;
	case NET_COLOR:
		dat.l = value;
		value = dat.f * 255;
		buf = MSG_GetSpace( msg, 1 );
		buf[0] = value;
		break;
	case NET_CHAR:
	case NET_BYTE:
		buf = MSG_GetSpace( msg, 1 );
		buf[0] = value;
		break;
	case NET_SHORT:
	case NET_WORD:
		buf = MSG_GetSpace( msg, 2 );
		buf[0] = value & 0xff;
		buf[1] = value>>8;
		break;
	case NET_LONG:
	case NET_FLOAT:
		buf = MSG_GetSpace( msg, 4 );
		buf[0] = (value>>0 ) & 0xff;
		buf[1] = (value>>8 ) & 0xff;
		buf[2] = (value>>16) & 0xff;
		buf[3] = (value>>24);
		break;
	case NET_ANGLE:
		value = ANGLE2SHORT( value );
		buf = MSG_GetSpace( msg, 2 );
		buf[0] = value & 0xff;
		buf[1] = value>>8;
		break;		
	default:
		Host_Error( "MSG_WriteBits: bad net.type (called at %s:%i)\n", filename, fileline );			
		break;
	}
}

/*
=======================
MSG_ReadBits

read # of bytes
=======================
*/
long _MSG_ReadBits( sizebuf_t *msg, int net_type, const char *filename, const int fileline )
{
	union { long l; float f; } dat;
	long value = 0;

	switch( net_type )
	{
	case NET_SCALE:
		value = (signed char)(msg->data[msg->readcount]);
		dat.f = value * 0.25f;
		value = dat.l;
		msg->readcount += 1;
		break;
	case NET_COLOR:
		value = (byte)(msg->data[msg->readcount]);
		dat.f = value / 255.0f;
		value = dat.l;
		msg->readcount += 1;
		break;
	case NET_CHAR:
		value = (signed char)msg->data[msg->readcount];
		msg->readcount += 1;
		break;
	case NET_BYTE:
		value = (byte)msg->data[msg->readcount];
		msg->readcount += 1;
		break;
	case NET_WORD:
	case NET_SHORT:
		value = (short)BuffLittleShort(msg->data + msg->readcount);
		msg->readcount += 2;
		break;
	case NET_LONG:
	case NET_FLOAT:
		value = (long)BuffLittleLong(msg->data + msg->readcount);
		msg->readcount += 4;
		break;
	case NET_ANGLE:
		value = (word)BuffLittleShort(msg->data + msg->readcount);
		value = SHORT2ANGLE( value );
		msg->readcount += 2;
		break;		
	default:
		Host_Error( "MSG_ReadBits: bad net.type (called at %s:%i)\n", filename, fileline );			
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

void _MSG_WriteString( sizebuf_t *sb, const char *s, const char *filename, int fileline )
{
	if( !s ) _MSG_WriteData( sb, "", 1, filename, fileline );
	else
	{
		int	l, i;
		char	string[MAX_SYSPATH];
                    
		l = com.strlen( s ) + 1;		
		if( l >= MAX_SYSPATH )
		{
			MsgDev( D_ERROR, "MSG_WriteString: exceeds %i symbols (called at %s:%i\n", MAX_SYSPATH, filename, fileline );
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

void _MSG_WritePos( sizebuf_t *sb, vec3_t pos, const char *filename, int fileline )
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
float MSG_ReadFloat( sizebuf_t *msg )
{
	union { float f; int l; } dat;
	dat.l = MSG_ReadBits( msg, NET_FLOAT );
	return dat.f;	
}

char *MSG_ReadString( sizebuf_t *msg )
{
	static char	string[MAX_SYSPATH];
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
	static char	string[MAX_SYSPATH];
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

void MSG_ReadData( sizebuf_t *msg, void *data, size_t length )
{
	int	i;
	for( i = 0; i < length; i++ )
		((byte *)data)[i] = MSG_ReadByte( msg );
}

void MSG_ReadPos( sizebuf_t *msg_read, vec3_t pos )
{
	pos[0] = MSG_ReadBits(msg_read, NET_FLOAT );
	pos[1] = MSG_ReadBits(msg_read, NET_FLOAT );
	pos[2] = MSG_ReadBits(msg_read, NET_FLOAT );
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
	
	num_fields = (sizeof(cmd_fields) / sizeof(cmd_fields[0])) - 1;
	if( num_fields > 31 ) return; // this should never happen

	// compare fields
	for( i = 0, field = cmd_fields; i < num_fields; i++, field++ )
	{
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );
		if(*fromF != *toF || field->force) flags |= 1<<i;
	}
	if( flags == 0 )
	{
		// nothing at all changed
		MSG_WriteLong( msg, -99 ); // no delta info
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
	int		i, flags;
	int		*fromF, *toF;

	*to = *from;

	if(*(int *)&msg->data[msg->readcount] == -99 )
	{
		MSG_ReadLong( msg );
		return;
	}
	for( i = 0, field = cmd_fields; field->name; i++, field++ )
	{
		// get flags of next packet if LONG out of range
		if((i & 31) == 0) flags = MSG_ReadLong( msg );
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );
		
		if(flags & (1<<i)) *toF = MSG_ReadBits( msg, field->bits );
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
	net_field_t	*field, *field2;
	int		i, j, k, flags;
	int		*fromF, *toF;
	int		num_fields;
	size_t		null_msg_size;
	size_t		buff_size;

	if( to == NULL )
	{
		if( from == NULL ) return;
		// a NULL to is a delta remove message
		MSG_WriteBits( msg, from->number, NET_WORD );
		MSG_WriteBits( msg, -99, NET_LONG );
		return;
	}

	num_fields = sizeof(ent_fields) / sizeof(net_field_t);
	null_msg_size = (( num_fields / 32 ) + (( num_fields % 32 ) ? 1 : 0 )) * 4 + sizeof(short);
	buff_size = msg->cursize;

	if( to->number < 0 || to->number >= host.max_edicts )
		Host_Error( "MSG_WriteDeltaEntity: Bad entity number: %i (called at %s:%i)\n", to->number, filename, fileline );

	MSG_WriteBits( msg, to->number, NET_WORD );
	for( i = j = 0, field = field2 = ent_fields; field->name; i++, j++, field++ )
	{
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );		
		if(*fromF != *toF || (newentity && field->force)) flags |= 1<<j;
		if( j > 31 || !ent_fields[i+1].name) // dump packet
		{
			MSG_WriteLong( msg, flags );	// send flags who indicates changes
			for( k = 0; field2->name; k++, field2++ )
			{
				if( k > 31 ) break; // return to main cycle
				toF = (int *)((byte *)to + field2->offset );
				if( flags & 1<<k ) MSG_WriteBits( msg, *toF, field2->bits );
			}
			j = flags = 0;
		}
	}

	// NOTE: null_msg_size is number of (ent_fields / 32) + (1), 
	// who indicates flags count multiplied by sizeof(long)
	// plus sizeof(short) (head number). If message equal null_message_size
	// we will be ignore it 
	if(!force && (( msg->cursize - buff_size ) == null_msg_size ))
		msg->cursize = buff_size; // kill message
}

/*
==================
MSG_ReadDeltaEntity

The entity number has already been read from the message, which
is how the from state is identified.
                             
If the delta removes the entity, entity_state_t->number will be set to -1

Can go from either a baseline or a previous packet_entity
==================
*/
void MSG_ReadDeltaEntity( sizebuf_t *msg, entity_state_t *from, entity_state_t *to, int number )
{
	net_field_t	*field;
	int		i, flags;
	int		*fromF, *toF;

	if( number < 0 || number >= host.max_edicts )
		Host_Error( "MSG_ReadDeltaEntity: bad delta entity number: %i\n", number );

	*to = *from;
	VectorCopy( from->origin, to->old_origin );
	to->number = number;

	if(*(int *)&msg->data[msg->readcount] == -99 )
	{
		// check for a remove
		MSG_ReadLong( msg );
		memset( to, 0, sizeof(*to));	
		to->number = -1;
		return;
	}
	for( i = 0, field = ent_fields; field->name; i++, field++ )
	{
		// get flags of next packet if LONG out of range
		if((i & 31) == 0) flags = MSG_ReadLong( msg );
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );
		
		if(flags & (1<<i)) *toF = MSG_ReadBits( msg, field->bits );
		else *toF = *fromF;	// no change
	}
}

/*
============================================================================

player state communication

============================================================================
*/
/*
===================
MSG_ParseDeltaPlayer
===================
*/
entity_state_t MSG_ParseDeltaPlayer( entity_state_t *from, entity_state_t *to )
{
	net_field_t	*field;
	int		*fromF, *toF, *outF;
	entity_state_t	dummy, result, *out;
	uint		i;

	// alloc space to copy state
	memset( &result, 0, sizeof( result ));
	out = &result;

	// clear to old value before delta parsing
	if( !from )
	{
		from = &dummy;
		memset( &dummy, 0, sizeof( dummy ));
	}
	*out = *from;
	
	for( i = 0, field = ent_fields; field->name; i++, field++ )
	{
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );
		outF = (int *)((byte *)out + field->offset );
		if( *fromF != *toF ) *outF = *toF;
	}

	return result;
}

/*
============================================================================

player state communication

============================================================================
*/
/*
=============
MSG_WriteDeltaPlayerstate

=============
*/
void MSG_WriteDeltaPlayerstate( entity_state_t *from, entity_state_t *to, sizebuf_t *msg )
{
	entity_state_t	dummy;
	entity_state_t	*ops, *ps = to;
	net_field_t	*field, *field2;
	int		*fromF, *toF;
	int		i, j, k;
	uint		flags = 0;
	
	if( !from )
	{
		memset (&dummy, 0, sizeof(dummy));
		ops = &dummy;
	}
	else ops = from;
	from = to;
	
	MSG_WriteByte( msg, svc_playerinfo );
 
	for( i = j = 0, field = field2 = ent_fields; field->name; i++, j++, field++ )
	{
		fromF = (int *)((byte *)ops + field->offset );
		toF = (int *)((byte *)ps + field->offset );		
		if(*fromF != *toF || field->force) flags |= 1<<j;
		if( j > 31 || !ent_fields[i+1].name) // dump packet
		{
			MSG_WriteLong( msg, flags );	// send flags who indicates changes
			for( k = 0; field2->name; k++, field2++ )
			{
				if( k > 31 ) break; // return to main cycle
				toF = (int *)((byte *)ps + field2->offset );
				if( flags & 1<<k ) MSG_WriteBits( msg, *toF, field2->bits );
			}
			j = flags = 0;
		}
	}
}


/*
===================
MSG_ReadDeltaPlayerstate
===================
*/
void MSG_ReadDeltaPlayerstate( sizebuf_t *msg, entity_state_t *from, entity_state_t *to )
{
	net_field_t	*field;
	int		*fromF, *toF;
	entity_state_t	dummy;
	uint		i, flags;

	// clear to old value before delta parsing
	if( !from )
	{
		from = &dummy;
		memset( &dummy, 0, sizeof( dummy ));
	}
	*to = *from;
	
	for( i = 0, field = ent_fields; field->name; i++, field++ )
	{
		// get flags of next packet if LONG out of range
		if((i & 31) == 0) flags = MSG_ReadLong( msg );
		fromF = (int *)((byte *)from + field->offset );
		toF = (int *)((byte *)to + field->offset );
		
		if(flags & (1<<i)) *toF = MSG_ReadBits( msg, field->bits );
		else *toF = *fromF;	// no change
	}
}