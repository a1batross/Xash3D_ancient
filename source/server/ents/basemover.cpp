//=======================================================================
//			Copyright (C) Shambler Team 2005
//		         basemover.cpp - base code for linear and 
//		   angular moving brushes e.g. doors, buttons e.t.c 			    
//=======================================================================

#include "extdll.h"
#include "defaults.h"
#include "utils.h"
#include "cbase.h"
#include "client.h"
#include "saverestore.h"
#include "player.h"

//=======================================================================
// 		   main functions ()
//=======================================================================
TYPEDESCRIPTION CBaseMover::m_SaveData[] = 
{
	DEFINE_FIELD( CBaseMover, m_flBlockedTime, FIELD_TIME ),
	DEFINE_FIELD( CBaseMover, m_iMode, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseMover, m_flMoveDistance, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseMover, m_flWait, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseMover, m_flLip, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseMover, m_flHeight, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseMover, m_flValue, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseMover, m_vecFinalDest, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseMover, m_vecPosition1, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseMover, m_vecPosition2, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseMover, m_vecFloor, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseMover, m_pfnCallWhenMoveDone, FIELD_FUNCTION ),
	DEFINE_FIELD( CBaseMover, m_vecAngle1, FIELD_VECTOR ),
	DEFINE_FIELD( CBaseMover, m_vecAngle2, FIELD_VECTOR ),
	DEFINE_FIELD( CBaseMover, m_flLinearMoveSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseMover, m_flAngularMoveSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseMover, m_vecFinalAngle, FIELD_VECTOR ),
	DEFINE_FIELD( CBaseMover, flTravelTime, FIELD_FLOAT ),
}; IMPLEMENT_SAVERESTORE( CBaseMover, CBaseBrush );

void CBaseMover :: AxisDir( void )
{
	// make backward compatibility
	if ( pev->movedir != g_vecZero) return;
	if ( FBitSet(pev->spawnflags, 128))
		pev->movedir = Vector ( 0, 0, 1 );	// around z-axis
	else if ( FBitSet(pev->spawnflags, 64))
		pev->movedir = Vector ( 1, 0, 0 );	// around x-axis
	else	pev->movedir = Vector ( 0, 1, 0 );	// around y-axis
}

void CBaseMover::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "lip"))
	{
		m_flLip = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "waveheight"))
	{
		//field for volume_water
		pev->scale = atof(pkvd->szValue) * (1.0/8.0);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "type"))
	{
		m_iMode = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_flWait = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "locksound"))
	{
		m_iStartSound = ALLOC_STRING(pkvd->szValue);
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
	else if (FStrEq(pkvd->szKeyName, "distance"))
	{
		m_flMoveDistance = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "height"))
	{
		m_flHeight = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "waveheight"))
	{
		// func_water wave height
		pev->scale = atof( pkvd->szValue ) * ( 1.0f / 8.0f );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "contents"))
	{
		// func_water contents
		pev->skin = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CBaseBrush::KeyValue( pkvd );
}

//=======================================================================
//	LinearMove
//
//   calculate pev->velocity and pev->nextthink to reach vecDest from
//   pev->origin traveling at flSpeed
//=======================================================================
void CBaseMover ::  LinearMove( Vector	vecInput, float flSpeed )
{
	ASSERTSZ(flSpeed != 0, "LinearMove:  no speed is defined!");
	
	m_flLinearMoveSpeed = flSpeed;
	m_vecFinalDest = vecInput;

	SetThink( LinearMoveNow );
	UTIL_SetThink( this );
}


void CBaseMover :: LinearMoveNow( void )
{
	Vector vecDest;

	if (m_pParent)vecDest = m_vecFinalDest + m_pParent->pev->origin;
	else vecDest = m_vecFinalDest;

	// Already there?
	if (vecDest == pev->origin)
	{
		LinearMoveDone();
		return;
	}
		
	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDest - pev->origin;
	
	// divide vector length by speed to get time to reach dest
	flTravelTime = vecDestDelta.Length() / m_flLinearMoveSpeed;

	// set nextthink to trigger a call to LinearMoveDone when dest is reached
	SetNextThink( flTravelTime );
	SetThink( LinearMoveDone );

	UTIL_SetVelocity( this, vecDestDelta / flTravelTime );
}

//=======================================================================
// After moving, set origin to exact final destination, 
// call "move done" function
//=======================================================================
void CBaseMover :: LinearMoveDone( void )
{
	SetThink(LinearMoveDoneNow);
	UTIL_SetThink( this );
}

void CBaseMover :: LinearMoveDoneNow( void )
{
	UTIL_SetVelocity(this, g_vecZero);
	if (m_pParent) UTIL_AssignOrigin(this, m_vecFinalDest + m_pParent->pev->origin); 
	else UTIL_AssignOrigin(this, m_vecFinalDest);
          
	DontThink();
	if( m_pfnCallWhenMoveDone )(this->*m_pfnCallWhenMoveDone)();
}

//=======================================================================
//	AngularMove
//
// calculate pev->velocity and pev->nextthink to reach vecDest from
// pev->origin traveling at flSpeed
// Just like LinearMove, but rotational.
//=======================================================================
void CBaseMover :: AngularMove( Vector vecDestAngle, float flSpeed )
{
	ASSERTSZ(flSpeed != 0, "AngularMove:  no speed is defined!");
	
	m_vecFinalAngle = vecDestAngle;
	m_flAngularMoveSpeed = flSpeed;

	SetThink( AngularMoveNow );
	UTIL_SetThink( this );
}

void CBaseMover :: AngularMoveNow()
{
	Vector vecDestAngle;

	if (m_pParent) vecDestAngle = m_vecFinalAngle + m_pParent->pev->angles;
	else vecDestAngle = m_vecFinalAngle;

	// Already there?
	if (vecDestAngle == pev->angles)
	{
		AngularMoveDone();
		return;
	}
	
	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDestAngle - pev->angles;
	
	// divide by speed to get time to reach dest
	flTravelTime = vecDestDelta.Length() / m_flAngularMoveSpeed;

	// set nextthink to trigger a call to AngularMoveDone when dest is reached
	SetNextThink( flTravelTime );
	SetThink( AngularMoveDone );

	// scale the destdelta vector by the time spent traveling to get velocity
	UTIL_SetAvelocity(this, vecDestDelta / flTravelTime );
}

void CBaseMover :: AngularMoveDone( void )
{
	SetThink( AngularMoveDoneNow );
	UTIL_SetThink( this );
}

