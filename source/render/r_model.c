//=======================================================================
//			Copyright XashXT Group 2007 ©
//		      r_model.c - model loading and caching
//=======================================================================

#include <stdio.h>		// sscanf support
#include "r_local.h"
#include "byteorder.h"
#include "mathlib.h"
#include "matrixlib.h"
#include "const.h"

typedef struct
{
	string	name;
	int	flags;
	int	shaderType;
	shader_t	*shader;
} mipRef_t;

typedef struct loadmodel_s
{
	string	name;
	rmodel_t	*mod;		// pointer to current mod
	int	numVerts;
	vec4_t	*points;		// vertexes
	vec4_t	*normals;		// normals
	vec2_t	*st;		// texture coords
	vec2_t	*lmst[LM_STYLES];	// lightmap texture coords
	vec4_t	*colors[LM_STYLES];	// colors used for vertex lighting
	int	numIndices;
	uint	*indices;
	int	numLightmaps;
	lmrect_t	*lmRects;
	int	numMiptex;
	mipRef_t	*miptex;
} loadmodel_t;

// the inline models from the current map are kept separate
static rmodel_t	*mod_inline;
static byte	r_fullvis[MAX_MAP_LEAFS/8];
static rmodel_t	r_models[MAX_MODELS];
int		registration_sequence;
mbrushmodel_t	*r_worldBrushModel;
loadmodel_t	*m_pLoadModel;
static int	r_nummodels;

/*
===============
R_PointInLeaf
===============
*/
mleaf_t *R_PointInLeaf( const vec3_t p )
{
	mnode_t		*node;
	cplane_t		*plane;

	if( !r_worldBrushModel || !r_worldBrushModel->nodes )
		Host_Error( "R_PointInLeaf: bad model\n" );

	node = r_worldBrushModel->nodes;
	do
	{
		plane = node->plane;
		node = node->children[PlaneDiff( p, plane ) < 0];
	}
	while( node->plane != NULL );

	return (mleaf_t *)node;
}

/*
=================
R_DecompressVis
=================
*/
static byte *R_DecompressVis( const byte *in )
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	byte		*out;
	int		c, row;

	row = (r_worldBrushModel->vis->numclusters+7)>>3;	
	out = decompressed;

	if( !in )
	{
		// no vis info, so make all visible
		while( row )
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;		
	}

	do
	{
		if( *in )
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		while( c )
		{
			*out++ = 0;
			c--;
		}
	} while( out - decompressed < row );
	
	return decompressed;
}

/*
=================
R_ClusterPVS
=================
*/
byte *R_ClusterPVS( int cluster )
{
	if (!r_worldBrushModel || !r_worldBrushModel->vis || cluster < 0 || cluster >= r_worldBrushModel->vis->numclusters )
		return r_fullvis;
	return R_DecompressVis((byte *)r_worldBrushModel->vis + r_worldBrushModel->vis->bitofs[cluster][DVIS_PVS]);
}

/*
=======================================================================

BRUSH MODELS

=======================================================================
*/
static byte *mod_base;
static mbrushmodel_t *m_pLoadBmodel;

/*
=================
Mod_CheckDeluxemaps
=================
*/
static void Mod_CheckDeluxemaps( const lump_t *l, byte *lmData )
{
	int		i, j;
	int		surfaces, lightmap;
	dsurface_t	*in;

	// there are no deluxemaps in the map if the number of lightmaps is
	// less than 2 or odd
	if( m_pLoadModel->numLightmaps < 2 || m_pLoadModel->numLightmaps & 1 )
		return;

	in = (void *)(mod_base + l->fileofs);
	surfaces = l->filelen / sizeof( *in );

	for( i = 0; i < surfaces; i++, in++ )
	{
		for( j = 0; j < LM_STYLES; j++ )
		{
			lightmap = LittleLong( in->lmapNum[j] );
			if( lightmap <= 0 ) continue;
			if( lightmap & 1 ) return;
		}
	}

	// check if the deluxemap is actually empty (q3map2, yay!)
	if( m_pLoadModel->numLightmaps == 2 )
	{
		// FIXME: replace with actual lightmap sizes
		int	lW = LIGHTMAP_WIDTH;
		int	lH = LIGHTMAP_HEIGHT;

		lmData += LM_SIZE;
		for( i = lW * lH; i > 0; i--, lmData += LIGHTMAP_BITS )
		{
			for( j = 0; j < LIGHTMAP_BITS; j++ )
			{
				if( lmData[j] != 0x00f ) break;
			}
			if( j != LIGHTMAP_BITS ) break;
		}

		// empty deluxemap
		if( !i )
		{
			m_pLoadModel->numLightmaps = 1;
			return;
		}
	}

	mapConfig.deluxeMaps = true;
	if( GL_Support( R_SHADER_GLSL100_EXT ))
		mapConfig.deluxeMappingEnabled = true;
}

/*
=================
Mod_LoadLighting
=================
*/
static void Mod_LoadLighting( const lump_t *faces )
{
	rgbdata_t	*lmap;
	size_t	size = 0;
	string	lmap_name;
	byte	*pixels;		// pack RGB buffer
	uint	i, lm_width = 0, lm_height = 0;
	
	if( m_pLoadModel->numLightmaps <= 0 ) return;
	m_pLoadModel->lmRects = Mem_Alloc( m_pLoadModel->mod->mempool, m_pLoadModel->numLightmaps * sizeof( *m_pLoadModel->lmRects ));

	// create one temp buffer for all lightmaps
	for( i = 0; i < m_pLoadModel->numLightmaps; i++ )
	{
		com.sprintf( lmap_name, "gfx/lightmaps/%s_%04d.tga", m_pLoadModel->name, i );
		lmap = FS_LoadImage( lmap_name, NULL, 0 );
		if( !lmap ) continue;	// potentially crash point ?

		Image_ExpandRGB( lmap );	// transform to RGB
		lm_width = lmap->width;
		lm_height = lmap->height;
		pixels = Mem_Realloc( r_temppool, pixels, size + lmap->size );
		Mem_Copy( pixels + size, lmap->buffer, lmap->size );
		size += lmap->size;

		Mem_Free( lmap );
	}

	Mod_CheckDeluxemaps( faces, pixels );

	// set overbright bits for lightmaps and lightgrid
	// deluxemapped maps have zero scale because most surfaces
	// have a gloss stage that makes them look brighter anyway
	if( !r_hwgamma->integer ) mapConfig.pow2MapOvrbr = r_mapoverbrightbits->integer;
	else mapConfig.pow2MapOvrbr = r_mapoverbrightbits->integer - r_overbrightbits->integer;
	if( mapConfig.pow2MapOvrbr < 0 ) mapConfig.pow2MapOvrbr = 0;

	R_BuildLightmaps( m_pLoadModel->numLightmaps, lm_width, lm_height, pixels, m_pLoadModel->lmRects );
}

