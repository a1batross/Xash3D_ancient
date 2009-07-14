//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_model.c - collision model
//=======================================================================

#include "cm_local.h"
#include "matrix_lib.h"
#include "const.h"

#define PATCHTESS_SAME_LODGROUP( a, b ) \
( \
(a).lodgroup[0] == (b).lodgroup[0] && \
(a).lodgroup[1] == (b).lodgroup[1] && \
(a).lodgroup[2] == (b).lodgroup[2] && \
(a).lodgroup[3] == (b).lodgroup[3] && \
(a).lodgroup[4] == (b).lodgroup[4] && \
(a).lodgroup[5] == (b).lodgroup[5] \
)

typedef struct patchtess_s
{
	patchinfo_t	info;

	// Auxiliary data used only by patch loading code in Mod_Q3BSP_LoadFaces
	int		surface_id;
	float		lodgroup[6];
	float		*originalvertex3f;
} patchtess_t;

clipmap_t		cm;
clipmap_static_t	cms;
studio_t		studio;

cvar_t *cm_noareas;
cmodel_t *loadmodel;
int registration_sequence = 0;

/*
===============================================================================

			CM COMMON UTILS

===============================================================================
*/
void CM_GetPoint( int index, vec3_t out )
{
	CM_ConvertPositionToMeters( out, cm.vertices[index] );
}

void CM_GetPoint2( int index, vec3_t out )
{
	CM_ConvertDimensionToMeters( out, cm.vertices[index] );
}

/*
=================
CM_BoundBrush
=================
*/
void CM_BoundBrush( cbrush_t *b )
{
	cbrushside_t	*sides;
	sides = &cm.brushsides[b->firstbrushside];

	b->bounds[0][0] = -sides[0].plane->dist;
	b->bounds[1][0] = sides[1].plane->dist;

	b->bounds[0][1] = -sides[2].plane->dist;
	b->bounds[1][1] = sides[3].plane->dist;

	b->bounds[0][2] = -sides[4].plane->dist;
	b->bounds[1][2] = sides[5].plane->dist;
}

void CM_SnapVertices( int numcomponents, int numvertices, float *vertices, float snap )
{
	int	i;
	double	isnap = 1.0 / snap;

	for( i = 0; i < numvertices * numcomponents; i++ )
		vertices[i] = floor( vertices[i] * isnap ) * snap;
}

int CM_RemoveDegenerateTriangles( int numtriangles, const int *inelement3i, int *outelement3i, const float *vertex3f )
{
	int	i, outtriangles;
	float	edgedir1[3], edgedir2[3], temp[3];

	// a degenerate triangle is one with no width (thickness, surface area)
	// these are characterized by having all 3 points colinear (along a line)
	// or having two points identical
	// the simplest check is to calculate the triangle's area

	for( i = 0, outtriangles = 0; i < numtriangles; i++, inelement3i += 3 )
	{
		// calculate first edge
		VectorSubtract( vertex3f + inelement3i[1] * 3, vertex3f + inelement3i[0] * 3, edgedir1 );
		VectorSubtract( vertex3f + inelement3i[2] * 3, vertex3f + inelement3i[0] * 3, edgedir2 );
		CrossProduct( edgedir1, edgedir2, temp );
		if( VectorLength2( temp ) < 0.001f )
			continue; // degenerate triangle (no area)

		// valid triangle (has area)
		VectorCopy( inelement3i, outelement3i );
		outelement3i += 3;
		outtriangles++;
	}
	return outtriangles;
}

/*
================
CM_FreeModel
================
*/
void CM_FreeModel( cmodel_t *mod )
{
	Mem_FreePool( &mod->mempool );
	Mem_Set( mod, 0, sizeof( *mod ));
	mod = NULL;
}

const void *CM_VisData( void ) { return cm.pvs; }
int CM_NumTextures( void ) { return cm.numshaders; }
int CM_NumClusters( void ) { return cm.numclusters; }
int CM_NumInlineModels( void ) { return cms.numbmodels; }
script_t *CM_EntityScript( void ) { return cm.entityscript; }
const char *CM_TexName( int index ) { return cm.shaders[index].name; }

/*
===============================================================================

					MAP LOADING

===============================================================================
*/
/*
=================
BSP_CreateMeshBuffer
=================
*/
void BSP_CreateMeshBuffer( int modelnum )
{
	csurface_t	*m_surface;
	int		d, i, j, k;
	int		flags;

	// ignore world or bsplib instance
	if( app_name == HOST_BSPLIB || modelnum >= cms.numbmodels )
		return;

	loadmodel = &cms.bmodels[modelnum];
	if( modelnum ) loadmodel->type = mod_brush;
	else loadmodel->type = mod_world; // level static geometry
	loadmodel->TraceBox = CM_TraceBmodel;
	loadmodel->PointContents = CM_PointContents;

	// because world loading collision tree from LUMP_COLLISION
	if( modelnum < 1 ) return;
	studio.m_pVerts = &studio.vertices[0]; // using studio vertex buffer for bmodels too
	studio.numverts = 0; // clear current count

	for( d = 0, i = loadmodel->firstface; d < loadmodel->numfaces; i++, d++ )
	{
		m_surface = cm.surfaces + i;
		flags = cm.shaders[m_surface->shadernum].flags;
		k = m_surface->firstvertex;

		// current implementation not supported meshes or patches
		if( m_surface->surfaceType != MST_PLANAR ) continue;

		// FIXME: sky is noclip for all physobjects
		if( flags & SURF_SKY ) continue;

		for( j = 0; j < m_surface->numvertices; j++ ) 
		{
			// because it's not a collision tree, just triangle mesh
			CM_GetPoint2( k+j, studio.m_pVerts[studio.numverts] );
			studio.numverts++;
		}
	}
	if( studio.numverts )
	{
		// grab vertices
		loadmodel->col[loadmodel->numbodies] = (cmesh_t *)Mem_Alloc( loadmodel->mempool, sizeof(*loadmodel->col[0]));
		loadmodel->col[loadmodel->numbodies]->verts = Mem_Alloc( loadmodel->mempool, studio.numverts * sizeof(vec3_t));
		Mem_Copy( loadmodel->col[loadmodel->numbodies]->verts, studio.m_pVerts, studio.numverts * sizeof(vec3_t));
		loadmodel->col[loadmodel->numbodies]->numverts = studio.numverts;
		loadmodel->numbodies++;
	}
}

