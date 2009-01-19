//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     view.cpp - view/refresh setup functions
//=======================================================================

#include "extdll.h"
#include "hud_iface.h"
#include "ref_params.h"
#include "hud.h"

void V_CalcShake( void )
{
	float	frametime;
	int	i;
	float	fraction, freq;

	if( gHUD.m_Shake.time == 0 )
		return;

	if(( gHUD.m_flTime > gHUD.m_Shake.time ) || gHUD.m_Shake.duration <= 0
	|| gHUD.m_Shake.amplitude <= 0 || gHUD.m_Shake.frequency <= 0 )
	{
		memset( &gHUD.m_Shake, 0, sizeof( gHUD.m_Shake ));
		return;
	}

	frametime = gHUD.m_flTimeDelta;

	if( gHUD.m_flTime > gHUD.m_Shake.nextShake )
	{
		// higher frequency means we recalc the extents more often and perturb the display again
		gHUD.m_Shake.nextShake = gHUD.m_flTime + (1.0f / gHUD.m_Shake.frequency);

		// Compute random shake extents (the shake will settle down from this)
		for( i = 0; i < 3; i++ )
		{
			gHUD.m_Shake.offset[i] = RANDOM_FLOAT( -gHUD.m_Shake.amplitude, gHUD.m_Shake.amplitude );
		}
		gHUD.m_Shake.angle = RANDOM_FLOAT( -gHUD.m_Shake.amplitude * 0.25, gHUD.m_Shake.amplitude * 0.25 );
	}

	// ramp down amplitude over duration (fraction goes from 1 to 0 linearly with slope 1/duration)
	fraction = ( gHUD.m_Shake.time - gHUD.m_flTime ) / gHUD.m_Shake.duration;

	// ramp up frequency over duration
	if( fraction )
	{
		freq = (gHUD.m_Shake.frequency / fraction);
	}
	else
	{
		freq = 0;
	}

	// square fraction to approach zero more quickly
	fraction *= fraction;

	// Sine wave that slowly settles to zero
	fraction = fraction * sin( gHUD.m_flTime * freq );
	
	// add to view origin
	gHUD.m_Shake.appliedOffset = gHUD.m_Shake.offset * fraction;
	
	// add to roll
	gHUD.m_Shake.appliedAngle = gHUD.m_Shake.angle * fraction;

	// drop amplitude a bit, less for higher frequency shakes
	float localAmp = gHUD.m_Shake.amplitude * ( frametime / ( gHUD.m_Shake.duration * gHUD.m_Shake.frequency ));
	gHUD.m_Shake.amplitude -= localAmp;
}

void V_ApplyShake( Vector& origin, Vector& angles, float factor )
{
	origin.MA( factor, origin, gHUD.m_Shake.appliedOffset );
	angles.z += gHUD.m_Shake.appliedAngle * factor;
}

void V_CalcRefdef( ref_params_t *parms )
{
}