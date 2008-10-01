//=======================================================================
//			Copyright XashXT Group 2007 ©
//		      r_model.c - model loading and caching
//=======================================================================

#include "r_local.h"
#include "byteorder.h"
#include "mathlib.h"
#include "matrixlib.h"
#include "const.h"

// the inline models from the current map are kept separate
static rmodel_t	r_inlinemodels[MAX_MODELS];
static byte	r_fullvis[MAX_MAP_LEAFS/8];
static rmodel_t	r_models[MAX_MODELS];
int		registration_sequence;
rmodel_t		*m_pLoadModel;
static int	r_nummodels;

/*
===============
R_PointInLeaf
===============
*/
node_t *R_PointInLeaf( const vec3_t p )
{
	node_t		*node;
	cplane_t		*plane;
	float		d;	

	if( !r_worldModel || !r_worldModel->nodes )
		Host_Error( "R_PointInLeaf: bad model\n" );

	node = r_worldModel->nodes;
	while( 1 )
	{
		if( node->contents != CONTENTS_NODE )
			break;
		plane = node->plane;
		d = DotProduct( p, plane->normal ) - plane->dist;
		if( d > 0 ) node = node->children[0];
		else node = node->children[1];
	}
	return node;
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

	row = (r_worldModel->vis->numClusters+7)>>3;	
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
	if (!r_worldModel || !r_worldModel->vis || cluster < 0 || cluster >= r_worldModel->numClusters )
		return r_worldModel->novis;
	return R_DecompressVis((byte *)r_worldModel->vis + r_worldModel->vis->bitOfs[cluster][DVIS_PVS]);
}

static void HSVtoRGB( float h, float s, float v, float rgb[3] )
{
	int	i;
	float	f, p, q, t;

	h *= 5;

	i = floor( h );
	f = h - i;

	p = v * ( 1 - s );
	q = v * ( 1 - s * f );
	t = v * ( 1 - s * ( 1 - f ) );

	switch( i )
	{
	case 0:
		rgb[0] = v;
		rgb[1] = t;
		rgb[2] = p;
		break;
	case 1:
		rgb[0] = q;
		rgb[1] = v;
		rgb[2] = p;
		break;
	case 2:
		rgb[0] = p;
		rgb[1] = v;
		rgb[2] = t;
		break;
	case 3:
		rgb[0] = p;
		rgb[1] = q;
		rgb[2] = v;
		break;
	case 4:
		rgb[0] = t;
		rgb[1] = p;
		rgb[2] = v;
		break;
	case 5:
		rgb[0] = v;
		rgb[1] = p;
		rgb[2] = q;
		break;
	}
}

/*
===============
R_ColorShiftLightingBytes

===============
*/
static void R_ColorShiftLightingBytes( byte in[4], byte out[4] )
{
	int	shift, r, g, b;

	// shift the color data based on overbright range
	shift = r_overbrightbits->integer;

	// shift the data based on overbright range
	r = in[0]<<shift;
	g = in[1]<<shift;
	b = in[2]<<shift;
	
	// normalize by color instead of saturating to white
	if(( r|g|b ) > 255 )
	{
		int	max;

		max = r > g ? r : g;
		max = max > b ? max : b;
		r = r * 255 / max;
		g = g * 255 / max;
		b = b * 255 / max;
	}

	out[0] = r;
	out[1] = g;
	out[2] = b;
	out[3] = in[3];
}

