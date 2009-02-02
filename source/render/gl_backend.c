//=======================================================================
//			Copyright XashXT Group 2007 ©
//		     gl_backend.c - open gl backend utilites
//=======================================================================

#include "r_local.h" 
#include "mathlib.h" 
#include "matrix_lib.h" 

// set initial values
extern dll_info_t opengl_dll;

static dllfunc_t wglswapintervalfuncs[] =
{
	{"wglSwapIntervalEXT", (void **) &pwglSwapIntervalEXT},
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
	Msg("GL_VENDOR: %s\n", gl_config.vendor_string);
	Msg("GL_RENDERER: %s\n", gl_config.renderer_string);
	Msg("GL_VERSION: %s\n", gl_config.version_string);
	Msg("GL_EXTENSIONS: %s\n", gl_config.extensions_string);

	Msg("GL_MAX_TEXTURE_SIZE: %i\n", gl_config.max_2d_texture_size );
	
	if( GL_Support( R_ARB_MULTITEXTURE ))
		Msg("GL_MAX_TEXTURE_UNITS_ARB: %i\n", gl_config.textureunits );
	if( GL_Support( R_TEXTURECUBEMAP_EXT ))
		Msg("GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB: %i\n", gl_config.max_cubemap_texture_size );
	if( GL_Support( R_FRAGMENT_PROGRAM_EXT ))
	{
		Msg("GL_MAX_TEXTURE_COORDS_ARB: %i\n", gl_config.texturecoords );
		Msg("GL_MAX_TEXTURE_IMAGE_UNITS_ARB: %i\n", gl_config.teximageunits );
	}
	if( GL_Support( R_ANISOTROPY_EXT ))
		Msg("GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: %.1f\n", gl_config.max_anisotropy );
	if( gl_config.texRectangle )
		Msg("GL_MAX_RECTANGLE_TEXTURE_SIZE_NV: %i\n", gl_config.max_2d_rectangle_size );

	Msg("\n");
	Msg("MODE: %i, %i x %i %s\n", r_mode->integer, r_width->integer, r_height->integer, (gl_config.fullscreen) ? "fullscreen" : "windowed" );
	Msg("GAMMA: %s w/ %i overbright bits\n", (gl_config.deviceSupportsGamma) ? "hardware" : "software", (gl_config.deviceSupportsGamma) ? r_overbrightbits->integer : 0 );
	Msg("\n");
}


