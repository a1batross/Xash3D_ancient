//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_model.c - collision model
//=======================================================================

#include "cm_local.h"
#include "matrix_lib.h"
#include "const.h"

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
	int vert_index;
	int edge_index = cm.surfedges[index];

	if(edge_index > 0) vert_index = cm.edges[edge_index].v[0];
	else vert_index = cm.edges[-edge_index].v[1];
	CM_ConvertPositionToMeters( out, cm.vertices[vert_index].point );
}

void CM_GetPoint2( int index, vec3_t out )
{
	int vert_index;
	int edge_index = cm.surfedges[index];

	if(edge_index > 0) vert_index = cm.edges[edge_index].v[0];
	else vert_index = cm.edges[-edge_index].v[1];
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
	Mem_Set( mod, 0, sizeof( *mod ));
	mod = NULL;
}

int CM_NumTextures( void ) { return cm.numshaders; }
int CM_NumClusters( void ) { return cm.numclusters; }
int CM_NumInlineModels( void ) { return cms.numbmodels; }
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
	if( app_name == HOST_BSPLIB || modelnum >= cms.numbmodels )
		return;

	loadmodel = &cms.bmodels[modelnum];
	if( modelnum ) loadmodel->type = mod_brush;
	else loadmodel->type = mod_world; // level static geometry
	loadmodel->TraceBox = CM_TraceBmodel;
	loadmodel->PointContents = CM_PointContents;
	loadmodel->AmbientLevel = CM_AmbientSounds;

	// because world loading collision tree from LUMP_COLLISION
	if( modelnum < 1 ) return;
	studio.m_pVerts = &studio.vertices[0]; // using studio vertex buffer for bmodels too
	studio.numverts = 0; // clear current count

	for( d = 0, i = loadmodel->firstface; d < loadmodel->numfaces; i++, d++ )
	{
		vec3_t *face;

		m_surface = cm.surfaces + i;
		flags = cm.shaders[cm.texinfo[m_surface->texinfo].shadernum].surfaceflags;
		k = m_surface->firstedge;

		// sky is noclip for all physobjects
		if( flags & SURF_SKY ) continue;
		face = Mem_Alloc( loadmodel->mempool, m_surface->numedges * sizeof( vec3_t ));
		for(j = 0; j < m_surface->numedges; j++ ) 
		{
			// because it's not a collision tree, just triangle mesh
			CM_GetPoint2( k+j, studio.m_pVerts[studio.numverts] );
			studio.numverts++;
		}
		if( face ) Mem_Free( face ); // faces with 0 edges ?
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

void BSP_LoadModels( wfile_t *l )
{
	dmodel_t	*in;
	cmodel_t	*out;
	size_t	filelen;
	int	i, j, n, c, count;

	in = (void *)WAD_Read( l, LUMP_MODELS, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadModels: funny lump size\n" );
	count = filelen / sizeof( *in );

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
void BSP_LoadShaders( wfile_t *l )
{
	dshader_t		*in;
	csurface_t	*out;
	size_t		filelen;
	int 		i;

	in = (void *)WAD_Read( l, LUMP_SHADERS, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadShaders: funny lump size\n" );
	cm.numshaders = filelen / sizeof(*in);
	cm.shaders = out = (csurface_t *)Mem_Alloc( cmappool, cm.numshaders * sizeof( *out ));

	for( i = 0; i < cm.numshaders; i++, in++, out++)
	{
		com.strncpy( out->name, in->name, MAX_SHADERPATH );
		out->contentflags = LittleLong( in->contentFlags );
		out->surfaceflags = LittleLong( in->surfaceFlags );
	}
}

/*
=================
BSP_LoadTexinfo
=================
*/
void BSP_LoadTexinfo( wfile_t *l )
{
	dtexinfo_t	*in, *out;
	size_t		filelen;
	int 		i, count;

	in = (void *)WAD_Read( l, LUMP_TEXINFO, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadTexinfo: funny lump size\n" );
	count = filelen / sizeof( *in );

	out = cm.texinfo = (dtexinfo_t *)Mem_Alloc( cmappool, count * sizeof( *out ));
	cm.numtexinfo = count;

	// store shaders indextable only
	for ( i = 0; i < count; i++, in++, out++)
	{
		out->shadernum = LittleLong( in->shadernum );
		out->value = LittleLong( in->value );
	}
}

/*
=================
BSP_LoadNodes
=================
*/
void BSP_LoadNodes( wfile_t *l )
{
	dnode_t	*in;
	cnode_t	*out;
	size_t	filelen;
	int	i, j, n, count;
	
	in = (void *)WAD_Read( l, LUMP_NODES, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadNodes: funny lump size\n" );
	count = filelen / sizeof( *in );

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
void BSP_LoadBrushes( wfile_t *l )
{
	dbrush_t	*in;
	cbrush_t	*out;
	size_t	filelen;
	int	i, j, n, count, maxplanes = 0;
	cplanef_t	*planes = NULL;
	
	in = (void *)WAD_Read( l, LUMP_BRUSHES, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadBrushes: funny lump size\n" );
	count = filelen / sizeof( *in );
	out = cm.brushes = (cbrush_t *)Mem_Alloc( cmappool, (count + 1) * sizeof( *out ));
	cm.numbrushes = count;

	for( i = 0; i < count; i++, out++, in++ )
	{
		out->firstbrushside = LittleLong( in->firstside );
		out->numsides = LittleLong( in->numsides );
		n = LittleLong( in->shadernum );
		if( n < 0 || n >= cm.numshaders )
			Host_Error( "BSP_LoadBrushes: invalid shader index %i (brush %i)\n", n, i );
		out->contents = cm.shaders[n].contentflags;
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
BSP_LoadLeafSurffaces
=================
*/
void BSP_LoadLeafSurfaces( wfile_t *l )
{
	dleafface_t	*in, *out;
	int		i, n, count;
	size_t		filelen;
	
	in = (void *)WAD_Read( l, LUMP_LEAFFACES, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadLeafFaces: funny lump size\n" );
	count = filelen / sizeof( *in );

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
void BSP_LoadLeafBrushes( wfile_t *l )
{
	dleafbrush_t	*in, *out;
	int		i, count;
	size_t		filelen;
	
	in = (void *)WAD_Read( l, LUMP_LEAFBRUSHES, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadLeafBrushes: funny lump size\n" );
	count = filelen / sizeof( *in );

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
void BSP_LoadLeafs( wfile_t *l )
{
	dleaf_t 	*in;
	cleaf_t	*out;
	int	i, j, n, c, count;
	int	emptyleaf = -1;
	size_t	filelen;
		
	in = (void *)WAD_Read( l, LUMP_LEAFS, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadLeafs: funny lump size\n" );

	count = filelen / sizeof( *in );
	if( count < 1 ) Host_Error( "Map %s with no leafs\n", cm.name );
	out = cm.leafs = (cleaf_t *)Mem_Alloc( cmappool, count * sizeof(*out));
	cm.numleafs = count;
	cm.numclusters = 0;

	for( i = 0; i < count; i++, in++, out++)
	{
		out->parent = NULL;
		out->plane = NULL;
		out->contents = LittleLong( in->contents );
		out->cluster = LittleLong( in->cluster );
		out->area = LittleLong( in->area );
		if( out->cluster >= cm.numclusters )
			cm.numclusters = out->cluster + 1;
		for( j = 0; j < 3; j++ )
		{
			// yes the mins/maxs are ints
			out->mins[j] = LittleLong( in->mins[j] ) - 1;
			out->maxs[j] = LittleLong( in->maxs[j] ) + 1;
		}

		for( j = 0; j < NUM_AMBIENTS; j++ )
			out->ambient_level[j] = (float)(in->sounds[j] / 255.0f);

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

	// probably any wall it's liquid ?
	if( cm.leafs[0].contents != CONTENTS_SOLID )
		Host_Error("Map %s with leaf 0 is not CONTENTS_SOLID\n", cm.name );

	for( i = 1; i < count; i++ )
	{
		if( !cm.leafs[i].contents )
		{
			emptyleaf = i;
			break;
		}
	}

	// stuck into brushes
	if( emptyleaf == -1 ) Host_Error( "Map %s does not have an empty leaf\n", cm.name );
}

/*
=================
BSP_LoadPlanes
=================
*/
void BSP_LoadPlanes( wfile_t *l )
{
	dplane_t	*in;
	cplane_t	*out;
	int	i, j, count;
	size_t	filelen;
	
	in = (void *)WAD_Read( l, LUMP_PLANES, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadPlanes: funny lump size\n" );

	count = filelen / sizeof( *in );
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
void BSP_LoadBrushSides( wfile_t *l )
{
	dbrushside_t 	*in;
	cbrushside_t	*out;
	int		i, j, num,count;
	size_t		filelen;

	in = (void *)WAD_Read( l, LUMP_BRUSHSIDES, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadBrushSides: funny lump size\n" );

	count = filelen / sizeof( *in );
	out = cm.brushsides = (cbrushside_t *)Mem_Alloc( cmappool, count * sizeof( *out ));
	cm.numbrushsides = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		num = LittleLong(in->planenum);
		out->plane = cm.planes + num;
		j = LittleLong( in->texinfo );
		j = bound( 0, j, cm.numtexinfo - 1 );
		out->shader = cm.shaders + cm.texinfo[j].shadernum;
	}
}

/*
=================
BSP_LoadAreas
=================
*/
void BSP_LoadAreas( wfile_t *l )
{
	darea_t 		*in;
	carea_t		*out;
	int		i, count;
	size_t		filelen;

	in = (void *)WAD_Read( l, LUMP_AREAS, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadAreas: funny lump size\n" );

	count = filelen / sizeof( *in );
  	out = cm.areas = (carea_t *)Mem_Alloc( cmappool, count * sizeof( *out ));
	cm.numareas = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->numareaportals = LittleLong( in->numareaportals );
		out->firstareaportal = LittleLong( in->firstareaportal );
		out->floodvalid = 0;
		out->floodnum = 0;
	}
}

/*
=================
BSP_LoadAreaPortals
=================
*/
void BSP_LoadAreaPortals( wfile_t *l )
{
	dareaportal_t	*in, *out;
	int		i, count;
	size_t		filelen;

	in = (void *)WAD_Read( l, LUMP_AREAPORTALS, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadAreaPortals: funny lump size\n" );

	count = filelen / sizeof( *in );
	out = cm.areaportals = (dareaportal_t *)Mem_Alloc( cmappool, count * sizeof( *out ));
	cm.numareaportals = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->portalnum = LittleLong( in->portalnum );
		out->otherarea = LittleLong( in->otherarea );
	}
}

/*
=================
BSP_LoadVisibility
=================
*/
void BSP_LoadVisibility( wfile_t *l )
{
	size_t	i, filelen;
	byte	*in;

	in = WAD_Read( l, LUMP_VISIBILITY, &filelen, TYPE_BINDATA );
	if( !filelen ) return;

	cm.visbase = (byte *)Mem_Alloc( cmappool, filelen );
	Mem_Copy( cm.visbase, in, filelen );

	cm.vis = (dvis_t *)cm.visbase; // conversion
	cm.vis->numclusters = LittleLong( cm.vis->numclusters );
	for( i = 0; i < cm.vis->numclusters; i++ )
	{
		cm.vis->bitofs[i][0] = LittleLong( cm.vis->bitofs[i][0] );
		cm.vis->bitofs[i][1] = LittleLong( cm.vis->bitofs[i][1] );
	}

	if( cm.numclusters != cm.vis->numclusters )
		Host_Error( "BSP_LoadVisibility: mismatch vis and leaf clusters (%i should be %i)\n", cm.vis->numclusters, cm.numclusters );
}

/*
=================
BSP_LoadEntityString
=================
*/
void BSP_LoadEntityString( wfile_t *l )
{
	size_t	filelen;
	byte	*in;

	in = WAD_Read( l, LUMP_ENTITIES, &filelen, TYPE_SCRIPT );
	cm.entitystring = (byte *)Mem_Alloc( cmappool, filelen );
	cm.checksum = LittleLong(Com_BlockChecksum( in, filelen ));
	Mem_Copy( cm.entitystring, in, filelen );
}

/*
=================
BSP_LoadVerts
=================
*/
void BSP_LoadVertexes( wfile_t *l )
{
	dvertex_t		*in;
	size_t		filelen;
	int		count;

	in = (void *)WAD_Read( l, LUMP_VERTEXES, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadVertexes: funny lump size\n" );

	count = filelen / sizeof( *in );
	cm.vertices = Mem_Alloc( cmappool, count * sizeof( *in ));
	Mem_Copy( cm.vertices, in, count * sizeof( *in ));

	SwapBlock((int *)cm.vertices, sizeof( *in ) * count );
}

/*
=================
BSP_LoadEdges
=================
*/
void BSP_LoadEdges( wfile_t *l )
{
	dedge_t	*in;
	size_t	filelen;
	int 	count;

	in = (void *)WAD_Read( l, LUMP_EDGES, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadEdges: funny lump size\n" );

	count = filelen / sizeof( *in );
	cm.edges = Mem_Alloc( cmappool, count * sizeof( *in ));
	Mem_Copy( cm.edges, in, count * sizeof( *in ));

	SwapBlock( (int *)cm.edges, sizeof(*in) * count );
}

/*
=================
BSP_LoadSurfedges
=================
*/
void BSP_LoadSurfedges( wfile_t *l )
{	
	dsurfedge_t	*in;
	size_t		filelen;
	int		count;
	
	in = (void *)WAD_Read( l, LUMP_SURFEDGES, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadSurfedges: funny lump size\n" );

	count = filelen / sizeof( *in );
	if( count < 1 ) Host_Error( "BSP_LoadSurfedges: map without surfedges\n" );
	cm.surfedges = Mem_Alloc( cmappool, count * sizeof( *in ));	
	Mem_Copy( cm.surfedges, in, count * sizeof( *in ));

	SwapBlock((int *)cm.surfedges, sizeof( *in ) * count );
}


/*
=================
BSP_LoadSurfaces
=================
*/
void BSP_LoadSurfaces( wfile_t *l )
{
	dsurface_t	*in, *out;
	size_t		filelen;
	int		i;

	in = (void *)WAD_Read( l, LUMP_SURFACES, &filelen, TYPE_BINDATA );
	if( filelen % sizeof( *in )) Host_Error( "BSP_LoadSurfaces: funny lump size\n" );

	cm.numsurfaces = filelen / sizeof( *in );
	cm.surfaces = out = Mem_Alloc( cmappool, cm.numsurfaces * sizeof( *in ));	

	for( i = 0; i < cm.numsurfaces; i++, in++, out++)
	{
		out->firstedge = LittleLong( in->firstedge );
		out->numedges = LittleLong( in->numedges );		
		out->texinfo = LittleLong( in->texinfo );
	}
}

/*
=================
BSP_LoadCollision
=================
*/
void BSP_LoadCollision( wfile_t *l )
{
	size_t	filelen;
	byte	*in;	

	in = (void *)WAD_Read( l, LUMP_COLLISION, &filelen, TYPE_BINDATA );
	if( filelen ) cms.world_tree = VFS_Create( in, filelen );	
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
			dsurface_t *m_surface = cm.surfaces + leaf->firstleafsurface[i];
			csurface_t *surface = cm.shaders + cm.texinfo[m_surface->texinfo].shadernum;
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
	cms.collision = NewtonCreateTreeCollision( gWorld, NULL );
	NewtonTreeCollisionBeginBuild( cms.collision );
}

void BSP_AddCollisionFace( int facenum )
{
	dsurface_t	*m_surface;
	int		j, k;
	int		flags;

	if( facenum < 0 || facenum >= cm.numsurfaces )
	{
		MsgDev( D_ERROR, "invalid face number %d, must be in range [0 == %d]\n", facenum, cm.numsurfaces - 1 );
		return;
	}
          
	m_surface = cm.surfaces + facenum;
	flags = cm.shaders[cm.texinfo[m_surface->texinfo].shadernum].surfaceflags;
	k = m_surface->firstedge;
	
	// sky is noclip for all physobjects
	if( flags & SURF_SKY ) return;

	if( cm_use_triangles->integer )
	{
		// convert polygon to triangles
		for( j = 0; j < m_surface->numedges - 2; j++ )
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
		vec3_t *face = Mem_Alloc( cmappool, m_surface->numedges * sizeof( vec3_t ));
		for(j = 0; j < m_surface->numedges; j++ ) CM_GetPoint( k+j, face[j] );
		NewtonTreeCollisionAddFace( cms.collision, m_surface->numedges, (float *)face[0], sizeof(vec3_t), 1);
		if( face ) Mem_Free( face ); // polygons with 0 edges ?
	}
}

void BSP_EndBuildTree( void )
{
	if( app_name == HOST_BSPLIB ) Msg("Optimize collision tree..." );
	NewtonTreeCollisionEndBuild( cms.collision, true );
	if( app_name == HOST_BSPLIB ) Msg(" done\n");
}

static void BSP_LoadTree( vfile_t* handle, void* buffer, size_t size )
{
	VFS_Read( handle, buffer, size );
}

void CM_LoadBSP( wfile_t *handle )
{
	// bsplib uses light version of loading
	BSP_LoadVertexes( handle );
	BSP_LoadEdges( handle );
	BSP_LoadSurfedges( handle );
	BSP_LoadShaders( handle );
	BSP_LoadTexinfo( handle );
	BSP_LoadSurfaces( handle );
	BSP_LoadModels( handle );
	BSP_LoadCollision( handle );
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
	if( app_name == HOST_BSPLIB ) Msg("Building collision tree...\n" );

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
	if( cms.loaded ) Mem_EmptyPool( cmappool );
	Mem_Set( &cm, 0, sizeof( cm ));

	for( i = 0, mod = cms.bmodels; i < cms.numbmodels; i++, mod++ )
		CM_FreeModel( mod );
	cms.numbmodels = 0;

	if( cms.body )
	{
		// and physical body release too
		NewtonDestroyBody( gWorld, cms.body );
		cms.body = NULL;
		cms.collision = NULL;
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
	dheader_t		*hdr;

	// make sure what old map released
	if( cms.handle ) WAD_Close( cms.handle );
	cms.handle = NULL;

	if( !com.strlen( name ))
	{
		CM_FreeWorld(); // release old map
		// cinematic servers won't have anything at all
		cm.numleafs = cm.numclusters = cm.numareas = 1;
		*checksum = 0;
		return &cms.bmodels[0];
	}

	if(!com.strcmp( cm.name, name ) && cms.loaded )
	{
		// singleplayer mode: serever already loading map
		*checksum = cm.checksum;
		if( !clientload )
		{
			// rebuild portals for server
			Mem_Set( cms.portalopen, 0, sizeof( cms.portalopen ));
			CM_FloodAreaConnections();
		}
		// still have the right version
		return &cms.bmodels[0];
	}

	CM_FreeWorld();		// release old map
	registration_sequence++;	// all models are invalid

	// load the newmap
	cms.handle = WAD_Open( name, "rb" );
	if( !cms.handle ) Host_Error( "Couldn't load %s\n", name );

	hdr = (dheader_t *)WAD_Read( cms.handle, LUMP_MAPINFO, NULL, TYPE_BINDATA );
	if( !hdr ) Host_Error( "CM_LoadMap: %s couldn't read header\n", name ); 

	hdr->ident = LittleLong( hdr->ident );
	hdr->version = LittleLong( hdr->version );

	if( hdr->ident != IDBSPMODHEADER )
		Host_Error( "CM_LoadMap: %s is not a IBSP file\n", name );
	if( hdr->version != BSPMOD_VERSION )
		Host_Error( "CM_LoadMap: %s has wrong version number (%i should be %i)\n", name, hdr->version, BSPMOD_VERSION );

	// load into heap
	BSP_LoadEntityString( cms.handle );
	BSP_LoadShaders( cms.handle );
	BSP_LoadTexinfo( cms.handle );
	BSP_LoadPlanes( cms.handle );
	BSP_LoadBrushSides( cms.handle );
	BSP_LoadBrushes( cms.handle );
	BSP_LoadVertexes( cms.handle );
	BSP_LoadEdges( cms.handle );
	BSP_LoadSurfedges( cms.handle );
	BSP_LoadSurfaces( cms.handle );		// used only for generate NewtonCollisionTree
	BSP_LoadLeafBrushes( cms.handle );
	BSP_LoadLeafSurfaces( cms.handle );
	BSP_LoadLeafs( cms.handle );
	BSP_LoadNodes( cms.handle );
	BSP_LoadAreas( cms.handle );
	BSP_LoadAreaPortals( cms.handle );
	BSP_LoadVisibility( cms.handle );
	BSP_LoadModels( cms.handle );
	BSP_LoadCollision( cms.handle );
	cms.loaded = true;

	BSP_RecursiveFindNumLeafs( cm.nodes );
	BSP_RecursiveSetParent( cm.nodes, NULL );

	CM_LoadWorld();		// load physics collision
	*checksum = cm.checksum;	// checksum of ents lump
	WAD_Close( cms.handle );	// release map buffer
	cms.handle = NULL;

	com.strncpy( cm.name, name, MAX_STRING );
	Mem_Set( cms.portalopen, 0, sizeof( cms.portalopen ));
	CM_FloodAreaConnections();

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
	if(studio.hdr)
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
	MsgDev( D_WARN, "CM_BrushModel: not implemented\n");
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