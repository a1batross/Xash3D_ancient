//=======================================================================
//			Copyright XashXT Group 2007 ©
//			host.c - dedicated and shared host
//=======================================================================

#include <setjmp.h>
#include "engine.h"

physic_exp_t	*Phys;
host_parm_t	host;	// host parms
stdlib_api_t	std;

byte	*zonepool;
int	ActiveApp;
bool	Minimized;
char	*buildstring = __TIME__ " " __DATE__;

void Key_Init (void);

HINSTANCE	global_hInstance;
dll_info_t physic_dll = { "physic.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(physic_exp_t) };

cvar_t	*timescale;
cvar_t	*fixedtime;
cvar_t	*dedicated;

cvar_t	*host_serverstate;
cvar_t	*host_frametime;

stdlib_api_t Host_GetStdio( bool crash_on_error )
{
	static stdlib_api_t		io;

	io = std;

	// overload some funcs
	io.print = Host_Print;
	io.printf = Host_Printf;
	io.dprintf = Host_DPrintf;
	io.wprintf = Host_DWarnf;

	if(crash_on_error) io.error = Sys_Error;
	else io.error = Host_Error;

	return io;
}

void Host_InitCommon( uint funcname, int argc, char **argv )
{
	// TODO: init basedir here
	FS_LoadGameInfo("gameinfo.txt");
	zonepool = Mem_AllocPool("Zone Engine");
}

void Host_FreeCommon( void )
{
	Mem_FreePool( &zonepool );
}

void Host_InitPhysic( void )
{
	static physic_imp_t		pi;
	launch_t			CreatePhys;  
	stdlib_api_t		io = Host_GetStdio( false );

	// phys callback
	pi.api_size = sizeof(physic_imp_t);
	pi.Transform = SV_Transform;

	Sys_LoadLibrary( &physic_dll );

	CreatePhys = (void *)physic_dll.main;
	Phys = CreatePhys( &io, &pi );
	
	Phys->Init();
}

void Host_FreePhysic( void )
{
	if(physic_dll.link)
	{
		Phys->Shutdown();
	}
	Sys_FreeLibrary( &physic_dll );
}

/*
================
Host_AbortCurrentFrame

aborts the current host frame and goes on with the next one
================
*/
void Host_AbortCurrentFrame( void )
{
	longjmp(host.abortframe, 1);
}

/*
==================
Host_GetServerState
==================
*/
int Host_ServerState( void )
{
	return host_serverstate->integer;
}

/*
==================
Host_SetServerState
==================
*/
void Host_SetServerState( int state )
{
	Cvar_SetValue("host_serverstate", state );
}


/*
=================
Host_Init
=================
*/
void Host_Init (uint funcname, int argc, char **argv)
{
	char	*s;

	host.state = HOST_INIT;	//initialzation started

	global_hInstance = (HINSTANCE)GetModuleHandle( NULL );
	host.type = funcname;

	srand(time(NULL)); // init random generator

	Host_InitCommon( funcname, argc, argv ); // loading common.dll
	Cmd_Init( argc, argv );
	Cvar_Init();
	Key_Init();
	PRVM_Init();

	// get default configuration
#if 1
	Cbuf_AddText("exec keys.rc\n");
	Cbuf_AddText("exec vars.rc\n");
#else
	Cbuf_AddText("exec default.cfg\n");
	Cbuf_AddText("exec config.cfg\n");
#endif
	Cbuf_Execute();

	// init commands and vars
	Cmd_AddCommand ("error", Host_Error_f);

	host_frametime = Cvar_Get ("host_frametime", "0.01", 0);
	host_serverstate = Cvar_Get ("host_serverstate", "0", 0);
	timescale = Cvar_Get ("timescale", "1", 0);
	fixedtime = Cvar_Get ("fixedtime", "0", 0);
	if(host.type == HOST_DEDICATED) dedicated = Cvar_Get ("dedicated", "1", CVAR_INIT);
	else dedicated = Cvar_Get ("dedicated", "0", CVAR_INIT);

	s = va("^1Xash %g ^3%s", XASH_VERSION, buildstring );
	Cvar_Get ("version", s, CVAR_SERVERINFO|CVAR_INIT);

	if (dedicated->value) Cmd_AddCommand ("quit", Sys_Quit);
       
	NET_Init();
	Netchan_Init();
	Host_InitPhysic();
       
	SV_Init();
	CL_Init();

	Cbuf_AddText("exec init.rc\n");
	Cbuf_Execute();

	// if stuffcmds wasn't run, then init.rc is probably missing, use default
	if(!host.stuffcmdsrun) Cbuf_ExecuteText( EXEC_NOW, "stuffcmds\n" );
	Sys_DoubleTime(); // initialize timer
}

/*
=================
Host_Frame
=================
*/
void Host_Frame (double time)
{
	char		*s;

	if (setjmp(host.abortframe)) return;

	rand(); // keep the random time dependent

	Sys_SendKeyEvents(); // get new key events

	do
	{
		s = Sys_ConsoleInput ();
		if(s) Cbuf_AddText (va("%s\n",s));
	} while( s );
	Cbuf_Execute();

	// if at a full screen console, don't update unless needed
	if (Minimized || host.type == HOST_DEDICATED )
	{
		Sys_Sleep (1);
	}

	SV_Frame (time);
	CL_Frame (time);

	host.framecount++;
}

/*
=================
Host_Main
=================
*/
void Host_Main( void )
{
	static double	time, oldtime, newtime;

	oldtime = host.realtime;

	// main window message loop
	while (host.type != HOST_OFFLINE)
	{
		oldtime = newtime;
		newtime = Sys_DoubleTime();
		time = newtime - oldtime;

		Host_Frame (time); // engine frame
	}
	host.state = HOST_SHUTDOWN;
}


/*
=================
Host_Shutdown
=================
*/
void Host_Free( void )
{
	SV_Shutdown ("Server shutdown\n", false);
	CL_Shutdown ();
	NET_Shutdown();
	Host_FreePhysic();
	Host_FreeCommon();
}

/*
================
Host_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void Host_Print( const char *txt )
{
	if(host.rd.target)
	{
		if((strlen (txt) + strlen(host.rd.buffer)) > (host.rd.buffersize - 1))
		{
			if(host.rd.flush)
			{
				host.rd.flush(host.rd.target, host.rd.buffer);
				*host.rd.buffer = 0;
			}
		}
		strcat (host.rd.buffer, txt);
		return;
	}

	Con_Print( txt ); // echo to client console
	Sys_Print( txt ); // echo to system console
	// sys print also stored messages into system log
}

/*
=============
Host_Printf

Both client and server can use this, and it will output
to the apropriate place.
=============
*/
void Host_Printf( const char *fmt, ... )
{
	va_list		argptr;
	char		msg[MAX_INPUTLINE];

	va_start( argptr, fmt );
	vsprintf( msg, fmt, argptr );
	va_end( argptr );

	Host_Print( msg );
}


/*
================
Host_DPrintf

A Msg that only shows up in developer mode
================
*/
void Host_DPrintf( int level, const char *fmt, ... )
{
	va_list		argptr;
	char		msg[MAX_INPUTLINE];
		
	// don't confuse non-developers with techie stuff...	
	if(host.developer < level) return;

	va_start( argptr, fmt );
	vsprintf( msg, fmt, argptr );
	va_end( argptr );

	switch(level)
	{
	case D_INFO:	
		Host_Print( msg );
		break;
	case D_WARN:
		Host_Print(va("^3Warning:^7 %s", msg));
		break;
	case D_ERROR:
		Host_Print(va("^1Error:^7 %s", msg));
		break;
	case D_LOAD:
		Host_Print(va("^2Loading: ^7%s", msg));
		break;
	case D_NOTE:
		Host_Print( msg );
		break;
	case D_MEMORY:
		Host_Print(va("^6Mem: ^7%s", msg));
		break;
	}
}

/*
================
Host_DWarnf

A Warning that only shows up in debug mode
================
*/
void Host_DWarnf( const char *fmt, ... )
{
	va_list		argptr;
	char		msg[MAX_INPUTLINE];
		
	// don't confuse non-developers with techie stuff...
	if (!host.debug) return;

	va_start( argptr, fmt );
	vsprintf( msg, fmt, argptr );
	va_end( argptr );
	
	Host_Print(va("^3Warning:^7 %s", msg));
}

/*
=================
Host_Error
=================
*/
void Host_Error( const char *error, ... )
{
	static char	hosterror1[MAX_INPUTLINE];
	static char	hosterror2[MAX_INPUTLINE];
	static bool	recursive = false;
	va_list		argptr;

	va_start( argptr, error );
	vsprintf( hosterror1, error, argptr );
	va_end( argptr );

	if (host.framecount < 3 || host.state == HOST_SHUTDOWN)
		Sys_Error ("%s", hosterror1 );
	else Host_Printf("Host_Error: %s", hosterror1);

	if(recursive)
	{ 
		Msg("Host_Error: recursive %s", hosterror2);
		Sys_Error ("%s", hosterror1);
	}
	recursive = true;
	strncpy(hosterror2, hosterror1, sizeof(hosterror2));

	SV_Shutdown (va("Server crashed: %s", hosterror1), false);
	CL_Drop(); // drop clients

	recursive = false;
	Host_AbortCurrentFrame();
	host.state = HOST_ERROR;
}

void Host_Error_f( void )
{
	Host_Error( "%s\n", Cmd_Argv(1));
}