//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_model.c - collision model
//=======================================================================

#include "cm_local.h"
#include "byteorder.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "const.h"

// to allow boxes to be treated as brush models, we allocate
// some extra indexes along with those needed by the map
#define BOX_BRUSHES		1
#define BOX_SIDES		6
#define BOX_LEAFS		2
#define BOX_PLANES		12
#define MAX_PATCH_SIZE	64
#define MAX_PATCH_VERTS	(MAX_PATCH_SIZE * MAX_PATCH_SIZE)

clipmap_t			cm;
clipmap_static_t		cms;

cmodel_t			*sv_models[MAX_MODELS]; // server replacement modeltable
cmodel_t        		box_model;
cplane_t       		*box_planes;
cbrush_t       		*box_brush;
cmodel_t			*loadmodel;

/*
===============================================================================

			CM COMMON UTILS

===============================================================================
*/
/*
===================
CM_InitBoxHull

Set up the planes and nodes so that the six floats of a bounding box
can just be stored out and get a proper clipping hull structure.
===================
*/
void CM_InitBoxHull( void )
{
	cplane_t       *p;
	cbrushside_t   *s;
	int             i, side;

	box_planes = &cm.planes[cm.numplanes];

	box_brush = &cm.brushes[cm.numbrushes];
	box_brush->numsides = 6;
	box_brush->sides = cm.brushsides + cm.numbrushsides;
	box_brush->contents = CONTENTS_BODY;
	box_brush->edges = (cbrushedge_t *)Mem_Alloc( cms.mempool, sizeof( cbrushedge_t ) * 12 );
	box_brush->numedges = 12;

	box_model.leaf.numleafbrushes = 1;
	box_model.leaf.firstleafbrush = cm.numleafbrushes;
	cm.leafbrushes[cm.numleafbrushes] = cm.numbrushes;

	for(i = 0; i < 6; i++)
	{
		side = i & 1;

		// brush sides
		s = &cm.brushsides[cm.numbrushsides + i];
		s->plane = cm.planes + (cm.numplanes + i * 2 + side);
		s->shadernum = 0;

		// planes
		p = &box_planes[i * 2];
		p->type = i>>1;
		p->signbits = 0;
		VectorClear(p->normal);
		p->normal[i>>1] = 1;

		p = &box_planes[i * 2 + 1];
		p->type = 3 + (i>>1);
		p->signbits = 0;
		VectorClear( p->normal );
		p->normal[i>>1] = -1;

		p->signbits = SignbitsForPlane( p->normal );
	}
}

/*
===================
CM_TempBoxModel

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
Capsules are handled differently though.
===================
*/
model_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, bool capsule )
{
	VectorCopy( mins, box_model.mins );
	VectorCopy( maxs, box_model.maxs );

	if( capsule ) return CAPSULE_MODEL_HANDLE;

	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -maxs[0];
	box_planes[2].dist = mins[0];
	box_planes[3].dist = -mins[0];
	box_planes[4].dist = maxs[1];
	box_planes[5].dist = -maxs[1];
	box_planes[6].dist = mins[1];
	box_planes[7].dist = -mins[1];
	box_planes[8].dist = maxs[2];
	box_planes[9].dist = -maxs[2];
	box_planes[10].dist = mins[2];
	box_planes[11].dist = -mins[2];

	// first side
	VectorSet(box_brush->edges[0].p0, mins[0], mins[1], mins[2]);
	VectorSet(box_brush->edges[0].p1, mins[0], maxs[1], mins[2]);
	VectorSet(box_brush->edges[1].p0, mins[0], maxs[1], mins[2]);
	VectorSet(box_brush->edges[1].p1, mins[0], maxs[1], maxs[2]);
	VectorSet(box_brush->edges[2].p0, mins[0], maxs[1], maxs[2]);
	VectorSet(box_brush->edges[2].p1, mins[0], mins[1], maxs[2]);
	VectorSet(box_brush->edges[3].p0, mins[0], mins[1], maxs[2]);
	VectorSet(box_brush->edges[3].p1, mins[0], mins[1], mins[2]);

	// opposite side
	VectorSet(box_brush->edges[4].p0, maxs[0], mins[1], mins[2]);
	VectorSet(box_brush->edges[4].p1, maxs[0], maxs[1], mins[2]);
	VectorSet(box_brush->edges[5].p0, maxs[0], maxs[1], mins[2]);
	VectorSet(box_brush->edges[5].p1, maxs[0], maxs[1], maxs[2]);
	VectorSet(box_brush->edges[6].p0, maxs[0], maxs[1], maxs[2]);
	VectorSet(box_brush->edges[6].p1, maxs[0], mins[1], maxs[2]);
	VectorSet(box_brush->edges[7].p0, maxs[0], mins[1], maxs[2]);
	VectorSet(box_brush->edges[7].p1, maxs[0], mins[1], mins[2]);

	// connecting edges
	VectorSet(box_brush->edges[8].p0, mins[0], mins[1], mins[2]);
	VectorSet(box_brush->edges[8].p1, maxs[0], mins[1], mins[2]);
	VectorSet(box_brush->edges[9].p0, mins[0], maxs[1], mins[2]);
	VectorSet(box_brush->edges[9].p1, maxs[0], maxs[1], mins[2]);
	VectorSet(box_brush->edges[10].p0, mins[0], maxs[1], maxs[2]);
	VectorSet(box_brush->edges[10].p1, maxs[0], maxs[1], maxs[2]);
	VectorSet(box_brush->edges[11].p0, mins[0], mins[1], maxs[2]);
	VectorSet(box_brush->edges[11].p1, maxs[0], mins[1], maxs[2]);

	VectorCopy( mins, box_brush->bounds[0] );
	VectorCopy( maxs, box_brush->bounds[1] );

	return BOX_MODEL_HANDLE;
}

