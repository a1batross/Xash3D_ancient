//=======================================================================
//			Copyright XashXT Group 2007 ©
//			host.c - dedicated and shared host
//=======================================================================

#include <setjmp.h>
#include "engine.h"
#include "server.h"
#include "client.h"

#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ))

physic_exp_t	*pe;
render_exp_t	*re;
vprogs_exp_t	*vm;
host_parm_t	host;	// host parms
stdlib_api_t	com, newcom;

byte	*zonepool;
char	*buildstring = __TIME__ " " __DATE__;

dll_info_t physic_dll = { "physic.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(physic_exp_t) };
dll_info_t render_dll = { "render.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(render_exp_t) };
dll_info_t vprogs_dll = { "vprogs.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(vprogs_exp_t) };

cvar_t	*timescale;
cvar_t	*fixedtime;
cvar_t	*dedicated;
cvar_t	*host_serverstate;
cvar_t	*host_frametime;
cvar_t	*host_cheats;
cvar_t	*r_fullscreen;
cvar_t	*vid_gamma;
cvar_t	*r_xpos;	// X coordinate of window position
cvar_t	*r_ypos;	// Y coordinate of window position
cvar_t	*cm_paused;

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

void Host_InitCommon( uint funcname, int argc, char **argv )
{
	char	dev_level[4];

	newcom = com;

	// overload some funcs
	newcom.error = Host_Error;

	// determine debug and developer mode
	if(FS_CheckParm ("-debug")) host.debug = true;
	if(FS_GetParmFromCmdLine("-dev", dev_level ))
		host.developer = atoi(dev_level);

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

	// phys callback
	pi.api_size = sizeof(physic_imp_t);
	pi.Transform = SV_Transform;
	pi.PlaySound = SV_PlaySound;
	pi.ClientMove = SV_PlayerMove;
	pi.GetModelVerts = SV_GetModelVerts;

	Sys_LoadLibrary( &physic_dll );

	CreatePhysic = (void *)physic_dll.main;
	pe = CreatePhysic( &newcom, &pi );
	
	pe->Init();
}

void Host_FreePhysic( void )
{
	if(physic_dll.link)
	{
		pe->Shutdown();
		memset( &pe, 0, sizeof(pe));
	}
	Sys_FreeLibrary( &physic_dll );
}

void Host_InitRender( void )
{
	static render_imp_t		ri;
	launch_t			CreateRender;
	
	ri.api_size = sizeof(render_imp_t);

          // studio callbacks
	ri.StudioEvent = CL_StudioEvent;
	ri.ShowCollision = pe->DrawCollision;
          
	Sys_LoadLibrary( &render_dll );
	
	CreateRender = (void *)render_dll.main;
	re = CreateRender( &newcom, &ri );

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

void Host_InitVprogs( int argc, char **argv )
{
	launch_t			CreateVprogs;  

	Sys_LoadLibrary( &vprogs_dll );

	CreateVprogs = (void *)vprogs_dll.main;
	vm = CreateVprogs( &newcom, NULL ); // second interface not allowed
	
	vm->Init( host.type, argc, argv );
}

void Host_FreeVprogs( void )
{
	if(vprogs_dll.link)
	{
		vm->Free();
		memset( &vm, 0, sizeof(vm));
	}
	Sys_FreeLibrary( &vprogs_dll );
}

/*
================
Host_AbortCurrentFrame

aborts the current host frame and goes on with the next one
================
*/
void Host_AbortCurrentFrame( void )
{
	longjmp( host.abortframe, 1 );
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
	scr_width = Cvar_Get("width", "640", 0 );
	scr_height = Cvar_Get("height", "480", 0 );
	vid_gamma = Cvar_Get( "vid_gamma", "1", CVAR_ARCHIVE );
	Cmd_AddCommand ("vid_restart", Host_VidRestart_f, "restarts video system" );

	Host_InitRender();
}

/*
=================
Host_Frame
=================
*/
void Host_Frame( double time )
{
	char		*s;

	if(setjmp(host.abortframe)) return;

	rand(); // keep the random time dependent

	Sys_SendKeyEvents(); // get new key events

	do
	{
		s = Sys_ConsoleInput ();
		if(s) Cbuf_AddText (va("%s\n",s));
	} while( s );
	Cbuf_Execute();

	// if at a full screen console, don't update unless needed
	if( host.state != HOST_FRAME || host.type == HOST_DEDICATED )
	{
		Sys_Sleep( 20 );
	}

	SV_Frame( time );
	CL_Frame( time );

	host.framecount++;
}

/*
====================
Host_WndProc

main window procedure
====================
*/
long _stdcall Host_WndProc( HWND hWnd, uint uMsg, WPARAM wParam, LPARAM lParam)
{
	int	 	temp = 0;

	switch (uMsg)
	{
	case WM_MOUSEWHEEL:
		if((short)HIWORD(wParam) > 0)
		{
			Key_Event( K_MWHEELUP, true, host.sv_timer );
			Key_Event( K_MWHEELUP, false, host.sv_timer );
		}
		else
		{
			Key_Event( K_MWHEELDOWN, true, host.sv_timer );
			Key_Event( K_MWHEELDOWN, false, host.sv_timer );
		}
		break;
	case WM_CREATE:
		host.hWnd = hWnd;
		r_xpos = Cvar_Get("r_xpos", "3", CVAR_ARCHIVE );
		r_ypos = Cvar_Get("r_ypos", "22", CVAR_ARCHIVE );
		r_fullscreen = Cvar_Get("fullscreen", "0", CVAR_ARCHIVE | CVAR_LATCH );
		break;
	case WM_DESTROY:
		host.hWnd = NULL;
		break;
	case WM_CLOSE:
		Cbuf_ExecuteText( EXEC_APPEND, "quit" );
		break;
	case WM_ACTIVATE:
		if(LOWORD(wParam) != WA_INACTIVE && HIWORD(wParam)) host.state = HOST_SLEEP;
		else if(LOWORD(wParam) == WA_INACTIVE) host.state = HOST_NOFOCUS;
		else host.state = HOST_FRAME;

		Key_ClearStates();	// FIXME!!!
		SNDDMA_Activate();

		if( host.state == HOST_FRAME )
		{
			M_Activate();
			SetForegroundWindow( hWnd );
			ShowWindow( hWnd, SW_RESTORE );
		}
		else if( r_fullscreen->integer )
		{
			ShowWindow( hWnd, SW_MINIMIZE );
		}
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
			M_Activate();
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
		M_Event( temp );
		break;
	case WM_SYSCOMMAND:
		if( wParam == SC_SCREENSAVE ) return 0;
		break;
	case WM_SYSKEYDOWN:
		if( wParam == 13 && r_fullscreen)
		{
			Cvar_SetValue( "fullscreen", !r_fullscreen->value );
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
		com.strcat (host.rd.buffer, txt);
		return;
	}
	Con_Print( txt ); // echo to client console
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

	if( host.framecount < 3 || host.state == HOST_SHUTDOWN )
		Sys_Error ("%s", hosterror1 );
	else Msg("Host_Error: %s", hosterror1);

	if(recursive)
	{ 
		Msg("Host_RecursiveError: %s", hosterror2 );
		Sys_Error ("%s", hosterror1 );
		return; // don't multiple executes
	}

	recursive = true;
	com.sprintf( host.finalmsg, "Server crashed: %s\n", hosterror1 );
	com.strncpy( host.finalmsg, "Server shutdown\n", MAX_STRING );

	SV_Shutdown( false );
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
	Key_Init();

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
		host_cheats = Cvar_Get("host_cheats", "1", CVAR_SYSTEMINFO );
		Cmd_AddCommand ("error", Host_Error_f, "just throw a fatal error to test shutdown procedures" );
		Cmd_AddCommand ("crash", Host_Crash_f, "a way to force a bus error for development reasons");
          }
          
	cm_paused = Cvar_Get("cm_paused", "0", 0 );
	host_frametime = Cvar_Get ("host_frametime", "0.01", 0);
	host_serverstate = Cvar_Get ("host_serverstate", "0", 0);
	timescale = Cvar_Get ("timescale", "1", 0);
	fixedtime = Cvar_Get ("fixedtime", "0", 0);
	if(host.type == HOST_DEDICATED) dedicated = Cvar_Get ("dedicated", "1", CVAR_INIT);
	else dedicated = Cvar_Get ("dedicated", "0", CVAR_INIT);

	s = va("^1Xash %g ^3%s", GI->version, buildstring );
	Cvar_Get ("version", s, CVAR_SERVERINFO|CVAR_INIT );

	if(dedicated->value) Cmd_AddCommand ("quit", Sys_Quit, "quit the game" );
       
	NET_Init();
	Netchan_Init();
	Host_InitPhysic();
	Host_InitVprogs( argc, argv );
      
	SV_Init();
	CL_Init();
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
	while( host.type != HOST_OFFLINE )
	{
		oldtime = newtime;
		newtime = Sys_DoubleTime();
		time = newtime - oldtime;

		// engine frame
		Host_Frame( time );
	}
}


/*
=================
Host_Shutdown
=================
*/
void Host_Free( void )
{
	host.state = HOST_SHUTDOWN;	// prepare host to normal shutdown
	com.strncpy( host.finalmsg, "Server shutdown\n", MAX_STRING );

	SV_Shutdown( false );
	CL_Shutdown();
	Host_FreeRender();
	NET_Shutdown();
	Host_FreeVprogs();
	Host_FreePhysic();
	Host_FreeCommon();
}