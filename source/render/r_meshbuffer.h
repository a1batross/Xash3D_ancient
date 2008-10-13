//=======================================================================
//			Copyright XashXT Group 2007 ©
//		     r_meshbuffer.h - configurable mesh buffer
//=======================================================================

#ifndef R_MESHBUFFER_H
#define R_MESHBUFFER_H

static _inline void R_PushElems( uint *elems, int count, int features )
{
	uint	*currentElem;

	// this is a fast path for non-batched geometry, use carefully
	// used on pics, sprites, and studio models
	if( features & MF_NONBATCHED )
	{
		// simply change elemsArray to point at elems
		r_stats.numIndices = count;
		indexArray = currentElem = elems;
	}
	else
	{
		currentElem = indexArray + r_stats.numIndices;
		r_stats.numIndices += count;

		// the following code assumes that R_PushElems is fed with triangles...
		for(; count > 0; count -= 3, elems += 3, currentElem += 3 )
		{
			currentElem[0] = r_stats.numVertices + elems[0];
			currentElem[1] = r_stats.numVertices + elems[1];
			currentElem[2] = r_stats.numVertices + elems[2];
		}
	}
}

static _inline void R_PushTrifanElems( int numverts )
{
	int	count;
	uint	*currentElem;

	currentElem = indexArray + r_stats.numIndices;
	r_stats.numIndices += numverts + numverts + numverts - 6;

	for( count = 2; count < numverts; count++, currentElem += 3 )
	{
		currentElem[0] = r_stats.numVertices;
		currentElem[1] = r_stats.numVertices + count - 1;
		currentElem[2] = r_stats.numVertices + count;
	}
}

_inline static void R_PushMesh( const rb_mesh_t *mesh, int features )
{
	int	numverts;

	if(!( mesh->indexes || ( features & MF_TRIFAN )) || !mesh->points )
		return;

	r_features = features;

	if( features & MF_TRIFAN )
		R_PushTrifanElems( mesh->numVerts );
	else R_PushElems( mesh->indexes, mesh->numIndexes, features );

	numverts = mesh->numVerts;

	if( features & MF_NONBATCHED )
	{
		if( features & MF_DEFORMVS )
		{
			if( mesh->points != inVertsArray )
				Mem_Copy( inVertsArray, mesh->points, numverts * sizeof( vec4_t ));

			if( ( features & MF_NORMALS ) && mesh->normal && ( mesh->normal != inNormalArray ))
				Mem_Copy( inNormalArray, mesh->normal, numverts * sizeof( vec4_t ));
		}
		else
		{
			vertexArray = mesh->points;

			if(( features & MF_NORMALS ) && mesh->normal )
				normalArray = mesh->normal;
		}

		if(( features & MF_STCOORDS ) && mesh->st )
			texCoordArray = mesh->st;

		if( ( features & MF_LMCOORDS ) && mesh->lm[0] )
		{
			lmCoordArray[0] = mesh->lm[0];
			if( features & MF_LMCOORDS1 )
			{
				lmCoordArray[1] = mesh->lm[1];
				if( features & MF_LMCOORDS2 )
				{
					lmCoordArray[2] = mesh->lm[2];
					if( features & MF_LMCOORDS3 )
						lmCoordArray[3] = mesh->lm[3];
				}
			}
		}

		if( ( features & MF_SVECTORS ) && mesh->sVectors )
			sVectorArray = mesh->sVectors;
	}
	else
	{
		if( mesh->points != inVertsArray )
			Mem_Copy( inVertsArray[r_stats.numVertices], mesh->points, numverts * sizeof( vec4_t ));

		if(( features & MF_NORMALS ) && mesh->normal && (mesh->normal != inNormalArray ))
			Mem_Copy( inNormalArray[r_stats.numVertices], mesh->normal, numverts * sizeof( vec4_t ));

		if(( features & MF_STCOORDS ) && mesh->st && (mesh->st != inTexCoordArray ))
			Mem_Copy( inTexCoordArray[r_stats.numVertices], mesh->st, numverts * sizeof( vec2_t ));

		if(( features & MF_LMCOORDS ) && mesh->lm[0] )
		{
			Mem_Copy( inLMCoordsArray[0][r_stats.numVertices], mesh->lm[0], numverts * sizeof( vec2_t ));
			if( features & MF_LMCOORDS1 )
			{
				memcpy( inLMCoordsArray[1][r_stats.numVertices], mesh->lm[1], numverts * sizeof( vec2_t ));
				if( features & MF_LMCOORDS2 )
				{
					Mem_Copy( inLMCoordsArray[2][r_stats.numVertices], mesh->lm[2], numverts * sizeof( vec2_t ));
					if( features & MF_LMCOORDS3 )
						Mem_Copy( inLMCoordsArray[3][r_stats.numVertices], mesh->lm[3], numverts * sizeof( vec2_t ));
				}
			}
		}

		if(( features & MF_SVECTORS ) && mesh->sVectors && (mesh->sVectors != inSVectorsArray ))
			Mem_Copy( inSVectorsArray[r_stats.numVertices], mesh->sVectors, numverts * sizeof( vec4_t ));
	}

	if(( features & MF_COLORS ) && mesh->color[0] )
	{
		Mem_Copy( inColorArray[0][r_stats.numVertices], mesh->color[0], numverts * sizeof( vec4_t ));
		if( features & MF_COLORS1 )
		{
			Mem_Copy( inColorArray[1][r_stats.numVertices], mesh->color[1], numverts * sizeof( vec4_t ));
			if( features & MF_COLORS2 )
			{
				Mem_Copy( inColorArray[2][r_stats.numVertices], mesh->color[2], numverts * sizeof( vec4_t ));
				if( features & MF_COLORS3 )
					Mem_Copy( inColorArray[3][r_stats.numVertices], mesh->color[3], numverts * sizeof( vec4_t ));
			}
		}
	}

	r_stats.numVertices += numverts;
	r_stats.totalVerts += numverts;
}

static _inline bool R_MeshOverflow( const rb_mesh_t *mesh )
{
	return ( r_stats.numVertices + mesh->numVerts > MAX_ARRAY_VERTS || r_stats.numIndices + mesh->numIndexes > MAX_ARRAY_ELEMENTS );
}

static _inline bool R_MeshOverflow2( const rb_mesh_t *mesh1, const rb_mesh_t *mesh2 )
{
	return ( r_stats.numVertices + mesh1->numVerts + mesh2->numVerts > MAX_ARRAY_VERTS ||
		r_stats.numIndices + mesh1->numIndexes + mesh2->numIndexes > MAX_ARRAY_ELEMENTS );
}

static _inline bool R_InvalidMesh( const rb_mesh_t *mesh )
{
	return ( !mesh->numVerts || !mesh->numIndexes || mesh->numVerts > MAX_ARRAY_VERTS || mesh->numIndexes > MAX_ARRAY_ELEMENTS );
}

#endif//R_MESHBUFFER_H