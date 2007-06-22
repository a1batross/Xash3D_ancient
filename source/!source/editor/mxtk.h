//=======================================================================
//			Copyright XashXT Group 2007 ©
//			mxtk.h - small interface creator
//=======================================================================
#ifndef GUIUTILS_H
#define GUIUTILS_H

#include <richedit.h>
#include <windowsx.h>
#include "resource.h"
#include "kbd_layout.h"
#include "options.h"

typedef struct
{
	HINSTANCE		gHinst;	//app hinstance
	HWND		hWnd;
	HWND		hConsole;
	HWND		hTree;
	HWND		hOptions;
	HWND		hTabs;
	HWND		hTips;
	HMODULE		richedit;

	HMENU		menu;	//root menu
	HMENU		file;
	HMENU		edit;
	HMENU		cmds;
	HMENU		help;

	int		width, height;
	int		top, bottom;

	int		scr_width;
	int		scr_height;
}GUI_Form;

typedef struct menuitem_s
{
	uint	itemId;	//menu item identifier
	char	name[32]; //display this name
	uint	iStyle;	//default, separator, checked
	uint	iHotKey;	
	uint	iModifier; //shift, ctrl //TODO: determine automatically
}menuitem_t;

#define ITEM_DEF		0
#define ITEM_SEP		1
#define ITEM_CHK		2

typedef struct mcolumn_s
{
	menuitem_t *item;
	int numitems;
	
}mcolumn_t;

void GUI_CreateEditorWindow( void );

//font operations
void GUI_SetFont( HWND hwnd );
void GUI_ApplyFont( HWND hwnd );

//accelerators
void GUI_AddAccelerator(int key, int flags, int cmd);
void GUI_CreateAccelTable( void );
bool LookupAccelTable( MSG msg );
void GUI_RemoveAccelTable( void );

//menu builder
void GUI_AddMenuItem( HMENU menu, const char *name, uint itemId, uint hotkey );
void GUI_AddMenuString( menuitem_t item );
void TEST_MenuList( void );

enum
{
	IDC_OPTIONS = 1901,
};

enum
{
	//menu "file"
	IDM_CREATE = 32,
	IDM_OPEN,
	IDM_CLOSE,
	IDM_SAVE,
	IDM_SAVEAS,
	IDM_QUIT,
	
	//menu "edit"
	IDM_UNDO,
	IDM_REDO,
	IDM_CUT,
	IDM_COPY,
	IDM_PASTE,
	IDM_DELETE,
	IDM_FIND,
	IDM_REPLACE,
	IDM_GOTO,
	
	//menu "commands"
	IDM_COMPILE,
	IDM_GETINFO,
	IDM_SETTINGS,
	IDM_SHOWCONSOLE,

	//menu "about"
	IDM_ABOUT,

	IDH_HIDECONSOLE,

	//custom menu's
	IDM_FIRSTCHILD,
};

void GUI_Error( const char *pMsg, ... );
void GUI_Print(const char *pMsg);
void GUI_DisableMenus( void );

extern mcolumn_t MenuInfo;
extern GUI_Form s_gui;
extern wnd_options_t w_opts;	//window options


#endif//GUIUTILS_H