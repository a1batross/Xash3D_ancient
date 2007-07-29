//=======================================================================
//			Copyright XashXT Group 2007 ©
//			platform.h - game platform dll
//=======================================================================
#ifndef GENERICEDIT_H
#define GENERICEDIT_H

#include <stdio.h>
#include <setjmp.h>
#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <string.h>
#include <basetypes.h>
#include <ref_launcher.h>
#include <ref_system.h>

#include "mxtk.h"
#include "options.h"

#define CLASSNAME		"SystemEditor"

//=====================================
//	main editor funcs
//=====================================
void InitEditor ( char *funcname, int argc, char **argv );
void EditorMain ( void );
void FreeEditor ( void );

extern int com_argc;
extern bool debug_mode;
extern platform_exp_t *pi;
extern char *com_argv[MAX_NUM_ARGVS];
int CheckParm (const char *parm);

/*
===========================================
System Events
===========================================
*/
void GUI_Msg( const char *pMsg, ... );
void GUI_MsgDev( const char *pMsg, ... );

extern stdinout_api_t std;
#define Msg GUI_Msg
#define MsgDev GUI_MsgDev
#define Sys_Error std.error

typedef enum {
	Action,
	Size,
	Timer,
	Idle,
	Show,
	Hide,
	MouseUp,
	MouseDown,
	MouseMove,
	MouseDrag,
	KeyUp,
	KeyDown,
	MouseWheel,
}eventlist_t;

typedef struct event_s
{
	int	event;
	HWND	*widget;
	int	action;
	int	width, height;
	int	x, y, buttons;
	int	key;
	int	modifiers;
	int	flags;
	int	zdelta;
}event_t;

typedef struct window_s
{
	int ( *handleEvent ) ( event_t *event );
	HWND Handle;

}window_t;

#endif//GENERICEDIT_H