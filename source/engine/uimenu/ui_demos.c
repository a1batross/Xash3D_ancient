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

#define ID_BACKGROUND		0
#define ID_CANCEL			1
#define ID_PLAY			2
#define ID_DELETE			3
#define ID_DEMOLIST			4
#define ID_TABLEHINT		5

#define MAX_DEMOS			100

typedef struct
{
	char		demos[MAX_DEMOS][CS_SIZE];
	char		*demosPtr[MAX_DEMOS];

	menuFramework_s	menu;

	menuBitmap_s	background;
	menuAction_s	play;
	menuAction_s	delete;
	menuAction_s	cancel;

	menuScrollList_s	demoList;

	menuAction_s	hintMessage;
	char		hintText[MAX_SYSPATH];
} uiDemos_t;

static uiDemos_t		uiDemos;


/*
=================
UI_Demos_GetDemosList
=================
*/
static void UI_Demos_GetDemoList( void )
{
	search_t	*search;
	int	i;

	search = FS_Search( "demos/*.dem", false );

	for( i = 0; i < MAX_DEMOS; i++ )
	{
		if( search && i < search->numfilenames )
		{
			FS_FileBase( search->filenames[i], uiDemos.demos[i] );
			uiDemos.demosPtr[i] = uiDemos.demos[i];
		}
		else uiDemos.demosPtr[i] = NULL;
	}

	uiDemos.demoList.itemNames = uiDemos.demosPtr;
	if( search ) Mem_Free( search );
}

/*
=================
UI_Demos_Callback
=================
*/
static void UI_Demos_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_CANCEL:
		UI_PopMenu();
		break;
	case ID_PLAY:
		Cbuf_ExecuteText( EXEC_APPEND, va( "playdemo %s\n", uiDemos.demos[uiDemos.demoList.curItem] ));
		break;
	case ID_DELETE:
		FS_Delete( va( "%s/demos/%s.dem", GI->gamedir, uiDemos.demos[uiDemos.demoList.curItem] ));
		UI_Demos_GetDemoList();
		break;
	}
}

/*
=================
UI_Demos_Init
=================
*/
static void UI_Demos_Init( void )
{
	Mem_Set( &uiDemos, 0, sizeof( uiDemos_t ));

	com.strncat( uiDemos.hintText, "Demo Name", MAX_STRING );
	com.strncat( uiDemos.hintText, uiEmptyString, MAX_STRING );

	uiDemos.background.generic.id	= ID_BACKGROUND;
	uiDemos.background.generic.type = QMTYPE_BITMAP;
	uiDemos.background.generic.flags = QMF_INACTIVE;
	uiDemos.background.generic.x = 0;
	uiDemos.background.generic.y = 0;
	uiDemos.background.generic.width = 1024;
	uiDemos.background.generic.height = 768;
	uiDemos.background.pic = ART_BACKGROUND;

	uiDemos.play.generic.id = ID_PLAY;
	uiDemos.play.generic.type = QMTYPE_ACTION;
	uiDemos.play.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiDemos.play.generic.x = 72;
	uiDemos.play.generic.y = 230;
	uiDemos.play.generic.name = "Play";
	uiDemos.play.generic.statusText = "Playback selected demo";
	uiDemos.play.generic.callback = UI_Demos_Callback;

	uiDemos.delete.generic.id = ID_DELETE;
	uiDemos.delete.generic.type = QMTYPE_ACTION;
	uiDemos.delete.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiDemos.delete.generic.x = 72;
	uiDemos.delete.generic.y = 280;
	uiDemos.delete.generic.name = "Delete";
	uiDemos.delete.generic.statusText = "Delete selected demo";
	uiDemos.delete.generic.callback = UI_Demos_Callback;

	uiDemos.cancel.generic.id = ID_CANCEL;
	uiDemos.cancel.generic.type = QMTYPE_ACTION;
	uiDemos.cancel.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiDemos.cancel.generic.x = 72;
	uiDemos.cancel.generic.y = 330;
	uiDemos.cancel.generic.name = "Cancel";
	uiDemos.cancel.generic.statusText = "Return back to main menu";
	uiDemos.cancel.generic.callback = UI_Demos_Callback;

	uiDemos.hintMessage.generic.id = ID_TABLEHINT;
	uiDemos.hintMessage.generic.type = QMTYPE_ACTION;
	uiDemos.hintMessage.generic.flags = QMF_INACTIVE|QMF_SMALLFONT;
	uiDemos.hintMessage.generic.color = uiColorLtGrey;
	uiDemos.hintMessage.generic.name = uiDemos.hintText;
	uiDemos.hintMessage.generic.x = 360;
	uiDemos.hintMessage.generic.y = 225;

	uiDemos.demoList.generic.id = ID_DEMOLIST;
	uiDemos.demoList.generic.type = QMTYPE_SCROLLLIST;
	uiDemos.demoList.generic.flags = QMF_PULSEIFFOCUS|QMF_DROPSHADOW|QMF_SMALLFONT;
	uiDemos.demoList.generic.x = 360;
	uiDemos.demoList.generic.y = 255;
	uiDemos.demoList.generic.width = 640;
	uiDemos.demoList.generic.height = 440;

	UI_Demos_GetDemoList();

	UI_AddItem( &uiDemos.menu, (void *)&uiDemos.background );
	UI_AddItem( &uiDemos.menu, (void *)&uiDemos.play );
	UI_AddItem( &uiDemos.menu, (void *)&uiDemos.delete );
	UI_AddItem( &uiDemos.menu, (void *)&uiDemos.cancel );
	UI_AddItem( &uiDemos.menu, (void *)&uiDemos.hintMessage );
	UI_AddItem( &uiDemos.menu, (void *)&uiDemos.demoList );
}

/*
=================
UI_Demos_Precache
=================
*/
void UI_Demos_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
}

/*
=================
UI_Demos_Menu
=================
*/
void UI_Demos_Menu( void )
{
	UI_Demos_Precache();
	UI_Demos_Init();

	UI_PushMenu( &uiDemos.menu );
}