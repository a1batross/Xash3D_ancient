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
int CM_PointLeafnum_r( const vec3_t p, int num )
{
	float		d;
	cnode_t		*node;
	cplane_t		*plane;

	while(num >= 0)
	{
		node = cm.nodes + num;
		plane = node->plane;
		
		if (plane->type < 3)d = p[plane->type] - plane->dist;
		else d = DotProduct (plane->normal, p) - plane->dist;
		if (d < 0) num = node->children[1];
		else num = node->children[0];
	}
	return -1 - num;
}

int CM_PointLeafnum( const vec3_t p )
{
	// map not loaded
	if ( !cm.numnodes ) return 0;
	return CM_PointLeafnum_r(p, 0);
}


/*
======================================================================

LEAF LISTING

======================================================================
*/
void CM_StoreLeafs( leaflist_t *ll, int nodenum )
{
	int	leafNum = -1 - nodenum;

	// store the lastLeaf even if the list is overflowed
	if ( cm.leafs[leafNum].cluster != -1 )
	{
		ll->lastleaf = leafNum;
	}

	if( ll->count >= ll->maxcount)
	{
		ll->overflowed = true;
		return;
	}
	ll->list[ll->count++] = leafNum;
}

void CM_StoreBrushes( leaflist_t *ll, int nodenum )
{
	int		i, k;
	int		leafnum;
	int		brushnum;
	cleaf_t		*leaf;
	cbrush_t		*b;

	leafnum = -1 - nodenum;

	leaf = &cm.leafs[leafnum];

	for ( k = 0 ; k < leaf->numleafbrushes; k++ )
	{
		brushnum = cm.leafbrushes[leaf->firstleafbrush + k];
		b = &cm.brushes[brushnum];

		// already checked this brush in another leaf
		if( b->checkcount == cm.checkcount ) continue;
		b->checkcount = cm.checkcount;
		for( i = 0; i < 3; i++ )
		{
			if( b->bounds[0][i] >= ll->bounds[1][i] || b->bounds[1][i] <= ll->bounds[0][i] )
				break;
		}
		if( i != 3 ) continue;
		if( ll->count >= ll->maxcount )
		{
			ll->overflowed = true;
			return;
		}
		((cbrush_t **)ll->list)[ll->count++] = b;
	}
}

/*
=============
CM_BoxLeafnums

Fills in a list of all the leafs touched
=============
*/
void CM_BoxLeafnums_r( leaflist_t *ll, int nodenum )
{
	cplane_t		*plane;
	cnode_t		*node;
	int		s;

	while( 1 )
	{
		if(nodenum < 0)
		{
			ll->storeleafs( ll, nodenum );
			return;
		}
	
		node = &cm.nodes[nodenum];
		plane = node->plane;
		s = BoxOnPlaneSide( ll->bounds[0], ll->bounds[1], plane );
		if (s == 1)
		{
			nodenum = node->children[0];
		}
		else if (s == 2)
		{
			nodenum = node->children[1];
		}
		else
		{
			// go down both
			CM_BoxLeafnums_r( ll, node->children[0] );
			nodenum = node->children[1];
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

	CM_BoxLeafnums_r( &ll, 0 );

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
	int		leafnum;
	int		i, k;
	int		brushnum;
	cleaf_t		*leaf;
	cbrush_t		*b;
	cbrushside_t	*side;
	int		contents;
	float		d;

	if(!cm.numnodes) return 0; // map not loaded
	if( model && model->type == mod_brush )
	{
		// ignore studio models
		leaf = &model->leaf;
	}
	else
	{
		leafnum = CM_PointLeafnum_r (p, 0);
		leaf = &cm.leafs[leafnum];
	}

	contents = 0;
	for( k = 0; k < leaf->numleafbrushes; k++)
	{
		brushnum = cm.leafbrushes[leaf->firstleafbrush + k];
		b = &cm.brushes[brushnum];

		// see if the point is in the brush
		for( i = 0; i < b->numsides; i++ )
		{
			side = &cm.brushsides[b->firstbrushside + i];
			d = DotProduct( p, side->plane->normal );
			if( d > side->plane->dist ) break;
		}
		if( i == b->numsides ) contents |= b->contents;
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