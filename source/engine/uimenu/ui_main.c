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
#define ART_IDLOGO		"gfx/shell/title_screen/logo_id"
#define ART_IDLOGO2		"gfx/shell/title_screen/logo_id_s"
#define ART_BLURLOGO	"gfx/shell/title_screen/logo_blur"
#define ART_BLURLOGO2	"gfx/shell/title_screen/logo_blur_s"
#define ART_COPYRIGHT	"gfx/shell/title_screen/copyrights"

#define ID_BACKGROUND	0

#define ID_CONSOLE		1
#define ID_NEWGAME		2
#define ID_CONFIGURATION	3
#define ID_SAVERESTORE	4	
#define ID_MULTIPLAYER	5
#define ID_CUSTOMGAME	6
#define ID_CREDITS		7
#define ID_QUIT		8

#define ID_IDSOFTWARE	9
#define ID_TEAMBLUR		10
#define ID_COPYRIGHT	11
#define ID_ALLYOURBASE	12

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

	menuBitmap_s	idSoftware;
	menuBitmap_s	teamBlur;
	menuBitmap_s	copyright;

	menuBitmap_s	allYourBase;
} uiMain_t;

static uiMain_t		uiMain;

static void UI_AllYourBase_Menu( void );

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
		UI_SinglePlayer_Menu();
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
		UI_Credits_Menu ();
		break;
	case ID_QUIT:
		UI_Quit_Menu();
		break;
	case ID_IDSOFTWARE:
		UI_GoToSite_Menu( "http://www.idsoftware.com" );
		break;
	case ID_TEAMBLUR:
		UI_GoToSite_Menu( "http://www.planetquake.com/blur" );
		break;
	case ID_ALLYOURBASE:
		UI_AllYourBase_Menu();
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
	uiMain.console.generic.x = 64;
	uiMain.console.generic.y = 230;
	uiMain.console.generic.callback = UI_Main_Callback;

	uiMain.newGame.generic.id = ID_NEWGAME;
	uiMain.newGame.generic.type = QMTYPE_ACTION;
	uiMain.newGame.generic.name = "New game";
	uiMain.newGame.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiMain.newGame.generic.x = 64;
	uiMain.newGame.generic.y = 280;
	uiMain.newGame.generic.callback = UI_Main_Callback;

	uiMain.configuration.generic.id = ID_CONFIGURATION;
	uiMain.configuration.generic.type = QMTYPE_ACTION;
	uiMain.configuration.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiMain.configuration.generic.name = "Configuration";
	uiMain.configuration.generic.x = 64;
	uiMain.configuration.generic.y = 330;
	uiMain.configuration.generic.callback = UI_Main_Callback;

	uiMain.saveRestore.generic.id = ID_SAVERESTORE;
	uiMain.saveRestore.generic.type = QMTYPE_ACTION;
	uiMain.saveRestore.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	if( cls.state == ca_active )
		uiMain.saveRestore.generic.name = "Save\\Load Game";
	else uiMain.saveRestore.generic.name = "Load Game";
	uiMain.saveRestore.generic.x = 64;
	uiMain.saveRestore.generic.y = 380;
	uiMain.saveRestore.generic.callback = UI_Main_Callback;

	uiMain.multiPlayer.generic.id = ID_MULTIPLAYER;
	uiMain.multiPlayer.generic.type = QMTYPE_ACTION;
	uiMain.multiPlayer.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiMain.multiPlayer.generic.name = "Multiplayer";
	uiMain.multiPlayer.generic.x = 64;
	uiMain.multiPlayer.generic.y = 430;
	uiMain.multiPlayer.generic.callback = UI_Main_Callback;

	uiMain.customGame.generic.id = ID_CUSTOMGAME;
	uiMain.customGame.generic.type = QMTYPE_ACTION;
	uiMain.customGame.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiMain.customGame.generic.name = "Custom Game";
	uiMain.customGame.generic.x = 64;
	uiMain.customGame.generic.y = 480;
	uiMain.customGame.generic.callback = UI_Main_Callback;

	uiMain.credits.generic.id = ID_CREDITS;
	uiMain.credits.generic.type = QMTYPE_ACTION;
	uiMain.credits.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiMain.credits.generic.name = "Credits";
	uiMain.credits.generic.x = 64;
	uiMain.credits.generic.y = 530;
	uiMain.credits.generic.callback = UI_Main_Callback;

	uiMain.quit.generic.id = ID_QUIT;
	uiMain.quit.generic.type = QMTYPE_ACTION;
	uiMain.quit.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiMain.quit.generic.name = "Quit";
	uiMain.quit.generic.x = 64;
	uiMain.quit.generic.y = 580;
	uiMain.quit.generic.callback = UI_Main_Callback;

	uiMain.idSoftware.generic.id = ID_IDSOFTWARE;
	uiMain.idSoftware.generic.type = QMTYPE_BITMAP;
	uiMain.idSoftware.generic.flags = QMF_PULSEIFFOCUS | QMF_MOUSEONLY;
	uiMain.idSoftware.generic.x = 0;
	uiMain.idSoftware.generic.y = 640;
	uiMain.idSoftware.generic.width = 128;
	uiMain.idSoftware.generic.height = 128;
	uiMain.idSoftware.generic.callback = UI_Main_Callback;
	uiMain.idSoftware.pic = ART_IDLOGO;
	uiMain.idSoftware.focusPic = ART_IDLOGO2;

	uiMain.teamBlur.generic.id = ID_TEAMBLUR;
	uiMain.teamBlur.generic.type = QMTYPE_BITMAP;
	uiMain.teamBlur.generic.flags = QMF_PULSEIFFOCUS | QMF_MOUSEONLY;
	uiMain.teamBlur.generic.x = 896;
	uiMain.teamBlur.generic.y = 640;
	uiMain.teamBlur.generic.width	= 128;
	uiMain.teamBlur.generic.height = 128;
	uiMain.teamBlur.generic.callback = UI_Main_Callback;
	uiMain.teamBlur.pic	= ART_BLURLOGO;
	uiMain.teamBlur.focusPic = ART_BLURLOGO2;

	uiMain.copyright.generic.id = ID_COPYRIGHT;
	uiMain.copyright.generic.type	= QMTYPE_BITMAP;
	uiMain.copyright.generic.flags = QMF_INACTIVE;
	uiMain.copyright.generic.x = 240;
	uiMain.copyright.generic.y = 680;
	uiMain.copyright.generic.width = 544;
	uiMain.copyright.generic.height = 48;
	uiMain.copyright.pic = ART_COPYRIGHT;

	uiMain.allYourBase.generic.id = ID_ALLYOURBASE;
	uiMain.allYourBase.generic.type = QMTYPE_BITMAP;
	uiMain.allYourBase.generic.flags = QMF_MOUSEONLY;
	uiMain.allYourBase.generic.x = 416;
	uiMain.allYourBase.generic.y = 158;
	uiMain.allYourBase.generic.width = 37;
	uiMain.allYourBase.generic.height = 35;
	uiMain.allYourBase.generic.callback = UI_Main_Callback;

	UI_AddItem( &uiMain.menu, (void *)&uiMain.background );
	if( host.developer ) UI_AddItem( &uiMain.menu, (void *)&uiMain.console );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.newGame );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.configuration );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.saveRestore );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.multiPlayer );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.customGame );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.credits );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.quit );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.idSoftware );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.teamBlur );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.copyright );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.allYourBase );
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
	re->RegisterShader( ART_IDLOGO, SHADER_NOMIP );
	re->RegisterShader( ART_IDLOGO2, SHADER_NOMIP );
	re->RegisterShader( ART_BLURLOGO, SHADER_NOMIP );
	re->RegisterShader( ART_BLURLOGO2, SHADER_NOMIP );
	re->RegisterShader( ART_COPYRIGHT, SHADER_NOMIP );
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


