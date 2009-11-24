//=======================================================================
//			Copyright XashXT Group 2007 ©
//			 s_load.c - sound managment
//=======================================================================

#include "sound.h"
#include "byteorder.h"

// during registration it is possible to have more sounds
// than could actually be referenced during gameplay,
// because we don't want to free anything until we are
// sure we won't need it.
#define MAX_SFX		4096
#define MAX_SFX_HASH	(MAX_SFX/4)

static sfx_t	s_knownSfx[MAX_SFX];
static sfx_t	*s_sfxHashList[MAX_SFX_HASH];
static int	s_numSfx = 0;
bool		s_registering = false;
int		s_registration_sequence = 0;

typedef struct loadformat_s
{
	char *formatstring;
	char *ext;
	bool (*loadfunc)( const char *name, byte **wav, wavinfo_t *info );
} loadformat_t;

/*
=================
S_SoundList_f
=================
*/
void S_SoundList_f( void )
{
	int		i;
	sfx_t		*sfx;
	sfxcache_t	*sc;
	int		size, totalSfx = 0;
	int		totalSize = 0;

	for( i = 0, sfx = s_knownSfx; i < s_numSfx; i++, sfx++ )
	{
		if( !sfx->touchFrame )
			continue;

		sc = sfx->cache;
		if( sc )
		{
			size = sc->length * sc->width * (sc->stereo + 1);
			totalSize += size;
			if( sc->loopstart >= 0 ) Msg( "L" );
			else Msg( " " );

			if( sfx->name[0] == '#' )
				Msg( " (%2db) %s : %s\n", sc->width * 8, memprint( size ), &sfx->name[1] );
			else Msg( " (%2db) %s : sound/%s\n", sc->width * 8, memprint( size ), sfx->name );
			totalSfx++;
		}
	}

	Msg("-------------------------------------------\n");
	Msg("%i total sounds\n", totalSfx );
	Msg("%s total memory\n", memprint( totalSize ));
	Msg("\n");
}

/*
================
S_ResampleSfx
================
*/
void S_ResampleSfx( sfx_t *sfx, int inrate, int inwidth, byte *data )
{
	float		stepscale;
	int		outcount, srcsample;
	int		i, sample, samplefrac, fracstep;
	sfxcache_t	*sc;

	if( !sfx ) return;	
	sc = sfx->cache;
	if( !sc ) return;

	stepscale = (float)inrate / dma.speed;	// this is usually 0.5, 1, or 2

	outcount = sc->length / stepscale;
	sc->length = outcount;
	if( sc->loopstart != -1 )
		sc->loopstart = sc->loopstart / stepscale;

	sc->speed = dma.speed;
	if( s_loadas8bit->integer )
		sc->width = 1;
	else sc->width = inwidth;
	sc->stereo = 0;

	// resample / decimate to the current source rate
	if( stepscale == 1 && inwidth == 1 && sc->width == 1 )
	{
		// fast special case
		for( i = 0; i < outcount; i++ )
			((signed char *)sc->data)[i] = (int)((unsigned char)(data[i]) - 128);
	}
	else
	{
		// general case
		samplefrac = 0;
		fracstep = stepscale * 256;
		for( i = 0; i < outcount; i++ )
		{
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;

			if( inwidth == 2 ) sample = LittleShort(((short *)data)[srcsample] );
			else sample = (int)( (unsigned char)(data[srcsample]) - 128) << 8;

			if( sc->width == 2 ) ((short *)sc->data)[i] = sample;
			else ((signed char *)sc->data)[i] = sample >> 8;
		}
	}
}

// return true if char 'c' is one of 1st 2 characters in pch
bool S_TestSoundChar( const char *pch, char c )
{
	int	i;
	char	*pcht = (char *)pch;

	// check first 2 characters
	for( i = 0; i < 2; i++ )
	{
		if( *pcht == c )
			return true;
		pcht++;
	}
	return false;
}

/*
===============================================================================

WAV loading

===============================================================================
*/
static byte *iff_data;
static byte *iff_dataPtr;
static byte *iff_end;
static byte *iff_lastChunk;
static int iff_chunkLen;

