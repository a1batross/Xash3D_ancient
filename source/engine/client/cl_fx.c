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
// cl_fx.c -- entity effects parsing and management

#include "common.h"
#include "client.h"
#include "const.h"

/*
==============================================================

LIGHT STYLE MANAGEMENT

==============================================================
*/

typedef struct
{
	int	length;
	float	value[3];
	float	map[MAX_STRING];
} clightstyle_t;

clightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
static int	lastofs;
	
/*
================
CL_ClearLightStyles
================
*/
void CL_ClearLightStyles (void)
{
	Mem_Set (cl_lightstyle, 0, sizeof(cl_lightstyle));
	lastofs = -1;
}

/*
================
CL_RunLightStyles
================
*/
void CL_RunLightStyles (void)
{
	int		ofs;
	clightstyle_t	*ls;
	int		i;
	
	ofs = cl.time / 100;
	if( ofs == lastofs ) return;
	lastofs = ofs;

	for( i = 0, ls = cl_lightstyle; i < MAX_LIGHTSTYLES; i++, ls++)
	{
		if( !ls->length )
		{
			ls->value[0] = ls->value[1] = ls->value[2] = 1.0;
			continue;
		}
		if( ls->length == 1 ) ls->value[0] = ls->value[1] = ls->value[2] = ls->map[0];
		else ls->value[0] = ls->value[1] = ls->value[2] = ls->map[ofs%ls->length];
	}
}


void CL_SetLightstyle( int i )
{
	char	*s;
	int	j, k;

	s = cl.configstrings[i+CS_LIGHTSTYLES];
	j = com.strlen( s );
	if( j >= MAX_STRING ) Host_Error("CL_SetLightStyle: lightstyle %s is too long\n", s );

	cl_lightstyle[i].length = j;

	for( k = 0; k < j; k++ )
		cl_lightstyle[i].map[k] = (float)(s[k]-'a')/(float)('m'-'a');
}

/*
================
CL_AddLightStyles
================
*/
void CL_AddLightStyles (void)
{
	int		i;
	clightstyle_t	*ls;

	for( i = 0, ls = cl_lightstyle; i < MAX_LIGHTSTYLES; i++, ls++ )
		re->AddLightStyle( i, ls->value );
}

/*
==============================================================

DLIGHT MANAGEMENT

==============================================================
*/

cdlight_t		cl_dlights[MAX_DLIGHTS];

/*
================
CL_ClearDlights
================
*/
void CL_ClearDlights (void)
{
	Mem_Set (cl_dlights, 0, sizeof(cl_dlights));
}

