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
pfnAddDLight

===============
*/
void pfnAddDLight( const float *org, const float *rgb, float radius, float time, int flags, int key )
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
CL_RunDLights

===============
*/
void CL_RunDLights( void )
{
	cdlight_t	*dl;
	int	i;
	
	for( i = 0, dl = cl_dlights; i < MAX_DLIGHTS; i++, dl++ )
	{
		if( dl->free ) continue;
		if( cl.time >= dl->end )
		{
			dl->free = true;
			return;
		}

		if( dl->fade )
		{
			dl->intensity = (float)(cl.time - dl->start) / (dl->end - dl->start);
			dl->intensity = dl->radius * (1.0 - dl->intensity);
		}
		else dl->intensity = dl->radius; // const
	}
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
		if( !dl->free ) re->AddDynLight( dl );
}

/*
==============================================================

DECALS MANAGEMENT

==============================================================
*/
#define MAX_DRAWDECALS		256
#define MAX_DECAL_VERTS		128
#define MAX_DECAL_FRAGMENTS		64

typedef struct cdecal_s
{
	struct cdecal_s	*prev, *next;

	int		die;			// remove after this time
	int		time;			// time when decal is placed
	int		fadetime;
	float		fadefreq;
	word		flags;
	vec4_t		color;

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
void CL_SpawnDecal( vec3_t org, vec3_t dir, float rot, float rad, float *col, float die, float fadetime, word flags, shader_t handle )
{
	int		i, j;
	cdecal_t		*dl;
	poly_t		*poly;
	vec3_t		axis[3];
	vec3_t		verts[MAX_DECAL_VERTS];
	fragment_t	*fr, fragments[MAX_DECAL_FRAGMENTS];
	int		numfragments;
	float		dietime, fadefreq;
	rgba_t		color;

	// invalid decal
	if( rad <= 0 || VectorIsNull( dir ))
		return;

	// calculate orientation matrix
	VectorNormalize2( dir, axis[0] );
	PerpendicularVector( axis[1], axis[0] );
	RotatePointAroundVector( axis[2], axis[0], axis[1], rot );
	CrossProduct( axis[0], axis[2], axis[1] );

	numfragments = re->GetFragments( org, rad, axis, MAX_DECAL_VERTS, verts, MAX_DECAL_FRAGMENTS, fragments );

	// no valid fragments
	if( !numfragments ) return;

	// clamp and scale colors
	color[0] = (byte)bound( 0, col[0] * 255, 255 );
	color[1] = (byte)bound( 0, col[1] * 255, 255 );
	color[2] = (byte)bound( 0, col[2] * 255, 255 );
	color[3] = (byte)bound( 0, col[3] * 255, 255 );

	rad = 0.5f / rad;
	VectorScale( axis[1], rad, axis[1] );
	VectorScale( axis[2], rad, axis[2] );

	if( die == -1.0f )
	{
		dietime = -1.0f;
		fadefreq = fadetime = 1.0f;
	}
	else
	{
		dietime = cl.time + (die * 1000);
		fadefreq = 0.001f / min( fadetime, die );
		fadetime = cl.time + (die - min( fadetime, die )) * 1000;
	}

	for( i = 0, fr = fragments; i < numfragments; i++, fr++ )
	{
		if( fr->numverts > MAX_DECAL_VERTS )
			return;
		else if( fr->numverts <= 0 )
			continue;

		// allocate decal
		dl = CL_AllocDecal ();
		Vector4Copy( col, dl->color );
		dl->die = dietime;
		dl->time = cl.time;
		dl->fadetime = fadetime;
		dl->fadefreq = fadefreq;
		dl->flags = flags;

		// setup polygon for drawing
		poly = dl->poly;
		poly->shadernum = handle;
		poly->numverts = fr->numverts;
		poly->fognum = fr->fognum;

		for( j = 0; j < fr->numverts; j++ )
		{
			vec3_t v;

			VectorCopy( verts[fr->firstvert+j], poly->verts[j] );
			VectorSubtract( poly->verts[j], org, v );
			poly->stcoords[j][0] = DotProduct( v, axis[1] ) + 0.5f;
			poly->stcoords[j][1] = DotProduct( v, axis[2] ) + 0.5f;
			for( i = 0; i < poly->numverts; i++ )
				*( int * )poly->colors[i] = *( int * )color;
		}
	}
}

/*
=================
CL_AddDecals
=================
*/
void CL_AddDecals( void )
{
	int		i;
	float		fade;
	cdecal_t		*dl, *next, *hnode;
	poly_t		*poly;
	rgba_t		color;

	// add decals in first-spawned - first-drawn order
	hnode = &cl_decals_headnode;
	for( dl = hnode->prev; dl != hnode; dl = next )
	{
		next = dl->prev;

		if( dl->die == -1.0f )
		{
			// static decals not fading, not removes
			re->AddPolygon( dl->poly );
			continue;
		}

		// it's time to DIE
		if( dl->die <= cl.time )
		{
			CL_FreeDecal( dl );
			continue;
		}
		poly = dl->poly;

		if( dl->flags & DECAL_FADEENERGY )
		{
			float	time = cl.time - dl->time;

			if( time < dl->fadetime )
			{
				fade = 255 - (time / dl->fadetime);
				color[0] = (byte)(( dl->color[0] * fade ) * 255);
				color[1] = (byte)(( dl->color[1] * fade ) * 255);
				color[2] = (byte)(( dl->color[2] * fade ) * 255);
			}
			for( i = 0; i < poly->numverts; i++ )
				*( int * )poly->colors[i] = *( int * )color;
		}

		// fade out
		if( dl->fadetime < cl.time )
		{
			fade = (dl->die - cl.time) * dl->fadefreq;

			if( dl->flags & DECAL_FADEALPHA )
			{
				color[0] = (byte)(( dl->color[0] ) * 255);
				color[1] = (byte)(( dl->color[1] ) * 255);
				color[2] = (byte)(( dl->color[2] ) * 255);
				color[3] = (byte)(( dl->color[3] * fade ) * 255);
			}
			else
			{
				color[0] = (byte)(( dl->color[0] * fade ) * 255);
				color[1] = (byte)(( dl->color[1] * fade ) * 255);
				color[2] = (byte)(( dl->color[2] * fade ) * 255);
				color[3] = (byte)(( dl->color[3] ) * 255);
			}
			for( i = 0; i < poly->numverts; i++ )
				*( int * )poly->colors[i] = *( int * )color;
		}
		re->AddPolygon( poly );
	}
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

		trace = CL_Trace( origin, vec3_origin, vec3_origin, point, MOVE_WORLDONLY, NULL, MASK_SOLID );
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

/*
===============
CL_SpawnStaticDecal

===============
*/
void CL_SpawnStaticDecal( vec3_t origin, int decalIndex, int entityIndex, int modelIndex )
{
	vec4_t	col = { 1.0f, 1.0f, 1.0f, 1.0f };
	float	radius = 32.0f;
	vec3_t	dir;

	if( entityIndex != 0 )
	{
		MsgDev( D_ERROR, "Current Xash version allows static decals only on world surfaces\n" );
		return;
	}

	CL_FindExplosionPlane( origin, radius, dir );
	decalIndex = bound( 0, decalIndex, MAX_DECALS - 1 );
	CL_SpawnDecal( origin, dir, 90.0f, radius, col, -1.0f, 0.0f, 0, cl.decal_shaders[decalIndex] );
}

/*
===============
pfnAddDecal

===============
*/
void pfnAddDecal( float *org, float *dir, float *rgba, float rot, float rad, HSPRITE hSpr, int flags )
{
	CL_SpawnDecal( org, dir, rot, rad, rgba, 120.0f, 30.0f, flags, hSpr );
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
	float		scale;
	bool		fog;

	// this part are private for engine
	cparticle_t	*next;
	shader_t		shader;

	int		time;
	int		flags;

	vec3_t		oldorigin;

	poly_t		poly;
	vec3_t		pVerts[4];
	vec2_t		pStcoords[4];
	rgba_t		pColor[4];
};

cparticle_t	*cl_active_particles, *cl_free_particles;
static cparticle_t	cl_particle_list[MAX_PARTICLES];
static vec3_t	cl_particle_velocities[NUMVERTEXNORMALS];
static vec3_t	cl_particlePalette[256];

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
	cparticle_t	*p;
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
		cl_particle_velocities[i][0] = (rand() & 255) * 0.01f;
		cl_particle_velocities[i][1] = (rand() & 255) * 0.01f;
		cl_particle_velocities[i][2] = (rand() & 255) * 0.01f;
	}

	for( i = 0, p = cl_particle_list; i < MAX_PARTICLES; i++, p++ )
	{
		p->pStcoords[0][0] = p->pStcoords[2][1] = p->pStcoords[1][0] = p->pStcoords[1][1] = 0;
		p->pStcoords[0][1] = p->pStcoords[2][0] = p->pStcoords[3][0] = p->pStcoords[3][1] = 1;
	}

	// Xash3D have built-in Quake1 palette - use it
	for( i = 0; pic && i < 256; i++ )
	{
		cl_particlePalette[i][0] = pic->palette[i*4+0] * (1.0f / 255);
		cl_particlePalette[i][1] = pic->palette[i*4+1] * (1.0f / 255);
		cl_particlePalette[i][2] = pic->palette[i*4+2] * (1.0f / 255);
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
	cparticle_t	*p, *next;
	cparticle_t	*active = NULL, *tail = NULL;
	rgba_t		modulate;
	vec3_t		origin, oldorigin, velocity;
	vec3_t		ambientLight, axis[3];
	float		alpha, radius, length;
	float		time, time2, gravity, dot;
	vec3_t		mins, maxs, color;
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

		time = (cl.time - p->time) * 0.001f;
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
			if( trace.flFraction != 0.0f && trace.flFraction != 1.0f )
			{
				// reflect velocity
				time = cl.time - (cls.frametime + cls.frametime * trace.flFraction) * 1000;
				time = (time - p->time) * 0.001f;

				velocity[0] = p->velocity[0];
				velocity[1] = p->velocity[1];
				velocity[2] = p->velocity[2] + p->accel[2] * gravity * time;
				VectorReflect( velocity, 0, trace.vecPlaneNormal, p->velocity );
				VectorScale( p->velocity, p->bounceFactor, p->velocity );

				// check for stop or slide along the plane
				if( trace.vecPlaneNormal[2] > 0 && p->velocity[2] < 1 )
				{
					if( trace.vecPlaneNormal[2] == 1 )
					{
						VectorClear( p->velocity );
						VectorClear( p->accel );
						p->flags &= ~PARTICLE_BOUNCE;
					}
					else
					{
						// FIXME: check for new plane or free fall
						dot = DotProduct( p->velocity, trace.vecPlaneNormal );
						VectorMA( p->velocity, -dot, trace.vecPlaneNormal, p->velocity );

						dot = DotProduct( p->accel, trace.vecPlaneNormal );
						VectorMA( p->accel, -dot, trace.vecPlaneNormal, p->accel );
					}
				}

				VectorCopy( trace.vecEndPos, origin );
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
		modulate[0] = (byte)bound( 0, color[0] * 255, 255 );
		modulate[1] = (byte)bound( 0, color[1] * 255, 255 );
		modulate[2] = (byte)bound( 0, color[2] * 255, 255 );
		modulate[3] = (byte)bound( 0, alpha * 255, 255 );

		if( p->flags & PARTICLE_INSTANT )
		{
			// instant particle
			p->alpha = 0;
			p->alphaVelocity = 0;
		}

		*(int *)p->pColor[0] = *(int *)modulate;
		*(int *)p->pColor[1] = *(int *)modulate;
		*(int *)p->pColor[2] = *(int *)modulate;
		*(int *)p->pColor[3] = *(int *)modulate;

		if( radius == 1.0f )
		{
			float	scale = 0.0f;

			// hack a scale up to keep quake particles from disapearing
			scale += (origin[0] - cl.refdef.vieworg[0]) * cl.refdef.forward[0];
			scale += (origin[1] - cl.refdef.vieworg[1]) * cl.refdef.forward[1];
			scale += (origin[2] - cl.refdef.vieworg[2]) * cl.refdef.forward[2];
			if( scale >= 20 ) radius = 1.0f + scale * 0.004f;
		}

		if( length != 1 )
		{
			// find orientation vectors
			VectorSubtract( cl.refdef.vieworg, p->origin, axis[0] );
			VectorSubtract( p->oldorigin, p->origin, axis[1] );
			CrossProduct( axis[0], axis[1], axis[2] );

			VectorNormalizeFast( axis[1] );
			VectorNormalizeFast( axis[2] );

			// find normal
			CrossProduct( axis[1], axis[2], axis[0] );
			VectorNormalizeFast( axis[0] );

			VectorMA( p->origin, -length, axis[1], p->oldorigin );
			VectorScale( axis[2], radius, axis[2] );

			p->pVerts[0][0] = oldorigin[0] + axis[2][0];
			p->pVerts[0][1] = oldorigin[1] + axis[2][1];
			p->pVerts[0][2] = oldorigin[2] + axis[2][2];
			p->pVerts[1][0] = origin[0] + axis[2][0];
			p->pVerts[1][1] = origin[1] + axis[2][1];
			p->pVerts[1][2] = origin[2] + axis[2][2];
			p->pVerts[2][0] = origin[0] - axis[2][0];
			p->pVerts[2][1] = origin[1] - axis[2][1];
			p->pVerts[2][2] = origin[2] - axis[2][2];
			p->pVerts[3][0] = oldorigin[0] - axis[2][0];
			p->pVerts[3][1] = oldorigin[1] - axis[2][1];
			p->pVerts[3][2] = oldorigin[2] - axis[2][2];
		}
		else
		{
			if( p->rotation )
			{
				// Rotate it around its normal
				RotatePointAroundVector( axis[1], cl.refdef.forward, cl.refdef.right, p->rotation );
				CrossProduct( cl.refdef.forward, axis[1], axis[2] );

				// The normal should point at the viewer
				VectorNegate( cl.refdef.forward, axis[0] );

				// Scale the axes by radius
				VectorScale( axis[1], radius, axis[1] );
				VectorScale( axis[2], radius, axis[2] );
			}
			else
			{
				// the normal should point at the viewer
				VectorNegate( cl.refdef.forward, axis[0] );

				// scale the axes by radius
				VectorScale( cl.refdef.right, radius, axis[1] );
				VectorScale( cl.refdef.up, radius, axis[2] );
			}

			p->pVerts[0][0] = origin[0] + axis[1][0] + axis[2][0];
			p->pVerts[0][1] = origin[1] + axis[1][1] + axis[2][1];
			p->pVerts[0][2] = origin[2] + axis[1][2] + axis[2][2];
			p->pVerts[1][0] = origin[0] - axis[1][0] + axis[2][0];
			p->pVerts[1][1] = origin[1] - axis[1][1] + axis[2][1];
			p->pVerts[1][2] = origin[2] - axis[1][2] + axis[2][2];
			p->pVerts[2][0] = origin[0] - axis[1][0] - axis[2][0];
			p->pVerts[2][1] = origin[1] - axis[1][1] - axis[2][1];
			p->pVerts[2][2] = origin[2] - axis[1][2] - axis[2][2];
			p->pVerts[3][0] = origin[0] + axis[1][0] - axis[2][0];
			p->pVerts[3][1] = origin[1] + axis[1][1] - axis[2][1];
			p->pVerts[3][2] = origin[2] + axis[1][2] - axis[2][2];
		}

		p->poly.numverts = 4;
		p->poly.verts = p->pVerts;
		p->poly.stcoords = p->pStcoords;
		p->poly.colors = p->pColor;
		p->poly.shadernum = p->shader;
		p->poly.fognum = p->fog ? 0 : -1;

		// send the particle to the renderer
		re->AddPolygon( &p->poly );
	}
	cl_active_particles = active;
}

/*
===============
CL_ParticleEffect

old good quake1 particles
===============
*/
void CL_ParticleEffect( const vec3_t org, const vec3_t dir, int color, int count )
{
	int		i, pal;
	cparticle_t	src;

	for( i = 0; i < count; i++ )
	{
		src.origin[0] = org[0] + RANDOM_FLOAT( -8, 8 );
		src.origin[1] = org[1] + RANDOM_FLOAT( -8, 8 );
		src.origin[2] = org[2] + RANDOM_FLOAT( -8, 8 );
		src.velocity[0] = dir[0] * 15;
		src.velocity[1] = dir[1] * 15;
		src.velocity[2] = dir[2] * 15;
		pal = (color & ~7) + (rand() & 7);
		src.color[0] = cl_particlePalette[pal][0];
		src.color[1] = cl_particlePalette[pal][1];
		src.color[2] = cl_particlePalette[pal][2];
		VectorClear( src.colorVelocity );
		VectorClear( src.accel );
		src.alpha = 1.0;
		src.alphaVelocity = -8.0 + RANDOM_FLOAT( 1.0, 5.0 ); // lifetime
		src.radius = 1.0; // quake particles have constant sizes
		src.radiusVelocity = 0;
		src.length = 1;
		src.lengthVelocity = 0;
		src.rotation = 0;	// quake particles doesn't rotating

		if( !pfnAddParticle( &src, cls.particle, 0 ))
			return;
	}
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
	p->scale = 1.0f;
	p->fog = false;

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
		re->AddRefEntity( &ent, ED_NORMAL );
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

	if( cl_testflashlight->integer )
	{
		vec3_t		end;
		edict_t		*ed = CL_GetLocalPlayer();
		int		cnt = CL_ContentsMask( ed );
		static shader_t	flashlight_shader = -1;
		trace_t		trace;

		Mem_Set( &dl, 0, sizeof( cdlight_t ));
#if 0
		if( flashlight_shader == -1 )
			flashlight_shader = re->RegisterShader( "flashlight", SHADER_GENERIC );
#endif
		VectorScale( cl.refdef.forward, 256, end );
		VectorAdd( end, cl.refdef.vieworg, end );

		trace = CL_Trace( cl.refdef.vieworg, vec3_origin, vec3_origin, end, 0, ed, cnt );
		VectorSet( dl.color, 1.0f, 1.0f, 1.0f );
		dl.intensity = 96;
		dl.texture = flashlight_shader;
		VectorCopy( trace.vecEndPos, dl.origin );

		re->AddDynLight( &dl );
	}
	if( cl_testlights->integer )
	{
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
