
#include "bsplib.h"
#include "byteorder.h"
#include "const.h"

//=============================================================================
wfile_t		*handle;
int		s_table;
int		nummodels;
dmodel_t		dmodels[MAX_MAP_MODELS];
int		visdatasize;
byte		dvisdata[MAX_MAP_VISIBILITY];
dvis_t		*dvis = (dvis_t *)dvisdata;
int		lightdatasize;
byte		dlightdata[MAX_MAP_LIGHTING];
int		entdatasize;
char		dentdata[MAX_MAP_ENTSTRING];
int		numleafs;
dleaf_t		dleafs[MAX_MAP_LEAFS];
int		numplanes;
dplane_t		dplanes[MAX_MAP_PLANES];
int		numvertexes;
dvertex_t		dvertexes[MAX_MAP_VERTS];
int		numnodes;
dnode_t		dnodes[MAX_MAP_NODES];
int		numtexinfo;
dtexinfo_t	texinfo[MAX_MAP_TEXINFO];
int		numfaces;
dface_t		dfaces[MAX_MAP_FACES];
int		numedges;
dedge_t		dedges[MAX_MAP_EDGES];
int		numleaffaces;
dword		dleaffaces[MAX_MAP_LEAFFACES];
int		numleafbrushes;
dword		dleafbrushes[MAX_MAP_LEAFBRUSHES];
int		numsurfedges;
int		dsurfedges[MAX_MAP_SURFEDGES];
int		numbrushes;
dbrush_t		dbrushes[MAX_MAP_BRUSHES];
int		numbrushsides;
dbrushside_t	dbrushsides[MAX_MAP_BRUSHSIDES];
int		nummiptex;
dmiptex_t		dmiptex[MAX_MAP_TEXTURES];
int		numareas;
darea_t		dareas[MAX_MAP_AREAS];
int		numareaportals;
dareaportal_t	dareaportals[MAX_MAP_AREAPORTALS];
byte		dcollision[MAX_MAP_COLLISION];
int		dcollisiondatasize = 256; // variable sized
int		dindexes[MAX_MAP_INDEXES];
int		numindexes;
char		dstringdata[MAX_MAP_STRINGDATA];
int		stringdatasize;
int		dstringtable[MAX_MAP_NUMSTRINGS];
int		numstrings;

/*
===============
String table system

===============
*/
const char *GetStringFromTable( int index )
{
	return &dstringdata[dstringtable[index]];
}

int GetIndexFromTable( const char *string )
{
	int i, len;

	for( i = 0; i < numstrings; i++ )
	{
		if(!com.stricmp( string, &dstringdata[dstringtable[i]]))
			return i; // found index
	}

	// register new string
	len = com.strlen( string );
	if( len + stringdatasize + 1 > MAX_MAP_STRINGDATA )
		Sys_Error( "GetIndexFromTable: string data lump limit exeeded\n" );

	if( numstrings + 1 > MAX_MAP_NUMSTRINGS )
		Sys_Error( "GetIndexFromTable: string table lump limit exeeded\n" );

	com.strcpy( &dstringdata[stringdatasize], string );
	dstringtable[numstrings] = stringdatasize;
	stringdatasize += len + 1; // null terminator
	numstrings++;

	return numstrings - 1; // current index
}

