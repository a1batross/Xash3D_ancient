//=======================================================================
//			Copyright XashXT Group 2007 ©
//			input.c - win32 input devices
//=======================================================================

#include "common.h"
#include "client.h"

bool in_mouseactive;	// false when not focus app
bool in_restore_spi;
bool in_mouseinitialized;
int  in_originalmouseparms[3];
int  in_newmouseparms[3] = { 0, 0, 1 };
bool in_mouseparmsvalid;
int  in_mouse_buttons;
int  in_mouse_oldbuttonstate;
int  window_center_x, window_center_y;
RECT window_rect;
POINT cur_pos;

/*
===========
IN_StartupMouse
===========
*/
void IN_StartupMouse( void )
{
	cvar_t	*cv;

	cv = Cvar_Get( "in_initmouse", "1", CVAR_SYSTEMINFO, "allow mouse device" );
	if ( !cv->value ) return; 

	in_mouse_buttons = 3;
	in_mouseparmsvalid = SystemParametersInfo( SPI_GETMOUSE, 0, in_originalmouseparms, 0 );
	in_mouseinitialized = true;
}

/*
===========
IN_ActivateMouse

Called when the window gains focus or changes in some way
===========
*/
void IN_ActivateMouse( void )
{
	int width, height;

	if( !in_mouseinitialized )
		return;

	if( in_mouseactive ) return;
	in_mouseactive = true;

	if( in_mouseparmsvalid )
		in_restore_spi = SystemParametersInfo( SPI_SETMOUSE, 0, in_newmouseparms, 0 );

	width = GetSystemMetrics(SM_CXSCREEN);
	height = GetSystemMetrics(SM_CYSCREEN);

	GetWindowRect( host.hWnd, &window_rect);
	if (window_rect.left < 0) window_rect.left = 0;
	if (window_rect.top < 0) window_rect.top = 0;
	if (window_rect.right >= width) window_rect.right = width - 1;
	if (window_rect.bottom >= height-1) window_rect.bottom = height - 1;

	window_center_x = (window_rect.right + window_rect.left)/2;
	window_center_y = (window_rect.top + window_rect.bottom)/2;
	SetCursorPos( window_center_x, window_center_y );

	SetCapture( host.hWnd );
	ClipCursor(&window_rect);
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
	
	if( !in_mouseinitialized )
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

	if( !in_mouseinitialized )
		return;

	// perform button actions
	for( i = 0; i < in_mouse_buttons; i++ )
	{
		if((mstate & (1<<i)) && !(in_mouse_oldbuttonstate & (1<<i)) )
		{
			Sys_QueEvent( -1, SE_KEY, K_MOUSE1 + i, true, 0, NULL );
		}
		if ( !(mstate & (1<<i)) && (in_mouse_oldbuttonstate & (1<<i)) )
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
	if( !in_mouseinitialized )
		return;
	if( host.state != HOST_FRAME )
	{
		IN_DeactivateMouse();
		return;
	}

	// uimenu.dat using mouse
	if((!cl.refresh_prepped && cls.key_dest != key_menu) || cls.key_dest == key_console )
	{
		if(!Cvar_VariableValue( "fullscreen"))
		{
			IN_DeactivateMouse();
			return;
		}
	}

	IN_ActivateMouse();
	IN_MouseMove();

}