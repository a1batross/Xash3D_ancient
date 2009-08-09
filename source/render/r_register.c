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
#include "mathlib.h"

glconfig_t	glConfig;
glstate_t		glState;
byte		*r_temppool;
byte		*r_framebuffer;

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
cvar_t *r_check_errors;
cvar_t *r_overbrightbits;
cvar_t *r_mapoverbrightbits;
cvar_t *r_vertexbuffers;
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
cvar_t *r_showtextures;
cvar_t *r_draworder;
cvar_t *r_width;
cvar_t *r_height;
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
cvar_t *r_physbdebug;

cvar_t *r_environment_color;
cvar_t *r_stencilbits;
cvar_t *r_gamma;
cvar_t *r_colorbits;
cvar_t *r_depthbits;
cvar_t *r_texturebits;
cvar_t *gl_texturemode;
cvar_t *gl_texture_anisotropy;
cvar_t *gl_texture_lodbias;
cvar_t *gl_round_down;
cvar_t *gl_compress_textures;
cvar_t *r_mode;
cvar_t *r_picmip;
cvar_t *r_skymip;
cvar_t *r_nobind;
cvar_t *r_clear;
cvar_t *r_polyblend;
cvar_t *r_lockpvs;
cvar_t *r_swapInterval;
cvar_t *r_frontbuffer;

cvar_t *gl_extensions;
cvar_t *gl_driver;
cvar_t *gl_finish;
cvar_t *gl_delayfinish;
cvar_t *gl_cull;

cvar_t *vid_fullscreen;
cvar_t *vid_displayfrequency;

static void GL_SetDefaultState( void );

// set initial values
extern dll_info_t opengl_dll;

static dllfunc_t wglswapintervalfuncs[] =
{
	{"wglSwapIntervalEXT", (void **) &pwglSwapIntervalEXT},
	{NULL, NULL}
};

static dllfunc_t wgl3DFXgammacontrolfuncs[] =
{
	{"wglGetDeviceGammaRamp3DFX", (void **) &pwglGetDeviceGammaRamp3DFX},
	{"wglSetDeviceGammaRamp3DFX", (void **) &pwglSetDeviceGammaRamp3DFX},
	{NULL, NULL}
};

static dllfunc_t opengl_110funcs[] =
{
	{"glClearColor", (void **) &pglClearColor},
	{"glClear", (void **) &pglClear},
	{"glAlphaFunc", (void **) &pglAlphaFunc},
	{"glBlendFunc", (void **) &pglBlendFunc},
	{"glCullFace", (void **) &pglCullFace},
	{"glDrawBuffer", (void **) &pglDrawBuffer},
	{"glReadBuffer", (void **) &pglReadBuffer},
	{"glEnable", (void **) &pglEnable},
	{"glDisable", (void **) &pglDisable},
	{"glEnableClientState", (void **) &pglEnableClientState},
	{"glDisableClientState", (void **) &pglDisableClientState},
	{"glGetBooleanv", (void **) &pglGetBooleanv},
	{"glGetDoublev", (void **) &pglGetDoublev},
	{"glGetFloatv", (void **) &pglGetFloatv},
	{"glGetIntegerv", (void **) &pglGetIntegerv},
	{"glGetError", (void **) &pglGetError},
	{"glGetString", (void **) &pglGetString},
	{"glFinish", (void **) &pglFinish},
	{"glFlush", (void **) &pglFlush},
	{"glClearDepth", (void **) &pglClearDepth},
	{"glDepthFunc", (void **) &pglDepthFunc},
	{"glDepthMask", (void **) &pglDepthMask},
	{"glDepthRange", (void **) &pglDepthRange},
	{"glFrontFace", (void **) &pglFrontFace},
	{"glDrawElements", (void **) &pglDrawElements},
	{"glColorMask", (void **) &pglColorMask},
	{"glIndexPointer", (void **) &pglIndexPointer},
	{"glVertexPointer", (void **) &pglVertexPointer},
	{"glNormalPointer", (void **) &pglNormalPointer},
	{"glColorPointer", (void **) &pglColorPointer},
	{"glTexCoordPointer", (void **) &pglTexCoordPointer},
	{"glArrayElement", (void **) &pglArrayElement},
	{"glColor3f", (void **) &pglColor3f},
	{"glColor3fv", (void **) &pglColor3fv},
	{"glColor4f", (void **) &pglColor4f},
	{"glColor4fv", (void **) &pglColor4fv},
	{"glColor4ub", (void **) &pglColor4ub},
	{"glColor4ubv", (void **) &pglColor4ubv},
	{"glTexCoord1f", (void **) &pglTexCoord1f},
	{"glTexCoord2f", (void **) &pglTexCoord2f},
	{"glTexCoord3f", (void **) &pglTexCoord3f},
	{"glTexCoord4f", (void **) &pglTexCoord4f},
	{"glTexGenf", (void **) &pglTexGenf},
	{"glTexGenfv", (void **) &pglTexGenfv},
	{"glTexGeni", (void **) &pglTexGeni},
	{"glVertex2f", (void **) &pglVertex2f},
	{"glVertex3f", (void **) &pglVertex3f},
	{"glVertex3fv", (void **) &pglVertex3fv},
	{"glNormal3f", (void **) &pglNormal3f},
	{"glNormal3fv", (void **) &pglNormal3fv},
	{"glBegin", (void **) &pglBegin},
	{"glEnd", (void **) &pglEnd},
	{"glLineWidth", (void**) &pglLineWidth},
	{"glPointSize", (void**) &pglPointSize},
	{"glMatrixMode", (void **) &pglMatrixMode},
	{"glOrtho", (void **) &pglOrtho},
	{"glRasterPos2f", (void **) &pglRasterPos2f},
	{"glFrustum", (void **) &pglFrustum},
	{"glViewport", (void **) &pglViewport},
	{"glPushMatrix", (void **) &pglPushMatrix},
	{"glPopMatrix", (void **) &pglPopMatrix},
	{"glLoadIdentity", (void **) &pglLoadIdentity},
	{"glLoadMatrixd", (void **) &pglLoadMatrixd},
	{"glLoadMatrixf", (void **) &pglLoadMatrixf},
	{"glMultMatrixd", (void **) &pglMultMatrixd},
	{"glMultMatrixf", (void **) &pglMultMatrixf},
	{"glRotated", (void **) &pglRotated},
	{"glRotatef", (void **) &pglRotatef},
	{"glScaled", (void **) &pglScaled},
	{"glScalef", (void **) &pglScalef},
	{"glTranslated", (void **) &pglTranslated},
	{"glTranslatef", (void **) &pglTranslatef},
	{"glReadPixels", (void **) &pglReadPixels},
	{"glDrawPixels", (void **) &pglDrawPixels},
	{"glStencilFunc", (void **) &pglStencilFunc},
	{"glStencilMask", (void **) &pglStencilMask},
	{"glStencilOp", (void **) &pglStencilOp},
	{"glClearStencil", (void **) &pglClearStencil},
	{"glTexEnvf", (void **) &pglTexEnvf},
	{"glTexEnvfv", (void **) &pglTexEnvfv},
	{"glTexEnvi", (void **) &pglTexEnvi},
	{"glTexParameterf", (void **) &pglTexParameterf},
	{"glTexParameterfv", (void **) &pglTexParameterfv},
	{"glTexParameteri", (void **) &pglTexParameteri},
	{"glHint", (void **) &pglHint},
	{"glPixelStoref", (void **) &pglPixelStoref},
	{"glPixelStorei", (void **) &pglPixelStorei},
	{"glGenTextures", (void **) &pglGenTextures},
	{"glDeleteTextures", (void **) &pglDeleteTextures},
	{"glBindTexture", (void **) &pglBindTexture},
	{"glTexImage1D", (void **) &pglTexImage1D},
	{"glTexImage2D", (void **) &pglTexImage2D},
	{"glTexSubImage1D", (void **) &pglTexSubImage1D},
	{"glTexSubImage2D", (void **) &pglTexSubImage2D},
	{"glCopyTexImage1D", (void **) &pglCopyTexImage1D},
	{"glCopyTexImage2D", (void **) &pglCopyTexImage2D},
	{"glCopyTexSubImage1D", (void **) &pglCopyTexSubImage1D},
	{"glCopyTexSubImage2D", (void **) &pglCopyTexSubImage2D},
	{"glScissor", (void **) &pglScissor},
	{"glPolygonOffset", (void **) &pglPolygonOffset},
	{"glPolygonMode", (void **) &pglPolygonMode},
	{"glPolygonStipple", (void **) &pglPolygonStipple},
	{"glClipPlane", (void **) &pglClipPlane},
	{"glGetClipPlane", (void **) &pglGetClipPlane},
	{"glShadeModel", (void **) &pglShadeModel},
	{NULL, NULL}
};

