//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sound.h - sndlib main header
//=======================================================================

#ifndef SOUND_H
#define SOUND_H

#include <windows.h>
#include "launch_api.h"
#include "qfiles_ref.h"
#include "engine_api.h"	// trace_t declaration
#include "vsound_api.h"
#include "s_openal.h"

extern stdlib_api_t com;
extern vsound_imp_t	si;
extern byte *sndpool;

#include "mathlib.h"

typedef enum
{
	S_OPENAL_110 = 0,		// base
	S_EXT_EFX,
	S_EXT_I3DL,
	S_EXT_EAX,
	S_EXT_EAX20,
	S_EXT_EAX30,
	S_EXTCOUNT
} s_openal_extensions;

enum
{
	CHAN_FIRSTPLAY,
	CHAN_LOOPED,
	CHAN_NORMAL,
};

typedef struct sfx_s
{
	string		name;
	bool		loaded;
	int		loopStart;	// looping point (in samples)
	int		samples;
	int		rate;
	int		sampleStep;	// ( samples / rate ) * 100
	uint		format;
	uint		bufferNum;

	int		touchFrame;
	bool		default_sound;
	uint		hashValue;
	struct sfx_s	*hashNext;
} sfx_t;

typedef struct
{
	string		loopName;
	stream_t		*intro_stream;
	stream_t		*main_stream;	// mainstream he-he
	bool		active;
} bg_track_t;

// structure used for fading in and out client sound volume.
typedef struct
{
	float		initial_percent;
	float		percent;  	// how far to adjust client's volume down by.
	float		starttime;	// GetHostTime() when we started adjusting volume
	float		fadeouttime;	// # of seconds to get to faded out state
	float		holdtime;		// # of seconds to hold
	float		fadeintime;	// # of seconds to restore
} soundfade_t;

typedef struct
{
	sfx_t		*sfx;		// NULL if unused
	int		state;		// channel state

	int		entnum;		// to allow overriding a specific sound
	int		entchannel;
	uint		startTime;	// for overriding oldest sounds
	bool		staticsound;	// use position instead of fetching entity's origin
	vec3_t		position;		// only use if fixedPosition is set
	float		volume;
	float		pitch;		// real-time pitch after any modulation or shift by dynamic data
	float		dist_mult;
	uint		sourceNum;	// openAL source
	bool		use_loop;		// don't loop default and local sounds
	bool		isSentence;	// bit who indicated sentence
} channel_t;

typedef struct
{
	vec3_t		position;
	vec3_t		velocity;
	float		orientation[6];
	int		waterlevel;
} listener_t;

typedef struct
{
	const char	*vendor_string;
	const char	*renderer_string;
	const char	*version_string;
	const char	*extensions_string;

	byte		extension[S_EXTCOUNT];
	string		deviceList[4];
	const char	*defDevice;
	uint		device_count;
	uint		num_slots;
	uint		num_sends;

	bool		allow_3DMode;

	// 3d mode extension (eax or i3d) 
	int (*Set3DMode)( const guid_t*, uint, uint, void*, uint );
	int (*Get3DMode)( const guid_t*, uint, uint, void*, uint );
} alconfig_t;

typedef struct
{
	aldevice		*hDevice;
	alcontext		*hALC;
	ref_params_t	*refdef;

	bool		initialized;
	bool		active;
	bool		paused;
	uint		framecount;
	int		max_channels;	// max channels that can be allocated by openAL
	int		total_channels;	// total playing channels at current time
	int		clientnum;
} alstate_t;

extern alconfig_t		al_config;
extern alstate_t 		al_state;

#define Host_Error		com.error
#define Z_Malloc( size )	Mem_Alloc( sndpool, size )

// cvars
extern cvar_t *s_alDevice;
extern cvar_t *s_allowEAX;
extern cvar_t *s_musicvolume;
extern cvar_t *s_check_errors;

//
// s_load.c
//
bool S_TestSoundChar( const char *pch, char c );
char *S_SkipSoundChar( const char *pch );
uint S_GetFormat( int width, int channels );

bool S_Init( void *hInst );
void S_Shutdown( void );
void S_Activate( bool active );
void S_SoundList_f( void );
bool S_CheckForErrors( void );
void S_Update( ref_params_t *fd );
void S_StartSound( const vec3_t pos, int ent, int chan, sound_t sfx, float vol, float attn, int pitch, int flags );
void S_StaticSound( const vec3_t pos, int ent, int chan, sound_t handle, float fvol, float attn, int pitch, int flags );
void S_StreamRawSamples( int samples, int rate, int width, int channels, const byte *data );
void S_StartBackgroundTrack( const char *intro, const char *loop );
channel_t *SND_PickStaticChannel( int entnum, sfx_t *sfx );
channel_t *SND_PickDynamicChannel( int entnum, int channel, sfx_t *sfx );
void S_FadeClientVolume( float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds );
int S_StartLocalSound( const char *name, float volume, int pitch, const float *org );
sfx_t *S_GetSfxByHandle( sound_t handle );
void S_StopSound( int entnum, int channel, const char *soundname );
void S_StreamBackgroundTrack( void );
void S_StopBackgroundTrack( void );
void S_ClearSoundBuffer( void );
bool S_LoadSound( sfx_t *sfx );
void S_StartStreaming( void );
void S_StopStreaming( void );
void S_StopAllSounds( void );
void S_FreeSounds( void );

// registration manager
void S_BeginRegistration( void );
sound_t S_RegisterSound( const char *sample );
void S_EndRegistration( void );


#endif//SOUND_H