/* -------------------------------------------------------------------------------

Copyright (C) 1999-2007 id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

----------------------------------------------------------------------------------

This code has been altered significantly from its original form, to support
several games based on the Quake III Arena engine, in the form of "Q3Map2."

------------------------------------------------------------------------------- */



/* marker */
#define BSP_C



/* dependencies */
#include "q3map2.h"
#include "stdio.h"		// sscanf


/* -------------------------------------------------------------------------------

functions

------------------------------------------------------------------------------- */


/*
ProcessAdvertisements()
copies advertisement info into the BSP structures
*/

static void ProcessAdvertisements( void ) {
	int					i;
	const char*			className;
	const char*			modelKey;
	int					modelNum;
	bspModel_t*			adModel;
	bspDrawSurface_t*	adSurface;

	MsgDev( D_NOTE, "--- ProcessAdvertisements ---\n" );

	for( i = 0; i < numEntities; i++ ) {

		/* is an advertisement? */
		className = ValueForKey( &entities[ i ], "classname" );

		if( !com.stricmp( "advertisement", className ) ) {
			
			modelKey = ValueForKey( &entities[ i ], "model" );

			if( strlen( modelKey ) > MAX_SHADERPATH - 1 ) {
				Sys_Break( "Model Key for entity exceeds ad struct string length." );
			} else {
				if( numBSPAds < MAX_MAP_ADVERTISEMENTS ) {
					bspAds[numBSPAds].cellId = IntForKey( &entities[ i ], "cellId" );
					strncpy( bspAds[numBSPAds].model, modelKey, sizeof( bspAds[numBSPAds].model ) );

					modelKey++;
					modelNum = atoi( modelKey );
					adModel = &bspModels[modelNum];
					
					if( adModel->numBSPSurfaces != 1 ) {
						Sys_Break( "Ad cell id %d has more than one surface.", bspAds[numBSPAds].cellId );
					}

					adSurface = &bspDrawSurfaces[adModel->firstBSPSurface];
					
					// store the normal for use at run time.. all ad verts are assumed to 
					// have identical normals (because they should be a simple rectangle)
					// so just use the first vert's normal
					VectorCopy( bspDrawVerts[adSurface->firstVert].normal, bspAds[numBSPAds].normal );

					// store the ad quad for quick use at run time
					if( adSurface->surfaceType == MST_PATCH ) {
						int v0 = adSurface->firstVert + adSurface->patchHeight - 1;
						int v1 = adSurface->firstVert + adSurface->numVerts - 1;
						int v2 = adSurface->firstVert + adSurface->numVerts - adSurface->patchWidth;
						int v3 = adSurface->firstVert;
						VectorCopy( bspDrawVerts[v0].xyz, bspAds[numBSPAds].rect[0] );
						VectorCopy( bspDrawVerts[v1].xyz, bspAds[numBSPAds].rect[1] );
						VectorCopy( bspDrawVerts[v2].xyz, bspAds[numBSPAds].rect[2] );
						VectorCopy( bspDrawVerts[v3].xyz, bspAds[numBSPAds].rect[3] );
					} else {
						Sys_Break( "Ad cell %d has an unsupported Ad Surface type.", bspAds[numBSPAds].cellId );
					}

					numBSPAds++;
				} else {
					Sys_Break( "Maximum number of map advertisements exceeded." );
				}
			}
		}
	}

	MsgDev( D_NOTE, "%9d in-game advertisements\n", numBSPAds );
}

/*
SetCloneModelNumbers() - ydnar
sets the model numbers for brush entities
*/