//=======================================================================
// After rotating, set angle to exact final angle, call "move done" function
//=======================================================================
void CBaseMover :: AngularMoveDoneNow( void )
{
	UTIL_SetAvelocity(this, g_vecZero);
	if (m_pParent) UTIL_AssignAngles(this, m_vecFinalAngle + m_pParent->pev->angles);
	else UTIL_AssignAngles(this, m_vecFinalAngle);

	DontThink();
	if ( m_pfnCallWhenMoveDone ) (this->*m_pfnCallWhenMoveDone)();
}

//=======================================================================
//	ComplexMove
//
// combinate LinearMove and AngularMove
//=======================================================================
void CBaseMover :: ComplexMove( Vector vecInput, Vector vecDestAngle, float flSpeed )
{
	ASSERTSZ(flSpeed != 0, "ComplexMove:  no speed is defined!");
	
	//set shared speed for moving and rotating
	m_flLinearMoveSpeed = flSpeed;
	m_flAngularMoveSpeed = flSpeed;
	
	//save local variables into global containers
	m_vecFinalDest = vecInput;
	m_vecFinalAngle = vecDestAngle;
	
	SetThink( ComplexMoveNow );
	UTIL_SetThink( this );
}

void CBaseMover :: ComplexMoveNow( void )
{
	Vector vecDest, vecDestAngle;

	if (m_pParent)//calculate destination
	{
		vecDest = m_vecFinalDest + m_pParent->pev->origin;
		vecDestAngle = m_vecFinalAngle + m_pParent->pev->angles;
	}
	else
	{
		vecDestAngle = m_vecFinalAngle;
		vecDest = m_vecFinalDest;
          }

	// Already there?
	if (vecDest == pev->origin && vecDestAngle == pev->angles)
	{
		ComplexMoveDone();
		return;
	}
	
	// Calculate TravelTime and final angles
	Vector vecDestLDelta = vecDest - pev->origin;
	Vector vecDestADelta = vecDestAngle - pev->angles;
	
	// divide vector length by speed to get time to reach dest
	flTravelTime = vecDestLDelta.Length() / m_flLinearMoveSpeed;
          
	// set nextthink to trigger a call to LinearMoveDone when dest is reached
	SetNextThink( flTravelTime );
	SetThink( ComplexMoveDone );

	//set linear and angular velocity now
	UTIL_SetVelocity( this, vecDestLDelta / flTravelTime );
	UTIL_SetAvelocity(this, vecDestADelta / flTravelTime );
}

void CBaseMover :: ComplexMoveDone( void )
{
	SetThink(ComplexMoveDoneNow);
	UTIL_SetThink( this );
}

void CBaseMover :: ComplexMoveDoneNow( void )
{
	UTIL_SetVelocity(this, g_vecZero);
	UTIL_SetAvelocity(this, g_vecZero);
	
	if (m_pParent)
	{
		UTIL_AssignOrigin(this, m_vecFinalDest + m_pParent->pev->origin);
		UTIL_AssignAngles(this, m_vecFinalAngle + m_pParent->pev->angles);
	}
	else
	{
		UTIL_AssignOrigin(this, m_vecFinalDest);
          	UTIL_AssignAngles(this, m_vecFinalAngle);
          }
	
	DontThink();
	if( m_pfnCallWhenMoveDone )(this->*m_pfnCallWhenMoveDone)();
}

//=======================================================================
// 		   func_door - classic QUAKE door
//=======================================================================
void CBaseDoor::Spawn( void )
{
	Precache();
          if(!IsRotatingDoor()) UTIL_LinearVector( this );
	CBaseBrush::Spawn();

	if ( pev->spawnflags & SF_NOTSOLID ) // make illusionary door 
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
	}
	else
	{
		pev->solid = SOLID_BSP;
          	pev->movetype = MOVETYPE_PUSH;
          }

	if( IsRotatingDoor( ))
	{
		// check for clockwise rotation
		if ( m_flMoveDistance < 0 ) pev->movedir = pev->movedir * -1;
	          
	          AxisDir();
		m_vecAngle1 = pev->angles;
		m_vecAngle2 = pev->angles + pev->movedir * m_flMoveDistance;

		ASSERTSZ( m_vecAngle1 != m_vecAngle2, "rotating door start/end positions are equal" );
         
          	SetBits ( pFlags, PF_ANGULAR );
          }
          
	UTIL_SetModel( ENT(pev), pev->model );
	UTIL_SetOrigin( this, pev->origin );

	// determine work style
	if ( m_iMode == 0 ) // normal door - only USE
	{
		SetUse ( DoorUse );
		SetTouch ( NULL );
		pev->team = 1; // info_node identifier. Do not edit!!!
	}

	if( m_iMode == 1 ) // classic QUAKE & HL door - only TOUCH
	{
		SetUse ( ShowInfo );//show info only
		SetTouch ( DoorTouch );
	}	

	if( m_iMode == 2 ) // combo door - USE and TOUCH
	{
		SetUse ( DoorUse );
		SetTouch ( DoorTouch );
	}

	// as default any door is toggleable, but if mapmaker set waittime > 0
	// door will de transformed into timebased door
          // if waittime is -1 - button forever stay pressed
          if ( m_flWait <= 0.0f ) pev->impulse = 1; // toggleable door
	if ( m_flLip == 0 ) m_flLip = 4; // standart offset from Quake1
	
	if ( pev->speed == 0 )
		pev->speed = 100; // default speed
	m_iState = STATE_OFF;
}

void CBaseDoor :: PostSpawn( void )
{
	if (m_pParent) m_vecPosition1 = pev->origin - m_pParent->pev->origin;
	else m_vecPosition1 = pev->origin;

	// Subtract 2 from size because the engine expands bboxes by 1 in all directions
	m_vecPosition2 = m_vecPosition1 + (pev->movedir * (fabs( pev->movedir.x * (pev->size.x-2) ) + fabs( pev->movedir.y * (pev->size.y-2) ) + fabs( pev->movedir.z * (pev->size.z-2) ) - m_flLip));

	ASSERTSZ( m_vecPosition1 != m_vecPosition2, "door start/end positions are equal" );

	if ( FBitSet( pev->spawnflags, SF_START_ON ))
	{	
		if ( m_pParent )
		{
			m_vecSpawnOffset = m_vecSpawnOffset + (m_vecPosition2 + m_pParent->pev->origin) - pev->origin;
			UTIL_AssignOrigin( this, m_vecPosition2 + m_pParent->pev->origin );
		}
		else
		{
			m_vecSpawnOffset = m_vecSpawnOffset + m_vecPosition2 - pev->origin;
			UTIL_AssignOrigin( this, m_vecPosition2 );
		}
		Vector vecTemp = m_vecPosition2;
		m_vecPosition2 = m_vecPosition1;
		m_vecPosition1 = vecTemp;
	}
}