void BSP_LoadModels( lump_t *l )
{
	dmodel_t	*in;
	cmodel_t	*out;
	int	i, j, n, c, count;

	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadModels: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	if( count < 1 ) Host_Error( "Map %s without models\n", cm.name );
	if( count > MAX_MODELS ) Host_Error( "Map %s has too many models\n", cm.name );
	cms.numbmodels = count;
	out = &cms.bmodels[0];

	for( i = 0; i < count; i++, in++, out++ )
	{
		for( j = 0; j < 3; j++ )
		{
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat(in->mins[j]) - 1;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
		}

		out->firstface = n = LittleLong( in->firstsurface );
		out->numfaces = c = LittleLong( in->numsurfaces );

		// skip other stuff, not using for building collision tree
		if( app_name == HOST_BSPLIB ) continue;

		// FIXME: calc bounding box right
		VectorCopy( out->mins, out->normalmins );
		VectorCopy( out->maxs, out->normalmaxs );
		VectorCopy( out->mins, out->rotatedmins );
		VectorCopy( out->maxs, out->rotatedmaxs );
		VectorCopy( out->mins, out->yawmins );
		VectorCopy( out->maxs, out->yawmaxs );

		if( n < 0 || n + c > cm.numsurfaces )
			Host_Error( "BSP_LoadModels: invalid face range %i : %i (%i faces)\n", n, n+c, cm.numsurfaces );
		out->firstbrush = n = LittleLong( in->firstbrush );
		out->numbrushes = c = LittleLong( in->numbrushes );
		if( n < 0 || n + c > cm.numbrushes )
			Host_Error( "BSP_LoadModels: invalid brush range %i : %i (%i brushes)\n", n, n+c, cm.numsurfaces );
		com.strncpy( out->name, va("*%i", i ), sizeof(out->name));
		out->mempool = Mem_AllocPool( va("^2%s", out->name )); // difference with render and cm pools
		BSP_CreateMeshBuffer( i ); // bsp physic
	}
}

