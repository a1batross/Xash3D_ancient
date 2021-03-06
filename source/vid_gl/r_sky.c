/*
Copyright (C) 1999 Stephen C. Taylor
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

// r_sky.c

#include "r_local.h"
#include "mathlib.h"
#include "matrix_lib.h"

#define MAX_CLIP_VERTS	64
#define SIDE_SIZE   	9
#define POINTS_LEN		( SIDE_SIZE * SIDE_SIZE )
#define ELEM_LEN		(( SIDE_SIZE-1 ) * ( SIDE_SIZE-1 ) * 6 )

#define SPHERE_RAD		10.0f
#define EYE_RAD		9.0f

#define SCALE_S		4.0f  // arbitrary (?) texture scaling factors
#define SCALE_T		4.0f
#define BOX_SIZE    	1.0f
#define BOX_STEP    	BOX_SIZE / ( SIDE_SIZE-1 ) * 2.0f

elem_t			r_skydome_elems[6][ELEM_LEN];
meshbuffer_t		r_skydome_mbuffer;

static mfog_t		*r_skyfog;
static msurface_t		*r_warpface;
static bool		r_warpfacevis;

static void Gen_BoxSide( skydome_t *skydome, int side, vec3_t orig, vec3_t drow, vec3_t dcol, float skyheight );
static void MakeSkyVec( float x, float y, float z, int axis, vec3_t v );
static void Gen_Box( skydome_t *skydome, float skyheight );

/*
==============
R_CreateSkydome
==============
*/
skydome_t *R_CreateSkydome( byte *mempool, float skyheight, ref_shader_t **farboxShaders, ref_shader_t **nearboxShaders )
{
	int	i, size;
	mesh_t	*mesh;
	skydome_t	*skydome;
	byte	*buffer;

	size = sizeof( skydome_t ) + sizeof( mesh_t ) * 6 + sizeof( vec4_t ) * POINTS_LEN * 6 +
		sizeof( vec4_t ) * POINTS_LEN * 6 + sizeof( vec2_t ) * POINTS_LEN * 12;
	buffer = Mem_Alloc( mempool, size );

	skydome = ( skydome_t * )buffer;
	Mem_Copy( skydome->farboxShaders, farboxShaders, sizeof( ref_shader_t* ) * 6 );
	Mem_Copy( skydome->nearboxShaders, nearboxShaders, sizeof( ref_shader_t* ) * 6 );
	buffer += sizeof( skydome_t );

	skydome->skyHeight = skyheight;
	skydome->meshes = ( mesh_t* )buffer;
	buffer += sizeof( mesh_t ) * 6;

	for( i = 0, mesh = skydome->meshes; i < 6; i++, mesh++ )
	{
		mesh->numVertexes = POINTS_LEN;
		mesh->xyzArray = ( vec4_t * )buffer; buffer += sizeof( vec4_t ) * POINTS_LEN;
		mesh->normalsArray = ( vec4_t * )buffer; buffer += sizeof( vec4_t ) * POINTS_LEN;
		skydome->sphereStCoords[i] = ( vec2_t * )buffer; buffer += sizeof( vec2_t ) * POINTS_LEN;
		skydome->linearStCoords[i] = ( vec2_t * )buffer; buffer += sizeof( vec2_t ) * POINTS_LEN;

		mesh->numElems = ELEM_LEN;
		mesh->elems = r_skydome_elems[i];
	}

	Gen_Box( skydome, skyheight );

	return skydome;
}

/*
==============
R_FreeSkydome
==============
*/
void R_FreeSkydome( skydome_t *skydome )
{
	if( skydome ) Mem_Free( skydome );
}

/*
==============
Gen_Box
==============
*/
static void Gen_Box( skydome_t *skydome, float skyheight )
{
	int	axis;
	vec3_t	orig, drow, dcol;

	for( axis = 0; axis < 6; axis++ )
	{
		MakeSkyVec( -BOX_SIZE, -BOX_SIZE, BOX_SIZE, axis, orig );
		MakeSkyVec( 0, BOX_STEP, 0, axis, drow );
		MakeSkyVec( BOX_STEP, 0, 0, axis, dcol );
		Gen_BoxSide( skydome, axis, orig, drow, dcol, skyheight );
	}
}

