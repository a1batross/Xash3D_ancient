//=======================================================================
//			Copyright XashXT Group 2008 ©
//	      svgame_api.h - entity interface between engine and svgame
//=======================================================================
#ifndef SVGAME_API_H
#define SVGAME_API_H

#define INTERFACE_VERSION	138

typedef enum
{
	at_console = 1,	// format: [msg]
	at_warning,	// format: Warning: [msg]
	at_error,		// format: Error: [msg]
	at_loading,	// print messages during loading
	at_aiconsole,	// same as at_console, but only shown if developer level is 5!
	at_logged		// server print to console ( only in multiplayer games ).
} ALERT_TYPE;

// Client_Printf modes
enum
{
	print_console = 0,
	print_center,
	print_chat
};

enum
{
	WALKMOVE_NORMAL = 0,
	WALKMOVE_NOMONSTERS,
	WALKMOVE_MISSILE,
	WALKMOVE_WORLDONLY,
	WALKMOVE_HITMODEL,
	WALKMOVE_CHECKONLY,
};

#define at_debug		at_console	// FIXME: stupid Laurie stuff

// NOTE: engine trace struct not matched with svgame trace
typedef struct
{
	bool		fAllSolid;	// if true, plane is not valid
	bool		fStartSolid;	// if true, the initial point was in a solid area
	bool		fStartStuck;	// if true, trace started from solid entity
	float		flFraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		vecEndPos;	// final position
	int		iStartContents;	// start pos conetnts
	int		iContents;	// final pos contents
	int		iHitgroup;	// 0 == generic, non zero is specific body part
	float		flPlaneDist;	// planes distance
	vec3_t		vecPlaneNormal;	// surface normal at impact
	const char	*pTexName;	// texture name that we hitting (brushes and studiomodels)
	edict_t		*pHit;		// entity the surface is on
} TraceResult;

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
	float	(*pfnVecToYaw)( const float *rgflVector );
	void	(*pfnVecToAngles)( const float *rgflVectorIn, float *rgflVectorOut );
	void	(*pfnMoveToOrigin)( edict_t *ent, const float *pflGoal, float dist, int iMoveType );
	void	(*pfnChangeYaw)( edict_t* ent );
	void	(*pfnChangePitch)( edict_t* ent );
	edict_t*	(*pfnFindEntityByString)( edict_t *pStartEdict, const char *pszField, const char *pszValue);
	int	(*pfnGetEntityIllum)( edict_t* pEnt );
	edict_t*	(*pfnFindEntityInSphere)( edict_t *pStartEdict, const float *org, float rad );
	edict_t*	(*pfnFindClientInPVS)( edict_t *pEdict );
	edict_t*	(*pfnFindClientInPHS)( edict_t *pEdict );
	edict_t*	(*pfnEntitiesInPVS)( edict_t *pplayer );
	edict_t*	(*pfnEntitiesInPHS)( edict_t *pplayer );
	void	(*pfnMakeVectors)( const float *rgflVector );
	void	(*pfnAngleVectors)( const float *rgflVector, float *forward, float *right, float *up );
	edict_t*	(*pfnCreateEntity)( void );
	void	(*pfnRemoveEntity)( edict_t* e );
	edict_t*	(*pfnCreateNamedEntity)( string_t className );
	void	(*pfnMakeStatic)( edict_t *ent );
	int	(*pfnEntIsOnFloor)( edict_t *e );
	int	(*pfnDropToFloor)( edict_t* e );
	int	(*pfnWalkMove)( edict_t *ent, float yaw, float dist, int iMode );
	void	(*pfnSetOrigin)( edict_t *e, const float *rgflOrigin );
	void	(*pfnSetAngles)( edict_t *e, const float *rgflAngles );
	void	(*pfnEmitSound)( edict_t *ent, int chan, const char *sample, float vol, float attn, int flags, int pitch );
	void	(*pfnEmitAmbientSound)( edict_t *ent, float *pos, const char *samp, float vol, float attn, int flags, int pitch );
	void	(*pfnTraceLine)( const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr );
	void	(*pfnTraceToss)( edict_t* pent, edict_t* pentToIgnore, TraceResult *ptr );
	void	(*pfnTraceHull)( const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr );
	void	(*pfnTraceModel)( const float *v1, const float *v2, edict_t *pent, TraceResult *ptr );
	const char *(*pfnTraceTexture)( edict_t *pTextureEntity, const float *v1, const float *v2 );
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
	void	(*pfnCVarRegister)( const char *name, const char *value, int flags, const char *desc );
	float	(*pfnCVarGetFloat)( const char *szVarName );
	const char* (*pfnCVarGetString)( const char *szVarName );
	void	(*pfnCVarSetFloat)( const char *szVarName, float flValue );
	void	(*pfnCVarSetString)( const char *szVarName, const char *szValue );
	void	(*pfnAlertMessage)( ALERT_TYPE level, char *szFmt, ... );
	void*	(*pfnPvAllocEntPrivateData)( edict_t *pEdict, long cb );
	void	(*pfnFreeEntPrivateData)( edict_t *pEdict );
	string_t	(*pfnAllocString)( const char *szValue );
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
	void	(*pfnClientPrintf)( edict_t* pEdict, int ptype, const char *szMsg );
	void	(*pfnServerPrint)( const char *szMsg );
	void	(*pfnAreaPortal)( edict_t *pEdict, bool enable );
	const char *(*pfnCmd_Args)( void );
	const char *(*pfnCmd_Argv)( int argc );
	int	(*pfnCmd_Argc)( void );
	void	(*pfnGetAttachment)( const edict_t *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles );
	void	(*pfnCRC_Init)( word *pulCRC );
	void	(*pfnCRC_ProcessBuffer)( word *pulCRC, void *p, int len );
	word	(*pfnCRC_Final)( word pulCRC );
	long	(*pfnRandomLong)( long  lLow,  long  lHigh );
	float	(*pfnRandomFloat)( float flLow, float flHigh );
	void	(*pfnSetView)( const edict_t *pClient, const edict_t *pViewent );
	void	(*pfnCrosshairAngle)( const edict_t *pClient, float pitch, float yaw );
	byte*	(*pfnLoadFile)( const char *filename, int *pLength );
	void	(*pfnFreeFile)( void *buffer );
	int	(*pfnCompareFileTime)( const char *filename1, const char *filename2, int *iCompare );
	void	(*pfnGetGameDir)( char *szGetGameDir );
	void	(*pfnStaticDecal)( const float *origin, int decalIndex, int entityIndex, int modelIndex );
	int	(*pfnPrecacheGeneric)( const char* s );
	int	(*pfnIsDedicatedServer)( void );	// is this a dedicated server?
	int	(*pfnIsMapValid)( char *filename );

	void	(*pfnInfo_RemoveKey)( char *s, char *key );	
	char*	(*pfnInfoKeyValue)( char *infobuffer, char *key );
	void	(*pfnSetKeyValue)( char *infobuffer, char *key, char *value );
	char*	(*pfnGetInfoKeyBuffer)( edict_t *e );	// passing in NULL gets the serverinfo
	void	(*pfnSetClientKeyValue)( int clientIndex, char *infobuffer, char *key, char *value );

	void	(*pfnSetSkybox)( const char *name );
	void	(*pfnPlayMusic)( const char *trackname, int flags );	// background track
	void	(*pfnDropClient)( int clientIndex );			// used for kick cheaters from server
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
	int	id;		// ordinal ID of this entity (used for entity <--> pointer conversions)
	edict_t	*pent;		// pointer to the in-game entity

	int	location;		// offset from the base data of this entity
	int	size;		// byte size of this entity's data
	int	flags;		// bit mask of transitions that this entity is in the PVS of
	string_t	classname;	// entity class name
} ENTITYTABLE;

