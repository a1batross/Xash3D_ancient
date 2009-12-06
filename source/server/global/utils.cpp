//=======================================================================
//			Copyright (C) Shambler Team 2005
//		    	     utils.cpp - Utility code. 
//			  Really not optional after all.			    
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "defaults.h"
#include "saverestore.h"
#include <time.h>
#include "shake.h"
#include "decals.h"
#include "player.h"
#include "baseweapon.h"
#include "gamerules.h"
#include "client.h"
#include "event_api.h"

FILE_GLOBAL char st_szNextMap[MAP_MAX_NAME];
FILE_GLOBAL char st_szNextSpot[MAP_MAX_NAME];
extern DLL_GLOBAL BOOL NewLevel;
int giAmmoIndex = 0;
BOOL CanAffect;

static const float bytedirs[NUMVERTEXNORMALS][3] =
{
#include "anorms.h"
};

//=========================================================
//	COM_Functions (text files parsing)
//=========================================================
#define COM_Format(x)	va_list szCommand;			\
			static char value[1024];		\
			va_start( szCommand, x );		\
			vsprintf( value, x, szCommand );	\
			va_end( szCommand ); 
 
void Msg( char *message, ... )
{
	COM_Format( message );
	g_engfuncs.pfnAlertMessage( at_console, "%s", value );
}
          
void DevMsg( char *message, ... ) 
{                                  
	COM_Format( message );
	g_engfuncs.pfnAlertMessage( at_aiconsole, "%s", value );
}

DLL_GLOBAL int DirToBits( const Vector dir )
{
	int	i, best = 0;
	float	d, bestd = 0;
	BOOL	normalized = FALSE;

	if( dir == g_vecZero )
		return NUMVERTEXNORMALS;

	if(( dir.x * dir.x + dir.y * dir.y + dir.z * dir.z ) == 1 )
		normalized = TRUE;

	for( i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		d = (dir.x * bytedirs[i][0] + dir.y * bytedirs[i][1] + dir.z * bytedirs[i][2] );
		if(( d == 1 ) && normalized )
			return i;
		if( d > bestd )
		{
			bestd = d;
			best = i;
		}
	}
	return best;
}

char *COM_ParseToken( const char **data )
{
	char *com_token = COM_Parse( data );

	// debug
//	ALERT( at_console, "ParseToken: %s\n", com_token );

	return com_token;	          
}

void COM_FreeFile( char *buffer )
{
	if( buffer ) FREE_FILE( buffer );
}

//=========================================================
//	check for null ents
//=========================================================

inline BOOL FNullEnt(EOFFSET eoffset)		{ return eoffset == 0; }
inline BOOL FNullEnt(const edict_t* pent)	{ return pent == NULL || FNullEnt(OFFSET(pent)); }
inline BOOL FNullEnt(entvars_t* pev)		{ return pev == NULL || FNullEnt(OFFSET(pev)); }
inline BOOL FNullEnt( CBaseEntity *ent )	{ return ent == NULL || FNullEnt( ent->edict()); }

//=========================================================
//	calculate brush model origin
//=========================================================
Vector VecBModelOrigin( entvars_t* pevBModel )
{
	return (pevBModel->absmin + pevBModel->absmax) * 0.5;
}

BOOL FClassnameIs(CBaseEntity *pEnt, const char* szClassname)
{ 
	return FStrEq(STRING(pEnt->pev->classname), szClassname);
}

//========================================================================
// UTIL_FireTargets - supported prefix "+", "-", "!", ">", "<", "?".
// supported also this and self pointers - e.g. "fadein(mywall)"
//========================================================================
void UTIL_FireTargets( int targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	//overload version UTIL_FireTargets
	if (!targetName) return;//no execute code, if target blank
	
	UTIL_FireTargets( STRING(targetName), pActivator, pCaller, useType, value);	
}

void UTIL_FireTargets( const char *targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	
	const char *inputTargetName = targetName;
	CBaseEntity *inputActivator = pActivator;
	CBaseEntity *pTarget = NULL;
	int i,j, found = false;
	char szBuf[80];

	if ( !targetName )return;

	//HACKHACK
	if(FStrEq(targetName, "tr_endchange" ))
	{
		SERVER_COMMAND("game_over\n");
		return;
	}
	
	if (targetName[0] == '+')
	{
		targetName++;
		useType = USE_ON;
	}
	else if (targetName[0] == '-')
	{
		targetName++;
		useType = USE_OFF;
	}
	else if (targetName[0] == '!')
	{
		targetName++;
		useType = USE_REMOVE;
	}
 	else if (targetName[0] == '<')
	{
		targetName++;
		useType = USE_SET;
	}
	else if (targetName[0] == '>')
	{
		targetName++;
		useType = USE_RESET;
	}	
	else if (targetName[0] == '?')
	{
		targetName++;
		useType = USE_SHOWINFO;
	}

	pTarget = UTIL_FindEntityByTargetname(pTarget, targetName, pActivator);

	if( !pTarget )//smart field name ?
	{
		//try to extract value from name (it's more usefully than "locus" specifier)
		for (i = 0; targetName[i]; i++)
		{
			if (targetName[i] == '.')//value specifier
			{
				value = atof(&targetName[i+1]);
				sprintf(szBuf, targetName);
				szBuf[i] = 0;
				targetName = szBuf;
				pTarget = UTIL_FindEntityByTargetname(NULL, targetName, inputActivator);						
				break;
			}
		}
		if( !pTarget )//try to extract activator specified
		{
			for (i = 0; targetName[i]; i++)
			{
				if (targetName[i] == '(')
				{
					i++;
					for (j = i; targetName[j]; j++)
					{
						if (targetName[j] == ')')
						{
							strncpy(szBuf, targetName+i, j-i);
							szBuf[j-i] = 0;
							pActivator = UTIL_FindEntityByTargetname(NULL, szBuf, inputActivator);
							if (!pActivator) return; //it's a locus specifier, but the locus is invalid.
							found = true;
							break;
						}
					}
					if (!found) ALERT(at_error, "Missing ')' in targetname: %s", inputTargetName);
					break;
				}
			}
	         		if (!found) return; // no, it's not a locus specifier.

			strncpy(szBuf, targetName, i-1);
			szBuf[i-1] = 0;
			targetName = szBuf;
			pTarget = UTIL_FindEntityByTargetname(NULL, targetName, inputActivator);

			if (!pTarget)return; // it's a locus specifier all right, but the target's invalid.
		}
	}

	DevMsg( "Firing: (%s) with %s and value %g\n", targetName, GetStringForUseType( useType ), value );

	do //start firing targets
	{
		if ( !(pTarget->pev->flags & FL_KILLME) )	// Don't use dying ents
		{
			if (useType == USE_REMOVE) UTIL_Remove( pTarget );
			else pTarget->Use( pActivator, pCaller, useType, value );
		}
		pTarget = UTIL_FindEntityByTargetname(pTarget, targetName, inputActivator);

	} while (pTarget);
}

//=========================================================
// UTIL_StripToken - for redundant keynames
//=========================================================
void UTIL_StripToken( const char *pKey, char *pDest )
{
	int i = 0;

	while ( pKey[i] && pKey[i] != '#' )
	{
		pDest[i] = pKey[i];
		i++;
	}
	pDest[i] = 0;
}


char* GetStringForUseType( USE_TYPE useType )
{
	switch(useType)
	{
		case USE_ON: return "USE_ON";
		case USE_OFF: return "USE_OFF";
		case USE_TOGGLE: return "USE_TOGGLE";
		case USE_REMOVE: return "USE_REMOVE";
		case USE_SET: return "USE_SET";
		case USE_RESET: return "USE_RESET";
		case USE_SHOWINFO: return "USE_SHOWINFO";
	default:
		return "UNKNOWN USE_TYPE!";
	}
}

char* GetStringForState( STATE state )
{
	switch(state)
	{
		case STATE_ON: return "ON";
		case STATE_OFF: return "OFF";
		case STATE_TURN_ON: return "TURN ON";
		case STATE_TURN_OFF: return "TURN OFF";
		case STATE_IN_USE: return "IN USE";
		case STATE_DEAD: return "DEAD";
	default:
		return "UNKNOWN STATE!";
	}
}

char* GetStringForDecalName( int decalname )
{
	char *name = "";
	int m_decal = UTIL_LoadDecalPreset( decalname );
	switch(m_decal)
	{
	case 1: name = gDecals[ DECAL_GUNSHOT1 + RANDOM_LONG(0,4)].name; break;
	case 2: name = gDecals[ DECAL_BLOOD1 + RANDOM_LONG(0,5)].name; break;
	case 3: name = gDecals[ DECAL_YBLOOD1 + RANDOM_LONG(0,5)].name; break;
	case 4: name = gDecals[ DECAL_GLASSBREAK1 + RANDOM_LONG(0,2)].name; break;
	case 5: name = gDecals[ DECAL_BIGSHOT1 + RANDOM_LONG(0,4)].name; break;
	case 6: name = gDecals[ DECAL_SCORCH1 + RANDOM_LONG(0,1)].name; break;
	case 7: name = gDecals[ DECAL_SPIT1 + RANDOM_LONG(0,1)].name; break;
	default: name = (char *)STRING( decalname ); break;
          }
	return name;
}

void PrintStringForDamage( int dmgbits )
{
	char szDmgBits[256];
	strcat(szDmgBits, "DAMAGE: " );
	if( dmgbits & DMG_GENERIC) strcat(szDmgBits, "Generic " );
	if( dmgbits & DMG_CRUSH) strcat(szDmgBits, "Crush "  );
	if( dmgbits & DMG_BULLET) strcat(szDmgBits, "Bullet "   );
	if( dmgbits & DMG_SLASH) strcat(szDmgBits, "Slash " );
	if( dmgbits & DMG_BURN) strcat(szDmgBits, "Burn " );
	if( dmgbits & DMG_FREEZE) strcat(szDmgBits, "Freeze "  );
	if( dmgbits & DMG_FALL) strcat(szDmgBits, "Fall " );
	if( dmgbits & DMG_BLAST) strcat(szDmgBits, "Blast "  );
	if( dmgbits & DMG_CLUB) strcat(szDmgBits, "Club "   );
	if( dmgbits & DMG_SHOCK) strcat(szDmgBits, "Shock " );
	if( dmgbits & DMG_SONIC) strcat(szDmgBits, "Sonic " );
	if( dmgbits & DMG_ENERGYBEAM) strcat(szDmgBits, "Energy Beam " );
	if( dmgbits & DMG_NEVERGIB) strcat(szDmgBits, "Never Gib " );
	if( dmgbits & DMG_ALWAYSGIB) strcat(szDmgBits, "Always Gib " );
	if( dmgbits & DMG_DROWN) strcat(szDmgBits, "Drown " );
	if( dmgbits & DMG_PARALYZE) strcat(szDmgBits, "Paralyze Gas " );
	if( dmgbits & DMG_NERVEGAS) strcat(szDmgBits, "Nerve Gas " );
	if( dmgbits & DMG_POISON) strcat(szDmgBits, "Poison "  );
	if( dmgbits & DMG_RADIATION) strcat(szDmgBits, "Radiation " );
	if( dmgbits & DMG_DROWNRECOVER) strcat(szDmgBits, "Drown Recover "  );
	if( dmgbits & DMG_ACID) strcat(szDmgBits, "Acid "   );
	if( dmgbits & DMG_SLOWBURN) strcat(szDmgBits, "Slow Burn " );
	if( dmgbits & DMG_SLOWFREEZE) strcat(szDmgBits, "Slow Freeze " );
	if( dmgbits & DMG_MORTAR) strcat(szDmgBits, "Mortar "  );
	if( dmgbits & DMG_NUCLEAR) strcat(szDmgBits, "Nuclear Explode " );		
          Msg("%s\n", szDmgBits );
}
char* GetContentsString( int contents )
{
	switch(contents)
	{
		case -1: return "EMPTY";
		case -2: return "SOLID";
		case -3: return "WATER";
		case -4: return "SLIME";
		case -5: return "LAVA";
		case -6: return "SKY";
		case -16: return "LADDER";
		case -17: return "FLYFIELD";
		case -18: return "GRAVITY_FLYFIELD";
		case -19: return "FOG";
		case -20: return "SPECIAL 1";
		case -21: return "SPECIAL 2";
		case -22: return "SPECIAL 3";
	default:
		return "NO CONTENTS!";
	}
}

char* GetStringForGlobalState( GLOBALESTATE state )
{
	switch(state)
	{
		case GLOBAL_ON: return "GLOBAL ON";
		case GLOBAL_OFF: return "GLOBAL OFF";
		case GLOBAL_DEAD: return "GLOBAL DEAD";
	default:
		return "UNKNOWN STATE!";
	}
}

