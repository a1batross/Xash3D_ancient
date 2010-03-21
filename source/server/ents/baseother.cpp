//=======================================================================
//			Copyright (C) XashXT Group 2007
//			    misc temporary entities
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "player.h"
#include "client.h"

//=======================================================================
// 		   sparkleent - explode post sparks
//=======================================================================
class CEnvShower : public CBaseEntity
{
	void Spawn( void );
	void Think( void );
	void Touch( CBaseEntity *pOther );
	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
};
LINK_ENTITY_TO_CLASS( sparkleent, CEnvShower );

void CEnvShower::Spawn( void )
{
	pev->velocity = RANDOM_FLOAT( 200, 300 ) * pev->angles;
	pev->velocity.x += RANDOM_FLOAT( -100.0f, 100.0f );
	pev->velocity.y += RANDOM_FLOAT( -100.0f, 100.0f );
	if ( pev->velocity.z >= 0 ) pev->velocity.z += 200;
	else pev->velocity.z -= 200;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->gravity = 0.5;
	SetNextThink( 0.1 );
	pev->solid = SOLID_NOT;
	UTIL_SetModel( edict(), "models/common/null.mdl");
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	pev->effects |= EF_NODRAW;
	pev->speed = RANDOM_FLOAT( 0.5f, 1.5f );
	pev->angles = g_vecZero;
}

void CEnvShower::Think( void )
{
	UTIL_Sparks( pev->origin );

	pev->speed -= 0.1;
	if ( pev->speed > 0 )
		SetNextThink( 0.1 );
	else UTIL_Remove( this );
	pev->flags &= ~FL_ONGROUND;
}

void CEnvShower::Touch( CBaseEntity *pOther )
{
	if( pev->flags & FL_ONGROUND )
		pev->velocity = pev->velocity * 0.1f;
	else pev->velocity = pev->velocity * 0.6f;

	if(( pev->velocity.x * pev->velocity.x + pev->velocity.y * pev->velocity.y ) < 10.0f )
		pev->speed = 0;
}

//====================================================================
//			ChangeLevelFire
//====================================================================
class ChangeLevelFire : public CBaseLogic
{
public:
	void PostActivate( void ) { UTIL_FireTargets( pev->target, this, this, USE_TOGGLE ); UTIL_Remove( this ); }
	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() | FCAP_FORCE_TRANSITION; }
};
LINK_ENTITY_TO_CLASS( fireent, ChangeLevelFire );

//=======================================================================
// 		   faderent - rendering time effects
//=======================================================================
class CRenderFxFader : public CBaseLogic
{
public:
	void Spawn ( void ) { m_hTarget = CBaseEntity::Instance( pev->owner ); }
	void Think ( void )
	{
		if (((CBaseEntity*)m_hTarget) == NULL)
		{
			UTIL_Remove( this );
			return;
		}

		float flDegree = (gpGlobals->time - pev->dmgtime)/pev->speed;
          
		if (flDegree >= 1)
		{
			m_hTarget->pev->renderamt = pev->renderamt + pev->health;
			m_hTarget->pev->rendercolor = pev->rendercolor + pev->movedir;
			m_hTarget->pev->scale = pev->scale + pev->frags;
			m_hTarget->pev->framerate = pev->framerate + pev->max_health;
			UTIL_FireTargets( pev->target, m_hTarget, this, USE_TOGGLE );
			m_hTarget = NULL;

			SetNextThink( 0.1 );
			SetThink(Remove);
		}
		else
		{
			m_hTarget->pev->renderamt = pev->renderamt + pev->health * flDegree;
			m_hTarget->pev->rendercolor.x = pev->rendercolor.x + pev->movedir.x * flDegree;
			m_hTarget->pev->rendercolor.y = pev->rendercolor.y + pev->movedir.y * flDegree;
			m_hTarget->pev->rendercolor.z = pev->rendercolor.z + pev->movedir.z * flDegree;
			m_hTarget->pev->scale = pev->scale + pev->frags * flDegree;
                    	m_hTarget->pev->framerate = pev->framerate + pev->max_health * flDegree;
			SetNextThink( 0.01 );//fader step
		}
	}
};
LINK_ENTITY_TO_CLASS( faderent, CRenderFxFader );

//=======================================================================
// 		   smokeent - temporary smoke
//=======================================================================
class CSmokeEnt : public CBaseAnimating
{
	void Spawn( void );
	void Think( void );
};
LINK_ENTITY_TO_CLASS( smokeent, CSmokeEnt );

void CSmokeEnt::Spawn( void )
{
	pev->solid	= SOLID_BBOX;
	pev->movetype	= MOVETYPE_NONE;
	pev->armorvalue	= gpGlobals->time;

	if( !pev->team ) pev->team = 50;
	if( pev->team > 250 ) pev->team = 250; // normalize and remember about random value 0 - 5
}

