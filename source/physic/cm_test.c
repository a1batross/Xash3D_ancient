//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_trace.c - combined tracing
//=======================================================================

#include "cm_local.h"
#include "matrixlib.h"

static float RayCastPlacement(const NewtonBody* body, const float* normal, int collisionID, void* userData, float intersetParam )
{
	float	*paramPtr;

	paramPtr = (float *)userData;
	paramPtr[0] = intersetParam;
	return intersetParam;
}


// find floor for character placement
float CM_FindFloor( vec3_t p0, float maxDist )
{
	vec3_t	p1;
	float	floor_dist = 1.2f;

	VectorCopy( p0, p1 ); 
	p1[1] -= maxDist;

	// shot a vertical ray from a high altitude and collected the intersection parameter.
	NewtonWorldRayCast( gWorld, &p0[0], &p1[0], RayCastPlacement, &floor_dist, NULL );

	// the intersection is the interpolated value
	return p0[1] - maxDist * floor_dist;
}

/*
==================
CM_PointLeafnum_r

==================
*/
int CM_PointLeafnum_r( const vec3_t p, cnode_t *node )
{
	cleaf_t		*leaf;

	// find which leaf the point is in
	while( node->plane )
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct(p, node->plane->normal)) < node->plane->dist];
	leaf = (cleaf_t *)node;

	return leaf - cm.leafs;
}

int CM_PointLeafnum( const vec3_t p )
{
	// map not loaded
	if ( !cm.numnodes ) return 0;
	return CM_PointLeafnum_r( p, cm.nodes );
}


/*
======================================================================

LEAF LISTING

======================================================================
*/
void CM_StoreLeafs( leaflist_t *ll, cnode_t *node )
{
	cleaf_t	*leaf = (cleaf_t *)node;

	// store the lastLeaf even if the list is overflowed
	if( leaf->cluster != -1 )
	{
		ll->lastleaf = leaf - cm.leafs;
	}

	if( ll->count >= ll->maxcount )
	{
		ll->overflowed = true;
		return;
	}
	ll->list[ll->count++] = leaf - cm.leafs;
}

/*
=============
CM_BoxLeafnums

Fills in a list of all the leafs touched
=============
*/
void CM_BoxLeafnums_r( leaflist_t *ll, cnode_t *node )
{
	cplane_t		*plane;
	int		s;

	while( 1 )
	{
		if( node->plane == NULL )
		{
			// it's a leaf!
			ll->storeleafs( ll, node );
			return;
		}
	
		plane = node->plane;
		s = BoxOnPlaneSide( ll->bounds[0], ll->bounds[1], plane );
		if( s == 1 )
		{
			node = node->children[0];
		}
		else if( s == 2 )
		{
			node = node->children[1];
		}
		else
		{
			// go down both
			CM_BoxLeafnums_r( ll, node->children[0] );
			node = node->children[1];
		}
	}
}

/*
==================
CM_BoxLeafnums
==================
*/
int CM_BoxLeafnums( const vec3_t mins, const vec3_t maxs, int *list, int listsize, int *lastleaf )
{
	leaflist_t	ll;

	cm.checkcount++;
	VectorCopy( mins, ll.bounds[0] );
	VectorCopy( maxs, ll.bounds[1] );
	ll.count = 0;
	ll.maxcount = listsize;
	ll.list = list;
	ll.storeleafs = CM_StoreLeafs;
	ll.lastleaf = 0;
	ll.overflowed = false;

	CM_BoxLeafnums_r( &ll, cm.nodes );

	if( lastleaf ) *lastleaf = ll.lastleaf;
	return ll.count;
}

