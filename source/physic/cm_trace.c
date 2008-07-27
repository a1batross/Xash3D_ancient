//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_trace.c - combined tracing
//=======================================================================

#include "cm_local.h"
#include "cm_utils.h"

// keep 1/8 unit away to keep the position valid before network snapping
// and to avoid various numeric issues
#define DIST_EPSILON		(0.125)	// 1/8 epsilon to keep floating point happy
#define RADIUS_EPSILON		1.0f
#define MAX_POSITION_LEAFS		1024
#define Square(x)			((x)*(x))


/*
===============================================================================

CM INTERNAL MATH

===============================================================================
*/
/*
================
RotatePoint
================
*/
void RotatePoint(vec3_t point, const vec3_t matrix[3])
{
	vec3_t	tvec;

	VectorCopy( point, tvec );
	point[0] = DotProduct( matrix[0], tvec );
	point[1] = DotProduct( matrix[1], tvec );
	point[2] = DotProduct( matrix[2], tvec );
}

/*
================
TransposeMatrix
================
*/
void TransposeMatrix( const vec3_t matrix[3], vec3_t transpose[3])
{
	int i, j;

	for (i = 0; i < 3; i++)
		for( j = 0; j < 3; j++)
			transpose[i][j] = matrix[j][i];
}

/*
================
CreateRotationMatrix
================
*/
void CreateRotationMatrix(const vec3_t angles, vec3_t matrix[3])
{
	AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
	VectorNegate( matrix[1], matrix[1] );
}

/*
================
CM_ProjectPointOntoVector
================
*/
void CM_ProjectPointOntoVector( vec3_t point, vec3_t vStart, vec3_t vDir, vec3_t vProj )
{
	vec3_t pVec;

	VectorSubtract( point, vStart, pVec );
	// project onto the directional vector for this segment
	VectorMA( vStart, DotProduct( pVec, vDir ), vDir, vProj );
}

/*
================
CM_DistanceFromLineSquared
================
*/
float CM_DistanceFromLineSquared(vec3_t p, vec3_t lp1, vec3_t lp2, vec3_t dir)
{
	vec3_t	proj, t;
	int	j;

	CM_ProjectPointOntoVector( p, lp1, dir, proj );
	for (j = 0; j < 3; j++)
	{ 
		if ((proj[j] > lp1[j] && proj[j] > lp2[j]) || (proj[j] < lp1[j] && proj[j] < lp2[j]))
			break;
	}
	if( j < 3 )
	{
		if(fabs(proj[j] - lp1[j]) < fabs(proj[j] - lp2[j]))
		{
			VectorSubtract(p, lp1, t);
		}
		else
		{
			VectorSubtract(p, lp2, t);
		}
		return VectorLength2(t);
	}
	VectorSubtract(p, proj, t);
	return VectorLength2(t);
}

/*
================
CM_VectorDistanceSquared
================
*/
float CM_VectorDistanceSquared( vec3_t p1, vec3_t p2 )
{
	vec3_t dir;

	VectorSubtract(p2, p1, dir);
	return VectorLength2(dir);
}

/*
================
SquareRootFloat
================
*/
float SquareRootFloat( float number )
{
	long		i;
	float		x, y;
	const float	f = 1.5F;

	x = number * 0.5F;
	y  = number;
	i  = *(long*)&y;
	i  = 0x5f3759df - (i>>1);
	y  = *(float *)&i;
	y  = y*(f-(x*y*y));
	y  = y*(f-(x*y*y));

	return number*y;
}

/*
===============================================================================

POSITION TESTING

===============================================================================
*/
/*
================
CM_TestBoxInBrush
================
*/
void CM_TestBoxInBrush( tracework_t *tw, cbrush_t *brush )
{
	cplane_t		*plane;
	float		t, dist, d1;
	cbrushside_t	*side;
	vec3_t		startp;
	int		i;

	if(!brush->numsides) return;

	// special test for axial
	if ( tw->bounds[0][0] > brush->bounds[1][0] || tw->bounds[0][1] > brush->bounds[1][1]
	  || tw->bounds[0][2] > brush->bounds[1][2] || tw->bounds[1][0] < brush->bounds[0][0]
	  || tw->bounds[1][1] < brush->bounds[0][1] || tw->bounds[1][2] < brush->bounds[0][2] )
	{
		return;
	}

	if( tw->sphere.use )
	{
		// the first six planes are the axial planes, so we only
		// need to test the remainder
		for( i = 6; i < brush->numsides; i++ )
		{
			side = &cm.brushsides[brush->firstbrushside + i];
			plane = side->plane;

			// adjust the plane distance apropriately for radius
			dist = plane->dist + tw->sphere.radius;
			// find the closest point on the capsule to the plane
			t = DotProduct( plane->normal, tw->sphere.offset );
			if( t > 0 )
			{
				VectorSubtract( tw->start, tw->sphere.offset, startp );
			}
			else
			{
				VectorAdd( tw->start, tw->sphere.offset, startp );
			}
			d1 = DotProduct( startp, plane->normal ) - dist;
			// if completely in front of face, no intersection
			if( d1 > 0 ) return;
		}
	}
	else
	{
		// the first six planes are the axial planes, so we only
		// need to test the remainder
		for( i = 6; i < brush->numsides; i++ )
		{
			side = &cm.brushsides[brush->firstbrushside + i];
			plane = side->plane;

			// adjust the plane distance apropriately for mins/maxs
			dist = plane->dist - DotProduct( tw->offsets[plane->signbits], plane->normal );
			d1 = DotProduct( tw->start, plane->normal ) - dist;
                    	
			// if completely in front of face, no intersection
			if ( d1 > 0 ) return;
		}
	}

	// inside this brush
	tw->result.startsolid = tw->result.allsolid = true;
	tw->result.fraction = 0;
	tw->result.contents = brush->contents;
}

