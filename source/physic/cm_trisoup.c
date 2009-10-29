//=======================================================================
//			Copyright XashXT Group 2009 ©
//			cm_trisoup.c - trimesh collision
//=======================================================================

#include "cm_local.h"
#include "mathlib.h"

/*
================================================================================

PATCH COLLIDE GENERATION

================================================================================
*/
#define	USE_HASHING

#define PLANE_HASHES	1024
static cmplane_t		*planeHashTable[PLANE_HASHES];
static int		numPlanes;
static cmplane_t		planes[SHADER_MAX_TRIANGLES];
static int		numFacets;
static cfacet_t		facets[SHADER_MAX_TRIANGLES];

/*
================
return a hash value for a plane
================
*/
static long CM_GenerateHashValue( vec4_t plane )
{
	long	hash;

	hash = (int)fabs( plane[3] ) / 8;
	hash &= ( PLANE_HASHES - 1 );

	return hash;
}

/*
================
CM_AddPlaneToHash
================
*/
static void CM_AddPlaneToHash( cmplane_t *p )
{
	long	hash;

	hash = CM_GenerateHashValue( p->plane );

	p->hashChain = planeHashTable[hash];
	planeHashTable[hash] = p;
}

/*
================
CM_CreateNewFloatPlane
================
*/
static int CM_CreateNewFloatPlane( vec4_t plane )
{
#ifndef USE_HASHING
	// add a new plane
	if( numPlanes == SHADER_MAX_TRIANGLES )
		Host_Error( "CM_CreateNewFloatPlane: SHADER_MAX_TRIANGLES limit exceeded\n" );

	Vector4Copy( plane, planes[numPlanes].plane );
	planes[numPlanes].signbits = SignbitsForPlane( plane );
	numPlanes++;

	return numPlanes - 1;
#else
	cmplane_t		*p; // temp;

	// create a new plane
	if( numPlanes == SHADER_MAX_TRIANGLES )
		Host_Error( "CM_FindPlane: SHADER_MAX_TRIANGLES limit exceeded\n" );

	p = &planes[numPlanes];
	Vector4Copy( plane, p->plane );
	p->signbits = SignbitsForPlane(plane);

	numPlanes++;

	CM_AddPlaneToHash( p );
	return numPlanes - 1;
#endif
}

/*
==================
CM_FindPlane2
==================
*/
static int CM_FindPlane2( float plane[4], int *flipped )
{
#ifndef USE_HASHING
	int	i;

	// see if the points are close enough to an existing plane
	for( i = 0; i < numPlanes; i++ )
	{
		if( CM_PlaneEqual( &planes[i], plane, flipped ))
			return i;
	}

	*flipped = false;
	return CM_CreateNewFloatPlane( plane );
#else
	cmplane_t	*p;
	int	i, h, hash;

	hash = CM_GenerateHashValue( plane );

	// search the border bins as well
	for( i = -1; i <= 1; i++ )
	{
		h = (hash + i) & (PLANE_HASHES - 1);

		for( p = planeHashTable[h]; p; p = p->hashChain )
		{
			if( CM_PlaneEqual( p, plane, flipped ))
				return p - planes;
		}
	}

	*flipped = false;
	return CM_CreateNewFloatPlane( plane );
#endif
}

/*
==================
CM_FindPlane
==================
*/
static int CM_FindPlane( const float *p1, const float *p2, const float *p3 )
{
	float	plane[4];
	int	dummy;

	if( !CM_PlaneFromPoints( plane, p1, p2, p3 ))
		return -1;

	return CM_FindPlane2( plane, &dummy );
}

/*
==================
CM_PointOnPlaneSide
==================
*/
static int CM_PointOnPlaneSide( float *p, int planeNum )
{
	float	d, *plane;

	if( planeNum == -1 )
		return SIDE_ON;

	plane = planes[planeNum].plane;
	d = DotProduct( p, plane ) - plane[3];

	if( d > PLANE_TRI_EPSILON )
		return SIDE_FRONT;

	if( d < -PLANE_TRI_EPSILON )
		return SIDE_BACK;

	return SIDE_ON;
}

