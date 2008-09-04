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

const char *R_GetStringFromTable( int index )
{
	if( m_pLoadModel->stringdata )
		return &m_pLoadModel->stringdata[m_pLoadModel->stringtable[index]];
	return NULL;
}

/*
===============
R_PointInLeaf
===============
*/
leaf_t *R_PointInLeaf( const vec3_t p )
{
	node_t		*node;
	cplane_t		*plane;
	float		d;	

	if( !r_worldModel || !r_worldModel->nodes )
		Host_Error( "Mod_PointInLeaf: bad model\n" );

	node = r_worldModel->nodes;
	while( 1 )
	{
		if( node->contents != -1 )
			return (leaf_t *)node;
		plane = node->plane;
		d = DotProduct( p, plane->normal ) - plane->dist;
		if( d > 0 ) node = node->children[0];
		else node = node->children[1];
	}
	return NULL; // never reached
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
	if( cluster == -1 || !r_worldModel || !r_worldModel->vis )
		return r_fullvis;
	return R_DecompressVis((byte *)r_worldModel->vis + r_worldModel->vis->bitOfs[cluster][DVIS_PVS]);
}

/*
=======================================================================

BRUSH MODELS

=======================================================================
*/
/*
=================
R_LoadStringData
=================
*/
void R_LoadStringData( const byte *base, const lump_t *l )
{
	if( !l->filelen )
	{
		m_pLoadModel->stringdata = NULL;
		return;
	}
	m_pLoadModel->stringdata = (char *)Mem_Alloc( m_pLoadModel->mempool, l->filelen );
	Mem_Copy( m_pLoadModel->stringdata, base + l->fileofs, l->filelen );
}

/*
=================
R_LoadStringTable
=================
*/
void R_LoadStringTable( const byte *base, const lump_t *l )
{	
	int	*in, *out;
	int	i, count;
	
	in = (void *)(base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("R_LoadStringTable: funny lump size in %s\n", m_pLoadModel->name );
	count = l->filelen / sizeof(*in);
	m_pLoadModel->stringtable = out = (int *)Mem_Alloc( m_pLoadModel->mempool, l->filelen );
	for ( i = 0; i < count; i++) out[i] = LittleLong( in[i] );
}

/*
=================
R_LoadVertexes
=================
*/
static void R_LoadVertexes( const byte *base, const lump_t *l )
{
	dvertex_t		*in;
	vertex_t		*out;
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
	}
}

/*
=================
R_LoadEdges
=================
*/
static void R_LoadEdges( const byte *base, const lump_t *l )
{
	dedge_t	*in;
	edge_t	*out;
	int 	i;

	in = (dedge_t *)(base + l->fileofs);
	if (l->filelen % sizeof(dedge_t))
		Host_Error( "R_LoadEdges: funny lump size in '%s'\n", m_pLoadModel->name);

	m_pLoadModel->numEdges = l->filelen / sizeof(dedge_t);
	m_pLoadModel->edges = out = Mem_Alloc( m_pLoadModel->mempool, m_pLoadModel->numEdges * sizeof(edge_t));

	for( i = 0; i < m_pLoadModel->numEdges; i++, in++, out++ )
	{
		out->v[0] = (uint)LittleLong(in->v[0]);
		out->v[1] = (uint)LittleLong(in->v[1]);
	}
}

/*
=================
R_LoadSurfEdges
=================
*/
static void R_LoadSurfEdges( const byte *base, const lump_t *l )
{
	int	*in, *out;
	int	i;
	
	in = (int *)(base + l->fileofs);
	if (l->filelen % sizeof(int))
		Host_Error( "R_LoadSurfEdges: funny lump size in '%s'\n", m_pLoadModel->name );

	m_pLoadModel->numSurfEdges = l->filelen / sizeof(int);
	m_pLoadModel->surfEdges = out = Mem_Alloc( m_pLoadModel->mempool, m_pLoadModel->numSurfEdges * sizeof(int));

	for( i = 0; i < m_pLoadModel->numSurfEdges; i++ )
		out[i] = LittleLong(in[i]);
}

