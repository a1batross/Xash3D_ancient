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
	int	maxEntities;	// total edicts count
	int	maxClients;	// for multiplayer saves
	float	time;		// sv.time at saved moment
	char	mapName[CS_SIZE];	// svs.mapname
} save_header_t;

typedef struct game_header_s
{
	int	mapCount;		// svs.mapcount
	int	serverflags;	// svgame.serverflags
	int	found_secrets;	// number of secrets found
	char	mapName[CS_SIZE];	// svs.mapname
	string	comment;		// svs.comment
} game_header_t;

static TYPEDESCRIPTION gSaveHeader[] =
{
	DEFINE_FIELD( save_header_t, numEntities, FIELD_INTEGER ),
	DEFINE_FIELD( save_header_t, numConnections, FIELD_INTEGER ),
	DEFINE_FIELD( save_header_t, maxEntities, FIELD_INTEGER ),
	DEFINE_FIELD( save_header_t, maxClients, FIELD_INTEGER ),
	DEFINE_FIELD( save_header_t, time, FIELD_TIME ),
	DEFINE_ARRAY( save_header_t, mapName, FIELD_CHARACTER, CS_SIZE ),
};

static TYPEDESCRIPTION gGameHeader[] =
{
	DEFINE_FIELD( game_header_t, mapCount, FIELD_INTEGER ),
	DEFINE_FIELD( game_header_t, serverflags, FIELD_INTEGER ),
	DEFINE_FIELD( game_header_t, found_secrets, FIELD_INTEGER ),
	DEFINE_ARRAY( game_header_t, mapName, FIELD_CHARACTER, CS_SIZE ),
	DEFINE_ARRAY( game_header_t, comment, FIELD_CHARACTER, MAX_STRING ),
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
	dkeyvalue_t	cvbuffer[512];
	string_t		csbuffer[MAX_CONFIGSTRINGS];

	// save areaportals state
	CM_GetAreaPortals( &portalstate, &portalsize );
	SV_AddSaveLump( f, LUMP_AREASTATE, portalstate, portalsize, true );
	if( portalstate ) Mem_Free( portalstate ); // allocated in physic.dll

	// make sure what all configstrings are passes through StringTable system
	for( i = 0; i < MAX_CONFIGSTRINGS; i++ )
		csbuffer[i] = StringTable_SetString( svgame.hStringTable, sv.configstrings[i] );
	SV_AddSaveLump( f, LUMP_CFGSTRING, &csbuffer, sizeof( csbuffer ), true );

	// save latched cvars	
	Cvar_LookupVars( CVAR_LATCH, cvbuffer, &numpairs, SV_SetPair );
	SV_AddSaveLump( f, LUMP_GAMECVARS, cvbuffer, numpairs * sizeof( dkeyvalue_t ), true );
}

