//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
//atoi(g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(edict()), "skin"));
//use this for custom change speed

//=========================================================
// path_corner - train node path.
//=========================================================
TYPEDESCRIPTION	CPathCorner::m_SaveData[] = 
{	DEFINE_FIELD( CPathCorner, m_pNextPath1, FIELD_CLASSPTR ),
	DEFINE_FIELD( CPathCorner, m_pNextPath2, FIELD_CLASSPTR ),
	DEFINE_FIELD( CPathCorner, m_pPrevPath, FIELD_CLASSPTR ),
};IMPLEMENT_SAVERESTORE(CPathCorner, CBaseLogic);

void CPathCorner :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "altpath"))
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;		
	}
	else CBaseLogic::KeyValue( pkvd );
}
LINK_ENTITY_TO_CLASS( path_corner, CPathCorner );

void CPathCorner :: Spawn( void )
{
	m_iState = STATE_ON;
	SetBits( pev->flags, FL_POINTENTITY );
}

void CPathCorner :: Link( void )
{
	CBaseEntity *pTarget;
	
	if ( FStringNull( pev->targetname ) ) return;

	if ( !FStringNull(pev->target) )
	{
		pTarget = UTIL_FindEntityByTargetname( NULL, STRING(pev->target));
		if(pTarget)
		{
			m_pNextPath1 = (CPathCorner*)pTarget;//valid path
			m_pNextPath1->SetPrev( this );
		}
		else DevMsg( "Dead end link %s\n", STRING(pev->target) );
	}
	if ( !FStringNull(pev->netname) )
	{
		pTarget = UTIL_FindEntityByTargetname( NULL, STRING(pev->netname));
		if(pTarget)
		{
			m_pNextPath2 = (CPathCorner*)pTarget;//valid path
			m_pNextPath2->SetPrev( this );
		}
		else DevMsg( "Dead end link %s\n", STRING(pev->netname) );
	}
}

void CPathCorner::UpdateTargets( void )
{
	UTIL_FireTargets(pev->message, this, this, USE_TOGGLE );

	if(pev->spawnflags & SF_CORNER_FIREONCE)
	{
		pev->message = iStringNull;	
		ClearBits( pev->spawnflags, SF_CORNER_FIREONCE );
	}
}

CBaseEntity *CPathCorner::ValidPath( CBaseEntity	*m_pPath )
{
	ASSERTSZ( m_pPath != this, "ValidPath: self path!\n");	

	if(m_pPath && m_pPath->edict() && m_pPath != this && ((CPathCorner *)m_pPath)->m_iState == STATE_ON)
		return m_pPath;
	return this;		
}

CBaseEntity *CPathCorner::GetNext( void )
{
	if(pev->team) return ValidPath( m_pNextPath2 );
	return ValidPath( m_pNextPath1 );	
}

void CPathCorner :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if (useType == USE_TOGGLE)
	{
		if(m_iState == STATE_ON) useType = USE_OFF;
		else useType = USE_ON;
	}
	if (useType == USE_ON) m_iState = STATE_ON;
	else if(useType == USE_OFF) m_iState = STATE_OFF;
	else if(useType == USE_SET) pev->team = !pev->team; //change path
	else if (useType == USE_SHOWINFO)
	{
		ALERT(at_console, "======/Xash Debug System/======\n");
		ALERT(at_console, "classname: %s\n", STRING(pev->classname));
		ALERT(at_console, "State: %s, wait %g\n", GetStringForState( GetState()), m_flWait);
		if(ValidPath(m_pPrevPath)) Msg("Prev path %s", STRING(m_pPrevPath->pev->targetname));
		if(ValidPath(m_pNextPath1))Msg("Next path %s", STRING(m_pNextPath1->pev->targetname));
		if(ValidPath(m_pNextPath2))Msg("Alt path %s", STRING(m_pNextPath2->pev->targetname));
		SHIFT;
	}
}


