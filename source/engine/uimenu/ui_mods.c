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

#define ART_BACKGROUND	"gfx/shell/misc/ui_sub_modifications"
#define ART_BANNER		"gfx/shell/banners/mods_t"

#define ID_BACKGROUND	0
#define ID_BANNER		1
#define ID_BACK		2
#define ID_LOAD		3
#define ID_MODLIST		4

typedef struct
{
	string		modsDir[MAX_MODS];
	string		modsDescription[MAX_MODS];
	char		*modsDescriptionPtr[MAX_MODS];

	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;
	menuBitmap_s	back;
	menuBitmap_s	load;

	menuScrollList_s	modList;
} uiMods_t;

static uiMods_t		uiMods;

/*
=================
UI_Mods_GetModList
=================
*/
static void UI_Mods_GetModList( void )
{
	int	i;

	for( i = 0; i < SI->numgames; i++ )
	{
		com.strncpy( uiMods.modsDir[i], SI->games[i]->gamefolder, sizeof( uiMods.modsDir[i] ));
		com.strncpy( uiMods.modsDescription[i], SI->games[i]->title, sizeof( uiMods.modsDescription[i] ));
		uiMods.modsDescriptionPtr[i] = uiMods.modsDescription[i];
	}
	for( ; i < MAX_MODS; i++ ) uiMods.modsDescriptionPtr[i] = NULL;

	uiMods.modList.itemNames = uiMods.modsDescriptionPtr;

	// see if the load button should be grayed
	// FIXME: this is a properly ?
	if( !com.stricmp( GI->gamefolder, uiMods.modsDir[0] ))
		uiMods.load.generic.flags |= QMF_GRAYED;
}

/*
=================
UI_Mods_Callback
=================
*/
static void UI_Mods_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event == QM_CHANGED )
	{
		// aee if the load button should be grayed
		if( !com.stricmp( GI->gamefolder, uiMods.modsDir[uiMods.modList.curItem] ))
			uiMods.load.generic.flags |= QMF_GRAYED;
		else uiMods.load.generic.flags &= ~QMF_GRAYED;
		return;
	}

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_BACK:
		UI_PopMenu();
		break;
	case ID_LOAD:
		if( cls.state == ca_active )
			break;	// don't fuck up the game

		// restart all engine systems with new game
		Cbuf_ExecuteText( EXEC_APPEND, va( "game %s\n", uiMods.modsDir[uiMods.modList.curItem] ));

		break;
	}
}

/*
=================
UI_Mods_Ownerdraw
=================
*/
static void UI_Mods_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( uiMods.menu.items[uiMods.menu.cursor] == self )
		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOXFOCUS );
	else UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOX );

	UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
}

/*
=================
UI_Mods_Init
=================
*/
static void UI_Mods_Init( void )
{
	Mem_Set( &uiMods, 0, sizeof( uiMods_t ));

	uiMods.background.generic.id = ID_BACKGROUND;
	uiMods.background.generic.type = QMTYPE_BITMAP;
	uiMods.background.generic.flags = QMF_INACTIVE;
	uiMods.background.generic.x = 0;
	uiMods.background.generic.y = 0;
	uiMods.background.generic.width = 1024;
	uiMods.background.generic.height = 768;
	uiMods.background.pic = ART_BACKGROUND;

	uiMods.banner.generic.id = ID_BANNER;
	uiMods.banner.generic.type = QMTYPE_BITMAP;
	uiMods.banner.generic.flags = QMF_INACTIVE;
	uiMods.banner.generic.x = 0;
	uiMods.banner.generic.y = 66;
	uiMods.banner.generic.width = 1024;
	uiMods.banner.generic.height = 46;
	uiMods.banner.pic = ART_BANNER;

	uiMods.back.generic.id = ID_BACK;
	uiMods.back.generic.type = QMTYPE_BITMAP;
	uiMods.back.generic.x = 310;
	uiMods.back.generic.y = 656;
	uiMods.back.generic.width = 198;
	uiMods.back.generic.height = 38;
	uiMods.back.generic.callback = UI_Mods_Callback;
	uiMods.back.generic.ownerdraw	= UI_Mods_Ownerdraw;
	uiMods.back.pic = UI_BACKBUTTON;

	uiMods.load.generic.id = ID_LOAD;
	uiMods.load.generic.type = QMTYPE_BITMAP;
	uiMods.load.generic.x = 516;
	uiMods.load.generic.y = 656;
	uiMods.load.generic.width = 198;
	uiMods.load.generic.height = 38;
	uiMods.load.generic.callback = UI_Mods_Callback;
	uiMods.load.generic.ownerdraw	= UI_Mods_Ownerdraw;
	uiMods.load.pic = UI_LOADBUTTON;

	uiMods.modList.generic.id = ID_MODLIST;
	uiMods.modList.generic.type = QMTYPE_SCROLLLIST;
	uiMods.modList.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiMods.modList.generic.x = 256;
	uiMods.modList.generic.y = 208;
	uiMods.modList.generic.width = 512;
	uiMods.modList.generic.height = 352;
	uiMods.modList.generic.callback = UI_Mods_Callback;

	UI_Mods_GetModList();

	UI_AddItem( &uiMods.menu, (void *)&uiMods.background );
	UI_AddItem( &uiMods.menu, (void *)&uiMods.banner );
	UI_AddItem( &uiMods.menu, (void *)&uiMods.back );
	UI_AddItem( &uiMods.menu, (void *)&uiMods.load );
	UI_AddItem( &uiMods.menu, (void *)&uiMods.modList );
}

/*
=================
UI_Mods_Precache
=================
*/
void UI_Mods_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
}

/*
=================
UI_Mods_Menu
=================
*/
void UI_Mods_Menu( void )
{
	UI_Mods_Precache();
	UI_Mods_Init();

	UI_PushMenu( &uiMods.menu );
}