static void SetCloneModelNumbers( void )
{
	int			i, j;
	int			models;
	char		modelValue[ 10 ];
	const char	*value, *value2, *value3;
	
	
	/* start with 1 (worldspawn is model 0) */
	models = 1;
	for( i = 1; i < numEntities; i++ )
	{
		/* only entities with brushes or patches get a model number */
		if( entities[ i ].brushes == NULL && entities[ i ].patches == NULL )
			continue;
		
		/* is this a clone? */
		value = ValueForKey( &entities[ i ], "_clone" );
		if( value[ 0 ] != '\0' )
			continue;
		
		/* add the model key */
		com.sprintf( modelValue, "*%d", models );
		SetKeyValue( &entities[ i ], "model", modelValue );
		
		/* increment model count */
		models++;
	}
	
	/* fix up clones */
	for( i = 1; i < numEntities; i++ )
	{
		/* only entities with brushes or patches get a model number */
		if( entities[ i ].brushes == NULL && entities[ i ].patches == NULL )
			continue;
		
		/* is this a clone? */
		value = ValueForKey( &entities[ i ], "_ins" );
		if( value[ 0 ] == '\0' )
			value = ValueForKey( &entities[ i ], "_instance" );
		if( value[ 0 ] == '\0' )
			value = ValueForKey( &entities[ i ], "_clone" );
		if( value[ 0 ] == '\0' )
			continue;
		
		/* find an entity with matching clone name */
		for( j = 0; j < numEntities; j++ )
		{
			/* is this a clone parent? */
			value2 = ValueForKey( &entities[ j ], "_clonename" );
			if( value2[ 0 ] == '\0' )
				continue;
			
			/* do they match? */
			if( strcmp( value, value2 ) == 0 )
			{
				/* get the model num */
				value3 = ValueForKey( &entities[ j ], "model" );
				if( value3[ 0 ] == '\0' )
				{
					MsgDev( D_WARN, "Cloned entity %s referenced entity without model\n", value2 );
					continue;
				}
				models = atoi( &value2[ 1 ] );
				
				/* add the model key */
				com.sprintf( modelValue, "*%d", models );
				SetKeyValue( &entities[ i ], "model", modelValue );
				
				/* nuke the brushes/patches for this entity (fixme: leak!) */
				entities[ i ].brushes = NULL;
				entities[ i ].patches = NULL;
			}
		}
	}
}



/*
FixBrushSides() - ydnar
matches brushsides back to their appropriate drawsurface and shader
*/

static void FixBrushSides( entity_t *e )
{
	int					i;
	mapDrawSurface_t	*ds;
	sideRef_t			*sideRef;
	bspBrushSide_t		*side;
	
	
	/* note it */
	MsgDev( D_NOTE, "--- FixBrushSides ---\n" );
	
	/* walk list of drawsurfaces */
	for( i = e->firstDrawSurf; i < numMapDrawSurfs; i++ )
	{
		/* get surface and try to early out */
		ds = &mapDrawSurfs[ i ];
		if( ds->outputNum < 0 )
			continue;
		
		/* walk sideref list */
		for( sideRef = ds->sideRef; sideRef != NULL; sideRef = sideRef->next )
		{
			/* get bsp brush side */
			if( sideRef->side == NULL || sideRef->side->outputNum < 0 )
				continue;
			side = &bspBrushSides[ sideRef->side->outputNum ];
			
			/* set drawsurface */
			side->surfaceNum = ds->outputNum;
			//%	MsgDev( D_NOTE, "DS: %7d Side: %7d     ", ds->outputNum, sideRef->side->outputNum );
			
			/* set shader */
			if( strcmp( bspShaders[ side->shaderNum ].shader, ds->shaderInfo->shader ) )
			{
				//%	MsgDev( D_NOTE, "Remapping %s to %s\n", bspShaders[ side->shaderNum ].shader, ds->shaderInfo->shader );
				side->shaderNum = EmitShader( ds->shaderInfo->shader, &ds->shaderInfo->contentFlags, &ds->shaderInfo->surfaceFlags );
			}
		}
	}
}



/*
ProcessWorldModel()
creates a full bsp + surfaces for the worldspawn entity
*/

