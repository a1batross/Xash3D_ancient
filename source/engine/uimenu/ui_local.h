/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef UI_LOCAL_H
#define UI_LOCAL_H

#include "mathlib.h"
#include "entity_def.h"
#include "render_api.h"

#define UI_WHITE_SHADER		"*white"
#define ART_MAIN_SPLASH		"gfx/shell/splash"
#define ART_BACKGROUND		"gfx/shell/background"
#define UI_CURSOR_NORMAL		"gfx/shell/cursor"
#define UI_CURSOR_DISABLED		"gfx/shell/denied"
#define UI_CURSOR_TYPING		"gfx/shell/typing"
#define UI_SLIDER_MAIN		"gfx/shell/slider"
#define UI_LEFTARROW		"gfx/shell/larrow_d"
#define UI_LEFTARROWFOCUS		"gfx/shell/larrow_f"
#define UI_LEFTARROWPRESSED		"gfx/shell/larrow_n"
#define UI_RIGHTARROW		"gfx/shell/rarrow_d"
#define UI_RIGHTARROWFOCUS		"gfx/shell/rarrow_f"
#define UI_RIGHTARROWPRESSED		"gfx/shell/rarrow_n"
#define UI_UPARROW			"gfx/shell/uparrow_d"
#define UI_UPARROWFOCUS		"gfx/shell/uparrow_f"
#define UI_UPARROWPRESSED		"gfx/shell/uparrow_n"
#define UI_DOWNARROW		"gfx/shell/dnarrow_d"
#define UI_DOWNARROWFOCUS		"gfx/shell/dnarrow_f"
#define UI_DOWNARROWPRESSED		"gfx/shell/dnarrow_n"
#define UI_CHECKBOX_EMPTY		"gfx/shell/cb_empty"
#define UI_CHECKBOX_GRAYED		"gfx/shell/cb_disabled"
#define UI_CHECKBOX_FOCUS		"gfx/shell/cb_over"
#define UI_CHECKBOX_PRESSED		"gfx/shell/cb_down"
#define UI_CHECKBOX_ENABLED		"gfx/shell/cb_checked"

#define UI_MOVEBOX			"gfx/shell/buttons/move_box"
#define UI_MOVEBOXFOCUS		"gfx/shell/buttons/move_box_s"
#define UI_BACKBUTTON		"gfx/shell/buttons/back_b"

#define UI_MAX_MENUDEPTH		8
#define UI_MAX_MENUITEMS		64

#define UI_CURSOR_SIZE		40

#define UI_PULSE_DIVISOR		75
#define UI_BLINK_TIME		250
#define UI_BLINK_MASK		499

#define UI_SMALL_CHAR_WIDTH		10
#define UI_SMALL_CHAR_HEIGHT		20
#define UI_MED_CHAR_WIDTH		18
#define UI_MED_CHAR_HEIGHT		26
#define UI_BIG_CHAR_WIDTH		20
#define UI_BIG_CHAR_HEIGHT		40

#define UI_MAX_FIELD_LINE		256
#define UI_OUTLINE_WIDTH		uiStatic.outlineWidth	// outline thickness

#define UI_MAXGAMES			100	// slots for savegame
#define UI_MAX_SERVERS		10

// menu banners used fiexed rectangle (virtual screenspace at 640x480)
#define UI_BANNER_POSX		72
#define UI_BANNER_POSY		72
#define UI_BANNER_WIDTH		736
#define UI_BANNER_HEIGHT		128

// Generic types
typedef enum
{
	QMTYPE_SCROLLLIST,
	QMTYPE_SPINCONTROL,
	QMTYPE_CHECKBOX,
	QMTYPE_SLIDER,
	QMTYPE_FIELD,
	QMTYPE_ACTION,
	QMTYPE_BITMAP
} menuType_t;

// Generic flags
#define QMF_LEFT_JUSTIFY		BIT(0)
#define QMF_CENTER_JUSTIFY		BIT(1)
#define QMF_RIGHT_JUSTIFY		BIT(2)
#define QMF_GRAYED			BIT(3)	// Grays and disables
#define QMF_INACTIVE		BIT(4)	// Disables any input
#define QMF_HIDDEN			BIT(5)	// Doesn't draw
#define QMF_NUMBERSONLY		BIT(6)	// Edit field is only numbers
#define QMF_LOWERCASE		BIT(7)	// Edit field is all lower case
#define QMF_UPPERCASE		BIT(8)	// Edit field is all upper case
#define QMF_BLINKIFFOCUS		BIT(9)
#define QMF_PULSEIFFOCUS		BIT(10)
#define QMF_HIGHLIGHTIFFOCUS		BIT(11)
#define QMF_SMALLFONT		BIT(12)
#define QMF_BIGFONT			BIT(13)
#define QMF_DROPSHADOW		BIT(14)
#define QMF_SILENT			BIT(15)	// Don't play sounds
#define QMF_HASMOUSEFOCUS		BIT(16)
#define QMF_MOUSEONLY		BIT(17)	// Only mouse input allowed
#define QMF_FOCUSBEHIND		BIT(18)	// Focus draws behind normal item
#define QMF_NOTIFY			BIT(19)	// draw notify at right screen side
#define QMF_ACT_ONRELEASE		BIT(20)	// call Key_Event when button is released

