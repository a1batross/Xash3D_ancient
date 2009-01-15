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
/*

===== player.cpp ========================================================

  functions dealing with the player

*/

#include "extdll.h"
#include "utils.h"

#include "cbase.h"
#include "client.h"
#include "player.h"
#include "nodes.h"
#include "baseweapon.h"
#include "soundent.h"
#include "monsters.h"
#include "shake.h"
#include "decals.h"
#include "gamerules.h"
#include "game.h"
#include "basebeams.h"
#include "defaults.h"


extern DLL_GLOBAL ULONG		g_ulModelIndexPlayer;
extern DLL_GLOBAL BOOL		g_fGameOver;
extern DLL_GLOBAL	BOOL	g_fDrawLines;
float g_flWeaponCheat;
int gEvilImpulse101;
BOOL GiveOnlyAmmo;
BOOL g_markFrameBounds = 0; //LRC
BOOL gInitHUD = TRUE;

int MapTextureTypeStepType(char chTextureType);
extern void CopyToBodyQue(entvars_t* pev);
extern void respawn(entvars_t *pev, BOOL fCopyCorpse);
extern Vector VecBModelOrigin(entvars_t *pevBModel );
extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );
extern void LinkUserMessages( void );

// the world node graph
extern CGraph	WorldGraph;

#define PLAYER_WALLJUMP_SPEED 300 // how fast we can spring off walls
#define PLAYER_LONGJUMP_SPEED 350 // how fast we longjump

#define TRAIN_ACTIVE	0x80
#define TRAIN_NEW		0xc0
#define TRAIN_OFF		0x00
#define TRAIN_NEUTRAL	0x01
#define TRAIN_SLOW		0x02
#define TRAIN_MEDIUM	0x03
#define TRAIN_FAST		0x04
#define TRAIN_BACK		0x05

#define	FLASH_DRAIN_TIME	 0.1 //100 units/3 minutes
#define	FLASH_CHARGE_TIME	 0.2 // 100 units/20 seconds  (seconds per unit)

// Global Savedata for player
TYPEDESCRIPTION	CBasePlayer::m_playerSaveData[] =
{
	DEFINE_FIELD( CBasePlayer, m_flFlashLightTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_iFlashBattery, FIELD_INTEGER ),

	DEFINE_FIELD( CBasePlayer, m_afButtonLast, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_afButtonPressed, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_afButtonReleased, FIELD_INTEGER ),

	DEFINE_ARRAY( CBasePlayer, m_rgItems, FIELD_INTEGER, MAX_ITEMS ),
	DEFINE_FIELD( CBasePlayer, m_afPhysicsFlags, FIELD_INTEGER ),

	DEFINE_FIELD( CBasePlayer, m_flTimeStepSound, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flTimeWeaponIdle, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flSwimTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flDuckTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flWallJumpTime, FIELD_TIME ),

	DEFINE_FIELD( CBasePlayer, m_fAirFinished, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_fPainFinished, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flSuitUpdate, FIELD_TIME ),
	DEFINE_ARRAY( CBasePlayer, m_rgSuitPlayList, FIELD_INTEGER, CSUITPLAYLIST ),
	DEFINE_FIELD( CBasePlayer, m_iSuitPlayNext, FIELD_INTEGER ),
	DEFINE_ARRAY( CBasePlayer, m_rgiSuitNoRepeat, FIELD_INTEGER, CSUITNOREPEAT ),
	DEFINE_ARRAY( CBasePlayer, m_rgflSuitNoRepeatTime, FIELD_TIME, CSUITNOREPEAT ),
	DEFINE_ARRAY( CBasePlayer, m_szAnimExtention, FIELD_CHARACTER, 32),
	DEFINE_FIELD( CBasePlayer, m_lastDamageAmount, FIELD_INTEGER ),

	DEFINE_ARRAY( CBasePlayer, m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES ),
	DEFINE_FIELD( CBasePlayer, m_pActiveItem, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayer, m_pLastItem, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayer, m_pNextItem, FIELD_CLASSPTR ),

	DEFINE_ARRAY( CBasePlayer, m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS ),
	DEFINE_FIELD( CBasePlayer, m_idrowndmg, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_idrownrestored, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_tSneaking, FIELD_TIME ),

	DEFINE_FIELD( CBasePlayer, m_iTrain, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_bitsHUDDamage, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_flFallVelocity, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, m_iTargetVolume, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iWeaponVolume, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iExtraSoundTypes, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iWeaponFlash, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_fLongJump, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_fInitHUD, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_tbdPrev, FIELD_TIME ),

	DEFINE_FIELD( CBasePlayer, m_pTank, FIELD_EHANDLE ), // NB: this points to a CFuncTank*Controls* now. --LRC
	DEFINE_FIELD( CBasePlayer, m_pMonitor, FIELD_EHANDLE ),
	DEFINE_FIELD( CBasePlayer, m_iHideHUD, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, pViewEnt, FIELD_CLASSPTR),
	DEFINE_FIELD( CBasePlayer, viewFlags, FIELD_INTEGER),
	DEFINE_FIELD( CBasePlayer, m_iSndRoomtype, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iFogStartDist, FIELD_INTEGER),
	DEFINE_FIELD( CBasePlayer, m_iFogEndDist, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iFogFinalEndDist, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_FogColor, FIELD_VECTOR ),
	DEFINE_FIELD( CBasePlayer, m_FogFadeTime, FIELD_INTEGER),
	DEFINE_FIELD( CBasePlayer, m_iWarHUD, FIELD_INTEGER),
          
          DEFINE_FIELD( CBasePlayer, m_FadeColor, FIELD_VECTOR ),
          DEFINE_FIELD( CBasePlayer, m_FadeAlpha, FIELD_INTEGER ),
          DEFINE_FIELD( CBasePlayer, m_iFadeFlags, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iFadeHold, FIELD_INTEGER ),
          
          DEFINE_FIELD( CBasePlayer, m_flStartTime, FIELD_TIME ),
	
	DEFINE_FIELD( CBasePlayer, Rain_dripsPerSecond, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, Rain_windX, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, Rain_windY, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, Rain_randX, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, Rain_randY, FIELD_FLOAT ),

	DEFINE_FIELD( CBasePlayer, Rain_ideal_dripsPerSecond, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, Rain_ideal_windX, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, Rain_ideal_windY, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, Rain_ideal_randX, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, Rain_ideal_randY, FIELD_FLOAT ),

	DEFINE_FIELD( CBasePlayer, Rain_endFade, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, Rain_nextFadeUpdate, FIELD_TIME ),

	//LRC
	//DEFINE_FIELD( CBasePlayer, m_iFogStartDist, FIELD_INTEGER ),
	//DEFINE_FIELD( CBasePlayer, m_iFogEndDist, FIELD_INTEGER ),
	//DEFINE_FIELD( CBasePlayer, m_vecFogColor, FIELD_VECTOR ),

	//DEFINE_FIELD( CBasePlayer, m_fDeadTime, FIELD_FLOAT ), // only used in multiplayer games
	//DEFINE_FIELD( CBasePlayer, m_fGameHUDInitialized, FIELD_INTEGER ), // only used in multiplayer games
	//DEFINE_FIELD( CBasePlayer, m_flStopExtraSoundTime, FIELD_TIME ),
	//DEFINE_FIELD( CBasePlayer, m_fKnownItem, FIELD_INTEGER ), // reset to zero on load
	//DEFINE_FIELD( CBasePlayer, m_iPlayerSound, FIELD_INTEGER ),	// Don't restore, set in Precache()
	//DEFINE_FIELD( CBasePlayer, m_pentSndLast, FIELD_EDICT ),	// Don't restore, client needs reset
	//DEFINE_FIELD( CBasePlayer, m_flSndRange, FIELD_FLOAT ),	// Don't restore, client needs reset
	//DEFINE_FIELD( CBasePlayer, m_fNewAmmo, FIELD_INTEGER ), // Don't restore, client needs reset
	//DEFINE_FIELD( CBasePlayer, m_flgeigerRange, FIELD_FLOAT ),	// Don't restore, reset in Precache()
	//DEFINE_FIELD( CBasePlayer, m_flgeigerDelay, FIELD_FLOAT ),	// Don't restore, reset in Precache()
	//DEFINE_FIELD( CBasePlayer, m_igeigerRangePrev, FIELD_FLOAT ),	// Don't restore, reset in Precache()
	//DEFINE_FIELD( CBasePlayer, m_iStepLeft, FIELD_INTEGER ), // Don't need to restore
	//DEFINE_ARRAY( CBasePlayer, m_szTextureName, FIELD_CHARACTER, CBTEXTURENAMEMAX ), // Don't need to restore
	//DEFINE_FIELD( CBasePlayer, m_chTextureType, FIELD_CHARACTER ), // Don't need to restore
	//DEFINE_FIELD( CBasePlayer, m_fNoPlayerSound, FIELD_BOOLEAN ), // Don't need to restore, debug
	//DEFINE_FIELD( CBasePlayer, m_iUpdateTime, FIELD_INTEGER ), // Don't need to restore
	//DEFINE_FIELD( CBasePlayer, m_iClientHealth, FIELD_INTEGER ), // Don't restore, client needs reset
	//DEFINE_FIELD( CBasePlayer, m_iClientBattery, FIELD_INTEGER ), // Don't restore, client needs reset
	//DEFINE_FIELD( CBasePlayer, m_iClientHideHUD, FIELD_INTEGER ), // Don't restore, client needs reset
	//DEFINE_FIELD( CBasePlayer, m_fWeapon, FIELD_BOOLEAN ),  // Don't restore, client needs reset
	//DEFINE_FIELD( CBasePlayer, m_nCustomSprayFrames, FIELD_INTEGER ), // Don't restore, depends on server message after spawning and only matters in multiplayer
	//DEFINE_FIELD( CBasePlayer, m_vecAutoAim, FIELD_VECTOR ), // Don't save/restore - this is recomputed
	//DEFINE_ARRAY( CBasePlayer, m_rgAmmoLast, FIELD_INTEGER, MAX_AMMO_SLOTS ), // Don't need to restore
	//DEFINE_FIELD( CBasePlayer, m_fOnTarget, FIELD_BOOLEAN ), // Don't need to restore
	//DEFINE_FIELD( CBasePlayer, m_nCustomSprayFrames, FIELD_INTEGER ), // Don't need to restore

};
LINK_ENTITY_TO_CLASS( player, CBasePlayer );



void CBasePlayer :: Pain( void )
{
	float	flRndSound;//sound randomizer

	flRndSound = RANDOM_FLOAT ( 0 , 1 );

	if ( flRndSound <= 0.33 )
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain5.wav", 1, ATTN_NORM);
	else if ( flRndSound <= 0.66 )
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain6.wav", 1, ATTN_NORM);
	else
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain7.wav", 1, ATTN_NORM);
}

/*
 *
 */
Vector VecVelocityForDamage(float flDamage)
{
	Vector vec(RANDOM_FLOAT(-100,100), RANDOM_FLOAT(-100,100), RANDOM_FLOAT(200,300));

	if (flDamage > -50)
		vec = vec * 0.7;
	else if (flDamage > -200)
		vec = vec * 2;
	else
		vec = vec * 10;

	return vec;
}

#if 0 /*
static void ThrowGib(entvars_t *pev, char *szGibModel, float flDamage)
{
	edict_t *pentNew = CREATE_ENTITY();
	entvars_t *pevNew = VARS(pentNew);

	pevNew->origin = pev->origin;
	SET_MODEL(ENT(pevNew), szGibModel);
	UTIL_SetSize(pevNew, g_vecZero, g_vecZero);

	pevNew->velocity		= VecVelocityForDamage(flDamage);
	pevNew->movetype		= MOVETYPE_BOUNCE;
	pevNew->solid			= SOLID_NOT;
	pevNew->avelocity.x		= RANDOM_FLOAT(0,600);
	pevNew->avelocity.y		= RANDOM_FLOAT(0,600);
	pevNew->avelocity.z		= RANDOM_FLOAT(0,600);
	CHANGE_METHOD(ENT(pevNew), em_think, Remove);
	pevNew->ltime		= gpGlobals->time;
	pevNew->nextthink	= gpGlobals->time + RANDOM_FLOAT(10,20);
	pevNew->frame		= 0;
	pevNew->flags		= 0;
}


static void ThrowHead(entvars_t *pev, char *szGibModel, floatflDamage)
{
	SET_MODEL(ENT(pev), szGibModel);
	pev->frame			= 0;
	pev->nextthink		= -1;
	pev->movetype		= MOVETYPE_BOUNCE;
	pev->takedamage		= DAMAGE_NO;
	pev->solid			= SOLID_NOT;
	pev->view_ofs		= Vector(0,0,8);
	UTIL_SetSize(pev, Vector(-16,-16,0), Vector(16,16,56));
	pev->velocity		= VecVelocityForDamage(flDamage);
	pev->avelocity		= RANDOM_FLOAT(-1,1) * Vector(0,600,0);
	pev->origin.z -= 24;
	ClearBits(pev->flags, FL_ONGROUND);
}


*/
#endif

int TrainSpeed(int iSpeed, int iMax)
{
	float fSpeed, fMax;
	int iRet = 0;

	fMax = (float)iMax;
	fSpeed = iSpeed;

	fSpeed = fSpeed/fMax;

	if (iSpeed < 0)
		iRet = TRAIN_BACK;
	else if (iSpeed == 0)
		iRet = TRAIN_NEUTRAL;
	else if (fSpeed < 0.33)
		iRet = TRAIN_SLOW;
	else if (fSpeed < 0.66)
		iRet = TRAIN_MEDIUM;
	else
		iRet = TRAIN_FAST;

	return iRet;
}

void CBasePlayer :: DeathSound( void )
{
	// water death sounds
	/*
	if (pev->waterlevel == 3)
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/h2odeath.wav", 1, ATTN_NONE);
		return;
	}
	*/

	// temporarily using pain sounds for death sounds
	switch (RANDOM_LONG(1,5))
	{
	case 1:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain5.wav", 1, ATTN_NORM);
		break;
	case 2:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain6.wav", 1, ATTN_NORM);
		break;
	case 3:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain7.wav", 1, ATTN_NORM);
		break;
	}

	// play one of the suit death alarms
	//LRC- if no suit, then no flatline sound. (unless it's a deathmatch.)
	if (!(m_iHideHUD & ITEM_SUIT) && !g_pGameRules->IsDeathmatch() )return;
	EMIT_GROUPNAME_SUIT(ENT(pev), "HEV_DEAD");
}

// override takehealth
// bitsDamageType indicates type of damage healed.

int CBasePlayer :: TakeHealth( float flHealth, int bitsDamageType )
{
	return CBaseMonster::TakeHealth (flHealth, bitsDamageType);
}

int CBasePlayer :: TakeArmor( float flArmor, int suit )
{
	if(!(m_iHideHUD & ITEM_SUIT)) return 0;
	
	if(CBaseMonster::TakeArmor(flArmor ))
	{
		if(!suit) return 1; //silent take armor
		int pct;
		char szcharge[64];

		// Suit reports new power level
		pct = (int)( (float)(pev->armorvalue * 100.0) * (1.0/MAX_NORMAL_BATTERY) + 0.5);
		pct = (pct / 5);
		if (pct > 0) pct--;
		
		sprintf( szcharge,"!HEV_%1dP", pct );
		SetSuitUpdate(szcharge, FALSE, SUIT_NEXT_IN_30SEC);
		
		return 1;
	}
	return 0;
}

int CBasePlayer :: TakeItem( int iItem )
{
	if ( m_iHideHUD & iItem )
		return 0;
	return 1;
}

Vector CBasePlayer :: GetGunPosition( )
{
	return pev->origin + pev->view_ofs;
}

//=========================================================
// TraceAttack
//=========================================================
void CBasePlayer :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	if ( pev->takedamage )
	{
		m_LastHitGroup = ptr->iHitgroup;

		switch ( ptr->iHitgroup )
		{
		case HITGROUP_GENERIC:
			break;
		case HITGROUP_HEAD:
			flDamage *= DMG_HEAD;
			break;
		case HITGROUP_CHEST:
			flDamage *= DMG_CHEST;
			break;
		case HITGROUP_STOMACH:
			flDamage *= DMG_STOMACH;
			break;
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
			flDamage *= DMG_ARM;
			break;
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			flDamage *= DMG_LEG;
			break;
		default:
			break;
		}

		if(bitsDamageType & DMG_NUCLEAR)
		{
			m_FadeColor = Vector(255, 255, 255);
			m_FadeAlpha = 240;			
			m_iFadeFlags = 0;
			m_iFadeTime = 25;
			fadeNeedsUpdate = TRUE;
		}
		else SpawnBlood(ptr->vecEndPos, BloodColor(), flDamage);// a little surface blood.
		TraceBleed( flDamage, vecDir, ptr, bitsDamageType );
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
	}
}

/*
	Take some damage.
	NOTE: each call to TakeDamage with bitsDamageType set to a time-based damage
	type will cause the damage time countdown to be reset.  Thus the ongoing effects of poison, radiation
	etc are implemented with subsequent calls to TakeDamage using DMG_GENERIC.
*/

#define ARMOR_RATIO	 0.2	// Armor Takes 80% of the damage
#define ARMOR_BONUS  0.5	// Each Point of Armor is work 1/x points of health

