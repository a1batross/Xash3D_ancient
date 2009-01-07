//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "sfx.h"
#include "cbase.h"
#include "basebeams.h"
#include "baseweapon.h"
#include "basetank.h"
#include "monsters.h"

TYPEDESCRIPTION	CFuncTank::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncTank, m_yawCenter, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_yawRate, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_yawRange, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_yawTolerance, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_pitchCenter, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_pitchRate, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_pitchRange, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_pitchTolerance, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_fireLast, FIELD_TIME ),
	DEFINE_FIELD( CFuncTank, m_fireRate, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_lastSightTime, FIELD_TIME ),
	DEFINE_FIELD( CFuncTank, m_persist, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_Range, FIELD_RANGE ),
	DEFINE_FIELD( CFuncTank, m_barrelPos, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTank, m_spriteScale, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_iszSpriteSmoke, FIELD_STRING ),
	DEFINE_FIELD( CFuncTank, m_iszSpriteFlash, FIELD_STRING ),
	DEFINE_FIELD( CFuncTank, m_bulletType, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTank, m_sightOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTank, m_spread, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTank, m_pControls, FIELD_CLASSPTR ), //LRC
	DEFINE_FIELD( CFuncTank, m_pSpot, FIELD_CLASSPTR ), //LRC
	DEFINE_FIELD( CFuncTank, m_flNextAttack, FIELD_TIME ),
	DEFINE_FIELD( CFuncTank, m_iBulletDamage, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTank, m_iszMaster, FIELD_STRING ),
	DEFINE_FIELD( CFuncTank, m_iszFireMaster, FIELD_STRING ), //LRC
};
IMPLEMENT_SAVERESTORE( CFuncTank, CBaseEntity );

