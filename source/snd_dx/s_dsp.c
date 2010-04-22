//=======================================================================
//			Copyright XashXT Group 2009 ©
//	   s_dsp.c - digital signal processing algorithms for audio FX
//=======================================================================

#include "sound.h"

#define SXDLY_MAX		0.400	// max delay in seconds
#define SXRVB_MAX		0.100	// max reverb reflection time
#define SXSTE_MAX		0.100	// max stereo delay line time
#define SOUNDCLIP(x)	((x) > (32767*256) ? (32767*256) : ((x) < (-32767*256) ? (-32767*256) : (x)))
#define CSXROOM		29
#define DSP_CONSTANT_GAIN	128

typedef int sample_t;		// delay lines must be 32 bit, now that we have 16 bit samples

typedef struct dlyline_s
{
	int	cdelaysamplesmax;	// size of delay line in samples
	int	lp;												// lowpass flag 0 = off, 1 = on

	int	idelayinput;	// i/o indices into circular delay line
	int	idelayoutput;
	int	idelayoutputxf;	// crossfade output pointer
	int	xfade;		// crossfade value

	int	delaysamples;	// current delay setting
	int	delayfeed;	// current feedback setting

	int	lp0, lp1, lp2, lp3, lp4, lp5;	// lowpass filter buffer

	int	mod;		// sample modulation count
	int	modcur;
	sample_t	*lpdelayline;	// buffer
} dlyline_t;

#define ISXMONODLY		0	// mono delay line
#define ISXRVB		1	// first of the reverb delay lines
#define CSXRVBMAX		2
#define ISXSTEREODLY	3	// 50ms left side delay
#define CSXDLYMAX		4

dlyline_t	rgsxdly[CSXDLYMAX];		// array of delay lines

#define gdly0 (rgsxdly[ISXMONODLY])
#define gdly1 (rgsxdly[ISXRVB])
#define gdly2 (rgsxdly[ISXRVB + 1])
#define gdly3 (rgsxdly[ISXSTEREODLY])

#define CSXLPMAX		10		// lowpass filter memory

int	rgsxlp[CSXLPMAX];		
int	sxamodl, sxamodr;			// amplitude modulation values
int	sxamodlt, sxamodrt;			// modulation targets
int	sxmod1, sxmod2;
int	sxmod1cur, sxmod2cur;

// Mono Delay parameters
cvar_t	*sxdly_delay;	// current delay in seconds
cvar_t	*sxdly_feedback;	// cycles
cvar_t	*sxdly_lp;	// lowpass filter

// Mono Reverb parameters
cvar_t	*sxrvb_size;	// room size 0 (off) 0.1 small - 0.35 huge
cvar_t	*sxrvb_feedback;	// reverb decay 0.1 short - 0.9 long
cvar_t	*sxrvb_lp;	// lowpass filter		

// stereo delay (no feedback)
cvar_t	*sxste_delay;	// straight left delay

// Underwater/special fx modulations
cvar_t	*sxmod_lowpass;
cvar_t	*sxmod_mod;
int	sxroom_typeprev;

// Main interface
cvar_t	*sxroom_type;	// legacy support
cvar_t	*sxroomwater_type;	// legacy support
cvar_t	*sxroom_off;	// legacy support

bool SXDLY_Init( int idelay, float delay );
void SXDLY_Free( int idelay );
void SXDLY_DoDelay( int count );
void SXRVB_DoReverb( int count );
void SXDLY_DoStereoDelay( int count );
void SXRVB_DoAMod( int count );

//=====================================================================
// Init/release all structures for sound effects
//=====================================================================
void SX_Init( void )
{
	sxdly_delay = Cvar_Get( "room_delay", "0", 0, "current delay in seconds" ); 
	sxdly_feedback = Cvar_Get( "room_feedback", "0.2", 0, "cycles" );
	sxdly_lp = Cvar_Get( "room_dlylp", "1", 0, "lowpass filter" ); 

	sxrvb_size = Cvar_Get( "room_size", "0", 0, "room size 0 (off) 0.1 small - 0.35 huge" ); 
	sxrvb_feedback = Cvar_Get( "room_refl", "0.7", 0, "reverb decay 0.1 short - 0.9 long" ); 
	sxrvb_lp = Cvar_Get( "room_rvblp", "1", 0, "lowpass filter" ); 		

	sxste_delay = Cvar_Get( "room_left", "0", 0, "straight left delay" ); 
	sxmod_lowpass = Cvar_Get( "room_lp", "0", 0, "no description" );
	sxmod_mod	= Cvar_Get( "room_mod", "0", 0, "no description" );

	sxroom_type = Cvar_Get( "room_type", "0", 0, "no description" );
	sxroomwater_type = Cvar_Get( "waterroom_type", "14", 0, "no description" );
	sxroom_off = Cvar_Get( "room_off", "0", 0, "no description" );

	Mem_Set( rgsxdly, 0, sizeof( dlyline_t ) * CSXDLYMAX );
	Mem_Set( rgsxlp, 0, sizeof( int ) * CSXLPMAX );

	sxroom_typeprev = -1;

	// init amplitude modulation params
	sxamodl = sxamodr = 255;
	sxamodlt = sxamodrt = 255;
		
	sxmod1 = 350 * (dma.speed / 11025);	// 11k was the original sample rate all dsp was tuned at	
	sxmod2 = 450 * (dma.speed / 11025);
	sxmod1cur = sxmod1;
	sxmod2cur = sxmod2;

	MsgDev( D_INFO, "FX Processor Initialized\n" );
}

void SX_Free( void )
{
	int	i;

	// release mono delay line
	SXDLY_Free( ISXMONODLY );

	// release reverb lines
	for( i = 0; i < CSXRVBMAX; i++ )
		SXDLY_Free( i + ISXRVB );
	SXDLY_Free( ISXSTEREODLY );
}

