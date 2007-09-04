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
// sys_win.h

#include <windows.h>
#include "engine.h"

extern HWND cl_hwnd;

//engine builddate
char *buildstring = __TIME__ " " __DATE__;
stdinout_api_t std;

/*
===============================================================================

SYSTEM IO

===============================================================================
*/
void Sys_Error( char *msg, ... )
{
	va_list		argptr;
	char		fmt[MAX_INPUTLINE];
	static bool	inupdate;
	
	va_start (argptr, msg);
	vsprintf (fmt, msg, argptr);
	va_end (argptr);

	Com_Error (ERR_FATAL, "%s", fmt);
}

void Sys_Quit (void)
{
	std.exit();
}

void Sys_Print(const char *pMsg)
{
	std.print((char *)pMsg );
}

/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput( void )
{
	return std.input();
}

/*
================
Sys_Init

move to launcher.dll
================
*/
void Sys_Init (void)
{
	OSVERSIONINFO	vinfo;

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx (&vinfo)) Sys_Error ("Couldn't get OS info");
	if (vinfo.dwMajorVersion < 4) Sys_Error ("%s requires windows version 4 or greater", GI.title);
}

/*
================
Sys_SendKeyEvents

Send Key_Event calls
================
*/
void Sys_SendKeyEvents (void)
{
	MSG        msg;

	while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (!GetMessage (&msg, NULL, 0, 0)) Sys_Quit ();
		host.sv_timer = msg.time;
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	// grab frame time 
	host.cl_timer = timeGetTime();	// FIXME: should this be at start?
}



/*
================
Sys_GetClipboardData

================
*/
char *Sys_GetClipboardData( void )
{
	char *data = NULL;
	char *cliptext;

	if ( OpenClipboard( NULL ) != 0 )
	{
		HANDLE hClipboardData;

		if ( ( hClipboardData = GetClipboardData( CF_TEXT ) ) != 0 )
		{
			if ( ( cliptext = GlobalLock( hClipboardData ) ) != 0 ) 
			{
				data = Z_Malloc( GlobalSize( hClipboardData ) + 1 );
				strcpy( data, cliptext );
				GlobalUnlock( hClipboardData );
			}
		}
		CloseClipboard();
	}
	return data;
}

/*
==============================================================================

 WINDOWS CRAP

==============================================================================
*/

/*
=================
Sys_AppActivate
=================
*/
void Sys_AppActivate (void)
{
	ShowWindow ( cl_hwnd, SW_RESTORE);
	SetForegroundWindow ( cl_hwnd );
}

/*
========================================================================

GAME DLL

========================================================================
*/
/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame( void *hinstance )
{
	if(!hinstance) return;
	FreeLibrary(hinstance);
	hinstance = NULL;
}

/*
=================
Sys_LoadGame

Loads the game dll
=================
*/
void *Sys_LoadGame (const char* procname, void *hinstance, void *parms)
{
	void	*(*GetGameAPI) (void *);
	char	basepath[MAX_SYSPATH];
	search_t	*gamedll;
          int	i;
	
	Sys_UnloadGame( hinstance );

	//find server.dll
	gamedll = FS_Search( "bin/*.dll" );
	if(!gamedll) 
	{
		Com_Error (ERR_DROP, "Can't find game.dll\n");
		return NULL;
	}

	// now run through the search paths
	for( i = 0; i < gamedll->numfilenames; i++ )
	{
		sprintf(basepath, "%s/%s", GI.gamedir, gamedll->filenames[i]);
		hinstance = LoadLibrary ( basepath );

		if (!hinstance)
		{
			sprintf(basepath, "%s/%s", GI.basedir, gamedll->filenames[i]);		
			hinstance = LoadLibrary ( basepath );
                    }

		if (hinstance)
		{
			if (( GetGameAPI = (void *)GetProcAddress( hinstance, procname )) == 0 )
				Sys_UnloadGame( hinstance );
			else break;
		}
		else MsgWarn("Can't loading %s\n", gamedll->filenames[i] );
	}

	GetGameAPI = (void *)GetProcAddress (hinstance, procname );

	if (!GetGameAPI)
	{
		Sys_UnloadGame( hinstance );
		return NULL;
	}
	return GetGameAPI (parms);
}

//=======================================================================

/*
==================
DllMain

==================
*/
launcher_exp_t DLLEXPORT *CreateAPI( stdinout_api_t histd )
{
	static launcher_exp_t Host;

	std = histd;

	Host.Init = Host_Init;
	Host.Main = Host_Main;
	Host.Free = Host_Free;

	return &Host;
}