void CFuncTank :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "yawrate"))
	{
		m_yawRate = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "yawrange"))
	{
		m_yawRange = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "yawtolerance"))
	{
		m_yawTolerance = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pitchrange"))
	{
		m_pitchRange = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pitchrate"))
	{
		m_pitchRate = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pitchtolerance"))
	{
		m_pitchTolerance = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firerate"))
	{
		m_fireRate = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "barrelend"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "spritescale"))
	{
		m_spriteScale = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "spritesmoke"))
	{
		m_iszSpriteSmoke = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "spriteflash"))
	{
		m_iszSpriteFlash = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "rotatesound"))
	{
		pev->noise = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "persistence"))
	{
		m_persist = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "bullet"))
	{
		m_bulletType = (TANKBULLET)atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "bullet_damage" )) 
	{
		m_iBulletDamage = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firespread"))
	{
		m_spread = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "Range"))
	{
		m_Range = RandomRange(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "master"))
	{
		m_iszMaster = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firemaster"))
	{
		m_iszFireMaster = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iClass"))
	{
		m_iTankClass = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBaseEntity::KeyValue( pkvd );
}

void CFuncTank :: Spawn( void )
{
	Precache();

	pev->movetype = MOVETYPE_PUSH;  // so it doesn't get pushed by anything
	pev->solid = SOLID_BSP;
	UTIL_SetModel( ENT(pev), pev->model );
          SetBits (pFlags, PF_ANGULAR);
	
	if ( IsActive() )
	{
		SetNextThink(1.0);
	}

	if (!m_iTankClass)
	{
          	m_iTankClass = 0;
	}

	if(m_Range.m_flMax == 0) m_Range.m_flMax = 4096;

	if ( m_fireRate <= 0 ) m_fireRate = 1;
	if ( m_spread > MAX_FIRING_SPREADS ) m_spread = 0;

	pev->oldorigin = pev->origin;
          pev->flags |= FL_TANK;//this is tank entity
}

void CFuncTank::PostSpawn( void )
{
	if (m_pParent)
	{
		m_yawCenter = pev->angles.y - m_pParent->pev->angles.y;
		m_pitchCenter = pev->angles.x - m_pParent->pev->angles.x;
	}
	else
	{
		m_yawCenter = pev->angles.y;
		m_pitchCenter = pev->angles.x;
	}

	CBaseEntity *pTarget = UTIL_FindEntityByTargetname(NULL, STRING(pev->netname), NULL );
          if(pTarget) 
          {
          	m_barrelPos = pTarget->pev->origin - pev->origin; //write offset
		UTIL_Remove( pTarget );
	}
	else if(!IsLaserTank()) //get "laserentity" position for tanklaser
	{
		Msg("Error! %s: %s haven't barrel end entity! Use default values.\n", STRING(pev->classname), STRING(pev->targetname));
		m_barrelPos = Vector( fabs(pev->absmax.x), 0, 0 );
	}
	m_sightOrigin = BarrelPosition(); // Point at the end of the barrel
}

void CFuncTank :: Precache( void )
{
	if ( m_iszSpriteSmoke )
		UTIL_PrecacheModel( m_iszSpriteSmoke );
	if ( m_iszSpriteFlash )
		UTIL_PrecacheModel( m_iszSpriteFlash );
	if ( pev->noise )
		UTIL_PrecacheSound( pev->noise );
}

BOOL CFuncTank :: StartControl( CBasePlayer* pController, CFuncTankControls *pControls )
{
	// we're already being controlled or playing a sequence
	if ( m_pControls != NULL )
		return FALSE;

	// Team only or disabled?
	if ( m_iszMaster )
	{
		if ( !UTIL_IsMasterTriggered( m_iszMaster, pController ) )
			return FALSE;
	}

	m_iState = STATE_ON;
	m_pControls = pControls;

	if (m_pSpot) m_pSpot->Revive();
	 
	SetNextThink(0.3);
	return TRUE;
}

void CFuncTank :: StopControl( CFuncTankControls* pControls)
{
	if ( !m_pControls || m_pControls != pControls)
	{
		return;
	}
          m_iState = STATE_OFF;
          
	if (m_pSpot) m_pSpot->Suspend(-1);
	StopRotSound(); //LRC

	DontThink();
	UTIL_SetAvelocity(this, g_vecZero);
	m_pControls = NULL;

	if ( IsActive() )
	{
		SetNextThink(1.0);
	}
}

void CFuncTank::UpdateSpot( void )
{
	if ( pev->spawnflags & SF_TANK_LASERSPOT )
	{
	          
		if (!m_pSpot) m_pSpot = CLaserSpot::CreateSpot();

		Vector vecAiming;
		UTIL_MakeVectorsPrivate( pev->angles, vecAiming, NULL, NULL );
		Vector vecSrc = BarrelPosition( );

		TraceResult tr;
		UTIL_TraceLine ( vecSrc, vecSrc + vecAiming * 8192, dont_ignore_monsters, ENT(pev), &tr );
		UTIL_SetOrigin( m_pSpot, tr.vecEndPos );
	}
}

void CFuncTank :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->spawnflags & SF_TANK_CANCONTROL )
	{
		if ( pActivator->Classify() != CLASS_PLAYER ) return;
	}
	else
	{
		if (useType == USE_TOGGLE)
		{
			if(IsActive()) useType = USE_OFF;
			else useType = USE_ON;
		}
		if (useType == USE_ON)
		{
			TankActivate();
			if (m_pSpot) m_pSpot->Revive();
		}
		else if (useType == USE_OFF)
		{
			TankDeactivate();
			if (m_pSpot) m_pSpot->Suspend(-1);
		}
	}
}

CBaseEntity *CFuncTank:: BestVisibleEnemy ( void )
{
	CBaseEntity	*pReturn;
	int		iNearest;
	int		iDist;
	int		iBestRelationship;
	int		iLookDist = m_Range.m_flMax ? m_Range.m_flMax : 512; //thanks to Waldo for this.

	iNearest = 8192;// so first visible entity will become the closest.
	pReturn = NULL;
	iBestRelationship = R_DL;

	CBaseEntity *pList[100];

	Vector delta = Vector( iLookDist, iLookDist, iLookDist );

	// Find only monsters/clients in box, NOT limited to PVS
	int count = UTIL_EntitiesInBox( pList, 100, pev->origin - delta, pev->origin + delta, FL_CLIENT|FL_MONSTER );
	int i;

	for (i = 0; i < count; i++ )
	{
		if ( pList[i]->IsAlive() )
		{
			if ( IRelationship( pList[i] ) > iBestRelationship )
			{
				// this entity is disliked MORE than the entity that we 
				// currently think is the best visible enemy. No need to do 
				// a distance check, just get mad at this one for now.
				iBestRelationship = IRelationship ( pList[i] );
				iNearest = ( pList[i]->pev->origin - pev->origin ).Length();
				pReturn = pList[i];
			}
			else if ( IRelationship( pList[i] ) == iBestRelationship )
			{
				// this entity is disliked just as much as the entity that
				// we currently think is the best visible enemy, so we only
				// get mad at it if it is closer.
				iDist = ( pList[i]->pev->origin - pev->origin ).Length();
				
				if ( iDist <= iNearest )
				{
					iNearest = iDist;
					//these are guaranteed to be the same! iBestRelationship = IRelationship ( pList[i] ); 
					pReturn = pList[i];
				}
			}
		}
	}
	return pReturn;
}


