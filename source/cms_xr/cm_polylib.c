//=======================================================================
//			Copyright XashXT Group 2009 ©
//		        cm_polylib.c - winding processing
//=======================================================================

#include "cm_local.h"
#include "mathlib.h"

/*
=============
AllocWinding
=============
*/
cwinding_t *CM_AllocWinding( int points )
{
	cwinding_t	*w;
	int		s;

	s = sizeof( vec3_t ) * points + sizeof( int );
	w = malloc( s );
	memset( w, 0, s );

	return w;
}

/*
==================
CM_CopyWinding
==================
*/
cwinding_t *CM_CopyWinding( cwinding_t *w )
{
	size_t		size;
	cwinding_t	*c;

	c = CM_AllocWinding( w->numpoints );
	size = (long)((cwinding_t *) 0)->p[w->numpoints];
	Mem_Copy( c, w, size );

	return c;
}

void CM_FreeWinding( cwinding_t *w )
{
	if(*(uint *)w == 0xDEADDEAD )
		Host_Error( "CM_FreeWinding: already freed\n" );
	*(uint *)w = 0xDEADDEAD;

	free( w );
}

/*
=============
CM_WindingBounds
=============
*/
void CM_WindingBounds( cwinding_t *w, vec3_t mins, vec3_t maxs )
{
	float	v;
	int	i, j;

	mins[0] = mins[1] = mins[2] = MAX_WORLD_COORD;
	maxs[0] = maxs[1] = maxs[2] = MIN_WORLD_COORD;

	for( i = 0; i < w->numpoints; i++ )
	{
		for( j = 0; j < 3; j++ )
		{
			v = w->p[i][j];
			if( v < mins[j] ) mins[j] = v;
			if( v > maxs[j] ) maxs[j] = v;
		}
	}
}

/*
=================
CM_BaseWindingForPlane
=================
*/
cwinding_t *CM_BaseWindingForPlane( vec3_t normal, float dist )
{
	int		i, x;
	float		max, v;
	vec3_t		org, vright, vup;
	cwinding_t	*w;

	// find the major axis
	max = -MAX_WORLD_COORD;
	x = -1;
	for( i = 0; i < 3; i++ )
	{
		v = fabs( normal[i] );
		if( v > max )
		{
			x = i;
			max = v;
		}
	}
	if( x == -1 ) Host_Error( "CM_BaseWindingForPlane: no axis found\n" );

	VectorCopy( vec3_origin, vup );
	switch( x )
	{
	case 0:
	case 1:
		vup[2] = 1;
		break;
	case 2:
		vup[0] = 1;
		break;
	}

	v = DotProduct( vup, normal );
	VectorMA( vup, -v, normal, vup );
	VectorNormalize( vup );

	VectorScale( normal, dist, org );

	CrossProduct( vup, normal, vright );

	VectorScale( vup, MAX_WORLD_COORD, vup );
	VectorScale( vright, MAX_WORLD_COORD, vright );

	// project a really big axis aligned box onto the plane
	w = CM_AllocWinding( 4 );

	VectorSubtract( org, vright, w->p[0] );
	VectorAdd( w->p[0], vup, w->p[0] );

	VectorAdd( org, vright, w->p[1] );
	VectorAdd( w->p[1], vup, w->p[1] );

	VectorAdd( org, vright, w->p[2] );
	VectorSubtract( w->p[2], vup, w->p[2] );

	VectorSubtract( org, vright, w->p[3] );
	VectorSubtract( w->p[3], vup, w->p[3] );

	w->numpoints = 4;

	return w;
}

/*
=============
CM_ChopWindingInPlace
=============
*/
void CM_ChopWindingInPlace( cwinding_t **inout, vec3_t normal, float dist, float epsilon )
{
	cwinding_t	*in;
	float		dists[MAX_POINTS_ON_WINDING+4];
	int		sides[MAX_POINTS_ON_WINDING+4];
	int		counts[3];
	static float	dot;		// VC 4.2 optimizer bug if not static
	int		i, j, maxpts;
	float		*p1, *p2;
	vec3_t		mid;
	cwinding_t	*f;

	in = *inout;
	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for( i = 0; i < in->numpoints; i++ )
	{
		dot = DotProduct( in->p[i], normal );
		dot -= dist;
		dists[i] = dot;
		if( dot > epsilon ) sides[i] = SIDE_FRONT;
		else if( dot < -epsilon ) sides[i] = SIDE_BACK;
		else sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];

	if( !counts[0] )
	{
		CM_FreeWinding( in );
		*inout = NULL;
		return;
	}
	if( !counts[1] ) return; // inout stays the same

	maxpts = in->numpoints + 4;	// cant use counts[0]+2 because of fp grouping errors

	f = CM_AllocWinding( maxpts );

	for( i = 0; i < in->numpoints; i++ )
	{
		p1 = in->p[i];

		if( sides[i] == SIDE_ON )
		{
			VectorCopy( p1, f->p[f->numpoints] );
			f->numpoints++;
			continue;
		}

		if( sides[i] == SIDE_FRONT )
		{
			VectorCopy( p1, f->p[f->numpoints] );
			f->numpoints++;
		}

		if( sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i] )
			continue;

		// generate a split point
		p2 = in->p[(i + 1) % in->numpoints];

		dot = dists[i] / (dists[i] - dists[i + 1]);
		for( j = 0; j < 3; j++ )
		{
			// avoid round off error when possible
			if( normal[j] == 1 ) mid[j] = dist;
			else if( normal[j] == -1 ) mid[j] = -dist;
			else mid[j] = p1[j] + dot * (p2[j] - p1[j]);
		}

		VectorCopy( mid, f->p[f->numpoints] );
		f->numpoints++;
	}

	if( f->numpoints > maxpts )
		Host_Error( "CM_ChopWindingInPlace: points exceeded estimate\n" );
	if( f->numpoints > MAX_POINTS_ON_WINDING )
		Host_Error( "CM_ChopWindingInPlace: MAX_POINTS_ON_WINDING limit exceeded\n" );

	CM_FreeWinding( in );
	*inout = f;
}