/*
================
CM_FreeModel
================
*/
void CM_FreeModel( cmodel_t *mod )
{
	if( !mod || !mod->mempool ) return;

	Mem_FreePool( &mod->mempool );
	Mem_Set( mod, 0, sizeof( *mod ));
}

const void *CM_VisData( void ) { return cm.pvs; }
int CM_NumShaders( void ) { return cm.numshaders; }
int CM_NumInlineModels( void ) { return cm.nummodels; }
script_t *CM_EntityScript( void ) { return cm.entityscript; }
const char *CM_ShaderName( int index ){ return cm.shaders[bound( 0, index, cm.numshaders-1 )].name; }

/*
===============================================================================

			MAP LOADING

===============================================================================
*/
void BSP_LoadModels( lump_t *l )
{
	dmodel_t	*in;
	cmodel_t	*out;
	int	i, j, count;
	int	*indexes;
	
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadModels: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	if( count < 1 ) Host_Error( "Map %s without models\n", cm.name );
	if( count > MAX_MODELS ) Host_Error( "Map %s has too many models\n", cm.name );
	cm.models = (cmodel_t *)Mem_Alloc( cms.mempool, count * sizeof( cmodel_t ));
	cm.nummodels = count;

	for( i = 0; i < count; i++, in++ )
	{
		out = &cm.models[i];
	
		for( j = 0; j < 3; j++ )
		{
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat( in->mins[j] ) - 1;
			out->maxs[j] = LittleFloat( in->maxs[j] ) + 1;
		}

		if( i == 0 )
		{
			out->type = mod_world;
			continue; // world model doesn't need other info
		}

		out->type = mod_brush;

		// make a "leaf" just to hold the model's brushes and surfaces
		out->leaf.numleafbrushes = LittleLong( in->numbrushes );
		indexes = Mem_Alloc( cms.mempool, out->leaf.numleafbrushes * 4 );
		out->leaf.firstleafbrush = indexes - cm.leafbrushes;

		for( j = 0; j < out->leaf.numleafbrushes; j++ )
			indexes[j] = LittleLong( in->firstbrush ) + j;

		out->leaf.numleafsurfaces = LittleLong( in->numsurfaces );
		indexes = Mem_Alloc( cms.mempool, out->leaf.numleafsurfaces * 4 );
		out->leaf.firstleafsurface = indexes - cm.leafsurfaces;

		for( j = 0; j < out->leaf.numleafsurfaces; j++ )
			indexes[j] = LittleLong( in->firstsurface ) + j;
	}
}

/*
=================
BSP_LoadShaders
=================
*/
void BSP_LoadShaders( lump_t *l )
{
	dshader_t		*in, *out;
	int 		i;

	in = ( void * )(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadShaders: funny lump size\n" );
	cm.numshaders = l->filelen / sizeof( *in );
	cm.shaders = out = (dshader_t *)Mem_Alloc( cms.mempool, cm.numshaders * sizeof( *out ));

	for( i = 0; i < cm.numshaders; i++, in++, out++)
	{
		FS_FileBase( in->name, out->name );
		out->contentFlags = LittleLong( in->contentFlags );
		out->surfaceFlags = LittleLong( in->surfaceFlags );
	}
}

/*
=================
BSP_LoadNodes
=================
*/
void BSP_LoadNodes( lump_t *l )
{
	dnode_t	*in;
	cnode_t	*out;
	int	i, j;
	
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadNodes: funny lump size\n" );
	cm.numnodes = l->filelen / sizeof( *in );

	if( cm.numnodes < 1 ) Host_Error( "Map %s has no nodes\n", cm.name );
	out = cm.nodes = (cnode_t *)Mem_Alloc( cms.mempool, cm.numnodes * sizeof( *out ));

	for( i = 0; i < cm.numnodes; i++, out++, in++ )
	{
		out->plane = cm.planes + LittleLong( in->planenum );
		for( j = 0; j < 2; j++ )
			out->children[j] = LittleLong( in->children[j] );
	}

}

/*
=================
CM_BoundBrush

sides must be valid
=================
*/
void CM_BoundBrush( cbrush_t *b )
{
	b->bounds[0][0] = -b->sides[0].plane->dist;
	b->bounds[1][0] = b->sides[1].plane->dist;
	b->bounds[0][1] = -b->sides[2].plane->dist;
	b->bounds[1][1] = b->sides[3].plane->dist;
	b->bounds[0][2] = -b->sides[4].plane->dist;
	b->bounds[1][2] = b->sides[5].plane->dist;
}

/*
=================
BSP_LoadBrushes
=================
*/
void BSP_LoadBrushes( lump_t *l )
{
	dbrush_t		*in;
	cbrush_t		*out;
	int		i, count;
	
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadBrushes: funny lump size\n" );
	count = l->filelen / sizeof( *in );
	out = cm.brushes = (cbrush_t *)Mem_Alloc( cms.mempool, (count + BOX_BRUSHES) * sizeof( *out ));
	cm.numbrushes = count;

	for( i = 0; i < count; i++, out++, in++ )
	{
		out->sides = cm.brushsides + LittleLong( in->firstside );
		out->numsides = LittleLong( in->numsides );

		out->shadernum = LittleLong( in->shadernum );
		if( out->shadernum < 0 || out->shadernum >= cm.numshaders )
			Host_Error( "BSP_LoadBrushes: bad shadernum: #%i", out->shadernum );
		out->contents = cm.shaders[out->shadernum].contentFlags;

		CM_BoundBrush( out );
	}
}

/*
=================
BSP_LoadLeafSurffaces
=================
*/
void BSP_LoadLeafSurfaces( lump_t *l )
{
	dleafface_t	*in, *out;
	int		i, count;
	
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadLeafFaces: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	out = cm.leafsurfaces = (dword *)Mem_Alloc( cms.mempool, count * sizeof( *out ));
	cm.numleafsurfaces = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		*out = LittleLong( *in );
	}
}