void GL_InitCommands( void )
{
	// system screen width and height (don't suppose for change from console at all)
	r_width = Cvar_Get("width", "640", CVAR_READ_ONLY, "screen width" );
	r_height = Cvar_Get("height", "480", CVAR_READ_ONLY, "screen height" );
	r_mode = Cvar_Get( "r_mode", "0", CVAR_ARCHIVE, "display resolution mode" );
	r_stencilbits = Cvar_Get( "r_stencilbits", "0", CVAR_ARCHIVE|CVAR_LATCH, "pixelformat stencil bits (0 - auto)" );
	r_colorbits = Cvar_Get( "r_colorbits", "0", CVAR_ARCHIVE|CVAR_LATCH, "pixelformat color bits (0 - auto)" );
	r_depthbits = Cvar_Get( "r_depthbits", "0", CVAR_ARCHIVE|CVAR_LATCH, "pixelformat depth bits (0 - auto)" );

	r_check_errors = Cvar_Get("r_check_errors", "1", CVAR_ARCHIVE, "ignore video engine errors" );
	r_lefthand = Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE, "viewmodel handedness" );
	r_norefresh = Cvar_Get ("r_norefresh", "0", 0, "no description" );
	r_fullbright = Cvar_Get ("r_fullbright", "0", CVAR_LATCH, "disable lightmaps" );
	r_drawentities = Cvar_Get ("r_drawentities", "1", CVAR_ARCHIVE, "render entities" );
	r_drawworld = Cvar_Get ("r_drawworld", "1", 0, "render world" );
	r_novis = Cvar_Get ("r_novis", "0", 0, "ignore vis information (perfomance test)");
	r_nocull = Cvar_Get ("r_nocull", "0", 0, "ignore frustrum culling (perfomance test)");
	r_speeds = Cvar_Get ("r_speeds", "0", 0, "shows r_speeds" );
	r_pause = Cvar_Get("paused", "0", 0, "renderer pause" );
	r_pause_bw = Cvar_Get("r_pause_effect", "0", CVAR_ARCHIVE, "allow pause effect" );
	r_physbdebug = Cvar_Get( "cm_debugdraw", "0", CVAR_ARCHIVE, "draw physics hulls" );
	r_himodels = Cvar_Get( "cl_himodels", "1", CVAR_ARCHIVE, "draw high-resolution models in multiplayer" );
	r_testmode = Cvar_Get("r_test", "0", CVAR_ARCHIVE, "developer cvar, for testing new effects" );

	r_lightlevel = Cvar_Get ("r_lightlevel", "0", 0, "no description" );

 	r_motionblur_intens = Cvar_Get( "r_motionblur_intens", "0.65", CVAR_ARCHIVE, "no description" );
	r_motionblur = Cvar_Get( "r_motionblur", "0", CVAR_ARCHIVE, "no description" );

	r_bloom = Cvar_Get( "r_bloom", "0", CVAR_ARCHIVE, "no description" );
	r_bloom_alpha = Cvar_Get( "r_bloom_alpha", "0.5", CVAR_ARCHIVE, "no description" );
	r_bloom_diamond_size = Cvar_Get( "r_bloom_diamond_size", "8", CVAR_ARCHIVE, "no description" );
	r_bloom_intensity = Cvar_Get( "r_bloom_intensity", "0.6", CVAR_ARCHIVE, "no description" );
	r_bloom_darken = Cvar_Get( "r_bloom_darken", "4", CVAR_ARCHIVE, "no description" );
	r_bloom_sample_size = Cvar_Get( "r_bloom_sample_size", "128", CVAR_ARCHIVE, "no description" );
	r_bloom_fast_sample = Cvar_Get( "r_bloom_fast_sample", "0", CVAR_ARCHIVE, "no description" );

	r_minimap_size = Cvar_Get ("r_minimap_size", "256", CVAR_ARCHIVE, "no description" );
	r_minimap_zoom = Cvar_Get ("r_minimap_zoom", "1", CVAR_ARCHIVE, "no description" );
	r_minimap_style = Cvar_Get ("r_minimap_style", "1", CVAR_ARCHIVE, "no description" );  
	r_minimap = Cvar_Get("r_minimap", "0", CVAR_ARCHIVE, "no description" ); 

	r_mirroralpha = Cvar_Get( "r_mirroralpha", "0.5", CVAR_ARCHIVE, "no description" );
	r_interpolate = Cvar_Get( "r_interpolate", "0", CVAR_ARCHIVE, "no description" );

	r_lightmap = Cvar_Get ("r_lightmap", "0", 0, "no description" );
	r_shadows = Cvar_Get ("r_shadows", "0", CVAR_ARCHIVE, "no description" );
	r_dynamiclights = Cvar_Get( "r_dynamic", "1", 0, "allow dynamic lights on level" );
	r_shadows = Cvar_Get( "r_shadows", "0", CVAR_ARCHIVE, "draw entity shadows" );
	r_caustics = Cvar_Get( "r_caustics", "0", CVAR_ARCHIVE, "draw underwater caustics" );
	r_showtris = Cvar_Get( "r_showtris", "0", 0, "show mesh triangles" );
	r_lockpvs = Cvar_Get( "r_lockpvs", "0", 0, "lockpvs area at current point (pvs test)" );
	r_fullscreen = Cvar_Get( "fullscreen", "0", CVAR_ARCHIVE, "set in 1 to enable fullscreen mode" );
	r_allow_software = Cvar_Get( "gl_software", "0", CVAR_ARCHIVE, "allow software gl acceleration" );

	r_nobind = Cvar_Get( "r_nobind", "0", CVAR_CHEAT, "disable all textures (perfomance test)" );
	r_drawparticles = Cvar_Get( "r_drawparticles", "1", CVAR_CHEAT, "disable particles (perfomance test)" );
	r_drawpolys = Cvar_Get( "r_drawpolys", "1", CVAR_CHEAT, "disable decal polygons (perfomance test)" );
	r_frontbuffer = Cvar_Get("r_frontBuffer", "0", CVAR_CHEAT, "directly draw into front buffer (perfomance test)" );
	r_showcluster = Cvar_Get("r_showcluster", "0", CVAR_CHEAT, "print info about current viewcluster" );
	r_shownormals = Cvar_Get("r_showNormals", "0", CVAR_CHEAT, "draw model normals" );
	r_showtangentspace = Cvar_Get("r_showTangentSpace", "0", CVAR_CHEAT, "draw model tangent space" );
	r_showmodelbounds = Cvar_Get("r_showmodelbounds", "0", CVAR_CHEAT, "draw entity bboxes" );
	r_showshadowvolumes = Cvar_Get("r_showshadowvolumes", "0", CVAR_CHEAT, "draw shadow volumes" );
	r_showtextures = Cvar_Get("r_showtextures", "0", CVAR_CHEAT, "show all uploaded textures" );
	r_offsetfactor = Cvar_Get("r_offsetfactor", "-1", CVAR_CHEAT, "z_trick offset factor" );
	r_offsetunits = Cvar_Get("r_offsetunits", "-2", CVAR_CHEAT, "z_trick offset uints" );
	r_debugsort = Cvar_Get( "r_debugsort", "0", CVAR_CHEAT, "enable custom z-sorting" );
	r_singleshader = Cvar_Get("r_singleshader", "0", CVAR_CHEAT|CVAR_LATCH, "apply default shader everywhere" );
	r_skipbackend = Cvar_Get("r_skipbackend", "0", CVAR_CHEAT, "skip 2d drawing (hud, console, etc)" );
	r_skipfrontend = Cvar_Get("r_skipfronend", "0", CVAR_CHEAT, "skip 3d drawing (scene)" );
	r_overbrightbits = Cvar_Get( "r_overbrightbits", "0", CVAR_ARCHIVE|CVAR_LATCH, "hardware gamma overbright" );
	r_showlightmaps = Cvar_Get( "r_showlightmaps", "0", CVAR_CHEAT, "show lightmap development tool" );
	r_caustics = Cvar_Get( "r_caustics", "0", CVAR_ARCHIVE, "enable water caustics" );

	r_modulate = Cvar_Get( "r_modulate", "1.0", CVAR_ARCHIVE|CVAR_LATCH, "modulate light" );
	r_ambientscale = Cvar_Get("r_ambientScale", "0.6", CVAR_ARCHIVE, "default ambient light level" );
	r_directedscale = Cvar_Get("r_directedScale", "1.0", CVAR_ARCHIVE, "direct light level" );
	r_intensity = Cvar_Get( "r_intensity", "1.0", CVAR_ARCHIVE|CVAR_LATCH, "textures upload intentsity" );
	r_texturefilter = Cvar_Get( "gl_texturefilter", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE, "texture filter" );
	r_texturefilteranisotropy = Cvar_Get( "r_anisotropy", "2.0", CVAR_ARCHIVE, "textures anisotropic filter" );
	r_texturelodbias = Cvar_Get( "r_texture_lodbias", "0.0", CVAR_ARCHIVE, "LOD bias for mipmapped textures" );
	r_max_texsize = Cvar_Get( "r_max_texsize", "512", CVAR_ARCHIVE|CVAR_LATCH, "maximum size for down sized textures" );
	r_max_normal_texsize = Cvar_Get( "r_max_normal_texsize", "256", CVAR_ARCHIVE|CVAR_LATCH, "maximum size for down sized normal map textures" );
	r_compress_textures = Cvar_Get( "r_compress_textures", "0", CVAR_ARCHIVE|CVAR_LATCH, "compress textures" );
	r_compress_normal_textures = Cvar_Get( "r_compress_normal_textures", "0", CVAR_ARCHIVE|CVAR_LATCH, "compress normal map textures" );
	r_round_down = Cvar_Get( "gl_round_down", "0", CVAR_SYSTEMINFO, "down size non-power of two textures" );
	r_detailtextures = Cvar_Get( "r_detailtextures", "0", CVAR_ARCHIVE|CVAR_LATCH, "allow detail textures" );
	r_vertexbuffers = Cvar_Get( "r_vertexbuffers", "1", CVAR_ARCHIVE, "store vertex data in VBOs" );

	r_swapInterval = Cvar_Get ("gl_swapinterval", "0", CVAR_ARCHIVE, "time beetween frames (in msec)" );
	gl_finish = Cvar_Get ("gl_finish", "0", CVAR_ARCHIVE, "no description" );
	gl_clear = Cvar_Get ("gl_clear", "0", 0, "clearing screen after each frame" );
	vid_gamma = Cvar_Get( "vid_gamma", "1", CVAR_ARCHIVE, "screen gamma" );

	Cmd_AddCommand( "modellist", R_ModelList_f, "display loaded models list" );
	Cmd_AddCommand( "shaderlist", R_ShaderList_f, "display loaded shaders list" );
	Cmd_AddCommand( "programlist", R_ProgramList_f, "display loaded glsl programs list" );
	Cmd_AddCommand( "texturelist", R_TextureList_f, "display loaded textures list" );
	Cmd_AddCommand( "r_info", R_RenderInfo_f, "display openGL supported extensions" );

	// range check some cvars
	if( vid_gamma->value > 3.0 ) Cvar_Set( "vid_gamma", "3.0" );
	else if( vid_gamma->value < 0.5 ) Cvar_Set( "vid_gamma", "0.5" );

	if( r_overbrightbits->integer > 2 ) Cvar_Set( "r_overbrightbits", "2" );
	else if( r_overbrightbits->integer < 0 ) Cvar_Set( "r_overbrightbits", "0" );

	if( r_modulate->value < 1.0 ) Cvar_Set( "r_modulate", "1.0" );
	if( r_intensity->value < 1.0) Cvar_Set( "r_intensity", "1.0" );
}

