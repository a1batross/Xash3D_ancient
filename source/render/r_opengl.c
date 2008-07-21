//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        r_opengl.c - openg32.dll handler
//=======================================================================

#include "gl_local.h"

glwstate_t	glw_state;

#define num_vidmodes	((int)(sizeof(vidmode) / sizeof(vidmode[0])) - 1)

typedef struct vidmode_s
{
	const char	*desc;
	int		width; 
	int		height;
	float		pixelheight;
} vidmode_t;

vidmode_t vidmode[] =
{
{"Mode  0: 4x3",	640,	480,	1	},
{"Mode  1: 4x3",	800,	600,	1	},
{"Mode  2: 4x3",	1024,	768,	1	},
{"Mode  3: 4x3",	1152,	864,	1	},
{"Mode  4: 4x3",	1280,	960,	1	},
{"Mode  5: 4x3",	1400,	1050,	1	},
{"Mode  6: 4x3",	1600,	1200,	1	},
{"Mode  7: 4x3",	1920,	1440,	1	},
{"Mode  8: 4x3",	2048,	1536,	1	},
{"Mode  9: 14x9",	840,	540,	1	},
{"Mode 10: 14x9",	1680,	1080,	1	},
{"Mode 11: 16x9",	640,	360,	1	},
{"Mode 12: 16x9",	683,	384,	1	},
{"Mode 13: 16x9",	960,	540,	1	},
{"Mode 14: 16x9",	1280,	720,	1	},
{"Mode 15: 16x9",	1366,	768,	1	},
{"Mode 16: 16x9",	1920,	1080,	1	},
{"Mode 17: 16x9",	2560,	1440,	1	},
{"Mode 18: NTSC",	360,	240,	1.125	},
{"Mode 19: NTSC",	720,	480,	1.125	},
{"Mode 20: PAL ",	360,	283,	0.9545	},
{"Mode 21: PAL ",	720,	566,	0.9545	},
{NULL,		0,	0,	0	},
};

static dllfunc_t wgl_funcs[] =
{
	{"wglChoosePixelFormat", (void **) &pwglChoosePixelFormat},
	{"wglDescribePixelFormat", (void **) &pwglDescribePixelFormat},
	{"wglGetPixelFormat", (void **) &pwglGetPixelFormat},
	{"wglSetPixelFormat", (void **) &pwglSetPixelFormat},
	{"wglSwapBuffers", (void **) &pwglSwapBuffers},
	{"wglCreateContext", (void **) &pwglCreateContext},
	{"wglDeleteContext", (void **) &pwglDeleteContext},
	{"wglGetProcAddress", (void **) &pwglGetProcAddress},
	{"wglMakeCurrent", (void **) &pwglMakeCurrent},
	{"wglGetCurrentContext", (void **) &pwglGetCurrentContext},
	{"wglGetCurrentDC", (void **) &pwglGetCurrentDC},
	{NULL, NULL}
};

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
	{"glDrawElements", (void **) &pglDrawElements},
	{"glColorMask", (void **) &pglColorMask},
	{"glVertexPointer", (void **) &pglVertexPointer},
	{"glNormalPointer", (void **) &pglNormalPointer},
	{"glColorPointer", (void **) &pglColorPointer},
	{"glTexCoordPointer", (void **) &pglTexCoordPointer},
	{"glArrayElement", (void **) &pglArrayElement},
	{"glColor3f", (void **) &pglColor3f},
	{"glColor3fv", (void **) &pglColor3fv},
	{"glColor4f", (void **) &pglColor4f},
	{"glColor4fv", (void **) &pglColor4fv},
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
	{"glBegin", (void **) &pglBegin},
	{"glEnd", (void **) &pglEnd},
	{"glLineWidth", (void**) &pglLineWidth},
	{"glPointSize", (void**) &pglPointSize},
	{"glMatrixMode", (void **) &pglMatrixMode},
	{"glOrtho", (void **) &pglOrtho},
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
	{"glMultiTexCoord2fARB", (void **) &pglMTexCoord2fSGIS},//FIXME
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
	{"glCompressedTexImage2DARB",    (void **) &pglCompressedTexImage2D},//FIXME
	{NULL, NULL}
};

dll_info_t opengl_dll = { "opengl32.dll", wgl_funcs, NULL, NULL, NULL, true, 0 };