// Set up a delay line buffer allowing a max delay of 'delay' seconds 
// Frees current buffer if it already exists. idelay indicates which of 
// the available delay lines to init.
bool SXDLY_Init( int idelay, float delay )
{
	size_t		cbsamples;
	dlyline_t 	*pdly;

	pdly = &(rgsxdly[idelay]);

	if( delay > SXDLY_MAX )
		delay = SXDLY_MAX;
	
	if( pdly->lpdelayline )
	{
		Mem_Free( pdly->lpdelayline );
		pdly->lpdelayline = NULL;
	}

	if( delay == 0.0 )
		return true;

	pdly->cdelaysamplesmax = dma.speed * delay;
	pdly->cdelaysamplesmax += 1;

	cbsamples = pdly->cdelaysamplesmax * sizeof( sample_t );
	pdly->lpdelayline = (sample_t *)Z_Malloc( cbsamples );

	// init delay loop input and output counters.
	// NOTE: init of idelayoutput only valid if pdly->delaysamples is set
	// NOTE: before this call!

	pdly->idelayinput = 0;
	pdly->idelayoutput = pdly->cdelaysamplesmax - pdly->delaysamples; 
	pdly->xfade = 0;
	pdly->lp = 1;
	pdly->mod = 0;
	pdly->modcur = 0;

	// init lowpass filter memory
	pdly->lp0 = pdly->lp1 = pdly->lp2 = pdly->lp3 = pdly->lp4 = pdly->lp5 = 0;

	return true;
}

// release delay buffer and deactivate delay
void SXDLY_Free( int idelay )
{
	dlyline_t	*pdly = &(rgsxdly[idelay]);

	if( pdly->lpdelayline )
	{
		Mem_Free( pdly->lpdelayline );
		pdly->lpdelayline = NULL;	// this deactivates the delay
	}
}


// check for new stereo delay param
void SXDLY_CheckNewStereoDelayVal( void )
{
	dlyline_t	*pdly = &(rgsxdly[ISXSTEREODLY]);
	int	delaysamples;

	if( dma.channels < 2 )
		return;

	// set up stereo delay
	if( sxste_delay->modified )
	{
		sxste_delay->modified = false;
			
		if( sxste_delay->value == 0.0 )
		{
			// deactivate delay line
			SXDLY_Free( ISXSTEREODLY );
		}
		else
		{
			delaysamples = min( sxste_delay->value, SXSTE_MAX ) * dma.speed;

			// init delay line if not active
			if( pdly->lpdelayline == NULL )
			{
				pdly->delaysamples = delaysamples;
				SXDLY_Init( ISXSTEREODLY, SXSTE_MAX );
			}

			// do crossfade to new delay if delay has changed
			if( delaysamples != pdly->delaysamples )
			{
				
				// set up crossfade from old pdly->delaysamples to new delaysamples
				pdly->idelayoutputxf = pdly->idelayinput - delaysamples;

				if( pdly->idelayoutputxf < 0 )
					pdly->idelayoutputxf += pdly->cdelaysamplesmax;
				pdly->xfade = 128;
			} 

			pdly->mod = 0;
			pdly->modcur = pdly->mod;

			// deactivate line if rounded down to 0 delay
			if( pdly->delaysamples == 0 )
				SXDLY_Free( ISXSTEREODLY );
				
		}
	}
}

// stereo delay, left channel only, no feedback

void SXDLY_DoStereoDelay( int count )
{
	int			left;
	sample_t			sampledly;
	sample_t			samplexf;
	portable_samplepair_t	*pbuf;
	int			countr;

	if( dma.channels < 2 )
		return;

	// process delay line if active
	if( rgsxdly[ISXSTEREODLY].lpdelayline )
	{
		pbuf = paintbuffer;
		countr = count;
		
		// process each sample in the paintbuffer...
		while( countr-- )
		{
			if( gdly3.mod && ( --gdly3.modcur < 0 ))
				gdly3.modcur = gdly3.mod;
		
			// get delay line sample from left line
			sampledly = *(gdly3.lpdelayline + gdly3.idelayoutput);
			left = pbuf->left;
			
			// only process if left value or delayline value are non-zero or xfading
			if( gdly3.xfade || sampledly || left )
			{
				// if we're not crossfading, and we're not modulating, but we'd like to be modulating,
				// then setup a new crossfade.

				if( !gdly3.xfade && !gdly3.modcur && gdly3.mod )
				{
					// set up crossfade to new delay value, if we're not already doing an xfade
					gdly3.idelayoutputxf = gdly3.idelayoutput + ((Com_RandomLong(0, 0xFF) * gdly3.delaysamples) >> 9); // 100 = ~ 9ms

					if( gdly3.idelayoutputxf >= gdly3.cdelaysamplesmax )
						gdly3.idelayoutputxf -= gdly3.cdelaysamplesmax;

					gdly3.xfade = 128;
				}
				
				// modify sampledly if crossfading to new delay value
				if( gdly3.xfade )
				{
					samplexf = (*(gdly3.lpdelayline + gdly3.idelayoutputxf) * (128 - gdly3.xfade)) >> 7;
					sampledly = ((sampledly * gdly3.xfade) >> 7) + samplexf;
					
					if( ++gdly3.idelayoutputxf >= gdly3.cdelaysamplesmax )
						gdly3.idelayoutputxf = 0;
	
					if( --gdly3.xfade == 0 ) 
						gdly3.idelayoutput = gdly3.idelayoutputxf;
				} 
				
				// save output value into delay line

				// left = CLIP(left);
				*(gdly3.lpdelayline + gdly3.idelayinput) = SOUNDCLIP( left );
								
				// save delay line sample into output buffer
				pbuf->left = SOUNDCLIP( sampledly );

			} 
			else 
			{
				// keep clearing out delay line, even if no signal in or out
				*(gdly3.lpdelayline + gdly3.idelayinput) = 0;
			}

			// update delay buffer pointers
			if( ++gdly3.idelayinput >= gdly3.cdelaysamplesmax )
				gdly3.idelayinput = 0;
			
			if( ++gdly3.idelayoutput >= gdly3.cdelaysamplesmax )
				gdly3.idelayoutput = 0;
			pbuf++;
		}

	}
}

