//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_model.c - collision model
//=======================================================================

#include "cm_local.h"
#include "byteorder.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "const.h"

clipmap_t			cm;
clipmap_static_t		cms;

cmodel_t			*sv_models[MAX_MODELS]; // server replacement modeltable
cplane_t			box_planes[6];
cbrushside_t		box_brushsides[6];
cbrush_t			box_brush[1];
cbrush_t			*box_markbrushes[1];
cmodel_t			box_cmodel[1];
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
	cplane_t		*p;
	cbrushside_t	*s;
	int		i, side;

	box_brush->numsides = 6;
	box_brush->brushsides = box_brushsides;
	box_brush->contents = BASECONT_BODY;

	box_markbrushes[0] = box_brush;

	box_cmodel->leaf.nummarkfaces = 0;
	box_cmodel->leaf.markfaces = NULL;
	box_cmodel->leaf.markbrushes = box_markbrushes;
	box_cmodel->leaf.nummarkbrushes = 1;

	for( i = 0; i < 6; i++ )
	{
		side = i & 1;

		// brush sides
		s = box_brushsides + i;
		s->plane = box_planes + i;
		s->shadernum = 0;

		// planes
		p = &box_planes[i];
		VectorClear( p->normal );

		if( i & 1 )
		{
			p->type = PLANE_NONAXIAL;
			p->normal[i>>1] = -1;
			p->signbits = (1<<(i>>1));
		}
		else
		{
			p->type = i>>1;
			p->normal[i>>1] = 1;
			p->signbits = 0;
		}
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
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = -mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = -mins[2];

	VectorCopy( mins, box_cmodel->mins );
	VectorCopy( maxs, box_cmodel->maxs );

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
static void BSP_LoadModels( lump_t *l )
{
	dmodel_t	*in;
	cmodel_t	*out;
	int	i, j, count;
	
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

		if( i == 0 ) out->type = mod_world;
		else out->type = mod_brush;

		// make a "leaf" just to hold the model's brushes and surfaces
		out->leaf.nummarkfaces = LittleLong( in->numsurfaces );
		out->leaf.markfaces = Mem_Alloc( cms.mempool, out->leaf.nummarkfaces * sizeof( cface_t* ));
		out->leaf.nummarkbrushes = LittleLong( in->numbrushes );
		out->leaf.markbrushes = Mem_Alloc( cms.mempool, out->leaf.nummarkbrushes * sizeof( cbrush_t* ));

		for( j = 0; j < out->leaf.nummarkfaces; j++ )
			out->leaf.markfaces[j] = cm.faces + LittleLong( in->firstsurface ) + j;
		for( j = 0; j < out->leaf.nummarkbrushes; j++ )
			out->leaf.markbrushes[j] = cm.brushes + LittleLong( in->firstbrush ) + j;
	}

	sv_models[1] = cm.models; // make link to world
}

/*
=================
BSP_LoadShaders
=================
*/
static void BSP_LoadShaders( lump_t *l )
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
BSP_LoadVerts
=================
*/
static void IBSP_LoadVertexes( lump_t *l )
{
	dvertexq_t	*in;
	vec3_t		*out;
	int		i;

	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadVertexes: funny lump size\n" );

	cm.numverts = l->filelen / sizeof( *in );
	cm.vertices = out = Mem_Alloc( cms.mempool, cm.numverts * sizeof( *out ));

	for( i = 0; i < cm.numverts; i++, in++ )
	{
		out[i][0] = LittleFloat( in->point[0] );
		out[i][1] = LittleFloat( in->point[1] );
		out[i][2] = LittleFloat( in->point[2] );
	}
}

static void RBSP_LoadVertexes( lump_t *l )
{
	dvertexr_t	*in;
	vec3_t		*out;
	int		i;

	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadVertexes: funny lump size\n" );

	cm.numverts = l->filelen / sizeof( *in );
	cm.vertices = out = Mem_Alloc( cms.mempool, cm.numverts * sizeof( *out ));

	for( i = 0; i < cm.numverts; i++, in++ )
	{
		out[i][0] = LittleFloat( in->point[0] );
		out[i][1] = LittleFloat( in->point[1] );
		out[i][2] = LittleFloat( in->point[2] );
	}
}

