//=======================================================================
//			Copyright (C) Shambler Team 2004
//		         basefunc.cpp - brush based entities: tanks, 
//			       cameras, vehicles etc			    
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "sfx.h" 
#include "decals.h"
#include "client.h"
#include "saverestore.h"
#include "gamerules.h"
#include "basebrush.h"
#include "defaults.h"
#include "player.h"

//=======================================================================
// 			STATIC BRUSHES
//=======================================================================

//=======================================================================
// func_wall - standard not moving wall. affect to physics 
//=======================================================================
class CFuncWall : public CBaseBrush
{
public:
	void Spawn( void );
	void TurnOn( void );
	void TurnOff( void );
	void Precache( void ){ CBaseBrush::Precache(); }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};
LINK_ENTITY_TO_CLASS( func_wall, CFuncWall );
LINK_ENTITY_TO_CLASS( func_wall_toggle, CFuncWall );
LINK_ENTITY_TO_CLASS( func_illusionary, CFuncWall );

void CFuncWall :: Spawn( void )
{	
	CBaseBrush::Spawn();

	if(FClassnameIs(pev, "func_illusionary" ))
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
	}
	else
	{
		pev->solid = SOLID_BSP;
          	pev->movetype = MOVETYPE_PUSH;
          }
	UTIL_SetModel( ENT(pev), pev->model );

	//Smart field system ®
	if (!FStringNull(pev->angles) && FStringNull(pev->origin))
	{
		pev->angles = g_vecZero;
		DevMsg( "\n======/Xash SmartFiled System/======\n\n");
		DevMsg( "Create brush origin for %s,\nif we want correctly set angles\n", STRING(pev->classname));
		SHIFT;
	}
	pev->angles[1] =  0 - pev->angles[1];	
	if(pev->spawnflags & SF_START_ON) TurnOff();
	else TurnOn();
}

void CFuncWall :: TurnOff( void )
{
	if(FClassnameIs(pev, "func_wall_toggle" ))
	{
		pev->solid = SOLID_NOT;
		pev->effects |= EF_NODRAW;
		UTIL_SetOrigin( this, pev->origin );
	}
	else pev->frame = 0;

	m_iState = STATE_OFF;
}

void CFuncWall :: TurnOn( void )
{
	if(FClassnameIs(pev, "func_wall_toggle" ))
	{
		pev->solid = SOLID_BSP;
		pev->effects &= ~EF_NODRAW;
		UTIL_SetOrigin( this, pev->origin );
	}
	else pev->frame = 1;

	m_iState = STATE_ON;
}

void CFuncWall :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (useType == USE_TOGGLE)
	{
		if(m_iState == STATE_OFF) useType = USE_ON;
		else useType = USE_OFF;
	}
	if (useType == USE_ON) TurnOn();
	else if (useType == USE_OFF)TurnOff();
	else if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		Msg( "State: %s, Visible: %s\n", GetStringForState( GetState()), pev->effects & EF_NODRAW ? "No" : "Yes");
		Msg( "Contents: %s, Texture frame: %.f\n", GetContentsString( pev->skin ), pev->frame);
	}
}
//=======================================================================
// func_breakable - generic breakable brush. 
//=======================================================================
class CFuncBreakable : public CBaseBrush
{
public:
	void Spawn( void );
	void Precache( void ){ CBaseBrush::Precache(); }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ){ if (IsBreakable()) Die(); }
};
LINK_ENTITY_TO_CLASS( func_breakable, CFuncBreakable );

void CFuncBreakable :: Spawn( void )
{
	CBaseBrush::Spawn();	
	UTIL_SetModel( ENT(pev), pev->model );

	if ( FBitSet( pev->spawnflags, SF_BREAK_TRIGGER_ONLY )) pev->takedamage = DAMAGE_NO;
	else pev->takedamage = DAMAGE_YES;
}
//=======================================================================
// func_monsterclip - invisible contents that monster can't walk across 
//=======================================================================
class CFuncClip : public CBaseEntity
{
public:
	void Spawn( void )
	{
		pev->effects = EF_NODRAW;
		pev->contents = CONTENTS_MONSTERCLIP;
		UTIL_SetModel( ENT(pev), pev->model );
	}
};
LINK_ENTITY_TO_CLASS( func_monsterclip, CFuncClip );

//=======================================================================
// func_lamp - switchable brush light. 
//=======================================================================
class CFuncLamp : public CBaseBrush
{
public:
	void Spawn( void );
	void Precache( void ){ CBaseBrush::Precache(); }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	void EXPORT Flicker( void );
	void Die( void );
};
LINK_ENTITY_TO_CLASS( func_lamp, CFuncLamp );

void CFuncLamp :: Spawn( void )
{	
	m_Material = Glass;
	CBaseBrush::Spawn();
	UTIL_SetModel( ENT(pev), pev->model );
	
	if(pev->spawnflags & SF_START_ON) Use( this, this, USE_ON, 0 );
	else Use( this, this, USE_OFF, 0 );
}

void CFuncLamp :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	
	if(IsLockedByMaster( useType )) return;
	if(m_iState == STATE_DEAD && useType != USE_SHOWINFO)return;//lamp is broken

	if (useType == USE_TOGGLE)
	{
		if(m_iState == STATE_OFF) useType = USE_ON;
		else useType = USE_OFF;
	}
	if (useType == USE_ON)
	{
		if(m_flDelay)//make flickering delay
		{
			pev->frame = 0;//light texture is on
			m_iState = STATE_TURN_ON;
			LIGHT_STYLE(m_iStyle, "mmamammmmammamamaaamammma");
			SetThink( Flicker );
			SetNextThink(m_flDelay);
		}
		else
		{         //instantly enable
			m_iState = STATE_ON;
			pev->frame = 0;//light texture is on
			LIGHT_STYLE(m_iStyle, "k");
			UTIL_FireTargets( pev->target, this, this, USE_ON );//lamp enable
		}
	}
	else if (useType == USE_OFF)
	{
		pev->frame = 1;//light texture is off
		LIGHT_STYLE(m_iStyle, "a");
		UTIL_FireTargets( pev->target, this, this, USE_OFF );//lamp disable
		m_iState = STATE_OFF;
	}
	else if (useType == USE_SET) Die();
	else if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		Msg( "State: %s, Light style %d\n", GetStringForState( GetState()), m_iStyle);
		Msg( "Texture frame: %.f. Health %.f\n", pev->frame, pev->health);
	}
}

void CFuncLamp::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if(m_iState == STATE_DEAD) return;
	const char *pTextureName;
	Vector start = pevAttacker->origin + pevAttacker->view_ofs;
	Vector end = start + vecDir * 1024;
	edict_t *pWorld = ptr->pHit;
	if ( pWorld )pTextureName = TRACE_TEXTURE( pWorld, start, end );
	if(strstr(pTextureName, "+0~") || strstr(pTextureName, "~") )//take damage only at light texture
	{
		UTIL_Sparks( ptr->vecEndPos );
		pev->oldorigin = ptr->vecEndPos;//save last point of damage
		pev->takedamage = DAMAGE_YES;//inflict damage only at light texture
	}
	else	pev->takedamage = DAMAGE_NO;
	CBaseLogic::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}
int CFuncLamp::TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if ( bitsDamageType & DMG_BLAST ) flDamage *= 3;
	else if ( bitsDamageType & DMG_CLUB) flDamage *= 2;
	else if ( bitsDamageType & DMG_SONIC ) flDamage *= 1.2;
	else if ( bitsDamageType & DMG_SHOCK ) flDamage *= 10;//!!!! over voltage
	else if ( bitsDamageType & DMG_BULLET) flDamage *= 0.5;//half damage at bullet
	pev->health -= flDamage;//calculate health

	if (pev->health <= 0)
	{
		Die();
		return 0;
	}
	CBaseBrush::DamageSound();

	return 1;
}

void CFuncLamp::Die( void )
{
	//lamp is random choose die style
	if(m_iState == STATE_OFF)
	{
		pev->frame = 1;//light texture is off
		LIGHT_STYLE(m_iStyle, "a");
		DontThink();
	}
	else
	{         //simple randomization
		pev->impulse = RANDOM_LONG(1, 2);
		SetThink( Flicker );
		SetNextThink( 0.1 + (RANDOM_LONG(1, 2) * 0.1));
	}

	m_iState = STATE_DEAD;//lamp is die
	pev->health = 0;//set health to NULL
	pev->takedamage = DAMAGE_NO;
	UTIL_FireTargets( pev->target, this, this, USE_OFF );//lamp broken

	switch ( RANDOM_LONG(0,1))
	{
		case 0: EMIT_SOUND(ENT(pev), CHAN_VOICE, "materials/glass/bustglass1.wav", 0.7, ATTN_IDLE); break;
		case 1: EMIT_SOUND(ENT(pev), CHAN_VOICE, "materials/glass/bustglass2.wav", 0.8, ATTN_IDLE); break;
	}
	Vector vecSpot = pev->origin + (pev->mins + pev->maxs) * 0.5;
	//spawn glass gibs
	SFX_MakeGibs( m_idShard, vecSpot, pev->size, g_vecZero, 50, BREAK_GLASS);
}