// If sxdly_delay or sxdly_feedback have changed, update delaysamples
// and delayfeed values.  This applies only to delay 0, the main echo line.

void SXDLY_CheckNewDelayVal( void )
{
	dlyline_t	*pdly = &(rgsxdly[ISXMONODLY]);

	if( sxdly_delay->modified )
	{
		sxdly_delay->modified = false;

		if( sxdly_delay->value == 0.0f )
		{
			// deactivate delay line
			SXDLY_Free( ISXMONODLY );
		}
		else
		{
			// init delay line if not active

			pdly->delaysamples = min( sxdly_delay->value, SXDLY_MAX ) * dma.speed;

			if( pdly->lpdelayline == NULL )
				SXDLY_Init( ISXMONODLY, SXDLY_MAX );

			// flush delay line and filters

			if( pdly->lpdelayline )
			{
				Mem_Set( pdly->lpdelayline, 0, pdly->cdelaysamplesmax * sizeof( sample_t ));
				pdly->lp0 = 0;
				pdly->lp1 = 0;
				pdly->lp2 = 0;
				pdly->lp3 = 0;
				pdly->lp4 = 0;
				pdly->lp5 = 0;
			}
			
			// init delay loop input and output counters
			pdly->idelayinput = 0;
			pdly->idelayoutput = pdly->cdelaysamplesmax - pdly->delaysamples; 
		
			// deactivate line if rounded down to 0 delay
			if( pdly->delaysamples == 0 )
				SXDLY_Free( ISXMONODLY );
			
		}
	}

	pdly->lp = sxdly_lp->integer;
	pdly->delayfeed = sxdly_feedback->value * 255;
}


// This routine updates both left and right output with 
// the mono delayed signal.  Delay is set through console vars room_delay
// and room_feedback.
void SXDLY_DoDelay( int count ) 
{
	int			val[2];
	int			valt, valt2;
	int			samp[2];
	int			sampsum, dfdly;
	sample_t			sampledly;
	portable_samplepair_t	*pbuf;
	int			countr;
	int			gain;

	// process mono delay line if active
	if( rgsxdly[ISXMONODLY].lpdelayline ) 
	{
		gain = DSP_CONSTANT_GAIN;
   		pbuf = paintbuffer;
		countr = count;

		// process each sample in the paintbuffer...
		while( countr-- ) 
		{
			// get delay line sample
			sampledly = *(gdly0.lpdelayline + gdly0.idelayoutput);

			if( dma.channels == 2 )
			{
				samp[0] = pbuf->left;
				samp[1] = pbuf->right;
				sampsum = samp[0] + samp[1];
			}
			else
			{
				samp[0] = pbuf->left;
				sampsum = samp[0];
			}
		
			// only process if delay line and paintbuffer samples are non zero
			if( sampledly || sampsum ) 
			{
				// get current sample from delay buffer
				dfdly = ((gdly0.delayfeed * sampledly) >> 8);

				if( dma.channels == 2 )
				{
					val[0] = SOUNDCLIP( samp[0] + dfdly );
					val[1] = SOUNDCLIP( samp[1] + dfdly );
					valt = SOUNDCLIP(( sampsum + (dfdly * 2)) >> 1);
				}
				else
				{
					val[0] = SOUNDCLIP(samp[0] + dfdly);
					valt = val[0];
				}
				
				// lowpass
				if( gdly0.lp ) 
				{
					if( dma.channels == 2 )
					{
						val[0] = (gdly0.lp0 + gdly0.lp1 + gdly0.lp2 + gdly0.lp3 + val[0]) / 5;
						val[1] = (gdly0.lp0 + gdly0.lp1 + gdly0.lp2 + gdly0.lp3 + val[1]) / 5;
						valt2 = (val[0] + val[1]) >> 1;
					}
					else
					{
						val[0] = (gdly0.lp0 + gdly0.lp1 + gdly0.lp2 + gdly0.lp3 + val[0]) / 5;
						valt2 = val[0];
					}

					gdly0.lp0 = gdly0.lp1;
					gdly0.lp1 = gdly0.lp2;
					gdly0.lp2 = gdly0.lp3;
					gdly0.lp3 = valt;
				} 
				else 
				{
					valt2 = valt;
				}

				// store delay output value into output buffer
				
				*(gdly0.lpdelayline + gdly0.idelayinput) = SOUNDCLIP( valt2 );
				
				// decrease output value by max gain of delay with feedback
				// to provide for unity gain reverb
				// note: this gain varies with the feedback value.
				if( dma.channels == 2 )
				{
					pbuf->left = ((val[0] * gain) >> 8);
					pbuf->right = ((val[1] * gain) >> 8);
				}
				else
				{
					pbuf->left = ((val[0] * gain) >> 8);
				}
			} 
			else 
			{
				// not playing samples, but must still flush lowpass buffer and delay line
				gdly0.lp0 = gdly0.lp1 = gdly0.lp2 = gdly0.lp3 = 0;
				*(gdly0.lpdelayline + gdly0.idelayinput) = valt2;

			}

			// update delay buffer pointers
			if( ++gdly0.idelayinput >= gdly0.cdelaysamplesmax )
				gdly0.idelayinput = 0;
			
			if( ++gdly0.idelayoutput >= gdly0.cdelaysamplesmax )
				gdly0.idelayoutput = 0;			
			pbuf++;
		}
	}
}

