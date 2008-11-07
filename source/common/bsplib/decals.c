//=======================================================================
//			Copyright XashXT Group 2008 ©
//		    decals.c - project static decals onto plane
//=======================================================================

#include "bsplib.h"
#include "const.h"

#define PLANAR_EPSILON		0.5f
#define MAX_PROJECTORS		1024
#define Vector2Subtract(a, b, c)	((c)[0] = (a)[0] - (b)[0], (c)[1] = (a)[1] - (b)[1])

typedef struct decalProjector_s
{
	bsp_shader_t	*si;
	vec3_t		mins, maxs;
	vec3_t		center;
	float		radius, radius2;
	int		numPlanes;	// either 5 or 6, for quad or triangle projectors
	vec4_t		planes[6];
	vec4_t		texMat[2];
}
decalProjector_t;

static int		numProjectors = 0;
static decalProjector_t	projectors[MAX_PROJECTORS];
static int		numDecalSurfaces = 0;
static vec3_t		entityOrigin;

/*
================
DVectorNormalize

normalizes a vector, returns the length, operates using doubles
================
*/
typedef double	dvec_t;
typedef dvec_t	dvec3_t[ 3 ];

dvec_t DVectorNormalize( dvec3_t in, dvec3_t out )
{
	dvec_t	len, ilen;
	
	
	len = (dvec_t) sqrt(DotProduct( in, in ));
	if( len == 0.0 )
	{
		VectorClear( out );
		return 0.0;
	}
	
	ilen = 1.0 / len;
	out[0] = in[0] * ilen;
	out[1] = in[1] * ilen;
	out[2] = in[2] * ilen;
	
	return len;
}

