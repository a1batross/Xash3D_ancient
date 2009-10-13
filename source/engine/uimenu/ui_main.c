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
#include "client.h"

#define ART_BACKGROUND	"gfx/shell/splash"
#define ART_MINIMIZE_N	"gfx/shell/min_n"
#define ART_MINIMIZE_F	"gfx/shell/min_f"
#define ART_CLOSEBTN_N	"gfx/shell/cls_n"
#define ART_CLOSEBTN_F	"gfx/shell/cls_f"

#define ID_BACKGROUND	0
#define ID_CONSOLE		1
#define ID_NEWGAME		2
#define ID_CONFIGURATION	3
#define ID_SAVERESTORE	4	
#define ID_MULTIPLAYER	5
#define ID_CUSTOMGAME	6
#define ID_CREDITS		7
#define ID_QUIT		8
#define ID_MINIMIZE		9
#define ID_COPYRIGHT	11
#define ID_MSGBOX	 	12
#define ID_YES	 	13
#define ID_NO	 	14

typedef struct
{
	menuFramework_s	menu;

	menuBitmap_s	background;
	menuAction_s	console;
	menuAction_s	newGame;
	menuAction_s	configuration;
	menuAction_s	saveRestore;
	menuAction_s	multiPlayer;
	menuAction_s	customGame;
	menuAction_s	credits;
	menuAction_s	quit;

	menuBitmap_s	minimizeBtn;
	menuBitmap_s	quitButton;

	// quit dialog
	menuBitmap_s	msgBox;
	menuAction_s	quitMessage;
	menuAction_s	yes;
	menuAction_s	no;
} uiMain_t;

static uiMain_t		uiMain;

/*
=================
UI_Main_KeyFunc
=================
*/
static const char *UI_Main_KeyFunc( int key )
{
	switch( key )
	{
	case K_ESCAPE:
	case K_MOUSE2:
		if( cls.state == ca_active )
		{
			// this shouldn't be happening, but for some reason it is in some mods
			UI_CloseMenu();
			return uiSoundNull;
		}
		return uiSoundNull;
	}
	return UI_DefaultKey( &uiMain.menu, key );
}

/*
=================
UI_MsgBox_Ownerdraw
=================
*/
static void UI_MsgBox_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	UI_FillRect( item->x, item->y, item->width, item->height, uiColorDkGrey );
}

static void UI_QuitDialog( void )
{
	// toggle main menu between active\inactive
	// show\hide quit dialog
	uiMain.console.generic.flags ^= QMF_INACTIVE; 
	uiMain.newGame.generic.flags ^= QMF_INACTIVE;
	uiMain.saveRestore.generic.flags ^= QMF_INACTIVE;
	uiMain.configuration.generic.flags ^= QMF_INACTIVE;
	uiMain.multiPlayer.generic.flags ^= QMF_INACTIVE;
	uiMain.customGame.generic.flags ^= QMF_INACTIVE;
	uiMain.credits.generic.flags ^= QMF_INACTIVE;
	uiMain.quit.generic.flags ^= QMF_INACTIVE;
	uiMain.minimizeBtn.generic.flags ^= QMF_INACTIVE;
	uiMain.quitButton.generic.flags ^= QMF_INACTIVE;

	uiMain.msgBox.generic.flags ^= QMF_HIDDEN;
	uiMain.quitMessage.generic.flags ^= QMF_HIDDEN;
	uiMain.no.generic.flags ^= QMF_HIDDEN;
	uiMain.yes.generic.flags ^= QMF_HIDDEN;

}

/*
=================
UI_Main_Callback
=================
*/
static void UI_Main_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_CONSOLE:
		UI_SetActiveMenu( UI_CLOSEMENU );
		cls.key_dest = key_console;
		break;
	case ID_NEWGAME:
		UI_NewGame_Menu();
		break;
	case ID_MULTIPLAYER:
		UI_MultiPlayer_Menu();
		break;
	case ID_CONFIGURATION:
		UI_Options_Menu();
		break;
	case ID_SAVERESTORE:
		UI_LoadGame_Menu(); // FIXME
		break;
	case ID_CUSTOMGAME:
		UI_Mods_Menu();
		break;
	case ID_CREDITS:
		UI_Credits_Menu();
		break;
	case ID_QUIT:
		UI_QuitDialog();
		break;
	case ID_MINIMIZE:
		Cbuf_ExecuteText( EXEC_APPEND, "minimize\n" );
		break;
	case ID_YES:
		Cbuf_ExecuteText( EXEC_APPEND, "quit\n" );
		break;
	case ID_NO:
		UI_QuitDialog();
		break;
	}
}

