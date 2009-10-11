/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// snd_mix.c -- portable code to mix sounds for snd_dma.c

#include "sound.h"

// global sound filters
#define FILTERTYPE_NONE	0
#define FILTERTYPE_LINEAR	1
#define FILTERTYPE_CUBIC	2

#define SOUND_MIX_WET	0	// mix only samples that don't have channel set to 'dry' (default)
#define SOUND_MIX_DRY	1	// mix only samples with channel set to 'dry' (ie: music)

samplepair_t	paintbuffer[(PAINTBUFFER_SIZE+1)];
samplepair_t	roombuffer[(PAINTBUFFER_SIZE+1)];
samplepair_t	facingbuffer[(PAINTBUFFER_SIZE+1)];
samplepair_t	facingawaybuffer[(PAINTBUFFER_SIZE+1)];
samplepair_t	drybuffer[(PAINTBUFFER_SIZE+1)];

// filter memory for upsampling
samplepair_t	cubicfilter1[3] = {{0,0},{0,0},{0,0}};
samplepair_t	cubicfilter2[3] = {{0,0},{0,0},{0,0}};

samplepair_t	linearfilter1[1] = {0,0};
samplepair_t	linearfilter2[1] = {0,0};
samplepair_t	linearfilter3[1] = {0,0};
samplepair_t	linearfilter4[1] = {0,0};
samplepair_t	linearfilter5[1] = {0,0};
samplepair_t	linearfilter6[1] = {0,0};
samplepair_t	linearfilter7[1] = {0,0};
samplepair_t	linearfilter8[1] = {0,0};

// temp paintbuffer - not included in main list of paintbuffers
samplepair_t	temppaintbuffer[(PAINTBUFFER_SIZE+1)];
samplepair_t	*g_curpaintbuffer;

paintbuffer_t	paintbuffers[CPAINTBUFFERS];

bool		g_bDspOff;
bool		g_bdirectionalfx;
int		snd_scaletable[SND_SCALE_LEVELS][256];
int		*snd_p, snd_linear_count, snd_vol;
short		*snd_out;

//===============================================================================
// Mix buffer (paintbuffer) management routines
//===============================================================================
void MIX_FreeAllPaintbuffers( void )
{
	// clear paintbuffer structs
	Mem_Set( paintbuffers, 0, CPAINTBUFFERS * sizeof( paintbuffer_t ));
}

bool MIX_InitAllPaintbuffers(void)
{
	// clear paintbuffer structs
	MIX_FreeAllPaintbuffers ();

	// front, rear & dry paintbuffers
	paintbuffers[IPAINTBUFFER].pbuf = paintbuffer;
	paintbuffers[IROOMBUFFER].pbuf = roombuffer;
	paintbuffers[IFACINGBUFFER].pbuf = facingbuffer;
	paintbuffers[IFACINGAWAYBUFFER].pbuf = facingawaybuffer;
	paintbuffers[IDRYBUFFER].pbuf	= drybuffer;

	// buffer flags
	paintbuffers[IROOMBUFFER].flags = SOUND_BUSS_ROOM;
	paintbuffers[IFACINGBUFFER].flags = SOUND_BUSS_FACING;
	paintbuffers[IFACINGAWAYBUFFER].flags = SOUND_BUSS_FACINGAWAY;

	MIX_SetCurrentPaintbuffer( IPAINTBUFFER );
	return true;
}

