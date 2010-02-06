//=======================================================================
//			Copyright (C) Shambler Team 2006
//			    rockets.h - projectiles code
//=======================================================================
#include "baseweapon.h"

class CGrenade : public CBaseMonster
{
public:
	void Spawn( void );

	static CGrenade *ShootTimed( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, float time );
	static CGrenade *ShootContact( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );

	void Explode( Vector vecPos, int bitsDamageType, int contents );

	void EXPORT BounceTouch( CBaseEntity *pOther );
	void EXPORT SlideTouch( CBaseEntity *pOther );
	void EXPORT ExplodeTouch( CBaseEntity *pOther );
	void EXPORT DangerSoundThink( void );
	void EXPORT PreDetonate( void );
	void EXPORT Detonate( void );
	void EXPORT DetonateUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT TumbleThink( void );

	virtual void BounceSound( void );
	virtual int	BloodColor( void ) { return DONT_BLEED; }
	virtual void Killed( entvars_t *pevAttacker, int iGib );

	BOOL m_fRegisteredSound;// whether or not this grenade has issued its DANGER sound to the world sound list yet.
};

class CRpgRocket : public CGrenade
{
public:
	int	Save( CSave &save );
	int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
	void Spawn( void );
	void Precache( void );
	void EXPORT FollowThink( void );
	void EXPORT IgniteThink( void );
	void EXPORT RocketTouch( CBaseEntity *pOther );
	void Detonate( void );
	void CreateTrail( void  );
	static CRpgRocket *Create ( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CBasePlayerWeapon *pLauncher );

	int m_iTrail;
	float m_flIgniteTime;
	CBasePlayerWeapon *m_pLauncher;// pointer back to the launcher that fired me. 
	BOOL b_setup;
};

class CApacheHVR : public CGrenade
{
	void Spawn( void );
	void Precache( void );
	void EXPORT IgniteThink( void );
	void EXPORT AccelerateThink( void );

	int m_iTrail;
};

class CNukeExplode : public CBaseLogic
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT ExplodeThink( void );
	static CNukeExplode *Create ( Vector vecOrigin, CBaseEntity *pOwner );
	short m_usExplodeSprite;
	short m_usExplodeSprite2;
	entvars_t *pevOwner; // keep pointer to rocket owner
};

class CWHRocket : public CBaseAnimating
{
public:
	int	Save( CSave &save );
	int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
	void Spawn( void );
	void Precache( void );
	void CreateTrail( void );
	void EXPORT FollowThink( void );
	void EXPORT NukeTouch( CBaseEntity *pOther );
	void EXPORT RemoveRocket ( void );
	void CalculateVelocity ( void );
	void Detonate ( bool explode = 1 );
	static CWHRocket *Create ( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CBasePlayerWeapon *pLauncher, BOOL Control );
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	
	int m_iTrail, m_iBurst;// rocket trail
	CBasePlayerWeapon *m_pLauncher;// pointer back to the launcher that fired me. 
	CBasePlayer *m_pPlayer;
	Vector m_Center, forward;
	BOOL b_setup;
};

class CBolt : public CBaseAnimating
{
public:
	void Spawn( void );
	void Precache( void );
	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
	void EXPORT BubbleThink( void );
	void Touch( CBaseEntity *pOther );
	static CBolt *Create( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner );

	int m_iTrail;
};