int CBasePlayer :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// have suit diagnose the problem - ie: report damage type
	int bitsDamage = bitsDamageType;
	int ffound = TRUE;
	int fmajor;
	int fcritical;
	int fTookDamage;
	int ftrivial;
	float flRatio;
	float flBonus;
	float flHealthPrev = pev->health;

	flBonus = ARMOR_BONUS;
	flRatio = ARMOR_RATIO;

	if ( ( bitsDamageType & DMG_BLAST ) && g_pGameRules->IsMultiplayer() )
	{
		// blasts damage armor more.
		flBonus *= 2;
	}

	// Already dead
	if ( !IsAlive() )
		return 0;
	// go take the damage first


	CBaseEntity *pAttacker = CBaseEntity::Instance(pevAttacker);

	if ( !g_pGameRules->FPlayerCanTakeDamage( this, pAttacker ) )
	{
		// Refuse the damage
		return 0;
	}

	// keep track of amount of damage last sustained
	m_lastDamageAmount = flDamage;

	// Armor.
	if (pev->armorvalue && !(bitsDamageType & (DMG_FALL | DMG_DROWN)) )// armor doesn't protect against fall or drown damage!
	{
		float flNew = flDamage * flRatio;

		float flArmor;

		flArmor = (flDamage - flNew) * flBonus;

		// Does this use more armor than we have?
		if (flArmor > pev->armorvalue)
		{
			flArmor = pev->armorvalue;
			flArmor *= (1/flBonus);
			flNew = flDamage - flArmor;
			pev->armorvalue = 0;
		}
		else
			pev->armorvalue -= flArmor;

		flDamage = flNew;
	}

	// this cast to INT is critical!!! If a player ends up with 0.5 health, the engine will get that
	// as an int (zero) and think the player is dead! (this will incite a clientside screentilt, etc)
	fTookDamage = CBaseMonster::TakeDamage(pevInflictor, pevAttacker, (int)flDamage, bitsDamageType);

	// reset damage time countdown for each type of time based damage player just sustained

	{
		for (int i = 0; i < CDMG_TIMEBASED; i++)
			if (bitsDamageType & (DMG_PARALYZE << i))
				m_rgbTimeBasedDamage[i] = 0;
	}


	// how bad is it, doc?

	ftrivial = (pev->health > 75 || m_lastDamageAmount < 5);
	fmajor = (m_lastDamageAmount > 25);
	fcritical = (pev->health < 30);

	// handle all bits set in this damage message,
	// let the suit give player the diagnosis

	// UNDONE: add sounds for types of damage sustained (ie: burn, shock, slash )

	// UNDONE: still need to record damage and heal messages for the following types

		// DMG_BURN
		// DMG_FREEZE
		// DMG_BLAST
		// DMG_SHOCK

	m_bitsDamageType |= bitsDamage; // Save this so we can report it to the client
	m_bitsHUDDamage = -1;  // make sure the damage bits get resent

	while (fTookDamage && (!ftrivial || (bitsDamage & DMG_TIMEBASED)) && ffound && bitsDamage)
	{
		ffound = FALSE;

		if (bitsDamage & DMG_CLUB)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG4", FALSE, SUIT_NEXT_IN_30SEC);	// minor fracture
			bitsDamage &= ~DMG_CLUB;
			ffound = TRUE;
		}
		if (bitsDamage & (DMG_FALL | DMG_CRUSH))
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG5", FALSE, SUIT_NEXT_IN_30SEC);	// major fracture
			else
				SetSuitUpdate("!HEV_DMG4", FALSE, SUIT_NEXT_IN_30SEC);	// minor fracture

			bitsDamage &= ~(DMG_FALL | DMG_CRUSH);
			ffound = TRUE;
		}

		if (bitsDamage & DMG_BULLET)
		{
			if (m_lastDamageAmount > 5)
				SetSuitUpdate("!HEV_DMG6", FALSE, SUIT_NEXT_IN_30SEC);	// blood loss detected
			//else
			//	SetSuitUpdate("!HEV_DMG0", FALSE, SUIT_NEXT_IN_30SEC);	// minor laceration

			bitsDamage &= ~DMG_BULLET;
			ffound = TRUE;
		}

		if (bitsDamage & DMG_SLASH)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG1", FALSE, SUIT_NEXT_IN_30SEC);	// major laceration
			else
				SetSuitUpdate("!HEV_DMG0", FALSE, SUIT_NEXT_IN_30SEC);	// minor laceration

			bitsDamage &= ~DMG_SLASH;
			ffound = TRUE;
		}
		if (bitsDamage & DMG_SONIC)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG2", FALSE, SUIT_NEXT_IN_1MIN);	// internal bleeding
			bitsDamage &= ~DMG_SONIC;
			ffound = TRUE;
		}

		if (bitsDamage & (DMG_POISON | DMG_PARALYZE))
		{
			SetSuitUpdate("!HEV_DMG3", FALSE, SUIT_NEXT_IN_1MIN);	// blood toxins detected
			bitsDamage &= ~(DMG_POISON | DMG_PARALYZE);
			ffound = TRUE;
		}

		if (bitsDamage & DMG_ACID)
		{
			SetSuitUpdate("!HEV_DET1", FALSE, SUIT_NEXT_IN_1MIN);	// hazardous chemicals detected
			bitsDamage &= ~DMG_ACID;
			ffound = TRUE;
		}

		if (bitsDamage & DMG_NERVEGAS)
		{
			SetSuitUpdate("!HEV_DET0", FALSE, SUIT_NEXT_IN_1MIN);	// biohazard detected
			bitsDamage &= ~DMG_NERVEGAS;
			ffound = TRUE;
		}

		if (bitsDamage & DMG_RADIATION)
		{
			SetSuitUpdate("!HEV_DET2", FALSE, SUIT_NEXT_IN_1MIN);	// radiation detected
			bitsDamage &= ~DMG_RADIATION;
			ffound = TRUE;
		}
		if (bitsDamage & DMG_NUCLEAR)
		{
			SetSuitUpdate("!HEV_DET2", FALSE, SUIT_NEXT_IN_1MIN);	// radiation detected
			bitsDamage &= ~DMG_NUCLEAR;
			ffound = TRUE;
		}
		if (bitsDamage & DMG_SHOCK)
		{
			bitsDamage &= ~DMG_SHOCK;
			ffound = TRUE;
		}
	}

	pev->punchangle.x = -2;

	if (fTookDamage && !ftrivial && fmajor && flHealthPrev >= 75)
	{
		// first time we take major damage...
		// turn automedic on if not on
		SetSuitUpdate("!HEV_MED1", FALSE, SUIT_NEXT_IN_30MIN);	// automedic on

		// give morphine shot if not given recently
		SetSuitUpdate("!HEV_HEAL7", FALSE, SUIT_NEXT_IN_30MIN);	// morphine shot
	}

	if (fTookDamage && !ftrivial && fcritical && flHealthPrev < 75)
	{

		// already took major damage, now it's critical...
		if (pev->health < 6)
			SetSuitUpdate("!HEV_HLTH3", FALSE, SUIT_NEXT_IN_10MIN);	// near death
		else if (pev->health < 20)
			SetSuitUpdate("!HEV_HLTH2", FALSE, SUIT_NEXT_IN_10MIN);	// health critical

		// give critical health warnings
		if (!RANDOM_LONG(0,3) && flHealthPrev < 50)
			SetSuitUpdate("!HEV_DMG7", FALSE, SUIT_NEXT_IN_5MIN); //seek medical attention
	}

	// if we're taking time based damage, warn about its continuing effects
	if (fTookDamage && (bitsDamageType & DMG_TIMEBASED) && flHealthPrev < 75)
		{
			if (flHealthPrev < 50)
			{
				if (!RANDOM_LONG(0,3))
					SetSuitUpdate("!HEV_DMG7", FALSE, SUIT_NEXT_IN_5MIN); //seek medical attention
			}
			else
				SetSuitUpdate("!HEV_HLTH1", FALSE, SUIT_NEXT_IN_10MIN);	// health dropping
		}

	return fTookDamage;
}

//=========================================================
// PackDeadPlayerItems - call this when a player dies to
// pack up the appropriate weapons and ammo items, and to
// destroy anything that shouldn't be packed.
//
// This is pretty brute force :(
//=========================================================
void CBasePlayer::PackDeadPlayerItems( void )
{
	int iWeaponRules;
	int iAmmoRules;
	int i;
	CBasePlayerWeapon *rgpPackWeapons[ 20 ];// 20 hardcoded for now. How to determine exactly how many weapons we have?
	int iPackAmmo[ MAX_AMMO_SLOTS + 1];
	int iPW = 0;// index into packweapons array
	int iPA = 0;// index into packammo array

	memset(rgpPackWeapons, NULL, sizeof(rgpPackWeapons) );
	memset(iPackAmmo, -1, sizeof(iPackAmmo) );

	// get the game rules
	iWeaponRules = g_pGameRules->DeadPlayerWeapons( this );
 	iAmmoRules = g_pGameRules->DeadPlayerAmmo( this );

	if ( iWeaponRules == GR_PLR_DROP_GUN_NO && iAmmoRules == GR_PLR_DROP_AMMO_NO )
	{
		// nothing to pack. Remove the weapons and return. Don't call create on the box!
		RemoveAllItems( TRUE );
		return;
	}

// go through all of the weapons and make a list of the ones to pack
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			// there's a weapon here. Should I pack it?
			CBasePlayerWeapon *pPlayerItem = m_rgpPlayerItems[ i ];

			while ( pPlayerItem )
			{
				switch( iWeaponRules )
				{
				case GR_PLR_DROP_GUN_ACTIVE:
					if ( m_pActiveItem && pPlayerItem == m_pActiveItem )
					{
						// this is the active item. Pack it.
						rgpPackWeapons[ iPW++ ] = (CBasePlayerWeapon *)pPlayerItem;
					}
					break;

				case GR_PLR_DROP_GUN_ALL:
					rgpPackWeapons[ iPW++ ] = (CBasePlayerWeapon *)pPlayerItem;
					break;

				default:
					break;
				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

// now go through ammo and make a list of which types to pack.
	if ( iAmmoRules != GR_PLR_DROP_AMMO_NO )
	{
		for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
		{
			if ( m_rgAmmo[ i ] > 0 )
			{
				// player has some ammo of this type.
				switch ( iAmmoRules )
				{
				case GR_PLR_DROP_AMMO_ALL:
					iPackAmmo[ iPA++ ] = i;
					break;

				case GR_PLR_DROP_AMMO_ACTIVE:
					if ( m_pActiveItem && i == m_pActiveItem->m_iPrimaryAmmoType )
					{
						// this is the primary ammo type for the active weapon
						iPackAmmo[ iPA++ ] = i;
					}
					else if ( m_pActiveItem && i == m_pActiveItem->m_iSecondaryAmmoType )
					{
						// this is the secondary ammo type for the active weapon
						iPackAmmo[ iPA++ ] = i;
					}
					break;

				default:
					break;
				}
			}
		}
	}

// create a box to pack the stuff into.
	CWeaponBox *pWeaponBox = (CWeaponBox *)CBaseEntity::Create( "item_weaponbox", pev->origin, pev->angles, edict() );

	pWeaponBox->pev->angles.x = 0;// don't let weaponbox tilt.
	pWeaponBox->pev->angles.z = 0;

	pWeaponBox->SetThink(& CWeaponBox::Kill );
	pWeaponBox->SetNextThink( 120 );

	// back these two lists up to their first elements
	iPA = 0;
	iPW = 0;

	// pack the ammo
	while( iPackAmmo[ iPA ] != -1 )
	{
		pWeaponBox->PackAmmo( CBasePlayerWeapon::AmmoInfoArray[ iPackAmmo[ iPA ] ].iszName, m_rgAmmo[iPackAmmo[iPA]] );
		iPA++;
	}

	// now pack all of the items in the lists
	while( rgpPackWeapons[ iPW ] )
	{
		// weapon unhooked from the player. Pack it into der box.
		pWeaponBox->PackWeapon( rgpPackWeapons[ iPW ] );

		iPW++;
	}

	pWeaponBox->pev->velocity = pev->velocity * 1.2;// weaponbox has player's velocity, then some.

	RemoveAllItems( TRUE );// now strip off everything that wasn't handled by the code above.
}

void CBasePlayer::RemoveAllItems( BOOL removeSuit )
{
	if (m_pActiveItem)
	{
		ResetAutoaim( );
		m_pActiveItem->Holster( );
		m_pActiveItem = NULL;
	}

	m_pLastItem = NULL;

	int i;
	CBasePlayerWeapon *pPendingItem;
	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		m_pActiveItem = m_rgpPlayerItems[i];
		while (m_pActiveItem)
		{
			pPendingItem = m_pActiveItem->m_pNext;
			m_pActiveItem->Drop( );
			m_pActiveItem = pPendingItem;
		}
		m_rgpPlayerItems[i] = NULL;
	}
	m_pActiveItem = NULL;

	pev->viewmodel	= 0;
	pev->weaponmodel	= 0;
	pev->weapons 	= 0;
	
	if ( removeSuit ) m_iHideHUD &= ~ITEM_SUIT;

	for ( i = 0; i < MAX_AMMO_SLOTS; i++)
		m_rgAmmo[i] = 0;

	UpdateClientData();

	// send Selected Weapon Message to our client
	MESSAGE_BEGIN( MSG_ONE, gmsg.CurWeapon, NULL, pev );
		WRITE_BYTE(0);
		WRITE_BYTE(0);
		WRITE_BYTE(0);
	MESSAGE_END();
}

/*
 * GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
 *
 * ENTITY_METHOD(PlayerDie)
 */
entvars_t *g_pevLastInflictor;  // Set in combat.cpp.  Used to pass the damage inflictor for death messages.
								// Better solution:  Add as parameter to all Killed() functions.

void CBasePlayer::Killed( entvars_t *pevAttacker, int iGib )
{
	CSound *pSound;

	// Holster weapon immediately, to allow it to cleanup
	if ( m_pActiveItem )
		m_pActiveItem->Holster( );

	m_pNextItem = NULL;

	g_pGameRules->PlayerKilled( this, pevAttacker, g_pevLastInflictor );

	if ( m_pTank != NULL )
	{
		m_pTank->Use( this, this, USE_OFF, 0 );
		m_pTank = NULL;
	}

	if( m_pMonitor != NULL )
	{
		m_pMonitor->Use( this, this, USE_SET, 1 );
		m_pMonitor = NULL;
	}
	// this client isn't going to be thinking for a while, so reset the sound until they respawn
	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( edict() ) );
	{
		if ( pSound )
		{
			pSound->Reset();
		}
	}

	SetAnimation( PLAYER_DIE );

	m_iRespawnFrames = 0;

	pev->modelindex = g_ulModelIndexPlayer;    // don't use eyes

	pev->deadflag		= DEAD_DYING;
	pev->movetype		= MOVETYPE_TOSS;
	ClearBits( pev->flags, FL_ONGROUND );
	if (pev->velocity.z < 10)
		pev->velocity.z += RANDOM_FLOAT(0,300);

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate(NULL, FALSE, 0);

	// send "health" update message to zero
	m_iClientHealth = 0;
	MESSAGE_BEGIN( MSG_ONE, gmsg.Health, NULL, pev );
		WRITE_BYTE( m_iClientHealth );
	MESSAGE_END();

	// Tell Ammo Hud that the player is dead
	MESSAGE_BEGIN( MSG_ONE, gmsg.CurWeapon, NULL, pev );
		WRITE_BYTE(0);
		WRITE_BYTE(0xFF);
		WRITE_BYTE(0xFF);
	MESSAGE_END();
         
	// reset FOV
	pev->fov = 90.0f;

	pViewEnt = 0;
	viewFlags = 0;
	viewNeedsUpdate = 1;

	// death fading         
          m_FadeColor = Vector( 128, 0, 0 );
	m_FadeAlpha = 240;			
	m_iFadeFlags = FFADE_OUT|FFADE_MODULATE|FFADE_STAYOUT;
	m_iFadeTime = 6;
	fadeNeedsUpdate = TRUE;
	
	if ( ( pev->health < -40 && iGib != GIB_NEVER ) || iGib == GIB_ALWAYS )
	{
		pev->solid			= SOLID_NOT;
		GibMonster();	// This clears pev->model
		pev->effects |= EF_NODRAW;
		return;
	}

	DeathSound();

	pev->angles.x = 0;
	pev->angles.z = 0;

	SetThink(PlayerDeathThink);
	SetNextThink( 0.1 );
}


// Set the activity based on an event or current state
void CBasePlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	int animDesired;
	float speed;
	char szAnim[64];

	speed = pev->velocity.Length2D();

	if( pev->flags & FL_FROZEN )
	{
		speed = 0;
		playerAnim = PLAYER_IDLE;
	}

	switch( playerAnim )
	{
	case PLAYER_JUMP:
		m_IdealActivity = ACT_HOP;
		break;

	case PLAYER_SUPERJUMP:
		m_IdealActivity = ACT_LEAP;
		break;

	case PLAYER_DIE:
		m_IdealActivity = ACT_DIESIMPLE;
		m_IdealActivity = GetDeathActivity( );
		break;

	case PLAYER_ATTACK1:
		switch( m_Activity )
		{
		case ACT_HOVER:
		case ACT_SWIM:
		case ACT_HOP:
		case ACT_LEAP:
		case ACT_DIESIMPLE:
			m_IdealActivity = m_Activity;
			break;
		default:
			m_IdealActivity = ACT_RANGE_ATTACK1;
			break;
		}
		break;
	case PLAYER_IDLE:
	case PLAYER_WALK:
		if ( !FBitSet( pev->flags, FL_ONGROUND ) && (m_Activity == ACT_HOP || m_Activity == ACT_LEAP) )	// Still jumping
		{
			m_IdealActivity = m_Activity;
		}
		else if ( pev->waterlevel > 1 && !FBitSet( pev->watertype, CONTENTS_FOG ))
		{
			if ( speed == 0 )
				m_IdealActivity = ACT_HOVER;
			else
				m_IdealActivity = ACT_SWIM;
		}
		else
		{
			m_IdealActivity = ACT_WALK;
		}
		break;
	}

	switch (m_IdealActivity)
	{
	case ACT_HOVER:
	case ACT_LEAP:
	case ACT_SWIM:
	case ACT_HOP:
	case ACT_DIESIMPLE:
	default:
		if ( m_Activity == m_IdealActivity)
			return;
		m_Activity = m_IdealActivity;

		animDesired = LookupActivity( m_Activity );
		// Already using the desired animation?
		if (pev->sequence == animDesired)
			return;

		pev->gaitsequence = 0;
		pev->sequence		= animDesired;
		pev->frame			= 0;
		ResetSequenceInfo( );
		return;

	case ACT_RANGE_ATTACK1:
		if ( FBitSet( pev->flags, FL_DUCKING ) )	// crouching
			strcpy( szAnim, "crouch_shoot_" );
		else
			strcpy( szAnim, "ref_shoot_" );
		strcat( szAnim, m_szAnimExtention );
		animDesired = LookupSequence( szAnim );
		if (animDesired == -1)
			animDesired = 0;

		if ( pev->sequence != animDesired || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		if (!m_fSequenceLoops)
		{
			pev->effects |= EF_NOINTERP;
		}

		m_Activity = m_IdealActivity;

		pev->sequence		= animDesired;
		ResetSequenceInfo( );
		break;

	case ACT_WALK:
		if (m_Activity != ACT_RANGE_ATTACK1 || m_fSequenceFinished)
		{
			if ( FBitSet( pev->flags, FL_DUCKING ) )	// crouching
				strcpy( szAnim, "crouch_aim_" );
			else
				strcpy( szAnim, "ref_aim_" );
			strcat( szAnim, m_szAnimExtention );
			animDesired = LookupSequence( szAnim );
			if (animDesired == -1)
				animDesired = 0;
			m_Activity = ACT_WALK;
		}
		else
		{
			animDesired = pev->sequence;
		}
	}

	if ( FBitSet( pev->flags, FL_DUCKING ) )
	{
		if ( speed == 0)
		{
			pev->gaitsequence	= LookupActivity( ACT_CROUCHIDLE );
			// pev->gaitsequence = LookupActivity( ACT_CROUCH );
		}
		else
		{
			pev->gaitsequence	= LookupActivity( ACT_CROUCH );
		}
	}
	else if ( speed > 220 )
	{
		pev->gaitsequence	= LookupActivity( ACT_RUN );
	}
	else if (speed > 0)
	{
		pev->gaitsequence	= LookupActivity( ACT_WALK );
	}
	else
	{
		// pev->gaitsequence	= LookupActivity( ACT_WALK );
		pev->gaitsequence	= LookupSequence( "deep_idle" );
	}


	// Already using the desired animation?
	if (pev->sequence == animDesired)
		return;

	//ALERT( at_console, "Set animation to %d\n", animDesired );
	// Reset to first frame of desired animation
	pev->sequence		= animDesired;
	pev->frame			= 0;
	ResetSequenceInfo( );
}


/*
===========
WaterMove
============
*/
#define AIRTIME	12		// lung full of air lasts this many seconds

void CBasePlayer::WaterMove()
{
	int air;

	if (pev->movetype == MOVETYPE_NOCLIP)
		return;

	if (pev->health < 0)
		return;

	// waterlevel 0 - not in water
	// waterlevel 1 - feet in water
	// waterlevel 2 - waist in water
	// waterlevel 3 - head in water

	if (pev->waterlevel != 3 || pev->watertype & MASK_WATER )
	{
		// not underwater

		// play 'up for air' sound
		if (m_fAirFinished < gpGlobals->time)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_wade1.wav", 1, ATTN_NORM);
		else if (m_fAirFinished < gpGlobals->time + 9)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_wade2.wav", 1, ATTN_NORM);

		m_fAirFinished = gpGlobals->time + AIRTIME;
		pev->dmg = 2;

		// if we took drowning damage, give it back slowly
		if (m_idrowndmg > m_idrownrestored)
		{
			// set drowning damage bit.  hack - dmg_drownrecover actually
			// makes the time based damage code 'give back' health over time.
			// make sure counter is cleared so we start count correctly.

			// NOTE: this actually causes the count to continue restarting
			// until all drowning damage is healed.

			m_bitsDamageType |= DMG_DROWNRECOVER;
			m_bitsDamageType &= ~DMG_DROWN;
			m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;
		}

	}
	else if (pev->watertype & MASK_WATER ) // FLYFIELD, FLYFIELD_GRAVITY & FOG aren't really water...
	{	// fully under water
		// stop restoring damage while underwater
		m_bitsDamageType &= ~DMG_DROWNRECOVER;
		m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;

		if (m_fAirFinished < gpGlobals->time)		// drown!
		{
			if (m_fPainFinished < gpGlobals->time)
			{
				// take drowning damage
				pev->dmg += 1;
				if (pev->dmg > 5)
					pev->dmg = 5;
				TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), pev->dmg, DMG_DROWN);
				m_fPainFinished = gpGlobals->time + 1;

				// track drowning damage, give it back when
				// player finally takes a breath

				m_idrowndmg += pev->dmg;
			}
		}
		else
		{
			m_bitsDamageType &= ~DMG_DROWN;
		}
	}

	if (!pev->waterlevel || pev->watertype & MASK_WATER )
	{
		if (FBitSet(pev->flags, FL_INWATER))
		{
			// play leave water sound
			switch (RANDOM_LONG(0,3))
			{
			case 0: EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM); break;
			case 1: EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade2.wav", 1, ATTN_NORM); break;
			case 2: EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade3.wav", 1, ATTN_NORM); break;
			case 3: EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade4.wav", 1, ATTN_NORM); break;
			}
			
			ClearBits(pev->flags, FL_INWATER);
		}
		return;
	}

	// make bubbles

	air = (int)(m_fAirFinished - gpGlobals->time);
	if (!RANDOM_LONG(0,0x1f) && RANDOM_LONG(0,AIRTIME-1) >= air)
	{
		switch (RANDOM_LONG(0,3))
			{
			case 0:	EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim1.wav", 0.8, ATTN_NORM); break;
			case 1:	EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim2.wav", 0.8, ATTN_NORM); break;
			case 2:	EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim3.wav", 0.8, ATTN_NORM); break;
			case 3:	EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim4.wav", 0.8, ATTN_NORM); break;
		}
	}

	if( FBitSet( pev->watertype, CONTENTS_LAVA ))		// do damage
	{
		if (pev->dmgtime < gpGlobals->time)
			TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), 10 * pev->waterlevel, DMG_BURN);
	}
	else if( FBitSet( pev->watertype, CONTENTS_SLIME ))		// do damage
	{
		pev->dmgtime = gpGlobals->time + 1;
		TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), 4 * pev->waterlevel, DMG_ACID);
	}

	if (!FBitSet( pev->flags, FL_INWATER ))
	{
		// player enter water sound
		if( FBitSet( pev->watertype, CONTENTS_WATER ))
		{
			switch (RANDOM_LONG(0,3))
			{
			case 0:	EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM); break;
			case 1:	EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade2.wav", 1, ATTN_NORM); break;
			case 2:	EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade3.wav", 1, ATTN_NORM); break;
			case 3:	EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade4.wav", 1, ATTN_NORM); break;
			}
		}

		SetBits( pev->flags, FL_INWATER );
		pev->dmgtime = 0;
	}

	if( !FBitSet( pev->flags, FL_WATERJUMP ))
		pev->velocity = pev->velocity - 0.8 * pev->waterlevel * gpGlobals->frametime * pev->velocity;
}