/*
=================
BSP_LoadShaders
=================
*/
void BSP_LoadShaders( lump_t *l )
{
	dshader_t		*in;
	cshader_t		*out;
	int 		i;

	in = ( void * )(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadShaders: funny lump size\n" );
	cm.numshaders = l->filelen / sizeof( *in );
	cm.shaders = out = (cshader_t *)Mem_Alloc( cmappool, cm.numshaders * sizeof( *out ));

	for( i = 0; i < cm.numshaders; i++, in++, out++)
	{
		com.strncpy( out->name, in->name, MAX_SHADERPATH );
		out->contents = LittleLong( in->contentFlags );
		out->flags = LittleLong( in->surfaceFlags );
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
	int	i, j, n, count;
	
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadNodes: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	if( count < 1 ) Host_Error( "Map %s has no nodes\n", cm.name );
	out = cm.nodes = (cnode_t *)Mem_Alloc( cmappool, count * sizeof( *out ));
	cm.numnodes = count;

	for( i = 0; i < count; i++, out++, in++ )
	{
		out->parent = NULL;
		n = LittleLong( in->planenum );
		if( n < 0 || n >= cm.numplanes)
			Host_Error( "BSP_LoadNodes: invalid planenum %i (%i planes)\n", n, cm.numplanes );
		out->plane = cm.planes + n;
		for( j = 0; j < 2; j++)
		{
			n = LittleLong( in->children[j]);
			if( n >= 0 )
			{
				if( n >= cm.numnodes )
					Host_Error( "BSP_LoadNodes: invalid child node index %i (%i nodes)\n", n, cm.numnodes );
				out->children[j] = cm.nodes + n;
			}
			else
			{
				n = -1 - n;
				if( n >= cm.numleafs )
					Host_Error( "BSP_LoadNodes: invalid child leaf index %i (%i leafs)\n", n, cm.numleafs );
				out->children[j] = (cnode_t *)(cm.leafs + n);
			}
		}

		for( j = 0; j < 3; j++ )
		{
			// yes the mins/maxs are ints
			out->mins[j] = LittleLong( in->mins[j] ) - 1;
			out->maxs[j] = LittleLong( in->maxs[j] ) + 1;
		}
	}

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
	int		i, j, n, count, maxplanes = 0;
	cplanef_t		*planes = NULL;
	
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadBrushes: funny lump size\n" );
	count = l->filelen / sizeof( *in );
	out = cm.brushes = (cbrush_t *)Mem_Alloc( cmappool, (count + 1) * sizeof( *out ));
	cm.numbrushes = count;

	for( i = 0; i < count; i++, out++, in++ )
	{
		out->firstbrushside = LittleLong( in->firstside );
		out->numsides = LittleLong( in->numsides );
		n = LittleLong( in->shadernum );
		if( n < 0 || n >= cm.numshaders )
			Host_Error( "BSP_LoadBrushes: invalid shader index %i (brush %i)\n", n, i );
		out->contents = cm.shaders[n].contents;
		CM_BoundBrush( out );

		// make a list of mplane_t structs to construct a colbrush from
		if( maxplanes < out->numsides )
		{
			maxplanes = out->numsides;
			planes = Mem_Realloc( cmappool, planes, sizeof(cplanef_t) * maxplanes );
		}
		for( j = 0; j < out->numsides; j++ )
		{
			VectorCopy( cm.brushsides[out->firstbrushside + j].plane->normal, planes[j].normal );
			planes[j].dist = cm.brushsides[out->firstbrushside + j].plane->dist;
			planes[j].surfaceflags = cm.brushsides[out->firstbrushside + j].shader->flags;
			planes[j].surface = cm.brushsides[out->firstbrushside + j].surface;
		}
		// make the colbrush from the planes
		out->colbrushf = CM_CollisionNewBrushFromPlanes( cmappool, out->numsides, planes, out->contents );
	}
	if( planes ) Mem_Free( planes );
}

/*
=================
BSP_LoadLeafSurffaces
=================
*/
void BSP_LoadLeafSurfaces( lump_t *l )
{
	dleafface_t	*in, *out;
	int		i, n, count;
	
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadLeafFaces: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	out = cm.leafsurfaces = (dword *)Mem_Alloc( cmappool, count * sizeof( *out ));
	cm.numleafsurfaces = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		n = LittleLong( *in );
		if( n < 0 || n >= cm.numsurfaces )
			Host_Error( "BSP_LoadLeafFaces: invalid face index %i (%i faces)\n", n, cm.numsurfaces );
		*out = n;
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
	out = cm.leafbrushes = (dleafbrush_t *)Mem_Alloc( cmappool, count * sizeof( *out ));
	cm.numleafbrushes = count;
	for( i = 0; i < count; i++, in++, out++ ) *out = LittleLong( *in );
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
	int	i, j, n, c, count;
		
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadLeafs: funny lump size\n" );

	count = l->filelen / sizeof( *in );
	if( count < 1 ) Host_Error( "Map %s with no leafs\n", cm.name );
	out = cm.leafs = (cleaf_t *)Mem_Alloc( cmappool, count * sizeof(*out));
	cm.numclusters = 0;
	cm.numleafs = count;
	cm.numareas = 1;

	for( i = 0; i < count; i++, in++, out++)
	{
		out->parent = NULL;
		out->plane = NULL;
		out->cluster = LittleLong( in->cluster );
		out->area = LittleLong( in->area );

		if( out->cluster >= cm.numclusters )
			cm.numclusters = out->cluster + 1;
		if( out->area >= cm.numareas )
			cm.numareas = out->area + 1;
		for( j = 0; j < 3; j++ )
		{
			// yes the mins/maxs are ints
			out->mins[j] = LittleLong( in->mins[j] ) - 1;
			out->maxs[j] = LittleLong( in->maxs[j] ) + 1;
		}
		n = LittleLong( in->firstleafsurface );
		c = LittleLong( in->numleafsurfaces );
		if( n < 0 || n + c > cm.numleafsurfaces )
			Host_Error( "BSP_LoadLeafs: invalid leafsurface range %i : %i (%i leafsurfaces)\n", n, n + c, cm.numleafsurfaces );
		out->firstleafsurface = cm.leafsurfaces + n;
		out->numleafsurfaces = c;
		n = LittleLong( in->firstleafbrush );
		c = LittleLong( in->numleafbrushes );
		if( n < 0 || n + c > cm.numleafbrushes )
			Host_Error( "BSP_LoadLeafs: invalid leafbrush range %i : %i (%i leafbrushes)\n", n, n + c, cm.numleafbrushes );
		out->firstleafbrush = cm.leafbrushes + n;
		out->numleafbrushes = c;
	}

	cm.areas = Mem_Alloc( cmappool, cm.numareas * sizeof( *cm.areas ));
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
	int	i, j, count;
	
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadPlanes: funny lump size\n" );

	count = l->filelen / sizeof( *in );
	if( count < 1 ) Host_Error( "Map %s with no planes\n", cm.name );
	out = cm.planes = (cplane_t *)Mem_Alloc( cmappool, count * sizeof( *out ));
	cm.numplanes = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		for( j = 0; j < 3; j++ ) 
			out->normal[j] = LittleFloat(in->normal[j]);
		out->dist = LittleFloat( in->dist );
		PlaneClassify( out ); // automatic plane classify		
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
	int		i, j, num,count;

	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadBrushSides: funny lump size\n" );

	count = l->filelen / sizeof( *in );
	out = cm.brushsides = (cbrushside_t *)Mem_Alloc( cmappool, count * sizeof( *out ));
	cm.numbrushsides = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		num = LittleLong( in->planenum );
		out->plane = cm.planes + num;
		j = LittleLong( in->shadernum );
		j = bound( 0, j, cm.numshaders - 1 );
		out->shader = cm.shaders + j;
		out->surface = NULL;
	}
}

void RBSP_LoadBrushSides( lump_t *l )
{
	dbrushsider_t 	*in;
	cbrushside_t	*out;
	int		i, j, num,count;

	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadBrushSides: funny lump size\n" );

	count = l->filelen / sizeof( *in );
	out = cm.brushsides = (cbrushside_t *)Mem_Alloc( cmappool, count * sizeof( *out ));
	cm.numbrushsides = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		num = LittleLong( in->planenum );
		out->plane = cm.planes + num;
		j = LittleLong( in->shadernum );
		j = bound( 0, j, cm.numshaders - 1 );
		out->shader = cm.shaders + j;
		j = LittleLong( in->surfacenum );
		j = bound( 0, j, cm.numsurfaces - 1 );
		out->surface = cm.surfaces + j;
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

	visbase = Mem_Alloc( cmappool, cm.visdata_size );
	Mem_Copy( visbase, (void * )(cms.base + l->fileofs), cm.visdata_size );

	cm.pvs = (dvis_t *)visbase;
	cm.pvs->numclusters = LittleLong( cm.pvs->numclusters );
	cm.pvs->rowsize = LittleLong( cm.pvs->rowsize );

	if( cm.numclusters != cm.pvs->numclusters )
		Host_Error( "BSP_LoadVisibility: mismatch vis and leaf clusters (%i should be %i)\n", cm.pvs->numclusters, cm.numclusters );
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
BSP_LoadVerts
=================
*/
void IBSP_LoadVertexes( lump_t *l )
{
	dvertexq_t	*in;
	vec3_t		*out;
	int		i;

	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadVertexes: funny lump size\n" );

	cm.numverts = l->filelen / sizeof( *in );
	cm.vertices = out = Mem_Alloc( cmappool, cm.numverts * sizeof( *out ));

	for( i = 0; i < cm.numverts; i++, in++ )
	{
		out[i][0] = LittleFloat( in->point[0] );
		out[i][1] = LittleFloat( in->point[1] );
		out[i][2] = LittleFloat( in->point[2] );
	}
}

void RBSP_LoadVertexes( lump_t *l )
{
	dvertexr_t	*in;
	vec3_t		*out;
	int		i;

	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadVertexes: funny lump size\n" );

	cm.numverts = l->filelen / sizeof( *in );
	cm.vertices = out = Mem_Alloc( cmappool, cm.numverts * sizeof( *out ));

	for( i = 0; i < cm.numverts; i++, in++ )
	{
		out[i][0] = LittleFloat( in->point[0] );
		out[i][1] = LittleFloat( in->point[1] );
		out[i][2] = LittleFloat( in->point[2] );
	}
}

/*
=================
BSP_LoadEdges
=================
*/
void BSP_LoadIndexes( lump_t *l )
{
	int	*in, count;

	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadIndices: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	// just for get number of triangles
	cm.numtriangles = count / 3;
}

/*
=================
BSP_LoadSurfaces
=================
*/
void IBSP_LoadSurfaces( lump_t *l )
{
	dsurfaceq_t	*in, *oldin;
	csurface_t	*out, *oldout;
	int		i, j, type;
	int		firstvertex, numverts, firstelem, numtriangles, finalvertices, finaltriangles;
	int		patchsize[2], xtess, ytess, cxtess, cytess, finalwidth, finalheight;
	float		*originalvertex3f;
	patchtess_t	*patchtess = NULL;
	int		patchtesscount = 0;
	bool		again;
		
	in = oldin = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadSurfaces: funny lump size\n" );

	cm.numsurfaces = l->filelen / sizeof( *in );
	cm.surfaces = out = oldout = Mem_Alloc( cmappool, cm.numsurfaces * sizeof( *out ));	

	if( cm.numsurfaces > 0 )
		patchtess = (patchtess_t*) Mem_Alloc( cmappool, cm.numsurfaces * sizeof( *patchtess ));

	Msg( "BSP_LoadSurfaces\n" );

	for( i = 0; i < cm.numsurfaces; i++, in++, out++)
	{
		type = LittleLong( in->facetype );

		// check face type first
		switch( type )
		{
		case MST_PLANAR:
		case MST_PATCH:
		case MST_TRISURF:
		case MST_FLARE:
			break;
		default:
			MsgDev( D_ERROR, "BSP_LoadSurfaces: face #%i: unknown face type %i\n", i, type );
			continue;
		}

		out->surfaceType = type;
		out->shadernum = LittleLong( in->shadernum );

		firstvertex = LittleLong( in->firstvert );
		numverts = LittleLong( in->numverts );
		firstelem = LittleLong( in->firstelem );
		numtriangles = LittleLong( in->numelems ) / 3;

		if( numtriangles * 3 != LittleLong( in->numelems ))
		{
			MsgDev( D_ERROR, "BSP_LoadSurfaces: face #%i (texture \"%s\"): numelements %i is not a multiple of 3\n", i, CM_TexName( out->shadernum ), LittleLong( in->numelems ));
			continue;
		}
		if( firstvertex < 0 || firstvertex + numverts > cm.numverts )
		{
			MsgDev( D_ERROR, "BSP_LoadSurfaces: face #%i (texture \"%s\"): invalid vertex range %i : %i (%i vertices)\n", i, CM_TexName( out->shadernum ), firstvertex, firstvertex + numverts, cm.numverts );
			continue;
		}
		if( firstelem < 0 || firstelem + numtriangles * 3 > cm.numtriangles * 3 )
		{
			MsgDev( D_ERROR, "BSP_LoadSurfaces: face #%i (texture \"%s\"): invalid element range %i : %i (%i elements)\n", i, CM_TexName( out->shadernum ), firstelem, firstelem + numtriangles * 3, cm.numtriangles * 3);
			continue;
		}
		switch( type )
		{
		case MST_PLANAR:
		case MST_TRISURF:
			break;	// no processing necessary
		case MST_PATCH:
			patchsize[0] = LittleLong( in->patch_cp[0] );
			patchsize[1] = LittleLong( in->patch_cp[1] );
			if( numverts != (patchsize[0] * patchsize[1]) || patchsize[0] < 3 || patchsize[1] < 3 || !(patchsize[0] & 1) || !(patchsize[1] & 1) || patchsize[0] * patchsize[1] >= 4225 )
			{
				MsgDev( D_ERROR, "BSP_LoadSurfaces: face #%i (texture \"%s\"): invalid patchsize %ix%i\n", i, CM_TexName( out->shadernum ), patchsize[0], patchsize[1]);
				continue;
			}
			originalvertex3f = (float *)(cm.vertices + firstvertex);
		
			// convert patch to MST_TRISURF
			xtess = CM_PatchTesselationOnX( patchsize[0], patchsize[1], 3, originalvertex3f, 15.0f );
			ytess = CM_PatchTesselationOnY( patchsize[0], patchsize[1], 3, originalvertex3f, 15.0f );
			xtess = bound( 0, xtess, 1024 );
			ytess = bound( 0, ytess, 1024 );

			cxtess = CM_PatchTesselationOnX( patchsize[0], patchsize[1], 3, originalvertex3f, 15.0f );
			cytess = CM_PatchTesselationOnY( patchsize[0], patchsize[1], 3, originalvertex3f, 15.0f );
			cxtess = bound( 0, cxtess, 1024 );
			cytess = bound( 0, cytess, 1024 );

			Msg( "PatchTesselation: [%i] %i, %i (triangles %i)\n", i, cxtess, cytess, numtriangles );

			// store it for the LOD grouping step
	 		patchtess[patchtesscount].info.xsize = patchsize[0];
	 		patchtess[patchtesscount].info.ysize = patchsize[1];
	 		patchtess[patchtesscount].info.lods[PATCH_LOD_VISUAL].xtess = xtess;
	 		patchtess[patchtesscount].info.lods[PATCH_LOD_VISUAL].ytess = ytess;
	 		patchtess[patchtesscount].info.lods[PATCH_LOD_COLLISION].xtess = cxtess;
	 		patchtess[patchtesscount].info.lods[PATCH_LOD_COLLISION].ytess = cytess;
	
			patchtess[patchtesscount].surface_id = i;
			patchtess[patchtesscount].lodgroup[0] = in->mins[0];
			patchtess[patchtesscount].lodgroup[1] = in->mins[1];
			patchtess[patchtesscount].lodgroup[2] = in->mins[2];
			patchtess[patchtesscount].lodgroup[3] = in->maxs[0];
			patchtess[patchtesscount].lodgroup[4] = in->maxs[1];
			patchtess[patchtesscount].lodgroup[5] = in->maxs[2];
			patchtess[patchtesscount].originalvertex3f = originalvertex3f;
			patchtesscount++;
		case MST_FLARE:
			// ignore collisions at all
			continue;
		}

		out->firstvertex = firstvertex;	
		out->numvertices = numverts;
	}		

	// fix patches tesselations so that they make no seams
	do
	{
		again = false;
		for( i = 0; i < patchtesscount; ++i )
		{
			for( j = i+1; j < patchtesscount; ++j )
			{
				if( !PATCHTESS_SAME_LODGROUP( patchtess[i], patchtess[j] ))
					continue;

				if( CM_PatchAdjustTesselation( 3, &patchtess[i].info, patchtess[i].originalvertex3f, &patchtess[j].info, patchtess[j].originalvertex3f ))
					again = true;
			}
		}
	} while( again );

	in = oldin;
	out = oldout;

	for( i = 0; i < cm.numsurfaces; i++, in++, out++)
	{
		firstvertex = LittleLong( in->firstvert );

		switch( out->surfaceType )
		{
		case MST_PLANAR:
		case MST_TRISURF:
			break;
		case MST_PATCH:
			patchsize[0] = LittleLong( in->patch_cp[0] );
			patchsize[1] = LittleLong( in->patch_cp[1] );
			originalvertex3f = (float *)(cm.vertices + firstvertex);

			xtess = ytess = cxtess = cytess = -1;
			for( j = 0; j < patchtesscount; ++j )
			{
				if( patchtess[j].surface_id == i )
				{
					xtess = patchtess[j].info.lods[PATCH_LOD_VISUAL].xtess;
					ytess = patchtess[j].info.lods[PATCH_LOD_VISUAL].ytess;
					cxtess = patchtess[j].info.lods[PATCH_LOD_COLLISION].xtess;
					cytess = patchtess[j].info.lods[PATCH_LOD_COLLISION].ytess;
					break;
				}
			}
			if( xtess == -1 )
			{
				MsgDev( D_ERROR, "patch %d isn't preprocessed?!?\n", i );
				xtess = ytess = cxtess = cytess = 0;
			}
			// build the lower quality collision geometry
			finalwidth = CM_PatchDimForTess( patchsize[0], cxtess );
			finalheight = CM_PatchDimForTess( patchsize[1], cytess );
			finalvertices = finalwidth * finalheight;
			finaltriangles = (finalwidth - 1) * (finalheight - 1) * 2;

			out->vertices = (float *)Mem_Alloc( cmappool, sizeof( float[3] ) * finalvertices );
			out->indices = (int *)Mem_Alloc( cmappool, sizeof( int[3] ) * finaltriangles );
			out->numvertices = finalvertices;
			out->numtriangles = finaltriangles;
			CM_PatchTesselateFloat( 3, sizeof( float[3] ), out->vertices, patchsize[0], patchsize[1], sizeof( float[3] ), originalvertex3f, cxtess, cytess );
			CM_PatchTriangleElements( out->indices, finalwidth, finalheight, 0 );

			CM_SnapVertices( 3, out->numvertices, out->vertices, 1 );

			out->numtriangles = CM_RemoveDegenerateTriangles( out->numtriangles, out->indices, out->indices, out->vertices );
			Msg( "Genarate patch with %i triangles\n", out->numtriangles );
			break;
		case MST_FLARE:
			continue;
		default:
			MsgDev( D_ERROR, "BSP_LoadSurfaces: face #%i: unknown face type %i\n", i, type );
			continue;
		}

		// calculate a bounding box
		VectorCopy( in->mins, out->mins );
		VectorCopy( in->maxs, out->maxs );
	}
	if( patchtess ) Mem_Free( patchtess );
}

void RBSP_LoadSurfaces( lump_t *l )
{
	dsurfacer_t	*in;
	csurface_t	*out;
	int		i;

	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadSurfaces: funny lump size\n" );

	cm.numsurfaces = l->filelen / sizeof( *in );
	cm.surfaces = out = Mem_Alloc( cmappool, cm.numsurfaces * sizeof( *out ));	

	for( i = 0; i < cm.numsurfaces; i++, in++, out++)
	{
		out->shadernum = LittleLong( in->shadernum );
		out->surfaceType = LittleLong( in->facetype );		
		out->firstvertex = LittleLong( in->firstvert );
		out->numvertices = LittleLong( in->numverts );
//		out->numtriangles = LittleLong( in->numelems ) / 3;
	}
}

/*
=================
BSP_LoadCollision
=================
*/
void BSP_LoadCollision( lump_t *l )
{
	cms.world_tree = VFS_Create( cms.base + l->fileofs, l->filelen );
}

static void BSP_RecursiveFindNumLeafs( cnode_t *node )
{
	int	numleafs;

	while( node->plane )
	{
		BSP_RecursiveFindNumLeafs( node->children[0] );
		node = node->children[1];
	}
	numleafs = ((cleaf_t *)node - cm.leafs) + 1;

	if( cm.numleafs < numleafs )  // these never happens
		Host_Error( "BSP_RecursiveFindNumLeafs: invalid leafs count %i > %i\n", numleafs, cm.numleafs );
}

static void BSP_RecursiveSetParent( cnode_t *node, cnode_t *parent )
{
	node->parent = parent;

	if( node->plane )
	{
		// this is a node, recurse to children
		BSP_RecursiveSetParent( node->children[0], node );
		BSP_RecursiveSetParent( node->children[1], node );

		// combine contents of children
		node->contents = node->children[0]->contents | node->children[1]->contents;
	}
	else
	{
		cleaf_t	*leaf = (cleaf_t *)node;
		int	i;

		// if this is a leaf, calculate supercontents mask from all collidable
		// primitives in the leaf (brushes and collision surfaces)
		// also flag if the leaf contains any collision surfaces
		leaf->contents = 0;
		// combine the supercontents values of all brushes in this leaf
		for( i = 0; i < leaf->numleafbrushes; i++ )
			leaf->contents |= cm.brushes[leaf->firstleafbrush[i]].contents;

		// check if this leaf contains any collision surfaces (patches)
		for( i = 0; i < leaf->numleafsurfaces; i++ )
		{
			csurface_t *m_surface = cm.surfaces + leaf->firstleafsurface[i];
			if( m_surface->numtriangles )
			{
				Msg( "FOUND COLLISION PATCH\n" );
				leaf->havepatches = true;
				leaf->contents |= cm.shaders[m_surface->shadernum].contents;
			}
		}
	}
}

/*
===============================================================================

			BSPLIB COLLISION MAKER

===============================================================================
*/
void BSP_BeginBuildTree( void )
{
	// create tree collision
	cms.collision = NewtonCreateTreeCollision( gWorld, NULL );
	NewtonTreeCollisionBeginBuild( cms.collision );
}

void BSP_AddCollisionFace( int facenum )
{
	csurface_t	*m_surface;
	int		j, k;
	int		flags;

	if( facenum < 0 || facenum >= cm.numsurfaces )
	{
		MsgDev( D_ERROR, "invalid face number %d, must be in range [0 == %d]\n", facenum, cm.numsurfaces - 1 );
		return;
	}
          
	m_surface = cm.surfaces + facenum;
	flags = cm.shaders[m_surface->shadernum].flags;
	k = m_surface->firstvertex;
	
	// sky is noclip for all physobjects
	if( flags & SURF_SKY ) return;

	if( cm_use_triangles->integer )
	{
		// convert polygon to triangles
		for( j = 0; j < m_surface->numvertices - 2; j++ )
		{
			vec3_t	face[3]; // triangle
			CM_GetPoint( k,	face[0] );
			CM_GetPoint( k+j+1, face[1] );
			CM_GetPoint( k+j+2, face[2] );
			NewtonTreeCollisionAddFace( cms.collision, 3, (float *)face[0], sizeof(vec3_t), 1 );
		}
	}
	else
	{
		vec3_t *face = Mem_Alloc( cmappool, m_surface->numvertices * sizeof( vec3_t ));
		for(j = 0; j < m_surface->numvertices; j++ ) CM_GetPoint( k+j, face[j] );
		NewtonTreeCollisionAddFace( cms.collision, m_surface->numvertices, (float *)face[0], sizeof(vec3_t), 1);
		if( face ) Mem_Free( face ); // polygons with 0 edges ?
	}
}

void BSP_EndBuildTree( void )
{
	if( app_name == HOST_BSPLIB ) Msg( "Optimize collision tree..." );
	NewtonTreeCollisionEndBuild( cms.collision, true );
	if( app_name == HOST_BSPLIB ) Msg( " done\n" );
}

static void BSP_LoadTree( vfile_t* handle, void* buffer, size_t size )
{
	VFS_Read( handle, buffer, size );
}

void CM_LoadBSP( const void *buffer )
{
	dheader_t		header;

	header = *(dheader_t *)buffer;
	cms.base = (byte *)buffer;

	// bsplib uses light version of loading
	IBSP_LoadVertexes( &header.lumps[LUMP_VERTEXES] );
	BSP_LoadIndexes( &header.lumps[LUMP_ELEMENTS] );
	BSP_LoadShaders( &header.lumps[LUMP_SHADERS] );
	IBSP_LoadSurfaces( &header.lumps[LUMP_SURFACES] );
	BSP_LoadModels( &header.lumps[LUMP_MODELS] );
	BSP_LoadCollision( &header.lumps[LUMP_COLLISION] );
	cms.loaded = true;
}

void CM_FreeBSP( void )
{
	int	i;
	cmodel_t	*mod;

	CM_FreeWorld();

	for( i = 0, mod = cms.cmodels; i < cms.numcmodels; i++, mod++)
	{
		if( !mod->name[0] ) continue;
		CM_FreeModel( mod );
	}
}

void CM_MakeCollisionTree( void )
{
	int	i, world = 0; // world index

	if( !cms.loaded ) Host_Error( "CM_MakeCollisionTree: map not loaded\n" );
	if( cms.collision ) return; // already generated
	if( app_name == HOST_BSPLIB ) Msg( "Building collision tree...\n" );

	BSP_BeginBuildTree();

	// world firstface index always equal 0
	if( app_name == HOST_BSPLIB )
		RunThreadsOnIndividual( cms.bmodels[world].numfaces, true, BSP_AddCollisionFace );
	else for( i = 0; i < cms.bmodels[world].numfaces; i++ ) BSP_AddCollisionFace( i );

	BSP_EndBuildTree();
}

void CM_SaveCollisionTree( file_t *f, cmsave_t callback )
{
	CM_MakeCollisionTree(); // create if needed
	NewtonTreeCollisionSerialize( cms.collision, callback, f );
}

void CM_LoadCollisionTree( void )
{
	if( !cms.world_tree ) return;
	cms.collision = NewtonCreateTreeCollisionFromSerialization( gWorld, NULL, BSP_LoadTree, cms.world_tree );
	VFS_Close( cms.world_tree );
}

void CM_LoadWorld( void )
{
	vec3_t	boxP0, boxP1;
	vec3_t	extra = { 10.0f, 10.0f, 10.0f }; 

	if( cms.world_tree ) CM_LoadCollisionTree();
	else CM_MakeCollisionTree(); // can be used for old maps or for product of alternative map compiler

	cms.body = NewtonCreateBody( gWorld, cms.collision );
	NewtonBodyGetMatrix( cms.body, &cm.matrix[0][0] );	// set the global position of this body 
	NewtonCollisionCalculateAABB( cms.collision, &cm.matrix[0][0], &boxP0[0], &boxP1[0] ); 
	NewtonReleaseCollision( gWorld, cms.collision );

	VectorSubtract( boxP0, extra, boxP0 );
	VectorAdd( boxP1, extra, boxP1 );

	NewtonSetWorldSize( gWorld, &boxP0[0], &boxP1[0] ); 
	NewtonSetSolverModel( gWorld, cm_solver_model->integer );
	NewtonSetFrictionModel( gWorld, cm_friction_model->integer );
}

void CM_FreeWorld( void )
{
	int 	i;
	cmodel_t	*mod;

	// free old stuff
	if( cms.loaded )
	{
		if( cm.entityscript )
			Com_CloseScript( cm.entityscript ); 
		Mem_EmptyPool( cmappool );
	}
	Mem_Set( &cm, 0, sizeof( cm ));

	for( i = 0, mod = cms.bmodels; i < cms.numbmodels; i++, mod++ )
		CM_FreeModel( mod );
	cms.numbmodels = 0;

	if( cms.body )
	{
		// and physical body release too
		NewtonDestroyBody( gWorld, cms.body );
		cms.collision = NULL;
		cms.body = NULL;
	}
	cms.loaded = false;
}

/*
==================
CM_BeginRegistration

Loads in the map and all submodels
==================
*/
cmodel_t *CM_BeginRegistration( const char *name, bool clientload, uint *checksum )
{
	uint		*buf;
	dheader_t		*hdr;
	size_t		length;

	if( !com.strlen( name ))
	{
		CM_FreeWorld(); // release old map
		// cinematic servers won't have anything at all
		cm.numleafs = cm.numclusters = cm.numareas = 1;
		*checksum = 0;
		return &cms.bmodels[0];
	}

	if( !com.strcmp( cm.name, name ) && cms.loaded )
	{
		// singleplayer mode: server already loading map
		*checksum = cm.checksum;
		if( !clientload )
		{
			// rebuild portals for server ...
			Mem_Set( cm.areaportals, 0, sizeof( cm.areaportals ));
			CM_FloodAreaConnections();

			// ... and reset entity script
			Com_ResetScript( cm.entityscript );
		}
		// still have the right version
		return &cms.bmodels[0];
	}

	CM_FreeWorld();		// release old map
	registration_sequence++;	// all models are invalid

	// load the newmap
	buf = (uint *)FS_LoadFile( name, &length );
	if( !buf ) Host_Error( "Couldn't load %s\n", name );

	hdr = (dheader_t *)buf;
	if( !hdr ) Host_Error( "CM_LoadMap: %s couldn't read header\n", name ); 

	*checksum = cm.checksum = LittleLong( Com_BlockChecksum( buf, length ));
	hdr = (dheader_t *)buf;
	SwapBlock(( int *)hdr, sizeof( dheader_t ));	
	cms.base = (byte *)buf;

	if( !memcmp( buf, "IBSP", 4 ) || !memcmp( buf, "RBSP", 4 ) || !memcmp( buf, "FBSP", 4 ))
	{
		switch( hdr->version )
		{
		case Q3IDBSP_VERSION:	// quake3 arena
		case RTCWBSP_VERSION:	// return to castle wolfenstein
		case RFIDBSP_VERSION:	// raven or qfusion bsp
			break;
		default:
			Host_Error( "CM_LoadMap: %s has wrong version number (%i should be %i)\n", name, hdr->version, Q3IDBSP_VERSION );		
			break;
		}
	}
	else Host_Error( "CM_LoadMap: %s is not a IBSP, RBSP or FBSP file\n", name );

	// load into heap
	BSP_LoadEntityString( &hdr->lumps[LUMP_ENTITIES] );
	BSP_LoadShaders( &hdr->lumps[LUMP_SHADERS] );
	BSP_LoadPlanes( &hdr->lumps[LUMP_PLANES] );
	IBSP_LoadBrushSides( &hdr->lumps[LUMP_BRUSHSIDES] );
	BSP_LoadBrushes( &hdr->lumps[LUMP_BRUSHES] );
	IBSP_LoadVertexes( &hdr->lumps[LUMP_VERTEXES] );
	BSP_LoadIndexes( &hdr->lumps[LUMP_ELEMENTS] );
	IBSP_LoadSurfaces( &hdr->lumps[LUMP_SURFACES] );		// used only for generate NewtonCollisionTree
	BSP_LoadLeafBrushes( &hdr->lumps[LUMP_LEAFBRUSHES] );
	BSP_LoadLeafSurfaces( &hdr->lumps[LUMP_LEAFSURFACES] );
	BSP_LoadLeafs( &hdr->lumps[LUMP_LEAFS] );
	BSP_LoadNodes( &hdr->lumps[LUMP_NODES] );
	BSP_LoadVisibility( &hdr->lumps[LUMP_VISIBILITY] );
	BSP_LoadModels( &hdr->lumps[LUMP_MODELS] );
//	BSP_LoadCollision( &hdr->lumps[LUMP_COLLISION] );
	cms.loaded = true;

	BSP_RecursiveFindNumLeafs( cm.nodes );
	BSP_RecursiveSetParent( cm.nodes, NULL );

	CM_LoadWorld();		// load physics collision
	Mem_Free( buf );		// release map buffer

	com.strncpy( cm.name, name, MAX_STRING );

	Mem_Set( cm.areaportals, 0, sizeof( cm.areaportals ));
	CM_FloodAreaConnections();
	CM_CalcPHS ();

	return &cms.bmodels[0];
}

void CM_EndRegistration( void )
{
	cmodel_t	*mod;
	int	i;

	for( i = 0, mod = &cms.cmodels[0]; i < cms.numcmodels; i++, mod++)
	{
		if(!mod->name[0]) continue;
		if( mod->registration_sequence != registration_sequence )
			CM_FreeModel( mod );
	}
}

int CM_LeafContents( int leafnum )
{
	if( leafnum < 0 || leafnum >= cm.numleafs )
		Host_Error("CM_LeafContents: bad number %i >= %i\n", leafnum, cm.numleafs );
	return cm.leafs[leafnum].contents;
}

int CM_LeafCluster( int leafnum )
{
	if( leafnum < 0 || leafnum >= cm.numleafs )
		Host_Error("CM_LeafCluster: bad number %i >= %i\n", leafnum, cm.numleafs );
	return cm.leafs[leafnum].cluster;
}

int CM_LeafArea( int leafnum )
{
	if( leafnum < 0 || leafnum >= cm.numleafs )
		Host_Error("CM_LeafArea: bad number %i >= %i\n", leafnum, cm.numleafs );
	return cm.leafs[leafnum].area;
}

/*
===================
CM_ModelBounds
===================
*/
void CM_ModelBounds( cmodel_t *cmod, vec3_t mins, vec3_t maxs )
{
	if( cmod )
	{
		VectorCopy( cmod->mins, mins );
		VectorCopy( cmod->maxs, maxs );
	}
	else
	{
		VectorSet( mins, -32, -32, -32 );
		VectorSet( maxs,  32,  32,  32 );
		MsgDev( D_WARN, "can't compute bounding box, use default size\n");
	}
}


/*
===============================================================================

STUDIO SHARED CMODELS

===============================================================================
*/
int CM_StudioExtractBbox( dstudiohdr_t *phdr, int sequence, float *mins, float *maxs )
{
	dstudioseqdesc_t	*pseqdesc;
	pseqdesc = (dstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);

	if(sequence == -1) return 0;
	VectorCopy( pseqdesc[sequence].bbmin, mins );
	VectorCopy( pseqdesc[sequence].bbmax, maxs );

	return 1;
}

void CM_GetBodyCount( void )
{
	if( studio.hdr )
	{
		studio.bodypart = (dstudiobodyparts_t *)((byte *)studio.hdr + studio.hdr->bodypartindex);
		studio.bodycount = studio.bodypart->nummodels;
	}
	else studio.bodycount = 0; // just reset it
}

/*
====================
CM_StudioCalcBoneQuaterion
====================
*/
void CM_StudioCalcBoneQuaterion( dstudiobone_t *pbone, float *q )
{
	int	i;
	vec3_t	angle1;

	for(i = 0; i < 3; i++) angle1[i] = pbone->value[i+3];
	AngleQuaternion( angle1, q );
}

/*
====================
CM_StudioCalcBonePosition
====================
*/
void CM_StudioCalcBonePosition( dstudiobone_t *pbone, float *pos )
{
	int	i;
	for(i = 0; i < 3; i++) pos[i] = pbone->value[i];
}

/*
====================
CM_StudioSetUpTransform
====================
*/
void CM_StudioSetUpTransform ( void )
{
	vec3_t	mins, maxs;
	vec3_t	modelpos;

	studio.numverts = studio.numtriangles = 0; // clear current count
	CM_StudioExtractBbox( studio.hdr, 0, mins, maxs );// adjust model center
	VectorAdd( mins, maxs, modelpos );
	VectorScale( modelpos, -0.5, modelpos );

	VectorSet( vec3_angles, 0.0f, -90.0f, 90.0f );	// rotate matrix for 90 degrees
	AngleVectors( vec3_angles, studio.rotmatrix[0], studio.rotmatrix[2], studio.rotmatrix[1] );

	studio.rotmatrix[0][3] = modelpos[0];
	studio.rotmatrix[1][3] = modelpos[1];
	studio.rotmatrix[2][3] = (fabs(modelpos[2]) > 0.25) ? modelpos[2] : mins[2]; // stupid newton bug
	studio.rotmatrix[2][2] *= -1;
}

void CM_StudioCalcRotations ( float pos[][3], vec4_t *q )
{
	dstudiobone_t	*pbone = (dstudiobone_t *)((byte *)studio.hdr + studio.hdr->boneindex);
	int		i;

	for (i = 0; i < studio.hdr->numbones; i++, pbone++ ) 
	{
		CM_StudioCalcBoneQuaterion( pbone, q[i] );
		CM_StudioCalcBonePosition( pbone, pos[i]);
	}
}

/*
====================
CM_StudioSetupBones
====================
*/
void CM_StudioSetupBones( void )
{
	int		i;
	dstudiobone_t	*pbones;
	static float	pos[MAXSTUDIOBONES][3];
	static vec4_t	q[MAXSTUDIOBONES];
	matrix4x4		bonematrix;

	CM_StudioCalcRotations( pos, q );
	pbones = (dstudiobone_t *)((byte *)studio.hdr + studio.hdr->boneindex);

	for (i = 0; i < studio.hdr->numbones; i++) 
	{
		Matrix4x4_FromOriginQuat( bonematrix, pos[i][0], pos[i][1], pos[i][2], q[i][0], q[i][1], q[i][2], q[i][3] );
		if( pbones[i].parent == -1 ) Matrix4x4_ConcatTransforms( studio.bones[i], studio.rotmatrix, bonematrix );
		else Matrix4x4_ConcatTransforms( studio.bones[i], studio.bones[pbones[i].parent], bonematrix );
	}
}

void CM_StudioSetupModel ( int bodypart, int body )
{
	int index;

	if(bodypart > studio.hdr->numbodyparts) bodypart = 0;
	studio.bodypart = (dstudiobodyparts_t *)((byte *)studio.hdr + studio.hdr->bodypartindex) + bodypart;

	index = body / studio.bodypart->base;
	index = index % studio.bodypart->nummodels;
	studio.submodel = (dstudiomodel_t *)((byte *)studio.hdr + studio.bodypart->modelindex) + index;
}

void CM_StudioAddMesh( int mesh )
{
	dstudiomesh_t	*pmesh = (dstudiomesh_t *)((byte *)studio.hdr + studio.submodel->meshindex) + mesh;
	short		*ptricmds = (short *)((byte *)studio.hdr + pmesh->triindex);
	int		i;

	while(i = *(ptricmds++))
	{
		for(i = abs(i); i > 0; i--, ptricmds += 4)
		{
			studio.m_pVerts[studio.numverts][0] = INCH2METER(studio.vtransform[ptricmds[0]][0]);
			studio.m_pVerts[studio.numverts][1] = INCH2METER(studio.vtransform[ptricmds[0]][1]);
			studio.m_pVerts[studio.numverts][2] = INCH2METER(studio.vtransform[ptricmds[0]][2]);
			studio.numverts++;
		}
	}
	studio.numtriangles += pmesh->numtris;
}

void CM_StudioLookMeshes ( void )
{
	int	i;

	for (i = 0; i < studio.submodel->nummesh; i++ ) 
		CM_StudioAddMesh( i );
}

void CM_StudioGetVertices( void )
{
	int		i;
	vec3_t		*pstudioverts;
	vec3_t		*pstudionorms;
	byte		*pvertbone;
	byte		*pnormbone;

	pvertbone = ((byte *)studio.hdr + studio.submodel->vertinfoindex);
	pnormbone = ((byte *)studio.hdr + studio.submodel->norminfoindex);
	pstudioverts = (vec3_t *)((byte *)studio.hdr + studio.submodel->vertindex);
	pstudionorms = (vec3_t *)((byte *)studio.hdr + studio.submodel->normindex);

	for( i = 0; i < studio.submodel->numverts; i++ )
	{
		Matrix4x4_Transform(  studio.bones[pvertbone[i]], pstudioverts[i], studio.vtransform[i]);
	}
	for( i = 0; i < studio.submodel->numnorms; i++ )
	{
		Matrix4x4_Transform( studio.bones[pnormbone[i]], pstudionorms[i], studio.ntransform[i]);
	}
	CM_StudioLookMeshes();
}

void CM_CreateMeshBuffer( byte *buffer )
{
	int	i, j;

	// setup global pointers
	studio.hdr = (dstudiohdr_t *)buffer;
	studio.m_pVerts = &studio.vertices[0];

	CM_GetBodyCount();

	for( i = 0; i < studio.bodycount; i++)
	{
		// already loaded
		if( loadmodel->col[i] ) continue;

		CM_StudioSetUpTransform();
		CM_StudioSetupBones();

		// lookup all bodies
		for (j = 0; j < studio.hdr->numbodyparts; j++)
		{
			CM_StudioSetupModel( j, i );
			CM_StudioGetVertices();
		}
		if( studio.numverts )
		{
			loadmodel->col[i] = (cmesh_t *)Mem_Alloc( loadmodel->mempool, sizeof(*loadmodel->col[0]));
			loadmodel->col[i]->verts = Mem_Alloc( loadmodel->mempool, studio.numverts * sizeof(vec3_t));
			Mem_Copy( loadmodel->col[i]->verts, studio.m_pVerts, studio.numverts * sizeof(vec3_t));
			loadmodel->col[i]->numtris = studio.numtriangles;
			loadmodel->col[i]->numverts = studio.numverts;
			loadmodel->numbodies++;
		}
	}
}

bool CM_StudioModel( byte *buffer, uint filesize )
{
	dstudiohdr_t	*phdr;
	dstudioseqdesc_t	*pseqdesc;

	phdr = (dstudiohdr_t *)buffer;
	if( phdr->version != STUDIO_VERSION )
	{
		MsgDev( D_ERROR, "CM_StudioModel: %s has wrong version number (%i should be %i)", phdr->name, phdr->version, STUDIO_VERSION);
		return false;
	}

	loadmodel->numbodies = 0;
	loadmodel->type = mod_studio;
	loadmodel->extradata = Mem_Alloc( loadmodel->mempool, filesize );
	Mem_Copy( loadmodel->extradata, buffer, filesize );

	// calcualte bounding box
	pseqdesc = (dstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	VectorCopy( pseqdesc[0].bbmin, loadmodel->mins );
	VectorCopy( pseqdesc[0].bbmax, loadmodel->maxs );
	loadmodel->numframes = pseqdesc[0].numframes;	// FIXME: get numframes from current sequence (not first)

	// FIXME: calc bounding box right
	VectorCopy( loadmodel->mins, loadmodel->normalmins );
	VectorCopy( loadmodel->maxs, loadmodel->normalmaxs );
	VectorCopy( loadmodel->mins, loadmodel->rotatedmins );
	VectorCopy( loadmodel->maxs, loadmodel->rotatedmaxs );
	VectorCopy( loadmodel->mins, loadmodel->yawmins );
	VectorCopy( loadmodel->maxs, loadmodel->yawmaxs );

	CM_CreateMeshBuffer( buffer ); // newton collision mesh

	return true;
}

bool CM_SpriteModel( byte *buffer, uint filesize )
{
	dsprite_t		*phdr;

	phdr = (dsprite_t *)buffer;

	if( phdr->version != SPRITE_VERSION )
	{
		MsgDev( D_ERROR, "CM_SpriteModel: %s has wrong version number (%i should be %i)\n", loadmodel->name, phdr->version, SPRITE_VERSION );
		return false;
	}
          
	loadmodel->type = mod_sprite;
	loadmodel->numbodies = 0; // sprites don't have bodies
	loadmodel->numframes = phdr->numframes;
	loadmodel->mins[0] = loadmodel->mins[1] = -phdr->bounds[0] / 2;
	loadmodel->maxs[0] = loadmodel->maxs[1] = phdr->bounds[0] / 2;
	loadmodel->mins[2] = -phdr->bounds[1] / 2;
	loadmodel->maxs[2] = phdr->bounds[1] / 2;

	// FIXME: calc bounding box right
	VectorCopy( loadmodel->mins, loadmodel->normalmins );
	VectorCopy( loadmodel->maxs, loadmodel->normalmaxs );
	VectorCopy( loadmodel->mins, loadmodel->rotatedmins );
	VectorCopy( loadmodel->maxs, loadmodel->rotatedmaxs );
	VectorCopy( loadmodel->mins, loadmodel->yawmins );
	VectorCopy( loadmodel->maxs, loadmodel->yawmaxs );

	return true;
}

bool CM_BrushModel( byte *buffer, uint filesize )
{
	MsgDev( D_WARN, "CM_BrushModel: not implemented\n" );
	return false;
}

cmodel_t *CM_RegisterModel( const char *name )
{
	byte	*buf;
	int	i, size;
	cmodel_t	*mod;

	if( !name[0] ) return NULL;
	if(name[0] == '*') 
	{
		i = com.atoi( name + 1);
		if( i < 1 || !cms.loaded || i >= cms.numbmodels)
		{
			MsgDev(D_WARN, "CM_InlineModel: bad submodel number %d\n", i );
			return NULL;
		}
		// prolonge registration
		cms.bmodels[i].registration_sequence = registration_sequence;
		return &cms.bmodels[i];
	}
	for( i = 0; i < cms.numcmodels; i++ )
          {
		mod = &cms.cmodels[i];
		if(!mod->name[0]) continue;
		if(!com.strcmp( name, mod->name ))
		{
			// prolonge registration
			mod->registration_sequence = registration_sequence;
			return mod;
		}
	} 

	// find a free model slot spot
	for( i = 0, mod = cms.cmodels; i < cms.numcmodels; i++, mod++)
	{
		if( !mod->name[0] ) break; // free spot
	}
	if( i == cms.numcmodels )
	{
		if( cms.numcmodels == MAX_MODELS )
		{
			MsgDev( D_ERROR, "CM_LoadModel: MAX_MODELS limit exceeded\n" );
			return NULL;
		}
		cms.numcmodels++;
	}

	com.strncpy( mod->name, name, sizeof(mod->name));
	buf = FS_LoadFile( name, &size );
	if(!buf)
	{
		MsgDev( D_ERROR, "CM_LoadModel: %s not found\n", name );
		Mem_Set(mod->name, 0, sizeof(mod->name));
		return NULL;
	}


	MsgDev( D_NOTE, "CM_LoadModel: load %s\n", name );
	mod->mempool = Mem_AllocPool( va("^2%s^7", mod->name ));
	loadmodel = mod;

	// call the apropriate loader
	switch(LittleLong(*(uint *)buf))
	{
	case IDSTUDIOHEADER:
		CM_StudioModel( buf, size );
		break;
	case IDSPRITEHEADER:
		CM_SpriteModel( buf, size );
		break;
	case IDBSPMODHEADER:
		CM_BrushModel( buf, size );//FIXME
		break;
	}
	Mem_Free( buf ); 
	return mod;
}