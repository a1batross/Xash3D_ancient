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
#include "client.h"

#define ART_BACKGROUND   	"gfx/shell/splash"
#define ART_BANNER	     	"gfx/shell/head_load"

#define ID_BACKGROUND	0
#define ID_BANNER		1
#define ID_LOAD		2
#define ID_DELETE		3
#define ID_CANCEL		4
#define ID_SAVELIST		5
#define ID_TABLEHINT	6
#define ID_LEVELSHOT	7

#define LEVELSHOT_X		72
#define LEVELSHOT_Y		400
#define LEVELSHOT_W		192
#define LEVELSHOT_H		160

#define TIME_LENGTH		20
#define NAME_LENGTH		32+TIME_LENGTH
#define GAMETIME_LENGTH	15+NAME_LENGTH

typedef struct
{
	char		saveName[UI_MAXGAMES][CS_SIZE];
	char		delName[UI_MAXGAMES][CS_SIZE];
	string		saveDescription[UI_MAXGAMES];
	char		*saveDescriptionPtr[UI_MAXGAMES];

	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;
	menuAction_s	load;
	menuAction_s	delete;
	menuAction_s	cancel;

	menuScrollList_s	savesList;

	menuBitmap_s	levelShot;
	menuAction_s	hintMessage;
	char		hintText[MAX_SYSPATH];
} uiLoadGame_t;

static uiLoadGame_t		uiLoadGame;


/*
=================
UI_LoadGame_GetGameList
=================
*/
static void UI_LoadGame_GetGameList( void )
{
	string	comment;
	search_t	*t;
	int	i;

	t = FS_Search( "save/*.sav", true );

	for( i = 0; t && i < t->numfilenames; i++ )
	{
		if( i >= UI_MAXGAMES ) break;
		
		if( !SV_GetComment( t->filenames[i], comment ))
		{
			if( com.strlen( comment ))
			{
				// get name string even if not found - SV_GetComment can be mark saves
				// as <CORRUPTED> <OLD VERSION> etc
				com.strncat( uiLoadGame.saveDescription[i], uiEmptyString, TIME_LENGTH );
				com.strncat( uiLoadGame.saveDescription[i], comment, NAME_LENGTH );
				com.strncat( uiLoadGame.saveDescription[i], uiEmptyString, NAME_LENGTH );
				uiLoadGame.saveDescriptionPtr[i] = uiLoadGame.saveDescription[i];
				FS_FileBase( t->filenames[i], uiLoadGame.delName[i] );
			}
			else uiLoadGame.saveDescriptionPtr[i] = NULL;
			continue;
		}

		// strip path, leave only filename (empty slots doesn't have savename)
		FS_FileBase( t->filenames[i], uiLoadGame.saveName[i] );
		FS_FileBase( t->filenames[i], uiLoadGame.delName[i] );

		// fill save desc
		com.strncat( uiLoadGame.saveDescription[i], comment + CS_SIZE, TIME_LENGTH );
		com.strncat( uiLoadGame.saveDescription[i], " ", TIME_LENGTH );
		com.strncat( uiLoadGame.saveDescription[i], comment + CS_SIZE + CS_TIME, TIME_LENGTH );
		com.strncat( uiLoadGame.saveDescription[i], uiEmptyString, TIME_LENGTH ); // fill remaining entries
		com.strncat( uiLoadGame.saveDescription[i], comment, NAME_LENGTH );
		com.strncat( uiLoadGame.saveDescription[i], uiEmptyString, NAME_LENGTH );
		com.strncat( uiLoadGame.saveDescription[i], comment + CS_SIZE + (CS_TIME * 2), GAMETIME_LENGTH );
		com.strncat( uiLoadGame.saveDescription[i], uiEmptyString, GAMETIME_LENGTH );
		uiLoadGame.saveDescriptionPtr[i] = uiLoadGame.saveDescription[i];
	}

	for( ; i < UI_MAXGAMES; i++ ) uiLoadGame.saveDescriptionPtr[i] = NULL;
	uiLoadGame.savesList.itemNames = uiLoadGame.saveDescriptionPtr;

	if( com.strlen( uiLoadGame.saveName[0] ) == 0 )
		uiLoadGame.load.generic.flags |= QMF_GRAYED;
	else uiLoadGame.load.generic.flags &= ~QMF_GRAYED;

	if( com.strlen( uiLoadGame.delName[0] ) == 0 )
		uiLoadGame.delete.generic.flags |= QMF_GRAYED;
	else uiLoadGame.delete.generic.flags &= ~QMF_GRAYED;
	
	if( t ) Mem_Free( t );
}

