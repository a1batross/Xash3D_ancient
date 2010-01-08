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

#define ART_BANNER	     	"gfx/shell/head_save"

#define ID_BACKGROUND	0
#define ID_BANNER		1
#define ID_SAVE		2
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
	menuAction_s	save;
	menuAction_s	delete;
	menuAction_s	cancel;

	menuScrollList_s	savesList;

	menuBitmap_s	levelShot;
	menuAction_s	hintMessage;
	char		hintText[MAX_SYSPATH];
} uiSaveGame_t;

static uiSaveGame_t		uiSaveGame;


/*
=================
UI_SaveGame_GetGameList
=================
*/
static void UI_SaveGame_GetGameList( void )
{
	string	comment;
	search_t	*t;
	int	i = 0, j;

	t = FS_Search( "save/*.sav", true );

	if( cls.state == ca_active )
	{
		// create new entry for current save game
		com.strncpy( uiSaveGame.saveName[i], "new", CS_SIZE );
		com.strncat( uiSaveGame.saveDescription[i], "Current", TIME_LENGTH );
		com.strncat( uiSaveGame.saveDescription[i], uiEmptyString, TIME_LENGTH ); // fill remaining entries
		com.strncat( uiSaveGame.saveDescription[i], "New Saved Game", NAME_LENGTH );
		com.strncat( uiSaveGame.saveDescription[i], uiEmptyString, NAME_LENGTH );
		com.strncat( uiSaveGame.saveDescription[i], "New", GAMETIME_LENGTH );
		com.strncat( uiSaveGame.saveDescription[i], uiEmptyString, GAMETIME_LENGTH );
		uiSaveGame.saveDescriptionPtr[i] = uiSaveGame.saveDescription[i];
		i++;
	}

	for( j = 0; t && j < t->numfilenames; i++, j++ )
	{
		if( i >= UI_MAXGAMES ) break;
		
		if( !SV_GetComment( t->filenames[j], comment ))
		{
			if( com.strlen( comment ))
			{
				// get name string even if not found - SV_GetComment can be mark saves
				// as <CORRUPTED> <OLD VERSION> etc
				com.strncat( uiSaveGame.saveDescription[i], uiEmptyString, TIME_LENGTH );
				com.strncat( uiSaveGame.saveDescription[i], comment, NAME_LENGTH );
				com.strncat( uiSaveGame.saveDescription[i], uiEmptyString, NAME_LENGTH );
				uiSaveGame.saveDescriptionPtr[i] = uiSaveGame.saveDescription[i];
				FS_FileBase( t->filenames[j], uiSaveGame.saveName[i] );
				FS_FileBase( t->filenames[j], uiSaveGame.delName[i] );
			}
			else uiSaveGame.saveDescriptionPtr[i] = NULL;
			continue;
		}

		// strip path, leave only filename (empty slots doesn't have savename)
		FS_FileBase( t->filenames[j], uiSaveGame.saveName[i] );
		FS_FileBase( t->filenames[j], uiSaveGame.delName[i] );

		// fill save desc
		com.strncat( uiSaveGame.saveDescription[i], comment + CS_SIZE, TIME_LENGTH );
		com.strncat( uiSaveGame.saveDescription[i], " ", TIME_LENGTH );
		com.strncat( uiSaveGame.saveDescription[i], comment + CS_SIZE + CS_TIME, TIME_LENGTH );
		com.strncat( uiSaveGame.saveDescription[i], uiEmptyString, TIME_LENGTH ); // fill remaining entries
		com.strncat( uiSaveGame.saveDescription[i], comment, NAME_LENGTH );
		com.strncat( uiSaveGame.saveDescription[i], uiEmptyString, NAME_LENGTH );
		com.strncat( uiSaveGame.saveDescription[i], comment + CS_SIZE + (CS_TIME * 2), GAMETIME_LENGTH );
		com.strncat( uiSaveGame.saveDescription[i], uiEmptyString, GAMETIME_LENGTH );
		uiSaveGame.saveDescriptionPtr[i] = uiSaveGame.saveDescription[i];
	}

	for( ; i < UI_MAXGAMES; i++ ) uiSaveGame.saveDescriptionPtr[i] = NULL;
	uiSaveGame.savesList.itemNames = uiSaveGame.saveDescriptionPtr;

	if( com.strlen( uiSaveGame.saveName[0] ) == 0 || cls.state != ca_active )
		uiSaveGame.save.generic.flags |= QMF_GRAYED;
	else uiSaveGame.save.generic.flags &= ~QMF_GRAYED;

	if( com.strlen( uiSaveGame.delName[0] ) == 0 )
		uiSaveGame.delete.generic.flags |= QMF_GRAYED;
	else uiSaveGame.delete.generic.flags &= ~QMF_GRAYED;
	
	if( t ) Mem_Free( t );
}