void CBaseDoor :: SetToggleState( int state )
{
	if ( m_iState == STATE_ON )
	{
		if ( m_pParent ) UTIL_AssignOrigin( this, m_vecPosition2 + m_pParent->pev->origin );
		else UTIL_AssignOrigin( this, m_vecPosition2 );
	}
	else
	{
		if ( m_pParent ) UTIL_AssignOrigin( this, m_vecPosition1 + m_pParent->pev->origin );
		else UTIL_AssignOrigin( this, m_vecPosition1 );
	}
}

void CBaseDoor::Precache( void )
{
	CBaseBrush::Precache();	// precache damage sound

	int m_sounds = UTIL_LoadSoundPreset( m_iMoveSound );
	switch ( m_sounds )		// load movesound sounds (sound will play when door is moving)
	{
	case 1:	pev->noise1 = UTIL_PrecacheSound ("doors/doormove1.wav");break;
	case 2:	pev->noise1 = UTIL_PrecacheSound ("doors/doormove2.wav");break;
	case 3:	pev->noise1 = UTIL_PrecacheSound ("doors/doormove3.wav");break;
	case 4:	pev->noise1 = UTIL_PrecacheSound ("doors/doormove4.wav");break;
	case 5:	pev->noise1 = UTIL_PrecacheSound ("doors/doormove5.wav");break;
	case 6:	pev->noise1 = UTIL_PrecacheSound ("doors/doormove6.wav");break;
	case 7:	pev->noise1 = UTIL_PrecacheSound ("doors/doormove7.wav");break;
	case 8:	pev->noise1 = UTIL_PrecacheSound ("doors/doormove8.wav");break;
	case 9:	pev->noise1 = UTIL_PrecacheSound ("doors/doormove9.wav");break;
	case 10:	pev->noise1 = UTIL_PrecacheSound ("doors/doormove10.wav");break;			
	case 0:	pev->noise1 = UTIL_PrecacheSound ("common/null.wav"); break;
	default:	pev->noise1 = UTIL_PrecacheSound(m_sounds); break; // custom sound or sentence
	}

	m_sounds = UTIL_LoadSoundPreset( m_iStopSound );
	switch ( m_sounds ) // load pushed sounds (sound will play at activate or pushed button)
	{
	case 1:	pev->noise2 = UTIL_PrecacheSound ("doors/doorstop1.wav");break;
	case 2:	pev->noise2 = UTIL_PrecacheSound ("doors/doorstop2.wav");break;
	case 3:	pev->noise2 = UTIL_PrecacheSound ("doors/doorstop3.wav");break;
	case 4:	pev->noise2 = UTIL_PrecacheSound ("doors/doorstop4.wav");break;
	case 5:	pev->noise2 = UTIL_PrecacheSound ("doors/doorstop5.wav");break;
	case 6:	pev->noise2 = UTIL_PrecacheSound ("doors/doorstop6.wav");break;
	case 7:	pev->noise2 = UTIL_PrecacheSound ("doors/doorstop7.wav");break;
	case 8:	pev->noise2 = UTIL_PrecacheSound ("doors/doorstop8.wav");break;	
	case 0:	pev->noise2 = UTIL_PrecacheSound ("common/null.wav"); break;
	default:	pev->noise2 = UTIL_PrecacheSound(m_sounds); break;//custom sound or sentence
	}

	if ( !FStringNull( m_sMaster ))//door has master
	{
		m_sounds = UTIL_LoadSoundPreset( m_iStartSound );
		switch ( m_sounds ) // load locked sounds
		{
		case 1:	pev->noise3 = UTIL_PrecacheSound ("!NA"); break;
		case 2:	pev->noise3 = UTIL_PrecacheSound ("!ND"); break;
		case 3:	pev->noise3 = UTIL_PrecacheSound ("!NF"); break;
		case 4:	pev->noise3 = UTIL_PrecacheSound ("!NFIRE"); break;
		case 5:	pev->noise3 = UTIL_PrecacheSound ("!NCHEM"); break;
		case 6:	pev->noise3 = UTIL_PrecacheSound ("!NRAD"); break;
		case 7:	pev->noise3 = UTIL_PrecacheSound ("!NCON"); break;
		case 8:	pev->noise3 = UTIL_PrecacheSound ("!NH"); break;
		case 9:	pev->noise3 = UTIL_PrecacheSound ("!NG"); break;
		case 0:	pev->noise3 = UTIL_PrecacheSound ("common/null.wav"); break;
		default:	pev->noise3 = UTIL_PrecacheSound(m_sounds); break;//custom sound or sentence
		}
	}
}

void CBaseDoor::DoorTouch( CBaseEntity *pOther )
{
	// make delay before retouching
	if ( gpGlobals->time < pev->dmgtime ) return;
	pev->dmgtime = gpGlobals->time + 1.0f;
	m_hActivator = pOther;// remember who activated the door

	if( pOther->IsPlayer( ))
		DoorUse ( pOther, this, USE_TOGGLE, 1 ); // player always sending 1
}

void CBaseDoor::DoorUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	if ( IsLockedByMaster( useType ))//passed only USE_SHOWINFO
	{
          	EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noise3 ), 1, ATTN_NORM );
		return;
	}
	if ( useType == USE_SHOWINFO ) // show info
	{
		ALERT(at_console, "======/Xash Debug System/======\n");
		ALERT(at_console, "classname: %s\n", STRING(pev->classname));
		ALERT(at_console, "State: %s, Speed %.2f\n", GetStringForState( GetState()), pev->speed );
		ALERT(at_console, "Texture frame: %.f. WaitTime: %.2f\n", pev->frame, m_flWait);
	}
	else if ( m_iState != STATE_DEAD ) // activate door
	{         
		// NOTE: STATE_DEAD is better method for simulate m_flWait -1 without fucking SetThink()
		if ( m_iState == STATE_TURN_ON || m_iState == STATE_TURN_OFF ) return; // door in-moving
		if ( useType == USE_TOGGLE)
		{
			if ( m_iState == STATE_OFF )
				useType = USE_ON;
			else useType = USE_OFF;
		}
		if ( useType == USE_ON )
		{
			if( m_iState == STATE_OFF )
				DoorGoUp();
		}
		else if ( useType == USE_OFF )
		{
			if(m_iState == STATE_ON && pev->impulse) DoorGoDown();
		}
		else if ( useType == USE_SET )
		{
			if ( value )
			{
				m_flWait = value;
				pev->impulse = 0;
			}
		}
		else if ( useType == USE_RESET )
		{
			m_flWait = 0;
			pev->impulse = 1;
		}
	}
}

