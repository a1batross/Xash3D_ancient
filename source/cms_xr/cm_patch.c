//=======================================================================
//			Copyright XashXT Group 2009 ©
//			cm_patch.c - curves collision
//=======================================================================

#include "cm_local.h"
#include "mathlib.h"

int			c_totalPatchBlocks;
vec3_t			debugBlockPoints[4];
const cSurfaceCollide_t	*debugSurfaceCollide;
const cfacet_t		*debugFacet;
bool			debugBlock;

/*
=================
CM_ClearLevelPatches
=================
*/
void CM_ClearLevelPatches( void )
{
	debugSurfaceCollide = NULL;
	debugFacet = NULL;
}

/*
================================================================================

GRID SUBDIVISION

================================================================================
*/

/*
=================
CM_NeedsSubdivision

Returns true if the given quadratic curve is not flat enough for our
collision detection purposes
=================
*/
static bool CM_NeedsSubdivision( vec3_t a, vec3_t b, vec3_t c )
{
	vec3_t	cmid, lmid;
	vec3_t	delta;
	float	dist;
	int	i;

	// calculate the linear midpoint
	for( i = 0; i < 3; i++ )
		lmid[i] = 0.5 * (a[i] + c[i]);

	// calculate the exact curve midpoint
	for( i = 0; i < 3; i++ )
		cmid[i] = 0.5f * (0.5f * (a[i] + b[i]) + 0.5f * (b[i] + c[i]));

	// see if the curve is far enough away from the linear mid
	VectorSubtract( cmid, lmid, delta );
	dist = VectorLength( delta );

	return dist >= SUBDIVIDE_DISTANCE;
}

/*
===============
CM_Subdivide

a, b, and c are control points.
the subdivided sequence will be: a, out1, out2, out3, c
===============
*/
static void CM_Subdivide( vec3_t a, vec3_t b, vec3_t c, vec3_t out1, vec3_t out2, vec3_t out3 )
{
	int	i;

	for( i = 0; i < 3; i++ )
	{
		out1[i] = 0.5f * (a[i] + b[i]);
		out3[i] = 0.5f * (b[i] + c[i]);
		out2[i] = 0.5f * (out1[i] + out3[i]);
	}
}

/*
=================
CM_TransposeGrid

Swaps the rows and columns in place
=================
*/
static void CM_TransposeGrid( cgrid_t *grid )
{
	int	i, j, l;
	vec3_t	temp;
	bool	tempWrap;

	if( grid->width > grid->height )
	{
		for( i = 0; i < grid->height; i++ )
		{
			for( j = i + 1; j < grid->width; j++ )
			{
				if( j < grid->height )
				{
					// swap the value
					VectorCopy( grid->points[i][j], temp );
					VectorCopy( grid->points[j][i], grid->points[i][j] );
					VectorCopy( temp, grid->points[j][i] );
				}
				else
				{
					// just copy
					VectorCopy( grid->points[j][i], grid->points[i][j] );
				}
			}
		}
	}
	else
	{
		for( i = 0; i < grid->width; i++ )
		{
			for( j = i + 1; j < grid->height; j++ )
			{
				if( j < grid->width )
				{
					// swap the value
					VectorCopy( grid->points[j][i], temp );
					VectorCopy( grid->points[i][j], grid->points[j][i] );
					VectorCopy( temp, grid->points[i][j] );
				}
				else
				{
					// just copy
					VectorCopy( grid->points[i][j], grid->points[j][i] );
				}
			}
		}
	}

	l = grid->width;
	grid->width = grid->height;
	grid->height = l;

	tempWrap = grid->wrapWidth;
	grid->wrapWidth = grid->wrapHeight;
	grid->wrapHeight = tempWrap;
}

/*
===================
CM_SetGridWrapWidth

If the left and right columns are exactly equal, set grid->wrapWidth true
===================
*/
static void CM_SetGridWrapWidth( cgrid_t *grid )
{
	int	i, j;
	float	d;

	for( i = 0; i < grid->height; i++ )
	{
		for( j = 0; j < 3; j++ )
		{
			d = grid->points[0][i][j] - grid->points[grid->width - 1][i][j];
			if( d < -WRAP_POINT_EPSILON || d > WRAP_POINT_EPSILON )
				break;
		}
		if( j != 3 ) break;
	}

	if( i == grid->height )
		grid->wrapWidth = true;
	else grid->wrapWidth = false;
}

