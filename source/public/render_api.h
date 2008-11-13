//=======================================================================
//			Copyright XashXT Group 2008 �
//		         render_api.h - xash renderer api
//=======================================================================
#ifndef RENDER_API_H
#define RENDER_API_H

// shader types used for shader loading
#define SHADER_SKY			0	// sky box shader
#define SHADER_FONT			1	// speical case for displayed fonts
#define SHADER_NOMIP		2	// 2d images

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
	bool	(*RegisterModel)( const char *name, int cl_index ); // also build replacement index table
	shader_t	(*RegisterShader)( const char *name, int shaderType );
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
	bool	(*EnvShot)( const char *filename, uint size, bool skyshot ); // write envshot with same name 
	void	(*DrawFill)( float x, float y, float w, float h );
	void	(*DrawStretchRaw)( int x, int y, int w, int h, int cols, int rows, byte *data, bool redraw );
	void	(*DrawStretchPic)( float x, float y, float w, float h, float s1, float t1, float s2, float t2, shader_t shader );

	void	(*DrawGetPicSize)( int *w, int *h, shader_t shader );

} render_exp_t;

typedef struct render_imp_s
{
	// interface validator
	size_t	api_size;			// must matched with sizeof(render_imp_t)

	// client fundamental callbacks
	void	(*UpdateScreen)( void );	// update screen while loading
	void	(*StudioEvent)( dstudioevent_t *event, entity_state_t *ent );
	void	(*ShowCollision)( cmdraw_t callback );	// debug
	long	(*WndProc)( void *hWnd, uint uMsg, uint wParam, long lParam );
	entity_state_t *(*GetClientEdict)( int index );
	entity_state_t *(*GetLocalPlayer)( void );
	int	(*GetMaxClients)( void );
} render_imp_t;

#endif//RENDER_API_H