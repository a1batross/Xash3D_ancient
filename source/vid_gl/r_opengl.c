//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        r_opengl.c - openg32.dll handler
//=======================================================================

#include "r_local.h"

glwstate_t	glw_state;

#define MAX_PFDS		256
#define num_vidmodes	((int)(sizeof(vidmode) / sizeof(vidmode[0])) - 1)
#define WINDOW_STYLE	(WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_VISIBLE)
#define GL_DRIVER_OPENGL	"OpenGL32"

typedef enum
{
	rserr_ok,
	rserr_invalid_fullscreen,
	rserr_invalid_mode,
	rserr_unknown
} rserr_t;

typedef struct vidmode_s
{
	const char	*desc;
	int		width; 
	int		height;
	bool		wideScreen;
} vidmode_t;

vidmode_t vidmode[] =
{
{"Mode  0: 4x3",	640,	480,	false	},
{"Mode  1: 4x3",	800,	600,	false	},
{"Mode  2: 4x3",	960,	720,	false	},
{"Mode  3: 4x3",	1024,	768,	false	},
{"Mode  4: 4x3",	1152,	864,	false	},
{"Mode  5: 4x3",	1280,	800,	false	},
{"Mode  6: 4x3",	1280,	960,	false	},
{"Mode  7: 4x3",	1280,	1024,	false	},
{"Mode  8: 4x3",	1600,	1200,	false	},
{"Mode  9: 4x3",	2048,	1536,	false	},

{"Mode 10: 16x9",	856,	480,	true	},
{"Mode 11: 16x9",	1024,	576,	true	},
{"Mode 12: 16x9",	1440,	900,	true	},
{"Mode 13: 16x9",	1680,	1050,	true	},
{"Mode 14: 16x9",	1920,	1200,	true	},
{"Mode 15: 16x9",	2560,	1600,	true	},
{NULL,		0,	0,	0	},
};

