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
static dshader_t	*r_map_shaders[MAX_MAP_SHADERS];	// hold contents and texture size
static rmodel_t	r_inlinemodels[MAX_MODELS];
static byte	r_fullvis[MAX_MAP_LEAFS/8];
static rmodel_t	r_models[MAX_MODELS];
int		registration_sequence;
rmodel_t		*m_pLoadModel;
static string	r_skyShader;
static int	r_nummodels;

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

	row = (r_worldModel->vis->numclusters+7)>>3;	
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
	if (!r_worldModel || !r_worldModel->vis || cluster < 0 || cluster >= r_worldModel->vis->numclusters )
		return r_worldModel->novis;
	return R_DecompressVis((byte *)r_worldModel->vis + r_worldModel->vis->bitofs[cluster][DVIS_PVS]);
}

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
	dsurfedge_t	*in, *out;
	int		i;
	
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
R_LoadShaders
=================
*/
void R_LoadShaders( const byte *base, const lump_t *l )
{
	dshader_t		*in;
	int 		i, count;
	int		surfaceParm, contents;
	int		shaderType = SHADER_TEXTURE;
	cvar_t		*scr_loading = Cvar_Get("scr_loading", "0", 0, "loading bar progress" );

	in = (void *)(base + l->fileofs);
	if( l->filelen % sizeof( *in ))
		Host_Error( "R_LoadShaders: funny lump size in '%s'\n", m_pLoadModel->name );
	count = l->filelen / sizeof( *in );

	m_pLoadModel->shaders = Mem_Alloc( m_pLoadModel->mempool, count * sizeof( shader_t* ));
	m_pLoadModel->numShaders = count;

	for( i = 0; i < count; i++, in++ )
	{
		surfaceParm = LittleLong( in->surfaceFlags );
		contents = LittleLong( in->contentFlags );
		r_map_shaders[i] = in; // hold pointers for texinfo

		Cvar_SetValue( "scr_loading", scr_loading->value + 50.0f / count );
		if( ri.UpdateScreen ) ri.UpdateScreen();

		// performance evaluation option                    
                    if( r_singleshader->integer )
		{
			m_pLoadModel->shaders[i] = tr.defaultShader;
			continue;
		}

		if( surfaceParm & SURF_NODRAW ) 
		{
			m_pLoadModel->shaders[i] = tr.nodrawShader;
			continue;
		}
		
		if( surfaceParm & SURF_SKY )
		{
			// setup sky shader
			com.strncpy( r_skyShader, in->name, MAX_STRING );
			shaderType = SHADER_SKY;
		}
		else shaderType = SHADER_TEXTURE;

		// lightmap visualization tool
		if( r_lightmap->integer && !(surfaceParm & SURF_NOLIGHTMAP))
		{
			m_pLoadModel->shaders[i] = tr.lightmapShader;
			continue;
		}

		// detect surfaceParm
		if( !m_pLoadModel->lightData || surfaceParm & SURF_WARP )
			surfaceParm |= SURF_NOLIGHTMAP;

		m_pLoadModel->shaders[i] = R_FindShader( in->name, shaderType, surfaceParm );
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
	int		shadernum;
	int		i, j, count;
	uint		surfaceParm = 0;

	in = (void *)(base + l->fileofs);
	if( l->filelen % sizeof( *in ))
		Host_Error( "R_LoadTexinfo: funny lump size in '%s'\n", m_pLoadModel->name );
	count = l->filelen / sizeof(*in);
          out = Mem_Alloc( m_pLoadModel->mempool, count * sizeof( *out ));
	
	m_pLoadModel->texInfo = out;
	m_pLoadModel->numTexInfo = count;

	for ( i = 0; i < count; i++, in++, out++ )
	{
		for( j = 0; j < 8; j++ )
			out->vecs[0][j] = LittleFloat( in->vecs[0][j] );

		shadernum = LittleLong( in->shadernum );
		if( shadernum < 0 || shadernum > m_pLoadModel->numShaders )
			Host_Error( "R_LoadTexInfo: bad shader number in '%s'\n", m_pLoadModel->name );
		out->shader = m_pLoadModel->shaders[shadernum];	// now all pointers are valid

		// also copy additional info from shaderInfo
		out->contentFlags = LittleLong( r_map_shaders[shadernum]->contentFlags );
		out->surfaceFlags = LittleLong( r_map_shaders[shadernum]->surfaceFlags );
		out->width = LittleLong( r_map_shaders[shadernum]->size[0] );
		out->height = LittleLong( r_map_shaders[shadernum]->size[1] );
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

	p = Mem_Alloc( m_pLoadModel->mempool, sizeof( surfPoly_t ));
	p->next = surf->poly;
	surf->poly = p;

	// create indices
	p->numIndices = (numVerts * 3);
	p->indices = Mem_Alloc( m_pLoadModel->mempool, p->numIndices * sizeof( uint ));

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
		if( texInfo->width != -1 ) s /= texInfo->width;
		else s /= texInfo->shader->stages[0]->bundles[0]->textures[0]->width;

		t = DotProduct( verts, texInfo->vecs[1] ) + texInfo->vecs[1][3];
		if( texInfo->height != -1) t /= texInfo->height;
		else t /= texInfo->shader->stages[0]->bundles[0]->textures[0]->height;

		p->vertices[i+1].st[0] = s;
		p->vertices[i+1].st[1] = t;

		totalST[0] += s;
		totalST[1] += t;

		// lightmap texture coordinates
		s = DotProduct(verts, texInfo->vecs[0]) + texInfo->vecs[0][3] - surf->textureMins[0];
		s += surf->lmS * LM_SAMPLE_SIZE;
		s += LM_SAMPLE_SIZE >> 1;
		s /= LM_SIZE * LM_SAMPLE_SIZE;

		t = DotProduct(verts, texInfo->vecs[1]) + texInfo->vecs[1][3] - surf->textureMins[1];
		t += surf->lmT * LM_SAMPLE_SIZE;
		t += LM_SAMPLE_SIZE >> 1;
		t /= LM_SIZE * LM_SAMPLE_SIZE;

		p->vertices[i+1].lm[0] = s;
		p->vertices[i+1].lm[1] = t;

		totalLM[0] += s;
		totalLM[1] += t;

		// vertex color
		p->vertices[i+1].color[0] = 1.0f;
		p->vertices[i+1].color[1] = 1.0f;
		p->vertices[i+1].color[2] = 1.0f;
		p->vertices[i+1].color[3] = 1.0f;

		if( texInfo->surfaceFlags & SURF_TRANS )
			p->vertices[i+1].color[3] *= 0.33;
		else if( texInfo->surfaceFlags & SURF_BLEND )
			p->vertices[i+1].color[3] *= 0.66;
	}

	// vertex
	VectorScale( total, 1.0 / numVerts, p->vertices[0].xyz );

	// texture coordinates
	p->vertices[0].st[0] = totalST[0] / numVerts;
	p->vertices[0].st[1] = totalST[1] / numVerts;

	// lightmap texture coordinates
	p->vertices[0].lm[0] = totalLM[0] / numVerts;
	p->vertices[0].lm[1] = totalLM[1] / numVerts;

	// vertex color
	p->vertices[0].color[0] = 1.0f;
	p->vertices[0].color[1] = 1.0f;
	p->vertices[0].color[2] = 1.0f;
	p->vertices[0].color[3] = 1.0f;

	if( texInfo->surfaceFlags & SURF_TRANS )
		p->vertices[0].color[3] *= 0.33;
	else if( texInfo->surfaceFlags & SURF_BLEND )
		p->vertices[0].color[3] *= 0.66;

	// copy first vertex to last
	Mem_Copy( &p->vertices[i+1], &p->vertices[1], sizeof( surfPolyVert_t ));
}

/*
=================
R_BuildPolygon
=================
*/
static void R_BuildPolygon( surface_t *surf, int numVerts, const float *verts )
{
	int		i;
	uint		index;
	float		s, t;
	surfPoly_t	*p;
	texInfo_t		*texInfo = surf->texInfo;
	
	p = Mem_Alloc( m_pLoadModel->mempool, sizeof( surfPoly_t ));
	p->next = surf->poly;
	surf->poly = p;

	// create indices
	p->numIndices = (numVerts - 2) * 3;
	p->indices = Mem_Alloc( m_pLoadModel->mempool, p->numIndices * sizeof( uint ));

	for( i = 0, index = 2; i < p->numIndices; i += 3, index++ )
	{
		p->indices[i+0] = 0;
		p->indices[i+1] = index-1;
		p->indices[i+2] = index;
	}

	// create vertices
	p->numVertices = numVerts;
	p->vertices = Mem_Alloc( m_pLoadModel->mempool, p->numVertices * sizeof( surfPolyVert_t ));
	
	for( i = 0; i < numVerts; i++, verts += 3 )
	{
		// vertex
		VectorCopy( verts, p->vertices[i].xyz );

		// texture coordinates
		s = DotProduct( verts, texInfo->vecs[0] ) + texInfo->vecs[0][3];
		if( texInfo->width != -1 ) s /= texInfo->width;
		else s /= texInfo->shader->stages[0]->bundles[0]->textures[0]->width;

		t = DotProduct( verts, texInfo->vecs[1] ) + texInfo->vecs[1][3];
		if( texInfo->height != -1) t /= texInfo->height;
		else t /= texInfo->shader->stages[0]->bundles[0]->textures[0]->height;

		p->vertices[i].st[0] = s;
		p->vertices[i].st[1] = t;

		// lightmap texture coordinates
		s = DotProduct( verts, texInfo->vecs[0] ) + texInfo->vecs[0][3] - surf->textureMins[0];
		s += surf->lmS * LM_SAMPLE_SIZE;
		s += LM_SAMPLE_SIZE >> 1;
		s /= LM_SIZE * LM_SAMPLE_SIZE;

		t = DotProduct( verts, texInfo->vecs[1] ) + texInfo->vecs[1][3] - surf->textureMins[1];
		t += surf->lmT * LM_SAMPLE_SIZE;
		t += LM_SAMPLE_SIZE >> 1;
		t /= LM_SIZE * LM_SAMPLE_SIZE;

		p->vertices[i].lm[0] = s;
		p->vertices[i].lm[1] = t;

		// vertex color
		Vector4Set( p->vertices[i].color, 1.0f, 1.0f, 1.0f, 1.0f );

		if( texInfo->surfaceFlags & SURF_TRANS )
			p->vertices[i].color[3] *= 0.33;
		else if( texInfo->surfaceFlags & SURF_BLEND )
			p->vertices[i].color[3] *= 0.66;
	}
}

/*
=================
R_BuildSurfacePolygons
=================
*/
static void R_BuildSurfacePolygons( surface_t *surf )
{
	vec3_t		verts[64];
	int		i, e;
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
static void R_LoadSurfaces( const byte *base, const lump_t *l )
{
	dsurface_t		*in;
	surface_t 	*out;
	int		i, lightofs;

	in = (dsurface_t *)(base + l->fileofs);
	if (l->filelen % sizeof(dsurface_t))
		Host_Error( "R_LoadFaces: funny lump size in '%s'\n", m_pLoadModel->name );

	m_pLoadModel->numSurfaces = l->filelen / sizeof(dsurface_t);
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

		if(!( out->flags & SURF_PLANEBACK ))
			VectorCopy( out->plane->normal, out->normal );
		else VectorNegate( out->plane->normal, out->normal );

		VectorNormalize( out->tangent );
		VectorNormalize( out->binormal );
		VectorNormalize( out->normal );

		// lighting info
		out->lmWidth = (out->extents[0] >> 4) + 1;
		out->lmHeight = (out->extents[1] >> 4) + 1;

		if( out->texInfo->surfaceFlags & (SURF_SKY|SURF_WARP|SURF_NODRAW))
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
	dleafface_t	*in;
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

	// novis lumps attempt to create always
	vis_length = ( m_pLoadModel->numClusters + 63 ) & ~63;
	m_pLoadModel->novis = Mem_Alloc( m_pLoadModel->mempool, vis_length );
	Mem_Set( m_pLoadModel->novis, 0xFF, vis_length );

	if( !l->filelen ) return;
	m_pLoadModel->vis = Mem_Alloc( m_pLoadModel->mempool, l->filelen );
	Mem_Copy( m_pLoadModel->vis, base + l->fileofs, l->filelen );

	m_pLoadModel->vis->numclusters = LittleLong( m_pLoadModel->vis->numclusters );
	for( i = 0; i < m_pLoadModel->vis->numclusters; i++ )
	{
		m_pLoadModel->vis->bitofs[i][0] = LittleLong( m_pLoadModel->vis->bitofs[i][0] );
		m_pLoadModel->vis->bitofs[i][1] = LittleLong( m_pLoadModel->vis->bitofs[i][1] );
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
	m_pLoadModel->numClusters = 0;

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
		out->firstMarkSurface = m_pLoadModel->markSurfaces + LittleLong( in->firstleafsurface );
		out->numMarkSurfaces = LittleLong( in->numleafsurfaces );

		// cluster count using for create novis lump
		if( out->cluster >= m_pLoadModel->numClusters )
			m_pLoadModel->numClusters = out->cluster + 1;
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
		out->firstSurface = LittleLong( in->firstsurface );
		out->numSurfaces = LittleLong( in->numsurfaces );

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
		out->firstFace = LittleLong( in->firstsurface );
		out->numFaces = LittleLong( in->numsurfaces );
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

	for( i = 0; i < 3; i++ ) m_pLoadModel->gridMins[i] = LittleFloat( header->mins[i] );
	for( i = 0; i < 3; i++ ) m_pLoadModel->gridSize[i] = LittleFloat( header->size[i] );
	for( i = 0; i < 4; i++ ) m_pLoadModel->gridBounds[i] = LittleLong( header->bounds[i] );
	m_pLoadModel->numGridPoints = LittleLong( header->numpoints );
	in = (lightGrid_t *)(data + sizeof( dlightgrid_t ));

	out = Mem_Alloc( m_pLoadModel->mempool, m_pLoadModel->numGridPoints * sizeof( lightGrid_t ));
	m_pLoadModel->lightGrid = out;

	for( i = 0; i < m_pLoadModel->numGridPoints; i++, in++, out++ )
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
	r_skyShader[0] = 0;

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
	R_LoadVertexes( mod_base, &header->lumps[LUMP_VERTEXES]);
	R_LoadEdges( mod_base, &header->lumps[LUMP_EDGES]);
	R_LoadSurfEdges( mod_base, &header->lumps[LUMP_SURFEDGES]);
	R_LoadLighting( mod_base, &header->lumps[LUMP_LIGHTING]);
	R_LoadLightgrid( mod_base, &header->lumps[LUMP_LIGHTGRID]);
	R_LoadPlanes( mod_base, &header->lumps[LUMP_PLANES]);
	R_LoadShaders( mod_base, &header->lumps[LUMP_SHADERS]);
	R_LoadTexInfo( mod_base, &header->lumps[LUMP_TEXINFO]);
	R_LoadSurfaces( mod_base, &header->lumps[LUMP_SURFACES]);
	R_LoadMarkSurfaces( mod_base, &header->lumps[LUMP_LEAFFACES]);
	R_LoadLeafs( mod_base, &header->lumps[LUMP_LEAFS]);
	R_LoadNodes( mod_base, &header->lumps[LUMP_NODES]);
	R_LoadVisibility( mod_base, &header->lumps[LUMP_VISIBILITY]);
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
		Mem_Set( mod->name, 0, sizeof( mod->name ));
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
	{
		Mod_Free( &r_models[0] );
	}
	else
	{
		// update progress bar
		Cvar_SetValue( "scr_loading", 50.0f );
		if( ri.UpdateScreen ) ri.UpdateScreen();
	}
	r_worldModel = Mod_ForName( fullname, true );
	R_ModRegisterShaders( r_worldModel );
	r_viewCluster = -1;
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
	R_ModRegisterShaders( mod );
	return mod;
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_EndRegistration

@@@@@@@@@@@@@@@@@@@@@
*/
void R_EndRegistration( const char *skyname )
{
	int	i;
	rmodel_t	*mod;

	if( com.strncmp( skyname, "<skybox>", 8 ))
	{
		// worldspawn:skyname
		R_SetupSky( skyname, 0, vec3_origin );
	}
	else if( com.strlen( r_skyShader ))
	{
		// setup skybox from map
		R_SetupSky( r_skyShader, 0, vec3_origin );
	}
	else
	{
		// default shader
		R_SetupSky( skyname, 0, vec3_origin );
	}

	for( i = 0, mod = r_models; i < r_nummodels; i++, mod++ )
	{
		if( !mod->name[0] ) continue;
		if( mod->registration_sequence != registration_sequence )
			Mod_Free( mod );
	}
	R_ShaderFreeUnused();
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
	Mem_Set( mod, 0, sizeof( *mod ));
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
	Mem_Set( r_fullvis, 0xFF, sizeof( r_fullvis ));

	r_worldModel = NULL;
	r_frameCount = 1;				// no dlight cache
	r_nummodels = 0;
	r_viewCluster = r_oldViewCluster = -1;		// force markleafs

	r_worldEntity = &r_entities[0];		// First entity is the world
	Mem_Set( r_worldEntity, 0, sizeof( ref_entity_t ));
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

	r_worldModel = NULL;
	r_worldEntity = NULL;

	Mod_FreeAll();
	Mem_Set( r_models, 0, sizeof( r_models ));
	r_nummodels = 0;
}
