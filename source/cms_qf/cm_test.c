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
CM_BrushContents
==================
*/
_inline static int CM_BrushContents( cbrush_t *brush, const vec3_t p )
{
	int		i;
	cbrushside_t	*brushside;

	for( i = 0, brushside = brush->brushsides; i < brush->numsides; i++, brushside++ )
		if( PlaneDiff( p, brushside->plane ) > 0.0f )
			return 0;
	return brush->contents;
}

/*
==================
CM_PatchContents
==================
*/
_inline static int CM_PatchContents( cface_t *patch, const vec3_t p )
{
	int	i, c;
	cbrush_t	*facet;

	for( i = 0, facet = patch->facets; i < patch->numfacets; i++, patch++ )
		if(( c = CM_BrushContents( facet, p )))
			return c;
	return 0;
}

/*
==================
CM_PointContents

==================
*/
int CM_PointContents( const vec3_t p, model_t model )
{
	int	i, superContents, contents;
	int	nummarkfaces, nummarkbrushes;
	cface_t	*patch, **markface;
	cbrush_t	*brush, **markbrush;
	cmodel_t	*cmodel;
	cleaf_t	*leaf;
			
	if( !cm.numnodes ) // map not loaded
		return 0;

	cmodel = CM_ClipHandleToModel( model );

	if( !cmodel || cmodel->type == mod_world )
	{
		leaf = &cm.leafs[CM_PointLeafnum( p )];
		superContents = leaf->contents;

		markbrush = leaf->markbrushes;
		nummarkbrushes = leaf->nummarkbrushes;

		markface = leaf->markfaces;
		nummarkfaces = leaf->nummarkfaces;
	}
	else
	{
		superContents = ~0;

		leaf = &cmodel->leaf;
		superContents = leaf->contents;

		markbrush = leaf->markbrushes;
		nummarkbrushes = leaf->nummarkbrushes;

		markface = leaf->markfaces;
		nummarkfaces = leaf->nummarkfaces;
	}

	contents = superContents;

	for( i = 0; i < nummarkbrushes; i++ )
	{
		brush = markbrush[i];

		// check if brush adds something to contents
		if( contents & brush->contents )
		{
			if(!( contents &= ~CM_BrushContents( brush, p )))
				return superContents;
		}
	}

	if( !cm_nocurves->integer )
	{
		for( i = 0; i < nummarkfaces; i++ )
		{
			patch = markface[i];

			// check if patch adds something to contents
			if( contents & patch->contents )
			{
				if( BoundsIntersect( p, p, patch->mins, patch->maxs ))
				{
					if(!( contents &= ~CM_PatchContents( patch, p )))
						return superContents;
				}
			}
		}
	}
	return ~contents & superContents;
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