//=======================================================================
//			Copyright XashXT Group 2008 ©
//		  vsound_api.h - xash sound engine (OpenAL based)
//=======================================================================
#ifndef VSOUND_API_H
#define VSOUND_API_H

typedef int		sound_t;

// snd internal flags (lower bits are used for snd channels)
#define CHAN_NO_PHS_ADD	(1<<3)	// send to all clients, not just ones in PHS (ATTN 0 will also do this)
#define CHAN_RELIABLE	(1<<4)	// send by reliable message, not datagram

/*
==============================================================================

VSOUND.DLL INTERFACE
==============================================================================
*/
typedef struct vsound_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vprogs_api_t)

	bool (*Init)( void *hInst );	// init sound
	void (*Shutdown)( void );

	// sound manager
	void (*BeginRegistration)( void );
	sound_t (*RegisterSound)( const char *name );
	void (*EndRegistration)( void );

	void (*StartSound)( const vec3_t pos, int ent, int chan, sound_t sfx, float vol, float attn, float pitch, bool loop );
	void (*StreamRawSamples)( int samples, int rate, int width, int channels, const byte *data );
	bool (*AddLoopingSound)( int entnum, sound_t handle, float volume, float attn );
	bool (*StartLocalSound)( const char *name, float volume, float pitch, const float *origin );
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
	void (*AddLoopingSounds)( void );

} vsound_imp_t;

#endif//VSOUND_API_H