//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	bspfile.c - read\save bsp file
//=======================================================================

#include "bsplib.h"
#include "byteorder.h"
#include "const.h"

wfile_t		*handle;
file_t		*wadfile;
int		s_table;
dheader_t		*header;
dheader_t		outheader;

int		num_entities;
bsp_entity_t	entities[MAX_MAP_ENTITIES];
int		num_lightbytes;
byte		*lightbytes;

char		dentdata[MAX_MAP_ENTSTRING];
int		entdatasize;
dshader_t		dshaders[MAX_MAP_SHADERS];
int		numshaders;
dplane_t		dplanes[MAX_MAP_PLANES];
int		numplanes;
dnode_t		dnodes[MAX_MAP_NODES];
int		numnodes;
dleaf_t		dleafs[MAX_MAP_LEAFS];
int		numleafs;
dword		dleaffaces[MAX_MAP_LEAFFACES];
int		numleaffaces;
dword		dleafbrushes[MAX_MAP_LEAFBRUSHES];
int		numleafbrushes;
dmodel_t		dmodels[MAX_MAP_MODELS];
int		nummodels;
dbrush_t		dbrushes[MAX_MAP_BRUSHES];
int		numbrushes;
dbrushside_t	dbrushsides[MAX_MAP_BRUSHSIDES];
int		numbrushsides;
dvertex_t		dvertexes[MAX_MAP_VERTEXES];
int		numvertexes;
int		dindexes[MAX_MAP_INDEXES];
int		numindexes;
dfog_t		dfogs[MAX_MAP_FOGS];
int		numfogs;
dsurface_t	dsurfaces[MAX_MAP_SURFACES];
int		numsurfaces;
byte		dcollision[MAX_MAP_COLLISION];
int		dcollisiondatasize;
dlightgrid_t	dlightgrid[MAX_MAP_LIGHTGRID];
int		numgridpoints;
byte		dvisdata[MAX_MAP_VISIBILITY];
dvis_t		*dvis = (dvis_t *)dvisdata;
int		visdatasize;

//=============================================================================
/*
===============
CompressVis
===============
*/
int CompressVis( byte *vis, byte *dest )
{
	int	j;
	int	rep;
	int	visrow;
	byte	*dest_p;
	
	dest_p = dest;
	visrow = (dvis->numclusters + 7)>>3;
	
	for( j = 0; j < visrow; j++ )
	{
		*dest_p++ = vis[j];
		if( vis[j] ) continue;

		rep = 1;
		for( j++; j < visrow; j++ )
			if( vis[j] || rep == 255 )
				break;
			else rep++;
		*dest_p++ = rep;
		j--;
	}
	return dest_p - dest;
}

/*
===================
DecompressVis
===================
*/
void DecompressVis( byte *in, byte *decompressed )
{
	int		c;
	byte	*out;
	int		row;

	row = (dvis->numclusters+7)>>3;	
	out = decompressed;

	do
	{
		if( *in )
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		if( !c ) Sys_Error( "DecompressVis: 0 repeat\n" );
		in += 2;
		while( c )
		{
			*out++ = 0;
			c--;
		}
	} while( out - decompressed < row );
}


/*
=============
SwapBSPFile

Byte swaps all data in a bsp file.
=============
*/
void SwapBSPFile( bool todisk )
{
	int	i, j;

	// shaders
	for( i = 0; i < numshaders; i++ )
	{
		dshaders[i].surfaceFlags = LittleLong( dshaders[i].surfaceFlags );
		dshaders[i].contents = LittleLong( dshaders[i].contents ); 

	}

	// fogs
	for( i = 0; i < numfogs; i++ )
	{
		dfogs[i].brushnum = LittleLong( dfogs[i].brushnum );
		dfogs[i].visibleSide = LittleLong( dfogs[i].visibleSide );
	}

	// planes
	SwapBlock( (int *)dplanes, numplanes * sizeof(dplanes[0]));

	// nodes
	SwapBlock( (int *)dnodes, numnodes * sizeof(dnodes[0]));	

	// leafs
	SwapBlock( (int *)dleafs, numleafs * sizeof(dleafs[0]));

	// leaffaces
	SwapBlock( (int *)dleaffaces, numleaffaces * sizeof(dleaffaces[0]));

	// leafbrushes
	SwapBlock( (int *)dleafbrushes, numleafbrushes * sizeof(dleafbrushes[0]));

	// models	
	SwapBlock( (int *)dmodels, nummodels * sizeof(dmodels[0]));

	// brushes
	SwapBlock( (int *)dbrushes, numbrushes * sizeof(dbrushes[0]));

	// brushsides
	SwapBlock( (int *)dbrushsides, numbrushsides * sizeof(dbrushsides[0]));

	// vertices
	SwapBlock( (int *)dvertexes, numvertexes * sizeof(dvertexes[0]));

	// indices
	SwapBlock( (int *)dindexes, numindexes * sizeof(dindexes[0]));

	// surfaces
	SwapBlock( (int *)dsurfaces, numsurfaces * sizeof(dsurfaces[0]));

	// visibility
	if( todisk ) j = dvis->numclusters;
	else j = LittleLong( dvis->numclusters );
	dvis->numclusters = LittleLong( dvis->numclusters );
	for( i = 0; i < j; i++ )
	{
		dvis->bitofs[i][0] = LittleLong( dvis->bitofs[i][0] );
		dvis->bitofs[i][1] = LittleLong( dvis->bitofs[i][1] );
	}
}

