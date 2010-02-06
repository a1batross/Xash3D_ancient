//=======================================================================
//			Copyright (C) Shambler Team 2006
//			    rockets.cpp - projectiles code
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "monsters.h"
#include "baseweapon.h"
#include "nodes.h"
#include "client.h"
#include "soundent.h"
#include "decals.h"
#include "defaults.h"


//===========================
//
//	Grenade code
//
//===========================
void CGrenade::Explode( Vector vecPos, int bitsDamageType, int contents )
{
	float	flRndSound;// sound randomizer

	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible
	pev->takedamage = DAMAGE_NO;

	if( contents != CONTENTS_SKY )
	{
		// silent remove in sky
		UTIL_Explode( vecPos, pev->owner, pev->impulse, pev->classname );

		flRndSound = RANDOM_FLOAT( 0 , 1 );
		switch ( RANDOM_LONG( 0, 2 ))
		{
		case 0: EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/debris1.wav", 0.55, ATTN_NORM ); break;
		case 1: EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/debris2.wav", 0.55, ATTN_NORM ); break;
		case 2: EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/debris3.wav", 0.55, ATTN_NORM ); break;
		}
	}

	pev->effects |= EF_NODRAW;
	SetThink( Remove );
	pev->velocity = g_vecZero;
	SetNextThink( 0.3 );
}

void CGrenade::Killed( entvars_t *pevAttacker, int iGib )
{
	Detonate( );
}

// Timed grenade, this think is called when time runs out.
void CGrenade::DetonateUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetThink( Detonate );
	SetNextThink( 0 );
}

void CGrenade::PreDetonate( void )
{
	CSoundEnt::InsertSound ( bits_SOUND_DANGER, pev->origin, 400, 0.3 );

	SetThink( Detonate );
	SetNextThink( 1 );
}

void CGrenade::Detonate( void )
{
	Vector	vecPos = pev->origin - pev->velocity.Normalize() * 32; // set expolsion pos
	int	contents = UTIL_PointContents( pev->origin + pev->velocity.Normalize() * 8 );

	Explode( vecPos, DMG_BLAST, contents );
}

void CGrenade::ExplodeTouch( CBaseEntity *pOther )
{
	pev->enemy = pOther->edict();

	Vector	vecPos = pev->origin - pev->velocity.Normalize() * 32; // set expolsion pos
	int	contents = UTIL_PointContents( pev->origin + pev->velocity.Normalize() * 8 );

	Explode( vecPos, DMG_BLAST, contents );
}

void CGrenade::DangerSoundThink( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	CSoundEnt::InsertSound ( bits_SOUND_DANGER, pev->origin + pev->velocity * 0.5, pev->velocity.Length( ), 0.2 );
	SetNextThink( 0.2 );

	if( pev->waterlevel != 0 && pev->watertype > CONTENTS_FLYFIELD )
	{
		pev->velocity = pev->velocity * 0.5;
	}
}

void CGrenade::BounceTouch( CBaseEntity *pOther )
{
	// don't hit the guy that launched this grenade
	if ( pOther->edict() == pev->owner )
		return;

	// only do damage if we're moving fairly fast
	if (m_flNextAttack < gpGlobals->time && pev->velocity.Length() > 100)
	{
		entvars_t *pevOwner = VARS( pev->owner );
		if( pevOwner )
		{
			TraceResult tr = UTIL_GetGlobalTrace( );
			ClearMultiDamage( );
			pOther->TraceAttack(pevOwner, 1, gpGlobals->v_forward, &tr, DMG_CLUB ); 
			ApplyMultiDamage( pev, pevOwner);
		}
		m_flNextAttack = gpGlobals->time + 1.0; // debounce
	}

	Vector vecTestVelocity;
	// pev->avelocity = Vector (300, 300, 300);

	// this is my heuristic for modulating the grenade velocity because grenades dropped purely vertical
	// or thrown very far tend to slow down too quickly for me to always catch just by testing velocity. 
	// trimming the Z velocity a bit seems to help quite a bit.
	vecTestVelocity = pev->velocity; 
	vecTestVelocity.z *= 0.45;

	if ( !m_fRegisteredSound && vecTestVelocity.Length() <= 60 )
	{
		//ALERT( at_console, "Grenade Registered!: %f\n", vecTestVelocity.Length() );

		// grenade is moving really slow. It's probably very close to where it will ultimately stop moving. 
		// go ahead and emit the danger sound.
		
		// register a radius louder than the explosion, so we make sure everyone gets out of the way
		CSoundEnt::InsertSound ( bits_SOUND_DANGER, pev->origin, pev->dmg / 0.4, 0.3 );
		m_fRegisteredSound = TRUE;
	}

	if ( pev->flags & FL_ONGROUND )
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.8;
		pev->sequence = 1;
	}
	else
	{
		// play bounce sound
		BounceSound();
	}
	pev->framerate = pev->velocity.Length() / 200.0;
	if( pev->framerate > 1.0 ) pev->framerate = 1;
	else if( pev->framerate < 0.5 ) pev->framerate = 0;

}