// Callback notifications
#define QM_GOTFOCUS			1
#define QM_LOSTFOCUS		2
#define QM_ACTIVATED		3
#define QM_CHANGED			4
#define QM_PRESSED			5

typedef struct
{
	int		cursor;
	int		cursorPrev;

	void		*items[UI_MAX_MENUITEMS];
	int		numItems;

	void		(*drawFunc)( void );
	const char	*(*keyFunc)( int key, bool down );
} menuFramework_s;

typedef struct
{
	menuType_t	type;
	const char	*name;
	int		id;

	uint		flags;

	int		x;
	int		y;
	int		width;
	int		height;

	int		x2;
	int		y2;
	int		width2;
	int		height2;

	byte		*color;
	byte		*focusColor;

	int		charWidth;
	int		charHeight;

	const char	*statusText;

	menuFramework_s	*parent;

	void		(*callback) (void *self, int event);
	void		(*ownerdraw) (void *self);
} menuCommon_s;

typedef struct
{
	menuCommon_s	generic;
 	const char	*background;
	const char	*upArrow;
	const char	*upArrowFocus;
	const char	*downArrow;
	const char	*downArrowFocus;
	const char	**itemNames;
	int		numItems;
	int		curItem;
	int		topItem;
	int		numRows;
} menuScrollList_s;

typedef struct
{
	menuCommon_s	generic;
	const char	*background;
	const char	*leftArrow;
	const char	*rightArrow;
	const char	*leftArrowFocus;
	const char	*rightArrowFocus;
	float		minValue;
	float		maxValue;
	float		curValue;
	float		range;
} menuSpinControl_s;

typedef struct
{
	menuCommon_s	generic;
	const char	*background;
	int		maxLenght;
	int		visibleLength;
	char		buffer[UI_MAX_FIELD_LINE];
	int		length;
	int		cursor;
} menuField_s;

typedef struct
{
	menuCommon_s	generic;
	const char	*background;
} menuAction_s;

typedef struct
{
	menuCommon_s	generic;
	const char	*pic;
	const char	*focusPic;
} menuBitmap_s;

typedef struct
{
	menuCommon_s	generic;
	bool		enabled;
	const char	*emptyPic;
	const char	*focusPic;	// can be replaced with pressPic manually
	const char	*checkPic;
	const char	*grayedPic;	// when QMF_GRAYED is set
} menuCheckBox_s;

typedef struct
{
	menuCommon_s	generic;
	float		minValue;
	float		maxValue;
	float		curValue;
	float		drawStep;
	int		numSteps;
	float		range;
	bool		keepSlider;	// when mouse button is holds
} menuSlider_s;

void UI_ScrollList_Init( menuScrollList_s *sl );
const char *UI_ScrollList_Key( menuScrollList_s *sl, int key, bool down );
void UI_ScrollList_Draw( menuScrollList_s *sl );

void UI_SpinControl_Init( menuSpinControl_s *sc );
const char *UI_SpinControl_Key( menuSpinControl_s *sc, int key, bool down );
void UI_SpinControl_Draw( menuSpinControl_s *sc );

void UI_Slider_Init( menuSlider_s *sl );
const char *UI_Slider_Key( menuSlider_s *sl, int key, bool down );
void UI_Slider_Draw( menuSlider_s *sl );

void UI_CheckBox_Init( menuCheckBox_s *cb );
const char *UI_CheckBox_Key( menuCheckBox_s *cb, int key, bool down );
void UI_CheckBox_Draw( menuCheckBox_s *cb );

void UI_Field_Init( menuField_s *f );
const char *UI_Field_Key( menuField_s *f, int key, bool down );
void UI_Field_Draw( menuField_s *f );

void UI_Action_Init( menuAction_s *t );
const char *UI_Action_Key( menuAction_s *t, int key, bool down );
void UI_Action_Draw( menuAction_s *t );

void UI_Bitmap_Init( menuBitmap_s *b );
const char *UI_Bitmap_Key( menuBitmap_s *b, int key, bool down );
void UI_Bitmap_Draw( menuBitmap_s *b );

// =====================================================================
// Main menu interface

extern cvar_t	*ui_mainfont;
extern cvar_t	*ui_namefont;
extern cvar_t	*ui_precache;
extern cvar_t	*ui_sensitivity;
extern render_exp_t	*re;

