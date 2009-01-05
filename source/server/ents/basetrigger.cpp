//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================
#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "player.h"
#include "baseweapon.h"

//=======================================================================
// func_trigger - volume, that fire target when player in or out
//=======================================================================
#define SF_TRIGGER_ALLOWMONSTERS	1 // monsters allowed to fire this trigger
#define SF_TRIGGER_NOCLIENTS		2 // players not allowed to fire this trigger
#define SF_TRIGGER_PUSHABLES		4 // only pushables can fire this trigger

class CBaseTrigger;
class CInOutRegister : public CPointEntity
{
public:
	BOOL IsRegistered ( CBaseEntity *pValue );
	CInOutRegister *Prune( void );
	CInOutRegister *Add( CBaseEntity *pValue );
	BOOL IsEmpty( void ) { return m_pNext ? FALSE:TRUE; };

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	CBaseTrigger *m_pField;
	CInOutRegister *m_pNext;
	EHANDLE m_hValue;
};

class CBaseTrigger : public CBaseLogic
{
public:
	void Spawn( void );
	void EXPORT Touch( CBaseEntity *pOther );
	void EXPORT Update( void );
	void EXPORT Remove( void ); //generic method for right delete trigger class
	virtual void FireOnEntry( CBaseEntity *pOther ){}
	virtual void FireOnLeave( CBaseEntity *pOther ){}

	BOOL CanTouch( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	STATE GetState() { return m_pRegister->IsEmpty()? STATE_OFF : STATE_ON; }
	CInOutRegister *m_pRegister;
};
TYPEDESCRIPTION	CBaseTrigger::m_SaveData[] =
{ DEFINE_FIELD( CBaseTrigger, m_pRegister, FIELD_CLASSPTR ), };
IMPLEMENT_SAVERESTORE(CBaseTrigger, CBaseLogic);

TYPEDESCRIPTION	CInOutRegister::m_SaveData[] =
{	DEFINE_FIELD( CInOutRegister, m_pField, FIELD_CLASSPTR ),
	DEFINE_FIELD( CInOutRegister, m_pNext, FIELD_CLASSPTR ),
	DEFINE_FIELD( CInOutRegister, m_hValue, FIELD_EHANDLE ),
}; IMPLEMENT_SAVERESTORE(CInOutRegister,CPointEntity);
LINK_ENTITY_TO_CLASS( zoneent, CInOutRegister );

BOOL CInOutRegister::IsRegistered ( CBaseEntity *pValue )
{
	if (m_hValue == pValue) return TRUE;
	else if (m_pNext) return m_pNext->IsRegistered( pValue );
	else return FALSE;
}

CInOutRegister *CInOutRegister::Add( CBaseEntity *pValue )
{
	if (m_hValue == pValue)
	{
		// it's already in the list, don't need to do anything
		return this;
	}
	else if (m_pNext)
	{
		// keep looking
		m_pNext = m_pNext->Add( pValue );
		return this;
	}
	else
	{
		// reached the end of the list; add the new entry, and trigger
		CInOutRegister *pResult = GetClassPtr( (CInOutRegister*)NULL );
		pResult->m_hValue = pValue;
		pResult->m_pNext = this;
		pResult->m_pField = m_pField;
		pResult->pev->classname = MAKE_STRING("zoneent");
		m_pField->FireOnEntry( pValue );
		return pResult;
	}
}

CInOutRegister *CInOutRegister::Prune( void )
{
	if ( m_hValue )
	{
		ASSERTSZ(m_pNext != NULL, "invalid InOut registry terminator\n");
		if ( m_pField->Intersects(m_hValue) )
		{
			// this entity is still inside the field, do nothing
			m_pNext = m_pNext->Prune();
			return this;
		}
		else
		{
			// this entity has just left the field, trigger
			m_pField->FireOnLeave( m_hValue );
			SetThink( Remove );
			SetNextThink( 0.1 );
			return m_pNext->Prune();
		}
	}
	else
	{	// this register has a missing or null value
		if (m_pNext)
		{
			// this is an invalid list entry, remove it
			SetThink( Remove );
			SetNextThink( 0.1 );
			return m_pNext->Prune();
		}
		else
		{
			// this is the list terminator, leave it.
			return this;
		}
	}
}

void CBaseTrigger::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "offtarget"))
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "modifier"))
	{
		pev->armorvalue = atof(pkvd->szValue) / 100.0;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "roomtype")) //for soundfx
	{
		pev->button = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	CBaseLogic::KeyValue( pkvd );
}

void CBaseTrigger :: Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_NO;
	UTIL_SetModel( ENT( pev ), pev->model );    // set size and link into world
	SetObjectClass( ED_TRIGGER );
          
	// create a null-terminator for the registry
	m_pRegister = GetClassPtr(( CInOutRegister *)NULL );
	m_pRegister->m_hValue = NULL;
	m_pRegister->m_pNext = NULL;
	m_pRegister->m_pField = this;
	m_pRegister->pev->classname = MAKE_STRING("zoneent");
	m_pRegister->SetObjectClass( ED_STATIC );

	SetThink( Update );
}

