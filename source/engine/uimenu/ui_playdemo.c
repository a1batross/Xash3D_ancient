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

#define ART_BANNER	     	"gfx/shell/head_play"

#define ID_BACKGROUND	0
#define ID_BANNER		1
#define ID_PLAY		2
#define ID_DELETE		3
#define ID_CANCEL		4
#define ID_DEMOLIST		5
#define ID_TABLEHINT	6
#define ID_LEVELSHOT	7
#define ID_MSGBOX	 	8
#define ID_MSGTEXT	 	9
#define ID_YES	 	10
#define ID_NO	 	11

#define LEVELSHOT_X		72
#define LEVELSHOT_Y		400
#define LEVELSHOT_W		192
#define LEVELSHOT_H		160

#define TITLE_LENGTH	32
#define MAPNAME_LENGTH	24+TITLE_LENGTH
#define MAXCLIENTS_LENGTH	16+MAPNAME_LENGTH

typedef struct
{
	char		demoName[UI_MAXGAMES][CS_SIZE];
	char		delName[UI_MAXGAMES][CS_SIZE];
	string		demoDescription[UI_MAXGAMES];
	char		*demoDescriptionPtr[UI_MAXGAMES];

	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;
	menuAction_s	play;
	menuAction_s	delete;
	menuAction_s	cancel;

	menuScrollList_s	demosList;

	menuBitmap_s	levelShot;
	menuAction_s	hintMessage;
	char		hintText[MAX_SYSPATH];

	// prompt dialog
	menuAction_s	msgBox;
	menuAction_s	promptMessage;
	menuAction_s	yes;
	menuAction_s	no;
} uiPlayDemo_t;

static uiPlayDemo_t		uiPlayDemo;

/*
=================
UI_MsgBox_Ownerdraw
=================
*/
static void UI_MsgBox_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	UI_FillRect( item->x, item->y, item->width, item->height, uiPromptBgColor );
}

static void UI_DeleteDialog( void )
{
	// toggle main menu between active\inactive
	// show\hide delete dialog
	uiPlayDemo.play.generic.flags ^= QMF_INACTIVE; 
	uiPlayDemo.delete.generic.flags ^= QMF_INACTIVE;
	uiPlayDemo.cancel.generic.flags ^= QMF_INACTIVE;
	uiPlayDemo.demosList.generic.flags ^= QMF_INACTIVE;

	uiPlayDemo.msgBox.generic.flags ^= QMF_HIDDEN;
	uiPlayDemo.promptMessage.generic.flags ^= QMF_HIDDEN;
	uiPlayDemo.no.generic.flags ^= QMF_HIDDEN;
	uiPlayDemo.yes.generic.flags ^= QMF_HIDDEN;

}

/*
=================
UI_PlayDemo_KeyFunc
=================
*/
static const char *UI_PlayDemo_KeyFunc( int key, bool down )
{
	if( down && key == K_ESCAPE && uiPlayDemo.play.generic.flags & QMF_INACTIVE )
	{
		UI_DeleteDialog();
		return uiSoundNull;
	}
	return UI_DefaultKey( &uiPlayDemo.menu, key, down );
}

/*
=================
UI_PlayDemo_GetDemoList
=================
*/
static void UI_PlayDemo_GetDemoList( void )
{
	string	comment;
	search_t	*t;
	int	i;

	t = FS_Search( "$demos/*.dem", true );

	for( i = 0; t && i < t->numfilenames; i++ )
	{
		if( i >= UI_MAXGAMES ) break;
		
		if( !CL_GetComment( t->filenames[i], comment ))
		{
			if( com.strlen( comment ))
			{
				// get name string even if not found - CL_GetComment can be mark demos
				// as <CORRUPTED> <OLD VERSION> etc
				com.strncat( uiPlayDemo.demoDescription[i], uiEmptyString, TITLE_LENGTH );
				com.strncat( uiPlayDemo.demoDescription[i], comment, MAPNAME_LENGTH );
				com.strncat( uiPlayDemo.demoDescription[i], uiEmptyString, MAXCLIENTS_LENGTH );
				uiPlayDemo.demoDescriptionPtr[i] = uiPlayDemo.demoDescription[i];
				FS_FileBase( t->filenames[i], uiPlayDemo.delName[i] );
			}
			else uiPlayDemo.demoDescriptionPtr[i] = NULL;
			continue;
		}

		// strip path, leave only filename (empty slots doesn't have savename)
		FS_FileBase( t->filenames[i], uiPlayDemo.demoName[i] );
		FS_FileBase( t->filenames[i], uiPlayDemo.delName[i] );

		// fill demo desc
		com.strncat( uiPlayDemo.demoDescription[i], comment + CS_SIZE, TITLE_LENGTH );
		com.strncat( uiPlayDemo.demoDescription[i], uiEmptyString, TITLE_LENGTH );
		com.strncat( uiPlayDemo.demoDescription[i], comment, MAPNAME_LENGTH );
		com.strncat( uiPlayDemo.demoDescription[i], uiEmptyString, MAPNAME_LENGTH ); // fill remaining entries
		com.strncat( uiPlayDemo.demoDescription[i], comment + CS_SIZE * 2, MAXCLIENTS_LENGTH );
		com.strncat( uiPlayDemo.demoDescription[i], uiEmptyString, MAXCLIENTS_LENGTH );
		uiPlayDemo.demoDescriptionPtr[i] = uiPlayDemo.demoDescription[i];
	}

	for( ; i < UI_MAXGAMES; i++ ) uiPlayDemo.demoDescriptionPtr[i] = NULL;
	uiPlayDemo.demosList.itemNames = uiPlayDemo.demoDescriptionPtr;

	if( com.strlen( uiPlayDemo.demoName[0] ) == 0 )
		uiPlayDemo.play.generic.flags |= QMF_GRAYED;
	else uiPlayDemo.play.generic.flags &= ~QMF_GRAYED;

	if( com.strlen( uiPlayDemo.delName[0] ) == 0 || !com.stricmp( cls.demoname, uiPlayDemo.delName[uiPlayDemo.demosList.curItem] ))
		uiPlayDemo.delete.generic.flags |= QMF_GRAYED;
	else uiPlayDemo.delete.generic.flags &= ~QMF_GRAYED;
	
	if( t ) Mem_Free( t );
}

