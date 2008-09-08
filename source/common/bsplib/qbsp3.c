//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        qbsp.c - emit bsp nodes and leafs
//=======================================================================

#include "bsplib.h"
#include "const.h"

int	entity_num;
/*
=========================================================

ONLY SAVE OUT PLANES THAT ARE ACTUALLY USED AS NODES

=========================================================
*/
/*
============
EmitShader
============
*/
int EmitShader( const char *shader )
{
	shader_t	*si;
	int	i;

	// force to create default shader
	if( !shader ) shader = "default";

	for( i = 0; i < numshaders; i++ )
	{
		if( !com.stricmp( shader, dshaders[i].name ))
			return i;
	}

	if( i == MAX_MAP_SHADERS ) Sys_Break( "MAX_MAP_SHADERS limit exceeded\n" );
	numshaders++;
	com.strncpy( dshaders[i].name, shader, MAX_QPATH );

	si = FindShader( shader );
	dshaders[i].flags = si->surfaceFlags;
	dshaders[i].contents = si->contents;

	return i;
}

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

/*
==================
EmitLeaf
==================
*/
void EmitLeaf( node_t *node )
{
	dleaf_t		*leaf_p;
	bspbrush_t	*b;
	surfaceref_t	*dsr;

	// emit a leaf
	if( numleafs >= MAX_MAP_LEAFS ) Sys_Break( "MAX_MAP_LEAFS limit exceeded\n" );

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
			Sys_Break( "MAX_MAP_LEAFBRUSHES limit exceeded\n" );
		dleafbrushes[numleafbrushes] = b->original->outputnum;
		numleafbrushes++;
	}
	leaf_p->numleafbrushes = numleafbrushes - leaf_p->firstleafbrush;

	// write the surfaces visible in this leaf
	if( node->opaque ) return; // no leaffaces in solids
	
	// add the drawSurfRef_t drawsurfs
	leaf_p->firstleafface = numleaffaces;
	for( dsr = node->surfaces; dsr; dsr = dsr->next )
	{
		if( numleaffaces >= MAX_MAP_LEAFFACES )
			Sys_Break( "MAX_MAP_LEAFFACES limit exceeded\n" );
		dleaffaces[numleaffaces] = dsr->outputnum;
		numleaffaces++;			
	}
	leaf_p->numleaffaces = numleaffaces - leaf_p->firstleafface;
}

/*
============
EmitDrawNode_r
============
*/
int EmitDrawNode_r( node_t *node )
{
	dnode_t		*n;
	int		i;

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
			cp->shadernum = EmitShader( b->sides[j].shader->name );
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
	numnodes = 0;
	numbrushsides = 0;
	numleaffaces = 0;
	numleafbrushes = 0;

	numleafs = 1;			// leave leaf 0 as an error
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

	mod->firstface = numsurfaces;
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
	EmitDrawNode_r( headnode );
	mod->numfaces = numsurfaces - mod->firstface;
	mod->numbrushes = numbrushes - mod->firstbrush;
	nummodels++;
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
	bspface_t		*faces;
	bool		leaked;

	MsgDev( D_INFO, "\n==================\n" );
	MsgDev( D_INFO, "Process world model" );
	MsgDev( D_INFO, "\n==================\n" );
	BeginModel();

	e = &entities[0];
	e->firstsurf = 0;

	// build an initial bsp tree using all of the sides
	// of all of the structural brushes
	faces = MakeStructuralBspFaceList ( entities[0].brushes );
	tree = FaceBSP( faces );
	MakeTreePortals( tree );
	FilterStructuralBrushesIntoTree( e, tree );

	// see if the bsp is completely enclosed
	if( FloodEntities( tree ))
	{
		// rebuild a better bsp tree using only the
		// sides that are visible from the inside
		FillOutside (tree->headnode);

		// chop the sides to the convex hull of
		// their visible fragments, giving us the smallest
		// polygons 
		ClipSidesIntoTree( e, tree );

		faces = MakeVisibleBspFaceList( entities[0].brushes );
		FreeTree (tree);
		tree = FaceBSP( faces );
		MakeTreePortals( tree );
		FilterStructuralBrushesIntoTree( e, tree );
		leaked = false;
	}
	else
	{
		Msg( "******* leaked *******\n" );
		LeakFile( tree );
		leaked = true;

		// chop the sides to the convex hull of
		// their visible fragments, giving us the smallest
		// polygons 
		ClipSidesIntoTree( e, tree );
	}

	// save out information for visibility processing
	NumberClusters( tree );
	if( !leaked ) WritePortalFile( tree );
	FloodAreas( tree );

	// add references to the detail brushes
	FilterDetailBrushesIntoTree( e, tree );

	// subdivide each drawsurf as required by shader tesselation
	if( !nosubdivide ) SubdivideDrawSurfs( e, tree );

	// add in any vertexes required to fix tjunctions
	if( !notjunc ) FixTJunctions( e );

	// allocate lightmaps for faces and patches
	AllocateLightmaps( e );

	// add references to the final drawsurfs in the apropriate clusters
	FilterDrawsurfsIntoTree( e, tree );

	EndModel( tree->headnode );
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
	bspbrush_t	*b, *bc;
	node_t		*node;

	MsgDev( D_INFO, "\n==================\n" );
	MsgDev( D_INFO, "Process submodel #%i", entity_num );
	MsgDev( D_INFO, "\n==================\n" );
	BeginModel();

	e = &entities[entity_num];
	e->firstsurf = numdrawsurfs;

	// just put all the brushes in an empty leaf
	node = AllocNode();
	node->planenum = PLANENUM_LEAF;
	for( b = e->brushes; b; b = b->next )
	{
		bc = CopyBrush( b );
		bc->next = node->brushlist;
		node->brushlist = bc;
	}

	tree = AllocTree();
	tree->headnode = node;

	ClipSidesIntoTree( e, tree );

	// subdivide each drawsurf as required by shader tesselation or fog
	if( !nosubdivide ) SubdivideDrawSurfs( e, tree );

	// add in any vertexes required to fix tjunctions
	if( !notjunc ) FixTJunctions( e );

	// allocate lightmaps for faces and patches
	AllocateLightmaps( e );

	// add references to the final drawsurfs in the apropriate clusters
	FilterDrawsurfsIntoTree( e, tree );

	EndModel ( node );
	FreeTree( tree );
}

/*
============
ProcessModels
============
*/
void ProcessModels( void )
{
	BeginBSPFile();
          
	for( entity_num = 0; entity_num < num_entities; entity_num++ )
	{
		if( !entities[entity_num].brushes )
			continue;
		if( !entity_num ) ProcessWorldModel();
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
	Msg("---- BSP ---- [%s]\n", onlyents ? "onlyents" : "normal" );

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