/*
================
Gen_BoxSide

I don't know exactly what Q3A does for skybox texturing, but
this is at least fairly close.  We tile the texture onto the
inside of a large sphere, and put the camera near the top of
the sphere. We place the box around the camera, and cast rays
through the box verts to the sphere to find the texture coordinates.
================
*/
static void Gen_BoxSide( skydome_t *skydome, int side, vec3_t orig, vec3_t drow, vec3_t dcol, float skyheight )
{
	vec3_t	pos, w, row, norm;
	float	*v, *n, *st, *st2;
	float	t, d, d2, b, b2, q[2], s;
	int	r, c;

	s = 1.0 / ( SIDE_SIZE-1 );
	d = EYE_RAD; // sphere center to camera distance
	d2 = d * d;
	b = SPHERE_RAD; // sphere radius
	b2 = b * b;
	q[0] = 1.0 / ( 2.0 * SCALE_S );
	q[1] = 1.0 / ( 2.0 * SCALE_T );

	v = skydome->meshes[side].xyzArray[0];
	n = skydome->meshes[side].normalsArray[0];
	st = skydome->sphereStCoords[side][0];
	st2 = skydome->linearStCoords[side][0];

	VectorCopy( orig, row );
	VectorClear( norm );

	for( r = 0; r < SIDE_SIZE; r++ )
	{
		VectorCopy( row, pos );
		for( c = 0; c < SIDE_SIZE; c++ )
		{
			// pos points from eye to vertex on box
			VectorScale( pos, skyheight, v );
			VectorCopy( pos, w );

			// Normalize pos -> w
			VectorNormalize( w );

			// Find distance along w to sphere
			t = sqrt( d2 * ( w[2] * w[2] - 1.0 ) + b2 ) - d * w[2];
			w[0] *= t;
			w[1] *= t;

			// use x and y on sphere as s and t
			// minus is here so skies scoll in correct (Q3A's) direction
			st[0] = -w[0] * q[0];
			st[1] = -w[1] * q[1];

			// avoid bilerp seam
			st[0] = ( bound( -1, st[0], 1 ) + 1.0 ) * 0.5;
			st[1] = ( bound( -1, st[1], 1 ) + 1.0 ) * 0.5;

			st2[0] = c * s;
			st2[1] = 1.0 - r * s;

			VectorAdd( pos, dcol, pos );
			VectorCopy( norm, n );

			v += 4;
			n += 4;
			st += 2;
			st2 += 2;
		}
		VectorAdd( row, drow, row );
	}
}

/*
==============
R_DrawSkySide
==============
*/
static void R_DrawSkySide( skydome_t *skydome, int side, ref_shader_t *shader, int features )
{
	meshbuffer_t *mbuffer = &r_skydome_mbuffer;

	if( RI.skyMins[0][side] >= RI.skyMaxs[0][side] || RI.skyMins[1][side] >= RI.skyMaxs[1][side] )
		return;

	mbuffer->shaderkey = shader->sortkey;
	mbuffer->dlightbits = 0;
	mbuffer->sortkey = MB_FOG2NUM( r_skyfog );

	skydome->meshes[side].stArray = skydome->linearStCoords[side];
	R_PushMesh( &skydome->meshes[side], features );
	R_RenderMeshBuffer( mbuffer );
}

/*
==============
R_DrawSkyBox
==============
*/
static void R_DrawSkyBox( skydome_t *skydome, ref_shader_t **shaders )
{
	int	i, features;
	const int	skytexorder[6] = { SKYBOX_RIGHT, SKYBOX_FRONT, SKYBOX_LEFT, SKYBOX_BACK, SKYBOX_TOP, SKYBOX_BOTTOM };

	features = shaders[0]->features;
	if( r_shownormals->integer )
		features |= MF_NORMALS;

	for( i = 0; i < 6; i++ )
		R_DrawSkySide( skydome, i, shaders[skytexorder[i]], features );
}

