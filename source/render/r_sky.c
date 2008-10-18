//=======================================================================
//			Copyright XashXT Group 2007 ©
//		      r_sky.c - render skydome or skybox
//=======================================================================

#include "r_local.h"
#include "r_meshbuffer.h"
#include "mathlib.h"
#include "matrixlib.h"
#include "const.h"

#define MAX_CLIP_VERTS	64

#define SIDE_SIZE		9
#define VERTS_SIZE		( SIDE_SIZE * SIDE_SIZE )
#define ELEMS_SIZE		(( SIDE_SIZE-1 )*( SIDE_SIZE-1 ) * 6 )

#define SPHERE_RAD		10.0
#define EYE_RAD		9.0

#define SCALE_S		4.0  // arbitrary (?) texture scaling factors
#define SCALE_T		4.0

#define BOX_SIZE		1.0f
#define BOX_STEP		BOX_SIZE / ( SIDE_SIZE-1 ) * 2.0f


static vec3_t skyclip[6] =
{
{  1,  1, 0 },
{  1, -1, 0 },
{  0, -1, 1 },
{  0,  1, 1 },
{  1,  0, 1 },
{ -1,  0, 1 }
};

// 1 = s, 2 = t, 3 = 2048
static const int st_to_vec[6][3] =
{
{  3, -1,  2 },
{ -3,  1,  2 },
{  1,  3,  2 },
{ -1, -3,  2 },
{ -2, -1,  3 },  // 0 degrees yaw, look straight up
{  2, -1, -3 }   // look straight down
};

// s = [0]/[2], t = [1]/[2]
static const int vec_to_st[6][3] =
{
{ -2,  3,  1 },
{  2,  3, -1 },
{  1,  3,  2 },
{ -1,  3, -2 },
{ -2, -1,  3 },
{ -2,  1, -3 }
};

static const int skytexorder_q2[6] =
{
	SKYBOX_RIGHT,
	SKYBOX_FRONT,
	SKYBOX_LEFT,
	SKYBOX_BACK,
	SKYBOX_TOP,
	SKYBOX_BOTTOM
};

uint		r_skydome_elems[6][ELEMS_SIZE];
meshbuffer_t	r_skydome_mbuffer;
static mfog_t	*r_skyfog;
static msurface_t	*r_warpface;
static bool	r_warpfacevis;

static void Gen_BoxSide( skydome_t *skydome, int side, vec3_t orig, vec3_t drow, vec3_t dcol, float skyheight );
static void Gen_Box( skydome_t *skydome, float skyheight );