int CFuncTank::IRelationship( CBaseEntity* pTarget )
{
	int iOtherClass = pTarget->Classify();
	if (iOtherClass == CLASS_NONE) return R_NO;

	if (!m_iTankClass)
	{
		if (iOtherClass == CLASS_PLAYER)
			return R_HT;
		else
			return R_NO;
	}
	else if (m_iTankClass == CLASS_PLAYER_ALLY)
	{
		switch (iOtherClass)
		{
		case CLASS_HUMAN_MILITARY:
		case CLASS_MACHINE:
		case CLASS_ALIEN_MILITARY:
		case CLASS_ALIEN_MONSTER:
		case CLASS_ALIEN_PREDATOR:
		case CLASS_ALIEN_PREY:
			return R_HT;
		default:
			return R_NO;
		}
	}
	else if (m_iTankClass == CLASS_HUMAN_MILITARY)
	{
		switch (iOtherClass)
		{
		case CLASS_PLAYER:
		case CLASS_PLAYER_ALLY:
		case CLASS_ALIEN_MILITARY:
		case CLASS_ALIEN_MONSTER:
		case CLASS_ALIEN_PREDATOR:
		case CLASS_ALIEN_PREY:
			return R_HT;
		case CLASS_HUMAN_PASSIVE:
			return R_DL;
		default:
			return R_NO;
		}
	}
	else if (m_iTankClass == CLASS_ALIEN_MILITARY)
	{
		switch (iOtherClass)
		{
		case CLASS_PLAYER:
		case CLASS_PLAYER_ALLY:
		case CLASS_HUMAN_MILITARY:
			return R_HT;
		case CLASS_HUMAN_PASSIVE:
			return R_DL;
		default:
			return R_NO;
		}
	}
	else	return R_NO;
}

BOOL CFuncTank :: InRange( float range )
{
	if ( range < m_Range.m_flMin )
		return FALSE;
	if ( m_Range.m_flMax > 0 && range > m_Range.m_flMax )
		return FALSE;

	return TRUE;
}

void CFuncTank :: Think( void )
{
	TrackTarget();

	if ( fabs(pev->avelocity.x) > 1 || fabs(pev->avelocity.y) > 1 )
		StartRotSound();
	else	StopRotSound();
}

