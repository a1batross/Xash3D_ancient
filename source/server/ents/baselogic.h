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
	string_t	m_sSet;		// used for logic_usetype
	string_t	m_sReset;		// used for logic_usetype
	float m_flMin, m_flMax;
};

#include "baseinfo.h"

class CEnvRainModify : public CPointEntity
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