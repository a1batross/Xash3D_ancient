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
	else if (FStrEq( pkvd->szKeyName, "roomtype" )) //for soundfx
	{
		pev->button = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "wait" )) // for trigger_push
	{
		m_flDelay = atof( pkvd->szValue );
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
		if(!IsLockedByMaster(pOther))
			UTIL_FireTargets(pev->target, pOther, this, USE_TOGGLE );
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
			UTIL_FireTargets( pev->target, pOther, this, USE_TOGGLE );
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

#define SF_PUSH_ONCE	1

//=======================================================================
//		trigger_push - triger that pushes player
//=======================================================================
class CTriggerPush : public CBaseTrigger
{
	void Spawn( void )
	{
                    if( pev->angles == g_vecZero )
                    	pev->angles.y = 360;
                    if( pev->speed == 0 ) pev->speed = 100;
                    UTIL_LinearVector( this );
                    
		if ( FBitSet( pev->spawnflags, 2 ))
			pev->solid = SOLID_NOT;
		else pev->solid = SOLID_TRIGGER;
		
		pev->movetype = MOVETYPE_NONE;
		UTIL_SetModel(ENT(pev), pev->model );
		SetBits( pev->effects, EF_NODRAW );
		UTIL_SetOrigin( this, pev->origin );
	}
	void PostActivate( void )
	{
		Vector		dir;
		CBaseEntity	*pOwner;
	
		if( FStringNull( pev->target ))
			return;	// dir set with angles

		pOwner = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ));
		if( !pOwner ) return; // dir set with angles

		if( pOwner->pev->flags & FL_POINTENTITY )
		{
			// xash allows to precache from random place
			UTIL_PrecacheSound( "world/jumppad.wav" );

			pev->owner = pOwner->edict();
			pev->button = TRUE;	// Q3A trigger_push
		}
	}
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		if( pev->solid == SOLID_NOT )
		{
			pev->solid = SOLID_TRIGGER;
			gpGlobals->force_retouch++;
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

		if( pev->button )
		{
			if( m_flWait >= gpGlobals->time ) return;

			if( !pOther->IsPlayer() || pOther->pev->movetype != MOVETYPE_WALK )
				return;

			float	time, dist, f;
			Vector	origin, velocity;

			origin = (pev->absmin + pev->absmax) * 0.5f;
			CBaseEntity *pTarg = CBaseEntity::Instance( pev->owner );

			// assume pev->owner is valid
			time = sqrt (( pTarg->pev->origin.z - origin.z ) / (0.5f * CVAR_GET_FLOAT( "sv_gravity" )));
			if( !time )
			{
				UTIL_Remove( this );
				return;
			}

			velocity = pTarg->pev->origin - origin;
			velocity.z = 0.0f;
			dist = velocity.Length();
			velocity = velocity.Normalize();

			f = dist / time;
			velocity *= f;
			velocity.z = time * CVAR_GET_FLOAT( "sv_gravity" );

			pOther->pev->flJumpPadTime = gpGlobals->time;
			pOther->pev->basevelocity = velocity;
			pOther->pev->velocity = g_vecZero;
			pOther->pev->flags &= ~FL_BASEVELOCITY;

			EMIT_SOUND( ENT( pev ), CHAN_VOICE, "world/jumppad.wav", VOL_NORM, ATTN_IDLE );

			m_flWait = gpGlobals->time + ( 2.0f * gpGlobals->frametime );

			if( FBitSet( pev->spawnflags, SF_PUSH_ONCE ))
				UTIL_Remove( this );
		}
		else if( pOther->pev->solid != SOLID_NOT && pOther->pev->solid != SOLID_BSP )
		{
			// instant trigger, just transfer velocity and remove
			if( FBitSet( pev->spawnflags, SF_PUSH_ONCE ))
			{
				pOther->pev->velocity = pOther->pev->velocity + (pev->speed * pev->movedir);
				if( pOther->pev->velocity.z > 0 ) pOther->pev->flags &= ~FL_ONGROUND;
				UTIL_Remove( this );
			}
			else
			{
				Vector vecPush = (pev->speed * pev->movedir);
				if ( pOther->pev->flags & FL_BASEVELOCITY )
					vecPush = vecPush + pOther->pev->basevelocity;
				pOther->pev->basevelocity = vecPush;
				pOther->pev->flags |= FL_BASEVELOCITY;
				pOther->pev->flJumpPadTime = gpGlobals->time;
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
LINK_ENTITY_TO_CLASS( func_soundfx, CTriggerSound );	// Xash 0.4 compatibility

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
		gpGlobals->force_retouch++;
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
	else if (useType == USE_RESET)
	{
		pev->dmg = 0;//reset dmg level
	}
	else if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		ALERT( at_console, "State: %s, Dmg value %g\n", GetStringForState( GetState()), pev->dmg );
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
			if ( gpGlobals->time != pev->dmg_take )
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
	else if( pev->dmgtime > gpGlobals->time && gpGlobals->time != pev->dmg_take ) return;

	fldmg = pev->dmg * 0.5;	// 0.5 seconds worth of damage, pev->dmg is damage/second

	if ( fldmg < 0 ) pOther->TakeHealth( -fldmg, pev->button );
	else pOther->TakeDamage( pev, pev, fldmg, pev->button );

	pev->dmg_take = gpGlobals->time;
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
class CTriggerTransition : public CPointEntity // Don't change this!
{
public:
	void Spawn( void )
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		UTIL_SetModel( ENT( pev ), pev->model );
		pev->model = pev->modelindex = 0;
	}
};
LINK_ENTITY_TO_CLASS( trigger_transition, CTriggerTransition );

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


#define SF_CAMERA_PLAYER_POSITION		1 // start from player eyes
#define SF_CAMERA_PLAYER_TARGET		2 // player it's camera target
#define SF_CAMERA_PLAYER_TAKECONTROL		4 // freeze player

void CTriggerCamera :: KeyValue( KeyValueData* pkvd )
{
	if (FStrEq( pkvd->szKeyName, "viewentity" ))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "moveto" ))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseLogic::KeyValue( pkvd );
}

