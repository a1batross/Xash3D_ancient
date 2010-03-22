//=======================================================================
//			Copyright (C) Shambler Team 2005
//		         	 dll_int.cpp - dll entry points 
//=======================================================================

#include	"extdll.h"
#include	"utils.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"client.h"
#include	"decals.h"
#include	"gamerules.h"
#include	"game.h"
#include	"defaults.h"
#include	"baseitem.h"
#include	"baseweapon.h"

void EntvarsKeyvalue( entvars_t *pev, KeyValueData *pkvd );

BOOL gTouchDisabled = FALSE;
enginefuncs_t g_engfuncs;
globalvars_t  *gpGlobals;

// Main DLL entry point
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	return TRUE;
}

static DLL_FUNCTIONS gFunctionTable = 
{
	sizeof( DLL_FUNCTIONS ),
	GameDLLInit,		// pfnGameInit
	DispatchSpawn,		// pfnSpawn
	DispatchThink,		// pfnThink
	DispatchUse,		// pfnUse
	DispatchTouch,		// pfnTouch
	DispatchBlocked,		// pfnBlocked
	DispatchKeyValue,		// pfnKeyValue
	DispatchSave,		// pfnSave
	DispatchRestore,		// pfnRestore
	DispatchObjectCollsionBox,	// pfnAbsBox

	SaveWriteFields,		// pfnSaveWriteFields
	SaveReadFields,		// pfnSaveReadFields

	SaveGlobalState,		// pfnSaveGlobalState
	RestoreGlobalState,		// pfnRestoreGlobalState
	ResetGlobalState,		// pfnResetGlobalState

	ClientConnect,		// pfnClientConnect
	ClientDisconnect,		// pfnClientDisconnect
	ClientKill,		// pfnClientKill
	ClientPutInServer,		// pfnClientPutInServer
	ClientCommand,		// pfnClientCommand
	ClientUserInfoChanged,	// pfnClientUserInfoChanged

	ServerActivate,		// pfnServerActivate
	ServerDeactivate,		// pfnServerDeactivate

	PlayerPreThink,		// pfnPlayerPreThink
	PlayerPostThink,		// pfnPlayerPostThink

	StartFrame,		// pfnStartFrame
	DispatchCreate,		// pfnCreate
	BuildLevelList,		// pfnParmsChangeLevel

	GetGameDescription,		// pfnGetGameDescription - returns string describing current .dll game.
	GetEntvarsDescirption,	// pfnGetEntvarsDescirption

	SpectatorConnect,		// pfnSpectatorConnect
	SpectatorDisconnect,	// pfnSpectatorDisconnect
	SpectatorThink,		// pfnSpectatorThink

	ServerClassifyEdict,	// pfnClassifyEdict

	PM_Move,			// pfnPM_Move
	PM_Init,			// pfnPM_Init
	PM_FindTextureType,		// pfnPM_FindTextureType

	SetupVisibility,		// pfnSetupVisibility	
	DispatchFrame,		// pfnPhysicsEntity
	AddToFullPack,		// pfnAddtoFullPack
	EndFrame,			// pfnEndFrame
	
	ShouldCollide,		// pfnShouldCollide
	UpdateEntityState,		// pfnUpdateEntityState
	OnFreeEntPrivateData,	// pfnOnFreeEntPrivateData
	CmdStart,			// pfnCmdStart
	CmdEnd,			// pfnCmdEnd

	GameDLLShutdown,		// pfnGameShutdown
};

//=======================================================================
//			General API entering point
//=======================================================================

int CreateAPI( DLL_FUNCTIONS *pFunctionTable, enginefuncs_t* pengfuncsFromEngine, globalvars_t *pGlobals )
{
	if( !pFunctionTable || !pengfuncsFromEngine || !pGlobals )
	{
		return FALSE;
	}

	memcpy( pFunctionTable, &gFunctionTable, sizeof( DLL_FUNCTIONS ));
	memcpy(&g_engfuncs, pengfuncsFromEngine, sizeof( enginefuncs_t ));
	gpGlobals = pGlobals;

	return TRUE;
}

//=======================================================================
//			dispatch functions
//=======================================================================

