//=======================================================================
//			Copyright (C) Shambler Team 2004
//		         util_.cpp - utility entities: util_fade, 
//			       util_shake, util_fov etc			    
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "saverestore.h"
#include "player.h"
#include "defaults.h"
#include "shake.h"

//=====================================================
// util_changehud: change player hud
//=====================================================
class CUtilChangeHUD : public CBaseLogic
{
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData* );
	void SetHUD( int hud_num );
	void ResetHUD( void );
	char* GetStringForMode( void );
};

void CUtilChangeHUD :: KeyValue( KeyValueData* pkvd )
{
	if (FStrEq(pkvd->szKeyName, "hud"))
	{
		pev->skin = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBaseEntity::KeyValue( pkvd );
}

void CUtilChangeHUD :: SetHUD( int hud_num )
{
	m_iState = STATE_ON;
	((CBasePlayer *)((CBaseEntity *)m_hActivator))->m_iWarHUD = pev->skin = hud_num;
}

void CUtilChangeHUD :: ResetHUD( void )
{
	m_iState = STATE_OFF;
	((CBasePlayer *)((CBaseEntity *)m_hActivator))->m_iWarHUD = 0;
	((CBasePlayer *)((CBaseEntity *)m_hActivator))->fadeNeedsUpdate = 1;
}

void CUtilChangeHUD :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if(pActivator && pActivator->IsPlayer())//only at player
	{
		m_hActivator = pActivator;//save activator
	}	
	else m_hActivator = UTIL_PlayerByIndex( 1 );
	
	if (useType == USE_TOGGLE)
	{
		if(m_iState == STATE_OFF) useType = USE_ON;
		else useType = USE_OFF;
	}
	if (useType == USE_ON ) SetHUD( pev->skin );
	else if (useType == USE_OFF) ResetHUD();
	else if (useType == USE_SET)
	{
		if(value) SetHUD( value );
		else ResetHUD();
	}
	else if (useType == USE_RESET) ResetHUD();
	else if (useType == USE_SHOWINFO)
	{
		ALERT(at_console, "======/Xash Debug System/======\n");
		ALERT(at_console, "classname: %s\n", STRING(pev->classname));
		ALERT(at_console, "State: %s, HUD Mode: %s\n", GetStringForState( GetState()), GetStringForMode());
		SHIFT;
	}
}

char* CUtilChangeHUD :: GetStringForMode( void )
{
	switch(pev->skin)
	{
		case 0: return "Disable Custom HUD";
		case 1: return "Draw Redeemer HUD";
		case 2: return "Draw Redeemer Underwater HUD";
		case 3: return "Draw Redeemer Noise Screen";
		case 4: return "Draw Security Camera Screen";
	default:	return "Draw None";
	}
}
LINK_ENTITY_TO_CLASS( util_changehud, CUtilChangeHUD );

//=====================================================
// util_command: activate a console command
//=====================================================
class CUtilCommand : public CBaseEntity
{
public:
	void PostActvaite( void )
	{
		if(pev->spawnflags & SF_START_ON) Use( this, this, USE_ON, 0 );
	}
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		char szCommand[256];

		if (useType == USE_SHOWINFO)
		{
			ALERT(at_console, "======/Xash Debug System/======\n");
			ALERT(at_console, "classname: %s\n", STRING(pev->classname));
			ALERT(at_console, "Command: %s\n", STRING(pev->netname));
			SHIFT;
		}
		else if (pev->netname)
		{
			const char *pString = (char *)STRING( pev->netname );
			int command, namelen = strlen(pString) - 1;
			if( namelen > 2) command = pev->netname;//command name
			else command = atoi( pString );
			switch( command )
			{
			case 0: strcpy( szCommand, "pause\n" ); break;	//pause level
			case 1: strcpy( szCommand, "reload\n" ); break;	//revert to saved game
			case 2: strcpy( szCommand, "game_over\n" ); break;//turn to menu
			case 3: strcpy( szCommand, "autosave\n" ); break; //autosave game
			case 4: strcpy( szCommand, "restart\n" ); break; //restart level
			case 5: strcpy( szCommand, "save quick\n" ); break; //emaulate F6
			case 6: strcpy( szCommand, "load quick\n" ); break; //emulate F7
			case 7: strcpy( szCommand, "exit\n" ); break; //return to windows
			case 8: strcpy( szCommand, "gametitle\n" ); break; //show game title
			case 9: strcpy( szCommand, "intermission\n" ); break; //show intermission
			default: sprintf( szCommand, "%s\n", STRING(pev->netname) ); break; //custom command
			}
			SERVER_COMMAND( szCommand );
			if ( pev->spawnflags & SF_FIREONCE ) UTIL_Remove( this );
		}
	}
};
LINK_ENTITY_TO_CLASS( util_command, CUtilCommand );

//=========================================================
//		display message text
//=========================================================
class CGameText : public CBaseLogic
{
public:
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
private:
	hudtextparms_t	m_textParms;
};
LINK_ENTITY_TO_CLASS( util_msgtext, CGameText );

TYPEDESCRIPTION	CGameText::m_SaveData[] = 
{ DEFINE_ARRAY( CGameText, m_textParms, FIELD_CHARACTER, sizeof(hudtextparms_t) ),
};IMPLEMENT_SAVERESTORE( CGameText, CBaseLogic );


