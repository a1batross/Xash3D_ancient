//=======================================================================
//			Copyright (C) Shambler Team 2004
//		    logicentity.cpp - all entities with prefix "logic_"
//			additional entities for smart system			    
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "defaults.h"

// Use CBaseDelay as main function (renamed as CBaseLogic, see declaration in cbase.h).
//=======================================================================
// 		   main functions ()
//=======================================================================

void CBaseLogic :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq( pkvd->szKeyName, "delay" ))
	{
		m_flDelay = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "wait" ))
	{
		m_flWait = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "master"))
	{
		m_sMaster = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	CBaseEntity::KeyValue( pkvd );
}

BOOL CBaseLogic :: IsLockedByMaster( void )
{
	if (UTIL_IsMasterTriggered(m_sMaster, m_hActivator))
		return FALSE;
	return TRUE;
}

BOOL CBaseLogic :: IsLockedByMaster( CBaseEntity *pActivator )
{
	if (UTIL_IsMasterTriggered(m_sMaster, pActivator))
		return FALSE;
	return TRUE;
}

BOOL CBaseLogic :: IsLockedByMaster( USE_TYPE useType )
{
	if (UTIL_IsMasterTriggered(m_sMaster, m_hActivator))
		return FALSE;
	else if (useType == USE_SHOWINFO) return FALSE;//pass to debug info
	return TRUE;
}

TYPEDESCRIPTION	CBaseLogic::m_SaveData[] = 
{
	DEFINE_FIELD( CBaseLogic, m_flDelay, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseLogic, m_flWait, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseLogic, m_hActivator, FIELD_EHANDLE ),
	DEFINE_FIELD( CBaseLogic, m_hTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( CBaseLogic, m_iState, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseLogic, m_sMaster, FIELD_STRING),
	DEFINE_FIELD( CBaseLogic, m_sSet, FIELD_STRING),
	DEFINE_FIELD( CBaseLogic, m_sReset, FIELD_STRING),
};IMPLEMENT_SAVERESTORE( CBaseLogic, CBaseEntity );

//=======================================================================
// 		   Logic_generator
//=======================================================================
#define NORMAL_MODE		0
#define RANDOM_MODE		1
#define RANDOM_MODE_WITH_DEC	2

#define SF_RANDOM		0x2
#define SF_DEC		0x4

class CGenerator : public CBaseLogic
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Think (void);

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

private:
	int m_iFireCount;
	int m_iMaxFireCount;
};
LINK_ENTITY_TO_CLASS( logic_generator, CGenerator );

void CGenerator :: Spawn()
{
	// Xash 0.2 compatible
	if( pev->spawnflags & SF_RANDOM )
		pev->button = RANDOM_MODE;
	if( pev->spawnflags & SF_DEC )
		pev->button = RANDOM_MODE_WITH_DEC;

	if( pev->button == RANDOM_MODE || pev->button == RANDOM_MODE_WITH_DEC )
	{ 	
		// set delay with decceleration
		if( !m_flDelay || pev->button == RANDOM_MODE_WITH_DEC ) m_flDelay = 0.005f;
		// generate max count automaticallly, if not set on map
		if( !m_iMaxFireCount ) m_iMaxFireCount = RANDOM_LONG( 100, 200 );
	}
	else
	{
		// Smart Field System ©
		if ( !m_iMaxFireCount )
			m_iMaxFireCount = -1; // disable counting for normal mode
	}

	if ( pev->spawnflags & SF_START_ON )
	{
		m_iState = STATE_ON; // initialy off in random mode
		SetNextThink(m_flDelay);
	}
}

TYPEDESCRIPTION CGenerator :: m_SaveData[] = 
{
	DEFINE_FIELD( CGenerator, m_iMaxFireCount, FIELD_INTEGER ),
	DEFINE_FIELD( CGenerator, m_iFireCount, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CGenerator, CBaseLogic );

void CGenerator :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq( pkvd->szKeyName, "maxcount" ))
	{
		m_iMaxFireCount = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if (FStrEq( pkvd->szKeyName, "mode" ))
	{
		pev->button = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseLogic::KeyValue( pkvd );
}

void CGenerator :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType == USE_TOGGLE )
	{
		if ( m_iState )
			useType = USE_OFF;
		else useType = USE_ON;
	}
	if ( useType == USE_ON )
	{
		if ( pev->button == RANDOM_MODE || pev->button == RANDOM_MODE_WITH_DEC )
		{
			if( pev->button == RANDOM_MODE_WITH_DEC ) m_flDelay = 0.005;
			m_iFireCount = RANDOM_LONG( 0, m_iMaxFireCount / 2 );
		}
		m_iState = STATE_ON;
		SetNextThink( 0 );//immediately start firing targets
	}
	else if ( useType == USE_OFF )
	{
		m_iState = STATE_OFF;
		DontThink();
	}
	else if ( useType == USE_SET )
	{
		// set max count of impulses
		m_iMaxFireCount = fabs( value );
	}	
	else if ( useType == USE_RESET )
	{
		 // immediately reset
		m_iFireCount = 0;
	}
	else if ( useType == USE_SHOWINFO )
	{
		ALERT( at_console, "======/Xash Debug System/======\n" );
		ALERT( at_console, "classname: %s\n", STRING( pev->classname ));
		ALERT( at_console, "State: %s, Delay time %f\n", GetStringForState( GetState()), m_flDelay );
		ALERT( at_console, "FireCount: %d, Max FireCount: %d\n", m_iFireCount, m_iMaxFireCount );
	}
}

