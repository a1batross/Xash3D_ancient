//=======================================================================
//			Copyright XashXT Group 2007 ©
//			input.c - win32 input devices
//=======================================================================

#include "common.h"
#include "input.h"
#include "client.h"

#define WM_MOUSEWHEEL	( WM_MOUSELAST + 1 )	// message that will be supported by the OS
#define WND_HEADSIZE	wnd_caption		// some offset
#define WND_BORDER		3			// sentinel border in pixels

bool	in_mouseactive;		// false when not focus app
bool	in_restore_spi;
bool	in_mouseinitialized;
int	in_originalmouseparms[3];
int	in_newmouseparms[3] = { 0, 0, 1 };
bool	in_mouse_suspended;
bool	in_mouseparmsvalid;
int	in_mouse_buttons;
int	in_mouse_oldbuttonstate;
int	window_center_x, window_center_y;
RECT	window_rect, real_rect;
uint	in_mouse_wheel;
int	wnd_caption;

cvar_t	*scr_xpos;		// X coordinate of window position
cvar_t	*scr_ypos;		// Y coordinate of window position
cvar_t	*scr_fullscreen;

static byte scan_to_key[128] = 
{ 
	0,27,'1','2','3','4','5','6','7','8','9','0','-','=',K_BACKSPACE,9,
	'q','w','e','r','t','y','u','i','o','p','[',']', 13 , K_CTRL,
	'a','s','d','f','g','h','j','k','l',';','\'','`',
	K_SHIFT,'\\','z','x','c','v','b','n','m',',','.','/',K_SHIFT,
	'*',K_ALT,' ',K_CAPSLOCK,
	K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,K_F9,K_F10,
	K_PAUSE,0,K_HOME,K_UPARROW,K_PGUP,K_KP_MINUS,K_LEFTARROW,K_KP_5,
	K_RIGHTARROW,K_KP_PLUS,K_END,K_DOWNARROW,K_PGDN,K_INS,K_DEL,
	0,0,0,K_F11,K_F12,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

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

/*
===========
IN_StartupMouse
===========
*/
void IN_StartupMouse( void )
{
	cvar_t	*cv;

	cv = Cvar_Get( "in_initmouse", "1", CVAR_SYSTEMINFO, "allow mouse device" );
	if( !cv->value ) return; 

	in_mouse_buttons = 3;
	in_mouseparmsvalid = SystemParametersInfo( SPI_GETMOUSE, 0, in_originalmouseparms, 0 );
	in_mouseinitialized = true;
	in_mouse_wheel = RegisterWindowMessage( "MSWHEEL_ROLLMSG" );
}

static bool IN_CursorInRect( void )
{
	POINT	curpos;
	
	if( !in_mouseinitialized || !in_mouseactive )
		return false;

	// find mouse movement
	GetCursorPos( &curpos );

	if( curpos.x < real_rect.left + WND_BORDER )
		return false;
	if( curpos.x > real_rect.right - WND_BORDER * 3 )
		return false;
	if( curpos.y < real_rect.top + WND_HEADSIZE + WND_BORDER )
		return false;
	if( curpos.y > real_rect.bottom - WND_BORDER * 3 )
		return false;
	return true;
}


/*
===========
IN_ActivateMouse

Called when the window gains focus or changes in some way
===========
*/
void IN_ActivateMouse( void )
{
	int		width, height;
	static int	oldstate;
	POINT		global_pos;
	int		x, y;
			
	if( !in_mouseinitialized )
		return;

	if( cls.key_dest == key_menu && !scr_fullscreen->integer )
	{
		// check for mouse leave-entering
		if( !in_mouse_suspended && !UI_MouseInRect())
			in_mouse_suspended = true;

		if( oldstate != in_mouse_suspended )
		{
			if( in_mouse_suspended )
			{
				UI_GetCursorPos( &global_pos );
			
				ClipCursor( NULL );
				ReleaseCapture();
				while( ShowCursor( true ) < 0 );
				UI_ShowCursor( false );

				x = real_rect.left + global_pos.x;
				y = real_rect.top + global_pos.y + WND_HEADSIZE;

				// set system cursor position
				SetCursorPos( x, y );
			}
		}

		oldstate = in_mouse_suspended;

		if( in_mouse_suspended && IN_CursorInRect())
		{
			GetCursorPos( &global_pos );
			in_mouse_suspended = false;
			in_mouseactive = false; // re-initialize mouse

			x = global_pos.x - real_rect.left;
			y = global_pos.y - real_rect.top - WND_HEADSIZE;

			// set menu cursor position
			UI_SetCursorPos( x, y );
			UI_ShowCursor( true );
		}
	}

	if( in_mouseactive ) return;
	in_mouseactive = true;

	if( in_mouseparmsvalid )
		in_restore_spi = SystemParametersInfo( SPI_SETMOUSE, 0, in_newmouseparms, 0 );

	width = GetSystemMetrics( SM_CXSCREEN );
	height = GetSystemMetrics( SM_CYSCREEN );

	GetWindowRect( host.hWnd, &window_rect );
	if( window_rect.left < 0 ) window_rect.left = 0;
	if( window_rect.top < 0 ) window_rect.top = 0;
	if( window_rect.right >= width ) window_rect.right = width - 1;
	if( window_rect.bottom >= height - 1 ) window_rect.bottom = height - 1;

	window_center_x = (window_rect.right + window_rect.left) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;
	SetCursorPos( window_center_x, window_center_y );

	SetCapture( host.hWnd );
	ClipCursor( &window_rect );
	while( ShowCursor(false) >= 0 );
}

/*
===========
IN_DeactivateMouse

Called when the window loses focus
===========
*/
void IN_DeactivateMouse( void )
{
	if( !in_mouseinitialized || !in_mouseactive )
		return;

	if( in_restore_spi )
		SystemParametersInfo( SPI_SETMOUSE, 0, in_originalmouseparms, 0 );

	in_mouseactive = false;
	ClipCursor( NULL );
	ReleaseCapture();
	while( ShowCursor(true) < 0 );
}

/*
================
IN_Mouse
================
*/
void IN_MouseMove( void )
{
	POINT	current_pos;
	int	mx, my;
	
	if( !in_mouseinitialized || !in_mouseactive || in_mouse_suspended )
		return;

	// find mouse movement
	GetCursorPos( &current_pos );

	// force the mouse to the center, so there's room to move
	SetCursorPos( window_center_x, window_center_y );
	mx = current_pos.x - window_center_x;
	my = current_pos.y - window_center_y;

	if( !mx && !my ) return;
	Sys_QueEvent( 0, SE_MOUSE, mx, my, 0, NULL );
}

/*
===========
IN_MouseEvent
===========
*/
void IN_MouseEvent( int mstate )
{
	int	i;

	if( !in_mouseinitialized || !in_mouseactive )
		return;

	// perform button actions
	for( i = 0; i < in_mouse_buttons; i++ )
	{
		if((mstate & (1<<i)) && !(in_mouse_oldbuttonstate & (1<<i)) )
		{
			Sys_QueEvent( -1, SE_KEY, K_MOUSE1 + i, true, 0, NULL );
		}
		if(!(mstate & (1<<i)) && (in_mouse_oldbuttonstate & (1<<i)) )
		{
			Sys_QueEvent( -1, SE_KEY, K_MOUSE1 + i, false, 0, NULL );
		}
	}	
	in_mouse_oldbuttonstate = mstate;
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown( void )
{
	IN_DeactivateMouse();
}


/*
===========
IN_Init
===========
*/
void IN_Init( void )
{
	IN_StartupMouse();
}

/*
==================
IN_Frame

Called every frame, even if not generating commands
==================
*/
void IN_Frame( void )
{
	bool	shutdownMouse = false;

	if( host.type == HOST_DEDICATED )
	{
		// let the dedicated server some sleep
		Sys_Sleep( 5 );
	}
	else
	{
		if( host.state == HOST_NOFOCUS )
		{
			if( Host_ServerState() && CL_Active( ))
				Sys_Sleep( 5 ); // listenserver
			else Sys_Sleep( 20 ); // sleep 20 ms otherwise
		}
		else if( host.state == HOST_SLEEP )
		{
			// completely sleep in minimized state
			Sys_Sleep( 20 );
		}
	}

	if( !in_mouseinitialized )
		return;
	if( host.state != HOST_FRAME )
	{
		IN_DeactivateMouse();
		return;
	}

	if(( !cl.video_prepped || cl.refdef.paused ) && cls.key_dest == key_game )
		shutdownMouse = true; // release mouse during vid_restart and during pause
	
	if( shutdownMouse && !Cvar_VariableInteger( "fullscreen" ))
	{
		IN_DeactivateMouse();
		return;
	}

	IN_ActivateMouse();
	IN_MouseMove();

}

/*
====================
IN_WndProc

main window procedure
====================
*/
long IN_WndProc( void *hWnd, uint uMsg, uint wParam, long lParam )
{
	int	 temp = 0;

	if( uMsg == in_mouse_wheel )
		uMsg = WM_MOUSEWHEEL;

	switch( uMsg )
	{
	case WM_KILLFOCUS:
		if( scr_fullscreen && scr_fullscreen->integer )
			ShowWindow( host.hWnd, SW_SHOWMINNOACTIVE );
		break;
	case WM_MOUSEWHEEL:
		if((short)HIWORD(wParam) > 0 )
		{
			Sys_QueEvent( -1, SE_KEY, K_MWHEELUP, true, 0, NULL );
			Sys_QueEvent( -1, SE_KEY, K_MWHEELUP, false, 0, NULL );
		}
		else
		{
			Sys_QueEvent( -1, SE_KEY, K_MWHEELDOWN, true, 0, NULL );
			Sys_QueEvent( -1, SE_KEY, K_MWHEELDOWN, false, 0, NULL );
		}
		break;
	case WM_CREATE:
		host.hWnd = hWnd;
		scr_xpos = Cvar_Get( "r_xpos", "3", CVAR_ARCHIVE, "window position by horizontal" );
		scr_ypos = Cvar_Get( "r_ypos", "22", CVAR_ARCHIVE, "window position by vertical" );
		scr_fullscreen = Cvar_Get( "fullscreen", "0", CVAR_ARCHIVE|CVAR_LATCH, "set in 1 to enable fullscreen mode" );
		GetWindowRect( host.hWnd, &real_rect );
		break;
	case WM_CLOSE:
		Cbuf_ExecuteText( EXEC_APPEND, "quit" );
		break;
	case WM_ACTIVATE:
		if( HIWORD( wParam ))
			host.state = HOST_SLEEP;
		else if( LOWORD( wParam ) == WA_INACTIVE )
			host.state = HOST_NOFOCUS;
		else host.state = HOST_FRAME;

		wnd_caption = GetSystemMetrics( SM_CYCAPTION );
		S_Activate( (host.state == HOST_FRAME) ? true : false );
		Key_ClearStates();	// FIXME!!!

		if( host.state == HOST_FRAME )
		{
			SetForegroundWindow( hWnd );
			ShowWindow( hWnd, SW_RESTORE );
		}
		else if( scr_fullscreen->integer )
		{
			ShowWindow( hWnd, SW_MINIMIZE );
		}
		break;
	case WM_MOVE:
		if( !scr_fullscreen->integer )
		{
			RECT 		r;
			int		xPos, yPos, style;

			xPos = (short)LOWORD( lParam );    // horizontal position 
			yPos = (short)HIWORD( lParam );    // vertical position 

			r.left = r.top = 0;
			r.right = r.bottom = 1;
			style = GetWindowLong( hWnd, GWL_STYLE );
			AdjustWindowRect( &r, style, FALSE );

			Cvar_SetValue( "r_xpos", xPos + r.left );
			Cvar_SetValue( "r_ypos", yPos + r.top );
			scr_xpos->modified = false;
			scr_ypos->modified = false;
			GetWindowRect( host.hWnd, &real_rect );
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
		if( wParam == VK_RETURN )
		{
			// alt+enter fullscreen switch
			Cvar_SetValue( "fullscreen", !Cvar_VariableValue( "fullscreen" ));
			Cbuf_AddText( "vid_restart\n" );
			return 0;
		}
		// intentional fallthrough
	case WM_KEYDOWN:
		Sys_QueEvent( -1, SE_KEY, Host_MapKey( lParam ), true, 0, NULL );
		break;
	case WM_SYSKEYUP:
	case WM_KEYUP:
		Sys_QueEvent( -1, SE_KEY, Host_MapKey( lParam ), false, 0, NULL );
		break;
	case WM_CHAR:
		Sys_QueEvent( -1, SE_CHAR, wParam, 0, 0, NULL );
		break;
	}
	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}