/*
==============
R_DrawBlackBottom

Draw dummy skybox side to prevent the HOM effect
==============
*/
static void R_DrawBlackBottom( skydome_t *skydome )
{
	int	features;

	features = tr.defaultShader->features;
	if( r_shownormals->integer )
		features |= MF_NORMALS;
	R_DrawSkySide( skydome, 5, tr.defaultShader, features );
}

/*
==============
R_DrawSky
==============
*/
void R_DrawSky( ref_shader_t *shader )
{
	int		i;
	vec3_t		mins, maxs;
	matrix4x4		m, oldm;
	elem_t		*elem;
	skydome_t		*skydome;
	meshbuffer_t	*mbuffer = &r_skydome_mbuffer;
	int		u, v, umin, umax, vmin, vmax;

	if( !shader ) return;
	skydome = shader->skyParms ? shader->skyParms : NULL;
	if( !skydome ) return;

	ClearBounds( mins, maxs );
	for( i = 0; i < 6; i++ )
	{
		if( RI.skyMins[0][i] >= RI.skyMaxs[0][i] || RI.skyMins[1][i] >= RI.skyMaxs[1][i] )
			continue;

		umin = (int)(( RI.skyMins[0][i] + 1.0f ) * 0.5f * (float)( SIDE_SIZE-1 ));
		umax = (int)(( RI.skyMaxs[0][i] + 1.0f ) * 0.5f * (float)( SIDE_SIZE-1 )) + 1;
		vmin = (int)(( RI.skyMins[1][i] + 1.0f ) * 0.5f * (float)( SIDE_SIZE-1 ));
		vmax = (int)(( RI.skyMaxs[1][i] + 1.0f ) * 0.5f * (float)( SIDE_SIZE-1 )) + 1;

		umin = bound( 0, umin, SIDE_SIZE-1 );
		umax = bound( 0, umax, SIDE_SIZE-1 );
		vmin = bound( 0, vmin, SIDE_SIZE-1 );
		vmax = bound( 0, vmax, SIDE_SIZE-1 );

		// box elems in tristrip order
		elem = skydome->meshes[i].elems;
		for( v = vmin; v < vmax; v++ )
		{
			for( u = umin; u < umax; u++ )
			{
				elem[0] = v * SIDE_SIZE + u;
				elem[1] = elem[4] = elem[0] + SIDE_SIZE;
				elem[2] = elem[3] = elem[0] + 1;
				elem[5] = elem[1] + 1;
				elem += 6;
			}
		}

		AddPointToBounds( skydome->meshes[i].xyzArray[vmin*SIDE_SIZE+umin], mins, maxs );
		AddPointToBounds( skydome->meshes[i].xyzArray[vmax*SIDE_SIZE+umax], mins, maxs );

		skydome->meshes[i].numElems = ( vmax-vmin )*( umax-umin ) * 6;
	}

	VectorAdd( mins, RI.viewOrigin, mins );
	VectorAdd( maxs, RI.viewOrigin, maxs );

	if( RI.refdef.flags & RDF_SKYPORTALINVIEW )
	{
		R_DrawSkyPortal( &RI.refdef.skyportal, mins, maxs );
		return;
	}

	// center skydome on camera to give the illusion of a larger space
	Matrix4x4_Copy( oldm, RI.modelviewMatrix );
	Matrix4x4_Copy( RI.modelviewMatrix, RI.worldviewMatrix );
	Matrix4x4_Copy( m, RI.worldviewMatrix );

	if( shader->skySpeed )
	{
		float	angle = shader->skySpeed * RI.refdef.time;
		Matrix4x4_ConcatRotate( m, angle, shader->skyAxis[0], shader->skyAxis[1], shader->skyAxis[2] );
	}
	Matrix4x4_SetOrigin( m, 0, 0, 0 );
	m[3][3] = 1.0f;
	GL_LoadMatrix( m );

	gldepthmin = 1;
	gldepthmax = 1;
	pglDepthRange( gldepthmin, gldepthmax );

	if( RI.params & RP_CLIPPLANE )
		pglDisable( GL_CLIP_PLANE0 );

	// it can happen that sky surfaces have no fog hull specified
	// yet there's a global fog hull (see wvwq3dm7)
	if( !r_skyfog ) r_skyfog = r_worldbrushmodel->globalfog;

	if( skydome->farboxShaders[0] )
		R_DrawSkyBox( skydome, skydome->farboxShaders );
	else R_DrawBlackBottom( skydome );

	if( shader->num_stages )
	{
		bool flush = false;
		int features = shader->features;

		if( r_shownormals->integer )
			features |= MF_NORMALS;

		for( i = 0; i < 6; i++ )
		{
			if( RI.skyMins[0][i] >= RI.skyMaxs[0][i] || RI.skyMins[1][i] >= RI.skyMaxs[1][i] )
				continue;

			flush = true;
			mbuffer->shaderkey = shader->sortkey;
			mbuffer->dlightbits = 0;
			mbuffer->sortkey = MB_FOG2NUM( r_skyfog );

			skydome->meshes[i].stArray = skydome->sphereStCoords[i];
			R_PushMesh( &skydome->meshes[i], features );
		}
		if( flush ) R_RenderMeshBuffer( mbuffer );
	}

	if( skydome->nearboxShaders[0] )
		R_DrawSkyBox( skydome, skydome->nearboxShaders );

	if( RI.params & RP_CLIPPLANE )
		pglEnable( GL_CLIP_PLANE0 );

	Matrix4x4_Copy( RI.modelviewMatrix, oldm );
	GL_LoadMatrix( RI.worldviewMatrix );

	gldepthmin = 0;
	gldepthmax = 1;
	pglDepthRange( gldepthmin, gldepthmax );

	r_skyfog = NULL;
}

