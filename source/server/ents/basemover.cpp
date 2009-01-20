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
	if( pev->movedir != g_vecZero ) return;
	
	// don't change this!
	if( FBitSet( pev->spawnflags, SF_DOOR_ROTATE_X ))
		pev->movedir = Vector ( 1, 0, 0 );	// around z-axis
	else if( FBitSet( pev->spawnflags, SF_DOOR_ROTATE_Z ))
		pev->movedir = Vector ( 0, 0, 1 );	// around x-axis
	else pev->movedir = Vector ( 0, 1, 0 );	// around y-axis
}

void CBaseMover::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq(pkvd->szKeyName, "lip" ))
	{
		m_flLip = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if( FStrEq( pkvd->szKeyName, "waveheight" ))
	{
		// field for volume_water
		pev->scale = atof(pkvd->szValue) * (1.0/8.0);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "type"))
	{
		m_iMode = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "locked_sound"))
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
	else if (FStrEq(pkvd->szKeyName, "distance") || FStrEq(pkvd->szKeyName, "rotation"))
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
		//func_water wave height
		pev->scale = atof(pkvd->szValue) * (1.0/8.0);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "contents"))
	{
		//func_water contents
		pev->skin = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBaseBrush::KeyValue( pkvd );
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
	SetNextThink( flTravelTime );//, TRUE );
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
	SetNextThink( flTravelTime );//, TRUE );
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
void CBaseDoor::Spawn( )
{
	Precache();
          if(!IsRotatingDoor())UTIL_LinearVector( this );
	CBaseBrush::Spawn();

	//make illusionary door 
	if(pev->spawnflags & SF_DOOR_PASSABLE || IsWater())
		pev->solid = SOLID_NOT;
	else pev->solid = SOLID_BSP;
          
          pev->movetype = MOVETYPE_PUSH;
        
	if(IsRotatingDoor())
	{
		// check for clockwise rotation
		AxisDir();

		if( FBitSet( pev->spawnflags, SF_DOOR_ROTATE_BACKWARDS ))
			pev->movedir = pev->movedir * -1;
		m_vecAngle1 = pev->angles;
		m_vecAngle2 = pev->angles + pev->movedir * m_flMoveDistance;

		ASSERTSZ(m_vecAngle1 != m_vecAngle2, "rotating door start/end positions are equal");
          	SetBits (pFlags, PF_ANGULAR);
          }
          
	UTIL_SetModel( ENT(pev), pev->model );
	UTIL_SetOrigin(this, pev->origin);

	//determine work style
	if ( FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY )) SetTouch ( NULL );
	else SetTouch ( DoorTouch );

          //if waittime is -1 - button forever stay pressed
	if (FBitSet(pev->spawnflags, SF_DOOR_NO_AUTO_RETURN)) pev->impulse = 1;//toggleable door
	if (m_flLip == 0) m_flLip = 4;//standart offset from Quake1
	if (pev->speed == 0) pev->speed = 100;//default speed
	m_iState = STATE_OFF;

	if ( IsRotatingDoor() && FBitSet (pev->spawnflags, SF_DOOR_START_OPEN))
	{
		pev->angles = m_vecAngle2;
		Vector vecSav = m_vecAngle1;
		m_vecAngle2 = m_vecAngle1;
		m_vecAngle1 = vecSav;
		pev->movedir = pev->movedir * -1;
	}
}

void CBaseDoor :: PostSpawn( void )
{
	if (m_pParent) m_vecPosition1 = pev->origin - m_pParent->pev->origin;
	else m_vecPosition1 = pev->origin;

	// Subtract 2 from size because the engine expands bboxes by 1 in all directions
	m_vecPosition2 = m_vecPosition1 + (pev->movedir * (fabs( pev->movedir.x * (pev->size.x-2) ) + fabs( pev->movedir.y * (pev->size.y-2) ) + fabs( pev->movedir.z * (pev->size.z-2) ) - m_flLip));

	ASSERTSZ(m_vecPosition1 != m_vecPosition2, "door start/end positions are equal");
	if ( FBitSet (pev->spawnflags, SF_DOOR_START_OPEN) )
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

void CBaseDoor::Precache( void )
{
	CBaseBrush::Precache();//precache damage sound

	int m_sounds = UTIL_LoadSoundPreset(m_iMoveSound);
	if(IsWater()) m_sounds = 0;
	switch (m_sounds)//load movesound sounds (sound will play when door is moving)
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
	default:	pev->noise1 = UTIL_PrecacheSound(m_sounds); break;//custom sound or sentence
	}

	m_sounds = UTIL_LoadSoundPreset(m_iStopSound);
	if(IsWater()) m_sounds = 0;
	switch (m_sounds)//load pushed sounds (sound will play at activate or pushed button)
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

	m_sounds = UTIL_LoadSoundPreset(m_iStartSound);
	if(IsWater()) m_sounds = 0;
	switch (m_sounds)//load locked sounds
	{
	case 1:	pev->noise3 = UTIL_PrecacheSound ("buttons/button1.wav");	break;
	case 2:	pev->noise3 = UTIL_PrecacheSound ("buttons/button2.wav");	break;
	case 3:	pev->noise3 = UTIL_PrecacheSound ("buttons/button3.wav");	break;
	case 4:	pev->noise3 = UTIL_PrecacheSound ("buttons/button4.wav");	break;
	case 5:	pev->noise3 = UTIL_PrecacheSound ("buttons/button5.wav");	break;
	case 6:	pev->noise3 = UTIL_PrecacheSound ("buttons/button6.wav");	break;
	case 7:	pev->noise3 = UTIL_PrecacheSound ("buttons/button7.wav");	break;
	case 8:	pev->noise3 = UTIL_PrecacheSound ("buttons/button8.wav");	break;
	case 9:	pev->noise3 = UTIL_PrecacheSound ("buttons/button9.wav");	break;
	case 10:	pev->noise3 = UTIL_PrecacheSound ("buttons/button10.wav");	break;
	case 11:	pev->noise3 = UTIL_PrecacheSound ("buttons/button11.wav");	break;
	case 12:	pev->noise3 = UTIL_PrecacheSound ("buttons/latchlocked1.wav");	break;
	case 13:	pev->noise3 = UTIL_PrecacheSound ("buttons/latchunlocked1.wav");	break;
	case 14:	pev->noise3 = UTIL_PrecacheSound ("buttons/buttons/lightswitch2.wav");	break;
	case 0:	pev->noise3 = UTIL_PrecacheSound ("common/null.wav"); break;
	default:	pev->noise3 = UTIL_PrecacheSound(m_sounds); break;//custom sound or sentence
	}
}