void CGrenade::SlideTouch( CBaseEntity *pOther )
{
	// don't hit the guy that launched this grenade
	if ( pOther->edict() == pev->owner )
		return;

	// pev->avelocity = Vector (300, 300, 300);

	if (pev->flags & FL_ONGROUND)
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.95;

		if( pev->velocity.x != 0 || pev->velocity.y != 0 )
		{
			// maintain sliding sound
		}
	}
	else	BounceSound();
}

void CGrenade :: BounceSound( void )
{
	switch( RANDOM_LONG( 0, 2 ))
	{
	case 0: EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/grenade/hit1.wav", 0.25, ATTN_NORM ); break;
	case 1: EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/grenade/hit2.wav", 0.25, ATTN_NORM ); break;
	case 2: EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/grenade/hit3.wav", 0.25, ATTN_NORM ); break;
	}
}

void CGrenade :: TumbleThink( void )
{
	if( !IsInWorld( ))
	{
		UTIL_Remove( this );
		return;
	}

	StudioFrameAdvance( );
	SetNextThink( 0.1 );

	if ( pev->dmgtime - 1 < gpGlobals->time )
	{
		CSoundEnt::InsertSound ( bits_SOUND_DANGER, pev->origin + pev->velocity * (pev->dmgtime - gpGlobals->time), 400, 0.1 );
	}

	if (pev->dmgtime <= gpGlobals->time)
	{
		SetThink( Detonate );
	}
	if( pev->waterlevel != 0 && pev->watertype > CONTENTS_FLYFIELD )
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2;
	}
}


void CGrenade:: Spawn( void )
{
	pev->movetype = MOVETYPE_BOUNCE;
	pev->classname = MAKE_STRING( "grenade" );
	
	pev->solid = SOLID_BBOX;

	SetObjectClass( ED_NORMAL );
	UTIL_SetModel( ENT( pev ), "models/props/hgrenade.mdl");

	pev->dmg = 100;
	m_fRegisteredSound = FALSE;
}


CGrenade *CGrenade::ShootContact( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity )
{
	CGrenade *pGrenade = GetClassPtr( (CGrenade *)NULL );
	pGrenade->Spawn();
	// contact grenades arc lower
	pGrenade->pev->gravity = 0.5;// lower gravity since grenade is aerodynamic and engine doesn't know it.
	UTIL_SetOrigin( pGrenade, vecStart );
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = UTIL_VecToAngles (pGrenade->pev->velocity);
	pGrenade->pev->owner = ENT(pevOwner);
	pGrenade->pev->flags |= FL_PROJECTILE;
	
	// make monsters afaid of it while in the air
	pGrenade->SetThink( DangerSoundThink );
	pGrenade->SetNextThink( 0 );
	
	// Tumble in air
	pGrenade->pev->avelocity.x = RANDOM_FLOAT ( -100, -500 );
	
	// Explode on contact
	pGrenade->SetTouch( ExplodeTouch );

	UTIL_SetModel( ENT( pGrenade->pev ), "models/props/grenade.mdl");
	UTIL_SetSize( pGrenade->pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ));
	pGrenade->pev->dmg = M203_DMG;

	return pGrenade;
}


