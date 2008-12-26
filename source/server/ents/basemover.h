//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================
#ifndef BASEMOVER_H
#define BASEMOVER_H

//rotating brush flags
#define SF_BRUSH_ROTATE_Z_AXIS		4
#define SF_BRUSH_ROTATE_X_AXIS		8

//door flags
#define SF_DOOR_START_OPEN			1
#define SF_DOOR_ROTATE_BACKWARDS		2
#define SF_DOOR_PASSABLE			8
#define SF_DOOR_ONEWAY			16
#define SF_DOOR_NO_AUTO_RETURN		32
#define SF_DOOR_ROTATE_Z			64
#define SF_DOOR_ROTATE_X			128
#define SF_DOOR_USE_ONLY			256
#define SF_DOOR_NOMONSTERS			512

// Tracktrain spawn flags
#define SF_TRACKTRAIN_NOPITCH		0x0001
#define SF_TRACKTRAIN_NOCONTROL		0x0002
#define SF_TRACKTRAIN_FORWARDONLY	0x0004
#define SF_TRACKTRAIN_PASSABLE		0x0008
#define SF_TRACKTRAIN_NOYAW			0x0010		//LRC
#define SF_TRACKTRAIN_AVELOCITY		0x800000	//LRC - avelocity has been set manually, don't turn.
#define SF_TRACKTRAIN_AVEL_GEARS	0x400000	//LRC - avelocity should be scaled up/down when the train changes gear.

// Spawnflag for CPathTrack
#define SF_PATH_DISABLED		0x00000001
#define SF_PATH_FIREONCE		0x00000002
#define SF_PATH_ALTREVERSE		0x00000004
#define SF_PATH_DISABLE_TRAIN	0x00000008
#define SF_PATH_ALTERNATE		0x00008000
#define SF_PATH_AVELOCITY		0x00080000 //LRC

//LRC - values in 'armortype'
#define PATHSPEED_SET			0
#define PATHSPEED_ACCEL			1
#define PATHSPEED_TIME			2
#define PATHSPEED_SET_MASTER	3

class CBaseMover : public CBaseBrush
{
public:
	void KeyValue( KeyValueData *pkvd );
	virtual void AxisDir( void );
	void (CBaseMover::*m_pfnCallWhenMoveDone)(void); //custom movedone function
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
	float	m_flMoveDistance;//rotating distance
	float	m_flBlockedTime; //set blocked refresh time
	int	m_iMode;//style of working
	float	m_flValue;//value to send
	float	m_flHeight;
	float	m_flLip;

	Vector	m_vecPosition1;	//startpos
	Vector	m_vecPosition2;	//endpos
	Vector	m_vecAngle1;	//startangle
	Vector	m_vecAngle2;	//endangle
	Vector	m_vecFinalDest;	//basemover finalpos
	Vector	m_vecFinalAngle;    //basemover finalangle
	Vector	m_vecFloor;	//basemover dest floor
	float	m_flLinearMoveSpeed;//member linear speed
	float	m_flAngularMoveSpeed;//member angular speed

	// common member functions
	void LinearMove ( Vector vecInput, float flSpeed );
	void AngularMove( Vector vecDestAngle, float flSpeed );
	void ComplexMove( Vector vecInput, Vector vecDestAngle, float flSpeed );
	void EXPORT LinearMoveNow( void ); 
	void EXPORT AngularMoveNow( void );
	void EXPORT ComplexMoveNow( void );
	void EXPORT LinearMoveDone( void );
	void EXPORT AngularMoveDone( void );
	void EXPORT ComplexMoveDone( void );
	void EXPORT LinearMoveDoneNow( void );
	void EXPORT AngularMoveDoneNow( void );
	void EXPORT ComplexMoveDoneNow( void );
};

class CBaseDoor : public CBaseMover
{
public:
	void Spawn( void );
	void Precache( void );
	virtual void PostSpawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	BOOL IsWater( void ){ return pev->skin != 0; };
	void EXPORT DoorTouch( CBaseEntity *pOther );
	virtual void Blocked( CBaseEntity *pOther );
	virtual int	ObjectCaps( void ) 
	{ 
		if ( FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY ))//door without name player can direct using
			return (CBaseMover::ObjectCaps() & ~FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE );
		else	return (CBaseMover::ObjectCaps() & ~FCAP_ACROSS_TRANSITION);
	};

	// local functions
	void EXPORT DoorGoUp( void );
	void EXPORT DoorGoDown( void );
	void EXPORT DoorHitTop( void );
	void EXPORT DoorHitBottom( void );
	virtual BOOL IsRotatingDoor( void ){ return FALSE; };
};