#define FTYPEDESC_GLOBAL		0x0001	// This field is masked for global entity save/restore
#define MAX_LEVEL_CONNECTIONS		32	// These are encoded in the lower 16bits of ENTITYTABLE->flags

#define FENTTABLE_GLOBAL		0x10000000
#define FENTTABLE_MOVEABLE		0x20000000
#define FENTTABLE_REMOVED		0x40000000
#define FENTTABLE_PLAYER		0x80000000

typedef struct saverestore_s
{
	char		*pBaseData;	// start of all entity save data
	char		*pCurrentData;	// current buffer pointer for sequential access
	int		size;		// current data size
	int		bufferSize;	// total space for data
	int		tokenSize;	// size of the linear list of tokens
	int		tokenCount;	// number of elements in the pTokens table
	char		**pTokens;	// hash table of entity strings (sparse)
	int		currentIndex;	// holds a global entity table ID
	int		tableCount;	// number of elements in the entity table
	int		connectionCount;	// number of elements in the levelList[]
	ENTITYTABLE	*pTable;		// array of ENTITYTABLE elements (1 for each entity)
	LEVELLIST		levelList[MAX_LEVEL_CONNECTIONS]; // list of connections from this level

	// smooth transition
	int		fUseLandmark;
	char		szLandmarkName[64];	// landmark we'll spawn near in next level
	vec3_t		vecLandmarkOffset;	// for landmark transitions
	float		time;
	char		szCurrentMap[64];	// To check global entities
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
#define DEFINE_ENTITY_GLOBAL_FIELD( name, fieldtype )	_FIELD( entvars_t, name, fieldtype, 1, FTYPEDESC_GLOBAL )
#define DEFINE_GLOBAL_FIELD( type, name, fieldtype )	_FIELD( type, name, fieldtype, 1, FTYPEDESC_GLOBAL )

typedef struct 
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(DLL_FUNCTIONS)

	// initialize/shutdown the game (one-time call after loading of game .dll )
	void	(*pfnGameInit)( void );				
	int	(*pfnSpawn)( edict_t *pent );
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
	void	(*pfnParmsNewLevel)( void );
	void	(*pfnParmsChangeLevel)( void );

	 // returns string describing current .dll.  E.g., TeamFotrress 2, Half-Life
	const char *(*pfnGetGameDescription)( void );     
	// notify game .dll that engine is going to shut down.  Allows mod authors to set a breakpoint.
	void	(*pfnHostError)( const char *error_string );
} DLL_FUNCTIONS;

// TODO: create single func
// typedef DLL_FUNCTIONS *(*GetEntityAPI)( enginefuncs_t* engfuncs, globalvars_t *pGlobals );
// set pointer to globals, returns export of dll_functions and use CreateAPI as base offset

typedef void (*GIVEFNPTRSTODLL)( enginefuncs_t* engfuncs, globalvars_t *pGlobals );
typedef int (*APIFUNCTION)( DLL_FUNCTIONS *pFunctionTable, int interfaceVersion );
typedef void (*LINK_ENTITY_FUNC)( entvars_t *pev );

#endif//SVGAME_API_H