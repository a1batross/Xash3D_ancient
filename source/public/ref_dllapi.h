 //=======================================================================
//			Copyright XashXT Group 2007 ©
//		  ref_dllapi.h - shared ifaces between engine parts
//=======================================================================
#ifndef REF_DLLAPI_H
#define REF_DLLAPI_H

#include "ref_dfiles.h"

typedef struct { uint b:5; uint g:6; uint r:5; } color16;
typedef struct { byte r:8; byte g:8; byte b:8; } color24;
typedef struct { byte r; byte g; byte b; byte a; } color32;

typedef struct { int ofs; int type; const char *name; } fields_t;	// prvm custom fields
typedef enum { mod_bad, mod_world, mod_brush, mod_studio, mod_sprite } modtype_t;
typedef void (*cmsave_t) (void* handle, const void* buffer, size_t size);
typedef void (*cmdraw_t)( int color, int numpoints, const float *points );

typedef struct edict_s		edict_t;
typedef struct sv_edict_s		sv_edict_t;
typedef struct cl_edict_s		cl_edict_t;
typedef struct ui_edict_s		ui_edict_t;
typedef struct sv_entvars_s		sv_entvars_t;
typedef struct cl_entvars_s		cl_entvars_t;
typedef struct ui_entvars_s		ui_entvars_t;
typedef struct sv_globalvars_s	sv_globalvars_t;
typedef struct cl_globalvars_s	cl_globalvars_t;
typedef struct ui_globalvars_s	ui_globalvars_t;
typedef struct physbody_s		physbody_t;

typedef enum
{
	ED_SPAWNED = 0,	// this entity requris to set own type with SV_ClassifyEdict
	ED_STATIC,	// this is a logic without model or entity with static model
	ED_AMBIENT,	// this is entity emitted ambient sounds only
	ED_NORMAL,	// normal entity with model (and\or) sound
	ED_BSPBRUSH,	// brush entity (a part of level)
	ED_CLIENT,	// this is a client entity
	ED_MONSTER,	// monster or bot (generic npc with AI)
	ED_TEMPENTITY,	// this edict will be removed on server when "lifetime" exceeds 
	ED_BEAM,		// laser beam (needs to recalculate pvs and frustum)
	ED_MOVER,		// func_train, func_door and another bsp or mdl movers
	ED_VIEWMODEL,	// client or bot viewmodel (for spectating)
	ED_ITEM,		// holdable items
	ED_RAGDOLL,	// dead body with simulated ragdolls
	ED_RIGIDBODY,	// simulated physic
	ED_TRIGGER,	// just for sorting on a server
	ED_PORTAL,	// realtime display, portal or mirror brush or model
	ED_MISSILE,	// greande, rocket e.t.c
	ED_DECAL,		// render will be merge real coords and normal
	ED_VEHICLE,	// controllable vehicle
	ED_MAXTYPES,
} edtype_t;

// phys movetype
typedef enum
{
	MOVETYPE_NONE,	// never moves
	MOVETYPE_NOCLIP,	// origin and angles change with no interaction
	MOVETYPE_PUSH,	// no clip to world, push on box contact
	MOVETYPE_WALK,	// gravity
	MOVETYPE_STEP,	// gravity, special edge handling
	MOVETYPE_FLY,
	MOVETYPE_TOSS,	// gravity
	MOVETYPE_BOUNCE,
	MOVETYPE_FOLLOW,	// attached models
	MOVETYPE_CONVEYOR,
	MOVETYPE_PUSHABLE,
	MOVETYPE_PHYSIC,	// phys simulation
} movetype_t;

// phys collision mode
typedef enum
{
	SOLID_NOT = 0,    	// no interaction with other objects
	SOLID_TRIGGER,	// only touch when inside, after moving
	SOLID_BBOX,	// touch on edge
	SOLID_BSP,    	// bsp clip, touch on edge
	SOLID_BOX,	// physbox
	SOLID_SPHERE,	// sphere
	SOLID_CYLINDER,	// cylinder e.g. barrel
	SOLID_MESH,	// custom convex hull
} solid_t;

// model_state_t communication
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

