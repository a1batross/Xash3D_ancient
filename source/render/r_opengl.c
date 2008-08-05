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
{"Mode 18: NTSC",	360,	240,	1.125f	},
{"Mode 19: NTSC",	720,	480,	1.125f	},
{"Mode 20: PAL ",	360,	283,	0.9545f	},
{"Mode 21: PAL ",	720,	566,	0.9545f	},
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

bool R_SetPixelformat( void )
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
	ZeroMemory( gl_state.stateRamp, sizeof(gl_state.stateRamp));
	GetDeviceGammaRamp( glw_state.hDC, gl_state.stateRamp );
	vid_gamma->modified = true;

	return true;
}

void R_Free_OpenGL( void )
{
	SetDeviceGammaRamp( glw_state.hDC, gl_state.stateRamp );

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

	if( gl_config.fullscreen )
	{
		ChangeDisplaySettings( 0, 0 );
		gl_config.fullscreen = false;
	}
	Sys_FreeLibrary( &opengl_dll );

	// now all extensions are disabled
	memset( gl_config.extension, 0, sizeof(gl_config.extension[0]) * R_EXTCOUNT);
}

void R_SaveVideoMode( int vid_mode )
{
	int	i = bound(0, vid_mode, num_vidmodes); // check range

	Cvar_FullSet("width", va("%i", vidmode[i].width ), CVAR_READ_ONLY );
	Cvar_FullSet("height", va("%i", vidmode[i].height ), CVAR_READ_ONLY );
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
	if(!R_SetPixelformat())
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
			gl_config.fullscreen = true;
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
				gl_config.fullscreen = false;
				if(!R_CreateWindow( width, height, false ))
					return rserr_invalid_mode;
				return rserr_invalid_fullscreen;
			}
			else
			{
				if(!R_CreateWindow( width, height, true ))
					return rserr_invalid_mode;
				gl_config.fullscreen = true;
				return rserr_ok;
			}
		}
	}
	else
	{
		ChangeDisplaySettings( 0, 0 );
		gl_config.fullscreen = false;
		if(!R_CreateWindow( width, height, false ))
			return rserr_invalid_mode;
	}
	return rserr_ok;
}

/*
==================
R_Init_OpenGL
==================
*/
bool R_Init_OpenGL( void )
{
	rserr_t	err;
	bool	fullscreen;

	fullscreen = r_fullscreen->integer;
	r_fullscreen->modified = false;
	r_mode->modified = false;

	if(( err = R_ChangeDisplaySettings( r_mode->integer, fullscreen )) == rserr_ok )
	{
		gl_config.prev_mode = r_mode->integer;
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
			Cvar_SetValue( "r_mode", gl_config.prev_mode );
			r_mode->modified = false;
			MsgDev( D_ERROR, "R_SetMode: invalid mode\n" );
		}
		// try setting it back to something safe
		if(( err = R_ChangeDisplaySettings( gl_config.prev_mode, false )) != rserr_ok )
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