void CGenerator :: Think( void )
{
	if ( m_iFireCount != -1 )
	{
		// if counter enabled
		if( m_iFireCount == m_iMaxFireCount )
		{
			m_iState = STATE_OFF;
			m_iFireCount = 0;
			DontThink();
			return;
		}
 		else m_iFireCount++;
	}

	if ( pev->button == RANDOM_MODE_WITH_DEC )
	{	
		//deceleration for random mode
		if( m_iMaxFireCount - m_iFireCount < 40 )
			m_flDelay += 0.005;
	}

	UTIL_FireTargets( pev->target, this, this, USE_TOGGLE );

	SetNextThink( m_flDelay );
}


//=======================================================================
// 		   Logic_watcher
//=======================================================================
#define LOGIC_AND  		0	// fire if all objects active
#define LOGIC_OR   		1         // fire if any object active
#define LOGIC_NAND 		2         // fire if not all objects active
#define LOGIC_NOR  		3         // fire if all objects disable
#define LOGIC_XOR		4         // fire if only one (any) object active
#define LOGIC_XNOR		5         // fire if active any number objects, but < then all

#define W_ON		0
#define W_OFF		1
#define W_TURNON		2
#define W_TURNOFF		3
#define W_IN_USE		4

class CStateWatcher : public CBaseLogic
{
public:
	void Spawn( void );
	void Think( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual STATE GetState( void ) { return m_iState; };
	virtual STATE GetState( CBaseEntity *pActivator ) { return EvalLogic(pActivator) ? STATE_ON : STATE_OFF; };

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int m_cTargets; // the total number of targets in this manager's fire list.
	int m_iTargetName[MAX_MULTI_TARGETS];// list of indexes into global string array

	BOOL EvalLogic( CBaseEntity *pEntity );
};

LINK_ENTITY_TO_CLASS( logic_watcher, CStateWatcher );

TYPEDESCRIPTION	CStateWatcher::m_SaveData[] =
{
	DEFINE_FIELD( CStateWatcher, m_cTargets, FIELD_INTEGER ),
	DEFINE_ARRAY( CStateWatcher, m_iTargetName, FIELD_STRING, MAX_MULTI_TARGETS ),
};IMPLEMENT_SAVERESTORE(CStateWatcher,CBaseLogic);

void CStateWatcher :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "mode"))
	{
		pev->button = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "type"))
	{
		pev->body = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "offtarget"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else // add this field to the target list
	{
		// this assumes that additional fields are targetnames and their values are delay values.
		if ( m_cTargets < MAX_MULTI_TARGETS )
		{
			char tmp[128];
			
			UTIL_StripToken( pkvd->szKeyName, tmp );
			m_iTargetName [ m_cTargets ] = ALLOC_STRING( tmp );
			m_cTargets++;
			pkvd->fHandled = TRUE;
		}
	}
}

void CStateWatcher :: Spawn ( void )
{
	SetNextThink( 0.1 );
}

void CStateWatcher :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (useType == USE_SHOWINFO)
	{
		ALERT(at_console, "======/Xash Debug System/======\n");
		ALERT(at_console, "classname: %s\n", STRING(pev->classname));
		ALERT(at_console, "State: %s, Number of targets %d\n", GetStringForState( GetState()), m_cTargets);
		ALERT(at_console, "Limit is %d entities\n", MAX_MULTI_TARGETS);
	}
}

void CStateWatcher :: Think ( void )
{
	if(EvalLogic(NULL)) 
	{
		if(m_iState == STATE_OFF)
		{
			m_iState = STATE_ON;
			UTIL_FireTargets( pev->target, this, this, USE_ON );
		}
	}
	else 
	{
		if(m_iState == STATE_ON)
		{
			m_iState = STATE_OFF;
			UTIL_FireTargets( pev->netname, this, this, USE_OFF );
		}
          }
 	SetNextThink( 0.05 );
}

