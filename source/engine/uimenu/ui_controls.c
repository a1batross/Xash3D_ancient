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

#define ART_BANNER		"gfx/shell/head_controls"

#define ID_BACKGROUND	0
#define ID_BANNER		1
#define ID_DEFAULTS		2
#define ID_ADVANCED		3
#define ID_DONE		4
#define ID_CANCEL		5
#define ID_KEYLIST		5
#define ID_TABLEHINT	6

#define MAX_KEYS		256
#define CMD_LENGTH		24
#define KEY1_LENGTH		20+CMD_LENGTH
#define KEY2_LENGTH		20+KEY1_LENGTH

typedef struct
{
	char		keysBind[MAX_KEYS][32];
	char		firstKey[MAX_KEYS][32];
	char		secondKey[MAX_KEYS][32];
	string		keysDescription[MAX_KEYS];
	char		*keysDescriptionPtr[MAX_KEYS];

	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;
	menuAction_s	defaults;
	menuAction_s	advanced;
	menuAction_s	done;
	menuAction_s	cancel;

	menuScrollList_s	keysList;
	menuAction_s	hintMessage;
	char		hintText[MAX_SYSPATH];
} uiControls_t;

static uiControls_t		uiControls;

/*
=================
UI_Controls_GetKeyBindings
=================
*/
static void UI_Controls_GetKeyBindings( char *command, int *twoKeys )
{
	int		i, count = 0;
	const char	*b;

	twoKeys[0] = twoKeys[1] = -1;

	for( i = 0; i < 256; i++ )
	{
		b = Key_GetBinding( i );
		if( !b ) continue;

		if( !com.stricmp( command, b ))
		{
			twoKeys[count] = i;
			count++;

			if( count == 2 ) break;
		}
	}
}

/*
=================
UI_Controls_GetKeysList
=================
*/
static void UI_Controls_GetKeysList( void )
{
	int	i, j;

	for( i = j = 0; i < MAX_KEYS; i++ )
	{
		if( com.strlen( Key_GetBinding( i )) == 0 ) continue;

		com.strncpy( uiControls.keysBind[j], Key_GetBinding( i ), sizeof( uiControls.keysBind[j] ));
		com.strncpy( uiControls.firstKey[j], Key_KeynumToString( i ), sizeof( uiControls.firstKey[j] ));
		com.strncpy( uiControls.secondKey[j], Key_KeynumToString( i ), sizeof( uiControls.secondKey[j] ));

		com.strncat( uiControls.keysDescription[j], uiControls.keysBind[j], CMD_LENGTH );
		com.strncat( uiControls.keysDescription[j], uiEmptyString, CMD_LENGTH );
		com.strncat( uiControls.keysDescription[j], uiControls.firstKey[j], KEY1_LENGTH );
		com.strncat( uiControls.keysDescription[j], uiEmptyString, KEY1_LENGTH );
		com.strncat( uiControls.keysDescription[j], uiControls.secondKey[j], KEY2_LENGTH );
		com.strncat( uiControls.keysDescription[j], uiEmptyString, KEY2_LENGTH );
		uiControls.keysDescriptionPtr[j] = uiControls.keysDescription[j];
		j++;
	}

	for( ; j < MAX_KEYS; j++ ) uiControls.keysDescriptionPtr[j] = NULL;
	uiControls.keysList.itemNames = uiControls.keysDescriptionPtr;
}

/*
=================
UI_Controls_Callback
=================
*/
static void UI_Controls_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_DONE:
	case ID_CANCEL:
		UI_PopMenu();
		break;
	case ID_DEFAULTS:
		Cbuf_ExecuteText( EXEC_NOW, "exec basekeys.rc\n" );
		UI_Controls_GetKeysList (); // reload all buttons
		break;
	case ID_ADVANCED:
		UI_AdvControls_Menu();
		break;
	}
}

