//=======================================================================
//			Copyright XashXT Group 2007 ©
//			s_stream.c - sound streaming
//=======================================================================

#include "sound.h"
#include "s_stream.h"

#define BUFFER_SIZE	16384

static bg_track_t	s_bgTrack;
static channel_t	*s_streamingChannel;

/*
=======================================================================

 OGG VORBIS STREAMING

=======================================================================
*/
static size_t ovc_read( void *ptr, size_t size, size_t nmemb, void *datasource )
{
	bg_track_t *track = (bg_track_t *)datasource;

	if (!size || !nmemb)
		return 0;

	return FS_Read( track->file, ptr, size * nmemb ) / size;
}

static int ovc_seek ( void *datasource, ogg_int64_t offset, int whence )
{
	bg_track_t *track = (bg_track_t *)datasource;

	switch( whence )
	{
	case SEEK_SET:
		FS_Seek(track->file, (int)offset, SEEK_SET);
		break;
	case SEEK_CUR:
		FS_Seek(track->file, (int)offset, SEEK_CUR);
		break;
	case SEEK_END:
		FS_Seek(track->file, (int)offset, SEEK_END);
		break;
	default:
		return -1;
	}
	return 0;
}

static int ovc_close( void *datasource )
{
	bg_track_t *track = (bg_track_t *)datasource;

	//FIXME: FS_Close( track->file );
	return 0;
}

static long ovc_tell (void *datasource)
{
	bg_track_t *track = (bg_track_t *)datasource;

	return FS_Tell( track->file );
}