void UTIL_Remove( CBaseEntity *pEntity )
{
	if( !pEntity ) return;
 
	pEntity->UpdateOnRemove();
	pEntity->pev->flags |= FL_KILLME;
	pEntity->pev->targetname = 0;
	pEntity->pev->health = 0;
}

void UTIL_AngularVector( CBaseEntity *pEnt )
{
	Vector movedir;

	UTIL_MakeVectors( pEnt->pev->angles );
	movedir.z = fabs(gpGlobals->v_forward.x);
	movedir.x = fabs(gpGlobals->v_forward.y);
	movedir.y = fabs(gpGlobals->v_forward.z);

	pEnt->pev->movedir = movedir;
	pEnt->pev->angles = g_vecZero;
}

void UTIL_LinearVector( CBaseEntity *pEnt )
{
	if( pEnt->pev->angles == Vector( 0, -1, 0 ))
	{
		pEnt->pev->movedir = Vector( 0, 0, 1 );
	}
	else if( pEnt->pev->angles == Vector( 0, -2, 0 ))
	{
		pEnt->pev->movedir = Vector( 0, 0, -1);
	}
	else
	{
		UTIL_MakeVectors( pEnt->pev->angles );
		pEnt->pev->movedir = gpGlobals->v_forward;
	}
	pEnt->pev->angles = g_vecZero;
}

Vector UTIL_GetAngleDistance( Vector vecAngles, float distance )
{
	//set one length vector
	if(vecAngles.x != 0) vecAngles.x = 1;
	if(vecAngles.y != 0) vecAngles.y = 1;
	if(vecAngles.z != 0) vecAngles.z = 1;
	return vecAngles * distance;
}

Vector UTIL_RandomVector(void)
{
	Vector out;
	out.x = RANDOM_FLOAT(-1.0, 1.0);
	out.y = RANDOM_FLOAT(-1.0, 1.0);
	out.z = RANDOM_FLOAT(-1.0, 1.0);
	return out;
}

Vector UTIL_RandomVector( Vector vmin, Vector vmax )
{
	Vector out;
	out.x = RANDOM_FLOAT( vmin.x, vmax.x );
	out.y = RANDOM_FLOAT( vmin.y, vmax.y );
	out.z = RANDOM_FLOAT( vmin.z, vmax.z );
	return out;
}

CBaseEntity *UTIL_FindGlobalEntity( string_t classname, string_t globalname )
{
	CBaseEntity *pReturn = UTIL_FindEntityByString( NULL, "globalname", STRING(globalname) );
	if ( pReturn )
	{
		if( !FClassnameIs( pReturn->pev, STRING( classname )))
		{
			ALERT( at_console, "Global entity found %s, wrong class %s\n", STRING(globalname), STRING(pReturn->pev->classname) );
			pReturn = NULL;
		}
	}

	return pReturn;
}

void UTIL_FindBreakable( CBaseEntity *Brush )
{
	Vector mins = Brush->pev->absmin;
	Vector maxs = Brush->pev->absmax;
	mins.z = Brush->pev->absmax.z;
	maxs.z += 8;

	CBaseEntity *pList[256];
	int count = UTIL_EntitiesInBox( pList, 256, mins, maxs, FL_ONGROUND );
	if ( count )
	{
		for ( int i = 0; i < count; i++ )
		{
			ClearBits( pList[i]->pev->flags, FL_ONGROUND );
			pList[i]->pev->groundentity = NULL;
		}
	}
}

CBaseEntity *UTIL_FindEntityForward( CBaseEntity *pMe )
{
	TraceResult tr;

	UTIL_MakeVectors( pMe->pev->viewangles );
	UTIL_TraceLine( pMe->pev->origin + pMe->pev->view_ofs, pMe->pev->origin + pMe->pev->view_ofs + gpGlobals->v_forward * 8192, dont_ignore_monsters, pMe->edict(), &tr );
	if( tr.flFraction != 1.0 && !FNullEnt( tr.pHit ))
	{
		CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
		return pHit;
	}
	return NULL;
}

void UTIL_FindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
{
	int			i, j, k;
	float		distance;
	float		*minmaxs[2] = {mins, maxs};
	TraceResult tmpTrace;
	Vector		vecHullEnd = tr.vecEndPos;
	Vector		vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace );
	if ( tmpTrace.flFraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace );
				if ( tmpTrace.flFraction < 1.0 )
				{
					float thisDistance = (tmpTrace.vecEndPos - vecSrc).Length();
					if ( thisDistance < distance )
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}

edict_t *UTIL_FindLandmark( string_t iLandmarkName )
{
	return UTIL_FindLandmark( STRING( iLandmarkName ));
}

edict_t *UTIL_FindLandmark( const char *pLandmarkName )
{
	CBaseEntity *pLandmark;

	if( FStringNull( pLandmarkName ) || FStrEq( pLandmarkName, "" ))
		return NULL; // landmark not specified

	pLandmark = UTIL_FindEntityByTargetname( NULL, pLandmarkName );
	while( pLandmark )
	{
		// Found the landmark
		if( FClassnameIs( pLandmark->pev, "info_landmark" ))
			return ENT( pLandmark->pev );
		else pLandmark = UTIL_FindEntityByTargetname( pLandmark, pLandmarkName );
	}

	Msg( "ERROR: Can't find landmark %s\n", pLandmarkName );
	return NULL;
}

int UTIL_FindTransition( CBaseEntity *pEntity, string_t iVolumeName )
{
	return UTIL_FindTransition( pEntity, STRING( iVolumeName ));
}

int UTIL_FindTransition( CBaseEntity *pEntity, const char *pVolumeName )
{
	CBaseEntity *pVolume;

	if( pEntity->ObjectCaps() & FCAP_FORCE_TRANSITION ) return 1;

	// if you're following another entity, follow it through the transition (weapons follow the player)
	if( pEntity->pev->movetype == MOVETYPE_FOLLOW && pEntity->pev->aiment != NULL )
	{
		pEntity = CBaseEntity::Instance( pEntity->pev->aiment );
	}

	int inVolume = 1;	// Unless we find a trigger_transition, everything is in the volume

	pVolume = UTIL_FindEntityByTargetname( NULL, pVolumeName );
	while( pVolume )
	{
		if( FClassnameIs( pVolume->pev, "trigger_transition" ))
		{
			if( pVolume->Intersects( pEntity )) // it touches one, it's in the volume
				return 1;
			else inVolume = 0;	// found a trigger_transition, but I don't intersect it
		}
		pVolume = UTIL_FindEntityByTargetname( pVolume, pVolumeName );
	}
	return inVolume;
}

/*
========================================================================
 UTIL_FindClientTransitions - returns list of client attached entites
 e.g. weapons, items and other followed entities
========================================================================
*/
edict_t *UTIL_FindClientTransitions( edict_t *pClient )
{
	edict_t	*pEdict, *chain;
	int	i;

	chain = NULL;
	pClient->v.chain = chain;	// client is always add to tail of chain
	chain = pClient;

	if( !pClient || pClient->free )
		return chain;

	for( i = 0; i < gpGlobals->numEntities; i++ )
	{
		pEdict = INDEXENT( i );
		if( pEdict->free ) continue;
		if( pEdict->v.movetype == MOVETYPE_FOLLOW && pEdict->v.aiment == pClient )
		{
			pEdict->v.chain = chain;
			chain = pEdict;
		}
	}
	return chain;
}

//========================================================================
//	UTIL_ClearPTR - clear all pointers before changelevel
//========================================================================
void UTIL_ClearPTR( void )
{
	CBaseEntity *pEntity = NULL;
	
	for( int i = 1; i < gpGlobals->numEntities; i++ )
	{
		edict_t *pEntityEdict = INDEXENT( i );
		if( pEntityEdict && !pEntityEdict->free && !FStringNull( pEntityEdict->v.globalname ))
		{
			pEntity = CBaseEntity::Instance( pEntityEdict );
		}
		if (!pEntity) continue;
		else pEntity->ClearPointers();
	}
}

//========================================================================
//	UTIL_ChangeLevel - used for loading next level
//========================================================================
void UTIL_ChangeLevel( string_t mapname, string_t spotname )
{
	UTIL_ChangeLevel( STRING( mapname ), STRING( spotname ));
}

void UTIL_ChangeLevel( const char *szNextMap, const char *szNextSpot )
{
	edict_t	*pentLandmark;

	ASSERT( !FStrEq( szNextMap, "" ));

	// don't work in deathmatch
	if( IsMultiplayer()) return;

	// some people are firing these multiple times in a frame, disable
	if( NewLevel ) return;

	CBaseEntity *pPlayer = UTIL_PlayerByIndex( 1 );
	if( !UTIL_FindTransition( pPlayer, szNextSpot ))
	{
		DevMsg( "Player isn't in the transition volume %s, aborting\n", szNextSpot );
		return;
	}

	// this object will get removed in the call to CHANGE_LEVEL, copy the params into "safe" memory
	strcpy( st_szNextMap, szNextMap );
	st_szNextSpot[0] = 0; // Init landmark to NULL

	// look for a landmark entity
	pentLandmark = UTIL_FindLandmark( szNextSpot );
	if( !FNullEnt( pentLandmark ))
	{
		strcpy( st_szNextSpot, szNextSpot );
		gpGlobals->vecLandmarkOffset = VARS( pentLandmark )->origin;
	}
	
	// map must exist and contain info_player_start
	if( IS_MAP_VALID( st_szNextMap ))
	{
		UTIL_ClearPTR();
		ALERT( at_aiconsole, "CHANGE LEVEL: %s %s\n", st_szNextMap, st_szNextSpot );
		CHANGE_LEVEL( st_szNextMap, st_szNextSpot );
	}
	else ALERT( at_warning, "map %s not found!\n", st_szNextMap );	
	NewLevel = TRUE; // UTIL_ChangeLevel is called	
}

//========================================================================
// 			Parent system utils
//		       Used for physics code too
//========================================================================
void UTIL_MarkChild ( CBaseEntity *pEnt, BOOL correctSpeed, BOOL desired )
{
	if(desired) //post affect
	{
		SetBits(pEnt->pFlags, PF_DESIRED);
		if (CanAffect)
		{         //apply post affect now
			PostFrameAffect( pEnt );
			return;
		}
	}
	else
	{
		SetBits(pEnt->pFlags, PF_AFFECT);

		if (correctSpeed)
		       SetBits (pEnt->pFlags, PF_CORECTSPEED);
		else ClearBits (pEnt->pFlags, PF_CORECTSPEED);
	}
	LinkChild( pEnt );//link children
}

void UTIL_SetThink ( CBaseEntity *pEnt )
{
	SetBits (pEnt->pFlags, PF_SETTHINK);
	pEnt->DontThink();
	UTIL_MarkChild( pEnt );
}

void UTIL_SetPostAffect( CBaseEntity *pEnt )
{
	SetBits(pEnt->pFlags, PF_POSTAFFECT);
	UTIL_MarkChild( pEnt );
}

void UTIL_SetAction( CBaseEntity *pEnt )
{
	SetBits(pEnt->pFlags, PF_ACTION);
	UTIL_MarkChild( pEnt );
}
//========================================================================
// 			Assign origin and angles
//========================================================================
void UTIL_AssignOrigin( CBaseEntity *pEntity, const Vector vecOrigin, BOOL bInitiator)
{
	Vector vecDiff = vecOrigin - pEntity->pev->origin;
	
	UTIL_SetOrigin(pEntity, vecOrigin );
	
	if (bInitiator && pEntity->m_pParent)
	{
		pEntity->OffsetOrigin = pEntity->pev->origin - pEntity->m_pParent->pev->origin;
	}
	if (pEntity->m_pChild)
	{
		CBaseEntity* pChild = pEntity->m_pChild;

		Vector vecTemp;
		while (pChild)
		{
			if (pChild->pev->movetype != MOVETYPE_PUSH || pChild->pev->velocity == pEntity->pev->velocity)
			{
				UTIL_AssignOrigin( pChild, vecOrigin + pChild->OffsetOrigin, FALSE );
			}
			else
			{
				vecTemp = vecDiff + pChild->pev->origin;
				UTIL_AssignOrigin( pChild, vecTemp, FALSE );
			}
			pChild = pChild->m_pNextChild;
		}
	}
}
void UTIL_AssignAngles( CBaseEntity *pEntity, const Vector vecAngles, BOOL bInitiator)
{
	Vector vecDiff = vecAngles - pEntity->pev->angles;

	UTIL_SetAngles(pEntity, vecAngles);
	
	if (bInitiator && pEntity->m_pParent)
		pEntity->OffsetAngles = vecAngles - pEntity->m_pParent->pev->angles;

	if (pEntity->m_pChild) // now I've moved pEntity, does anything else have to move with it?
	{
		CBaseEntity* pChild = pEntity->m_pChild;
		Vector vecTemp;
		while (pChild)
		{
			if (pChild->pev->avelocity == pEntity->pev->avelocity)
				UTIL_AssignAngles( pChild, vecAngles + pChild->OffsetAngles, FALSE );
			else
			{
				vecTemp = vecDiff + pChild->pev->angles;
				UTIL_AssignAngles( pChild, vecTemp, FALSE );
			}
			pChild = pChild->m_pNextChild;
		}
	}
}

