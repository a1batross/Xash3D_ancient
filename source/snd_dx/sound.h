//=======================================================================
//			Copyright XashXT Group 2009 ©
//			sound.h - sndlib main header
//=======================================================================

#ifndef SOUND_H
#define SOUND_H

#include <windows.h>
#include "launch_api.h"
#include "qfiles_ref.h"
#include "engine_api.h"	// trace_t declaration
#include "vsound_api.h"

extern stdlib_api_t com;
extern vsound_imp_t	si;
extern byte *sndpool;

#include "mathlib.h"

typedef struct
{
	int		left;
	int		right;
} portable_samplepair_t;

typedef struct sfx_s
{
	string 		name;
	wavdata_t		*cache;

	int		touchFrame;
	bool		default_sound;
	uint		hashValue;
	struct sfx_s	*hashNext;
} sfx_t;

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
	int		channels;
	int		samples;		// mono samples in buffer
	int		submission_chunk;	// don't mix less than this #
	int		samplepos;	// in mono samples
	int		samplebits;
	int		speed;
	byte		*buffer;
} dma_t;

typedef struct
{
	sfx_t		*sfx;		// sfx number
	int		end;		// end time in global paintsamples
	int 		pos;		// sample position in sfx

	int		leftvol;		// 0-255 left volume
	int		rightvol;		// 0-255 right volume
	int		dleftvol;		// 0-255 left volume - doppler outgoing wav
	int		drightvol;	// 0-255 right volume - doppler outgoing wav

	int		entnum;		// entity soundsource
	int		entchannel;	// sound channel (CHAN_STREAM, CHAN_VOICE, etc.)	
	vec3_t		origin;		// only use if fixed_origin is set
	vec3_t		direction;	// initially null, then used entity direction
	bool		staticsound;	// use origin instead of fetching entnum's origin
	float		dist_mult;	// distance multiplier (attenuation/clipK)
	int		master_vol;	// 0-255 master volume
	bool		isSentence;	// bit who indicated sentence
	int		basePitch;	// base pitch percent (100% is normal pitch playback)
	float		pitch;		// real-time pitch after any modulation or shift by dynamic data

	bool		bfirstpass;	// true if this is first time sound is spatialized
	float		ob_gain;		// gain drop if sound source obscured from listener
	float		ob_gain_target;	// target gain while crossfading between ob_gain & ob_gain_target
	float		ob_gain_inc;	// crossfade increment
	bool		bTraced;		// true if channel was already checked this frame for obscuring
	float		radius;		// radius of this sound effect
	bool		doppler_effect;	// this chanel has doppler effect
	bool		use_loop;		// don't loop default and local sounds
} channel_t;

typedef struct
{
	vec3_t		origin;		// simorg
	vec3_t		vieworg;		// simorg + view_ofs
	vec3_t		velocity;
	vec3_t		forward;
	vec3_t		right;
	vec3_t		up;

	int		entnum;
	int		waterlevel;
	float		frametime;	// used for sound fade
	bool		ingame;		// listener in-game ?
	bool		paused;
} listener_t;

typedef struct
{
	int		rate;
	int		width;
	int		channels;
	int		loopstart;
	int		samples;
	int		dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

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

/*
====================================================================

  SYSTEM SPECIFIC FUNCTIONS

====================================================================
*/
#define Host_Error		com.error
#define Z_Malloc( size )	Mem_Alloc( sndpool, size )

// initializes cycling through a DMA buffer and returns information on it
bool SNDDMA_Init( void *hInst );
int SNDDMA_GetSoundtime( void );
void SNDDMA_Shutdown( void );
void SNDDMA_BeginPainting( void );
void SNDDMA_Submit( void );

//====================================================================

#define MAX_DYNAMIC_CHANNELS	24
#define MAX_CHANNELS	128
#define MAX_RAW_SAMPLES	8192

extern portable_samplepair_t paintbuffer[];
extern channel_t	channels[MAX_CHANNELS];
extern int	total_channels;
extern int	paintedtime;
extern int	s_rawend;
extern dma_t	dma;
extern listener_t	s_listener;

extern cvar_t	*s_check_errors;
extern cvar_t	*s_volume;
extern cvar_t	*s_khz;
extern cvar_t	*s_show;
extern cvar_t	*s_mixahead;
extern cvar_t	*s_primary;

extern portable_samplepair_t		s_rawsamples[MAX_RAW_SAMPLES];

void S_InitScaletable( void );
wavdata_t *S_LoadSound( sfx_t *sfx );
void S_PaintChannels( int endtime );
float S_GetMasterVolume( void );
void S_PrintDeviceName( void );

// s_load.c
bool S_TestSoundChar( const char *pch, char c );
char *S_SkipSoundChar( const char *pch );

// s_dsp.c
void SX_Init( void );
void SX_Free( void );
void SX_RoomFX( int endtime, int fFilter, int fTimefx );

bool S_Init( void *hInst );
void S_Shutdown( void );
void S_Activate( bool active );
void S_SoundList_f( void );
void S_SoundInfo_f( void );

// if origin is NULL, the sound will be dynamically sourced from the entity
void S_StartSound( const vec3_t pos, int ent, int chan, sound_t sfx, float vol, float attn, int pitch, int flags );
void S_StartStaticSound( const vec3_t pos, int ent, int chan, sound_t handle, float fvol, float attn, int pitch, int flags );
void S_StreamRawSamples( int samples, int rate, int width, int channels, const byte *data );
bool S_AddLoopingSound( int entnum, sound_t handle, float volume, float attn );
void S_StartBackgroundTrack( const char *intro, const char *loop );
channel_t *SND_PickDynamicChannel( int entnum, int channel, sfx_t *sfx );
channel_t *SND_PickStaticChannel( int entnum, sfx_t *sfx );
void S_FadeClientVolume( float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds );
int S_StartLocalSound( const char *name, float volume, int pitch, const float *org );
sfx_t *S_GetSfxByHandle( sound_t handle );
void S_StopSound( int entnum, int channel, const char *soundname );
void S_StopBackgroundTrack( void );
void S_RenderFrame( ref_params_t *fd );
void S_StartStreaming( void );
void S_StopStreaming( void );
void S_StopAllSounds( void );
void S_FreeSounds( void );

//
// s_mouth.c
//
void SND_InitMouth( int entnum, int entchannel );
void SND_CloseMouth( channel_t *ch );

//
// s_vox.c
//
void VOX_SetChanVol( channel_t *ch );
void VOX_LoadSound( channel_t *pchan, const char *psz );

void S_BeginRegistration( void );
sound_t S_RegisterSound( const char *sample );
void S_EndRegistration( void );

#endif//SOUND_H