/*
===============
CompressVis

===============
*/
int CompressVis( byte *vis, byte *dest )
{
	int		j;
	int		rep;
	int		visrow;
	byte	*dest_p;
	
	dest_p = dest;
	visrow = (dvis->numclusters + 7)>>3;
	
	for (j=0 ; j<visrow ; j++)
	{
		*dest_p++ = vis[j];
		if (vis[j])
			continue;

		rep = 1;
		for ( j++; j<visrow ; j++)
			if (vis[j] || rep == 255)
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

//	row = (r_numvisleafs+7)>>3;	
	row = (dvis->numclusters+7)>>3;	
	out = decompressed;

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		if (!c) Sys_Error("DecompressVis: 0 repeat");
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
}

//=============================================================================

/*
=============
SwapBSPFile

Byte swaps all data in a bsp file.
=============
*/
void SwapBSPFile (bool todisk)
{
	int	i, j;
	
	// models	
	SwapBlock( (int *)dmodels, nummodels * sizeof(dmodels[0]));

	// vertexes
	SwapBlock( (int *)dvertexes, numvertexes * sizeof(dvertexes[0]));

	// planes
	SwapBlock( (int *)dplanes, numplanes * sizeof(dplanes[0]));
	
	// texinfos
	SwapBlock( (int *)texinfo, numtexinfo * sizeof(texinfo[0]));

	// nodes
	SwapBlock( (int *)dnodes, numnodes * sizeof(dnodes[0]));

	// leafs
	SwapBlock( (int *)dleafs, numleafs * sizeof(dleafs[0]));

	// leaffaces
	SwapBlock( (int *)dleaffaces, numleaffaces * sizeof(dleaffaces[0]));

	// leafbrushes
	SwapBlock( (int *)dleafbrushes, numleafbrushes * sizeof(dleafbrushes[0]));

	// surfedges
	SwapBlock( (int *)texinfo, numsurfedges * sizeof(dsurfedges[0]));

	// edges
	SwapBlock( (int *)dedges, numedges * sizeof(dedges[0]));

	// brushes
	SwapBlock( (int *)dbrushes, numbrushes * sizeof(dbrushes[0]));

	// areas
	SwapBlock( (int *)dareas, numareas * sizeof(dareas[0]));

	// areasportals
	SwapBlock( (int *)dareaportals, numareaportals * sizeof(dareaportals[0]));

	// brushsides
	SwapBlock( (int *)dbrushsides, numbrushsides * sizeof(dbrushsides[0]));

	// indices
	SwapBlock( (int *)dindexes, numindexes * sizeof(dindexes[0]));

	// miptexes
	SwapBlock( (int *)dmiptex, nummiptex * sizeof(dmiptex[0]));

	// faces
	// FIXME: i want using SwapBlock too
	for( i = 0; i < numfaces; i++ )
	{
		dfaces[i].planenum = LittleLong( dfaces[i].planenum );
		dfaces[i].firstedge = LittleLong( dfaces[i].firstedge );
		dfaces[i].numedges = LittleLong( dfaces[i].numedges );
		dfaces[i].texinfo = LittleLong( dfaces[i].texinfo );	
		dfaces[i].lightofs = LittleLong( dfaces[i].lightofs );
		dfaces[i].side = LittleShort (dfaces[i].side);
	}

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
	numplanes = CopyLump (LUMP_PLANES, dplanes, sizeof(dplane_t));
	numvertexes = CopyLump (LUMP_VERTEXES, dvertexes, sizeof(dvertex_t));
	visdatasize = CopyLump (LUMP_VISIBILITY, dvisdata, 1);
	numnodes = CopyLump (LUMP_NODES, dnodes, sizeof(dnode_t));
	numtexinfo = CopyLump (LUMP_TEXINFO, texinfo, sizeof(dtexinfo_t));
	numfaces = CopyLump (LUMP_FACES, dfaces, sizeof(dface_t));
	lightdatasize = CopyLump (LUMP_LIGHTING, dlightdata, 1);
	numleafs = CopyLump (LUMP_LEAFS, dleafs, sizeof(dleaf_t));
	numleaffaces = CopyLump (LUMP_LEAFFACES, dleaffaces, sizeof(dleaffaces[0]));
	numleafbrushes = CopyLump (LUMP_LEAFBRUSHES, dleafbrushes, sizeof(dleafbrushes[0]));
	numedges = CopyLump (LUMP_EDGES, dedges, sizeof(dedge_t));
	numsurfedges = CopyLump (LUMP_SURFEDGES, dsurfedges, sizeof(dsurfedges[0]));
	nummodels = CopyLump (LUMP_MODELS, dmodels, sizeof(dmodel_t));
	numbrushes = CopyLump (LUMP_BRUSHES, dbrushes, sizeof(dbrush_t));
	numbrushsides = CopyLump (LUMP_BRUSHSIDES, dbrushsides, sizeof(dbrushside_t));
	dcollisiondatasize = CopyLump(LUMP_COLLISION, dcollision, 1);
	numindexes = CopyLump (LUMP_INDEXES, dindexes, sizeof(dindexes[0]));
	nummiptex = CopyLump(LUMP_TEXTURES, dmiptex, sizeof(dmiptex_t));
	stringdatasize = CopyLump( LUMP_STRINGDATA, dstringdata, sizeof(dstringdata[0]));
	numstrings = CopyLump( LUMP_STRINGTABLE, dstringtable, sizeof(dstringtable[0]));
	numareas = CopyLump (LUMP_AREAS, dareas, sizeof(darea_t));
	numareaportals = CopyLump (LUMP_AREAPORTALS, dareaportals, sizeof(dareaportal_t));

	// swap everything
	SwapBSPFile (false);

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
	memset (header, 0, sizeof(dheader_t));
	
	SwapBSPFile (true);

	header->ident = LittleLong (IDBSPMODHEADER);
	header->version = LittleLong (BSPMOD_VERSION);
	
	// build path
	sprintf( path, "maps/%s.bsp", gs_filename );
	MsgDev( D_NOTE, "writing %s\n", path );
	if( pe ) pe->FreeBSP();
	
	wadfile = FS_Open( path, "wb" );
	FS_Write( wadfile, header, sizeof(dheader_t));	// overwritten later

	AddLump (LUMP_ENTITIES, dentdata, entdatasize);
	AddLump (LUMP_PLANES, dplanes, numplanes*sizeof(dplane_t));
	AddLump (LUMP_VERTEXES, dvertexes, numvertexes*sizeof(dvertex_t));
	AddLump (LUMP_VISIBILITY, dvisdata, visdatasize);
	AddLump (LUMP_NODES, dnodes, numnodes*sizeof(dnode_t));
	AddLump (LUMP_TEXINFO, texinfo, numtexinfo*sizeof(dtexinfo_t));
	AddLump (LUMP_FACES, dfaces, numfaces*sizeof(dface_t));
	AddLump (LUMP_LIGHTING, dlightdata, lightdatasize);
	AddLump (LUMP_LEAFS, dleafs, numleafs*sizeof(dleaf_t));
	AddLump (LUMP_LEAFFACES, dleaffaces, numleaffaces*sizeof(dleaffaces[0]));
	AddLump (LUMP_LEAFBRUSHES, dleafbrushes, numleafbrushes*sizeof(dleafbrushes[0]));
	AddLump (LUMP_EDGES, dedges, numedges*sizeof(dedge_t));
	AddLump (LUMP_SURFEDGES, dsurfedges, numsurfedges*sizeof(dsurfedges[0]));
	AddLump (LUMP_MODELS, dmodels, nummodels*sizeof(dmodel_t));
	AddLump (LUMP_BRUSHES, dbrushes, numbrushes*sizeof(dbrush_t));
	AddLump (LUMP_BRUSHSIDES, dbrushsides, numbrushsides*sizeof(dbrushside_t));
	AddLump (LUMP_COLLISION, dcollision, dcollisiondatasize );
	AddLump (LUMP_TEXTURES, dmiptex, nummiptex*sizeof(dmiptex[0]));
	AddLump (LUMP_INDEXES, dindexes, numindexes*sizeof(dindexes[0]));
	AddLump (LUMP_STRINGDATA, dstringdata, stringdatasize * sizeof(dstringdata[0]));
	AddLump (LUMP_STRINGTABLE, dstringtable, numstrings * sizeof(dstringtable[0]));
	AddLump (LUMP_AREAS, dareas, numareas*sizeof(darea_t));
	AddLump (LUMP_AREAPORTALS, dareaportals, numareaportals*sizeof(dareaportal_t));

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

	if (!Com_MatchToken( "{")) Sys_Error ("ParseEntity: { not found");
	if (num_entities == MAX_MAP_ENTITIES) Sys_Error ("num_entities == MAX_MAP_ENTITIES");

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