/*
================
MakeTextureMatrix

generates a texture projection matrix for a triangle
returns false if a texture matrix cannot be created
================
*/
static bool MakeTextureMatrix( decalProjector_t *dp, vec4_t projection, dvertex_t *a, dvertex_t *b, dvertex_t *c )
{
	int		i, j;
	double		bb, s, t, d;
	dvec3_t		pa, pb, pc;
	dvec3_t		bary, xyz;
	dvec3_t		vecs[3], axis[3], lengths;
	
	
	// project triangle onto plane of projection
	d = DotProduct( a->point, projection ) - projection[3];
	VectorMA( a->point, -d, projection, pa );
	d = DotProduct( b->point, projection ) - projection[3];
	VectorMA( b->point, -d, projection, pb );
	d = DotProduct( c->point, projection ) - projection[3];
	VectorMA( c->point, -d, projection, pc );
	
	/* two methods */
	#if 1
	{
		// old code
		
		// calculate barycentric basis for the triangle
		bb = (b->st[0] - a->st[0]) * (c->st[1] - a->st[1]) - (c->st[0] - a->st[0]) * (b->st[1] - a->st[1]);
		if( fabs( bb ) < 0.00000001 ) return false;
		
		// calculate texture origin
		// calculate s vector
		s = a->st[0] + 1.0;
		t = a->st[1] + 0.0;
		bary[0] = ((b->st[0] - s) * (c->st[1] - t) - (c->st[0] - s) * (b->st[1] - t)) / bb;
		bary[1] = ((c->st[0] - s) * (a->st[1] - t) - (a->st[0] - s) * (c->st[1] - t)) / bb;
		bary[2] = ((a->st[0] - s) * (b->st[1] - t) - (b->st[0] - s) * (a->st[1] - t)) / bb;
		
		xyz[0] = bary[0] * pa[0] + bary[1] * pb[0] + bary[2] * pc[0];
		xyz[1] = bary[0] * pa[1] + bary[1] * pb[1] + bary[2] * pc[1];
		xyz[2] = bary[0] * pa[2] + bary[1] * pb[2] + bary[2] * pc[2];
		
		VectorSubtract( xyz, pa, vecs[0] );
		
		// calculate t vector
		s = a->st[0] + 0.0;
		t = a->st[1] + 1.0;
		bary[0] = ((b->st[0] - s) * (c->st[1] - t) - (c->st[0] - s) * (b->st[1] - t)) / bb;
		bary[1] = ((c->st[0] - s) * (a->st[1] - t) - (a->st[0] - s) * (c->st[1] - t)) / bb;
		bary[2] = ((a->st[0] - s) * (b->st[1] - t) - (b->st[0] - s) * (a->st[1] - t)) / bb;
		
		xyz[0] = bary[0] * pa[0] + bary[1] * pb[0] + bary[2] * pc[0];
		xyz[1] = bary[0] * pa[1] + bary[1] * pb[1] + bary[2] * pc[1];
		xyz[2] = bary[0] * pa[2] + bary[1] * pb[2] + bary[2] * pc[2];
		
		VectorSubtract( xyz, pa, vecs[1] );
		
		// calcuate r vector
		VectorScale( projection, -1.0, vecs[2] );
		
		// calculate transform axis
		for( i = 0; i < 3; i++ )
			lengths[i] = DVectorNormalize( vecs[i], axis[i] );
		for( i = 0; i < 2; i++ )
			for( j = 0; j < 3; j++ )
				dp->texMat[i][j] = lengths[i] > 0.0 ? (axis[i][j] / lengths[i]) : 0.0;

		// calculalate translation component
		dp->texMat[0][3] = a->st[0] - DotProduct( a->point, dp->texMat[0] );
		dp->texMat[1][3] = a->st[1] - DotProduct( a->point, dp->texMat[1] );
	}
	#else
	{
		int		k;
		dvec3_t		origin, deltas[3];
		double		texDeltas[3][2];
		double		delta, texDelta;
		
		
		// new code
		
		// calculate deltas
		VectorSubtract( pa, pb, deltas[0] );
		VectorSubtract( pa, pc, deltas[1] );
		VectorSubtract( pb, pc, deltas[2] );
		Vector2Subtract( a->st, b->st, texDeltas[0] );
		Vector2Subtract( a->st, c->st, texDeltas[1] );
		Vector2Subtract( b->st, c->st, texDeltas[2] );
		
		for( i = 0; i < 2; i++ )
		{
			for( j = 0; j < 3; j++ )
			{
				delta = 0.0;
				texDelta = 0.0;
				
				for( k = 0; k < 3; k++ )
				{
					if( fabs( deltas[k][j] ) > delta && fabs( texDeltas[k][i] ) > texDelta )
					{
						delta = deltas[k][j];
						texDelta = texDeltas[k][i];
					}
				}
				
				// set texture matrix component
				if( fabs( delta ) > 0.0 )
					dp->texMat[i][j] = texDelta / delta;
				else dp->texMat[i][j] = 0.0;
			}
		
			// set translation component
			dp->texMat[ i ][3] = a->st[ i ] - DotProduct( pa, dp->texMat[ i ] );
		}
	}
	#endif

	// test texture matrix
	s = DotProduct( a->point, dp->texMat[0] ) + dp->texMat[0][3];
	t = DotProduct( a->point, dp->texMat[1] ) + dp->texMat[1][3];
	if( fabs( s - a->st[0] ) > 0.01 || fabs( t - a->st[1] ) > 0.01 )
	{
		MsgDev( D_ERROR, "Bad texture matrix! (A) (%f, %f) != (%f, %f)\n", s, t, a->st[0], a->st[1] );
		// return false;
	}
	s = DotProduct( b->point, dp->texMat[0] ) + dp->texMat[0][3];
	t = DotProduct( b->point, dp->texMat[1] ) + dp->texMat[1][3];
	if( fabs( s - b->st[0] ) > 0.01 || fabs( t - b->st[1] ) > 0.01 )
	{
		MsgDev( D_ERROR, "Bad texture matrix! (B) (%f, %f) != (%f, %f)\n", s, t, b->st[0], b->st[1] );
		// return false;
	}
	s = DotProduct( c->point, dp->texMat[0] ) + dp->texMat[0][3];
	t = DotProduct( c->point, dp->texMat[1] ) + dp->texMat[1][3];
	if( fabs( s - c->st[0] ) > 0.01 || fabs( t - c->st[1] ) > 0.01 )
	{
		MsgDev( D_ERROR, "Bad texture matrix! (C) (%f, %f) != (%f, %f)\n", s, t, c->st[0], c->st[1] );
		// return false;
	}
	return true;
}

