//=======================================================================
//			Copyright XashXT Group 2007 �
//			sound.h - sndlib main header
//=======================================================================

#ifndef SOUND_H
#define SOUND_H

#include <windows.h>
#include "launch_api.h"
#include "qfiles_ref.h"
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

typedef struct
{
	int	rate;
	int	width;
	int	loopstart;
	int	channels;
	int	samples;
} wavinfo_t;

typedef struct sfx_s
{
	string		name;
	bool		loaded;
	int		loopstart;	// looping point (in samples)
	int		samples;
	int		rate;
	uint		format;
	uint		bufferNum;

	bool		default_sound;
	int		registration_sequence;
} sfx_t;

typedef struct
{
	string		introName;
	string		loopName;
	bool		looping;
	file_t		*file;
	int		start;
	int		rate;
	uint		format;
	void		*vorbisFile;
} bg_track_t;

// a playSound will be generated by each call to S_StartSound.
// when the mixer reaches playSound->beginTime, the playSound will be
// assigned to a channel.
typedef struct playsound_s
{
	struct playsound_s	*prev, *next;
	sfx_t		*sfx;
	int		entnum;
	int		entchannel;
	bool		fixedPosition;	// Use position instead of fetching entity's origin
	bool		use_loop;		// ignore looping sounds for local sound
	vec3_t		position;		// Only use if fixedPosition is set
	float		volume;
	float		attenuation;
	float		beginTime;	// Begin at this time
	float		pitch;
} playSound_t;

typedef struct
{
	bool		streaming;
	sfx_t		*sfx;		// NULL if unused
	int		state;		// channel state
	int		entnum;		// to allow overriding a specific sound
	int		entchannel;
	float		startTime;	// for overriding oldest sounds

	bool		loopsound;	// is looping sound ?
	int		loopnum;		// looping entity number
	int		loopframe;	// for stopping looping sounds
	int		loopstart;	// check it for set properly offset in samples

	bool		fixedPosition;	// use position instead of fetching entity's origin
	vec3_t		position;		// only use if fixedPosition is set
	float		volume;
	float		pitch;
	float		distanceMult;
	uint		sourceNum;	// openAL source
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
	uint		framecount;
	int		num_channels;
	int		clientnum;
} alstate_t;

extern alconfig_t al_config;
extern alstate_t  al_state;

#define Host_Error com.error
#define Z_Malloc( size )	Mem_Alloc( sndpool, size )

// cvars
extern cvar_t *s_alDevice;
extern cvar_t *s_soundfx;
extern cvar_t *s_musicvolume;
extern cvar_t *s_check_errors;

bool S_Init( void *hInst );
void S_Shutdown( void );
void S_Activate( bool active );
void S_SoundList_f( void );
bool S_CheckForErrors( void );
void S_Update( ref_params_t *fd );
void S_StartSound( const vec3_t pos, int ent, int chan, sound_t sfx, float vol, float attn, float pitch, int flags );
void S_StreamRawSamples( int samples, int rate, int width, int channels, const byte *data );
bool S_AddLoopingSound( int entnum, sound_t handle, float volume, float attn );
void S_StartBackgroundTrack( const char *intro, const char *loop );
channel_t	*S_PickChannel( int entNum, int entChannel );
int S_StartLocalSound( const char *name, float volume, float pitch, const float *org );
sfx_t *S_GetSfxByHandle( sound_t handle );
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