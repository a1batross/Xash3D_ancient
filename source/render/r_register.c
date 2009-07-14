/*
Copyright (C) 2007 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// r_register.c
#include "r_local.h"

glconfig_t	glConfig;
glstate_t	glState;

cvar_t *r_norefresh;
cvar_t *r_drawentities;
cvar_t *r_drawworld;
cvar_t *r_speeds;
cvar_t *r_drawelements;
cvar_t *r_fullbright;
cvar_t *r_lightmap;
cvar_t *r_novis;
cvar_t *r_nocull;
cvar_t *r_lerpmodels;
cvar_t *r_ignorehwgamma;
cvar_t *r_overbrightbits;
cvar_t *r_mapoverbrightbits;
cvar_t *r_flares;
cvar_t *r_flaresize;
cvar_t *r_flarefade;
cvar_t *r_dynamiclight;
cvar_t *r_coronascale;
cvar_t *r_detailtextures;
cvar_t *r_subdivisions;
cvar_t *r_faceplanecull;
cvar_t *r_showtris;
cvar_t *r_shownormals;
cvar_t *r_draworder;

cvar_t *r_fastsky;
cvar_t *r_portalonly;
cvar_t *r_portalmaps;
cvar_t *r_portalmaps_maxtexsize;

cvar_t *r_lighting_bumpscale;
cvar_t *r_lighting_deluxemapping;
cvar_t *r_lighting_diffuse2heightmap;
cvar_t *r_lighting_specular;
cvar_t *r_lighting_glossintensity;
cvar_t *r_lighting_glossexponent;
cvar_t *r_lighting_models_followdeluxe;
cvar_t *r_lighting_ambientscale;
cvar_t *r_lighting_directedscale;
cvar_t *r_lighting_packlightmaps;
cvar_t *r_lighting_maxlmblocksize;

cvar_t *r_offsetmapping;
cvar_t *r_offsetmapping_scale;
cvar_t *r_offsetmapping_reliefmapping;

cvar_t *r_occlusion_queries;
cvar_t *r_occlusion_queries_finish;

cvar_t *r_shadows;
cvar_t *r_shadows_alpha;
cvar_t *r_shadows_nudge;
cvar_t *r_shadows_projection_distance;
cvar_t *r_shadows_maxtexsize;
cvar_t *r_shadows_pcf;
cvar_t *r_shadows_self_shadow;

cvar_t *r_bloom;
cvar_t *r_bloom_alpha;
cvar_t *r_bloom_diamond_size;
cvar_t *r_bloom_intensity;
cvar_t *r_bloom_darken;
cvar_t *r_bloom_sample_size;
cvar_t *r_bloom_fast_sample;

cvar_t *r_outlines_world;
cvar_t *r_outlines_scale;
cvar_t *r_outlines_cutoff;

cvar_t *r_allow_software;
cvar_t *r_3dlabs_broken;

cvar_t *r_lodbias;
cvar_t *r_lodscale;
cvar_t *r_lefthand;

cvar_t *r_environment_color;
cvar_t *r_stencilbits;
cvar_t *r_gamma;
cvar_t *r_colorbits;
cvar_t *r_texturebits;
cvar_t *r_texturemode;
cvar_t *r_mode;
cvar_t *r_picmip;
cvar_t *r_skymip;
cvar_t *r_nobind;
cvar_t *r_clear;
cvar_t *r_polyblend;
cvar_t *r_lockpvs;
cvar_t *r_swapinterval;

cvar_t *gl_extensions;
cvar_t *gl_drawbuffer;
cvar_t *gl_driver;
cvar_t *gl_finish;
cvar_t *gl_delayfinish;
cvar_t *gl_cull;

cvar_t *vid_fullscreen;
cvar_t *vid_displayfrequency;

static bool	r_verbose;
byte		*r_temppool;

static void R_FinalizeGLExtensions( void );
static void R_GfxInfo_f( void );

//=======================================================================

#define	GLINF_FOFS(x) (size_t)&(((glextinfo_t *)0)->x)
#define	GLINF_EXMRK() GLINF_FOFS(_extMarker)
#define	GLINF_FROM(from,ofs) (*((char *)from + ofs))

typedef struct
{
	const char * const name;				// constant pointer to constant string
	void ** const pointer;					// constant pointer to function's pointer (function itself)
} gl_extension_func_t;

typedef struct
{
	const char * const prefix;				// constant pointer to constant string
	const char * const name;
	const char * const cvar_default;
	gl_extension_func_t * const funcs;		// constant pointer to array of functions
	const size_t offset;					// offset to respective variable
	const size_t depOffset;					// offset to required pre-initialized variable
} gl_extension_t;

static void R_FinalizeGLExtensions( void );

#define GL_EXTENSION_FUNC_EXT(name,func) { name, (void ** const)func }
#define GL_EXTENSION_FUNC(name) GL_EXTENSION_FUNC_EXT("gl"#name,&(qgl##name))

/* GL_ARB_multitexture */
static const gl_extension_func_t gl_ext_multitexture_ARB_funcs[] =
{
	 GL_EXTENSION_FUNC(ActiveTextureARB)
	,GL_EXTENSION_FUNC(ClientActiveTextureARB)
	,GL_EXTENSION_FUNC_EXT(NULL,NULL)
};

/* GL_EXT_compiled_vertex_array */
static const gl_extension_func_t gl_ext_compiled_vertex_array_EXT_funcs[] =
{
	 GL_EXTENSION_FUNC(LockArraysEXT)
	,GL_EXTENSION_FUNC(UnlockArraysEXT)
	,GL_EXTENSION_FUNC_EXT(NULL,NULL)
};

/* GL_ARB_vertex_buffer_object */
static const gl_extension_func_t gl_ext_vertex_buffer_object_ARB_funcs[] =
{
	 GL_EXTENSION_FUNC(BindBufferARB)
	,GL_EXTENSION_FUNC(DeleteBuffersARB)
	,GL_EXTENSION_FUNC(GenBuffersARB)
	,GL_EXTENSION_FUNC(BufferDataARB)
	,GL_EXTENSION_FUNC_EXT(NULL,NULL)
};

/* GL_EXT_texture3D */
static const gl_extension_func_t gl_ext_texture3D_EXT_funcs[] =
{
	 GL_EXTENSION_FUNC(TexImage3D)
	,GL_EXTENSION_FUNC(TexSubImage3D)
	,GL_EXTENSION_FUNC_EXT(NULL,NULL)
};