/*
=================
BSP_LoadLeafBrushes
=================
*/
void BSP_LoadLeafBrushes( lump_t *l )
{
	dleafbrush_t	*in, *out;
	int		i, count;
	
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadLeafBrushes: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	if( count < 1 ) Host_Error( "Map %s with no leaf brushes\n", cm.name );
	out = cm.leafbrushes = (dword *)Mem_Alloc( cms.mempool, (count + BOX_BRUSHES) * sizeof( *out ));
	cm.numleafbrushes = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		*out = LittleLong( *in );
	}
}


/*
=================
BSP_LoadLeafs
=================
*/
void BSP_LoadLeafs( lump_t *l )
{
	dleaf_t 	*in;
	cleaf_t	*out;
	int	i, count;
		
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadLeafs: funny lump size\n" );

	count = l->filelen / sizeof( *in );
	if( count < 1 ) Host_Error( "Map %s with no leafs\n", cm.name );
	out = cm.leafs = (cleaf_t *)Mem_Alloc( cms.mempool, (count + BOX_LEAFS) * sizeof(*out));
	cm.numleafs = count;
	cm.numareas = 1;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->cluster = LittleLong( in->cluster );
		out->area = LittleLong( in->area ) + 1;
		out->firstleafbrush = LittleLong( in->firstleafbrush );
		out->numleafbrushes = LittleLong( in->numleafbrushes );
		out->firstleafsurface = LittleLong( in->firstleafsurface );
		out->numleafsurfaces = LittleLong( in->numleafsurfaces );

		if( out->cluster >= cm.numclusters )
			cm.numclusters = out->cluster + 1;
		if( out->area >= cm.numareas )
			cm.numareas = out->area + 1;
	}

	cm.areas = Mem_Alloc( cms.mempool, cm.numareas * sizeof( *cm.areas ));
}

/*
=================
BSP_LoadPlanes
=================
*/
void BSP_LoadPlanes( lump_t *l )
{
	dplane_t	*in;
	cplane_t	*out;
	int	i, count;
	
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadPlanes: funny lump size\n" );

	count = l->filelen / sizeof( *in );
	if( count < 1 ) Host_Error( "Map %s with no planes\n", cm.name );
	out = cm.planes = (cplane_t *)Mem_Alloc( cms.mempool, (count + BOX_PLANES) * sizeof( *out ));
	cm.numplanes = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		int	j, bits = 0;

		for( j = 0; j < 3; j++ )
		{
			out->normal[j] = LittleFloat(in->normal[j]);
			if( out->normal[j] < 0.0f ) bits |= (1<<j);
		}

		out->dist = LittleFloat( in->dist );
		out->type = PlaneTypeForNormal( out->normal );
		out->signbits = bits;
	}
}

/*
=================
BSP_LoadBrushSides
=================
*/
void IBSP_LoadBrushSides( lump_t *l )
{
	dbrushsideq_t 	*in;
	cbrushside_t	*out;
	int		i, count;

	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadBrushSides: funny lump size\n" );

	count = l->filelen / sizeof( *in );
	out = cm.brushsides = (cbrushside_t *)Mem_Alloc( cms.mempool, (count + BOX_SIDES) * sizeof( *out ));
	cm.numbrushsides = count;

	for( i = 0; i < count; i++, in++, out++)
	{
		out->planenum = LittleLong( in->planenum );
		out->plane = cm.planes + out->planenum;
		out->shadernum = LittleLong( in->shadernum );

		if( out->shadernum < 0 || out->shadernum >= cm.numshaders )
			Host_Error( "BSP_LoadBrushSides: bad shadernum: #%i\n", out->shadernum );
		out->surfaceFlags = cm.shaders[out->shadernum].surfaceFlags;
	}
}

static void RBSP_LoadBrushSides( lump_t *l )
{
	dbrushsider_t 	*in;
	cbrushside_t	*out;
	int		i, count;

	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadBrushSides: funny lump size\n" );

	count = l->filelen / sizeof( *in );
	out = cm.brushsides = (cbrushside_t *)Mem_Alloc( cms.mempool, (count + BOX_SIDES) * sizeof( *out ));
	cm.numbrushsides = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		out->planenum = LittleLong( in->planenum );
		out->plane = cm.planes + out->planenum;
		out->shadernum = LittleLong( in->shadernum );

		if( out->shadernum < 0 || out->shadernum >= cm.numshaders )
			Host_Error( "BSP_LoadBrushSides: bad shadernum: #%i\n", out->shadernum );
		out->surfaceFlags = cm.shaders[out->shadernum].surfaceFlags;
	}
}