// check for a parameter change on the reverb processor
#define RVB_XFADE		(32 * dma.speed / 11025)	// xfade time between new delays
#define RVB_MODRATE1	(500 * (dma.speed / 11025))	// how often, in samples, to change delay (1st rvb)
#define RVB_MODRATE2	(700 * (dma.speed / 11025))	// how often, in samples, to change delay (2nd rvb)

void SXRVB_CheckNewReverbVal( void ) 
{
	dlyline_t *pdly;
	int	delaysamples;
	int	i, mod;

	if( sxrvb_size->modified ) 
	{
		sxrvb_size->modified = false;
		
		if( sxrvb_size->value == 0.0 ) 
		{
			// deactivate all delay lines
			SXDLY_Free( ISXRVB );
			SXDLY_Free( ISXRVB + 1 );
		} 
		else 
		{			
			for( i = ISXRVB; i < ISXRVB + CSXRVBMAX; i++ ) 
			{
				// init delay line if not active
				pdly = &(rgsxdly[i]);
				
				switch( i )
				{
				case ISXRVB:					
					delaysamples = min(sxrvb_size->value, SXRVB_MAX) * dma.speed;
					pdly->mod = RVB_MODRATE1;		
					break;
				case ISXRVB+1:
					delaysamples = min(sxrvb_size->value * 0.71, SXRVB_MAX) * dma.speed;
					pdly->mod = RVB_MODRATE2;
					break;
				default:
					delaysamples = 0;
					break;
				}

				mod = pdly->mod;

				if( pdly->lpdelayline == NULL ) 
				{
					pdly->delaysamples = delaysamples;
					SXDLY_Init( i, SXRVB_MAX );
				}
				
				pdly->modcur = pdly->mod = mod;

				// do crossfade to new delay if delay has changed
				if( delaysamples != pdly->delaysamples ) 
				{
					// set up crossfade from old pdly->delaysamples to new delaysamples
					pdly->idelayoutputxf = pdly->idelayinput - delaysamples;

					if( pdly->idelayoutputxf < 0 )
						pdly->idelayoutputxf += pdly->cdelaysamplesmax;

					pdly->xfade = RVB_XFADE;
				}

				// deactivate line if rounded down to 0 delay
				if( pdly->delaysamples == 0 ) 
					SXDLY_Free( i );
			}
		}
	}

	rgsxdly[ISXRVB].delayfeed = (sxrvb_feedback->value) * 255;
	rgsxdly[ISXRVB].lp = sxrvb_lp->value;

	rgsxdly[ISXRVB + 1].delayfeed = (sxrvb_feedback->value) * 255;
	rgsxdly[ISXRVB + 1].lp = sxrvb_lp->value;

}

