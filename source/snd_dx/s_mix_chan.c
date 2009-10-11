//=======================================================================
//			Copyright XashXT Group 2009 ©
//			 s_mix_chan.c - channel mixer
//=======================================================================

#include "sound.h"

void SND_PaintChannelFrom8( int *volume, byte *pData8, int count )
{
	int 	data;
	int		*lscale, *rscale;
	int		i;

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];

	for( i = 0; i < count; i++ )
	{
		data = pData8[i];

		paintbuffer[i].left += lscale[data];
		paintbuffer[i].right += rscale[data];
	}
}

// Applies volume scaling (evenly) to all fl,fr,rl,rr volumes
// used for voice ducking and panning between various mix busses

// Called just before mixing wav data to current paintbuffer.
// a) if another player in a multiplayer game is speaking, scale all volumes down.
// b) if mixing to IROOMBUFFER, scale all volumes by ch.dspmix and dsp_room gain
// c) if mixing to IFACINGAWAYBUFFER, scale all volumes by ch.dspface and dsp_facingaway gain
// d) If SURROUND_ON, but buffer is not surround, recombined front/rear volumes

void MIX_ScaleChannelVolume( paintbuffer_t *ppaint, channel_t *pChannel, int volume[CCHANVOLUMES], int mixchans )
{
	float	scale, cone;
	int	i, *pvol;
	int	mixflag = ppaint->flags;
	char	wavtype = pChannel->wavtype;

	pvol = &pChannel->leftvol;

	// copy channel volumes into output array
	for( i = 0; i < CCHANVOLUMES; i++ )
		volume[i] = pvol[i];

	// If mixing to the room buss, adjust volume based on channel's dspmix setting.
	// dspmix is DSP_MIX_MAX (~0.78) if sound is far from player, DSP_MIX_MIN (~0.24) if sound is near player

	if( mixflag & SOUND_BUSS_ROOM )
	{
		// get current idsp_room gain
		float	dsp_gain = DSP_GetGain( idsp_room );

		// if dspmix is 1.0, 100% of sound goes to both IROOMBUFFER and IFACINGBUFFER
		for( i = 0; i < CCHANVOLUMES; i++ )
			volume[i] = (int)((float)(volume[i]) * pChannel->dspmix * dsp_gain );
	}

	// If mixing to facing/facingaway buss, adjust volume based on sound entity's facing direction.

	// If sound directly faces player, ch->dspface = 1.0.  If facing directly away, ch->dspface = -1.0.
	// mix to lowpass buffer if facing away, to allpass if facing
	
	// scale 1.0 - facing player, scale 0, facing away

	scale = (pChannel->dspface + 1.0) / 2.0;

	// UNDONE: get front cone % from channel to set this.

	// bias scale such that 1.0 to 'cone' is considered facing.  Facing cone narrows as cone -> 1.0
	// and 'cone' -> 0.0 becomes 1.0 -> 0.0

	cone = 0.6f;
	scale = scale * ( 1 / cone );
	scale = bound( 0.0f, scale, 1.0f );

	// pan between facing and facing away buffers

	if( !g_bdirectionalfx || wavtype != CHAR_DIRECTIONAL )
	{
		// if no directional fx mix 0% to facingaway buffer
		// if wavtype is DOPPLER, mix 0% to facingaway buffer - DOPPLER wavs have a custom mixer
		// if wavtype is OMNI, mix 0% to faceingaway buffer - OMNI wavs have no directionality
		// if wavtype is DIRECTIONAL and stereo encoded, mix 0% to facingaway buffer - 
		// DIRECTIONAL STEREO wavs have a custom mixer
		scale = 1.0;
	}

	if( mixflag & SOUND_BUSS_FACING )
	{
		// facing player
		// if dspface is 1.0, 100% of sound goes to IFACINGBUFFER
		for( i = 0; i < CCHANVOLUMES; i++ )
			volume[i] = (int)((float)(volume[i]) * scale * (1.0 - pChannel->dspmix));
	}
	else if( mixflag & SOUND_BUSS_FACINGAWAY )
	{
		// facing away from player
		// if dspface is 0.0, 100% of sound goes to IFACINGAWAYBUFFER

		// get current idsp_facingaway gain
		float dsp_gain = DSP_GetGain( idsp_facingaway );

		for( i = 0; i < CCHANVOLUMES; i++ )
			volume[i] = (int)((float)(volume[i]) * (1.0-scale) * dsp_gain * (1.0-pChannel->dspmix));
	}

	// NOTE: this must occur last in this routine: 
	for( i = 0; i < CCHANVOLUMES; i++ )
		volume[i] = bound( 0, volume[i], 255 );
}