/*
================
CM_TestInLeaf
================
*/
void CM_TestInLeaf( tracework_t *tw, cleaf_t *leaf )
{
	int		k;
	int		brushnum;
	cbrush_t		*b;

	if(!(leaf->contents & maptrace.contents)) return;
	// test box position against all brushes in the leaf
	for (k = 0; k < leaf->numleafbrushes; k++)
	{
		brushnum = cm.leafbrushes[leaf->firstleafbrush + k];
		b = &cm.brushes[brushnum];
		// already checked this brush in another leaf
		if (b->checkcount == cm.checkcount) continue;
		b->checkcount = cm.checkcount;
		if(!(b->contents & tw->contents)) continue;
		
		CM_TestBoxInBrush( tw, b );
		if( tw->result.allsolid ) return;
	}
}

/*
==================
CM_TestCapsuleInCapsule

capsule inside capsule check
==================
*/
void CM_TestCapsuleInCapsule( tracework_t *tw, cmodel_t *model )
{
	int	i;
	vec3_t	mins, maxs;
	vec3_t	top, bottom;
	vec3_t	p1, p2, tmp;
	vec3_t	offset, symetricSize[2];
	float	radius, halfwidth, halfheight, offs, r;

	CM_ModelBounds( model, mins, maxs );

	VectorAdd( tw->start, tw->sphere.offset, top );
	VectorSubtract( tw->start, tw->sphere.offset, bottom );
	for( i = 0 ; i < 3 ; i++ )
	{
		offset[i] = ( mins[i] + maxs[i] ) * 0.5;
		symetricSize[0][i] = mins[i] - offset[i];
		symetricSize[1][i] = maxs[i] - offset[i];
	}
	halfwidth = symetricSize[1][0];
	halfheight = symetricSize[1][2];
	radius = ( halfwidth > halfheight ) ? halfheight : halfwidth;
	offs = halfheight - radius;

	r = Square(tw->sphere.radius + radius);
	// check if any of the spheres overlap
	VectorCopy(offset, p1);
	p1[2] += offs;
	VectorSubtract(p1, top, tmp);
	if ( VectorLength2(tmp) < r )
	{
		tw->result.startsolid = tw->result.allsolid = true;
		tw->result.fraction = 0;
	}
	VectorSubtract(p1, bottom, tmp);
	if ( VectorLength2(tmp) < r )
	{
		tw->result.startsolid = tw->result.allsolid = true;
		tw->result.fraction = 0;
	}
	VectorCopy(offset, p2);
	p2[2] -= offs;
	VectorSubtract(p2, top, tmp);
	if ( VectorLength2(tmp) < r )
	{
		tw->result.startsolid = tw->result.allsolid = true;
		tw->result.fraction = 0;
	}
	VectorSubtract(p2, bottom, tmp);
	if ( VectorLength2(tmp) < r )
	{
		tw->result.startsolid = tw->result.allsolid = true;
		tw->result.fraction = 0;
	}
	// if between cylinder up and lower bounds
	if((top[2] >= p1[2] && top[2] <= p2[2]) || (bottom[2] >= p1[2] && bottom[2] <= p2[2]))
	{
		top[2] = p1[2] = 0;		// 2d coordinates
		VectorSubtract(top, p1, tmp);	// if the cylinders overlap
		if( VectorLength2(tmp) < r )
		{
			tw->result.startsolid = tw->result.allsolid = true;
			tw->result.fraction = 0;
		}
	}
}

/*
==================
CM_TestBoundingBoxInCapsule

bounding box inside capsule check
==================
*/
void CM_TestBoundingBoxInCapsule( tracework_t *tw, cmodel_t *model )
{
	vec3_t		mins, maxs, offset, size[2];
	cmodel_t		*cmod;
	int 		i;

	// mins maxs of the capsule
	CM_ModelBounds(model, mins, maxs);

	// offset for capsule center
	for( i = 0 ; i < 3 ; i++ )
	{
		offset[i] = ( mins[i] + maxs[i] ) * 0.5;
		size[0][i] = mins[i] - offset[i];
		size[1][i] = maxs[i] - offset[i];
		tw->start[i] -= offset[i];
		tw->end[i] -= offset[i];
	}

	// replace the bounding box with the capsule
	tw->sphere.use = true;
	tw->sphere.radius = ( maxs[0] > maxs[2] ) ? maxs[2]: maxs[0];
	tw->sphere.halfheight = maxs[2];
	VectorSet( tw->sphere.offset, 0, 0, maxs[2] - tw->sphere.radius );

	// replace the capsule with the bounding box
	cmod = CM_TempBoxModel( tw->mins, tw->maxs, false );
	// calculate collision
	CM_TestInLeaf( tw, &cmod->leaf );
}