#ifdef BSP_WADFILE
size_t CopyLump( const char *lumpname, void *dest, size_t block_size )
{
	size_t	length;
	byte	*in;

	if( !handle ) return 0;

	in = WAD_Read( handle, lumpname, &length, TYPE_BINDATA );
	if( length % block_size ) Sys_Break( "LoadBSPFile: %s funny lump size\n", lumpname );
	Mem_Copy( dest, in, length );
	BSP_Free( in ); // no need more
	return length / block_size;
}

void AddLump( const char *lumpname, const void *data, size_t length )
{
	int	compress = CMP_NONE;

	if( !handle ) return;
	if( length > 0xffff ) compress = CMP_ZLIB;
	WAD_Write( handle, lumpname, data, length, TYPE_BINDATA, compress );
}

#else

size_t CopyLump( const int lumpname, void *dest, size_t block_size )
{
	size_t	length, ofs;

	length = header->lumps[lumpname].filelen;
	ofs = header->lumps[lumpname].fileofs;
	
	if( length % block_size) Sys_Break( "LoadBSPFile: %i funny lump size\n", lumpname );
	Mem_Copy(dest, (byte *)header + ofs, length);

	return length / block_size;
}

void AddLump( const int lumpname, const void *data, size_t length )
{
	lump_t *lump;

	lump = &header->lumps[lumpname];
	lump->fileofs = LittleLong( FS_Tell( wadfile ));
	lump->filelen = LittleLong( length );

	FS_Write( wadfile, data, (length + 3) & ~3 );
}
#endif

static void CopyLightGridLumps( void )
{
	int		i, index;
	word		*inArray;
	dlightgrid_t	*in, *out;
	
	if( header->lumps[LUMP_LIGHTARRAY].filelen % sizeof(*inArray))
		Sys_Break( "LoadBSPFile: %i funny lump size\n", LUMP_LIGHTARRAY );
	numgridpoints = header->lumps[LUMP_LIGHTARRAY].filelen / sizeof(*inArray);
	inArray = (void*)( (byte*)header + header->lumps[LUMP_LIGHTARRAY].fileofs);
	in = (void*)( (byte*)header + header->lumps[LUMP_LIGHTGRID].fileofs);
	out = &dlightgrid[0];

	for( i = 0; i < numgridpoints; i++ )
	{
		index = LittleShort( *inArray );
		Mem_Copy( out, &in[index], sizeof( *in ));
		inArray++;
		out++;
	}
}

