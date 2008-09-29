//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	brush.c - allocate & processing
//=======================================================================

#include "bsplib.h"
#include "const.h"

#define SNAP_EPSILON	0.01
int	c_active_brushes;

/*
================
AllocSideRef

allocates and assigns a brush side reference
================
*/

sideRef_t *AllocSideRef( side_t *side, sideRef_t *next )
{
	sideRef_t *sideRef;

	// dummy check
	if( side == NULL ) return next;
	sideRef = BSP_Malloc( sizeof( *sideRef ));
	sideRef->side = side;
	sideRef->next = next;
	return sideRef;
}


/*
================
CountBrushList
================
*/
int CountBrushList( bspbrush_t *brushes )
{
	int c = 0;
	for( ; brushes; brushes = brushes->next ) c++;
	return c;
}

/*
================
AllocBrush
================
*/
bspbrush_t *AllocBrush( int numsides )
{
	bspbrush_t	*bb;
	size_t		c;

	c = (int)&(((bspbrush_t *)0)->sides[numsides]);
	bb = BSP_Malloc( c );
	if( GetNumThreads() == 1 ) c_active_brushes++;
	return bb;
}

/*
================
FreeBrush
================
*/
void FreeBrush( bspbrush_t *brush )
{
	int	i;

	for( i = 0; i < brush->numsides; i++ )
	{
		if( brush->sides[i].winding )
			FreeWinding( brush->sides[i].winding );
	}

	Mem_Free( brush );
	if( GetNumThreads() == 1 ) c_active_brushes--;
}

/*
================
FreeBrushList
================
*/
void FreeBrushList( bspbrush_t *brushes )
{
	bspbrush_t	*next;

	for( ; brushes; brushes = next )
	{
		next = brushes->next;
		FreeBrush( brushes );
	}		
}

/*
==================
CopyBrush

Duplicates the brush, the sides, and the windings
==================
*/
bspbrush_t *CopyBrush( bspbrush_t *brush )
{
	bspbrush_t	*newbrush;
	size_t		size;
	int		i;
	
	size = (int)&(((bspbrush_t *)0)->sides[brush->numsides]);

	newbrush = AllocBrush( brush->numsides );
	Mem_Copy( newbrush, brush, size );
	newbrush->next = NULL;

	for( i = 0; i < brush->numsides; i++ )
	{
		if( brush->sides[i].winding )
			newbrush->sides[i].winding = CopyWinding( brush->sides[i].winding );
	}

	return newbrush;
}

/*
==================
BoundBrush

Sets the mins/maxs based on the windings
==================
*/
bool BoundBrush( bspbrush_t *brush )
{
	int		i, j;
	winding_t		*w;

	ClearBounds( brush->mins, brush->maxs );
	for( i = 0; i < brush->numsides; i++ )
	{
		w = brush->sides[i].winding;
		if( !w ) continue;
		for( j = 0; j < w->numpoints; j++ )
			AddPointToBounds( w->p[j], brush->mins, brush->maxs );
	}
	for( i = 0; i < 3; i++ )
	{
		if( brush->mins[i] < MIN_WORLD_COORD || brush->maxs[i] > MAX_WORLD_COORD )
			return false;
		if( brush->mins[i] >= brush->maxs[i] )
			return false;
	}
	return true;
}

/*
==================
SnapWeldVector

welds two vec3_t's into a third, taking into account nearest-to-integer
instead of averaging
==================
*/
void SnapWeldVector( vec3_t a, vec3_t b, vec3_t out )
{
	int	i;
	vec_t	ai, bi, outi;
	
	if( a == NULL || b == NULL || out == NULL )
		return;
	
	// do each element
	for( i = 0; i < 3; i++ )
	{
		// round to integer
		ai = floor( a[i] + 0.5 );
		bi = floor( a[i] + 0.5 );
		
		// prefer exact integer
		if( ai == a[i] ) out[i] = a[i];
		else if( bi == b[i] ) out[i] = b[i];

		// use nearest
		else if( fabs( ai - a[i] ) < fabs( bi < b[i] ))
			out[i] = a[i];
		else out[i] = b[i];
		
		// snap
		outi = floor( out[i] + 0.5 );
		if( fabs( outi - out[i] ) <= SNAP_EPSILON )
			out[i] = outi;
	}
}