/* GL_EXT_draw_range_elements */
static const gl_extension_func_t gl_ext_draw_range_elements_EXT_funcs[] =
{
	 GL_EXTENSION_FUNC(DrawRangeElementsEXT)
	,GL_EXTENSION_FUNC_EXT("glDrawRangeElements",&qglDrawRangeElementsEXT)
	,GL_EXTENSION_FUNC_EXT(NULL,NULL)
};

/* GL_ARB_GLSL (meta extension) */
static const gl_extension_func_t gl_ext_GLSL_ARB_funcs[] =
{
	 GL_EXTENSION_FUNC(DeleteObjectARB)
	,GL_EXTENSION_FUNC(GetHandleARB)
	,GL_EXTENSION_FUNC(DetachObjectARB)
	,GL_EXTENSION_FUNC(CreateShaderObjectARB)
	,GL_EXTENSION_FUNC(ShaderSourceARB)
	,GL_EXTENSION_FUNC(CompileShaderARB)
	,GL_EXTENSION_FUNC(CreateProgramObjectARB)
	,GL_EXTENSION_FUNC(AttachObjectARB)
	,GL_EXTENSION_FUNC(LinkProgramARB)
	,GL_EXTENSION_FUNC(UseProgramObjectARB)
	,GL_EXTENSION_FUNC(ValidateProgramARB)
	,GL_EXTENSION_FUNC(Uniform1fARB)
	,GL_EXTENSION_FUNC(Uniform2fARB)
	,GL_EXTENSION_FUNC(Uniform3fARB)
	,GL_EXTENSION_FUNC(Uniform4fARB)
	,GL_EXTENSION_FUNC(Uniform1iARB)
	,GL_EXTENSION_FUNC(Uniform2iARB)
	,GL_EXTENSION_FUNC(Uniform3iARB)
	,GL_EXTENSION_FUNC(Uniform4iARB)
	,GL_EXTENSION_FUNC(Uniform1fvARB)
	,GL_EXTENSION_FUNC(Uniform2fvARB)
	,GL_EXTENSION_FUNC(Uniform3fvARB)
	,GL_EXTENSION_FUNC(Uniform4fvARB)
	,GL_EXTENSION_FUNC(Uniform1ivARB)
	,GL_EXTENSION_FUNC(Uniform2ivARB)
	,GL_EXTENSION_FUNC(Uniform3ivARB)
	,GL_EXTENSION_FUNC(Uniform4ivARB)
	,GL_EXTENSION_FUNC(UniformMatrix2fvARB)
	,GL_EXTENSION_FUNC(UniformMatrix3fvARB)
	,GL_EXTENSION_FUNC(UniformMatrix4fvARB)
	,GL_EXTENSION_FUNC(GetObjectParameterfvARB)
	,GL_EXTENSION_FUNC(GetObjectParameterivARB)
	,GL_EXTENSION_FUNC(GetInfoLogARB)
	,GL_EXTENSION_FUNC(GetAttachedObjectsARB)
	,GL_EXTENSION_FUNC(GetUniformLocationARB)
	,GL_EXTENSION_FUNC(GetActiveUniformARB)
	,GL_EXTENSION_FUNC(GetUniformfvARB)
	,GL_EXTENSION_FUNC(GetUniformivARB)
	,GL_EXTENSION_FUNC(GetShaderSourceARB)

	,GL_EXTENSION_FUNC(VertexAttribPointerARB)
	,GL_EXTENSION_FUNC(EnableVertexAttribArrayARB)
	,GL_EXTENSION_FUNC(DisableVertexAttribArrayARB)
	,GL_EXTENSION_FUNC(BindAttribLocationARB)
	,GL_EXTENSION_FUNC(GetActiveAttribARB)
	,GL_EXTENSION_FUNC(GetAttribLocationARB)

	,GL_EXTENSION_FUNC_EXT(NULL,NULL)
};

/* GL_ARB_occlusion_query */
static const gl_extension_func_t gl_ext_occlusion_query_ARB_funcs[] =
{
	 GL_EXTENSION_FUNC(GenQueriesARB)
	,GL_EXTENSION_FUNC(DeleteQueriesARB)
	,GL_EXTENSION_FUNC(IsQueryARB)
	,GL_EXTENSION_FUNC(BeginQueryARB)
	,GL_EXTENSION_FUNC(EndQueryARB)
	,GL_EXTENSION_FUNC(GetQueryivARB)
	,GL_EXTENSION_FUNC(GetQueryObjectivARB)
	,GL_EXTENSION_FUNC(GetQueryObjectuivARB)
	,GL_EXTENSION_FUNC_EXT(NULL,NULL)
};

#ifdef _WIN32

/* WGL_EXT_swap_interval */
static const gl_extension_func_t wgl_ext_swap_interval_EXT_funcs[] =
{
	 GL_EXTENSION_FUNC_EXT("wglSwapIntervalEXT",&qglSwapInterval)
	,GL_EXTENSION_FUNC_EXT(NULL,NULL)
};

#endif

#ifdef GLX_VERSION

/* GLX_SGI_occlusion_query */
static const gl_extension_func_t glx_ext_swap_control_SGI_funcs[] =
{
	 GL_EXTENSION_FUNC_EXT("glXSwapIntervalSGI",&qglSwapInterval)

	,GL_EXTENSION_FUNC_EXT(NULL,NULL)
};

#endif

//
// legacy crap
//

/* GL_SGIS_multitexture */
static const gl_extension_func_t gl_ext_multitexture_SGIS_funcs[] =
{
	 GL_EXTENSION_FUNC(SelectTextureSGIS)
	,GL_EXTENSION_FUNC_EXT(NULL,NULL)
};

#ifdef _WIN32

/* WGL_3DFX_gamma_control */
static const gl_extension_func_t wgl_ext_gamma_control_3DFX_funcs[] =
{
	 GL_EXTENSION_FUNC_EXT("wglGetDeviceGammaRamp3DFX",&qwglGetDeviceGammaRamp3DFX)
	,GL_EXTENSION_FUNC_EXT("wglSetDeviceGammaRamp3DFX",&qwglSetDeviceGammaRamp3DFX)
	,GL_EXTENSION_FUNC_EXT(NULL,NULL)
};