void GL_RemoveCommands( void )
{
	Cmd_RemoveCommand( "modellist" );
	Cmd_RemoveCommand( "shaderlist" );
	Cmd_RemoveCommand( "programlist" );
	Cmd_RemoveCommand( "texturelist" );
	Cmd_RemoveCommand( "r_vboinfo" );
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
	if(FS_GetParmFromCmdLine("-dev", dev_level ))
		glw_state.developer = com.atoi( dev_level );
}

void GL_ShutdownBackend( void )
{
	if( r_framebuffer ) Mem_Free( r_framebuffer );
	GL_RemoveCommands();
}

void GL_SetExtension( int r_ext, int enable )
{
	if( r_ext >= 0 && r_ext < R_EXTCOUNT )
		gl_config.extension[r_ext] = enable ? GL_TRUE : GL_FALSE;
	else MsgDev( D_ERROR, "GL_SetExtension: invalid extension %d\n", r_ext );
}

bool GL_Support( int r_ext )
{
	if( r_ext >= 0 && r_ext < R_EXTCOUNT )
		return gl_config.extension[r_ext] ? true : false;
	MsgDev( D_ERROR, "GL_Support: invalid extension %d\n", r_ext );
	return false;		
}

void *GL_GetProcAddress( const char *name )
{
	void	*p = NULL;

	if( pwglGetProcAddress != NULL )
		p = (void *)pwglGetProcAddress( name );
	if( !p )  p = (void *)Sys_GetProcAddress( &opengl_dll, name );

	return p;
}

