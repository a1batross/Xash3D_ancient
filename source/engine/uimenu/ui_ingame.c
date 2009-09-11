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
#include "client.h"

#define ART_LOGO		"gfx/shell/ingui/main"
#define ART_BLUR		"gfx/shell/ingui/edge_blur"
#define ART_LOADGAME	"gfx/shell/ingui/titles/load_game_n"
#define ART_LOADGAME2	"gfx/shell/ingui/titles/load_game_s"
#define ART_SAVEGAME	"gfx/shell/ingui/titles/save_game_n"
#define ART_SAVEGAME2	"gfx/shell/ingui/titles/save_game_s"
#define ART_OPTIONS		"gfx/shell/ingui/titles/options_n"
#define ART_OPTIONS2	"gfx/shell/ingui/titles/options_s"
#define ART_RESUMEGAME	"gfx/shell/ingui/titles/return_to_game_n"
#define ART_RESUMEGAME2	"gfx/shell/ingui/titles/return_to_game_s"
#define ART_LEAVEGAME	"gfx/shell/ingui/titles/return_to_menu_n"
#define ART_LEAVEGAME2	"gfx/shell/ingui/titles/return_to_menu_s"
#define ART_EXITGAME	"gfx/shell/ingui/titles/exit_game_n"
#define ART_EXITGAME2	"gfx/shell/ingui/titles/exit_game_s"

#define ID_LOGO		0
#define ID_BLUR		1

#define ID_LOADGAME		2
#define ID_SAVEGAME		3
#define ID_OPTIONS		4
#define ID_RESUMEGAME	5
#define ID_LEAVEGAME	6
#define ID_EXITGAME		7

typedef struct
{
	menuFramework_s	menu;

	menuBitmap_s	logo;
	menuBitmap_s	blur;
	menuBitmap_s	loadGame;
	menuBitmap_s	saveGame;
	menuBitmap_s	options;
	menuBitmap_s	resumeGame;
	menuBitmap_s	leaveGame;
	menuBitmap_s	exitGame;
} uiInGame_t;

static uiInGame_t		uiInGame;


/*
=================
UI_InGame_Callback
=================
*/
static void UI_InGame_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_LOADGAME:
		UI_LoadGame_Menu();
		break;
	case ID_SAVEGAME:
		UI_SaveGame_Menu();
		break;
	case ID_OPTIONS:
		UI_Options_Menu();
		break;
	case ID_RESUMEGAME:
		UI_CloseMenu();
		break;
	case ID_LEAVEGAME:
		Cbuf_ExecuteText( EXEC_APPEND, "disconnect\n" );
		break;
	case ID_EXITGAME:
		UI_Quit_Menu();
		break;
	}
}

