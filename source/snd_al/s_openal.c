//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        s_openal.c - openal32.dll handler
//=======================================================================

#include "sound.h"

static dllfunc_t openal_funcs[] =
{
	{"alcOpenDevice", (void **) &palcOpenDevice },
	{"alcCloseDevice", (void **) &palcCloseDevice },
	{"alcCreateContext", (void **) &palcCreateContext },
	{"alcDestroyContext", (void **) &palcDestroyContext },
	{"alcMakeContextCurrent", (void **) &palcMakeContextCurrent },
	{"alcProcessContext", (void **) &palcProcessContext },
	{"alcSuspendContext", (void **) &palcSuspendContext },
	{"alcGetCurrentContext", (void **) &palcGetCurrentContext },
	{"alcGetContextsDevice", (void **) &palcGetContextsDevice },
	{"alcGetString", (void **) &palcGetString },
	{"alcGetIntegerv", (void **) &palcGetIntegerv },
	{"alcGetError", (void **) &palcGetError },
	{"alcIsExtensionPresent", (void **) &palcIsExtensionPresent },
	{"alcGetProcAddress", (void **) &palcGetProcAddress },
	{"alcGetEnumValue", (void **) &palcGetEnumValue },

	{"alBufferData", (void **) &palBufferData },
	{"alDeleteBuffers", (void **) &palDeleteBuffers },
	{"alDeleteSources", (void **) &palDeleteSources },
	{"alDisable", (void **) &palDisable },
	{"alDistanceModel", (void **) &palDistanceModel },
	{"alDopplerFactor", (void **) &palDopplerFactor },
	{"alDopplerVelocity", (void **) &palDopplerVelocity },
	{"alEnable", (void **) &palEnable },
	{"alGenBuffers", (void **) &palGenBuffers },
	{"alGenSources", (void **) &palGenSources },
	{"alGetBoolean", (void **) &palGetBoolean },
	{"alGetBooleanv", (void **) &palGetBooleanv },
	{"alGetBufferf", (void **) &palGetBufferf },
	{"alGetBufferi", (void **) &palGetBufferi },
	{"alGetDouble", (void **) &palGetDouble },
	{"alGetDoublev", (void **) &palGetDoublev },
	{"alGetEnumValue", (void **) &palGetEnumValue },
	{"alGetError", (void **) &palGetError },
	{"alGetFloat", (void **) &palGetFloat },
	{"alGetFloatv", (void **) &palGetFloatv },
	{"alGetInteger", (void **) &palGetInteger },
	{"alGetIntegerv", (void **) &palGetIntegerv },
	{"alGetListener3f", (void **) &palGetListener3f },
	{"alGetListenerf", (void **) &palGetListenerf },
	{"alGetListenerfv", (void **) &palGetListenerfv },
	{"alGetListeneri", (void **) &palGetListeneri },
	{"alGetProcAddress", (void **) &palGetProcAddress },
	{"alGetSource3f", (void **) &palGetSource3f },
	{"alGetSourcef", (void **) &palGetSourcef },
	{"alGetSourcefv", (void **) &palGetSourcefv },
	{"alGetSourcei", (void **) &palGetSourcei },
	{"alGetString", (void **) &palGetString },
	{"alIsBuffer", (void **) &palIsBuffer },
	{"alIsEnabled", (void **) &palIsEnabled },
	{"alIsExtensionPresent", (void **) &palIsExtensionPresent },
	{"alIsSource", (void **) &palIsSource },
	{"alListener3f", (void **) &palListener3f },
	{"alListenerf", (void **) &palListenerf },
	{"alListenerfv", (void **) &palListenerfv },
	{"alListeneri", (void **) &palListeneri },
	{"alSource3f", (void **) &palSource3f },
	{"alSourcef", (void **) &palSourcef },
	{"alSourcefv", (void **) &palSourcefv },
	{"alSourcei", (void **) &palSourcei },
	{"alSourcePause", (void **) &palSourcePause },
	{"alSourcePausev", (void **) &palSourcePausev },
	{"alSourcePlay", (void **) &palSourcePlay },
	{"alSourcePlayv", (void **) &palSourcePlayv },
	{"alSourceQueueBuffers", (void **) &palSourceQueueBuffers },
	{"alSourceRewind", (void **) &palSourceRewind },
	{"alSourceRewindv", (void **) &palSourceRewindv },
	{"alSourceStop", (void **) &palSourceStop },
	{"alSourceStopv", (void **) &palSourceStopv },
	{"alSourceUnqueueBuffers", (void **) &palSourceUnqueueBuffers },
	{ NULL, NULL }
};