static dllfunc_t wgl_funcs[] =
{
	{"wglChoosePixelFormat", (void **) &pwglChoosePixelFormat},
	{"wglDescribePixelFormat", (void **) &pwglDescribePixelFormat},
//	{"wglGetPixelFormat", (void **) &pwglGetPixelFormat},
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

dll_info_t opengl_dll = { "opengl32.dll", wgl_funcs, NULL, NULL, NULL, true };

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

/*
=================
R_ChoosePFD
=================
*/
static int R_ChoosePFD( int colorBits, int depthBits, int stencilBits )
{
	PIXELFORMATDESCRIPTOR	PFDs[MAX_PFDS], *current, *selected;
	int			i, numPFDs, pixelFormat = 0;
	uint			flags = PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;

	MsgDev( D_NOTE, "R_ChoosePFD( %i, %i, %i )\n", colorBits, depthBits, stencilBits);

	// Count PFDs
	if( glw_state.minidriver )
		numPFDs = pwglDescribePixelFormat( glw_state.hDC, 0, 0, NULL );
	else numPFDs = DescribePixelFormat( glw_state.hDC, 0, 0, NULL );

	if( numPFDs > MAX_PFDS )
	{
		MsgDev( D_NOTE, "too many PFDs returned (%i > %i), reduce it\n", numPFDs, MAX_PFDS );
		numPFDs = MAX_PFDS;
	}
	else if( numPFDs < 1 )
	{
		MsgDev( D_ERROR, "R_ChoosePFD failed\n" );
		return 0;
	}

	MsgDev( D_NOTE, "R_ChoosePFD: %i PFDs found\n", numPFDs );

	// run through all the PFDs, looking for the best match
	for( i = 1, current = PFDs; i <= numPFDs; i++, current++ )
	{
		if( glw_state.minidriver )
			pwglDescribePixelFormat( glw_state.hDC, i, sizeof( PIXELFORMATDESCRIPTOR ), current );
		else DescribePixelFormat( glw_state.hDC, i, sizeof( PIXELFORMATDESCRIPTOR ), current );

		// check acceleration
		if(( current->dwFlags & PFD_GENERIC_FORMAT ) && !r_allow_software->integer )
		{
			MsgDev( D_NOTE, "PFD %i rejected, software acceleration\n", i );
			continue;
		}

		// check flags
		if(( current->dwFlags & flags ) != flags )
		{
			MsgDev( D_NOTE, "PFD %i rejected, improper flags (0x%x instead of 0x%x)\n", i, current->dwFlags, flags );
			continue;
		}

		// Check pixel type
		if( current->iPixelType != PFD_TYPE_RGBA )
		{
			MsgDev( D_NOTE, "PFD %i rejected, not RGBA\n", i );
			continue;
		}

		// check color bits
		if( current->cColorBits < colorBits )
		{
			MsgDev( D_NOTE, "PFD %i rejected, insufficient color bits (%i < %i)\n", i, current->cColorBits, colorBits );
			continue;
		}

		// check depth bits
		if( current->cDepthBits < depthBits )
		{
			MsgDev( D_NOTE, "PFD %i rejected, insufficient depth bits (%i < %i)\n", i, current->cDepthBits, depthBits );
			continue;
		}

		// check stencil bits
		if( current->cStencilBits < stencilBits )
		{
			MsgDev( D_NOTE, "PFD %i rejected, insufficient stencil bits (%i < %i)\n", i, current->cStencilBits, stencilBits );
			continue;
		}

		// if we don't have a selected PFD yet, then use it
		if( !pixelFormat )
		{
			selected = current;
			pixelFormat = i;
			continue;
		}

		if( colorBits != selected->cColorBits )
		{
			if( colorBits == current->cColorBits || current->cColorBits > selected->cColorBits )
			{
				selected = current;
				pixelFormat = i;
				continue;
			}
		}

		if( depthBits != selected->cDepthBits )
		{
			if( depthBits == current->cDepthBits || current->cDepthBits > selected->cDepthBits )
			{
				selected = current;
				pixelFormat = i;
				continue;
			}
		}

		if( stencilBits != selected->cStencilBits )
		{
			if( stencilBits == current->cStencilBits || current->cStencilBits > selected->cStencilBits )
			{
				selected = current;
				pixelFormat = i;
				continue;
			}
		}
	}

	if( !pixelFormat )
	{
		MsgDev( D_ERROR, "R_ChoosePFD: no hardware acceleration found\n" );
		return 0;
	}

	if( selected->dwFlags & PFD_GENERIC_FORMAT )
	{
		if( selected->dwFlags & PFD_GENERIC_ACCELERATED )
		{
			MsgDev( D_NOTE, "R_ChoosePFD:: MCD acceleration found\n" );
			glw_state.software = false;
		}
		else
		{
			MsgDev( D_NOTE, "R_ChoosePFD: using software emulation\n" );
			glw_state.software = true;
		}
	}
	else
	{
		MsgDev( D_NOTE, "using hardware acceleration\n");
		glw_state.software = false;
	}
	MsgDev( D_LOAD, "R_ChoosePFD: PIXELFORMAT %i selected\n", pixelFormat );

	return pixelFormat;
}

bool R_SetPixelformat( void )
{
	PIXELFORMATDESCRIPTOR	PFD;
	int			colorBits, depthBits, stencilBits;
	int			pixelFormat;
	size_t			gamma_size;
	byte			*savedGamma;

	Sys_LoadLibrary( NULL, &opengl_dll );	// load opengl32.dll
	if( !opengl_dll.link ) return false;

	glw_state.minidriver = false;	// FIXME

	if( glw_state.minidriver )
	{
    		if(( glw_state.hDC = pwglGetCurrentDC()) == NULL )
			return false;
	}
	else
	{
    		if(( glw_state.hDC = GetDC( glw_state.hWnd )) == NULL )
			return false;
	}

	// set color/depth/stencil
	colorBits = (r_colorbits->integer) ? r_colorbits->integer : 32;
	depthBits = (r_depthbits->integer) ? r_depthbits->integer : 24;
	stencilBits = (r_stencilbits->integer) ? r_stencilbits->integer : 0;

	// choose a pixel format
	pixelFormat = R_ChoosePFD( colorBits, depthBits, stencilBits );
	if( !pixelFormat )
	{
		// try again with default color/depth/stencil
		if( colorBits > 16 || depthBits > 16 || stencilBits > 0 )
			pixelFormat = R_ChoosePFD( 16, 16, 0 );
		else pixelFormat = R_ChoosePFD( 32, 24, 0 );

		if( !pixelFormat )
		{
			MsgDev( D_ERROR, "R_SetPixelformat: failed to find an appropriate PIXELFORMAT\n" );
			ReleaseDC( glw_state.hWnd, glw_state.hDC );
			glw_state.hDC = NULL;
			return false;
		}
	}

	// set the pixel format
	if( glw_state.minidriver )
	{
		pwglDescribePixelFormat( glw_state.hDC, pixelFormat, sizeof( PIXELFORMATDESCRIPTOR ), &PFD );

		if( !pwglSetPixelFormat( glw_state.hDC, pixelFormat, &PFD ))
		{
			MsgDev( D_ERROR, "R_SetPixelformat: wglSetPixelFormat failed\n" );
			return R_DeleteContext();
		}
	}
	else
	{
		DescribePixelFormat( glw_state.hDC, pixelFormat, sizeof( PIXELFORMATDESCRIPTOR ), &PFD );

		if( !SetPixelFormat( glw_state.hDC, pixelFormat, &PFD ))
		{
			MsgDev( D_ERROR, "R_SetPixelformat: failed\n" );
			return R_DeleteContext();
		}
	}

	glConfig.color_bits = PFD.cColorBits;
	glConfig.depth_bits = PFD.cDepthBits;
	glConfig.stencil_bits = PFD.cStencilBits;

	if(!(glw_state.hGLRC = pwglCreateContext( glw_state.hDC )))
		return R_DeleteContext();
	if(!(pwglMakeCurrent( glw_state.hDC, glw_state.hGLRC )))
		return R_DeleteContext();

	if( PFD.cStencilBits != 0 )
		glState.stencilEnabled = true;
	else glState.stencilEnabled = false;

	// print out PFD specifics 
	MsgDev( D_NOTE, "GL PFD: color(%d-bits) Z(%d-bit)\n", ( int )PFD.cColorBits, ( int )PFD.cDepthBits );

	// init gamma ramp
	ZeroMemory( glState.stateRamp, sizeof(glState.stateRamp));

	if( pwglGetDeviceGammaRamp3DFX )
		glConfig.deviceSupportsGamma = pwglGetDeviceGammaRamp3DFX( glw_state.hDC, glState.stateRamp );
	else glConfig.deviceSupportsGamma = GetDeviceGammaRamp( glw_state.hDC, glState.stateRamp );

	// share this extension so engine can grab them
	GL_SetExtension( R_HARDWARE_GAMMA_CONTROL, glConfig.deviceSupportsGamma );

	savedGamma = FS_LoadFile( "config/gamma.rc", &gamma_size );
	if( !savedGamma || gamma_size != sizeof( glState.stateRamp ))
	{
		// saved gamma not found or corrupted file
		FS_WriteFile( "config/gamma.rc", glState.stateRamp, sizeof(glState.stateRamp));
		MsgDev( D_NOTE, "gamma.rc initialized\n" );
		if( savedGamma ) Mem_Free( savedGamma );
	}
	else
	{
		GL_BuildGammaTable();

		// validate base gamma
		if( !memcmp( savedGamma, glState.stateRamp, sizeof( glState.stateRamp )))
		{
			// all ok, previous gamma is valid
			MsgDev( D_NOTE, "R_SetPixelformat: validate screen gamma - ok\n" );
		}
		else if( !memcmp( glState.gammaRamp, glState.stateRamp, sizeof( glState.stateRamp )))
		{
			// screen gamma is equal to render gamma (probably previous instance crashed)
			// run additional check to make sure it
			if( memcmp( savedGamma, glState.stateRamp, sizeof( glState.stateRamp )))
			{
				// yes, current gamma it's totally wrong, restore it from gamma.rc
				MsgDev( D_NOTE, "R_SetPixelformat: restore original gamma after crash\n" );
				Mem_Copy( glState.stateRamp, savedGamma, sizeof( glState.gammaRamp ));
			}
			else
			{
				// oops, savedGamma == glState.stateRamp == glState.gammaRamp
				// probably r_gamma set as default
				MsgDev( D_NOTE, "R_SetPixelformat: validate screen gamma - disabled\n" ); 
			}
		}
		else if( !memcmp( glState.gammaRamp, savedGamma, sizeof( glState.stateRamp )))
		{
			// saved gamma is equal render gamma, probably gamma.rc writed after crash
			// run additional check to make sure it
			if( memcmp( savedGamma, glState.stateRamp, sizeof( glState.stateRamp )))
			{
				// yes, saved gamma it's totally wrong, get origianl gamma from screen
				MsgDev( D_NOTE, "R_SetPixelformat: merge gamma.rc after crash\n" );
				FS_WriteFile( "config/gamma.rc", glState.stateRamp, sizeof( glState.stateRamp ));
			}
			else
			{
				// oops, savedGamma == glState.stateRamp == glState.gammaRamp
				// probably r_gamma set as default
				MsgDev( D_NOTE, "R_SetPixelformat: validate screen gamma - disabled\n" ); 
			}
		}
		else
		{
			// current gamma unset by other application, we can restore it here
			MsgDev( D_NOTE, "R_SetPixelformat: restore original gamma after crash\n" );
			Mem_Copy( glState.stateRamp, savedGamma, sizeof( glState.gammaRamp ));			
		}
		Mem_Free( savedGamma );
	}
	r_gamma->modified = true;

	return true;
}

void R_Free_OpenGL( void )
{
	SetDeviceGammaRamp( glw_state.hDC, glState.stateRamp );

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

	if( glState.fullScreen )
	{
		ChangeDisplaySettings( 0, 0 );
		glState.fullScreen = false;
	}
	Sys_FreeLibrary( &opengl_dll );

	// now all extensions are disabled
	Mem_Set( glConfig.extension, 0, sizeof( glConfig.extension[0] ) * R_EXTCOUNT );
	glw_state.initialized = false;
}

void R_SaveVideoMode( int vid_mode )
{
	int	i = bound( 0, vid_mode, num_vidmodes ); // check range

	glState.width = vidmode[i].width;
	glState.height = vidmode[i].height;
	glState.wideScreen = vidmode[i].wideScreen;

	Cvar_FullSet( "width", va( "%i", vidmode[i].width ), CVAR_READ_ONLY );
	Cvar_FullSet( "height", va( "%i", vidmode[i].height ), CVAR_READ_ONLY );
	Cvar_SetValue( "r_mode", i ); // merge if out of bounds
	MsgDev( D_NOTE, "Set: %s [%dx%d]\n", vidmode[i].desc, vidmode[i].width, vidmode[i].height );
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
	
	com.strcpy( wndname, FS_Title());

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

	if( !RegisterClass( &wc ))
	{ 
		MsgDev( D_ERROR, "R_CreateWindow: couldn't register window class %s\n" "Xash Window" );
		return false;
	}

	if( fullscreen )
	{
		exstyle = WS_EX_TOPMOST;
		stylebits = WS_POPUP|WS_VISIBLE;
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

		// adjust window coordinates if necessary 
		// so that the window is completely on screen
		if( x < 0 ) x = 0;
		if( y < 0 ) y = 0;

		if( Cvar_VariableInteger( "r_mode" ) != glConfig.prev_mode )
		{
			if((x + w > glw_state.desktopWidth) || (y + h > glw_state.desktopHeight))
			{
				x = ( glw_state.desktopWidth - w ) / 2;
				y = ( glw_state.desktopHeight - h ) / 2;
			}
		}
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
	if( !R_SetPixelformat( ))
	{
		ShowWindow( glw_state.hWnd, SW_HIDE );
		DestroyWindow( glw_state.hWnd );
		glw_state.hWnd = NULL;

		UnregisterClass( "Xash Window", glw_state.hInst );
		MsgDev( D_ERROR, "OpenGL driver not installed\n" );
		return false;
	}

	SetForegroundWindow( glw_state.hWnd );
	SetFocus( glw_state.hWnd );
	return true;
}

rserr_t R_ChangeDisplaySettings( int vid_mode, bool fullscreen )
{
	int	width, height;
	HDC	hDC;
	
	R_SaveVideoMode( vid_mode );

	width = r_width->integer;
	height = r_height->integer;

	// check our desktop attributes
	hDC = GetDC( GetDesktopWindow() );
	glw_state.desktopBitsPixel = GetDeviceCaps( hDC, BITSPIXEL );
	glw_state.desktopWidth = GetDeviceCaps( hDC, HORZRES );
	glw_state.desktopHeight = GetDeviceCaps( hDC, VERTRES );
	ReleaseDC( GetDesktopWindow(), hDC );

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

		if( vid_displayfrequency->integer > 0 )
		{
			dm.dmFields |= DM_DISPLAYFREQUENCY;
			dm.dmDisplayFrequency = vid_displayfrequency->integer;
		}
		
		if( ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) == DISP_CHANGE_SUCCESSFUL )
		{
			glState.fullScreen = true;
			if( !R_CreateWindow( width, height, true ))
				return rserr_invalid_mode;
			return rserr_ok;
		}
		else
		{
			dm.dmPelsWidth = width * 2;
			dm.dmPelsHeight = height;
			dm.dmFields = DM_PELSWIDTH|DM_PELSHEIGHT;
			if( r_depthbits->integer != 0 )
			{
				dm.dmBitsPerPel = r_depthbits->integer;
				dm.dmFields |= DM_BITSPERPEL;
			}

			// our first CDS failed, so maybe we're running on some weird dual monitor system 
			if( ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
			{
				ChangeDisplaySettings( 0, 0 );
				glState.fullScreen = false;
				if( !R_CreateWindow( width, height, false ))
					return rserr_invalid_mode;
				return rserr_invalid_fullscreen;
			}
			else
			{
				if( !R_CreateWindow( width, height, true ))
					return rserr_invalid_mode;
				glState.fullScreen = true;
				return rserr_ok;
			}
		}
	}
	else
	{
		ChangeDisplaySettings( 0, 0 );
		glState.fullScreen = false;
		if( !R_CreateWindow( width, height, false ))
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

	fullscreen = vid_fullscreen->integer;
	vid_fullscreen->modified = false;
	r_mode->modified = false;

	if(( err = R_ChangeDisplaySettings( r_mode->integer, fullscreen )) == rserr_ok )
	{
		glConfig.prev_mode = r_mode->integer;
	}
	else
	{
		if( err == rserr_invalid_fullscreen )
		{
			Cvar_SetValue( "fullscreen", 0 );
			vid_fullscreen->modified = false;
			MsgDev( D_ERROR, "R_SetMode: fullscreen unavailable in this mode\n" );
			if(( err = R_ChangeDisplaySettings( r_mode->integer, false )) == rserr_ok )
				return true;
		}
		else if( err == rserr_invalid_mode )
		{
			Cvar_SetValue( "r_mode", glConfig.prev_mode );
			r_mode->modified = false;
			MsgDev( D_ERROR, "R_SetMode: invalid mode\n" );
		}

		// try setting it back to something safe
		if(( err = R_ChangeDisplaySettings( glConfig.prev_mode, false )) != rserr_ok )
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
void R_CheckForErrors_( const char *filename, const int fileline )
{
	int	err;
	char	*str;

	if( !r_check_errors->integer )
		return;
	if((err = pglGetError()) == GL_NO_ERROR )
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
	Host_Error( "R_CheckForErrors: %s (called at %s:%i)\n", str, filename, fileline );
}