/*
=================
UI_InGame_Init
=================
*/
static void UI_InGame_Init( void )
{
	Mem_Set( &uiInGame, 0, sizeof( uiInGame_t ));

	uiInGame.logo.generic.id = ID_LOGO;
	uiInGame.logo.generic.type = QMTYPE_BITMAP;
	uiInGame.logo.generic.flags = QMF_INACTIVE;
	uiInGame.logo.generic.x = 0;
	uiInGame.logo.generic.y = 238;
	uiInGame.logo.generic.width = 1024;
	uiInGame.logo.generic.height = 292;
	uiInGame.logo.pic = ART_LOGO;

	uiInGame.blur.generic.id = ID_BLUR;
	uiInGame.blur.generic.type = QMTYPE_BITMAP;
	uiInGame.blur.generic.flags = QMF_INACTIVE;
	uiInGame.blur.generic.x = 0;
	uiInGame.blur.generic.y = 230;
	uiInGame.blur.generic.width = 1024;
	uiInGame.blur.generic.height = 308;
	uiInGame.blur.pic = ART_BLUR;

	uiInGame.loadGame.generic.id = ID_LOADGAME;
	uiInGame.loadGame.generic.type = QMTYPE_BITMAP;
	uiInGame.loadGame.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiInGame.loadGame.generic.x = 330;
	uiInGame.loadGame.generic.y = 258;
	uiInGame.loadGame.generic.width = 362;
	uiInGame.loadGame.generic.height = 42;
	uiInGame.loadGame.generic.callback = UI_InGame_Callback;
	uiInGame.loadGame.pic = ART_LOADGAME;
	uiInGame.loadGame.focusPic = ART_LOADGAME2;

	uiInGame.saveGame.generic.id = ID_SAVEGAME;
	uiInGame.saveGame.generic.type = QMTYPE_BITMAP;
	uiInGame.saveGame.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiInGame.saveGame.generic.x = 330;
	uiInGame.saveGame.generic.y = 300;
	uiInGame.saveGame.generic.width = 362;
	uiInGame.saveGame.generic.height = 42;
	uiInGame.saveGame.generic.callback = UI_InGame_Callback;
	uiInGame.saveGame.pic = ART_SAVEGAME;
	uiInGame.saveGame.focusPic = ART_SAVEGAME2;

	uiInGame.options.generic.id = ID_OPTIONS;
	uiInGame.options.generic.type	= QMTYPE_BITMAP;
	uiInGame.options.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiInGame.options.generic.x = 367;
	uiInGame.options.generic.y = 342;
	uiInGame.options.generic.width = 290;
	uiInGame.options.generic.height = 42;
	uiInGame.options.generic.callback = UI_InGame_Callback;
	uiInGame.options.pic = ART_OPTIONS;
	uiInGame.options.focusPic = ART_OPTIONS2;

	uiInGame.resumeGame.generic.id = ID_RESUMEGAME;
	uiInGame.resumeGame.generic.type = QMTYPE_BITMAP;
	uiInGame.resumeGame.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiInGame.resumeGame.generic.x = 258;
	uiInGame.resumeGame.generic.y = 384;
	uiInGame.resumeGame.generic.width = 508;
	uiInGame.resumeGame.generic.height = 42;
	uiInGame.resumeGame.generic.callback = UI_InGame_Callback;
	uiInGame.resumeGame.pic = ART_RESUMEGAME;
	uiInGame.resumeGame.focusPic = ART_RESUMEGAME2;

	uiInGame.leaveGame.generic.id	= ID_LEAVEGAME;
	uiInGame.leaveGame.generic.type = QMTYPE_BITMAP;
	uiInGame.leaveGame.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiInGame.leaveGame.generic.x = 260;
	uiInGame.leaveGame.generic.y = 426;
	uiInGame.leaveGame.generic.width = 504;
	uiInGame.leaveGame.generic.height = 42;
	uiInGame.leaveGame.generic.callback = UI_InGame_Callback;
	uiInGame.leaveGame.pic = ART_LEAVEGAME;
	uiInGame.leaveGame.focusPic = ART_LEAVEGAME2;

	uiInGame.exitGame.generic.id = ID_EXITGAME;
	uiInGame.exitGame.generic.type = QMTYPE_BITMAP;
	uiInGame.exitGame.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiInGame.exitGame.generic.x = 342;
	uiInGame.exitGame.generic.y = 468;
	uiInGame.exitGame.generic.width = 340;
	uiInGame.exitGame.generic.height = 42;
	uiInGame.exitGame.generic.callback = UI_InGame_Callback;
	uiInGame.exitGame.pic = ART_EXITGAME;
	uiInGame.exitGame.focusPic = ART_EXITGAME2;

	UI_AddItem( &uiInGame.menu, (void *)&uiInGame.logo );
	UI_AddItem( &uiInGame.menu, (void *)&uiInGame.blur );
	UI_AddItem( &uiInGame.menu, (void *)&uiInGame.loadGame );
	UI_AddItem( &uiInGame.menu, (void *)&uiInGame.saveGame );
	UI_AddItem( &uiInGame.menu, (void *)&uiInGame.options );
	UI_AddItem( &uiInGame.menu, (void *)&uiInGame.resumeGame );
	UI_AddItem( &uiInGame.menu, (void *)&uiInGame.leaveGame );
	UI_AddItem( &uiInGame.menu, (void *)&uiInGame.exitGame );
}

/*
=================
UI_InGame_Precache
=================
*/
void UI_InGame_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_LOGO, SHADER_NOMIP );
	re->RegisterShader( ART_BLUR, SHADER_NOMIP );
	re->RegisterShader( ART_LOADGAME, SHADER_NOMIP );
	re->RegisterShader( ART_LOADGAME2, SHADER_NOMIP );
	re->RegisterShader( ART_SAVEGAME, SHADER_NOMIP );
	re->RegisterShader( ART_SAVEGAME2, SHADER_NOMIP );
	re->RegisterShader( ART_OPTIONS, SHADER_NOMIP );
	re->RegisterShader( ART_OPTIONS2, SHADER_NOMIP );
	re->RegisterShader( ART_RESUMEGAME, SHADER_NOMIP );
	re->RegisterShader( ART_RESUMEGAME2, SHADER_NOMIP );
	re->RegisterShader( ART_LEAVEGAME, SHADER_NOMIP );
	re->RegisterShader( ART_LEAVEGAME2, SHADER_NOMIP );
	re->RegisterShader( ART_EXITGAME, SHADER_NOMIP );
	re->RegisterShader( ART_EXITGAME2, SHADER_NOMIP );
}

/*
=================
UI_InGame_Menu
=================
*/
void UI_InGame_Menu( void )
{
	if( cls.state != ca_active )
	{
		// this shouldn't be happening, but for some reason it is in some mods
		UI_Main_Menu();
		return;
	}

	UI_InGame_Precache();
	UI_InGame_Init();

	UI_PushMenu( &uiInGame.menu );
}