/*
=================
UI_SaveGame_Callback
=================
*/
static void UI_SaveGame_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;
	string		pathJPG;

	if( event == QM_CHANGED )
	{
		// never overwrite existing saves, because their names was never get collision
		if( com.strlen( uiSaveGame.saveName[uiSaveGame.savesList.curItem] ) == 0 || cls.state != ca_active )
			uiSaveGame.save.generic.flags |= QMF_GRAYED;
		else uiSaveGame.save.generic.flags &= ~QMF_GRAYED;

		if( com.strlen( uiSaveGame.delName[uiSaveGame.savesList.curItem] ) == 0 )
			uiSaveGame.delete.generic.flags |= QMF_GRAYED;
		else uiSaveGame.delete.generic.flags &= ~QMF_GRAYED;
		return;
	}

	if( event != QM_ACTIVATED )
		return;
	
	switch( item->id )
	{
	case ID_CANCEL:
		UI_PopMenu();
		break;
	case ID_SAVE:
		if( com.strlen( uiSaveGame.saveName[uiSaveGame.savesList.curItem] ))
		{
			com.snprintf( pathJPG, sizeof( pathJPG ), "save/%s.jpg", uiSaveGame.saveName[uiSaveGame.savesList.curItem] );
			Cbuf_ExecuteText( EXEC_APPEND, va( "save \"%s\"\n", uiSaveGame.saveName[uiSaveGame.savesList.curItem] ));
			if( re ) re->FreeShader( pathJPG ); // unload shader from video-memory
			UI_CloseMenu();
		}
		break;
	case ID_DELETE:
		if( com.strlen( uiSaveGame.delName[uiSaveGame.savesList.curItem] ))
		{
			com.snprintf( pathJPG, sizeof( pathJPG ), "save/%s.jpg", uiSaveGame.delName[uiSaveGame.savesList.curItem] );
			Cbuf_ExecuteText( EXEC_NOW, va( "delsave \"%s\"\n", uiSaveGame.delName[uiSaveGame.savesList.curItem] ));
			if( re ) re->FreeShader( pathJPG ); // unload shader from video-memory
			UI_SaveGame_GetGameList();
		}
		break;
	}
}