void S_ApplyDSPEffects( int idsp, samplepair_t *pbuffront, int samplecount )
{
	DSP_Process( idsp, pbuffront, samplecount );
}

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

	snd_vol = S_GetMasterVolume() * 256;
	snd_p = (int *)PAINTBUFFER;
	lpaintedtime = paintedtime;

	if( !pbuf ) return;

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

	if( s_testsound->integer )
	{
		int	i, count;

		// write a fixed sine wave
		count = (endtime - paintedtime);
		for( i = 0; i < count; i++ )
		{
			paintbuffer[i].left = com.sin(( paintedtime + i ) * 0.1f ) * 20000 * 256;
			paintbuffer[i].right = paintbuffer[i].left;
		}
	}

	// general case
	p = (int *)paintbuffer;
	count = (endtime - paintedtime) * dma.channels;
	out_mask = dma.samples - 1; 
	out_idx = paintedtime * dma.channels & out_mask;
	step = 3 - dma.channels;
	snd_vol = S_GetMasterVolume() * 256;

	if( !pbuf ) return;

	if( dma.samplebits == 16 )
	{
		short	*out = (short *)pbuf;

		while( count-- )
		{
			val = (*p * snd_vol) >> 8;
			p += step;
			val = CLIP( val );

			out[out_idx] = val;
			out_idx = (out_idx + 1) & out_mask;
		}
	}
	else if( dma.samplebits == 8 )
	{
		byte	*out = (byte *)pbuf;

		while( count-- )
		{
			val = (*p * snd_vol) >> 8;
			p += step;
			val = CLIP( val );

			out[out_idx] = (val>>8) + 128;
			out_idx = (out_idx + 1) & out_mask;
		}
	}
}

/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

// pass in index -1...count+2, return pointer to source sample in either paintbuffer or delay buffer
_inline samplepair_t * S_GetNextpFilter(int i, samplepair_t *pbuffer, samplepair_t *pfiltermem)
{
	// The delay buffer is assumed to precede the paintbuffer by 6 duplicated samples
	if (i == -1)
		return (&(pfiltermem[0]));
	if (i == 0)
		return (&(pfiltermem[1]));
	if (i == 1)
		return (&(pfiltermem[2]));

	// return from paintbuffer, where samples are doubled.  
	// even samples are to be replaced with interpolated value.

	return (&(pbuffer[(i-2)*2 + 1]));
}

// pass forward over passed in buffer and cubic interpolate all odd samples
// pbuffer: buffer to filter (in place)
// prevfilter:  filter memory. NOTE: this must match the filtertype ie: filtercubic[] for FILTERTYPE_CUBIC
// if NULL then perform no filtering. UNDONE: should have a filter memory array type
// count: how many samples to upsample. will become count*2 samples in buffer, in place.

void S_Interpolate2xCubic( samplepair_t *pbuffer, samplepair_t *pfiltermem, int cfltmem, int count )
{

// implement cubic interpolation on 2x upsampled buffer.   Effectively delays buffer contents by 2 samples.
// pbuffer: contains samples at 0, 2, 4, 6...
// temppaintbuffer is temp buffer, same size as paintbuffer, used to store processed values
// count: number of samples to process in buffer ie: how many samples at 0, 2, 4, 6...

// finpos is the fractional, inpos the integer part.
//		finpos = 0.5 for upsampling by 2x
//		inpos is the position of the sample

//		xm1 = x [inpos - 1];
//		x0 = x [inpos + 0];
//		x1 = x [inpos + 1];
//		x2 = x [inpos + 2];
//		a = (3 * (x0-x1) - xm1 + x2) / 2;
//		b = 2*x1 + xm1 - (5*x0 + x2) / 2;
//		c = (x1 - xm1) / 2;
//		y [outpos] = (((a * finpos) + b) * finpos + c) * finpos + x0;

	int i, upCount = count << 1;
	int a, b, c;
	int xm1, x0, x1, x2;
	samplepair_t *psamp0;
	samplepair_t *psamp1;
	samplepair_t *psamp2;
	samplepair_t *psamp3;
	int outpos = 0;

	Assert (upCount <= PAINTBUFFER_SIZE);

	// pfiltermem holds 6 samples from previous buffer pass

	// process 'count' samples

	for ( i = 0; i < count; i++)
	{
		
		// get source sample pointer

		psamp0 = S_GetNextpFilter(i-1, pbuffer, pfiltermem);
		psamp1 = S_GetNextpFilter(i,   pbuffer, pfiltermem);
		psamp2 = S_GetNextpFilter(i+1, pbuffer, pfiltermem);
		psamp3 = S_GetNextpFilter(i+2, pbuffer, pfiltermem);

		// write out original sample to interpolation buffer

		temppaintbuffer[outpos++] = *psamp1;

		// get all left samples for interpolation window

		xm1 = psamp0->left;
		x0 = psamp1->left;
		x1 = psamp2->left;
		x2 = psamp3->left;
		
		// interpolate

		a = (3 * (x0-x1) - xm1 + x2) / 2;
		b = 2*x1 + xm1 - (5*x0 + x2) / 2;
		c = (x1 - xm1) / 2;
		
		// write out interpolated sample

		temppaintbuffer[outpos].left = a/8 + b/4 + c/2 + x0;
		
		// get all right samples for window

		xm1 = psamp0->right;
		x0 = psamp1->right;
		x1 = psamp2->right;
		x2 = psamp3->right;
		
		// interpolate

		a = (3 * (x0-x1) - xm1 + x2) / 2;
		b = 2*x1 + xm1 - (5*x0 + x2) / 2;
		c = (x1 - xm1) / 2;
		
		// write out interpolated sample, increment output counter

		temppaintbuffer[outpos++].right = a/8 + b/4 + c/2 + x0;
		
		Assert( outpos <= ARRAYSIZE( temppaintbuffer ));
	}
	
	Assert(cfltmem >= 3);

	// save last 3 samples from paintbuffer
	
	pfiltermem[0] = pbuffer[upCount - 5];
	pfiltermem[1] = pbuffer[upCount - 3];
	pfiltermem[2] = pbuffer[upCount - 1];

	// copy temppaintbuffer back into paintbuffer

	for (i = 0; i < upCount; i++)
		pbuffer[i] = temppaintbuffer[i];
}

