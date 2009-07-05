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
	
	ofs = (int)(cl.time * 10);
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
	if( j >= MAX_STRING ) Host_Error("CL_SetLightStyle: lightstyle %s is too long\n", s );

	cl_lightstyle[i].length = j;

	for( k = 0; k < j; k++ )
		cl_lightstyle[i].map[k] = (float)(s[k]-'a') / (float)('m'-'a');
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
	int		i;
	cdlight_t		*dl;

	// first look for an exact key match
	if( key )
	{
		dl = cl_dlights;
		for( i = 0; i < MAX_DLIGHTS; i++, dl++ )
		{
			if( dl->key == key )
			{
				Mem_Set( dl, 0, sizeof( *dl ));
				dl->key = key;
				return dl;
			}
		}
	}

	// then look for anything else
	dl = cl_dlights;
	for( i = 0; i < MAX_DLIGHTS; i++, dl++ )
	{
		if( dl->die < cl.time )
		{
			Mem_Set( dl, 0, sizeof( *dl ));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	Mem_Set( dl, 0, sizeof( *dl ));
	dl->key = key;
	return dl;
}

/*
===============
pfnAddDLight

===============
*/
void pfnAddDLight( const float *org, const float *rgb, float radius, float decay, float time, int key )
{
	cdlight_t		*dl;

	if( radius <= 0 )
	{
		MsgDev( D_WARN, "CL_AddDLight: ignore light with radius <= 0\n" );
		return;
	}

	dl = CL_AllocDlight( key );
	if( !dl )
	{
		MsgDev( D_ERROR, "CL_AddDLight: no free dlights\n" );
		return;
	}

	VectorCopy( org, dl->origin );
	VectorCopy( rgb, dl->color );
	dl->die = cl.time + time;
	dl->decay = decay;
	dl->radius = radius;
}


/*
===============
CL_RunDLights

===============
*/
void CL_RunDLights( void )
{
	cdlight_t	*dl;
	int	i;

	for( i = 0, dl = cl_dlights; i < MAX_DLIGHTS; i++, dl++ )
	{
		if( !dl->radius ) continue;
		if( dl->die < cl.time )
		{
			dl->radius = 0;
			return;
		}

		dl->radius -= cls.frametime * dl->decay;
		if( dl->radius < 0 ) dl->radius = 0;
	}
}

/*
===============
CL_AddDLights

===============
*/
void CL_AddDLights( void )
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

DECALS MANAGEMENT

==============================================================
*/
#define MAX_DECAL_MARKS		2048
#define DECAL_FADETIME		30.0f	// 30 seconds
#define DECAL_STAYTIME		120.0f	// 120 seconds

typedef struct cdecal_s
{
	struct cdecal_s	*prev, *next;
	float		time;
	rgba_t		modulate;
	bool		alphaFade;
	shader_t		shader;
	int		numVerts;
	polyVert_t	verts[MAX_VERTS_ON_POLY];
	vec3_t		origin;
} cdecal_t;

static cdecal_t	cl_activeDecals;
static cdecal_t	*cl_freeDecals;
static cdecal_t	cl_decalList[MAX_DECAL_MARKS];

/*
=================
CL_FreeDecal
=================
*/
static void CL_FreeDecal( cdecal_t *decal )
{
	if( !decal->prev ) return;

	decal->prev->next = decal->next;
	decal->next->prev = decal->prev;

	decal->next = cl_freeDecals;
	cl_freeDecals = decal;
}

/*
=================
CL_AllocDecal

will always succeed, even if it requires freeing an old active mark
=================
*/
static cdecal_t *CL_AllocDecal( void )
{
	cdecal_t	*decal;

	if( !cl_freeDecals )
		CL_FreeDecal( cl_activeDecals.prev );

	decal = cl_freeDecals;
	cl_freeDecals = cl_freeDecals->next;

	Mem_Set( decal, 0, sizeof( cdecal_t ));

	decal->next = cl_activeDecals.next;
	decal->prev = &cl_activeDecals;
	cl_activeDecals.next->prev = decal;
	cl_activeDecals.next = decal;

	return decal;
}

/*
=================
CL_ClearDecals
=================
*/
void CL_ClearDecals( void )
{
	int	i;

	Mem_Set( cl_decalList, 0, sizeof( cl_decalList ));

	cl_activeDecals.next = &cl_activeDecals;
	cl_activeDecals.prev = &cl_activeDecals;
	cl_freeDecals = cl_decalList;

	for( i = 0; i < MAX_DECAL_MARKS - 1; i++ )
		cl_decalList[i].next = &cl_decalList[i+1];
}

/*
=================
CL_AddDecal

called from render after clipping
=================
*/
void CL_AddDecal( vec3_t org, matrix3x3 m, shader_t s, rgba_t rgba, bool fade, decalFragment_t *df, const vec3_t *v )
{
	cdecal_t	*decal;
	vec3_t	delta;
	int	i;

	decal = CL_AllocDecal();
	VectorCopy( org, decal->origin );
	decal->time = cl.time;
	Vector4Copy( rgba, decal->modulate );
	decal->alphaFade = fade;
	decal->shader = s;
	decal->numVerts = df->numVerts;

	for( i = 0; i < df->numVerts; i++ )
	{
		VectorCopy( v[df->firstVert + i], decal->verts[i].point );

		VectorSubtract( decal->verts[i].point, org, delta );
		decal->verts[i].st[0] = 0.5 + DotProduct( delta, m[1] );
		decal->verts[i].st[1] = 0.5 + DotProduct( delta, m[2] );
		Vector4Copy( rgba, decal->verts[i].modulate );
	}
}

/*
=================
CL_AddDecals
=================
*/
void CL_AddDecals( void )
{
	cdecal_t	*decal, *next;
	float	c, time, fadeTime;
	int	i;

	fadeTime = DECAL_FADETIME;	// 30 seconds

	for( decal = cl_activeDecals.next; decal != &cl_activeDecals; decal = next )
	{
		// crab next now, so if the decal is freed we still have it
		next = decal->next;

		if( cl.time >= decal->time + DECAL_STAYTIME )
		{
			CL_FreeDecal( decal );
			continue;
		}

		// HACKHACK: fade out glowing energy decals
		if( decal->shader == re->RegisterShader( "particles/energy", SHADER_GENERIC ))
		{
			time = cl.time - decal->time;

			if( time < fadeTime )
			{
				c = 255 - (time / fadeTime);

				for( i = 0; i < decal->numVerts; i++ )
				{
					decal->verts[i].modulate[0] = decal->modulate[0] * c;
					decal->verts[i].modulate[1] = decal->modulate[1] * c;
					decal->verts[i].modulate[2] = decal->modulate[2] * c;
				}
			}
		}

		// fade out with time
		time = decal->time + DECAL_STAYTIME - cl.time;

		if( time < fadeTime )
		{
			c = time / fadeTime;

			if( decal->alphaFade )
			{
				for( i = 0; i < decal->numVerts; i++ )
					decal->verts[i].modulate[3] = decal->modulate[3] * c;
			}
			else
			{
				for( i = 0; i < decal->numVerts; i++ )
				{
					decal->verts[i].modulate[0] = decal->modulate[0] * c;
					decal->verts[i].modulate[1] = decal->modulate[1] * c;
					decal->verts[i].modulate[2] = decal->modulate[2] * c;
				}
			}
		}
		re->AddPolygon( decal->shader, decal->numVerts, decal->verts );
	}
}

/*
===============
pfnAddDecal

===============
*/
void pfnAddDecal( float *org, float *dir, float *rgba, float rot, float rad, HSPRITE hSpr, int flags )
{
	bool	fade, temp;
	rgba_t	color;

	fade = (flags & DECAL_FADE) ? true : false;
	temp = (flags & DECAL_TEMPORARY) ? true : false;

	color[0] = 255 * rgba[0];
	color[1] = 255 * rgba[1];
	color[2] = 255 * rgba[2];
	color[3] = 255 * rgba[3];

	re->ImpactMark( org, dir, rot, rad, color, fade, hSpr, temp );	
}

/*
==============================================================

PARTICLE MANAGEMENT

==============================================================
*/
struct cparticle_s
{
	// this part are shared both client and engine
	vec3_t		origin;
	vec3_t		velocity;
	vec3_t		accel;
	vec3_t		color;
	vec3_t		colorVelocity;
	float		alpha;
	float		alphaVelocity;
	float		radius;
	float		radiusVelocity;
	float		length;
	float		lengthVelocity;
	float		rotation;
	float		bounceFactor;

	// this part are private for engine
	cparticle_t	*next;
	shader_t		shader;

	float		time;
	int		flags;

	vec3_t		oldorigin;
};

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
	rgba_t		modulate;
	vec3_t		origin, oldorigin, velocity, color;
	vec3_t		ambientLight;
	float		alpha, radius, length;
	float		time, time2, gravity, dot;
	vec3_t		mins, maxs;
	int		contents;
	trace_t		trace;

	if( !cl_particles->integer ) return;

	if( EDICT_NUM( cl.frame.ps.number )->pvClientData->current.gravity != 0 )
		gravity = EDICT_NUM( cl.frame.ps.number )->pvClientData->current.gravity / clgame.movevars.gravity;
	else gravity = 1.0f;

	for( p = cl_active_particles; p; p = next )
	{
		// grab next now, so if the particle is freed we still have it
		next = p->next;

		time = cl.time - p->time;
		time2 = time * time;

		alpha = p->alpha + p->alphaVelocity * time;
		radius = p->radius + p->radiusVelocity * time;
		length = p->length + p->lengthVelocity * time;

		if( alpha <= 0 || radius <= 0 || length <= 0 )
		{
			// faded out
			CL_FreeParticle( p );
			continue;
		}

		color[0] = p->color[0] + p->colorVelocity[0] * time;
		color[1] = p->color[1] + p->colorVelocity[1] * time;
		color[2] = p->color[2] + p->colorVelocity[2] * time;

		origin[0] = p->origin[0] + p->velocity[0] * time + p->accel[0] * time2;
		origin[1] = p->origin[1] + p->velocity[1] * time + p->accel[1] * time2;
		origin[2] = p->origin[2] + p->velocity[2] * time + p->accel[2] * time2 * gravity;

		if( p->flags & PARTICLE_UNDERWATER )
		{
			// underwater particle
			VectorSet( oldorigin, origin[0], origin[1], origin[2] + radius );

			if(!(CL_PointContents( oldorigin ) & MASK_WATER ))
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
			contents = CL_PointContents( origin );
			if( contents & MASK_WATER )
			{
				// add friction
				if( contents & CONTENTS_WATER )
				{
					VectorScale( p->velocity, 0.25, p->velocity );
					VectorScale( p->accel, 0.25, p->accel );
				}
				if( contents & CONTENTS_SLIME )
				{
					VectorScale( p->velocity, 0.20, p->velocity );
					VectorScale( p->accel, 0.20, p->accel );
				}
				if( contents & CONTENTS_LAVA )
				{
					VectorScale( p->velocity, 0.10, p->velocity );
					VectorScale( p->accel, 0.10, p->accel );
				}

				// don't add friction again
				p->flags &= ~PARTICLE_FRICTION;
				length = 1;
				
				// reset
				p->time = cl.time;
				VectorCopy( origin, p->origin );
				VectorCopy( color, p->color );
				p->alpha = alpha;
				p->radius = radius;

				// don't stretch
				p->flags &= ~PARTICLE_STRETCH;
				p->length = length;
				p->lengthVelocity = 0;
			}
		}

		if( p->flags & PARTICLE_BOUNCE )
		{
			edict_t	*clent = EDICT_NUM( cl.frame.ps.number );

			// bouncy particle
			VectorSet(mins, -radius, -radius, -radius);
			VectorSet(maxs, radius, radius, radius);

			trace = CL_Trace( p->oldorigin, mins, maxs, origin, MOVE_NORMAL, clent, MASK_SOLID );
			if( trace.fraction != 0.0 && trace.fraction != 1.0 )
			{
				// reflect velocity
				time = cl.time - (cls.frametime + cls.frametime * trace.fraction);
				time = time - p->time;

				velocity[0] = p->velocity[0];
				velocity[1] = p->velocity[1];
				velocity[2] = p->velocity[2] + p->accel[2] * gravity * time;
				VectorReflect( velocity, 0, trace.plane.normal, p->velocity );
				VectorScale( p->velocity, p->bounceFactor, p->velocity );

				// check for stop or slide along the plane
				if( trace.plane.normal[2] > 0 && p->velocity[2] < 1 )
				{
					if( trace.plane.normal[2] == 1 )
					{
						VectorClear( p->velocity );
						VectorClear( p->accel );
						p->flags &= ~PARTICLE_BOUNCE;
					}
					else
					{
						// FIXME: check for new plane or free fall
						dot = DotProduct( p->velocity, trace.plane.normal );
						VectorMA( p->velocity, -dot, trace.plane.normal, p->velocity );

						dot = DotProduct( p->accel, trace.plane.normal );
						VectorMA( p->accel, -dot, trace.plane.normal, p->accel );
					}
				}

				VectorCopy( trace.endpos, origin );
				length = 1;

				// reset
				p->time = cl.time;
				VectorCopy( origin, p->origin );
				VectorCopy( color, p->color );
				p->alpha = alpha;
				p->radius = radius;

				// don't stretch
				p->flags &= ~PARTICLE_STRETCH;
				p->length = length;
				p->lengthVelocity = 0;
			}
		}

		// save current origin if needed
		if( p->flags & (PARTICLE_BOUNCE|PARTICLE_STRETCH))
		{
			VectorCopy( p->oldorigin, oldorigin );
			VectorCopy( origin, p->oldorigin ); // FIXME: pause
		}

		if( p->flags & PARTICLE_VERTEXLIGHT )
		{
			// vertex lit particle
			re->LightForPoint( origin, ambientLight );
			VectorMultiply( color, ambientLight, color );
		}

		// bound color and alpha and convert to byte
		modulate[0] = bound( 0, 255 * color[0], 255 );
		modulate[1] = bound( 0, 255 * color[1], 255 );
		modulate[2] = bound( 0, 255 * color[2], 255 );
		modulate[3] = bound( 0, 255 * alpha, 255 );

		if( p->flags & PARTICLE_INSTANT )
		{
			// instant particle
			p->alpha = 0;
			p->alphaVelocity = 0;
		}

		// send the particle to the renderer
		re->AddParticle( p->shader, origin, oldorigin, radius, length, p->rotation, modulate );
	}

	cl_active_particles = active;
}

