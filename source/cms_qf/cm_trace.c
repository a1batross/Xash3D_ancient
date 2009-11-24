//=======================================================================
//			Copyright XashXT Group 2009 ©
//			cm_trace.c - geometry tracing
//=======================================================================

#include "cm_local.h"
#include "mathlib.h"
#include "matrix_lib.h"

/*
===============================================================================

BOX TRACING

===============================================================================
*/
// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON	(1.0f / 32.0f)
#define FRAC_EPSILON	(1.0f / 1024.0f)

vec3_t	trace_start, trace_end;
vec3_t	trace_mins, trace_maxs;
vec3_t	trace_startmins, trace_endmins;
vec3_t	trace_startmaxs, trace_endmaxs;
vec3_t	trace_absmins, trace_absmaxs;
vec3_t	trace_extents;
trace_t	*trace_trace;
float	trace_realfraction;
int	trace_contents;
bool	trace_ispoint;	// optimized case

/*
================
CM_ClipBoxToBrush
================
*/
void CM_ClipBoxToBrush( cbrush_t *brush )
{
	cplane_t		*p, *clipplane;
	float		enterfrac, leavefrac, distfrac;
	float		d, d1, d2, f;
	bool		getout, startout;
	cbrushside_t	*side, *leadside;
	int		i;

	if( !brush->numsides )
		return;

	enterfrac = -1;
	leavefrac = 1;
	clipplane = NULL;

	getout = false;
	startout = false;
	leadside = NULL;
	side = brush->brushsides;

	for( i = 0; i < brush->numsides; i++, side++ )
	{
		p = side->plane;

		// push the plane out apropriately for mins/maxs
		if( p->type < 3 )
		{
			d1 = trace_startmins[p->type] - p->dist;
			d2 = trace_endmins[p->type] - p->dist;
		}
		else
		{
			switch( p->signbits )
			{
			case 0:
				d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] - p->dist;
				d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmins[2] - p->dist;
				break;
			case 1:
				d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] - p->dist;
				d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmins[2] - p->dist;
				break;
			case 2:
				d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] - p->dist;
				d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmins[2] - p->dist;
				break;
			case 3:
				d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] - p->dist;
				d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmins[2] - p->dist;
				break;
			case 4:
				d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
				d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
				break;
			case 5:
				d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
				d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
				break;
			case 6:
				d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
				d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
				break;
			case 7:
				d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
				d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
				break;
			default:
				d1 = d2 = 0; // shut up compiler
				break;
			}
		}

		if( d2 > 0 ) getout = true;	// endpoint is not in solid
		if( d1 > 0 ) startout = true;

		// if completely in front of face, no intersection
		if( d1 > 0 && d2 >= d1 )
			return;
		if( d1 <= 0 && d2 <= 0 )
			continue;

		// crosses face
		d = 1 / (d1 - d2);
		f = d1 * d;
		if( d > 0 )
		{	
			// enter
			if( f > enterfrac )
			{
				distfrac = d;
				enterfrac = f;
				clipplane = p;
				leadside = side;
			}
		}
		else if( d < 0 )
		{	
			// leave
			if( f < leavefrac )
				leavefrac = f;
		}
	}

	if( !startout )
	{
		// original point was inside brush
		trace_trace->fStartSolid = true;
		if( !getout ) trace_trace->fAllSolid = true;
		return;
	}

	if( enterfrac - FRAC_EPSILON <= leavefrac )
	{
		if( enterfrac > -1 && enterfrac < trace_realfraction )
		{
			if( enterfrac < 0 ) enterfrac = 0;
			trace_realfraction = enterfrac;
			VectorCopy( clipplane->normal, trace_trace->vecPlaneNormal );
			trace_trace->flPlaneDist = clipplane->dist;
			trace_trace->pTexName = cm.shaders[leadside->shadernum].name;
			trace_trace->iContents = brush->contents;
			trace_trace->flFraction = enterfrac - DIST_EPSILON * distfrac;
		}
	}
}

