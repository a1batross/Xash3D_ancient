//=======================================================================
//			Copyright XashXT Group 2007 ©
//			console.c - win and dos console
//=======================================================================

#include "launch.h"

HINSTANCE	base_hInstance;

/*
===============================================================================

WIN32 CONSOLE

===============================================================================
*/

// console defines
#define SUBMIT_ID		1	// "submit" button
#define QUIT_ON_ESCAPE_ID	2	// escape event
#define EDIT_ID		110
#define INPUT_ID		109
#define IDI_ICON1		101
#define SYSCONSOLE		"SystemConsole"

typedef struct
{
	HWND		hWnd;
	HWND		hwndBuffer;
	HWND		hwndButtonSubmit;
	HWND		hwndErrorBox;
	HWND		hwndErrorText;
	HBRUSH		hbrEditBackground;
	HBRUSH		hbrErrorBackground;
	HFONT		hfBufferFont;
	HFONT		hfButtonFont;
	HWND		hwndInputLine;
	char		errorString[80];
	char		consoleText[512], returnedText[512];
	int		status;
	int		windowWidth, windowHeight;
	WNDPROC		SysInputLineWndProc;
	size_t		outLen;
} WinConData;
static WinConData s_wcd;

void Con_ShowConsole( bool show )
{
	if( !s_wcd.hWnd || Sys.hooked_out )
		return;
	if ( show == s_wcd.status )
		return;

	s_wcd.status = show;
	if( show )
	{
		ShowWindow( s_wcd.hWnd, SW_SHOWNORMAL );
		SendMessage( s_wcd.hwndBuffer, EM_LINESCROLL, 0, 0xffff );
	}
	else ShowWindow( s_wcd.hWnd, SW_HIDE );
}

static long _stdcall Con_WndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool	s_timePolarity;
	PAINTSTRUCT	ps;
	HDC		hdc;

	switch (uMsg)
	{
	case WM_ACTIVATE:
		if ( LOWORD( wParam ) != WA_INACTIVE ) SetFocus( s_wcd.hwndInputLine );
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, (LPPAINTSTRUCT)&ps);
		EndPaint(hWnd,(LPPAINTSTRUCT)&ps);
		return TRUE;
		break;
	case WM_CLOSE:
		if( Sys.app_state == SYS_ERROR )
		{
			// send windows message
			PostQuitMessage( 0 );
		}
		else Sys_Exit(); //otherwise
		return 0;
	case WM_CTLCOLORSTATIC:
		if (( HWND )lParam == s_wcd.hwndBuffer )
		{
			SetBkColor( ( HDC ) wParam, RGB( 0x90, 0x90, 0x90 ));
			SetTextColor( ( HDC ) wParam, RGB( 0xff, 0xff, 0xff ));
			return ( long ) s_wcd.hbrEditBackground;
		}
		else if (( HWND )lParam == s_wcd.hwndErrorBox )
		{
			if ( s_timePolarity & 1 )
			{
				SetBkColor(( HDC )wParam, RGB( 0x80, 0x80, 0x80 ));
				SetTextColor(( HDC )wParam, RGB( 0xff, 0x0, 0x00 ));
			}
			else
			{
				SetBkColor(( HDC )wParam, RGB( 0x80, 0x80, 0x80 ));
				SetTextColor(( HDC )wParam, RGB( 0x00, 0x0, 0x00 ));
			}
			return ( long )s_wcd.hbrErrorBackground;
		}
		break;
	case WM_COMMAND:
		if ( wParam == SUBMIT_ID )
		{
			SendMessage(s_wcd.hwndInputLine, WM_CHAR, 13, 0L);
			SetFocus( s_wcd.hwndInputLine );
		}
		break;
	case WM_HOTKEY:
		switch(LOWORD(wParam))
		{
		case QUIT_ON_ESCAPE_ID:
			PostQuitMessage( 0 );
			break;
		}
		break;
	case WM_CREATE:
		s_wcd.hbrEditBackground = CreateSolidBrush( RGB( 0x90, 0x90, 0x90 ) );
		s_wcd.hbrErrorBackground = CreateSolidBrush( RGB( 0x80, 0x80, 0x80 ) );
		SetTimer( hWnd, 1, 1000, NULL );
		break;
	case WM_ERASEBKGND:
		return DefWindowProc( hWnd, uMsg, wParam, lParam );
	case WM_TIMER:
		if ( wParam == 1 )
		{
			s_timePolarity = !s_timePolarity;
			if ( s_wcd.hwndErrorBox ) InvalidateRect( s_wcd.hwndErrorBox, NULL, FALSE );
		}
		break;
	}
	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