void CFuncLamp :: Flicker( void )
{
	if(m_iState == STATE_TURN_ON)//flickering on enable
	{
		LIGHT_STYLE(m_iStyle, "k");
		UTIL_FireTargets( pev->target, this, this, USE_ON );//Lamp enabled
		m_iState = STATE_ON;
		DontThink();
		return;
	}
	if(pev->impulse == 1)//fadeout on break
	{
		pev->frame = 1;//light texture is off
		LIGHT_STYLE(m_iStyle, "a");
		SetThink( NULL );
		return;
	}
	if(pev->impulse == 2)//broken flickering
	{
		//make different flickering
		switch ( RANDOM_LONG(0,3))
		{
			case 0: LIGHT_STYLE(m_iStyle, "abcaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"); break;
			case 1: LIGHT_STYLE(m_iStyle, "acaaabaaaaaaaaaaaaaaaaaaaaaaaaaaa"); break;
			case 2: LIGHT_STYLE(m_iStyle, "aaafbaaaaaaaaaaaaaaaaaaaaaaaaaaaa"); break;
			case 3: LIGHT_STYLE(m_iStyle, "aaaaaaaaaaaaagaaaaaaaaacaaaacaaaa"); break;
		}		
		pev->frags = RANDOM_FLOAT(0.5, 10);
                   	UTIL_Sparks( pev->oldorigin );
		switch ( RANDOM_LONG(0, 2) )
		{
			case 0: EMIT_SOUND(ENT(pev), CHAN_VOICE, "materials/spark1.wav", 0.4, ATTN_IDLE); break;
			case 1: EMIT_SOUND(ENT(pev), CHAN_VOICE, "materials/spark2.wav", 0.3, ATTN_IDLE); break;
			case 2: EMIT_SOUND(ENT(pev), CHAN_VOICE, "materials/spark3.wav", 0.35, ATTN_IDLE); break;
		}
		if(pev->frags > 6.5) pev->impulse = 1;//stop sparking obsolete
	}
	SetNextThink(pev->frags); 
}

//=======================================================================
// func_conveyor - conveyor belt 
//=======================================================================
class CFuncConveyor : public CBaseBrush
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void UpdateSpeed( float speed );
	void TogglePhysic( BOOL enable );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};
LINK_ENTITY_TO_CLASS( func_conveyor, CFuncConveyor );

void CFuncConveyor :: Spawn( void )
{
	CBaseBrush::Spawn();
	UTIL_LinearVector( this ); // movement direction

	if( !m_pParent ) pev->flags |= FL_WORLDBRUSH;	
	UTIL_SetModel( ENT(pev), STRING( pev->model ));
	
	if( pev->spawnflags & SF_NOTSOLID )
		TogglePhysic( FALSE );
	else TogglePhysic( TRUE );

	// smart field system ®
	if( pev->speed == 0 ) pev->speed = 100;
	pev->frags = pev->speed; // save initial speed
	if( pev->spawnflags & SF_START_ON ) Use( this, this, USE_ON, 0 );
}

void CFuncConveyor :: TogglePhysic( BOOL enable )
{
	if( enable )
	{
		pev->solid = SOLID_BSP; 
		pev->movetype = MOVETYPE_CONVEYOR;
	}
	else
	{
		pev->solid = SOLID_NOT;
          	pev->movetype = MOVETYPE_NONE;
	}
}

void CFuncConveyor :: UpdateSpeed( float speed )
{
	// save speed into framerate, and direction into body
	pev->framerate = pev->speed;
	pev->body = (pev->speed < 0 );
	UTIL_FireTargets( pev->target, this, this, USE_TOGGLE, speed ); // conveyer change speed
}

void CFuncConveyor :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if(IsLockedByMaster( useType )) return;

	if( useType == USE_TOGGLE )
	{
		if( m_iState == STATE_ON )
			useType = USE_OFF;
		else useType = USE_ON;
	}
	if( useType == USE_ON ) 
	{
		UpdateSpeed( pev->speed );
		if(!( pev->spawnflags & SF_NOTSOLID )) //don't push
			pev->movetype = MOVETYPE_CONVEYOR;
		m_iState = STATE_ON;
	}
	else if( useType == USE_OFF )
	{
		UpdateSpeed( 0.1 );
		pev->movetype = MOVETYPE_NONE;
		m_iState = STATE_OFF;
	}
	else if( useType == USE_SET )	// set new speed
	{	
		if( value ) pev->speed = value;	// set new speed ( can be negative )
		else pev->speed = -pev->speed;	// just reverse
		if( m_iState == STATE_ON ) UpdateSpeed( pev->speed );	
	}
	else if( useType == USE_RESET ) // restore initial speed
	{	
		pev->speed = pev->frags;
		if( m_iState == STATE_ON ) UpdateSpeed( pev->speed );	
	}
	else if( useType == USE_SHOWINFO )
	{
		ALERT( at_console, "======/Xash Debug System/======\n" );
		ALERT( at_console, "classname: %s\n", STRING( pev->classname ));
		ALERT( at_console, "State: %s, Conveyor speed %f\n", GetStringForState( GetState()), pev->speed );
		SHIFT;
	}	
}

//=======================================================================
// func_mirror - breakable mirror brush 
//=======================================================================
class CFuncMirror : public CBaseBrush
{
public:
	void Spawn( void );
	void StartMessage( CBasePlayer *pPlayer );
	void Precache( void ){ CBaseBrush::Precache(); }
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual STATE GetState( void ) { return pev->body ? STATE_ON:STATE_OFF; };
};
LINK_ENTITY_TO_CLASS( func_mirror, CFuncMirror );

void CFuncMirror::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "alpha"))
	{
		pev->sequence = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	CBaseBrush::KeyValue( pkvd );
}

void CFuncMirror :: Spawn( void )
{
	m_Material = Glass;
	CBaseBrush::Spawn();
	if (!m_pParent) pev->flags = FL_WORLDBRUSH;

	if(IsMultiplayer())
	{
		ALERT(at_console, "\n======/Xash SmartFiled System/======\n\n");
		ALERT(at_console, "Sorry, %s don't working in multiplayer\nPlease, use static world mirrors\n", STRING(pev->classname));
		SHIFT;
	}
	UTIL_SetModel( ENT(pev), pev->model ); 
	if(pev->spawnflags & SF_NOTSOLID)//make illusionary wall 
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
	}
	else
	{
		pev->solid = SOLID_BSP;
          	pev->movetype = MOVETYPE_PUSH;
	}
	pev->body = 1;
}

void CFuncMirror :: StartMessage( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsg.AddMirror, NULL, pPlayer->pev );
		WRITE_SHORT( entindex() );
	MESSAGE_END();
}

void CFuncMirror :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( useType == USE_TOGGLE )
	{
		if( pev->body )
			useType = USE_OFF;
		else useType = USE_ON;
	}
	if( useType == USE_ON )  pev->body = TRUE;
	if( useType == USE_OFF ) pev->body = FALSE;
	if( useType == USE_SET ) Die();
	if( useType == USE_SHOWINFO )
	{
		DEBUGHEAD;
		ALERT(at_console, "State: %s, alpha %d\n", GetStringForState( GetState()), pev->sequence);
		ALERT(at_console, "Renderamt: %.f, Health %.f\n", pev->renderamt, pev->health);
	}
}

//=======================================================================
// func_monitor - A monitor that renders the view from a camera entity. 
//=======================================================================
class CFuncMonitor : public CBaseBrush
{
public:
	void Spawn( void );
	void Precache( void ){ CBaseBrush::Precache(); }
	void StartMessage( CBasePlayer *pPlayer );
	void ChangeCamera( string_t newcamera );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual BOOL IsFuncScreen( void ) { return TRUE; }
	virtual STATE GetState( void ) { return pev->body ? STATE_ON:STATE_OFF; };
	virtual int ObjectCaps( void );
	BOOL OnControls( entvars_t *pevTest );
	CBaseEntity *pCamera;
};
LINK_ENTITY_TO_CLASS( func_monitor, CFuncMonitor );

void CFuncMonitor::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "camera"))
	{
		pev->target = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "type"))
	{
		pev->impulse = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "mode"))
	{
		pev->skin = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	CBaseBrush::KeyValue( pkvd );
}

void CFuncMonitor :: Spawn()
{
	m_Material = Computer;
	CBaseBrush::Spawn();

	if(pev->spawnflags & SF_NOTSOLID)//make illusionary wall 
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
	}
	else
	{
		pev->solid = SOLID_BSP;
          	pev->movetype = MOVETYPE_PUSH;
	}
	UTIL_SetModel(ENT(pev), pev->model );

	//enable monitor
	if(pev->spawnflags & SF_START_ON)pev->body = 1;
}

int CFuncMonitor :: ObjectCaps( void ) 
{
	if (pev->impulse)
		return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE;
	else	return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION);
}

void CFuncMonitor::StartMessage( CBasePlayer *pPlayer )
{
	//send monitor index
	MESSAGE_BEGIN( MSG_ONE, gmsg.AddScreen, NULL, pPlayer->pev );
		WRITE_BYTE( entindex() );
	MESSAGE_END();
	ChangeCamera( pev->target );
}

void CFuncMonitor::ChangeCamera( string_t newcamera )
{
	pCamera = UTIL_FindEntityByTargetname( NULL, STRING( newcamera ));
         
	if( pCamera ) pev->sequence = pCamera->entindex();
	else if( pev->body ) pev->frame = 1; // noise if not found camera

	// member newcamera name
	pev->target = newcamera;
}

void CFuncMonitor :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	
	if(IsLockedByMaster( useType )) return;

	if (useType == USE_TOGGLE)
	{
		if(pev->body) useType = USE_OFF;
		else useType = USE_ON;
	}
	if (useType == USE_ON)pev->body = 1;
	else if (useType == USE_OFF)
	{
		pev->body = 0;
		pev->frame = 0;
	}
	else if (useType == USE_SET)
	{
		if(pActivator->IsPlayer())
		{
			if( value ) UTIL_SetView(pActivator, iStringNull, 0 );
			else if(pev->body)
			{
				UTIL_SetView( pev->target, CAMERA_ON );
				m_pController = (CBasePlayer*)pActivator;
				m_pController->m_pMonitor = this;

				if (m_pParent)
					m_vecPlayerPos = m_pController->pev->origin - m_pParent->pev->origin;
				else	m_vecPlayerPos = m_pController->pev->origin;
			}
		}
	}
	else if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		Msg( "State: %s, Mode: %s\n", GetStringForState( GetState()), pev->skin ? "Color" : "B&W");
		Msg( "Type: %s. Camera Name: %s\n", pev->impulse ? "Duke Nukem Style" : "Half-Life 2 Style", STRING( pev->target) );
	}
}

BOOL CFuncMonitor :: OnControls( entvars_t *pevTest )
{
	if(m_pParent && ((m_vecPlayerPos + m_pParent->pev->origin) - pevTest->origin).Length() <= 30)
		return TRUE;
	else if((m_vecPlayerPos - pevTest->origin).Length() <= 30 )
		return TRUE;
	return FALSE;
}

