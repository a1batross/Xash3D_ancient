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
#define ART_BANNER		"gfx/shell/head_customize"

#define ID_BACKGROUND	0
#define ID_BANNER		1
#define ID_DONE		2
#define ID_ADVOPTIONS	3
#define ID_VIEW		4
#define ID_NAME		5
#define ID_MODEL		6

#define MAX_PLAYERMODELS	100

typedef struct
{
	string		models[MAX_PLAYERMODELS];
	int		num_models;
	string		currentModel;

	ref_params_t	refdef;
	edict_t		ent;
	
	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;

	menuAction_s	done;
	menuAction_s	AdvOptions;
	menuBitmap_s	view;

	menuField_s	name;
	menuSpinControl_s	model;
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
	string	name, path;
	int	i;

	uiPlayerSetup.num_models = 0;

	// Get file list
	search = FS_Search( "models/player/*", true );
	if( !search ) return;

	// add default singleplayer model
	com.strncpy( uiPlayerSetup.models[uiPlayerSetup.num_models], "player", sizeof(uiPlayerSetup.models[uiPlayerSetup.num_models] ));
	uiPlayerSetup.num_models++;

	// build the model list
	for( i = 0; search && i < search->numfilenames; i++ )
	{
		FS_FileBase( search->filenames[i], name );
		com.snprintf( path, MAX_STRING, "models/player/%s/%s.mdl", name, name );
		if( !FS_FileExists( path )) continue;

		com.strncpy( uiPlayerSetup.models[uiPlayerSetup.num_models], name, sizeof(uiPlayerSetup.models[uiPlayerSetup.num_models] ));
		uiPlayerSetup.num_models++;
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
	int	i;

	com.strncpy( uiPlayerSetup.name.buffer, Cvar_VariableString( "name" ), sizeof( uiPlayerSetup.name.buffer ));

	// find models
	UI_PlayerSetup_FindModels();

	// select current model
	for( i = 0; i < uiPlayerSetup.num_models; i++ )
	{
		if( !com.stricmp( uiPlayerSetup.models[i], Cvar_VariableString( "model" )))
		{
			uiPlayerSetup.model.curValue = (float)i;
			break;
		}
	}

	com.strncpy( uiPlayerSetup.currentModel, uiPlayerSetup.models[(int)uiPlayerSetup.model.curValue], sizeof(uiPlayerSetup.currentModel ));
	uiPlayerSetup.model.maxValue = (float)(uiPlayerSetup.num_models - 1);
}

/*
=================
UI_PlayerSetup_SetConfig
=================
*/
static void UI_PlayerSetup_SetConfig( void )
{
	Cvar_Set( "name", uiPlayerSetup.name.buffer );
	Cvar_Set( "model", uiPlayerSetup.currentModel );
}

/*
=================
UI_PlayerSetup_UpdateConfig
=================
*/
static void UI_PlayerSetup_UpdateConfig( void )
{
	string	path, name;

	// see if the model has changed
	if( com.stricmp( uiPlayerSetup.currentModel, uiPlayerSetup.models[(int)uiPlayerSetup.model.curValue] ))
	{
		com.strncpy( uiPlayerSetup.currentModel, uiPlayerSetup.models[(int)uiPlayerSetup.model.curValue], sizeof( uiPlayerSetup.currentModel ));
	}

	uiPlayerSetup.model.generic.name = uiPlayerSetup.models[(int)uiPlayerSetup.model.curValue];
	com.strncpy( name, uiPlayerSetup.models[(int)uiPlayerSetup.model.curValue], sizeof( name ));

	if( !com.stricmp( name, "player" ))
		com.strncpy( path, "models/player.mdl", MAX_STRING );
	else com.snprintf( path, MAX_STRING, "models/player/%s/%s.mdl", name, name );
	uiPlayerSetup.ent.v.model = re->RegisterModel( path, MAX_MODELS - 1 );
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
	case ID_DONE:
		UI_PopMenu();
		break;
	case ID_ADVOPTIONS:
		UI_GameOptions_Menu();
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
	float	realtime = uiStatic.realTime * 0.001f;

	// draw the background
	UI_FillRect( item->x, item->y, item->width, item->height, uiColorDkGrey );

	// draw the rectangle
	UI_DrawRectangle( item->x, item->y, item->width, item->height, uiScrollOutlineColor );

	V_ClearScene();

	// update renderer timings
	uiPlayerSetup.ent.v.animtime = realtime;
	uiPlayerSetup.refdef.time = realtime;
	uiPlayerSetup.refdef.frametime = cls.frametime;

	// draw the player model
	re->AddRefEntity( &uiPlayerSetup.ent, ED_NORMAL );
	re->RenderFrame( &uiPlayerSetup.refdef );
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
	uiPlayerSetup.banner.generic.x = 65;
	uiPlayerSetup.banner.generic.y = 92;
	uiPlayerSetup.banner.generic.width = 690;
	uiPlayerSetup.banner.generic.height = 120;
	uiPlayerSetup.banner.pic = ART_BANNER;

	uiPlayerSetup.done.generic.id = ID_DONE;
	uiPlayerSetup.done.generic.type = QMTYPE_ACTION;
	uiPlayerSetup.done.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiPlayerSetup.done.generic.x = 72;
	uiPlayerSetup.done.generic.y = 230;
	uiPlayerSetup.done.generic.name = "Done";
	uiPlayerSetup.done.generic.statusText = "Go back to the Multiplayer Menu";
	uiPlayerSetup.done.generic.callback = UI_PlayerSetup_Callback;

	uiPlayerSetup.AdvOptions.generic.id = ID_ADVOPTIONS;
	uiPlayerSetup.AdvOptions.generic.type = QMTYPE_ACTION;
	uiPlayerSetup.AdvOptions.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiPlayerSetup.AdvOptions.generic.x = 72;
	uiPlayerSetup.AdvOptions.generic.y = 280;
	uiPlayerSetup.AdvOptions.generic.name = "Adv. Options";
	uiPlayerSetup.AdvOptions.generic.statusText = "Configure handness, fov and other advanced options";
	uiPlayerSetup.AdvOptions.generic.callback = UI_PlayerSetup_Callback;

	uiPlayerSetup.view.generic.id = ID_VIEW;
	uiPlayerSetup.view.generic.type = QMTYPE_BITMAP;
	uiPlayerSetup.view.generic.flags = QMF_INACTIVE;
	uiPlayerSetup.view.generic.x = 628;
	uiPlayerSetup.view.generic.y = 224;
	uiPlayerSetup.view.generic.width = 190;
	uiPlayerSetup.view.generic.height = 280;
	uiPlayerSetup.view.generic.ownerdraw = UI_PlayerSetup_Ownerdraw;

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

	UI_PlayerSetup_GetConfig();

	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.background );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.banner );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.done );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.AdvOptions );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.view );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.name );
	UI_AddItem( &uiPlayerSetup.menu, (void *)&uiPlayerSetup.model );

	// setup render and actor
	uiPlayerSetup.refdef.fov_x = 40;

	// NOTE: must be called after UI_AddItem whan we sure what UI_ScaleCoords is done
	uiPlayerSetup.refdef.viewport[0] = uiPlayerSetup.view.generic.x + (uiPlayerSetup.view.generic.width / 12);
	uiPlayerSetup.refdef.viewport[1] = uiPlayerSetup.view.generic.y + (uiPlayerSetup.view.generic.height / 12);
	uiPlayerSetup.refdef.viewport[2] = uiPlayerSetup.view.generic.width - (uiPlayerSetup.view.generic.width / 6);
	uiPlayerSetup.refdef.viewport[3] = uiPlayerSetup.view.generic.height - (uiPlayerSetup.view.generic.height / 6);

	uiPlayerSetup.refdef.fov_y = UI_PlayerSetup_CalcFov( uiPlayerSetup.refdef.fov_x, uiPlayerSetup.refdef.viewport[2], uiPlayerSetup.refdef.viewport[3] );
	uiPlayerSetup.refdef.flags = RDF_NOWORLDMODEL;

	uiPlayerSetup.ent.serialnumber = MAX_EDICTS - 1;
	uiPlayerSetup.ent.v.sequence = 1;
	uiPlayerSetup.ent.v.scale = 1.0f;
	uiPlayerSetup.ent.v.frame = -1.0f;
	uiPlayerSetup.ent.v.framerate = 1.0f;
	uiPlayerSetup.ent.v.modelindex = MAX_MODELS - 1;
	uiPlayerSetup.ent.v.effects |= (EF_ANIMATE|EF_FULLBRIGHT);
	uiPlayerSetup.ent.v.controller[0] = 127;
	uiPlayerSetup.ent.v.controller[1] = 127;
	uiPlayerSetup.ent.v.controller[2] = 127;
	uiPlayerSetup.ent.v.controller[3] = 127;
	uiPlayerSetup.ent.v.origin[0] = 80;
	uiPlayerSetup.ent.v.origin[2] = 2;
	uiPlayerSetup.ent.v.angles[1] = 180;
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