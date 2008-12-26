//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#define SF_TANK_ACTIVE			0x0001
#define SF_TANK_LINEOFSIGHT		0x0010
#define SF_TANK_CANCONTROL		0x0020
#define SF_TANK_LASERSPOT		0x0040 //LRC
#define SF_TANK_MATCHTARGET		0x0080 //LRC
#define SF_TANK_SOUNDON			0x8000
#define SF_TANK_SEQFIRE			0x10000 //LRC - a TankSequence is telling me to fire

static Vector gTankSpread[] =
{
	Vector( 0, 0, 0 ),			// perfect
	Vector( 0.025, 0.025, 0.025 ),	// small cone
	Vector( 0.05, 0.05, 0.05 ),		// medium cone
	Vector( 0.1, 0.1, 0.1 ),		// large cone
	Vector( 0.25, 0.25, 0.25 ),		// extra-large cone
};
#define MAX_FIRING_SPREADS ARRAYSIZE(gTankSpread)

enum TANKBULLET
{
	TANK_BULLET_NONE = 0,
	TANK_BULLET_9MM = 1,
	TANK_BULLET_MP5 = 2,
	TANK_BULLET_12MM = 3,
};

class CFuncTankControls;
class CFuncTank : public CBaseBrush
{
public:
	void	Spawn( void );
	void	PostSpawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	Think( void );
	void	TrackTarget( void );
	int	IRelationship( CBaseEntity* pTarget );
	int	Classify( void ) { return m_iTankClass; }

	void TryFire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
	virtual void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
	virtual Vector UpdateTargetPosition( CBaseEntity *pTarget )
	{
		return pTarget->BodyTarget( pev->origin );
	}

	void	StartRotSound( void );
	void	StopRotSound( void );
	
	inline BOOL IsActive( void ) { return (pev->spawnflags & SF_TANK_ACTIVE)?TRUE:FALSE; }
	inline void TankActivate( void ) { pev->spawnflags |= SF_TANK_ACTIVE; SetNextThink(0.1); m_fireLast = 0; }
	inline void TankDeactivate( void ) { pev->spawnflags &= ~SF_TANK_ACTIVE; m_fireLast = 0; StopRotSound(); }
	inline BOOL CanFire( void ) { return (gpGlobals->time - m_lastSightTime) < m_persist; }
	BOOL InRange( float range );
	void TankTrace( const Vector &vecStart, const Vector &vecForward, const Vector &vecSpread, TraceResult &tr );

	Vector BarrelPosition( void )
	{
		Vector forward, right, up;
		UTIL_MakeVectorsPrivate( pev->angles, forward, right, up );
		return pev->origin + (forward * m_barrelPos.x) + (right * m_barrelPos.y) + (up * m_barrelPos.z);
	}

	void	AdjustAnglesForBarrel( Vector &angles, float distance );
	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void UpdateSpot( void );
	BOOL StartControl( CBasePlayer* pController, CFuncTankControls* pControls );
	void StopControl( CFuncTankControls* pControls );
          
	CBaseEntity* BestVisibleEnemy( void );
	CFuncTankControls* m_pControls; 	// tankcontrols is used as a go-between.
	CLaserSpot*  m_pSpot;		// Laser spot entity

	virtual BOOL IsRocketTank( void ) { return FALSE; }
	virtual BOOL IsLaserTank( void ) { return FALSE; }

protected:
	float		m_flNextAttack;
	
	float		m_yawCenter;	// "Center" yaw
	float		m_yawRate;		// Max turn rate to track targets
	float		m_yawRange;		// Range of turning motion (one-sided: 30 is +/- 30 degress from center)
								// Zero is full rotation
	float		m_yawTolerance;	// Tolerance angle

	float		m_pitchCenter;	// "Center" pitch
	float		m_pitchRate;	// Max turn rate on pitch
	float		m_pitchRange;	// Range of pitch motion as above
	float		m_pitchTolerance;	// Tolerance angle

	float		m_lastSightTime;// Last time I saw target
	float		m_persist;		// Persistence of firing (how long do I shoot when I can't see)
	RandomRange	m_Range;
	float		m_minRange;		// Minimum range to aim/track
	float		m_maxRange;		// Max range to aim/track
	float		m_fireLast;		// Last time I fired
	float		m_fireRate;		// How many rounds/second

	Vector		m_barrelPos;	// Length of the freakin barrel
	float		m_spriteScale;	// Scale of any sprites we shoot
	int		m_iszSpriteSmoke;
	int		m_iszSpriteFlash;
	TANKBULLET	m_bulletType;	// Bullet type
	int		m_iBulletDamage; // 0 means use Bullet type's default damage
	
	Vector		m_sightOrigin;	// Last sight of target
	int		m_spread;		// firing spread
	int		m_iszMaster;	// Master entity
	int		m_iszFireMaster;//LRC - Fire-Master entity (prevents firing when inactive)

	int		m_iTankClass;	// Behave As
};

class CFuncTankControls : public CBaseLogic
{
public:
	virtual int ObjectCaps( void ) { return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE; }
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );
	void HandleTank ( CBaseEntity *pActivator, CBaseEntity *m_pTank, BOOL activate );

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	BOOL OnControls( entvars_t *pevTest );

	Vector	m_vecControllerUsePos; // where was the player standing when he used me?

	CBasePlayer* m_pController;
	//max 32 tanks allowed to control
	int m_iTankName [ MAX_MULTI_TARGETS ];// list if indexes into global string array
	int m_cTanks;// the total number of targets in this manager's fire list.
	EHANDLE m_hPlayer;
};