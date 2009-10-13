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


#define ART_BACKGROUND		"gfx/shell/splash"
#define ART_BANNER			"gfx/shell/banners/gameoptions_t"
#define ART_TEXT1			"gfx/shell/text/gameoptions_text_p1"
#define ART_TEXT2			"gfx/shell/text/gameoptions_text_p2"

#define ID_BACKGROUND		0
#define ID_BANNER			1

#define ID_TEXT1			2
#define ID_TEXT2			3
#define ID_TEXTSHADOW1		4
#define ID_TEXTSHADOW2		5

#define ID_BACK			6

#define ID_CROSSHAIR		7
#define ID_MAXFPS			8
#define ID_FOV			9
#define ID_HAND			10
#define ID_ALLOWDOWNLOAD		11

static const char	*uiGameOptionsYesNo[] = { "False", "True" };
static const char	*uiGameOptionsHand[] = { "Right", "Left", "Hidden" };

typedef struct
{
	int		showCrosshair;

	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;

	menuBitmap_s	textShadow1;
	menuBitmap_s	textShadow2;
	menuBitmap_s	text1;
	menuBitmap_s	text2;

	menuBitmap_s	back;

	menuBitmap_s	crosshair;
	menuField_s	maxFPS;
	menuField_s	fov;
	menuSpinControl_s	hand;
	menuSpinControl_s	allowDownload;
} uiGameOptions_t;

static uiGameOptions_t	uiGameOptions;


/*
=================
UI_GameOptions_GetConfig
=================
*/
static void UI_GameOptions_GetConfig( void )
{
	uiGameOptions.showCrosshair = Cvar_VariableInteger( "cl_crosshair" );

	com.snprintf( uiGameOptions.maxFPS.buffer, sizeof( uiGameOptions.maxFPS.buffer ), "%i", Cvar_VariableInteger( "cl_maxfps "));
	com.snprintf( uiGameOptions.fov.buffer, sizeof( uiGameOptions.fov.buffer ), "%i", Cvar_VariableInteger( "fov" ));
	uiGameOptions.hand.curValue = bound( 0, Cvar_VariableInteger( "hand" ), 2 );

	if( Cvar_VariableInteger( "allow_download" )) uiGameOptions.allowDownload.curValue = 1;
}

/*
=================
UI_GameOptions_SetConfig
=================
*/
static void UI_GameOptions_SetConfig( void )
{
	Cvar_SetValue( "cl_crosshair", uiGameOptions.showCrosshair );

	if( com.atoi( uiGameOptions.maxFPS.buffer ) >= 0 )
		Cvar_SetValue( "cl_maxfps", com.atof( uiGameOptions.maxFPS.buffer ));

	if( com.atoi( uiGameOptions.fov.buffer ) >= 1 && com.atoi( uiGameOptions.fov.buffer ) <= 179 )
		Cvar_SetValue( "fov", com.atof( uiGameOptions.fov.buffer ));

	Cvar_SetValue( "hand", uiGameOptions.hand.curValue );
	Cvar_SetValue( "allow_download", uiGameOptions.allowDownload.curValue );
}

/*
=================
UI_GameOptions_UpdateConfig
=================
*/
static void UI_GameOptions_UpdateConfig( void )
{
	uiGameOptions.hand.generic.name = uiGameOptionsHand[(int)uiGameOptions.hand.curValue];
	uiGameOptions.allowDownload.generic.name = uiGameOptionsYesNo[(int)uiGameOptions.allowDownload.curValue];
}

/*
=================
UI_GameOptions_Callback
=================
*/
static void UI_GameOptions_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event == QM_CHANGED )
	{
		UI_GameOptions_UpdateConfig();
		UI_GameOptions_SetConfig();
		return;
	}

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_BACK:
		UI_PopMenu();
		break;
	case ID_CROSSHAIR:
		uiGameOptions.showCrosshair = !uiGameOptions.showCrosshair;
		UI_GameOptions_UpdateConfig();
		UI_GameOptions_SetConfig();
		break;
	}
}

/*
=================
UI_GameOptions_Ownerdraw
=================
*/
static void UI_GameOptions_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( self == (void *)&uiGameOptions.crosshair )
	{
	}
	else
	{
		if( uiGameOptions.menu.items[uiGameOptions.menu.cursor] == self )
			UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOXFOCUS );
		else UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOX );

		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
	}
}

