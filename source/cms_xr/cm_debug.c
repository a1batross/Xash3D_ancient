//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    cm_debug.c - draw collision hulls outlines
//=======================================================================

#include "cm_local.h"
#include "mathlib.h"

//#define DEBUG_SIZE
#define DEBUG_BLOCK
//#define DEBUG_BBOX

void CM_DrawCollision( cmdraw_t drawPoly ) 
{ 
	const cSurfaceCollide_t	*pc;
	cfacet_t			*facet;
	cwinding_t		*w;
	int			i, j, k;
	int			curplanenum, planenum;
	int			curinward, inward;
	float			plane[4];
#ifdef DEBUG_SIZE
	vec3_t			mins = { -15, -15, -28 }, maxs = { 15, 15, 28 };
	vec3_t			mins = { 0, 0, 0 }, maxs = { 0, 0, 0 };
	vec3_t			v1, v2;
#endif

	if( !drawPoly ) return;
	if( !debugSurfaceCollide ) return;

	pc = debugSurfaceCollide;

	for( i = 0, facet = pc->facets; i < pc->numFacets; i++, facet++ )
	{
		for( k = 0; k < facet->numBorders + 1; k++ )
		{
			if( k < facet->numBorders )
			{
				planenum = facet->borderPlanes[k];
				inward = facet->borderInward[k];
			}
			else
			{
				planenum = facet->surfacePlane;
				inward = false;
			}

			Vector4Copy( pc->planes[planenum].plane, plane );
			if( inward )
			{
				VectorNegate( plane, plane );
				plane[3] = -plane[3];
			}
#ifdef DEBUG_SIZE
			plane[3] += cm_debugsize->value;

			for( n = 0; n < 3; n++ )
			{
				if( plane[n] > 0 )
					v1[n] = maxs[n];
				else v1[n] = mins[n];
			}

			VectorNegate( plane, v2 );
			plane[3] += fabs( DotProduct( v1, v2 ));
#endif
			w = CM_BaseWindingForPlane( plane, plane[3] );

			for( j = 0; j < facet->numBorders + 1 && w; j++ )
			{
				if( j < facet->numBorders )
				{
					curplanenum = facet->borderPlanes[j];
					curinward = facet->borderInward[j];
				}
				else
				{
					curplanenum = facet->surfacePlane;
					curinward = false;
				}

				if( curplanenum == planenum )
					continue;

				Vector4Copy( pc->planes[curplanenum].plane, plane );
				if( !curinward )
				{
					VectorNegate( plane, plane );
					plane[3] = -plane[3];
				}
#if DEBUG_SIZE
				// if( !facet->borderNoAdjust[j] )
					plane[3] -= cm_debugsize->value;

				for( n = 0; n < 3; n++ )
				{
					if( plane[n] > 0 )
						v1[n] = maxs[n];
					else v1[n] = mins[n];
				}
				VectorNegate( plane, v2 );
				plane[3] -= fabs( DotProduct( v1, v2 ));
#endif
				CM_ChopWindingInPlace( &w, plane, plane[3], 0.1f );
			}

			if( w )
			{
				if( facet == debugFacet )
					drawPoly( 4, w->numpoints, w->p[0] );
				else drawPoly( 1, w->numpoints, w->p[0] );
				CM_FreeWinding( w );
			}
		}
	}

#ifdef DEBUG_BLOCK
	// draw the debug block
	{
		vec3_t          v[3];

		VectorCopy( debugBlockPoints[0], v[0] );
		VectorCopy( debugBlockPoints[1], v[1] );
		VectorCopy( debugBlockPoints[2], v[2] );
		drawPoly( 2, 3, v[0] );

		VectorCopy(debugBlockPoints[2], v[0]);
		VectorCopy(debugBlockPoints[3], v[1]);
		VectorCopy(debugBlockPoints[0], v[2]);
		drawPoly( 2, 3, v[0] );
	}
#endif

#ifdef DEBUG_BBOX
	{
		vec3_t          v[4];

		v[0][0] = pc->bounds[1][0];
		v[0][1] = pc->bounds[1][1];
		v[0][2] = pc->bounds[1][2];

		v[1][0] = pc->bounds[1][0];
		v[1][1] = pc->bounds[0][1];
		v[1][2] = pc->bounds[1][2];

		v[2][0] = pc->bounds[0][0];
		v[2][1] = pc->bounds[0][1];
		v[2][2] = pc->bounds[1][2];

		v[3][0] = pc->bounds[0][0];
		v[3][1] = pc->bounds[1][1];
		v[3][2] = pc->bounds[1][2];

		drawPoly( 2, 4, v[0] );
	}
#endif
}