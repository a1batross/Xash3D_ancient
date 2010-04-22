//=======================================================================
//			Copyright XashXT Group 2009 ©
//		      s_mix.c - portable code to mix sounds
//=======================================================================

#include "sound.h"

#define PAINTBUFFER_SIZE	2048
portable_samplepair_t	paintbuffer[PAINTBUFFER_SIZE];
int			snd_scaletable[32][256];
int 			*snd_p, snd_linear_count, snd_vol;
short			*snd_out;

void S_WriteLinearBlastStereo16( void )
{
	int	i, val;

	for( i = 0; i < snd_linear_count; i += 2 )
	{
		val = snd_p[i]>>8;
		if( val > 0x7fff ) snd_out[i] = 0x7fff;
		else if( val < (short)0x8000 )
			snd_out[i] = (short)0x8000;
		else snd_out[i] = val;

		val = snd_p[i+1]>>8;
		if( val > 0x7fff ) snd_out[i+1] = 0x7fff;
		else if( val < (short)0x8000 )
			snd_out[i+1] = (short)0x8000;
		else snd_out[i+1] = val;
	}
}

void S_TransferStereo16( dword *pbuf, int endtime )
{
	int	lpos, lpaintedtime;
	
	snd_p = (int *)paintbuffer;
	lpaintedtime = paintedtime;

	while( lpaintedtime < endtime )
	{
		// handle recirculating buffer issues
		lpos = lpaintedtime & ((dma.samples >> 1) - 1);

		snd_out = (short *) pbuf + (lpos << 1);

		snd_linear_count = (dma.samples>>1) - lpos;
		if( lpaintedtime + snd_linear_count > endtime )
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count <<= 1;

		// write a linear blast of samples
		S_WriteLinearBlastStereo16();

		snd_p += snd_linear_count;
		lpaintedtime += (snd_linear_count >> 1);
	}
}

/*
===================
S_TransferPaintBuffer

===================
*/
void S_TransferPaintBuffer( int endtime )
{
	int 	out_idx, count;
	int 	step, val;
	int 	*p, out_mask;
	dword	*pbuf;

	pbuf = (dword *)dma.buffer;

	if( dma.samplebits == 16 && dma.channels == 2 )
	{	
		// optimized case
		S_TransferStereo16( pbuf, endtime );
	}
	else
	{	
		// general case
		p = (int *)paintbuffer;
		count = (endtime - paintedtime) * dma.channels;
		out_mask = dma.samples - 1; 
		out_idx = paintedtime * dma.channels & out_mask;
		step = 3 - dma.channels;

		if( dma.samplebits == 16 )
		{
			short	*out = (short *)pbuf;

			while( count-- )
			{
				val = *p >> 8;
				p += step;
				if( val > 0x7fff ) val = 0x7fff;
				else if( val < (short)0x8000 )
					val = (short)0x8000;
				out[out_idx] = val;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
		else if( dma.samplebits == 8 )
		{
			byte	*out = (byte *)pbuf;

			while( count-- )
			{
				val = *p >> 8;
				p += step;
				if( val > 0x7fff ) val = 0x7fff;
				else if( val < (short)0x8000 )
					val = (short)0x8000;
				out[out_idx] = (val>>8) + 128;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
	}
}

/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/
void S_PaintChannelFrom8( channel_t *ch, wavdata_t *sc, int count )
{
	int	i, data;
	int	*lscale, *rscale;
	byte	*sfx;

	ch->leftvol = bound( 0, ch->leftvol, 255 );
	ch->rightvol = bound( 0, ch->rightvol, 255 );
		
	lscale = snd_scaletable[ch->leftvol>>3];
	rscale = snd_scaletable[ch->rightvol>>3];
	sfx = (signed char *)sc->buffer + ch->pos;

	for( i = 0; i < count; i++ )
	{
		data = sfx[i];
		paintbuffer[i].left += lscale[data];
		paintbuffer[i].right += rscale[data];
	}
	ch->pos += count;
}

void S_PaintChannelFrom16( channel_t *ch, wavdata_t *sc, int count )
{
	int		i, data;
	int		left, right;
	signed short	*sfx;

	ch->leftvol = bound( 0, ch->leftvol, 255 );
	ch->rightvol = bound( 0, ch->rightvol, 255 );
	sfx = (signed short *)sc->buffer + ch->pos;

	for( i = 0; i < count; i++ )
	{
		data = sfx[i];
		left = ( data * ch->leftvol ) >> 8;
		right = (data * ch->rightvol) >> 8;
		paintbuffer[i].left += left;
		paintbuffer[i].right += right;
	}
	ch->pos += count;
}

void S_MixAllChannels( int endtime, int end )
{
	channel_t *ch;
	wavdata_t	*sc;
	int	i, count = 0;	
	int	ltime;

	// paint in the channels.
	for( i = 0, ch = channels; i < total_channels; i++, ch++ )
	{
		if( !ch->sfx ) continue;
		if( !ch->leftvol && !ch->rightvol )
			continue;

		sc = S_LoadSound( ch->sfx );
		if( !sc ) continue;

		ltime = paintedtime;
		
		while( ltime < end )
		{
			// paint up to end
			if( ch->end < end )
				count = ch->end - ltime;
			else count = end - ltime;

			if( count > 0 )
			{	
				if( sc->width == 1 )
					S_PaintChannelFrom8( ch, sc, count );
				else S_PaintChannelFrom16( ch, sc, count );
	
				ltime += count;
			}

			// if at end of loop, restart
			if( ltime >= ch->end )
			{
				if( ch->autosound && ch->use_loop )
				{	
					// autolooped sounds always go back to start
					ch->pos = 0;
					ch->end = ltime + sc->samples;
				}
				else if( sc->loopStart >= 0 && ch->use_loop )
				{
					ch->pos = sc->loopStart;
					ch->end = ltime + sc->samples - ch->pos;
				}
				else
				{
					// channel just stopped
					ch->sfx = NULL;
					break;
				}
			}
		}
	}
}

void S_PaintChannels( int endtime )
{
	int	end;

	snd_vol = S_GetMasterVolume () * 256;

	while( paintedtime < endtime )
	{
		// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if( endtime - paintedtime > PAINTBUFFER_SIZE )
			end = paintedtime + PAINTBUFFER_SIZE;

		// clear the paint buffer
		if( s_rawend < paintedtime )
		{
			Mem_Set( paintbuffer, 0, (end - paintedtime) * sizeof( portable_samplepair_t ));
		}
		else
		{	
			int	i, stop;

			// copy from the streaming sound source
			stop = (end < s_rawend) ? end : s_rawend;

			for( i = paintedtime; i < stop; i++ )
				paintbuffer[i - paintedtime] = s_rawsamples[i & (MAX_RAW_SAMPLES - 1)];
			
			for( ; i < end; i++ )
				paintbuffer[i-paintedtime].left = paintbuffer[i-paintedtime].right = 0;
		}

		S_MixAllChannels( endtime, end );

		SX_RoomFX( endtime, true, true );

		// transfer out according to DMA format
		S_TransferPaintBuffer( end );
		paintedtime = end;
	}
}

void S_InitScaletable( void )
{
	int	i, j;
	int	scale;

	for( i = 0; i < 32; i++ )
	{
		scale = i * 8 * 256 * S_GetMasterVolume();
		for( j = 0; j < 256; j++ ) snd_scaletable[i][j] = ((signed char)j) * scale;
	}
	s_volume->modified = false;
}