//=========================================================
// path_track - tracktrain node path.
//=========================================================
TYPEDESCRIPTION	CPathTrack::m_SaveData[] = 
{
	DEFINE_FIELD( CPathTrack, m_length, FIELD_FLOAT ),
	DEFINE_FIELD( CPathTrack, m_pnext, FIELD_CLASSPTR ),
	DEFINE_FIELD( CPathTrack, m_paltpath, FIELD_CLASSPTR ),
	DEFINE_FIELD( CPathTrack, m_pprevious, FIELD_CLASSPTR ),
	DEFINE_FIELD( CPathTrack, m_altName, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CPathTrack, CBaseEntity );
LINK_ENTITY_TO_CLASS( path_track, CPathTrack );

//
// Cache user-entity-field values until spawn is called.
//
void CPathTrack :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "altpath"))
	{
		m_altName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "turnspeed")) //LRC
	{
		if (pkvd->szValue[0]) // if the field is blank, don't set the spawnflag.
		{
			pev->spawnflags |= SF_PATH_AVELOCITY;
			UTIL_StringToVector( (float*)pev->avelocity, pkvd->szValue);
		}
		pkvd->fHandled = TRUE;
	}
	else
		CBaseLogic::KeyValue( pkvd );
}

void CPathTrack :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int on;

	// Use toggles between two paths
	if ( m_paltpath )
	{
		on = !FBitSet( pev->spawnflags, SF_PATH_ALTERNATE );
		if (useType == USE_TOGGLE)
		{
			if(on) useType = USE_OFF;
			else useType = USE_ON;
		}
		if (useType == USE_ON)ClearBits( pev->spawnflags, SF_PATH_ALTERNATE );
          	else if(useType == USE_OFF)SetBits( pev->spawnflags, SF_PATH_ALTERNATE );
	}
	else	// Use toggles between enabled/disabled
	{
		on = !FBitSet( pev->spawnflags, SF_PATH_DISABLED );
		if (useType == USE_TOGGLE)
		{
			if(on) useType = USE_OFF;
			else useType = USE_ON;
		}
		if (useType == USE_ON)ClearBits( pev->spawnflags, SF_PATH_DISABLED );
          	else if(useType == USE_OFF)SetBits( pev->spawnflags, SF_PATH_DISABLED );
	}
}


void CPathTrack :: Link( void  )
{
	CBaseEntity *pTarget;

	if( !FStringNull( pev->target ))
	{
		pTarget = UTIL_FindEntityByTargetname( NULL, STRING(pev->target) );

		if( pTarget )
		{
			m_pnext = (CPathTrack*)pTarget;
			m_pnext->SetPrevious( this );
		}
		else ALERT( at_console, "Dead end link %s\n", STRING(pev->target) );
	}

	// Find "alternate" path
	if ( m_altName )
	{
		pTarget = UTIL_FindEntityByTargetname( NULL, STRING(m_altName) );
		if ( pTarget )		// If no next pointer, this is the end of a path
		{
			m_paltpath = (CPathTrack*)pTarget;
			m_paltpath->SetPrevious( this );
		}
	}
}


void CPathTrack :: Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));

	m_pnext = NULL;
	m_pprevious = NULL;
// DEBUGGING CODE
#if PATH_SPARKLE_DEBUG
	SetThink( Sparkle );
	SetNextThink( 0.5 );
#endif
}


void CPathTrack::Activate( void )
{
	if ( !FStringNull( pev->targetname ) )		// Link to next, and back-link
		Link();

	CBaseLogic::Activate();
}

CBaseEntity	*CPathTrack :: ValidPath( CBaseEntity	*ppath, int testFlag )
{
	if ( !ppath )
		return NULL;

	if ( testFlag && FBitSet( ppath->pev->spawnflags, SF_PATH_DISABLED ) )
		return NULL;

	return ppath;
}


void CPathTrack :: Project( CBaseEntity *pstart, CBaseEntity *pend, Vector *origin, float dist )
{
	if ( pstart && pend )
	{
		Vector dir = (pend->pev->origin - pstart->pev->origin);
		dir = dir.Normalize();
		*origin = pend->pev->origin + dir * dist;
	}
}

CBaseEntity *CPathTrack::GetNext( void )
{
	if ( m_paltpath && FBitSet( pev->spawnflags, SF_PATH_ALTERNATE ) && !FBitSet( pev->spawnflags, SF_PATH_ALTREVERSE ) )
		return m_paltpath;
	
	return m_pnext;
}



CBaseEntity *CPathTrack::GetPrev( void )
{
	if ( m_paltpath && FBitSet( pev->spawnflags, SF_PATH_ALTERNATE ) && FBitSet( pev->spawnflags, SF_PATH_ALTREVERSE ) )
		return m_paltpath;
	
	return m_pprevious;
}