/*
==================
CM_PointContents

==================
*/
int CM_PointContents( const vec3_t p, cmodel_t *model )
{
	int	i, num, contents = 0;
	cbrush_t	*brush;

	if( !cm.numnodes ) return 0; // map not loaded

	// test if the point is inside each brush
	if( model && model->type == mod_brush )
	{
		// submodels are effectively one leaf
		for( i = 0, brush = cm.brushes + model->firstbrush; i < model->numbrushes; i++, brush++ )
			if( brush->colbrushf && CM_CollisionPointInsideBrushFloat( p, brush->colbrushf ))
				contents |= brush->colbrushf->contents;
	}
	else
	{
		cnode_t	*node = cm.nodes;
		cleaf_t	*leaf;
	
		leaf = &cm.leafs[CM_PointLeafnum_r( p, cm.nodes )];

		// now check the brushes in the leaf
		for( i = 0; i < leaf->numleafbrushes; i++ )
		{
			num = cm.leafbrushes[leaf->firstleafbrush + i];
			brush = &cm.brushes[num];

			if( brush->colbrushf && CM_CollisionPointInsideBrushFloat( p, brush->colbrushf ))
				contents |= brush->colbrushf->contents;
		}
	}
	return contents;
}

/*
==================
CM_TransformedPointContents

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
int CM_TransformedPointContents( const vec3_t p, cmodel_t *model, const vec3_t origin, const vec3_t angles )
{
	vec3_t	p_l;
	vec3_t	temp;
	vec3_t	forward, right, up;

	// subtract origin offset
	VectorSubtract( p, origin, p_l );

	// rotate start and end into the models frame of reference
	if(!VectorIsNull( angles ))
	{
		AngleVectors( angles, forward, right, up );
		VectorCopy( p_l, temp );
		p_l[0] = DotProduct( temp, forward );
		p_l[1] = -DotProduct( temp, right );
		p_l[2] = DotProduct( temp, up );
	}
	return CM_PointContents( p_l, model );
}

static void CM_TracePoint_r( trace_t *trace, cmodel_t *model, cnode_t *node, const vec3_t p, int markframe )
{
	int	i;
	cleaf_t	*leaf;
	cbrushf_t	*brush;

	// find which leaf the point is in
	leaf = &cm.leafs[CM_PointLeafnum_r( p, cm.nodes )];
	for( i = 0; i < leaf->numleafbrushes; i++ )
	{
		brush = cm.brushes[leaf->firstleafbrush + i].colbrushf;
		if( brush && brush->markframe != markframe && BoxesOverlap( p, p, brush->mins, brush->maxs))
		{
			brush->markframe = markframe;
			CM_CollisionTracePointBrushFloat( trace, p, brush );
		}
	}
	// can't do point traces on curves (they have no thickness)
}

static void CM_TraceLine_r( trace_t *trace, cmodel_t *model, cnode_t *node, const vec3_t start, const vec3_t end, vec_t startfrac, vec_t endfrac, const vec3_t linestart, const vec3_t lineend, int markframe, const vec3_t segmentmins, const vec3_t segmentmaxs )
{
	int		i, startside, endside;
	float		dist1, dist2, midfrac, mid[3];
	float		nodesegmentmins[3], nodesegmentmaxs[3];
	cleaf_t		*leaf;
	cplane_t		*plane;
	cbrushf_t		*brush;

	// walk the tree until we hit a leaf, recursing for any split cases
	while( node->plane )
	{
		// abort if this part of the bsp tree can not be hit by this trace
		plane = node->plane;
		// axial planes are much more common than non-axial, so an optimized
		// axial case pays off here
		if( plane->type < 3 )
		{
			dist1 = start[plane->type] - plane->dist;
			dist2 = end[plane->type] - plane->dist;
		}
		else
		{
			dist1 = DotProduct( start, plane->normal ) - plane->dist;
			dist2 = DotProduct( end, plane->normal ) - plane->dist;
		}
		startside = dist1 < 0;
		endside = dist2 < 0;
		if( startside == endside )
		{
			// most of the time the line fragment is on one side of the plane
			node = node->children[startside];
		}
		else
		{
			// line crosses node plane, split the line
			dist1 = PlaneDiff(linestart, plane);
			dist2 = PlaneDiff(lineend, plane);
			midfrac = dist1 / (dist1 - dist2);
			VectorLerp(linestart, midfrac, lineend, mid);
			// take the near side first
			CM_TraceLine_r( trace, model, node->children[startside], start, mid, startfrac, midfrac, linestart, lineend, markframe, segmentmins, segmentmaxs );
			// if we found an impact on the front side, don't waste time
			// exploring the far side
			if( midfrac <= trace->realfraction )
				CM_TraceLine_r( trace, model, node->children[endside], mid, end, midfrac, endfrac, linestart, lineend, markframe, segmentmins, segmentmaxs );
			return;
		}
	}

	// hit a leaf
	nodesegmentmins[0] = min(start[0], end[0]) - 1;
	nodesegmentmins[1] = min(start[1], end[1]) - 1;
	nodesegmentmins[2] = min(start[2], end[2]) - 1;
	nodesegmentmaxs[0] = max(start[0], end[0]) + 1;
	nodesegmentmaxs[1] = max(start[1], end[1]) + 1;
	nodesegmentmaxs[2] = max(start[2], end[2]) + 1;

	// line trace the brushes
	leaf = (cleaf_t *)node;

	for( i = 0; i < leaf->numleafbrushes; i++ )
	{
		brush = cm.brushes[leaf->firstleafbrush + i].colbrushf;
		if( brush && brush->markframe != markframe && BoxesOverlap( nodesegmentmins, nodesegmentmaxs, brush->mins, brush->maxs ))
		{
			brush->markframe = markframe;
			CM_CollisionTraceLineBrushFloat( trace, linestart, lineend, brush, brush );
		}
	}
}

static void CM_TraceBrush_r( trace_t *trace, cmodel_t *model, cnode_t *node, const cbrushf_t *thisbrush_start, const cbrushf_t *thisbrush_end, int markframe, const vec3_t segmentmins, const vec3_t segmentmaxs )
{
	int		i, sides;
	cleaf_t		*leaf;
	cbrushf_t		*brush;
	cplane_t		*plane;
	float		nodesegmentmins[3], nodesegmentmaxs[3];

	// walk the tree until we hit a leaf, recursing for any split cases
	while( node->plane )
	{
		// abort if this part of the bsp tree can not be hit by this trace
		plane = node->plane;

		// axial planes are much more common than non-axial, so an optimized
		// axial case pays off here
		if( plane->type < 3 )
		{
			// this is an axial plane, compare bounding box directly to it and
			// recurse sides accordingly
			sides = ((segmentmaxs[plane->type] >= plane->dist) + ((segmentmins[plane->type] < plane->dist) * 2));
		}
		else
		{
			// this is a non-axial plane, so check if the start and end boxes
			// are both on one side of the plane to handle 'diagonal' cases
			sides = BoxOnPlaneSide( thisbrush_start->mins, thisbrush_start->maxs, plane) | BoxOnPlaneSide( thisbrush_end->mins, thisbrush_end->maxs, plane );
		}
		if( sides == 3 )
		{
			// segment crosses plane
			CM_TraceBrush_r( trace, model, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs );
			sides = 2;
		}
		// if sides == 0 then the trace itself is bogus (Not A Number values),
		// in this case we simply pretend the trace hit nothing
		if( sides == 0 ) return; // ERROR: NAN bounding box!
		// take whichever side the segment box is on
		node = node->children[sides - 1];
	}
	// abort if this part of the bsp tree can not be hit by this trace

	nodesegmentmins[0] = max(segmentmins[0], node->mins[0] - 1);
	nodesegmentmins[1] = max(segmentmins[1], node->mins[1] - 1);
	nodesegmentmins[2] = max(segmentmins[2], node->mins[2] - 1);
	nodesegmentmaxs[0] = min(segmentmaxs[0], node->maxs[0] + 1);
	nodesegmentmaxs[1] = min(segmentmaxs[1], node->maxs[1] + 1);
	nodesegmentmaxs[2] = min(segmentmaxs[2], node->maxs[2] + 1);

	// hit a leaf
	leaf = (cleaf_t *)node; //it's right ?

	for( i = 0; i < leaf->numleafbrushes; i++ )
	{
		brush = cm.brushes[leaf->firstleafbrush + i].colbrushf;
		if( brush && brush->markframe != markframe && BoxesOverlap( nodesegmentmins, nodesegmentmaxs, brush->mins, brush->maxs ))
		{
			brush->markframe = markframe;
			CM_CollisionTraceBrushBrushFloat( trace, thisbrush_start, thisbrush_end, brush, brush );
		}
	}
}

void CM_TraceBox( const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, cmodel_t *model, trace_t *trace, int contentsmask )
{
	int		i;
	float		segmentmins[3], segmentmaxs[3];
	static int	markframe = 0;
	cbrush_t		*brush;

	memset( trace, 0, sizeof(*trace));
	trace->fraction = 1;
	trace->realfraction = 1;
	trace->contentsmask = contentsmask;

	if((VectorLength2( mins ) + VectorLength2( maxs)) == 0)
	{
		if(VectorCompare( start, end ))
		{
			// point trace
			if( model && model->type == mod_brush )
			{
				for( i = 0, brush = cm.brushes + model->firstbrush; i < model->numbrushes; i++, brush++ )
					if( brush->colbrushf ) CM_CollisionTracePointBrushFloat( trace, start, brush->colbrushf );
			}
			else CM_TracePoint_r( trace, model, cm.nodes, start, ++markframe );
		}
		else
		{
			// line trace
			segmentmins[0] = min(start[0], end[0]) - 1;
			segmentmins[1] = min(start[1], end[1]) - 1;
			segmentmins[2] = min(start[2], end[2]) - 1;
			segmentmaxs[0] = max(start[0], end[0]) + 1;
			segmentmaxs[1] = max(start[1], end[1]) + 1;
			segmentmaxs[2] = max(start[2], end[2]) + 1;
			if( model && model->type == mod_brush )
			{
				for( i = 0, brush = cm.brushes + model->firstbrush; i < model->numbrushes; i++, brush++ )
					if( brush->colbrushf ) CM_CollisionTraceLineBrushFloat( trace, start, end, brush->colbrushf, brush->colbrushf );
			}
			else CM_TraceLine_r( trace, model, cm.nodes, start, end, 0, 1, start, end, ++markframe, segmentmins, segmentmaxs );
		}
	}
	else
	{
		// box trace, performed as brush trace
		cbrushf_t	*thisbrush_start, *thisbrush_end;
		vec3_t	boxstartmins, boxstartmaxs, boxendmins, boxendmaxs;

		segmentmins[0] = min(start[0], end[0]) + mins[0] - 1;
		segmentmins[1] = min(start[1], end[1]) + mins[1] - 1;
		segmentmins[2] = min(start[2], end[2]) + mins[2] - 1;
		segmentmaxs[0] = max(start[0], end[0]) + maxs[0] + 1;
		segmentmaxs[1] = max(start[1], end[1]) + maxs[1] + 1;
		segmentmaxs[2] = max(start[2], end[2]) + maxs[2] + 1;
		
		VectorAdd(start, mins, boxstartmins);
		VectorAdd(start, maxs, boxstartmaxs);
		VectorAdd(end, mins, boxendmins);
		VectorAdd(end, maxs, boxendmaxs);

		thisbrush_start = CM_CollisionBrushForBox( identitymatrix, boxstartmins, boxstartmaxs, 0, 0, NULL );
		thisbrush_end = CM_CollisionBrushForBox( identitymatrix, boxendmins, boxendmaxs, 0, 0, NULL );

		if( model && model->type == mod_brush )
		{
			for( i = 0, brush = cm.brushes + model->firstbrush; i < model->numbrushes; i++, brush++ )
				if( brush->colbrushf ) CM_CollisionTraceBrushBrushFloat( trace, thisbrush_start, thisbrush_end, brush->colbrushf, brush->colbrushf );
		}
		else CM_TraceBrush_r( trace, model, cm.nodes, thisbrush_start, thisbrush_end, ++markframe, segmentmins, segmentmaxs);
	}
}