/*
==================
CM_GenerateBoundaryForPoints
==================
*/
static int CM_GenerateBoundaryForPoints( const vec4_t triPlane, const vec3_t p1, const vec3_t p2 )
{
	vec3_t	up;

	VectorMA( p1, 4, triPlane, up );
	return CM_FindPlane( p1, p2, up );
}

/*
===================
CM_SetBorderInward
===================
*/
static void CM_SetBorderInward( cfacet_t *facet, cTriangleSoup_t *triSoup, int i, int which )
{
	float	*points[4];
	int	k, l, numPoints;

	switch( which )
	{
	case 0:
		points[0] = triSoup->points[i][0];
		points[1] = triSoup->points[i][1];
		points[2] = triSoup->points[i][2];
		numPoints = 3;
		break;
	case 1:
		points[0] = triSoup->points[i][2];
		points[1] = triSoup->points[i][1];
		points[2] = triSoup->points[i][0];
		numPoints = 3;
		break;
	default:
		Host_Error( "CM_SetBorderInward: bad parameter %i\n", which );
		numPoints = 0;
		break;
	}

	for( k = 0; k < facet->numBorders; k++ )
	{
		int	front, back;

		front = 0;
		back = 0;

		for( l = 0; l < numPoints; l++ )
		{
			int	side;

			side = CM_PointOnPlaneSide( points[l], facet->borderPlanes[k] );
			if( side == SIDE_FRONT ) front++;
			if( side == SIDE_BACK ) back++;
		}

		if( front && !back ) facet->borderInward[k] = true;
		else if( back && !front ) facet->borderInward[k] = false;
		else if( !front && !back ) facet->borderPlanes[k] = -1; // flat side border
		else
		{
			// bisecting side border
			MsgDev( D_WARN, "CM_SetBorderInward: mixed plane sides\n" );
			facet->borderInward[k] = false;
		}
	}
}

/*
==================
CM_ValidateFacet

If the facet isn't bounded by its borders, we screwed up.
==================
*/
static bool CM_ValidateFacet( cfacet_t *facet )
{
	int		j;
	cwinding_t	*w;
	float		plane[4];
	vec3_t		bounds[2];

	if( facet->surfacePlane == -1 )
		return false;

	Vector4Copy( planes[facet->surfacePlane].plane, plane );

	w = CM_BaseWindingForPlane( plane, plane[3] );
	for( j = 0; j < facet->numBorders && w; j++ )
	{
		if( facet->borderPlanes[j] == -1 )
		{
			CM_FreeWinding( w );
			return false;
		}

		Vector4Copy( planes[facet->borderPlanes[j]].plane, plane );

		if( !facet->borderInward[j] )
		{
			VectorSubtract( vec3_origin, plane, plane );
			plane[3] = -plane[3];
		}
		CM_ChopWindingInPlace( &w, plane, plane[3], ON_EPSILON );
	}

	if( !w ) return false; // winding was completely chopped away

	// see if the facet is unreasonably large
	CM_WindingBounds( w, bounds[0], bounds[1] );
	CM_FreeWinding( w );

	for( j = 0; j < 3; j++ )
	{
		if( bounds[1][j] - bounds[0][j] > MAX_WORLD_COORD )
			return false; // we must be missing a plane
		if( bounds[0][j] >= MAX_WORLD_COORD )
			return false;
		if( bounds[1][j] <= MIN_WORLD_COORD )
			return false;
	}

	return true; // winding is fine
}

