//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_trace.c - combined tracing
//=======================================================================

#include "cm_local.h"

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
			CM_StoreLeafs( ll, node );
			return;
		}
	
		plane = node->plane;
		s = BoxOnPlaneSide( ll->mins, ll->maxs, plane );
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

	cms.checkcount++;
	VectorCopy( mins, ll.mins );
	VectorCopy( maxs, ll.maxs );
	ll.count = 0;
	ll.maxcount = listsize;
	ll.list = list;
	ll.lastleaf = -1;
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
	int	i, contents = 0;
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
	
		// find which leaf the point is in
		while( node->plane )
			node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct(p, node->plane->normal)) < node->plane->dist];
		leaf = (cleaf_t *)node;
		// now check the brushes in the leaf
		for( i = 0; i < leaf->numleafbrushes; i++ )
		{
			brush = cm.brushes + leaf->firstleafbrush[i];
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