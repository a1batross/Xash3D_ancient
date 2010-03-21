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
	void	KeyValue( KeyValueData *pkvd )
	{
		if (FStrEq(pkvd->szKeyName, "texture"))
		{
			pev->skin = DECAL_INDEX( pkvd->szValue );
			if ( pev->skin >= 0 ) return;
			Msg( "Can't find decal %s\n", pkvd->szValue );
		}
	}
	void	PostSpawn( void )
	{
		if ( pev->skin < 0 ) { REMOVE_ENTITY(ENT(pev)); return; }
		TraceResult trace;
		int entityIndex, modelIndex;
		UTIL_TraceLine( pev->origin - Vector(5,5,5), pev->origin + Vector(5,5,5),  ignore_monsters, ENT(pev), &trace );
		entityIndex = (short)ENTINDEX(trace.pHit);
		if ( entityIndex ) modelIndex = (int)VARS(trace.pHit)->modelindex;
		else modelIndex = 0;
		g_engfuncs.pfnStaticDecal( pev->origin, (int)pev->skin, entityIndex, modelIndex );
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
		pev->viewangles[ROLL] = atof( pkvd->szValue ) / 360.0f;
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
	pev->flags |= FL_PHS_FILTER;	// use phs so can collect portal sounds

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

	pev->frame = pOwner->pev->viewangles[ROLL]; // rollangle

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

//=========================================================
// info_path - train node path.
//=========================================================
void CInfoPath :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "wait" ))
	{
		m_flDelay = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq (pkvd->szKeyName, "newspeed" ))
	{
		if ( pkvd->szValue[0] == '+' ) pev->button = SPEED_INCREMENT; //increase speed
		else if ( pkvd->szValue[0] == '-' ) pev->button = SPEED_DECREMENT; //decrease speed
		else if ( pkvd->szValue[0] == '*' ) pev->button = SPEED_MULTIPLY; //multiply speed by
		else if ( pkvd->szValue[0] == ':' ) pev->button = SPEED_DIVIDE; //divide speed by
		else pev->button = SPEED_MASTER; // just set new speed
		if( pev->button ) pkvd->szValue++;
		pev->speed = atof( pkvd->szValue );
		ALERT( at_console, "pev->button %d, pev->speed %g\n", pev->button, pev->speed );
		pkvd->fHandled = TRUE;
	}
	else if ( !FClassnameIs( pev, "path_corner" ) && m_cPaths < MAX_MULTI_TARGETS )
	{
		char tmp[128];

		UTIL_StripToken( pkvd->szKeyName, tmp );
		m_iPathName[m_cPaths] = ALLOC_STRING( tmp );
		m_iPathWeight[m_cPaths] = atof( pkvd->szValue );
		m_cPaths++;
		pkvd->fHandled = TRUE;
	}
	else CBaseEntity::KeyValue( pkvd );
}

TYPEDESCRIPTION CInfoPath::m_SaveData[] = 
{	DEFINE_FIELD( CInfoPath, m_cPaths, FIELD_INTEGER ),
	DEFINE_FIELD( CInfoPath, m_index, FIELD_INTEGER ),
	DEFINE_ARRAY( CInfoPath, m_iPathName, FIELD_STRING, MAX_MULTI_TARGETS ),
	DEFINE_ARRAY( CInfoPath, m_iPathWeight, FIELD_INTEGER, MAX_MULTI_TARGETS ),
	DEFINE_ARRAY( CInfoPath, m_pNextPath, FIELD_CLASSPTR, MAX_MULTI_TARGETS ),
	DEFINE_FIELD( CInfoPath, m_pPrevPath, FIELD_CLASSPTR ),
};IMPLEMENT_SAVERESTORE( CInfoPath, CBaseLogic );

LINK_ENTITY_TO_CLASS( info_path, CInfoPath );
LINK_ENTITY_TO_CLASS( path_corner, CInfoPath );

void CInfoPath :: Spawn( void )
{
	if( FClassnameIs( pev, "path_corner" ))
	{
		// compatible mode
		m_iPathName[m_cPaths] = pev->target;
		m_iPathWeight[m_cPaths] = 0;
		m_cPaths++;
	}

	int r_index = 0;
	int w_index = m_cPaths - 1;

	while( r_index < w_index )
	{
		// we store target with right index in tempname
		int name = m_iPathName [r_index];
		int weight = m_iPathWeight[r_index];
		
		// target with right name is free, record new value from wrong name
		m_iPathName [r_index] = m_iPathName [w_index];
		m_iPathWeight[r_index] = m_iPathWeight[w_index];
		
		// ok, we can swap targets
		m_iPathName [w_index] = name;
		m_iPathWeight[w_index] = weight;
		
		r_index++;
		w_index--;
	}
 
	m_iState = STATE_ON;
          m_index = 0;
	SetBits( pev->flags, FL_POINTENTITY );
	UTIL_SetModel( ENT( pev ), "blabla.mdl");
	pev->scale = 0.1f;
}