// main routine for updating the paintbuffer with new reverb values.
// This routine updates both left and right lines with 
// the mono reverb signal.  Delay is set through console vars room_reverb
// and room_feedback.  2 reverbs operating in parallel.
void SXRVB_DoReverbMono( int count ) 
{
	portable_samplepair_t	*pbuf;
	int			val;
	int			valt;
	int			mono;
	sample_t			sampledly;
	sample_t			samplexf;
	int			countr;
	int			voutm;
	int			gain;

	// process reverb lines if active
	if( rgsxdly[ISXRVB].lpdelayline ) 
	{
		gain = DSP_CONSTANT_GAIN;

		pbuf = paintbuffer;
		countr = count;		

		// process each sample in the paintbuffer...

		while( countr-- ) 
		{
			mono = pbuf->left;
			voutm = 0;
	
			if( --gdly1.modcur < 0 )
				gdly1.modcur = gdly1.mod;

			// ========================== ISXRVB============================	

			// get sample from delay line

			sampledly = *(gdly1.lpdelayline + gdly1.idelayoutput);

			// only process if something is non-zero
			if( gdly1.xfade || sampledly || mono ) 
			{
				// modulate delay rate
				// UNDONE: modulation disabled
				if( 0 && !gdly1.xfade && !gdly1.modcur && gdly1.mod )
				{
					// set up crossfade to new delay value, if we're not already doing an xfade
					gdly1.idelayoutputxf = gdly1.idelayoutput + ((Com_RandomLong(0, 0xFF) * gdly1.delaysamples) >> 9); // 100 = ~ 9ms

					if( gdly1.idelayoutputxf >= gdly1.cdelaysamplesmax )
						gdly1.idelayoutputxf -= gdly1.cdelaysamplesmax;

					gdly1.xfade = RVB_XFADE;
				}
				
				// modify sampledly if crossfading to new delay value

				if( gdly1.xfade ) 
				{
					samplexf = (*(gdly1.lpdelayline + gdly1.idelayoutputxf) * (RVB_XFADE - gdly1.xfade)) / RVB_XFADE;
					sampledly = ((sampledly * gdly1.xfade) / RVB_XFADE) + samplexf;
					
					if( ++gdly1.idelayoutputxf >= gdly1.cdelaysamplesmax )
						gdly1.idelayoutputxf = 0;

					if( --gdly1.xfade == 0 ) 
						gdly1.idelayoutput = gdly1.idelayoutputxf;
				} 

				if( sampledly ) 
				{
					// get current sample from delay buffer
					// calculate delayed value
					val = SOUNDCLIP(mono + ((gdly1.delayfeed * sampledly) >> 8));	
				} 
				else 
				{
					val = mono;
				}

				// lowpass
				if( gdly1.lp ) 
				{
					valt = (gdly1.lp0 + gdly1.lp1 + (val<<1)) >> 2;
					gdly1.lp1 = gdly1.lp0;
					gdly1.lp0 = val;
				} 
				else 
				{
					valt = val;
				}

				// store delay output value into output buffer
				*(gdly1.lpdelayline + gdly1.idelayinput) = valt;
				voutm = valt;
			} 
			else 
			{
				// not playing samples, but still must flush lowpass buffer & delay line
					
				gdly1.lp0 = gdly1.lp1 = 0;
				*(gdly1.lpdelayline + gdly1.idelayinput) = 0;
				voutm = 0;
			}

			// update delay buffer pointers
			if( ++gdly1.idelayinput >= gdly1.cdelaysamplesmax )
				gdly1.idelayinput = 0;
			
			if( ++gdly1.idelayoutput >= gdly1.cdelaysamplesmax )
				gdly1.idelayoutput = 0;

			// ========================== ISXRVB + 1========================

			if( --gdly2.modcur < 0 )
				gdly2.modcur = gdly2.mod;
			
			if( gdly2.lpdelayline ) 
			{
				// get sample from delay line

				sampledly = *(gdly2.lpdelayline + gdly2.idelayoutput);

				// only process if something is non-zero
				if( gdly2.xfade || sampledly || mono ) 
				{
					// UNDONE: modulation disabled
					if( 0 && !gdly2.xfade && gdly2.modcur && gdly2.mod ) 
					{
						// set up crossfade to new delay value, if we're not already doing an xfade
						gdly2.idelayoutputxf = gdly2.idelayoutput + ((Com_RandomLong(0,0xFF) * gdly2.delaysamples) >> 9); // 100 = ~ 9ms

						if( gdly2.idelayoutputxf >= gdly2.cdelaysamplesmax )
						    	gdly2.idelayoutputxf -= gdly2.cdelaysamplesmax;

						gdly2.xfade = RVB_XFADE;
					}
					
					// modify sampledly if crossfading to new delay value
					if( gdly2.xfade ) 
					{
						samplexf = (*(gdly2.lpdelayline + gdly2.idelayoutputxf) * (RVB_XFADE - gdly2.xfade)) / RVB_XFADE;
						sampledly = ((sampledly * gdly2.xfade) / RVB_XFADE) + samplexf;
						
						if( ++gdly2.idelayoutputxf >= gdly2.cdelaysamplesmax )
							gdly2.idelayoutputxf = 0;

						if( --gdly2.xfade == 0 ) 
							gdly2.idelayoutput = gdly2.idelayoutputxf;
					} 
						
					if( sampledly ) 
					{
						// get current sample from delay buffer
						val = SOUNDCLIP(mono + ((gdly2.delayfeed * sampledly) >> 8));
					} 
					else 
					{
						val = mono;
					}

					// lowpass
					if( gdly2.lp ) 
					{
						valt = (gdly2.lp0 + gdly2.lp1 + (val<<1)) >> 2;
						gdly2.lp0 = val;
					} 
					else 
					{
						valt = val;
					}

					// store delay output value into output buffer
					*(gdly2.lpdelayline + gdly2.idelayinput) = valt;
					voutm += valt;
				} 
				else 
				{
					// not playing samples, but still must flush lowpass buffer
					gdly2.lp0 = gdly2.lp1 = 0;
					*(gdly2.lpdelayline + gdly2.idelayinput) = 0;
				}

				// update delay buffer pointers
				if( ++gdly2.idelayinput >= gdly2.cdelaysamplesmax )
					gdly2.idelayinput = 0;
				
				if( ++gdly2.idelayoutput >= gdly2.cdelaysamplesmax )
					gdly2.idelayoutput = 0;	
			}

			// ============================ Mix ================================

			// add mono delay to left and right channels
			// drop output by inverse of cascaded gain for both reverbs

			voutm = (gain * voutm) >> 8;

			pbuf->left = SOUNDCLIP( voutm );
			pbuf++;
		}
	}
}