static dllfunc_t pointparametersfunc[] =
{
	{"glPointParameterfEXT", (void **) &pglPointParameterfEXT},
	{"glPointParameterfvEXT", (void **) &pglPointParameterfvEXT},
	{NULL, NULL}
};

static dllfunc_t drawrangeelementsfuncs[] =
{
	{"glDrawRangeElements", (void **) &pglDrawRangeElements},
	{NULL, NULL}
};

static dllfunc_t drawrangeelementsextfuncs[] =
{
	{"glDrawRangeElementsEXT", (void **) &pglDrawRangeElementsEXT},
	{NULL, NULL}
};

static dllfunc_t sgis_multitexturefuncs[] =
{
	{"glSelectTextureSGIS", (void **) &pglSelectTextureSGIS},
	{"glMTexCoord2fSGIS", (void **) &pglMTexCoord2fSGIS},
	{NULL, NULL}
};

static dllfunc_t multitexturefuncs[] =
{
	{"glMultiTexCoord1fARB", (void **) &pglMultiTexCoord1f},
	{"glMultiTexCoord2fARB", (void **) &pglMultiTexCoord2f},
	{"glMultiTexCoord3fARB", (void **) &pglMultiTexCoord3f},
	{"glMultiTexCoord4fARB", (void **) &pglMultiTexCoord4f},
	{"glActiveTextureARB", (void **) &pglActiveTextureARB},
	{"glClientActiveTextureARB", (void **) &pglClientActiveTexture},
	{"glClientActiveTextureARB", (void **) &pglClientActiveTextureARB},
	{NULL, NULL}
};

static dllfunc_t compiledvertexarrayfuncs[] =
{
	{"glLockArraysEXT", (void **) &pglLockArraysEXT},
	{"glUnlockArraysEXT", (void **) &pglUnlockArraysEXT},
	{"glDrawArrays", (void **) &pglDrawArrays},
	{NULL, NULL}
};

static dllfunc_t texture3dextfuncs[] =
{
	{"glTexImage3DEXT", (void **) &pglTexImage3D},
	{"glTexSubImage3DEXT", (void **) &pglTexSubImage3D},
	{"glCopyTexSubImage3DEXT", (void **) &pglCopyTexSubImage3D},
	{NULL, NULL}
};

static dllfunc_t atiseparatestencilfuncs[] =
{
	{"glStencilOpSeparateATI", (void **) &pglStencilOpSeparate},
	{"glStencilFuncSeparateATI", (void **) &pglStencilFuncSeparate},
	{NULL, NULL}
};

static dllfunc_t gl2separatestencilfuncs[] =
{
	{"glStencilOpSeparate", (void **) &pglStencilOpSeparate},
	{"glStencilFuncSeparate", (void **) &pglStencilFuncSeparate},
	{NULL, NULL}
};

static dllfunc_t stenciltwosidefuncs[] =
{
	{"glActiveStencilFaceEXT", (void **) &pglActiveStencilFaceEXT},
	{NULL, NULL}
};

static dllfunc_t blendequationfuncs[] =
{
	{"glBlendEquationEXT", (void **) &pglBlendEquationEXT},
	{NULL, NULL}
};

