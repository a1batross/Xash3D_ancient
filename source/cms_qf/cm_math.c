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
=================
CategorizePlane

A slightly more complex version of SignbitsForPlane and PlaneTypeForNormal,
which also tries to fix possible floating point glitches (like -0.00000 cases)
=================
*/
void CM_CategorizePlane( cplane_t *plane )
{
	int	i;

	plane->signbits = 0;
	plane->type = PLANE_NONAXIAL;

	for( i = 0; i < 3; i++ )
	{
		if( plane->normal[i] < 0 )
		{
			plane->signbits |= 1<<i;
			if( plane->normal[i] == -1.0f )
			{
				plane->signbits = (1<<i);
				plane->normal[0] = plane->normal[1] = plane->normal[2] = 0;
				plane->normal[i] = -1.0f;
				break;
			}
		}
		else if( plane->normal[i] == 1.0f )
		{
			plane->type = i;
			plane->signbits = 0;
			plane->normal[0] = plane->normal[1] = plane->normal[2] = 0;
			plane->normal[i] = 1.0f;
			break;
		}
	}
}

/*
=====================
CM_PlaneFromPoints

Returns false if the triangle is degenrate.
The normal will point out of the clock for clockwise ordered points
=====================
*/
bool CM_PlaneFromPoints( vec3_t verts[3], cplane_t *plane )
{
	vec3_t	v1, v2;

	VectorSubtract( verts[1], verts[0], v1 );
	VectorSubtract( verts[2], verts[0], v2 );
	CrossProduct( v2, v1, plane->normal );
	if( VectorNormalizeLength( plane->normal ) == 0.0f )
	{
		VectorClear( plane->normal );
		return false;
	}
	plane->dist = DotProduct( verts[0], plane->normal );
	return true;
}

/*
=================
CM_ComparePlanes
=================
*/
bool CM_ComparePlanes( const vec3_t p1normal, float p1dist, const vec3_t p2normal, float p2dist )
{
	if( fabs( p1normal[0] - p2normal[0] ) < PLANE_NORMAL_EPSILON
	 && fabs( p1normal[1] - p2normal[1] ) < PLANE_NORMAL_EPSILON
	 && fabs( p1normal[2] - p2normal[2] ) < PLANE_NORMAL_EPSILON
	 && fabs( p1dist - p2dist ) < PLANE_DIST_EPSILON )
		return true;
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
		if( fabs( normal[i] - 1.0f ) < PLANE_NORMAL_EPSILON )
		{
			VectorClear( normal );
			normal[i] = 1;
			break;
		}
		if( fabs( normal[i] - -1.0f ) < PLANE_NORMAL_EPSILON )
		{
			VectorClear( normal );
			normal[i] = -1;
			break;
		}
	}
}

/*
==============
CM_SnapPlane
==============
*/
void CM_SnapPlane( vec3_t normal, float *dist )
{
	CM_SnapVector( normal );

	if( fabs( *dist - Q_rint( *dist )) < PLANE_DIST_EPSILON )
		*dist = Q_rint( *dist );
}