//=======================================================================
//			Copyright XashXT Group 2008 ©
//			sv_save.c - game serialization
//=======================================================================

#include "common.h"
#include "server.h"
#include "byteorder.h"
#include "matrix_lib.h"
#include "const.h"

// contains some info from globalvars_t
typedef struct save_header_s
{
	int	numEntities;	// actual edicts count
	int	numConnections;	// level transitions count
	float	time;		// sv.time at saved moment
	char	mapName[CS_SIZE];	// svs.mapname
} save_header_t;

typedef struct game_header_s
{
	int	mapCount;		// svs.mapcount
	char	mapName[CS_SIZE];	// svs.mapname
	char	comment[CS_SIZE];	// svs.comment
} game_header_t;

static TYPEDESCRIPTION gSaveHeader[] =
{
	DEFINE_FIELD( save_header_t, numEntities, FIELD_INTEGER ),
	DEFINE_FIELD( save_header_t, numConnections, FIELD_INTEGER ),
	DEFINE_FIELD( save_header_t, time, FIELD_TIME ),
	DEFINE_ARRAY( save_header_t, mapName, FIELD_CHARACTER, CS_SIZE ),
};

static TYPEDESCRIPTION gGameHeader[] =
{
	DEFINE_FIELD( game_header_t, mapCount, FIELD_INTEGER ),
	DEFINE_ARRAY( game_header_t, mapName, FIELD_CHARACTER, CS_SIZE ),
	DEFINE_ARRAY( game_header_t, comment, FIELD_CHARACTER, CS_SIZE ),
};

static TYPEDESCRIPTION gAdjacency[] =
{
	DEFINE_ARRAY( LEVELLIST, mapName, FIELD_CHARACTER, CS_SIZE ),
	DEFINE_ARRAY( LEVELLIST, landmarkName, FIELD_CHARACTER, CS_SIZE ),
	DEFINE_FIELD( LEVELLIST, pentLandmark, FIELD_EDICT ),
	DEFINE_FIELD( LEVELLIST, vecLandmarkOrigin, FIELD_VECTOR ),
};

static TYPEDESCRIPTION gETable[] =
{
	DEFINE_FIELD( ENTITYTABLE, id, FIELD_INTEGER ),
	DEFINE_FIELD( ENTITYTABLE, location, FIELD_INTEGER ),
	DEFINE_FIELD( ENTITYTABLE, size, FIELD_INTEGER ),
	DEFINE_FIELD( ENTITYTABLE, flags, FIELD_INTEGER ),
	DEFINE_FIELD( ENTITYTABLE, classname, FIELD_STRING ),
};

// FIXME: implement _rotr into Xash::stdlib
static uint SV_HashString( const char *pszToken )
{
	uint hash = 0;

	while( *pszToken )
		hash = _rotr( hash, 4 ) ^ *pszToken++;
	return hash;
}

static word SV_TokenHash( const char *pszToken )
{
	word	hash; 
	int	i, index;

	// only valid if SAVERESTOREDATA have been initialized
	if( !svgame.SaveData.pTokens || !svgame.SaveData.tokenCount ) return 0;

	hash = (word)(SV_HashString( pszToken ) % (uint)svgame.SaveData.tokenCount );
	for( i = 0; i < svgame.SaveData.tokenCount; i++ )
	{
		index = hash + i;
		if( index >= svgame.SaveData.tokenCount )
			index -= svgame.SaveData.tokenCount;

		if( !svgame.SaveData.pTokens[index] || !com.strcmp( pszToken, svgame.SaveData.pTokens[index] ))
		{
			svgame.SaveData.pTokens[index] = (char *)pszToken;
			return index;
		}
	}
	
	// consider doing overflow table(s) after the main table & limiting linear hash table search
	MsgDev( D_ERROR, "SV_TokenHash is completely full!\n" );
	return 0;
}

static void SV_UpdateTokens( const TYPEDESCRIPTION *fields, int fieldCount )
{
	int	i;

	for( i = 0; i < fieldCount; i++, fields++ )
		SV_TokenHash( fields->fieldName );
}

