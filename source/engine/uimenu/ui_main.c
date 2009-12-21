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
#include "com_library.h"
#include "ui_local.h"
#include "input.h"
#include "client.h"

#define ART_MINIMIZE_N	"gfx/shell/min_n"
#define ART_MINIMIZE_F	"gfx/shell/min_f"
#define ART_CLOSEBTN_N	"gfx/shell/cls_n"
#define ART_CLOSEBTN_F	"gfx/shell/cls_f"

#define ID_BACKGROUND	0
#define ID_CONSOLE		1
#define ID_RESUME		2
#define ID_NEWGAME		3
#define ID_HAZARDCOURSE	4
#define ID_CONFIGURATION	5
#define ID_SAVERESTORE	6	
#define ID_MULTIPLAYER	7
#define ID_CUSTOMGAME	8
#define ID_CREDITS		9
#define ID_QUIT		10
#define ID_MINIMIZE		11
#define ID_MSGBOX	 	12
#define ID_MSGTEXT	 	13
#define ID_YES	 	14
#define ID_NO	 	15

typedef struct
{
	menuFramework_s	menu;

	menuBitmap_s	background;
	menuAction_s	console;
	menuAction_s	resumeGame;
	menuAction_s	newGame;
	menuAction_s	hazardCourse;
	menuAction_s	configuration;
	menuAction_s	saveRestore;
	menuAction_s	multiPlayer;
	menuAction_s	customGame;
	menuAction_s	credits;
	menuAction_s	quit;

	menuBitmap_s	minimizeBtn;
	menuBitmap_s	quitButton;

	// quit dialog
	menuAction_s	msgBox;
	menuAction_s	quitMessage;
	menuAction_s	yes;
	menuAction_s	no;
} uiMain_t;

static uiMain_t		uiMain;

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
	uiMain.resumeGame.generic.flags ^= QMF_INACTIVE;
	uiMain.newGame.generic.flags ^= QMF_INACTIVE;
	uiMain.hazardCourse.generic.flags ^= QMF_INACTIVE;
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
UI_Main_KeyFunc
=================
*/
static const char *UI_Main_KeyFunc( int key )
{
	switch( key )
	{
	case K_ESCAPE:
		if( cls.state == ca_active )
			UI_CloseMenu();
		else UI_QuitDialog ();
		return uiSoundNull;
	}
	return UI_DefaultKey( &uiMain.menu, key );
}

