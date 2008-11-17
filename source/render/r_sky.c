//=======================================================================
//			Copyright XashXT Group 2007 ©
//		      r_sky.c - render sky dome or skybox
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "matrixlib.h"

#define MAX_CLIP_VERTS	64

#define ST_SCALE		1.0 / SKY_SIZE
#define ST_MIN		1.0 / 512
#define ST_MAX		511.0 / 512

#define SPHERE_RAD		10.0
#define SPHERE_RAD2		SPHERE_RAD * SPHERE_RAD
#define EYE_RAD		9.0
#define EYE_RAD2		EYE_RAD * EYE_RAD
#define BOX_SIZE		1.0
#define BOX_STEP		BOX_SIZE / SKY_SIZE * 2.0

static vec3_t r_skyClip[6] =
{
{  1,  1,  0},
{  1, -1,  0},
{  0, -1,  1},
{  0,  1,  1},
{  1,  0,  1},
{ -1,  0,  1} 
};

static int r_skyStToVec[6][3] =
{
{  3, -1,  2},
{ -3,  1,  2},
{  1,  3,  2},
{ -1, -3,  2},
{ -2, -1,  3},			// 0 degrees yaw, look straight up
{  2, -1, -3}			// Look straight down
};

static int r_skyVecToSt[6][3] =
{
{ -2,  3,  1},
{  2,  3, -1},
{  1,  3,  2},
{ -1,  3, -2},
{ -2, -1,  3},
{ -2,  1, -3}
};


/*
 =================
 R_FillSkySide
 =================
*/
static void R_FillSkySide( skySide_t *skySide, float skyHeight, vec3_t org, vec3_t row, vec3_t col )
{
	vec3_t		xyz, normal, w;
	vec2_t		st, sphere;
	float		dist;
	int		r, c;
	int		vert = 0;

	skySide->numIndices = SKY_INDICES;
	skySide->numVertices = SKY_VERTICES;

	// find normal
	CrossProduct( col, row, normal );
	VectorNormalize( normal );

	for( r = 0; r <= SKY_SIZE; r++ )
	{
		VectorCopy( org, xyz );

		for( c = 0; c <= SKY_SIZE; c++ )
		{
			// find s and t for box
			st[0] = c * ST_SCALE;
			st[1] = 1.0 - r * ST_SCALE;

			if( !GL_Support( R_CLAMPTOEDGE_EXT ))
			{
				// avoid bilerp seam
				st[0] = bound( ST_MIN, st[0], ST_MAX );
				st[1] = bound( ST_MIN, st[1], ST_MAX );
			}

			// normalize
			VectorNormalize2( xyz, w );

			// find distance along w to sphere
			dist = com.sqrt(EYE_RAD2 * (w[2] * w[2] - 1.0) + SPHERE_RAD2) - EYE_RAD * w[2];

			// use x and y on sphere as s and t
			// minus is here so skies scroll in correct (Q3's) direction
			sphere[0] = ((-(w[0] * dist) * ST_SCALE) + 1) * 0.5;
			sphere[1] = ((-(w[1] * dist) * ST_SCALE) + 1) * 0.5;

			// store vertex
			skySide->vertices[vert].xyz[0] = xyz[0] * skyHeight;
			skySide->vertices[vert].xyz[1] = xyz[1] * skyHeight;
			skySide->vertices[vert].xyz[2] = xyz[2] * skyHeight;
			skySide->vertices[vert].normal[0] = normal[0];
			skySide->vertices[vert].normal[1] = normal[1];
			skySide->vertices[vert].normal[2] = normal[2];
			skySide->vertices[vert].st[0] = st[0];
			skySide->vertices[vert].st[1] = st[1];
			skySide->vertices[vert].sphere[0] = sphere[0];
			skySide->vertices[vert].sphere[1] = sphere[1];
			vert++;

			VectorAdd( xyz, col, xyz );
		}
		VectorAdd( org, row, org );
	}
}

/*
=================
R_MakeSkyVec
=================
*/
static void R_MakeSkyVec( int axis, float x, float y, float z, vec3_t v )
{
	vec3_t	b;
	int	i, j;

	VectorSet( b, x, y, z );

	for( i = 0; i < 3; i++ )
	{
		j = r_skyStToVec[axis][i];
		if( j < 0 ) v[i] = -b[-j - 1];
		else v[i] = b[j - 1];
	}
}