/*
=================
BSP_LoadVisibility
=================
*/
void BSP_LoadVisibility( lump_t *l )
{
	byte	*visbase;

	cm.visdata_size = l->filelen;
	if( !cm.visdata_size ) return;

	visbase = Mem_Alloc( cms.mempool, cm.visdata_size );
	Mem_Copy( visbase, (void *)(cms.base + l->fileofs), cm.visdata_size );

	cm.pvs = (dvis_t *)visbase;
	cm.pvs->numclusters = LittleLong( cm.pvs->numclusters );
	cm.pvs->rowsize = LittleLong( cm.pvs->rowsize );

	if( cm.numclusters != cm.pvs->numclusters )
		Host_Error( "BSP_LoadVisibility: mismatch vis and leaf clusters (%i should be %i)\n", cm.pvs->numclusters, cm.numclusters );
}

/*
=================
BSP_BrushEdgesAreTheSame
=================
*/
static bool BSP_BrushEdgesAreTheSame( const vec3_t p0, const vec3_t p1, const vec3_t q0, const vec3_t q1 )
{
	if( VectorCompareEpsilon( p0, q0, ON_EPSILON ) && VectorCompareEpsilon( p1, q1, ON_EPSILON ))
		return true;

	if( VectorCompareEpsilon( p1, q0, ON_EPSILON ) && VectorCompareEpsilon( p0, q1, ON_EPSILON ))
		return true;
	return false;
}

/*
=================
BSP_AddEdgeToBrush
=================
*/
static bool BSP_AddEdgeToBrush( const vec3_t p0, const vec3_t p1, cbrushedge_t *edges, int *numEdges )
{
	int	i;

	if( !edges || !numEdges )
		return false;

	for( i = 0; i < *numEdges; i++ )
	{
		if( BSP_BrushEdgesAreTheSame( p0, p1, edges[i].p0, edges[i].p1 ))
			return false;
	}

	VectorCopy( p0, edges[*numEdges].p0 );
	VectorCopy( p1, edges[*numEdges].p1 );
	(*numEdges)++;

	return true;
}

/*
=================
BSP_CreateBrushSideWindings
=================
*/
static void BSP_CreateBrushSideWindings( void )
{
	cwinding_t	*w;
	cplane_t		*plane;
	cbrush_t		*brush;
	cbrushside_t	*side, *chopSide;
	cbrushedge_t	*tempEdges;
	int		numEdges;
	int		edgesAlloc;
	int		totalEdgesAlloc = 0;
	int		totalEdges = 0;
	int		i, j, k;

	for( i = 0; i < cm.numbrushes; i++ )
	{
		brush = &cm.brushes[i];
		numEdges = 0;

		// walk the list of brush sides
		for( j = 0; j < brush->numsides; j++ )
		{
			// get side and plane
			side = &brush->sides[j];
			plane = side->plane;

			w = CM_BaseWindingForPlane( plane->normal, plane->dist );

			// walk the list of brush sides
			for( k = 0; k < brush->numsides && w != NULL; k++ )
			{
				chopSide = &brush->sides[k];

				if( chopSide == side )
					continue;

				if( chopSide->planenum == ( side->planenum ^ 1 ))
					continue;	// back side clipaway

				plane = &cm.planes[chopSide->planenum ^ 1];
				CM_ChopWindingInPlace( &w, plane->normal, plane->dist, 0 );
			}

			if( w ) numEdges += w->numpoints;

			// set side winding
			side->winding = w;
		}

		// Allocate a temporary buffer of the maximal size
		tempEdges = (cbrushedge_t *)Mem_Alloc( cms.mempool, sizeof( cbrushedge_t ) * numEdges );
		brush->numedges = 0;

		// compose the points into edges
		for( j = 0; j < brush->numsides; j++ )
		{
			side = &brush->sides[j];

			if( side->winding )
			{
				for( k = 0; k < side->winding->numpoints - 1; k++ )
				{
					if( brush->numedges == numEdges )
						Host_Error( "Insufficient memory allocated for collision map edges\n" );

					BSP_AddEdgeToBrush( side->winding->p[k], side->winding->p[k+1], tempEdges, &brush->numedges );
				}

				CM_FreeWinding( side->winding );
				side->winding = NULL;
			}
		}

		// Allocate a buffer of the actual size
		edgesAlloc = sizeof(cbrushedge_t) * brush->numedges;
		totalEdgesAlloc += edgesAlloc;
		brush->edges = (cbrushedge_t *)Mem_Alloc( cms.mempool, edgesAlloc );

		// Copy temporary buffer to permanent buffer
		Mem_Copy( brush->edges, tempEdges, edgesAlloc );

		// free temporary buffer
		Mem_Free( tempEdges );

		totalEdges += brush->numedges;
	}

	MsgDev( D_INFO, "CreateBrushWindings: allocated %s for %d collision map edges...\n", memprint( totalEdgesAlloc ), totalEdges );
}

/*
=================
BSP_LoadEntityString
=================
*/
void BSP_LoadEntityString( lump_t *l )
{
	byte	*in;

	in = (void *)(cms.base + l->fileofs);
	cm.entityscript = Com_OpenScript( LUMP_ENTITIES, in, l->filelen );
}