/*
=================
R_LoadLighting
=================
*/
static void R_LoadLighting( const byte *base, const lump_t *l )
{
	if( r_fullbright->integer || !l->filelen )
		return;

	m_pLoadModel->lightData = Mem_Alloc( m_pLoadModel->mempool, l->filelen );
	Mem_Copy( m_pLoadModel->lightData, base + l->fileofs, l->filelen );
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
R_LoadTextures
=================
*/
void R_LoadTextures( const byte *base, const lump_t *l )
{
	dmiptex_t		*in, *in2;
	mipTex_t		*out, *step;
	int 		i, next, count;

	in = in2 = (void *)(base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("R_LoadTextures: funny lump size in '%s'\n", m_pLoadModel->name );
	count = l->filelen / sizeof(*in);

	out = (mipTex_t *)Mem_Alloc( m_pLoadModel->mempool, count * sizeof(*out));
 
	m_pLoadModel->textures = out;
	m_pLoadModel->numTextures = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		com.strncpy( out->name, R_GetStringFromTable( LittleLong( in->s_name )), MAX_STRING );
		out->width = LittleLong( in->size[0] );
		out->height = LittleLong( in->size[1] );

		// make stubs
		out->image = r_defaultTexture;

		next = LittleLong( in->s_next );
		if( next ) out->next = m_pLoadModel->textures + next;
		else out->next = NULL;
	}

	// count animation frames
	for( i = 0; i < count; i++ )
	{
		out = &m_pLoadModel->textures[i];
		out->numframes = 1;
		for( step = out->next; step && step != out; step = step->next )
			out->numframes++;
	}
}

/*
=================
R_LoadTexInfo
=================
*/
static void R_LoadTexInfo( const byte *base, const lump_t *l )
{
	dtexinfo_t	*in;
	texInfo_t		*out;
	int		texnum;
	int		i, j, count;
	uint		surfaceParm = 0;

	in = (void *)(base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error( "R_LoadTexinfo: funny lump size in '%s'\n", m_pLoadModel->name );
	count = l->filelen / sizeof(*in);
          out = Mem_Alloc( m_pLoadModel->mempool, count * sizeof(*out));
	
	m_pLoadModel->texInfo = out;
	m_pLoadModel->numTexInfo = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		for( j = 0; j < 8; j++ )
			out->vecs[0][j] = LittleFloat( in->vecs[0][j] );

		out->flags = LittleLong( in->flags );
		out->contents = LittleLong( in->contents );
		texnum = LittleLong( in->texnum );

		if( texnum < 0 || texnum > m_pLoadModel->numTextures )
			Host_Error( "R_LoadTexInfo: bad texture number in '%s'\n", m_pLoadModel->name );
		out->texture = m_pLoadModel->textures + texnum;

		if( out->flags & (SURF_SKY|SURF_NODRAW))
		{
			// this is not actually needed
			out->shader = r_defaultShader;
			continue;
		}

		// get surfaceParm
		if( out->flags & (SURF_WARP|SURF_ALPHA|SURF_BLEND|SURF_ADDITIVE|SURF_CHROME))
		{
			surfaceParm = 0;

			if( out->flags & SURF_WARP )
				surfaceParm |= SURFACEPARM_WARP;
			if( out->flags & SURF_ALPHA )
				surfaceParm |= SURFACEPARM_ALPHA;
			if( out->flags & SURF_BLEND )
				surfaceParm |= SURFACEPARM_BLEND;
			if( out->flags & SURF_ADDITIVE )
				surfaceParm |= SURFACEPARM_ADDITIVE;
			if( out->flags & SURF_ADDITIVE )
				surfaceParm |= SURFACEPARM_CHROME;
		}
		else surfaceParm = SURFACEPARM_LIGHTMAP;

		if( out->flags & SURF_MIRROR|SURF_PORTAL )
		{
			surfaceParm &= ~SURFACEPARM_LIGHTMAP;
		}

		// performance evaluation option
		if( r_singleshader->integer )
		{
			out->shader = r_defaultShader;
			continue;
		}

		// lightmap visualization tool
		if( r_lightmap->integer && (surfaceParm & SURFACEPARM_LIGHTMAP))
		{
			out->shader = r_lightmapShader;
			continue;
		}

		// load the shader
		out->shader = R_FindShader( out->texture->name, SHADER_TEXTURE, surfaceParm );

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
	int		i, e;
	vertex_t		*v;

	ClearBounds( surf->mins, surf->maxs );

	for( i = 0; i < surf->numEdges; i++ )
	{
		e = m_pLoadModel->surfEdges[surf->firstEdge + i];
		if( e >= 0 ) v = &m_pLoadModel->vertexes[m_pLoadModel->edges[e].v[0]];
		else v = &m_pLoadModel->vertexes[m_pLoadModel->edges[-e].v[1]];
		AddPointToBounds( v->point, surf->mins, surf->maxs );
	}
}

/*
=================
R_CalcSurfaceExtents

Fills in surf->textureMins and surf->extents
=================
*/
static void R_CalcSurfaceExtents( surface_t *surf )
{
	float		mins[2], maxs[2], val;
	int  		bmins[2], bmaxs[2];
	int 		i, j, e;
	vertex_t		*v;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -999999;

	for( i = 0; i < surf->numEdges; i++ )
	{
		e = m_pLoadModel->surfEdges[surf->firstEdge + i];
		if( e >= 0 ) v = &m_pLoadModel->vertexes[m_pLoadModel->edges[e].v[0]];
		else v = &m_pLoadModel->vertexes[m_pLoadModel->edges[-e].v[1]];

		for( j = 0; j < 2; j++ )
		{
			val = DotProduct(v->point, surf->texInfo->vecs[j]) + surf->texInfo->vecs[j][3];
			if( val < mins[j] ) mins[j] = val;
			if( val > maxs[j] ) maxs[j] = val;
		}
	}

	for( i = 0; i < 2; i++ )
	{
		bmins[i] = floor(mins[i] / 16);
		bmaxs[i] = ceil(maxs[i] / 16);

		surf->textureMins[i] = bmins[i] * 16;
		surf->extents[i] = (bmaxs[i] - bmins[i]) * 16;
	}
}

/*
=================
R_SubdividePolygon
=================
*/
static void R_SubdividePolygon( surface_t *surf, int numVerts, float *verts )
{
	int		i, j;
	vec3_t		mins, maxs;
	float		*v;
	vec3_t		front[64], back[64];
	int		f, b;
	float		m, dist, dists[64];
	int		subdivideSize;
	uint		index;
	float		s, t;
	vec3_t		total;
	vec2_t		totalST, totalLM;
	surfPoly_t	*p;
	texInfo_t		*texInfo = surf->texInfo;

	subdivideSize = texInfo->shader->tessSize;

	ClearBounds( mins, maxs );
	for( i = 0, v = verts; i < numVerts; i++, v += 3 )
		AddPointToBounds( v, mins, maxs );

	for( i = 0; i < 3; i++ )
	{
		m = subdivideSize * floor(((mins[i] + maxs[i]) * 0.5) / subdivideSize + 0.5);
		if( maxs[i] - m < 8 ) continue;
		if( m - mins[i] < 8 ) continue;

		// cut it
		v = verts + i;
		for( j = 0; j < numVerts; j++, v += 3 )
			dists[j] = *v - m;

		// wrap cases
		dists[j] = dists[0];
		v -= i;
		VectorCopy( verts, v );

		for( f = j = b = 0, v = verts; j < numVerts; j++, v += 3 )
		{
			if( dists[j] >= 0 )
			{
				VectorCopy(v, front[f]);
				f++;
			}
			if( dists[j] <= 0 )
			{
				VectorCopy(v, back[b]);
				b++;
			}
			
			if( dists[j] == 0 || dists[j+1] == 0 )
				continue;
			
			if((dists[j] > 0) != (dists[j+1] > 0))
			{
				// clip point
				dist = dists[j] / (dists[j] - dists[j+1]);
				front[f][0] = back[b][0] = v[0] + (v[3] - v[0]) * dist;
				front[f][1] = back[b][1] = v[1] + (v[4] - v[1]) * dist;
				front[f][2] = back[b][2] = v[2] + (v[5] - v[2]) * dist;
				f++;
				b++;
			}
		}

		R_SubdividePolygon( surf, f, front[0] );
		R_SubdividePolygon( surf, b, back[0] );
		return;
	}

	p = Mem_Alloc( m_pLoadModel->mempool, sizeof(surfPoly_t));
	p->next = surf->poly;
	surf->poly = p;

	// create indices
	p->numIndices = (numVerts * 3);
	p->indices = Mem_Alloc( m_pLoadModel->mempool, p->numIndices * sizeof(unsigned));

	for( i = 0, index = 2; i < p->numIndices; i += 3, index++ )
	{
		p->indices[i+0] = 0;
		p->indices[i+1] = index - 1;
		p->indices[i+2] = index;
	}

	// create vertices
	p->numVertices = (numVerts + 2);
	p->vertices = Mem_Alloc( m_pLoadModel->mempool, p->numVertices * sizeof(surfPolyVert_t));

	VectorClear( total );
	totalST[0] = totalST[1] = 0;
	totalLM[0] = totalLM[1] = 0;

	for( i = 0; i < numVerts; i++, verts += 3 )
	{
		// vertex
		VectorCopy( verts, p->vertices[i+1].xyz );
		VectorAdd( total, verts, total );

		// texture coordinates
		s = DotProduct( verts, texInfo->vecs[0] ) + texInfo->vecs[0][3];
		if( texInfo->texture->width != -1 ) s /= texInfo->texture->width;
		else s /= texInfo->texture->image->width;

		t = DotProduct( verts, texInfo->vecs[1] ) + texInfo->vecs[1][3];
		if( texInfo->texture->height != -1) t /= texInfo->texture->height;
		else t /= texInfo->texture->image->height;

		p->vertices[i+1].st[0] = s;
		p->vertices[i+1].st[1] = t;

		totalST[0] += s;
		totalST[1] += t;

		// lightmap texture coordinates
		s = DotProduct(verts, texInfo->vecs[0]) + texInfo->vecs[0][3] - surf->textureMins[0];
		s += surf->lmS * 16;
		s += 8;
		s /= LIGHTMAP_WIDTH * 16;

		t = DotProduct(verts, texInfo->vecs[1]) + texInfo->vecs[1][3] - surf->textureMins[1];
		t += surf->lmT * 16;
		t += 8;
		t /= LIGHTMAP_HEIGHT * 16;

		p->vertices[i+1].lightmap[0] = s;
		p->vertices[i+1].lightmap[1] = t;

		totalLM[0] += s;
		totalLM[1] += t;

		// vertex color
		p->vertices[i+1].color[0] = 1.0f;
		p->vertices[i+1].color[1] = 1.0f;
		p->vertices[i+1].color[2] = 1.0f;
		p->vertices[i+1].color[3] = 1.0f;
	}

	// vertex
	VectorScale( total, 1.0 / numVerts, p->vertices[0].xyz );

	// texture coordinates
	p->vertices[0].st[0] = totalST[0] / numVerts;
	p->vertices[0].st[1] = totalST[1] / numVerts;

	// lightmap texture coordinates
	p->vertices[0].lightmap[0] = totalLM[0] / numVerts;
	p->vertices[0].lightmap[1] = totalLM[1] / numVerts;

	// vertex color
	p->vertices[0].color[0] = 1.0f;
	p->vertices[0].color[1] = 1.0f;
	p->vertices[0].color[2] = 1.0f;
	p->vertices[0].color[3] = 1.0f;

	// copy first vertex to last
	Mem_Copy( &p->vertices[i+1], &p->vertices[1], sizeof(surfPolyVert_t));
}

/*
=================
R_BuildPolygon
=================
*/
static void R_BuildPolygon( surface_t *surf, int numVerts, float *verts )
{
	int		i;
	uint		index;
	float		s, t;
	texInfo_t		*texInfo = surf->texInfo;
	surfPoly_t	*p;
	
	p = Mem_Alloc( m_pLoadModel->mempool, sizeof(surfPoly_t));
	p->next = surf->poly;
	surf->poly = p;

	// create indices
	p->numIndices = (numVerts - 2) * 3;
	p->indices = Mem_Alloc( m_pLoadModel->mempool, p->numIndices * sizeof(uint));

	for( i = 0, index = 2; i < p->numIndices; i += 3, index++ )
	{
		p->indices[i+0] = 0;
		p->indices[i+1] = index-1;
		p->indices[i+2] = index;
	}

	// create vertices
	p->numVertices = numVerts;
	p->vertices = Mem_Alloc( m_pLoadModel->mempool, p->numVertices * sizeof(surfPolyVert_t));
	
	for( i = 0; i < numVerts; i++, verts += 3 )
	{
		// vertex
		VectorCopy( verts, p->vertices[i].xyz );

		// texture coordinates
		s = DotProduct( verts, texInfo->vecs[0] ) + texInfo->vecs[0][3];
		if( texInfo->texture->width != -1 ) s /= texInfo->texture->width;
		else s /= texInfo->texture->image->width;

		t = DotProduct( verts, texInfo->vecs[1] ) + texInfo->vecs[1][3];
		if( texInfo->texture->height != -1) t /= texInfo->texture->height;
		else t /= texInfo->texture->image->height;

		p->vertices[i].st[0] = s;
		p->vertices[i].st[1] = t;

		// lightmap texture coordinates
		s = DotProduct(verts, texInfo->vecs[0]) + texInfo->vecs[0][3] - surf->textureMins[0];
		s += surf->lmS * 16;
		s += 8;
		s /= LIGHTMAP_WIDTH * 16;

		t = DotProduct(verts, texInfo->vecs[1]) + texInfo->vecs[1][3] - surf->textureMins[1];
		t += surf->lmT * 16;
		t += 8;
		t /= LIGHTMAP_HEIGHT * 16;

		p->vertices[i].lightmap[0] = s;
		p->vertices[i].lightmap[1] = t;

		// vertex color
		p->vertices[i+1].color[0] = 1.0f;
		p->vertices[i+1].color[1] = 1.0f;
		p->vertices[i+1].color[2] = 1.0f;
		p->vertices[i+1].color[3] = 1.0f;
	}
}

/*
=================
R_BuildSurfacePolygons
=================
*/
static void R_BuildSurfacePolygons( surface_t *surf )
{
	int		i, e;
	vec3_t		verts[MAX_BUILD_SIDES];
	vertex_t		*v;

	// convert edges back to a normal polygon
	for( i = 0; i < surf->numEdges; i++ )
	{
		e = m_pLoadModel->surfEdges[surf->firstEdge + i];
		if( e >= 0 ) v = &m_pLoadModel->vertexes[m_pLoadModel->edges[e].v[0]];
		else v = &m_pLoadModel->vertexes[m_pLoadModel->edges[-e].v[1]];
		VectorCopy( v->point, verts[i] );
	}

	if( surf->texInfo->shader->flags & SHADER_TESSSIZE )
		R_SubdividePolygon( surf, surf->numEdges, verts[0] );
	else R_BuildPolygon( surf, surf->numEdges, verts[0] );
}

/*
=================
R_LoadFaces
=================
*/
static void R_LoadFaces( const byte *base, const lump_t *l )
{
	dface_t		*in;
	surface_t 	*out;
	int		i, lightofs;

	in = (dface_t *)(base + l->fileofs);
	if (l->filelen % sizeof(dface_t))
		Host_Error( "R_LoadFaces: funny lump size in '%s'\n", m_pLoadModel->name );

	m_pLoadModel->numSurfaces = l->filelen / sizeof(dface_t);
	m_pLoadModel->surfaces = out = Mem_Alloc( m_pLoadModel->mempool, m_pLoadModel->numSurfaces * sizeof(surface_t));

	R_BeginBuildingLightmaps();

	for( i = 0; i < m_pLoadModel->numSurfaces; i++, in++, out++ )
	{
		out->firstEdge = LittleLong( in->firstedge );
		out->numEdges = LittleLong( in->numedges );

		if( LittleShort( in->side )) out->flags |= SURF_PLANEBACK;
		out->plane = m_pLoadModel->planes + LittleLong( in->planenum );
		out->texInfo = m_pLoadModel->texInfo + LittleLong( in->texinfo );

		R_CalcSurfaceBounds( out );
		R_CalcSurfaceExtents( out );

		// tangent vectors
		VectorCopy( out->texInfo->vecs[0], out->tangent );
		VectorNegate( out->texInfo->vecs[1], out->binormal );

		if(!(out->flags & SURF_PLANEBACK))
			VectorCopy( out->plane->normal, out->normal );
		else VectorNegate( out->plane->normal, out->normal );

		VectorNormalize( out->tangent );
		VectorNormalize( out->binormal );
		VectorNormalize( out->normal );

		// lighting info
		out->lmWidth = (out->extents[0] >> 4) + 1;
		out->lmHeight = (out->extents[1] >> 4) + 1;

		if( out->texInfo->flags & (SURF_SKY|SURF_WARP|SURF_NODRAW))
			lightofs = -1;
		else lightofs = LittleLong( in->lightofs );

		if( m_pLoadModel->lightData && lightofs != -1 )
			out->lmSamples = m_pLoadModel->lightData + lightofs;

		while( out->numStyles < MAX_LIGHTSTYLES && in->styles[out->numStyles] != 255 )
		{
			out->styles[out->numStyles] = in->styles[out->numStyles];
			out->numStyles++;
		}

		// create lightmap
		R_BuildSurfaceLightmap( out );

		// create polygons
		R_BuildSurfacePolygons( out );
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
	int	i;

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
R_LoadLeafs
=================
*/
static void R_LoadLeafs( const byte *base, const lump_t *l )
{
	dleaf_t	*in;
	leaf_t	*out;
	int	i, j;

	in = (dleaf_t *)(base + l->fileofs);
	if (l->filelen % sizeof(dleaf_t))
		Host_Error( "R_LoadLeafs: funny lump size in '%s'\n", m_pLoadModel->name );

	m_pLoadModel->numLeafs = l->filelen / sizeof(dleaf_t);
	m_pLoadModel->leafs = out = Mem_Alloc( m_pLoadModel->mempool, m_pLoadModel->numLeafs * sizeof(leaf_t));

	for( i = 0; i < m_pLoadModel->numLeafs; i++, in++, out++ )
	{
		for( j = 0; j < 3; j++ )
		{
			out->mins[j] = LittleLong( in->mins[j] );
			out->maxs[j] = LittleLong( in->maxs[j] );
		}

		out->contents = LittleLong( in->contents );
		out->cluster = LittleLong( in->cluster );
		out->area = LittleLong( in->area );
		out->firstMarkSurface = m_pLoadModel->markSurfaces + LittleLong( in->firstleafface );
		out->numMarkSurfaces = LittleLong( in->numleaffaces );

		// mark the surfaces for caustics
		if( out->contents & (CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA))
		{
			for( j = 0; j < out->numMarkSurfaces; j++ )
			{
				if( out->firstMarkSurface[j]->texInfo->flags & SURF_WARP )
					continue;	// HACK: ignore warped surfaces

				if( out->contents & CONTENTS_WATER )
					out->firstMarkSurface[j]->flags |= SURF_WATERCAUSTICS;
				if( out->contents & CONTENTS_SLIME )
					out->firstMarkSurface[j]->flags |= SURF_SLIMECAUSTICS;
				if( out->contents & CONTENTS_LAVA )
					out->firstMarkSurface[j]->flags |= SURF_LAVACAUSTICS;
			}
		}
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
	if( node->contents != -1 ) return;

	R_NodeSetParent( node->children[0], node );
	R_NodeSetParent( node->children[1], node );
}

/*
=================
R_LoadNodes
=================
*/
static void R_LoadNodes( const byte *base, const lump_t *l )
{
	dnode_t		*in;
	node_t		*out;
	int		i, j, p;

	in = (dnode_t *)(base + l->fileofs);
	if (l->filelen % sizeof(dnode_t))
		Host_Error( "R_LoadNodes: funny lump size in '%s'\n", m_pLoadModel->name );

	m_pLoadModel->numNodes = l->filelen / sizeof(dnode_t);
	m_pLoadModel->nodes = out = Mem_Alloc( m_pLoadModel->mempool, m_pLoadModel->numNodes * sizeof(node_t));

	for( i = 0; i < m_pLoadModel->numNodes; i++, in++, out++ )
	{
		for( j = 0; j < 3; j++ )
		{
			out->mins[j] = LittleLong(in->mins[j]);
			out->maxs[j] = LittleLong(in->maxs[j]);
		}
	
		out->plane = m_pLoadModel->planes + LittleLong( in->planenum );
		out->contents = -1;
		out->firstSurface = LittleLong( in->firstface );
		out->numSurfaces = LittleLong( in->numfaces );

		for( j = 0; j < 2; j++ )
		{
			p = LittleLong( in->children[j] );
			if( p >= 0 ) out->children[j] = m_pLoadModel->nodes + p;
			else out->children[j] = (node_t *)(m_pLoadModel->leafs + (-1 - p));
		}
	}

	// set nodes and leafs
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
		model->numLeafs = bm->visLeafs;
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
	byte		*data;
	dlightgrid_t	*header;
	lightGrid_t	*in, *out;
	int		i;

	if( !l->filelen ) return;
	data = (byte *)(base + l->fileofs);
	header = (dlightgrid_t *)data;
	if(( l->filelen - sizeof(dlightgrid_t)) % sizeof(lightGrid_t))
		Host_Error( "R_LoadLightgrid: funny lump size in '%s'\n", m_pLoadModel->name );

	m_pLoadModel->gridMins[0] = LittleFloat( header->mins[0] );
	m_pLoadModel->gridMins[1] = LittleFloat( header->mins[1] );
	m_pLoadModel->gridMins[2] = LittleFloat( header->mins[2] );

	m_pLoadModel->gridSize[0] = LittleFloat( header->size[0] );
	m_pLoadModel->gridSize[1] = LittleFloat( header->size[1] );
	m_pLoadModel->gridSize[2] = LittleFloat( header->size[2] );

	m_pLoadModel->gridBounds[0] = LittleLong( header->bounds[0] );
	m_pLoadModel->gridBounds[1] = LittleLong( header->bounds[1] );
	m_pLoadModel->gridBounds[2] = LittleLong( header->bounds[2] );
	m_pLoadModel->gridBounds[3] = LittleLong( header->bounds[3] );
	m_pLoadModel->gridPoints = LittleLong( header->points );
	in = (lightGrid_t *)(data + sizeof(dlightgrid_t));

	m_pLoadModel->lightGrid = out = Mem_Alloc( m_pLoadModel->mempool, m_pLoadModel->gridPoints * sizeof(lightGrid_t));

	for( i = 0; i < m_pLoadModel->gridPoints; i++, in++, out++ )
	{
		out->lightDir[0] = LittleFloat( in->lightDir[0] );
		out->lightDir[1] = LittleFloat( in->lightDir[1] );
		out->lightDir[2] = LittleFloat( in->lightDir[2] );
	}
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
	R_LoadStringData( mod_base, &header->lumps[LUMP_STRINGDATA]);
	R_LoadStringTable( mod_base, &header->lumps[LUMP_STRINGTABLE]);
	R_LoadVertexes( mod_base, &header->lumps[LUMP_VERTEXES]);
	R_LoadEdges( mod_base, &header->lumps[LUMP_EDGES]);
	R_LoadSurfEdges( mod_base, &header->lumps[LUMP_SURFEDGES]);
	R_LoadLighting( mod_base, &header->lumps[LUMP_LIGHTING]);
	R_LoadLightgrid( mod_base, &header->lumps[LUMP_LIGHTGRID]);
	R_LoadPlanes( mod_base, &header->lumps[LUMP_PLANES]);
	R_LoadTextures( mod_base, &header->lumps[LUMP_TEXTURES]);
	R_LoadTexInfo( mod_base, &header->lumps[LUMP_TEXINFO]);
	R_LoadFaces( mod_base, &header->lumps[LUMP_FACES]);
	R_LoadMarkSurfaces( mod_base, &header->lumps[LUMP_LEAFFACES]);
	R_LoadVisibility( mod_base, &header->lumps[LUMP_VISIBILITY]);
	R_LoadLeafs( mod_base, &header->lumps[LUMP_LEAFS]);
	R_LoadNodes( mod_base, &header->lumps[LUMP_NODES]);
	R_LoadSubmodels( mod_base, &header->lumps[LUMP_MODELS]);

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
	r_oldViewCluster = -1;	// force markleafs

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
	int	i;
	
	mod = Mod_ForName( name, false );
	if( mod )
	{
		switch( mod->type )
		{
		case mod_world:
		case mod_brush:
		case mod_studio:
		case mod_sprite:
			for( i = 0; i < mod->numTextures; i++ )
				mod->textures[i].image->registration_sequence = registration_sequence;
			break;
		default: return NULL; // mod_bad, etc
		}
	}
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
	r_viewCluster = r_oldViewCluster = -1;		// force markleafs

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