void CBaseDoor :: ShowInfo ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if(useType == USE_SHOWINFO)//show info
	{
		ALERT(at_console, "======/Xash Debug System/======\n");
		ALERT(at_console, "classname: %s\n", STRING(pev->classname));
		ALERT(at_console, "State: %s, Speed %.2f\n", GetStringForState( GetState()), pev->speed );
		ALERT(at_console, "Texture frame: %.f. WaitTime: %.2f\n", pev->frame, m_flWait);
	}
}

void CBaseDoor::DoorGoUp( void )
{
	// It could be going-down, if blocked.
	ASSERT( m_iState == STATE_OFF || m_iState == STATE_TURN_OFF );
	EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise1), 1, ATTN_NORM);
	SET_AREAPORTAL( edict(), true ); // open areaportal

	m_iState = STATE_TURN_ON;
	SetMoveDone( DoorHitTop );

	UTIL_FireTargets( pev->target, m_hActivator, this, USE_ON );
	if(IsRotatingDoor()) AngularMove(m_vecAngle2, pev->speed);
	else LinearMove(m_vecPosition2, pev->speed);
}

void CBaseDoor::DoorHitTop( void )
{
	STOP_SOUND( ENT( pev ), CHAN_STATIC, STRING( pev->noise1 ));
	EMIT_SOUND( ENT( pev ), CHAN_STATIC, STRING( pev->noise2 ), 1, ATTN_NORM );

	ASSERT( m_iState == STATE_TURN_ON );
	m_iState = STATE_ON;

	if( m_flWait == -1 )
	{
		m_iState = STATE_DEAD; // keep door in this position
		return;
	}

	if( pev->impulse == 0 ) // time base door
	{
		SetThink( DoorGoDown );
		SetNextThink( m_flWait );
	}
	
	// Fire the close target (if startopen is set, then "top" is closed)
	if( pev->spawnflags & SF_START_ON )
		UTIL_FireTargets( pev->target, m_hActivator, this, USE_OFF );
	else 	UTIL_FireTargets( pev->target, m_hActivator, this, USE_ON );
}

void CBaseDoor::DoorGoDown( void )
{
	EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise1), 1, ATTN_NORM);
	
	ASSERT(m_iState == STATE_ON);
	m_iState = STATE_TURN_OFF;
	SetMoveDone( DoorHitBottom );
	if(IsRotatingDoor())AngularMove( m_vecAngle1, pev->speed);
	else LinearMove(m_vecPosition1, pev->speed);
}

void CBaseDoor::DoorHitBottom( void )
{
	STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise1) );
	EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise2), 1, ATTN_NORM);

	ASSERT(m_iState == STATE_TURN_OFF);
	m_iState = STATE_OFF;

	SET_AREAPORTAL( edict(), false );	// close areaportal

	if ( pev->spawnflags & SF_START_ON )
		UTIL_FireTargets( pev->target, m_hActivator, this, USE_ON );
	else UTIL_FireTargets( pev->target, m_hActivator, this, USE_OFF );

	UTIL_FireTargets( pev->target, m_hActivator, this, USE_ON );
}

void CBaseDoor::Blocked( CBaseEntity *pOther )
{
	CBaseEntity	*pTarget	= NULL;
	CBaseDoor		*pDoor	= NULL;

	UTIL_AssignOrigin( this, pev->origin );
	// make delay before retouching
	if( gpGlobals->time < m_flBlockedTime ) return;
	m_flBlockedTime = gpGlobals->time + 0.5;

	if(m_pParent && m_pParent->edict() && pFlags & PF_PARENTMOVE) m_pParent->Blocked( pOther);
	if ( pev->dmg ) pOther->TakeDamage( pev, pev, pev->dmg, DMG_CRUSH );

	if (m_flWait >= 0)
	{
		STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise1) );
		if(IsRotatingDoor())
		{
			if (m_iState == STATE_TURN_ON) DoorGoUp();
			else if (m_iState == STATE_TURN_OFF) DoorGoDown();
	          }
	          else
	          {
			if (m_iState == STATE_TURN_ON) DoorGoDown();
			else if (m_iState == STATE_TURN_OFF) DoorGoUp();
		}
	}
	SetNextThink( 0 );
}
LINK_ENTITY_TO_CLASS( func_door, CBaseDoor );

//=======================================================================
// 		   func_door_rotating - classic rotating door
//=======================================================================
void CRotDoor::Spawn( void )
{
	CBaseDoor::Spawn();
	
	if ( FBitSet (pev->spawnflags, SF_START_ON) )
	{
		pev->angles = m_vecAngle2;
		Vector vecSav = m_vecAngle1;
		m_vecAngle2 = m_vecAngle1;
		m_vecAngle1 = vecSav;
		pev->movedir = pev->movedir * -1;
	}
}

void CRotDoor :: SetToggleState( int state )
{
	if ( state == STATE_ON ) pev->angles = m_vecAngle2;
	else pev->angles = m_vecAngle1;
	UTIL_SetOrigin( this, pev->origin );
}
LINK_ENTITY_TO_CLASS( func_door_rotating, CRotDoor );

//=======================================================================
// 		   func_momentary_door
//=======================================================================
void CMomentaryDoor::Precache( void )
{
	if(IsWater()) return;//no need sounds for water

	CBaseBrush::Precache(); 
	
	int m_sounds = UTIL_LoadSoundPreset(m_iMoveSound);
	switch (m_sounds)//load pushed sounds (sound will play at activate or pushed button)
	{
	case 1:	pev->noise = UTIL_PrecacheSound ("materials/doors/doormove1.wav");break;
	case 2:	pev->noise = UTIL_PrecacheSound ("materials/doors/doormove2.wav");break;
	case 3:	pev->noise = UTIL_PrecacheSound ("materials/doors/doormove3.wav");break;
	case 4:	pev->noise = UTIL_PrecacheSound ("materials/doors/doormove4.wav");break;
	case 5:	pev->noise = UTIL_PrecacheSound ("materials/doors/doormove5.wav");break;
	case 6:	pev->noise = UTIL_PrecacheSound ("materials/doors/doormove6.wav");break;				
	case 0:	pev->noise = UTIL_PrecacheSound ("common/null.wav"); break;
	default:	pev->noise = UTIL_PrecacheSound(m_sounds); break;//custom sound or sentence
	}

	m_sounds = UTIL_LoadSoundPreset(m_iStopSound);
	switch (m_sounds)//load pushed sounds (sound will play at activate or pushed button)
	{
	case 1:	pev->noise2 = UTIL_PrecacheSound ("materials/doors/doorstop1.wav");break;
	case 2:	pev->noise2 = UTIL_PrecacheSound ("materials/doors/doorstop2.wav");break;
	case 3:	pev->noise2 = UTIL_PrecacheSound ("materials/doors/doorstop3.wav");break;
	case 4:	pev->noise2 = UTIL_PrecacheSound ("materials/doors/doorstop4.wav");break;
	case 5:	pev->noise2 = UTIL_PrecacheSound ("materials/doors/doorstop5.wav");break;
	case 6:	pev->noise2 = UTIL_PrecacheSound ("materials/doors/doorstop6.wav");break;
	case 7:	pev->noise2 = UTIL_PrecacheSound ("materials/doors/doorstop7.wav");break;
	case 8:	pev->noise2 = UTIL_PrecacheSound ("materials/doors/doorstop8.wav");break;							
	case 0:	pev->noise2 = UTIL_PrecacheSound ("common/null.wav"); break;
	default:	pev->noise2 = UTIL_PrecacheSound(m_sounds); break;//custom sound or sentence
	}
}