/*
=================
CM_SubdivideGridColumns

Adds columns as necessary to the grid until
all the aproximating points are within SUBDIVIDE_DISTANCE
from the true curve
=================
*/
static void CM_SubdivideGridColumns( cgrid_t *grid )
{
	int	i, j, k;

	for( i = 0; i < grid->width - 2; )
	{
		// grid->points[i][x] is an interpolating control point
		// grid->points[i+1][x] is an aproximating control point
		// grid->points[i+2][x] is an interpolating control point

		// first see if we can collapse the aproximating collumn away
		for( j = 0; j < grid->height; j++ )
		{
			if( CM_NeedsSubdivision( grid->points[i][j], grid->points[i + 1][j], grid->points[i + 2][j] ))
				break;
		}

		if( j == grid->height )
		{
			// all of the points were close enough to the linear midpoints
			// that we can collapse the entire column away
			for( j = 0; j < grid->height; j++ )
			{
				// remove the column
				for( k = i + 2; k < grid->width; k++ )
					VectorCopy( grid->points[k][j], grid->points[k - 1][j] );
			}
			grid->width--;

			// go to the next curve segment
			i++;
			continue;
		}

		// we need to subdivide the curve
		for( j = 0; j < grid->height; j++ )
		{
			vec3_t	prev, mid, next;

			// save the control points now
			VectorCopy( grid->points[i][j], prev );
			VectorCopy( grid->points[i + 1][j], mid );
			VectorCopy( grid->points[i + 2][j], next );

			// make room for two additional columns in the grid
			// columns i+1 will be replaced, column i+2 will become i+4
			// i+1, i+2, and i+3 will be generated
			for( k = grid->width - 1; k > i + 1; k-- )
				VectorCopy( grid->points[k][j], grid->points[k + 2][j] );

			// generate the subdivided points
			CM_Subdivide( prev, mid, next, grid->points[i+1][j], grid->points[i+2][j], grid->points[i+3][j] );
		}

		grid->width += 2;

		// the new aproximating point at i+1 may need to be removed
		// or subdivided farther, so don't advance i
	}
}

/*
======================
CM_ComparePoints
======================
*/
static bool CM_ComparePoints( float *a, float *b )
{
	float	d;

	d = a[0] - b[0];
	if( d < -ON_EPSILON || d > ON_EPSILON )
		return false;

	d = a[1] - b[1];
	if( d < -ON_EPSILON || d > ON_EPSILON )
		return false;

	d = a[2] - b[2];
	if( d < -ON_EPSILON || d > ON_EPSILON )
		return false;

	return true;
}

/*
=================
CM_RemoveDegenerateColumns

If there are any identical columns, remove them
=================
*/
static void CM_RemoveDegenerateColumns( cgrid_t *grid )
{
	int	i, j, k;

	for( i = 0; i < grid->width - 1; i++ )
	{
		for( j = 0; j < grid->height; j++ )
		{
			if(!CM_ComparePoints( grid->points[i][j], grid->points[i + 1][j] ))
				break;
		}

		if( j != grid->height )
			continue;	// not degenerate

		for( j = 0; j < grid->height; j++ )
		{
			// remove the column
			for( k = i + 2; k < grid->width; k++ )
				VectorCopy( grid->points[k][j], grid->points[k-1][j] );
		}
		grid->width--;

		// check against the next column
		i--;
	}
}

/*
================================================================================

PATCH COLLIDE GENERATION

================================================================================
*/

static int	numPlanes;
static cmplane_t	planes[MAX_PATCH_PLANES];
static int	numFacets;
static cfacet_t	facets[MAX_PATCH_PLANES];	// maybe MAX_FACETS ??

/*
==================
CM_FindPlane2
==================
*/
static int CM_FindPlane2( float plane[4], int *flipped )
{
	int	i;

	// see if the points are close enough to an existing plane
	for( i = 0; i < numPlanes; i++ )
	{
		if( CM_PlaneEqual( &planes[i], plane, flipped ))
			return i;
	}

	// add a new plane
	if( numPlanes == MAX_PATCH_PLANES )
		Host_Error( "CM_FindPlane2: MAX_PATCH_PLANES limit exceeded\n" );

	Vector4Copy( plane, planes[numPlanes].plane );
	planes[numPlanes].signbits = SignbitsForPlane( plane );

	numPlanes++;
	*flipped = false;

	return numPlanes - 1;
}