void SXRVB_DoReverb( int count ) 
{
	int		val[2];
	int		valt[2];
	int		left;
	int		right;				
	sample_t		sampledly;
	sample_t		samplexf;
	portable_samplepair_t *pbuf;
	int		countr;
	int		voutm[2];
	int		gain;

	if( dma.channels < 2 )
	{
		SXRVB_DoReverbMono( count );
		return;
	}

	// process reverb lines if active
	if( rgsxdly[ISXRVB].lpdelayline ) 
	{
		gain = DSP_CONSTANT_GAIN;

		pbuf = paintbuffer;
		countr = count;		

		// process each sample in the paintbuffer...

		while( countr-- ) 
		{
			left = pbuf->left;
			right = pbuf->right;
			voutm[0] = 0;
			voutm[1] = 0;

			if( --gdly1.modcur < 0 )
				gdly1.modcur = gdly1.mod;

			// ========================== ISXRVB============================	

			// get sample from delay line

			sampledly = *(gdly1.lpdelayline + gdly1.idelayoutput);

			// only process if something is non-zero
			if( gdly1.xfade || sampledly || left || right ) 
			{
				// modulate delay rate
				// UNDONE: modulation disabled
				if( 0 && !gdly1.xfade && !gdly1.modcur && gdly1.mod )
				{
					// set up crossfade to new delay value, if we're not already doing an xfade
					gdly1.idelayoutputxf = gdly1.idelayoutput + ((Com_RandomLong(0, 0xFF) * gdly1.delaysamples) >> 9); // 100 = ~ 9ms

					if( gdly1.idelayoutputxf >= gdly1.cdelaysamplesmax )
						gdly1.idelayoutputxf -= gdly1.cdelaysamplesmax;

					gdly1.xfade = RVB_XFADE;
				}
				
				// modify sampledly if crossfading to new delay value

				if( gdly1.xfade ) 
				{
					samplexf = (*(gdly1.lpdelayline + gdly1.idelayoutputxf) * (RVB_XFADE - gdly1.xfade)) / RVB_XFADE;
					sampledly = ((sampledly * gdly1.xfade) / RVB_XFADE) + samplexf;
					
					if( ++gdly1.idelayoutputxf >= gdly1.cdelaysamplesmax )
						gdly1.idelayoutputxf = 0;

					if( --gdly1.xfade == 0 ) 
						gdly1.idelayoutput = gdly1.idelayoutputxf;
				} 

				if( sampledly ) 
				{
					// get current sample from delay buffer
					// calculate delayed value
					val[0] = SOUNDCLIP(left + ((gdly1.delayfeed * sampledly) >> 8));	
					val[1] = SOUNDCLIP(right + ((gdly1.delayfeed * sampledly) >> 8));
					
				} 
				else 
				{
					val[0] = left;
					val[1] = right;
				}

				// lowpass
				if( gdly1.lp ) 
				{
					valt[0] = (gdly1.lp0 + gdly1.lp1 + (val[0]<<1)) >> 2;
					valt[1] = (gdly1.lp0 + gdly1.lp1 + (val[1]<<1)) >> 2;

					gdly1.lp1 = gdly1.lp0;
					gdly1.lp0 = (val[0] + val[1]) >> 1;
				} 
				else 
				{
					valt[0] = val[0];
					valt[1] = val[1];
				}

				// store delay output value into output buffer
				*(gdly1.lpdelayline + gdly1.idelayinput) = (valt[0] + valt[1]) >> 1;
				voutm[0] = valt[0];
				voutm[1] = valt[1];
			} 
			else 
			{
				// not playing samples, but still must flush lowpass buffer & delay line
					
				gdly1.lp0 = gdly1.lp1 = 0;
				*(gdly1.lpdelayline + gdly1.idelayinput) = 0;
				voutm[0] = 0;
				voutm[1] = 0;
			}

			// update delay buffer pointers
			if( ++gdly1.idelayinput >= gdly1.cdelaysamplesmax )
				gdly1.idelayinput = 0;
			
			if( ++gdly1.idelayoutput >= gdly1.cdelaysamplesmax )
				gdly1.idelayoutput = 0;

			// ========================== ISXRVB + 1========================

			if( --gdly2.modcur < 0 )
				gdly2.modcur = gdly2.mod;
			
			if( gdly2.lpdelayline ) 
			{
				// get sample from delay line

				sampledly = *(gdly2.lpdelayline + gdly2.idelayoutput);

				// only process if something is non-zero
				if( gdly2.xfade || sampledly || left || right ) 
				{
					// UNDONE: modulation disabled
					if( 0 && !gdly2.xfade && gdly2.modcur && gdly2.mod ) 
					{
						// set up crossfade to new delay value, if we're not already doing an xfade
						gdly2.idelayoutputxf = gdly2.idelayoutput + ((Com_RandomLong(0,0xFF) * gdly2.delaysamples) >> 9); // 100 = ~ 9ms

						if( gdly2.idelayoutputxf >= gdly2.cdelaysamplesmax )
						    	gdly2.idelayoutputxf -= gdly2.cdelaysamplesmax;

						gdly2.xfade = RVB_XFADE;
					}
					
					// modify sampledly if crossfading to new delay value
					if( gdly2.xfade ) 
					{
						samplexf = (*(gdly2.lpdelayline + gdly2.idelayoutputxf) * (RVB_XFADE - gdly2.xfade)) / RVB_XFADE;
						sampledly = ((sampledly * gdly2.xfade) / RVB_XFADE) + samplexf;
						
						if( ++gdly2.idelayoutputxf >= gdly2.cdelaysamplesmax )
							gdly2.idelayoutputxf = 0;

						if( --gdly2.xfade == 0 ) 
							gdly2.idelayoutput = gdly2.idelayoutputxf;
					} 
						
					if( sampledly ) 
					{
						// get current sample from delay buffer
						val[0] = SOUNDCLIP(left + ((gdly2.delayfeed * sampledly) >> 8));
						val[1] = SOUNDCLIP(right + ((gdly2.delayfeed * sampledly) >> 8));
					} 
					else 
					{
						val[0] = left;
						val[1] = right;
					}

					// lowpass
					if( gdly2.lp ) 
					{
						valt[0] = (gdly2.lp0 + gdly2.lp1 + (val[0]<<1)) >> 2;
						valt[1] = (gdly2.lp0 + gdly2.lp1 + (val[1]<<1)) >> 2;
						gdly2.lp0 = (val[0] + val[1]) >> 1;
					} 
					else 
					{
						valt[0] = val[0];
						valt[1] = val[1];
					}

					// store delay output value into output buffer
					*(gdly2.lpdelayline + gdly2.idelayinput) = (valt[0] + valt[1]) >> 1;
					voutm[0] += valt[0];
					voutm[1] += valt[1];
				} 
				else 
				{
					// not playing samples, but still must flush lowpass buffer
					gdly2.lp0 = gdly2.lp1 = 0;
					*(gdly2.lpdelayline + gdly2.idelayinput) = 0;
				}

				// update delay buffer pointers
				if( ++gdly2.idelayinput >= gdly2.cdelaysamplesmax )
					gdly2.idelayinput = 0;
				
				if( ++gdly2.idelayoutput >= gdly2.cdelaysamplesmax )
					gdly2.idelayoutput = 0;	
			}

			// ============================ Mix================================

			// add mono delay to left and right channels
			// drop output by inverse of cascaded gain for both reverbs

			voutm[0] = (gain * voutm[0]) >> 8;
			voutm[1] = (gain * voutm[1]) >> 8;

			pbuf->left = SOUNDCLIP( voutm[0] );
			pbuf->right = SOUNDCLIP( voutm[1] );

			pbuf++;
		}
	}
}