// TRUE if the player is attached to a ladder
BOOL CBasePlayer::IsOnLadder( void )
{
	return ( pev->movetype == MOVETYPE_FLY );
}

//
// check for a jump-out-of-water
//
void CBasePlayer::CheckWaterJump( )
{
	if ( IsOnLadder() )  // Don't water jump if on a ladders?  (This prevents the bug where 
		return;              //  you bounce off water when you are facing it on a ladder).

	Vector vecStart = pev->origin;
	vecStart.z += 8; 

	UTIL_MakeVectors(pev->angles);
	gpGlobals->v_forward.z = 0;
	gpGlobals->v_forward = gpGlobals->v_forward.Normalize();
	
	Vector vecEnd = vecStart + gpGlobals->v_forward * 24;
	
	TraceResult tr;
	UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, ENT(pev)/*pentIgnore*/, &tr);
	if (tr.flFraction < 1)		// solid at waist
	{
		vecStart.z += pev->maxs.z - 8;
		vecEnd = vecStart + gpGlobals->v_forward * 24;
		pev->movedir = tr.vecPlaneNormal * -50;
		UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, ENT(pev)/*pentIgnore*/, &tr);
		if (tr.flFraction == 1)		// open at eye level
		{
			SetBits( pev->flags, FL_WATERJUMP );
			pev->velocity.z = 225;
			pev->teleport_time = gpGlobals->time + 2;  // safety net
		}
	}
}


void CBasePlayer::PlayerDeathThink(void)
{
	float flForward;

	if (FBitSet(pev->flags, FL_ONGROUND))
	{
		flForward = pev->velocity.Length() - 20;
		if (flForward <= 0)
			pev->velocity = g_vecZero;
		else
			pev->velocity = flForward * pev->velocity.Normalize();
	}

	if ( HasWeapons() )
	{
		// we drop the guns here because weapons that have an area effect and can kill their user
		// will sometimes crash coming back from CBasePlayer::Killed() if they kill their owner because the
		// player class sometimes is freed. It's safer to manipulate the weapons once we know
		// we aren't calling into any of their code anymore through the player pointer.
		PackDeadPlayerItems();
	}

	//disable redeemer HUD
	MESSAGE_BEGIN( MSG_ONE, gmsg.WarHUD, NULL, pev );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	if (pev->modelindex && (!m_fSequenceFinished) && (pev->deadflag == DEAD_DYING))
	{
		StudioFrameAdvance( );

		m_iRespawnFrames++;				// Note, these aren't necessarily real "frames", so behavior is dependent on # of client movement commands
		if ( m_iRespawnFrames < 120 )   // Animations should be no longer than this
			return;
	}

	// once we're done animating our death and we're on the ground, we want to set movetype to None so our dead body won't do collisions and stuff anymore
	// this prevents a bug where the dead body would go to a player's head if he walked over it while the dead player was clicking their button to respawn
	if ( pev->movetype != MOVETYPE_NONE && FBitSet(pev->flags, FL_ONGROUND) )
		pev->movetype = MOVETYPE_NONE;

	if (pev->deadflag == DEAD_DYING)
		pev->deadflag = DEAD_DEAD;

	StopAnimation();

	pev->effects |= EF_NOINTERP;
	pev->framerate = 0.0;

	BOOL fAnyButtonDown = (pev->button & ~IN_SCORE );

	// wait for all buttons released
	if (pev->deadflag == DEAD_DEAD)
	{
		if (fAnyButtonDown)
			return;

		if ( g_pGameRules->FPlayerCanRespawn( this ) )
		{
			m_fDeadTime = gpGlobals->time;
			pev->deadflag = DEAD_RESPAWNABLE;
		}

		return;
	}

// if the player has been dead for one second longer than allowed by forcerespawn,
// forcerespawn isn't on. Send the player off to an intermission camera until they
// choose to respawn.
	if ( g_pGameRules->IsMultiplayer() && ( gpGlobals->time > (m_fDeadTime + 6) ) && !(m_afPhysicsFlags & PFLAG_OBSERVER) )
	{
		// go to dead camera.
		StartDeathCam();
	}

// wait for any button down,  or mp_forcerespawn is set and the respawn time is up
	if (!fAnyButtonDown
		&& !( g_pGameRules->IsMultiplayer() && CVAR_GET_FLOAT( "mp_forcerespawn" ) > 0 && (gpGlobals->time > (m_fDeadTime + 5))) )
		return;

	pev->button = 0;
	m_iRespawnFrames = 0;

	//ALERT(at_console, "Respawn\n");

	respawn(pev, !(m_afPhysicsFlags & PFLAG_OBSERVER) );// don't copy a corpse if we're in deathcam.
	DontThink();
}

//=========================================================
// StartDeathCam - find an intermission spot and send the
// player off into observer mode
//=========================================================
void CBasePlayer::StartDeathCam( void )
{
	CBaseEntity *pSpot, *pNewSpot;
	int iRand;

	if ( pev->view_ofs == g_vecZero )
	{
		// don't accept subsequent attempts to StartDeathCam()
		return;
	}

	pSpot = UTIL_FindEntityByClassname( NULL, "info_intermission");

	if ( pSpot )
	{
		// at least one intermission spot in the world.
		iRand = RANDOM_LONG( 0, 3 );

		while ( iRand > 0 )
		{
			pNewSpot = UTIL_FindEntityByTargetname( pSpot, "info_intermission");

			if ( pNewSpot )
			{
				pSpot = pNewSpot;
			}

			iRand--;
		}

		CopyToBodyQue( pev );
		StartObserver( pSpot->pev->origin, pSpot->pev->viewangles );
	}
	else
	{
		// no intermission spot. Push them up in the air, looking down at their corpse
		TraceResult tr;
		CopyToBodyQue( pev );
		UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, 128 ), ignore_monsters, edict(), &tr );
		StartObserver( tr.vecEndPos, UTIL_VecToAngles( tr.vecEndPos - pev->origin  ) );
		return;
	}
}

void CBasePlayer::StartObserver( Vector vecPosition, Vector vecViewAngle )
{
	m_afPhysicsFlags |= PFLAG_OBSERVER;

	pev->view_ofs = g_vecZero;
	pev->angles = pev->viewangles = vecViewAngle;
	pev->fixangle = TRUE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_NONE;
	pev->modelindex = 0;
	UTIL_SetOrigin( this, vecPosition );
}

//
// PlayerUse - handles USE keypress
//
#define	PLAYER_SEARCH_RADIUS	(float)64

void CBasePlayer::PlayerUse ( void )
{
	// Was use pressed or released?
	if ( ! ((pev->button | m_afButtonPressed | m_afButtonReleased) & IN_USE) )
		return;

	// Hit Use on a train?
	if ( m_afButtonPressed & IN_USE )
	{
		if ( m_pTank != NULL )
		{
			// Stop controlling the tank
			// TODO: Send HUD Update
			m_pTank->Use( this, this, USE_OFF, 0 );
			m_pTank = NULL;
			return;
		}
		else if (m_pMonitor != NULL )
		{
			m_pMonitor->Use( this, this, USE_SET, 1 );
			m_pMonitor = NULL;
			return;
		}
		else
		{
			if ( m_afPhysicsFlags & PFLAG_ONTRAIN )
			{
				m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
				m_iTrain = TRAIN_NEW|TRAIN_OFF;
				return;
			}
			else
			{	// Start controlling the train!
				CBaseEntity *pTrain = CBaseEntity::Instance( pev->groundentity );

				if ( pTrain && !(pev->button & IN_JUMP) && FBitSet(pev->flags, FL_ONGROUND) && (pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) && pTrain->OnControls(pev) )
				{
					m_afPhysicsFlags |= PFLAG_ONTRAIN;
					m_iTrain = TrainSpeed(pTrain->pev->speed, pTrain->pev->impulse);
					m_iTrain |= TRAIN_NEW;
					EMIT_SOUND( ENT(pev), CHAN_ITEM, "trains/train_use1.wav", 0.8, ATTN_NORM);
					return;
				}
			}
		}
	}

	CBaseEntity *pObject = NULL;
	CBaseEntity *pClosest = NULL;
	Vector		vecLOS;
	float flMaxDot = VIEW_FIELD_NARROW;
	float flDot;
	TraceResult tr;
	int caps;

	UTIL_MakeVectors ( pev->viewangles );// so we know which way we are facing

	//LRC- try to get an exact entity to use.
	// (is this causing "use-buttons-through-walls" problems? Surely not!)
	UTIL_TraceLine( pev->origin + pev->view_ofs, pev->origin + pev->view_ofs + (gpGlobals->v_forward * PLAYER_SEARCH_RADIUS), dont_ignore_monsters, ENT(pev), &tr );
	if (tr.pHit)
	{
		pObject = CBaseEntity::Instance(tr.pHit);
		if (!pObject || !(pObject->ObjectCaps() & (FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE)))
		{
			pObject = NULL;
		}
	}

	if (!pObject) //LRC- couldn't find a direct solid object to use, try the normal method
	{
		while ((pObject = UTIL_FindEntityInSphere( pObject, pev->origin, PLAYER_SEARCH_RADIUS )) != NULL)
		{
			caps = pObject->ObjectCaps();
			if (caps & (FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE) && !(caps & FCAP_ONLYDIRECT_USE)) //LRC - we can't see 'direct use' entities in this section
			{
				// !!!PERFORMANCE- should this check be done on a per case basis AFTER we've determined that
				// this object is actually usable? This dot is being done for every object within PLAYER_SEARCH_RADIUS
				// when player hits the use key. How many objects can be in that area, anyway? (sjb)
				vecLOS = (VecBModelOrigin( pObject->pev ) - (pev->origin + pev->view_ofs));

//				ALERT(at_console, "absmin %f %f %f, absmax %f %f %f, mins %f %f %f, maxs %f %f %f, size %f %f %f\n", pObject->pev->absmin.x, pObject->pev->absmin.y, pObject->pev->absmin.z, pObject->pev->absmax.x, pObject->pev->absmax.y, pObject->pev->absmax.z, pObject->pev->mins.x, pObject->pev->mins.y, pObject->pev->mins.z, pObject->pev->maxs.x, pObject->pev->maxs.y, pObject->pev->maxs.z, pObject->pev->size.x, pObject->pev->size.y, pObject->pev->size.z);//LRCTEMP
				// This essentially moves the origin of the target to the corner nearest the player to test to see
				// if it's "hull" is in the view cone
				vecLOS = UTIL_ClampVectorToBox( vecLOS, pObject->pev->size * 0.5 );

				flDot = DotProduct (vecLOS , gpGlobals->v_forward);
				if (flDot > flMaxDot || vecLOS == g_vecZero ) // LRC - if the player is standing inside this entity, it's also ok to use it.
				{// only if the item is in front of the user
					pClosest = pObject;
					flMaxDot = flDot;
//					ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
				}
//				ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
			}
		}
		pObject = pClosest;
	}

	// Found an object
	if (pObject )
	{
		//!!!UNDONE: traceline here to prevent USEing buttons through walls
		caps = pObject->ObjectCaps();

		if ( m_afButtonPressed & IN_USE )
			EMIT_SOUND( ENT(pev), CHAN_ITEM, "common/wpn_select.wav", 0.4, ATTN_NORM);

		if ( ( (pev->button & IN_USE) && (caps & FCAP_CONTINUOUS_USE) ) ||
			 ( (m_afButtonPressed & IN_USE) && (caps & (FCAP_IMPULSE_USE|FCAP_ONOFF_USE)) ) )
		{
			if ( caps & FCAP_CONTINUOUS_USE )
				m_afPhysicsFlags |= PFLAG_USING;

			pObject->Use( this, this, USE_SET, 1 );
		}
		// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
		// (actually, nothing uses on/off. They're either continuous - rechargers and momentary
		// buttons - or they're impulse - buttons, doors, tanks, trains, etc.) --LRC
		else if ( (m_afButtonReleased & IN_USE) && (pObject->ObjectCaps() & FCAP_ONOFF_USE) )	// BUGBUG This is an "off" use
		{
			pObject->Use( this, this, USE_SET, 0 );
		}
	}
	else
	{
		if ( m_afButtonPressed & IN_USE )
			EMIT_SOUND( ENT(pev), CHAN_ITEM, "common/wpn_denyselect.wav", 0.4, ATTN_NORM);
	}
}



void CBasePlayer :: Jump( void )
{
	float		flScale;
	Vector		vecWallCheckDir;// direction we're tracing a line to find a wall when walljumping
	Vector		vecAdjustedVelocity;
	Vector		vecSpot;
	TraceResult	tr;

	if( FBitSet( pev->flags, FL_WATERJUMP ))
		return;

	if( pev->waterlevel >= 2 && !FBitSet( pev->watertype, CONTENTS_FOG ))
	{
		if( pev->watertype & CONTENTS_WATER )
			flScale = 100;
		else if( pev->watertype & CONTENTS_SLIME )
			flScale =  80;
		else flScale = 50;
		pev->velocity.z = flScale;

		// play swiming sound
		if (m_flSwimTime < gpGlobals->time)
		{
			m_flSwimTime = gpGlobals->time + 1;
			switch (RANDOM_LONG(0,3))
			{ 
			case 0: EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM); break;
			case 1: EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade2.wav", 1, ATTN_NORM); break;
			case 2: EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade3.wav", 1, ATTN_NORM); break;
			case 3: EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade4.wav", 1, ATTN_NORM); break;
			}
		}
		return;
	}

	// jump velocity is sqrt( height * gravity * 2)

	// If this isn't the first frame pressing the jump button, break out.
	if ( !FBitSet( m_afButtonPressed, IN_JUMP ) )
		return;         // don't pogo stick

	if ( !(pev->flags & FL_ONGROUND) || !pev->groundentity )
	{
		return;
	}

	// many features in this function use v_forward, so makevectors now.
	UTIL_MakeVectors (pev->angles);

	ClearBits(pev->flags, FL_ONGROUND);// don't stairwalk

	SetAnimation( PLAYER_JUMP );

	// play step sound for current texture at full volume
	
	PlayStepSound(MapTextureTypeStepType(m_chTextureType), 1.0);

	if ( FBitSet( pev->flags, FL_DUCKING ) || FBitSet(m_afPhysicsFlags, PFLAG_DUCKING) )
	{
		if ( m_fLongJump && (pev->button & IN_DUCK) && ( gpGlobals->time - m_flDuckTime < 1 ) && pev->velocity.Length() > 50 )
		{
			Msg( "LongJump!\n" );
			
			// play longjump 'servo' sound
			if ( RANDOM_LONG(0,1) ) EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain2.wav", 1, ATTN_NORM);
			else EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain4.wav", 1, ATTN_NORM);

			pev->punchangle.x = -5;
			pev->velocity = gpGlobals->v_forward * (PLAYER_LONGJUMP_SPEED * 1.6);
			pev->velocity.z = sqrt( 2 * 800 * 56.0 ); // jump 56 units		
			SetAnimation( PLAYER_SUPERJUMP );
		}
		else
		{	// ducking jump
			pev->velocity.z = sqrt( 2 * 800 * 45.0 ); // jump 45 units
		}
          }
	else
	{
		pev->velocity.z = sqrt( 2 * 800 * 45.0 ); // jump 45 units
	}
}



// This is a glorious hack to find free space when you've crouched into some solid space
// Our crouching collisions do not work correctly for some reason and this is easier
// than fixing the problem :(
void FixPlayerCrouchStuck( edict_t *pPlayer )
{
	TraceResult trace;

	// Move up as many as 18 pixels if the player is stuck.
	for ( int i = 0; i < 18; i++ )
	{
		UTIL_TraceHull( pPlayer->v.origin, pPlayer->v.origin, dont_ignore_monsters, head_hull, pPlayer, &trace );
		if ( trace.fStartSolid )
			pPlayer->v.origin.z ++;
		else
			break;
	}
}

// It takes 0.4 seconds to duck
#define TIME_TO_DUCK	0.4

// This is a little more complicated now than it used to be, but I think for the better -
// The player's origin no longer moves when ducking.  If you start ducking while standing on ground,
// your view offset is non-linearly blended to the ducking position and then your hull is changed.
// In air, duck works just like it used to - you pick up your feet (more like tucking).
// PFLAG_DUCKING was added to track the state where the player is in the process of ducking, but hasn't
// reached the crouched position yet.
//
// A few of nice side effects of this are:
//	a) On ground status does not change when ducking (good on trains, etc)
//	b) origin stays constant
//	c) Superjump can begin while still in run animation (nice when looking at others in multiplayer)
//
void CBasePlayer::Duck( )
{
	float time, duckFraction;

	if (pev->button & IN_DUCK) 
	{
		if(( m_afButtonPressed & IN_DUCK ) && !(pev->flags & FL_DUCKING) )
		{
			m_flDuckTime = gpGlobals->time;
			SetBits(m_afPhysicsFlags,PFLAG_DUCKING);		// We are in the process of ducking
		}
		time = (gpGlobals->time - m_flDuckTime);

		// if we're not already done ducking
		if ( FBitSet( m_afPhysicsFlags, PFLAG_DUCKING ) )
		{
			// Finish ducking immediately if duck time is over or not on ground
			if ( time >= TIME_TO_DUCK || !(pev->flags & FL_ONGROUND) )
			{
				// HACKHACK - Fudge for collision bug - no time to fix this properly
				if ( pev->flags & FL_ONGROUND )
				{
					pev->origin = pev->origin - (VEC_DUCK_HULL_MIN - VEC_HULL_MIN);
					FixPlayerCrouchStuck( edict() );
				}


				UTIL_SetOrigin( this, pev->origin );

				UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
				pev->view_ofs = VEC_DUCK_VIEW;
				SetBits( pev->flags, FL_DUCKING );		// Hull is duck hull
				ClearBits( m_afPhysicsFlags, PFLAG_DUCKING );	// Done ducking (don't ease-in when the player hits the ground)
			}
			else
			{
				// Calc parametric time
				duckFraction = UTIL_SplineFraction( time, (1.0/TIME_TO_DUCK) );
				pev->view_ofs = ((VEC_DUCK_VIEW - (VEC_DUCK_HULL_MIN - VEC_HULL_MIN)) * duckFraction) + (VEC_VIEW * (1-duckFraction));
			}
		}
		SetAnimation( PLAYER_WALK );
	}
	else 
	{
		TraceResult trace;
		Vector newOrigin = pev->origin;

		if ( pev->flags & FL_ONGROUND )
			newOrigin = newOrigin + (VEC_DUCK_HULL_MIN - VEC_HULL_MIN);
		
		UTIL_TraceHull( newOrigin, newOrigin, dont_ignore_monsters, human_hull, ENT(pev), &trace );

		if ( !trace.fStartSolid )
		{
			ClearBits( pev->flags, FL_DUCKING );
			ClearBits( m_afPhysicsFlags, PFLAG_DUCKING );
			pev->view_ofs = VEC_VIEW;
			UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
			pev->origin = newOrigin;
		}
	}
}


//
// ID's player as such.
//
int  CBasePlayer::Classify ( void )
{
	return CLASS_PLAYER;
}