/*
=================
S_GetLittleShort
=================
*/
static short S_GetLittleShort( void )
{
	short	val = 0;

	val += (*(iff_dataPtr+0) << 0);
	val += (*(iff_dataPtr+1) << 8);
	iff_dataPtr += 2;

	return val;
}

/*
=================
S_GetLittleLong
=================
*/
static int S_GetLittleLong( void )
{
	int	val = 0;

	val += (*(iff_dataPtr+0) << 0);
	val += (*(iff_dataPtr+1) << 8);
	val += (*(iff_dataPtr+2) << 16);
	val += (*(iff_dataPtr+3) << 24);
	iff_dataPtr += 4;

	return val;
}

/*
=================
S_FindNextChunk
=================
*/
static void S_FindNextChunk( const char *name )
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
		iff_chunkLen = S_GetLittleLong();
		if (iff_chunkLen < 0)
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
S_FindChunk
=================
*/
static void S_FindChunk( const char *name )
{
	iff_lastChunk = iff_data;
	S_FindNextChunk( name );
}

/*
=================
S_LoadWAV
=================
*/
static bool S_LoadWAV( const char *name, byte **wav, wavinfo_t *info )
{
	byte	*buffer, *out;
	int	length, samples;

	buffer = FS_LoadFile( name, &length );
	if( !buffer ) return false;

	iff_data = buffer;
	iff_end = buffer + length;

	// dind "RIFF" chunk
	S_FindChunk( "RIFF" );
	if( !( iff_dataPtr && !com.strncmp( iff_dataPtr + 8, "WAVE", 4 )))
	{
		MsgDev( D_WARN, "S_LoadWAV: missing 'RIFF/WAVE' chunks (%s)\n", name );
		Mem_Free( buffer );
		return false;
	}

	// get "fmt " chunk
	iff_data = iff_dataPtr + 12;
	S_FindChunk( "fmt " );
	if( !iff_dataPtr )
	{
		MsgDev( D_WARN, "S_LoadWAV: missing 'fmt ' chunk (%s)\n", name );
		Mem_Free( buffer );
		return false;
	}

	iff_dataPtr += 8;
	if( S_GetLittleShort() != 1 )
	{
		MsgDev( D_WARN, "S_LoadWAV: microsoft PCM format only (%s)\n", name );
		Mem_Free( buffer );
		return false;
	}

	info->channels = S_GetLittleShort();
	if( info->channels != 1 )
	{
		MsgDev( D_WARN, "S_LoadWAV: only mono WAV files supported (%s)\n", name );
		Mem_Free( buffer );
		return false;
	}

	info->rate = S_GetLittleLong();
	iff_dataPtr += 4+2;

	info->width = S_GetLittleShort() / 8;
	if( info->width != 1 && info->width != 2 )
	{
		MsgDev( D_WARN, "S_LoadWAV: only 8 and 16 bit WAV files supported (%s)\n", name );
		Mem_Free( buffer );
		return false;
	}

	// get cue chunk
	S_FindChunk( "cue " );
	if( iff_dataPtr )
	{
		iff_dataPtr += 32;
		info->loopstart = S_GetLittleLong();
		S_FindNextChunk( "LIST" ); // if the next chunk is a LIST chunk, look for a cue length marker
		if( iff_dataPtr )
		{
			if( !com.strncmp((const char *)iff_dataPtr + 28, "mark", 4 ))
			{	
				// this is not a proper parse, but it works with CoolEdit...
				iff_dataPtr += 24;
				info->samples = info->loopstart + S_GetLittleLong(); // samples in loop
			}
		}
	}
	else 
	{
		info->loopstart = -1;
		info->samples = 0;
	}

	// find data chunk
	S_FindChunk( "data" );
	if( !iff_dataPtr )
	{
		MsgDev( D_WARN, "S_LoadWAV: missing 'data' chunk (%s)\n", name );
		Mem_Free( buffer );
		return false;
	}

	iff_dataPtr += 4;
	samples = S_GetLittleLong() / info->width;

	if( info->samples )
	{
		if( samples < info->samples )
		{
			MsgDev( D_ERROR, "S_LoadWAV: %s has a bad loop length\n", name );
			Mem_Free( buffer );
			return false;
		}
	}
	else info->samples = samples;

	if( info->samples <= 0 )
	{
		MsgDev( D_WARN, "S_LoadWAV: file with %i samples (%s)\n", info->samples, name );
		Mem_Free( buffer );
		return false;
	}

	// Load the data
	*wav = out = Z_Malloc( info->samples * info->width );
	Mem_Copy( out, buffer + (iff_dataPtr - buffer), info->samples * info->width );
	Mem_Free( buffer );

	return true;
}

