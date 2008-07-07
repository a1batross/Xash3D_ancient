//=======================================================================
//			Copyright XashXT Group 2007 ©
//			net_msg.c - network messages
//=======================================================================

#include "common.h"
#include "mathlib.h"

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
void _MSG_WriteChar (sizebuf_t *sb, int c, const char *filename, int fileline)
{
	byte	*buf;
	
	if (c < -128 || c > 127) 
		MsgDev( D_ERROR, "MSG_WriteChar: range error %d (called at %s:%i)\n", c, filename, fileline);

	buf = _SZ_GetSpace (sb, 1, filename, fileline );
	buf[0] = c;
}

void _MSG_WriteByte (sizebuf_t *sb, int c, const char *filename, int fileline)
{
	byte	*buf;
	
	if (c < 0 || c > 255)
		MsgDev( D_ERROR, "MSG_WriteByte: range error %d (called at %s:%i)\n", c, filename, fileline);

	buf = _SZ_GetSpace (sb, 1, filename, fileline);
	buf[0] = c;
}

void _MSG_WriteData( sizebuf_t *sb, const void *data, size_t length, const char *filename, int fileline )
{
	int	i;

	for( i = 0; i < length; i++ )
	{
		_MSG_WriteByte( sb, ((byte *)data)[i], filename, fileline );
	}
}

void _MSG_WriteShort (sizebuf_t *sb, int c, const char *filename, int fileline)
{
	byte	*buf;
	
	if (c < -32767 || c > 32767)
		MsgDev( D_ERROR, "MSG_WriteShort: range error %d (called at %s:%i)\n", c, filename, fileline);

	buf = _SZ_GetSpace (sb, 2, filename, fileline);
	buf[0] = c&0xff;
	buf[1] = c>>8;
}

void _MSG_WriteWord (sizebuf_t *sb, int c, const char *filename, int fileline)
{
	byte	*buf;
	
	if (c < 0 || c > 65535)
		MsgDev( D_ERROR, "MSG_WriteWord: range error %d (called at %s:%i)\n", c, filename, fileline);

	buf = _SZ_GetSpace (sb, 2, filename, fileline);
	buf[0] = c&0xff;
	buf[1] = c>>8;
}

void _MSG_WriteLong (sizebuf_t *sb, int c, const char *filename, int fileline)
{
	byte	*buf;
	
	buf = _SZ_GetSpace (sb, 4, filename, fileline);
	buf[0] = (c>>0 ) & 0xff;
	buf[1] = (c>>8 ) & 0xff;
	buf[2] = (c>>16) & 0xff;
	buf[3] = (c>>24);
}

void _MSG_WriteFloat (sizebuf_t *sb, float f, const char *filename, int fileline)
{
	union
	{
		float	f;
		int	l;
	} dat;
	
	dat.f = f;
	dat.l = LittleLong (dat.l);
	_SZ_Write (sb, &dat.l, 4, filename, fileline);
}

void _MSG_WriteString (sizebuf_t *sb, const char *s, const char *filename, int fileline)
{
	if (!s || !*s) _MSG_WriteChar (sb, 0, filename, fileline );
	else _SZ_Write (sb, s, strlen(s)+1, filename, fileline);
}

void _MSG_WriteUnterminatedString (sizebuf_t *sb, const char *s, const char *filename, int fileline)
{
	if (s && *s) _SZ_Write (sb, (byte *)s, (int)strlen(s), filename, fileline);
}

void _MSG_WriteCoord16(sizebuf_t *sb, float f, const char *filename, int fileline)
{
	_MSG_WriteShort(sb, (int)(f * SV_COORD_FRAC), filename, fileline );
}

void _MSG_WriteAngle16 (sizebuf_t *sb, float f, const char *filename, int fileline)
{
	_MSG_WriteWord(sb, ANGLE2SHORT(f), filename, fileline );
}

