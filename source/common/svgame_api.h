//=======================================================================
//			Copyright XashXT Group 2008 ©
//	      svgame_api.h - entity interface between engine and svgame
//=======================================================================
#ifndef SVGAME_API_H
#define SVGAME_API_H

#include "trace_def.h"
#include "event_api.h"

typedef struct globalvars_s
{	
	float		time;
	float		frametime;
	string_t		mapname;
	string_t		startspot;
	vec3_t		spotOffset;	// landmark offset

	BOOL		deathmatch;
	BOOL		coop;
	BOOL		teamplay;

	int		serverflags;
	int		maxClients;
	int		numClients;	// actual clients count
	int		maxEntities;
	int		numEntities;	// actual ents count

	vec3_t		v_forward;
	vec3_t		v_right;
	vec3_t		v_up;

	BOOL		trace_allsolid;
	BOOL		trace_startsolid;
	BOOL		trace_startstuck;
	float		trace_fraction;
	vec3_t		trace_endpos;
	vec3_t		trace_plane_normal;
	float		trace_plane_dist;
	int		trace_start_contents;
	int		trace_contents;
	int		trace_hitgroup;
	const char	*trace_texture;	// texture name that we hitting (brushes and studiomodels)
	edict_t		*trace_ent;

	int		total_secrets;
	int		found_secrets;	// number of secrets found
	int		total_monsters;
	int		killed_monsters;	// number of monsters killed

	void		*pSaveData;	// (SAVERESTOREDATA *) pointer
} globalvars_t;