void CBaseDoor::DoorTouch( CBaseEntity *pOther )
{
	//make delay before retouching
	if ( gpGlobals->time < pev->dmgtime) return;
	pev->dmgtime = gpGlobals->time + 1.0;
	//m_hActivator = pOther;// remember who activated the door

	if (!FStringNull(pev->targetname))
	{
		// play locked sound
		EMIT_SOUND(ENT(pev), CHAN_VOICE, (char*)STRING(pev->noise3), m_flVolume, ATTN_NORM);
		return; 
	}

	if( pOther->IsPlayer( )) Use( pOther, this, USE_TOGGLE, 1 ); // player always sending 1
}

void CBaseDoor::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	if(IsLockedByMaster( useType ))//passed only USE_SHOWINFO
	{
          	EMIT_SOUND(ENT(pev), CHAN_VOICE, (char*)STRING(pev->noise3), m_flVolume, ATTN_NORM);
		return;
	}
	if(useType == USE_SHOWINFO)//show info
	{
		DEBUGHEAD;
		Msg( "State: %s, Speed %g\n", GetStringForState( GetState()), pev->speed );
		Msg( "Texture frame: %g. WaitTime: %g\n", pev->frame, m_flWait);
	}
	else if(m_iState != STATE_DEAD)//activate door
	{         
		//NOTE: STATE_DEAD is better method for simulate m_flWait -1 without fucking SetThink()
		if (m_iState == STATE_TURN_ON || m_iState == STATE_TURN_OFF )return;//door in-moving
		if (useType == USE_TOGGLE)
		{
			if(m_iState == STATE_OFF) useType = USE_ON;
			else useType = USE_OFF;
		}
		if(useType == USE_ON)
		{
			if( m_iState == STATE_OFF )DoorGoUp();
		}
		else if( useType == USE_OFF )
		{
			if(m_iState == STATE_ON && pev->impulse) DoorGoDown();
		}
		else if( useType == USE_SET )pev->frame = !pev->frame; //change texture
	}
}

void CBaseDoor::DoorGoUp( void )
{
	// It could be going-down, if blocked.
	ASSERT(m_iState == STATE_OFF || m_iState == STATE_TURN_OFF);
	EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise1), m_flVolume, ATTN_NORM);
	m_iState = STATE_TURN_ON;
	SetMoveDone( DoorHitTop );
          SetTouch( NULL );
	
	UTIL_FireTargets( pev->target, m_hActivator, this, USE_ON );
	if(IsRotatingDoor()) 
	{
		int sign = 1;
		if ( m_hActivator != NULL && !FBitSet( pev->spawnflags, SF_DOOR_ONEWAY ) && pev->movedir.y)
		{
			// Y axis rotation, move away from the player
			Vector vec = m_hActivator->pev->origin - pev->origin;
			UTIL_MakeVectors ( m_hActivator->pev->angles );
			Vector vnext = (m_hActivator->pev->origin + (gpGlobals->v_forward * 10)) - pev->origin;
			if ( (vec.x * vnext.y - vec.y * vnext.x) < 0 ) sign = -1;
		}
		AngularMove(m_vecAngle2 * sign, pev->speed);
	}
	else LinearMove(m_vecPosition2, pev->speed);
}

void CBaseDoor::DoorHitTop( void )
{
	STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise1) );
	EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise2), m_flVolume, ATTN_NORM);

	ASSERT(m_iState == STATE_TURN_ON);
	m_iState = STATE_ON;
          
	if(pev->impulse == 0)//time base door
	{
		if(m_flWait == -1) m_iState = STATE_DEAD;//keep door in this position
		else 
		{
			SetThink( DoorGoDown );
			SetNextThink( m_flWait );
		}
	}
	else if ( !FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY )) SetTouch( DoorTouch );
	
	// Fire the close target (if startopen is set, then "top" is closed)
	if (pev->spawnflags & SF_DOOR_START_OPEN)
		UTIL_FireTargets( pev->netname, m_hActivator, this, USE_TOGGLE );
	UTIL_FireTargets( pev->target, m_hActivator, this, USE_TOGGLE );
}

void CBaseDoor::DoorGoDown( void )
{
	EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise1), m_flVolume, ATTN_NORM);
	
	ASSERT(m_iState == STATE_ON);
	m_iState = STATE_TURN_OFF;
	SetMoveDone( DoorHitBottom );
	if(IsRotatingDoor())AngularMove( m_vecAngle1, pev->speed);
	else LinearMove(m_vecPosition1, pev->speed);
}

void CBaseDoor::DoorHitBottom( void )
{
	STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise1) );
	EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise2), m_flVolume, ATTN_NORM);

	ASSERT(m_iState == STATE_TURN_OFF);
	m_iState = STATE_OFF;

	if ( FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY )) SetTouch ( NULL );
	else SetTouch( DoorTouch );

	if (!(pev->spawnflags & SF_DOOR_START_OPEN))
		UTIL_FireTargets( pev->netname, m_hActivator, this, USE_TOGGLE );
	UTIL_FireTargets( pev->target, m_hActivator, this, USE_TOGGLE );
}