void CBasePlayer::AddPoints( int score, BOOL bAllowNegativeScore )
{
	// Positive score always adds
	if ( score < 0 )
	{
		if ( !bAllowNegativeScore )
		{
			if ( pev->frags < 0 )		// Can't go more negative
				return;

			if ( -score > pev->frags )	// Will this go negative?
			{
				score = -pev->frags;		// Sum will be 0
			}
		}
	}

	pev->frags += score;

	MESSAGE_BEGIN( MSG_ALL, gmsg.ScoreInfo );
		WRITE_BYTE( ENTINDEX(edict()) );
		WRITE_SHORT( pev->frags );
		WRITE_SHORT( m_iDeaths );
		WRITE_SHORT( 0 );
		WRITE_SHORT( g_pGameRules->GetTeamIndex( m_szTeamName ) + 1 );
	MESSAGE_END();
}


void CBasePlayer::AddPointsToTeam( int score, BOOL bAllowNegativeScore )
{
	int index = entindex();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		if ( pPlayer && i != index )
		{
			if ( g_pGameRules->PlayerRelationship( this, pPlayer ) == GR_TEAMMATE )
			{
				pPlayer->AddPoints( score, bAllowNegativeScore );
			}
		}
	}
}

//Player ID
void CBasePlayer::InitStatusBar()
{
	m_flStatusBarDisappearDelay = 0;
	m_SbarString1[0] = m_SbarString0[0] = 0;
}

void CBasePlayer::UpdateStatusBar()
{
	int newSBarState[ SBAR_END ];
	char sbuf0[ SBAR_STRING_SIZE ];
	char sbuf1[ SBAR_STRING_SIZE ];

	memset( newSBarState, 0, sizeof(newSBarState) );
	strcpy( sbuf0, m_SbarString0 );
	strcpy( sbuf1, m_SbarString1 );

	// Find an ID Target
	TraceResult tr;
	UTIL_MakeVectors( pev->viewangles + pev->punchangle );
	Vector vecSrc = EyePosition();
	Vector vecEnd = vecSrc + (gpGlobals->v_forward * MAX_ID_RANGE);
	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, edict(), &tr);

	if (tr.flFraction != 1.0)
	{
		if ( !FNullEnt( tr.pHit ) )
		{
			CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
			if (pEntity->Classify() == CLASS_PLAYER )
			{
				newSBarState[ SBAR_ID_TARGETNAME ] = ENTINDEX( pEntity->edict() );
				strcpy( sbuf1, "1 %p1\n2 Health: %i2%%\n3 Armor: %i3%%" );

				// allies and medics get to see the targets health
				if ( g_pGameRules->PlayerRelationship( this, pEntity ) == GR_TEAMMATE )
				{
					newSBarState[ SBAR_ID_TARGETHEALTH ] = 100 * (pEntity->pev->health / pEntity->pev->max_health);
					newSBarState[ SBAR_ID_TARGETARMOR ] = pEntity->pev->armorvalue; //No need to get it % based since 100 it's the max.
				}

				m_flStatusBarDisappearDelay = gpGlobals->time + 1.0;
			}
		}
		else if ( m_flStatusBarDisappearDelay > gpGlobals->time )
		{
			// hold the values for a short amount of time after viewing the object
			newSBarState[ SBAR_ID_TARGETNAME ] = m_izSBarState[ SBAR_ID_TARGETNAME ];
			newSBarState[ SBAR_ID_TARGETHEALTH ] = m_izSBarState[ SBAR_ID_TARGETHEALTH ];
			newSBarState[ SBAR_ID_TARGETARMOR ] = m_izSBarState[ SBAR_ID_TARGETARMOR ];
		}
	}

	BOOL bForceResend = FALSE;

	if ( strcmp( sbuf0, m_SbarString0 ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsg.StatusText, NULL, pev );
			WRITE_BYTE( 0 );
			WRITE_STRING( sbuf0 );
		MESSAGE_END();

		strcpy( m_SbarString0, sbuf0 );

		// make sure everything's resent
		bForceResend = TRUE;
	}

	if ( strcmp( sbuf1, m_SbarString1 ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsg.StatusText, NULL, pev );
			WRITE_BYTE( 1 );
			WRITE_STRING( sbuf1 );
		MESSAGE_END();

		strcpy( m_SbarString1, sbuf1 );

		// make sure everything's resent
		bForceResend = TRUE;
	}

	// Check values and send if they don't match
	for (int i = 1; i < SBAR_END; i++)
	{
		if ( newSBarState[i] != m_izSBarState[i] || bForceResend )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsg.StatusValue, NULL, pev );
				WRITE_BYTE( i );
				WRITE_SHORT( newSBarState[i] );
			MESSAGE_END();

			m_izSBarState[i] = newSBarState[i];
		}
	}
}






// play a footstep if it's time - this will eventually be frame-based. not time based.

#define STEP_CONCRETE	0		// default step sound
#define STEP_METAL		1		// metal floor
#define STEP_DIRT		2		// dirt, sand, rock
#define STEP_VENT		3		// ventillation duct
#define STEP_GRATE		4		// metal grating
#define STEP_TILE		5		// floor tiles
#define STEP_SLOSH		6		// shallow liquid puddle
#define STEP_WADE		7		// wading in liquid
#define STEP_LADDER		8		// climbing ladder

// Play correct step sound for material we're on or in

void CBasePlayer :: PlayStepSound(int step, float fvol)
{
	static int iSkipStep = 0;

	if ( !g_pGameRules->PlayFootstepSounds( this, fvol ) )
		return;

	// irand - 0,1 for right foot, 2,3 for left foot
	// used to alternate left and right foot
	int irand = RANDOM_LONG(0,1) + (m_iStepLeft * 2);

	m_iStepLeft = !m_iStepLeft;

	switch (step)
	{
	default:
	case STEP_CONCRETE:
		switch (irand)
		{
		// right foot
		case 0:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_step1.wav", fvol, ATTN_NORM);	break;
		case 1:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_step3.wav", fvol, ATTN_NORM);	break;
		// left foot
		case 2:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_step2.wav", fvol, ATTN_NORM);	break;
		case 3:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_step4.wav", fvol, ATTN_NORM);	break;
		}
		break;
	case STEP_METAL:
		switch(irand)
		{
		// right foot
		case 0:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_metal1.wav", fvol, ATTN_NORM);	break;
		case 1:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_metal3.wav", fvol, ATTN_NORM);	break;
		// left foot
		case 2:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_metal2.wav", fvol, ATTN_NORM);	break;
		case 3:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_metal4.wav", fvol, ATTN_NORM);	break;
		}
		break;
	case STEP_DIRT:
		switch(irand)
		{
		// right foot
		case 0:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_dirt1.wav", fvol, ATTN_NORM);	break;
		case 1:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_dirt3.wav", fvol, ATTN_NORM);	break;
		// left foot
		case 2:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_dirt2.wav", fvol, ATTN_NORM);	break;
		case 3:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_dirt4.wav", fvol, ATTN_NORM);	break;
		}
		break;
	case STEP_VENT:
		switch(irand)
		{
		// right foot
		case 0:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_duct1.wav", fvol, ATTN_NORM);	break;
		case 1:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_duct3.wav", fvol, ATTN_NORM);	break;
		// left foot
		case 2:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_duct2.wav", fvol, ATTN_NORM);	break;
		case 3:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_duct4.wav", fvol, ATTN_NORM);	break;
		}
		break;
	case STEP_GRATE:
		switch(irand)
		{
		// right foot
		case 0:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_grate1.wav", fvol, ATTN_NORM);	break;
		case 1:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_grate3.wav", fvol, ATTN_NORM);	break;
		// left foot
		case 2:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_grate2.wav", fvol, ATTN_NORM);	break;
		case 3:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_grate4.wav", fvol, ATTN_NORM);	break;
		}
		break;
	case STEP_TILE:
		if (!RANDOM_LONG(0,4))
			irand = 4;
		switch(irand)
		{
		// right foot
		case 0:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_tile1.wav", fvol, ATTN_NORM);	break;
		case 1:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_tile3.wav", fvol, ATTN_NORM);	break;
		// left foot
		case 2:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_tile2.wav", fvol, ATTN_NORM);	break;
		case 3:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_tile4.wav", fvol, ATTN_NORM);	break;
		case 4: EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_tile5.wav", fvol, ATTN_NORM);	break;
		}
		break;
	case STEP_SLOSH:
		switch(irand)
		{
		// right foot
		case 0:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_slosh1.wav", fvol, ATTN_NORM);	break;
		case 1:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_slosh3.wav", fvol, ATTN_NORM);	break;
		// left foot
		case 2:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_slosh2.wav", fvol, ATTN_NORM);	break;
		case 3:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_slosh4.wav", fvol, ATTN_NORM);	break;
		}
		break;
	case STEP_WADE:
		if ( iSkipStep == 0 )
		{
			iSkipStep++;
			break;
		}

		if ( iSkipStep++ == 3 )
		{
			iSkipStep = 0;
		}

		switch (irand)
		{
		// right foot
		case 0:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_wade1.wav", fvol, ATTN_NORM);	break;
		case 1:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_wade2.wav", fvol, ATTN_NORM);	break;
		// left foot
		case 2:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_wade3.wav", fvol, ATTN_NORM);	break;
		case 3:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_wade4.wav", fvol, ATTN_NORM);	break;
		}
		break;
	case STEP_LADDER:
		switch(irand)
		{
		// right foot
		case 0:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_ladder1.wav", fvol, ATTN_NORM);	break;
		case 1:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_ladder3.wav", fvol, ATTN_NORM);	break;
		// left foot
		case 2:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_ladder2.wav", fvol, ATTN_NORM);	break;
		case 3:	EMIT_SOUND( ENT(pev), CHAN_BODY, "player/pl_ladder4.wav", fvol, ATTN_NORM);	break;
		}
		break;
	}
}	

// Simple mapping from texture type character to step type

int MapTextureTypeStepType(char chTextureType)
{
	switch (chTextureType)
	{
	default:
	case CHAR_TEX_CONCRETE: return STEP_CONCRETE;	
	case CHAR_TEX_METAL: return STEP_METAL;	
	case CHAR_TEX_DIRT: return STEP_DIRT;	
	case CHAR_TEX_VENT: return STEP_VENT;	
	case CHAR_TEX_GRATE: return STEP_GRATE;	
	case CHAR_TEX_TILE: return STEP_TILE;
	case CHAR_TEX_SLOSH: return STEP_SLOSH;
	}
}

// Play left or right footstep based on material player is on or in

void CBasePlayer :: UpdateStepSound( void )
{
	int	fWalking;
	float fvol;
	char szbuffer[64];
	const char *pTextureName;
	Vector start, end;
	float rgfl1[3];
	float rgfl2[3];
	Vector knee;
	Vector feet;
	Vector center;
	float height;
	float speed;
	float velrun;
	float velwalk;
	float flduck;
	int	fLadder;
	int step;

	if (gpGlobals->time <= m_flTimeStepSound)
		return;

	if( pev->flags & FL_FROZEN )
		return;

	speed = pev->velocity.Length();

	// determine if we are on a ladder
	fLadder = IsOnLadder();

	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!	
	if( FBitSet( pev->flags, FL_DUCKING ) || fLadder )
	{
		velwalk = 60;		// These constants should be based on cl_movespeedkey * cl_forwardspeed somehow
		velrun = 80;		// UNDONE: Move walking to server
		flduck = 0.1;
	}
	else
	{
		velwalk = 120;
		velrun = 210;
		flduck = 0.0;
	}

	// ALERT (at_console, "vel: %f\n", vecVel.Length());
	
	// if we're on a ladder or on the ground, and we're moving fast enough,
	// play step sound.  Also, if m_flTimeStepSound is zero, get the new
	// sound right away - we just started moving in new level.

	if ((fLadder || FBitSet (pev->flags, FL_ONGROUND)) && pev->velocity != g_vecZero 
		&& (speed >= velwalk || !m_flTimeStepSound))
	{
		SetAnimation( PLAYER_WALK );
		
		fWalking = speed < velrun;		

		center = knee = feet = (pev->absmin + pev->absmax) * 0.5;
		height = pev->absmax.z - pev->absmin.z;

		knee.z = pev->absmin.z + height * 0.2;
		feet.z = pev->absmin.z;

		// find out what we're stepping in or on...
		if( fLadder )
		{
			step = STEP_LADDER;
			fvol = 0.35;
			m_flTimeStepSound = gpGlobals->time + 0.35;
		}
		else if( UTIL_PointContents( knee ) & MASK_WATER )
		{
			step = STEP_WADE;
			fvol = 0.65;
			m_flTimeStepSound = gpGlobals->time + 0.6;
		}
		else if( UTIL_PointContents( feet ) & MASK_WATER )
		{
			step = STEP_SLOSH;
			fvol = fWalking ? 0.2 : 0.5;
			m_flTimeStepSound = fWalking ? gpGlobals->time + 0.4 : gpGlobals->time + 0.3;		
		}
		else
		{
			// find texture under player, if different from current texture, 
			// get material type

			start = end = center;							// center point of player BB
			start.z = end.z = pev->absmin.z;				// copy zmin
			start.z += 4.0;									// extend start up
			end.z -= 24.0;									// extend end down
			
			start.CopyToArray(rgfl1);
			end.CopyToArray(rgfl2);

			pTextureName = TRACE_TEXTURE( ENT( pev->groundentity), rgfl1, rgfl2 );
			if ( pTextureName )
			{
				// strip leading '-0' or '{' or '!'
				if (*pTextureName == '-')
					pTextureName += 2;
				if (*pTextureName == '{' || *pTextureName == '!')
					pTextureName++;
				
				if (_strnicmp(pTextureName, m_szTextureName, CBTEXTURENAMEMAX-1))
				{
					// current texture is different from texture player is on...
					// set current texture
					strcpy(szbuffer, pTextureName);
					szbuffer[CBTEXTURENAMEMAX - 1] = 0;
					strcpy(m_szTextureName, szbuffer);
					
					// ALERT ( at_aiconsole, "texture: %s\n", m_szTextureName );

					// get texture type
					m_chTextureType = TEXTURETYPE_Find(m_szTextureName);	
				}
			}
			
			step = MapTextureTypeStepType(m_chTextureType);

			switch (m_chTextureType)
			{
			default:
			case CHAR_TEX_CONCRETE:						
				fvol = fWalking ? 0.2 : 0.5;
				m_flTimeStepSound = fWalking ? gpGlobals->time + 0.4 : gpGlobals->time + 0.3;
				break;

			case CHAR_TEX_METAL:	
				fvol = fWalking ? 0.2 : 0.5;
				m_flTimeStepSound = fWalking ? gpGlobals->time + 0.4 : gpGlobals->time + 0.3;
				break;

			case CHAR_TEX_DIRT:	
				fvol = fWalking ? 0.25 : 0.55;
				m_flTimeStepSound = fWalking ? gpGlobals->time + 0.4 : gpGlobals->time + 0.3;
				break;

			case CHAR_TEX_VENT:	
				fvol = fWalking ? 0.4 : 0.7;
				m_flTimeStepSound = fWalking ? gpGlobals->time + 0.4 : gpGlobals->time + 0.3;
				break;

			case CHAR_TEX_GRATE:
				fvol = fWalking ? 0.2 : 0.5;
				m_flTimeStepSound = fWalking ? gpGlobals->time + 0.4 : gpGlobals->time + 0.3;
				break;

			case CHAR_TEX_TILE:	
				fvol = fWalking ? 0.2 : 0.5;
				m_flTimeStepSound = fWalking ? gpGlobals->time + 0.4 : gpGlobals->time + 0.3;
				break;

			case CHAR_TEX_SLOSH:
				fvol = fWalking ? 0.2 : 0.5;
				m_flTimeStepSound = fWalking ? gpGlobals->time + 0.4 : gpGlobals->time + 0.3;
				break;
			}
		}
		
		m_flTimeStepSound += flduck; // slower step time if ducking

		// play the sound

		// 35% volume if ducking
		if ( pev->flags & FL_DUCKING )
			fvol *= 0.35;

		PlayStepSound(step, fvol);
	}
}


#define CLIMB_SHAKE_FREQUENCY	22	// how many frames in between screen shakes when climbing
#define	MAX_CLIMB_SPEED			200	// fastest vertical climbing speed possible
#define	CLIMB_SPEED_DEC			15	// climbing deceleration rate
#define	CLIMB_PUNCH_X			-7  // how far to 'punch' client X axis when climbing
#define CLIMB_PUNCH_Z			7	// how far to 'punch' client Z axis when climbing

void CBasePlayer::PreThink(void)
{
	int buttonsChanged = (m_afButtonLast ^ pev->button);	// These buttons have changed this frame

	// Debounced button codes for pressed/released
	// UNDONE: Do we need auto-repeat?
	m_afButtonPressed =  buttonsChanged & pev->button;		// The ones that changed and are now down are "pressed"
	m_afButtonReleased = buttonsChanged & (~pev->button);	// The ones that changed and aren't down are "released"

	g_pGameRules->PlayerThink( this );

	if ( g_fGameOver )
		return;         // intermission or finale

	UTIL_MakeVectors(pev->viewangles);             // is this still used?

	ItemPreFrame( );
	WaterMove();

	if ( g_pGameRules && g_pGameRules->FAllowFlashlight() )
		m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
	else
		m_iHideHUD |= HIDEHUD_FLASHLIGHT;

	// JOHN: checks if new client data (for HUD and view control) needs to be sent to the client
	UpdateClientData();

	CheckTimeBasedDamage();

	CheckSuitUpdate();

	if (pev->waterlevel == 2) 
		CheckWaterJump();
	else if ( pev->waterlevel == 0 )
	{
		pev->flags &= ~FL_WATERJUMP;	// Clear this if out of water
	}

	if (pev->deadflag >= DEAD_DYING)
	{
		PlayerDeathThink();
		return;
	}

	// So the correct flags get sent to client asap.
	//
	if ( m_afPhysicsFlags & PFLAG_ONTRAIN )
		pev->flags |= FL_ONTRAIN;
	else
		pev->flags &= ~FL_ONTRAIN;

	// Train speed control
	if ( m_afPhysicsFlags & PFLAG_ONTRAIN )
	{
		CBaseEntity *pTrain = CBaseEntity::Instance( pev->groundentity );
		float vel;

		if ( !pTrain )
		{
			TraceResult trainTrace;
			// Maybe this is on the other side of a level transition
			UTIL_TraceLine( pev->origin, pev->origin + Vector(0,0,-38), ignore_monsters, ENT(pev), &trainTrace );

			// HACKHACK - Just look for the func_tracktrain classname
			if ( trainTrace.flFraction != 1.0 && trainTrace.pHit )
			pTrain = CBaseEntity::Instance( trainTrace.pHit );


			if ( !pTrain || !(pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) || !pTrain->OnControls(pev) )
			{
				//ALERT( at_error, "In train mode with no train!\n" );
				m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
				m_iTrain = TRAIN_NEW|TRAIN_OFF;
				return;
			}
		}
		else if ( !FBitSet( pev->flags, FL_ONGROUND ) || FBitSet( pTrain->pev->spawnflags, 0x2 ) || (pev->button & (IN_MOVELEFT|IN_MOVERIGHT) ) )
		{
			// Turn off the train if you jump, strafe, or the train controls go dead
			m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
			m_iTrain = TRAIN_NEW|TRAIN_OFF;
			return;
		}

		pev->velocity = g_vecZero;
		vel = 0;
		if ( m_afButtonPressed & IN_FORWARD )
		{
			vel = 1;
			pTrain->Use( this, this, USE_SET, (float)vel );
		}
		else if ( m_afButtonPressed & IN_BACK )
		{
			vel = -1;
			pTrain->Use( this, this, USE_SET, (float)vel );
		}

		if (vel)
		{
			m_iTrain = TrainSpeed(pTrain->pev->speed, pTrain->pev->impulse);
			m_iTrain |= TRAIN_ACTIVE|TRAIN_NEW;
		}

	} else if (m_iTrain & TRAIN_ACTIVE)
		m_iTrain = TRAIN_NEW; // turn off train

	if( pev->button & IN_JUMP )
	{
		// If on a ladder, jump off the ladder
		// else Jump
		Jump();
	}

	// If trying to duck, already ducked, or in the process of ducking
	if ((pev->button & IN_DUCK) || FBitSet( pev->flags, FL_DUCKING) || (m_afPhysicsFlags & PFLAG_DUCKING) )
		Duck();

	// play a footstep if it's time - this will eventually be frame-based. not time based.
	
	UpdateStepSound();

	if ( !FBitSet ( pev->flags, FL_ONGROUND ) )
	{
		m_flFallVelocity = -pev->velocity.z;
	}

	// StudioFrameAdvance( );//!!!HACKHACK!!! Can't be hit by traceline when not animating?

	// Clear out ladder pointer
	m_hEnemy = NULL;

	if ( m_afPhysicsFlags & PFLAG_ONBARNACLE )
	{
		pev->velocity = g_vecZero;
	}
}
/* Time based Damage works as follows:
	1) There are several types of timebased damage:

		#define DMG_PARALYZE		(1 << 14)	// slows affected creature down
		#define DMG_NERVEGAS		(1 << 15)	// nerve toxins, very bad
		#define DMG_POISON			(1 << 16)	// blood poisioning
		#define DMG_RADIATION		(1 << 17)	// radiation exposure
		#define DMG_DROWNRECOVER	(1 << 18)	// drown recovery
		#define DMG_ACID			(1 << 19)	// toxic chemicals or acid burns
		#define DMG_SLOWBURN		(1 << 20)	// in an oven
		#define DMG_SLOWFREEZE		(1 << 21)	// in a subzero freezer
                    #define DMG_NUCLEAR		(1 << 24)
		
	2) A new hit inflicting tbd restarts the tbd counter - each monster has an 8bit counter,
		per damage type. The counter is decremented every second, so the maximum time
		an effect will last is 255/60 = 4.25 minutes.  Of course, staying within the radius
		of a damaging effect like fire, nervegas, radiation will continually reset the counter to max.

	3) Every second that a tbd counter is running, the player takes damage.  The damage
		is determined by the type of tdb.
			Paralyze		- 1/2 movement rate, 30 second duration.
			Nervegas		- 5 points per second, 16 second duration = 80 points max dose.
			Poison			- 2 points per second, 25 second duration = 50 points max dose.
			Radiation		- 1 point per second, 50 second duration = 50 points max dose.
			Drown			- 5 points per second, 2 second duration.
			Acid/Chemical	- 5 points per second, 10 second duration = 50 points max.
			Burn			- 10 points per second, 2 second duration.
			Freeze			- 3 points per second, 10 second duration = 30 points max.

	4) Certain actions or countermeasures counteract the damaging effects of tbds:

		Armor/Heater/Cooler - Chemical(acid),burn, freeze all do damage to armor power, then to body
							- recharged by suit recharger
		Air In Lungs		- drowning damage is done to air in lungs first, then to body
							- recharged by poking head out of water
							- 10 seconds if swiming fast
		Air In SCUBA		- drowning damage is done to air in tanks first, then to body
							- 2 minutes in tanks. Need new tank once empty.
		Radiation Syringe	- Each syringe full provides protection vs one radiation dosage
		Antitoxin Syringe	- Each syringe full provides protection vs one poisoning (nervegas or poison).
		Health kit			- Immediate stop to acid/chemical, fire or freeze damage.
		Radiation Shower	- Immediate stop to radiation damage, acid/chemical or fire damage.


*/

