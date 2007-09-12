//=======================================================================
//			Copyright XashXT Group 2007 ©
//			editor.c - game editor dll
//=======================================================================

#include "editor.h"

#define IDI_ICON1		101

#define MAIN_WND_WIDTH	640
#define MAIN_WND_HEIGHT	480
#define OPTS_WND_WIDTH	420
#define OPTS_WND_HEIGHT	380
#define C_PAGES		3

#define EDITOR_CONSOLE	"Xash Editor Console"
#define EDITOR_SETTINGS	"Xash Editor Settings"


static MSG msg;
static window_t g_MainWindow;
static window_t g_IdleWindow;
 
typedef struct tag_dlghdr
{ 
	HWND		hwndTab;       // tab control 
	HWND		hwndDisplay;   // current child dialog box 
	RECT		rcDisplay;     // display rectangle for the tab control 
	DLGTEMPLATE	*apRes[C_PAGES]; 
}DLGHDR; 

GUI_Form 			s_gui;
wnd_options_t		w_opts;	//window options
platform_exp_t		*pi;//platform utils 
static bool editor_init = false;
static char textbuffer[MAX_INPUTLINE];
dll_info_t platform_dll = { "platform.dll", NULL, "CreateAPI", NULL, NULL, false, PLATFORM_API_VERSION, sizeof(platform_exp_t) };

/*
=============================================================================

GUI Set Font

=============================================================================
*/

void GUI_AddToolTip( HWND hwnd, const char *text )
{
	TOOLINFO ti;

	memset (&ti, 0, sizeof (TOOLINFO));
	ti.cbSize = sizeof (TOOLINFO);
	ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	ti.uId = (UINT)hwnd;
	ti.lpszText = (LPTSTR) text;
	SendMessage (s_gui.hTips, TTM_ADDTOOL, 0, (LPARAM) &ti);
}

static void GUI_AddButton( HWND parent, const char *name, int x, int y, int id )
{
	HWND btn;
	btn = CreateWindow( "button", name, WS_CHILD | WS_VISIBLE, x, y, 78, 26,
		parent, (HMENU)id, s_gui.gHinst, NULL);
	GUI_ApplyFont( btn );
	GUI_AddToolTip( btn, "test" );
}

void GUI_DisplayTooltips( HWND hwnd, DWORD dParam )
{
	switch( dParam )
	{
	case IDB_CHOOSEFONT:
		Msg("display help\n");
		//MessageBox(hwnd, "Choose default font for editor", "Help", MB_OK|MB_ICONINFORMATION);
		break;
	}
}


// DoLockDlgRes - loads and locks a dialog box template resource. 
// Returns the address of the locked resource. 
// lpszResName - name of the resource 
 
DLGTEMPLATE *WINAPI DoLockDlgRes(LPCSTR lpszResName) 
{
	HGLOBAL hglb;
	HRSRC hrsrc = FindResource(GetModuleHandle("editor"), lpszResName, RT_DIALOG); 

	if(!hrsrc) Sys_Error("not found res\n");

	hglb = LoadResource(GetModuleHandle("editor"), hrsrc); 
	return (DLGTEMPLATE *)LockResource(hglb); 
} 

// OnChildDialogInit - Positions the child dialog box to fall 
// within the display area of the tab control. 
 
void WINAPI OnChildDialogInit(HWND hwndDlg) 
{
	HWND hwndParent = GetParent(hwndDlg); 
	DLGHDR *pHdr = (DLGHDR *) GetWindowLong( hwndParent, GWL_USERDATA); 
	SetWindowPos(hwndDlg, HWND_TOP, pHdr->rcDisplay.left, pHdr->rcDisplay.top, 0, 0, SWP_NOSIZE); 
	ShowWindow(hwndDlg, SW_SHOWNORMAL);
	GUI_AddToolTip(GetDlgItem(hwndDlg, IDB_CHOOSEFONT), "Choose font" );
} 

void GUI_UpdateOptions( WPARAM wParam );

static LRESULT CALLBACK ChildDialogProc(HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
	HELPINFO *hi;
	
	switch( uMessage )
	{
	case WM_INITDIALOG:
		OnChildDialogInit( hwnd );
		break;
		//return true;
	case WM_COMMAND:
		GUI_UpdateOptions(wParam);
		break;
	case WM_HELP:
		hi = (HELPINFO *)lParam;
		GUI_DisplayTooltips( hwnd, hi->iCtrlId );
		break;
	}
	return DefWindowProc (hwnd, uMessage, wParam, lParam);
}


