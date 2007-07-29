//=======================================================================
//			Copyright XashXT Group 2007 ©
//			console.c - win and dos console
//=======================================================================

#include "launcher.h"

HINSTANCE	base_hInstance;

/*
===============================================================================

WIN32 CONSOLE

===============================================================================
*/

//console defines
#define SUBMIT_ID		1
#define QUIT_ID		2
#define CLEAR_ID		3
#define EDIT_ID		100
#define INPUT_ID		101
#define IDI_ICON1		101
#define SYSCONSOLE		"SystemConsole"

typedef struct
{
	HWND		hWnd;
	HWND		hwndBuffer;
	HWND		hwndButtonSubmit;
	HWND		hwndErrorBox;
	HWND		hwndErrorText;
	HBITMAP		hbmLogo;
	HBITMAP		hbmClearBitmap;
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
}WinConData;
static WinConData s_wcd;

void Sys_ShowConsoleW( bool show )
{
	if ( show == s_wcd.status ) return;
	s_wcd.status = show;
	if ( !s_wcd.hWnd ) return;

	if( show )
	{
		ShowWindow( s_wcd.hWnd, SW_SHOWNORMAL );
		SendMessage( s_wcd.hwndBuffer, EM_LINESCROLL, 0, 0xffff );
	}
	else ShowWindow( s_wcd.hWnd, SW_HIDE );
}