/*
================
TransformDecalProjector

transforms a decal projector
note: non-normalized axes will screw up the plane transform
================
*/
static void TransformDecalProjector( decalProjector_t *in, vec3_t axis[3], vec3_t origin, decalProjector_t *out )
{
	int		i;
	
	out->si = in->si;
	out->numPlanes = in->numPlanes;
	
	// translate bounding box and sphere (note: rotated projector bounding box will be invalid!)
	VectorSubtract( in->mins, origin, out->mins );
	VectorSubtract( in->maxs, origin, out->maxs );
	VectorSubtract( in->center, origin, out->center );
	out->radius = in->radius;
	out->radius2 = in->radius2;
	
	// translate planes
	for( i = 0; i < in->numPlanes; i++ )
	{
		out->planes[i][0] = DotProduct( in->planes[i], axis[0] );
		out->planes[i][1] = DotProduct( in->planes[i], axis[1] );
		out->planes[i][2] = DotProduct( in->planes[i], axis[2] );
		out->planes[i][3] = in->planes[i][3] - DotProduct( out->planes[i], origin );
	}
	
	// translate texture matrix
	for( i = 0; i < 2; i++ )
	{
		out->texMat[i][0] = DotProduct( in->texMat[i], axis[0] );
		out->texMat[i][1] = DotProduct( in->texMat[i], axis[1] );
		out->texMat[i][2] = DotProduct( in->texMat[i], axis[2] );
		out->texMat[i][3] = in->texMat[i][3] + DotProduct( out->texMat[i], origin );
	}
}


/*
================
MakeDecalProjector

creates a new decal projector from a triangle
================
*/
static int MakeDecalProjector( bsp_shader_t *si, vec4_t projection, float distance, int numVerts, dvertex_t **dv )
{
	int		i, j;
	decalProjector_t	*dp;
	vec3_t		xyz;
	
	if( numVerts != 3 && numVerts != 4 )
		return -1;
	
	if( numProjectors >= MAX_PROJECTORS )
	{
		MsgDev( D_WARN, "MAX_PROJECTORS limit (%d) exceeded\n", MAX_PROJECTORS );
		return -2;
	}
	
	dp = &projectors[numProjectors];
	memset( dp, 0, sizeof( *dp ));
	
	dp->si = si;
	dp->numPlanes = numVerts + 2;
	
	if( !MakeTextureMatrix( dp, projection, dv[0], dv[1], dv[2] ))
		return -1;
	
	ClearBounds( dp->mins, dp->maxs );
	for( i = 0; i < numVerts; i++ )
	{
		AddPointToBounds( dv[i]->point, dp->mins, dp->maxs );
		VectorMA( dv[i]->point, distance, projection, xyz );
		AddPointToBounds( xyz, dp->mins, dp->maxs );
	}
	
	// make bouding sphere
	VectorAverage( dp->mins, dp->maxs, dp->center );
	VectorSubtract( dp->maxs, dp->center, xyz );
	dp->radius = VectorLength( xyz );
	dp->radius2 = dp->radius * dp->radius;
	
	// make the front plane
	if( !PlaneFromPoints( dp->planes[0], dv[0]->point, dv[1]->point, dv[2]->point ))
		return -1;
	
	// make the back plane
	VectorSubtract( vec3_origin, dp->planes[0], dp->planes[1] );
	VectorMA( dv[0]->point, distance, projection, xyz );
	dp->planes[1][3] = DotProduct( xyz, dp->planes[1] );
	
	// make the side planes
	for( i = 0; i < numVerts; i++ )
	{
		j = (i + 1) % numVerts;
		VectorMA( dv[i]->point, distance, projection, xyz );
		if( !PlaneFromPoints( dp->planes[i+2], dv[j]->point, dv[i]->point, xyz ) )
			return -1;
	}
	
	numProjectors++;
	return numProjectors - 1;
}