BOOL CBaseTrigger :: CanTouch( CBaseEntity *pOther )
{
	if ( gpGlobals->time < pev->dmgtime) return FALSE;
	pev->dmgtime = gpGlobals->time + m_flWait;
	
	// Only touch clients, monsters, or pushables (depending on flags)
	if (pOther->pev->flags & FL_CLIENT) return !(pev->spawnflags & SF_TRIGGER_NOCLIENTS);
	else if (pOther->pev->flags & FL_MONSTER) return pev->spawnflags & SF_TRIGGER_ALLOWMONSTERS;
	else if (pOther->IsPushable()) return pev->spawnflags & SF_TRIGGER_PUSHABLES;
	return FALSE;
}

void CBaseTrigger :: Touch( CBaseEntity *pOther )
{
	if (!CanTouch(pOther)) return;
          m_hActivator = pOther; //save activator;
	
	m_pRegister = m_pRegister->Add(pOther);
	if (m_fNextThink <= 0 && !m_pRegister->IsEmpty())SetNextThink( 0.1 );
}

void CBaseTrigger :: Update( void )
{
	// Prune handles all Intersects tests and fires targets as appropriate
	m_pRegister = m_pRegister->Prune();

	if (m_pRegister->IsEmpty()) DontThink();
	else SetNextThink( 0.1 );
}

void CBaseTrigger :: Remove( void )
{
	UTIL_Remove( m_pRegister );
	UTIL_Remove( this );
}

//=======================================================================
//		trigger_mutiple - classic QUAKE trigger
//=======================================================================
class CTriggerMulti : public CBaseTrigger
{
	void FireOnEntry( CBaseEntity *pOther )
	{
		if(!IsLockedByMaster(pOther)) FireTargets();
	}
	void FireOnLeave( CBaseEntity *pOther )
	{
		if (!IsLockedByMaster(pOther))
			UTIL_FireTargets(pev->netname, pOther, this, USE_TOGGLE );
	}
};
LINK_ENTITY_TO_CLASS( trigger_multiple, CTriggerMulti );

//=======================================================================
//		trigger_once - classic QUAKE trigger
//=======================================================================
class CTriggerOnce : public CBaseTrigger
{
	void FireOnEntry( CBaseEntity *pOther )
	{
		if ( !IsLockedByMaster(pOther))
		{
			FireTargets();
			SetThink( Remove );
			SetNextThink( 0 );
		}
	}
};
LINK_ENTITY_TO_CLASS( trigger_once, CTriggerOnce );

//=======================================================================
//		trigger_autosave - game autosave
//=======================================================================
class CTriggerSave : public CBaseTrigger
{
	void FireOnEntry( CBaseEntity *pOther ) { SERVER_COMMAND( "autosave\n" ); }
	void FireOnLeave( CBaseEntity *pOther )
	{
		SetThink( Remove );
		SetNextThink( 0 );
	}
};
LINK_ENTITY_TO_CLASS( trigger_autosave, CTriggerSave );

//=======================================================================
//		func_friction - game autosave
//=======================================================================
class CChangeFriction : public CBaseTrigger
{
	void Spawn( void )
	{
		pev->solid = SOLID_TRIGGER;
		pev->movetype = MOVETYPE_NONE;
		UTIL_SetModel(ENT(pev), pev->model );
	}
	void Touch( CBaseEntity *pOther )
	{
		if ( pOther->pev->movetype != 11 && pOther->pev->movetype != 10 )
			pOther->pev->friction = pev->armorvalue;
	}
};
LINK_ENTITY_TO_CLASS( func_friction, CChangeFriction );

//=======================================================================
//		trigger_push - triger that pushes player
//=======================================================================
class CTriggerPush : public CBaseTrigger
{
	void Spawn( void )
	{
                    if ( pev->angles == g_vecZero ) pev->angles.y = 360;
                    if (pev->speed == 0) pev->speed = 100;
                    UTIL_LinearVector( this );
                    
		if ( FBitSet (pev->spawnflags, 2) ) pev->solid = SOLID_NOT;
		else pev->solid = SOLID_TRIGGER;
		
		pev->movetype = MOVETYPE_NONE;
		UTIL_SetModel(ENT(pev), pev->model );
		SetBits( pev->effects, EF_NODRAW );
		UTIL_SetOrigin( this, pev->origin );
	}
	void Use ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		if( pev->solid == SOLID_NOT )
		{
			pev->solid = SOLID_TRIGGER;
			pev->contents = CONTENTS_TRIGGER;
		}
		else pev->solid = SOLID_NOT;
		UTIL_SetOrigin( this, pev->origin );
	}
	void Touch( CBaseEntity *pOther )
	{
		switch( pOther->pev->movetype )
		{
		case MOVETYPE_NONE:
		case MOVETYPE_PUSH:
		case MOVETYPE_NOCLIP:
		case MOVETYPE_FOLLOW:
			return;
		}
                    
		if ( pOther->pev->solid != SOLID_NOT && pOther->pev->solid != SOLID_BSP )
		{
			// Instant trigger, just transfer velocity and remove
			if (FBitSet(pev->spawnflags, 1))
			{
				pOther->pev->velocity = pOther->pev->velocity + (pev->speed * pev->movedir);
				if ( pOther->pev->velocity.z > 0 ) pOther->pev->flags &= ~FL_ONGROUND;
				UTIL_Remove( this );
			}
			else
			{
				Vector vecPush = (pev->speed * pev->movedir);
				vecPush = vecPush + pOther->pev->velocity;
				pOther->pev->velocity = vecPush;
			}
		}
	}
};
LINK_ENTITY_TO_CLASS( trigger_push, CTriggerPush );

