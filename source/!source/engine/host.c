//=======================================================================
//			Copyright XashXT Group 2007 ©
//			host.c - dedicated and shared host
//=======================================================================

#include <setjmp.h>
#include "engine.h"

#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ))

physic_exp_t	*Phys;
render_exp_t	*re;
host_parm_t	host;	// host parms
stdlib_api_t	std;

byte	*zonepool;
int	ActiveApp;
bool	Minimized;
char	*buildstring = __TIME__ " " __DATE__;
viddef_t	viddef;

void Key_Init (void);

dll_info_t physic_dll = { "physic.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(physic_exp_t) };
dll_info_t render_dll = { "render.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(render_exp_t) };

cvar_t	*timescale;
cvar_t	*fixedtime;
cvar_t	*dedicated;

cvar_t	*host_serverstate;
cvar_t	*host_frametime;
cvar_t	*vid_gamma;
cvar_t	*r_xpos;	// X coordinate of window position
cvar_t	*r_ypos;	// Y coordinate of window position
cvar_t	*r_fullscreen;

/*
=======
Host_MapKey

Map from windows to engine keynums
=======
*/
static int Host_MapKey( int key )
{
	int	result, modified;
	bool	is_extended = false;

	modified = ( key >> 16 ) & 255;
	if( modified > 127 ) return 0;

	if ( key & ( 1 << 24 ))
		is_extended = true;

	result = scan_to_key[modified];

	if( !is_extended )
	{
		switch ( result )
		{
		case K_HOME: return K_KP_HOME;
		case K_UPARROW: return K_KP_UPARROW;
		case K_PGUP: return K_KP_PGUP;
		case K_LEFTARROW: return K_KP_LEFTARROW;
		case K_RIGHTARROW: return K_KP_RIGHTARROW;
		case K_END: return K_KP_END;
		case K_DOWNARROW: return K_KP_DOWNARROW;
		case K_PGDN: return K_KP_PGDN;
		case K_INS: return K_KP_INS;
		case K_DEL: return K_KP_DEL;
		default: return result;
		}
	}
	else
	{
		switch ( result )
		{
		case K_PAUSE: return K_KP_NUMLOCK;
		case 0x0D: return K_KP_ENTER;
		case 0x2F: return K_KP_SLASH;
		case 0xAF: return K_KP_PLUS;
		}
		return result;
	}
}

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
	launch_t			CreatePhysic;  
	stdlib_api_t		io = Host_GetStdio( false );

	// phys callback
	pi.api_size = sizeof(physic_imp_t);
	pi.Transform = SV_Transform;

	Sys_LoadLibrary( &physic_dll );

	CreatePhysic = (void *)physic_dll.main;
	Phys = CreatePhysic( &io, &pi );
	
	Phys->Init();
}

void Host_FreePhysic( void )
{
	if(physic_dll.link)
	{
		Phys->Shutdown();
		memset( &Phys, 0, sizeof(Phys));
	}
	Sys_FreeLibrary( &physic_dll );
}

typedef struct vidmode_s
{
	const char *description;
	int         width, height;
	int         mode;
} vidmode_t;

vidmode_t vid_modes[] =
{
	{ "Mode 0: 640x480",   640, 480,   1 },
	{ "Mode 1: 800x600",   800, 600,   2 },
	{ "Mode 2: 1024x768",  1024, 768,  3 },
	{ "Mode 3: 1280x960",  1280, 960,  4 },
	{ "Mode 4: 1280x1024", 1280, 1024, 5 },
	{ "Mode 5: 1600x1200", 1600, 1200, 6 },
	{ "Mode 6: 2048x1536", 2048, 1536, 7 }
};

bool VID_GetModeInfo( int *width, int *height, int mode )
{
	if ( mode < 0 || mode >= VID_NUM_MODES )
		return false;

	*width  = vid_modes[mode].width;
	*height = vid_modes[mode].height;

	return true;
}

void VID_NewWindow ( int width, int height)
{
	viddef.width  = width;
	viddef.height = height;

	cl.force_refdef = true;		// can't use a paused refdef
}

void Host_InitRender( void )
{
	static render_imp_t		ri;
	stdlib_api_t		io = Host_GetStdio( false );
	launch_t			CreateRender;
	
	ri.api_size = sizeof(render_imp_t);

	// console interaction
	ri.Cmd_AddCommand = Cmd_AddCommand;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Cmd_ExecuteText = Cbuf_ExecuteText;
	ri.Cvar_Get = _Cvar_Get;
	ri.Cvar_Set = Cvar_Set;
	ri.Cvar_SetValue = Cvar_SetValue;

          // studio callbacks
	ri.StudioEvent = CL_StudioEvent;
	ri.ShowCollision = Phys->ShowCollision;

	ri.Vid_GetModeInfo = VID_GetModeInfo;
	ri.Vid_MenuInit = VID_MenuInit;
	ri.Vid_NewWindow = VID_NewWindow;
          
	Sys_LoadLibrary( &render_dll );
	
	CreateRender = (void *)render_dll.main;
	re = CreateRender( &io, &ri );

	if(!re->Init(GetModuleHandle(NULL), Host_WndProc )) 
		Sys_Error("VID_InitRender: can't init render.dll\nUpdate your opengl drivers\n");

	CL_Snd_Restart_f();
}

void Host_FreeRender( void )
{
	if(render_dll.link)
	{
		re->Shutdown();
		memset( &re, 0, sizeof(re));
	}
	Sys_FreeLibrary( &render_dll );
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
Host_VidRestart_f

Restart the video subsystem
=================
*/
void Host_VidRestart_f( void )
{
	cl.force_refdef = true;	// can't use a paused refdef
	S_StopAllSounds();		// don't let them loop during the restart
	cl.refresh_prepped = false;

	Host_FreeRender();		// release render.dll
	Host_InitRender();		// load it again
}

/*
============
VID_Init
============
*/
void VID_Init( void )
{
	vid_gamma = Cvar_Get( "vid_gamma", "1", CVAR_ARCHIVE );
	Cmd_AddCommand ("vid_restart", Host_VidRestart_f, "restarts video system" );

	Host_InitRender();
}

/*
==================
VID_AppActivate
==================
*/
void VID_AppActivate(BOOL fActive, BOOL minimize)
{
	Minimized = minimize;

	Key_ClearStates();	// FIXME!!!

	if (fActive && !Minimized ) ActiveApp = true;
	else ActiveApp = false;

	if (!ActiveApp ) IN_Activate( false );
	else IN_Activate( true );
}

/*
=================
Host_Frame
=================
*/
void Host_Frame( double time )
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
====================
MainWndProc

main window procedure
====================
*/
long _stdcall Host_WndProc( HWND hWnd, uint uMsg, WPARAM wParam, LPARAM lParam)
{
	int		i, zDelta, temp = 0; 

	switch (uMsg)
	{
	case WM_MOUSEWHEEL:
		//if(m_mouse->integer != 1 || (!r_fullscreen->integer && (cls.key_dest == key_console)))
		{
			zDelta = (short)HIWORD( wParam ) / 120;
			if( zDelta > 0 )
			{
				for(i = 0; i < zDelta; i++)
				{
					Key_Event( K_MWHEELUP, true, host.sv_timer );
					Key_Event( K_MWHEELUP, false, host.sv_timer );
				}
			}
			else
			{
				for(i = 0; i < -zDelta; i++)
				{
					Key_Event( K_MWHEELDOWN, true, host.sv_timer );
					Key_Event( K_MWHEELDOWN, false, host.sv_timer );
				}
			}
			return 0;
		}
		break;
	case WM_CREATE:
		host.hWnd = hWnd;
		r_xpos = Cvar_Get ("r_xpos", "3", CVAR_ARCHIVE );
		r_ypos = Cvar_Get ("r_ypos", "22", CVAR_ARCHIVE );
		r_fullscreen = Cvar_Get ("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH );
		break;
	case WM_DESTROY:
		host.hWnd = NULL;
		break;
	case WM_CLOSE:
		Cbuf_ExecuteText( EXEC_APPEND, "quit" );
		break;
	case WM_ACTIVATE:
		VID_AppActivate(LOWORD(wParam) != WA_INACTIVE, (BOOL) HIWORD(wParam));
		SNDDMA_Activate();
		if( render_dll.link ) re->AppActivate( LOWORD(wParam) != WA_INACTIVE);
		break;
	case WM_MOVE:
		if (!r_fullscreen->integer )
		{
			RECT 		r;
			int		xPos, yPos, style;

			xPos = (short) LOWORD(lParam);    // horizontal position 
			yPos = (short) HIWORD(lParam);    // vertical position 

			r.left = r.top = 0;
			r.right = r.bottom = 1;
			style = GetWindowLong( hWnd, GWL_STYLE );
			AdjustWindowRect( &r, style, FALSE );

			Cvar_SetValue( "r_xpos", xPos + r.left);
			Cvar_SetValue( "r_ypos", yPos + r.top);
			r_xpos->modified = false;
			r_ypos->modified = false;
			if (ActiveApp) IN_Activate( true );
		}
		break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEMOVE:
		if(wParam & MK_LBUTTON) temp |= 1;
		if(wParam & MK_RBUTTON) temp |= 2;
		if(wParam & MK_MBUTTON) temp |= 4;
		IN_MouseEvent( temp );
		break;
	case WM_SYSCOMMAND:
		if( wParam == SC_SCREENSAVE ) return 0;
		break;
	case WM_SYSKEYDOWN:
		if( wParam == 13 )
		{
			if( r_fullscreen ) Cvar_SetValue( "r_fullscreen", !r_fullscreen->value );
			return 0;
		}
		// intentional fallthrough
	case WM_KEYDOWN:
		Key_Event( Host_MapKey( lParam ), true, host.sv_timer);
		break;
	case WM_SYSKEYUP:
	case WM_KEYUP:
		Key_Event( Host_MapKey( lParam ), false, host.sv_timer);
		break;
	case WM_CHAR:
		CL_CharEvent( wParam );
		break;
	}
	return DefWindowProc( hWnd, uMsg, wParam, lParam );
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
	if( Cmd_Argc() == 1 ) Sys_Error( "break\n" );
	else Sys_Error( "%s\n", Cmd_Argv( 1 ));
}

/*
=================
Host_Crash_f
=================
*/
static void Host_Crash_f (void)
{
	*(int *)0 = 0xffffffff;
}

/*
=================
Host_Init
=================
*/
void Host_Init (uint funcname, int argc, char **argv)
{
	char	*s;

	host.state = HOST_INIT;	// initialzation started
	host.type = funcname;

	srand(time(NULL));		// init random generator

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
	if(host.developer)
	{
		Cmd_AddCommand ("error", Host_Error_f, "just throw a fatal error to test shutdown procedures" );
		Cmd_AddCommand ("crash", Host_Crash_f, "a way to force a bus error for development reasons");
          }
          
	host_frametime = Cvar_Get ("host_frametime", "0.01", 0);
	host_serverstate = Cvar_Get ("host_serverstate", "0", 0);
	timescale = Cvar_Get ("timescale", "1", 0);
	fixedtime = Cvar_Get ("fixedtime", "0", 0);
	if(host.type == HOST_DEDICATED) dedicated = Cvar_Get ("dedicated", "1", CVAR_INIT);
	else dedicated = Cvar_Get ("dedicated", "0", CVAR_INIT);

	s = va("^1Xash %g ^3%s", XASH_VERSION, buildstring );
	Cvar_Get ("version", s, CVAR_SERVERINFO|CVAR_INIT);

	if(dedicated->value) Cmd_AddCommand ("quit", Sys_Quit, "quit the game" );
       
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

		Host_Frame( time ); // engine frame
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
	SV_Shutdown("Server shutdown\n", false );
	CL_Shutdown();
	Host_FreeRender();
	NET_Shutdown();
	Host_FreePhysic();
	Host_FreeCommon();
}