static void AddLightGridLumps( void )
{
	int		i, j, k, c, d;
	int		numGridPoints, maxGridPoints;
	dlightgrid_t	*gridPoints, *in, *out;
	int		numGridArray;
	word		*gridArray;
	bool		bad;
	
	maxGridPoints = (numgridpoints < MAX_MAP_GRID) ? numgridpoints : MAX_MAP_GRID;
	gridPoints = BSP_Malloc( maxGridPoints * sizeof( *gridPoints ));
	gridArray = BSP_Malloc( numgridpoints * sizeof( *gridArray ));
	
	numGridPoints = 0;
	numGridArray = numgridpoints;
	
	// for each bsp grid point, find an approximate twin
	MsgDev( D_INFO, "Storing lightgrid: %d points\n", numgridpoints );
	for( i = 0; i < numGridArray; i++ )
	{
		in = &dlightgrid[i];
		
		for( j = 0; j < numgridpoints; j++ )
		{
			out = &gridPoints[j];
			
			if(*((uint*) in->styles) != *((uint*) out->styles))
				continue;
			
			d = abs( in->latLong[0] - out->latLong[0] );
			if( d < (255 - LG_EPSILON) && d > LG_EPSILON )
				continue;
			d = abs( in->latLong[1] - out->latLong[1] );
			if( d < 255 - LG_EPSILON && d > LG_EPSILON )
				continue;
			
			// compare light
			bad = false;
			for( k = 0; (k < LM_STYLES && bad == false); k++ )
			{
				for( c = 0; c < 3; c++ )
				{
					if(abs((int)in->ambient[k][c] - (int)out->ambient[k][c]) > LG_EPSILON
					|| abs((int)in->direct[k][c] - (int)out->direct[k][c]) > LG_EPSILON )
					{
						bad = true;
						break;
					}
				}
			}
			
			if( bad ) continue;
			break; // found
		}
		
		// set sample index
		gridArray[i] = (word)j;
		
		// if no sample found, add a new one
		if( j >= numGridPoints && numGridPoints < maxGridPoints )
		{
			out = &gridPoints[numGridPoints++];
			memcpy( out, in, sizeof( *in ) );
		}
	}
	
	// swap array
	for( i = 0; i < numGridArray; i++ ) gridArray[i] = LittleShort( gridArray[i] );
	AddLump( LUMP_LIGHTGRID, gridPoints, (numGridPoints * sizeof( *gridPoints )));
	AddLump( LUMP_LIGHTARRAY, gridArray, (numGridArray * sizeof( *gridArray )));
	if( gridPoints ) BSP_Free( gridPoints );
	if( gridArray ) BSP_Free( gridArray );
}

/*
=============
LoadBSPFile
=============
*/
bool LoadBSPFile( void )
{
	byte	*buffer;
	
	buffer = (byte *)FS_LoadFile( va("maps/%s.bsp", gs_filename ), NULL );
	if( !buffer ) return false;

	header = (dheader_t *)buffer; // load the file header
	if( pe ) pe->LoadBSP( buffer );

	MsgDev( D_NOTE, "reading %s.bsp\n", gs_filename );
	
	// swap the header
	SwapBlock( (int *)header, sizeof( header ));

	if( header->ident != IDBSPMODHEADER )
		Sys_Break( "%s.bsp is not a IBSP file\n", gs_filename );
	if( header->version != BSPMOD_VERSION )
		Sys_Break( "%s.bsp is version %i, not %i\n", gs_filename, header->version, BSPMOD_VERSION );

	entdatasize = CopyLump( LUMP_ENTITIES, dentdata, 1);
	numshaders = CopyLump( LUMP_SHADERS, dshaders, sizeof(dshader_t));
	numplanes = CopyLump( LUMP_PLANES, dplanes, sizeof(dplane_t));
	numnodes = CopyLump( LUMP_NODES, dnodes, sizeof(dnode_t));
	numleafs = CopyLump( LUMP_LEAFS, dleafs, sizeof(dleaf_t));
	numleaffaces = CopyLump( LUMP_LEAFSURFACES, dleaffaces, sizeof(dleaffaces[0]));
	numleafbrushes = CopyLump( LUMP_LEAFBRUSHES, dleafbrushes, sizeof(dleafbrushes[0]));
	nummodels = CopyLump( LUMP_MODELS, dmodels, sizeof(dmodel_t));
	numbrushes = CopyLump( LUMP_BRUSHES, dbrushes, sizeof(dbrush_t));
	numbrushsides = CopyLump( LUMP_BRUSHSIDES, dbrushsides, sizeof(dbrushside_t));
	numvertexes = CopyLump( LUMP_VERTICES, dvertexes, sizeof(dvertex_t));
	numindexes = CopyLump( LUMP_INDICES, dindexes, sizeof(dindexes[0]));
	numfogs = CopyLump( LUMP_FOGS, dfogs, sizeof( dfog_t ));
	numsurfaces = CopyLump( LUMP_SURFACES, dsurfaces, sizeof(dsurfaces[0]));
	dcollisiondatasize = CopyLump( LUMP_COLLISION, dcollision, 1);
	visdatasize = CopyLump( LUMP_VISIBILITY, dvisdata, 1);
	CopyLightGridLumps();

	// swap everything
	SwapBSPFile( false );

	return true;
}