long _stdcall Con_InputLineProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char inputBuffer[1024];

	switch ( uMsg )
	{
	case WM_KILLFOCUS:
		if (( HWND ) wParam == s_wcd.hWnd || ( HWND ) wParam == s_wcd.hwndErrorBox )
		{
			SetFocus( hWnd );
			return 0;
		}
		break;
	case WM_CHAR:
		if ( wParam == 13 )
		{
			GetWindowText( s_wcd.hwndInputLine, inputBuffer, sizeof( inputBuffer ) );
			com_strncat( s_wcd.consoleText, inputBuffer, sizeof( s_wcd.consoleText ) - com_strlen( s_wcd.consoleText ) - 5 );
			com_strcat( s_wcd.consoleText, "\n" );
			SetWindowText( s_wcd.hwndInputLine, "" );
			Msg(">%s\n", inputBuffer );
			return 0;
		}
	}
	return CallWindowProc( s_wcd.SysInputLineWndProc, hWnd, uMsg, wParam, lParam );
}


/*
===============================================================================

WIN32 IO

===============================================================================
*/
/*
================
Con_PrintA

print into cmd32 console
================
*/
void Con_PrintA(const char *pMsg)
{
	DWORD	cbWritten;

	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), pMsg, com_strlen(pMsg), &cbWritten, 0 );
}

/*
================
Con_PrintW

print into window console
================
*/
void Con_PrintW( const char *pMsg )
{
	size_t	len = com.strlen( pMsg );

	// replace selection instead of appending if we're overflowing
	s_wcd.outLen += len;
	if( s_wcd.outLen >= 0x7fff )
	{
		SendMessage( s_wcd.hwndBuffer, EM_SETSEL, 0, -1 );
		s_wcd.outLen = len;
	} 

	SendMessage( s_wcd.hwndBuffer, EM_REPLACESEL, 0, (LPARAM)pMsg );

	// put this text into the windows console
	SendMessage( s_wcd.hwndBuffer, EM_LINESCROLL, 0, 0xffff );
	SendMessage( s_wcd.hwndBuffer, EM_SCROLLCARET, 0, 0 );
}