void CFuncTank::TrackTarget( void )
{
	TraceResult tr;
	BOOL updateTime = FALSE, lineOfSight;
	Vector angles, direction, targetPosition, barrelEnd;
	Vector v_right, v_up;
	CBaseEntity *pTarget;
	CBasePlayer* pController = NULL;

	if (m_pControls && m_pControls->m_pController)
	{
		UpdateSpot();
		pController = m_pControls->m_pController;
		pController->pev->viewmodel = 0;
		SetNextThink(0.05);

		if( pev->spawnflags & SF_TANK_MATCHTARGET )
		{
			// "Match target" mode:
			// first, get the player's angles
			angles = pController->pev->viewangles;

			// work out what point the player is looking at
			UTIL_MakeVectorsPrivate( angles, direction, NULL, NULL );

			targetPosition = pController->EyePosition() + direction * 1000;

			edict_t *ownerTemp = pev->owner; //LRC store the owner, so we can put it back after the check
			pev->owner = pController->edict(); //LRC when doing the matchtarget check, don't hit the player or the tank.

			UTIL_TraceLine(
				pController->EyePosition(),
				targetPosition,
				missile, //the opposite of ignore_monsters: target them if we go anywhere near!
				ignore_glass,
				edict(), &tr
			);

			pev->owner = ownerTemp; //LRC put the owner back

			// Work out what angle we need to face to look at that point
			direction = tr.vecEndPos - pev->origin;
			angles = UTIL_VecToAngles( direction );
			targetPosition = tr.vecEndPos;

			// Calculate the additional rotation to point the end of the barrel at the target
			// (instead of the gun's center)
			AdjustAnglesForBarrel( angles, direction.Length() );
		}
		else
		{
			// "Match angles" mode
			// just get the player's angles
			angles = pController->pev->viewangles;
			angles[0] = 0 - angles[0];

			UpdateSpot();
			SetNextThink( 0.05 ); // g-cont. for more smoothing motion a laser spot
		}
	}
	else
	{
		if ( IsActive() )
		{
			SetNextThink(0.1);
		}
		else
		{
			DontThink();
			UTIL_SetAvelocity(this, g_vecZero);
			return;
		}

		UpdateSpot();

		// if we can't see any players
		pTarget = BestVisibleEnemy();
		if ( FNullEnt( pTarget ) )
		{
			if ( IsActive() ) SetNextThink(2); // No enemies visible, wait 2 secs
			return;
		}

		// Calculate angle needed to aim at target
		barrelEnd = BarrelPosition();
		targetPosition = pTarget->pev->origin + pTarget->pev->view_ofs;
		float range = (targetPosition - barrelEnd).Length();
		
		if ( !InRange( range ) )
			return;

		UTIL_TraceLine( barrelEnd, targetPosition, dont_ignore_monsters, edict(), &tr );
		
		if ( tr.flFraction == 1.0 || tr.pHit == ENT(pTarget->pev) )
		{
			lineOfSight = TRUE;

			if ( InRange( range ) && pTarget->IsAlive() )
			{
				updateTime = TRUE; // I think I saw him, pa!
				m_sightOrigin = UpdateTargetPosition( pTarget );
			}
		}
		else
		{
			// No line of sight, don't track
			lineOfSight = FALSE;
		}

		direction = m_sightOrigin - pev->origin;
		angles = UTIL_VecToAngles( direction );
		AdjustAnglesForBarrel( angles, direction.Length() );
	}

	angles.x = -angles.x;

	float currentYawCenter, currentPitchCenter;

	// Force the angles to be relative to the center position
	if (m_pParent)
	{
		currentYawCenter = m_yawCenter + m_pParent->pev->angles.y;
		currentPitchCenter = m_pitchCenter + m_pParent->pev->angles.x;
	}
	else
	{
		currentYawCenter = m_yawCenter;
		currentPitchCenter = m_pitchCenter;
	}

	angles.y = currentYawCenter + UTIL_AngleDistance( angles.y, currentYawCenter );
	angles.x = currentPitchCenter + UTIL_AngleDistance( angles.x, currentPitchCenter );

	// Limit against range in y
	if (m_yawRange < 360)
	{
		if ( angles.y > currentYawCenter + m_yawRange )
		{
			angles.y = currentYawCenter + m_yawRange;
			updateTime = FALSE;	// If player is outside fire arc, we didn't really see him
		}
		else if ( angles.y < (currentYawCenter - m_yawRange) )
		{
			angles.y = (currentYawCenter - m_yawRange);
			updateTime = FALSE; // If player is outside fire arc, we didn't really see him
		}
	}
	// we can always 'see' the whole vertical arc, so it's just the yaw we needed to check.

	if ( updateTime )
		m_lastSightTime = gpGlobals->time;

	// Move toward target at rate or less
	float distY = UTIL_AngleDistance( angles.y, pev->angles.y );
	Vector setAVel = g_vecZero;

	setAVel.y = distY * 10;
	if ( setAVel.y > m_yawRate ) setAVel.y = m_yawRate;
	else if ( setAVel.y < -m_yawRate ) setAVel.y = -m_yawRate;

	// Limit against range in x
	if ( angles.x > currentPitchCenter + m_pitchRange ) angles.x = currentPitchCenter + m_pitchRange;
	else if ( angles.x < currentPitchCenter - m_pitchRange ) angles.x = currentPitchCenter - m_pitchRange;

	// Move toward target at rate or less
	float distX = UTIL_AngleDistance( angles.x, pev->angles.x );
	setAVel.x = distX  * 10;

	if ( setAVel.x > m_pitchRate ) setAVel.x = m_pitchRate;
	else if ( setAVel.x < -m_pitchRate ) setAVel.x = -m_pitchRate;

	UTIL_SetAvelocity(this, setAVel);

	// notify the TankSequence if we're (pretty close to) facing the target

	// firing with player-controlled tanks:
	if ( pController )
	{
		if ( gpGlobals->time < m_flNextAttack )
		return;

		if ( pController->pev->button & IN_ATTACK )
		{
			Vector forward;
			UTIL_MakeVectorsPrivate( pev->angles, forward, NULL, NULL );

			// to make sure the gun doesn't fire too many bullets
			m_fireLast = gpGlobals->time - (1/m_fireRate) - 0.01;

			TryFire( BarrelPosition(), forward, pController->pev );
		
			if ( pController && pController->IsPlayer() )
				((CBasePlayer *)pController)->m_iWeaponVolume = LOUD_GUN_VOLUME;

			m_flNextAttack = gpGlobals->time + (1/m_fireRate);
		}
		else m_iState = STATE_ON;
	}
	// firing with automatic guns:
	else if ( CanFire() && ( (fabs(distX) < m_pitchTolerance && fabs(distY) < m_yawTolerance) || (pev->spawnflags & SF_TANK_LINEOFSIGHT) ) )
	{
		BOOL fire = FALSE;
		Vector forward;
		UTIL_MakeVectorsPrivate( pev->angles, forward, NULL, NULL );

		if ( pev->spawnflags & SF_TANK_LINEOFSIGHT )
		{
			float length = direction.Length();
			UTIL_TraceLine( barrelEnd, barrelEnd + forward * length, dont_ignore_monsters, edict(), &tr );
			if ( tr.pHit == ENT(pTarget->pev) )
				fire = TRUE;
		}
		else fire = TRUE;

		if ( fire )
		{
			TryFire( BarrelPosition(), forward, pev );
		}
		else	
		{
			m_fireLast = 0;
		}
	}
	else	m_fireLast = 0;
}

