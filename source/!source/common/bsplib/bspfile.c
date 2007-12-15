
#include "bsplib.h"

void GetLeafNums (void);

//=============================================================================

int	nummodels;
dmodel_t	dmodels[MAX_MAP_MODELS];

int	visdatasize;
byte	dvisdata[MAX_MAP_VISIBILITY];
dvis_t	*dvis = (dvis_t *)dvisdata;

int	lightdatasize;
byte	dlightdata[MAX_MAP_LIGHTING];

int	entdatasize;
char	dentdata[MAX_MAP_ENTSTRING];

int	numleafs;
dleaf_t	dleafs[MAX_MAP_LEAFS];

int	numplanes;
dplane_t	dplanes[MAX_MAP_PLANES];

int	numvertexes;
dvertex_t	dvertexes[MAX_MAP_VERTS];

int	numnodes;
dnode_t	dnodes[MAX_MAP_NODES];

int	numtexinfo;
dsurfdesc_t	texinfo[MAX_MAP_TEXINFO];

int	numfaces;
dface_t	dfaces[MAX_MAP_FACES];

int	numedges;
dedge_t	dedges[MAX_MAP_EDGES];

int	numleaffaces;
word	dleaffaces[MAX_MAP_LEAFFACES];

int	numleafbrushes;
word	dleafbrushes[MAX_MAP_LEAFBRUSHES];

int	numsurfedges;
int	dsurfedges[MAX_MAP_SURFEDGES];

int	numbrushes;
dbrush_t	dbrushes[MAX_MAP_BRUSHES];

int	numbrushsides;
dbrushside_t	dbrushsides[MAX_MAP_BRUSHSIDES];

int	numareas;
darea_t	dareas[MAX_MAP_AREAS];

int	numareaportals;
dareaportal_t	dareaportals[MAX_MAP_AREAPORTALS];

byte	dcollision[MAX_MAP_COLLISION];
int	dcollisiondatasize = 256;

