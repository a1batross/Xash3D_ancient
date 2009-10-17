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
#define ART_BANNER		"gfx/shell/head_newgame"

#define ID_BACKGROUND	0
#define ID_BANNER		1

#define ID_EASY	  	2
#define ID_MEDIUM	  	3
#define ID_DIFFICULT  	4
#define ID_CANCEL		5

#define ID_MSGBOX	 	6
#define ID_MSGTEXT	 	7
#define ID_YES	 	8
#define ID_NO	 	9

typedef struct
{
	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;

	menuAction_s	easy;
	menuAction_s	medium;
	menuAction_s	hard;
	menuAction_s	cancel;

	// newgame prompt dialog
	menuAction_s	msgBox;
	menuAction_s	dlgMessage1;
	menuAction_s	dlgMessage2;
	menuAction_s	yes;
	menuAction_s	no;

	float		skill;

} uiNewGame_t;

static uiNewGame_t	uiNewGame;

/*
=================
UI_NewGame_StartGame
=================
*/
static void UI_NewGame_StartGame( float skill )
{
	Cvar_SetValue( "skill", skill );
	Cvar_SetValue( "deathmatch", 0 );
	Cvar_SetValue( "gamerules", 0 );
	Cvar_SetValue( "teamplay", 0 );
	Cvar_SetValue( "paused", 0 );
	Cvar_SetValue( "coop", 0 );

	Cbuf_ExecuteText( EXEC_APPEND, "loading; killserver; wait; newgame\n" );
}

static void UI_PromptDialog( float skill )
{
	if( cls.state != ca_active )
	{
		UI_NewGame_StartGame( skill );
		return;
	}
	
	uiNewGame.skill = skill;

	// toggle main menu between active\inactive
	// show\hide quit dialog
	uiNewGame.easy.generic.flags ^= QMF_INACTIVE; 
	uiNewGame.medium.generic.flags ^= QMF_INACTIVE;
	uiNewGame.hard.generic.flags ^= QMF_INACTIVE;
	uiNewGame.cancel.generic.flags ^= QMF_INACTIVE;

	uiNewGame.msgBox.generic.flags ^= QMF_HIDDEN;
	uiNewGame.dlgMessage1.generic.flags ^= QMF_HIDDEN;
	uiNewGame.dlgMessage2.generic.flags ^= QMF_HIDDEN;
	uiNewGame.no.generic.flags ^= QMF_HIDDEN;
	uiNewGame.yes.generic.flags ^= QMF_HIDDEN;

}

/*
=================
UI_NewGame_Callback
=================
*/
static void UI_NewGame_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_EASY:
		UI_PromptDialog( 0.0f );
		break;
	case ID_MEDIUM:
		UI_PromptDialog( 1.0f );
		break;
	case ID_DIFFICULT:
		UI_PromptDialog( 2.0f );
		break;
	case ID_CANCEL:
		UI_PopMenu();
		break;
	case ID_YES:
		UI_NewGame_StartGame( uiNewGame.skill );
		break;
	case ID_NO:
		UI_PromptDialog( 0.0f ); // clear skill
		break;
	}
}

/*
=================
UI_MsgBox_Ownerdraw
=================
*/
static void UI_MsgBox_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	UI_FillRect( item->x, item->y, item->width, item->height, uiColorDkGrey );
}