/*
==============
R_CreateSkydome
==============
*/
skydome_t *R_CreateSkydome( float skyheight, ref_shader_t **farboxShaders, ref_shader_t	**nearboxShaders )
{
	int		i, size;
	rb_mesh_t		*mesh;
	skydome_t 	*skydome;
	byte		*buffer;

	size = sizeof( skydome_t ) + sizeof( rb_mesh_t ) * 6 + sizeof( vec4_t ) * VERTS_SIZE * 6 +
		sizeof( vec4_t ) * VERTS_SIZE * 6 + sizeof( vec2_t ) * VERTS_SIZE * 11;
	buffer = Mem_Alloc( r_shaderpool, size );

	skydome = ( skydome_t * )buffer;
	Mem_Copy( skydome->farboxShaders, farboxShaders, sizeof(ref_shader_t *) * 6 );
	Mem_Copy( skydome->nearboxShaders, nearboxShaders, sizeof(ref_shader_t *) * 6 );
	buffer += sizeof( skydome_t );

	skydome->meshes = ( rb_mesh_t * )buffer;
	buffer += sizeof( rb_mesh_t ) * 6;

	for( i = 0, mesh = skydome->meshes; i < 6; i++, mesh++ )
	{
		mesh->numVerts = VERTS_SIZE;
		mesh->points = ( vec4_t * )buffer;
		buffer += sizeof( vec4_t ) * VERTS_SIZE;
		mesh->normal = ( vec4_t * )buffer;
		buffer += sizeof( vec4_t ) * VERTS_SIZE;
		if( i != 5 )
		{
			skydome->sphereStCoords[i] = (vec2_t *)buffer;
			buffer += sizeof( vec2_t ) * VERTS_SIZE;
		}
		skydome->linearStCoords[i] = (vec2_t *)buffer;
		buffer += sizeof(vec2_t) * VERTS_SIZE;

		mesh->numIndexes = ELEMS_SIZE;
		mesh->indexes = r_skydome_elems[i];
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
	Mem_Free( skydome );
}

void MakeSkyVec( float x, float y, float z, int axis, vec3_t v )
{
	int	j, k;
	vec3_t	b;

	b[0] = x;
	b[1] = y;
	b[2] = z;

	for( j = 0; j < 3; j++ )
	{
		k = st_to_vec[axis][j];
		if( k < 0 ) v[j] = -b[-k - 1];
		else v[j] = b[k - 1];
	}
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
	float	*v, *n, *st = NULL, *st2;
	int	r, c;
	float	t, d, d2, b, b2, q[2], s;

	s = 1.0 / ( SIDE_SIZE-1 );
	d = EYE_RAD;	// sphere center to camera distance
	d2 = d * d;
	b = SPHERE_RAD;	// sphere radius
	b2 = b * b;
	q[0] = 1.0f / ( 2.0f * SCALE_S );
	q[1] = 1.0f / ( 2.0f * SCALE_T );

	v = skydome->meshes[side].points[0];
	n = skydome->meshes[side].normal[0];
	if( side != 5 ) st = skydome->sphereStCoords[side][0];
	st2 = skydome->linearStCoords[side][0];

	VectorCopy( orig, row );

//	CrossProduct( dcol, drow, norm );
//	VectorNormalize( norm );
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

			if( st )
			{
				// use x and y on sphere as s and t
				// minus is here so skies scoll in correct (Q3A's) direction
				st[0] = -w[0] * q[0];
				st[1] = -w[1] * q[1];

				// to avoid bilerp seam
				st[0] = ( bound( -1, st[0], 1 ) + 1.0 ) * 0.5f;
				st[1] = ( bound( -1, st[1], 1 ) + 1.0 ) * 0.5f;
			}

			st2[0] = c * s;
			st2[1] = 1.0 - r * s;

			VectorAdd( pos, dcol, pos );
			VectorCopy( norm, n );

			v += 4;
			n += 4;
			if( st ) st += 2;
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
	meshbuffer_t	*mbuffer = &r_skydome_mbuffer;

	if( Ref.skyMins[0][side] >= Ref.skyMaxs[0][side] || Ref.skyMins[1][side] >= Ref.skyMaxs[1][side] )
		return;

	mbuffer->shaderKey = shader->sortKey;
	mbuffer->dlightbits = 0;
	mbuffer->sortKey = R_FOGNUM( r_skyfog );

	skydome->meshes[side].st = skydome->linearStCoords[side];
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

	features = shaders[0]->features;
	if( r_shownormals->integer )
		features |= MF_NORMALS;

	for( i = 0; i < 6; i++ )
		R_DrawSkySide( skydome, i, shaders[skytexorder_q2[i]], features );
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
	ref_shader_t	*shader;

	// FIXME: register another shader instead maybe?
	shader = R_OcclusionShader();

	features = shader->features;
	if( r_shownormals->integer ) features |= MF_NORMALS;

	// skies ought not to write to depth buffer
	R_DrawSkySide( skydome, 5, shader, features );
}

/*
==============
R_DrawSky
==============
*/
void R_DrawSky( ref_shader_t *shader )
{
	vec3_t		mins, maxs;
	matrix4x4		m, oldm;
	uint		*elem;
	skydome_t		*skydome = r_skydomes[shader-r_shaders[0]] ? r_skydomes[shader-r_shaders[0]] : NULL;
	meshbuffer_t	*mbuffer = &r_skydome_mbuffer;
	int		i, u, v, umin, umax, vmin, vmax;

	if( !skydome ) return;

	ClearBounds( mins, maxs );
	for( i = 0; i < 6; i++ )
	{
		if( Ref.skyMins[0][i] >= Ref.skyMaxs[0][i] || Ref.skyMins[1][i] >= Ref.skyMaxs[1][i] )
			continue;

		umin = (int)(( Ref.skyMins[0][i] + 1.0f ) * 0.5f * (float)( SIDE_SIZE-1 ));
		umax = (int)(( Ref.skyMaxs[0][i] + 1.0f ) * 0.5f * (float)( SIDE_SIZE-1 )) + 1;
		vmin = (int)(( Ref.skyMins[1][i] + 1.0f ) * 0.5f * (float)( SIDE_SIZE-1 ));
		vmax = (int)(( Ref.skyMaxs[1][i] + 1.0f ) * 0.5f * (float)( SIDE_SIZE-1 )) + 1;

		umin = bound( 0, umin, SIDE_SIZE-1 );
		umax = bound( 0, umax, SIDE_SIZE-1 );
		vmin = bound( 0, vmin, SIDE_SIZE-1 );
		vmax = bound( 0, vmax, SIDE_SIZE-1 );

		// box elems in tristrip order
		elem = skydome->meshes[i].indexes;
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

		AddPointToBounds( skydome->meshes[i].points[vmin*SIDE_SIZE+umin], mins, maxs );
		AddPointToBounds( skydome->meshes[i].points[vmax*SIDE_SIZE+umax], mins, maxs );

		skydome->meshes[i].numIndexes = ( vmax-vmin )*( umax-umin )*6;
	}

	VectorAdd( mins, Ref.vieworg, mins );
	VectorAdd( maxs, Ref.vieworg, maxs );

	if( Ref.refdef.rdflags & RDF_SKYPORTALINVIEW )
	{
		R_DrawSkyPortal( &Ref.refdef.skyportal, mins, maxs );
		return;
	}

	// center skydome on camera to give the illusion of a larger space
	Matrix4x4_Copy( Ref.modelViewMatrix, oldm );
	Matrix4x4_Copy( Ref.worldMatrix, Ref.modelViewMatrix );
	Matrix4x4_Copy( Ref.worldMatrix, m );
	Matrix4x4_SetOrigin( m, 0.0f, 0.0f, 0.0f );
	m[3][3] = 1.0; // FIXME

	// build sky matrix
#if 0
	VectorNegate( r_forward, r_backward );
	VectorNegate( r_right, r_left );
	Matrix4x4_FromVectors( r_skybox, r_left, r_up, r_backward, r_origin );
	if( sky->rotate ) Matrix4x4_ConcatRotate( r_skybox, r_refdef.time * sky->rotate, sky->axis[0], sky->axis[1], sky->axis[2] );
	Matrix4x4_Transpose( r_skyMatrix, r_skybox ); // finally we need to transpose matrix
#endif
	GL_LoadMatrix( m );

	gldepthmin = 1;
	gldepthmax = 1;
	pglDepthRange( gldepthmin, gldepthmax );

	if( Ref.params & RP_CLIPPLANE ) pglDisable( GL_CLIP_PLANE0 );

	// it can happen that sky surfaces have no fog hull specified
	// yet there's a global fog hull (see wvwq3dm7)
	if( !r_skyfog ) r_skyfog = r_worldBrushModel->globalfog;

	if( skydome->farboxShaders[0] )
		R_DrawSkyBox( skydome, skydome->farboxShaders );
	else R_DrawBlackBottom( skydome );

	if( shader->numStages )
	{
		bool	flush = false;
		int	features = shader->features;

		if( r_shownormals->integer ) features |= MF_NORMALS;

		for( i = 0; i < 5; i++ )
		{
			if( Ref.skyMins[0][i] >= Ref.skyMaxs[0][i] || Ref.skyMins[1][i] >= Ref.skyMaxs[1][i] )
				continue;

			flush = true;
			mbuffer->shaderKey = shader->sortKey;
			mbuffer->dlightbits = 0;
			mbuffer->sortKey = R_FOGNUM( r_skyfog );

			skydome->meshes[i].st = skydome->sphereStCoords[i];
			R_PushMesh( &skydome->meshes[i], features );
		}
		if( flush ) R_RenderMeshBuffer( mbuffer );
	}

	if( skydome->nearboxShaders[0] ) R_DrawSkyBox( skydome, skydome->nearboxShaders );

	if( Ref.params & RP_CLIPPLANE ) pglEnable( GL_CLIP_PLANE0 );

	Matrix4x4_Copy( oldm, Ref.modelViewMatrix );
	GL_LoadMatrix( Ref.worldMatrix );

	gldepthmin = 0;
	gldepthmax = 1;
	pglDepthRange( gldepthmin, gldepthmax );

	r_skyfog = NULL;
}

//===================================================================

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

	if( !r_skyfog ) r_skyfog = r_warpface->fog;
	r_warpfacevis = true;

	// project new texture coords
	for( i = 0; i < nump; i++, vecs += 3 )
	{
		j = vec_to_st[axis][2];
		dv = ( j > 0 ) ? vecs[j - 1] : -vecs[-j - 1];

		if( dv < 0.001 ) continue; // don't divide by zero

		dv = 1.0f / dv;

		j = vec_to_st[axis][0];
		s = ( j < 0 ) ? -vecs[-j -1] * dv : vecs[j-1] * dv;

		j = vec_to_st[axis][1];
		t = ( j < 0 ) ? -vecs[-j -1] * dv : vecs[j-1] * dv;

		if( s < Ref.skyMins[0][axis] ) Ref.skyMins[0][axis] = s;
		if( t < Ref.skyMins[1][axis] ) Ref.skyMins[1][axis] = t;
		if( s > Ref.skyMaxs[0][axis] ) Ref.skyMaxs[0][axis] = s;
		if( t > Ref.skyMaxs[1][axis] ) Ref.skyMaxs[1][axis] = t;
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

	if( nump > MAX_CLIP_VERTS ) Host_Error( "ClipSkyPolygon: MAX_CLIP_VERTS limit exceeded\n" );
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
		else sides[i] = SIDE_ON;
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
	VectorCopy( vecs, ( vecs + ( i*3 )));
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
	ClipSkyPolygon( newc[0], newv[0][0], stage+1 );
	ClipSkyPolygon( newc[1], newv[1][0], stage+1 );
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
	uint	*elem;
	rb_mesh_t *mesh;
	vec3_t	verts[4];

	// calculate vertex values for sky box
	r_warpface = fa;
	r_warpfacevis = false;

	mesh = fa->mesh;
	elem = mesh->indexes;
	vert = mesh->points;

	for( i = 0; i < mesh->numIndexes; i += 3, elem += 3 )
	{
		VectorSubtract( vert[elem[0]], Ref.vieworg, verts[0] );
		VectorSubtract( vert[elem[1]], Ref.vieworg, verts[1] );
		VectorSubtract( vert[elem[2]], Ref.vieworg, verts[2] );
		ClipSkyPolygon( 3, verts[0], 0 );
	}
	return r_warpfacevis;
}

/*
==============
R_ClearSky
==============
*/
void R_ClearSky( void )
{
	int i;

	Ref.params |= RP_NOSKY;
	for( i = 0; i < 6; i++ )
	{
		Ref.skyMins[0][i] = Ref.skyMins[1][i] = 9999999;
		Ref.skyMaxs[0][i] = Ref.skyMaxs[1][i] = -9999999;
	}
}