// If player is taking time based damage, continue doing damage to player -
// this simulates the effect of being poisoned, gassed, dosed with radiation etc -
// anything that continues to do damage even after the initial contact stops.
// Update all time based damage counters, and shut off any that are done.

// The m_bitsDamageType bit MUST be set if any damage is to be taken.
// This routine will detect the initial on value of the m_bitsDamageType
// and init the appropriate counter.  Only processes damage every second.

//#define PARALYZE_DURATION	30		// number of 2 second intervals to take damage
//#define PARALYZE_DAMAGE		0.0		// damage to take each 2 second interval

//#define NERVEGAS_DURATION	16
//#define NERVEGAS_DAMAGE		5.0

//#define POISON_DURATION		25
//#define POISON_DAMAGE		2.0

//#define RADIATION_DURATION	50
//#define RADIATION_DAMAGE	1.0

//#define ACID_DURATION		10
//#define ACID_DAMAGE			5.0

//#define SLOWBURN_DURATION	2
//#define SLOWBURN_DAMAGE		1.0

//#define SLOWFREEZE_DURATION	1.0
//#define SLOWFREEZE_DAMAGE	3.0

/* */


void CBasePlayer::CheckTimeBasedDamage()
{
	int i;
	BYTE bDuration = 0;

	static float gtbdPrev = 0.0;

	if (!(m_bitsDamageType & DMG_TIMEBASED))
		return;

	// only check for time based damage approx. every 2 seconds
	if (abs(gpGlobals->time - m_tbdPrev) < 2.0)
		return;

	m_tbdPrev = gpGlobals->time;

	for (i = 0; i < CDMG_TIMEBASED; i++)
	{
		// make sure bit is set for damage type
		if (m_bitsDamageType & (DMG_PARALYZE << i))
		{
			switch (i)
			{
			case itbd_Paralyze:
				// UNDONE - flag movement as half-speed
				bDuration = PARALYZE_DURATION;
				break;
			case itbd_NerveGas:
//				TakeDamage(pev, pev, NERVEGAS_DAMAGE, DMG_GENERIC);
				bDuration = NERVEGAS_DURATION;
				break;
			case itbd_Poison:
				TakeDamage(pev, pev, POISON_DAMAGE, DMG_GENERIC);
				bDuration = POISON_DURATION;
				break;
			case itbd_Radiation:
//				TakeDamage(pev, pev, RADIATION_DAMAGE, DMG_GENERIC);
				bDuration = RADIATION_DURATION;
				break;
			case itbd_DrownRecover:
				// NOTE: this hack is actually used to RESTORE health
				// after the player has been drowning and finally takes a breath
				if (m_idrowndmg > m_idrownrestored)
				{
					int idif = min(m_idrowndmg - m_idrownrestored, 10);

					TakeHealth(idif, DMG_GENERIC);
					m_idrownrestored += idif;
				}
				bDuration = 4;	// get up to 5*10 = 50 points back
				break;
			case itbd_Acid:
//				TakeDamage(pev, pev, ACID_DAMAGE, DMG_GENERIC);
				bDuration = ACID_DURATION;
				break;
			case itbd_SlowBurn:
//				TakeDamage(pev, pev, SLOWBURN_DAMAGE, DMG_GENERIC);
				bDuration = SLOWBURN_DURATION;
				break;
			case itbd_SlowFreeze:
//				TakeDamage(pev, pev, SLOWFREEZE_DAMAGE, DMG_GENERIC);
				bDuration = SLOWFREEZE_DURATION;
				break;
			default:
				bDuration = 0;
			}

			if (m_rgbTimeBasedDamage[i])
			{
				// decrement damage duration, detect when done.
				if (!m_rgbTimeBasedDamage[i] || --m_rgbTimeBasedDamage[i] == 0)
				{
					m_rgbTimeBasedDamage[i] = 0;
					// if we're done, clear damage bits
					m_bitsDamageType &= ~(DMG_PARALYZE << i);
				}
			}
			else
				// first time taking this damage type - init damage duration
				m_rgbTimeBasedDamage[i] = bDuration;
		}
	}
}

/*
THE POWER SUIT

The Suit provides 3 main functions: Protection, Notification and Augmentation.
Some functions are automatic, some require power.
The player gets the suit shortly after getting off the train in C1A0 and it stays
with him for the entire game.

Protection

	Heat/Cold
		When the player enters a hot/cold area, the heating/cooling indicator on the suit
		will come on and the battery will drain while the player stays in the area.
		After the battery is dead, the player starts to take damage.
		This feature is built into the suit and is automatically engaged.
	Radiation Syringe
		This will cause the player to be immune from the effects of radiation for N seconds. Single use item.
	Anti-Toxin Syringe
		This will cure the player from being poisoned. Single use item.
	Health
		Small (1st aid kits, food, etc.)
		Large (boxes on walls)
	Armor
		The armor works using energy to create a protective field that deflects a
		percentage of damage projectile and explosive attacks. After the armor has been deployed,
		it will attempt to recharge itself to full capacity with the energy reserves from the battery.
		It takes the armor N seconds to fully charge.

Notification (via the HUD)

x	Health
x	Ammo
x	Automatic Health Care
		Notifies the player when automatic healing has been engaged.
x	Geiger counter
		Classic Geiger counter sound and status bar at top of HUD
		alerts player to dangerous levels of radiation. This is not visible when radiation levels are normal.
x	Poison
	Armor
		Displays the current level of armor.

Augmentation

	Reanimation (w/adrenaline)
		Causes the player to come back to life after he has been dead for 3 seconds.
		Will not work if player was gibbed. Single use.
	Long Jump
		Used by hitting the ??? key(s). Caused the player to further than normal.
	SCUBA
		Used automatically after picked up and after player enters the water.
		Works for N seconds. Single use.

Things powered by the battery

	Armor
		Uses N watts for every M units of damage.
	Heat/Cool
		Uses N watts for every second in hot/cold area.
	Long Jump
		Uses N watts for every jump.
	Alien Cloak
		Uses N watts for each use. Each use lasts M seconds.
	Alien Shield
		Augments armor. Reduces Armor drain by one half

*/

// if in range of radiation source, ping geiger counter

#define GEIGERDELAY 0.25

void CBasePlayer :: UpdateGeigerCounter( void )
{
	BYTE range;

	// delay per update ie: don't flood net with these msgs
	if (gpGlobals->time < m_flgeigerDelay)
		return;

	m_flgeigerDelay = gpGlobals->time + GEIGERDELAY;

	// send range to radition source to client

	range = (BYTE) (m_flgeigerRange / 4);

	if (range != m_igeigerRangePrev)
	{
		m_igeigerRangePrev = range;

		MESSAGE_BEGIN( MSG_ONE, gmsg.GeigerRange, NULL, pev );
			WRITE_BYTE( range );
		MESSAGE_END();
	}

	// reset counter and semaphore
	if (!RANDOM_LONG(0,3))
		m_flgeigerRange = 1000;

}

/*
================
CheckSuitUpdate

Play suit update if it's time
================
*/

#define SUITUPDATETIME	3.5
#define SUITFIRSTUPDATETIME 0.1

void CBasePlayer::CheckSuitUpdate()
{
	int i;
	int isentence = 0;
	int isearch = m_iSuitPlayNext;

	// Ignore suit updates if no suit
	if (!(m_iHideHUD & ITEM_SUIT))
		return;

	// if in range of radiation source, ping geiger counter
	UpdateGeigerCounter();

	if ( g_pGameRules->IsMultiplayer() )
	{
		// don't bother updating HEV voice in multiplayer.
		return;
	}

	if ( gpGlobals->time >= m_flSuitUpdate && m_flSuitUpdate > 0)
	{
		// play a sentence off of the end of the queue
		for (i = 0; i < CSUITPLAYLIST; i++)
			{
			if (isentence = m_rgSuitPlayList[isearch])
				break;

			if (++isearch == CSUITPLAYLIST)
				isearch = 0;
			}

		if (isentence)
		{
			m_rgSuitPlayList[isearch] = 0;
			if (isentence > 0)
			{
				// play sentence number

				char sentence[CBSENTENCENAME_MAX+1];
				strcpy(sentence, "!");
				strcat(sentence, gszallsentencenames[isentence]);
				EMIT_SOUND_SUIT(ENT(pev), sentence);
			}
			else
			{
				// play sentence group
				EMIT_GROUPID_SUIT(ENT(pev), -isentence);
			}
		m_flSuitUpdate = gpGlobals->time + SUITUPDATETIME;
		}
		else
			// queue is empty, don't check
			m_flSuitUpdate = 0;
	}
}

// add sentence to suit playlist queue. if fgroup is true, then
// name is a sentence group (HEV_AA), otherwise name is a specific
// sentence name ie: !HEV_AA0.  If iNoRepeat is specified in
// seconds, then we won't repeat playback of this word or sentence
// for at least that number of seconds.

void CBasePlayer::SetSuitUpdate(char *name, int fgroup, int iNoRepeatTime)
{
	int i;
	int isentence;
	int iempty = -1;


	// Ignore suit updates if no suit
	if (!(m_iHideHUD & ITEM_SUIT))
		return;

	if ( g_pGameRules->IsMultiplayer() )
	{
		// due to static channel design, etc. We don't play HEV sounds in multiplayer right now.
		return;
	}

	// if name == NULL, then clear out the queue

	if (!name)
	{
		for (i = 0; i < CSUITPLAYLIST; i++)
			m_rgSuitPlayList[i] = 0;
		return;
	}
	// get sentence or group number
	if (!fgroup)
	{
		isentence = SENTENCEG_Lookup( name, NULL );
		if( isentence < 0 )
		{
			ALERT( at_console, "HEV couldn't find sentence %s\n", name );
			return;
		}
	}
	else
		// mark group number as negative
		isentence = -SENTENCEG_GetIndex(name);

	// check norepeat list - this list lets us cancel
	// the playback of words or sentences that have already
	// been played within a certain time.

	for (i = 0; i < CSUITNOREPEAT; i++)
	{
		if (isentence == m_rgiSuitNoRepeat[i])
			{
			// this sentence or group is already in
			// the norepeat list

			if (m_rgflSuitNoRepeatTime[i] < gpGlobals->time)
				{
				// norepeat time has expired, clear it out
				m_rgiSuitNoRepeat[i] = 0;
				m_rgflSuitNoRepeatTime[i] = 0.0;
				iempty = i;
				break;
				}
			else
				{
				// don't play, still marked as norepeat
				return;
				}
			}
		// keep track of empty slot
		if (!m_rgiSuitNoRepeat[i])
			iempty = i;
	}

	// sentence is not in norepeat list, save if norepeat time was given

	if (iNoRepeatTime)
	{
		if (iempty < 0)
			iempty = RANDOM_LONG(0, CSUITNOREPEAT-1); // pick random slot to take over
		m_rgiSuitNoRepeat[iempty] = isentence;
		m_rgflSuitNoRepeatTime[iempty] = iNoRepeatTime + gpGlobals->time;
	}

	// find empty spot in queue, or overwrite last spot

	m_rgSuitPlayList[m_iSuitPlayNext++] = isentence;
	if (m_iSuitPlayNext == CSUITPLAYLIST)
		m_iSuitPlayNext = 0;

	if (m_flSuitUpdate <= gpGlobals->time)
	{
		if (m_flSuitUpdate == 0)
			// play queue is empty, don't delay too long before playback
			m_flSuitUpdate = gpGlobals->time + SUITFIRSTUPDATETIME;
		else
			m_flSuitUpdate = gpGlobals->time + SUITUPDATETIME;
	}

}

/*
================
CheckPowerups

Check for turning off powerups

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
================
*/
	static void
CheckPowerups(entvars_t *pev)
{
	if (pev->health <= 0)
		return;

	pev->modelindex = g_ulModelIndexPlayer;    // don't use eyes
}


//=========================================================
// UpdatePlayerSound - updates the position of the player's
// reserved sound slot in the sound list.
//=========================================================
void CBasePlayer :: UpdatePlayerSound ( void )
{
	int iBodyVolume;
	int iVolume;
	CSound *pSound;

	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt :: ClientSoundIndex( edict() ) );

	if( !pSound )
	{
		ALERT ( at_console, "Client lost reserved sound!\n" );
		return;
	}

	pSound->m_iType = bits_SOUND_NONE;

	// now calculate the best target volume for the sound. If the player's weapon
	// is louder than his body/movement, use the weapon volume, else, use the body volume.

	if ( FBitSet ( pev->flags, FL_ONGROUND ) )
	{
		iBodyVolume = pev->velocity.Length();

		// clamp the noise that can be made by the body, in case a push trigger,
		// weapon recoil, or anything shoves the player abnormally fast.
		if ( iBodyVolume > 512 )
		{
			iBodyVolume = 512;
		}
	}
	else
	{
		iBodyVolume = 0;
	}

	if ( pev->button & IN_JUMP )
	{
		iBodyVolume += 100;
	}

// convert player move speed and actions into sound audible by monsters.
	if ( m_iWeaponVolume > iBodyVolume )
	{
		m_iTargetVolume = m_iWeaponVolume;

		// OR in the bits for COMBAT sound if the weapon is being louder than the player.
		pSound->m_iType |= bits_SOUND_COMBAT;
	}
	else
	{
		m_iTargetVolume = iBodyVolume;
	}

	// decay weapon volume over time so bits_SOUND_COMBAT stays set for a while
	m_iWeaponVolume -= 250 * gpGlobals->frametime;
	if ( m_iWeaponVolume < 0 )
	{
		iVolume = 0;
	}


	// if target volume is greater than the player sound's current volume, we paste the new volume in
	// immediately. If target is less than the current volume, current volume is not set immediately to the
	// lower volume, rather works itself towards target volume over time. This gives monsters a much better chance
	// to hear a sound, especially if they don't listen every frame.
	iVolume = pSound->m_iVolume;

	if ( m_iTargetVolume > iVolume )
	{
		iVolume = m_iTargetVolume;
	}
	else if ( iVolume > m_iTargetVolume )
	{
		iVolume -= 250 * gpGlobals->frametime;

		if ( iVolume < m_iTargetVolume )
		{
			iVolume = 0;
		}
	}

	if ( m_fNoPlayerSound )
	{
		// debugging flag, lets players move around and shoot without monsters hearing.
		iVolume = 0;
	}

	if ( gpGlobals->time > m_flStopExtraSoundTime )
	{
		// since the extra sound that a weapon emits only lasts for one client frame, we keep that sound around for a server frame or two
		// after actual emission to make sure it gets heard.
		m_iExtraSoundTypes = 0;
	}

	if ( pSound )
	{
		pSound->m_vecOrigin = pev->origin;
		pSound->m_iType |= ( bits_SOUND_PLAYER | m_iExtraSoundTypes );
		pSound->m_iVolume = iVolume;
	}

	// keep track of virtual muzzle flash
	m_iWeaponFlash -= 256 * gpGlobals->frametime;
	if (m_iWeaponFlash < 0)
		m_iWeaponFlash = 0;

	//UTIL_MakeVectors ( pev->angles );
	//gpGlobals->v_forward.z = 0;

	// Below are a couple of useful little bits that make it easier to determine just how much noise the
	// player is making.
	// UTIL_ParticleEffect ( pev->origin + gpGlobals->v_forward * iVolume, g_vecZero, 255, 25 );
	//ALERT ( at_console, "%d/%d\n", iVolume, m_iTargetVolume );
}


void CBasePlayer::PostThink()
{
	if ( g_fGameOver ) return; // intermission or finale
	if ( !IsAlive () ) return;

	if ( m_pTank != NULL )
	{
		if ( m_pTank->OnControls( pev ) && !pev->weaponmodel );
		else
		{	// they've moved off the platform
			m_pTank->Use( this, this, USE_OFF, 0 );
			m_pTank = NULL;
		}
	}

	if ( m_pMonitor != NULL )
	{
		if ( !m_pMonitor->OnControls( pev ) )
		{	// they've moved off the platform
			m_pMonitor->Use( this, this, USE_SET, 1 );
			m_pMonitor = NULL;
		}
	}

	// do weapon stuff
	ItemPostFrame( );

// check to see if player landed hard enough to make a sound
// falling farther than half of the maximum safe distance, but not as far a max safe distance will
// play a bootscrape sound, and no damage will be inflicted. Fallling a distance shorter than half
// of maximum safe distance will make no sound. Falling farther than max safe distance will play a
// fallpain sound, and damage will be inflicted based on how far the player fell

	if ( (FBitSet(pev->flags, FL_ONGROUND)) && (pev->health > 0) && m_flFallVelocity >= PLAYER_FALL_PUNCH_THRESHHOLD )
	{
		float fvol = 0.5;

		// ALERT ( at_console, "%f\n", m_flFallVelocity );

		if( FBitSet( pev->watertype, CONTENTS_WATER ))
		{
			// Did he hit the world or a non-moving entity?
			// BUG - this happens all the time in water, especially when
			// BUG - water has current force
			// if ( !pev->groundentity || VARS(pev->groundentity)->velocity.z == 0 )
				// EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM);
		}
		else if ( m_flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED )
		{
			// after this point, we start doing damage
			switch ( RANDOM_LONG(0,1) )
			{
			case 0:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_fallpain2.wav", 1, ATTN_NORM);
			case 1:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_fallpain3.wav", 1, ATTN_NORM);
			}
			
			float flFallDamage = g_pGameRules->FlPlayerFallDamage( this );

			if ( flFallDamage > pev->health )
			{
				// splat
				// NOTE: play on item channel because we play footstep landing on body channel
				EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/bodysplat.wav", 1, ATTN_NORM);
			}

			if ( flFallDamage > 0 )
			{
				TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), flFallDamage, DMG_FALL );
			}

			fvol = 1.0;
		}
		else if ( m_flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED / 2 )
		{
			// EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_jumpland2.wav", 1, ATTN_NORM);
			fvol = 0.85;
		}
		else if ( m_flFallVelocity < PLAYER_MIN_BOUNCE_SPEED )
		{
			fvol = 0;
		}

		if ( fvol > 0.0 )
		{
			// get current texture under player right away
			m_flTimeStepSound = 0;
			UpdateStepSound();
			
			// play step sound for current texture
			PlayStepSound(MapTextureTypeStepType(m_chTextureType), fvol);

		// knock the screen around a little bit, temporary effect
			pev->punchangle.x = m_flFallVelocity * 0.018;	// punch x axis

			if ( pev->punchangle.x > 8 )
			{
				pev->punchangle.x = 8;
			}

			if ( RANDOM_LONG(0,1) )
			{
				pev->punchangle.z = m_flFallVelocity * 0.009;
			}
		
		}

		if ( IsAlive() )
		{
			SetAnimation( PLAYER_WALK );
		}
    }

	if (FBitSet(pev->flags, FL_ONGROUND))
	{
		if (m_flFallVelocity > 64 && !g_pGameRules->IsMultiplayer())
		{
			CSoundEnt::InsertSound ( bits_SOUND_PLAYER, pev->origin, m_flFallVelocity, 0.2 );
			// ALERT( at_console, "fall %f\n", m_flFallVelocity );
		}
		m_flFallVelocity = 0;
	}

	// select the proper animation for the player character
	if ( IsAlive() )
	{
		if (!pev->velocity.x && !pev->velocity.y)
			SetAnimation( PLAYER_IDLE );
		else if ((pev->velocity.x || pev->velocity.y) && (FBitSet(pev->flags, FL_ONGROUND)))
			SetAnimation( PLAYER_WALK );
		else if (pev->waterlevel > 1)
			SetAnimation( PLAYER_WALK );
	}

	StudioFrameAdvance( );
	CheckPowerups(pev);

	UpdatePlayerSound();

	// Track button info so we can detect 'pressed' and 'released' buttons next frame
	m_afButtonLast = pev->button;
}


