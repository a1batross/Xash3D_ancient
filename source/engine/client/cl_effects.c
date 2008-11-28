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
PF_addlight

void AddLight( vector pos, vector color, float radius, float decay, float time, float key )
===============
*/
void PF_addlight( void )
{
	cdlight_t		*dl;
	const float	*pos, *col;
	float		radius, decay, time;
	int		key;

	if( !VM_ValidateArgs( "AddLight", 6 ))
		return;

	pos = PRVM_G_VECTOR(OFS_PARM0);
	col = PRVM_G_VECTOR(OFS_PARM1);
	radius = PRVM_G_FLOAT(OFS_PARM2);
	decay = PRVM_G_FLOAT(OFS_PARM3);
	time = PRVM_G_FLOAT(OFS_PARM4);
	key = (int)PRVM_G_FLOAT(OFS_PARM5);

	if( radius <= 0 )
	{
		VM_Warning( "AddLight: ignore light with radius <= 0\n" );
		return;
	}

	dl = CL_AllocDlight( key );
	VectorCopy( pos, dl->origin );
	VectorCopy( col, dl->color );
	dl->die = cl.time + (time * 1000);
	dl->decay = (decay * 1000);
	dl->radius = radius;
}


/*
===============
CL_RunDLights

===============
*/
void CL_RunDLights( void )
{
	int	i;
	cdlight_t	*dl;

	dl = cl_dlights;
	for( i = 0; i < MAX_DLIGHTS; i++, dl++ )
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
#define MAX_DECALS			2048
#define DECAL_FADETIME		(30 * 1000)	// 30 seconds
#define DECAL_STAYTIME		(120 * 1000)	// 120 seconds

typedef struct cdecal_s
{
	struct cdecal_s	*prev, *next;
	int		time;
	vec4_t		modulate;
	bool		alphaFade;
	shader_t		shader;
	int		numVerts;
	polyVert_t	verts[MAX_VERTS_ON_POLY];
	vec3_t		origin;
} cdecal_t;

static cdecal_t	cl_activeDecals;
static cdecal_t	*cl_freeDecals;
static cdecal_t	cl_decalList[MAX_DECALS];

/*
=================
CL_FreeDecal
=================
*/
static void CL_FreeDecal( cdecal_t *decal )
{
	if( !decal->prev )
		return;

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

	for( i = 0; i < MAX_DECALS - 1; i++ )
		cl_decalList[i].next = &cl_decalList[i+1];
}

/*
=================
CL_AddDecal

called from render after clipping
=================
*/
void CL_AddDecal( vec3_t org, matrix3x3 m, shader_t s, vec4_t rgba, bool fade, decalFragment_t *df, const vec3_t *v )
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
	int	i, time, fadeTime;
	float	c;

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
				c = 1.0 - ((float)time / fadeTime);

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
			c = (float)time / fadeTime;

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
PF_adddecal

void AddDecal( vector org, vector dir, vector color, float rot, float radius, float alpha, float fade, float shader, float temp )
===============
*/
void PF_adddecal( void )
{
	float	*org, *dir, *col;
	float	rot, radius, alpha;
	shader_t	shader;
	bool	fade, temp;
	vec4_t	modulate;

	if( !VM_ValidateArgs( "AddDecal", 9 ))
		return;

	VM_ValidateString( PRVM_G_STRING( OFS_PARM7 ));
	org = PRVM_G_VECTOR(OFS_PARM0);
	dir = PRVM_G_VECTOR(OFS_PARM1);
	col = PRVM_G_VECTOR(OFS_PARM2);
	rot = PRVM_G_FLOAT(OFS_PARM3);
	radius = PRVM_G_FLOAT(OFS_PARM4);
	alpha = PRVM_G_FLOAT(OFS_PARM5);
	fade = (bool)PRVM_G_FLOAT(OFS_PARM6);
	shader = re->RegisterShader( PRVM_G_STRING(OFS_PARM7), SHADER_GENERIC );
	temp = (bool)PRVM_G_FLOAT(OFS_PARM8);

	Vector4Set( modulate, col[0], col[1], col[2], alpha );
	re->ImpactMark( org, dir, rot, radius, modulate, fade, shader, temp );	
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