/*
=================
UI_PlayDemo_Callback
=================
*/
static void UI_PlayDemo_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;
	string		pathPIC;

	if( event == QM_CHANGED )
	{
		if( com.strlen( uiPlayDemo.demoName[uiPlayDemo.demosList.curItem] ) == 0 )
			uiPlayDemo.play.generic.flags |= QMF_GRAYED;
		else uiPlayDemo.play.generic.flags &= ~QMF_GRAYED;

		if( com.strlen( uiPlayDemo.delName[uiPlayDemo.demosList.curItem] ) == 0 || !com.stricmp( cls.demoname, uiPlayDemo.delName[uiPlayDemo.demosList.curItem] ))
			uiPlayDemo.delete.generic.flags |= QMF_GRAYED;
		else uiPlayDemo.delete.generic.flags &= ~QMF_GRAYED;
		return;
	}

	if( event != QM_ACTIVATED )
		return;
	
	switch( item->id )
	{
	case ID_CANCEL:
		UI_PopMenu();
		break;
	case ID_PLAY:
		if( cls.demoplayback || cls.demorecording )
		{
			Cbuf_ExecuteText( EXEC_APPEND, "stop" );
			uiPlayDemo.play.generic.name = "Play";
			uiPlayDemo.play.generic.statusText = "Play a demo";
			uiPlayDemo.delete.generic.flags &= ~QMF_GRAYED;
		}
		else if( com.strlen( uiPlayDemo.demoName[uiPlayDemo.demosList.curItem] ))
			Cbuf_ExecuteText( EXEC_APPEND, va( "playdemo \"%s\"\n", uiPlayDemo.demoName[uiPlayDemo.demosList.curItem] ));
		break;
	case ID_NO:
	case ID_DELETE:
		UI_DeleteDialog();
		break;
	case ID_YES:
		if( com.strlen( uiPlayDemo.delName[uiPlayDemo.demosList.curItem] ))
		{
			com.snprintf( pathPIC, sizeof( pathPIC ), "demos/%s.%s", uiPlayDemo.delName[uiPlayDemo.demosList.curItem], SI->savshot_ext );
			Cbuf_ExecuteText( EXEC_NOW, va( "killdemo \"%s\"\n", uiPlayDemo.delName[uiPlayDemo.demosList.curItem] ));
			if( re ) re->FreeShader( pathPIC ); // unload shader from video-memory
			UI_PlayDemo_GetDemoList();
		}
		UI_DeleteDialog();
		break;
	}
}

/*
=================
UI_PlayDemo_Ownerdraw
=================
*/
static void UI_PlayDemo_Ownerdraw( void *self )
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

		if( com.strlen( uiPlayDemo.demoName[uiPlayDemo.demosList.curItem] ))
		{
			string	pathPIC;

			com.snprintf( pathPIC, sizeof( pathPIC ), "demos/%s.%s", uiPlayDemo.demoName[uiPlayDemo.demosList.curItem], SI->savshot_ext );

			if( !FS_FileExists( pathPIC ))
				UI_DrawPic( x, y, w, h, uiColorWhite, "gfx/hud/static" );
			else UI_DrawPic( x, y, w, h, uiColorWhite, pathPIC );
		}
		else UI_DrawPic( x, y, w, h, uiColorWhite, "gfx/hud/static" );

		// draw the rectangle
		UI_DrawRectangle( item->x, item->y, item->width, item->height, uiInputFgColor );
	}
}

