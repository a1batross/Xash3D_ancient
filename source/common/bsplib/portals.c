//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	portals.c - make vis portals
//=======================================================================

#include "bsplib.h"
#include "const.h"

int		c_active_portals;
int		c_peak_portals;
int		c_boundary;
int		c_boundary_sides;

#define BASE_WINDING_EPSILON		0.001
#define SIDESPACE			8

/*
===========
AllocPortal
===========
*/
portal_t *AllocPortal( void )
{
	portal_t	*p;
	
	if(GetNumThreads() == 1 )
		c_active_portals++;
	if( c_active_portals > c_peak_portals )
		c_peak_portals = c_active_portals;
	
	p = BSP_Malloc( sizeof( portal_t ));
	
	return p;
}

void FreePortal( portal_t *p )
{
	if( p->winding )
		FreeWinding( p->winding );
	if( GetNumThreads() == 1 )
		c_active_portals--;
	Mem_Free( p );
}

//==============================================================

/*
=============
Portal_Passable

Returns true if the portal has non-opaque leafs on both sides
=============
*/
bool Portal_Passable( portal_t *p )
{
	if( !p->onnode ) return false; // to global outsideleaf
	if( p->nodes[0]->planenum != PLANENUM_LEAF || p->nodes[1]->planenum != PLANENUM_LEAF )
		Sys_Error( "Portal_EntityFlood: not a leaf\n" );

	if( !p->nodes[0]->opaque && !p->nodes[1]->opaque )
		return true;
	return false;
}


//=============================================================================

int	c_tinyportals;

/*
=============
AddPortalToNodes
=============
*/
void AddPortalToNodes( portal_t *p, node_t *front, node_t *back )
{
	if( p->nodes[0] || p->nodes[1] )
		Sys_Error( "AddPortalToNode: allready included\n" );

	p->nodes[0] = front;
	p->next[0] = front->portals;
	front->portals = p;
	
	p->nodes[1] = back;
	p->next[1] = back->portals;
	back->portals = p;
}

/*
=============
RemovePortalFromNode
=============
*/
void RemovePortalFromNode( portal_t *portal, node_t *l )
{
	portal_t	**pp, *t;
	
	// remove reference to the current portal
	pp = &l->portals;
	while( 1 )
	{
		t = *pp;
		if( !t ) Sys_Error( "RemovePortalFromNode: portal not in leaf\n" );	
		if( t == portal ) break;

		if( t->nodes[0] == l )
			pp = &t->next[0];
		else if( t->nodes[1] == l )
			pp = &t->next[1];
		else Sys_Error( "RemovePortalFromNode: portal not bounding leaf\n" );
	}
	
	if( portal->nodes[0] == l )
	{
		*pp = portal->next[0];
		portal->nodes[0] = NULL;
	}
	else if( portal->nodes[1] == l )
	{
		*pp = portal->next[1];	
		portal->nodes[1] = NULL;
	}
}

/*
================
MakeHeadnodePortals

The created portals will face the global outside_node
================
*/
void MakeHeadnodePortals( tree_t *tree )
{
	vec3_t		bounds[2];
	int		i, j, n;
	portal_t		*p, *portals[6];
	plane_t		bplanes[6], *pl;
	node_t		*node;

	node = tree->headnode;

	// pad with some space so there will never be null volume leafs
	for( i = 0; i < 3; i++ )
	{
		bounds[0][i] = tree->mins[i] - SIDESPACE;
		bounds[1][i] = tree->maxs[i] + SIDESPACE;
		if( bounds[0][i] >= bounds[1][i] )
			Sys_Error( "MakeHeadnodePortals: backwards tree mins/maxs\n" );
	}
	
	tree->outside_node.planenum = PLANENUM_LEAF;
	tree->outside_node.brushlist = NULL;
	tree->outside_node.portals = NULL;
	tree->outside_node.opaque = false;

	for( i = 0; i < 3; i++ )
	{
		for( j = 0; j < 2; j++ )
		{
			n = j * 3 + i;

			p = AllocPortal ();
			portals[n] = p;
			
			pl = &bplanes[n];
			memset( pl, 0, sizeof( *pl ));
			if( j )
			{
				pl->normal[i] = -1;
				pl->dist = -bounds[j][i];
			}
			else
			{
				pl->normal[i] = 1;
				pl->dist = bounds[j][i];
			}
			p->plane = *pl;
			p->winding = BaseWindingForPlane( pl->normal, pl->dist );
			AddPortalToNodes( p, node, &tree->outside_node );
		}
	}
		
	// clip the basewindings by all the other planes
	for( i = 0; i < 6; i++ )
	{
		for( j = 0; j < 6; j++ )
		{
			if( j == i ) continue;
			ChopWindingInPlace( &portals[i]->winding, bplanes[j].normal, bplanes[j].dist, ON_EPSILON);
		}
	}
}