/*
================
Con_CreateConsole

create win32 console
================
*/
void Con_CreateConsole( void )
{
	HDC hDC;
	WNDCLASS wc;
	RECT rect;
	int	nHeight;
	int	swidth, sheight, fontsize;
	string	Title, FontName;
	int	DEDSTYLE = WS_POPUPWINDOW | WS_CAPTION;
	int	CONSTYLE = WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_BORDER|WS_EX_CLIENTEDGE|ES_LEFT|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY;

	if( Sys.con_silentmode ) return;
	Sys_InitLog();

	if( Sys.hooked_out ) 
	{
		// just init log
		Sys.Con_Print = Con_PrintA;
		return;
	}
	Sys.Con_Print = Con_PrintW;

	Mem_Set( &wc, 0, sizeof( wc ));
	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)Con_WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = Sys.hInstance;
	wc.hIcon         = LoadIcon( Sys.hInstance, MAKEINTRESOURCE( IDI_ICON1 ));
	wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (void *)COLOR_3DSHADOW;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = SYSCONSOLE;

	if(!RegisterClass( &wc ))
	{
		// print into log
		MsgDev( D_WARN, "Can't register window class '%s'\n", SYSCONSOLE );
		return;
	} 

	if( Sys.con_showcredits )
	{
		CONSTYLE &= ~WS_VSCROLL;
		rect.left = 0;
		rect.right = 536;
		rect.top = 0;
		rect.bottom = 280;
		com.strncpy( FontName, "Arial", MAX_STRING );
		fontsize = 16;
	}
	else if( Sys.con_readonly )
	{
		rect.left = 0;
		rect.right = 536;
		rect.top = 0;
		rect.bottom = 364;
		com.strncpy( FontName, "Fixedsys", MAX_STRING );
		fontsize = 8;
	}
	else // dedicated console
	{
		rect.left = 0;
		rect.right = 540;
		rect.top = 0;
		rect.bottom = 392;
		com.strncpy( FontName, "System", MAX_STRING );
		fontsize = 14;
	}

	com.strncpy( Title, Sys.caption, MAX_STRING );
	AdjustWindowRect( &rect, DEDSTYLE, FALSE );

	hDC = GetDC( GetDesktopWindow() );
	swidth = GetDeviceCaps( hDC, HORZRES );
	sheight = GetDeviceCaps( hDC, VERTRES );
	ReleaseDC( GetDesktopWindow(), hDC );

	s_wcd.windowWidth = rect.right - rect.left;
	s_wcd.windowHeight = rect.bottom - rect.top;

	s_wcd.hWnd = CreateWindowEx( WS_EX_DLGMODALFRAME, SYSCONSOLE, Title, DEDSTYLE, ( swidth - 600 ) / 2, ( sheight - 450 ) / 2 , rect.right - rect.left + 1, rect.bottom - rect.top + 1, NULL, NULL, base_hInstance, NULL );
	if ( s_wcd.hWnd == NULL )
	{
		Msg( "Can't create window '%s'\n", Title );
		return;
	}

	// create fonts
	hDC = GetDC( s_wcd.hWnd );
	nHeight = -MulDiv( fontsize, GetDeviceCaps( hDC, LOGPIXELSY), 72);
	s_wcd.hfBufferFont = CreateFont( nHeight, 0, 0, 0, FW_LIGHT, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_MODERN | FIXED_PITCH, FontName );
	ReleaseDC( s_wcd.hWnd, hDC );

	if( !Sys.con_readonly )
	{
		// create the input line
		s_wcd.hwndInputLine = CreateWindowEx( WS_EX_CLIENTEDGE, "edit", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL, 0, 366, 450, 25, s_wcd.hWnd, ( HMENU ) INPUT_ID, base_hInstance, NULL );

		s_wcd.hwndButtonSubmit = CreateWindow( "button", NULL, BS_PUSHBUTTON | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 452, 367, 87, 25, s_wcd.hWnd, ( HMENU ) SUBMIT_ID, base_hInstance, NULL );
		SendMessage( s_wcd.hwndButtonSubmit, WM_SETTEXT, 0, ( LPARAM ) "submit" );
          }
          
	// create the scrollbuffer
	GetClientRect( s_wcd.hWnd, &rect );

	s_wcd.hwndBuffer = CreateWindowEx( WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE, "edit", NULL, CONSTYLE, 0, 0, rect.right - rect.left, min(365, rect.bottom), s_wcd.hWnd, ( HMENU )EDIT_ID, base_hInstance, NULL );
	SendMessage( s_wcd.hwndBuffer, WM_SETFONT, ( WPARAM ) s_wcd.hfBufferFont, 0 );

	if(!Sys.con_readonly)
	{
		s_wcd.SysInputLineWndProc = ( WNDPROC )SetWindowLong( s_wcd.hwndInputLine, GWL_WNDPROC, ( long ) Con_InputLineProc );
		SendMessage( s_wcd.hwndInputLine, WM_SETFONT, ( WPARAM ) s_wcd.hfBufferFont, 0 );
          }

	// show console if needed
	if( Sys.con_showalways )
	{          
		// make console visible
		ShowWindow( s_wcd.hWnd, SW_SHOWDEFAULT);
		UpdateWindow( s_wcd.hWnd );
		SetForegroundWindow( s_wcd.hWnd );

		if(Sys.con_readonly) SetFocus( s_wcd.hWnd );
		else SetFocus( s_wcd.hwndInputLine );
		s_wcd.status = true;
          }
	else s_wcd.status = false;
}