/*
================
CM_TestBoxInBrush
================
*/
void CM_TestBoxInBrush( cbrush_t *brush )
{
	int		i;
	cplane_t		*p;
	cbrushside_t	*side;

	if( !brush->numsides )
		return;

	side = brush->brushsides;
	for( i = 0; i < brush->numsides; i++, side++ )
	{
		p = side->plane;

		// push the plane out apropriately for mins/maxs
		// if completely in front of face, no intersection
		if( p->type < 3 )
		{
			if( trace_startmins[p->type] > p->dist )
				return;
		}
		else
		{
			switch( p->signbits )
			{
			case 0:
				if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] > p->dist )
					return;
				break;
			case 1:
				if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] > p->dist )
					return;
				break;
			case 2:
				if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] > p->dist )
					return;
				break;
			case 3:
				if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] > p->dist )
					return;
				break;
				case 4:
				if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
					return;
				break;
			case 5:
				if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
					return;
				break;
			case 6:
				if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
					return;
				break;
			case 7:
				if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
					return;
				break;
			default:	return;
			}
		}
	}

	// inside this brush
	trace_realfraction = 0.0f;
	trace_trace->fStartSolid = trace_trace->fAllSolid = true;
	trace_trace->iContents = brush->contents;
}

/*
================
CM_CollideBox
================
*/
static void CM_CollideBox( cbrush_t **markbrushes, int nummarkbrushes, cface_t **markfaces, int nummarkfaces, void (*func)(cbrush_t *b) )
{
	int	i, j;
	cbrush_t	*b;
	cface_t	*patch;
	cbrush_t	*facet;

	// trace line against all brushes
	for( i = 0; i < nummarkbrushes; i++ )
	{
		b = markbrushes[i];
		if( b->checkcount == cms.checkcount )
			continue;	// already checked this brush
		b->checkcount = cms.checkcount;
		if(!( b->contents & trace_contents ))
			continue;
		if( func ) func( b );
		if( !trace_realfraction )
			return;
	}

	if( cm_nocurves->integer || !nummarkfaces )
		return;

	// trace line against all patches
	for( i = 0; i < nummarkfaces; i++ )
	{
		patch = markfaces[i];
		if( patch->checkcount == cms.checkcount )
			continue;	// already checked this patch
		patch->checkcount = cms.checkcount;

		if(!( patch->contents & trace_contents ))
			continue;

		if( !BoundsIntersect( patch->mins, patch->maxs, trace_absmins, trace_absmaxs ))
			continue;

		facet = patch->facets;
		for( j = 0; j < patch->numfacets; j++, facet++ )
		{
			if( func ) func( facet );
			if( !trace_realfraction )
				return;
		}
	}
}

/*
================
CM_ClipBox
================
*/
static _inline void CM_ClipBox( cbrush_t **markbrushes, int nummarkbrushes, cface_t **markfaces, int nummarkfaces )
{
	CM_CollideBox( markbrushes, nummarkbrushes, markfaces, nummarkfaces, CM_ClipBoxToBrush );
}

/*
================
CM_TestBox
================
*/
static _inline void CM_TestBox( cbrush_t **markbrushes, int nummarkbrushes, cface_t **markfaces, int nummarkfaces )
{
	CM_CollideBox( markbrushes, nummarkbrushes, markfaces, nummarkfaces, CM_TestBoxInBrush );
}