/*
==================
CM_AddFacetBevels
==================
*/
static void CM_AddFacetBevels( cfacet_t *facet )
{

	int		i, j, k, l;
	int		axis, dir, order, flipped;
	float		plane[4], d, newplane[4];
	vec3_t		mins, maxs, vec, vec2;
	cwinding_t	*w, *w2;

	Vector4Copy( planes[facet->surfacePlane].plane, plane );

	w = CM_BaseWindingForPlane( plane, plane[3] );

	for( j = 0; j < facet->numBorders && w; j++ )
	{
		if( facet->borderPlanes[j] == facet->surfacePlane )
			continue;
		Vector4Copy( planes[facet->borderPlanes[j]].plane, plane );

		if( !facet->borderInward[j] )
		{
			VectorNegate( plane, plane );
			plane[3] = -plane[3];
		}
		CM_ChopWindingInPlace( &w, plane, plane[3], ON_EPSILON );
	}

	if( !w ) return;

	CM_WindingBounds( w, mins, maxs );

	//
	// add the axial planes
	//
	order = 0;
	for( axis = 0; axis < 3; axis++ )
	{
		for( dir = -1; dir <= 1; dir += 2, order++ )
		{
			VectorClear( plane );
			plane[axis] = dir;

			if( dir == 1 ) plane[3] = maxs[axis];
			else plane[3] = -mins[axis];

			// if it's the surface plane
			if( CM_PlaneEqual( &planes[facet->surfacePlane], plane, &flipped ))
				continue;

			// see if the plane is allready present
			for( i = 0; i < facet->numBorders; i++ )
			{
				if( CM_PlaneEqual( &planes[facet->borderPlanes[i]], plane, &flipped ))
					break;
			}

			if( i == facet->numBorders )
			{
				if( facet->numBorders > MAX_FACET_BEVELS )
					MsgDev( D_ERROR, "CM_AddFacetBevels: too many bevels\n" );

				facet->borderPlanes[facet->numBorders] = CM_FindPlane2(plane, &flipped);
				facet->borderNoAdjust[facet->numBorders] = 0;
				facet->borderInward[facet->numBorders] = flipped;
				facet->numBorders++;
			}
		}
	}

	//
	// add the edge bevels
	//

	// test the non-axial plane edges
	for( j = 0; j < w->numpoints; j++ )
	{
		k = (j + 1) % w->numpoints;
		VectorSubtract( w->p[j], w->p[k], vec );

		// if it's a degenerate edge
		if( VectorNormalizeLength( vec ) < 0.5f )
			continue;

		CM_SnapVector( vec );
		for( k = 0; k < 3; k++ )
		{
			if( vec[k] == -1 || vec[k] == 1 )
				break; // axial
		}

		if( k < 3 ) continue; // only test non-axial edges

		// try the six possible slanted axials from this edge
		for( axis = 0; axis < 3; axis++ )
		{
			for( dir = -1; dir <= 1; dir += 2 )
			{
				// construct a plane
				VectorClear( vec2 );
				vec2[axis] = dir;
				CrossProduct( vec, vec2, plane );
				if( VectorNormalizeLength( plane ) < 0.5f )
					continue;
				plane[3] = DotProduct( w->p[j], plane );

				// if all the points of the facet winding are
				// behind this plane, it is a proper edge bevel
				for( l = 0; l < w->numpoints; l++ )
				{
					d = DotProduct( w->p[l], plane ) - plane[3];
					if( d > ON_EPSILON ) break; // point in front
				}

				if( l < w->numpoints ) continue;

				// if it's the surface plane
				if( CM_PlaneEqual( &planes[facet->surfacePlane], plane, &flipped ))
					continue;

				// see if the plane is allready present
				for( i = 0; i < facet->numBorders; i++ )
				{
					if( CM_PlaneEqual( &planes[facet->borderPlanes[i]], plane, &flipped ))
						break;
				}

				if( i == facet->numBorders )
				{
					if( facet->numBorders > MAX_FACET_BEVELS )
						MsgDev( D_ERROR, "CM_AddFacetBevels: too many bevels\n" );
					facet->borderPlanes[facet->numBorders] = CM_FindPlane2( plane, &flipped );

					for( k = 0; k < facet->numBorders; k++ )
					{
						if( facet->borderPlanes[facet->numBorders] == facet->borderPlanes[k] )
							MsgDev( D_WARN, "CM_AddFacetBevels: bevel plane already used\n" );
					}

					facet->borderNoAdjust[facet->numBorders] = 0;
					facet->borderInward[facet->numBorders] = flipped;

					w2 = CM_CopyWinding( w );
					Vector4Copy( planes[facet->borderPlanes[facet->numBorders]].plane, newplane );

					if( !facet->borderInward[facet->numBorders] )
					{
						VectorNegate( newplane, newplane );
						newplane[3] = -newplane[3];
					}

					CM_ChopWindingInPlace( &w2, newplane, newplane[3], ON_EPSILON );

					if( !w2 )
					{
						MsgDev( D_WARN, "CM_AddFacetBevels: invalid bevel\n" );
						continue;
					}
					else CM_FreeWinding( w2 );

					facet->numBorders++;
					// already got a bevel
				}
			}
		}
	}
	CM_FreeWinding( w );

	// add opposite plane
	facet->borderPlanes[facet->numBorders] = facet->surfacePlane;
	facet->borderNoAdjust[facet->numBorders] = 0;
	facet->borderInward[facet->numBorders] = true;
	facet->numBorders++;
}