//========================================================================
// 			Set origin and angles
//========================================================================
void UTIL_MergePos ( CBaseEntity *pEnt, int loopbreaker )
{
	if (loopbreaker <= 0)return;
	if (!pEnt->m_pParent)return;
          
	Vector forward, right, up, vecOrg, vecAngles;
          
	UTIL_MakeVectorsPrivate( pEnt->m_pParent->pev->angles, forward, right, up );

	if(pEnt->m_pParent->pev->flags & FL_MONSTER)
		vecOrg = pEnt->PostOrigin = pEnt->m_pParent->pev->origin + (forward * pEnt->OffsetOrigin.x) + ( right * pEnt->OffsetOrigin.y) + (up * pEnt->OffsetOrigin.z);
	else	vecOrg = pEnt->PostOrigin = pEnt->m_pParent->pev->origin + (forward * pEnt->OffsetOrigin.x) + (-right * pEnt->OffsetOrigin.y) + (up * pEnt->OffsetOrigin.z);

	vecAngles = pEnt->PostAngles = pEnt->m_pParent->pev->angles + pEnt->OffsetAngles;
	SetBits(pEnt->pFlags, PF_POSTAFFECT | PF_DESIRED );

	if ( pEnt->m_pChild )
	{
		CBaseEntity *pMoving = pEnt->m_pChild;
		int sloopbreaker = MAX_CHILDS;
		while (pMoving)
		{
			UTIL_MergePos(pMoving, loopbreaker - 1 );
			pMoving = pMoving->m_pNextChild;
			sloopbreaker--;
			if (sloopbreaker <= 0)break;
		}
	}
          
	if(pEnt->pFlags & PF_MERGEPOS)
	{
		UTIL_AssignOrigin( pEnt, vecOrg );
		UTIL_AssignAngles( pEnt, vecAngles );
		ClearBits(pEnt->pFlags, PF_MERGEPOS);
	}
	if(pEnt->pFlags & PF_POSTORG)
	{
		pEnt->pev->origin = vecOrg;
		pEnt->pev->angles = vecAngles;
          }
}

void UTIL_ComplexRotate( CBaseEntity *pParent, CBaseEntity *pChild, const Vector &dest, float m_flTravelTime )
{
	float time = m_flTravelTime;
	Vector vel = pParent->pev->velocity;
	
	// Attempt at getting the train to rotate properly around the origin of the trackchange
	if ( time <= 0 ) return;

	Vector offset = pChild->pev->origin - pParent->pev->origin;
	Vector delta = dest - pParent->pev->angles;
	// Transform offset into local coordinates
	UTIL_MakeInvVectors( delta, gpGlobals );
	Vector local;
	local.x = DotProduct( offset, gpGlobals->v_forward );
	local.y = DotProduct( offset, gpGlobals->v_right );
	local.z = DotProduct( offset, gpGlobals->v_up );

	local = local - offset;
	pChild->pev->velocity = vel + (local * (1.0 / time));
	pChild->pev->avelocity = pParent->pev->avelocity;
}

void UTIL_SetOrigin( CBaseEntity *pEntity, const Vector &vecOrigin )
{
	SET_ORIGIN(ENT(pEntity->pev), vecOrigin );
}

void UTIL_SetAngles( CBaseEntity *pEntity, const Vector &vecAngles )
{
	pEntity->pev->angles = vecAngles;
}

void UTIL_SynchDoors( CBaseEntity *pEntity )
{
	CBaseDoor *pDoor = NULL;

	if ( !FStringNull( pEntity->pev->targetname ) )
	{
		while(1)
		{
			pDoor = (CBaseDoor *)UTIL_FindEntityByTargetname (pDoor, STRING(pEntity->pev->targetname));

			if ( pDoor != pEntity )
			{
				if (FNullEnt(pDoor)) break;

				if ( FClassnameIs ( pDoor, "func_door" ) && pDoor->m_flWait >= 0 )
				{
					if (pDoor->pev->velocity == pEntity->pev->velocity)
					{
						UTIL_SetOrigin( pDoor, pEntity->pev->origin );
						UTIL_SetVelocity( pDoor, g_vecZero );
					}
					if (pDoor->GetState() == STATE_TURN_ON) pDoor->DoorGoDown();
					else if (pDoor->GetState() == STATE_TURN_OFF) pDoor->DoorGoUp();
				}
				if( FClassnameIs ( pDoor, "func_door_rotating" )  && pDoor->m_flWait >= 0 )
				{
					if(pDoor->pev->avelocity == pEntity->pev->avelocity)
					{
						UTIL_SetAngles( pDoor, pEntity->pev->angles );
						UTIL_SetAvelocity( pDoor, g_vecZero );
					}
					if (pDoor->GetState() == STATE_TURN_ON) pDoor->DoorGoDown();
					else if (pDoor->GetState() == STATE_TURN_OFF) pDoor->DoorGoUp();
				}                                                            
			}
		}
	}
}

//========================================================================
// 			Set childs velocity and avelocity
//========================================================================
void UTIL_SetChildVelocity ( CBaseEntity *pEnt, const Vector vecSet, int loopbreaker )
{
	if (loopbreaker <= 0)return;
	if (!pEnt->m_pParent)return;

	Vector vecNew;
	vecNew = (pEnt->pev->velocity - pEnt->m_pParent->pev->velocity) + vecSet;

	if ( pEnt->m_pChild )
	{
		CBaseEntity *pMoving = pEnt->m_pChild;
		int sloopbreaker = MAX_CHILDS;
		while (pMoving)
		{
			UTIL_SetChildVelocity(pMoving, vecNew, loopbreaker - 1 );
			pMoving = pMoving->m_pNextChild;
			sloopbreaker--;
			if (sloopbreaker <= 0)break;
		}
	}
	pEnt->pev->velocity = vecNew;
}

void UTIL_SetVelocity ( CBaseEntity *pEnt, const Vector vecSet )
{
	Vector vecNew;
	if (pEnt->m_pParent)
		vecNew = vecSet + pEnt->m_pParent->pev->velocity;
	else	vecNew = vecSet;

	if ( pEnt->m_pChild )
	{
		CBaseEntity *pMoving = pEnt->m_pChild;
		int sloopbreaker = MAX_CHILDS;
		while (pMoving)
		{
			UTIL_SetChildVelocity(pMoving, vecNew, MAX_CHILDS );
          		if(vecSet != g_vecZero)SetBits(pMoving->pFlags, PF_PARENTMOVE);
                              else ClearBits(pMoving->pFlags, PF_PARENTMOVE);
                              
			pMoving = pMoving->m_pNextChild;
			sloopbreaker--;
			if (sloopbreaker <= 0)break;
		}
	}
	pEnt->pev->velocity = vecNew;
}

void UTIL_SetChildAvelocity ( CBaseEntity *pEnt, const Vector vecSet, int loopbreaker )
{
	if (loopbreaker <= 0)return;
	if (!pEnt->m_pParent)return;

	Vector vecNew = (pEnt->pev->avelocity - pEnt->m_pParent->pev->avelocity) + vecSet;
	
	if ( pEnt->m_pChild )
	{
		CBaseEntity *pMoving = pEnt->m_pChild;
		int sloopbreaker = MAX_CHILDS;
		while (pMoving)
		{
			UTIL_SetChildAvelocity(pMoving, vecNew, loopbreaker - 1 );
			pMoving = pMoving->m_pNextChild;
			sloopbreaker--;
			if (sloopbreaker <= 0)break;
		}
	}
	
	pEnt->pev->avelocity = vecNew;
}

void UTIL_SetAvelocity ( CBaseEntity *pEnt, const Vector vecSet )
{
	Vector vecNew;
	
	if (pEnt->m_pParent)
		vecNew = vecSet + pEnt->m_pParent->pev->avelocity;
	else	vecNew = vecSet;

	if ( pEnt->m_pChild )
	{
		CBaseEntity *pMoving = pEnt->m_pChild;
		int sloopbreaker = MAX_CHILDS;
		while (pMoving)
		{
                              UTIL_SetChildAvelocity(pMoving, vecNew, MAX_CHILDS );
			UTIL_MergePos( pMoving );
          		if(vecSet != g_vecZero)SetBits(pMoving->pFlags, PF_PARENTMOVE);
                              else ClearBits(pMoving->pFlags, PF_PARENTMOVE);
                              
			pMoving = pMoving->m_pNextChild;
			sloopbreaker--;
			if (sloopbreaker <= 0)break;
		}
	}
	pEnt->pev->avelocity = vecNew;
}

//========================================================================
// Precache and set resources - add check for present or invalid name
// NOTE: game will not crashed if model not specified, this code is legacy
//========================================================================
void UTIL_SetModel( edict_t *e, string_t s, const char *c ) // set default model if not found
{
	if( FStringNull( s )) UTIL_SetModel( e, c );
	else UTIL_SetModel( e, s );
}

void UTIL_SetModel( edict_t *e, string_t model )
{
	UTIL_SetModel( e, STRING( model ));
}

void UTIL_SetModel( edict_t *e, const char *model )
{
	if( !model || !*model ) 
	{
		g_engfuncs.pfnSetModel( e, "models/common/null.mdl" );
		return;
	}

	// is this brush model?
	if( model[0] == '*' )
	{
		g_engfuncs.pfnSetModel( e, model );
		return;
	}

	// verify file exists
	if( FILE_EXISTS( model ))
	{
		g_engfuncs.pfnSetModel( e, model );
		return;
	}

	if( !strcmp( UTIL_FileExtension( model ), "mdl" ))
	{
		// this is model
		g_engfuncs.pfnSetModel(e, "models/common/error.mdl" );
	}
	else if( !strcmp( UTIL_FileExtension( model ), "spr" ))
	{
		// this is sprite
		g_engfuncs.pfnSetModel( e, "sprites/error.spr" );
	}
	else
	{
		// set null model
		g_engfuncs.pfnSetModel( e, "models/common/null.mdl" );
	}
}

int UTIL_PrecacheModel( string_t s, const char *e ) // precache default model if not found
{
	if( FStringNull( s ))
		return UTIL_PrecacheModel( e );
	return UTIL_PrecacheModel( s );
}

int UTIL_PrecacheModel( string_t s ){ return UTIL_PrecacheModel( STRING( s )); }
int UTIL_PrecacheModel( const char* s )
{
	if( FStringNull( s ))
	{	
		// set null model
		return g_sModelIndexNullModel;
	}

	// no need to precache brush
	if( s[0] == '*' ) return 0;

	if( FILE_EXISTS( s ))
	{
		return g_engfuncs.pfnPrecacheModel( s );
	}

	if( !strcmp( UTIL_FileExtension( s ), "mdl" ))
	{
		ALERT( at_warning, "model \"%s\" not found!\n", s );
		return g_sModelIndexErrorModel;
	}
	else if( !strcmp( UTIL_FileExtension( s ), "spr" ))
	{
		ALERT( at_warning, "sprite \"%s\" not found!\n", s );
		return g_sModelIndexErrorSprite;
	}
	else
	{
		ALERT( at_error, "invalid name \"%s\"!\n", s );
		return g_sModelIndexNullModel;
	}
}

//========================================================================
// Precaches the ammo and queues the ammo info for sending to clients
//========================================================================
void AddAmmoName( string_t iAmmoName )
{
	// make sure it's not already in the registry
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		if( !CBasePlayerWeapon::AmmoInfoArray[i].iszName ) continue;
		if( CBasePlayerWeapon::AmmoInfoArray[i].iszName == iAmmoName )
			return; // ammo already in registry, just quite
	}

	giAmmoIndex++;
	ASSERT( giAmmoIndex < MAX_AMMO_SLOTS );
	if( giAmmoIndex >= MAX_AMMO_SLOTS )
		giAmmoIndex = 0;

	CBasePlayerWeapon::AmmoInfoArray[giAmmoIndex].iszName = iAmmoName;
	CBasePlayerWeapon::AmmoInfoArray[giAmmoIndex].iId = giAmmoIndex; // yes, this info is redundant
}