/*
=================
UI_LoadGame_Callback
=================
*/
static void UI_LoadGame_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event == QM_CHANGED )
	{
		if( com.strlen( uiLoadGame.saveName[uiLoadGame.savesList.curItem] ) == 0 )
			uiLoadGame.load.generic.flags |= QMF_GRAYED;
		else uiLoadGame.load.generic.flags &= ~QMF_GRAYED;

		if( com.strlen( uiLoadGame.delName[uiLoadGame.savesList.curItem] ) == 0 )
			uiLoadGame.delete.generic.flags |= QMF_GRAYED;
		else uiLoadGame.delete.generic.flags &= ~QMF_GRAYED;
		return;
	}

	if( event != QM_ACTIVATED )
		return;
	
	switch( item->id )
	{
	case ID_CANCEL:
		UI_PopMenu();
		break;
	case ID_LOAD:
		if( com.strlen( uiLoadGame.saveName[uiLoadGame.savesList.curItem] ))
			Cbuf_ExecuteText( EXEC_APPEND, va( "load \"%s\"\n", uiLoadGame.saveName[uiLoadGame.savesList.curItem] ));
		break;
	case ID_DELETE:
		if( com.strlen( uiLoadGame.delName[uiLoadGame.savesList.curItem] ))
		{
			Cbuf_ExecuteText( EXEC_NOW, va( "delete \"%s\"\n", uiLoadGame.delName[uiLoadGame.savesList.curItem] ));
			UI_LoadGame_GetGameList();
		}
		break;
	}
}

/*
=================
UI_LoadGame_Ownerdraw
=================
*/
static void UI_LoadGame_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( item->type != QMTYPE_ACTION && item->id == ID_LEVELSHOT )
	{
		int	x, y, w, h;

		// draw the levelshot
		x = LEVELSHOT_X;
		y = LEVELSHOT_Y;
		w = LEVELSHOT_W;
		h = LEVELSHOT_H;
		
		UI_ScaleCoords( &x, &y, &w, &h );

		if( com.strlen( uiLoadGame.saveName[uiLoadGame.savesList.curItem] ))
		{
			string	pathJPG;

			com.snprintf( pathJPG, sizeof( pathJPG ), "save/%s.jpg", uiLoadGame.saveName[uiLoadGame.savesList.curItem] );

			if( !FS_FileExists( pathJPG ))
				UI_DrawPic( x, y, w, h, uiColorWhite, "gfx/hud/static" );
			else UI_DrawPic( x, y, w, h, uiColorWhite, pathJPG );
		}
		else UI_DrawPic( x, y, w, h, uiColorWhite, "gfx/hud/static" );

		// draw the rectangle
		UI_DrawRectangle( item->x, item->y, item->width, item->height, uiScrollOutlineColor );
	}
}