/*
==================
CM_FindPlane
==================
*/
static int CM_FindPlane( float *p1, float *p2, float *p3 )
{
	float	d, plane[4];
	int	i;

	if( !CM_PlaneFromPoints( plane, p1, p2, p3 ))
		return -1;

	// see if the points are close enough to an existing plane
	for( i = 0; i < numPlanes; i++ )
	{
		if( DotProduct( plane, planes[i].plane ) < 0.0f )
			continue;	// allow backwards planes?

		d = DotProduct( p1, planes[i].plane ) - planes[i].plane[3];
		if( d < -PLANE_TRI_EPSILON || d > PLANE_TRI_EPSILON )
			continue;

		d = DotProduct( p2, planes[i].plane) - planes[i].plane[3];
		if( d < -PLANE_TRI_EPSILON || d > PLANE_TRI_EPSILON )
			continue;

		d = DotProduct( p3, planes[i].plane) - planes[i].plane[3];
		if( d < -PLANE_TRI_EPSILON || d > PLANE_TRI_EPSILON )
			continue;

		// found it
		return i;
	}

	// add a new plane
	if( numPlanes == MAX_PATCH_PLANES )
		Host_Error( "MAX_PATCH_PLANES limit exceeded\n" );

	Vector4Copy(plane, planes[numPlanes].plane);
	planes[numPlanes].signbits = SignbitsForPlane(plane);
	numPlanes++;

	return numPlanes - 1;
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
CM_GridPlane
==================
*/
static int CM_GridPlane( int gridPlanes[MAX_GRID_SIZE][MAX_GRID_SIZE][2], int i, int j, int tri )
{
	int	p;

	p = gridPlanes[i][j][tri];
	if( p != -1 )
		return p;

	p = gridPlanes[i][j][!tri];
	if( p != -1 )
		return p;

	// should never happen
	MsgDev( D_WARN, "CM_GridPlane: unresolvable\n" );
	return -1;
}

/*
==================
CM_EdgePlaneNum
==================
*/
static int CM_EdgePlaneNum( cgrid_t *grid, int gridPlanes[MAX_GRID_SIZE][MAX_GRID_SIZE][2], int i, int j, int k )
{
	float	*p1, *p2;
	vec3_t	up;
	int	p;

	switch( k )
	{
	case 0:	// top border
		p1 = grid->points[i][j];
		p2 = grid->points[i+1][j];
		p = CM_GridPlane(gridPlanes, i, j, 0 );
		VectorMA( p1, 4, planes[p].plane, up );
		return CM_FindPlane( p1, p2, up );
	case 2:	// bottom border
		p1 = grid->points[i][j+1];
		p2 = grid->points[i+1][j+1];
		p = CM_GridPlane( gridPlanes, i, j, 1 );
		VectorMA( p1, 4, planes[p].plane, up );
		return CM_FindPlane( p2, p1, up );
	case 3:	// left border
		p1 = grid->points[i][j];
		p2 = grid->points[i][j+1];
		p = CM_GridPlane( gridPlanes, i, j, 1 );
		VectorMA( p1, 4, planes[p].plane, up );
		return CM_FindPlane( p2, p1, up );
	case 1:	// right border
		p1 = grid->points[i+1][j];
		p2 = grid->points[i+1][j+1];
		p = CM_GridPlane( gridPlanes, i, j, 0 );
		VectorMA( p1, 4, planes[p].plane, up );
		return CM_FindPlane( p1, p2, up );
	case 4:	// diagonal out of triangle 0
		p1 = grid->points[i+1][j+1];
		p2 = grid->points[i][j];
		p = CM_GridPlane( gridPlanes, i, j, 0 );
		VectorMA( p1, 4, planes[p].plane, up );
		return CM_FindPlane( p1, p2, up );
	case 5:	// diagonal out of triangle 1
		p1 = grid->points[i][j];
		p2 = grid->points[i+1][j+1];
		p = CM_GridPlane( gridPlanes, i, j, 1 );
		VectorMA( p1, 4, planes[p].plane, up );
		return CM_FindPlane( p1, p2, up );
	}

	Host_Error( "CM_EdgePlaneNum: bad planenum %i\n", k );
	return -1;
}

/*
===================
CM_SetBorderInward
===================
*/
static void CM_SetBorderInward( cfacet_t *facet, cgrid_t *grid, int gridPlanes[MAX_GRID_SIZE][MAX_GRID_SIZE][2], int i, int j, int which )
{
	int	k, l;
	float	*points[4];
	int	numPoints;

	switch( which )
	{
	case -1:
		points[0] = grid->points[i][j];
		points[1] = grid->points[i+1][j];
		points[2] = grid->points[i+1][j+1];
		points[3] = grid->points[i][j+1];
		numPoints = 4;
		break;
	case 0:
		points[0] = grid->points[i][j];
		points[1] = grid->points[i+1][j];
		points[2] = grid->points[i+1][j+1];
		numPoints = 3;
		break;
	case 1:
		points[0] = grid->points[i+1][j+1];
		points[1] = grid->points[i][j+1];
		points[2] = grid->points[i][j];
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

		if( front && !back )
			facet->borderInward[k] = true;
		else if( back && !front )
			facet->borderInward[k] = false;
		else if( !front && !back )
			facet->borderPlanes[k] = -1;	// flat side border
		else
		{
			// bisecting side border
			MsgDev( D_WARN, "CM_SetBorderInward: mixed plane sides\n" );
			facet->borderInward[k] = false;
			if( !debugBlock )
			{
				debugBlock = true;
				VectorCopy( grid->points[i][j], debugBlockPoints[0] );
				VectorCopy( grid->points[i+1][j], debugBlockPoints[1] );
				VectorCopy( grid->points[i+1][j+1], debugBlockPoints[2] );
				VectorCopy( grid->points[i][j+1], debugBlockPoints[3] );
			}
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

	if( !w ) return false;	// winding was completely chopped away

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
			VectorSubtract( vec3_origin, plane, plane );
			plane[3] = -plane[3];
		}
		CM_ChopWindingInPlace( &w, plane, plane[3], ON_EPSILON );
	}

	if( !w ) return;

	CM_WindingBounds( w, mins, maxs );

	// add the axial planes
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
				facet->borderPlanes[facet->numBorders] = CM_FindPlane2( plane, &flipped );
				facet->borderNoAdjust[facet->numBorders] = 0;
				facet->borderInward[facet->numBorders] = flipped;
				facet->numBorders++;
			}
		}
	}

	// add the edge bevels
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

				//if it's the surface plane
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
						cm.numInvalidBevels++;
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