void CGameText::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "channel"))
	{
		m_textParms.channel = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "x"))
	{
		m_textParms.x = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "y"))
	{
		m_textParms.y = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "effect"))
	{
		m_textParms.effect = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "color"))
	{
		int color[4];
		UTIL_StringToIntArray( color, 4, pkvd->szValue );
		m_textParms.r1 = color[0];
		m_textParms.g1 = color[1];
		m_textParms.b1 = color[2];
		m_textParms.a1 = color[3];
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "color2"))
	{
		int color[4];
		UTIL_StringToIntArray( color, 4, pkvd->szValue );
		m_textParms.r2 = color[0];
		m_textParms.g2 = color[1];
		m_textParms.b2 = color[2];
		m_textParms.a2 = color[3];
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "fadein"))
	{
		m_textParms.fadeinTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "fadeout"))
	{
		m_textParms.fadeoutTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "holdtime"))
	{
		m_textParms.holdTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "fxtime"))
	{
		m_textParms.fxTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	CBaseLogic::KeyValue( pkvd );
}

void CGameText::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
          CBaseEntity *pPlayer = NULL;
	if ( IsMultiplayer())UTIL_HudMessageAll( m_textParms, STRING(pev->message) );
	else
	{
		if ( pActivator && pActivator->IsPlayer() ) pPlayer = pActivator;
		else pPlayer = UTIL_PlayerByIndex( 1 );
	}
	if ( pPlayer ) UTIL_HudMessage( pPlayer, m_textParms, STRING(pev->message));
	if ( pev->spawnflags & SF_FIREONCE ) UTIL_Remove( this );
}

//=========================================================
//	remove all items
//=========================================================
#define SF_REMOVE_SUIT 1

class CUtilStripWeapons : public CPointEntity
{
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		CBasePlayer *pPlayer = NULL;

		if ( pActivator && pActivator->IsPlayer() ) pPlayer = (CBasePlayer *)pActivator;
		else pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( 1 );
		if ( pPlayer ) pPlayer->RemoveAllItems( pev->spawnflags & SF_REMOVE_SUIT );
		if ( pev->spawnflags & SF_FIREONCE ) UTIL_Remove( this );
	}
};
LINK_ENTITY_TO_CLASS( util_weaponstrip, CUtilStripWeapons );

//=========================================================
// set global fog on a map
//=========================================================
class CUtilSetFog : public CBaseLogic
{
public:
	void PostActivate( void );
	void KeyValue( KeyValueData *pkvd );
	void TurnOn( void );
	void TurnOff( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};
LINK_ENTITY_TO_CLASS( util_setfog, CUtilSetFog );

void CUtilSetFog :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "startdist"))
	{
		pev->dmg_take = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "enddist"))
	{
		pev->dmg_save = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "fadetime"))
	{
		m_flDelay = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBaseEntity::KeyValue( pkvd );
}

void CUtilSetFog :: PostActivate ( void )
{
	pev->effects |= EF_NODRAW;
	if (pev->spawnflags & SF_START_ON) 
	{
		TurnOn();
		ClearBits(pev->spawnflags, SF_START_ON);
	}
}

void CUtilSetFog :: TurnOn ( void )
{
	m_iState = STATE_ON;
	UTIL_SetFogAll(pev->rendercolor, m_flDelay, pev->dmg_take, pev->dmg_save );
}

void CUtilSetFog :: TurnOff ( void )
{
	m_iState = STATE_OFF;
	UTIL_SetFogAll(pev->rendercolor, -m_flDelay, pev->dmg_take, pev->dmg_save );
}

void CUtilSetFog :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (useType == USE_TOGGLE)
	{
		if(m_iState == STATE_ON) useType = USE_OFF;
		else useType = USE_ON;
	}
	if (useType == USE_ON) TurnOn();
	else if ( useType == USE_OFF ) TurnOff();
}

//=========================================================
//	UTIL_RainModify (set new rain or snow)
//=========================================================
void CUtilRainModify::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;

	if (IsMultiplayer() || FStringNull( pev->targetname))
		SetBits( pev->spawnflags, SF_START_ON );
}

TYPEDESCRIPTION	CUtilRainModify::m_SaveData[] = 
{
	DEFINE_FIELD( CUtilRainModify, randXY, FIELD_RANGE ),
	DEFINE_FIELD( CUtilRainModify, windXY, FIELD_RANGE ),
};
IMPLEMENT_SAVERESTORE( CUtilRainModify, CPointEntity );

void CUtilRainModify::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "drips"))
	{
		pev->button = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "fadetime"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "rand"))
	{
		randXY = RandomRange(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "wind"))
	{
		windXY = RandomRange(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

void CUtilRainModify::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if(useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		Msg("Wind X: %g Y: %g. Rand X: %g Y: %g\n", windXY[1], windXY[0], randXY[1], randXY[0] );
		Msg("Fade time %g. Drips per second %d\n", pev->dmg, pev->button );
	}
	else if(!pev->spawnflags & SF_START_ON)
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(1);
		if (pev->dmg)
		{
			pPlayer->Rain_ideal_dripsPerSecond = pev->button;
			pPlayer->Rain_ideal_randX = randXY[1];
			pPlayer->Rain_ideal_randY = randXY[0];
			pPlayer->Rain_ideal_windX = windXY[1];
			pPlayer->Rain_ideal_windY = windXY[0];
			pPlayer->Rain_endFade = gpGlobals->time + pev->dmg;
			pPlayer->Rain_nextFadeUpdate = gpGlobals->time + 1;
		}
		else
		{
			pPlayer->Rain_dripsPerSecond = pev->button;
			pPlayer->Rain_randX = randXY[1];
			pPlayer->Rain_randY = randXY[0];
			pPlayer->Rain_windX = windXY[1];
			pPlayer->Rain_windY = windXY[0];
			pPlayer->rainNeedsUpdate = 1;
		}
	}
}
LINK_ENTITY_TO_CLASS( util_rainmodify, CUtilRainModify );