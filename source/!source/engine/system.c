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
uint sys_msg_time;
uint sys_frame_time;

bool s_win95;
int starttime;

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

	timeBeginPeriod( 1 );

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx (&vinfo)) Sys_Error ("Couldn't get OS info");

	if (vinfo.dwMajorVersion < 4) Sys_Error ("%s requires windows version 4 or greater", GI.title);
	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32s) Sys_Error ("%s doesn't run on Win32s", GI.title);
	else if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS ) s_win95 = true;
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
		sys_msg_time = msg.time;
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	// grab frame time 
	sys_frame_time = timeGetTime();	// FIXME: should this be at start?
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
================
Sys_Milliseconds
================
*/
int	curtime;
int Sys_Milliseconds (void)
{
	static int		base;
	static bool	initialized = false;
          
          //return (int)(pi.DoubleTime() * 1000);
	
	if (!initialized)
	{	// let base retain 16 bits of effectively random data
		base = timeGetTime() & 0xffff0000;
		initialized = true;
	}
	curtime = timeGetTime() - base;

	return curtime;
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

HINSTANCE	game_library;

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame (void)
{
	if (!FreeLibrary (game_library))
		Com_Error (ERR_FATAL, "FreeLibrary failed for game library");
	game_library = NULL;
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void *Sys_GetGameAPI (const char* procname, void *parms)
{
	void	*(*GetGameAPI) (void *);
	char	basepath[MAX_SYSPATH];
	search_t	*gamedll;
          int	i;
	
	if (game_library)
		Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	//find server.dll
	gamedll = FS_Search( "bin/*.dll" );
	if(!gamedll) Com_Error (ERR_DROP, "can't found game DLL");

	// now run through the search paths
	for( i = 0; i < gamedll->numfilenames; i++ )
	{
		sprintf(basepath, "%s/%s", GI.gamedir, gamedll->filenames[i]);
		game_library = LoadLibrary ( basepath );

		if (!game_library)
		{
			sprintf(basepath, "%s/%s", GI.basedir, gamedll->filenames[i]);		
			game_library = LoadLibrary ( basepath );
                    }

		if (game_library)
		{
			if (( GetGameAPI = (void *)GetProcAddress( game_library, procname )) == 0 )
				Sys_UnloadGame();
			else break;
		}
		else Msg("Can't loading %s\n", gamedll->filenames[i] );
	}

	GetGameAPI = (void *)GetProcAddress (game_library, procname );

	if (!GetGameAPI)
	{
		Sys_UnloadGame ();		
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