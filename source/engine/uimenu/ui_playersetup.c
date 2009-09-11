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


#define ART_BACKGROUND	"gfx/shell/misc/ui_sub_options"
#define ART_BANNER		"gfx/shell/banners/playersetup_t"
#define ART_TEXT1		"gfx/shell/text/playersetup_text_p1"
#define ART_TEXT2		"gfx/shell/text/playersetup_text_p2"
#define ART_PLAYERVIEW	"gfx/shell/segments/player_view"

#define ID_BACKGROUND	0
#define ID_BANNER		1

#define ID_TEXT1		2
#define ID_TEXT2		3
#define ID_TEXTSHADOW1	4
#define ID_TEXTSHADOW2	5

#define ID_BACK		6

#define ID_VIEW		7
#define ID_NAME		8
#define ID_MODEL		9
#define ID_SKIN		10

#define MAX_PLAYERMODELS	256
#define MAX_PLAYERSKINS	2048

typedef struct
{
	string		playerModels[MAX_PLAYERMODELS];
	int		numPlayerModels;
	string		currentModel;
	
	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;

	menuBitmap_s	textShadow1;
	menuBitmap_s	textShadow2;
	menuBitmap_s	text1;
	menuBitmap_s	text2;
	menuBitmap_s	back;
	menuBitmap_s	view;

	menuField_s	name;
	menuSpinControl_s	model;
	menuSpinControl_s	skin;
} uiPlayerSetup_t;

static uiPlayerSetup_t	uiPlayerSetup;


/*
=================
UI_PlayerSetup_CalcFov

I need this here...
=================
*/
float UI_PlayerSetup_CalcFov( float fovX, float width, float height )
{
	float	x, y;

	fovX = bound( 1, fovX, 179 );
	x = width / com.tan( fovX / 360 * M_PI );
	y = atan( height / x) * 360 / M_PI;

	return y;
}

/*
=================
UI_PlayerSetup_FindModels
=================
*/
static void UI_PlayerSetup_FindModels( void )
{
	search_t	*search;
	int	i;

	uiPlayerSetup.numPlayerModels = 0;

	// Get file list
	search = FS_Search( "models/players/*.mdl", true );
	if( !search ) return;

	// build the model list
	for( i = 0; search && i < search->numfilenames; i++ )
	{
		com.strncpy( uiPlayerSetup.playerModels[uiPlayerSetup.numPlayerModels], search->filenames[i],
		sizeof(uiPlayerSetup.playerModels[uiPlayerSetup.numPlayerModels] ));
		uiPlayerSetup.numPlayerModels++;
	}
	Mem_Free( search );
}

/*
=================
UI_PlayerSetup_GetConfig
=================
*/
static void UI_PlayerSetup_GetConfig( void )
{
	string	model;
	char	*ch;
	int	i;

	com.strncpy( uiPlayerSetup.name.buffer, Cvar_VariableString( "name" ), sizeof( uiPlayerSetup.name.buffer ));

	// Get user set skin
	com.strncpy( model, Cvar_VariableString( "skin" ), sizeof(model));
	ch = strchr(model, '/');

	// find models
	UI_PlayerSetup_FindModels();

	// select current model
	for( i = 0; i < uiPlayerSetup.numPlayerModels; i++ )
	{
		if( !com.stricmp( uiPlayerSetup.playerModels[i], model ))
		{
			uiPlayerSetup.model.curValue = (float)i;
			break;
		}
	}

	com.strncpy( uiPlayerSetup.currentModel, uiPlayerSetup.playerModels[(int)uiPlayerSetup.model.curValue], sizeof(uiPlayerSetup.currentModel ));
	uiPlayerSetup.model.maxValue = (float)(uiPlayerSetup.numPlayerModels - 1);

	// select current skin
	uiPlayerSetup.skin.curValue = (float)Cvar_VariableInteger( "skin" );
	uiPlayerSetup.skin.maxValue = 1; // FIXME
}

/*
=================
UI_PlayerSetup_SetConfig
=================
*/
static void UI_PlayerSetup_SetConfig( void )
{
	Cvar_Set( "name", uiPlayerSetup.name.buffer );
	Cvar_SetValue( "skin", uiPlayerSetup.skin.curValue );
}

/*
=================
UI_PlayerSetup_UpdateConfig
=================
*/
static void UI_PlayerSetup_UpdateConfig( void )
{
	// see if the model has changed
	if( com.stricmp( uiPlayerSetup.currentModel, uiPlayerSetup.playerModels[(int)uiPlayerSetup.model.curValue] ))
	{
		com.strncpy( uiPlayerSetup.currentModel, uiPlayerSetup.playerModels[(int)uiPlayerSetup.model.curValue], sizeof( uiPlayerSetup.currentModel ));
		uiPlayerSetup.skin.curValue = (float)0; // FIXME
	}

	uiPlayerSetup.model.generic.name = uiPlayerSetup.playerModels[(int)uiPlayerSetup.model.curValue];
}