//=======================================================================
//		func_teleport - classic XASH portal
//=======================================================================
class CFuncTeleport : public CFuncMonitor
{
public:
	void Spawn( void );
	void Touch ( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	void StartMessage( CBasePlayer *pPlayer );
};
LINK_ENTITY_TO_CLASS( func_portal, CFuncTeleport );
LINK_ENTITY_TO_CLASS( trigger_teleport, CFuncTeleport );

void CFuncTeleport :: Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	UTIL_SetModel(ENT(pev), pev->model);
	if (FClassnameIs(pev, "func_portal")) //portal mode
	{
		pev->impulse = 0;//can't use teleport
		pev->body = 1;//enabled
	}
	else SetBits( pev->effects, EF_NODRAW );//classic mode
}

void CFuncTeleport::StartMessage( CBasePlayer *pPlayer )
{
	//send portal index
	MESSAGE_BEGIN( MSG_ONE, gmsg.AddPortal, NULL, pPlayer->pev );
		WRITE_BYTE( entindex() );
	MESSAGE_END();
	ChangeCamera( pev->target );
}
void CFuncTeleport :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "landmark"))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget"))
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	CBaseLogic::KeyValue( pkvd );
}

void CFuncTeleport :: Touch( CBaseEntity *pOther )
{
	entvars_t* pevToucher = pOther->pev;
	CBaseEntity *pTarget = NULL;

	// only teleport monsters or clients or projectiles
	if( !FBitSet( pevToucher->flags, FL_CLIENT|FL_MONSTER|FL_PROJECTILE ) && !pOther->IsPushable())
		return;
	if( IsLockedByMaster( pOther )) return;

	pTarget = UTIL_FindEntityByTargetname( pTarget, STRING(pev->target) );
	if ( !pTarget ) return;

	CBaseEntity *pLandmark = UTIL_FindEntityByTargetname( NULL, STRING(pev->message) );
	if ( pLandmark )
	{
		Vector vecOriginOffs = pTarget->pev->origin - pLandmark->pev->origin;

		// do we need to rotate the entity?
		if ( pLandmark->pev->angles != pTarget->pev->angles )
		{
			Vector vecVA;
			float ydiff = pTarget->pev->angles.y - pLandmark->pev->angles.y;

			// set new angle to face
			pOther->pev->angles.y += ydiff;
			if( pOther->IsPlayer())
			{
				pOther->pev->angles.x = pOther->pev->viewangles.x;
				pOther->pev->fixangle = TRUE;
			}

			// set new velocity
			vecVA = UTIL_VecToAngles(pOther->pev->velocity);
			vecVA.y += ydiff;
			UTIL_MakeVectors(vecVA);
			pOther->pev->velocity = gpGlobals->v_forward * pOther->pev->velocity.Length();
			pOther->pev->velocity.z = -pOther->pev->velocity.z;

			// set new origin
			Vector vecPlayerOffs = pOther->pev->origin - pLandmark->pev->origin;
			vecVA = UTIL_VecToAngles(vecPlayerOffs);
			UTIL_MakeVectors(vecVA);
			vecVA.y += ydiff;
			UTIL_MakeVectors(vecVA);
			Vector vecPlayerOffsNew = gpGlobals->v_forward * vecPlayerOffs.Length();
			vecPlayerOffsNew.z = -vecPlayerOffsNew.z;
			vecOriginOffs = vecOriginOffs + vecPlayerOffsNew - vecPlayerOffs;
		}
		UTIL_SetOrigin( pOther, pOther->pev->origin + vecOriginOffs );
	}
	else
	{
		Vector tmp = pTarget->pev->origin;

		if( pOther->IsPlayer( ))
			tmp.z -= pOther->pev->mins.z; // make origin adjustments
		tmp.z++;
		UTIL_SetOrigin( pOther, tmp );

		pOther->pev->angles = pTarget->pev->angles;
		pOther->pev->velocity = g_vecZero;
		if( pOther->IsPlayer( ))
		{
			pOther->pev->viewangles = pTarget->pev->angles;
			pOther->pev->fixangle = TRUE;
		}
	}
          
	ChangeCamera( pev->target ); // update PVS
	pevToucher->flags &= ~FL_ONGROUND;
	pevToucher->fixangle = TRUE;

	UTIL_FireTargets( pev->netname, pOther, this, USE_TOGGLE ); // fire target
}

//=======================================================================
// 			ANGULAR MOVING BRUSHES
//=======================================================================
//=======================================================================
// func_rotate - standard not moving wall. affect to physics 
//=======================================================================
class CFuncRotating : public CBaseBrush
{
public:
	void Spawn( void  );
	void Precache( void  );
	void KeyValue( KeyValueData* pkvd);
	void Touch ( CBaseEntity *pOther );
	void Blocked( CBaseEntity *pOther );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Think( void );
	void PostActivate ( void );
	
	void RampPitchVol ( void );
	void SetSpeed( float newspeed );
	void SetAngularImpulse( float impulse );
	float flDegree;
};
LINK_ENTITY_TO_CLASS( func_rotating, CFuncRotating );

void CFuncRotating :: KeyValue( KeyValueData* pkvd)
{
	if( FStrEq( pkvd->szKeyName, "spintime" ))
	{
		pev->frags = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if( FStrEq( pkvd->szKeyName, "spawnorigin" ))
	{
		Vector	tmp;
		UTIL_StringToVector( tmp, pkvd->szValue );
		if( tmp != g_vecZero ) pev->origin = tmp;
		pkvd->fHandled = TRUE;
	}
	else CBaseBrush::KeyValue( pkvd );
}

void CFuncRotating :: Precache( void )
{
	CBaseBrush::Precache();
	int m_sounds = UTIL_LoadSoundPreset(m_iMoveSound);

	switch (m_sounds)
	{
	case 1:
		pev->noise3 = UTIL_PrecacheSound ("materials/fans/fan1.wav");
		break;
	case 2:
		pev->noise3 = UTIL_PrecacheSound ("materials/fans/fan2.wav");
		break;
	case 3:
		pev->noise3 = UTIL_PrecacheSound ("materials/fans/fan3.wav");
		break;
	case 4:
		pev->noise3 = UTIL_PrecacheSound ("materials/fans/fan4.wav");
		break;
	case 5:
		pev->noise3 = UTIL_PrecacheSound ("materials/fans/fan5.wav");
		break;
	case 0:
		pev->noise3 = UTIL_PrecacheSound ("common/null.wav");
		break;
	default:
		pev->noise3 = UTIL_PrecacheSound(m_sounds);
		break;
	}
}

void CFuncRotating :: Spawn( void )
{
	Precache();
	CBaseBrush::Spawn();

	if ( pev->spawnflags & 64 )
	{
		pev->solid = SOLID_NOT;
		pev->contents = CONTENTS_NONE;
		pev->movetype = MOVETYPE_PUSH;
	}
	else
	{
		pev->solid = SOLID_BSP;
		pev->movetype = MOVETYPE_PUSH;
	}
	
	SetBits (pFlags, PF_ANGULAR);//enetity has angular velocity
	m_iState = STATE_OFF;
          m_pitch = PITCH_NORM - 1;
          pev->dmg_save = 0;//cur speed is NULL
          pev->button = pev->speed;//member start speed
	AxisDir();

	UTIL_SetOrigin(this, pev->origin);
	UTIL_SetModel( ENT(pev), pev->model );
}

void CFuncRotating :: Touch ( CBaseEntity *pOther )
{
	// don't hurt toucher - just apply velocity him
	entvars_t	*pevOther = pOther->pev;
	pevOther->velocity = (pevOther->origin - VecBModelOrigin(pev) ).Normalize() * pev->dmg;
}

void CFuncRotating :: Blocked( CBaseEntity *pOther )
{
	if(m_pParent && m_pParent->edict() && pFlags & PF_PARENTMOVE)m_pParent->Blocked( pOther );

	UTIL_AssignAngles(this, pev->angles );
	if ( gpGlobals->time < m_flBlockedTime)return;
	m_flBlockedTime = gpGlobals->time + 0.1;

	if((m_iState == STATE_OFF || m_iState == STATE_TURN_OFF) && pev->avelocity.Length() < 350)
	{
		SetAngularImpulse(-(pev->dmg_save + RANDOM_FLOAT( 5,  10)));
		return;
	}
	     
	if(pOther->TakeDamage( pev, pev, fabs(pev->speed), DMG_CRUSH ))//blocked entity died ?
	{
		SetAngularImpulse(-(pev->dmg_save + RANDOM_FLOAT( 10,  20)));
		UTIL_FireTargets( pev->target, this, this, USE_SET );//damage
	}
	else if (RANDOM_LONG(0,1))
	{
		SetSpeed( 0 );//fan damaged - disable it
          	UTIL_FireTargets( pev->target, this, this, USE_SET );
          	//fan damaged( we can use counter and master for bloked broken fan)
	}
	if(!pOther->IsPlayer() && !pOther->IsMonster())//crash fan
	{
		if(IsBreakable())//if set strength - give damage myself
			CBaseBrush::TakeDamage( pev, pev, fabs(pev->speed/100), DMG_CRUSH );//if we give damage
          }
}

void CFuncRotating :: PostActivate ()
{
	if ( pev->spawnflags & SF_START_ON )SetSpeed( pev->speed );
	ClearBits( pev->spawnflags, SF_START_ON);

	if(m_iState == STATE_ON )//restore sound if needed
		EMIT_SOUND_DYN( ENT(pev), CHAN_STATIC, (char *)STRING(pev->noise3), m_flVolume, ATTN_NORM, SND_CHANGE_PITCH | SND_CHANGE_VOL, m_pitch);
	Msg("CFuncRotating:PostActivate()\n" );
	SetNextThink( 0.05 ); //force to think
}

void CFuncRotating :: RampPitchVol ( void )
{
	float fpitch;
	int pitch;
	float speedfactor = fabs(pev->dmg_save/pev->button);
	
	m_flVolume = speedfactor; //slowdown volume ramps down to 0
	fpitch = PITCHMIN + (PITCHMAX - PITCHMIN) * speedfactor;
	
	pitch = (int)fpitch;
	if (pitch == PITCH_NORM)pitch = PITCH_NORM-1;

	//normalize volume and pitch
	if(m_flVolume > 1.0)m_flVolume = 1.0;
	if(m_flVolume < 0.0)m_flVolume = 0.0;
	if(pitch < PITCHMIN) pitch = PITCHMIN;
	if(pitch > 255) pitch = 255;
	m_pitch = pitch;//refresh pitch
	
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, (char *)STRING(pev->noise3), m_flVolume, ATTN_NORM, SND_CHANGE_PITCH | SND_CHANGE_VOL, m_pitch);
}