//=======================================================================
//		trigger_gravity - classic HALF_LIFE gravity modifier
//=======================================================================
class CTriggerGravity : public CBaseTrigger
{
	void FireOnEntry( CBaseEntity *pOther ) { pOther->pev->gravity = pev->gravity; }
	//void FireOnLeave( CBaseEntity *pOther ) { pOther->pev->gravity = EARTH_GRAVITY;}
};
LINK_ENTITY_TO_CLASS( trigger_gravity, CTriggerGravity);

//=======================================================================
//		trigger_sound - a DSP zone, brush prototype of env_sound
//=======================================================================
class CTriggerSound : public CBaseTrigger
{
	void FireOnEntry( CBaseEntity *pOther )
	{
		if (!pOther->IsPlayer()) return;
		pev->impulse = ((CBasePlayer *)pOther)->m_iSndRoomtype; //save old dsp sound
		((CBasePlayer *)pOther)->m_iSndRoomtype = pev->button; //set new dsp
		((CBasePlayer *)pOther)->hearNeedsUpdate = 1;
	}
	void FireOnLeave( CBaseEntity *pOther )
	{
		if (!pOther->IsPlayer()) return;
		((CBasePlayer *)pOther)->m_iSndRoomtype = pev->impulse; //restore old dsp
		((CBasePlayer *)pOther)->hearNeedsUpdate = 1;
	}
};
LINK_ENTITY_TO_CLASS( trigger_sound, CTriggerSound );

//=======================================================================
// 		   trigger_relay
//=======================================================================
class CTriggerRelay : public CBaseLogic
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Spawn( void ){ m_iState = STATE_OFF; }
	void Think ( void );
};
LINK_ENTITY_TO_CLASS( trigger_relay, CTriggerRelay );

void CTriggerRelay::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "triggerstate"))
	{
		int type = atoi( pkvd->szValue );
		switch( type )
		{
		case 0: pev->impulse = USE_OFF; break;
		case 2: pev->impulse = USE_TOGGLE; break;
		case 3: pev->impulse = USE_SET; break;
		default: pev->impulse = USE_ON; break;
		}
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "locktarget"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBaseLogic::KeyValue( pkvd );
}

void CTriggerRelay::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;		//save activator
	if(pev->impulse) pev->button = pev->impulse;
	else pev->button = (int)useType;	//save use type
	pev->frags = value;			//save our value

	if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		Msg( "target is %s, locktarget %s\n", STRING(pev->target), STRING(pev->message));
		Msg( "new value %g\n", pev->frags );
	}
	else //activate target
	{
		m_iState = STATE_ON;
		SetNextThink(m_flDelay);
	}
}

void CTriggerRelay::Think ( void )
{
	if (IsLockedByMaster()) UTIL_FireTargets( pev->message, m_hActivator, this, (USE_TYPE)pev->button, pev->frags );
	else
	{
		UTIL_FireTargets( pev->target, m_hActivator, this, (USE_TYPE)pev->button, pev->frags );
          	UTIL_FireTargets( m_iszKillTarget, m_hActivator, this, USE_REMOVE );
          }
          
	//suhtdown
	m_iState = STATE_OFF;
	DontThink();//just in case
	if ( pev->spawnflags & 1 ) UTIL_Remove( this );
}

//=======================================================================
// 		   trigger_auto
//=======================================================================
class CAutoTrigger : public CBaseLogic
{
public:
	void KeyValue( KeyValueData *pkvd );
	void PostActivate( void );
};
LINK_ENTITY_TO_CLASS( trigger_auto, CAutoTrigger );

void CAutoTrigger::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "triggerstate"))
	{
		int type = atoi( pkvd->szValue );
		switch( type )
		{
		case 0: pev->impulse = USE_OFF; break;
		case 2: pev->impulse = USE_TOGGLE; break;
		case 3: pev->impulse = USE_SET; break;
		default: pev->impulse = USE_ON; break;
		}
		pkvd->fHandled = TRUE;
	}
	else CBaseLogic::KeyValue( pkvd );
}