// OnSelChanged - processes the TCN_SELCHANGE notification. 
// hwndDlg - handle to the parent dialog box. 
void WINAPI OnSelChanged(HWND hwndDlg) 
{
	DLGHDR *pHdr = (DLGHDR *) GetWindowLong( hwndDlg, GWL_USERDATA); 
	int iSel = TabCtrl_GetCurSel(pHdr->hwndTab); 
 
	// Destroy the current child dialog box, if any. 
	if (pHdr->hwndDisplay != NULL) DestroyWindow(pHdr->hwndDisplay); 
 
	// Create the new child dialog box. 
	pHdr->hwndDisplay = CreateDialogIndirect(s_gui.gHinst, pHdr->apRes[iSel], hwndDlg, ChildDialogProc); 
} 

void WINAPI OnTabbedDialogInit( HWND hwndDlg ) 
{
	DLGHDR *pHdr = (DLGHDR *)LocalAlloc(LPTR, sizeof(DLGHDR)); 
	DWORD dwDlgBase = GetDialogBaseUnits(); 
	int cxMargin = LOWORD(dwDlgBase) / 4; 
	int cyMargin = HIWORD(dwDlgBase) / 8; 
	TCITEM tie; 
	RECT rcTab; 
	int i; 
 
	// Save a pointer to the DLGHDR structure. 
	SetWindowLong(hwndDlg, GWL_USERDATA, (LONG) pHdr); 
 
	// Create the tab control. 
	pHdr->hwndTab = CreateWindow( WC_TABCONTROL, "", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
		0, 0, OPTS_WND_WIDTH, OPTS_WND_HEIGHT, hwndDlg, NULL, s_gui.gHinst, NULL ); 
	if (pHdr->hwndTab == NULL)
	{
		// handle error
		GUI_Error("can't create hwnd dialog\n" );
		return;
	}

	GUI_ApplyFont( pHdr->hwndTab );
 
	// Add a tab for each of the three child dialog boxes. 
	tie.mask = TCIF_TEXT | TCIF_IMAGE; 
	tie.iImage = -1; 
	tie.pszText = "General"; 
	TabCtrl_InsertItem(pHdr->hwndTab, 0, &tie); 
	tie.pszText = "Compilers"; 
	TabCtrl_InsertItem(pHdr->hwndTab, 1, &tie); 
	tie.pszText = "QuakeC"; 
	TabCtrl_InsertItem(pHdr->hwndTab, 2, &tie); 
 
 	// Lock the resources for the three child dialog boxes. 
	pHdr->apRes[0] = DoLockDlgRes(MAKEINTRESOURCE(DLG_FIRST)); 
	pHdr->apRes[1] = DoLockDlgRes(MAKEINTRESOURCE(DLG_SECOND)); 
	pHdr->apRes[2] = DoLockDlgRes(MAKEINTRESOURCE(DLG_THIRD)); 
 
	// Determine the bounding rectangle for all child dialog boxes. 
	SetRectEmpty(&rcTab); 
    
	for (i = 0; i < C_PAGES; i++)
	{ 
		if (pHdr->apRes[i]->cx > rcTab.right) 
			rcTab.right = pHdr->apRes[i]->cx; 
		if (pHdr->apRes[i]->cy > rcTab.bottom)
			rcTab.bottom = pHdr->apRes[i]->cy; 
	} 
	
	rcTab.right = rcTab.right * LOWORD(dwDlgBase) / 4; 
	rcTab.bottom = rcTab.bottom * HIWORD(dwDlgBase) / 8; 
 
	// Calculate how large to make the tab control, so 
	// the display area can accommodate all the child dialog boxes. 
	TabCtrl_AdjustRect(pHdr->hwndTab, TRUE, &rcTab); 
	OffsetRect(&rcTab, cxMargin - rcTab.left, cyMargin - rcTab.top); 
 
	// Calculate the display rectangle. 
	CopyRect(&pHdr->rcDisplay, &rcTab); 
	TabCtrl_AdjustRect(pHdr->hwndTab, FALSE, &pHdr->rcDisplay); 
 
	// Set the size and position of the tab control, buttons, 
	// and dialog box. 
	SetWindowPos(pHdr->hwndTab, NULL, rcTab.left, rcTab.top, OPTS_WND_WIDTH + 10, OPTS_WND_HEIGHT, SWP_NOZORDER); 
 
	// Size the dialog box. 
	SetWindowPos(hwndDlg, NULL, 0, 0, OPTS_WND_WIDTH + 10, OPTS_WND_HEIGHT, SWP_NOMOVE | SWP_NOZORDER); 

	s_gui.hTips = CreateWindowEx (0, TOOLTIPS_CLASS, "", WS_POPUP | WS_EX_TOPMOST, 0, 0, 0, 0, pHdr->hwndTab, NULL, s_gui.gHinst, NULL);

 
	// Simulate selection of the first item. 
	OnSelChanged( hwndDlg ); 
} 

