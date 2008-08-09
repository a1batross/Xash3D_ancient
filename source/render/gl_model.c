/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// models.c -- model loading and caching

#include "gl_local.h"
#include "byteorder.h"

rmodel_t	*loadmodel;
int		modfilelen;

void Mod_LoadSpriteModel (rmodel_t *mod, void *buffer);
void Mod_LoadStudioModel (rmodel_t *mod, void *buffer);
void Mod_LoadBrushModel (rmodel_t *mod, void *buffer);
rmodel_t *Mod_LoadModel (rmodel_t *mod, bool crash);

byte	mod_novis[MAX_MAP_LEAFS/8];
rmodel_t	mod_known[MAX_MODELS];
rmodel_t	mod_inline[MAX_MODELS];
int	mod_numknown;

// the inline * models from the current map are kept seperate


int registration_sequence;

const char *Mod_GetStringFromTable( int index )
{
	if( loadmodel->stringdata )
		return &loadmodel->stringdata[loadmodel->stringtable[index]];
	return NULL;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, rmodel_t *model)
{
	mnode_t		*node;
	float		d;
	cplane_t		*plane;
	
	if (!model || !model->nodes) Host_Error("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents != -1)
			return (mleaf_t *)node;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0) node = node->children[0];
		else node = node->children[1];
	}
	
	return NULL;	// never reached
}


/*
===================
Mod_DecompressVis
===================
*/
byte *Mod_DecompressVis (byte *in, rmodel_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	byte	*out;
	int		row;

	row = (model->vis->numclusters+7)>>3;	
	out = decompressed;

	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;		
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
	
	return decompressed;
}

/*
==============
Mod_ClusterPVS
==============
*/
byte *Mod_ClusterPVS (int cluster, rmodel_t *model)
{
	if (cluster == -1 || !model->vis) return mod_novis;
	return Mod_DecompressVis ( (byte *)model->vis + model->vis->bitofs[cluster][DVIS_PVS], model);
}


//===============================================================================

/*
================
Mod_Modellist_f
================
*/
void Mod_Modellist_f (void)
{
	int		i;
	rmodel_t	*mod;
	int		total;

	total = 0;
	Msg("Loaded models:\n");
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		//Msg("%8i : %s\n",mod->mempool->totalsize, mod->name);
		//total += mod->mempool->totalsize;
	}
	Msg("Total resident: %i\n", total);
}

/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
rmodel_t *Mod_ForName( const char *name, bool crash)
{
	rmodel_t	*mod;
	uint	*buf;
	int	i;
	
	if( !name[0] ) return NULL;

	// inline models are grabbed only from worldmodel
	if( name[0] == '*' )
	{
		i = com.atoi(name + 1);
		if( i < 1 || !r_worldmodel || i >= r_worldmodel->numsubmodels)
		{
			Msg("Warning: bad inline model number %i\n", i );
			return NULL;
		}
		// prolonge registration
		mod_inline[i].registration_sequence = registration_sequence;
		return &mod_inline[i];
	}

	// search the currently loaded models
	for( i = 0, mod = mod_known; i < mod_numknown; i++, mod++ )
	{
		if (!mod->name[0]) continue;
		if (!com.strcmp (mod->name, name))
		{
			// prolonge registration
			mod->registration_sequence = registration_sequence;
			return mod;
		}
	}
	
	// find a free model slot spot
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!mod->name[0]) break;// free spot
	}
	if (i == mod_numknown)
	{
		if( mod_numknown == MAX_MODELS )
		{
			MsgDev( D_ERROR, "Mod_ForName: MAX_MODELS limit exceeded\n" );
			return NULL;
		}
		mod_numknown++;
	}
	com.strncpy( mod->name, name, MAX_STRING );
	
	// load the file
	buf = (uint *)FS_LoadFile (mod->name, &modfilelen);
	if (!buf)
	{
		if (crash) Host_Error( "Mod_NumForName: %s not found\n", mod->name);
		memset (mod->name, 0, sizeof(mod->name));
		return NULL;
	}

	mod->mempool = Mem_AllocPool(va("^1%s^7", mod->name ));
	
	loadmodel = mod;
	
	//
	// fill it in
	//

	// call the apropriate loader
	switch (LittleLong(*(uint *)buf))
	{
	case IDBSPMODHEADER:
		Mod_LoadBrushModel (mod, buf);
		break;
	case IDSTUDIOHEADER:
		Mod_LoadStudioModel ( mod, buf );
		break;
	case IDSPRITEHEADER:
		Mod_LoadSpriteModel ( mod, buf );
		break;
	default:
		//will be freed at end of registration
		Msg("Mod_NumForName: unknown file id for %s, unloaded\n", mod->name);
		break;
	}
	Mem_Free( buf );//free buffer

	return mod;
}