void CTriggerCamera :: Spawn (void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;

	m_iState = STATE_OFF;
	
	UTIL_SetModel( ENT( pev ), "models/common/null.mdl" );
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	SetBits( pev->flags, FL_POINTENTITY );
}

void CTriggerCamera::PostSpawn( void )
{
	m_pGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ));
	if ( m_pGoalEnt ) UTIL_SetOrigin( this, m_pGoalEnt->pev->origin );
}

void CTriggerCamera::OverrideReset( void )
{
	// find path_corner on a next level
	m_pGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ));
	if( m_pGoalEnt ) UTIL_SetOrigin( this, m_pGoalEnt->pev->origin );
}

void CTriggerCamera::PostActivate( void )
{
	if (FStrEq( STRING( pev->target ), "player" ) || ( pev->spawnflags & SF_CAMERA_PLAYER_TARGET ))
		pTarget = UTIL_PlayerByIndex( 1 );
	else pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ));
}

void CTriggerCamera::Think( void )
{
	SetNextThink( gpGlobals->frametime );

	Move();
	pev->dmgtime = gpGlobals->time;
	if ( pTarget ) UTIL_WatchTarget( this, pTarget );

	if( m_flWait && pev->teleport_time < gpGlobals->time )
	{
		TurnOff();
	}
}

void CTriggerCamera :: UpdatePlayerView( void )
{
	int flags = 0;

	if( m_hActivator == NULL || !m_hActivator->edict() || !( m_hActivator->pev->flags & FL_CLIENT ))
	{
		ALERT( at_error, "Camera: No Client!\n" );
		return;
	}

	if( pev->spawnflags & SF_CAMERA_PLAYER_TAKECONTROL )
	{
		int state;

		if( GetState() == STATE_ON )
			state = TRUE;
		else state = FALSE;
		
		// freeze player
		((CBasePlayer *)((CBaseEntity *)m_hActivator))->EnableControl( state );
	}
#if 1
	if( GetState() == STATE_OFF )
		flags |= CAMERA_ON;
	else flags = 0; 

	CBaseEntity *pCamera = UTIL_FindEntityByTargetname( NULL, STRING( pev->netname ));
	if( pCamera && !pCamera->IsBSPModel( ))
		UTIL_SetView( m_hActivator, pCamera, flags );
	else UTIL_SetView( m_hActivator, this, flags );
#else
	// enchanced engine SET_VIEW (see code in client\global\view.cpp)
	if( GetState() == STATE_OFF )
		SET_VIEW( m_hActivator->edict(), edict() );
	else SET_VIEW( m_hActivator->edict(), m_hActivator->edict() );
#endif
}

