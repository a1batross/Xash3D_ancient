//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         cm_collision.c - collision hulls
//=======================================================================

#include "physic.h"

typedef struct cfacedesc_s
{
	int	flags;	// surface description
} cfacedesc_t;

typedef struct collision_tree_s
{
	byte		*pmod_base;	// buffer
	dvertex_t		*vertices;
	int		*surfedges;
	dface_t		*surfaces;
	dedge_t		*edges;
	dmodel_t		*models;
	cfacedesc_t	*surfdesc;

	int		num_models;
	int		num_faces;	// for bounds checking

	vfile_t		*world_tree;	// pre-calcualated collision tree (not including submodels, worldmodel only)

	NewtonCollision	*collision;
	NewtonBody	*body;
	bool		loaded;		// map is loaded?
	bool		tree_build;	// phys tree is builded ?
	bool		use_thread;	// bsplib use thread

} collision_tree_t;

collision_tree_t map;

/*
=================
BSP_LoadVerts
=================
*/
void BSP_LoadVerts( lump_t *l )
{
	dvertex_t		*in, *out;
	int		i, count;

	in = (void *)(map.pmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadVerts: funny lump size\n");
	count = l->filelen / sizeof(*in);
	map.vertices = out = Mem_Alloc( physpool, count * sizeof(*out));

	// swap and scale vertices 
	for ( i = 0; i < count; i++, in++, out++)
	{
		//CM_ConvertPositionToMeters( out->point, in->point );
		out->point[0] = LittleFloat(in->point[0]);
		out->point[1] = LittleFloat(in->point[1]);
		out->point[2] = LittleFloat(in->point[2]);
	}
}

/*
=================
BSP_LoadEdges
=================
*/
void BSP_LoadEdges( lump_t *l )
{
	dedge_t	*in, *out;
	int 	i, count;

	in = (void *)(map.pmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadEdges: funny lump size\n");
	count = l->filelen / sizeof(*in);
	map.edges = out = Mem_Alloc( physpool, count * sizeof(*out));

	for ( i = 0; i < count; i++, in++, out++)
	{
		out->v[0] = (word)LittleShort(in->v[0]);
		out->v[1] = (word)LittleShort(in->v[1]);
	}
}

/*
=================
BSP_LoadSurfedges
=================
*/
void BSP_LoadSurfedges( lump_t *l )
{	
	int	*in, *out;
	int	i, count;
	
	in = (void *)(map.pmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadSurfedges: funny lump size\n");
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES) Host_Error("BSP_LoadSurfedges: funny lump size\n");
	map.surfedges = out = Mem_Alloc( physpool, count * sizeof(*out));	

	for ( i = 0; i < count; i++) out[i] = LittleLong(in[i]);
}

/*
=================
BSP_LoadFaces
=================
*/
void BSP_LoadFaces( lump_t *l )
{
	dface_t		*in, *out;
	int		i;

	in = (void *)(map.pmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadFaces: funny lump size\n");
	map.num_faces = l->filelen / sizeof(*in);
	map.surfaces = out = Mem_Alloc( physpool, map.num_faces * sizeof(*out));	

	for( i = 0; i < map.num_faces; i++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);		
		out->desc = LittleShort(in->desc);
	}
}

/*
=================
BSP_LoadModels
=================
*/
void BSP_LoadModels( lump_t *l )
{
	dmodel_t		*in, *out;
	int		i;

	in = (void *)(map.pmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadModels: funny lump size\n");
	map.num_models = l->filelen / sizeof(*in);
	map.models = out = Mem_Alloc( physpool, map.num_models * sizeof(*out));
	if(map.num_models < 1) Host_Error("Map with no models\n");
	if(map.num_models > MAX_MAP_MODELS) Host_Error("Map has too many models\n");

	for ( i = 0; i < map.num_models; i++, in++, out++)
	{
		CM_ConvertDimensionToMeters( out->mins, in->mins ); 
		CM_ConvertDimensionToMeters( out->maxs, in->maxs ); 
		out->headnode = LittleLong( in->headnode );
		out->firstface = LittleLong( in->firstface );
		out->numfaces = LittleLong( in->numfaces );
		out->firstbrush = LittleLong( in->firstbrush );
		out->numbrushes = LittleLong( in->numbrushes );
	}
}

/*
=================
BSP_LoadSurfDesc
=================
*/
void BSP_LoadSurfDesc( lump_t *l )
{
	dsurfdesc_t	*in;
	cfacedesc_t	*out;
	int 		i, count;

	in = (void *)(map.pmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadSurfDesc: funny lump size\n" );
	count = l->filelen / sizeof(*in);
          map.surfdesc = out = Mem_Alloc( physpool, count * sizeof(*out));

	for ( i = 0; i < count; i++, in++, out++)
	{
		out->flags = LittleLong (in->flags);
	}
}

/*
=================
BSP_LoadModels
=================
*/
void BSP_LoadCollision( lump_t *l )
{
	byte	*in;
	int	i, count;
	
	in = (void *)(map.pmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadCollision: funny lump size\n");
	count = l->filelen / sizeof(*in);

	if(count == 256)
	{
		// LUMP_POP always have null array with size of 256 bytes
		// it's like pop.lmp from Quake1, but never used in Quake2
		for(i = 0; i < count; i++) if( in[i] != 0 ) break;

		// legacy LUMP_POP, not a collision tree
		if(i == count) return;
	}
	map.world_tree = com.vfcreate( in, count );	
}

void CM_GetPoint( int index, vec3_t out )
{
	int vert_index;
	int edge_index = map.surfedges[index];

	if(edge_index > 0) vert_index = map.edges[edge_index].v[0];
	else vert_index = map.edges[-edge_index].v[1];
	CM_ConvertPositionToMeters( out, map.vertices[vert_index].point );
}

void BSP_BeginBuildTree( void )
{
	// create tree collision
	map.collision = NewtonCreateTreeCollision( gWorld, NULL );
	NewtonTreeCollisionBeginBuild( map.collision );
}

void BSP_AddCollisionFace( int facenum )
{
	dface_t		*m_face;
	int		j, k;
	int		flags;

	if(facenum < 0 || facenum >= map.num_faces)
	{
		MsgDev(D_ERROR, "invalid face number %d, must be in range [0 == %d]\n", map.num_faces - 1);
		return;
	}

	m_face = map.surfaces + facenum;
	flags = map.surfdesc[m_face->desc].flags;
	k = m_face->firstedge;

	// sky is noclip for all physobjects
	if(flags & SURF_SKY) return;

	if( cm_use_triangles->integer )
	{
		// convert polygon to triangles
		for(j = 0; j < m_face->numedges - 2; j++)
		{
			vec3_t	face[3]; // triangle

			CM_GetPoint( k, face[0] );
			CM_GetPoint( k+j+1, face[1] );
			CM_GetPoint( k+j+2, face[2] );
			NewtonTreeCollisionAddFace( map.collision, 3, (float *)face[0], sizeof(vec3_t), 1);
		}
	}
	else
	{
		vec3_t *face = Mem_Alloc( physpool, m_face->numedges * sizeof(vec3_t));
		for(j = 0; j < m_face->numedges; j++ ) CM_GetPoint( k+j, face[j] );
		NewtonTreeCollisionAddFace( map.collision, m_face->numedges, (float *)face[0], sizeof(vec3_t), 1);
		Mem_Free( face );
	}
}

void BSP_EndBuildTree( void )
{
	if(map.use_thread) Msg("Optimize collision tree..." );
	NewtonTreeCollisionEndBuild( map.collision, true );
	if(map.use_thread) Msg(" done\n");
}

static void BSP_LoadTree( vfile_t* handle, void* buffer, size_t size )
{
	VFS_Read( handle, buffer, size );
	Msg("BSP_LoadTree: size %d\n", size );
}

void CM_LoadBSP( const void *buffer )
{
	dheader_t		header;

	header = *(dheader_t *)buffer;
	map.pmod_base = (byte *)buffer;

	// loading level
	BSP_LoadVerts(&header.lumps[LUMP_VERTEXES]);
	BSP_LoadEdges(&header.lumps[LUMP_EDGES]);
	BSP_LoadSurfedges(&header.lumps[LUMP_SURFEDGES]);
	BSP_LoadFaces(&header.lumps[LUMP_FACES]);
	BSP_LoadModels(&header.lumps[LUMP_MODELS]);
	BSP_LoadSurfDesc(&header.lumps[LUMP_SURFDESC]);
	BSP_LoadCollision(&header.lumps[LUMP_COLLISION]); 
	map.loaded = true;
	map.use_thread = true;

	// keep bspdata because we want create bsp models
	// as kinematic objects: doors, fans, pendulums etc
}

void CM_FreeBSP( void )
{
	if(!map.loaded) return;

	Mem_Free( map.surfedges );
	Mem_Free( map.edges );
	Mem_Free( map.surfaces );
	Mem_Free( map.vertices );
	Mem_Free( map.models );

	map.num_models = 0;
	map.num_faces = 0;
	map.loaded = false;
}

void CM_MakeCollisionTree( void )
{
	int	i, world = 0; // world index

	if(!map.loaded) Host_Error("CM_MakeCollisionTree: map not loaded\n");
	if(map.collision) return; // already generated
	if(map.use_thread) Msg("Building collision tree...\n" );

	BSP_BeginBuildTree();

	// world firstface index always equal 0
	if(map.use_thread) RunThreadsOnIndividual( map.models[world].numfaces, true, BSP_AddCollisionFace );
	else for( i = 0; i < map.models[world].numfaces; i++ ) BSP_AddCollisionFace( i );

	BSP_EndBuildTree();
}

void CM_SaveCollisionTree( file_t *f, cmsave_t callback )
{
	CM_MakeCollisionTree(); // create if needed
	NewtonTreeCollisionSerialize( map.collision, callback, f );
}

void CM_LoadCollisionTree( void )
{
	map.collision = NewtonCreateTreeCollisionFromSerialization( gWorld, NULL, BSP_LoadTree, map.world_tree );
}

void CM_LoadWorld( const void *buffer )
{
	matrix4x4		m_matrix;
	vec3_t		boxP0, boxP1;
	vec3_t		extra = { 10.0f, 10.0f, 10.0f }; 

	CM_LoadBSP( buffer ); // loading bspdata

	map.use_thread = false;
	if(map.world_tree) CM_LoadCollisionTree();
	else CM_MakeCollisionTree();

	map.body = NewtonCreateBody( gWorld, map.collision );
	NewtonBodyGetMatrix( map.body, &m_matrix[0][0] );	// set the global position of this body 
	NewtonCollisionCalculateAABB( map.collision, &m_matrix[0][0], &boxP0[0], &boxP1[0] ); 
	NewtonReleaseCollision (gWorld, map.collision );

	VectorSubtract( boxP0, extra, boxP0 );
	VectorAdd( boxP1, extra, boxP1 );

	NewtonSetWorldSize( gWorld, &boxP0[0], &boxP1[0] ); 
	NewtonSetSolverModel( gWorld, cm_solver_model->integer );
	NewtonSetFrictionModel( gWorld, cm_friction_model->integer );
}

void CM_FreeWorld( void )
{
	CM_FreeBSP();

	if(!map.body) return; 
	// and physical body release too
	NewtonDestroyBody( gWorld, map.body );
	VFS_Close( map.world_tree );
	map.body = NULL;
	map.collision = NULL;
	map.world_tree = NULL;
}