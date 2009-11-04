//=======================================================================
//			Copyright XashXT Group 2009 ©
//			  cm_math.c - physic mathlib
//=======================================================================

#include "cm_local.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "const.h"

#define LINE_DISTANCE_EPSILON		1e-05f

/*
==============
BoxOnPlaneSide (engine fast version)

Returns SIDE_FRONT, SIDE_BACK, or SIDE_ON
==============
*/
int CM_BoxOnPlaneSide( const vec3_t emins, const vec3_t emaxs, const cplane_t *p )
{
	if( p->type < 3 ) return ((emaxs[p->type] >= p->dist) | ((emins[p->type] < p->dist) << 1));
	switch( p->signbits )
	{
	default:
	case 0: return (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 1: return (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 2: return (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 3: return (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 4: return (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 5: return (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 6: return (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 7: return (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	}
}

/*
====================
CM_BoundsIntersectPoint
====================
*/
bool CM_BoundsIntersectPoint( const vec3_t mins, const vec3_t maxs, const vec3_t point )
{
	if( maxs[0] < point[0] - SURFACE_CLIP_EPSILON )
		return false;
	if( maxs[1] < point[1] - SURFACE_CLIP_EPSILON )
		return false;
	if( maxs[2] < point[2] - SURFACE_CLIP_EPSILON )
		return false;
	if( mins[0] > point[0] + SURFACE_CLIP_EPSILON )
		return false;
	if( mins[1] > point[1] + SURFACE_CLIP_EPSILON )
		return false;
	if( mins[2] > point[2] + SURFACE_CLIP_EPSILON )
		return false;
	return true;
}

/*
====================
CM_BoundsIntersect
====================
*/
bool CM_BoundsIntersect( const vec3_t mins, const vec3_t maxs, const vec3_t mins2, const vec3_t maxs2 )
{
	if( maxs[0] < mins2[0] - SURFACE_CLIP_EPSILON )
		return false;
	if( maxs[1] < mins2[1] - SURFACE_CLIP_EPSILON )
		return false;
	if( maxs[2] < mins2[2] - SURFACE_CLIP_EPSILON )
		return false;
	if( mins[0] > maxs2[0] + SURFACE_CLIP_EPSILON )
		return false;
	if( mins[1] > maxs2[1] + SURFACE_CLIP_EPSILON )
		return false;
	if( mins[2] > maxs2[2] + SURFACE_CLIP_EPSILON )
		return false;
	return true;
}

/*
=====================
CM_PlaneFromPoints

Returns false if the triangle is degenrate.
The normal will point out of the clock for clockwise ordered points
=====================
*/
bool CM_PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c )
{
	vec3_t	d1, d2;

	VectorSubtract( b, a, d1 );
	VectorSubtract( c, a, d2 );
	CrossProduct( d2, d1, plane );
	if( VectorNormalizeLength( plane ) == 0.0f )
		return false;

	plane[3] = DotProduct( a, plane );
	return true;
}

/*
==================
CM_PlaneEqual
==================
*/
bool CM_PlaneEqual( cmplane_t *p, float plane[4], int *flipped )
{
	float	invplane[4];

	if( VectorCompareEpsilon( p->plane, plane, NORMAL_EPSILON ) && fabs( p->plane[3] - plane[3] ) < DIST_EPSILON )
	{
		*flipped = false;
		return true;
	}

	VectorNegate( plane, invplane );
	invplane[3] = -plane[3];

	if( VectorCompareEpsilon( p->plane, invplane, NORMAL_EPSILON ) && fabs( p->plane[3] - invplane[3] ) < DIST_EPSILON )
	{
		*flipped = true;
		return true;
	}
	return false;
}

/*
==================
CM_SnapVector
==================
*/
void CM_SnapVector( vec3_t normal )
{
	int	i;

	for( i = 0; i < 3; i++ )
	{
		if( fabs( normal[i] - 1.0f ) < NORMAL_EPSILON )
		{
			VectorClear( normal );
			normal[i] = 1;
			break;
		}
		if( fabs( normal[i] - -1.0f ) < NORMAL_EPSILON )
		{
			VectorClear( normal );
			normal[i] = -1;
			break;
		}
	}
}