void CMomentaryDoor::Spawn( void )
{
	Precache();
	CBaseBrush::Spawn();
	UTIL_LinearVector( this );//movement direction

	if(pev->spawnflags & SF_NOTSOLID)pev->solid = SOLID_NOT; //make illusionary wall 
	else pev->solid = SOLID_BSP;
 	pev->movetype = MOVETYPE_PUSH;
          
 	m_iState = STATE_OFF;
 	UTIL_SetOrigin(this, pev->origin);
	UTIL_SetModel( ENT(pev), pev->model );
	SetTouch( NULL );//just in case
}

void CMomentaryDoor::PostSpawn( void )
{
	if (m_pParent) m_vecPosition1 = pev->origin - m_pParent->pev->origin;
	else m_vecPosition1 = pev->origin;
	
	// Subtract 2 from size because the engine expands bboxes by 1 in all directions making the size too big
	m_vecPosition2	= m_vecPosition1 + (pev->movedir * (fabs( pev->movedir.x * (pev->size.x-2) ) + fabs( pev->movedir.y * (pev->size.y-2) ) + fabs( pev->movedir.z * (pev->size.z-2) ) - m_flLip));
	ASSERTSZ(m_vecPosition1 != m_vecPosition2, "door start/end positions are equal");

	if(pev->spawnflags & SF_START_ON)
	{
		if (m_pParent)
		{
			m_vecSpawnOffset = m_vecSpawnOffset + (m_vecPosition2 + m_pParent->pev->origin) - pev->origin;
			UTIL_AssignOrigin(this, m_vecPosition2 + m_pParent->pev->origin);
		}
		else
		{
			m_vecSpawnOffset = m_vecSpawnOffset + m_vecPosition2 - pev->origin;
			UTIL_AssignOrigin(this, m_vecPosition2);
		}
		Vector vecTemp = m_vecPosition2;
		m_vecPosition2 = m_vecPosition1;
		m_vecPosition1 = vecTemp;
	}
}

void CMomentaryDoor::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (useType == USE_ON) value = 1;
	else if(useType == USE_OFF) value = 0;
	else if(useType == USE_SET);
	else if(useType == USE_SHOWINFO)//show info
	{
		Msg("======/Xash Debug System/======\n");
		Msg("classname: %s\n", STRING(pev->classname));
		Msg("State: %s, Lip %.2f\n", GetStringForState( GetState()), m_flLip );
		SHIFT;	
	}
	else return;
	
	if ( value > 1.0 )value = 1.0;

	if (IsLockedByMaster()) return;
	Vector move = m_vecPosition1 + (value * (m_vecPosition2 - m_vecPosition1));

	float speed = 0;
	Vector delta;

	if (pev->speed) speed = pev->speed;
	else
	{
		// default: get there in 0.1 secs
		delta = move - pev->origin;
          	speed = delta.Length() * 10;
          }
          
	//FIXME: allow for it being told to move at the same speed in the _opposite_ direction!
	if ( speed != 0 )
	{
		// This entity only thinks when it moves
		if ( m_iState == STATE_OFF )
		{
			//ALERT(at_console,"USE: start moving to %f %f %f.\n", move.x, move.y, move.z);
			m_iState = STATE_ON;
			EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise), 1, ATTN_NORM);
		}

		LinearMove( move, speed );
		SetMoveDone( MomentaryMoveDone );
	}
}

void CMomentaryDoor::MomentaryMoveDone( void )
{
	m_iState = STATE_OFF;
	STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise));
	EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise2), 1, ATTN_NORM);
}
LINK_ENTITY_TO_CLASS( func_momentary_door, CMomentaryDoor );

//=======================================================================
// 		   func_platform (a lift\elevator)
//=======================================================================
void CBasePlatform::Precache( void )
{
	CBaseBrush::Precache();//precache damage sound

	int m_sounds = UTIL_LoadSoundPreset(m_iMoveSound);
	switch (m_sounds)//load movesound sounds (sound will play when door is moving)
	{
	case 1:	pev->noise = UTIL_PrecacheSound ("plats/bigmove1.wav");break;
	case 2:	pev->noise = UTIL_PrecacheSound ("plats/bigmove2.wav");break;
	case 3:	pev->noise = UTIL_PrecacheSound ("plats/elevmove1.wav");break;
	case 4:	pev->noise = UTIL_PrecacheSound ("plats/elevmove2.wav");break;
	case 5:	pev->noise = UTIL_PrecacheSound ("plats/elevmove3.wav");break;
	case 6:	pev->noise = UTIL_PrecacheSound ("plats/freightmove1.wav");break;
	case 7:	pev->noise = UTIL_PrecacheSound ("plats/freightmove2.wav");break;
	case 8:	pev->noise = UTIL_PrecacheSound ("plats/heavymove1.wav");break;
	case 9:	pev->noise = UTIL_PrecacheSound ("plats/rackmove1.wav");break;
	case 10:	pev->noise = UTIL_PrecacheSound ("plats/railmove1.wav");break;			
	case 11:	pev->noise = UTIL_PrecacheSound ("plats/squeekmove1.wav");break;
	case 12:	pev->noise = UTIL_PrecacheSound ("plats/talkmove1.wav");break;
	case 13:	pev->noise = UTIL_PrecacheSound ("plats/talkmove2.wav");break;		
	case 0:	pev->noise = UTIL_PrecacheSound ("common/null.wav"); break;
	default:	pev->noise = UTIL_PrecacheSound(m_sounds); break;//custom sound or sentence
	}

	m_sounds = UTIL_LoadSoundPreset(m_iStopSound);
	switch (m_sounds)//load pushed sounds (sound will play at activate or pushed button)
	{
	case 1:	pev->noise1 = UTIL_PrecacheSound ("plats/bigstop1.wav");break;
	case 2:	pev->noise1 = UTIL_PrecacheSound ("plats/bigstop2.wav");break;
	case 3:	pev->noise1 = UTIL_PrecacheSound ("plats/freightstop1.wav");break;
	case 4:	pev->noise1 = UTIL_PrecacheSound ("plats/heavystop2.wav");break;
	case 5:	pev->noise1 = UTIL_PrecacheSound ("plats/rackstop1.wav");break;
	case 6:	pev->noise1 = UTIL_PrecacheSound ("plats/railstop1.wav");break;
	case 7:	pev->noise1 = UTIL_PrecacheSound ("plats/squeekstop1.wav");break;
	case 8:	pev->noise1 = UTIL_PrecacheSound ("plats/talkstop1.wav");break;	
	case 0:	pev->noise1 = UTIL_PrecacheSound ("common/null.wav"); break;
	default:	pev->noise1 = UTIL_PrecacheSound(m_sounds); break;//custom sound or sentence
	}

	UTIL_PrecacheSound( "buttons/button11.wav" );//error sound
}

