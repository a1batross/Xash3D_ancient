//=======================================================================
//			Copyright XashXT Group 2007 �
//			cm_model.c - collision model
//=======================================================================

#include "cm_local.h"
#include "matrixlib.h"
#include "const.h"

clipmap_t		cm;
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
	int vert_index = cm.indices[index];
	CM_ConvertPositionToMeters( out, cm.vertices[vert_index].point );
}

void CM_GetPoint2( int index, vec3_t out )
{
	int vert_index = cm.indices[index];
	CM_ConvertDimensionToMeters( out, cm.vertices[vert_index].point );
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

/*
================
CM_FreeModel
================
*/
void CM_FreeModel( cmodel_t *mod )
{
	Mem_FreePool( &mod->mempool );
	memset( mod, 0, sizeof(*mod));
	mod = NULL;
}

int CM_NumTextures( void ) { return cm.numshaders; }
int CM_NumClusters( void ) { return cm.numclusters; }
int CM_NumInlineModels( void ) { return cm.numbmodels; }
const char *CM_EntityString( void ) { return cm.entitystring; }
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
	dsurface_t	*m_surface;
	int		d, i, j, k;
	int		flags;

	// ignore world or bsplib instance
	if( app_name == HOST_BSPLIB || modelnum >= cm.numbmodels )
		return;

	loadmodel = &cm.bmodels[modelnum];
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
		flags = cm.shaders[m_surface->shadernum].surfaceflags;
		k = m_surface->firstvertex;

		// sky is noclip for all physobjects
		if(flags & SURF_SKY) continue;
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

	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadModels: funny lump size\n");
	count = l->filelen / sizeof(*in);

	if(count < 1) Host_Error("Map %s without models\n", cm.name );
	if(count > MAX_MODELS ) Host_Error("Map %s has too many models\n", cm.name );
	cm.numbmodels = count;
	out = &cm.bmodels[0];

	Msg("BSP_LoadModels: %i\n", cm.numbmodels );
	for ( i = 0; i < count; i++, in++, out++ )
	{
		for( j = 0; j < 3; j++ )
		{
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat(in->mins[j]) - 1;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
		}

		out->firstface = n = LittleLong( in->firstface );
		out->numfaces = c = LittleLong( in->numfaces );

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
			Host_Error("BSP_LoadModels: invalid face range %i : %i (%i faces)\n", n, n+c, cm.numsurfaces );
		out->firstbrush = n = LittleLong( in->firstbrush );
		out->numbrushes = c = LittleLong( in->numbrushes );
		if( n < 0 || n + c > cm.numbrushes )
			Host_Error("BSP_LoadModels: invalid brush range %i : %i (%i brushes)\n", n, n+c, cm.numsurfaces );
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
	csurface_t	*out;
	int 		i;

	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadShaders: funny lump size\n" );
	cm.numshaders = l->filelen / sizeof(*in);
	cm.shaders = out = (csurface_t *)Mem_Alloc( cmappool, cm.numshaders * sizeof(*out));

	Msg("BSP_LoadShaders: %i\n", cm.numshaders );
	for ( i = 0; i < cm.numshaders; i++, in++, out++)
	{
		com.strncpy( out->name, in->name, MAX_QPATH );
		out->contentflags = LittleLong( in->contents );
		out->surfaceflags = LittleLong( in->flags );
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
	
	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadNodes: funny lump size\n");
	count = l->filelen / sizeof(*in);

	if( count < 1 ) Host_Error("Map %s has no nodes\n", cm.name );
	out = cm.nodes = (cnode_t *)Mem_Alloc( cmappool, count * sizeof(*out));
	cm.numnodes = count;

	Msg("BSP_LoadNodes: %i\n", cm.numnodes );
	for (i = 0; i < count; i++, out++, in++)
	{
		out->parent = NULL;
		n = LittleLong( in->planenum );
		if( n < 0 || n >= cm.numplanes)
			Host_Error("BSP_LoadNodes: invalid planenum %i (%i planes)\n", n, cm.numplanes );
		out->plane = cm.planes + n;
		for( j = 0; j < 2; j++)
		{
			n = LittleLong( in->children[j]);
			if( n >= 0 )
			{
				if( n >= cm.numnodes )
					Host_Error("BSP_LoadNodes: invalid child node index %i (%i nodes)\n", n, cm.numnodes );
				out->children[j] = cm.nodes + n;
			}
			else
			{
				n = -1 - n;
				if( n >= cm.numleafs )
					Host_Error("BSP_LoadNodes: invalid child leaf index %i (%i leafs)\n", n, cm.numleafs );
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
	dbrush_t	*in;
	cbrush_t	*out;
	int	i, j, count, maxplanes = 0;
	cplanef_t	*planes = NULL;
	
	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadBrushes: funny lump size\n");
	count = l->filelen / sizeof(*in);
	out = cm.brushes = (cbrush_t *)Mem_Alloc( cmappool, (count + 1) * sizeof(*out));
	cm.numbrushes = count;

	Msg("BSP_LoadBrushes: %i\n", cm.numbrushes );
	for (i = 0; i < count; i++, out++, in++)
	{
		out->firstbrushside = LittleLong(in->firstside);
		out->numsides = LittleLong(in->numsides);
		out->contents = LittleLong(in->contents);
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
			planes[j].surfaceflags = cm.brushsides[out->firstbrushside + j].shader->surfaceflags;
			planes[j].surface = cm.brushsides[out->firstbrushside + j].shader;
		}
		// make the colbrush from the planes
		out->colbrushf = CM_CollisionNewBrushFromPlanes( cmappool, out->numsides, planes, out->contents );
	}
}

/*
=================
BSP_LoadLeafFaces
=================
*/
void BSP_LoadLeafFaces( lump_t *l )
{
	dword	*in, *out;
	int	i, n, count;
	
	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadLeafFaces: funny lump size\n");
	count = l->filelen / sizeof(*in);

	out = cm.leafsurfaces = (dword *)Mem_Alloc( cmappool, count * sizeof(*out));
	cm.numleafsurfaces = count;

	Msg("BSP_LoadLeafFaces: %i\n", cm.numleafsurfaces );
	for( i = 0; i < count; i++, in++, out++ )
	{
		n = LittleLong( *in );
		if( n < 0 || n >= cm.numsurfaces )
			Host_Error("BSP_LoadLeafFaces: invalid face index %i (%i faces)\n", n, cm.numsurfaces );
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
	dword	*in, *out;
	int	i, count;
	
	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("CMod_LoadLeafBrushes: funny lump size\n");
	count = l->filelen / sizeof(*in);

	if( count < 1 ) Host_Error("Map %s with no leaf brushes\n", cm.name );
	out = cm.leafbrushes = (dword *)Mem_Alloc( cmappool, count * sizeof(*out));
	cm.numleafbrushes = count;
	Msg("BSP_LoadLeafBrushes: %i\n", cm.numleafbrushes );
	for ( i = 0; i < count; i++, in++, out++) *out = LittleShort(*in);
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
	int	emptyleaf = -1;
	
	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadLeafs: funny lump size\n");
	count = l->filelen / sizeof(*in);
	if( count < 1 ) Host_Error("Map %s with no leafs\n", cm.name );
	out = cm.leafs = (cleaf_t *)Mem_Alloc( cmappool, count * sizeof(*out));
	cm.numleafs = count;
	cm.numclusters = 0;

	Msg("BSP_LoadLeafs: %i\n", cm.numleafs );
	for ( i = 0; i < count; i++, in++, out++)
	{
		out->parent = NULL;
		out->plane = NULL;
		out->contents = LittleLong( in->contents );
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
		n = LittleLong( in->firstleafface );
		c = LittleLong( in->numleaffaces );
		if( n < 0 || n + c > cm.numleafsurfaces )
			Host_Error("BSP_LoadLeafs: invalid leafsurface range %i : %i (%i leafsurfaces)\n", n, n + c, cm.numleafsurfaces);
		out->firstleafsurface = cm.leafsurfaces + n;
		out->numleafsurfaces = c;
		n = LittleLong( in->firstleafbrush );
		c = LittleLong( in->numleafbrushes );
		if( n < 0 || n + c > cm.numleafbrushes )
			Host_Error("BSP_LoadLeafs: invalid leafbrush range %i : %i (%i leafbrushes)\n", n, n + c, cm.numleafbrushes);
		out->firstleafbrush = cm.leafbrushes + n;
		out->numleafbrushes = c;
	}

	Msg("BSP_LoadLeafs: areas %i\n", cm.numareas );
	cm.areas = Mem_Alloc( cmappool, cm.numareas * sizeof( *cm.areas ));
	cm.areaportals = Mem_Alloc( cmappool, cm.numareas * cm.numareas * sizeof( *cm.areaportals ));

	// probably any wall it's liquid ?
	if( cm.leafs[0].contents != CONTENTS_SOLID )
		Host_Error("Map %s with leaf 0 is not CONTENTS_SOLID\n", cm.name );

	for( i = 1; i < count; i++ )
	{
		if(!cm.leafs[i].contents)
		{
			emptyleaf = i;
			break;
		}
	}

	// stuck into brushes
	if( emptyleaf == -1 ) Host_Error("Map %s does not have an empty leaf\n", cm.name );
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
	
	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("CMod_LoadPlanes: funny lump size\n");
	count = l->filelen / sizeof(*in);
	if (count < 1) Host_Error("Map %s with no planes\n", cm.name );
	out = cm.planes = (cplane_t *)Mem_Alloc( cmappool, count * sizeof(*out));
	cm.numplanes = count;

	Msg("BSP_LoadPlanes: %i\n", cm.numplanes );
	for ( i = 0; i < count; i++, in++, out++)
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
void BSP_LoadBrushSides( lump_t *l )
{
	dbrushside_t 	*in;
	cbrushside_t	*out;
	int		i, j, num,count;

	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("CMod_LoadBrushSides: funny lump size\n");
	count = l->filelen / sizeof(*in);
	out = cm.brushsides = (cbrushside_t *)Mem_Alloc( cmappool, count * sizeof(*out));
	cm.numbrushsides = count;

	Msg("BSP_LoadBrushsides: %i\n", cm.numbrushsides );
	for ( i = 0; i < count; i++, in++, out++)
	{
		num = LittleLong(in->planenum);
		out->plane = cm.planes + num;
		j = LittleLong( in->shadernum );
		j = bound( 0, j, cm.numshaders - 1 );
		out->shader = cm.shaders + j;
	}
}


/*
=================
BSP_LoadVisibility
=================
*/
void BSP_LoadVisibility( lump_t *l )
{
	if( !l->filelen )
	{
		size_t	pvs_size, row_size;
	
		// create novis lump
		row_size = (cm.numclusters + 7) / 8;
		pvs_size = row_size * cm.numclusters;
		cm.pvs = (dvis_t *)Mem_Alloc( cmappool, pvs_size + sizeof(dvis_t) - 1 );
		cm.pvs->numclusters = cm.numclusters;
		cm.pvs->rowsize = row_size;
		memset( cm.pvs->data, 0xFF, pvs_size );
		return;
	}

	cm.pvs = (dvis_t *)Mem_Alloc( cmappool, l->filelen );
	Mem_Copy( cm.pvs, cm.mod_base + l->fileofs, l->filelen );

	cm.pvs->numclusters = LittleLong ( cm.pvs->numclusters );
	cm.pvs->rowsize = LittleLong ( cm.pvs->rowsize );
}

/*
=================
BSP_LoadHearability
=================
*/
void BSP_LoadHearability( lump_t *l )
{
	if( !l->filelen )
	{
		size_t	phs_size, row_size;
	
		// create novis lump
		row_size = (cm.numclusters + 7) / 8;
		phs_size = row_size * cm.numclusters;
		cm.phs = (dvis_t *)Mem_Alloc( cmappool, phs_size + sizeof(dvis_t) - 1 );
		cm.phs->numclusters = cm.numclusters;
		cm.phs->rowsize = row_size;
		memset( cm.phs->data, 0xFF, phs_size );
		return;
	}

	cm.phs = (dvis_t *)Mem_Alloc( cmappool, l->filelen );
	Mem_Copy( cm.phs, cm.mod_base + l->fileofs, l->filelen );

	cm.phs->numclusters = LittleLong ( cm.phs->numclusters );
	cm.phs->rowsize = LittleLong ( cm.phs->rowsize );
}

/*
=================
BSP_LoadEntityString
=================
*/
void BSP_LoadEntityString( lump_t *l )
{
	cm.entitystring = (byte *)Mem_Alloc( cmappool, l->filelen );
	Mem_Copy( cm.entitystring, cm.mod_base + l->fileofs, l->filelen );
}

/*
=================
BSP_LoadVerts
=================
*/
void BSP_LoadVertexes( lump_t *l )
{
	dvertex_t		*in;
	int		count;

	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadVerts: funny lump size\n");
	count = l->filelen / sizeof(*in);
	cm.vertices = Mem_Alloc( cmappool, count * sizeof( *in ));
	Mem_Copy( cm.vertices, in, count * sizeof( *in ));

	// FIXME: these code swapped colors
	SwapBlock( (int *)cm.vertices, sizeof(*in) * count );
}

/*
=================
BSP_LoadEdges
=================
*/
void BSP_LoadIndexes( lump_t *l )
{
	int	*in, count;

	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadEdges: funny lump size\n");
	count = l->filelen / sizeof(*in);
	cm.indices = Mem_Alloc( cmappool, count * sizeof(*in));
	Mem_Copy( cm.indices, in, count * sizeof(*in));
	SwapBlock((int *)cm.indices, sizeof(*in) * count );
}

/*
=================
BSP_LoadSurfaces
=================
*/
void BSP_LoadSurfaces( lump_t *l )
{
	dsurface_t	*in;

	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadSurfaces: funny lump size\n");
	cm.numsurfaces = l->filelen / sizeof(*in);
	cm.surfaces = Mem_Alloc( cmappool, cm.numsurfaces * sizeof(*in));	

	Msg("BSP_LoadSurfaces: %i\n", cm.numsurfaces );
	Mem_Copy( cm.surfaces, in, cm.numsurfaces * sizeof(*in));
	SwapBlock((int *)cm.surfaces, cm.numsurfaces * sizeof(*in));
}

/*
=================
BSP_LoadCollision
=================
*/
void BSP_LoadCollision( lump_t *l )
{
	cm.world_tree = VFS_Create( cm.mod_base + l->fileofs, l->filelen );	
}

/*
=================
BSP_LoadBuiltinProgs
=================
*/
void BSP_LoadBuiltinProgs( lump_t *l )
{	
	// not implemented
	if( !l->filelen )
	{
		return;
	}
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

	// these never happens
	if( cm.numleafs < numleafs ) cm.numleafs = numleafs;
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
			dsurface_t *m_surface = cm.surfaces + leaf->firstleafsurface[i];
			csurface_t *surface = cm.shaders + m_surface->shadernum;
			if( surface->numtriangles )
			{
				leaf->havepatches = true;
				leaf->contents |= surface->contentflags;
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
	cm.collision = NewtonCreateTreeCollision( gWorld, NULL );
	NewtonTreeCollisionBeginBuild( cm.collision );
}

void BSP_AddCollisionFace( int facenum )
{
	dsurface_t	*m_surface;
	int		i, j, k;
	int		flags;

	if( facenum < 0 || facenum >= cm.numsurfaces )
	{
		MsgDev( D_ERROR, "invalid face number %d, must be in range [0 == %d]\n", facenum, cm.numsurfaces - 1 );
		return;
	}
          
	m_surface = cm.surfaces + facenum;
	flags = cm.shaders[m_surface->shadernum].surfaceflags;
	k = m_surface->firstvertex;
	Msg("add collision face %i [%i verts, inds %i]\n", facenum, m_surface->numvertices, m_surface->numindices );
	
	// sky is noclip for all physobjects
	if( flags & SURF_SKY ) return;

	if( cm_use_triangles->integer )
	{
		// convert polygon to triangles
		for( j = 0; j < m_surface->numvertices - 2; j++ )
		{
			vec3_t	face[3]; // triangle
			Msg("triangle: %i ", j );
			CM_GetPoint( k,	face[0] );
			CM_GetPoint( k+j+2, face[1] );
			CM_GetPoint( k+j+1, face[2] );
			for( i = 0; i < 3; i++ )
				Msg("( %.f %.f %.f ) ", face[i][0], face[i][1], face[i][2] );
			Msg("\n" );
			NewtonTreeCollisionAddFace( cm.collision, 3, (float *)face[0], sizeof(vec3_t), 1 );
		}
	}
	else
	{
		vec3_t *face = Mem_Alloc( cmappool, m_surface->numvertices * sizeof(vec3_t));
		for(j = 0; j < m_surface->numvertices; j++ ) CM_GetPoint( k+j, face[j] );
		NewtonTreeCollisionAddFace( cm.collision, m_surface->numvertices, (float *)face[0], sizeof(vec3_t), 1);
		if( face ) Mem_Free( face ); // polygons with 0 edges ?
	}
}

void BSP_EndBuildTree( void )
{
	if( app_name == HOST_BSPLIB ) Msg("Optimize collision tree..." );
	NewtonTreeCollisionEndBuild( cm.collision, true );
	if( app_name == HOST_BSPLIB ) Msg(" done\n");
}

static void BSP_LoadTree( vfile_t* handle, void* buffer, size_t size )
{
	VFS_Read( handle, buffer, size );
}

void CM_LoadBSP( const void *buffer )
{
	dheader_t		header;

	header = *(dheader_t *)buffer;
	cm.mod_base = (byte *)buffer;

	// bsplib uses light version of loading
	BSP_LoadVertexes(&header.lumps[LUMP_VERTICES]);
	BSP_LoadIndexes(&header.lumps[LUMP_INDICES]);
	BSP_LoadShaders(&header.lumps[LUMP_SHADERS]);
	BSP_LoadSurfaces(&header.lumps[LUMP_SURFACES]);
	BSP_LoadModels(&header.lumps[LUMP_MODELS]);
	BSP_LoadCollision(&header.lumps[LUMP_COLLISION]);
	cm.loaded = true;
}

void CM_FreeBSP( void )
{
	int	i;
	cmodel_t	*mod;

	CM_FreeWorld();
	for( i = 0, mod = &cm.cmodels[0]; i < cm.numcmodels; i++, mod++)
	{
		if( mod->name ) CM_FreeModel( mod );
	}
}

void CM_MakeCollisionTree( void )
{
	int	i, world = 0; // world index

	if( !cm.loaded ) Host_Error( "CM_MakeCollisionTree: map not loaded\n" );
	if( cm.collision ) return; // already generated
	if( app_name == HOST_BSPLIB ) Msg("Building collision tree...\n" );

	BSP_BeginBuildTree();

	// world firstface index always equal 0
	if( app_name == HOST_BSPLIB )
		RunThreadsOnIndividual( cm.bmodels[world].numfaces, true, BSP_AddCollisionFace );
	else for( i = 0; i < cm.bmodels[world].numfaces; i++ ) BSP_AddCollisionFace( i );

	BSP_EndBuildTree();
}

void CM_SaveCollisionTree( file_t *f, cmsave_t callback )
{
	CM_MakeCollisionTree(); // create if needed
	NewtonTreeCollisionSerialize( cm.collision, callback, f );
}

void CM_LoadCollisionTree( void )
{
	cm.collision = NewtonCreateTreeCollisionFromSerialization( gWorld, NULL, BSP_LoadTree, cm.world_tree );
	VFS_Close( cm.world_tree );
	cm.world_tree = NULL;
}

void CM_LoadWorld( const void *buffer )
{
	vec3_t		boxP0, boxP1;
	vec3_t		extra = { 10.0f, 10.0f, 10.0f }; 

	if( cm.world_tree ) CM_LoadCollisionTree();
	else CM_MakeCollisionTree(); // can be used for old maps or for product of alternative map compiler

	cm.body = NewtonCreateBody( gWorld, cm.collision );
	NewtonBodyGetMatrix( cm.body, &cm.matrix[0][0] );	// set the global position of this body 
	NewtonCollisionCalculateAABB( cm.collision, &cm.matrix[0][0], &boxP0[0], &boxP1[0] ); 
	NewtonReleaseCollision( gWorld, cm.collision );

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
	if( cm.loaded ) Mem_EmptyPool( cmappool );
	cm.numclusters = cm.floodvalid = 0;
	cm.numplanes = cm.numnodes = cm.numleafs = 0;
	cm.floodvalid = cm.numbrushsides = cm.numshaders = 0;
	cm.numbrushes = cm.numleafsurfaces = cm.numareas = 0;
	cm.numleafbrushes = cm.numsurfaces = cm.numbmodels = 0;
	
	cm.name[0] = 0;
	memset( cm.matrix, 0, sizeof(matrix4x4));
	memset( cm.nullvis, 0xFF, MAX_MAP_LEAFS / 8 );
	
	// free bmodels too
	for (i = 0, mod = &cm.bmodels[0]; i < cm.numbmodels; i++, mod++)
	{
		if(!mod->name[0]) continue;
		if(mod->registration_sequence != registration_sequence)
			CM_FreeModel( mod );
	}
	cm.numbmodels = 0;

	if( cm.body )
	{
		// and physical body release too
		NewtonDestroyBody( gWorld, cm.body );
		cm.body = NULL;
		cm.collision = NULL;
	}
	cm.loaded = false;
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

	if(!com.strlen(name))
	{
		CM_FreeWorld(); // release old map
		// cinematic servers won't have anything at all
		cm.numleafs = cm.numclusters = cm.numareas = 1;
		*checksum = 0;
		return &cm.bmodels[0];
	}
	if(!com.strcmp( cm.name, name ) && cm.loaded )
	{
		// singleplayer mode: serever already loading map
		*checksum = cm.checksum;
		if( !clientload )
		{
			// rebuild portals for server
			memset( cm.portalstate, 0, sizeof(cm.portalstate));
			CM_FloodAreaConnections();
		}
		// still have the right version
		return &cm.bmodels[0];
	}

	CM_FreeWorld();		// release old map
	registration_sequence++;	// all models are invalid

	// load the newmap
	buf = (uint *)FS_LoadFile( name, &length );
	if(!buf) Host_Error("Couldn't load %s\n", name );

	*checksum = cm.checksum = LittleLong(Com_BlockChecksum (buf, length));
	hdr = (dheader_t *)buf;
	SwapBlock( (int *)hdr, sizeof(dheader_t));	
	if( hdr->version != BSPMOD_VERSION )
		Host_Error("CM_LoadMap: %s has wrong version number (%i should be %i)\n", name, hdr->version, BSPMOD_VERSION );
	cm.mod_base = (byte *)buf;

	// load into heap
	BSP_LoadEntityString(&hdr->lumps[LUMP_ENTITIES]);
	BSP_LoadShaders(&hdr->lumps[LUMP_SHADERS]);
	BSP_LoadPlanes(&hdr->lumps[LUMP_PLANES]);
	BSP_LoadBrushSides(&hdr->lumps[LUMP_BRUSHSIDES]);
	BSP_LoadBrushes(&hdr->lumps[LUMP_BRUSHES]);
	BSP_LoadVertexes(&hdr->lumps[LUMP_VERTICES]);
	BSP_LoadIndexes(&hdr->lumps[LUMP_INDICES]);
	BSP_LoadSurfaces(&hdr->lumps[LUMP_SURFACES]);		// used only for generate NewtonCollisionTree
	BSP_LoadLeafBrushes(&hdr->lumps[LUMP_LEAFBRUSHES]);
	BSP_LoadLeafFaces(&hdr->lumps[LUMP_LEAFFACES]);
	BSP_LoadLeafs(&hdr->lumps[LUMP_LEAFS]);
	BSP_LoadNodes(&hdr->lumps[LUMP_NODES]);
	BSP_LoadVisibility(&hdr->lumps[LUMP_VISIBILITY]);
	BSP_LoadHearability(&hdr->lumps[LUMP_HEARABILITY]);
	BSP_LoadModels(&hdr->lumps[LUMP_MODELS]);
	BSP_LoadCollision(&hdr->lumps[LUMP_COLLISION]);

	BSP_RecursiveFindNumLeafs( cm.nodes );
	BSP_RecursiveSetParent( cm.nodes, NULL );

	CM_LoadWorld( buf );// load physics collision
	Mem_Free( buf );	// release map buffer

	com.strncpy( cm.name, name, MAX_STRING );
	memset( cm.portalstate, 0, sizeof(cm.portalstate));
	CM_FloodAreaConnections();
	cm.loaded = true;

	return &cm.bmodels[0];
}

void CM_EndRegistration( void )
{
	cmodel_t	*mod;
	int	i;

	for( i = 0, mod = &cm.cmodels[0]; i < cm.numcmodels; i++, mod++)
	{
		if(!mod->name[0]) continue;
		if( mod->registration_sequence != registration_sequence )
			CM_FreeModel( mod );
	}
}

int CM_LeafContents( int leafnum )
{
	if( leafnum < 0 || leafnum >= cm.numleafs )
		Host_Error("CM_LeafContents: bad number %d\n", leafnum );
	return cm.leafs[leafnum].contents;
}

int CM_LeafCluster( int leafnum )
{
	if( leafnum < 0 || leafnum >= cm.numleafs )
		Host_Error("CM_LeafCluster: bad number %d\n", leafnum );
	return cm.leafs[leafnum].cluster;
}

int CM_LeafArea( int leafnum )
{
	if (leafnum < 0 || leafnum >= cm.numleafs)
		Host_Error("CM_LeafArea: bad number %d\n", leafnum );
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
int CM_StudioExtractBbox( studiohdr_t *phdr, int sequence, float *mins, float *maxs )
{
	mstudioseqdesc_t	*pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);

	if(sequence == -1) return 0;
	VectorCopy( pseqdesc[sequence].bbmin, mins );
	VectorCopy( pseqdesc[sequence].bbmax, maxs );

	return 1;
}

void CM_GetBodyCount( void )
{
	if(studio.hdr)
	{
		studio.bodypart = (mstudiobodyparts_t *)((byte *)studio.hdr + studio.hdr->bodypartindex);
		studio.bodycount = studio.bodypart->nummodels;
	}
	else studio.bodycount = 0; // just reset it
}

/*
====================
CM_StudioCalcBoneQuaterion
====================
*/
void CM_StudioCalcBoneQuaterion( mstudiobone_t *pbone, float *q )
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
void CM_StudioCalcBonePosition( mstudiobone_t *pbone, float *pos )
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
	mstudiobone_t	*pbone = (mstudiobone_t *)((byte *)studio.hdr + studio.hdr->boneindex);
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
	mstudiobone_t	*pbones;
	static float	pos[MAXSTUDIOBONES][3];
	static vec4_t	q[MAXSTUDIOBONES];
	matrix4x4		bonematrix;

	CM_StudioCalcRotations( pos, q );
	pbones = (mstudiobone_t *)((byte *)studio.hdr + studio.hdr->boneindex);

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
	studio.bodypart = (mstudiobodyparts_t *)((byte *)studio.hdr + studio.hdr->bodypartindex) + bodypart;

	index = body / studio.bodypart->base;
	index = index % studio.bodypart->nummodels;
	studio.submodel = (mstudiomodel_t *)((byte *)studio.hdr + studio.bodypart->modelindex) + index;
}

void CM_StudioAddMesh( int mesh )
{
	mstudiomesh_t	*pmesh = (mstudiomesh_t *)((byte *)studio.hdr + studio.submodel->meshindex) + mesh;
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
	studio.hdr = (studiohdr_t *)buffer;
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
	studiohdr_t	*phdr;
	mstudioseqdesc_t	*pseqdesc;

	phdr = (studiohdr_t *)buffer;
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
	pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
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
	MsgDev( D_WARN, "CM_BrushModel: not implemented\n");
	return false;
}

cmodel_t *CM_RegisterModel( const char *name )
{
	byte	*buf;
	int	i, size;
	cmodel_t	*mod;

	if(!name[0]) return NULL;
	if(name[0] == '*') 
	{
		i = com.atoi( name + 1);
		if( i < 1 || !cm.loaded || i >= cm.numbmodels)
		{
			MsgDev(D_WARN, "CM_InlineModel: bad submodel number %d\n", i );
			return NULL;
		}
		// prolonge registration
		cm.bmodels[i].registration_sequence = registration_sequence;
		return &cm.bmodels[i];
	}
	for( i = 0; i < cm.numcmodels; i++ )
          {
		mod = &cm.cmodels[i];
		if(!mod->name[0]) continue;
		if(!com.strcmp( name, mod->name ))
		{
			// prolonge registration
			mod->registration_sequence = registration_sequence;
			return mod;
		}
	} 

	// find a free model slot spot
	for( i = 0, mod = cm.cmodels; i < cm.numcmodels; i++, mod++)
	{
		if(!mod->name[0]) break; // free spot
	}
	if( i == cm.numcmodels)
	{
		if( cm.numcmodels == MAX_MODELS )
		{
			MsgDev( D_ERROR, "CM_LoadModel: MAX_MODELS limit exceeded\n" );
			return NULL;
		}
		cm.numcmodels++;
	}

	com.strncpy( mod->name, name, sizeof(mod->name));
	buf = FS_LoadFile( name, &size );
	if(!buf)
	{
		MsgDev( D_ERROR, "CM_LoadModel: %s not found\n", name );
		memset(mod->name, 0, sizeof(mod->name));
		return NULL;
	}

	MsgDev(D_NOTE, "CM_LoadModel: load %s\n", name );
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