// entity_state_t communication
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
	vec3_t		old_origin;	// for lerping animation
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
	int		pm_time;		// each unit = 8 ms
	vec3_t		delta_angles;	// add to command angles to get view direction 
	vec3_t		punch_angles;	// add to view direction to get render angles 
	vec3_t		viewangles;	// already calculated view angles on server-side
	vec3_t		viewoffset;	// viewoffset over ground
	int		maxspeed;		// sv_maxspeed will be duplicate on all clients
	float		health;		// client health (other parms can be send by custom messages)
	float		fov;		// horizontal field of view
	model_state_t	pmodel;		// weaponmodel info
} entity_state_t;

// usercmd_t communication
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
========================================================================

internal physic data

hold linear and angular velocity, current position stored too
========================================================================
*/
typedef struct physdata_s
{
	vec3_t		origin;
	vec3_t		angles;
	vec3_t		velocity;
	vec3_t		avelocity;	// "omega" in newton
	vec3_t		mins;		// for calculate size 
	vec3_t		maxs;		// and setup offset matrix

	int		movetype;		// moving type
	int		hulltype;		// solid type

	physbody_t	*body;		// ptr to physic body
} physdata_t;

// euler angle order
#define PITCH			0
#define YAW			1
#define ROLL			2

//
// engine constnat limits, touching networking protocol modify with cation
//
#define MAX_DLIGHTS			128	// dynamic lights (per one frame)
#define MAX_LIGHTSTYLES		256	// can be blindly increased
#define MAX_CLASSNAMES		512	// maxcount of various edicts classnames
#define MAX_SOUNDS			512	// openal software limit
#define MAX_MODELS			4096	// total count of brush & studio various models per one map
#define MAX_PARTICLES		32768	// pre one frame
#define MAX_EDICTS			65535	// absolute limit that never be reached, (do not edit!)

// FIXME: player_state_t->renderfx
#define RDF_NOWORLDMODEL		(1<<0)		// used for player configuration screen
#define RDF_IRGOGGLES		(1<<1)
#define RDF_PAIN			(1<<2)

// encoded bmodel mask
#define SOLID_BMODEL	0xffffff

/*
==============================================================================

MAP CONTENTS & SURFACES DESCRIPTION
==============================================================================
*/
#define PLANE_X			0	// 0 - 2 are axial planes
#define PLANE_Y			1	// 3 needs alternate calc
#define PLANE_Z			2

/*
==============================================================================

ENGINE TRACE FORMAT
==============================================================================
*/
typedef struct cplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;		// for fast side tests
	byte	signbits;		// signx + (signy<<1) + (signz<<1)
	byte	pad[2];
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
	byte	*mempool;		// personal mempool
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
	void (*TraceBox)( const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, struct cmodel_s *model, struct trace_s *trace, int brushsmask );
	int (*PointContents)( const vec3_t point, struct cmodel_s *model );
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

// pmove_state_t is the information necessary for client side movement
#define PM_NORMAL		0 // can accelerate and turn
#define PM_SPECTATOR	1
#define PM_DEAD		2 // no acceleration or turning
#define PM_GIB		3 // different bounding box
#define PM_FREEZE		4
#define PM_INTERMISSION	5
#define PM_NOCLIP		6

// pmove->pm_flags
#define PMF_DUCKED		1
#define PMF_JUMP_HELD	2
#define PMF_ON_GROUND	4
#define PMF_TIME_WATERJUMP	8	// pm_time is waterjump
#define PMF_TIME_LAND	16	// pm_time is time before rejump
#define PMF_TIME_TELEPORT	32	// pm_time is non-moving time
#define PMF_NO_PREDICTION	64	// temporarily disables prediction (used for grappling hook)
#define PMF_ALL_TIMES	(PMF_TIME_WATERJUMP|PMF_TIME_LAND|PMF_TIME_TELEPORT)

// button bits
#define	BUTTON_ATTACK	1
#define	BUTTON_ATTACK2	2
#define	BUTTON_USE	4
#define	BUTTON_WALKING	8
#define	BUTTON_ANY	128 // any key whatsoever

#define PM_MAXTOUCH		32