void GL_CheckExtension( const char *name, const dllfunc_t *funcs, const char *cvarname, int r_ext )
{
	const dllfunc_t	*func;
	cvar_t		*parm;

	MsgDev( D_NOTE, "GL_CheckExtension: %s ", name );

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

	if((name[2] == '_' || name[3] == '_') && !com.strstr( gl_config.extensions_string, name ))
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
	int          i, v;

	Mem_Copy( gl_state.gammaRamp, gl_state.stateRamp, sizeof(gl_state.gammaRamp ));
	for( i = 0; i < 256; i++ )
	{
		v = 255 * pow((float)(i + 0.5) / 255, vid_gamma->value ) + 0.5;
		v = bound( 0, v, 255 );
		gl_state.gammaRamp[i+0] =   ((word)v)<<8;
		gl_state.gammaRamp[i+256] = ((word)v)<<8;
		gl_state.gammaRamp[i+512] = ((word)v)<<8;
	}
}

void GL_UpdateGammaRamp( void )
{
	GL_BuildGammaTable();
	SetDeviceGammaRamp( glw_state.hDC, gl_state.gammaRamp );
}

/*
===============
GL_UpdateSwapInterval
===============
*/
void GL_UpdateSwapInterval( void )
{
	if ( r_swapInterval->modified )
	{
		r_swapInterval->modified = false;

		if( pwglSwapIntervalEXT )
			pwglSwapIntervalEXT( r_swapInterval->integer );
	}
}

