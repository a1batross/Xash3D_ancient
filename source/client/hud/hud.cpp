//=======================================================================
//			Copyright (C) XashXT Group 2007
//=======================================================================

#include "extdll.h"
#include "hud_iface.h"
#include "hud.h"

void CHud :: Init( void )
{
	InitMessages();
	m_Ammo.Init();
	m_Health.Init();
	m_SayText.Init();
	m_Geiger.Init();
	m_Train.Init();
	m_Battery.Init();
	m_Flash.Init();
	m_Redeemer.Init();
	m_Zoom.Init();
	m_Message.Init();
	m_Scoreboard.Init();
	m_StatusBar.Init();
	m_DeathNotice.Init();
	m_AmmoSecondary.Init();
	m_TextMessage.Init();
	m_StatusIcons.Init();
	m_Menu.Init();
	m_Sound.Init();
	m_MOTD.Init();
		
	MsgFunc_ResetHUD(0, 0, NULL );
}

CHud :: ~CHud( void )
{
	m_Sound.Close();

	if( m_pHudList )
	{
		HUDLIST *pList;
		while( m_pHudList )
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			FREE( pList );
		}
		m_pHudList = NULL;
	}
}

int CHud :: GetSpriteIndex( const char *SpriteName )
{
	// use built-in Shader Manager
	return LOAD_SHADER( SpriteName );
}

void CHud :: VidInit( void )
{
	// ----------
	// Load Sprites
	// ---------

	m_hsprCursor = 0;
	m_hHudError = 0;

	// TODO: build real table of fonts widthInChars
	for( int i = 0; i < 256; i++ )
		charWidths[i] = SMALLCHAR_WIDTH;
	iCharHeight = SMALLCHAR_HEIGHT;

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex( "number_0" );
	GetImageSize( NULL, &m_iFontHeight, m_HUD_number_0 );

	// loading error sprite
	m_HUD_error = GetSpriteIndex( "error" );
	m_hHudError = GetSprite( m_HUD_error );
	
	m_Sound.VidInit();
	m_Ammo.VidInit();
	m_Health.VidInit();
	m_Geiger.VidInit();
	m_Train.VidInit();
	m_Battery.VidInit();
	m_Flash.VidInit();
	m_Redeemer.VidInit();
	m_Zoom.VidInit();
	m_MOTD.VidInit();
	m_Message.VidInit();
	m_Scoreboard.VidInit();
	m_StatusBar.VidInit();
	m_DeathNotice.VidInit();
	m_SayText.VidInit();
	m_Menu.VidInit();
	m_AmmoSecondary.VidInit();
	m_TextMessage.VidInit();
	m_StatusIcons.VidInit();
}

void CHud :: Think( void )
{
	HUDLIST *pList = m_pHudList;

	while( pList )
	{
		if (pList->p->m_iFlags & HUD_ACTIVE)
			pList->p->Think();
		pList = pList->pNext;
	}

	// think about default fov
	if( m_iFOV == 0 )
	{
		// only let players adjust up in fov, and only if they are not overriden by something else
		m_iFOV = max( CVAR_GET_FLOAT( "default_fov" ), 90 );  
	}
}

int CHud :: UpdateClientData( ref_params_t *cdata, float time )
{
	memcpy( m_vecOrigin, cdata->origin, sizeof( vec3_t ));
	memcpy( m_vecAngles, cdata->angles, sizeof( vec3_t ));
	
	m_iKeyBits = cdata->iKeyBits;
	m_iWeaponBits = cdata->iWeaponBits;

	Think();

	cdata->fov_x = m_iFOV;
	cdata->iKeyBits = m_iKeyBits;
	cdata->v_idlescale = m_iConcussionEffect;

	if( m_flMouseSensitivity )
		cdata->mouse_sensitivity = m_flMouseSensitivity;

	return 1;
}

int CHud :: Redraw( float flTime )
{
	m_fOldTime = m_flTime;	// save time of previous redraw
	m_flTime = flTime;
	m_flTimeDelta = (double)m_flTime - m_fOldTime;

	// clock was reset, reset delta
	if( m_flTimeDelta < 0 ) m_flTimeDelta = 0;

	// make levelshot if needed
	MAKE_LEVELSHOT();

	// draw screen fade before hud
	DrawScreenFade();

	// redeemer hud stuff
	if( m_Redeemer.m_iHudMode > 0 )
	{
		m_Redeemer.Draw( flTime );
		return 1;
	}

	// zoom hud stuff
	if( m_Zoom.m_iHudMode > 0 )
	{
		m_Zoom.Draw( flTime );
		return 1;
	}

	// custom view active, and flag "draw hud" isn't set
	if(( viewFlags & 1 ) && !( viewFlags & 2 ))
		return 1;
	
	if( CVAR_GET_FLOAT( "hud_draw" ))
	{
		HUDLIST *pList = m_pHudList;

		while( pList )
		{
			if( !m_iIntermission )
			{
				if(( pList->p->m_iFlags & HUD_ACTIVE ) && !(m_iHideHUDDisplay & HIDEHUD_ALL ))
					pList->p->Draw(flTime);
			}
			else
			{
				// it's an intermission, so only draw hud elements
				// that are set to draw during intermissions
				if( pList->p->m_iFlags & HUD_INTERMISSION )
					pList->p->Draw( flTime );
			}
			pList = pList->pNext;
		}
	}
	return 1;
}