static void SV_AddSaveLump( wfile_t *f, const char *lumpname, void *data, size_t len, bool compress )
{
	if( f ) WAD_Write( f, lumpname, data, len, TYPE_BINDATA, ( compress ? CMP_ZLIB : CMP_NONE ));
}

static void SV_SetPair( const char *name, const char *value, dkeyvalue_t *cvars, int *numpairs )
{
	if( !name || !value ) return; // ignore emptycvars
	cvars[*numpairs].epair[DENT_KEY] = StringTable_SetString( svgame.hStringTable, name );
	cvars[*numpairs].epair[DENT_VAL] = StringTable_SetString( svgame.hStringTable, value);
	(*numpairs)++; // increase epairs
}

static void SV_SaveBuffer( wfile_t *f, const char *lumpname, bool compress )
{
	// write result into lump
	SV_AddSaveLump( f, lumpname, svgame.SaveData.pBaseData, svgame.SaveData.size, compress );

	// clear buffer after writing
	Mem_Set( svgame.SaveData.pBaseData, 0, svgame.SaveData.bufferSize );
	svgame.SaveData.pCurrentData = svgame.SaveData.pBaseData;
	svgame.SaveData.size = 0; // reset current bufSize
}

static void SV_ReadBuffer( wfile_t *f, const char *lumpname )
{
	// an older pointer will automatically free memory when calling WAD_Close
	// so we don't need to care about it
	svgame.SaveData.pBaseData = WAD_Read( f, lumpname, &svgame.SaveData.bufferSize, TYPE_BINDATA );
	svgame.SaveData.pCurrentData = svgame.SaveData.pBaseData;
	svgame.SaveData.size = 0; // reset current bufSize
}

static void SV_SaveEngineData( wfile_t *f )
{
	byte		*portalstate = NULL;
	int		i, portalsize, numpairs = 0;
	dkeyvalue_t	cvbuffer[MAX_FIELDS];
	string_t		csbuffer[MAX_CONFIGSTRINGS];

	// save areaportals state
	portalstate = Z_Malloc( MAX_MAP_AREAPORTALS );
	pe->GetAreaPortals( &portalstate, &portalsize );
	SV_AddSaveLump( f, LUMP_AREASTATE, portalstate, portalsize, true );
	if( portalstate ) Mem_Free( portalstate ); // release portalinfo

	// make sure what all configstrings are passes through StringTable system
	for( i = 0; i < MAX_CONFIGSTRINGS; i++ )
		csbuffer[i] = StringTable_SetString( svgame.hStringTable, sv.configstrings[i] );
	SV_AddSaveLump( f, LUMP_CFGSTRING, &csbuffer, sizeof( csbuffer ), true );

	// save latched cvars	
	Cvar_LookupVars( CVAR_LATCH, cvbuffer, &numpairs, SV_SetPair );
	SV_AddSaveLump( f, LUMP_GAMECVARS, cvbuffer, numpairs * sizeof( dkeyvalue_t ), true );
}