/*
=====================
CM_GenerateFacetFor3Points
=====================
*/
bool CM_GenerateFacetFor3Points( cfacet_t * facet, const vec3_t p1, const vec3_t p2, const vec3_t p3 )
{
	vec4_t	plane;

	// if we can't generate a valid plane for the points, ignore the facet
	if( facet->surfacePlane == -1 )
	{
		facet->numBorders = 0;
		return false;
	}

	Vector4Copy( planes[facet->surfacePlane].plane, plane );
	facet->numBorders = 3;
	facet->borderNoAdjust[0] = false;
	facet->borderNoAdjust[1] = false;
	facet->borderNoAdjust[2] = false;

	facet->borderPlanes[0] = CM_GenerateBoundaryForPoints( plane, p1, p2 );
	facet->borderPlanes[1] = CM_GenerateBoundaryForPoints( plane, p2, p3 );
	facet->borderPlanes[2] = CM_GenerateBoundaryForPoints( plane, p3, p1 );

	return true;
}

/*
=====================
CM_GenerateFacetFor4Points
=====================
*/
bool CM_GenerateFacetFor4Points( cfacet_t *facet, const vec3_t p1, const vec3_t p2, const vec3_t p3, const vec3_t p4 )
{
	float	dist;
	vec4_t	plane;

	// if we can't generate a valid plane for the points, ignore the facet
	if( facet->surfacePlane == -1 )
	{
		facet->numBorders = 0;
		return false;
	}

	Vector4Copy( planes[facet->surfacePlane].plane, plane );

	// if the fourth point is also on the plane, we can make a quad facet
	dist = DotProduct( p4, plane ) - plane[3];
	if( fabs( dist ) > ON_EPSILON )
	{
		facet->numBorders = 0;
		return false;
	}

	facet->numBorders = 4;
	facet->borderNoAdjust[0] = false;
	facet->borderNoAdjust[1] = false;
	facet->borderNoAdjust[2] = false;
	facet->borderNoAdjust[3] = false;
	facet->borderPlanes[0] = CM_GenerateBoundaryForPoints( plane, p1, p2 );
	facet->borderPlanes[1] = CM_GenerateBoundaryForPoints( plane, p2, p3 );
	facet->borderPlanes[2] = CM_GenerateBoundaryForPoints( plane, p3, p4 );
	facet->borderPlanes[3] = CM_GenerateBoundaryForPoints( plane, p4, p1 );

	return true;
}