typedef enum
{
	EN_TOP,
	EN_RIGHT,
	EN_BOTTOM,
	EN_LEFT
} edgeName_t;

/*
==================
CM_PatchCollideFromGrid
==================
*/
static void CM_SurfaceCollideFromGrid( cgrid_t *grid, cSurfaceCollide_t *sc )
{
	int	i, j;
	float	*p1, *p2, *p3;
	int	gridPlanes[MAX_GRID_SIZE][MAX_GRID_SIZE][2];
	cfacet_t	*facet;
	int	borders[4];
	int	noAdjust[4];

	numPlanes = 0;
	numFacets = 0;

	// find the planes for each triangle of the grid
	for( i = 0; i < grid->width - 1; i++ )
	{
		for( j = 0; j < grid->height - 1; j++ )
		{
			p1 = grid->points[i][j];
			p2 = grid->points[i+1][j];
			p3 = grid->points[i+1][j+1];
			gridPlanes[i][j][0] = CM_FindPlane( p1, p2, p3 );

			p1 = grid->points[i+1][j+1];
			p2 = grid->points[i][j+1];
			p3 = grid->points[i][j];
			gridPlanes[i][j][1] = CM_FindPlane( p1, p2, p3 );
		}
	}

	// create the borders for each facet
	for( i = 0; i < grid->width - 1; i++ )
	{
		for( j = 0; j < grid->height - 1; j++ )
		{
			borders[EN_TOP] = -1;
			if( j > 0 ) borders[EN_TOP] = gridPlanes[i][j-1][1];
			else if( grid->wrapHeight ) borders[EN_TOP] = gridPlanes[i][grid->height-2][1];
			noAdjust[EN_TOP] = (borders[EN_TOP] == gridPlanes[i][j][0]);
			if( borders[EN_TOP] == -1 || noAdjust[EN_TOP] )
				borders[EN_TOP] = CM_EdgePlaneNum( grid, gridPlanes, i, j, 0 );

			borders[EN_BOTTOM] = -1;
			if( j < grid->height - 2 ) borders[EN_BOTTOM] = gridPlanes[i][j+1][0];
			else if( grid->wrapHeight ) borders[EN_BOTTOM] = gridPlanes[i][0][0];
			noAdjust[EN_BOTTOM] = (borders[EN_BOTTOM] == gridPlanes[i][j][1]);
			if( borders[EN_BOTTOM] == -1 || noAdjust[EN_BOTTOM] )
				borders[EN_BOTTOM] = CM_EdgePlaneNum( grid, gridPlanes, i, j, 2 );

			borders[EN_LEFT] = -1;
			if( i > 0 ) borders[EN_LEFT] = gridPlanes[i - 1][j][0];
			else if( grid->wrapWidth ) borders[EN_LEFT] = gridPlanes[grid->width-2][j][0];
			noAdjust[EN_LEFT] = (borders[EN_LEFT] == gridPlanes[i][j][1]);
			if( borders[EN_LEFT] == -1 || noAdjust[EN_LEFT] )
				borders[EN_LEFT] = CM_EdgePlaneNum( grid, gridPlanes, i, j, 3 );

			borders[EN_RIGHT] = -1;
			if( i < grid->width - 2 ) borders[EN_RIGHT] = gridPlanes[i+1][j][1];
			else if( grid->wrapWidth ) borders[EN_RIGHT] = gridPlanes[0][j][1];
			noAdjust[EN_RIGHT] = (borders[EN_RIGHT] == gridPlanes[i][j][0]);
			if( borders[EN_RIGHT] == -1 || noAdjust[EN_RIGHT] )
				borders[EN_RIGHT] = CM_EdgePlaneNum( grid, gridPlanes, i, j, 1 );

			if( numFacets == MAX_FACETS )
				Host_Error( "MAX_FACETS limit exceeded\n" );

			facet = &facets[numFacets];
			Mem_Set( facet, 0, sizeof( *facet ));

			if( gridPlanes[i][j][0] == gridPlanes[i][j][1] )
			{
				if( gridPlanes[i][j][0] == -1 )
					continue;	// degenrate

				facet->surfacePlane = gridPlanes[i][j][0];
				facet->numBorders = 4;
				facet->borderPlanes[0] = borders[EN_TOP];
				facet->borderNoAdjust[0] = noAdjust[EN_TOP];
				facet->borderPlanes[1] = borders[EN_RIGHT];
				facet->borderNoAdjust[1] = noAdjust[EN_RIGHT];
				facet->borderPlanes[2] = borders[EN_BOTTOM];
				facet->borderNoAdjust[2] = noAdjust[EN_BOTTOM];
				facet->borderPlanes[3] = borders[EN_LEFT];
				facet->borderNoAdjust[3] = noAdjust[EN_LEFT];
				CM_SetBorderInward( facet, grid, gridPlanes, i, j, -1 );

				if( CM_ValidateFacet( facet ))
				{
					CM_AddFacetBevels( facet );
					numFacets++;
				}
			}
			else
			{
				// two seperate triangles
				facet->surfacePlane = gridPlanes[i][j][0];
				facet->numBorders = 3;
				facet->borderPlanes[0] = borders[EN_TOP];
				facet->borderNoAdjust[0] = noAdjust[EN_TOP];
				facet->borderPlanes[1] = borders[EN_RIGHT];
				facet->borderNoAdjust[1] = noAdjust[EN_RIGHT];
				facet->borderPlanes[2] = gridPlanes[i][j][1];
				if( facet->borderPlanes[2] == -1 )
				{
					facet->borderPlanes[2] = borders[EN_BOTTOM];
					if( facet->borderPlanes[2] == -1 )
						facet->borderPlanes[2] = CM_EdgePlaneNum( grid, gridPlanes, i, j, 4 );
				}

				CM_SetBorderInward( facet, grid, gridPlanes, i, j, 0 );

				if( CM_ValidateFacet( facet ))
				{
					CM_AddFacetBevels( facet );
					numFacets++;
				}

				if( numFacets == MAX_FACETS )
					Host_Error( "MAX_FACETS limit exceeded\n" );

				facet = &facets[numFacets];
				Mem_Set( facet, 0, sizeof( *facet ));

				facet->surfacePlane = gridPlanes[i][j][1];
				facet->numBorders = 3;
				facet->borderPlanes[0] = borders[EN_BOTTOM];
				facet->borderNoAdjust[0] = noAdjust[EN_BOTTOM];
				facet->borderPlanes[1] = borders[EN_LEFT];
				facet->borderNoAdjust[1] = noAdjust[EN_LEFT];
				facet->borderPlanes[2] = gridPlanes[i][j][0];
				if( facet->borderPlanes[2] == -1 )
				{
					facet->borderPlanes[2] = borders[EN_TOP];
					if( facet->borderPlanes[2] == -1 )
						facet->borderPlanes[2] = CM_EdgePlaneNum( grid, gridPlanes, i, j, 5 );
				}

				CM_SetBorderInward( facet, grid, gridPlanes, i, j, 1 );

				if( CM_ValidateFacet( facet ))
				{
					CM_AddFacetBevels( facet );
					numFacets++;
				}
			}
		}
	}

	// copy the results out
	sc->numPlanes = numPlanes;
	sc->numFacets = numFacets;
	sc->facets = Mem_Alloc( cms.mempool, numFacets * sizeof( *sc->facets ));
	Mem_Copy( sc->facets, facets, numFacets * sizeof( *sc->facets ));
	sc->planes = Mem_Alloc( cms.mempool, numPlanes * sizeof( *sc->planes ));
	Mem_Copy( sc->planes, planes, numPlanes * sizeof( *sc->planes ));
}