void CFuncTank::AdjustAnglesForBarrel( Vector &angles, float distance )
{
	float r2, d2;


	if ( m_barrelPos.y != 0 || m_barrelPos.z != 0 )
	{
		distance -= m_barrelPos.z;
		d2 = distance * distance;
		if ( m_barrelPos.y )
		{
			r2 = m_barrelPos.y * m_barrelPos.y;
			angles.y += (180.0 / M_PI) * atan2( m_barrelPos.y, sqrt( d2 - r2 ) );
		}
		if ( m_barrelPos.z )
		{
			r2 = m_barrelPos.z * m_barrelPos.z;
			angles.x += (180.0 / M_PI) * atan2( -m_barrelPos.z, sqrt( d2 - r2 ) );
		}
	}
}

// Check the FireMaster before actually firing
void CFuncTank::TryFire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	m_iState = STATE_IN_USE;
	if (UTIL_IsMasterTriggered(m_iszFireMaster, NULL))
	{
		Fire( barrelEnd, forward, pevAttacker );
	}
}

// Fire targets and spawn sprites
void CFuncTank::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	if ( m_fireLast != 0 )
	{
		if ( m_iszSpriteSmoke )
		{
			CSprite *pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteSmoke), barrelEnd, TRUE );
			pSprite->AnimateAndDie( RANDOM_FLOAT( 15.0, 20.0 ) );
			pSprite->SetTransparency( kRenderTransAlpha, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, 255, kRenderFxNone );
			pSprite->pev->velocity.z = RANDOM_FLOAT(40, 80);
			pSprite->SetScale( m_spriteScale );
		}
		if ( m_iszSpriteFlash )
		{
			CSprite *pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteFlash), barrelEnd, TRUE );
			pSprite->AnimateAndDie( 60 );
			pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
			pSprite->SetScale( m_spriteScale );
		}
		UTIL_FireTargets( pev->target, this, this, USE_TOGGLE );
	}
	m_fireLast = gpGlobals->time;
}