/*
================
ProcessDecals

finds all decal entities and creates decal projectors
================
*/
void ProcessDecals( void )
{
	int		i, j, x, y, pw[5], r, iterations;
	float		distance;
	vec4_t		projection, plane;
	vec3_t		origin, target, delta;
	bsp_entity_t	*e, *e2;
	parseMesh_t	*p;
	bsp_mesh_t	*mesh, *subdivided;
	dvertex_t		*dv[4];
	const char	*value;
	
	MsgDev( D_NOTE, "--- ProcessDecals ---\n" );
	
	for( i = 0; i < num_entities; i++ )
	{
		e = &entities[i];
		value = ValueForKey( e, "classname" );
		if( com.stricmp( value, "_decal" ) )
			continue;
		
		if( e->patches == NULL )
		{
			MsgDev( D_WARN, "Decal entity without any patch meshes, ignoring.\n" );
			e->epairs = NULL;
			continue;
		}
		
		value = ValueForKey( e, "target" );
		e2 = FindTargetEntity( value );
		
		if( e2 == NULL )
		{
			MsgDev( D_WARN, "Decal entity without a valid target, ignoring.\n" );
			continue;
		}
		
		for( p = e->patches; p != NULL; p = e->patches )
		{
			if( VectorCompare( e->origin, vec3_origin ))
			{
				VectorAdd( p->eMins, p->eMaxs, origin );
				VectorScale( origin, 0.5f, origin );
			}
			else VectorCopy( e->origin, origin );
			
			VectorCopy( e2->origin, target );
			VectorSubtract( target, origin, delta );
			
			VectorCopy( delta, projection );
			distance = VectorNormalizeLength( projection );
			projection[3] = DotProduct( origin, projection );
			
			if( distance > 0.125f )
			{
				iterations = IterationsForCurve( p->longestCurve, patchSubdivisions );
				subdivided = SubdivideMesh2( p->mesh, iterations );
				
				// fit it to the curve and remove colinear verts on rows/columns
				PutMeshOnCurve( *subdivided );
				mesh = RemoveLinearMeshColumnsRows( subdivided );
				FreeMesh( subdivided );
				
				// offset by projector origin
				for( j = 0; j < (mesh->width * mesh->height); j++ )
					VectorAdd( mesh->verts[j].point, e->origin, mesh->verts[j].point );
				
				// iterate through the mesh quads
				for( y = 0; y < (mesh->height - 1); y++ )
				{
					for( x = 0; x < (mesh->width - 1); x++ )
					{
						pw[0] = x + (y * mesh->width);
						pw[1] = x + ((y + 1) * mesh->width);
						pw[2] = x + 1 + ((y + 1) * mesh->width);
						pw[3] = x + 1 + (y * mesh->width);
						pw[4] = x + (y * mesh->width); // same as pw[0]
						
						r = (x + y) & 1;
						
						dv[0] = &mesh->verts[pw[r+0]];
						dv[1] = &mesh->verts[pw[r+1]];
						dv[2] = &mesh->verts[pw[r+2]];
						dv[3] = &mesh->verts[pw[r+3]];
						
						// planar? (nuking this optimization as it doesn't work on non-rectangular quads)
						plane[0] = 0.0f;	// stupid msvc
						if( 0 && PlaneFromPoints( plane, dv[0]->point, dv[1]->point, dv[2]->point ) &&
							fabs( DotProduct( dv[1]->point, plane ) - plane[3] ) <= PLANAR_EPSILON )
						{
							// make a quad projector
							MakeDecalProjector( p->shader, projection, distance, 4, dv );
						}
						else
						{
							// make first triangle
							MakeDecalProjector( p->shader, projection, distance, 3, dv );
							
							// make second triangle
							dv[1] = dv[2];
							dv[2] = dv[3];
							MakeDecalProjector( p->shader, projection, distance, 3, dv );
						}
					}
				}
				BSP_Free( mesh );
			}
			
			// remove patch from entity
			e->patches = p->next;
		}
	}
	MsgDev( D_INFO, "%6i decal projectors\n", numProjectors );
}