//========================================================================
// Precaches entity from other entity
//========================================================================
void UTIL_PrecacheEntity( string_t szClassname ) { UTIL_PrecacheEntity( STRING( szClassname )); }
void UTIL_PrecacheEntity( const char *szClassname )
{
	edict_t	*pent;
	int	istr = ALLOC_STRING( szClassname );
          
	pent = CREATE_NAMED_ENTITY( istr );
	if( FNullEnt( pent )) return;

	CBaseEntity *pEntity = CBaseEntity::Instance( VARS( pent ));
	if( pEntity ) pEntity->Precache();
	REMOVE_ENTITY( pent );
}

//========================================================================
// Precaches aurora particle and set it
//========================================================================
int UTIL_PrecacheAurora( string_t s ) { return UTIL_PrecacheAurora( STRING( s )); }
int UTIL_PrecacheAurora( const char *s )
{
	char path[256];
	sprintf( path, "scripts/aurora/%s.aur", s );	

	if( FILE_EXISTS( path ))
	{
		return ALLOC_STRING( path );
	}
	else // otherwise
	{
		if( !s || !*s )Msg( "Warning: Aurora not specified!\n", s);
		else ALERT( at_warning, "aurora %s not found!\n", s );
		return MAKE_STRING( "scripts/aurora/error.aur" );
	} 
}

void UTIL_SetAurora( CBaseEntity *pAttach, int aur, int attachment )
{
	MESSAGE_BEGIN( MSG_ALL, gmsg.Particle );
		WRITE_BYTE( pAttach->entindex() );
		WRITE_STRING( STRING(aur) );
	MESSAGE_END();
}
//========================================================================
// Set client beams
//========================================================================
void UTIL_SetBeams( char *szFile, CBaseEntity *pStart, CBaseEntity *pEnd )
{
	MESSAGE_BEGIN( MSG_ALL, gmsg.Beams );
		WRITE_STRING( szFile );
		WRITE_BYTE( pStart->entindex());	// beam start entity
		WRITE_BYTE( pEnd->entindex() );	// beam end entity
	MESSAGE_END();
}

//========================================================================
// Precaches and play sound
//========================================================================
int UTIL_PrecacheSound( string_t s, const char *e ) // precache default model if not found
{
	if (FStringNull( s ))
		return UTIL_PrecacheSound( e );
	return UTIL_PrecacheSound( s );
}
int UTIL_PrecacheSound( string_t s ){ return UTIL_PrecacheSound( STRING( s )); }
int UTIL_PrecacheSound( const char* s )
{
	if( !s || !*s ) return MAKE_STRING( "common/null.wav" ); // set null sound
	if( *s == '!' ) return MAKE_STRING( s ); // sentence - just make string

	char path[256];
	sprintf( path, "sound/%s", s );

	// check file for existing
	if( FILE_EXISTS( path ))
	{
		g_engfuncs.pfnPrecacheSound( s );
		return MAKE_STRING( s );
	}

	if( !strcmp( UTIL_FileExtension( s ), "wav" ))
	{
		// this is sound
		ALERT( at_warning, "sound \"%s\" not found!\n", s );
	}
	else if( !strcmp( UTIL_FileExtension( s ), "ogg" ))
	{
		// this is sound
		ALERT( at_warning, "sound \"%s\" not found!\n", s );
	}
	else
	{
		// unknown format
		ALERT( at_error, "invalid name \"%s\"!\n", s );
	}

	g_engfuncs.pfnPrecacheSound("common/null.wav");
	return MAKE_STRING( "common/null.wav" );
}

int UTIL_LoadSoundPreset( string_t pString ) { return UTIL_LoadSoundPreset( STRING( pString )); }
int UTIL_LoadSoundPreset( const char *pString )
{
	// try to load direct sound path
	// so we supported 99 presets...
	int m_sound, namelen = strlen( pString ) - 1;

	if( namelen > 2 ) // yes, it's sound path
		m_sound = ALLOC_STRING( pString );
	else if( pString[0] == '!' ) // sentence
		m_sound = ALLOC_STRING( pString );
	else m_sound = atoi( pString ); // no, it's preset
	return m_sound;
}

int UTIL_LoadDecalPreset( string_t pString ) { return UTIL_LoadDecalPreset( STRING( pString )); }
int UTIL_LoadDecalPreset( const char *pString )
{
	// try to load direct sound path
	// so we supported 9 decal groups...
	int m_decal, namelen = strlen(pString) - 1;

	if( namelen > 1 ) // yes, it's decal name
		m_decal = ALLOC_STRING( pString );
	else m_decal = atoi( pString ); // no, it's preset
	return m_decal;
}

float UTIL_CalcDistance( Vector vecAngles )
{
	for(int i = 0; i < 3; i++ )
	{
		if(vecAngles[i] < -180) vecAngles[i] += 360; 
		else if(vecAngles[i] > 180) vecAngles[i] -= 360;
          }
          return vecAngles.Length();
}

//========================================================================
// Watches for pWatching enetity and rotate pWatcher entity angles
//========================================================================
void UTIL_WatchTarget( CBaseEntity *pWatcher, CBaseEntity *pTarget)
{
	Vector vecGoal = UTIL_VecToAngles( (pTarget->pev->origin - pWatcher->pev->origin).Normalize() );
	vecGoal.x = -vecGoal.x;
	
	if (pWatcher->pev->angles.y > 360) pWatcher->pev->angles.y -= 360;
	if (pWatcher->pev->angles.y < 0) pWatcher->pev->angles.y += 360;

	float dx = vecGoal.x - pWatcher->pev->angles.x;
	float dy = vecGoal.y - pWatcher->pev->angles.y;

	if (dx < -180) dx += 360;
	if (dx > 180) dx = dx - 360;

	if (dy < -180) dy += 360;
	if (dy > 180) dy = dy - 360;

	pWatcher->pev->avelocity.x = dx * pWatcher->pev->speed * gpGlobals->frametime;
	pWatcher->pev->avelocity.y = dy * pWatcher->pev->speed * gpGlobals->frametime;
}

float UTIL_Approach( float target, float value, float speed )
{
	float delta = target - value;

	if ( delta > speed )
		value += speed;
	else if ( delta < -speed )
		value -= speed;
	else 
		value = target;

	return value;
}


float UTIL_ApproachAngle( float target, float value, float speed )
{
	target = UTIL_AngleMod( target );
	value = UTIL_AngleMod( target );
	
	float delta = target - value;

	// Speed is assumed to be positive
	if ( speed < 0 )
		speed = -speed;

	if ( delta < -180 )
		delta += 360;
	else if ( delta > 180 )
		delta -= 360;

	if ( delta > speed )
		value += speed;
	else if ( delta < -speed )
		value -= speed;
	else 
		value = target;

	return value;
}


float UTIL_AngleDistance( float next, float cur )
{
	float delta = next - cur;

	while ( delta < -180 )
		delta += 360;
	while ( delta > 180 )
		delta -= 360;

	return delta;
}

BOOL UTIL_EntIsVisible( entvars_t* pev, entvars_t* pevTarget )
{
	Vector vecSpot1 = pev->origin + pev->view_ofs;
	Vector vecSpot2 = pevTarget->origin + pevTarget->view_ofs;
	TraceResult tr;

	UTIL_TraceLine( vecSpot1, vecSpot2, ignore_monsters, ENT(pev), &tr );

	if ( tr.fInOpen && tr.fInWater ) return FALSE; // sight line crossed contents
	if ( tr.flFraction == 1.0f ) return TRUE;
	return FALSE;
}

//========================================================================
// Place all system resourses here
//========================================================================
void UTIL_PrecacheResourse( void )
{
	// null and errors stuff
	g_sModelIndexErrorModel  = UTIL_PrecacheModel("models/common/error.mdl");//last crash point
          g_sModelIndexErrorSprite = UTIL_PrecacheModel("sprites/error.spr");
          g_sModelIndexNullModel   = UTIL_PrecacheModel("models/common/null.mdl"); 
          g_sModelIndexNullSprite  = UTIL_PrecacheModel("sprites/null.spr");

	// global sprites and models
	g_sModelIndexFireball = UTIL_PrecacheModel ("sprites/explode.spr");// fireball
	g_sModelIndexWExplosion = UTIL_PrecacheModel ("sprites/wxplode.spr");// underwater fireball
	g_sModelIndexSmoke = UTIL_PrecacheModel ("sprites/steam1.spr");// smoke
	g_sModelIndexBubbles = UTIL_PrecacheModel ("sprites/bubble.spr");//bubbles
	g_sModelIndexLaser = UTIL_PrecacheModel( "sprites/laserbeam.spr" );
	g_sModelIndexBloodSpray = UTIL_PrecacheModel ("sprites/bloodspray.spr");
	g_sModelIndexBloodDrop = UTIL_PrecacheModel ("sprites/blood.spr");

	// events
	m_usEjectBrass = UTIL_PrecacheEvent( "evEjectBrass" );
          
	// player items and weapons
	memset( CBasePlayerWeapon::ItemInfoArray, 0, sizeof( CBasePlayerWeapon::ItemInfoArray ));
	memset( CBasePlayerWeapon::AmmoInfoArray, 0, sizeof( CBasePlayerWeapon::AmmoInfoArray ));
	giAmmoIndex = 0;

	// custom precaches
	char *token;
	const char *pfile = (const char *)LOAD_FILE( "scripts/precache.txt", NULL );
	if( pfile )
	{
		char *afile = (char *)pfile;

		token = COM_ParseToken( &pfile );
			
		while( token )
		{
			if( !FStriCmp( token, "entity" )) 
			{                                          
				token = COM_ParseToken( &pfile );
				UTIL_PrecacheEntity( ALLOC_STRING( token ));
			}
			else if( !FStriCmp( token, "dmentity" )) 
			{                                          
				token = COM_ParseToken( &pfile );
				if( IsDeatchmatch()) UTIL_PrecacheEntity( ALLOC_STRING( token ));
			}
			else if( !FStriCmp( token, "model" )) 
			{                                          
				token = COM_ParseToken( &pfile );
				UTIL_PrecacheModel( ALLOC_STRING( token ));
			}
			else if( !FStriCmp( token, "dmmodel" )) 
			{                                          
				token = COM_ParseToken( &pfile );
				if( IsDeatchmatch()) UTIL_PrecacheModel( ALLOC_STRING( token ));
			}
			else if( !FStriCmp( token, "sound" )) 
			{                                          
				token = COM_ParseToken( &pfile );
				UTIL_PrecacheSound( ALLOC_STRING( token ));
			}
			else if( !FStriCmp( token, "dmsound" )) 
			{                                          
				token = COM_ParseToken( &pfile );
				if( IsDeatchmatch()) UTIL_PrecacheSound( ALLOC_STRING( token ));
			}
			else if( !FStriCmp( token, "aurora" )) 
			{                                          
				token = COM_ParseToken( &pfile );
				UTIL_PrecacheAurora( ALLOC_STRING( token ));
			}
			token = COM_ParseToken( &pfile );
		}
		COM_FreeFile( afile );
	}
}

BOOL IsMultiplayer( void )
{
	if( g_pGameRules->IsMultiplayer()) 
		return TRUE;
	return FALSE;
}

BOOL IsDeatchmatch( void )
{
	if( g_pGameRules->IsDeathmatch() ) 
		return TRUE;
	return FALSE;
}

float UTIL_WeaponTimeBase( void )
{
	return gpGlobals->time;
}

static unsigned int glSeed = 0; 

