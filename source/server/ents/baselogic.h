//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#ifndef BASELOGIC_H
#define BASELOGIC_H

#define MAX_MULTI_TARGETS	32 // maximum number of targets that can be added in a list

class CBaseLogic : public CBaseEntity
{
public:
	BOOL IsLockedByMaster( void );
	BOOL IsLockedByMaster( USE_TYPE useType );
	BOOL IsLockedByMaster( CBaseEntity *pActivator );
	void FireTargets( USE_TYPE useType = USE_TOGGLE, float value = 0 );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void KeyValue( KeyValueData* pkvd);
	virtual int  Save( CSave &save );
	virtual int  Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
          virtual STATE GetState( void ) { return m_iState; };
          
	float	m_flDelay;
	float	m_flWait;
	EHANDLE	m_hActivator;
	EHANDLE	m_hTarget;
	STATE	m_iState;
	string_t	m_sMaster;
	string_t	m_iszKillTarget; //evil stuff. agrhh
	string_t	m_sSet;//used for logic_usetype
	string_t	m_sReset;//used for logic_usetype
	string_t	m_globalstate;
	float m_flMin, m_flMax;
};

#define SF_CORNER_WAITTRIG		0x001
#define SF_CORNER_TELEPORT		0x002
#define SF_CORNER_FIREONCE		0x004

class CPathCorner : public CBaseLogic
{
public:
	void Spawn( );
	void KeyValue( KeyValueData* pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	float GetDelay( void ){ return pev->spawnflags & SF_CORNER_WAITTRIG ? -1 : m_flWait; }
          void GetSpeed( float *speed ) { if(pev->speed != 0) *speed = pev->speed; }
	void UpdateTargets( void );
	void Link( void );
	void PostActivate( void ){ Link(); }
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	CPathCorner *Instance( edict_t *pent )
	{ 
		if ( FClassnameIs( pent, "path_corner" ) )
			return (CPathCorner *)GET_PRIVATE(pent);
		return NULL;
	}

	CPathCorner *m_pNextPath1;
	CPathCorner *m_pNextPath2;
	CPathCorner *m_pPrevPath;
	CBaseEntity *ValidPath( CBaseEntity *m_pPath );
	CBaseEntity *GetNext( void );
	CBaseEntity *GetPrev( void ){ return ValidPath( m_pPrevPath ); }
	void SetPrev( CPathCorner *pPrev ) { m_pPrevPath = (CPathCorner *)ValidPath((CPathCorner *)pPrev); }
};

class CPathTrack : public CBaseLogic
{
public:
	void		Spawn( void );
	void		Activate( void );
	void		KeyValue( KeyValueData* pkvd);
	
	void		SetPrevious( CPathTrack *pprevious );
	void		Link( void );
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	CBaseEntity	*ValidPath( CBaseEntity *ppath, int testFlag );		// Returns ppath if enabled, NULL otherwise
	void		Project( CBaseEntity *pstart, CBaseEntity *pend, Vector *origin, float dist );

	static CPathTrack *Instance( edict_t *pent );

	CBaseEntity	*LookAhead( Vector *origin, float dist, int move );
	CBaseEntity	*Nearest( Vector origin ); //notused

	CBaseEntity	*GetNext( void );
	CBaseEntity	*GetPrev( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];
#if PATH_SPARKLE_DEBUG
	void EXPORT Sparkle(void);
#endif

	float		m_length;
	string_t	m_altName;
	CPathTrack	*m_pnext;
	CPathTrack	*m_pprevious;
	CPathTrack	*m_paltpath;
};

class CMultiSource : public CBaseLogic
{
public:
	void Spawn( );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	STATE GetState( void );
	void EXPORT Register( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	EHANDLE	m_rgEntities[MAX_MULTI_TARGETS];
	int	m_rgTriggered[MAX_MULTI_TARGETS];

	int m_iTotal;
};

#include "baseinfo.h"

class CUtilRainModify : public CPointEntity
{
public:
	void	Spawn( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int drips;
	RandomRange windXY;
	RandomRange randXY;
	float fadeTime;
};

#endif //BASELOGIC_H