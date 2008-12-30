	//=======================================================================
//			Copyright XashXT Group 2008 ©
//		         render_api.h - xash renderer api
//=======================================================================
#ifndef RENDER_API_H
#define RENDER_API_H

// shader types used for shader loading
#define SHADER_SKY			0	// sky box shader
#define SHADER_FONT			1	// speical case for displayed fonts
#define SHADER_NOMIP		2	// 2d images
#define SHADER_GENERIC		3	// generic shader

typedef struct
{
	vec3_t		point;
	vec4_t		modulate;
	vec2_t		st;
} polyVert_t;

typedef struct
{
	int		firstVert;
	int		numVerts;
} decalFragment_t;

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

	void	(*BeginRegistration)( const char *map );
	bool	(*RegisterModel)( const char *name, int cl_index ); // also build replacement index table
	shader_t	(*RegisterShader)( const char *name, int shaderType );
	void	(*EndRegistration)( const char *skyname );

	// prepare frame to rendering
	bool	(*AddRefEntity)( entity_state_t *s1, entity_state_t *s2, float lerp );
	bool	(*AddDynLight)( vec3_t org, vec3_t color, float intensity );
	bool	(*AddParticle)( shader_t shader, const vec3_t p1, const vec3_t p2, float rad, float len, float rot, int col );
	bool	(*AddPolygon)( shader_t shader, int numVerts, const polyVert_t *verts );
	bool	(*AddLightStyle)( int stylenum, vec3_t color );
	void	(*ClearScene)( void );

	void	(*BeginFrame)( void );
	void	(*RenderFrame)( ref_params_t *fd );
	void	(*EndFrame)( void );

	// misc utilities
	void	(*SetColor)( const float *rgba );
	void	(*SetParms)( shader_t handle, kRenderMode_t rendermode, int frame );
	bool	(*ScrShot)( const char *filename, int shot_type ); // write screenshot with same name 
	bool	(*EnvShot)( const char *filename, uint size, bool skyshot ); // write envshot with same name 
	void	(*LightForPoint)( const vec3_t point, vec3_t ambientLight );
	void	(*DrawFill)( float x, float y, float w, float h );
	void	(*DrawStretchRaw)( int x, int y, int w, int h, int cols, int rows, byte *data, bool redraw );
	void	(*DrawStretchPic)( float x, float y, float w, float h, float s1, float t1, float s2, float t2, shader_t shader );
	void	(*ImpactMark)( vec3_t org, vec3_t dir, float rot, float radius, vec4_t mod, bool fade, shader_t s, bool tmp );
	void	(*DrawGetPicSize)( int *w, int *h, int frame, shader_t shader );

} render_exp_t;

typedef struct render_imp_s
{
	// interface validator
	size_t	api_size;			// must matched with sizeof(render_imp_t)

	// client fundamental callbacks
	void	(*UpdateScreen)( void );	// update screen while loading
	void	(*StudioEvent)( dstudioevent_t *event, entity_state_t *ent );
	void	(*AddDecal)( vec3_t org, matrix3x3 m, shader_t s, vec4_t rgba, bool fade, decalFragment_t *df, const vec3_t *v );
	void	(*ShowCollision)( cmdraw_t callback );	// debug
	long	(*WndProc)( void *hWnd, uint uMsg, uint wParam, long lParam );
	entity_state_t *(*GetClientEdict)( int index );
	entity_state_t *(*GetLocalPlayer)( void );
	int	(*GetMaxClients)( void );
} render_imp_t;

#endif//RENDER_API_H