int DispatchSpawn( edict_t *pent )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);

	if( pEntity )
	{
		// Initialize these or entities who don't link to the world won't have anything in here
		pEntity->pev->absmin = pEntity->pev->origin - Vector( 1.0f, 1.0f, 1.0f );
		pEntity->pev->absmax = pEntity->pev->origin + Vector( 1.0f, 1.0f, 1.0f );

		pEntity->Spawn();
		pEntity = (CBaseEntity *)GET_PRIVATE(pent);

		if ( pEntity )
		{
			pEntity->pev->colormap = ENTINDEX(pent);
			if ( g_pGameRules && !g_pGameRules->IsAllowedToSpawn( pEntity ) )
				return -1; // return that this entity should be deleted
			if ( pEntity->pev->flags & FL_KILLME )
				return -1;
		}


		// Handle global stuff here
		if ( pEntity && pEntity->pev->globalname ) 
		{
			const globalentity_t *pGlobal = gGlobalState.EntityFromTable( pEntity->pev->globalname );
			if ( pGlobal )
			{
				// Already dead? delete
				if ( pGlobal->state == GLOBAL_DEAD ) return -1;
				else if ( !FStrEq( STRING(gpGlobals->mapname), pGlobal->levelName ) )
					pEntity->MakeDormant();// Hasn't been moved to this level yet, wait but stay alive
				// In this level & not dead, continue on as normal
			}
			else gGlobalState.EntityAdd( pEntity->pev->globalname, gpGlobals->mapname, GLOBAL_ON );
		}
	}
	return 0;
}

int DispatchCreate( edict_t *pent, const char *szName )
{
	if( FNullEnt( pent ) || FStringNull( szName ))
		return -1;

	int istr = ALLOC_STRING( szName );

	// handle virtual entities here
	if( !strncmp( szName, "weapon_", 7 ))
	{
		CBasePlayerWeapon *pWeapon = GetClassPtr((CBasePlayerWeapon *)VARS( pent ));
		if( !pWeapon ) return -1;
		pWeapon->pev->netname = istr; 
		return 0;
	}
	else if( !strncmp( szName, "item_", 5 ) || !strncmp( szName, "ammo_", 5 ))
	{
		CItem *pItem = GetClassPtr((CItem *)VARS( pent ));
		if( !pItem ) return -1;
  		pItem->pev->netname = istr; 
		return 0;
	}
	return -1;
}

void DispatchKeyValue( edict_t *pentKeyvalue, KeyValueData *pkvd )
{
	if ( !pkvd || !pentKeyvalue )return;

	EntvarsKeyvalue( VARS(pentKeyvalue), pkvd );

	// if the key was an entity variable, or there's no class set yet,
	// don't look for the object, it may not exist yet.
	if( pkvd->fHandled || pkvd->szClassName == NULL )
		return;

	// Get the actualy entity object
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pentKeyvalue);

	if( !pEntity ) return;
	pEntity->KeyValue( pkvd );
}

/*
-----------------
DispatchFrame

this function can override any physics movement
and let user use custom physic.
e.g. you can replace MOVETYPE_PUSH for new movewith system
and many many other things.
-----------------
*/
int DispatchFrame( edict_t *pent )
{
	return 0;
}

void DispatchTouch( edict_t *pentTouched, edict_t *pentOther )
{
	if ( gTouchDisabled ) return;

	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pentTouched);
	CBaseEntity *pOther = (CBaseEntity *)GET_PRIVATE( pentOther );

	if ( pEntity && pOther && ! ((pEntity->pev->flags | pOther->pev->flags) & FL_KILLME) )
		pEntity->Touch( pOther );
}

void DispatchUse( edict_t *pentUsed, edict_t *pentOther )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pentUsed);
	CBaseEntity *pOther = (CBaseEntity *)GET_PRIVATE(pentOther);

	if( pEntity && !( pEntity->pev->flags & FL_KILLME ))
		pEntity->Use( pOther, pOther, USE_TOGGLE, 0 );
}

void DispatchThink( edict_t *pent )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);

	if( pEntity )
	{
		if( FBitSet( pEntity->pev->flags, FL_DORMANT ))
			ALERT( at_error, "Dormant entity %s is thinking!\n", STRING( pEntity->pev->classname ));
		pEntity->Think();
	}
}

void DispatchBlocked( edict_t *pentBlocked, edict_t *pentOther )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE( pentBlocked );
	CBaseEntity *pOther = (CBaseEntity *)GET_PRIVATE( pentOther );

	if( pEntity ) pEntity->Blocked( pOther );
}