CGrenade * CGrenade:: ShootTimed( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, float time )
{
	CGrenade *pGrenade = GetClassPtr( (CGrenade *)NULL );

	pGrenade->pev->sequence = RANDOM_LONG( 3, 6 );
	pGrenade->pev->origin = vecStart;
	pGrenade->Spawn();
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = UTIL_VecToAngles( pGrenade->pev->velocity );
	pGrenade->pev->owner = ENT(pevOwner);
	
	pGrenade->SetTouch( BounceTouch );	// Bounce if touched
	
	// Take one second off of the desired detonation time and set the think to PreDetonate. PreDetonate
	// will insert a DANGER sound into the world sound list and delay detonation for one second so that 
	// the grenade explodes after the exact amount of time specified in the call to ShootTimed(). 

	pGrenade->pev->dmgtime = gpGlobals->time + time;
	pGrenade->SetThink( TumbleThink );
	pGrenade->SetNextThink( 0.1 );

	if( time < 0.1f )
	{
		pGrenade->SetNextThink( 0 );
		pGrenade->pev->velocity = Vector( 0, 0, 0 );
	}
		
	pGrenade->pev->framerate = 1.0;
          pGrenade->pev->flags |= FL_PROJECTILE;
          
	// Tumble through the air
	pGrenade->pev->avelocity.x = -400;

	pGrenade->pev->gravity = 0.5;
	pGrenade->pev->friction = 0.8;
	pGrenade->pev->scale = 0.5;	// original Valve model is too big :)

	UTIL_SetModel( ENT( pGrenade->pev ), "models/props/hgrenade.mdl" );
	UTIL_SetSize( pGrenade->pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ));
	pGrenade->pev->dmg = 100;

	return pGrenade;
}

LINK_ENTITY_TO_CLASS( grenade, CGrenade );

//===========================
//
//	Rocket code
//
//===========================
CRpgRocket *CRpgRocket::Create ( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CBasePlayerWeapon *pLauncher )
{
	CRpgRocket *pRocket = GetClassPtr( (CRpgRocket *)NULL );

	pRocket->pev->origin = vecOrigin;
	pRocket->pev->angles = vecAngles;
	pRocket->Spawn();
	pRocket->SetTouch( RocketTouch );
	pRocket->m_pLauncher = pLauncher;// remember what RPG fired me. 
	pRocket->m_pLauncher->m_cActiveRocket++;// register this missile as active for the launcher
	pRocket->pev->owner = pOwner->edict();
          pRocket->pev->flags |= FL_PROJECTILE;

	return pRocket;
}

TYPEDESCRIPTION	CRpgRocket::m_SaveData[] = 
{
	DEFINE_FIELD( CRpgRocket, m_flIgniteTime, FIELD_TIME ),
	DEFINE_FIELD( CRpgRocket, m_pLauncher, FIELD_CLASSPTR ),
};
IMPLEMENT_SAVERESTORE( CRpgRocket, CGrenade );

void CRpgRocket :: Spawn( void )
{
	Precache( );

	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	UTIL_SetModel(ENT(pev), "models/props/rocket.mdl");
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( this, pev->origin );

	pev->classname = MAKE_STRING( "rpg_rocket" );

	SetThink( IgniteThink );
	SetTouch( RocketTouch );

	pev->angles.x -= 30;
	UTIL_MakeVectors( pev->angles );
	pev->angles.x = -(pev->angles.x + 30);
	pev->velocity = gpGlobals->v_forward * 250;
	pev->gravity = 0.5;

	SetNextThink( 0.4 );
	pev->dmg = RPG_ROCKET_DMG;
}

void CRpgRocket :: RocketTouch ( CBaseEntity *pOther )
{
	CBaseEntity *pPlayer = CBaseEntity::Instance(pev->owner);
	if ( m_pLauncher ) m_pLauncher->m_cActiveRocket--;
	STOP_SOUND( edict(), CHAN_VOICE, "weapons/rpg/rocket1.wav" );

	ExplodeTouch( pOther );
}

