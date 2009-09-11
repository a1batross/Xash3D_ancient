//=======================================================================
//			Copyright XashXT Group 2008 ©
//		         engine_api.h - xash engine api
//=======================================================================
#ifndef ENGINE_API_H
#define ENGINE_API_H

#include "const.h"

//
// engine constant limits, touching networking protocol modify with precaution
//
#define MAX_DLIGHTS			32	// dynamic lights (per one frame)
#define MAX_LIGHTSTYLES		256	// can't be blindly increased
#define MAX_DECALS			256	// server decal indexes
#define MAX_USER_MESSAGES		200	// another 56 messages reserved for engine routines
#define MAX_CLASSNAMES		512	// maxcount of various edicts classnames
#define MAX_SOUNDS			512	// openal software limit
#define MAX_MODELS			4096	// total count of brush & studio various models per one map
#define MAX_PARTICLES		32768	// pre one frame
#define MAX_EDICTS			65535	// absolute limit that never be reached, (do not edit!)
#define MAX_VERTS_ON_POLY		10	// decal vertices

// usercmd_t communication (a part of network protocol)
typedef struct usercmd_s
{
	int		msec;
	int		angles[3];
	int		forwardmove;
	int		sidemove;
	int		upmove;
	int		buttons;
	int		lightlevel;
} usercmd_t;

/*
==============================================================================

ENGINE TRACE FORMAT
==============================================================================
*/
#define SIDE_FRONT		0
#define SIDE_BACK		1
#define SIDE_ON		2
#define SIDE_CROSS		-2

#define PLANE_X		0	// 0 - 2 are axial planes
#define PLANE_Y		1	// 3 needs alternate calc
#define PLANE_Z		2
#define PLANE_NONAXIAL	3

typedef struct cplane_s
{
	vec3_t	normal;
	float	dist;
	short	type;		// for fast side tests
	short	signbits;		// signx + (signy<<1) + (signz<<1)
} cplane_t;

typedef struct cmesh_s
{
	vec3_t	*verts;
	int	*indices;
	uint	numverts;
	uint	numtris;
} cmesh_t;

typedef struct cmodel_s
{
	string	name;		// model name
	byte	*mempool;		// private mempool
	int	registration_sequence;

	vec3_t	mins, maxs;	// model boundbox
	int	type;		// model type
	int	firstface;	// used to create collision tree
	int	numfaces;		
	int	firstbrush;	// used to create collision brush
	int	numbrushes;
	int	numframes;	// sprite framecount
	int	numbodies;	// physmesh numbody
	cmesh_t	*col[256];	// max bodies
	byte	*extradata;	// server studio uses this

	// g-cont: stupid pushmodel stuff
	vec3_t	normalmins;	// bounding box at angles '0 0 0'
	vec3_t	normalmaxs;
	vec3_t	yawmins;		// bounding box if yaw angle is not 0, but pitch and roll are
	vec3_t	yawmaxs;
	vec3_t	rotatedmins;	// bounding box if pitch or roll are used
	vec3_t	rotatedmaxs;

	// custom traces for various model types
	void (*TraceBox)( const vec3_t, const vec3_t, const vec3_t, const vec3_t, struct cmodel_s*, struct trace_s*, int );
	int (*PointContents)( const vec3_t point, struct cmodel_s *model );
} cmodel_t;

typedef struct trace_s
{
	bool		allsolid;		// if true, plane is not valid
	bool		startsolid;	// if true, the initial point was in a solid area
	bool		startstuck;	// trace started from solid entity
	float		fraction;		// time completed, 1.0 = didn't hit anything
	float		realfraction;	// like fraction but is not nudged away from the surface
	vec3_t		endpos;		// final position
	cplane_t		plane;		// surface normal at impact
	const char	*pTexName;	// texturename
	int		contents;		// contents on other side of surface hit
	int		startcontents;
	int		contentsmask;
	int		surfaceflags;
	int		hitgroup;		// hit a studiomodel hitgroup #
	int		flags;		// misc trace flags

	union
	{
		edict_t	*ent;		// not set by CM_*() functions
		void	*gp;		// render traceline use this		
	};
} trace_t;

/*
=================
CategorizePlane

A slightly more complex version of SignbitsForPlane and PlaneTypeForNormal,
which also tries to fix possible floating point glitches (like -0.00000 cases)
=================
*/
_inline void CategorizePlane( cplane_t *plane )
{
	int	i;

	plane->signbits = 0;
	plane->type = PLANE_NONAXIAL;

	for( i = 0; i < 3; i++ )
	{
		if( plane->normal[i] < 0 )
		{
			plane->signbits |= 1<<i;
			if( plane->normal[i] == -1.0f )
			{
				plane->signbits = (1<<i);
				plane->normal[0] = plane->normal[1] = plane->normal[2] = 0;
				plane->normal[i] = -1.0f;
				break;
			}
		}
		else if( plane->normal[i] == 1.0f )
		{
			plane->type = i;
			plane->signbits = 0;
			plane->normal[0] = plane->normal[1] = plane->normal[2] = 0;
			plane->normal[i] = 1.0f;
			break;
		}
	}
}

/*
=================
PlaneTypeForNormal
=================
*/
_inline int PlaneTypeForNormal( const vec3_t normal )
{
	if( normal[0] == 1.0 || normal[0] == -1.0 )
		return PLANE_X;
	if( normal[1] == 1.0 || normal[1] == -1.0 )
		return PLANE_Y;
	if( normal[2] == 1.0 || normal[2] == -1.0 )
		return PLANE_Z;
	return PLANE_NONAXIAL;
}

/*
==============
BoxOnPlaneSide (engine fast version)

Returns SIDE_FRONT, SIDE_BACK, or SIDE_ON
==============
*/
_inline int BoxOnPlaneSide( const vec3_t emins, const vec3_t emaxs, const cplane_t *p )
{
	if( p->type < 3 ) return ((emaxs[p->type] >= p->dist) | ((emins[p->type] < p->dist) << 1));
	switch( p->signbits )
	{
	default:
	case 0: return (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 1: return (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 2: return (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 3: return (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 4: return (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 5: return (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 6: return (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 7: return (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	}
}

/*
==============================================================================

Generic LAUNCH.DLL INTERFACE
==============================================================================
*/
typedef struct launch_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(launch_api_t)
	size_t	com_size;		// must matched with sizeof(stdlib_api_t)

	void ( *Init ) ( const int argc, const char **argv );	// init host
	void ( *Main ) ( void );				// host frame
	void ( *Free ) ( void );				// close host
	void (*CPrint) ( const char *msg );			// host print
} launch_exp_t;

#endif//ENGINE_API_H