/*
=================
BSP_LoadSurfaces
=================
*/
void IBSP_LoadSurfaces( lump_t *surfs, lump_t *verts, lump_t *elems, bool xreal_bsp )
{
	dvertexq_t	*dvi, *dvi_p;
	dvertexx_t	*dvx, *dvx_p;
	dsurfaceq_t	*in;
	int		count;
	csurface_t	*surface;
	int		i, j, numVertexes;
	static vec3_t	vertexes[SHADER_MAX_VERTEXES];
	static int	indexes[SHADER_MAX_INDEXES];
	int		width, height;
	int		shaderNum;
	int		numIndexes;
	int		*index, *index_p;

	in = (void *)(cms.base + surfs->fileofs);
	if( surfs->filelen % sizeof( *in ))
		Host_Error( "BSP_LoadSurfaces: funny lump size\n" );
	cm.numsurfaces = count = surfs->filelen / sizeof( *in );
	cm.surfaces = Mem_Alloc( cms.mempool, cm.numsurfaces * sizeof( csurface_t ));

	if( xreal_bsp )
	{
		dvx = (void *)(cms.base + verts->fileofs);
		if( verts->filelen % sizeof( *dvx ))
			Host_Error( "BSP_LoadSurfaces: funny lump size\n" );
          }
          else
          {
		dvi = (void *)(cms.base + verts->fileofs);
		if( verts->filelen % sizeof( *dvi ))
			Host_Error( "BSP_LoadSurfaces: funny lump size\n" );
          }

	index = (void *)(cms.base + elems->fileofs);
	if( elems->filelen % sizeof( *index ))
		Host_Error( "BSP_LoadSurfaces: funny lump size\n" );

	// scan through all the surfaces, but only load patches,
	// not planar faces
	for( i = 0; i < count; i++, in++ )
	{
		shaderNum = LittleLong( in->shadernum );
		if( cm.shaders[shaderNum].surfaceFlags & SURF_NONSOLID )
			continue;
			
		if( LittleLong( in->facetype ) == MST_PATCH )
		{
			cm.surfaces[i] = surface = Mem_Alloc( cms.mempool, sizeof( *surface ));
			surface->type = MST_PATCH;

			// load the full drawverts onto the stack
			width = LittleLong( in->patch_cp[0] );
			height = LittleLong(in->patch_cp[1] );
			numVertexes = width * height;

			if( numVertexes > MAX_PATCH_VERTS )
				Host_Error( "BSP_LoadSurfaces: MAX_PATCH_VERTS limit exceeded\n" );

			if( xreal_bsp )
			{
				dvx_p = dvx + LittleLong( in->firstvert );
				for( j = 0; j < numVertexes; j++, dvx_p++ )
				{
					vertexes[j][0] = LittleFloat( dvx_p->point[0] );
					vertexes[j][1] = LittleFloat( dvx_p->point[1] );
					vertexes[j][2] = LittleFloat( dvx_p->point[2] );
				}
			}
			else
			{
				dvi_p = dvi + LittleLong( in->firstvert );
				for( j = 0; j < numVertexes; j++, dvi_p++ )
				{
					vertexes[j][0] = LittleFloat( dvi_p->point[0] );
					vertexes[j][1] = LittleFloat( dvi_p->point[1] );
					vertexes[j][2] = LittleFloat( dvi_p->point[2] );
				}
                              }
                              
			shaderNum = LittleLong( in->shadernum );
			surface->contents = cm.shaders[shaderNum].contentFlags;
			surface->surfaceFlags = cm.shaders[shaderNum].surfaceFlags;
			surface->name = cm.shaders[shaderNum].name;

			// create the internal facet structure
			surface->sc = CM_GeneratePatchCollide( width, height, vertexes );
		}
		else if( LittleLong( in->facetype ) == MST_TRISURF && cm_triangles->integer )
		{
			cm.surfaces[i] = surface = Mem_Alloc( cms.mempool, sizeof( *surface ));
			surface->type = MST_TRISURF;

			// load the full drawverts onto the stack
			numVertexes = LittleLong( in->numverts );
			if( numVertexes > SHADER_MAX_VERTEXES )
				Host_Error( "BSP_LoadSurfaces: SHADER_MAX_VERTEXES limit exceeded\n" );

			if( xreal_bsp )
			{
				dvx_p = dvx + LittleLong( in->firstvert );
				for( j = 0; j < numVertexes; j++, dvx_p++ )
				{
					vertexes[j][0] = LittleFloat( dvx_p->point[0] );
					vertexes[j][1] = LittleFloat( dvx_p->point[1] );
					vertexes[j][2] = LittleFloat( dvx_p->point[2] );
				}
			}
			else
			{
				dvi_p = dvi + LittleLong( in->firstvert );
				for( j = 0; j < numVertexes; j++, dvi_p++ )
				{
					vertexes[j][0] = LittleFloat( dvi_p->point[0] );
					vertexes[j][1] = LittleFloat( dvi_p->point[1] );
					vertexes[j][2] = LittleFloat( dvi_p->point[2] );
				}
                              }

			numIndexes = LittleLong( in->numelems );
			if( numIndexes > SHADER_MAX_INDEXES )
				Host_Error( "BSP_LoadSurfaces: SHADER_MAX_INDEXES limit exceeded\n" );

			index_p = index + LittleLong( in->firstelem );
			for( j = 0; j < numIndexes; j++, index_p++ )
			{
				indexes[j] = LittleLong( *index_p );

				if( indexes[j] < 0 || indexes[j] >= numVertexes )
					Host_Error( "BSP_LoadSurfaces: bad index in trisoup surface\n" );
			}

			shaderNum = LittleLong( in->shadernum );
			surface->contents = cm.shaders[shaderNum].contentFlags;
			surface->surfaceFlags = cm.shaders[shaderNum].surfaceFlags;
			surface->name = cm.shaders[shaderNum].name;

			// create the internal facet structure
			surface->sc = CM_GenerateTriangleSoupCollide( numVertexes, vertexes, numIndexes, indexes );
		}
	}
}