/*
==================
CM_PositionTest
==================
*/
void CM_PositionTest( tracework_t *tw )
{
	int		leafs[MAX_POSITION_LEAFS];
	int		i;
	leaflist_t	ll;

	// identify the leafs we are touching
	VectorAdd( tw->start, tw->mins, ll.bounds[0] );
	VectorAdd( tw->start, tw->maxs, ll.bounds[1] );

	for( i = 0; i < 3; i++ )
	{
		ll.bounds[0][i] -= 1;
		ll.bounds[1][i] += 1;
	}

	ll.count = 0;
	ll.maxcount = MAX_POSITION_LEAFS;
	ll.list = leafs;
	ll.storeleafs = CM_StoreLeafs;
	ll.lastleaf = 0;
	ll.overflowed = false;
	cm.checkcount++;

	CM_BoxLeafnums_r( &ll, 0 );
	cm.checkcount++;

	// test the contents of the leafs
	for( i = 0; i < ll.count; i++)
	{
		CM_TestInLeaf( tw, &cm.leafs[leafs[i]] );
		if( tw->result.allsolid )
			break;
	}
}

/*
===============================================================================

TRACING

===============================================================================
*/
/*
================
CM_TraceThroughBrush
================
*/
void CM_TraceThroughBrush( tracework_t *tw, cbrush_t *brush )
{
	int		i;
	cplane_t		*plane, *clipplane;
	float		dist;
	float		enterFrac, leaveFrac;
	float		t, f, d1, d2;
	bool		getout, startout;
	cbrushside_t	*side, *leadside;
	vec3_t		startp, endp;

	enterFrac = -1.0;
	leaveFrac = 1.0;
	clipplane = NULL;

	if( !brush->numsides ) return;

	getout = false;
	startout = false;
	leadside = NULL;

	if ( tw->sphere.use )
	{
		// compare the trace against all planes of the brush
		// find the latest time the trace crosses a plane towards the interior
		// and the earliest time the trace crosses a plane towards the exterior
		for( i = 0; i < brush->numsides; i++ )
		{
			side = &cm.brushsides[brush->firstbrushside + i];
			plane = side->plane;

			// adjust the plane distance apropriately for radius
			dist = plane->dist + tw->sphere.radius;

			// find the closest point on the capsule to the plane
			t = DotProduct( plane->normal, tw->sphere.offset );
			if ( t > 0 )
			{
				VectorSubtract( tw->start, tw->sphere.offset, startp );
				VectorSubtract( tw->end, tw->sphere.offset, endp );
			}
			else
			{
				VectorAdd( tw->start, tw->sphere.offset, startp );
				VectorAdd( tw->end, tw->sphere.offset, endp );
			}

			d1 = DotProduct( startp, plane->normal ) - dist;
			d2 = DotProduct( endp, plane->normal ) - dist;

			if( d2 > 0 ) getout = true;	// endpoint is not in solid
			if( d1 > 0 ) startout = true;

			// if completely in front of face, no intersection with the entire brush
			if( d1 > 0 && ( d2 >= DIST_EPSILON || d2 >= d1 ))
				return;

			// if it doesn't cross the plane, the plane isn't relevent
			if( d1 <= 0 && d2 <= 0 ) continue;

			// crosses face
			if( d1 > d2 )
			{	// enter
				f = (d1 - DIST_EPSILON) / (d1-d2);
				if( f < 0 ) f = 0;
				if( f > enterFrac )
				{
					enterFrac = f;
					clipplane = plane;
					leadside = side;
				}
			}
			else
			{	// leave
				f = (d1 + DIST_EPSILON) / (d1-d2);
				if( f > 1 ) f = 1;
				if( f < leaveFrac ) leaveFrac = f;
			}
		}
	}
	else
	{
		// compare the trace against all planes of the brush
		// find the latest time the trace crosses a plane towards the interior
		// and the earliest time the trace crosses a plane towards the exterior
		for (i = 0; i < brush->numsides; i++)
		{
			side = &cm.brushsides[brush->firstbrushside + i];
			plane = side->plane;

			// adjust the plane distance apropriately for mins/maxs
			dist = plane->dist - DotProduct( tw->offsets[ plane->signbits ], plane->normal );

			d1 = DotProduct( tw->start, plane->normal ) - dist;
			d2 = DotProduct( tw->end, plane->normal ) - dist;

			if( d2 > 0 ) getout = true;	// endpoint is not in solid
			if( d1 > 0 ) startout = true;

			// if completely in front of face, no intersection with the entire brush
			if(d1 > 0 && ( d2 >= DIST_EPSILON || d2 >= d1 )  )
				return;

			// if it doesn't cross the plane, the plane isn't relevent
			if( d1 <= 0 && d2 <= 0 ) continue;

			// crosses face
			if( d1 > d2 )
			{
				// enter
				f = (d1 - DIST_EPSILON) / (d1-d2);
				if( f < 0 ) f = 0;
				if( f > enterFrac )
				{
					enterFrac = f;
					clipplane = plane;
					leadside = side;
				}
			}
			else
			{
				// leave
				f = (d1 + DIST_EPSILON) / (d1-d2);
				if( f > 1 ) f = 1;
				if( f < leaveFrac )
				{
					leaveFrac = f;
				}
			}
		}
          }

	// all planes have been checked, and the trace was not
	// completely outside the brush
	if(!startout)
	{	
		// original point was inside brush
		tw->result.startsolid = true;
		if(!getout)
		{
			tw->result.allsolid = true;
			tw->result.fraction = 0;
			tw->result.contents = brush->contents;
		}
		return;
	}
	
	if( enterFrac < leaveFrac )
	{
		if( enterFrac > -1 && enterFrac < tw->result.fraction )
		{
			if( enterFrac < 0 ) enterFrac = 0;
			tw->result.fraction = enterFrac;
			tw->result.plane = *clipplane;
			tw->result.flags = leadside->surface->flags;
			tw->result.surface = leadside->surface;
			tw->result.contents = brush->contents;
		}
	}
}