void CBasePlatform :: Setup( void )
{
 	Precache();	//precache moving & stop sounds
         
	pev->angles = g_vecZero;

	if (IsWater()) pev->solid = SOLID_NOT; // special contents (water, slime, e.t.c. )
          else pev->solid = SOLID_BSP;

	pev->movetype = MOVETYPE_PUSH;
	UTIL_SetOrigin(this, pev->origin);
	UTIL_SetSize(pev, pev->mins, pev->maxs);
	UTIL_SetModel(ENT(pev), pev->model );
	// vecPosition1 is the top position, vecPosition2 is the bottom
	if (m_pParent) m_vecPosition1 = pev->origin - m_pParent->pev->origin;
	else m_vecPosition1 = pev->origin;
	m_vecPosition2 = m_vecPosition1;

	if(IsMovingPlatform() || IsComplexPlatform())
	{
		m_vecPosition2.z = m_vecPosition2.z + step();
          	ASSERTSZ(m_vecPosition1 != m_vecPosition2, "moving platform start/end positions are equal\n");
          }
	if(IsRotatingPlatform() || IsComplexPlatform())
	{
		if ( m_flMoveDistance < 0 ) pev->movedir = pev->movedir * -1;

		AxisDir();
		m_vecAngle1 = pev->angles;
		m_vecAngle2 = pev->angles + pev->movedir * m_flMoveDistance;
		
		ASSERTSZ(m_vecAngle1 != m_vecAngle2, "rotating platform start/end positions are equal\n");
		SetBits (pFlags, PF_ANGULAR);
	}
	if (!IsWater())CBaseBrush::Spawn();
          
	if (pev->speed == 0) pev->speed = 150;
	m_iState = STATE_OFF;

}

void CBasePlatform :: PostSpawn( void )
{
	if ( FBitSet( pev->spawnflags, SF_START_ON ) )          
	{
		if (m_pParent) UTIL_AssignOrigin (this, m_vecPosition2 + m_pParent->pev->origin);
		else UTIL_AssignOrigin (this, m_vecPosition2);
		UTIL_AssignAngles(this, m_vecAngle2);
		m_iState = STATE_ON;
	}
	else
	{
		if (m_pParent) UTIL_AssignOrigin (this, m_vecPosition1 + m_pParent->pev->origin);
		else UTIL_AssignOrigin (this, m_vecPosition1);
		UTIL_AssignAngles(this, m_vecAngle1);
		m_iState = STATE_OFF;
	}
}

void CBasePlatform :: Spawn( void )
{
	Setup();
}

void CBasePlatform :: PostActivate( void )
{
	if(m_iState == STATE_TURN_OFF || m_iState == STATE_TURN_ON)//platform "in-moving" ? restore sound!
	{
		EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise), m_flVolume, ATTN_NORM);
	}
}

void CBasePlatform :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	
	if (useType == USE_TOGGLE)
	{
		if(m_iState == STATE_ON) useType = USE_OFF;
		else useType = USE_ON;
	}
	if (useType == USE_ON)
	{
		GoUp();	
	}
	else if ( useType == USE_OFF )
	{
		GoDown();	
	}
	else if ( useType == USE_SET )
	{
		GoToFloor( value );	
	}
	else if ( useType == USE_RESET )
	{
		GoToFloor( 1 );	
	}
	else if (useType == USE_SHOWINFO)
	{
		ALERT( at_console, "======/Xash Debug System/======\n");
		ALERT( at_console, "classname: %s\n", STRING(pev->classname));
		if( IsWater( )) ALERT( at_console, "Contents: %s, WaveHeight %g\n", GetContentsString( pev->skin ), pev->scale );
		else ALERT( at_console, "State: %s, floor %g\n", GetStringForState( GetState( )), CalcFloor( ));
		ALERT( at_console, "distance %g, speed %g\n", m_flMoveDistance, pev->speed );

	}
}

void CBasePlatform :: GoToFloor( float floor )
{
	float curfloor = CalcFloor();
          m_flValue = floor;

	if(curfloor <= 0) return;
	if(curfloor == floor) //already there?
	{
		//pass trough
		UTIL_FireTargets( pev->target, m_hActivator, this, USE_RESET, m_flValue );
		return;
	}

          m_vecFloor = m_vecPosition1;
          m_vecFloor.z = pev->origin.z + (floor * step()) - (curfloor * step());
	
	EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise), m_flVolume, ATTN_NORM);
        
	if(floor > curfloor)m_iState = STATE_TURN_ON;
	else m_iState = STATE_TURN_OFF;
	
	SetMoveDone( HitFloor );
	
	if(fabs(floor - curfloor) > 1.0) //create floor informator for prop_counter
	{
		CBaseEntity *pFloor = CBaseEntity::Create( "floorent", pev->origin, g_vecZero, edict() );
          	pFloor->pev->target = pev->netname;
		pFloor->PostActivate();
          }
	LinearMove(m_vecFloor, pev->speed);
}

void CBasePlatform :: HitFloor( void )
{
	STOP_SOUND(ENT(pev), CHAN_STATIC, STRING( pev->noise ));
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, STRING( pev->noise1 ), m_flVolume, ATTN_NORM );

	ASSERT( m_iState == STATE_TURN_ON || m_iState == STATE_TURN_OFF );
	UTIL_FireTargets( pev->target, m_hActivator, this, USE_TOGGLE, m_flValue );
	UTIL_FireTargets( pev->netname, m_hActivator, this, USE_SET, m_flValue );
	m_vecPosition2 = pev->origin; // save current floor
	if( m_iState == STATE_TURN_ON ) m_iState = STATE_ON;
	if( m_iState == STATE_TURN_OFF ) m_iState = STATE_OFF;
}

