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


#define ART_BACKGROUND   	"gfx/shell/misc/ui_sub_options"
#define ART_BANNER	     	"gfx/shell/banners/options_t"
#define ART_PLAYERSETUP  	"gfx/shell/misc/players"
#define ART_PLAYERSETUP2 	"gfx/shell/misc/players_s"
#define ART_CONTROLS     	"gfx/shell/misc/controls"
#define ART_CONTROLS2    	"gfx/shell/misc/controls_s"
#define ART_GAMEOPTIONS  	"gfx/shell/misc/goptions"
#define ART_GAMEOPTIONS2 	"gfx/shell/misc/goptions_s"
#define ART_AUDIO	     	"gfx/shell/misc/audio"
#define ART_AUDIO2	     	"gfx/shell/misc/audio_s"
#define ART_VIDEO	     	"gfx/shell/misc/video"
#define ART_VIDEO2	     	"gfx/shell/misc/video_s"
#define ART_PERFORMANCE  	"gfx/shell/misc/performance"
#define ART_PERFORMANCE2 	"gfx/shell/misc/performance_s"
#define ART_NETWORK	     	"gfx/shell/misc/network"
#define ART_NETWORK2     	"gfx/shell/misc/network_s"
#define ART_DEFAULTS     	"gfx/shell/misc/defaults"
#define ART_DEFAULTS2    	"gfx/shell/misc/defaults_s"

#define ID_BACKGROUND    	0
#define ID_BANNER	     	1

#define ID_PLAYERSETUP   	2
#define ID_CONTROLS	     	3
#define ID_GAMEOPTIONS   	4
#define ID_AUDIO	     	5
#define ID_VIDEO	     	6
#define ID_PERFORMANCE   	7
#define ID_NETWORK	     	8
#define ID_DEFAULTS	     	9
#define ID_BACK	     	10

typedef struct
{
	menuFramework_s	menu;
	
	menuBitmap_s	background;
	menuBitmap_s	banner;
	menuBitmap_s	back;
	menuBitmap_s	playerSetup;
	menuBitmap_s	controls;
	menuBitmap_s	gameOptions;
	menuBitmap_s	audio;
	menuBitmap_s	video;
	menuBitmap_s	performance;
	menuBitmap_s	network;
	menuBitmap_s	defaults;
} uiOptions_t;

static uiOptions_t		uiOptions;


/*
=================
UI_Options_Callback
=================
*/
static void UI_Options_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_BACK:
		UI_PopMenu();
		break;
	case ID_PLAYERSETUP:
		UI_PlayerSetup_Menu();
		break;
	case ID_CONTROLS:
		UI_Controls_Menu();
		break;
	case ID_GAMEOPTIONS:
		UI_GameOptions_Menu();
		break;
	case ID_AUDIO:
		UI_Audio_Menu();
		break;
	case ID_VIDEO:
		UI_Video_Menu();
		break;
	case ID_PERFORMANCE:
		UI_Performance_Menu();
		break;
	case ID_NETWORK:
		UI_Network_Menu();
		break;
	case ID_DEFAULTS:
		UI_Defaults_Menu();
		break;
	}
}

/*
=================
UI_Options_Ownerdraw
=================
*/
static void UI_Options_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( uiOptions.menu.items[uiOptions.menu.cursor] == self )
		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOXFOCUS );
	else UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOX );
	UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
}