static void XBSP_LoadVertexes( lump_t *l )
{
	dvertexx_t	*in;
	vec3_t		*out;
	int		i;

	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadVertexes: funny lump size\n" );

	cm.numverts = l->filelen / sizeof( *in );
	cm.vertices = out = Mem_Alloc( cms.mempool, cm.numverts * sizeof( *out ));

	for( i = 0; i < cm.numverts; i++, in++ )
	{
		out[i][0] = LittleFloat( in->point[0] );
		out[i][1] = LittleFloat( in->point[1] );
		out[i][2] = LittleFloat( in->point[2] );
	}
}

/*
=================
BSP_LoadNodes
=================
*/
static void BSP_LoadNodes( lump_t *l )
{
	dnode_t	*in;
	cnode_t	*out;
	int	i;
	
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadNodes: funny lump size\n" );
	cm.numnodes = l->filelen / sizeof( *in );

	if( cm.numnodes < 1 ) Host_Error( "Map %s has no nodes\n", cm.name );
	out = cm.nodes = (cnode_t *)Mem_Alloc( cms.mempool, cm.numnodes * sizeof( *out ));

	for( i = 0; i < cm.numnodes; i++, out++, in++ )
	{
		out->plane = cm.planes + LittleLong( in->planenum );
		out->children[0] = LittleLong( in->children[0] );
		out->children[1] = LittleLong( in->children[1] );
	}
}

/*
=================
BSP_LoadBrushes
=================
*/
static void BSP_LoadBrushes( lump_t *l )
{
	dbrush_t		*in;
	cbrush_t		*out;
	int		i, count;
	int		shadernum;
	
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadBrushes: funny lump size\n" );
	count = l->filelen / sizeof( *in );
	out = cm.brushes = (cbrush_t *)Mem_Alloc( cms.mempool, count * sizeof( *out ));
	cm.numbrushes = count;

	for( i = 0; i < count; i++, out++, in++ )
	{
		out->brushsides = cm.brushsides + LittleLong( in->firstside );
		out->numsides = LittleLong( in->numsides );
		shadernum = LittleLong( in->shadernum );

		if( shadernum < 0 || shadernum >= cm.numshaders )
			Host_Error( "BSP_LoadBrushes: bad shadernum: #%i\n", shadernum );
		out->contents = cm.shaders[shadernum].contentFlags;
	}
}

/*
=================
BSP_LoadMarkSurffaces
=================
*/
static void BSP_LoadMarkSurfaces( lump_t *l )
{
	dleafface_t	*in;
	cface_t		**out;
	int		i, j, count;
	
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadLeafFaces: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	if( count < 1 ) Host_Error( "Map %s has no leaffaces\n", cm.name );
	out = cm.markfaces = (cface_t **)Mem_Alloc( cms.mempool, count * sizeof( *out ));
	cm.nummarkfaces = count;

	for( i = 0; i < count; i++ )
	{
		j = LittleLong( in[i] );
		if( j < 0 ||  j >= cm.numfaces )
			Host_Error( "BSP_LoadLeafFaces: bad surfacenum #%i\n", j );
		out[i] = cm.faces + j;
	}
}

/*
=================
BSP_LoadMarkBrushes
=================
*/
static void BSP_LoadMarkBrushes( lump_t *l )
{
	dleafbrush_t	*in;
	cbrush_t		**out;
	int		i, count;
	
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadLeafBrushes: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	if( count < 1 ) Host_Error( "Map %s with no leaf brushes\n", cm.name );
	out = cm.markbrushes = (cbrush_t **)Mem_Alloc( cms.mempool, count * sizeof( *out ));
	cm.nummarkbrushes = count;

	for( i = 0; i < count; i++, in++ )
		out[i] = cm.brushes + LittleLong( *in );
}