int CHud :: DrawHudString( int xpos, int ypos, int iMaxX, char *szIt, int r, int g, int b )
{
	// draw the string until we hit the null character or a newline character
	for( ; *szIt != 0 && *szIt != '\n'; szIt++ )
	{
		int next = xpos + gHUD.charWidths[*szIt]; // variable-width fonts look cool
		if ( next > iMaxX )
			return xpos;

		TextMessageDrawChar( xpos, ypos, *szIt, r, g, b );
		xpos = next;		
	}

	return xpos;
}

int CHud :: DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b )
{
	char szString[32];
	sprintf( szString, "%d", iNumber );
	return DrawHudStringReverse( xpos, ypos, iMinX, szString, r, g, b );

}

int CHud :: DrawHudStringReverse( int xpos, int ypos, int iMinX, char *szString, int r, int g, int b )
{
	// find the end of the string
	for ( char *szIt = szString; *szIt != 0; szIt++ )
	{ // we should count the length?		
	}

	// iterate throug the string in reverse
	for( szIt--;  szIt != (szString-1);  szIt-- )	
	{
		int next = xpos - gHUD.charWidths[ *szIt ]; // variable-width fonts look cool
		if( next < iMinX )
			return xpos;
		xpos = next;

		TextMessageDrawChar( xpos, ypos, *szIt, r, g, b );
	}

	return xpos;
}

int CHud :: DrawHudNumber( int x, int y, int iFlags, int iNumber, int r, int g, int b )
{
	int iWidth = GetSpriteRect( m_HUD_number_0 ).right - GetSpriteRect( m_HUD_number_0 ).left;
	int k;
	
	if( iNumber > 0 )
	{
		// SPR_Draw 100's
		if( iNumber >= 100 )
		{
			k = iNumber / 100;
			SPR_Set(GetSprite(LOAD_SHADER( va( "number_%i", k ))), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(LOAD_SHADER( va( "number_%i", k ))));
			x += iWidth;
		}
		else if( iFlags & DHN_3DIGITS )
		{
			x += iWidth;
		}

		// SPR_Draw 10's
		if( iNumber >= 10 )
		{
			k = (iNumber % 100)/10;
			SPR_Set(GetSprite(LOAD_SHADER( va( "number_%i", k ))), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(LOAD_SHADER( va( "number_%i", k ))));
			x += iWidth;
		}
		else if( iFlags & (DHN_3DIGITS|DHN_2DIGITS))
		{
			x += iWidth;
		}

		// SPR_Draw ones
		k = iNumber % 10;
		SPR_Set(GetSprite(LOAD_SHADER( va( "number_%i", k ))), r, g, b );
		SPR_DrawAdditive(0,  x, y, &GetSpriteRect(LOAD_SHADER( va( "number_%i", k ))));
		x += iWidth;
	} 
	else if( iFlags & DHN_DRAWZERO ) 
	{
		SPR_Set(GetSprite(m_HUD_number_0), r, g, b );

		// SPR_Draw 100's
		if( iFlags & DHN_3DIGITS )
		{
			x += iWidth;
		}

		if( iFlags & (DHN_3DIGITS|DHN_2DIGITS)) x += iWidth;
		SPR_DrawAdditive( 0,  x, y, &GetSpriteRect( m_HUD_number_0 ));
		x += iWidth;
	}
	return x;
}

int CHud::GetNumWidth( int iNumber, int iFlags )
{
	if( iFlags & DHN_3DIGITS ) return 3;
	if( iFlags & DHN_2DIGITS ) return 2;

	if( iNumber <= 0 )
	{
		if( iFlags & DHN_DRAWZERO )
			return 1;
		else return 0;
	}

	if( iNumber < 10  ) return 1;
	if( iNumber < 100 ) return 2;

	return 3;

}

void CHud::AddHudElem( CHudBase *phudelem )
{
	HUDLIST	*pdl, *ptemp;

	if( !phudelem ) return;

	pdl = (HUDLIST *)CALLOC( sizeof( HUDLIST ), 1 );
	pdl->p = phudelem;

	if( !m_pHudList )
	{
		m_pHudList = pdl;
		return;
	}

	ptemp = m_pHudList;

	while( ptemp->pNext )
		ptemp = ptemp->pNext;
	ptemp->pNext = pdl;
}