void GL_InitExtensions( void )
{
	int	flags = 0;

	// initialize gl extensions
	GL_CheckExtension( "OpenGL 1.1.0", opengl_110funcs, NULL, R_OPENGL_110 );
	if( !r_framebuffer ) r_framebuffer = Mem_Alloc( r_temppool, r_width->integer * r_height->integer * 4 );

	// get our various GL strings
	gl_config.vendor_string = pglGetString( GL_VENDOR );
	gl_config.renderer_string = pglGetString( GL_RENDERER );
	gl_config.version_string = pglGetString( GL_VERSION );
	gl_config.extensions_string = pglGetString( GL_EXTENSIONS );
	MsgDev( D_INFO, "Video: %s\n", gl_config.renderer_string );

	GL_CheckExtension( "WGL_EXT_swap_control", wglswapintervalfuncs, NULL, R_WGL_SWAPCONTROL );
	GL_CheckExtension( "glDrawRangeElements", drawrangeelementsfuncs, "gl_drawrangeelments", R_DRAW_RANGEELEMENTS_EXT );
	if(!GL_Support( R_DRAW_RANGEELEMENTS_EXT )) GL_CheckExtension("GL_EXT_draw_range_elements", drawrangeelementsextfuncs, "gl_drawrangeelments", R_DRAW_RANGEELEMENTS_EXT );

	// multitexture
	GL_CheckExtension("GL_ARB_multitexture", multitexturefuncs, "gl_arb_multitexture", R_ARB_MULTITEXTURE );
	if(GL_Support( R_ARB_MULTITEXTURE ))
	{
		pglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &gl_config.textureunits );
		GL_CheckExtension( "GL_ARB_texture_env_combine", NULL, "gl_texture_env_combine", R_COMBINE_EXT );
		if(!GL_Support( R_COMBINE_EXT )) GL_CheckExtension("GL_EXT_texture_env_combine", NULL, "gl_texture_env_combine", R_COMBINE_EXT );
		if(GL_Support( R_COMBINE_EXT )) GL_CheckExtension( "GL_ARB_texture_env_dot3", NULL, "gl_texture_env_dot3", R_DOT3_ARB_EXT );
	}
	else gl_config.textureunits = 1;

	// 3d texture support
	GL_CheckExtension( "GL_EXT_texture3D", texture3dextfuncs, "gl_texture_3d", R_TEXTURE_3D_EXT );
	if(GL_Support( R_TEXTURE_3D_EXT ))
	{
		pglGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &gl_config.max_3d_texture_size );
		if( gl_config.max_3d_texture_size < 32 )
		{
			GL_SetExtension( R_TEXTURE_3D_EXT, false );
			MsgDev( D_ERROR, "GL_EXT_texture3D reported bogus GL_MAX_3D_TEXTURE_SIZE, disabled\n" );
		}
	}

	GL_CheckExtension( "GL_SGIS_generate_mipmap", NULL, "gl_sgis_generate_mipmaps", R_SGIS_MIPMAPS_EXT );

	// hardware cubemaps
	GL_CheckExtension( "GL_ARB_texture_cube_map", NULL, "gl_texture_cubemap", R_TEXTURECUBEMAP_EXT );
	if(GL_Support( R_TEXTURECUBEMAP_EXT )) pglGetIntegerv( GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &gl_config.max_cubemap_texture_size );

	// point particles extension
	GL_CheckExtension( "GL_EXT_point_parameters", pointparametersfunc, NULL, R_EXT_POINTPARAMETERS );

	GL_CheckExtension( "GL_ARB_texture_non_power_of_two", NULL, "gl_texture_npot", R_ARB_TEXTURE_NPOT_EXT );
	GL_CheckExtension( "GL_ARB_texture_compression", texturecompressionfuncs, "gl_dds_hardware_support", R_TEXTURE_COMPRESSION_EXT );
	GL_CheckExtension( "GL_EXT_compiled_vertex_array", compiledvertexarrayfuncs, "gl_cva_support", R_CUSTOM_VERTEX_ARRAY_EXT );
	if(!GL_Support(R_CUSTOM_VERTEX_ARRAY_EXT)) GL_CheckExtension( "GL_SGI_compiled_vertex_array", compiledvertexarrayfuncs, "gl_cva_support", R_CUSTOM_VERTEX_ARRAY_EXT );		
	GL_CheckExtension( "GL_EXT_texture_edge_clamp", NULL, "gl_clamp_to_edge", R_CLAMPTOEDGE_EXT );
	if(!GL_Support( R_CLAMPTOEDGE_EXT )) GL_CheckExtension("GL_SGIS_texture_edge_clamp", NULL, "gl_clamp_to_edge", R_CLAMPTOEDGE_EXT );

	GL_CheckExtension( "GL_EXT_texture_filter_anisotropic", NULL, "gl_texture_anisotropy", R_ANISOTROPY_EXT );
	if(GL_Support( R_ANISOTROPY_EXT )) pglGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_config.max_anisotropy );

	GL_CheckExtension( "GL_EXT_texture_lod_bias", NULL, "gl_ext_texture_lodbias", R_TEXTURE_LODBIAS );
	if(GL_Support( R_TEXTURE_LODBIAS )) pglGetFloatv( GL_MAX_TEXTURE_LOD_BIAS_EXT, &gl_config.max_lodbias );

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

	if(GL_Support( R_FRAGMENT_PROGRAM_EXT ))
	{
		pglGetIntegerv( GL_MAX_TEXTURE_COORDS_ARB, &gl_config.texturecoords );
		pglGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &gl_config.teximageunits );
	}

	// vp and fp shaders
	GL_CheckExtension( "GL_ARB_shader_objects", shaderobjectsfuncs, "gl_shaderobjects", R_SHADER_OBJECTS_EXT );
	GL_CheckExtension( "GL_ARB_shading_language_100", NULL, "gl_glslprogram", R_SHADER_GLSL100_EXT );
	GL_CheckExtension( "GL_ARB_vertex_shader", vertexshaderfuncs, "gl_vertexshader", R_VERTEX_SHADER_EXT );
	GL_CheckExtension( "GL_ARB_fragment_shader", NULL, "gl_pixelshader", R_FRAGMENT_SHADER_EXT );

	// rectangle textures support
	if( com.strstr( gl_config.extensions_string, "GL_NV_texture_rectangle" ))
	{
		gl_config.texRectangle = GL_TEXTURE_RECTANGLE_NV;
		pglGetIntegerv( GL_MAX_RECTANGLE_TEXTURE_SIZE_NV, &gl_config.max_2d_rectangle_size );
	}
	else if( com.strstr( gl_config.extensions_string, "GL_EXT_texture_rectangle" ))
	{
		gl_config.texRectangle = GL_TEXTURE_RECTANGLE_EXT;
		pglGetIntegerv( GL_MAX_RECTANGLE_TEXTURE_SIZE_EXT, &gl_config.max_2d_rectangle_size );
	}
	else gl_config.texRectangle = gl_config.max_2d_rectangle_size = 0; // no rectangle

	if( GL_Support( R_TEXTURE_COMPRESSION_EXT )) flags |= IL_DDS_HARDWARE;
	flags |= IL_USE_LERPING;

	Image_Init( NULL, flags );
	glw_state.initialized = true;
}