BOOL CStateWatcher :: EvalLogic ( CBaseEntity *pActivator )
{
	int i;
	BOOL b;
	BOOL xorgot = FALSE;

	CBaseEntity* pEntity;

	for (i = 0; i < m_cTargets; i++)
	{
		pEntity = UTIL_FindEntityByTargetname(NULL,STRING(m_iTargetName[i]), pActivator);
		if (pEntity != NULL);
		else continue;
 		b = FALSE;
 
		switch (pEntity->GetState())
		{
		case STATE_ON:	 if (pev->body == W_ON)    	b = TRUE; break;
		case STATE_OFF:	 if (pev->body == W_OFF)   	b = TRUE; break;
		case STATE_TURN_ON:	 if (pev->body == W_TURNON)	b = TRUE; break;
		case STATE_TURN_OFF: if (pev->body == W_TURNOFF)	b = TRUE; break;
		case STATE_IN_USE:	 if (pev->body == W_IN_USE)	b = TRUE; break;
                    }
		// handle the states for this logic mode
		if (b)
		{
			switch (pev->button)
			{
				case LOGIC_OR:  return TRUE;
				case LOGIC_NOR: return FALSE;
				case LOGIC_XOR: 
					if(xorgot)     return FALSE;
					xorgot = TRUE;
					break;
				case LOGIC_XNOR:
					if(xorgot)     return TRUE;
					xorgot = TRUE;
				break;
			}
		}
		else // b is false
		{
			switch (pev->button)
			{
		         		case LOGIC_AND:  return FALSE;
				case LOGIC_NAND: return TRUE;
			}
		}
	}

	// handle the default cases for each logic mode
	switch (pev->button)
	{
		case LOGIC_AND:
		case LOGIC_NOR:  return TRUE;
		case LOGIC_XOR:  return xorgot;
		case LOGIC_XNOR: return !xorgot;
		default:	       return FALSE;
	}
}

//=======================================================================
// 		   Logic_manager
//=======================================================================
#define FL_CLONE			0x80000000
#define SF_LOOP			0x2


class CMultiManager : public CBaseLogic
{
public:
	void KeyValue( KeyValueData *pkvd );
	void PostActivate( void );
	void Spawn ( void );
	void Think ( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];


	int		m_cTargets;	// the total number of targets in this manager's fire list.
	int		m_index;		// Current target
	float		m_startTime;	// Time we started firing
	int		m_iTargetName   [ MAX_MULTI_TARGETS ];// list if indexes into global string array
	float		m_flTargetDelay [ MAX_MULTI_TARGETS ];// delay (in seconds)
private:
	inline BOOL IsClone( void ) { return (pev->spawnflags & FL_CLONE) ? TRUE : FALSE; }
	inline BOOL ShouldClone( void ) 
	{ 
		if ( IsClone() )return FALSE;
		//work in progress and calling again ?
		return (m_iState == STATE_ON) ? TRUE : FALSE; 
	}

	CMultiManager *Clone( void );
};
LINK_ENTITY_TO_CLASS( logic_manager, CMultiManager );

// Global Savedata for multi_manager
TYPEDESCRIPTION	CMultiManager::m_SaveData[] = 
{	DEFINE_FIELD( CMultiManager, m_cTargets, FIELD_INTEGER ),
	DEFINE_FIELD( CMultiManager, m_index, FIELD_INTEGER ),
	DEFINE_FIELD( CMultiManager, m_startTime, FIELD_TIME ),
	DEFINE_ARRAY( CMultiManager, m_iTargetName, FIELD_STRING, MAX_MULTI_TARGETS ),
	DEFINE_ARRAY( CMultiManager, m_flTargetDelay, FIELD_FLOAT, MAX_MULTI_TARGETS ),
};IMPLEMENT_SAVERESTORE(CMultiManager, CBaseLogic);

void CMultiManager :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq( pkvd->szKeyName, "delay" ))
	{
		m_flDelay = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "master" ))
	{
		m_sMaster = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( m_cTargets < MAX_MULTI_TARGETS )
	{
		char tmp[128];

		UTIL_StripToken( pkvd->szKeyName, tmp );
		m_iTargetName[m_cTargets] = ALLOC_STRING( tmp );
		m_flTargetDelay[m_cTargets] = atof( pkvd->szValue );
		m_cTargets++;
		pkvd->fHandled = TRUE;
	}
}

void CMultiManager :: Spawn( void )
{
	// Sort targets
	// Quick and dirty bubble sort
	int swapped = 1;

	while ( swapped )
	{
		swapped = 0;
		for ( int i = 1; i < m_cTargets; i++ )
		{
			if ( m_flTargetDelay[i] < m_flTargetDelay[i-1] )
			{
				// Swap out of order elements
				int name = m_iTargetName[i];
				float delay = m_flTargetDelay[i];
				m_iTargetName[i] = m_iTargetName[i-1];
				m_flTargetDelay[i] = m_flTargetDelay[i-1];
				m_iTargetName[i-1] = name;
				m_flTargetDelay[i-1] = delay;
				swapped = 1;
			}
		}
	}
	
 	m_iState = STATE_OFF;
          m_index = 0;
}

void CMultiManager :: PostActivate( void )
{
	if ( pev->spawnflags & SF_START_ON )
	{
		Use( this, this, USE_TOGGLE, 0 );
		ClearBits( pev->spawnflags, SF_START_ON );
	}
}

void CMultiManager :: Think( void )
{
	float	time;

	time = gpGlobals->time - m_startTime;
	while ( m_index < m_cTargets && m_flTargetDelay[ m_index ] <= time )
	{
		UTIL_FireTargets( m_iTargetName[ m_index ], m_hActivator, this, USE_TOGGLE, pev->frags );
		m_index++;
	}	
	if ( m_index >= m_cTargets )// have we fired all targets?
	{
		if ( pev->spawnflags & SF_LOOP)//continue firing
		{
			m_index = 0;
			m_startTime = m_flDelay + gpGlobals->time;
		}
		else
		{
                    	m_iState = STATE_OFF;
			DontThink();
			return;
		}
		if ( IsClone() )
		{
			UTIL_Remove( this );
			return;
		}
	}
	m_iState = STATE_ON; //continue firing targets
	pev->nextthink = m_startTime + m_flTargetDelay[ m_index ];	
}

