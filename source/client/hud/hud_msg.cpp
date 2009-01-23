/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
//  hud_msg.cpp
//
#include "extdll.h"
#include "utils.h"
#include "hud.h"

// CHud message handlers
DECLARE_HUDMESSAGE( HUDColor );
DECLARE_HUDMESSAGE( SetFog );
DECLARE_HUDMESSAGE( RoomType );
DECLARE_HUDMESSAGE( SetSky );
DECLARE_HUDMESSAGE( RainData );
DECLARE_HUDMESSAGE( SetBody );
DECLARE_HUDMESSAGE( SetSkin );
DECLARE_HUDMESSAGE( ScreenFade );
DECLARE_HUDMESSAGE( WeaponAnim );
DECLARE_HUDMESSAGE( ResetHUD );
DECLARE_HUDMESSAGE( InitHUD );
DECLARE_HUDMESSAGE( ViewMode );
DECLARE_HUDMESSAGE( Particle );
DECLARE_HUDMESSAGE( Concuss );
DECLARE_HUDMESSAGE( GameMode );
DECLARE_HUDMESSAGE( CamData );
DECLARE_HUDMESSAGE( AddMirror );
DECLARE_HUDMESSAGE( AddScreen );
DECLARE_HUDMESSAGE( AddPortal );
DECLARE_HUDMESSAGE( TempEntity );
DECLARE_HUDMESSAGE( ServerName );
DECLARE_HUDMESSAGE( ScreenShake );
DECLARE_HUDMESSAGE( Intermission );

int CHud :: InitMessages( void )
{
	HOOK_MESSAGE( ResetHUD );
	HOOK_MESSAGE( GameMode );
	HOOK_MESSAGE( ServerName );
	HOOK_MESSAGE( Intermission );
	HOOK_MESSAGE( InitHUD );
	HOOK_MESSAGE( ViewMode );
	HOOK_MESSAGE( Concuss );
	HOOK_MESSAGE( RoomType );
	HOOK_MESSAGE( HUDColor );
	HOOK_MESSAGE( Particle );
	HOOK_MESSAGE( TempEntity );
	HOOK_MESSAGE( SetFog );
	HOOK_MESSAGE( SetSky );
	HOOK_MESSAGE( CamData );
	HOOK_MESSAGE( RainData ); 
	HOOK_MESSAGE( WeaponAnim );
	HOOK_MESSAGE( SetBody );
	HOOK_MESSAGE( SetSkin );
	HOOK_MESSAGE( AddMirror);
	HOOK_MESSAGE( AddScreen );
	HOOK_MESSAGE( AddPortal );
	HOOK_MESSAGE( ScreenFade );
	HOOK_MESSAGE( ScreenShake );

	viewEntityIndex = 0; // trigger_viewset stuff
	viewFlags = 0;
	m_flFOV = 0;
	m_iHUDColor = RGB_YELLOWISH; // 255, 160, 0
	
	CVAR_REGISTER( "zoom_sensitivity_ratio", "1.2", 0, "mouse sensitivity when zooming" );
	CVAR_REGISTER( "default_fov", "90", 0, "default client fov" );
	CVAR_REGISTER( "hud_draw", "1", FCVAR_ARCHIVE, "hud drawing modes" );
	CVAR_REGISTER( "hud_takesshots", "0", 0, "take screenshots at 30 fps" );
	CVAR_REGISTER( "hud_scale", "0", FCVAR_ARCHIVE|FCVAR_LATCH, "scale hud at current resolution" );

	// clear any old HUD list
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

	m_flTime = 1.0;

	return 1;
}

int CHud :: MsgFunc_ResetHUD(const char *pszName, int iSize, void *pbuf )
{
	// clear all hud data
	HUDLIST *pList = m_pHudList;

	while( pList )
	{
		if( pList->p ) pList->p->Reset();
		pList = pList->pNext;
	}

	// reset sensitivity
	m_flMouseSensitivity = 0;

	// reset concussion effect
	m_iConcussionEffect = 0;

	m_iIntermission = 0;

	// reset fog
	m_fStartDist = 0;
	m_fEndDist = 0;

	return 1;
}

int CHud :: MsgFunc_ViewMode( const char *pszName, int iSize, void *pbuf )
{
	// CAM_ToFirstPerson();
	return 1;
}

int CHud :: MsgFunc_InitHUD( const char *pszName, int iSize, void *pbuf )
{
	m_fStartDist = 0;
	m_fEndDist = 0;
	m_iSkyMode = SKY_OFF;
	m_iIntermission = 0;

	// prepare all hud data
	HUDLIST *pList = m_pHudList;

	while (pList)
	{
		if ( pList->p ) pList->p->InitHUDData();
		pList = pList->pNext;
	}

	return 1;
}

int CHud :: MsgFunc_Intermission( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );
	m_iIntermission = 1;
	END_READ();

	return 1;
}