void CBasePlatform :: GoDown( void )
{
	EMIT_SOUND( ENT( pev ), CHAN_STATIC, STRING( pev->noise ), m_flVolume, ATTN_NORM );

	ASSERT( m_iState == STATE_ON || m_iState == STATE_TURN_ON );
	m_iState = STATE_TURN_OFF;
	SetMoveDone( HitBottom );

	if ( IsRotatingPlatform( )) AngularMove( m_vecAngle1, pev->speed );
	else if ( IsMovingPlatform( )) LinearMove( m_vecPosition1, pev->speed );
	else if ( IsComplexPlatform( )) ComplexMove( m_vecPosition1, m_vecAngle1, pev->speed );
	else HitBottom(); // don't brake platform status
}

void CBasePlatform :: HitBottom( void )
{
	STOP_SOUND(ENT(pev), CHAN_STATIC, STRING( pev->noise ));
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, STRING( pev->noise1 ), m_flVolume, ATTN_NORM );

	ASSERT( m_iState == STATE_TURN_OFF );
	UTIL_FireTargets( pev->netname, m_hActivator, this, USE_SET, 1 );
	m_iState = STATE_OFF;
}

void CBasePlatform :: GoUp( void )
{
	EMIT_SOUND( ENT( pev ), CHAN_STATIC, STRING( pev->noise ), m_flVolume, ATTN_NORM );
	
	ASSERT( m_iState == STATE_OFF || m_iState == STATE_TURN_OFF );
	m_iState = STATE_TURN_ON;
	SetMoveDone( HitTop );

	if ( IsRotatingPlatform( )) AngularMove( m_vecAngle2, pev->speed );
	else if ( IsMovingPlatform( )) LinearMove( m_vecPosition2, pev->speed );
	else if ( IsComplexPlatform( )) ComplexMove( m_vecPosition2, m_vecAngle2, pev->speed );
	else HitTop(); // don't brake platform status
}

void CBasePlatform :: HitTop( void )
{
	STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise));
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, (char*)STRING(pev->noise1), m_flVolume, ATTN_NORM);
	
	ASSERT(m_iState == STATE_TURN_ON);
	UTIL_FireTargets( pev->netname, m_hActivator, this, USE_SET, 2 );
	m_iState = STATE_ON;
}

void CBasePlatform :: Blocked( CBaseEntity *pOther )
{
	UTIL_AssignOrigin(this, pev->origin);
	//make delay before retouching
	if ( gpGlobals->time < m_flBlockedTime) return;
	m_flBlockedTime = gpGlobals->time + 0.5;
	
	if(m_pParent && m_pParent->edict() && pFlags & PF_PARENTMOVE) m_pParent->Blocked( pOther);
	pOther->TakeDamage( pev, pev, 1, DMG_CRUSH );
	STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise));
	ASSERT(m_iState == STATE_TURN_ON || m_iState == STATE_TURN_OFF);

	if ( m_iState == STATE_TURN_ON ) GoDown();
	else if ( m_iState == STATE_TURN_OFF ) GoUp();
	SetNextThink( 0 );
}

LINK_ENTITY_TO_CLASS( func_water, CBasePlatform );
LINK_ENTITY_TO_CLASS( func_platform, CBasePlatform );
LINK_ENTITY_TO_CLASS( func_platform_rotating, CBasePlatform );

//=======================================================================
// 		   func_train (Classic QUAKE Train)
//=======================================================================
TYPEDESCRIPTION	CFuncTrain::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncTrain, pPath, FIELD_CLASSPTR ),
	DEFINE_FIELD( CFuncTrain, pNextPath, FIELD_CLASSPTR ),
};
IMPLEMENT_SAVERESTORE( CFuncTrain, CBasePlatform );

void CFuncTrain :: Spawn( void )
{
	Precache(); // precache moving & stop sounds

	if(pev->spawnflags & SF_NOTSOLID)//make illusionary train 
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
	}
	else
	{
		pev->solid = SOLID_BSP;
          	pev->movetype = MOVETYPE_PUSH;
          }

	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetModel( ENT(pev), pev->model );
	CBaseBrush::Spawn();

	// determine method for calculating origin
	if( pev->origin != g_vecZero ) pev->impulse = 1;
          
	if (pev->speed == 0) pev->speed = 100;
	m_iState = STATE_OFF;
}

void CFuncTrain :: PostSpawn( void )
{
	if (!FindPath()) return;

	if ( pev->impulse )
	{
		m_vecSpawnOffset = m_vecSpawnOffset + pPath->pev->origin - pev->origin;
		if (m_pParent) UTIL_AssignOrigin (this, pPath->pev->origin - m_pParent->pev->origin );
		else UTIL_AssignOrigin (this, pPath->pev->origin );
	}
	else
	{
		m_vecSpawnOffset = m_vecSpawnOffset + (pPath->pev->origin - TrainOrg()) - pev->origin;
		if ( m_pParent ) UTIL_AssignOrigin (this, pPath->pev->origin - TrainOrg() - m_pParent->pev->origin );
		else UTIL_AssignOrigin (this, pPath->pev->origin - TrainOrg());
	}
}

void CFuncTrain :: PostActivate( void )
{
	if ( m_iState == STATE_ON ) // platform "in-moving" ? restore sound!
	{
		EMIT_SOUND (ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise), m_flVolume, ATTN_NORM);
	}
	if ( pev->spawnflags & SF_START_ON )
	{	
		m_iState = STATE_OFF; // restore sound on a next level
		SetThink( Next );	// evil stuff...
		SetNextThink( 0.1 );
		ClearBits( pev->spawnflags, SF_START_ON );//fire once
	}
}

BOOL CFuncTrain::FindPath( void )
{
	// find start track
	pPath = UTIL_FindEntityByTargetname (NULL, STRING( pev->target ));
	if( pPath && pPath->edict( ))
		return TRUE;
	return FALSE;
}

BOOL CFuncTrain::FindNextPath( void )
{
	if( !pPath )
	{
		ALERT( at_error, "CFuncTrain::FindNextpath failed\n" );
		return FALSE;
	}

	// get pointer to next target
	if( pev->speed > 0 ) pNextPath = ((CInfoPath *)pPath)->GetNext();
	if( pev->speed < 0 ) pNextPath = ((CInfoPath *)pPath)->GetPrev();
	
	if( pNextPath && pNextPath->edict( )) // validate path
	{
		// record new value (this will be used after changelevel)
		pev->target = pNextPath->pev->targetname; 
		return TRUE; // path found
	}
	switch ( m_iMode )
	{
	case 1: UpdateSpeed(); break;
	case 2: UpdateSpeed();
	default: Stop(); break; 
	}
	return FALSE;
}

