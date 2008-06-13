//=======================================================================
//			Copyright XashXT Group 2007 ©
//			 s_load.c - sound managment
//=======================================================================

#include "sound.h"

#define MAX_SFX		4096
static sfx_t s_knownSfx[MAX_SFX];
static int s_numSfx = 0;

#define SFX_HASHSIZE	128
static sfx_t *sfxHash[SFX_HASHSIZE];

/*
=================
S_SoundList_f
=================
*/
void S_SoundList_f( void )
{
	sfx_t	*sfx;
	int	i, samples = 0;

	Msg("\n");
	Msg("      -samples -hz-- -format- -name--------\n");

	for( i = 0; i < s_numSfx; i++ )
	{
		sfx = &s_knownSfx[i];
		Msg("%4i: ", i);

		if( sfx->loaded )
		{
			samples += sfx->samples;
			Msg("%8i ", sfx->samples);
			Msg("%5i ", sfx->rate);

			switch( sfx->format )
			{
			case AL_FORMAT_STEREO16: Msg("STEREO16 "); break;
			case AL_FORMAT_STEREO8:  Msg("STEREO8  "); break;
			case AL_FORMAT_MONO16:   Msg("MONO16   "); break;
			case AL_FORMAT_MONO8:    Msg("MONO8    "); break;
			default: Msg("???????? "); break;
			}

			if( sfx->name[0] == '#' ) Msg("%s", &sfx->name[1]);
			else Msg("sound/%s", sfx->name);

			if( sfx->default_snd ) Msg(" (DEFAULTED)\n");
			else Msg("\n");
		}
		else
		{
			if( sfx->name[0] == '*' ) Msg("      placeholder       %s\n", sfx->name);
			else Msg("      not loaded        %s\n", sfx->name);
		}
	}

	Msg("-------------------------------------------\n");
	Msg("%i total samples\n", samples);
	Msg("%i total sounds\n", s_numSfx);
	Msg("\n");
}

/*
 =======================================================================

 WAV LOADING

 =======================================================================
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
	int	length;

	buffer = FS_LoadFile( name, &length );
	if( !buffer ) return false;

	iff_data = buffer;
	iff_end = buffer + length;

	// dind "RIFF" chunk
	S_FindChunk( "RIFF" );
	if(!(iff_dataPtr && !com.strncmp(iff_dataPtr+8, "WAVE", 4)))
	{
		MsgDev( D_WARN, "S_LoadWAV: missing 'RIFF/WAVE' chunks (%s)\n", name );
		Mem_Free( buffer );
		return false;
	}

	// get "fmt " chunk
	iff_data = iff_dataPtr + 12;
	S_FindChunk("fmt ");
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

	// Find data chunk
	S_FindChunk("data");
	if( !iff_dataPtr )
	{
		MsgDev( D_WARN, "S_LoadWAV: missing 'data' chunk (%s)\n", name );
		Mem_Free( buffer );
		return false;
	}

	iff_dataPtr += 4;
	info->samples = S_GetLittleLong() / info->width;

	if( info->samples <= 0 )
	{
		MsgDev( D_WARN, "S_LoadWAV: file with 0 samples (%s)\n", name);
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
static void S_UploadSound( byte *data, int width, int channels, sfx_t *sfx )
{
	int	size;

	// calculate buffer size
	size = sfx->samples * width * channels;

	// Set buffer format
	if( width == 2 )
	{
		if( channels == 2 ) sfx->format = AL_FORMAT_STEREO16;
		else sfx->format = AL_FORMAT_MONO16;
	}
	else
	{
		if( channels == 2 ) sfx->format = AL_FORMAT_STEREO8;
		else sfx->format = AL_FORMAT_MONO8;
	}

	// upload the sound
	palGenBuffers(1, &sfx->bufferNum);
	palBufferData( sfx->bufferNum, sfx->format, data, size, sfx->rate );
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
	for( i = 0; i < info->samples; i++ )
		((short *)out)[i] = sin(i * 0.1f) * 20000;
}

/*
=================
S_LoadSound
=================
*/
bool S_LoadSound( sfx_t *sfx )
{
	string		name;
	byte		*data;
	wavinfo_t		info;

	if( !sfx ) return false;
	if( sfx->name[0] == '*' ) return false;
	if( sfx->loaded ) return true;	// see if still in memory

	// load it from disk
	if( sfx->name[0] == '#' )
		com.snprintf( name, sizeof(name), "%s", &sfx->name[1]);
	else com.snprintf( name, sizeof(name), "sound/%s", sfx->name );

	if(!S_LoadWAV(name, &data, &info))
	{
		sfx->default_snd = true;

		MsgWarn( "couldn't find sound '%s', using default...\n", name );
		S_CreateDefaultSound( &data, &info );
	}

	// load it in
	sfx->loaded = true;
	sfx->samples = info.samples;
	sfx->rate = info.rate;

	S_UploadSound( data, info.width, info.channels, sfx );
	Mem_Free(data);

	return true;
}

