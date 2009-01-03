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
//=========================================================
// GameRules.cpp
//=========================================================

#include	"extdll.h"
#include	"utils.h"
#include	"cbase.h"
#include	"player.h"
#include "baseweapon.h"
#include	"gamerules.h"
#include	"teamplay_gamerules.h"
#include	"game.h"

extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );

DLL_GLOBAL CGameRules*	g_pGameRules = NULL;
extern DLL_GLOBAL BOOL	g_fGameOver;

int g_teamplay = 0;

//=========================================================
//=========================================================
BOOL CGameRules::CanHaveAmmo( CBasePlayer *pPlayer, const char *pszAmmoName, int iMaxCarry )
{
	int iAmmoIndex;

	if ( pszAmmoName )
	{
		iAmmoIndex = pPlayer->GetAmmoIndex( pszAmmoName );

		if ( iAmmoIndex > -1 )
		{
			if ( pPlayer->AmmoInventory( iAmmoIndex ) < iMaxCarry )
			{
				// player has room for more of this type of ammo
				return TRUE;
			}
		}
	}

	return FALSE;
}

//=========================================================
//=========================================================
edict_t *CGameRules :: GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	edict_t *pentSpawnSpot = EntSelectSpawnPoint( pPlayer );

	pPlayer->pev->origin = VARS(pentSpawnSpot)->origin + Vector(0,0,1);
	pPlayer->pev->v_angle  = g_vecZero;
	pPlayer->pev->velocity = g_vecZero;
	pPlayer->pev->angles = VARS(pentSpawnSpot)->angles;
	pPlayer->pev->punchangle = g_vecZero;
	pPlayer->pev->fixangle = TRUE;
	
	//LRC
	if (pentSpawnSpot->v.spawnflags & 1) // the START WITH SUIT flag
	{
		g_startSuit = TRUE;
	}
	
	return pentSpawnSpot;
}

//=========================================================
//=========================================================
BOOL CGameRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerWeapon *pWeapon )
{
	// only living players can have items
	if ( pPlayer->pev->deadflag != DEAD_NO )
		return FALSE;

	if ( pWeapon->pszAmmo1() )
	{
		if ( !CanHaveAmmo( pPlayer, pWeapon->pszAmmo1(), pWeapon->iMaxAmmo1() ) )
		{
			// we can't carry anymore ammo for this gun. We can only 
			// have the gun if we aren't already carrying one of this type
			if ( pPlayer->HasPlayerItem( pWeapon ) )
			{
				return FALSE;
			}
		}
	}
	else
	{
		// weapon doesn't use ammo, don't take another if you already have it.
		if ( pPlayer->HasPlayerItem( pWeapon ) )
		{
			return FALSE;
		}
	}

	// note: will fall through to here if GetItemInfo doesn't fill the struct!
	return TRUE;
}

//=========================================================
// load the SkillData struct with the proper values based on the skill level.
//=========================================================
void CGameRules::RefreshSkillData ( void )
{
}

//=========================================================
// instantiate the proper game rules object
//=========================================================

CGameRules *InstallGameRules( void )
{
	SERVER_COMMAND( "exec game.rc\n" );
	SERVER_EXECUTE( );

	ALERT( at_aiconsole, "InstallGameRules\n" );

	if ( !gpGlobals->deathmatch )
	{
		// generic half-life
		g_teamplay = 0;
		return new CHalfLifeRules;
	}
	else
	{
		if( CVAR_GET_FLOAT( "mp_teamplay" ) > 0 )
		{
			// teamplay

			g_teamplay = 1;
			return new CHalfLifeTeamplay;
		}
		if ((int)gpGlobals->deathmatch == 1)
		{
			// vanilla deathmatch
			g_teamplay = 0;
			return new CHalfLifeMultiplay;
		}
		else
		{
			// vanilla deathmatch??
			g_teamplay = 0;
			return new CHalfLifeMultiplay;
		}
	}
}