// engine hands this to DLLs for functionality callbacks
typedef struct enginefuncs_s
{
	// interface validator
	int	api_size;			// must matched with sizeof( enginefuncs_t )

	int	(*pfnPrecacheModel)( const char* s );
	int	(*pfnPrecacheSound)( const char* s );
	void	(*pfnSetModel)( edict_t *e, const char *m );
	int	(*pfnModelIndex)( const char *m );
	int	(*pfnModelFrames)( int modelIndex );
	void	(*pfnSetSize)( edict_t *e, const float *rgflMin, const float *rgflMax );
	void	(*pfnChangeLevel)( const char* s1, const char* s2 );
	edict_t*	(*pfnFindClientInPHS)( edict_t *pEdict );	// was pfnGetSpawnParms
	edict_t*	(*pfnEntitiesInPHS)( edict_t *pplayer );	// was pfnSaveSpawnParms
	float	(*pfnVecToYaw)( const float *rgflVector );
	void	(*pfnVecToAngles)( const float *rgflVectorIn, float *rgflVectorOut );
	void	(*pfnMoveToOrigin)( edict_t *ent, const float *pflGoal, float dist, int iMoveType );
	void	(*pfnChangeYaw)( edict_t* ent );
	void	(*pfnChangePitch)( edict_t* ent );
	edict_t*	(*pfnFindEntityByString)( edict_t *pStartEdict, const char *pszField, const char *pszValue);
	int	(*pfnGetEntityIllum)( edict_t* pEnt );
	edict_t*	(*pfnFindEntityInSphere)( edict_t *pStartEdict, const float *org, float rad );
	edict_t*	(*pfnFindClientInPVS)( edict_t *pEdict );
	edict_t*	(*pfnEntitiesInPVS)( edict_t *pplayer );
	void	(*pfnMakeVectors)( const float *rgflVector );
	void	(*pfnAngleVectors)( const float *rgflVector, float *forward, float *right, float *up );
	edict_t*	(*pfnCreateEntity)( void );
	void	(*pfnRemoveEntity)( edict_t* e );
	edict_t*	(*pfnCreateNamedEntity)( string_t className );
	void	(*pfnMakeStatic)( edict_t *ent );
	void	(*pfnLinkEdict)( edict_t *e );		// was pfnEntIsOnFloor
	int	(*pfnDropToFloor)( edict_t* e );
	int	(*pfnWalkMove)( edict_t *ent, float yaw, float dist, int iMode );
	void	(*pfnSetOrigin)( edict_t *e, const float *rgflOrigin );
	void	(*pfnEmitSound)( edict_t *ent, int chan, const char *sample, float vol, float attn, int flags, int pitch );
	void	(*pfnEmitAmbientSound)( edict_t *ent, float *pos, const char *samp, float vol, float attn, int flags, int pitch );
	void	(*pfnTraceLine)( const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr );
	void	(*pfnTraceToss)( edict_t* pent, edict_t* pentToIgnore, TraceResult *ptr );
	int	(*pfnTraceMonsterHull)( edict_t *pEdict, const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr );
	void	(*pfnTraceHull)( const float *v1, const float *mins, const float *maxs, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr );
	void	(*pfnTraceModel)( const float *v1, const float *v2, edict_t *pent, TraceResult *ptr );
	const char *(*pfnTraceTexture)( edict_t *pTextureEntity, const float *v1, const float *v2 );
	void	(*pfnTraceSphere)( const float *v1, const float *v2, int fNoMonsters, float radius, edict_t *pentToSkip, TraceResult *ptr );
	void	(*pfnGetAimVector)( edict_t* ent, float speed, float *rgflReturn );
	void	(*pfnServerCommand)( const char* str );
	void	(*pfnServerExecute)( void );
	void	(*pfnClientCommand)( edict_t* pEdict, char* szFmt, ... );
	void	(*pfnParticleEffect)( const float *org, const float *dir, float color, float count );
	void	(*pfnLightStyle)( int style, char* val );
	int	(*pfnDecalIndex)( const char *name );
	int	(*pfnPointContents)( const float *rgflVector);
	void	(*pfnMessageBegin)( int msg_dest, int msg_type, const float *pOrigin, edict_t *ed );
	void	(*pfnMessageEnd)( void );
	void	(*pfnWriteByte)( int iValue );
	void	(*pfnWriteChar)( int iValue );
	void	(*pfnWriteShort)( int iValue );
	void	(*pfnWriteLong)( int iValue );
	void	(*pfnWriteAngle)( float flValue );
	void	(*pfnWriteCoord)( float flValue );
	void	(*pfnWriteString)( const char *sz );
	void	(*pfnWriteEntity)( int iValue );
	cvar_t*	(*pfnCVarRegister)( const char *name, const char *value, int flags, const char *desc );
	float	(*pfnCVarGetFloat)( const char *szVarName );
	const char* (*pfnCVarGetString)( const char *szVarName );
	void	(*pfnCVarSetFloat)( const char *szVarName, float flValue );
	void	(*pfnCVarSetString)( const char *szVarName, const char *szValue );
	void	(*pfnAlertMessage)( ALERT_TYPE level, char *szFmt, ... );
	void	(*pfnWriteFloat)( float flValue );		// was pfnEngineFprintf
	void*	(*pfnPvAllocEntPrivateData)( edict_t *pEdict, long cb );
	void*	(*pfnPvEntPrivateData)( edict_t *pEdict );
	void	(*pfnFreeEntPrivateData)( edict_t *pEdict );
	const char *(*pfnGetString)( string_t iString );		// was pfnSzFromIndex
	string_t	(*pfnAllocString)( const char *szValue );
	entvars_t *(*pfnGetVarsOfEnt)( edict_t *pEdict );
	edict_t*	(*pfnPEntityOfEntOffset)( int iEntOffset );
	int	(*pfnEntOffsetOfPEntity)( const edict_t *pEdict );
	int	(*pfnIndexOfEdict)( const edict_t *pEdict );
	edict_t*	(*pfnPEntityOfEntIndex)( int iEntIndex );
	edict_t*	(*pfnFindEntityByVars)( entvars_t* pvars );
	void*	(*pfnGetModelPtr)( edict_t* pEdict );
	int	(*pfnRegUserMsg)( const char *pszName, int iSize );
	void	(*pfnAnimationAutomove)( const edict_t* pEdict, float flTime );
	void	(*pfnGetBonePosition)( const edict_t* pEdict, int iBone, float *rgflOrigin, float *rgflAngles );
	dword	(*pfnFunctionFromName)( const char *pName );
	const char *(*pfnNameForFunction)( dword function );
	void	(*pfnClientPrintf)( edict_t* pEdict, PRINT_TYPE ptype, const char *szMsg );
	void	(*pfnServerPrint)( const char *szMsg );
	const char *(*pfnCmd_Args)( void );
	const char *(*pfnCmd_Argv)( int argc );
	int	(*pfnCmd_Argc)( void );
	void	(*pfnGetAttachment)( const edict_t *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles );
	void	(*pfnCRC_Init)( word *pulCRC );
	void	(*pfnCRC_ProcessBuffer)( word *pulCRC, void *p, int len );
	void	(*pfnCRC_ProcessByte)( word *pulCRC, byte ch );
	word	(*pfnCRC_Final)( word pulCRC );
	long	(*pfnRandomLong)( long lLow, long lHigh );
	float	(*pfnRandomFloat)( float flLow, float flHigh );
	void	(*pfnSetView)( const edict_t *pClient, const edict_t *pViewent );
	float	(*pfnTime)( void );					// host.realtime, not sv.time
	void	(*pfnCrosshairAngle)( const edict_t *pClient, float pitch, float yaw );
	byte*	(*pfnLoadFile)( const char *filename, int *pLength );
	void	(*pfnFreeFile)( void *buffer );
	void	(*pfnEndGame)( const char *engine_command );		// was pfnEndSection
	int	(*pfnCompareFileTime)( const char *filename1, const char *filename2, int *iCompare );
	void	(*pfnGetGameDir)( char *szGetGameDir );
	void	(*pfnClassifyEdict)( edict_t *pEdict, int ed_type );	// was pfnCvar_RegisterVariable
	void	(*pfnAreaPortal)( edict_t *pEdict, BOOL enable );		// was pfnFadeClientVolume
	void	(*pfnSetClientMaxspeed)( const edict_t *pEdict, float fNewMaxspeed );
	edict_t	*(*pfnCreateFakeClient)( const char *netname ); // returns NULL if fake client can't be created
	void	(*pfnThinkFakeClient)( edict_t *client, usercmd_t *cmd );	// was pfnRunPlayerMove, like it
	int	(*pfnFileExists)( const char *filename );		// was pfnNumberOfEntities - see gpGlobals->numEntities
	char*	(*pfnGetInfoKeyBuffer)( edict_t *e );	// passing in NULL gets the serverinfo
	char*	(*pfnInfoKeyValue)( char *infobuffer, char *key );
	void	(*pfnSetKeyValue)( char *infobuffer, char *key, char *value );
	void	(*pfnSetClientKeyValue)( int clientIndex, char *infobuffer, char *key, char *value );
	int	(*pfnIsMapValid)( char *filename );
	void	(*pfnStaticDecal)( const float *origin, int decalIndex, int entityIndex, int modelIndex );
	int	(*pfnPrecacheGeneric)( const char* s );
	void	(*pfnSetSkybox)( const char *name );			// was pfnGetPlayerUserId
	void	(*pfnPlayMusic)( const char *trackname, int flags );	// was pfnBuildSoundMsg
	int	(*pfnIsDedicatedServer)( void );	// is this a dedicated server?
	void*	(*pfnMemAlloc)(size_t cb, const char *file, const int line);// was pfnCVarGetPointer
	void	(*pfnMemFree)(void *mem, const char *file, const int line);	// was pfnGetPlayerWONId
	void	(*pfnInfo_RemoveKey)( char *s, char *key );
	void	*(*pfnFOpen)( const char* path, const char* mode );	// was pfnGetPhysicsKeyValue	
	int	(*pfnFClose)( void *file );				// was pfnSetPhysicsKeyValue				
	long	(*pfnFTell)( void *file );				// was pfnGetPhysicsInfoString
	word	(*pfnPrecacheEvent)( int type, const char *psz );
	void	(*pfnPlaybackEvent)( int flags, const edict_t *pInvoker, word eventindex, float delay, event_args_t *args );
	long	(*pfnFWrite)(void *file, const void* data, size_t datasize);// was pfnSetFatPVS
	long	(*pfnFRead)( void *file, void* buffer, size_t buffersize );	// was pfnSetFatPAS
	int	(*pfnFGets)( void *file, byte *string, size_t bufsize );	// was pfnCheckVisibility
	int	(*pfnFSeek)( void *file, long offset, int whence );	// was pfnDeltaSetField
	void	(*pfnDropClient)( int clientIndex );			// was pfnDeltaUnsetField
	void	(*pfnHostError)( const char *szFmt, ... );		// was pfnDeltaAddEncoder
	void	(*pfnGetPlayerPing)( const edict_t *pClient, int *ping );	// was pfnGetCurrentPlayer
	BOOL	(*pfnCanSkipPlayer)( const edict_t *player );

	// after this point enginefuncs_t completely unmatched with Half-Life interface
} enginefuncs_t;