// =====================================================================

#define ART_BANNER		"gfx/shell/banners/aybabtu_t"
#define ART_MOO		"gfx/shell/misc/moo"
#define SOUND_MOO		"misc/moo.wav"

#define ID_BANNER		1
#define ID_BACK		2
#define ID_MOO		3

typedef struct
{
	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;
	menuBitmap_s	back;
	menuBitmap_s	moo;
} uiAllYourBase_t;

static uiAllYourBase_t	uiAllYourBase;


/*
=================
UI_AllYourBase_Callback
=================
*/
static void UI_AllYourBase_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_BACK:
		UI_PopMenu();
		break;
	}
}

/*
=================
UI_AllYourBase_Ownerdraw
=================
*/
static void UI_AllYourBase_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( uiAllYourBase.menu.items[uiAllYourBase.menu.cursor] == self )
		UI_DrawPic(item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOXFOCUS );
	else UI_DrawPic(item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOX );

	UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
}

/*
=================
UI_AllYourBase_Init
=================
*/
static void UI_AllYourBase_Init( void )
{
	Mem_Set( &uiAllYourBase, 0, sizeof( uiAllYourBase_t ));

	uiAllYourBase.background.generic.id = ID_BACKGROUND;
	uiAllYourBase.background.generic.type = QMTYPE_BITMAP;
	uiAllYourBase.background.generic.flags = QMF_INACTIVE | QMF_GRAYED;
	uiAllYourBase.background.generic.x = 0;
	uiAllYourBase.background.generic.y = 0;
	uiAllYourBase.background.generic.width = 1024;
	uiAllYourBase.background.generic.height	= 768;
	uiAllYourBase.background.pic = ART_BACKGROUND;

	uiAllYourBase.banner.generic.id = ID_BANNER;
	uiAllYourBase.banner.generic.type = QMTYPE_BITMAP;
	uiAllYourBase.banner.generic.flags = QMF_INACTIVE;
	uiAllYourBase.banner.generic.x = 0;
	uiAllYourBase.banner.generic.y = 66;
	uiAllYourBase.banner.generic.width = 1024;
	uiAllYourBase.banner.generic.height = 46;
	uiAllYourBase.banner.pic = ART_BANNER;

	uiAllYourBase.back.generic.id	= ID_BACK;
	uiAllYourBase.back.generic.type = QMTYPE_BITMAP;
	uiAllYourBase.back.generic.x = 413;
	uiAllYourBase.back.generic.y = 656;
	uiAllYourBase.back.generic.width = 198;
	uiAllYourBase.back.generic.height = 38;
	uiAllYourBase.back.generic.callback = UI_AllYourBase_Callback;
	uiAllYourBase.back.generic.ownerdraw = UI_AllYourBase_Ownerdraw;
	uiAllYourBase.back.pic = UI_BACKBUTTON;

	uiAllYourBase.moo.generic.id = ID_MOO;
	uiAllYourBase.moo.generic.type = QMTYPE_BITMAP;
	uiAllYourBase.moo.generic.flags = QMF_INACTIVE;
	uiAllYourBase.moo.generic.x = 256;
	uiAllYourBase.moo.generic.y = 128;
	uiAllYourBase.moo.generic.width = 512;
	uiAllYourBase.moo.generic.height = 512;
	uiAllYourBase.moo.pic = ART_MOO;

	UI_AddItem( &uiAllYourBase.menu, (void **)&uiAllYourBase.background );
	UI_AddItem( &uiAllYourBase.menu, (void **)&uiAllYourBase.banner );
	UI_AddItem( &uiAllYourBase.menu, (void **)&uiAllYourBase.back );
	UI_AddItem( &uiAllYourBase.menu, (void **)&uiAllYourBase.moo );
}

/*
=================
UI_AllYourBase_Precache
=================
*/
static void UI_AllYourBase_Precache( void )
{
	S_RegisterSound( SOUND_MOO );

	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
	re->RegisterShader( ART_MOO, SHADER_NOMIP );
}

/*
=================
UI_AllYourBase_Menu
=================
*/
static void UI_AllYourBase_Menu( void )
{
	UI_AllYourBase_Precache();
	UI_AllYourBase_Init();

	UI_PushMenu( &uiAllYourBase.menu );
	UI_StartSound( SOUND_MOO );
}