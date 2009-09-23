//=======================================================================
//			Copyright (C) Shambler Team 2004
//			globals.h - store global variables, 
//=======================================================================
#ifndef GLOBALS_H
#define GLOBALS_H

extern DLL_GLOBAL short		g_sModelIndexLaser;// holds the index for the laser beam
extern DLL_GLOBAL short		g_sModelIndexFireball;// holds the index for the fireball
extern DLL_GLOBAL short		g_sModelIndexSmoke;// holds the index for the smoke cloud
extern DLL_GLOBAL short		g_sModelIndexWExplosion;// holds the index for the underwater explosion
extern DLL_GLOBAL short    		g_sModelIndexBubbles;// holds the index for the bubbles model
extern DLL_GLOBAL short		g_sModelIndexBloodDrop;// holds the sprite index for blood drops
extern DLL_GLOBAL short		g_sModelIndexBloodSpray;// holds the sprite index for blood spray (bigger)
extern DLL_GLOBAL int    		g_sModelIndexLaserDot;
extern DLL_GLOBAL ULONG		g_ulFrameCount;
extern DLL_GLOBAL BOOL		g_fGameOver;
extern DLL_GLOBAL BOOL		g_startSuit;

// C functions for external declarations that call the appropriate C++ methods

#ifdef _WIN32
#define EXPORT	_declspec( dllexport )
#else
#define EXPORT	/* */
#endif

extern "C" EXPORT int GetEntityAPI( DLL_FUNCTIONS *pFunctionTable, int interfaceVersion );

extern int DispatchSpawn( edict_t *pent );
extern int DispatchCreate( edict_t *pent, const char *szName );
extern void DispatchKeyValue( edict_t *pentKeyvalue, KeyValueData *pkvd );
extern void DispatchTouch( edict_t *pentTouched, edict_t *pentOther );
extern void DispatchUse( edict_t *pentUsed, edict_t *pentOther );
extern void DispatchThink( edict_t *pent );
extern void DispatchFrame( edict_t *pent );
extern void DispatchBlocked( edict_t *pentBlocked, edict_t *pentOther );
extern void DispatchSave( edict_t *pent, SAVERESTOREDATA *pSaveData );
extern int  DispatchRestore( edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity );
extern void DispatchObjectCollsionBox( edict_t *pent );
extern int ServerClassifyEdict( edict_t *pentToClassify );
extern void UpdateEntityState( struct entity_state_s *to, edict_t *from, int baseline );
extern void SaveWriteFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount );
extern void SaveReadFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount );
extern void SaveGlobalState( SAVERESTOREDATA *pSaveData );
extern void RestoreGlobalState( SAVERESTOREDATA *pSaveData );
extern void ResetGlobalState( void );
extern void OnFreeEntPrivateData( edict_s *pEdict );
extern int ShouldCollide( edict_t *pentTouched, edict_t *pentOther );

// spectator funcs
extern void SpectatorConnect( edict_t *pEntity );
extern void SpectatorDisconnect( edict_t *pEntity );
extern void SpectatorThink( edict_t *pEntity );

typedef void (CBaseEntity::*BASEPTR)(void);
typedef void (CBaseEntity::*ENTITYFUNCPTR)(CBaseEntity *pOther );
typedef void (CBaseEntity::*USEPTR)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

//
// EHANDLE. Safe way to point to CBaseEntities who may die between frames
//
class EHANDLE
{
private:
	edict_t *m_pent;
	int		m_serialnumber;
public:
	edict_t *Get( void );
	edict_t *Set( edict_t *pent );

	operator int ();

	operator CBaseEntity *();

	CBaseEntity * operator = (CBaseEntity *pEntity);
	CBaseEntity * operator ->();
};


//
// Converts a entvars_t * to a class pointer
// It will allocate the class and entity if necessary
//
template <class T> T * GetClassPtr( T *a )
{
	entvars_t *pev = (entvars_t *)a;

	// allocate entity if necessary
	if (pev == NULL)
		pev = VARS(CREATE_ENTITY());

	// get the private data
	a = (T *)GET_PRIVATE(ENT(pev));

	if (a == NULL)
	{
		// allocate private data
		a = new(pev) T;
		a->pev = pev;
	}
	return a;
}

#endif //GLOBALS_H