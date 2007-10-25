//=======================================================================
//			Copyright XashXT Group 2007 ©
//			 utils.c - physics callbacks
//=======================================================================

#include "physic.h"

long _ftol( double );
long _ftol2( double dblSource ) 
{
	return _ftol( dblSource );
}

int		numsurfaces;
int		numverts, numedges;
int		*map_surfedges;
csurface_t	*map_surfaces;
dvertex_t		*map_vertices;
dedge_t		*map_edges;
byte		*pmod_base;
NewtonCollision	*collision;

void* Palloc (int size )
{
	return Mem_Alloc( physpool, size );
}

void Pfree (void *ptr, int size )
{
	Mem_Free( ptr );
}

/*
================
GL_BuildPolygonFromSurface
================
*/

void Phys_BuildPolygonFromSurface(csurface_t *fa)
{
	int		i, lindex;
	dedge_t		*cur_edge;
	float		*vec;
	vec3_t		*face;

	// reconstruct the polygon
	face = Mem_Alloc( physpool, sizeof(vec3_t) * fa->numedges);

	for(i = 0; i < fa->numedges; i++)
	{
		lindex = map_surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			cur_edge = &map_edges[lindex];
			vec = map_vertices[cur_edge->v[0]].point;
		}
		else
		{
			cur_edge = &map_edges[-lindex];
			vec = map_vertices[cur_edge->v[1]].point;
		}
		VectorCopy (vec, face[i]);
	}
	// add newton polygon
	NewtonTreeCollisionAddFace(collision, fa->numedges, (float *)face, sizeof(vec3_t), 1);
}

void Phys_LoadVerts( lump_t *l )
{
	dvertex_t		*in, *out;
	int		i, count;

	in = (void *)(pmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("MOD_LoadBmodel: funny lump size\n");
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc( physpool, count * sizeof(*out));

	map_vertices = out;
	numverts = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		out->point[0] = LittleFloat (in->point[0]);
		out->point[1] = LittleFloat (in->point[1]);
		out->point[2] = LittleFloat (in->point[2]);
	}
}

void Phys_LoadFaces( lump_t *l )
{
	dface_t		*in;
	csurface_t 	*out;
	int		side, count, surfnum;

	in = (void *)(pmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("MOD_LoadBmodel: funny lump size\n");
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc( physpool, count * sizeof(*out));	

	map_surfaces = out;
	numsurfaces = count;

	for ( surfnum = 0; surfnum < count; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);		
		out->flags = 0;

		side = LittleShort(in->side);
		if (side) out->flags |= SURF_PLANEBACK;			
		Phys_BuildPolygonFromSurface( out );
	}
}

void Phys_LoadEdges (lump_t *l)
{
	dedge_t	*in, *out;
	int 	i, count;

	in = (void *)(pmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("MOD_LoadBmodel: funny lump size\n");
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc( physpool, count * sizeof(*out));

	map_edges = out;
	numedges = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		out->v[0] = (word)LittleShort(in->v[0]);
		out->v[1] = (word)LittleShort(in->v[1]);
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Phys_LoadSurfedges (lump_t *l)
{	
	int	i, count;
	int	*in, *out;
	
	in = (void *)(pmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("MOD_LoadBmodel: funny lump size\n");
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES) Host_Error("MOD_LoadBmodel: funny lump size\n");

	map_surfedges = out = Mem_Alloc( physpool, count * sizeof(*out));	
	for ( i = 0; i < count; i++) out[i] = LittleLong (in[i]);
}

void Phys_LoadBSP( uint *buffer )
{
	dheader_t		header;

	header = *(dheader_t *)buffer;
	pmod_base = (byte *)buffer;

	// create the collsion tree geometry
	collision = NewtonCreateTreeCollision(gWorld, NULL );
	NewtonTreeCollisionBeginBuild( collision );

	Phys_LoadVerts(&header.lumps[LUMP_VERTEXES]);
	Phys_LoadEdges(&header.lumps[LUMP_EDGES]);
	Phys_LoadSurfedges(&header.lumps[LUMP_SURFEDGES]);
	Phys_LoadFaces(&header.lumps[LUMP_FACES]);

	NewtonTreeCollisionEndBuild(collision, 1);
	Msg("physic map generated\n");

}