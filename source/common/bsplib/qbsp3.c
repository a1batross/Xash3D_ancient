//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        qbsp.c - emit bsp nodes and leafs
//=======================================================================

#include "bsplib.h"
#include "const.h"

int	block_xl = -8, block_xh = 7, block_yl = -8, block_yh = 7;
int	entity_num;
node_t	*block_nodes[10][10];
int	c_nofaces;
int	c_facenodes;
int	firstmodleaf;

/*
=========================================================

ONLY SAVE OUT PLANES THAT ARE ACTUALLY USED AS NODES

=========================================================
*/
/*
============
EmitPlanes

There is no oportunity to discard planes, because all of the original
brushes will be saved in the map.
============
*/
void EmitPlanes( void )
{
	int		i;
	dplane_t		*dp;
	plane_t		*mp;

	mp = mapplanes;
	for( i = 0; i < nummapplanes; i++, mp++ )
	{
		dp = &dplanes[numplanes];
		VectorCopy( mp->normal, dp->normal );
		dp->dist = mp->dist;
		numplanes++;
	}
}


//========================================================

void EmitMarkFace( dleaf_t *leaf_p, bspface_t *f )
{
	int	i;
	int	facenum;

	while( f->merged ) f = f->merged;

	if( f->split[0] )
	{
		EmitMarkFace( leaf_p, f->split[0] );
		EmitMarkFace( leaf_p, f->split[1] );
		return;
	}

	facenum = f->outputnumber;
	if( facenum == -1 ) return; // degenerate face

	if( facenum < 0 || facenum >= numfaces )
		Sys_Break( "bad leafface\n" );
	for( i = leaf_p->firstleafface; i < numleaffaces; i++ )
		if( dleaffaces[i] == facenum ) break; // merged out face

	if( i == numleaffaces )
	{
		if( numleaffaces >= MAX_MAP_LEAFFACES )
			Sys_Break( "MAX_MAP_LEAFFACES limit exceeded\n" );
		dleaffaces[numleaffaces] =  facenum;
		numleaffaces++;
	}
}


/*
==================
EmitLeaf
==================
*/
void EmitLeaf( node_t *node )
{
	portal_t		*p;
	bspface_t		*f;
	bspbrush_t	*b;
	dleaf_t		*leaf_p;
	int		s;

	// emit a leaf
	if( numleafs >= MAX_MAP_LEAFS ) Sys_Break( "MAX_MAP_LEAFS linit exceeded\n" );

	leaf_p = &dleafs[numleafs];
	numleafs++;

	leaf_p->contents = node->contents;
	leaf_p->cluster = node->cluster;
	leaf_p->area = node->area;

	// write bounding box info
	VectorCopy( node->mins, leaf_p->mins );
	VectorCopy( node->maxs, leaf_p->maxs );
	
	// write the leafbrushes
	leaf_p->firstleafbrush = numleafbrushes;
	for( b = node->brushlist; b; b = b->next )
	{
		if( numleafbrushes >= MAX_MAP_LEAFBRUSHES )
			Sys_Break( "MAX_MAP_LEAFBRUSHES\n" );
		dleafbrushes[numleafbrushes] = b->original->outputnum;
		numleafbrushes++;
	}
	leaf_p->numleafbrushes = numleafbrushes - leaf_p->firstleafbrush;

	// write the leaffaces
	if( leaf_p->contents & CONTENTS_SOLID ) return; // no leaffaces in solids

	leaf_p->firstleafface = numleaffaces;
	for( p = node->portals; p; p = p->next[s] )	
	{
		s = (p->nodes[1] == node);
		f = p->face[s];
		if( !f ) continue;	// not a visible portal
		EmitMarkFace( leaf_p, f );
	}
	leaf_p->numleaffaces = numleaffaces - leaf_p->firstleafface;
}


/*
==================
EmitFace
==================
*/
void EmitFace( bspface_t *f )
{
	dface_t	*df;
	int	i, e;

	f->outputnumber = -1;

	if( f->numpoints < 3 ) return; // degenerated
	if( f->merged || f->split[0] || f->split[1] )
		return; // not a final face

	// save output number so leaffaces can use
	f->outputnumber = numfaces;

	if( numfaces >= MAX_MAP_FACES )
		Sys_Break( "MAX_MAP_FACES limit exceeded\n" );
	df = &dfaces[numfaces];
	numfaces++;

	// planenum is used by qlight, but not quake
	df->planenum = f->planenum & (~1);
	df->side = f->planenum & 1;

	df->firstedge = numsurfedges;
	df->numedges = f->numpoints;
	df->texinfo = f->texinfo;

	// FIXME: emit indices here, not edges
	for( i = 0; i < f->numpoints; i++ )
	{
		e = GetEdge( f->vertexnums[i], f->vertexnums[(i+1)%f->numpoints], f );
		if( numsurfedges >= MAX_MAP_SURFEDGES )
			Sys_Break( "MAX_MAP_SURFEDGES limit exceeded\n" );
		dsurfedges[numsurfedges] = e;
		numsurfedges++;
	}
}

