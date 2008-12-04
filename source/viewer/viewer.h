//=======================================================================
//			Copyright XashXT Group 2007 �
//			viewer.h - recource viewer
//=======================================================================
#ifndef GENERICVIEW_H
#define GENERICVIEW_H

#include <stdio.h>
#include <setjmp.h>
#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <string.h>
#include "launch_api.h"

#include "mxtk.h"
#include "options.h"

#define CLASSNAME		"SystemViewer"

//=====================================
//	main editor funcs
//=====================================
void InitViewer ( int argc, char **argv );
void ViewerMain ( void );
void FreeViewer ( void );

extern int com_argc;
extern int dev_mode;
extern char **com_argv;

/*
===========================================
System Events
===========================================
*/
extern stdlib_api_t com;
#define Sys_Error com.error

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

} window_t;

#endif//GENERICVIEW_H