int CHud::MsgFunc_HUDColor(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pszName, iSize, pbuf );

	m_iHUDColor = READ_LONG();

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_SetFog( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	m_FogColor.x = (float)(READ_BYTE() / 255.0f);
	m_FogColor.y = (float)(READ_BYTE() / 255.0f);
	m_FogColor.z = (float)(READ_BYTE() / 255.0f);

	m_fStartDist = READ_SHORT();
          m_fEndDist = READ_SHORT();

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_SetSky( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	m_iSkyMode = READ_BYTE();
	m_vecSkyPos[0] = READ_COORD();
	m_vecSkyPos[1] = READ_COORD();
	m_vecSkyPos[2] = READ_COORD();

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_GameMode( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	m_Teamplay = READ_BYTE();

	END_READ();
	
	return 1;
}


int CHud :: MsgFunc_Damage( const char *pszName, int iSize, void *pbuf )
{
	int	armor, blood;
	int	i;
	float	count;

	BEGIN_READ( pszName, iSize, pbuf );
	armor = READ_BYTE();
	blood = READ_BYTE();

	float	from[3];
	for( i = 0; i < 3; i++ )
		from[i] = READ_COORD();

	count = (blood * 0.5) + (armor * 0.5);
	if( count < 10 ) count = 10;

	END_READ();

	// TODO: kick viewangles,  show damage visually

	return 1;
}

int CHud :: MsgFunc_Concuss( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	m_iConcussionEffect = READ_BYTE();
	if( m_iConcussionEffect )
		m_StatusIcons.EnableIcon( "dmg_concuss", 255, 160, 0 );
	else m_StatusIcons.DisableIcon( "dmg_concuss" );

	END_READ();

	return 1;
}

int CHud :: MsgFunc_CamData( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	gHUD.viewEntityIndex = READ_SHORT();
	gHUD.viewFlags = READ_SHORT();

	if( gHUD.viewFlags )
		m_iCameraMode = 1;
	else m_iCameraMode = m_iLastCameraMode;

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_RainData( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	Rain.dripsPerSecond = READ_SHORT();
	Rain.distFromPlayer = READ_COORD();
	Rain.windX = READ_COORD();
	Rain.windY = READ_COORD();
	Rain.randX = READ_COORD();
	Rain.randY = READ_COORD();
	Rain.weatherMode = READ_SHORT();
	Rain.globalHeight= READ_COORD();	// FIXME: calc on client side ?

	END_READ();	

	return 1;
}

int CHud :: MsgFunc_WeaponAnim( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	edict_t *viewmodel = GetViewModel();
	viewmodel->v.sequence = READ_BYTE();
	viewmodel->v.body = READ_BYTE();
	viewmodel->v.framerate = READ_BYTE() * 0.0625;
	viewmodel->v.effects |= EF_ANIMATE;
	viewmodel->v.frame = -1; // force to start new sequence
	viewmodel->v.scale = 1.0f;

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_SetBody( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	edict_t *viewmodel = GetViewModel();
	viewmodel->v.body = READ_BYTE();

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_SetSkin( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	edict_t *viewmodel = GetViewModel();
	viewmodel->v.skin = READ_BYTE();

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_AddScreen( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	// AddScreenBrushEntity( READ_BYTE() );

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_AddMirror( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	// AddMirrorBrushEntity( READ_BYTE() );

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_AddPortal( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	// AddPortalBrushEntity( READ_BYTE() );

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_Particle( const char *pszName,  int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	int idx = READ_BYTE();
	char *sz = READ_STRING();
	// CreateAurora( idx, sz );

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_TempEntity( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	HUD_ParseTempEntity();

	END_READ();
	
	return 1;
}

int CHud::MsgFunc_ServerName( const char *pszName, int iSize, void *pbuf )
{
	char	m_szServerName[32];

	BEGIN_READ( pszName, iSize, pbuf );
	strncpy( m_szServerName, READ_STRING(), 32 );
	END_READ();
	
 	return 1;
}

int CHud :: MsgFunc_RoomType( const char *pszName, int iSize, void *pbuf )
{
	// FIXME: needs callback to sound engine
	return 1;
}

int CHud :: MsgFunc_ScreenFade( const char *pszName, int iSize, void *pbuf )
{
	// FIXME: implement
	return 1;
}

int CHud::MsgFunc_ScreenShake( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, iSize, pbuf );

	ShakeCommand_t eCommand = (ShakeCommand_t)READ_BYTE();
	float amplitude = READ_FLOAT();
	float frequency = READ_FLOAT();
	float duration = READ_FLOAT();

	if( eCommand == SHAKE_STOP )
	{
		m_Shake.amplitude = 0;
		m_Shake.frequency = 0;
		m_Shake.duration = 0;
	}
	else
	{
		if(( eCommand == SHAKE_START) || ( eCommand == SHAKE_FREQUENCY )) 
		{
			m_Shake.frequency = frequency;
		}

		if(( eCommand == SHAKE_START) || ( eCommand == SHAKE_AMPLITUDE ))
		{
			// don't overwrite larger existing shake unless we are told to.
			if(( amplitude > m_Shake.amplitude ) || ( eCommand == SHAKE_AMPLITUDE ))
			{
				m_Shake.amplitude = amplitude;
			}
		}

		// only reset the timer for a new shake.
		if( eCommand == SHAKE_START )
		{
			m_Shake.duration = duration;
			m_Shake.nextShake = 0;
			m_Shake.time = m_flTime + duration;
		}
	}

	END_READ();

	return 1;
}