void _MSG_WriteCoord32(sizebuf_t *sb, float f, const char *filename, int fileline)
{
	_MSG_WriteFloat(sb, f, filename, fileline );
}

void _MSG_WriteAngle32(sizebuf_t *sb, float f, const char *filename, int fileline)
{
	_MSG_WriteFloat(sb, f, filename, fileline );
}

void _MSG_WritePos16(sizebuf_t *sb, vec3_t pos, const char *filename, int fileline)
{
	_MSG_WriteCoord16(sb, pos[0], filename, fileline );
	_MSG_WriteCoord16(sb, pos[1], filename, fileline );
	_MSG_WriteCoord16(sb, pos[2], filename, fileline );
}

void _MSG_WritePos32(sizebuf_t *sb, vec3_t pos, const char *filename, int fileline)
{
	_MSG_WriteCoord32(sb, pos[0], filename, fileline );
	_MSG_WriteCoord32(sb, pos[1], filename, fileline );
	_MSG_WriteCoord32(sb, pos[2], filename, fileline );
}

void _MSG_WriteDeltaUsercmd (sizebuf_t *buf, usercmd_t *from, usercmd_t *cmd, const char *filename, int fileline)
{
	int	bits = 0;

	// send the movement message
	if (cmd->angles[0] != from->angles[0]) bits |= CM_ANGLE1;
	if (cmd->angles[1] != from->angles[1]) bits |= CM_ANGLE2;
	if (cmd->angles[2] != from->angles[2]) bits |= CM_ANGLE3;

	if (cmd->forwardmove != from->forwardmove) bits |= CM_FORWARD;
	if (cmd->sidemove != from->sidemove) bits |= CM_SIDE;
	if (cmd->upmove != from->upmove) bits |= CM_UP;

	if (cmd->buttons != from->buttons) bits |= CM_BUTTONS;
	if (cmd->impulse != from->impulse) bits |= CM_IMPULSE;

	_MSG_WriteByte (buf, bits, filename, fileline );

	if (bits & CM_ANGLE1) _MSG_WriteShort (buf, cmd->angles[0], filename, fileline );
	if (bits & CM_ANGLE2) _MSG_WriteShort (buf, cmd->angles[1], filename, fileline );
	if (bits & CM_ANGLE3) _MSG_WriteShort (buf, cmd->angles[2], filename, fileline );
	
	if (bits & CM_FORWARD) _MSG_WriteShort (buf, cmd->forwardmove, filename, fileline );
	if (bits & CM_SIDE) _MSG_WriteShort (buf, cmd->sidemove, filename, fileline );
	if (bits & CM_UP) _MSG_WriteShort (buf, cmd->upmove, filename, fileline );

 	if (bits & CM_BUTTONS) _MSG_WriteByte (buf, cmd->buttons, filename, fileline );
 	if (bits & CM_IMPULSE) _MSG_WriteByte (buf, cmd->impulse, filename, fileline );

	_MSG_WriteByte (buf, cmd->msec, filename, fileline );
	_MSG_WriteByte (buf, cmd->lightlevel, filename, fileline );
}