void CBaseDoor::Blocked( CBaseEntity *pOther )
{
	UTIL_AssignOrigin( this, pev->origin );
	// make delay before retouching
	if( gpGlobals->time < m_flBlockedTime ) return;
	m_flBlockedTime = gpGlobals->time + 0.5;

	if( m_pParent && m_pParent->edict() && pFlags & PF_PARENTMOVE ) m_pParent->Blocked( pOther );
	if( pev->dmg ) pOther->TakeDamage( pev, pev, pev->dmg, DMG_CRUSH );

	if( m_flWait >= 0 )
	{
		STOP_SOUND( ENT( pev ), CHAN_STATIC, (char*)STRING( pev->noise1 ));
		if( m_iState == STATE_TURN_ON ) DoorGoDown();
		else if( m_iState == STATE_TURN_OFF ) DoorGoUp();
	}
          
	// what the hell does this ?
	// UTIL_SynchDoors( this );
	SetNextThink( 0 );
}
LINK_ENTITY_TO_CLASS( func_door, CBaseDoor );
LINK_ENTITY_TO_CLASS( func_water, CBaseDoor );
LINK_ENTITY_TO_CLASS( func_door_rotating, CRotDoor );

//=======================================================================
// 		   func_momentary_door
//=======================================================================
void CMomentaryDoor::Precache( void )
{
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
			EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise), m_flVolume, ATTN_NORM);
		}

		LinearMove( move, speed );
		SetMoveDone( MomentaryMoveDone );
	}
}

void CMomentaryDoor::MomentaryMoveDone( void )
{
	m_iState = STATE_OFF;
	STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise));
	EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise2), m_flVolume, ATTN_NORM);
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

	pev->solid = SOLID_BSP;

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
          	//m_vecPosition2.z = pev->origin.z - m_flHeight;
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
	CBaseBrush::Spawn();
          
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
	if (useType == USE_ON) GoUp();	
	else if ( useType == USE_OFF ) GoDown();	
	else if ( useType == USE_SET ) GoToFloor( value );
	else if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		Msg( "State: %s, floor %g\n", GetStringForState( GetState()), CalcFloor());
		Msg( "distance %g, speed %g\n", m_flMoveDistance, pev->speed);
	}
}

void CBasePlatform :: GoToFloor( float floor )
{
	float curfloor = CalcFloor();
          m_flValue = floor;

	if(curfloor <= 0) return;
	if(curfloor == floor) return; //already there?

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
	STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise));
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, (char*)STRING(pev->noise1), m_flVolume, ATTN_NORM);

	ASSERT(m_iState == STATE_TURN_ON || m_iState == STATE_TURN_OFF);
	UTIL_FireTargets( pev->target, m_hActivator, this, USE_TOGGLE, m_flValue );
	UTIL_FireTargets( pev->netname, m_hActivator, this, USE_SET, m_flValue );
	m_vecPosition2 = pev->origin; //save current floor
	if(m_iState == STATE_TURN_ON) m_iState = STATE_ON;
	if(m_iState == STATE_TURN_OFF) m_iState = STATE_OFF;
}

void CBasePlatform :: GoDown( void )
{
	EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise), m_flVolume, ATTN_NORM);

	ASSERT(m_iState == STATE_ON || m_iState == STATE_TURN_ON);
	m_iState = STATE_TURN_OFF;
	SetMoveDone( HitBottom );

	if(IsRotatingPlatform()) AngularMove( m_vecAngle1, pev->speed );
	else if(IsMovingPlatform()) LinearMove( m_vecPosition1, pev->speed );
	else if(IsComplexPlatform())ComplexMove( m_vecPosition1, m_vecAngle1, pev->speed );
	else HitBottom();//don't brake platform status
}

void CBasePlatform :: HitBottom( void )
{
	STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise));
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, (char*)STRING(pev->noise1), m_flVolume, ATTN_NORM);

	ASSERT(m_iState == STATE_TURN_OFF);
	UTIL_FireTargets( pev->netname, m_hActivator, this, USE_SET, 1 );
	m_iState = STATE_OFF;
}

void CBasePlatform :: GoUp( void )
{
	EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise), m_flVolume, ATTN_NORM);
	
	ASSERT(m_iState == STATE_OFF || m_iState == STATE_TURN_OFF);
	m_iState = STATE_TURN_ON;
	SetMoveDone( HitTop );

	if(IsRotatingPlatform()) AngularMove( m_vecAngle2, pev->speed );
	else if(IsMovingPlatform()) LinearMove( m_vecPosition2, pev->speed );
	else if(IsComplexPlatform())ComplexMove( m_vecPosition2, m_vecAngle2, pev->speed );
	else HitTop();//don't brake platform status
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

	if (m_iState == STATE_TURN_ON) GoDown();
	else if (m_iState == STATE_TURN_OFF) GoUp();
	SetNextThink( 0 );
}
LINK_ENTITY_TO_CLASS( func_platform, CBasePlatform );
LINK_ENTITY_TO_CLASS( func_platrot, CBasePlatform );


//=======================================================================
// 		   func_train (Classic QUAKE Train)
//=======================================================================
TYPEDESCRIPTION	CFuncTrain::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncTrain, pCurPath, FIELD_CLASSPTR ),
	DEFINE_FIELD( CFuncTrain, pNextPath, FIELD_CLASSPTR ),
};
IMPLEMENT_SAVERESTORE( CFuncTrain, CBasePlatform );

