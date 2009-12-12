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

#include "common.h"
#include "ui_local.h"
#include "input.h"

#define ART_BACKGROUND		"gfx/shell/splash"
#define ART_BANNER			"gfx/shell/head_controls"
#define ART_TEXT_LOOK		"gfx/shell/text/cont_look_text_p1"
#define ART_TEXT_LOOK2		"gfx/shell/text/cont_look_text_p2"
#define ART_TEXT_MOVE		"gfx/shell/text/cont_move_text_p1"
#define ART_TEXT_MOVE2		"gfx/shell/text/cont_move_text_p2"
#define ART_TEXT_SHOOT		"gfx/shell/text/cont_shoot_text_p1"
#define ART_TEXT_SHOOT2		"gfx/shell/text/cont_shoot_text_p2"
#define ART_TEXT_MISC		"gfx/shell/text/cont_misc_text_p1"
#define ART_TEXT_MISC2		"gfx/shell/text/cont_misc_text_p2"
#define ART_LOOK			"gfx/shell/buttons/controls_look"
#define ART_MOVE			"gfx/shell/buttons/controls_move"
#define ART_SHOOT			"gfx/shell/buttons/controls_shoot"
#define ART_MISC			"gfx/shell/buttons/controls_misc"

#define ID_BACKGROUND		0
#define ID_BANNER			1

#define ID_TEXT1			2
#define ID_TEXT2			3
#define ID_TEXTSHADOW1		4
#define ID_TEXTSHADOW2		5

#define ID_BACK			6

#define ID_LOOK			7
#define ID_MOVE			8
#define ID_SHOOT			9
#define ID_MISC			10

#define ID_MOUSESPEED		11
#define ID_SMOOTHMOUSE		12
#define ID_INVERTMOUSE		13
#define ID_LOOKUP			14
#define ID_LOOKDOWN			15
#define ID_MOUSELOOK		16
#define ID_FREELOOK			17
#define ID_CENTERVIEW		18
#define ID_ZOOMVIEW			19


#define ID_ALWAYSRUN		20
#define ID_RUN			21
#define ID_FORWARD			22
#define ID_BACKWARD			23
#define ID_STEPLEFT			24
#define ID_STEPRIGHT		25
#define ID_JUMP			26
#define ID_CROUCH			27
#define ID_LEFT			28
#define ID_RIGHT			39
#define ID_SIDESTEP			30

#define ID_ATTACK			31
#define ID_NEXTWEAP			32
#define ID_PREVWEAP			33
#define ID_BLASTER			34
#define ID_SHOTGUN			35
#define ID_SUPERSHOTGUN		36
#define ID_MACHINEGUN		37
#define ID_CHAINGUN			38
#define ID_GRENADELAUNCHER		39
#define ID_ROCKETLAUNCHER		40
#define ID_HYPERBLASTER		41
#define ID_RAILGUN			42
#define ID_BFG			43

#define ID_SHOWSCORES		44
#define ID_USEITEM			45
#define ID_NEXTITEM			46
#define ID_PREVITEM			47
#define ID_DROPITEM			48
#define ID_FIELDCOMPUTER		49
#define ID_INVENTORY		50
#define ID_FLIPOFF			51
#define ID_SALUTE			52
#define ID_TAUNT			53
#define ID_WAVE			54
#define ID_POINT			55
#define ID_CHAT			56
#define ID_CHATTEAM			57

static const char	*uiControlsYesNo[] = { "False", "True" };
static const char	*uiControlsMouseAccel[] = { "None", "Low", "Medium", "High" };

typedef struct
{
	int	id;
	char	*command;
	int	bind1;
	int	bind2;
} uiControlsBind_t;

uiControlsBind_t	uiControlsBindings[] =
{
{ID_LOOKUP,	"+lookup",		-1,	-1},
{ID_LOOKDOWN,	"+lookdown",		-1,	-1},
{ID_MOUSELOOK,	"+mlook",			-1,	-1},
{ID_CENTERVIEW,	"centerview",		-1,	-1},
{ID_ZOOMVIEW,	"+zoom",		   	-1,	-1},
{ID_RUN,		"+speed",		   	-1,	-1},
{ID_FORWARD,	"+forward",	   	-1,	-1},
{ID_BACKWARD,	"+back",		   	-1,	-1},
{ID_STEPLEFT,	"+moveleft",	   	-1,	-1},
{ID_STEPRIGHT,	"+moveright",	   	-1,	-1},
{ID_JUMP,		"+moveup",	   	-1,	-1},
{ID_CROUCH,	"+movedown",	   	-1,	-1},
{ID_LEFT,		"+left",		   	-1,	-1},
{ID_RIGHT,	"+right",		   	-1,	-1},
{ID_SIDESTEP,	"+strafe",	   	-1,	-1},
{ID_ATTACK,	"+attack",	   	-1,	-1},
{ID_NEXTWEAP,	"weapnext",	   	-1,	-1},
{ID_PREVWEAP,	"weapprev",	   	-1,	-1},
{ID_BLASTER,	"use Blaster",	   	-1,	-1},
{ID_SHOTGUN,	"use Shotgun",	   	-1,	-1},
{ID_SUPERSHOTGUN,	"use Super Shotgun",	-1,	-1},
{ID_MACHINEGUN,	"use Machinegun",		-1,	-1},
{ID_CHAINGUN,	"use Chaingun",	   	-1,	-1},
{ID_GRENADELAUNCHER,"use Grenade Launcher",	-1,	-1},
{ID_ROCKETLAUNCHER,	"use Rocket Launcher",	-1,	-1},
{ID_HYPERBLASTER,	"use HyperBlaster",		-1,	-1},
{ID_RAILGUN,	"use Railgun",		-1,	-1},
{ID_BFG,		"use BFG10K",		-1,	-1},
{ID_SHOWSCORES,	"cmd score",		-1,	-1},
{ID_USEITEM,	"invuse",			-1,	-1},
{ID_NEXTITEM,	"invnext",		-1,	-1},
{ID_PREVITEM,	"invprev",		-1,	-1},
{ID_DROPITEM,	"invdrop",		-1,	-1},
{ID_FIELDCOMPUTER,	"cmd help",		-1,	-1},
{ID_INVENTORY,	"inven",			-1,	-1},
{ID_FLIPOFF,	"wave 0",			-1,	-1},
{ID_SALUTE,	"wave 1",			-1,	-1},
{ID_TAUNT,	"wave 2",			-1,	-1},
{ID_WAVE,		"wave 3",			-1,	-1},
{ID_POINT,	"wave 4",			-1,	-1},
{ID_CHAT,		"messagemode",		-1,	-1},
{ID_CHATTEAM,	"messagemode2",		-1,	-1},
{-1,		NULL,			-1,	-1}
};

