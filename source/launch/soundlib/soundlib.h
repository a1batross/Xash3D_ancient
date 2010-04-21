//=======================================================================
//			Copyright XashXT Group 2010 ©
//			soundlib.h - engine sound lib
//=======================================================================
#ifndef SOUNDLIB_H
#define SOUNDLIB_H

#include "launch.h"
#include "byteorder.h"

typedef struct loadwavformat_s
{
	const char *formatstring;
	const char *ext;
	bool (*loadfunc)( const char *name, const byte *buffer, size_t filesize );
} loadwavformat_t;

typedef struct savewavformat_s
{
	const char *formatstring;
	const char *ext;
	bool (*savefunc)( const char *name, wavdata_t *pix );
} savewavformat_t;

typedef struct sndlib_s
{
	const loadwavformat_t	*baseformats;	// used for loading internal images
	const loadwavformat_t	*loadformats;
	const savewavformat_t	*saveformats;

	// current sound state
	int		type;		// sound type
	word		rate;		// num samples per second (e.g. 11025 - 11 khz)
	word		width;		// resolution - bum bits divided by 8 (8 bit is 1, 16 bit is 2)
	word		channels;		// num channels (1 - mono, 2 - stereo)
	int		loopstart;	// start looping from
	uint		samples;		// total samplecount in sound
	uint		flags;		// additional sound flags
	size_t		size;		// sound unpacked size (for bounds checking)
	byte		*wav;		// sound pointer (see sound_type for details)

	byte		*tempbuffer;	// for convert operations
	int		cmd_flags;
} sndlib_t;

/*
========================================================================

.OGG sound format	(OGG Vorbis)

========================================================================
*/
// defined in sound_ogg.c

extern sndlib_t sound;
//
// formats load
//
bool Sound_LoadWAV( const char *name, const byte *buffer, size_t filesize );
bool Sound_LoadOGG( const char *name, const byte *buffer, size_t filesize );
bool Sound_LoadSND( const char *name, const byte *buffer, size_t filesize );	// snd - doom1 sounds

//
// formats save
//
bool Sound_SaveWAV( const char *name, wavdata_t *pix );

#endif//SOUNDLIB_H