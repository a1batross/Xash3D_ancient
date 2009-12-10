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

#define ART_BACKGROUND   	"gfx/shell/splash"
#define ART_BANNER	     	"gfx/shell/banners/loadgame_t"
#define ART_LISTBACK     	"gfx/shell/segments/files_box"
#define ART_LEVELSHOTBLUR	"gfx/shell/segments/sp_mapshot"

#define UI_MAXGAMES		14

#define ID_BACKGROUND	0
#define ID_BANNER		1

#define ID_BACK		2
#define ID_LOAD		3

#define ID_LISTBACK		4
#define ID_GAMETITLE	5
#define ID_LISTGAMES	6
#define ID_LEVELSHOT	20

#define ID_NEWGAME		21
#define ID_SAVEGAME		22
#define ID_DELETEGAME	23

typedef struct
{
	char		map[CS_SIZE];
	char		time[CS_TIME];
	char		date[CS_TIME];
	char		name[CS_SIZE];
	bool		valid;
} uiLoadGameGame_t;

static rgba_t		uiLoadGameColor = { 0, 76, 127, 255 };

typedef struct
{
	uiLoadGameGame_t	games[UI_MAXGAMES];
	int		currentGame;

	int		currentLevelShot;
	long		fadeTime;

	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;

	menuBitmap_s	back;
	menuBitmap_s	load;

	menuBitmap_s	listBack;
	menuAction_s	gameTitle;
	menuAction_s	listGames[UI_MAXGAMES];	
	menuBitmap_s	levelShot;

	menuBitmap_s	newGame;
	menuBitmap_s	saveGame;
	menuBitmap_s	deleteGame;

} uiLoadGame_t;

static uiLoadGame_t		uiLoadGame;


/*
=================
UI_LoadGame_GetGameList
=================
*/
static void UI_LoadGame_GetGameList( void )
{
	string	name;
	int	i;

	for( i = 0; i < UI_MAXGAMES; i++ )
	{
		if( !SV_GetComment( name, i ))
		{
			// get name string even if not found - SV_GetComment can be mark saves
			// as <CORRUPTED> <OLD VERSION> <EMPTY> etc
			com.strncpy(uiLoadGame.games[i].name, name, sizeof( uiLoadGame.games[i].name ));
			com.strncpy(uiLoadGame.games[i].map, "", sizeof( uiLoadGame.games[i].map ));
			com.strncpy(uiLoadGame.games[i].time, "", sizeof( uiLoadGame.games[i].time ));
			com.strncpy(uiLoadGame.games[i].date, "", sizeof( uiLoadGame.games[i].date ));
			uiLoadGame.games[i].valid = false;
			continue;
		}

		com.strncpy( uiLoadGame.games[i].map, name + CS_SIZE + (CS_TIME * 2), CS_SIZE );
		com.strncpy( uiLoadGame.games[i].time, name + CS_SIZE + CS_TIME, CS_TIME );
		com.strncpy( uiLoadGame.games[i].date, name + CS_SIZE, CS_TIME );
		com.strncpy( uiLoadGame.games[i].name, name, CS_SIZE );
		uiLoadGame.games[i].valid = true;
	}

	// select first valid slot
	for( i = 0; i < UI_MAXGAMES; i++ )
	{
		if( uiLoadGame.games[i].valid )
		{
			uiLoadGame.listGames[i].generic.color = uiColorOrange;
			uiLoadGame.currentGame = i;
			break;
		}
	}

	// couldn't find a valid slot, so gray load button
	if( i == UI_MAXGAMES ) uiLoadGame.load.generic.flags |= QMF_GRAYED;
}