typedef enum
{
	LOOK,
	MOVE,
	SHOOT,
	MISC
} uiControlsSection_t;

typedef struct
{
	uiControlsSection_t		controlsSection;
	bool			waitingForKey;

	menuFramework_s		menu;

	menuBitmap_s		background;
	menuBitmap_s		banner;

	menuBitmap_s		textShadow1;
	menuBitmap_s		textShadow2;
	menuBitmap_s		text1;
	menuBitmap_s		text2;

	menuBitmap_s		back;

	menuBitmap_s		look;
	menuBitmap_s		move;
	menuBitmap_s		shoot;
	menuBitmap_s		misc;

	menuSpinControl_s		mouseSpeed;
	menuSpinControl_s		smoothMouse;
	menuSpinControl_s		invertMouse;
	menuAction_s		loopUp;
	menuAction_s		lookDown;
	menuAction_s		mouseLook;
	menuSpinControl_s		freeLook;
	menuAction_s		centerView;
	menuAction_s		zoomView;
	
	menuSpinControl_s		alwaysRun;
	menuAction_s		run;
	menuAction_s		forward;
	menuAction_s		backward;
	menuAction_s		stepLeft;
	menuAction_s		stepRight;
	menuAction_s		jump;
	menuAction_s		crouch;
	menuAction_s		left;
	menuAction_s		right;
	menuAction_s		sideStep;

	menuAction_s		attack;
	menuAction_s		nextWeap;
	menuAction_s		prevWeap;
	menuAction_s		blaster;
	menuAction_s		shotgun;
	menuAction_s		superShotgun;
	menuAction_s		machinegun;
	menuAction_s		chaingun;
	menuAction_s		grenadeLauncher;
	menuAction_s		rocketLauncher;
	menuAction_s		hyperBlaster;
	menuAction_s		railgun;
	menuAction_s		bfg;

	menuAction_s		showScores;
	menuAction_s		useItem;
	menuAction_s		nextItem;
	menuAction_s		prevItem;
	menuAction_s		dropItem;
	menuAction_s		fieldComputer;
	menuAction_s		inventory;
	menuAction_s		flipoff;
	menuAction_s		salute;
	menuAction_s		taunt;
	menuAction_s		wave;
	menuAction_s		point;
	menuAction_s		chat;
	menuAction_s		chatTeam;
} uiControls_t;

static uiControls_t			uiControls;

/*
=================
UI_Controls_BindingForControl
=================
*/
static uiControlsBind_t *UI_Controls_BindingForControl( int id )
{
	uiControlsBind_t	*binding;

	for( binding = uiControlsBindings; binding->id != -1; binding++ )
		if( binding->id == id ) break;

	return binding;
}

/*
=================
UI_Controls_GetKeyBindings
=================
*/
static void UI_Controls_GetKeyBindings( char *command, int *twoKeys )
{
	int	i, count = 0;
	char	*b;

	twoKeys[0] = twoKeys[1] = -1;

	for( i = 0; i < 256; i++ )
	{
		b = Key_GetBinding( i );
		if( !b ) continue;

		if( !com.stricmp( command, b ))
		{
			twoKeys[count] = i;
			count++;

			if( count == 2 ) break;
		}
	}
}

/*
=================
UI_Controls_GetConfig
=================
*/
static void UI_Controls_GetConfig( void )
{
	uiControlsBind_t	*binding;
	int		twoKeys[2];

	// Get key bindings
	for( binding = uiControlsBindings; binding->id != -1; binding++ )
	{
		UI_Controls_GetKeyBindings( binding->command, twoKeys );
		binding->bind1 = twoKeys[0];
		binding->bind2 = twoKeys[1];
	}

	uiControls.mouseSpeed.curValue = Cvar_VariableInteger( "m_sensitivity" );
	uiControls.mouseSpeed.curValue = bound( 1, uiControls.mouseSpeed.curValue, 30 );

	if( Cvar_VariableInteger( "m_filter" ))
		uiControls.smoothMouse.curValue = 1;

	if( Cvar_VariableValue( "m_pitch" ) < 0 )
		uiControls.invertMouse.curValue = 1;

	if( Cvar_VariableInteger( "cl_mouselook" ))
		uiControls.freeLook.curValue = 1;

	if( Cvar_VariableInteger( "cl_run" ))
		uiControls.alwaysRun.curValue = 1;
}

/*
=================
UI_Controls_SetConfig
=================
*/
static void UI_Controls_SetConfig( void )
{
	uiControlsBind_t	*binding;

	// set key bindings
	for( binding = uiControlsBindings; binding->id != -1; binding++ )
	{
		if( binding->bind1 != -1 )
		{
			Key_SetBinding( binding->bind1, binding->command );
			
			if( binding->bind2 != -1 )
				Key_SetBinding( binding->bind2, binding->command );
		}
	}

	Cvar_SetValue( "m_sensitivity", uiControls.mouseSpeed.curValue );
	Cvar_SetValue( "m_filter", uiControls.smoothMouse.curValue );

	if((int)uiControls.invertMouse.curValue)
		Cvar_SetValue( "m_pitch", -fabs(Cvar_VariableValue( "m_pitch" )));
	else Cvar_SetValue( "m_pitch", fabs(Cvar_VariableValue( "m_pitch" )));

	Cvar_SetValue( "cl_mouselook", uiControls.freeLook.curValue );
	Cvar_SetValue( "cl_run", uiControls.alwaysRun.curValue );
}