void CFuncTrain :: Spawn( void )
{
	Precache(); //precache moving & stop sounds
	CBaseBrush::Spawn();
	
	if(pev->spawnflags & SF_DOOR_PASSABLE || IsWater())//make illusionary train 
		pev->solid = SOLID_NOT;
	else pev->solid = SOLID_BSP;
          
          pev->movetype = MOVETYPE_PUSH;
	UTIL_SetOrigin(this, pev->origin);
	UTIL_SetSize(pev, pev->mins, pev->maxs);
	UTIL_SetModel(ENT(pev), pev->model );

	//determine method for calculating origin
	if(pev->origin != g_vecZero) pev->impulse = 1;
          
	if (pev->speed == 0) pev->speed = 100;
	m_iState = STATE_OFF;
}

void CFuncTrain :: PostSpawn( void )
{
	if (!FindPath()) return;

	if (pev->impulse)
	{
		m_vecSpawnOffset = m_vecSpawnOffset + pCurPath->pev->origin - pev->origin;
		if (m_pParent) UTIL_AssignOrigin (this, pCurPath->pev->origin - m_pParent->pev->origin );
		else UTIL_AssignOrigin (this, pCurPath->pev->origin );
	}
	else
	{
		m_vecSpawnOffset = m_vecSpawnOffset + (pCurPath->pev->origin - TrainOrg()) - pev->origin;
		if (m_pParent) UTIL_AssignOrigin (this, pCurPath->pev->origin - TrainOrg() - m_pParent->pev->origin );
		else UTIL_AssignOrigin (this, pCurPath->pev->origin - TrainOrg());
	}
	pev->target = pCurPath->pev->target;
}

void CFuncTrain :: PostActivate( void )
{
	if(m_iState == STATE_ON)//platform "in-moving" ? restore sound!
	{
		EMIT_SOUND (ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise), m_flVolume, ATTN_NORM);
	}
	if ( pev->spawnflags & SF_START_ON )
	{	
		m_iState = STATE_OFF; //restore sound on a next level
		SetThink( Next );	//evil stuff...
		SetNextThink( 0.1 );
		ClearBits( pev->spawnflags, SF_START_ON );//fire once
	}
}

void CFuncTrain::ClearPointers( void )
{
	CBaseEntity::ClearPointers();
	pCurPath = NULL;
	pNextPath = NULL;
}

void CFuncTrain::OverrideReset( void )
{
	// Are we moving?
	if ( m_iState == STATE_ON )
	{
		pev->target = pev->message; 
		Msg("Override target %s\n", STRING( pev->target ));
		if (FindPath()) SetBits( pev->spawnflags, SF_START_ON );//PostActivate member
		else Stop();
	}
}

CBaseEntity *CFuncTrain::FindPath( void )
{
	//find start track
	Msg("Find corner %s\n", STRING( pev->target ));
	pCurPath = UTIL_FindEntityByTargetname (NULL, STRING(pev->target));
	if(pCurPath && pCurPath->edict())
	{
		return pCurPath;
	}
	return NULL;
}

CBaseEntity *CFuncTrain::FindNextPath( void )
{
	//safe way to check patch
	if(!pCurPath) return FALSE;
	
	// get pointer to next target
	if(pev->speed > 0)pNextPath = ((CPathCorner *)pCurPath)->GetNext();
	if(pev->speed < 0)pNextPath = ((CPathCorner *)pCurPath)->GetPrev();
	
	if(pNextPath && pNextPath->edict()) //validate path
	{
		//record new value (this will be used after changelevel)
		//pev->target = pNextPath->pev->targetname; 

		//pCurPath = pNextPath;
		Msg("CFuncTrain::FindNextPath: pCurPath %s pNextPath %s\n", STRING( pCurPath->pev->targetname ), STRING( pNextPath->pev->targetname ));
		return pNextPath; //path found
	}

	Stop();//stopping platform 
	return NULL;
}

void CFuncTrain :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if (useType == USE_TOGGLE)
	{
		if(m_iState == STATE_ON) useType = USE_OFF;
		else useType = USE_ON;
	}
	if (useType == USE_ON) Next();
	else if ( useType == USE_OFF ) Stop();
	else if (useType == USE_SET) Reverse();//reverse train
	else if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		ALERT(at_console, "State: %s, speed %g\n", GetStringForState( GetState()), pev->speed );
		if(GetPrev() && GetPrev()->edict()) Msg("Prev path %s", STRING(GetPrev()->pev->targetname) );
		if(GetNext() && GetNext()->edict()) Msg("Next path %s", STRING(GetNext()->pev->targetname) );
		SHIFT;
	}
}

void CFuncTrain :: Next( void )
{
	CBaseEntity *pTarg = FindPath();
          
          if(!pTarg) return;
	
	//linear move to next corner.
	if(m_iState == STATE_OFF)//enable train sound
	{
		STOP_SOUND( edict(), CHAN_STATIC, (char*)STRING(pev->noise) );
		EMIT_SOUND (ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise), m_flVolume, ATTN_NORM);
	}

	ClearBits(pev->effects, EF_NOINTERP); //enable interpolation
	m_iState = STATE_ON;
          //pev->message = ((CPathCorner *)pCurPath)->GetNext()->pev->targetname;
          pev->message = pev->target;
          pev->target = pTarg->pev->target;
          
	/*if( pev->speed < 0 && FBitSet(pNextPath->pev->spawnflags, SF_CORNER_TELEPORT))
	{
		SetBits(pev->effects, EF_NOINTERP);
		if (m_pParent) UTIL_AssignOrigin(this, pNextPath->pev->origin - m_pParent->pev->origin );
		else UTIL_AssignOrigin(this, pNextPath->pev->origin );
		Wait(); // Get on with doing the next path corner.
		return;
	}*/

	if (m_pParent)
	{
		if (pev->impulse) LinearMove( pTarg->pev->origin - m_pParent->pev->origin, fabs( pev->speed));
		else  LinearMove (pTarg->pev->origin - TrainOrg() - m_pParent->pev->origin, fabs(pev->speed));
	}
	else
	{
		if (pev->impulse) LinearMove( pTarg->pev->origin, fabs(pev->speed) );
		else  LinearMove (pTarg->pev->origin - TrainOrg(), fabs(pev->speed) );
	}
	pCurPath = pTarg;
	SetMoveDone( Wait );
}

