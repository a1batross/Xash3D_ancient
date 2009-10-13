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


#define ART_BACKGROUND	"gfx/shell/splash"
#define ART_BANNER	  	"gfx/shell/banners/video_t"
#define ART_TEXT1	  	"gfx/shell/text/video_text_p1"
#define ART_TEXT2	  	"gfx/shell/text/video_text_p2"
#define ART_ADVANCED  	"gfx/shell/buttons/advanced_b"

#define ID_BACKGROUND 	0
#define ID_BANNER	  	1

#define ID_TEXT1	  	2
#define ID_TEXT2	  	3
#define ID_TEXTSHADOW1	4
#define ID_TEXTSHADOW2	5

#define ID_CANCEL	  	6
#define ID_ADVANCED	  	7
#define ID_APPLY	  	8

#define ID_GRAPHICSSETTINGS	9
#define ID_GLEXTENSIONS	10
#define ID_VIDEOMODE	11
#define ID_FULLSCREEN	12
#define ID_DISPLAYFREQUENCY	13
#define ID_COLORDEPTH	14
#define ID_HARDWAREGAMMA	15
#define ID_GAMMA		16

#define ID_TEXTUREDETAIL	19
#define ID_TEXTUREQUALITY	20
#define ID_TEXTUREFILTER	21

static const char *uiVideoYesNo[] = { "False", "True" };
static const char *uiVideoBits[] = { "Default", "16 bit", "32 bit" };
static const char *uiVideoDetail[] = { "Low", "Medium", "High", "Max" };
static const char *uiVideoTextureFilters[] = { "Bilinear", "Trilinear" };
static const char *uiVideoSettings[] = { "Custom", "Fastest", "Fast", "Normal", "High Quality" };
static const char *uiVideoModes[] =
{
	"640 x 480",
	"800 x 600",
	"960 x 720",
	"1024 x 768",
	"1152 x 864",
	"1280 x 800",
	"1280 x 960",
	"1280 x 1024",
	"1600 x 1200",
	"2048 x 1536",
	"856 x 480 (wide)",
	"1024 x 576 (wide)",
	"1440 x 900 (wide)",
	"1680 x 1050 (wide)",
	"1920 x 1200 (wide)"
	"2560 x 1600 (wide)",
};

typedef struct
{
	float		glExtensions;
	float		videoMode;
	float		fullScreen;
	float		displayFrequency;
	float		colorDepth;
	float		hardwareGamma;
	float		gamma;
	float		textureDetail;
	float		textureQuality;
	float		textureFilter;
} uiVideoValues_t;

static uiVideoValues_t uiVideoTemplates[4] =
{
{ 1, 3, 1, 0, 0, 1, 1.0, 0, 0, 0 }, // fastest
{ 1, 3, 1, 0, 1, 1, 1.0, 1, 0, 0 }, // fast
{ 1, 3, 1, 0, 2, 1, 1.0, 2, 1, 0 }, // normal
{ 1, 4, 1, 0, 2, 1, 1.0, 3, 2, 1 }, // high quality
};

static uiVideoValues_t	uiVideoInitial;

typedef struct
{
	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;
	menuBitmap_s	textShadow1;
	menuBitmap_s	textShadow2;
	menuBitmap_s	text1;
	menuBitmap_s	text2;

	menuBitmap_s	cancel;
	menuBitmap_s	advanced;
	menuBitmap_s	apply;

	menuSpinControl_s	graphicsSettings;
	menuSpinControl_s	glExtensions;
	menuSpinControl_s	videoMode;
	menuSpinControl_s	fullScreen;
	menuField_s      	displayFrequency;
	menuSpinControl_s	colorDepth;
	menuSpinControl_s	hardwareGamma;
	menuSpinControl_s	gamma;
	menuSpinControl_s	textureDetail;
	menuSpinControl_s	textureQuality;
	menuSpinControl_s	textureFilter;
} uiVideo_t;

static uiVideo_t		uiVideo;