/*
=================
UI_GameOptions_Init
=================
*/
static void UI_GameOptions_Init( void )
{
	Mem_Set( &uiGameOptions, 0, sizeof( uiGameOptions_t ));

	uiGameOptions.background.generic.id = ID_BACKGROUND;
	uiGameOptions.background.generic.type = QMTYPE_BITMAP;
	uiGameOptions.background.generic.flags = QMF_INACTIVE;
	uiGameOptions.background.generic.x = 0;
	uiGameOptions.background.generic.y = 0;
	uiGameOptions.background.generic.width = 1024;
	uiGameOptions.background.generic.height = 768;
	uiGameOptions.background.pic = ART_BACKGROUND;

	uiGameOptions.banner.generic.id = ID_BANNER;
	uiGameOptions.banner.generic.type = QMTYPE_BITMAP;
	uiGameOptions.banner.generic.flags = QMF_INACTIVE;
	uiGameOptions.banner.generic.x = 0;
	uiGameOptions.banner.generic.y = 66;
	uiGameOptions.banner.generic.width = 1024;
	uiGameOptions.banner.generic.height = 46;
	uiGameOptions.banner.pic = ART_BANNER;

	uiGameOptions.textShadow1.generic.id = ID_TEXTSHADOW1;
	uiGameOptions.textShadow1.generic.type = QMTYPE_BITMAP;
	uiGameOptions.textShadow1.generic.flags	= QMF_INACTIVE;
	uiGameOptions.textShadow1.generic.x = 182;
	uiGameOptions.textShadow1.generic.y = 170;
	uiGameOptions.textShadow1.generic.width = 256;
	uiGameOptions.textShadow1.generic.height = 256;
	uiGameOptions.textShadow1.generic.color	= uiColorBlack;
	uiGameOptions.textShadow1.pic	= ART_TEXT1;

	uiGameOptions.textShadow2.generic.id = ID_TEXTSHADOW2;
	uiGameOptions.textShadow2.generic.type = QMTYPE_BITMAP;
	uiGameOptions.textShadow2.generic.flags	= QMF_INACTIVE;
	uiGameOptions.textShadow2.generic.x = 182;
	uiGameOptions.textShadow2.generic.y = 426;
	uiGameOptions.textShadow2.generic.width = 256;
	uiGameOptions.textShadow2.generic.height = 256;
	uiGameOptions.textShadow2.generic.color	= uiColorBlack;
	uiGameOptions.textShadow2.pic	= ART_TEXT2;

	uiGameOptions.text1.generic.id = ID_TEXT1;
	uiGameOptions.text1.generic.type = QMTYPE_BITMAP;
	uiGameOptions.text1.generic.flags = QMF_INACTIVE;
	uiGameOptions.text1.generic.x	= 180;
	uiGameOptions.text1.generic.y = 168;
	uiGameOptions.text1.generic.width = 256;
	uiGameOptions.text1.generic.height = 256;
	uiGameOptions.text1.pic = ART_TEXT1;

	uiGameOptions.text2.generic.id = ID_TEXT2;
	uiGameOptions.text2.generic.type = QMTYPE_BITMAP;
	uiGameOptions.text2.generic.flags = QMF_INACTIVE;
	uiGameOptions.text2.generic.x = 180;
	uiGameOptions.text2.generic.y	= 424;
	uiGameOptions.text2.generic.width = 256;
	uiGameOptions.text2.generic.height = 256;
	uiGameOptions.text2.pic = ART_TEXT2;

	uiGameOptions.back.generic.id	= ID_BACK;
	uiGameOptions.back.generic.type = QMTYPE_BITMAP;
	uiGameOptions.back.generic.x = 413;
	uiGameOptions.back.generic.y = 656;
	uiGameOptions.back.generic.width = 198;
	uiGameOptions.back.generic.height = 38;
	uiGameOptions.back.generic.callback = UI_GameOptions_Callback;
	uiGameOptions.back.generic.ownerdraw = UI_GameOptions_Ownerdraw;
	uiGameOptions.back.pic = UI_BACKBUTTON;

	uiGameOptions.crosshair.generic.id = ID_CROSSHAIR;
	uiGameOptions.crosshair.generic.type = QMTYPE_BITMAP;
	uiGameOptions.crosshair.generic.flags = 0;
	uiGameOptions.crosshair.generic.x = 646;
	uiGameOptions.crosshair.generic.y = 144;
	uiGameOptions.crosshair.generic.width = 64;
	uiGameOptions.crosshair.generic.height = 64;
	uiGameOptions.crosshair.generic.callback = UI_GameOptions_Callback;
	uiGameOptions.crosshair.generic.ownerdraw = UI_GameOptions_Ownerdraw;
	uiGameOptions.crosshair.generic.statusText = "Click to change crosshair color";

	uiGameOptions.maxFPS.generic.id = ID_MAXFPS;
	uiGameOptions.maxFPS.generic.type = QMTYPE_FIELD;
	uiGameOptions.maxFPS.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW | QMF_NUMBERSONLY;
	uiGameOptions.maxFPS.generic.x = 580;
	uiGameOptions.maxFPS.generic.y = 288;
	uiGameOptions.maxFPS.generic.width = 198;
	uiGameOptions.maxFPS.generic.height = 30;
	uiGameOptions.maxFPS.generic.callback = UI_GameOptions_Callback;
	uiGameOptions.maxFPS.generic.statusText = "Cap your game frame rate";
	uiGameOptions.maxFPS.maxLenght = 3;
	
	uiGameOptions.fov.generic.id = ID_FOV;
	uiGameOptions.fov.generic.type = QMTYPE_FIELD;
	uiGameOptions.fov.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW | QMF_NUMBERSONLY;
	uiGameOptions.fov.generic.x = 580;
	uiGameOptions.fov.generic.y = 352;
	uiGameOptions.fov.generic.width = 198;
	uiGameOptions.fov.generic.height = 30;
	uiGameOptions.fov.generic.callback = UI_GameOptions_Callback;
	uiGameOptions.fov.generic.statusText = "Set your field of view";
	uiGameOptions.fov.maxLenght = 3;

	uiGameOptions.hand.generic.id = ID_HAND;
	uiGameOptions.hand.generic.type = QMTYPE_SPINCONTROL;
	uiGameOptions.hand.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiGameOptions.hand.generic.x = 580;
	uiGameOptions.hand.generic.y = 384;
	uiGameOptions.hand.generic.width = 198;
	uiGameOptions.hand.generic.height = 30;
	uiGameOptions.hand.generic.callback = UI_GameOptions_Callback;
	uiGameOptions.hand.generic.statusText = "Choose your gun hand preference";
	uiGameOptions.hand.minValue = 0;
	uiGameOptions.hand.maxValue = 2;
	uiGameOptions.hand.range = 1;

	uiGameOptions.allowDownload.generic.id = ID_ALLOWDOWNLOAD;
	uiGameOptions.allowDownload.generic.type = QMTYPE_SPINCONTROL;
	uiGameOptions.allowDownload.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiGameOptions.allowDownload.generic.x = 580;
	uiGameOptions.allowDownload.generic.y = 416;
	uiGameOptions.allowDownload.generic.width = 198;
	uiGameOptions.allowDownload.generic.height = 30;
	uiGameOptions.allowDownload.generic.callback = UI_GameOptions_Callback;
	uiGameOptions.allowDownload.generic.statusText = "Allow download of files from servers";
	uiGameOptions.allowDownload.minValue = 0;
	uiGameOptions.allowDownload.maxValue = 1;
	uiGameOptions.allowDownload.range = 1;

	UI_GameOptions_GetConfig();

	UI_AddItem( &uiGameOptions.menu, (void *)&uiGameOptions.background );
	UI_AddItem( &uiGameOptions.menu, (void *)&uiGameOptions.banner );
	UI_AddItem( &uiGameOptions.menu, (void *)&uiGameOptions.textShadow1 );
	UI_AddItem( &uiGameOptions.menu, (void *)&uiGameOptions.textShadow2 );
	UI_AddItem( &uiGameOptions.menu, (void *)&uiGameOptions.text1 );
	UI_AddItem( &uiGameOptions.menu, (void *)&uiGameOptions.text2 );
	UI_AddItem( &uiGameOptions.menu, (void *)&uiGameOptions.back );
	UI_AddItem( &uiGameOptions.menu, (void *)&uiGameOptions.crosshair );
	UI_AddItem( &uiGameOptions.menu, (void *)&uiGameOptions.maxFPS );
	UI_AddItem( &uiGameOptions.menu, (void *)&uiGameOptions.fov );
	UI_AddItem( &uiGameOptions.menu, (void *)&uiGameOptions.hand );
	UI_AddItem( &uiGameOptions.menu, (void *)&uiGameOptions.allowDownload );
}

/*
=================
UI_GameOptions_Precache
=================
*/
void UI_GameOptions_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT1, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT2, SHADER_NOMIP );
}

/*
=================
UI_GameOptions_Menu
=================
*/
void UI_GameOptions_Menu( void )
{
	UI_GameOptions_Precache();
	UI_GameOptions_Init();

	UI_GameOptions_UpdateConfig();
	UI_PushMenu( &uiGameOptions.menu );
}