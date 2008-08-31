
#include "bsplib.h"
#include "const.h"

int	c_nodes;
int	c_nonvis;
vec_t	microvolume = 0.3;

void FindBrushInTree (node_t *node, int brushnum)
{
	bspbrush_t	*b;

	if (node->planenum == PLANENUM_LEAF)
	{
		for (b=node->brushlist ; b ; b=b->next)
			if (b->original->brushnum == brushnum)
				Msg ("here\n");
		return;
	}
	FindBrushInTree (node->children[0], brushnum);
	FindBrushInTree (node->children[1], brushnum);
}

//==================================================

static void pw(winding_t *w)
{
	int		i;
	for (i=0 ; i<w->numpoints ; i++)
		Msg ("(%5.1f, %5.1f, %5.1f)\n",w->p[i][0], w->p[i][1],w->p[i][2]);
}

void PrintBrush (bspbrush_t *brush)
{
	int		i;

	Msg ("brush: %p\n", brush);
	for (i=0;i<brush->numsides ; i++)
	{
		pw(brush->sides[i].winding);
		Msg ("\n");
	}
}
/*
==================
PointInLeaf

==================
*/
node_t	*PointInLeaf (node_t *node, vec3_t point)
{
	vec_t		d;
	plane_t		*plane;

	while (node->planenum != PLANENUM_LEAF)
	{
		plane = &mapplanes[node->planenum];
		d = DotProduct (point, plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return node;
}

//========================================================

/*
==============
Q_BoxOnPlaneSide

Returns PSIDE_FRONT, PSIDE_BACK, or PSIDE_BOTH
==============
*/
int Q_BoxOnPlaneSide (vec3_t mins, vec3_t maxs, plane_t *plane)
{
	int		side;
	int		i;
	vec3_t	corners[2];
	vec_t	dist1, dist2;

	// axial planes are easy
	if (plane->type < 3)
	{
		side = 0;
		if (maxs[plane->type] > plane->dist+EQUAL_EPSILON)
			side |= PSIDE_FRONT;
		if (mins[plane->type] < plane->dist-EQUAL_EPSILON)
			side |= PSIDE_BACK;
		return side;
	}

	// create the proper leading and trailing verts for the box

	for (i=0 ; i<3 ; i++)
	{
		if (plane->normal[i] < 0)
		{
			corners[0][i] = mins[i];
			corners[1][i] = maxs[i];
		}
		else
		{
			corners[1][i] = mins[i];
			corners[0][i] = maxs[i];
		}
	}

	dist1 = DotProduct (plane->normal, corners[0]) - plane->dist;
	dist2 = DotProduct (plane->normal, corners[1]) - plane->dist;
	side = 0;
	if (dist1 >= EQUAL_EPSILON)
		side = PSIDE_FRONT;
	if (dist2 < EQUAL_EPSILON)
		side |= PSIDE_BACK;

	return side;
}

/*
============
QuickTestBrushToPlanenum

============
*/
int QuickTestBrushToPlanenum (bspbrush_t *brush, int planenum, int *numsplits)
{
	int			i, num;
	plane_t		*plane;
	int			s;

	*numsplits = 0;

	// if the brush actually uses the planenum,
	// we can tell the side for sure
	for (i = 0; i < brush->numsides; i++)
	{
		num = brush->sides[i].planenum;
		if (num >= 0x10000) Sys_Error("bad planenum");
		if (num == planenum) return PSIDE_BACK|PSIDE_FACING;
		if (num == (planenum ^ 1) ) return PSIDE_FRONT|PSIDE_FACING;
	}

	// box on plane side
	plane = &mapplanes[planenum];
	s = Q_BoxOnPlaneSide (brush->mins, brush->maxs, plane);

	// if both sides, count the visible faces split
	if (s == PSIDE_BOTH)
	{
		*numsplits += 3;
	}

	return s;
}

/*
============
TestBrushToPlanenum

============
*/
int TestBrushToPlanenum (bspbrush_t *brush, int planenum, int *numsplits, bool *hintsplit, int *epsilonbrush)
{
	int		i, j, num;
	plane_t		*plane;
	int		s;
	winding_t		*w;
	vec_t		d, d_front, d_back;
	int		front, back;

	*numsplits = 0;
	*hintsplit = false;

	// if the brush actually uses the planenum,
	// we can tell the side for sure
	for (i=0 ; i<brush->numsides ; i++)
	{
		num = brush->sides[i].planenum;
		if (num >= 0x10000) Sys_Error("bad planenum");
		if (num == planenum) return PSIDE_BACK|PSIDE_FACING;
		if (num == (planenum ^ 1) ) return PSIDE_FRONT|PSIDE_FACING;
	}

	// box on plane side
	plane = &mapplanes[planenum];
	s = Q_BoxOnPlaneSide (brush->mins, brush->maxs, plane);

	if (s != PSIDE_BOTH)
		return s;

// if both sides, count the visible faces split
	d_front = d_back = 0;

	for (i=0 ; i<brush->numsides ; i++)
	{
		if (brush->sides[i].texinfo == TEXINFO_NODE)
			continue;		// on node, don't worry about splits
		if (!brush->sides[i].visible)
			continue;		// we don't care about non-visible
		w = brush->sides[i].winding;
		if (!w)
			continue;
		front = back = 0;
		for (j=0 ; j<w->numpoints; j++)
		{
			d = DotProduct (w->p[j], plane->normal) - plane->dist;
			if (d > d_front)
				d_front = d;
			if (d < d_back)
				d_back = d;

			if (d > 0.1) // EQUAL_EPSILON)
				front = 1;
			if (d < -0.1) // EQUAL_EPSILON)
				back = 1;
		}
		if (front && back)
		{
			if ( !(brush->sides[i].surf & SURF_SKIP) )
			{
				(*numsplits)++;
				if (brush->sides[i].surf & SURF_HINT)
					*hintsplit = true;
			}
		}
	}

	if ( (d_front > 0.0 && d_front < 1.0)
		|| (d_back < 0.0 && d_back > -1.0) )
		(*epsilonbrush)++;

#if 0
	if (*numsplits == 0)
	{	//	didn't really need to be split
		if (front)
			s = PSIDE_FRONT;
		else if (back)
			s = PSIDE_BACK;
		else
			s = 0;
	}
#endif

	return s;
}

//========================================================


/*
================
Leafnode
================
*/
void LeafNode (node_t *node, bspbrush_t *brushes)
{
	bspbrush_t	*b;
	int			i;

	node->planenum = PLANENUM_LEAF;
	node->contents = 0;

	for (b=brushes ; b ; b=b->next)
	{
		// if the brush is solid and all of its sides are on nodes,
		// it eats everything
		if (b->original->contents & CONTENTS_SOLID)
		{
			for (i=0 ; i<b->numsides ; i++)
				if (b->sides[i].texinfo != TEXINFO_NODE)
					break;
			if (i == b->numsides)
			{
				node->contents = CONTENTS_SOLID;
				break;
			}
		}
		node->contents |= b->original->contents;
	}

	node->brushlist = brushes;
}


//============================================================

void CheckPlaneAgainstParents (int pnum, node_t *node)
{
	node_t	*p;

	for (p = node->parent; p; p = p->parent)
	{
		if (p->planenum == pnum)
			Sys_Error("Tried parent");
	}
}

bool CheckPlaneAgainstVolume (int pnum, node_t *node)
{
	bspbrush_t	*front, *back;
	bool		good;

	SplitBrush (node->volume, pnum, &front, &back);

	good = (front && back);

	if (front) FreeBrush (front);
	if (back) FreeBrush (back);

	return good;
}

/*
================
SelectSplitSide

Using a hueristic, choses one of the sides out of the brushlist
to partition the brushes with.
Returns NULL if there are no valid planes to split with..
================
*/
side_t *SelectSplitSide (bspbrush_t *brushes, node_t *node)
{
	int			value, bestvalue;
	bspbrush_t	*brush, *test;
	side_t		*side, *bestside;
	int			i, j, pass, numpasses;
	int			pnum;
	int			s;
	int			front, back, both, facing, splits;
	int			bsplits;
	int			bestsplits;
	int			epsilonbrush;
	bool	hintsplit;

	bestside = NULL;
	bestvalue = -99999;
	bestsplits = 0;

	// the search order goes: visible-structural, visible-detail,
	// nonvisible-structural, nonvisible-detail.
	// If any valid plane is available in a pass, no further
	// passes will be tried.
	numpasses = 4;
	for (pass = 0 ; pass < numpasses ; pass++)
	{
		for (brush = brushes ; brush ; brush=brush->next)
		{
			if ( (pass & 1) && !(brush->original->contents & CONTENTS_DETAIL) )
				continue;
			if ( !(pass & 1) && (brush->original->contents & CONTENTS_DETAIL) )
				continue;
			for (i=0 ; i<brush->numsides ; i++)
			{
				side = brush->sides + i;
				if (side->bevel)
					continue;	// never use a bevel as a spliter
				if (!side->winding)
					continue;	// nothing visible, so it can't split
				if (side->texinfo == TEXINFO_NODE)
					continue;	// allready a node splitter
				if (side->tested)
					continue;	// we allready have metrics for this plane
				if (side->surf & SURF_SKIP)
					continue;	// skip surfaces are never chosen
				if ( side->visible ^ (pass<2) )
					continue;	// only check visible faces on first pass

				pnum = side->planenum;
				pnum &= ~1;	// allways use positive facing plane

				CheckPlaneAgainstParents (pnum, node);

				if (!CheckPlaneAgainstVolume (pnum, node))
					continue;	// would produce a tiny volume

				front = 0;
				back = 0;
				both = 0;
				facing = 0;
				splits = 0;
				epsilonbrush = 0;

				for (test = brushes ; test ; test=test->next)
				{
					s = TestBrushToPlanenum (test, pnum, &bsplits, &hintsplit, &epsilonbrush);

					splits += bsplits;
					if (bsplits && (s&PSIDE_FACING) )
						Sys_Error("PSIDE_FACING with splits");

					test->testside = s;
					// if the brush shares this face, don't bother
					// testing that facenum as a splitter again
					if (s & PSIDE_FACING)
					{
						facing++;
						for (j=0 ; j<test->numsides ; j++)
						{
							if ( (test->sides[j].planenum&~1) == pnum)
								test->sides[j].tested = true;
						}
					}
					if (s & PSIDE_FRONT)
						front++;
					if (s & PSIDE_BACK)
						back++;
					if (s == PSIDE_BOTH)
						both++;
				}

				// give a value estimate for using this plane

				value =  5*facing - 5*splits - abs(front-back);
//					value =  -5*splits;
//					value =  5*facing - 5*splits;
				if (mapplanes[pnum].type < 3)
					value+=5;		// axial is better
				value -= epsilonbrush*1000;	// avoid!

				// never split a hint side except with another hint
				if (hintsplit && !(side->surf & SURF_HINT) )
					value = -9999999;

				// save off the side test so we don't need
				// to recalculate it when we actually seperate
				// the brushes
				if (value > bestvalue)
				{
					bestvalue = value;
					bestside = side;
					bestsplits = splits;
					for (test = brushes ; test ; test=test->next)
						test->side = test->testside;
				}
			}
		}

		// if we found a good plane, don't bother trying any
		// other passes
		if (bestside)
		{
			if (pass > 1)
			{
				if (GetNumThreads() == 1)
					c_nonvis++;
			}
			if (pass > 0)
				node->detail_seperator = true;	// not needed for vis
			break;
		}
	}

	//
	// clear all the tested flags we set
	//
	for (brush = brushes ; brush ; brush=brush->next)
	{
		for (i=0 ; i<brush->numsides ; i++)
			brush->sides[i].tested = false;
	}

	return bestside;
}

/*
================
SplitBrushList
================
*/
void SplitBrushList (bspbrush_t *brushes, 
	node_t *node, bspbrush_t **front, bspbrush_t **back)
{
	bspbrush_t	*brush, *newbrush, *newbrush2;
	side_t		*side;
	int			sides;
	int			i;

	*front = *back = NULL;

	for (brush = brushes ; brush ; brush=brush->next)
	{
		sides = brush->side;

		if (sides == PSIDE_BOTH)
		{	// split into two brushes
			SplitBrush (brush, node->planenum, &newbrush, &newbrush2);
			if (newbrush)
			{
				newbrush->next = *front;
				*front = newbrush;
			}
			if (newbrush2)
			{
				newbrush2->next = *back;
				*back = newbrush2;
			}
			continue;
		}

		newbrush = CopyBrush (brush);

		// if the planenum is actualy a part of the brush
		// find the plane and flag it as used so it won't be tried
		// as a splitter again
		if (sides & PSIDE_FACING)
		{
			for (i=0 ; i<newbrush->numsides ; i++)
			{
				side = newbrush->sides + i;
				if ( (side->planenum& ~1) == node->planenum)
					side->texinfo = TEXINFO_NODE;
			}
		}


		if (sides & PSIDE_FRONT)
		{
			newbrush->next = *front;
			*front = newbrush;
			continue;
		}
		if (sides & PSIDE_BACK)
		{
			newbrush->next = *back;
			*back = newbrush;
			continue;
		}
	}
}


/*
================
BuildTree_r
================
*/
node_t *BuildTree_r (node_t *node, bspbrush_t *brushes)
{
	node_t		*newnode;
	side_t		*bestside;
	int			i;
	bspbrush_t	*children[2];

	if (GetNumThreads() == 1)
		c_nodes++;

	// find the best plane to use as a splitter
	bestside = SelectSplitSide (brushes, node);
	if (!bestside)
	{
		// leaf node
		node->side = NULL;
		node->planenum = -1;
		LeafNode (node, brushes);
		return node;
	}

	// this is a splitplane node
	node->side = bestside;
	node->planenum = bestside->planenum & ~1;	// always use front facing

	SplitBrushList (brushes, node, &children[0], &children[1]);
	FreeBrushList (brushes);

	// allocate children before recursing
	for (i=0 ; i<2 ; i++)
	{
		newnode = AllocNode ();
		newnode->parent = node;
		node->children[i] = newnode;
	}

	SplitBrush (node->volume, node->planenum, &node->children[0]->volume,
		&node->children[1]->volume);

	// recursively process children
	for (i=0 ; i<2 ; i++)
	{
		node->children[i] = BuildTree_r (node->children[i], children[i]);
	}

	return node;
}

//===========================================================

/*
=================
BrushBSP

The incoming list will be freed before exiting
=================
*/
tree_t *BrushBSP (bspbrush_t *brushlist, vec3_t mins, vec3_t maxs)
{
	node_t		*node;
	bspbrush_t	*b;
	int			c_faces, c_nonvisfaces;
	int			c_brushes;
	tree_t		*tree;
	int			i;
	vec_t		volume;

	tree = AllocTree ();

	c_faces = 0;
	c_nonvisfaces = 0;
	c_brushes = 0;
	for (b=brushlist ; b ; b=b->next)
	{
		c_brushes++;

		volume = BrushVolume (b);
		if (volume < microvolume)
		{
			Msg ("WARNING: entity %i, brush %i: microbrush\n", b->original->entitynum, b->original->brushnum);
		}

		for (i=0 ; i<b->numsides ; i++)
		{
			if (b->sides[i].bevel)
				continue;
			if (!b->sides[i].winding)
				continue;
			if (b->sides[i].texinfo == TEXINFO_NODE)
				continue;
			if (b->sides[i].visible)
				c_faces++;
			else
				c_nonvisfaces++;
		}

		AddPointToBounds (b->mins, tree->mins, tree->maxs);
		AddPointToBounds (b->maxs, tree->mins, tree->maxs);
	}

	c_nodes = 0;
	c_nonvis = 0;
	node = AllocNode ();

	node->volume = BrushFromBounds (mins, maxs);

	tree->headnode = node;

	node = BuildTree_r (node, brushlist);
	return tree;
}