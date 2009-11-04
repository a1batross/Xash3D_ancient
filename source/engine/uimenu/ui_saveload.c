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

#define ART_BACKGROUND	"gfx/shell/splash"
#define ART_BANNER		"gfx/shell/head_saveload"

#define ID_BACKGROUND	0
#define ID_BANNER		1

#define ID_LOAD	  	2
#define ID_SAVE	  	3
#define ID_DONE		4

#define ID_MSGHINT		5

typedef struct
{
	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;

	menuAction_s	save;
	menuAction_s	load;
	menuAction_s	done;

	menuAction_s	hintMessage;
	char		hintText[MAX_SYSPATH];
} uiSaveLoad_t;

static uiSaveLoad_t	uiSaveLoad;

/*
=================
UI_SaveLoad_Callback
=================
*/
static void UI_SaveLoad_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_LOAD:
		UI_LoadGame_Menu();
		break;
	case ID_SAVE:
		UI_SaveGame_Menu();
		break;
	case ID_DONE:
		UI_PopMenu();
		break;
	}
}

/*
=================
UI_SaveLoad_Init
=================
*/
static void UI_SaveLoad_Init( void )
{
	Mem_Set( &uiSaveLoad, 0, sizeof( uiSaveLoad_t ));

	com.strncat( uiSaveLoad.hintText, "During play, you can quickly save your game by pressing ", MAX_SYSPATH );
	com.strncat( uiSaveLoad.hintText, Key_KeynumToString( Key_GetKey( "savequick" )), MAX_SYSPATH );
	com.strncat( uiSaveLoad.hintText, ".\nLoad this game again by pressing ", MAX_SYSPATH );
	com.strncat( uiSaveLoad.hintText, Key_KeynumToString( Key_GetKey( "loadquick" )), MAX_SYSPATH );
	com.strncat( uiSaveLoad.hintText, ".\n", MAX_SYSPATH );

	uiSaveLoad.background.generic.id = ID_BACKGROUND;
	uiSaveLoad.background.generic.type = QMTYPE_BITMAP;
	uiSaveLoad.background.generic.flags = QMF_INACTIVE;
	uiSaveLoad.background.generic.x = 0;
	uiSaveLoad.background.generic.y = 0;
	uiSaveLoad.background.generic.width = 1024;
	uiSaveLoad.background.generic.height = 768;
	uiSaveLoad.background.pic = ART_BACKGROUND;

	uiSaveLoad.banner.generic.id = ID_BANNER;
	uiSaveLoad.banner.generic.type = QMTYPE_BITMAP;
	uiSaveLoad.banner.generic.flags = QMF_INACTIVE;
	uiSaveLoad.banner.generic.x = 65;
	uiSaveLoad.banner.generic.y = 92;
	uiSaveLoad.banner.generic.width = 690;
	uiSaveLoad.banner.generic.height = 120;
	uiSaveLoad.banner.pic = ART_BANNER;

	uiSaveLoad.load.generic.id = ID_LOAD;
	uiSaveLoad.load.generic.type = QMTYPE_ACTION;
	uiSaveLoad.load.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiSaveLoad.load.generic.name = "Load game";
	uiSaveLoad.load.generic.statusText = "Load a previously saved game";
	uiSaveLoad.load.generic.x = 72;
	uiSaveLoad.load.generic.y = 230;
	uiSaveLoad.load.generic.callback = UI_SaveLoad_Callback;

	uiSaveLoad.save.generic.id = ID_SAVE;
	uiSaveLoad.save.generic.type = QMTYPE_ACTION;
	uiSaveLoad.save.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiSaveLoad.save.generic.name = "Save game";
	uiSaveLoad.save.generic.statusText = "Save current game";
	uiSaveLoad.save.generic.x = 72;
	uiSaveLoad.save.generic.y = 280;
	uiSaveLoad.save.generic.callback = UI_SaveLoad_Callback;

	uiSaveLoad.done.generic.id = ID_DONE;
	uiSaveLoad.done.generic.type = QMTYPE_ACTION;
	uiSaveLoad.done.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiSaveLoad.done.generic.name = "Done";
	uiSaveLoad.done.generic.statusText = "Go back to the Main Menu";
	uiSaveLoad.done.generic.x = 72;
	uiSaveLoad.done.generic.y = 330;
	uiSaveLoad.done.generic.callback = UI_SaveLoad_Callback;

	uiSaveLoad.hintMessage.generic.id = ID_MSGHINT;
	uiSaveLoad.hintMessage.generic.type = QMTYPE_ACTION;
	uiSaveLoad.hintMessage.generic.flags = QMF_INACTIVE|QMF_SMALLFONT;
	uiSaveLoad.hintMessage.generic.color = uiColorLtGrey;
	uiSaveLoad.hintMessage.generic.name = uiSaveLoad.hintText;
	uiSaveLoad.hintMessage.generic.x = 360;
	uiSaveLoad.hintMessage.generic.y = 480;

	UI_AddItem( &uiSaveLoad.menu, (void *)&uiSaveLoad.background );
	UI_AddItem( &uiSaveLoad.menu, (void *)&uiSaveLoad.banner );
	UI_AddItem( &uiSaveLoad.menu, (void *)&uiSaveLoad.load );
	UI_AddItem( &uiSaveLoad.menu, (void *)&uiSaveLoad.save );
	UI_AddItem( &uiSaveLoad.menu, (void *)&uiSaveLoad.done );
	UI_AddItem( &uiSaveLoad.menu, (void *)&uiSaveLoad.hintMessage );
}

/*
=================
UI_SaveLoad_Precache
=================
*/
void UI_SaveLoad_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
}

/*
=================
UI_SaveLoad_Menu
=================
*/
void UI_SaveLoad_Menu( void )
{
	UI_SaveLoad_Precache();
	UI_SaveLoad_Init();

	UI_PushMenu( &uiSaveLoad.menu );
}