// amplitude modulator, low pass filter for underwater weirdness
void SXRVB_DoAMod( int count ) 
{
	int			sample[2];
	int			valtsample[2];
	portable_samplepair_t	*pbuf;
	int			countr;
	int			fLowpass;
	int			fmod;

	// process reverb lines if active
	if( sxmod_lowpass->integer > 0 || sxmod_mod->integer > 0 )
	{
		pbuf = paintbuffer;
		countr = count;		
		
		fLowpass = (sxmod_lowpass->integer > 0);
		fmod = (sxmod_mod->integer > 0);

		// process each sample in the paintbuffer...
		while( countr-- )
		{
			if( dma.channels == 2 )
			{
				sample[0] = pbuf->left;
				sample[1] = pbuf->right;
			}		
			else
			{
				sample[0] = pbuf->left;
			}

			// only process if non-zero
			if( fLowpass )
			{
				valtsample[0] = sample[0];
				sample[0] = (rgsxlp[0] + rgsxlp[1] + rgsxlp[2] + rgsxlp[3] + rgsxlp[4] + sample[0]);
				sample[0] = ((sample[0] << 1) + (sample[0] << 3)) >> 6;

				rgsxlp[0] = rgsxlp[1];
				rgsxlp[1] = rgsxlp[2];
				rgsxlp[2] = rgsxlp[3];
				rgsxlp[3] = rgsxlp[4];
				rgsxlp[4] = valtsample[0];

				if (dma.channels > 1)
				{
					valtsample[1] = sample[1];
					sample[1] = (rgsxlp[5] + rgsxlp[6] + rgsxlp[7] + rgsxlp[8] + rgsxlp[9] + sample[1]);
					sample[1] = ((sample[1] << 1) + (sample[1] << 3)) >> 6;

					rgsxlp[5] = rgsxlp[6];
					rgsxlp[6] = rgsxlp[7];
					rgsxlp[7] = rgsxlp[8];
					rgsxlp[8] = rgsxlp[9];
					rgsxlp[9] = valtsample[1];
				}

			}
				
			if( fmod )
			{
				if( --sxmod1cur < 0 )
					sxmod1cur = sxmod1;

				if( !sxmod1cur )
					sxamodlt = Com_RandomLong(32,255);
				
				if( --sxmod2cur < 0 )
					sxmod2cur = sxmod2;

				if( !sxmod2cur )
					sxamodrt = Com_RandomLong(32,255);
				
				if( dma.channels == 2 )
				{
					sample[0] = (sample[0] * sxamodl) >> 8;
					sample[1] = (sample[1] * sxamodr) >> 8;
				}
				else
				{
					sample[0] = (sample[0] * (sxamodl+sxamodr)) >> 9;
				}

				if( sxamodl < sxamodlt ) 
					sxamodl++;
				else if( sxamodl > sxamodlt )
					sxamodl--;

				if( sxamodr < sxamodrt ) 
					sxamodr++;
				else if( sxamodr > sxamodrt )
					sxamodr--;
			}

			if( dma.channels == 2 )
			{
				pbuf->left = SOUNDCLIP( sample[0] );
				pbuf->right = SOUNDCLIP( sample[1] );
			}
			else
			{
				pbuf->left = SOUNDCLIP( sample[0] );
			}
			pbuf++;			
		}
	}
}

typedef struct sx_preset_s 
{
	float	room_lp;		// for water fx, lowpass for entire room
	float	room_mod;		// stereo amplitude modulation for room
	float	room_size;	// reverb: initial reflection size
	float	room_refl;	// reverb: decay time
	float	room_rvblp;	// reverb: low pass filtering level
	float	room_delay;	// mono delay: delay time
	float	room_feedback;	// mono delay: decay time
	float	room_dlylp;	// mono delay: low pass filtering level
	float	room_left;	// left channel delay time
} sx_preset_t;

sx_preset_t rgsxpre[CSXROOM] = 
{
// SXROOM_OFF		0
//	lp	mod	size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.0,	0.0,	2.0,	0.0},

// SXROOM_GENERIC		1	// general, low reflective, diffuse room
//	lp	mod	size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.065,	0.1,	0.0,	0.01},

// SXROOM_METALIC_S		2	// highly reflective, parallel surfaces
// SXROOM_METALIC_M		3
// SXROOM_METALIC_L		4

//	lp	mod	size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.02,	0.75,	0.0,	0.01}, // 0.001
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.03,	0.78,	0.0,	0.02}, // 0.002
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.06,	0.77,	0.0,	0.03}, // 0.003

// SXROOM_TUNNEL_S		5	// resonant reflective, long surfaces
// SXROOM_TUNNEL_M		6
// SXROOM_TUNNEL_L		7
//	lp	mod	size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.05,	0.85,	1.0,	0.018,	0.7,	2.0,	0.01}, // 0.01
	{0.0,	0.0,	0.05,	0.88,	1.0,	0.020,	0.7,	2.0,	0.02}, // 0.02
	{0.0,	0.0,	0.05,	0.92,	1.0,	0.025,	0.7,	2.0,	0.04}, // 0.04

// SXROOM_CHAMBER_S		8	// diffuse, moderately reflective surfaces
// SXROOM_CHAMBER_M		9
// SXROOM_CHAMBER_L		10
//	lp	mod	size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.05,	0.84,	1.0,	0.0,	0.0,	2.0,	0.012}, // 0.003
	{0.0,	0.0,	0.05,	0.90,	1.0,	0.0,	0.0,	2.0,	0.008}, // 0.002
	{0.0,	0.0,	0.05,	0.95,	1.0,	0.0,	0.0,	2.0,	0.004}, // 0.001