/*
================
CM_TraceThroughLeaf
================
*/
void CM_TraceThroughLeaf( tracework_t *tw, cleaf_t *leaf )
{
	int		k;
	int		brushnum;
	cbrush_t		*b;

	// trace line against all brushes in the leaf
	for( k = 0; k < leaf->numleafbrushes; k++ )
	{
		brushnum = cm.leafbrushes[leaf->firstleafbrush + k];

		b = &cm.brushes[brushnum];
		// already checked this brush in another leaf
		if( b->checkcount == cm.checkcount ) continue;
		b->checkcount = cm.checkcount;
		if(!(b->contents & tw->contents)) continue;

		CM_TraceThroughBrush( tw, b );
		if( !tw->result.fraction ) return;
	}
}

/*
================
CM_TraceThroughSphere

get the first intersection of the ray with the sphere
================
*/
void CM_TraceThroughSphere( tracework_t *tw, vec3_t origin, float radius, vec3_t start, vec3_t end )
{
	float	l1, l2, length, scale, fraction;
	float	a, b, c, d, sqrtd;
	vec3_t	v1, dir, intersection;

	// if inside the sphere
	VectorSubtract( start, origin, dir );
	l1 = VectorLength2(dir);
	if (l1 < Square(radius))
	{
		tw->result.fraction = 0;
		tw->result.startsolid = true;
		// test for allsolid
		VectorSubtract(end, origin, dir);
		l1 = VectorLength2(dir);
		if(l1 < Square(radius))
		{
			tw->result.allsolid = true;
		}
		return;
	}

	VectorSubtract( end, start, dir );
	length = VectorNormalizeLength( dir );

	l1 = CM_DistanceFromLineSquared(origin, start, end, dir);
	VectorSubtract(end, origin, v1);
	l2 = VectorLength2(v1);

	// if no intersection with the sphere and the end point is at least an epsilon away
	if(l1 >= Square(radius) && l2 > Square(radius + DIST_EPSILON))
		return;

	// | origin - (start + t * dir) | = radius
	// a = dir[0]^2 + dir[1]^2 + dir[2]^2;
	// b = 2 * (dir[0] * (start[0] - origin[0]) + dir[1] * (start[1] - origin[1]) + dir[2] * (start[2] - origin[2]));
	// c = (start[0] - origin[0])^2 + (start[1] - origin[1])^2 + (start[2] - origin[2])^2 - radius^2;

	VectorSubtract( start, origin, v1 );
	a = 1.0f;	// dir is normalized so a = 1
	b = 2.0f * (dir[0] * v1[0] + dir[1] * v1[1] + dir[2] * v1[2]);
	c = v1[0] * v1[0] + v1[1] * v1[1] + v1[2] * v1[2] - (radius + RADIUS_EPSILON) * (radius + RADIUS_EPSILON);

	d = b * b - 4.0f * c;
	if( d > 0 )
	{
		sqrtd = SquareRootFloat( d );
		fraction = (- b - sqrtd) * 0.5f;
		if( fraction < 0 ) fraction = 0;
		else fraction /= length;

		if( fraction < tw->result.fraction )
		{
			tw->result.fraction = fraction;
			VectorSubtract(end, start, dir);
			VectorMA(start, fraction, dir, intersection);
			VectorSubtract( intersection, origin, dir );
			scale = 1 / (radius + RADIUS_EPSILON);
			VectorScale( dir, scale, dir );
			VectorCopy(dir, tw->result.plane.normal);
			VectorAdd( tw->origin, intersection, intersection);
			tw->result.plane.dist = DotProduct(tw->result.plane.normal, intersection);
			tw->result.contents = CONTENTS_MONSTER;
		}
	}
	else if (d == 0)
	{
		// slide along the sphere
	}
	// no intersection at all
}