#endif

#undef GL_EXTENSION_FUNC
#undef GL_EXTENSION_FUNC_EXT

//=======================================================================

#define GL_EXTENSION_EXT(pre,name,val,funcs,dep) { #pre, #name, #val, (gl_extension_func_t * const)funcs, GLINF_FOFS(name), GLINF_FOFS(dep) }
#define GL_EXTENSION(pre,name,funcs) GL_EXTENSION_EXT(pre,name,1,funcs,_extMarker)

//
// OpenGL extensions list
//
// short notation: vendor, name, default value, list of functions
// extended notation: vendor, name, default value, list of functions, required extension
static const gl_extension_t gl_extensions_decl[] =
{
	 GL_EXTENSION( ARB, multitexture, &gl_ext_multitexture_ARB_funcs )
	,GL_EXTENSION( SGIS, multitexture, &gl_ext_multitexture_SGIS_funcs )
	,GL_EXTENSION( EXT, compiled_vertex_array, &gl_ext_compiled_vertex_array_EXT_funcs )
	,GL_EXTENSION( SGI, compiled_vertex_array, &gl_ext_compiled_vertex_array_EXT_funcs )
	,GL_EXTENSION_EXT( ARB, vertex_buffer_object, 0, &gl_ext_vertex_buffer_object_ARB_funcs, _extMarker )
	,GL_EXTENSION( EXT, texture3D, &gl_ext_texture3D_EXT_funcs )
	,GL_EXTENSION( EXT, draw_range_elements, &gl_ext_draw_range_elements_EXT_funcs )
	,GL_EXTENSION( ARB, occlusion_query, &gl_ext_occlusion_query_ARB_funcs )
	,GL_EXTENSION( ARB, texture_env_add, NULL )
	,GL_EXTENSION( ARB, texture_env_combine, NULL )
	,GL_EXTENSION( EXT, texture_env_combine, NULL )
	,GL_EXTENSION_EXT( ARB, texture_compression, 0, NULL, _extMarker )
	,GL_EXTENSION( EXT, texture_edge_clamp, NULL )
	,GL_EXTENSION( SGIS, texture_edge_clamp, NULL )
	,GL_EXTENSION( EXT, texture_filter_anisotropic, NULL )
	,GL_EXTENSION( ARB, texture_cube_map, NULL )
	,GL_EXTENSION( EXT, bgra, NULL )
	,GL_EXTENSION( ARB, depth_texture, NULL )
	,GL_EXTENSION( ARB, shadow, NULL )
	,GL_EXTENSION( SGIS, generate_mipmap, NULL )
	,GL_EXTENSION( ARB, texture_non_power_of_two, NULL )

	// extensions required by meta-extension gl_ext_GLSL
	,GL_EXTENSION_EXT( ARB, vertex_shader, 1, NULL, multitexture )
	,GL_EXTENSION_EXT( ARB, fragment_shader, 1, NULL, vertex_shader )
	,GL_EXTENSION_EXT( ARB, shader_objects, 1, NULL, fragment_shader )
	,GL_EXTENSION_EXT( ARB, shading_language_100, 1, NULL, shader_objects )

	// meta GLSL extensions
	,GL_EXTENSION_EXT( \0, GLSL, 1, &gl_ext_GLSL_ARB_funcs, shading_language_100 )
	,GL_EXTENSION_EXT( \0, GLSL_branching, 1, NULL, GLSL )
	,GL_EXTENSION_EXT( \0, GLSL_no_half_types, 0, NULL, GLSL )

#ifdef GLX_VERSION
	,GL_EXTENSION( GLX_SGI, swap_control, &glx_ext_swap_control_SGI_funcs )
#endif

#ifdef _WIN32
	,GL_EXTENSION( WGL_3DFX, gamma_control, &wgl_ext_gamma_control_3DFX_funcs )
	,GL_EXTENSION( WGL_EXT, swap_control, &wgl_ext_swap_interval_EXT_funcs )
#endif
};

static const int num_gl_extensions = sizeof( gl_extensions_decl ) / sizeof( gl_extensions_decl[0] );

#undef GL_EXTENSION
#undef GL_EXTENSION_EXT

