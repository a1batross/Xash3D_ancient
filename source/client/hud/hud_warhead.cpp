//=======================================================================
//			Copyright (C) Shambler Team 2004
//			  warhead.cpp - hud for weapon_redeemer
//			    
//=======================================================================

#include "extdll.h"
#include "hud_iface.h"
#include "hud.h"

#define GUIDE_S	SPR_Width( m_hCrosshair, 0 )
#define READOUT_S	128
DECLARE_MESSAGE( m_Redeemer, WarHUD )

int CHudRedeemer::Init( void )
{
	m_iHudMode = 0;

	HOOK_MESSAGE( WarHUD );
	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem( this );

	return 1;
}

int CHudRedeemer :: VidInit( void )
{
	m_hCrosshair = SPR_Load( "sprites/guidedx.spr" );
	m_hSprite = SPR_Load( "sprites/readout.spr" );
	m_hCamRec = SPR_Load( "sprites/cam_rec.spr" );
	m_hStatic = SPR_Load( "sprites/static.spr" );
	m_hCamera = SPR_Load( "sprites/camera.spr" );

	m_iHudMode = 0;
	return 1;
}

int CHudRedeemer :: MsgFunc_WarHUD( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );
	m_iHudMode = READ_BYTE();
	END_READ();

	return 1;
}

int CHudRedeemer :: Draw( float flTime )
{
	int x, y, w, h;
	wrect_t rc;
	int frame;

	// setup screen rectangle
	rc.left = rc.top = 0;
	rc.right = ScreenWidth;
	rc.bottom = ScreenHeight;

	if( m_iHudMode == 1 )		// draw crosshair and readout
	{
		y = (ScreenWidth - GUIDE_S) / 2;
		x = (ScreenHeight - GUIDE_S) / 2;

		SPR_Set( m_hCrosshair, 255, 128, 128 );
		SPR_DrawAdditive( 0,  y, x, NULL);

		int yOffset = ((int)(flTime * 850) % READOUT_S) - READOUT_S;
		SPR_Set( m_hSprite, 255, 128, 128 );
		for( ; yOffset < ScreenHeight; yOffset += READOUT_S )
			SPR_DrawAdditive( 0, 0, yOffset, NULL );
		SetScreenFade( Vector( 1, 0, 0 ), 0.25, 0, 0, FFADE_STAYOUT ); // enable red fade
	}
	else if( m_iHudMode == 2 )		// draw alpha noise
	{
		SPR_Set(m_hStatic, 255, 255, 255 );

		// play at 15fps
		frame = (int)(flTime * 15) % SPR_Frames( m_hStatic );

		y = x = 0;
		w = SPR_Width( m_hStatic, 0);
		h = SPR_Height( m_hStatic, 0);

		for( y = -(rand() % h); y < ScreenHeight; y += h ) 
			for( x = -(rand() % w); x < ScreenWidth; x += w ) 
				SPR_DrawAdditive ( frame, x, y, NULL );

		y = (ScreenWidth - GUIDE_S) / 2;
		x = (ScreenHeight - GUIDE_S) / 2;

		SPR_Set( m_hCrosshair, 255, 128, 128 );
		SPR_DrawAdditive( 0,  y, x, NULL );

		int yOffset = ((int)(flTime * 850) % READOUT_S) - READOUT_S;
		SPR_Set(m_hSprite, 255, 128, 128 );
		for( ; yOffset < ScreenHeight; yOffset += READOUT_S )
			SPR_DrawAdditive( 0, 0, yOffset, NULL );
		SetScreenFade( Vector( 1, 0, 0 ), 0.25, 0, 0, FFADE_STAYOUT ); // enable red fade
	}
	else if( m_iHudMode == 3 )		// draw static noise
	{         
		// play at 25fps
		frame = (int)(flTime * 25) % SPR_Frames( m_hStatic );

		SPR_Set( m_hStatic, 255, 255, 255 );
		SPR_Draw( frame, 0, 0, &rc );

		// disable fade
		SetScreenFade( Vector( 1, 1, 1 ), 0, 0, 0, FFADE_OUT );
	}
	else if( m_iHudMode == 4 )		// draw videocamera screen
	{         
		// play at 15fps
		frame = (int)(flTime * 1.5f) % SPR_Frames( m_hCamRec );
                    
		// draw interlaces
		SPR_Set( m_hCamera, 16, 96, 16 ); //, 64 ); // give this value from breaklight.bsp (xash03)
		SPR_DrawAdditive( frame, 0, 0, &rc );

		// draw recorder icon
		SPR_Set( m_hCamRec, 255, 0, 0 ); //, 255 ); // give this value from breaklight.bsp (xash03)

		float scale = 2.5f; // REC[*] sprite scale
		wrect_t m_rcCamRec;
		
		// calculating pos with different resolutions
		w = SPR_Width( m_hCamRec, 0 ) * scale; 
		h = SPR_Height( m_hCamRec, 0 ) * scale;
		x = ScreenWidth - (w * 1.5f);
		y = w * 0.4f;
		m_rcCamRec.left = x;
		m_rcCamRec.right = x + w; 
		m_rcCamRec.top = y;
		m_rcCamRec.bottom = y + h;
		SPR_DrawAdditive( frame, 0, 0, &m_rcCamRec );
				
		// disable fade
		SetScreenFade( Vector( 1, 1, 1 ), 0, 0, 0, FFADE_OUT );
	}
	return 1;
}