void CRpgRocket::Detonate( void )
{
	CBaseEntity *pPlayer = CBaseEntity::Instance(pev->owner);
	if ( m_pLauncher ) m_pLauncher->m_cActiveRocket--;
	STOP_SOUND( edict(), CHAN_VOICE, "weapons/rpg/rocket1.wav" );

	CGrenade :: Detonate();
}

void CRpgRocket :: Precache( void )
{
	UTIL_PrecacheModel( "models/props/rocket.mdl" );
	UTIL_PrecacheSound( "weapons/rpg/rocket1.wav" );
	UTIL_PrecacheSound( "weapons/rpg/beep.wav" );
	UTIL_PrecacheSound( "weapons/rpg/beep2.wav" );
	m_iTrail = UTIL_PrecacheModel( "sprites/smoke.spr" );
}

void CRpgRocket :: IgniteThink( void  )
{
	pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;

	CreateTrail();
	m_flIgniteTime = gpGlobals->time;

	// set to follow laser spot
	SetThink( FollowThink );
	SetNextThink( 0.1 );
}

void CRpgRocket :: CreateTrail( void  )
{
	if(b_setup) return;

	// make rocket sound after save\load
	EMIT_SOUND( ENT(pev), CHAN_VOICE, "weapons/rpg/rocket1.wav", 1, 0.5 );

	// restore rocket trail
	SFX_Trail(entindex(), m_iTrail);
	b_setup = TRUE;
}

void CRpgRocket :: FollowThink( void  )
{
	CBaseEntity *pOther = NULL;
	Vector vecTarget;
	Vector vecDir;
	float flDist, flMax, flDot;
	TraceResult tr;
	UTIL_MakeAimVectors( pev->angles );

	CreateTrail();

	vecTarget = gpGlobals->v_forward;
	flMax = 4096;
	// Examine all entities within a reasonable radius
	while ((pOther = UTIL_FindEntityByClassname( pOther, "misc_laserdot" )) != NULL)
	{
		UTIL_TraceLine ( pev->origin, pOther->pev->origin, dont_ignore_monsters, ENT(pev), &tr );
		// Msg( "%f\n", tr.flFraction );
		if (tr.flFraction >= 0.90)
		{
			vecDir = pOther->pev->origin - pev->origin;
			flDist = vecDir.Length( );
			vecDir = vecDir.Normalize( );
			flDot = DotProduct( gpGlobals->v_forward, vecDir );
			if ((flDot > 0) && (flDist * (1 - flDot) < flMax))
			{
				flMax = flDist * (1 - flDot);
				vecTarget = vecDir;
			}
		}
	}

	pev->angles = UTIL_VecToAngles( vecTarget );

	// this acceleration and turning math is totally wrong, but it seems to respond well so don't change it.
	float flSpeed = pev->velocity.Length();
	if (gpGlobals->time - m_flIgniteTime < 1.0)
	{
		pev->velocity = pev->velocity * 0.2 + vecTarget * (flSpeed * 0.8 + 400);
		if ( pev->waterlevel == 3 && pev->watertype > CONTENTS_FLYFIELD )
		{
			// go slow underwater
			if (pev->velocity.Length() > 300)
			{
				pev->velocity = pev->velocity.Normalize() * 300;
			}
			UTIL_BubbleTrail( pev->origin - pev->velocity * 0.1, pev->origin, 4 );
		} 
		else 
		{
			if (pev->velocity.Length() > 2000)
			{
				pev->velocity = pev->velocity.Normalize() * 2000;
			}
		}
	}
	else
	{
		if (pev->effects & EF_LIGHT)
		{
			pev->effects = 0;
			STOP_SOUND( ENT(pev), CHAN_VOICE, "weapons/rpg/rocket1.wav" );
		}
		pev->velocity = pev->velocity * 0.2 + vecTarget * flSpeed * 0.798;
		if ((pev->waterlevel == 0 || pev->watertype == CONTENTS_FOG) && pev->velocity.Length() < 1500)
		{
			Detonate( );
		}
	}
	//Msg( "%.0f\n", flSpeed );

	SetNextThink( 0.05 );
}
LINK_ENTITY_TO_CLASS( rpg_rocket, CRpgRocket );

