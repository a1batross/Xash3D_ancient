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
	This code is part of DynGL, a method of dynamically loading an OpenGL
	library without much pain designed by Joseph Carter and is based
	loosely on previous work done both by Zephaniah E. Hull and Joseph.

	Both contributors have decided to disclaim all Copyright to this work.
	It is released to the Public Domain WITHOUT ANY WARRANTY whatsoever,
	express or implied, in the hopes that others will use it instead of
	other less-evolved hacks which usually don't work right.  ;)
*/

/*
	The following code is loosely based on DynGL code by Joseph Carter 
	and Zephaniah E. Hull. Adapted by Victor Luchits for qfusion project.
*/

/*
** QGL_WIN.C
**
** This file implements the operating system binding of GL to QGL function
** pointers.  When doing a port of Qfusion you must implement the following
** two functions:
**
** QGL_Init() - loads libraries, assigns function pointers, etc.
** QGL_Shutdown() - unloads libraries, NULLs function pointers
*/

#include <windows.h>
#include <GL/gl.h>
#include "launch_api.h"
#include "glw_win.h"

extern stdlib_api_t		com;		// engine toolbox

#define QGL_EXTERN

#define QGL_FUNC(type,name,params) type (APIENTRY * p##name) params;
#define QGL_EXT(type,name,params) type (APIENTRY * p##name) params;
#define QGL_WGL(type,name,params) type (APIENTRY * p##name) params;
#define QGL_WGL_EXT(type,name,params) type (APIENTRY * p##name) params;
#define QGL_GLX(type,name,params)
#define QGL_GLX_EXT(type,name,params)

#include "qgl.h"

#undef QGL_GLX_EXT
#undef QGL_GLX
#undef QGL_WGL_EXT
#undef QGL_WGL
#undef QGL_EXT
#undef QGL_FUNC

static const char *_pglGetGLWExtensionsString( void );
static const char *_pglGetGLWExtensionsStringInit( void );

/*
** QGL_Shutdown
**
** Unloads the specified DLL then nulls out all the proc pointers.
*/
void QGL_Shutdown( void )
{
	if( glw_state.hinstOpenGL )
		FreeLibrary( glw_state.hinstOpenGL );
	glw_state.hinstOpenGL = NULL;

	pglGetGLWExtensionsString = NULL;

#define QGL_FUNC(type,name,params) (p##name) = NULL;
#define QGL_EXT(type,name,params) (p##name) = NULL;
#define QGL_WGL(type,name,params) (p##name) = NULL;
#define QGL_WGL_EXT(type,name,params) (p##name) = NULL;
#define QGL_GLX(type,name,params)
#define QGL_GLX_EXT(type,name,params)

#include "qgl.h"

#undef QGL_GLX_EXT
#undef QGL_GLX
#undef QGL_WGL_EXT
#undef QGL_WGL
#undef QGL_EXT
#undef QGL_FUNC
}

#pragma warning (disable : 4113 4133 4047 )

/*
** QGL_Init
**
** This is responsible for binding our pgl function pointers to 
** the appropriate GL stuff. In Windows this means doing a 
** LoadLibrary and a bunch of calls to GetProcAddress. On other
** operating systems we need to do the right thing, whatever that
** might be.
** 
*/
bool QGL_Init( const char *dllname )
{
	if( ( glw_state.hinstOpenGL = LoadLibrary( dllname ) ) == 0 ) {
		char *buf;
		
		buf = NULL;
		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &buf, 0, NULL );
		MsgDev( D_ERROR, "%s\n", buf );

		return false;
	}

#define QGL_FUNC(type,name,params) (p##name) = ( void * )GetProcAddress( glw_state.hinstOpenGL, #name ); \
	if( !(p##name) ) { MsgDev( D_ERROR, "QGL_Init: Failed to get address for %s\n", #name ); return false; }
#define QGL_EXT(type,name,params) (p##name) = NULL;
#define QGL_WGL(type,name,params) (p##name) = ( void * )GetProcAddress( glw_state.hinstOpenGL, #name ); \
	if( !(p##name) ) { MsgDev( D_ERROR, "QGL_Init: Failed to get address for %s\n", #name ); return false; }
#define QGL_WGL_EXT(type,name,params) (p##name) = NULL;
#define QGL_GLX(type,name,params)
#define QGL_GLX_EXT(type,name,params)

#include "qgl.h"

#undef QGL_GLX_EXT
#undef QGL_GLX
#undef QGL_WGL_EXT
#undef QGL_WGL
#undef QGL_EXT
#undef QGL_FUNC

	pglGetGLWExtensionsString = _pglGetGLWExtensionsStringInit;

	return true;
}

/*
** pglGetProcAddress
*/
void *pglGetProcAddress( const GLubyte *procName ) {
	return (void *)pwglGetProcAddress( (LPCSTR)procName );
}

/*
** pglGetGLWExtensionsString
*/
static const char *_pglGetGLWExtensionsStringInit( void )
{
	pwglGetExtensionsStringEXT = ( void * )pglGetProcAddress( (const GLubyte *)"wglGetExtensionsStringEXT" );
	pglGetGLWExtensionsString = _pglGetGLWExtensionsString;
	return pglGetGLWExtensionsString ();
}

static const char *_pglGetGLWExtensionsString( void )
{
	if( pwglGetExtensionsStringEXT )
		return pwglGetExtensionsStringEXT ();
	return NULL;
}
