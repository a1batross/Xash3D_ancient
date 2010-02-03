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

#define ART_BANNER		"gfx/shell/head_custom"

#define ID_BACKGROUND	0
#define ID_BANNER		1
#define ID_ACTIVATE		2
#define ID_DONE		3
#define ID_GOTOSITE		4
#define ID_MODLIST		5
#define ID_TABLEHINT	6

#define TYPE_LENGTH		10
#define NAME_LENGTH		32+TYPE_LENGTH
#define VER_LENGTH		10+NAME_LENGTH
#define SIZE_LENGTH		10+VER_LENGTH

typedef struct
{
	string		modsDir[MAX_MODS];
	string		modsWebSites[MAX_MODS];
	string		modsDescription[MAX_MODS];
	char		*modsDescriptionPtr[MAX_MODS];

	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;
	menuAction_s	load;
	menuAction_s	go2url;
	menuAction_s	done;

	menuScrollList_s	modList;
	menuAction_s	hintMessage;
	char		hintText[MAX_SYSPATH];
} uiCustomGame_t;

static uiCustomGame_t	uiCustomGame;

/*
=================
UI_CustomGame_GetModList
=================
*/
static void UI_CustomGame_GetModList( void )
{
	int	i;

	Com_Assert( SIZE_LENGTH >= MAX_STRING );

	for( i = 0; i < SI->numgames; i++ )
	{
		com.strncpy( uiCustomGame.modsDir[i], SI->games[i]->gamefolder, sizeof( uiCustomGame.modsDir[i] ));
		com.strncpy( uiCustomGame.modsWebSites[i], SI->games[i]->game_url, sizeof( uiCustomGame.modsWebSites[i] ));

		if( com.strlen( SI->games[i]->type ))
			com.strncat( uiCustomGame.modsDescription[i], SI->games[i]->type, TYPE_LENGTH );
		com.strncat( uiCustomGame.modsDescription[i], uiEmptyString, TYPE_LENGTH );
		com.strncat( uiCustomGame.modsDescription[i], SI->games[i]->title, NAME_LENGTH );
		com.strncat( uiCustomGame.modsDescription[i], uiEmptyString, NAME_LENGTH );
		com.strncat( uiCustomGame.modsDescription[i], va( "%g", SI->games[i]->version ), VER_LENGTH );
		com.strncat( uiCustomGame.modsDescription[i], uiEmptyString, VER_LENGTH );
		if( SI->games[i]->size > 0 )
			com.strncat( uiCustomGame.modsDescription[i], com.pretifymem( SI->games[i]->size, 0 ), SIZE_LENGTH );
		else com.strncat( uiCustomGame.modsDescription[i], "0.0 Mb", SIZE_LENGTH );     
		com.strncat( uiCustomGame.modsDescription[i], uiEmptyString, SIZE_LENGTH );
		uiCustomGame.modsDescriptionPtr[i] = uiCustomGame.modsDescription[i];

		if( !com.strcmp( SI->GameInfo->gamefolder, SI->games[i]->gamefolder ))
			uiCustomGame.modList.curItem = i;
	}
	for( ; i < MAX_MODS; i++ ) uiCustomGame.modsDescriptionPtr[i] = NULL;

	uiCustomGame.modList.itemNames = uiCustomGame.modsDescriptionPtr;

	// see if the load button should be grayed
	if( !com.stricmp( GI->gamefolder, uiCustomGame.modsDir[uiCustomGame.modList.curItem] ))
		uiCustomGame.load.generic.flags |= QMF_GRAYED;
	if( com.strlen( uiCustomGame.modsWebSites[0] ) == 0 )
		uiCustomGame.go2url.generic.flags |= QMF_GRAYED;
}

/*
=================
UI_CustomGame_Callback
=================
*/
static void UI_CustomGame_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event == QM_CHANGED )
	{
		// aee if the load button should be grayed
		if( !com.stricmp( GI->gamefolder, uiCustomGame.modsDir[uiCustomGame.modList.curItem] ))
			uiCustomGame.load.generic.flags |= QMF_GRAYED;
		else uiCustomGame.load.generic.flags &= ~QMF_GRAYED;

		if( com.strlen( uiCustomGame.modsWebSites[uiCustomGame.modList.curItem] ) == 0 )
			uiCustomGame.go2url.generic.flags |= QMF_GRAYED;
		else uiCustomGame.go2url.generic.flags &= ~QMF_GRAYED;
		return;
	}

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_DONE:
		UI_PopMenu();
		break;
	case ID_GOTOSITE:
		if( com.strlen( uiCustomGame.modsWebSites[uiCustomGame.modList.curItem] ))
			Sys_ShellExecute( uiCustomGame.modsWebSites[uiCustomGame.modList.curItem], NULL, false );
		break;
	case ID_ACTIVATE:
		if( cls.state == ca_active )
			break;	// don't fuck up the game

		// restart all engine systems with new game
		Cbuf_ExecuteText( EXEC_APPEND, va( "game %s\n", uiCustomGame.modsDir[uiCustomGame.modList.curItem] ));
		break;
	}
}