// pass forward over passed in buffer and linearly interpolate all odd samples
// pbuffer: buffer to filter (in place)
// prevfilter:  filter memory. NOTE: this must match the filtertype ie: filterlinear[] for FILTERTYPE_LINEAR
//				if NULL then perform no filtering.
// count: how many samples to upsample. will become count*2 samples in buffer, in place.

void S_Interpolate2xLinear( samplepair_t *pbuffer, samplepair_t *pfiltermem, int cfltmem, int count )
{
	int i, upCount = count<<1;

	Assert (upCount <= PAINTBUFFER_SIZE);
	Assert (cfltmem >= 1);

	// use interpolation value from previous mix

	pbuffer[0].left = (pfiltermem->left + pbuffer[0].left) >> 1;
	pbuffer[0].right = (pfiltermem->right + pbuffer[0].right) >> 1;

	for ( i = 2; i < upCount; i+=2)
	{
		// use linear interpolation for upsampling

		pbuffer[i].left = (pbuffer[i].left + pbuffer[i-1].left) >> 1;
		pbuffer[i].right = (pbuffer[i].right + pbuffer[i-1].right) >> 1;
	}

	// save last value to be played out in buffer

	*pfiltermem = pbuffer[upCount - 1]; 
}

// upsample by 2x, optionally using interpolation
// count: how many samples to upsample. will become count*2 samples in buffer, in place.
// pbuffer: buffer to upsample into (in place)
// pfiltermem:  filter memory. NOTE: this must match the filtertype ie: filterlinear[] for FILTERTYPE_LINEAR
//				if NULL then perform no filtering.
// cfltmem: max number of sample pairs filter can use
// filtertype: FILTERTYPE_NONE, _LINEAR, _CUBIC etc.  Must match prevfilter.

void S_MixBufferUpsample2x( int count, samplepair_t *pbuffer, samplepair_t *pfiltermem, int cfltmem, int filtertype )
{
	int i, j, upCount = count<<1;
	
	// reverse through buffer, duplicating contents for 'count' samples

	for (i = upCount - 1, j = count - 1; j >= 0; i-=2, j--)
	{	
		pbuffer[i] = pbuffer[j];
		pbuffer[i-1] = pbuffer[j];
	}
	
	// pass forward through buffer, interpolate all even slots

	switch (filtertype)
	{
	default:
		break;
	case FILTERTYPE_LINEAR:
		S_Interpolate2xLinear(pbuffer, pfiltermem, cfltmem, count);
		break;
	case FILTERTYPE_CUBIC:
		S_Interpolate2xCubic(pbuffer, pfiltermem, cfltmem, count);
		break;
	}
}

