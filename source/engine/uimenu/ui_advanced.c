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
#define ART_BANNER			"gfx/shell/banners/advanced_t"
#define ART_TEXT1			"gfx/shell/text/advanced_text_p1"
#define ART_TEXT2			"gfx/shell/text/advanced_text_p2"

#define ID_BACKGROUND		0
#define ID_BANNER			1

#define ID_TEXT1			2
#define ID_TEXT2			3
#define ID_TEXTSHADOW1		4
#define ID_TEXTSHADOW2		5

#define ID_CANCEL			6
#define ID_APPLY			7

#define ID_MULTITEXTURE		8
#define ID_VBO			9
#define ID_TEXENVCOMBINE		10
#define ID_TEXENVADD		11
#define ID_TEXENVDOT3		12
#define ID_VERTEXPROGRAM		13
#define ID_FRAGMENTPROGRAM		14
#define ID_CUBEMAPTEXTURE		15
#define ID_RECTANGLETEXTURE		16
#define ID_TEXTURECOMPRESS		17
#define ID_ANISOTROPIC		18
#define ID_MAXANISOTROPY		19

static const char *uiAdvancedYesNo[] = { "False", "True" };

typedef struct
{
	float	multitexture;
	float	vbo;
	float	texEnvCombine;
	float	texEnvAdd;
	float	texEnvDOT3;
	float	vertexProgram;
	float	fragmentProgram;
	float	cubeMapTexture;
	float	rectangleTexture;
	float	textureCompress;
	float	anisotropic;
	float	maxAnisotropy;

} uiAdvancedValues_t;

static uiAdvancedValues_t	uiAdvancedInitial;

typedef struct
{
	menuFramework_s		menu;

	menuBitmap_s		background;
	menuBitmap_s		banner;

	menuBitmap_s		textShadow1;
	menuBitmap_s		textShadow2;
	menuBitmap_s		text1;
	menuBitmap_s		text2;

	menuBitmap_s		cancel;
	menuBitmap_s		apply;

	menuSpinControl_s		multitexture;
	menuSpinControl_s		vbo;
	menuSpinControl_s		texEnvCombine;
	menuSpinControl_s		texEnvAdd;
	menuSpinControl_s		texEnvDOT3;
	menuSpinControl_s		vertexProgram;
	menuSpinControl_s		fragmentProgram;
	menuSpinControl_s		cubeMapTexture;
	menuSpinControl_s		rectangleTexture;
	menuSpinControl_s		textureCompress;
	menuSpinControl_s		anisotropic;
	menuSpinControl_s		maxAnisotropy;
} uiAdvanced_t;

static uiAdvanced_t			uiAdvanced;