static dllfunc_t shaderobjectsfuncs[] =
{
	{"glDeleteObjectARB", (void **) &pglDeleteObjectARB},
	{"glGetHandleARB", (void **) &pglGetHandleARB},
	{"glDetachObjectARB", (void **) &pglDetachObjectARB},
	{"glCreateShaderObjectARB", (void **) &pglCreateShaderObjectARB},
	{"glShaderSourceARB", (void **) &pglShaderSourceARB},
	{"glCompileShaderARB", (void **) &pglCompileShaderARB},
	{"glCreateProgramObjectARB", (void **) &pglCreateProgramObjectARB},
	{"glAttachObjectARB", (void **) &pglAttachObjectARB},
	{"glLinkProgramARB", (void **) &pglLinkProgramARB},
	{"glUseProgramObjectARB", (void **) &pglUseProgramObjectARB},
	{"glValidateProgramARB", (void **) &pglValidateProgramARB},
	{"glUniform1fARB", (void **) &pglUniform1fARB},
	{"glUniform2fARB", (void **) &pglUniform2fARB},
	{"glUniform3fARB", (void **) &pglUniform3fARB},
	{"glUniform4fARB", (void **) &pglUniform4fARB},
	{"glUniform1iARB", (void **) &pglUniform1iARB},
	{"glUniform2iARB", (void **) &pglUniform2iARB},
	{"glUniform3iARB", (void **) &pglUniform3iARB},
	{"glUniform4iARB", (void **) &pglUniform4iARB},
	{"glUniform1fvARB", (void **) &pglUniform1fvARB},
	{"glUniform2fvARB", (void **) &pglUniform2fvARB},
	{"glUniform3fvARB", (void **) &pglUniform3fvARB},
	{"glUniform4fvARB", (void **) &pglUniform4fvARB},
	{"glUniform1ivARB", (void **) &pglUniform1ivARB},
	{"glUniform2ivARB", (void **) &pglUniform2ivARB},
	{"glUniform3ivARB", (void **) &pglUniform3ivARB},
	{"glUniform4ivARB", (void **) &pglUniform4ivARB},
	{"glUniformMatrix2fvARB", (void **) &pglUniformMatrix2fvARB},
	{"glUniformMatrix3fvARB", (void **) &pglUniformMatrix3fvARB},
	{"glUniformMatrix4fvARB", (void **) &pglUniformMatrix4fvARB},
	{"glGetObjectParameterfvARB", (void **) &pglGetObjectParameterfvARB},
	{"glGetObjectParameterivARB", (void **) &pglGetObjectParameterivARB},
	{"glGetInfoLogARB", (void **) &pglGetInfoLogARB},
	{"glGetAttachedObjectsARB", (void **) &pglGetAttachedObjectsARB},
	{"glGetUniformLocationARB", (void **) &pglGetUniformLocationARB},
	{"glGetActiveUniformARB", (void **) &pglGetActiveUniformARB},
	{"glGetUniformfvARB", (void **) &pglGetUniformfvARB},
	{"glGetUniformivARB", (void **) &pglGetUniformivARB},
	{"glGetShaderSourceARB", (void **) &pglGetShaderSourceARB},
	{"glVertexAttribPointerARB", (void **) &pglVertexAttribPointerARB},
	{"glEnableVertexAttribArrayARB", (void **) &pglEnableVertexAttribArrayARB},
	{"glDisableVertexAttribArrayARB", (void **) &pglDisableVertexAttribArrayARB},
	{"glBindAttribLocationARB", (void **) &pglBindAttribLocationARB},
	{"glGetActiveAttribARB", (void **) &pglGetActiveAttribARB},
	{"glGetAttribLocationARB", (void **) &pglGetAttribLocationARB},
	{NULL, NULL}
};

static dllfunc_t vertexshaderfuncs[] =
{
	{"glVertexAttribPointerARB", (void **) &pglVertexAttribPointerARB},
	{"glEnableVertexAttribArrayARB", (void **) &pglEnableVertexAttribArrayARB},
	{"glDisableVertexAttribArrayARB", (void **) &pglDisableVertexAttribArrayARB},
	{"glBindAttribLocationARB", (void **) &pglBindAttribLocationARB},
	{"glGetActiveAttribARB", (void **) &pglGetActiveAttribARB},
	{"glGetAttribLocationARB", (void **) &pglGetAttribLocationARB},
	{NULL, NULL}
};

static dllfunc_t cgprogramfuncs[] =
{
	{"glBindProgramARB", (void **) &pglBindProgramARB},
	{"glDeleteProgramsARB", (void **) &pglDeleteProgramsARB},
	{"glGenProgramsARB", (void **) &pglGenProgramsARB},
	{"glProgramStringARB", (void **) &pglProgramStringARB},
	{"glProgramEnvParameter4fARB", (void **) &pglProgramEnvParameter4fARB},
	{"glProgramLocalParameter4fARB", (void **) &pglProgramLocalParameter4fARB},
	{NULL, NULL}
};

static dllfunc_t vbofuncs[] =
{
	{"glBindBufferARB"    , (void **) &pglBindBufferARB},
	{"glDeleteBuffersARB" , (void **) &pglDeleteBuffersARB},
	{"glGenBuffersARB"    , (void **) &pglGenBuffersARB},
	{"glIsBufferARB"      , (void **) &pglIsBufferARB},
	{"glMapBufferARB"     , (void **) &pglMapBufferARB},
	{"glUnmapBufferARB"   , (void **) &pglUnmapBufferARB},
	{"glBufferDataARB"    , (void **) &pglBufferDataARB},
	{"glBufferSubDataARB" , (void **) &pglBufferSubDataARB},
	{NULL, NULL}
};

static dllfunc_t occlusionfunc[] =
{
	{"glGenQueriesARB"       , (void **) &pglGenQueriesARB},
	{"glDeleteQueriesARB"    , (void **) &pglDeleteQueriesARB},
	{"glIsQueryARB"          , (void **) &pglIsQueryARB},
	{"glBeginQueryARB"       , (void **) &pglBeginQueryARB},
	{"glEndQueryARB"         , (void **) &pglEndQueryARB},
	{"glGetQueryivARB"       , (void **) &pglGetQueryivARB},
	{"glGetQueryObjectivARB" , (void **) &pglGetQueryObjectivARB},
	{"glGetQueryObjectuivARB", (void **) &pglGetQueryObjectuivARB},
	{NULL, NULL}
};

static dllfunc_t texturecompressionfuncs[] =
{
	{"glCompressedTexImage3DARB",    (void **) &pglCompressedTexImage3DARB},
	{"glCompressedTexImage2DARB",    (void **) &pglCompressedTexImage2DARB},
	{"glCompressedTexImage1DARB",    (void **) &pglCompressedTexImage1DARB},
	{"glCompressedTexSubImage3DARB", (void **) &pglCompressedTexSubImage3DARB},
	{"glCompressedTexSubImage2DARB", (void **) &pglCompressedTexSubImage2DARB},
	{"glCompressedTexSubImage1DARB", (void **) &pglCompressedTexSubImage1DARB},
	{"glGetCompressedTexImageARB",   (void **) &pglGetCompressedTexImage},
	{NULL, NULL}
};

