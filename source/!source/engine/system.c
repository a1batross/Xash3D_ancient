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
	host.cl_timer = host.realtime * 1000;	// FIXME: should this be at start?
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
==================
DllMain

==================
*/
launcher_exp_t DLLEXPORT *CreateAPI( stdinout_api_t *input )
{
         	static launcher_exp_t Host;

	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but first argument will be 0
	// and always make exception, run simply check for avoid it
	if(input) std = *input;

	Host.apiversion = LAUNCHER_API_VERSION;
	Host.api_size = sizeof(launcher_exp_t);

	Host.Init = Host_Init;
	Host.Main = Host_Main;
	Host.Free = Host_Free;

	return &Host;
}