/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

byte	*mod_base;


/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}
	loadmodel->lightdata = Mem_Alloc( loadmodel->mempool, l->filelen);
	memcpy(loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}

/*
=================
Mod_LoadStringData
=================
*/
void Mod_LoadStringData( lump_t *l )
{
	if (!l->filelen)
	{
		loadmodel->stringdata = NULL;
		return;
	}
	loadmodel->stringdata = (char *)Mem_Alloc( loadmodel->mempool, l->filelen );
	Mem_Copy(loadmodel->stringdata, mod_base + l->fileofs, l->filelen );
}

void Mod_LoadStringTable( lump_t *l )
{	
	int	*in, *out;
	int	i, count;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("MOD_LoadBmodel: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof(*in);
	loadmodel->stringtable = out = (int *)Mem_Alloc( loadmodel->mempool, l->filelen );
	for ( i = 0; i < count; i++) out[i] = LittleLong(in[i]);
}

/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility (lump_t *l)
{
	int		i;

	if (!l->filelen)
	{
		loadmodel->vis = NULL;
		return;
	}
	loadmodel->vis = Mem_Alloc( loadmodel->mempool, l->filelen);
	memcpy (loadmodel->vis, mod_base + l->fileofs, l->filelen);

	loadmodel->vis->numclusters = LittleLong (loadmodel->vis->numclusters);
	for (i=0 ; i<loadmodel->vis->numclusters ; i++)
	{
		loadmodel->vis->bitofs[i][0] = LittleLong (loadmodel->vis->bitofs[i][0]);
		loadmodel->vis->bitofs[i][1] = LittleLong (loadmodel->vis->bitofs[i][1]);
	}
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t		*in;
	mvertex_t		*out;
	int		i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc( loadmodel->mempool, count*sizeof(*out));

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t		*in;
	msubmodel_t	*out;
	int		i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
          out = Mem_Alloc( loadmodel->mempool, count*sizeof(*out));
	
	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{	
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
		}
		out->radius = RadiusFromBounds (out->mins, out->maxs);
		out->headnode = LittleLong (in->headnode);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges (lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc( loadmodel->mempool, count * sizeof(*out));

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (uint)LittleLong(in->v[0]);
		out->v[1] = (uint)LittleLong(in->v[1]);
	}
}

/*
=================
Mod_LoadSurfDesc
=================
*/
void Mod_LoadSurfDesc( lump_t *l )
{
	dsurfdesc_t	*in;
	mtexinfo_t	*out, *step;
	int		i, j, count;
	int		next;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
          out = Mem_Alloc( loadmodel->mempool, count * sizeof(*out));
	
	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		for( j = 0; j < 8; j++ )
			out->vecs[0][j] = LittleFloat(in->vecs[0][j]);

		out->flags = LittleLong (in->flags);
		next = LittleLong (in->animid);
		if( next > 0 ) out->next = loadmodel->texinfo + next;
		else out->next = NULL;

		// fixed texture size
		out->size[0] = LittleLong( in->size[0] );
		out->size[1] = LittleLong( in->size[1] );
		out->texid = LittleLong( in->texid );		// will be loading later
		out->image = r_notexture;			// make default
	}

	// count animation frames
	for( i = 0; i < count; i++ )
	{
		out = &loadmodel->texinfo[i];
		out->numframes = 1;
		for( step = out->next; step && step != out; step = step->next )
			out->numframes++;
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t		*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;
	
	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0) v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] + 
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;

//		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 512 /* 256 */ )
//			Host_Error ("Bad surface extents");
	}
}