// passed to pfnKeyValue
typedef struct KeyValueData_s
{
	char	*szClassName;	// in: entity classname
	char	*szKeyName;	// in: name of key
	char	*szValue;		// in: value of key
	long	fHandled;		// out: DLL sets to true if key-value pair was understood
} KeyValueData;

typedef struct
{
	char	mapName[64];
	char	landmarkName[64];
	edict_t	*pentLandmark;
	vec3_t	vecLandmarkOrigin;
} LEVELLIST;

typedef struct 
{
	int	id;		// ENG ordinal ID of this entity (used for entity <--> pointer conversions)
	edict_t	*pent;		// ENG pointer to the in-game entity

	int	location;		// offset from the base data of this entity
	int	size;		// byte size of this entity's data
	int	flags;		// bit mask of transitions that this entity is in the PVS of
	string_t	classname;	// entity class name
} ENTITYTABLE;

#define FTYPEDESC_GLOBAL		0x0001	// This field is masked for global entity save/restore
#define MAX_LEVEL_CONNECTIONS		16	// These are encoded in the lower 16bits of ENTITYTABLE->flags

#define FENTTABLE_GLOBAL		0x10000000
#define FENTTABLE_MOVEABLE		0x20000000
#define FENTTABLE_REMOVED		0x40000000
#define FENTTABLE_PLAYER		0x80000000