/*
================
Con_DestroyConsole

destroy win32 console
================
*/
void Con_DestroyConsole( void )
{
	// last text message into console or log 
	MsgDev( D_NOTE, "Sys_FreeLibrary: Unloading launch.dll\n" );

	Sys_CloseLog();
	if( Sys.hooked_out ) return;

	if ( s_wcd.hWnd )
	{
		DeleteObject(s_wcd.hbrEditBackground);
		DeleteObject( s_wcd.hbrErrorBackground);
                    DeleteObject( s_wcd.hfBufferFont );
		
		ShowWindow( s_wcd.hWnd, SW_HIDE );
		DestroyWindow( s_wcd.hWnd );
		s_wcd.hWnd = 0;
	}
	UnregisterClass( SYSCONSOLE, Sys.hInstance );
}

/*
================
Sys_Input

returned input text 
================
*/
char *Sys_Input( void )
{
	if( s_wcd.consoleText[0] == 0 ) return NULL;
		
	com_strncpy( s_wcd.returnedText, s_wcd.consoleText, sizeof(s_wcd.returnedText));
	s_wcd.consoleText[0] = 0;
	
	return s_wcd.returnedText;
}

/*
================
Con_SetFocus

change focus to console hwnd 
================
*/
void Con_RegisterHotkeys( void )
{
	if(Sys.hooked_out) return;

	SetFocus( s_wcd.hWnd );

	// user can hit escape for quit
	RegisterHotKey( s_wcd.hWnd, QUIT_ON_ESCAPE_ID, 0, VK_ESCAPE );
}

/*
===============================================================================

SYSTEM LOG

===============================================================================
*/
void Sys_InitLog( void )
{
	const char	*mode;

	if( Sys.app_state == SYS_RESTART )
		mode = "a";
	else mode = "w";

	// create log if needed
	if( Sys.log_active && !Sys.con_silentmode )
	{
		Sys.logfile = fopen( Sys.log_path, mode );
		if(!Sys.logfile) MsgDev( D_ERROR, "Sys_InitLog: can't create log file %s\n", Sys.log_path );

		fprintf( Sys.logfile, "=======================================================================\n" );
		fprintf( Sys.logfile, "\t%s started at %s\n", Sys.caption, com_timestamp( TIME_FULL ));
		fprintf( Sys.logfile, "=======================================================================\n");
	}
}

void Sys_CloseLog( void )
{
	string	event_name;

	// continue logged
	switch( Sys.app_state )
	{
	case SYS_CRASH: com_strncpy( event_name, "crashed", MAX_STRING ); break;
	case SYS_ABORT: com_strncpy( event_name, "aborted by user", MAX_STRING ); break;
	case SYS_ERROR: com_strncpy( event_name, "stopped with error", MAX_STRING ); break;
	case SYS_RESTART: com_strncpy( event_name, "restarted", MAX_STRING ); break;
	default: com_strncpy( event_name, "stopped", MAX_STRING ); break;
	}

	if( Sys.logfile )
	{
		fprintf( Sys.logfile, "\n");
		fprintf( Sys.logfile, "=======================================================================");
		fprintf( Sys.logfile, "\n\t%s %s at %s\n", Sys.caption, event_name, com_timestamp(TIME_FULL));
		fprintf( Sys.logfile, "=======================================================================\n");
		if( Sys.app_state == SYS_RESTART ) fprintf( Sys.logfile, "\n" ); // just for tabulate

		fclose( Sys.logfile );
		Sys.logfile = NULL;
	}
}

void Sys_PrintLog( const char *pMsg )
{
	if( !Sys.logfile ) return;
	fprintf( Sys.logfile, pMsg );
}