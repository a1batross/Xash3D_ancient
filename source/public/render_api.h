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

// screenshot types
#define VID_SCREENSHOT		0
#define VID_LEVELSHOT		1
#define VID_MINISHOT		2

// render supported extensions
#define R_WGL_SWAPCONTROL		1		
#define R_HARDWARE_GAMMA_CONTROL	2	
#define R_ARB_VERTEX_BUFFER_OBJECT_EXT	3
#define R_ENV_COMBINE_EXT		4
#define R_ARB_MULTITEXTURE		5
#define R_TEXTURECUBEMAP_EXT		6
#define R_DOT3_ARB_EXT		7
#define R_ANISOTROPY_EXT		8
#define R_TEXTURE_LODBIAS		9
#define R_OCCLUSION_QUERIES_EXT	10
#define R_TEXTURE_COMPRESSION_EXT	11
#define R_SHADER_GLSL100_EXT		12

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

// hold values that needs for right studio lerping
typedef struct
{
	float		frame;
	float		animtime; 
	int		sequence;
	float		sequencetime;

	// CLIENT SPECIFIC
	vec3_t		gaitorigin;		// client oldorigin used to calc velocity
	float		gaitframe;		// client->frame + yaw
	float		gaityaw;			// local value

	// EF_ANIMATE stuff
	int		m_fSequenceLoops;		// sequence is looped
	int		m_fSequenceFinished;	// sequence is finished
	float		m_flFrameRate;     		// looped sequence framerate
	float		m_flGroundSpeed;   		// looped sequence ground speed (movement)
	float		m_flLastEventCheck;		// last time when event is checked

          float		curanimtime;		// HACKHACK current animtime	
	float		curframe;			// HACKHACK current frame

	int		cursequence;		// HACKHACK current sequence
	byte		curblending[16];		// HACKHACK current blending
	byte		curcontroller[16];		// HACKHACL current blending

	byte		blending[16];		// previous blending values
	byte		controller[16];		// previous controller values
	byte		seqblending[16];		// blending between sequence when it's changed
} studioframe_t;

typedef struct
{
	char		*name;
	byte		*mempool;

	droqchunk_t	chunk;
	dcell_t		cells[256];
	dquadcell_t	qcells[256];
	
	byte		*vid_buffer; 
	byte		*vid_pic[2]; 
	byte		*pic;
	byte		*pic_pending;

	bool		new_frame;

	int		s_rate;
	int		s_width;
	int		s_channels;

	int		width;
	int		height;

	file_t		*file;
	int		headerlen;

	uint		time;			// Sys_Milliseconds for first cinematic frame
	uint		frame;
} cinematics_t;

/*
==============================================================================

RENDER.DLL INTERFACE
==============================================================================
*/
typedef struct render_exp_s
{
	// interface validator
	size_t	api_size;			// must matched with sizeof(render_exp_t)
	size_t	com_size;			// must matched with sizeof(stdlib_api_t)

	// initialize
	bool	(*Init)( bool full );	// init all render systems
	void	(*Shutdown)( bool full );	// shutdown all render systems

	void	(*BeginRegistration)( const char *map, const dvis_t *visData );
	bool	(*RegisterModel)( const char *name, int cl_index ); // also build replacement index table
	shader_t	(*RegisterShader)( const char *name, int shaderType );
	void	(*EndRegistration)( const char *skyname );
	void	(*FreeShader)( const char *shadername );

	// prepare frame to rendering
	bool	(*AddRefEntity)( edict_t *pRefEntity, int ed_type );
	bool	(*AddDynLight)( const void *dlight );
	bool	(*AddPolygon)( const poly_t *poly );
	bool	(*AddLightStyle)( int stylenum, vec3_t color );
	void	(*ClearScene)( void );

	void	(*BeginFrame)( bool clearScene );
	void	(*RenderFrame)( const ref_params_t *fd );
	void	(*EndFrame)( void );

	// triapi implementation
	void	(*RenderMode)( const kRenderMode_t mode );
	void	(*Normal3f)( const float x, const float y, const float z );
	void	(*Vertex3f)( const float x, const float y, const float z );
	void	(*Color4ub)( const byte r, const byte g, const byte b, const byte a );
	void	(*Fog)( float flFogColor[3], float flStart, float flEnd, int bOn );
	void	(*TexCoord2f)( const float u, const float v );
	void	(*Bind)( shader_t shader, int frame );
	void	(*CullFace)( int mode );
	void	(*Enable)( int cap );
	void	(*Disable)( int cap );
	void	(*Begin)( int mode );
	void	(*End)( void );

	// misc utilities
	void	(*SetColor)( const rgba_t color );
	void	(*SetParms)( shader_t handle, kRenderMode_t rendermode, int frame );
	void	(*GetParms)( int *w, int *h, int *frames, int frame, shader_t shader );
	bool	(*ScrShot)( const char *filename, int shot_type ); // write screenshot with same name 
	bool	(*EnvShot)( const char *filename, uint size, bool skyshot ); // write envshot with same name 
	void	(*LightForPoint)( const vec3_t point, vec3_t ambientLight );
	void	(*DrawStretchRaw)( int x, int y, int w, int h, int cols, int rows, byte *data, bool redraw );
	void	(*DrawStretchPic)( float x, float y, float w, float h, float s1, float t1, float s2, float t2, shader_t shader );
	int	(*GetFragments)( const vec3_t org, float rad, vec3_t axis[3], int maxverts, vec3_t *verts, int maxfrags, fragment_t *frags );
	int	(*WorldToScreen)( const float *world, float *screen );
	void	(*ScreenToWorld)( const float *screen, float *world );
	bool 	(*RSpeedsMessage)( char *out, size_t size );
	bool	(*Support)( int extension );
} render_exp_t;

typedef struct render_imp_s
{
	// interface validator
	size_t	api_size;			// must matched with sizeof(render_imp_t)

	// client fundamental callbacks
	void	(*UpdateScreen)( void );	// update screen while loading
	void	(*StudioEvent)( dstudioevent_t *event, edict_t *ent );
	void	(*StudioFxTransform)( edict_t *ent, float matrix[4][4] );
	void	(*ShowCollision)( cmdraw_t callback );	// debug
	long	(*WndProc)( void *hWnd, uint uMsg, uint wParam, long lParam );
	bool	(*GetAttachment)( int entityIndex, int number, vec3_t origin, vec3_t angles );
	bool	(*SetAttachment)( int entityIndex, int number, vec3_t origin, vec3_t angles );
	edict_t	*(*GetClientEdict)( int index );
	studioframe_t *(*GetStudioFrame)( int entityIndex );
	byte	(*GetMouthOpen)( int entityIndex );
	edict_t	*(*GetLocalPlayer)( void );
	int	(*GetMaxClients)( void );
	float	(*GetLerpFrac)( void );
	void	(*DrawTriangles)( int fTrans );

	// RoQ decoder imports
	void	(*RoQ_ReadChunk)( cinematics_t *cin );
	byte	*(*RoQ_ReadNextFrame)( cinematics_t *cin, bool silent );
} render_imp_t;

#endif//RENDER_API_H