static void SV_SaveServerData( wfile_t *f )
{
	SAVERESTOREDATA	*pSaveData;
	string_t		hash_strings[4095];
	int		i, numstrings;
	ENTITYTABLE	*pTable;
	save_header_t	shdr;
	game_header_t	ghdr;

	// initialize local mempool
	svgame.temppool = Mem_AllocPool( "Save Pool" );

	// initialize SAVERESTOREDATA
	Mem_Set( &svgame.SaveData, 0, sizeof( SAVERESTOREDATA ));
	svgame.SaveData.bufferSize = 0x80000;	// reserve 512K for now
	svgame.SaveData.pBaseData = Mem_Alloc( svgame.temppool, svgame.SaveData.bufferSize );
	svgame.SaveData.pCurrentData = svgame.SaveData.pBaseData;
	svgame.SaveData.tokenCount = 0xFFF;	// assume a maximum of 4K-1 symbol table entries
	svgame.SaveData.pTokens = Mem_Alloc( svgame.temppool, svgame.SaveData.tokenCount * sizeof( char* ));
	svgame.SaveData.time = svgame.globals->time;
	pSaveData = svgame.globals->pSaveData = &svgame.SaveData;

	// initialize level connections
	svgame.dllFuncs.pfnBuildLevelList();

	// initialize save header	
	shdr.numEntities = svgame.globals->numEntities;
	shdr.numConnections = svgame.SaveData.connectionCount;
	shdr.time = svgame.SaveData.time;
	com.strncpy( shdr.mapName, svs.mapname, CS_SIZE );

	// initialize game header
	ghdr.mapCount = svs.spawncount;
	com.strncpy( ghdr.mapName, svs.mapname, CS_SIZE );
	com.strncpy( ghdr.comment, svs.comment, CS_SIZE );

	// initialize ENTITYTABLE
	pSaveData->tableCount = svgame.globals->numEntities;
	pSaveData->pTable = Mem_Alloc( svgame.temppool, pSaveData->tableCount * sizeof( ENTITYTABLE ));
	
	for( i = 0; i < svgame.globals->numEntities; i++ )
	{
		edict_t	*pent = EDICT_NUM( i );

		pTable = &pSaveData->pTable[i];		
		pTable->id = pent->serialnumber;
		pTable->pent = pent;

		// setup some flags
		if( pent->v.flags & FL_CLIENT ) pTable->flags |= FENTTABLE_PLAYER;
		if( pent->free ) pTable->flags |= FENTTABLE_REMOVED;
	}

	// write save header
	svgame.dllFuncs.pfnSaveWriteFields( pSaveData, "Save Header", &shdr, gSaveHeader, ARRAYSIZE( gSaveHeader ));

	// write level connections
	for( i = 0; i < pSaveData->connectionCount; i++ )
	{
		LEVELLIST	*pList = &pSaveData->levelList[i];
		svgame.dllFuncs.pfnSaveWriteFields( pSaveData, "ADJACENCY", pList, gAdjacency, ARRAYSIZE( gAdjacency ));
	}

	SV_SaveBuffer( f, LUMP_ADJACENCY, false );

	// write entity descriptions
	for( i = 0; i < svgame.globals->numEntities; i++ )
	{
		edict_t	*pent = EDICT_NUM( i );

		if( !pent->free && pent->v.classname )
			svgame.dllFuncs.pfnSave( pent, pSaveData );
		pSaveData->currentIndex++; // move pointer
	}

	SV_SaveBuffer( f, LUMP_ENTITIES, false );

	// write entity table
	for( i = 0; i < pSaveData->tableCount; i++ )
	{
		pTable = &pSaveData->pTable[i];
		svgame.dllFuncs.pfnSaveWriteFields( pSaveData, "ETABLE", pTable, gETable, ARRAYSIZE( gETable ));
	}

	// write result into lump
	SV_SaveBuffer( f, LUMP_ENTTABLE, false );

	// write game header
	svgame.dllFuncs.pfnSaveWriteFields( pSaveData, "Game Header", &ghdr, gGameHeader, ARRAYSIZE( gGameHeader ));

	// at end of description save globals
	svgame.dllFuncs.pfnSaveGlobalState( pSaveData );

	// write result into lump
	SV_SaveBuffer( f, LUMP_GLOBALS, false );

	// save used hash strings
	for( i = numstrings = 0; i < svgame.SaveData.tokenCount; i++ )
	{
		if( !svgame.SaveData.pTokens[i] ) continue;
		hash_strings[numstrings] = StringTable_SetString( svgame.hStringTable, svgame.SaveData.pTokens[i] );
		numstrings++;
	}

	// save hash table
	SV_AddSaveLump( f, LUMP_HASHTABLE, hash_strings, numstrings * sizeof( string_t ), true );

	// do cleanup operations
	Mem_Set( &svgame.SaveData, 0, sizeof( SAVERESTOREDATA ));
	svgame.globals->pSaveData = NULL;
	Mem_FreePool( &svgame.temppool );
}

