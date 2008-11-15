//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        r_backend.h - rendering backend
//=======================================================================

#ifndef R_BACKEND_H
#define R_BACKEND_H

/*
=======================================================================

 GL STATE MACHINE

=======================================================================
*/
typedef enum
{
	R_OPENGL_110 = 0,		// base
	R_SGIS_MIPMAPS_EXT,
	R_WGL_SWAPCONTROL,
	R_COMBINE_EXT,
	R_DRAW_RANGEELEMENTS_EXT,
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
	R_VERTEX_SHADER_EXT,	// glsl vertex program
	R_FRAGMENT_SHADER_EXT,	// glsl fragment program	
	R_VERTEX_PROGRAM_EXT,	// cg vertex program
	R_FRAGMENT_PROGRAM_EXT,	// cg fragment program
	R_EXT_POINTPARAMETERS,
	R_SEPARATESTENCIL_EXT,
	R_ARB_TEXTURE_NPOT_EXT,
	R_ARB_VERTEX_BUFFER_OBJECT_EXT,
	R_CUSTOM_VERTEX_ARRAY_EXT,
	R_TEXTURE_COMPRESSION_EXT,
	R_TEXTURE_ENV_ADD_EXT,
	R_CLAMP_TEXBORDER_EXT,
	R_TEXTURE_LODBIAS,
	R_EXTCOUNT
} ref_glext_t;

typedef struct glstate_s
{
	bool		orthogonal;
	word		gammaRamp[768];		// current gamma ramp
	word		stateRamp[768];		// original gamma ramp
	uint		screenTexture;

	vec4_t		draw_color;		// current color
	int		texNum[MAX_TEXTURE_UNITS];
	int		texEnv[MAX_TEXTURE_UNITS];
	uint		activeTMU;

	// opengl current state
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
} glstate_t;

// contains constant values that are always
// setting by R_Init and using as read-only
typedef struct glconfig_s
{
	const char	*renderer_string;		// ptrs to OpenGL32.dll, use with caution
	const char	*vendor_string;
	const char	*version_string;

	// list of supported extensions
	const char	*extensions_string;
	byte		extension[R_EXTCOUNT];
	uint		max_entities;		// server MaxEdicts

	int		textureunits;
	int		texturecoords;
	int		teximageunits;
	GLint		max_2d_texture_size;
	GLint		max_2d_rectangle_size;
	GLint		max_3d_texture_size;
	GLint		max_cubemap_texture_size;
	GLfloat		max_anisotropy;
	GLfloat		max_lodbias;
	GLint		texRectangle;

	bool		deviceSupportsGamma;
	bool		fullscreen;
	int		prev_mode;
} glconfig_t;

extern glconfig_t  gl_config;
extern glstate_t   gl_state;

/*
=======================================================================

BACKEND

=======================================================================
*/

#define MAX_VERTEXES		4096
#define MAX_ELEMENTS		MAX_VERTEXES * 6

// vbo offsets
#define BUFFER_OFFSET_POINT		0	// in bytes
#define BUFFER_OFFSET_NORMAL		12
#define BUFFER_OFFSET_TANGENT		24
#define BUFFER_OFFSET_BINORMAL	36
#define BUFFER_OFFSET_STCOORD		48
#define BUFFER_OFFSET_LMCOORD		56
#define BUFFER_OFFSET_COLOR		64

typedef uint ref_vindex_t;

typedef struct ref_varray_s
{
	vec3_t		point;
	vec3_t		normal;
	vec3_t		tangent;
	vec3_t		binormal;
	vec2_t		stcoord;
	vec2_t		lmcoord;
	vec4_t		color;
} ref_varray_t;

typedef struct ref_buffer_s
{
	byte		*pointer;
	int		size;
	uint		usage;
	uint		bufNum;
} ref_buffer_t;

typedef struct
{
	vec3_t		origin;
	vec3_t		angles;

	matrix4x4		entityMatrix;
	matrix4x4		worldMatrix;
	matrix4x4		modelViewMatrix;	// worldMatrix * entityMatrix
	matrix4x4		projectionMatrix;
} viewParms_t;

typedef struct
{
	int		width;
	int		height;

	vrect_t		viewport;
	viewParms_t	viewParms;

	ref_entity_t	*CurrentEntity;
	ref_shader_t	*CurrentShader;
	rmodel_t		*CurrentModel;
	float		CurrentTime;

	int		numIndex;
	int		numVertex;
	ref_vindex_t	indexArray[MAX_ELEMENTS];
	ref_varray_t	vertsArray[MAX_VERTEXES];
	vec3_t		stArray[MAX_TEXTURE_UNITS][MAX_VERTEXES];

	ref_buffer_t	*stBuffer[MAX_TEXTURE_UNITS];
	ref_buffer_t	*vertexBuffer;

	uint		registration_sequence;
} ref_backend_t;

typedef struct
{
	// builtin shaders
	ref_shader_t	*defaultShader;
	ref_shader_t	*lightmapShader;
	ref_shader_t	*skyboxShader;
	ref_shader_t	*particleShader;
} ref_globals_t;

extern ref_backend_t	ref;
extern ref_globals_t	tr;

// backend
ref_buffer_t *RB_AllocVertexBuffer( size_t size, GLuint usage );
void RB_UpdateVertexBuffer( ref_buffer_t *vertexBuffer, const void *data, size_t size );
void RB_InitVertexBuffers( void );
void RB_ShutdownVertexBuffers( void );
void RB_DrawElements( void );

// debug
void RB_ShowTextures( void );

#endif//R_BACKEND_H