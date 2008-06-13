/*
Copyright (C) 1997-2001 Id Software, Inc.

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
/*
** GLW_IMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
** GLimp_SwitchFullscreen
**
*/

#include <assert.h>
#include "gl_local.h"

static bool GLimp_SwitchFullscreen( int width, int height );
bool GLimp_InitGL (void);

glwstate_t glw_state;
static char wndname[128];

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

void R_GetVideoMode( int vid_mode )
{
	int	i = bound(0, vid_mode, num_vidmodes); // check range

	Cvar_SetValue("width", vidmode[i].width );
	Cvar_SetValue("height", vidmode[i].height );
	Cvar_SetValue("r_mode", i ); // merge if out of bounds
	MsgDev(D_NOTE, "Set: %s [%dx%d]\n", vidmode[i].desc, vidmode[i].width, vidmode[i].height );
}

static bool VerifyDriver( void )
{
	char buffer[1024];

	strcpy( buffer, qglGetString( GL_RENDERER ));
	strlwr( buffer );

	if (!strcmp( buffer, "gdi generic" ))
	{
		if ( !glw_state.mcd_accelerated )
			return false;
	}
	return true;
}

/*
** VID_CreateWindow
*/
#define	WINDOW_CLASS_NAME	"Xash Window"

bool VID_CreateWindow( int width, int height, bool fullscreen )
{
	WNDCLASS		wc;
	RECT		r;
	cvar_t		*r_xpos, *r_ypos;
	int		stylebits;
	int		x, y, w, h;
	int		exstyle;

	strcpy(wndname, FS_Title ); //critical stuff.

	// Register the frame class
	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)glw_state.wndproc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = glw_state.hInstance;
	wc.hIcon         = LoadIcon( glw_state.hInstance, MAKEINTRESOURCE(101));
	wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = (void *)COLOR_3DSHADOW;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = WINDOW_CLASS_NAME;

	if (!RegisterClass (&wc))
	{ 
		Msg("Couldn't register window class");
		return false;
	}

	if (fullscreen)
	{
		exstyle = WS_EX_TOPMOST;
		stylebits = WS_POPUP|WS_VISIBLE;
	}
	else
	{
		exstyle = 0;
		stylebits = WINDOW_STYLE;
	}

	r.left = 0;
	r.top = 0;
	r.right  = width;
	r.bottom = height;

	AdjustWindowRect (&r, stylebits, FALSE);

	w = r.right - r.left;
	h = r.bottom - r.top;

	if (fullscreen)
	{
		x = 0;
		y = 0;
	}
	else
	{
		r_xpos = Cvar_Get ("r_xpos", "3", 0);
		r_ypos = Cvar_Get ("r_ypos", "22", 0);
		x = r_xpos->value;
		y = r_ypos->value;
	}

	glw_state.hWnd = CreateWindowEx( exstyle, WINDOW_CLASS_NAME, wndname, stylebits, x, y, w, h, NULL, NULL, glw_state.hInstance, NULL);

	if (!glw_state.hWnd) 
	{
		Msg("Couldn't create window");
		return false;
	}
	
	ShowWindow( glw_state.hWnd, SW_SHOW );
	UpdateWindow( glw_state.hWnd );

	// init all the gl stuff for the window
	if (!GLimp_InitGL ())
	{
		Msg("VID_CreateWindow() - GLimp_InitGL failed\n");
		return false;
	}

	SetForegroundWindow( glw_state.hWnd );
	SetFocus( glw_state.hWnd );
	return true;
}


