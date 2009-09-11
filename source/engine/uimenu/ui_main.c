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

#define ART_BACKGROUND	"gfx/shell/title_screen/title_backg"
#define ART_LOGO		"gfx/shell/title_screen/q2e_logo"
#define ART_SINGLEPLAYER	"gfx/shell/title_screen/singleplayer_n"
#define ART_SINGLEPLAYER2	"gfx/shell/title_screen/singleplayer_s"
#define ART_MULTIPLAYER	"gfx/shell/title_screen/multiplayer_n"
#define ART_MULTIPLAYER2	"gfx/shell/title_screen/multiplayer_s"
#define ART_OPTIONS		"gfx/shell/title_screen/options_n"
#define ART_OPTIONS2	"gfx/shell/title_screen/options_s"
#define ART_DEMOS		"gfx/shell/title_screen/demos_n"
#define ART_DEMOS2		"gfx/shell/title_screen/demos_s"
#define ART_MODS		"gfx/shell/title_screen/mods_n"
#define ART_MODS2		"gfx/shell/title_screen/mods_s"
#define ART_QUIT		"gfx/shell/title_screen/quit_n"
#define ART_QUIT2		"gfx/shell/title_screen/quit_s"
#define ART_IDLOGO		"gfx/shell/title_screen/logo_id"
#define ART_IDLOGO2		"gfx/shell/title_screen/logo_id_s"
#define ART_BLURLOGO	"gfx/shell/title_screen/logo_blur"
#define ART_BLURLOGO2	"gfx/shell/title_screen/logo_blur_s"
#define ART_COPYRIGHT	"gfx/shell/title_screen/copyrights"

#define ID_BACKGROUND	0
#define ID_LOGO		1

#define ID_SINGLEPLAYER	2
#define ID_MULTIPLAYER	3
#define ID_OPTIONS		4
#define ID_DEMOS		5
#define ID_MODS		6
#define ID_QUIT		7

#define ID_IDSOFTWARE	8
#define ID_TEAMBLUR		9
#define ID_COPYRIGHT	10
#define ID_ALLYOURBASE	11

