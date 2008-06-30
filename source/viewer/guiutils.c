//=======================================================================
//			Copyright XashXT Group 2007 ©
//			guiutils.c - resource viewer
//=======================================================================

#include "viewer.h"

int dev_mode = 0;
/*
=============================================================================

GUI Set Font (change and apply new font settings)

=============================================================================
*/

/*
====================
GUI_SetFont

Must call once at start application
====================
*/
void GUI_SetFont( HWND hwnd )
{
	CHARFORMAT cf;
	memset(&cf, 0, sizeof(cf));
	cf.cbSize = sizeof(cf);
	cf.dwMask = w_opts.font_type;
	strcpy(cf.szFaceName, w_opts.fontname);
	cf.yHeight = w_opts.font_size;
	cf.crTextColor = w_opts.font_color;

	//go to "Courier", 14pt as default
	SendMessage(hwnd, EM_SETCHARFORMAT, SCF_ALL, (WPARAM)&cf);
}

/*
====================
GUI_ApplyFont

Apply new font for specified window
====================
*/
void GUI_ApplyFont( HWND hwnd )
{
	SendMessage (hwnd, WM_SETFONT, (WPARAM) (HFONT) GetStockObject (ANSI_VAR_FONT), MAKELPARAM(TRUE, 0));
}

/*
=============================================================================

GUI Register Accelerators (Hot keys)

=============================================================================
*/
static HACCEL g_hAccel = 0;
static ACCEL g_AccelTable[64] = { 0 };
static int g_numAccel = 0;
#define MAX_HOTKEYS		0x40

/*
====================
GUI_AddAccelerator

Add another hotkeys combination
Must be before call GUI_CreateAccelTable()
====================
*/
void GUI_AddAccelerator(int key, int flags, int cmd)
{
	if (g_numAccel < MAX_HOTKEYS)
	{
		g_AccelTable[g_numAccel].fVirt = flags;
		g_AccelTable[g_numAccel].key = key;
		g_AccelTable[g_numAccel].cmd = cmd;
		g_numAccel++;
	}
	else MsgDev( D_ERROR, "GUI_AddAccelerator: can't register hotkey! Hokey limit exceed\n");
}

/*
====================
GUI_CreateAccelTable

Create a table with hotkeys
====================
*/
void GUI_CreateAccelTable( void )
{
	//create accel table
	g_hAccel = CreateAcceleratorTable (g_AccelTable, g_numAccel);
	if(!g_hAccel) MsgDev( D_ERROR, "GUI_CreateAccelTable: can't create accel table\n");
}

/*
====================
LookupAccelTable

Called from main program loop
====================
*/
bool LookupAccelTable( MSG msg )
{
	//nothing to lookup?
	if(!g_hAccel) return false;

	return TranslateAccelerator(s_gui.hWnd, g_hAccel, &msg);
} 

/*
====================
GUI_RemoveAccelTable

Called before quit from application
====================
*/
void GUI_RemoveAccelTable( void )
{
	if (g_hAccel) DestroyAcceleratorTable (g_hAccel);
	g_numAccel = 0;
	g_hAccel = 0;
}

/*
=============================================================================

GUI Create Menu's 

=============================================================================
*/
void GUI_AddMenuItem( HMENU menu, const char *name, uint itemId, uint hotkey )
{
	if(strlen(name)) AppendMenu(menu, 0, itemId, name );
          else 
          {
          	AppendMenu(menu, MF_SEPARATOR, 0, 0);
		return;
	}

	if(hotkey)
	{
		int style = FVIRTKEY;
		int length = strlen(name) - 1;

		//parse modifiers
		if (stristr (name, "ctrl")) style |= FCONTROL;
		if (stristr (name, "shift")) style |= FSHIFT;
		if (stristr (name, "alt")) style |= FALT;
	
		GUI_AddAccelerator(hotkey, style, itemId);
	}
}