/*
=================
UI_Controls_Init
=================
*/
static void UI_Controls_Init( void )
{
	Mem_Set( &uiControls, 0, sizeof( uiControls_t ));

	com.strncat( uiControls.hintText, "Action", CMD_LENGTH );
	com.strncat( uiControls.hintText, uiEmptyString, CMD_LENGTH );
	com.strncat( uiControls.hintText, "Key/Button", KEY1_LENGTH );
	com.strncat( uiControls.hintText, uiEmptyString, KEY1_LENGTH );
	com.strncat( uiControls.hintText, "Alternate", KEY2_LENGTH );
	com.strncat( uiControls.hintText, uiEmptyString, KEY2_LENGTH );

	uiControls.background.generic.id = ID_BACKGROUND;
	uiControls.background.generic.type = QMTYPE_BITMAP;
	uiControls.background.generic.flags = QMF_INACTIVE;
	uiControls.background.generic.x = 0;
	uiControls.background.generic.y = 0;
	uiControls.background.generic.width = 1024;
	uiControls.background.generic.height = 768;
	uiControls.background.pic = ART_BACKGROUND;

	uiControls.banner.generic.id = ID_BANNER;
	uiControls.banner.generic.type = QMTYPE_BITMAP;
	uiControls.banner.generic.flags = QMF_INACTIVE;
	uiControls.banner.generic.x = UI_BANNER_POSX;
	uiControls.banner.generic.y = UI_BANNER_POSY;
	uiControls.banner.generic.width = UI_BANNER_WIDTH;
	uiControls.banner.generic.height = UI_BANNER_HEIGHT;
	uiControls.banner.pic = ART_BANNER;

	uiControls.defaults.generic.id = ID_DEFAULTS;
	uiControls.defaults.generic.type = QMTYPE_ACTION;
	uiControls.defaults.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiControls.defaults.generic.x = 72;
	uiControls.defaults.generic.y = 230;
	uiControls.defaults.generic.name = "Use defaults";
	uiControls.defaults.generic.statusText = "Reset all buttons binding to their default values";
	uiControls.defaults.generic.callback = UI_Controls_Callback;

	uiControls.advanced.generic.id = ID_ADVANCED;
	uiControls.advanced.generic.type = QMTYPE_ACTION;
	uiControls.advanced.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiControls.advanced.generic.x = 72;
	uiControls.advanced.generic.y = 280;
	uiControls.advanced.generic.name = "Adv controls";
	uiControls.advanced.generic.statusText = "Change mouse sensitivity, enable autoaim, mouselook and crosshair";
	uiControls.advanced.generic.callback = UI_Controls_Callback;

	uiControls.done.generic.id = ID_DONE;
	uiControls.done.generic.type = QMTYPE_ACTION;
	uiControls.done.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiControls.done.generic.x = 72;
	uiControls.done.generic.y = 330;
	uiControls.done.generic.name = "Ok";
	uiControls.done.generic.statusText = "Save changes and return to configuration menu";
	uiControls.done.generic.callback = UI_Controls_Callback;

	uiControls.cancel.generic.id = ID_CANCEL;
	uiControls.cancel.generic.type = QMTYPE_ACTION;
	uiControls.cancel.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiControls.cancel.generic.x = 72;
	uiControls.cancel.generic.y = 380;
	uiControls.cancel.generic.name = "Cancel";
	uiControls.cancel.generic.statusText = "Discard changes and return to configuration menu";
	uiControls.cancel.generic.callback = UI_Controls_Callback;

	uiControls.hintMessage.generic.id = ID_TABLEHINT;
	uiControls.hintMessage.generic.type = QMTYPE_ACTION;
	uiControls.hintMessage.generic.flags = QMF_INACTIVE|QMF_SMALLFONT;
	uiControls.hintMessage.generic.color = uiColorHelp;
	uiControls.hintMessage.generic.name = uiControls.hintText;
	uiControls.hintMessage.generic.x = 360;
	uiControls.hintMessage.generic.y = 225;

	uiControls.keysList.generic.id = ID_KEYLIST;
	uiControls.keysList.generic.type = QMTYPE_SCROLLLIST;
	uiControls.keysList.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_SMALLFONT;
	uiControls.keysList.generic.x = 360;
	uiControls.keysList.generic.y = 255;
	uiControls.keysList.generic.width = 640;
	uiControls.keysList.generic.height = 440;
	uiControls.keysList.generic.callback = UI_Controls_Callback;

	UI_Controls_GetKeysList();

	UI_AddItem( &uiControls.menu, (void *)&uiControls.background );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.banner );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.defaults );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.advanced );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.done );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.cancel );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.hintMessage );
	UI_AddItem( &uiControls.menu, (void *)&uiControls.keysList );
}

/*
=================
UI_Controls_Precache
=================
*/
void UI_Controls_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
}

/*
=================
UI_Controls_Menu
=================
*/
void UI_Controls_Menu( void )
{
	UI_Controls_Precache();
	UI_Controls_Init();

	UI_PushMenu( &uiControls.menu );
}