void GL_BuildPolygonFromSurface(msurface_t *fa);
void GL_CreateSurfaceLightmap (msurface_t *surf);
void GL_EndBuildingLightmaps (void);
void GL_BeginBuildingLightmaps (rmodel_t *m);

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum;
	int			planenum, side;
	int			ti;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc( loadmodel->mempool, count*sizeof(*out));	

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	m_pRenderModel = loadmodel;

	GL_BeginBuildingLightmaps (loadmodel);

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleLong(in->numedges);		
		out->flags = 0;
		out->polys = NULL;

		planenum = LittleLong( in->planenum );
		side = LittleShort(in->side);
		if( side ) out->flags |= SURF_PLANEBACK;			

		out->plane = loadmodel->planes + planenum;

		ti = LittleLong (in->desc);
		if (ti < 0 || ti >= loadmodel->numtexinfo)
			Host_Error("MOD_LoadBmodel: bad texinfo number");
		out->texinfo = loadmodel->texinfo + ti;

		CalcSurfaceExtents (out);
				
		// lighting info

		for( i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else
			out->samples = loadmodel->lightdata + i;
		
	// set the drawing flags
		
		if (out->texinfo->flags & SURF_WARP)
		{
			out->flags |= SURF_DRAWTURB;
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			GL_SubdivideSurface (out);	// cut up polygon for warps
		}
		// create lightmaps and polygons
		if(!(out->texinfo->flags & (SURF_SKY|SURF_BLEND|SURF_WARP) ) )
			GL_CreateSurfaceLightmap (out);

		if (! (out->texinfo->flags & SURF_WARP) ) 
			GL_BuildPolygonFromSurface(out);

	}

	GL_EndBuildingLightmaps ();
}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents != -1)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes (lump_t *l)
{
	int		i, j, count, p;
	dnode_t		*in;
	mnode_t 		*out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc( loadmodel->mempool, count*sizeof(*out));

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleLong (in->mins[j]);
			out->minmaxs[3+j] = LittleLong (in->maxs[j]);
		}
	
		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleLong (in->firstface);
		out->numsurfaces = LittleLong (in->numfaces);
		out->contents = -1;	// differentiate from leafs

		for (j=0 ; j<2 ; j++)
		{
			p = LittleLong (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}
	
	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs (lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;
//	glpoly_t	*poly;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc( loadmodel->mempool, count*sizeof(*out));

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleLong (in->mins[j]);
			out->minmaxs[3+j] = LittleLong (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->cluster = LittleLong(in->cluster);
		out->area = LittleLong(in->area);

		out->firstmarksurface = loadmodel->marksurfaces + LittleLong(in->firstleafface);
		out->nummarksurfaces = LittleLong(in->numleaffaces);
		
		// gl underwater warp
#if 0
		if (out->contents & (CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA|CONTENTS_THINWATER) )
		{
			for (j=0 ; j<out->nummarksurfaces ; j++)
			{
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
				for (poly = out->firstmarksurface[j]->polys ; poly ; poly=poly->next)
					poly->flags |= SURF_UNDERWATER;
			}
		}
#endif
	}	
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces (lump_t *l)
{	
	int		i, j, count;
	dword		*in;
	msurface_t 	**out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc( loadmodel->mempool, count*sizeof(*out));

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleLong(in[i]);
		if (j < 0 ||  j >= loadmodel->numsurfaces)
			Host_Error ("Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges (lump_t *l)
{	
	int		i, count;
	int		*in, *out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES)
		Host_Error ("MOD_LoadBmodel: bad surfedges count in %s: %i",
		loadmodel->name, count);

	out = Mem_Alloc( loadmodel->mempool, count*sizeof(*out));	

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
}


/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes (lump_t *l)
{
	int		i, j;
	cplane_t		*out;
	dplane_t		*in;
	int		count;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc( loadmodel->mempool, count*2*sizeof(*out));	
	
	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
			out->normal[j] = LittleFloat (in->normal[j]);
		out->dist = LittleFloat( in->dist );
		PlaneClassify( out );
	}
}

/*
=================
Mod_SetupSubmodels
=================
*/
void Mod_SetupSubmodels( rmodel_t *mod )
{
	int		i;
	msubmodel_t 	*bm;
	
	for (i = 0; i < mod->numsubmodels; i++ )
	{
		rmodel_t	*starmod;

		bm = &mod->submodels[i];
		starmod = &mod_inline[i];
		*starmod = *loadmodel;
		
		starmod->firstmodelsurface = bm->firstface;
		starmod->nummodelsurfaces = bm->numfaces;
		starmod->type = mod_brush;
		starmod->firstnode = bm->headnode;
		if( starmod->firstnode >= loadmodel->numnodes )
			Host_Error ("Inline model %i has bad firstnode", i);

		VectorCopy (bm->maxs, starmod->maxs);
		VectorCopy (bm->mins, starmod->mins);
		starmod->radius = bm->radius;
	
		if (i == 0) *loadmodel = *starmod;
		starmod->numleafs = bm->visleafs;
	}
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel (rmodel_t *mod, void *buffer)
{
	int	i;
	dheader_t	*header;

	
	loadmodel->type = mod_world;
	if (loadmodel != mod_known)
	{
		Msg("Warning: loaded a brush model after the world\n");
		return;
	}

	header = (dheader_t *)buffer;

	i = LittleLong (header->version);
	if (i != BSPMOD_VERSION)
		Host_Error ("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPMOD_VERSION);

	// swap all the lumps
	mod_base = (byte *)header;

	for (i = 0; i < sizeof(dheader_t)/4; i++) ((int *)header)[i] = LittleLong ( ((int *)header)[i]);

	// load into heap
	Mod_LoadStringData (&header->lumps[LUMP_STRINGDATA]);
	Mod_LoadStringTable (&header->lumps[LUMP_STRINGTABLE]);
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadSurfDesc (&header->lumps[LUMP_SURFDESC]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_LEAFFACES]);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes (&header->lumps[LUMP_NODES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);
	Mod_SetupSubmodels( mod );// set up the submodels

	mod->numframes = 2;					// regular and alternate animation
	mod->registration_sequence = registration_sequence;	// register model
}

/*
=================
Mod_LoadStudioModel
=================
*/
void Mod_LoadStudioModel (rmodel_t *mod, void *buffer)
{
	R_StudioLoadModel (mod, buffer);
	mod->type = mod_studio;
}

/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (rmodel_t *mod, void *buffer)
{
	R_SpriteLoadModel( mod, buffer );
	mod->type = mod_sprite;
}

//=============================================================================

/*
@@@@@@@@@@@@@@@@@@@@@
R_BeginRegistration

Specifies the model that will be used as the world
@@@@@@@@@@@@@@@@@@@@@
*/
void R_BeginRegistration( const char *mapname )
{
	char	fullname[MAX_QPATH];

	registration_sequence++;
	r_oldviewcluster = -1;		// force markleafs

	com.sprintf (fullname, "maps/%s.bsp", mapname );

	// explicitly free the old map if different
	// this guarantees that mod_known[0] is the world map
	if( com.strcmp( mod_known[0].name, fullname ))
		Mod_Free( &mod_known[0] );
	r_worldmodel = Mod_ForName( fullname, true );

	r_viewcluster = -1;
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_RegisterModel

@@@@@@@@@@@@@@@@@@@@@
*/
rmodel_t *R_RegisterModel (const char *name)
{
	rmodel_t		*mod;
	int		i;
	
	mod = Mod_ForName (name, false);
	if (mod)
	{
		switch(mod->type)
		{
		case mod_world:
		case mod_brush:
			for (i = 0; i < mod->numtexinfo; i++)
				mod->texinfo[i].image->registration_sequence = registration_sequence;
			break;
		case mod_studio:
		case mod_sprite:
			for(i = 0; i < mod->numtexinfo; i++)
				mod->skins[i]->registration_sequence = registration_sequence;
			break;
		default: return NULL;
		}
	}
	return mod;
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_EndRegistration

@@@@@@@@@@@@@@@@@@@@@
*/
void R_EndRegistration (void)
{
	int	i;
	rmodel_t	*mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!mod->name[0]) continue;
		if (mod->registration_sequence != registration_sequence)
			Mod_Free (mod);
	}
	R_ImageFreeUnused();
}


//=============================================================================


/*
================
Mod_Free
================
*/
void Mod_Free (rmodel_t *mod)
{
	Mem_FreePool( &mod->mempool );
	memset (mod, 0, sizeof(*mod));
	mod = NULL;
}

/*
================
Mod_FreeAll
================
*/
void Mod_FreeAll (void)
{
	int		i;

	for (i=0 ; i<mod_numknown ; i++)
	{
		if (mod_known[i].mempool)
			Mod_Free (&mod_known[i]);
	}
}