/*
=================
UI_SaveGame_Ownerdraw
=================
*/
static void UI_SaveGame_Ownerdraw( void *self )
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

		if( com.strlen( uiSaveGame.saveName[uiSaveGame.savesList.curItem] ))
		{
			string	pathJPG;

			com.snprintf( pathJPG, sizeof( pathJPG ), "save/%s.jpg", uiSaveGame.saveName[uiSaveGame.savesList.curItem] );

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
UI_SaveGame_Init
=================
*/
static void UI_SaveGame_Init( void )
{
	Mem_Set( &uiSaveGame, 0, sizeof( uiSaveGame_t ));

	com.strncat( uiSaveGame.hintText, "Time", TIME_LENGTH );
	com.strncat( uiSaveGame.hintText, uiEmptyString, TIME_LENGTH );
	com.strncat( uiSaveGame.hintText, "Game", NAME_LENGTH );
	com.strncat( uiSaveGame.hintText, uiEmptyString, NAME_LENGTH );
	com.strncat( uiSaveGame.hintText, "Elapsed time", GAMETIME_LENGTH );
	com.strncat( uiSaveGame.hintText, uiEmptyString, GAMETIME_LENGTH );

	uiSaveGame.background.generic.id = ID_BACKGROUND;
	uiSaveGame.background.generic.type = QMTYPE_BITMAP;
	uiSaveGame.background.generic.flags = QMF_INACTIVE;
	uiSaveGame.background.generic.x = 0;
	uiSaveGame.background.generic.y = 0;
	uiSaveGame.background.generic.width = 1024;
	uiSaveGame.background.generic.height = 768;
	uiSaveGame.background.pic = ART_BACKGROUND;

	uiSaveGame.banner.generic.id = ID_BANNER;
	uiSaveGame.banner.generic.type = QMTYPE_BITMAP;
	uiSaveGame.banner.generic.flags = QMF_INACTIVE;
	uiSaveGame.banner.generic.x = UI_BANNER_POSX;
	uiSaveGame.banner.generic.y = UI_BANNER_POSY;
	uiSaveGame.banner.generic.width = UI_BANNER_WIDTH;
	uiSaveGame.banner.generic.height = UI_BANNER_HEIGHT;
	uiSaveGame.banner.pic = ART_BANNER;

	uiSaveGame.save.generic.id = ID_SAVE;
	uiSaveGame.save.generic.type = QMTYPE_ACTION;
	uiSaveGame.save.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiSaveGame.save.generic.x = 72;
	uiSaveGame.save.generic.y = 230;
	uiSaveGame.save.generic.name = "Save";
	uiSaveGame.save.generic.statusText = "Save current game";
	uiSaveGame.save.generic.callback = UI_SaveGame_Callback;

	uiSaveGame.delete.generic.id = ID_DELETE;
	uiSaveGame.delete.generic.type = QMTYPE_ACTION;
	uiSaveGame.delete.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiSaveGame.delete.generic.x = 72;
	uiSaveGame.delete.generic.y = 280;
	uiSaveGame.delete.generic.name = "Delete";
	uiSaveGame.delete.generic.statusText = "Delete saved game";
	uiSaveGame.delete.generic.callback = UI_SaveGame_Callback;

	uiSaveGame.cancel.generic.id = ID_CANCEL;
	uiSaveGame.cancel.generic.type = QMTYPE_ACTION;
	uiSaveGame.cancel.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiSaveGame.cancel.generic.x = 72;
	uiSaveGame.cancel.generic.y = 330;
	uiSaveGame.cancel.generic.name = "Cancel";
	uiSaveGame.cancel.generic.statusText = "Return back to main menu";
	uiSaveGame.cancel.generic.callback = UI_SaveGame_Callback;

	uiSaveGame.hintMessage.generic.id = ID_TABLEHINT;
	uiSaveGame.hintMessage.generic.type = QMTYPE_ACTION;
	uiSaveGame.hintMessage.generic.flags = QMF_INACTIVE|QMF_SMALLFONT;
	uiSaveGame.hintMessage.generic.color = uiColorLtGrey;
	uiSaveGame.hintMessage.generic.name = uiSaveGame.hintText;
	uiSaveGame.hintMessage.generic.x = 360;
	uiSaveGame.hintMessage.generic.y = 225;

	uiSaveGame.levelShot.generic.id = ID_LEVELSHOT;
	uiSaveGame.levelShot.generic.type = QMTYPE_BITMAP;
	uiSaveGame.levelShot.generic.flags = QMF_INACTIVE;
	uiSaveGame.levelShot.generic.x = LEVELSHOT_X;
	uiSaveGame.levelShot.generic.y = LEVELSHOT_Y;
	uiSaveGame.levelShot.generic.width = LEVELSHOT_W;
	uiSaveGame.levelShot.generic.height = LEVELSHOT_H;
	uiSaveGame.levelShot.generic.ownerdraw = UI_SaveGame_Ownerdraw;

	uiSaveGame.savesList.generic.id = ID_SAVELIST;
	uiSaveGame.savesList.generic.type = QMTYPE_SCROLLLIST;
	uiSaveGame.savesList.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_SMALLFONT;
	uiSaveGame.savesList.generic.x = 360;
	uiSaveGame.savesList.generic.y = 255;
	uiSaveGame.savesList.generic.width = 640;
	uiSaveGame.savesList.generic.height = 440;
	uiSaveGame.savesList.generic.callback = UI_SaveGame_Callback;

	UI_SaveGame_GetGameList();

	UI_AddItem( &uiSaveGame.menu, (void *)&uiSaveGame.background );
	UI_AddItem( &uiSaveGame.menu, (void *)&uiSaveGame.banner );
	UI_AddItem( &uiSaveGame.menu, (void *)&uiSaveGame.save );
	UI_AddItem( &uiSaveGame.menu, (void *)&uiSaveGame.delete );
	UI_AddItem( &uiSaveGame.menu, (void *)&uiSaveGame.cancel );
	UI_AddItem( &uiSaveGame.menu, (void *)&uiSaveGame.hintMessage );
	UI_AddItem( &uiSaveGame.menu, (void *)&uiSaveGame.levelShot );
	UI_AddItem( &uiSaveGame.menu, (void *)&uiSaveGame.savesList );
}

/*
=================
UI_SaveGame_Precache
=================
*/
void UI_SaveGame_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
}

/*
=================
UI_SaveGame_Menu
=================
*/
void UI_SaveGame_Menu( void )
{
	string	libpath;

	Com_BuildPath( "server", libpath );
	if( !FS_FileExists( libpath )) return;

	UI_SaveGame_Precache();
	UI_SaveGame_Init();

	UI_PushMenu( &uiSaveGame.menu );
}