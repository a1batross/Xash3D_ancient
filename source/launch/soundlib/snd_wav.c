//=======================================================================
//			Copyright XashXT Group 2010 ©
//			snd_wav.c - wav format load & save
//=======================================================================

#include "soundlib.h"

static const byte *iff_data;
static const byte *iff_dataPtr;
static const byte *iff_end;
static const byte *iff_lastChunk;
static int iff_chunkLen;

/*
=================
GetLittleShort
=================
*/
static short GetLittleShort( void )
{
	short	val = 0;

	val += (*(iff_dataPtr+0) << 0);
	val += (*(iff_dataPtr+1) << 8);
	iff_dataPtr += 2;

	return val;
}

/*
=================
GetLittleLong
=================
*/
static int GetLittleLong( void )
{
	int	val = 0;

	val += (*(iff_dataPtr+0) << 0);
	val += (*(iff_dataPtr+1) << 8);
	val += (*(iff_dataPtr+2) <<16);
	val += (*(iff_dataPtr+3) <<24);
	iff_dataPtr += 4;

	return val;
}

/*
=================
FindNextChunk
=================
*/
static void FindNextChunk( const char *name )
{
	while( 1 )
	{
		iff_dataPtr = iff_lastChunk;

		if( iff_dataPtr >= iff_end )
		{
			// didn't find the chunk
			iff_dataPtr = NULL;
			return;
		}
		
		iff_dataPtr += 4;
		iff_chunkLen = GetLittleLong();

		if( iff_chunkLen < 0 )
		{
			iff_dataPtr = NULL;
			return;
		}

		iff_dataPtr -= 8;
		iff_lastChunk = iff_dataPtr + 8 + ((iff_chunkLen + 1) & ~1);

		if(!com.strncmp( iff_dataPtr, name, 4 ))
			return;
	}
}

/*
============
StreamFindNextChunk
============
*/
bool StreamFindNextChunk( file_t *file, const char *name, int *last_chunk )
{
	char	chunkName[4];
	int	iff_chunk_len;

	while( 1 )
	{
		FS_Seek( file, *last_chunk, SEEK_SET );

		if( FS_Eof( file ))
			return false;	// didn't find the chunk

		FS_Seek( file, 4, SEEK_CUR );
		FS_Read( file, &iff_chunk_len, sizeof( iff_chunk_len ));
		iff_chunk_len = LittleLong( iff_chunk_len );
		if( iff_chunk_len < 0 )
			return false;	// didn't find the chunk

		FS_Seek( file, -8, SEEK_CUR );
		*last_chunk = FS_Tell( file ) + 8 + (( iff_chunk_len + 1 ) & ~1 );
		FS_Read( file, chunkName, 4 );
		if( !com.strncmp( chunkName, name, 4 ))
			return true;
	}
	return false;
}

/*
=================
FindChunk
=================
*/
static void FindChunk( const char *name )
{
	iff_lastChunk = iff_data;
	FindNextChunk( name );
}

/*
=============
Sound_LoadWAV
=============
*/
bool Sound_LoadWAV( const char *name, const byte *buffer, size_t filesize )
{
	int	samples;

	if( !buffer || filesize <= 0 ) return false;

	iff_data = buffer;
	iff_end = buffer + filesize;

	// dind "RIFF" chunk
	FindChunk( "RIFF" );
	if( !( iff_dataPtr && !com.strncmp( iff_dataPtr + 8, "WAVE", 4 )))
	{
		MsgDev( D_ERROR, "Sound_LoadWAV: %s missing 'RIFF/WAVE' chunks\n", name );
		return false;
	}

	// get "fmt " chunk
	iff_data = iff_dataPtr + 12;
	FindChunk( "fmt " );
	if( !iff_dataPtr )
	{
		MsgDev( D_ERROR, "Sound_LoadWAV: %s missing 'fmt ' chunk\n", name );
		return false;
	}

	iff_dataPtr += 8;
	if( GetLittleShort() != 1 )
	{
		MsgDev( D_ERROR, "Sound_LoadWAV: %s not a microsoft PCM format\n", name );
		return false;
	}

	sound.channels = GetLittleShort();
	if( sound.channels != 1 )
	{
		MsgDev( D_ERROR, "Sound_LoadWAV: only mono WAV files supported (%s)\n", name );
		return false;
	}

	sound.rate = GetLittleLong();
	iff_dataPtr += 6;

	sound.width = GetLittleShort() / 8;
	if( sound.width != 1 && sound.width != 2 )
	{
		MsgDev( D_WARN, "Sound_LoadWAV: only 8 and 16 bit WAV files supported (%s)\n", name );
		return false;
	}

	// get cue chunk
	FindChunk( "cue " );

	if( iff_dataPtr )
	{
		iff_dataPtr += 32;
		sound.loopstart = GetLittleLong();
		FindNextChunk( "LIST" ); // if the next chunk is a LIST chunk, look for a cue length marker

		if( iff_dataPtr )
		{
			if( !com.strncmp( iff_dataPtr + 28, "mark", 4 ))
			{	
				// this is not a proper parse, but it works with CoolEdit...
				iff_dataPtr += 24;
				sound.samples = sound.loopstart + GetLittleLong(); // samples in loop
			}
		}
	}
	else 
	{
		sound.loopstart = -1;
		sound.samples = 0;
	}

	// find data chunk
	FindChunk( "data" );
	if( !iff_dataPtr )
	{
		MsgDev( D_WARN, "Sound_LoadWAV: %s missing 'data' chunk\n", name );
		return false;
	}

	iff_dataPtr += 4;
	samples = GetLittleLong() / sound.width;

	if( sound.samples )
	{
		if( samples < sound.samples )
		{
			MsgDev( D_ERROR, "Sound_LoadWAV: %s has a bad loop length\n", name );
			return false;
		}
	}
	else sound.samples = samples;

	if( sound.samples <= 0 )
	{
		MsgDev( D_ERROR, "Sound_LoadWAV: file with %i samples (%s)\n", sound.samples, name );
		return false;
	}

	sound.type = WF_PCMDATA;

	// Load the data
	sound.size = sound.samples * sound.width;
	sound.wav = Mem_Alloc( Sys.soundpool, sound.size );

	Mem_Copy( sound.wav, buffer + (iff_dataPtr - buffer), sound.size );

	return true;
}