/*
=============
SV_WriteSaveFile
=============
*/
void SV_WriteSaveFile( const char *name, bool autosave )
{
	wfile_t	*savfile = NULL;
	char	path[MAX_SYSPATH];

	if( sv.state != ss_active ) return;
	if( svgame.globals->deathmatch || svgame.globals->coop || svgame.globals->teamplay )
	{
		MsgDev( D_ERROR, "SV_WriteSaveFile: can't savegame in a multiplayer\n" );
		return;
	}
	if( Host_MaxClients() == 1 && svs.clients[0].edict->v.health <= 0 )
	{
		MsgDev( D_ERROR, "SV_WriteSaveFile: can't savegame while dead!\n" );
		return;
	}

	com.sprintf( path, "save/%s", name );
	savfile = WAD_Open( path, "wb" );

	if( !savfile )
	{
		MsgDev(D_ERROR, "SV_WriteSaveFile: failed to open %s\n", path );
		return;
	}

	com.snprintf( svs.comment, CS_SIZE, "%s - %s", sv.configstrings[CS_NAME], timestamp( TIME_FULL ));
	MsgDev( D_INFO, "Saving game..." );

	// write lumps
	SV_SaveEngineData( savfile );
	SV_SaveServerData( savfile );
	StringTable_Save( svgame.hStringTable, savfile );	// must be last

	WAD_Close( savfile );

	MsgDev( D_INFO, "done.\n" );
}

void SV_ReadComment( wfile_t *l )
{
	SAVERESTOREDATA	*pSaveData;
	game_header_t	ghdr;

	// initialize SAVERESTOREDATA
	Mem_Set( &svgame.SaveData, 0, sizeof( SAVERESTOREDATA ));
	svgame.SaveData.tokenCount = 0xFFF;	// assume a maximum of 4K-1 symbol table entries
	svgame.SaveData.pTokens = (char **)Z_Malloc( svgame.SaveData.tokenCount * sizeof( char* ));
	pSaveData = svgame.globals->pSaveData = &svgame.SaveData;
	SV_ReadBuffer( l, LUMP_GLOBALS );

	SV_UpdateTokens( gGameHeader, ARRAYSIZE( gGameHeader ));

	// read game header
	svgame.dllFuncs.pfnSaveReadFields( pSaveData, "Game Header", &ghdr, gGameHeader, ARRAYSIZE( gGameHeader ));

	com.strncpy( svs.comment, ghdr.comment, CS_SIZE );
	if( svgame.SaveData.pTokens ) Mem_Free( svgame.SaveData.pTokens );
	Mem_Set( &svgame.SaveData, 0, sizeof( SAVERESTOREDATA ));
}

void SV_ReadHashTable( wfile_t *l )
{
	string_t	*in_table;
	int	i, hash_size;

	in_table = (string_t *)WAD_Read( l, LUMP_HASHTABLE, &hash_size, TYPE_BINDATA );

	for( i = 0; i < hash_size / sizeof( string_t ); i++, in_table++ )
		SV_TokenHash( STRING( *in_table ));
}

void SV_ReadCvars( wfile_t *l )
{
	dkeyvalue_t	*in;
	int		i, numpairs;
	const char	*name, *value;

	in = (dkeyvalue_t *)WAD_Read( l, LUMP_GAMECVARS, &numpairs, TYPE_BINDATA );
	if( numpairs % sizeof( *in )) Host_Error( "Sav_LoadCvars: funny lump size\n" );
	numpairs /= sizeof( dkeyvalue_t );

	for( i = 0; i < numpairs; i++ )
	{
		name = StringTable_GetString( svgame.hStringTable, in[i].epair[DENT_KEY] );
		value = StringTable_GetString( svgame.hStringTable, in[i].epair[DENT_VAL] );
		Cvar_SetLatched( name, value );
	}
}