typedef struct saverestore_s
{
	char		*pBaseData;	// ENG start of all entity save data
	char		*pCurrentData;	// current buffer pointer for sequential access
	int		size;		// current data size
	int		bufferSize;	// ENG total space for data (Valve used 512 kb's)
	int		tokenSize;	// always equal 0 (probably not used)
	int		tokenCount;	// ENG number of elements in the pTokens table (Valve used 4096 tokens)
	char		**pTokens;	// ENG hash table of entity strings (sparse)
	int		currentIndex;	// ENG holds a global entity table ID
	int		tableCount;	// ENG number of elements in the entity table (numEntities)
	int		connectionCount;	// number of elements in the levelList[]
	ENTITYTABLE	*pTable;		// ENG array of ENTITYTABLE elements (1 for each entity)
	LEVELLIST		levelList[MAX_LEVEL_CONNECTIONS]; // list of connections from this level

	// smooth transition
	int		fUseLandmark;	// ENG
	char		szLandmarkName[64];	// probably not used
	vec3_t		vecLandmarkOffset;	// for landmark transitions
	char		szCurrentMap[64];	// ENG To check global entities
	float		time;		// ENG current sv.time
} SAVERESTOREDATA;

typedef enum _fieldtypes
{
	FIELD_FLOAT = 0,		// any floating point value
	FIELD_STRING,		// a string ID (return from ALLOC_STRING)
	FIELD_ENTITY,		// an entity offset (EOFFSET)
	FIELD_CLASSPTR,		// CBaseEntity *
	FIELD_EHANDLE,		// entity handle
	FIELD_EVARS,		// EVARS *
	FIELD_EDICT,		// edict_t *, or edict_t *  (same thing)
	FIELD_VECTOR,		// any vector
	FIELD_POSITION_VECTOR,	// a world coordinate (these are fixed up across level transitions automagically)
	FIELD_POINTER,		// arbitrary data pointer... to be removed, use an array of FIELD_CHARACTER
	FIELD_INTEGER,		// any integer or enum
	FIELD_FUNCTION,		// a class function pointer (Think, Use, etc)
	FIELD_BOOLEAN,		// boolean, implemented as an int, I may use this as a hint for compression
	FIELD_SHORT,		// 2 byte integer
	FIELD_CHARACTER,		// a byte
	FIELD_TIME,		// a floating point time (these are fixed up automatically too!)
	FIELD_MODELNAME,		// engine string that is a model name (needs precache)
	FIELD_SOUNDNAME,		// engine string that is a sound name (needs precache)
          FIELD_RANGE,		// Min and Max range for generate random value
	FIELD_INTEGER64,		// long integer
	FIELD_DOUBLE,		// float with double precision
 
	FIELD_TYPECOUNT,		// MUST BE LAST
} FIELDTYPE;

typedef struct 
{
	FIELDTYPE	fieldType;
	char	*fieldName;
	int	fieldOffset;
	short	fieldSize;
	short	flags;
} TYPEDESCRIPTION;

