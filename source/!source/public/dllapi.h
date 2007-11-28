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

/*
==============================================================================

LAUNCH.DLL INTERFACE
==============================================================================
*/
typedef struct launch_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(launch_api_t)

	void ( *Init ) ( uint funcname, int argc, char **argv ); // init host
	void ( *Main ) ( void ); // host frame
	void ( *Free ) ( void ); // close host

} launch_exp_t;

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
	bool (*Init)( void *hInstance, void *WndProc );	// init all render systems
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
	bool	(*ScrShot)( const char *filename, bool force_gamma ); // write screenshot with same name 
	void	(*DrawFill)(float x, float y, float w, float h );
	void	(*DrawStretchRaw) (int x, int y, int w, int h, int cols, int rows, byte *data, bool redraw );
	void	(*DrawStretchPic)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, char *name);

	void	(*DrawGetPicSize) (int *w, int *h, char *name);	// get rid of this

} render_exp_t;

typedef struct render_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(render_imp_t)

	// client fundamental callbacks
	void	(*StudioEvent)( mstudioevent_t *event, entity_t *ent );
	void	(*CM_DrawCollision)( cmdraw_t drawfunc );
	void	(*ShowCollision)( void );	// debug

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
	bool (*Init)( void );	// init all physic systems
	void (*Shutdown)( void );	// shutdown all render systems

	void (*LoadBSP)( uint *buf );	// generate tree collision
	void (*FreeBSP)( void );	// release tree collision
	void (*DrawCollision)( cmdraw_t callback );// debug
	void (*Frame)( float time );	// physics frame

	// simple objects
	void (*CreateBody)( sv_edict_t *ed, vec3_t mins, vec3_t maxs, vec3_t org, vec3_t ang, int solid, NewtonCollision **newcol, NewtonBody **newbody );
	void (*RemoveBody)( NewtonBody *body );

} physic_exp_t;

typedef struct physic_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(physic_imp_t)

	void (*Transform)( sv_edict_t *ed, vec3_t origin, vec3_t angles );
	float *(*GetModelVerts)( sv_edict_t *ent, int *numvertices );

} physic_imp_t;

// this is the only function actually exported at the linker level
typedef void *(*launch_t)( stdlib_api_t*, void* );

#endif//REF_SHARED_H