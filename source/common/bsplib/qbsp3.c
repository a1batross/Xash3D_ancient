// csg4.c
#include "bsplib.h"

char	outbase[32];
int	block_xl = -8, block_xh = 7, block_yl = -8, block_yh = 7;
int	entity_num;
bool	onlyents;
char	path[MAX_SYSPATH];
node_t *block_nodes[10][10];

bool full_compile = false;
bool onlyents = false;
bool onlyvis = false;
bool onlyrad = false;

dll_info_t physic_dll = { "physic.dll", NULL, "CreateAPI", NULL, NULL, false, sizeof(physic_exp_t) };
physic_exp_t *pe;

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
		dist = mid*32768;
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
		dist = mid*32768;
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

	mins[0] = xblock*32768;
	mins[1] = yblock*32768;
	mins[2] = -131072;
	maxs[0] = (xblock+1)*32768;
	maxs[1] = (yblock+1)*32768;
	maxs[2] = 131072;

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

	brushes = ChopBrushes (brushes);
	tree = BrushBSP (brushes, mins, maxs);

	block_nodes[xblock+5][yblock+5] = tree->headnode;
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
	if (block_xh * 32768 > map_maxs[0])
		block_xh = floor(map_maxs[0]/32768.0);
	if ( (block_xl+1) * 32768 < map_mins[0])
		block_xl = floor(map_mins[0]/32768.0);
	if (block_yh * 32768 > map_maxs[1])
		block_yh = floor(map_maxs[1]/32768.0);
	if ( (block_yl+1) * 32768 < map_mins[1])
		block_yl = floor(map_mins[1]/32768.0);

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

		tree->mins[0] = (block_xl)*32768;
		tree->mins[1] = (block_yl)*32768;
		tree->mins[2] = map_mins[2] - 8;

		tree->maxs[0] = (block_xh+1)*32768;
		tree->maxs[1] = (block_yh+1)*32768;
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

	mins[0] = mins[1] = mins[2] = -131072;
	maxs[0] = maxs[1] = maxs[2] = 131072;
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

static void AddCollision(void* handle, const void* buffer, size_t size)
{
	Mem_Copy( dcollision + dcollisiondatasize, (void *)buffer, size );
	dcollisiondatasize += size;
}

void ProcessCollisionTree( void )
{
	if( !physic_dll.link ) return;

	dcollisiondatasize = 0;
	pe->WriteCollisionLump( NULL, AddCollision );
}

void Init_PhysicsLibrary( void )
{
	static physic_imp_t		pi;
	launch_t			CreatePhysic;

	pi.api_size = sizeof(physic_imp_t);
	Sys_LoadLibrary( &physic_dll );

	if(physic_dll.link)
	{
		CreatePhysic = (void *)physic_dll.main;
		pe = CreatePhysic( &com, &pi ); // sys_error not overrided
		pe->Init(); // initialize phys callback
	}
	else memset( &pe, 0, sizeof(pe));
}

void Free_PhysicLibrary( void )
{
	if(physic_dll.link)
	{
		pe->Shutdown();
		memset( &pe, 0, sizeof(pe));
	}
	Sys_FreeLibrary( &physic_dll );
}

/*
============
WbspMain
============
*/
void WbspMain ( bool option )
{
	onlyents = option;
	
	Msg("---- CSG ---- [%s]\n", onlyents ? "onlyents" : "normal" );

	// delete portal and line files
	com.sprintf (path, "%s/maps/%s.prt", com.GameInfo->gamedir, gs_filename);
	remove(path);
	com.sprintf (path, "%s/maps/%s.lin", com.GameInfo->gamedir, gs_filename);
	remove(path);

	// if onlyents, just grab the entites and resave
	if( onlyents )
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

bool PrepareBSPModel ( const char *dir, const char *name, byte params )
{
	int	numshaders;
	
	if( dir ) com.strncpy(gs_basedir, dir, sizeof(gs_basedir));
	if( name ) com.strncpy(gs_filename, name, sizeof(gs_filename));

	// copy state
	onlyents = (params & BSP_ONLYENTS) ? true : false;
	onlyvis = (params & BSP_ONLYVIS) ? true : false ;
	onlyrad = (params & BSP_ONLYRAD) ? true : false;
	full_compile = (params & BSP_FULLCOMPILE) ? true : false;

	FS_LoadGameInfo( "gameinfo.txt" ); // same as normal gamemode

	Init_PhysicsLibrary();
	numshaders = LoadShaderInfo();
	Msg( "%5i shaderInfo\n", numshaders );

	return true;
}

bool CompileBSPModel ( void )
{
	// must be first!
	if( onlyents ) WbspMain( true );
	else if( onlyvis && !onlyrad ) WvisMain ( full_compile );
	else if( onlyrad && !onlyvis ) WradMain( full_compile );
	else if( onlyrad && onlyvis )
	{
		WbspMain( false );
		WvisMain( full_compile );
		WradMain( full_compile );
	}
          else WbspMain( false ); // just create bsp

	Free_PhysicLibrary();

	if( onlyrad && onlyvis && full_compile )
	{
		// delete all temporary files after final compile
		com.sprintf(path, "%s/maps/%s.prt", com.GameInfo->gamedir, gs_filename);
		remove(path);
		com.sprintf(path, "%s/maps/%s.lin", com.GameInfo->gamedir, gs_filename);
		remove(path);
		com.sprintf(path, "%s/maps/%s.log", com.GameInfo->gamedir, gs_filename);
		remove(path);
	}

	return true;
}