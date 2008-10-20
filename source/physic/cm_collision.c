//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         cm_collision.c - collision engine
//=======================================================================

#include "cm_local.h"
#include "matrixlib.h"
#include "const.h"

#define MAX_BRUSHFORBOX		16 // must be power of two

cvar_t *cm_impactnudge;
cvar_t *cm_startnudge;
cvar_t *cm_endnudge;
cvar_t *cm_enternudge;
cvar_t *cm_leavenudge;
cvar_t *cm_prefernudgedfraction;
static uint brushforbox_index = 0;

static cpointf_t polyf_points[MAX_BUILD_SIDES];
static cplanef_t polyf_planes[MAX_BUILD_SIDES + 2];
static cbrushf_t polyf_brush;
static cpointf_t polyf_pointsstart[MAX_BUILD_SIDES], polyf_pointsend[MAX_BUILD_SIDES];
static cplanef_t polyf_planesstart[MAX_BUILD_SIDES + 2], polyf_planesend[MAX_BUILD_SIDES + 2];
static cbrushf_t polyf_brushstart, polyf_brushend;
static cpointf_t brushforbox_point[MAX_BRUSHFORBOX*8];
static cplanef_t brushforbox_plane[MAX_BRUSHFORBOX*6];
static cbrushf_t brushforbox_brush[MAX_BRUSHFORBOX];
static cbrushf_t brushforpoint_brush[MAX_BRUSHFORBOX];

void CM_CollisionInit( void )
{
	cm_impactnudge = Cvar_Get( "cm_impactnudge", "0.03125", 0, "how much to back off from the impact" );
	cm_startnudge = Cvar_Get( "cm_startnudge", "0", 0, "how much to bias collision trace start" );
	cm_endnudge = Cvar_Get( "cm_endnudge", "0", 0, "how much to bias collision trace end" );
	cm_enternudge = Cvar_Get( "cm_enternudge", "0", 0, "how much to bias collision entry fraction" );
	cm_leavenudge = Cvar_Get( "cm_leavenudge", "0", 0, "how much to bias collision exit fraction" );
	cm_prefernudgedfraction = Cvar_Get( "cm_prefernudgedfraction", "1", 0, "whether to sort collision events by nudged fraction (1) or real fraction (0)" );
}

void CM_CollisionPrintBrushAsQHull( cbrushf_t *brush, const char *name )
{
	int	i;
	Msg( "3 %s\n%i\n", name, brush->numpoints );
	for( i = 0; i < brush->numpoints; i++ )
		Msg( "%f %f %f\n", brush->points[i].v[0], brush->points[i].v[1], brush->points[i].v[2] );

	// FIXME: optimize!
	Msg( "4\n%i\n", brush->numplanes );
	for( i = 0; i < brush->numplanes; i++ )
		Msg( "%f %f %f %f\n", brush->planes[i].normal[0], brush->planes[i].normal[1], brush->planes[i].normal[2], brush->planes[i].dist );
}

void CM_CollisionValidateBrush( cbrushf_t *brush )
{
	int	j, k, pointsoffplanes, pointonplanes, pointswithinsufficientplanes, printbrush;
	float	d;

	printbrush = false;
	if( !brush->numpoints )
	{
		MsgDev( D_ERROR, "CM_CollisionValidateBrush: brush with no points!\n" );
		printbrush = true;
	}

	if( brush->numplanes )
	{
		pointsoffplanes = 0;
		pointswithinsufficientplanes = 0;

		for( k = 0; k < brush->numplanes; k++ )
			if( DotProduct(brush->planes[k].normal, brush->planes[k].normal) < 0.0001f )
				Msg( "CM_CollisionValidateBrush: plane #%i (%f %f %f %f) is degenerate\n", k, brush->planes[k].normal[0], brush->planes[k].normal[1], brush->planes[k].normal[2], brush->planes[k].dist );

		for( j = 0; j < brush->numpoints; j++ )
		{
			pointonplanes = 0;
			for( k = 0; k < brush->numplanes; k++ )
			{
				d = DotProduct(brush->points[j].v, brush->planes[k].normal) - brush->planes[k].dist;
				if( d > COLLISION_PLANE_DIST_EPSILON )
				{
					Msg("CM_CollisionValidateBrush: point #%i (%f %f %f) infront of plane #%i (%f %f %f %f)\n", j, brush->points[j].v[0], brush->points[j].v[1], brush->points[j].v[2], k, brush->planes[k].normal[0], brush->planes[k].normal[1], brush->planes[k].normal[2], brush->planes[k].dist);
					printbrush = true;
				}
				if( fabs(d) > COLLISION_PLANE_DIST_EPSILON )
					pointsoffplanes++;
				else pointonplanes++;
			}
			if( pointonplanes < 3 ) pointswithinsufficientplanes++;
		}
		if( pointswithinsufficientplanes )
		{
			Msg( "CM_CollisionValidateBrush: some points have insufficient planes, every point must be on at least 3 planes to form a corner.\n");
			printbrush = true;
		}
		if( pointsoffplanes == 0 ) // all points are on all planes
		{
			Msg("CM_CollisionValidateBrush: all points lie on all planes (degenerate, no brush volume!)\n");
			printbrush = true;
		}
	}
	if( printbrush ) CM_CollisionPrintBrushAsQHull( brush, "unnamed" );
}

float nearestplanedist_float( const float *normal, const cpointf_t *points, int numpoints )
{
	float	dist, bestdist;

	if( !numpoints ) return 0;
	bestdist = DotProduct( points->v, normal );
	points++;
	while( --numpoints )
	{
		dist = DotProduct( points->v, normal );
		bestdist = min( bestdist, dist );
		points++;
	}
	return bestdist;
}

float furthestplanedist_float( const float *normal, const cpointf_t *points, int numpoints )
{
	float	dist, bestdist;

	if( !numpoints ) return 0;
	bestdist = DotProduct( points->v, normal );
	points++;
	while( --numpoints )
	{
		dist = DotProduct( points->v, normal );
		bestdist = max( bestdist, dist );
		points++;
	}
	return bestdist;
}