static void SV_SaveServerData( wfile_t *f, const char *name, bool bUseLandMark )
{
	SAVERESTOREDATA	*pSaveData;
	string_t		hash_strings[4095];
	int		i, numstrings;
	int		level_flags = 0;
	ENTITYTABLE	*pTable;
	save_header_t	shdr;
	game_header_t	ghdr;

	// initialize SAVERESTOREDATA
	Mem_Set( &svgame.SaveData, 0, sizeof( SAVERESTOREDATA ));
	svgame.SaveData.bufferSize = 0x80000;	// reserve 512K for now
	svgame.SaveData.pBaseData = Mem_Alloc( svgame.temppool, svgame.SaveData.bufferSize );
	svgame.SaveData.pCurrentData = svgame.SaveData.pBaseData;
	svgame.SaveData.tokenCount = 0xFFF;	// assume a maximum of 4K-1 symbol table entries
	svgame.SaveData.pTokens = Mem_Alloc( svgame.temppool, svgame.SaveData.tokenCount * sizeof( char* ));
	svgame.SaveData.time = svgame.globals->time;
	pSaveData = svgame.globals->pSaveData = &svgame.SaveData;

	// initialize the ENTITYTABLE
	pSaveData->tableCount = svgame.globals->numEntities;
	pSaveData->pTable = Mem_Alloc( svgame.temppool, pSaveData->tableCount * sizeof( ENTITYTABLE ));

	for( i = 0; i < svgame.globals->numEntities; i++ )
	{
		pTable = &pSaveData->pTable[i];		
		
		pTable->pent = EDICT_NUM( i );
		// setup some flags
		if( pTable->pent->v.flags & FL_CLIENT ) pTable->flags |= FENTTABLE_PLAYER;
		if( pTable->pent->free ) pTable->flags |= FENTTABLE_REMOVED;
	}

	pSaveData->fUseLandmark = bUseLandMark;

	// initialize level connections
	svgame.dllFuncs.pfnParmsChangeLevel();

	// initialize save header	
	shdr.numEntities = svgame.globals->numEntities;
	shdr.maxEntities = svgame.globals->maxEntities;
	shdr.numConnections = svgame.SaveData.connectionCount;
	shdr.maxClients = svgame.globals->maxClients;
	shdr.time = svgame.SaveData.time;
	com.strncpy( shdr.mapName, svs.mapname, CS_SIZE );

	// initialize game header
	ghdr.mapCount = svs.spawncount;
	ghdr.serverflags = svgame.globals->serverflags;
	ghdr.found_secrets = svgame.globals->found_secrets;
	com.strncpy( ghdr.mapName, sv.name, CS_SIZE );
	Mem_Copy( ghdr.comment, svs.comment, MAX_STRING ); // can't use strncpy!

	// write save header
	svgame.dllFuncs.pfnSaveWriteFields( pSaveData, "Save Header", &shdr, gSaveHeader, ARRAYSIZE( gSaveHeader ));

	// write level connections
	for( i = 0; i < pSaveData->connectionCount; i++ )
	{
		LEVELLIST	*pList = &pSaveData->levelList[i];
		svgame.dllFuncs.pfnSaveWriteFields( pSaveData, "ADJACENCY", pList, gAdjacency, ARRAYSIZE( gAdjacency ));

		if( sv.changelevel && !com.strcmp( pList->mapName, name ))
		{
			level_flags = (1<<i);
			Msg( "%s set level flags to: %x\n", name, level_flags );
		}
	}

	SV_SaveBuffer( f, LUMP_ADJACENCY, false );

	// write entity descriptions
	for( i = 0; i < svgame.globals->numEntities; i++ )
	{
		edict_t		*pent = EDICT_NUM( i );
		ENTITYTABLE	*pTable = &pSaveData->pTable[pSaveData->currentIndex];

		if( sv.changelevel )
		{
			bool	bSave = false;
		
			// check for client ents
			if( pTable->flags & (FENTTABLE_PLAYER|FENTTABLE_GLOBAL))
				bSave = true;

			if( pTable->flags & FENTTABLE_MOVEABLE && pTable->flags & level_flags )
				bSave = true;

			if( bSave )
			{
				svgame.dllFuncs.pfnSave( pent, pSaveData );
				if( pTable->classname && pTable->size )
					pTable->id = pent->serialnumber;
				else pTable->flags |= FENTTABLE_REMOVED;
			}
			else pTable->flags |= FENTTABLE_REMOVED;

		}
		else
		{
			// savegame
			if( !pent->free && pent->v.classname )
			{
				svgame.dllFuncs.pfnSave( pent, pSaveData );
				if( pTable->classname && pTable->size )
					pTable->id = pent->serialnumber;
				else pTable->flags |= FENTTABLE_REMOVED;
			}
			else pTable->flags |= FENTTABLE_REMOVED;
		}
		pSaveData->currentIndex++; // move pointer
	}

	SV_SaveBuffer( f, LUMP_BASEENTS, false );

	// write entity table
	for( i = 0; i < pSaveData->tableCount; i++ )
	{
		pTable = &pSaveData->pTable[i];

		// entityes without classname can't be restored
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
}

/* 
================== 
SV_SaveGetName
================== 
*/  
void SV_SaveGetName( int lastnum, char *filename )
{
	int	a, b;

	if( !filename ) return;
	if( lastnum < 0 || lastnum > 99 )
	{
		// bound
		com.strcpy( filename, "save99" );
		return;
	}

	a = lastnum / 10;
	b = lastnum % 10;

	com.sprintf( filename, "save%i%i", a, b );
}

/*
=============
SV_WriteSaveFile
=============
*/
void SV_WriteSaveFile( const char *inname, bool autosave, bool bUseLandmark )
{
	wfile_t	*savfile = NULL;
	int	n, minutes, tens, units;
	string	path, name;
	float	total;
	
	if( sv.state != ss_active ) return;
	if( svgame.globals->deathmatch || svgame.globals->coop || svgame.globals->teamplay )
	{
		MsgDev( D_ERROR, "SV_WriteSaveFile: can't savegame in a multiplayer\n" );
		return;
	}
	if( sv_maxclients->integer == 1 && svs.clients[0].edict->v.health <= 0 )
	{
		MsgDev( D_ERROR, "SV_WriteSaveFile: can't savegame while dead!\n" );
		return;
	}

	if( !com.stricmp( inname, "new" ))
	{
		// scan for a free filename
		for( n = 0; n < 100; n++ )
		{
			SV_SaveGetName( n, name );
			if( !FS_FileExists( va( "save/%s.sav", name )))
				break;
		}
		if( n == 100 )
		{
			Msg( "^3ERROR: no free slots for savegame\n" );
			return;
		}
	}
	else com.strncpy( name, inname, sizeof( name ));

	com.sprintf( path, "save/%s.sav", name );

	// make sure what oldsave is removed
	if( FS_FileExists( va( "save/%s.sav", name )))
		FS_Delete( va( "%s/save/%s.sav", GI->gamedir, name ));
	if( FS_FileExists( va( "save/%s.%s", name, CL_LevelshotType( ))))
		FS_Delete( va( "%s/save/%s.%s", GI->gamedir, name, CL_LevelshotType( )));

	savfile = WAD_Open( path, "wb" );

	if( !savfile )
	{
		MsgDev( D_ERROR, "SV_WriteSaveFile: failed to open %s\n", path );
		return;
	}

	// calc time
	total = svgame.globals->time;
	minutes = (int)total / 60;
	n = total - minutes * 60;
	tens = n / 10;
	units = n % 10;

	// split comment to sections
	com.strncpy( svs.comment, sv.configstrings[CS_NAME], CS_SIZE );
	com.strncpy( svs.comment + CS_SIZE, timestamp( TIME_DATE_ONLY ), CS_TIME );
	com.strncpy( svs.comment + CS_SIZE + CS_TIME, timestamp( TIME_NO_SECONDS ), CS_TIME );
	com.strncpy( svs.comment + CS_SIZE + (CS_TIME * 2), va( "%i:%i%i", minutes, tens, units ), CS_SIZE );
	if( !autosave ) MsgDev( D_INFO, "Saving game..." );

	// write lumps
	SV_SaveEngineData( savfile );
	SV_SaveServerData( savfile, name, bUseLandmark );
	StringTable_Save( svgame.hStringTable, savfile );	// must be last

	WAD_Close( savfile );

	// write saveshot for preview, but autosave 
	if( !autosave ) Cbuf_AddText( va( "saveshot \"%s\"\n", name ));
	if( !autosave ) MsgDev( D_INFO, "done.\n" );
}

void SV_ReadComment( wfile_t *l )
{
	SAVERESTOREDATA	*pSaveData;
	game_header_t	ghdr;

 	// init the game to get acess for read funcs
	if( !svgame.hInstance ) SV_LoadProgs( "server" );

	// initialize SAVERESTOREDATA
	Mem_Set( &svgame.SaveData, 0, sizeof( SAVERESTOREDATA ));
	svgame.SaveData.tokenCount = 0xFFF;	// assume a maximum of 4K-1 symbol table entries
	svgame.SaveData.pTokens = (char **)Mem_Alloc( host.mempool, svgame.SaveData.tokenCount * sizeof( char* ));
	pSaveData = svgame.globals->pSaveData = &svgame.SaveData;
	SV_ReadBuffer( l, LUMP_GLOBALS );

	SV_UpdateTokens( gGameHeader, ARRAYSIZE( gGameHeader ));

	// read game header
	svgame.dllFuncs.pfnSaveReadFields( pSaveData, "Game Header", &ghdr, gGameHeader, ARRAYSIZE( gGameHeader ));

	Mem_Copy( svs.comment, ghdr.comment, MAX_STRING );
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
	if( numpairs % sizeof( *in )) Host_Error( "SV_ReadCvars: funny lump size\n" );
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
	CM_SetAreaPortals( in, size ); // CM_ReadPortalState();
}

void SV_ReadGlobals( wfile_t *l )
{
	SAVERESTOREDATA	*pSaveData;
	game_header_t	ghdr;

	// initialize SAVERESTOREDATA
	Mem_Set( &svgame.SaveData, 0, sizeof( SAVERESTOREDATA ));
	svgame.SaveData.tokenCount = 0xFFF;	// assume a maximum of 4K-1 symbol table entries
	svgame.SaveData.pTokens = Mem_Alloc( svgame.temppool, svgame.SaveData.tokenCount * sizeof( char* ));
	pSaveData = svgame.globals->pSaveData = &svgame.SaveData;
	SV_ReadBuffer( l, LUMP_GLOBALS );

	SV_ReadHashTable( l );

	// read the game header
	svgame.dllFuncs.pfnSaveReadFields( pSaveData, "Game Header", &ghdr, gGameHeader, ARRAYSIZE( gGameHeader ));
	svgame.globals->serverflags= ghdr.serverflags;	// across transition flags (e.g. Q1 runes)
		
	if( sv.loadgame && !sv.changelevel )
	{
		Msg( "SV_ReadGlobals()\n" );
		svs.spawncount = ghdr.mapCount; // restore spawncount
		svgame.globals->found_secrets = ghdr.found_secrets;
		Mem_Copy( svs.comment, ghdr.comment, MAX_STRING );
		com.strncpy( svs.mapname, ghdr.mapName, MAX_STRING );
	}

	// restore global state
	svgame.dllFuncs.pfnRestoreGlobalState( pSaveData );

// FIXME: this is needs ?
//	svgame.dllFuncs.pfnServerDeactivate();
}

void SV_ReadEntities( wfile_t *l )
{
	SAVERESTOREDATA	*pSaveData;
	ENTITYTABLE	*pTable;
	LEVELLIST		*pList;
	save_header_t	shdr;
	edict_t		*ent;
	int		i, level_flags = 0;
	int		num_moveables = 0;

	// initialize world properly
	ent = EDICT_NUM( 0 );
	if( ent->free ) SV_InitEdict( ent );
	ent->v.model = MAKE_STRING( sv.configstrings[CS_MODELS+1] );
	ent->v.modelindex = 1;	// world model
	ent->v.solid = SOLID_BSP;
	ent->v.movetype = MOVETYPE_PUSH;

	// SAVERESTOREDATA partially initialized, continue filling
	pSaveData = svgame.globals->pSaveData = &svgame.SaveData;
	SV_ReadBuffer( l, LUMP_ADJACENCY );

	if( sv.loadgame )
	{
		pSaveData->fUseLandmark = true;
	}
	else if( sv.changelevel )
	{
		if( com.strlen( sv.startspot ))
			pSaveData->fUseLandmark = true;
		else pSaveData->fUseLandmark = false; // can be changed later
	}			

	// read save header
	svgame.dllFuncs.pfnSaveReadFields( pSaveData, "Save Header", &shdr, gSaveHeader, ARRAYSIZE( gSaveHeader ));
	pSaveData->connectionCount = shdr.numConnections;

	if( sv.loadgame )
	{
		svgame.globals->maxClients = shdr.maxClients;
		svgame.globals->maxEntities = shdr.maxEntities;

		// FIXME: set it properly
		Cvar_FullSet( "sv_maxclients", va( "%i", shdr.maxClients ), CVAR_SERVERINFO|CVAR_LATCH );
		com.strncpy( sv.name, shdr.mapName, MAX_STRING );
		svgame.globals->mapname = MAKE_STRING( sv.name );

		// holds during changelevel, no needs to save\restore
		svgame.globals->startspot = MAKE_STRING( sv.startspot );
		svgame.globals->time = shdr.time;
		sv.time = svgame.globals->time * 1000; 
	}

	// read ADJACENCY sections
	for( i = 0; i < pSaveData->connectionCount; i++ )
	{
		pList = &pSaveData->levelList[i];		
		svgame.dllFuncs.pfnSaveReadFields( pSaveData, "ADJACENCY", pList, gAdjacency, ARRAYSIZE( gAdjacency ));

		if( sv.changelevel && !com.strcmp( pList->mapName, sv.name ))
		{
			level_flags = (1<<i);
			Msg( "%s get level flags: %x\n", sv.name, level_flags );
			com.strcpy( pSaveData->szCurrentMap, pList->mapName );
			com.strcpy( pSaveData->szLandmarkName, pList->landmarkName );
			VectorCopy( pList->vecLandmarkOrigin, pSaveData->vecLandmarkOffset );
			pSaveData->fUseLandmark = true;
			pSaveData->time = shdr.time;
		}
	}

	// initialize ENTITYTABLE
	pSaveData->tableCount = shdr.numEntities;
	pSaveData->pTable = Mem_Alloc( svgame.temppool, pSaveData->tableCount * sizeof( ENTITYTABLE ));

	if( sv.loadgame ) // allocate edicts
		while( svgame.globals->numEntities < shdr.numEntities ) SV_AllocEdict();

	// set client fields on player ents
	for( i = 0; i < svgame.globals->maxClients; i++ )
	{
		// setup all clients
		ent = EDICT_NUM( i + 1 );
		SV_InitEdict( ent );
		ent->pvServerData->client = svs.clients + i;
		ent->pvServerData->client->edict = ent;
	}

	SV_ReadBuffer( l, LUMP_ENTTABLE );

	// read entity table
	for( i = 0; i < pSaveData->tableCount; i++ )
	{
		edict_t	*pent = EDICT_NUM( i );

		pTable = &pSaveData->pTable[i];		
		svgame.dllFuncs.pfnSaveReadFields( pSaveData, "ETABLE", pTable, gETable, ARRAYSIZE( gETable ));

		if( sv.loadgame )
		{
			if( pTable->flags & FENTTABLE_REMOVED ) SV_FreeEdict( pent );
			else pent = SV_AllocPrivateData( pent, pTable->classname );
		}
		else if( sv.changelevel )
		{
			bool	bAlloc = false;
				
			// check for client or global entity
			if( pTable->flags & (FENTTABLE_PLAYER|FENTTABLE_GLOBAL))
				bAlloc = true;

			if( pTable->flags & FENTTABLE_MOVEABLE && pTable->flags & level_flags )
				bAlloc = true;
			
			if( !pTable->id || !pTable->classname )
				bAlloc = false;

			if( bAlloc )
			{
				if( pent->free ) SV_InitEdict( pent );
				pent = SV_AllocPrivateData( pent, pTable->classname );
				svgame.globals->numEntities++;
				num_moveables++;
			}
		}

		if( sv.loadgame && ( pTable->id != pent->serialnumber ))
			MsgDev( D_ERROR, "ETABLE id( %i ) != edict->id( %i )\n", pTable->id, pent->serialnumber );

		pTable->pent = pent;
	}

	Msg( "total %i moveables, %i entities\n", num_moveables, svgame.globals->numEntities );

	if( sv.changelevel )
	{
		// spawn all the entities of the newmap
		SV_SpawnEntities( sv.name, CM_GetEntityScript( ));
	}

	SV_ReadBuffer( l, LUMP_BASEENTS );

	// and read entities ...
	for( i = 0; i < pSaveData->tableCount; i++ )
	{
		edict_t	*pent = EDICT_NUM( i );
		pTable = &pSaveData->pTable[i];

		if( sv.loadgame )
		{
			// ignore removed edicts
			if( !pent->free && pTable->classname )
			{
				svgame.dllFuncs.pfnRestore( pent, pSaveData, false );
			}
		}
		else if( sv.changelevel )
		{
			bool	bRestore = false;
			bool	bGlobal = (pTable->flags & FENTTABLE_GLOBAL) ? true : false;

			if( pTable->flags & (FENTTABLE_PLAYER|FENTTABLE_GLOBAL)) bRestore = true;
			if( pTable->flags & FENTTABLE_MOVEABLE && pTable->flags & level_flags )
				bRestore = true;
			if( !pTable->id || !pTable->classname ) bRestore = false;

			if( bRestore )
			{
				MsgDev( D_INFO, "Transfering %s ( *%i )\n", STRING( pTable->classname ), i );
				svgame.dllFuncs.pfnRestore( pent, pSaveData, bGlobal );
			}
		}
		pSaveData->currentIndex++;
	}

	// do cleanup operations
	Mem_Set( &svgame.SaveData, 0, sizeof( SAVERESTOREDATA ));
	svgame.globals->pSaveData = NULL;
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

	com.sprintf( path, "save/%s.sav", name );
	savfile = WAD_Open( path, "rb" );
	Msg( "SV_ReadSaveFile( %s )\n", path );

	if( !savfile )
	{
		MsgDev(D_ERROR, "SV_ReadSaveFile: can't open %s\n", path );
		return;
	}

	if( svgame.hInstance ) StringTable_Delete( svgame.hStringTable ); // remove old string table
	svgame.hStringTable = StringTable_Load( savfile, "Server" );

	if( sv.loadgame && !sv.changelevel )
	{
		SV_ReadCvars( savfile );
		SV_InitGame();	// start a new game fresh with new cvars

		sv.loadgame = true;	// restore state
	}

	SV_ReadGlobals( savfile );
	WAD_Close( savfile );
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

	com.sprintf( path, "save/%s.sav", name );
	savfile = WAD_Open( path, "rb" );

	if( !savfile )
	{
		MsgDev( D_ERROR, "SV_ReadLevelFile: can't open %s\n", path );
		return;
	}

	SV_ReadCfgString( savfile );
	SV_ReadEntities( savfile );
	SV_ReadAreaPortals( savfile );
	WAD_Close( savfile );
}

/*
=============
SV_MergeLevelFile
=============
*/
void SV_MergeLevelFile( const char *name )
{
	char		path[MAX_SYSPATH];
	wfile_t		*savfile;

	com.sprintf( path, "save/%s.sav", name );
	savfile = WAD_Open( path, "rb" );
	Msg( "SV_ReadSaveFile( %s )\n", path );

	if( !savfile )
	{
		MsgDev( D_ERROR, "SV_MergeLevel: can't open %s\n", path );
		return;
	}

	SV_ReadEntities( savfile );
	WAD_Close( savfile );
}

/*
=============
SV_ChangeLevel
=============
*/
void SV_ChangeLevel( bool bUseLandmark, const char *mapname, const char *start )
{
	string	level;
	string	oldlevel;
	string	_startspot;
	char	*startspot, *savename;

	if( sv.state != ss_active )
	{
		Msg( "SV_ChangeLevel: server not running\n");
		return;
	}

	sv.loadgame = false;
	sv.changelevel = true;

	if( bUseLandmark )
	{
		com.strncpy( _startspot, start, MAX_STRING );
		startspot = _startspot;
		savename = _startspot;
	}
	else
	{
		startspot = NULL;
		savename = level;
	}

	com.strncpy( level, mapname, MAX_STRING );
	com.strncpy( oldlevel, sv.name, MAX_STRING );

	// NOTE: we must save state even landmark is missed
	// so transfer client weapons and env_global states
	SV_WriteSaveFile( level, true, bUseLandmark );

	SV_BroadcastCommand( "reconnect\n" );

	SV_SendClientMessages();

	SV_DeactivateServer ();

	svs.spawncount = Com_RandomLong( 0, 65535 );

	SV_SpawnServer( level, startspot );

	SV_LevelInit( level, oldlevel, level );

	SV_ActivateServer ();
}

bool SV_GetComment( const char *savename, char *comment )
{
	wfile_t		*savfile;
	int		result;

	if( !comment ) return false;
	result = WAD_Check( savename ); 

	switch( result )
	{
	case 0:
		com.strncpy( comment, "", MAX_STRING );
		return false;
	case 1:	break;
	default:
		com.strncpy( comment, "<corrupted>", MAX_STRING );
		return false;
	}

	savfile = WAD_Open( savename, "rb" );
	SV_ReadComment( savfile );
	Mem_Copy( comment, svs.comment, MAX_STRING );
	WAD_Close( savfile );

	return true;
}

const char *SV_GetLatestSave( void )
{
	search_t	*f = FS_Search( "save/*.sav", true );
	int	i, found = 0;
	long	newest = 0, ft;
	string	savename;	

	if( !f ) return NULL;

	for( i = 0; i < f->numfilenames; i++ )
	{
		if( WAD_Check( f->filenames[i] ) != 1 )
			continue; // corrupted or somewhat 

		ft = FS_FileTime( va( "%s/%s", GI->gamedir, f->filenames[i] ));
		
		// found a match?
		if( ft > 0 )
		{
			// should we use the matche?
			if( !found || Host_CompareFileTime( newest, ft ) < 0 )
			{
				newest = ft;
				com.strncpy( savename, f->filenames[i], MAX_STRING );
				found = 1;
			}
		}
	}
	Mem_Free( f ); // release search

	if( found )
		return va( "%s", savename ); // move to static memory
	return NULL; 
}