void CFuncRotating :: SetSpeed( float newspeed )
{
	float speedfactor = fabs(pev->dmg_save/pev->speed);//speedfactor
	pev->armortype = gpGlobals->time;		//starttime
	pev->armorvalue = newspeed - pev->dmg_save;	//speed offset
	pev->scale = pev->dmg_save;			//cur speed
	
	//any speed change is turn on
	//may be it's wrong, but nice working :)
	if((pev->dmg_save > 0 && newspeed < 0) || (pev->dmg_save < 0 && newspeed > 0))//detect fast reverse
	{
		if(m_iState != STATE_OFF)pev->dmg_take = pev->frags * speedfactor;//fast reverse
		else pev->dmg_take = pev->frags;//get normal time
		m_iState = STATE_TURN_ON;
	}
	else if(newspeed == 0)
	{
		m_iState = STATE_TURN_OFF;
		pev->dmg_take = pev->frags * 2.5 * speedfactor;//spin down is longer
	}
	else //just change speed, not zerocrossing	
	{
		m_iState = STATE_TURN_ON;
		pev->dmg_take = pev->frags;//get normal time
	}
	SetNextThink( 0.05 );//force to think
}

void CFuncRotating :: SetAngularImpulse( float impulse )
{
	if( fabs(impulse) < 20)pev->scale = fabs(impulse / MAX_AVELOCITY);
	else pev->scale = 0.01;//scale factor
	
	m_iState = STATE_OFF;//impulse disable fan
	pev->dmg_save = pev->dmg_save + impulse;//add our impulse to curspeed	
	
	//normalize avelocity
	if(pev->dmg_save > MAX_AVELOCITY)pev->dmg_save = MAX_AVELOCITY;
	pev->max_health = MatFrictionTable( m_Material);//set friction coefficient

	SetNextThink(0.05);//pushable entity must thinking for apply velocity
}

void CFuncRotating :: Think( void )
{
	if(m_iState == STATE_TURN_ON || m_iState == STATE_TURN_OFF)
	{
                    flDegree = (gpGlobals->time - pev->armortype)/pev->dmg_take;
		
		if (flDegree >= 1)//full spin
		{
			if(m_iState == STATE_TURN_ON)
			{
				m_iState = STATE_ON;
				pev->dmg_save = pev->speed;//normalize speed
				UTIL_SetAvelocity(this, pev->movedir * pev->speed);//set normalized avelocity
				EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, (char *)STRING(pev->noise3), m_flVolume, ATTN_NORM, SND_CHANGE_PITCH | SND_CHANGE_VOL, m_pitch);
				UTIL_FireTargets( pev->target, this, this, USE_ON );//fan is full on
			}
			if(m_iState == STATE_TURN_OFF)
			{
				m_iState = STATE_OFF;
				if (pev->movedir == Vector(0,0,1))
					SetAngularImpulse(RANDOM_FLOAT(3.9, 6.8) * -pev->dmg_save);
				//set small negative impulse for pretty phys effect :)
			}
		}
		else
		{
			//calc new speed every frame
			pev->dmg_save = pev->scale + pev->armorvalue * flDegree;
			UTIL_SetAvelocity(this, pev->movedir * pev->dmg_save);
			RampPitchVol(); //play pitched sound
		}
	}
	if(m_iState == STATE_OFF)//apply impulse and friction
	{
		if(pev->dmg_save > 0)pev->dmg_save -= pev->max_health * pev->scale + 0.01;	
		else if(pev->dmg_save < 0)pev->dmg_save += pev->max_health * pev->scale + 0.01;
		UTIL_SetAvelocity(this, pev->movedir * pev->dmg_save);
		RampPitchVol(); //play pitched sound
	}
	//ALERT(at_console, "pev->avelocity %.2f %.2f %.2f\n", pev->avelocity.x, pev->avelocity.y, pev->avelocity.z);         
	SetNextThink( 0.05 );

	if(pev->avelocity.Length() < 0.5)//watch for speed
	{
		UTIL_SetAvelocity(this, g_vecZero);//stop
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, (char *)STRING(pev->noise3), 0, 0, SND_STOP, m_pitch);
		DontThink();//break thinking
		UTIL_FireTargets( pev->target, this, this, USE_OFF );//fan is full stopped
	}
}

void CFuncRotating :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	
	if(IsLockedByMaster( useType )) return;

	if (useType == USE_TOGGLE)
	{
		if(m_iState == STATE_TURN_OFF || m_iState == STATE_OFF) useType = USE_ON;
		else useType = USE_OFF;
	}	
	if (useType == USE_ON)SetSpeed( pev->speed );
	else if(useType == USE_OFF)SetSpeed( 0 );
	else if (useType == USE_SET)//reverse speed
	{
		//write new speed
		if(value) 
		{
			if(pev->speed < 0)
				pev->speed = -value;
			if(pev->speed > 0)
				pev->speed = value;
		}
		else pev->speed = -pev->speed;//just reverse
		//apply speed immediately if fan no off or not turning off
		if(m_iState != STATE_OFF && m_iState != STATE_TURN_OFF)SetSpeed( pev->speed );
	}
	else if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		ALERT(at_console, "State: %s, CurSpeed %.1f\n", GetStringForState( GetState()), pev->dmg_save);
		ALERT(at_console, "SpinTime: %.2f, MaxSpeed %.f\n", pev->frags, pev->speed);
	}
}

//=======================================================================
// 	func_pendulum - swinging brush 
//=======================================================================
class CPendulum : public CBaseMover
{
public:
	void Spawn ( void );
	void Precache( void );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Think( void );
	void Blocked( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	void Touch( CBaseEntity *pOther );
	void SetImpulse( float speed );
	void SetSwing( float speed );
	void PostActivate( void );

	int dir;//FIXME
};
LINK_ENTITY_TO_CLASS( func_pendulum, CPendulum );

void CPendulum :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "distance" ))
	{
		pev->scale = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "damp" ))
	{
		pev->max_health = atof( pkvd->szValue ) * 0.01f;
		if( pev->max_health > 0.01f ) pev->max_health = 0.01f;
		pkvd->fHandled = TRUE;
	}
	else CBaseBrush::KeyValue( pkvd );
}

void CPendulum :: Precache( void )
{
	CBaseBrush::Precache();
	int m_sounds = UTIL_LoadSoundPreset( m_iMoveSound );

	switch (m_sounds)
	{
	case 1:
		pev->noise3 = UTIL_PrecacheSound( "materials/swing/swing1.wav" );
		break;
	case 2:
		pev->noise3 = UTIL_PrecacheSound( "materials/swing/swing2.wav" );
		break;
	case 0:
		pev->noise3 = UTIL_PrecacheSound( "common/null.wav" );
		break;
	default:
		pev->noise3 = UTIL_PrecacheSound( m_sounds );
		break;
	}
}

void CPendulum :: Spawn( void )
{
	Precache();
	CBaseBrush::Spawn();

	if ( pev->spawnflags & SF_NOTSOLID )
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_PUSH;
	}
	else
	{
		pev->solid = SOLID_BSP;
		pev->movetype = MOVETYPE_PUSH;
	}
	
	SetBits (pFlags, PF_ANGULAR);
	dir = 1; // evil stuff...
          m_iState = STATE_OFF;
          pev->dmg_save = 0; //cur speed is NULL

	// NOTE: I'm not have better idea than it
	// So, field "angles" setting movedir only
	// Turn your brush into your fucking worldcraft!	
	UTIL_AngularVector( this );
          
	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetModel( ENT( pev ), pev->model );
}

void CPendulum :: PostActivate( void )
{
	if ( pev->spawnflags & SF_START_ON )SetSwing( pev->speed );
	ClearBits( pev->spawnflags, SF_START_ON);

	if(m_iState == STATE_ON )//restore sound if needed
	SetNextThink( 0.05 );//force to think
}

void CPendulum :: Touch ( CBaseEntity *pOther )
{
	// don't hurt toucher - just apply velocity him
	entvars_t	*pevOther = pOther->pev;
	pevOther->velocity = (pevOther->origin - VecBModelOrigin(pev) ).Normalize() * pev->dmg;
}

void CPendulum::Blocked( CBaseEntity *pOther )
{
	// what can do pendulum on blocking ?
	// Nothing :)
	pev->armortype = gpGlobals->time;
}

void CPendulum :: SetImpulse( float speed )
{
	if( dir == pev->impulse )
		return;
	dir = pev->impulse; 
          	
	SetSwing( speed );
}

void CPendulum :: SetSwing( float speed )
{
	if(m_iState == STATE_OFF)
		pev->armorvalue = speed;		//save our speed
	else	pev->armorvalue = pev->armorvalue + speed;//apply impulse
	
	//silence normalize speed if needed
	if(pev->armorvalue > MAX_AVELOCITY)pev->armorvalue = MAX_AVELOCITY;

	//normalize speed at think time
	//calc min distance at this speed
	float factor = MAX_AVELOCITY * gpGlobals->frametime; // max avelocity * think time
	float mindist = (pev->armorvalue/factor) * 2;//full minimal dist
          if(mindist > pev->scale)
          {
          	pev->scale = mindist;//set minimal distance for this speed
		if(m_iState == STATE_OFF)
		{
			Msg( "\n======/Xash SmartFiled System/======\n\n");
			Msg( "Warning! %s has too small distance for current speed!\nSmart filed normalize distance to %.f\n", STRING(pev->classname), pev->scale);
			SHIFT;
          	}
          }
          
	if( m_iState == STATE_OFF )
		pev->dmg_take = pev->scale * 0.5; // calc half distance
	else pev->dmg_take = pev->dmg_take + sqrt( speed );
	pev->dmg_save = pev->armorvalue; // at center speed is maximum

	//random choose starting dir
	if(m_iState == STATE_OFF)
	{
		if( RANDOM_LONG( 0, 1 ))
			pev->impulse = -1;	// backward
          	else pev->impulse = 1;	// forward
		m_iState = STATE_ON;
          }
	SetNextThink( 0.01f ); // force to think
}