//===================================================
/*
================
BaseWindingForNode
================
*/
winding_t	*BaseWindingForNode( node_t *node )
{
	winding_t	*w;
	node_t		*n;
	plane_t		*plane;
	vec3_t		normal;
	vec_t		dist;

	w = BaseWindingForPlane( mapplanes[node->planenum].normal, mapplanes[node->planenum].dist );

	// clip by all the parents
	for( n = node->parent; n && w; )
	{
		plane = &mapplanes[n->planenum];

		if( n->children[0] == node )
		{
			// take front
			ChopWindingInPlace( &w, plane->normal, plane->dist, BASE_WINDING_EPSILON );
		}
		else
		{	// take back
			VectorNegate( plane->normal, normal );
			dist = -plane->dist;
			ChopWindingInPlace( &w, normal, dist, BASE_WINDING_EPSILON );
		}
		node = n;
		n = n->parent;
	}
	return w;
}

//============================================================

/*
==================
MakeNodePortal

create the new portal by taking the full plane winding for the cutting plane
and clipping it by all of parents of this node
==================
*/
void MakeNodePortal( node_t *node )
{
	portal_t		*new_portal, *p;
	winding_t		*w;
	vec3_t		normal;
	float		dist;
	int		side;

	w = BaseWindingForNode( node );

	// clip the portal by all the other portals in the node
	for( p = node->portals; p && w; p = p->next[side] )	
	{
		if( p->nodes[0] == node )
		{
			side = 0;
			VectorCopy( p->plane.normal, normal );
			dist = p->plane.dist;
		}
		else if( p->nodes[1] == node )
		{
			side = 1;
			VectorSubtract( vec3_origin, p->plane.normal, normal);
			dist = -p->plane.dist;
		}
		else Sys_Error( "MakeNodePortal: mislinked portal\n" );
		ChopWindingInPlace( &w, normal, dist, ON_EPSILON );
	}

	if( !w ) return;

	if(WindingIsTiny( w ))
	{
		c_tinyportals++;
		FreeWinding( w );
		return;
	}

	new_portal = AllocPortal();
	new_portal->plane = mapplanes[node->planenum];
	new_portal->onnode = node;
	new_portal->winding = w;
	new_portal->hint = node->hint;
	AddPortalToNodes( new_portal, node->children[0], node->children[1] );
}


/*
==============
SplitNodePortals

Move or split the portals that bound node so that the node's
children have portals instead of node.
==============
*/
void SplitNodePortals( node_t *node )
{
	portal_t		*p, *next_portal, *new_portal;
	node_t		*f, *b, *other_node;
	int		side;
	plane_t		*plane;
	winding_t		*frontwinding, *backwinding;

	plane = &mapplanes[node->planenum];
	f = node->children[0];
	b = node->children[1];

	for( p = node->portals; p; p = next_portal )	
	{
		if( p->nodes[0] == node )
			side = 0;
		else if( p->nodes[1] == node )
			side = 1;
		else Sys_Error( "SplitNodePortals: mislinked portal\n" );
		next_portal = p->next[side];

		other_node = p->nodes[!side];
		RemovePortalFromNode( p, p->nodes[0] );
		RemovePortalFromNode( p, p->nodes[1] );

		// cut the portal into two portals, one on each side of the cut plane
		ClipWindingEpsilon( p->winding, plane->normal, plane->dist, 0.001, &frontwinding, &backwinding );

		if( frontwinding && WindingIsTiny( frontwinding ))
		{
			if( !f->tinyportals )
				VectorCopy( frontwinding->p[0], f->referencepoint );
			f->tinyportals++;
			if( !other_node->tinyportals )
				VectorCopy( frontwinding->p[0], other_node->referencepoint );
			other_node->tinyportals++;

			FreeWinding( frontwinding );
			frontwinding = NULL;
			c_tinyportals++;
		}

		if( backwinding && WindingIsTiny( backwinding ))
		{
			if( !b->tinyportals )
				VectorCopy( backwinding->p[0], b->referencepoint );
			b->tinyportals++;
			if( !other_node->tinyportals )
				VectorCopy( backwinding->p[0], other_node->referencepoint );
			other_node->tinyportals++;

			FreeWinding( backwinding );
			backwinding = NULL;
			c_tinyportals++;
		}

		// tiny windings on both sides
		if( !frontwinding && !backwinding ) continue;

		if( !frontwinding )
		{
			FreeWinding( backwinding );
			if( side == 0 ) AddPortalToNodes( p, b, other_node );
			else AddPortalToNodes( p, other_node, b );
			continue;
		}
		if( !backwinding )
		{
			FreeWinding( frontwinding );
			if( side == 0 ) AddPortalToNodes( p, f, other_node );
			else AddPortalToNodes( p, other_node, f );
			continue;
		}
		
		// the winding is split
		new_portal = AllocPortal ();
		*new_portal = *p;
		new_portal->winding = backwinding;
		FreeWinding( p->winding );
		p->winding = frontwinding;

		if( side == 0 )
		{
			AddPortalToNodes( p, f, other_node );
			AddPortalToNodes( new_portal, b, other_node );
		}
		else
		{
			AddPortalToNodes( p, other_node, f );
			AddPortalToNodes( new_portal, other_node, b );
		}
	}
	node->portals = NULL;
}