/*
=================
S_OpenBackgroundTrack
=================
*/
static bool S_OpenBackgroundTrack (const char *name, bg_track_t *track)
{
	vorbisfile_t	*vorbisFile;
	vorbis_info_t	*vorbisInfo;
	ov_callbacks_t	vorbisCallbacks = { ovc_read, ovc_seek, ovc_close, ovc_tell };

	track->file = FS_Open( name, "rb" );
	if( !track->file )
	{
		MsgWarn( "S_OpenBackgroundTrack: couldn't find %s\n", name );
		return false;
	}
	track->vorbisFile = vorbisFile = Z_Malloc(sizeof(vorbisfile_t));

	if( ov_open_callbacks(track, vorbisFile, NULL, 0, vorbisCallbacks) < 0 )
	{
		MsgWarn( "S_OpenBackgroundTrack: couldn't open ogg stream %s\n", name );
		return false;
	}

	vorbisInfo = ov_info( vorbisFile, -1 );
	if( vorbisInfo->channels != 1 && vorbisInfo->channels != 2)
	{
		MsgWarn( "S_OpenBackgroundTrack: only mono and stereo ogg files supported %s\n", name );
		return false;
	}

	track->start = ov_raw_tell( vorbisFile );
	track->rate = vorbisInfo->rate;
	track->format = (vorbisInfo->channels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;

	return true;
}

/*
=================
S_CloseBackgroundTrack
=================
*/
static void S_CloseBackgroundTrack( bg_track_t *track )
{
	if( track->vorbisFile )
	{
		ov_clear( track->vorbisFile );
		Mem_Free( track->vorbisFile );
		track->vorbisFile = NULL;
	}
	if( track->file )
	{
		FS_Close( track->file );
		track->file = 0;
	}
}

/*
=================
S_StreamBackgroundTrack
=================
*/
void S_StreamBackgroundTrack( void )
{
	byte	data[BUFFER_SIZE];
	int	processed, queued, state;
	int	size, read, dummy;
	uint	buffer;

	if( !s_bgTrack.file || !s_musicvolume->value )
		return;
	if(!s_streamingChannel) return;

	// unqueue and delete any processed buffers
	palGetSourcei( s_streamingChannel->sourceNum, AL_BUFFERS_PROCESSED, &processed );
	if( processed > 0 )
	{
		while( processed-- )
		{
			palSourceUnqueueBuffers(s_streamingChannel->sourceNum, 1, &buffer);
			palDeleteBuffers(1, &buffer);
		}
	}

	// make sure we always have at least 4 buffers in the queue
	palGetSourcei( s_streamingChannel->sourceNum, AL_BUFFERS_QUEUED, &queued );
	while( queued < 4 )
	{
		size = 0;
		// stream from disk
		while( size < BUFFER_SIZE )
		{
			read = ov_read( s_bgTrack.vorbisFile, data + size, BUFFER_SIZE - size, 0, 2, 1, &dummy );
			if( read == 0 )
			{
				// end of file
				if(!s_bgTrack.looping)
				{
					// close the intro track
					S_CloseBackgroundTrack( &s_bgTrack );

					// open the loop track
					if(!S_OpenBackgroundTrack(s_bgTrack.loopName, &s_bgTrack))
					{
						S_StopBackgroundTrack();
						return;
					}
					s_bgTrack.looping = true;
				}

				// restart the track, skipping over the header
				ov_raw_seek(s_bgTrack.vorbisFile, (ogg_int64_t)s_bgTrack.start );

				// try streaming again
				read = ov_read( s_bgTrack.vorbisFile, data + size, BUFFER_SIZE - size, 0, 2, 1, &dummy );
			}
			if( read <= 0 )
			{
				// an error occurred
				S_StopBackgroundTrack();
				return;
			}
			size += read;
		}

		// upload and queue the new buffer
		palGenBuffers( 1, &buffer );
		palBufferData( buffer, s_bgTrack.format, data, size, s_bgTrack.rate );
		palSourceQueueBuffers( s_streamingChannel->sourceNum, 1, &buffer );
		queued++;
	}

	// update volume
	palSourcef( s_streamingChannel->sourceNum, AL_GAIN, s_musicvolume->value );

	// if not playing, then do so
	palGetSourcei( s_streamingChannel->sourceNum, AL_SOURCE_STATE, &state );
	if( state != AL_PLAYING ) palSourcePlay(s_streamingChannel->sourceNum);
}

/*
=================
S_StartBackgroundTrack
=================
*/
void S_StartBackgroundTrack( const char *introTrack, const char *loopTrack )
{
	if( !al_state.initialized ) return;

	// stop any playing tracks
	S_StopBackgroundTrack();

	// Start it up
	com.snprintf( s_bgTrack.introName, sizeof(s_bgTrack.introName), "music/%s.ogg", introTrack);
	com.snprintf( s_bgTrack.loopName, sizeof(s_bgTrack.loopName), "music/%s.ogg", loopTrack );

	S_StartStreaming();

	// open the intro track
	if(!S_OpenBackgroundTrack( s_bgTrack.introName, &s_bgTrack))
	{
		S_StopBackgroundTrack();
		return;
	}
	S_StreamBackgroundTrack();
}

/*
=================
S_StopBackgroundTrack
=================
*/
void S_StopBackgroundTrack( void )
{
	if( !al_state.initialized ) return;

	S_StopStreaming();
	S_CloseBackgroundTrack(&s_bgTrack);
	memset(&s_bgTrack, 0, sizeof(bg_track_t));
}

/*
=================
S_StartStreaming
=================
*/
void S_StartStreaming( void )
{
	if( !al_state.initialized ) return;
	if( s_streamingChannel ) return; // already started

	s_streamingChannel = S_PickChannel( 0, 0 );
	if( !s_streamingChannel ) return;

	s_streamingChannel->streaming = true;

// FIXME: OpenAL bug?
//palDeleteSources(1, &s_streamingChannel->sourceNum);
//palGenSources(1, &s_streamingChannel->sourceNum);

	// set up the source
	palSourcei(s_streamingChannel->sourceNum, AL_BUFFER, 0 );
	palSourcei(s_streamingChannel->sourceNum, AL_LOOPING, 0 );
	palSourcei(s_streamingChannel->sourceNum, AL_SOURCE_RELATIVE, 1 );
	palSourcefv(s_streamingChannel->sourceNum, AL_POSITION, vec3_origin);
	palSourcefv(s_streamingChannel->sourceNum, AL_VELOCITY, vec3_origin);
	palSourcef(s_streamingChannel->sourceNum, AL_REFERENCE_DISTANCE, 1.0);
	palSourcef(s_streamingChannel->sourceNum, AL_MAX_DISTANCE, 1.0);
	palSourcef(s_streamingChannel->sourceNum, AL_ROLLOFF_FACTOR, 0.0);
}

/*
=================
S_StopStreaming
=================
*/
void S_StopStreaming( void )
{
	int	processed;
	uint	buffer;

	if( !al_state.initialized ) return;
	if( !s_streamingChannel ) return;	// already stopped

	s_streamingChannel->streaming = false;
	// clean up the source
	palSourceStop(s_streamingChannel->sourceNum);

	palGetSourcei(s_streamingChannel->sourceNum, AL_BUFFERS_PROCESSED, &processed);
	if( processed > 0 )
	{
		while( processed-- )
		{
			palSourceUnqueueBuffers(s_streamingChannel->sourceNum, 1, &buffer);
			palDeleteBuffers(1, &buffer);
		}
	}

	palSourcei(s_streamingChannel->sourceNum, AL_BUFFER, 0);

// FIXME: OpenAL bug?
//palDeleteSources(1, &s_streamingChannel->sourceNum);
//palGenSources(1, &s_streamingChannel->sourceNum);
	s_streamingChannel = NULL;
}

/*
=================
S_StreamRawSamples

Cinematic streaming
=================
*/
void S_StreamRawSamples( int samples, int rate, int width, int channels, const byte *data )
{
	int	processed, state, size;
	uint	format, buffer;

	if( !al_state.initialized ) return;
	if( !s_streamingChannel ) return;

	// unqueue and delete any processed buffers
	palGetSourcei( s_streamingChannel->sourceNum, AL_BUFFERS_PROCESSED, &processed );
	if( processed > 0 )
	{
		while( processed-- )
		{
			palSourceUnqueueBuffers(s_streamingChannel->sourceNum, 1, &buffer);
			palDeleteBuffers(1, &buffer);
		}
	}

	// calculate buffer size
	size = samples * width * channels;

	// set buffer format
	if( width == 2 )
	{
		if( channels == 2 ) format = AL_FORMAT_STEREO16;
		else format = AL_FORMAT_MONO16;
	}
	else
	{
		if( channels == 2 ) format = AL_FORMAT_STEREO8;
		else format = AL_FORMAT_MONO8;
	}

	// upload and queue the new buffer
	palGenBuffers( 1, &buffer );
	palBufferData( buffer, format, (byte *)data, size, rate );
	palSourceQueueBuffers( s_streamingChannel->sourceNum, 1, &buffer );

	// update volume
	palSourcef( s_streamingChannel->sourceNum, AL_GAIN, 1.0f );

	// if not playing, then do so
	palGetSourcei(s_streamingChannel->sourceNum, AL_SOURCE_STATE, &state);
	if( state != AL_PLAYING ) palSourcePlay( s_streamingChannel->sourceNum );
}
