//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        qbsp.c - emit bsp nodes and leafs
//=======================================================================

#include "bsplib.h"
#include "const.h"

#define BLOCK_SIZE		8192	// test

int	block_xl = -8, block_xh = 7, block_yl = -8, block_yh = 7;
int	entity_num;
node_t	*block_nodes[10][10];

/*
============
BlockTree

============
*/
node_t *BlockTree (int xl, int yl, int xh, int yh)
{
	node_t	*node;
	vec3_t	normal;
	float	dist;
	int		mid;

	if (xl == xh && yl == yh)
	{
		node = block_nodes[xl+5][yl+5];
		if (!node)
		{	// return an empty leaf
			node = AllocNode ();
			node->planenum = PLANENUM_LEAF;
			node->contents = 0; //CONTENTS_SOLID;
			return node;
		}
		return node;
	}

	// create a seperator along the largest axis
	node = AllocNode ();

	if (xh - xl > yh - yl)
	{	// split x axis
		mid = xl + (xh-xl)/2 + 1;
		normal[0] = 1;
		normal[1] = 0;
		normal[2] = 0;
		dist = mid*BLOCK_SIZE;
		node->planenum = FindFloatPlane (normal, dist);
		node->children[0] = BlockTree ( mid, yl, xh, yh);
		node->children[1] = BlockTree ( xl, yl, mid-1, yh);
	}
	else
	{
		mid = yl + (yh-yl)/2 + 1;
		normal[0] = 0;
		normal[1] = 1;
		normal[2] = 0;
		dist = mid*BLOCK_SIZE;
		node->planenum = FindFloatPlane (normal, dist);
		node->children[0] = BlockTree ( xl, mid, xh, yh);
		node->children[1] = BlockTree ( xl, yl, xh, mid-1);
	}

	return node;
}

/*
============
ProcessBlock_Thread

============
*/
int brush_start, brush_end;

void ProcessBlock_Thread (int blocknum)
{
	int		xblock, yblock;
	vec3_t		mins, maxs;
	bspbrush_t	*brushes;
	tree_t		*tree;
	node_t		*node;

	yblock = block_yl + blocknum / (block_xh-block_xl+1);
	xblock = block_xl + blocknum % (block_xh-block_xl+1);

	mins[0] = xblock*BLOCK_SIZE;
	mins[1] = yblock*BLOCK_SIZE;
	mins[2] = MIN_WORLD_COORD;
	maxs[0] = (xblock+1)*BLOCK_SIZE;
	maxs[1] = (yblock+1)*BLOCK_SIZE;
	maxs[2] = MAX_WORLD_COORD;

	// the makelist and chopbrushes could be cached between the passes...
	brushes = MakeBspBrushList (brush_start, brush_end, mins, maxs);
	if (!brushes)
	{
		node = AllocNode ();
		node->planenum = PLANENUM_LEAF;
		node->contents = CONTENTS_SOLID;
		block_nodes[xblock+5][yblock+5] = node;
		return;
	}

//	brushes = ChopBrushes (brushes);
	tree = BrushBSP (brushes, mins, maxs);

	block_nodes[xblock+5][yblock+5] = tree->headnode;

	Mem_Free( tree );
}