static dllfunc_t openal_effects[] =
{
	{"alGenEffects", (void **) &alGenEffects },
	{"alDeleteEffects", (void **) &alDeleteEffects },
	{"alIsEffect", (void **) &alIsEffect },
	{"alEffecti", (void **) &alEffecti },
	{"alEffectiv", (void **) &alEffectiv },
	{"alEffectf", (void **) &alEffectf },
	{"alEffectfv", (void **) &alEffectfv },
	{"alGetEffecti", (void **) &alGetEffecti },
	{"alGetEffectiv", (void **) &alGetEffectiv },
	{"alGetEffectf", (void **) &alGetEffectf },
	{"alGetEffectfv", (void **) &alGetEffectfv },
	{"alGenFilters", (void **) &alGenFilters },
	{"alDeleteFilters", (void **) &alDeleteFilters },
	{"alIsFilter", (void **) &alIsFilter },
	{"alFilteri", (void **) &alFilteri },
	{"alFilteriv", (void **) &alFilteriv },
	{"alFilterf", (void **) &alFilterf },
	{"alFilterfv", (void **) &alFilterfv },
	{"alGetFilteri", (void **) &alGetFilteri },
	{"alGetFilteriv", (void **) &alGetFilteriv },
	{"alGetFilterf", (void **) &alGetFilterf },
	{"alGetFilterfv", (void **) &alGetFilterfv },
	{"alGenAuxiliaryEffectSlots", (void **) &alGenAuxiliaryEffectSlots },
	{"alDeleteAuxiliaryEffectSlots", (void **) &alDeleteAuxiliaryEffectSlots },
	{"alIsAuxiliaryEffectSlot", (void **) &alIsAuxiliaryEffectSlot },
	{"alAuxiliaryEffectSloti", (void **) &alAuxiliaryEffectSloti },
	{"alAuxiliaryEffectSlotiv", (void **) &alAuxiliaryEffectSlotiv },
	{"alAuxiliaryEffectSlotf", (void **) &alAuxiliaryEffectSlotf },
	{"alAuxiliaryEffectSlotfv", (void **) &alAuxiliaryEffectSlotfv },
	{"alGetAuxiliaryEffectSloti", (void **) &alGetAuxiliaryEffectSloti },
	{"alGetAuxiliaryEffectSlotiv", (void **) &alGetAuxiliaryEffectSlotiv },
	{"alGetAuxiliaryEffectSlotf", (void **) &alGetAuxiliaryEffectSlotf },
	{"alGetAuxiliaryEffectSlotfv", (void **) &alGetAuxiliaryEffectSlotfv },
	{ NULL, NULL }
};

dll_info_t openal_dll = { "openal32.dll", openal_funcs, NULL, NULL, NULL, false };

alconfig_t al_config;
alstate_t  al_state;

cvar_t *s_eax;

/*
=================
S_InitDriver
=================
*/
static bool S_InitDriver( void )
{
	int	attrlist[3] = { ALC_FREQUENCY, 44100, 0 };
	int	*al_contxt = attrlist;

	if(( al_state.hDevice = palcOpenDevice( s_alDevice->string )) == NULL )
	{
		Msg("alOpenDevice - failed\n" );
		return false;
	}

	if(( al_state.hALC = palcCreateContext( al_state.hDevice, al_contxt )) == NULL )
		goto failed;

	if( !palcMakeContextCurrent( al_state.hALC ))
		goto failed;
	return true;

failed:
	if( al_state.hALC )
	{
		palcDestroyContext( al_state.hALC );
		al_state.hALC = NULL;
	}

	if( al_state.hDevice )
	{
		palcCloseDevice( al_state.hDevice );
		al_state.hDevice = NULL;
	}

	// release openal at all
	Sys_FreeLibrary( &openal_dll );
	Mem_Set( &al_config, 0, sizeof( alconfig_t ));
	Mem_Set( &al_state, 0, sizeof( alstate_t ));
	
	return false;
}

static bool S_SetupEFX( void )
{
	const dllfunc_t	*func;

	// get the function pointers
	for( func = openal_effects; func && func->name != NULL; func++ )
	{
		if( !( *func->func = palGetProcAddress( func->name )))
		{
			MsgDev( D_ERROR, "S_InitEffects: %s missing or invalid function\n", func->name );
			return false;
		}
	}
	return true;
}