cbrushf_t *CM_CollisionNewBrushFromPlanes( byte *mempool, int numoriginalplanes, const cplanef_t *originalplanes, int supercontents )
{
	// TODO: planesbuf could be replaced by a remapping table
	int	j, k, m, w, xyzflags;
	int	numpointsbuf = 0, maxpointsbuf = MAX_BUILD_SIDES, numplanesbuf = 0;
	int	maxplanesbuf = MAX_BUILD_SIDES, numelementsbuf = 0, maxelementsbuf = MAX_BUILD_SIDES;
	double	maxdist;
	cbrushf_t	*brush;
	cpointf_t	pointsbuf[MAX_BUILD_SIDES];
	cplanef_t	planesbuf[MAX_BUILD_SIDES];
	int	elementsbuf[2048];
	int	polypointbuf[MAX_BUILD_SIDES];
	int	pmaxpoints = 128;
	int	pnumpoints;
	double	p[2][384];

#if 0
	// enable these if debugging to avoid seeing garbage in unused data
	memset(pointsbuf, 0, sizeof(pointsbuf));
	memset(planesbuf, 0, sizeof(planesbuf));
	memset(elementsbuf, 0, sizeof(elementsbuf));
	memset(polypointbuf, 0, sizeof(polypointbuf));
	memset(p, 0, sizeof(p));
#endif

	// figure out how large a bounding box we need to properly compute this brush
	maxdist = 0;
	for( j = 0; j < numoriginalplanes; j++ ) maxdist = max( maxdist, fabs(originalplanes[j].dist));

	// now make it large enough to enclose the entire brush, and round it off to a reasonable multiple of 1024
	maxdist = floor( maxdist * (4.0 / 1024.0) + 2) * 1024.0;

	// construct a collision brush (points, planes, and renderable mesh) from
	// a set of planes, this also optimizes out any unnecessary planes (ones
	// whose polygon is clipped away by the other planes)
	for( j = 0; j < numoriginalplanes; j++ )
	{
		// add the plane uniquely (no duplicates)
		for( k = 0; k < numplanesbuf; k++ )
			if(VectorCompare( planesbuf[k].normal, originalplanes[j].normal) && planesbuf[k].dist == originalplanes[j].dist )
				break;
		// if the plane is a duplicate, skip it
		if( k < numplanesbuf ) continue;
		// check if there are too many and skip the brush
		if( numplanesbuf >= maxplanesbuf )
		{
			MsgDev( D_ERROR, "CM_CollisionNewBrushFromPlanes: failed to build collision brush: too many planes for buffer\n" );
			return NULL;
		}

		// add the new plane
		VectorCopy( originalplanes[j].normal, planesbuf[numplanesbuf].normal );
		planesbuf[numplanesbuf].dist = originalplanes[j].dist;
		planesbuf[numplanesbuf].surfaceflags = originalplanes[j].surfaceflags;
		planesbuf[numplanesbuf].surface = originalplanes[j].surface;
		numplanesbuf++;

		// create a large polygon from the plane
		w = 0;
		PolygonD_QuadForPlane( p[w], originalplanes[j].normal[0], originalplanes[j].normal[1], originalplanes[j].normal[2], originalplanes[j].dist, maxdist );
		pnumpoints = 4;

		// clip it by all other planes
		for( k = 0; k < numoriginalplanes && pnumpoints >= 3 && pnumpoints <= pmaxpoints; k++ )
		{
			// skip the plane this polygon
			// (nothing happens if it is processed, this is just an optimization)
			if( k != j )
			{
				// we want to keep the inside of the brush plane so we flip
				// the cutting plane
				PolygonD_Divide( pnumpoints, p[w], -originalplanes[k].normal[0], -originalplanes[k].normal[1], -originalplanes[k].normal[2], -originalplanes[k].dist, COLLISION_PLANE_DIST_EPSILON, pmaxpoints, p[!w], &pnumpoints, 0, NULL, NULL, NULL );
				w = !w;
			}
		}

		// if nothing is left, skip it
		if( pnumpoints < 3 ) continue;

		for( k = 0; k < pnumpoints; k++ )
		{
			int	l, m;

			m = 0;
			for( l = 0; l < numoriginalplanes; l++ )
				if( fabs(DotProduct( &p[w][k*3], originalplanes[l].normal) - originalplanes[l].dist) < COLLISION_PLANE_DIST_EPSILON )
					m++;
			if( m < 3 ) break;
		}
		if( k < pnumpoints )
			MsgDev( D_WARN, "CM_CollisionNewBrushFromPlanes: polygon point does not lie on at least 3 planes\n" );

		// check if there are too many polygon vertices for buffer
		if( pnumpoints > pmaxpoints )
		{
			MsgDev( D_ERROR, "CM_CollisionNewBrushFromPlanes: failed to build collision brush: too many points for buffer\n" );
			return NULL;
		}

		// check if there are too many triangle elements for buffer
		if( numelementsbuf + (pnumpoints - 2) * 3 > maxelementsbuf )
		{
			MsgDev( D_ERROR, "CM_CollisionNewBrushFromPlanes: failed to build collision brush: too many triangle elements for buffer\n" );
			return NULL;
		}

		for( k = 0; k < pnumpoints; k++ )
		{
			float	v[3];

			// downgrade to float precision before comparing
			VectorCopy(&p[w][k*3], v);

			// check if there is already a matching point (no duplicates)
			for( m = 0; m < numpointsbuf; m++ )
				if( VectorDistance2( v, pointsbuf[m].v) < COLLISION_SNAP2 )
					break;

			// if there is no match, add a new one
			if( m == numpointsbuf )
			{
				// check if there are too many and skip the brush
				if( numpointsbuf >= maxpointsbuf )
				{
					MsgDev( D_ERROR, "CM_CollisionNewBrushFromPlanes: failed to build collision brush: too many points for buffer\n" );
					return NULL;
				}
				// add the new one
				VectorCopy(&p[w][k*3], pointsbuf[numpointsbuf].v);
				numpointsbuf++;
			}
			// store the index into a buffer
			polypointbuf[k] = m;
		}

		// add the triangles for the polygon
		// (this particular code makes a triangle fan)
		for( k = 0; k < pnumpoints - 2; k++ )
		{
			elementsbuf[numelementsbuf++] = polypointbuf[0];
			elementsbuf[numelementsbuf++] = polypointbuf[k + 1];
			elementsbuf[numelementsbuf++] = polypointbuf[k + 2];
		}
	}

	// if nothing is left, there's nothing to allocate
	if( numplanesbuf < 4 )
	{
		MsgDev( D_ERROR, "CM_CollisionNewBrushFromPlanes: failed to build collision brush: %i triangles, %i planes (input was %i planes), %i vertices\n", numelementsbuf / 3, numplanesbuf, numoriginalplanes, numpointsbuf );
		return NULL;
	}

	// if no triangles or points could be constructed, then this routine failed but the brush is not discarded
	if( numelementsbuf < 12 || numpointsbuf < 4 )
		MsgDev( D_WARN, "CM_CollisionNewBrushFromPlanes: unable to rebuild triangles/points for collision brush: %i triangles, %i planes (input was %i planes), %i vertices\n", numelementsbuf / 3, numplanesbuf, numoriginalplanes, numpointsbuf );

	// validate plane distances
	for( j = 0; j < numplanesbuf; j++ )
	{
		float d = furthestplanedist_float( planesbuf[j].normal, pointsbuf, numpointsbuf );
		if( fabs(planesbuf[j].dist - d) > COLLISION_PLANE_DIST_EPSILON)
			MsgDev( D_NOTE, "plane %f %f %f %f mismatches dist %f\n", planesbuf[j].normal[0], planesbuf[j].normal[1], planesbuf[j].normal[2], planesbuf[j].dist, d );
	}

	// allocate the brush and copy to it
	brush = (cbrushf_t *)Mem_Alloc( mempool, sizeof(cbrushf_t) + sizeof(cpointf_t) * numpointsbuf + sizeof(cplanef_t) * numplanesbuf + sizeof(int) * numelementsbuf );
	brush->contents = supercontents;
	brush->numplanes = numplanesbuf;
	brush->numpoints = numpointsbuf;
	brush->numtriangles = numelementsbuf / 3;
	brush->planes = (cplanef_t *)(brush + 1);
	brush->points = (cpointf_t *)(brush->planes + brush->numplanes);
	brush->elements = (int *)(brush->points + brush->numpoints);

	for( j = 0; j < brush->numpoints; j++ )
	{
		brush->points[j].v[0] = pointsbuf[j].v[0];
		brush->points[j].v[1] = pointsbuf[j].v[1];
		brush->points[j].v[2] = pointsbuf[j].v[2];
	}
	for( j = 0; j < brush->numplanes; j++ )
	{
		brush->planes[j].normal[0] = planesbuf[j].normal[0];
		brush->planes[j].normal[1] = planesbuf[j].normal[1];
		brush->planes[j].normal[2] = planesbuf[j].normal[2];
		brush->planes[j].dist = planesbuf[j].dist;
		brush->planes[j].surfaceflags = planesbuf[j].surfaceflags;
		brush->planes[j].surface = planesbuf[j].surface;
	}
	for( j = 0; j < brush->numtriangles * 3; j++ )
		brush->elements[j] = elementsbuf[j];

	xyzflags = 0;
	VectorClear( brush->mins );
	VectorClear( brush->maxs );
	for( j = 0; j < min(6, numoriginalplanes); j++ )
	{
		if( originalplanes[j].normal[0] ==  1 )
		{
			xyzflags |=  1;
			brush->maxs[0] = originalplanes[j].dist;
		}
		else if( originalplanes[j].normal[0] == -1 )
		{
			xyzflags |=  2;
			brush->mins[0] = -originalplanes[j].dist;
		}
		else if( originalplanes[j].normal[1] == 1 )
		{
			xyzflags |=  4;
			brush->maxs[1] = originalplanes[j].dist;
		}
		else if( originalplanes[j].normal[1] == -1 )
		{
			xyzflags |=  8;
			brush->mins[1] = -originalplanes[j].dist;
		}
		else if( originalplanes[j].normal[2] ==  1 )
		{
			xyzflags |= 16;
			brush->maxs[2] = originalplanes[j].dist;
		}
		else if( originalplanes[j].normal[2] == -1 )
		{
			xyzflags |= 32;
			brush->mins[2] = -originalplanes[j].dist;
		}
	}

	// if not all xyzflags were set, then this is not a brush from q3map/q3map2, and needs reconstruction of the bounding box
	// (this case works for any brush with valid points, but sometimes brushes are not reconstructed properly and hence the points
	// are not valid, so this is reserved as a fallback case)
	if( xyzflags != 63 )
	{
		VectorCopy( brush->points[0].v, brush->mins );
		VectorCopy( brush->points[0].v, brush->maxs );

		for( j = 1; j < brush->numpoints; j++ )
		{
			brush->mins[0] = min(brush->mins[0], brush->points[j].v[0]);
			brush->mins[1] = min(brush->mins[1], brush->points[j].v[1]);
			brush->mins[2] = min(brush->mins[2], brush->points[j].v[2]);
			brush->maxs[0] = max(brush->maxs[0], brush->points[j].v[0]);
			brush->maxs[1] = max(brush->maxs[1], brush->points[j].v[1]);
			brush->maxs[2] = max(brush->maxs[2], brush->points[j].v[2]);
		}
	}

	brush->mins[0] -= 1;
	brush->mins[1] -= 1;
	brush->mins[2] -= 1;
	brush->maxs[0] += 1;
	brush->maxs[1] += 1;
	brush->maxs[2] += 1;
	CM_CollisionValidateBrush( brush );
	return brush;
}