/*
===============
CL_AllocDlight

===============
*/
cdlight_t *CL_AllocDlight (int key)
{
	int		i;
	cdlight_t	*dl;

	// first look for an exact key match
	if (key)
	{
		dl = cl_dlights;
		for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
		{
			if (dl->key == key)
			{
				Mem_Set (dl, 0, sizeof(*dl));
				dl->key = key;
				return dl;
			}
		}
	}

// then look for anything else
	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (dl->die < cl.time)
		{
			Mem_Set (dl, 0, sizeof(*dl));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	Mem_Set (dl, 0, sizeof(*dl));
	dl->key = key;
	return dl;
}

/*
===============
CL_NewDlight
===============
*/
void CL_NewDlight (int key, float x, float y, float z, float radius, float time)
{
	cdlight_t	*dl;

	dl = CL_AllocDlight (key);
	dl->origin[0] = x;
	dl->origin[1] = y;
	dl->origin[2] = z;
	dl->radius = radius;
	dl->die = cl.time + time;
}


/*
===============
CL_RunDLights

===============
*/
void CL_RunDLights (void)
{
	int			i;
	cdlight_t	*dl;

	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (!dl->radius)
			continue;
		
		if (dl->die < cl.time)
		{
			dl->radius = 0;
			return;
		}
		dl->radius -= cls.frametime*dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}

/*
===============
CL_AddDLights

===============
*/
void CL_AddDLights (void)
{
	int	i;
	cdlight_t	*dl;

	dl = cl_dlights;
	for( i = 0; i < MAX_DLIGHTS; i++, dl++ )
	{
		if( dl->radius ) re->AddDynLight( dl->origin, dl->color, dl->radius );
	}
}



/*
==============================================================

PARTICLE MANAGEMENT

==============================================================
*/
#define PARTICLE_GRAVITY		40

#define PARTICLE_BOUNCE		1
#define PARTICLE_FRICTION		2
#define PARTICLE_VERTEXLIGHT		4
#define PARTICLE_STRETCH		8
#define PARTICLE_UNDERWATER		16
#define PARTICLE_INSTANT		32

typedef struct cparticle_s
{
	struct cparticle_s	*next;
	shader_t		shader;

	int		time;
	int		flags;
	
	vec3_t		org;
	vec3_t		org2;
	vec3_t		vel;
	vec3_t		accel;
	vec3_t		color;
	vec3_t		colorVel;
	float		alpha;
	float		alphaVel;
	float		radius;
	float		radiusVel;
	float		length;
	float		lengthVel;
	float		rotation;
	float		bounceFactor;
} cparticle_t;

cparticle_t *cl_active_particles, *cl_free_particles;
static cparticle_t	cl_particle_list[MAX_PARTICLES];
static vec3_t cl_particle_velocities[NUMVERTEXNORMALS];

/*
=================
CL_FreeParticle
=================
*/
static void CL_FreeParticle( cparticle_t *p )
{
	p->next = cl_free_particles;
	cl_free_particles = p;
}

/*
=================
CL_AllocParticle
=================
*/
static cparticle_t *CL_AllocParticle( void )
{
	cparticle_t	*p;

	if( !cl_free_particles )
		return NULL;

	if( cl_particlelod->integer > 1 )
	{
		if(!(Com_RandomLong( 0, 1 ) % cl_particlelod->integer))
			return NULL;
	}

	p = cl_free_particles;
	cl_free_particles = p->next;
	p->next = cl_active_particles;
	cl_active_particles = p;

	return p;
}

/*
===============
CL_ClearParticles
===============
*/
void CL_ClearParticles( void )
{
	int		i;
	
	cl_active_particles = NULL;
	cl_free_particles = cl_particle_list;

	for( i = 0; i < MAX_PARTICLES; i++ )
		cl_particle_list[i].next = &cl_particle_list[i+1];

	cl_particle_list[MAX_PARTICLES-1].next = NULL;

	for( i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		cl_particle_velocities[i][0] = (rand() & 255) * 0.01;
		cl_particle_velocities[i][1] = (rand() & 255) * 0.01;
		cl_particle_velocities[i][2] = (rand() & 255) * 0.01;
	}
}

/*
===============
CL_AddParticles
===============
*/
void CL_AddParticles( void )
{
	cparticle_t	*p, *next;
	cparticle_t	*active = NULL, *tail = NULL;
	dword		modulate;
	vec3_t		org, org2, vel, color;
	vec3_t		ambientLight;
	float		alpha, radius, length;
	float		time, time2, gravity, dot;
	vec3_t		mins, maxs;
	int		contents;
	trace_t		trace;

	if( !cl_particles->integer ) return;

	if( PRVM_EDICT_NUM( cl.frame.ps.number )->priv.cl->current.gravity != 0 )
		gravity = PRVM_EDICT_NUM( cl.frame.ps.number )->priv.cl->current.gravity / 800.0;
	else gravity = 1.0f;

	for( p = cl_active_particles; p; p = next )
	{
		// grab next now, so if the particle is freed we still have it
		next = p->next;

		time = (cl.time - p->time) * 0.001;
		time2 = time * time;

		alpha = p->alpha + p->alphaVel * time;
		radius = p->radius + p->radiusVel * time;
		length = p->length + p->lengthVel * time;

		if( alpha <= 0 || radius <= 0 || length <= 0 )
		{
			// faded out
			CL_FreeParticle( p );
			continue;
		}

		color[0] = p->color[0] + p->colorVel[0] * time;
		color[1] = p->color[1] + p->colorVel[1] * time;
		color[2] = p->color[2] + p->colorVel[2] * time;

		org[0] = p->org[0] + p->vel[0] * time + p->accel[0] * time2;
		org[1] = p->org[1] + p->vel[1] * time + p->accel[1] * time2;
		org[2] = p->org[2] + p->vel[2] * time + p->accel[2] * time2 * gravity;

		if( p->flags & PARTICLE_UNDERWATER )
		{
			// underwater particle
			VectorSet( org2, org[0], org[1], org[2] + radius );

			if(!(CL_PointContents( org2 ) & MASK_WATER ))
			{
				// not underwater
				CL_FreeParticle( p );
				continue;
			}
		}

		p->next = NULL;
		if( !tail ) active = tail = p;
		else
		{
			tail->next = p;
			tail = p;
		}

		if( p->flags & PARTICLE_FRICTION )
		{
			// water friction affected particle
			contents = CL_PointContents( org );
			if( contents & MASK_WATER )
			{
				// add friction
				if( contents & CONTENTS_WATER )
				{
					VectorScale( p->vel, 0.25, p->vel );
					VectorScale( p->accel, 0.25, p->accel );
				}
				if( contents & CONTENTS_SLIME )
				{
					VectorScale( p->vel, 0.20, p->vel );
					VectorScale( p->accel, 0.20, p->accel );
				}
				if( contents & CONTENTS_LAVA )
				{
					VectorScale( p->vel, 0.10, p->vel );
					VectorScale( p->accel, 0.10, p->accel );
				}

				// don't add friction again
				p->flags &= ~PARTICLE_FRICTION;
				length = 1;
				
				// reset
				p->time = cl.time;
				VectorCopy( org, p->org );
				VectorCopy( color, p->color );
				p->alpha = alpha;
				p->radius = radius;

				// don't stretch
				p->flags &= ~PARTICLE_STRETCH;
				p->length = length;
				p->lengthVel = 0;
			}
		}

		if( p->flags & PARTICLE_BOUNCE )
		{
			edict_t	*clent = PRVM_EDICT_NUM( cl.frame.ps.number );

			// bouncy particle
			VectorSet(mins, -radius, -radius, -radius);
			VectorSet(maxs, radius, radius, radius);

			trace = CL_Trace( p->org2, mins, maxs, org, MOVE_NORMAL, clent, MASK_SOLID );
			if( trace.fraction != 0.0 && trace.fraction != 1.0 )
			{
				// reflect velocity
				time = cl.time - (cls.frametime + cls.frametime * trace.fraction) * 1000;
				time = (time - p->time) * 0.001;

				VectorSet( vel, p->vel[0], p->vel[1], p->vel[2]+p->accel[2]*gravity*time );
				VectorReflect( vel, 0, trace.plane.normal, p->vel );
				VectorScale( p->vel, p->bounceFactor, p->vel );

				// check for stop or slide along the plane
				if( trace.plane.normal[2] > 0 && p->vel[2] < 1 )
				{
					if( trace.plane.normal[2] == 1 )
					{
						VectorClear( p->vel );
						VectorClear( p->accel );
						p->flags &= ~PARTICLE_BOUNCE;
					}
					else
					{
						// FIXME: check for new plane or free fall
						dot = DotProduct( p->vel, trace.plane.normal );
						VectorMA( p->vel, -dot, trace.plane.normal, p->vel );

						dot = DotProduct( p->accel, trace.plane.normal );
						VectorMA( p->accel, -dot, trace.plane.normal, p->accel );
					}
				}

				VectorCopy( trace.endpos, org );
				length = 1;

				// reset
				p->time = cl.time;
				VectorCopy( org, p->org );
				VectorCopy( color, p->color );
				p->alpha = alpha;
				p->radius = radius;

				// don't stretch
				p->flags &= ~PARTICLE_STRETCH;
				p->length = length;
				p->lengthVel = 0;
			}
		}

		// save current origin if needed
		if( p->flags & (PARTICLE_BOUNCE|PARTICLE_STRETCH))
		{
			VectorCopy( p->org2, org2 );
			VectorCopy( org, p->org2 ); // FIXME: pause
		}

		if( p->flags & PARTICLE_VERTEXLIGHT )
		{
			// vertex lit particle
			re->LightForPoint( org, ambientLight );
			VectorMultiply( color, ambientLight, color );
		}

		// bound color and alpha and convert to byte
		modulate = PackRGBA( color[0], color[1], color[2], alpha );

		if( p->flags & PARTICLE_INSTANT )
		{
			// instant particle
			p->alpha = 0;
			p->alphaVel = 0;
		}

		// send the particle to the renderer
		re->AddParticle( p->shader, org, org2, radius, length, p->rotation, modulate );
	}

	cl_active_particles = active;
}

/*
===============
PF_addparticle

void AddParticle(vector, vector, vector, vector, vector, vector, vector, string, float)
===============
*/
void PF_addparticle( void )
{
	float		*pos, *vel, *color, *colorVel;
	float		*alphaVel, *radiusVel, *lengthVel;
	shader_t		shader;
	uint		flags;	
	cparticle_t	*p;

	if( !VM_ValidateArgs( "AddParticle", 9 ))
		return;

	VM_ValidateString( PRVM_G_STRING( OFS_PARM7 ));
	pos = PRVM_G_VECTOR(OFS_PARM0);
	vel = PRVM_G_VECTOR(OFS_PARM1);
	color = PRVM_G_VECTOR(OFS_PARM2);
	colorVel = PRVM_G_VECTOR(OFS_PARM3);
	alphaVel = PRVM_G_VECTOR(OFS_PARM4);
	radiusVel = PRVM_G_VECTOR(OFS_PARM5);
	lengthVel = PRVM_G_VECTOR(OFS_PARM6);
	shader = re->RegisterShader( PRVM_G_STRING(OFS_PARM7), SHADER_GENERIC );
	flags = (uint)PRVM_G_FLOAT(OFS_PARM8);

	p = CL_AllocParticle();
	if( !p )
	{
		VM_Warning( "AddParticle: no free particles\n" );
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	p->shader = shader;
	p->time = cl.time;
	p->flags = flags;

	VectorCopy( pos, p->org );
	VectorCopy( vel, p->vel );
	VectorSet( p->accel, 0.0f, 0.0f, alphaVel[2] ); 
	VectorCopy( color, p->color );
	VectorCopy( colorVel, p->colorVel );
	p->alpha = alphaVel[0];
	p->alphaVel = alphaVel[1];

	p->radius = radiusVel[0];
	p->radiusVel = radiusVel[1];
	p->length = lengthVel[0];
	p->lengthVel = lengthVel[1];
	p->rotation = radiusVel[2];
	p->bounceFactor = lengthVel[2];

	// needs to save old origin
	if( flags & (PARTICLE_BOUNCE|PARTICLE_FRICTION))
		VectorCopy( p->org, p->org2 );
	PRVM_G_FLOAT(OFS_RETURN) = 1;
}

/*
===============
CL_ParticleEffect

Wall impact puffs
===============
*/
void CL_ParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, j;
	cparticle_t	*p;
	float		d;

	for( i = 0; i < count; i++ )
	{
		p = CL_AllocParticle();
		if( !p ) return;

		p->time = cl.time;
		VectorCopy( UnpackRGBA( color ), p->color );

		d = rand()&31;
		for( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j] + ((rand()&7)-4) + d*dir[j];
			p->vel[j] = RANDOM_FLOAT( -1.0f, 1.0f ) * 20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;
		p->alphaVel = -1.0 / (0.5 + RANDOM_FLOAT(0, 1) * 0.3);
		p->radius = 2;
		p->radiusVel = 0;
		p->length = 1;
		p->lengthVel = 0;
		p->rotation = 0;
	}
}

/*
===============
CL_TeleportSplash

===============
*/
void CL_TeleportSplash( vec3_t org )
{
	int		i, j, k;
	cparticle_t	*p;
	shader_t		teleShader;
	float		vel, color;
	vec3_t		dir;

	if( !cl_particles->integer )
		return;

	teleShader = re->RegisterShader( "particles/glow", SHADER_GENERIC );

	for( i = -16; i < 16; i += 4 )
	{
		for( j = -16; j < 16; j += 4 )
		{
			for( k = -24; k < 32; k += 4 )
			{
				p = CL_AllocParticle();
				if( !p ) return;
		
				VectorSet( dir, j*8, i*8, k*8 );
				VectorNormalizeFast( dir );

				vel = 50 + (rand() & 63);
				color = 0.1 + (0.2 * Com_RandomFloat( -1.0f, 1.0f ));
								
				p->shader = teleShader;
				p->time = cl.time;
				p->flags = 0;
				
				p->org[0] = org[0] + i + (rand() & 3);
				p->org[1] = org[1] + j + (rand() & 3);
				p->org[2] = org[2] + k + (rand() & 3);
				p->vel[0] = dir[0] * vel;
				p->vel[1] = dir[1] * vel;
				p->vel[2] = dir[2] * vel;
				p->accel[0] = 0;
				p->accel[1] = 0;
				p->accel[2] = -PARTICLE_GRAVITY;
				p->color[0] = color;
				p->color[1] = color;
				p->color[2] = color;
				p->colorVel[0] = 0;
				p->colorVel[1] = 0;
				p->colorVel[2] = 0;
				p->alpha = 1.0;
				p->alphaVel = -1.0 / (0.3 + (rand() & 7) * 0.02);
				p->radius = 2;
				p->radiusVel = 0;
				p->length = 1;
				p->lengthVel = 0;
				p->rotation = 0;
			}
		}
	}
}

/*
=================
CL_ExplosionParticles
=================
*/
void CL_ExplosionParticles( const vec3_t org )
{
	cparticle_t	*p;
	int		i, flags;
	shader_t		sparksShader;
	shader_t		smokeShader;

	if( !cl_particles->integer )
		return;

	// sparks
	flags = PARTICLE_STRETCH;
	flags |= PARTICLE_BOUNCE;
	flags |= PARTICLE_FRICTION;
	sparksShader = re->RegisterShader( "particles/sparks", SHADER_GENERIC );
	smokeShader = re->RegisterShader( "particles/smoke", SHADER_GENERIC );

	for( i = 0; i < 384; i++ )
	{
		p = CL_AllocParticle();
		if( !p ) return;

		p->shader = sparksShader;
		p->time = cl.time;
		p->flags = flags;

		p->org[0] = org[0] + ((rand() % 32) - 16);
		p->org[1] = org[1] + ((rand() % 32) - 16);
		p->org[2] = org[2] + ((rand() % 32) - 16);
		p->vel[0] = (rand() % 512) - 256;
		p->vel[1] = (rand() % 512) - 256;
		p->vel[2] = (rand() % 512) - 256;
		p->accel[0] = 0;
		p->accel[1] = 0;
		p->accel[2] = -60 + (30 * RANDOM_FLOAT( -1.0f, 1.0f ));
		p->color[0] = 1.0;
		p->color[1] = 1.0;
		p->color[2] = 1.0;
		p->colorVel[0] = 0;
		p->colorVel[1] = 0;
		p->colorVel[2] = 0;
		p->alpha = 1.0;
		p->alphaVel = -3.0;
		p->radius = 0.5 + (0.2 * RANDOM_FLOAT( -1.0f, 1.0f ));
		p->radiusVel = 0;
		p->length = 8 + (4 * RANDOM_FLOAT( -1.0f, 1.0f ));
		p->lengthVel = 8 + (4 * RANDOM_FLOAT( -1.0f, 1.0f ));
		p->rotation = 0;
		p->bounceFactor = 0.2;
		VectorCopy( p->org, p->org2 );
	}

	// Smoke
	flags = 0;
	flags |= PARTICLE_VERTEXLIGHT;

	for( i = 0; i < 5; i++ )
	{
		p = CL_AllocParticle();
		if( !p ) return;

		p->shader = smokeShader;
		p->time = cl.time;
		p->flags = flags;

		p->org[0] = org[0] + RANDOM_FLOAT( -1.0f, 1.0f ) * 10;
		p->org[1] = org[1] + RANDOM_FLOAT( -1.0f, 1.0f ) * 10;
		p->org[2] = org[2] + RANDOM_FLOAT( -1.0f, 1.0f ) * 10;
		p->vel[0] = RANDOM_FLOAT( -1.0f, 1.0f ) * 10;
		p->vel[1] = RANDOM_FLOAT( -1.0f, 1.0f ) * 10;
		p->vel[2] = RANDOM_FLOAT( -1.0f, 1.0f ) * 10 + (25 + RANDOM_FLOAT( -1.0f, 1.0f ) * 5);
		p->accel[0] = 0;
		p->accel[1] = 0;
		p->accel[2] = 0;
		p->color[0] = 0;
		p->color[1] = 0;
		p->color[2] = 0;
		p->colorVel[0] = 0.75f;
		p->colorVel[1] = 0.75f;
		p->colorVel[2] = 0.75f;
		p->alpha = 0.5f;
		p->alphaVel = -(0.1 + RANDOM_FLOAT( 0.0f, 1.0f ) * 0.1f);
		p->radius = 30 + (15 * RANDOM_FLOAT( -1.0f, 1.0f ));
		p->radiusVel = 15 + (7.5 * RANDOM_FLOAT( -1.0f, 1.0f ));
		p->length = 1;
		p->lengthVel = 0;
		p->rotation = rand() % 360;
	}
}

/*
==============
CL_ClearEffects

==============
*/
void CL_ClearEffects( void )
{
	CL_ClearParticles ();
	CL_ClearDlights ();
	CL_ClearLightStyles ();
}