CMultiManager *CMultiManager::Clone( void )
{
	CMultiManager *pMulti = GetClassPtr( (CMultiManager *)NULL );

	edict_t *pEdict = pMulti->pev->pContainingEntity;
	memcpy( pMulti->pev, pev, sizeof(*pev) );
	pMulti->pev->pContainingEntity = pEdict;

	pMulti->pev->spawnflags |= FL_CLONE;
	pMulti->m_cTargets = m_cTargets;
	pMulti->m_flDelay = m_flDelay;
	pMulti->m_iState = m_iState;
	memcpy( pMulti->m_iTargetName, m_iTargetName, sizeof( m_iTargetName ));
	memcpy( pMulti->m_flTargetDelay, m_flTargetDelay, sizeof( m_flTargetDelay ));

	return pMulti;
}


void CMultiManager :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if( IsLockedByMaster( useType )) return;
	pev->frags = value; // save our value
	
	if ( useType == USE_TOGGLE )
	{
		// we send USE_ON or USE_OF from this
		if( m_iState == STATE_OFF )
			useType = USE_ON;
		else useType = USE_OFF;
	}
	if (useType == USE_ON)
	{
	          if ( ShouldClone() )
	          {
	          	// create clone if needed
			CMultiManager *pClone = Clone();
			pClone->Use( pActivator, pCaller, useType, value );
			return;
		}

		if ( m_iState == STATE_OFF )
		{
			if( m_hActivator == this ) // ok, manager execute himself 
  			{
				// only for follow reason: auto start
				m_startTime = m_flDelay + gpGlobals->time;
				m_iState = STATE_TURN_ON;
				m_index = 0;
				SetNextThink( m_flDelay );
			}
			else // ok no himself fire and manager on
			{
                             		m_startTime = m_flDelay + gpGlobals->time;
				m_iState = STATE_TURN_ON;
				m_index = 0;
				SetNextThink( m_flDelay );
			}
		}
	}	
	else if ( useType == USE_OFF )
	{	
		m_iState = STATE_OFF;
		DontThink();
	}
 	else if ( useType == USE_SET )
 	{ 
		m_index = 0; // reset fire index
		while ( m_index < m_cTargets )
		{	
			// firing all targets instantly
	         		UTIL_FireTargets( m_iTargetName[ m_index ], this, this, USE_TOGGLE );
			m_index++;
			if(m_hActivator == this) break;//break if current target - himself
		}
 	}
 	else if ( useType == USE_RESET )
 	{ 
		m_index = 0; // reset fire index
		m_iState = STATE_TURN_ON;
		m_startTime = m_flDelay + gpGlobals->time;
		SetNextThink( m_flDelay );
 	}	
	else if ( useType == USE_SHOWINFO )
	{
		ALERT( at_console, "======/Xash Debug System/======\n");
		ALERT( at_console, "classname: %s\n", STRING(pev->classname));
		ALERT( at_console, "State: %s, number of targets %d\n", GetStringForState( GetState()), m_cTargets);
		if( m_iState == STATE_ON )
	     		ALERT( at_console, "Current target %s, delay time %f\n", STRING(m_iTargetName[ m_index ]), m_flTargetDelay[ m_index ]);
		else ALERT( at_console, "No targets for firing.\n");
	}
}
//=======================================================================
// 		   Logic_switcher
//=======================================================================
#define MODE_INCREMENT			0
#define MODE_DECREMENT			1
#define MODE_RANDOM_VALUE			2

class CSwitcher : public CBaseLogic
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn ( void );
	void Think ( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int		m_cTargets;// the total number of targets in this manager's fire list.
	int		m_index;		// Current target
	int		m_iTargetName   [ MAX_MULTI_TARGETS ];// list if indexes into global string array
	int		m_iTargetNumber [ MAX_MULTI_TARGETS ];// list of target numbers
};
LINK_ENTITY_TO_CLASS( logic_switcher, CSwitcher );

// Global Savedata for switcher
TYPEDESCRIPTION	CSwitcher::m_SaveData[] = 
{	DEFINE_FIELD( CSwitcher, m_cTargets, FIELD_INTEGER ),
	DEFINE_FIELD( CSwitcher, m_index, FIELD_INTEGER ),
	DEFINE_ARRAY( CSwitcher, m_iTargetName, FIELD_STRING, MAX_MULTI_TARGETS ),
	DEFINE_ARRAY( CSwitcher, m_iTargetNumber, FIELD_INTEGER, MAX_MULTI_TARGETS ),
};IMPLEMENT_SAVERESTORE(CSwitcher, CBaseLogic);