/*
===================
CM_GeneratePatchCollide

Creates an internal structure that will be used to perform
collision detection with a patch mesh.

Points is packed as concatenated rows.
===================
*/
cSurfaceCollide_t *CM_GeneratePatchCollide( int width, int height, vec3_t *points )
{
	cSurfaceCollide_t	*sc;
	static cgrid_t	grid;
	int		i, j;

	if( width <= 2 || height <= 2 || !points )
	{
		Host_Error( "CM_GeneratePatchCollide: bad params: (%i, %i)\n", width, height );
	}

	if( !( width & 1 ) || !( height & 1 ))
		Host_Error( "CM_GeneratePatchCollide: even sizes are invalid for quadratic meshes\n" );

	if( width > MAX_GRID_SIZE || height > MAX_GRID_SIZE )
		Host_Error( "CM_GeneratePatchCollide: source is > MAX_GRID_SIZE\n" );

	// build a grid
	grid.width = width;
	grid.height = height;
	grid.wrapWidth = false;
	grid.wrapHeight = false;

	for( i = 0; i < width; i++ )
	{
		for( j = 0; j < height; j++ )
			VectorCopy(points[j*width+i], grid.points[i][j]);
	}

	// subdivide the grid
	CM_SetGridWrapWidth( &grid );
	CM_SubdivideGridColumns( &grid );
	CM_RemoveDegenerateColumns( &grid );

	CM_TransposeGrid( &grid );

	CM_SetGridWrapWidth( &grid );
	CM_SubdivideGridColumns( &grid );
	CM_RemoveDegenerateColumns( &grid );

	// we now have a grid of points exactly on the curve
	// the aproximate surface defined by these points will be
	// collided against
	sc = Mem_Alloc( cms.mempool, sizeof( *sc ));
	ClearBounds( sc->bounds[0], sc->bounds[1] );

	for( i = 0; i < grid.width; i++ )
	{
		for( j = 0; j < grid.height; j++ )
			AddPointToBounds( grid.points[i][j], sc->bounds[0], sc->bounds[1] );
	}

	c_totalPatchBlocks += (grid.width - 1) * (grid.height - 1);

	// generate a bsp tree for the surface
	CM_SurfaceCollideFromGrid( &grid, sc );

	// expand by one unit for epsilon purposes
	sc->bounds[0][0] -= 1;
	sc->bounds[0][1] -= 1;
	sc->bounds[0][2] -= 1;

	sc->bounds[1][0] += 1;
	sc->bounds[1][1] += 1;
	sc->bounds[1][2] += 1;

	return sc;
}
