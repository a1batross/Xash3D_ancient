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
#define ART_TEXT1			"gfx/shell/text/audio_text_p1"
#define ART_TEXT2			"gfx/shell/text/audio_text_p2"

#define ID_BACKGROUND		0
#define ID_BANNER			1

#define ID_TEXT1			2
#define ID_TEXT2			3

#define ID_DONE			6
#define ID_APPLY			7

#define ID_AUDIODEVICE		10
#define ID_MASTERVOLUME		11
#define ID_SFXVOLUME		12
#define ID_MUSICVOLUME		13
#define ID_ROLLOFFFACTOR		14
#define ID_DOPPLERFACTOR		15
#define ID_EAX			16

#define MAX_AUDIODEVICES		16

static const char *uiAudioYesNo[] = { "False", "True" };

typedef struct
{
	float		audioDevice;
	float		masterVolume;
	float		sfxVolume;
	float		musicVolume;
	float		rolloffFactor;
	float		dopplerFactor;
	float		eax;
} uiAudioValues_t;

static uiAudioValues_t	uiAudioInitial;

typedef struct
{
	char		*audioDeviceList[MAX_AUDIODEVICES];
	int		numAudioDevices;

	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;

	menuBitmap_s	text1;
	menuBitmap_s	text2;

	menuAction_s	done;
	menuAction_s	apply;

	menuSpinControl_s	audioDevice;
	menuSpinControl_s	masterVolume;
	menuSpinControl_s	sfxVolume;
	menuSpinControl_s	musicVolume;
	menuSpinControl_s	rolloffFactor;
	menuSpinControl_s	dopplerFactor;
	menuSpinControl_s	eax;
} uiAudio_t;

static uiAudio_t		uiAudio;


/*
=================
UI_Audio_GetDeviceList
=================
*/
static void UI_Audio_GetDeviceList( void )
{
	uiAudio.audioDeviceList[0] = "Generic Software";
	uiAudio.audioDeviceList[1] = "Generic Hardware";
	uiAudio.numAudioDevices = 2;

	uiAudio.audioDevice.maxValue = uiAudio.numAudioDevices - 1;
	uiAudio.audioDevice.curValue = uiAudio.numAudioDevices - 1;
}

/*
=================
UI_Audio_GetConfig
=================
*/
static void UI_Audio_GetConfig( void )
{
	UI_Audio_GetDeviceList();

	uiAudio.masterVolume.curValue = Cvar_VariableValue( "s_volume" );
	uiAudio.sfxVolume.curValue = Cvar_VariableValue( "s_sfxvolume" );
	uiAudio.musicVolume.curValue = Cvar_VariableValue( "s_musicvolume" );
	uiAudio.rolloffFactor.curValue = Cvar_VariableValue( "s_rollofffactor" );
	uiAudio.dopplerFactor.curValue = Cvar_VariableValue( "s_dopplerfactor" );

	if( Cvar_VariableInteger( "s_soundfx" ))
		uiAudio.eax.curValue = 1;

	// save initial values
	uiAudioInitial.audioDevice = uiAudio.audioDevice.curValue;
	uiAudioInitial.masterVolume = uiAudio.masterVolume.curValue;
	uiAudioInitial.sfxVolume = uiAudio.sfxVolume.curValue;
	uiAudioInitial.musicVolume = uiAudio.musicVolume.curValue;
	uiAudioInitial.rolloffFactor = uiAudio.rolloffFactor.curValue;
	uiAudioInitial.dopplerFactor = uiAudio.dopplerFactor.curValue;
	uiAudioInitial.eax = uiAudio.eax.curValue;
}

/*
=================
UI_Audio_SetConfig
=================
*/
static void UI_Audio_SetConfig( void )
{
	Cvar_Set( "s_device", uiAudio.audioDeviceList[(int)uiAudio.audioDevice.curValue] );
	Cvar_SetValue( "s_volume", uiAudio.masterVolume.curValue );
	Cvar_SetValue( "s_volume", uiAudio.sfxVolume.curValue );
	Cvar_SetValue( "s_musicvolume", uiAudio.musicVolume.curValue );
	Cvar_SetValue( "s_rollofffactor", uiAudio.rolloffFactor.curValue );
	Cvar_SetValue("s_dopplerfactor", uiAudio.dopplerFactor.curValue );
	Cvar_SetValue( "s_soundfx", uiAudio.eax.curValue );

	// restart sound subsystem
	Cbuf_ExecuteText( EXEC_NOW, "snd_restart\n" );
}