// sound channels
#define CHAN_AUTO		0
#define CHAN_WEAPON		1
#define CHAN_VOICE		2
#define CHAN_ITEM		3
#define CHAN_BODY		4
#define CHAN_ANNOUNCER	5  // announcer
// flags
#define CHAN_NO_PHS_ADD	8  // Send to all clients, not just ones in PHS (ATTN 0 will also do this)
#define CHAN_RELIABLE	16 // Send by reliable message, not datagram

// Sound attenuation values
#define ATTN_NONE		0  // Full volume the entire level
#define ATTN_NORM		1
#define ATTN_IDLE		2
#define ATTN_STATIC		3  // Diminish very rapidly with distance

typedef struct vrect_s
{
	int	x, y;
	int	width;
	int	height;
} vrect_t;

typedef struct
{
	vrect_t		rect;		// screen rectangle
	float		fov_x;		// field of view by vertical
	float		fov_y;		// field of view by horizontal
	vec3_t		vieworg;		// client origin + viewoffset
	vec3_t		viewangles;	// client angles
	float		time;		// time is used to shaders auto animate
	float		oldtime;		// oldtime using for lerping studio models
	uint		rdflags;		// client view effects: RDF_UNDERWATER, RDF_MOTIONBLUR, etc
	byte		*areabits;	// if not NULL, only areas with set bits will be drawn
} refdef_t;

typedef struct pmove_s
{
	entity_state_t	ps;		// state (in / out)

	// command (in)
	usercmd_t		cmd;

	physbody_t	*body;		// pointer to physobject

	// results (out)
	int		numtouch;
	edict_t		*touchents[PM_MAXTOUCH];	// max touch
	edict_t		*groundentity;

	vec3_t		mins, maxs;	// bounding box size
	int		watertype;
	int		waterlevel;
	float		xyspeed;		// avoid to compute it twice

	// callbacks to test the world
	void		(*trace)( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, trace_t *tr );
	int		(*pointcontents)( vec3_t point );
} pmove_t;

/*
==============================================================================

LAUNCH.DLL INTERFACE
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

/*
==============================================================================

RENDER.DLL INTERFACE
==============================================================================
*/
typedef struct render_exp_s
{
	// interface validator
	size_t	api_size;			// must matched with sizeof(render_exp_t)

	// initialize
	bool	(*Init)( void );		// init all render systems
	void	(*Shutdown)( void );	// shutdown all render systems

	void	(*BeginRegistration)( const char *map );
	bool	(*RegisterModel)( const char *name, int sv_index );
	bool	(*RegisterImage)( const char *name, int sv_index );
	bool	(*PrecacheImage)( const char *name );
	void	(*SetSky) (char *name, float rotate, vec3_t axis);
	void	(*EndRegistration)( void );

	// prepare frame to rendering
	bool	(*AddRefEntity)( entity_state_t *s1, entity_state_t *s2, float lerp );
	bool	(*AddDynLight)( vec3_t org, vec3_t color, float intensity );
	bool	(*AddParticle)( vec3_t org, float alpha, int color );
	bool	(*AddLightStyle)( int stylenum, vec3_t color );
	void	(*ClearScene)( void );

	void	(*BeginFrame)( void );
	void	(*RenderFrame)( refdef_t *fd );
	void	(*EndFrame)( void );

	void	(*SetColor)( const float *rgba );
	bool	(*ScrShot)( const char *filename, int shot_type ); // write screenshot with same name 
	void	(*DrawFill)(float x, float y, float w, float h );
	void	(*DrawStretchRaw)( int x, int y, int w, int h, int cols, int rows, byte *data, bool redraw );
	void	(*DrawStretchPic)( float x, float y, float w, float h, float s1, float t1, float s2, float t2, const char *name );

	void	(*DrawGetPicSize)( int *w, int *h, const char *name );

} render_exp_t;

typedef struct render_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(render_imp_t)

	// client fundamental callbacks
	void	(*StudioEvent)( dstudioevent_t *event, entity_state_t *ent );
	void	(*ShowCollision)( cmdraw_t callback );	// debug
	long	(*WndProc)( void *hWnd, uint uMsg, uint wParam, long lParam );
	entity_state_t *(*GetClientEdict)( int index );
	entity_state_t *(*GetLocalPlayer)( void );
	int	(*GetMaxClients)( void );
} render_imp_t;

