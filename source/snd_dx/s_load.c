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

static int	s_numSfx = 0;
static sfx_t	s_knownSfx[MAX_SFX];
static sfx_t	*s_sfxHashList[MAX_SFX_HASH];
bool		s_registering = false;
int		s_registration_sequence = 0;

/*
=================
S_SoundList_f
=================
*/
void S_SoundList_f( void )
{
	sfx_t		*sfx;
	wavdata_t		*sc;
	int		i, totalSfx = 0;
	int		totalSize = 0;

	for( i = 0, sfx = s_knownSfx; i < s_numSfx; i++, sfx++ )
	{
		if( !sfx->touchFrame )
			continue;

		sc = sfx->cache;
		if( sc )
		{
			totalSize += sc->size;

			if( sc->loopStart >= 0 ) Msg( "L" );
			else Msg( " " );
			Msg( " (%2db) %s : sound/%s\n", sc->width * 8, memprint( sc->size ), sfx->name );
			totalSfx++;
		}
	}

	Msg( "-------------------------------------------\n" );
	Msg( "%i total sounds\n", totalSfx );
	Msg( "%s total memory\n", memprint( totalSize ));
	Msg( "\n" );
}

/*
================
S_ResampleSfx
================
*/
void S_ResampleSfx( sfx_t *sfx, int inrate, int inwidth, byte *data )
{
	float	stepscale;
	int	outcount, srcsample;
	int	i, sample, samplefrac, fracstep;
	wavdata_t	*sc;

	if( !sfx ) return;	
	sc = sfx->cache;
	if( !sc ) return;

	stepscale = (float)inrate / dma.speed;	// this is usually 0.5, 1, or 2

	outcount = sc->samples / stepscale;
	sc->samples = outcount;
	if( sc->loopStart != -1 )
		sc->loopStart = sc->loopStart / stepscale;

	sc->rate = dma.speed;
	sc->width = inwidth;
	sc->channels = 1;	// default to Mono

	// resample / decimate to the current source rate
	if( stepscale == 1 && inwidth == 1 && sc->width == 1 )
	{
		// fast special case
		for( i = 0; i < outcount; i++ )
			((signed char *)sc->buffer)[i] = (int)((unsigned char)(data[i]) - 128);
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

			if( sc->width == 2 ) ((short *)sc->buffer)[i] = sample;
			else ((signed char *)sc->buffer)[i] = sample >> 8;
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

// return pointer to first valid character in file name
char *S_SkipSoundChar( const char *pch )
{
	char *pcht = (char *)pch;

	// check first character
	if( *pcht == '!' )
		pcht++;
	return pcht;
}

/*
=================
S_UploadSound
=================
*/
static void S_UploadSound( wavdata_t *sc, sfx_t *sfx )
{
	size_t	samples;
	float	stepscale;
	byte	*indata;
	
	// calculate buffer size
	stepscale = (float)sc->rate / dma.speed;	
	samples = sc->samples / stepscale;
	sc->size = samples * sc->width * sc->channels;
	indata = sc->buffer; // detach old buffer
	sfx->cache = sc;

	// UNDONE: we need resample sounds in launch.dll, not here
	sc->buffer = Z_Malloc( sc->size ); // allocate room for resampled wav

	S_ResampleSfx( sfx, sc->rate, sc->width, indata );
	Mem_Free( indata );	// no reason to keep this data
}

/*
=================
S_CreateDefaultSound
=================
*/
static wavdata_t *S_CreateDefaultSound( void )
{
	wavdata_t	*sc;
	int	i;

	sc = Z_Malloc( sizeof( wavdata_t ));

	sc->rate = 22050;
	sc->width = 2;
	sc->channels = 1;
	sc->samples = 11025;
	sc->loopStart = -1;
	sc->size = sc->samples * sc->width * sc->channels;
	sc->buffer = Z_Malloc( sc->size );

	if( s_check_errors->integer )
	{
		// create 1 kHz tone as default sound
		for( i = 0; i < sc->samples; i++ )
			((short *)sc->buffer)[i] = com.sin( i * 0.1f ) * 20000;
	}
	else
	{
		// create silent sound
		for( i = 0; i < sc->samples; i++ )
			((short *)sc->buffer)[i] =  i;
	}

	return sc;
}

/*
=================
S_LoadSound
=================
*/
wavdata_t *S_LoadSound( sfx_t *sfx )
{
	wavdata_t	*sc;

	if( !sfx ) return NULL;
	if( sfx->name[0] == '*' ) return NULL;
	if( sfx->cache ) return sfx->cache; // see if still in memory

	// load it from disk
	sc = FS_LoadSound( sfx->name, NULL, 0 );

	if( sc == NULL )
	{
		sc = S_CreateDefaultSound();
		sfx->default_sound = true;
	}

	// upload and resample
	S_UploadSound( sc, sfx );

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

	if( sfx->cache ) FS_FreeSound( sfx->cache );
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

	Mem_Set( s_knownSfx, 0, sizeof( s_knownSfx ));
	Mem_Set( s_sfxHashList, 0, sizeof( s_sfxHashList ));

	s_numSfx = 0;
}