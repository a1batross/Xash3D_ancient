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

TRACING

===============================================================================
*/
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
void CM_BoxTrace( const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, cmodel_t *model, trace_t *tr, int brushmask )
{
	CM_TraceBox( start, end, mins, maxs, model, tr, brushmask );
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
	}

	// sweep the box through the model
	CM_TraceBox( start_l, end_l, symetricSize[0], symetricSize[1], model, &cm.trace, brushmask );

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