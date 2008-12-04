//=======================================================================
//			Copyright XashXT Group 2007 ©
//			 s_load.c - sound managment
//=======================================================================

#include "sound.h"
#include "s_stream.h"

#define MAX_SFX		4096
sound_t ambient_sfx[NUM_AMBIENTS];
static sfx_t s_knownSfx[MAX_SFX];
static int s_numSfx = 0;
int s_registration_sequence = 0;
bool s_registering = false;

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
			Msg( "\n" );
		}
		else
		{
			if( sfx->name[0] == '*' ) Msg("      placeholder       %s\n", sfx->name);
			else Msg("      not loaded        %s\n", sfx->name);
		}
	}

	Msg("-------------------------------------------\n");
	Msg("%i total samples\n", samples );
	Msg("%i total sounds\n", s_numSfx );
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
	int	length, samples;

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

	// get cue chunk
	S_FindChunk("cue ");
	if( iff_dataPtr )
	{
		iff_dataPtr += 32;
		info->loopstart = S_GetLittleLong();
		S_FindNextChunk("LIST"); // if the next chunk is a LIST chunk, look for a cue length marker
		if( iff_dataPtr )
		{
			if(!com.strncmp ((const char *)iff_dataPtr + 28, "mark", 4))
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
	S_FindChunk("data");
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
			return false;
		}
	}
	else info->samples = samples;

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
 =======================================================================

 OGG LOADING

 =======================================================================
*/
typedef struct
{
	byte		*buffer;
	ogg_int64_t	ind;
	ogg_int64_t	buffsize;
} ov_decode_t;

static size_t ovc_read( void *ptr, size_t size, size_t nb, void *datasource )
{
	ov_decode_t *sound = (ov_decode_t *)datasource;
	size_t	remain, length;

	remain = sound->buffsize - sound->ind;
	length = size * nb;
	if( remain < length ) length = remain - remain % size;

	Mem_Copy( ptr, sound->buffer + sound->ind, length );
	sound->ind += length;

	return length / size;
}

static int ovc_seek ( void *datasource, ogg_int64_t offset, int whence )
{
	ov_decode_t *sound = (ov_decode_t*)datasource;

	switch( whence )
	{
		case SEEK_SET:
			break;
		case SEEK_CUR:
			offset += sound->ind;
			break;
		case SEEK_END:
			offset += sound->buffsize;
			break;
		default:
			return -1;
	}
	if( offset < 0 || offset > sound->buffsize )
		return -1;

	sound->ind = offset;
	return 0;
}

static int ovc_close( void *datasource )
{
	return 0;
}

static long ovc_tell (void *datasource)
{
	return ((ov_decode_t*)datasource)->ind;
}

/*
=================
S_LoadOGG
=================
*/
static bool S_LoadOGG( const char *name, byte **wav, wavinfo_t *info )
{
	vorbisfile_t	vf;
	vorbis_info_t	*vi;
	vorbis_comment_t	*vc;
	fs_offset_t	filesize;
	ov_decode_t	ov_decode;
	ogg_int64_t	length, done = 0;
	ov_callbacks_t	ov_callbacks = { ovc_read, ovc_seek, ovc_close, ovc_tell };
	byte		*data, *buffer;
	const char	*comm;
	int		dummy;
	long		ret;

	// load the file
	data = FS_LoadFile( name, &filesize );
	if( !data ) return false;

	// Open it with the VorbisFile API
	ov_decode.buffer = data;
	ov_decode.ind = 0;
	ov_decode.buffsize = filesize;
	if( ov_open_callbacks( &ov_decode, &vf, NULL, 0, ov_callbacks ) < 0 )
	{
		MsgDev( D_ERROR, "S_LoadOGG: couldn't open ogg stream %s\n", name );
		Mem_Free( data );
		return false;
	}

	// get the stream information
	vi = ov_info( &vf, -1 );
	if( vi->channels != 1 )
	{
		MsgDev( D_ERROR, "S_LoadOGG: only mono OGG files supported (%s)\n", name );
		ov_clear( &vf );
		Mem_Free( data );
		return false;
	}

	info->channels = vi->channels;
	info->rate = vi->rate;
	info->width = 2; // always 16-bit PCM
	info->loopstart = -1;
	length = ov_pcm_total( &vf, -1 ) * vi->channels * 2;  // 16 bits => "* 2"
	if( !length )
	{
		// bad ogg file
		MsgDev( D_ERROR, "S_LoadOGG: (%s) is probably corrupted\n", name );
		ov_clear( &vf );
		Mem_Free( data );
		return false;
	}
	buffer = (byte *)Z_Malloc( length );

	// decompress ogg into pcm wav format
	while((ret = ov_read( &vf, &buffer[done], (int)(length - done), big_endian, 2, 1, &dummy )) > 0)
		done += ret;
	info->samples = done / ( vi->channels * 2 );
	vc = ov_comment( &vf, -1 );

	if( vc )
	{
		comm = vorbis_comment_query( vc, "LOOP_START", 0 );
		if( comm ) 
		{
			//FXIME: implement
			Msg("ogg 'cue' %d\n", com.atoi(comm) );
			//info->loopstart = bound( 0, com.atoi(comm), info->samples );
 		}
 	}

	// close file
	ov_clear( &vf );
	Mem_Free( data );
	*wav = buffer;	// load the data

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
	palGenBuffers( 1, &sfx->bufferNum );
	palBufferData( sfx->bufferNum, sfx->format, data, size, sfx->rate );
	S_CheckForErrors();
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
			((short *)out)[i] = sin(i * 0.1f) * 20000;
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
	{"sound/%s.%s", "ogg", S_LoadOGG},
	{"sound/%s.%s", "wav", S_LoadWAV},
	{"%s.%s", "ogg", S_LoadOGG},
	{"%s.%s", "wav", S_LoadWAV},
	{NULL, NULL}
};
bool S_LoadSound( sfx_t *sfx )
{
	byte		*data;
	wavinfo_t		info;
	const char	*ext;
	string		loadname, path;
	loadformat_t	*format;
	bool		anyformat;

	if( !sfx ) return false;
	if( sfx->name[0] == '*' ) return false;
	if( sfx->loaded ) return true;	// see if still in memory

	// load it from disk
	ext = FS_FileExtension( sfx->name );
	anyformat = !com.stricmp(ext, "") ? true : false;

	com.strncpy( loadname, sfx->name, sizeof(loadname) - 1);
	FS_StripExtension( loadname ); // remove extension if needed

	// developer warning
	if(!anyformat) MsgDev(D_NOTE, "Note: %s will be loading only with ext .%s\n", loadname, ext );

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
	MsgDev(D_WARN, "FS_LoadSound: couldn't load %s\n", sfx->name );
	S_CreateDefaultSound( &data, &info );
	info.loopstart = -1;

snd_loaded:
	// load it in
	sfx->loopstart = info.loopstart;
	sfx->samples = info.samples;
	sfx->rate = info.rate;
	S_UploadSound( data, info.width, info.channels, sfx );
	sfx->loaded = true;
	Mem_Free( data );

	return true;
}

