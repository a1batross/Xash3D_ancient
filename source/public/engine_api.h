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
#define MAX_DLIGHTS			128	// dynamic lights (per one frame)
#define MAX_LIGHTSTYLES		256	// can't be blindly increased
#define MAX_DECALS			256	// server decal indexes
#define MAX_USER_MESSAGES		200	// another 56 messages reserved for engine routines
#define MAX_CLASSNAMES		512	// maxcount of various edicts classnames
#define MAX_SOUNDS			512	// openal software limit
#define MAX_MODELS			4096	// total count of brush & studio various models per one map
#define MAX_PARTICLES		32768	// pre one frame
#define MAX_EDICTS			65535	// absolute limit that never be reached, (do not edit!)
#define MAX_VERTS_ON_POLY		10	// decal vertices

// model_state_t communication (a part of network protocol)
typedef struct model_state_s
{
	int		index;		// server & client shared modelindex	
	int		colormap;		// change base color for some textures or sprite frames
	float		scale;		// model or sprite scale, affects to physics too
	float		frame;		// % playback position in animation sequences (0..255)
	float		animtime;		// auto-animating time
	float		framerate;	// custom framerate, specified by QC
	int		sequence;		// animation sequence (0 - 255)
	int		gaitsequence;	// client\nps\bot gaitsequence
	int		skin;		// skin for studiomodels
	int		body;		// sub-model selection for studiomodels
	float		blending[16];	// studio animation blending
	float		controller[16];	// studio bone controllers
} model_state_t;

// entity_state_t communication (a part of network protocol)
typedef struct entity_state_s
{
	// engine specific
	uint		number;		// edict index
	edtype_t		ed_type;		// edict type
	string_t		classname;	// edict classname
	int		soundindex;	// looped ambient sound

	// physics information
	vec3_t		origin;
	vec3_t		angles;		// entity angles, not viewangles
	vec3_t		velocity;		// player velocity
	vec3_t		infotarget;	// portal camera, etc
	model_state_t	model;		// general entity model
	solid_t		solidtype;	// entity solidtype
	movetype_t	movetype;		// entity movetype
	int		gravity;		// gravity multiplier
	int		aiment;		// attahced entity (not a physic attach, only rendering)
	int		solid;		// using for symmetric bboxes
	vec3_t		mins;		// not symmetric entity bbox    
	vec3_t		maxs;

	// render information
	uint		effects;		// effect flags like q1 and hl1
	int		renderfx;		// render effects same as hl1
	float		renderamt;	// alpha value or like somewhat
	vec3_t		rendercolor;	// hl1 legacy stuff, working, but not needed
	int		rendermode;	// hl1 legacy stuff, working, but not needed

	// client specific
	int		pm_type;		// client movetype
	int		pm_flags;		// ducked, jump_held, etc
	vec3_t		delta_angles;	// add to command angles to get view direction 
	vec3_t		punch_angles;	// add to view direction to get render angles 
	vec3_t		viewangles;	// already calculated view angles on server-side
	vec3_t		viewoffset;	// viewoffset over ground
	int		maxspeed;		// sv_maxspeed will be duplicate on all clients
	float		health;		// client health (other parms can be send by custom messages)
	float		fov;		// horizontal field of view
	int		weapon1;		// weapon lowbytes
	int		weapon2;		// weapon highbytes
	model_state_t	pmodel;		// weaponmodel info
} entity_state_t;

// usercmd_t communication (a part of network protocol)
typedef struct usercmd_s
{
	int		msec;
	int		angles[3];
	int		forwardmove;
	int		sidemove;
	int		upmove;
	int		buttons;
	int		impulse;
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

typedef struct cplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;		// for fast side tests
	byte	signbits;		// signx + (signy<<1) + (signz<<1)
	byte	pad[2];		// padding bytes
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
	bool (*AmbientLevel)( const vec3_t point, float *volumes, struct cmodel_s *model );
} cmodel_t;

typedef struct csurface_s
{
	string	name;
	int	surfaceflags;
	int	contentflags;
	int	value;

	vec3_t	mins;
	vec3_t	maxs;

	// patches support
	int	numtriangles;
	int	numvertices;
	int	*indices;
	float	*vertices;
	int	markframe;
} csurface_t;

typedef struct trace_s
{
	bool		allsolid;		// if true, plane is not valid
	bool		startsolid;	// if true, the initial point was in a solid area
	bool		startstuck;	// trace started from solid entity
	float		fraction;		// time completed, 1.0 = didn't hit anything
	float		realfraction;	// like fraction but is not nudged away from the surface
	vec3_t		endpos;		// final position
	cplane_t		plane;		// surface normal at impact
	csurface_t	*surface;		// surface hit
	int		contents;		// contents on other side of surface hit
	int		startcontents;
	int		contentsmask;
	int		surfaceflags;
	int		hitgroup;		// hit a studiomodel hitgroup #
	int		flags;		// misc trace flags
	edict_t		*ent;		// not set by CM_*() functions
} trace_t;


_inline void PlaneClassify( cplane_t *p )
{
	// for optimized plane comparisons
	if( p->normal[0] == 1 || p->normal[0] == -1 )
		p->type = PLANE_X;
	else if( p->normal[1] == 1 || p->normal[1] == -1 )
		p->type = PLANE_Y;
	else if( p->normal[2] == 1 || p->normal[2] == -1 )
		p->type = PLANE_Z;
	else p->type = 3; // needs alternate calc

	// for BoxOnPlaneSide
	p->signbits = 0;
	if( p->normal[0] < 0 ) p->signbits |= 1;
	if( p->normal[1] < 0 ) p->signbits |= 2;
	if( p->normal[2] < 0 ) p->signbits |= 4;
}

/*
==============
BoxOnPlaneSide (engine fast version)

Returns SIDE_FRONT, SIDE_BACK, or SIDE_ON
==============
*/
_inline int BoxOnPlaneSide( const vec3_t emins, const vec3_t emaxs, cplane_t *p )
{
	if (p->type < 3) return ((emaxs[p->type] >= p->dist) | ((emins[p->type] < p->dist) << 1));
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

	void ( *Init ) ( int argc, char **argv );		// init host
	void ( *Main ) ( void );				// host frame
	void ( *Free ) ( void );				// close host
	void (*CPrint) ( const char *msg );			// host print
	void (*MSG_Init)( sizebuf_t *buf, byte *data, size_t len );	// MSG init network buffer
} launch_exp_t;

#endif//ENGINE_API_H