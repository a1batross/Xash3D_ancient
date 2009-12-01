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
	if (FStrEq(pkvd->szKeyName, "delay") || FStrEq(pkvd->szKeyName, "MaxDelay"))
	{
		m_flDelay = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "wait") || FStrEq(pkvd->szKeyName, "MaxWait"))
	{
		m_flWait = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "master"))
	{
		m_sMaster = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "killtarget"))
	{	
		m_iszKillTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "globalstate"))
	{
		m_globalstate = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	CBaseEntity::KeyValue( pkvd );
}

void CBaseLogic :: FireTargets( USE_TYPE useType, float value )
{
	//fire our targets
	UTIL_FireTargets( pev->target, m_hActivator, this, useType, value );
	UTIL_FireTargets( m_iszKillTarget, m_hActivator, this, USE_REMOVE );
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
	DEFINE_FIELD( CBaseLogic, m_hActivator, FIELD_EHANDLE ),
	DEFINE_FIELD( CBaseLogic, m_hTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( CBaseLogic, m_iState, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseLogic, m_sMaster, FIELD_STRING),
	DEFINE_FIELD( CBaseLogic, m_iszKillTarget, FIELD_STRING ),
	DEFINE_FIELD( CBaseLogic, m_sSet, FIELD_STRING),
	DEFINE_FIELD( CBaseLogic, m_sReset, FIELD_STRING),
	DEFINE_FIELD( CBaseLogic, m_flWait, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseLogic, m_globalstate, FIELD_STRING ),
};IMPLEMENT_SAVERESTORE( CBaseLogic, CBaseEntity );

//=======================================================================
// 		   Logic_generator
//=======================================================================
#define NORMAL_MODE			0
#define RANDOM_MODE			1
#define RANDOM_MODE_WITH_DEC		2

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
	if(pev->button == RANDOM_MODE || pev->button == RANDOM_MODE_WITH_DEC)
	{ 	//set delay with decceleration
		if (!m_flDelay || pev->button == RANDOM_MODE_WITH_DEC) m_flDelay = 0.005;
		//generate max count automaticallly, if not set on map
		if (!m_iMaxFireCount) m_iMaxFireCount = RANDOM_LONG(100, 200);
	}
	else
	{
		//Smart Field System ©
		if (!m_iMaxFireCount) m_iMaxFireCount = -1;//disable counting for normal mode
	}
	if(pev->spawnflags & SF_START_ON)
	{
		m_iState = STATE_ON;//initialy off in random mode
		SetNextThink(m_flDelay);
	}
}

TYPEDESCRIPTION	CGenerator :: m_SaveData[] = 
{
	DEFINE_FIELD( CGenerator, m_iMaxFireCount, FIELD_INTEGER ),
	DEFINE_FIELD( CGenerator, m_iFireCount, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CGenerator, CBaseLogic );

void CGenerator :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "maxcount"))
	{
		m_iMaxFireCount = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "mode"))
	{
		pev->button = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBaseLogic::KeyValue( pkvd );
}

void CGenerator :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (useType == USE_TOGGLE)
	{
		if(m_iState) useType = USE_OFF;
		else useType = USE_ON;
	}
	if (useType == USE_ON)
	{
		if(pev->button == RANDOM_MODE || pev->button == RANDOM_MODE_WITH_DEC)
		{
			if(pev->button == RANDOM_MODE_WITH_DEC) m_flDelay = 0.005;
			m_iFireCount = RANDOM_LONG(0, m_iMaxFireCount/2);
		}
		m_iState = STATE_ON;
		SetNextThink( 0 );//immediately start firing targets
	}
	else if (useType == USE_OFF)
	{
		m_iState = STATE_OFF;
		DontThink();
	}
	else if (useType == USE_SET) //set max count of impulses
		m_iMaxFireCount = value;
		
	else if (useType == USE_RESET) //immediately reset
		m_iFireCount = 0;

	else if (useType == USE_SHOWINFO)
	{
		ALERT(at_console, "======/Xash Debug System/======\n");
		ALERT(at_console, "classname: %s\n", STRING(pev->classname));
		ALERT(at_console, "State: %s, Delay time %f\n", GetStringForState( GetState()), m_flDelay);
		ALERT(at_console, "FireCount: %d, Max FireCount: %d\n", m_iFireCount, m_iMaxFireCount);
	}
}

void CGenerator :: Think( void )
{
	if(m_iFireCount != -1)//if counter enabled
	{
		if(m_iFireCount == m_iMaxFireCount)
		{
			m_iFireCount = NULL;
			DontThink();
			m_iState = STATE_OFF;
			return;
		}
 		else m_iFireCount++;
	}
	if(pev->button == RANDOM_MODE_WITH_DEC)
	{	//deceleration for random mode
		if(m_iMaxFireCount - m_iFireCount < 40) m_flDelay += 0.005;
	}
	UTIL_FireTargets( pev->target, this, this, USE_TOGGLE );
	SetNextThink(m_flDelay);
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
};
LINK_ENTITY_TO_CLASS( logic_switcher, CSwitcher );