//===========================
//	apache HVR rocket
//===========================
void CApacheHVR :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	UTIL_SetModel(ENT(pev), "models/HVR.mdl");
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( this, pev->origin );

	SetThink( IgniteThink );
	SetTouch( ExplodeTouch );

	UTIL_MakeAimVectors( pev->angles );
	pev->movedir = gpGlobals->v_forward;
	pev->gravity = 0.5;

	SetNextThink( 0.1 );

	pev->dmg = 150;
}

void CApacheHVR :: Precache( void )
{
	UTIL_PrecacheModel("models/HVR.mdl");
	m_iTrail = UTIL_PrecacheModel("sprites/smoke.spr");
	UTIL_PrecacheSound("weapons/rocket1.wav");
}

void CApacheHVR :: IgniteThink( void  )
{
	// pev->movetype = MOVETYPE_TOSS;

	// pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 1, 0.5 );

	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, gmsg.TempEntity );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(m_iTrail );	// model
		WRITE_BYTE( 15 ); // life
		WRITE_BYTE( 5 );  // width
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)

	// set to accelerate
	SetThink( AccelerateThink );
	SetNextThink( 0.1 );
}


void CApacheHVR :: AccelerateThink( void  )
{
	// check world boundaries
	if(!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	// accelerate
	float flSpeed = pev->velocity.Length();
	if (flSpeed < 1800)
	{
		pev->velocity = pev->velocity + pev->movedir * 200;
	}

	// re-aim
	pev->angles = UTIL_VecToAngles( pev->velocity );

	SetNextThink( 0.1 );
}
LINK_ENTITY_TO_CLASS( hvr_rocket, CApacheHVR );

//===========================
//	Nuclear explode code
//===========================
CNukeExplode *CNukeExplode::Create ( Vector vecOrigin, CBaseEntity *pOwner )
{
	CNukeExplode *pNuke = GetClassPtr( (CNukeExplode *)NULL );
	pNuke->pev->origin = vecOrigin;
	pNuke->Spawn();
	pNuke->pev->classname = MAKE_STRING( "nuclear_explode" );
	pNuke->pevOwner = pOwner->pev;

	return pNuke;
}

void CNukeExplode :: Spawn( void )
{
	Precache();
	UTIL_SetModel( ENT( pev ), "models/props/nexplode.mdl" );	
	TraceResult tr;
	UTIL_TraceLine ( pev->origin, pev->origin + Vector ( 0, 0, -32 ),  ignore_monsters, ENT(pev), &tr);
	UTIL_ScreenShake( pev->origin, 16, 1, 2, 1700 );
	pev->oldorigin = tr.vecEndPos + (tr.vecPlaneNormal * 30); // save normalized position

	// create first explode sprite
	SFX_Explode( m_usExplodeSprite, pev->oldorigin, 100, TE_EXPLFLAG_NOPARTICLES|TE_EXPLFLAG_NOSOUND );
	EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/warhead/whexplode.wav", 1, ATTN_NONE );

	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 190;
	pev->scale = 0.5;
	UTIL_SetOrigin( this, pev->origin );

	SetThink( ExplodeThink );

	pev->dmg = REDEEMER_ROCKET_DMG;
	SetNextThink( 0 );
}

void CNukeExplode :: Precache( void )
{
	m_usExplodeSprite = UTIL_PrecacheModel( "sprites/war_explo01.spr" );
	m_usExplodeSprite2 = UTIL_PrecacheModel( "sprites/war_explo02.spr" );
	UTIL_PrecacheModel( "models/props/nexplode.mdl" );
	UTIL_PrecacheSound( "weapons/warhead/whexplode.wav" );
}

void CNukeExplode :: ExplodeThink( void )
{
	pev->renderamt = UTIL_Approach( 0, pev->renderamt, 200 * gpGlobals->frametime );
 	pev->scale = UTIL_Approach( 30, pev->scale, 30 * gpGlobals->frametime );

	if( pev->scale >= 8 && pev->scale < 8.2 ) // create second explode sprite
		SFX_Explode( m_usExplodeSprite2, pev->oldorigin, 100, TE_EXPLFLAG_NOPARTICLES|TE_EXPLFLAG_NOSOUND );

	pev->owner = NULL; // can't traceline attack owner if this is set

	::RadiusDamage( pev->origin, pev, pevOwner, pev->renderamt/2, pev->scale * 30, CLASS_NONE, DMG_BLAST|DMG_NUCLEAR );
	if( pev->scale > 25 ) UTIL_Remove( this );

	SetNextThink( 0.01 );
}
LINK_ENTITY_TO_CLASS( nuclear_explode, CNukeExplode );

//===========================
//	Nuke rocket code
//===========================
CWHRocket *CWHRocket::Create( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CBasePlayerWeapon *pLauncher, BOOL Control )
{
	CWHRocket *pRocket = GetClassPtr( (CWHRocket *)NULL );

	pRocket->pev->origin = vecOrigin;
	pRocket->pev->angles = vecAngles;
	pRocket->m_pLauncher = pLauncher;	// remember what RPG fired me. 
	pRocket->pev->owner = pOwner->edict();
          pRocket->pev->button = Control;	// memeber rocket type         
	pRocket->pev->classname = MAKE_STRING( "nuke_rocket" );
          pRocket->m_pLauncher->m_cActiveRocket++;// register rocket
	pRocket->Spawn();
	pRocket->pev->flags |= FL_PROJECTILE;
		
	return pRocket;
}

TYPEDESCRIPTION CWHRocket::m_SaveData[] = 
{
	DEFINE_FIELD( CWHRocket, m_pLauncher, FIELD_CLASSPTR ),
	DEFINE_FIELD( CWHRocket, m_pPlayer, FIELD_CLASSPTR ),
};
IMPLEMENT_SAVERESTORE( CWHRocket, CBaseAnimating );


void CWHRocket :: Spawn( void )
{
	Precache( );

	m_pPlayer = (CBasePlayer*)CBasePlayer::Instance( pev->owner );
	if( !m_pPlayer ) // leveldesigner may put rocket on a map
	{
		if( !IsMultiplayer())
		{
			ALERT( at_warning, "player pointer is not valid\n" );
			m_pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( 1 );	
		}
		else
		{
			UTIL_Remove( this );
			return;
		}
	}
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_SLIDEBOX;
	pev->takedamage = DAMAGE_YES;
	pev->health = 10;
          pev->speed = WARHEAD_SPEED; // set initial speed
	
	UTIL_SetModel( ENT( pev ), "models/props/whrocket.mdl" );
	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/warhead/launch.wav", 1, 0.5 );
	UTIL_SetOrigin( this, pev->origin );
		
	SetThink( FollowThink );
	SetTouch( NukeTouch );

	UTIL_MakeVectors( pev->angles );
	pev->angles.x = -(pev->angles.x);
	pev->velocity = gpGlobals->v_forward * pev->speed;
	m_Center = pev->angles;

	if( pev->button )
	{
		UTIL_SetView( m_pPlayer, this, CAMERA_ON|INVERSE_X );
		m_pLauncher->m_iOnControl = 1; // start controlling
		m_pPlayer->m_iWarHUD = 1;	// enable warhead HUD
	}
	SetNextThink( 0.1 );
}

void CWHRocket :: Precache( void )
{
	UTIL_PrecacheModel("models/props/whrocket.mdl");
	UTIL_PrecacheSound("weapons/warhead/launch.wav");
	UTIL_PrecacheSound("weapons/warhead/whfly.wav");
	UTIL_PrecacheEntity( "nuclear_explode" );//explode
	m_iTrail = UTIL_PrecacheAurora("whtrail");
	m_iBurst = UTIL_PrecacheAurora("whburst");
	b_setup = FALSE;
}

void CWHRocket :: CreateTrail( void )
{
	if( b_setup ) return; // restore function
	EMIT_SOUND( ENT(pev), CHAN_VOICE, "weapons/warhead/whfly.wav", 1, 0.5 );
	UTIL_SetAurora( this, m_iTrail );
	UTIL_SetAurora( this, m_iBurst );
          pev->renderfx = kRenderFxAurora;
	b_setup = TRUE;
}

void CWHRocket :: FollowThink( void  )
{
	Vector angles, velocity;//private angles & velocity to transform
	CreateTrail();
	
	if( pev->button ) // controllable rocket
	{
		UTIL_MakeVectorsPrivate( m_pPlayer->pev->viewangles, forward, NULL, NULL );

		angles = m_pPlayer->pev->viewangles;
		angles.x = -angles.x;
		float steer = WARHEAD_MAX_SPEED / pev->speed; // steer factor
		
		angles.x = m_Center.x + UTIL_AngleDistance( angles.x, m_Center.x );
		angles.y = m_Center.y + UTIL_AngleDistance( angles.y, m_Center.y );
      		angles.z = m_Center.z + UTIL_AngleDistance( angles.z, m_Center.z );

		float distX = UTIL_AngleDistance( angles.x, pev->angles.x );
		pev->avelocity.x = distX * steer;

		float distY = UTIL_AngleDistance( angles.y, pev->angles.y );
		pev->avelocity.y = distY * steer;

		float distZ = UTIL_AngleDistance( angles.z, pev->angles.z );
		pev->avelocity.z = distY * -steer + distZ * steer;

		pev->velocity = forward * pev->speed + pev->avelocity;
		if( m_pLauncher && m_pLauncher->m_iOnControl == 2 )
		{
			Detonate(); // check for himself destroy
			return;
		}
	}
          CalculateVelocity();

          //Msg("Speed %.f\n", pev->velocity.Length());
	SetNextThink( 0.1 );
}

void CWHRocket::CalculateVelocity ( void )
{
	if(pev->waterlevel == 3)//go slow underwater
	{
		if (pev->speed > WARHEAD_SPEED_UNDERWATER) pev->speed -= 30;
		GetAttachment( 0, pev->oldorigin, pev->movedir );
		UTIL_BubbleTrail( pev->oldorigin - pev->velocity * 0.1, pev->oldorigin, 4 );
		if( pev->button ) m_pPlayer->m_iWarHUD = 2;
          }
	else
	{
		pev->speed += 5; // accelerate rocket
		if( pev->button ) m_pPlayer->m_iWarHUD = 1;
	}
	if( pev->speed > WARHEAD_SPEED )
		pev->velocity = pev->velocity.Normalize() * WARHEAD_SPEED;
}

int CWHRocket::TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	pev->health -= flDamage;//calculate health
	if (pev->health <= 0) Detonate();
	return 1;
}

