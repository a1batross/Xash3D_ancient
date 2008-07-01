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
// cl_tent.c -- client side temporary entities

#include "common.h"
#include "client.h"

// temp entity events
typedef enum
{
	TE_TELEPORT = 0,
	TE_GUNSHOT,
	TE_BLOOD,
} temp_event_t;

/*
=================
CL_ParseTempEnts
=================
*/
void CL_ParseTempEnts( sizebuf_t *msg )
{
	int		type;
	vec3_t		pos, dir;

	type = MSG_ReadByte( msg );

	switch( type )
	{
	case TE_TELEPORT:
		MSG_ReadPos32( msg, pos );
		CL_TeleportSplash( pos );
		break;
	case TE_BLOOD:			// bullet hitting flesh
		MSG_ReadPos32(msg, pos);
		MSG_ReadPos32(msg, dir);
		CL_ParticleEffect (pos, dir, 0xe8, 60);
		break;

	case TE_GUNSHOT:	// bullet hitting wall
		MSG_ReadPos32(msg, pos);
		MSG_ReadPos32(msg, dir);
		CL_ParticleEffect( pos, dir, 0, 40 );
		break;
	default:
		Host_Error( "CL_ParseTEnt: bad type\n" );
		break;
	}
}