void CAutoTrigger::PostActivate( void )
{
	if ( !m_globalstate || gGlobalState.EntityGetState( m_globalstate ) == GLOBAL_ON )
	{
		FireTargets( (USE_TYPE)pev->impulse );
		if ( pev->spawnflags & 1 ) UTIL_Remove( this );
	}
}

//=======================================================================
// 		   trigger_changetarget
//=======================================================================
class CChangeTarget : public CBaseLogic
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};
LINK_ENTITY_TO_CLASS( trigger_changetarget, CChangeTarget );

void CChangeTarget::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "newtarget") || FStrEq(pkvd->szKeyName, "m_iszNewTarget"))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseLogic::KeyValue( pkvd );
}

void CChangeTarget::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ), pActivator );

	if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		Msg( "Current target %s, new target %s\n", STRING(pev->target), STRING(pev->message) );
		Msg( "target entity is: %s, current target: %s\n", STRING(pTarget->pev->classname), STRING(pTarget->pev->target));
	}
	else
	{
		if (pTarget)
		{
			if (FStrEq(STRING(pev->message), "*this"))
			{
				if (pActivator) pTarget->pev->target = pActivator->pev->targetname;
				else ALERT(at_error, "util_settarget \"%s\" requires a self pointer!\n", STRING(pev->targetname));
			}
			else 
			{
				if(pTarget->IsFuncScreen()) pTarget->ChangeCamera( pev->message );
				else pTarget->pev->target = pev->message;
			}
			CBaseMonster *pMonster = pTarget->MyMonsterPointer( );
			if (pMonster) pMonster->m_pGoalEnt = NULL;
			
		}
	}
}

//=======================================================================
// 		   multi_manager
//=======================================================================
#define FL_CLONE			0x80000000

class CMultiManager : public CBaseLogic
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn ( void );
	void Think ( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
 	BOOL HasTarget( string_t targetname );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];


	int	m_cTargets;	// the total number of targets in this manager's fire list.
	int	m_index;		// Current target
	float	m_startTime;	// Time we started firing
	int	m_iTargetName   [ MAX_MULTI_TARGETS ];// list if indexes into global string array
	float	m_flTargetDelay [ MAX_MULTI_TARGETS ];// delay (in seconds)
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
LINK_ENTITY_TO_CLASS( multi_manager, CMultiManager );

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
	if ( m_cTargets < MAX_MULTI_TARGETS )
	{
		char tmp[128];

		UTIL_StripToken( pkvd->szKeyName, tmp );
		m_iTargetName [ m_cTargets ] = ALLOC_STRING( tmp );
		m_flTargetDelay [ m_cTargets ] = RandomRange((char *)STRING(ALLOC_STRING(pkvd->szValue))).Random();
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

BOOL CMultiManager::HasTarget( string_t targetname )
{
	for ( int i = 0; i < m_cTargets; i++ )
		if ( FStrEq(STRING(targetname), STRING(m_iTargetName[i])) ) return TRUE;
	return FALSE;
}

void CMultiManager :: Think ( void )
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
		if ( IsClone() )
		{
			UTIL_Remove( this );
                   		return;
                   	}

		//stop manager
                   	m_iState = STATE_OFF;
		DontThink();
		return;
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
	pMulti->m_flWait = m_flWait;
	pMulti->m_iState = m_iState;
	memcpy( pMulti->m_iTargetName, m_iTargetName, sizeof( m_iTargetName ) );
	memcpy( pMulti->m_flTargetDelay, m_flTargetDelay, sizeof( m_flTargetDelay ) );

	return pMulti;
}

void CMultiManager :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if(IsLockedByMaster( useType )) return;
	pev->frags = value;//save our value
	
	if (useType == USE_TOGGLE || useType == USE_ON)
	{
	          if ( ShouldClone() )//create clone if needed
		{
			CMultiManager *pClone = Clone();
			pClone->Use( pActivator, pCaller, useType, value );
			return;
		}
		if(m_iState == STATE_OFF)
		{
			m_startTime = m_flWait + gpGlobals->time;
			m_iState = STATE_TURN_ON;
			m_index = 0;
			SetNextThink( m_flWait );
		}
	}	
	else if (useType == USE_OFF)
	{	
		m_iState = STATE_OFF;
		DontThink();
	}
	else if (useType == USE_SHOWINFO)//only show info if locked by master
	{
		DEBUGHEAD;
		Msg("State: %s, number of targets %d\n", GetStringForState( GetState()), m_cTargets);
		if(m_iState == STATE_ON) Msg("Current target %s, delay time %f\n", STRING(m_iTargetName[ m_index ]), m_flTargetDelay[ m_index ]);
		else Msg("No targets for firing.\n");
	}
}

//=======================================================================
// 		   multi_master
//=======================================================================
#define LOGIC_AND  	0	// fire if all objects active
#define LOGIC_OR   	1         // fire if any object active
#define LOGIC_NAND 	2         // fire if not all objects active
#define LOGIC_NOR  	3         // fire if all objects disable
#define LOGIC_XOR	4         // fire if only one (any) object active
#define LOGIC_XNOR	5         // fire if active any number objects, but < then all