//===============================================================================
// PAINTBUFFER ROUTINES
//===============================================================================
// Set current paintbuffer to pbuf.  
// The set paintbuffer is used by all subsequent mixing, upsampling and dsp routines.
// Also sets the rear paintbuffer if paintbuffer has fsurround true.
// (otherwise, rearpaintbuffer is NULL)
_inline void MIX_SetCurrentPaintbuffer(int ipaintbuffer)
{
	// set front and rear paintbuffer
	Assert( ipaintbuffer < CPAINTBUFFERS );
	g_curpaintbuffer = paintbuffers[ipaintbuffer].pbuf;
	Assert( g_curpaintbuffer != NULL );
}

// return index to current paintbuffer
_inline int MIX_GetCurrentPaintbufferIndex( void )
{
	int	i;

	for( i = 0; i < CPAINTBUFFERS; i++ )
	{
		if( g_curpaintbuffer == paintbuffers[i].pbuf )
			return i;
	}
	return 0;
}

_inline paintbuffer_t *MIX_GetCurrentPaintbufferPtr( void )
{
	int	ipaint = MIX_GetCurrentPaintbufferIndex();
	
	Assert( ipaint < CPAINTBUFFERS );
	return &paintbuffers[ipaint];
}

int S_ConvertLoopedPosition( sfxcache_t *cache, int samplePosition )
{
	// if the wave is looping and we're past the end of the sample
	// convert to a position within the loop
	// At the end of the loop, we return a short buffer, and subsequent call
	// will loop back and get the rest of the buffer
	if( cache->loopstart >= 0 && samplePosition >= cache->length )
	{
		// size of loop
		int	loopSize = cache->length - cache->loopstart;

		// subtract off starting bit of the wave
		samplePosition -= cache->loopstart;
		
		if( loopSize )
		{
			// "real" position in memory (mod off extra loops)
			samplePosition = cache->loopstart + (samplePosition % loopSize);
		}
		// ERROR? if no loopSize
	}
	return samplePosition;
}

int S_GetOutputData( sfxcache_t *cache, void **pData, int samplePosition, int sampleCount )
{
	int	totalSampleCount;

	// handle position looping
	samplePosition = S_ConvertLoopedPosition( cache, samplePosition );

	// how many samples are available (linearly not counting looping)
	totalSampleCount = cache->length - samplePosition;

	// may be asking for a sample out of range, clip at zero
	if( totalSampleCount < 0 ) totalSampleCount = 0;

	// clip max output samples to max available
	if( sampleCount > totalSampleCount ) sampleCount = totalSampleCount;

	// byte offset in sample database
	samplePosition *= cache->width;

	// if we are returning some samples, store the pointer
	if( sampleCount )
	{
		*pData = cache->data + samplePosition;
		Assert( *pData );
	}
	return sampleCount;
}