void CFuncTrain :: Wait( void )
{
	UTIL_FireTargets(pev->netname, this, this, USE_TOGGLE );
	((CPathCorner *)pCurPath)->UpdateTargets();
	((CPathCorner *)pCurPath)->GetSpeed( &pev->speed );
	if(!Stop(((CPathCorner *)pCurPath)->GetDelay())) Next();// go to next corner
}

BOOL CFuncTrain :: Teleport( void )
{
	if( FBitSet(pNextPath->pev->spawnflags, SF_CORNER_TELEPORT))
	{
		SetBits(pev->effects, EF_NOINTERP);

		//determine teleportation point
		if(pev->speed > 0) 
		{
			pNextPath = pNextPath->GetNext();
			((CPathCorner *)pCurPath)->UpdateTargets();
			UTIL_FireTargets(pev->netname, this, this, USE_TOGGLE );
                    }
		if (m_pParent) UTIL_AssignOrigin(this, pNextPath->pev->origin - m_pParent->pev->origin );
		else UTIL_AssignOrigin(this, pNextPath->pev->origin );
	
		pCurPath = pNextPath;
		Next(); // Get on with doing the next path corner.
		return TRUE;
	}
	return FALSE;
}

BOOL CFuncTrain :: Stop( float flWait )
{
	if( flWait == 0 ) return FALSE;
	m_iState = STATE_OFF;
 
 	// clear the sound channel.
	STOP_SOUND( edict(), CHAN_STATIC, (char*)STRING(pev->noise) );
	EMIT_SOUND (ENT(pev), CHAN_VOICE, (char*)STRING(pev->noise1), m_flVolume, ATTN_NORM);

	UTIL_SetVelocity(this, g_vecZero);
	UTIL_SetAvelocity(this, g_vecZero);
	if( flWait > 0)
	{
		SetNextThink( flWait );
		SetThink( Next );		
	}

	//wait for retrigger
	if(flWait == -1) DontThink();
	return TRUE;
}

void CFuncTrain::Reverse( void )
{
	//update path if dir changed
	if(pev->speed != 0 && pNextPath && pNextPath->edict()) pCurPath = pNextPath;
	pev->speed = -pev->speed;
	if(m_iState == STATE_ON) Next(); //reverse now!
}

void CFuncTrain :: Blocked( CBaseEntity *pOther )
{
	// Keep "movewith" entities in line
	UTIL_AssignOrigin(this, pev->origin);

	if ( gpGlobals->time < m_flBlockedTime) return;
	m_flBlockedTime = gpGlobals->time + 0.5;

	if(m_pParent && m_pParent->edict() && pFlags & PF_PARENTMOVE) m_pParent->Blocked( pOther);
	pOther->TakeDamage( pev, pev, 1, DMG_CRUSH );
	STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise));
	ASSERT(m_iState == STATE_ON);

	Stop( 0.5 );
}
LINK_ENTITY_TO_CLASS( func_train, CFuncTrain );

//=======================================================================
// 		   func_tracktrain (controllable train)
//=======================================================================
void CFuncTrackTrain :: Precache( void )
{
	CBaseBrush::Precache(); // precache damage sound

	int m_sounds = UTIL_LoadSoundPreset( m_iMoveSound );
	switch( m_sounds )
	{
	case 1: pev->noise = UTIL_PrecacheSound( "trains/ttrain1.wav" ); break;
	case 2: pev->noise = UTIL_PrecacheSound( "trains/ttrain2.wav" ); break;
	case 3: pev->noise = UTIL_PrecacheSound( "trains/ttrain3.wav" ); break; 
	case 4: pev->noise = UTIL_PrecacheSound( "trains/ttrain4.wav" ); break;
	case 5: pev->noise = UTIL_PrecacheSound( "trains/ttrain6.wav" ); break;
	case 6: pev->noise = UTIL_PrecacheSound( "trains/ttrain7.wav" ); break;
	default: pev->noise = UTIL_PrecacheSound( m_sounds ); break; // custom sound or sentence
	}
          
	// always constant
	pev->noise1 = UTIL_PrecacheSound( "trains/ttrain_brake1.wav" );
	pev->noise2 = UTIL_PrecacheSound( "trains/ttrain_start1.wav" );
}

void CFuncTrackTrain :: Spawn( void )
{
	if ( pev->speed == 0 ) m_speed = 100;
	else m_speed = pev->speed;
	pev->speed = 0;
	pev->velocity = g_vecZero;
	m_vecBaseAvel = pev->avelocity; //LRC - save it for later
	pev->avelocity = g_vecZero;
	pev->impulse = m_speed;
	m_dir = 1;

	if ( FStringNull(pev->target) ) Msg("Warning: %s with no target!\n", STRING(pev->classname));

	//if ( pev->spawnflags & SF_NOTSOLID ) pev->solid = SOLID_NOT;
	if ( pev->spawnflags & 8 ) pev->solid = SOLID_NOT; //temp solution
	else pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	SetBits (pFlags, PF_ANGULAR);
	UTIL_SetModel( ENT(pev), pev->model );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( this, pev->origin );
	pev->oldorigin = pev->origin; // Cache off placed origin for train controls

	m_controlMins = pev->mins;
	m_controlMaxs = pev->maxs;
	m_controlMaxs.z += 72;

	NextThink( 0.1, FALSE );
	SetThink( Find );
	Precache();
}