//===================================================================

vec3_t skyclip[6] =
{
{  1,  1,  0 },
{  1, -1,  0 },
{  0, -1,  1 },
{  0,  1,  1 },
{  1,  0,  1 },
{ -1,  0,  1 }
};

// 1 = s, 2 = t, 3 = 2048
int st_to_vec[6][3] =
{
{  3, -1,  2 },
{ -3,  1,  2 },
{  1,  3,  2 },
{ -1, -3,  2 },
{ -2, -1,  3 },  // 0 degrees yaw, look straight up
{  2, -1, -3 }   // look straight down
};

// s = [0]/[2], t = [1]/[2]
int vec_to_st[6][3] =
{
{ -2,  3,  1 },
{  2,  3, -1 },
{  1,  3,  2 },
{ -1,  3, -2 },
{ -2, -1,  3 },
{ -2,  1, -3 }
};

/*
==============
DrawSkyPolygon
==============
*/
void DrawSkyPolygon( int nump, vec3_t vecs )
{
	int	i, j;
	vec3_t	v, av;
	float	s, t, dv;
	int	axis;
	float	*vp;

	// decide which face it maps to
	VectorClear( v );

	for( i = 0, vp = vecs; i < nump; i++, vp += 3 )
		VectorAdd( vp, v, v );

	av[0] = fabs( v[0] );
	av[1] = fabs( v[1] );
	av[2] = fabs( v[2] );

	if(( av[0] > av[1] ) && ( av[0] > av[2] ))
		axis = ( v[0] < 0 ) ? 1 : 0;
	else if(( av[1] > av[2] ) && ( av[1] > av[0] ))
		axis = ( v[1] < 0 ) ? 3 : 2;
	else axis = ( v[2] < 0 ) ? 5 : 4;

	if( !r_skyfog )
		r_skyfog = r_warpface->fog;
	r_warpfacevis = true;

	// project new texture coords
	for( i = 0; i < nump; i++, vecs += 3 )
	{
		j = vec_to_st[axis][2];
		dv = ( j > 0 ) ? vecs[j - 1] : -vecs[-j - 1];

		if( dv < 0.001f )
			continue; // don't divide by zero

		dv = 1.0f / dv;

		j = vec_to_st[axis][0];
		s = ( j < 0 ) ? -vecs[-j -1] * dv : vecs[j-1] * dv;

		j = vec_to_st[axis][1];
		t = ( j < 0 ) ? -vecs[-j -1] * dv : vecs[j-1] * dv;

		if( s < RI.skyMins[0][axis] ) RI.skyMins[0][axis] = s;
		if( t < RI.skyMins[1][axis] ) RI.skyMins[1][axis] = t;
		if( s > RI.skyMaxs[0][axis] ) RI.skyMaxs[0][axis] = s;
		if( t > RI.skyMaxs[1][axis] ) RI.skyMaxs[1][axis] = t;
	}
}