/*
=================
BSP_LoadLeafs
=================
*/
static void BSP_LoadLeafs( lump_t *l )
{
	dleaf_t 	*in;
	cleaf_t	*out;
	int	i, j, k, count;
		
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadLeafs: funny lump size\n" );

	count = l->filelen / sizeof( *in );
	if( count < 1 ) Host_Error( "Map %s with no leafs\n", cm.name );
	out = cm.leafs = (cleaf_t *)Mem_Alloc( cms.mempool, count * sizeof( *out ));
	cm.numleafs = count;
	cm.numareas = 1;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->contents = 0;
		out->cluster = LittleLong( in->cluster );
		out->area = LittleLong( in->area ) + 1;
		out->markbrushes = cm.markbrushes + LittleLong( in->firstleafbrush );
		out->nummarkbrushes = LittleLong( in->numleafbrushes );
		out->markfaces = cm.markfaces + LittleLong( in->firstleafsurface );
		out->nummarkfaces = LittleLong( in->numleafsurfaces );

		// or brushes' contents
		for( j = 0; j < out->nummarkbrushes; j++ )
			out->contents |= out->markbrushes[j]->contents;

		// exclude markfaces that have no facets
		// so we don't perform this check at runtime
		for( j = 0; j < out->nummarkfaces; )
		{
			k = j;
			if( !out->markfaces[j]->facets )
			{
				for( ; (++j < out->nummarkfaces) && !out->markfaces[j]->facets; );
				if( j < out->nummarkfaces )
					memmove( &out->markfaces[k], &out->markfaces[j], (out->nummarkfaces - j) * sizeof( *out->markfaces ));
				out->nummarkfaces -= j - k;

			}
			j = k + 1;
		}

		// OR patches' contents
		for( j = 0; j < out->nummarkfaces; j++ )
			out->contents |= out->markfaces[j]->contents;

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
static void BSP_LoadPlanes( lump_t *l )
{
	dplane_t	*in;
	cplane_t	*out;
	int	i, j, count;
	
	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadPlanes: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	if( count < 1 ) Host_Error( "Map %s with no planes\n", cm.name );
	out = cm.planes = (cplane_t *)Mem_Alloc( cms.mempool, count * sizeof( *out ));
	cm.numplanes = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->signbits = 0;
		out->type = PLANE_NONAXIAL;

		for( j = 0; j < 3; j++ )
		{
			out->normal[j] = LittleFloat( in->normal[j] );
			if( out->normal[j] < 0.0f ) out->signbits |= (1 << j);
			if( out->normal[j] == 1.0f ) out->type = j;
		}
		out->dist = LittleFloat( in->dist );
	}
}