/*
================
CM_TraceThroughVerticalCylinder

get the first intersection of the ray with the cylinder
the cylinder extends halfheight above and below the origin
================
*/
void CM_TraceThroughVerticalCylinder( tracework_t *tw, vec3_t origin, float radius, float halfheight, vec3_t start, vec3_t end )
{
	float	length, scale, fraction, l1, l2;
	float	a, b, c, d, sqrtd;
	vec3_t	v1, dir, start2d, end2d, org2d, intersection;

	// 2d coordinates
	VectorSet(start2d, start[0], start[1], 0);
	VectorSet(end2d, end[0], end[1], 0);
	VectorSet(org2d, origin[0], origin[1], 0);
	// if between lower and upper cylinder bounds
	if( start[2] <= origin[2] + halfheight && start[2] >= origin[2] - halfheight )
	{
		// if inside the cylinder
		VectorSubtract(start2d, org2d, dir);
		l1 = VectorLength2(dir);
		if( l1 < Square(radius))
		{
			tw->result.fraction = 0;
			tw->result.startsolid = true;
			VectorSubtract(end2d, org2d, dir);
			l1 = VectorLength2(dir);
			if(l1 < Square(radius))
			{
				tw->result.allsolid = true;
			}
			return;
		}
	}

	VectorSubtract( end2d, start2d, dir );
	length = VectorNormalizeLength( dir );

	l1 = CM_DistanceFromLineSquared( org2d, start2d, end2d, dir );
	VectorSubtract(end2d, org2d, v1);
	l2 = VectorLength2(v1);

	// if no intersection with the cylinder and the end point is at least an epsilon away
	if( l1 >= Square(radius) && l2 > Square(radius + DIST_EPSILON))
		return;

	// (start[0] - origin[0] - t * dir[0]) ^ 2 + (start[1] - origin[1] - t * dir[1]) ^ 2 = radius ^ 2
	// (v1[0] + t * dir[0]) ^ 2 + (v1[1] + t * dir[1]) ^ 2 = radius ^ 2;
	// v1[0] ^ 2 + 2 * v1[0] * t * dir[0] + (t * dir[0]) ^ 2 + v1[1] ^ 2 + 2 * v1[1] * t * dir[1] + (t * dir[1]) ^ 2 = radius ^ 2
	// t ^ 2 * (dir[0] ^ 2 + dir[1] ^ 2) + t * (2 * v1[0] * dir[0] + 2 * v1[1] * dir[1]) + v1[0] ^ 2 + v1[1] ^ 2 - radius ^ 2 = 0

	VectorSubtract( start, origin, v1 );
	a = 1.0f; // dir is normalized so we can use a = 1
	b = 2.0f * (v1[0] * dir[0] + v1[1] * dir[1]);
	c = v1[0] * v1[0] + v1[1] * v1[1] - (radius + RADIUS_EPSILON) * (radius + RADIUS_EPSILON);

	d = b * b - 4.0f * c;
	if( d > 0 )
	{
		sqrtd = SquareRootFloat(d);
		fraction = (- b - sqrtd) * 0.5f;// / (2.0f * a);
		if( fraction < 0 ) fraction = 0;
		else fraction /= length;

		if( fraction < tw->result.fraction )
		{
			VectorSubtract( end, start, dir );
			VectorMA( start, fraction, dir, intersection );

			// if the intersection is between the cylinder lower and upper bound
			if( intersection[2] <= origin[2] + halfheight && intersection[2] >= origin[2] - halfheight )
			{
				tw->result.fraction = fraction;
				VectorSubtract(intersection, origin, dir);
				dir[2] = 0;
				scale = 1 / (radius + RADIUS_EPSILON);
				VectorScale( dir, scale, dir );
				VectorCopy(dir, tw->result.plane.normal);
				VectorAdd( tw->origin, intersection, intersection );
				tw->result.plane.dist = DotProduct( tw->result.plane.normal, intersection );
				tw->result.contents = CONTENTS_MONSTER;
			}
		}
	}
	else if( d == 0 )
	{
		// slide along the cylinder
	}
	// no intersection at all
}

/*
================
CM_TraceCapsuleThroughCapsule

capsule vs. capsule collision (not rotated)
================
*/
void CM_TraceCapsuleThroughCapsule( tracework_t *tw, cmodel_t *model )
{
	int	i;
	vec3_t	mins, maxs;
	vec3_t	top, bottom, starttop, startbottom, endtop, endbottom;
	vec3_t	offset, symetricSize[2];
	float	radius, halfwidth, halfheight, offs, h;

	CM_ModelBounds( model, mins, maxs );

	// test trace bounds vs. capsule bounds
	if ( tw->bounds[0][0] > maxs[0] + RADIUS_EPSILON || tw->bounds[0][1] > maxs[1] + RADIUS_EPSILON
	  || tw->bounds[0][2] > maxs[2] + RADIUS_EPSILON || tw->bounds[1][0] < mins[0] - RADIUS_EPSILON
	  || tw->bounds[1][1] < mins[1] - RADIUS_EPSILON || tw->bounds[1][2] < mins[2] - RADIUS_EPSILON )
	{
		return;
	}

	// top origin and bottom origin of each sphere at start and end of trace
	VectorAdd( tw->start, tw->sphere.offset, starttop );
	VectorSubtract( tw->start, tw->sphere.offset, startbottom );
	VectorAdd( tw->end, tw->sphere.offset, endtop );
	VectorSubtract( tw->end, tw->sphere.offset, endbottom );

	// calculate top and bottom of the capsule spheres to collide with
	for( i = 0 ; i < 3 ; i++ )
	{
		offset[i] = ( mins[i] + maxs[i] ) * 0.5f;
		symetricSize[0][i] = mins[i] - offset[i];
		symetricSize[1][i] = maxs[i] - offset[i];
	}

	halfwidth = symetricSize[1][0];
	halfheight = symetricSize[1][2];
	radius = ( halfwidth > halfheight ) ? halfheight : halfwidth;
	offs = halfheight - radius;
	VectorCopy( offset, top );

	top[2] += offs;
	VectorCopy(offset, bottom);
	bottom[2] -= offs;

	// expand radius of spheres
	radius += tw->sphere.radius;
	// if there is horizontal movement
	if( tw->start[0] != tw->end[0] || tw->start[1] != tw->end[1] )
	{
		// height of the expanded cylinder is the height of both cylinders minus the radius of both spheres
		h = halfheight + tw->sphere.halfheight - radius;

		// if the cylinder has a height
		if( h > 0 )
		{
			// test for collisions between the cylinders
			CM_TraceThroughVerticalCylinder(tw, offset, radius, h, tw->start, tw->end);
		}
	}

	// test for collision between the spheres
	CM_TraceThroughSphere( tw, top, radius, startbottom, endbottom );
	CM_TraceThroughSphere( tw, bottom, radius, starttop, endtop );
}