/*
===============
R_RegisterGLExtensions
===============
*/
void R_RegisterGLExtensions( void )
{
	int i;
	char *var, name[128];
	cvar_t *cvar;
	gl_extension_func_t *func;
	const gl_extension_t *extension;

	memset( &glConfig.ext, 0, sizeof( glextinfo_t ) );

	// only initialize some common variables
	if( !gl_extensions->integer )
	{
		R_FinalizeGLExtensions ();
		return;
	}

	// gl_ext_vertex_buffer_object is crashy..
	Cvar_Set( "gl_ext_vertex_buffer_object", "0" );

	for( i = 0, extension = gl_extensions_decl; i < num_gl_extensions; i++, extension++ )
	{
		// register a cvar and check if this extension is explicitly disabled
		com.snprintf( name, sizeof( name ), "gl_ext_%s", extension->name );
		cvar = Cvar_Get( name, extension->cvar_default ? extension->cvar_default : "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, va( "enable or disable %s", name ));
		if( !cvar->integer )
			continue;

		// an alternative extension of higher priority is available so ignore this one
		var = &(GLINF_FROM( &glConfig.ext, extension->offset ));
		if( *var )
			continue;

		// required extension is not available, ignore
		if( extension->depOffset != GLINF_EXMRK() && !GLINF_FROM( &glConfig.ext, extension->depOffset ) )
			continue;

		// let's see what the driver's got to say about this...
		if( *extension->prefix )
		{
			const char *extstring = ( !strncmp( extension->prefix, "WGL", 3 ) || !strncmp( extension->prefix, "GLX", 3 ) ) 
				? glConfig.glwExtensionsString : glConfig.extensionsString;

			com.snprintf( name, sizeof( name ), "%s_%s", extension->prefix, extension->name );
			if( !com.strstr( extstring, name ) )
				continue;
		}

		// initialize function pointers
		func = extension->funcs;
		if( func )
		{
			do {
				*(func->pointer) = ( void * )qglGetProcAddress( (const GLubyte *)func->name );
				if( !*(func->pointer) )
					break;
			} while( (++func)->name );

			// some function is missing
			if( func->name )
			{
				gl_extension_func_t *func2 = extension->funcs;

				// whine about buggy driver
				MsgDev( D_ERROR, "R_RegisterGLExtensions: broken %s support, contact your video card vendor\n", cvar->name );

				// reset previously initialized functions back to NULL
				do {
					*(func2->pointer) = NULL;
				} while( ++func2 != func );

				continue;
			}
		}

		// mark extension as available
		*var = true;
	}

	R_FinalizeGLExtensions ();
}

/*
===============
R_PrintGLExtensionsInfo
===============
*/
void R_PrintGLExtensionsInfo( void )
{
	int i;
	size_t lastOffset;
	const gl_extension_t *extension;

	for( i = 0, lastOffset = 0, extension = gl_extensions_decl; i < num_gl_extensions; i++, extension++ )
	{
		if( lastOffset != extension->offset )
		{
			lastOffset = extension->offset;
			Msg( "%s: %s\n", extension->name, GLINF_FROM( &glConfig.ext, lastOffset ) ? "enabled" : "disabled" );
		}
	}
}

/*
===============
R_FinalizeGLExtensions

Verify correctness of values provided by the driver, init some variables
===============
*/
static void R_FinalizeGLExtensions( void )
{
	int	flags = 0;

	glConfig.maxTextureSize = 0;
	qglGetIntegerv( GL_MAX_TEXTURE_SIZE, &glConfig.maxTextureSize );
	if( glConfig.maxTextureSize <= 0 )
		glConfig.maxTextureSize = 256;

	Cvar_Get( "gl_max_texture_size", "0", CVAR_INIT, "opengl texture max dims" );
	Cvar_Set( "gl_max_texture_size", va( "%i", glConfig.maxTextureSize ) );

	/* GL_ARB_texture_cube_map */
	glConfig.maxTextureCubemapSize = 0;
	if( glConfig.ext.texture_cube_map )
		qglGetIntegerv( GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &glConfig.maxTextureCubemapSize );
	if( glConfig.maxTextureCubemapSize <= 1 )
		glConfig.ext.texture_cube_map = false;

	/* GL_EXT_texture3D */
	glConfig.maxTextureSize3D = 0;
	if( glConfig.ext.texture3D )
		qglGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &glConfig.maxTextureSize3D );
	if( glConfig.maxTextureSize3D <= 1 )
		glConfig.ext.texture3D = false;

	/* GL_ARB_multitexture */
	glConfig.maxTextureUnits = 1;
	if( glConfig.ext.multitexture ) {
		qglGetIntegerv( GL_MAX_TEXTURE_UNITS, &glConfig.maxTextureUnits );
		glConfig.maxTextureUnits = bound( 1, glConfig.maxTextureUnits, MAX_TEXTURE_UNITS );
	}
	if( glConfig.maxTextureUnits == 1 )
		glConfig.ext.multitexture = false;

	/* GL_EXT_texture_filter_anisotropic */
	glConfig.maxTextureFilterAnisotropic = 0;
	if( strstr( glConfig.extensionsString, "GL_EXT_texture_filter_anisotropic" ) )
		qglGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glConfig.maxTextureFilterAnisotropic );

	Cvar_Get( "gl_ext_texture_filter_anisotropic_max", "0", CVAR_INIT, "textures anisotropic filter" );
	Cvar_Set( "gl_ext_texture_filter_anisotropic_max", va( "%i", glConfig.maxTextureFilterAnisotropic ) );

	glConfig.curTextureFilterAnisotropic = (int)Cvar_VariableValue( "gl_ext_texture_filter_anisotropic" );
	glConfig.curTextureFilterAnisotropic = bound( 0, glConfig.curTextureFilterAnisotropic, glConfig.maxTextureFilterAnisotropic );

	if( strstr( glConfig.extensionsString, "GL_ARB_texture_compression" ))
		flags |= IL_DDS_HARDWARE;
	flags |= IL_USE_LERPING;

	Image_Init( NULL, flags );
}

//=======================================================================