/*
==================
MSG_WriteDeltaEntity

Writes part of a packetentities message.
Can delta from either a baseline or a previous packet_entity
==================
*/
void _MSG_WriteDeltaEntity (entity_state_t *from, entity_state_t *to, sizebuf_t *msg, bool force, bool newentity, const char *filename, int fileline)
{
	int	bits = 0;

	if (!to->number) Host_Error("Unset entity number\n");
	if (to->number >= MAX_EDICTS) Host_Error("Entity number >= MAX_EDICTS\n");

	// send an update
	if (to->number >= 256) bits |= U_NUMBER16; // number8 is implicit otherwise

	if (to->origin[0] != from->origin[0]) bits |= U_ORIGIN1;
	if (to->origin[1] != from->origin[1]) bits |= U_ORIGIN2;
	if (to->origin[2] != from->origin[2]) bits |= U_ORIGIN3;

	if ( to->angles[0] != from->angles[0] ) bits |= U_ANGLE1;		
	if ( to->angles[1] != from->angles[1] ) bits |= U_ANGLE2;
	if ( to->angles[2] != from->angles[2] ) bits |= U_ANGLE3;
		
	if ( to->skin != from->skin ) 
	{
		if (to->skin < 256) bits |= U_SKIN8;
		else bits |= U_SKIN16;
	}		
	if ( to->effects != from->effects )
	{
		if (to->effects < 256) bits |= U_EFFECTS8;
		else if (to->effects < 32768) bits |= U_EFFECTS16;
		else bits |= U_EFFECTS8 | U_EFFECTS16;
	}
	if ( to->renderfx != from->renderfx )
	{
		if (to->renderfx < 256) bits |= U_RENDERFX8;
		else if (to->renderfx < 32768) bits |= U_RENDERFX16;
		else bits |= U_RENDERFX8 | U_RENDERFX16;
	}

	if ( to->frame != from->frame ) bits |= U_FRAME;
	if ( to->solid != from->solid ) bits |= U_SOLID;
	if ( to->modelindex != from->modelindex ) bits |= U_MODEL;
	if ( to->weaponmodel != from->weaponmodel ) bits |= U_WEAPONMODEL;
	if ( to->body != from->body ) bits |= U_BODY;
	if ( to->sequence != from->sequence ) bits |= U_SEQUENCE;
	if ( to->soundindex != from->soundindex ) bits |= U_SOUNDIDX;
	if ( to->alpha != from->alpha ) bits |= U_ALPHA;
	if ( to->animtime != from->animtime ) bits |= U_ANIMTIME;
	if ( newentity ) bits |= U_OLDORIGIN;
	
	// write the message
	if (!bits && !force) return; // nothing to send!

	if (bits & 0xff000000) bits |= U_MOREBITS3 | U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x00ff0000) bits |= U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x0000ff00) bits |= U_MOREBITS1;

	_MSG_WriteByte (msg, bits & 255, filename, fileline );

	if (bits & 0xff000000)
	{
		MSG_WriteByte (msg, (bits >> 8 ) & 255 );
		MSG_WriteByte (msg, (bits >> 16) & 255 );
		MSG_WriteByte (msg, (bits >> 24) & 255 );
	}
	else if (bits & 0x00ff0000)
	{
		MSG_WriteByte (msg, (bits >> 8 ) & 255 );
		MSG_WriteByte (msg, (bits >> 16) & 255 );
	}
	else if (bits & 0x0000ff00)
	{
		MSG_WriteByte (msg, (bits >> 8 ) & 255 );
	}
	//----------

	if (bits & U_NUMBER16)
	{
		MSG_WriteShort (msg, to->number );
	}
	else _MSG_WriteByte (msg, to->number, filename, fileline);

	if (bits & U_MODEL) MSG_WriteShort (msg, to->modelindex );
	if (bits & U_WEAPONMODEL) _MSG_WriteShort (msg, to->weaponmodel, filename, fileline);

	if (bits & U_FRAME ) _MSG_WriteFloat (msg, to->frame, filename, fileline);
	if (bits & U_SKIN8 ) _MSG_WriteByte (msg, to->skin, filename, fileline);
	if (bits & U_SKIN16) 
	{
		MSG_WriteShort( msg, to->skin );
          }
	if ( (bits & (U_EFFECTS8|U_EFFECTS16)) == (U_EFFECTS8|U_EFFECTS16))
		_MSG_WriteLong (msg, to->effects, filename, fileline);
	else if (bits & U_EFFECTS8) _MSG_WriteByte (msg, to->effects, filename, fileline);
	else if (bits & U_EFFECTS16)
	{
		MSG_WriteShort( msg, to->effects );
          }
	if ( (bits & (U_RENDERFX8|U_RENDERFX16)) == (U_RENDERFX8|U_RENDERFX16)) 
		_MSG_WriteLong (msg, to->renderfx, filename, fileline);
	else if (bits & U_RENDERFX8) _MSG_WriteByte (msg, to->renderfx, filename, fileline);
	else if (bits & U_RENDERFX16) 
	{
		MSG_WriteShort( msg, to->renderfx );
	}

	if (bits & U_ORIGIN1) MSG_WriteCoord32(msg, to->origin[0]);
	if (bits & U_ORIGIN2) MSG_WriteCoord32(msg, to->origin[1]);
	if (bits & U_ORIGIN3) MSG_WriteCoord32(msg, to->origin[2]);

	if (bits & U_ANGLE1) MSG_WriteAngle32(msg, to->angles[0]);
	if (bits & U_ANGLE2) MSG_WriteAngle32(msg, to->angles[1]);
	if (bits & U_ANGLE3) MSG_WriteAngle32(msg, to->angles[2]);

	if (bits & U_OLDORIGIN) _MSG_WritePos32(msg, to->old_origin, filename, fileline);
	if (bits & U_SEQUENCE) _MSG_WriteByte (msg, to->sequence, filename, fileline);
	if (bits & U_SOLID) 
	{
		MSG_WriteLong( msg, to->solid );
	}
	if (bits & U_ALPHA) _MSG_WriteFloat (msg, to->alpha, filename, fileline);
	if (bits & U_SOUNDIDX) _MSG_WriteByte (msg, to->soundindex, filename, fileline); 
	if (bits & U_BODY) _MSG_WriteByte (msg,	to->body, filename, fileline);
	if (bits & U_ANIMTIME) _MSG_WriteFloat (msg, to->animtime, filename, fileline); 
}

