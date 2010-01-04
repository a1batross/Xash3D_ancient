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

#define ART_BANNER	  	"gfx/shell/head_vidoptions"
#define ART_GAMMA		"gfx/shell/gamma"

#define ID_BACKGROUND 	0
#define ID_BANNER	  	1
#define ID_DONE	  	2
#define ID_BLOOM_INTENSITY	3
#define ID_GAMMA		4
#define ID_GLARE_REDUCTION	5 
#define ID_SIMPLE_SKY	6

typedef struct
{
	int		outlineWidth;
	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;
	menuBitmap_s	testImage;

	menuAction_s	done;

	menuSlider_s	bloomIntensity;
	menuSlider_s	gammaIntensity;
	menuSlider_s	glareReduction;
	menuCheckBox_s	fastSky;
} uiVidOptions_t;

static uiVidOptions_t	uiVidOptions;


/*
=================
UI_VidOptions_GetConfig
=================
*/
static void UI_VidOptions_GetConfig( void )
{
	uiVidOptions.bloomIntensity.curValue = Cvar_VariableValue( "r_bloom_intensity" ) / 10.0f;
	uiVidOptions.gammaIntensity.curValue = (Cvar_VariableValue( "vid_gamma" ) - 0.5f) / 1.8f;
	uiVidOptions.glareReduction.curValue = Cvar_VariableValue( "r_flaresize" ) / 60.0f;

	if( Cvar_VariableInteger( "r_fastsky" ))
		uiVidOptions.fastSky.enabled = 1;

	uiVidOptions.outlineWidth = 2;
	UI_ScaleCoords( NULL, NULL, &uiVidOptions.outlineWidth, NULL );
}

/*
=================
UI_VidOptions_UpdateConfig
=================
*/
static void UI_VidOptions_UpdateConfig( void )
{
	Cvar_SetValue( "r_bloom_intensity", uiVidOptions.bloomIntensity.curValue * 10.0f );
	Cvar_SetValue( "vid_gamma", (uiVidOptions.gammaIntensity.curValue * 1.8f) + 0.5f );
	Cvar_SetValue( "r_flaresize", uiVidOptions.glareReduction.curValue * 60.0f );
	Cvar_SetValue( "r_fastsky", uiVidOptions.fastSky.enabled );
}

/*
=================
UI_VidOptions_Ownerdraw
=================
*/
static void UI_VidOptions_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
	UI_DrawRectangleExt( item->x, item->y, item->width, item->height, uiColorRed, uiVidOptions.outlineWidth );
}

/*
=================
UI_VidOptions_Callback
=================
*/
static void UI_VidOptions_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	switch( item->id )
	{
	case ID_SIMPLE_SKY:
		if( event == QM_PRESSED )
			((menuCheckBox_s *)self)->focusPic = UI_CHECKBOX_PRESSED;
		else ((menuCheckBox_s *)self)->focusPic = UI_CHECKBOX_FOCUS;
		break;
	}

	if( event == QM_CHANGED )
	{
		UI_VidOptions_UpdateConfig();
		return;
	}

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_DONE:
		UI_PopMenu();
		break;
	}
}