/*
==============================================================================

PHYSIC.DLL INTERFACE
==============================================================================
*/
typedef struct physic_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(physic_exp_t)

	// initialize
	bool (*Init)( void );				// init all physic systems
	void (*Shutdown)( void );				// shutdown all render systems

	void (*LoadBSP)( const void *buf );			// load bspdata ( bsplib use this )
	void (*FreeBSP)( void );				// free bspdata
	void (*WriteCollisionLump)( file_t *f, cmsave_t callback );	// write collision data into LUMP_COLLISION

	void (*DrawCollision)( cmdraw_t callback );		// debug draw world
	void (*Frame)( float time );				// physics frame

	cmodel_t *(*BeginRegistration)( const char *name, bool clientload, uint *checksum );
	cmodel_t *(*RegisterModel)( const char *name );
	void (*EndRegistration)( void );

	void (*SetAreaPortals)( byte *portals, size_t size );
	void (*GetAreaPortals)( byte **portals, size_t *size );
	void (*SetAreaPortalState)( int area, int otherarea, bool open );

	int (*NumClusters)( void );
	int (*NumTextures)( void );
	int (*NumBmodels )( void );
	const char *(*GetEntityString)( void );
	const char *(*GetTextureName)( int index );
	byte *(*ClusterPVS)( int cluster );
	byte *(*ClusterPHS)( int cluster );
	int (*PointLeafnum)( vec3_t p );
	int (*BoxLeafnums)( vec3_t mins, vec3_t maxs, int *list, int listsize, int *lastleaf );
	int (*LeafCluster)( int leafnum );
	int (*LeafArea)( int leafnum );
	bool (*AreasConnected)( int area1, int area2 );
	int (*WriteAreaBits)( byte *buffer, int area );

	void (*ClipToGenericEntity)( trace_t *trace, cmodel_t *model, const vec3_t bodymins, const vec3_t bodymaxs, int bodysupercontents, matrix4x4 matrix, matrix4x4 inversematrix, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contentsmask );
	void (*ClipToWorld)( trace_t *trace, cmodel_t *model, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contentsmask );
	void (*CombineTraces)( trace_t *cliptrace, const trace_t *trace, edict_t *touch, bool is_bmodel );

	// player movement code
	void (*PlayerMove)( pmove_t *pmove, bool clientmove );
	
	// simple objects
	physbody_t *(*CreateBody)( sv_edict_t *ed, cmodel_t *mod, matrix4x3 transform, int solid );
	physbody_t *(*CreatePlayer)( sv_edict_t *ed, cmodel_t *mod, matrix4x3 transform );

	void (*SetOrigin)( physbody_t *body, vec3_t origin );
	void (*SetParameters)( physbody_t *body, cmodel_t *mod, int material, float mass );
	bool (*GetForce)(physbody_t *body, vec3_t vel, vec3_t avel, vec3_t force, vec3_t torque );
	void (*SetForce)(physbody_t *body, vec3_t vel, vec3_t avel, vec3_t force, vec3_t torque );
	bool (*GetMassCentre)( physbody_t *body, matrix3x3 mass );
	void (*SetMassCentre)( physbody_t *body, matrix3x3 mass );
	void (*RemoveBody)( physbody_t *body );
} physic_exp_t;

typedef struct physic_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(physic_imp_t)

	void (*ClientMove)( sv_edict_t *ed );
	void (*Transform)( sv_edict_t *ed, matrix4x3 transform );
	void (*PlaySound)( sv_edict_t *ed, float volume, const char *sample );
	float *(*GetModelVerts)( sv_edict_t *ent, int *numvertices );
} physic_imp_t;

/*
==============================================================================

VPROGS.DLL INTERFACE
==============================================================================
*/
#define PRVM_MAX_STACK_DEPTH		1024
#define PRVM_LOCALSTACK_SIZE		16384
#define PRVM_MAX_OPENFILES		256
#define PRVM_MAX_OPENSEARCHES		128

// prog flags
#define PRVM_OP_STATE		1
#define PRVM_FE_CHAIN		4
#define PRVM_FE_CLASSNAME		8
#define PRVM_OP_THINKTIME		16