/*
=================
UI_LoadGame_Callback
=================
*/
static void UI_LoadGame_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event != QM_ACTIVATED )
		return;

	if( item->type == QMTYPE_ACTION )
	{
		// reset color, get current game, set color
		uiLoadGame.listGames[uiLoadGame.currentGame].generic.color = uiColorOrange;
		uiLoadGame.currentGame = item->id - ID_LISTGAMES;
		uiLoadGame.listGames[uiLoadGame.currentGame].generic.color = uiColorYellow;

		// restart levelshot animation
		uiLoadGame.currentLevelShot = 0;
		uiLoadGame.fadeTime = uiStatic.realTime;
		return;
	}
	
	switch( item->id )
	{
	case ID_BACK:
		UI_PopMenu();
		break;
	case ID_LOAD:
		if( uiLoadGame.games[uiLoadGame.currentGame].valid )
			Cbuf_ExecuteText( EXEC_APPEND, va( "load save%i\n", uiLoadGame.currentGame ));
		break;
	case ID_NEWGAME:
		UI_NewGame_Menu();
		break;
	case ID_SAVEGAME:
		UI_SaveGame_Menu();
		break;
	case ID_DELETEGAME:
		Cbuf_ExecuteText( EXEC_NOW, va( "delete save%i\n", uiLoadGame.currentGame ));
		UI_LoadGame_GetGameList();
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

	if( item->type == QMTYPE_ACTION )
	{
		rgba_t	color;
		char	*time, *date, *name;
		bool	centered;

		if( item->id == ID_GAMETITLE )
		{
			time = "Time";
			date = "Date";
			name = "Map Name";
			centered = false;
		}
		else
		{
			time = uiLoadGame.games[item->id - ID_LISTGAMES].time;
			date = uiLoadGame.games[item->id - ID_LISTGAMES].date;
			name = uiLoadGame.games[item->id - ID_LISTGAMES].name;
			centered = !uiLoadGame.games[item->id - ID_LISTGAMES].valid;

			if( com.strstr( uiLoadGame.games[item->id - ID_LISTGAMES].name, "<empty>" ))
				centered = true;
			if( com.strstr( uiLoadGame.games[item->id - ID_LISTGAMES].name, "<corrupted>" ))
				centered = true;
		}

		if( !centered )
		{
			UI_DrawString( item->x, item->y, 82*uiStatic.scaleX, item->height, time, item->color, true, item->charWidth, item->charHeight, 1, true );
			UI_DrawString( item->x + 83*uiStatic.scaleX, item->y, 82*uiStatic.scaleX, item->height, date, item->color, true, item->charWidth, item->charHeight, 1, true );
			UI_DrawString( item->x + 83*uiStatic.scaleX + 83*uiStatic.scaleX, item->y, 296*uiStatic.scaleX, item->height, name, item->color, true, item->charWidth, item->charHeight, 1, true );
		}
		else UI_DrawString( item->x, item->y, item->width, item->height, name, item->color, true, item->charWidth, item->charHeight, 1, true );

		if( self != UI_ItemAtCursor( item->parent ))
			return;

		*(uint *)color = *(uint *)item->color;

		if( !centered )
		{
			UI_DrawString(item->x, item->y, 82*uiStatic.scaleX, item->height, time, color, true, item->charWidth, item->charHeight, 1, true);
			UI_DrawString(item->x + 83*uiStatic.scaleX, item->y, 82*uiStatic.scaleX, item->height, date, color, true, item->charWidth, item->charHeight, 1, true);
			UI_DrawString(item->x + 83*uiStatic.scaleX + 83*uiStatic.scaleX, item->y, 296*uiStatic.scaleX, item->height, name, color, true, item->charWidth, item->charHeight, 1, true);
		}
		else UI_DrawString(item->x, item->y, item->width, item->height, name, color, true, item->charWidth, item->charHeight, 1, true);
	}
	else
	{
		if( item->id == ID_LEVELSHOT )
		{
			int	x, y, w, h;
			int	prev;
			rgba_t	color = { 255, 255, 255, 255 };

			// draw the levelshot
			x = 570;
			y = 210;
			w = 410;
			h = 202;
		
			UI_ScaleCoords( &x, &y, &w, &h );

			if( uiLoadGame.games[uiLoadGame.currentGame].map[0] )
			{
				string	pathJPG;

				if( uiStatic.realTime - uiLoadGame.fadeTime >= 3000 )
				{
					uiLoadGame.fadeTime = uiStatic.realTime;

					uiLoadGame.currentLevelShot++;
					if( uiLoadGame.currentLevelShot == 3 )
						uiLoadGame.currentLevelShot = 0;
				}

				prev = uiLoadGame.currentLevelShot - 1;
				if( prev < 0 ) prev = 2;

				color[3] = bound( 0.0f, (float)(uiStatic.realTime - uiLoadGame.fadeTime) * 0.001f, 1.0f ) * 255;

				com.snprintf( pathJPG, sizeof( pathJPG ), "save/save%i.jpg", uiLoadGame.currentGame );

				if( !FS_FileExists( pathJPG ))
					UI_DrawPic( x, y, w, h, uiColorWhite, "gfx/hud/static" );
				else UI_DrawPic( x, y, w, h, uiColorWhite, pathJPG );
			}
			else UI_DrawPic( x, y, w, h, uiColorWhite, "gfx/hud/static" );

			// draw the blurred frame
			UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
		}
		else
		{
			if( uiLoadGame.menu.items[uiLoadGame.menu.cursor] == self )
				UI_DrawPic(item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOXFOCUS );
			else UI_DrawPic(item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOX );

			UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
		}
	}
}