void CPendulum :: Think( void )
{
	float m_flDegree = (gpGlobals->time - pev->armortype)/pev->frags;
	float m_flAngle = UTIL_CalcDistance(pev->angles);
	float m_flAngleOffset = pev->dmg_take - m_flAngle; 
	float m_flStep = ((pev->armorvalue * 0.01)/(pev->dmg_take * 0.01)) / M_PI;
	float m_damping = pev->max_health;
	if(m_iState == STATE_TURN_OFF) m_damping = 0.09;//hack to disable pendulum
	
	if(m_flDegree >= 1 && m_flAngleOffset <= 0.5)//extremum point
	{
		//recalc time
		pev->armortype = gpGlobals->time;
		pev->frags = pev->dmg_take/pev->armorvalue;
		pev->impulse = -pev->impulse;//change movedir
	}
	if(pev->avelocity.Length() > pev->armorvalue * 0.6 && m_damping)//apply damp
	{
		pev->armorvalue -= pev->armorvalue * m_damping;
		pev->dmg_take -= pev->dmg_take * m_damping;
          }

	if(pev->avelocity.Length() > pev->armorvalue * 0.95 && pev->dmg_take > 5.0)//zerocrossing
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, (char *)STRING(pev->noise3), m_flVolume, ATTN_IDLE, SND_CHANGE_VOL, PITCH_NORM);
		UTIL_FireTargets( pev->target, this, this, USE_OFF );
	}

	pev->dmg_save = pev->armorvalue * (m_flAngleOffset / pev->dmg_take);
	UTIL_SetAvelocity(this, pev->movedir * pev->dmg_save * pev->impulse);

	SetNextThink(0.01);
	if(pev->avelocity.Length() < 0.5 && pev->dmg_take < 2.0)//watch for speed and dist
	{
		UTIL_SetAvelocity(this, g_vecZero);//stop
		UTIL_AssignAngles(this, pev->angles );
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, (char *)STRING(pev->noise3), 0, 0, SND_STOP, m_pitch);
		DontThink();//break thinking
		UTIL_FireTargets( pev->target, this, this, USE_OFF );//pendulum is full stopped
		m_iState = STATE_OFF;
	}
}

void CPendulum :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	
	if( IsLockedByMaster( useType )) return;

	if( useType == USE_TOGGLE )
	{
		if(m_iState == STATE_OFF) useType = USE_ON;
		else useType = USE_OFF;
	}
	if( useType == USE_ON ) SetSwing( pev->speed );
	else if( useType == USE_OFF ) m_iState = STATE_TURN_OFF;
	else if( useType == USE_SET )
	{
		if( value ) pev->speed = value;
		else SetImpulse( pev->speed );//any idea ???
	}
	else if( useType == USE_RESET ) SetImpulse( pev->speed );
	else if(useType == USE_SHOWINFO)
	{
		ALERT(at_console, "======/Xash Debug System/======\n");
		ALERT(at_console, "classname: %s\n", STRING(pev->classname));
		ALERT(at_console, "State: %s, CurSpeed %.1f\n", GetStringForState( GetState()), pev->dmg_save);
		ALERT(at_console, "Distance: %.2f, Swing Time %.f\n", pev->dmg_take, pev->frags);
	}
}

//=======================================================================
// func_clock - simply rotating clock from Quake1.Scourge Of Armagon 
//=======================================================================
class CFuncClock : public CBaseLogic
{
public:
	void Spawn ( void );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Think( void );
	void PostActivate( void );
	void KeyValue( KeyValueData *pkvd );
	Vector curtime, finaltime;
	int bellcount;//calc hours
};
LINK_ENTITY_TO_CLASS( func_clock, CFuncClock );

void CFuncClock :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "type"))
	{
		switch(atoi( pkvd->szValue ))
		{
		case 1:  pev->impulse = MINUTES; break;	//minutes
		case 2:  pev->impulse = HOURS;   break;	//hours
		case 0:                                 //default:
		default: pev->impulse = SECONDS; break;	//seconds
		}
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "curtime"))
	{
		UTIL_StringToVector( curtime, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "event"))
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
}

void CFuncClock :: Spawn ( void )
{
	pev->solid = SOLID_NOT;
	UTIL_SetModel( ENT(pev), pev->model );

	// NOTE: I'm not have better idea than it
	// So, field "angles" setting movedir only
	// Turn your brush into your fucking worldcraft!	
	UTIL_AngularVector( this );
          SetBits( pFlags, PF_ANGULAR );
	
	if( pev->impulse == HOURS ) //do time operations
	{
		// normalize our time
		if( curtime.x > 11 ) curtime.x = 0;
		if( curtime.y > 59 ) curtime.y = 0;
		if( curtime.z > 59 ) curtime.z = 0;
		
		// member full hours
		pev->team = curtime.x;
		
		// calculate seconds
		finaltime.z = curtime.z * (SECONDS / 60); // seconds
                    finaltime.y = curtime.y * (MINUTES / 60) + finaltime.z;
                    finaltime.x = curtime.x * (HOURS   / 12) + finaltime.y;
	}
}

void CFuncClock :: PostActivate( void )
{
	// NOTE: We should be sure what all entities has be spawned
	// else we called this from Activate()

	if( pev->frags ) return;
	if( pev->impulse == HOURS && curtime != g_vecZero ) // it's hour entity and time set
	{
		// try to find minutes and seconds entity
		CBaseEntity *pEntity = NULL;
		while( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, pev->size.z ))
		{
			if( FClassnameIs( pEntity, "func_clock" ))
			{
				//write start hours, minutes and seconds
				if( pEntity->pev->impulse ==  HOURS  ) pEntity->pev->dmg_take = finaltime.x;
				if( pEntity->pev->impulse == MINUTES ) pEntity->pev->dmg_take = finaltime.y;
				if( pEntity->pev->impulse == SECONDS ) pEntity->pev->dmg_take = finaltime.z;	
			}
		}
	}
	pev->frags = 1;	// clock init
	m_iState = STATE_ON;// always on
	
	SetNextThink( 0 );	// force to think
}

void CFuncClock :: Think ( void )
{
	float seconds, ang, pos;

	seconds = gpGlobals->time + pev->dmg_take;
	pos = seconds / pev->impulse;
	pos = pos - floor( pos );
	ang = 360 * pos;

	UTIL_SetAngles( this, pev->movedir * ang );

	if(pev->impulse == HOURS) // play bell sound
	{
		pev->button = pev->angles.Length() / 30; // half hour
		if ( pev->team != pev->button ) // new hour
		{
			// member new hour
			bellcount = pev->team = pev->button;
			if( bellcount == 0 ) bellcount = 12; // merge for 0.00.00
			UTIL_FireTargets( pev->netname, this, this, USE_SET, bellcount ); // send hours info
			UTIL_FireTargets( pev->netname, this, this, USE_ON );//activate bell or logic_generator
		}
	}

	SetNextThink( 1 ); // refresh at one second
}

//=======================================================================
// 	    func_healthcharger - brush healthcharger 
//=======================================================================
class CWallCharger : public CBaseBrush
{
public:
	void Spawn( );
	void Precache( void );
	void Think (void);
	void KeyValue( KeyValueData* pkvd);
	virtual int ObjectCaps( void )
	{
		int flags = CBaseBrush:: ObjectCaps() & ~FCAP_ACROSS_TRANSITION;
		if(m_iState == STATE_DEAD) return flags;
		return  flags | FCAP_CONTINUOUS_USE;
	}
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	float m_flNextCharge; 
	float m_flSoundTime;
};
LINK_ENTITY_TO_CLASS(func_healthcharger, CWallCharger);
LINK_ENTITY_TO_CLASS(func_recharge, 	 CWallCharger);

void CWallCharger :: KeyValue( KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "chargetime"))//affect only in multiplayer
	{
		pev->armortype = atof(pkvd->szValue);
		if(pev->armortype <= 0 )pev->armortype = 1;
		pkvd->fHandled = TRUE;
	}
	else CBaseBrush::KeyValue( pkvd );
}

void CWallCharger::Spawn()
{
	Precache();
	CBaseBrush::Spawn();

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin(this, pev->origin);
	UTIL_SetSize(pev, pev->mins, pev->maxs);
	UTIL_SetModel(ENT(pev), pev->model );
	
	m_iState = STATE_OFF;
	pev->frame = 0;			

	//set capacity
	if (FClassnameIs(this, "func_recharge"))
		pev->frags = SUIT_CHARGER_CAP;
	else	pev->frags = HEATH_CHARGER_CAP;

	pev->dmg_save = pev->frags;
}

void CWallCharger::Precache()
{
	CBaseBrush::Precache();
	if (FClassnameIs(this, "func_recharge"))
	{
		UTIL_PrecacheSound("items/suitcharge1.wav");
		UTIL_PrecacheSound("items/suitchargeno1.wav");
		UTIL_PrecacheSound("items/suitchargeok1.wav");
	}
	else
	{
		UTIL_PrecacheSound("items/medshot4.wav");
		UTIL_PrecacheSound("items/medshotno1.wav");
		UTIL_PrecacheSound("items/medcharge4.wav");
	}
}