/*
=================
UI_LoadGame_Init
=================
*/
static void UI_LoadGame_Init( void )
{
	Mem_Set( &uiLoadGame, 0, sizeof( uiLoadGame_t ));

	com.strncat( uiLoadGame.hintText, "Time", TIME_LENGTH );
	com.strncat( uiLoadGame.hintText, uiEmptyString, TIME_LENGTH );
	com.strncat( uiLoadGame.hintText, "Game", NAME_LENGTH );
	com.strncat( uiLoadGame.hintText, uiEmptyString, NAME_LENGTH );
	com.strncat( uiLoadGame.hintText, "Elapsed time", GAMETIME_LENGTH );
	com.strncat( uiLoadGame.hintText, uiEmptyString, GAMETIME_LENGTH );

	uiLoadGame.background.generic.id = ID_BACKGROUND;
	uiLoadGame.background.generic.type = QMTYPE_BITMAP;
	uiLoadGame.background.generic.flags = QMF_INACTIVE;
	uiLoadGame.background.generic.x = 0;
	uiLoadGame.background.generic.y = 0;
	uiLoadGame.background.generic.width = 1024;
	uiLoadGame.background.generic.height = 768;
	uiLoadGame.background.pic = ART_BACKGROUND;

	uiLoadGame.banner.generic.id = ID_BANNER;
	uiLoadGame.banner.generic.type = QMTYPE_BITMAP;
	uiLoadGame.banner.generic.flags = QMF_INACTIVE;
	uiLoadGame.banner.generic.x = 65;
	uiLoadGame.banner.generic.y = 92;
	uiLoadGame.banner.generic.width = 690;
	uiLoadGame.banner.generic.height = 120;
	uiLoadGame.banner.pic = ART_BANNER;

	uiLoadGame.load.generic.id = ID_LOAD;
	uiLoadGame.load.generic.type = QMTYPE_ACTION;
	uiLoadGame.load.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiLoadGame.load.generic.x = 72;
	uiLoadGame.load.generic.y = 230;
	uiLoadGame.load.generic.name = "Load";
	uiLoadGame.load.generic.statusText = "Load saved game";
	uiLoadGame.load.generic.callback = UI_LoadGame_Callback;

	uiLoadGame.delete.generic.id = ID_DELETE;
	uiLoadGame.delete.generic.type = QMTYPE_ACTION;
	uiLoadGame.delete.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiLoadGame.delete.generic.x = 72;
	uiLoadGame.delete.generic.y = 280;
	uiLoadGame.delete.generic.name = "Delete";
	uiLoadGame.delete.generic.statusText = "Delete saved game";
	uiLoadGame.delete.generic.callback = UI_LoadGame_Callback;

	uiLoadGame.cancel.generic.id = ID_CANCEL;
	uiLoadGame.cancel.generic.type = QMTYPE_ACTION;
	uiLoadGame.cancel.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiLoadGame.cancel.generic.x = 72;
	uiLoadGame.cancel.generic.y = 330;
	uiLoadGame.cancel.generic.name = "Cancel";
	uiLoadGame.cancel.generic.statusText = "Return back to main menu";
	uiLoadGame.cancel.generic.callback = UI_LoadGame_Callback;

	uiLoadGame.hintMessage.generic.id = ID_TABLEHINT;
	uiLoadGame.hintMessage.generic.type = QMTYPE_ACTION;
	uiLoadGame.hintMessage.generic.flags = QMF_INACTIVE|QMF_SMALLFONT;
	uiLoadGame.hintMessage.generic.color = uiColorLtGrey;
	uiLoadGame.hintMessage.generic.name = uiLoadGame.hintText;
	uiLoadGame.hintMessage.generic.x = 360;
	uiLoadGame.hintMessage.generic.y = 225;

	uiLoadGame.levelShot.generic.id = ID_LEVELSHOT;
	uiLoadGame.levelShot.generic.type = QMTYPE_BITMAP;
	uiLoadGame.levelShot.generic.flags = QMF_INACTIVE;
	uiLoadGame.levelShot.generic.x = LEVELSHOT_X;
	uiLoadGame.levelShot.generic.y = LEVELSHOT_Y;
	uiLoadGame.levelShot.generic.width = LEVELSHOT_W;
	uiLoadGame.levelShot.generic.height = LEVELSHOT_H;
	uiLoadGame.levelShot.generic.ownerdraw = UI_LoadGame_Ownerdraw;

	uiLoadGame.savesList.generic.id = ID_SAVELIST;
	uiLoadGame.savesList.generic.type = QMTYPE_SCROLLLIST;
	uiLoadGame.savesList.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_SMALLFONT;
	uiLoadGame.savesList.generic.x = 360;
	uiLoadGame.savesList.generic.y = 255;
	uiLoadGame.savesList.generic.width = 640;
	uiLoadGame.savesList.generic.height = 440;
	uiLoadGame.savesList.generic.callback = UI_LoadGame_Callback;

	UI_LoadGame_GetGameList();

	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.background );
	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.banner );
	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.load );
	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.delete );
	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.cancel );
	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.hintMessage );
	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.levelShot );
	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.savesList );
}

/*
=================
UI_LoadGame_Precache
=================
*/
void UI_LoadGame_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
}

/*
=================
UI_LoadGame_Menu
=================
*/
void UI_LoadGame_Menu( void )
{
	string	libpath;

	Com_BuildPath( "server", libpath );
	if( !FS_FileExists( libpath )) return;

	UI_LoadGame_Precache();
	UI_LoadGame_Init();

	UI_PushMenu( &uiLoadGame.menu );
}