void SV_ReadCfgString( wfile_t *l )
{
	string_t	*in;
	int	i, numstrings;

	in = (string_t *)WAD_Read( l, LUMP_CFGSTRING, &numstrings, TYPE_BINDATA );
	if( numstrings % sizeof(*in)) Host_Error( "Sav_LoadCfgString: funny lump size\n" );
	numstrings /= sizeof( string_t ); // because old saves can contain last values of MAX_CONFIGSTRINGS

	// unpack the cfg string data
	for( i = 0; i < numstrings; i++ )
		com.strncpy( sv.configstrings[i], StringTable_GetString( svgame.hStringTable, in[i] ), CS_SIZE );  
}

void SV_ReadAreaPortals( wfile_t *l )
{
	byte	*in;
	int	size;

	in = WAD_Read( l, LUMP_AREASTATE, &size, TYPE_BINDATA );
	pe->SetAreaPortals( in, size ); // CM_ReadPortalState
}

void SV_ReadGlobals( wfile_t *l )
{
	SAVERESTOREDATA	*pSaveData;
	game_header_t	ghdr;

	// initialize local mempool
	svgame.temppool = Mem_AllocPool( "Restore Pool" );

	// initialize SAVERESTOREDATA
	Mem_Set( &svgame.SaveData, 0, sizeof( SAVERESTOREDATA ));
	svgame.SaveData.tokenCount = 0xFFF;	// assume a maximum of 4K-1 symbol table entries
	svgame.SaveData.pTokens = Mem_Alloc( svgame.temppool, svgame.SaveData.tokenCount * sizeof( char* ));
	pSaveData = svgame.globals->pSaveData = &svgame.SaveData;
	SV_ReadBuffer( l, LUMP_GLOBALS );

	SV_ReadHashTable( l );

	// read the game header
	svgame.dllFuncs.pfnSaveReadFields( pSaveData, "Game Header", &ghdr, gGameHeader, ARRAYSIZE( gGameHeader ));

	svs.spawncount = ghdr.mapCount; // restore spawncount
	com.strncpy( svs.comment, ghdr.comment, MAX_STRING );
	com.strncpy( svs.mapname, ghdr.mapName, MAX_STRING );

	// restore global state
	svgame.dllFuncs.pfnRestoreGlobalState( pSaveData );
	svgame.dllFuncs.pfnServerDeactivate();

	// leave pool unfreed, because we have partially filled hash tokens
}

void SV_RestoreEdict( edict_t *ent )
{
	SV_SetPhysForce( ent ); // restore forces ...
	SV_SetMassCentre( ent ); // and mass force
}

