//=======================================================================
//			Copyright XashXT Group 2007 ©
//			net_msg.c - network messages
//=======================================================================

#include "engine.h"

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
		Msg("MSG_WriteChar: range error %d (called at %s:%i)\n", c, filename, fileline);

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void _MSG_WriteByte (sizebuf_t *sb, int c, const char *filename, int fileline)
{
	byte	*buf;
	
	if (c < 0 || c > 255)
		Msg("MSG_WriteByte: range error %d (called at %s:%i)\n", c, filename, fileline);

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void _MSG_WriteShort (sizebuf_t *sb, int c, const char *filename, int fileline)
{
	byte	*buf;
	
	if (c < -32767 || c > 32767)
		Msg("MSG_WriteShort: range error %d (called at %s:%i)\n", c, filename, fileline);

	buf = SZ_GetSpace (sb, 2);
	buf[0] = c&0xff;
	buf[1] = c>>8;
}

void _MSG_WriteWord (sizebuf_t *sb, int c, const char *filename, int fileline)
{
	byte	*buf;
	
	if (c < 0 || c > 65535)
		Msg("MSG_WriteWord: range error %d (called at %s:%i)\n", c, filename, fileline);

	buf = SZ_GetSpace (sb, 2);
	buf[0] = c&0xff;
	buf[1] = c>>8;
}

void _MSG_WriteLong (sizebuf_t *sb, int c, const char *filename, int fileline)
{
	byte	*buf;
	
	buf = SZ_GetSpace (sb, 4);
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
	SZ_Write (sb, &dat.l, 4);
}

void _MSG_WriteString (sizebuf_t *sb, char *s, const char *filename, int fileline)
{
	if (!s || !*s) _MSG_WriteChar (sb, 0, filename, fileline );
	else SZ_Write (sb, s, strlen(s)+1);
}

void _MSG_WriteUnterminatedString (sizebuf_t *sb, const char *s, const char *filename, int fileline)
{
	if (s && *s) SZ_Write (sb, (byte *)s, (int)strlen(s));
}

void _MSG_WriteCoord (sizebuf_t *sb, float f, const char *filename, int fileline)
{
	_MSG_WriteShort (sb, (int)(f * 8), filename, fileline );
}

void _MSG_WriteVector (sizebuf_t *sb, float *v, const char *filename, int fileline )
{
	_MSG_WriteCoord (sb, v[0], filename, fileline );
	_MSG_WriteCoord (sb, v[1], filename, fileline );
	_MSG_WriteCoord (sb, v[2], filename, fileline );
}

void _MSG_WritePos (sizebuf_t *sb, vec3_t pos, const char *filename, int fileline)
{
	_MSG_WriteShort (sb, (int)(pos[0] * 8), filename, fileline );
	_MSG_WriteShort (sb, (int)(pos[1] * 8), filename, fileline );
	_MSG_WriteShort (sb, (int)(pos[2] * 8), filename, fileline );
}

void _MSG_WriteAngle (sizebuf_t *sb, float f, const char *filename, int fileline)
{
	_MSG_WriteByte (sb, (int)(f * 256/360) & 255, filename, fileline );
}

void _MSG_WriteAngle16 (sizebuf_t *sb, float f, const char *filename, int fileline)
{
	_MSG_WriteWord (sb, ANGLE2SHORT(f), filename, fileline );
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
	if ( to->frame != from->frame )
	{
		if (to->frame < 256) bits |= U_FRAME8;
		else bits |= U_FRAME16;
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
	
	if ( to->solid != from->solid ) bits |= U_SOLID;
	if ( to->event ) bits |= U_EVENT; // event is not delta compressed, just 0 compressed
	
	if ( to->modelindex != from->modelindex ) bits |= U_MODEL;
	if ( to->weaponmodel != from->weaponmodel ) bits |= U_WEAPONMODEL;
	if ( to->body != from->body ) bits |= U_BODY;
	if ( to->sequence != from->sequence ) bits |= U_SEQUENCE;
	if ( to->sound != from->sound ) bits |= U_SOUND;
	if (newentity || (to->renderfx & RF_BEAM)) bits |= U_OLDORIGIN;
	if( to->alpha != from->alpha ) bits |= U_ALPHA;

	// write the message
	if (!bits && !force) return; // nothing to send!

	//----------

	if (bits & 0xff000000) bits |= U_MOREBITS3 | U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x00ff0000) bits |= U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x0000ff00) bits |= U_MOREBITS1;

	_MSG_WriteByte (msg, bits & 255, filename, fileline );

	if (bits & 0xff000000)
	{
		_MSG_WriteByte (msg, (bits >> 8 ) & 255, filename, fileline );
		_MSG_WriteByte (msg, (bits >> 16) & 255, filename, fileline );
		_MSG_WriteByte (msg, (bits >> 24) & 255, filename, fileline );
	}
	else if (bits & 0x00ff0000)
	{
		_MSG_WriteByte (msg, (bits >> 8 ) & 255, filename, fileline );
		_MSG_WriteByte (msg, (bits >> 16) & 255, filename, fileline );
	}
	else if (bits & 0x0000ff00)
	{
		_MSG_WriteByte (msg, (bits >> 8 ) & 255, filename, fileline );
	}

	//----------

	if (bits & U_NUMBER16) _MSG_WriteShort (msg, to->number, filename, fileline );
	else _MSG_WriteByte (msg, to->number, filename, fileline);

	if (bits & U_MODEL) _MSG_WriteByte (msg, to->modelindex, filename, fileline);
	if (bits & U_WEAPONMODEL) _MSG_WriteByte (msg, to->weaponmodel, filename, fileline);

	if (bits & U_FRAME8) _MSG_WriteByte (msg, to->frame, filename, fileline);
	if (bits & U_FRAME16) _MSG_WriteShort (msg, to->frame, filename, fileline);

	if (bits & U_SKIN8 ) _MSG_WriteByte (msg, to->skin, filename, fileline);
	if (bits & U_SKIN16) _MSG_WriteShort (msg, to->skin, filename, fileline);

	if ( (bits & (U_EFFECTS8|U_EFFECTS16)) == (U_EFFECTS8|U_EFFECTS16))
		_MSG_WriteLong (msg, to->effects, filename, fileline);
	else if (bits & U_EFFECTS8) _MSG_WriteByte (msg, to->effects, filename, fileline);
	else if (bits & U_EFFECTS16) _MSG_WriteShort (msg, to->effects, filename, fileline);

	if ( (bits & (U_RENDERFX8|U_RENDERFX16)) == (U_RENDERFX8|U_RENDERFX16)) 
		_MSG_WriteLong (msg, to->renderfx, filename, fileline);
	else if (bits & U_RENDERFX8) _MSG_WriteByte (msg, to->renderfx, filename, fileline);
	else if (bits & U_RENDERFX16) _MSG_WriteShort (msg, to->renderfx, filename, fileline);

	if (bits & U_ORIGIN1) _MSG_WriteCoord (msg, to->origin[0], filename, fileline);		
	if (bits & U_ORIGIN2) _MSG_WriteCoord (msg, to->origin[1], filename, fileline);
	if (bits & U_ORIGIN3) _MSG_WriteCoord (msg, to->origin[2], filename, fileline);

	if (bits & U_ANGLE1) _MSG_WriteAngle(msg, to->angles[0], filename, fileline);
	if (bits & U_ANGLE2) _MSG_WriteAngle(msg, to->angles[1], filename, fileline);
	if (bits & U_ANGLE3) _MSG_WriteAngle(msg, to->angles[2], filename, fileline);

	if (bits & U_OLDORIGIN)
	{
		_MSG_WriteCoord (msg, to->old_origin[0], filename, fileline);
		_MSG_WriteCoord (msg, to->old_origin[1], filename, fileline);
		_MSG_WriteCoord (msg, to->old_origin[2], filename, fileline);
	}

	if (bits & U_SEQUENCE) _MSG_WriteByte (msg, to->sequence, filename, fileline);
	if (bits & U_SOLID) _MSG_WriteShort (msg, to->solid, filename, fileline);
	if (bits & U_ALPHA) _MSG_WriteFloat (msg, to->alpha, filename, fileline);
	if (bits & U_SOUND) _MSG_WriteByte (msg, to->sound, filename, fileline);
	if (bits & U_EVENT) _MSG_WriteByte (msg, to->event, filename, fileline);
	if (bits & U_BODY) _MSG_WriteByte (msg,	to->body, filename, fileline);
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
	MsgWarn("MSG_EndReading: received with errors\n");
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

float MSG_ReadCoord (sizebuf_t *msg_read)
{
	return MSG_ReadShort(msg_read) * (1.0/8);
}

void MSG_ReadPos (sizebuf_t *msg_read, vec3_t pos)
{
	pos[0] = MSG_ReadShort(msg_read) * (1.0/8);
	pos[1] = MSG_ReadShort(msg_read) * (1.0/8);
	pos[2] = MSG_ReadShort(msg_read) * (1.0/8);
}

float MSG_ReadAngle (sizebuf_t *msg_read)
{
	return MSG_ReadChar(msg_read) * (360.0/256);
}

float MSG_ReadAngle16 (sizebuf_t *msg_read)
{
	return SHORT2ANGLE(MSG_ReadShort(msg_read));
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

void MSG_ReadData (sizebuf_t *msg_read, void *data, int len)
{
	int		i;

	for (i = 0; i < len; i++)
		((byte *)data)[i] = MSG_ReadByte (msg_read);
}


/*
=======================
   SZ BUFFER
=======================
*/

void SZ_Init (sizebuf_t *buf, byte *data, int length)
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

void *SZ_GetSpace (sizebuf_t *buf, int length)
{
	void	*data;
	
	if (buf->cursize + length > buf->maxsize)
	{
		if (length > buf->maxsize)
			Host_Error("SZ_GetSpace: %i is > full buffer size\n", length);
			
		Msg ("SZ_GetSpace: overflow [cursize %d maxsize %d]\n", buf->cursize + length, buf->maxsize );
		SZ_Clear (buf); 
		buf->overflowed = true;
	}
	data = buf->data + buf->cursize;
	buf->cursize += length;
	
	return data;
}

void SZ_Write (sizebuf_t *buf, void *data, int length)
{
	Mem_Copy(SZ_GetSpace(buf, length), data, length);		
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