bool R_DeleteContext( void )
{
	if( glw_state.hGLRC )
	{
		pwglDeleteContext( glw_state.hGLRC );
		glw_state.hGLRC = NULL;
	}
	if( glw_state.hDC )
	{
		ReleaseDC( glw_state.hWnd, glw_state.hDC );
		glw_state.hDC = NULL;
	}
	return false;
}

bool R_Init_OpenGL( void )
{
	long	flags = PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_GENERIC_ACCELERATED|PFD_DOUBLEBUFFER;
	int	pixelformat;	

	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),// size of this pfd
		1,			// version number
		flags,			// support window|OpenGL|generic accel|double buffer
		PFD_TYPE_RGBA,		// RGBA type
		24,			// 24-bit color depth
		8, 0, 8, 0, 8, 0,		// color bits set to 32
		8, 0,			// alpha bit 8
		0,			// no accumulation buffer
		0, 0, 0, 0, 		// accum bits ignored
		32,			// 32-bit z-buffer
		8,			// 8-bit stencil buffer
		0,			// no auxiliary buffer
		PFD_MAIN_PLANE,		// main layer
		0,			// reserved
		0, 0, 0			// layer masks ignored
	};

	Sys_LoadLibrary( &opengl_dll );	// load opengl32.dll
	if( !opengl_dll.link ) return false;

    	if(( glw_state.hDC = GetDC( glw_state.hWnd )) == NULL )
		return false;

	glw_state.minidriver = false;
	if( glw_state.minidriver )
	{
		if(!(pixelformat = pwglChoosePixelFormat( glw_state.hDC, &pfd)))
			return false;
		if(!(pwglSetPixelFormat( glw_state.hDC, pixelformat, &pfd)))
			return false;
		pwglDescribePixelFormat( glw_state.hDC, pixelformat, sizeof( pfd ), &pfd );
	}
	else
	{
		if(!( pixelformat = ChoosePixelFormat( glw_state.hDC, &pfd )))
			return false;
		if(!(SetPixelFormat( glw_state.hDC, pixelformat, &pfd )))
			return false;
		DescribePixelFormat( glw_state.hDC, pixelformat, sizeof( pfd ), &pfd );
	}

	if(!(glw_state.hGLRC = pwglCreateContext( glw_state.hDC )))
		return R_DeleteContext();
	if(!(pwglMakeCurrent( glw_state.hDC, glw_state.hGLRC )))
		return R_DeleteContext();

	// print out PFD specifics 
	MsgDev(D_NOTE, "GL PFD: color(%d-bits) Z(%d-bit)\n", ( int )pfd.cColorBits, ( int )pfd.cDepthBits );

	// init gamma ramp
	ZeroMemory( gl_config.original_ramp, sizeof(gl_config.original_ramp));
	GetDeviceGammaRamp( glw_state.hDC, gl_config.original_ramp );
	vid_gamma->modified = true;

	return true;
}

void R_Free_OpenGL( void )
{
	SetDeviceGammaRamp( glw_state.hDC, gl_config.original_ramp );

	if( pwglMakeCurrent ) pwglMakeCurrent( NULL, NULL );
	if( glw_state.hGLRC )
	{
		if( pwglDeleteContext ) pwglDeleteContext( glw_state.hGLRC );
		glw_state.hGLRC = NULL;
	}
	if( glw_state.hDC )
	{
		ReleaseDC( glw_state.hWnd, glw_state.hDC );
		glw_state.hDC   = NULL;
	}
	if( glw_state.hWnd )
	{
		DestroyWindow ( glw_state.hWnd );
		glw_state.hWnd = NULL;
	}
	UnregisterClass( "Xash Window", glw_state.hInst );

	if( gl_state.fullscreen )
	{
		ChangeDisplaySettings( 0, 0 );
		gl_state.fullscreen = false;
	}
	Sys_FreeLibrary( &opengl_dll );

	// now all extensions are disabled
	memset( gl_config.extension, 0, sizeof(gl_config.extension[0]) * R_EXTCOUNT);
}

void R_SaveVideoMode( int vid_mode )
{
	int	i = bound(0, vid_mode, num_vidmodes); // check range

	Cvar_SetValue("width", vidmode[i].width );
	Cvar_SetValue("height", vidmode[i].height );
	Cvar_SetValue("r_mode", i ); // merge if out of bounds
	MsgDev(D_NOTE, "Set: %s [%dx%d]\n", vidmode[i].desc, vidmode[i].width, vidmode[i].height );
}

