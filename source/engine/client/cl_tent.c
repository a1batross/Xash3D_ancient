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
	TE_SPIKE,
	TE_SPARKS,
	TE_GUNSHOT,
	TE_SUPERSPIKE,
	TE_EXPLOSION,
	TE_LIGHTNING1,	
	TE_LIGHTNING2,
	TE_LIGHTNING3,
	TE_LAVASPLASH,
	TE_BLOOD
} temp_event_t;

/*
=================
 CL_FindExplosionPlane

 Disgusting hack
=================
*/
static void CL_FindExplosionPlane( const vec3_t org, float radius, vec3_t dir )
{
	static vec3_t	planes[6] = {{0, 0, 1}, {0, 1, 0}, {1, 0, 0}, {0, 0, -1}, {0, -1, 0}, {-1, 0, 0}};
	trace_t		trace;
	vec3_t		point;
	float		best = 1.0;
	int		i;

	VectorClear( dir );

	for( i = 0; i < 6; i++ )
	{
		VectorMA( org, radius, planes[i], point );

		trace = CL_Trace( org, vec3_origin, vec3_origin, point, MOVE_WORLDONLY, NULL, MASK_SOLID );
		if( trace.allsolid || trace.fraction == 1.0 )
			continue;

		if( trace.fraction < best )
		{
			best = trace.fraction;
			VectorCopy( trace.plane.normal, dir );
		}
	}
}

/*
=================
CL_ParseTempEnts
=================
*/
void CL_ParseTempEnts( sizebuf_t *msg )
{
	int		type;
	vec3_t		pos, dir;
	sound_t		sound_fx;
	shader_t		sfx_1, sfx_2;

	type = MSG_ReadByte( msg );

	switch( type )
	{
	case TE_TELEPORT:
		MSG_ReadPos( msg, pos );
		CL_TeleportSplash( pos );
		break;
	case TE_BLOOD:			// bullet hitting flesh
		MSG_ReadPos(msg, pos);
		MSG_ReadPos(msg, dir);
		CL_ParticleEffect( pos, dir, 0xe8, 60 );
		break;
	case TE_SPIKE:
		MSG_ReadPos(msg, pos);
		break;
	case TE_GUNSHOT:	// bullet hitting wall
		MSG_ReadPos(msg, pos);
		CL_ParticleEffect( pos, dir, 0, 40 );
		break;
	case TE_EXPLOSION:
		MSG_ReadPos(msg, pos);
		CL_FindExplosionPlane( pos, 40, dir );

		sfx_1 = re->RegisterShader( "engine/explosion", SHADER_GENERIC );
		sfx_2 = re->RegisterShader( "engine/{scorch", SHADER_GENERIC );
		// CL_Explosion( pos, dir, 40, rand() % 360, 350, 1.0, 0.5, 0.5, sfx_1 );
		CL_ExplosionParticles( pos );
		// CL_ImpactMark( pos, dir, rand() % 360, 40, 1, 1, 1, 1, false, sfx_2, false );
		sound_fx = S_RegisterSound( "weapons/r_exp3.wav" );
		S_StartSound( pos, 0, CHAN_AUTO, sound_fx, 1.0f, ATTN_NORM );
		break;
	default:
		Host_Error( "CL_ParseTEnt: bad type %i\n", type );
		break;
	}
}