/*
=============================================================================

GUI Table Options

=============================================================================
*/
void GUI_ResizeTab(HWND hwnd)
{
	TC_ITEM ti;

	int index = TabCtrl_GetCurSel (hwnd);
	
	if( index >= 0 )
	{
		ti.mask = TCIF_PARAM;
		TabCtrl_GetItem (hwnd, index, &ti);
		if (s_gui.hOptions)
		{
			RECT rc, rc2;
                              HDWP hdwp;
          		
			GetWindowRect (hwnd, &rc);
			ScreenToClient (s_gui.hTabs, (LPPOINT) &rc.left);
			ScreenToClient (s_gui.hTabs, (LPPOINT) &rc.right);

			TabCtrl_GetItemRect (hwnd, index, &rc2);
			rc.top += (rc2.bottom - rc2.top) - 6;

			hdwp = BeginDeferWindowPos (2);
			DeferWindowPos (hdwp, s_gui.hTabs, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
			DeferWindowPos (hdwp, s_gui.hTabs, HWND_TOP, rc.left + 3, rc.top - 8, rc.right - rc.left, rc.bottom - rc.top, SWP_SHOWWINDOW);
			EndDeferWindowPos (hdwp);
		}
	}
}

static void GUI_AddTab( const char *name )
{
	TC_ITEM ti;

	ti.mask = TCIF_TEXT | TCIF_PARAM;
	ti.pszText = (LPSTR)name;

	TabCtrl_InsertItem (s_gui.hTabs, TabCtrl_GetItemCount(s_gui.hTabs), &ti);
	GUI_ResizeTab(s_gui.hTabs);
}

bool GUI_CloseOptions( void )
{
	ShowWindow( s_gui.hOptions, SW_HIDE );

	//return to main window
	EnableWindow( s_gui.hWnd, true );
	SetFocus( s_gui.hWnd );

	return false;
}

bool GUI_ApplyOptions( void )
{
	//TODO:
	//1. copy parameters from temporary struct to global
	//2. apply some changes immediately if needed
	//3. save settings into "bin/editor.dat"
	
	
	return GUI_CloseOptions();
}

void GUI_UpdateOptions( WPARAM wParam )
{
	CHOOSEFONT cf;
	static LOGFONT lf;        // logical font structure
	static DWORD rgbCurrent;   // current text color

	ZeroMemory(&cf, sizeof(cf));
	cf.lStructSize = sizeof (cf);
	cf.hwndOwner = s_gui.hWnd;
	cf.lpLogFont = &lf;
	cf.rgbColors = rgbCurrent;
	cf.Flags = CF_SCREENFONTS | CF_EFFECTS;

	switch(LOWORD(wParam))
	{
	case IDOK:
		GUI_ApplyOptions();
		break;
	case IDCANCEL:
		GUI_CloseOptions();
		break;
	case IDB_CHOOSEFONT:
		if(ChooseFont(&cf))
		{
			Msg("apply font %s\n", cf.lpLogFont->lfFaceName);
			strcpy(w_opts.fontname, cf.lpLogFont->lfFaceName);
			//Msg("apply font %d\n", cf.lpLogFont->lfHeight -MulDiv(cf.iPointSize, GetDeviceCaps(hDC, LOGPIXELSY), 72));		
		}
		break;
	}
}

static LRESULT CALLBACK OptWndProc (HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;
	
	switch( uMessage )
	{
	case WM_CREATE:
		OnTabbedDialogInit( hwnd );
		break;
	case WM_PAINT:
		hdc = BeginPaint(hwnd, (LPPAINTSTRUCT)&ps);
		EndPaint(hwnd,(LPPAINTSTRUCT)&ps);
		return TRUE;
		break;
	case WM_COMMAND:
		GUI_UpdateOptions(wParam);
		break;
	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code)
		{
		case TCN_SELCHANGE:
			OnSelChanged( hwnd );
			break;
		}
		break;
		return true;
	case WM_SIZE:
		break;
	case WM_CLOSE:
		return GUI_CloseOptions();
		break;
	}
	return DefWindowProc (hwnd, uMessage, wParam, lParam);
}

