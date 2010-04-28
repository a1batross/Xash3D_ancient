//=======================================================================
//			Copyright XashXT Group 2007 ©
//			s_stream.c - sound streaming
//=======================================================================

#include "sound.h"

#define NUM_MUSIC_BUFFERS	4
#define MUSIC_BUFFER_SIZE	4096
#define RAW_BUFFER_SIZE	16384

static bg_track_t		s_bgTrack;
static channel_t		*s_streamingChannel;
static uint		musicBuffers[NUM_MUSIC_BUFFERS];

/*
=================
S_CloseBackgroundTrack
=================
*/
static void S_CloseBackgroundTrack( bg_track_t *track )
{
	if( track->intro_stream )
	{
		FS_CloseStream( track->intro_stream );
		track->intro_stream = NULL;
	}

	if( track->main_stream )
	{
		FS_CloseStream( track->main_stream );
		track->main_stream = NULL;
	}
}

/*
=================
S_ProcessBuffers
=================
*/
static void S_ProcessBuffers( uint bufnum )
{
	uint	format;
	stream_t	*curstream;
	byte	decode_buffer[MUSIC_BUFFER_SIZE];
	wavdata_t	*info;
	size_t	size;

	if( s_bgTrack.intro_stream )
		curstream = s_bgTrack.intro_stream;
	else curstream = s_bgTrack.main_stream;

	if( !curstream ) return; // stopped

	size = FS_ReadStream( curstream, MUSIC_BUFFER_SIZE, decode_buffer );

	// run out data to read, start at the beginning again
	if( size == 0 )
	{
		FS_CloseStream( curstream );

		// the intro stream just finished playing so we don't need to reopen
		// the music stream.
		if( s_bgTrack.intro_stream )
			s_bgTrack.intro_stream = NULL;
		else s_bgTrack.main_stream = FS_OpenStream( s_bgTrack.loopName );
		
		curstream = s_bgTrack.main_stream;

		if( !curstream )
		{
			S_StopBackgroundTrack();
			return;
		}
		size = FS_ReadStream( curstream, MUSIC_BUFFER_SIZE, decode_buffer );
	}

	info = FS_StreamInfo( curstream );

	format = S_GetFormat( info->width, info->channels );

	if( size == 0 )
	{
		// we have no data to buffer, so buffer silence
		byte dummyData[2] = { 0 };
		palBufferData( bufnum, AL_FORMAT_MONO16, (void *)dummyData, 2, 22050 );
	}
	else palBufferData( bufnum, format, decode_buffer, size, info->rate );

	if( S_CheckForErrors( ))
	{
		S_StopBackgroundTrack ();
		return;
	}
}

/*
=================
S_StreamBackgroundTrack
=================
*/
void S_StreamBackgroundTrack( void )
{
	int	numBuffers;
	int	state;

	if( !s_bgTrack.active )
		return;

	palGetSourcei( s_streamingChannel->sourceNum, AL_BUFFERS_PROCESSED, &numBuffers );

	while( numBuffers-- )
	{
		uint	bufNum;

		palSourceUnqueueBuffers( s_streamingChannel->sourceNum, 1, &bufNum );
		S_ProcessBuffers( bufNum );
		palSourceQueueBuffers( s_streamingChannel->sourceNum, 1, &bufNum );
	}

	// hitches can cause OpenAL to be starved of buffers when streaming.
	// If this happens, it will stop playback. This restarts the source if
	// it is no longer playing, and if there are buffers available
	palGetSourcei( s_streamingChannel->sourceNum, AL_SOURCE_STATE, &state );
	palGetSourcei( s_streamingChannel->sourceNum, AL_BUFFERS_QUEUED, &numBuffers );

	if( state == AL_STOPPED && numBuffers )
	{
		MsgDev( D_INFO, "restarted background track\n" );
		palSourcePlay( s_streamingChannel->sourceNum );
	}

	// Set the gain property
	palSourcef( s_streamingChannel->sourceNum, AL_GAIN, s_musicvolume->value );
}