bool R_CreateWindow( int width, int height, bool fullscreen )
{
	WNDCLASS		wc;
	RECT		rect;
	cvar_t		*r_xpos, *r_ypos;
	int		stylebits = WINDOW_STYLE;
	int		x = 0, y = 0, w, h;
	int		exstyle = 0;
	static char	wndname[128];
	
	com.strcpy( wndname, GI->title );

	// register the frame class
	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)glw_state.wndproc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = glw_state.hInst;
	wc.hIcon         = LoadIcon( glw_state.hInst, MAKEINTRESOURCE( 101 ));
	wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (void *)COLOR_3DSHADOW;
	wc.lpszClassName = "Xash Window";
	wc.lpszMenuName  = 0;

	if(!RegisterClass( &wc ))
	{ 
		MsgDev( D_ERROR, "R_CreateWindow: couldn't register window class %s\n" "Xash Window" );
		return false;
	}

	if( fullscreen )
	{
		exstyle = WS_EX_TOPMOST;
		stylebits = WS_POPUP | WS_VISIBLE;
	}

	rect.left = 0;
	rect.top = 0;
	rect.right  = width;
	rect.bottom = height;

	AdjustWindowRect( &rect, stylebits, FALSE );
	w = rect.right - rect.left;
	h = rect.bottom - rect.top;

	if( !fullscreen )
	{
		r_xpos = Cvar_Get( "r_xpos", "3", CVAR_ARCHIVE, "window position by horizontal" );
		r_ypos = Cvar_Get( "r_ypos", "22", CVAR_ARCHIVE, "window position by vertical" );
		x = r_xpos->integer;
		y = r_ypos->integer;
	}

	glw_state.hWnd = CreateWindowEx( exstyle, "Xash Window", wndname, stylebits, x, y, w, h, NULL, NULL, glw_state.hInst, NULL );

	if( !glw_state.hWnd ) 
	{
		MsgDev( D_ERROR, "R_CreateWindow: couldn't create window %s\n", wndname );
		return false;
	}
	
	ShowWindow( glw_state.hWnd, SW_SHOW );
	UpdateWindow( glw_state.hWnd );

	// init all the gl stuff for the window
	if(!R_Init_OpenGL())
	{
		MsgDev( D_ERROR, "OpenGL driver not installed\n" );
		return false;
	}

	SetForegroundWindow( glw_state.hWnd );
	SetFocus( glw_state.hWnd );
	return true;
}

rserr_t R_ChangeDisplaySettings( int vid_mode, bool fullscreen )
{
	int width, height;

	R_SaveVideoMode( vid_mode );

	width = r_width->integer;
	height = r_height->integer;

	// destroy the existing window
	if( glw_state.hWnd ) R_Free_OpenGL();

	// do a CDS if needed
	if( fullscreen )
	{
		DEVMODE	dm;

		ZeroMemory( &dm, sizeof( dm ));
		dm.dmSize = sizeof( dm );
		dm.dmPelsWidth = width;
		dm.dmPelsHeight = height;
		dm.dmFields = DM_PELSWIDTH|DM_PELSHEIGHT;
		
		if(ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) == DISP_CHANGE_SUCCESSFUL)
		{
			gl_state.fullscreen = true;
			if(!R_CreateWindow( width, height, true ))
				return rserr_invalid_mode;
			return rserr_ok;
		}
		else
		{
			dm.dmPelsWidth = width * 2;
			dm.dmPelsHeight = height;
			dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
			if( gl_bitdepth->value != 0 )
			{
				dm.dmBitsPerPel = gl_bitdepth->value;
				dm.dmFields |= DM_BITSPERPEL;
			}

			// our first CDS failed, so maybe we're running on some weird dual monitor system 
			if( ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
			{
				ChangeDisplaySettings( 0, 0 );
				gl_state.fullscreen = false;
				if(!R_CreateWindow( width, height, false ))
					return rserr_invalid_mode;
				return rserr_invalid_fullscreen;
			}
			else
			{
				if(!R_CreateWindow( width, height, true ))
					return rserr_invalid_mode;
				gl_state.fullscreen = true;
				return rserr_ok;
			}
		}
	}
	else
	{
		ChangeDisplaySettings( 0, 0 );
		gl_state.fullscreen = false;
		if(!R_CreateWindow( width, height, false ))
			return rserr_invalid_mode;
	}
	return rserr_ok;
}