void SV_ReadEntities( wfile_t *l )
{
	SAVERESTOREDATA	*pSaveData;
	ENTITYTABLE	*pTable;
	LEVELLIST		*pList;
	save_header_t	shdr;
	int		i;

	// SAVERESTOREDATA partially initialized, continue filling
	pSaveData = svgame.globals->pSaveData = &svgame.SaveData;
	SV_ReadBuffer( l, LUMP_ADJACENCY );
	
	// read save header
	svgame.dllFuncs.pfnSaveReadFields( pSaveData, "Save Header", &shdr, gSaveHeader, ARRAYSIZE( gSaveHeader ));

	SV_ConfigString( CS_MAXCLIENTS, va( "%i", Host_MaxClients( )));
	com.strncpy( sv.name, shdr.mapName, MAX_STRING );
	svgame.globals->mapname = MAKE_STRING( sv.name );
	svgame.globals->time = shdr.time;
	sv.time = (int)(shdr.time * 1000);
	pSaveData->connectionCount = shdr.numConnections;

	// read ADJACENCY sections
	for( i = 0; i < pSaveData->connectionCount; i++ )
	{
		pList = &pSaveData->levelList[i];		
		svgame.dllFuncs.pfnSaveReadFields( pSaveData, "ADJACENCY", pList, gAdjacency, ARRAYSIZE( gAdjacency ));
	}

	// initialize ENTITYTABLE
	pSaveData->tableCount = shdr.numEntities;
	pSaveData->pTable = Mem_Alloc( svgame.temppool, pSaveData->tableCount * sizeof( ENTITYTABLE ));
	while( svgame.globals->numEntities < shdr.numEntities ) SV_AllocEdict(); // allocate edicts

	SV_ReadBuffer( l, LUMP_ENTTABLE );

	// read entity table
	for( i = 0; i < pSaveData->tableCount; i++ )
	{
		edict_t	*pent = EDICT_NUM( i );

		pTable = &pSaveData->pTable[i];		
		svgame.dllFuncs.pfnSaveReadFields( pSaveData, "ETABLE", pTable, gETable, ARRAYSIZE( gETable ));

		if( pTable->id != pent->serialnumber )
			MsgDev( D_ERROR, "ETABLE id( %i ) != edict->id( %i )\n", pTable->id, pent->serialnumber );
		if( pTable->flags & FENTTABLE_REMOVED ) SV_FreeEdict( pent );
		else pent = SV_AllocPrivateData( pent, pTable->classname );
		pTable->pent = pent;
	}

	SV_ReadBuffer( l, LUMP_ENTITIES );
	pSaveData->fUseLandmark = true;

	// and read entities ...
	for( i = 0; i < pSaveData->tableCount; i++ )
	{
		edict_t	*pent = EDICT_NUM( i );
		pTable = &pSaveData->pTable[i];

		// ignore removed edicts
		if( !pent->free && pTable->classname )
		{
			svgame.dllFuncs.pfnRestore( pent, pSaveData, false );
			SV_RestoreEdict( pent );
		}
		pSaveData->currentIndex++;
	}
		
	// do cleanup operations
	Mem_Set( &svgame.SaveData, 0, sizeof( SAVERESTOREDATA ));
	svgame.globals->pSaveData = NULL;
	Mem_FreePool( &svgame.temppool );
}

/*
=============
SV_ReadSaveFile
=============
*/
void SV_ReadSaveFile( const char *name )
{
	char		path[MAX_SYSPATH];
	wfile_t		*savfile;

	com.sprintf( path, "save/%s", name );
	savfile = WAD_Open( path, "rb" );

	if( !savfile )
	{
		MsgDev(D_ERROR, "SV_ReadSaveFile: can't open %s\n", path );
		return;
	}

	sv.loadgame = true;	// to avoid clearing StringTables in SV_Shutdown
	StringTable_Delete( svgame.hStringTable ); // remove old string table
	svgame.hStringTable = StringTable_Load( savfile, name );
	SV_ReadCvars( savfile );
	
	SV_InitGame(); // start a new game fresh with new cvars

	// SV_Shutdown will be clear svs ans sv struct, so load it here
	sv.loadgame = true;	// restore state
	SV_ReadGlobals( savfile );
	WAD_Close( savfile );
	CL_Drop();
}

/*
=============
SV_ReadLevelFile
=============
*/
void SV_ReadLevelFile( const char *name )
{
	char		path[MAX_SYSPATH];
	wfile_t		*savfile;

	com.sprintf( path, "save/%s", name );
	savfile = WAD_Open( path, "rb" );

	if( !savfile )
	{
		MsgDev( D_ERROR, "SV_ReadLevelFile: can't open %s\n", path );
		return;
	}

	SV_ReadCfgString( savfile );
	SV_ReadAreaPortals( savfile );
	SV_ReadEntities( savfile );
	WAD_Close( savfile );
}

bool SV_GetComment( char *comment, int savenum )
{
	wfile_t		*savfile;
	int		result;

	if( !comment ) return false;
	result = WAD_Check( va( "save/save%i.bin", savenum )); 

	switch( result )
	{
	case 0:
		com.strncpy( comment, "<empty>", MAX_STRING );
		return false;
	case 1:	break;
	default:
		com.strncpy( comment, "<corrupted>", MAX_STRING );
		return false;
	}

	savfile = WAD_Open( va( "save/save%i.bin", savenum ), "rb" );
	SV_ReadComment( savfile );
	com.strncpy( comment, svs.comment, MAX_STRING );
	WAD_Close( savfile );

	return true;
}