/*
=======================
   reading functions
=======================
*/
void MSG_BeginReading (sizebuf_t *msg)
{
	msg->readcount = 0;
	msg->errorcount = 0;
}

void MSG_EndReading (sizebuf_t *msg)
{
	if(!msg->errorcount) return;
	MsgDev( D_ERROR, "MSG_EndReading: received with errors\n");
}

// returns -1 if no more characters are available
int MSG_ReadChar (sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount + 1 > msg_read->cursize)
	{
		c = -1;
		msg_read->errorcount++;
	}
	else c = (signed char)msg_read->data[msg_read->readcount];
	msg_read->readcount++;
	
	return c;
}

int MSG_ReadByte (sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount + 1 > msg_read->cursize)
	{
		c = -1;
		msg_read->errorcount++;
	}
	else c = (byte)msg_read->data[msg_read->readcount];
	msg_read->readcount++;
	
	return c;
}

int MSG_ReadShort (sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount + 2 > msg_read->cursize)
	{
		c = -1;
		msg_read->errorcount++;
	}
	else c = (short)BuffLittleShort(msg_read->data + msg_read->readcount);
	msg_read->readcount += 2;
	
	return c;
}

int MSG_ReadLong (sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount + 4 > msg_read->cursize)
	{
		c = -1;
		msg_read->errorcount++;
	}
	else c = BuffLittleLong(msg_read->data + msg_read->readcount);
	msg_read->readcount += 4;
	
	return c;
}

float MSG_ReadFloat (sizebuf_t *msg_read)
{
	union
	{
		byte	b[4];
		float	f;
		int	l;
	} dat;

	if (msg_read->readcount + 4 > msg_read->cursize)
	{
		dat.f = -1;
		msg_read->errorcount++;
	}
	else 
	{
		dat.b[0] = msg_read->data[msg_read->readcount+0];
		dat.b[1] = msg_read->data[msg_read->readcount+1];
		dat.b[2] = msg_read->data[msg_read->readcount+2];
		dat.b[3] = msg_read->data[msg_read->readcount+3];
	}
	msg_read->readcount += 4;
	dat.l = LittleLong (dat.l);

	return dat.f;	
}