/*
=================
S_UploadSound
=================
*/
static void S_UploadSound( byte *data, wavinfo_t *info, sfx_t *sfx )
{
	sfxcache_t	*sc;
	size_t		size, samples;
	float		stepscale;
	
	// calculate buffer size
	stepscale = (float)info->rate / dma.speed;	
	samples = info->samples / stepscale;
	size = samples * info->width * info->channels;

	sc = sfx->cache = Z_Malloc( size + sizeof( sfxcache_t ));
	sc->length = info->samples;
	sc->loopstart = info->loopstart;
	sc->speed = info->rate;
	sc->width = info->width;
	sc->stereo = info->channels;

	S_ResampleSfx( sfx, sc->speed, sc->width, data + info->dataofs );
}

/*
=================
S_CreateDefaultSound
=================
*/
static void S_CreateDefaultSound( byte **wav, wavinfo_t *info )
{
	byte	*out;
	int	i;

	info->rate = 22050;
	info->width = 2;
	info->channels = 1;
	info->samples = 11025;

	*wav = out = Z_Malloc( info->samples * info->width );

	if( s_check_errors->integer )
	{
		// create 1 kHz tone as default sound
		for( i = 0; i < info->samples; i++ )
			((short *)out)[i] = com.sin( i * 0.1f ) * 20000;
	}
	else
	{
		// create silent sound
		for( i = 0; i < info->samples; i++ )
			((short *)out)[i] =  i;
	}
}

/*
=================
S_LoadSound
=================
*/
loadformat_t load_formats[] =
{
{ "sound/%s.%s", "wav", S_LoadWAV },
{ "%s.%s", "wav", S_LoadWAV },
{ NULL, NULL }
};

sfxcache_t *S_LoadSound( sfx_t *sfx )
{
	byte		*data;
	wavinfo_t		info;
	const char	*ext;
	string		loadname, path;
	loadformat_t	*format;
	bool		anyformat;

	if( !sfx ) return NULL;
	if( sfx->name[0] == '*' ) return NULL;
	if( sfx->cache ) return sfx->cache; // see if still in memory

	// load it from disk
	ext = FS_FileExtension( sfx->name );
	anyformat = !com.stricmp( ext, "" ) ? true : false;

	com.strncpy( loadname, sfx->name, sizeof( loadname ));
	FS_StripExtension( loadname ); // remove extension if needed
	Mem_Set( &info, 0, sizeof( info ));

	// developer warning
	if( !anyformat ) MsgDev( D_NOTE, "Note: %s will be loading only with ext .%s\n", loadname, ext );

	// now try all the formats in the selected list
	for( format = load_formats; format->formatstring; format++ )
	{
		if( anyformat || !com.stricmp( ext, format->ext ))
		{
			com.sprintf( path, format->formatstring, loadname, format->ext );
			if( format->loadfunc( path, &data, &info ))
				goto snd_loaded;
		}
	}

	sfx->default_sound = true;
	MsgDev( D_WARN, "FS_LoadSound: couldn't load %s\n", sfx->name );
	S_CreateDefaultSound( &data, &info );
	info.loopstart = -1;

snd_loaded:

	// load it in
	S_UploadSound( data, &info, sfx );
	Mem_Free( data );

	return sfx->cache;
}