// =======================================================================
// Load a sound
// =======================================================================
/*
================
return a hash value for the sfx name
================
*/
static long S_HashSFXName( const char *name )
{
	int	i = 0;
	long	hash = 0;
	char	letter;

	while( name[i] != '\0' )
	{
		letter = com.tolower(name[i]);
		if (letter =='.') break;		// don't include extension
		if (letter =='\\') letter = '/';	// damn path names
		hash += (long)(letter)*(i+119);
		i++;
	}
	hash &= (SFX_HASHSIZE-1);
	return hash;
}

/*
=================
S_FindSound
=================
*/
sfx_t *S_FindSound( const char *name )
{
	int	i, hash;
	sfx_t	*sfx;

	if( !name || !name[0] )
	{
		MsgWarn("S_FindSound: empty name\n");
		return NULL;
	}
	if( com.strlen(name) >= MAX_STRING )
	{
		MsgWarn("S_FindSound: sound name too long: %s", name);
		return NULL;
	}

	hash = S_HashSFXName(name);
	sfx = sfxHash[hash];

	// see if already loaded
	while( sfx )
	{
		if (!stricmp(sfx->name, name))
			return sfx;
		sfx = sfx->nextHash;
	}

	// find a free sfx slot
	for (i = 0; i < s_numSfx; i++)
	{
		if(!s_knownSfx[i].name[0])
			break;
	}

	if (i == s_numSfx)
	{
		if (s_numSfx == MAX_SFX)
		{
			MsgWarn("S_FindName: MAX_SFX limit exceeded\n");
			return NULL;
		}
		s_numSfx++;
	}

	sfx = &s_knownSfx[i];
	memset (sfx, 0, sizeof(*sfx));
	strcpy (sfx->name, name);
	sfx->nextHash = sfxHash[hash];
	sfxHash[hash] = sfx;

	return sfx;
}

/*
=====================
S_BeginRegistration
=====================
*/
void S_BeginRegistration( void )
{
}

/*
=====================
S_EndRegistration
=====================
*/
void S_EndRegistration( void )
{
}

/*
=================
S_RegisterSound
=================
*/
sound_t S_RegisterSound( const char *name )
{
	sfx_t	*sfx;

	if(!al_state.initialized)
		return 0;

	sfx = S_FindSound( name );
	S_LoadSound( sfx );

	if( !sfx ) return 0;
	return sfx - s_knownSfx;
}

sfx_t *S_GetSfxByHandle( sound_t handle )
{
	if( handle < 0 || handle >= s_numSfx )
	{
		MsgWarn("S_GetSfxByHandle: handle %i out of range (%i)\n", handle, s_numSfx );
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
	int		i;

	// Stop all sounds
	S_StopAllSounds();

	// Free all sounds
	for (i = 0; i < s_numSfx; i++)
	{
		sfx = &s_knownSfx[i];
		palDeleteBuffers(1, &sfx->bufferNum);
	}

	memset( sfxHash, 0, sizeof(sfx_t *)*SFX_HASHSIZE);
	memset( s_knownSfx, 0, sizeof(s_knownSfx));

	s_numSfx = 0;
}