void CSwitcher :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq( pkvd->szKeyName, "mode" ))
	{
		pev->button = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "delay" ))
	{
		m_flDelay = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( m_cTargets < MAX_MULTI_TARGETS ) 
	{
		// add this field to the target list
		// this assumes that additional fields are targetnames and their values are delay values.
		
		char tmp[128];
		UTIL_StripToken( pkvd->szKeyName, tmp );
		m_iTargetName [ m_cTargets ] = ALLOC_STRING( tmp );
		m_iTargetNumber [ m_cTargets ] = atoi( pkvd->szValue );
		m_cTargets++;
		pkvd->fHandled = TRUE;
	}
}

void CSwitcher :: Spawn( void )
{
	// Sort targets
	// Quick and dirty bubble sort
	int swapped = 1;

	while ( swapped )
	{
		swapped = 0;
		for ( int i = 1; i < m_cTargets; i++ )
		{
			if ( m_iTargetNumber[i] < m_iTargetNumber[i-1] )
			{
				// Swap out of order elements
				int name = m_iTargetName[i];
				int number = m_iTargetNumber[i];
				m_iTargetName[i] = m_iTargetName[i-1];
				m_iTargetNumber[i] = m_iTargetNumber[i-1];
				m_iTargetName[i-1] = name;
				m_iTargetNumber[i-1] = number;
				swapped = 1;
			}
		}
	}

	m_iState = STATE_OFF;
	m_index = 0;

	if ( pev->spawnflags & SF_START_ON )
	{
		m_iState = STATE_ON;
 		SetNextThink ( m_flDelay );
	}	
}

void CSwitcher :: Think ( void )
{
	if ( pev->button == MODE_INCREMENT )
	{
		// increase target number
		m_index++;
		if( m_index >= m_cTargets )
			m_index = 0;
	}
	else if ( pev->button == MODE_DECREMENT )
	{
		m_index--;
		if( m_index < 0 )
			m_index = m_cTargets - 1;
	}
	else if ( pev->button == MODE_RANDOM_VALUE )
	{
		m_index = RANDOM_LONG( 0, m_cTargets - 1 );
	}
	SetNextThink ( m_flDelay );
}

void CSwitcher :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if(IsLockedByMaster( useType )) return;

	if ( useType == USE_SET )
	{
		// set new target for activate (direct choose or increment\decrement)
		if( pev->spawnflags & SF_START_ON )
		{
			m_iState = STATE_ON;
			SetNextThink (m_flDelay);
			return;
		}

		// set maximum priority for direct choose
		if( value ) 
		{
			m_index = (value - 1);
			if( m_index >= m_cTargets )
				m_index = -1;
			return;
		}
		if( pev->button == MODE_INCREMENT )
		{
			m_index++;
			if ( m_index >= m_cTargets )
				m_index = 0; 	
		}
                    else if( pev->button == MODE_DECREMENT )
		{
			m_index--;
			if ( m_index < 0 )
				m_index = m_cTargets - 1;
		}
		else if( pev->button == MODE_RANDOM_VALUE )
		{
			m_index = RANDOM_LONG( 0, m_cTargets - 1 );
		}
	}
	else if ( useType == USE_RESET )
	{
		// reset switcher
		m_iState = STATE_OFF;
		DontThink();
		m_index = 0;
		return;
	}
	else if ( useType == USE_SHOWINFO )
	{
		ALERT( at_console, "======/Xash Debug System/======\n");
		ALERT( at_console, "classname: %s\n", STRING(pev->classname));
		ALERT( at_console, "State: %s, number of targets %d\n", GetStringForState( GetState()), m_cTargets - 1);
		ALERT( at_console, "Current target %s, target number %d\n", STRING(m_iTargetName[ m_index ]), m_index );
	}
	else if ( m_index != -1 ) // fire any other USE_TYPE and right index
	{
		UTIL_FireTargets( m_iTargetName[m_index], m_hActivator, this, useType, value );
	}
}

//=======================================================================
// 		   logic_changetarget
//=======================================================================
class CLogicChangeTarget : public CBaseLogic
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int ObjectCaps( void ) { return CBaseLogic::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};
LINK_ENTITY_TO_CLASS( logic_changetarget, CLogicChangeTarget );
LINK_ENTITY_TO_CLASS( trigger_changetarget, CLogicChangeTarget );

void CLogicChangeTarget::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "newtarget" ))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseLogic::KeyValue( pkvd );
}

void CLogicChangeTarget::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ), pActivator );

	if ( useType == USE_SHOWINFO )
	{
		ALERT( at_console, "======/Xash Debug System/======\n");
		ALERT( at_console, "classname: %s\n", STRING( pev->classname ));
		ALERT( at_console, "Current target %s, new target %s\n", STRING( pev->target ), STRING( pev->message ));
		ALERT( at_console, "target entity is: %s, current target: %s\n", STRING( pTarget->pev->classname ), STRING( pTarget->pev->target ));
	}
	else
	{
		if ( pTarget )
		{
			const char *target = STRING( pev->message );

			if ( FStrEq( target, "*this" ) || FStrEq( target, "*locus" )) // Xash 0.2 compatibility
			{
				if ( pActivator ) pTarget->pev->target = pActivator->pev->targetname;
				else ALERT( at_error, "%s \"%s\" requires a self pointer!\n", STRING( pev->classname ), STRING( pev->targetname ));
			}
			else
			{
				if ( pTarget->IsFuncScreen( ))
					pTarget->ChangeCamera( pev->message );
				else pTarget->pev->target = pev->message;
			}
			CBaseMonster *pMonster = pTarget->MyMonsterPointer( );
			if ( pMonster ) pMonster->m_pGoalEnt = NULL; // force to refresh goal entity
		}
	}
}

