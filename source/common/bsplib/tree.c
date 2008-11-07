//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	  tree.c - node bsp tree
//=======================================================================

#include "bsplib.h"
#include "const.h"

node_t *NodeForPoint( node_t *node, vec3_t origin )
{
	plane_t	*plane;
	vec_t	d;

	while( node->planenum != PLANENUM_LEAF )
	{
		plane = &mapplanes[node->planenum];
		d = DotProduct( origin, plane->normal ) - plane->dist;
		if( d >= 0 ) node = node->children[0];
		else node = node->children[1];
	}
	return node;
}

/*
=============
FreeTreePortals_r
=============
*/
void FreeTreePortals_r( node_t *node )
{
	portal_t	*p, *nextp;
	int	s;

	// free children
	if( node->planenum != PLANENUM_LEAF )
	{
		FreeTreePortals_r( node->children[0] );
		FreeTreePortals_r( node->children[1] );
	}

	// free portals
	for( p = node->portals; p; p = nextp )
	{
		s = ( p->nodes[1] == node );
		nextp = p->next[s];

		RemovePortalFromNode( p, p->nodes[!s] );
		FreePortal( p );
	}
	node->portals = NULL;
}

/*
=============
FreeTree_r
=============
*/
void FreeTree_r( node_t *node )
{
	// free children
	if( node->planenum != PLANENUM_LEAF )
	{
		FreeTree_r( node->children[0] );
		FreeTree_r( node->children[1] );
	}

	// free bspbrushes
	FreeBrushList( node->brushlist );

	// free the node
	if( node->volume )
		FreeBrush( node->volume );
	BSP_Free( node );
}


/*
=============
FreeTree
=============
*/
void FreeTree( tree_t *tree )
{
	FreeTreePortals_r( tree->headnode );
	FreeTree_r( tree->headnode );
	BSP_Free( tree );
}