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


#define ART_BANNER			"gfx/shell/head_audio"

#define ID_BACKGROUND		0
#define ID_BANNER			1
#define ID_DONE			2
#define ID_APPLY			3
#define ID_SOUNDLIBRARY		4
#define ID_SOUNDVOLUME		5
#define ID_MUSICVOLUME		6
#define ID_EAX			7
#define ID_A3D			8
#define ID_MSGHINT			9

#define MAX_AUDIODEVICES		MAX_RENDERS
static const char			*uiAudioYesNo[] = { "False", "True" };

typedef struct
{
	int		soundLibrary;
	float		soundVolume;
	float		musicVolume;
	bool		enableEAX;
	bool		enableA3D;
} uiAudioValues_t;

static uiAudioValues_t	uiAudioInitial;

typedef struct
{
	char		*audioList[MAX_AUDIODEVICES];
	int		numSoundDlls;

	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;

	menuAction_s	done;
	menuAction_s	apply;

	menuSpinControl_s	soundLibrary;
	menuSlider_s	soundVolume;
	menuSlider_s	musicVolume;
	menuCheckBox_s	EAX;
	menuCheckBox_s	A3D;

	menuAction_s	hintMessage;
	char		hintText[MAX_SYSPATH];
} uiAudio_t;

static uiAudio_t		uiAudio;


/*
=================
UI_Audio_GetDeviceList
=================
*/
static void UI_Audio_GetDeviceList( void )
{
	int	i;

	for( i = 0; i < num_audio_dlls; i++ )
	{
		uiAudio.audioList[i] = audio_dlls[i];
		if( !com.stricmp( uiAudio.audioList[i], Cvar_VariableString( "host_audio" )))
			uiAudio.soundLibrary.curValue = i;
	}

	uiAudio.numSoundDlls = num_audio_dlls;
	uiAudio.soundLibrary.maxValue = uiAudio.numSoundDlls - 1;
}

/*
=================
UI_Audio_GetConfig
=================
*/
static void UI_Audio_GetConfig( void )
{
	UI_Audio_GetDeviceList();

	uiAudio.soundVolume.curValue = Cvar_VariableValue( "s_volume" );
	uiAudio.musicVolume.curValue = Cvar_VariableValue( "s_musicvolume" );

	if( Cvar_VariableInteger( "s_allowEAX" ))
		uiAudio.EAX.enabled = 1;

	if( Cvar_VariableInteger( "s_allowA3D" ))
		uiAudio.A3D.enabled = 1;

	// save initial values
	uiAudioInitial.soundLibrary = uiAudio.soundLibrary.curValue;
	uiAudioInitial.soundVolume = uiAudio.soundVolume.curValue;
	uiAudioInitial.musicVolume = uiAudio.musicVolume.curValue;
	uiAudioInitial.enableEAX = uiAudio.EAX.enabled;
	uiAudioInitial.enableA3D = uiAudio.A3D.enabled;
}

/*
=================
UI_Audio_SetConfig
=================
*/
static void UI_Audio_SetConfig( void )
{
	Cvar_FullSet( "host_audio", uiAudio.audioList[(int)uiAudio.soundLibrary.curValue], CVAR_SYSTEMINFO );
	Cvar_SetValue( "s_volume", uiAudio.soundVolume.curValue );
	Cvar_SetValue( "s_musicvolume", uiAudio.musicVolume.curValue );
	Cvar_SetValue( "s_allowEAX", uiAudio.EAX.enabled );
	Cvar_SetValue( "s_allowA3D", uiAudio.A3D.enabled );

	// sound subsystem will be automatically restarted on nextframe
	Cbuf_ExecuteText( EXEC_APPEND, "snd_restart\n" );
}