void CWallCharger::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	m_hActivator = pActivator;
	
	if(IsLockedByMaster( useType )) return;
	
	if(useType == USE_SHOWINFO)//show info
	{
		ALERT(at_console, "======/Xash Debug System/======\n");
		ALERT(at_console, "classname: %s\n", STRING(pev->classname));
		ALERT(at_console, "State: %s, Volume %.1f\n", GetStringForState( GetState()), m_flVolume );
		ALERT(at_console, "Texture frame: %.f. ChargeLevel: %.f\n", pev->frame, pev->dmg_save);
	}
	else if(pActivator && pActivator->IsPlayer())//take health
	{
		if (pev->dmg_save <= 0)
		{
			if (m_iState == STATE_IN_USE || m_iState == STATE_ON)
			{
				pev->frame = 1;
				if (FClassnameIs(this, "func_recharge"))
					STOP_SOUND( ENT(pev), CHAN_STATIC, "items/suitcharge1.wav" );
				else	STOP_SOUND( ENT(pev), CHAN_STATIC, "items/medcharge4.wav" );
				m_iState = STATE_DEAD;//recharge in multiplayer	
				if(IsMultiplayer()) SetNextThink( 3);//delay before recahrge
				else DontThink();
			}
		}

		if((m_iState == STATE_DEAD) || (!(((CBasePlayer *)pActivator)->m_iHideHUD & ITEM_SUIT)))
		{
			if (m_flSoundTime <= gpGlobals->time)
			{
				m_flSoundTime = gpGlobals->time + 0.62;
				if (FClassnameIs(this, "func_recharge"))
					EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "items/suitchargeno1.wav", m_flVolume, ATTN_IDLE, SND_CHANGE_VOL, PITCH_NORM );
				else	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "items/medshotno1.wav", m_flVolume, ATTN_IDLE, SND_CHANGE_VOL, PITCH_NORM );
				
			}
			return;
		}
		SetNextThink( 0.25);//time before disable

		if (m_flNextCharge >= gpGlobals->time) return;

		if (m_iState == STATE_OFF)
		{
			m_iState = STATE_ON;
			if (FClassnameIs(this, "func_recharge"))
				EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "items/suitchargeok1.wav", m_flVolume, ATTN_IDLE, SND_CHANGE_VOL, PITCH_NORM );
			else	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "items/medshot4.wav", m_flVolume, ATTN_IDLE, SND_CHANGE_VOL, PITCH_NORM );
			
			m_flSoundTime = 0.56 + gpGlobals->time;
		}
		if (m_iState == STATE_ON && m_flSoundTime <= gpGlobals->time)
		{
			m_iState = STATE_IN_USE;
			if (FClassnameIs(this, "func_recharge"))
				EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "items/suitcharge1.wav", m_flVolume, ATTN_IDLE, SND_CHANGE_VOL, PITCH_NORM );
			else	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "items/medcharge4.wav", m_flVolume, ATTN_IDLE, SND_CHANGE_VOL, PITCH_NORM );
		}

                    if (FClassnameIs(this, "func_recharge"))
                    {
                    	if (m_hActivator->pev->armorvalue < 100)
			{
				UTIL_FireTargets( pev->target, this, this, USE_OFF, pev->dmg_save );//decrement counter
				pev->dmg_save--;
				m_hActivator->pev->armorvalue++;
			}
		}
		else
		{
			if ( pActivator->TakeHealth(1, DMG_GENERIC )) 
			{
				UTIL_FireTargets( pev->target, this, this, USE_OFF, pev->dmg_save );//decrement counter
				pev->dmg_save--;
			}
		}
		m_flNextCharge = gpGlobals->time + 0.1;
	}
}

void CWallCharger :: Think( void )
{
	if (m_iState == STATE_IN_USE || m_iState == STATE_ON)
	{
		if (FClassnameIs(this, "func_recharge"))
			STOP_SOUND( ENT(pev), CHAN_STATIC, "items/suitcharge1.wav" );
		else	STOP_SOUND( ENT(pev), CHAN_STATIC, "items/medcharge4.wav" ); 

		m_iState = STATE_OFF;	
		DontThink();
		return;
	}
	if(m_iState == STATE_DEAD && IsMultiplayer())//recharge
	{
		pev->dmg_save++;//ressurection
		UTIL_FireTargets( pev->target, this, this, USE_ON, pev->dmg_save );//increment counter
		if(pev->dmg_save >= pev->frags)
		{
			//take full charge
			if (FClassnameIs(this, "func_recharge"))
				EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "items/suitchargeok1.wav", m_flVolume, ATTN_IDLE, SND_CHANGE_VOL, PITCH_NORM );
			else	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "items/medshot4.wav", m_flVolume, ATTN_IDLE, SND_CHANGE_VOL, PITCH_NORM );
			pev->dmg_save = pev->frags;//normalize
			pev->frame = 0;//enable
			m_iState = STATE_OFF;//can use
			DontThink();
			return;
		}			
	}
	SetNextThink( pev->armortype );
}

//=======================================================================
// 	    func_button - standard linear button from Quake1 
//=======================================================================
class CBaseButton : public CBaseMover
{
public:
	void Spawn( void );
	void PostSpawn( void );
	void Precache( void );
	virtual void KeyValue( KeyValueData* pkvd);
	virtual int ObjectCaps( void )
	{
		int flags = CBaseMover:: ObjectCaps() & ~FCAP_ACROSS_TRANSITION;
		if(m_iState == STATE_DEAD) return flags;
		return  flags | FCAP_IMPULSE_USE | FCAP_ONLYDIRECT_USE;
	}
	void EXPORT ButtonUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT ShowInfo ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT ButtonTouch( CBaseEntity *pOther );
	void EXPORT ButtonReturn( void );
	void EXPORT ButtonDone( void );
	void ButtonActivate( void );
};
LINK_ENTITY_TO_CLASS( func_button, CBaseButton );	

void CBaseButton::KeyValue( KeyValueData *pkvd )
{
	//rename standard fields for button
	if (FStrEq(pkvd->szKeyName, "locksound"))
	{
		m_iStopSound = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pushsound"))
	{
		m_iStartSound = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBaseMover::KeyValue( pkvd );
}

void CBaseButton::Precache( void )
{
	CBaseBrush::Precache();//precache damage sound

	int m_sounds = UTIL_LoadSoundPreset(m_iStartSound);
	switch (m_sounds)//load pushed sounds (sound will play at activate or pushed button)
	{
	case 1:	pev->noise = UTIL_PrecacheSound ("materials/buttons/blip1.wav");break;
	case 2:	pev->noise = UTIL_PrecacheSound ("materials/buttons/blip2.wav");break;
	case 3:	pev->noise = UTIL_PrecacheSound ("materials/buttons/blip3.wav");break;
	case 4:	pev->noise = UTIL_PrecacheSound ("materials/buttons/blip4.wav");break;
	case 5:	pev->noise = UTIL_PrecacheSound ("materials/buttons/blip5.wav");break;
	case 6:	pev->noise = UTIL_PrecacheSound ("materials/buttons/blip6.wav");break;
	case 0:	pev->noise = UTIL_PrecacheSound ("common/null.wav"); break;
	default:	pev->noise = UTIL_PrecacheSound(m_sounds); break;//custom sound or sentence
	}

	if (!FStringNull(m_sMaster))//button has master
	{
		m_sounds = UTIL_LoadSoundPreset(m_iStopSound);
		switch (m_sounds)//load locked sounds
		{
		case 1:	pev->noise3 = UTIL_PrecacheSound ("materials/buttons/locked1.wav");break;
		case 2:	pev->noise3 = UTIL_PrecacheSound ("materials/buttons/locked2.wav");break;
		case 0:	pev->noise3 = UTIL_PrecacheSound ("common/null.wav"); break;
		default:	pev->noise3 = UTIL_PrecacheSound(m_sounds); break;//custom sound or sentence
		}
	}
}

void CBaseButton::Spawn( )
{ 
	Precache();
	CBaseBrush::Spawn();

	if(pev->spawnflags & SF_NOTSOLID)//not solid button
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		pev->contents = CONTENTS_NONE;
	}
	else
	{
		pev->solid = SOLID_BSP;
         		pev->movetype = MOVETYPE_PUSH;
          }
	UTIL_SetModel(ENT(pev), pev->model);
	
	//determine work style
	if(m_iMode == 0)//classic HL button - only USE
	{
		SetUse ( ButtonUse );
		SetTouch ( NULL );
	}
	if(m_iMode == 1)//classic QUAKE button - only TOUCH
	{
		SetUse ( ShowInfo );//show info only
		SetTouch ( ButtonTouch );
	}	
	if(m_iMode == 2)//combo button - USE and TOUCH
	{
		SetUse ( ButtonUse );
		SetTouch ( ButtonTouch );
	}
	
	//as default any button is toggleable, but if mapmaker set waittime > 0
	//button will transformed into timebased button
          //if waittime is -1 - button forever stay pressed
          if(m_flWait == 0) pev->impulse = 1;//toggleable button
          
	if (m_flLip == 0) m_flLip = 4;//standart offset from Quake1

	m_iState = STATE_OFF;//disable at spawn
          UTIL_LinearVector( this );//movement direction
          	
	m_vecPosition1 = pev->origin;
	m_vecPosition2 = m_vecPosition1 + (pev->movedir * (fabs( pev->movedir.x * (pev->size.x-2) ) + fabs( pev->movedir.y * (pev->size.y-2) ) + fabs( pev->movedir.z * (pev->size.z-2) ) - m_flLip));

	// Is this a non-moving button?
	if( pev->speed == 0 )m_vecPosition2 = m_vecPosition1;
}

void CBaseButton :: PostSpawn( void )
{
	if (m_pParent) m_vecPosition1 = pev->origin - m_pParent->pev->origin;
	else m_vecPosition1 = pev->origin;
	m_vecPosition2 = m_vecPosition1 + (pev->movedir * (fabs( pev->movedir.x * (pev->size.x-2) ) + fabs( pev->movedir.y * (pev->size.y-2) ) + fabs( pev->movedir.z * (pev->size.z-2) ) - m_flLip));

	// Is this a non-moving button?
	if ( pev->speed == 0 ) m_vecPosition2 = m_vecPosition1;
}

void CBaseButton :: ShowInfo ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if(useType == USE_SHOWINFO)//show info
	{
		DEBUGHEAD;
		ALERT(at_console, "State: %s, Speed %.2f\n", GetStringForState( GetState()), pev->speed );
		ALERT(at_console, "Texture frame: %.f. WaitTime: %.2f\n", pev->frame, m_flWait);
	}
}

