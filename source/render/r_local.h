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
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;

	// list of supported extensions
	const char *extensions_string;
	byte	extension[R_EXTCOUNT];

	int	textureunits;
	int	max_3d_texture_size;
	int	max_cubemap_texture_size;
	int	max_anisotropy;
	GLint	texRectangle;
	bool	fullscreen;
	int	prev_mode;
} glconfig_t;

#endif//R_LOCAL_H