/*
=================
UI_LoadGame_Init
=================
*/
static void UI_LoadGame_Init( void )
{
	int	i, y;

	Mem_Set( &uiLoadGame, 0, sizeof( uiLoadGame_t ));

	uiLoadGame.fadeTime = uiStatic.realTime;

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
	uiLoadGame.banner.generic.x = 0;
	uiLoadGame.banner.generic.y = 66;
	uiLoadGame.banner.generic.width = 1024;
	uiLoadGame.banner.generic.height = 46;
	uiLoadGame.banner.pic = ART_BANNER;

	uiLoadGame.back.generic.id = ID_BACK;
	uiLoadGame.back.generic.type = QMTYPE_BITMAP;
	uiLoadGame.back.generic.x = 310;
	uiLoadGame.back.generic.y = 656;
	uiLoadGame.back.generic.width = 198;
	uiLoadGame.back.generic.height = 38;
	uiLoadGame.back.generic.callback = UI_LoadGame_Callback;
	uiLoadGame.back.generic.ownerdraw = UI_LoadGame_Ownerdraw;
	uiLoadGame.back.pic = UI_BACKBUTTON;

	uiLoadGame.load.generic.id = ID_LOAD;
	uiLoadGame.load.generic.type = QMTYPE_BITMAP;
	uiLoadGame.load.generic.x = 516;
	uiLoadGame.load.generic.y = 656;
	uiLoadGame.load.generic.width = 198;
	uiLoadGame.load.generic.height = 38;
	uiLoadGame.load.generic.callback = UI_LoadGame_Callback;
	uiLoadGame.load.generic.ownerdraw = UI_LoadGame_Ownerdraw;
	uiLoadGame.load.pic	= UI_LOADBUTTON;

	uiLoadGame.listBack.generic.id = ID_LISTBACK;
	uiLoadGame.listBack.generic.type = QMTYPE_BITMAP;
	uiLoadGame.listBack.generic.flags = QMF_INACTIVE;
	uiLoadGame.listBack.generic.x	= 42;
	uiLoadGame.listBack.generic.y	= 146;
	uiLoadGame.listBack.generic.width = 462;
	uiLoadGame.listBack.generic.height = 476;
	uiLoadGame.listBack.pic = ART_LISTBACK;

	uiLoadGame.gameTitle.generic.id = ID_GAMETITLE;
	uiLoadGame.gameTitle.generic.type = QMTYPE_ACTION;
	uiLoadGame.gameTitle.generic.flags = QMF_INACTIVE|QMF_SMALLFONT;
	uiLoadGame.gameTitle.generic.x = 42;
	uiLoadGame.gameTitle.generic.y = 146;
	uiLoadGame.gameTitle.generic.width = 462;
	uiLoadGame.gameTitle.generic.height = 24;
	uiLoadGame.gameTitle.generic.ownerdraw = UI_LoadGame_Ownerdraw;

	for( i = 0, y = 182; i < UI_MAXGAMES; i++, y += 32 )
	{
		uiLoadGame.listGames[i].generic.id = ID_LISTGAMES+i;
		uiLoadGame.listGames[i].generic.type = QMTYPE_ACTION;
		uiLoadGame.listGames[i].generic.flags = QMF_SILENT|QMF_SMALLFONT;
		uiLoadGame.listGames[i].generic.x = 42;
		uiLoadGame.listGames[i].generic.y = y;
		uiLoadGame.listGames[i].generic.width = 462;
		uiLoadGame.listGames[i].generic.height = 24;
		uiLoadGame.listGames[i].generic.callback = UI_LoadGame_Callback;
		uiLoadGame.listGames[i].generic.ownerdraw = UI_LoadGame_Ownerdraw;
	}

	uiLoadGame.levelShot.generic.id = ID_LEVELSHOT;
	uiLoadGame.levelShot.generic.type = QMTYPE_BITMAP;
	uiLoadGame.levelShot.generic.flags = QMF_INACTIVE;
	uiLoadGame.levelShot.generic.x = 568;
	uiLoadGame.levelShot.generic.y = 208;
	uiLoadGame.levelShot.generic.width = 414;
	uiLoadGame.levelShot.generic.height = 206;
	uiLoadGame.levelShot.generic.ownerdraw = UI_LoadGame_Ownerdraw;
	uiLoadGame.levelShot.pic = ART_LEVELSHOTBLUR;

	uiLoadGame.newGame.generic.id	= ID_NEWGAME;
	uiLoadGame.newGame.generic.type = QMTYPE_BITMAP;
	uiLoadGame.newGame.generic.x = 676;
	uiLoadGame.newGame.generic.y = 468;
	uiLoadGame.newGame.generic.width = 198;
	uiLoadGame.newGame.generic.height = 38;
	uiLoadGame.newGame.generic.callback = UI_LoadGame_Callback;
	uiLoadGame.newGame.generic.ownerdraw = UI_LoadGame_Ownerdraw;
	uiLoadGame.newGame.pic = UI_NEWGAMEBUTTON;

	uiLoadGame.saveGame.generic.id = ID_SAVEGAME;
	uiLoadGame.saveGame.generic.type = QMTYPE_BITMAP;
	uiLoadGame.saveGame.generic.x = 676;
	uiLoadGame.saveGame.generic.y	= 516;
	uiLoadGame.saveGame.generic.width = 198;
	uiLoadGame.saveGame.generic.height = 38;
	uiLoadGame.saveGame.generic.callback = UI_LoadGame_Callback;
	uiLoadGame.saveGame.generic.ownerdraw = UI_LoadGame_Ownerdraw;
	uiLoadGame.saveGame.pic = UI_SAVEBUTTON;

	uiLoadGame.deleteGame.generic.id = ID_DELETEGAME;
	uiLoadGame.deleteGame.generic.type = QMTYPE_BITMAP;
	uiLoadGame.deleteGame.generic.x = 676;
	uiLoadGame.deleteGame.generic.y = 564;
	uiLoadGame.deleteGame.generic.width = 198;
	uiLoadGame.deleteGame.generic.height = 38;
	uiLoadGame.deleteGame.generic.callback = UI_LoadGame_Callback;
	uiLoadGame.deleteGame.generic.ownerdraw = UI_LoadGame_Ownerdraw;
	uiLoadGame.deleteGame.pic = UI_DELETEBUTTON;

	UI_LoadGame_GetGameList();

	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.background );
	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.banner );
	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.back );
	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.load );
	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.listBack );
	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.gameTitle );

	for( i = 0; i < UI_MAXGAMES; i++ )
		UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.listGames[i] );

	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.levelShot );
	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.newGame );
	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.saveGame );
	UI_AddItem( &uiLoadGame.menu, (void *)&uiLoadGame.deleteGame );
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
	re->RegisterShader( ART_LISTBACK, SHADER_NOMIP );
	re->RegisterShader( ART_LEVELSHOTBLUR, SHADER_NOMIP );
}

/*
=================
UI_LoadGame_Menu
=================
*/
void UI_LoadGame_Menu( void )
{
	UI_LoadGame_Precache();
	UI_LoadGame_Init();

	UI_PushMenu( &uiLoadGame.menu );
}