//============================================================================

/*
=============
WriteBSPFile

Swaps the bsp file in place, so it should not be referenced again
=============
*/
void WriteBSPFile( void )
{		
	header = &outheader;
	memset( header, 0, sizeof( dheader_t ));
	
	SwapBSPFile( true );

	header->ident = LittleLong( IDBSPMODHEADER );
	header->version = LittleLong( BSPMOD_VERSION );
	
	// build path
	MsgDev( D_NOTE, "\n\nwriting %s.bsp\n", gs_filename );
	if( pe ) pe->FreeBSP();
	
	wadfile = FS_Open( va( "maps/%s.bsp", gs_filename ), "wb" );
	FS_Write( wadfile, header, sizeof( dheader_t ));	// overwritten later

	AddLump (LUMP_ENTITIES, dentdata, entdatasize);
	AddLump (LUMP_SHADERS, dshaders, numshaders*sizeof(dshaders[0]));
	AddLump (LUMP_PLANES, dplanes, numplanes*sizeof(dplanes[0]));
	AddLump (LUMP_NODES, dnodes, numnodes*sizeof(dnodes[0]));
	AddLump (LUMP_LEAFS, dleafs, numleafs*sizeof(dleafs[0]));
	AddLump (LUMP_LEAFSURFACES, dleaffaces, numleaffaces*sizeof(dleaffaces[0]));
	AddLump (LUMP_LEAFBRUSHES, dleafbrushes, numleafbrushes*sizeof(dleafbrushes[0]));
	AddLump (LUMP_MODELS, dmodels, nummodels*sizeof(dmodels[0]));
	AddLump (LUMP_BRUSHES, dbrushes, numbrushes*sizeof(dbrushes[0]));
	AddLump (LUMP_BRUSHSIDES, dbrushsides, numbrushsides*sizeof(dbrushsides[0]));
	AddLump (LUMP_VERTICES, dvertexes, numvertexes*sizeof(dvertexes[0]));
	AddLump (LUMP_INDICES, dindexes, numindexes*sizeof(dindexes[0]));
	AddLump (LUMP_FOGS, dfogs, numfogs*sizeof(dfogs[0]));
	AddLump (LUMP_SURFACES, dsurfaces, numsurfaces*sizeof(dsurfaces[0]));	
	AddLump (LUMP_COLLISION, dcollision, dcollisiondatasize );
	AddLightGridLumps();
	AddLump (LUMP_VISIBILITY, dvisdata, visdatasize);

	FS_Seek( wadfile, 0, SEEK_SET );
	FS_Write( wadfile, header, sizeof(dheader_t));
	FS_Close( wadfile );
}

//============================================
//	misc parse functions
//============================================

void StripTrailing( char *e )
{
	char	*s;

	s = e + com.strlen( e ) - 1;
	while( s >= e && *s <= 32 )
	{
		*s = 0;
		s--;
	}
}

/*
=================
ParseEpair
=================
*/
epair_t *ParseEpair( token_t *token )
{
	epair_t	*e;

	e = BSP_Malloc( sizeof( epair_t ));
	
	if( com.strlen( token->string ) >= MAX_KEY - 1 )
		Sys_Break( "ParseEpair: token too long\n" );
	e->key = copystring( token->string );
	Com_ReadToken( mapfile, SC_PARSE_GENERIC, token );
	if( com.strlen( token->string ) >= MAX_VALUE - 1 )
		Sys_Break( "ParseEpair: token too long\n" );
	e->value = copystring( token->string );

	// strip trailing spaces
	StripTrailing( e->key );
	StripTrailing( e->value );

	return e;
}