/*
=================
UI_Options_Init
=================
*/
static void UI_Options_Init( void )
{
	Mem_Set( &uiOptions, 0, sizeof( uiOptions_t ));

	uiOptions.background.generic.id = ID_BACKGROUND;
	uiOptions.background.generic.type = QMTYPE_BITMAP;
	uiOptions.background.generic.flags = QMF_INACTIVE;
	uiOptions.background.generic.x = 0;
	uiOptions.background.generic.y = 0;
	uiOptions.background.generic.width = 1024;
	uiOptions.background.generic.height = 768;
	uiOptions.background.pic = ART_BACKGROUND;

	uiOptions.banner.generic.id = ID_BANNER;
	uiOptions.banner.generic.type = QMTYPE_BITMAP;
	uiOptions.banner.generic.flags = QMF_INACTIVE;
	uiOptions.banner.generic.x = 0;
	uiOptions.banner.generic.y = 66;
	uiOptions.banner.generic.width = 1024;
	uiOptions.banner.generic.height = 46;
	uiOptions.banner.pic = ART_BANNER;

	uiOptions.back.generic.id = ID_BACK;
	uiOptions.back.generic.type = QMTYPE_BITMAP;
	uiOptions.back.generic.x = 413;
	uiOptions.back.generic.y = 656;
	uiOptions.back.generic.width = 198;
	uiOptions.back.generic.height = 38;
	uiOptions.back.generic.callback = UI_Options_Callback;
	uiOptions.back.generic.ownerdraw = UI_Options_Ownerdraw;
	uiOptions.back.pic = UI_BACKBUTTON;

	uiOptions.playerSetup.generic.id = ID_PLAYERSETUP;
	uiOptions.playerSetup.generic.type = QMTYPE_BITMAP;
	uiOptions.playerSetup.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiOptions.playerSetup.generic.x = 323;
	uiOptions.playerSetup.generic.y = 174;
	uiOptions.playerSetup.generic.width = 378;
	uiOptions.playerSetup.generic.height = 42;
	uiOptions.playerSetup.generic.callback = UI_Options_Callback;
	uiOptions.playerSetup.pic = ART_PLAYERSETUP;
	uiOptions.playerSetup.focusPic = ART_PLAYERSETUP2;

	uiOptions.controls.generic.id	= ID_CONTROLS;
	uiOptions.controls.generic.type = QMTYPE_BITMAP;
	uiOptions.controls.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiOptions.controls.generic.x = 361;
	uiOptions.controls.generic.y = 216;
	uiOptions.controls.generic.width = 302;
	uiOptions.controls.generic.height = 42;
	uiOptions.controls.generic.callback = UI_Options_Callback;
	uiOptions.controls.pic = ART_CONTROLS;
	uiOptions.controls.focusPic = ART_CONTROLS2;

	uiOptions.gameOptions.generic.id = ID_GAMEOPTIONS;
	uiOptions.gameOptions.generic.type = QMTYPE_BITMAP;
	uiOptions.gameOptions.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiOptions.gameOptions.generic.x = 320;
	uiOptions.gameOptions.generic.y = 258;
	uiOptions.gameOptions.generic.width = 384;
	uiOptions.gameOptions.generic.height = 42;
	uiOptions.gameOptions.generic.callback = UI_Options_Callback;
	uiOptions.gameOptions.pic = ART_GAMEOPTIONS;
	uiOptions.gameOptions.focusPic = ART_GAMEOPTIONS2;

	uiOptions.audio.generic.id = ID_AUDIO;
	uiOptions.audio.generic.type = QMTYPE_BITMAP;
	uiOptions.audio.generic.flags	= QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiOptions.audio.generic.x = 406;
	uiOptions.audio.generic.y = 300;
	uiOptions.audio.generic.width = 212;
	uiOptions.audio.generic.height = 42;
	uiOptions.audio.generic.callback = UI_Options_Callback;
	uiOptions.audio.pic	= ART_AUDIO;
	uiOptions.audio.focusPic = ART_AUDIO2;

	uiOptions.video.generic.id = ID_VIDEO;
	uiOptions.video.generic.type = QMTYPE_BITMAP;
	uiOptions.video.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiOptions.video.generic.x = 409;
	uiOptions.video.generic.y = 342;
	uiOptions.video.generic.width = 206;
	uiOptions.video.generic.height = 42;
	uiOptions.video.generic.callback = UI_Options_Callback;
	uiOptions.video.pic	= ART_VIDEO;
	uiOptions.video.focusPic = ART_VIDEO2;

	uiOptions.performance.generic.id = ID_PERFORMANCE;
	uiOptions.performance.generic.type = QMTYPE_BITMAP;
	uiOptions.performance.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiOptions.performance.generic.x = 315;
	uiOptions.performance.generic.y = 384;
	uiOptions.performance.generic.width = 394;
	uiOptions.performance.generic.height = 42;
	uiOptions.performance.generic.callback = UI_Options_Callback;
	uiOptions.performance.pic = ART_PERFORMANCE;
	uiOptions.performance.focusPic = ART_PERFORMANCE2;

	uiOptions.network.generic.id = ID_NETWORK;
	uiOptions.network.generic.type = QMTYPE_BITMAP;
	uiOptions.network.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiOptions.network.generic.x = 364;
	uiOptions.network.generic.y = 426;
	uiOptions.network.generic.width = 296;
	uiOptions.network.generic.height = 42;
	uiOptions.network.generic.callback = UI_Options_Callback;
	uiOptions.network.pic = ART_NETWORK;
	uiOptions.network.focusPic = ART_NETWORK2;

	uiOptions.defaults.generic.id	= ID_DEFAULTS;
	uiOptions.defaults.generic.type = QMTYPE_BITMAP;
	uiOptions.defaults.generic.flags = QMF_PULSEIFFOCUS | QMF_FOCUSBEHIND;
	uiOptions.defaults.generic.x = 367;
	uiOptions.defaults.generic.y = 510;
	uiOptions.defaults.generic.width = 290;
	uiOptions.defaults.generic.height = 42;
	uiOptions.defaults.generic.callback = UI_Options_Callback;
	uiOptions.defaults.pic = ART_DEFAULTS;
	uiOptions.defaults.focusPic = ART_DEFAULTS2;

	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.background );
	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.banner );
	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.back );
	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.playerSetup );
	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.controls );
	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.gameOptions );
	UI_AddItem(  &uiOptions.menu, (void *)&uiOptions.audio );
	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.video );
	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.performance );
	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.network );
	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.defaults );
}

/*
=================
UI_Options_Precache
=================
*/
void UI_Options_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
	re->RegisterShader( ART_PLAYERSETUP, SHADER_NOMIP );
	re->RegisterShader( ART_PLAYERSETUP2, SHADER_NOMIP );
	re->RegisterShader( ART_CONTROLS, SHADER_NOMIP );
	re->RegisterShader( ART_CONTROLS2, SHADER_NOMIP );
	re->RegisterShader( ART_GAMEOPTIONS, SHADER_NOMIP );
	re->RegisterShader( ART_GAMEOPTIONS2, SHADER_NOMIP );
	re->RegisterShader( ART_AUDIO, SHADER_NOMIP );
	re->RegisterShader( ART_AUDIO2, SHADER_NOMIP );
	re->RegisterShader( ART_VIDEO, SHADER_NOMIP );
	re->RegisterShader( ART_VIDEO2, SHADER_NOMIP );
	re->RegisterShader( ART_PERFORMANCE, SHADER_NOMIP );
	re->RegisterShader( ART_PERFORMANCE2, SHADER_NOMIP );
	re->RegisterShader( ART_NETWORK, SHADER_NOMIP );
	re->RegisterShader( ART_NETWORK2, SHADER_NOMIP );
	re->RegisterShader( ART_DEFAULTS, SHADER_NOMIP );
	re->RegisterShader( ART_DEFAULTS2, SHADER_NOMIP );
}

/*
=================
UI_Options_Menu
=================
*/
void UI_Options_Menu( void )
{
	UI_Options_Precache();
	UI_Options_Init();
	
	UI_PushMenu( &uiOptions.menu );
}