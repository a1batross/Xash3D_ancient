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
void Sys_Error( const char *error, ... )
{
	char		syserror1[MAX_INPUTLINE];
	va_list		argptr;
	
	va_start( argptr, error );
	vsprintf( syserror1, error, argptr );
	va_end( argptr );

	SV_Shutdown(va("Server fatal crashed: %s\n", syserror1), false);
	CL_Shutdown();

	std.error("%s", syserror1);
}

void Sys_Quit (void)
{
	std.exit();
}

void Sys_Print(const char *pMsg)
{
	std.print((char *)pMsg );
}

double Sys_DoubleTime( void )
{
	// precision timer
	host.realtime = pi->DoubleTime();
	return host.realtime;
}

void Sys_Sleep(int milliseconds)
{
	if (milliseconds < 1)
		milliseconds = 1;
	Sleep(milliseconds);
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

	// keep the random time dependent
	rand();
}



/*
================
Sys_GetClipboardData

FIXME: move to launcher.dll
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
int Sys_Milliseconds (void)
{
	static int	sys_timeBase;
	static int	sys_curtime;
	static bool	init = false;

	if (!init)
	{
		sys_timeBase = timeGetTime();
		init = true;
	}
	sys_curtime = timeGetTime() - sys_timeBase;

	return sys_curtime;
}

/*
===============================================================================

DLL MANAGEMENT

===============================================================================
*/
void Sys_UnloadLibrary (void* handle)
{
	if (handle == NULL) return;

	FreeLibrary (handle);
	handle = NULL;
}

void* Sys_GetProcAddress (void *handle, const char* name)
{
	return (void *)GetProcAddress (handle, name);
}

bool Sys_LoadLibrary (const char** dllnames, void* handle, const dllfunc_t *fcts)
{
	const dllfunc_t *func;
	HMODULE dllhandle = 0;
	unsigned int i;

	if (handle == NULL)
		return false;

	// Initializations
	for (func = fcts; func && func->name != NULL; func++)
		*func->func = NULL;

	// Try every possible name
	Con_Printf ("Trying to load library...");
	for (i = 0; dllnames[i] != NULL; i++)
	{
		Con_Printf (" \"%s\"", dllnames[i]);
		dllhandle = LoadLibrary (dllnames[i]);
		if (dllhandle) break;
	}

	// No DLL found
	if (! dllhandle)
	{
		Con_Printf(" - failed.\n");
		return false;
	}

	Con_Printf(" - loaded.\n");

	// Get the function adresses
	for (func = fcts; func && func->name != NULL; func++)
	{
		if (!(*func->func = (void *) Sys_GetProcAddress (dllhandle, func->name)))
		{
			Con_Printf ("Missing function \"%s\" - broken library!\n", func->name);
			Sys_UnloadLibrary (&dllhandle);
			return false;
		}
	}

	handle = dllhandle;
	return true;
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