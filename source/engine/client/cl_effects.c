//=======================================================================
//			Copyright XashXT Group 2008 ©
//		cl_effects.c - entity effects parsing and management
//=======================================================================

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
void CL_ClearLightStyles( void )
{
	Mem_Set( cl_lightstyle, 0, sizeof( cl_lightstyle ));
	lastofs = -1;
}

/*
================
CL_RunLightStyles
================
*/
void CL_RunLightStyles( void )
{
	int		i, ofs;
	clightstyle_t	*ls;
	
	ofs = cl.time / 100;
	if( ofs == lastofs ) return;
	lastofs = ofs;

	for( i = 0, ls = cl_lightstyle; i < MAX_LIGHTSTYLES; i++, ls++ )
	{
		if( !ls->length )
		{
			VectorSet( ls->value, 1.0f, 1.0f, 1.0f );
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

	if( j >= MAX_STRING )
		Host_Error("CL_SetLightStyle: lightstyle %s is too long\n", s );

	cl_lightstyle[i].length = j;

	for( k = 0; k < j; k++ )
		cl_lightstyle[i].map[k] = (float)(s[k]-'a') / (float)('m'-'a');
}

/*
================
CL_AddLightStyles
================
*/
void CL_AddLightStyles( void )
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
typedef struct
{
	// these values common with dlight_t so don't move them
	vec3_t		origin;
	union
	{
		vec3_t	color;		// dlight color
		vec3_t	angles;		// spotlight angles
	};
	float		intensity;
	shader_t		texture;		// light image e.g. for flashlight
	vec2_t		cone;		// spotlight cone

	// cdlight_t private starts here
	int		key;		// so entities can reuse same entry
	int		start;		// stop lighting after this time
	int		end;		// drop this each second
	float		radius;		// radius (not an intensity)
	bool		fade;
	bool		free;		// this light is unused at current time
} cdlight_t;

cdlight_t		cl_dlights[MAX_DLIGHTS];

/*
================
CL_ClearDlights
================
*/
void CL_ClearDlights( void )
{
	Mem_Set( cl_dlights, 0, sizeof( cl_dlights ));
}

/*
===============
CL_AllocDlight

===============
*/
cdlight_t *CL_AllocDlight( int key )
{
	int		i, time, index;
	cdlight_t		*dl;

	// first look for an exact key match
	if( key )
	{
		for( i = 0, dl = cl_dlights; i < MAX_DLIGHTS; i++, dl++ )
		{
			if( dl->key == key )
			{
				// reuse this light
				Mem_Set( dl, 0, sizeof( *dl ));
				dl->key = key;
				return dl;
			}
		}
	}
	else
	{
		for( i = 0, dl = cl_dlights; i < MAX_DLIGHTS; i++, dl++ )
		{
			if( dl->free )
			{
				Mem_Set( dl, 0, sizeof( *dl ));
				return dl;
			}
		}
	}

	// find the oldest light
	time = cl.time;
	index = 0;

	for( i = 0, dl = cl_dlights; i < MAX_DLIGHTS; i++, dl++ )
	{
		if( dl->start < time )
		{
			time = dl->start;
			index = i;
		}
	}

	dl = &cl_dlights[index];
	Mem_Set( dl, 0, sizeof( *dl ));
	dl->key = key;

	return dl;
}

/*
===============
CL_AddDLight

===============
*/
void CL_AddDLight( const float *org, const float *rgb, float radius, float time, int flags, int key )
{
	cdlight_t		*dl;

	if( radius <= 0 )
	{
		MsgDev( D_WARN, "CL_AddDLight: ignore light with radius <= 0\n" );
		return;
	}

	dl = CL_AllocDlight( key );
	dl->free = false;
	dl->texture = -1;	// dlight

	VectorCopy( org, dl->origin );
	VectorCopy( rgb, dl->color );
	dl->radius = radius;
	dl->start = cl.time;
	dl->end = dl->start + (time * 1000);
	dl->fade = (flags & DLIGHT_FADE) ? true : false;
}

/*
===============
CL_AddDLights

===============
*/
void CL_AddDLights( void )
{
	cdlight_t	*dl;
	int	i;
	
	for( i = 0, dl = cl_dlights; i < MAX_DLIGHTS; i++, dl++ )
	{
		if( dl->free ) continue;
		if( cl.time >= dl->end )
		{
			dl->free = true;
			continue;
		}

		if( dl->fade )
		{
			dl->intensity = (float)(cl.time - dl->start) / (dl->end - dl->start);
			dl->intensity = dl->radius * (1.0 - dl->intensity);
		}
		else dl->intensity = dl->radius; // const

		re->AddDynLight( dl );
	}
}

/*
================
CL_TestLights

if cl_testlights is set, create 32 lights models
================
*/
void CL_TestLights( void )
{
	int		i, j;
	float		f, r;
	cdlight_t		dl;
	edict_t		*ed = CL_GetLocalPlayer();

	if( !cl_testlights->integer ) return;

	Mem_Set( &dl, 0, sizeof( cdlight_t ));
	
	for( i = 0; i < bound( 1, cl_testlights->integer, MAX_DLIGHTS ); i++ )
	{
		r = 64 * ( (i%4) - 1.5 );
		f = 64 * (i/4) + 128;

		for( j = 0; j < 3; j++ )
			dl.origin[j] = cl.refdef.vieworg[j] + cl.refdef.forward[j] * f + cl.refdef.right[j] * r;

		dl.color[0] = ((i%6)+1) & 1;
		dl.color[1] = (((i%6)+1) & 2)>>1;
		dl.color[2] = (((i%6)+1) & 4)>>2;
		dl.intensity = 200;
		dl.texture = -1;

		if( !re->AddDynLight( &dl ))
			break; 
	}
}

/*
==============================================================

PARTICLE MANAGEMENT

==============================================================
*/
struct particle_s
{
	// this part are shared both client and engine
	byte		color;	// colorIndex for R_GetPaletteColor
	vec3_t		org;
	vec3_t		vel;
	float		die;
	float		ramp;
	int		type;

	// this part are private for engine
	particle_t	*next;

	poly_t		poly;
	vec3_t		pVerts[4];
	vec2_t		pStcoords[4];
	rgba_t		pColor[4];
};

float cl_avertexnormals[NUMVERTEXNORMALS][3] =
{
#include "anorms.h"
};

// particle ramps
int ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
int ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
int ramp3[8] = { 0x6d, 0x6b, 6, 5, 4, 3 };

particle_t	*cl_active_particles, *cl_free_particles;
static vec3_t	cl_particle_avelocities[NUMVERTEXNORMALS];
static particle_t	cl_particle_list[MAX_PARTICLES];
static rgb_t	cl_particlePalette[256];

void CL_GetPaletteColor( int colorIndex, vec3_t outColor )
{
	if( !outColor ) return;
	colorIndex = bound( 0, colorIndex, 255 );
	VectorCopy( cl_particlePalette[colorIndex], outColor );
}

/*
=================
CL_AllocParticle
=================
*/
particle_t *CL_AllocParticle( void )
{
	particle_t	*p;

	if( !cl_free_particles )
		return NULL;

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
	particle_t	*p;
	rgbdata_t		*pic;
	byte		buf[1];	// fake stub

	pic = FS_LoadImage( "#quake.pal", buf, 768 );
		
	cl_active_particles = NULL;
	cl_free_particles = cl_particle_list;

	for( i = 0; i < MAX_PARTICLES; i++ )
		cl_particle_list[i].next = &cl_particle_list[i+1];

	cl_particle_list[MAX_PARTICLES-1].next = NULL;

	for( i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		cl_particle_avelocities[i][0] = (rand() & 255) * 0.01f;
		cl_particle_avelocities[i][1] = (rand() & 255) * 0.01f;
		cl_particle_avelocities[i][2] = (rand() & 255) * 0.01f;
	}

	for( i = 0, p = cl_particle_list; i < MAX_PARTICLES; i++, p++ )
	{
		p->pStcoords[0][0] = p->pStcoords[2][1] = p->pStcoords[1][0] = p->pStcoords[1][1] = 0;
		p->pStcoords[0][1] = p->pStcoords[2][0] = p->pStcoords[3][0] = p->pStcoords[3][1] = 1;
	}

	// Xash3D have built-in Quake1 palette - use it
	for( i = 0; pic && i < 256; i++ )
	{
		cl_particlePalette[i][0] = pic->palette[i*4+0];
		cl_particlePalette[i][1] = pic->palette[i*4+1];
		cl_particlePalette[i][2] = pic->palette[i*4+2];
	}

	if( pic ) FS_FreeImage( pic );
	else MsgDev( D_WARN, "SV_InitParticles: palette not installed\n" );
}

/*
===============
CL_AddParticles
===============
*/
void CL_AddParticles( void )
{
	particle_t	*p, *kill;
	float		time1, time2, time3;
	float		grav, dvel, frametime;
	vec3_t		up, right, point1;
	float		scale = 3.0f;	// FIXME: make cvar
	rgba_t		modulate;
	int		i;

	VectorScale( cl.refdef.right, scale, right );
	VectorScale( cl.refdef.up, scale, up );

	frametime = cls.frametime;
	time3 = frametime * 15;
	time2 = frametime * 10; // 15;
	time1 = frametime * 5;
	grav = frametime * clgame.movevars.gravity * 0.05f;
	dvel = frametime * 4;

	while( 1 ) 
	{
		// free time-expired particles
		kill = cl_active_particles;
		if( kill && kill->die < clgame.globals->time )
		{
			cl_active_particles = kill->next;
			kill->next = cl_free_particles;
			cl_free_particles = kill;
			continue;
		}
		break;
	}

	for( p = cl_active_particles; p; p = p->next )
	{
		while( 1 )
		{
			kill = p->next;
			if( kill && kill->die < clgame.globals->time )
			{
				p->next = kill->next;
				kill->next = cl_free_particles;
				cl_free_particles = kill;
				continue;
			}
			break;
		}

		scale = 0.0f;

		// hack a scale up to keep particles from disapearing
		scale += (p->org[0] - cl.refdef.vieworg[0]) * cl.refdef.forward[0];
		scale += (p->org[1] - cl.refdef.vieworg[1]) * cl.refdef.forward[1];
		scale += (p->org[2] - cl.refdef.vieworg[2]) * cl.refdef.forward[2];

		if( scale < 20.0f ) scale = 1.0f;
		else scale = 1.0f + scale * 0.004f;

		// setup color
		modulate[0] = cl_particlePalette[p->color][0];
		modulate[1] = cl_particlePalette[p->color][1];
		modulate[2] = cl_particlePalette[p->color][2];
		modulate[3] = 0xFF; // quake particles doesn't have transparency

		*(int *)p->pColor[0] = *(int *)modulate;
		*(int *)p->pColor[1] = *(int *)modulate;
		*(int *)p->pColor[2] = *(int *)modulate;
		*(int *)p->pColor[3] = *(int *)modulate;

		VectorMA( p->org, 1.0f, up, point1 );
		VectorMA( point1, 0.0f, right, p->pVerts[0] );

		VectorMA( p->org, 0.0f, up, point1 );
		VectorMA( point1, 0.0f, right, p->pVerts[1] );

		VectorMA( p->org, 0.0f, up, point1 );
		VectorMA( point1, 1.0f, right, p->pVerts[2] );

		VectorMA( p->org, 1.0f, up, point1 );
		VectorMA( point1, 1.0f, right, p->pVerts[3] );

		p->poly.numverts = 4;
		p->poly.verts = p->pVerts;
		p->poly.stcoords = p->pStcoords;
		p->poly.colors = p->pColor;
		p->poly.shadernum = cls.particle;
		p->poly.fognum = -1;

		// send the particle to the renderer
		re->AddPolygon( &p->poly );


		// evaluate particle
		p->org[0] += p->vel[0] * frametime;
		p->org[1] += p->vel[1] * frametime;
		p->org[2] += p->vel[2] * frametime;
		
		switch( p->type )
		{
		case pt_static:
			break;
		case pt_fire:
			p->ramp += time1;
			if( p->ramp >= 6 )
				p->die = -1;
			else p->color = ramp3[(int)p->ramp];
			p->vel[2] += grav;
			break;
		case pt_explode:
			p->ramp += time2;
			if( p->ramp >=8 )
				p->die = -1;
			else p->color = ramp1[(int)p->ramp];
			for( i = 0; i < 3; i++ )
				p->vel[i] += p->vel[i] * dvel;
			p->vel[2] -= grav;
			break;
		case pt_explode2:
			p->ramp += time3;
			if( p->ramp >=8 )
				p->die = -1;
			else p->color = ramp2[(int)p->ramp];
			for( i = 0; i < 3; i++ )
				p->vel[i] -= p->vel[i] * frametime;
			p->vel[2] -= grav;
			break;
		case pt_blob:
			for( i = 0; i < 3; i++ )
				p->vel[i] += p->vel[i] * dvel;
			p->vel[2] -= grav;
			break;
		case pt_blob2:
			for( i = 0; i < 2; i++ )
				p->vel[i] -= p->vel[i] * dvel;
			p->vel[2] -= grav;
			break;
		case pt_grav:
			p->vel[2] -= grav * 20;
			break;
		case pt_slowgrav:
			p->vel[2] -= grav;
			break;
		}
	}
}

/*
===============
CL_ParticleEffect

old good quake1 particles
===============
*/
void CL_ParticleEffect( const vec3_t org, const vec3_t dir, int color, int count )
{
	int		i, j;
	particle_t	*p;
	
	for( i = 0; i < count; i++ )
	{
		p = CL_AllocParticle();
		if( !p ) return;

		if( count == 1024 )
		{	
			// rocket explosion
			p->die = clgame.globals->time + 5.0f;
			p->color = ramp1[0];
			p->ramp = rand() & 3;
			if( i & 1 )
			{
				p->type = pt_explode;
				for( j = 0; j < 3; j++ )
				{
					p->org[j] = org[j] + ((rand() % 32) - 16);
					p->vel[j] = (rand() % 512) - 256;
				}
			}
			else
			{
				p->type = pt_explode2;
				for( j = 0; j < 3; j++ )
				{
					p->org[j] = org[j] + ((rand() % 32) - 16);
					p->vel[j] = (rand() % 512) - 256;
				}
			}
		}
		else
		{
			p->die = clgame.globals->time + 0.1f * (rand() % 5);
			p->color = (color & ~7) + (rand() & 7);
			p->type = pt_slowgrav;

			for( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + ((rand() & 15 ) - 8);
				p->vel[j] = dir[j] * 15;// + (rand() % 300) - 150;
			}
		}
	}
}

/*
===============
CL_EntityParticles

EF_BRIGHTFIELD effect
===============
*/
void CL_EntityParticles( edict_t *ent )
{
	int		i, count;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	float		dist, beamlength = 16;
	vec3_t		forward;	
	particle_t	*p;

	dist = 64;
	count = 50;

	for( i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		angle = clgame.globals->time * cl_particle_avelocities[i][0];
		com.sincos( angle, &sy, &cy );
		angle = clgame.globals->time * cl_particle_avelocities[i][1];
		com.sincos( angle, &sp, &cp );
		angle = clgame.globals->time * cl_particle_avelocities[i][2];
		com.sincos( angle, &sr, &cr );
	
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		p = CL_AllocParticle();
		if( !p ) return;

		p->die = clgame.globals->time + 0.01f;
		p->color = 0x6f;
		p->type = pt_explode;
		
		p->org[0] = ent->v.origin[0] + cl_avertexnormals[i][0] * dist + forward[0] * beamlength;
		p->org[1] = ent->v.origin[1] + cl_avertexnormals[i][1] * dist + forward[1] * beamlength;
		p->org[2] = ent->v.origin[2] + cl_avertexnormals[i][2] * dist + forward[2] * beamlength;
	}
}

/*
===============
CL_ParticleExplosion

===============
*/
void CL_ParticleExplosion( const vec3_t org )
{
	int		i, j;
	particle_t	*p;
	
	for( i = 0; i < 1024; i++ )
	{
		p = CL_AllocParticle();
		if( !p ) return;

		p->die = clgame.globals->time + 5.0f;
		p->color = ramp1[0];
		p->ramp = rand() & 3;

		if( i & 1 )
		{
			p->type = pt_explode;
			for( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
		else
		{
			p->type = pt_explode2;
			for( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
	}
}

/*
===============
CL_ParticleExplosion2

===============
*/
void CL_ParticleExplosion2( const vec3_t org, int colorStart, int colorLength)
{
	int		i, j;
	int		colorMod = 0;
	particle_t	*p;

	for( i = 0; i < 512; i++ )
	{
		p = CL_AllocParticle();
		if( !p ) return;

		p->die = clgame.globals->time + 0.3f;
		p->color = colorStart + (colorMod % colorLength);
		colorMod++;

		p->type = pt_blob;
		for( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j] + ((rand() % 32) - 16);
			p->vel[j] = (rand() % 512) - 256;
		}
	}
}

/*
===============
CL_BlobExplosion

===============
*/
void CL_BlobExplosion( const vec3_t org )
{
	int		i, j;
	particle_t	*p;
	
	for( i = 0; i < 1024; i++ )
	{
		p = CL_AllocParticle();
		if( !p ) return;

		p->die = clgame.globals->time + 1.0f + (rand() & 8) * 0.05f;

		if( i & 1 )
		{
			p->type = pt_blob;
			p->color = 66 + rand() % 6;
			for( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
		else
		{
			p->type = pt_blob2;
			p->color = 150 + rand() % 6;
			for( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
	}
}

/*
===============
CL_LavaSplash

===============
*/
void CL_LavaSplash( const vec3_t org )
{
	int		i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;

	for( i = -16; i < 16; i++ )
	{
		for( j = -16; j <16; j++ )
		{
			for( k = 0; k < 1; k++ )
			{
				p = CL_AllocParticle();
				if( !p ) return;
		
				p->die = clgame.globals->time + 2.0f + (rand() & 31) * 0.02f;
				p->color = 224 + (rand() & 7);
				p->type = pt_slowgrav;
				
				dir[0] = j * 8 + (rand() & 7);
				dir[1] = i * 8 + (rand() & 7);
				dir[2] = 256;
	
				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand() & 63);
	
				VectorNormalize( dir );
				vel = 50 + (rand() & 63);
				VectorScale( dir, vel, p->vel );
			}
		}
	}
}

/*
===============
CL_TeleportSplash

===============
*/
void CL_TeleportSplash( const vec3_t org )
{
	int		i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;

	for( i = -16; i < 16; i+=4 )
	{
		for( j =-16; j < 16; j += 4 )
		{
			for( k = -24; k < 32; k += 4 )
			{
				p = CL_AllocParticle();
				if( !p ) return;
		
				p->die = clgame.globals->time + 0.2f + (rand() & 7) * 0.02f;
				p->color = 7 + (rand() & 7);
				p->type = pt_slowgrav;
				
				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;
	
				p->org[0] = org[0] + i + (rand() & 3);
				p->org[1] = org[1] + j + (rand() & 3);
				p->org[2] = org[2] + k + (rand() & 3);
	
				VectorNormalize( dir );
				vel = 50.0f + (rand() & 63);
				VectorScale( dir, vel, p->vel );
			}
		}
	}
}

/*
===============
CL_RocketTrail

===============
*/
void CL_RocketTrail( const vec3_t org, const vec3_t end, int type )
{
	vec3_t		vec, start;
	float		len;
	particle_t	*p;
	int		j, dec;
	static int	tracercount;

	VectorCopy( org, start );
	VectorSubtract( end, start, vec );
	len = VectorNormalizeLength( vec );

	if( type < 128 )
	{
		dec = 3;
	}
	else
	{
		dec = 1;
		type -= 128;
	}

	while( len > 0 )
	{
		len -= dec;

		p = CL_AllocParticle();
		if( !p ) return;
		
		VectorClear( p->vel );
		p->die = clgame.globals->time + 2.0f;

		switch( type )
		{
		case 0:	// rocket trail
			p->ramp = (rand() & 3);
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			for( j = 0; j < 3; j++ )
				p->org[j] = start[j] + ((rand()%6)-3);
			break;
		case 1:	// smoke smoke
			p->ramp = (rand() & 3) + 2;
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			for( j = 0; j < 3; j++ )
				p->org[j] = start[j] + ((rand() % 6) - 3);
			break;
		case 2:	// blood
			p->type = pt_grav;
			p->color = 67 + (rand()&3);
			for( j = 0; j < 3; j++ )
				p->org[j] = start[j] + ((rand() % 6) - 3);
			break;
		case 3:
		case 5:	// tracer
			p->die = clgame.globals->time + 0.5f;
			p->type = pt_static;
			if( type == 3 ) p->color = 52 + ((tracercount & 4)<<1);
			else p->color = 230 + ((tracercount & 4)<<1);
			tracercount++;
			VectorCopy( start, p->org );
			if( tracercount & 1 )
			{
				p->vel[0] = 30 *  vec[1];
				p->vel[1] = 30 * -vec[0];
			}
			else
			{
				p->vel[0] = 30 * -vec[1];
				p->vel[1] = 30 *  vec[0];
			}
			break;
		case 4:	// slight blood
			p->type = pt_grav;
			p->color = 67 + (rand() & 3);
			for( j = 0; j < 3; j++ )
				p->org[j] = start[j] + ((rand() % 6) - 3);
			len -= 3;
			break;
		case 6:	// voor trail
			p->color = 9 * 16 + 8 + (rand() & 3);
			p->type = pt_static;
			p->die = clgame.globals->time + 0.3f;
			for( j = 0; j < 3; j++ )
				p->org[j] = start[j] + ((rand() & 15) - 8);
			break;
		}
		VectorAdd( start, vec, start );
	}
}

/*
==============================================================

DECALS MANAGEMENT

==============================================================
*/
#define MAX_DRAWDECALS	1024
#define MAX_DECAL_VERTS	128	// per one decal
#define MAX_DECAL_FRAGMENTS	64

typedef struct cdecal_s
{
	struct cdecal_s	*prev;
	struct cdecal_s	*next;

	edict_t		*pent;	// parent entity
	poly_t		*poly;
} cdecal_t;

static cdecal_t	cl_decals[MAX_DRAWDECALS];
static cdecal_t	cl_decals_headnode, *cl_free_decals;
static poly_t	cl_decal_polys[MAX_DRAWDECALS];
static vec3_t	cl_decal_verts[MAX_DRAWDECALS][MAX_DECAL_VERTS];
static vec2_t	cl_decal_stcoords[MAX_DRAWDECALS][MAX_DECAL_VERTS];
static rgba_t	cl_decal_colors[MAX_DRAWDECALS][MAX_DECAL_VERTS];

/*
=================
CL_ClearDecals
=================
*/
void CL_ClearDecals( void )
{
	int	i;

	Mem_Set( cl_decals, 0, sizeof( cl_decals ));
	Mem_Set( cl_decal_polys, 0, sizeof( cl_decal_polys ));

	// link decals
	cl_free_decals = cl_decals;
	cl_decals_headnode.prev = &cl_decals_headnode;
	cl_decals_headnode.next = &cl_decals_headnode;

	for( i = 0; i < MAX_DRAWDECALS; i++ )
	{
		if( i < MAX_DRAWDECALS - 1 )
			cl_decals[i].next = &cl_decals[i+1];

		cl_decals[i].poly = &cl_decal_polys[i];
		cl_decals[i].poly->verts = cl_decal_verts[i];
		cl_decals[i].poly->stcoords = cl_decal_stcoords[i];
		cl_decals[i].poly->colors = cl_decal_colors[i];
	}
}

/*
=================
CL_AllocDecal

Returns either a free decal or the oldest one
=================
*/
cdecal_t *CL_AllocDecal( void )
{
	cdecal_t	*dl;

	if( cl_free_decals )
	{	
		// take a free decal if possible
		dl = cl_free_decals;
		cl_free_decals = dl->next;
	}
	else
	{	
		// grab the oldest one otherwise
		dl = cl_decals_headnode.prev;
		dl->prev->next = dl->next;
		dl->next->prev = dl->prev;
	}

	// put the decal at the start of the list
	dl->prev = &cl_decals_headnode;
	dl->next = cl_decals_headnode.next;
	dl->next->prev = dl;
	dl->prev->next = dl;

	return dl;
}

/*
=================
CL_FreeDecal
=================
*/
void CL_FreeDecal( cdecal_t *dl )
{
	// remove from linked active list
	dl->prev->next = dl->next;
	dl->next->prev = dl->prev;

	// insert into linked free list
	dl->next = cl_free_decals;
	cl_free_decals = dl;
}

/*
=================
CL_SpawnDecal
=================
*/
int CL_SpawnDecal( HSPRITE m_hDecal, edict_t *pEnt, const vec3_t pos, int colorIndex, float roll, float scale )
{
	cdecal_t		*dl;
	poly_t		*poly;
	vec3_t		axis[3];
	int		i, j, numfragments;
	vec3_t		dir, verts[MAX_DECAL_VERTS];
	fragment_t	*fr, fragments[MAX_DECAL_FRAGMENTS];
	float		radius = 32.0f; // search radius
	rgba_t		color;

	// invalid decal
	if( scale <= 0.0f || !m_hDecal )
		return false;

	CL_FindExplosionPlane( pos, scale, dir );

	// calculate orientation matrix
	VectorNormalize2( dir, axis[0] );
	PerpendicularVector( axis[1], axis[0] );
	RotatePointAroundVector( axis[2], axis[0], axis[1], roll );
	CrossProduct( axis[0], axis[2], axis[1] );

	numfragments = re->GetFragments( pos, scale, axis, MAX_DECAL_VERTS, verts, MAX_DECAL_FRAGMENTS, fragments );

	// no valid fragments
	if( !numfragments ) return false;

	// setup decal color
	if( colorIndex )
	{
		colorIndex = bound( 0, colorIndex, 255 );
		color[0] = cl_particlePalette[colorIndex][0];
		color[1] = cl_particlePalette[colorIndex][1];
		color[2] = cl_particlePalette[colorIndex][2];
		color[3] = 0xFF;
	}
	else Vector4Set( color, 255, 255, 255, 255 );

	scale = 0.5f / scale;
	VectorScale( axis[1], scale, axis[1] );
	VectorScale( axis[2], scale, axis[2] );

	for( i = 0, fr = fragments; i < numfragments; i++, fr++ )
	{
		if( fr->numverts >= MAX_DECAL_VERTS )
			return -1; // in case we partially failed
		else if( fr->numverts <= 0 )
			continue;

		// allocate decal
		dl = CL_AllocDecal();
		dl->pent = pEnt;

		// setup polygon for drawing
		poly = dl->poly;
		poly->shadernum = m_hDecal;
		poly->numverts = fr->numverts;
		poly->fognum = fr->fognum;

		for( j = 0; j < fr->numverts; j++ )
		{
			vec3_t	v;

			VectorCopy( verts[fr->firstvert+j], poly->verts[j] );
			VectorSubtract( poly->verts[j], pos, v );
			poly->stcoords[j][0] = DotProduct( v, axis[1] ) + 0.5f;
			poly->stcoords[j][1] = DotProduct( v, axis[2] ) + 0.5f;
			*(int *)poly->colors[j] = *(int *)color;
		}
	}
	return true; // all done
}

/*
=================
CL_AddDecals
=================
*/
void CL_AddDecals( void )
{
	cdecal_t	*dl, *next, *hnode;

	// add decals in first-spawned - first-drawn order
	hnode = &cl_decals_headnode;
	for( dl = hnode->prev; dl != hnode; dl = next )
	{
		next = dl->prev;

		if( dl->pent && dl->pent->free )
		{
			// remove attached decals
			CL_FreeDecal( dl );
			continue;
		}
		re->AddPolygon( dl->poly );
	}
}

/*
===============
CL_SpawnStaticDecal

===============
*/
void CL_SpawnStaticDecal( vec3_t origin, int decalIndex, int entityIndex, int modelIndex )
{
	edict_t	*pEnt;

	pEnt = CL_GetEdictByIndex( entityIndex );
	decalIndex = bound( 0, decalIndex, MAX_DECALS - 1 );

	CL_SpawnDecal( cl.decal_shaders[decalIndex], pEnt, origin, 0, 90.0f, 32.0f );
}

void CL_FindExplosionPlane( const vec3_t origin, float radius, vec3_t result )
{
	static vec3_t	planes[6] = {{0, 0, 1}, {0, 1, 0}, {1, 0, 0}, {0, 0, -1}, {0, -1, 0}, {-1, 0, 0}};
	float		best = 1.0f;
	vec3_t		point, dir;
	trace_t		trace;
	int		i;

	if( !result ) return;
	VectorClear( dir );

	for( i = 0; i < 6; i++ )
	{
		VectorMA( origin, radius, planes[i], point );

		trace = CL_Move( origin, vec3_origin, vec3_origin, point, MOVE_NOMONSTERS, NULL );
		if( trace.fAllSolid || trace.flFraction == 1.0f )
			continue;

		if( trace.flFraction < best )
		{
			best = trace.flFraction;
			VectorCopy( trace.vecPlaneNormal, dir );
		}
	}
	VectorCopy( dir, result );
}

void CL_LightForPoint( const vec3_t point, vec3_t ambientLight )
{
	if( re ) re->LightForPoint( point, ambientLight );
	else VectorClear( ambientLight );
}

/*
===============
CL_ResetEvent

===============
*/
void CL_ResetEvent( event_info_t *ei )
{
	Mem_Set( ei, 0, sizeof( *ei ));
}

word CL_PrecacheEvent( const char *name )
{
	int	i;
	
	if( !name || !name[0] ) return 0;

	for( i = 1; i < MAX_EVENTS && cl.configstrings[CS_EVENTS+i][0]; i++ )
		if( !com.strcmp( cl.configstrings[CS_EVENTS+i], name ))
			return i;
	return 0;
}

bool CL_FireEvent( event_info_t *ei )
{
	user_event_t	*ev;
	const char	*name;
	int		i;

	if( !ei || !ei->index )
		return false;

	// get the func pointer
	for( i = 0; i < MAX_EVENTS; i++ )
	{
		ev = clgame.events[i];		
		if( !ev ) break;

		if( ev->index == ei->index )
		{
			if( ev->func )
			{
				ev->func( &ei->args );
				return true;
			}

			name = cl.configstrings[CS_EVENTS+ei->index];
			MsgDev( D_ERROR, "CL_FireEvent: %s not hooked\n", name );
			break;			
		}
	}
	return false;
}

void CL_FireEvents( void )
{
	int		i;
	event_state_t	*es;
	event_info_t	*ei;
	bool		success;

	es = &cl.events;

	for( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &es->ei[i];

		if( ei->index == 0 )
			continue;

		if( cls.state == ca_disconnected )
		{
			CL_ResetEvent( ei );
			continue;
		}

		// delayed event!
		if( ei->fire_time && ( ei->fire_time > clgame.globals->time ))
			continue;

		success = CL_FireEvent( ei );

		// zero out the remaining fields
		CL_ResetEvent( ei );
	}
}

event_info_t *CL_FindEmptyEvent( void )
{
	int		i;
	event_state_t	*es;
	event_info_t	*ei;

	es = &cl.events;

	// look for first slot where index is != 0
	for( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &es->ei[i];
		if( ei->index != 0 )
			continue;
		return ei;
	}

	// no slots available
	return NULL;
}

event_info_t *CL_FindUnreliableEvent( void )
{
	int		i;
	event_state_t	*es;
	event_info_t	*ei;

	es = &cl.events;
	for ( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &es->ei[i];
		if( ei->index != 0 )
		{
			// it's reliable, so skip it
			if( ei->flags & FEV_RELIABLE )
				continue;
		}
		return ei;
	}

	// this should never happen
	return NULL;
}

void CL_QueueEvent( int flags, int index, float delay, event_args_t *args )
{
	bool		unreliable = (flags & FEV_RELIABLE) ? false : true;
	event_info_t	*ei;

	// find a normal slot
	ei = CL_FindEmptyEvent();
	if( !ei && unreliable )
	{
		return;
	}

	// okay, so find any old unreliable slot
	if( !ei )
	{
		ei = CL_FindUnreliableEvent();
		if( !ei ) return;
	}

	ei->index	= index;
	ei->fire_time = delay ? (clgame.globals->time + delay) : 0.0f;
	ei->flags	= flags;
	
	// copy in args event data
	Mem_Copy( &ei->args, args, sizeof( ei->args ));
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
	CL_ClearDecals ();
}
