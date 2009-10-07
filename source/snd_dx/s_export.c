//=======================================================================
//			Copyright XashXT Group 2007 ©
//			s_export.c - sound library main
//=======================================================================

#include "sound.h"

vsound_imp_t	si;
stdlib_api_t	com;
byte 		*sndpool;

vsound_exp_t DLLEXPORT *CreateAPI( stdlib_api_t *input, vsound_imp_t *engfuncs )
{
	static vsound_exp_t		snd;

	com = *input;

	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but second argument will be 0
	// and always make exception, run simply check for avoid it
	if( engfuncs ) si = *engfuncs;

	// generic functions
	snd.api_size = sizeof( vsound_exp_t );
	snd.com_size = sizeof( stdlib_api_t );
	
	snd.Init = S_Init;
	snd.Shutdown = S_Shutdown;

	// sound manager
	snd.BeginRegistration = S_BeginRegistration;
	snd.RegisterSound = S_RegisterSound;
	snd.EndRegistration = S_EndRegistration;

	snd.StartSound = S_StartSound;
	snd.StreamRawSamples = S_StreamRawSamples;
	snd.AddLoopingSound = S_AddLoopingSound;
	snd.StartLocalSound = S_StartLocalSound;
	snd.StartBackgroundTrack = S_StartBackgroundTrack;
	snd.StopBackgroundTrack = S_StopBackgroundTrack;

	snd.StartStreaming = S_StartStreaming;
	snd.StopStreaming = S_StopStreaming;

	snd.RenderFrame = S_Update;
	snd.StopAllSounds = S_StopAllSounds;
	snd.FreeSounds = S_FreeSounds;

	snd.Activate = S_Activate;

	return &snd;
}