/*
=================
UI_Controls_UpdateConfig
=================
*/
static void UI_Controls_UpdateConfig( void )
{
	static char	sensitivityText[32];
	menuFramework_s	*menu = &uiControls.menu;
	int		i;

	if( uiControls.waitingForKey )
	{
		// disable all controls
		for( i = 6; i <= 57; i++ )
			((menuCommon_s *)menu->items[i])->flags |= QMF_GRAYED;

		// enable current control
		((menuCommon_s *)menu->items[menu->cursor])->flags &= ~QMF_GRAYED;
		return;
	}

	// enable all controls
	for( i = 6; i <= 57; i++ )
		((menuCommon_s *)menu->items[i])->flags &= ~QMF_GRAYED;

	com.snprintf( sensitivityText, sizeof(sensitivityText), "%i", (int)uiControls.mouseSpeed.curValue );
	uiControls.mouseSpeed.generic.name = sensitivityText;

	uiControls.smoothMouse.generic.name = uiControlsYesNo[(int)uiControls.smoothMouse.curValue];
	uiControls.invertMouse.generic.name = uiControlsYesNo[(int)uiControls.invertMouse.curValue];
	uiControls.freeLook.generic.name = uiControlsYesNo[(int)uiControls.freeLook.curValue];
	uiControls.alwaysRun.generic.name = uiControlsYesNo[(int)uiControls.alwaysRun.curValue];

	// hide all controls
	for( i = 11; i <= 57; i++ )
		((menuCommon_s *)menu->items[i])->flags |= QMF_HIDDEN;

	// Show the appropriate pics and controls for this section
	switch( uiControls.controlsSection )
	{
	case LOOK:
		uiControls.textShadow1.pic = ART_TEXT_LOOK;
		uiControls.textShadow2.pic = ART_TEXT_LOOK2;
		uiControls.text1.pic = ART_TEXT_LOOK;
		uiControls.text2.pic = ART_TEXT_LOOK2;

		for( i = 11; i <= 20; i++ )
			((menuCommon_s *)menu->items[i])->flags &= ~QMF_HIDDEN;
		break;
	case MOVE:
		uiControls.textShadow1.pic = ART_TEXT_MOVE;
		uiControls.textShadow2.pic = ART_TEXT_MOVE2;
		uiControls.text1.pic = ART_TEXT_MOVE;
		uiControls.text2.pic = ART_TEXT_MOVE2;

		for( i = 21; i <= 31; i++ )
			((menuCommon_s *)menu->items[i])->flags &= ~QMF_HIDDEN;
		break;
	case SHOOT:
		uiControls.textShadow1.pic = ART_TEXT_SHOOT;
		uiControls.textShadow2.pic = ART_TEXT_SHOOT2;
		uiControls.text1.pic = ART_TEXT_SHOOT;
		uiControls.text2.pic = ART_TEXT_SHOOT2;

		for( i = 32; i <= 44; i++ )
			((menuCommon_s *)menu->items[i])->flags &= ~QMF_HIDDEN;
		break;
	case MISC:
		uiControls.textShadow1.pic = ART_TEXT_MISC;
		uiControls.textShadow2.pic = ART_TEXT_MISC2;
		uiControls.text1.pic = ART_TEXT_MISC;
		uiControls.text2.pic = ART_TEXT_MISC2;

		for( i = 45; i <= 57; i++ )
			((menuCommon_s *)menu->items[i])->flags &= ~QMF_HIDDEN;
		break;
	}
}

/*
=================
UI_Controls_KeyFunc
=================
*/
static const char *UI_Controls_KeyFunc( int key )
{
	menuCommon_s	*item = (menuCommon_s *)(uiControls.menu.items[uiControls.menu.cursor]);
	uiControlsBind_t	*binding;

	if( item->type == QMTYPE_ACTION )
	{
		if( !uiControls.waitingForKey )
		{
			switch( key )
			{
			case K_MOUSE1:
			case K_ENTER:
			case K_KP_ENTER:
				uiControls.waitingForKey = true;
				UI_Controls_UpdateConfig();
				UI_Controls_SetConfig();
		
				return uiSoundMove;
			case K_BACKSPACE:
			case K_DEL:
			case K_KP_DEL:
				binding = UI_Controls_BindingForControl( item->id );

				if( binding->bind1 != -1 )
				{
					Key_SetBinding( binding->bind1, "" );
					binding->bind1 = -1;
				}
				if( binding->bind2 != -1 )
				{
					Key_SetBinding( binding->bind2, "" );
					binding->bind2 = -1;
				}

				UI_Controls_UpdateConfig();
				UI_Controls_SetConfig();
		
				return uiSoundOut;
			default:
				return UI_DefaultKey( &uiControls.menu, key );
			}
		}
		else
		{
			switch( key )
			{
			case K_ESCAPE:
				uiControls.waitingForKey = false;
				UI_Controls_UpdateConfig();
				UI_Controls_SetConfig();

				return uiSoundOut;
			case '`':
			case '~':
				return UI_DefaultKey( &uiControls.menu, key );
			default:
				// remove from any other bind
				for( binding = uiControlsBindings; binding->id != -1; binding++ )
				{
					if( binding->bind2 == key )
					{
						Key_SetBinding( binding->bind2, "" );
						binding->bind2 = -1;
					}
					if( binding->bind1 == key )
					{
						Key_SetBinding( binding->bind1, "" );
						binding->bind1 = binding->bind2;
						binding->bind2 = -1;
					}
				}

				// bind key to this control
				binding = UI_Controls_BindingForControl( item->id );
				if( binding->bind1 == -1 )
					binding->bind1 = key;
				else if( binding->bind1 != key && binding->bind2 == -1 )
					binding->bind2 = key;
				else
				{
					Key_SetBinding( binding->bind1, "" );
					Key_SetBinding( binding->bind2, "" );
					binding->bind1 = key;
					binding->bind2 = -1;
				}

				uiControls.waitingForKey = false;
				UI_Controls_UpdateConfig();
				UI_Controls_SetConfig();

				return uiSoundMove;
			}
		}
	}
	return UI_DefaultKey( &uiControls.menu, key );
}