/*
===============
pfnAddParticle

===============
*/
bool pfnAddParticle( cparticle_t *src, HSPRITE shader, int flags )
{
	cparticle_t	*p;

	if( !src ) return false;

	p = CL_AllocParticle();
	if( !p )
	{
		MsgDev( D_ERROR, "CL_AddParticle: no free particles\n" );
		return false;
	}

	p->shader = shader;
	p->time = cl.time;
	p->flags = flags;

	// sizeof( src ) != sizeof( p ), we must copy params manually
	VectorCopy( src->origin, p->origin );
	VectorCopy( src->velocity, p->velocity );
	VectorCopy( src->accel, p->accel ); 
	VectorCopy( src->color, p->color );
	VectorCopy( src->colorVelocity, p->colorVelocity );
	p->alpha = src->alpha;

	p->radius = src->radius;
	p->length = src->length;
	p->rotation = src->rotation;
	p->alphaVelocity = src->alphaVelocity;
	p->radiusVelocity = src->radiusVelocity;
	p->lengthVelocity = src->lengthVelocity;
	p->bounceFactor = src->bounceFactor;

	// needs to save old origin
	if( flags & (PARTICLE_BOUNCE|PARTICLE_FRICTION))
		VectorCopy( p->origin, p->oldorigin );

	return true;
}