/*
===============
GL_SelectTexture
===============
*/
void GL_SelectTexture( GLenum texture )
{
	if(!GL_Support( R_ARB_MULTITEXTURE ))
		return;

	if( gl_state.activeTMU == texture )
		return;

	gl_state.activeTMU = texture;
	texture = texture + GL_TEXTURE0_ARB;

	pglActiveTextureARB( texture );
	pglClientActiveTextureARB( texture );
}

/*
=================
GL_BindTexture
=================
*/
void GL_BindTexture( texture_t *texture )
{
	if( !texture ) texture = r_defaultTexture;

	// performance evaluation option
	if( r_nobind->integer && !(texture->flags & TF_STATIC))
		texture = r_defaultTexture;

	if( gl_state.texNum[gl_state.activeTMU] == texture->texnum )
		return;
	gl_state.texNum[gl_state.activeTMU] = texture->texnum;
	pglBindTexture( texture->target, texture->texnum );
}

/*
===============
GL_TexEnv
===============
*/
void GL_TexEnv( GLint texenv )
{
	if( gl_state.texEnv[gl_state.activeTMU] == texenv )
		return;
	gl_state.texEnv[gl_state.activeTMU] = texenv;
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, texenv );
}

/*
 =================
 GL_Enable
 =================
*/
void GL_Enable( GLenum cap )
{

	switch( cap )
	{
	case GL_CULL_FACE:
		if( gl_state.cullFace ) return;
		gl_state.cullFace = true;
		break;
	case GL_POLYGON_OFFSET_FILL:
		if( gl_state.polygonOffsetFill ) return;
		gl_state.polygonOffsetFill = true;
		break;
	case GL_VERTEX_PROGRAM_ARB:
		if(!GL_Support( R_VERTEX_PROGRAM_EXT ) || gl_state.vertexProgram )
			return;
		gl_state.vertexProgram = true;
		break;
	case GL_FRAGMENT_PROGRAM_ARB:
		if(!GL_Support( R_FRAGMENT_PROGRAM_EXT ) || gl_state.fragmentProgram )
			return;
		gl_state.fragmentProgram = true;
		break;
	case GL_ALPHA_TEST:
		if( gl_state.alpha_test ) return;
		gl_state.alpha_test = true;
		break;
	case GL_BLEND:
		if( gl_state.blend ) return;
		gl_state.blend = true;
		break;
	case GL_DEPTH_TEST:
		if( gl_state.depth_test ) return;
		gl_state.depth_test = true;
		break;
	}
	pglEnable( cap );
}

/*
 =================
 GL_Disable
 =================
*/
void GL_Disable( GLenum cap )
{
	switch( cap )
	{
	case GL_CULL_FACE:
		if(!gl_state.cullFace ) return;
		gl_state.cullFace = false;
		break;
	case GL_POLYGON_OFFSET_FILL:
		if( !gl_state.polygonOffsetFill ) return;
		gl_state.polygonOffsetFill = false;
		break;
	case GL_VERTEX_PROGRAM_ARB:
		if(!GL_Support( R_VERTEX_PROGRAM_EXT ) || !gl_state.vertexProgram )
			return;
		gl_state.vertexProgram = false;
		break;
	case GL_FRAGMENT_PROGRAM_ARB:
		if(!GL_Support( R_FRAGMENT_PROGRAM_EXT ) || !gl_state.fragmentProgram )
			return;
		gl_state.fragmentProgram = false;
		break;
	case GL_ALPHA_TEST:
		if(!gl_state.alpha_test ) return;
		gl_state.alpha_test = false;
		break;
	case GL_BLEND:
		if( !gl_state.blend ) return;
		gl_state.blend = false;
		break;
	case GL_DEPTH_TEST:
		if( !gl_state.depth_test ) return;
		gl_state.depth_test = false;
		break;
	}
	pglDisable( cap );
}