// checks if the spot is clear of players
BOOL IsSpawnPointValid( CBaseEntity *pPlayer, CBaseEntity *pSpot )
{
	CBaseEntity *ent = NULL;

	if ( pSpot->GetState( pPlayer ) != STATE_ON )
	{
		return FALSE;
	}

	while ( (ent = UTIL_FindEntityInSphere( ent, pSpot->pev->origin, 128 )) != NULL )
	{
		// if ent is a client, don't spawn on 'em
		if ( ent->IsPlayer() && ent != pPlayer )
			return FALSE;
	}

	return TRUE;
}


DLL_GLOBAL CBaseEntity	*g_pLastSpawn;

/*
============
EntSelectSpawnPoint

Returns the entity to spawn at

USES AND SETS GLOBAL g_pLastSpawn
============
*/
edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer )
{
	CBaseEntity *pSpot;
	edict_t		*player;

	player = pPlayer->edict();

// choose a info_player_deathmatch point
	if (g_pGameRules->IsCoOp())
	{
		pSpot = UTIL_FindEntityByClassname( g_pLastSpawn, "info_player_coop");
		if ( !FNullEnt(pSpot) )
			goto ReturnSpot;
		pSpot = UTIL_FindEntityByClassname( g_pLastSpawn, "info_player_start");
		if ( !FNullEnt(pSpot) )
			goto ReturnSpot;
	}
	else if ( g_pGameRules->IsDeathmatch() )
	{
		pSpot = g_pLastSpawn;
		// Randomize the start spot
		for ( int i = RANDOM_LONG(1,5); i > 0; i-- )
			pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );
		if ( FNullEnt( pSpot ) )  // skip over the null point
			pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );

		CBaseEntity *pFirstSpot = pSpot;

		do
		{
			if ( pSpot )
			{
				// check if pSpot is valid
				if ( IsSpawnPointValid( pPlayer, pSpot ) )
				{
					if ( pSpot->pev->origin == Vector( 0, 0, 0 ) )
					{
						pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );
						continue;
					}

					// if so, go to pSpot
					goto ReturnSpot;
				}
			}
			// increment pSpot
			pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );
		} while ( pSpot != pFirstSpot ); // loop if we're not back to the start

		// we haven't found a place to spawn yet,  so kill any guy at the first spawn point and spawn there
		if ( !FNullEnt( pSpot ) )
		{
			CBaseEntity *ent = NULL;
			while ( (ent = UTIL_FindEntityInSphere( ent, pSpot->pev->origin, 128 )) != NULL )
			{
				// if ent is a client, kill em (unless they are ourselves)
				if ( ent->IsPlayer() && !(ent->edict() == player) )
					ent->TakeDamage( VARS(INDEXENT(0)), VARS(INDEXENT(0)), 300, DMG_GENERIC );
			}
			goto ReturnSpot;
		}
	}

	// If startspot is set, (re)spawn there.
	if ( FStringNull( gpGlobals->startspot ) || !strlen(STRING(gpGlobals->startspot)))
	{
		pSpot = UTIL_FindEntityByClassname(NULL, "info_player_start");
		if ( !FNullEnt(pSpot) )
			goto ReturnSpot;
	}
	else
	{
		pSpot = UTIL_FindEntityByTargetname( NULL, STRING(gpGlobals->startspot) );
		if ( !FNullEnt(pSpot) )
			goto ReturnSpot;
	}

ReturnSpot:
	if ( FNullEnt( pSpot ) )
	{
		ALERT(at_error, "PutClientInServer: no info_player_start on level");
		return INDEXENT(0);
	}

	g_pLastSpawn = pSpot;
	return pSpot->edict();
}

void CBasePlayer::Spawn( void )
{
//	ALERT(at_console, "PLAYER spawns at time %f\n", gpGlobals->time);

	pev->classname	= MAKE_STRING( "player" );
	pev->health	= 100;
	pev->armorvalue	= 0;
	pev->takedamage	= DAMAGE_AIM;
	pev->solid	= SOLID_BBOX;
	pev->movetype	= MOVETYPE_WALK;
	pev->max_health	= pev->health;
	pev->flags	= FL_CLIENT;
	m_fAirFinished	= gpGlobals->time + 12;
	pev->dmg		= 2;				// initial water damage
	pev->effects	= 0;
	pev->deadflag	= DEAD_NO;
	pev->dmg_take	= 0;
	pev->dmg_save	= 0;
	pev->friction	= 1.0;
	pev->gravity	= 1.0;
	pev->renderfx	= 0;
	pev->rendercolor	= g_vecZero;
	pev->mass		= 90;	// lbs
	m_bitsHUDDamage	= -1;
	m_bitsDamageType	= 0;
	m_afPhysicsFlags	= 0;
	m_fLongJump	= 1;	// no longjump module.
	Rain_dripsPerSecond = 0;
	Rain_windX = 0;
	Rain_windY = 0;
	Rain_randX = 0;
	Rain_randY = 0;
	Rain_ideal_dripsPerSecond = 0;
	Rain_ideal_windX = 0;
	Rain_ideal_windY = 0;
	Rain_ideal_randX = 0;
	Rain_ideal_randY = 0;
	Rain_endFade = 0;
	Rain_nextFadeUpdate = 0;
	GiveOnlyAmmo = FALSE;

	pev->fov = 90.0f;	// init field of view.
	//m_iAcessLevel = 2;

	m_flNextDecalTime = 0;	// let this player decal as soon as he spawns.

	m_flgeigerDelay = gpGlobals->time + 2.0;	// wait a few seconds until user-defined message registrations
												// are recieved by all clients

	m_flTimeStepSound	= 0;
	m_iStepLeft = 0;
	m_flFieldOfView = 0.5;// some monsters use this to determine whether or not the player is looking at them.

	m_bloodColor	= BLOOD_COLOR_RED;
	m_flNextAttack	= UTIL_WeaponTimeBase();
	StartSneaking();

	m_iFlashBattery = 99;
	m_flFlashLightTime = 1; // force first message

// dont let uninitialized value here hurt the player
	m_flFallVelocity = 0;

	g_pGameRules->SetDefaultPlayerTeam( this );
	g_pGameRules->GetPlayerSpawnSpot( this );

	UTIL_SetModel(ENT(pev), "models/player.mdl");
	g_ulModelIndexPlayer = pev->modelindex;
	pev->sequence = LookupActivity( ACT_IDLE );

	if( FBitSet( pev->flags, FL_DUCKING ))
		UTIL_SetSize( pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	else UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );

    	pev->view_ofs = VEC_VIEW;
	pViewEnt = 0;
	viewFlags = 0;
	Precache();
	m_HackedGunPos		= Vector( 0, 32, 0 );

	if( m_iPlayerSound == SOUNDLIST_EMPTY )
	{
		ALERT ( at_console, "Couldn't alloc player sound slot!\n" );
	}

	m_fNoPlayerSound = FALSE;// normal sound behavior.

	m_pLastItem = NULL;
	m_fInitHUD = TRUE;
	m_iClientHideHUD = -1;  // force this to be recalculated
	m_fWeapon = FALSE;
	m_pClientActiveItem = NULL;
	m_iClientBattery = -1;

	// reset all ammo values to 0
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		m_rgAmmo[i] = 0;
		m_rgAmmoLast[i] = 0;  // client ammo values also have to be reset  (the death hud clear messages does on the client side)
	}

	m_lastx = m_lasty = 0;

	m_flNextChatTime = gpGlobals->time;

	g_pGameRules->PlayerSpawn( this );
}


void CBasePlayer :: Precache( void )
{
	// in the event that the player JUST spawned, and the level node graph
	// was loaded, fix all of the node graph pointers before the game starts.

	// SOUNDS / MODELS ARE PRECACHED in ClientPrecache() (game specific)
	// because they need to precache before any clients have connected

	// init geiger counter vars during spawn and each time
	// we cross a level transition

	m_flgeigerRange = 1000;
	m_igeigerRangePrev = 1000;

	m_bitsDamageType = 0;
	m_bitsHUDDamage = -1;
	GiveOnlyAmmo = FALSE;

	m_iClientBattery = -1;

	m_iTrain = TRAIN_NEW;

	// Make sure any necessary user messages have been registered
	LinkUserMessages();
	viewNeedsUpdate = 1;
	hearNeedsUpdate = 1;
	fogNeedsUpdate = 1;
	fadeNeedsUpdate = 1;
	m_iClientWarHUD = -1;

	m_iUpdateTime = 5;  // won't update for 1/2 a second

	if ( gInitHUD ) m_fInitHUD = TRUE;
	rainNeedsUpdate = 1;

	//clear fade effects
	if(IsMultiplayer())
	{
		m_FadeColor = Vector(255, 255, 255);
		m_FadeAlpha = 0;			
		m_iFadeFlags = 0;
		m_iFadeTime = 0;
		m_iFadeHold = 0;
		fadeNeedsUpdate = TRUE;
	}
}


int CBasePlayer::Save( CSave &save )
{
	if ( !CBaseMonster::Save(save) )
		return 0;

	return save.WriteFields( "cPLAYER", "PLAYER", this, m_playerSaveData, ARRAYSIZE(m_playerSaveData) );
}


//
// Marks everything as new so the player will resend this to the hud.
//
void CBasePlayer::RenewItems(void)
{

}

int CBasePlayer::Restore( CRestore &restore )
{
	if ( !CBaseMonster::Restore(restore) )
		return 0;

	int status = restore.ReadFields( "PLAYER", this, m_playerSaveData, ARRAYSIZE(m_playerSaveData) );

	SAVERESTOREDATA *pSaveData = (SAVERESTOREDATA *)gpGlobals->pSaveData;
	// landmark isn't present.
	if( !pSaveData->fUseLandmark )
	{
		ALERT( at_error, "No Landmark:%s\n", pSaveData->szLandmarkName );

		// default to normal spawn
		edict_t* pentSpawnSpot = EntSelectSpawnPoint( this );
		pev->origin = VARS(pentSpawnSpot)->origin + Vector(0,0,1);
		pev->angles = VARS(pentSpawnSpot)->angles;
	}
	pev->viewangles.z = 0;	// Clear out roll
	pev->angles = pev->viewangles;

	pev->fixangle = TRUE;           // turn this way immediately

// Copied from spawn() for now
	m_bloodColor	= BLOOD_COLOR_RED;

    g_ulModelIndexPlayer = pev->modelindex;

	if( FBitSet( pev->flags, FL_DUCKING ))
	{
		// Use the crouch HACK
		FixPlayerCrouchStuck( edict() );
		UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
	}
	else
	{
		UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
	}

	RenewItems();

	return status;
}



void CBasePlayer::SelectNextItem( int iItem )
{
	CBasePlayerWeapon *pItem;

	pItem = m_rgpPlayerItems[ iItem ];

	if (!pItem)
		return;

	if (pItem == m_pActiveItem)
	{
		// select the next one in the chain
		pItem = m_pActiveItem->m_pNext;
		if (! pItem)
		{
			return;
		}

		CBasePlayerWeapon *pLast;
		pLast = pItem;
		while (pLast->m_pNext)
			pLast = pLast->m_pNext;

		// relink chain
		pLast->m_pNext = m_pActiveItem;
		m_pActiveItem->m_pNext = NULL;
		m_rgpPlayerItems[ iItem ] = pItem;
	}

	ResetAutoaim( );

	if (m_pActiveItem) m_pActiveItem->Holster( );
	QueueItem(pItem);
	if (m_pActiveItem) m_pActiveItem->Deploy( );
}

void CBasePlayer::QueueItem(CBasePlayerWeapon *pItem)
{
	if(!m_pActiveItem)// no active weapon
	{
		m_pActiveItem = pItem;
		return;// just set this item as active
	}
    	else
	{
		m_pLastItem = m_pActiveItem;
		m_pActiveItem = NULL;// clear current
	}
	m_pNextItem = pItem;// add item to queue
}

void CBasePlayer::SelectItem(const char *pstr)
{
	if (!pstr) return;

	CBasePlayerWeapon *pItem = NULL;
	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			pItem = m_rgpPlayerItems[i];

			while (pItem)
			{
				if(FStrEq( STRING( pItem->pev->netname), pstr ))
					break;
				pItem = pItem->m_pNext;
			}
		}

		if (pItem)
			break;
	}

	if (!pItem)
		return;


	if (pItem == m_pActiveItem)
		return;

	ResetAutoaim( );

	if (m_pActiveItem) m_pActiveItem->Holster( );
	QueueItem(pItem);
	if (m_pActiveItem) m_pActiveItem->Deploy( );
}


void CBasePlayer::SelectLastItem(void)
{
	if (!m_pLastItem)
	{
		return;
	}

	if ( m_pActiveItem && !m_pActiveItem->CanHolster() )
	{
		return;
	}

	ResetAutoaim( );

	// FIX, this needs to queue them up and delay
	if (m_pActiveItem) m_pActiveItem->Holster( );
	QueueItem(m_pLastItem);
	if (m_pActiveItem) m_pActiveItem->Deploy( );
}

//==============================================
// HasWeapons - do I have any weapons at all?
//==============================================
BOOL CBasePlayer::HasWeapons( void )
{
	int i;

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			return TRUE;
		}
	}

	return FALSE;
}

void CBasePlayer::SelectPrevItem( int iItem )
{
}


const char *CBasePlayer::TeamID( void )
{
	if ( pev == NULL )		// Not fully connected yet
		return "";

	// return their team name
	return m_szTeamName;
}


//==============================================
// !!!UNDONE:ultra temporary SprayCan entity to apply
// decal frame at a time. For PreAlpha CD
//==============================================
class CSprayCan : public CBaseEntity
{
public:
	void	Spawn ( entvars_t *pevOwner );
	void	Think( void );

	virtual int	ObjectCaps( void ) { return FCAP_DONT_SAVE; }
};

void CSprayCan::Spawn ( entvars_t *pevOwner )
{
	pev->origin = pevOwner->origin + Vector ( 0 , 0 , 32 );
	pev->angles = pevOwner->viewangles;
	pev->owner = ENT(pevOwner);
	pev->frame = 0;

	SetNextThink( 0.1 );
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/sprayer.wav", 1, ATTN_NORM);
}

void CSprayCan::Think( void )
{
	TraceResult	tr;
	int playernum;
	int nFrames;
	CBasePlayer *pPlayer;

	pPlayer = (CBasePlayer *)GET_PRIVATE(pev->owner);

	if (pPlayer)
		nFrames = pPlayer->GetCustomDecalFrames();
	else
		nFrames = -1;

	playernum = ENTINDEX(pev->owner);

	// ALERT(at_console, "Spray by player %i, %i of %i\n", playernum, (int)(pev->frame + 1), nFrames);

	UTIL_MakeVectors(pev->angles);
	UTIL_TraceLine ( pev->origin, pev->origin + gpGlobals->v_forward * 128, ignore_monsters, pev->owner, & tr);

	// No customization present.
	if (nFrames == -1)
	{
		UTIL_DecalTrace( &tr, DECAL_LAMBDA6 );
		UTIL_Remove( this );
	}
	else
	{
		UTIL_PlayerDecalTrace( &tr, playernum, pev->frame, TRUE );
		// Just painted last custom frame.
		if ( pev->frame++ >= (nFrames - 1))
			UTIL_Remove( this );
	}

	SetNextThink( 0.1 );
}

class	CBloodSplat : public CBaseEntity
{
public:
	void	Spawn ( entvars_t *pevOwner );
	void	Spray ( void );
};

void CBloodSplat::Spawn ( entvars_t *pevOwner )
{
	pev->origin = pevOwner->origin + Vector ( 0 , 0 , 32 );
	pev->angles = pevOwner->viewangles;
	pev->owner = ENT(pevOwner);

	SetThink(&CBloodSplat:: Spray );
	SetNextThink( 0.1 );
}

void CBloodSplat::Spray ( void )
{
	TraceResult	tr;

	if ( g_Language != LANGUAGE_GERMAN )
	{
		UTIL_MakeVectors(pev->angles);
		UTIL_TraceLine ( pev->origin, pev->origin + gpGlobals->v_forward * 128, ignore_monsters, pev->owner, & tr);

		UTIL_BloodDecalTrace( &tr, BLOOD_COLOR_RED );
	}
	SetThink( Remove );
	SetNextThink( 0.1 );
}

//==============================================

void CBasePlayer::GiveNamedItem( const char *pszName )
{
	edict_t	*pent;
	int istr = MAKE_STRING(pszName);
          	
	if( FUNCTION_FROM_NAME( pszName ))
	{
		pent = CREATE_NAMED_ENTITY(istr);
		if ( FNullEnt( pent )) return;
	}
	else if( !strncmp( pszName, "weapon_", 7 ))
	{
		//may be this a weapon_generic entity?
		pent = CREATE_NAMED_ENTITY(MAKE_STRING("weapon_generic"));
		if ( FNullEnt( pent )) return;//this never gonna called anymore. just in case
  		pent->v.netname = istr; 
	}
	else if( !strncmp( pszName, "item_", 5 ))
	{
		//may be this a weapon_generic entity?
		pent = CREATE_NAMED_ENTITY(MAKE_STRING("item_generic"));
		if ( FNullEnt( pent )) return;//this never gonna called anymore. just in case
  		pent->v.netname = istr; 
	}
	else if(!strncmp( pszName, "ammo_", 5))
	{
		//may be this a weapon_generic entity?
		pent = CREATE_NAMED_ENTITY(MAKE_STRING("item_generic"));
		if ( FNullEnt( pent )) return;//this never gonna called anymore. just in case
  		pent->v.netname = istr; 
	}
	else //unknown error
	{
		Msg("can't create %s\n", pszName );
		return;
	}

          DevMsg("Give %s\n", STRING(pent->v.classname));

	VARS( pent )->origin = pev->origin;
	pent->v.spawnflags |= SF_NORESPAWN;

	DispatchSpawn( pent );
	DispatchTouch( pent, ENT( pev ) );
}

BOOL CBasePlayer :: FlashlightIsOn( void )
{
	return FBitSet(pev->effects, EF_DIMLIGHT);
}


void CBasePlayer :: FlashlightTurnOn( void )
{
	if ( !g_pGameRules->FAllowFlashlight()) return;

	if (m_iFlashBattery == 0)
          {
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, SOUND_FLASHLIGHT_ON, 1.0, ATTN_NORM, 0, PITCH_NORM );
          	return;
	}

	if (m_iHideHUD & ITEM_SUIT)
	{
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, SOUND_FLASHLIGHT_ON, 1.0, ATTN_NORM, 0, PITCH_NORM );
		SetBits(pev->effects, EF_DIMLIGHT);
		MESSAGE_BEGIN( MSG_ONE, gmsg.Flashlight, NULL, pev );
		WRITE_BYTE(1);
		WRITE_BYTE(m_iFlashBattery);
		MESSAGE_END();

		m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->time;
	}
}


void CBasePlayer :: FlashlightTurnOff( void )
{
	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, SOUND_FLASHLIGHT_OFF, 1.0, ATTN_NORM, 0, PITCH_NORM );

	ClearBits(pev->effects, EF_DIMLIGHT);
	MESSAGE_BEGIN( MSG_ONE, gmsg.Flashlight, NULL, pev );
		WRITE_BYTE(0);
		WRITE_BYTE(m_iFlashBattery);
	MESSAGE_END();

	m_flFlashLightTime = FLASH_CHARGE_TIME + gpGlobals->time;

}