void R_Register( void )
{
	cvar_t	*r_width, *r_height;

	Cmd_ExecuteString( "vidlatch\n" );

	// system screen width and height (don't suppose for change from console at all)
	r_width = Cvar_Get( "width", "640", CVAR_READ_ONLY, "screen width" );
	r_height = Cvar_Get( "height", "480", CVAR_READ_ONLY, "screen height" );

	r_norefresh = Cvar_Get( "r_norefresh", "0", 0, "disable rendering (use with caution)" );
	r_fullbright = Cvar_Get( "r_fullbright", "0", CVAR_CHEAT|CVAR_LATCH_VIDEO, "disable lightmaps, get fullbright" );
	r_lightmap = Cvar_Get( "r_lightmap", "0", CVAR_CHEAT, "lightmap debugging tool" );
	r_drawentities = Cvar_Get( "r_drawentities", "1", CVAR_CHEAT, "render entities" );
	r_drawworld = Cvar_Get( "r_drawworld", "1", CVAR_CHEAT, "render world" );
	r_novis = Cvar_Get( "r_novis", "0", 0, "ignore vis information (perfomance test)" );
	r_nocull = Cvar_Get( "r_nocull", "0", 0, "ignore frustrum culling (perfomance test)" );
	r_lerpmodels = Cvar_Get( "r_lerpmodels", "1", 0, "use lerping for alias and studio models" );
	r_speeds = Cvar_Get( "r_speeds", "0", 0, "shows r_speeds" );
	r_drawelements = Cvar_Get( "r_drawelements", "1", 0, "use gldrawElements or glDrawRangeElements" );
	r_showtris = Cvar_Get( "r_showtris", "0", CVAR_CHEAT, "show mesh triangles" );
	r_lockpvs = Cvar_Get( "r_lockpvs", "0", CVAR_CHEAT, "lockpvs area at current point (pvs test)" );
	r_clear = Cvar_Get( "r_clear", "0", CVAR_ARCHIVE, "clearing screen after each frame" );
	r_mode = Cvar_Get( "r_mode", VID_DEFAULTMODE, CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "display resolution mode" );
	r_nobind = Cvar_Get( "r_nobind", "0", 0, "disable all textures (perfomance test)" );
	r_picmip = Cvar_Get( "r_picmip", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "reduces resolution of textures by powers of 2" );
	r_skymip = Cvar_Get( "r_skymip", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "reduces resolution of skybox textures by powers of 2" );
	r_polyblend = Cvar_Get( "r_polyblend", "1", 0, "tints view while underwater, hurt, etc" );
	r_lefthand = Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE, "viewmodel handedness" );

	r_bloom = Cvar_Get( "r_bloom", "0", CVAR_ARCHIVE, "enable or disable bloom-effect" );
	r_bloom_alpha = Cvar_Get( "r_bloom_alpha", "0.2", CVAR_ARCHIVE, "bloom alpha multiplier" );
	r_bloom_diamond_size = Cvar_Get( "r_bloom_diamond_size", "8", CVAR_ARCHIVE, "bloom diamond size (can be 1, 2, 4, 6 or 8)" );
	r_bloom_intensity = Cvar_Get( "r_bloom_intensity", "1.3", CVAR_ARCHIVE, "bloom intensity scale" );
	r_bloom_darken = Cvar_Get( "r_bloom_darken", "4", CVAR_ARCHIVE, "bloom darken scale" );
	r_bloom_sample_size = Cvar_Get( "r_bloom_sample_size", "320", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "bloom rendering viewport size (must be power of two)" );
	r_bloom_fast_sample = Cvar_Get( "r_bloom_fast_sample", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "enable fast bloom pass" );

	r_environment_color = Cvar_Get( "r_environment_color", "0 0 0", CVAR_ARCHIVE, "map environment light color" );
	r_ignorehwgamma = Cvar_Get( "r_ignorehwgamma", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "ignore hardware gamma (e.g. not support)" );
	r_overbrightbits = Cvar_Get( "r_overbrightbits", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "renderer overbright bits" );
	r_mapoverbrightbits = Cvar_Get( "r_mapoverbrightbits", "2", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "current map overbright bits" );

	r_detailtextures = Cvar_Get( "r_detailtextures", "1", CVAR_ARCHIVE, "enable or disable detail textures" );
	r_flares = Cvar_Get( "r_flares", "0", CVAR_ARCHIVE, "enable flares rendering" );
	r_flaresize = Cvar_Get( "r_flaresize", "40", CVAR_ARCHIVE, "override shader flare size" );
	r_flarefade = Cvar_Get( "r_flarefade", "3", CVAR_ARCHIVE, "override shader flare fading" );

	r_dynamiclight = Cvar_Get( "r_dynamiclight", "1", CVAR_ARCHIVE, "allow dinamic lights on a map" );
	r_coronascale = Cvar_Get( "r_coronascale", "0.2", 0, "light coronas scale" );
	r_subdivisions = Cvar_Get( "r_subdivisions", "4", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "bezier curve subdivision" );
	r_faceplanecull = Cvar_Get( "r_faceplanecull", "1", CVAR_ARCHIVE, "culling face planes" );
	r_shownormals = Cvar_Get( "r_shownormals", "0", CVAR_CHEAT, "show mesh normals" );
	r_draworder = Cvar_Get( "r_draworder", "0", CVAR_CHEAT, "ignore mesh sorting" );

	r_fastsky = Cvar_Get( "r_fastsky", "0", CVAR_ARCHIVE, "enable algorhytem fo fast sky rendering (for old machines)" );
	r_portalonly = Cvar_Get( "r_portalonly", "0", 0, "render only portals" );
	r_portalmaps = Cvar_Get( "r_portalmaps", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "use portal maps for portal rendering" );
	r_portalmaps_maxtexsize = Cvar_Get( "r_portalmaps_maxtexsize", "800", CVAR_ARCHIVE, "portal maps texture dims" );

	r_allow_software = Cvar_Get( "r_allow_software", "0", 0, "allow OpenGL software emulation" );
	r_3dlabs_broken = Cvar_Get( "r_3dlabs_broken", "1", CVAR_ARCHIVE, "3dLabs renderer issues" );

	r_lighting_bumpscale = Cvar_Get( "r_lighting_bumpscale", "8", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "lighting bumpscale" );
	r_lighting_deluxemapping = Cvar_Get( "r_lighting_deluxemapping", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "allow deluxemapping for some maps" );
	r_lighting_diffuse2heightmap = Cvar_Get( "r_lighting_diffuse2heightmap", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "convert diffuse maps to heightmaps" );
	r_lighting_specular = Cvar_Get( "r_lighting_specular", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "using lighting specular" );
	r_lighting_glossintensity = Cvar_Get( "r_lighting_glossintensity", "2", CVAR_ARCHIVE, "gloss intensity" );
	r_lighting_glossexponent = Cvar_Get( "r_lighting_glossexponent", "48", CVAR_ARCHIVE, "gloss exponent factor" );
	r_lighting_models_followdeluxe = Cvar_Get( "r_lighting_models_followdeluxe", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "no description" );
	r_lighting_ambientscale = Cvar_Get( "r_lighting_ambientscale", "0.6", 0, "map ambient lighting scale" );
	r_lighting_directedscale = Cvar_Get( "r_lighting_directedscale", "1", 0, "map directed lighting scale" );
	r_lighting_packlightmaps = Cvar_Get( "r_lighting_packlightmaps", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "pack internal lightmaps (save video memory)" );
	r_lighting_maxlmblocksize = Cvar_Get( "r_lighting_maxlmblocksize", "1024", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "pack lightmap max block" );

	r_offsetmapping = Cvar_Get( "r_offsetmapping", "2", CVAR_ARCHIVE, "offsetmapping factor" );
	r_offsetmapping_scale = Cvar_Get( "r_offsetmapping_scale", "0.02", CVAR_ARCHIVE, "offsetmapping scale" );
	r_offsetmapping_reliefmapping = Cvar_Get( "r_offsetmapping_reliefmapping", "0", CVAR_ARCHIVE, "use reliefmapping instead offesetmapping" );

	r_occlusion_queries = Cvar_Get( "r_occlusion_queries", "2", CVAR_ARCHIVE, "use occlusion queries culling" );
	r_occlusion_queries_finish = Cvar_Get( "r_occlusion_queries_finish", "1", CVAR_ARCHIVE, "use glFinish for occlusion queries" );

	r_shadows = Cvar_Get( "r_shadows", "0", CVAR_ARCHIVE, "enable model shadows (1 - simple planar, 2 - shadowmaps)" );
	r_shadows_alpha = Cvar_Get( "r_shadows_alpha", "0.4", CVAR_ARCHIVE, "planar shadows alpha-value" );
	r_shadows_nudge = Cvar_Get( "r_shadows_nudge", "1", CVAR_ARCHIVE, "planar shadows nudge factor" );
	r_shadows_projection_distance = Cvar_Get( "r_shadows_projection_distance", "32", CVAR_ARCHIVE, "maximum projection dist for planar shadows" );
	r_shadows_maxtexsize = Cvar_Get( "r_shadows_maxtexsize", "0", CVAR_ARCHIVE, "shadowmaps texture size (0 - custom)" );
	r_shadows_pcf = Cvar_Get( "r_shadows_pcf", "0", CVAR_ARCHIVE, "allow pcf filtration" );
	r_shadows_self_shadow = Cvar_Get( "r_shadows_self_shadow", "0", CVAR_ARCHIVE, "allow self-shadowing" );