void CFuncTrackTrain :: NextThink( float thinkTime, BOOL alwaysThink )
{
	if ( alwaysThink ) pev->flags |= FL_ALWAYSTHINK;
	else pev->flags &= ~FL_ALWAYSTHINK;
	SetNextThink( thinkTime, TRUE );
}

void CFuncTrackTrain :: Blocked( CBaseEntity *pOther )
{
	// Blocker is on-ground on the train
	if ( FBitSet( pOther->pev->flags, FL_ONGROUND ) && VARS(pOther->pev->groundentity) == pev )
	{
		float deltaSpeed = fabs(pev->speed);
		if ( deltaSpeed > 50 ) deltaSpeed = 50;
		if ( !pOther->pev->velocity.z ) pOther->pev->velocity.z += deltaSpeed;
		return;
	}
	else pOther->pev->velocity = (pOther->pev->origin - pev->origin ).Normalize() * pev->dmg;
	if ( pev->dmg <= 0 ) return;// we can't hurt this thing, so we're not concerned with it
	pOther->TakeDamage( pev, pev, pev->dmg, DMG_CRUSH );
}

void CFuncTrackTrain::OverrideReset( void )
{
	NextThink( 0.1, FALSE );
	SetThink( NearestPath );
}

void CFuncTrackTrain :: Find( void )
{
	pPath = (CPathTrack*)UTIL_FindEntityByTargetname( NULL, STRING(pev->target) );
	if ( !pPath ) return;

	entvars_t *pevTarget = pPath->pev;
	if ( !FClassnameIs( pevTarget, "path_track" ) )
	{
		ALERT( at_error, "func_track_train must be on a path of path_track\n" );
		pPath = NULL;
		return;
	}

	Vector nextPos = pevTarget->origin;
	nextPos.z += m_height;

	Vector look = nextPos;
	look.z -= m_height;
	((CPathTrack *)pPath)->LookAhead( &look, m_length, 0 );
	look.z += m_height;

	Vector vTemp = UTIL_VecToAngles( look - nextPos );
	vTemp.y += 180;

	if ( pev->spawnflags & SF_TRACKTRAIN_NOPITCH )
	{
		vTemp.x = 0;
		//pev->angles.x = 0;
	}

	UTIL_AssignAngles(this, vTemp); 
	UTIL_AssignOrigin ( this, nextPos );

	NextThink( 0.1, FALSE );
	SetThink( PostponeNext );
	pev->speed = m_startSpeed;

	UpdateSound();
}

TYPEDESCRIPTION	CFuncTrackTrain::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncTrackTrain, pPath, FIELD_CLASSPTR ),
	DEFINE_FIELD( CFuncTrackTrain, m_length, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_height, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_speed, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_dir, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_startSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_controlMins, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTrackTrain, m_controlMaxs, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTrackTrain, m_flVolume, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_flBank, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_oldSpeed, FIELD_FLOAT ),
};
IMPLEMENT_SAVERESTORE( CFuncTrackTrain, CBaseMover );


void CFuncTrackTrain :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "wheels"))
	{
		m_length = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "height"))
	{
		m_height = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "startspeed"))
	{
		m_startSpeed = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "sounds"))
	{
		m_iMoveSound = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "bank"))
	{
		m_flBank = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CBaseMover::KeyValue( pkvd );
}





void CFuncTrackTrain :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
//	ALERT( at_console, "TRAIN: use\n" );

	m_hActivator = pActivator;	//AJH

	if (useType == USE_TOGGLE)
	{
		if(pev->speed != 0) useType = USE_OFF;//temp solution
		else useType = USE_ON;
	}
	if (useType == USE_ON)
	{
		pev->speed = m_speed * m_dir;
		PostponeNext();	
	}
	else if ( useType == USE_OFF )
	{
		pev->speed = 0;
		UTIL_SetVelocity(this, g_vecZero); //LRC
		UTIL_SetAvelocity(this, g_vecZero); //LRC
		StopSound();
		SetThink( NULL );
	}
	else if ( useType == USE_SET )
	{
		float delta = value;

		delta = ((int)(pev->speed * 4) / (int)m_speed)*0.25 + 0.25 * delta;
		if ( delta > 1 )
			delta = 1;
		else if ( delta < -1 )
			delta = -1;
		if ( pev->spawnflags & SF_TRACKTRAIN_FORWARDONLY )
		{
			if ( delta < 0 )delta = 0;
		}
		pev->speed = m_speed * delta;
                   
                    if(pev->speed == 0)
                    {
			UTIL_SetVelocity(this, g_vecZero); 
			UTIL_SetAvelocity(this, g_vecZero);
			StopSound();
			SetThink( NULL );
		}
	          if(pPath == NULL) 
	          {
			delta = 0; //G-Cont. Set speed to 0, and don't controls, if tracktrain on trackchange
			return;
	          }
		PostponeNext();	
		ALERT( at_aiconsole, "TRAIN(%s), speed to %.2f\n", STRING(pev->targetname), pev->speed );
	}
}

#define TRAIN_STARTPITCH	60
#define TRAIN_MAXPITCH		200
#define TRAIN_MAXSPEED		1000	// approx max speed for sound pitch calculation

void CFuncTrackTrain :: StopSound( void )
{
	// if sound playing, stop it
	if( m_soundPlaying && pev->noise )
	{
		STOP_SOUND( ENT( pev ), CHAN_STATIC, STRING( pev->noise ));
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "trains/ttrain_brake1.wav", m_flVolume, ATTN_NORM, 0, 100 );
	}
	m_soundPlaying = 0;
}