/*
===============
ForceClientDllUpdate

When recording a demo, we need to have the server tell us the entire client state
so that the client side .dll can behave correctly.
Reset stuff so that the state is transmitted.
===============
*/
void CBasePlayer :: ForceClientDllUpdate( void )
{
	m_iClientHealth  = -1;
	m_iClientBattery = -1;
	m_iTrain |= TRAIN_NEW;  // Force new train message.
	m_fWeapon = FALSE;          // Force weapon send
	m_fKnownItem = FALSE;    // Force weaponinit messages.
	m_fInitHUD = TRUE;		// Force HUD gmsg.ResetHUD message

	// Now force all the necessary messages
	//  to be sent.
	UpdateClientData();
}

/*
============
ImpulseCommands
============
*/
void CBasePlayer::ImpulseCommands( )
{
	TraceResult	tr;// UNDONE: kill me! This is temporary for PreAlpha CDs

	// Handle use events
	PlayerUse();

	int iImpulse = (int)pev->impulse;
	switch (iImpulse)
	{
	case 100: //enable flashlight
		if ( FlashlightIsOn()) FlashlightTurnOff();
		else FlashlightTurnOn();
		break;
	case 201: // paint decal
		if( gpGlobals->time < m_flNextDecalTime ) break;

		UTIL_MakeVectors(pev->viewangles);
		UTIL_TraceLine ( pev->origin + pev->view_ofs, pev->origin + pev->view_ofs + gpGlobals->v_forward * 128, ignore_monsters, ENT(pev), & tr);

		if ( tr.flFraction != 1.0 )
		{	
			// line hit something, so paint a decal
			m_flNextDecalTime = gpGlobals->time + CVAR_GET_FLOAT( "decalfrequency" );
			CSprayCan *pCan = GetClassPtr((CSprayCan *)NULL);
			pCan->Spawn( pev );
		}
		break;
	case 204: //Demo recording, update client dll specific data again.
		ForceClientDllUpdate();
		break;
	default:
		// check all of the cheat impulse commands now
		CheatImpulseCommands( iImpulse );
		break;
	}

	pev->impulse = 0;
}

//=========================================================
//=========================================================
void CBasePlayer::CheatImpulseCommands( int iImpulse )
{
	if ( !g_flWeaponCheat) return;


	CBaseEntity *pEntity;
	TraceResult tr;

	switch ( iImpulse )
	{

	case 101:
	{
		if(GiveOnlyAmmo)
		{
			gEvilImpulse101 = TRUE;
			GiveNamedItem( "item_suit" );	//player may don't give suit at first time
			GiveNamedItem("weapon_handgrenade");
			GiveNamedItem( "ammo_buckshot" );
			GiveNamedItem( "ammo_9mmclip" );
			GiveNamedItem( "ammo_357" );
			GiveNamedItem( "ammo_556" );
			GiveNamedItem( "ammo_762" );
			GiveNamedItem( "ammo_9mmAR" );
         			GiveNamedItem( "ammo_m203" );
			GiveNamedItem( "ammo_rpgclip" );
			gEvilImpulse101 = FALSE;
		}
		else
		{
			gEvilImpulse101 = TRUE;
			char *pfile = (char *)LOAD_FILE( "scripts/impulse101.txt", NULL );
			int ItemName [256];
			int count = 0;
			char token[32];

			if(pfile)
			{
				while ( pfile )
				{
					//parsing impulse101.txt
					pfile = COM_ParseFile(pfile, token);
					if(strlen(token))
					{
						ItemName[ count ] = ALLOC_STRING( (char *)token );
						count++;
					}
				}
				COM_FreeFile( pfile );
				for( int i = 0; i < count; i++ )
					GiveNamedItem( (char *)STRING( ItemName[i]) );
			}
			GiveOnlyAmmo = TRUE;//no need to give all weapons again
			gEvilImpulse101 = FALSE;
		}
	}
	break;

	case 102:
	{
		pEntity = UTIL_FindEntityForward( this );
		if (pEntity) pEntity->Use( this, this, USE_TOGGLE, 0);
		break;
	}
	case 103:	// print global info about current entity or texture
	{
		TraceResult tr;
		edict_t *pWorld = g_engfuncs.pfnPEntityOfEntIndex( 0 );
		Vector start = pev->origin + pev->view_ofs;
		Vector end = start + gpGlobals->v_forward * 1024;
		UTIL_TraceLine( start, end, ignore_monsters, edict(), &tr );
		if ( tr.pHit ) pWorld = tr.pHit;
		const char *pTextureName = TRACE_TEXTURE( pWorld, start, end );
		pEntity = UTIL_FindEntityForward( this );
		if ( pEntity )
		{
			CBaseMonster *pMonster = pEntity->MyMonsterPointer();
			if ( pMonster ) pMonster->ReportAIState();
			else pEntity->Use( this, this, USE_SHOWINFO, 0 );
		}
		else if ( pTextureName ) ALERT( at_console, "Texture: %s\n", pTextureName );
	}
	break;

	case 104:// show shortest paths for entire level to nearest node
	{
		Create("node_viewer_fly", pev->origin, pev->angles);
		Create("node_viewer_human", pev->origin, pev->angles);
	}
	break;
	case 105:// remove any solid entity.
		pEntity = UTIL_FindEntityForward( this );
		if ( pEntity ) UTIL_Remove (pEntity);
		break;
	case 106://give weapon_debug
		GiveNamedItem( "weapon_debug" );
		break;
	}
}

//
// Add a weapon to the player (Item == Weapon == Selectable Object)
//
int CBasePlayer::AddPlayerItem( CBasePlayerWeapon *pItem )
{
	CBasePlayerWeapon *pInsert;

	pInsert = m_rgpPlayerItems[pItem->iItemSlot()];
          
	while (pInsert)
	{
		if (FClassnameIs( pInsert->pev, STRING( pItem->pev->netname)))
		{
			if (pItem->AddDuplicate( pInsert ))
			{
				g_pGameRules->PlayerGotWeapon ( this, pItem );
				pItem->CheckRespawn();
				pItem->Drop( );
			}
			else if (gEvilImpulse101)
			{
				// FIXME: remove anyway for deathmatch testing
				pItem->Drop();
			}
			return FALSE;
		}
		pInsert = pInsert->m_pNext;
	}

	if (pItem->AddToPlayer( this ))
	{
		g_pGameRules->PlayerGotWeapon ( this, pItem );
		pItem->CheckRespawn();

		pItem->m_pNext = m_rgpPlayerItems[pItem->iItemSlot()];
		m_rgpPlayerItems[pItem->iItemSlot()] = pItem;

		// should we switch to this item?
		if ( g_pGameRules->FShouldSwitchWeapon( this, pItem ) )
		{
			SwitchWeapon( pItem );
		}

		return TRUE;
	}
	else if (gEvilImpulse101)
	{
		// FIXME: remove anyway for deathmatch testing
		pItem->Drop();
	}

	return FALSE;
}



int CBasePlayer::RemovePlayerItem( CBasePlayerWeapon *pItem )
{
	if (m_pActiveItem == pItem)
	{
		ResetAutoaim( );
		pItem->Holster( );
		pItem->DontThink();// crowbar may be trying to swing again, etc.
		pItem->SetThink( NULL );
		m_pActiveItem = NULL;
		pev->viewmodel = 0;
		pev->weaponmodel = 0;
	}
	else if ( m_pLastItem == pItem )
		m_pLastItem = NULL;

	CBasePlayerWeapon *pPrev = m_rgpPlayerItems[pItem->iItemSlot()];

	if (pPrev == pItem)
	{
		m_rgpPlayerItems[pItem->iItemSlot()] = pItem->m_pNext;
		return TRUE;
	}
	else
	{
		while (pPrev && pPrev->m_pNext != pItem)
		{
			pPrev = pPrev->m_pNext;
		}
		if (pPrev)
		{
			pPrev->m_pNext = pItem->m_pNext;
			return TRUE;
		}
	}
	return FALSE;
}


//
// Returns the unique ID for the ammo, or -1 if error
//
int CBasePlayer :: GiveAmmo( int iCount, char *szName, int iMax )
{
	if ( !szName ) return -1;

	if ( !g_pGameRules->CanHaveAmmo( this, szName, iMax ) )
	{
		// game rules say I can't have any more of this ammo type.
		return -1;
	}

	int i = 0;

	i = GetAmmoIndex( szName );

	if ( i < 0 || i >= MAX_AMMO_SLOTS ) return -1;

	int iAdd = min( iCount, iMax - m_rgAmmo[i] );
	if ( iAdd < 1 ) return i;

	m_rgAmmo[ i ] += iAdd;


	if ( gmsg.AmmoPickup )// make sure the ammo messages have been linked first
	{
		// Send the message that ammo has been picked up
		MESSAGE_BEGIN( MSG_ONE, gmsg.AmmoPickup, NULL, pev );
			WRITE_BYTE( GetAmmoIndex(szName) );
			WRITE_BYTE( iAdd );
		MESSAGE_END();
	}

	return i;
}


/*
============
ItemPreFrame

Called every frame by the player PreThink
============
*/
void CBasePlayer::ItemPreFrame()
{
	if ( gpGlobals->time < m_flNextAttack )
	{
		return;
	}

	if (!m_pActiveItem)// XWider
	{
		if(m_pNextItem)
		{
			m_pActiveItem = m_pNextItem;
			m_pActiveItem->Deploy();
			m_pNextItem = NULL;
		}
	}

	if (!m_pActiveItem)
		return;

	m_pActiveItem->ItemPreFrame( );
}


/*
============
ItemPostFrame

Called every frame by the player PostThink
============
*/
void CBasePlayer::ItemPostFrame()
{
	static int fInSelect = FALSE;

	// check if the player is using a tank
	if ( m_pTank != NULL || m_pMonitor != NULL)return;
          
    	if ( gpGlobals->time < m_flNextAttack )
	{
		return;
	}

	ImpulseCommands();
          
	if (!m_pActiveItem)
		return;
	m_pActiveItem->ItemPostFrame( );
}

int CBasePlayer::AmmoInventory( int iAmmoIndex )
{
	if (iAmmoIndex == -1)
	{
		return -1;
	}

	return m_rgAmmo[ iAmmoIndex ];
}

int CBasePlayer :: GetAmmoIndex( const char *psz )
{
	int	i;

	if( !psz || !stricmp( psz, "none" )) return -1;

	for( i = 1; i < MAX_AMMO_SLOTS; i++ )
	{
		if( !CBasePlayerWeapon::AmmoInfoArray[i].iszName )
			continue;

		if( !stricmp( psz, STRING( CBasePlayerWeapon::AmmoInfoArray[i].iszName )))
			return i;
	}
	return -1;
}

const char *CBasePlayer::GetAmmoName( int index )
{
	return STRING( CBasePlayerWeapon::AmmoInfoArray[index].iszName );
}

// Called from UpdateClientData
// makes sure the client has all the necessary ammo info,  if values have changed
void CBasePlayer::SendAmmoUpdate(void)
{
	for (int i=0; i < MAX_AMMO_SLOTS;i++)
	{
		if (m_rgAmmo[i] != m_rgAmmoLast[i])
		{
			m_rgAmmoLast[i] = m_rgAmmo[i];

			ASSERT( m_rgAmmo[i] >= 0 );
			ASSERT( m_rgAmmo[i] < 255 );

			// send "Ammo" update message
			MESSAGE_BEGIN( MSG_ONE, gmsg.AmmoX, NULL, pev );
				WRITE_BYTE( i );
				WRITE_BYTE( max( min( m_rgAmmo[i], 254 ), 0 ) );  // clamp the value to one byte
			MESSAGE_END();
		}
	}
}

/*
=========================================================
	UpdateClientData

resends any changed player HUD info to the client.
Called every frame by Player::PreThink
Also called at start of demo recording and playback by
ForceClientDllUpdate to ensure the demo gets messages
reflecting all of the HUD state info.
=========================================================
*/

void CBasePlayer :: UpdateClientData( void )
{
	// check for cheats here
	g_flWeaponCheat = CVAR_GET_FLOAT( "sv_cheats" );  // Is the impulse 101 command allowed?
	
	if (m_fInitHUD)
	{
		m_fInitHUD = FALSE;
		gInitHUD = FALSE;

		MESSAGE_BEGIN( MSG_ONE, gmsg.ResetHUD, NULL, pev );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		if ( !m_fGameHUDInitialized )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsg.InitHUD, NULL, pev );
			MESSAGE_END();

			g_pGameRules->InitHUD( this );
			m_fGameHUDInitialized = TRUE;
			if ( g_pGameRules->IsMultiplayer() )
			{
				UTIL_FireTargets( "game_playerjoin", this, this, USE_TOGGLE );
			}
			m_iStartMessage = 1; //send player init messages
		}

		UTIL_FireTargets( "game_playerspawn", this, this, USE_TOGGLE );
		InitStatusBar();
	}

	if ( m_iHideHUD != m_iClientHideHUD )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsg.HideWeapon, NULL, pev );
			WRITE_BYTE( m_iHideHUD );
		MESSAGE_END();

		m_iClientHideHUD = m_iHideHUD;
	}

	if( viewNeedsUpdate != 0 )
	{
		int indexToSend;

		//we can entity to look at
		if(pViewEnt)indexToSend = pViewEnt->entindex();
		else	//just reset camera
		{
			indexToSend = 0;
			viewFlags = 0; //clear possibly ACTIVE flag
		}

		MESSAGE_BEGIN(MSG_ONE, gmsg.CamData, NULL, pev);
			WRITE_SHORT( indexToSend );
			WRITE_SHORT( viewFlags );
		MESSAGE_END();

		viewNeedsUpdate = 0;
	}
          
          if( hearNeedsUpdate != 0 )
          {
		// update dsp sound
          	MESSAGE_BEGIN( MSG_ONE, gmsg.RoomType, NULL, pev );
			WRITE_SHORT( m_iSndRoomtype );
		MESSAGE_END();
          	hearNeedsUpdate = 0;
          }

	if( fadeNeedsUpdate != 0 )
	{
		// update screenfade
		MESSAGE_BEGIN( MSG_ONE, gmsg.Fade, NULL, pev );
			WRITE_SHORT( FixedUnsigned16( m_iFadeTime, 1<<12 ));  
			WRITE_SHORT( FixedUnsigned16( m_iFadeHold, 1<<12 ));
			WRITE_SHORT( m_iFadeFlags );		// fade flags
			WRITE_BYTE( (byte)m_FadeColor.x );	// fade red
			WRITE_BYTE( (byte)m_FadeColor.y );	// fade green
			WRITE_BYTE( (byte)m_FadeColor.z );	// fade blue
			WRITE_BYTE( (byte)m_FadeAlpha );	// fade alpha
		MESSAGE_END();
		fadeNeedsUpdate = 0;
	}

	if (m_iStartMessage != 0)
	{
		SendStartMessages();
		m_iStartMessage = 0;
	}

	if(m_FogFadeTime != 0)
	{
		float flDegree = (gpGlobals->time - m_flStartTime) / m_FogFadeTime;
		m_iFogEndDist -= (FOG_LIMIT - m_iFogFinalEndDist)*(flDegree / m_iFogFinalEndDist);
 		
		// cap it
		if (m_iFogEndDist > FOG_LIMIT)
		{
			m_iFogEndDist = FOG_LIMIT;
			m_FogFadeTime = 0;
		}
		if (m_iFogEndDist < m_iFogFinalEndDist)
		{
			m_iFogEndDist = m_iFogFinalEndDist;
			m_FogFadeTime = 0;
		}
		fogNeedsUpdate = TRUE;
	}

	if(fogNeedsUpdate != 0)
	{
		//update fog
		MESSAGE_BEGIN( MSG_ONE, gmsg.SetFog, NULL, pev );
			WRITE_BYTE ( m_FogColor.x );
			WRITE_BYTE ( m_FogColor.y );
			WRITE_BYTE ( m_FogColor.z );
			WRITE_SHORT ( m_iFogStartDist );
			WRITE_SHORT ( m_iFogEndDist );
		MESSAGE_END();
		fogNeedsUpdate = 0;
	}

	if( m_iWarHUD != m_iClientWarHUD )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsg.WarHUD, NULL, pev );
			WRITE_BYTE( m_iWarHUD );//make static screen
		MESSAGE_END();
		m_iClientWarHUD = m_iWarHUD;
	}

	if (pev->health != m_iClientHealth)
	{
		int iHealth = max( pev->health, 0 );  // make sure that no negative health values are sent

		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsg.Health, NULL, pev );
			WRITE_BYTE( iHealth );
		MESSAGE_END();

		m_iClientHealth = pev->health;
	}


	if (pev->armorvalue != m_iClientBattery)
	{
		m_iClientBattery = pev->armorvalue;

		ASSERT( gmsg.Battery > 0 );
		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsg.Battery, NULL, pev );
			WRITE_SHORT( (int)pev->armorvalue);
		MESSAGE_END();
	}

	if (pev->dmg_take || pev->dmg_save || m_bitsHUDDamage != m_bitsDamageType)
	{
		// Comes from inside me if not set
		Vector damageOrigin = pev->origin;
		// send "damage" message
		// causes screen to flash, and pain compass to show direction of damage
		edict_t *other = pev->dmg_inflictor;
		if ( other )
		{
			CBaseEntity *pEntity = CBaseEntity::Instance(other);
			if ( pEntity )
				damageOrigin = pEntity->Center();
		}

		// only send down damage type that have hud art
		int visibleDamageBits = m_bitsDamageType & DMG_SHOWNHUD;

		MESSAGE_BEGIN( MSG_ONE, gmsg.Damage, NULL, pev );
			WRITE_BYTE( pev->dmg_save );
			WRITE_BYTE( pev->dmg_take );
			WRITE_LONG( visibleDamageBits );
			WRITE_COORD( damageOrigin.x );
			WRITE_COORD( damageOrigin.y );
			WRITE_COORD( damageOrigin.z );
		MESSAGE_END();

		pev->dmg_take = 0;
		pev->dmg_save = 0;
		m_bitsHUDDamage = m_bitsDamageType;

		// Clear off non-time-based damage indicators
		m_bitsDamageType &= DMG_TIMEBASED;
	}

	// Update Flashlight
	if ((m_flFlashLightTime) && (m_flFlashLightTime <= gpGlobals->time))
	{
		if (FlashlightIsOn())
		{
			if (m_iFlashBattery)
			{
				m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->time;
				m_iFlashBattery--;
				
				if (!m_iFlashBattery) FlashlightTurnOff();
			}
		}
		else
		{
			if (m_iFlashBattery < 100)
			{
				m_flFlashLightTime = FLASH_CHARGE_TIME + gpGlobals->time;
				m_iFlashBattery++;
			}
			else m_flFlashLightTime = 0;
		}

		MESSAGE_BEGIN( MSG_ONE, gmsg.FlashBattery, NULL, pev );
			WRITE_BYTE(m_iFlashBattery);
		MESSAGE_END();
	}

	// calculate and update rain fading
	if (Rain_endFade > 0)
	{
		if (gpGlobals->time < Rain_endFade)
		{ // we're in fading process
			if (Rain_nextFadeUpdate <= gpGlobals->time)
			{
				int secondsLeft = Rain_endFade - gpGlobals->time + 1;

				Rain_dripsPerSecond += (Rain_ideal_dripsPerSecond - Rain_dripsPerSecond) / secondsLeft;
				Rain_windX += (Rain_ideal_windX - Rain_windX) / (float)secondsLeft;
				Rain_windY += (Rain_ideal_windY - Rain_windY) / (float)secondsLeft;
				Rain_randX += (Rain_ideal_randX - Rain_randX) / (float)secondsLeft;
				Rain_randY += (Rain_ideal_randY - Rain_randY) / (float)secondsLeft;

				Rain_nextFadeUpdate = gpGlobals->time + 1; // update once per second
				rainNeedsUpdate = 1;

				ALERT(at_aiconsole, "Rain fading: curdrips: %i, idealdrips %i\n", Rain_dripsPerSecond, Rain_ideal_dripsPerSecond);
			}
		}
		else
		{ // finish fading process
			Rain_nextFadeUpdate = 0;
			Rain_endFade = 0;

			Rain_dripsPerSecond = Rain_ideal_dripsPerSecond;
			Rain_windX = Rain_ideal_windX;
			Rain_windY = Rain_ideal_windY;
			Rain_randX = Rain_ideal_randX;
			Rain_randY = Rain_ideal_randY;
			rainNeedsUpdate = 1;

			ALERT(at_aiconsole, "Rain fading finished at %i drips\n", Rain_dripsPerSecond);
		}		
	}

	// send rain message
	if (rainNeedsUpdate)
	{
		//search for rain_settings entity
		CBaseEntity *pFind; 
		pFind = UTIL_FindEntityByClassname( NULL, "env_rain" );
		if (!FNullEnt( pFind ))
		{
			// rain allowed on this map
			CBaseEntity *pEnt = CBaseEntity::Instance( pFind->edict() );

			float raindistance = pEnt->pev->armorvalue;
			float rainheight = pEnt->pev->origin[2];
			int rainmode = pEnt->pev->button;

			// search for constant rain_modifies
			pFind = UTIL_FindEntityByClassname( NULL, "util_rainmodify" );
			while ( !FNullEnt( pFind ) )
			{
				if (pFind->pev->spawnflags & 1)
				{
					// copy settings to player's data and clear fading
					CBaseEntity *pEnt = CBaseEntity::Instance( pFind->edict() );
					CUtilRainModify *pRainModify = (CUtilRainModify *)pEnt;

					Rain_dripsPerSecond = pRainModify->pev->button;
					Rain_windX = pRainModify->windXY[1];
					Rain_windY = pRainModify->windXY[0];
					Rain_randX = pRainModify->randXY[1];
					Rain_randY = pRainModify->randXY[0];

					Rain_endFade = 0;
					break;
				}
				pFind = UTIL_FindEntityByClassname( pFind, "util_rainmodify" );
			}

			MESSAGE_BEGIN(MSG_ONE, gmsg.RainData, NULL, pev);
				WRITE_SHORT(Rain_dripsPerSecond);
				WRITE_COORD(raindistance);
				WRITE_COORD(Rain_windX);
				WRITE_COORD(Rain_windY);
				WRITE_COORD(Rain_randX);
				WRITE_COORD(Rain_randY);
				WRITE_SHORT(rainmode);
				WRITE_COORD(rainheight);
			MESSAGE_END();

			if (Rain_dripsPerSecond)
				ALERT(at_aiconsole, "Sending enabling rain message\n");
			else
				ALERT(at_aiconsole, "Sending disabling rain message\n");
		}
		else
		{
			// no rain on this map
			Rain_dripsPerSecond = 0;
			Rain_windX = 0;
			Rain_windY = 0;
			Rain_randX = 0;
			Rain_randY = 0;
			Rain_ideal_dripsPerSecond = 0;
			Rain_ideal_windX = 0;
			Rain_ideal_windY = 0;
			Rain_ideal_randX = 0;
			Rain_ideal_randY = 0;
			Rain_endFade = 0;
			Rain_nextFadeUpdate = 0;
		}
		rainNeedsUpdate = 0;
	}

	// Update Flashlight
	if ((m_flFlashLightTime) && (m_flFlashLightTime <= gpGlobals->time))
	{
		if (FlashlightIsOn())
		{
			if (m_iFlashBattery)
			{
				m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->time;
				m_iFlashBattery--;

				if (!m_iFlashBattery)
					FlashlightTurnOff();
			}
		}
		else m_flFlashLightTime = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsg.FlashBattery, NULL, pev );
			WRITE_BYTE(m_iFlashBattery);
		MESSAGE_END();
	}

	if (m_iTrain & TRAIN_NEW)
	{
		ASSERT( gmsg.Train > 0 );
		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsg.Train, NULL, pev );
			WRITE_BYTE(m_iTrain & 0xF);
		MESSAGE_END();

		m_iTrain &= ~TRAIN_NEW;
	}

	//
	// New Weapon?
	//
	if (!m_fKnownItem)
	{
		m_fKnownItem = TRUE;

	// WeaponInit Message
	// byte  = # of weapons
	//
	// for each weapon:
	// byte		name str length (not including null)
	// bytes... name
	// byte		Ammo Type
	// byte		Ammo2 Type
	// byte		bucket
	// byte		bucket pos
	// byte		flags
	// ????		Icons

		// Send ALL the weapon info now
		int i;

		for (i = 0; i < MAX_WEAPONS; i++)
		{
			ItemInfo& II = CBasePlayerWeapon::ItemInfoArray[i];

			if( !II.iId ) continue;

			const char *pszName;
			if( !II.iszName )
				pszName = "Empty";
			else pszName = STRING( II.iszName );
                              
			MESSAGE_BEGIN( MSG_ONE, gmsg.WeaponList, NULL, pev );
				WRITE_STRING( pszName );		// string	weapon name
				WRITE_BYTE( GetAmmoIndex( STRING( II.iszAmmo1 )));	// byte	Ammo Type
				WRITE_BYTE( II.iMaxAmmo1 );		// byte	Max Ammo 1
				WRITE_BYTE( GetAmmoIndex(STRING( II.iszAmmo2 )));	// byte	Ammo2 Type
				WRITE_BYTE( II.iMaxAmmo2 );		// byte	Max Ammo 2
				WRITE_BYTE( II.iSlot );		// byte	bucket
				WRITE_BYTE( II.iPosition );		// byte	bucket pos
				WRITE_BYTE( II.iId );		// byte	id (bit index into pev->weapons)
				WRITE_BYTE( II.iFlags );		// byte	Flags
			MESSAGE_END();
		}
	}

	SendAmmoUpdate();

	// Update all the items
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] )  // each item updates it's successors
			m_rgpPlayerItems[i]->UpdateClientData( this );
	}

	// Cache and client weapon change
	m_pClientActiveItem = m_pActiveItem;

	// update Status Bar
	if( m_flNextSBarUpdateTime < gpGlobals->time )
	{
		UpdateStatusBar();
		m_flNextSBarUpdateTime = gpGlobals->time + 0.2;
	}
}