/*
==================
CM_SurfaceCollideFromTriangleSoup
==================
*/
static void CM_SurfaceCollideFromTriangleSoup( cTriangleSoup_t *triSoup, cSurfaceCollide_t *sc )
{
	float	*p1, *p2, *p3;
	int	i, i1, i2, i3;
	cfacet_t	*facet;

	numPlanes = 0;
	numFacets = 0;

#ifdef USE_HASHING
	// initialize hash table
	Mem_Set( planeHashTable, 0, sizeof( planeHashTable ));
#endif

	// find the planes for each triangle of the grid
	for( i = 0; i < triSoup->numTriangles; i++ )
	{
		p1 = triSoup->points[i][0];
		p2 = triSoup->points[i][1];
		p3 = triSoup->points[i][2];

		triSoup->trianglePlanes[i] = CM_FindPlane( p1, p2, p3 );
	}

	// create the borders for each triangle
	for( i = 0; i < triSoup->numTriangles; i++ )
	{
		facet = &facets[numFacets];
		Mem_Set( facet, 0, sizeof( *facet ));

		i1 = triSoup->indexes[i*3+0];
		i2 = triSoup->indexes[i*3+1];
		i3 = triSoup->indexes[i*3+2];

		p1 = triSoup->points[i][0];
		p2 = triSoup->points[i][1];
		p3 = triSoup->points[i][2];

		facet->surfacePlane = triSoup->trianglePlanes[i];

		// try and make a quad out of two triangles
#if 0
		if( i != triSoup->numTriangles - 1 )
		{
			int	i4, i5, i6;
			float	*p4;

			i4 = triSoup->indexes[i*3+3];
			i5 = triSoup->indexes[i*3+4];
			i6 = triSoup->indexes[i*3+5];

			if( i4 == i3 && i5 == i2 )
			{
				p4 = triSoup->points[i][5]; // vertex at i6

				if(CM_GenerateFacetFor4Points( facet, p1, p2, p4, p3 ))
				{
					CM_SetBorderInward( facet, triSoup, i, 0 );

					if( CM_ValidateFacet( facet ))
					{
						CM_AddFacetBevels( facet );
						numFacets++;

						i++; // skip next tri
						continue;
					}
				}
			}
		}
#endif

		if( CM_GenerateFacetFor3Points( facet, p1, p2, p3 ))
		{
			CM_SetBorderInward( facet, triSoup, i, 0 );

			if( CM_ValidateFacet( facet ))
			{
				CM_AddFacetBevels( facet );
				numFacets++;
			}
		}
	}

	// copy the results out
	sc->numPlanes = numPlanes;
	sc->planes = Mem_Alloc( cms.mempool, numPlanes * sizeof( *sc->planes ));
	Mem_Copy( sc->planes, planes, numPlanes * sizeof( *sc->planes ));

	sc->numFacets = numFacets;
	sc->facets = Mem_Alloc( cms.mempool, numFacets * sizeof( *sc->facets ));
	Mem_Copy( sc->facets, facets, numFacets * sizeof( *sc->facets ));
}


/*
===================
CM_GenerateTriangleSoupCollide

Creates an internal structure that will be used to perform
collision detection with a triangle soup mesh.

Points is packed as concatenated rows.
===================
*/
cSurfaceCollide_t *CM_GenerateTriangleSoupCollide( int numVertexes, vec3_t *vertexes, int numIndexes, int *indexes )
{
	cSurfaceCollide_t		*sc;
	static cTriangleSoup_t	triSoup;
	int			i, j;

	if( numVertexes <= 2 || !vertexes || numIndexes <= 2 || !indexes )
		Host_Error( "CM_GenerateTriangleSoupCollide: bad params: ( %i, %i )\n", numVertexes, numIndexes );

	if( numIndexes > SHADER_MAX_INDEXES )
		Host_Error( "CM_GenerateTriangleSoupCollide: source is > SHADER_MAX_TRIANGLES\n" );

	// build a triangle soup
	triSoup.numTriangles = numIndexes / 3;
	for( i = 0; i < triSoup.numTriangles; i++ )
	{
		for( j = 0; j < 3; j++ )
		{
			triSoup.indexes[i*3+j] = indexes[i*3+j];
			VectorCopy(vertexes[indexes[i*3+j]], triSoup.points[i][j]);
		}
	}

	sc = Mem_Alloc( cms.mempool, sizeof( *sc ));
	ClearBounds( sc->bounds[0], sc->bounds[1] );

	for( i = 0; i < triSoup.numTriangles; i++ )
	{
		for( j = 0; j < 3; j++ )
			AddPointToBounds( triSoup.points[i][j], sc->bounds[0], sc->bounds[1] );
	}

	// generate a bsp tree for the surface
	CM_SurfaceCollideFromTriangleSoup( &triSoup, sc );

	// expand by one unit for epsilon purposes
	sc->bounds[0][0] -= 1;
	sc->bounds[0][1] -= 1;
	sc->bounds[0][2] -= 1;

	sc->bounds[1][0] += 1;
	sc->bounds[1][1] += 1;
	sc->bounds[1][2] += 1;

	MsgDev( D_INFO, "CM_GenerateTriangleSoupCollide: %i planes %i facets\n", sc->numPlanes, sc->numFacets );

	return sc;
}
