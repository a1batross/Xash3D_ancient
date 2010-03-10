//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     triapi.cpp - triangle rendering, if any
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "r_particle.h"
#include "r_weather.h"
#include "r_beams.h"

void HUD_DrawTriangles( void )
{
}

void HUD_DrawTransparentTriangles( void )
{
	R_DrawWeather();

	for( int i = 0; i < 3; i++ )
	{
		// NOTE: for some reasons in Xash3D
		// we needs for three iterations of PartSystem
		g_pParticleSystems->UpdateSystems();
	}
}

void HUD_RenderCallback( int fTrans )
{
	if( !fTrans ) HUD_DrawTriangles ();
	else HUD_DrawTransparentTriangles();

	g_pViewRenderBeams->UpdateBeams( fTrans );
}