void CPathTrack::SetPrevious( CPathTrack *pprev )
{
	// Only set previous if this isn't my alternate path
	if ( pprev && !FStrEq( STRING(pprev->pev->targetname), STRING(m_altName) ) )
		m_pprevious = pprev;
}


// Assumes this is ALWAYS enabled
CBaseEntity *CPathTrack :: LookAhead( Vector *origin, float dist, int move )
{
	CBaseEntity *pcurrent;
	float originalDist = dist;
	
	pcurrent = this;
	Vector currentPos = *origin;

	if ( dist < 0 )		// Travelling backwards through path
	{
		dist = -dist;
		while ( dist > 0 )
		{
			Vector dir = pcurrent->pev->origin - currentPos;
			float length = dir.Length();
			if ( !length )
			{
				if ( !ValidPath(pcurrent->GetPrev(), move) ) 	// If there is no previous node, or it's disabled, return now.
				{
					if ( !move )
						Project( pcurrent->GetNext(), pcurrent, origin, dist );
					return NULL;
				}
				pcurrent = pcurrent->GetPrev();
			}
			else if ( length > dist )	// enough left in this path to move
			{
				*origin = currentPos + (dir * (dist / length));
				return pcurrent;
			}
			else
			{
				dist -= length;
				currentPos = pcurrent->pev->origin;
				*origin = currentPos;
				if ( !ValidPath(pcurrent->GetPrev(), move) )	// If there is no previous node, or it's disabled, return now.
					return NULL;

				pcurrent = pcurrent->GetPrev();
			}
		}
		*origin = currentPos;
		return pcurrent;
	}
	else 
	{
		while ( dist > 0 )
		{
			if ( !ValidPath(pcurrent->GetNext(), move) )	// If there is no next node, or it's disabled, return now.
			{
				if ( !move )
					Project( pcurrent->GetPrev(), pcurrent, origin, dist );
				return NULL;
			}
			Vector dir = pcurrent->GetNext()->pev->origin - currentPos;
			float length = dir.Length();
			if ( !length  && !ValidPath( pcurrent->GetNext()->GetNext(), move ) )
			{
				if ( dist == originalDist ) // HACK -- up against a dead end
					return NULL;
				return pcurrent;
			}
			if ( length > dist )	// enough left in this path to move
			{
				*origin = currentPos + (dir * (dist / length));
				return pcurrent;
			}
			else
			{
				dist -= length;
				currentPos = pcurrent->GetNext()->pev->origin;
				pcurrent = pcurrent->GetNext();
				*origin = currentPos;
			}
		}
		*origin = currentPos;
	}

	return pcurrent;
}

	
// Assumes this is ALWAYS enabled
CBaseEntity *CPathTrack :: Nearest( Vector origin )
{
	int			deadCount;
	float		minDist, dist;
	Vector		delta;
	CBaseEntity	*ppath, *pnearest;


	delta = origin - pev->origin;
	delta.z = 0;
	minDist = delta.Length();
	pnearest = this;
	ppath = GetNext();

	// Hey, I could use the old 2 racing pointers solution to this, but I'm lazy :)
	deadCount = 0;
	while ( ppath && ppath != this )
	{
		deadCount++;
		if ( deadCount > 9999 )
		{
			ALERT( at_error, "Bad sequence of path_tracks from %s", STRING(pev->targetname) );
			return NULL;
		}
		delta = origin - ppath->pev->origin;
		delta.z = 0;
		dist = delta.Length();
		if ( dist < minDist )
		{
			minDist = dist;
			pnearest = ppath;
		}
		ppath = ppath->GetNext();
	}
	return pnearest;
}


CPathTrack *CPathTrack::Instance( edict_t *pent )
{ 
	if ( FClassnameIs( pent, "path_track" ) )
		return (CPathTrack *)GET_PRIVATE(pent);
	return NULL;
}


	// DEBUGGING CODE
#if PATH_SPARKLE_DEBUG
void CPathTrack :: Sparkle( void )
{

	SetNextThink( 0.2 );
	if ( FBitSet( pev->spawnflags, SF_PATH_DISABLED ) )
		UTIL_ParticleEffect(pev->origin, Vector(0,0,100), 210, 10);
	else
		UTIL_ParticleEffect(pev->origin, Vector(0,0,100), 84, 10);
}
#endif