void CFuncTrackTrain :: UpdateSound( void )
{
	float flpitch;
	
	if( !pev->noise ) return;

	flpitch = TRAIN_STARTPITCH + (abs(pev->speed) * (TRAIN_MAXPITCH - TRAIN_STARTPITCH) / TRAIN_MAXSPEED);

	if( !m_soundPlaying )
	{
		// play startup sound for train
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "trains/ttrain_start1.wav", m_flVolume, ATTN_NORM, 0, 100 );
		EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, STRING( pev->noise ), m_flVolume, ATTN_NORM, 0, (int)flpitch );
		m_soundPlaying = 1;
	} 
	else
	{
		// update pitch
		EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, STRING( pev->noise ), m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, (int)flpitch );
	}
}

void CFuncTrackTrain::ClearPointers( void )
{
	CBaseEntity::ClearPointers();
	pPath = NULL;
}

void CFuncTrackTrain :: PostActivate( void )
{
}

void CFuncTrackTrain :: PostponeNext( void )
{
	DesiredAction();//temp fix
	//UTIL_SetAction( this );
}

void CFuncTrackTrain :: DesiredAction( void ) // Next( void )
{
	float time = 0.5;

	if ( !pev->speed )
	{
		UTIL_SetVelocity(this, g_vecZero);
		DontThink();
		ALERT( at_aiconsole, "TRAIN(%s): Speed is 0\n", STRING(pev->targetname) );
		StopSound();
		return;
	}
          
	if ( !pPath )
	{	
		UTIL_SetVelocity(this, g_vecZero);
		DontThink();
		ALERT( at_aiconsole, "TRAIN(%s): Lost path\n", STRING(pev->targetname) );
		StopSound();
		return;
	}

	UpdateSound();

	Vector nextPos = pev->origin;
          
	nextPos.z -= m_height;
	CBaseEntity *pnext = ((CPathTrack *)pPath)->LookAhead( &nextPos, pev->speed * 0.1, 1 );
	nextPos.z += m_height;

	UTIL_SetVelocity( this, (nextPos - pev->origin) * 10 ); //LRC
	Vector nextFront = pev->origin;

	nextFront.z -= m_height;
	if ( m_length > 0 )
		((CPathTrack *)pPath)->LookAhead( &nextFront, m_length, 0 );
	else
		((CPathTrack *)pPath)->LookAhead( &nextFront, 100, 0 );
	nextFront.z += m_height;

	Vector delta = nextFront - pev->origin;

	Vector angles = UTIL_VecToAngles( delta );
	// The train actually points west
	angles.y += 180; //LRC, FIXME: add a 'built facing' field.

	angles.fixangle();
	pev->angles.fixangle();

	if ( !pnext || (delta.x == 0 && delta.y == 0) ) angles = pev->angles;

	float vy, vx, vz;
	if ( !(pev->spawnflags & SF_TRACKTRAIN_NOPITCH) ) vx = 10*UTIL_AngleDistance( angles.x, pev->angles.x );
	else vx = m_vecBaseAvel.x;

	if ( !(pev->spawnflags & SF_TRACKTRAIN_NOYAW) ) vy = 10*UTIL_AngleDistance( angles.y, pev->angles.y );
	else vy = m_vecBaseAvel.y;

	if ( m_flBank != 0 )
	{
		if ( pev->avelocity.y < -5 ) vz = UTIL_AngleDistance( UTIL_ApproachAngle( -m_flBank, pev->angles.z, m_flBank*2 ), pev->angles.z);
		else if ( pev->avelocity.y > 5 ) vz = UTIL_AngleDistance( UTIL_ApproachAngle( m_flBank, pev->angles.z, m_flBank*2 ), pev->angles.z);
		else vz = UTIL_AngleDistance( UTIL_ApproachAngle( 0, pev->angles.z, m_flBank*4 ), pev->angles.z) * 4;
	}
	else vz = m_vecBaseAvel.z;
	UTIL_SetAvelocity(this, Vector(vx, vy, vz));
		
	if ( pnext )
	{
		if ( pnext != pPath )
		{
			CBaseEntity *pFire;
			if ( pev->speed >= 0 ) // check whether we're going forwards or backwards
				pFire = pnext;
			else 
				pFire = pPath;

			pPath = pnext;
			// Fire the pass target if there is one
			if ( pFire->pev->message )
			{
				UTIL_FireTargets( pFire->pev->message, this, this, USE_TOGGLE );
				if ( FBitSet( pFire->pev->spawnflags, SF_PATH_FIREONCE ) )
					pFire->pev->message = 0;
			}

			if ( pFire->pev->spawnflags & SF_PATH_DISABLE_TRAIN )
				pev->spawnflags |= SF_TRACKTRAIN_NOCONTROL;

			float setting = ((int)(pev->speed*4) / (int)m_speed) / 4.0; //LRC - one of { 1, 0.75, 0.5, 0.25, 0, ... -1 }

			CBaseEntity* pDest; //LRC - the path_track we're heading for, after pFire.
			if (pev->speed > 0)
				pDest = pFire->GetNext();
			else
				pDest = pFire->GetPrev();

			if ( pFire->pev->speed != 0)
			{
				//ALERT( at_console, "TrackTrain setting is %d / %d = %.2f\n", (int)(pev->speed*4), (int)m_speed, setting );

				switch ( (int)(pFire->pev->armortype) )
				{
				case PATHSPEED_SET:
					// Don't override speed if under user control
					if (pev->spawnflags & SF_TRACKTRAIN_NOCONTROL)
					pev->speed = pFire->pev->speed;
					ALERT( at_aiconsole, "TrackTrain %s speed set to %4.2f\n", STRING(pev->targetname), pev->speed );
					break;
				case PATHSPEED_SET_MASTER:
					m_speed = pFire->pev->speed;
					pev->impulse = m_speed;
					pev->speed = setting * m_speed;
					ALERT( at_aiconsole, "TrackTrain %s master speed set to %4.2f\n", STRING(pev->targetname), pev->speed );
					break;
				case PATHSPEED_ACCEL:
					m_speed += pFire->pev->speed;
					pev->impulse = m_speed;
					pev->speed = setting * m_speed;
					ALERT( at_aiconsole, "TrackTrain %s speed accel to %4.2f\n", STRING(pev->targetname), pev->speed );
					break;
				case PATHSPEED_TIME:
					float distance = (pev->origin - pDest->pev->origin).Length();
					// ALERT( at_console, "pFire=%s, distance=%.2f, ospeed=%.2f, nspeed=%.2f\n", STRING(pFire->pev->targetname), distance, pev->speed, distance / pFire->pev->speed);
					m_speed = distance / pFire->pev->speed;
					pev->impulse = m_speed;
					pev->speed = setting * m_speed;
					ALERT( at_aiconsole, "TrackTrain %s speed to %4.2f (timed)\n", STRING(pev->targetname), pev->speed );
					break;
				}
			}
			//LRC- FIXME: add support, here, for a Teleport flag.
		}
		SetThink( PostponeNext );
		NextThink( time, TRUE );
	}
	else	// end of path, stop
	{
		Vector vecTemp; //LRC
		StopSound();
		vecTemp = (nextPos - pev->origin); //LRC

		UTIL_SetAvelocity(this, g_vecZero);
		float distance = vecTemp.Length(); //LRC
		m_oldSpeed = pev->speed;


		pev->speed = 0;
		
		// Move to the dead end
		
		// Are we there yet?
		if ( distance > 0 )
		{
			// no, how long to get there?
			time = distance / m_oldSpeed;
			UTIL_SetVelocity( this, vecTemp * (m_oldSpeed / distance) );
			SetThink( DeadEnd );
			NextThink( time, FALSE );
		}
		else
		{
			UTIL_SetVelocity( this, vecTemp );
			DeadEnd();
		}
	}
}