/*
=================
GL_CullFace
=================
*/
void GL_CullFace( GLenum mode )
{
	if( gl_state.cullMode == mode )
		return;

	gl_state.cullMode = mode;
	pglCullFace( mode );
}

/*
 =================
 GL_PolygonOffset
 =================
*/
void GL_PolygonOffset( GLfloat factor, GLfloat units )
{
	if( gl_state.offsetFactor == factor && gl_state.offsetUnits == units )
		return;

	gl_state.offsetFactor = factor;
	gl_state.offsetUnits = units;
	pglPolygonOffset( factor, units );
}

/*
 =================
 GL_AlphaFunc
 =================
*/
void GL_AlphaFunc( GLenum func, GLclampf ref )
{
	if( gl_state.alphaFunc == func && gl_state.alphaRef == ref )
		return;

	gl_state.alphaFunc = func;
	gl_state.alphaRef = ref;
	pglAlphaFunc( func, ref );
}

/*
 =================
 GL_BlendFunc
 =================
*/
void GL_BlendFunc( GLenum src, GLenum dst )
{
	if( gl_state.blendSrc == src && gl_state.blendDst == dst )
		return;

	gl_state.blendSrc = src;
	gl_state.blendDst = dst;
	pglBlendFunc( src, dst );
}

/*
 =================
 GL_DepthFunc
 =================
*/
void GL_DepthFunc( GLenum func )
{
	if( gl_state.depthFunc == func )
		return;

	gl_state.depthFunc = func;
	pglDepthFunc( func );
}

/*
=================
 GL_DepthMask
=================
*/
void GL_DepthMask( GLboolean mask )
{
	if( gl_state.depthMask == mask )
		return;

	gl_state.depthMask = mask;
	pglDepthMask( mask );
}

/*
=============
GL_SetColor

=============
*/
void GL_SetColor( const void *data )
{
	float *color = (float *)data;

	if( color ) 
	{
		gl_state.draw_color[0] = 255 * color[0];
		gl_state.draw_color[1] = 255 * color[1];
		gl_state.draw_color[2] = 255 * color[2];
		gl_state.draw_color[3] = 255 * color[3];
	}
	else
	{
		gl_state.draw_color[0] = 255;
		gl_state.draw_color[1] = 255;
		gl_state.draw_color[2] = 255;
		gl_state.draw_color[3] = 255;
	}
}

void GL_LoadMatrix( matrix4x4 source )
{
	gl_matrix	dest;

#if 0
	if( Matrix4x4_Compare( source, gl_state.matrix ))
		return; // ident

	Matrix4x4_Copy( gl_state.matrix, source );
#endif
	Matrix4x4_ToArrayFloatGL( source, dest );
	pglLoadMatrixf( dest );
}

void GL_SaveMatrix( GLenum target, matrix4x4 dest )
{
	gl_matrix	source;

	pglGetFloatv( target, source );
	Matrix4x4_FromArrayFloatGL( dest, source );
}