// =======================================================================
// Load a sound
// =======================================================================
/*
==================
S_FindSound

==================
*/
sfx_t *S_FindSound( const char *name )
{
	int	i;
	sfx_t	*sfx;
	uint	hash;

	if( !name || !name[0] ) return NULL;
	if( com.strlen( name ) >= MAX_STRING )
	{
		MsgDev( D_ERROR, "S_FindSound: sound name too long: %s", name );
		return NULL;
	}

	// see if already loaded
	hash = Com_HashKey( name, MAX_SFX_HASH );
	for( sfx = s_sfxHashList[hash]; sfx; sfx = sfx->hashNext )
	{
		if( !com.strcmp( sfx->name, name ))
		{
			// prolonge registration
			sfx->touchFrame = s_registration_sequence;
			return sfx;
		}
	}

	// find a free sfx slot spot
	for( i = 0, sfx = s_knownSfx; i < s_numSfx; i++, sfx++)
	{
		if( !sfx->name[0] ) break; // free spot
	}
	if( i == s_numSfx )
	{
		if( s_numSfx == MAX_SFX )
		{
			MsgDev( D_ERROR, "S_FindName: MAX_SFX limit exceeded\n" );
			return NULL;
		}
		s_numSfx++;
	}
	
	sfx = &s_knownSfx[i];
	Mem_Set( sfx, 0, sizeof( *sfx ));
	com.strncpy( sfx->name, name, MAX_STRING );
	sfx->touchFrame = s_registration_sequence;
	sfx->hashValue = Com_HashKey( sfx->name, MAX_SFX_HASH );

	// link it in
	sfx->hashNext = s_sfxHashList[sfx->hashValue];
	s_sfxHashList[sfx->hashValue] = sfx;
		
	return sfx;
}

/*
==================
S_FreeSound
==================
*/
static void S_FreeSound( sfx_t *sfx )
{
	sfx_t	*hashSfx;
	sfx_t	**prev;

	if( !sfx || !sfx->name[0] ) return;

	// de-link it from the hash tree
	prev = &s_sfxHashList[sfx->hashValue];
	while( 1 )
	{
		hashSfx = *prev;
		if( !hashSfx )
			break;

		if( hashSfx == sfx )
		{
			*prev = hashSfx->hashNext;
			break;
		}
		prev = &hashSfx->hashNext;
	}

	if( sfx->cache ) Mem_Free( sfx->cache );
	Mem_Set( sfx, 0, sizeof( *sfx ));
}

/*
=====================
S_BeginRegistration

=====================
*/
void S_BeginRegistration( void )
{
	s_registration_sequence++;
	s_registering = true;
}

/*
=====================
S_EndRegistration

=====================
*/
void S_EndRegistration( void )
{
	sfx_t	*sfx;
	int	i;

	// free any sounds not from this registration sequence
	for( i = 0, sfx = s_knownSfx; i < s_numSfx; i++, sfx++ )
	{
		if( !sfx->name[0] ) continue;
		if( sfx->touchFrame != s_registration_sequence )
			S_FreeSound( sfx ); // don't need this sound
	}

	// load everything in
	for( i = 0, sfx = s_knownSfx; i < s_numSfx; i++, sfx++ )
	{
		if( !sfx->name[0] ) continue;
		S_LoadSound( sfx );
	}
	s_registering = false;
}

/*
==================
S_RegisterSound

==================
*/
sound_t S_RegisterSound( const char *name )
{
	sfx_t	*sfx;

	if( !sound_started )
		return -1;

	sfx = S_FindSound( name );
	if( !sfx ) return -1;

	sfx->touchFrame = s_registration_sequence;
	if( !s_registering ) S_LoadSound( sfx );

	return sfx - s_knownSfx;
}

sfx_t *S_GetSfxByHandle( sound_t handle )
{
	if( handle < 0 || handle >= s_numSfx )
	{
		MsgDev( D_ERROR, "S_GetSfxByHandle: handle %i out of range (%i)\n", handle, s_numSfx );
		return NULL;
	}
	return &s_knownSfx[handle];
}

/*
=================
S_FreeSounds
=================
*/
void S_FreeSounds( void )
{
	sfx_t	*sfx;
	int	i;

	// stop all sounds
	S_StopAllSounds();

	// free all sounds
	for( i = 0, sfx = s_knownSfx; i < s_numSfx; i++, sfx++ )
		S_FreeSound( sfx );

	s_numSfx = 0;
	Mem_Set( s_knownSfx, 0, sizeof( s_knownSfx ));
	Mem_Set( s_sfxHashList, 0, sizeof( s_sfxHashList ));
}