/*
============
ProcessWorldModel

============
*/
void ProcessWorldModel( void )
{
	bsp_entity_t	*e;
	tree_t		*tree;
	bool	leaked;
	bool	optimize;

	e = &entities[entity_num];

	brush_start = e->firstbrush;
	brush_end = brush_start + e->numbrushes;
	leaked = false;

	//
	// perform per-block operations
	//
	if (block_xh * BLOCK_SIZE > map_maxs[0])
		block_xh = floor(map_maxs[0]/BLOCK_SIZE);
	if ( (block_xl+1) * BLOCK_SIZE < map_mins[0])
		block_xl = floor(map_mins[0]/BLOCK_SIZE);
	if (block_yh * BLOCK_SIZE > map_maxs[1])
		block_yh = floor(map_maxs[1]/BLOCK_SIZE);
	if ( (block_yl+1) * BLOCK_SIZE < map_mins[1])
		block_yl = floor(map_mins[1]/BLOCK_SIZE);

	if (block_xl <-4) block_xl = -4;
	if (block_yl <-4) block_yl = -4;
	if (block_xh > 3) block_xh = 3;
	if (block_yh > 3) block_yh = 3;

	for (optimize = false; optimize <= true; optimize++)
	{
		RunThreadsOnIndividual ((block_xh-block_xl+1)*(block_yh-block_yl+1), true, ProcessBlock_Thread);

		// build the division tree
		// oversizing the blocks guarantees that all the boundaries
		// will also get nodes.

		tree = AllocTree ();
		tree->headnode = BlockTree (block_xl-1, block_yl-1, block_xh+1, block_yh+1);

		tree->mins[0] = (block_xl)*BLOCK_SIZE;
		tree->mins[1] = (block_yl)*BLOCK_SIZE;
		tree->mins[2] = map_mins[2] - 8;

		tree->maxs[0] = (block_xh+1)*BLOCK_SIZE;
		tree->maxs[1] = (block_yh+1)*BLOCK_SIZE;
		tree->maxs[2] = map_maxs[2] + 8;

		//
		// perform the global operations
		//
		MakeTreePortals (tree);

		if (FloodEntities (tree))
		{
			FillOutside (tree->headnode);
		}
		else
		{
			Msg("**** leaked ****\n");
			leaked = true;
			LeakFile (tree);
		}

		MarkVisibleSides (tree, brush_start, brush_end);
		if (leaked) break;
		if (!optimize)
		{
			FreeTree (tree);
		}
	}

	FloodAreas(tree);
	MakeFaces(tree->headnode);
	FixTjuncs(tree->headnode);
	PruneNodes(tree->headnode);

	WriteBSP(tree->headnode);

	if(!leaked) WritePortalFile( tree );

	FreeTree( tree );
}

/*
============
ProcessSubModel

============
*/
void ProcessSubModel (void)
{
	bsp_entity_t	*e;
	int		start, end;
	tree_t		*tree;
	bspbrush_t	*list;
	vec3_t		mins, maxs;

	e = &entities[entity_num];

	start = e->firstbrush;
	end = start + e->numbrushes;

	mins[0] = mins[1] = mins[2] = MIN_WORLD_COORD;
	maxs[0] = maxs[1] = maxs[2] = MAX_WORLD_COORD;
	list = MakeBspBrushList (start, end, mins, maxs);
	list = ChopBrushes (list);
	tree = BrushBSP (list, mins, maxs);
	MakeTreePortals (tree);
	MarkVisibleSides (tree, start, end);
	MakeFaces (tree->headnode);
	FixTjuncs (tree->headnode);
	WriteBSP (tree->headnode);
	FreeTree (tree);
}

/*
============
ProcessModels
============
*/
void ProcessModels (void)
{
	BeginBSPFile ();
          
	for (entity_num = 0; entity_num < num_entities; entity_num++)
	{
		if (!entities[entity_num].numbrushes)
			continue;

		BeginModel ();
		if (entity_num == 0) ProcessWorldModel ();
		else ProcessSubModel ();
		EndModel ();
	}

	EndBSPFile ();
}

/*
============
WbspMain
============
*/
void WbspMain( void )
{
	Msg( "\n---- bsp ---- [%s]\n", (bsp_parms & BSPLIB_ONLYENTS) ? "onlyents" : "normal" );

	if(!( bsp_parms & BSPLIB_ONLYENTS ))
	{
		// delete portal and line files
		com.sprintf( path, "%s/maps/%s.prt", com.GameInfo->gamedir, gs_filename );
		FS_Delete( path );
		com.sprintf( path, "%s/maps/%s.lin", com.GameInfo->gamedir, gs_filename );
		FS_Delete( path );
	}

	// if onlyents, just grab the entites and resave
	if( bsp_parms & BSPLIB_ONLYENTS )
	{
		LoadBSPFile();
		num_entities = 0;

		LoadMapFile();
		SetModelNumbers();
		SetLightStyles();
		UnparseEntities();
		WriteBSPFile();
	}
	else
	{
		// start from scratch
		LoadMapFile();
		SetModelNumbers();
		SetLightStyles();
		ProcessModels();
		LoadBSPFile();
		ProcessCollisionTree();
		WriteBSPFile();
	}
}