/*
==================
CM_RecursiveHullCheck
==================
*/
void CM_RecursiveHullCheck( int num, float p1f, float p2f, const vec3_t p1, const vec3_t p2 )
{
	cnode_t	*node;
	cplane_t	*plane;
	int	side;
	float	t1, t2, offset;
	float	frac, frac2;
	float	idist, midf;
	vec3_t	mid;
loc0:
	if( trace_realfraction <= p1f )
		return; // already hit something nearer

	// if < 0, we are in a leaf node
	if( num < 0 )
	{
		cleaf_t	*leaf;

		leaf = &cm.leafs[-1 - num];
		if(!( leaf->contents & BASECONT_SOLID ))
		{
			if( leaf->contents == BASECONT_NONE )
				trace_trace->fInOpen = true;
			else trace_trace->fInWater = true;
		}
		if( leaf->contents & trace_contents )
			CM_ClipBox( leaf->markbrushes, leaf->nummarkbrushes, leaf->markfaces, leaf->nummarkfaces );
		return;
	}

	// find the point distances to the seperating plane
	// and the offset for the size of the box
	node = cm.nodes + num;
	plane = node->plane;

	if( plane->type < 3 )
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = trace_extents[plane->type];
	}
	else
	{
		t1 = DotProduct( plane->normal, p1 ) - plane->dist;
		t2 = DotProduct( plane->normal, p2 ) - plane->dist;
		offset = 0.0f;

		if( !trace_ispoint ) 
		{
			offset += fabs( trace_extents[0] * plane->normal[0] );
			offset += fabs( trace_extents[1] * plane->normal[1] );
			offset += fabs( trace_extents[2] * plane->normal[2] );
		}
	}

	// see which sides we need to consider
	if( t1 >= offset && t2 >= offset )
	{
		num = node->children[0];
		goto loc0;
	}
	if( t1 < -offset && t2 < -offset )
	{
		num = node->children[1];
		goto loc0;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if( t1 < t2 )
	{
		idist = 1.0 / (t1 - t2);
		side = 1;
		frac2 = (t1 + offset) * idist;
		frac = (t1 - offset) * idist;
	}
	else if( t1 > t2 )
	{
		idist = 1.0 / (t1 - t2);
		side = 0;
		frac2 = (t1 - offset) * idist;
		frac = (t1 + offset) * idist;
	}
	else
	{
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	frac = bound( 0.0f, frac, 1.0f );
	midf = p1f + (p2f - p1f) * frac;
	VectorLerp( p1, frac, p2, mid );
	CM_RecursiveHullCheck( node->children[side], p1f, midf, p1, mid );

	// go past the node
	frac2 = bound( 0.0f, frac2, 1.0f );
	midf = p1f + (p2f - p1f) * frac2;
	VectorLerp( p1, frac2, p2, mid );
	CM_RecursiveHullCheck( node->children[side^1], midf, p2f, mid, p2 );
}



//======================================================================

/*
==================
CM_BoxTrace
==================
*/
void CM_BoxTrace( trace_t *tr, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, model_t handle, int mask, trType_t type )
{
	bool	notworld;
	cmodel_t	*cmodel;

	if( !tr ) return;

	cmodel = CM_ClipHandleToModel( handle );
	notworld = (cmodel && ( cmodel->type != mod_world ));

	cms.checkcount++;		// for multi-check avoidance

	// fill in a default trace
	Mem_Set( tr, 0, sizeof( *tr ));
	tr->flFraction = trace_realfraction = 1.0f;
	tr->iHitgroup = -1;

	if( !cm.numnodes )	// map not loaded
		return;

	trace_trace = tr;
	trace_contents = mask;
	VectorCopy( start, trace_start );
	VectorCopy( end, trace_end );
	VectorCopy( mins, trace_mins );
	VectorCopy( maxs, trace_maxs );

	// build a bounding box of the entire move
	ClearBounds( trace_absmins, trace_absmaxs );

	VectorAdd( start, trace_mins, trace_startmins );
	AddPointToBounds( trace_startmins, trace_absmins, trace_absmaxs );

	VectorAdd( start, trace_maxs, trace_startmaxs );
	AddPointToBounds( trace_startmaxs, trace_absmins, trace_absmaxs );

	VectorAdd( end, trace_mins, trace_endmins );
	AddPointToBounds( trace_endmins, trace_absmins, trace_absmaxs );

	VectorAdd( end, trace_maxs, trace_endmaxs );
	AddPointToBounds( trace_endmaxs, trace_absmins, trace_absmaxs );

	// check for position test special case
	if( VectorCompare( start, end ))
	{
		int	leafs[1024];
		int	i, numleafs;
		vec3_t	c1, c2;
		int	topnode;
		cleaf_t	*leaf;

		if( notworld )
		{
			if( BoundsIntersect( cmodel->mins, cmodel->maxs, trace_absmins, trace_absmaxs ))
				CM_TestBox( cmodel->leaf.markbrushes, cmodel->leaf.nummarkbrushes, cmodel->leaf.markfaces, cmodel->leaf.nummarkfaces );
		}
		else
		{
			for( i = 0; i < 3; i++ )
			{
				c1[i] = start[i] + mins[i] - 1;
				c2[i] = start[i] + maxs[i] + 1;
			}

			numleafs = CM_BoxLeafnums( c1, c2, leafs, 1024, &topnode );
			for( i = 0; i < numleafs; i++ )
			{
				leaf = &cm.leafs[leafs[i]];

				if( leaf->contents & trace_contents )
				{
					CM_TestBox( leaf->markbrushes, leaf->nummarkbrushes, leaf->markfaces, leaf->nummarkfaces );
					if( tr->fAllSolid ) break;
				}
			}
		}

		tr->flFraction = trace_realfraction;
		VectorCopy( start, tr->vecEndPos );
		return;
	}

	//
	// check for point special case
	//
	if( VectorIsNull( mins ) && VectorIsNull( maxs ))
	{
		trace_ispoint = true;
		VectorClear( trace_extents );
	}
	else
	{
		trace_ispoint = false;
		trace_extents[0] = -mins[0] > maxs[0] ? -mins[0] : maxs[0];
		trace_extents[1] = -mins[1] > maxs[1] ? -mins[1] : maxs[1];
		trace_extents[2] = -mins[2] > maxs[2] ? -mins[2] : maxs[2];
	}

	// general sweeping through world
	if( !notworld ) CM_RecursiveHullCheck( 0.0f, 0.0f, 1.0f, start, end );
	else if( BoundsIntersect( cmodel->mins, cmodel->maxs, trace_absmins, trace_absmaxs ))
		CM_ClipBox( cmodel->leaf.markbrushes, cmodel->leaf.nummarkbrushes, cmodel->leaf.markfaces, cmodel->leaf.nummarkfaces );

	tr->flFraction = bound( 0.0f, tr->flFraction, 1.0f );
	if( tr->flFraction == 1.0f ) VectorCopy( end, tr->vecEndPos );
	else VectorLerp( start, tr->flFraction, end, tr->vecEndPos );
}


/*
==================
CM_TransformedBoxTrace

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
void CM_TransformedBoxTrace( trace_t *tr, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, model_t handle, int mask, const vec3_t origin, const vec3_t angles, trType_t type )
{
	vec3_t	start_l, end_l;
	vec3_t	a, temp;
	vec3_t	axis[3];
	bool	rotated;
	cmodel_t	*cmodel;

	if( !tr ) return;

	// subtract origin offset
	VectorSubtract( start, origin, start_l );
	VectorSubtract( end, origin, end_l );
	cmodel = CM_ClipHandleToModel( handle );

	// rotate start and end into the models frame of reference
	if( cmodel && (cmodel->type != mod_world) && !VectorIsNull( angles ))
		rotated = true;
	else rotated = false;

	if( rotated )
	{
		Matrix3x3_FromAngles( angles, axis );

		VectorCopy( start_l, temp );
		Matrix3x3_TransformVector( axis, temp, start_l );

		VectorCopy( end_l, temp );
		Matrix3x3_TransformVector( axis, temp, end_l );
	}

	// sweep the box through the model
	CM_BoxTrace( tr, start_l, end_l, mins, maxs, handle, mask, type );

	if( rotated && tr->flFraction != 1.0 )
	{
		VectorNegate( angles, a );
		Matrix3x3_FromAngles( a, axis );

		VectorCopy( tr->vecPlaneNormal, temp );
		Matrix3x3_TransformVector( axis, temp, tr->vecPlaneNormal );
	}

	if( tr->flFraction == 1 ) VectorCopy( end, tr->vecEndPos );
	else VectorLerp( start, tr->flFraction, end, tr->vecEndPos );
}