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

#define ART_BANNER		"gfx/shell/head_creategame"

#define ID_BACKGROUND	0
#define ID_BANNER		1
#define ID_ADVOPTIONS	2
#define ID_DONE		3
#define ID_CANCEL		4
#define ID_MAPLIST		5
#define ID_TABLEHINT	6
#define ID_MAXCLIENTS	7
#define ID_HOSTNAME		8
#define ID_PASSWORD		9
#define ID_DEDICATED	10

#define MAPNAME_LENGTH	20
#define TITLE_LENGTH	20+MAPNAME_LENGTH

typedef struct
{
	string		mapName[UI_MAXGAMES];
	string		mapsDescription[UI_MAXGAMES];
	char		*mapsDescriptionPtr[UI_MAXGAMES];

	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;
	menuAction_s	advOptions;
	menuAction_s	done;
	menuAction_s	cancel;

	menuField_s	maxClients;
	menuField_s	hostName;
	menuField_s	password;
	menuCheckBox_s	dedicatedServer;

	menuScrollList_s	mapsList;
	menuAction_s	hintMessage;
	char		hintText[MAX_SYSPATH];
} uiCreateGame_t;

static uiCreateGame_t	uiCreateGame;

/*
=================
UI_CreateGame_Begin
=================
*/
static void UI_CreateGame_Begin( void )
{
	if( Host_ServerState())
		Cbuf_ExecuteText( EXEC_APPEND, "disconnect\n" );
/*
	Cvar_SetValue( "deathmatch", !((int)uiStartServer.rules.curValue));
	Cvar_SetValue( "coop", ((int)uiStartServer.rules.curValue));
	Cvar_SetValue( "timelimit", atoi(uiStartServer.timeLimit.buffer));
	Cvar_SetValue( "fraglimit", atoi(uiStartServer.fragLimit.buffer));
	Cvar_SetValue( "sv_maxclients", atoi(uiStartServer.maxClients.buffer));
	Cvar_Set( "sv_hostname", uiStartServer.hostName.buffer );

	Cbuf_ExecuteText( EXEC_APPEND, va( "map %s\n", uiCreateGame.mapsList[uiCreateGame.mapsList.curItem] ));
*/
}

/*
=================
UI_CreateGame_GetMapsList
=================
*/
static void UI_CreateGame_GetMapsList( void )
{
	string	token;
	script_t	*script;
	int	numMaps = 0;

	// create new maplist if not exist
	if( !Cmd_CheckMapsList() || (script = Com_OpenScript( "scripts/maps.lst", NULL, 0 )) == NULL )
	{
		MsgDev( D_ERROR, "Cmd_GetMapsList: can't open maps.lst\n" );
		return;
	}

	while( Com_ReadString( script, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, token ))
	{
		if( numMaps >= UI_MAXGAMES ) break;
		com.strncat( uiCreateGame.mapName[numMaps], token, sizeof( uiCreateGame.mapName[0] ));
		com.strncat( uiCreateGame.mapsDescription[numMaps], token, MAPNAME_LENGTH );
		com.strncat( uiCreateGame.mapsDescription[numMaps], uiEmptyString, MAPNAME_LENGTH );
		if( !Com_ReadString( script, SC_PARSE_GENERIC, token )) break; // unexpected end of file
		com.strncat( uiCreateGame.mapsDescription[numMaps], token, TITLE_LENGTH );
		com.strncat( uiCreateGame.mapsDescription[numMaps], uiEmptyString, TITLE_LENGTH );
		uiCreateGame.mapsDescriptionPtr[numMaps] = uiCreateGame.mapsDescription[numMaps];
		numMaps++;
	}

	if( script ) Com_CloseScript( script );
	for( ; numMaps < UI_MAXGAMES; numMaps++ ) uiCreateGame.mapsDescriptionPtr[numMaps] = NULL;
	uiCreateGame.mapsList.itemNames = uiCreateGame.mapsDescriptionPtr;
}

/*
=================
UI_CreateGame_Callback
=================
*/
static void UI_CreateGame_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	switch( item->id )
	{
	case ID_DEDICATED:
		if( event == QM_PRESSED )
			((menuCheckBox_s *)self)->focusPic = UI_CHECKBOX_PRESSED;
		else ((menuCheckBox_s *)self)->focusPic = UI_CHECKBOX_FOCUS;
		break;
	}

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_ADVOPTIONS:
		// UNDONE: not implemented
		break;
	case ID_DONE:
		UI_CreateGame_Begin();
		break;
	case ID_CANCEL:
		UI_PopMenu();
		break;
	}
}

