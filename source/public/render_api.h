//=======================================================================
//			Copyright XashXT Group 2008 ©
//		         render_api.h - xash renderer api
//=======================================================================
#ifndef RENDER_API_H
#define RENDER_API_H

#include "ref_params.h"

// shader types used for shader loading
#define SHADER_SKY			1	// sky box shader
#define SHADER_FONT			2	// special case for displayed fonts
#define SHADER_NOMIP		3	// 2d images
#define SHADER_GENERIC		4	// generic shader

// refdef flags
#define RDF_NOWORLDMODEL		BIT(0) 	// used for player configuration screen
#define RDF_OLDAREABITS		BIT(1) 	// forces R_MarkLeaves if not set
#define RDF_PORTALINVIEW		BIT(2)	// cull entities using vis too because pvs\areabits are merged serverside
#define RDF_SKYPORTALINVIEW		BIT(3)	// draw skyportal instead of regular sky
#define RDF_NOFOVADJUSTMENT		BIT(4)	// do not adjust fov for widescreen
#define RDF_WORLDOUTLINES		BIT(5)	// draw cell outlines for world surfaces

typedef struct
{
	int		numverts;
	vec3_t		*verts;
	vec2_t		*stcoords;
	rgba_t		*colors;

	union
	{
		struct ref_shader_s	*shader;
		shader_t		shadernum;
	};

	int		fognum;
	vec3_t		normal;
} poly_t;

typedef struct
{
	int		firstvert;
	int		numverts;	// can't exceed MAX_POLY_VERTS
	int		fognum;	// -1 - do not bother adding fog later at rendering stage
	   			//  0 - determine fog later
	   			// >0 - valid fog volume number returned by R_GetClippedFragments
	vec3_t		normal;
} fragment_t;

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
	bool	(*Init)( bool full );	// init all render systems
	void	(*Shutdown)( bool full );	// shutdown all render systems

	void	(*BeginRegistration)( const char *map, const dvis_t *visData );
	bool	(*RegisterModel)( const char *name, int cl_index ); // also build replacement index table
	shader_t	(*RegisterShader)( const char *name, int shaderType );
	void	(*EndRegistration)( const char *skyname );

	// prepare frame to rendering
	bool	(*AddRefEntity)( edict_t *pRefEntity, int ed_type, float lerp );
	bool	(*AddDynLight)( vec3_t org, vec3_t color, float intensity, shader_t shader );
	bool	(*AddPolygon)( const poly_t *poly );
	bool	(*AddLightStyle)( int stylenum, vec3_t color );
	void	(*ClearScene)( void );

	void	(*BeginFrame)( void );
	void	(*RenderFrame)( ref_params_t *fd );
	void	(*EndFrame)( void );

	// misc utilities
	void	(*SetColor)( const void *color );
	void	(*SetParms)( shader_t handle, kRenderMode_t rendermode, int frame );
	void	(*GetParms)( int *w, int *h, int *frames, int frame, shader_t shader );
	bool	(*ScrShot)( const char *filename, int shot_type ); // write screenshot with same name 
	bool	(*EnvShot)( const char *filename, uint size, bool skyshot ); // write envshot with same name 
	void	(*LightForPoint)( const vec3_t point, vec3_t ambientLight );
	void	(*DrawFill)( float x, float y, float w, float h );
	void	(*DrawStretchRaw)( int x, int y, int w, int h, int cols, int rows, byte *data, bool redraw );
	void	(*DrawStretchPic)( float x, float y, float w, float h, float s1, float t1, float s2, float t2, shader_t shader );
	int	(*GetFragments)( const vec3_t org, float rad, vec3_t axis[3], int maxverts, vec3_t *verts, int maxfrags, fragment_t *frags );
} render_exp_t;

typedef struct render_imp_s
{
	// interface validator
	size_t	api_size;			// must matched with sizeof(render_imp_t)

	// client fundamental callbacks
	void	(*UpdateScreen)( void );	// update screen while loading
	void	(*StudioEvent)( dstudioevent_t *event, edict_t *ent );
	void	(*ShowCollision)( cmdraw_t callback );	// debug
	bool	(*Trace)( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end );
	long	(*WndProc)( void *hWnd, uint uMsg, uint wParam, long lParam );
	edict_t	*(*GetClientEdict)( int index );
	edict_t	*(*GetLocalPlayer)( void );
	int	(*GetMaxClients)( void );
} render_imp_t;

#endif//RENDER_API_H