//=======================================================================
//			Copyright XashXT Group 2008 ©
//			system.c - system implementation
//=======================================================================

#include "common.h"

/*
===============================================================================

SYSTEM IO

===============================================================================
*/
void Sys_Error( const char *error, ... )
{
	char		errorstring[MAX_SYSPATH];
	static bool	recursive = false;
	va_list		argptr;

	va_start( argptr, error );
	com.vsprintf( errorstring, error, argptr );
	va_end( argptr );

	// don't multiple executes
	if( recursive )
	{
		// echo to system console and log
		Sys_Print( va("Sys_RecursiveError: %s\n", errorstring ));
		return;
	}	

	recursive = true;

	// prepare host to close
	com.sprintf( host.finalmsg, "Server fatal crashed: %s\n", errorstring );
	host.state = HOST_ERROR;	// lock shutdown state
	Host_FreeRender();		// close render to show message error

	com.error("%s", errorstring );
}