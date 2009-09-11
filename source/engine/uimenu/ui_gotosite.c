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


#define ART_BACKGROUND	"gfx/shell/title_screen/title_backg"
#define ART_MSGBOX		"gfx/shell/title_screen/site_load"

#define ID_BACKGROUND	0

#define ID_MSGBOX		1
#define ID_YES		2
#define ID_NO		3

typedef struct
{
	string		webAddress;

	menuFramework_s	menu;
	menuBitmap_s	background;
	menuBitmap_s	msgBox;
	menuBitmap_s	no;
	menuBitmap_s	yes;
} uiGoToSite_t;

static uiGoToSite_t		uiGoToSite;


/*
=================
UI_GoToSite_Callback
=================
*/ 
static void UI_GoToSite_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_YES:
		Sys_ShellExecute( uiGoToSite.webAddress, NULL, true );
		break;
	case ID_NO:
		UI_PopMenu();
		break;
	}
}

/*
=================
UI_GoToSite_Ownerdraw
=================
*/
static void UI_GoToSite_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( uiGoToSite.menu.items[uiGoToSite.menu.cursor] == self )
		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOXFOCUS );
	else UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOX );
	
	UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
}

/*
=================
UI_GoToSite_Init
=================
*/
static void UI_GoToSite_Init( void )
{
	Mem_Set( &uiGoToSite, 0, sizeof( uiGoToSite_t ));

	uiGoToSite.background.generic.id = ID_BACKGROUND;
	uiGoToSite.background.generic.type = QMTYPE_BITMAP;
	uiGoToSite.background.generic.flags = QMF_INACTIVE | QMF_GRAYED;
	uiGoToSite.background.generic.x = 0;
	uiGoToSite.background.generic.y = 0;
	uiGoToSite.background.generic.width = 1024;
	uiGoToSite.background.generic.height = 768;
	uiGoToSite.background.pic = ART_BACKGROUND;

	uiGoToSite.msgBox.generic.id = ID_MSGBOX;
	uiGoToSite.msgBox.generic.type = QMTYPE_BITMAP;
	uiGoToSite.msgBox.generic.flags = QMF_INACTIVE;
	uiGoToSite.msgBox.generic.x = 174;
	uiGoToSite.msgBox.generic.y = 256;
	uiGoToSite.msgBox.generic.width = 676;
	uiGoToSite.msgBox.generic.height = 256;
	uiGoToSite.msgBox.pic = ART_MSGBOX;

	uiGoToSite.no.generic.id = ID_NO;
	uiGoToSite.no.generic.type = QMTYPE_BITMAP;
	uiGoToSite.no.generic.flags = 0;
	uiGoToSite.no.generic.x = 310;
	uiGoToSite.no.generic.y = 434;
	uiGoToSite.no.generic.width = 198;
	uiGoToSite.no.generic.height = 38;
	uiGoToSite.no.generic.callback = UI_GoToSite_Callback;
	uiGoToSite.no.generic.ownerdraw = UI_GoToSite_Ownerdraw;
	uiGoToSite.no.pic = UI_CANCELBUTTON;

	uiGoToSite.yes.generic.id = ID_YES;
	uiGoToSite.yes.generic.type = QMTYPE_BITMAP;
	uiGoToSite.yes.generic.flags = 0;
	uiGoToSite.yes.generic.x = 516;
	uiGoToSite.yes.generic.y = 434;
	uiGoToSite.yes.generic.width = 198;
	uiGoToSite.yes.generic.height = 38;
	uiGoToSite.yes.generic.callback = UI_GoToSite_Callback;
	uiGoToSite.yes.generic.ownerdraw = UI_GoToSite_Ownerdraw;
	uiGoToSite.yes.pic = UI_ACCEPTBUTTON;

	UI_AddItem( &uiGoToSite.menu, (void *)&uiGoToSite.background );
	UI_AddItem( &uiGoToSite.menu, (void *)&uiGoToSite.msgBox );
	UI_AddItem( &uiGoToSite.menu, (void *)&uiGoToSite.no );
	UI_AddItem( &uiGoToSite.menu, (void *)&uiGoToSite.yes );
}

/*
=================
UI_GoToSite_Precache
=================
*/
void UI_GoToSite_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_MSGBOX, SHADER_NOMIP );
}

/*
=================
UI_GoToSite_Menu
=================
*/
void UI_GoToSite_Menu( const char *webAddress )
{
	UI_GoToSite_Precache();
	UI_GoToSite_Init();

	com.strncpy( uiGoToSite.webAddress, webAddress, sizeof( uiGoToSite.webAddress ));
	UI_PushMenu( &uiGoToSite.menu );
}