typedef struct
{
	menuFramework_s	*menuActive;
	menuFramework_s	*menuStack[UI_MAX_MENUDEPTH];
	int		menuDepth;

	netadr_t		serverAddresses[UI_MAX_SERVERS];
	char		serverNames[UI_MAX_SERVERS][80];
	int		numServers;

	float		scaleX;
	float		scaleY;
	int		outlineWidth;
	int		sliderWidth;

	int		cursorX;
	int		cursorY;
	int		realTime;
	bool		firstDraw;
	bool		enterSound;
	bool		mouseInRect;
	bool		hideCursor;
	bool		visible;
	bool		initialized;

	shader_t		menuFont;
	shader_t		nameFont;
} uiStatic_t;

extern uiStatic_t		uiStatic;

extern string		uiEmptyString;
extern const char		*uiSoundIn;
extern const char		*uiSoundMove;
extern const char		*uiSoundOut;
extern const char		*uiSoundBuzz;
extern const char		*uiSoundGlow;
extern const char		*uiSoundNull;

//
// keys.c
//
char *Key_GetBinding( int keynum );
void Key_SetBinding( int keynum, char *binding );
char *Key_KeynumToString( int keynum );
bool Key_IsDown( int keynum );

//
// cl_game.c
//
extern rgba_t		uiColorWhite;
extern rgba_t		uiColorLtGrey;
extern rgba_t		uiColorMdGrey;
extern rgba_t		uiColorDkGrey;
extern rgba_t		uiColorBlack;
extern rgba_t		uiColorRed;
extern rgba_t		uiColorGreen;
extern rgba_t		uiColorBlue;
extern rgba_t		uiColorYellow;
extern rgba_t		uiColorOrange;
extern rgba_t		uiColorCyan;
extern rgba_t		uiColorMagenta;
extern rgba_t		uiScrollOutlineColor;
extern rgba_t		uiScrollSelColor;

void UI_ScaleCoords( int *x, int *y, int *w, int *h );
bool UI_CursorInRect( int x, int y, int w, int h );
void UI_DrawPic( int x, int y, int w, int h, const rgba_t color, const char *pic );
void UI_FillRect( int x, int y, int w, int h, const rgba_t color );
#define UI_DrawRectangle( x, y, w, h, color ) UI_DrawRectangleExt( x, y, w, h, color, uiStatic.outlineWidth )
void UI_DrawRectangleExt( int in_x, int in_y, int in_w, int in_h, const rgba_t color, int outlineWidth );
#define UI_DrawString( x, y, w, h, str, col, fcol, cw, ch, j, s ) UI_DrawStringExt( x, y, w, h, str, col, fcol, cw, ch, j, s, uiStatic.menuFont )
void UI_DrawStringExt( int x, int y, int w, int h, const char *str, const rgba_t col, bool forceCol, int charW, int charH, int justify, bool shadow, shader_t font );
void UI_StartSound( const char *sound );

void UI_AddItem ( menuFramework_s *menu, void *item );
void UI_CursorMoved( menuFramework_s *menu );
void UI_SetCursor( menuFramework_s *menu, int cursor );
void UI_SetCursorToItem( menuFramework_s *menu, void *item );
void *UI_ItemAtCursor( menuFramework_s *menu );
void UI_AdjustCursor( menuFramework_s *menu, int dir );
void UI_DrawMenu( menuFramework_s *menu );
const char *UI_DefaultKey( menuFramework_s *menu, int key, bool down );
const char *UI_ActivateItem( menuFramework_s *menu, menuCommon_s *item );
void UI_RefreshServerList( void );
bool UI_CreditsActive( void );

void UI_CloseMenu( void );
void UI_PushMenu( menuFramework_s *menu );
void UI_PopMenu( void );

// Precache
void UI_Main_Precache( void );
void UI_NewGame_Precache( void );
void UI_LoadGame_Precache( void );
void UI_SaveGame_Precache( void );
void UI_SaveLoad_Precache( void );
void UI_MultiPlayer_Precache( void );
void UI_Options_Precache( void );
void UI_PlayerSetup_Precache( void );
void UI_Controls_Precache( void );
void UI_AdvControls_Precache( void );
void UI_GameOptions_Precache( void );
void UI_Audio_Precache( void );
void UI_Video_Precache( void );
void UI_VidOptions_Precache( void );
void UI_VidModes_Precache( void );
void UI_Demos_Precache( void );
void UI_CustomGame_Precache( void );
void UI_Credits_Precache( void );
void UI_GoToSite_Precache( void );

// Menus
void UI_Main_Menu( void );
void UI_NewGame_Menu( void );
void UI_LoadGame_Menu( void );
void UI_SaveGame_Menu( void );
void UI_SaveLoad_Menu( void );
void UI_MultiPlayer_Menu( void );
void UI_Options_Menu( void );
void UI_PlayerSetup_Menu( void );
void UI_Controls_Menu( void );
void UI_AdvControls_Menu( void );
void UI_GameOptions_Menu( void );
void UI_Audio_Menu( void );
void UI_Video_Menu( void );
void UI_VidOptions_Menu( void );
void UI_VidModes_Menu( void );
void UI_Demos_Menu( void );
void UI_CustomGame_Menu( void );
void UI_Credits_Menu( void );

#endif // UI_LOCAL_H