/*
=================
UI_Controls_Callback
=================
*/
static void UI_Controls_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event == QM_CHANGED )
	{
		UI_Controls_UpdateConfig();
		UI_Controls_SetConfig();
		return;
	}

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_BACK:
		UI_PopMenu();
		break;
	case ID_LOOK:
		uiControls.controlsSection = LOOK;
		UI_Controls_UpdateConfig();
		break;
	case ID_MOVE:
		uiControls.controlsSection = MOVE;
		UI_Controls_UpdateConfig();
		break;
	case ID_SHOOT:
		uiControls.controlsSection = SHOOT;
		UI_Controls_UpdateConfig();
		break;
	case ID_MISC:
		uiControls.controlsSection = MISC;
		UI_Controls_UpdateConfig();
		break;
	}
}

/*
=================
UI_Controls_Ownerdraw
=================
*/
static void UI_Controls_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( item->type == QMTYPE_ACTION )
	{
		uiControlsBind_t	*binding;
		char		name[32], name2[32];
		rgba_t		color;

		// get the text for this control
		binding = UI_Controls_BindingForControl( item->id );
		if( binding->bind1 == -1 )
			com.strncpy( name, "???", sizeof( name ));
		else {
			com.strncpy( name, Key_KeynumToString( binding->bind1 ), sizeof( name ));
			com.strupr( name, name );

			if( binding->bind2 != -1 )
			{
				com.strncpy( name2, Key_KeynumToString( binding->bind2 ), sizeof( name2 ));
				com.strupr( name2, name2 );

				com.strncat( name, " or ", sizeof( name ));
				com.strncat( name, name2, sizeof( name ));
			}
		}

		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuAction_s *)self)->background );

		if( item->flags & QMF_GRAYED )
		{
			UI_DrawString( item->x, item->y, item->width, item->height, name, uiColorDkGrey, true, item->charWidth, item->charHeight, 1, true );
			return; // grayed
		}

		if( item != (menuCommon_s *)UI_ItemAtCursor( item->parent ))
		{
			UI_DrawString( item->x, item->y, item->width, item->height, name, item->color, false, item->charWidth, item->charHeight, 1, true );
			return; // no focus
		}

		UI_DrawString( item->x, item->y, item->width, item->height, name, item->color, false, item->charWidth, item->charHeight, 1, true );

		*(uint *)color = *(uint *)uiColorWhite;
		color[3] = 255 * (0.5 + 0.5 * com.sin( uiStatic.realTime / UI_PULSE_DIVISOR ));

		UI_DrawString( item->x, item->y, item->width, item->height, name, color, false, item->charWidth, item->charHeight, 1, true );
	}
	else
	{
		if( uiControls.menu.items[uiControls.menu.cursor] == self )
			UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOXFOCUS );
		else UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOX );

		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
	}
}