#define ST_ON	0
#define ST_OFF	1
#define ST_TURNON	2
#define ST_TURNOFF	3
#define ST_IN_USE	4

class CMultiMaster : public CBaseLogic
{
public:
	void Spawn ( void ){ SetNextThink( 0.1 ); }
	void Think ( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual STATE GetState( void ) { return m_iState; };
	virtual STATE GetState( CBaseEntity *pActivator ) { return EvalLogic(pActivator)?STATE_ON:STATE_OFF; };

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int m_cTargets;// the total number of targets in this manager's fire list.
	int m_iTargetName[ MAX_MULTI_TARGETS ];// list of indexes into global string array
          int m_iTargetState[ MAX_MULTI_TARGETS ];//list of wishstate targets
	
	BOOL EvalLogic ( CBaseEntity *pEntity );
};
LINK_ENTITY_TO_CLASS( multi_watcher, CMultiMaster );

TYPEDESCRIPTION	CMultiMaster::m_SaveData[] =
{
	DEFINE_FIELD( CMultiMaster, m_cTargets, FIELD_INTEGER ),
	DEFINE_ARRAY( CMultiMaster, m_iTargetName, FIELD_STRING, MAX_MULTI_TARGETS ),
	DEFINE_ARRAY( CMultiMaster, m_iTargetState, FIELD_INTEGER, MAX_MULTI_TARGETS ),
};IMPLEMENT_SAVERESTORE(CMultiMaster,CBaseLogic);

void CMultiMaster :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "mode"))
	{
		pev->button = atof(pkvd->szValue);
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
			m_iTargetState [ m_cTargets ] = atoi (pkvd->szValue);
			m_cTargets++;
			pkvd->fHandled = TRUE;
		}
	}
}

void CMultiMaster :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		Msg("State: %s, Number of targets %d\n", GetStringForState( GetState()), m_cTargets);
		Msg("Limit is %d entities\n", MAX_MULTI_TARGETS);
	}
}

void CMultiMaster :: Think ( void )
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

BOOL CMultiMaster :: EvalLogic ( CBaseEntity *pActivator )
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
		case STATE_ON:	 if(m_iTargetState[i] == ST_ON)    	b = TRUE; break;
		case STATE_OFF:	 if(m_iTargetState[i] == ST_OFF)   	b = TRUE; break;
		case STATE_TURN_ON:	 if(m_iTargetState[i] == ST_TURNON)	b = TRUE; break;
		case STATE_TURN_OFF: if(m_iTargetState[i] == ST_TURNOFF)	b = TRUE; break;
		case STATE_IN_USE:	 if(m_iTargetState[i] == ST_IN_USE)	b = TRUE; break;
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
//		trigger_changelevel - classic HALF_LIFE changelevel
//=======================================================================
class CChangeLevel : public CBaseLogic
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	virtual int ObjectCaps( void ) { return CBaseLogic :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void ChangeLevelNow( CBaseEntity *pActivator );
	void Touch( CBaseEntity *pOther ){ if (pOther->IsPlayer())Think(); }
	void Think( void )
	{
		FireAfter();
		//SERVER_COMMAND( "intermission\n" );
		UTIL_ChangeLevel( pev->netname, pev->message );
	}
	void FireAfter( void );
};
LINK_ENTITY_TO_CLASS( trigger_changelevel, CChangeLevel );

void CChangeLevel :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "map"))
	{
		char m_szMapName[MAP_MAX_NAME];
		if (strlen(pkvd->szValue) >= MAP_MAX_NAME) 
			Msg("ERROR: Map name '%s' too long (%d chars)\n", pkvd->szValue, MAP_MAX_NAME );

		strcpy(m_szMapName, pkvd->szValue);
		for (int i = 0; m_szMapName[i]; i++) { m_szMapName[i] = tolower(m_szMapName[i]); }
		pev->netname = ALLOC_STRING( m_szMapName );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "landmark"))
	{
		if (strlen(pkvd->szValue) >= MAP_MAX_NAME)
			Msg("ERROR: Landmark name '%s' too long (%d chars)\n", pkvd->szValue, MAP_MAX_NAME );
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "changetarget"))
	{
		pev->target = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	CBaseEntity::KeyValue( pkvd );
}

void CChangeLevel :: Spawn( void )
{
	if( FStringNull( pev->netname ))
		ALERT( at_error, "a % doesn't have a map\n", STRING( pev->classname ));
	if( FStringNull( pev->message ))
		ALERT( at_error, "trigger_changelevel to %s doesn't have a landmark\n", STRING( pev->netname ));

	// determine work style
	if( FStringNull( pev->targetname )) SetUse( NULL );
	else SetTouch( NULL );

	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	UTIL_SetModel(ENT(pev), pev->model);
	SetBits( pev->effects, EF_NODRAW );//make invisible
}

