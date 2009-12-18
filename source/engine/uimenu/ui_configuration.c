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

#define ART_BANNER	     	"gfx/shell/head_config"

#define ID_BACKGROUND    	0
#define ID_BANNER	     	1

#define ID_CONTROLS		2
#define ID_AUDIO	     	3
#define ID_VIDEO	     	4
#define ID_UPDATE   	5
#define ID_DONE	     	6

typedef struct
{
	menuFramework_s	menu;
	
	menuBitmap_s	background;
	menuBitmap_s	banner;

	menuAction_s	controls;
	menuAction_s	audio;
	menuAction_s	video;
	menuAction_s	update;
	menuAction_s	done;
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
	case ID_DONE:
		UI_PopMenu();
		break;
	case ID_CONTROLS:
		UI_Controls_Menu();
		break;
	case ID_AUDIO:
		UI_Audio_Menu();
		break;
	case ID_VIDEO:
		UI_Video_Menu();
		break;
	case ID_UPDATE:
		break;
	}
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
	uiOptions.banner.generic.x = UI_BANNER_POSX;
	uiOptions.banner.generic.y = UI_BANNER_POSY;
	uiOptions.banner.generic.width = UI_BANNER_WIDTH;
	uiOptions.banner.generic.height = UI_BANNER_HEIGHT;
	uiOptions.banner.pic = ART_BANNER;

	uiOptions.controls.generic.id	= ID_CONTROLS;
	uiOptions.controls.generic.type = QMTYPE_ACTION;
	uiOptions.controls.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	uiOptions.controls.generic.x = 72;
	uiOptions.controls.generic.y = 230;
	uiOptions.controls.generic.name = "Controls";
	uiOptions.controls.generic.statusText = "Change keyboard and mouse settings";
	uiOptions.controls.generic.callback = UI_Options_Callback;

	uiOptions.audio.generic.id = ID_AUDIO;
	uiOptions.audio.generic.type = QMTYPE_ACTION;
	uiOptions.audio.generic.flags	= QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	uiOptions.audio.generic.x = 72;
	uiOptions.audio.generic.y = 280;
	uiOptions.audio.generic.name = "Audio";
	uiOptions.audio.generic.statusText = "Change sound volume and quality";
	uiOptions.audio.generic.callback = UI_Options_Callback;

	uiOptions.video.generic.id = ID_VIDEO;
	uiOptions.video.generic.type = QMTYPE_ACTION;
	uiOptions.video.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	uiOptions.video.generic.x = 72;
	uiOptions.video.generic.y = 330;
	uiOptions.video.generic.name = "Video";
	uiOptions.video.generic.statusText = "Change screen size, video mode and gamma";
	uiOptions.video.generic.callback = UI_Options_Callback;

	uiOptions.update.generic.id = ID_UPDATE;
	uiOptions.update.generic.type = QMTYPE_ACTION;
	uiOptions.update.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY|QMF_GRAYED; // FIXME:implement
	uiOptions.update.generic.x = 72;
	uiOptions.update.generic.y = 380;
	uiOptions.update.generic.name = "Update";
	uiOptions.update.generic.statusText = "Donwload the latest version of the Xash3D engine";
	uiOptions.update.generic.callback = UI_Options_Callback;

	uiOptions.done.generic.id = ID_DONE;
	uiOptions.done.generic.type = QMTYPE_ACTION;
	uiOptions.done.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	uiOptions.done.generic.x = 72;
	uiOptions.done.generic.y = 430;
	uiOptions.done.generic.name = "Done";
	uiOptions.done.generic.statusText = "Go back to the Main Menu";
	uiOptions.done.generic.callback = UI_Options_Callback;

	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.background );
	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.banner );
	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.done );
	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.controls );
	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.audio );
	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.video );
	UI_AddItem( &uiOptions.menu, (void *)&uiOptions.update );
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