/*
================
FixWinding

removes degenerate edges from a winding
returns true if the winding is valid
================
*/
bool FixWinding( winding_t *w )
{
	bool	valid = true;
	int	i, j, k;
	vec3_t	vec;
	float	dist;

	if( !w ) return false;
	
	// check all verts
	for( i = 0; i < w->numpoints; i++ )
	{
		// don't remove points if winding is a triangle
		if( w->numpoints == 3 ) return valid;
		
		// get second point index
		j = (i + 1) % w->numpoints;
		
		// degenerate edge?
		VectorSubtract( w->p[i], w->p[j], vec );
		dist = VectorLength( vec );
		if( dist < DEGENERATE_EPSILON )
		{
			valid = false;
		
			// create an average point (ydnar 2002-01-26: using nearest-integer weld preference)
			SnapWeldVector( w->p[i], w->p[j], vec );
			VectorCopy( vec, w->p[i] );
		
			// move the remaining verts
			for( k = i + 2; k < w->numpoints; k++ )
			{
				VectorCopy( w->p[k], w->p[k-1] );
			}
			w->numpoints--;
		}
	}
	
	// one last check and return
	if( w->numpoints < 3 )
		valid = false;
	return valid;
}

/*
================
MakeBrushWindings

makes basewindigs for sides and mins/maxs for the brush
returns false if the brush doesn't enclose a valid volume
================
*/
bool CreateBrushWindings( bspbrush_t *brush )
{
	int		i, j;
	winding_t		*w;
	side_t		*side;
	plane_t		*plane;

	for( i = 0; i < brush->numsides; i++ )
	{
		side = &brush->sides[i];
		plane = &mapplanes[side->planenum];

		// make huge winding
		w = BaseWindingForPlane( plane->normal, plane->dist );

		for ( j = 0; j < brush->numsides && w; j++ )
		{
			if( i == j ) continue;
			if( brush->sides[j].planenum == ( brush->sides[i].planenum ^ 1 ))
				continue;	// back side clipaway
			if( brush->sides[j].bevel )
				continue;
			plane = &mapplanes[brush->sides[j].planenum^1];
			ChopWindingInPlace( &w, plane->normal, plane->dist, 0 );

			FixWinding( w );
		}

		// free any existing winding
		if( side->winding ) FreeWinding( side->winding );
		side->winding = w;
	}
	return BoundBrush( brush );
}

/*
==================
BrushFromBounds

Creates a new axial brush
==================
*/
bspbrush_t *BrushFromBounds( vec3_t mins, vec3_t maxs )
{
	bspbrush_t	*b;
	vec3_t		normal;
	vec_t		dist;
	int		i = 6;

	b = AllocBrush( i );
	b->numsides = i;
	for( i = 0; i < 3; i++ )
	{
		VectorClear( normal );
		normal[i] = 1;
		dist = maxs[i];
		b->sides[i].planenum = FindFloatPlane( normal, dist, 1, (vec3_t*) &maxs );

		normal[i] = -1;
		dist = -mins[i];
		b->sides[3+i].planenum = FindFloatPlane( normal, dist, 1, (vec3_t*) &mins );
	}

	CreateBrushWindings( b );

	return b;
}