/*
=================
UI_Audio_UpdateConfig
=================
*/
static void UI_Audio_UpdateConfig( void )
{
	uiAudio.soundLibrary.generic.name = uiAudio.audioList[(int)uiAudio.soundLibrary.curValue];

	Cvar_SetValue( "s_volume", uiAudio.soundVolume.curValue );
	Cvar_SetValue( "s_musicvolume", uiAudio.musicVolume.curValue );

	// See if the apply button should be enabled or disabled
	uiAudio.apply.generic.flags |= QMF_GRAYED;

	if( uiAudioInitial.soundLibrary != uiAudio.soundLibrary.curValue )
	{
		uiAudio.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}

	if( uiAudioInitial.enableEAX != uiAudio.EAX.enabled )
	{
		uiAudio.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}

	if( uiAudioInitial.enableA3D != uiAudio.A3D.enabled )
	{
		uiAudio.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}
}

/*
=================
UI_Audio_Callback
=================
*/
static void UI_Audio_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	switch( item->id )
	{
	case ID_EAX:
	case ID_A3D:
		if( event == QM_PRESSED )
			((menuCheckBox_s *)self)->focusPic = UI_CHECKBOX_PRESSED;
		else ((menuCheckBox_s *)self)->focusPic = UI_CHECKBOX_FOCUS;
		break;
	}

	if( event == QM_CHANGED )
	{
		UI_Audio_UpdateConfig();
		return;
	}

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_DONE:
		UI_PopMenu();
		break;
	case ID_APPLY:
		UI_Audio_SetConfig();
		UI_Audio_GetConfig();
		UI_Audio_UpdateConfig();
		break;
	}
}