static LONG WINAPI ConWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool s_timePolarity;

	switch (uMsg)
	{
	case WM_ACTIVATE:
		if ( LOWORD( wParam ) != WA_INACTIVE ) SetFocus( s_wcd.hwndInputLine );
		break;
	case WM_CLOSE:
		if(sys_error)
		{
			//send windows message
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
			SendMessage(s_wcd.hwndInputLine,WM_CHAR, 13, 0L);
			SetFocus( s_wcd.hwndInputLine );
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

LONG WINAPI InputLineWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char inputBuffer[1024];

	switch ( uMsg )
	{
	case WM_KILLFOCUS:
		if ( ( HWND ) wParam == s_wcd.hWnd || ( HWND ) wParam == s_wcd.hwndErrorBox )
		{
			SetFocus( hWnd );
			return 0;
		}
		break;
	case WM_CHAR:
		if ( wParam == 13 )
		{
			GetWindowText( s_wcd.hwndInputLine, inputBuffer, sizeof( inputBuffer ) );
			strncat( s_wcd.consoleText, inputBuffer, sizeof( s_wcd.consoleText ) - strlen( s_wcd.consoleText ) - 5 );
			strcat( s_wcd.consoleText, "\n" );
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
Sys_PrintW

print into win32 console
================
*/
void Sys_PrintW(const char *pMsg)
{
	char buffer[MAX_INPUTLINE*2];
	char *b = buffer;
	const char *msg;
	int bufLen;
	int i = 0;
	static unsigned long s_totalChars;

	// if the message is REALLY long, use just the last portion of it
	if ( strlen( pMsg ) > MAX_INPUTLINE - 1 )
		msg = pMsg + strlen( pMsg ) - MAX_INPUTLINE + 1;
	else msg = pMsg;

	// copy into an intermediate buffer
	while ( msg[i] && ( ( b - buffer ) < sizeof( buffer ) - 1 ) )
	{
		if ( msg[i] == '\n' && msg[i+1] == '\r' )
		{
			b[0] = '\r';
			b[1] = '\n';
			b += 2;
			i++;
		}
		else if ( msg[i] == '\r' )
		{
			b[0] = '\r';
			b[1] = '\n';
			b += 2;
		}
		else if ( msg[i] == '\n' )
		{
			b[0] = '\r';
			b[1] = '\n';
			b += 2;
		}
		else if ( IsColorString( &msg[i] ) )
		{
			i++;
		}
		else
		{
			*b= msg[i];
			b++;
		}
		i++;
	}
	*b = 0;
	bufLen = b - buffer;
	s_totalChars += bufLen;

	// replace selection instead of appending if we're overflowing
	if ( s_totalChars > 0x7fff )
	{
		SendMessage( s_wcd.hwndBuffer, EM_SETSEL, 0, -1 );
		s_totalChars = bufLen;
	}

	// put this text into the windows console
	SendMessage( s_wcd.hwndBuffer, EM_LINESCROLL, 0, 0xffff );
	SendMessage( s_wcd.hwndBuffer, EM_SCROLLCARET, 0, 0 );
	SendMessage( s_wcd.hwndBuffer, EM_REPLACESEL, 0, (LPARAM) buffer );
}

/*
================
Sys_Msg

formatted message
================
*/
void Sys_MsgW( const char *pMsg, ... )
{
	va_list		argptr;
	char text[MAX_INPUTLINE];
	
	va_start (argptr, pMsg);
	vsprintf (text, pMsg, argptr);
	va_end (argptr);

	Sys_Print( text );
}

void Sys_MsgDevW( const char *pMsg, ... )
{
	va_list		argptr;
	char text[MAX_INPUTLINE];
	
	if(debug_mode)
	{
		va_start (argptr, pMsg);
		vsprintf (text, pMsg, argptr);
		va_end (argptr);
		Sys_Print( text );
	}
}

/*
================
Sys_CreateConsoleW

create win32 console
================
*/
void Sys_CreateConsoleW( void )
{
	HDC hDC;
	WNDCLASS wc;
	RECT rect;
	int nHeight;
	int swidth, sheight;
	int DEDSTYLE = WS_POPUPWINDOW | WS_CAPTION;// | WS_MINIMIZEBOX;
	int CONSTYLE = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | WS_EX_CLIENTEDGE | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY;

	memset( &wc, 0, sizeof( wc ) );

	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC) ConWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = base_hInstance;
	wc.hIcon         = LoadIcon( base_hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = (void *)COLOR_3DSHADOW;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = SYSCONSOLE;

	if (!RegisterClass (&wc) ) return;
	
	if(console_read_only)
	{
		rect.left = 0;
		rect.right = 536;
		rect.top = 0;
		rect.bottom = 364;
	}
	else
	{
		rect.left = 0;
		rect.right = 540;
		rect.top = 0;
		rect.bottom = 392;
	}
	AdjustWindowRect( &rect, DEDSTYLE, FALSE );

	hDC = GetDC( GetDesktopWindow() );
	swidth = GetDeviceCaps( hDC, HORZRES );
	sheight = GetDeviceCaps( hDC, VERTRES );
	ReleaseDC( GetDesktopWindow(), hDC );

	s_wcd.windowWidth = rect.right - rect.left;
	s_wcd.windowHeight = rect.bottom - rect.top;

	s_wcd.hWnd = CreateWindowEx( WS_EX_DLGMODALFRAME, SYSCONSOLE, "Xash Console", DEDSTYLE, ( swidth - 600 ) / 2, ( sheight - 450 ) / 2 , rect.right - rect.left + 1, rect.bottom - rect.top + 1, NULL, NULL, base_hInstance, NULL );

	if ( s_wcd.hWnd == NULL ) return;

	// create fonts
	hDC = GetDC( s_wcd.hWnd );
	nHeight = -MulDiv( 8, GetDeviceCaps( hDC, LOGPIXELSY), 72);
	s_wcd.hfBufferFont = CreateFont( nHeight, 0, 0, 0, FW_LIGHT, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_MODERN | FIXED_PITCH, "Fixedsys" );
	ReleaseDC( s_wcd.hWnd, hDC );

	if(!console_read_only)
	{
		// create the input line
		s_wcd.hwndInputLine = CreateWindowEx( WS_EX_CLIENTEDGE, "edit", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL, 0, 366, 450, 25, s_wcd.hWnd, ( HMENU ) INPUT_ID, base_hInstance, NULL );

		s_wcd.hwndButtonSubmit = CreateWindow( "button", NULL, BS_PUSHBUTTON | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 452, 367, 87, 25, s_wcd.hWnd, ( HMENU ) SUBMIT_ID, base_hInstance, NULL );
		SendMessage( s_wcd.hwndButtonSubmit, WM_SETTEXT, 0, ( LPARAM ) "submit" );
          }
          
	// create the scrollbuffer
	GetClientRect(s_wcd.hWnd, &rect);

	s_wcd.hwndBuffer = CreateWindowEx(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE, "edit", NULL, CONSTYLE, 0, 0, rect.right - rect.left, 365, s_wcd.hWnd, ( HMENU )EDIT_ID, base_hInstance, NULL );
	SendMessage( s_wcd.hwndBuffer, WM_SETFONT, ( WPARAM ) s_wcd.hfBufferFont, 0 );

	if(!console_read_only)
	{
		s_wcd.SysInputLineWndProc = ( WNDPROC ) SetWindowLong( s_wcd.hwndInputLine, GWL_WNDPROC, ( long ) InputLineWndProc );
		SendMessage( s_wcd.hwndInputLine, WM_SETFONT, ( WPARAM ) s_wcd.hfBufferFont, 0 );
          }

	//show console if needed
	if( show_always )
	{          
		//make console visible
		ShowWindow( s_wcd.hWnd, SW_SHOWDEFAULT);
		UpdateWindow( s_wcd.hWnd );
		SetForegroundWindow( s_wcd.hWnd );

		if(console_read_only) SetFocus( s_wcd.hWnd );
		else SetFocus( s_wcd.hwndInputLine );
		s_wcd.status = true;
          }
	else s_wcd.status = false;
}

/*
================
Sys_DestroyConsoleW

destroy win32 console
================
*/
void Sys_DestroyConsoleW( void )
{
	if ( s_wcd.hWnd )
	{

		DeleteObject(s_wcd.hbrEditBackground);
		DeleteObject( s_wcd.hbrErrorBackground);
                    DeleteObject( s_wcd.hfBufferFont );
		
		ShowWindow( s_wcd.hWnd, SW_HIDE );
		DestroyWindow( s_wcd.hWnd );
		s_wcd.hWnd = 0;
	}

	UnregisterClass (SYSCONSOLE, base_hInstance);
}

/*
================
Sys_InputW

returned input text 
================
*/
char *Sys_InputW( void )
{
	if ( s_wcd.consoleText[0] == 0 ) return NULL;
		
	strcpy( s_wcd.returnedText, s_wcd.consoleText );
	s_wcd.consoleText[0] = 0;
	
	return s_wcd.returnedText;
}

/*
================
Sys_ErrorW

NOTE: we must prepare engine to shutdown
before call this
================
*/
void Sys_ErrorW(char *error, ...)
{
	va_list		argptr;
	MSG		msg;
	char		text[MAX_INPUTLINE];
         
	if(sys_error) return; //don't multiple executes
	Sys_Print( "Error: " );
	
	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);
         
	sys_error = true;
	
	Sys_ShowConsole( true );
	Sys_Print( text ); //print error message

	ZeroMemory(&msg, sizeof(msg));
	SetFocus( s_wcd.hWnd );

	// wait for the user to quit
	while(msg.message != WM_QUIT)
	{
		if(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} 
		else Sleep( 20 );
	}
	Sys_Exit();
}