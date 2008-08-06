//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        r_local.h - render internal types
//=======================================================================

#ifndef R_LOCAL_H
#define R_LOCAL_H

// limits
#define MAX_TEXTURES		4096
#define MAX_PROGRAMS		512
#define MAX_SHADERS			1024
#define MAX_LIGHTMAPS		128
#define MAX_VERTEX_BUFFERS		2048

typedef enum
{
	R_OPENGL_110 = 0,		// base
	R_WGL_SWAPCONTROL,
	R_COMBINE_EXT,
	R_DRAWRANGEELMENTS,
	R_ARB_MULTITEXTURE,
	R_LOCKARRAYS_EXT,
	R_TEXTURE_3D_EXT,
	R_TEXTURECUBEMAP_EXT,
	R_DOT3_ARB_EXT,
	R_CLAMPTOEDGE_EXT,
	R_ANISOTROPY_EXT,
	R_BLEND_MINMAX_EXT,
	R_STENCILTWOSIDE_EXT,
	R_BLEND_SUBTRACT_EXT,
	R_SHADER_OBJECTS_EXT,
	R_SHADER_GLSL100_EXT,
	R_VERTEX_SHADER_EXT,
	R_FRAGMENT_SHADER_EXT,
	R_EXT_POINTPARAMETERS,
	R_SEPARATESTENCIL_EXT,
	R_ARB_TEXTURE_NPOT_EXT,
	R_ARB_VERTEX_BUFFER_OBJECT_EXT,
	R_CUSTOM_VERTEX_ARRAY_EXT,
	R_TEXTURE_COMPRESSION_EXT,
	R_EXTCOUNT
} r_opengl_extensions;

typedef struct
{
	byte		open;		// 0 = mouth closed, 255 = mouth agape
	byte		sndcount;		// counter for running average
	int		sndavg;		// running average
} mouth_t;

typedef struct latchedvars_s
{
	float		animtime;
	float		sequencetime;
	vec3_t		origin;
	vec3_t		angles;		

	vec3_t		gaitorigin;
	int		sequence;
	float		frame;

	byte		blending[MAXSTUDIOBLENDS];
	byte		seqblending[MAXSTUDIOBLENDS];
	byte		controller[MAXSTUDIOCONTROLLERS];

} latchedvars_t;

// client entity
typedef struct ref_entity_s
{
	edtype_t		ent_type;		// entity type
	int		index;		// entity index
	rmodel_t		*model;		// opaque type outside refresh
	rmodel_t		*weaponmodel;	// opaque type outside refresh	

	latchedvars_t	prev;		// previous frame values for lerping
	
	vec3_t		angles;
	vec3_t		origin;		// position

	float		framerate;	// custom framerate
          float		animtime;		// lerping animtime	
	float		frame;		// also used as RF_BEAM's diameter

	int		body;
	int		skin;
	
	byte		blending[MAXSTUDIOBLENDS];
	vec3_t		attachment[MAXSTUDIOATTACHMENTS];
	byte		controller[MAXSTUDIOCONTROLLERS];
	mouth_t		mouth;		// for synchronizing mouth movements.
	
          int		movetype;		// entity moving type
	int		sequence;
	float		scale;
	
	// misc
	float		backlerp;		// 0.0 = current, 1.0 = old
	vec3_t		rendercolor;	// hl1 rendercolor
	float		renderamt;	// hl1 alphavalues
	int		renderfx;		// server will be translate hl1 values into flags
	int		colormap;		// q1 and hl1 model colormap (can applied for sprites)
	int		effects;		// q1 effect flags, EF_ROTATE, EF_DIMLIGHT etc

	// these values will be calculated locally, not from entity_state
	vec3_t		realangles;
	int		m_fSequenceLoops;
	int		m_fSequenceFinished;
	int		renderframe;	// using for gait cycle	
	int		gaitsequence;	// client->sequence + yaw
	float		gaitframe;	// client->frame + yaw
	float		gaityaw;		// local value
} ref_entity_t;

typedef struct lightstyle_s
{
	float		rgb[3];		// 0.0 - 2.0
	float		white;		// highest of rgb
} lightstyle_t;

typedef struct particle_s
{
	vec3_t	origin;
	int	color;
	float	alpha;

} particle_t;

typedef struct dlight_s
{
	vec3_t	origin;
	vec3_t	color;
	float	intensity;
} dlight_t;

/*
 =======================================================================

 GL STATE MANAGER

 =======================================================================
*/
typedef struct glstate_s
{
	bool		orthogonal;
	word		gammaRamp[768];		// current gamma ramp
	word		stateRamp[768];		// original gamma ramp
	uint		screenTexture;

	uint		activeTMU;
	int		*texNum;
	int		*texEnv;

	// render current state
	bool		cullFace;
	bool		polygonOffsetFill;
	bool		vertexProgram;
	bool		fragmentProgram;
	bool		alpha_test;
	bool		depth_test;
	bool		blend;

	// OpenGL current state
	GLenum		cullMode;
	GLfloat		offsetFactor;
	GLfloat		offsetUnits;
	GLenum		alphaFunc;
	GLclampf		alphaRef;
	GLenum		blendSrc;
	GLenum		blendDst;
	GLenum		depthFunc;
	GLboolean		depthMask;
	GLfloat		polygonoffset[2];

	// old stuff
	vec4_t		draw_color;	// using with Draw_* functions
	int		lightmap_textures;
	float		inverse_intensity;
} glstate_t;

// contains constant values that are always
// setting by R_Init and using as read-only
typedef struct glconfig_s
{
	const char	*renderer_string;
	const char	*vendor_string;
	const char	*version_string;

	// list of supported extensions
	const char	*extensions_string;
	byte		extension[R_EXTCOUNT];

	int		textureunits;
	int		max_3d_texture_size;
	int		max_cubemap_texture_size;
	int		max_anisotropy;
	GLint		texRectangle;
	bool		fullscreen;
	int		prev_mode;
} glconfig_t;

#endif//R_LOCAL_H