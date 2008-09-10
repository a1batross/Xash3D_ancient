
#include "bsplib.h"
#include "byteorder.h"
#include "const.h"

//=============================================================================
wfile_t		*handle;
int		s_table;
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
int		dcollisiondatasize = 256; // variable sized
dsurface_t	dsurfaces[MAX_MAP_SURFACES];
int		numsurfaces;
byte		dlightdata[MAX_MAP_LIGHTDATA];
int		lightdatasize;
dlightgrid_t	dlightgrid[MAX_MAP_LIGHTGRID];
int		numlightgrids;
byte		dpvsdata[MAX_MAP_VISIBILITY];
int		pvsdatasize;
byte		dphsdata[MAX_MAP_VISIBILITY];
int		phsdatasize;

//=============================================================================

/*
=============
SwapBSPFile

Byte swaps all data in a bsp file.
=============
*/
void SwapBSPFile( void )
{
	int	i;

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
	// FIXME: these code swapped colors
	SwapBlock( (int *)dvertexes, numvertexes * sizeof(dvertexes[0]));

	// indices
	SwapBlock( (int *)dindexes, numindexes * sizeof(dindexes[0]));

	// surfaces
	SwapBlock( (int *)dsurfaces, numsurfaces * sizeof(dsurfaces[0]));

	// visibility
	((int *)&dpvsdata)[0] = LittleLong( ((int *)&dpvsdata)[0] );
	((int *)&dpvsdata)[1] = LittleLong( ((int *)&dpvsdata)[1] );

	// hearability
	((int *)&dphsdata)[0] = LittleLong( ((int *)&dphsdata)[0] );
	((int *)&dphsdata)[1] = LittleLong( ((int *)&dphsdata)[1] );
}

size_t BSP_LoadLump( const char *lumpname, void *dest, size_t block_size )
{
	size_t	length;
	byte	*in;

	if( !handle ) return 0;

	in = WAD_Read( handle, lumpname, &length, TYPE_BINDATA );
	if( length % block_size ) Sys_Break( "BSP_CopyLump: %s funny lump size\n", lumpname );
	Mem_Copy( dest, in, length );
	Mem_Free( in ); // no need more
	return length / block_size;
}

void BSP_SaveLump( const char *lumpname, const void *data, size_t length, bool compress )
{
	if( !handle ) return;
	WAD_Write( handle, lumpname, data, length, TYPE_BINDATA, ( compress ? CMP_ZLIB : CMP_NONE ));
}

dheader_t	*header;

int CopyLump( int lump, void *dest, int size )
{
	int		length, ofs;

	length = header->lumps[lump].filelen;
	ofs = header->lumps[lump].fileofs;
	
	if (length % size) Sys_Error("LoadBSPFile: odd lump size");
	Mem_Copy(dest, (byte *)header + ofs, length);

	return length / size;
}

/*
=============
LoadBSPFile
=============
*/
bool LoadBSPFile( void )
{
	int	i, size;
	char	path[MAX_SYSPATH];
	byte	*buffer;
	
	sprintf(path, "maps/%s.bsp", gs_filename );
	buffer = (byte *)FS_LoadFile(path, &size);
	if(!size) return false;

	header = (dheader_t *)buffer; // load the file header
	if(pe) pe->LoadBSP( buffer );

	MsgDev(D_NOTE, "reading %s\n", path);
	
	// swap the header
	for(i = 0; i < sizeof(dheader_t)/4; i++)
		((int *)header)[i] = LittleLong (((int *)header)[i]);

	if(header->ident != IDBSPMODHEADER) Sys_Error("%s is not a IBSP file", gs_filename);
	if(header->version != BSPMOD_VERSION) Sys_Error("%s is version %i, not %i", gs_filename, header->version, BSPMOD_VERSION);

	entdatasize = CopyLump (LUMP_ENTITIES, dentdata, 1);
	numshaders = CopyLump(LUMP_SHADERS, dshaders, sizeof(dshader_t));
	numplanes = CopyLump (LUMP_PLANES, dplanes, sizeof(dplane_t));
	numnodes = CopyLump (LUMP_NODES, dnodes, sizeof(dnode_t));
	numleafs = CopyLump (LUMP_LEAFS, dleafs, sizeof(dleaf_t));
	numleaffaces = CopyLump (LUMP_LEAFFACES, dleaffaces, sizeof(dleaffaces[0]));
	numleafbrushes = CopyLump (LUMP_LEAFBRUSHES, dleafbrushes, sizeof(dleafbrushes[0]));
	nummodels = CopyLump (LUMP_MODELS, dmodels, sizeof(dmodel_t));
	numbrushes = CopyLump (LUMP_BRUSHES, dbrushes, sizeof(dbrush_t));
	numbrushsides = CopyLump (LUMP_BRUSHSIDES, dbrushsides, sizeof(dbrushside_t));
	numvertexes = CopyLump (LUMP_VERTICES, dvertexes, sizeof(dvertex_t));
	numindexes = CopyLump (LUMP_INDICES, dindexes, sizeof(dindexes[0]));
	dcollisiondatasize = CopyLump(LUMP_COLLISION, dcollision, 1);
	numsurfaces = CopyLump (LUMP_SURFACES, dsurfaces, sizeof(dsurfaces[0]));
	lightdatasize = CopyLump (LUMP_LIGHTMAPS, dlightdata, 1);
	pvsdatasize = CopyLump (LUMP_VISIBILITY, dpvsdata, 1);
	phsdatasize = CopyLump (LUMP_HEARABILITY, dphsdata, 1);

	// swap everything
	SwapBSPFile();

	return true;
}