/*

/*
=======================================================================

BRUSH MODELS

=======================================================================
*/
/*
=================
R_LoadVertexes
=================
*/
static void R_LoadVertexes( const byte *base, const lump_t *l )
{
	dvertex_t		*in;
	vertex_t		*out;
	const float	*rgba;
	int		i;

	in = (dvertex_t *)(base + l->fileofs);
	if( l->filelen % sizeof(dvertex_t))
		Host_Error( "R_LoadVertexes: funny lump size in '%s'\n", m_pLoadModel->name );

	m_pLoadModel->numVertexes = l->filelen / sizeof(dvertex_t);
	m_pLoadModel->vertexes = out = Mem_Alloc( m_pLoadModel->mempool, m_pLoadModel->numVertexes * sizeof(vertex_t));

	for( i = 0; i < m_pLoadModel->numVertexes; i++, in++, out++ )
	{
		out->point[0] = LittleFloat(in->point[0]);
		out->point[1] = LittleFloat(in->point[1]);
		out->point[2] = LittleFloat(in->point[2]);

		out->normal[0] = LittleFloat(in->normal[0]);
		out->normal[1] = LittleFloat(in->normal[1]);
		out->normal[2] = LittleFloat(in->normal[2]);

		out->st[0] = LittleFloat(in->st[0]);
		out->st[1] = LittleFloat(in->st[1]);

		// FIXME: implement LIGHTSTYLES
		out->lm[0] = LittleFloat(in->lm[0][0]);
		out->lm[1] = LittleFloat(in->lm[1][0]);

		rgba = UnpackRGBA( in->rgba[0] );
		Vector4Set( out->color, rgba[0], rgba[1], rgba[2], rgba[3] );
		
	}
}

/*
=================
R_LoadIndexes
=================
*/
static void R_LoadIndexes( const byte *base, const lump_t *l )
{
	int	*in;

	in = (int *)(base + l->fileofs);
	if( l->filelen % sizeof( *in ))
		Host_Error( "R_LoadIndices: funny lump size in '%s'\n", m_pLoadModel->name);

	m_pLoadModel->numIndexes = l->filelen / sizeof( *in );
	m_pLoadModel->indexes = Mem_Alloc( m_pLoadModel->mempool, l->filelen );
	Mem_Copy( m_pLoadModel->indexes, in, l->filelen );
	SwapBlock( (int *)m_pLoadModel->indexes, m_pLoadModel->numIndexes * sizeof( *in ));
}

/*
=================
R_LoadLightmaps
=================
*/
static void R_LoadLightmaps( void )
{
	float	maxIntensity = 0;
	double	sumIntensity = 0;
	string	lmap_name;
	byte	*buf_p, *pixels;
	rgbdata_t	*lmap;
	int	i, j;	

	if( r_fullbright->integer || m_pLoadModel->numLightmaps <= 0 )
		return;

	Msg("m_pLoadModel->numLightmaps %d\n", m_pLoadModel->numLightmaps );

	for( i = 0; i < m_pLoadModel->numLightmaps; i++ )
	{
		com.sprintf( lmap_name, "gfx/lightmaps/%s_%04d.tga", m_pLoadModel->name, i );
		lmap = FS_LoadImage( lmap_name, NULL, 0 );
		if( !lmap ) continue;

		Image_ExpandRGBA( lmap );
		pixels = Mem_Realloc( r_temppool, pixels, lmap->width * lmap->height * 4 );
		buf_p = lmap->buffer;

		if ( r_showlightmaps->integer == 2 )
		{	
			// color code by intensity as development tool	(FIXME: check range)
			for( j = 0; j < lmap->width * lmap->height; j++ )
			{
				float r = buf_p[j*4+0];
				float g = buf_p[j*4+1];
				float b = buf_p[j*4+2];
				float intensity;
				float out[3];

				intensity = 0.33f * r + 0.685f * g + 0.063f * b;

				if( intensity > 255 ) intensity = 1.0f;
				else intensity /= 255.0f;

				if( intensity > maxIntensity )
					maxIntensity = intensity;

				HSVtoRGB( intensity, 1.00, 0.50, out );

				pixels[j*4+0] = out[0] * 255;
				pixels[j*4+1] = out[1] * 255;
				pixels[j*4+2] = out[2] * 255;
				pixels[j*4+3] = 255;
				sumIntensity += intensity;
			}
		}
		else
		{
			for( j = 0; j < lmap->width * lmap->height; j++ )
			{
				R_ColorShiftLightingBytes( &buf_p[j*4], &pixels[j*4] );
				pixels[j*4+3] = 255;
			}
		}
		m_pLoadModel->lightMaps[i] = R_CreateTexture( va("*lightmap%d", i ), pixels, lmap->width, lmap->height, 0, TF_CLAMP );
		FS_FreeImage( lmap ); // no reason to keep image
	}

	Mem_Free( pixels );

	if( r_showlightmaps->integer == 2 )
	{
		MsgDev( D_INFO, "Brightest lightmap value: %d\n", ( int )( maxIntensity * 255 ));
	}
}