/*
============
EmitDrawingNode_r
============
*/
int EmitDrawNode_r( node_t *node )
{
	dnode_t	*n;
	bspface_t	*f;
	int	i;

	if( node->planenum == PLANENUM_LEAF )
	{
		EmitLeaf( node );
		return -numleafs;
	}

	// emit a node	
	if( numnodes == MAX_MAP_NODES )
		Sys_Break( "MAX_MAP_NODES limit exceeded\n" );
	n = &dnodes[numnodes];
	numnodes++;

	VectorCopy( node->mins, n->mins );
	VectorCopy( node->maxs, n->maxs );

	if( node->planenum & 1 )
		Sys_Break( "WriteDrawNodes_r: odd planenum\n" );
	n->planenum = node->planenum;
	n->firstface = numfaces;

	if( !node->faces ) c_nofaces++;
	else c_facenodes++;

	for( f = node->faces; f; f = f->next )
		EmitFace( f );
	n->numfaces = numfaces - n->firstface;

	// recursively output the other nodes
	for( i = 0; i < 2; i++ )
	{
		if( node->children[i]->planenum == PLANENUM_LEAF )
		{
			n->children[i] = -(numleafs + 1);
			EmitLeaf( node->children[i] );
		}
		else
		{
			n->children[i] = numnodes;	
			EmitDrawNode_r( node->children[i] );
		}
	}
	return n - dnodes;
}

//===========================================================

/*
============
SetModelNumbers
============
*/
void SetModelNumbers( void )
{
	int	i, models = 1;

	for( i = 1; i<num_entities; i++ )
	{
		if( entities[i].brushes )
		{
			SetKeyValue( &entities[i], "model", va( "*%i", models ));
			models++;
		}
	}
}

/*
============
SetLightStyles
============
*/

void SetLightStyles( void )
{
	char		*t;
	bsp_entity_t	*e;
	int		i, j, k, stylenum = 0;
	char		lighttargets[64][MAX_QPATH];


	// any light that is controlled (has a targetname)
	// must have a unique style number generated for it
	for( i = 1; i < num_entities; i++ )
	{
		e = &entities[i];

		t = ValueForKey( e, "classname" );
		if( com.strncmp( t, "light", 5 ))
		{
			if(!com.strncmp( t, "func_light", 10 ))
			{
				// may create func_light family 
				// e.g. func_light_fluoro, func_light_broken etc
				k = com.atoi(ValueForKey( e, "spawnflags" ));
				if( k & SF_START_ON ) t = "-2"; // initially on
				else t = "-1"; // initially off
			}
			else t = ValueForKey( e, "style" );
			
			switch( com.atoi( t ))
			{
			case 0: continue; // not a light, no style, generally pretty boring
			case -1: // normal switchable texlight (start off)
				SetKeyValue( e, "style", va( "%i", 32 + stylenum ));
				stylenum++;
				continue;
			case -2: // backwards switchable texlight (start on)
				SetKeyValue(e, "style", va( "%i", -(32 + stylenum )));
				stylenum++;
				continue;
			default: continue; // nothing to set
			}
                    }
		t = ValueForKey( e, "targetname" );
		if( !t[0] ) continue;
		
		// find this targetname
		for( j = 0; j < stylenum; j++ )
		{
			if( !com.strcmp( lighttargets[j], t ))
				break; // already exist
		}
		if( j == stylenum )
		{
			if( stylenum == 64 )
			{
				Msg( "switchable lightstyles limit (%i) exceeded at entity #%i\n", 64, i );
				break; // nothing to process
			}
			com.strncpy( lighttargets[j], t, MAX_QPATH );
			stylenum++;
		}
		SetKeyValue( e, "style", va( "%i", 32 + j ));
	}

}

//===========================================================

/*
============
EmitBrushes
============
*/
void EmitBrushes( bspbrush_t *brushes )
{
	int		j;
	dbrush_t		*db;
	bspbrush_t	*b;
	dbrushside_t	*cp;

	for( b = brushes; b; b = b->next )
	{
		if( numbrushes == MAX_MAP_BRUSHES ) Sys_Break( "MAX_MAP_BRUSHES limit exceeded\n" );
		b->outputnum = numbrushes;

		db = &dbrushes[numbrushes];
		numbrushes++;

		db->contents = b->contents;
		db->firstside = numbrushsides;
		db->numsides = 0;

		for( j = 0; j < b->numsides; j++ )
		{
			if( numbrushsides == MAX_MAP_BRUSHSIDES )
				Sys_Break( "MAX_MAP_BRUSHSIDES limit exceeded\n" );
			cp = &dbrushsides[numbrushsides];
			db->numsides++;
			numbrushsides++;
			cp->planenum = b->sides[j].planenum;
			cp->texinfo = b->sides[j].texinfo;
		}
	}
}

//===========================================================