/*
=================
UI_Advanced_GetConfig
=================
*/
static void UI_Advanced_GetConfig( void )
{
	if( Cvar_VariableInteger( "r_arb_multitexture" ))
		uiAdvanced.multitexture.curValue = 1;

	if( Cvar_VariableInteger( "r_arb_vertex_buffer_object" ))
		uiAdvanced.vbo.curValue = 1;

	if( Cvar_VariableInteger( "r_arb_texture_env_combine" ))
		uiAdvanced.texEnvCombine.curValue = 1;

	if( Cvar_VariableInteger( "r_arb_texture_env_add" ))
		uiAdvanced.texEnvAdd.curValue = 1;

	if( Cvar_VariableInteger( "r_arb_texture_env_dot3" ))
		uiAdvanced.texEnvDOT3.curValue = 1;

	if( Cvar_VariableInteger( "r_arb_vertex_program" ))
		uiAdvanced.vertexProgram.curValue = 1;

	if( Cvar_VariableInteger( "r_arb_fragment_program" ))
		uiAdvanced.fragmentProgram.curValue = 1;

	if (Cvar_VariableInteger( "r_arb_texture_cube_map" ))
		uiAdvanced.cubeMapTexture.curValue = 1;

	if( Cvar_VariableInteger( "r_ext_texture_rectangle" ))
		uiAdvanced.rectangleTexture.curValue = 1;

	if( Cvar_VariableInteger( "r_arb_texture_compression" ))
		uiAdvanced.textureCompress.curValue = 1;

	if( Cvar_VariableInteger( "r_ext_texture_filter_anisotropic" ))
		uiAdvanced.anisotropic.curValue = 1;

	uiAdvanced.maxAnisotropy.curValue = Cvar_VariableValue( "r_textureFilterAnisotropy" );

	// save initial values
	uiAdvancedInitial.multitexture = uiAdvanced.multitexture.curValue;
	uiAdvancedInitial.vbo = uiAdvanced.vbo.curValue;
	uiAdvancedInitial.texEnvCombine = uiAdvanced.texEnvCombine.curValue;
	uiAdvancedInitial.texEnvAdd = uiAdvanced.texEnvAdd.curValue;
	uiAdvancedInitial.texEnvDOT3 = uiAdvanced.texEnvDOT3.curValue;
	uiAdvancedInitial.vertexProgram = uiAdvanced.vertexProgram.curValue;
	uiAdvancedInitial.fragmentProgram = uiAdvanced.fragmentProgram.curValue;
	uiAdvancedInitial.cubeMapTexture = uiAdvanced.cubeMapTexture.curValue;
	uiAdvancedInitial.rectangleTexture = uiAdvanced.rectangleTexture.curValue;
	uiAdvancedInitial.textureCompress = uiAdvanced.textureCompress.curValue;
	uiAdvancedInitial.anisotropic = uiAdvanced.anisotropic.curValue;
	uiAdvancedInitial.maxAnisotropy = uiAdvanced.maxAnisotropy.curValue;
}

/*
=================
UI_Advanced_SetConfig
=================
*/
static void UI_Advanced_SetConfig( void )
{
	Cvar_SetValue( "r_arb_multitexture", uiAdvanced.multitexture.curValue );
	Cvar_SetValue( "r_arb_vertex_buffer_object", uiAdvanced.vbo.curValue );
	Cvar_SetValue( "r_arb_texture_env_combine", uiAdvanced.texEnvCombine.curValue );
	Cvar_SetValue( "r_arb_texture_env_add", uiAdvanced.texEnvAdd.curValue );
	Cvar_SetValue( "r_arb_texture_env_dot3", uiAdvanced.texEnvDOT3.curValue );
	Cvar_SetValue( "r_arb_vertex_program", uiAdvanced.vertexProgram.curValue );
	Cvar_SetValue( "r_arb_fragment_program", uiAdvanced.fragmentProgram.curValue );
	Cvar_SetValue( "r_arb_texture_cube_map", uiAdvanced.cubeMapTexture.curValue );
	Cvar_SetValue( "r_ext_texture_rectangle", uiAdvanced.rectangleTexture.curValue );
	Cvar_SetValue( "r_arb_texture_compression", uiAdvanced.textureCompress.curValue );
	Cvar_SetValue( "r_ext_texture_filter_anisotropic", uiAdvanced.anisotropic.curValue );
	Cvar_SetValue( "r_textureFilterAnisotropy", uiAdvanced.maxAnisotropy.curValue );

	// restart video subsystem
	Cbuf_ExecuteText( EXEC_NOW, "vid_restart\n" );
}