void CM_CollisionCalcPlanesForPolygonBrushFloat( cbrushf_t *brush )
{
	int	i;
	float	edge0[3], edge1[3], edge2[3], normal[3], dist, bestdist;
	cpointf_t	*p, *p2;

	// FIXME: these probably don't actually need to be normalized if the collision code does not care
	if( brush->numpoints == 3 )
	{
		// optimized triangle case
		TriangleNormal( brush->points[0].v, brush->points[1].v, brush->points[2].v, brush->planes[0].normal );
		if( DotProduct( brush->planes[0].normal, brush->planes[0].normal) < 0.0001f )
		{
			// there's no point in processing a degenerate triangle (GIGO - Garbage In, Garbage Out)
			brush->numplanes = 0;
			return;
		}
		else
		{
			brush->numplanes = 5;
			VectorNormalize(brush->planes[0].normal);
			brush->planes[0].dist = DotProduct(brush->points->v, brush->planes[0].normal);
			VectorNegate(brush->planes[0].normal, brush->planes[1].normal);
			brush->planes[1].dist = -brush->planes[0].dist;
			VectorSubtract(brush->points[2].v, brush->points[0].v, edge0);
			VectorSubtract(brush->points[0].v, brush->points[1].v, edge1);
			VectorSubtract(brush->points[1].v, brush->points[2].v, edge2);
			{
				float	projectionnormal[3], projectionedge0[3], projectionedge1[3], projectionedge2[3];
				int	i, best = 0;
				float	dist, bestdist;

				bestdist = fabs( brush->planes[0].normal[0] );
				for( i = 1; i < 3; i++ )
				{
					dist = fabs( brush->planes[0].normal[i] );
					if( bestdist < dist )
					{
						bestdist = dist;
						best = i;
					}
				}
				VectorClear( projectionnormal );
				if( brush->planes[0].normal[best] < 0 )
					projectionnormal[best] = -1;
				else projectionnormal[best] = 1;
				VectorCopy( edge0, projectionedge0 );
				VectorCopy( edge1, projectionedge1 );
				VectorCopy( edge2, projectionedge2 );
				projectionedge0[best] = 0;
				projectionedge1[best] = 0;
				projectionedge2[best] = 0;
				CrossProduct( projectionedge0, projectionnormal, brush->planes[2].normal );
				CrossProduct( projectionedge1, projectionnormal, brush->planes[3].normal );
				CrossProduct( projectionedge2, projectionnormal, brush->planes[4].normal );
			}

			VectorNormalize(brush->planes[2].normal);
			VectorNormalize(brush->planes[3].normal);
			VectorNormalize(brush->planes[4].normal);
			brush->planes[2].dist = DotProduct(brush->points[2].v, brush->planes[2].normal);
			brush->planes[3].dist = DotProduct(brush->points[0].v, brush->planes[3].normal);
			brush->planes[4].dist = DotProduct(brush->points[1].v, brush->planes[4].normal);

			if( ph.developer >= D_NOTE )
			{
				// validation code
				if( fabs(DotProduct(brush->points[0].v, brush->planes[0].normal) - brush->planes[0].dist) > 0.01f || fabs(DotProduct(brush->points[1].v, brush->planes[0].normal) - brush->planes[0].dist) > 0.01f || fabs(DotProduct(brush->points[2].v, brush->planes[0].normal) - brush->planes[0].dist) > 0.01f)
					MsgDev( D_NOTE, "CM_CollisionCalcPlanesForPolygonBrushFloat: edges (%f %f %f to %f %f %f to %f %f %f) off front plane 0 (%f %f %f %f)\n", brush->points[0].v[0], brush->points[0].v[1], brush->points[0].v[2], brush->points[1].v[0], brush->points[1].v[1], brush->points[1].v[2], brush->points[2].v[0], brush->points[2].v[1], brush->points[2].v[2], brush->planes[0].normal[0], brush->planes[0].normal[1], brush->planes[0].normal[2], brush->planes[0].dist);
				if( fabs(DotProduct(brush->points[0].v, brush->planes[1].normal) - brush->planes[1].dist) > 0.01f || fabs(DotProduct(brush->points[1].v, brush->planes[1].normal) - brush->planes[1].dist) > 0.01f || fabs(DotProduct(brush->points[2].v, brush->planes[1].normal) - brush->planes[1].dist) > 0.01f)
					MsgDev( D_NOTE, "CM_CollisionCalcPlanesForPolygonBrushFloat: edges (%f %f %f to %f %f %f to %f %f %f) off back plane 1 (%f %f %f %f)\n", brush->points[0].v[0], brush->points[0].v[1], brush->points[0].v[2], brush->points[1].v[0], brush->points[1].v[1], brush->points[1].v[2], brush->points[2].v[0], brush->points[2].v[1], brush->points[2].v[2], brush->planes[1].normal[0], brush->planes[1].normal[1], brush->planes[1].normal[2], brush->planes[1].dist);
				if( fabs(DotProduct(brush->points[2].v, brush->planes[2].normal) - brush->planes[2].dist) > 0.01f || fabs(DotProduct(brush->points[0].v, brush->planes[2].normal) - brush->planes[2].dist) > 0.01f)
					MsgDev( D_NOTE, "CM_CollisionCalcPlanesForPolygonBrushFloat: edge 0 (%f %f %f to %f %f %f) off front plane 2 (%f %f %f %f)\n", brush->points[2].v[0], brush->points[2].v[1], brush->points[2].v[2], brush->points[0].v[0], brush->points[0].v[1], brush->points[0].v[2], brush->planes[2].normal[0], brush->planes[2].normal[1], brush->planes[2].normal[2], brush->planes[2].dist);
				if( fabs(DotProduct(brush->points[0].v, brush->planes[3].normal) - brush->planes[3].dist) > 0.01f || fabs(DotProduct(brush->points[1].v, brush->planes[3].normal) - brush->planes[3].dist) > 0.01f)
					MsgDev( D_NOTE, "CM_CollisionCalcPlanesForPolygonBrushFloat: edge 0 (%f %f %f to %f %f %f) off front plane 2 (%f %f %f %f)\n", brush->points[0].v[0], brush->points[0].v[1], brush->points[0].v[2], brush->points[1].v[0], brush->points[1].v[1], brush->points[1].v[2], brush->planes[3].normal[0], brush->planes[3].normal[1], brush->planes[3].normal[2], brush->planes[3].dist);
				if( fabs(DotProduct(brush->points[1].v, brush->planes[4].normal) - brush->planes[4].dist) > 0.01f || fabs(DotProduct(brush->points[2].v, brush->planes[4].normal) - brush->planes[4].dist) > 0.01f)
					MsgDev( D_NOTE, "CM_CollisionCalcPlanesForPolygonBrushFloat: edge 0 (%f %f %f to %f %f %f) off front plane 2 (%f %f %f %f)\n", brush->points[1].v[0], brush->points[1].v[1], brush->points[1].v[2], brush->points[2].v[0], brush->points[2].v[1], brush->points[2].v[2], brush->planes[4].normal[0], brush->planes[4].normal[1], brush->planes[4].normal[2], brush->planes[4].dist);
			}
		}
	}
	else
	{
		// choose best surface normal for polygon's plane
		bestdist = 0;
		for( i = 0, p = brush->points + 1; i < brush->numpoints - 2; i++, p++ )
		{
			VectorSubtract( p[-1].v, p[0].v, edge0 );
			VectorSubtract( p[1].v, p[0].v, edge1 );
			CrossProduct( edge0, edge1, normal );
			dist = DotProduct(normal, normal);
			if( i == 0 || bestdist < dist )
			{
				bestdist = dist;
				VectorCopy(normal, brush->planes->normal);
			}
		}
		if( bestdist < 0.0001f )
		{
			// there's no point in processing a degenerate triangle (GIGO - Garbage In, Garbage Out)
			brush->numplanes = 0;
			return;
		}
		else
		{
			brush->numplanes = brush->numpoints + 2;
			VectorNormalize( brush->planes->normal );
			brush->planes->dist = DotProduct( brush->points->v, brush->planes->normal );

			// negate plane to create other side
			VectorNegate( brush->planes[0].normal, brush->planes[1].normal );
			brush->planes[1].dist = -brush->planes[0].dist;
			for( i = 0, p = brush->points + (brush->numpoints - 1), p2 = brush->points; i < brush->numpoints; i++, p = p2, p2++ )
			{
				VectorSubtract( p->v, p2->v, edge0 );
				CrossProduct( edge0, brush->planes->normal, brush->planes[i + 2].normal );
				VectorNormalize( brush->planes[i + 2].normal );
				brush->planes[i + 2].dist = DotProduct( p->v, brush->planes[i + 2].normal );
			}
		}
	}

	if( ph.developer >= D_ERROR )
	{
		// validity check - will be disabled later
		CM_CollisionValidateBrush( brush );
		for( i = 0; i < brush->numplanes; i++ )
		{
			int	j;
			for( j = 0, p = brush->points; j < brush->numpoints; j++, p++ )
				if( DotProduct( p->v, brush->planes[i].normal ) > brush->planes[i].dist + COLLISION_PLANE_DIST_EPSILON )
					MsgDev( D_ERROR, "Error in brush plane generation, plane %i\n", i );
		}
	}
}