/*
================
CM_TraceBoundingBoxThroughCapsule

bounding box vs. capsule collision
================
*/
void CM_TraceBoundingBoxThroughCapsule( tracework_t *tw, cmodel_t *model )
{
	vec3_t		mins, maxs, offset, size[2];
	cmodel_t 		*cmod;
	int		i;

	// mins maxs of the capsule
	CM_ModelBounds( model, mins, maxs );

	// offset for capsule center
	for( i = 0; i < 3; i++ )
	{
		offset[i] = ( mins[i] + maxs[i] ) * 0.5f;
		size[0][i] = mins[i] - offset[i];
		size[1][i] = maxs[i] - offset[i];
		tw->start[i] -= offset[i];
		tw->end[i] -= offset[i];
	}

	// replace the bounding box with the capsule
	tw->sphere.use = true;
	tw->sphere.radius = ( size[1][0] > size[1][2] ) ? size[1][2]: size[1][0];
	tw->sphere.halfheight = size[1][2];
	VectorSet( tw->sphere.offset, 0, 0, size[1][2] - tw->sphere.radius );

	// replace the capsule with the bounding box
	cmod = CM_TempBoxModel(tw->mins, tw->maxs, false);

	// calculate collision
	CM_TraceThroughLeaf( tw, &cmod->leaf );
}

