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
void S_StartBackgroundTrack( const char *introTrack, const char *loopTrack )
{
	if( !sound_started ) return;
	S_StopBackgroundTrack();

	// start it up
	com.snprintf( s_bgTrack.introName, sizeof(s_bgTrack.introName), "media/%s.ogg", introTrack);
	com.snprintf( s_bgTrack.loopName, sizeof(s_bgTrack.loopName), "media/%s.ogg", loopTrack );

	S_StartStreaming();

	// UNDONE: process streaming
}

void S_StopBackgroundTrack( void )
{
	if( !sound_started ) return;

	S_StopStreaming();

	// UNDONE: close background track
	Mem_Set( &s_bgTrack, 0, sizeof( bg_track_t ));
}

void S_StartStreaming( void )
{
	// UNDONE: allocate static channel for streaimng
}

void S_StopStreaming( void )
{
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

	if( !sound_started )
		return;

	if( s_rawend < paintedtime ) s_rawend = paintedtime;
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