void RBSP_LoadSurfaces( lump_t *surfs, lump_t *verts, lump_t *elems )
{
	dvertexr_t	*dv, *dv_p;
	dsurfacer_t	*in;
	int		count;
	csurface_t	*surface;
	int		i, j, numVertexes;
	static vec3_t	vertexes[SHADER_MAX_VERTEXES];
	static int	indexes[SHADER_MAX_INDEXES];
	int		width, height;
	int		shaderNum;
	int		numIndexes;
	int		*index, *index_p;

	in = (void *)(cms.base + surfs->fileofs);
	if( surfs->filelen % sizeof( *in ))
		Host_Error( "BSP_LoadSurfaces: funny lump size\n" );
	cm.numsurfaces = count = surfs->filelen / sizeof( *in );
	cm.surfaces = Mem_Alloc( cms.mempool, cm.numsurfaces * sizeof( csurface_t ));

	dv = (void *)(cms.base + verts->fileofs);
	if( verts->filelen % sizeof( *dv ))
		Host_Error( "BSP_LoadSurfaces: funny lump size\n" );

	index = (void *)(cms.base + elems->fileofs);
	if( elems->filelen % sizeof( *index ))
		Host_Error( "BSP_LoadSurfaces: funny lump size\n" );

	// scan through all the surfaces, but only load patches,
	// not planar faces
	for( i = 0; i < count; i++, in++ )
	{
		shaderNum = LittleLong( in->shadernum );
		if( cm.shaders[shaderNum].surfaceFlags & SURF_NONSOLID )
			continue;

		if( LittleLong( in->facetype ) == MST_PATCH )
		{
			cm.surfaces[i] = surface = Mem_Alloc( cms.mempool, sizeof( *surface ));
			surface->type = MST_PATCH;

			// load the full drawverts onto the stack
			width = LittleLong( in->patch_cp[0] );
			height = LittleLong(in->patch_cp[1] );
			numVertexes = width * height;

			if( numVertexes > MAX_PATCH_VERTS )
				Host_Error( "BSP_LoadSurfaces: MAX_PATCH_VERTS limit exceeded\n" );

			dv_p = dv + LittleLong( in->firstvert );
			for( j = 0; j < numVertexes; j++, dv_p++ )
			{
				vertexes[j][0] = LittleFloat( dv_p->point[0] );
				vertexes[j][1] = LittleFloat( dv_p->point[1] );
				vertexes[j][2] = LittleFloat( dv_p->point[2] );
			}
                              
			shaderNum = LittleLong( in->shadernum );
			surface->contents = cm.shaders[shaderNum].contentFlags;
			surface->surfaceFlags = cm.shaders[shaderNum].surfaceFlags;
			surface->name = cm.shaders[shaderNum].name;

			// create the internal facet structure
			surface->sc = CM_GeneratePatchCollide( width, height, vertexes );
		}
		else if( LittleLong( in->facetype ) == MST_TRISURF && cm_triangles->integer )
		{
			cm.surfaces[i] = surface = Mem_Alloc( cms.mempool, sizeof( *surface ));
			surface->type = MST_TRISURF;

			// load the full drawverts onto the stack
			numVertexes = LittleLong( in->numverts );
			if( numVertexes > SHADER_MAX_VERTEXES )
				Host_Error( "BSP_LoadSurfaces: SHADER_MAX_VERTEXES limit exceeded\n" );

			dv_p = dv + LittleLong( in->firstvert );
			for( j = 0; j < numVertexes; j++, dv_p++ )
			{
				vertexes[j][0] = LittleFloat( dv_p->point[0] );
				vertexes[j][1] = LittleFloat( dv_p->point[1] );
				vertexes[j][2] = LittleFloat( dv_p->point[2] );
			}

			numIndexes = LittleLong( in->numelems );
			if( numIndexes > SHADER_MAX_INDEXES )
				Host_Error( "BSP_LoadSurfaces: SHADER_MAX_INDEXES limit exceeded\n" );

			index_p = index + LittleLong( in->firstelem );
			for( j = 0; j < numIndexes; j++, index_p++ )
			{
				indexes[j] = LittleLong( *index_p );

				if( indexes[j] < 0 || indexes[j] >= numVertexes )
					Host_Error( "BSP_LoadSurfaces: bad index in trisoup surface\n" );
			}

			shaderNum = LittleLong( in->shadernum );
			surface->contents = cm.shaders[shaderNum].contentFlags;
			surface->surfaceFlags = cm.shaders[shaderNum].surfaceFlags;
			surface->name = cm.shaders[shaderNum].name;

			// create the internal facet structure
			surface->sc = CM_GenerateTriangleSoupCollide( numVertexes, vertexes, numIndexes, indexes );
		}
	}
}