unsigned int seed_table[ 256 ] =
{
	28985, 27138, 26457, 9451, 17764, 10909, 28790, 8716, 6361, 4853, 17798, 21977, 19643, 20662, 10834, 20103,
	27067, 28634, 18623, 25849, 8576, 26234, 23887, 18228, 32587, 4836, 3306, 1811, 3035, 24559, 18399, 315,
	26766, 907, 24102, 12370, 9674, 2972, 10472, 16492, 22683, 11529, 27968, 30406, 13213, 2319, 23620, 16823,
	10013, 23772, 21567, 1251, 19579, 20313, 18241, 30130, 8402, 20807, 27354, 7169, 21211, 17293, 5410, 19223,
	10255, 22480, 27388, 9946, 15628, 24389, 17308, 2370, 9530, 31683, 25927, 23567, 11694, 26397, 32602, 15031,
	18255, 17582, 1422, 28835, 23607, 12597, 20602, 10138, 5212, 1252, 10074, 23166, 19823, 31667, 5902, 24630,
	18948, 14330, 14950, 8939, 23540, 21311, 22428, 22391, 3583, 29004, 30498, 18714, 4278, 2437, 22430, 3439,
	28313, 23161, 25396, 13471, 19324, 15287, 2563, 18901, 13103, 16867, 9714, 14322, 15197, 26889, 19372, 26241,
	31925, 14640, 11497, 8941, 10056, 6451, 28656, 10737, 13874, 17356, 8281, 25937, 1661, 4850, 7448, 12744,
	21826, 5477, 10167, 16705, 26897, 8839, 30947, 27978, 27283, 24685, 32298, 3525, 12398, 28726, 9475, 10208,
	617, 13467, 22287, 2376, 6097, 26312, 2974, 9114, 21787, 28010, 4725, 15387, 3274, 10762, 31695, 17320,
	18324, 12441, 16801, 27376, 22464, 7500, 5666, 18144, 15314, 31914, 31627, 6495, 5226, 31203, 2331, 4668,
	12650, 18275, 351, 7268, 31319, 30119, 7600, 2905, 13826, 11343, 13053, 15583, 30055, 31093, 5067, 761,
	9685, 11070, 21369, 27155, 3663, 26542, 20169, 12161, 15411, 30401, 7580, 31784, 8985, 29367, 20989, 14203,
	29694, 21167, 10337, 1706, 28578, 887, 3373, 19477, 14382, 675, 7033, 15111, 26138, 12252, 30996, 21409,
	25678, 18555, 13256, 23316, 22407, 16727, 991, 9236, 5373, 29402, 6117, 15241, 27715, 19291, 19888, 19847
};

unsigned int U_Random( void ) 
{ 
	glSeed *= 69069; 
	glSeed += seed_table[ glSeed & 0xff ];
 
	return ( ++glSeed & 0x0fffffff ); 
} 

void U_Srand( unsigned int seed )
{
	glSeed = seed_table[ seed & 0xff ];
}

/*
=====================
UTIL_SharedRandomLong
=====================
*/
int UTIL_SharedRandomLong( unsigned int seed, int low, int high )
{
	unsigned int range;

	U_Srand( (int)seed + low + high );

	range = high - low + 1;
	if ( !(range - 1) )
	{
		return low;
	}
	else
	{
		int offset;
		int rnum;

		rnum = U_Random();

		offset = rnum % range;

		return (low + offset);
	}
}

/*
=====================
UTIL_SharedRandomFloat
=====================
*/
float UTIL_SharedRandomFloat( unsigned int seed, float low, float high )
{
	//
	unsigned int range;

	U_Srand( (int)seed + *(int *)&low + *(int *)&high );

	U_Random();
	U_Random();

	range = high - low;
	if ( !range )
	{
		return low;
	}
	else
	{
		int tensixrand;
		float offset;

		tensixrand = U_Random() & 65535;

		offset = (float)tensixrand / 65536.0;

		return (low + offset * range );
	}
}

int g_groupmask = 0;
int g_groupop = 0;

// Normal overrides
void UTIL_SetGroupTrace( int groupmask, int op )
{
	g_groupmask		= groupmask;
	g_groupop		= op;

	ENGINE_SETGROUPMASK( g_groupmask, g_groupop );
}

void UTIL_UnsetGroupTrace( void )
{
	g_groupmask		= 0;
	g_groupop		= 0;

	ENGINE_SETGROUPMASK( 0, 0 );
}

// Smart version, it'll clean itself up when it pops off stack
UTIL_GroupTrace::UTIL_GroupTrace( int groupmask, int op )
{
	m_oldgroupmask	= g_groupmask;
	m_oldgroupop	= g_groupop;

	g_groupmask		= groupmask;
	g_groupop		= op;

	ENGINE_SETGROUPMASK( g_groupmask, g_groupop );
}

UTIL_GroupTrace::~UTIL_GroupTrace( void )
{
	g_groupmask		=	m_oldgroupmask;
	g_groupop		=	m_oldgroupop;

	ENGINE_SETGROUPMASK( g_groupmask, g_groupop );
}

#ifdef DEBUG
edict_t *DBG_EntOfVars( const entvars_t *pev )
{
	if( pev->pContainingEntity != NULL )
		return pev->pContainingEntity;

	ALERT( at_console, "entvars_t pContainingEntity is NULL, calling into engine" );
	edict_t* pent = (*g_engfuncs.pfnFindEntityByVars)((entvars_t*)pev);

	if( pent == NULL ) ALERT( at_console, "DAMN!  Even the engine couldn't FindEntityByVars!" );
	((entvars_t *)pev)->pContainingEntity = pent;
	return pent;
}
#endif //DEBUG


#ifdef	DEBUG
void DBG_AssertFunction( BOOL fExpr, const char* szExpr, const char* szFile, int szLine, const char* szMessage )
{
	if( fExpr ) return;

	char szOut[512];
	if( szMessage != NULL )
		sprintf( szOut, "ASSERT FAILED:\n %s \n(%s@%d)\n%s", szExpr, szFile, szLine, szMessage );
	else sprintf( szOut, "ASSERT FAILED:\n %s \n(%s@%d)", szExpr, szFile, szLine );
	HOST_ERROR( szOut );
}
#endif	// DEBUG

BOOL UTIL_GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerWeapon *pCurrentWeapon )
{
	return g_pGameRules->GetNextBestWeapon( pPlayer, pCurrentWeapon );
}

unsigned short UTIL_PrecacheEvent( const char *s )
{
	return g_engfuncs.pfnPrecacheEvent( 1, s );
}

void UTIL_PlaybackEvent( int flags, const edict_t *pInvoker, int ev_index, float delay, event_args_t *args )
{
	g_engfuncs.pfnPlaybackEvent( flags, pInvoker, ev_index, delay, args );
}

void UTIL_PlaybackEvent( int flags, const edict_t *pInvoker, int ev_index, float delay, Vector origin, Vector angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	event_args_t	args;

	args.flags = 0;
	if( !FNullEnt( pInvoker ))
		args.entindex = ENTINDEX( (edict_t *)pInvoker );
	else args.entindex = 0;
	origin.CopyToArray( args.origin );
	angles.CopyToArray( args.angles );
	// don't add velocity - engine will be reset it for some reasons
	args.fparam1 = fparam1;
	args.fparam2 = fparam2;
	args.iparam1 = iparam1;
	args.iparam2 = iparam2;
	args.bparam1 = bparam1;
	args.bparam2 = bparam2;

	UTIL_PlaybackEvent( flags, pInvoker, ev_index, delay, &args );
}

// ripped this out of the engine
float	UTIL_AngleMod(float a)
{
	if (a < 0)
	{
		a = a + 360 * ((int)(a / 360) + 1);
	}
	else if (a >= 360)
	{
		a = a - 360 * ((int)(a / 360));
	}
	// a = (360.0/65536) * ((int)(a*(65536/360.0)) & 65535);
	return a;
}

float UTIL_AngleDiff( float destAngle, float srcAngle )
{
	float delta;

	delta = destAngle - srcAngle;
	if ( destAngle > srcAngle )
	{
		if ( delta >= 180 )
			delta -= 360;
	}
	else
	{
		if ( delta <= -180 )
			delta += 360;
	}
	return delta;
}

Vector UTIL_VecToAngles( const Vector &vec )
{
	float	rgflVecOut[3];

	VEC_TO_ANGLES( vec, rgflVecOut );

	return Vector( rgflVecOut );
}
	
//LRC - pass in a normalised axis vector and a number of degrees, and this returns the corresponding
// angles value for an entity.
inline Vector UTIL_AxisRotationToAngles( const Vector &vecAxis, float flDegs )
{
	Vector vecTemp = UTIL_AxisRotationToVec( vecAxis, flDegs );
	float rgflVecOut[3];
	//ugh, mathsy.
	rgflVecOut[0] = asin(vecTemp.z) * (-180.0 / M_PI);
	rgflVecOut[1] = acos(vecTemp.x) * (180.0 / M_PI);
	if (vecTemp.y < 0)
		rgflVecOut[1] = -rgflVecOut[1];
	rgflVecOut[2] = 0; //for now
	return Vector(rgflVecOut);
}

//LRC - as above, but returns the position of point 1 0 0 under the given rotation
Vector UTIL_AxisRotationToVec( const Vector &vecAxis, float flDegs )
{
	float rgflVecOut[3];
	float flRads = flDegs * (M_PI / 180.0);
	float c = cos(flRads);
	float s = sin(flRads);
	float v = vecAxis.x * (1-c);
	//ugh, more maths. Thank goodness for internet geometry sites...
	rgflVecOut[0] = vecAxis.x*v + c;
	rgflVecOut[1] = vecAxis.y*v + vecAxis.z*s;
	rgflVecOut[2] = vecAxis.z*v - vecAxis.y*s;
	return Vector(rgflVecOut);
}
	
void UTIL_MoveToOrigin( edict_t *pent, const Vector &vecGoal, float flDist, int iMoveType )
{
	MOVE_TO_ORIGIN( pent, vecGoal, flDist, iMoveType ); 
}

int UTIL_EntitiesInBox( CBaseEntity **pList, int listMax, const Vector &mins, const Vector &maxs, int flagMask )
{
	edict_t		*pEdict = g_engfuncs.pfnPEntityOfEntIndex( 1 );
	CBaseEntity *pEntity;
	int			count;

	count = 0;

	if ( !pEdict )
		return count;

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		if ( pEdict->free )	// Not in use
			continue;
		
		if ( flagMask && !(pEdict->v.flags & flagMask) )	// Does it meet the criteria?
			continue;

		if ( mins.x > pEdict->v.absmax.x ||
			 mins.y > pEdict->v.absmax.y ||
			 mins.z > pEdict->v.absmax.z ||
			 maxs.x < pEdict->v.absmin.x ||
			 maxs.y < pEdict->v.absmin.y ||
			 maxs.z < pEdict->v.absmin.z )
			 continue;

		pEntity = CBaseEntity::Instance(pEdict);
		if ( !pEntity )
			continue;

		pList[ count ] = pEntity;
		count++;

		if ( count >= listMax )
			return count;
	}

	return count;
}


int UTIL_MonstersInSphere( CBaseEntity **pList, int listMax, const Vector &center, float radius )
{
	edict_t		*pEdict = g_engfuncs.pfnPEntityOfEntIndex( 1 );
	CBaseEntity *pEntity;
	int			count;
	float		distance, delta;

	count = 0;
	float radiusSquared = radius * radius;

	if ( !pEdict )
		return count;

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		if ( pEdict->free )	// Not in use
			continue;
		
		if ( !(pEdict->v.flags & (FL_CLIENT|FL_MONSTER)) )	// Not a client/monster ?
			continue;

		// Use origin for X & Y since they are centered for all monsters
		// Now X
		delta = center.x - pEdict->v.origin.x;//(pEdict->v.absmin.x + pEdict->v.absmax.x)*0.5;
		delta *= delta;

		if ( delta > radiusSquared )
			continue;
		distance = delta;
		
		// Now Y
		delta = center.y - pEdict->v.origin.y;//(pEdict->v.absmin.y + pEdict->v.absmax.y)*0.5;
		delta *= delta;

		distance += delta;
		if ( distance > radiusSquared )
			continue;

		// Now Z
		delta = center.z - (pEdict->v.absmin.z + pEdict->v.absmax.z)*0.5;
		delta *= delta;

		distance += delta;
		if ( distance > radiusSquared )
			continue;

		pEntity = CBaseEntity::Instance(pEdict);
		if ( !pEntity )
			continue;

		pList[ count ] = pEntity;
		count++;

		if ( count >= listMax )
			return count;
	}


	return count;
}


CBaseEntity *UTIL_FindEntityInSphere( CBaseEntity *pStartEntity, const Vector &vecCenter, float flRadius )
{
	edict_t	*pentEntity;

	if (pStartEntity)
		pentEntity = pStartEntity->edict();
	else
		pentEntity = NULL;

	pentEntity = FIND_ENTITY_IN_SPHERE( pentEntity, vecCenter, flRadius);

	if (!FNullEnt(pentEntity))
		return CBaseEntity::Instance(pentEntity);
	return NULL;
}

CBaseEntity *UTIL_FindPlayerInSphere( const Vector &vecCenter, float flRadius )
{
	edict_t	*pentEntity = NULL;

	pentEntity = FIND_ENTITY_IN_SPHERE( pentEntity, vecCenter, flRadius);
          while(!FNullEnt(pentEntity))
          {
		if(pentEntity->v.flags & FL_CLIENT) break; //found player
		pentEntity = FIND_ENTITY_IN_SPHERE( pentEntity, vecCenter, flRadius);
          }
	if (!FNullEnt(pentEntity))
		return CBaseEntity::Instance(pentEntity);
	return NULL;
}