/*
=================
UI_Video_GetConfig
=================
*/
static void UI_Video_GetConfig( void )
{
	if( Cvar_VariableInteger( "gl_extensions" ))
		uiVideo.glExtensions.curValue = 1;

	uiVideo.videoMode.curValue = Cvar_VariableInteger( "r_mode" );

	if( Cvar_VariableInteger( "fullscreen" ))
		uiVideo.fullScreen.curValue = 1;

	com.snprintf( uiVideo.displayFrequency.buffer, sizeof( uiVideo.displayFrequency.buffer ), "%i", Cvar_VariableInteger( "vid_displayfrequency" ));

	switch (Cvar_VariableInteger( "r_colorbits" ))
	{
	case 32:
		uiVideo.colorDepth.curValue = 2;
		break;
	case 16:
		uiVideo.colorDepth.curValue = 1;
		break;
	default:
		uiVideo.colorDepth.curValue = 0;
		break;
	}

	if( !Cvar_VariableInteger( "r_ignorehwgamma" ))
		uiVideo.hardwareGamma.curValue = 1;

	uiVideo.gamma.curValue = Cvar_VariableValue( "vid_gamma" );
	uiVideo.textureDetail.curValue = bound( 0, 3 - Cvar_VariableInteger( "r_picmip" ), 3 );

	switch( Cvar_VariableInteger( "r_texturebits" ))
	{
	case 32:
		uiVideo.textureQuality.curValue = 2;
		break;
	case 16:
		uiVideo.textureQuality.curValue = 1;
		break;
	default:
		uiVideo.textureQuality.curValue = 0;
		break;
	}

	if( !com.stricmp( Cvar_VariableString( "gl_texturemode" ), "GL_LINEAR_MIPMAP_LINEAR" ))
		uiVideo.textureFilter.curValue = 1;

	// save initial values
	uiVideoInitial.glExtensions = uiVideo.glExtensions.curValue;
	uiVideoInitial.videoMode = uiVideo.videoMode.curValue;
	uiVideoInitial.fullScreen = uiVideo.fullScreen.curValue;
	uiVideoInitial.displayFrequency = atoi(uiVideo.displayFrequency.buffer);
	uiVideoInitial.colorDepth = uiVideo.colorDepth.curValue;
	uiVideoInitial.hardwareGamma = uiVideo.hardwareGamma.curValue;
	uiVideoInitial.gamma = uiVideo.gamma.curValue;
	uiVideoInitial.textureDetail = uiVideo.textureDetail.curValue;
	uiVideoInitial.textureQuality = uiVideo.textureQuality.curValue;
	uiVideoInitial.textureFilter = uiVideo.textureFilter.curValue;
}

/*
=================
UI_Video_SetConfig
=================
*/
static void UI_Video_SetConfig( void )
{
	Cvar_SetValue( "gl_extensions", uiVideo.glExtensions.curValue );
	Cvar_SetValue( "r_mode", uiVideo.videoMode.curValue );
	Cvar_SetValue( "fullscreen", (int)uiVideo.fullScreen.curValue );
	Cvar_SetValue( "vid_displayfrequency", com.atoi( uiVideo.displayFrequency.buffer ));

	switch( (int)uiVideo.colorDepth.curValue )
	{
	case 2:
		Cvar_SetValue( "r_colorbits", 32 );
		Cvar_SetValue( "r_depthbits", 24 );
		Cvar_SetValue( "r_stencilbits", 8 );
		break;
	case 1:
		Cvar_SetValue( "r_colorbits", 16 );
		Cvar_SetValue( "r_depthbits", 16 );
		Cvar_SetValue( "r_stencilbits", 0 );
		break;
	case 0:
		Cvar_SetValue( "r_colorbits", 0 );
		Cvar_SetValue( "r_depthbits", 0 );
		Cvar_SetValue( "r_stencilbits", 0 );
		break;
	}

	Cvar_SetValue( "r_ignorehwgamma", !(int)uiVideo.hardwareGamma.curValue );
	Cvar_SetValue( "vid_gamma", uiVideo.gamma.curValue );
	
	switch( (int)uiVideo.textureDetail.curValue )
	{
	case 0:
		Cvar_SetValue( "gl_round_down", 1 );
		Cvar_SetValue( "r_picmip", 3 );
		break;
	case 1:
		Cvar_SetValue( "gl_round_down", 1 );
		Cvar_SetValue( "r_picmip", 2 );
		break;
	case 2:
		Cvar_SetValue( "gl_round_down", 0 );
		Cvar_SetValue( "r_picmip", 1 );
		break;
	case 3:
		Cvar_SetValue( "gl_round_down", 0 );
		Cvar_SetValue( "r_picmip", 0 );
		break;
	}

	switch( (int)uiVideo.textureQuality.curValue )
	{
	case 2:
		Cvar_SetValue( "r_texturebits", 32 );
		break;
	case 1:
		Cvar_SetValue( "r_texturebits", 16 );
		break;
	case 0:
		Cvar_SetValue( "r_texturebits", 0 );
		break;
	}

	if( (int)uiVideo.textureFilter.curValue == 1 )
		Cvar_Set( "gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR" );
	else Cvar_Set( "gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST" );

	// restart video subsystem
	Cbuf_ExecuteText( EXEC_NOW, "vid_restart\n" );
}