/*
=================
R_AddSkyPolygon
=================
*/
static void R_AddSkyPolygon( int numVerts, vec3_t verts )
{
	sky_t	*sky = r_worldModel->sky;
	int	i, j, axis;
	vec3_t	v, av;
	float	s, t, div;
	float	*vp;

	// decide which face it maps to
	VectorClear( v );
	for( i = 0, vp = verts; i < numVerts; i++, vp += 3 )
		VectorAdd( vp, v, v );

	VectorSet( av, fabs(v[0]), fabs(v[1]), fabs(v[2]));

	if( av[0] > av[1] && av[0] > av[2] )
	{
		if( v[0] < 0 ) axis = 1;
		else axis = 0;
	}
	else if( av[1] > av[2] && av[1] > av[0] )
	{
		if( v[1] < 0 ) axis = 3;
		else axis = 2;
	}
	else
	{
		if( v[2] < 0 ) axis = 5;
		else axis = 4;
	}

	// project new texture coords
	for( i = 0; i < numVerts; i++, verts += 3 )
	{
		j = r_skyVecToSt[axis][2];
		if( j > 0 ) div = verts[j - 1];
		else div = -verts[-j - 1];
		
		if( div < 0.001 ) continue; // don't divide by zero

		div = 1.0 / div;

		j = r_skyVecToSt[axis][0];
		if( j < 0 ) s = -verts[-j - 1] * div;
		else s = verts[j - 1] * div;
		
		j = r_skyVecToSt[axis][1];
		if( j < 0 ) t = -verts[-j - 1] * div;
		else t = verts[j - 1] * div;

		if( s < sky->mins[0][axis] ) sky->mins[0][axis] = s;
		if( t < sky->mins[1][axis] ) sky->mins[1][axis] = t;
		if( s > sky->maxs[0][axis] ) sky->maxs[0][axis] = s;
		if( t > sky->maxs[1][axis] ) sky->maxs[1][axis] = t;
	}
}

/*
 =================
 R_ClipSkyPolygon
 =================
*/
static void R_ClipSkyPolygon( int numVerts, vec3_t verts, int stage )
{
	int		i;
	float		*v;
	int		f, b;
	float		dist;
	bool		frontSide, backSide;
	vec3_t		front[MAX_CLIP_VERTS], back[MAX_CLIP_VERTS];
	float		dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];

	if( numVerts > MAX_CLIP_VERTS-2 )
		Host_Error( "R_ClipSkyPolygon: MAX_CLIP_VERTS limit exceeded\n" );

	if( stage == 6 )
	{
		// fully clipped, so add it
		R_AddSkyPolygon( numVerts, verts );
		return;
	}

	frontSide = backSide = false;

	for( i = 0, v = verts; i < numVerts; i++, v += 3 )
	{
		dists[i] = dist = DotProduct( v, r_skyClip[stage] );

		if( dist > ON_EPSILON )
		{
			frontSide = true;
			sides[i] = SIDE_FRONT;
		}
		else if( dist < -ON_EPSILON )
		{
			backSide = true;
			sides[i] = SIDE_BACK;
		}
		else sides[i] = SIDE_ON;
	}

	if( !frontSide || !backSide )
	{
		// not clipped
		R_ClipSkyPolygon( numVerts, verts, stage+1 );
		return;
	}

	// clip it
	dists[i] = dists[0];
	sides[i] = sides[0];
	VectorCopy( verts, (verts + (i*3)));

	f = b = 0;

	for( i = 0, v = verts; i < numVerts; i++, v += 3 )
	{
		switch( sides[i] )
		{
		case SIDE_FRONT:
			VectorCopy( v, front[f] );
			f++;
			break;
		case SIDE_BACK:
			VectorCopy( v, back[b] );
			b++;
			break;
		case SIDE_ON:
			VectorCopy( v, front[f] );
			VectorCopy( v, back[b] );
			f++;
			b++;
			break;
		}

		if( sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i] )
			continue;

		dist = dists[i] / (dists[i] - dists[i+1]);
		front[f][0] = back[b][0] = v[0] + (v[3] - v[0]) * dist;
		front[f][1] = back[b][1] = v[1] + (v[4] - v[1]) * dist;
		front[f][2] = back[b][2] = v[2] + (v[5] - v[2]) * dist;
		f++;
		b++;
	}

	// continue clipping
	R_ClipSkyPolygon( f, front[0], stage+1 );
	R_ClipSkyPolygon( b, back[0], stage+1 );
}

