//=======================================================================
//			Copyright (C) Shambler Team 2004
//		         warhead.cpp - hud for sniper rifle
//=======================================================================

#include "extdll.h"
#include "triangle_api.h"
#include "hud_iface.h"
#include "hud.h"

void DrawQuad(float xmin, float ymin, float xmax, float ymax)
{
	//top left
	g_engfuncs.pTriAPI->TexCoord2f(0,0);
	g_engfuncs.pTriAPI->Vertex3f(xmin, ymin, 0); 
	//bottom left
	g_engfuncs.pTriAPI->TexCoord2f(0,1);
	g_engfuncs.pTriAPI->Vertex3f(xmin, ymax, 0);
	//bottom right
	g_engfuncs.pTriAPI->TexCoord2f(1,1);
	g_engfuncs.pTriAPI->Vertex3f(xmax, ymax, 0);
	//top right
	g_engfuncs.pTriAPI->TexCoord2f(1,0);
	g_engfuncs.pTriAPI->Vertex3f(xmax, ymin, 0);
}

DECLARE_MESSAGE( m_Zoom, ZoomHUD )

int CHudZoom :: Init( void )
{
	m_iHudMode = 0;
	HOOK_MESSAGE( ZoomHUD );

	m_iFlags |= HUD_ACTIVE;

	gHUD.AddHudElem( this );
	return 1;
}

int CHudZoom::VidInit( void )
{
	m_hLines = SPR_Load( "sprites/snlines.spr" );
	m_hCrosshair = SPR_Load( "sprites/snzoom.spr" );
	m_iHudMode = 0;
	return 1;
}

int CHudZoom :: MsgFunc_ZoomHUD( const char *pszName,  int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );
	m_iHudMode = READ_BYTE();
	END_READ();

	return 1;
}

int CHudZoom :: Draw( float flTime )
{
	if( !m_hLines || !m_hCrosshair ) return 0;
	if( !m_iHudMode ) return 0; // draw scope

	float left = (float)(ScreenWidth - ScreenHeight) / 2;
	float right = left + ScreenHeight;

	// draw crosshair          
	SPR_Set( m_hCrosshair, 255, 255, 255 );
	SPR_DrawTransColor( 0, left, 0, ScreenHeight, ScreenHeight );

	// draw side-lines
	SPR_Set( m_hLines, 255, 255, 255 );
	SPR_Draw( 0, 0, 0, left, ScreenHeight );
	SPR_Draw( 0, right, 0, left, ScreenHeight );

	return 1;
}