#ifdef HARDWARE_OUTLINES
	r_outlines_world = Cvar_Get( "r_outlines_world", "1.8", CVAR_ARCHIVE, "cel-shading world outline thinkness" );
	r_outlines_scale = Cvar_Get( "r_outlines_scale", "1", CVAR_ARCHIVE, "outilines scale" );
	r_outlines_cutoff = Cvar_Get( "r_outlines_cutoff", "712", CVAR_ARCHIVE, "cutoff factor" );
#endif

	r_lodbias = Cvar_Get( "r_lodbias", "0", CVAR_ARCHIVE, "textures lod bias" );
	r_lodscale = Cvar_Get( "r_lodscale", "5.0", CVAR_ARCHIVE, "LOD scale factor" );

	r_gamma = Cvar_Get( "r_gamma", "1.0", CVAR_ARCHIVE, "gamma amount" );
	r_colorbits = Cvar_Get( "r_colorbits", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO, "pixelformat color bits (0 - auto)" );
	r_texturebits = Cvar_Get( "r_texturebits", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO, "no description" );
	r_texturemode = Cvar_Get( "r_texturemode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE, "texture filter" );
	r_stencilbits = Cvar_Get( "r_stencilbits", "8", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "pixelformat stencil bits (0 - auto)" );

	r_swapinterval = Cvar_Get( "r_swapinterval", "0", CVAR_ARCHIVE,  "time beetween frames (in msec)" );

	// make sure r_swapinterval is checked after vid_restart
	r_swapinterval->modified = true;

	gl_finish = Cvar_Get( "gl_finish", "0", CVAR_ARCHIVE, "use glFinish instead of glFlush" );
	gl_delayfinish = Cvar_Get( "gl_delayfinish", "1", CVAR_ARCHIVE, "no description" );
	gl_cull = Cvar_Get( "gl_cull", "1", 0, "allow GL_CULL_FACE" );
	gl_driver = Cvar_Get( "gl_driver", "opengl32.dll", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "OpenGl default driver name" );
	gl_drawbuffer = Cvar_Get( "gl_drawbuffer", "GL_BACK", 0, "use back or front buffer" );
	gl_extensions = Cvar_Get( "gl_extensions", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "allow gl_extensions" );

	vid_displayfrequency = Cvar_Get ( "vid_displayfrequency", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "fullscreen refresh rate" );
	vid_fullscreen = Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "set in 1 to enable fullscreen mode" );

	Cmd_AddCommand( "imagelist", R_ImageList_f, "display loaded textures list" );
	Cmd_AddCommand( "shaderlist", R_ShaderList_f, "display loaded shaders list" );
	Cmd_AddCommand( "shaderdump", R_ShaderDump_f, "dump shaders into text file" );
	Cmd_AddCommand( "modellist", Mod_Modellist_f, "display loaded models list" );
	Cmd_AddCommand( "gfxinfo", R_GfxInfo_f, "display openGL supported extensions" );
	Cmd_AddCommand( "glslprogramlist", R_ProgramList_f, "display loaded GLSL shaders list" );
	Cmd_AddCommand( "glslprogramdump", R_ProgramDump_f, "sump GLSL shaders into text file" );
}

/*
==================
R_SetMode
==================
*/
static bool R_SetMode( void )
{
	int err;
	bool fullscreen;

	if( vid_fullscreen->modified && !glConfig.allowCDS )
	{
		MsgDev( D_INFO, "R_SetMode() - CDS not allowed with this driver\n" );
		Cvar_SetValue( "vid_fullscreen", !vid_fullscreen->integer );
		vid_fullscreen->modified = false;
	}

	fullscreen = vid_fullscreen->integer;
	vid_fullscreen->modified = false;

	if( r_mode->integer < -1 )
	{
		MsgDev( D_ERROR, "Bad mode %i or custom resolution\n", r_mode->integer );
		Cvar_Set( "r_mode", VID_DEFAULTMODE );
	}

	r_mode->modified = false;

	if( ( err = GLimp_SetMode( r_mode->integer, fullscreen ) ) == rserr_ok )
	{
		glState.previousMode = r_mode->integer;
	}
	else
	{
		if( err == rserr_invalid_fullscreen )
		{
			Cvar_SetValue( "vid_fullscreen", 0 );
			vid_fullscreen->modified = false;
			MsgDev( D_ERROR, "ref_gl::R_SetMode() - fullscreen unavailable in this mode\n" );
			if( ( err = GLimp_SetMode( r_mode->integer, false ) ) == rserr_ok )
				return true;
		}
		else if( err == rserr_invalid_mode )
		{
			Cvar_SetValue( "r_mode", glState.previousMode );
			r_mode->modified = false;
			MsgDev( D_ERROR, "ref_gl::R_SetMode() - invalid mode\n" );
		}

		// try setting it back to something safe
		if( ( err = GLimp_SetMode( glState.previousMode, false ) ) != rserr_ok )
		{
			MsgDev( D_ERROR, "ref_gl::R_SetMode() - could not revert to safe mode\n" );
			return false;
		}
	}

	if( r_ignorehwgamma->integer )
		glState.hwGamma = false;
	else
		glState.hwGamma = GLimp_GetGammaRamp( 256, glState.orignalGammaRamp );
	if( glState.hwGamma )
		r_gamma->modified = true;

	return true;
}