/*
=================
R_RenderInfo_f
=================
*/
void R_RenderInfo_f( void )
{

	Msg("\n");
	Msg("GL_VENDOR: %s\n", glConfig.vendor_string);
	Msg("GL_RENDERER: %s\n", glConfig.renderer_string);
	Msg("GL_VERSION: %s\n", glConfig.version_string);
	Msg("GL_EXTENSIONS: %s\n", glConfig.extensions_string);

	Msg("GL_MAX_TEXTURE_SIZE: %i\n", glConfig.max_2d_texture_size );
	
	if( GL_Support( R_ARB_MULTITEXTURE ))
		Msg("GL_MAX_TEXTURE_UNITS_ARB: %i\n", glConfig.max_texture_units );
	if( GL_Support( R_TEXTURECUBEMAP_EXT ))
		Msg("GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB: %i\n", glConfig.max_cubemap_texture_size );
	if( GL_Support( R_ANISOTROPY_EXT ))
		Msg("GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: %.1f\n", glConfig.max_texture_anisotropy );
	if( glConfig.texRectangle )
		Msg("GL_MAX_RECTANGLE_TEXTURE_SIZE_NV: %i\n", glConfig.max_2d_rectangle_size );

	Msg("\n");
	Msg("MODE: %i, %i x %i %s\n", r_mode->integer, r_width->integer, r_height->integer, (glState.fullScreen) ? "fullscreen" : "windowed" );
	Msg("GAMMA: %s w/ %i overbright bits\n", (glConfig.deviceSupportsGamma) ? "hardware" : "software", (glConfig.deviceSupportsGamma) ? r_overbrightbits->integer : 0 );
	Msg("\n");
	Msg( "CDS: %s\n", glConfig.allowCDS ? "enabled" : "disabled" );
	Msg( "PICMIP: %i\n", r_picmip->integer );
	Msg( "TEXTUREMODE: %s\n", gl_texturemode->string );
	Msg( "VERTICAL SYNC: %s\n", r_swapInterval->integer ? "enabled" : "disabled" );
}

//=======================================================================