cbrushf_t *CM_CollisionAllocBrushFromPermanentPolygonFloat( byte *mempool, int numpoints, float *points, int supercontents )
{
	cbrushf_t	*brush = (cbrushf_t *)Mem_Alloc( mempool, sizeof(cbrushf_t) + sizeof(cplanef_t) * (numpoints + 2));

	brush->contents = supercontents;
	brush->numpoints = numpoints;
	brush->numplanes = numpoints + 2;
	brush->planes = (cplanef_t *)(brush + 1);
	brush->points = (cpointf_t *)points;
	Host_Error( "CM_CollisionAllocBrushFromPermanentPolygonFloat: FIXME: this code needs to be updated to generate a mesh..." );
	return brush;
}

// NOTE: start and end of each brush pair must have same numplanes/numpoints
void CM_CollisionTraceBrushBrushFloat( trace_t *trace, const cbrushf_t *thisbrush_start, const cbrushf_t *thisbrush_end, const cbrushf_t *thatbrush_start, const cbrushf_t *thatbrush_end )
{
	int		nplane, nplane2, hitsurfaceflags = 0;
	float		enterfrac = -1, leavefrac = 1, d1, d2, f, imove, newimpactnormal[3], enterfrac2 = -1;
	const cplanef_t	*startplane, *endplane;
	csurface_t	*hitsurface = NULL;

	VectorClear( newimpactnormal );

	for( nplane = 0; nplane < thatbrush_start->numplanes + thisbrush_start->numplanes; nplane++ )
	{
		nplane2 = nplane;
		if( nplane2 >= thatbrush_start->numplanes )
		{
			nplane2 -= thatbrush_start->numplanes;
			startplane = thisbrush_start->planes + nplane2;
			endplane = thisbrush_end->planes + nplane2;
			if( ph.developer >= D_ERROR )
			{
				// any brush with degenerate planes is not worth handling
				if( DotProduct(startplane->normal, startplane->normal) < 0.9f || DotProduct(endplane->normal, endplane->normal) < 0.9f)
				{
					MsgDev( D_ERROR, "CM_CollisionTraceBrushBrushFloat: degenerate thisbrush plane!\n" );
					return;
				}
				f = furthestplanedist_float(startplane->normal, thisbrush_start->points, thisbrush_start->numpoints);
				if( fabs(f - startplane->dist) > COLLISION_PLANE_DIST_EPSILON )
					MsgDev( D_WARN, "startplane->dist %f != calculated %f (thisbrush_start)\n", startplane->dist, f );
			}
			d1 = nearestplanedist_float( startplane->normal, thisbrush_start->points, thisbrush_start->numpoints) - furthestplanedist_float( startplane->normal, thatbrush_start->points, thatbrush_start->numpoints ) - cm_startnudge->value;
			d2 = nearestplanedist_float( endplane->normal, thisbrush_end->points, thisbrush_end->numpoints) - furthestplanedist_float( endplane->normal, thatbrush_end->points, thatbrush_end->numpoints ) - cm_endnudge->value;
		}
		else
		{
			startplane = thatbrush_start->planes + nplane2;
			endplane = thatbrush_end->planes + nplane2;
			if( ph.developer >= D_ERROR )
			{
				// any brush with degenerate planes is not worth handling
				if( DotProduct(startplane->normal, startplane->normal) < 0.9f || DotProduct(endplane->normal, endplane->normal) < 0.9f )
				{
					MsgDev( D_ERROR, "CM_CollisionTraceBrushBrushFloat: degenerate thatbrush plane!\n" );
					return;
				}
				f = furthestplanedist_float( startplane->normal, thatbrush_start->points, thatbrush_start->numpoints );
				if( fabs(f - startplane->dist) > COLLISION_PLANE_DIST_EPSILON )
					MsgDev( D_WARN, "startplane->dist %f != calculated %f (thatbrush_start)\n", startplane->dist, f );
			}
			d1 = nearestplanedist_float( startplane->normal, thisbrush_start->points, thisbrush_start->numpoints ) - startplane->dist - cm_startnudge->value;
			d2 = nearestplanedist_float( endplane->normal, thisbrush_end->points, thisbrush_end->numpoints ) - endplane->dist - cm_endnudge->value;
		}

		if( d1 > d2 )
		{
			// moving into brush
			if( d2 >= cm_enternudge->value ) return;
			if( d1 > 0 )
			{
				// enter
				imove = 1 / (d1 - d2);
				f = (d1 - cm_enternudge->value) * imove;
				if( f < 0 ) f = 0;
				// check if this will reduce the collision time range
				if( enterfrac < f )
				{
					// reduced collision time range
					enterfrac = f;
					// if the collision time range is now empty, no collision
					if( enterfrac > leavefrac ) return;
					// if the collision would be further away than the trace's
					// existing collision data, we don't care about this
					// collision
					if( enterfrac > trace->realfraction ) return;
					// calculate the nudged fraction and impact normal we'll
					// need if we accept this collision later
					enterfrac2 = (d1 - cm_impactnudge->value) * imove;
					VectorLerp( startplane->normal, enterfrac, endplane->normal, newimpactnormal );
					hitsurfaceflags = startplane->surfaceflags;
					hitsurface = startplane->surface;
				}
			}
		}
		else
		{
			// moving out of brush
			if( d1 > 0 ) return;
			if( d2 > 0 )
			{
				// leave
				f = (d1 + cm_leavenudge->value) / (d1 - d2);
				if( f > 1 ) f = 1;
				// check if this will reduce the collision time range
				if( leavefrac > f )
				{
					// reduced collision time range
					leavefrac = f;
					// if the collision time range is now empty, no collision
					if( enterfrac > leavefrac ) return;
				}
			}
		}
	}

	// at this point we know the trace overlaps the brush because it was not
	// rejected at any point in the loop above

	// see if the trace started outside the brush or not
	if( enterfrac > -1 )
	{
		// started outside, and overlaps, therefore there is a collision here
		// store out the impact information
		if( trace->contentsmask & thatbrush_start->contents )
		{
			trace->contents = thatbrush_start->contents;
			trace->surfaceflags = hitsurfaceflags;
			trace->surface = hitsurface;
			trace->realfraction = bound(0, enterfrac, 1);
			trace->fraction = bound(0, enterfrac2, 1);
			if( cm_prefernudgedfraction->integer )
				trace->realfraction = trace->fraction;
			VectorCopy( newimpactnormal, trace->plane.normal );
		}
	}
	else
	{
		// started inside, update startsolid and friends
		trace->startcontents |= thatbrush_start->contents;
		if( trace->contentsmask & thatbrush_start->contents )
		{
			trace->startsolid = true;
			if( leavefrac < 1 ) trace->allsolid = true;
		}
	}
}