/*
================
ProjectDecalOntoWinding

projects a decal onto a winding
================
*/
static void ProjectDecalOntoWinding( decalProjector_t *dp, drawsurf_t *ds, winding_t *w )
{
	int		i, j;
	float		d, d2, alpha;
	winding_t		*front, *back;
	drawsurf_t	*ds2;
	dvertex_t		*dv;
	vec4_t		plane;
	
	if( w->numpoints < 3 )
	{
		FreeWinding( w );
		return;
	}
	
	// offset by entity origin
	for( i = 0; i < w->numpoints; i++ )
		VectorAdd( w->p[i], entityOrigin, w->p[i] );
	
	// make a plane from the winding
	if( !PlaneFromPoints( plane, w->p[0], w->p[1], w->p[2] ))
	{
		FreeWinding( w );
		return;
	}
	
	// backface check
	d = DotProduct( dp->planes[0], plane );
	if( d < -0.0001f )
	{
		FreeWinding( w );
		return;
	}
	
	for( i = 0; i < dp->numPlanes; i++ )
	{
		// chop winding by the plane
		ClipWindingEpsilon( w, dp->planes[i], dp->planes[i][3], 0.0625f, &front, &back );
		FreeWinding( w );
		
		// lose the front fragment
		if( front != NULL ) FreeWinding( front );
		
		// if nothing left in back, then bail
		if( back == NULL ) return;
		
		// reset winding
		w = back;
	}
	
	// nothing left?
	if( w == NULL || w->numpoints < 3 ) return;
	
	// add to counts
	numDecalSurfaces++;
	
	// make a new surface
	ds2 = AllocDrawSurf( SURFACE_DECAL );
	
	// set it up
	ds2->entitynum = ds->entitynum;
	ds2->castShadows = ds->castShadows;
	ds2->recvShadows = ds->recvShadows;
	ds2->shader = dp->si;
	ds2->fogNum = ds->fogNum;	// why was this -1?
	ds2->lightmapScale = ds->lightmapScale;
	ds2->numVerts = w->numpoints;
	ds2->verts = BSP_Malloc( ds2->numVerts * sizeof( *ds2->verts ));
	
	// set vertexes
	for( i = 0; i < ds2->numVerts; i++ )
	{
		dv = &ds2->verts[i];
		
		d = DotProduct( w->p[i], dp->planes[0] ) - dp->planes[0][3];
		d2 = DotProduct( w->p[i], dp->planes[1] ) - dp->planes[1][3];
		alpha = 255.0f * d2 / (d + d2);
		alpha = bound( 0, alpha, 255 );
		
		// set misc
		VectorSubtract( w->p[i], entityOrigin, dv->point );
		VectorCopy( plane, dv->normal );
		dv->st[0] = DotProduct( dv->point, dp->texMat[0] ) + dp->texMat[0][3];
		dv->st[1] = DotProduct( dv->point, dp->texMat[1] ) + dp->texMat[1][3];
		
		// set color
		for( j = 0; j < LM_STYLES; j++ )
		{
			dv->color[j][0] = 255;
			dv->color[j][1] = 255;
			dv->color[j][2] = 255;
			dv->color[j][3] = alpha;
		}
	}
}

/*
================
ProjectDecalOntoFace

projects a decal onto a brushface surface
================
*/
static void ProjectDecalOntoFace( decalProjector_t *dp, drawsurf_t *ds )
{
	vec4_t		plane;
	float		d;
	winding_t		*w;
	
	if( ds->sideRef == NULL || ds->sideRef->side == NULL )
		return;
	
	// backface check
	if( ds->planar )
	{
		VectorCopy( mapplanes[ds->planeNum].normal, plane );
		plane[3] = mapplanes[ds->planeNum].dist + DotProduct( plane, entityOrigin );
		d = DotProduct( dp->planes[0], plane );
		if( d < -0.0001f ) return;
	}
	
	// generate decal
	w = WindingFromDrawSurf( ds );
	ProjectDecalOntoWinding( dp, ds, w );
}



/*
================
ProjectDecalOntoPatch

projects a decal onto a patch surface
================
*/
static void ProjectDecalOntoPatch( decalProjector_t *dp, drawsurf_t *ds )
{
	int		x, y, pw[5], r, iterations;
	vec4_t		plane;
	float		d;
	bsp_mesh_t	src, *mesh, *subdivided;
	winding_t		*w;
	
	
	// backface check
	if( ds->planar )
	{
		VectorCopy( mapplanes[ ds->planeNum ].normal, plane );
		plane[3] = mapplanes[ ds->planeNum ].dist + DotProduct( plane, entityOrigin );
		d = DotProduct( dp->planes[0], plane );
		if( d < -0.0001f ) return;
	}
	
	// tesselate the patch
	src.width = ds->patchWidth;
	src.height = ds->patchHeight;
	src.verts = ds->verts;
	iterations = IterationsForCurve( ds->longestCurve, patchSubdivisions );
	subdivided = SubdivideMesh2( src, iterations );
	
	// fit it to the curve and remove colinear verts on rows/columns
	PutMeshOnCurve( *subdivided );
	mesh = RemoveLinearMeshColumnsRows( subdivided );
	FreeMesh( subdivided );
	
	// iterate through the mesh quads
	for( y = 0; y < (mesh->height - 1); y++ )
	{
		for( x = 0; x < (mesh->width - 1); x++ )
		{
			// set indexes
			pw[0] = x + (y * mesh->width);
			pw[1] = x + ((y + 1) * mesh->width);
			pw[2] = x + 1 + ((y + 1) * mesh->width);
			pw[3] = x + 1 + (y * mesh->width);
			pw[ 4 ] = x + (y * mesh->width); // same as pw[0]
			
			// set radix
			r = (x + y) & 1;
			
			// generate decal for first triangle
			w = AllocWinding( 3 );
			w->numpoints = 3;
			VectorCopy( mesh->verts[pw[r+0]].point, w->p[0] );
			VectorCopy( mesh->verts[pw[r+1]].point, w->p[1] );
			VectorCopy( mesh->verts[pw[r+2]].point, w->p[2] );
			ProjectDecalOntoWinding( dp, ds, w );
			
			// generate decal for second triangle
			w = AllocWinding( 3 );
			w->numpoints = 3;
			VectorCopy( mesh->verts[pw[r+0]].point, w->p[0] );
			VectorCopy( mesh->verts[pw[r+2]].point, w->p[1] );
			VectorCopy( mesh->verts[pw[r+3]].point, w->p[2] );
			ProjectDecalOntoWinding( dp, ds, w );
		}
	}
	BSP_Free( mesh );
}