void CWHRocket :: NukeTouch ( CBaseEntity *pOther )
{
	pev->enemy = pOther->edict(); //save enemy

	Vector vecAngles( pev->angles );
	vecAngles.x = -vecAngles.x;

	UTIL_MakeVectors( vecAngles );

	// check for sky
	if( UTIL_PointContents( pev->origin + gpGlobals->v_forward * 32 ) == CONTENTS_SKY )
		Detonate( FALSE ); // silent remove in sky
	else Detonate();
	
	SetNextThink( 0.7f );
}

void CWHRocket :: Detonate ( bool explode )
{
	// NOTE: Player can controlled one rocket at moment
	// but non controlled rocket don't reset this indicator
	if( pev->button ) m_pPlayer->m_iWarHUD = 3; // make static noise

	// launcher callback
	if( m_pLauncher ) m_pLauncher->m_cActiveRocket--;

	pev->takedamage = DAMAGE_NO;
	pev->velocity = g_vecZero;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	pev->model = iStringNull; // invisible

	STOP_SOUND( edict(), CHAN_VOICE, "weapons/warhead/whfly.wav" );//stop flying sound

	if( explode ) 
	{
		// make nuclear explosion if needed
		CNukeExplode::Create( pev->origin, m_pPlayer );
		SetThink( RemoveRocket );
		SetNextThink( 0.7 );
	}
	else
	{
		SetThink( RemoveRocket );
		SetNextThink( 0.7 );	
	}
}
void CWHRocket :: RemoveRocket ( void )
{
	if( m_pLauncher ) m_pLauncher->m_iOnControl = 0; //stop controlling 
	if(pev->button)
	{
		m_pPlayer->m_iWarHUD = 0;//reset HUD
		UTIL_SetView(m_pPlayer, iStringNull, 0 );//reset view
	}

	SetThink( Remove );
	SetNextThink( 0.1 );	
}
LINK_ENTITY_TO_CLASS( nuke_rocket, CWHRocket );