/*
================
ParseEntity
================
*/
bool ParseEntity( void )
{
	epair_t		*e;
	token_t		token;
	bsp_entity_t	*mapent;

	if( !Com_ReadToken( mapfile, SC_ALLOW_NEWLINES, &token ))
		return false;

	if( com.stricmp( token.string, "{" )) Sys_Break( "ParseEntity: '{' not found\n" );
	if( num_entities == MAX_MAP_ENTITIES ) Sys_Break( "MAX_MAP_ENTITIES limit excceded\n" );

	mapent = &entities[num_entities];
	num_entities++;

	while( 1 )
	{
		if( !Com_ReadToken( mapfile, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
			Sys_Break( "ParseEntity: EOF without closing brace\n" );
		if( !com.stricmp( token.string, "}" )) break;
		e = ParseEpair( &token );
		e->next = mapent->epairs;
		mapent->epairs = e;
	}
	return true;
}

/*
================
ParseEntities

Parses the dentdata string into entities
================
*/
void ParseEntities( void )
{
	num_entities = 0;
	mapfile = Com_OpenScript( "entities", dentdata, entdatasize );
	if( mapfile ) while( ParseEntity( ));
	Com_CloseScript( mapfile );
}


/*
================
UnparseEntities

Generates the dentdata string from all the entities
================
*/
void UnparseEntities( void )
{
	epair_t		*ep;
	char		*buf, *end;
	char		line[2048];
	char		key[MAX_KEY], value[MAX_VALUE];
	const char	*value2;
	int		i;

	buf = dentdata;
	end = buf;
	*end = 0;
	
	for( i = 0; i < num_entities; i++ )
	{
		ep = entities[i].epairs;
		if( !ep ) continue;	// ent got removed

		// certain entities get stripped from bsp file */
		value2 = ValueForKey( &entities[i], "classname" );
		if(!com.stricmp( value2, "_decal" ) || !com.stricmp( value2, "_skybox" ))
			continue;
		
		com.strcat( end, "{\n" );
		end += 2;
				
		for( ep = entities[i].epairs; ep; ep = ep->next )
		{
			com.strncpy( key, ep->key, MAX_KEY );
			StripTrailing( key );
			com.strncpy( value, ep->value, MAX_VALUE );
			StripTrailing( value );
				
			com.snprintf( line, 2048, "\"%s\" \"%s\"\n", key, value );
			com.strcat( end, line );
			end += com.strlen( line );
		}
		com.strcat( end, "}\n" );
		end += 2;

		if( end > buf + MAX_MAP_ENTSTRING )
			Sys_Break( "Entity text too long\n" );
	}
	entdatasize = end - buf + 1;
}

void SetKeyValue( bsp_entity_t *ent, const char *key, const char *value )
{
	epair_t	*ep;
	
	for( ep = ent->epairs; ep; ep = ep->next )
	{
		if(!com.strcmp( ep->key, key ))
		{
			BSP_Free( ep->value );
			ep->value = copystring( value );
			return;
		}
	}
	ep = BSP_Malloc( sizeof( *ep ));
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = copystring( key );
	ep->value = copystring( value );
}

char *ValueForKey( const bsp_entity_t *ent, const char *key )
{
	epair_t	*ep;

	if( !ent ) return "";
	
	for( ep = ent->epairs; ep; ep = ep->next )
	{
		if(!com.strcmp( ep->key, key ))
			return ep->value;
	}
	return "";
}

long IntForKey( const bsp_entity_t *ent, const char *key )
{
	char	*k;
	
	k = ValueForKey( ent, key );
	return com.atoi( k );
}

vec_t FloatForKey( const bsp_entity_t *ent, const char *key )
{
	char	*k;
	
	k = ValueForKey( ent, key );
	return com.atof( k );
}

void GetVectorForKey( const bsp_entity_t *ent, const char *key, vec3_t vec )
{
	char	*k;
	double	v1, v2, v3;

	k = ValueForKey( ent, key );

	// scanf into doubles, then assign, so it is vec_t size independent
	v1 = v2 = v3 = 0;
	sscanf( k, "%lf %lf %lf", &v1, &v2, &v3 );
	VectorSet( vec, v1, v2, v3 );
}

/*
================
FindTargetEntity

finds an entity target
================
*/
bsp_entity_t *FindTargetEntity( const char *target )
{
	int		i;
	const char	*n;

	
	// walk entity list
	for( i = 0; i < num_entities; i++ )
	{
		n = ValueForKey( &entities[i], "targetname" );
		if( !com.strcmp( n, target ))
			return &entities[i];
	}
	return NULL;
}

/*
================
GetEntityShadowFlags

gets an entity's shadow flags
note: does not set them to defaults if the keys are not found!
================
*/
void GetEntityShadowFlags( const bsp_entity_t *ent, const bsp_entity_t *ent2, int *castShadows, int *recvShadows )
{
	const char	*value;
	
	// get cast shadows
	// FIXME: make ZHLT shadowflags support here
	if( castShadows != NULL )
	{
		value = ValueForKey( ent, "_castShadows" );
		if( value[0] == '\0' ) value = ValueForKey( ent, "_cs" );
		if( value[0] == '\0' ) value = ValueForKey( ent2, "_castShadows" );
		if( value[0] == '\0' ) value = ValueForKey( ent2, "_cs" );
		if( value[0] != '\0' ) *castShadows = com.atoi( value );
	}
	
	// receive
	if( recvShadows != NULL )
	{
		value = ValueForKey( ent, "_receiveShadows" );
		if( value[0] == '\0' ) value = ValueForKey( ent, "_rs" );
		if( value[0] == '\0' ) value = ValueForKey( ent2, "_receiveShadows" );
		if( value[0] == '\0' ) value = ValueForKey( ent2, "_rs" );
		if( value[0] != '\0' ) *recvShadows = com.atoi( value );
	}
}

/*
================
Com_GetTokenAppend

gets a token and appends its text to the specified buffer
================
*/
static int oldScriptLine = 0;
static int tabDepth = 0;

bool Com_GetTokenAppend( script_t *script, char *buffer, bool crossline, token_t *token )
{
	bool	result;
	int	i, flags = SC_PARSE_GENERIC;

	if( crossline ) flags |= SC_ALLOW_NEWLINES;	
	result = Com_ReadToken( script, flags, token );
	if( result == false || buffer == NULL || token->string[0] == '\0' )
		return result;
	
	// pre-tabstops
	if( !com.stricmp( token->string, "}" )) tabDepth--;
	
	// append?
	if( oldScriptLine != token->line )
	{
		com.strcat( buffer, "\n" );
		for( i = 0; i < tabDepth; i++ )
			com.strcat( buffer, "\t" );
	}
	else com.strcat( buffer, " " );

	oldScriptLine = token->line;
	com.strcat( buffer, token->string );
	
	// post-tabstops
	if( !com.stricmp( token->string, "{" )) tabDepth++;
	
	return result;
}

void Com_Parse1DMatrixAppend( script_t *script, char *buffer, int x, vec_t *m )
{
	token_t	token;
	int	i;
	
	if( !Com_GetTokenAppend( script, buffer, true, &token ) || com.stricmp( token.string, "(" ))
		Sys_Break( "Com_Parse1DMatrixAppend: line %d: ( not found!\n", token.line );
	for( i = 0; i < x; i++ )
	{
		if(!Com_GetTokenAppend( script, buffer, false, &token ))
			Sys_Break( "Com_Parse1DMatrixAppend: line %d: Number not found!\n", token.line );
		m[i] = com.atof( token.string );
	}
	if( !Com_GetTokenAppend( script, buffer, true, &token ) || com.stricmp( token.string, ")" ))
		Sys_Break( "Com_Parse1DMatrixAppend: line %d: ) not found!", token.line );
}

void Com_CheckToken( script_t *script, const char *match )
{
	token_t	token;
	
	Com_ReadToken( script, SC_ALLOW_NEWLINES, &token );

	if( com.stricmp( token.string, match ))
	{
		Sys_Break( "Com_CheckToken: \"%s\" not found\n", match );
	}
}

void Com_Parse1DMatrix( script_t *script, int x, vec_t *m )
{
	int	i;

	Com_CheckToken( script, "(" );

	for( i = 0; i < x; i++ )
		Com_ReadFloat( script, false, &m[i] );
	Com_CheckToken( script, ")" );
}

void Com_Parse2DMatrix( script_t *script, int y, int x, vec_t *m )
{
	int	i;

	Com_CheckToken( script, "(" );

	for( i = 0; i < y; i++ )
	{
		Com_Parse1DMatrix( script, x, m+i*x );
	}
	Com_CheckToken( script, ")" );
}

void Com_Parse3DMatrix( script_t *script, int z, int y, int x, vec_t *m )
{
	int	i;

	Com_CheckToken( script, "(" );

	for( i = 0; i < z; i++ )
	{
		Com_Parse2DMatrix( script, y, x, m+i*x*y );
	}
	Com_CheckToken( script, ")" );
}