/*
** GLimp_SetMode
*/
rserr_t GLimp_SetMode( int vid_mode, bool fullscreen )
{
	int width, height;

	R_GetVideoMode( vid_mode );

	width = r_width->integer;
	height = r_height->integer;

	// destroy the existing window
	if (glw_state.hWnd) GLimp_Shutdown();

	// do a CDS if needed
	if( fullscreen )
	{
		DEVMODE dm;

		memset( &dm, 0, sizeof( dm ) );
		dm.dmSize = sizeof( dm );

		dm.dmPelsWidth  = width;
		dm.dmPelsHeight = height;
		dm.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;

		if ( gl_bitdepth->value != 0 )
		{
			dm.dmBitsPerPel = gl_bitdepth->value;
			dm.dmFields |= DM_BITSPERPEL;
		}
		else
		{
			HDC hdc = GetDC( NULL );
			int bitspixel = GetDeviceCaps( hdc, BITSPIXEL );
			ReleaseDC( 0, hdc );
		}

		if( ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) == DISP_CHANGE_SUCCESSFUL )
		{
			gl_state.fullscreen = true;

			if( !VID_CreateWindow (width, height, true) )
				return rserr_invalid_mode;

			return rserr_ok;
		}
		else
		{
			dm.dmPelsWidth = width * 2;
			dm.dmPelsHeight = height;
			dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

			if ( gl_bitdepth->value != 0 )
			{
				dm.dmBitsPerPel = gl_bitdepth->value;
				dm.dmFields |= DM_BITSPERPEL;
			}

			/*
			** our first CDS failed, so maybe we're running on some weird dual monitor
			** system 
			*/
			if ( ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
			{
				ChangeDisplaySettings( 0, 0 );

				gl_state.fullscreen = false;
				if ( !VID_CreateWindow (width, height, false) )
					return rserr_invalid_mode;
				return rserr_invalid_fullscreen;
			}
			else
			{
				if ( !VID_CreateWindow (width, height, true) )
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
		if ( !VID_CreateWindow (width, height, false) )
			return rserr_invalid_mode;
	}

	return rserr_ok;
}

/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem.  Under OpenGL this means NULLing out the current DC and
** HGLRC, deleting the rendering context, and releasing the DC acquired
** for the window.  The state structure is also nulled out.
**
*/
void GLimp_Shutdown( void )
{
	SetDeviceGammaRamp( glw_state.hDC, gl_config.original_ramp );

	if ( qwglMakeCurrent && !qwglMakeCurrent( NULL, NULL ) )
		Msg("R_Shutdown() - wglMakeCurrent failed\n");
	if ( glw_state.hGLRC )
	{
		if (  qwglDeleteContext && !qwglDeleteContext( glw_state.hGLRC ) )
			Msg("R_Shutdown() - wglDeleteContext failed\n");
		glw_state.hGLRC = NULL;
	}
	if (glw_state.hDC)
	{
		if ( !ReleaseDC( glw_state.hWnd, glw_state.hDC ) )
			Msg("R_Shutdown() - ReleaseDC failed\n" );
		glw_state.hDC   = NULL;
	}
	if (glw_state.hWnd)
	{
		DestroyWindow ( glw_state.hWnd );
		glw_state.hWnd = NULL;
	}

	if ( glw_state.log_fp )
	{
		fclose( glw_state.log_fp );
		glw_state.log_fp = 0;
	}

	UnregisterClass (WINDOW_CLASS_NAME, glw_state.hInstance);

	if ( gl_state.fullscreen )
	{
		ChangeDisplaySettings( 0, 0 );
		gl_state.fullscreen = false;
	}
}


/*
** GLimp_Init
**
** This routine is responsible for initializing the OS specific portions
** of OpenGL.  Under Win32 this means dealing with the pixelformats and
** doing the wgl interface stuff.
*/
bool GLimp_Init( void *hinstance )
{
#define OSR2_BUILD_NUMBER 1111

	OSVERSIONINFO	vinfo;

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	glw_state.allowdisplaydepthchange = false;

	if ( GetVersionEx( &vinfo) )
	{
		if ( vinfo.dwMajorVersion > 4 )
		{
			glw_state.allowdisplaydepthchange = true;
		}
		else if ( vinfo.dwMajorVersion == 4 )
		{
			if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT )
			{
				glw_state.allowdisplaydepthchange = true;
			}
			else if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
			{
				if ( LOWORD( vinfo.dwBuildNumber ) >= OSR2_BUILD_NUMBER )
				{
					glw_state.allowdisplaydepthchange = true;
				}
			}
		}
	}
	else
	{
		Msg("GLimp_Init() - GetVersionEx failed\n" );
		return false;
	}

	glw_state.hInstance = ( HINSTANCE ) hinstance;
	glw_state.wndproc = ri.WndProc;

	return true;
}

bool GLimp_InitGL (void)
{
    PIXELFORMATDESCRIPTOR pfd = 
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,				// version number
		PFD_DRAW_TO_WINDOW |		// support window
		PFD_SUPPORT_OPENGL |		// support OpenGL
		PFD_GENERIC_ACCELERATED |		// accelerated
		PFD_DOUBLEBUFFER,			// double buffered
		PFD_TYPE_RGBA,			// RGBA type
		24,				// 24-bit color depth
		0, 0, 0, 0, 0, 0,			// color bits ignored
		0,				// no alpha buffer
		0,				// shift bit ignored
		0,				// no accumulation buffer
		0, 0, 0, 0, 			// accum bits ignored
		32,				// 32-bit z-buffer	
		0,				// no stencil buffer
		0,				// no auxiliary buffer
		PFD_MAIN_PLANE,			// main layer
		0,				// reserved
		0, 0, 0				// layer masks ignored
    };
	int  pixelformat;
	
	glw_state.minidriver = false;
	/*
	** Get a DC for the specified window
	*/
	if ( glw_state.hDC != NULL )
		Msg("GLimp_Init() - non-NULL DC exists\n" );

    	if ( ( glw_state.hDC = GetDC( glw_state.hWnd ) ) == NULL )
	{
		Msg("GLimp_Init() - GetDC failed\n" );
		return false;
	}

	if ( glw_state.minidriver )
	{
		if ( (pixelformat = qwglChoosePixelFormat( glw_state.hDC, &pfd)) == 0 )
		{
			Msg("GLimp_Init() - qwglChoosePixelFormat failed\n");
			return false;
		}
		if ( qwglSetPixelFormat( glw_state.hDC, pixelformat, &pfd) == FALSE )
		{
			Msg("GLimp_Init() - qwglSetPixelFormat failed\n");
			return false;
		}
		qwglDescribePixelFormat( glw_state.hDC, pixelformat, sizeof( pfd ), &pfd );
	}
	else
	{
		if ( ( pixelformat = ChoosePixelFormat( glw_state.hDC, &pfd)) == 0 )
		{
			Msg("GLimp_Init() - ChoosePixelFormat failed\n");
			return false;
		}
		if ( SetPixelFormat( glw_state.hDC, pixelformat, &pfd) == FALSE )
		{
			Msg("GLimp_Init() - SetPixelFormat failed\n");
			return false;
		}
		DescribePixelFormat( glw_state.hDC, pixelformat, sizeof( pfd ), &pfd );

		if ( !( pfd.dwFlags & PFD_GENERIC_ACCELERATED ) )
		{
			extern cvar_t *gl_allow_software;

			if ( gl_allow_software->value )
				glw_state.mcd_accelerated = true;
			else
				glw_state.mcd_accelerated = false;
		}
		else
		{
			glw_state.mcd_accelerated = true;
		}
	}

	/*
	** startup the OpenGL subsystem by creating a context and making
	** it current
	*/
	if ( ( glw_state.hGLRC = qwglCreateContext( glw_state.hDC ) ) == 0 )
	{
		Msg("GLimp_Init() - qwglCreateContext failed\n");
		goto fail;
	}

	if ( !qwglMakeCurrent( glw_state.hDC, glw_state.hGLRC ) )
	{
		Msg("GLimp_Init() - qwglMakeCurrent failed\n");
		goto fail;
	}

	if ( !VerifyDriver() )
	{
		Msg("GLimp_Init() - no hardware acceleration detected\n" );
		goto fail;
	}

	// Vertex arrays
	qglEnableClientState (GL_VERTEX_ARRAY);
	qglEnableClientState (GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer (2, GL_FLOAT, sizeof(tex_array[0]), tex_array[0]);
	qglVertexPointer (3, GL_FLOAT, sizeof(vert_array[0]),vert_array[0]);
	qglColorPointer (4, GL_FLOAT, sizeof(col_array[0]), col_array[0]);

	/*
	** print out PFD specifics 
	*/
	MsgDev(D_NOTE, "GL PFD: color(%d-bits) Z(%d-bit)\n", ( int )pfd.cColorBits, ( int )pfd.cDepthBits );

	ZeroMemory(gl_config.original_ramp, sizeof(gl_config.original_ramp));
	GetDeviceGammaRamp( glw_state.hDC, gl_config.original_ramp );
	vid_gamma->modified = true;

	return true;
fail:
	if ( glw_state.hGLRC )
	{
		qwglDeleteContext( glw_state.hGLRC );
		glw_state.hGLRC = NULL;
	}

	if ( glw_state.hDC )
	{
		ReleaseDC( glw_state.hWnd, glw_state.hDC );
		glw_state.hDC = NULL;
	}
	return false;
}

/*
** GLimp_BeginFrame
*/
void GLimp_BeginFrame( void )
{
	if ( gl_bitdepth->modified )
	{
		if ( gl_bitdepth->value != 0 && !glw_state.allowdisplaydepthchange )
		{
			Cvar_SetValue( "gl_bitdepth", 0 );
			Msg("gl_bitdepth requires Win95 OSR2.x or WinNT 4.x\n" );
		}
		gl_bitdepth->modified = false;
	}
	qglDrawBuffer( GL_BACK );
}

/*
** GLimp_EndFrame
** 
** Responsible for doing a swapbuffers and possibly for other stuff
** as yet to be determined.  Probably better not to make this a GLimp
** function and instead do a call to GLimp_SwapBuffers.
*/
void GLimp_EndFrame (void)
{
	int err;

	err = qglGetError();
	assert( err == GL_NO_ERROR );

	if ( stricmp( gl_drawbuffer->string, "GL_BACK" ) == 0 )
	{
		if ( !qwglSwapBuffers( glw_state.hDC ) )
			Sys_Error("GLimp_EndFrame() - SwapBuffers() failed!\n" );
	}
}   