// Global Savedata for switcher
TYPEDESCRIPTION	CSwitcher::m_SaveData[] = 
{	DEFINE_FIELD( CSwitcher, m_cTargets, FIELD_INTEGER ),
	DEFINE_FIELD( CSwitcher, m_index, FIELD_INTEGER ),
	DEFINE_ARRAY( CSwitcher, m_iTargetName, FIELD_STRING, MAX_MULTI_TARGETS ),
};IMPLEMENT_SAVERESTORE(CSwitcher, CBaseLogic);

void CSwitcher :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "mode"))
	{
		pev->button = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( m_cTargets < MAX_MULTI_TARGETS ) 
	{
		// add this field to the target list
		// this assumes that additional fields are targetnames and their values are delay values.
		
		char tmp[128];
		UTIL_StripToken( pkvd->szKeyName, tmp );
		m_iTargetName [ m_cTargets ] = ALLOC_STRING( tmp );
		m_cTargets++;
		pkvd->fHandled = TRUE;
	}
}

void CSwitcher :: Spawn( void )
{
	int r_index = 0;
	int w_index = m_cTargets - 1;

	while(r_index < w_index)
	{
		//we store target with right index in tempname
		int name = m_iTargetName [r_index];
		
		//target with right name is free, record new value from wrong name
		m_iTargetName [r_index] = m_iTargetName [w_index];
		
		//ok, we can swap targets
		m_iTargetName [w_index] = name;
		r_index++;
		w_index--;
	}
	
	m_iState = STATE_OFF;
	m_index = 0;
	if(pev->spawnflags & SF_START_ON)
	{
		m_iState = STATE_ON;
 		SetNextThink (m_flDelay);
	}	
}

void CSwitcher :: Think ( void )
{
	if(pev->button == MODE_INCREMENT)//increase target number
	{
		m_index++;
		if(m_index >= m_cTargets) m_index = 0;
	}
	else if(pev->button == MODE_DECREMENT)
	{
		m_index--;
		if(m_index < 0) m_index = m_cTargets - 1;
	}
	else if(pev->button == MODE_RANDOM_VALUE)
	{
		m_index = RANDOM_LONG (0, m_cTargets - 1);
	}
	SetNextThink (m_flDelay);
}

void CSwitcher :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if(IsLockedByMaster( useType )) return;

	if (useType == USE_SET)//set new target for activate (direct choose or increment\decrement)
	{         
		if(pev->spawnflags & SF_START_ON)
		{
			m_iState = STATE_ON;
			SetNextThink (m_flDelay);
			return;
		}
		//set maximum priority for direct choose
		if(value) 
		{
			m_index = (value - 1);
			if(m_index >= m_cTargets) m_index = -1;
			return;
		}
		if(pev->button == MODE_INCREMENT)
		{
			m_index++;
			if(m_index >= m_cTargets) m_index = 0; 	
		}
                    else if(pev->button == MODE_DECREMENT)//increase target number
		{
			m_index--;
			if(m_index < 0) m_index = m_cTargets - 1;
		}
		else if(pev->button == MODE_RANDOM_VALUE)
		{
			m_index = RANDOM_LONG (0, m_cTargets - 1);
		}
	}
	else if (useType == USE_RESET)
	{
		//reset switcher
		m_iState = STATE_OFF;
		DontThink();
		m_index = 0;
		return;
	}
	else if (useType == USE_SHOWINFO)
	{
		ALERT(at_console, "======/Xash Debug System/======\n");
		ALERT(at_console, "classname: %s\n", STRING(pev->classname));
		ALERT(at_console, "State: %s, number of targets %d\n", GetStringForState(GetState()), m_cTargets-1);
		ALERT(at_console, "Current target %s, target number %d\n", STRING(m_iTargetName[m_index]), m_index);
	}
	else if(m_index != -1)//fire any other USE_TYPE and right index
	{
		UTIL_FireTargets( m_iTargetName[ m_index ], m_hActivator, this, useType, value );
	}
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
	if (FStrEq(pkvd->szKeyName, "count"))
	{
		pev->frags = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "maxcount"))
	{
		pev->body = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBaseLogic::KeyValue( pkvd );
}

void CLogicCounter :: Spawn( void )
{
	//Smart Field System ©
	if (pev->frags == 0) pev->frags = 2;
	if (pev->body == 0) pev->body = 100;
	if (pev->frags == -1) pev->frags = RANDOM_LONG (1, pev->body);

	//save number of impulses
	pev->impulse = pev->frags;
	m_iState = STATE_OFF;//always disable
}

void CLogicCounter :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (useType == USE_SET)
	{
		if(value) pev->impulse = pev->frags = value; //write new value
		else pev->impulse = pev->frags = RANDOM_LONG (1, pev->body);//write random value
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
	if (FStrEq(pkvd->szKeyName, "toggle"))
	{
		pev->target = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "enable"))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "disable"))
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "set"))
	{
		m_sSet = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "reset"))
	{
		m_sReset = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	CBaseLogic::KeyValue( pkvd );
}