/*
=================
R_DrawSkyBox
=================
*/
static void R_DrawSkyBox( texture_t *textures[6], bool blended )
{
	sky_t	*sky = r_worldModel->sky;
	skySide_t	*skySide;
	int	i;

	// set the state
	GL_TexEnv( GL_REPLACE );

	if( blended )
	{
		GL_Enable( GL_CULL_FACE );
		GL_Disable( GL_POLYGON_OFFSET_FILL );
		GL_Disable( GL_VERTEX_PROGRAM_ARB );
		GL_Disable( GL_FRAGMENT_PROGRAM_ARB );
		GL_Enable( GL_ALPHA_TEST );
		GL_Enable( GL_BLEND );
		GL_Enable( GL_DEPTH_TEST );

		GL_CullFace( GL_FRONT );
		GL_AlphaFunc( GL_GREATER, 0.0 );
		GL_BlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
		GL_DepthFunc( GL_LEQUAL );
		GL_DepthMask( GL_FALSE );
	}
	else
	{
		GL_Enable( GL_CULL_FACE );
		GL_Disable( GL_POLYGON_OFFSET_FILL );
		GL_Disable( GL_VERTEX_PROGRAM_ARB );
		GL_Disable( GL_FRAGMENT_PROGRAM_ARB );
		GL_Disable( GL_ALPHA_TEST );
		GL_Disable( GL_BLEND );
		GL_Enable( GL_DEPTH_TEST );

		GL_CullFace( GL_FRONT );
		GL_DepthFunc( GL_LEQUAL );
		GL_DepthMask( GL_FALSE );
	}

	pglDepthRange( 1, 1 );
	pglEnable( GL_TEXTURE_2D );

	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglDisableClientState( GL_NORMAL_ARRAY );
	pglDisableClientState( GL_COLOR_ARRAY );

	// draw it
	for( i = 0, skySide = sky->skySides; i < 6; i++, skySide++ )
	{
		if( sky->mins[0][i] >= sky->maxs[0][i] || sky->mins[1][i] >= sky->maxs[1][i] )
			continue;

		r_stats.numVertices += skySide->numVertices;
		r_stats.numIndices += skySide->numIndices;
		r_stats.totalIndices += skySide->numIndices;

		GL_BindTexture( textures[i] );

		if( GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
		{
			RB_UpdateVertexBuffer( sky->vbo[i], skySide->vertices, skySide->numVertices * sizeof(skySideVert_t));

			pglEnableClientState( GL_VERTEX_ARRAY );
			pglVertexPointer( 3, GL_FLOAT, sizeof(skySideVert_t), sky->vbo[i]->pointer );
			pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			pglTexCoordPointer( 2, GL_FLOAT, sizeof(skySideVert_t), sky->vbo[i]->pointer + 12 );

			if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
				pglDrawRangeElementsEXT( GL_TRIANGLES, 0, skySide->numVertices, skySide->numIndices, GL_UNSIGNED_INT, skySide->indices );
			else pglDrawElements( GL_TRIANGLES, skySide->numIndices, GL_UNSIGNED_INT, skySide->indices );
		}
		else
		{
			pglEnableClientState( GL_VERTEX_ARRAY );
			pglVertexPointer( 3, GL_FLOAT, sizeof(skySideVert_t), skySide->vertices->xyz );

			pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			pglTexCoordPointer( 2, GL_FLOAT, sizeof(skySideVert_t), skySide->vertices->st );

			if( GL_Support( R_CUSTOM_VERTEX_ARRAY_EXT )) pglLockArraysEXT( 0, skySide->numVertices );

			if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
				pglDrawRangeElementsEXT( GL_TRIANGLES, 0, skySide->numVertices, skySide->numIndices, GL_UNSIGNED_INT, skySide->indices );
			else pglDrawElements( GL_TRIANGLES, skySide->numIndices, GL_UNSIGNED_INT, skySide->indices );

			if( GL_Support( R_CUSTOM_VERTEX_ARRAY_EXT )) pglUnlockArraysEXT();
		}
	}
	pglDisable( GL_TEXTURE_2D );
}

/*
=================
R_DrawSky
=================
*/
void R_DrawSky( void )
{
	sky_t		*sky = r_worldModel->sky;
	skySide_t		*skySide;
	skySideVert_t	*v;
	uint		*index;
	vec3_t		r_backward, r_left;
	matrix4x4		r_skybox, r_skyMatrix;
	int		s, t, sMin, sMax, tMin, tMax;
	int		i, j;

	// build sky matrix
	VectorNegate( r_forward, r_backward );
	VectorNegate( r_right, r_left );
	Matrix4x4_FromVectors( r_skybox, r_left, r_up, r_backward, r_origin );
	if( sky->rotate ) Matrix4x4_ConcatRotate( r_skybox, r_refdef.time * sky->rotate, sky->axis[0], sky->axis[1], sky->axis[2] );
	Matrix4x4_Transpose( r_skyMatrix, r_skybox ); // finally we need to transpose matrix
	GL_LoadMatrix( r_skyMatrix );

	// build indices in tri-strip order
	for( i = 0, skySide = sky->skySides; i < 6; i++, skySide++ )
	{
		if( sky->mins[0][i] >= sky->maxs[0][i] || sky->mins[1][i] >= sky->maxs[1][i] )
			continue;

		sMin = (int)((sky->mins[0][i] + 1) * 0.5 * (float)(SKY_SIZE));
		sMax = (int)((sky->maxs[0][i] + 1) * 0.5 * (float)(SKY_SIZE)) + 1;
		tMin = (int)((sky->mins[1][i] + 1) * 0.5 * (float)(SKY_SIZE));
		tMax = (int)((sky->maxs[1][i] + 1) * 0.5 * (float)(SKY_SIZE)) + 1;

		sMin = bound(0, sMin, SKY_SIZE);
		sMax = bound(0, sMax, SKY_SIZE);
		tMin = bound(0, tMin, SKY_SIZE);
		tMax = bound(0, tMax, SKY_SIZE);

		index = skySide->indices;
		for( t = tMin; t < tMax; t++ )
		{
			for( s = sMin; s < sMax; s++ )
			{
				index[0] = t * (SKY_SIZE+1) + s;
				index[1] = index[4] = index[0] + (SKY_SIZE+1);
				index[2] = index[3] = index[0] + 1;
				index[5] = index[1] + 1;
				index += 6;
			}
		}
		skySide->numIndices = (sMax-sMin) * (tMax-tMin) * 6;
	}

	// draw the far box
	if( sky->shader->skyParms.farBox[0] )
		R_DrawSkyBox( sky->shader->skyParms.farBox, false );

	// draw the cloud layers
	if( sky->shader->numStages )
	{
		pglDepthRange( 1, 1 );

		for( i = 0, skySide = sky->skySides; i < 6; i++, skySide++ )
		{
			if( sky->mins[0][i] >= sky->maxs[0][i] || sky->mins[1][i] >= sky->maxs[1][i] )
				continue;

			// draw it
			RB_CheckMeshOverflow( skySide->numIndices, skySide->numVertices );

			for( j = 0; j < skySide->numIndices; j += 3 )
			{
				ref.indexArray[ref.numIndex++] = ref.numVertex + skySide->indices[j+0];
				ref.indexArray[ref.numIndex++] = ref.numVertex + skySide->indices[j+1];
				ref.indexArray[ref.numIndex++] = ref.numVertex + skySide->indices[j+2];
			}

			for( j = 0, v = skySide->vertices; j < skySide->numVertices; j++, v++ )
			{
				ref.vertexArray[ref.numVertex][0] = v->xyz[0];
				ref.vertexArray[ref.numVertex][1] = v->xyz[1];
				ref.vertexArray[ref.numVertex][2] = v->xyz[2];
				ref.normalArray[ref.numVertex][0] = v->normal[0];
				ref.normalArray[ref.numVertex][1] = v->normal[1];
				ref.normalArray[ref.numVertex][2] = v->normal[2];
				ref.inTexCoordArray[ref.numVertex][0] = v->sphere[0];
				ref.inTexCoordArray[ref.numVertex][1] = v->sphere[1];
				ref.colorArray[ref.numVertex][0] = 1.0f;
				ref.colorArray[ref.numVertex][1] = 1.0f;
				ref.colorArray[ref.numVertex][2] = 1.0f;
				ref.colorArray[ref.numVertex][3] = 1.0f;
				ref.numVertex++;
			}
		}
		// flush
		RB_RenderMesh();
	}

	// draw the near box
	if( sky->shader->skyParms.nearBox[0] )
		R_DrawSkyBox( sky->shader->skyParms.nearBox, true );

	pglDepthRange( 0, 1 );
	GL_LoadMatrix( r_worldMatrix );
}

/*
=================
R_ClearSky
=================
*/
void R_ClearSky( void )
{
	sky_t	*sky = r_worldModel->sky;
	int	i;

	for( i = 0; i < 6; i++ )
	{
		sky->mins[0][i] = sky->mins[1][i] = 999999;
		sky->maxs[0][i] = sky->maxs[1][i] = -999999;
	}
}

/*
=================
R_ClipSkySurface
=================
*/
void R_ClipSkySurface( surface_t *surf )
{
	surfPoly_t	*p;
	surfPolyVert_t	*v;
	vec3_t		verts[MAX_CLIP_VERTS];
	int		i;

	// calculate vertex values for sky box
	for( p = surf->poly; p; p = p->next )
	{
		for( i = 0, v = p->vertices; i < p->numVertices; i++, v++ )
			VectorSubtract( v->xyz, r_refdef.vieworg, verts[i] );
		R_ClipSkyPolygon( p->numVertices, verts[0], 0 );
	}
}

/*
=================
R_AddSkyToList
=================
*/
void R_AddSkyToList( void )
{
	sky_t	*sky = r_worldModel->sky;
	int	i;

	// check for no sky at all
	for( i = 0; i < 6; i++ )
	{
		if( sky->mins[0][i] < sky->maxs[0][i] && sky->mins[1][i] < sky->maxs[1][i] )
			break;
	}
		
	if( i == 6 ) return; // nothing visible

	// HACK: force full sky to draw when rotating
	if( sky->rotate )
	{
		for( i = 0; i < 6; i++ )
		{
			sky->mins[0][i] = -1;
			sky->mins[1][i] = -1;
			sky->maxs[0][i] = 1;
			sky->maxs[1][i] = 1;
		}
	}

	// add it
	R_AddMeshToList( MESH_SKY, NULL, sky->shader, r_worldEntity, 0 );
}

/*
=================
R_SetupSky
=================
*/
void R_SetupSky( const char *name, float rotate, const vec3_t axis )
{
	sky_t		*sky = NULL;
	skySide_t		*skySide;
	skySideVert_t	*v;
	vec3_t		org, row, col;
	void		*map;
	int		i, j;

	if( !com.strlen( name )) return; // invalid name
	r_worldModel->sky = Mem_Realloc( r_temppool, r_worldModel->sky, sizeof( sky_t ));
	sky = r_worldModel->sky;
	sky->shader = R_FindShader( name, SHADER_SKY, 0 );

	sky->rotate = rotate;
	if(!VectorIsNull( axis ))
		VectorCopy( axis, sky->axis );
	else VectorSet( sky->axis, 0, 0, 1 );

	// fill sky sides
	for( i = 0, skySide = sky->skySides; i < 6; i++, skySide++ )
	{
		R_MakeSkyVec( i, -BOX_SIZE, -BOX_SIZE, BOX_SIZE, org );
		R_MakeSkyVec( i, 0, BOX_STEP, 0, row );
		R_MakeSkyVec( i, BOX_STEP, 0, 0, col );

		R_FillSkySide( skySide, sky->shader->skyParms.cloudHeight, org, row, col );

		// If VBO is enabled, put the sky box arrays in a static buffer
		if( GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
		{
			sky->vbo[i] = RB_AllocVertexBuffer( skySide->numVertices * sizeof(skySideVert_t), GL_STATIC_DRAW_ARB );

			// fill it in with vertices and texture coordinates
			map = pglMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
			if( map )
			{
				for( j = 0, v = skySide->vertices; j < skySide->numVertices; j++, v++ )
				{
					((float *)map)[j*5+0] = v->xyz[0];
					((float *)map)[j*5+1] = v->xyz[1];
					((float *)map)[j*5+2] = v->xyz[2];
					((float *)map)[j*5+3] = v->st[0];
					((float *)map)[j*5+4] = v->st[1];
				}
				pglUnmapBufferARB( GL_ARRAY_BUFFER_ARB );
			}
		}
	}
}