CBasePlayer *UTIL_FindPlayerInPVS( edict_t *pent )
{
	edict_t *pentPlayer = FIND_CLIENT_IN_PVS( pent );
	CBasePlayer *pPlayer = NULL;

	if(!FNullEnt(pentPlayer)) //get pointer to player
		pPlayer = GetClassPtr((CBasePlayer *)VARS(pentPlayer));
	return pPlayer;
}

CBaseEntity *UTIL_FindEntityByString( CBaseEntity *pStartEntity, const char *szKeyword, const char *szValue )
{
	edict_t	*pentEntity;
	CBaseEntity *pEntity;

	if (pStartEntity)
		pentEntity = pStartEntity->edict();
	else
		pentEntity = NULL;

	for (;;)
	{
		// Don't change this to use UTIL_FindEntityByString!
		pentEntity = FIND_ENTITY_BY_STRING( pentEntity, szKeyword, szValue );

		// if pentEntity (the edict) is null, we're at the end of the entities. Give up.
		if (FNullEnt(pentEntity))
		{
			return NULL;
		}
		else
		{
			// ...but if only pEntity (the classptr) is null, we've just got one dud, so we try again.
			pEntity = CBaseEntity::Instance(pentEntity);
			if (pEntity)
				return pEntity;
		}
	}
}

CBaseEntity *UTIL_FindEntityByClassname( CBaseEntity *pStartEntity, const char *szName )
{
	return UTIL_FindEntityByString( pStartEntity, "classname", szName );
}

CBaseEntity *UTIL_FindEntityByTargetname( CBaseEntity *pStartEntity, const char *szName )
{
	return UTIL_FindEntityByString( pStartEntity, "targetname", szName );
}

CBaseEntity *UTIL_FindEntityByTargetname( CBaseEntity *pStartEntity, const char *szName, CBaseEntity *pActivator )
{
	return UTIL_FindEntityByTargetname( pStartEntity, szName );
}

CBaseEntity *UTIL_FindEntityByTarget( CBaseEntity *pStartEntity, const char *szName )
{
	return UTIL_FindEntityByString( pStartEntity, "target", szName );
}

CBaseEntity *UTIL_FindEntityGeneric( const char *szWhatever, Vector &vecSrc, float flRadius )
{
	CBaseEntity *pEntity = NULL;

	pEntity = UTIL_FindEntityByTargetname( NULL, szWhatever );
	if (pEntity)
		return pEntity;

	CBaseEntity *pSearch = NULL;
	float flMaxDist2 = flRadius * flRadius;
	while ((pSearch = UTIL_FindEntityByClassname( pSearch, szWhatever )) != NULL)
	{
		float flDist2 = (pSearch->pev->origin - vecSrc).Length();
		flDist2 = flDist2 * flDist2;
		if (flMaxDist2 > flDist2)
		{
			pEntity = pSearch;
			flMaxDist2 = flDist2;
		}
	}
	return pEntity;
}


// returns a CBaseEntity pointer to a player by index.  Only returns if the player is spawned and connected
// otherwise returns NULL
// Index is 1 based
CBaseEntity *UTIL_PlayerByIndex( int playerIndex )
{
	CBaseEntity *pPlayer = NULL;

	if ( playerIndex > 0 && playerIndex <= gpGlobals->maxClients )
	{
		edict_t *pPlayerEdict = INDEXENT( playerIndex );
		if ( pPlayerEdict && !pPlayerEdict->free )
		{
			pPlayer = CBaseEntity::Instance( pPlayerEdict );
		}
	}
	
	return pPlayer;
}


void UTIL_MakeVectors( const Vector &vecAngles )
{
	MAKE_VECTORS( vecAngles );
}


void UTIL_MakeAimVectors( const Vector &vecAngles )
{
	float rgflVec[3];
	vecAngles.CopyToArray(rgflVec);
	rgflVec[0] = -rgflVec[0];
	MAKE_VECTORS(rgflVec);
}


#define SWAP(a,b,temp)	((temp)=(a),(a)=(b),(b)=(temp))

void UTIL_MakeInvVectors( const Vector &vec, globalvars_t *pgv )
{
	MAKE_VECTORS(vec);

	float tmp;
	pgv->v_right = pgv->v_right * -1;

	SWAP(pgv->v_forward.y, pgv->v_right.x, tmp);
	SWAP(pgv->v_forward.z, pgv->v_up.x, tmp);
	SWAP(pgv->v_right.z, pgv->v_up.y, tmp);
}


void UTIL_EmitAmbientSound( edict_t *entity, const Vector &vecOrigin, const char *samp, float vol, float attenuation, int fFlags, int pitch )
{
	float rgfl[3];
	vecOrigin.CopyToArray(rgfl);

	if (samp && *samp == '!')
	{
		char name[32];
		if (SENTENCEG_Lookup(samp, name) >= 0)
			EMIT_AMBIENT_SOUND(entity, rgfl, name, vol, attenuation, fFlags, pitch);
	}
	else
		EMIT_AMBIENT_SOUND(entity, rgfl, samp, vol, attenuation, fFlags, pitch);
}

// Shake the screen of all clients within radius
// radius == 0, shake all clients
// UNDONE: Allow caller to shake clients not ONGROUND?
// UNDONE: Fix falloff model (disabled)?
// UNDONE: Affect user controls?
//LRC UNDONE: Work during trigger_camera?
//-----------------------------------------------------------------------------
// Compute shake amplitude
//-----------------------------------------------------------------------------
float ComputeShakeAmplitude( const Vector &center, const Vector &shakePt, float amplitude, float radius ) 
{
	if( radius <= 0 )
		return amplitude;

	float localAmplitude = -1;
	Vector delta = center - shakePt;
	float distance = delta.Length();

	if( distance <= radius )
	{
		// make the amplitude fall off over distance
		float flPerc = 1.0 - (distance / radius);
		localAmplitude = amplitude * flPerc;
	}

	return localAmplitude;
}

void UTIL_ScreenShake( const Vector &center, float amplitude, float frequency, float duration, float radius, ShakeCommand_t eCommand, BOOL bAirShake )
{
	int		i;
	float		localAmplitude;

	for( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		//
		// Only shake players that are on the ground.
		//
		if( !pPlayer || (!bAirShake && !FBitSet( pPlayer->pev->flags, FL_ONGROUND )))
		{
			continue;
		}

		localAmplitude = ComputeShakeAmplitude( center, pPlayer->pev->origin, amplitude, radius );

		// This happens if the player is outside the radius, in which case we should ignore 
		// all commands
		if( localAmplitude < 0 ) continue;

		if (( localAmplitude > 0 ) || ( eCommand == SHAKE_STOP ))
		{
			if ( eCommand == SHAKE_STOP ) localAmplitude = 0;

			MESSAGE_BEGIN( MSG_ONE, gmsg.Shake, NULL, pPlayer->edict() );
				WRITE_BYTE( eCommand );	// shake command (SHAKE_START, STOP, FREQUENCY, AMPLITUDE)
				WRITE_FLOAT( localAmplitude );// shake magnitude/amplitude
				WRITE_FLOAT( frequency );	// shake noise frequency
				WRITE_FLOAT( duration );	// shake lasts this long
			MESSAGE_END();
		}
	}
}

void UTIL_ScreenShake( const Vector &center, float amplitude, float frequency, float duration, float radius )
{
	UTIL_ScreenShake( center, amplitude, frequency, duration, radius, SHAKE_START, FALSE );
}

void UTIL_ScreenShakeAll( const Vector &center, float amplitude, float frequency, float duration )
{
	UTIL_ScreenShake( center, amplitude, frequency, duration, 0, SHAKE_START, FALSE );
}

void UTIL_ScreenFadeAll( const Vector &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	if(IsMultiplayer())
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			UTIL_ScreenFade( color, fadeTime, fadeHold, alpha, flags, i );
	}
	else UTIL_ScreenFade( color, fadeTime, fadeHold, alpha, flags );
}


void UTIL_ScreenFade( const Vector &color, float fadeTime, float fadeHold, int alpha, int flags, int playernum )
{
	CBasePlayer *pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( playernum );

	if ( pPlayer )
	{
		if ( flags & FFADE_CUSTOMVIEW )
		{
			if ( pPlayer->viewFlags == 0 ) return;
			ClearBits( flags, FFADE_CUSTOMVIEW ); // don't send this flag to engine!!!
		}

		if ( flags & FFADE_OUT && fadeHold == 0 )
			SetBits( flags, FFADE_STAYOUT ); 
		else ClearBits( flags, FFADE_STAYOUT );

		pPlayer->m_FadeColor = color;
		pPlayer->m_FadeAlpha = alpha;			
		pPlayer->m_iFadeFlags = flags;
		pPlayer->m_iFadeTime = fadeTime;
		pPlayer->m_iFadeHold = fadeHold;
		pPlayer->fadeNeedsUpdate = TRUE;
	}
}


void UTIL_HudMessage( CBaseEntity *pEntity, const hudtextparms_t &textparms, const char *pMessage )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsg.TempEntity, NULL, pEntity->edict() );
		WRITE_BYTE( TE_TEXTMESSAGE );
		WRITE_BYTE( textparms.channel & 0xFF );

		WRITE_SHORT( FixedSigned16( textparms.x, 1<<13 ) );
		WRITE_SHORT( FixedSigned16( textparms.y, 1<<13 ) );
		WRITE_BYTE( textparms.effect );

		WRITE_BYTE( textparms.r1 );
		WRITE_BYTE( textparms.g1 );
		WRITE_BYTE( textparms.b1 );
		WRITE_BYTE( textparms.a1 );

		WRITE_BYTE( textparms.r2 );
		WRITE_BYTE( textparms.g2 );
		WRITE_BYTE( textparms.b2 );
		WRITE_BYTE( textparms.a2 );

		WRITE_SHORT( FixedUnsigned16( textparms.fadeinTime, 1<<8 ) );
		WRITE_SHORT( FixedUnsigned16( textparms.fadeoutTime, 1<<8 ) );
		WRITE_SHORT( FixedUnsigned16( textparms.holdTime, 1<<8 ) );

		if ( textparms.effect == 2 )
			WRITE_SHORT( FixedUnsigned16( textparms.fxTime, 1<<8 ) );
		
		if ( strlen( pMessage ) < 512 )
		{
			WRITE_STRING( pMessage );
		}
		else
		{
			char tmp[512];
			strncpy( tmp, pMessage, 511 );
			tmp[511] = 0;
			WRITE_STRING( tmp );
		}
	MESSAGE_END();
}

void UTIL_HudMessageAll( const hudtextparms_t &textparms, const char *pMessage )
{
	int			i;

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
			UTIL_HudMessage( pPlayer, textparms, pMessage );
	}
}
					 
void UTIL_ClientPrintAll( int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	MESSAGE_BEGIN( MSG_ALL, gmsg.TextMsg );
		WRITE_BYTE( msg_dest );
		WRITE_STRING( msg_name );

		if ( param1 )
			WRITE_STRING( param1 );
		if ( param2 )
			WRITE_STRING( param2 );
		if ( param3 )
			WRITE_STRING( param3 );
		if ( param4 )
			WRITE_STRING( param4 );

	MESSAGE_END();
}

void ClientPrint( entvars_t *client, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	MESSAGE_BEGIN( MSG_ONE, gmsg.TextMsg, NULL, client );
		WRITE_BYTE( msg_dest );
		WRITE_STRING( msg_name );

		if ( param1 )
			WRITE_STRING( param1 );
		if ( param2 )
			WRITE_STRING( param2 );
		if ( param3 )
			WRITE_STRING( param3 );
		if ( param4 )
			WRITE_STRING( param4 );

	MESSAGE_END();
}

void UTIL_SayText( const char *pText, CBaseEntity *pEntity )
{
	if ( !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsg.SayText, NULL, pEntity->edict() );
		WRITE_BYTE( pEntity->entindex() );
		WRITE_STRING( pText );
	MESSAGE_END();
}

void UTIL_SayTextAll( const char *pText, CBaseEntity *pEntity )
{
	MESSAGE_BEGIN( MSG_ALL, gmsg.SayText, NULL );
		WRITE_BYTE( pEntity->entindex() );
		WRITE_STRING( pText );
	MESSAGE_END();
}