void CFuncTank::TankTrace( const Vector &vecStart, const Vector &vecForward, const Vector &vecSpread, TraceResult &tr )
{
	// get circular gaussian spread
	float x, y, z;
	do {
		x = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
		y = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
		z = x*x+y*y;
	} while (z > 1);
	Vector vecDir = vecForward +
		x * vecSpread.x * gpGlobals->v_right +
		y * vecSpread.y * gpGlobals->v_up;
	Vector vecEnd;
	
	vecEnd = vecStart + vecDir * 4096;
	UTIL_TraceLine( vecStart, vecEnd, dont_ignore_monsters, edict(), &tr );
}

	
void CFuncTank::StartRotSound( void )
{
	if ( !pev->noise || (pev->spawnflags & SF_TANK_SOUNDON) )
		return;
	pev->spawnflags |= SF_TANK_SOUNDON;
	EMIT_SOUND( edict(), CHAN_STATIC, (char*)STRING(pev->noise), 0.85, ATTN_NORM);
}


void CFuncTank::StopRotSound( void )
{
	if ( pev->spawnflags & SF_TANK_SOUNDON )
		STOP_SOUND( edict(), CHAN_STATIC, (char*)STRING(pev->noise) );
	pev->spawnflags &= ~SF_TANK_SOUNDON;
}

//============================================================================
// FUNC TANK
//============================================================================
class CFuncTankGun : public CFuncTank
{
public:
	void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
};
LINK_ENTITY_TO_CLASS( func_tank, CFuncTankGun );

void CFuncTankGun::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	int i;

	if ( m_fireLast != 0 )
	{
		// FireBullets needs gpGlobals->v_up, etc.
		UTIL_MakeAimVectors(pev->angles);

		int bulletCount = (gpGlobals->time - m_fireLast) * m_fireRate;
		if ( bulletCount > 0 )
		{
			for ( i = 0; i < bulletCount; i++ )
			{
				switch( m_bulletType )
				{
				case TANK_BULLET_9MM:
					FireBullets( 1, barrelEnd, forward, gTankSpread[m_spread], 4096, BULLET_9MM, 1, m_iBulletDamage, pevAttacker );
					break;

				case TANK_BULLET_MP5:
					FireBullets( 1, barrelEnd, forward, gTankSpread[m_spread], 4096, BULLET_MP5, 1, m_iBulletDamage, pevAttacker );
					break;

				case TANK_BULLET_12MM:
					FireBullets( 1, barrelEnd, forward, gTankSpread[m_spread], 4096, BULLET_12MM, 1, m_iBulletDamage, pevAttacker );
					break;

				default:
				case TANK_BULLET_NONE:
					break;
				}
			}
			CFuncTank::Fire( barrelEnd, forward, pevAttacker );
		}
	}
	else	CFuncTank::Fire( barrelEnd, forward, pevAttacker );
}


//============================================================================
// FUNC TANK LASER
//============================================================================
class CFuncTankLaser : public CFuncTank
{
public:
	void	Activate( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
	BOOL	IsLaserTank( void ) { return TRUE; }
	void	Think( void );
	void	PostActivate( void );
	void	PostSpawn( void );
	CLaser *GetLaser( void );

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

private:
	CLaser	*m_pLaser;
	float	m_laserTime;
};
LINK_ENTITY_TO_CLASS( func_tanklaser, CFuncTankLaser );

TYPEDESCRIPTION	CFuncTankLaser::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncTankLaser, m_pLaser, FIELD_CLASSPTR ),
	DEFINE_FIELD( CFuncTankLaser, m_laserTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CFuncTankLaser, CFuncTank );

void CFuncTankLaser::Activate( void )
{
	if ( !GetLaser() )
	{
		UTIL_Remove(this);
		Msg( "Error: Laser tank with no env_laser!\n" );
	}
	else
	{
		m_pLaser->TurnOff();
	}
	CFuncTank::Activate();
}

void CFuncTankLaser::PostSpawn( void )
{
	CFuncTank::PostSpawn();

	if(m_barrelPos == g_vecZero) //check for custom barrelend
	{
		if(GetLaser()) m_barrelPos = m_pLaser->pev->origin - pev->origin; //write offset
		else m_barrelPos = Vector( fabs(pev->absmax.x), 0, 0 ); //write default
		m_sightOrigin = BarrelPosition(); // Point at the end of the barrel
	}
}