//=======================================================================
//			Logic_set
//=======================================================================
#define MODE_LOCAL			0
#define MODE_GLOBAL_WRITE		1
#define MODE_GLOBAL_READ		2

#define ACT_SAME			0
#define ACT_PLAYER			1
#define ACT_THIS			2

class CLogicSet : public CBaseLogic
{
public:
	void KeyValue( KeyValueData *pkvd );
	void PostActivate( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Think ( void );

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	string_t m_globalstate;
	float m_flValue;
};
LINK_ENTITY_TO_CLASS( logic_set, CLogicSet );

void CLogicSet :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq( pkvd->szKeyName, "value" ))
	{
		m_flValue = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "mode" ))
	{
		pev->frags = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "globalstate" ))
	{
		m_globalstate = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "initialstate" ))
	{
		pev->impulse = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "activator" ))
	{
		pev->body = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CBaseLogic::KeyValue( pkvd );
}

// Global Savedata
TYPEDESCRIPTION CLogicSet::m_SaveData[] = 
{
	DEFINE_FIELD( CLogicSet, m_flValue, FIELD_FLOAT ),
	DEFINE_FIELD( CLogicSet, m_globalstate, FIELD_STRING ),
};IMPLEMENT_SAVERESTORE(CLogicSet, CBaseLogic);

void CLogicSet :: PostActivate ( void )
{
	m_iState = STATE_OFF;
	 	
 	if( pev->spawnflags & SF_START_ON )
 	{
		// write global state
		if( pev->frags == MODE_GLOBAL_WRITE )
			Use( this, this, (USE_TYPE)pev->impulse, m_flValue );
		else Use( this, this, USE_TOGGLE, m_flValue );
	}
}

void CLogicSet :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// get entity global state
	GLOBALESTATE curstate = gGlobalState.EntityGetState( m_globalstate );
	
	//set activator
	if ( pev->body == ACT_SAME )
		m_hActivator = pActivator;
	else if ( pev->body == ACT_PLAYER )
		m_hActivator = UTIL_FindEntityByClassname( NULL, "player" );
	else if ( pev->body == ACT_THIS )
		m_hActivator = this;

	// save use type
	pev->button = (int)useType;

	if ( useType == USE_SHOWINFO )
	{
		ALERT( at_console, "======/Xash Debug System/======\n");
		ALERT( at_console, "classname: %s\n", STRING(pev->classname));
		ALERT( at_console, "mode %s\n", pev->frags?"Global":"Local");
		if( m_globalstate )
			ALERT( at_console, "Global state: %s = %s\n", STRING( m_globalstate ), GetStringForGlobalState( curstate ));
		else ALERT( at_console, "Global state not found\n" );
	}
	else // any other USE_TYPE
	{
		if( pev->frags == MODE_LOCAL )
		{
          		if( m_flDelay )
			{
				m_iState = STATE_ON;
				SetNextThink( m_flDelay );
			}
			else 
			{
				UTIL_FireTargets( pev->target, m_hActivator, this, useType, m_flValue );
			}
		}
		else if( pev->frags == MODE_GLOBAL_WRITE && m_globalstate )
		{
			// can controlling entity in global mode
			if( curstate != GLOBAL_DEAD )
			{
          			if ( useType == USE_TOGGLE )
          			{
					if( curstate ) useType = USE_OFF; 
					else useType = USE_ON;
          			}
          			if ( useType == USE_OFF ) pev->impulse = GLOBAL_OFF;
          			else if ( useType == USE_ON )  pev->impulse = GLOBAL_ON;
          			else if ( useType == USE_SET ) pev->impulse = GLOBAL_DEAD;
   			}
			else pev->impulse = curstate; // originally loop, isn't true ? :)
			
			if( m_flDelay )
			{
				m_iState = STATE_ON;
				SetNextThink( m_flDelay );
			}
			else
			{
				// set global state with prefixes
				if ( gGlobalState.EntityInTable( m_globalstate ))
					gGlobalState.EntitySetState( m_globalstate, (GLOBALESTATE)pev->impulse );
				else gGlobalState.EntityAdd( m_globalstate, gpGlobals->mapname, (GLOBALESTATE)pev->impulse );
				// firing targets
				UTIL_FireTargets( pev->target, m_hActivator, this, useType, m_flValue );
			}
		}
		else if( pev->frags == MODE_GLOBAL_READ && gGlobalState.EntityGetState( m_globalstate ) == GLOBAL_ON )
		{
		          // fire only if global mode == GLOBAL_ON
		         	if( m_flDelay )
			{
				m_iState = STATE_ON;
				SetNextThink(m_flDelay);
			}
			else UTIL_FireTargets( pev->target, m_hActivator, this, useType, m_flValue );
		}
	}
}