/*
=================
Stream_OpenWAV
=================
*/
stream_t *Stream_OpenWAV( const char *filename )
{
	stream_t	*stream;
	int 	iff_data, last_chunk;
	char	chunkName[4];
	file_t	*file;
	short	t;

	if( !filename || !*filename )
		return NULL;

	// open
	file = FS_Open( filename, "rb" );
	if( !file ) return NULL;	

	// find "RIFF" chunk
	if( !StreamFindNextChunk( file, "RIFF", &last_chunk ))
	{
		MsgDev( D_ERROR, "Stream_OpenWAV: %s missing RIFF chunk\n", filename );
		FS_Close( file );
		return NULL;
	}

	FS_Read( file, chunkName, 4 );
	if( !com.strncmp( chunkName, "WAVE", 4 ))
	{
		MsgDev( D_ERROR, "Stream_OpenWAV: %s missing WAVE chunk\n", filename );
		FS_Close( file );
		return NULL;
	}

	// get "fmt " chunk
	iff_data = FS_Tell( file ) + 4;
	last_chunk = iff_data;
	if( !StreamFindNextChunk( file, "fmt ", &last_chunk ))
	{
		MsgDev( D_ERROR, "Stream_OpenWAV: %s missing 'fmt ' chunk\n", filename );
		FS_Close( file );
		return NULL;
	}

	FS_Read( file, chunkName, 4 );

	FS_Read( file, &t, sizeof( t ));
	if( LittleShort( t ) != 1 )
	{
		MsgDev( D_ERROR, "Stream_OpenWAV: %s not a microsoft PCM format\n", filename );
		FS_Close( file );
		return NULL;
	}

	FS_Read( file, &t, sizeof( t ));
	sound.channels = LittleShort( t );

	FS_Read( file, &sound.rate, sizeof( int ));
	sound.rate = LittleLong( sound.rate );

	FS_Seek( file, 6, SEEK_CUR );

	FS_Read( file, &t, sizeof( t ));
	sound.width = LittleShort( t ) / 8;

	sound.loopstart = 0;

	// find data chunk
	last_chunk = iff_data;
	if( !StreamFindNextChunk( file, "data", &last_chunk ))
	{
		MsgDev( D_ERROR, "Stream_OpenWAV: %s missing 'data' chunk\n", filename );
		FS_Close( file );
		return NULL;
	}

	FS_Read( file, &sound.samples, sizeof( int ));
	sound.samples = ( LittleLong( sound.samples ) / sound.width ) / sound.channels;

	// at this point we have valid stream
	stream = Mem_Alloc( Sys.soundpool, sizeof( stream_t ));
	stream->file = file;
	stream->size = sound.samples * sound.width * sound.channels;
	stream->channels = sound.channels;
	stream->width = sound.width;
	stream->rate = sound.rate;
	stream->type = WF_PCMDATA;
	
	return stream;
}

/*
=================
Stream_ReadWAV

assume stream is valid
=================
*/
long Stream_ReadWAV( stream_t *stream, long bytes, void *buffer )
{
	int	samples, remaining;

	if( !stream->file ) return 0;	// invalid file

	remaining = stream->size - stream->pos;
	if( remaining <= 0 ) return 0;
	if( bytes > remaining ) bytes = remaining;

	stream->pos += bytes;
	samples = ( bytes / stream->width ) / stream->channels;
	FS_Read( stream->file, buffer, bytes );
	Sound_ByteSwapRawSamples( samples, stream->width, stream->channels, buffer );

	return bytes;
}

/*
=================
Stream_FreeWAV

assume stream is valid
=================
*/
void Stream_FreeWAV( stream_t *stream )
{
	if( stream->file )
		FS_Close( stream->file );
	Mem_Free( stream );
}

/*
=============
Sound_SaveWAV
=============
*/
bool Sound_SaveWAV( const char *name, wavdata_t *pix )
{
	// FIXME: implement
	return false;
}