char *MSG_ReadString (sizebuf_t *msg_read)
{
	static char	string[2048];
	int		l = 0, c;
	
	do
	{
		c = MSG_ReadChar (msg_read);
		if (c == -1 || c == 0) break;
		string[l] = c;
		l++;
	} while (l < sizeof(string) - 1);
	string[l] = 0; //write terminator
	
	return string;
}

char *MSG_ReadStringLine (sizebuf_t *msg_read)
{
	static char	string[2048];
	int		l = 0, c;
	
	do
	{
		c = MSG_ReadChar (msg_read);
		if (c == -1 || c == 0 || c == '\n') break;
		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);
	string[l] = 0; // write terminator
	
	return string;
}

float MSG_ReadCoord16(sizebuf_t *msg_read)
{
	return MSG_ReadShort(msg_read) * CL_COORD_FRAC;
}

float MSG_ReadCoord32(sizebuf_t *msg_read)
{
	return MSG_ReadFloat(msg_read);
}

void MSG_ReadPos32(sizebuf_t *msg_read, vec3_t pos)
{
	pos[0] = MSG_ReadFloat(msg_read);
	pos[1] = MSG_ReadFloat(msg_read);
	pos[2] = MSG_ReadFloat(msg_read);
}

void MSG_ReadPos16( sizebuf_t *msg_read, vec3_t pos )
{
	pos[0] = MSG_ReadCoord16(msg_read);
	pos[1] = MSG_ReadCoord16(msg_read);
	pos[2] = MSG_ReadCoord16(msg_read);
}

float MSG_ReadAngle16(sizebuf_t *msg_read)
{
	return SHORT2ANGLE(MSG_ReadShort(msg_read));
}

float MSG_ReadAngle32(sizebuf_t *msg_read)
{
	return MSG_ReadFloat(msg_read);
}

void MSG_ReadDeltaUsercmd (sizebuf_t *msg_read, usercmd_t *from, usercmd_t *move)
{
	int bits = MSG_ReadByte (msg_read);
	Mem_Copy (move, from, sizeof(*move));

	// read current angles
	if (bits & CM_ANGLE1) move->angles[0] = MSG_ReadShort (msg_read);
	if (bits & CM_ANGLE2) move->angles[1] = MSG_ReadShort (msg_read);
	if (bits & CM_ANGLE3) move->angles[2] = MSG_ReadShort (msg_read);
		
	// read movement
	if (bits & CM_FORWARD) move->forwardmove = MSG_ReadShort (msg_read);
	if (bits & CM_SIDE) move->sidemove = MSG_ReadShort (msg_read);
	if (bits & CM_UP) move->upmove = MSG_ReadShort (msg_read);
	
	// read buttons
	if (bits & CM_BUTTONS) move->buttons = MSG_ReadByte (msg_read);
	if (bits & CM_IMPULSE) move->impulse = MSG_ReadByte (msg_read);
	
	move->msec = MSG_ReadByte (msg_read); // read time to run command
	move->lightlevel = MSG_ReadByte (msg_read); // read the light level
}