/*
=================
 GL_SetDefaultState
=================
*/
void GL_SetDefaultState( void )
{
	int		i;

	// Reset the state manager
	gl_state.orthogonal = false;
	gl_state.activeTMU = 0;

	for( i = 0; i < MAX_TEXTURE_UNITS; i++ )
	{
		gl_state.texNum[i] = 0;
		gl_state.texEnv[i] = GL_MODULATE;
	}

	gl_state.cullFace = true;
	gl_state.polygonOffsetFill = false;
	gl_state.vertexProgram = false;
	gl_state.fragmentProgram = false;
	gl_state.alpha_test = false;
	gl_state.blend = false;
	gl_state.depth_test = true;

	gl_state.cullMode = GL_FRONT;
	gl_state.offsetFactor = -1;
	gl_state.offsetUnits = -2;
	gl_state.alphaFunc = GL_GREATER;
	gl_state.alphaRef = 0.0;
	gl_state.blendSrc = GL_SRC_ALPHA;
	gl_state.blendDst = GL_ONE_MINUS_SRC_ALPHA;
	gl_state.depthFunc = GL_LEQUAL;
	gl_state.depthMask = GL_TRUE;

	// Set default state
	pglEnable( GL_CULL_FACE );
	pglDisable( GL_POLYGON_OFFSET_FILL );
	pglDisable( GL_ALPHA_TEST );
	pglDisable( GL_BLEND );
	pglEnable( GL_DEPTH_TEST );

	if(GL_Support( R_VERTEX_PROGRAM_EXT )) pglDisable( GL_VERTEX_PROGRAM_ARB );
	if(GL_Support( R_FRAGMENT_PROGRAM_EXT )) pglDisable( GL_FRAGMENT_PROGRAM_ARB );

	pglCullFace( GL_FRONT );
	pglPolygonOffset( -1, -2 );
	pglAlphaFunc( GL_GREATER, 0.0 );
	pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	pglDepthFunc( GL_LEQUAL );
	pglDepthMask( GL_TRUE );

	pglClearColor( 0.5, 0.5, 0.5, 0.5 );
	pglClearDepth( 1.0 );
	pglClearStencil( 128 );

	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	pglShadeModel( GL_SMOOTH );

	pglEnable( GL_SCISSOR_TEST );
	pglDisable( GL_STENCIL_TEST );

	if(GL_Support( R_ARB_MULTITEXTURE ))
	{
		for( i = MAX_TEXTURE_UNITS - 1; i > 0; i-- )
		{
			if( i >= gl_config.textureunits )
				continue;

			pglActiveTextureARB( GL_TEXTURE0_ARB + i );
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

			if(GL_Support( R_TEXTURECUBEMAP_EXT ))
				pglDisable( GL_TEXTURE_CUBE_MAP_ARB );
			pglDisable( GL_TEXTURE_2D );
		}
		pglActiveTextureARB( GL_TEXTURE0_ARB );
	}

	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	if(GL_Support( R_TEXTURECUBEMAP_EXT ))
		pglDisable( GL_TEXTURE_CUBE_MAP_ARB );

	Matrix4x4_LoadIdentity( gl_state.matrix );
	pglDisable( GL_TEXTURE_2D );
	gl_state.draw_color[0] = 255;
	gl_state.draw_color[1] = 255;
	gl_state.draw_color[2] = 255;
	gl_state.draw_color[3] = 255;
		
	GL_UpdateSwapInterval();
}

/*
=================
GL_Setup3D
=================
*/
void GL_Setup3D( void )
{
	int	x, y, w, h;
	int	bits;

	if( gl_finish->integer )  pglFinish();
	x = r_refdef.viewport[0];
	y = r_height->integer - r_refdef.viewport[3] - r_refdef.viewport[1];
	w = r_refdef.viewport[2];
	h = r_height->integer;

	// Set up viewport
	pglViewport( x, y, w, h );
	pglScissor( x, y, w, h );

	// Set up projection
	pglMatrixMode( GL_PROJECTION );
	pglLoadMatrixf( gl_projectionMatrix );
	pglMatrixMode( GL_MODELVIEW );

	// Set state
	gl_state.orthogonal = false;

	GL_TexEnv( GL_MODULATE );
	GL_Enable( GL_CULL_FACE );
	GL_Disable( GL_POLYGON_OFFSET_FILL );
	GL_Disable( GL_VERTEX_PROGRAM_ARB );
	GL_Disable( GL_FRAGMENT_PROGRAM_ARB );
	GL_Disable( GL_ALPHA_TEST );
	GL_Disable( GL_BLEND );
	GL_Enable( GL_DEPTH_TEST );

	GL_CullFace( GL_FRONT );
	GL_DepthFunc( GL_LEQUAL );
	GL_DepthMask( GL_TRUE );

	// Clear depth buffer, and optionally stencil buffer
	bits = GL_DEPTH_BUFFER_BIT;

	if( r_shadows->integer )
	{
		pglClearStencil( 128 );
		bits |= GL_STENCIL_BUFFER_BIT;
	}
	pglClear( bits );
}

/*
 =================
 GL_Setup2D
 =================
*/
void GL_Setup2D( void )
{
	if( gl_finish->integer ) pglFinish();

	// set 2D virtual screen size
	pglViewport( 0, 0, r_width->integer, r_height->integer );
	pglScissor( 0, 0, r_width->integer, r_height->integer );

	// set up projection
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();
	pglOrtho( 0, r_width->integer, r_height->integer, 0, -1, 1 );

	// Set up modelview
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	// Set state
	gl_state.orthogonal = true;

	GL_TexEnv( GL_MODULATE );

	GL_Disable( GL_CULL_FACE );
	GL_Disable( GL_POLYGON_OFFSET_FILL );
	GL_Disable( GL_VERTEX_PROGRAM_ARB );
	GL_Disable( GL_FRAGMENT_PROGRAM_ARB );
	GL_Disable( GL_ALPHA_TEST );
	GL_Enable( GL_BLEND );
	GL_Disable( GL_DEPTH_TEST );

	GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	GL_DepthMask( GL_FALSE );
}