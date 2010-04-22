//=======================================================================
//			Copyright XashXT Group 2008 ©
//		  vsound_api.h - xash sound engine (OpenAL based)
//=======================================================================
#ifndef VSOUND_API_H
#define VSOUND_API_H

#include "ref_params.h"

typedef int		sound_t;

// snd internal flags (lower bits are used for snd channels)
#define CHAN_NO_PHS_ADD	(1<<3)	// send to all clients, not just ones in PHS (ATTN 0 will also do this)
#define CHAN_RELIABLE	(1<<4)	// send by reliable message, not datagram

typedef struct
{
	byte	mouthopen;	// 0 = mouth closed, 255 = mouth agape
	byte	sndcount;		// counter for running average
	int	sndavg;		// running average
} mouth_t;

typedef struct
{
	// requested outputs ( NULL == not requested )
	float	*pOrigin;		// vec3_t
	float	*pAngles;		// vec3_t
	float	*pflRadius;	// vec_t
} soundinfo_t;

/*
==============================================================================

VSOUND.DLL INTERFACE
==============================================================================
*/
typedef struct vsound_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vsound_api_t)
	size_t	com_size;		// must matched with sizeof(stdlib_api_t)

	bool (*Init)( void *hInst );	// init sound
	void (*Shutdown)( void );

	// sound manager
	void (*BeginRegistration)( void );
	sound_t (*RegisterSound)( const char *name );
	void (*EndRegistration)( void );

	void (*StartSound)( const vec3_t pos, int ent, int chan, sound_t sfx, float vol, float attn, int pitch, int flags );
	void (*StreamRawSamples)( int samples, int rate, int width, int channels, const byte *data );
	bool (*StartLocalSound)( const char *name, float volume, int pitch, const float *origin );
	void (*FadeClientVolume)( float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds );
	void (*StartBackgroundTrack)( const char *introTrack, const char *loopTrack );
	void (*StopBackgroundTrack)( void );

	void (*StartStreaming)( void );
	void (*StopStreaming)( void );

	void (*RenderFrame)( ref_params_t *fd );
	void (*StopSound)( int entnum, int channel );
	void (*StopAllSounds)( void );
	void (*FreeSounds)( void );

	void (*Activate)( bool active );

} vsound_exp_t;

typedef struct vsound_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vsound_imp_t)

	trace_t (*TraceLine)( const vec3_t start, const vec3_t end );
	bool (*GetEntitySpatialization)( int entnum, soundinfo_t *info );
	void (*GetSoundSpatialization)( int entnum, vec3_t origin, vec3_t velocity );
	int  (*PointContents)( const vec3_t point );
	edict_t *(*GetClientEdict)( int index );
	mouth_t *(*GetEntityMouth)( edict_t *ent );
	int  (*GetServerTime)( void );
	bool (*IsInGame)( void );	// returns false for menu, console, etc
	bool (*IsActive)( void );	// returns true when client is completely in-game
} vsound_imp_t;

#endif//VSOUND_API_H