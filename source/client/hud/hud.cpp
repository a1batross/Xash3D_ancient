//=======================================================================
//			Copyright (C) XashXT Group 2007
//=======================================================================

#include "extdll.h"
#include "utils.h"
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
		
	MsgFunc_ResetHUD( 0, 0, NULL );
	CVAR_REGISTER( "cl_crosshair", "1", FCVAR_ARCHIVE, "show weapon chrosshair" );
}

CHud :: ~CHud( void )
{
	delete [] m_rghSprites;
	delete [] m_rgrcRects;
	delete [] m_rgszSpriteNames;

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
	// look through the loaded sprite name list for SpriteName
	for( int i = 0; i < m_iSpriteCount; i++ )
	{
		if(!strncmp( SpriteName, m_rgszSpriteNames + (i * MAX_SPRITE_NAME_LENGTH), MAX_SPRITE_NAME_LENGTH ))
			return i;
	}
	return -1; // invalid sprite
}

void CHud :: VidInit( void )
{
	// ----------
	// Load Sprites
	// ---------

	m_hsprCursor = 0;
	m_hHudError = 0;
	m_hHudFont = 0;
	spot = NULL; // clear intermission spot

	Draw_VidInit ();
	ClearAllFades ();

	// setup screen info
	m_scrinfo.iRealWidth = CVAR_GET_FLOAT( "width" );
	m_scrinfo.iRealHeight = CVAR_GET_FLOAT( "height" );

	if( CVAR_GET_FLOAT( "hud_scale" ))
	{
		// virtual screen space 640x480
		// see cl_screen.c from Quake3 code for more details
		m_scrinfo.iWidth = 640;
		m_scrinfo.iHeight = 480;
	}
	else
	{
		m_scrinfo.iWidth = CVAR_GET_FLOAT( "width" );
		m_scrinfo.iHeight = CVAR_GET_FLOAT( "height" );
	}

	// TODO: build real table of fonts widthInChars
	for( int i = 0; i < 256; i++ )
		m_scrinfo.charWidths[i] = SMALLCHAR_WIDTH;
	m_scrinfo.iCharHeight = SMALLCHAR_HEIGHT;

	// Only load this once
	if ( !m_pSpriteList )
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList( "scripts/hud.txt", &m_iSpriteCount );

		if( m_pSpriteList )
		{
			// allocated memory for sprite handle arrays
 			m_rghSprites = new HSPRITE[m_iSpriteCount];
			m_rgrcRects = new wrect_t[m_iSpriteCount];
			m_rgszSpriteNames = new char[m_iSpriteCount * MAX_SPRITE_NAME_LENGTH];

			client_sprite_t *p = m_pSpriteList;
			for ( int j = 0; j < m_iSpriteCount; j++ )
			{
				m_rghSprites[j] = SPR_Load( p->szSprite );
				m_rgrcRects[j] = p->rc;
				strncpy( &m_rgszSpriteNames[j * MAX_SPRITE_NAME_LENGTH], p->szName, MAX_SPRITE_NAME_LENGTH );
				p++;
			}
		}
		else
		{
			ALERT( at_warning, "hud.txt couldn't load\n" );
			CVAR_SET_FLOAT( "hud_draw", 0 );
			return;
		}
	}
	else
	{
		// engine may be release unused shaders after reloading map or change level
		// loading them again here
		client_sprite_t *p = m_pSpriteList;
		for( int j = 0; j < m_iSpriteCount; j++ )
		{
			m_rghSprites[j] = SPR_Load( p->szSprite );
			p++;
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex( "number_0" );
	m_iFontHeight = GetSpriteRect( m_HUD_number_0 ).bottom - GetSpriteRect( m_HUD_number_0 ).top;

	// loading error sprite
	m_HUD_error = GetSpriteIndex( "error" );
	m_hHudError = GetSprite( m_HUD_error );
	m_hHudFont = GetSprite( GetSpriteIndex( "hud_font" ));

	// loading TE shaders
	m_hDefaultParticle = TEX_Load( "textures/particles/default" );
	m_hGlowParticle = TEX_Load( "textures/particles/glow" );
	m_hDroplet = TEX_Load( "textures/particles/droplet" );
	m_hBubble = TEX_Load( "textures/particles/bubble" );
	m_hSparks = TEX_Load( "textures/particles/spark" );
	m_hSmoke = TEX_Load( "textures/particles/smoke" );
	
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
	float def_fov = CVAR_GET_FLOAT( "default_fov" );
	if( m_flFOV == 0.0f ) m_flFOV = max( CVAR_GET_FLOAT( "default_fov" ), 90 );
	
	// change sensitivity
	if( m_flFOV == def_fov )
	{
		m_flMouseSensitivity = 0;
	}
	else
	{
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = CVAR_GET_FLOAT( "sensitivity" ) * ( m_flFOV / def_fov );
		m_flMouseSensitivity *= CVAR_GET_FLOAT( "zoom_sensitivity_ratio" ); // apply zoom factor
	}
}

int CHud :: UpdateClientData( client_data_t *cdata, float time )
{
	memcpy( m_vecOrigin, cdata->origin, sizeof( vec3_t ));
	memcpy( m_vecAngles, cdata->angles, sizeof( vec3_t ));

	m_iKeyBits = cdata->iKeyBits;
	m_iWeaponBits = cdata->iWeaponBits;

	Think();

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
	static float m_flShotTime;

	// clock was reset, reset delta
	if( m_flTimeDelta < 0 ) m_flTimeDelta = 0;

	// make levelshot if needed
	MAKE_LEVELSHOT();
	m_iDrawPlaque = 1;	// clear plaque stuff

	// draw screen fade before hud
	DrawScreenFade();

	// take a screenshot if the client's got the cvar set
	if( CVAR_GET_FLOAT( "hud_takesshots" ))
	{
		if( m_flTime > m_flShotTime )
		{
			CLIENT_COMMAND( "screenshot\n" );
			m_flShotTime = m_flTime + 0.04f;
		}
	}

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
	if(( viewFlags & CAMERA_ON ) && !( viewFlags & DRAW_HUD ))
		return 1;
	
	if( CVAR_GET_FLOAT( "hud_draw" ))
	{
		HUDLIST *pList = m_pHudList;

		while( pList )
		{
			if( !m_iIntermission )
			{
				if(( pList->p->m_iFlags & HUD_ACTIVE ) && !(m_iHideHUDDisplay & HIDEHUD_ALL ))
					pList->p->Draw( flTime );
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
		int next = xpos + gHUD.m_scrinfo.charWidths[*szIt]; // variable-width fonts look cool
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
		int next = xpos - gHUD.m_scrinfo.charWidths[ *szIt ]; // variable-width fonts look cool
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
			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(m_HUD_number_0 + k));
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
			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(m_HUD_number_0 + k));
			x += iWidth;
		}
		else if( iFlags & (DHN_3DIGITS|DHN_2DIGITS))
		{
			x += iWidth;
		}

		// SPR_Draw ones
		k = iNumber % 10;
		SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
		SPR_DrawAdditive(0,  x, y, &GetSpriteRect(m_HUD_number_0 + k));
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
		SPR_DrawAdditive( 0,  x, y, &GetSpriteRect(m_HUD_number_0));
		x += iWidth;
	}
	return x;
}

int CHud::GetNumWidth( int iNumber, int iFlags )
{
	if( iFlags & (DHN_3DIGITS)) return 3;
	if( iFlags & (DHN_2DIGITS)) return 2;

	if( iNumber <= 0 )
	{
		if( iFlags & (DHN_DRAWZERO))
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