/*
==================
BeginBSPFile
==================
*/
void BeginBSPFile( void )
{
	// these values may actually be initialized
	// if the file existed when loaded, so clear them explicitly

	nummodels = 0;
	numfaces = 0;
	numnodes = 0;
	numbrushsides = 0;
	numvertexes = 0;
	numleaffaces = 0;
	numleafbrushes = 0;
	numsurfedges = 0;
	numedges = 1;			// edge 0 is not used, because 0 can't be negated.
	numvertexes = 1;			// leave vertex 0 as an error
	numleafs = 1;			// leave leaf 0 as an error
	numindexes = 1;			// store number of initial vertex					
	dleafs[0].contents = CONTENTS_SOLID;	// assume error
}


/*
============
EndBSPFile
============
*/
void EndBSPFile( void )
{
	EmitPlanes();
	UnparseEntities();

	// write the map
	WriteBSPFile();
}


/*
==================
BeginModel
==================
*/
void BeginModel( void )
{
	bspbrush_t	*b;
	bsp_entity_t	*e;
	dmodel_t		*mod;
	vec3_t		mins, maxs;

	if( nummodels == MAX_MAP_MODELS )
		Sys_Break( "MAX_MAP_MODELS limit exceeded\n" );
	mod = &dmodels[nummodels];

	// bound the brushes
	e = &entities[entity_num];

	ClearBounds( mins, maxs );
	for( b = e->brushes; b; b = b->next )
	{
		if( !b->numsides ) continue;	// not a real brush (origin brush, etc)
		AddPointToBounds (b->mins, mins, maxs);
		AddPointToBounds (b->maxs, mins, maxs);
	}

	VectorCopy( mins, mod->mins );
	VectorCopy( maxs, mod->maxs );

	mod->firstface = numfaces;
	mod->firstbrush = numbrushes;

	EmitBrushes( e->brushes );
}


/*
==================
EndModel
==================
*/
void EndModel( node_t *headnode )
{
	dmodel_t	*mod;

	mod = &dmodels[nummodels];
	dmodels[nummodels].headnode = EmitDrawNode_r( headnode );
	mod->numfaces = numfaces - mod->firstface;
	mod->numbrushes = numbrushes - mod->firstbrush;
	EmitAreaPortals( headnode );
	nummodels++;
}

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
void ProcessBlock_Thread( int blocknum )
{
	int		xblock, yblock;
	vec3_t		mins, maxs;
	bspbrush_t	*brushes, *start;
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

	start = entities[entity_num].brushes;

	// the makelist and chopbrushes could be cached between the passes...
	brushes = MakeBspBrushList( start, mins, maxs, false );
	if( !brushes )
	{
		node = AllocNode ();
		node->planenum = PLANENUM_LEAF;
		node->contents = CONTENTS_SOLID;
		block_nodes[xblock+5][yblock+5] = node;
		return;
	}

	brushes = ChopBrushes( brushes );
	tree = BrushBSP( brushes, mins, maxs );

	block_nodes[xblock+5][yblock+5] = tree->headnode;
}

/*
============
ProcessWorldModel

============
*/
void ProcessWorldModel( void )
{
	tree_t		*tree;
	bool		optimize;
	bool		leaked = false;

	BeginModel();

	// perform per-block operations
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

		MarkVisibleSides( tree, entities[entity_num].brushes );
		if( leaked ) break;
		if( !optimize ) FreeTree( tree );
	}

	FloodAreas(tree);
	MakeFaces(tree->headnode);
	FixTjuncs(tree->headnode);
	PruneNodes(tree->headnode);

	EndModel( tree->headnode );

	if(!leaked) WritePortalFile( tree );

	FreeTree( tree );
}

/*
============
ProcessSubModel

============
*/
void ProcessSubModel( void )
{
	bsp_entity_t	*e;
	tree_t		*tree;
	bspbrush_t	*list;
	vec3_t		mins, maxs;

	BeginModel();

	e = &entities[entity_num];

	mins[0] = mins[1] = mins[2] = -131072;
	maxs[0] = maxs[1] = maxs[2] = 131072;
	list = MakeBspBrushList( e->brushes, mins, maxs, false );
	list = ChopBrushes( list );
	tree = BrushBSP( list, mins, maxs );
	MakeTreePortals( tree );
	MarkVisibleSides( tree, e->brushes );
	MakeFaces (tree->headnode);
	FixTjuncs (tree->headnode);
	EndModel( tree->headnode );
	FreeTree( tree );
}

/*
============
ProcessModels
============
*/
void ProcessModels (void)
{
	BeginBSPFile ();
          
	for( entity_num = 0; entity_num < num_entities; entity_num++ )
	{
		if( !entities[entity_num].brushes )
			continue;
		if( !entity_num )
			ProcessWorldModel();
		else ProcessSubModel();
	}

	EndBSPFile();
}

/*
============
WbspMain
============
*/
void WbspMain ( bool option )
{
	Msg("---- CSG ---- [%s]\n", onlyents ? "onlyents" : "normal" );

	// if onlyents, just grab the entites and resave
	if( option )
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