/*
================
CalcNodeBounds
================
*/
void CalcNodeBounds( node_t *node )
{
	portal_t	*p;
	int	s, i;

	// calc mins/maxs for both leafs and nodes
	ClearBounds( node->mins, node->maxs );
	for( p = node->portals; p; p = p->next[s] )
	{
		s = ( p->nodes[1] == node );
		for( i = 0; i < p->winding->numpoints; i++ )
			AddPointToBounds( p->winding->p[i], node->mins, node->maxs );
	}
}

/*
==================
MakeTreePortals_r
==================
*/
void MakeTreePortals_r( node_t *node )
{
	int		i;
	const float	*v;

	CalcNodeBounds( node );
	if( node->mins[0] >= node->maxs[0] )
	{
		v = node->referencepoint;
		MsgDev( D_WARN, "node without a volume\n" );
		MsgDev( D_WARN, "node has %d tiny portals\n", node->tinyportals );
		MsgDev( D_WARN, "node reference point %1.2f %1.2f %1.2f\n", v[0], v[1], v[2] );
	}

	for( i = 0; i < 3; i++ )
	{
		if( node->mins[i] < MIN_WORLD_COORD || node->maxs[i] > MAX_WORLD_COORD )
		{
			MsgDev( D_WARN, "node with unbounded volume\n" );
			break;
		}
	}
	if( node->planenum == PLANENUM_LEAF ) return;

	MakeNodePortal( node );
	SplitNodePortals( node );

	MakeTreePortals_r( node->children[0] );
	MakeTreePortals_r( node->children[1] );
}

/*
==================
MakeTreePortals
==================
*/
void MakeTreePortals( tree_t *tree )
{
	Msg( "----- MakeTreePortals -----\n" );
	MakeHeadnodePortals( tree );
	MakeTreePortals_r( tree->headnode );
	Msg( "%6d tiny portals\n", c_tinyportals);
}

/*
=========================================================

FLOOD ENTITIES

=========================================================
*/

int		c_floodedleafs;

/*
=============
FloodPortals_r
=============
*/
void FloodPortals_r( node_t *node, int dist )
{
	portal_t	*p;
	int	s;

	if( node->occupied ) return;
	if( node->opaque ) return;

	c_floodedleafs++;
	node->occupied = dist;

	for( p = node->portals; p; p = p->next[s] )
	{
		s = (p->nodes[1] == node);
		FloodPortals_r( p->nodes[!s], dist + 1 );
	}
}

/*
=============
PlaceOccupant
=============
*/
bool PlaceOccupant( node_t *headnode, vec3_t origin, bsp_entity_t *occupant )
{
	node_t	*node;
	plane_t	*plane;
	vec_t	d;

	// find the leaf to start in
	node = headnode;
	while( node->planenum != PLANENUM_LEAF )
	{
		plane = &mapplanes[node->planenum];
		d = DotProduct( origin, plane->normal ) - plane->dist;
		if( d >= 0 ) node = node->children[0];
		else node = node->children[1];
	}

	if( node->opaque ) return false;
	node->occupant = occupant;

	FloodPortals_r( node, 1 );

	return true;
}

/*
=============
FloodEntities

Marks all nodes that can be reached by entites
=============
*/
bool FloodEntities( tree_t *tree )
{
	int		i;
	vec3_t		origin;
	const char	*cl;
	bool		inside;
	node_t		*headnode;

	headnode = tree->headnode;
	Msg( "--- FloodEntities ---\n" );
	inside = false;
	tree->outside_node.occupied = 0;

	c_floodedleafs = 0;
	for( i = 1; i < num_entities; i++ )
	{
		GetVectorForKey( &entities[i], "origin", origin );
		if( VectorIsNull( origin )) continue;

		cl = ValueForKey( &entities[i], "classname" );
		origin[2] += 1; // so objects on floor are ok

		if( PlaceOccupant( headnode, origin, &entities[i] ))
			inside = true;
	}

	Msg( "%5i flooded leafs\n", c_floodedleafs );

	if( !inside ) Msg( "no entities in open -- no filling\n" );
	else if( tree->outside_node.occupied )
		Msg( "entity reached from outside -- no filling\n" );

	return (bool)(inside && !tree->outside_node.occupied);
}