/*
==================
R_SetMode
==================
*/
bool R_SetMode( void )
{
	rserr_t	err;
	bool	fullscreen;

	fullscreen = r_fullscreen->integer;
	r_fullscreen->modified = false;
	r_mode->modified = false;

	if(( err = R_ChangeDisplaySettings( r_mode->integer, fullscreen )) == rserr_ok )
	{
		gl_state.prev_mode = r_mode->integer;
	}
	else
	{
		if( err == rserr_invalid_fullscreen )
		{
			Cvar_SetValue( "fullscreen", 0 );
			r_fullscreen->modified = false;
			MsgDev( D_ERROR, "R_SetMode: fullscreen unavailable in this mode\n" );
			if(( err = R_ChangeDisplaySettings( r_mode->integer, false )) == rserr_ok )
				return true;
		}
		else if( err == rserr_invalid_mode )
		{
			Cvar_SetValue( "r_mode", gl_state.prev_mode );
			r_mode->modified = false;
			MsgDev( D_ERROR, "R_SetMode: invalid mode\n" );
		}
		// try setting it back to something safe
		if(( err = R_ChangeDisplaySettings( gl_state.prev_mode, false )) != rserr_ok )
		{
			MsgDev( D_ERROR, "R_SetMode: could not revert to safe mode\n" );
			return false;
		}
	}
	return true;
}

/*
=================
R_CheckForErrors
=================
*/
void R_CheckForErrors( void )
{
	int	err;
	char	*str;

	if( !r_check_errors->integer )
		return;
	if((err = pglGetError()) == GL_NO_ERROR)
		return;

	switch( err )
	{
	case GL_STACK_OVERFLOW:
		str = "GL_STACK_OVERFLOW";
		break;
	case GL_STACK_UNDERFLOW:
		str = "GL_STACK_UNDERFLOW";
		break;
	case GL_INVALID_ENUM:
		str = "GL_INVALID_ENUM";
		break;
	case GL_INVALID_VALUE:
		str = "GL_INVALID_VALUE";
		break;
	case GL_INVALID_OPERATION:
		str = "GL_INVALID_OPERATION";
		break;
	case GL_OUT_OF_MEMORY:
		str = "GL_OUT_OF_MEMORY";
		break;
	default:
		str = "UNKNOWN ERROR";
		break;
	}
	Host_Error( "R_CheckForErrors: %s", str );
}

void R_EndFrame( void )
{
	if( stricmp( gl_drawbuffer->string, "GL_BACK" ) == 0 )
	{
		if( !pwglSwapBuffers( glw_state.hDC ) )
			Host_Error("GLimp_EndFrame() - SwapBuffers() failed!\n" );
	}
}