//============================================================================

file_t	*wadfile;	// because bsp it's really wadfile with fixed lump counter
dheader_t	outheader;

void AddLump( int lumpnum, const void *data, size_t length )
{
	lump_t *lump;

	lump = &header->lumps[lumpnum];
	lump->fileofs = LittleLong( FS_Tell( wadfile ));
	lump->filelen = LittleLong( length );

	FS_Write( wadfile, data, (length + 3) & ~3 );
}

/*
=============
WriteBSPFile

Swaps the bsp file in place, so it should not be referenced again
=============
*/
void WriteBSPFile( void )
{		
	char path[MAX_SYSPATH];
	
	header = &outheader;
	memset( header, 0, sizeof( dheader_t ));
	
	SwapBSPFile();

	header->ident = LittleLong( IDBSPMODHEADER );
	header->version = LittleLong( BSPMOD_VERSION );
	
	// build path
	sprintf( path, "maps/%s.bsp", gs_filename );
	MsgDev( D_NOTE, "writing %s\n", path );
	if( pe ) pe->FreeBSP();
	
	wadfile = FS_Open( path, "wb" );
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
	AddLump (LUMP_VISIBILITY, dpvsdata, pvsdatasize);
	AddLump (LUMP_HEARABILITY, dphsdata, phsdatasize);

	FS_Seek( wadfile, 0, SEEK_SET );
	FS_Write( wadfile, header, sizeof(dheader_t));
	FS_Close( wadfile );
}

//============================================

int num_entities;
bsp_entity_t entities[MAX_MAP_ENTITIES];

void StripTrailing (char *e)
{
	char	*s;

	s = e + strlen(e)-1;
	while (s >= e && *s <= 32)
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
epair_t *ParseEpair (void)
{
	epair_t	*e;

	e = Malloc (sizeof(epair_t));
	
	if (strlen(com_token) >= MAX_KEY - 1)Sys_Error ("ParseEpar: token too long");
	e->key = copystring(com_token);
	Com_GetToken (false);
	if (strlen(com_token) >= MAX_VALUE - 1)Sys_Error ("ParseEpar: token too long");
	e->value = copystring(com_token);

	// strip trailing spaces
	StripTrailing (e->key);
	StripTrailing (e->value);

	return e;
}


/*
================
ParseEntity
================
*/
bool ParseEntity (void)
{
	epair_t	*e;
	bsp_entity_t	*mapent;

	if (!Com_GetToken (true)) return false;

	if( !Com_MatchToken( "{" )) Sys_Break( "ParseEntity: '{' not found\n" );
	if( num_entities == MAX_MAP_ENTITIES ) Sys_Break( "MAX_MAP_ENTITIES limit excceded\n" );

	mapent = &entities[num_entities];
	num_entities++;

	do
	{
		if (!Com_GetToken (true)) Sys_Error("ParseEntity: EOF without closing brace");
		if (Com_MatchToken("}")) break;
		e = ParseEpair();
		e->next = mapent->epairs;
		mapent->epairs = e;
	} while (1);
	
	return true;
}

/*
================
ParseEntities

Parses the dentdata string into entities
================
*/
void ParseEntities (void)
{
	num_entities = 0;
	if(Com_LoadScript("entities", dentdata, entdatasize))
		while(ParseEntity());
}


/*
================
UnparseEntities

Generates the dentdata string from all the entities
================
*/
void UnparseEntities (void)
{
	char	*buf, *end;
	epair_t	*ep;
	char	line[2048];
	int	i;
	char	key[1024], value[1024];

	buf = dentdata;
	end = buf;
	*end = 0;
	
	for (i = 0; i < num_entities; i++)
	{
		ep = entities[i].epairs;
		if (!ep) continue;	// ent got removed
		
		strcat (end,"{\n");
		end += 2;
				
		for (ep = entities[i].epairs ; ep ; ep=ep->next)
		{
			strcpy (key, ep->key);
			StripTrailing (key);
			strcpy (value, ep->value);
			StripTrailing (value);
				
			sprintf (line, "\"%s\" \"%s\"\n", key, value);
			strcat (end, line);
			end += strlen(line);
		}
		strcat (end,"}\n");
		end += 2;

		if (end > buf + MAX_MAP_ENTSTRING) Sys_Error("Entity text too long");
	}
	entdatasize = end - buf + 1;
}

void SetKeyValue (bsp_entity_t *ent, char *key, char *value)
{
	epair_t	*ep;
	
	for (ep=ent->epairs ; ep ; ep=ep->next)
		if (!strcmp (ep->key, key) )
		{
			Mem_Free (ep->value);
			ep->value = copystring(value);
			return;
		}
	ep = Malloc (sizeof(*ep));
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = copystring(key);
	ep->value = copystring(value);
}

char *ValueForKey (bsp_entity_t *ent, char *key)
{
	epair_t	*ep;
	
	for (ep=ent->epairs ; ep ; ep=ep->next)
		if (!strcmp (ep->key, key) )
			return ep->value;
	return "";
}

vec_t FloatForKey (bsp_entity_t *ent, char *key)
{
	char	*k;
	
	k = ValueForKey (ent, key);
	return atof(k);
}

void GetVectorForKey (bsp_entity_t *ent, char *key, vec3_t vec)
{
	char	*k;
	double	v1, v2, v3;

	k = ValueForKey (ent, key);
	// scanf into doubles, then assign, so it is vec_t size independent
	v1 = v2 = v3 = 0;
	sscanf (k, "%lf %lf %lf", &v1, &v2, &v3);
	vec[0] = v1;
	vec[1] = v2;
	vec[2] = v3;
}