char *UTIL_dtos1( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

char *UTIL_dtos2( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

char *UTIL_dtos3( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

char *UTIL_dtos4( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

void UTIL_ShowMessage( const char *pString, CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsg.HudText, NULL, pEntity->edict() );
	WRITE_STRING( pString );
	MESSAGE_END();
}


void UTIL_ShowMessageAll( const char *pString )
{
	// loop through all players

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer ) UTIL_ShowMessage( pString, pPlayer );
	}
}
void UTIL_SetFogAll( Vector color, int iFadeTime, int iStartDist, int iEndDist )
{
	// loop through all players
         
	if(IsMultiplayer())
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			UTIL_SetFog( color, iFadeTime, iStartDist, iEndDist, i );
	}
	else UTIL_SetFog( color, iFadeTime, iStartDist, iEndDist );
}

void UTIL_SetFog( Vector color, int iFadeTime, int iStartDist, int iEndDist, int playernum )
{
	CBasePlayer *pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( playernum );
	if ( pPlayer )
	{
		if(pPlayer->m_FogFadeTime != 0) return;//fading in progress !!!TODO: make smooth re-fading
		if(IsMultiplayer()) iFadeTime = 0; //disable fading in multiplayer
		if( iFadeTime > 0 )
		{
			pPlayer->m_iFogEndDist = FOG_LIMIT;
			pPlayer->m_iFogFinalEndDist = iEndDist;
		}
		else if( iFadeTime < 0 )
		{
			pPlayer->m_iFogEndDist = iEndDist;
			pPlayer->m_iFogFinalEndDist = iEndDist;
		}
		else pPlayer->m_iFogEndDist = iEndDist;
		pPlayer->m_iFogStartDist = iStartDist;
		pPlayer->m_FogColor = color;
		pPlayer->m_FogFadeTime = iFadeTime;			
		pPlayer->m_flStartTime = gpGlobals->time;
		pPlayer->fogNeedsUpdate = TRUE;
	}
}

// Overloaded to add IGNORE_GLASS
void UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, IGNORE_GLASS ignoreGlass, edict_t *pentIgnore, TraceResult *ptr )
{
	TRACE_LINE( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE) | (ignoreGlass?0x100:0), pentIgnore, ptr );
}


void UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, edict_t *pentIgnore, TraceResult *ptr )
{
	TRACE_LINE( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE), pentIgnore, ptr );
}


void UTIL_TraceHull( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, int hullNumber, edict_t *pentIgnore, TraceResult *ptr )
{
	TRACE_HULL( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE), hullNumber, pentIgnore, ptr );
}

void UTIL_TraceModel( const Vector &vecStart, const Vector &vecEnd, int hullNumber, edict_t *pentModel, TraceResult *ptr )
{
	g_engfuncs.pfnTraceModel( vecStart, vecEnd, pentModel, ptr );
}


TraceResult UTIL_GetGlobalTrace( void )
{
	TraceResult	tr;

	tr.fAllSolid	= gpGlobals->trace_allsolid;
	tr.fStartSolid	= gpGlobals->trace_startsolid;
	tr.fInOpen    	= gpGlobals->trace_inopen;
	tr.fInWater   	= gpGlobals->trace_inwater;
	tr.flFraction 	= gpGlobals->trace_fraction;
	tr.flPlaneDist	= gpGlobals->trace_plane_dist;
	tr.pHit	  	= gpGlobals->trace_ent;
	tr.vecEndPos	= gpGlobals->trace_endpos;
	tr.vecPlaneNormal	= gpGlobals->trace_plane_normal;
	tr.iHitgroup	= gpGlobals->trace_hitgroup;

	return tr;
}

	
void UTIL_SetSize( entvars_t *pev, const Vector &vecMin, const Vector &vecMax )
{
	SET_SIZE( ENT(pev), vecMin, vecMax );
}
	
	
float UTIL_VecToYaw( const Vector &vec )
{
	return VEC_TO_YAW(vec);
}

void UTIL_SetEdictOrigin( edict_t *pEdict, const Vector &vecOrigin )
{
	SET_ORIGIN(pEdict, vecOrigin );
}

// 'links' the entity into the world


void UTIL_ParticleEffect( const Vector &vecOrigin, const Vector &vecDirection, ULONG ulColor, ULONG ulCount )
{
	PARTICLE_EFFECT( vecOrigin, vecDirection, (float)ulColor, (float)ulCount );
}


float UTIL_SplineFraction( float value, float scale )
{
	value = scale * value;
	float valueSquared = value * value;

	// Nice little ease-in, ease-out spline-like curve
	return 3 * valueSquared - 2 * valueSquared * value;
}


char* UTIL_VarArgs( char *format, ... )
{
	va_list		argptr;
	static char		string[1024];
	
	va_start (argptr, format);
	vsprintf (string, format,argptr);
	va_end (argptr);

	return string;	
}
	
Vector UTIL_GetAimVector( edict_t *pent, float flSpeed )
{
	Vector tmp;
	GET_AIM_VECTOR(pent, flSpeed, tmp);
	return tmp;
}

BOOL UTIL_IsMasterTriggered(string_t iszMaster, CBaseEntity *pActivator)
{
	int i, j, found = false;
	const char *szMaster;
	char szBuf[80];
	CBaseEntity *pMaster;
	int reverse = false;


	if (iszMaster)
	{
		//Msg( "IsMasterTriggered(%s, %s \"%s\")\n", STRING(iszMaster), STRING(pActivator->pev->classname), STRING(pActivator->pev->targetname));
		szMaster = STRING(iszMaster);
		if (szMaster[0] == '~') //inverse master
		{
			reverse = true;
			szMaster++;
		}

		pMaster = UTIL_FindEntityByTargetname( NULL, szMaster );
		if ( !pMaster )
		{
			for (i = 0; szMaster[i]; i++)
			{
				if (szMaster[i] == '(')
				{
					for (j = i+1; szMaster[j]; j++)
					{
						if (szMaster[j] == ')')
						{
							strncpy(szBuf, szMaster+i+1, (j-i)-1);
							szBuf[(j-i)-1] = 0;
							pActivator = UTIL_FindEntityByTargetname( NULL, szBuf );
							found = true;
							break;
						}
					}
					if (!found) // no ) found
					{
						ALERT(at_error, "Missing ')' in master \"%s\"\n", szMaster);
						return FALSE;
					}
					break;
				}
			}

			if( !found ) // no ( found
			{
				ALERT( at_console, "Master \"%s\" not found!\n", szMaster );
				return TRUE;
			}

			strncpy(szBuf, szMaster, i);
			szBuf[i] = 0;
			pMaster = UTIL_FindEntityByTargetname( NULL, szBuf );
		}

		if( pMaster )
		{
			if( reverse )
				return (pMaster->GetState( pActivator ) != STATE_ON );
			return (pMaster->GetState( pActivator ) == STATE_ON);
		}
	}

	// if the entity has no master (or the master is missing), just say yes.
	return TRUE;
}

BOOL UTIL_ShouldShowBlood( int color )
{
	if ( color != DONT_BLEED )
	{
		if ( color == BLOOD_COLOR_RED )
		{
			if ( CVAR_GET_FLOAT("violence_hblood") != 0 )
				return TRUE;
		}
		else
		{
			if ( CVAR_GET_FLOAT("violence_ablood") != 0 )
				return TRUE;
		}
	}
	return FALSE;
}

int UTIL_PointContents( const Vector &vec )
{
	return POINT_CONTENTS( vec );
}

void UTIL_BloodStream( const Vector &origin, const Vector &direction, int color, int amount )
{
	if ( !UTIL_ShouldShowBlood( color ) )
		return;


	MESSAGE_BEGIN( MSG_PVS, gmsg.TempEntity, origin );
		WRITE_BYTE( TE_BLOODSTREAM );
		WRITE_COORD( origin.x );
		WRITE_COORD( origin.y );
		WRITE_COORD( origin.z );
		WRITE_COORD( direction.x );
		WRITE_COORD( direction.y );
		WRITE_COORD( direction.z );
		WRITE_BYTE( color );
		WRITE_BYTE( min( amount, 255 ) );
	MESSAGE_END();
}				

void UTIL_BloodDrips( const Vector &origin, const Vector &direction, int color, int amount )
{
	if ( !UTIL_ShouldShowBlood( color ) )
		return;

	if ( color == DONT_BLEED || amount == 0 )
		return;

	if ( g_Language == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED )
		color = 0;

	if ( g_pGameRules->IsMultiplayer() )
	{
		// scale up blood effect in multiplayer for better visibility
		amount *= 2;
	}

	if ( amount > 255 )
		amount = 255;

	MESSAGE_BEGIN( MSG_PVS, gmsg.TempEntity, origin );
		WRITE_BYTE( TE_BLOODSPRITE );
		WRITE_COORD( origin.x);								// pos
		WRITE_COORD( origin.y);
		WRITE_COORD( origin.z);
		WRITE_SHORT( g_sModelIndexBloodSpray );				// initial sprite model
		WRITE_SHORT( g_sModelIndexBloodDrop );				// droplet sprite models
		WRITE_BYTE( color );								// color index into host_basepal
		WRITE_BYTE( min( max( 3, amount / 10 ), 16 ) );		// size
	MESSAGE_END();
}				

Vector UTIL_RandomBloodVector( void )
{
	Vector direction;

	direction.x = RANDOM_FLOAT ( -1, 1 );
	direction.y = RANDOM_FLOAT ( -1, 1 );
	direction.z = RANDOM_FLOAT ( 0, 1 );

	return direction;
}


void UTIL_BloodDecalTrace( TraceResult *pTrace, int bloodColor )
{
	if ( UTIL_ShouldShowBlood( bloodColor ) )
	{
		if ( bloodColor == BLOOD_COLOR_RED )
			UTIL_DecalTrace( pTrace, DECAL_BLOOD1 + RANDOM_LONG(0,5) );
		else
			UTIL_DecalTrace( pTrace, DECAL_YBLOOD1 + RANDOM_LONG(0,5) );
	}
}


void UTIL_DecalTrace( TraceResult *pTrace, int decalNumber )
{
	short entityIndex;
	int index;
	int message;

	if ( decalNumber < 0 )
		return;

	index = gDecals[ decalNumber ].index;

	if ( index < 0 )
		return;

	if (pTrace->flFraction == 1.0)
		return;

	// Only decal BSP models
	if ( pTrace->pHit )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( pTrace->pHit );
		if ( pEntity && !pEntity->IsBSPModel() )
			return;
		entityIndex = ENTINDEX( pTrace->pHit );
	}
	else 
		entityIndex = 0;

	message = TE_DECAL;
	if ( entityIndex != 0 )
	{
		if ( index > 255 )
		{
			message = TE_DECALHIGH;
			index -= 256;
		}
	}
	else
	{
		message = TE_WORLDDECAL;
		if ( index > 255 )
		{
			message = TE_WORLDDECALHIGH;
			index -= 256;
		}
	}
	
	MESSAGE_BEGIN( MSG_BROADCAST, gmsg.TempEntity );
		WRITE_BYTE( message );
		WRITE_COORD( pTrace->vecEndPos.x );
		WRITE_COORD( pTrace->vecEndPos.y );
		WRITE_COORD( pTrace->vecEndPos.z );
		WRITE_BYTE( index );
		if ( entityIndex )
			WRITE_SHORT( entityIndex );
	MESSAGE_END();
}

/*
==============
UTIL_PlayerDecalTrace

A player is trying to apply his custom decal for the spray can.
Tell connected clients to display it, or use the default spray can decal
if the custom can't be loaded.
==============
*/
void UTIL_PlayerDecalTrace( TraceResult *pTrace, int playernum, int decalNumber, BOOL bIsCustom )
{
	int index;
	
	if (!bIsCustom)
	{
		if ( decalNumber < 0 )
			return;

		index = gDecals[ decalNumber ].index;
		if ( index < 0 )
			return;
	}
	else
		index = decalNumber;

	if (pTrace->flFraction == 1.0)
		return;

	MESSAGE_BEGIN( MSG_BROADCAST, gmsg.TempEntity );
		WRITE_BYTE( TE_PLAYERDECAL );
		WRITE_BYTE ( playernum );
		WRITE_COORD( pTrace->vecEndPos.x );
		WRITE_COORD( pTrace->vecEndPos.y );
		WRITE_COORD( pTrace->vecEndPos.z );
		WRITE_SHORT( (short)ENTINDEX(pTrace->pHit) );
		WRITE_BYTE( index );
	MESSAGE_END();
}

void UTIL_GunshotDecalTrace( TraceResult *pTrace, int decalNumber )
{
	if ( decalNumber < 0 )
		return;

	int index = gDecals[ decalNumber ].index;
	if ( index < 0 )
		return;

	if( pTrace->flFraction == 1.0 )
		return;

	MESSAGE_BEGIN( MSG_PAS, gmsg.TempEntity, pTrace->vecEndPos );
		WRITE_BYTE( TE_GUNSHOTDECAL );
		WRITE_COORD( pTrace->vecEndPos.x );
		WRITE_COORD( pTrace->vecEndPos.y );
		WRITE_COORD( pTrace->vecEndPos.z );
		WRITE_DIR( pTrace->vecPlaneNormal );
		WRITE_SHORT( (short)ENTINDEX( pTrace->pHit ));
		WRITE_BYTE( index );
	MESSAGE_END();
}