void CSmokeEnt::Think( void )
{
	if( pev->dmgtime )
	{
		if( pev->dmgtime < gpGlobals->time )
		{
			UTIL_Remove( this );
			return;
		}
		else if( RANDOM_FLOAT( 0, pev->dmgtime - pev->armorvalue ) > pev->dmgtime - gpGlobals->time )
		{
			SetNextThink( 0.2 );
			return;
		}
	}
	
	Vector VecSrc = UTIL_RandomVector( pev->absmin, pev->absmax );

	MESSAGE_BEGIN( MSG_PVS, gmsg.TempEntity, VecSrc );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( VecSrc.x );
		WRITE_COORD( VecSrc.y );
		WRITE_COORD( VecSrc.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( RANDOM_LONG( 0, 5 ) + pev->impulse ); // scale * 10
		WRITE_BYTE( RANDOM_LONG( 0, 3 ) + 8  ); // framerate
	MESSAGE_END();

	StudioFrameAdvance( );//animate model if present
	SetNextThink( 0.2 );
}

//=========================================================
//	func_platform floor indicator
//=========================================================
class CFloorEnt:public CPointEntity
{
	void Think( void );
	void PostActivate( void );
	CBasePlatform *pElevator;	// no need to save - PostActivate will be restore this
};

void CFloorEnt::PostActivate( void )
{
	pElevator = (CBasePlatform*)CBasePlatform::Instance( pev->owner );
	SetNextThink( 0 );
}

void CFloorEnt::Think ( void )
{
	if(pElevator && (pElevator->GetState() == STATE_TURN_ON || pElevator->GetState() == STATE_TURN_OFF) )
	{
		UTIL_FireTargets( pev->target, pElevator, this, USE_SET, pElevator->CalcFloor());
		SetNextThink( 0.01 );
	}
	else UTIL_Remove( this ); 
}
LINK_ENTITY_TO_CLASS( floorent, CFloorEnt );


//====================================================================
//			Laser pointer target
//====================================================================
CLaserSpot *CLaserSpot::CreateSpot( entvars_t *pevOwner )
{
	CLaserSpot *pSpot = GetClassPtr( (CLaserSpot *)NULL );
	pSpot->Spawn();
	pSpot->pev->classname = MAKE_STRING( "misc_laserdot" );

	if( pevOwner ) 
	{
		// predictable laserspot (cl_lw must be set to 1)
		pSpot->pev->flags |= FL_SKIPLOCALHOST;
		pSpot->pev->owner = ENT( pevOwner );
		pevOwner->effects |= EF_LASERSPOT;
	}
	return pSpot;
}

void CLaserSpot::Precache( void )
{
	UTIL_PrecacheModel( "sprites/laserdot.spr" );
	UTIL_PrecacheSound( "weapons/spot_on.wav" );
	UTIL_PrecacheSound( "weapons/spot_off.wav" ); 
}  

void CLaserSpot::Spawn( void )
{
	Precache( );

	// laser dot settings
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	pev->rendermode = kRenderGlow;
	pev->renderfx = kRenderFxNoDissipation;
	pev->renderamt = 255;
	pev->rendercolor = Vector( 200, 12, 12 );

	UTIL_SetModel( ENT( pev ), "sprites/laserdot.spr" );
	UTIL_SetOrigin( this, pev->origin );
}

void CLaserSpot::Suspend( float flSuspendTime )
{
	pev->effects |= EF_NODRAW;

	if( !FNullEnt( pev->owner ))
		pev->owner->v.effects &= ~EF_LASERSPOT;
	
	// -1 means suspend indefinitely
	if( flSuspendTime == -1 ) SetThink( NULL );
	else
	{
		SetThink( Revive );
		SetNextThink( flSuspendTime );
	}
}

void CLaserSpot::Revive( void )
{
	if( !FNullEnt( pev->owner ))
		pev->owner->v.effects |= EF_LASERSPOT;

	pev->effects &= ~EF_NODRAW;
	SetThink( NULL );
}

void CLaserSpot::UpdateOnRemove( void )
{
	// tell the owner about laserspot
	if( !FNullEnt( pev->owner ))
		pev->owner->v.effects &= ~EF_LASERSPOT;

	CBaseEntity :: UpdateOnRemove ();
}

void CLaserSpot::Update( CBasePlayer *m_pPlayer )
{
	TraceResult tr; 
		
	UTIL_MakeVectors( m_pPlayer->pev->viewangles );
	UTIL_TraceLine( m_pPlayer->GetGunPosition(), m_pPlayer->GetGunPosition() + gpGlobals->v_forward * 8192, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr );
	UTIL_SetOrigin( this, tr.vecEndPos );

	if( UTIL_PointContents( tr.vecEndPos ) == CONTENTS_SKY && VARS( tr.pHit )->solid == SOLID_BSP ) 
		pev->effects |= EF_NODRAW;
	else pev->effects &= ~EF_NODRAW;
}
LINK_ENTITY_TO_CLASS( misc_laserdot, CLaserSpot );