/*
=================
UI_Video_UpdateConfig
=================
*/
static void UI_Video_UpdateConfig( void )
{
	static char	gammaText[8];
	static char	intensityText[8];

	uiVideo.graphicsSettings.generic.name = uiVideoSettings[(int)uiVideo.graphicsSettings.curValue];

	if((int)uiVideo.graphicsSettings.curValue != 0 )
	{
		uiVideo.glExtensions.curValue = uiVideoTemplates[(int)uiVideo.graphicsSettings.curValue - 1].glExtensions;
		uiVideo.glExtensions.generic.flags |= QMF_GRAYED;
		uiVideo.videoMode.curValue = uiVideoTemplates[(int)uiVideo.graphicsSettings.curValue - 1].videoMode;
		uiVideo.videoMode.generic.flags |= QMF_GRAYED;
		uiVideo.fullScreen.curValue = uiVideoTemplates[(int)uiVideo.graphicsSettings.curValue - 1].fullScreen;
		uiVideo.fullScreen.generic.flags |= QMF_GRAYED;
		com.snprintf( uiVideo.displayFrequency.buffer, sizeof( uiVideo.displayFrequency.buffer ), "%i", (int)uiVideoTemplates[(int)uiVideo.graphicsSettings.curValue - 1].displayFrequency );
		uiVideo.displayFrequency.generic.flags |= QMF_GRAYED;
		uiVideo.colorDepth.curValue = uiVideoTemplates[(int)uiVideo.graphicsSettings.curValue - 1].colorDepth;
		uiVideo.colorDepth.generic.flags |= QMF_GRAYED;
		uiVideo.hardwareGamma.curValue = uiVideoTemplates[(int)uiVideo.graphicsSettings.curValue - 1].hardwareGamma;
		uiVideo.hardwareGamma.generic.flags |= QMF_GRAYED;
		uiVideo.gamma.curValue = uiVideoTemplates[(int)uiVideo.graphicsSettings.curValue - 1].gamma;
		uiVideo.gamma.generic.flags |= QMF_GRAYED;
		uiVideo.textureDetail.curValue = uiVideoTemplates[(int)uiVideo.graphicsSettings.curValue - 1].textureDetail;
		uiVideo.textureDetail.generic.flags |= QMF_GRAYED;
		uiVideo.textureQuality.curValue = uiVideoTemplates[(int)uiVideo.graphicsSettings.curValue - 1].textureQuality;
		uiVideo.textureQuality.generic.flags |= QMF_GRAYED;
		uiVideo.textureFilter.curValue = uiVideoTemplates[(int)uiVideo.graphicsSettings.curValue - 1].textureFilter;
		uiVideo.textureFilter.generic.flags |= QMF_GRAYED;
	}
	else
	{
		uiVideo.glExtensions.generic.flags &= ~QMF_GRAYED;
		uiVideo.videoMode.generic.flags &= ~QMF_GRAYED;
		uiVideo.fullScreen.generic.flags &= ~QMF_GRAYED;
		uiVideo.displayFrequency.generic.flags &= ~QMF_GRAYED;
		uiVideo.colorDepth.generic.flags &= ~QMF_GRAYED;
		uiVideo.hardwareGamma.generic.flags &= ~QMF_GRAYED;
		uiVideo.gamma.generic.flags &= ~QMF_GRAYED;
		uiVideo.textureDetail.generic.flags &= ~QMF_GRAYED;
		uiVideo.textureQuality.generic.flags &= ~QMF_GRAYED;
		uiVideo.textureFilter.generic.flags &= ~QMF_GRAYED;
	}

	uiVideo.glExtensions.generic.name = uiVideoYesNo[(int)uiVideo.glExtensions.curValue];
	uiVideo.videoMode.generic.name = uiVideoModes[(int)uiVideo.videoMode.curValue];
	uiVideo.fullScreen.generic.name = uiVideoYesNo[(int)uiVideo.fullScreen.curValue];
	uiVideo.colorDepth.generic.name = uiVideoBits[(int)uiVideo.colorDepth.curValue];
	uiVideo.hardwareGamma.generic.name = uiVideoYesNo[(int)uiVideo.hardwareGamma.curValue];
	com.snprintf( gammaText, sizeof( gammaText ), "%.2f", uiVideo.gamma.curValue );
	uiVideo.gamma.generic.name = gammaText;
	uiVideo.textureDetail.generic.name = uiVideoDetail[(int)uiVideo.textureDetail.curValue];
	uiVideo.textureQuality.generic.name = uiVideoBits[(int)uiVideo.textureQuality.curValue];
	uiVideo.textureFilter.generic.name = uiVideoTextureFilters[(int)uiVideo.textureFilter.curValue];

	// some settings can be updated here
	if( re->Support( R_HARDWARE_GAMMA_CONTROL ))
		Cvar_SetValue( "vid_gamma", uiVideo.gamma.curValue );

	if( (int)uiVideo.textureFilter.curValue == 1 )
		Cvar_Set( "gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR" );
	else Cvar_Set( "gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST" );

	// see if the apply button should be enabled or disabled
	uiVideo.apply.generic.flags |= QMF_GRAYED;

	if( uiVideoInitial.glExtensions != uiVideo.glExtensions.curValue )
	{
		uiVideo.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}

	if( uiVideoInitial.videoMode != uiVideo.videoMode.curValue )
	{
		uiVideo.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}

	if( uiVideoInitial.fullScreen != uiVideo.fullScreen.curValue )
	{
		uiVideo.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}

	if( uiVideoInitial.displayFrequency != atoi(uiVideo.displayFrequency.buffer ))
	{
		uiVideo.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}

	if( uiVideoInitial.colorDepth != uiVideo.colorDepth.curValue )
	{
		uiVideo.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}

	if( uiVideoInitial.hardwareGamma != uiVideo.hardwareGamma.curValue )
	{
		uiVideo.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}

	if( uiVideoInitial.gamma != uiVideo.gamma.curValue )
	{
		if( !re->Support( R_HARDWARE_GAMMA_CONTROL ))
		{
			uiVideo.apply.generic.flags &= ~QMF_GRAYED;
			return;
		}
	}

	if( uiVideoInitial.textureDetail != uiVideo.textureDetail.curValue )
	{
		uiVideo.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}

	if( uiVideoInitial.textureQuality != uiVideo.textureQuality.curValue )
	{
		uiVideo.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}
}