void MSG_ReadDeltaEntity( sizebuf_t *msg_read, entity_state_t *from, entity_state_t *to, int number, int bits )
{
	// set everything to the state we are delta'ing from
	*to = *from;

	VectorCopy (from->origin, to->old_origin);
	to->number = number;

	if (bits & U_MODEL) to->modelindex = MSG_ReadShort (msg_read);
	if (bits & U_WEAPONMODEL) to->weaponmodel = MSG_ReadShort (msg_read);
		
	if (bits & U_FRAME ) to->frame = MSG_ReadFloat (msg_read);
	if (bits & U_SKIN8 ) to->skin = MSG_ReadByte(msg_read);
	if (bits & U_SKIN16) to->skin = MSG_ReadShort(msg_read);

	if ( (bits & (U_EFFECTS8|U_EFFECTS16)) == (U_EFFECTS8|U_EFFECTS16) )
		to->effects = MSG_ReadLong(msg_read);
	else if (bits & U_EFFECTS8 ) to->effects = MSG_ReadByte(msg_read);
	else if (bits & U_EFFECTS16) to->effects = MSG_ReadShort(msg_read);

	if ( (bits & (U_RENDERFX8|U_RENDERFX16)) == (U_RENDERFX8|U_RENDERFX16) )
		to->renderfx = MSG_ReadLong(msg_read);
	else if (bits & U_RENDERFX8 ) to->renderfx = MSG_ReadByte(msg_read);
	else if (bits & U_RENDERFX16) to->renderfx = MSG_ReadShort(msg_read);

	if (bits & U_ORIGIN1) to->origin[0] = MSG_ReadCoord32(msg_read);
	if (bits & U_ORIGIN2) to->origin[1] = MSG_ReadCoord32(msg_read);
	if (bits & U_ORIGIN3) to->origin[2] = MSG_ReadCoord32(msg_read);
		
	if (bits & U_ANGLE1) to->angles[0] = MSG_ReadAngle32(msg_read);
	if (bits & U_ANGLE2) to->angles[1] = MSG_ReadAngle32(msg_read);
	if (bits & U_ANGLE3) to->angles[2] = MSG_ReadAngle32(msg_read);
	if (bits & U_OLDORIGIN) MSG_ReadPos32(msg_read, to->old_origin);

	if (bits & U_SEQUENCE) to->sequence = MSG_ReadByte(msg_read);
	if (bits & U_SOLID) to->solid = MSG_ReadLong(msg_read);
	if (bits & U_ALPHA) to->alpha = MSG_ReadFloat(msg_read);
	if (bits & U_SOUNDIDX) to->soundindex = MSG_ReadByte (msg_read);

	if (bits & U_BODY) to->body = MSG_ReadByte (msg_read);
	if (bits & U_ANIMTIME) to->animtime = MSG_ReadFloat (msg_read);
}

void MSG_ReadData (sizebuf_t *msg_read, void *data, int len)
{
	int		i;

	for (i = 0; i < len; i++)
	{
		((byte *)data)[i] = MSG_ReadByte (msg_read);
	}
}


/*
=======================
   SZ BUFFER
=======================
*/

void SZ_Init( sizebuf_t *buf, byte *data, size_t length )
{
	memset (buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
}

void SZ_Clear (sizebuf_t *buf)
{
	buf->cursize = 0;
	buf->overflowed = false;
}

void *_SZ_GetSpace (sizebuf_t *buf, int length, const char *filename, int fileline )
{
	void	*data;
	
	if (buf->cursize + length > buf->maxsize)
	{
		if (length > buf->maxsize)
			Host_Error("SZ_GetSpace: length[%i] > buffer maxsize [%i], called at %s:%i\n", length, buf->maxsize, filename, fileline );
			
		MsgDev( D_WARN, "SZ_GetSpace: overflow [cursize %d maxsize %d], called at %s:%i\n", buf->cursize + length, buf->maxsize, filename, fileline );
		SZ_Clear (buf); 
		buf->overflowed = true;
	}
	data = buf->data + buf->cursize;
	buf->cursize += length;
	
	return data;
}

void _SZ_Write (sizebuf_t *buf, const void *data, int length, const char *filename, int fileline)
{
	Mem_Copy(_SZ_GetSpace(buf, length, filename, fileline), (void *)data, length);		
}

void SZ_Print (sizebuf_t *buf, char *data)
{
	int		len;
	
	len = strlen(data) + 1;
	if (buf->cursize)
	{
		if (buf->data[buf->cursize - 1]) Mem_Copy ((byte *)SZ_GetSpace(buf, len),data,len); //no trailing 0
		else Mem_Copy ((byte *)SZ_GetSpace(buf, len-1)-1,data,len); // write over trailing 0
	}
	else Mem_Copy ((byte *)SZ_GetSpace(buf, len),data,len);
}