void ProcessWorldModel( void )
{
	int		i, s;
	entity_t		*e;
	tree_t		*tree;
	face_t		*faces;
	bool		ignoreLeaks, leaked;
	char		shader[MAX_SYSPATH];
	const char	*value;
	
	
	/* sets integer blockSize from worldspawn "_blocksize" key if it exists */
	value = ValueForKey( &entities[0], "_blocksize" );
	if( value[0] == '\0' ) value = ValueForKey( &entities[0], "blocksize" );
	if( value[0] == '\0' ) value = ValueForKey( &entities[ 0 ], "chopsize" );	/* sof2 */
	if( value[0] != '\0' )
	{
		/* scan 3 numbers */
		s = sscanf( value, "%d %d %d", &blockSize[ 0 ], &blockSize[ 1 ], &blockSize[ 2 ] );
		
		/* handle legacy case */
		if( s == 1 )
		{
			blockSize[ 1 ] = blockSize[ 0 ];
			blockSize[ 2 ] = blockSize[ 0 ];
		}
	}
	MsgDev( D_NOTE, "block size = { %d %d %d }\n", blockSize[ 0 ], blockSize[ 1 ], blockSize[ 2 ] );
	
	/* sof2: ignore leaks? */
	value = ValueForKey( &entities[ 0 ], "_ignoreleaks" );	/* ydnar */
	if( value[0] == '\0' ) value = ValueForKey( &entities[0], "ignoreleaks" );
	if( value[0] == '1' ) ignoreLeaks = true;
	else ignoreLeaks = false;
	
	/* begin worldspawn model */
	BeginModel();
	e = &entities[0];
	e->firstDrawSurf = 0;
	
	/* ydnar: gs mods */
	ClearMetaTriangles();

	/* check for patches with adjacent edges that need to lod together */
	PatchMapDrawSurfs( e );

	/* build an initial bsp tree using all of the sides of all of the structural brushes */
	faces = MakeStructuralBSPFaceList( entities[ 0 ].brushes );
	tree = FaceBSP( faces );
	MakeTreePortals( tree );
	FilterStructuralBrushesIntoTree( e, tree );
	
	/* see if the bsp is completely enclosed */
	if( FloodEntities( tree ) || ignoreLeaks )
	{
		/* rebuild a better bsp tree using only the sides that are visible from the inside */
		FillOutside( tree->headnode );

		/* chop the sides to the convex hull of their visible fragments, giving us the smallest polygons */
		ClipSidesIntoTree( e, tree );
		
		/* build a visible face tree */
		faces = MakeVisibleBSPFaceList( entities[ 0 ].brushes );
		FreeTree( tree );
		tree = FaceBSP( faces );
		MakeTreePortals( tree );
		FilterStructuralBrushesIntoTree( e, tree );
		leaked = false;
		
		/* ydnar: flood again for skybox */
		if( skyboxPresent )
			FloodEntities( tree );
	}
	else
	{
		Msg( "**********************\n" );
		Msg( "******* leaked *******\n" );
		Msg( "**********************\n" );
		LeakFile( tree );
		if( leaktest )
		{
			Sys_Break( "--- MAP LEAKED, ABORTING LEAKTEST ---\n" );
		}
		leaked = true;
		
		/* chop the sides to the convex hull of their visible fragments, giving us the smallest polygons */
		ClipSidesIntoTree( e, tree );
	}
	
	/* save out information for visibility processing */
	NumberClusters( tree );
	if( !leaked )
		WritePortalFile( tree );
	
	/* flood from entities */
	FloodAreas( tree );
	
	/* create drawsurfs for triangle models */
	AddTriangleModels( e );
	
	/* create drawsurfs for surface models */
	AddEntitySurfaceModels( e );
	
	/* generate bsp brushes from map brushes */
	EmitBrushes( e->brushes, &e->firstBrush, &e->numBrushes );
	
	/* add references to the detail brushes */
	FilterDetailBrushesIntoTree( e, tree );
	
	/* drawsurfs that cross fog boundaries will need to be split along the fog boundary */
	if( !nofog )
		FogDrawSurfaces( e );
	
	/* subdivide each drawsurf as required by shader tesselation */
	if( !nosubdivide )
		SubdivideFaceSurfaces( e, tree );
	
	/* add in any vertexes required to fix t-junctions */
	if( !notjunc )
		FixTJunctions( e );
	
	/* ydnar: classify the surfaces */
	ClassifyEntitySurfaces( e );
	
	/* ydnar: project decals */
	MakeEntityDecals( e );
	
	/* ydnar: meta surfaces */
	MakeEntityMetaTriangles( e );
	SmoothMetaTriangles();
	FixMetaTJunctions();
	MergeMetaTriangles();
	
	/* ydnar: debug portals */
	if( debugPortals )
		MakeDebugPortalSurfs( tree );
	
	/* ydnar: fog hull */
	value = ValueForKey( &entities[ 0 ], "_foghull" );
	if( value[ 0 ] != '\0' )
	{
		com.sprintf( shader, "textures/%s", value );
		MakeFogHullSurfs( e, tree, shader );
	}
	
	/* ydnar: bug 645: do flares for lights */
	for( i = 0; i < numEntities && emitFlares; i++ )
	{
		entity_t	*light, *target;
		const char	*value, *flareShader;
		vec3_t		origin, targetOrigin, normal, color;
		int			lightStyle;
		
		
		/* get light */
		light = &entities[ i ];
		value = ValueForKey( light, "classname" );
		if( !strcmp( value, "light" ) )
		{
			/* get flare shader */
			flareShader = ValueForKey( light, "_flareshader" );
			value = ValueForKey( light, "_flare" );
			if( flareShader[ 0 ] != '\0' || value[ 0 ] != '\0' )
			{
				/* get specifics */
				GetVectorForKey( light, "origin", origin );
				GetVectorForKey( light, "_color", color );
				lightStyle = IntForKey( light, "_style" );
				if( lightStyle == 0 )
					lightStyle = IntForKey( light, "style" );
				
				/* handle directional spotlights */
				value = ValueForKey( light, "target" );
				if( value[ 0 ] != '\0' )
				{
					/* get target light */
					target = FindTargetEntity( value );
					if( target != NULL )
					{
						GetVectorForKey( target, "origin", targetOrigin );
						VectorSubtract( targetOrigin, origin, normal );
						VectorNormalize( normal );
					}
				}
				else
					//%	VectorClear( normal );
					VectorSet( normal, 0, 0, -1 );
				
				/* create the flare surface (note shader defaults automatically) */
				DrawSurfaceForFlare( mapEntityNum, origin, normal, color, (char*) flareShader, lightStyle );
			}
		}
	}
	
	/* add references to the final drawsurfs in the apropriate clusters */
	FilterDrawsurfsIntoTree( e, tree );
	
	/* match drawsurfaces back to original brushsides (sof2) */
	FixBrushSides( e );
	
	/* finish */
	EndModel( e, tree->headnode );
	FreeTree( tree );
}