void CChangeLevel :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		Msg( "Mapname: %s, Landmark name %f\n", STRING(pev->netname), STRING(pev->message));
		SHIFT;
	}
	else SetNextThink( 0 );
}

void CChangeLevel :: FireAfter( void )
{
	// Create an entity to fire the changetarget
	if (!FStringNull( pev->target ))
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( 1 );
		CBaseEntity *pChlFire = CBaseEntity::Create( "fireent", pPlayer->pev->origin, g_vecZero, NULL );
		if ( pChlFire ) pChlFire->pev->target = pev->target;
	}
}

//=========================================================
//		trigger_playerfreeze
//=========================================================
class CTriggerFreeze : public CBaseLogic
{
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		m_hActivator = pActivator;
		
		if (pActivator && pActivator->pev->flags & FL_CLIENT);
		else pActivator = UTIL_PlayerByIndex( 1 );

		if (useType == USE_TOGGLE)
		{
			if(m_iState == STATE_ON) useType = USE_OFF;
			else useType = USE_ON;
		}
		if (useType == USE_ON)
		{
			((CBasePlayer *)((CBaseEntity *)pActivator))->EnableControl(FALSE);
			m_iState = STATE_ON;
			if(m_flDelay) SetNextThink( m_flDelay );
		}
         		else if(useType == USE_OFF)
         		{
			((CBasePlayer *)((CBaseEntity *)pActivator))->EnableControl(TRUE);
			m_iState = STATE_OFF;
			DontThink();
         		}
		else if(useType == USE_SHOWINFO) { DEBUGHEAD; }
	}
	void Think( void )
	{
		((CBasePlayer *)((CBaseEntity *)m_hActivator))->EnableControl(TRUE);
		m_iState = STATE_OFF;
	}

};
LINK_ENTITY_TO_CLASS( trigger_playerfreeze, CTriggerFreeze );

//=======================================================================
//		trigger_hurt - classic QUAKE damage inflictor
//=======================================================================
class CTriggerHurt : public CBaseLogic
{
public:
	void Spawn( void );
	void Think( void );
	void Touch ( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	void Use ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};
LINK_ENTITY_TO_CLASS( trigger_hurt, CTriggerHurt );

void CTriggerHurt :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damagetype"))
	{
		pev->button |= atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBaseEntity::KeyValue( pkvd );
}

void CTriggerHurt :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	UTIL_SetModel(ENT(pev), pev->model);
	UTIL_SetOrigin( this, pev->origin );
	SetBits( pev->effects, EF_NODRAW );
	m_iState = STATE_OFF;
	                            
	if (!FBitSet(pev->spawnflags, 2 )) Use( this, this, USE_ON, 0 );
}

void CTriggerHurt :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (useType == USE_TOGGLE)
	{
		if(m_iState) useType = USE_OFF;
		else useType = USE_ON;
	}
	if (useType == USE_ON)
	{
		pev->solid = SOLID_TRIGGER;
		pev->contents = CONTENTS_TRIGGER;
		UTIL_SetOrigin( this, pev->origin );
		if (pev->button & DMG_RADIATION) SetNextThink( RANDOM_FLOAT(0.0, 0.5) );
		m_iState = STATE_ON;
	}
	else if (useType == USE_OFF)
	{
		pev->solid = SOLID_NOT;
		UTIL_SetOrigin( this, pev->origin );
		if (pev->button & DMG_RADIATION) DontThink();
		m_iState = STATE_OFF;
	}
	else if (useType == USE_SET)
	{
		pev->dmg = value;//set dmg level
	}	
	else if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		Msg( "State: %s, Dmg value %g\n", GetStringForState( GetState()), pev->dmg);
		PrintStringForDamage( pev->button );
	}
	
}

void CTriggerHurt :: Touch ( CBaseEntity *pOther )
{
	float fldmg;

	if ( !pOther->pev->takedamage ) return;
	if ( !FBitSet( pOther->pev->flags, FL_CLIENT|FL_MONSTER ) && !pOther->IsPushable() ) return;

	if ( IsMultiplayer() )
	{
		if ( pev->dmgtime > gpGlobals->time )
		{
			if ( gpGlobals->time != pev->pain_finished )
			{
				// too early to hurt again, and not same frame with a different entity
				if ( pOther->IsPlayer() )
				{
					int playerMask = 1 << (pOther->entindex() - 1);
					if ( pev->impulse & playerMask ) return;
					pev->impulse |= playerMask;
				}
				else return;
			}
		}
		else
		{
			pev->impulse = 0;
			if ( pOther->IsPlayer() )
			{
				int playerMask = 1 << (pOther->entindex() - 1);
				pev->impulse |= playerMask;
			}
		}
	}
	else if( pev->dmgtime > gpGlobals->time && gpGlobals->time != pev->pain_finished ) return;

	fldmg = pev->dmg * 0.5;	// 0.5 seconds worth of damage, pev->dmg is damage/second

	if ( fldmg < 0 ) pOther->TakeHealth( -fldmg, pev->button );
	else pOther->TakeDamage( pev, pev, fldmg, pev->button );

	pev->pain_finished = gpGlobals->time;
	pev->dmgtime = gpGlobals->time + 0.5;
}

