//=======================================================================
//			Copyright XashXT Group 2007 ©
//		  dllapi.h - shared ifaces between engine parts
//=======================================================================
#ifndef REF_SHARED_H
#define REF_SHARED_H

#include "stdref.h"

#define MAX_LIGHTSTYLES	256

#define	EF_TELEPORTER	(1<<0)		// particle fountain
#define	EF_ROTATE		(1<<1)		// rotate (bonus items)

// shared client/renderer flags
#define	RF_MINLIGHT	1		// allways have some light (viewmodel)
#define	RF_VIEWERMODEL	2		// don't draw through eyes, only mirrors
#define	RF_WEAPONMODEL	4		// only draw through eyes
#define	RF_FULLBRIGHT	8		// allways draw full intensity
#define	RF_DEPTHHACK	16		// for view weapon Z crunching
#define	RF_TRANSLUCENT	32
#define	RF_FRAMELERP	64
#define	RF_BEAM		128
#define	RF_IR_VISIBLE	256		// skin is an index in image_precache
#define	RF_GLOW		512		// pulse lighting for bonus items

// render private flags
#define	RDF_NOWORLDMODEL	1		// used for player configuration screen
#define	RDF_IRGOGGLES	2
#define	RDF_PAIN           	4

// encoded bmodel mask
#define SOLID_BMODEL	0xffffff

// content masks
#define MASK_ALL		(-1)
#define MASK_SOLID		(CONTENTS_SOLID|CONTENTS_WINDOW)
#define MASK_PLAYERSOLID	(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define MASK_DEADSOLID	(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW)
#define MASK_MONSTERSOLID	(CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define MASK_WATER		(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define MASK_OPAQUE		(CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA)
#define MASK_SHOT		(CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEADMONSTER)
#define MASK_CURRENT	(CONTENTS_CURRENT_0|CONTENTS_CURRENT_90|CONTENTS_CURRENT_180|CONTENTS_CURRENT_270|CONTENTS_CURRENT_UP|CONTENTS_CURRENT_DOWN)

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
#define	BUTTON_USE	2
#define	BUTTON_ATTACK2	4
#define	BUTTONS_ATTACK	(BUTTON_ATTACK | BUTTON_ATTACK2)
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

// player_state->stats[] indexes
enum player_stats
{
	STAT_HEALTH_ICON = 0,
	STAT_HEALTH,
	STAT_AMMO_ICON,
	STAT_AMMO,
	STAT_ARMOR_ICON,
	STAT_ARMOR,
	STAT_SELECTED_ICON,
	STAT_PICKUP_ICON,
	STAT_PICKUP_STRING,
	STAT_TIMER_ICON,
	STAT_TIMER,
	STAT_HELPICON,
	STAT_SELECTED_ITEM,
	STAT_LAYOUTS,
	STAT_FRAGS,
	STAT_FLASHES,		// cleared each frame, 1 = health, 2 = armor
	STAT_CHASE,
	STAT_SPECTATOR,
	STAT_SPEED = 22,
	STAT_ZOOM,
	MAX_STATS = 32,
};

typedef enum 
{
	R_BEAM,
	R_SPRITE,
	R_POLYGON,
	R_BSPMODEL,
	R_VIEWMODEL,			// studio model that drawn trough eyes
	R_STUDIOMODEL,
	R_TYPES_COUNT,
} reftype_t;

typedef struct dlight_s
{
	vec3_t	origin;
	vec3_t	color;
	float	intensity;

} dlight_t;

typedef struct particle_s
{
	vec3_t	origin;
	int	color;
	float	alpha;

} particle_t;

typedef struct lightstyle_s
{
	float	rgb[3];		// 0.0 - 2.0
	float	white;		// highest of rgb

} lightstyle_t;

typedef struct latchedvars_s
{
	float		animtime;
	float		sequencetime;
	vec3_t		origin;
	vec3_t		angles;		

	int		sequence;
	float		frame;

	byte		blending[MAXSTUDIOBLENDS];
	byte		seqblending[MAXSTUDIOBLENDS];
	byte		controller[MAXSTUDIOCONTROLLERS];

} latchedvars_t;