void DispatchSave( edict_t *pent, SAVERESTOREDATA *pSaveData )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);
	
	if( pEntity && pSaveData )
	{
		// ALERT( at_console, "DispatchSave( %s, %i )\n", STRING( pent->v.classname ), pent->serialnumber );
		ENTITYTABLE *pTable = &pSaveData->pTable[ pSaveData->currentIndex ];

		if( pTable->pent != pent )
			ALERT( at_error, "ENTITY TABLE OR INDEX IS WRONG!!!!\n" );

		if( pEntity->ObjectCaps() & FCAP_DONT_SAVE ) return;

		// These don't use ltime & nextthink as times really, but we'll fudge around it.
		if( pEntity->pev->movetype == MOVETYPE_PUSH )
		{
			float delta = gpGlobals->time - pEntity->pev->ltime;
			pEntity->pev->ltime += delta;
			pEntity->pev->nextthink += delta;
			pEntity->m_fPevNextThink = pEntity->pev->nextthink;
			pEntity->m_fNextThink += delta;
		}

		pTable->location = pSaveData->size;		// Remember entity position for file I/O
		pTable->classname = pEntity->pev->classname;	// Remember entity class for respawn

		CSave saveHelper( pSaveData );
		pEntity->Save( saveHelper );

		pTable->size = pSaveData->size - pTable->location;// Size of entity block is data size written to block
	}
}

int DispatchRestore( edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);

	if( pEntity && pSaveData )
	{
		entvars_t tmpVars;
		Vector oldOffset;

		// ALERT( at_console, "DispatchRestore( %s )\n", STRING( pent->v.classname ));
		CRestore restoreHelper( pSaveData );
		if ( globalEntity )
		{
			CRestore tmpRestore( pSaveData );
			tmpRestore.PrecacheMode( 0 );
			tmpRestore.ReadEntVars( "ENTVARS", &tmpVars );

			pSaveData->size = pSaveData->pTable[pSaveData->currentIndex].location;
			pSaveData->pCurrentData = pSaveData->pBaseData + pSaveData->size;

			const globalentity_t *pGlobal = gGlobalState.EntityFromTable( tmpVars.globalname );
			
			// Don't overlay any instance of the global that isn't the latest
			// pSaveData->szCurrentMapName is the level this entity is coming from
			// pGlobla->levelName is the last level the global entity was active in.
			// If they aren't the same, then this global update is out of date.
			if ( !FStrEq( pSaveData->szCurrentMap, pGlobal->levelName ) )return 0;

			// Compute the new global offset
			oldOffset = pSaveData->vecLandmarkOffset;
			CBaseEntity *pNewEntity = UTIL_FindGlobalEntity( tmpVars.classname, tmpVars.globalname );
			if ( pNewEntity )
			{
				// Tell the restore code we're overlaying a global entity from another level
				restoreHelper.SetGlobalMode( 1 );	// Don't overwrite global fields
				pSaveData->vecLandmarkOffset = (pSaveData->vecLandmarkOffset - pNewEntity->pev->mins) + tmpVars.mins;
				pEntity = pNewEntity;// we're going to restore this data OVER the old entity
				pent = ENT( pEntity->pev );
				// Update the global table to say that the global definition of this entity should come from this level
				gGlobalState.EntityUpdate( pEntity->pev->globalname, gpGlobals->mapname );
			}
			else
			{
				// This entity will be freed automatically by the engine.  If we don't do a restore on a matching entity (below)
				// or call EntityUpdate() to move it to this level, we haven't changed global state at all.
				return 0;
			}

		}

		if( FClassnameIs( &pent->v, "trigger_camera" ))
		{
			ALERT( at_console, "pev->nextthink is %g\n", tmpVars.nextthink );
		}

		if ( pEntity->ObjectCaps() & FCAP_MUST_SPAWN )
		{
			pEntity->Restore( restoreHelper );
			pEntity->Spawn();
		}
		else
		{
			pEntity->Restore( restoreHelper );
			pEntity->Precache( );
		}

		// Again, could be deleted, get the pointer again.
		pEntity = (CBaseEntity *)GET_PRIVATE(pent);
		if (pEntity )pEntity->pev->colormap = ENTINDEX(pent);

		// Is this an overriding global entity (coming over the transition), or one restoring in a level
		if ( globalEntity )
		{
			pSaveData->vecLandmarkOffset = oldOffset;
			if ( pEntity )
			{
				pEntity->RestorePhysics();
				UTIL_SetOrigin( pEntity, pEntity->pev->origin );
				pEntity->OverrideReset();
			}
		}
		else if ( pEntity && pEntity->pev->globalname ) 
		{
			const globalentity_t *pGlobal = gGlobalState.EntityFromTable( pEntity->pev->globalname );
			if ( pGlobal )
			{
				// Already dead? delete
				if ( pGlobal->state == GLOBAL_DEAD ) return -1;
				else if ( !FStrEq( STRING(gpGlobals->mapname), pGlobal->levelName ) )
				{
					pEntity->MakeDormant();	// Hasn't been moved to this level yet, wait but stay alive
				}
				// In this level & not dead, continue on as normal
			}
			else
			{
				ALERT( at_error, "Global Entity %s (%s) not in table!!!\n", STRING(pEntity->pev->globalname), STRING(pEntity->pev->classname) );
				// Spawned entities default to 'On'
				gGlobalState.EntityAdd( pEntity->pev->globalname, gpGlobals->mapname, GLOBAL_ON );
			}
		}
	}
	return 0;
}

void SaveWriteFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	CSave saveHelper( pSaveData );
	saveHelper.WriteFields( "SWF", pname, pBaseData, pFields, fieldCount );
}

void SaveReadFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	CRestore restoreHelper( pSaveData );
	restoreHelper.ReadFields( pname, pBaseData, pFields, fieldCount );
}

void OnFreeEntPrivateData( edict_s *pEdict )
{
	if( pEdict && pEdict->pvPrivateData )
	{
		((CBaseEntity*)pEdict->pvPrivateData)->~CBaseEntity();
	}
}

void SetObjectCollisionBox( entvars_t *pev )
{
	if (( pev->solid == SOLID_BSP ) && ( pev->angles != g_vecZero ))
	{	
		// expand for rotation
		float	max = 0, v;
		int	i;

		for ( i = 0; i < 3; i++ )
		{
			v = fabs( pev->mins[i] );
			if ( v > max ) max = v;
			v = fabs( pev->maxs[i] );
			if ( v > max ) max = v;
		}

		for ( i = 0; i < 3; i++ )
		{
			pev->absmin[i] = pev->origin[i] - max;
			pev->absmax[i] = pev->origin[i] + max;
		}
	}
	else
	{
		pev->absmin = pev->origin + pev->mins;
		pev->absmax = pev->origin + pev->maxs;
	}

	pev->absmin.x -= 1;
	pev->absmin.y -= 1;
	pev->absmin.z -= 1;
	pev->absmax.x += 1;
	pev->absmax.y += 1;
	pev->absmax.z += 1;
}

void DispatchObjectCollsionBox( edict_t *pent )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);
	if( pEntity ) pEntity->SetObjectCollisionBox();
	else SetObjectCollisionBox( &pent->v );
}

//=======================================================================
//			ehandle operations
//=======================================================================

edict_t * EHANDLE::Get( void ) 
{ 
	if (m_pent)
	{
		if (m_pent->serialnumber == m_serialnumber) return m_pent; 
		else return NULL;
	}
	return NULL; 
};

edict_t *EHANDLE::Set( edict_t *pent ) 
{ 
	m_pent = pent;  
	if (pent) m_serialnumber = m_pent->serialnumber; 
	return pent; 
};


EHANDLE :: operator CBaseEntity *() 
{ 
	return (CBaseEntity *)GET_PRIVATE(Get()); 
};


CBaseEntity *EHANDLE :: operator = (CBaseEntity *pEntity)
{
	if (pEntity)
	{
		m_pent = ENT( pEntity->pev );
		if (m_pent)m_serialnumber = m_pent->serialnumber;
	}
	else
	{
		m_pent = NULL;
		m_serialnumber = 0;
	}
	return pEntity;
}

EHANDLE :: operator int()
{
	return Get() != NULL;
}

CBaseEntity *EHANDLE :: operator->()
{
	return (CBaseEntity *)GET_PRIVATE(Get()); 
}