/*
================
ProjectDecalOntoTriangles

projects a decal onto a triangle surface
================
*/
static void ProjectDecalOntoTriangles( decalProjector_t *dp, drawsurf_t *ds )
{
	int		i;
	vec4_t		plane;
	float		d;
	winding_t		*w;
	
	// triangle surfaces without shaders don't get marks by default
	if( ds->type == SURFACE_TRIANGLES && ds->shader->shaderText == NULL )
		return;
	
	// backface check
	if( ds->planar )
	{
		VectorCopy( mapplanes[ds->planeNum].normal, plane );
		plane[3] = mapplanes[ds->planeNum].dist + DotProduct( plane, entityOrigin );
		d = DotProduct( dp->planes[0], plane );
		if( d < -0.0001f ) return;
	}
	
	// iterate through triangles
	for( i = 0; i < ds->numIndexes; i += 3 )
	{
		// generate decal
		w = AllocWinding( 3 );
		w->numpoints = 3;
		VectorCopy( ds->verts[ds->indexes[i]].point, w->p[0] );
		VectorCopy( ds->verts[ds->indexes[i+1]].point, w->p[1] );
		VectorCopy( ds->verts[ds->indexes[i+2]].point, w->p[2] );
		ProjectDecalOntoWinding( dp, ds, w );
	}
}


/*
================
MakeEntityDecals

projects decals onto world surfaces
================
*/
void MakeEntityDecals( bsp_entity_t *e )
{
	int		i, j, k;
	decalProjector_t	dp;
	drawsurf_t	*ds;
	vec3_t		identityAxis[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	
	MsgDev( D_NOTE, "--- MakeEntityDecals ---\n" );
	
	VectorCopy( e->origin, entityOrigin );
	VectorClear( entityOrigin );
	
	// FIXME: make thread
	for( i = 0; i < numProjectors; i++ )
	{
		// get projector
		TransformDecalProjector( &projectors[i], identityAxis, e->origin, &dp );
		
		// walk the list of surfaces in the entity
		for( j = e->firstsurf; j < numdrawsurfs; j++ )
		{
			ds = &drawsurfs[j];
			if( ds->numVerts <= 0 ) continue;
			
			// ignore autosprite or nomarks
			if( ds->shader->autosprite || (ds->shader->surfaceFlags & SURF_NOMARKS))
				continue;
			
			for( k = 0; k < 3; k++ )
				if( ds->mins[k] >= (dp.center[k] + dp.radius) || ds->maxs[k] <= (dp.center[k] - dp.radius))
					break;
			if( k < 3 ) continue;
			
			switch( ds->type )
			{
			case SURFACE_FACE:
				ProjectDecalOntoFace( &dp, ds );
				break;
			case SURFACE_PATCH:
				ProjectDecalOntoPatch( &dp, ds );
				break;
			case SURFACE_TRIANGLES:
			case SURFACE_FORCED_META:
			case SURFACE_META:
				ProjectDecalOntoTriangles( &dp, ds );
				break;
			default: break;
			}
		}
	}
	MsgDev( D_INFO, "%6i decal surfaces\n", numDecalSurfaces );
}