void CFuncTankLaser::PostActivate( void )
{
	//set laser parent once only
	if(m_pLaser && m_pLaser->m_pParent != this) 
	{
		m_pLaser->SetParent( this );
	}
}

void CFuncTankLaser::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "laserentity"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CFuncTank::KeyValue( pkvd );
}


CLaser *CFuncTankLaser::GetLaser( void )
{
	if ( m_pLaser ) return m_pLaser;

	CBaseEntity	*pEntity;

	pEntity = UTIL_FindEntityByTargetname( NULL, STRING(pev->message) );
	while ( pEntity )
	{
		// Found the laser
		if ( FClassnameIs( pEntity->pev, "env_laser" ) )
		{
			m_pLaser = (CLaser *)pEntity;
			m_pLaser->pev->angles = pev->angles; //copy tank angles
			break;
		}
		else pEntity = UTIL_FindEntityByTargetname( pEntity, STRING(pev->message) );
	}

	return m_pLaser;
}


void CFuncTankLaser::Think( void )
{
	if ( m_pLaser && m_pLaser->GetState() == STATE_ON && (gpGlobals->time > m_laserTime)) 
		m_pLaser->TurnOff();
	CFuncTank::Think();
}

void CFuncTankLaser::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	int i;
	TraceResult tr;

	if ( m_fireLast != 0 && GetLaser() )
	{
		// TankTrace needs gpGlobals->v_up, etc.
		UTIL_MakeAimVectors(pev->angles);

		int bulletCount = (gpGlobals->time - m_fireLast) * m_fireRate;
		if ( bulletCount )
		{
			for ( i = 0; i < bulletCount; i++ )
			{
				m_pLaser->pev->origin = barrelEnd;
				TankTrace( barrelEnd, forward, gTankSpread[m_spread], tr );
				
				m_laserTime = gpGlobals->time + 0.1;
				m_pLaser->TurnOn();
				m_pLaser->pev->dmgtime = gpGlobals->time - 1.0;
			}
			CFuncTank::Fire( barrelEnd, forward, pev );
		}
	}
	else	CFuncTank::Fire( barrelEnd, forward, pev );

}

//============================================================================
// FUNC TANK ROCKET
//============================================================================
class CFuncTankRocket : public CFuncTank
{
public:
	void Precache( void );
	void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
	BOOL IsRocketTank( void ) { return TRUE; }
};
LINK_ENTITY_TO_CLASS( func_tankrocket, CFuncTankRocket );

void CFuncTankRocket::Precache( void )
{
	UTIL_PrecacheEntity( "rpg_rocket" );
	CFuncTank::Precache();
}

void CFuncTankRocket::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	int i;

	if ( m_fireLast != 0 )
	{
		int bulletCount = (gpGlobals->time - m_fireLast) * m_fireRate;
		if ( bulletCount > 0 )
		{
			for ( i = 0; i < bulletCount; i++ )
			{
				CBaseEntity *pRocket = CBaseEntity::Create( "rpg_rocket", barrelEnd, pev->angles, edict() );
			}
			CFuncTank::Fire( barrelEnd, forward, pev );
		}
	}
	else	CFuncTank::Fire( barrelEnd, forward, pev );
}

//============================================================================
// FUNC TANK MORTAR
//============================================================================
class CFuncTankMortar : public CFuncTank
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
};
LINK_ENTITY_TO_CLASS( func_tankmortar, CFuncTankMortar );


void CFuncTankMortar::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "iMagnitude"))
	{
		pev->impulse = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CFuncTank::KeyValue( pkvd );
}


void CFuncTankMortar::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	if ( m_fireLast != 0 )
	{
		int bulletCount = (gpGlobals->time - m_fireLast) * m_fireRate;
		// Only create 1 explosion
		if ( bulletCount > 0 )
		{
			TraceResult tr;

			// TankTrace needs gpGlobals->v_up, etc.
			UTIL_MakeAimVectors(pev->angles);

			TankTrace( barrelEnd, forward, gTankSpread[m_spread], tr );
                              
			UTIL_Explode( tr.vecEndPos, edict(), pev->impulse );
			CFuncTank::Fire( barrelEnd, forward, pev );
		}
	}
	else	CFuncTank::Fire( barrelEnd, forward, pev );
}