void CTriggerHurt :: Think( void )
{
	edict_t *pentPlayer;
	CBasePlayer *pPlayer = NULL;
	float flRange;
	entvars_t *pevTarget;
	Vector vecSpot1;
	Vector vecSpot2;
	Vector vecRange;
	Vector origin;
	Vector view_ofs;

	origin = pev->origin;
	view_ofs = pev->view_ofs;

	pev->origin = (pev->absmin + pev->absmax) * 0.5;
	pev->view_ofs = pev->view_ofs * 0.0;
	pentPlayer = FIND_CLIENT_IN_PVS(edict());

	pev->origin = origin;
	pev->view_ofs = view_ofs;

	if (!FNullEnt(pentPlayer))
	{
		pPlayer = GetClassPtr( (CBasePlayer *)VARS(pentPlayer));
		pevTarget = VARS(pentPlayer);
		vecSpot1 = (pev->absmin + pev->absmax) * 0.5;
		vecSpot2 = (pevTarget->absmin + pevTarget->absmax) * 0.5;
		vecRange = vecSpot1 - vecSpot2;
		flRange = vecRange.Length();
		if (pPlayer->m_flgeigerRange >= flRange) pPlayer->m_flgeigerRange = flRange;
	}
	SetNextThink( 0.25 );
}

//=======================================================================
// trigger_transition - area that moving all entities inside across level 
//=======================================================================
class CVolumeTransition : public CPointEntity // Don't change this!
{
public:
	void Spawn( void )
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		UTIL_SetModel(ENT(pev), pev->model);
		pev->model = NULL;
		pev->modelindex = 0;
	}
};
LINK_ENTITY_TO_CLASS( trigger_transition, CVolumeTransition );