void CBaseButton :: ButtonUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;//save our activator
	
	if(IsLockedByMaster( useType ))//passed only USE_SHOWINFO
	{
          	EMIT_SOUND(ENT(pev), CHAN_VOICE, (char*)STRING(pev->noise3), 1, ATTN_NORM);
		return;
	}
	if(useType == USE_SHOWINFO)//show info
	{
		ALERT(at_console, "======/Xash Debug System/======\n");
		ALERT(at_console, "classname: %s\n", STRING(pev->classname));
		ALERT(at_console, "State: %s, Speed %.2f\n", GetStringForState( GetState()), pev->speed );
		ALERT(at_console, "Texture frame: %.f. WaitTime: %.2f\n", pev->frame, m_flWait);
	}
	else if(m_iState != STATE_DEAD)//activate button
	{         
		//NOTE: STATE_DEAD is better method for simulate m_flWait -1 without fucking SetThink()
		if(pActivator && pActivator->IsPlayer())//code for player
		{
			if(pev->impulse == 1)//toggleable button
			{
				if (m_iState == STATE_TURN_ON || m_iState == STATE_TURN_OFF )return;//buton in-moving
				else if( m_iState == STATE_ON )//button is active, disable
				{
					ButtonReturn();
				}
				else if( m_iState == STATE_OFF )//activate
				{
					ButtonActivate();
				}
			}
			else //time based button
			{         
				//can activate only disabled button
				if( m_iState == STATE_OFF ) ButtonActivate();
				else return;//does nothing :)	
			}
		}
		else //activate button from other entity
		{
			//NOTE: Across activation just fire output without changes
			UTIL_FireTargets( pev->target, pActivator, pCaller, useType, value );
		}
	}
}

void CBaseButton:: ButtonTouch( CBaseEntity *pOther )
{
	//make delay before retouching
	if ( gpGlobals->time < m_flBlockedTime) return;
	m_flBlockedTime = gpGlobals->time + 1.0;

	if(pOther->IsPlayer())ButtonUse ( pOther, this, USE_TOGGLE, 1 );//player always sending 1
}

void CBaseButton::ButtonActivate( )
{
	ASSERT(m_iState == STATE_OFF);
	m_iState = STATE_TURN_ON;
	
	EMIT_SOUND(ENT(pev), CHAN_VOICE, (char*)STRING(pev->noise), 1, ATTN_NORM);

	if(pev->speed)
	{
		SetMoveDone( ButtonDone );
		LinearMove( m_vecPosition2, pev->speed);
	}
	else ButtonDone();//immediately switch
}

void CBaseButton::ButtonDone( void )
{
	if(m_iState == STATE_TURN_ON)//turn on
	{
		m_iState = STATE_ON;
		pev->frame = 1;	// change skin

		if(pev->impulse)
			UTIL_FireTargets( pev->target, m_hActivator, this, USE_ON, 0 );//fire target	
		else UTIL_FireTargets( pev->target, m_hActivator, this, USE_TOGGLE, 0 );//fire target	

		if(m_flWait == -1)
		{
			m_iState = STATE_DEAD;//keep button in this position
			return;
		}

		if(pev->impulse == 0)//time base button
		{
			SetThink( ButtonReturn );
			SetNextThink( m_flWait );
		}
	}
	if(m_iState == STATE_TURN_OFF)//turn off
	{
		m_iState = STATE_OFF;//just change state :)
		if(pev->impulse) 
			UTIL_FireTargets( pev->target, m_hActivator, this, USE_OFF, 0 );//fire target	
	}
}

void CBaseButton::ButtonReturn( void )
{
	ASSERT(m_iState == STATE_ON);
	m_iState = STATE_TURN_OFF;
	pev->frame = 0;// use normal textures

	//make sound for toggleable button
	if(pev->impulse) EMIT_SOUND(ENT(pev), CHAN_VOICE, (char*)STRING(pev->noise), 1, ATTN_NORM);
	if(pev->speed)
	{
		SetMoveDone( ButtonDone );
		LinearMove( m_vecPosition1, pev->speed);
	}
	else ButtonDone();//immediately switch
}

//=======================================================================
// func_lever - momentary rotating button
//=======================================================================
class CFuncLever : public CBaseMover
{
public:
	void Spawn ( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void )
	{
		int flags = CBaseMover:: ObjectCaps() & ~FCAP_ACROSS_TRANSITION;
		if(m_iState == STATE_DEAD) return flags;
		return  flags | FCAP_CONTINUOUS_USE;
	}
	
	BOOL MaxRange( void )
	{
		if(pev->impulse ==  1 && m_flMoveDistance > 0) return TRUE;
		if(pev->impulse == -1 && m_flMoveDistance < 0) return TRUE;
		return FALSE;
	}
	BOOL BackDir( void )//determine start direction
	{
		if( pev->dmg_save && m_flMoveDistance < 0) return TRUE;
		if(!pev->dmg_save && m_flMoveDistance > 0) return TRUE;
		return FALSE;	
	}
	void PlayMoveSound( void );
	void CalcValue( void );
	void Think( void );
	
	float mTimeToReverse, SendTime, nextplay;
};
LINK_ENTITY_TO_CLASS( func_lever, CFuncLever );

void CFuncLever::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "backspeed"))
	{
		pev->dmg_save = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "movesound"))
	{
		m_iMoveSound = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "stopsound"))
	{
		m_iStopSound = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBaseMover::KeyValue( pkvd );
}

void CFuncLever::Precache( void )
{
	int m_sounds = UTIL_LoadSoundPreset(m_iMoveSound);
	switch (m_sounds)//load pushed sounds (sound will play at activate or pushed button)
	{
	case 1:	pev->noise = UTIL_PrecacheSound ("materials/buttons/lever1.wav");break;
	case 2:	pev->noise = UTIL_PrecacheSound ("materials/buttons/lever2.wav");break;
	case 3:	pev->noise = UTIL_PrecacheSound ("materials/buttons/lever3.wav");break;
	case 4:	pev->noise = UTIL_PrecacheSound ("materials/buttons/lever4.wav");break;
	case 5:	pev->noise = UTIL_PrecacheSound ("materials/buttons/lever5.wav");break;
	case 0:	pev->noise = UTIL_PrecacheSound ("common/null.wav"); break;
	default:	pev->noise = UTIL_PrecacheSound(m_sounds); break;//custom sound or sentence
	}

	m_sounds = UTIL_LoadSoundPreset(m_iStopSound);
	switch (m_sounds)//load locked sounds
	{
	case 1:	pev->noise2 = UTIL_PrecacheSound ("materials/buttons/lstop1.wav");break;
	case 0:	pev->noise2 = UTIL_PrecacheSound ("common/null.wav"); break;
	default:	pev->noise2 = UTIL_PrecacheSound(m_sounds); break;//custom sound or sentence
	}
}

void CFuncLever::Spawn( void )
{
	Precache();
	if(pev->spawnflags & SF_NOTSOLID)//not solid button
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		pev->contents = CONTENTS_NONE;
	}
	else
	{
		pev->solid = SOLID_BSP;
         		pev->movetype = MOVETYPE_PUSH;
          }
	UTIL_SetModel(ENT(pev), pev->model);
	UTIL_SetOrigin(this, pev->origin);

	AxisDir();
          SetBits (pFlags, PF_ANGULAR);
          
	//Smart field system ®
	if( pev->speed == 0 ) pev->speed = 100;//check null speed
	if( pev->speed > 800) pev->speed = 800;//check max speed
	if( fabs(m_flMoveDistance) < pev->speed/2)
	{
		if(m_flMoveDistance > 0)m_flMoveDistance = -pev->speed/2;
          	if(m_flMoveDistance < 0)m_flMoveDistance =  pev->speed/2;
          	if(m_flMoveDistance ==0)m_flMoveDistance = 45;
          }
	//check for direction (right direction will be set at first called use)
	if(BackDir())pev->impulse = -1;//backward dir
	else pev->impulse = 1;//forward dir
}

void CFuncLever::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	if(IsLockedByMaster( useType ))//strange stuff...
	{
          	EMIT_SOUND(ENT(pev), CHAN_VOICE, (char*)STRING(pev->noise2), 1, ATTN_NORM);
		return;
	}
	if(useType == USE_SHOWINFO)
	{
	 	ALERT(at_console, "======/Xash Debug System/======\n");
		ALERT(at_console, "classname: %s\n", STRING(pev->classname));
		ALERT(at_console, "State: %s, Value %.2f\n", GetStringForState( GetState()), pev->ideal_yaw );
		ALERT(at_console, "Distance: %.f. Speed %.2f\n", m_flMoveDistance, pev->speed );
          }
	else if(m_iState != STATE_DEAD)
	{
		if(pActivator && pActivator->IsPlayer())//code only for player
		{
			if(gpGlobals->time > mTimeToReverse && !pev->dmg_save)//don't switch if backspeed set
				pev->impulse = -pev->impulse;
			mTimeToReverse = gpGlobals->time + 0.3;//max time to change dir	

			pev->frags = pev->speed;//send speed
			m_iState = STATE_IN_USE;//in use
			SetNextThink(0.01);
		}
	}
}