/*
=========================================================

FLOOD AREAS

=========================================================
*/

int		c_areas;

/*
=============
FloodAreas_r
=============
*/
void FloodAreas_r( node_t *node )
{
	portal_t		*p;
	int		s;
	bspbrush_t	*b;

	if( node->areaportal )
	{
		if( node->area == -1 )
			node->area = c_areas;

		// this node is part of an area portal brush
		b = node->brushlist->original;

		// if the current area has allready touched this
		// portal, we are done
		if( b->portalareas[0] == c_areas || b->portalareas[1] == c_areas )
			return;

		// note the current area as bounding the portal
		if( b->portalareas[1] != -1 )
		{
			MsgDev( D_WARN, "areaportal brush %i touches 3 areas\n", b->brushnum );
			return;
		}
		if( b->portalareas[0] != -1 )
			b->portalareas[1] = c_areas;
		else b->portalareas[0] = c_areas;
		return;
	}

	if( node->area != -1 ) return; // allready got it
	if( node->cluster == -1 ) return;
	node->area = c_areas;

	for( p = node->portals; p; p = p->next[s] )
	{
		s = (p->nodes[1] == node);

		if( !Portal_Passable( p ))
			continue;
		FloodAreas_r( p->nodes[!s] );
	}
}


/*
=============
FindAreas_r

Just decend the tree, and for each node that hasn't had an
area set, flood fill out from there
=============
*/
void FindAreas_r( node_t *node )
{
	if( node->planenum != PLANENUM_LEAF )
	{
		FindAreas_r( node->children[0] );
		FindAreas_r( node->children[1] );
		return;
	}

	if( node->opaque ) return;
	if( node->areaportal ) return;
	if( node->area != -1 ) return; // allready got it

	FloodAreas_r( node );
	c_areas++;
}

/*
=============
CheckAreas_r
=============
*/
void CheckAreas_r( node_t *node )
{
	bspbrush_t	*b;

	if( node->planenum != PLANENUM_LEAF )
	{
		CheckAreas_r( node->children[0] );
		CheckAreas_r( node->children[1] );
		return;
	}

	if( node->opaque ) return;

	if( node->cluster != -1 )
	{
		if( node->area == -1 )
			MsgDev( D_WARN, "cluster %d has area set to -1\n", node->cluster );
	}
	if( node->areaportal )
	{
		b = node->brushlist->original;
		// check if the areaportal touches two areas
		if( b->portalareas[0] == -1 || b->portalareas[1] == -1 )
			MsgDev( D_WARN, "areaportal brush %i doesn't touch two areas\n", b->brushnum );
	}
}

/*
=============
FloodAreas

Mark each leaf with an area, bounded by CONTENTS_AREAPORTAL
=============
*/
void FloodAreas( tree_t *tree )
{
	MsgDev( D_INFO, "--- FloodAreas ---\n" );
	FindAreas_r( tree->headnode );

	// check for areaportal brushes that don't touch two areas
	CheckAreas_r( tree->headnode );

	MsgDev( D_INFO, "%5i areas\n", c_areas );
}

//======================================================

int	c_outside;
int	c_inside;
int	c_solid;

void FillOutside_r( node_t *node )
{
	if( node->planenum != PLANENUM_LEAF )
	{
		FillOutside_r( node->children[0] );
		FillOutside_r( node->children[1] );
		return;
	}

	// anything not reachable by an entity
	// can be filled away
	if( !node->occupied )
	{
		if( !node->opaque )
		{
			c_outside++;
			node->opaque = true;
		}
		else c_solid++;
	}
	else c_inside++;
}

/*
=============
FillOutside

Fill all nodes that can't be reached by entities
=============
*/
void FillOutside( node_t *headnode )
{
	c_outside = 0;
	c_inside = 0;
	c_solid = 0;
	MsgDev( D_INFO, "--- FillOutside ---\n" );
	FillOutside_r( headnode );
	MsgDev( D_INFO, "%5i solid leafs\n", c_solid );
	MsgDev( D_INFO, "%5i leafs filled\n", c_outside );
	MsgDev( D_INFO, "%5i inside leafs\n", c_inside );
}