void CFuncTrain::UpdateSpeed( float value )
{
	// update path if dir changed
	if(( value > 0 && pev->speed < 0 ) || ( value < 0 && pev->speed > 0 ) || value == 0 )
	{
		if( pNextPath && pNextPath->edict( ))
			pPath = pNextPath;
	}

	if( value != 0 ) pev->speed = value; // get new speed
	else pev->speed = -pev->speed;

	if( m_iState == STATE_ON ) Next(); // re-calculate speed now!
}

void CFuncTrain::ClearPointers( void )
{
	CBaseEntity::ClearPointers();
	pPath = NULL;
	pNextPath = NULL;
}

void CFuncTrain::OverrideReset( void )
{
	// Are we moving?
	if ( m_iState == STATE_ON )
	{
		if ( FindPath( )) SetBits( pev->spawnflags, SF_START_ON ); // PostActivate member
		else Stop();
	}
}

void CFuncTrain :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if ( useType == USE_TOGGLE )
	{
		if( m_iState == STATE_ON )
			useType = USE_OFF;
		else useType = USE_ON;
	}
	if ( useType == USE_ON ) Next();
	else if ( useType == USE_OFF ) Stop();
	else if ( useType == USE_SET )
	{
		UpdateSpeed( value ); // set new speed
	}
	else if ( useType == USE_RESET )
	{
		UTIL_FireTargets( pev->netname, pActivator, pCaller, USE_TOGGLE ); // just fire
	}
	else if ( useType == USE_SHOWINFO )
	{
		DEBUGHEAD;
		ALERT( at_console, "State: %s, speed %g\n", GetStringForState( GetState( )), pev->speed );
		if( GetPrev() && GetPrev()->edict( )) ALERT( at_console, "Prev path %s", STRING( GetPrev()->pev->targetname ));
		if( GetNext() && GetNext()->edict( )) ALERT( at_console, "Next path %s", STRING( GetNext()->pev->targetname ));
		ALERT( at_console, "\n" );
	}
}

void CFuncTrain :: Next( void )
{
	if( !FindNextPath( )) return;

	// linear move to next corner.
	if ( m_iState == STATE_OFF ) // enable train sound
	{
		STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ));
		EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ), m_flVolume, ATTN_NORM );
	}

	ClearBits( pev->effects, EF_NOINTERP ); // enable interpolation
	m_iState = STATE_ON;

	if( pev->speed < 0 && FBitSet( pNextPath->pev->spawnflags, SF_CORNER_TELEPORT ))
	{
		SetBits( pev->effects, EF_NOINTERP );
		if ( m_pParent ) UTIL_AssignOrigin( this, pNextPath->pev->origin - m_pParent->pev->origin );
		else UTIL_AssignOrigin( this, pNextPath->pev->origin );
		Wait(); // Get on with doing the next path corner.
		return;
	}

	if ( m_pParent )
	{
		if ( pev->impulse ) LinearMove( pNextPath->pev->origin - m_pParent->pev->origin, fabs( pev->speed ));
		else  LinearMove ( pNextPath->pev->origin - TrainOrg() - m_pParent->pev->origin, fabs( pev->speed ));
	}
	else
	{
		if ( pev->impulse ) LinearMove( pNextPath->pev->origin, fabs( pev->speed ));
		else  LinearMove ( pNextPath->pev->origin - TrainOrg(), fabs( pev->speed ));
	}
	SetMoveDone( Wait );

}

BOOL CFuncTrain :: Teleport( void )
{
	if( !FindNextPath( ))
		return FALSE;

	if( FBitSet( pNextPath->pev->spawnflags, SF_CORNER_TELEPORT ))
	{
		SetBits( pev->effects, EF_NOINTERP );

		// determine teleportation point
		if( pev->speed > 0 ) 
		{
			pNextPath = pNextPath->GetNext();
			UpdateTargets();
                    }

		if( !pNextPath || !pNextPath->edict() )
			return FALSE; // dead end

		if ( m_pParent )
			UTIL_AssignOrigin(this, pNextPath->pev->origin - m_pParent->pev->origin );
		else UTIL_AssignOrigin(this, pNextPath->pev->origin );
	
		pPath = pNextPath;
		Next(); // Get on with doing the next path corner.
		return TRUE;
	}
	return FALSE;
}

void CFuncTrain :: Wait( void )
{
	UpdateTargets();
	if(Teleport( )) return;
	
	if( pNextPath )
	{	
		pPath = pNextPath;//move pointer
		((CInfoPath *)pPath)->GetSpeed( &pev->speed );
		if(!Stop(((CInfoPath *)pPath)->GetDelay())) Next(); // go to next corner
	}
	else Stop();
}

void CFuncTrain :: UpdateTargets( void )
{
	// fire the pass target if there is one
	if( !pNextPath || !pNextPath->edict() ) return;

	UTIL_FireTargets( pNextPath->pev->message, this, this, USE_TOGGLE );
	if ( FBitSet( pNextPath->pev->spawnflags, SF_CORNER_FIREONCE ))
		pNextPath->pev->message = iStringNull;
	UTIL_FireTargets( pev->netname, this, this, USE_TOGGLE );
}

BOOL CFuncTrain :: Stop( float flWait )
{
	if( flWait == 0 ) return FALSE;
	m_iState = STATE_OFF;

	if( pPath && pPath->edict() )
	{
		UTIL_FireTargets( pPath->pev->message, this, this, USE_TOGGLE );
		if ( FBitSet( pPath->pev->spawnflags, SF_CORNER_FIREONCE ))
			pPath->pev->message = iStringNull;
		UTIL_FireTargets( pev->netname, this, this, USE_TOGGLE );
	} 
 
 	// clear the sound channel.
	STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ));
	EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noise1 ), m_flVolume, ATTN_NORM );

	UTIL_SetVelocity( this, g_vecZero );
	UTIL_SetAvelocity( this, g_vecZero );

	if( flWait > 0 )
	{
		SetNextThink( flWait );
		SetThink( Next );		
	}
	else if( flWait == -1 ) DontThink(); // wait for retrigger
	return TRUE;
}

void CFuncTrain :: Blocked( CBaseEntity *pOther )
{
	// Keep "movewith" entities in line
	UTIL_AssignOrigin(this, pev->origin);

	if ( gpGlobals->time < m_flBlockedTime) return;
	m_flBlockedTime = gpGlobals->time + 0.5;

	if( m_pParent && m_pParent->edict() && pFlags & PF_PARENTMOVE )
		m_pParent->Blocked( pOther );
	pOther->TakeDamage( pev, pev, 1, DMG_CRUSH );
	STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ));
	ASSERT(m_iState == STATE_ON);

	Stop( 0.5 );
}
LINK_ENTITY_TO_CLASS( func_train, CFuncTrain );