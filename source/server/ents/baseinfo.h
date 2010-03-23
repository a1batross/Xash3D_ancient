//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#ifndef BASEINFO_H
#define BASEINFO_H

class CPointEntity : public CBaseEntity
{
public:
	void Spawn( void ){ SetBits( pev->flags, FL_POINTENTITY ); pev->solid = SOLID_NOT; }
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

class CNullEntity : public CBaseEntity
{
public:
	void Spawn( void ){ REMOVE_ENTITY( ENT( pev )); }
};

class CBaseDMStart : public CPointEntity
{
public:
	void	KeyValue( KeyValueData *pkvd );
	STATE	GetState( CBaseEntity *pEntity );

private:
};

#define SPEED_MASTER	0
#define SPEED_INCREMENT	1
#define SPEED_DECREMENT	2
#define SPEED_MULTIPLY	3
#define SPEED_DIVIDE	4

#define SF_CORNER_WAITFORTRIG	0x1
#define SF_CORNER_TELEPORT	0x2
#define SF_CORNER_FIREONCE	0x4

class CInfoPath : public CBaseLogic
{
public:
	void Spawn( );
	void KeyValue( KeyValueData* pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void PostActivate( void );
	float GetDelay( void ) { return m_flDelay; }
          
          void GetSpeed( float *speed )
          {
		if( !pev->speed ) return;
		float curspeed = *speed; // save our speed

		// operate	
		if( pev->button == SPEED_INCREMENT ) curspeed += pev->speed;
		if( pev->button == SPEED_DECREMENT ) curspeed -= pev->speed;
		if( pev->button == SPEED_MULTIPLY ) curspeed *= pev->speed;
		if( pev->button == SPEED_DIVIDE ) curspeed /= pev->speed;
		if( pev->button == SPEED_MASTER ) curspeed = pev->speed;
		// check validate speed
		if( curspeed <= -MAX_VELOCITY || curspeed >= MAX_VELOCITY )
		{
			curspeed = *speed; // set old value
			ALERT( at_console, "\n======/Xash SmartField System/======\n\n" );	
			ALERT( at_console, "%s: %s contains invalid speed operation! Check it!\n Speed not changed!\n", STRING( pev->classname ), STRING( pev->targetname ));
		}
		*speed = curspeed;
	}
	
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int	m_cPaths;		// the total number of targets in this manager's fire list.
	int	m_index;		// Current target
	int	m_iPathName[MAX_MULTI_TARGETS]; // list if indexes into global string array
	int	m_iPathWeight[MAX_MULTI_TARGETS]; // list if weight into global string array

	CInfoPath *Instance( edict_t *pent )
	{ 
		if ( FClassnameIs( pent, "info_path" ))
			return (CInfoPath *)GET_PRIVATE( pent );
		return NULL;
	}

	CInfoPath	*m_pNextPath[MAX_MULTI_TARGETS];
	CInfoPath	*m_pPrevPath;
	CBaseEntity *GetNext( void );
	CBaseEntity *GetPrev( void );
	void SetPrev( CInfoPath *pPrev );
};

class CLaserSpot : public CBaseEntity // laser spot for different weapons
{
	void Spawn( void );
	void Precache( void );
	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
public:
	void Suspend( float flSuspendTime );
	void Update( CBasePlayer *m_pPlayer );
	void EXPORT Revive( void );
	void UpdateOnRemove( void );
	void Killed( void ){ UTIL_Remove( this ); }
	
	static CLaserSpot *CreateSpot( entvars_t *pevOwner = NULL );
};

#endif //BASEINFO_H