/*
==================
BrushVolume

==================
*/
vec_t BrushVolume( bspbrush_t *brush )
{
	int		i;
	vec3_t		corner;
	winding_t		*w = NULL;
	vec_t		d, area, volume;
	plane_t		*plane;

	if( !brush ) return 0;

	// grab the first valid point as the corner

	for( i = 0; i < brush->numsides; i++ )
	{
		w = brush->sides[i].winding;
		if( w ) break;
	}
	if( !w ) return 0;
	VectorCopy( w->p[0], corner );

	// make tetrahedrons to all other faces

	for ( volume = 0; i < brush->numsides; i++ )
	{
		w = brush->sides[i].winding;
		if( !w ) continue;
		plane = &mapplanes[brush->sides[i].planenum];
		d = -(DotProduct (corner, plane->normal) - plane->dist);
		area = WindingArea( w );
		volume += d * area;
	}

	volume /= 3;
	return volume;
}

/*
====================
WriteBSPBrushMap

writes a map with the split bsp brushes
just for compiler debug
====================
*/
void WriteBSPBrushMap( const char *name, bspbrush_t *list )
{
	file_t		*f;
	side_t		*s;
	int		i;
	winding_t		*w;
	
	MsgDev( D_NOTE, "Writing %s\n", name );
	f = FS_Open( name, "wb" );
	if( !f ) return;

	FS_Printf( f, "{\n\"classname\" \"worldspawn\"\n" );

	for(  ; list; list = list->next )
	{
		FS_Printf( f, "{\n" );
		for( i = 0, s = list->sides; i < list->numsides; i++, s++ )
		{
			w = BaseWindingForPlane( mapplanes[s->planenum].normal, mapplanes[s->planenum].dist );

			FS_Printf( f,"( %i %i %i ) ", (int)w->p[0][0], (int)w->p[0][1], (int)w->p[0][2] );
			FS_Printf( f,"( %i %i %i ) ", (int)w->p[1][0], (int)w->p[1][1], (int)w->p[1][2] );
			FS_Printf( f,"( %i %i %i ) ", (int)w->p[2][0], (int)w->p[2][1], (int)w->p[2][2] );

			FS_Printf (f, "notexture 0 0 0 1 1\n" );
			FreeWinding( w );
		}
		FS_Printf( f, "}\n" );
	}
	FS_Printf( f, "}\n" );
	FS_Close( f );
}

//=====================================================================================

/*
====================
FilterBrushIntoTree_r

====================
*/
int FilterBrushIntoTree_r( bspbrush_t *b, node_t *node )
{
	bspbrush_t	*front, *back;
	int		c;

	if( !b ) return 0;

	// add it to the leaf list
	if( node->planenum == PLANENUM_LEAF )
	{
		b->next = node->brushlist;
		node->brushlist = b;

		// classify the leaf by the structural brush
		if( !b->detail )
		{
			if( b->opaque )
			{
				node->opaque = true;
				node->areaportal = false;
			}
			else if( b->contents & CONTENTS_AREAPORTAL )
			{
				if( !node->opaque ) node->areaportal = true;
			}
		}
		return 1;
	}

	// split it by the node plane
	SplitBrush( b, node->planenum, &front, &back );
	FreeBrush( b );

	c = 0;
	c += FilterBrushIntoTree_r( front, node->children[0] );
	c += FilterBrushIntoTree_r( back, node->children[1] );

	return c;
}

/*
=====================
FilterDetailBrushesIntoTree

Fragment all the detail brushes into the structural leafs
=====================
*/
void FilterDetailBrushesIntoTree( bsp_entity_t *e, tree_t *tree )
{
	bspbrush_t	*b, *newb;
	int		c_unique, c_clusters;
	int		i, r;

	c_unique = 0;
	c_clusters = 0;
	for( b = e->brushes; b; b = b->next )
	{
		if( !b->detail ) continue;
		c_unique++;
		newb = CopyBrush( b );
		r = FilterBrushIntoTree_r( newb, tree->headnode );
		c_clusters += r;

		// mark all sides as visible so drawsurfs are created
		if( r )
		{
			for( i = 0; i < b->numsides; i++ )
				if( b->sides[i].winding )
					b->sides[i].visible = true;
		}
	}

	MsgDev( D_INFO, "%5i detail brushes\n", c_unique );
	MsgDev( D_INFO, "%5i cluster references\n", c_clusters );
}

