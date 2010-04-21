//=======================================================================
//			Copyright XashXT Group 2010 ©
//		snd_main.c - load & save various sound formats
//=======================================================================

#include "soundlib.h"
#include "mathlib.h"

// global sound variables
sndlib_t	sound;

void Sound_Reset( void )
{
	// reset global variables
	sound.width = sound.rate = 0;
	sound.channels = sound.loopstart = 0;
	sound.samples = sound.flags = 0;
	sound.type = WF_UNKNOWN;

	sound.wav = NULL;
	sound.size = 0;
}

wavdata_t *SoundPack( void )
{
	wavdata_t	*pack = Mem_Alloc( Sys.soundpool, sizeof( wavdata_t ));

	pack->buffer = sound.wav;
	pack->width = sound.width;
	pack->rate = sound.rate;
	pack->type = sound.type;
	pack->size = sound.size;
	pack->loopStart = sound.loopstart;
	pack->samples = sound.samples;
	pack->channels = sound.channels;
	pack->flags = sound.flags;

	return pack;
}

/*
================
FS_LoadSound

loading and unpack to wav any known sound
================
*/
wavdata_t *FS_LoadSound( const char *filename, const byte *buffer, size_t size )
{
          const char	*ext = FS_FileExtension( filename );
	string		path, loadname;
	bool		anyformat = true;
	int		filesize = 0;
	const loadwavformat_t *format;
	byte		*f;

	Sound_Reset(); // clear old image
	com.strncpy( loadname, filename, sizeof( loadname ));

	if( com.stricmp( ext, "" ))
	{
		// we needs to compare file extension with list of supported formats
		// and be sure what is real extension, not a filename with dot
		for( format = sound.loadformats; format && format->formatstring; format++ )
		{
			if( !com.stricmp( format->ext, ext ))
			{
				FS_StripExtension( loadname );
				anyformat = false;
				break;
			}
		}
	}

	// HACKHACK: skip any checks, load file from buffer
	if( filename[0] == '#' && buffer && size ) goto load_internal;

	// engine notify
	if( !anyformat ) MsgDev( D_NOTE, "Note: %s will be loading only with ext .%s\n", loadname, ext );
	
	// now try all the formats in the selected list
	for( format = sound.loadformats; format && format->formatstring; format++)
	{
		if( anyformat || !com_stricmp( ext, format->ext ))
		{
			com_sprintf( path, format->formatstring, loadname, "", format->ext );
			f = FS_LoadFile( path, &filesize );
			if( f && filesize > 0 )
			{
				if( format->loadfunc( path, f, filesize ))
				{
					Mem_Free(f); // release buffer
					return SoundPack(); // loaded
				}
				else Mem_Free(f); // release buffer 
			}
		}
	}

load_internal:
	for( format = sound.baseformats; format && format->formatstring; format++ )
	{
		if( anyformat || !com_stricmp( ext, format->ext ))
		{
			if( buffer && size > 0  )
			{
				if( format->loadfunc( loadname, buffer, size ))
					return SoundPack(); // loaded
			}
		}
	}

	if( !sound.loadformats || sound.loadformats->ext == NULL )
		MsgDev( D_NOTE, "FS_LoadSound: soundlib offline\n" );
	else if( filename[0] != '#' )
		MsgDev( D_WARN, "FS_LoadSound: couldn't load \"%s\"\n", loadname );

	return NULL;
}

/*
================
Sound_Save

writes image as any known format
================
*/
bool FS_SaveSound( const char *filename, wavdata_t *wav )
{
	// FIXME: not implemented
	return false;
}

/*
================
Sound_FreeSound

free WAV buffer
================
*/
void FS_FreeSound( wavdata_t *pack )
{
	if( pack )
	{
		if( pack->buffer ) Mem_Free( pack->buffer );
		Mem_Free( pack );
	}
	else MsgDev( D_WARN, "FS_FreeSound: trying to free NULL sound\n" );
}