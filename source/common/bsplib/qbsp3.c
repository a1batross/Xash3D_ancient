//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        qbsp.c - emit bsp nodes and leafs
//=======================================================================

#include "bsplib.h"
#include "const.h"

int	entity_num;
bool	emitFlares;
bool	debugPortals;

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
int EmitShader( const char *shader, int *contents, int *surfaceFlags )
{
	bsp_shader_t	*si;
	int		i;

	// force to create default shader
	if( !shader ) shader = "noshader";

	for( i = 0; i < numshaders; i++ )
	{
		// handle custom surface/content flags
		if( surfaceFlags != NULL && dshaders[i].surfaceFlags != *surfaceFlags )
			continue;
		if( contents != NULL && dshaders[i].contents != *contents )
			continue;
		if( !com.stricmp( shader, dshaders[i].name ))
			return i;
	}

	si = FindShader( shader );
	if( i == MAX_MAP_SHADERS ) Sys_Break( "MAX_MAP_SHADERS limit exceeded\n" );
	numshaders++;

	// FXIME: check name and cutoff "textures/" if necessary, engine have spport for it
	com.strncpy( dshaders[i].name, shader, MAX_QPATH );
	dshaders[i].surfaceFlags = si->surfaceFlags;
	dshaders[i].contents = si->contents;

	// handle custom content/surface flags
	if( surfaceFlags != NULL ) dshaders[i].surfaceFlags = *surfaceFlags;
	if( contents != NULL ) dshaders[i].contents = *contents;

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
	leaf_p->firstleafsurface = numleaffaces;
	for( dsr = node->surfaces; dsr; dsr = dsr->next )
	{
		if( numleaffaces >= MAX_MAP_LEAFFACES )
			Sys_Break( "MAX_MAP_LEAFFACES limit exceeded\n" );
		dleaffaces[numleaffaces] = dsr->outputnum;
		numleaffaces++;			
	}
	leaf_p->numleafsurfaces = numleaffaces - leaf_p->firstleafsurface;
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

	for( i = 1; i < num_entities; i++ )
	{
		if( entities[i].brushes || entities[i].patches )
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
	int		i, j, k, style, stylenum = 0;
	char		lightTargets[MAX_SWITCHED_LIGHTS][64];
	int		lightStyles[MAX_SWITCHED_LIGHTS];

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

		// get existing style
		style = IntForKey( e, "style" );
		if( style < LS_NORMAL || style > LS_NONE )
			Sys_Break( "Invalid lightstyle (%d) on entity %d\n", style, i );
		
		// find this targetname
		for( j = 0; j < stylenum; j++ )
		{
			if( lightStyles[j] == style && !com.strcmp( lightTargets[j], t ))
				break;
		}
		if( j == stylenum )
		{
			if( stylenum == MAX_SWITCHED_LIGHTS )
			{
				MsgDev( D_WARN, "switchable lightstyles limit (%i) exceeded at entity #%i\n", 64, i );
				break; // nothing to process
			}
			com.strncpy( lightTargets[j], t, MAX_QPATH );
			lightStyles[j] = style;
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
void EmitBrushes( bspbrush_t *brushes, int *firstBrush, int *numBrushes )
{
	int		j;
	dbrush_t		*db;
	bspbrush_t	*b;
	dbrushside_t	*cp;

	if( firstBrush ) *firstBrush = numbrushes;
	if( numBrushes ) *numBrushes = 0;

	for( b = brushes; b; b = b->next )
	{
		if( numbrushes == MAX_MAP_BRUSHES ) Sys_Break( "MAX_MAP_BRUSHES limit exceeded\n" );
		b->outputnum = numbrushes;

		db = &dbrushes[numbrushes];
		numbrushes++;
		if( numBrushes ) (*numBrushes)++;

		db->shadernum = EmitShader( b->shader->name, &b->shader->contents, &b->shader->surfaceFlags );
		db->firstside = numbrushsides;
		db->numsides = 0;

		for( j = 0; j < b->numsides; j++ )
		{
			// set output number to bogus initially
			b->sides[j].outputnum = -1;
			
			if( numbrushsides == MAX_MAP_BRUSHSIDES )
				Sys_Break( "MAX_MAP_BRUSHSIDES limit exceeded\n" );
			b->sides[j].outputnum = numbrushsides;
			cp = &dbrushsides[numbrushsides];
			db->numsides++;
			numbrushsides++;
			cp->planenum = b->sides[j].planenum;
			if( b->sides[j].shader )			
				cp->shadernum = EmitShader( b->sides[j].shader->name, &b->sides[j].shader->contents, &b->sides[j].shader->surfaceFlags );
			else cp->shadernum = EmitShader( NULL, NULL, NULL );
		}
	}
}

/*
============
EmitFogs

turns map fogs into bsp fogs
============
*/
void EmitFogs( void )
{
	int	i, j;
	
	numfogs = numMapFogs;
	
	// walk list
	for( i = 0; i < numMapFogs; i++ )
	{
		com.strcpy( dfogs[i].shader, mapFogs[i].si->name );
		
		// global fog doesn't have an associated brush
		if( mapFogs[i].brush == NULL )
		{
			dfogs[i].brushnum = -1;
			dfogs[i].visibleSide = -1;
		}
		else
		{
			dfogs[i].brushnum = mapFogs[i].brush->outputnum;
			
			// try to use forced visible side
			if( mapFogs[i].visibleSide >= 0 )
			{
				dfogs[i].visibleSide = mapFogs[i].visibleSide;
				continue;
			}
			
			// find visible side
			for( j = 0; j < 6; j++ )
			{
				if( mapFogs[i].brush->sides[j].visibleHull != NULL )
				{
					MsgDev( D_INFO, "Fog %d has visible side %d\n", i, j );
					dfogs[i].visibleSide = j;
					break;
				}
			}
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

	// set the first 6 drawindexes to 0 1 2 2 1 3 for triangles and quads
	numindexes = 6;
	dindexes[0] = 0;
	dindexes[1] = 1;
	dindexes[2] = 2;
	dindexes[3] = 0;
	dindexes[4] = 2;
	dindexes[5] = 3;
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

	// write the surface extra file
	WriteSurfaceExtraFile();

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
	vec3_t		lgMins, lgMaxs;	// lightgrid mins/maxs
	parseMesh_t	*p;
	int		i;

	if( nummodels == MAX_MAP_MODELS )
		Sys_Break( "MAX_MAP_MODELS limit exceeded\n" );
	mod = &dmodels[nummodels];

	// bound the brushes
	e = &entities[entity_num];

	ClearBounds( mins, maxs );
	ClearBounds( lgMins, lgMaxs );

	for( b = e->brushes; b; b = b->next )
	{
		if( !b->numsides ) continue;	// not a real brush (origin brush, etc)
		AddPointToBounds (b->mins, mins, maxs);
		AddPointToBounds (b->maxs, mins, maxs);

		// lightgrid bounds
		if( b->contents & CONTENTS_LIGHTGRID )
		{
			AddPointToBounds( b->mins, lgMins, lgMaxs );
			AddPointToBounds( b->maxs, lgMins, lgMaxs );
		}
	}

	for( p = e->patches; p; p = p->next )
	{
		for( i = 0; i < (p->mesh.width * p->mesh.height); i++ )
			AddPointToBounds( p->mesh.verts[i].point, mins, maxs );
	}

	if( lgMins[0] < 99999 )
	{
		// use lightgrid bounds
		VectorCopy( lgMins, mod->mins );
		VectorCopy( lgMaxs, mod->maxs );
	}
	else
	{
		// use brush/patch bounds
		VectorCopy( mins, mod->mins );
		VectorCopy( maxs, mod->maxs );
	}

	mod->firstface = numsurfaces;
	mod->firstbrush = numbrushes;
}


/*
==================
EndModel
==================
*/
void EndModel( bsp_entity_t *e, node_t *headnode )
{
	dmodel_t	*mod;

	mod = &dmodels[nummodels];
	EmitDrawNode_r( headnode );

	mod->numfaces = numsurfaces - mod->firstface;
	mod->firstbrush = e->firstBrush;
	mod->numbrushes = e->numBrushes;
	nummodels++;
}

/*
==================
SetCloneModelNumbers

sets the model numbers for brush entities
==================
*/
static void SetCloneModelNumbers( void )
{
	int		i, j;
	int		models;
	const char	*value, *value2, *value3;
	
	
	// start with 1 (worldspawn is model 0)
	models = 1;
	for( i = 1; i < num_entities; i++ )
	{
		// only entities with brushes or patches get a model number
		if( entities[i].brushes == NULL && entities[i].patches == NULL )
			continue;
		
		// is this a clone? 
		value = ValueForKey( &entities[i], "_clone" );
		if( value[0] != '\0' ) continue;
		
		// add the model key
		SetKeyValue( &entities[i], "model", va("*%d", models ));
		models++;
	}
	
	// fix up clones
	for( i = 1; i < num_entities; i++ )
	{
		// only entities with brushes or patches get a model number
		if( entities[i].brushes == NULL && entities[i].patches == NULL )
			continue;
		
		// is this a clone?
		value = ValueForKey( &entities[ i ], "_ins" );
		if( value[0] == '\0' ) value = ValueForKey( &entities[ i ], "_instance" );
		if( value[0] == '\0' ) value = ValueForKey( &entities[ i ], "_clone" );
		if( value[0] == '\0' ) continue;
		
		// find an entity with matching clone name
		for( j = 0; j < num_entities; j++ )
		{
			// is this a clone parent?
			value2 = ValueForKey( &entities[j], "_clonename" );
			if( value2[0] == '\0' ) continue;
			
			// do they match?
			if( !com.strcmp( value, value2 ))
			{
				value3 = ValueForKey( &entities[j], "model" );
				if( value3[0] == '\0' )
				{
					MsgDev( D_WARN, "Cloned entity %s referenced entity without model\n", value2 );
					continue;
				}
				models = com.atoi( &value2[1] );
				
				SetKeyValue( &entities[ i ], "model", va( "*%d", models ));
				
				// nuke the brushes/patches for this entity
				// not a leak, Zone Memory will be freed at end of compile 
				entities[i].brushes = NULL;
				entities[i].patches = NULL;
			}
		}
	}
}



/*
==================
FixBrushSides

matches brushsides back to their appropriate drawsurface and shader
==================
*/
static void FixBrushSides( bsp_entity_t *e )
{
	int		i;
	drawsurf_t	*ds;
	sideRef_t		*sideRef;
	dbrushside_t	*side;
	
	MsgDev( D_NOTE, "--- FixBrushSides ---\n" );
	
	// walk list of drawsurfaces
	for( i = e->firstsurf; i < numdrawsurfs; i++ )
	{
		ds = &drawsurfs[i];
		if( ds->outputnum < 0 ) continue;
		
		for( sideRef = ds->sideRef; sideRef != NULL; sideRef = sideRef->next )
		{
			/* get bsp brush side */
			if( sideRef->side == NULL || sideRef->side->outputnum < 0 )
				continue;
			side = &dbrushsides[sideRef->side->outputnum];
			side->surfacenum = ds->outputnum;
			
			if( com.strcmp( dshaders[side->shadernum].name, ds->shader->name ))
				side->shadernum = EmitShader( ds->shader->name, &ds->shader->contents, &ds->shader->surfaceFlags );
		}
	}
}

/*
============
ProcessWorldModel

============
*/
void ProcessWorldModel( void )
{
	int		i;
	bsp_entity_t	*e;
	tree_t		*tree;
	bspface_t		*faces;
	bool		leaked;
	string		shader;
	const char	*value;
	
	MsgDev( D_INFO, "\n==================\n" );
	MsgDev( D_INFO, "Process world model" );
	MsgDev( D_INFO, "\n==================\n" );
	BeginModel();

	e = &entities[0];
	e->firstsurf = 0;

	ClearMetaTriangles();

	// check for patches with adjacent edges that need to lod together
	PatchMapDrawSurfs( e );

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

	// create drawsurfs for triangle models
	AddTriangleModels( e );

	// create drawsurfs for surface models
	AddEntitySurfaceModels( e );

	// generate bsp brushes from map brushes
	EmitBrushes( e->brushes, &e->firstBrush, &e->numBrushes );

	// add references to the detail brushes
	FilterDetailBrushesIntoTree( e, tree );

	// drawsurfs that cross fog boundaries will need to be split along the fog boundary
	FogDrawSurfaces( e );

	// subdivide each drawsurf as required by shader tesselation
	if( !nosubdivide ) SubdivideFaceSurfaces( e, tree );

	// add in any vertexes required to fix tjunctions
	if( !notjunc ) FixTJunctions( e );

	// ydnar: classify the surfaces
	ClassifyEntitySurfaces( e );

	// project decals
	MakeEntityDecals( e );

	MakeEntityMetaTriangles( e );
	SmoothMetaTriangles();
	FixMetaTJunctions();
	MergeMetaTriangles();

	// debug portals
	if( debugPortals ) MakeDebugPortalSurfs( tree );

	// global fog hull
	value = ValueForKey( &entities[0], "_foghull" );
	if( value[0] != '\0' )
	{
		com.sprintf( shader, "textures/%s", value );
		MakeFogHullSurfs( e, tree, shader );
	}
	
	// do flares for lights
	for( i = 0; i < num_entities && emitFlares; i++ )
	{
		bsp_entity_t	*light, *target;
		const char	*value, *flareShader;
		vec3_t		origin, targetOrigin, normal, color;
		int		lightStyle;
		
		light = &entities[ i ];
		value = ValueForKey( light, "classname" );
		if( !com.strcmp( value, "light" ) )
		{
			flareShader = ValueForKey( light, "_flareshader" );
			value = ValueForKey( light, "_flare" );
			if( flareShader[0] != '\0' || value[0] != '\0' )
			{
				GetVectorForKey( light, "origin", origin );
				GetVectorForKey( light, "_color", color );
				lightStyle = IntForKey( light, "_style" );
				if( lightStyle == 0 ) lightStyle = IntForKey( light, "style" );
				
				// handle directional spotlights
				value = ValueForKey( light, "target" );
				if( value[0] != '\0' )
				{
					target = FindTargetEntity( value );
					if( target != NULL )
					{
						GetVectorForKey( target, "origin", targetOrigin );
						VectorSubtract( targetOrigin, origin, normal );
						VectorNormalize( normal );
					}
				}
				else VectorSet( normal, 0, 0, -1 );
				
				// create the flare surface (note shader defaults automatically)
				DrawSurfaceForFlare( entity_num, origin, normal, color, (char*)flareShader, lightStyle );
			}
		}
	}

	// add references to the final drawsurfs in the apropriate clusters
	FilterDrawsurfsIntoTree( e, tree );

	EndModel( e, tree->headnode );
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

	ClearMetaTriangles();
	
	// check for patches with adjacent edges that need to lod together
	PatchMapDrawSurfs( e );

	// just put all the brushes in an empty leaf
	node = AllocNode();
	node->planenum = PLANENUM_LEAF;
	tree = AllocTree();
	tree->headnode = node;

	ClipSidesIntoTree( e, tree );

	// create drawsurfs for triangle models
	AddTriangleModels( e );
	
	// create drawsurfs for surface models
	AddEntitySurfaceModels( e );

	// generate bsp brushes from map brushes
	EmitBrushes( e->brushes, &e->firstBrush, &e->numBrushes );

	// just put all the brushes in headnode
	for( b = e->brushes; b; b = b->next )
	{
		bc = CopyBrush( b );
		bc->next = node->brushlist;
		node->brushlist = bc;
	}

	// subdivide each drawsurf as required by shader tesselation or fog
	if( !nosubdivide ) SubdivideFaceSurfaces( e, tree );

	// add in any vertexes required to fix tjunctions
	if( !notjunc ) FixTJunctions( e );

	// classify the surfaces and project lightmaps
	ClassifyEntitySurfaces( e );

	// project decals
	MakeEntityDecals( e );

	// ydnar: meta surfaces
	MakeEntityMetaTriangles( e );
	SmoothMetaTriangles();
	FixMetaTJunctions();
	MergeMetaTriangles();

	// add references to the final drawsurfs in the apropriate clusters
	FilterDrawsurfsIntoTree( e, tree );

	// match drawsurfaces back to original brushsides
	FixBrushSides( e );

	EndModel( e, node );
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
	CreateMapFogs();
          
	for( entity_num = 0; entity_num < num_entities; entity_num++ )
	{
		if( !entities[entity_num].brushes && !entities[entity_num].patches )
			continue;
		if( !entity_num ) ProcessWorldModel();
		else ProcessSubModel();
	}

	EmitFogs();
	SetLightStyles();
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
		// create bsp and some subinfo
		LoadMapFile();
		ProcessDecals();
		SetCloneModelNumbers();
		ProcessModels();

		// create collision tree
		LoadBSPFile();
		ProcessCollisionTree();
		WriteBSPFile();
	}
}