/*
=====================
FilterStructuralBrushesIntoTree

Mark the leafs as opaque and areaportals
=====================
*/
void FilterStructuralBrushesIntoTree( bsp_entity_t *e, tree_t *tree )
{
	bspbrush_t	*b, *newb;
	int		c_unique, c_clusters;
	int		i, r;

	c_unique = 0;
	c_clusters = 0;
	for( b = e->brushes; b; b = b->next )
	{
		if( b->detail ) continue;
		c_unique++;
		newb = CopyBrush( b );
		r = FilterBrushIntoTree_r( newb, tree->headnode );
		c_clusters += r;

		// mark all sides as visible so drawsurfs are created
		if( r )
		{
			for( i = 0; i < b->numsides; i++ )
				if( b->sides[i].winding )
					b->sides[i].visible = true;
		}
	}

	MsgDev( D_INFO, "%5i structural brushes\n", c_unique );
	MsgDev( D_INFO, "%5i cluster references\n", c_clusters );
}

/*
================
AllocTree
================
*/
tree_t *AllocTree( void )
{
	tree_t	*tree;

	tree = BSP_Malloc(sizeof( *tree ));
	ClearBounds( tree->mins, tree->maxs );

	return tree;
}

/*
================
AllocNode
================
*/
node_t *AllocNode( void )
{
	node_t	*node;

	node = BSP_Malloc(sizeof( *node ));

	return node;
}

/*
================
WindingIsTiny

Returns true if the winding would be crunched out of
existance by the vertex snapping.
================
*/
bool WindingIsTiny( winding_t *w )
{
	int	i, j;
	vec_t	len;
	vec3_t	delta;
	int	edges = 0;

	for( i = 0; i < w->numpoints; i++ )
	{
		j = i == w->numpoints - 1 ? 0 : i+1;
		VectorSubtract( w->p[j], w->p[i], delta );
		len = VectorLength( delta );
		if (len > 0.2 )
		{
			if( ++edges == 3 )
				return false;
		}
	}
	return true;
}

/*
================
WindingIsHuge

Returns true if the winding still has one of the points
from basewinding for plane
================
*/
bool WindingIsHuge ( winding_t *w )
{
	int	i, j;

	for( i = 0; i < w->numpoints; i++ )
	{
		for( j = 0; j < 3; j++ )
			if( w->p[i][j] <= MIN_WORLD_COORD || w->p[i][j] >= MAX_WORLD_COORD )
				return true;
	}
	return false;
}

/*
==================
BrushMostlyOnSide

==================
*/
int BrushMostlyOnSide( bspbrush_t *brush, plane_t *plane )
{
	int		i, j;
	winding_t		*w;
	vec_t		d, max = 0;
	int		side = PSIDE_FRONT;

	for( i = 0; i < brush->numsides; i++ )
	{
		w = brush->sides[i].winding;
		if( !w ) continue;
		for( j = 0; j < w->numpoints; j++ )
		{
			d = DotProduct( w->p[j], plane->normal ) - plane->dist;
			if( d > max )
			{
				max = d;
				side = PSIDE_FRONT;
			}
			if( -d > max )
			{
				max = -d;
				side = PSIDE_BACK;
			}
		}
	}
	return side;
}