#define offsetof( s, m )				(size_t)&(((s *)0)->m)
#define ARRAYSIZE( p )				(sizeof( p ) / sizeof( p[0] ))
#define _FIELD( type, name, fieldtype, count, flags )	{ fieldtype, #name, offsetof( type, name ), count, flags }
#define DEFINE_FIELD( type, name, fieldtype )		_FIELD( type, name, fieldtype, 1, 0 )
#define DEFINE_ARRAY( type, name, fieldtype, count )	_FIELD( type, name, fieldtype, count, 0 )
#define DEFINE_ENTITY_FIELD( name, fieldtype )		_FIELD( entvars_t, name, fieldtype, 1, 0 )
#define DEFINE_ENTITY_FIELD_ARRAY( name, fieldtype, count )	_FIELD( entvars_t, name, fieldtype, count, 0 )
#define DEFINE_ENTITY_GLOBAL_FIELD( name, fieldtype )	_FIELD( entvars_t, name, fieldtype, 1, FTYPEDESC_GLOBAL )
#define DEFINE_GLOBAL_FIELD( type, name, fieldtype )	_FIELD( type, name, fieldtype, 1, FTYPEDESC_GLOBAL )

typedef struct 
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(DLL_FUNCTIONS)

	// initialize/shutdown the game (one-time call after loading of game .dll )
	void	(*pfnGameInit)( void );				
	void	(*pfnGameShutdown)( void );
	int	(*pfnSpawn)( edict_t *pent );
	int	(*pfnCreate)( edict_t *pent, const char *szName );	// create custom entities
	void	(*pfnThink)( edict_t *pent );
	void	(*pfnUse)( edict_t *pentUsed, edict_t *pentOther );
	void	(*pfnTouch)( edict_t *pentTouched, edict_t *pentOther );
	void	(*pfnBlocked)( edict_t *pentBlocked, edict_t *pentOther );
	void	(*pfnKeyValue)( edict_t *pentKeyvalue, KeyValueData *pkvd );
	void	(*pfnSave)( edict_t *pent, SAVERESTOREDATA *pSaveData );
	int 	(*pfnRestore)( edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity );
	void	(*pfnSetAbsBox)( edict_t *pent );

	void	(*pfnSaveWriteFields)( SAVERESTOREDATA*, const char*, void*, TYPEDESCRIPTION*, int );
	void	(*pfnSaveReadFields)( SAVERESTOREDATA*, const char*, void*, TYPEDESCRIPTION*, int );
	void	(*pfnSaveGlobalState)( SAVERESTOREDATA * );
	void	(*pfnRestoreGlobalState)( SAVERESTOREDATA * );
	void	(*pfnResetGlobalState)( void );

	int	(*pfnClientConnect)( edict_t *pEntity, const char *userinfo );
	
	void	(*pfnClientDisconnect)( edict_t *pEntity );
	void	(*pfnClientKill)( edict_t *pEntity );
	void	(*pfnClientPutInServer)( edict_t *pEntity );
	void	(*pfnClientCommand)( edict_t *pEntity );
	void	(*pfnClientUserInfoChanged)( edict_t *pEntity, char *infobuffer );
	void	(*pfnServerActivate)( edict_t *pEdictList, int edictCount, int clientMax );
	void	(*pfnServerDeactivate)( void );
	void	(*pfnPlayerPreThink)( edict_t *pEntity );
	void	(*pfnPlayerPostThink)( edict_t *pEntity );

	void	(*pfnStartFrame)( void );
	void	(*pfnFrame)( edict_t *pent );
	void	(*pfnEndFrame)( void );
	void	(*pfnBuildLevelList)( void );

	int	(*pfnShouldCollide)( edict_t *pentTouched, edict_t *pentOther );
	int	(*pfnClassifyEdict)( edict_t *pentToClassify );
	void	(*pfnUpdateEntityState)( struct entity_state_s *to, edict_t *from, int baseline );
	void	(*pfnOnFreeEntPrivateData)( edict_t *pEnt );
	
	 // returns string describing current .dll.  E.g., TeamFotrress 2, Half-Life
	const char *(*pfnGetGameDescription)( void );     

	// Spectator funcs
	void	(*pfnSpectatorConnect)( edict_t *pEntity );
	void	(*pfnSpectatorDisconnect)( edict_t *pEntity );
	void	(*pfnSpectatorThink)( edict_t *pEntity );
} DLL_FUNCTIONS;

typedef int (*SERVERAPI)( DLL_FUNCTIONS *pFunctionTable, enginefuncs_t* engfuncs, globalvars_t *pGlobals );
typedef void (*LINK_ENTITY_FUNC)( entvars_t *pev );

#endif//SVGAME_API_H