/*
=================
UI_Controls_Init
=================
*/
static void UI_Controls_Init( void )
{
	Mem_Set( &uiControls, 0, sizeof( uiControls_t ));

	uiControls.menu.keyFunc = UI_Controls_KeyFunc;

	uiControls.background.generic.id = ID_BACKGROUND;
	uiControls.background.generic.type = QMTYPE_BITMAP;
	uiControls.background.generic.flags = QMF_INACTIVE;
	uiControls.background.generic.x = 0;
	uiControls.background.generic.y = 0;
	uiControls.background.generic.width = 1024;
	uiControls.background.generic.height = 768;
	uiControls.background.pic = ART_BACKGROUND;

	uiControls.banner.generic.id = ID_BANNER;
	uiControls.banner.generic.type = QMTYPE_BITMAP;
	uiControls.banner.generic.flags = QMF_INACTIVE;
	uiControls.banner.generic.x = 65;
	uiControls.banner.generic.y = 92;
	uiControls.banner.generic.width = 690;
	uiControls.banner.generic.height = 120;
	uiControls.banner.pic = ART_BANNER;

	uiControls.textShadow1.generic.id = ID_TEXTSHADOW1;
	uiControls.textShadow1.generic.type = QMTYPE_BITMAP;
	uiControls.textShadow1.generic.flags = QMF_INACTIVE;
	uiControls.textShadow1.generic.x = 182;
	uiControls.textShadow1.generic.y = 190;
	uiControls.textShadow1.generic.width = 256;
	uiControls.textShadow1.generic.height = 256;
	uiControls.textShadow1.generic.color = uiColorBlack;

	uiControls.textShadow2.generic.id = ID_TEXTSHADOW2;
	uiControls.textShadow2.generic.type = QMTYPE_BITMAP;
	uiControls.textShadow2.generic.flags = QMF_INACTIVE;
	uiControls.textShadow2.generic.x = 182;
	uiControls.textShadow2.generic.y = 446;
	uiControls.textShadow2.generic.width = 256;
	uiControls.textShadow2.generic.height = 256;
	uiControls.textShadow2.generic.color = uiColorBlack;

	uiControls.text1.generic.id = ID_TEXT1;
	uiControls.text1.generic.type = QMTYPE_BITMAP;
	uiControls.text1.generic.flags = QMF_INACTIVE;
	uiControls.text1.generic.x = 180;
	uiControls.text1.generic.y = 188;
	uiControls.text1.generic.width = 256;
	uiControls.text1.generic.height = 256;

	uiControls.text2.generic.id = ID_TEXT2;
	uiControls.text2.generic.type = QMTYPE_BITMAP;
	uiControls.text2.generic.flags = QMF_INACTIVE;
	uiControls.text2.generic.x = 180;
	uiControls.text2.generic.y = 444;
	uiControls.text2.generic.width = 256;
	uiControls.text2.generic.height = 256;

	uiControls.back.generic.id = ID_BACK;
	uiControls.back.generic.type = QMTYPE_BITMAP;
	uiControls.back.generic.x = 413;
	uiControls.back.generic.y = 656;
	uiControls.back.generic.width = 198;
	uiControls.back.generic.height = 38;
	uiControls.back.generic.callback = UI_Controls_Callback;
	uiControls.back.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.back.pic = UI_BACKBUTTON;

	uiControls.look.generic.id = ID_LOOK;
	uiControls.look.generic.type = QMTYPE_BITMAP;
	uiControls.look.generic.x = 102;
	uiControls.look.generic.y = 130;
	uiControls.look.generic.width = 198;
	uiControls.look.generic.height = 30;
	uiControls.look.generic.callback = UI_Controls_Callback;
	uiControls.look.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.look.pic	= ART_LOOK;

	uiControls.move.generic.id = ID_MOVE;
	uiControls.move.generic.type = QMTYPE_BITMAP;
	uiControls.move.generic.x = 310;
	uiControls.move.generic.y = 130;
	uiControls.move.generic.width = 198;
	uiControls.move.generic.height = 30;
	uiControls.move.generic.callback = UI_Controls_Callback;
	uiControls.move.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.move.pic	= ART_MOVE;

	uiControls.shoot.generic.id = ID_SHOOT;
	uiControls.shoot.generic.type	= QMTYPE_BITMAP;
	uiControls.shoot.generic.x = 516;
	uiControls.shoot.generic.y = 130;
	uiControls.shoot.generic.width = 198;
	uiControls.shoot.generic.height = 30;
	uiControls.shoot.generic.callback = UI_Controls_Callback;
	uiControls.shoot.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.shoot.pic = ART_SHOOT;
	
	uiControls.misc.generic.id = ID_MISC;
	uiControls.misc.generic.type = QMTYPE_BITMAP;
	uiControls.misc.generic.x = 724;
	uiControls.misc.generic.y = 130;
	uiControls.misc.generic.width = 198;
	uiControls.misc.generic.height = 30;
	uiControls.misc.generic.callback = UI_Controls_Callback;
	uiControls.misc.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.misc.pic = ART_MISC;

	uiControls.mouseSpeed.generic.id = ID_MOUSESPEED;
	uiControls.mouseSpeed.generic.type = QMTYPE_SPINCONTROL;
	uiControls.mouseSpeed.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiControls.mouseSpeed.generic.x = 580;
	uiControls.mouseSpeed.generic.y = 180;
	uiControls.mouseSpeed.generic.width = 198;
	uiControls.mouseSpeed.generic.height = 30;
	uiControls.mouseSpeed.generic.callback = UI_Controls_Callback;
	uiControls.mouseSpeed.generic.statusText = "Select your mouse speed";
	uiControls.mouseSpeed.minValue = 1;
	uiControls.mouseSpeed.maxValue = 30;
	uiControls.mouseSpeed.range = 1;

	uiControls.smoothMouse.generic.id = ID_SMOOTHMOUSE;
	uiControls.smoothMouse.generic.type = QMTYPE_SPINCONTROL;
	uiControls.smoothMouse.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiControls.smoothMouse.generic.x = 580;
	uiControls.smoothMouse.generic.y = 212;
	uiControls.smoothMouse.generic.width = 198;
	uiControls.smoothMouse.generic.height = 30;
	uiControls.smoothMouse.generic.callback = UI_Controls_Callback;
	uiControls.smoothMouse.generic.statusText	= "Enable/Disable smooth mouse movement";
	uiControls.smoothMouse.minValue = 0;
	uiControls.smoothMouse.maxValue = 1;
	uiControls.smoothMouse.range = 1;

	uiControls.invertMouse.generic.id = ID_INVERTMOUSE;
	uiControls.invertMouse.generic.type = QMTYPE_SPINCONTROL;
	uiControls.invertMouse.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiControls.invertMouse.generic.x = 580;
	uiControls.invertMouse.generic.y = 244;
	uiControls.invertMouse.generic.width = 198;
	uiControls.invertMouse.generic.height = 30;
	uiControls.invertMouse.generic.callback = UI_Controls_Callback;
	uiControls.invertMouse.generic.statusText	= "Invert mouse movement";
	uiControls.invertMouse.minValue = 0;
	uiControls.invertMouse.maxValue = 1;
	uiControls.invertMouse.range = 1;

	uiControls.loopUp.generic.id = ID_LOOKUP;
	uiControls.loopUp.generic.type = QMTYPE_ACTION;
	uiControls.loopUp.generic.x = 580;
	uiControls.loopUp.generic.y = 276;
	uiControls.loopUp.generic.width = 198;
	uiControls.loopUp.generic.height = 30;
	uiControls.loopUp.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.loopUp.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";
	
	uiControls.lookDown.generic.id = ID_LOOKDOWN;
	uiControls.lookDown.generic.type = QMTYPE_ACTION;
	uiControls.lookDown.generic.x = 580;
	uiControls.lookDown.generic.y = 308;
	uiControls.lookDown.generic.width = 198;
	uiControls.lookDown.generic.height = 30;
	uiControls.lookDown.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.lookDown.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";
	
	uiControls.mouseLook.generic.id = ID_MOUSELOOK;
	uiControls.mouseLook.generic.type = QMTYPE_ACTION;
	uiControls.mouseLook.generic.x = 580;
	uiControls.mouseLook.generic.y = 340;
	uiControls.mouseLook.generic.width = 198;
	uiControls.mouseLook.generic.height = 30;
	uiControls.mouseLook.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.mouseLook.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.freeLook.generic.id = ID_FREELOOK;
	uiControls.freeLook.generic.type = QMTYPE_SPINCONTROL;
	uiControls.freeLook.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiControls.freeLook.generic.x = 580;
	uiControls.freeLook.generic.y = 372;
	uiControls.freeLook.generic.width = 198;
	uiControls.freeLook.generic.height = 30;
	uiControls.freeLook.generic.callback = UI_Controls_Callback;
	uiControls.freeLook.generic.statusText = "Enable/Disable free mouse looking";
	uiControls.freeLook.minValue = 0;
	uiControls.freeLook.maxValue = 1;
	uiControls.freeLook.range = 1;

	uiControls.centerView.generic.id = ID_CENTERVIEW;
	uiControls.centerView.generic.type = QMTYPE_ACTION;
	uiControls.centerView.generic.x = 580;
	uiControls.centerView.generic.y = 404;
	uiControls.centerView.generic.width = 198;
	uiControls.centerView.generic.height = 30;
	uiControls.centerView.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.centerView.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.zoomView.generic.id = ID_ZOOMVIEW;
	uiControls.zoomView.generic.type = QMTYPE_ACTION;
	uiControls.zoomView.generic.x = 580;
	uiControls.zoomView.generic.y = 436;
	uiControls.zoomView.generic.width = 198;
	uiControls.zoomView.generic.height = 30;
	uiControls.zoomView.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.zoomView.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.alwaysRun.generic.id = ID_ALWAYSRUN;
	uiControls.alwaysRun.generic.type = QMTYPE_SPINCONTROL;
	uiControls.alwaysRun.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiControls.alwaysRun.generic.x = 580;
	uiControls.alwaysRun.generic.y = 180;
	uiControls.alwaysRun.generic.width = 198;
	uiControls.alwaysRun.generic.height = 30;
	uiControls.alwaysRun.generic.callback = UI_Controls_Callback;
	uiControls.alwaysRun.generic.statusText = "Always run in-game";
	uiControls.alwaysRun.minValue = 0;
	uiControls.alwaysRun.maxValue = 1;
	uiControls.alwaysRun.range = 1;

	uiControls.run.generic.id = ID_RUN;
	uiControls.run.generic.type = QMTYPE_ACTION;
	uiControls.run.generic.x = 580;
	uiControls.run.generic.y = 212;
	uiControls.run.generic.width = 198;
	uiControls.run.generic.height = 30;
	uiControls.run.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.run.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";
	
	uiControls.forward.generic.id = ID_FORWARD;
	uiControls.forward.generic.type = QMTYPE_ACTION;
	uiControls.forward.generic.x = 580;
	uiControls.forward.generic.y = 244;
	uiControls.forward.generic.width = 198;
	uiControls.forward.generic.height = 30;
	uiControls.forward.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.forward.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.backward.generic.id = ID_BACKWARD;
	uiControls.backward.generic.type = QMTYPE_ACTION;
	uiControls.backward.generic.x = 580;
	uiControls.backward.generic.y = 276;
	uiControls.backward.generic.width = 198;
	uiControls.backward.generic.height = 30;
	uiControls.backward.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.backward.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.stepLeft.generic.id = ID_STEPLEFT;
	uiControls.stepLeft.generic.type = QMTYPE_ACTION;
	uiControls.stepLeft.generic.x = 580;
	uiControls.stepLeft.generic.y = 308;
	uiControls.stepLeft.generic.width = 198;
	uiControls.stepLeft.generic.height = 30;
	uiControls.stepLeft.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.stepLeft.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.stepRight.generic.id = ID_STEPRIGHT;
	uiControls.stepRight.generic.type = QMTYPE_ACTION;
	uiControls.stepRight.generic.x = 580;
	uiControls.stepRight.generic.y = 340;
	uiControls.stepRight.generic.width = 198;
	uiControls.stepRight.generic.height = 30;
	uiControls.stepRight.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.stepRight.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.jump.generic.id = ID_JUMP;
	uiControls.jump.generic.type = QMTYPE_ACTION;
	uiControls.jump.generic.x = 580;
	uiControls.jump.generic.y = 372;
	uiControls.jump.generic.width = 198;
	uiControls.jump.generic.height = 30;
	uiControls.jump.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.jump.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.crouch.generic.id = ID_CROUCH;
	uiControls.crouch.generic.type = QMTYPE_ACTION;
	uiControls.crouch.generic.x = 580;
	uiControls.crouch.generic.y = 404;
	uiControls.crouch.generic.width = 198;
	uiControls.crouch.generic.height = 30;
	uiControls.crouch.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.crouch.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.left.generic.id = ID_LEFT;
	uiControls.left.generic.type = QMTYPE_ACTION;
	uiControls.left.generic.x = 580;
	uiControls.left.generic.y = 436;
	uiControls.left.generic.width = 198;
	uiControls.left.generic.height = 30;
	uiControls.left.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.left.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";
	
	uiControls.right.generic.id = ID_RIGHT;
	uiControls.right.generic.type = QMTYPE_ACTION;
	uiControls.right.generic.x = 580;
	uiControls.right.generic.y = 468;
	uiControls.right.generic.width = 198;
	uiControls.right.generic.height = 30;
	uiControls.right.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.right.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.sideStep.generic.id = ID_SIDESTEP;
	uiControls.sideStep.generic.type = QMTYPE_ACTION;
	uiControls.sideStep.generic.x = 580;
	uiControls.sideStep.generic.y = 500;
	uiControls.sideStep.generic.width = 198;
	uiControls.sideStep.generic.height = 30;
	uiControls.sideStep.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.sideStep.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.attack.generic.id = ID_ATTACK;
	uiControls.attack.generic.type = QMTYPE_ACTION;
	uiControls.attack.generic.x = 580;
	uiControls.attack.generic.y = 180;
	uiControls.attack.generic.width = 198;
	uiControls.attack.generic.height = 30;
	uiControls.attack.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.attack.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.nextWeap.generic.id = ID_NEXTWEAP;
	uiControls.nextWeap.generic.type = QMTYPE_ACTION;
	uiControls.nextWeap.generic.x = 580;
	uiControls.nextWeap.generic.y = 212;
	uiControls.nextWeap.generic.width = 198;
	uiControls.nextWeap.generic.height = 30;
	uiControls.nextWeap.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.nextWeap.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.prevWeap.generic.id = ID_PREVWEAP;
	uiControls.prevWeap.generic.type = QMTYPE_ACTION;
	uiControls.prevWeap.generic.x = 580;
	uiControls.prevWeap.generic.y = 244;
	uiControls.prevWeap.generic.width = 198;
	uiControls.prevWeap.generic.height = 30;
	uiControls.prevWeap.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.prevWeap.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.blaster.generic.id = ID_BLASTER;
	uiControls.blaster.generic.type = QMTYPE_ACTION;
	uiControls.blaster.generic.x = 580;
	uiControls.blaster.generic.y = 276;
	uiControls.blaster.generic.width = 198;
	uiControls.blaster.generic.height = 30;
	uiControls.blaster.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.blaster.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.shotgun.generic.id = ID_SHOTGUN;
	uiControls.shotgun.generic.type = QMTYPE_ACTION;
	uiControls.shotgun.generic.x = 580;
	uiControls.shotgun.generic.y = 308;
	uiControls.shotgun.generic.width = 198;
	uiControls.shotgun.generic.height = 30;
	uiControls.shotgun.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.shotgun.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.superShotgun.generic.id = ID_SUPERSHOTGUN;
	uiControls.superShotgun.generic.type = QMTYPE_ACTION;
	uiControls.superShotgun.generic.x = 580;
	uiControls.superShotgun.generic.y = 340;
	uiControls.superShotgun.generic.width = 198;
	uiControls.superShotgun.generic.height = 30;
	uiControls.superShotgun.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.superShotgun.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.machinegun.generic.id = ID_MACHINEGUN;
	uiControls.machinegun.generic.type = QMTYPE_ACTION;
	uiControls.machinegun.generic.x = 580;
	uiControls.machinegun.generic.y = 372;
	uiControls.machinegun.generic.width = 198;
	uiControls.machinegun.generic.height = 30;
	uiControls.machinegun.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.machinegun.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.chaingun.generic.id = ID_CHAINGUN;
	uiControls.chaingun.generic.type = QMTYPE_ACTION;
	uiControls.chaingun.generic.x = 580;
	uiControls.chaingun.generic.y = 404;
	uiControls.chaingun.generic.width = 198;
	uiControls.chaingun.generic.height = 30;
	uiControls.chaingun.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.chaingun.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.grenadeLauncher.generic.id = ID_GRENADELAUNCHER;
	uiControls.grenadeLauncher.generic.type = QMTYPE_ACTION;
	uiControls.grenadeLauncher.generic.x = 580;
	uiControls.grenadeLauncher.generic.y = 436;
	uiControls.grenadeLauncher.generic.width = 198;
	uiControls.grenadeLauncher.generic.height = 30;
	uiControls.grenadeLauncher.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.grenadeLauncher.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.rocketLauncher.generic.id = ID_ROCKETLAUNCHER;
	uiControls.rocketLauncher.generic.type = QMTYPE_ACTION;
	uiControls.rocketLauncher.generic.x = 580;
	uiControls.rocketLauncher.generic.y = 468;
	uiControls.rocketLauncher.generic.width = 198;
	uiControls.rocketLauncher.generic.height = 30;
	uiControls.rocketLauncher.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.rocketLauncher.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.hyperBlaster.generic.id = ID_HYPERBLASTER;
	uiControls.hyperBlaster.generic.type = QMTYPE_ACTION;
	uiControls.hyperBlaster.generic.x = 580;
	uiControls.hyperBlaster.generic.y = 500;
	uiControls.hyperBlaster.generic.width = 198;
	uiControls.hyperBlaster.generic.height = 30;
	uiControls.hyperBlaster.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.hyperBlaster.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.railgun.generic.id = ID_RAILGUN;
	uiControls.railgun.generic.type = QMTYPE_ACTION;
	uiControls.railgun.generic.x = 580;
	uiControls.railgun.generic.y = 532;
	uiControls.railgun.generic.width = 198;
	uiControls.railgun.generic.height = 30;
	uiControls.railgun.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.railgun.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.bfg.generic.id = ID_BFG;
	uiControls.bfg.generic.type = QMTYPE_ACTION;
	uiControls.bfg.generic.x = 580;
	uiControls.bfg.generic.y = 564;
	uiControls.bfg.generic.width = 198;
	uiControls.bfg.generic.height = 30;
	uiControls.bfg.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.bfg.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.showScores.generic.id = ID_SHOWSCORES;
	uiControls.showScores.generic.type = QMTYPE_ACTION;
	uiControls.showScores.generic.x = 580;
	uiControls.showScores.generic.y = 180;
	uiControls.showScores.generic.width = 198;
	uiControls.showScores.generic.height = 30;
	uiControls.showScores.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.showScores.generic.statusText	= "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.useItem.generic.id = ID_USEITEM;
	uiControls.useItem.generic.type = QMTYPE_ACTION;
	uiControls.useItem.generic.x = 580;
	uiControls.useItem.generic.y = 212;
	uiControls.useItem.generic.width = 198;
	uiControls.useItem.generic.height = 30;
	uiControls.useItem.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.useItem.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.nextItem.generic.id = ID_NEXTITEM;
	uiControls.nextItem.generic.type = QMTYPE_ACTION;
	uiControls.nextItem.generic.x = 580;
	uiControls.nextItem.generic.y = 244;
	uiControls.nextItem.generic.width = 198;
	uiControls.nextItem.generic.height = 30;
	uiControls.nextItem.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.nextItem.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.prevItem.generic.id = ID_PREVITEM;
	uiControls.prevItem.generic.type = QMTYPE_ACTION;
	uiControls.prevItem.generic.x = 580;
	uiControls.prevItem.generic.y = 276;
	uiControls.prevItem.generic.width = 198;
	uiControls.prevItem.generic.height = 30;
	uiControls.prevItem.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.prevItem.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.dropItem.generic.id = ID_DROPITEM;
	uiControls.dropItem.generic.type = QMTYPE_ACTION;
	uiControls.dropItem.generic.x = 580;
	uiControls.dropItem.generic.y = 308;
	uiControls.dropItem.generic.width = 198;
	uiControls.dropItem.generic.height = 30;
	uiControls.dropItem.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.dropItem.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.fieldComputer.generic.id = ID_FIELDCOMPUTER;
	uiControls.fieldComputer.generic.type = QMTYPE_ACTION;
	uiControls.fieldComputer.generic.x = 580;
	uiControls.fieldComputer.generic.y = 340;
	uiControls.fieldComputer.generic.width = 198;
	uiControls.fieldComputer.generic.height = 30;
	uiControls.fieldComputer.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.fieldComputer.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.inventory.generic.id = ID_INVENTORY;
	uiControls.inventory.generic.type = QMTYPE_ACTION;
	uiControls.inventory.generic.x = 580;
	uiControls.inventory.generic.y = 372;
	uiControls.inventory.generic.width = 198;
	uiControls.inventory.generic.height = 30;
	uiControls.inventory.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.inventory.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.flipoff.generic.id = ID_FLIPOFF;
	uiControls.flipoff.generic.type = QMTYPE_ACTION;
	uiControls.flipoff.generic.x = 580;
	uiControls.flipoff.generic.y = 404;
	uiControls.flipoff.generic.width = 198;
	uiControls.flipoff.generic.height = 30;
	uiControls.flipoff.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.flipoff.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.salute.generic.id = ID_SALUTE;
	uiControls.salute.generic.type = QMTYPE_ACTION;
	uiControls.salute.generic.x = 580;
	uiControls.salute.generic.y = 436;
	uiControls.salute.generic.width = 198;
	uiControls.salute.generic.height = 30;
	uiControls.salute.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.salute.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";
	
	uiControls.taunt.generic.id = ID_TAUNT;
	uiControls.taunt.generic.type = QMTYPE_ACTION;
	uiControls.taunt.generic.x = 580;
	uiControls.taunt.generic.y = 468;
	uiControls.taunt.generic.width = 198;
	uiControls.taunt.generic.height = 30;
	uiControls.taunt.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.taunt.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.wave.generic.id = ID_WAVE;
	uiControls.wave.generic.type = QMTYPE_ACTION;
	uiControls.wave.generic.x = 580;
	uiControls.wave.generic.y = 500;
	uiControls.wave.generic.width = 198;
	uiControls.wave.generic.height = 30;
	uiControls.wave.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.wave.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.point.generic.id = ID_POINT;
	uiControls.point.generic.type = QMTYPE_ACTION;
	uiControls.point.generic.x = 580;
	uiControls.point.generic.y = 532;
	uiControls.point.generic.width = 198;
	uiControls.point.generic.height = 30;
	uiControls.point.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.point.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.chat.generic.id = ID_CHAT;
	uiControls.chat.generic.type = QMTYPE_ACTION;
	uiControls.chat.generic.x = 580;
	uiControls.chat.generic.y = 564;
	uiControls.chat.generic.width = 198;
	uiControls.chat.generic.height = 30;
	uiControls.chat.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.chat.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	uiControls.chatTeam.generic.id = ID_CHATTEAM;
	uiControls.chatTeam.generic.type = QMTYPE_ACTION;
	uiControls.chatTeam.generic.x = 580;
	uiControls.chatTeam.generic.y = 596;
	uiControls.chatTeam.generic.width = 198;
	uiControls.chatTeam.generic.height = 30;
	uiControls.chatTeam.generic.ownerdraw = UI_Controls_Ownerdraw;
	uiControls.chatTeam.generic.statusText = "Press ENTER or click to change. Press BACKSPACE to clear.";

	UI_Controls_GetConfig();
	
	UI_AddItem( &uiControls.menu, (void *)&uiControls.background );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.banner );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.textShadow1 );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.textShadow2 );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.text1 );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.text2 );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.back );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.look );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.move );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.shoot );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.misc );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.mouseSpeed );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.smoothMouse );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.invertMouse );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.loopUp );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.lookDown );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.mouseLook );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.freeLook );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.centerView );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.zoomView );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.alwaysRun );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.run );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.forward );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.backward );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.stepLeft );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.stepRight );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.jump );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.crouch );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.left );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.right );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.sideStep );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.attack );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.nextWeap );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.prevWeap );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.blaster );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.shotgun );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.superShotgun );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.machinegun );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.chaingun );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.grenadeLauncher );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.rocketLauncher );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.hyperBlaster );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.railgun );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.bfg );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.showScores );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.useItem );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.nextItem );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.prevItem );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.dropItem );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.fieldComputer );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.inventory );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.flipoff );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.salute );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.taunt );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.wave );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.point );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.chat );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.chatTeam );
}

/*
=================
UI_Controls_Precache
=================
*/
void UI_Controls_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT_LOOK, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT_LOOK2, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT_MOVE, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT_MOVE2, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT_SHOOT, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT_SHOOT2, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT_MISC, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT_MISC2, SHADER_NOMIP );
	re->RegisterShader( ART_LOOK, SHADER_NOMIP );
	re->RegisterShader( ART_MOVE, SHADER_NOMIP );
	re->RegisterShader( ART_SHOOT, SHADER_NOMIP );
	re->RegisterShader( ART_MISC, SHADER_NOMIP );
}

/*
=================
UI_Controls_Menu
=================
*/
void UI_Controls_Menu( void )
{
	UI_Controls_Precache();
	UI_Controls_Init();

	UI_Controls_UpdateConfig();
	UI_PushMenu( &uiControls.menu );
}