/*
================
CL_TestEntities

if cl_testentities is set, create 32 player models
================
*/
void CL_TestEntities( void )
{
	int	i, j;
	float	f, r;
	edict_t	ent;

	if( !cl_testentities->integer )
		return;

	Mem_Set( &ent, 0, sizeof( edict_t ));
	V_ClearScene();

	for( i = 0; i < 32; i++ )
	{
		r = 64 * ((i%4) - 1.5 );
		f = 64 * (i/4) + 128;

		for( j = 0; j < 3; j++ )
			ent.v.origin[j] = cl.refdef.vieworg[j]+cl.refdef.forward[j] * f + cl.refdef.right[j] * r;

		ent.v.scale = 1.0f;
		ent.serialnumber = cl.frame.ps.number;
		ent.v.controller[0] = ent.v.controller[1] = 90.0f;
		ent.v.controller[2] = ent.v.controller[3] = 180.0f;
		ent.v.modelindex = cl.frame.ps.modelindex;
		re->AddRefEntity( &ent, ED_NORMAL, 1.0f );
	}
}

/*
================
CL_TestLights

If cl_testlights is set, create 32 lights models
================
*/
void CL_TestLights( void )
{
	int	i, j;
	float	f, r;
	cdlight_t	dl;

	if( !cl_testlights->integer )
		return;

	Mem_Set( &dl, 0, sizeof( cdlight_t ));
	V_ClearScene();
	
	for( i = 0; i < 32; i++ )
	{
		r = 64 * ( (i%4) - 1.5 );
		f = 64 * (i/4) + 128;

		for( j = 0; j < 3; j++ )
			dl.origin[j] = cl.refdef.vieworg[j] + cl.refdef.forward[j] * f + cl.refdef.right[j] * r;

		dl.color[0] = ((i%6)+1) & 1;
		dl.color[1] = (((i%6)+1) & 2)>>1;
		dl.color[2] = (((i%6)+1) & 4)>>2;
		dl.radius = 200;
		re->AddDynLight( dl.origin, dl.color, dl.radius ); 
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
	CL_ClearDecals ();
}