void UTIL_Sparks( const Vector &position )
{
	MESSAGE_BEGIN( MSG_PVS, gmsg.TempEntity, position );
		WRITE_BYTE( TE_SPARKS );
		WRITE_COORD( position.x );
		WRITE_COORD( position.y );
		WRITE_COORD( position.z );
	MESSAGE_END();
}

void UTIL_Explode( const Vector &center, edict_t *pOwner, int radius, int name )
{
	CBaseEntity *pExplosion = CBaseEntity::Create( "env_explosion", center, g_vecZero, pOwner );
	if(pExplosion)
	{
		if( name ) pExplosion->pev->classname = name;
		pExplosion->pev->dmg = radius;
		pExplosion->pev->owner = pOwner;
		pExplosion->pev->spawnflags |= SF_FIREONCE; //remove entity after explode
		pExplosion->Use( NULL, NULL, USE_ON, 0 );
	}
}

void UTIL_Ricochet( const Vector &position, float scale )
{
	MESSAGE_BEGIN( MSG_PVS, gmsg.TempEntity, position );
		WRITE_BYTE( TE_ARMOR_RICOCHET );
		WRITE_COORD( position.x );
		WRITE_COORD( position.y );
		WRITE_COORD( position.z );
		WRITE_BYTE( (int)( scale * 10 ));
	MESSAGE_END();
}


BOOL UTIL_TeamsMatch( const char *pTeamName1, const char *pTeamName2 )
{
	// Everyone matches unless it's teamplay
	if ( !g_pGameRules->IsTeamplay() )
		return TRUE;

	// Both on a team?
	if ( *pTeamName1 != 0 && *pTeamName2 != 0 )
	{
		if ( !FStriCmp( pTeamName1, pTeamName2 ) )	// Same Team?
			return TRUE;
	}

	return FALSE;
}

BOOL UTIL_IsFacing( entvars_t *pevTest, const Vector &reference )
{
	Vector vecDir = (reference - pevTest->origin);
	vecDir.z = 0;
	vecDir = vecDir.Normalize();
	Vector forward, angle;
	angle = pevTest->viewangles;
	angle.x = 0;

	UTIL_MakeVectorsPrivate( angle, forward, NULL, NULL );
	// He's facing me, he meant it
	if( DotProduct( forward, vecDir ) > 0.96 ) // +/- 15 degrees or so
	{
		return TRUE;
	}
	return FALSE;
}

void UTIL_StringToVector( float *pVector, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int	j;

	strcpy( tempString, pString );
	pstr = pfront = tempString;

	for ( j = 0; j < 3; j++ )			// lifted from pr_edict.c
	{
		pVector[j] = atof( pfront );

		while ( *pstr && *pstr != ' ' )
			pstr++;
		if (!*pstr)
			break;
		pstr++;
		pfront = pstr;
	}
	if (j < 2)
	{
		/*
		ALERT( at_error, "Bad field in entity!! %s:%s == \"%s\"\n",
			pkvd->szClassName, pkvd->szKeyName, pkvd->szValue );
		*/
		for (j = j+1;j < 3; j++)
			pVector[j] = 0;
	}
}


//LRC - randomized vectors of the form "0 0 0 .. 1 0 0"
void UTIL_StringToRandomVector( float *pVector, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int	j;
	float pAltVec[3];

	strcpy( tempString, pString );
	pstr = pfront = tempString;

	for ( j = 0; j < 3; j++ )			// lifted from pr_edict.c
	{
		pVector[j] = atof( pfront );

		while ( *pstr && *pstr != ' ' ) pstr++;
		if (!*pstr) break;
		pstr++;
		pfront = pstr;
	}
	if (j < 2)
	{
		/*
		ALERT( at_error, "Bad field in entity!! %s:%s == \"%s\"\n",
			pkvd->szClassName, pkvd->szKeyName, pkvd->szValue );
		*/
		for (j = j+1;j < 3; j++)
			pVector[j] = 0;
	}
	else if (*pstr == '.')
	{
		pstr++;
		if (*pstr != '.') return;
		pstr++;
		if (*pstr != ' ') return;

		UTIL_StringToVector(pAltVec, pstr);

		pVector[0] = RANDOM_FLOAT( pVector[0], pAltVec[0] );
		pVector[1] = RANDOM_FLOAT( pVector[1], pAltVec[1] );
		pVector[2] = RANDOM_FLOAT( pVector[2], pAltVec[2] );
	}
}


void UTIL_StringToIntArray( int *pVector, int count, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int	j;

	strcpy( tempString, pString );
	pstr = pfront = tempString;

	for ( j = 0; j < count; j++ )			// lifted from pr_edict.c
	{
		pVector[j] = atoi( pfront );

		while ( *pstr && *pstr != ' ' )
			pstr++;
		if (!*pstr)
			break;
		pstr++;
		pfront = pstr;
	}

	for ( j++; j < count; j++ )
	{
		pVector[j] = 0;
	}
}

Vector UTIL_ClampVectorToBox( const Vector &input, const Vector &clampSize )
{
	Vector sourceVector = input;

	if ( sourceVector.x > clampSize.x )
		sourceVector.x -= clampSize.x;
	else if ( sourceVector.x < -clampSize.x )
		sourceVector.x += clampSize.x;
	else
		sourceVector.x = 0;

	if ( sourceVector.y > clampSize.y )
		sourceVector.y -= clampSize.y;
	else if ( sourceVector.y < -clampSize.y )
		sourceVector.y += clampSize.y;
	else
		sourceVector.y = 0;
	
	if ( sourceVector.z > clampSize.z )
		sourceVector.z -= clampSize.z;
	else if ( sourceVector.z < -clampSize.z )
		sourceVector.z += clampSize.z;
	else
		sourceVector.z = 0;

	return sourceVector.Normalize();
}


float UTIL_WaterLevel( const Vector &position, float minz, float maxz )
{
	Vector midUp = position;
	midUp.z = minz;

	if( UTIL_PointContents( midUp ) != CONTENTS_WATER )
		return minz;

	midUp.z = maxz;
	if( UTIL_PointContents( midUp ) == CONTENTS_WATER )
		return maxz;

	float diff = maxz - minz;
	while( diff > 1.0 )
	{
		midUp.z = minz + diff/2.0;
		if( UTIL_PointContents( midUp ) == CONTENTS_WATER )
		{
			minz = midUp.z;
		}
		else
		{
			maxz = midUp.z;
		}
		diff = maxz - minz;
	}
	return midUp.z;
}

void UTIL_Bubbles( Vector mins, Vector maxs, int count )
{
	Vector mid =  (mins + maxs) * 0.5;

	float flHeight = UTIL_WaterLevel( mid,  mid.z, mid.z + 1024 );
	flHeight = flHeight - mins.z;

	MESSAGE_BEGIN( MSG_PAS, gmsg.TempEntity, mid );
		WRITE_BYTE( TE_BUBBLES );
		WRITE_COORD( mins.x );	// mins
		WRITE_COORD( mins.y );
		WRITE_COORD( mins.z );
		WRITE_COORD( maxs.x );	// maxz
		WRITE_COORD( maxs.y );
		WRITE_COORD( maxs.z );
		WRITE_COORD( flHeight );			// height
		WRITE_SHORT( g_sModelIndexBubbles );
		WRITE_BYTE( count ); // count
		WRITE_COORD( 8 ); // speed
	MESSAGE_END();
}

void UTIL_BubbleTrail( Vector from, Vector to, int count )
{
	float flHeight = UTIL_WaterLevel( from,  from.z, from.z + 256 );
	flHeight = flHeight - from.z;

	if (flHeight < 8)
	{
		flHeight = UTIL_WaterLevel( to,  to.z, to.z + 256 );
		flHeight = flHeight - to.z;
		if (flHeight < 8)
			return;

		// UNDONE: do a ploink sound
		flHeight = flHeight + to.z - from.z;
	}

	if (count > 255) 
		count = 255;

	MESSAGE_BEGIN( MSG_BROADCAST, gmsg.TempEntity );
		WRITE_BYTE( TE_BUBBLETRAIL );
		WRITE_COORD( from.x );	// mins
		WRITE_COORD( from.y );
		WRITE_COORD( from.z );
		WRITE_COORD( to.x );	// maxz
		WRITE_COORD( to.y );
		WRITE_COORD( to.z );
		WRITE_COORD( flHeight );			// height
		WRITE_SHORT( g_sModelIndexBubbles );
		WRITE_BYTE( count ); // count
		WRITE_COORD( 8 ); // speed
	MESSAGE_END();
}

BOOL UTIL_IsValidEntity( edict_t *pent )
{
	if ( !pent || pent->free || (pent->v.flags & FL_KILLME) )
		return FALSE;
	return TRUE;
}

//=========================================================
// UTIL_LogPrintf - Prints a logged message to console.
// Preceded by LOG: ( timestamp ) < message >
//=========================================================
void UTIL_LogPrintf( char *fmt, ... )
{
	va_list			argptr;
	static char		string[1024];
	
	va_start ( argptr, fmt );
	vsprintf ( string, fmt, argptr );
	va_end   ( argptr );

	// Print to server console
	ALERT( at_logged, "%s", string );
}

//============
// UTIL_FileExtension
// returns file extension
//============
const char *UTIL_FileExtension( const char *in )
{
	const char *separator, *backslash, *colon, *dot;

	separator = strrchr( in, '/' );
	backslash = strrchr( in, '\\' );
	if( !separator || separator < backslash )
		separator = backslash;
	colon = strrchr( in, ':' );
	if( !separator || separator < colon )
		separator = colon;
	dot = strrchr( in, '.' );
	if( dot == NULL || (separator && ( dot < separator )))
		return "";
	return dot + 1;
}

//=========================================================
// UTIL_DotPoints - returns the dot product of a line from
// src to check and vecdir.
//=========================================================
float UTIL_DotPoints ( const Vector &vecSrc, const Vector &vecCheck, const Vector &vecDir )
{
	Vector2D	vec2LOS;

	vec2LOS = ( vecCheck - vecSrc ).Make2D();
	vec2LOS = vec2LOS.Normalize();

	return DotProduct (vec2LOS , ( vecDir.Make2D() ) );
}

//for trigger_viewset
int HaveCamerasInPVS( edict_t* edict )
{
	CBaseEntity *pViewEnt = NULL;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pEntity = UTIL_PlayerByIndex( i );
		if (!pEntity) continue;
		CBasePlayer *pPlayer = (CBasePlayer *)pEntity;
		if (pPlayer && pPlayer->viewFlags & 1) // custom view active
		{
			pViewEnt = pPlayer->pViewEnt;
			if (!pViewEnt->edict())return 0;

			edict_t *view = pViewEnt->edict();
			edict_t *pent = UTIL_EntitiesInPVS( edict );

			while ( !FNullEnt( pent ) )
			{
				if (pent == view)return TRUE;
				pent = pent->v.chain;
			}
		}
	}
	return 0;
}
void UTIL_SetView( int ViewEntity, int flags )
{
	//Light version SetView
	//Please don't use this in multiplayer
	CBaseEntity *m_pPlayer = UTIL_PlayerByIndex( 1 );
	UTIL_SetView( m_pPlayer, ViewEntity, flags );
}

void UTIL_SetView( CBaseEntity *pActivator, int ViewEntity, int flags )
{
          CBaseEntity *pViewEnt = 0;
          
	if(ViewEntity)
	{
		//try to find by targetname
		pViewEnt = UTIL_FindEntityByString( NULL, "targetname", STRING(ViewEntity));
		//try to find by classname
		if(FNullEnt(pViewEnt))
			pViewEnt = UTIL_FindEntityByString( NULL, "classname", STRING(ViewEntity));
		if(pViewEnt && pViewEnt->pev->flags & FL_MONSTER) flags |= MONSTER_VIEW;//detect monster view
	}
	UTIL_SetView( pActivator, pViewEnt, flags );
}

void UTIL_SetView( CBaseEntity *pActivator, CBaseEntity *pViewEnt, int flags )
{
	((CBasePlayer *)pActivator)->pViewEnt = pViewEnt;
	((CBasePlayer *)pActivator)->viewFlags = flags;
	((CBasePlayer *)pActivator)->viewNeedsUpdate = 1;
}