void CFuncTrackTrain::DeadEnd( void )
{
	// Fire the dead-end target if there is one
	CBaseEntity *pTrack, *pNext;

	pTrack = pPath;

	ALERT( at_aiconsole, "TRAIN(%s): Dead end ", STRING(pev->targetname) );
	// Find the dead end path node
	// HACKHACK -- This is bugly, but the train can actually stop moving at a different node depending on it's speed
	// so we have to traverse the list to it's end.
	if ( pTrack )
	{
		if ( m_oldSpeed < 0 )
		{
			do
			{
				pNext = ((CPathTrack *)pTrack)->ValidPath( pTrack->GetPrev(), TRUE );
				if ( pNext )
					pTrack = pNext;
			} while ( pNext );
		}
		else
		{
			do
			{
				pNext = ((CPathTrack *)pTrack)->ValidPath( pTrack->GetNext(), TRUE );
				if ( pNext )
					pTrack = pNext;
			} while ( pNext );
		}
	}

	UTIL_SetVelocity( this, g_vecZero );
	UTIL_SetAvelocity(this, g_vecZero );
	
	if ( pTrack )
	{
		ALERT( at_aiconsole, "at %s\n", STRING(pTrack->pev->targetname) );
		if ( pTrack->pev->netname )
			UTIL_FireTargets( pTrack->pev->netname, this, this, USE_TOGGLE );
	}
}


void CFuncTrackTrain :: SetControls( entvars_t *pevControls )
{
	Vector offset = pevControls->origin - pev->oldorigin;

	m_controlMins = pevControls->mins + offset;
	m_controlMaxs = pevControls->maxs + offset;
}


BOOL CFuncTrackTrain :: OnControls( entvars_t *pevTest )
{
	Vector offset = pevTest->origin - pev->origin;

	if ( pev->spawnflags & SF_TRACKTRAIN_NOCONTROL )
		return FALSE;

	// Transform offset into local coordinates
	UTIL_MakeVectors( pev->angles );
	Vector local;
	local.x = DotProduct( offset, gpGlobals->v_forward );
	local.y = -DotProduct( offset, gpGlobals->v_right );
	local.z = DotProduct( offset, gpGlobals->v_up );

	if ( local.x >= m_controlMins.x && local.y >= m_controlMins.y && local.z >= m_controlMins.z &&
		 local.x <= m_controlMaxs.x && local.y <= m_controlMaxs.y && local.z <= m_controlMaxs.z )
		 return TRUE;

	return FALSE;
}

void CFuncTrackTrain :: NearestPath( void )
{
	CBaseEntity *pTrack = NULL;
	CBaseEntity *pNearest = NULL;
	float dist, closest;

	closest = 1024;

	while ((pTrack = UTIL_FindEntityInSphere( pTrack, pev->origin, 1024 )) != NULL)
	{
		// filter out non-tracks
		if ( !(pTrack->pev->flags & (FL_CLIENT|FL_MONSTER)) && FClassnameIs( pTrack->pev, "path_track" ) )
		{
			dist = (pev->origin - pTrack->pev->origin).Length();
			if ( dist < closest )
			{
				closest = dist;
				pNearest = pTrack;
			}
		}
	}

	if( !pNearest )
	{
		ALERT( at_console, "Can't find a nearby track !!!\n" );
		SetThink( NULL );
		return;
	}

	ALERT( at_aiconsole, "TRAIN: %s, Nearest track is %s\n", STRING(pev->targetname), STRING(pNearest->pev->targetname) );
	// If I'm closer to the next path_track on this path, then it's my real path
	pTrack = ((CPathTrack *)pNearest)->GetNext();
	if ( pTrack )
	{
		if ( (pev->origin - pTrack->pev->origin).Length() < (pev->origin - pNearest->pev->origin).Length() )
			pNearest = pTrack;
	}

	pPath = (CPathTrack *)pNearest;

	if ( pev->speed != 0 )
	{
		NextThink( 0.1, FALSE );
		SetThink( PostponeNext );
	}
}
LINK_ENTITY_TO_CLASS( func_tracktrain, CFuncTrackTrain );