class CRotDoor : public CBaseDoor
{
public:
         	virtual void PostSpawn( void ) {}
	virtual BOOL IsRotatingDoor( void ){ return TRUE; };
};

class CMomentaryDoor : public CBaseMover
{
public:
	void Spawn( void );
	void Precache( void );
	void PostSpawn( void );
	void EXPORT MomentaryMoveDone( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseMover :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

class CBasePlatform : public CBaseMover
{
public:
	void Precache( void );
	void Spawn( void );
	void PostSpawn( void );
	void Setup( void );
          void PostActivate( void );
	virtual void Blocked( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	float CalcFloor( void )
	{
		float curfloor = ((pev->origin.z - m_vecPosition1.z) / step()) + 1;
		if (curfloor - floor(curfloor) > 0.5) return ceil(curfloor);
		else return floor(curfloor);
	}
	float step( void ) { return pev->size.z + m_flHeight - 2; }
	float ftime( void ) { return step() / pev->speed; } //time to moving between floors
	
	void EXPORT GoUp( void );
	void EXPORT GoDown( void );
	void GoToFloor( float floor );
	void EXPORT HitTop( void );
	void EXPORT HitBottom( void );
	void EXPORT HitFloor( void );
	virtual BOOL IsMovingPlatform( void )  { return m_flHeight != 0 && m_flMoveDistance == 0; }
          virtual BOOL IsRotatingPlatform( void ){ return m_flHeight == 0 && m_flMoveDistance != 0; }
          virtual BOOL IsComplexPlatform( void ) { return m_flHeight != 0 && m_flMoveDistance != 0; }
};

class CFuncTrain : public CBasePlatform
{
public:
	void Spawn( void );
	void PostSpawn( void );
	void OverrideReset( void );
          void PostActivate( void );
	void ClearPointers( void );
	
	BOOL Stop( float flWait = -1 );
	BOOL Teleport( void );
	void Blocked( CBaseEntity *pOther );
	BOOL IsWater( void ){ return pev->skin != 0; };
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	Vector TrainOrg( void ) { return (pev->mins + pev->maxs) * 0.5; }

	void EXPORT Wait( void );
	void EXPORT Next( void );

	//path operations
	CBaseEntity *FindPath( void );
	CBaseEntity *FindNextPath( void );
	void Reverse( void );
	
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
	
	CBaseEntity *pCurPath, *pNextPath;
};

class CFuncTrackTrain : public CBaseMover
{
public:
	void Spawn( void );
	void Precache( void );

	void Blocked( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData* pkvd );

	void EXPORT DesiredAction( void ); //LRC - used to be called Next!
          void PostActivate( void );
	void ClearPointers( void );

//	void EXPORT Next( void );
	void EXPORT PostponeNext( void );
	void EXPORT Find( void );
	void EXPORT NearestPath( void );
	void EXPORT DeadEnd( void );

	void NextThink( float thinkTime, BOOL alwaysThink );

	void SetTrack( CBaseEntity *track ) { pPath = ((CPathTrack *)track)->Nearest(pev->origin); }
	void SetControls( entvars_t *pevControls );
	BOOL OnControls( entvars_t *pev );

	void StopSound ( void );
	void UpdateSound ( void );
	
	static CFuncTrackTrain *Instance( edict_t *pent )
	{
		if ( FClassnameIs( pent, "func_tracktrain" ) )
			return (CFuncTrackTrain *)GET_PRIVATE(pent);
		return NULL;
	}

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	
	static TYPEDESCRIPTION m_SaveData[];
	virtual int ObjectCaps( void ) { return CBaseMover :: ObjectCaps() | FCAP_DIRECTIONAL_USE; }

	virtual void	OverrideReset( void );
	
	CBaseEntity	*pPath;
	float		m_length;
	float		m_height;
	// I get it... this records the train's max speed (as set by the level designer), whereas
	// pev->speed records the current speed (as set by the player). --LRC
	// m_speed is also stored, as an int, in pev->impulse.
	float		m_speed;
	float		m_dir;
	float		m_startSpeed;
	Vector		m_controlMins;
	Vector		m_controlMaxs;
	int		m_soundPlaying;
	float		m_flBank;
	float		m_oldSpeed;
	Vector		m_vecBaseAvel; // LRC - the underlying avelocity, superceded by normal turning behaviour where applicable
};

#endif //BASEMOVER_H