/*
=================
UI_CustomGame_Init
=================
*/
static void UI_CustomGame_Init( void )
{
	Mem_Set( &uiCustomGame, 0, sizeof( uiCustomGame_t ));

	com.strncat( uiCustomGame.hintText, "Type", TYPE_LENGTH );
	com.strncat( uiCustomGame.hintText, uiEmptyString, TYPE_LENGTH );
	com.strncat( uiCustomGame.hintText, "Name", NAME_LENGTH );
	com.strncat( uiCustomGame.hintText, uiEmptyString, NAME_LENGTH );
	com.strncat( uiCustomGame.hintText, "Version", VER_LENGTH );
	com.strncat( uiCustomGame.hintText, uiEmptyString, VER_LENGTH );
	com.strncat( uiCustomGame.hintText, "Size", SIZE_LENGTH );
	com.strncat( uiCustomGame.hintText, uiEmptyString, SIZE_LENGTH );

	uiCustomGame.background.generic.id = ID_BACKGROUND;
	uiCustomGame.background.generic.type = QMTYPE_BITMAP;
	uiCustomGame.background.generic.flags = QMF_INACTIVE;
	uiCustomGame.background.generic.x = 0;
	uiCustomGame.background.generic.y = 0;
	uiCustomGame.background.generic.width = 1024;
	uiCustomGame.background.generic.height = 768;
	uiCustomGame.background.pic = ART_BACKGROUND;

	uiCustomGame.banner.generic.id = ID_BANNER;
	uiCustomGame.banner.generic.type = QMTYPE_BITMAP;
	uiCustomGame.banner.generic.flags = QMF_INACTIVE;
	uiCustomGame.banner.generic.x = UI_BANNER_POSX;
	uiCustomGame.banner.generic.y = UI_BANNER_POSY;
	uiCustomGame.banner.generic.width = UI_BANNER_WIDTH;
	uiCustomGame.banner.generic.height = UI_BANNER_HEIGHT;
	uiCustomGame.banner.pic = ART_BANNER;

	uiCustomGame.load.generic.id = ID_ACTIVATE;
	uiCustomGame.load.generic.type = QMTYPE_ACTION;
	uiCustomGame.load.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiCustomGame.load.generic.x = 72;
	uiCustomGame.load.generic.y = 230;
	uiCustomGame.load.generic.name = "Activate";
	uiCustomGame.load.generic.statusText = "Activate selected custom game";
	uiCustomGame.load.generic.callback = UI_CustomGame_Callback;

	uiCustomGame.go2url.generic.id = ID_GOTOSITE;
	uiCustomGame.go2url.generic.type = QMTYPE_ACTION;
	uiCustomGame.go2url.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiCustomGame.go2url.generic.x = 72;
	uiCustomGame.go2url.generic.y = 280;
	uiCustomGame.go2url.generic.name = "Visit web site";
	uiCustomGame.go2url.generic.statusText = "Visit the web site of game developrs";
	uiCustomGame.go2url.generic.callback = UI_CustomGame_Callback;

	uiCustomGame.done.generic.id = ID_DONE;
	uiCustomGame.done.generic.type = QMTYPE_ACTION;
	uiCustomGame.done.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiCustomGame.done.generic.x = 72;
	uiCustomGame.done.generic.y = 330;
	uiCustomGame.done.generic.name = "Done";
	uiCustomGame.done.generic.statusText = "Return to main menu";
	uiCustomGame.done.generic.callback = UI_CustomGame_Callback;

	uiCustomGame.hintMessage.generic.id = ID_TABLEHINT;
	uiCustomGame.hintMessage.generic.type = QMTYPE_ACTION;
	uiCustomGame.hintMessage.generic.flags = QMF_INACTIVE|QMF_SMALLFONT;
	uiCustomGame.hintMessage.generic.color = uiColorHelp;
	uiCustomGame.hintMessage.generic.name = uiCustomGame.hintText;
	uiCustomGame.hintMessage.generic.x = 360;
	uiCustomGame.hintMessage.generic.y = 225;

	uiCustomGame.modList.generic.id = ID_MODLIST;
	uiCustomGame.modList.generic.type = QMTYPE_SCROLLLIST;
	uiCustomGame.modList.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_SMALLFONT;
	uiCustomGame.modList.generic.x = 360;
	uiCustomGame.modList.generic.y = 255;
	uiCustomGame.modList.generic.width = 640;
	uiCustomGame.modList.generic.height = 440;
	uiCustomGame.modList.generic.callback = UI_CustomGame_Callback;

	UI_CustomGame_GetModList();

	UI_AddItem( &uiCustomGame.menu, (void *)&uiCustomGame.background );
	UI_AddItem( &uiCustomGame.menu, (void *)&uiCustomGame.banner );
	UI_AddItem( &uiCustomGame.menu, (void *)&uiCustomGame.load );
	UI_AddItem( &uiCustomGame.menu, (void *)&uiCustomGame.go2url );
	UI_AddItem( &uiCustomGame.menu, (void *)&uiCustomGame.done );
	UI_AddItem( &uiCustomGame.menu, (void *)&uiCustomGame.hintMessage );
	UI_AddItem( &uiCustomGame.menu, (void *)&uiCustomGame.modList );
}

/*
=================
UI_CustomGame_Precache
=================
*/
void UI_CustomGame_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
}

/*
=================
UI_CustomGame_Menu
=================
*/
void UI_CustomGame_Menu( void )
{
	UI_CustomGame_Precache();
	UI_CustomGame_Init();

	UI_PushMenu( &uiCustomGame.menu );
}