void CM_FreeWorld( void )
{
	// free old stuff
	if( cms.loaded )
	{
		if( cm.entityscript )
			Com_CloseScript( cm.entityscript ); 
		Mem_EmptyPool( cms.mempool );
		cms.loaded = false;
	}

	CM_ClearLevelPatches ();
	Mem_Set( &cm, 0, sizeof( cm ));
}

void CM_FreeModels( void )
{
	int	i;

	for( i = 0; i < cms.nummodels; i++ )
		CM_FreeModel( &cms.models[i] );
}

/*
==================
CM_BeginRegistration

Loads in the map and all submodels
==================
*/
void CM_BeginRegistration( const char *name, bool clientload, uint *checksum )
{
	uint		*buf;
	dheader_t		*hdr;
	size_t		length;
	bool		xreal_bsp;
	bool		raven_bsp;

	if( !com.strlen( name ))
	{
		CM_FreeWorld(); // release old map
		// cinematic servers won't have anything at all
		cm.numleafs = cm.numclusters = cm.numareas = 1;
		*checksum = 0;
		return;
	}

	if( !com.strcmp( cm.name, name ) && cms.loaded )
	{
		// singleplayer mode: server already loading map
		*checksum = cm.checksum;
		if( !clientload )
		{
			// rebuild portals for server ...
			CM_FloodAreaConnections();

			// ... and reset entity script
			Com_ResetScript( cm.entityscript );
		}
		// still have the right version
		return;
	}

	CM_FreeWorld();		// release old map
	cms.registration_sequence++;	// all models are invalid

	// load the newmap
	buf = (uint *)FS_LoadFile( name, &length );
	if( !buf ) Host_Error( "Couldn't load %s\n", name );

	hdr = (dheader_t *)buf;
	if( !hdr ) Host_Error( "CM_LoadMap: %s couldn't read header\n", name ); 

	*checksum = cm.checksum = LittleLong( Com_BlockChecksum( buf, length ));
	hdr = (dheader_t *)buf;
	SwapBlock(( int *)hdr, sizeof( dheader_t ));	
	cms.base = (byte *)buf;

	xreal_bsp = false;
	raven_bsp = false;

	// call the apropriate loader
	switch( LittleLong(*(uint *)buf ))
	{
	case QFBSPMODHEADER:
	case RBBSPMODHEADER:
		if( hdr->version == RFIDBSP_VERSION )
			raven_bsp = true;
		else Host_Error( "CM_LoadMap: %s has wrong version number (%i should be %i)\n", name, hdr->version, RFIDBSP_VERSION );	
		break;
	case XRBSPMODHEADER:
		if( hdr->version == XRIDBSP_VERSION )
			xreal_bsp = true;
		else Host_Error( "CM_LoadMap: %s has wrong version number (%i should be %i)\n", name, hdr->version, XRIDBSP_VERSION );	
		break;
	case IDBSPMODHEADER:
		if( hdr->version == Q3IDBSP_VERSION || hdr->version == RTCWBSP_VERSION || hdr->version == IGIDBSP_VERSION )
			xreal_bsp = false;
		else Host_Error( "CM_LoadMap: %s has wrong version number (%i should be %i)\n", name, hdr->version, Q3IDBSP_VERSION );	
		break;
	default:
		Host_Error( "CM_LoadMap: %s is not a IBSP, RBSP, XBSP or FBSP file\n", name );
		break;
	}

	com.strncpy( cm.name, name, MAX_STRING );

	// load into heap
	BSP_LoadEntityString( &hdr->lumps[LUMP_ENTITIES] );
	BSP_LoadShaders( &hdr->lumps[LUMP_SHADERS] );
	BSP_LoadLeafs( &hdr->lumps[LUMP_LEAFS] );
	BSP_LoadLeafBrushes( &hdr->lumps[LUMP_LEAFBRUSHES] );
	BSP_LoadLeafSurfaces( &hdr->lumps[LUMP_LEAFSURFACES] );
	BSP_LoadPlanes( &hdr->lumps[LUMP_PLANES] );
	if( raven_bsp || hdr->version == IGIDBSP_VERSION )
		RBSP_LoadBrushSides( &hdr->lumps[LUMP_BRUSHSIDES] );
	else IBSP_LoadBrushSides( &hdr->lumps[LUMP_BRUSHSIDES] );
	BSP_LoadBrushes( &hdr->lumps[LUMP_BRUSHES] );
	BSP_LoadModels( &hdr->lumps[LUMP_MODELS] );
	BSP_LoadNodes( &hdr->lumps[LUMP_NODES] );
	BSP_LoadVisibility( &hdr->lumps[LUMP_VISIBILITY] );

	if( raven_bsp ) RBSP_LoadSurfaces( &hdr->lumps[LUMP_SURFACES], &hdr->lumps[LUMP_VERTEXES], &hdr->lumps[LUMP_ELEMENTS] );
	else IBSP_LoadSurfaces( &hdr->lumps[LUMP_SURFACES], &hdr->lumps[LUMP_VERTEXES], &hdr->lumps[LUMP_ELEMENTS], xreal_bsp );

	BSP_CreateBrushSideWindings ();
	CM_InitBoxHull ();
	CM_FloodAreaConnections ();
	CM_CalcPHS ();
	Mem_Free( buf );
	
	cms.loaded = true;	// all done
}

