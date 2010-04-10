//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_trace.c - combined tracing
//=======================================================================

#include "cm_local.h"
#include "mathlib.h"

/*
==================
CM_PointLeafnum_r

==================
*/
int CM_PointLeafnum_r( const vec3_t p, int num )
{
	float	d;
	cnode_t	*node;
	cplane_t	*plane;

	while( num >= 0 )
	{
		node = cm.nodes + num;
		plane = node->plane;

		if( plane->type < 3 )
			d = p[plane->type] - plane->dist;
		else d = DotProduct( plane->normal, p ) - plane->dist;
		if( d < 0 ) num = node->children[1];
		else num = node->children[0];
	}

	return (-1 - num);
}

int CM_PointLeafnum( const vec3_t p )
{
	// map not loaded
	if ( !cm.numnodes ) return 0;
	return CM_PointLeafnum_r( p, 0 );
}


/*
======================================================================

LEAF LISTING

======================================================================
*/
void CM_StoreLeafs( leaflist_t *ll, int nodenum )
{
	int	leafNum;

	leafNum = (-1 - nodenum);

	// store the lastLeaf even if the list is overflowed
	if( cm.leafs[leafNum].cluster != -1 )
	{
		ll->lastleaf = leafNum;
	}

	if( ll->count >= ll->maxcount )
	{
		ll->overflowed = true;
		return;
	}
	ll->list[ll->count++] = leafNum;
}

/*
=============
CM_BoxLeafnums

Fills in a list of all the leafs touched
=============
*/
void CM_BoxLeafnums_r( leaflist_t *ll, int nodenum )
{
	cplane_t	*plane;
	cnode_t	*node;
	int	s;

	while( 1 )
	{
		if( nodenum < 0 )
		{
			CM_StoreLeafs( ll, nodenum );
			return;
		}

		node = &cm.nodes[nodenum];
		plane = node->plane;
		s = CM_BoxOnPlaneSide( ll->bounds[0], ll->bounds[1], plane );

		if( s == 1 ) nodenum = node->children[0];
		else if( s == 2 ) nodenum = node->children[1];
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

	cms.checkcount++;
	VectorCopy( mins, ll.bounds[0] );
	VectorCopy( maxs, ll.bounds[1] );
	ll.count = 0;
	ll.maxcount = listsize;
	ll.list = list;
	ll.lastleaf = -1;
	ll.overflowed = false;

	CM_BoxLeafnums_r( &ll, 0 );

	if( lastleaf )
		*lastleaf = ll.lastleaf;

	return ll.count;
}

/*
==================
CM_PointContents

==================
*/
int CM_PointContents( const vec3_t p, model_t model )
{
	int		leafnum;
	int		i, k;
	int		brushnum;
	cleaf_t		*leaf;
	cbrush_t		*b;
	int		contents;
	cmodel_t		*clipm;
	float		d;

	if( !cm.numnodes )
		return 0;

	if( model )
	{
		clipm = CM_ClipHandleToModel( model );
		leaf = &clipm->leaf;
	}
	else
	{
		leafnum = CM_PointLeafnum_r( p, 0 );
		leaf = &cm.leafs[leafnum];
	}

	if( leaf->area == -1 )
	{
		// p is in the void and we should return solid so particles can be removed from the void
		return BASECONT_SOLID;
	}

	contents = 0;
	for( k = 0; k < leaf->numleafbrushes; k++ )
	{
		brushnum = cm.leafbrushes[leaf->firstleafbrush + k];
		b = &cm.brushes[brushnum];

		if(!CM_BoundsIntersectPoint( b->bounds[0], b->bounds[1], p ))
			continue;

		// see if the point is in the brush
		for( i = 0; i < b->numsides; i++ )
		{
			d = DotProduct( p, b->sides[i].plane->normal );

			if( d > b->sides[i].plane->dist )
				break;
		}

		if( i == b->numsides )
			contents |= b->contents;
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
int CM_TransformedPointContents( const vec3_t p, model_t model, const vec3_t origin, const vec3_t angles )
{
	vec3_t	p_l;
	vec3_t	temp;
	vec3_t	forward, right, up;

	// subtract origin offset
	VectorSubtract( p, origin, p_l );

	// rotate start and end into the models frame of reference
	if( model != BOX_MODEL_HANDLE && !VectorIsNull( angles ))
	{
		AngleVectors( angles, forward, right, up );
		VectorCopy( p_l, temp );
		p_l[0] = DotProduct( temp, forward );
		p_l[1] = -DotProduct( temp, right );
		p_l[2] = DotProduct( temp, up );
	}
	return CM_PointContents( p_l, model );
}

bool CM_BoxVisible( const vec3_t mins, const vec3_t maxs, byte *visbits )
{
	int	leafList[256];
	int	i, count;

	if( !visbits || !mins || !maxs ) return true;

	// FIXME: Could save a loop here by traversing the tree in this routine like the code above
	count = CM_BoxLeafnums( mins, maxs, leafList, 256, NULL );

	for( i = 0; i < count; i++ )
	{
		int cluster = CM_LeafCluster( leafList[i] );

		if( visbits[cluster>>3] & (1<<(cluster & 7)))
			return true;
	}
	return false;
}