/*
===============
CompressVis

===============
*/
int CompressVis (byte *vis, byte *dest)
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
void DecompressVis (byte *in, byte *decompressed)
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
	dmodel_t	*d;

	
	// models	
	for (i = 0; i < nummodels; i++)
	{
		d = &dmodels[i];

		d->firstface = LittleLong (d->firstface);
		d->numfaces = LittleLong (d->numfaces);
		d->headnode = LittleLong (d->headnode);
		
		for( j = 0; j < 3; j++ )
		{
			d->mins[j] = LittleFloat(d->mins[j]);
			d->maxs[j] = LittleFloat(d->maxs[j]);
		}
	}

	// vertexes
	for( i = 0; i < numvertexes; i++)
	{
		for (j=0 ; j<3 ; j++)
			dvertexes[i].point[j] = LittleFloat (dvertexes[i].point[j]);
	}
		
	// planes
	for(i = 0; i < numplanes; i++)
	{
		for(j = 0; j < 3; j++)
			dplanes[i].normal[j] = LittleFloat (dplanes[i].normal[j]);
		dplanes[i].dist = LittleFloat (dplanes[i].dist);
	}
	
	// texinfos
	for (i=0 ; i<numtexinfo ; i++)
	{
		for (j=0 ; j<8 ; j++)
			texinfo[i].vecs[0][j] = LittleFloat (texinfo[i].vecs[0][j]);
		texinfo[i].flags = LittleLong (texinfo[i].flags);
		texinfo[i].value = LittleLong (texinfo[i].value);
		texinfo[i].nexttexinfo = LittleLong (texinfo[i].nexttexinfo);
	}
	
	// faces
	for (i=0 ; i<numfaces ; i++)
	{
		dfaces[i].desc = LittleShort (dfaces[i].desc);
		dfaces[i].planenum = LittleShort (dfaces[i].planenum);
		dfaces[i].side = LittleShort (dfaces[i].side);
		dfaces[i].lightofs = LittleLong (dfaces[i].lightofs);
		dfaces[i].firstedge = LittleLong (dfaces[i].firstedge);
		dfaces[i].numedges = LittleShort (dfaces[i].numedges);
	}

	// nodes
	for (i=0 ; i<numnodes ; i++)
	{
		dnodes[i].planenum = LittleLong (dnodes[i].planenum);
		for (j=0 ; j<3 ; j++)
		{
			dnodes[i].mins[j] = LittleShort (dnodes[i].mins[j]);
			dnodes[i].maxs[j] = LittleShort (dnodes[i].maxs[j]);
		}
		dnodes[i].children[0] = LittleLong (dnodes[i].children[0]);
		dnodes[i].children[1] = LittleLong (dnodes[i].children[1]);
		dnodes[i].firstface = LittleShort (dnodes[i].firstface);
		dnodes[i].numfaces = LittleShort (dnodes[i].numfaces);
	}

	// leafs
	for (i=0 ; i<numleafs ; i++)
	{
		dleafs[i].contents = LittleLong (dleafs[i].contents);
		dleafs[i].cluster = LittleShort (dleafs[i].cluster);
		dleafs[i].area = LittleShort (dleafs[i].area);
		for (j=0 ; j<3 ; j++)
		{
			dleafs[i].mins[j] = LittleShort (dleafs[i].mins[j]);
			dleafs[i].maxs[j] = LittleShort (dleafs[i].maxs[j]);
		}

		dleafs[i].firstleafface = LittleShort (dleafs[i].firstleafface);
		dleafs[i].numleaffaces = LittleShort (dleafs[i].numleaffaces);
		dleafs[i].firstleafbrush = LittleShort (dleafs[i].firstleafbrush);
		dleafs[i].numleafbrushes = LittleShort (dleafs[i].numleafbrushes);
	}

	// leaffaces
	for (i=0 ; i<numleaffaces ; i++)
		dleaffaces[i] = LittleShort (dleaffaces[i]);

	// leafbrushes
	for (i=0 ; i<numleafbrushes ; i++)
		dleafbrushes[i] = LittleShort (dleafbrushes[i]);

	// surfedges
	for (i=0 ; i<numsurfedges ; i++)
		dsurfedges[i] = LittleLong (dsurfedges[i]);

	// edges
	for (i=0 ; i<numedges ; i++)
	{
		dedges[i].v[0] = LittleShort (dedges[i].v[0]);
		dedges[i].v[1] = LittleShort (dedges[i].v[1]);
	}

	// brushes
	for (i=0 ; i<numbrushes ; i++)
	{
		dbrushes[i].firstside = LittleLong (dbrushes[i].firstside);
		dbrushes[i].numsides = LittleLong (dbrushes[i].numsides);
		dbrushes[i].contents = LittleLong (dbrushes[i].contents);
	}

	// areas
	for (i=0 ; i<numareas ; i++)
	{
		dareas[i].numareaportals = LittleLong (dareas[i].numareaportals);
		dareas[i].firstareaportal = LittleLong (dareas[i].firstareaportal);
	}

	// areasportals
	for (i=0 ; i<numareaportals ; i++)
	{
		dareaportals[i].portalnum = LittleLong (dareaportals[i].portalnum);
		dareaportals[i].otherarea = LittleLong (dareaportals[i].otherarea);
	}

	// brushsides
	for (i=0 ; i<numbrushsides ; i++)
	{
		dbrushsides[i].planenum = LittleShort (dbrushsides[i].planenum);
		dbrushsides[i].surfdesc = LittleShort (dbrushsides[i].surfdesc);
	}

	// visibility
	if (todisk) j = dvis->numclusters;
	else j = LittleLong(dvis->numclusters);
	dvis->numclusters = LittleLong (dvis->numclusters);
	for (i = 0; i < j; i++)
	{
		dvis->bitofs[i][0] = LittleLong (dvis->bitofs[i][0]);
		dvis->bitofs[i][1] = LittleLong (dvis->bitofs[i][1]);
	}
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
	
	sprintf(path, "maps/%s.bsp", gs_mapname );
	buffer = (byte *)FS_LoadFile(path, &size);
	if(!size) return false;

	header = (dheader_t *)buffer; // load the file header
	if(pe) pe->LoadBSP( buffer );

	MsgDev(D_NOTE, "reading %s\n", path);
	
	// swap the header
	for(i = 0; i < sizeof(dheader_t)/4; i++)
		((int *)header)[i] = LittleLong (((int *)header)[i]);

	if(header->ident != IDBSPMODHEADER) Sys_Error("%s is not a IBSP file", gs_mapname);
	if(header->version != BSPMOD_VERSION) Sys_Error("%s is version %i, not %i", gs_mapname, header->version, BSPMOD_VERSION);

	entdatasize = CopyLump (LUMP_ENTITIES, dentdata, 1);
	numplanes = CopyLump (LUMP_PLANES, dplanes, sizeof(dplane_t));
	numvertexes = CopyLump (LUMP_VERTEXES, dvertexes, sizeof(dvertex_t));
	visdatasize = CopyLump (LUMP_VISIBILITY, dvisdata, 1);
	numnodes = CopyLump (LUMP_NODES, dnodes, sizeof(dnode_t));
	numtexinfo = CopyLump (LUMP_SURFDESC, texinfo, sizeof(dsurfdesc_t));
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
	numareas = CopyLump (LUMP_AREAS, dareas, sizeof(darea_t));
	numareaportals = CopyLump (LUMP_AREAPORTALS, dareaportals, sizeof(dareaportal_t));

	// swap everything
	SwapBSPFile (false);

	return true;
}