/*
=================
UI_Advanced_UpdateConfig
=================
*/
static void UI_Advanced_UpdateConfig( void )
{
	static char	anisotropyText[8];

	uiAdvanced.multitexture.generic.name = uiAdvancedYesNo[(int)uiAdvanced.multitexture.curValue];
	uiAdvanced.vbo.generic.name = uiAdvancedYesNo[(int)uiAdvanced.vbo.curValue];
	uiAdvanced.texEnvCombine.generic.name = uiAdvancedYesNo[(int)uiAdvanced.texEnvCombine.curValue];
	uiAdvanced.texEnvAdd.generic.name = uiAdvancedYesNo[(int)uiAdvanced.texEnvAdd.curValue];
	uiAdvanced.texEnvDOT3.generic.name = uiAdvancedYesNo[(int)uiAdvanced.texEnvDOT3.curValue];
	uiAdvanced.vertexProgram.generic.name = uiAdvancedYesNo[(int)uiAdvanced.vertexProgram.curValue];
	uiAdvanced.fragmentProgram.generic.name = uiAdvancedYesNo[(int)uiAdvanced.fragmentProgram.curValue];
	uiAdvanced.cubeMapTexture.generic.name = uiAdvancedYesNo[(int)uiAdvanced.cubeMapTexture.curValue];
	uiAdvanced.rectangleTexture.generic.name = uiAdvancedYesNo[(int)uiAdvanced.rectangleTexture.curValue];
	uiAdvanced.textureCompress.generic.name = uiAdvancedYesNo[(int)uiAdvanced.textureCompress.curValue];
	uiAdvanced.anisotropic.generic.name = uiAdvancedYesNo[(int)uiAdvanced.anisotropic.curValue];

	if( !re->Support( R_ANISOTROPY_EXT )) uiAdvanced.maxAnisotropy.curValue = 2.0;
	com.snprintf( anisotropyText, sizeof( anisotropyText ), "%.2f", uiAdvanced.maxAnisotropy.curValue );
	uiAdvanced.maxAnisotropy.generic.name = anisotropyText;

	// Some settings can be updated here
	Cvar_SetValue( "r_textureFilterAnisotropy", uiAdvanced.maxAnisotropy.curValue );

	// See if the apply button should be enabled or disabled
	uiAdvanced.apply.generic.flags |= QMF_GRAYED;

	if( uiAdvancedInitial.multitexture != uiAdvanced.multitexture.curValue )
	{
		uiAdvanced.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}
	if( uiAdvancedInitial.vbo != uiAdvanced.vbo.curValue )
	{
		uiAdvanced.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}
	if( uiAdvancedInitial.texEnvCombine != uiAdvanced.texEnvCombine.curValue )
	{
		uiAdvanced.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}
	if( uiAdvancedInitial.texEnvAdd != uiAdvanced.texEnvAdd.curValue )
	{
		uiAdvanced.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}
	if( uiAdvancedInitial.texEnvDOT3 != uiAdvanced.texEnvDOT3.curValue )
	{
		uiAdvanced.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}
	if( uiAdvancedInitial.vertexProgram != uiAdvanced.vertexProgram.curValue){
		uiAdvanced.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}
	if( uiAdvancedInitial.fragmentProgram != uiAdvanced.fragmentProgram.curValue )
	{
		uiAdvanced.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}
	if( uiAdvancedInitial.cubeMapTexture != uiAdvanced.cubeMapTexture.curValue )
	{
		uiAdvanced.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}
	if( uiAdvancedInitial.rectangleTexture != uiAdvanced.rectangleTexture.curValue )
	{
		uiAdvanced.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}
	if( uiAdvancedInitial.textureCompress != uiAdvanced.textureCompress.curValue )
	{
		uiAdvanced.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}
	if( uiAdvancedInitial.anisotropic != uiAdvanced.anisotropic.curValue )
	{
		uiAdvanced.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}
}

/*
=================
UI_Advanced_Callback
=================
*/
static void UI_Advanced_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event == QM_CHANGED )
	{
		UI_Advanced_UpdateConfig();
		return;
	}

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_CANCEL:
		UI_PopMenu();
		break;
	case ID_APPLY:
		UI_Advanced_SetConfig();
		break;
	}
}

/*
=================
UI_Advanced_Ownerdraw
=================
*/
static void UI_Advanced_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( uiAdvanced.menu.items[uiAdvanced.menu.cursor] == self )
		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOXFOCUS );
	else UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOX );

	if( item->flags & QMF_GRAYED )
		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorDkGrey, ((menuBitmap_s *)self)->pic );
	else UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
}