//===========================
//	crossbow bolt
//===========================
CBolt *CBolt::Create( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner )
{
	// Create a new entity with CBolt private data
	CBolt *pBolt = GetClassPtr( (CBolt *)NULL );
	pBolt->pev->classname = MAKE_STRING("bolt");
	pBolt->Spawn();
	pBolt->pev->origin = vecOrigin;
	pBolt->pev->angles = vecAngles;
	pBolt->pev->owner = pOwner->edict();
          pBolt->pev->avelocity.z = 10;

	return pBolt;
}

void CBolt::Spawn( )
{
	Precache( );
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	pev->gravity = 0.5;
	UTIL_SetModel(ENT(pev), "models/crossbow_bolt.mdl");

	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));

	EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/xbow_fire1.wav", 1, ATTN_NORM);

	SetThink( BubbleThink );
	SetNextThink( 0.2 );
}

void CBolt::Precache( )
{
	UTIL_PrecacheModel("models/crossbow_bolt.mdl");
	UTIL_PrecacheSound("weapons/xbow_hitbod1.wav");
	UTIL_PrecacheSound("weapons/xbow_hitbod2.wav");
	UTIL_PrecacheSound("weapons/xbow_hit1.wav");
	UTIL_PrecacheSound("weapons/xbow_fire1.wav");
}