/*
ProcessSubModel()
creates bsp + surfaces for other brush models
*/

void ProcessSubModel( void )
{
	entity_t	*e;
	tree_t		*tree;
	brush_t		*b, *bc;
	node_t		*node;
	
	
	/* start a brush model */
	BeginModel();
	e = &entities[ mapEntityNum ];
	e->firstDrawSurf = numMapDrawSurfs;
	
	/* ydnar: gs mods */
	ClearMetaTriangles();
	
	/* check for patches with adjacent edges that need to lod together */
	PatchMapDrawSurfs( e );
	
	/* allocate a tree */
	node = AllocNode();
	node->planenum = PLANENUM_LEAF;
	tree = AllocTree();
	tree->headnode = node;
	
	/* add the sides to the tree */
	ClipSidesIntoTree( e, tree );
	
	/* ydnar: create drawsurfs for triangle models */
	AddTriangleModels( e );
	
	/* create drawsurfs for surface models */
	AddEntitySurfaceModels( e );
	
	/* generate bsp brushes from map brushes */
	EmitBrushes( e->brushes, &e->firstBrush, &e->numBrushes );

	/* just put all the brushes in headnode */
	for( b = e->brushes; b; b = b->next )
	{
		bc = CopyBrush( b );
		bc->next = node->brushlist;
		node->brushlist = bc;
	}
	
	/* subdivide each drawsurf as required by shader tesselation */
	if( !nosubdivide )
		SubdivideFaceSurfaces( e, tree );
	
	/* add in any vertexes required to fix t-junctions */
	if( !notjunc )
		FixTJunctions( e );
	
	/* ydnar: classify the surfaces and project lightmaps */
	ClassifyEntitySurfaces( e );
	
	/* ydnar: project decals */
	MakeEntityDecals( e );
	
	/* ydnar: meta surfaces */
	MakeEntityMetaTriangles( e );
	SmoothMetaTriangles();
	FixMetaTJunctions();
	MergeMetaTriangles();
	
	/* add references to the final drawsurfs in the apropriate clusters */
	FilterDrawsurfsIntoTree( e, tree );
	
	/* match drawsurfaces back to original brushsides (sof2) */
	FixBrushSides( e );
	
	/* finish */
	EndModel( e, node );
	FreeTree( tree );
}



/*
ProcessModels()
process world + other models into the bsp
*/

void ProcessModels( void )
{
	entity_t	*entity;
	
	/* start a new bsp */
	BeginBSPFile();
	
	/* create map fogs */
	CreateMapFogs();
	
	/* walk entity list */
	for( mapEntityNum = 0; mapEntityNum < numEntities; mapEntityNum++ )
	{
		/* get entity */
		entity = &entities[ mapEntityNum ];
		if( entity->brushes == NULL && entity->patches == NULL )
			continue;
		
		/* process the model */
		MsgDev( D_NOTE, "--- %s (%i) ---\n", (mapEntityNum) ? "Model" : "World", numBSPModels );
		if( mapEntityNum == 0 ) ProcessWorldModel();
		else ProcessSubModel();
	}
	
	/* write fogs */
	EmitFogs();
}