// client entity
typedef struct entity_s
{
	model_t		*model;		// opaque type outside refresh
	model_t		*weaponmodel;	// opaque type outside refresh	

	latchedvars_t	prev;		// previous frame values for lerping
	
	vec3_t		angles;
	vec3_t		origin;		// also used as RF_BEAM's "from"
	float		oldorigin[3];	// also used as RF_BEAM's "to"

          float		animtime;	
	float		frame;		// also used as RF_BEAM's diameter
	float		framerate;

	int		body;
	int		skin;
	
	byte		blending[MAXSTUDIOBLENDS];
	byte		controller[MAXSTUDIOCONTROLLERS];
	byte		mouth;		//TODO: move to struct
	
          int		movetype;		//entity moving type
	int		sequence;
	float		scale;
	
	vec3_t		attachment[MAXSTUDIOATTACHMENTS];
	
	// misc
	float		backlerp;		// 0.0 = current, 1.0 = old
	int		skinnum;		// also used as RF_BEAM's palette index

	int		lightstyle;	// for flashing entities
	float		alpha;		// ignore if RF_TRANSLUCENT isn't set

	image_t		*image;		// NULL for inline skin
	int		flags;

} entity_t;

typedef struct
{
	int		x, y, width, height;// in virtual screen coordinates
	float		fov_x, fov_y;
	float		vieworg[3];
	float		viewangles[3];
	float		blend[4];		// rgba 0-1 full screen blend
	float		time;		// time is used to auto animate
	int		rdflags;		// RDF_UNDERWATER, etc

	byte		*areabits;	// if not NULL, only areas with set bits will be drawn

	lightstyle_t	*lightstyles;	// [MAX_LIGHTSTYLES]

	int		num_entities;
	entity_t		*entities;

	int		num_dlights;
	dlight_t		*dlights;

	int		num_particles;
	particle_t	*particles;

} refdef_t;

// viewmodel state
typedef struct vmodel_state_s
{
	int		index;	// client modelindex
	vec3_t		angles;	// can be some different with viewangles
	vec3_t		offset;	// center offset
	int		sequence;	// studio animation sequence
	float		frame;	// studio frame
	int		body;	// weapon body
	int		skin;	// weapon skin 
} vmodel_state_t;

// thirdperson model state
typedef struct pmodel_state_s
{
	int		index;	// client modelindex
	int		sequence;	// studio animation sequence
	int		frame;	// studio frame
} pmodel_state_t;

// player_state_t communication
typedef struct player_state_s
{
	int		bobcycle;		// for view bobbing and footstep generation
	float		bobtime;
	byte		pm_type;		// player movetype
	byte		pm_flags;		// ducked, jump_held, etc
	byte		pm_time;		// each unit = 8 ms

	vec3_t		origin;
	vec3_t		velocity;
	vec3_t		delta_angles;	// add to command angles to get view direction
	short		gravity;		// gravity value
	short		speed;		// maxspeed
	edict_t		*groundentity;	// current ground entity
	int		viewheight;	// height over ground
	int		effects;		// copied to entity_state_t->effects
	int		weapon;		// copied to entity_state_t->weapon
	vec3_t		viewangles;	// for fixed views
	vec3_t		viewoffset;	// add to pmovestate->origin
	vec3_t		kick_angles;	// add to view direction to get render angles
	vec3_t		oldviewangles;	// for lerping viewmodel position
	vec4_t		blend;		// rgba full screen effect
	short		stats[MAX_STATS];
	float		fov;		// horizontal field of view

	// player model and viewmodel
	vmodel_state_t	vmodel;
	pmodel_state_t	pmodel;

} player_state_t;

// user_cmd_t communication
typedef struct usercmd_s
{
	byte		msec;
	byte		buttons;
	short		angles[3];
	byte		impulse;		// remove?
	byte		lightlevel;	// light level the player is standing on
	short		forwardmove, sidemove, upmove;
} usercmd_t;

typedef struct pmove_s
{
	player_state_t	ps;		// state (in / out)

	// command (in)
	usercmd_t		cmd;

	physbody_t	*body;		// pointer to physobject

	// results (out)
	int		numtouch;
	edict_t		*touchents[PM_MAXTOUCH];	// max touch

	vec3_t		mins, maxs;	// bounding box size
	int		watertype;
	int		waterlevel;
	float		xyspeed;		// avoid to compute it twice

	// callbacks to test the world
	trace_t		(*trace)( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end );
	int		(*pointcontents)( vec3_t point );
} pmove_t;

typedef struct sizebuf_s
{
	bool	overflowed;	// set to true if the buffer size failed
	byte	*data;
	int	maxsize;
	int	cursize;
	int	readcount;
	int	errorcount;		// cause by errors
} sizebuf_t;

/*
==============================================================================

LAUNCH.DLL INTERFACE
==============================================================================
*/
typedef struct launch_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(launch_api_t)

	void ( *Init ) ( uint funcname, int argc, char **argv );	// init host
	void ( *Main ) ( void );				// host frame
	void ( *Free ) ( void );				// close host
	void ( *Cmd  ) ( void );				// forward cmd to server
	void (*CPrint) ( const char *msg );			// host print
} launch_exp_t;