/*
=================
UI_CreateGame_Init
=================
*/
static void UI_CreateGame_Init( void )
{
	Mem_Set( &uiCreateGame, 0, sizeof( uiCreateGame_t ));

	com.strncat( uiCreateGame.hintText, "Map", MAPNAME_LENGTH );
	com.strncat( uiCreateGame.hintText, uiEmptyString, MAPNAME_LENGTH );
	com.strncat( uiCreateGame.hintText, "Title", TITLE_LENGTH );
	com.strncat( uiCreateGame.hintText, uiEmptyString, TITLE_LENGTH );

	uiCreateGame.background.generic.id = ID_BACKGROUND;
	uiCreateGame.background.generic.type = QMTYPE_BITMAP;
	uiCreateGame.background.generic.flags = QMF_INACTIVE;
	uiCreateGame.background.generic.x = 0;
	uiCreateGame.background.generic.y = 0;
	uiCreateGame.background.generic.width = 1024;
	uiCreateGame.background.generic.height = 768;
	uiCreateGame.background.pic = ART_BACKGROUND;

	uiCreateGame.banner.generic.id = ID_BANNER;
	uiCreateGame.banner.generic.type = QMTYPE_BITMAP;
	uiCreateGame.banner.generic.flags = QMF_INACTIVE;
	uiCreateGame.banner.generic.x = UI_BANNER_POSX;
	uiCreateGame.banner.generic.y = UI_BANNER_POSY;
	uiCreateGame.banner.generic.width = UI_BANNER_WIDTH;
	uiCreateGame.banner.generic.height = UI_BANNER_HEIGHT;
	uiCreateGame.banner.pic = ART_BANNER;

	uiCreateGame.advOptions.generic.id = ID_ADVOPTIONS;
	uiCreateGame.advOptions.generic.type = QMTYPE_ACTION;
	uiCreateGame.advOptions.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_GRAYED;
	uiCreateGame.advOptions.generic.x = 72;
	uiCreateGame.advOptions.generic.y = 230;
	uiCreateGame.advOptions.generic.name = "Adv. Options";
	uiCreateGame.advOptions.generic.statusText = "Open the LAN game advanced options menu";
	uiCreateGame.advOptions.generic.callback = UI_CreateGame_Callback;

	uiCreateGame.done.generic.id = ID_DONE;
	uiCreateGame.done.generic.type = QMTYPE_ACTION;
	uiCreateGame.done.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiCreateGame.done.generic.x = 72;
	uiCreateGame.done.generic.y = 280;
	uiCreateGame.done.generic.name = "Ok";
	uiCreateGame.done.generic.statusText = "Start the multiplayer game";
	uiCreateGame.done.generic.callback = UI_CreateGame_Callback;

	uiCreateGame.cancel.generic.id = ID_CANCEL;
	uiCreateGame.cancel.generic.type = QMTYPE_ACTION;
	uiCreateGame.cancel.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiCreateGame.cancel.generic.x = 72;
	uiCreateGame.cancel.generic.y = 330;
	uiCreateGame.cancel.generic.name = "Cancel";
	uiCreateGame.cancel.generic.statusText = "Return to LAN game menu";
	uiCreateGame.cancel.generic.callback = UI_CreateGame_Callback;

	uiCreateGame.dedicatedServer.generic.id = ID_DEDICATED;
	uiCreateGame.dedicatedServer.generic.type = QMTYPE_CHECKBOX;
	uiCreateGame.dedicatedServer.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_ACT_ONRELEASE|QMF_MOUSEONLY|QMF_DROPSHADOW;
	uiCreateGame.dedicatedServer.generic.name = "Dedicated server";
	uiCreateGame.dedicatedServer.generic.x = 72;
	uiCreateGame.dedicatedServer.generic.y = 685;
	uiCreateGame.dedicatedServer.generic.callback = UI_CreateGame_Callback;
	uiCreateGame.dedicatedServer.generic.statusText = "faster, but you can't join the server from this machine";

	uiCreateGame.hintMessage.generic.id = ID_TABLEHINT;
	uiCreateGame.hintMessage.generic.type = QMTYPE_ACTION;
	uiCreateGame.hintMessage.generic.flags = QMF_INACTIVE|QMF_SMALLFONT;
	uiCreateGame.hintMessage.generic.color = uiColorHelp;
	uiCreateGame.hintMessage.generic.name = uiCreateGame.hintText;
	uiCreateGame.hintMessage.generic.x = 590;
	uiCreateGame.hintMessage.generic.y = 215;

	uiCreateGame.mapsList.generic.id = ID_MAPLIST;
	uiCreateGame.mapsList.generic.type = QMTYPE_SCROLLLIST;
	uiCreateGame.mapsList.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_SMALLFONT;
	uiCreateGame.mapsList.generic.x = 590;
	uiCreateGame.mapsList.generic.y = 245;
	uiCreateGame.mapsList.generic.width = 410;
	uiCreateGame.mapsList.generic.height = 440;
	uiCreateGame.mapsList.generic.callback = UI_CreateGame_Callback;

	uiCreateGame.hostName.generic.id = ID_HOSTNAME;
	uiCreateGame.hostName.generic.type = QMTYPE_FIELD;
	uiCreateGame.hostName.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiCreateGame.hostName.generic.name = "Server Name:";
	uiCreateGame.hostName.generic.x = 350;
	uiCreateGame.hostName.generic.y = 260;
	uiCreateGame.hostName.generic.width = 205;
	uiCreateGame.hostName.generic.height = 32;
	uiCreateGame.hostName.generic.callback = UI_CreateGame_Callback;
	uiCreateGame.hostName.maxLenght = 16;
	com.strncpy( uiCreateGame.hostName.buffer, Cvar_VariableString( "sv_hostname" ), sizeof( uiCreateGame.hostName.buffer ));

	uiCreateGame.maxClients.generic.id = ID_MAXCLIENTS;
	uiCreateGame.maxClients.generic.type = QMTYPE_FIELD;
	uiCreateGame.maxClients.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NUMBERSONLY;
	uiCreateGame.maxClients.generic.name = "Max Players:";
	uiCreateGame.maxClients.generic.x = 350;
	uiCreateGame.maxClients.generic.y = 360;
	uiCreateGame.maxClients.generic.width = 205;
	uiCreateGame.maxClients.generic.height = 32;
	uiCreateGame.maxClients.maxLenght = 3;
	if( Cvar_VariableInteger( "sv_maxclients" ) <= 1 )
		com.snprintf( uiCreateGame.maxClients.buffer, sizeof( uiCreateGame.maxClients.buffer ), "8" );
	else com.snprintf( uiCreateGame.maxClients.buffer, sizeof( uiCreateGame.maxClients.buffer ), "%i", Cvar_VariableInteger( "sv_maxclients" ));

	uiCreateGame.password.generic.id = ID_PASSWORD;
	uiCreateGame.password.generic.type = QMTYPE_FIELD;
	uiCreateGame.password.generic.flags = QMF_CENTER_JUSTIFY|QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiCreateGame.password.generic.name = "Password:";
	uiCreateGame.password.generic.x = 350;
	uiCreateGame.password.generic.y = 460;
	uiCreateGame.password.generic.width = 205;
	uiCreateGame.password.generic.height = 32;
	uiCreateGame.password.generic.callback = UI_CreateGame_Callback;
	uiCreateGame.password.maxLenght = 16;

	UI_CreateGame_GetMapsList();

	UI_AddItem( &uiCreateGame.menu, (void *)&uiCreateGame.background );
	UI_AddItem( &uiCreateGame.menu, (void *)&uiCreateGame.banner );
	UI_AddItem( &uiCreateGame.menu, (void *)&uiCreateGame.advOptions );
	UI_AddItem( &uiCreateGame.menu, (void *)&uiCreateGame.done );
	UI_AddItem( &uiCreateGame.menu, (void *)&uiCreateGame.cancel );
	UI_AddItem( &uiCreateGame.menu, (void *)&uiCreateGame.maxClients );
	UI_AddItem( &uiCreateGame.menu, (void *)&uiCreateGame.hostName );
	UI_AddItem( &uiCreateGame.menu, (void *)&uiCreateGame.password );
	UI_AddItem( &uiCreateGame.menu, (void *)&uiCreateGame.dedicatedServer );
	UI_AddItem( &uiCreateGame.menu, (void *)&uiCreateGame.hintMessage );
	UI_AddItem( &uiCreateGame.menu, (void *)&uiCreateGame.mapsList );
}

/*
=================
UI_CreateGame_Precache
=================
*/
void UI_CreateGame_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
}

/*
=================
UI_CreateGame_Menu
=================
*/
void UI_CreateGame_Menu( void )
{
	UI_CreateGame_Precache();
	UI_CreateGame_Init();

	UI_PushMenu( &uiCreateGame.menu );
}