void GUI_CreateOptionsWindow( void )
{
	WNDCLASS wc;
	RECT rect;

	int w_pos, h_pos;
	//int WNDSTYLE = WS_POPUP | WS_BORDER | WS_CAPTION | WS_MINIMIZEBOX;
	int WNDSTYLE = WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION;
	int TABSTYLE = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_TOOLTIPS;
          int bpos_x, bpos_y;
	
	memset( &wc, 0, sizeof( wc ));
	
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = OptWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = s_gui.gHinst;
	wc.hIcon = LoadIcon(NULL, NULL);
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) COLOR_3DSHADOW;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = EDITOR_SETTINGS;

	if (!RegisterClass (&wc)) GUI_Error("Can't create options window\n");

	//move window into center of screen
	rect.left = 0;
	rect.right = OPTS_WND_WIDTH;
	rect.top = 0;
	rect.bottom = OPTS_WND_HEIGHT;

	AdjustWindowRect( &rect, WNDSTYLE, FALSE );

          w_pos = ( s_gui.scr_width - OPTS_WND_WIDTH ) / 2;
          h_pos = ( s_gui.scr_height - OPTS_WND_HEIGHT ) / 2;

	s_gui.hOptions = CreateWindowEx( WS_EX_CLIENTEDGE | WS_EX_CONTEXTHELP, EDITOR_SETTINGS, "Settings", WNDSTYLE, 
		w_pos, h_pos, rect.right - rect.left, rect.bottom - rect.top, s_gui.hWnd, NULL, s_gui.gHinst, NULL);

	GetClientRect(s_gui.hOptions, &rect);

	bpos_x = rect.right - rect.left - 78 - 5;
	bpos_y = rect.bottom - rect.top - 26 - 5;
	
	GUI_AddButton( s_gui.hOptions, "&Cancel", bpos_x, bpos_y, IDCANCEL );
	bpos_x -= 78 + 5;//move next button
	GUI_AddButton( s_gui.hOptions, "&OK", bpos_x, bpos_y, IDOK );
}

/*
=============================================================================

GUI Acellerators

=============================================================================
*/


void GUI_ResetWndOptions( void )
{
	char	dev_level[4];

	//get info about debug mode
	if(CheckParm ("-debug")) debug_mode = true;	
	if(GetParmFromCmdLine("-dev", dev_level ))
		dev_mode = atoi(dev_level);

	s_gui.gHinst = (HINSTANCE) GetModuleHandle( NULL );
	
	//reset options
	w_opts.id = IDEDITORHEADER;
	w_opts.version = (int)EDITOR_VERSION;
	w_opts.csize = sizeof(wnd_options_t);
	w_opts.show_console = true;
	w_opts.con_scale = 6L;
	w_opts.exp_scale = 5L;

	w_opts.font_size = 7;
	strcpy(w_opts.fontname, "Courier");
	w_opts.font_type = CFM_BOLD | CFM_FACE | CFM_COLOR;
	w_opts.font_color = RGB(0, 0, 0);

	w_opts.width = s_gui.width;
	w_opts.height = s_gui.height;
}

void GUI_LoadWndOptions( wnd_options_t *settings )
{
	memcpy( &w_opts, settings, sizeof(w_opts));

	s_gui.width = w_opts.width;
	s_gui.height = w_opts.height;
}

bool GUI_LoadPlatfrom( char *funcname, int argc, char **argv )
{
	stdinout_api_t	pistd;//platform callback
	platform_t	CreatePlat;

	//create callbacks for platform.dll
	pistd.printf = GUI_Msg;
	pistd.dprintf = GUI_MsgDev;
	pistd.wprintf = GUI_MsgWarn;
	pistd.error = GUI_Error;
	pistd.exit = std.exit;
	pistd.print = GUI_Print;
	pistd.input = std.input;
	pistd.sleep = std.sleep;

	pistd.LoadLibrary = std.LoadLibrary;
	pistd.FreeLibrary = std.FreeLibrary;

	//loading platform.dll
	if (!Sys_LoadLibrary( &platform_dll ))
	{
		GUI_Error("couldn't find platform.dll\n");
		return false;	
	}
	CreatePlat = (void *)platform_dll.main;
	pi = CreatePlat( &pistd );//make links
	
	//initialziing platform.dll
	pi->Init( argc, argv );

	pi->Fs.ClearSearchPath();
	pi->AddGameHierarchy( "bin" );

	return true;
}

HWND GUI_CreateConsole( bool readonly )
{
	HWND newwnd;
	DWORD dwStyle = WS_CHILD | WS_HSCROLL |
		WS_VSCROLL | ES_LEFT | ES_WANTRETURN | ES_MULTILINE | ES_AUTOVSCROLL;

	if (readonly) dwStyle |= ES_READONLY;
	if (!s_gui.richedit) s_gui.richedit = LoadLibrary("riched32.dll");

	newwnd = CreateWindowEx(WS_EX_CLIENTEDGE, s_gui.richedit ? RICHEDIT_CLASS:"EDIT", "",
		dwStyle, 0, 0, 0, 0, s_gui.hWnd, NULL, s_gui.gHinst, NULL);

	if (!newwnd)
	{
		//fall back to the earlier version
		newwnd = CreateWindowEx(WS_EX_CLIENTEDGE, s_gui.richedit ? RICHEDIT_CLASS10A : "EDIT", "",
		dwStyle, 0, 0, 0, 0, s_gui.hWnd, NULL, s_gui.gHinst, NULL);

	}
	if (!newwnd)
	{
		//we don't have RICHEDIT installed properly
		FreeLibrary(s_gui.richedit);
		s_gui.richedit = NULL;

		newwnd = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", dwStyle, 0, 0, 0, 0,
		s_gui.hWnd, NULL, s_gui.gHinst, NULL);
	}

	GUI_SetFont( newwnd );
	if (s_gui.richedit) SendMessage(newwnd, EM_EXLIMITTEXT, 0, 1<<20);

	return newwnd;
}