/*
=============
LoadBSPFileTexinfo

Only loads the texinfo lump, so qdata can scan for textures
=============
*/
void LoadBSPFileTexinfo( char *filename )
{
	int	i;
	file_t	*f;
	int	length, ofs;

	header = Malloc(sizeof(dheader_t));

	f = FS_Open (filename, "rb" );
	FS_Read(f, header, sizeof(dheader_t));
	//fread (header, sizeof(dheader_t), 1, f);

	// swap the header
	for (i=0 ; i< sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

	if (header->ident != IDBSPMODHEADER) Sys_Error("%s is not a IBSP file", filename);
	if (header->version != BSPMOD_VERSION) Sys_Error("%s is version %i, not %i", filename, header->version, BSPMOD_VERSION);

	length = header->lumps[LUMP_SURFDESC].filelen;
	ofs = header->lumps[LUMP_SURFDESC].fileofs;

	FS_Seek(f, ofs, SEEK_SET);
	FS_Read(f, texinfo, length );
	FS_Close(f);

	numtexinfo = length / sizeof(dsurfdesc_t);
	Free (header); // everything has been copied out
	SwapBSPFile (false);
}


//============================================================================

file_t *wadfile;
dheader_t	outheader;

void AddLump (int lumpnum, const void *data, int len)
{
	lump_t *lump;

	lump = &header->lumps[lumpnum];
	lump->fileofs = LittleLong( FS_Tell(wadfile) );
	lump->filelen = LittleLong( len );

	FS_Write(wadfile, data, (len+3)&~3 );
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
	
	//build path
	sprintf (path, "maps/%s.bsp", gs_mapname );
	MsgDev(D_NOTE, "writing %s\n", path);
	if(pe) pe->FreeBSP();
	
	wadfile = FS_Open( path, "wb" );
	FS_Write( wadfile, header, sizeof(dheader_t));	// overwritten later

	AddLump (LUMP_ENTITIES, dentdata, entdatasize);
	AddLump (LUMP_PLANES, dplanes, numplanes*sizeof(dplane_t));
	AddLump (LUMP_VERTEXES, dvertexes, numvertexes*sizeof(dvertex_t));
	AddLump (LUMP_VISIBILITY, dvisdata, visdatasize);
	AddLump (LUMP_NODES, dnodes, numnodes*sizeof(dnode_t));
	AddLump (LUMP_SURFDESC, texinfo, numtexinfo*sizeof(dsurfdesc_t));
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
			Free (ep->value);
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