void CLogicUseType::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if(IsLockedByMaster( useType )) return;
	if(useType == USE_TOGGLE)	UTIL_FireTargets(pev->target, pActivator, this, useType, value );
	else if (useType == USE_ON)	UTIL_FireTargets(pev->message, pActivator, this, useType, value );
	else if (useType == USE_OFF)	UTIL_FireTargets(pev->netname, pActivator, this, useType, value );
	else if (useType == USE_SET)	UTIL_FireTargets(m_sSet, pActivator, this, useType, value );
	else if (useType == USE_RESET)UTIL_FireTargets(m_sReset, pActivator, this, useType, value );
	else if (useType == USE_SHOWINFO)
	{
		ALERT(at_console, "======/Xash Debug System/======\n");
		ALERT(at_console, "classname: %s\n", STRING(pev->classname));
		ALERT(at_console, "This entity don't displayed settings!\n" );
		SHIFT;
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
		if (useType == USE_SHOWINFO)
		{
			DEBUGHEAD;
			ALERT(at_console, "Mode %s. Scale %.3f.\n", pev->impulse ? "Bool" : "Float", pev->scale);
			SHIFT;
		}
		else
		{
			if(pev->impulse == 0)//bool logic
			{
				if(value >= 0.99)UTIL_FireTargets(pev->target, pActivator, this, USE_ON, 1 );
				if(value <= 0.01)UTIL_FireTargets(pev->target, pActivator, this, USE_OFF,0 );
			}
			if(pev->impulse == 1)//direct scale
				UTIL_FireTargets(pev->target, pActivator, this, USE_SET, value * pev->scale );
			if(pev->impulse == 2)//inverse sacle
				UTIL_FireTargets(pev->target, pActivator, this, USE_SET, pev->scale*(1-value));
		}         	
	}
};
LINK_ENTITY_TO_CLASS( logic_scale, CLogicScale);

/*QUAKED func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) START_ON
"wait"			base time between triggering all targets, default is 1
"random"		wait variance, default is 0

so, the basic time between firing is a random time between
(wait - random) and (wait + random)

"delay"			delay before first firing when turned on, default is 0

"pausetime"		additional delay used only the very first time
				and only if spawned with START_ON

These can used but not touched.
*/
class CLogicTimer : public CBaseLogic
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Think( void );
};
LINK_ENTITY_TO_CLASS( func_timer, CLogicTimer );
LINK_ENTITY_TO_CLASS( logic_timer, CLogicTimer );

void CLogicTimer :: Think( void )
{
	UTIL_FireTargets( pev->target, m_hActivator, this, USE_TOGGLE, 0.0f );
	SetNextThink( m_flWait + RANDOM_FLOAT( 0.0f, 1.0f ) * pev->frags );
}

void CLogicTimer :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "random" ))
	{
		pev->frags = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if ( FStrEq( pkvd->szKeyName, "pausetime" ))
	{
		pev->health = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CBaseLogic::KeyValue( pkvd );
}

void CLogicTimer :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	// if on, turn it off
	if( pev->nextthink )
	{
		DontThink ();
		return;
	}

	// turn it on
	if( m_flDelay )
		SetNextThink( m_flDelay );
	else SetNextThink( 0 );
}

void CLogicTimer :: Spawn( void )
{
	if ( pev->frags <= 0.0f )
		pev->frags = 1.0f;
	if ( !m_flWait ) m_flWait = 1.0f;

	if ( pev->frags >= m_flWait )
		pev->frags = m_flWait - gpGlobals->frametime;

	if ( pev->spawnflags & 1 )
	{
		SetNextThink( 1.0f + pev->health + m_flDelay + m_flWait + RANDOM_FLOAT( 0.0f, 1.0f ) * pev->frags );
		m_hActivator = this;
	}
}

/*QUAKED target_delay (1 0 0) (-8 -8 -8) (8 8 8)
"wait" seconds to pause before firing targets.
"random" delay variance, total delay = delay +/- random seconds
*/
class CLogicDelay : public CBaseLogic
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Think( void );
};
LINK_ENTITY_TO_CLASS( target_delay, CLogicDelay );

void CLogicDelay :: Think( void )
{
	UTIL_FireTargets( pev->target, m_hActivator, this, USE_TOGGLE, 0.0f );
}

void CLogicDelay :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "random" ))
	{
		pev->frags = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CBaseLogic::KeyValue( pkvd );
}

void CLogicDelay :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetNextThink( m_flWait + RANDOM_FLOAT( 0.0f, 1.0f ) * pev->frags );
	m_hActivator = pActivator;
}

void CLogicDelay :: Spawn( void )
{
	if ( !m_flWait ) m_flWait = 1.0f;
}
			