typedef struct
{
	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	logo;

	menuBitmap_s	singlePlayer;
	menuBitmap_s	multiPlayer;
	menuBitmap_s	options;
	menuBitmap_s	cinematics;
	menuBitmap_s	demos;
	menuBitmap_s	mods;
	menuBitmap_s	quit;

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
	case ID_SINGLEPLAYER:
		UI_SinglePlayer_Menu();
		break;
	case ID_MULTIPLAYER:
		UI_MultiPlayer_Menu();
		break;
	case ID_OPTIONS:
		UI_Options_Menu();
		break;
	case ID_DEMOS:
		UI_Demos_Menu();
		break;
	case ID_MODS:
		UI_Mods_Menu();
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

	uiMain.logo.generic.id = ID_LOGO;
	uiMain.logo.generic.type = QMTYPE_BITMAP;
	uiMain.logo.generic.flags = QMF_INACTIVE;
	uiMain.logo.generic.x = 0;
	uiMain.logo.generic.y = 0;
	uiMain.logo.generic.width = 1024;
	uiMain.logo.generic.height = 256;
	uiMain.logo.pic = ART_LOGO;

	uiMain.singlePlayer.generic.id = ID_SINGLEPLAYER;
	uiMain.singlePlayer.generic.type = QMTYPE_BITMAP;
	uiMain.singlePlayer.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiMain.singlePlayer.generic.x	= 285;
	uiMain.singlePlayer.generic.y	= 300;
	uiMain.singlePlayer.generic.width = 454;
	uiMain.singlePlayer.generic.height = 42;
	uiMain.singlePlayer.generic.callback = UI_Main_Callback;
	uiMain.singlePlayer.pic = ART_SINGLEPLAYER;
	uiMain.singlePlayer.focusPic = ART_SINGLEPLAYER2;

	uiMain.multiPlayer.generic.id = ID_MULTIPLAYER;
	uiMain.multiPlayer.generic.type = QMTYPE_BITMAP;
	uiMain.multiPlayer.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiMain.multiPlayer.generic.x = 306;
	uiMain.multiPlayer.generic.y = 342;
	uiMain.multiPlayer.generic.width = 412;
	uiMain.multiPlayer.generic.height = 42;
	uiMain.multiPlayer.generic.callback = UI_Main_Callback;
	uiMain.multiPlayer.pic = ART_MULTIPLAYER;
	uiMain.multiPlayer.focusPic = ART_MULTIPLAYER2;

	uiMain.options.generic.id = ID_OPTIONS;
	uiMain.options.generic.type = QMTYPE_BITMAP;
	uiMain.options.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiMain.options.generic.x = 367;
	uiMain.options.generic.y = 384;
	uiMain.options.generic.width = 290;
	uiMain.options.generic.height	= 42;
	uiMain.options.generic.callback = UI_Main_Callback;
	uiMain.options.pic = ART_OPTIONS;
	uiMain.options.focusPic = ART_OPTIONS2;

	uiMain.demos.generic.id = ID_DEMOS;
	uiMain.demos.generic.type = QMTYPE_BITMAP;
	uiMain.demos.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiMain.demos.generic.x = 386;
	uiMain.demos.generic.y = 468;
	uiMain.demos.generic.width = 252;
	uiMain.demos.generic.height = 42;
	uiMain.demos.generic.callback	= UI_Main_Callback;
	uiMain.demos.pic = ART_DEMOS;
	uiMain.demos.focusPic = ART_DEMOS2;

	uiMain.mods.generic.id = ID_MODS;
	uiMain.mods.generic.type = QMTYPE_BITMAP;
	uiMain.mods.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiMain.mods.generic.x = 291;
	uiMain.mods.generic.y = 510;
	uiMain.mods.generic.width = 442;
	uiMain.mods.generic.height = 42;
	uiMain.mods.generic.callback = UI_Main_Callback;
	uiMain.mods.pic = ART_MODS;
	uiMain.mods.focusPic = ART_MODS2;

	uiMain.quit.generic.id = ID_QUIT;
	uiMain.quit.generic.type = QMTYPE_BITMAP;
	uiMain.quit.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiMain.quit.generic.x = 415;
	uiMain.quit.generic.y = 594;
	uiMain.quit.generic.width = 194;
	uiMain.quit.generic.height = 42;
	uiMain.quit.generic.callback = UI_Main_Callback;
	uiMain.quit.pic = ART_QUIT;
	uiMain.quit.focusPic = ART_QUIT2;

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
	UI_AddItem( &uiMain.menu, (void *)&uiMain.logo );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.singlePlayer );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.multiPlayer );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.options );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.demos );
	UI_AddItem( &uiMain.menu, (void *)&uiMain.mods );
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
	re->RegisterShader( ART_LOGO, SHADER_NOMIP );
	re->RegisterShader( ART_SINGLEPLAYER, SHADER_NOMIP );
	re->RegisterShader( ART_SINGLEPLAYER2, SHADER_NOMIP );
	re->RegisterShader( ART_MULTIPLAYER, SHADER_NOMIP );
	re->RegisterShader( ART_MULTIPLAYER2, SHADER_NOMIP );
	re->RegisterShader( ART_OPTIONS, SHADER_NOMIP );
	re->RegisterShader( ART_OPTIONS2, SHADER_NOMIP );
	re->RegisterShader( ART_DEMOS, SHADER_NOMIP );
	re->RegisterShader( ART_DEMOS2, SHADER_NOMIP );
	re->RegisterShader( ART_MODS, SHADER_NOMIP );
	re->RegisterShader( ART_MODS2, SHADER_NOMIP );
	re->RegisterShader( ART_QUIT, SHADER_NOMIP );
	re->RegisterShader( ART_QUIT2, SHADER_NOMIP );
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