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
byte		dcollision[MAX_MAP_COLLISION];
int		dcollisiondatasize;
dsurface_t	dsurfaces[MAX_MAP_SURFACES];
int		numsurfaces;
byte		dlightdata[MAX_MAP_LIGHTDATA];
int		lightdatasize;
byte		dlightgrid[MAX_MAP_LIGHTGRID];
int		numgridpoints;
int		visdatasize;
byte		dvisdata[MAX_MAP_VISIBILITY];
dvis_t		*dvis = (dvis_t *)dvisdata;

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
		dshaders[i].contents = LittleLong( dshaders[i].contents ); 
		dshaders[i].flags = LittleLong( dshaders[i].flags );
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
	Mem_Free( in ); // no need more
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
	
	if( length % block_size) Sys_Break( "LoadBSPFile: %i funny lump size", lumpname );
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
	numleaffaces = CopyLump( LUMP_LEAFFACES, dleaffaces, sizeof(dleaffaces[0]));
	numleafbrushes = CopyLump( LUMP_LEAFBRUSHES, dleafbrushes, sizeof(dleafbrushes[0]));
	nummodels = CopyLump( LUMP_MODELS, dmodels, sizeof(dmodel_t));
	numbrushes = CopyLump( LUMP_BRUSHES, dbrushes, sizeof(dbrush_t));
	numbrushsides = CopyLump( LUMP_BRUSHSIDES, dbrushsides, sizeof(dbrushside_t));
	numvertexes = CopyLump( LUMP_VERTICES, dvertexes, sizeof(dvertex_t));
	numindexes = CopyLump( LUMP_INDICES, dindexes, sizeof(dindexes[0]));
	dcollisiondatasize = CopyLump( LUMP_COLLISION, dcollision, 1);
	numsurfaces = CopyLump( LUMP_SURFACES, dsurfaces, sizeof(dsurfaces[0]));
	lightdatasize = CopyLump( LUMP_LIGHTMAPS, dlightdata, 1);
	numgridpoints = CopyLump( LUMP_LIGHTGRID, dlightgrid, sizeof(dlightgrid_t));
	visdatasize = CopyLump( LUMP_VISIBILITY, dvisdata, 1);

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
	MsgDev( D_NOTE, "writing %s.bsp\n", gs_filename );
	if( pe ) pe->FreeBSP();
	
	wadfile = FS_Open( va( "maps/%s.bsp", gs_filename ), "wb" );
	FS_Write( wadfile, header, sizeof( dheader_t ));	// overwritten later

	AddLump (LUMP_ENTITIES, dentdata, entdatasize);
	AddLump (LUMP_SHADERS, dshaders, numshaders*sizeof(dshaders[0]));
	AddLump (LUMP_PLANES, dplanes, numplanes*sizeof(dplanes[0]));
	AddLump (LUMP_NODES, dnodes, numnodes*sizeof(dnodes[0]));
	AddLump (LUMP_LEAFS, dleafs, numleafs*sizeof(dleafs[0]));
	AddLump (LUMP_LEAFFACES, dleaffaces, numleaffaces*sizeof(dleaffaces[0]));
	AddLump (LUMP_LEAFBRUSHES, dleafbrushes, numleafbrushes*sizeof(dleafbrushes[0]));
	AddLump (LUMP_MODELS, dmodels, nummodels*sizeof(dmodels[0]));
	AddLump (LUMP_BRUSHES, dbrushes, numbrushes*sizeof(dbrushes[0]));
	AddLump (LUMP_BRUSHSIDES, dbrushsides, numbrushsides*sizeof(dbrushsides[0]));
	AddLump (LUMP_VERTICES, dvertexes, numvertexes*sizeof(dvertexes[0]));
	AddLump (LUMP_INDICES, dindexes, numindexes*sizeof(dindexes[0]));
	AddLump (LUMP_COLLISION, dcollision, dcollisiondatasize );
	AddLump (LUMP_SURFACES, dsurfaces, numsurfaces*sizeof(dsurfaces[0]));	
	AddLump (LUMP_LIGHTMAPS, dlightdata, lightdatasize);
	AddLump (LUMP_LIGHTGRID, dlightgrid, numgridpoints*sizeof(dlightgrid_t));
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
	Com_ReadToken( mapfile, 0, token );
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
		if( !Com_ReadToken( mapfile, SC_ALLOW_NEWLINES, &token ))
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
	epair_t	*ep;
	char	*buf, *end;
	char	line[2048];
	char	key[MAX_KEY], value[MAX_VALUE];
	int	i;

	buf = dentdata;
	end = buf;
	*end = 0;
	
	for( i = 0; i < num_entities; i++ )
	{
		ep = entities[i].epairs;
		if( !ep ) continue;	// ent got removed
		
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
			Mem_Free( ep->value );
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

char *ValueForKey( bsp_entity_t *ent, const char *key )
{
	epair_t	*ep;
	
	for( ep = ent->epairs; ep; ep = ep->next )
	{
		if(!com.strcmp( ep->key, key ))
			return ep->value;
	}
	return "";
}

vec_t FloatForKey( bsp_entity_t *ent, const char *key )
{
	char	*k;
	
	k = ValueForKey( ent, key );
	return com.atof( k );
}

void GetVectorForKey( bsp_entity_t *ent, const char *key, vec3_t vec )
{
	char	*k;
	double	v1, v2, v3;

	k = ValueForKey( ent, key );

	// scanf into doubles, then assign, so it is vec_t size independent
	v1 = v2 = v3 = 0;
	sscanf( k, "%lf %lf %lf", &v1, &v2, &v3 );
	VectorSet( vec, v1, v2, v3 );
}

void Com_CheckToken( const char *match )
{
	token_t	token;
	
	Com_ReadToken( mapfile, SC_ALLOW_NEWLINES, &token );

	if( com.stricmp( token.string, match ))
	{
		Sys_Break( "Com_CheckToken: \"%s\" not found\n", match );
	}
}

void Com_Parse1DMatrix( int x, vec_t *m )
{
	int	i;

	Com_CheckToken( "(" );

	for( i = 0; i < x; i++ )
		Com_ReadFloat( mapfile, false, &m[i] );
	Com_CheckToken( ")" );
}

void Com_Parse2DMatrix( int y, int x, vec_t *m )
{
	int	i;

	Com_CheckToken( "(" );

	for( i = 0; i < y; i++ )
	{
		Com_Parse1DMatrix( x, m+i*x );
	}
	Com_CheckToken( ")" );
}

void Com_Parse3DMatrix( int z, int y, int x, vec_t *m )
{
	int	i;

	Com_CheckToken( "(" );

	for( i = 0; i < z; i++ )
	{
		Com_Parse2DMatrix( y, x, m+i*x*y );
	}
	Com_CheckToken( ")" );
}