void CLogicSet :: Think ( void )
{
	if(pev->frags == MODE_GLOBAL_WRITE)//can controlling entity in global mode
	{
		// set state after delay 
		if ( gGlobalState.EntityInTable( m_globalstate ))
			gGlobalState.EntitySetState( m_globalstate, (GLOBALESTATE)pev->impulse );
		else gGlobalState.EntityAdd( m_globalstate, gpGlobals->mapname, (GLOBALESTATE)pev->impulse );

		// firing targets
		UTIL_FireTargets( pev->target, m_hActivator, this, (USE_TYPE)pev->button, m_flValue );
	}
	else UTIL_FireTargets( pev->target, m_hActivator, this, (USE_TYPE)pev->button, m_flValue );

	// suhtdown
	m_iState = STATE_OFF;
	DontThink(); // just in case
}

//=======================================================================
// 		   Logic_counter
//=======================================================================
class CLogicCounter : public CBaseLogic
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );
};
LINK_ENTITY_TO_CLASS( logic_counter, CLogicCounter );

void CLogicCounter :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq( pkvd->szKeyName, "count" ))
	{
		pev->frags = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if (FStrEq( pkvd->szKeyName, "maxcount" ))
	{
		pev->body = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseLogic::KeyValue( pkvd );
}

void CLogicCounter :: Spawn( void )
{
	// smart Field System ©
	if ( pev->frags == 0 ) pev->frags = 2;
	if ( pev->body == 0 ) pev->body = 100;
	if ( pev->frags == -1 ) pev->frags = RANDOM_LONG( 1, pev->body );

	// save number of impulses
	pev->impulse = pev->frags;
	m_iState = STATE_OFF; // always disable
}

void CLogicCounter :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType == USE_SET )
	{
		if( value ) pev->impulse = pev->frags = value; // write new value
		else pev->impulse = pev->frags = RANDOM_LONG( 1, pev->body ); // write random value
	}
	else if (useType == USE_RESET) pev->frags = pev->impulse; //restore counter to default
	else if (useType == USE_SHOWINFO)
	{
		ALERT(at_console, "======/Xash Debug System/======\n");
		ALERT(at_console, "classname: %s\n", STRING(pev->classname));
		ALERT(at_console, "start count %d, current count %.f\n",pev->impulse , pev->impulse - pev->frags );
		ALERT(at_console, "left activates: %.f\n", pev->frags);
	}
	else //any other useType
	{
		pev->frags--;
		if(pev->frags > 0) return;

		pev->frags = 0; //don't reset counter in negative value
		UTIL_FireTargets( pev->target, pActivator, this, useType, value ); //activate target
	}
}

//=======================================================================
// 		   Logic_relay
//=======================================================================
class CLogicRelay : public CBaseLogic
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Spawn( void );
	void Think ( void );
};
LINK_ENTITY_TO_CLASS( logic_relay, CLogicRelay );

void CLogicRelay::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq( pkvd->szKeyName, "target2" ))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CBaseLogic::KeyValue( pkvd );
}

void CLogicRelay :: Spawn( void )
{
	m_iState = STATE_OFF; // always disable
}

void CLogicRelay::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator; 	//save activator
	pev->button = (int)useType;	//save use type
	pev->frags = value; //save our value

	if ( useType == USE_SHOWINFO )
	{
		ALERT( at_console, "======/Xash Debug System/======\n");
		ALERT( at_console, "classname: %s\n", STRING( pev->classname ));
		ALERT( at_console, "target is %s, mtarget %s\n", STRING( pev->target ), STRING( pev->message ));
		ALERT( at_console, "new value %.2f\n", pev->frags );
	}
	else // activate target
	{
		if( m_flDelay )
		{
			m_iState = STATE_ON;
			SetNextThink( m_flDelay );
		}
		else SetNextThink( 0 ); // fire target immediately
	}
}

void CLogicRelay::Think ( void )
{
	if( IsLockedByMaster( ))
		UTIL_FireTargets( pev->message, m_hActivator, this, (USE_TYPE)pev->button, pev->frags );
	else UTIL_FireTargets( pev->target, m_hActivator, this, (USE_TYPE)pev->button, pev->frags );

	// shutdown
	m_iState = STATE_OFF;
	DontThink(); // just in case
}

//=======================================================================
// 		   Logic_state
//=======================================================================
class CLogicState : public CBaseLogic
{
public:
	void Spawn( void );
	void Think( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};
LINK_ENTITY_TO_CLASS( logic_state, CLogicState );


void CLogicState::Spawn( void )
{
	if ( pev->spawnflags & SF_START_ON )
		m_iState = STATE_ON;
	else m_iState = STATE_OFF;
}

void CLogicState::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator; // save activator
	pev->frags = value;	// save value
	
	if(IsLockedByMaster( useType )) return;