void GUI_CreateEditorWindow( void )
{
	s_gui.hWnd = CreateWindowEx( WS_EX_CLIENTEDGE, CLASSNAME, "Xash Resource Editor", WS_OVERLAPPEDWINDOW, 
		s_gui.top, s_gui.bottom, s_gui.width, s_gui.height, NULL, NULL, s_gui.gHinst, NULL);

	s_gui.hConsole = GUI_CreateConsole( true );
}


/*
=============================================================================

GUI Console System

=============================================================================
*/
void GUI_PrintIntoBuffer(const char *pMsg)
{
	strcat(textbuffer, pMsg );
}

void GUI_ExecuteBuffer(void)
{
	editor_init = true;
	GUI_Print(textbuffer); 
}

/*
====================
GUI_Print

stdout into editor internal console
====================
*/
void GUI_Print(const char *pMsg)
{
	CHARFORMAT cf;	
	char buffer[MAX_INPUTLINE*2];
	char *b = buffer;
	const char *msg;
	int bufLen;
	int i = 0;
	int color = RGB(0, 0, 0);
	static unsigned long s_totalChars;

	if(!editor_init)
	{
		GUI_PrintIntoBuffer( pMsg );
		return;
	}

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
		else if ( IsColorString( &msg[i] ))
		{
			int code = 0;
			i++;
			code = atoi( &msg[i] );
			if(code == 5) color = RGB(255, 0, 0);
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

	//Edit_SetSel(s_gui.hConsole, 0, s_totalChars);
	memset(&cf, 0, sizeof(cf));
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_COLOR;
	cf.crTextColor = color;

	// replace selection instead of appending if we're overflowing
	if ( s_totalChars > 0x7fff )
	{
		SendMessage( s_gui.hConsole, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
		//SendMessage( s_gui.hConsole, EM_SETSEL, 0, -1 );
		s_totalChars = bufLen;
		//Edit_SetSel(s_gui.hConsole, bufLen, s_totalChars);
	}

	// put this text into the windows console
	SendMessage( s_gui.hConsole, EM_LINESCROLL, 0, 0xffff );
	SendMessage( s_gui.hConsole, EM_SCROLLCARET, 0, 0 );
	SendMessage( s_gui.hConsole, EM_REPLACESEL, 0, (LPARAM) buffer );
}

/*
================
GUI_Msg

formatted message
================
*/
void GUI_Msg( const char *pMsg, ... )
{
	va_list		argptr;
	char text[MAX_INPUTLINE];
	
	va_start (argptr, pMsg);
	vsprintf (text, pMsg, argptr);
	va_end (argptr);

	GUI_Print( text );

	//echo into system console
	std.print( text );
}

void GUI_MsgDev( int level, const char *pMsg, ... )
{
	va_list		argptr;
	char text[MAX_INPUTLINE];
	
	if(dev_mode >= level)
	{
		va_start (argptr, pMsg);
		vsprintf (text, pMsg, argptr);
		va_end (argptr);
		GUI_Print( text );

		//echo into system console
		std.print( text );
	}
}

void GUI_MsgWarn( const char *pMsg, ... )
{
	va_list		argptr;
	char text[MAX_INPUTLINE];
	
	if(debug_mode)
	{
		va_start (argptr, pMsg);
		vsprintf (text, pMsg, argptr);
		va_end (argptr);
		GUI_Print( text );

		//echo into system console
		std.print( text );
	}
}

void GUI_Error( const char *pMsg, ... )
{
	va_list		argptr;
	char text[MAX_INPUTLINE];
	
	va_start (argptr, pMsg);
	vsprintf (text, pMsg, argptr);
	va_end (argptr);
	
	GUI_DisableMenus();
	GUI_Print( text );
	std.print( text );//echo into system console
	
	//3. waiting for user input
	
	//std.exit();
}

void GUI_CreateMenus( void )
{
	int TREESTYLE = WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES;
	
	//create root menu
	s_gui.menu = CreateMenu();

	//create volumes
	s_gui.file = CreateMenu(); AppendMenu(s_gui.menu, MF_POPUP, (UINT)s_gui.file, "&File");
	s_gui.edit = CreateMenu(); AppendMenu(s_gui.menu, MF_POPUP, (UINT)s_gui.edit, "&Edit");
	s_gui.cmds = CreateMenu(); AppendMenu(s_gui.menu, MF_POPUP, (UINT)s_gui.cmds, "&Tools");
	s_gui.help = CreateMenu(); AppendMenu(s_gui.menu, MF_POPUP, (UINT)s_gui.help, "&Help");

	//build menu "file"
	GUI_AddMenuItem(s_gui.file, "&New...	Ctrl + N", IDM_CREATE, VK_N );
	GUI_AddMenuItem(s_gui.file, "&Open...	Ctrl + O", IDM_OPEN, VK_O );
	GUI_AddMenuItem(s_gui.file, "&Close", IDM_CLOSE, 0 );
	GUI_AddMenuItem(s_gui.file, "", 0, 0 );//separator
	GUI_AddMenuItem(s_gui.file, "&Save...	Ctrl + S", IDM_SAVE, VK_S );
	GUI_AddMenuItem(s_gui.file, "Save &As...", IDM_SAVEAS, 0 );
	GUI_AddMenuItem(s_gui.file, "", 0, 0 );//separator		
	GUI_AddMenuItem(s_gui.file, "E&xit", IDM_QUIT, 0 );

	//build menu "edit"
	GUI_AddMenuItem(s_gui.edit, "&Undo	Ctrl + Z", IDM_UNDO, VK_Z );
	GUI_AddMenuItem(s_gui.edit, "&Redo	Ctrl + Y", IDM_REDO, VK_Y );
	GUI_AddMenuItem(s_gui.edit, "", 0, 0 );//separator	
	GUI_AddMenuItem(s_gui.edit, "Cu&t	Ctrl + X", IDM_CUT, VK_X );
	GUI_AddMenuItem(s_gui.edit, "&Copy	Ctrl + C", IDM_COPY, VK_C );
	GUI_AddMenuItem(s_gui.edit, "&Paste	Ctrl + V", IDM_PASTE, VK_V );
	GUI_AddMenuItem(s_gui.edit, "&Delete	Del", IDM_DELETE, VK_DELETE );
	GUI_AddMenuItem(s_gui.edit, "", 0, 0 );//separator
	GUI_AddMenuItem(s_gui.edit, "&Find	Ctrl + F", IDM_FIND, VK_F );
	GUI_AddMenuItem(s_gui.edit, "R&eplace	Ctrl + H", IDM_REPLACE, VK_H );			
	GUI_AddMenuItem(s_gui.edit, "&Go to...	Ctrl + G", IDM_GOTO, VK_G );
	
	//build menu "tools"
	GUI_AddMenuItem(s_gui.cmds, "&Compile...	F7", IDM_COMPILE, VK_F7 );
	GUI_AddMenuItem(s_gui.cmds, "D&ump Info...	F3", IDM_GETINFO, VK_F3 );
	GUI_AddMenuItem(s_gui.cmds, "", 0, 0 );//separator
	GUI_AddMenuItem(s_gui.cmds, "&Settings...	F10", IDM_SETTINGS, VK_F10 );

	AppendMenu(s_gui.cmds, MF_CHECKED, IDM_SHOWCONSOLE, "Show console");          
	RegisterHotKey(s_gui.hWnd, IDH_HIDECONSOLE, 0, VK_ESCAPE );
	
	//fill menu "help"
	AppendMenu(s_gui.help, 0, IDM_ABOUT,	"&About");
	
	SetMenu(s_gui.hWnd, s_gui.menu);

	s_gui.hTree = CreateWindowEx (WS_EX_CLIENTEDGE, WC_TREEVIEW, "", TREESTYLE,
		0, 0, 127, 380, s_gui.hWnd, (HMENU)IDM_FIRSTCHILD, s_gui.gHinst, NULL);

	GUI_CreateAccelTable();
	ShowWindow(s_gui.hTree, SW_SHOWDEFAULT);
}

void GUI_DisableMenus( void )
{
	EnableMenuItem(s_gui.file, IDM_CREATE, MF_GRAYED);
	EnableMenuItem(s_gui.file, IDM_OPEN, MF_GRAYED);
	EnableMenuItem(s_gui.file, IDM_CLOSE, MF_GRAYED);
	EnableMenuItem(s_gui.file, IDM_SAVE, MF_GRAYED);
	EnableMenuItem(s_gui.file, IDM_SAVEAS, MF_GRAYED);

	EnableMenuItem(s_gui.edit, IDM_UNDO, MF_GRAYED);
	EnableMenuItem(s_gui.edit, IDM_REDO, MF_GRAYED);
	EnableMenuItem(s_gui.edit, IDM_CUT, MF_GRAYED);
	EnableMenuItem(s_gui.edit, IDM_COPY, MF_GRAYED);
	EnableMenuItem(s_gui.edit, IDM_PASTE, MF_GRAYED);	
	EnableMenuItem(s_gui.edit, IDM_DELETE, MF_GRAYED);
	EnableMenuItem(s_gui.edit, IDM_FIND, MF_GRAYED);
	EnableMenuItem(s_gui.edit, IDM_REPLACE, MF_GRAYED);
	EnableMenuItem(s_gui.edit, IDM_GOTO, MF_GRAYED);

	EnableMenuItem(s_gui.cmds, IDM_COMPILE, MF_GRAYED);
	EnableMenuItem(s_gui.cmds, IDM_GETINFO, MF_GRAYED);

	EnableMenuItem(s_gui.cmds, IDM_SETTINGS, MF_GRAYED);
}

void GUI_ShowConsole( void )
{
	w_opts.show_console = true;
	ShowWindow( s_gui.hConsole, SW_SHOWNORMAL );
	SendMessage( s_gui.hConsole, EM_LINESCROLL, 0, 0xffff );
	CheckMenuItem(s_gui.cmds, IDM_SHOWCONSOLE, MF_CHECKED);
	SendMessage( s_gui.hWnd, WM_SIZE, 1, 0 );
}

void GUI_HideConsole( void )
{
	w_opts.show_console = false;
	ShowWindow(s_gui.hConsole, SW_HIDE);
	CheckMenuItem(s_gui.cmds, IDM_SHOWCONSOLE, MF_UNCHECKED);
	SendMessage( s_gui.hWnd, WM_SIZE, 1, 0 );//update windows
}

void GUI_UpdateDefault( WPARAM wParam )
{
	switch(LOWORD(wParam))
	{
	case IDM_DELETE:
		Msg("delete accel\n");
		break;
	case IDM_COMPILE:
		break;
	case IDM_GETINFO:
		break;
	case IDM_SETTINGS:
		ShowWindow( s_gui.hOptions, SW_SHOWNORMAL );
		SetFocus( s_gui.hOptions );
		EnableWindow( s_gui.hWnd, false );
		break;
	case IDM_SHOWCONSOLE:
		if(w_opts.show_console) GUI_HideConsole(); 
		else GUI_ShowConsole();
		break;
	case IDM_ABOUT:
		if(!w_opts.show_console) GUI_ShowConsole();
		Msg("Xash Resource Editor. Ver %g\n", EDITOR_VERSION );
		Msg("Copyright XashXT Group 2007 ©.\n");		
		break;
	}
}

void GUI_UpdateMenu( WPARAM wParam )
{
	switch(LOWORD(wParam))
	{
	case IDM_CREATE:
		Msg("Create new file\n");
		break;
	case IDM_OPEN:
		Msg("open file\n");
		break;
	case IDM_CLOSE:
		break;
	case IDM_SAVE:
		break;
	case IDM_SAVEAS:
		break;
	case IDM_QUIT:
		PostQuitMessage (0);
		break;
	default:
		GUI_UpdateDefault( wParam );
		break;
	}
}

void GUI_HotKeys( WPARAM wParam )
{
	switch(LOWORD(wParam))
	{
	case IDH_HIDECONSOLE:
		if(w_opts.show_console)
			GUI_HideConsole(); 
		break;
	default:
		MsgWarn("GUI_HotKeys: call unused hotkey %d\n", LOWORD(wParam));
		break; 
	}
}

static LRESULT CALLBACK WndProc (HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;
	
	switch (uMessage)
	{
	case WM_CREATE:
		break;
	case WM_PAINT:
		hdc = BeginPaint(hwnd, (LPPAINTSTRUCT)&ps);
		EndPaint(hwnd,(LPPAINTSTRUCT)&ps);
		return TRUE;
		break;
	case WM_HOTKEY:
		GUI_HotKeys( wParam );
		break;
	case WM_CHILDACTIVATE:
		Msg("process child\n");
		break;
	case WM_SIZE:
		if(s_gui.hWnd)
		{
			int con_w, con_h, con_y;
			int exp_w, exp_h, coffs;
			RECT rect;
			GetWindowRect(s_gui.hWnd, &rect);
			w_opts.width = rect.right - rect.left;
			w_opts.height = rect.bottom - rect.top;
			GetClientRect(s_gui.hWnd, &rect);

			con_h = (rect.bottom - rect.top) / w_opts.con_scale;	// console scale factor
			con_y = rect.bottom - rect.top - con_h;			// console hight
			con_w = rect.right - rect.left;

			if(s_gui.hConsole && IsWindowVisible(s_gui.hConsole)) coffs = con_h;
                              else coffs = 0;

			exp_w = (rect.right - rect.left) / w_opts.exp_scale;	// explorer scale factor
			exp_h = rect.bottom - rect.top - coffs;			// explorer height
			
			//make me sure what handle is valid
			if(s_gui.hConsole) SetWindowPos(s_gui.hConsole, NULL, 0, con_y, con_w, con_h, 0);
			if(s_gui.hTree) SetWindowPos(s_gui.hTree, NULL, 0, 0, exp_w, exp_h, 0);
		}
		break;
	case WM_COMMAND:
		GUI_UpdateMenu(wParam);
		break;
	case WM_CLOSE:
		if (hwnd == s_gui.hWnd) PostQuitMessage (0);
		else ShowWindow (hwnd, SW_HIDE);
		return 0;
		break;
	}
	
	return DefWindowProc (hwnd, uMessage, wParam, lParam);
}

void InitEditor ( char *funcname, int argc, char **argv )
{
	HDC hDC;
	WNDCLASS wc;
	RECT rect;
          int WNDSTYLE = WS_OVERLAPPEDWINDOW;
	int iErrors = 0;
	
	memset( &wc, 0, sizeof( wc ));

	com_argc = argc;
	memcpy(com_argv, argv, MAX_NUM_ARGVS );
	
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = s_gui.gHinst;
	wc.hIcon = LoadIcon (wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) COLOR_WINDOW;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = CLASSNAME;

	if (!wc.hIcon) wc.hIcon = LoadIcon (NULL, IDI_WINLOGO);
	if (!RegisterClass (&wc)) Sys_Error("Can't create editor window\n");

	// move window into center of screen
	rect.left = 0;
	rect.right = MAIN_WND_WIDTH;
	rect.top = 0;
	rect.bottom = MAIN_WND_HEIGHT;

	AdjustWindowRect( &rect, WNDSTYLE, FALSE );

	hDC = GetDC( GetDesktopWindow() );
	s_gui.scr_width = GetDeviceCaps( hDC, HORZRES );
	s_gui.scr_height = GetDeviceCaps( hDC, VERTRES );
	ReleaseDC( GetDesktopWindow(), hDC );

	s_gui.width = rect.right - rect.left;
	s_gui.height = rect.bottom - rect.top;
          s_gui.top = ( s_gui.scr_width - MAIN_WND_WIDTH ) / 2;
          s_gui.bottom = ( s_gui.scr_height - MAIN_WND_HEIGHT ) / 2;

	GUI_ResetWndOptions(); // load default settings
	InitCommonControls ();
          
	if(GUI_LoadPlatfrom( funcname, argc, argv )) //load config
	{
		wnd_options_t *config_dat;
		int config_size;
		
		config_dat = (wnd_options_t *)pi->Fs.LoadFile( "editor.dat", &config_size );

		if(config_dat) //verify our config before read
		{
			if(config_dat->id != IDEDITORHEADER)
			{
				MsgWarn("InitEditor: editor.dat have mismath header!\n");
				iErrors++;
			}
			if(config_dat->version != (int)EDITOR_VERSION)
			{
				MsgWarn("InitEditor: editor.dat have mismath version!\n");
				iErrors++;
			}
			if(config_dat->csize != config_size)
			{
				MsgWarn("InitEditor: editor.dat have mismath size!\n");
				iErrors++;
			}
			//copy settings into main structure
			if(!iErrors) GUI_LoadWndOptions( config_dat );
		}
	}
	else iErrors++;

	GUI_CreateEditorWindow();
	GUI_CreateMenus();
	GUI_CreateOptionsWindow();
	
	//apply chnages
	if(w_opts.show_console) GUI_ShowConsole(); 
	else GUI_HideConsole();
	
	if(iErrors) GUI_DisableMenus(); // apply error
	GUI_ExecuteBuffer(); //show all messages
	
	//end of all initializations
	ShowWindow(s_gui.hWnd, SW_SHOWDEFAULT);
	MsgDev(D_INFO, "------- Xash Recource Editor ver. %g initialized -------\n", EDITOR_VERSION );
}

void EditorMain ( void )
{
	// wait for the user to quit
	while(msg.message != WM_QUIT)
	{
		if(GetMessage (&msg, 0, 0, 0))
		{
			if (!LookupAccelTable( msg ))
			{
				TranslateMessage (&msg);
				DispatchMessage (&msg);
			}
		}
	}	

	if(platform_dll.link)
	{
		// save our settings
		pi->Fs.WriteFile("editor.dat", &w_opts, w_opts.csize );
	}
}

void FreeEditor ( void )
{
	// free richedit32
	if (s_gui.richedit) FreeLibrary( s_gui.richedit );

	// free platform
	if(platform_dll.link)
	{
		pi->Shutdown();
		Sys_FreeLibrary(&platform_dll);
	}	

	GUI_RemoveAccelTable();
	UnregisterClass (CLASSNAME, s_gui.gHinst);
}