/*
=================
UI_PlayerSetup_Callback
=================
*/
static void UI_PlayerSetup_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event == QM_CHANGED )
	{
		UI_PlayerSetup_UpdateConfig();
		UI_PlayerSetup_SetConfig();
		return;
	}

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_BACK:
		UI_PopMenu();
		break;
	}
}

/*
=================
UI_PlayerSetup_Ownerdraw
=================
*/
static void UI_PlayerSetup_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( item->id == ID_VIEW )
	{
		int		x = 630, y = 226, w = 316, h = 316;
		static ref_params_t	refDef;
		static edict_t	ent;

		// draw the background
		UI_ScaleCoords( &x, &y, &w, &h );
		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, uiPlayerSetup.view.pic );

		re->ClearScene();

		// draw the player model
		Mem_Set( &ent, 0, sizeof( edict_t ));
		ent.v.model = re->RegisterModel( uiPlayerSetup.currentModel, MAX_MODELS - 1 );
//		ent.v.sequence = sequence;
//		ent.v.gaitsequence = sequence;
		ent.v.modelindex = MAX_MODELS - 1;
		ent.v.effects |= EF_ANIMATE;
		ent.v.controller[0] = 127;
		ent.v.controller[1] = 127;
		ent.v.controller[2] = 127;
		ent.v.controller[3] = 127;
		VectorSet( ent.v.origin, 80, 0, 0 );
		VectorCopy( ent.v.origin, ent.v.oldorigin );

		re->AddRefEntity( &ent, ED_NORMAL, 1.0f );

		// create and render the scene
		refDef.viewport[0] = item->x + (item->width / 12);
		refDef.viewport[1] = item->y + (item->height / 12);
		refDef.viewport[2] = item->width - (item->width / 6);
		refDef.viewport[3] = item->height - (item->height / 6);
		refDef.fov_x = 40;
		refDef.fov_y = UI_PlayerSetup_CalcFov( refDef.fov_x, refDef.viewport[2], refDef.viewport[3] );
		refDef.rdflags = RDF_NOWORLDMODEL;
		refDef.oldtime = refDef.time;
		refDef.time = uiStatic.realTime;
		re->RenderFrame( &refDef );
	}
	else
	{
		if( uiPlayerSetup.menu.items[uiPlayerSetup.menu.cursor] == self )
			UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOXFOCUS );
		else UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOX );

		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
	}
}