//===============================================================================
// SOFTWARE MIXING ROUTINES
//===============================================================================

// UNDONE: optimize these

// grab samples from left source channel only and mix as if mono. 
// volume array contains appropriate spatialization volumes for doppler left (incoming sound)

void SW_Mix8StereoDopplerLeft( samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, uint rateScaleFix, int outCount ) 
{
	int	sampleIndex = 0;
	uint	sampleFrac = inputOffset;
	int	i, *lscale, *rscale;

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];

	for( i = 0; i < outCount; i++ )
	{
		pOutput[i].left += lscale[pData[sampleIndex]];
		pOutput[i].right += rscale[pData[sampleIndex]];
		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART( sampleFrac )<<1;
		sampleFrac = FIX_FRACPART( sampleFrac );
	}
}

// grab samples from right source channel only and mix as if mono.
// volume array contains appropriate spatialization volumes for doppler right (outgoing sound)
void SW_Mix8StereoDopplerRight( samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, uint rateScaleFix, int outCount ) 
{
	int	sampleIndex = 0;
	uint	sampleFrac = inputOffset;
	int	i, *lscale, *rscale;

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];

	for( i = 0; i < outCount; i++ )
	{
		pOutput[i].left += lscale[pData[sampleIndex+1]];
		pOutput[i].right += rscale[pData[sampleIndex+1]];
		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}

}

// grab samples from left source channel only and mix as if mono. 
// volume array contains appropriate spatialization volumes for doppler left (incoming sound)
void SW_Mix16StereoDopplerLeft( samplepair_t *pOutput, int *volume, short *pData, int inputOffset, uint rateScaleFix, int outCount ) 
{
	int	i, sampleIndex = 0;
	uint	sampleFrac = inputOffset;

	for( i = 0; i < outCount; i++ )
	{
		pOutput[i].left += (volume[0] * (int)(pData[sampleIndex]))>>8;
		pOutput[i].right += (volume[1] * (int)(pData[sampleIndex]))>>8;

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART( sampleFrac )<<1;
		sampleFrac = FIX_FRACPART( sampleFrac );
	}
}


// grab samples from right source channel only and mix as if mono.
// volume array contains appropriate spatialization volumes for doppler right (outgoing sound)