/*
=================
UI_Audio_UpdateConfig
=================
*/
static void UI_Audio_UpdateConfig( void )
{
	static char	masterVolumeText[8];
	static char	sfxVolumeText[8];
	static char	musicVolumeText[8];
	static char	rolloffFactorText[8];
	static char	dopplerFactorText[8];

	uiAudio.audioDevice.generic.name = uiAudio.audioDeviceList[(int)uiAudio.audioDevice.curValue];

	com.snprintf( masterVolumeText, sizeof( masterVolumeText ), "%.1f", uiAudio.masterVolume.curValue );
	uiAudio.masterVolume.generic.name = masterVolumeText;

	com.snprintf( sfxVolumeText, sizeof( sfxVolumeText ), "%.1f", uiAudio.sfxVolume.curValue );
	uiAudio.sfxVolume.generic.name = sfxVolumeText;

	com.snprintf( musicVolumeText, sizeof( musicVolumeText ), "%.1f", uiAudio.musicVolume.curValue );
	uiAudio.musicVolume.generic.name = musicVolumeText;

	com.snprintf( rolloffFactorText, sizeof( rolloffFactorText ), "%.1f", uiAudio.rolloffFactor.curValue );
	uiAudio.rolloffFactor.generic.name = rolloffFactorText;

	com.snprintf( dopplerFactorText, sizeof( dopplerFactorText ), "%.1f", uiAudio.dopplerFactor.curValue );
	uiAudio.dopplerFactor.generic.name = dopplerFactorText;

	uiAudio.eax.generic.name = uiAudioYesNo[(int)uiAudio.eax.curValue];

	Cvar_SetValue( "s_volume", uiAudio.masterVolume.curValue );
	Cvar_SetValue( "s_volume", uiAudio.sfxVolume.curValue );
	Cvar_SetValue( "s_musicvolume", uiAudio.musicVolume.curValue );
	Cvar_SetValue( "s_rollofffactor", uiAudio.rolloffFactor.curValue );
	Cvar_SetValue( "s_dopplerfactor", uiAudio.dopplerFactor.curValue );

	// See if the apply button should be enabled or disabled
	uiAudio.apply.generic.flags |= QMF_GRAYED;

	if( uiAudioInitial.audioDevice != uiAudio.audioDevice.curValue )
	{
		uiAudio.apply.generic.flags &= ~QMF_GRAYED;
		return;
	}

	if( uiAudioInitial.eax != uiAudio.eax.curValue )
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

	uiAudio.text1.generic.id = ID_TEXT1;
	uiAudio.text1.generic.type = QMTYPE_BITMAP;
	uiAudio.text1.generic.flags = QMF_INACTIVE;
	uiAudio.text1.generic.x = 180;
	uiAudio.text1.generic.y = 168;
	uiAudio.text1.generic.width = 256;
	uiAudio.text1.generic.height = 256;
	uiAudio.text1.pic = ART_TEXT1;

	uiAudio.text2.generic.id = ID_TEXT2;
	uiAudio.text2.generic.type = QMTYPE_BITMAP;
	uiAudio.text2.generic.flags = QMF_INACTIVE;
	uiAudio.text2.generic.x = 180;
	uiAudio.text2.generic.y = 424;
	uiAudio.text2.generic.width = 256;
	uiAudio.text2.generic.height = 256;
	uiAudio.text2.pic = ART_TEXT2;

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

	uiAudio.audioDevice.generic.id = ID_AUDIODEVICE;
	uiAudio.audioDevice.generic.type = QMTYPE_SPINCONTROL;
	uiAudio.audioDevice.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAudio.audioDevice.generic.x = 580;
	uiAudio.audioDevice.generic.y = 256;
	uiAudio.audioDevice.generic.width = 198;
	uiAudio.audioDevice.generic.height = 30;
	uiAudio.audioDevice.generic.callback = UI_Audio_Callback;
	uiAudio.audioDevice.generic.statusText = "Select audio output device";
	uiAudio.audioDevice.minValue = 0;
	uiAudio.audioDevice.maxValue = 0;
	uiAudio.audioDevice.range = 1;

	uiAudio.masterVolume.generic.id = ID_MASTERVOLUME;
	uiAudio.masterVolume.generic.type = QMTYPE_SPINCONTROL;
	uiAudio.masterVolume.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAudio.masterVolume.generic.x = 580;
	uiAudio.masterVolume.generic.y = 288;
	uiAudio.masterVolume.generic.width = 198;
	uiAudio.masterVolume.generic.height = 30;
	uiAudio.masterVolume.generic.callback = UI_Audio_Callback;
	uiAudio.masterVolume.generic.statusText = "Set master volume level";
	uiAudio.masterVolume.minValue	= 0.0;
	uiAudio.masterVolume.maxValue	= 1.0;
	uiAudio.masterVolume.range = 0.1;

	uiAudio.sfxVolume.generic.id = ID_SFXVOLUME;
	uiAudio.sfxVolume.generic.type = QMTYPE_SPINCONTROL;
	uiAudio.sfxVolume.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAudio.sfxVolume.generic.x = 580;
	uiAudio.sfxVolume.generic.y = 320;
	uiAudio.sfxVolume.generic.width = 198;
	uiAudio.sfxVolume.generic.height = 30;
	uiAudio.sfxVolume.generic.callback = UI_Audio_Callback;
	uiAudio.sfxVolume.generic.statusText = "Set sound effects volume level";
	uiAudio.sfxVolume.minValue = 0.0;
	uiAudio.sfxVolume.maxValue = 1.0;
	uiAudio.sfxVolume.range = 0.1;

	uiAudio.musicVolume.generic.id = ID_MUSICVOLUME;
	uiAudio.musicVolume.generic.type = QMTYPE_SPINCONTROL;
	uiAudio.musicVolume.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAudio.musicVolume.generic.x = 580;
	uiAudio.musicVolume.generic.y = 352;
	uiAudio.musicVolume.generic.width = 198;
	uiAudio.musicVolume.generic.height = 30;
	uiAudio.musicVolume.generic.callback = UI_Audio_Callback;
	uiAudio.musicVolume.generic.statusText = "Set background music volume level";
	uiAudio.musicVolume.minValue = 0.0;
	uiAudio.musicVolume.maxValue = 1.0;
	uiAudio.musicVolume.range = 0.1;

	uiAudio.rolloffFactor.generic.id = ID_ROLLOFFFACTOR;
	uiAudio.rolloffFactor.generic.type = QMTYPE_SPINCONTROL;
	uiAudio.rolloffFactor.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAudio.rolloffFactor.generic.x = 580;
	uiAudio.rolloffFactor.generic.y = 384;
	uiAudio.rolloffFactor.generic.width = 198;
	uiAudio.rolloffFactor.generic.height = 30;
	uiAudio.rolloffFactor.generic.callback = UI_Audio_Callback;
	uiAudio.rolloffFactor.generic.statusText = "Set sound rolloff factor";
	uiAudio.rolloffFactor.minValue = 0.0;
	uiAudio.rolloffFactor.maxValue = 10.0;
	uiAudio.rolloffFactor.range = 1.0;

	uiAudio.dopplerFactor.generic.id = ID_DOPPLERFACTOR;
	uiAudio.dopplerFactor.generic.type = QMTYPE_SPINCONTROL;
	uiAudio.dopplerFactor.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAudio.dopplerFactor.generic.x = 580;
	uiAudio.dopplerFactor.generic.y = 416;
	uiAudio.dopplerFactor.generic.width = 198;
	uiAudio.dopplerFactor.generic.height = 30;
	uiAudio.dopplerFactor.generic.callback = UI_Audio_Callback;
	uiAudio.dopplerFactor.generic.statusText = "Set sound doppler factor";
	uiAudio.dopplerFactor.minValue = 0.0;
	uiAudio.dopplerFactor.maxValue = 10.0;
	uiAudio.dopplerFactor.range = 1.0;

	uiAudio.eax.generic.id = ID_EAX;
	uiAudio.eax.generic.type = QMTYPE_SPINCONTROL;
	uiAudio.eax.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiAudio.eax.generic.x = 580;
	uiAudio.eax.generic.y = 448;
	uiAudio.eax.generic.width = 198;
	uiAudio.eax.generic.height = 30;
	uiAudio.eax.generic.callback = UI_Audio_Callback;
	uiAudio.eax.generic.statusText = "enable/disable Environmental Audio eXtensions";
	uiAudio.eax.minValue = 0;
	uiAudio.eax.maxValue = 1;
	uiAudio.eax.range = 1;

	UI_Audio_GetConfig();

	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.background );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.banner );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.text1 );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.text2 );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.done );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.apply );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.audioDevice );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.masterVolume );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.sfxVolume );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.musicVolume );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.rolloffFactor );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.dopplerFactor );
	UI_AddItem( &uiAudio.menu, (void *)&uiAudio.eax );
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
	re->RegisterShader( ART_TEXT1, SHADER_NOMIP );
	re->RegisterShader( ART_TEXT2, SHADER_NOMIP );
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