void CM_EndRegistration( void )
{
	cmodel_t	*mod;
	int	i;

	for( i = 0, mod = &cms.models[0]; i < cms.nummodels; i++, mod++ )
	{
		if( !mod->name[0] ) continue;
		if( mod->registration_sequence != cms.registration_sequence )
			CM_FreeModel( mod );
	}
}

int CM_LeafCluster( int leafnum )
{
	if( leafnum < 0 || leafnum >= cm.numleafs )
		Host_Error( "CM_LeafCluster: bad number %i >= %i\n", leafnum, cm.numleafs );
	return cm.leafs[leafnum].cluster;
}

int CM_LeafArea( int leafnum )
{
	if( leafnum < 0 || leafnum >= cm.numleafs )
		Host_Error( "CM_LeafArea: bad number %i >= %i\n", leafnum, cm.numleafs );
	return cm.leafs[leafnum].area;
}

/*
==================
CM_ClipHandleToModel
==================
*/
cmodel_t *CM_ClipHandleToModel( model_t handle )
{
	if( handle < 0 || handle > MAX_MODELS )
	{
		Host_Error( "CM_ClipHandleToModel: bad handle #%i\n", handle );
		return NULL;
	}

	if( handle == BOX_MODEL_HANDLE )
		return &box_model;
	if( handle == CAPSULE_MODEL_HANDLE )
		return &box_model;
	return sv_models[handle];
}
/*
===================
CM_Extradata
===================
*/
void *CM_Extradata( model_t handle )
{
	cmodel_t	*mod = CM_ClipHandleToModel( handle );

	if( mod && mod->type == mod_studio )
		return mod->extradata;
	return NULL;
}

/*
===================
CM_ModelFrames
===================
*/
void CM_ModelFrames( model_t handle, int *numFrames )
{
	cmodel_t	*mod = CM_ClipHandleToModel( handle );

	if( mod && mod->type == mod_sprite )
		if( numFrames ) *numFrames = mod->numframes;
	else if( numFrames ) *numFrames = 0;
}

/*
===================
CM_ModelBounds
===================
*/
void CM_ModelBounds( model_t handle, vec3_t mins, vec3_t maxs )
{
	cmodel_t	*cmod = CM_ClipHandleToModel( handle );

	if( cmod )
	{
		if( mins ) VectorCopy( cmod->mins, mins );
		if( maxs ) VectorCopy( cmod->maxs, maxs );
	}
	else
	{
		MsgDev( D_ERROR, "Mod_GetBounds: NULL model\n" );
		if( mins ) VectorSet( mins, -32, -32, -32 );
		if( maxs ) VectorSet( maxs,  32,  32,  32 );
	}
}

cmodel_t *CM_ModForName( const char *name )
{
	byte	*buf;
	int	i, size;
	cmodel_t	*mod;

	if( !name || !name[0] )
		return NULL;

	// check for worldmodel
	if( !com.strcmp( name, cm.name ))
		return &cm.models[0];

	// check for submodel
	if( name[0] == '*' ) 
	{
		i = com.atoi( name + 1 );
		if( i < 1 || !cms.loaded || i >= cm.nummodels )
		{
			MsgDev( D_ERROR, "CM_InlineModel: bad submodel number %d\n", i );
			return NULL;
		}
		return &cm.models[i];
	}

	// check for studio or sprite model
	for( i = 0; i < cms.nummodels; i++ )
          {
		mod = &cms.models[i];
		if( !mod->name[0] ) continue;
		if( !com.strcmp( name, mod->name ))
		{
			// prolonge registration
			mod->registration_sequence = cms.registration_sequence;
			return mod;
		}
	} 

	// find a free model slot spot
	for( i = 0, mod = cms.models; i < cms.nummodels; i++, mod++ )
		if( !mod->name[0] ) break; // free spot

	if( i == cms.nummodels )
	{
		if( cms.nummodels == MAX_MODELS )
			Host_Error( "Mod_ForName: MAX_MODELS limit exceeded\n" );
		cms.nummodels++;
	}

	buf = FS_LoadFile( name, &size );
	if( !buf )
	{
		MsgDev( D_ERROR, "CM_LoadModel: %s not found\n", name );
		Mem_Set( mod->name, 0, sizeof( mod->name ));
		return NULL;
	}


	MsgDev( D_NOTE, "CM_LoadModel: %s\n", name );
	com.strncpy( mod->name, name, sizeof( mod->name ));
	loadmodel = mod;

	// call the apropriate loader
	switch( LittleLong( *(uint *)buf ))
	{
	case IDSTUDIOHEADER:
		CM_StudioModel( buf, size );
		break;
	case IDSPRITEHEADER:
		CM_SpriteModel( buf, size );
		break;
	default:
		MsgDev( D_ERROR, "Mod_ForName: unknown fileid for %s\n", name );
		mod->name[0] = '\0';
		Mem_Free( buf );
		return NULL;
	}

	Mem_Free( buf ); 
	return mod;
}

bool CM_RegisterModel( const char *name, int index )
{
	cmodel_t	*mod;

	// this array used for acess to servermodels
	mod = CM_ModForName( name );
	sv_models[index] = mod;

	return (mod != NULL);
}