/*
=================
UI_Advanced_Init
=================
*/
static void UI_Advanced_Init( void )
{

	Mem_Set( &uiAdvanced, 0, sizeof( uiAdvanced_t ));

	uiAdvanced.background.generic.id = ID_BACKGROUND;
	uiAdvanced.background.generic.type = QMTYPE_BITMAP;
	uiAdvanced.background.generic.flags = QMF_INACTIVE;
	uiAdvanced.background.generic.x = 0;
	uiAdvanced.background.generic.y = 0;
	uiAdvanced.background.generic.width = 1024;
	uiAdvanced.background.generic.height = 768;
	uiAdvanced.background.pic = ART_BACKGROUND;

	uiAdvanced.banner.generic.id = ID_BANNER;
	uiAdvanced.banner.generic.type = QMTYPE_BITMAP;
	uiAdvanced.banner.generic.flags = QMF_INACTIVE;
	uiAdvanced.banner.generic.x = 0;
	uiAdvanced.banner.generic.y = 66;
	uiAdvanced.banner.generic.width = 1024;
	uiAdvanced.banner.generic.height = 46;
	uiAdvanced.banner.pic = ART_BANNER;

	uiAdvanced.textShadow1.generic.id = ID_TEXTSHADOW1;
	uiAdvanced.textShadow1.generic.type = QMTYPE_BITMAP;
	uiAdvanced.textShadow1.generic.flags = QMF_INACTIVE;
	uiAdvanced.textShadow1.generic.x = 182;
	uiAdvanced.textShadow1.generic.y = 170;
	uiAdvanced.textShadow1.generic.width = 256;
	uiAdvanced.textShadow1.generic.height = 256;
	uiAdvanced.textShadow1.generic.color = uiColorBlack;
	uiAdvanced.textShadow1.pic = ART_TEXT1;

	uiAdvanced.textShadow2.generic.id = ID_TEXTSHADOW2;
	uiAdvanced.textShadow2.generic.type = QMTYPE_BITMAP;
	uiAdvanced.textShadow2.generic.flags = QMF_INACTIVE;
	uiAdvanced.textShadow2.generic.x = 182;
	uiAdvanced.textShadow2.generic.y = 426;
	uiAdvanced.textShadow2.generic.width = 256;
	uiAdvanced.textShadow2.generic.height = 256;
	uiAdvanced.textShadow2.generic.color = uiColorBlack;
	uiAdvanced.textShadow2.pic = ART_TEXT2;

	uiAdvanced.text1.generic.id = ID_TEXT1;
	uiAdvanced.text1.generic.type = QMTYPE_BITMAP;
	uiAdvanced.text1.generic.flags = QMF_INACTIVE;
	uiAdvanced.text1.generic.x = 180;
	uiAdvanced.text1.generic.y = 168;
	uiAdvanced.text1.generic.width = 256;
	uiAdvanced.text1.generic.height = 256;
	uiAdvanced.text1.pic = ART_TEXT1;

	uiAdvanced.text2.generic.id = ID_TEXT2;
	uiAdvanced.text2.generic.type	= QMTYPE_BITMAP;
	uiAdvanced.text2.generic.flags = QMF_INACTIVE;
	uiAdvanced.text2.generic.x = 180;
	uiAdvanced.text2.generic.y = 424;
	uiAdvanced.text2.generic.width = 256;
	uiAdvanced.text2.generic.height = 256;
	uiAdvanced.text2.pic = ART_TEXT2;

	uiAdvanced.cancel.generic.id = ID_CANCEL;
	uiAdvanced.cancel.generic.type = QMTYPE_BITMAP;
	uiAdvanced.cancel.generic.x = 310;
	uiAdvanced.cancel.generic.y = 656;
	uiAdvanced.cancel.generic.width = 198;
	uiAdvanced.cancel.generic.height = 38;
	uiAdvanced.cancel.generic.callback = UI_Advanced_Callback;
	uiAdvanced.cancel.generic.ownerdraw = UI_Advanced_Ownerdraw;
	uiAdvanced.cancel.pic = UI_CANCELBUTTON;

	uiAdvanced.apply.generic.id = ID_APPLY;
	uiAdvanced.apply.generic.type	= QMTYPE_BITMAP;
	uiAdvanced.apply.generic.x = 516;
	uiAdvanced.apply.generic.y = 656;
	uiAdvanced.apply.generic.width = 198;
	uiAdvanced.apply.generic.height = 38;
	uiAdvanced.apply.generic.callback = UI_Advanced_Callback;
	uiAdvanced.apply.generic.ownerdraw = UI_Advanced_Ownerdraw;
	uiAdvanced.apply.pic = UI_APPLYBUTTON;

	uiAdvanced.multitexture.generic.id = ID_MULTITEXTURE;
	uiAdvanced.multitexture.generic.type = QMTYPE_SPINCONTROL;
	uiAdvanced.multitexture.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAdvanced.multitexture.generic.x = 580;
	uiAdvanced.multitexture.generic.y = 160;
	uiAdvanced.multitexture.generic.width = 198;
	uiAdvanced.multitexture.generic.height = 30;
	uiAdvanced.multitexture.generic.callback = UI_Advanced_Callback;
	uiAdvanced.multitexture.generic.statusText = "Allow multitexturing to save rendering passes";
	uiAdvanced.multitexture.minValue = 0;
	uiAdvanced.multitexture.maxValue = 1;
	uiAdvanced.multitexture.range	= 1;

	uiAdvanced.vbo.generic.id = ID_VBO;
	uiAdvanced.vbo.generic.type = QMTYPE_SPINCONTROL;
	uiAdvanced.vbo.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAdvanced.vbo.generic.x = 580;
	uiAdvanced.vbo.generic.y = 192;
	uiAdvanced.vbo.generic.width = 198;
	uiAdvanced.vbo.generic.height = 30;
	uiAdvanced.vbo.generic.callback = UI_Advanced_Callback;
	uiAdvanced.vbo.generic.statusText = "Allow geometry to be stored in fast video or AGP memory";
	uiAdvanced.vbo.minValue = 0;
	uiAdvanced.vbo.maxValue = 1;
	uiAdvanced.vbo.range = 1;

	uiAdvanced.texEnvCombine.generic.id = ID_TEXENVCOMBINE;
	uiAdvanced.texEnvCombine.generic.type = QMTYPE_SPINCONTROL;
	uiAdvanced.texEnvCombine.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAdvanced.texEnvCombine.generic.x = 580;
	uiAdvanced.texEnvCombine.generic.y = 224;
	uiAdvanced.texEnvCombine.generic.width = 198;
	uiAdvanced.texEnvCombine.generic.height = 30;
	uiAdvanced.texEnvCombine.generic.callback = UI_Advanced_Callback;
	uiAdvanced.texEnvCombine.generic.statusText = "Allow advanced texture blending in shader effects";
	uiAdvanced.texEnvCombine.minValue = 0;
	uiAdvanced.texEnvCombine.maxValue = 1;
	uiAdvanced.texEnvCombine.range = 1;

	uiAdvanced.texEnvAdd.generic.id = ID_TEXENVADD;
	uiAdvanced.texEnvAdd.generic.type = QMTYPE_SPINCONTROL;
	uiAdvanced.texEnvAdd.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAdvanced.texEnvAdd.generic.x = 580;
	uiAdvanced.texEnvAdd.generic.y = 256;
	uiAdvanced.texEnvAdd.generic.width = 198;
	uiAdvanced.texEnvAdd.generic.height = 30;
	uiAdvanced.texEnvAdd.generic.callback = UI_Advanced_Callback;
	uiAdvanced.texEnvAdd.generic.statusText = "Allow additive texture blending to save rendering passes";
	uiAdvanced.texEnvAdd.minValue = 0;
	uiAdvanced.texEnvAdd.maxValue	= 1;
	uiAdvanced.texEnvAdd.range = 1;

	uiAdvanced.texEnvDOT3.generic.id = ID_TEXENVDOT3;
	uiAdvanced.texEnvDOT3.generic.type = QMTYPE_SPINCONTROL;
	uiAdvanced.texEnvDOT3.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAdvanced.texEnvDOT3.generic.x = 580;
	uiAdvanced.texEnvDOT3.generic.y = 288;
	uiAdvanced.texEnvDOT3.generic.width = 198;
	uiAdvanced.texEnvDOT3.generic.height = 30;
	uiAdvanced.texEnvDOT3.generic.callback = UI_Advanced_Callback;
	uiAdvanced.texEnvDOT3.generic.statusText = "Allow DOT3 texture blending in shader effects";
	uiAdvanced.texEnvDOT3.minValue = 0;
	uiAdvanced.texEnvDOT3.maxValue = 1;
	uiAdvanced.texEnvDOT3.range = 1;

	uiAdvanced.vertexProgram.generic.id = ID_VERTEXPROGRAM;
	uiAdvanced.vertexProgram.generic.type = QMTYPE_SPINCONTROL;
	uiAdvanced.vertexProgram.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAdvanced.vertexProgram.generic.x = 580;
	uiAdvanced.vertexProgram.generic.y = 320;
	uiAdvanced.vertexProgram.generic.width = 198;
	uiAdvanced.vertexProgram.generic.height = 30;
	uiAdvanced.vertexProgram.generic.callback = UI_Advanced_Callback;
	uiAdvanced.vertexProgram.generic.statusText = "Allow vertex programs in shader effects";
	uiAdvanced.vertexProgram.minValue = 0;
	uiAdvanced.vertexProgram.maxValue = 1;
	uiAdvanced.vertexProgram.range = 1;

	uiAdvanced.fragmentProgram.generic.id = ID_FRAGMENTPROGRAM;
	uiAdvanced.fragmentProgram.generic.type = QMTYPE_SPINCONTROL;
	uiAdvanced.fragmentProgram.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAdvanced.fragmentProgram.generic.x = 580;
	uiAdvanced.fragmentProgram.generic.y = 352;
	uiAdvanced.fragmentProgram.generic.width = 198;
	uiAdvanced.fragmentProgram.generic.height = 30;
	uiAdvanced.fragmentProgram.generic.callback = UI_Advanced_Callback;
	uiAdvanced.fragmentProgram.generic.statusText = "Allow fragment programs in shader effects";
	uiAdvanced.fragmentProgram.minValue = 0;
	uiAdvanced.fragmentProgram.maxValue = 1;
	uiAdvanced.fragmentProgram.range = 1;

	uiAdvanced.cubeMapTexture.generic.id = ID_CUBEMAPTEXTURE;
	uiAdvanced.cubeMapTexture.generic.type = QMTYPE_SPINCONTROL;
	uiAdvanced.cubeMapTexture.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAdvanced.cubeMapTexture.generic.x = 580;
	uiAdvanced.cubeMapTexture.generic.y = 384;
	uiAdvanced.cubeMapTexture.generic.width = 198;
	uiAdvanced.cubeMapTexture.generic.height = 30;
	uiAdvanced.cubeMapTexture.generic.callback = UI_Advanced_Callback;
	uiAdvanced.cubeMapTexture.generic.statusText = "Allow cube map textures in shader effects";
	uiAdvanced.cubeMapTexture.minValue = 0;
	uiAdvanced.cubeMapTexture.maxValue = 1;
	uiAdvanced.cubeMapTexture.range = 1;

	uiAdvanced.rectangleTexture.generic.id = ID_RECTANGLETEXTURE;
	uiAdvanced.rectangleTexture.generic.type = QMTYPE_SPINCONTROL;
	uiAdvanced.rectangleTexture.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAdvanced.rectangleTexture.generic.x = 580;
	uiAdvanced.rectangleTexture.generic.y = 416;
	uiAdvanced.rectangleTexture.generic.width = 198;
	uiAdvanced.rectangleTexture.generic.height = 30;
	uiAdvanced.rectangleTexture.generic.callback = UI_Advanced_Callback;
	uiAdvanced.rectangleTexture.generic.statusText = "Allow rectangle textures for motion blur";
	uiAdvanced.rectangleTexture.minValue = 0;
	uiAdvanced.rectangleTexture.maxValue = 1;
	uiAdvanced.rectangleTexture.range = 1;

	uiAdvanced.textureCompress.generic.id = ID_TEXTURECOMPRESS;
	uiAdvanced.textureCompress.generic.type = QMTYPE_SPINCONTROL;
	uiAdvanced.textureCompress.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAdvanced.textureCompress.generic.x = 580;
	uiAdvanced.textureCompress.generic.y = 448;
	uiAdvanced.textureCompress.generic.width = 198;
	uiAdvanced.textureCompress.generic.height = 30;
	uiAdvanced.textureCompress.generic.callback = UI_Advanced_Callback;
	uiAdvanced.textureCompress.generic.statusText = "Allow texture compression to save texture memory";
	uiAdvanced.textureCompress.minValue = 0;
	uiAdvanced.textureCompress.maxValue = 1;
	uiAdvanced.textureCompress.range = 1;

	uiAdvanced.anisotropic.generic.id = ID_ANISOTROPIC;
	uiAdvanced.anisotropic.generic.type = QMTYPE_SPINCONTROL;
	uiAdvanced.anisotropic.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAdvanced.anisotropic.generic.x = 580;
	uiAdvanced.anisotropic.generic.y = 480;
	uiAdvanced.anisotropic.generic.width = 198;
	uiAdvanced.anisotropic.generic.height = 30;
	uiAdvanced.anisotropic.generic.callback = UI_Advanced_Callback;
	uiAdvanced.anisotropic.generic.statusText = "Allow anisotropic texture filtering";
	uiAdvanced.anisotropic.minValue = 0;
	uiAdvanced.anisotropic.maxValue = 1;
	uiAdvanced.anisotropic.range = 1;

	uiAdvanced.maxAnisotropy.generic.id = ID_MAXANISOTROPY;
	uiAdvanced.maxAnisotropy.generic.type = QMTYPE_SPINCONTROL;
	uiAdvanced.maxAnisotropy.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAdvanced.maxAnisotropy.generic.x = 580;
	uiAdvanced.maxAnisotropy.generic.y = 512;
	uiAdvanced.maxAnisotropy.generic.width = 198;
	uiAdvanced.maxAnisotropy.generic.height = 30;
	uiAdvanced.maxAnisotropy.generic.callback = UI_Advanced_Callback;
	uiAdvanced.maxAnisotropy.generic.statusText = "Set anisotropy filter level (Higher values = Better quality)";
	uiAdvanced.maxAnisotropy.minValue = 1.0;

// FIXME: get maximum anisotropy level from render
//	uiAdvanced.maxAnisotropy.maxValue = uiStatic.glConfig.maxTextureMaxAnisotropy;
	uiAdvanced.maxAnisotropy.range = 1.0;

	UI_Advanced_GetConfig();

	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.background );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.banner );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.textShadow1 );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.textShadow2 );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.text1 );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.text2 );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.cancel );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.apply );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.multitexture );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.vbo );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.texEnvCombine );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.texEnvAdd );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.texEnvDOT3 );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.vertexProgram );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.fragmentProgram );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.cubeMapTexture );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.rectangleTexture );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.textureCompress );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.anisotropic );
	UI_AddItem( &uiAdvanced.menu, (void *)&uiAdvanced.maxAnisotropy );
}

/*
=================
UI_Advanced_Precache
=================
*/
void UI_Advanced_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT1, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT2, SHADER_NOMIP );
}

/*
=================
UI_Advanced_Menu
=================
*/
void UI_Advanced_Menu( void )
{
	UI_Advanced_Precache();
	UI_Advanced_Init();

	UI_Advanced_UpdateConfig();
	UI_PushMenu( &uiAdvanced.menu );
}