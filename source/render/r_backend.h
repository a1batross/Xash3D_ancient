/*
Copyright (C) 2002-2007 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#ifndef __R_BACKEND_H__
#define __R_BACKEND_H__

#define MAX_ARRAY_VERTS			4096
#define MAX_ARRAY_ELEMENTS		MAX_ARRAY_VERTS*6
#define MAX_ARRAY_TRIANGLES		MAX_ARRAY_ELEMENTS/3
#define MAX_ARRAY_NEIGHBORS		MAX_ARRAY_TRIANGLES*3

enum
{
	VBO_VERTS,
	VBO_NORMALS,
	VBO_COLORS,
//	VBO_INDEXES,
	VBO_TC0,

	VBO_ENDMARKER
};

#define MAX_VERTEX_BUFFER_OBJECTS   VBO_ENDMARKER+MAX_TEXTURE_UNITS-1

extern ALIGN( 16 ) vec4_t inVertsArray[MAX_ARRAY_VERTS];
extern ALIGN( 16 ) vec4_t inNormalsArray[MAX_ARRAY_VERTS];
extern vec4_t inSVectorsArray[MAX_ARRAY_VERTS];
extern elem_t inElemsArray[MAX_ARRAY_ELEMENTS];
extern vec2_t inCoordsArray[MAX_ARRAY_VERTS];
extern vec2_t inLightmapCoordsArray[LM_STYLES][MAX_ARRAY_VERTS];
extern rgba_t inColorsArray[LM_STYLES][MAX_ARRAY_VERTS];

extern elem_t *elemsArray;
extern vec4_t *vertsArray;
extern vec4_t *normalsArray;
extern vec4_t *sVectorsArray;
extern vec2_t *coordsArray;
extern vec2_t *lightmapCoordsArray[LM_STYLES];
extern rgba_t colorArray[MAX_ARRAY_VERTS];

extern int r_numVertexBufferObjects;
extern GLuint r_vertexBufferObjects[MAX_VERTEX_BUFFER_OBJECTS];

extern int r_features;

//===================================================================

typedef struct
{
	unsigned int numVerts, numElems, numColors;
	unsigned int c_totalVerts, c_totalTris, c_totalFlushes, c_totalKeptLocks;
} rbackacc_t;

extern rbackacc_t r_backacc;

//===================================================================

void R_BackendInit( void );
void R_BackendShutdown( void );
void R_BackendStartFrame( void );
void R_BackendEndFrame( void );
void R_BackendResetCounters( void );

void R_BackendBeginTriangleOutlines( void );
void R_BackendEndTriangleOutlines( void );

void R_BackendCleanUpTextureUnits( void );

void R_BackendSetPassMask( int mask );
void R_BackendResetPassMask( void );

void R_LockArrays( int numverts );
void R_UnlockArrays( void );
void R_UnlockArrays( void );
void R_FlushArrays( void );
void R_FlushArraysMtex( void );
void R_ClearArrays( void );

static _inline void R_PushElems( elem_t *elems, int count, int features )
{
	elem_t	*currentElem;

	// this is a fast path for non-batched geometry, use carefully
	// used on pics, sprites, .dpm, .md3 and .md2 models
	if( features & MF_NONBATCHED )
	{
		// simply change elemsArray to point at elems
		r_backacc.numElems = count;
		elemsArray = currentElem = elems;
	}
	else
	{
		currentElem = elemsArray + r_backacc.numElems;
		r_backacc.numElems += count;

		// the following code assumes that R_PushElems is fed with triangles...
		for(; count > 0; count -= 3, elems += 3, currentElem += 3 )
		{
			currentElem[0] = r_backacc.numVerts + elems[0];
			currentElem[1] = r_backacc.numVerts + elems[1];
			currentElem[2] = r_backacc.numVerts + elems[2];
		}
	}
}

static _inline void R_PushTrifanElems( int numverts )
{
	int count;
	elem_t *currentElem;

	currentElem = elemsArray + r_backacc.numElems;
	r_backacc.numElems += numverts + numverts + numverts - 6;

	for( count = 2; count < numverts; count++, currentElem += 3 )
	{
		currentElem[0] = r_backacc.numVerts;
		currentElem[1] = r_backacc.numVerts + count - 1;
		currentElem[2] = r_backacc.numVerts + count;
	}
}

static _inline void R_PushMesh( const mesh_t *mesh, int features )
{
	int numverts;

	if( !( mesh->elems || ( features & MF_TRIFAN ) ) || !mesh->xyzArray )
		return;

	r_features = features;

	if( features & MF_TRIFAN )
		R_PushTrifanElems( mesh->numVertexes );
	else
		R_PushElems( mesh->elems, mesh->numElems, features );

	numverts = mesh->numVertexes;

	if( features & MF_NONBATCHED )
	{
		if( features & MF_DEFORMVS )
		{
			if( mesh->xyzArray != inVertsArray )
				memcpy( inVertsArray, mesh->xyzArray, numverts * sizeof( vec4_t ) );

			if( ( features & MF_NORMALS ) && mesh->normalsArray && ( mesh->normalsArray != inNormalsArray ) )
				memcpy( inNormalsArray, mesh->normalsArray, numverts * sizeof( vec4_t ) );
		}
		else
		{
			vertsArray = mesh->xyzArray;

			if( ( features & MF_NORMALS ) && mesh->normalsArray )
				normalsArray = mesh->normalsArray;
		}

		if( ( features & MF_STCOORDS ) && mesh->stArray )
			coordsArray = mesh->stArray;

		if( ( features & MF_LMCOORDS ) && mesh->lmstArray[0] )
		{
			lightmapCoordsArray[0] = mesh->lmstArray[0];
			if( features & MF_LMCOORDS1 )
			{
				lightmapCoordsArray[1] = mesh->lmstArray[1];
				if( features & MF_LMCOORDS2 )
				{
					lightmapCoordsArray[2] = mesh->lmstArray[2];
					if( features & MF_LMCOORDS3 )
						lightmapCoordsArray[3] = mesh->lmstArray[3];
				}
			}
		}

		if( ( features & MF_SVECTORS ) && mesh->sVectorsArray )
			sVectorsArray = mesh->sVectorsArray;
	}
	else
	{
		if( mesh->xyzArray != inVertsArray )
			memcpy( inVertsArray[r_backacc.numVerts], mesh->xyzArray, numverts * sizeof( vec4_t ) );

		if( ( features & MF_NORMALS ) && mesh->normalsArray && (mesh->normalsArray != inNormalsArray ) )
			memcpy( inNormalsArray[r_backacc.numVerts], mesh->normalsArray, numverts * sizeof( vec4_t ) );

		if( ( features & MF_STCOORDS ) && mesh->stArray && (mesh->stArray != inCoordsArray ) )
			memcpy( inCoordsArray[r_backacc.numVerts], mesh->stArray, numverts * sizeof( vec2_t ) );

		if( ( features & MF_LMCOORDS ) && mesh->lmstArray[0] )
		{
			memcpy( inLightmapCoordsArray[0][r_backacc.numVerts], mesh->lmstArray[0], numverts * sizeof( vec2_t ) );
			if( features & MF_LMCOORDS1 )
			{
				memcpy( inLightmapCoordsArray[1][r_backacc.numVerts], mesh->lmstArray[1], numverts * sizeof( vec2_t ) );
				if( features & MF_LMCOORDS2 )
				{
					memcpy( inLightmapCoordsArray[2][r_backacc.numVerts], mesh->lmstArray[2], numverts * sizeof( vec2_t ) );
					if( features & MF_LMCOORDS3 )
						memcpy( inLightmapCoordsArray[3][r_backacc.numVerts], mesh->lmstArray[3], numverts * sizeof( vec2_t ) );
				}
			}
		}

		if( ( features & MF_SVECTORS ) && mesh->sVectorsArray && (mesh->sVectorsArray != inSVectorsArray ) )
			memcpy( inSVectorsArray[r_backacc.numVerts], mesh->sVectorsArray, numverts * sizeof( vec4_t ) );
	}

	if( ( features & MF_COLORS ) && mesh->colorsArray[0] )
	{
		memcpy( inColorsArray[0][r_backacc.numVerts], mesh->colorsArray[0], numverts * sizeof( rgba_t ) );
		if( features & MF_COLORS1 )
		{
			memcpy( inColorsArray[1][r_backacc.numVerts], mesh->colorsArray[1], numverts * sizeof( rgba_t ) );
			if( features & MF_COLORS2 )
			{
				memcpy( inColorsArray[2][r_backacc.numVerts], mesh->colorsArray[2], numverts * sizeof( rgba_t ) );
				if( features & MF_COLORS3 )
					memcpy( inColorsArray[3][r_backacc.numVerts], mesh->colorsArray[3], numverts * sizeof( rgba_t ) );
			}
		}
	}

	r_backacc.numVerts += numverts;
	r_backacc.c_totalVerts += numverts;
}

static _inline bool R_MeshOverflow( const mesh_t *mesh )
{
	return ( r_backacc.numVerts + mesh->numVertexes > MAX_ARRAY_VERTS ||
		r_backacc.numElems + mesh->numElems > MAX_ARRAY_ELEMENTS );
}

static _inline bool R_MeshOverflow2( const mesh_t *mesh1, const mesh_t *mesh2 )
{
	return ( r_backacc.numVerts + mesh1->numVertexes + mesh2->numVertexes > MAX_ARRAY_VERTS ||
		r_backacc.numElems + mesh1->numElems + mesh2->numElems > MAX_ARRAY_ELEMENTS );
}

static _inline bool R_InvalidMesh( const mesh_t *mesh )
{
	return ( !mesh->numVertexes || !mesh->numElems ||
		mesh->numVertexes > MAX_ARRAY_VERTS || mesh->numElems > MAX_ARRAY_ELEMENTS );
}

void R_RenderMeshBuffer( const meshbuffer_t *mb );

#endif /*__R_BACKEND_H__*/