// NOTE: start and end brush pair must have same numplanes/numpoints
void CM_CollisionTraceLineBrushFloat( trace_t *trace, const vec3_t linestart, const vec3_t lineend, const cbrushf_t *thatbrush_start, const cbrushf_t *thatbrush_end )
{
	int		nplane, hitsurfaceflags = 0;
	float		enterfrac = -1, leavefrac = 1, d1, d2, f, imove, newimpactnormal[3], enterfrac2 = -1;
	const cplanef_t	*startplane, *endplane;
	csurface_t	*hitsurface = NULL;

	VectorClear( newimpactnormal );

	for( nplane = 0; nplane < thatbrush_start->numplanes; nplane++ )
	{
		startplane = thatbrush_start->planes + nplane;
		endplane = thatbrush_end->planes + nplane;
		d1 = DotProduct( startplane->normal, linestart ) - startplane->dist - cm_startnudge->value;
		d2 = DotProduct( endplane->normal, lineend ) - endplane->dist - cm_endnudge->value;
		if( ph.developer >= D_ERROR )
		{
			// any brush with degenerate planes is not worth handling
			if( DotProduct(startplane->normal, startplane->normal) < 0.9f || DotProduct(endplane->normal, endplane->normal) < 0.9f)
			{
				MsgDev( D_WARN, "CM_CollisionTraceLineBrushFloat: degenerate plane!\n");
				return;
			}
			if (thatbrush_start->numpoints)
			{
				f = furthestplanedist_float( startplane->normal, thatbrush_start->points, thatbrush_start->numpoints );
				if( fabs(f - startplane->dist) > COLLISION_PLANE_DIST_EPSILON )
					MsgDev( D_WARN, "startplane->dist %f != calculated %f\n", startplane->dist, f);
			}
		}

		if( d1 > d2 )
		{
			// moving into brush
			if( d2 >= cm_enternudge->value ) return;
			if( d1 > 0 )
			{
				// enter
				imove = 1 / (d1 - d2);
				f = (d1 - cm_enternudge->value) * imove;
				if( f < 0 ) f = 0;
				// check if this will reduce the collision time range
				if( enterfrac < f )
				{
					// reduced collision time range
					enterfrac = f;
					// if the collision time range is now empty, no collision
					if( enterfrac > leavefrac ) return;
					// if the collision would be further away than the trace's
					// existing collision data, we don't care about this
					// collision
					if( enterfrac > trace->realfraction ) return;
					// calculate the nudged fraction and impact normal we'll
					// need if we accept this collision later
					enterfrac2 = (d1 - cm_impactnudge->value) * imove;
					VectorLerp( startplane->normal, enterfrac, endplane->normal, newimpactnormal );
					hitsurfaceflags = startplane->surfaceflags;
					hitsurface = startplane->surface;
				}
			}
		}
		else
		{
			// moving out of brush
			if( d1 > 0 ) return;
			if( d2 > 0 )
			{
				// leave
				f = ( d1 + cm_leavenudge->value) / (d1 - d2);
				// check if this will reduce the collision time range
				if( leavefrac > f )
				{
					// reduced collision time range
					leavefrac = f;
					// if the collision time range is now empty, no collision
					if( enterfrac > leavefrac ) return;
				}
			}
		}
	}

	// at this point we know the trace overlaps the brush because it was not
	// rejected at any point in the loop above

	// see if the trace started outside the brush or not
	if( enterfrac > -1 )
	{
		// started outside, and overlaps, therefore there is a collision here
		// store out the impact information
		if( trace->contentsmask & thatbrush_start->contents )
		{
			trace->contents = thatbrush_start->contents;
			trace->surfaceflags = hitsurfaceflags;
			trace->surface = hitsurface;
			trace->realfraction = bound( 0, enterfrac, 1 );
			trace->fraction = bound( 0, enterfrac2, 1 );
			if( cm_prefernudgedfraction->integer )
				trace->realfraction = trace->fraction;
			VectorCopy( newimpactnormal, trace->plane.normal );
		}
	}
	else
	{
		// started inside, update startsolid and friends
		trace->startcontents |= thatbrush_start->contents;
		if( trace->contentsmask & thatbrush_start->contents)
		{
			trace->startsolid = true;
			if( leavefrac < 1 ) trace->allsolid = true;
		}
	}
}

bool CM_CollisionPointInsideBrushFloat( const vec3_t point, const cbrushf_t *brush )
{
	int		nplane;
	const cplanef_t	*plane;

	if(!BoxesOverlap( point, point, brush->mins, brush->maxs ))
		return false;
	for( nplane = 0, plane = brush->planes; nplane < brush->numplanes; nplane++, plane++ )
		if(DotProduct(plane->normal, point) > plane->dist)
			return false;
	return true;
}

void CM_CollisionTracePointBrushFloat( trace_t *trace, const vec3_t point, const cbrushf_t *thatbrush )
{
	if(!CM_CollisionPointInsideBrushFloat( point, thatbrush ))
		return;

	trace->startcontents |= thatbrush->contents;
	if( trace->contentsmask & thatbrush->contents )
	{
		trace->startsolid = true;
		trace->allsolid = true;
	}
}

void CM_CollisionSnapCopyPoints( int numpoints, const cpointf_t *in, cpointf_t *out, float fractionprecision, float invfractionprecision )
{
	int i;
	for( i = 0; i < numpoints; i++ )
	{
		out[i].v[0] = floor(in[i].v[0] * fractionprecision + 0.5f) * invfractionprecision;
		out[i].v[1] = floor(in[i].v[1] * fractionprecision + 0.5f) * invfractionprecision;
		out[i].v[2] = floor(in[i].v[2] * fractionprecision + 0.5f) * invfractionprecision;
	}
}

void CM_CollisionTraceBrushPolygonFloat( trace_t *trace, const cbrushf_t *thisbrush_start, const cbrushf_t *thisbrush_end, int numpoints, const float *points, int supercontents )
{
	if( numpoints > MAX_BUILD_SIDES )
	{
		MsgDev( D_ERROR, "Polygon with more than %d points not supported\n", MAX_BUILD_SIDES );
		return;
	}
	polyf_brush.numpoints = numpoints;
	polyf_brush.numplanes = numpoints + 2;
	polyf_brush.planes = polyf_planes;
	polyf_brush.contents = supercontents;
	polyf_brush.points = polyf_points;
	CM_CollisionSnapCopyPoints( polyf_brush.numpoints, (cpointf_t *)points, polyf_points, COLLISION_SNAPSCALE, COLLISION_SNAP );
	CM_CollisionCalcPlanesForPolygonBrushFloat( &polyf_brush );
	CM_CollisionTraceBrushBrushFloat( trace, thisbrush_start, thisbrush_end, &polyf_brush, &polyf_brush );
}

void CM_CollisionTraceBrushTriangleMeshFloat( trace_t *trace, const cbrushf_t *thisbrush_start, const cbrushf_t *thisbrush_end, int numtriangles, const int *element3i, const float *vertex3f, int supercontents, int surfaceflags, csurface_t *surface, const vec3_t segmentmins, const vec3_t segmentmaxs )
{
	int	i;

	polyf_brush.numpoints = 3;
	polyf_brush.numplanes = 5;
	polyf_brush.points = polyf_points;
	polyf_brush.planes = polyf_planes;
	polyf_brush.contents = supercontents;

	for( i = 0; i < polyf_brush.numplanes; i++ )
	{
		polyf_brush.planes[i].surfaceflags = surfaceflags;
		polyf_brush.planes[i].surface = surface;
	}

	for( i = 0; i < numtriangles; i++, element3i += 3 )
	{
		if( TriangleOverlapsBox( vertex3f + element3i[0]*3, vertex3f + element3i[1]*3, vertex3f + element3i[2]*3, segmentmins, segmentmaxs))
		{
			VectorCopy(vertex3f + element3i[0] * 3, polyf_points[0].v);
			VectorCopy(vertex3f + element3i[1] * 3, polyf_points[1].v);
			VectorCopy(vertex3f + element3i[2] * 3, polyf_points[2].v);
			CM_CollisionSnapCopyPoints( polyf_brush.numpoints, polyf_points, polyf_points, COLLISION_SNAPSCALE, COLLISION_SNAP );
			CM_CollisionCalcPlanesForPolygonBrushFloat( &polyf_brush );
			CM_CollisionTraceBrushBrushFloat( trace, thisbrush_start, thisbrush_end, &polyf_brush, &polyf_brush );
		}
	}
}