/*
=================
UI_Main_HazardCourse
=================
*/
static void UI_Main_HazardCourse( void )
{
	Cvar_SetValue( "skill", 0.0f );
	Cvar_SetValue( "deathmatch", 0.0f );
	Cvar_SetValue( "gamerules", 0.0f );
	Cvar_SetValue( "teamplay", 0.0f );
	Cvar_SetValue( "pausable", 1.0f ); // singleplayer is always allowing pause
	Cvar_SetValue( "coop", 0.0f );

	Cbuf_ExecuteText( EXEC_APPEND, va( "loading; killserver; wait; map %s\n", GI->trainmap ));
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
	case ID_RESUME:
		UI_CloseMenu();
		break;
	case ID_NEWGAME:
		UI_NewGame_Menu();
		break;
	case ID_HAZARDCOURSE:
		UI_Main_HazardCourse();
		break;
	case ID_MULTIPLAYER:
		UI_MultiPlayer_Menu();
		break;
	case ID_CONFIGURATION:
		UI_Options_Menu();
		break;
	case ID_SAVERESTORE:
		if( cls.state == ca_active )
			UI_SaveLoad_Menu();
		else UI_LoadGame_Menu();
		break;
	case ID_CUSTOMGAME:
		UI_CustomGame_Menu();
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
	string	libpath;
	
	Mem_Set( &uiMain, 0, sizeof( uiMain_t ));

	uiMain.menu.keyFunc	= UI_Main_KeyFunc;

	uiMain.background.generic.id = ID_BACKGROUND;
	uiMain.background.generic.type = QMTYPE_BITMAP;
	uiMain.background.generic.flags = QMF_INACTIVE;
	uiMain.background.generic.x = 0;
	uiMain.background.generic.y = 0;
	uiMain.background.generic.width = 1024;
	uiMain.background.generic.height = 768;
	uiMain.background.pic = ART_MAIN_SPLASH;

	uiMain.console.generic.id = ID_CONSOLE;
	uiMain.console.generic.type = QMTYPE_ACTION;
	uiMain.console.generic.name = "Console";
	uiMain.console.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiMain.console.generic.x = 72;
	uiMain.console.generic.y = (cls.state == ca_active) ? 130 : 180;
	uiMain.console.generic.callback = UI_Main_Callback;

	uiMain.resumeGame.generic.id = ID_RESUME;
	uiMain.resumeGame.generic.type = QMTYPE_ACTION;
	uiMain.resumeGame.generic.name = "Resume game";
	uiMain.resumeGame.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	uiMain.resumeGame.generic.statusText = "Return to game.";
	uiMain.resumeGame.generic.x = 72;
	uiMain.resumeGame.generic.y = 180;
	uiMain.resumeGame.generic.callback = UI_Main_Callback;

	uiMain.newGame.generic.id = ID_NEWGAME;
	uiMain.newGame.generic.type = QMTYPE_ACTION;
	uiMain.newGame.generic.name = "New game";
	uiMain.newGame.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	uiMain.newGame.generic.statusText = "Start a new game.";
	uiMain.newGame.generic.x = 72;
	uiMain.newGame.generic.y = 230;
	uiMain.newGame.generic.callback = UI_Main_Callback;

	uiMain.hazardCourse.generic.id = ID_HAZARDCOURSE;
	uiMain.hazardCourse.generic.type = QMTYPE_ACTION;
	uiMain.hazardCourse.generic.name = "Hazard course";
	uiMain.hazardCourse.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	uiMain.hazardCourse.generic.statusText = "Learn how to play the game";
	uiMain.hazardCourse.generic.x = 72;
	uiMain.hazardCourse.generic.y = 280;
	uiMain.hazardCourse.generic.callback = UI_Main_Callback;

	uiMain.saveRestore.generic.id = ID_SAVERESTORE;
	uiMain.saveRestore.generic.type = QMTYPE_ACTION;
	uiMain.saveRestore.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;

	// server.dll needs for reading savefiles or startup newgame
	Com_BuildPath( "server", libpath );
	if( !FS_FileExists( libpath ))
	{
		uiMain.saveRestore.generic.flags |= QMF_GRAYED;
		uiMain.newGame.generic.flags |= QMF_GRAYED;
	}

	if( cls.state == ca_active )
	{
		uiMain.saveRestore.generic.name = "Save\\Load Game";
		uiMain.saveRestore.generic.statusText = "Load a saved game, save the current game.";
	}
	else
	{
		uiMain.saveRestore.generic.name = "Load Game";
		uiMain.saveRestore.generic.statusText = "Load a previously saved game.";
		uiMain.resumeGame.generic.flags |= QMF_HIDDEN;
	}
	uiMain.saveRestore.generic.x = 72;
	uiMain.saveRestore.generic.y = com.strlen( GI->trainmap ) ? 330 : 280;
	uiMain.saveRestore.generic.callback = UI_Main_Callback;

	uiMain.configuration.generic.id = ID_CONFIGURATION;
	uiMain.configuration.generic.type = QMTYPE_ACTION;
	uiMain.configuration.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	uiMain.configuration.generic.name = "Configuration";
	uiMain.configuration.generic.statusText = "Change game settings, configure controls";
	uiMain.configuration.generic.x = 72;
	uiMain.configuration.generic.y = com.strlen( GI->trainmap ) ? 380 : 330;
	uiMain.configuration.generic.callback = UI_Main_Callback;

	uiMain.multiPlayer.generic.id = ID_MULTIPLAYER;
	uiMain.multiPlayer.generic.type = QMTYPE_ACTION;
	uiMain.multiPlayer.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	uiMain.multiPlayer.generic.name = "Multiplayer";
	uiMain.multiPlayer.generic.statusText = "Search for internet servers, configure character";
	uiMain.multiPlayer.generic.x = 72;
	uiMain.multiPlayer.generic.y = com.strlen( GI->trainmap ) ? 430 : 380;
	uiMain.multiPlayer.generic.callback = UI_Main_Callback;

	uiMain.customGame.generic.id = ID_CUSTOMGAME;
	uiMain.customGame.generic.type = QMTYPE_ACTION;
	uiMain.customGame.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	uiMain.customGame.generic.name = "Custom Game";
	uiMain.customGame.generic.statusText = "Select a custom game";
	uiMain.customGame.generic.x = 72;
	uiMain.customGame.generic.y = com.strlen( GI->trainmap ) ? 480 : 430;
	uiMain.customGame.generic.callback = UI_Main_Callback;

	uiMain.credits.generic.id = ID_CREDITS;
	uiMain.credits.generic.type = QMTYPE_ACTION;
	uiMain.credits.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	uiMain.credits.generic.name = "About";
	uiMain.credits.generic.statusText = "Game credits";
	uiMain.credits.generic.x = 72;
	uiMain.credits.generic.y = com.strlen( GI->trainmap ) ? 530 : 480;
	uiMain.credits.generic.callback = UI_Main_Callback;

	uiMain.quit.generic.id = ID_QUIT;
	uiMain.quit.generic.type = QMTYPE_ACTION;
	uiMain.quit.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	uiMain.quit.generic.name = "Quit";
	uiMain.quit.generic.statusText = "Quit from game";
	uiMain.quit.generic.x = 72;
	uiMain.quit.generic.y = com.strlen( GI->trainmap ) ? 580 : 530;
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
	uiMain.msgBox.generic.type = QMTYPE_ACTION;
	uiMain.msgBox.generic.flags = QMF_INACTIVE|QMF_HIDDEN;
	uiMain.msgBox.generic.ownerdraw = UI_MsgBox_Ownerdraw; // just a fill rectangle
	uiMain.msgBox.generic.x = 192;
	uiMain.msgBox.generic.y = 256;
	uiMain.msgBox.generic.width = 640;
	uiMain.msgBox.generic.height = 256;

	uiMain.quitMessage.generic.id = ID_MSGBOX;
	uiMain.quitMessage.generic.type = QMTYPE_ACTION;
	uiMain.quitMessage.generic.flags = QMF_INACTIVE|QMF_DROPSHADOW|QMF_HIDDEN;
	uiMain.quitMessage.generic.name = "Are you sure you want to quit?";
	uiMain.quitMessage.generic.x = 248;
	uiMain.quitMessage.generic.y = 280;

	uiMain.yes.generic.id = ID_YES;
	uiMain.yes.generic.type = QMTYPE_ACTION;
	uiMain.yes.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_HIDDEN;
	uiMain.yes.generic.name = "Ok";
	uiMain.yes.generic.x = 380;
	uiMain.yes.generic.y = 460;
	uiMain.yes.generic.callback = UI_Main_Callback;

	uiMain.no.generic.id = ID_NO;
	uiMain.no.generic.type = QMTYPE_ACTION;
	uiMain.no.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_HIDDEN;
	uiMain.no.generic.name = "Cancel";
	uiMain.no.generic.x = 530;
	uiMain.no.generic.y = 460;
	uiMain.no.generic.callback = UI_Main_Callback;

	UI_AddItem( &uiMain.menu, (void *)&uiMain.background );
	if( host.developer ) UI_AddItem( &uiMain.menu, (void *)&uiMain.console );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.resumeGame );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.newGame );
	if( com.strlen( GI->trainmap ))
		UI_AddItem( &uiMain.menu, (void *)&uiMain.hazardCourse );
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

	re->RegisterShader( ART_MAIN_SPLASH, SHADER_NOMIP );
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
	UI_Main_Precache();
	UI_Main_Init();

	UI_PushMenu( &uiMain.menu );
}