void R_InitExtensions( void )
{
	// initialize gl extensions
	GL_CheckExtension( "OpenGL 1.1.0", opengl_110funcs, NULL, R_OPENGL_110 );

	// get our various GL strings
	gl_config.vendor_string = pglGetString( GL_VENDOR );
	gl_config.renderer_string = pglGetString( GL_RENDERER );
	gl_config.version_string = pglGetString( GL_VERSION );
	gl_config.extensions_string = pglGetString( GL_EXTENSIONS );
	MsgDev(D_INFO, "Video: %s\n", gl_config.renderer_string );

	GL_CheckExtension( "WGL_EXT_swap_control", wglswapintervalfuncs, NULL, R_WGL_SWAPCONTROL );
	GL_CheckExtension( "glDrawRangeElements", drawrangeelementsfuncs, "gl_drawrangeelments", R_DRAWRANGEELMENTS );
	if(!GL_Support( R_DRAWRANGEELMENTS ))
		GL_CheckExtension("GL_EXT_draw_range_elements", drawrangeelementsextfuncs, "gl_drawrangeelments", R_DRAWRANGEELMENTS );

	GL_CheckExtension("GL_ARB_multitexture", multitexturefuncs, "gl_arb_multitexture", R_ARB_MULTITEXTURE );

	if(GL_Support( R_ARB_MULTITEXTURE ))
	{
		pglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &gl_state.textureunits );
		GL_CheckExtension( "GL_ARB_texture_env_combine", NULL, "gl_texture_env_combine", R_COMBINE_EXT );
		if(!GL_Support( R_COMBINE_EXT ))
			GL_CheckExtension("GL_EXT_texture_env_combine", NULL, "gl_texture_env_combine", R_COMBINE_EXT );
		if(GL_Support( R_COMBINE_EXT ))
			GL_CheckExtension( "GL_ARB_texture_env_dot3", NULL, "gl_texture_env_dot3", R_DOT3_ARB_EXT );
	}

	GL_CheckExtension( "GL_EXT_texture3D", texture3dextfuncs, "gl_texture_3d", R_TEXTURE_3D_EXT );

	if(GL_Support( R_TEXTURE_3D_EXT ))
	{
		pglGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &gl_state.max_3d_texture_size );
		if( gl_state.max_3d_texture_size < 32 )
		{
			GL_SetExtension( R_TEXTURE_3D_EXT, false );
			MsgDev( D_ERROR, "GL_EXT_texture3D reported bogus GL_MAX_3D_TEXTURE_SIZE, disabled\n" );
		}
	}

	GL_CheckExtension( "GL_ARB_texture_cube_map", NULL, "gl_texture_cubemap", R_TEXTURECUBEMAP_EXT );
	if(GL_Support( R_TEXTURECUBEMAP_EXT )) pglGetIntegerv( GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &gl_state.max_cubemap_texture_size );

	GL_CheckExtension( "GL_ARB_texture_non_power_of_two", NULL, "gl_texture_npot", R_ARB_TEXTURE_NPOT_EXT );
	GL_CheckExtension( "GL_ARB_texture_compression", texturecompressionfuncs, "gl_dds_hardware_support", R_TEXTURE_COMPRESSION_EXT );
	GL_CheckExtension( "GL_EXT_compiled_vertex_array", compiledvertexarrayfuncs, "gl_cva_support", R_CUSTOM_VERTEX_ARRAY_EXT );
	if(!GL_Support(R_CUSTOM_VERTEX_ARRAY_EXT))
		GL_CheckExtension( "GL_SGI_compiled_vertex_array", compiledvertexarrayfuncs, "gl_cva_support", R_CUSTOM_VERTEX_ARRAY_EXT );		
	GL_CheckExtension( "GL_EXT_texture_edge_clamp", NULL, "gl_clamp_to_edge", R_CLAMPTOEDGE_EXT );
	if(!GL_Support( R_CLAMPTOEDGE_EXT )) 
		GL_CheckExtension("GL_SGIS_texture_edge_clamp", NULL, "gl_clamp_to_edge", R_CLAMPTOEDGE_EXT );

	GL_CheckExtension( "GL_EXT_texture_filter_anisotropic", NULL, "gl_texture_anisotropy", R_ANISOTROPY_EXT );
	if(GL_Support( R_ANISOTROPY_EXT )) pglGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_state.max_anisotropy );

	GL_CheckExtension( "GL_EXT_blend_minmax", blendequationfuncs, "gl_ext_customblend", R_BLEND_MINMAX_EXT );
	GL_CheckExtension( "GL_EXT_blend_subtract", blendequationfuncs, "gl_ext_customblend", R_BLEND_SUBTRACT_EXT );

	GL_CheckExtension( "glStencilOpSeparate", gl2separatestencilfuncs, "gl_separate_stencil",R_SEPARATESTENCIL_EXT );
	if(!GL_Support( R_SEPARATESTENCIL_EXT ))
		GL_CheckExtension("GL_ATI_separate_stencil", atiseparatestencilfuncs, "gl_separate_stencil", R_SEPARATESTENCIL_EXT );

	GL_CheckExtension( "GL_EXT_stencil_two_side", stenciltwosidefuncs, "gl_stenciltwoside", R_STENCILTWOSIDE_EXT );
	GL_CheckExtension( "GL_ARB_vertex_buffer_object", vbofuncs, "gl_vertex_buffer_object", R_ARB_VERTEX_BUFFER_OBJECT_EXT );

	// we don't care if it's an extension or not, they are identical functions, so keep it simple in the rendering code
	if( pglDrawRangeElements == NULL ) pglDrawRangeElements = pglDrawRangeElementsEXT;

	GL_CheckExtension( "GL_ARB_shader_objects", shaderobjectsfuncs, "gl_shaderobjects", R_SHADER_OBJECTS_EXT );
	GL_CheckExtension( "GL_ARB_shading_language_100", NULL, "gl_glslprogram", R_SHADER_GLSL100_EXT );
	GL_CheckExtension( "GL_ARB_vertex_shader", vertexshaderfuncs, "gl_vertexshader", R_VERTEX_SHADER_EXT );
	GL_CheckExtension( "GL_ARB_fragment_shader", NULL, "gl_pixelshader", R_FRAGMENT_SHADER_EXT );
}