void CInfoPath :: PostActivate( void )
{
	// find all paths and save into array
	for(int i = 0; i < m_cPaths; i++ )
	{
		CBaseEntity *pNext = UTIL_FindEntityByTargetname( NULL, STRING( m_iPathName[i] ));
		if( pNext ) // found path
		{
			m_pNextPath[i] = (CInfoPath*)pNext; // valid path
			m_pNextPath[i]->SetPrev( this );
		}
	}
}

CBaseEntity *CInfoPath::GetNext( void )
{
	int total = 0;
	for ( int i = 0; i < m_cPaths; i++ )
	{
		total += m_iPathWeight[i];
	}

	if ( total ) // weighted random choose
	{
		int chosen = RANDOM_LONG( 0, total );
		int curpos = 0;
		for ( i = 0; i < m_cPaths; i++ )
		{
			curpos += m_iPathWeight[i];
			if ( curpos >= chosen )
			{
				m_index = i;
				break;
			}
		}
	}

	// validate path          
	ASSERTSZ( m_pNextPath[m_index] != this, "GetNext: self path!\n");
	if( m_pNextPath[m_index] && m_pNextPath[m_index]->edict() && m_pNextPath[m_index] != this && m_pNextPath[m_index]->m_iState == STATE_ON )

		return m_pNextPath[m_index];
	return NULL;
}

CBaseEntity *CInfoPath::GetPrev( void )
{
	// validate path
	ASSERTSZ( m_pPrevPath != this, "GetPrev: self path!\n");
	if(m_pPrevPath && m_pPrevPath->edict() && m_pPrevPath != this && m_pPrevPath->m_iState == STATE_ON )
		return m_pPrevPath;
	return NULL;
}

void CInfoPath::SetPrev( CInfoPath *pPrev )
{
	if( pPrev && pPrev->edict() && pPrev != this )
		m_pPrevPath = pPrev;
}

void CInfoPath :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if ( useType == USE_TOGGLE )
	{
		if( m_iState == STATE_ON )
			useType = USE_OFF;
		else useType = USE_ON;
	}
	if ( useType == USE_ON )
	{
		m_iState = STATE_ON;
	}
	else if ( useType == USE_OFF )
	{
		m_iState = STATE_OFF;
	}
	if ( useType == USE_SET ) // set new path
	{         
		if( value > 0.0f ) 
		{
			m_index = (value - 1);
			if( m_index >= m_cPaths )
				m_index = 0;
		}
	}
	else if ( useType == USE_RESET )
	{
		m_index = 0;
	}
	else if ( useType == USE_SHOWINFO )
	{
		ALERT( at_console, "======/Xash Debug System/======\n");
		ALERT( at_console, "classname: %s\n", STRING(pev->classname));
		ALERT( at_console, "State: %s, number of targets %d, path weight %d\n", GetStringForState( GetState()), m_cPaths- 1, m_iPathWeight[m_index]);
		if( m_pPrevPath && m_pPrevPath->edict( ))
			ALERT( at_console, "Prev path %s", STRING( m_pPrevPath->pev->targetname ));
		if( m_pNextPath[m_index] && m_pNextPath[m_index]->edict( ))
			ALERT( at_console, "Prev path %s", STRING( m_pNextPath[m_index]->pev->targetname ));
		ALERT( at_console, "\n" );
	}
}

LINK_ENTITY_TO_CLASS( infodecal, CDecal );
LINK_ENTITY_TO_CLASS( info_target, CInfoTarget );
LINK_ENTITY_TO_CLASS( info_teleport_destination, CPointEntity );
LINK_ENTITY_TO_CLASS( info_portal, CPortalSurface );
LINK_ENTITY_TO_CLASS( info_player_intermission, CPointEntity );
LINK_ENTITY_TO_CLASS( info_notnull, CPointEntity );
LINK_ENTITY_TO_CLASS( info_null, CNullEntity );
LINK_ENTITY_TO_CLASS( info_texlights, CNullEntity);
LINK_ENTITY_TO_CLASS( info_compile_parameters, CNullEntity);
LINK_ENTITY_TO_CLASS( info_intermission, CInfoIntermission );
LINK_ENTITY_TO_CLASS( info_camera, CInfoIntermission );
LINK_ENTITY_TO_CLASS( info_player_deathmatch, CBaseDMStart );
LINK_ENTITY_TO_CLASS( info_player_start, CPointEntity );
LINK_ENTITY_TO_CLASS( info_landmark, CPointEntity );