int S_MixDataToDevice( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset )
{
	float	inputRate, rate;
	int	startingOffset = outputOffset;	// save this to compute total output
	mixer_t	*pMixer;
	
	// shouldn't be playing this if finished, but return if we are
	if( !pChannel || !pChannel->pMixer || pChannel->pMixer->m_finished )
		return 0;

	pMixer = pChannel->pMixer;	
	inputRate = ( pChannel->pitch * pChannel->pMixer->m_pData->speed );
	rate = inputRate / outputRate;

	// if we are terminating this wave prematurely, then make sure we detect the limit
	if( pMixer->m_forcedEndSample )
	{
		// How many total input samples will we need?
		int	samplesRequired = (int)(sampleCount * rate);

		// will this hit the end?
		if( pMixer->m_sample + samplesRequired >= pMixer->m_forcedEndSample )
		{
			// yes, mark finished and truncate the sample request
			pMixer->m_finished = true;
			sampleCount = (int)((pMixer->m_forcedEndSample - pMixer->m_sample) / rate );
		}
	}

	while( sampleCount > 0 )
	{
		bool	advanceSample = true;
		int	availableSamples, outputSampleCount;
		char	*pData = NULL;

		// compute number of input samples required
		double	end = pMixer->m_sample + rate * sampleCount;
		int	i, j, inputSampleCount = (int)(ceil(end) - floor(pMixer->m_sample));
		double	sampleFraction;

		// ask the source for the data
		char	copyBuf[4096];

		if( pMixer->m_delaySamples > 0 )
		{
			int	num_zero_samples = min( pMixer->m_delaySamples, inputSampleCount );
			int	sampleSize, readBytes;

			// decrement data amount
			pMixer->m_delaySamples -= num_zero_samples;

			sampleSize = pMixer->m_pData->width;
			readBytes = sampleSize * num_zero_samples;

			Assert( readBytes <= sizeof( copyBuf ));

			pData = &copyBuf[0];

			// now copy in some zeroes
			Mem_Set( pData, 0, readBytes );
			availableSamples = num_zero_samples;
			advanceSample = false;
		}
		else
		{
			availableSamples = S_GetOutputData( pMixer->m_pData, (void**)&pData,
			pMixer->m_sample, inputSampleCount );
		}

		// none available, bail out
		if( !availableSamples )
		{
			break;
		}

		sampleFraction = pMixer->m_sample - floor( pMixer->m_sample );
		if( availableSamples < inputSampleCount )
		{
			// How many samples are there given the number of input samples and the rate.
			outputSampleCount = (int)ceil((availableSamples - sampleFraction) / rate);
		}
		else
		{
			outputSampleCount = sampleCount;
		}

		// Verify that we won't get a buffer overrun.
		Assert( floor( sampleFraction + rate * ( outputSampleCount - 1 )) <= availableSamples);

		// mix this data to all active paintbuffers

		// save current paintbuffer
		j = MIX_GetCurrentPaintbufferIndex();
		
		for( i = 0; i < CPAINTBUFFERS; i++ )
		{
			if( paintbuffers[i].factive && pMixer->MixFunc )
			{
				// mix chan into all active paintbuffers
				MIX_SetCurrentPaintbuffer(i);

				pMixer->MixFunc( pChannel, pData, outputOffset,
				FIX_FLOAT(sampleFraction), FIX_FLOAT(rate), outputSampleCount );
			}
		}
		MIX_SetCurrentPaintbuffer( j );

		if( advanceSample )
			pMixer->m_sample += outputSampleCount * rate;
		outputOffset += outputSampleCount;
		sampleCount -= outputSampleCount;
	}

	// did we run out of samples? if so, mark finished
	if( sampleCount > 0 ) pMixer->m_finished = true;

	// total number of samples mixed !!! at the output clock rate !!!
	return outputOffset - startingOffset;
}

void S_PaintChannelFrom8( channel_t *ch, sfxcache_t *sc, int count, int offset )
{
	int 			data;
	int			*lscale, *rscale;
	byte			*sfx;
	samplepair_t	*samp;
	int			i;

	if( ch->leftvol > 255 ) ch->leftvol = 255;
	if( ch->rightvol > 255 ) ch->rightvol = 255;
		
	lscale = snd_scaletable[ch->leftvol>>3];
	rscale = snd_scaletable[ch->rightvol>>3];
	sfx = (signed char *)sc->data + ch->pos;

	samp = &paintbuffer[offset];

	for( i = 0; i < count; i++, samp++ )
	{
		data = sfx[i];
		samp->left += lscale[data];
		samp->right += rscale[data];
	}
	ch->pos += count;
}

void S_PaintChannelFrom16( channel_t *ch, sfxcache_t *sc, int count, int offset )
{
	int			data;
	int			left, right;
	int			leftvol, rightvol;
	signed short		*sfx;
	samplepair_t	*samp;
	int			i;

	leftvol = ch->leftvol * snd_vol;
	rightvol = ch->rightvol * snd_vol;
	sfx = (signed short *)sc->data + ch->pos;

	samp = &paintbuffer[offset];

	for( i = 0; i < count; i++, samp++ )
	{
		data = sfx[i];
		left = ( data * leftvol ) >> 8;
		right = (data * rightvol) >> 8;
		samp->left += left;
		samp->right += right;
	}
	ch->pos += count;
}