// SXROOM_BRITE_S		11	// diffuse, highly reflective
// SXROOM_BRITE_M		12
// SXROOM_BRITE_L		13
//	lp	mod	size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.05,	0.7,	0.0,	0.0,	0.0,	2.0,	0.012}, // 0.003
	{0.0,	0.0,	0.055,	0.78,	0.0,	0.0,	0.0,	2.0,	0.008}, // 0.002
	{0.0,	0.0,	0.05,	0.86,	0.0,	0.0,	0.0,	2.0,	0.002}, // 0.001

// SXROOM_WATER1		14	// underwater fx
// SXROOM_WATER2		15
// SXROOM_WATER3		16
//	lp	mod	size	refl	rvblp	delay	feedbk	dlylp	left  
	{1.0,	0.0,	0.0,	0.0,	1.0,	0.0,	0.0,	2.0,	0.01},
	{1.0,	0.0,	0.0,	0.0,	1.0,	0.06,	0.85,	2.0,	0.02},
	{1.0,	0.0,	0.0,	0.0,	1.0,	0.2,	0.6,	2.0,	0.05},

// SXROOM_CONCRETE_S	17	// bare, reflective, parallel surfaces
// SXROOM_CONCRETE_M	18
// SXROOM_CONCRETE_L	19
//	lp	mod	size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.05,	0.8,	1.0,	0.0,	0.48,	2.0,	0.016}, // 0.15 delay, 0.008 left
	{0.0,	0.0,	0.06,	0.9,	1.0,	0.0,	0.52,	2.0,	0.01 }, // 0.22 delay, 0.005 left
	{0.0,	0.0,	0.07,	0.94,	1.0,	0.3,	0.6,	2.0,	0.008}, // 0.001

// SXROOM_OUTSIDE1		20	// echoing, moderately reflective
// SXROOM_OUTSIDE2		21	// echoing, dull
// SXROOM_OUTSIDE3		22	// echoing, very dull
//	lp	mod	size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.3,	0.42,	2.0,	0.0},
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.35,	0.48,	2.0,	0.0},
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.38,	0.6,	2.0,	0.0},

// SXROOM_CAVERN_S		23	// large, echoing area
// SXROOM_CAVERN_M		24
// SXROOM_CAVERN_L		25
//	lp	mod	size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.05,	0.9,	1.0,	0.2,	0.28,	0.0,	0.0},
	{0.0,	0.0,	0.07,	0.9,	1.0,	0.3,	0.4,	0.0,	0.0},
	{0.0,	0.0,	0.09,	0.9,	1.0,	0.35,	0.5,	0.0,	0.0},

// SXROOM_WEIRDO1		26		
// SXROOM_WEIRDO2		27
// SXROOM_WEIRDO3		28
//	lp	mod	size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	1.0,	0.01,	0.9,	0.0,	0.0,	0.0,	2.0,	0.05},
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.009,	0.999,	2.0,	0.04},
	{0.0,	0.0,	0.001,	0.999,	0.0,	0.2,	0.8,	2.0,	0.05}

};

// main routine for processing room sound fx
// if fFilter is true, then run in-line filter (for underwater fx)
// if fTimefx is true, then run reverb and delay fx
// NOTE: only processes preset room_types from 0-29 (CSXROOM)
void SX_RoomFX( int endtime, int fFilter, int fTimefx ) 
{
	int	i, fReset;
	int	sampleCount;
	int	roomType;

	// return right away if fx processing is turned off
	if( sxroom_off->value != 0.0f )
		return;

	sampleCount = endtime - paintedtime;
	if( sampleCount < 0 ) return;

	fReset = false;
	if( s_listener.waterlevel > 2 )
		roomType = sxroomwater_type->integer;
	else roomType = sxroom_type->integer;

	// only process legacy roomtypes here
	if( roomType >= CSXROOM ) return;

	if( roomType != sxroom_typeprev ) 
	{
		MsgDev( D_NOTE, "svc_roomtype: set to %i\n", roomType );

		sxroom_typeprev = roomType;

		i = roomType;
		if( i < CSXROOM && i >= 0 )
		{
			Cvar_SetValue( "room_lp", rgsxpre[i].room_lp );
			Cvar_SetValue( "room_mod", rgsxpre[i].room_mod );
			Cvar_SetValue( "room_size", rgsxpre[i].room_size );
			Cvar_SetValue( "room_refl", rgsxpre[i].room_refl );
			Cvar_SetValue( "room_rvblp", rgsxpre[i].room_rvblp );
			Cvar_SetValue( "room_delay", rgsxpre[i].room_delay );
			Cvar_SetValue( "room_feedback", rgsxpre[i].room_feedback );
			Cvar_SetValue( "room_dlylp", rgsxpre[i].room_dlylp );
			Cvar_SetValue( "room_left", rgsxpre[i].room_left );
		}

		SXRVB_CheckNewReverbVal();
		SXDLY_CheckNewDelayVal();
		SXDLY_CheckNewStereoDelayVal();

		fReset = true;
	}

	if( fReset || roomType != 0 ) 
	{
		// debug code
		SXRVB_CheckNewReverbVal();
		SXDLY_CheckNewDelayVal();
		SXDLY_CheckNewStereoDelayVal();	
		// debug code

		if( fFilter ) SXRVB_DoAMod( sampleCount );

		if( fTimefx )
		{
			SXRVB_DoReverb( sampleCount );
			SXDLY_DoDelay( sampleCount );
			SXDLY_DoStereoDelay( sampleCount );
		}
	} 
}