//============================================================================
// FUNC TANK CONTROLS
//============================================================================
void CFuncTankControls::Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
	UTIL_SetModel( ENT(pev), pev->model );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( this, pev->origin );
}

void CFuncTankControls :: KeyValue( KeyValueData *pkvd )
{
	if ( m_cTanks < MAX_MULTI_TARGETS ) 
	{
		// add this field to the target list
		// this assumes that additional fields are targetnames and their values are delay values.
		
		char tmp[128];
		UTIL_StripToken( pkvd->szKeyName, tmp );
		m_iTankName[ m_cTanks ] = ALLOC_STRING( tmp );
		m_cTanks++;
		pkvd->fHandled = TRUE;
	}
}

TYPEDESCRIPTION	CFuncTankControls::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncTankControls, m_pController, FIELD_CLASSPTR ),
	DEFINE_FIELD( CFuncTankControls, m_vecControllerUsePos, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTankControls, m_cTanks, FIELD_INTEGER ),
	DEFINE_ARRAY( CFuncTankControls, m_iTankName, FIELD_STRING, MAX_MULTI_TARGETS ),
};
IMPLEMENT_SAVERESTORE( CFuncTankControls, CBaseEntity );

BOOL CFuncTankControls :: OnControls( entvars_t *pevTest )
{
	Vector offset = pevTest->origin - pev->origin;

	if (m_pParent)
	{
		if ( ((m_vecControllerUsePos + m_pParent->pev->origin) - pevTest->origin).Length() <= 30 )
			return TRUE;
	}
	else if ( (m_vecControllerUsePos - pevTest->origin).Length() <= 30 ) return TRUE;
	return FALSE;
}

void CFuncTankControls :: HandleTank ( CBaseEntity *pActivator, CBaseEntity *m_pTank, BOOL activate )
{
	if(m_pTank->pev->flags & FL_TANK)//it's tank entity
	{
		if(activate)
		{
			if (((CFuncTank*)m_pTank)->StartControl((CBasePlayer*)pActivator, this))
			{
				m_iState = STATE_ON; //we have active tank!
			}
		}
		else ((CFuncTank*)m_pTank)->StopControl(this);
	}
}

void CFuncTankControls :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *tryTank = NULL;
	if ( useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		return;
	}

	if ( !m_pController && useType != USE_OFF )
	{
		if (!pActivator || !(pActivator->IsPlayer())) return;
		if (m_iState != STATE_OFF || ((CBasePlayer*)pActivator)->m_pTank != NULL) return;

		//find all specified tanks
		for(int i = 0; i < m_cTanks; i++)
		{
			//find all tanks with current name
			while( tryTank = UTIL_FindEntityByTargetname(tryTank, STRING(m_iTankName[i])))
			{
				HandleTank( pActivator, tryTank, TRUE );
			}			
		}
		if (m_iState == STATE_ON)
		{
			// we found at least one tank to use, so holster player's weapon
			m_pController = (CBasePlayer*)pActivator;
			m_pController->m_pTank = this;
			if ( m_pController->m_pActiveItem )
			{
				m_pController->m_pActiveItem->Holster();
				m_pController->pev->weaponmodel = 0;
				//viewmodel reset in tank
			}
			m_pController->m_iHideHUD |= HIDEHUD_WEAPONS;

			if (m_pParent)
				m_vecControllerUsePos = m_pController->pev->origin - m_pParent->pev->origin;
			else	m_vecControllerUsePos = m_pController->pev->origin;
		}
	}
	else if (m_pController && useType != USE_ON)
	{
		//find all specified tanks
		for(int i = 0; i < m_cTanks; i++)
		{
			//find all tanks with current name
			while( tryTank = UTIL_FindEntityByTargetname(tryTank, STRING(m_iTankName[i])))
			{
				HandleTank( pActivator, tryTank, FALSE );
			}			
		}

		// bring back player's weapons
		if ( m_pController->m_pActiveItem ) m_pController->m_pActiveItem->Deploy();

		m_pController->m_iHideHUD &= ~HIDEHUD_WEAPONS;
		m_pController->m_pTank = NULL;				

		m_pController = NULL;
		m_iState = STATE_OFF;
	}

}

LINK_ENTITY_TO_CLASS( func_tankcontrols, CFuncTankControls );