void S_PaintChannels( int endtime )
{
	channel_t 	*ch;
	sfxcache_t	*sc;
	playsound_t	*ps;
	int		i, end, ltime, count = 0;
	float		dsp_room_gain;
	float		dsp_facingaway_gain;
	float		dsp_player_gain;
	float		dsp_water_gain;

	snd_vol = s_volume->value * 256;

	CheckNewDspPresets();

	g_bDspOff = dsp_off->integer ? 1 : 0;

	if( !g_bDspOff )
		g_bdirectionalfx = dsp_facingaway->integer ? 1 : 0;
	else g_bdirectionalfx = 0;
	
	// get dsp preset gain values, update gain crossfaders,
	// used when mixing dsp processed buffers into paintbuffer

	// update crossfader - gain only used in MIX_ScaleChannelVolume
	dsp_room_gain = DSP_GetGain( idsp_room );
	// update crossfader - gain only used in MIX_ScaleChannelVolume
	dsp_facingaway_gain	= DSP_GetGain( idsp_facingaway );
	dsp_player_gain = DSP_GetGain( idsp_player );
	dsp_water_gain = DSP_GetGain( idsp_water );

	while( paintedtime < endtime )
	{
		// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if( endtime - paintedtime > PAINTBUFFER_SIZE )
			end = paintedtime + PAINTBUFFER_SIZE;

		// start any playsounds
		while( 1 )
		{
			ps = s_pendingplays.next;
			if( ps == &s_pendingplays )
				break;	// no more pending sounds
			if( ps->begin <= paintedtime )
			{
				S_IssuePlaysound( ps );
				continue;
			}

			if( ps->begin < end ) end = ps->begin; // stop here
			break;
		}

		// clear the paint buffer
		if( s_rawend < paintedtime )
		{
			Mem_Set( paintbuffer, 0, (end - paintedtime) * sizeof( samplepair_t ));
		}
		else
		{	
			int	stop;

			// copy from the streaming sound source
			stop = (end < s_rawend) ? end : s_rawend;

			for( i = paintedtime; i < stop; i++ )
				paintbuffer[i - paintedtime] = s_rawsamples[i & (MAX_RAW_SAMPLES - 1)];
			
			for( ; i < end; i++ )
				paintbuffer[i-paintedtime].left = paintbuffer[i-paintedtime].right = 0;
		}

		// paint in the channels.
		for( i = 0, ch = channels; i < MAX_CHANNELS; i++, ch++ )
		{
			ltime = paintedtime;
		
			while( ltime < end )
			{
				if( !ch->sfx || ( !ch->leftvol && !ch->rightvol ))
					break;

				// max painting is to the end of the buffer
				count = end - ltime;

				// might be stopped by running out of data
				if( ch->end - ltime < count ) count = ch->end - ltime;
		
				sc = S_LoadSound( ch->sfx, ch );
				if( !sc ) break;

				if( count > 0 && ch->sfx )
				{	
					if( sc->width == 1 )
						S_PaintChannelFrom8( ch, sc, count, ltime - paintedtime );
					else S_PaintChannelFrom16( ch, sc, count, ltime - paintedtime );
	
					ltime += count;
				}

				// if at end of loop, restart
				if( ltime >= ch->end )
				{
					if( ch->autosound && ch->use_loop )
					{	// autolooping sounds always go back to start
						ch->pos = 0;
						ch->end = ltime + sc->length;
					}
					else if( sc->loopstart >= 0 && ch->use_loop )
					{
						ch->pos = sc->loopstart;
						ch->end = ltime + sc->length - ch->pos;
					}
					else ch->sfx = NULL; // channel just stopped
				}
			}
		}

		SX_RoomFX( endtime, count, endtime - paintedtime );

		// transfer out according to DMA format
		S_TransferPaintBuffer( end );
		paintedtime = end;
	}
}

void S_InitScaletable( void )
{
	int	i, j;

	for( i = 0; i < SND_SCALE_LEVELS; i++ )
		for( j = 0; j < 256; j++ )
			snd_scaletable[i][j] = ((signed char)j) * i * (1<<SND_SCALE_SHIFT);
}