void CTriggerCamera :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( pActivator && pActivator->IsPlayer( ))
	{
		// only at player
		m_hActivator = pActivator;
	}	
	else if( !IsMultiplayer( ))
	{
		m_hActivator = UTIL_PlayerByIndex( 1 );
	}		
	else
	{
		ALERT( at_warning, "%s: %s activator not player. Ignored.\n", STRING( pev->classname ), STRING( pev->targetname ));		
		return;
	}

	if ( useType == USE_TOGGLE )
	{
		if ( m_iState == STATE_OFF )
			useType = USE_ON;
		else useType = USE_OFF;
	}
	if ( useType == USE_ON )
	{
		TurnOn();
	}
	else if ( useType == USE_OFF )
	{
		TurnOff();
	}
	else if ( useType == USE_SHOWINFO )
	{
		ALERT( at_console, "======/Xash Debug System/======\n");
		ALERT( at_console, "classname: %s\n", STRING( pev->classname ));
		ALERT( at_console, "State: %s, Look at %s\n", GetStringForState( GetState()), pev->netname ? STRING( pev->netname ) : STRING( pev->targetname ));
		ALERT( at_console, "Speed: %g Camera target: %s\n", pev->speed, pTarget ? STRING(pTarget->pev->targetname) : "None" );
	}
}

void CTriggerCamera::Move( void )
{
	// Not moving on a path, return
	if ( !m_pGoalEnt ) return;

	// Subtract movement from the previous frame
	pev->frags -= pev->speed * gpGlobals->frametime;

	// Have we moved enough to reach the target?
	if ( pev->frags <= 0 )
	{
		// Fire the passtarget if there is one
		UTIL_FireTargets(m_pGoalEnt->pev->target, this, this, USE_TOGGLE );
		if ( FBitSet( m_pGoalEnt->pev->spawnflags, SF_FIREONCE ) ) m_pGoalEnt->pev->target = iStringNull;
		if ( FBitSet( m_pGoalEnt->pev->spawnflags, SF_TELEPORT_TONEXT ) )
		{
			m_pGoalEnt = m_pGoalEnt->GetNext();
			if ( m_pGoalEnt ) UTIL_AssignOrigin( this, m_pGoalEnt->pev->origin); 
		}
		
		// Time to go to the next target
		m_pGoalEnt = m_pGoalEnt->GetNext();

		// Set up next corner
		if ( !m_pGoalEnt ) UTIL_SetVelocity( this, g_vecZero );
		else
		{
			pev->target = m_pGoalEnt->pev->targetname; //save last corner
			((CInfoPath *)m_pGoalEnt)->GetSpeed( &pev->armorvalue );

			Vector delta = m_pGoalEnt->pev->origin - pev->origin;
			pev->frags = delta.Length();
			pev->movedir = delta.Normalize();
			m_flDelay = gpGlobals->time + ((CInfoPath *)m_pGoalEnt)->GetDelay();
		}
	}

	if ( m_flDelay > gpGlobals->time )
		pev->speed = UTIL_Approach( 0, pev->speed, 500 * gpGlobals->frametime );
	else pev->speed = UTIL_Approach( pev->armorvalue, pev->speed, 500 * gpGlobals->frametime );
	
	if( !pTarget ) UTIL_WatchTarget( this, m_pGoalEnt ); // watch for track

	float fraction = 2 * gpGlobals->frametime;
	UTIL_SetVelocity( this, ((pev->movedir * pev->speed) * fraction) + (pev->velocity * ( 1.0f - fraction )));
}

void CTriggerCamera::TurnOff( void )
{
	if( m_pGoalEnt ) m_pGoalEnt = m_pGoalEnt->GetPrev();
	UTIL_SetVelocity( this, g_vecZero );
	UTIL_SetAvelocity( this, g_vecZero );
	UpdatePlayerView();
	m_iState = STATE_OFF;
	DontThink();
}

void CTriggerCamera::TurnOn( void )
{
	pev->dmgtime = gpGlobals->time;
	pev->armorvalue = pev->speed;
	pev->frags = 0;

	// copy over player information
	if ( pev->spawnflags & SF_CAMERA_PLAYER_POSITION )
	{
		UTIL_SetOrigin( this, m_hActivator->pev->origin + m_hActivator->pev->view_ofs );
		pev->angles.x = -m_hActivator->pev->angles.x;
		pev->angles.y = m_hActivator->pev->angles.y;
		pev->angles.z = 0;
		pev->velocity = m_hActivator->pev->velocity;
		ClearBits( pev->spawnflags, SF_CAMERA_PLAYER_POSITION );
	}
	
	// time-based camera
	if( m_flWait ) pev->teleport_time = gpGlobals->time + m_flWait;

	Move();
	UpdatePlayerView();
	m_iState = STATE_ON;
	SetNextThink( gpGlobals->frametime );
}