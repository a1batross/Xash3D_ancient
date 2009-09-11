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


#define ART_BACKGROUND		"gfx/shell/misc/ui_sub_options"
#define ART_MSGBOX	  		"gfx/shell/misc/defaults_warn"

#define ID_BACKGROUND		0

#define ID_MSGBOX			1
#define ID_YES			2
#define ID_NO			3

typedef struct
{
	menuFramework_s	menu;
	menuBitmap_s	background;

	menuBitmap_s	msgBox;
	menuBitmap_s	no;
	menuBitmap_s	yes;
} uiDefaults_t;

static uiDefaults_t		uiDefaults;


/*
=================
UI_Defaults_Callback
=================
*/ 
static void UI_Defaults_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_YES:
		// set default bindings and restart all cvars
		Cbuf_ExecuteText( EXEC_APPEND, "unsetall\n" );
		Cbuf_ExecuteText( EXEC_APPEND, "unbindall\n" );
		Cbuf_ExecuteText( EXEC_APPEND, "exec basekeys.rc\n" );
		Cbuf_ExecuteText( EXEC_APPEND, "exec basevars.rc\n" );

		// restart all the subsystems for the changes to take effect
		Cbuf_ExecuteText( EXEC_APPEND, "net_restart\n" );
		Cbuf_ExecuteText( EXEC_APPEND, "snd_restart\n" );
		Cbuf_ExecuteText( EXEC_APPEND, "vid_restart\n" );
		break;
	case ID_NO:
		UI_PopMenu();
		break;
	}
}

/*
=================
UI_Defaults_Ownerdraw
=================
*/
static void UI_Defaults_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( uiDefaults.menu.items[uiDefaults.menu.cursor] == self )
		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOXFOCUS );
	else UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOX );
	
	UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
}

/*
=================
UI_Defaults_Init
=================
*/
static void UI_Defaults_Init( void )
{
	Mem_Set( &uiDefaults, 0, sizeof( uiDefaults_t ));

	uiDefaults.background.generic.id = ID_BACKGROUND;
	uiDefaults.background.generic.type = QMTYPE_BITMAP;
	uiDefaults.background.generic.flags = QMF_INACTIVE | QMF_GRAYED;
	uiDefaults.background.generic.x = 0;
	uiDefaults.background.generic.y = 0;
	uiDefaults.background.generic.width = 1024;
	uiDefaults.background.generic.height = 768;
	uiDefaults.background.pic = ART_BACKGROUND;

	uiDefaults.msgBox.generic.id = ID_MSGBOX;
	uiDefaults.msgBox.generic.type = QMTYPE_BITMAP;
	uiDefaults.msgBox.generic.flags = QMF_INACTIVE;
	uiDefaults.msgBox.generic.x = 174;
	uiDefaults.msgBox.generic.y = 256;
	uiDefaults.msgBox.generic.width = 676;
	uiDefaults.msgBox.generic.height = 256;
	uiDefaults.msgBox.pic = ART_MSGBOX;

	uiDefaults.no.generic.id = ID_NO;
	uiDefaults.no.generic.type = QMTYPE_BITMAP;
	uiDefaults.no.generic.flags = 0;
	uiDefaults.no.generic.x = 310;
	uiDefaults.no.generic.y = 434;
	uiDefaults.no.generic.width = 198;
	uiDefaults.no.generic.height = 38;
	uiDefaults.no.generic.callback = UI_Defaults_Callback;
	uiDefaults.no.generic.ownerdraw = UI_Defaults_Ownerdraw;
	uiDefaults.no.pic = UI_CANCELBUTTON;

	uiDefaults.yes.generic.id = ID_YES;
	uiDefaults.yes.generic.type = QMTYPE_BITMAP;
	uiDefaults.yes.generic.flags = 0;
	uiDefaults.yes.generic.x = 516;
	uiDefaults.yes.generic.y = 434;
	uiDefaults.yes.generic.width = 198;
	uiDefaults.yes.generic.height = 38;
	uiDefaults.yes.generic.callback = UI_Defaults_Callback;
	uiDefaults.yes.generic.ownerdraw = UI_Defaults_Ownerdraw;
	uiDefaults.yes.pic = UI_ACCEPTBUTTON;

	UI_AddItem( &uiDefaults.menu, (void *)&uiDefaults.background );
	UI_AddItem( &uiDefaults.menu, (void *)&uiDefaults.msgBox );
	UI_AddItem( &uiDefaults.menu, (void *)&uiDefaults.no );
	UI_AddItem( &uiDefaults.menu, (void *)&uiDefaults.yes );
}

/*
=================
UI_Defaults_Precache
=================
*/
void UI_Defaults_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_MSGBOX, SHADER_NOMIP );
}

/*
=================
UI_Defaults_Menu
=================
*/
void UI_Defaults_Menu( void )
{
	UI_Defaults_Precache();
	UI_Defaults_Init();

	UI_PushMenu( &uiDefaults.menu );
}