/*
=================
R_LoadPlanes
=================
*/
static void R_LoadPlanes( const byte *base, const lump_t *l )
{
	dplane_t		*in;
	cplane_t		*out;
	int		i;
	
	in = (dplane_t *)(base + l->fileofs);
	if (l->filelen % sizeof(dplane_t))
		Host_Error( "R_LoadPlanes: funny lump size in '%s'\n", m_pLoadModel->name );

	m_pLoadModel->numPlanes = l->filelen / sizeof(dplane_t);
	m_pLoadModel->planes = out = Mem_Alloc( m_pLoadModel->mempool, m_pLoadModel->numPlanes * sizeof(cplane_t));

	for( i = 0; i < m_pLoadModel->numPlanes; i++, in++, out++ )
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
R_LoadShaders
=================
*/
void R_LoadShaders( const byte *base, const lump_t *l )
{
	dshader_t		*in;
	mipTex_t		*out;
	int 		i, count;

	in = (void *)(base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("R_LoadShaders: funny lump size in '%s'\n", m_pLoadModel->name );
	count = l->filelen / sizeof(*in);

	out = (mipTex_t *)Mem_Alloc( m_pLoadModel->mempool, count * sizeof(*out));
 
	m_pLoadModel->shaders = out;
	m_pLoadModel->numShaders = count;

	for ( i = 0; i < count; i++, in++, out++ )
	{
		com.strncpy( out->name, in->name, MAX_QPATH );
		out->shader = r_defaultShader; // real shaders will load later
		out->contents = LittleLong( in->contents );
		out->flags = LittleLong( in->surfaceFlags );
	}
}

/*
=================
R_CalcSurfaceBounds

Fills in surf->mins and surf->maxs
=================
*/
static void R_CalcSurfaceBounds( surface_t *surf )
{
	int		i;
	vertex_t		*v;

	ClearBounds( surf->mins, surf->maxs );

	for( i = 0; i < surf->numVertexes; i++ )
	{
		v = &m_pLoadModel->vertexes[surf->firstVertex + i];
		AddPointToBounds( v->point, surf->mins, surf->maxs );
	}
}

/*
=================
R_BuildPolygon
=================
*/
static void R_BuildSurfacePolygon( surface_t *surf )
{
	const vertex_t	*verts;
	surfPoly_t	*p;
	int		i;

	p = Mem_Alloc( m_pLoadModel->mempool, sizeof(surfPoly_t));
	p->next = surf->poly;
	surf->poly = p;

	// create indices
	p->numIndices = surf->numIndexes;
	p->indices = Mem_Alloc( m_pLoadModel->mempool, p->numIndices * sizeof(uint));
	Mem_Copy( p->indices, &m_pLoadModel->indexes[surf->firstIndex], p->numIndices * sizeof(uint));

	// create vertices
	p->numVertices = surf->numVertexes;
	p->vertices = Mem_Alloc( m_pLoadModel->mempool, p->numVertices * sizeof(surfPolyVert_t));
	verts = &m_pLoadModel->vertexes[surf->firstVertex];
	
	for( i = 0; i < surf->numVertexes; i++, verts++ )
	{
		// vertex
		VectorCopy( verts->point, p->vertices[i].xyz );
		p->vertices[i].st[0] = verts->st[0];
		p->vertices[i].st[1] = verts->st[1];
		p->vertices[i].lightmap[0] = verts->lm[0];
		p->vertices[i].lightmap[1] = verts->lm[1];

		// vertex color
		Vector4Copy( verts->color, p->vertices[i].color );
	}
}

/*
=================
R_LoadSurfaces
=================
*/
static void R_LoadSurfaces( const byte *base, const lump_t *l )
{
	dsurface_t	*in;
	surface_t 	*out;
	int		i;

	in = (dsurface_t *)(base + l->fileofs);
	if (l->filelen % sizeof(dsurface_t))
		Host_Error( "R_LoadSurfaces: funny lump size in '%s'\n", m_pLoadModel->name );

	m_pLoadModel->numSurfaces = l->filelen / sizeof(dsurface_t);
	m_pLoadModel->surfaces = out = Mem_Alloc( m_pLoadModel->mempool, m_pLoadModel->numSurfaces * sizeof(surface_t));
	m_pLoadModel->numLightmaps = 0;

	R_BeginBuildingLightmaps();

	for( i = 0; i < m_pLoadModel->numSurfaces; i++, in++, out++ )
	{
		if( LittleLong( in->flat.planeside )) out->flags |= SURF_PLANEBACK;
		out->firstIndex = LittleLong( in->firstindex );
		out->numIndexes = LittleLong( in->numindices );
		out->firstVertex = LittleLong( in->firstvertex );
		out->numVertexes = LittleLong( in->numvertices );
		out->plane = m_pLoadModel->planes + LittleLong( in->flat.planenum );
		out->texInfo = m_pLoadModel->shaders + LittleLong( in->shadernum );

		R_CalcSurfaceBounds( out );

		// FIXME: tangent vectors
		// VectorCopy( out->texInfo->vecs[0], out->tangent );
		// VectorNegate( out->texInfo->vecs[1], out->binormal );
		if(!(out->flags & SURF_PLANEBACK))
			VectorCopy( out->plane->normal, out->normal );
		else VectorNegate( out->plane->normal, out->normal );

		// FIXME: tangent vectors
		// VectorNormalize( out->tangent );
		// VectorNormalize( out->binormal );
		VectorNormalize( out->normal );

		// lighting info
		out->lmS = LittleLong( in->lmapX[0] );
		out->lmT = LittleLong( in->lmapY[0] );
		out->lmWidth = LittleLong( in->lmapWidth );
		out->lmHeight = LittleLong( in->lmapHeight );

		out->lmNum = LittleLong( in->lmapNum[0] );
		if( out->lmNum >= m_pLoadModel->numLightmaps )
			m_pLoadModel->numLightmaps = out->lmNum + 1;
		if( out->lmNum == -1 ) out->lmNum = 255; // turn up fullbright

		while( out->numStyles < MAX_LIGHTSTYLES && in->lStyles[out->numStyles] != 255 )
		{
			out->styles[out->numStyles] = in->lStyles[out->numStyles];
			out->numStyles++;
		}

		// create lightmap
		R_BuildSurfaceLightmap( out );

		// create polygons
		R_BuildSurfacePolygon( out );
	}
	R_EndBuildingLightmaps();
}

/*
=================
R_LoadMarkSurfaces
=================
*/
static void R_LoadMarkSurfaces( const byte *base, const lump_t *l )
{
	dword		*in;
	surface_t		**out;
	int		i, j;
	
	in = (dword *)(base + l->fileofs);
	if (l->filelen % sizeof(dword))
		Host_Error( "R_LoadMarkSurfaces: funny lump size in '%s'\n", m_pLoadModel->name );

	m_pLoadModel->numMarkSurfaces = l->filelen / sizeof(dword);
	m_pLoadModel->markSurfaces = out = Mem_Alloc( m_pLoadModel->mempool, m_pLoadModel->numMarkSurfaces * sizeof(surface_t *));

	for (i = 0; i < m_pLoadModel->numMarkSurfaces; i++ )
	{
		j = LittleLong( in[i] );
		if (j < 0 ||  j >= m_pLoadModel->numMarkSurfaces)
			Host_Error( "R_LoadMarkSurfaces: bad surface number in '%s'\n", m_pLoadModel->name );
		out[i] = m_pLoadModel->surfaces + j;
	}
}

/*
=================
R_LoadVisibility
=================
*/
static void R_LoadVisibility( const byte *base, const lump_t *l )
{
	size_t	vis_length;
	int	i;

	vis_length = ( m_pLoadModel->numClusters + 63 ) & ~63;
	m_pLoadModel->novis = Mem_Alloc( m_pLoadModel->mempool, vis_length );
	memset( m_pLoadModel->novis, 0xFF, vis_length );

	if( !l->filelen ) return;
	m_pLoadModel->vis = Mem_Alloc( m_pLoadModel->mempool, l->filelen );
	Mem_Copy( m_pLoadModel->vis, base + l->fileofs, l->filelen );

	m_pLoadModel->vis->numClusters = LittleLong( m_pLoadModel->vis->numClusters );
	for( i = 0; i < m_pLoadModel->vis->numClusters; i++ )
	{
		m_pLoadModel->vis->bitOfs[i][0] = LittleLong(m_pLoadModel->vis->bitOfs[i][0]);
		m_pLoadModel->vis->bitOfs[i][1] = LittleLong(m_pLoadModel->vis->bitOfs[i][1]);
	}
}

/*
=================
R_SetParent
=================
*/
static void R_NodeSetParent( node_t *node, node_t *parent )
{
	node->parent = parent;
	if( node->contents != CONTENTS_NODE ) return;

	R_NodeSetParent( node->children[0], node );
	R_NodeSetParent( node->children[1], node );
}

/*
=================
R_LoadLeafNodes
=================
*/
static void R_LoadLeafNodes( const byte *base, const lump_t *nodes, const lump_t *leafs )
{
	int		i, j, p;
	node_t 		*out;
	dnode_t		*inNode;
	dleaf_t		*inLeaf;
	int		numNodes, numLeafs;

	inNode = (void *)(base + nodes->fileofs);
	if( nodes->filelen % sizeof(dnode_t) || leafs->filelen % sizeof(dleaf_t))
		Host_Error( "R_LoadLeafNodes: funny lump size in '%s'\n", m_pLoadModel->name );
	numNodes = nodes->filelen / sizeof(dnode_t);
	numLeafs = leafs->filelen / sizeof(dleaf_t);

	out = Mem_Alloc( m_pLoadModel->mempool, (numNodes + numLeafs) * sizeof( *out ));	

	m_pLoadModel->nodes = out;
	m_pLoadModel->numNodes = numNodes + numLeafs;

	// load nodes
	for( i = 0; i < numNodes; i++, inNode++, out++ )
	{
		for( j = 0; j < 3; j++ )
		{
			out->mins[j] = LittleLong( inNode->mins[j] );
			out->maxs[j] = LittleLong( inNode->maxs[j] );
		}
	
		p = LittleLong( inNode->planenum );
		if( p < 0 || p >= m_pLoadModel->numPlanes )
			Host_Error( "R_LoadLeafNodes: bad planenum %i\n", p );
		out->plane = m_pLoadModel->planes + p;
		out->contents = CONTENTS_NODE;	// differentiate from leafs

		for( j = 0; j < 2; j++ )
		{
			p = LittleLong( inNode->children[j] );
			if( p >= 0 ) out->children[j] = m_pLoadModel->nodes + p;
			else out->children[j] = m_pLoadModel->nodes + numNodes + (-1 - p);
		}
	}
	
	// load leafs
	inLeaf = (void *)(base + leafs->fileofs);
	for( i = 0; i < numLeafs; i++, inLeaf++, out++ )
	{
		for( j = 0; j < 3; j++ )
		{
			out->mins[j] = LittleLong( inLeaf->mins[j] );
			out->maxs[j] = LittleLong( inLeaf->maxs[j] );
		}

		out->cluster = LittleLong( inLeaf->cluster );
		out->area = LittleLong( inLeaf->area );

		if( out->cluster >= m_pLoadModel->numClusters )
			m_pLoadModel->numClusters = out->cluster + 1;

		out->firstMarkSurface = m_pLoadModel->markSurfaces + LittleLong( inLeaf->firstleafsurface );
		out->numMarkSurfaces = LittleLong( inLeaf->numleafsurfaces );
	}	

	// chain decendants
	R_NodeSetParent( m_pLoadModel->nodes, NULL );
}

/*
=================
R_SetupSubmodels
=================
*/
static void R_SetupSubmodels( void )
{
	int		i;
	submodel_t	*bm;
	rmodel_t		*model;

	for( i = 0; i < m_pLoadModel->numSubmodels; i++ )
	{
		bm = &m_pLoadModel->submodels[i];
		model = &r_inlinemodels[i];

		*model = *m_pLoadModel;
		model->numModelSurfaces = bm->numFaces;
		model->firstModelSurface = bm->firstFace;
		model->type = mod_brush;
		VectorCopy( bm->maxs, model->maxs );
		VectorCopy( bm->mins, model->mins );
		model->radius = bm->radius;

		if( i == 0 ) *m_pLoadModel = *model;
		else com.snprintf( model->name, sizeof(model->name), "*%i", i );
	}
}

/*
=================
R_LoadSubmodels
=================
*/
static void R_LoadSubmodels( const byte *base, const lump_t *l )
{
	dmodel_t		*in;
	submodel_t	*out;
	int		i, j;

	in = (dmodel_t *)(base + l->fileofs);
	if( l->filelen % sizeof(dmodel_t))
		Host_Error( "R_LoadSubmodels: funny lump size in '%s'\n", m_pLoadModel->name );

	m_pLoadModel->numSubmodels = l->filelen / sizeof(dmodel_t);
	m_pLoadModel->submodels = out = Mem_Alloc( m_pLoadModel->mempool, m_pLoadModel->numSubmodels * sizeof(submodel_t));

	for( i = 0; i < m_pLoadModel->numSubmodels; i++, in++, out++ )
	{
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

	R_SetupSubmodels();	// set up the submodels
}

/*
=================
R_LoadLightgrid
=================
*/

static void R_LoadLightgrid( const byte *base, const lump_t *l )
{
	if( !l->filelen ) return;
	// FIXME: implement
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel( rmodel_t *mod, const void *buffer )
{
	dheader_t		*header;
	byte		*mod_base;

	m_pLoadModel->type = mod_world;
	m_pLoadModel->numClusters = 0;

	if( m_pLoadModel != r_models )
	{
		MsgDev( D_ERROR, "loaded a brush model after the world\n");
		return;
	}

	header = (dheader_t *)buffer;

	// Byte swap the header fields and sanity check
	SwapBlock( (int *)header, sizeof(dheader_t));

	if( header->version != BSPMOD_VERSION )
		Host_Error( "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, header->version, BSPMOD_VERSION );
	mod_base = (byte *)header;

	// load into heap
	R_LoadShaders( mod_base, &header->lumps[LUMP_SHADERS]);
	R_LoadPlanes( mod_base, &header->lumps[LUMP_PLANES]);
	R_LoadVertexes( mod_base, &header->lumps[LUMP_VERTICES]);
	R_LoadIndexes( mod_base, &header->lumps[LUMP_INDICES]);
	R_LoadSurfaces( mod_base, &header->lumps[LUMP_SURFACES]);
	R_LoadMarkSurfaces( mod_base, &header->lumps[LUMP_LEAFSURFACES]);
	R_LoadLeafNodes( mod_base, &header->lumps[LUMP_NODES], &header->lumps[LUMP_LEAFS] );
	R_LoadVisibility( mod_base, &header->lumps[LUMP_VISIBILITY]);
	R_LoadSubmodels( mod_base, &header->lumps[LUMP_MODELS]);
	R_LoadLightgrid( mod_base, &header->lumps[LUMP_LIGHTGRID]);

	R_LoadLightmaps();					// load external lightmaps

	mod->registration_sequence = registration_sequence;	// register model
}

/*
=================
Mod_LoadStudioModel
=================
*/
void Mod_LoadStudioModel( rmodel_t *mod, const void *buffer )
{
	R_StudioLoadModel( mod, buffer );
	mod->type = mod_studio;
}

/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel( rmodel_t *mod, const void *buffer )
{
	R_SpriteLoadModel( mod, buffer );
	mod->type = mod_sprite;
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
		if( i < 1 || !r_worldModel || i >= r_worldModel->numSubmodels )
		{
			MsgDev( D_WARN, "Warning: bad inline model number %i\n", i );
			return NULL;
		}
		// prolonge registration
		r_inlinemodels[i].registration_sequence = registration_sequence;
		return &r_inlinemodels[i];
	}

	// search the currently loaded models
	for( i = 0, mod = r_models; i < r_nummodels; i++, mod++ )
	{
		if( !mod->name[0] ) continue;
		if( !com.strcmp( mod->name, name ))
		{
			// prolonge registration
			mod->registration_sequence = registration_sequence;
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

	com.strncpy( mod->name, name, MAX_STRING );
	
	// load the file
	buf = (uint *)FS_LoadFile( mod->name, NULL );
	if( !buf )
	{
		if( crash ) Host_Error( "Mod_NumForName: %s not found\n", mod->name );
		memset( mod->name, 0, sizeof( mod->name ));
		return NULL;
	}

	mod->mempool = Mem_AllocPool(va("^1%s^7", mod->name ));
	m_pLoadModel = mod;
	
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

	registration_sequence++;
	com.sprintf( fullname, "maps/%s.bsp", mapname );

	// explicitly free the old map if different
	if( com.strcmp( r_models[0].name, fullname ))
		Mod_Free( &r_models[0] );
	r_worldModel = Mod_ForName( fullname, true );
	r_viewCluster = -1;

	// load some needed shaders
	r_waterCausticsShader = R_FindShader( "waterCaustics", SHADER_TEXTURE, 0 );
	r_slimeCausticsShader = R_FindShader( "slimeCaustics", SHADER_TEXTURE, 0 );
	r_lavaCausticsShader = R_FindShader( "lavaCaustics", SHADER_TEXTURE, 0 );
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_RegisterModel

@@@@@@@@@@@@@@@@@@@@@
*/
rmodel_t *R_RegisterModel( const char *name )
{
	rmodel_t	*mod;
	
	mod = Mod_ForName( name, false );
	R_ShaderRegisterImages( mod );
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
		if( mod->registration_sequence != registration_sequence )
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

	r_worldModel = NULL;
	r_frameCount = 1;				// no dlight cache
	r_nummodels = 0;
	r_viewCluster = -1;				// force markleafs

	r_worldEntity = &r_entities[0];		// First entity is the world
	memset( r_worldEntity, 0, sizeof( ref_entity_t ));
	r_worldEntity->ent_type = ED_NORMAL;
	r_worldEntity->model = r_worldModel;
	AxisClear( r_worldEntity->axis );
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

	r_worldModel = NULL;
	r_worldEntity = NULL;

	Mod_FreeAll();
	memset( r_models, 0, sizeof( r_models ));
	r_nummodels = 0;
}