static void S_InitEffects( void )
{
	uint	uiEffects[1] = { 0 };
		
	alGenEffects(1, &uiEffects[0]);
	if( palGetError() == AL_NO_ERROR )
	{
		MsgDev( D_NOTE, "S_InitEffects:" );
		alEffecti( uiEffects[0], AL_EFFECT_TYPE, AL_EFFECT_REVERB );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_revrb" );
		alEffecti( uiEffects[0], AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_eaxrevrb" );
		alEffecti( uiEffects[0], AL_EFFECT_TYPE, AL_EFFECT_CHORUS );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_chorus" );
		alEffecti( uiEffects[0], AL_EFFECT_TYPE, AL_EFFECT_DISTORTION );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_distortion" );
		alEffecti( uiEffects[0], AL_EFFECT_TYPE, AL_EFFECT_ECHO );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_echo" );
		alEffecti( uiEffects[0], AL_EFFECT_TYPE, AL_EFFECT_FLANGER );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_flanger" );
		alEffecti( uiEffects[0], AL_EFFECT_TYPE, AL_EFFECT_FREQUENCY_SHIFTER );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_frequency_shifter" );
		alEffecti( uiEffects[0], AL_EFFECT_TYPE, AL_EFFECT_VOCAL_MORPHER );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_vocal_morpher" );
		alEffecti( uiEffects[0], AL_EFFECT_TYPE, AL_EFFECT_PITCH_SHIFTER );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_pitch_shifter" );
		alEffecti( uiEffects[0], AL_EFFECT_TYPE, AL_EFFECT_RING_MODULATOR );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_ring_modulator" );
		alEffecti( uiEffects[0], AL_EFFECT_TYPE, AL_EFFECT_AUTOWAH );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_autowah" );
		alEffecti( uiEffects[0], AL_EFFECT_TYPE, AL_EFFECT_COMPRESSOR );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_compressor" );
		alEffecti( uiEffects[0], AL_EFFECT_TYPE, AL_EFFECT_EQUALIZER );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_equalizer" );
		MsgDev( D_NOTE, "\n" );
	}
	alDeleteEffects(1, &uiEffects[0]);
}

static void S_InitFilters( void )
{
	uint	uiFilters[1] = { 0 };

	alGenFilters(1, &uiFilters[0]);
	if( palGetError() == AL_NO_ERROR )
	{
		MsgDev( D_NOTE, "S_InitFilters:" );
		alFilteri( uiFilters[0], AL_FILTER_TYPE, AL_FILTER_LOWPASS );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_lowpass" );
		alFilteri( uiFilters[0], AL_FILTER_TYPE, AL_FILTER_HIGHPASS );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_highpass" );
		alFilteri( uiFilters[0], AL_FILTER_TYPE, AL_FILTER_BANDPASS );
		if(!palGetError()) MsgDev( D_NOTE, " ^3al_ext_bandpass" );
		MsgDev( D_NOTE, "\n" );
	}
	alDeleteFilters( 1, &uiFilters[0] );
}

/*
=================
S_InitExtensions

grab extensions
=================
*/
static void S_InitExtensions( void )
{
	// set initial state
	al_config.Get3DMode = NULL;
	al_config.Set3DMode = NULL;
	al_config.allow_3DMode = false;

	if( !s_allowEAX->integer ) return;

	// check I3DL2 extension for NVidia's Sound Storm chips. I3DL2 is hidden and can be missed in extension list.
	if( !com.strcmp( NVIDIA_DEVICE_NAME, al_config.deviceList[0] ))
	{
		I3DL2Get = (void *)palGetProcAddress( "I3DL2Get" );
		I3DL2Set = (void *)palGetProcAddress( "I3DL2Set" );
		if( I3DL2Get && I3DL2Set )
		{
			al_config.allow_3DMode = true;
			al_config.Get3DMode = I3DL2Get;
			al_config.Set3DMode = I3DL2Set;
			MsgDev( D_NOTE, "S_InitExtensions: enable I3DL2\n" );
		}
	}
	if( palIsExtensionPresent( "EAX3.0" ) && !al_config.allow_3DMode )
	{
		alEAXSet = (void *)palGetProcAddress( "EAXSet" );
		alEAXGet = (void *)palGetProcAddress( "EAXGet" );
		if( alEAXSet && alEAXGet )
		{
			al_config.allow_3DMode = true;
			al_config.Get3DMode = alEAXGet;
			al_config.Set3DMode = alEAXSet;
			MsgDev( D_NOTE, "S_InitExtensions: enable EAX 3.0\n" );
		}
	}
	if( palIsExtensionPresent( "EAX2.0" ) && !al_config.allow_3DMode )
	{
		alEAXSet = (void *)palGetProcAddress( "EAXSet" );
		alEAXGet = (void *)palGetProcAddress( "EAXGet" );
		if( alEAXSet && alEAXGet )
		{
			al_config.allow_3DMode = true;
			al_config.Get3DMode = alEAXGet;
			al_config.Set3DMode = alEAXSet;
			MsgDev( D_NOTE, "S_InitExtensions: enable EAX 2.0\n" );
		}
	}
	if( palIsExtensionPresent( "EAX" ) && !al_config.allow_3DMode )
	{
		alEAXSet = (void *)palGetProcAddress( "EAXSet" );
		alEAXGet = (void *)palGetProcAddress( "EAXGet" );
		if( alEAXSet && alEAXGet )
		{
			al_config.allow_3DMode = true;
			al_config.Get3DMode = alEAXGet;
			al_config.Set3DMode = alEAXSet;
			MsgDev( D_NOTE, "S_InitExtensions: enable EAX 1.0\n" );
		}
	}

	if( palcIsExtensionPresent( al_state.hDevice, "ALC_EXT_EFX" ))
	{
		uint	uiEffectSlots[MAX_EFFECTS] = { 0 };

		if( !S_SetupEFX( )) return;

		// testing effect slots
		for( al_config.num_slots = 0; al_config.num_slots < MAX_EFFECTS; al_config.num_slots++ )
		{
			alGenAuxiliaryEffectSlots( 1, &uiEffectSlots[al_config.num_slots] );
			if( palGetError() != AL_NO_ERROR ) break;
			
		}
		palcGetIntegerv( al_state.hDevice, ALC_MAX_AUXILIARY_SENDS, 1, &al_config.num_sends );

		S_InitEffects();
		S_InitFilters();

		alDeleteAuxiliaryEffectSlots( al_config.num_slots, uiEffectSlots );
	}
}

bool S_Init_OpenAL( void )
{
	Sys_LoadLibrary( NULL, &openal_dll );

	if( !openal_dll.link )
	{
		MsgDev( D_ERROR, "OpenAL driver not installed\n");
		return false;
	}

	// Get device list
	if( palcIsExtensionPresent( NULL, "ALC_ENUMERATION_EXT" ))
	{
		// get list of devices
		const char *device_enum = palcGetString( NULL, ALC_DEVICE_SPECIFIER );
		al_config.defDevice = palcGetString( NULL, ALC_DEFAULT_DEVICE_SPECIFIER );

		while( *device_enum )
		{
			// enumerate devices
			com.strncpy( al_config.deviceList[al_config.device_count++], device_enum, MAX_STRING );
			while(*device_enum) device_enum++;
			device_enum++;
		}
	}
	else
	{
		MsgDev( D_ERROR, "can't enumerate OpenAL devices\n" );
		return false;
	} 

	// initialize the device, context, etc...
	if( !S_InitDriver( )) return false;

	// get some openal strings
	al_config.vendor_string = palGetString( AL_VENDOR );
	al_config.renderer_string = palGetString( AL_RENDERER );	// stupid name :-)
	al_config.version_string = palGetString( AL_VERSION );
	al_config.extensions_string = palGetString( AL_EXTENSIONS );
	MsgDev( D_INFO, "Audio: %s\n", al_config.renderer_string );

	// Initialize extensions
	S_InitExtensions();

	return true;
}

void S_Free_OpenAL( void )
{
	if( al_state.hALC )
	{
		if( palcMakeContextCurrent )
			palcMakeContextCurrent( NULL );
		if( palcDestroyContext )
			palcDestroyContext( al_state.hALC );
		al_state.hALC = NULL;
	}

	if( al_state.hDevice )
	{
		if( palcCloseDevice )
			palcCloseDevice( al_state.hDevice );
		al_state.hDevice = NULL;
	}

	Sys_FreeLibrary( &openal_dll );
	Mem_Set( &al_config, 0, sizeof( alconfig_t ));
	Mem_Set( &al_state, 0, sizeof( alstate_t ));
}