/*
=================
UI_Video_Callback
=================
*/
static void UI_Video_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event == QM_CHANGED )
	{
		UI_Video_UpdateConfig();
		return;
	}

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_CANCEL:
		UI_PopMenu();
		break;
	case ID_ADVANCED:
		UI_Advanced_Menu();
		break;
	case ID_APPLY:
		UI_Video_SetConfig();
		break;
	}
}

/*
=================
UI_Video_Ownerdraw
=================
*/
static void UI_Video_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( uiVideo.menu.items[uiVideo.menu.cursor] == self )
		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOXFOCUS );
	else UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOX );

	if( item->flags & QMF_GRAYED )
		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorDkGrey, ((menuBitmap_s *)self)->pic );
	else UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
}

/*
=================
UI_Video_Init
=================
*/
static void UI_Video_Init( void )
{
	Mem_Set( &uiVideo, 0, sizeof( uiVideo_t ));

	uiVideo.background.generic.id = ID_BACKGROUND;
	uiVideo.background.generic.type = QMTYPE_BITMAP;
	uiVideo.background.generic.flags = QMF_INACTIVE;
	uiVideo.background.generic.x = 0;
	uiVideo.background.generic.y = 0;
	uiVideo.background.generic.width = 1024;
	uiVideo.background.generic.height = 768;
	uiVideo.background.pic = ART_BACKGROUND;

	uiVideo.banner.generic.id = ID_BANNER;
	uiVideo.banner.generic.type = QMTYPE_BITMAP;
	uiVideo.banner.generic.flags = QMF_INACTIVE;
	uiVideo.banner.generic.x = 0;
	uiVideo.banner.generic.y = 66;
	uiVideo.banner.generic.width = 1024;
	uiVideo.banner.generic.height = 46;
	uiVideo.banner.pic = ART_BANNER;

	uiVideo.textShadow1.generic.id = ID_TEXTSHADOW1;
	uiVideo.textShadow1.generic.type = QMTYPE_BITMAP;
	uiVideo.textShadow1.generic.flags = QMF_INACTIVE;
	uiVideo.textShadow1.generic.x = 182;
	uiVideo.textShadow1.generic.y = 170;
	uiVideo.textShadow1.generic.width = 256;
	uiVideo.textShadow1.generic.height = 256;
	uiVideo.textShadow1.generic.color = uiColorBlack;
	uiVideo.textShadow1.pic = ART_TEXT1;

	uiVideo.textShadow2.generic.id = ID_TEXTSHADOW2;
	uiVideo.textShadow2.generic.type = QMTYPE_BITMAP;
	uiVideo.textShadow2.generic.flags = QMF_INACTIVE;
	uiVideo.textShadow2.generic.x = 182;
	uiVideo.textShadow2.generic.y = 426;
	uiVideo.textShadow2.generic.width = 256;
	uiVideo.textShadow2.generic.height = 256;
	uiVideo.textShadow2.generic.color = uiColorBlack;
	uiVideo.textShadow2.pic = ART_TEXT2;

	uiVideo.text1.generic.id = ID_TEXT1;
	uiVideo.text1.generic.type = QMTYPE_BITMAP;
	uiVideo.text1.generic.flags = QMF_INACTIVE;
	uiVideo.text1.generic.x = 180;
	uiVideo.text1.generic.y = 168;
	uiVideo.text1.generic.width = 256;
	uiVideo.text1.generic.height = 256;
	uiVideo.text1.pic = ART_TEXT1;

	uiVideo.text2.generic.id = ID_TEXT2;
	uiVideo.text2.generic.type = QMTYPE_BITMAP;
	uiVideo.text2.generic.flags = QMF_INACTIVE;
	uiVideo.text2.generic.x = 180;
	uiVideo.text2.generic.y = 424;
	uiVideo.text2.generic.width = 256;
	uiVideo.text2.generic.height = 256;
	uiVideo.text2.pic = ART_TEXT2;

	uiVideo.cancel.generic.id = ID_CANCEL;
	uiVideo.cancel.generic.type = QMTYPE_BITMAP;
	uiVideo.cancel.generic.x = 206;
	uiVideo.cancel.generic.y = 656;
	uiVideo.cancel.generic.width = 198;
	uiVideo.cancel.generic.height = 38;
	uiVideo.cancel.generic.callback = UI_Video_Callback;
	uiVideo.cancel.generic.ownerdraw = UI_Video_Ownerdraw;
	uiVideo.cancel.pic = UI_CANCELBUTTON;

	uiVideo.advanced.generic.id = ID_ADVANCED;
	uiVideo.advanced.generic.type = QMTYPE_BITMAP;
	uiVideo.advanced.generic.x = 413;
	uiVideo.advanced.generic.y = 656;
	uiVideo.advanced.generic.width = 198;
	uiVideo.advanced.generic.height = 38;
	uiVideo.advanced.generic.callback = UI_Video_Callback;
	uiVideo.advanced.generic.ownerdraw = UI_Video_Ownerdraw;
	uiVideo.advanced.pic = ART_ADVANCED;

	uiVideo.apply.generic.id = ID_APPLY;
	uiVideo.apply.generic.type = QMTYPE_BITMAP;
	uiVideo.apply.generic.x = 620;
	uiVideo.apply.generic.y = 656;
	uiVideo.apply.generic.width = 198;
	uiVideo.apply.generic.height = 38;
	uiVideo.apply.generic.callback = UI_Video_Callback;
	uiVideo.apply.generic.ownerdraw = UI_Video_Ownerdraw;
	uiVideo.apply.pic = UI_APPLYBUTTON;

	uiVideo.graphicsSettings.generic.id = ID_GRAPHICSSETTINGS;
	uiVideo.graphicsSettings.generic.type = QMTYPE_SPINCONTROL;
	uiVideo.graphicsSettings.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiVideo.graphicsSettings.generic.x = 580;
	uiVideo.graphicsSettings.generic.y = 160;
	uiVideo.graphicsSettings.generic.width = 198;
	uiVideo.graphicsSettings.generic.height = 30;
	uiVideo.graphicsSettings.generic.callback = UI_Video_Callback;
	uiVideo.graphicsSettings.generic.statusText = "Set a predefined graphics configuration";
	uiVideo.graphicsSettings.minValue = 0;
	uiVideo.graphicsSettings.maxValue = 4;
	uiVideo.graphicsSettings.range = 1;
	
	uiVideo.glExtensions.generic.id = ID_GLEXTENSIONS;
	uiVideo.glExtensions.generic.type = QMTYPE_SPINCONTROL;
	uiVideo.glExtensions.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiVideo.glExtensions.generic.x = 580;
	uiVideo.glExtensions.generic.y = 192;
	uiVideo.glExtensions.generic.width = 198;
	uiVideo.glExtensions.generic.height = 30;
	uiVideo.glExtensions.generic.callback = UI_Video_Callback;
	uiVideo.glExtensions.generic.statusText = "Enable/Disable graphic extensions on your card";
	uiVideo.glExtensions.minValue = 0;
	uiVideo.glExtensions.maxValue = 1;
	uiVideo.glExtensions.range = 1;

	uiVideo.videoMode.generic.id = ID_VIDEOMODE;
	uiVideo.videoMode.generic.type = QMTYPE_SPINCONTROL;
	uiVideo.videoMode.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiVideo.videoMode.generic.x = 580;
	uiVideo.videoMode.generic.y = 224;
	uiVideo.videoMode.generic.width = 198;
	uiVideo.videoMode.generic.height = 30;
	uiVideo.videoMode.generic.callback = UI_Video_Callback;
	uiVideo.videoMode.generic.statusText = "Set your game resolution";
	uiVideo.videoMode.minValue = -1;
	uiVideo.videoMode.maxValue = 12;
	uiVideo.videoMode.range = 1;
	
	uiVideo.fullScreen.generic.id = ID_FULLSCREEN;
	uiVideo.fullScreen.generic.type = QMTYPE_SPINCONTROL;
	uiVideo.fullScreen.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiVideo.fullScreen.generic.x = 580;
	uiVideo.fullScreen.generic.y = 256;
	uiVideo.fullScreen.generic.width = 198;
	uiVideo.fullScreen.generic.height = 30;
	uiVideo.fullScreen.generic.callback = UI_Video_Callback;
	uiVideo.fullScreen.generic.statusText = "Switch between fullscreen and windowed mode";
	uiVideo.fullScreen.minValue = 0;
	uiVideo.fullScreen.maxValue = 1;
	uiVideo.fullScreen.range = 1;

	uiVideo.displayFrequency.generic.id = ID_DISPLAYFREQUENCY;
	uiVideo.displayFrequency.generic.type = QMTYPE_FIELD;
	uiVideo.displayFrequency.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW | QMF_NUMBERSONLY;
	uiVideo.displayFrequency.generic.x = 580;
	uiVideo.displayFrequency.generic.y = 288;
	uiVideo.displayFrequency.generic.width = 198;
	uiVideo.displayFrequency.generic.height = 30;
	uiVideo.displayFrequency.generic.callback = UI_Video_Callback;
	uiVideo.displayFrequency.generic.statusText = "Set your monitor refresh rate (0 = Disabled)";
	uiVideo.displayFrequency.maxLenght = 3;

	uiVideo.colorDepth.generic.id = ID_COLORDEPTH;
	uiVideo.colorDepth.generic.type = QMTYPE_SPINCONTROL;
	uiVideo.colorDepth.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiVideo.colorDepth.generic.x = 580;
	uiVideo.colorDepth.generic.y = 320;
	uiVideo.colorDepth.generic.width = 198;
	uiVideo.colorDepth.generic.height = 30;
	uiVideo.colorDepth.generic.callback = UI_Video_Callback;
	uiVideo.colorDepth.generic.statusText = "Set your game color depth (32 bit = Better quality)";
	uiVideo.colorDepth.minValue = 0;
	uiVideo.colorDepth.maxValue = 2;
	uiVideo.colorDepth.range = 1;

	uiVideo.hardwareGamma.generic.id = ID_HARDWAREGAMMA;
	uiVideo.hardwareGamma.generic.type = QMTYPE_SPINCONTROL;
	uiVideo.hardwareGamma.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiVideo.hardwareGamma.generic.x = 580;
	uiVideo.hardwareGamma.generic.y = 352;
	uiVideo.hardwareGamma.generic.width = 198;
	uiVideo.hardwareGamma.generic.height = 30;
	uiVideo.hardwareGamma.generic.callback = UI_Video_Callback;
	uiVideo.hardwareGamma.generic.statusText = "Set the display device gamma ramp";
	uiVideo.hardwareGamma.minValue = 0;
	uiVideo.hardwareGamma.maxValue = 1;
	uiVideo.hardwareGamma.range = 1;

	uiVideo.gamma.generic.id = ID_GAMMA;
	uiVideo.gamma.generic.type = QMTYPE_SPINCONTROL;
	uiVideo.gamma.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiVideo.gamma.generic.x = 580;
	uiVideo.gamma.generic.y = 384;
	uiVideo.gamma.generic.width = 198;
	uiVideo.gamma.generic.height = 30;
	uiVideo.gamma.generic.callback = UI_Video_Callback;
	uiVideo.gamma.generic.statusText = "Set display device gamma level";
	uiVideo.gamma.minValue = 0.5;
	uiVideo.gamma.maxValue = 3.0;
	uiVideo.gamma.range = 0.25;
	
	uiVideo.textureDetail.generic.id = ID_TEXTUREDETAIL;
	uiVideo.textureDetail.generic.type = QMTYPE_SPINCONTROL;
	uiVideo.textureDetail.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiVideo.textureDetail.generic.x = 580;
	uiVideo.textureDetail.generic.y = 480;
	uiVideo.textureDetail.generic.width = 198;
	uiVideo.textureDetail.generic.height = 30;
	uiVideo.textureDetail.generic.callback = UI_Video_Callback;
	uiVideo.textureDetail.generic.statusText = "Set texture detail level (Max = Better quality)";
	uiVideo.textureDetail.minValue = 0;
	uiVideo.textureDetail.maxValue = 3;
	uiVideo.textureDetail.range = 1;

	uiVideo.textureQuality.generic.id = ID_TEXTUREQUALITY;
	uiVideo.textureQuality.generic.type = QMTYPE_SPINCONTROL;
	uiVideo.textureQuality.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiVideo.textureQuality.generic.x = 580;
	uiVideo.textureQuality.generic.y = 512;
	uiVideo.textureQuality.generic.width = 198;
	uiVideo.textureQuality.generic.height = 30;
	uiVideo.textureQuality.generic.callback = UI_Video_Callback;
	uiVideo.textureQuality.generic.statusText = "Set texture color depth (32 bit = Better quality)";
	uiVideo.textureQuality.minValue = 0;
	uiVideo.textureQuality.maxValue = 2;
	uiVideo.textureQuality.range = 1;

	uiVideo.textureFilter.generic.id = ID_TEXTUREFILTER;
	uiVideo.textureFilter.generic.type = QMTYPE_SPINCONTROL;
	uiVideo.textureFilter.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiVideo.textureFilter.generic.x = 580;
	uiVideo.textureFilter.generic.y = 544;
	uiVideo.textureFilter.generic.width = 198;
	uiVideo.textureFilter.generic.height = 30;
	uiVideo.textureFilter.generic.callback = UI_Video_Callback;
	uiVideo.textureFilter.generic.statusText = "Set texture filter (Trilinear = Better quality)";
	uiVideo.textureFilter.minValue = 0;
	uiVideo.textureFilter.maxValue = 1;
	uiVideo.textureFilter.range = 1;

	UI_Video_GetConfig();

	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.background );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.banner );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.textShadow1 );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.textShadow2 );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.text1 );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.text2 );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.cancel );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.advanced );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.apply );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.graphicsSettings );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.glExtensions );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.videoMode );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.fullScreen );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.displayFrequency );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.colorDepth );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.hardwareGamma );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.gamma );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.textureDetail );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.textureQuality );
	UI_AddItem( &uiVideo.menu, (void *)&uiVideo.textureFilter );
}

/*
=================
UI_Video_Precache
=================
*/
void UI_Video_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT1, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT2, SHADER_NOMIP );
	re->RegisterShader( ART_ADVANCED, SHADER_NOMIP );
}

/*
=================
UI_Video_Menu
=================
*/
void UI_Video_Menu( void )
{
	UI_Video_Precache();
	UI_Video_Init();

	UI_Video_UpdateConfig();
	UI_PushMenu( &uiVideo.menu );
}