/*
=================
UI_Audio_Init
=================
*/
static void UI_Audio_Init( void )
{
	Mem_Set( &uiAudio, 0, sizeof( uiAudio_t ));

	com.strncat( uiAudio.hintText, "Change Xash3D sound engine to get more compatibility\n", MAX_SYSPATH );
	com.strncat( uiAudio.hintText, "or enable new features (EAX, Filters etc)", MAX_SYSPATH );

	uiAudio.background.generic.id	= ID_BACKGROUND;
	uiAudio.background.generic.type = QMTYPE_BITMAP;
	uiAudio.background.generic.flags = QMF_INACTIVE;
	uiAudio.background.generic.x = 0;
	uiAudio.background.generic.y = 0;
	uiAudio.background.generic.width = 1024;
	uiAudio.background.generic.height = 768;
	uiAudio.background.pic = ART_BACKGROUND;

	uiAudio.banner.generic.id = ID_BANNER;
	uiAudio.banner.generic.type = QMTYPE_BITMAP;
	uiAudio.banner.generic.flags = QMF_INACTIVE;
	uiAudio.banner.generic.x = UI_BANNER_POSX;
	uiAudio.banner.generic.y = UI_BANNER_POSY;
	uiAudio.banner.generic.width = UI_BANNER_WIDTH;
	uiAudio.banner.generic.height	= UI_BANNER_HEIGHT;
	uiAudio.banner.pic = ART_BANNER;

	uiAudio.hintMessage.generic.id = ID_MSGHINT;
	uiAudio.hintMessage.generic.type = QMTYPE_ACTION;
	uiAudio.hintMessage.generic.flags = QMF_INACTIVE|QMF_SMALLFONT;
	uiAudio.hintMessage.generic.color = uiColorLtGrey;
	uiAudio.hintMessage.generic.name = uiAudio.hintText;
	uiAudio.hintMessage.generic.x = 320;
	uiAudio.hintMessage.generic.y = 490;

	uiAudio.apply.generic.id = ID_APPLY;
	uiAudio.apply.generic.type = QMTYPE_ACTION;
	uiAudio.apply.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiAudio.apply.generic.x = 72;
	uiAudio.apply.generic.y = 230;
	uiAudio.apply.generic.name = "Apply";
	uiAudio.apply.generic.statusText = "Apply changes";
	uiAudio.apply.generic.callback = UI_Audio_Callback;

	uiAudio.done.generic.id = ID_DONE;
	uiAudio.done.generic.type = QMTYPE_ACTION;
	uiAudio.done.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiAudio.done.generic.x = 72;
	uiAudio.done.generic.y = 280;
	uiAudio.done.generic.name = "Done";
	uiAudio.done.generic.statusText = "Go back to the Configuration Menu";
	uiAudio.done.generic.callback = UI_Audio_Callback;

	uiAudio.soundLibrary.generic.id = ID_SOUNDLIBRARY;
	uiAudio.soundLibrary.generic.type = QMTYPE_SPINCONTROL;
	uiAudio.soundLibrary.generic.flags = QMF_CENTER_JUSTIFY|QMF_SMALLFONT|QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW;
	uiAudio.soundLibrary.generic.x = 360;
	uiAudio.soundLibrary.generic.y = 550;
	uiAudio.soundLibrary.generic.width = 168;
	uiAudio.soundLibrary.generic.height = 26;
	uiAudio.soundLibrary.generic.callback = UI_Audio_Callback;
	uiAudio.soundLibrary.generic.statusText = "Select audio engine";
	uiAudio.soundLibrary.minValue = 0;
	uiAudio.soundLibrary.maxValue = 0;
	uiAudio.soundLibrary.range = 1;

	uiAudio.soundVolume.generic.id = ID_SOUNDVOLUME;
	uiAudio.soundVolume.generic.type = QMTYPE_SLIDER;
	uiAudio.soundVolume.generic.flags = QMF_PULSEIFFOCUS|QMF_DROPSHADOW;
	uiAudio.soundVolume.generic.name = "Game sound volume";
	uiAudio.soundVolume.generic.x = 320;
	uiAudio.soundVolume.generic.y = 280;
	uiAudio.soundVolume.generic.callback = UI_Audio_Callback;
	uiAudio.soundVolume.generic.statusText = "Set master volume level";
	uiAudio.soundVolume.minValue	= 0.0;
	uiAudio.soundVolume.maxValue	= 1.0;
	uiAudio.soundVolume.range = 0.05f;

	uiAudio.musicVolume.generic.id = ID_MUSICVOLUME;
	uiAudio.musicVolume.generic.type = QMTYPE_SLIDER;
	uiAudio.musicVolume.generic.flags = QMF_PULSEIFFOCUS|QMF_DROPSHADOW;
	uiAudio.musicVolume.generic.name = "Game music volume";
	uiAudio.musicVolume.generic.x = 320;
	uiAudio.musicVolume.generic.y = 340;
	uiAudio.musicVolume.generic.callback = UI_Audio_Callback;
	uiAudio.musicVolume.generic.statusText = "Set background music volume level";
	uiAudio.musicVolume.minValue = 0.0;
	uiAudio.musicVolume.maxValue = 1.0;
	uiAudio.musicVolume.range = 0.05f;

	uiAudio.EAX.generic.id = ID_EAX;
	uiAudio.EAX.generic.type = QMTYPE_CHECKBOX;
	uiAudio.EAX.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_ACT_ONRELEASE|QMF_MOUSEONLY|QMF_DROPSHADOW;
	uiAudio.EAX.generic.name = "Enable EAX 2.0 support";
	uiAudio.EAX.generic.x = 320;
	uiAudio.EAX.generic.y = 370;
	uiAudio.EAX.generic.callback = UI_Audio_Callback;
	uiAudio.EAX.generic.statusText = "enable/disable Environmental Audio eXtensions";

	uiAudio.A3D.generic.id = ID_A3D;
	uiAudio.A3D.generic.type = QMTYPE_CHECKBOX;
	uiAudio.A3D.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_GRAYED|QMF_ACT_ONRELEASE|QMF_MOUSEONLY|QMF_DROPSHADOW;
	uiAudio.A3D.generic.name = "Enable A3D 2.0 support";
	uiAudio.A3D.generic.x = 320;
	uiAudio.A3D.generic.y = 420;
	uiAudio.A3D.generic.callback = UI_Audio_Callback;
	uiAudio.A3D.generic.statusText = "enable/disable Aureal Audio 3D environment sound";

	UI_Audio_GetConfig();

	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.background );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.banner );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.done );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.apply );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.hintMessage );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.soundLibrary );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.soundVolume );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.musicVolume );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.EAX );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.A3D );
}

/*
=================
UI_Audio_Precache
=================
*/
void UI_Audio_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
}

/*
=================
UI_Audio_Menu
=================
*/
void UI_Audio_Menu( void )
{
	UI_Audio_Precache();
	UI_Audio_Init();

	UI_Audio_UpdateConfig();
	UI_PushMenu( &uiAudio.menu );
}