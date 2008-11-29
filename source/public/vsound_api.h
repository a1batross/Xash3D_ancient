//=======================================================================
//			Copyright XashXT Group 2008 ©
//		  vsound_api.h - xash sound engine (OpenAL based)
//=======================================================================
#ifndef VSOUND_API_H
#define VSOUND_API_H

// sound channels
typedef enum
{
	CHAN_AUTO = 0,
	CHAN_WEAPON,
	CHAN_VOICE,
	CHAN_ITEM,
	CHAN_BODY,
	CHAN_ANNOUNCER,		// announcer

	// snd flags
	CHAN_NO_PHS_ADD = 8,	// send to all clients, not just ones in PHS (ATTN 0 will also do this)
	CHAN_RELIABLE = 16,		// send by reliable message, not datagram
} snd_channel_t;

// sound attenuation values
typedef enum
{
	ATTN_NONE = 0,		// full volume the entire level
	ATTN_NORM,
	ATTN_IDLE,
	ATTN_STATIC,		// diminish very rapidly with distance
} snd_attenuation_t;

#define PITCH_LOW		95	// other values are possible - 0-255, where 255 is very high
#define PITCH_NORM		100	// non-pitch shifted
#define PITCH_HIGH		120

/*
==============================================================================

VSOUND.DLL INTERFACE
==============================================================================
*/
typedef struct vsound_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vprogs_api_t)

	void (*Init)( void *hInst );	// init host
	void (*Shutdown)( void );	// close host

	// sound manager
	void (*BeginRegistration)( void );
	sound_t (*RegisterSound)( const char *name );
	void (*EndRegistration)( void );

	void (*StartSound)( const vec3_t pos, int ent, int chan, sound_t sfx, float vol, float attn, float pitch, bool loop );
	void (*StreamRawSamples)( int samples, int rate, int width, int channels, const byte *data );
	bool (*AddLoopingSound)( int entnum, sound_t handle, float volume, float attn );
	bool (*StartLocalSound)( const char *name );
	void (*StartBackgroundTrack)( const char *introTrack, const char *loopTrack );
	void (*StopBackgroundTrack)( void );

	void (*StartStreaming)( void );
	void (*StopStreaming)( void );

	void (*Frame)( int entnum, const vec3_t pos, const vec3_t vel, const vec3_t at, const vec3_t up );
	void (*StopAllSounds)( void );
	void (*FreeSounds)( void );

	void (*Activate)( bool active );

} vsound_exp_t;

typedef struct vsound_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vsound_imp_t)

	void (*GetSoundSpatialization)( int entnum, vec3_t origin, vec3_t velocity );
	int  (*PointContents)( const vec3_t point );
	bool (*AmbientLevel)( const vec3_t point, float *volumes );
	void (*AddLoopingSounds)( void );

} vsound_imp_t;

#endif//VSOUND_API_H