/*
===============
R_SetDefaultTexState
===============
*/
static void R_SetDefaultTexState( void )
{
	memset( glState.currentTextures, -1, MAX_TEXTURE_UNITS*sizeof(*glState.currentTextures) );
	memset( glState.currentEnvModes, -1, MAX_TEXTURE_UNITS*sizeof(*glState.currentEnvModes) );
	memset( glState.texIdentityMatrix, 0, MAX_TEXTURE_UNITS*sizeof(*glState.texIdentityMatrix) );
	memset( glState.genSTEnabled, 0, MAX_TEXTURE_UNITS*sizeof(*glState.genSTEnabled) );
	memset( glState.texCoordArrayMode, 0, MAX_TEXTURE_UNITS*sizeof(*glState.texCoordArrayMode) );
}

/*
===============
R_SetDefaultState
===============
*/
static void R_SetDefaultState( void )
{
	// FIXME: dynamically allocate these?
	static GLuint r_currentTextures[MAX_TEXTURE_UNITS];
	static int r_currentEnvModes[MAX_TEXTURE_UNITS];
	static bool	r_texIdentityMatrix[MAX_TEXTURE_UNITS];
	static int r_genSTEnabled[MAX_TEXTURE_UNITS];
	static int r_texCoordArrayMode[MAX_TEXTURE_UNITS];

	memset( &glState, 0, sizeof(glState) );

	glState.currentTextures = r_currentTextures;
	glState.currentEnvModes = r_currentEnvModes;
	glState.texIdentityMatrix = r_texIdentityMatrix;
	glState.genSTEnabled = r_genSTEnabled;
	glState.texCoordArrayMode = r_texCoordArrayMode;

	R_SetDefaultTexState ();

	// set our "safe" modes
	glState.previousMode = 3;
	glState.initializedMedia = false;
}

/*
===============
R_SetGLDefaults
===============
*/
static void R_SetGLDefaults( void )
{
	int i;

	qglFinish();

	qglClearColor( 1, 0, 0.5, 0.5 );

	qglEnable( GL_DEPTH_TEST );
	qglDisable( GL_CULL_FACE );
	qglEnable( GL_SCISSOR_TEST );
	qglDepthFunc( GL_LEQUAL );
	qglDepthMask( GL_FALSE );

	qglColor4f( 1, 1, 1, 1 );

	if( glState.stencilEnabled )
	{
		qglDisable( GL_STENCIL_TEST );
		qglStencilMask( ( GLuint ) ~0 );
		qglStencilFunc( GL_EQUAL, 128, 0xFF );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_INCR );
	}

	// enable gouraud shading
	qglShadeModel( GL_SMOOTH );

	qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	qglPolygonOffset( -1, -2 );

	// properly disable multitexturing at startup
	for( i = glConfig.maxTextureUnits-1; i > 0; i-- )
	{
		GL_SelectTexture( i );
		GL_TexEnv( GL_MODULATE );
		qglDisable( GL_BLEND );
		qglDisable( GL_TEXTURE_2D );
	}

	GL_SelectTexture( 0 );
	qglDisable( GL_BLEND );
	qglDisable( GL_ALPHA_TEST );
	qglDisable( GL_POLYGON_OFFSET_FILL );
	qglEnable( GL_TEXTURE_2D );

	GL_Cull( 0 );
	GL_FrontFace( 0 );

	GL_SetState( GLSTATE_DEPTHWRITE );
	GL_TexEnv( GL_MODULATE );

	R_TextureMode( r_texturemode->string );

	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );

	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
}

/*
==================
R_GfxInfo_f
==================
*/
static void R_GfxInfo_f( void )
{
	Msg( "\n" );
	Msg( "GL_VENDOR: %s\n", glConfig.vendorString );
	Msg( "GL_RENDERER: %s\n", glConfig.rendererString );
	Msg( "GL_VERSION: %s\n", glConfig.versionString );
	Msg( "GL_EXTENSIONS: %s\n", glConfig.extensionsString );
	if( *glConfig.glwExtensionsString )
		Msg( "GLW_EXTENSIONS: %s\n", glConfig.glwExtensionsString );
	Msg( "GL_MAX_TEXTURE_SIZE: %i\n", glConfig.maxTextureSize );
	Msg( "GL_MAX_TEXTURE_UNITS: %i\n", glConfig.maxTextureUnits );
	if( glConfig.ext.texture_cube_map )
		Msg( "GL_MAX_CUBE_MAP_TEXTURE_SIZE: %i\n", glConfig.maxTextureCubemapSize );
	if( glConfig.ext.texture3D )
		Msg( "GL_MAX_3D_TEXTURE_SIZE: %i\n", glConfig.maxTextureSize3D );
	if( glConfig.ext.texture_filter_anisotropic )
		Msg( "GL_MAX_TEXTURE_MAX_ANISOTROPY: %i\n", glConfig.maxTextureFilterAnisotropic );
	Msg( "\n" );

	Msg( "mode: %i, %s, %s\n", r_mode->integer,
		glState.fullScreen ? "fullscreen" : "", glState.wideScreen ? "widescreen" : "" );
	Msg( "CDS: %s\n", glConfig.allowCDS ? "enabled" : "disabled" );
	Msg( "picmip: %i\n", r_picmip->integer );
	Msg( "texturemode: %s\n", r_texturemode->string );
	Msg( "vertical sync: %s\n", r_swapinterval->integer ? "enabled" : "disabled" );

	R_PrintGLExtensionsInfo ();
}