/*
=================
UI_PlayDemo_Init
=================
*/
static void UI_PlayDemo_Init( void )
{
	Mem_Set( &uiPlayDemo, 0, sizeof( uiPlayDemo_t ));

	uiPlayDemo.menu.keyFunc = UI_PlayDemo_KeyFunc;

	com.strncat( uiPlayDemo.hintText, "Title", TITLE_LENGTH );
	com.strncat( uiPlayDemo.hintText, uiEmptyString, TITLE_LENGTH );
	com.strncat( uiPlayDemo.hintText, "Map", MAPNAME_LENGTH );
	com.strncat( uiPlayDemo.hintText, uiEmptyString, MAPNAME_LENGTH );
	com.strncat( uiPlayDemo.hintText, "Max Clients", MAXCLIENTS_LENGTH );
	com.strncat( uiPlayDemo.hintText, uiEmptyString, MAXCLIENTS_LENGTH );

	uiPlayDemo.background.generic.id = ID_BACKGROUND;
	uiPlayDemo.background.generic.type = QMTYPE_BITMAP;
	uiPlayDemo.background.generic.flags = QMF_INACTIVE;
	uiPlayDemo.background.generic.x = 0;
	uiPlayDemo.background.generic.y = 0;
	uiPlayDemo.background.generic.width = 1024;
	uiPlayDemo.background.generic.height = 768;
	uiPlayDemo.background.pic = ART_BACKGROUND;

	uiPlayDemo.banner.generic.id = ID_BANNER;
	uiPlayDemo.banner.generic.type = QMTYPE_BITMAP;
	uiPlayDemo.banner.generic.flags = QMF_INACTIVE;
	uiPlayDemo.banner.generic.x = UI_BANNER_POSX;
	uiPlayDemo.banner.generic.y = UI_BANNER_POSY;
	uiPlayDemo.banner.generic.width = UI_BANNER_WIDTH;
	uiPlayDemo.banner.generic.height = UI_BANNER_HEIGHT;
	uiPlayDemo.banner.pic = ART_BANNER;

	uiPlayDemo.play.generic.id = ID_PLAY;
	uiPlayDemo.play.generic.type = QMTYPE_ACTION;
	uiPlayDemo.play.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiPlayDemo.play.generic.x = 72;
	uiPlayDemo.play.generic.y = 230;

	if( cls.demoplayback )
	{
		uiPlayDemo.play.generic.name = "Stop";
		uiPlayDemo.play.generic.statusText = "Stop a demo playing";
	}
	else if( cls.demorecording )
	{
		uiPlayDemo.play.generic.name = "Stop";
		uiPlayDemo.play.generic.statusText = "Stop a demo recording";
	}
	else
	{
		uiPlayDemo.play.generic.name = "Play";
		uiPlayDemo.play.generic.statusText = "Play a demo";
	}
	uiPlayDemo.play.generic.callback = UI_PlayDemo_Callback;

	uiPlayDemo.delete.generic.id = ID_DELETE;
	uiPlayDemo.delete.generic.type = QMTYPE_ACTION;
	uiPlayDemo.delete.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiPlayDemo.delete.generic.x = 72;
	uiPlayDemo.delete.generic.y = 280;
	uiPlayDemo.delete.generic.name = "Delete";
	uiPlayDemo.delete.generic.statusText = "Delete a demo";
	uiPlayDemo.delete.generic.callback = UI_PlayDemo_Callback;

	uiPlayDemo.cancel.generic.id = ID_CANCEL;
	uiPlayDemo.cancel.generic.type = QMTYPE_ACTION;
	uiPlayDemo.cancel.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiPlayDemo.cancel.generic.x = 72;
	uiPlayDemo.cancel.generic.y = 330;
	uiPlayDemo.cancel.generic.name = "Cancel";
	uiPlayDemo.cancel.generic.statusText = "Return back to main menu";
	uiPlayDemo.cancel.generic.callback = UI_PlayDemo_Callback;

	uiPlayDemo.hintMessage.generic.id = ID_TABLEHINT;
	uiPlayDemo.hintMessage.generic.type = QMTYPE_ACTION;
	uiPlayDemo.hintMessage.generic.flags = QMF_INACTIVE|QMF_SMALLFONT;
	uiPlayDemo.hintMessage.generic.color = uiColorHelp;
	uiPlayDemo.hintMessage.generic.name = uiPlayDemo.hintText;
	uiPlayDemo.hintMessage.generic.x = 360;
	uiPlayDemo.hintMessage.generic.y = 225;

	uiPlayDemo.levelShot.generic.id = ID_LEVELSHOT;
	uiPlayDemo.levelShot.generic.type = QMTYPE_BITMAP;
	uiPlayDemo.levelShot.generic.flags = QMF_INACTIVE;
	uiPlayDemo.levelShot.generic.x = LEVELSHOT_X;
	uiPlayDemo.levelShot.generic.y = LEVELSHOT_Y;
	uiPlayDemo.levelShot.generic.width = LEVELSHOT_W;
	uiPlayDemo.levelShot.generic.height = LEVELSHOT_H;
	uiPlayDemo.levelShot.generic.ownerdraw = UI_PlayDemo_Ownerdraw;

	uiPlayDemo.demosList.generic.id = ID_DEMOLIST;
	uiPlayDemo.demosList.generic.type = QMTYPE_SCROLLLIST;
	uiPlayDemo.demosList.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_SMALLFONT;
	uiPlayDemo.demosList.generic.x = 360;
	uiPlayDemo.demosList.generic.y = 255;
	uiPlayDemo.demosList.generic.width = 640;
	uiPlayDemo.demosList.generic.height = 440;
	uiPlayDemo.demosList.generic.callback = UI_PlayDemo_Callback;

	uiPlayDemo.msgBox.generic.id = ID_MSGBOX;
	uiPlayDemo.msgBox.generic.type = QMTYPE_ACTION;
	uiPlayDemo.msgBox.generic.flags = QMF_INACTIVE|QMF_HIDDEN;
	uiPlayDemo.msgBox.generic.ownerdraw = UI_MsgBox_Ownerdraw; // just a fill rectangle
	uiPlayDemo.msgBox.generic.x = 192;
	uiPlayDemo.msgBox.generic.y = 256;
	uiPlayDemo.msgBox.generic.width = 640;
	uiPlayDemo.msgBox.generic.height = 256;

	uiPlayDemo.promptMessage.generic.id = ID_MSGBOX;
	uiPlayDemo.promptMessage.generic.type = QMTYPE_ACTION;
	uiPlayDemo.promptMessage.generic.flags = QMF_INACTIVE|QMF_DROPSHADOW|QMF_HIDDEN;
	uiPlayDemo.promptMessage.generic.name = "Delete selected demo?";
	uiPlayDemo.promptMessage.generic.x = 315;
	uiPlayDemo.promptMessage.generic.y = 280;

	uiPlayDemo.yes.generic.id = ID_YES;
	uiPlayDemo.yes.generic.type = QMTYPE_ACTION;
	uiPlayDemo.yes.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_HIDDEN;
	uiPlayDemo.yes.generic.name = "Ok";
	uiPlayDemo.yes.generic.x = 380;
	uiPlayDemo.yes.generic.y = 460;
	uiPlayDemo.yes.generic.callback = UI_PlayDemo_Callback;

	uiPlayDemo.no.generic.id = ID_NO;
	uiPlayDemo.no.generic.type = QMTYPE_ACTION;
	uiPlayDemo.no.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_HIDDEN;
	uiPlayDemo.no.generic.name = "Cancel";
	uiPlayDemo.no.generic.x = 530;
	uiPlayDemo.no.generic.y = 460;
	uiPlayDemo.no.generic.callback = UI_PlayDemo_Callback;

	UI_PlayDemo_GetDemoList();

	UI_AddItem( &uiPlayDemo.menu, (void *)&uiPlayDemo.background );
	UI_AddItem( &uiPlayDemo.menu, (void *)&uiPlayDemo.banner );
	UI_AddItem( &uiPlayDemo.menu, (void *)&uiPlayDemo.play );
	UI_AddItem( &uiPlayDemo.menu, (void *)&uiPlayDemo.delete );
	UI_AddItem( &uiPlayDemo.menu, (void *)&uiPlayDemo.cancel );
	UI_AddItem( &uiPlayDemo.menu, (void *)&uiPlayDemo.hintMessage );
	UI_AddItem( &uiPlayDemo.menu, (void *)&uiPlayDemo.levelShot );
	UI_AddItem( &uiPlayDemo.menu, (void *)&uiPlayDemo.demosList );
	UI_AddItem( &uiPlayDemo.menu, (void *)&uiPlayDemo.msgBox );
	UI_AddItem( &uiPlayDemo.menu, (void *)&uiPlayDemo.promptMessage );
	UI_AddItem( &uiPlayDemo.menu, (void *)&uiPlayDemo.no );
	UI_AddItem( &uiPlayDemo.menu, (void *)&uiPlayDemo.yes );
}

/*
=================
UI_PlayDemo_Precache
=================
*/
void UI_PlayDemo_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
}

/*
=================
UI_PlayDemo_Menu
=================
*/
void UI_PlayDemo_Menu( void )
{
	UI_PlayDemo_Precache();
	UI_PlayDemo_Init();

	UI_PushMenu( &uiPlayDemo.menu );
}