/*
=================
S_StartBackgroundTrack
=================
*/
void S_StartBackgroundTrack( const char *introTrack, const char *mainTrack )
{
	bool	issame = false;
	int	i;

	// stop any playing tracks
	S_StopBackgroundTrack();

	if(( !introTrack || !*introTrack ) && ( !mainTrack || !*mainTrack ))
		return;

	S_StartStreaming();

	if( !mainTrack || !*mainTrack )
	{
		mainTrack = introTrack;
		issame = true;
	}
	else if( introTrack && *introTrack && !com.strcmp( introTrack, mainTrack ))
	{
		issame = true;
	}

	// copy the loop over
	com.snprintf( s_bgTrack.loopName, sizeof( s_bgTrack.loopName ), "media/%s", mainTrack );

	if( !issame )
	{
		// Open the intro and don't mind whether it succeeds.
		// The important part is the loop.
		s_bgTrack.intro_stream = FS_OpenStream( va( "media/%s", introTrack ));
	}
	else s_bgTrack.intro_stream = NULL;

	s_bgTrack.main_stream = FS_OpenStream( s_bgTrack.loopName );

	if( !s_bgTrack.main_stream )
	{
		S_StopBackgroundTrack();
		return;
	}

	// generate the musicBuffers
	palGenBuffers( NUM_MUSIC_BUFFERS, musicBuffers );
	
	// queue the musicBuffers up
	for( i = 0; i < NUM_MUSIC_BUFFERS; i++ )
	{
		S_ProcessBuffers( musicBuffers[i] );
	}

	palSourceQueueBuffers( s_streamingChannel->sourceNum, NUM_MUSIC_BUFFERS, musicBuffers );

	// set the initial gain property
	palSourcef( s_streamingChannel->sourceNum, AL_GAIN, s_musicvolume->value );
	
	// Start playing
	palSourcePlay( s_streamingChannel->sourceNum );
	s_bgTrack.active = true;
}

/*
=================
S_StopBackgroundTrack
=================
*/
void S_StopBackgroundTrack( void )
{
	S_StopStreaming();
	S_CloseBackgroundTrack(&s_bgTrack);
	Mem_Set(&s_bgTrack, 0, sizeof(bg_track_t));
}

/*
=================
S_StartStreaming
=================
*/
void S_StartStreaming( void )
{
	if( s_streamingChannel ) return; // already started

	s_streamingChannel = SND_PickStaticChannel( 0, NULL );
	if( !s_streamingChannel ) return;

	// lock stream channel to avoid re-use it
	s_streamingChannel->entchannel = CHAN_STREAM;
	s_streamingChannel->sfx = NULL;

	// set up the source
	palSourcei( s_streamingChannel->sourceNum, AL_BUFFER, 0 );
	palSourcei( s_streamingChannel->sourceNum, AL_LOOPING, 0 );
	palSourcei( s_streamingChannel->sourceNum, AL_SOURCE_RELATIVE, 1 );
	palSourcefv( s_streamingChannel->sourceNum, AL_POSITION, vec3_origin );
	palSourcefv( s_streamingChannel->sourceNum, AL_VELOCITY, vec3_origin );
	palSourcef( s_streamingChannel->sourceNum, AL_REFERENCE_DISTANCE, 1.0f );
	palSourcef( s_streamingChannel->sourceNum, AL_MAX_DISTANCE, 1.0f );
	palSourcef( s_streamingChannel->sourceNum, AL_ROLLOFF_FACTOR, 0.0f );
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

	if( !s_streamingChannel ) return;	// already stopped

	s_streamingChannel->entchannel = 0;	// can be reused now
	s_streamingChannel->sfx = NULL;

	// clean up the source
	palSourceStop( s_streamingChannel->sourceNum );

	palGetSourcei( s_streamingChannel->sourceNum, AL_BUFFERS_PROCESSED, &processed );
	if( processed > 0 )
	{
		while( processed-- )
		{
			palSourceUnqueueBuffers( s_streamingChannel->sourceNum, 1, &buffer );
			palDeleteBuffers( 1, &buffer );
		}
	}

	palSourcei( s_streamingChannel->sourceNum, AL_BUFFER, 0 );
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