/*
=================
UI_VidOptions_Init
=================
*/
static void UI_VidOptions_Init( void )
{
	Mem_Set( &uiVidOptions, 0, sizeof( uiVidOptions_t ));

	uiVidOptions.background.generic.id = ID_BACKGROUND;
	uiVidOptions.background.generic.type = QMTYPE_BITMAP;
	uiVidOptions.background.generic.flags = QMF_INACTIVE;
	uiVidOptions.background.generic.x = 0;
	uiVidOptions.background.generic.y = 0;
	uiVidOptions.background.generic.width = 1024;
	uiVidOptions.background.generic.height = 768;
	uiVidOptions.background.pic = ART_BACKGROUND;

	uiVidOptions.banner.generic.id = ID_BANNER;
	uiVidOptions.banner.generic.type = QMTYPE_BITMAP;
	uiVidOptions.banner.generic.flags = QMF_INACTIVE;
	uiVidOptions.banner.generic.x = UI_BANNER_POSX;
	uiVidOptions.banner.generic.y = UI_BANNER_POSY;
	uiVidOptions.banner.generic.width = UI_BANNER_WIDTH;
	uiVidOptions.banner.generic.height = UI_BANNER_HEIGHT;
	uiVidOptions.banner.pic = ART_BANNER;

	uiVidOptions.testImage.generic.id = ID_BANNER;
	uiVidOptions.testImage.generic.type = QMTYPE_BITMAP;
	uiVidOptions.testImage.generic.flags = QMF_INACTIVE;
	uiVidOptions.testImage.generic.x = 390;
	uiVidOptions.testImage.generic.y = 225;
	uiVidOptions.testImage.generic.width = 460;
	uiVidOptions.testImage.generic.height = 450;
	uiVidOptions.testImage.pic = ART_GAMMA;
	uiVidOptions.testImage.generic.ownerdraw = UI_VidOptions_Ownerdraw;

	uiVidOptions.done.generic.id = ID_DONE;
	uiVidOptions.done.generic.type = QMTYPE_ACTION;
	uiVidOptions.done.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiVidOptions.done.generic.x = 72;
	uiVidOptions.done.generic.y = 435;
	uiVidOptions.done.generic.name = "Done";
	uiVidOptions.done.generic.statusText = "Go back to the Video Menu";
	uiVidOptions.done.generic.callback = UI_VidOptions_Callback;

	uiVidOptions.bloomIntensity.generic.id = ID_BLOOM_INTENSITY;
	uiVidOptions.bloomIntensity.generic.type = QMTYPE_SLIDER;
	uiVidOptions.bloomIntensity.generic.flags = QMF_PULSEIFFOCUS|QMF_DROPSHADOW;
	uiVidOptions.bloomIntensity.generic.name = "Bloom intensity";
	uiVidOptions.bloomIntensity.generic.x = 72;
	uiVidOptions.bloomIntensity.generic.y = 280;
	uiVidOptions.bloomIntensity.generic.callback = UI_VidOptions_Callback;
	uiVidOptions.bloomIntensity.generic.statusText = "Set bloom intensity (0 value disables bloom)";
	uiVidOptions.bloomIntensity.minValue = 0.0;
	uiVidOptions.bloomIntensity.maxValue = 1.0;
	uiVidOptions.bloomIntensity.range = 0.05f;

	uiVidOptions.gammaIntensity.generic.id = ID_GAMMA;
	uiVidOptions.gammaIntensity.generic.type = QMTYPE_SLIDER;
	uiVidOptions.gammaIntensity.generic.flags = QMF_PULSEIFFOCUS|QMF_DROPSHADOW;
	uiVidOptions.gammaIntensity.generic.name = "Gamma";
	uiVidOptions.gammaIntensity.generic.x = 72;
	uiVidOptions.gammaIntensity.generic.y = 340;
	uiVidOptions.gammaIntensity.generic.callback = UI_VidOptions_Callback;
	uiVidOptions.gammaIntensity.generic.statusText = "Set gamma value (0.5 - 2.3)";
	uiVidOptions.gammaIntensity.minValue = 0.0;
	uiVidOptions.gammaIntensity.maxValue = 1.0;
	uiVidOptions.gammaIntensity.range = 0.05f;

	uiVidOptions.glareReduction.generic.id = ID_GLARE_REDUCTION;
	uiVidOptions.glareReduction.generic.type = QMTYPE_SLIDER;
	uiVidOptions.glareReduction.generic.flags = QMF_PULSEIFFOCUS|QMF_DROPSHADOW;
	uiVidOptions.glareReduction.generic.name = "Glare reduction";
	uiVidOptions.glareReduction.generic.x = 72;
	uiVidOptions.glareReduction.generic.y = 400;
	uiVidOptions.glareReduction.generic.callback = UI_VidOptions_Callback;
	uiVidOptions.glareReduction.generic.statusText = "Set glare reduction level";
	uiVidOptions.glareReduction.minValue = 0.0;
	uiVidOptions.glareReduction.maxValue = 1.0;
	uiVidOptions.glareReduction.range = 0.05f;

	uiVidOptions.fastSky.generic.id = ID_SIMPLE_SKY;
	uiVidOptions.fastSky.generic.type = QMTYPE_CHECKBOX;
	uiVidOptions.fastSky.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_ACT_ONRELEASE|QMF_MOUSEONLY|QMF_DROPSHADOW;
	uiVidOptions.fastSky.generic.name = "Draw simple sky";
	uiVidOptions.fastSky.generic.x = 72;
	uiVidOptions.fastSky.generic.y = 685;
	uiVidOptions.fastSky.generic.callback = UI_VidOptions_Callback;
	uiVidOptions.fastSky.generic.statusText = "enable/disable fast sky rendering (for old computers)";

	UI_VidOptions_GetConfig();

	UI_AddItem( &uiVidOptions.menu, (void *)&uiVidOptions.background );
	UI_AddItem( &uiVidOptions.menu, (void *)&uiVidOptions.banner );
	UI_AddItem( &uiVidOptions.menu, (void *)&uiVidOptions.done );
	UI_AddItem( &uiVidOptions.menu, (void *)&uiVidOptions.bloomIntensity );
	UI_AddItem( &uiVidOptions.menu, (void *)&uiVidOptions.gammaIntensity );
	UI_AddItem( &uiVidOptions.menu, (void *)&uiVidOptions.glareReduction );
	UI_AddItem( &uiVidOptions.menu, (void *)&uiVidOptions.fastSky );
	UI_AddItem( &uiVidOptions.menu, (void *)&uiVidOptions.testImage );
}

/*
=================
UI_VidOptions_Precache
=================
*/
void UI_VidOptions_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
	re->RegisterShader( ART_GAMMA, SHADER_NOMIP );
}

/*
=================
UI_VidOptions_Menu
=================
*/
void UI_VidOptions_Menu( void )
{
	UI_VidOptions_Precache();
	UI_VidOptions_Init();

	UI_VidOptions_UpdateConfig();
	UI_PushMenu( &uiVidOptions.menu );
}