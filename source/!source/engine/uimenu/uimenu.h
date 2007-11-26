//=======================================================================
//			Copyright XashXT Group 2007 ©
//		     uimenu.h - Xash menu system definition
//=======================================================================

#ifndef UIMENU_H
#define UIMENU_H

#include "engine.h"
#include "client.h"
#include "progsvm.h"
#include "keycodes.h"

#define MAXMENUITEMS	64

// menu types
#define MTYPE_SLIDER	0
#define MTYPE_LIST		1
#define MTYPE_ACTION	2
#define MTYPE_SPINCONTROL	3
#define MTYPE_SEPARATOR  	4
#define MTYPE_FIELD		5

// menu flags
#define QMF_LEFT_JUSTIFY	0x00000001
#define QMF_GRAYED		0x00000002
#define QMF_NUMBERSONLY	0x00000004

typedef struct _tag_menuframework
{
	int	x, y;
	int	cursor;

	int	nitems;
	int	nslots;
	void	*items[64];

	const char *statusbar;
	void (*cursordraw)( struct _tag_menuframework *m );
} menuframework_s;

typedef struct
{
	const char *name;
	menuframework_s *parent;
	int	type;
	int	x, y;
	int	cursor_offset;
	int	localdata[4];
	uint	flags;
	const char *statusbar;

	void (*callback)( void *self );
	void (*statusbarfunc)( void *self );
	void (*ownerdraw)( void *self );
	void (*cursordraw)( void *self );
} menucommon_s;

typedef struct
{
	menucommon_s generic;
	field_t	field;	
} menufield_s;

typedef struct 
{
	menucommon_s generic;
	float	minvalue;
	float	maxvalue;
	float	curvalue;
	float	range;
} menuslider_s;

typedef struct
{
	menucommon_s generic;
	int	curvalue;
	const char **itemnames;
} menulist_s;

typedef struct
{
	menucommon_s generic;
} menuaction_s;

typedef struct
{
	menucommon_s generic;
} menuseparator_s;

bool Field_Key( menufield_s *field, int key );
void Menu_AddItem( menuframework_s *menu, void *item );
void Menu_AdjustCursor( menuframework_s *menu, int dir );
void Menu_Center( menuframework_s *menu );
void Menu_Draw( menuframework_s *menu );
void *Menu_ItemAtCursor( menuframework_s *m );
bool Menu_SelectItem( menuframework_s *s );
void Menu_SetStatusBar( menuframework_s *s, const char *string );
void Menu_SlideItem( menuframework_s *s, int dir );
int  Menu_TallySlots( menuframework_s *menu );
void M_FindKeysForCommand(char *command, int *keys);
void Menu_DrawString( int, int, const char * );
void Menu_DrawStringDark( int, int, const char * );
void Menu_DrawStringR2L( int, int, const char * );
void Menu_DrawStringR2LDark( int, int, const char * );

// progs menu
#define UI_MAX_EDICTS	(1 << 12) // should be enough for a menu

extern bool ui_active;
extern const int vm_ui_numbuiltins;
extern prvm_builtin_t vm_ui_builtins[];

void UI_Init( void );
void UI_KeyEvent( int key );
void UI_ToggleMenu_f( void );
void UI_Shutdown( void );
void UI_Draw( void );

#endif//UIMENU_H