void CM_CollisionTraceLinePolygonFloat( trace_t *trace, const vec3_t linestart, const vec3_t lineend, int numpoints, const float *points, int supercontents )
{
	if( numpoints > MAX_BUILD_SIDES )
	{
		MsgDev( D_ERROR, "Polygon with more than %d points not supported\n", MAX_BUILD_SIDES );
		return;
	}

	polyf_brush.numpoints = numpoints;
	polyf_brush.numplanes = numpoints + 2;
	polyf_brush.points = polyf_points;
	CM_CollisionSnapCopyPoints( polyf_brush.numpoints, (cpointf_t *)points, polyf_points, COLLISION_SNAPSCALE, COLLISION_SNAP );
	polyf_brush.planes = polyf_planes;
	polyf_brush.contents = supercontents;
	CM_CollisionCalcPlanesForPolygonBrushFloat( &polyf_brush );
	CM_CollisionTraceLineBrushFloat( trace, linestart, lineend, &polyf_brush, &polyf_brush );
}

void CM_CollisionTraceLineTriangleMeshFloat( trace_t *trace, const vec3_t linestart, const vec3_t lineend, int numtriangles, const int *element3i, const float *vertex3f, int supercontents, int surfaceflags, csurface_t *surface, const vec3_t segmentmins, const vec3_t segmentmaxs )
{
	int i;
#if 1
	// FIXME: snap vertices?
	for( i = 0; i < numtriangles; i++, element3i += 3 )
		CM_CollisionTraceLineTriangleFloat( trace, linestart, lineend, vertex3f + element3i[0] * 3, vertex3f + element3i[1] * 3, vertex3f + element3i[2] * 3, supercontents, surfaceflags, surface );
#else
	polyf_brush.numpoints = 3;
	polyf_brush.numplanes = 5;
	polyf_brush.points = polyf_points;
	polyf_brush.planes = polyf_planes;
	polyf_brush.supercontents = supercontents;

	for( i = 0; i < polyf_brush.numplanes; i++ )
	{
		polyf_brush.planes[i].contents = supercontents;
		polyf_brush.planes[i].surfaceflags = surfaceflags;
		polyf_brush.planes[i].surface = surface;
	}

	for( i = 0; i < numtriangles; i++, element3i += 3 )
	{
		if(TriangleOverlapsBox( vertex3f + element3i[0]*3, vertex3 + [element3i[1]*3, vertex3f + element3i[2]*3, segmentmins, segmentmaxs ))
		{
			VectorCopy(vertex3f + element3i[0] * 3, polyf_points[0].v);
			VectorCopy(vertex3f + element3i[1] * 3, polyf_points[1].v);
			VectorCopy(vertex3f + element3i[2] * 3, polyf_points[2].v);
			CM_CollisionSnapCopyPoints( polyf_brush.numpoints, polyf_points, polyf_points, COLLISION_SNAPSCALE, COLLISION_SNAP );
			CM_CollisionCalcPlanesForPolygonBrushFloat( &polyf_brush );
			CM_CollisionTraceLineBrushFloat( trace, linestart, lineend, &polyf_brush, &polyf_brush );
		}
	}
#endif
}

void CM_CollisionTraceBrushPolygonTransformFloat( trace_t *trace, const cbrushf_t *thisbrush_start, const cbrushf_t *thisbrush_end, int numpoints, const float *points, const matrix4x4 polygonmatrixstart, const matrix4x4 polygonmatrixend, int supercontents, int surfaceflags, csurface_t *surface )
{
	int	i;

	if( numpoints > MAX_BUILD_SIDES )
	{
		MsgDev( D_ERROR, "Polygon with more than %d points not supported\n", MAX_BUILD_SIDES );
		return;
	}

	polyf_brushstart.numpoints = numpoints;
	polyf_brushstart.numplanes = numpoints + 2;
	polyf_brushstart.points = polyf_pointsstart;
	polyf_brushstart.planes = polyf_planesstart;
	polyf_brushstart.contents = supercontents;

	for( i = 0; i < numpoints; i++ )
		Matrix4x4_Transform( polygonmatrixstart, points + i * 3, polyf_brushstart.points[i].v );

	polyf_brushend.numpoints = numpoints;
	polyf_brushend.numplanes = numpoints + 2;
	polyf_brushend.points = polyf_pointsend;
	polyf_brushend.planes = polyf_planesend;
	polyf_brushend.contents = supercontents;

	for( i = 0; i < numpoints; i++ )
		Matrix4x4_Transform( polygonmatrixend, points + i * 3, polyf_brushend.points[i].v );

	for( i = 0; i < polyf_brushstart.numplanes; i++ )
	{
		polyf_brushstart.planes[i].surfaceflags = surfaceflags;
		polyf_brushstart.planes[i].surface = surface;
	}

	CM_CollisionSnapCopyPoints( polyf_brushstart.numpoints, polyf_pointsstart, polyf_pointsstart, COLLISION_SNAPSCALE, COLLISION_SNAP );
	CM_CollisionSnapCopyPoints( polyf_brushend.numpoints, polyf_pointsend, polyf_pointsend, COLLISION_SNAPSCALE, COLLISION_SNAP );
	CM_CollisionCalcPlanesForPolygonBrushFloat( &polyf_brushstart );
	CM_CollisionCalcPlanesForPolygonBrushFloat( &polyf_brushend );
	CM_CollisionTraceBrushBrushFloat( trace, thisbrush_start, thisbrush_end, &polyf_brushstart, &polyf_brushend );
}

void CM_CollisionInitBrushForBox( void )
{
	int i;
	for( i = 0; i < MAX_BRUSHFORBOX; i++ )
	{
		brushforbox_brush[i].numpoints = 8;
		brushforbox_brush[i].numplanes = 6;
		brushforbox_brush[i].points = brushforbox_point + i * 8;
		brushforbox_brush[i].planes = brushforbox_plane + i * 6;
		brushforpoint_brush[i].numpoints = 1;
		brushforpoint_brush[i].numplanes = 0;
		brushforpoint_brush[i].points = brushforbox_point + i * 8;
		brushforpoint_brush[i].planes = brushforbox_plane + i * 6;
	}
}

cbrushf_t *CM_CollisionBrushForBox( const matrix4x4 matrix, const vec3_t mins, const vec3_t maxs, int supercontents, int surfaceflags, csurface_t *surface )
{
	int	i, j;
	vec3_t	v;
	cbrushf_t	*brush;

	if( brushforbox_brush[0].numpoints == 0 )
		CM_CollisionInitBrushForBox();

	// FIXME: these probably don't actually need to be normalized if the collision code does not care
	if(VectorCompare( mins, maxs ))
	{
		// point brush
		brush = brushforpoint_brush + ((brushforbox_index++) % MAX_BRUSHFORBOX);
		VectorCopy( mins, brush->points->v );
	}
	else
	{
		brush = brushforbox_brush + ((brushforbox_index++) % MAX_BRUSHFORBOX);
		// FIXME: optimize
		for( i = 0; i < 8; i++ )
		{
			v[0] = i & 1 ? maxs[0] : mins[0];
			v[1] = i & 2 ? maxs[1] : mins[1];
			v[2] = i & 4 ? maxs[2] : mins[2];
			Matrix4x4_Transform( matrix, v, brush->points[i].v );
		}
		// FIXME: optimize!
		for( i = 0; i < 6; i++ )
		{
			VectorClear(v);
			v[i >> 1] = i & 1 ? 1 : -1;
			Matrix4x4_Transform3x3( matrix, v, brush->planes[i].normal );
			VectorNormalize( brush->planes[i].normal );
		}
	}

	brush->contents = supercontents;
	for( j = 0; j < brush->numplanes; j++ )
	{
		brush->planes[j].surfaceflags = surfaceflags;
		brush->planes[j].surface = surface;
		brush->planes[j].dist = furthestplanedist_float( brush->planes[j].normal, brush->points, brush->numpoints );
	}

	VectorCopy( brush->points[0].v, brush->mins );
	VectorCopy( brush->points[0].v, brush->maxs );

	for( j = 1; j < brush->numpoints; j++ )
	{
		brush->mins[0] = min( brush->mins[0], brush->points[j].v[0] );
		brush->mins[1] = min( brush->mins[1], brush->points[j].v[1] );
		brush->mins[2] = min( brush->mins[2], brush->points[j].v[2] );
		brush->maxs[0] = max( brush->maxs[0], brush->points[j].v[0] );
		brush->maxs[1] = max( brush->maxs[1], brush->points[j].v[1] );
		brush->maxs[2] = max( brush->maxs[2], brush->points[j].v[2] );
	}

	brush->mins[0] -= 1;
	brush->mins[1] -= 1;
	brush->mins[2] -= 1;
	brush->maxs[0] += 1;
	brush->maxs[1] += 1;
	brush->maxs[2] += 1;

	CM_CollisionValidateBrush( brush );

	return brush;
}

void CM_CollisionClipTrace_BrushBox( trace_t *trace, const vec3_t cmins, const vec3_t cmaxs, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitsupercontentsmask, int supercontents, int surfaceflags, csurface_t *surface )
{
	cbrushf_t	*boxbrush, *thisbrush_start, *thisbrush_end;
	vec3_t	startmins, startmaxs, endmins, endmaxs;

	// create brushes for the collision
	VectorAdd( start, mins, startmins );
	VectorAdd( start, maxs, startmaxs );
	VectorAdd( end, mins, endmins );
	VectorAdd( end, maxs, endmaxs );

	boxbrush = CM_CollisionBrushForBox( identitymatrix, cmins, cmaxs, supercontents, surfaceflags, surface );
	thisbrush_start = CM_CollisionBrushForBox( identitymatrix, startmins, startmaxs, 0, 0, NULL );
	thisbrush_end = CM_CollisionBrushForBox( identitymatrix, endmins, endmaxs, 0, 0, NULL );

	memset( trace, 0, sizeof(trace_t));
	trace->contentsmask = hitsupercontentsmask;
	trace->fraction = 1;
	trace->realfraction = 1;
	trace->allsolid = true;
	CM_CollisionTraceBrushBrushFloat( trace, thisbrush_start, thisbrush_end, boxbrush, boxbrush );
}

// NOTE: this can be used for tracing a moving sphere vs a stationary sphere,
// by simply adding the moving sphere's radius to the sphereradius parameter,
// all the results are correct (impactpoint, impactnormal, and fraction)
float CM_CollisionClipTrace_LineSphere( double *linestart, double *lineend, double *sphereorigin, double sphereradius, double *impactpoint, double *impactnormal )
{
	double	dir[3], scale, v[3], deviationdist, impactdist, linelength;

	// make sure the impactpoint and impactnormal are valid even if there is
	// no collision
	VectorCopy( lineend, impactpoint );
	VectorClear( impactnormal );
	VectorSubtract( lineend, linestart, dir ); // calculate line direction

	// normalize direction
	linelength = VectorLength( dir );
	if( linelength )
	{
		scale = 1.0 / linelength;
		VectorScale( dir, scale, dir );
	}

	// this dotproduct calculates the distance along the line at which the
	// sphere origin is (nearest point to the sphere origin on the line)
	impactdist = DotProduct( sphereorigin, dir ) - DotProduct( linestart, dir );

	// calculate point on line at that distance, and subtract the
	// sphereorigin from it, so we have a vector to measure for the distance
	// of the line from the sphereorigin (deviation, how off-center it is)
	VectorMA( linestart, impactdist, dir, v );
	VectorSubtract( v, sphereorigin, v );
	deviationdist = VectorLength2( v );

	// if outside the radius, it's a miss for sure
	// (we do this comparison using squared radius to avoid a sqrt)
	if( deviationdist > sphereradius * sphereradius ) return 1; // miss (off to the side)
	// nudge back to find the correct impact distance
	impactdist -= sphereradius - deviationdist/sphereradius;

	if( impactdist >= linelength ) return 1; // miss (not close enough)
	if( impactdist < 0 ) return 1; // miss (linestart is past or inside sphere)

	VectorMA( linestart, impactdist, dir, impactpoint );	// calculate new impactpoint
	VectorSubtract( impactpoint, sphereorigin, impactnormal );	// calculate impactnormal (surface normal at point of impact)
	VectorNormalize( impactnormal );			// normalize impactnormal

	// return fraction of movement distance
	return impactdist / linelength;
}

void CM_CollisionTraceLineTriangleFloat( trace_t *trace, const vec3_t linestart, const vec3_t lineend, const float *point0, const float *point1, const float *point2, int supercontents, int surfaceflags, csurface_t *surface )
{
	float	d1, d2, d, f, impact[3], edgenormal[3], faceplanenormal[3];
	float	faceplanedist, faceplanenormallength2, edge01[3], edge21[3], edge02[3];

	// this function executes:
	// 32 ops when line starts behind triangle
	// 38 ops when line ends infront of triangle
	// 43 ops when line fraction is already closer than this triangle
	// 72 ops when line is outside edge 01
	// 92 ops when line is outside edge 21
	// 115 ops when line is outside edge 02
	// 123 ops when line impacts triangle and updates trace results

	// this code is designed for clockwise triangles, conversion to
	// counterclockwise would require swapping some things around...
	// it is easier to simply swap the point0 and point2 parameters to this
	// function when calling it than it is to rewire the internals.

	// calculate the faceplanenormal of the triangle, this represents the front side
	// 15 ops
	VectorSubtract( point0, point1, edge01 );
	VectorSubtract( point2, point1, edge21 );
	CrossProduct( edge01, edge21, faceplanenormal );
	// there's no point in processing a degenerate triangle (GIGO - Garbage In, Garbage Out)
	// 6 ops
	faceplanenormallength2 = DotProduct( faceplanenormal, faceplanenormal );
	if( faceplanenormallength2 < 0.0001f ) return;
	
	// calculate the distance
	// 5 ops
	faceplanedist = DotProduct( point0, faceplanenormal );

	// if start point is on the back side there is no collision
	// (we don't care about traces going through the triangle the wrong way)

	// calculate the start distance
	// 6 ops
	d1 = DotProduct( faceplanenormal, linestart );
	if( d1 <= faceplanedist ) return;

	// calculate the end distance
	// 6 ops
	d2 = DotProduct( faceplanenormal, lineend );
	// if both are in front, there is no collision
	if( d2 >= faceplanedist ) return;

	// from here on we know d1 is >= 0 and d2 is < 0
	// this means the line starts infront and ends behind, passing through it

	// calculate the recipricol of the distance delta,
	// so we can use it multiple times cheaply (instead of division)
	// 2 ops
	d = 1.0f / (d1 - d2);
	// calculate the impact fraction by taking the start distance (> 0)
	// and subtracting the face plane distance (this is the distance of the
	// triangle along that same normal)
	// then multiply by the recipricol distance delta
	// 2 ops
	f = (d1 - faceplanedist) * d;
	// skip out if this impact is further away than previous ones
	// 1 ops
	if( f > trace->realfraction ) return;
	// calculate the perfect impact point for classification of insidedness
	// 9 ops
	impact[0] = linestart[0] + f * (lineend[0] - linestart[0]);
	impact[1] = linestart[1] + f * (lineend[1] - linestart[1]);
	impact[2] = linestart[2] + f * (lineend[2] - linestart[2]);

	// calculate the edge normal and reject if impact is outside triangle
	// (an edge normal faces away from the triangle, to get the desired normal
	//  a crossproduct with the faceplanenormal is used, and because of the way
	// the insidedness comparison is written it does not need to be normalized)

	// first use the two edges from the triangle plane math
	// the other edge only gets calculated if the point survives that long

	// 20 ops
	CrossProduct( edge01, faceplanenormal, edgenormal );
	if(DotProduct( impact, edgenormal ) > DotProduct( point1, edgenormal ))
		return;

	// 20 ops
	CrossProduct( faceplanenormal, edge21, edgenormal );
	if( DotProduct( impact, edgenormal ) > DotProduct( point2, edgenormal ))
		return;

	// 23 ops
	VectorSubtract( point0, point2, edge02 );
	CrossProduct( faceplanenormal, edge02, edgenormal );
	if( DotProduct( impact, edgenormal ) > DotProduct( point0, edgenormal ))
		return;

	// 8 ops (rare)

	// store the new trace fraction
	trace->realfraction = f;

	// calculate a nudged fraction to keep it out of the surface
	// (the main fraction remains perfect)
	trace->fraction = f - cm_impactnudge->value * d;

	if( cm_prefernudgedfraction->integer )
		trace->realfraction = trace->fraction;

	// store the new trace plane (because collisions only happen from
	// the front this is always simply the triangle normal, never flipped)
	d = 1.0 / sqrt(faceplanenormallength2);
	VectorScale( faceplanenormal, d, trace->plane.normal );
	trace->plane.dist = faceplanedist * d;

	trace->contents = supercontents;
	trace->surfaceflags = surfaceflags;
	trace->surface = surface;
}

cbsp_t *CM_CollisionCreateCollisionBSP( byte *mempool )
{
	cbsp_t *bsp = (cbsp_t *)Mem_Alloc( mempool, sizeof(cbsp_t));
	bsp->mempool = mempool;
	bsp->nodes = (cbspnode_t *)Mem_Alloc( bsp->mempool, sizeof(cbspnode_t));
	return bsp;
}

void CM_CollisionFreeCollisionBSPNode( cbspnode_t *node )
{
	if( node->children[0] ) CM_CollisionFreeCollisionBSPNode( node->children[0] );
	if( node->children[1] ) CM_CollisionFreeCollisionBSPNode( node->children[1] );
	while( --node->numcbrushf ) Mem_Free( node->cbrushflist[node->numcbrushf]);
	Mem_Free( node );
}

void CM_CollisionFreeCollisionBSP( cbsp_t *bsp )
{
	CM_CollisionFreeCollisionBSPNode( bsp->nodes );
	Mem_Free( bsp );
}

void CM_CollisionBoundingBoxOfBrushTraceSegment( const cbrushf_t *start, const cbrushf_t *end, vec3_t mins, vec3_t maxs, float startfrac, float endfrac )
{
	int		i;
	cpointf_t		*ps, *pe;
	float		tempstart[3], tempend[3];

	VectorLerp( start->points[0].v, startfrac, end->points[0].v, mins );
	VectorCopy( mins, maxs );

	for( i = 0, ps = start->points, pe = end->points; i < start->numpoints; i++, ps++, pe++ )
	{
		VectorLerp( ps->v, startfrac, pe->v, tempstart );
		VectorLerp( ps->v, endfrac, pe->v, tempend );
		mins[0] = min(mins[0], min(tempstart[0], tempend[0]));
		mins[1] = min(mins[1], min(tempstart[1], tempend[1]));
		mins[2] = min(mins[2], min(tempstart[2], tempend[2]));
		maxs[0] = min(maxs[0], min(tempstart[0], tempend[0]));
		maxs[1] = min(maxs[1], min(tempstart[1], tempend[1]));
		maxs[2] = min(maxs[2], min(tempstart[2], tempend[2]));
	}

	mins[0] -= 1;
	mins[1] -= 1;
	mins[2] -= 1;
	maxs[0] += 1;
	maxs[1] += 1;
	maxs[2] += 1;
}

void CM_CollisionClipTrace_Box( trace_t *trace, const vec3_t cmins, const vec3_t cmaxs, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contentsmask, int contents, int surfaceflags, csurface_t *surface )
{
	cbrushf_t		cbox;
	cplanef_t		cbox_planes[6];

	cbox.contents = contents;
	cbox.numplanes = 6;
	cbox.numpoints = 0;
	cbox.numtriangles = 0;
	cbox.planes = cbox_planes;
	cbox.points = NULL;
	cbox.elements = NULL;
	cbox.markframe = 0;
	cbox.mins[0] = 0;
	cbox.mins[1] = 0;
	cbox.mins[2] = 0;
	cbox.maxs[0] = 0;
	cbox.maxs[1] = 0;
	cbox.maxs[2] = 0;
	cbox_planes[0].normal[0] =  1;
	cbox_planes[0].normal[1] =  0;
	cbox_planes[0].normal[2] =  0;
	cbox_planes[0].dist = cmaxs[0] - mins[0];
	cbox_planes[1].normal[0] = -1;
	cbox_planes[1].normal[1] =  0;
	cbox_planes[1].normal[2] =  0;
	cbox_planes[1].dist = maxs[0] - cmins[0];
	cbox_planes[2].normal[0] =  0;
	cbox_planes[2].normal[1] =  1;
	cbox_planes[2].normal[2] =  0;
	cbox_planes[2].dist = cmaxs[1] - mins[1];
	cbox_planes[3].normal[0] =  0;
	cbox_planes[3].normal[1] = -1;
	cbox_planes[3].normal[2] =  0;
	cbox_planes[3].dist = maxs[1] - cmins[1];
	cbox_planes[4].normal[0] =  0;
	cbox_planes[4].normal[1] =  0;
	cbox_planes[4].normal[2] =  1;
	cbox_planes[4].dist = cmaxs[2] - mins[2];
	cbox_planes[5].normal[0] =  0;
	cbox_planes[5].normal[1] =  0;
	cbox_planes[5].normal[2] = -1;
	cbox_planes[5].dist = maxs[2] - cmins[2];
	cbox_planes[0].surfaceflags = surfaceflags;
	cbox_planes[0].surface = surface;
	cbox_planes[1].surfaceflags = surfaceflags;
	cbox_planes[1].surface = surface;
	cbox_planes[2].surfaceflags = surfaceflags;
	cbox_planes[2].surface = surface;
	cbox_planes[3].surfaceflags = surfaceflags;
	cbox_planes[3].surface = surface;
	cbox_planes[4].surfaceflags = surfaceflags;
	cbox_planes[4].surface = surface;
	cbox_planes[5].surfaceflags = surfaceflags;
	cbox_planes[5].surface = surface;

	memset(trace, 0, sizeof(trace_t));
	trace->contentsmask = contentsmask;
	trace->fraction = 1;
	trace->realfraction = 1;
	CM_CollisionTraceLineBrushFloat( trace, start, end, &cbox, &cbox );
}

//===========================================

void CM_CollisionClipToGenericEntity( trace_t *trace, cmodel_t *model, const vec3_t bodymins, const vec3_t bodymaxs, int bodysupercontents, matrix4x4 matrix, matrix4x4 inversematrix, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitsupercontentsmask )
{
	float	tempnormal[3];
	float	starttransformed[3];
	float	endtransformed[3];

	memset( trace, 0, sizeof(*trace));
	trace->fraction = trace->realfraction = 1;
	VectorCopy( end, trace->endpos );

	Matrix4x4_Transform( inversematrix, start, starttransformed );
	Matrix4x4_Transform( inversematrix, end, endtransformed );

	if( model && model->TraceBox )
		model->TraceBox( starttransformed, endtransformed, mins, maxs, model, trace, hitsupercontentsmask );
	else CM_CollisionClipTrace_Box( trace, bodymins, bodymaxs, starttransformed, mins, maxs, endtransformed, hitsupercontentsmask, bodysupercontents, 0, NULL );
	trace->fraction = bound( 0, trace->fraction, 1 );
	trace->realfraction = bound( 0, trace->realfraction, 1 );

	if( trace->fraction < 1 )
	{
		VectorLerp( start, trace->fraction, end, trace->endpos );
		VectorCopy( trace->plane.normal, tempnormal );
		Matrix4x4_Transform3x3( matrix, tempnormal, trace->plane.normal );
		// FIXME: should recalc trace->plane.dist
	}
}

void CM_CollisionClipToWorld( trace_t *trace, cmodel_t *model, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contents )
{
	memset( trace, 0, sizeof(*trace));
	trace->fraction = trace->realfraction = 1;

	if( model && model->TraceBox )
		model->TraceBox( start, end, mins, maxs, model, trace, contents );
	trace->fraction = bound( 0, trace->fraction, 1 );
	trace->realfraction = bound( 0, trace->realfraction, 1 );
	VectorLerp( start, trace->fraction, end, trace->endpos );
}

void CM_CollisionCombineTraces( trace_t *cliptrace, const trace_t *trace, edict_t *touch, bool is_bmodel )
{
	// take the 'best' answers from the new trace and combine with existing data
	if( trace->allsolid ) cliptrace->allsolid = true;
	if( trace->startsolid )
	{
		if( is_bmodel ) cliptrace->startstuck = true;
		cliptrace->startsolid = true;
		if( cliptrace->realfraction == 1 )
			cliptrace->ent = touch;
	}

	if( trace->realfraction < cliptrace->realfraction )
	{
		cliptrace->fraction = trace->fraction;
		cliptrace->realfraction = trace->realfraction;
		VectorCopy( trace->endpos, cliptrace->endpos );
		cliptrace->plane = trace->plane;
		cliptrace->ent = touch;
		cliptrace->contents = trace->contents;
		cliptrace->surfaceflags = trace->surfaceflags;
		cliptrace->surface = trace->surface;
	}
	cliptrace->contents |= trace->contents;
}

void CM_CollisionDrawForEachBrush( void )
{
	cbrushf_t	*draw;
	int	i, j, color; 

	if( !ph.debug_line ) return;

	for( i = 0; i < cm.numbmodels; i++ )
	{
		for( j = 0; j < cm.bmodels[i].numbrushes; j++ )
		{
			draw = cm.brushes[cm.bmodels[i].firstbrush + j].colbrushf;
			if( !draw ) continue;
			if( i == 0 ) color = PackRGBA( 1, 0.7f, 0, 1 );	// world
			else color = PackRGBA( 1, 0.1f, 0.1f, 1 );
			ph.debug_line( color, draw->numpoints, (float *)&draw->points->v[0] );
		}
	}
}