//=======================================================================
//	trigger_camera - generic camera
//=======================================================================
class CTriggerCamera : public CBaseLogic
{
	void Spawn (void );
	void PostSpawn( void );
	void UpdatePlayerView( void );
	void Think( void );
	void PostActivate( void );
	void OverrideReset( void );
	void Move( void );
	void TurnOn( void );
	void TurnOff(void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData* );
	int ObjectCaps( void ) { return CBaseLogic::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	CBaseEntity* pTarget;	
};
LINK_ENTITY_TO_CLASS( trigger_camera, CTriggerCamera );


#define SF_CAMERA_PLAYER_POSITION		1 //start from player eyes
#define SF_CAMERA_PLAYER_TARGET		2 //player it's camera target
#define SF_CAMERA_PLAYER_TAKECONTROL		4 //freeze player

void CTriggerCamera :: KeyValue( KeyValueData* pkvd )
{
	if (FStrEq(pkvd->szKeyName, "viewentity"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "moveto"))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	CBaseLogic::KeyValue( pkvd );
}

void CTriggerCamera :: Spawn (void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;

	m_iState = STATE_OFF;
	
	UTIL_SetModel(ENT(pev),"models/common/null.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetBits( pev->flags, FL_POINTENTITY );
}

void CTriggerCamera::PostSpawn( void )
{
	m_pGoalEnt = UTIL_FindEntityByTargetname (NULL, STRING(pev->message));
	if(m_pGoalEnt) UTIL_SetOrigin( this, m_pGoalEnt->pev->origin );
}

void CTriggerCamera::OverrideReset( void )
{
	//find path_corner on a next level
	m_pGoalEnt = UTIL_FindEntityByTargetname (NULL, STRING(pev->message));
	if(m_pGoalEnt) UTIL_SetOrigin( this, m_pGoalEnt->pev->origin );
}

void CTriggerCamera::PostActivate( void )
{
	if (FStrEq(STRING(pev->target), "player") || (pev->spawnflags & SF_CAMERA_PLAYER_TARGET))
		pTarget = UTIL_PlayerByIndex( 1 );
	else pTarget = UTIL_FindEntityByTargetname( NULL, STRING(pev->target) );
}

void CTriggerCamera::Think( void )
{
	SetNextThink( 0 );

	Move();
	pev->dmgtime = gpGlobals->time;
	if ( pTarget ) UTIL_WatchTarget( this, pTarget );
	if(m_flWait && pev->teleport_time < gpGlobals->time) TurnOff();
}

void CTriggerCamera :: UpdatePlayerView( void )
{
	int flags;

	if(pev->spawnflags & SF_CAMERA_PLAYER_TAKECONTROL) //freeze player
		((CBasePlayer *)((CBaseEntity *)m_hActivator))->EnableControl(GetState() ? FALSE : TRUE );
	if(GetState() == STATE_OFF) flags = 0;
	else flags |= CAMERA_ON; 
	CBaseEntity *pCamera = UTIL_FindEntityByTargetname( NULL, STRING(pev->netname) );
	if(pCamera && !pCamera->IsBSPModel()) UTIL_SetView( (CBaseEntity *)m_hActivator, pCamera, flags );
	else UTIL_SetView( (CBaseEntity *)m_hActivator, this, flags );
}

void CTriggerCamera :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if(pActivator && pActivator->IsPlayer())//only at player
	{
		m_hActivator = pActivator;//save activator
	}	
	else m_hActivator = UTIL_PlayerByIndex( 1 );
		
	if (useType == USE_TOGGLE)
	{
		if(m_iState == STATE_OFF) useType = USE_ON;
		else useType = USE_OFF;
	}
	if (useType == USE_ON ) TurnOn();
	else if (useType == USE_OFF) TurnOff();
	else if (useType == USE_SHOWINFO)
	{
		Msg("======/Xash Debug System/======\n");
		Msg("classname: %s\n", STRING(pev->classname));
		Msg("State: %s, Look at %s\n", GetStringForState( GetState()), pev->netname ? STRING(pev->netname) : STRING(pev->targetname));
		Msg("Speed: %g Camera target: %s\n", pev->speed, pTarget ? STRING(pTarget->pev->targetname) : "None" );
	}
}

void CTriggerCamera::Move( void )
{
	// Not moving on a path, return
	if (!m_pGoalEnt) return;

	// Subtract movement from the previous frame
	pev->frags -= pev->speed * gpGlobals->frametime;

	// Have we moved enough to reach the target?
	if ( pev->frags <= 0 )
	{
		// Fire the passtarget if there is one
		UTIL_FireTargets(m_pGoalEnt->pev->message, this, this, USE_TOGGLE );
		if ( FBitSet( m_pGoalEnt->pev->spawnflags, SF_CORNER_FIREONCE ) ) m_pGoalEnt->pev->message = iStringNull;
		if ( FBitSet( m_pGoalEnt->pev->spawnflags, SF_CORNER_TELEPORT ) )
		{
			m_pGoalEnt = m_pGoalEnt->GetNext();
			if ( m_pGoalEnt ) UTIL_AssignOrigin( this, m_pGoalEnt->pev->origin); 
		}
		if ( FBitSet( m_pGoalEnt->pev->spawnflags, SF_CORNER_WAITTRIG ) )
		{
			//strange feature...
		}
		
		// Time to go to the next target
		m_pGoalEnt = m_pGoalEnt->GetNext();

		// Set up next corner
		if ( !m_pGoalEnt ) UTIL_SetVelocity( this, g_vecZero );
		else
		{
			pev->message = m_pGoalEnt->pev->targetname; //save last corner
			((CPathCorner *)m_pGoalEnt)->GetSpeed( &pev->armorvalue );

			Vector delta = m_pGoalEnt->pev->origin - pev->origin;
			pev->frags = delta.Length();
			pev->movedir = delta.Normalize();
			m_flDelay = gpGlobals->time + m_pGoalEnt->GetDelay();
		}
	}

	if ( m_flDelay > gpGlobals->time )
		pev->speed = UTIL_Approach( 0, pev->speed, 500 * gpGlobals->frametime );
	else	pev->speed = UTIL_Approach( pev->armorvalue, pev->speed, 500 * gpGlobals->frametime );
	
	if(!pTarget)UTIL_WatchTarget( this, m_pGoalEnt );//watch for track

	float fraction = 2 * gpGlobals->frametime;
	UTIL_SetVelocity( this, ((pev->movedir * pev->speed) * fraction) + (pev->velocity * (1-fraction)));
}

void CTriggerCamera::TurnOff( void )
{
	m_iState = STATE_OFF;
	if(m_pGoalEnt) m_pGoalEnt = m_pGoalEnt->GetPrev();
	UTIL_SetVelocity( this, g_vecZero );
	UTIL_SetAvelocity( this, g_vecZero );
	UpdatePlayerView();
	FireTargets();
	DontThink();
}

void CTriggerCamera::TurnOn( void )
{
	pev->dmgtime = gpGlobals->time;
	m_iState = STATE_ON;
	
	pev->armorvalue = pev->speed;
	m_flDelay = gpGlobals->time;
	pev->frags = 0;

	// copy over player information
	if (pev->spawnflags & SF_CAMERA_PLAYER_POSITION )
	{
		UTIL_SetOrigin( this, m_hActivator->pev->origin + m_hActivator->pev->view_ofs );
		pev->angles.x = -m_hActivator->pev->angles.x;
		pev->angles.y = m_hActivator->pev->angles.y;
		pev->angles.z = 0;
		pev->velocity = m_hActivator->pev->velocity;
		ClearBits( pev->spawnflags, SF_CAMERA_PLAYER_POSITION );
	}
	
	//time-based camera
	if(m_flWait)pev->teleport_time = gpGlobals->time + m_flWait;

	Move();
	UpdatePlayerView();
	SetNextThink( 0 );
}