/*
OnlyEnts()
this is probably broken unless teamed with a radiant version that preserves entity order
*/

void OnlyEnts( void )
{
	char out[ 1024 ];

	
	/* note it */
	MsgDev( D_INFO, "--- OnlyEnts ---\n" );
	
	com.sprintf( out, "maps/%s.bsp", source );
	LoadBSPFile( out );
	numEntities = 0;

	LoadShaderInfo();
	LoadMapFile( name, false );
	SetModelNumbers();
	SetLightStyles();
	
	numBSPEntities = numEntities;
	UnparseEntities();
	
	WriteBSPFile( out );
}



/*
BSPMain() - ydnar
handles creation of a bsp from a map file
*/

int BSPMain( int argc, char **argv )
{
	int	i;
	char	path[ 1024 ];
	bool	onlyents = false;
	
	SetDrawSurfacesBuffer();
	mapDrawSurfs = Malloc( sizeof( mapDrawSurface_t ) * MAX_MAP_DRAW_SURFS );
	numMapDrawSurfs = 0;
	
	/* set standard game flags */
	maxSurfaceVerts = game->maxSurfaceVerts;
	maxSurfaceIndexes = game->maxSurfaceIndexes;
	emitFlares = game->emitFlares;

	if( FS_CheckParm( "-onlyents" )) onlyents = true;
	if( FS_CheckParm( "-nowater" )) nowater = true;
	if( FS_CheckParm( "-nodetail" )) nodetail = true;	
	if( FS_CheckParm( "-fulldetail" )) fulldetail = true;
	if( FS_CheckParm( "-nofog" )) nofog = true;
	if( FS_CheckParm( "-nosubdivide" )) nosubdivide = true;
	if( FS_CheckParm( "-leaktest" )) leaktest = true;
	if( FS_CheckParm( "-nocurves" )) noCurveBrushes = true;
	if( FS_CheckParm( "-notjunc" )) notjunc = true;

	/* process arguments */
	for( i = 1; i < (argc - 1); i++ )
	{
		if( !strcmp( argv[ i ], "-fakemap" ) )
		{
			MsgDev( D_INFO, "Generating fakemap.map\n" );
			fakemap = true;
		}
		else if( !strcmp( argv[ i ],  "-samplesize" ) )
 		{
			sampleSize = atoi( argv[ i + 1 ] );
			if( sampleSize < 1 )
				sampleSize = 1;
 			i++;
			MsgDev( D_INFO, "Lightmap sample size set to %dx%d units\n", sampleSize, sampleSize );
 		}
		else if( !strcmp( argv[ i ],  "-custinfoparms") )
		{
			MsgDev( D_INFO, "Custom info parms enabled\n" );
			useCustomInfoParms = true;
		}
		
		/* sof2 args */
		else if( !strcmp( argv[ i ], "-rename" ) )
		{
			MsgDev( D_INFO, "Appending _bsp suffix to misc_model shaders (SOF2)\n" );
			renameModelShaders = true;
		}
		
		/* ydnar args */
		else if( !strcmp( argv[ i ],  "-ne" ) )
 		{
			normalEpsilon = atof( argv[ i + 1 ] );
 			i++;
			MsgDev( D_INFO, "Normal epsilon set to %f\n", normalEpsilon );
 		}
		else if( !strcmp( argv[ i ],  "-de" ) )
 		{
			distanceEpsilon = atof( argv[ i + 1 ] );
 			i++;
			MsgDev( D_INFO, "Distance epsilon set to %f\n", distanceEpsilon );
 		}
		else if( !strcmp( argv[ i ],  "-mv" ) )
 		{
			maxLMSurfaceVerts = atoi( argv[ i + 1 ] );
			if( maxLMSurfaceVerts < 3 )
				maxLMSurfaceVerts = 3;
			if( maxLMSurfaceVerts > maxSurfaceVerts )
				maxSurfaceVerts = maxLMSurfaceVerts;
 			i++;
			MsgDev( D_INFO, "Maximum lightmapped surface vertex count set to %d\n", maxLMSurfaceVerts );
 		}
		else if( !strcmp( argv[ i ],  "-mi" ) )
 		{
			maxSurfaceIndexes = atoi( argv[ i + 1 ] );
			if( maxSurfaceIndexes < 3 )
				maxSurfaceIndexes = 3;
 			i++;
			MsgDev( D_INFO, "Maximum per-surface index count set to %d\n", maxSurfaceIndexes );
 		}
		else if( !strcmp( argv[ i ], "-np" ) )
		{
			npDegrees = atof( argv[ i + 1 ] );
			if( npDegrees < 0.0f )
				shadeAngleDegrees = 0.0f;
			else if( npDegrees > 0.0f )
				MsgDev( D_INFO, "Forcing nonplanar surfaces with a breaking angle of %f degrees\n", npDegrees );
			i++;
		}
		else if( !strcmp( argv[ i ],  "-snap" ) )
 		{
			bevelSnap = atoi( argv[ i + 1 ]);
			if( bevelSnap < 0 )
				bevelSnap = 0;
 			i++;
			if( bevelSnap > 0 )
				MsgDev( D_INFO, "Snapping brush bevel planes to %d units\n", bevelSnap );
 		}
		else if( !strcmp( argv[ i ],  "-texrange" ) )
 		{
			texRange = atoi( argv[ i + 1 ]);
			if( texRange < 0 )
				texRange = 0;
 			i++;
			MsgDev( D_INFO, "Limiting per-surface texture range to %d texels\n", texRange );
 		}
		else if( !strcmp( argv[ i ], "-nohint" ) )
		{
			MsgDev( D_INFO, "Hint brushes disabled\n" );
			noHint = true;
		}
		else if( !strcmp( argv[ i ], "-flat" ) )
		{
			MsgDev( D_INFO, "Flatshading enabled\n" );
			flat = true;
		}
		else if( !strcmp( argv[ i ], "-meta" ) )
		{
			MsgDev( D_INFO, "Creating meta surfaces from brush faces\n" );
			meta = true;
		}
		else if( !strcmp( argv[ i ], "-patchmeta" ) )
		{
			MsgDev( D_INFO, "Creating meta surfaces from patches\n" );
			patchMeta = true;
		}
		else if( !strcmp( argv[ i ], "-flares" ) )
		{
			MsgDev( D_INFO, "Flare surfaces enabled\n" );
			emitFlares = true;
		}
		else if( !strcmp( argv[ i ], "-noflares" ) )
		{
			MsgDev( D_INFO, "Flare surfaces disabled\n" );
			emitFlares = false;
		}
		else if( !strcmp( argv[ i ], "-skyfix" ) )
		{
			MsgDev( D_INFO, "GL_CLAMP sky fix/hack/workaround enabled\n" );
			skyFixHack = true;
		}
		else if( !strcmp( argv[ i ], "-debugsurfaces" ) )
		{
			MsgDev( D_INFO, "emitting debug surfaces\n" );
			debugSurfaces = true;
		}
		else if( !strcmp( argv[ i ], "-debuginset" ) )
		{
			MsgDev( D_INFO, "Debug surface triangle insetting enabled\n" );
			debugInset = true;
		}
		else if( !strcmp( argv[ i ], "-debugportals" ) )
		{
			MsgDev( D_INFO, "Debug portal surfaces enabled\n" );
			debugPortals = true;
		}
	}

	/* copy source name */
	strcpy( source, gs_filename );
	FS_StripExtension( source );
	
	/* ydnar: set default sample size */
	SetDefaultSampleSize( sampleSize );
	
	/* delete portal, line and surface files */
	com.sprintf( path, "%s/maps/%s.prt", GI->gamedir, source );
	FS_Delete( path );
	com.sprintf( path, "%s/maps/%s.lin", GI->gamedir, source );
	FS_Delete( path );
	com.sprintf( path, "%s/maps/%s.log", GI->gamedir, source );
	FS_Delete( path );
	com.snprintf( name, sizeof( name ), "maps/%s.map", gs_filename );	
	enable_log = true;

	if( onlyents )
	{
		OnlyEnts();
		return 0;
	}

	MsgDev( D_INFO, "--- BSP ---\n" );
	
	/* load shaders */
	LoadShaderInfo();

	/* load original file from temp spot in case it was renamed by the editor on the way in */
	LoadMapFile( name, false );
	
	/* ydnar: decal setup */
	ProcessDecals();
	
	/* ydnar: cloned brush model entities */
	SetCloneModelNumbers();
	
	/* process world and submodels */
	ProcessModels();
	
	/* set light styles from targetted light entities */
	SetLightStyles();
	
	/* process in game advertisements */
	ProcessAdvertisements();

	/* finish and write bsp */
	EndBSPFile();
	
	return 0;
}

