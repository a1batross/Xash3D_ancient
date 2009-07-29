//=======================================================================
//			Copyright (C) Shambler Team 2004
//		         baseinfo.cpp - point info entities. 
//			       e.g. info_target			    
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "player.h"
#include "client.h"

//=======================================================================
// 			info_target (target entity)
//=======================================================================

class CInfoTarget : public CPointEntity
{
public:
	void Spawn( void )
	{
		pev->solid = SOLID_NOT;
		UTIL_SetModel(ENT(pev),"models/common/null.mdl");
		UTIL_SetSize(pev, g_vecZero, g_vecZero);
		SetBits( pev->flags, FL_POINTENTITY );
	}
};

void CBaseDMStart::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "master"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

STATE CBaseDMStart::GetState( CBaseEntity *pEntity )
{
	if (UTIL_IsMasterTriggered( pev->netname, pEntity ))
		return STATE_ON;
	else	return STATE_OFF;
}

//=========================================================
//	static infodecal
//=========================================================
class CDecal : public CBaseEntity
{
public:
	void KeyValue( KeyValueData *pkvd )
	{
		if (FStrEq(pkvd->szKeyName, "texture"))
		{
			pev->skin = DECAL_INDEX( pkvd->szValue );
			if ( pev->skin >= 0 ) return;
			Msg( "Can't find decal %s\n", pkvd->szValue );
		}
	}
	void PostSpawn( void ) { if(FStringNull(pev->targetname))MakeDecal(); }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) { MakeDecal(); }
	void MakeDecal( void )
	{
		if ( pev->skin < 0 ) { REMOVE_ENTITY(ENT(pev)); return; }
		TraceResult trace;
		int entityIndex, modelIndex;
		UTIL_TraceLine( pev->origin - Vector(5,5,5), pev->origin + Vector(5,5,5),  ignore_monsters, ENT(pev), &trace );
		entityIndex = (short)ENTINDEX(trace.pHit);
		if ( entityIndex ) modelIndex = (int)VARS(trace.pHit)->modelindex;
		else modelIndex = 0;
                    
		if(FStringNull(pev->targetname)) g_engfuncs.pfnStaticDecal( pev->origin, (int)pev->skin, entityIndex, modelIndex );
		else
		{
			MESSAGE_BEGIN( MSG_BROADCAST, gmsg.TempEntity );
			WRITE_BYTE( TE_BSPDECAL );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_SHORT( (int)pev->skin );
			WRITE_SHORT( entityIndex );
			if(entityIndex) WRITE_SHORT( modelIndex );
			MESSAGE_END();
                    }
		
		SetThink( Remove );
		SetNextThink( 0.3 );
	}
};

//=========================================================
// Multiplayer intermission spots.
//=========================================================
class CInfoIntermission : public CPointEntity
{
	void Spawn( void );
	void Think( void );
	void PostActivate( void );
	CBaseEntity *pTarget;
	void KeyValue( KeyValueData *pkvd );

};

void CInfoIntermission :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetModel( ENT( pev ), "sprites/null.spr" );
          
	SetNextThink( 1 );// let targets spawn!
}

void CInfoIntermission::PostActivate( void )
{
	if( FStrEq( STRING( pev->classname ), "info_intermission" ))
		pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ));
	if( !pev->speed ) pev->speed = 100;
}

void CInfoIntermission::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "mangle" ))
	{
		Vector	tmp;

		// Quake1 intermission angles
		UTIL_StringToVector( tmp, pkvd->szValue );
		if( tmp != g_vecZero ) pev->angles = tmp;
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "roll" ))
	{
		// Quake3 portal camera
		pev->frame = atof( pkvd->szValue ) / 360.0f;
		pkvd->fHandled = TRUE;
	}
	else CBaseEntity::KeyValue( pkvd );
}

void CInfoIntermission::Think ( void )
{
	if( pTarget ) 
	{
		UTIL_WatchTarget( this, pTarget );
		SetNextThink( 0 );
	}
}

//=========================================================
// 		portal surfaces
//=========================================================
class CPortalSurface : public CPointEntity
{
	void Spawn( void );
	void Think( void );
	void PostActivate( void );
};

void CPortalSurface :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;

	SetObjectClass( ED_PORTAL );
	UTIL_SetOrigin( this, pev->origin );
	pev->modelindex = 1;	// world
          
	SetNextThink( 1 );// let targets spawn!
}

void CPortalSurface :: Think( void )
{
	if( FNullEnt( pev->owner ))
		pev->oldorigin = pev->origin;
	else pev->oldorigin = pev->owner->v.origin;

	SetNextThink( 0.1f );
}