/*
================
SplitBrush

Generates two new brushes, leaving the original unchanged
================
*/
void SplitBrush( bspbrush_t *brush, int planenum, bspbrush_t **front, bspbrush_t **back )
{
	bspbrush_t	*b[2];
	int		i, j;
	winding_t		*w, *cw[2], *midwinding;
	plane_t		*plane, *plane2;
	side_t		*s, *cs;
	float		d, d_front, d_back;

	*front = *back = NULL;
	plane = &mapplanes[planenum];

	// check all points
	d_front = d_back = 0;
	for( i = 0; i < brush->numsides; i++ )
	{
		w = brush->sides[i].winding;
		if( !w ) continue;
		for( j = 0; j < w->numpoints; j++ )
		{
			d = DotProduct( w->p[j], plane->normal ) - plane->dist;
			if( d > 0 && d > d_front ) d_front = d;
			if( d < 0 && d < d_back ) d_back = d;
		}
	}
	if( d_front < 0.1 )
	{	
		// only on back
		*back = CopyBrush( brush );
		return;
	}
	if( d_back > -0.1 )
	{	
		// only on front
		*front = CopyBrush( brush );
		return;
	}

	// create a new winding from the split plane
	w = BaseWindingForPlane( plane->normal, plane->dist );
	for( i = 0; i < brush->numsides && w; i++ )
	{
		plane2 = &mapplanes[brush->sides[i].planenum ^ 1];
		ChopWindingInPlace( &w, plane2->normal, plane2->dist, 0 );
	}

	if( !w || WindingIsTiny( w ))
	{	
		// the brush isn't really split
		int	side;

		side = BrushMostlyOnSide( brush, plane );
		if( side == PSIDE_FRONT ) *front = CopyBrush( brush );
		if( side == PSIDE_BACK ) *back = CopyBrush( brush );
		return;
	}

	if( WindingIsHuge( w )) MsgDev( D_WARN, "huge winding\n" );

	midwinding = w;

	// split it for real
	for( i = 0; i < 2; i++ )
	{
		b[i] = AllocBrush( brush->numsides + 1 );
		Mem_Copy( b[i], brush, sizeof( bspbrush_t ) - sizeof( brush->sides ));
		b[i]->numsides = 0;
		b[i]->next = NULL;
		b[i]->original = brush->original;
	}

	// split all the current windings
	for( i = 0; i < brush->numsides; i++ )
	{
		s = &brush->sides[i];
		w = s->winding;
		if( !w ) continue;
		ClipWindingEpsilon( w, plane->normal, plane->dist, 0, &cw[0], &cw[1] );
		for( j = 0; j < 2; j++ )
		{
			if( !cw[j] ) continue;
			cs = &b[j]->sides[b[j]->numsides];
			b[j]->numsides++;
			*cs = *s;
			cs->winding = cw[j];
		}
	}


	// see if we have valid polygons on both sides
	for( i = 0; i < 2; i++ )
	{
		if( b[i]->numsides < 3 || !BoundBrush (b[i]))
		{
			if( b[i]->numsides >= 3 )
				MsgDev( D_WARN, "bogus brush after clip\n" );
			FreeBrush( b[i] );
			b[i] = NULL;
		}
	}

	if( !(b[0] && b[1] ))
	{
		if( !b[0] && !b[1] )
			MsgDev( D_INFO, "split removed brush\n" );
		else MsgDev( D_INFO, "split not on both sides\n" );
		if( b[0] )
		{
			FreeBrush( b[0] );
			*front = CopyBrush( brush );
		}
		if( b[1] )
		{
			FreeBrush( b[1] );
			*back = CopyBrush (brush);
		}
		return;
	}

	// add the midwinding to both sides
	for( i = 0; i < 2; i++ )
	{
		cs = &b[i]->sides[b[i]->numsides];
		b[i]->numsides++;

		cs->planenum = planenum^i^1;
		cs->shader = NULL;
		cs->visible = false;
		if( i == 0 ) cs->winding = CopyWinding( midwinding );
		else cs->winding = midwinding;
	}

	for( i = 0; i < 2; i++ )
	{
		d = BrushVolume (b[i]);
		if( d < 1.0 )
		{
			FreeBrush( b[i] );
			b[i] = NULL;
		}
	}

	*front = b[0];
	*back = b[1];
}