/*
=================
UI_PlayerSetup_Init
=================
*/
static void UI_PlayerSetup_Init( void )
{
	Mem_Set( &uiPlayerSetup, 0, sizeof( uiPlayerSetup_t ));

	uiPlayerSetup.background.generic.id = ID_BACKGROUND;
	uiPlayerSetup.background.generic.type = QMTYPE_BITMAP;
	uiPlayerSetup.background.generic.flags = QMF_INACTIVE;
	uiPlayerSetup.background.generic.x = 0;
	uiPlayerSetup.background.generic.y = 0;
	uiPlayerSetup.background.generic.width = 1024;
	uiPlayerSetup.background.generic.height = 768;
	uiPlayerSetup.background.pic = ART_BACKGROUND;

	uiPlayerSetup.banner.generic.id = ID_BANNER;
	uiPlayerSetup.banner.generic.type = QMTYPE_BITMAP;
	uiPlayerSetup.banner.generic.flags = QMF_INACTIVE;
	uiPlayerSetup.banner.generic.x = 0;
	uiPlayerSetup.banner.generic.y = 66;
	uiPlayerSetup.banner.generic.width = 1024;
	uiPlayerSetup.banner.generic.height = 46;
	uiPlayerSetup.banner.pic  = ART_BANNER;

	uiPlayerSetup.textShadow1.generic.id = ID_TEXTSHADOW1;
	uiPlayerSetup.textShadow1.generic.type = QMTYPE_BITMAP;
	uiPlayerSetup.textShadow1.generic.flags = QMF_INACTIVE;
	uiPlayerSetup.textShadow1.generic.x = 98;
	uiPlayerSetup.textShadow1.generic.y = 230;
	uiPlayerSetup.textShadow1.generic.width = 128;
	uiPlayerSetup.textShadow1.generic.height = 256;
	uiPlayerSetup.textShadow1.generic.color = uiColorBlack;
	uiPlayerSetup.textShadow1.pic = ART_TEXT1;

	uiPlayerSetup.textShadow2.generic.id = ID_TEXTSHADOW2;
	uiPlayerSetup.textShadow2.generic.type = QMTYPE_BITMAP;
	uiPlayerSetup.textShadow2.generic.flags = QMF_INACTIVE;
	uiPlayerSetup.textShadow2.generic.x = 98;
	uiPlayerSetup.textShadow2.generic.y = 490;
	uiPlayerSetup.textShadow2.generic.width = 128;
	uiPlayerSetup.textShadow2.generic.height = 128;
	uiPlayerSetup.textShadow2.generic.color = uiColorBlack;
	uiPlayerSetup.textShadow2.pic = ART_TEXT2;

	uiPlayerSetup.text1.generic.id = ID_TEXT1;
	uiPlayerSetup.text1.generic.type = QMTYPE_BITMAP;
	uiPlayerSetup.text1.generic.flags = QMF_INACTIVE;
	uiPlayerSetup.text1.generic.x = 96;
	uiPlayerSetup.text1.generic.y = 228;
	uiPlayerSetup.text1.generic.width = 128;
	uiPlayerSetup.text1.generic.height = 256;
	uiPlayerSetup.text1.pic  = ART_TEXT1;

	uiPlayerSetup.text2.generic.id = ID_TEXT2;
	uiPlayerSetup.text2.generic.type = QMTYPE_BITMAP;
	uiPlayerSetup.text2.generic.flags = QMF_INACTIVE;
	uiPlayerSetup.text2.generic.x = 96;
	uiPlayerSetup.text2.generic.y = 488;
	uiPlayerSetup.text2.generic.width = 128;
	uiPlayerSetup.text2.generic.height = 128;
	uiPlayerSetup.text2.pic  = ART_TEXT2;

	uiPlayerSetup.back.generic.id = ID_BACK;
	uiPlayerSetup.back.generic.type = QMTYPE_BITMAP;
	uiPlayerSetup.back.generic.x = 413;
	uiPlayerSetup.back.generic.y = 656;
	uiPlayerSetup.back.generic.width = 198;
	uiPlayerSetup.back.generic.height = 38;
	uiPlayerSetup.back.generic.callback = UI_PlayerSetup_Callback;
	uiPlayerSetup.back.generic.ownerdraw = UI_PlayerSetup_Ownerdraw;
	uiPlayerSetup.back.pic  = UI_BACKBUTTON;

	uiPlayerSetup.view.generic.id = ID_VIEW;
	uiPlayerSetup.view.generic.type = QMTYPE_BITMAP;
	uiPlayerSetup.view.generic.flags = QMF_INACTIVE;
	uiPlayerSetup.view.generic.x = 628;
	uiPlayerSetup.view.generic.y = 224;
	uiPlayerSetup.view.generic.width = 320;
	uiPlayerSetup.view.generic.height = 320;
	uiPlayerSetup.view.generic.ownerdraw = UI_PlayerSetup_Ownerdraw;
	uiPlayerSetup.view.pic  = ART_PLAYERVIEW;

	uiPlayerSetup.name.generic.id = ID_NAME;
	uiPlayerSetup.name.generic.type = QMTYPE_FIELD;
	uiPlayerSetup.name.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiPlayerSetup.name.generic.x = 368;
	uiPlayerSetup.name.generic.y = 224;
	uiPlayerSetup.name.generic.width = 198;
	uiPlayerSetup.name.generic.height = 30;
	uiPlayerSetup.name.generic.callback = UI_PlayerSetup_Callback;
	uiPlayerSetup.name.generic.statusText = "Enter your multiplayer display name";
	uiPlayerSetup.name.maxLenght = 32;

	uiPlayerSetup.model.generic.id = ID_MODEL;
	uiPlayerSetup.model.generic.type = QMTYPE_SPINCONTROL;
	uiPlayerSetup.model.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiPlayerSetup.model.generic.x = 368;
	uiPlayerSetup.model.generic.y = 256;
	uiPlayerSetup.model.generic.width = 198;
	uiPlayerSetup.model.generic.height = 30;
	uiPlayerSetup.model.generic.callback = UI_PlayerSetup_Callback;
	uiPlayerSetup.model.generic.statusText = "Select a model for representation in multiplayer";
	uiPlayerSetup.model.minValue = 0;
	uiPlayerSetup.model.maxValue = 1;
	uiPlayerSetup.model.range  = 1;

	uiPlayerSetup.skin.generic.id = ID_SKIN;
	uiPlayerSetup.skin.generic.type = QMTYPE_SPINCONTROL;
	uiPlayerSetup.skin.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiPlayerSetup.skin.generic.x = 368;
	uiPlayerSetup.skin.generic.y = 288;
	uiPlayerSetup.skin.generic.width = 198;
	uiPlayerSetup.skin.generic.height = 30;
	uiPlayerSetup.skin.generic.callback = UI_PlayerSetup_Callback;
	uiPlayerSetup.skin.generic.statusText = "Select a skin for representation in multiplayer";
	uiPlayerSetup.skin.minValue  = 0;
	uiPlayerSetup.skin.maxValue  = 1;
	uiPlayerSetup.skin.range  = 1;

	UI_PlayerSetup_GetConfig();

	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.background );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.banner );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.textShadow1 );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.textShadow2 );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.text1 );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.text2 );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.back );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.view );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.name );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.model );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.skin );
}

/*
=================
UI_PlayerSetup_Precache
=================
*/
void UI_PlayerSetup_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT1, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT2, SHADER_NOMIP );
	re->RegisterShader( ART_PLAYERVIEW, SHADER_NOMIP );
}

/*
=================
UI_PlayerSetup_Menu
=================
*/
void UI_PlayerSetup_Menu( void )
{
	UI_PlayerSetup_Precache();
	UI_PlayerSetup_Init();

	UI_PlayerSetup_UpdateConfig();
	UI_PushMenu( &uiPlayerSetup.menu );
}