/*
=================
UI_NewGame_Init
=================
*/
static void UI_NewGame_Init( void )
{
	Mem_Set( &uiNewGame, 0, sizeof( uiNewGame_t ));

	uiNewGame.background.generic.id = ID_BACKGROUND;
	uiNewGame.background.generic.type = QMTYPE_BITMAP;
	uiNewGame.background.generic.flags = QMF_INACTIVE;
	uiNewGame.background.generic.x = 0;
	uiNewGame.background.generic.y = 0;
	uiNewGame.background.generic.width = 1024;
	uiNewGame.background.generic.height = 768;
	uiNewGame.background.pic = ART_BACKGROUND;

	uiNewGame.banner.generic.id = ID_BANNER;
	uiNewGame.banner.generic.type = QMTYPE_BITMAP;
	uiNewGame.banner.generic.flags = QMF_INACTIVE;
	uiNewGame.banner.generic.x = 65;
	uiNewGame.banner.generic.y = 92;
	uiNewGame.banner.generic.width = 690;
	uiNewGame.banner.generic.height = 120;
	uiNewGame.banner.pic = ART_BANNER;

	uiNewGame.easy.generic.id = ID_EASY;
	uiNewGame.easy.generic.type = QMTYPE_ACTION;
	uiNewGame.easy.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiNewGame.easy.generic.name = "Easy";
	uiNewGame.easy.generic.statusText = "Play the game on the 'easy' skill setting";
	uiNewGame.easy.generic.x = 72;
	uiNewGame.easy.generic.y = 230;
	uiNewGame.easy.generic.callback = UI_NewGame_Callback;

	uiNewGame.medium.generic.id = ID_MEDIUM;
	uiNewGame.medium.generic.type = QMTYPE_ACTION;
	uiNewGame.medium.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiNewGame.medium.generic.name = "Medium";
	uiNewGame.medium.generic.statusText = "Play the game on the 'medium' skill setting";
	uiNewGame.medium.generic.x = 72;
	uiNewGame.medium.generic.y = 280;
	uiNewGame.medium.generic.callback = UI_NewGame_Callback;

	uiNewGame.hard.generic.id = ID_DIFFICULT;
	uiNewGame.hard.generic.type = QMTYPE_ACTION;
	uiNewGame.hard.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiNewGame.hard.generic.name = "Difficult";
	uiNewGame.hard.generic.statusText = "Play the game on the 'difficult' skill setting";
	uiNewGame.hard.generic.x = 72;
	uiNewGame.hard.generic.y = 330;
	uiNewGame.hard.generic.callback = UI_NewGame_Callback;

	uiNewGame.cancel.generic.id = ID_CANCEL;
	uiNewGame.cancel.generic.type = QMTYPE_ACTION;
	uiNewGame.cancel.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiNewGame.cancel.generic.name = "Cancel";
	uiNewGame.cancel.generic.statusText = "Go back to the main Menu";
	uiNewGame.cancel.generic.x = 72;
	uiNewGame.cancel.generic.y = 380;
	uiNewGame.cancel.generic.callback = UI_NewGame_Callback;

	uiNewGame.msgBox.generic.id = ID_MSGBOX;
	uiNewGame.msgBox.generic.type = QMTYPE_ACTION;
	uiNewGame.msgBox.generic.flags = QMF_INACTIVE|QMF_HIDDEN;
	uiNewGame.msgBox.generic.ownerdraw = UI_MsgBox_Ownerdraw; // just a fill rectangle
	uiNewGame.msgBox.generic.x = 192;
	uiNewGame.msgBox.generic.y = 256;
	uiNewGame.msgBox.generic.width = 640;
	uiNewGame.msgBox.generic.height = 256;

	uiNewGame.dlgMessage1.generic.id = ID_MSGTEXT;
	uiNewGame.dlgMessage1.generic.type = QMTYPE_ACTION;
	uiNewGame.dlgMessage1.generic.flags = QMF_INACTIVE|QMF_HIDDEN;
	uiNewGame.dlgMessage1.generic.name = "Starting a new game will exit";
	uiNewGame.dlgMessage1.generic.x = 248;
	uiNewGame.dlgMessage1.generic.y = 280;

	uiNewGame.dlgMessage2.generic.id = ID_MSGTEXT;
	uiNewGame.dlgMessage2.generic.type = QMTYPE_ACTION;
	uiNewGame.dlgMessage2.generic.flags = QMF_INACTIVE|QMF_HIDDEN;
	uiNewGame.dlgMessage2.generic.name = "any current game, OK to exit?";
	uiNewGame.dlgMessage2.generic.x = 248;
	uiNewGame.dlgMessage2.generic.y = 310;

	uiNewGame.yes.generic.id = ID_YES;
	uiNewGame.yes.generic.type = QMTYPE_ACTION;
	uiNewGame.yes.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_HIDDEN;
	uiNewGame.yes.generic.name = "Ok";
	uiNewGame.yes.generic.x = 380;
	uiNewGame.yes.generic.y = 460;
	uiNewGame.yes.generic.callback = UI_NewGame_Callback;

	uiNewGame.no.generic.id = ID_NO;
	uiNewGame.no.generic.type = QMTYPE_ACTION;
	uiNewGame.no.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_HIDDEN;
	uiNewGame.no.generic.name = "Cancel";
	uiNewGame.no.generic.x = 530;
	uiNewGame.no.generic.y = 460;
	uiNewGame.no.generic.callback = UI_NewGame_Callback;

	UI_AddItem( &uiNewGame.menu, (void *)&uiNewGame.background );
	UI_AddItem( &uiNewGame.menu, (void *)&uiNewGame.banner );
	UI_AddItem( &uiNewGame.menu, (void *)&uiNewGame.easy );
	UI_AddItem( &uiNewGame.menu, (void *)&uiNewGame.medium );
	UI_AddItem( &uiNewGame.menu, (void *)&uiNewGame.hard );
	UI_AddItem( &uiNewGame.menu, (void *)&uiNewGame.cancel );
	UI_AddItem( &uiNewGame.menu, (void *)&uiNewGame.msgBox );
	UI_AddItem( &uiNewGame.menu, (void *)&uiNewGame.dlgMessage1 );
	UI_AddItem( &uiNewGame.menu, (void *)&uiNewGame.dlgMessage2 );
	UI_AddItem( &uiNewGame.menu, (void *)&uiNewGame.no );
	UI_AddItem( &uiNewGame.menu, (void *)&uiNewGame.yes );
}

/*
=================
UI_NewGame_Precache
=================
*/
void UI_NewGame_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
}

/*
=================
UI_NewGame_Menu
=================
*/
void UI_NewGame_Menu( void )
{
	UI_NewGame_Precache();
	UI_NewGame_Init();

	UI_PushMenu( &uiNewGame.menu );
}