void CFuncLever::Think( void )
{
	UTIL_SetAvelocity(this, pev->movedir * pev->frags * pev->impulse);//set speed and dir
	if(m_iState == STATE_TURN_OFF && pev->dmg_save)pev->frags = -pev->dmg_save;//backspeed
 	if(!pev->dmg_save)pev->frags -= pev->frags * 0.05;
	CalcValue();
	PlayMoveSound();

	//wacthing code
          if(pev->avelocity.Length() < 0.5)m_iState = STATE_OFF;//watch min speed
	if(pev->ideal_yaw <= 0.01)//min range
	{
		if(pev->dmg_save && m_iState != STATE_IN_USE)
		{
			UTIL_SetAvelocity(this, g_vecZero);
			m_iState = STATE_OFF;//off
		}
		else if(!MaxRange())UTIL_SetAvelocity(this, g_vecZero);
	}
	if(pev->ideal_yaw >= 0.99)//max range
	{
		if(pev->dmg_save && m_iState == STATE_IN_USE)
		{
			UTIL_SetAvelocity(this, g_vecZero);
			if(pev->spawnflags & SF_START_ON)//Stay open flag
				m_iState = STATE_DEAD;//dead
			else m_iState = STATE_TURN_OFF;//must return
		}
		else if(MaxRange() && m_iState == STATE_IN_USE)
		{
			UTIL_SetAvelocity(this, g_vecZero);
			if(pev->spawnflags & SF_START_ON)//Stay open flag
				m_iState = STATE_DEAD;//dead
			else m_iState = STATE_OFF;//dist is out
		}
	}
	if(m_iState == STATE_IN_USE)m_iState = STATE_TURN_OFF;//try disable

	SetNextThink(0.01);

	if(m_iState == STATE_OFF || m_iState == STATE_DEAD)//STATE_DEAD is one end way
	{
		if(pev->dmg_save)//play sound only auto return
			EMIT_SOUND(ENT(pev), CHAN_VOICE, (char*)STRING(pev->noise2), 1, ATTN_NORM);
		UTIL_SetAvelocity(this, g_vecZero);
		UTIL_AssignAngles( this, pev->angles );
		DontThink();//break thinking
	}
	//ALERT(at_console, "Ideal Yaw %f\n", pev->ideal_yaw);
}

void CFuncLever::CalcValue( void )
{
	//calc value to send
	//pev->ideal_yaw = UTIL_CalcDistance(pev->angles) / fabs(m_flMoveDistance);
	pev->ideal_yaw = pev->angles.Length() / fabs(m_flMoveDistance);
	if(pev->ideal_yaw >= 1)pev->ideal_yaw = 1;//normalize value
	else if(pev->ideal_yaw <= 0)pev->ideal_yaw = 0;//normalize value
	
	if(gpGlobals->time > SendTime)
	{
		UTIL_FireTargets( pev->target, this, this, USE_SET, pev->ideal_yaw );//send value
		SendTime = gpGlobals->time + 0.01;//time to nextsend
	}
}

void CFuncLever::PlayMoveSound( void )
{
	if(m_iState == STATE_TURN_OFF && pev->dmg_save )
		nextplay = fabs((pev->dmg_save * 0.01) / (m_flMoveDistance * 0.01));
	else nextplay = fabs((pev->frags * 0.01) / (m_flMoveDistance * 0.01));
	nextplay = 1/(nextplay * 5);

	if(gpGlobals->time > m_flBlockedTime)
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, (char*)STRING(pev->noise), 1, ATTN_NORM);
		m_flBlockedTime = gpGlobals->time + nextplay;
	}
}

//=======================================================================
// func_ladder - makes an area vertically negotiable
//=======================================================================
class CLadder : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
};
LINK_ENTITY_TO_CLASS( func_ladder, CLadder );

void CLadder :: Precache( void )
{
	pev->solid = SOLID_NOT;
	pev->contents = CONTENTS_LADDER;

	pev->effects |= EF_NODRAW;
}

void CLadder :: Spawn( void )
{
	Precache();

	UTIL_SetModel(ENT(pev), pev->model); // set size and link into world
	pev->movetype = MOVETYPE_PUSH;
}

//=======================================================================
// func_scaner - retinal scaner
//=======================================================================
class CFuncScaner : public CBaseBrush
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void PostActivate( void );
	void Think( void );
	BOOL VisionCheck( void );
	BOOL CanSee(CBaseEntity *pLooker );
	CBaseEntity *pSensor;
	CBaseEntity *pLooker;
};
LINK_ENTITY_TO_CLASS( func_scaner, CFuncScaner );

void CFuncScaner::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "sensor"))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "acesslevel"))
	{
		pev->impulse = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "sense"))
	{
		pev->health = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	CBaseLogic::KeyValue( pkvd );
}

void CFuncScaner :: Precache( void )
{
	CBaseBrush::Precache();

	UTIL_PrecacheSound( "buttons/blip1.wav" );
	UTIL_PrecacheSound( "buttons/blip2.wav" );
	UTIL_PrecacheSound( "buttons/button7.wav" );
	UTIL_PrecacheSound( "buttons/button11.wav" );
}

void CFuncScaner :: Spawn( void )
{
	Precache();
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	if(!pev->health) pev->health = 15;
	UTIL_SetModel(ENT(pev), pev->model);
	UTIL_SetOrigin(this, pev->origin);
          m_flDelay = 0.1;
          
	SetNextThink( 1 );
}

void CFuncScaner :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		Msg( "State: %s, Acess Level %d\n", GetStringForState( GetState()), pev->impulse);
		Msg( "Sense Corner: %g\n", pev->health);
	}
	else if (useType == USE_SET)
	{
		if(value) 
		{
			pev->armorvalue = (int)value;//set new acess level for looker
			pev->frame = 1;
		}
	}
}

void CFuncScaner :: Think( void )
{
	if (VisionCheck() && m_iState != STATE_ON && m_iState != STATE_DEAD)
	{
		if(pev->armorvalue)
		{
			m_iState = STATE_DEAD;
			pLooker->m_iAcessLevel = (int)pev->armorvalue;
			pev->armorvalue = 0;
			//change acess level sound
			EMIT_SOUND( edict(), CHAN_BODY, "buttons/blip2.wav", 1.0, ATTN_NORM );
		}
		else if(m_iState == STATE_OFF)//scaner is off
		{
			//ok, we have seen entity
			if(pLooker->pev->flags & FL_CLIENT) m_flDelay = 0.1;
			else m_flDelay = 0.9;
			m_iState = STATE_TURN_ON;
		}
		else if(m_iState == STATE_TURN_ON)//scaner is turn on
		{
			EMIT_SOUND( edict(), CHAN_BODY, "buttons/blip1.wav", 1.0, ATTN_NORM );
			pev->frame = 1;			
			m_flDelay = 0.3;
			pev->button++;
			if(pev->button > 9)
			{
				m_iState = STATE_ON;
				pev->team = pLooker->m_iAcessLevel;//save our level acess
				pev->button = 0;
			}
		}
	}
	else if(m_iState == STATE_ON)//scaner is on
	{
		EMIT_SOUND( edict(), CHAN_BODY, "buttons/blip1.wav", 1.0, ATTN_NORM );
		m_flDelay = 0.1;
		pev->button++;
		if(pev->button > 9)
		{
			m_iState = STATE_OFF;
			pev->frame = 0;
			pev->button = 0;
			m_flDelay = 2.0;

			if(pev->team >= pev->impulse )//acees level is math or higher ?
			{
				EMIT_SOUND( edict(), CHAN_BODY, "buttons/button7.wav", 1.0, ATTN_NORM );
				UTIL_FireTargets( pev->target, this, this, USE_TOGGLE ); 
			}
			else 
			{
				EMIT_SOUND( edict(), CHAN_BODY, "buttons/button11.wav", 1.0, ATTN_NORM );
				UTIL_FireTargets( pev->netname, this, this, USE_TOGGLE ); 
			}
		}
	}
	else if(m_iState == STATE_TURN_ON)
	{
		EMIT_SOUND( edict(), CHAN_BODY, "buttons/button11.wav", 1.0, ATTN_NORM );
		m_iState = STATE_OFF;
		pev->button = 0;
		pev->frame = 0;
		m_flDelay = 2.0;
	}
	else if(m_iState == STATE_DEAD)
	{
		m_iState = STATE_OFF;
		pev->button = 0;
		m_flDelay = 2.0;
	}
	
	// is this a sensible rate?
	SetNextThink( m_flDelay );
}

void CFuncScaner :: PostActivate( void )
{
	if (pev->message) 
	{
		pSensor = UTIL_FindEntityByTargetname(NULL, STRING(pev->message));
		if(!pSensor) pSensor = this;
	}
	else pSensor = this;
}

BOOL CFuncScaner :: VisionCheck( void )
{
	pLooker = UTIL_FindEntityInSphere( NULL, pSensor->pev->origin, 30 );
	
	if (pLooker)
	{
		while( pLooker != NULL )
		{
			if(pLooker && pLooker->pev->flags & (FL_MONSTER | FL_CLIENT))
				return CanSee(pLooker);//looker found
			pLooker = UTIL_FindEntityInSphere( pLooker, pSensor->pev->origin, 30 );	
		}
	}
	return FALSE;//no lookers
}

BOOL CFuncScaner :: CanSee(CBaseEntity *pLooker )
{
	if(!pSensor || !pLooker) return FALSE;

	if ((pLooker->EyePosition() - pSensor->pev->origin).Length() > 30) return FALSE;

	// copied from CBaseMonster's FInViewCone function
	Vector2D	vec2LOS;
	float flDot, flComp = cos(pev->health/2 * M_PI/180.0);
	
	UTIL_MakeVectors ( pLooker->pev->angles );
	vec2LOS = ( pSensor->pev->origin - pLooker->pev->origin ).Make2D();
	vec2LOS = vec2LOS.Normalize();
	flDot = DotProduct (vec2LOS , gpGlobals->v_forward.Make2D() );

	if (flDot <= flComp) return FALSE;
	return TRUE;
}

//=======================================================================
// volume of space that the player must stand in to control the train
//=======================================================================
class CFuncTrainControls : public CPointEntity
{
public:
	void Spawn( void );
	void PostSpawn( void );
};
LINK_ENTITY_TO_CLASS( func_traincontrols, CFuncTrainControls );

void CFuncTrainControls :: PostSpawn( void )
{
	CBaseEntity *pTarget = NULL;

	do 
	{
	pTarget = UTIL_FindEntityByTargetname( pTarget, STRING(pev->target) );
	} while ( pTarget && !FClassnameIs(pTarget->pev, "func_tracktrain") );

	if ( !pTarget )
	{
		ALERT( at_console, "TrackTrainControls: No train %s\n", STRING(pev->target) );
		return;
	}

	CFuncTrackTrain *ptrain = (CFuncTrackTrain*)pTarget;
	ptrain->SetControls( pev );
	UTIL_Remove( this );
}

void CFuncTrainControls :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	UTIL_SetModel( ENT(pev), pev->model );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( this, pev->origin );
}