/*
==================
CM_TraceThroughTree

Traverse all the contacted leafs from the start to the end position.
If the trace is a point, they will be exactly in order, but for larger
trace volumes it is possible to hit something in a later leaf with
a smaller intercept fraction.
==================
*/
void CM_TraceThroughTree( tracework_t *tw, int num, float p1f, float p2f, vec3_t p1, vec3_t p2 )
{
	cnode_t		*node;
	cplane_t		*plane;
	float		t1, t2, offset;
	float		idist, frac, frac2;
	vec3_t		mid;
	int		side;
	float		midf;

	if(tw->result.fraction <= p1f)
		return; // already hit something nearer

	// if < 0, we are in a leaf node
	if (num < 0)
	{
		CM_TraceThroughLeaf( tw, &cm.leafs[-1-num] );
		return;
	}

	//
	// find the point distances to the seperating plane
	// and the offset for the size of the box
	//
	node = cm.nodes + num;
	plane = node->plane;

	// adjust the plane distance apropriately for mins/maxs
	if ( plane->type < 3 )
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = tw->extents[plane->type];
	}
	else
	{
		t1 = DotProduct(plane->normal, p1) - plane->dist;
		t2 = DotProduct(plane->normal, p2) - plane->dist;
		if ( tw->ispoint ) offset = 0;
		else offset = 2048; //WTF ?
	}

	// see which sides we need to consider
	if( t1 >= offset + 1 && t2 >= offset + 1 )
	{
		CM_TraceThroughTree( tw, node->children[0], p1f, p2f, p1, p2 );
		return;
	}
	if( t1 < -offset - 1 && t2 < -offset - 1 )
	{
		CM_TraceThroughTree( tw, node->children[1], p1f, p2f, p1, p2 );
		return;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if( t1 < t2 )
	{
		idist = 1.0/(t1-t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON) * idist;
		frac = (t1 - offset + DIST_EPSILON) * idist;
	}
	else if( t1 > t2 )
	{
		idist = 1.0/(t1-t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON) * idist;
		frac = (t1 + offset + DIST_EPSILON) * idist;
	}
	else
	{
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	if( frac < 0 ) frac = 0;
	if( frac > 1 ) frac = 1;
		
	midf = p1f + (p2f - p1f) * frac;
	mid[0] = p1[0] + frac*(p2[0] - p1[0]);
	mid[1] = p1[1] + frac*(p2[1] - p1[1]);
	mid[2] = p1[2] + frac*(p2[2] - p1[2]);

	CM_TraceThroughTree( tw, node->children[side], p1f, midf, p1, mid );

	// go past the node
	if( frac2 < 0 ) frac2 = 0;
	if( frac2 > 1 ) frac2 = 1;
		
	midf = p1f + (p2f - p1f)*frac2;
	mid[0] = p1[0] + frac2*(p2[0] - p1[0]);
	mid[1] = p1[1] + frac2*(p2[1] - p1[1]);
	mid[2] = p1[2] + frac2*(p2[2] - p1[2]);

	CM_TraceThroughTree( tw, node->children[side^1], midf, p2f, mid, p2 );
}

/*
==================
CM_Trace
==================
*/
void CM_Trace( trace_t *results, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, cmodel_t *mod, const vec3_t origin, int brushmask, bool capsule, sphere_t *sphere )
{
	int		i;
	tracework_t	tw;
	vec3_t		offset;
	
	cm.checkcount++;		// for multi-check avoidance

	// fill in a default trace
	memset( &tw, 0, sizeof(tw) );
	tw.result.fraction = 1;	// assume it goes the entire distance until shown otherwise
	tw.result.surface = &(cm.nullsurface);
	VectorCopy( origin, tw.origin );

	if(!cm.numnodes)
	{
		*results = tw.result;
		return;		// map not loaded, shouldn't happen
	}

	// allow NULL to be passed in for 0,0,0
	if ( !mins ) mins = vec3_origin;
	if ( !maxs ) maxs = vec3_origin;

	// set basic parms
	tw.contents = brushmask;

	// adjust so that mins and maxs are always symetric, which
	// avoids some complications with plane expanding of rotated
	// bmodels
	for ( i = 0 ; i < 3 ; i++ )
	{
		offset[i] = ( mins[i] + maxs[i] ) * 0.5f;
		tw.mins[i] = mins[i] - offset[i];
		tw.maxs[i] = maxs[i] - offset[i];
		tw.start[i] = start[i] + offset[i];
		tw.end[i] = end[i] + offset[i];
	}

	// if a sphere is already specified
	if( sphere )
	{
		tw.sphere = *sphere;
	}
	else
	{
		tw.sphere.use = capsule;
		tw.sphere.radius = ( tw.maxs[0] > tw.maxs[2] ) ? tw.maxs[2]: tw.maxs[0];
		tw.sphere.halfheight = tw.maxs[2];
		VectorSet( tw.sphere.offset, 0, 0, tw.maxs[2] - tw.sphere.radius );
	}

	tw.maxOffset = tw.maxs[0] + tw.maxs[1] + tw.maxs[2];

	// tw.offsets[signbits] = vector to apropriate corner from origin
	tw.offsets[0][0] = tw.mins[0];
	tw.offsets[0][1] = tw.mins[1];
	tw.offsets[0][2] = tw.mins[2];

	tw.offsets[1][0] = tw.maxs[0];
	tw.offsets[1][1] = tw.mins[1];
	tw.offsets[1][2] = tw.mins[2];

	tw.offsets[2][0] = tw.mins[0];
	tw.offsets[2][1] = tw.maxs[1];
	tw.offsets[2][2] = tw.mins[2];

	tw.offsets[3][0] = tw.maxs[0];
	tw.offsets[3][1] = tw.maxs[1];
	tw.offsets[3][2] = tw.mins[2];

	tw.offsets[4][0] = tw.mins[0];
	tw.offsets[4][1] = tw.mins[1];
	tw.offsets[4][2] = tw.maxs[2];

	tw.offsets[5][0] = tw.maxs[0];
	tw.offsets[5][1] = tw.mins[1];
	tw.offsets[5][2] = tw.maxs[2];

	tw.offsets[6][0] = tw.mins[0];
	tw.offsets[6][1] = tw.maxs[1];
	tw.offsets[6][2] = tw.maxs[2];

	tw.offsets[7][0] = tw.maxs[0];
	tw.offsets[7][1] = tw.maxs[1];
	tw.offsets[7][2] = tw.maxs[2];

	// calculate bounds
	if( tw.sphere.use )
	{
		for( i = 0; i < 3; i++ )
		{
			if( tw.start[i] < tw.end[i] )
			{
				tw.bounds[0][i] = tw.start[i] - fabs(tw.sphere.offset[i]) - tw.sphere.radius;
				tw.bounds[1][i] = tw.end[i] + fabs(tw.sphere.offset[i]) + tw.sphere.radius;
			}
			else
			{
				tw.bounds[0][i] = tw.end[i] - fabs(tw.sphere.offset[i]) - tw.sphere.radius;
				tw.bounds[1][i] = tw.start[i] + fabs(tw.sphere.offset[i]) + tw.sphere.radius;
			}
		}
	}
	else
	{
		for ( i = 0 ; i < 3 ; i++ )
		{
			if ( tw.start[i] < tw.end[i] )
			{
				tw.bounds[0][i] = tw.start[i] + tw.mins[i];
				tw.bounds[1][i] = tw.end[i] + tw.maxs[i];
			}
			else
			{
				tw.bounds[0][i] = tw.end[i] + tw.mins[i];
				tw.bounds[1][i] = tw.start[i] + tw.maxs[i];
			}
		}
	}

	// check for position test special case
	if( VectorCompare( start, end ))
	{
		if( mod )
		{
			if( !com.strcmp( mod->name, "*4094" )) // capsule
			{
				if( tw.sphere.use ) CM_TestCapsuleInCapsule( &tw, mod );
				else CM_TestBoundingBoxInCapsule( &tw, mod );
			}
			else CM_TestInLeaf( &tw, &mod->leaf );
		}
		else CM_PositionTest( &tw );
	}
	else
	{
		// check for point special case
		if( VectorIsNull( tw.mins ))
		{
			tw.ispoint = true;
			VectorClear( tw.extents );
		}
		else
		{
			tw.ispoint = false;
			tw.extents[0] = tw.maxs[0];
			tw.extents[1] = tw.maxs[1];
			tw.extents[2] = tw.maxs[2];
		}

		// general sweeping through world
		if( mod )
		{
			if( !com.strcmp( mod->name, "*4094" )) // capsule
			{
				if( tw.sphere.use ) CM_TraceCapsuleThroughCapsule( &tw, mod );
				else CM_TraceBoundingBoxThroughCapsule( &tw, mod );
			}
			else CM_TraceThroughLeaf( &tw, &mod->leaf );
		}
		else CM_TraceThroughTree( &tw, 0, 0, 1, tw.start, tw.end );
	}

	// generate endpos from the original, unmodified start/end
	if ( tw.result.fraction == 1 )
	{
		VectorCopy (end, tw.result.endpos);
	}
	else
	{
		for( i = 0; i < 3; i++ )
		{
			tw.result.endpos[i] = start[i] + tw.result.fraction * (end[i] - start[i]);
		}
	}

	// If allsolid is set (was entirely inside something solid), the plane is not valid.
	// If fraction == 1.0, we never hit anything, and thus the plane is not valid.
	// Otherwise, the normal on the plane should have unit length
	*results = tw.result;
}

/*
===============================================================================

		PUBLIC FUNCTIONS

===============================================================================
*/
/*
==================
CM_BoxTrace
==================
*/
trace_t CM_BoxTrace( const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, cmodel_t *model, int brushmask, bool capsule )
{
	CM_Trace( &cm.trace, start, end, mins, maxs, model, vec3_origin, brushmask, capsule, NULL );
	return cm.trace;
}

/*
==================
CM_TransformedBoxTrace

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
trace_t CM_TransformedBoxTrace( const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, cmodel_t *model, int brushmask, vec3_t origin, vec3_t angles, int capsule )
{
	vec3_t		start_l, end_l;
	vec3_t		offset;
	vec3_t		symetricSize[2];
	vec3_t		matrix[3], transpose[3];
	float		t, halfwidth, halfheight;
	sphere_t		sphere;
	bool		rotated;
	int		i;

	if( !mins ) mins = vec3_origin;
	if( !maxs ) maxs = vec3_origin;

	// adjust so that mins and maxs are always symetric, which
	// avoids some complications with plane expanding of rotated
	// bmodels
	for ( i = 0 ; i < 3 ; i++ )
	{
		offset[i] = ( mins[i] + maxs[i] ) * 0.5f;
		symetricSize[0][i] = mins[i] - offset[i];
		symetricSize[1][i] = maxs[i] - offset[i];
		start_l[i] = start[i] + offset[i];
		end_l[i] = end[i] + offset[i];
	}

	// subtract origin offset
	VectorSubtract( start_l, origin, start_l );
	VectorSubtract( end_l, origin, end_l );

	// rotate start and end into the models frame of reference
	if(com.strcmp( model->name, "*4095" ) && !VectorIsNull( angles )) rotated = true;
	else rotated = false;

	halfwidth = symetricSize[1][0];
	halfheight = symetricSize[1][2];

	sphere.use = capsule;
	sphere.radius = ( halfwidth > halfheight ) ? halfheight : halfwidth;
	sphere.halfheight = halfheight;
	t = halfheight - sphere.radius;

	if( rotated )
	{
		// rotation on trace line (start-end) instead of rotating the bmodel
		// NOTE: This is still incorrect for bounding boxes because the actual bounding
		// box that is swept through the model is not rotated. We cannot rotate
		// the bounding box or the bmodel because that would make all the brush
		// bevels invalid.
		// However this is correct for capsules since a capsule itself is rotated too.
		CreateRotationMatrix( angles, matrix );
		RotatePoint( start_l, matrix );
		RotatePoint( end_l, matrix );

		// rotated sphere offset for capsule
		sphere.offset[0] = matrix[0][2] * t;
		sphere.offset[1] = -matrix[1][2] * t;
		sphere.offset[2] = matrix[2][2] * t;
	}
	else
	{
		VectorSet( sphere.offset, 0, 0, t );
	}

	// sweep the box through the model
	CM_Trace( &cm.trace, start_l, end_l, symetricSize[0], symetricSize[1], model, origin, brushmask, capsule, &sphere );

	// if the bmodel was rotated and there was a collision
	if( rotated && cm.trace.fraction != 1.0 )
	{
		// rotation of bmodel collision plane
		TransposeMatrix( matrix, transpose );
		RotatePoint( cm.trace.plane.normal, transpose );
	}

	// re-calculate the end position of the trace because the trace.endpos
	// calculated by CM_Trace could be rotated and have an offset
	cm.trace.endpos[0] = start[0] + cm.trace.fraction * (end[0] - start[0]);
	cm.trace.endpos[1] = start[1] + cm.trace.fraction * (end[1] - start[1]);
	cm.trace.endpos[2] = start[2] + cm.trace.fraction * (end[2] - start[2]);

	return cm.trace;
}