/*
=================
Mod_LoadVertexes
=================
*/
static void Mod_LoadVertexes( const lump_t *l )
{
	int	i, count, j;
	dvertex_t	*in;
	float	*out_points, *out_normals, *out_st, *out_lmst[LM_STYLES], *out_colors[LM_STYLES];
	byte	*buffer;
	size_t	bufSize;
	vec3_t	color, fcolor;
	float	div;

	in = (void *)( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadVertexes: funny lump size in %s", m_pLoadModel->name );
	count = l->filelen / sizeof( *in );

	bufSize = 0;
	bufSize += count * (sizeof(vec4_t) + sizeof(vec4_t) + sizeof(vec2_t) + (sizeof(vec2_t) + sizeof( vec4_t )) * LM_STYLES );
	buffer = Mem_Alloc( m_pLoadModel->mod->mempool, bufSize );

	m_pLoadModel->numVerts = count;
	m_pLoadModel->points = (vec4_t *)buffer;
	buffer += count * sizeof( vec4_t );
	m_pLoadModel->normals = (vec4_t *)buffer;
	buffer += count * sizeof( vec4_t );
	m_pLoadModel->st = (vec2_t *)buffer;
	buffer += count * sizeof( vec2_t );

	for( i = 0; i < LM_STYLES; i++ )
	{
		m_pLoadModel->lmst[i] = ( vec2_t * )buffer;
		buffer += count * sizeof( vec2_t );
		m_pLoadModel->colors[i] = ( vec4_t * )buffer;
		buffer += count * sizeof( vec4_t );
	}

	out_points = m_pLoadModel->points[0];
	out_normals = m_pLoadModel->normals[0];
	out_st = m_pLoadModel->st[0];

	for( i = 0; i < LM_STYLES; i++ )
	{
		out_lmst[i] = m_pLoadModel->lmst[i][0];
		out_colors[i] = m_pLoadModel->colors[i][0];
	}

	if( r_mapoverbrightbits->integer > 0 )
		div = (float)( 1<<r_mapoverbrightbits->integer ) / 255.0f;
	else div = 1.0f / 255.0f;

	for( i = 0; i < count; i++, in++, out_points += 4, out_normals += 4, out_st += 2 )
	{
		for( j = 0; j < 3; j++ )
		{
			out_points[j] = LittleFloat( in->point[j] );
			out_normals[j] = LittleFloat( in->normal[j] );
		}

		out_points[3] = 1;
		out_normals[3] = 0;

		for( j = 0; j < 2; j++ )
			out_st[j] = LittleFloat( in->st[j] );

		for( j = 0; j < LM_STYLES; out_lmst[j] += 2, out_colors[j] += 4, j++ )
		{
			out_lmst[j][0] = LittleFloat( in->lm[j][0] );
			out_lmst[j][1] = LittleFloat( in->lm[j][1] );

			if( r_fullbright->integer )
			{
				out_colors[j][0] = 1.0f;
				out_colors[j][1] = 1.0f;
				out_colors[j][2] = 1.0f;
				out_colors[j][3] = in->color[j][3] / 255.0f;
			}
			else
			{
				color[0] = (( float )in->color[j][0] * div );
				color[1] = (( float )in->color[j][1] * div );
				color[2] = (( float )in->color[j][2] * div );
				ColorNormalize( color, fcolor );

				out_colors[j][0] = fcolor[0];
				out_colors[j][1] = fcolor[1];
				out_colors[j][2] = fcolor[2];
				out_colors[j][3] = in->color[j][3] / 255.0f;
			}
		}
	}
}

/*
=================
Mod_LoadIndices
=================
*/
static void Mod_LoadIndices( const lump_t *l )
{
	int	*in;

	in = (int *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadIndices: funny lump size in '%s'\n", m_pLoadModel->name );

	m_pLoadModel->numIndices = l->filelen / sizeof( *in );
	m_pLoadModel->indices = Mem_Alloc( m_pLoadModel->mod->mempool, m_pLoadModel->numIndices * sizeof( *in ));
	Mem_Copy( m_pLoadModel->indices, in, l->filelen );
	SwapBlock((uint *)m_pLoadModel->indices, m_pLoadModel->numIndices * sizeof( *in ));
}

/*
=================
Mod_LoadPlanes
=================
*/
static void Mod_LoadPlanes( const lump_t *l )
{
	dplane_t		*in;
	cplane_t		*out;
	int		i;
	
	in = (dplane_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(dplane_t))
		Host_Error( "Mod_LoadPlanes: funny lump size in '%s'\n", m_pLoadModel->name );

	m_pLoadBmodel->numPlanes = l->filelen / sizeof(dplane_t);
	m_pLoadBmodel->planes = out = Mem_Alloc( m_pLoadModel->mod->mempool, m_pLoadBmodel->numPlanes * sizeof(cplane_t));

	for( i = 0; i < m_pLoadBmodel->numPlanes; i++, in++, out++ )
	{
		out->normal[0] = LittleFloat( in->normal[0] );
		out->normal[1] = LittleFloat( in->normal[1] );
		out->normal[2] = LittleFloat( in->normal[2] );
		out->dist = LittleFloat( in->dist );
		PlaneClassify( out );
	}
}

/*
=================
Mod_LoadShaders
=================
*/
void Mod_LoadShaders( const lump_t *l )
{
	dshader_t		*in;
	mipRef_t		*out;
	int 		i, count;
	int		contents;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error( "Mod_LoadShaders: funny lump size in '%s'\n", m_pLoadModel->name );
	count = l->filelen / sizeof(*in);

	out = (mipRef_t *)Mem_Alloc( m_pLoadModel->mod->mempool, count * sizeof( *out ));

	m_pLoadModel->mod->numShaders = count;
	m_pLoadModel->numMiptex = count;
	m_pLoadModel->miptex = out;

	for ( i = 0; i < count; i++, in++, out++ )
	{
		com.strncpy( out->name, in->name, sizeof( out->name ));
		out->flags = LittleLong( in->surfaceFlags );
		contents = LittleLong( in->contents );

		// detect surfaceParm
		if( contents & ( MASK_WATER|CONTENTS_FOG ) )
			out->flags |= SURF_NOMARKS;
		if( !m_pLoadModel->numLightmaps )
			out->flags |= SURF_NOLIGHTMAP;
		out->shader = r_defaultShader;
	}
}

/*
=================
Mod_LoadNodes
=================
*/
static void Mod_LoadNodes( const lump_t *l )
{
	int	i, j, count, p;
	dnode_t	*in;
	mnode_t	*out;
	bool	badBounds;

	in = (void *)( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadNodes: funny lump size in %s\n", m_pLoadModel->name );
	count = l->filelen / sizeof( *in );
	out = Mem_Alloc( m_pLoadModel->mod->mempool, count * sizeof( *out ));

	m_pLoadBmodel->nodes = out;
	m_pLoadBmodel->numnodes = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		p = LittleLong( in->planenum );
		if( p < 0 || p >= m_pLoadBmodel->numPlanes )
			Host_Error( "Mod_LoadNodes: bad planenum %i\n", p );
		out->plane = m_pLoadBmodel->planes + p;

		for( j = 0; j < 2; j++ )
		{
			p = LittleLong( in->children[j] );
			if( p >= 0 ) out->children[j] = m_pLoadBmodel->nodes + p;
			else out->children[j] = (mnode_t *)(m_pLoadBmodel->leafs + ( -1 - p ));
		}

		badBounds = false;
		for( j = 0; j < 3; j++ )
		{
			out->mins[j] = LittleFloat( in->mins[j] );
			out->maxs[j] = LittleFloat( in->maxs[j] );
			if( out->mins[j] > out->maxs[j] ) badBounds = true;
		}

		if( badBounds || VectorCompare( out->mins, out->maxs ))
		{
			MsgDev( D_WARN, "bad node %i bounds:\n", i );
			MsgDev( D_WARN, "mins: %i %i %i\n", rint( out->mins[0] ), rint( out->mins[1] ), rint( out->mins[2] ) );
			MsgDev( D_WARN, "maxs: %i %i %i\n", rint( out->maxs[0] ), rint( out->maxs[1] ), rint( out->maxs[2] ) );
		}
	}
}

/*
=================
Mod_LoadLeafs
=================
*/
static void Mod_LoadLeafs( const lump_t *leafs, const lump_t *lfaces )
{
	int	i, j, k, count, countMarkSurfaces;
	dleaf_t	*in;
	mleaf_t	*out;
	size_t	size;
	byte	*buffer;
	bool	badBounds;
	int	*inMarkSurfaces;
	int	numVisLeafs;
	int	numMarkSurfaces, firstMarkSurface;
	int	numVisSurfaces, numFragmentSurfaces;

	inMarkSurfaces = (void *)(mod_base + lfaces->fileofs);
	if( lfaces->filelen % sizeof( *inMarkSurfaces ) )
		Host_Error( "Mod_LoadMarksurfaces: funny lump size in %s\n", m_pLoadModel->name );
	countMarkSurfaces = lfaces->filelen / sizeof( *inMarkSurfaces );

	in = (void *)(mod_base + leafs->fileofs);
	if( leafs->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadLeafs: funny lump size in %s\n", m_pLoadModel->name );
	count = lfaces->filelen / sizeof( *in );
	out = Mem_Alloc( m_pLoadModel->mod->mempool, count * sizeof( *out ));

	m_pLoadBmodel->leafs = out;
	m_pLoadBmodel->numleafs = count;

	numVisLeafs = 0;
	m_pLoadBmodel->visleafs = Mem_Alloc( m_pLoadModel->mod->mempool, ( count + 1 ) * sizeof( out ));

	for( i = 0; i < count; i++, in++, out++ )
	{
		badBounds = false;
		for( j = 0; j < 3; j++ )
		{
			out->mins[j] = LittleFloat( in->mins[j] );
			out->maxs[j] = LittleFloat( in->maxs[j] );
			if( out->mins[j] > out->maxs[j] ) badBounds = true;
		}
		out->cluster = LittleLong( in->cluster );

		if( i && ( badBounds || VectorCompare( out->mins, out->maxs )))
		{
			MsgDev( D_WARN, "bad leaf %i bounds:\n", i );
			MsgDev( D_WARN, "mins: %i %i %i\n", rint(out->mins[0]), rint(out->mins[1]), rint(out->mins[2]));
			MsgDev( D_WARN, "maxs: %i %i %i\n", rint(out->maxs[0]), rint(out->maxs[1]), rint(out->maxs[2]));
			MsgDev( D_WARN, "cluster: %i\n", LittleLong( in->cluster ));
			MsgDev( D_WARN, "surfaces: %i\n", LittleLong( in->numleafsurfaces ));
			MsgDev( D_WARN, "brushes: %i\n", LittleLong( in->numleafbrushes ));
			out->cluster = -1;
		}

		if( m_pLoadBmodel->vis )
		{
			if( out->cluster >= m_pLoadBmodel->vis->numclusters )
				Host_Error( "MOD_LoadBmodel: leaf cluster > numclusters" );
		}

		out->plane = NULL;
		out->area = LittleLong( in->area ) + 1;

		numMarkSurfaces = LittleLong( in->numleafsurfaces );
		if( !numMarkSurfaces ) continue;

		firstMarkSurface = LittleLong( in->firstleafsurface );
		if( firstMarkSurface < 0 || numMarkSurfaces + firstMarkSurface > countMarkSurfaces )
			Host_Error( "MOD_LoadMarksurfaces: bad marksurfaces in leaf %i\n", i );

		numVisSurfaces = numFragmentSurfaces = 0;
		for( j = 0; j < numMarkSurfaces; j++ )
		{
			k = LittleLong( inMarkSurfaces[firstMarkSurface + j] );
			if( k < 0 || k >= m_pLoadBmodel->numsurfaces )
				Host_Error( "Mod_LoadMarksurfaces: bad surface number %i\n", k );

			if( R_SurfPotentiallyVisible( m_pLoadBmodel->surfaces + k ))
			{
				numVisSurfaces++;
				if( R_SurfPotentiallyFragmented( m_pLoadBmodel->surfaces + k ))
					numFragmentSurfaces++;
			}
		}

		if( !numVisSurfaces ) continue;

		size = numVisSurfaces + 1;
		if( numFragmentSurfaces )
			size += numFragmentSurfaces + 1;
		size *= sizeof( msurface_t * );

		buffer = (byte *)Mem_Alloc( m_pLoadModel->mod->mempool, size );

		out->firstVisSurface = (msurface_t **)buffer;
		buffer += (numVisSurfaces + 1) * sizeof( msurface_t* );
		if( numFragmentSurfaces )
		{
			out->firstFragmentSurface = ( msurface_t ** )buffer;
			buffer += ( numFragmentSurfaces + 1 ) * sizeof( msurface_t* );
		}

		numVisSurfaces = numFragmentSurfaces = 0;

		for( j = 0; j < numMarkSurfaces; j++ )
		{
			k = LittleLong( inMarkSurfaces[firstMarkSurface + j] );

			if( R_SurfPotentiallyVisible( m_pLoadBmodel->surfaces + k ))
			{
				out->firstVisSurface[numVisSurfaces++] = m_pLoadBmodel->surfaces + k;
				if( R_SurfPotentiallyFragmented( m_pLoadBmodel->surfaces + k ))
					out->firstFragmentSurface[numFragmentSurfaces++] = m_pLoadBmodel->surfaces + k;
			}
		}
		m_pLoadBmodel->visleafs[numVisLeafs++] = out;
	}

	m_pLoadBmodel->visleafs = Mem_Realloc( m_pLoadModel->mod->mempool, m_pLoadBmodel->visleafs, (numVisLeafs + 1) * sizeof( out ));
}

/*
=================
Mod_CreateMeshForSurface
=================
*/
static rb_mesh_t *Mod_CreateMeshForSurface( const dsurface_t *in, msurface_t *out )
{
	rb_mesh_t	*mesh = NULL;
	bool	createSTverts;
	byte	*buffer;
	size_t	bufSize;

	if(( mapConfig.deluxeMappingEnabled && !(LittleLong( in->lmapNum[0] ) < 0 || in->lStyles[0] == 255) ) || ( out->shader->flags & SHADER_PORTAL_CAPTURE2 ))
		createSTverts = true;
	else createSTverts = false;

	switch( out->faceType )
	{
	case MST_FLARE:
		{
			int	i;

			for( i = 0; i < 3; i++ )
			{
				out->origin[i] = LittleFloat( in->flare.origin[i] );
				out->color[i] = bound( 0, LittleFloat( in->flare.color[i] ), 1.0f );
			}
			break;
		}
	case MST_PATCH:
		{
			int	i, j, u, v, p;
			int	patch_cp[2], step[2], size[2], flat[2];
			float	subdivLevel, f;
			int	numVerts, firstVert;
			vec4_t	tempv[MAX_ARRAY_VERTS];
			vec4_t	colors[MAX_ARRAY_VERTS];
			uint	*elems;

			patch_cp[0] = LittleLong( in->patch.width );
			patch_cp[1] = LittleLong( in->patch.height );

			if( !patch_cp[0] || !patch_cp[1] )
				break;

			subdivLevel = 4; // r_subdivisions->value;
			if( subdivLevel < 1 ) subdivLevel = 1;

			numVerts = LittleLong( in->numvertices );
			firstVert = LittleLong( in->firstvertex );

			// find the degree of subdivision in the u and v directions
			Patch_GetFlatness( subdivLevel, (vec_t *)m_pLoadModel->points[firstVert], 4, patch_cp, flat );

			// allocate space for mesh
			step[0] = (1<<flat[0]);
			step[1] = (1<<flat[1]);
			size[0] = (patch_cp[0]>>1) * step[0] + 1;
			size[1] = (patch_cp[1]>>1) * step[1] + 1;
			numVerts = size[0] * size[1];

			if( numVerts > MAX_ARRAY_VERTS )
				break;

			bufSize = sizeof( rb_mesh_t ) + numVerts * (sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec2_t ));
			for( j = 0; j < LM_STYLES && in->lStyles[j] != 255; j++ )
				bufSize += numVerts * sizeof( vec2_t );
			for( j = 0; j < LM_STYLES && in->vStyles[j] != 255; j++ )
				bufSize += numVerts * sizeof( vec4_t );
			if( createSTverts ) bufSize += numVerts * sizeof( vec4_t );
			buffer = ( byte * )Mem_Alloc( m_pLoadModel->mod->mempool, bufSize );

			mesh = ( rb_mesh_t * )buffer;
			buffer += sizeof( rb_mesh_t );
			mesh->numVerts = numVerts;
			mesh->points = (vec4_t *)buffer;
			buffer += numVerts * sizeof( vec4_t );
			mesh->normal = (vec4_t *)buffer;
			buffer += numVerts * sizeof( vec4_t );
			mesh->st = (vec2_t *)buffer;
			buffer += numVerts * sizeof( vec2_t );

			Patch_Evaluate( m_pLoadModel->points[firstVert], patch_cp, step, mesh->points[0], 4 );
			Patch_Evaluate( m_pLoadModel->normals[firstVert], patch_cp, step, mesh->normal[0], 4 );
			Patch_Evaluate( m_pLoadModel->st[firstVert], patch_cp, step, mesh->st[0], 2 );

			for( i = 0; i < numVerts; i++ )
				VectorNormalize( mesh->normal[i] );

			for( j = 0; j < LM_STYLES && in->lStyles[j] != 255; j++ )
			{
				mesh->lm[j] = (vec2_t *)buffer; buffer += numVerts * sizeof( vec2_t );
				Patch_Evaluate(m_pLoadModel->lmst[j][firstVert], patch_cp, step, mesh->lm[j][0], 2 );
			}

			for( j = 0; j < LM_STYLES && in->vStyles[j] != 255; j++ )
			{
				mesh->color[j] = (vec4_t *)buffer;
				buffer += numVerts * sizeof( vec4_t );
				for( i = 0; i < numVerts; i++ )
					Vector4Scale( m_pLoadModel->colors[j][firstVert + i], ( 1.0f / 255.0f ), colors[i] );
				Patch_Evaluate( colors[0], patch_cp, step, tempv[0], 4 );

				for( i = 0; i < numVerts; i++ )
				{
					f = max( max( tempv[i][0], tempv[i][1] ), tempv[i][2] );
					if( f > 1.0f ) f = 1.0f / f;
					else f = 1.0f;

					mesh->color[j][i][0] = tempv[i][0] * f;
					mesh->color[j][i][1] = tempv[i][1] * f;
					mesh->color[j][i][2] = tempv[i][2] * f;
					mesh->color[j][i][3] = bound( 0, tempv[i][3], 1 );
				}
			}

			// compute new elems
			mesh->numIndexes = (size[0] - 1) * (size[1] - 1) * 6;
			elems = mesh->indexes = (uint * )Mem_Alloc( m_pLoadModel->mod->mempool, mesh->numIndexes * sizeof( uint ));
			for( v = 0, i = 0; v < size[1] - 1; v++ )
			{
				for( u = 0; u < size[0] - 1; u++ )
				{
					p = v * size[0] + u;
					elems[0] = p;
					elems[1] = p + size[0];
					elems[2] = p + 1;
					elems[3] = p + 1;
					elems[4] = p + size[0];
					elems[5] = p + size[0] + 1;
					elems += 6;
				}
			}

			if( createSTverts )
			{
				mesh->sVectors = (vec4_t *)buffer;
				buffer += numVerts * sizeof( vec4_t );
				R_BuildTangentVectors( mesh->numVerts, mesh->points, mesh->normal, mesh->st, mesh->numIndexes / 3, mesh->indexes, mesh->sVectors );
			}
			break;
		}
	case MST_PLANAR:
	case MST_TRIANGLE_SOUP:
		{
			int	j, numVerts, firstVert, numElems, firstElem;

			numVerts = LittleLong( in->numvertices );
			firstVert = LittleLong( in->firstvertex );

			numElems = LittleLong( in->numindices );
			firstElem = LittleLong( in->firstindex );

			bufSize = sizeof( rb_mesh_t ) + numVerts * (sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec2_t ) + numElems * sizeof( uint ));
			for( j = 0; j < LM_STYLES && in->lStyles[j] != 255; j++ )
				bufSize += numVerts * sizeof( vec2_t );
			for( j = 0; j < LM_STYLES && in->vStyles[j] != 255; j++ )
				bufSize += numVerts * sizeof( vec4_t );
			if( createSTverts ) bufSize += numVerts * sizeof( vec4_t );
			if( out->faceType == MST_PLANAR ) bufSize += sizeof( cplane_t );

			buffer = ( byte * )Mem_Alloc( m_pLoadModel->mod->mempool, bufSize );

			mesh = ( rb_mesh_t * )buffer;
			buffer += sizeof( rb_mesh_t );
			mesh->numVerts = numVerts;
			mesh->numIndexes = numElems;

			mesh->points = ( vec4_t * )buffer;
			buffer += numVerts * sizeof( vec4_t );
			mesh->normal = ( vec4_t * )buffer;
			buffer += numVerts * sizeof( vec4_t );
			mesh->st = ( vec2_t * )buffer;
			buffer += numVerts * sizeof( vec2_t );

			Mem_Copy( mesh->points, m_pLoadModel->points + firstVert, numVerts * sizeof( vec4_t ));
			Mem_Copy( mesh->normal, m_pLoadModel->normals + firstVert, numVerts * sizeof( vec4_t ));
			Mem_Copy( mesh->st, m_pLoadModel->st + firstVert, numVerts * sizeof( vec2_t ));

			for( j = 0; j < LM_STYLES && in->lStyles[j] != 255; j++ )
			{
				mesh->lm[j] = ( vec2_t * )buffer;
				buffer += numVerts * sizeof( vec2_t );
				Mem_Copy( mesh->lm[j], m_pLoadModel->lmst[j] + firstVert, numVerts * sizeof( vec2_t ));
			}
			for( j = 0; j < LM_STYLES && in->vStyles[j] != 255; j++ )
			{
				mesh->color[j] = ( vec4_t * )buffer;
				buffer += numVerts * sizeof( vec4_t );
				Mem_Copy( mesh->color[j], m_pLoadModel->colors[j] + firstVert, numVerts * sizeof( vec4_t ));
			}

			mesh->indexes = (uint *)buffer;
			buffer += numElems * sizeof( uint );
			Mem_Copy( mesh->indexes, m_pLoadModel->indices + firstElem, numElems * sizeof( uint ));

			if( createSTverts )
			{
				mesh->sVectors = (vec4_t *)buffer;
				buffer += numVerts * sizeof( vec4_t );
				R_BuildTangentVectors( mesh->numVerts, mesh->points, mesh->normal, mesh->st, mesh->numIndexes / 3, mesh->indexes, mesh->sVectors );
			}

			if( out->faceType == MST_PLANAR )
			{
				cplane_t	*plane;

				plane = out->plane = (cplane_t *)buffer;
				buffer += sizeof( cplane_t );

				for( j = 0; j < 3; j++ )
					plane->normal[j] = LittleFloat( in->patch.normal[j] );
				PlaneClassify( plane );
				plane->dist = DotProduct( mesh->points[0], plane->normal );
			}
			break;
		}
	case MST_FOLIAGE:
		// FIXME: parse foliage surfaces properly
		break;
	}

	return mesh;
}

/*
=================
Mod_LoadSurfaceCommon
=================
*/
static void Mod_LoadSurfaceCommon( const dsurface_t *in, msurface_t *out )
{
	int		i, shaderType;
	rb_mesh_t		*mesh;
	mfog_t		*fog;
	mipRef_t		*mipRef;
	int		shadernum, fognum;
	float		*vert;
	lmrect_t		*lmRects[LM_STYLES];
	int		lightmaps[LM_STYLES];
	byte		lStyles[LM_STYLES];
	byte		vStyles[LM_STYLES];
	vec3_t		ebbox = { 0, 0, 0 };

	out->faceType = LittleLong( in->surfaceType );

	// lighting info
	for( i = 0; i < LM_STYLES; i++ )
	{
		lightmaps[i] = LittleLong( in->lmapNum[i] );
		// calculate lightmap count
		if( lightmaps[i] >= m_pLoadModel->numLightmaps )
			m_pLoadModel->numLightmaps = lightmaps[i] + 1;

		if( lightmaps[i] < 0 || out->faceType == MST_FLARE )
		{
			lmRects[i] = NULL;
			lightmaps[i] = -1;
			lStyles[i] = 255;
		}
		else if( lightmaps[i] >= m_pLoadModel->numLightmaps )
		{
			// FIXME: this is a death code
			MsgDev( D_ERROR, "bad lightmap number: %i\n", lightmaps[i] );
			lmRects[i] = NULL;
			lightmaps[i] = -1;
			lStyles[i] = 255;
		}
		else
		{
			lmRects[i] = &m_pLoadModel->lmRects[lightmaps[i]];
			lightmaps[i] = lmRects[i]->texNum;
			lStyles[i] = in->lStyles[i];
		}
		vStyles[i] = in->vStyles[i];
	}

	// add this super style
	R_AddSuperLightStyle( lightmaps, lStyles, vStyles, lmRects );

	shadernum = LittleLong( in->shadernum );
	if( shadernum < 0 || shadernum >= m_pLoadModel->numMiptex )
		Host_Error( "Mod_LoadFaceCommon: bad shader number %i\n", shadernum );
	mipRef = m_pLoadModel->miptex + shadernum;

	if( out->faceType == MST_FLARE )
		shaderType = SHADER_FLARE;
	else if( lightmaps[0] < 0 || lStyles[0] == 255 )
		shaderType = SHADER_VERTEX;
	else shaderType = SHADER_SURFACE;

	// set shader type
	mipRef->shaderType = shaderType;

	if( !mipRef->shader )
	{
		mipRef->shader = R_FindShader( mipRef->name, mipRef->shaderType, mipRef->flags );
		m_pLoadModel->mod->shaders[shadernum] = out->shader = mipRef->shader;
		if( out->faceType == MST_FLARE ) out->shader->flags |= SHADER_FLARE; // force SHADER_FLARE flag
	}

	out->flags = mipRef->flags;
	R_DeformVertexesBBoxForShader( out->shader, ebbox );

	fognum = LittleLong( in->fognum );
	if( fognum != -1 && ( fognum < m_pLoadBmodel->numFogs ))
	{
		fog = m_pLoadBmodel->fogs + fognum;
		if( fog->shader && fog->shader->fog_dist )
			out->fog = fog;
	}

	mesh = out->mesh = Mod_CreateMeshForSurface( in, out );
	if( !mesh ) return;

	ClearBounds( out->mins, out->maxs );
	for( i = 0, vert = mesh->points[0]; i < mesh->numVerts; i++, vert += 4 )
		AddPointToBounds( vert, out->mins, out->maxs );
	VectorSubtract( out->mins, ebbox, out->mins );
	VectorAdd( out->maxs, ebbox, out->maxs );
}

/*
=================
Mod_LoadLightgrid
=================
*/
static void Mod_LoadLightgrid( const lump_t *l )
{
	int		count;
	dlightgrid_t	*in;
	mlightgrid_t	*out;

	in = (void *)( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadLightgrid: funny lump size in %s\n", m_pLoadModel->name );
	count = l->filelen / sizeof( *in );
	out = Mem_Alloc( m_pLoadModel->mod->mempool, count * sizeof( *out ));

	m_pLoadBmodel->lightgrid = out;
	m_pLoadBmodel->numGridPoints = count;

	// lightgrid is all 8 bit
	Mem_Copy( out, in, count * sizeof( *out ));
}

/*
=================
Mod_LoadLightArray
=================
*/
static void Mod_LoadLightArray( const lump_t *l )
{
	int		i, count;
	short		*in;		// FIXME: update format to int ?
	mlightgrid_t	**out;

	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadLightArray: funny lump size in %s\n", m_pLoadModel->name );
	count = l->filelen / sizeof( *in );
	out = Mem_Alloc( m_pLoadModel->mod->mempool, count * sizeof( *out ));

	m_pLoadBmodel->lightarray = out;
	m_pLoadBmodel->numLightArrayPoints = count;

	for( i = 0; i < count; i++, in++, out++ )
		*out = m_pLoadBmodel->lightgrid + LittleShort( *in );
}

/*
=================
Mod_LoadEntities
=================
*/
static void Mod_LoadEntities( const lump_t *l, vec3_t gridSize, vec3_t ambient )
{
	int	n;
	char	*data;
	bool	isworld;
	float	gridsizef[3] = { 0, 0, 0 };
	float	colorf[3] = { 0, 0, 0 };
	float	ambientf = 0;
	char	key[MAX_KEY];
	char	value[MAX_VALUE];
	char	*token;

	VectorClear( gridSize );
	VectorClear( ambient );

	data = (char *)mod_base + l->fileofs;
	if( !data || !data[0] ) return;

	while(( token = Com_ParseToken( &data, true )) && token[0] == '{' )
	{
		isworld = false;

		while( 1 )
		{
			token = Com_ParseToken( &data, true );
			if( !token[0] ) break; // error
			if( token[0] == '}' ) break; // end of entity

			com.strncpy( key, token, sizeof( key ));
			while( key[com.strlen(key) - 1] == ' ' )  // remove trailing spaces
				key[com.strlen(key) - 1] = 0;

			token = Com_ParseToken( &data, false );
			if( !token[0] ) break; // error

			com.strncpy( value, token, sizeof( value ));

			// now that we have the key pair worked out...
			if( !com.strcmp( key, "classname" ) )
			{
				if( !com.strcmp( value, "worldspawn" ))
					isworld = true;
			}
			else if( !com.strcmp( key, "gridsize" ))
			{
				n = sscanf( value, "%f %f %f", &gridsizef[0], &gridsizef[1], &gridsizef[2] );
				if( n != 3 )
				{
					int	gridsizei[3] = { 0, 0, 0 };
					sscanf( value, "%i %i %i", &gridsizei[0], &gridsizei[1], &gridsizei[2] );
					VectorCopy( gridsizei, gridsizef );
				}
			}
			else if( !com.strcmp( key, "_ambient" ) || ( !com.strcmp( key, "ambient" ) && ambientf == 0.0f ))
			{
				sscanf( value, "%f", &ambientf );
				if( !ambientf )
				{
					int	ia = 0;
					n = sscanf( value, "%i", &ia );
					ambientf = ia;
				}
			}
			else if( !com.strcmp( key, "_color" ))
			{
				n = sscanf( value, "%f %f %f", &colorf[0], &colorf[1], &colorf[2] );
				if( n != 3 )
				{
					int	colori[3] = { 0, 0, 0 };
					sscanf( value, "%i %i %i", &colori[0], &colori[1], &colori[2] );
					VectorCopy( colori, colorf );
				}
			}
		}

		if( isworld )
		{
			VectorCopy( gridsizef, gridSize );

			if( VectorCompare( colorf, vec3_origin ) )
				VectorSet( colorf, 1.0, 1.0, 1.0 );
			VectorScale( colorf, ambientf, ambient );
			break;
		}
	}
}

/*
=================
Mod_LoadFogs
=================
*/
static void Mod_LoadFogs( const lump_t *fogs, const lump_t *brushes, const lump_t *brushsides )
{
	dfog_t		*in;
	mfog_t		*out;
	dbrush_t		*brush;
	dbrush_t		*inbrushes;
	dbrushside_t	*inbrushsides = NULL;
	dbrushside_t	*brushside = NULL;
	int		i, j, count, p;

	inbrushes = (void *)(mod_base + brushes->fileofs);
	if( brushes->filelen % sizeof( *inbrushes ))
		Host_Error( "Mod_LoadBrushes: funny lump size in %s\n", m_pLoadModel->name );

	inbrushsides = (void *)(mod_base + brushsides->fileofs );
	if( brushsides->filelen % sizeof( *inbrushsides ))
		Host_Error( "Mod_LoadBrushsides: funny lump size in %s\n", m_pLoadModel->name );

	in = (void *)(mod_base + fogs->fileofs);
	if( fogs->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadFogs: funny lump size in %s\n", m_pLoadModel->name );
	count = fogs->filelen / sizeof( *in );
	out = Mem_Alloc( m_pLoadModel->mod->mempool, count * sizeof( *out ));

	m_pLoadBmodel->fogs = out;
	m_pLoadBmodel->numFogs = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->shader = R_RegisterShader( in->shader );
		p = LittleLong( in->brushnum );
		if( p == -1 ) continue;

		brush = inbrushes + p;
		p = LittleLong( brush->firstside );
		if( p == -1 )
		{
			out->shader = NULL;
			continue;
		}

		brushside = inbrushsides + p;
		p = LittleLong( in->visibleSide );
		out->numplanes = LittleLong( brush->numsides );
		out->planes = Mem_Alloc( m_pLoadModel->mod->mempool, out->numplanes * sizeof( cplane_t ));

		if( p != -1 ) out->visible = m_pLoadBmodel->planes + LittleLong( brushside[p].planenum );
		for( j = 0; j < out->numplanes; j++ )
			out->planes[j] = *(m_pLoadBmodel->planes + LittleLong( brushside[j].planenum ));
	}
}

/*
=================
Mod_LoadSurfaces
=================
*/
static void Mod_LoadSurfaces( const lump_t *l )
{
	int		i, count;
	dsurface_t	*in;
	msurface_t	*out;

	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadFaces: funny lump size in %s\n", m_pLoadModel->name );
	count = l->filelen / sizeof( *in );
	out = Mem_Alloc( m_pLoadModel->mod->mempool, count * sizeof( *out ));

	m_pLoadBmodel->surfaces = out;
	m_pLoadBmodel->numsurfaces = count;

	for( i = 0; i < count; i++, in++, out++ )
		Mod_LoadSurfaceCommon( in, out );
}

/*
=================
R_LoadVisibility
=================
*/
static void Mod_LoadVisibility( const lump_t *l )
{
	size_t	vis_length;
	int	i;

	vis_length = ( m_pLoadBmodel->vis->numclusters + 63 ) & ~63;
	Mem_Set( r_fullvis, 0xFF, vis_length );	// never reach MAX_MAP_LEAFS/8

	if( !l->filelen ) return;
	m_pLoadBmodel->vis = Mem_Alloc( m_pLoadModel->mod->mempool, l->filelen );
	Mem_Copy( m_pLoadBmodel->vis, mod_base + l->fileofs, l->filelen );

	m_pLoadBmodel->vis->numclusters = LittleLong( m_pLoadBmodel->vis->numclusters );
	for( i = 0; i < m_pLoadBmodel->vis->numclusters; i++ )
	{
		m_pLoadBmodel->vis->bitofs[i][0] = LittleLong( m_pLoadBmodel->vis->bitofs[i][0] );
		m_pLoadBmodel->vis->bitofs[i][1] = LittleLong( m_pLoadBmodel->vis->bitofs[i][1] );
	}
}


/*
=================
Mod_SetupSubmodels
=================
*/
static void Mod_SetupSubmodels( void )
{
	int		i;

	// set up the submodels
	for( i = 0; i < m_pLoadBmodel->numSubmodels; i++ )
	{
		rmodel_t		*starmod;
		mbrushmodel_t	*bmodel;
		msubmodel_t	*bm;
	
		bm = &m_pLoadBmodel->submodels[i];
		starmod = &mod_inline[i];
		bmodel = (mbrushmodel_t *)starmod->extradata;

		Mem_Copy( starmod, m_pLoadModel->mod, sizeof( rmodel_t ));
		Mem_Copy( bmodel, m_pLoadModel->mod->extradata, sizeof( mbrushmodel_t ));

		bmodel->firstModelSurface = bmodel->surfaces + bm->firstFace;
		bmodel->numModelSurfaces = bm->numFaces;
		starmod->extradata = bmodel;
		starmod->type = mod_brush;

		VectorCopy( bm->maxs, starmod->maxs );
		VectorCopy( bm->mins, starmod->mins );
		starmod->radius = bm->radius;

		if( i == 0 ) *m_pLoadModel->mod = *starmod;
		else bmodel->numSubmodels = 0;
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
static void Mod_LoadSubmodels( const lump_t *l )
{
	int		i, j, count;
	dmodel_t		*in;
	msubmodel_t	*out;
	mbrushmodel_t	*bmodel;

	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadSubmodels: funny lump size in %s\n", m_pLoadModel->name );
	count = l->filelen / sizeof( *in );
	out = Mem_Alloc( m_pLoadModel->mod->mempool, count * sizeof( *out ));

	mod_inline = Mem_Alloc( m_pLoadModel->mod->mempool, count * (sizeof( *mod_inline ) + sizeof( *bmodel )));
	m_pLoadModel->mod->extradata = bmodel = (mbrushmodel_t *)( (byte *)mod_inline + count * sizeof( *mod_inline ));

	m_pLoadBmodel = bmodel;
	m_pLoadBmodel->submodels = out;
	m_pLoadBmodel->numSubmodels = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		mod_inline[i].extradata = bmodel + i;

		for( j = 0; j < 3; j++ )
		{
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat( in->mins[j] ) - 1;
			out->maxs[j] = LittleFloat( in->maxs[j] ) + 1;
		}

		out->radius = RadiusFromBounds( out->mins, out->maxs );
		out->firstFace = LittleLong( in->firstface );
		out->numFaces = LittleLong( in->numfaces );
	}
}

/*
=================
Mod_SetParent

chain decendants
=================
*/
static void Mod_SetParent( mnode_t *node, mnode_t *parent )
{
	node->parent = parent;
	if( !node->plane ) return;

	// go both sides on node
	Mod_SetParent( node->children[0], node );
	Mod_SetParent( node->children[1], node );
}

/*
=================
Mod_ApplySuperStylesToFace
=================
*/
static void Mod_ApplySuperStylesToFace( const dsurface_t *in, msurface_t *out )
{
	int	j, k;
	float	*lmArray;
	rb_mesh_t	*mesh;
	lmrect_t	*lmRects[LM_STYLES];
	int	lightmaps[LM_STYLES];
	byte	lStyles[LM_STYLES];
	byte	vStyles[LM_STYLES];

	for( j = 0; j < LM_STYLES; j++ )
	{
		lightmaps[j] = LittleLong( in->lmapNum[j] );

		if( lightmaps[j] < 0 || out->faceType == MST_FLARE || lightmaps[j] >= m_pLoadModel->numLightmaps )
		{
			lmRects[j] = NULL;
			lightmaps[j] = -1;
			lStyles[j] = 255;
		}
		else
		{
			lmRects[j] = &m_pLoadModel->lmRects[lightmaps[j]];
			lightmaps[j] = lmRects[j]->texNum;

			if( mapConfig.lightmapsPacking )
			{                       
				// scale/shift lightmap coords
				mesh = out->mesh;
				lmArray = mesh->lm[j][0];
				for( k = 0; k < mesh->numVerts; k++, lmArray += 2 )
				{
					lmArray[0] = (double)( lmArray[0] ) * lmRects[j]->texMatrix[0][0] + lmRects[j]->texMatrix[0][1];
					lmArray[1] = (double)( lmArray[1] ) * lmRects[j]->texMatrix[1][0] + lmRects[j]->texMatrix[1][1];
				}
			}
			lStyles[j] = in->lStyles[j];
		}
		vStyles[j] = in->vStyles[j];
	}
	out->superLightStyle = R_AddSuperLightStyle( lightmaps, lStyles, vStyles, lmRects );
}

/*
=================
Mod_Finish
=================
*/
static void Mod_Finish( const lump_t *faces, vec3_t gridSize, vec3_t ambient )
{
	int		i, j;
	dsurface_t	*in;
	msurface_t	*surf;
	mfog_t		*testFog;
	bool		globalFog;

	// set up lightgrid
	if( gridSize[0] < 1 || gridSize[1] < 1 || gridSize[2] < 1 )
		VectorSet( m_pLoadBmodel->gridSize, 64.0f, 64.0f, 128.0f );
	else VectorCopy( gridSize, m_pLoadBmodel->gridSize );

	for( j = 0; j < 3; j++ )
	{
		vec3_t maxs;

		m_pLoadBmodel->gridMins[j] = m_pLoadBmodel->gridSize[j] * ceil( ( m_pLoadBmodel->submodels[0].mins[j] + 1 ) / m_pLoadBmodel->gridSize[j] );
		maxs[j] = m_pLoadBmodel->gridSize[j] *floor(( m_pLoadBmodel->submodels[0].maxs[j] - 1 ) / m_pLoadBmodel->gridSize[j] );
		m_pLoadBmodel->gridBounds[j] = ( maxs[j] - m_pLoadBmodel->gridMins[j] ) / m_pLoadBmodel->gridSize[j] + 1;
	}
	m_pLoadBmodel->gridBounds[3] = m_pLoadBmodel->gridBounds[1] * m_pLoadBmodel->gridBounds[0];

	// ambient lighting
	for( i = 0; i < 3; i++ )
		mapConfig.ambient[i] = bound( 0, ambient[i] * ((float)(1<<mapConfig.pow2MapOvrbr) / 255.0f ), 1.0f );
	R_SortSuperLightStyles();

	for( i = 0, testFog = m_pLoadBmodel->fogs; i < m_pLoadBmodel->numFogs; testFog++, i++ )
	{
		if( !testFog->shader ) continue;
		if( testFog->visible ) continue;

		testFog->visible = Mem_Alloc( m_pLoadModel->mod->mempool, sizeof( cplane_t ));
		VectorSet( testFog->visible->normal, 0, 0, 1 );
		testFog->visible->type = PLANE_Z;
		testFog->visible->dist = m_pLoadBmodel->submodels[0].maxs[0] + 1;
	}

	// make sure that the only fog in the map has valid shader
	globalFog = ( m_pLoadBmodel->numFogs == 1 );
	if( globalFog )
	{
		testFog = &m_pLoadBmodel->fogs[0];
		if( !testFog->shader ) globalFog = false;
	}

	// apply super-lightstyles to map surfaces
	in = (void *)(mod_base + faces->fileofs);

	for( i = 0, surf = m_pLoadBmodel->surfaces; i < m_pLoadBmodel->numsurfaces; i++, in++, surf++ )
	{
		if( globalFog && surf->mesh && surf->fog != testFog )
		{
			if(!( surf->shader->flags & SHADER_SKY ) && !surf->shader->fog_dist )
				globalFog = false;
		}

		if( !R_SurfPotentiallyVisible( surf ))
			continue;
		Mod_ApplySuperStylesToFace( in, surf );
	}

	if( globalFog )
	{
		m_pLoadBmodel->globalfog = testFog;
		MsgDev( D_INFO, "Global fog detected: %s\n", testFog->shader->name );
	}

	if( m_pLoadModel->points ) Mem_Free( m_pLoadModel->points );
	if( m_pLoadModel->indices ) Mem_Free( m_pLoadModel->indices );
	if( m_pLoadModel->lmRects ) Mem_Free( m_pLoadModel->lmRects );

	// g-cont. we need to loading shaders after mod_finish, so leave in memory
	// comment this
	if( m_pLoadModel->miptex ) Mem_Free( m_pLoadModel->miptex );

	Mod_SetParent( m_pLoadBmodel->nodes, NULL );
	Mod_SetupSubmodels();
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel( rmodel_t *mod, const void *buffer )
{
	dheader_t		*header;
	vec3_t		gridSize, ambient;
	
	if( m_pLoadModel == NULL ) Host_Error("m_pLoadModel == NULL\n" );
	if( m_pLoadModel->mod != r_models )
	{
		MsgDev( D_ERROR, "loaded a brush model after the world\n");
		return;
	}

	header = (dheader_t *)buffer;

	// byte swap the header fields and sanity check
	SwapBlock( (int *)header, sizeof(dheader_t));

	if( header->version != BSPMOD_VERSION )
		Host_Error( "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, header->version, BSPMOD_VERSION );
	mod_base = (byte *)header;

	// load into heap
	Mod_LoadSubmodels( &header->lumps[LUMP_MODELS] );
	Mod_LoadEntities( &header->lumps[LUMP_ENTITIES], gridSize, ambient );
	Mod_LoadVertexes( &header->lumps[LUMP_VERTICES] );
	Mod_LoadIndices( &header->lumps[LUMP_INDICES] );
	Mod_LoadLighting( &header->lumps[LUMP_SURFACES] );
	Mod_LoadLightgrid( &header->lumps[LUMP_LIGHTGRID] );
	Mod_LoadShaders( &header->lumps[LUMP_SHADERS] );
	Mod_LoadPlanes( &header->lumps[LUMP_PLANES] );
	Mod_LoadFogs( &header->lumps[LUMP_FOGS], &header->lumps[LUMP_BRUSHES], &header->lumps[LUMP_BRUSHSIDES] );
	Mod_LoadSurfaces( &header->lumps[LUMP_SURFACES] );
	Mod_LoadLeafs( &header->lumps[LUMP_LEAFS], &header->lumps[LUMP_LEAFSURFACES] );
	Mod_LoadNodes( &header->lumps[LUMP_NODES] );
	Mod_LoadLightArray( &header->lumps[LUMP_LIGHTARRAY] );
	Mod_LoadVisibility( &header->lumps[LUMP_VISIBILITY] );

	Mod_Finish( &header->lumps[LUMP_SURFACES], gridSize, ambient );
	m_pLoadModel->mod->sequence = registration_sequence;	// register model
	m_pLoadModel->mod->type = mod_world;
}

/*
=================
Mod_RegisterShader

needs only for more smooth moving of "loading" progress bar
=================
*/
bool Mod_RegisterShader( const char *unused, int index )
{
	mipRef_t	*in;
	shader_t	*out;
	
	// nothing to load
	//if( !r_worldBrushModel ) 
	// FIXME: get to work
	return false;

	m_pLoadModel->mod = r_worldModel;
	in = m_pLoadModel->miptex + index;
	out = r_worldModel->shaders[index];

	if( !in->name[0] ) Sys_Break( "Mod_RegisterShader: null name!\n" );

	// now all pointers are valid
	out = R_FindShader( in->name, in->shaderType, in->flags );
	return true;
}

/*
=================
Mod_LoadStudioModel
=================
*/
void Mod_LoadStudioModel( rmodel_t *mod, const void *buffer )
{
	R_StudioLoadModel( mod, buffer );
	m_pLoadModel->mod->type = mod_studio;
}

/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel( rmodel_t *mod, const void *buffer )
{
	R_SpriteLoadModel( mod, buffer );
	m_pLoadModel->mod->type = mod_sprite;
}
/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
rmodel_t *Mod_ForName( const char *name, bool crash )
{
	rmodel_t	*mod;
	uint	i, *buf;
	
	if( !name[0] ) return NULL;

	// inline models are grabbed only from worldmodel
	if( name[0] == '*' )
	{
		i = com.atoi( name + 1 );
		if( i < 1 || !r_worldBrushModel || i >= r_worldBrushModel->numSubmodels )
		{
			MsgDev( D_WARN, "Warning: bad inline model number %i\n", i );
			return NULL;
		}
		// prolonge registration
		mod_inline[i].sequence = registration_sequence;
		return &mod_inline[i];
	}

	// search the currently loaded models
	for( i = 0, mod = r_models; i < r_nummodels; i++, mod++ )
	{
		if( !mod->name[0] ) continue;
		if( !com.strcmp( mod->name, name ))
		{
			// prolonge registration
			mod->sequence = registration_sequence;
			return mod;
		}
	}
	
	// find a free model slot spot
	for( i = 0, mod = r_models; i < r_nummodels; i++, mod++ )
		if( !mod->name[0] ) break; // free spot

	if( i == r_nummodels )
	{
		if( r_nummodels == MAX_MODELS )
		{
			MsgDev( D_ERROR, "Mod_ForName: MAX_MODELS limit exceeded\n" );
			return NULL;
		}
		r_nummodels++;
	}

	mod->type = mod_bad;
	com.strncpy( mod->name, name, MAX_STRING );

	// load the file
	buf = (uint *)FS_LoadFile( mod->name, NULL );
	if( !buf )
	{
		if( crash ) Host_Error( "Mod_ForName: %s not found\n", mod->name );
		Mem_Set( mod->name, 0, sizeof( mod->name ));
		return NULL;
	}

	mod->mempool = Mem_AllocPool(va("^1%s^7", mod->name ));
	m_pLoadModel->mod = mod;
	m_pLoadModel->points = NULL;
	m_pLoadModel->indices = NULL;
	m_pLoadModel->lmRects = NULL;
	m_pLoadModel->miptex = NULL;
	
	//
	// fill it in
	//

	// call the apropriate loader
	switch (LittleLong(*(uint *)buf))
	{
	case IDBSPMODHEADER:
		Mod_LoadBrushModel( mod, buf );
		break;
	case IDSTUDIOHEADER:
		Mod_LoadStudioModel( mod, buf );
		break;
	case IDSPRITEHEADER:
		Mod_LoadSpriteModel( mod, buf );
		break;
	default:
		// will be freed at end of registration
		MsgDev( D_ERROR, "Mod_NumForName: unknown file id for %s, unloaded\n", mod->name );
		break;
	}
	Mem_Free( buf );	// free file buffer

	return mod;
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_BeginRegistration

Specifies the model that will be used as the world
@@@@@@@@@@@@@@@@@@@@@
*/
void R_BeginRegistration( const char *mapname )
{
	string	fullname;
	int	i;

	registration_sequence++;
	com.sprintf( fullname, "maps/%s.bsp", mapname );

	// explicitly free the old map if different
	if( com.strcmp( r_models[0].name, fullname ))
		Mod_Free( &r_models[0] );

	mapConfig.pow2MapOvrbr = 0;
	mapConfig.lightmapsPacking = false;
	mapConfig.deluxeMaps = false;
	mapConfig.deluxeMappingEnabled = false;
	VectorClear( mapConfig.ambient );

	r_farclip_min = Z_NEAR; // sky shaders will most likely modify this value
	r_worldModel = Mod_ForName( fullname, true );
	r_worldBrushModel = (mbrushmodel_t *)r_worldModel->extradata;

	r_worldEntity->scale = 1.0f;
	r_worldEntity->model = r_worldModel;
	r_worldEntity->ent_type = ED_BSPBRUSH;
	Matrix3x3_LoadIdentity( r_worldEntity->matrix );

	r_frameCount = 1;
	r_oldViewCluster = r_viewCluster = -1;  // force markleafs

	for( i = 0; i < r_worldModel->numShaders; i++ )
		R_ShaderRegisterImages( r_worldModel->shaders[i] ); // update world shaders
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_RegisterModel

@@@@@@@@@@@@@@@@@@@@@
*/
rmodel_t *R_RegisterModel( const char *name )
{
	rmodel_t	*mod;
	int	i;
	
	mod = Mod_ForName( name, false );
	if( mod ) Msg("R_RegisterModel: %s\n", mod->name );	

	for( i = 0; i < mod->numShaders; i++ )
		R_ShaderRegisterImages( mod->shaders[i] );
	return mod;
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_EndRegistration

@@@@@@@@@@@@@@@@@@@@@
*/
void R_EndRegistration( void )
{
	int	i;
	rmodel_t	*mod;

	for( i = 0, mod = r_models; i < r_nummodels; i++, mod++ )
	{
		if( !mod->name[0] ) continue;
		if( mod->sequence != registration_sequence )
			Mod_Free( mod );
	}
	R_ImageFreeUnused();
}

/*
=================
R_ModelList_f
=================
*/
void R_ModelList_f( void )
{
	rmodel_t	*mod;
	int	i;

	Msg( "\n" );
	Msg( "-----------------------------------\n" );

	for( i = 0; i < r_nummodels, mod = r_models; i++, mod++ )
	{
		if( !mod->name[0] ) continue; // free slot
		Msg( "%s%s\n", mod->name, (mod->type == mod_bad) ? " (DEFAULTED)" : "" );
	}

	Msg( "-----------------------------------\n" );
	Msg( "%i total models\n", r_nummodels );
	Msg( "\n");
}

/*
================
Mod_Free
================
*/
void Mod_Free( rmodel_t *mod )
{
	Mem_FreePool( &mod->mempool );
	memset( mod, 0, sizeof( *mod ));
	mod = NULL;
}

/*
================
Mod_FreeAll
================
*/
void Mod_FreeAll( void )
{
	int	i;

	for( i = 0; i < r_nummodels; i++ )
	{
		if( r_models[i].mempool )
			Mod_Free( &r_models[i] );
	}
}

/*
=================
R_InitModels
=================
*/
void R_InitModels( void )
{
	memset( r_fullvis, 255, sizeof( r_fullvis ));

	m_pLoadModel = Mem_Alloc( r_temppool, sizeof( loadmodel_t ));
	r_worldBrushModel = NULL;
	r_worldModel = NULL;
	r_frameCount = 1;				// no dlight cache
	r_nummodels = 0;
	r_viewCluster = -1;				// force markleafs

	r_worldEntity = &r_entities[0];		// First entity is the world
	memset( r_worldEntity, 0, sizeof( ref_entity_t ));
	r_worldEntity->ent_type = ED_NORMAL;
	r_worldEntity->model = r_worldModel;
	Matrix3x3_LoadIdentity( r_worldEntity->matrix );
	VectorSet( r_worldEntity->rendercolor, 1.0f, 1.0f, 1.0f );
	r_worldEntity->renderamt = 1.0f;		// i'm hope we don't want to see semisolid world :) 
	
	R_StudioInit();
}

/*
=================
R_ShutdownModels
=================
*/
void R_ShutdownModels( void )
{
	R_StudioShutdown();

	if( m_pLoadModel )
		Mem_Free( m_pLoadModel );
	r_worldModel = NULL;
	r_worldEntity = NULL;

	Mod_FreeAll();
	memset( r_models, 0, sizeof( r_models ));
	r_nummodels = 0;
}