/*
==============
ClipSkyPolygon
==============
*/
void ClipSkyPolygon( int nump, vec3_t vecs, int stage )
{
	float	*norm;
	float	*v;
	bool	front, back;
	float	d, e;
	float	dists[MAX_CLIP_VERTS + 1];
	int	sides[MAX_CLIP_VERTS + 1];
	vec3_t	newv[2][MAX_CLIP_VERTS + 1];
	int	newc[2];
	int	i, j;

	if( nump > MAX_CLIP_VERTS )
		Host_Error( "ClipSkyPolygon: MAX_CLIP_VERTS\n" );

loc1:
	if( stage == 6 )
	{	
		// fully clipped, so draw it
		DrawSkyPolygon( nump, vecs );
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for( i = 0, v = vecs; i < nump; i++, v += 3 )
	{
		d = DotProduct( v, norm );
		if( d > ON_EPSILON )
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if( d < -ON_EPSILON )
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
		{
			sides[i] = SIDE_ON;
		}
		dists[i] = d;
	}

	if( !front || !back )
	{	
		// not clipped
		stage++;
		goto loc1;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy( vecs, ( vecs + ( i * 3 )));
	newc[0] = newc[1] = 0;

	for( i = 0, v = vecs; i < nump; i++, v += 3 )
	{
		switch( sides[i] )
		{
		case SIDE_FRONT:
			VectorCopy( v, newv[0][newc[0]] );
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy( v, newv[1][newc[1]] );
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy( v, newv[0][newc[0]] );
			newc[0]++;
			VectorCopy( v, newv[1][newc[1]] );
			newc[1]++;
			break;
		}

		if( sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i] )
			continue;

		d = dists[i] / ( dists[i] - dists[i+1] );
		for( j = 0; j < 3; j++ )
		{
			e = v[j] + d * ( v[j+3] - v[j] );
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon( newc[0], newv[0][0], stage + 1 );
	ClipSkyPolygon( newc[1], newv[1][0], stage + 1 );
}

/*
=================
R_AddSkySurface
=================
*/
bool R_AddSkySurface( msurface_t *fa )
{
	int	i;
	vec4_t	*vert;
	elem_t	*elem;
	mesh_t	*mesh;
	vec3_t	verts[4];

	// calculate vertex values for sky box
	r_warpface = fa;
	r_warpfacevis = false;

	if( tr.currentSkyShader && tr.currentSkyShader->skySpeed )
	{
		// HACK: force full sky to draw when rotating
		for( i = 0; i < 6; i++ )
		{
			RI.skyMins[0][i] = RI.skyMins[1][i] = -1;
			RI.skyMaxs[0][i] = RI.skyMaxs[1][i] = 1;
		}
	}

	mesh = fa->mesh;
	elem = mesh->elems;
	vert = mesh->xyzArray;
	for( i = 0; i < mesh->numElems; i += 3, elem += 3 )
	{
		VectorSubtract( vert[elem[0]], RI.viewOrigin, verts[0] );
		VectorSubtract( vert[elem[1]], RI.viewOrigin, verts[1] );
		VectorSubtract( vert[elem[2]], RI.viewOrigin, verts[2] );
		ClipSkyPolygon( 3, verts[0], 0 );
	}
	return r_warpfacevis;
}

/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox( void )
{
	int	i;

	RI.params |= RP_NOSKY;
	for( i = 0; i < 6; i++ )
	{
		RI.skyMins[0][i] = RI.skyMins[1][i] = 9999999;
		RI.skyMaxs[0][i] = RI.skyMaxs[1][i] = -9999999;
	}
}

static void MakeSkyVec( float x, float y, float z, int axis, vec3_t v )
{
	int	j, k;
	vec3_t	b;

	VectorSet( b, x, y, z );

	for( j = 0; j < 3; j++ )
	{
		k = st_to_vec[axis][j];
		if( k < 0 ) v[j] = -b[-k-1];
		else v[j] = b[k-1];
	}
}