void SW_Mix16StereoDopplerRight( samplepair_t *pOutput, int *volume, short *pData, int inputOffset, uint rateScaleFix, int outCount ) 
{
	int	i, sampleIndex = 0;
	uint	sampleFrac = inputOffset;
	
	for( i = 0; i < outCount; i++ )
	{
		pOutput[i].left += (volume[0] * (int)(pData[sampleIndex+1]))>>8;
		pOutput[i].right += (volume[1] * (int)(pData[sampleIndex+1]))>>8;

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}

// mix left wav (front facing) with right wav (rear facing) based on soundfacing direction
void SW_Mix8StereoDirectional( float soundfacing, samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, uint rateScaleFix, int outCount ) 
{
	int		sampleIndex = 0;
	uint		sampleFrac = inputOffset;
	int		i, x;
	int		l, r;
	signed char	lb,rb;
	
	// if soundfacing -1.0, sound source is facing away from player
	// if soundfacing 0.0, sound source is perpendicular to player
	// if soundfacing 1.0, sound source is facing player

	int	frontmix = (int)(256.0f * ((1.f + soundfacing) / 2.f));	// 0 -> 256
	int	rearmix  = (int)(256.0f * ((1.f - soundfacing) / 2.f));	// 256 -> 0

	for( i = 0; i < outCount; i++ )
	{
		lb = (pData[sampleIndex]);		// get left byte
		rb = (pData[sampleIndex+1]);		// get right byte

		l = ((int)lb) << 8;			// convert to 16 bit. UNDONE: better dithering
		r = ((int)rb) << 8;

		x = ((l * frontmix) >> 8) + ((r * rearmix) >> 8);

		pOutput[i].left += (volume[0] * (int)(x))>>8;
		pOutput[i].right += (volume[1] * (int)(x))>>8;

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART( sampleFrac )<<1;
		sampleFrac = FIX_FRACPART( sampleFrac );
	}
}

// mix left wav (front facing) with right wav (rear facing) based on soundfacing direction
void SW_Mix16StereoDirectional( float soundfacing, samplepair_t *pOutput, int *volume, short *pData, int inputOffset, uint rateScaleFix, int outCount ) 
{
	int	sampleIndex = 0;
	uint	sampleFrac = inputOffset;
	int	i, x;
	int	l, r;
	
	// if soundfacing -1.0, sound source is facing away from player
	// if soundfacing 0.0, sound source is perpendicular to player
	// if soundfacing 1.0, sound source is facing player

	int	frontmix = (int)(256.0f * ((1.f + soundfacing) / 2.f));	// 0 -> 256
	int	rearmix  = (int)(256.0f * ((1.f - soundfacing) / 2.f));	// 256 -> 0

	for( i = 0; i < outCount; i++ )
	{
		l = (int)(pData[sampleIndex]);
		r = (int)(pData[sampleIndex+1]);

		x = ((l * frontmix) >> 8) + ((r * rearmix) >> 8);

		pOutput[i].left += (volume[0] * (int)(x))>>8;
		pOutput[i].right += (volume[1] * (int)(x))>>8;

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}

void SW_Mix8StereoDistVar( float distmix, samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, uint rateScaleFix, int outCount ) 
{
	int		sampleIndex = 0;
	uint		sampleFrac = inputOffset;
	int		i, x;
	int		l, r;
	signed char	lb, rb;

	// distmix 0 - sound is near player (100% wav left)
	// distmix 1.0 - sound is far from player (100% wav right)
	int		nearmix  = (int)(256.0f * (1.0f - distmix));
	int		farmix = (int)(256.0f * distmix);
	
	// if mixing at max or min range, skip crossfade (KDB: perf)

	if( !nearmix )
	{
		for( i = 0; i < outCount; i++ )
		{
			rb = (pData[sampleIndex+1]);	// get right byte
			x = ((int)rb) << 8;	 // convert to 16 bit. UNDONE: better dithering
			
			pOutput[i].left += (volume[0] * (int)(x))>>8;
			pOutput[i].right += (volume[1] * (int)(x))>>8;

			sampleFrac += rateScaleFix;
			sampleIndex += FIX_INTPART(sampleFrac)<<1;
			sampleFrac = FIX_FRACPART(sampleFrac);
		}
		return;
	}

	if( !farmix )
	{
		for( i = 0; i < outCount; i++ )
		{
			lb = (pData[sampleIndex]); // get left byte
			x = ((int)lb) << 8;	 // convert to 16 bit. UNDONE: better dithering
			
			pOutput[i].left += (volume[0] * (int)(x))>>8;
			pOutput[i].right += (volume[1] * (int)(x))>>8;

			sampleFrac += rateScaleFix;
			sampleIndex += FIX_INTPART(sampleFrac)<<1;
			sampleFrac = FIX_FRACPART(sampleFrac);
		}
		return;
	}

	// crossfade left/right
	for( i = 0; i < outCount; i++ )
	{
		lb = (pData[sampleIndex]);		// get left byte
		rb = (pData[sampleIndex+1]);	// get right byte

		l = ((int)lb) << 8;			// convert to 16 bit. UNDONE: better dithering
		r = ((int)rb) << 8;
		
		x = ((l * nearmix) >> 8) + ((r * farmix) >> 8);

		pOutput[i].left += (volume[0] * (int)(x))>>8;
		pOutput[i].right += (volume[1] * (int)(x))>>8;

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}

void SW_Mix16StereoDistVar( float distmix, samplepair_t *pOutput, int *volume, short *pData, int inputOffset, uint rateScaleFix, int outCount ) 
{
	int	sampleIndex = 0;
	uint	sampleFrac = inputOffset; 
	int	i, x;
	int	l, r;
	
	// distmix 0 - sound is near player (100% wav left)
	// distmix 1.0 - sound is far from player (100% wav right)
	int	nearmix = (int)( 256.0f * ( 1.0f - distmix ));
	int	farmix =  (int)( 256.0f * distmix );

	// if mixing at max or min range, skip crossfade (KDB: perf)
	if( !nearmix )
	{
		for( i = 0; i < outCount; i++ )
		{
			x = pData[sampleIndex+1];	// right sample

			pOutput[i].left += (volume[0] * x)>>8;
			pOutput[i].right += (volume[1] * x)>>8;

			sampleFrac += rateScaleFix;
			sampleIndex += FIX_INTPART(sampleFrac)<<1;
			sampleFrac = FIX_FRACPART(sampleFrac);
		}
		return;
	}

	if( !farmix )
	{
		for( i = 0; i < outCount; i++ )
		{
			x = pData[sampleIndex];		// left sample
		
			pOutput[i].left += (volume[0] * x)>>8;
			pOutput[i].right += (volume[1] * x)>>8;

			sampleFrac += rateScaleFix;
			sampleIndex += FIX_INTPART(sampleFrac)<<1;
			sampleFrac = FIX_FRACPART(sampleFrac);
		}
		return;
	}

	// crossfade left/right
	for( i = 0; i < outCount; i++ )
	{
		l = pData[sampleIndex];
		r = pData[sampleIndex+1];

		x = ((l * nearmix) >> 8) + ((r * farmix) >> 8);

		pOutput[i].left += (volume[0] * x)>>8;
		pOutput[i].right += (volume[1] * x)>>8;

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}


void SW_Mix8Mono( samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, uint rateScaleFix, int outCount )
{
	// UNDONE: Native code this and use adc to integrate the int/frac parts separately
	// UNDONE: Optimize when not pitch shifting?
	int	sampleIndex = 0;
	uint	sampleFrac = inputOffset;
	int	i, *lscale, *rscale;

	// not using pitch shift?
	if( rateScaleFix == FIX( 1 ))
	{
		// paintbuffer native code
		if( pOutput == paintbuffer )
		{
			SND_PaintChannelFrom8( volume, (byte *)pData, outCount );
			return;
		}

	}

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];

	for( i = 0; i < outCount; i++ )
	{
		pOutput[i].left += lscale[pData[sampleIndex]];
		pOutput[i].right += rscale[pData[sampleIndex]];
		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac);
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}

void SW_Mix8Stereo( samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, uint rateScaleFix, int outCount )
{
	int	sampleIndex = 0;
	uint	sampleFrac = inputOffset;
	int	i, *lscale, *rscale;

	lscale = snd_scaletable[volume[0] >> SND_SCALE_SHIFT];
	rscale = snd_scaletable[volume[1] >> SND_SCALE_SHIFT];

	for( i = 0; i < outCount; i++ )
	{
		pOutput[i].left += lscale[pData[sampleIndex]];
		pOutput[i].right += rscale[pData[sampleIndex+1]];
		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}

void SW_Mix16Mono( samplepair_t *pOutput, int *volume, short *pData, int inputOffset, uint rateScaleFix, int outCount )
{
	int	i, sampleIndex = 0;
	uint	sampleFrac = inputOffset;

	for( i = 0; i < outCount; i++ )
	{
		pOutput[i].left += (volume[0] * (int)(pData[sampleIndex]))>>8;
		pOutput[i].right += (volume[1] * (int)(pData[sampleIndex]))>>8;
		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac);
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}

void SW_Mix16Stereo( samplepair_t *pOutput, int *volume, short *pData, int inputOffset, uint rateScaleFix, int outCount )
{
	int	i, sampleIndex = 0;
	uint	sampleFrac = inputOffset;

	for( i = 0; i < outCount; i++ )
	{
		pOutput[i].left += (volume[0] * (int)(pData[sampleIndex]))>>8;
		pOutput[i].right += (volume[1] * (int)(pData[sampleIndex+1]))>>8;

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}


//===============================================================================
// DISPATCHERS FOR MIXING ROUTINES
//===============================================================================
void Mix8MonoWavtype( channel_t *pChannel, samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, uint rateScaleFix, int outCount )
{
	SW_Mix8Mono( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
}

void Mix16MonoWavtype( channel_t *pChannel, samplepair_t *pOutput, int *volume, short *pData, int inputOffset, uint rateScaleFix, int outCount )
{
	SW_Mix16Mono( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
}

void Mix8StereoWavtype( channel_t *pChannel, samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, uint rateScaleFix, int outCount )
{
	switch ( pChannel->wavtype )
	{
	case CHAR_DOPPLER:
		SW_Mix8StereoDopplerLeft( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		SW_Mix8StereoDopplerRight( pOutput, &volume[IFRONT_LEFTD], pData, inputOffset, rateScaleFix, outCount );
		break;
	case CHAR_DIRECTIONAL:
		SW_Mix8StereoDirectional( pChannel->dspface, pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		break;
	case CHAR_DISTVARIANT:
		SW_Mix8StereoDistVar( pChannel->distmix, pOutput, volume, pData, inputOffset, rateScaleFix, outCount);
		break;
	default:
		SW_Mix8Stereo( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		break;
	}
}

void Mix16StereoWavtype( channel_t *pChannel, samplepair_t *pOutput, int *volume, short *pData, int inputOffset, uint rateScaleFix, int outCount )
{
	switch ( pChannel->wavtype )
	{
	case CHAR_DOPPLER:
		SW_Mix16StereoDopplerLeft( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		SW_Mix16StereoDopplerRight( pOutput, &volume[IFRONT_LEFTD], pData, inputOffset, rateScaleFix, outCount );
		break;
	case CHAR_DIRECTIONAL:
		SW_Mix16StereoDirectional( pChannel->dspface, pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		break;
	case CHAR_DISTVARIANT:
		SW_Mix16StereoDistVar( pChannel->distmix, pOutput, volume, pData, inputOffset, rateScaleFix, outCount);
		break;
	default:
		SW_Mix16Stereo( pOutput, volume, pData, inputOffset, rateScaleFix, outCount );
		break;
	}
}

void Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, uint rateScaleFix, int outCount )
{
	int		volume[CCHANVOLUMES];
	paintbuffer_t	*ppaint = MIX_GetCurrentPaintbufferPtr();

	MIX_ScaleChannelVolume( ppaint, pChannel, volume, 1 );

	Mix8MonoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, (byte *)pData, inputOffset, rateScaleFix, outCount );
}


void Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, uint rateScaleFix, int outCount )
{
	int		volume[CCHANVOLUMES];
	paintbuffer_t	*ppaint = MIX_GetCurrentPaintbufferPtr();

	MIX_ScaleChannelVolume( ppaint, pChannel, volume, 2 );
	Mix8StereoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, (byte *)pData, inputOffset, rateScaleFix, outCount );
}


void Mix16Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, uint rateScaleFix, int outCount )
{
	int		volume[CCHANVOLUMES];
	paintbuffer_t	*ppaint = MIX_GetCurrentPaintbufferPtr();

	MIX_ScaleChannelVolume( ppaint, pChannel, volume, 1 );
	Mix16MonoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, (short *)pData, inputOffset, rateScaleFix, outCount );
}


void Mix16Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, uint rateScaleFix, int outCount )
{
	int		volume[CCHANVOLUMES];
	paintbuffer_t	*ppaint = MIX_GetCurrentPaintbufferPtr();

	MIX_ScaleChannelVolume( ppaint, pChannel, volume, 2 );
	Mix16StereoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, (short *)pData, inputOffset, rateScaleFix, outCount );
}
