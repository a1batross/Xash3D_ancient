//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================
#ifndef BASEMOVER_H
#define BASEMOVER_H

class CBaseMover : public CBaseBrush
{
public:
	void KeyValue( KeyValueData *pkvd );
	virtual void AxisDir( void );
	void (CBaseMover::*m_pfnCallWhenMoveDone)(void); // custom movedone function
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	virtual int IsWater( void ){ return pev->skin != 0; };
	static	TYPEDESCRIPTION m_SaveData[];
	float	m_flMoveDistance;		// rotating distance
	float	m_flBlockedTime;		// set blocked refresh time
	int	m_iMode;			// style of working
	float	m_flValue;		// value to send
	float	m_flHeight;
	float	m_flWait;
	float	m_flLip;

	Vector	m_vecPosition1;		// startpos
	Vector	m_vecPosition2;		// endpos
	Vector	m_vecAngle1;		// startangle
	Vector	m_vecAngle2;		// endangle
	Vector	m_vecFinalDest;		// basemover finalpos
	Vector	m_vecFinalAngle;    	// basemover finalangle
	Vector	m_vecFloor;		// basemover dest floor
	float	m_flLinearMoveSpeed;	// member linear speed
	float	m_flAngularMoveSpeed;	// member angular speed

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
	void EXPORT DoorUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT ShowInfo( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT DoorTouch( CBaseEntity *pOther );
	virtual void Blocked( CBaseEntity *pOther );
	virtual void SetToggleState( int state );
	virtual int ObjectCaps( void ) 
	{ 
		if ( FStringNull( pev->targetname ) && m_iMode != 1 ) // door without name player can direct using
			return (CBaseMover::ObjectCaps() & ~FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE | FCAP_ONLYDIRECT_USE);
		return (CBaseMover::ObjectCaps() & ~FCAP_ACROSS_TRANSITION);
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
	void Spawn( void );
         	virtual void PostSpawn( void ) {}
	virtual void SetToggleState( int state );
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
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	Vector TrainOrg( void ) { return (pev->mins + pev->maxs) * 0.5; }

	void EXPORT Wait( void );
	void EXPORT Next( void );

	//path operations
	BOOL FindPath( void );
	BOOL FindNextPath( void );
	void UpdateTargets( void );
	void UpdateSpeed( float value = 0 );
	
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
	
	CBaseEntity	*pPath, *pNextPath;
};

#endif //BASEMOVER_H