/*
==============================================================================

IMGLIB.DLL INTERFACE
==============================================================================
*/
typedef struct imglib_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(imglib_api_t)

	void ( *Init ) ( uint funcname );	// init host
	void ( *Free ) ( void );		// close host

	// global operations
	rgbdata_t *(*LoadImage)(const char *path, char *data, int size );	// extract image into rgba buffer
	void (*SaveImage)(const char *filename, rgbdata_t *buffer );	// save image into specified format

	bool (*DecompressDXTC)( rgbdata_t **image );
	bool (*DecompressARGB)( rgbdata_t **image );

	bool (*ResampleImage)( const char *name, rgbdata_t **pix, int w, int h, bool free_baseimage ); // resample image
	void (*FreeImage)( rgbdata_t *pack );				// free image buffer

} imglib_exp_t;

/*
==============================================================================

RENDER.DLL INTERFACE
==============================================================================
*/
typedef struct render_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(render_exp_t)

	// initialize
	bool (*Init)( void *hInst );	// init all render systems
	void (*Shutdown)( void );	// shutdown all render systems

	void	(*BeginRegistration) (char *map);
	model_t	*(*RegisterModel) (char *name);
	image_t	*(*RegisterSkin) (char *name);
	image_t	*(*RegisterPic) (char *name);
	void	(*SetSky) (char *name, float rotate, vec3_t axis);
	void	(*EndRegistration) (void);

	void	(*BeginFrame)( void );
	void	(*RenderFrame) (refdef_t *fd);
	void	(*EndFrame)( void );

	void	(*SetColor)( const float *rgba );
	bool	(*ScrShot)( const char *filename, bool levelshot ); // write screenshot with same name 
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
	void	(*StudioEvent)( mstudioevent_t *event, entity_t *ent );
	void	(*ShowCollision)( cmdraw_t callback );	// debug
	long	(*WndProc)( void *hWnd, uint uMsg, uint wParam, long lParam );
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
	void (*SetAreaPortalState)( int portalnum, bool open );

	int (*NumClusters)( void );
	int (*NumTextures)( void );
	int (*NumBmodels )( void );
	const char *(*GetEntityString)( void );
	const char *(*GetTextureName)( int index );
	int (*PointContents)( const vec3_t p, cmodel_t *model );
	int (*TransformedPointContents)( const vec3_t p, cmodel_t *model, const vec3_t origin, const vec3_t angles );
	trace_t (*BoxTrace)( const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, cmodel_t *model, int brushmask, bool capsule );
	trace_t (*TransformedBoxTrace)( const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, cmodel_t *model, int brushmask, vec3_t origin, vec3_t angles, bool capsule );
	byte *(*ClusterPVS)( int cluster );
	byte *(*ClusterPHS)( int cluster );
	int (*PointLeafnum)( vec3_t p );
	int (*BoxLeafnums)( vec3_t mins, vec3_t maxs, int *list, int listsize, int *lastleaf );
	int (*LeafCluster)( int leafnum );
	int (*LeafArea)( int leafnum );
	bool (*AreasConnected)( int area1, int area2 );
	int (*WriteAreaBits)( byte *buffer, int area );

	// player movement code
	void (*PlayerMove)( pmove_t *pmove, bool clientmove );

	void (*ServerMove)( pmove_t *pmove );
	void (*ClientMove)( pmove_t *pmove );
	
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
typedef struct vprogs_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vprogs_api_t)

	void ( *Init ) ( uint funcname, int argc, char **argv );	// init host
	void ( *Free ) ( void );				// close host

	// compiler functions
	void ( *PrepareDAT )( const char *dir, const char *name );
	void ( *CompileDAT )( void );

	// edict operations
	void (*WriteGlobals)( vfile_t *f );
	void (*ParseGlobals)( const char *data );
	void (*PrintEdict)( edict_t *ed );
	void (*WriteEdict)( vfile_t *f, edict_t *ed );
	const char *(*ParseEdict)( const char *data, edict_t *ed );
	edict_t *(*AllocEdict)( void );
	void (*FreeEdict)( edict_t *ed );
	void (*IncreaseEdicts)( void );

	// load ents description
	void (*LoadFromFile)( const char *data );

	// string manipulations
	const char *(*GetString)( int num );
	int (*SetEngineString)( const char *s );
	int (*SetTempString)( const char *s );
	int (*AllocString)( size_t bufferlength, char **pointer );
	void (*FreeString)( int num );

	void (*InitProg)( int prognr );
	void (*SetProg)( int prognr );
	void (*LoadProgs)( const char *filename, int numrequiredfunc, char **required_func, int numrequiredfields, fields_t *required_field );
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

	void (*ExecuteProgram)( func_t fnum, const char *errormessage );
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

#endif//REF_SHARED_H