enum
{
	PRVM_SERVERPROG = 0,
	PRVM_CLIENTPROG,
	PRVM_MENUPROG,
	PRVM_DECOMPILED,
	PRVM_MAXPROGS,	// must be last			
};

typedef void (*prvm_builtin_t)( void );
typedef struct edict_state_s
{
	bool	free;
	float	freetime;

} vm_edict_t;

struct edict_s
{
	// engine-private fields (stored in dynamically resized array)
	union
	{
		void			*vp;	// generic edict
		vm_edict_t		*ed;	// vm edict state 
		sv_edict_t		*sv;	// sv edict state
		cl_edict_t		*cl;	// cl edict state
		vm_edict_t		*ui;	// ui edict state
	} priv;

	// QuakeC prog fields (stored in dynamically resized array)
	union
	{
		void			*vp;	// generic entvars
		sv_entvars_t		*sv;	// server entvars
		cl_entvars_t		*cl;	// client entvars
		ui_entvars_t		*ui;	// uimenu entvars
	} progs;

};

typedef struct mfunction_s
{
	int		first_statement;	// negative numbers are builtins
	int		parm_start;
	int		locals;		// total ints of parms + locals

	// these are doubles so that they can count up to 54bits or so rather than 32bit
	double		profile;		// runtime
	double		builtinsprofile;	// cost of builtin functions called by this function
	double		callcount;	// times the functions has been called since the last profile call

	int		s_name;
	int		s_file;		// source file defined in
	int		numparms;
	byte		parm_size[MAX_PARMS];
} mfunction_t;

typedef struct prvm_stack_s
{
	int			s;
	mfunction_t		*f;
} prvm_stack_t;

typedef struct prvm_prog_s
{
	dprograms_t	*progs;
	mfunction_t	*functions;
	char		*strings;
	int		stringssize;
	ddef_t		*fielddefs;
	ddef_t		*globaldefs;
	dstatement_t	*statements;
	dsource_t		*sources;		// debug version include packed source files
	int		*linenums;	// debug versions only
	void		*types;		// (type_t *)
	int		edict_size;	// in bytes
	int		edictareasize;	// in bytes (for bound checking)
	int		pev_save;		// used by PRVM_PUSH_GLOBALS\PRVM_POP_GLOBALS
	int		other_save;	// used by PRVM_PUSH_GLOBALS\PRVM_POP_GLOBALS	
	int		*statement_linenums;// NULL if not available
	double		*statement_profile; // only incremented if prvm_statementprofiling is on

	union
	{
		float			*gp;
		sv_globalvars_t		*sv;
		cl_globalvars_t		*cl;
		ui_globalvars_t		*ui;
	} globals;

	int		maxknownstrings;
	int		numknownstrings;
	int		firstfreeknownstring;
	const char	**knownstrings;
	byte		*knownstrings_freeable;
	const char	***stringshash;
	byte		*progs_mempool;
	prvm_builtin_t	*builtins;
	int		numbuiltins;
	int		argc;
	int		trace;
	mfunction_t	*xfunction;
	int		xstatement;
	prvm_stack_t	stack[PRVM_MAX_STACK_DEPTH + 1];
	int		depth;
	int		localstack[PRVM_LOCALSTACK_SIZE];
	int		localstack_used;
	word		filecrc;
	int		intsize;
	vfile_t		*file[PRVM_MAX_OPENFILES];
	script_t		*script;
	search_t		*search;
	int		num_edicts;
	int		max_edicts;
	int		limit_edicts;
	int		reserved_edicts;
	edict_t		*edicts;
	void		*edictsfields;
	void		*edictprivate;
	int		edictprivate_size;
	float		*time;
	float		_time;
	bool		protect_world;
	bool		loadintoworld;
	bool		loaded;
	char		*name;
	int		flag;
	ddef_t		*pev; // if pev != 0 then there is a global pev

	// function pointers
	void		(*begin_increase_edicts)(void);
	void		(*end_increase_edicts)(void);
	void		(*init_edict)(edict_t *edict);
	void		(*free_edict)(edict_t *ed);
	void		(*count_edicts)(void);
	bool		(*load_edict)(edict_t *ent);		// initialize edict for first loading
	void		(*restore_edict)(edict_t *ent);	// restore edict from savegame or changelevel
	void		(*init_cmd)(void);
	void		(*reset_cmd)(void);
	void		(*error_cmd)(const char *format, ...);

} prvm_prog_t;