/*
===============
R_Init
===============
*/
int R_InitRender( void *hinstance, void *hWnd, bool verbose )
{
	char renderer_buffer[1024];
	char vendor_buffer[1024];
	int err;
	char *driver;

	r_verbose = verbose;
	r_temppool = Mem_AllocPool( "Render TempPool" );

	Msg( "\n----- R_Init -----\n" );

	R_Register();

	R_SetDefaultState();

	glConfig.allowCDS = true;

	driver = gl_driver->string;

	// initialize our QGL dynamic bindings
init_qgl:
	if( !QGL_Init( gl_driver->string ) )
	{
		QGL_Shutdown();
		Msg( "ref_gl::R_Init() - could not load \"%s\"\n", gl_driver->string );

		if( com.strcmp( gl_driver->string, "opengl32.dll" ))
		{
			Cvar_Set( gl_driver->name, "opengl32.dll" );
			goto init_qgl;
		}

		return false;
	}

	// initialize OS-specific parts of OpenGL
	if( !GLimp_Init( hinstance, hWnd ) )
	{
		QGL_Shutdown();
		return false;
	}

	// create the window and set up the context
	if( !R_SetMode() )
	{
		QGL_Shutdown();
		Msg( "ref_gl::R_Init() - could not R_SetMode()\n" );
		return false;
	}

	/*
	** get our various GL strings
	*/
	glConfig.vendorString = (const char *)qglGetString( GL_VENDOR );
	glConfig.rendererString = (const char *)qglGetString( GL_RENDERER );
	glConfig.versionString = (const char *)qglGetString( GL_VERSION );
	glConfig.extensionsString = (const char *)qglGetString( GL_EXTENSIONS );
	glConfig.glwExtensionsString = (const char *)qglGetGLWExtensionsString ();

	if( !glConfig.vendorString ) glConfig.vendorString = "";
	if( !glConfig.rendererString ) glConfig.rendererString = "";
	if( !glConfig.versionString ) glConfig.versionString = "";
	if( !glConfig.extensionsString ) glConfig.extensionsString = "";
	if( !glConfig.glwExtensionsString ) glConfig.glwExtensionsString = "";

	com.strncpy( renderer_buffer, glConfig.rendererString, sizeof( renderer_buffer ) );
	com.strlwr( renderer_buffer, renderer_buffer );

	com.strncpy( vendor_buffer, glConfig.vendorString, sizeof( vendor_buffer ) );
	com.strlwr( vendor_buffer, vendor_buffer );

	if( com.strstr( renderer_buffer, "voodoo" ) )
	{
		if( !com.strstr( renderer_buffer, "rush" ) )
			glConfig.renderer = GL_RENDERER_VOODOO;
		else
			glConfig.renderer = GL_RENDERER_VOODOO_RUSH;
	}
	else if( com.strstr( vendor_buffer, "sgi" ) )
		glConfig.renderer = GL_RENDERER_SGI;
	else if( com.strstr( renderer_buffer, "permedia" ) )
		glConfig.renderer = GL_RENDERER_PERMEDIA2;
	else if( com.strstr( renderer_buffer, "glint" ) )
		glConfig.renderer = GL_RENDERER_GLINT_MX;
	else if( com.strstr( renderer_buffer, "glzicd" ) )
		glConfig.renderer = GL_RENDERER_REALIZM;
	else if( com.strstr( renderer_buffer, "gdi" ) )
		glConfig.renderer = GL_RENDERER_MCD;
	else if( com.strstr( renderer_buffer, "pcx2" ) )
		glConfig.renderer = GL_RENDERER_PCX2;
	else if( com.strstr( renderer_buffer, "verite" ) )
		glConfig.renderer = GL_RENDERER_RENDITION;
	else
		glConfig.renderer = GL_RENDERER_OTHER;

#ifdef GL_FORCEFINISH
	Cvar_SetValue( "gl_finish", 1 );
#endif

	// MCD has buffering issues
	if( glConfig.renderer == GL_RENDERER_MCD )
		Cvar_SetValue( "gl_finish", 1 );

	glConfig.allowCDS = true;
	if( glConfig.renderer & GL_RENDERER_3DLABS )
	{
		if( r_3dlabs_broken->integer )
			glConfig.allowCDS = false;
		else
			glConfig.allowCDS = true;
	}

	R_RegisterGLExtensions();

	R_SetGLDefaults();

	if( r_verbose )
		R_GfxInfo_f();

	R_BackendInit();

	R_ClearScene();

	err = qglGetError();
	if( err != GL_NO_ERROR )
		Msg( "glGetError() = 0x%x\n", err );

	Msg( "----- finished R_Init -----\n" );

	return true;
}

/*
===============
R_InitMedia
===============
*/
static void R_InitMedia( void )
{
	if( glState.initializedMedia )
		return;

	R_InitMeshLists();

	R_InitLightStyles();
	R_InitGLSLPrograms();
	R_InitImages();
	R_InitCinematics ();
	R_InitShaders( !r_verbose );
	R_InitModels();
	R_InitSkinFiles();
	R_InitCoronas();
	R_InitShadows();
	R_InitOcclusionQueries();
	R_InitCustomColors ();
#ifdef HARDWARE_OUTLINES
	R_InitOutlines ();
#endif

	R_SetDefaultTexState ();

	memset( &RI, 0, sizeof( refinst_t ) );

	glState.initializedMedia = true;
}

/*
===============
R_FreeMedia
===============
*/
static void R_FreeMedia( void )
{
	if( !glState.initializedMedia )
		return;

	R_ShutdownOcclusionQueries();
	R_ShutdownShadows();
	R_ShutdownSkinFiles();
	R_ShutdownModels();
	R_ShutdownShaders();
	R_ShutdownCinematics ();
	R_ShutdownImages();
	R_ShutdownGLSLPrograms();

	R_FreeMeshLists();

	glState.initializedMedia = false;
}

bool R_Init( bool full )
{
	bool	result = false;

	if( !full )
	{
		R_InitMedia();
		return true;
	}

	result = R_InitRender( GetModuleHandle( NULL ), ri.WndProc, true );
	if( result ) R_InitMedia();

	return result;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown( bool full )
{
	if( full )
	{
		Cmd_RemoveCommand( "modellist" );
		Cmd_RemoveCommand( "screenshot" );
		Cmd_RemoveCommand( "envshot" );
		Cmd_RemoveCommand( "imagelist" );
		Cmd_RemoveCommand( "gfxinfo" );
		Cmd_RemoveCommand( "shaderdump" );
		Cmd_RemoveCommand( "shaderlist" );
		Cmd_RemoveCommand( "glslprogramlist" );
		Cmd_RemoveCommand( "glslprogramdump" );

		// free shaders, models, etc.
		R_FreeMedia();

		Mem_FreePool( &r_temppool );

		// shutdown rendering backend
		R_BackendShutdown();

		// restore original gamma
		if( glState.hwGamma )
			GLimp_SetGammaRamp( 256, glState.orignalGammaRamp );

		// shut down OS specific OpenGL stuff like contexts, etc.
		GLimp_Shutdown();

		// shutdown our QGL subsystem
		QGL_Shutdown();
	}
	else R_FreeMedia();
	r_verbose = false;
}