void GL_InitCommands( void )
{
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
	r_physbdebug = Cvar_Get( "cm_debugdraw", "0", CVAR_ARCHIVE, "draw physics hulls" );

	r_bloom = Cvar_Get( "r_bloom", "0", CVAR_ARCHIVE, "enable or disable bloom-effect" );
	r_bloom_alpha = Cvar_Get( "r_bloom_alpha", "0.2", CVAR_ARCHIVE, "bloom alpha multiplier" );
	r_bloom_diamond_size = Cvar_Get( "r_bloom_diamond_size", "8", CVAR_ARCHIVE, "bloom diamond size (can be 1, 2, 4, 6 or 8)" );
	r_bloom_intensity = Cvar_Get( "r_bloom_intensity", "1.3", CVAR_ARCHIVE, "bloom intensity scale" );
	r_bloom_darken = Cvar_Get( "r_bloom_darken", "4", CVAR_ARCHIVE, "bloom darken scale" );
	r_bloom_sample_size = Cvar_Get( "r_bloom_sample_size", "320", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "bloom rendering viewport size (must be power of two)" );
	r_bloom_fast_sample = Cvar_Get( "r_bloom_fast_sample", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "enable fast bloom pass" );

	r_environment_color = Cvar_Get( "r_environment_color", "128 128 128", CVAR_ARCHIVE, "map environment light color" );
	r_ignorehwgamma = Cvar_Get( "r_ignorehwgamma", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "ignore hardware gamma (e.g. not support)" );
	r_overbrightbits = Cvar_Get( "r_overbrightbits", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "renderer overbright bits" );
	r_mapoverbrightbits = Cvar_Get( "r_mapoverbrightbits", "2", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "current map overbright bits" );
	r_vertexbuffers = Cvar_Get( "r_vertexbuffers", "0", CVAR_ARCHIVE, "store vertex data in VBOs" );

	r_detailtextures = Cvar_Get( "r_detailtextures", "1", CVAR_ARCHIVE, "enable or disable detail textures" );
	r_flares = Cvar_Get( "r_flares", "0", CVAR_ARCHIVE, "enable flares rendering" );
	r_flaresize = Cvar_Get( "r_flaresize", "40", CVAR_ARCHIVE, "override shader flare size" );
	r_flarefade = Cvar_Get( "r_flarefade", "3", CVAR_ARCHIVE, "override shader flare fading" );

	r_dynamiclight = Cvar_Get( "r_dynamiclight", "1", CVAR_ARCHIVE, "allow dinamic lights on a map" );
	r_coronascale = Cvar_Get( "r_coronascale", "0.2", 0, "light coronas scale" );
	r_subdivisions = Cvar_Get( "r_subdivisions", "4", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "bezier curve subdivision" );
	r_faceplanecull = Cvar_Get( "r_faceplanecull", "1", CVAR_ARCHIVE, "culling face planes" );
	r_shownormals = Cvar_Get( "r_shownormals", "0", CVAR_CHEAT, "show mesh normals" );
	r_showtextures = Cvar_Get("r_showtextures", "0", CVAR_CHEAT, "show all uploaded textures" );
	r_draworder = Cvar_Get( "r_draworder", "0", CVAR_CHEAT, "ignore mesh sorting" );

	r_fastsky = Cvar_Get( "r_fastsky", "0", CVAR_ARCHIVE, "enable algorhytem fo fast sky rendering (for old machines)" );
	r_portalonly = Cvar_Get( "r_portalonly", "0", 0, "render only portals" );
	r_portalmaps = Cvar_Get( "r_portalmaps", "1", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "use portal maps for portal rendering" );
	r_portalmaps_maxtexsize = Cvar_Get( "r_portalmaps_maxtexsize", "512", CVAR_ARCHIVE, "portal maps texture dims" );

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

	r_outlines_world = Cvar_Get( "r_outlines_world", "1.8", CVAR_ARCHIVE, "cel-shading world outline thinkness" );
	r_outlines_scale = Cvar_Get( "r_outlines_scale", "1", CVAR_ARCHIVE, "outilines scale" );
	r_outlines_cutoff = Cvar_Get( "r_outlines_cutoff", "712", CVAR_ARCHIVE, "cutoff factor" );

	r_lodbias = Cvar_Get( "r_lodbias", "0", CVAR_ARCHIVE, "md3 or skm lod bias" );
	r_lodscale = Cvar_Get( "r_lodscale", "5.0", CVAR_ARCHIVE, "md3 or skm LOD scale factor" );

	r_gamma = Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE, "gamma amount" );
	r_colorbits = Cvar_Get( "r_colorbits", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO, "pixelformat color bits (0 - auto)" );
	r_depthbits = Cvar_Get( "r_depthbits", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO, "pixelformat depth bits (0 - auto)" );
	r_texturebits = Cvar_Get( "r_texturebits", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO, "no description" );
	gl_texturemode = Cvar_Get( "gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE, "texture filter" );
	gl_texture_anisotropy = Cvar_Get( "r_anisotropy", "2.0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO, "textures anisotropic filter" );
	gl_texture_lodbias = Cvar_Get( "r_texture_lodbias", "0.0", CVAR_ARCHIVE, "LOD bias for mipmapped textures" );
	gl_round_down = Cvar_Get( "gl_round_down", "0", CVAR_SYSTEMINFO, "down size non-power of two textures" );
	gl_compress_textures = Cvar_Get( "gl_compress_textures", "0", CVAR_ARCHIVE|CVAR_LATCH, "compress textures in video memory" ); 
	r_stencilbits = Cvar_Get( "r_stencilbits", "8", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "pixelformat stencil bits (0 - auto)" );
	r_check_errors = Cvar_Get("r_check_errors", "1", CVAR_ARCHIVE, "ignore video engine errors" );
	r_swapInterval = Cvar_Get( "r_swapInterval", "0", CVAR_ARCHIVE,  "time beetween frames (in msec)" );

	// make sure r_swapinterval is checked after vid_restart
	r_swapInterval->modified = true;

	gl_finish = Cvar_Get( "gl_finish", "0", CVAR_ARCHIVE, "use glFinish instead of glFlush" );
	gl_delayfinish = Cvar_Get( "gl_delayfinish", "1", CVAR_ARCHIVE, "no description" );
	gl_cull = Cvar_Get( "gl_cull", "1", 0, "allow GL_CULL_FACE" );
	gl_driver = Cvar_Get( "gl_driver", "opengl32.dll", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "OpenGl default driver name" );
	r_frontbuffer = Cvar_Get( "r_frontbuffer", "0", 0, "use back or front buffer" );
	gl_extensions = Cvar_Get( "gl_extensions", "1", CVAR_SYSTEMINFO, "allow gl_extensions" );

	vid_displayfrequency = Cvar_Get ( "vid_displayfrequency", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "fullscreen refresh rate" );
	vid_fullscreen = Cvar_Get( "fullscreen", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO, "set in 1 to enable fullscreen mode" );

	Cmd_AddCommand( "texturelist", R_TextureList_f, "display loaded textures list" );
	Cmd_AddCommand( "shaderlist", R_ShaderList_f, "display loaded shaders list" );
	Cmd_AddCommand( "shaderdump", R_ShaderDump_f, "dump specified shader into console" );
	Cmd_AddCommand( "modellist", Mod_Modellist_f, "display loaded models list" );
	Cmd_AddCommand( "r_info", R_RenderInfo_f, "display openGL supported extensions" );
	Cmd_AddCommand( "glslprogramlist", R_ProgramList_f, "display loaded GLSL shaders list" );
	Cmd_AddCommand( "glslprogramdump", R_ProgramDump_f, "sump GLSL shaders into text file" );
}

void GL_RemoveCommands( void )
{
	Cmd_RemoveCommand( "modellist" );
	Cmd_RemoveCommand( "shaderlist" );
	Cmd_RemoveCommand( "shaderdump" );
	Cmd_RemoveCommand( "modellist" );
	Cmd_RemoveCommand( "texturelist" );
	Cmd_RemoveCommand( "glslprogramlist" );
	Cmd_RemoveCommand( "glslprogramdump" );
	Cmd_RemoveCommand( "r_info");
}

void GL_InitBackend( void )
{
	char	dev_level[4];

	GL_InitCommands();

	glw_state.wndproc = ri.WndProc;
	glw_state.hInst = GetModuleHandle( NULL );
	r_temppool = Mem_AllocPool( "Render Memory" );

	// check developer mode
	if(FS_GetParmFromCmdLine( "-dev", dev_level ))
		glw_state.developer = com.atoi( dev_level );

	GL_SetDefaultState();
}

void GL_ShutdownBackend( void )
{
	if( r_framebuffer ) Mem_Free( r_framebuffer );
	GL_RemoveCommands();

	Mem_FreePool( &r_temppool );
}

void GL_SetExtension( int r_ext, int enable )
{
	if( r_ext >= 0 && r_ext < R_EXTCOUNT )
		glConfig.extension[r_ext] = enable ? GL_TRUE : GL_FALSE;
	else MsgDev( D_ERROR, "GL_SetExtension: invalid extension %d\n", r_ext );
}

bool GL_Support( int r_ext )
{
	if( r_ext >= 0 && r_ext < R_EXTCOUNT )
		return glConfig.extension[r_ext] ? true : false;
	MsgDev( D_ERROR, "GL_Support: invalid extension %d\n", r_ext );
	return false;		
}

void *GL_GetProcAddress( const char *name )
{
	void	*p = NULL;

	if( pwglGetProcAddress != NULL )
		p = (void *)pwglGetProcAddress( name );
	if( !p ) p = (void *)Sys_GetProcAddress( &opengl_dll, name );

	return p;
}

void GL_CheckExtension( const char *name, const dllfunc_t *funcs, const char *cvarname, int r_ext )
{
	const dllfunc_t	*func;
	cvar_t		*parm;

	MsgDev( D_NOTE, "GL_CheckExtension: %s ", name );

	if( gl_extensions->integer == 0 && r_ext != R_OPENGL_110 )
	{
		GL_SetExtension( r_ext, 0 );	// update render info
		return;
	}

	if( cvarname )
	{
		// system config disable extensions
		parm = Cvar_Get( cvarname, "1", CVAR_SYSTEMINFO, va( "enable or disable %s", name ));
		GL_SetExtension( r_ext, parm->integer );	// update render info
		if( parm->integer == 0 )
		{
			MsgDev( D_NOTE, "- disabled\n");
			return; // nothing to process at
		}
	}

	if((name[2] == '_' || name[3] == '_') && !com.strstr( glConfig.extensions_string, name ))
	{
		GL_SetExtension( r_ext, false );	// update render info
		MsgDev( D_NOTE, "- failed\n");
		return;
	}

	// clear exports
	for( func = funcs; func && func->name; func++ )
		*func->func = NULL;

	GL_SetExtension( r_ext, true ); // predict extension state
	for( func = funcs; func && func->name != NULL; func++ )
	{
		// functions are cleared before all the extensions are evaluated
		if(!(*func->func = (void *)GL_GetProcAddress( func->name )))
			GL_SetExtension( r_ext, false ); // one or more functions are invalid, extension will be disabled
	}
	if(GL_Support( r_ext )) MsgDev( D_NOTE, "- enabled\n");
}

void GL_BuildGammaTable( void )
{
	int         i, v;
	double	invGamma, div;

	invGamma = 1.0 / bound( 0.5f, r_gamma->value, 3.0f );
	div = (double)( 1 << max( 0, r_overbrightbits->integer )) / 255.5f;
	Mem_Copy( glState.gammaRamp, glState.stateRamp, sizeof( glState.gammaRamp ));
	
	for( i = 0; i < 256; i++ )
	{
		v = (int)(65535.0 * pow(((double)i + 0.5 ) * div, invGamma ) + 0.5 );
		glState.gammaRamp[i+0]   = ((word)bound( 0, v, 65535 ));
		glState.gammaRamp[i+256] = ((word)bound( 0, v, 65535 ));
		glState.gammaRamp[i+512] = ((word)bound( 0, v, 65535 ));
	}
}

void GL_UpdateGammaRamp( void )
{
	if( r_ignorehwgamma->integer ) return;
	if( !glConfig.deviceSupportsGamma ) return;

	GL_BuildGammaTable();

	if( pwglGetDeviceGammaRamp3DFX )
		pwglSetDeviceGammaRamp3DFX( glw_state.hDC, glState.gammaRamp );
	else SetDeviceGammaRamp( glw_state.hDC, glState.gammaRamp );
}

/*
===============
GL_UpdateSwapInterval
===============
*/
void GL_UpdateSwapInterval( void )
{
	if( r_swapInterval->modified )
	{
		r_swapInterval->modified = false;

		if( pwglSwapIntervalEXT )
			pwglSwapIntervalEXT( r_swapInterval->integer );
	}
}

/*
===============
GL_SetDefaultTexState
===============
*/
static void GL_SetDefaultTexState( void )
{
	Mem_Set( glState.currentTextures, -1, MAX_TEXTURE_UNITS * sizeof( *glState.currentTextures ));
	Mem_Set( glState.currentEnvModes, -1, MAX_TEXTURE_UNITS * sizeof( *glState.currentEnvModes ));
	Mem_Set( glState.texIdentityMatrix, 0, MAX_TEXTURE_UNITS * sizeof( *glState.texIdentityMatrix ));
	Mem_Set( glState.genSTEnabled, 0, MAX_TEXTURE_UNITS * sizeof( *glState.genSTEnabled ));
	Mem_Set( glState.texCoordArrayMode, 0, MAX_TEXTURE_UNITS * sizeof( *glState.texCoordArrayMode ));
}

/*
===============
GL_SetDefaultState
===============
*/
static void GL_SetDefaultState( void )
{
	Mem_Set( &glState, 0, sizeof( glState ));
	GL_SetDefaultTexState ();

	glState.initializedMedia = false;
}

/*
===============
GL_SetDefaults
===============
*/
static void GL_SetDefaults( void )
{
	int	i;

	pglFinish();

	pglClearColor( 1, 0, 0.5, 0.5 );

	pglEnable( GL_DEPTH_TEST );
	pglDisable( GL_CULL_FACE );
	pglEnable( GL_SCISSOR_TEST );
	pglDepthFunc( GL_LEQUAL );
	pglDepthMask( GL_FALSE );

	pglColor4f( 1, 1, 1, 1 );

	if( glState.stencilEnabled )
	{
		pglDisable( GL_STENCIL_TEST );
		pglStencilMask( ( GLuint ) ~0 );
		pglStencilFunc( GL_EQUAL, 128, 0xFF );
		pglStencilOp( GL_KEEP, GL_KEEP, GL_INCR );
	}

	// enable gouraud shading
	pglShadeModel( GL_SMOOTH );

	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	pglPolygonOffset( -1, -2 );

	// properly disable multitexturing at startup
	for( i = glConfig.max_texture_units - 1; i > 0; i-- )
	{
		GL_SelectTexture( i );
		GL_TexEnv( GL_MODULATE );
		pglDisable( GL_BLEND );
		pglDisable( GL_TEXTURE_2D );
	}

	GL_SelectTexture( 0 );
	pglDisable( GL_BLEND );
	pglDisable( GL_ALPHA_TEST );
	pglDisable( GL_POLYGON_OFFSET_FILL );
	pglEnable( GL_TEXTURE_2D );

	GL_Cull( 0 );
	GL_FrontFace( 0 );

	GL_SetState( GLSTATE_DEPTHWRITE );
	GL_TexEnv( GL_MODULATE );

	R_SetTextureParameters();

	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
}

void GL_InitExtensions( void )
{
	int	flags = 0;

	// initialize gl extensions
	GL_CheckExtension( "OpenGL 1.1.0", opengl_110funcs, NULL, R_OPENGL_110 );
	if( !r_framebuffer ) r_framebuffer = Mem_Alloc( r_temppool, r_width->integer * r_height->integer * 4 );

	// get our various GL strings
	glConfig.vendor_string = pglGetString( GL_VENDOR );
	glConfig.renderer_string = pglGetString( GL_RENDERER );
	glConfig.version_string = pglGetString( GL_VERSION );
	glConfig.extensions_string = pglGetString( GL_EXTENSIONS );
	MsgDev( D_INFO, "Video: %s\n", glConfig.renderer_string );

	GL_CheckExtension( "WGL_3DFX_gamma_control", wgl3DFXgammacontrolfuncs, NULL, R_WGL_3DFX_GAMMA_CONTROL );
	GL_CheckExtension( "WGL_EXT_swap_control", wglswapintervalfuncs, NULL, R_WGL_SWAPCONTROL );
	GL_CheckExtension( "glDrawRangeElements", drawrangeelementsfuncs, "gl_drawrangeelments", R_DRAW_RANGEELEMENTS_EXT );
	if(!GL_Support( R_DRAW_RANGEELEMENTS_EXT )) GL_CheckExtension("GL_EXT_draw_range_elements", drawrangeelementsextfuncs, "gl_drawrangeelments", R_DRAW_RANGEELEMENTS_EXT );

	// multitexture
	glConfig.max_texture_units = 1;
	GL_CheckExtension("GL_ARB_multitexture", multitexturefuncs, "gl_arb_multitexture", R_ARB_MULTITEXTURE );
	if(GL_Support( R_ARB_MULTITEXTURE ))
	{
		pglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &glConfig.max_texture_units );
		GL_CheckExtension( "GL_ARB_texture_env_combine", NULL, "gl_texture_env_combine", R_COMBINE_EXT );
		if(!GL_Support( R_COMBINE_EXT )) GL_CheckExtension("GL_EXT_texture_env_combine", NULL, "gl_texture_env_combine", R_COMBINE_EXT );
		if(GL_Support( R_COMBINE_EXT )) GL_CheckExtension( "GL_ARB_texture_env_dot3", NULL, "gl_texture_env_dot3", R_DOT3_ARB_EXT );
	}
	else
	{
		GL_CheckExtension( "GL_SGIS_multitexture", sgis_multitexturefuncs, "gl_sgis_multitexture", R_ARB_MULTITEXTURE );
		if( GL_Support( R_ARB_MULTITEXTURE )) glConfig.max_texture_units = 2;
	}
	if( glConfig.max_texture_units == 1 ) GL_SetExtension( R_ARB_MULTITEXTURE, false );

	// 3d texture support
	GL_CheckExtension( "GL_EXT_texture3D", texture3dextfuncs, "gl_texture_3d", R_TEXTURE_3D_EXT );
	if(GL_Support( R_TEXTURE_3D_EXT ))
	{
		pglGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &glConfig.max_3d_texture_size );
		if( glConfig.max_3d_texture_size < 32 )
		{
			GL_SetExtension( R_TEXTURE_3D_EXT, false );
			MsgDev( D_ERROR, "GL_EXT_texture3D reported bogus GL_MAX_3D_TEXTURE_SIZE, disabled\n" );
		}
	}

	GL_CheckExtension( "GL_SGIS_generate_mipmap", NULL, "gl_sgis_generate_mipmaps", R_SGIS_MIPMAPS_EXT );

	// hardware cubemaps
	GL_CheckExtension( "GL_ARB_texture_cube_map", NULL, "gl_texture_cubemap", R_TEXTURECUBEMAP_EXT );
	if(GL_Support( R_TEXTURECUBEMAP_EXT )) pglGetIntegerv( GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &glConfig.max_cubemap_texture_size );

	// point particles extension
	GL_CheckExtension( "GL_EXT_point_parameters", pointparametersfunc, NULL, R_EXT_POINTPARAMETERS );

	GL_CheckExtension( "GL_ARB_texture_non_power_of_two", NULL, "gl_texture_npot", R_ARB_TEXTURE_NPOT_EXT );
	GL_CheckExtension( "GL_ARB_texture_compression", texturecompressionfuncs, "gl_dds_hardware_support", R_TEXTURE_COMPRESSION_EXT );
	GL_CheckExtension( "GL_EXT_compiled_vertex_array", compiledvertexarrayfuncs, "gl_cva_support", R_CUSTOM_VERTEX_ARRAY_EXT );
	if(!GL_Support(R_CUSTOM_VERTEX_ARRAY_EXT)) GL_CheckExtension( "GL_SGI_compiled_vertex_array", compiledvertexarrayfuncs, "gl_cva_support", R_CUSTOM_VERTEX_ARRAY_EXT );		
	GL_CheckExtension( "GL_EXT_texture_edge_clamp", NULL, "gl_clamp_to_edge", R_CLAMPTOEDGE_EXT );
	if(!GL_Support( R_CLAMPTOEDGE_EXT )) GL_CheckExtension("GL_SGIS_texture_edge_clamp", NULL, "gl_clamp_to_edge", R_CLAMPTOEDGE_EXT );

	glConfig.max_texture_anisotropy = 0.0f;
	GL_CheckExtension( "GL_EXT_texture_filter_anisotropic", NULL, "gl_ext_anisotropic_filter", R_ANISOTROPY_EXT );
	if(GL_Support( R_ANISOTROPY_EXT )) pglGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glConfig.max_texture_anisotropy );

	GL_CheckExtension( "GL_EXT_texture_lod_bias", NULL, "gl_ext_texture_lodbias", R_TEXTURE_LODBIAS );
	if(GL_Support( R_TEXTURE_LODBIAS )) pglGetFloatv( GL_MAX_TEXTURE_LOD_BIAS_EXT, &glConfig.max_texture_lodbias );

	GL_CheckExtension( "GL_ARB_texture_border_clamp", NULL, "gl_ext_texborder_clamp", R_CLAMP_TEXBORDER_EXT );

	GL_CheckExtension( "GL_EXT_blend_minmax", blendequationfuncs, "gl_ext_customblend", R_BLEND_MINMAX_EXT );
	GL_CheckExtension( "GL_EXT_blend_subtract", blendequationfuncs, "gl_ext_customblend", R_BLEND_SUBTRACT_EXT );

	GL_CheckExtension( "glStencilOpSeparate", gl2separatestencilfuncs, "gl_separate_stencil",R_SEPARATESTENCIL_EXT );
	if(!GL_Support( R_SEPARATESTENCIL_EXT )) GL_CheckExtension("GL_ATI_separate_stencil", atiseparatestencilfuncs, "gl_separate_stencil", R_SEPARATESTENCIL_EXT );

	GL_CheckExtension( "GL_EXT_stencil_two_side", stenciltwosidefuncs, "gl_stenciltwoside", R_STENCILTWOSIDE_EXT );
	GL_CheckExtension( "GL_ARB_vertex_buffer_object", vbofuncs, "gl_vertex_buffer_object", R_ARB_VERTEX_BUFFER_OBJECT_EXT );

	// we don't care if it's an extension or not, they are identical functions, so keep it simple in the rendering code
	if( pglDrawRangeElementsEXT == NULL ) pglDrawRangeElementsEXT = pglDrawRangeElements;

	GL_CheckExtension( "GL_ARB_vertex_program", cgprogramfuncs, "gl_vertexprogram", R_VERTEX_PROGRAM_EXT );
	GL_CheckExtension( "GL_ARB_fragment_program", cgprogramfuncs, "gl_fragmentprogram", R_FRAGMENT_PROGRAM_EXT );
	GL_CheckExtension( "GL_ARB_texture_env_add", NULL, "gl_texture_env_add", R_TEXTURE_ENV_ADD_EXT );

	// vp and fp shaders
	GL_CheckExtension( "GL_ARB_shader_objects", shaderobjectsfuncs, "gl_shaderobjects", R_SHADER_OBJECTS_EXT );
	GL_CheckExtension( "GL_ARB_shading_language_100", NULL, "gl_glslprogram", R_SHADER_GLSL100_EXT );
	GL_CheckExtension( "GL_ARB_vertex_shader", vertexshaderfuncs, "gl_vertexshader", R_VERTEX_SHADER_EXT );
	GL_CheckExtension( "GL_ARB_fragment_shader", NULL, "gl_pixelshader", R_FRAGMENT_SHADER_EXT );

	GL_CheckExtension( "GL_ARB_depth_texture", NULL, "gl_depthtexture", R_DEPTH_TEXTURE );
	GL_CheckExtension( "GL_ARB_shadow", NULL, "gl_arb_shadow", R_SHADOW_EXT );
	GL_CheckExtension( "GLSL_no_half_types", NULL, "glsl_no_half_types", R_GLSL_NO_HALF_TYPES );
	GL_CheckExtension( "GLSL_branching", NULL, "glsl_branching", R_GLSL_BRANCHING );

	// occlusion queries
	GL_CheckExtension( "GL_ARB_occlusion_query", occlusionfunc, "gl_occlusion_queries", R_OCCLUSION_QUERIES_EXT );

	// rectangle textures support
	if( com.strstr( glConfig.extensions_string, "GL_NV_texture_rectangle" ))
	{
		glConfig.texRectangle = GL_TEXTURE_RECTANGLE_NV;
		pglGetIntegerv( GL_MAX_RECTANGLE_TEXTURE_SIZE_NV, &glConfig.max_2d_rectangle_size );
	}
	else if( com.strstr( glConfig.extensions_string, "GL_EXT_texture_rectangle" ))
	{
		glConfig.texRectangle = GL_TEXTURE_RECTANGLE_EXT;
		pglGetIntegerv( GL_MAX_RECTANGLE_TEXTURE_SIZE_EXT, &glConfig.max_2d_rectangle_size );
	}
	else glConfig.texRectangle = glConfig.max_2d_rectangle_size = 0; // no rectangle

	glConfig.max_2d_texture_size = 0;
	pglGetIntegerv( GL_MAX_TEXTURE_SIZE, &glConfig.max_2d_texture_size );
	if( glConfig.max_2d_texture_size <= 0 ) glConfig.max_2d_texture_size = 256;

	Cvar_Get( "gl_max_texture_size", "0", CVAR_INIT, "opengl texture max dims" );
	Cvar_Set( "gl_max_texture_size", va( "%i", glConfig.max_2d_texture_size ));

	// MCD has buffering issues
	if(com.strstr( glConfig.renderer_string, "gdi" ))
		Cvar_SetValue( "gl_finish", 1 );

	glConfig.allowCDS = true;
	if( com.strstr( glConfig.renderer_string, "permedia" ) || com.strstr( glConfig.renderer_string, "glint" ))
	{
		if( r_3dlabs_broken->integer )
			glConfig.allowCDS = false;
		else glConfig.allowCDS = true;
	}

	Cvar_Set( "r_anisotropy", va( "%f", bound( 0, gl_texture_anisotropy->value, glConfig.max_texture_anisotropy )));

	if( GL_Support( R_TEXTURE_COMPRESSION_EXT )) flags |= IL_DDS_HARDWARE;
	flags |= IL_USE_LERPING|IL_ALLOW_OVERWRITE;

	Image_Init( NULL, flags );
	glw_state.initialized = true;
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
	R_InitShaders();
	R_InitModels();
	R_InitSkinFiles();
	R_InitCoronas();
	R_InitShadows();
	R_InitOcclusionQueries();
	R_InitCustomColors ();
	R_InitOutlines ();

	GL_SetDefaultTexState ();

	Mem_Set( &RI, 0, sizeof( refinst_t ));

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
	if( full )
	{
		GL_InitBackend();

		// create the window and set up the context
		if( !R_Init_OpenGL())
		{
			R_Free_OpenGL();
			return false;
		}

		GL_InitExtensions();
		GL_SetDefaults();

		R_BackendInit();
		R_ClearScene();
	}
	R_InitMedia();
	R_CheckForErrors();

	return true;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown( bool full )
{
	// free shaders, models, etc.
	R_FreeMedia();

	if( full )
	{
		GL_ShutdownBackend ();

		// shutdown rendering backend
		R_BackendShutdown();

		// shut down OS specific OpenGL stuff like contexts, etc.
		R_Free_OpenGL();
	}
}

/*
===============
R_NewMap

do some cleanup operations
===============
*/
void R_NewMap( void )
{
	R_ShutdownOcclusionQueries();
	R_FreeMeshLists();

	R_InitMeshLists();
	R_InitOcclusionQueries();

	R_InitLightStyles();	// clear lightstyles
	R_InitCustomColors();	// clear custom colors
	R_InitCoronas();		// update corona shader (because we can't make it static)

	GL_SetDefaultTexState ();
	Mem_Set( &RI, 0, sizeof( refinst_t ));	
	r_worldbrushmodel = NULL;	// during loading process
	r_worldmodel = NULL;
}