void CBolt::Touch( CBaseEntity *pOther )
{
	SetTouch( NULL );

	if (pOther->IsMonster() || pOther->IsPlayer())
	{
		TraceResult tr = UTIL_GetGlobalTrace( );
		entvars_t	*pevOwner;

		pevOwner = VARS( pev->owner );
		ClearMultiDamage( );
		pOther->TraceAttack(pevOwner, BOLT_DMG, pev->velocity.Normalize(), &tr, DMG_NEVERGIB ); 
		ApplyMultiDamage( pev, pevOwner );

		pev->velocity = Vector( 0, 0, 0 );
		
		// play body "thwack" sound
		switch( RANDOM_LONG(0,1) )
		{
		case 0: EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM); break;
		case 1: EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM); break;
		}

		Killed( pev, GIB_NEVER );
	}
	else
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "weapons/xbow_hit1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,7));

		SetNextThink( 0 );

		Vector vecDir = pev->velocity.Normalize( );
		UTIL_SetOrigin( this, pev->origin - vecDir * 12 );
		pev->angles = UTIL_VecToAngles( vecDir );
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = Vector( 0, 0, 0 );
		pev->avelocity.z = 0;
		pev->angles.z = RANDOM_LONG(0, 360);
			
		if ( pOther == g_pWorld ) SetThink( PVSRemove );
                    else if( pOther->IsBSPModel())
                    {
                    	SetParent( pOther );//glue bolt with parent system
                    	SetThink( PVSRemove );
                    }
		else SetThink( Remove );
		if( UTIL_PointContents( pev->origin ) != CONTENTS_WATER )
			UTIL_Sparks( pev->origin );
	}
}

void CBolt::BubbleThink( void )
{
	if (pev->waterlevel == 3) UTIL_BubbleTrail( pev->origin - pev->velocity * 0.1, pev->origin, 1 );
	SetNextThink( 0.1 );
}

LINK_ENTITY_TO_CLASS( bolt, CBolt );