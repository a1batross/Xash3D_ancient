//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     triapi.cpp - triangle rendering, if any
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "aurora.h"
#include "r_particle.h"
#include "r_weather.h"
#include "r_beams.h"

void HUD_DrawTriangles( void )
{
}

void HUD_DrawTransparentTriangles( void )
{
	R_DrawWeather();

	// Aurora particles
	g_pParticleSystems->UpdateSystems();

	// classic particles
	g_pParticles->Update();
}

void HUD_RenderCallback( int fTrans )
{
	if( !fTrans ) HUD_DrawTriangles ();
	else HUD_DrawTransparentTriangles();

	g_pViewRenderBeams->UpdateBeams( fTrans );
}