//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_trace.c - combined tracing
//=======================================================================

#include "cm_local.h"
#include "cm_utils.h"
#include "matrixlib.h"
#include "const.h"

/*
===============================================================================

TRACING

===============================================================================
*/ 
static void CM_TracePoint_r( trace_t *trace, cmodel_t *model, cnode_t *node, const vec3_t p, int markframe )
{
	int	i;
	cleaf_t	*leaf;
	cbrushf_t	*brush;

	// find which leaf the point is in
	while( node->plane )
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct(p, node->plane->normal)) < node->plane->dist];
	// point trace the brushes
	leaf = (cleaf_t *)node;
	for( i = 0; i < leaf->numleafbrushes; i++ )
	{
		brush = cm.brushes[leaf->firstleafbrush[i]].colbrushf;
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
	csurface_t	*surface;
	
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
		brush = cm.brushes[leaf->firstleafbrush[i]].colbrushf;
		if( brush && brush->markframe != markframe && BoxesOverlap( nodesegmentmins, nodesegmentmaxs, brush->mins, brush->maxs ))
		{
			brush->markframe = markframe;
			CM_CollisionTraceLineBrushFloat( trace, linestart, lineend, brush, brush );
		}
	}
	// can't do point traces on curves (they have no thickness)
	if( leaf->havepatches && !VectorCompare( start, end ))
	{
		// line trace the curves
		for( i = 0; i < leaf->numleafsurfaces; i++ )
		{
			surface = cm.shaders + cm.texinfo[cm.surfaces[leaf->firstleafsurface[i]].texinfo].shadernum;
			if( surface->numtriangles && surface->markframe != markframe && BoxesOverlap(nodesegmentmins, nodesegmentmaxs, surface->mins, surface->maxs))
			{
				surface->markframe = markframe;
				CM_CollisionTraceLineTriangleMeshFloat( trace, linestart, lineend, surface->numtriangles, surface->indices, surface->vertices, surface->contentflags, surface->surfaceflags, surface, segmentmins, segmentmaxs );
			}
		}
	}
}

static void CM_TraceBrush_r( trace_t *trace, cmodel_t *model, cnode_t *node, const cbrushf_t *thisbrush_start, const cbrushf_t *thisbrush_end, int markframe, const vec3_t segmentmins, const vec3_t segmentmaxs )
{
	int		i, sides;
	cleaf_t		*leaf;
	cbrushf_t		*brush;
	cplane_t		*plane;
	csurface_t	*surface;
	vec3_t		nodesegmentmins;
	vec3_t		nodesegmentmaxs;

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
	leaf = (cleaf_t *)node;

	for( i = 0; i < leaf->numleafbrushes; i++ )
	{
 		brush = cm.brushes[leaf->firstleafbrush[i]].colbrushf;
		if( brush && brush->markframe != markframe && BoxesOverlap( nodesegmentmins, nodesegmentmaxs, brush->mins, brush->maxs ))
		{
			brush->markframe = markframe;
			CM_CollisionTraceBrushBrushFloat( trace, thisbrush_start, thisbrush_end, brush, brush );
		}
	}
	if( leaf->havepatches )
	{
		for( i = 0; i < leaf->numleafsurfaces; i++ )
		{
			surface = cm.shaders + cm.texinfo[cm.surfaces[leaf->firstleafsurface[i]].texinfo].shadernum;
			if( surface->numtriangles && surface->markframe != markframe && BoxesOverlap( nodesegmentmins, nodesegmentmaxs, surface->mins, surface->maxs ))
			{
				surface->markframe = markframe;
				CM_CollisionTraceBrushTriangleMeshFloat( trace, thisbrush_start, thisbrush_end, surface->numtriangles, surface->indices, surface->vertices, surface->contentflags, surface->surfaceflags, surface, segmentmins, segmentmaxs );
			}
		}
	}
}

void CM_TraceBmodel( const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, cmodel_t *model, trace_t *trace, int contentsmask )
{
	int		i, j;
	float		segmentmins[3], segmentmaxs[3];
	static int	markframe = 0;
	csurface_t	*surface;
	cbrush_t		*brush;

	Mem_Set( trace, 0, sizeof(*trace));
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
				for( i = 0, j = model->firstface; i < model->numfaces; i++, j++ )
				{
					surface = cm.shaders + cm.texinfo[cm.surfaces[j].texinfo].shadernum;
					if( surface->numtriangles ) CM_CollisionTraceLineTriangleMeshFloat( trace, start, end, surface->numtriangles, surface->indices, surface->vertices, surface->contentflags, surface->surfaceflags, surface, segmentmins, segmentmaxs );
				}
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
			for( i = 0, j = model->firstface; i < model->numfaces; i++, j++ )
			{
				surface = cm.shaders + cm.texinfo[cm.surfaces[j].texinfo].shadernum;
				if( surface->numtriangles ) CM_CollisionTraceLineTriangleMeshFloat( trace, start, end, surface->numtriangles, surface->indices, surface->vertices, surface->contentflags, surface->surfaceflags, surface, segmentmins, segmentmaxs );
			}
		}
		else CM_TraceBrush_r( trace, model, cm.nodes, thisbrush_start, thisbrush_end, ++markframe, segmentmins, segmentmaxs );
	}
}

void CM_TraceStudio( const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, cmodel_t *mod, trace_t *trace, int contentsmask )
{
	vec3_t		segmentmins;
	vec3_t		segmentmaxs;
	
	Mem_Set(trace, 0, sizeof(*trace));
	trace->fraction = 1;
	trace->realfraction = 1;
	trace->contentsmask = contentsmask;

	if( VectorLength2(mins) + VectorLength2(maxs) == 0 )
	{
		// line trace
		segmentmins[0] = min(start[0], end[0]) - 1;
		segmentmins[1] = min(start[1], end[1]) - 1;
		segmentmins[2] = min(start[2], end[2]) - 1;
		segmentmaxs[0] = max(start[0], end[0]) + 1;
		segmentmaxs[1] = max(start[1], end[1]) + 1;
		segmentmaxs[2] = max(start[2], end[2]) + 1;
		CM_CollisionTraceLineTriangleMeshFloat( trace, start, end, mod->col[0]->numtris, mod->col[0]->indices, (const float *)mod->col[0]->verts, CONTENTS_SOLID, 0, NULL, segmentmins, segmentmaxs );
	}
	else
	{
		// box trace, performed as brush trace
		cbrushf_t		*thisbrush_start, *thisbrush_end;
		vec3_t		boxstartmins, boxstartmaxs, boxendmins, boxendmaxs;

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
		CM_CollisionTraceBrushTriangleMeshFloat( trace, thisbrush_start, thisbrush_end, mod->col[0]->numtris, mod->col[0]->indices, (const float *)mod->col[0]->verts, CONTENTS_SOLID, 0, NULL, segmentmins, segmentmaxs );
	}
}