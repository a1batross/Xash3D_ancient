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

typedef struct streamformat_s
{
	const char *formatstring;
	const char *ext;

	stream_t *(*openfunc)( const char *filename );
	long (*readfunc)( stream_t *stream, long bytes, void *buffer );
	void (*freefunc)( stream_t *stream );
} streamformat_t;

typedef struct sndlib_s
{
	const loadwavformat_t	*baseformats;	// used for loading internal images
	const loadwavformat_t	*loadformats;
	const savewavformat_t	*saveformats;
	const streamformat_t	*streamformat;	// music stream

	// current sound state
	int		type;		// sound type
	int		rate;		// num samples per second (e.g. 11025 - 11 khz)
	int		width;		// resolution - bum bits divided by 8 (8 bit is 1, 16 bit is 2)
	int		channels;		// num channels (1 - mono, 2 - stereo)
	int		loopstart;	// start looping from
	uint		samples;		// total samplecount in sound
	uint		flags;		// additional sound flags
	size_t		size;		// sound unpacked size (for bounds checking)
	byte		*wav;		// sound pointer (see sound_type for details)

	byte		*tempbuffer;	// for convert operations
	int		cmd_flags;
} sndlib_t;

typedef struct stream_s
{
	const streamformat_t	*format;	// streamformat to operate

	// current stream state
	file_t			*file;	// stream file
	int			width;	// resolution - num bits divided by 8 (8 bit is 1, 16 bit is 2)
	int			rate;	// stream rate
	int			channels;	// stream channels
	int			type;	// wavtype
	size_t			size;	// total stream size
	int			pos;	// keep track wav position
	void			*ptr;
};

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
// stream operate
//
stream_t *Stream_OpenWAV( const char *filename );
long Stream_ReadWAV( stream_t *stream, long bytes, void *buffer );
void Stream_FreeWAV( stream_t *stream );
stream_t *Stream_OpenOGG( const char *filename );
long Stream_ReadOGG( stream_t *stream, long bytes, void *buffer );
void Stream_FreeOGG( stream_t *stream );

//
// formats save
//
bool Sound_SaveWAV( const char *name, wavdata_t *pix );

//
// snd_utils.c
//
void Sound_ByteSwapRawSamples( int samples, int width, int s_channels, const byte *data );

#endif//SOUNDLIB_H