/*
=================
BSP_LoadBrushSides
=================
*/
static void IBSP_LoadBrushSides( lump_t *l )
{
	dbrushsideq_t 	*in;
	cbrushside_t	*out;
	int		i, count;

	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in )) Host_Error( "BSP_LoadBrushSides: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	out = cm.brushsides = (cbrushside_t *)Mem_Alloc( cms.mempool, count * sizeof( *out ));
	cm.numbrushsides = count;

	for( i = 0; i < count; i++, in++, out++)
	{
		out->plane = cm.planes + LittleLong( in->planenum );
		out->shadernum = LittleLong( in->shadernum );

		if( out->shadernum < 0 || out->shadernum >= cm.numshaders )
			Host_Error( "BSP_LoadBrushSides: bad shadernum: #%i\n", out->shadernum );
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

	out = cm.brushsides = (cbrushside_t *)Mem_Alloc( cms.mempool, count * sizeof( *out ));
	cm.numbrushsides = count;

	for( i = 0; i < count; i++, in++, out++)
	{
		out->plane = cm.planes + LittleLong( in->planenum );
		out->shadernum = LittleLong( in->shadernum );

		if( out->shadernum < 0 || out->shadernum >= cm.numshaders )
			Host_Error( "BSP_LoadBrushSides: bad shadernum: #%i\n", out->shadernum );
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
static void BSP_LoadSurface( cface_t *out, int shadernum, int firstvert, int numverts, int *patch_cp )
{
	dshader_t	*shader;

	shadernum = LittleLong( shadernum );
	if( shadernum < 0 || shadernum >= cm.numshaders )
		return;

	shader = &cm.shaders[shadernum];
	if( !shader->contentFlags || (shader->surfaceFlags & SURF_NONSOLID ))
		return;

	patch_cp[0] = LittleLong( patch_cp[0] );
	patch_cp[1] = LittleLong( patch_cp[1] );
	if( patch_cp[0] <= 0 || patch_cp[1] <= 0 )
		return;

	firstvert = LittleLong( firstvert );
	if( numverts <= 0 || firstvert < 0 || firstvert >= cm.numverts )
		return;

	CM_CreatePatch( out, shader, cm.vertices + firstvert, patch_cp );
}

static void IBSP_LoadSurfaces( lump_t *l )
{
	dsurfaceq_t	*in;
	cface_t		*out;
	int		i, count;

	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in ))
		Host_Error( "BSP_LoadSurfaces: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	if( count < 1 ) Host_Error( "Map %s with no faces\n", cm.name );
	out = cm.faces = Mem_Alloc( cms.mempool, count * sizeof( *out ));
	cm.numfaces = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		if( LittleLong( in->facetype ) != MST_PATCH ) continue;
		BSP_LoadSurface( out, in->shadernum, in->firstvert, in->numverts, in->patch_cp );
	}
}

static void RBSP_LoadSurfaces( lump_t *l )
{
	dsurfacer_t	*in;
	cface_t		*out;
	int		i, count;

	in = (void *)(cms.base + l->fileofs);
	if( l->filelen % sizeof( *in ))
		Host_Error( "BSP_LoadSurfaces: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	if( count < 1 ) Host_Error( "Map %s with no faces\n", cm.name );
	out = cm.faces = Mem_Alloc( cms.mempool, count * sizeof( *out ));
	cm.numfaces = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		if( LittleLong( in->facetype ) != MST_PATCH ) continue;
		BSP_LoadSurface( out, in->shadernum, in->firstvert, in->numverts, in->patch_cp );
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
		sv_models[1] = NULL; // no worldmodel
		if( checksum ) *checksum = 0;
		return;
	}

	if( !com.strcmp( cm.name, name ) && cms.loaded )
	{
		// singleplayer mode: server already loading map
		if( checksum ) *checksum = cm.checksum;
		if( !clientload )
		{
			// rebuild portals for server ...
			Mem_Set( cm.areaportals, 0, sizeof( cm.areaportals ));
			CM_FloodAreaConnections();

			// ... and reset entity script
			Com_ResetScript( cm.entityscript );
		}
		sv_models[1] = cm.models; // make link to world

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
	cm.checksum = LittleLong( Com_BlockChecksum( buf, length ));

	if( checksum ) *checksum = cm.checksum;
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
	BSP_LoadShaders( &hdr->lumps[LUMP_SHADERS] );
	BSP_LoadPlanes( &hdr->lumps[LUMP_PLANES] );
	if( raven_bsp || ( hdr->version == IGIDBSP_VERSION && !xreal_bsp ))
		RBSP_LoadBrushSides( &hdr->lumps[LUMP_BRUSHSIDES] );
	else IBSP_LoadBrushSides( &hdr->lumps[LUMP_BRUSHSIDES] );
	BSP_LoadBrushes( &hdr->lumps[LUMP_BRUSHES] );
	BSP_LoadMarkBrushes( &hdr->lumps[LUMP_LEAFBRUSHES] );	

	if( raven_bsp )
	{
		RBSP_LoadVertexes( &hdr->lumps[LUMP_VERTEXES] );
		RBSP_LoadSurfaces( &hdr->lumps[LUMP_SURFACES] );
	}
	else if( xreal_bsp )
	{
		XBSP_LoadVertexes( &hdr->lumps[LUMP_VERTEXES] );
		IBSP_LoadSurfaces( &hdr->lumps[LUMP_SURFACES] );
	}
	else
	{
		IBSP_LoadVertexes( &hdr->lumps[LUMP_VERTEXES] );
		IBSP_LoadSurfaces( &hdr->lumps[LUMP_SURFACES] );
	}

	BSP_LoadMarkSurfaces( &hdr->lumps[LUMP_LEAFSURFACES] );
	BSP_LoadLeafs( &hdr->lumps[LUMP_LEAFS] );
	BSP_LoadNodes( &hdr->lumps[LUMP_NODES] );
	BSP_LoadModels( &hdr->lumps[LUMP_MODELS] );
	BSP_LoadVisibility( &hdr->lumps[LUMP_VISIBILITY] );
	BSP_LoadEntityString( &hdr->lumps[LUMP_ENTITIES] );
	if( cm.numverts ) Mem_Free( cm.vertices );

	CM_InitBoxHull ();

	Mem_Set( cm.areaportals, 0, sizeof( cm.areaportals ));
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
		return box_cmodel;
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
CM_ModelType
===================
*/
modtype_t CM_ModelType( model_t handle )
{
	cmodel_t	*mod = CM_ClipHandleToModel( handle );

	if( !mod ) return mod_bad;
	return mod->type;
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
		MsgDev( D_ERROR, "Mod_GetBounds: NULL model %i\n", handle );
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

	if( index < 0 || index > MAX_MODELS )
		return false;

	// this array used for acess to servermodels
	mod = CM_ModForName( name );
	sv_models[index] = mod;

	return (mod != NULL);
}