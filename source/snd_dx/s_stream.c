//=======================================================================
//			Copyright XashXT Group 2009 ©
//			s_stream.c - sound streaming
//=======================================================================

#include "sound.h"
#include "byteorder.h"

portable_samplepair_t	s_rawsamples[MAX_RAW_SAMPLES];
int			s_rawend;
static bg_track_t		s_bgTrack;

/*
=================
S_StartBackgroundTrack
=================
*/
void S_StartBackgroundTrack( const char *introTrack, const char *mainTrack )
{
	S_StopBackgroundTrack();

	if(( !introTrack || !*introTrack ) && ( !mainTrack || !*mainTrack ))
		return;

	if( !introTrack )
	{
		introTrack = "";
	}
	if( !mainTrack || !*mainTrack )
	{
		mainTrack = introTrack;
	}
	if( !*introTrack ) return;

	if( mainTrack )
	{
		com.snprintf( s_bgTrack.loopName, sizeof( s_bgTrack.loopName ), "media/%s", mainTrack );
	}
	else s_bgTrack.loopName[0] = 0;

	S_StartStreaming();

	// close the background track, but DON'T reset s_rawend
	// if restarting the same back ground track
	if( s_bgTrack.stream )
	{
		FS_CloseStream( s_bgTrack.stream );
		s_bgTrack.stream = NULL;
	}

	// open stream
	s_bgTrack.stream = FS_OpenStream( va( "media/%s", introTrack ));
}

void S_StopBackgroundTrack( void )
{
	S_StopStreaming();

	if( !s_bgTrack.stream ) return;

	FS_CloseStream( s_bgTrack.stream );
	Mem_Set( &s_bgTrack, 0, sizeof( bg_track_t ));
	s_rawend = 0;
}

/*
=================
S_StreamBackgroundTrack
=================
*/
void S_StreamBackgroundTrack( void )
{
	int	bufferSamples;
	int	fileSamples;
	byte	raw[MAX_RAW_SAMPLES];
	int	r, fileBytes;
	float	musicVolume = 0.5f;

	if( !s_bgTrack.stream ) return;

	// graeme see if this is OK
	musicVolume = ( musicVolume + ( s_musicvolume->value * 2 )) / 4.0f;

	// don't bother playing anything if musicvolume is 0
	if( musicVolume <= 0 ) return;

	// see how many samples should be copied into the raw buffer
	if( s_rawend < soundtime )
		s_rawend = soundtime;

	while( s_rawend < soundtime + MAX_RAW_SAMPLES )
	{
		wavdata_t	*info = FS_StreamInfo( s_bgTrack.stream );

		bufferSamples = MAX_RAW_SAMPLES - (s_rawend - soundtime);

		// decide how much data needs to be read from the file
		fileSamples = bufferSamples * info->rate / dma.speed;

		// our max buffer size
		fileBytes = fileSamples * ( info->width * info->channels );

		if( fileBytes > sizeof( raw ))
		{
			fileBytes = sizeof( raw );
			fileSamples = fileBytes / ( info->width * info->channels );
		}

		// read
		r = FS_ReadStream( s_bgTrack.stream, fileBytes, raw );

		if( r < fileBytes )
		{
			fileBytes = r;
			fileSamples = r / ( info->width * info->channels );
		}

		if( r > 0 )
		{
			// add to raw buffer
			// FIXME: apply musicVolume
			S_StreamRawSamples( fileSamples, info->rate, info->width, info->channels, raw );
		}
		else
		{
			// loop
			if( s_bgTrack.loopName[0] )
			{
				FS_CloseStream( s_bgTrack.stream );
				s_bgTrack.stream = NULL;
				S_StartBackgroundTrack( s_bgTrack.loopName, s_bgTrack.loopName );
				if( !s_bgTrack.stream ) return;
			}
			else
			{
				S_StopBackgroundTrack();
				return;
			}
		}

	}
}

/*
=================
S_StartStreaming
=================
*/
void S_StartStreaming( void )
{
}

/*
=================
S_StopStreaming
=================
*/
void S_StopStreaming( void )
{
	// clear s_rawend here ?
}

/*
============
S_StreamRawSamples

Cinematic streaming and voice over network
============
*/
void S_StreamRawSamples( int samples, int rate, int width, int channels, const byte *data )
{
	int	i, src, dst;
	float	scale;

	if( s_rawend < paintedtime )
		s_rawend = paintedtime;
	scale = (float)rate / dma.speed;

	if( channels == 2 && width == 2 )
	{
		if( scale == 1.0f )
		{	
			// optimized case
			for( i = 0; i < samples; i++ )
			{
				dst = s_rawend & (MAX_RAW_SAMPLES - 1);
				s_rawend++;
				s_rawsamples[dst].left = LittleShort(((short *)data)[i*2]) << 8;
				s_rawsamples[dst].right = LittleShort(((short *)data)[i*2+1]) << 8;
			}
		}
		else
		{
			for( i = src = 0; src < samples; i++ )
			{
				src = i * scale;
				if( src >= samples ) break;

				dst = s_rawend & (MAX_RAW_SAMPLES - 1);
				s_rawend++;
				s_rawsamples[dst].left = LittleShort(((short *)data)[src*2]) << 8;
				s_rawsamples[dst].right = LittleShort(((short *)data)[src*2+1]) << 8;
			}
		}
	}
	else if( channels == 1 && width == 2 )
	{
		for( i = src = 0; src < samples; i++ )
		{
			src = i * scale;
			if( src >= samples ) break;

			dst = s_rawend & (MAX_RAW_SAMPLES - 1);
			s_rawend++;
			s_rawsamples[dst].left = LittleShort(((short *)data)[src]) << 8;
			s_rawsamples[dst].right = LittleShort(((short *)data)[src]) << 8;
		}
	}
	else if( channels == 2 && width == 1 )
	{
		for( i = src = 0; src < samples; i++ )
		{
			src = i * scale;
			if( src >= samples ) break;

			dst = s_rawend & (MAX_RAW_SAMPLES - 1);
			s_rawend++;
			s_rawsamples[dst].left = ((char *)data)[src*2] << 16;
			s_rawsamples[dst].right = ((char *)data)[src*2+1] << 16;
		}
	}
	else if( channels == 1 && width == 1 )
	{
		for( i = src = 0; src < samples; i++ )
		{
			src = i * scale;
			if( src >= samples ) break;

			dst = s_rawend & (MAX_RAW_SAMPLES - 1);
			s_rawend++;
			s_rawsamples[dst].left = (((byte *)data)[src]-128) << 16;
			s_rawsamples[dst].right = (((byte *)data)[src]-128) << 16;
		}
	}
}