typedef struct vprogs_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vprogs_api_t)

	void ( *Init ) ( int argc, char **argv );	// init host
	void ( *Free ) ( void );			// close host

	// compiler functions
	void ( *PrepareDAT )( const char *dir, const char *name );
	void ( *CompileDAT )( void );
	void ( *Update )( dword time );		// refreshing compile, exec some programs e.t.c

	// edict operations
	edict_t *(*AllocEdict)( void );
	void (*FreeEdict)( edict_t *ed );
	void (*PrintEdict)( edict_t *ed );

	// savegame stuff
	void (*WriteGlobals)( void *buffer, void *ptr, setpair_t callback );
	void (*ReadGlobals)( int s_table, dkeyvalue_t *globals, int count );
	void (*WriteEdict)( edict_t *ed, void *buffer, void *ptr, setpair_t callback );
	void (*ReadEdict)( int s_table, int ednum, dkeyvalue_t *fields, int numpairs );

	// load ents description
	void (*LoadFromFile)( const char *data );

	// string manipulations
	const char *(*GetString)( int num );
	int (*SetEngineString)( const char *s );
	int (*SetTempString)( const char *s );

	void (*InitProg)( int prognr );
	void (*SetProg)( int prognr );
	bool (*ProgLoaded)( int prognr );
	void (*LoadProgs)( const char *filename );
	void (*ResetProg)( void );

	// abstract layer for searching globals and locals
	func_t (*FindFunctionOffset)( const char *function );
	int (*FindGlobalOffset)( const char *global );	
	int (*FindFieldOffset)( const char *field );	
	mfunction_t *(*FindFunction)( const char *name );
	ddef_t *(*FindGlobal)( const char *name );
	ddef_t *(*FindField)( const char *name );

	void (*StackTrace)( void );
	void (*Warning)( const char *fmt, ... );
	void (*Error)( const char *fmt, ... );

	void (*ExecuteProgram)( func_t fnum, const char *name, const char *file, const int line );
	void (*Crash)( void );	// crash virtual machine

	prvm_prog_t *prog;		// virtual machine current state

} vprogs_exp_t;

/*
==============================================================================

VSOUND.DLL INTERFACE
==============================================================================
*/
typedef struct vsound_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vprogs_api_t)

	void (*Init)( void *hInst );	// init host
	void (*Shutdown)( void );	// close host

	// sound manager
	void (*BeginRegistration)( void );
	sound_t (*RegisterSound)( const char *name );
	void (*EndRegistration)( void );

	void (*StartSound)( const vec3_t pos, int entnum, int channel, sound_t sfx, float vol, float attn, bool use_loop );
	void (*StreamRawSamples)( int samples, int rate, int width, int channels, const byte *data );
	bool (*AddLoopingSound)( int entnum, sound_t handle, float volume, float attn );
	bool (*StartLocalSound)( const char *name );
	void (*StartBackgroundTrack)( const char *introTrack, const char *loopTrack );
	void (*StopBackgroundTrack)( void );

	void (*StartStreaming)( void );
	void (*StopStreaming)( void );

	void (*Frame)( int entnum, const vec3_t pos, const vec3_t vel, const vec3_t at, const vec3_t up );
	void (*StopAllSounds)( void );
	void (*FreeSounds)( void );

	void (*Activate)( bool active );

} vsound_exp_t;

typedef struct vsound_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vsound_imp_t)

	void (*GetSoundSpatialization)( int entnum, vec3_t origin, vec3_t velocity );
	int  (*PointContents)( vec3_t point );
	void (*AddLoopingSounds)( void );

} vsound_imp_t;

// this is the only function actually exported at the linker level
typedef void *(*launch_t)( stdlib_api_t*, void* );
typedef struct { size_t api_size; } generic_api_t;

#endif//REF_DLLAPI_H