void CBasePlayer :: SendStartMessages( void )
{
	edict_t *pEdict = g_engfuncs.pfnPEntityOfEntIndex( 1 );
	CBaseEntity *pEntity;

	if ( !pEdict ) return;

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		if ( pEdict->free ) continue;// Not in use

		pEntity = CBaseEntity::Instance(pEdict);
		if ( !pEntity ) continue;

		pEntity->StartMessage( this );
	}
}

//=========================================================
// FBecomeProne - Overridden for the player to set the proper
// physics flags when a barnacle grabs player.
//=========================================================
BOOL CBasePlayer :: FBecomeProne ( void )
{
	m_afPhysicsFlags |= PFLAG_ONBARNACLE;
	return TRUE;
}

//=========================================================
// BarnacleVictimBitten - bad name for a function that is called
// by Barnacle victims when the barnacle pulls their head
// into its mouth. For the player, just die.
//=========================================================
void CBasePlayer :: BarnacleVictimBitten ( entvars_t *pevBarnacle )
{
	TakeDamage ( pevBarnacle, pevBarnacle, pev->health + pev->armorvalue, DMG_SLASH | DMG_ALWAYSGIB );
}

//=========================================================
// BarnacleVictimReleased - overridden for player who has
// physics flags concerns.
//=========================================================
void CBasePlayer :: BarnacleVictimReleased ( void )
{
	m_afPhysicsFlags &= ~PFLAG_ONBARNACLE;
}


//=========================================================
// Illumination
// return player light level plus virtual muzzle flash
//=========================================================
int CBasePlayer :: Illumination( void )
{
	int iIllum = CBaseEntity::Illumination( );

	iIllum += m_iWeaponFlash;
	if (iIllum > 255)
		return 255;
	return iIllum;
}


void CBasePlayer :: EnableControl(BOOL fControl)
{
	if( !fControl )
	{
		pev->flags |= FL_FROZEN;
		pev->velocity = g_vecZero; //LRC - stop view bobbing
	}
	else pev->flags &= ~FL_FROZEN;
}


#define DOT_1DEGREE   0.9998476951564
#define DOT_2DEGREE   0.9993908270191
#define DOT_3DEGREE   0.9986295347546
#define DOT_4DEGREE   0.9975640502598
#define DOT_5DEGREE   0.9961946980917
#define DOT_6DEGREE   0.9945218953683
#define DOT_7DEGREE   0.9925461516413
#define DOT_8DEGREE   0.9902680687416
#define DOT_9DEGREE   0.9876883405951
#define DOT_10DEGREE  0.9848077530122
#define DOT_15DEGREE  0.9659258262891
#define DOT_20DEGREE  0.9396926207859
#define DOT_25DEGREE  0.9063077870367

//=========================================================
// Autoaim
// set crosshair position to point to enemey
//=========================================================
Vector CBasePlayer :: GetAutoaimVector( float flDelta )
{
	Vector vecSrc = GetGunPosition( );
	float flDist = 8192;

	BOOL m_fOldTargeting = m_fOnTarget;
	Vector angles = AutoaimDeflection(vecSrc, flDist, flDelta );

	// update ontarget if changed
	if ( !g_pGameRules->AllowAutoTargetCrosshair() )
		m_fOnTarget = 0;

	if (angles.x > 180)
		angles.x -= 360;
	if (angles.x < -180)
		angles.x += 360;
	if (angles.y > 180)
		angles.y -= 360;
	if (angles.y < -180)
		angles.y += 360;

	if (angles.x > 25)
		angles.x = 25;
	if (angles.x < -25)
		angles.x = -25;
	if (angles.y > 12)
		angles.y = 12;
	if (angles.y < -12)
		angles.y = -12;


	m_vecAutoAim = angles * 0.9;

	// Don't send across network if sv_aim is 0
	if( CVAR_GET_FLOAT( "sv_aim" ) != 0 )
	{
		if ( m_vecAutoAim.x != m_lastx || m_vecAutoAim.y != m_lasty )
		{
			SET_CROSSHAIRANGLE( edict(), -m_vecAutoAim.x, m_vecAutoAim.y );

			m_lastx = m_vecAutoAim.x;
			m_lasty = m_vecAutoAim.y;
		}
	}

	// ALERT( at_console, "%f %f\n", angles.x, angles.y );

	UTIL_MakeVectors( pev->viewangles + pev->punchangle + m_vecAutoAim );
	return gpGlobals->v_forward;
}


Vector CBasePlayer :: AutoaimDeflection( Vector &vecSrc, float flDist, float flDelta  )
{
	edict_t		*pEdict = g_engfuncs.pfnPEntityOfEntIndex( 1 );
	CBaseEntity	*pEntity;
	float		bestdot;
	Vector		bestdir;
	edict_t		*bestent;
	TraceResult tr;

	if( CVAR_GET_FLOAT( "sv_aim" ) == 0 )
	{
		m_fOnTarget = FALSE;
		return g_vecZero;
	}

	UTIL_MakeVectors( pev->viewangles + pev->punchangle + m_vecAutoAim );

	// try all possible entities
	bestdir = gpGlobals->v_forward;
	bestdot = flDelta; // +- 10 degrees
	bestent = NULL;

	m_fOnTarget = FALSE;

	UTIL_TraceLine( vecSrc, vecSrc + bestdir * flDist, dont_ignore_monsters, edict(), &tr );


	if ( tr.pHit && tr.pHit->v.takedamage != DAMAGE_NO)
	{
		// don't look through water
		if (!((pev->waterlevel != 3 && tr.pHit->v.waterlevel == 3)
			|| (pev->waterlevel == 3 && tr.pHit->v.waterlevel == 0)))
		{
			if (tr.pHit->v.takedamage == DAMAGE_AIM)
				m_fOnTarget = TRUE;

			return m_vecAutoAim;
		}
	}

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		Vector center;
		Vector dir;
		float dot;

		if ( pEdict->free )	// Not in use
			continue;

		if (pEdict->v.takedamage != DAMAGE_AIM)
			continue;
		if (pEdict == edict())
			continue;
		if ( !g_pGameRules->ShouldAutoAim( this, pEdict ) )
			continue;

		pEntity = Instance( pEdict );
		if (pEntity == NULL)
			continue;

		if (!pEntity->IsAlive())
			continue;

		// don't look through water
		if ((pev->waterlevel != 3 && pEntity->pev->waterlevel == 3)
			|| (pev->waterlevel == 3 && pEntity->pev->waterlevel == 0))
			continue;

		center = pEntity->BodyTarget( vecSrc );

		dir = (center - vecSrc).Normalize( );

		// make sure it's in front of the player
		if (DotProduct (dir, gpGlobals->v_forward ) < 0)
			continue;

		dot = fabs( DotProduct (dir, gpGlobals->v_right ) )
			+ fabs( DotProduct (dir, gpGlobals->v_up ) ) * 0.5;

		// tweek for distance
		dot *= 1.0 + 0.2 * ((center - vecSrc).Length() / flDist);

		if (dot > bestdot)
			continue;	// to far to turn

		UTIL_TraceLine( vecSrc, center, dont_ignore_monsters, edict(), &tr );
		if (tr.flFraction != 1.0 && tr.pHit != pEdict)
		{
			// ALERT( at_console, "hit %s, can't see %s\n", STRING( tr.pHit->v.classname ), STRING( pEdict->v.classname ) );
			continue;
		}

		// don't shoot at friends
		if (IRelationship( pEntity ) < 0)
		{
			if ( !pEntity->IsPlayer() && !g_pGameRules->IsDeathmatch())
				// ALERT( at_console, "friend\n");
				continue;
		}

		// can shoot at this one
		bestdot = dot;
		bestent = pEdict;
		bestdir = dir;
	}

	if (bestent)
	{
		bestdir = UTIL_VecToAngles (bestdir);
		bestdir.x = -bestdir.x;
		bestdir = bestdir - pev->viewangles - pev->punchangle;

		if (bestent->v.takedamage == DAMAGE_AIM)
			m_fOnTarget = TRUE;

		return bestdir;
	}

	return Vector( 0, 0, 0 );
}


void CBasePlayer :: ResetAutoaim( )
{
	if (m_vecAutoAim.x != 0 || m_vecAutoAim.y != 0)
	{
		m_vecAutoAim = Vector( 0, 0, 0 );
		SET_CROSSHAIRANGLE( edict(), 0, 0 );
	}
	m_fOnTarget = FALSE;
}

/*
=============
SetCustomDecalFrames

  UNDONE:  Determine real frame limit, 8 is a placeholder.
  Note:  -1 means no custom frames present.
=============
*/
void CBasePlayer :: SetCustomDecalFrames( int nFrames )
{
	if (nFrames > 0 &&
		nFrames < 8)
		m_nCustomSprayFrames = nFrames;
	else
		m_nCustomSprayFrames = -1;
}

/*
=============
GetCustomDecalFrames

  Returns the # of custom frames this player's custom clan logo contains.
=============
*/
int CBasePlayer :: GetCustomDecalFrames( void )
{
	return m_nCustomSprayFrames;
}


//=========================================================
// DropPlayerItem - drop the named item, or if no name,
// the active item.
//=========================================================
void CBasePlayer::DropPlayerItem ( char *pszItemName )
{
	if( !g_pGameRules->IsMultiplayer() || (CVAR_GET_FLOAT( "mp_weaponstay" ) > 0 ))
	{
		// no dropping in single player.
		//return;
	}

	if ( !strlen( pszItemName ) )
	{
		// if this string has no length, the client didn't type a name!
		// assume player wants to drop the active item.
		// make the string null to make future operations in this function easier
		pszItemName = NULL;
	}

	CBasePlayerWeapon *pWeapon;
	int i;

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pWeapon = m_rgpPlayerItems[ i ];

		while ( pWeapon )
		{
			if ( pszItemName )
			{
				// try to match by name.
				if ( !strcmp( pszItemName, STRING( pWeapon->pev->netname ) ) )
				{
					// match!
					break;
				}
			}
			else
			{
				// trying to drop active item
				if ( pWeapon == m_pActiveItem )
				{
					// active item!
					break;
				}
			}

			pWeapon = pWeapon->m_pNext;
		}


		// if we land here with a valid pWeapon pointer, that's because we found the
		// item we want to drop and hit a BREAK;  pWeapon is the item.
		if ( pWeapon )
		{
			g_pGameRules->GetNextBestWeapon( this, pWeapon );

			UTIL_MakeVectors ( pev->angles );

			pev->weapons &= ~(1<<pWeapon->m_iId);// take item off hud

			CWeaponBox *pWeaponBox = (CWeaponBox *)CBaseEntity::Create( "item_weaponbox", pev->origin + gpGlobals->v_forward * 10, pev->angles, edict() );
			pWeaponBox->pev->angles.x = 0;
			pWeaponBox->pev->angles.z = 0;
			pWeaponBox->PackWeapon( pWeapon );
			pWeaponBox->pev->velocity = gpGlobals->v_forward * 300 + gpGlobals->v_forward * 100;

			// drop half of the ammo for this weapon.
			int	iAmmoIndex;

			iAmmoIndex = GetAmmoIndex ( pWeapon->pszAmmo1() ); // ???

			if ( iAmmoIndex != -1 )
			{
				// this weapon weapon uses ammo, so pack an appropriate amount.
				if ( pWeapon->iFlags() & ITEM_FLAG_EXHAUSTIBLE )
				{
					// pack up all the ammo, this weapon is its own ammo type
					pWeaponBox->PackAmmo( MAKE_STRING(pWeapon->pszAmmo1()), m_rgAmmo[ iAmmoIndex ] );
					m_rgAmmo[ iAmmoIndex ] = 0;

				}
				else
				{
					// pack half of the ammo
					pWeaponBox->PackAmmo( MAKE_STRING(pWeapon->pszAmmo1()), m_rgAmmo[ iAmmoIndex ] / 2 );
					m_rgAmmo[ iAmmoIndex ] /= 2;
				}

			}

			return;// we're done, so stop searching with the FOR loop.
		}
	}
}

//=========================================================
// HasPlayerItem Does the player already have this item?
//=========================================================
BOOL CBasePlayer::HasPlayerItem( CBasePlayerWeapon *pCheckItem )
{
	CBasePlayerWeapon *pItem = m_rgpPlayerItems[pCheckItem->iItemSlot()];

	while (pItem)
	{
		if (FClassnameIs( pItem->pev, STRING( pCheckItem->pev->netname) ))
			return TRUE;
		pItem = pItem->m_pNext;
	}

	return FALSE;
}

//=========================================================
// HasNamedPlayerItem Does the player already have this item?
//=========================================================
BOOL CBasePlayer::HasNamedPlayerItem( const char *pszItemName )
{
	CBasePlayerWeapon *pItem;
	int i;

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pItem = m_rgpPlayerItems[ i ];

		while (pItem)
		{
			if ( !strcmp( pszItemName, STRING( pItem->pev->netname )))
				return TRUE;
			pItem = pItem->m_pNext;
		}
	}

	return FALSE;
}

//=========================================================
//
//=========================================================
BOOL CBasePlayer :: SwitchWeapon( CBasePlayerWeapon *pWeapon )
{
	if ( !pWeapon->CanDeploy() ) return FALSE;
	ResetAutoaim( );

	if (m_pActiveItem) m_pActiveItem->Holster( );
	QueueItem(pWeapon);
	if (m_pActiveItem) m_pActiveItem->Deploy( );

	return TRUE;
}

//=========================================================
// Body queue class here.... It's really just CBaseEntity
//=========================================================
class CCorpse : public CBaseEntity
{
	virtual int ObjectCaps( void ) { return FCAP_DONT_SAVE; }	
};
LINK_ENTITY_TO_CLASS( bodyque, CCorpse );

//=========================================================
// Dead HEV suit prop
// LRC- i.e. the dead blokes you see in Xen.
//=========================================================
class CDeadHEV : public CBaseMonster
{
public:
	void Spawn( void );
	int	Classify ( void ) { return	CLASS_HUMAN_MILITARY; }

	void KeyValue( KeyValueData *pkvd );

	int	m_iPose;// which sequence to display	-- temporary, don't need to save
	static char *m_szPoses[4];
};

char *CDeadHEV::m_szPoses[] = { "deadback", "deadsitting", "deadstomach", "deadtable" };

void CDeadHEV::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "pose"))
	{
		m_iPose = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( pkvd );
}

LINK_ENTITY_TO_CLASS( monster_hevsuit_dead, CDeadHEV );

//=========================================================
// ********** DeadHEV SPAWN **********
//=========================================================
void CDeadHEV :: Spawn( void )
{
	PRECACHE_MODEL("models/player.mdl");
	SET_MODEL(ENT(pev), "models/player.mdl");

	pev->effects		= 0;
	pev->yaw_speed		= 8;
	pev->sequence		= 0;
	pev->body			= 1;
	m_bloodColor		= BLOOD_COLOR_RED;

	pev->sequence = LookupSequence( m_szPoses[m_iPose] );

	if( pev->sequence == -1 )
	{
		ALERT ( at_console, "Dead hevsuit with bad pose\n" );
		pev->sequence = 0;
		pev->effects = EF_BRIGHTFIELD;
	}

	// Corpses have less health
	pev->health			= 8;

	MonsterInitDead();
}