// =======================================================================
// Load a sound
// =======================================================================
/*
=================
S_FindSound
=================
*/
sfx_t *S_FindSound( const char *name )
{
	sfx_t	*sfx;
	int	i;

	if( !name || !name[0] ) return NULL;
	if( com.strlen(name) >= MAX_STRING )
	{
		MsgDev( D_ERROR, "S_FindSound: sound name too long: %s", name );
		return NULL;
	}

	for( i = 0; i < s_numSfx; i++ )
          {
		sfx = &s_knownSfx[i];
		if( !sfx->name[0] ) continue;
		if( !com.strcmp( name, sfx->name ))
		{
			// prolonge registration
			sfx->registration_sequence = s_registration_sequence;
			return sfx;
		}
	} 

	// find a free sfx slot spot
	for( i = 0, sfx = s_knownSfx; i < s_numSfx; i++, sfx++)
	{
		if(!sfx->name[0]) break; // free spot
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
	Mem_Set( sfx, 0, sizeof(*sfx));
	com.strncpy( sfx->name, name, MAX_STRING );
	sfx->registration_sequence = s_registration_sequence;

	return sfx;
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

	S_InitAmbientSounds();
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
		if( sfx->registration_sequence != s_registration_sequence )
		{	
			// don't need this sound
			palDeleteBuffers( 1, &sfx->bufferNum );
			Mem_Set( sfx, 0, sizeof( sfx_t )); // free spot
		}
	}

	// load everything in
	for( i = 0, sfx = s_knownSfx; i < s_numSfx; i++, sfx++ )
	{
		if( !sfx->name[0] )continue;
		S_LoadSound( sfx );
	}
	s_registering = false;
}

/*
=================
S_RegisterSound
=================
*/
sound_t S_RegisterSound( const char *name )
{
	sfx_t	*sfx;

	if( !al_state.initialized )
		return -1;

	sfx = S_FindSound( name );
	if( !sfx ) return -1;

	sfx->registration_sequence = s_registration_sequence;
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
	for (i = 0; i < s_numSfx; i++)
	{
		sfx = &s_knownSfx[i];
		if( !sfx->loaded ) continue;
		palDeleteBuffers(1, &sfx->bufferNum);
	}

	Mem_Set( s_knownSfx, 0, sizeof(s_knownSfx));
	s_numSfx = 0;
}

void S_InitAmbientSounds( void )
{
	if( s_ambientvolume->value == 0.0f )
		return;

	// FIXME: create external script for replace this sounds
	ambient_sfx[AMBIENT_SKY] = S_RegisterSound( "ambience/wind2.wav" );
	ambient_sfx[AMBIENT_WATER] = S_RegisterSound( "ambience/water1.wav" );
	ambient_sfx[AMBIENT_SLIME] = S_RegisterSound( "misc/null.wav" );
	ambient_sfx[AMBIENT_LAVA] = S_RegisterSound( "misc/null.wav" );
}