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
=============
Sound_SaveWAV
=============
*/
bool Sound_SaveWAV( const char *name, wavdata_t *pix )
{
	// FIXME: implement
	return false;
}