void CPortalSurface :: PostActivate( void )
{
	Vector		dir;
	CBaseEntity	*pTarget, *pOwner;

	SetNextThink( 0 );

	if( FStringNull( pev->target ))
		return;	// mirror

	pOwner = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ));
	if( !pOwner )
	{
		ALERT( at_warning, "Couldn't find target for %s\n", STRING( pev->classname ));
		UTIL_Remove( this );
		return;
	}

	pev->owner = pOwner->edict();

	// q3a swinging camera support
	if( pOwner->pev->spawnflags & 1 )
		pev->framerate = 25;
	else if( pOwner->pev->spawnflags & 2 )
		pev->framerate = 75;
	if( pOwner->pev->spawnflags & 4 )
		pev->effects &= ~EF_ROTATE;
	else pev->effects |= EF_ROTATE;

	pev->frame = pOwner->pev->frame; // rollangle

	// see if the portal_camera has a target
	if( !FStringNull( pOwner->pev->target ))
		pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pOwner->pev->target ));
	else pTarget = NULL;

	if( pTarget )
	{
		pev->movedir = pTarget->pev->origin - pOwner->pev->origin;
		pev->movedir.Normalize();
	}
	else 
	{
		pev->angles = pOwner->pev->angles;
		UTIL_LinearVector( this );
	}
}

//====================================================================
//			multisource
//====================================================================

TYPEDESCRIPTION CMultiSource::m_SaveData[] =
{
	DEFINE_ARRAY( CMultiSource, m_rgEntities, FIELD_EHANDLE, MAX_MULTI_TARGETS ),
	DEFINE_ARRAY( CMultiSource, m_rgTriggered, FIELD_INTEGER, MAX_MULTI_TARGETS ),
	DEFINE_FIELD( CMultiSource, m_iTotal, FIELD_INTEGER ),
}; IMPLEMENT_SAVERESTORE( CMultiSource, CBaseLogic );
LINK_ENTITY_TO_CLASS( multisource, CMultiSource );

void CMultiSource::Spawn()
{ 
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	SetNextThink( 0.1 );
	pev->spawnflags |= SF_START_ON;
	SetThink( Register );
}

void CMultiSource::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	int i = 0;

	// Find the entity in our list
	while (i < m_iTotal) if ( m_rgEntities[i++] == pCaller ) break;

	// if we didn't find it, report error and leave
	if (i > m_iTotal) return;

	STATE s = GetState();
	m_rgTriggered[i-1] ^= 1;
	if ( s == GetState()) return;

	if ( s == STATE_OFF )
	{
		USE_TYPE useType = USE_TOGGLE;
		if ( m_globalstate ) useType = USE_ON;
		UTIL_FireTargets( pev->target, NULL, this, useType, value );
		UTIL_FireTargets( m_iszKillTarget, NULL, this, USE_REMOVE );
	}
}

STATE CMultiSource::GetState( void )
{
	// Is everything triggered?
	int i = 0;

	// Still initializing?
	if ( pev->spawnflags & SF_START_ON ) return STATE_OFF;

	while (i < m_iTotal)
	{
		if (m_rgTriggered[i] == 0) break;
		i++;
	}

	if (i == m_iTotal)
	{
		if ( !m_globalstate || gGlobalState.EntityGetState( m_globalstate ) == GLOBAL_ON )
			return STATE_ON;
	}
	return STATE_OFF;
}

void CMultiSource::Register(void)
{ 
	m_iTotal = 0;
	memset( m_rgEntities, 0, MAX_MULTI_TARGETS * sizeof(EHANDLE) );

	SetThink(NULL);

	// search for all entities which target this multisource (pev->targetname)

	CBaseEntity *pTarget = UTIL_FindEntityByTarget( NULL, STRING(pev->targetname) );
	while (pTarget && (m_iTotal < MAX_MULTI_TARGETS))
	{
		m_rgEntities[m_iTotal++] = pTarget;
		pTarget = UTIL_FindEntityByTarget( pTarget, STRING(pev->targetname));
	}

	pTarget = UTIL_FindEntityByClassname(NULL, "multi_manager");
	while (pTarget && (m_iTotal < MAX_MULTI_TARGETS))
	{
		if ( pTarget->HasTarget(pev->targetname) ) m_rgEntities[m_iTotal++] = pTarget;
		pTarget = UTIL_FindEntityByClassname( pTarget, "multi_manager" );
	}
	pev->spawnflags &= ~SF_START_ON;
}


LINK_ENTITY_TO_CLASS( infodecal, CDecal );
LINK_ENTITY_TO_CLASS( info_target, CInfoTarget );
LINK_ENTITY_TO_CLASS( target_position, CPointEntity );
LINK_ENTITY_TO_CLASS( info_teleport_destination, CPointEntity );
LINK_ENTITY_TO_CLASS( misc_portal_surface, CPortalSurface );
LINK_ENTITY_TO_CLASS( info_null, CNullEntity);
LINK_ENTITY_TO_CLASS( info_texlights, CNullEntity);
LINK_ENTITY_TO_CLASS( info_compile_parameters, CNullEntity);
LINK_ENTITY_TO_CLASS( info_intermission, CInfoIntermission );
LINK_ENTITY_TO_CLASS( misc_portal_camera, CInfoIntermission);
LINK_ENTITY_TO_CLASS( info_player_deathmatch, CBaseDMStart);
LINK_ENTITY_TO_CLASS( info_player_start, CPointEntity);
LINK_ENTITY_TO_CLASS( info_landmark, CPointEntity);