	if ( useType == USE_TOGGLE )
	{
		// set use type
		if( m_iState == STATE_TURN_OFF || m_iState == STATE_OFF )
			useType = USE_ON;
		else if( m_iState == STATE_TURN_ON || m_iState == STATE_ON )
			useType = USE_OFF;
	}
	if ( useType == USE_ON )
	{
		//enable entity
		if ( m_iState == STATE_TURN_OFF || m_iState == STATE_OFF )
		{
			// activate turning off entity
 			if ( m_flDelay )
 			{
 				// we have time to turning on
				m_iState = STATE_TURN_ON;
				SetNextThink( m_flDelay );
			}
			else
			{
				// just enable entity
                              	m_iState = STATE_ON;
				UTIL_FireTargets( pev->target, pActivator, this, USE_ON, pev->frags );
				DontThink(); // break thinking
			}
		}
	}
	else if( useType == USE_OFF )
	{
		// disable entity
		if ( m_iState == STATE_TURN_ON || m_iState == STATE_ON )
		{
			// deactivate turning on entity
 			if ( m_flWait )
 			{
 				// we have time to turning off
				m_iState = STATE_TURN_OFF;
				SetNextThink( m_flWait );
			}
			else
			{
				// just disable entity
				m_iState = STATE_OFF;
				UTIL_FireTargets( pev->target, pActivator, this, USE_OFF, pev->frags );
				DontThink();//break thinking
			}
		}
	}
 	else if( useType == USE_SET )
 	{
 		// explicit on 
 		m_iState = STATE_ON;
		DontThink();
	}
	else if ( useType == USE_RESET )
	{
		// explicit off
		m_iState = STATE_OFF;
		DontThink();
	}
	else if ( useType == USE_SHOWINFO )
	{
		ALERT( at_console, "======/Xash Debug System/======\n");
		ALERT( at_console, "classname: %s\n", STRING(pev->classname));
		ALERT( at_console, "time, before enable %.1f, time, before disable %.1f\n", m_flDelay, m_flWait );
		ALERT( at_console, "current state %s\n", GetStringForState( GetState()) );
	}
}

void CLogicState::Think( void )
{
	if ( m_iState == STATE_TURN_ON )
	{
		m_iState = STATE_ON;
		UTIL_FireTargets( pev->target, m_hActivator, this, USE_ON, pev->frags );
	}
	else if ( m_iState == STATE_TURN_OFF )
	{
		m_iState = STATE_OFF;
		UTIL_FireTargets( pev->target, m_hActivator, this, USE_OFF, pev->frags );
	}
	DontThink();
}

//=======================================================================
// 		   Logic_usetype - sorting different usetypes
//=======================================================================
class CLogicUseType : public CBaseLogic
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};
LINK_ENTITY_TO_CLASS( logic_usetype, CLogicUseType );

void CLogicUseType::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "toggle" ))
	{
		pev->target = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "enable" ))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "disable" ))
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "set" ))
	{
		m_sSet = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "reset" ))
	{
		m_sReset = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseLogic::KeyValue( pkvd );
}

void CLogicUseType::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( IsLockedByMaster( useType )) return;
	if ( useType == USE_TOGGLE ) UTIL_FireTargets( pev->target, pActivator, this, useType, value );
	else if ( useType == USE_ON )	UTIL_FireTargets( pev->message, pActivator, this, useType, value );
	else if ( useType == USE_OFF ) UTIL_FireTargets( pev->netname, pActivator, this, useType, value );
	else if ( useType == USE_SET ) UTIL_FireTargets( m_sSet, pActivator, this, useType, value );
	else if ( useType == USE_RESET )UTIL_FireTargets( m_sReset, pActivator, this, useType, value );
	else if ( useType == USE_SHOWINFO )
	{
		ALERT( at_console, "======/Xash Debug System/======\n" );
		ALERT( at_console, "classname: %s\n", STRING( pev->classname ));
		ALERT( at_console, "This entity doesn't have displayed settings!\n\n" );
	}
}

//=======================================================================
// 		   Logic_scale - apply scale for value
//=======================================================================
class CLogicScale : public CBaseLogic
{
public:
	void KeyValue( KeyValueData *pkvd )
	{
		if (FStrEq(pkvd->szKeyName, "mode"))
		{
			pev->impulse = atoi( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else	CBaseLogic::KeyValue( pkvd );
	}
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		if ( useType == USE_SHOWINFO )
		{
			DEBUGHEAD;
			ALERT( at_console, "Mode %s. Scale %.3f.\n\n", pev->impulse ? "Bool" : "Float", pev->scale );
			SHIFT;
		}
		else
		{
			if( pev->impulse == 0 ) // bool logic
			{
				if( value >= 0.99f ) UTIL_FireTargets(pev->target, pActivator, this, USE_ON, 1 );
				if( value <= 0.01f ) UTIL_FireTargets(pev->target, pActivator, this, USE_OFF,0 );
			}
			if( pev->impulse == 1 ) // direct scale
				UTIL_FireTargets( pev->target, pActivator, this, USE_SET, value * pev->scale );
			if( pev->impulse == 2 ) // inverse sacle
				UTIL_FireTargets( pev->target, pActivator, this, USE_SET, pev->scale * ( 1 - value ));
		}         	
	}
};
LINK_ENTITY_TO_CLASS( logic_scale, CLogicScale );			