/*
=================
UI_Main_Init
=================
*/
static void UI_Main_Init( void )
{
	Mem_Set( &uiMain, 0, sizeof( uiMain_t ));

	uiMain.menu.keyFunc	= UI_Main_KeyFunc;

	uiMain.background.generic.id = ID_BACKGROUND;
	uiMain.background.generic.type = QMTYPE_BITMAP;
	uiMain.background.generic.flags = QMF_INACTIVE;
	uiMain.background.generic.x = 0;
	uiMain.background.generic.y = 0;
	uiMain.background.generic.width = 1024;
	uiMain.background.generic.height = 768;
	uiMain.background.pic = ART_BACKGROUND;

	uiMain.console.generic.id = ID_CONSOLE;
	uiMain.console.generic.type = QMTYPE_ACTION;
	uiMain.console.generic.name = "Console";
	uiMain.console.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiMain.console.generic.x = 72;
	uiMain.console.generic.y = 180;
	uiMain.console.generic.callback = UI_Main_Callback;

	uiMain.newGame.generic.id = ID_NEWGAME;
	uiMain.newGame.generic.type = QMTYPE_ACTION;
	uiMain.newGame.generic.name = "New game";
	uiMain.newGame.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiMain.newGame.generic.statusText = "Start a new game.";
	uiMain.newGame.generic.x = 72;
	uiMain.newGame.generic.y = 230;
	uiMain.newGame.generic.callback = UI_Main_Callback;

	uiMain.saveRestore.generic.id = ID_SAVERESTORE;
	uiMain.saveRestore.generic.type = QMTYPE_ACTION;
	uiMain.saveRestore.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	if( cls.state == ca_active )
	{
		uiMain.saveRestore.generic.name = "Save\\Load Game";
		uiMain.saveRestore.generic.statusText = "Load a saved game, save the current game.";
	}
	else
	{
		uiMain.saveRestore.generic.name = "Load Game";
		uiMain.saveRestore.generic.statusText = "Load a previously saved game.";
	}
	uiMain.saveRestore.generic.x = 72;
	uiMain.saveRestore.generic.y = 280;
	uiMain.saveRestore.generic.callback = UI_Main_Callback;

	uiMain.configuration.generic.id = ID_CONFIGURATION;
	uiMain.configuration.generic.type = QMTYPE_ACTION;
	uiMain.configuration.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiMain.configuration.generic.name = "Configuration";
	uiMain.configuration.generic.statusText = "Change game settings, configure controls";
	uiMain.configuration.generic.x = 72;
	uiMain.configuration.generic.y = 330;
	uiMain.configuration.generic.callback = UI_Main_Callback;

	uiMain.multiPlayer.generic.id = ID_MULTIPLAYER;
	uiMain.multiPlayer.generic.type = QMTYPE_ACTION;
	uiMain.multiPlayer.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiMain.multiPlayer.generic.name = "Multiplayer";
	uiMain.multiPlayer.generic.statusText = "Search for internet servers, configure character";
	uiMain.multiPlayer.generic.x = 72;
	uiMain.multiPlayer.generic.y = 380;
	uiMain.multiPlayer.generic.callback = UI_Main_Callback;

	uiMain.customGame.generic.id = ID_CUSTOMGAME;
	uiMain.customGame.generic.type = QMTYPE_ACTION;
	uiMain.customGame.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiMain.customGame.generic.name = "Custom Game";
	uiMain.customGame.generic.statusText = "Select a custom game";
	uiMain.customGame.generic.x = 72;
	uiMain.customGame.generic.y = 430;
	uiMain.customGame.generic.callback = UI_Main_Callback;

	uiMain.credits.generic.id = ID_CREDITS;
	uiMain.credits.generic.type = QMTYPE_ACTION;
	uiMain.credits.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiMain.credits.generic.name = "About";
	uiMain.credits.generic.statusText = "Game credits";
	uiMain.credits.generic.x = 72;
	uiMain.credits.generic.y = 480;
	uiMain.credits.generic.callback = UI_Main_Callback;

	uiMain.quit.generic.id = ID_QUIT;
	uiMain.quit.generic.type = QMTYPE_ACTION;
	uiMain.quit.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiMain.quit.generic.name = "Quit";
	uiMain.quit.generic.statusText = "Quit from game";
	uiMain.quit.generic.x = 72;
	uiMain.quit.generic.y = 530;
	uiMain.quit.generic.callback = UI_Main_Callback;

	uiMain.minimizeBtn.generic.id = ID_MINIMIZE;
	uiMain.minimizeBtn.generic.type = QMTYPE_BITMAP;
	uiMain.minimizeBtn.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_MOUSEONLY;
	uiMain.minimizeBtn.generic.x = 952;
	uiMain.minimizeBtn.generic.y = 13;
	uiMain.minimizeBtn.generic.width = 32;
	uiMain.minimizeBtn.generic.height = 32;
	uiMain.minimizeBtn.generic.callback = UI_Main_Callback;
	uiMain.minimizeBtn.pic = ART_MINIMIZE_N;
	uiMain.minimizeBtn.focusPic = ART_MINIMIZE_F;

	uiMain.quitButton.generic.id = ID_QUIT;
	uiMain.quitButton.generic.type = QMTYPE_BITMAP;
	uiMain.quitButton.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_MOUSEONLY;
	uiMain.quitButton.generic.x = 984;
	uiMain.quitButton.generic.y = 13;
	uiMain.quitButton.generic.width = 32;
	uiMain.quitButton.generic.height = 32;
	uiMain.quitButton.generic.callback = UI_Main_Callback;
	uiMain.quitButton.pic = ART_CLOSEBTN_N;
	uiMain.quitButton.focusPic = ART_CLOSEBTN_F;

	uiMain.msgBox.generic.id = ID_MSGBOX;
	uiMain.msgBox.generic.type = QMTYPE_BITMAP;
	uiMain.msgBox.generic.flags = QMF_INACTIVE|QMF_HIDDEN;
	uiMain.msgBox.generic.ownerdraw = UI_MsgBox_Ownerdraw; // just a fill rectangle
	uiMain.msgBox.generic.x = 192;
	uiMain.msgBox.generic.y = 256;
	uiMain.msgBox.generic.width = 640;
	uiMain.msgBox.generic.height = 256;

	uiMain.quitMessage.generic.id = ID_MSGBOX;
	uiMain.quitMessage.generic.type = QMTYPE_ACTION;
	uiMain.quitMessage.generic.flags = QMF_INACTIVE|QMF_HIDDEN;
	uiMain.quitMessage.generic.name = "Are you sure you want to quit?";
	uiMain.quitMessage.generic.x = 248;
	uiMain.quitMessage.generic.y = 280;

	uiMain.yes.generic.id = ID_YES;
	uiMain.yes.generic.type = QMTYPE_ACTION;
	uiMain.yes.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_HIDDEN;
	uiMain.yes.generic.name = "Ok";
	uiMain.yes.generic.x = 380;
	uiMain.yes.generic.y = 460;
	uiMain.yes.generic.callback = UI_Main_Callback;

	uiMain.no.generic.id = ID_NO;
	uiMain.no.generic.type = QMTYPE_ACTION;
	uiMain.no.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_HIDDEN;
	uiMain.no.generic.name = "Cancel";
	uiMain.no.generic.x = 530;
	uiMain.no.generic.y = 460;
	uiMain.no.generic.callback = UI_Main_Callback;

	UI_AddItem( &uiMain.menu, (void *)&uiMain.background );
	if( host.developer ) UI_AddItem( &uiMain.menu, (void *)&uiMain.console );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.newGame );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.saveRestore );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.configuration );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.multiPlayer );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.customGame );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.credits );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.quit );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.minimizeBtn );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.quitButton );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.msgBox );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.quitMessage );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.no );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.yes );
}

/*
=================
UI_Main_Precache
=================
*/
void UI_Main_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_MINIMIZE_N, SHADER_NOMIP );
	re->RegisterShader( ART_MINIMIZE_F, SHADER_NOMIP );
	re->RegisterShader( ART_CLOSEBTN_N, SHADER_NOMIP );
	re->RegisterShader( ART_CLOSEBTN_F, SHADER_NOMIP );
}

/*
=================
UI_Main_Menu
=================
*/
void UI_Main_Menu( void )
{
	if( cls.state == ca_active )
	{
		// this shouldn't be happening, but for some reason it is in some mods
		UI_InGame_Menu();
		return;
	}

	UI_Main_Precache();
	UI_Main_Init();

	UI_PushMenu( &uiMain.menu );
}