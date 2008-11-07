//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	fog.c - create fog volumes
//=======================================================================

#include "bsplib.h"

int	defaultFogNum = -1;
int	numMapFogs;
int	numFogFragments;
int	numFogPatchFragments;

fog_t	mapFogs[MAX_MAP_FOGS];

/*
=============
DrawSurfToMesh

converts a patch drawsurface to a mesh_t
=============
*/
bsp_mesh_t *DrawSurfToMesh( drawsurf_t *ds )
{
	bsp_mesh_t	*m;
	
	m = BSP_Malloc( sizeof( *m ));
	m->width = ds->patchWidth;
	m->height = ds->patchHeight;
	m->verts = BSP_Malloc( sizeof(m->verts[0]) * m->width * m->height );
	Mem_Copy( m->verts, ds->verts, sizeof(m->verts[0]) * m->width * m->height );
	
	return m;
}

/*
=============
SplitMeshByPlane

chops a mesh by a plane
=============
*/
void SplitMeshByPlane( bsp_mesh_t *in, vec3_t normal, float dist, bsp_mesh_t **front, bsp_mesh_t **back )
{
	int		w, h, split;
	float		d[MAX_PATCH_SIZE][MAX_PATCH_SIZE];
	dvertex_t		*dv, *v1, *v2;
	int		c_front, c_back, c_on;
	bsp_mesh_t	*f, *b;
	int		i;
	float		frac;
	int		frontAprox, backAprox;

	for( i = 0; i < 2; i++ )
	{
		dv = in->verts;
		c_front = c_back = c_on = 0;

		for( h = 0; h < in->height; h++ )
		{
			for( w = 0; w < in->width; w++, dv++ )
			{
				d[h][w] = DotProduct( dv->point, normal ) - dist;
				if( d[h][w] > ON_EPSILON )
					c_front++;
				else if( d[h][w] < -ON_EPSILON )
					c_back++;
				else c_on++;
			}
		}

		*front = NULL;
		*back = NULL;

		if( !c_front )
		{
			*back = in;
			return;
		}
		if( !c_back )
		{
			*front = in;
			return;
		}

		// find a split point
		split = -1;
		for( w = 0; w < in->width -1; w++ )
		{
			if(( d[0][w] < 0 ) != ( d[0][w+1] < 0 ))
			{
				if( split == -1 )
				{
					split = w;
					break;
				}
			}
		}

		if( split == -1 )
		{
			if( i == 1 )
			{
				MsgDev( D_NOTE, "No crossing points in patch\n" );
				*front = in;
				return;
			}

			in = TransposeMesh( in );
			InvertMesh( in );
			continue;
		}

		// make sure the split point stays the same for all other rows
		for ( h = 1; h < in->height; h++ )
		{
			for( w = 0; w < in->width - 1; w++ )
			{
				if(( d[h][w] < 0 ) != ( d[h][w+1] < 0 ))
				{
					if( w != split )
					{
						MsgDev( D_WARN, "multiple crossing points for patch - can't clip\n" );
						*front = in;
						return;
					}
				}
			}
			if(( d[h][split] < 0 ) == ( d[h][split+1] < 0 ))
			{
				MsgDev( D_WARN, "differing crossing points for patch - can't clip\n" );
				*front = in;
				return;
			}
		}
		break;
	}


	// create two new meshes
	f = BSP_Malloc( sizeof( *f ));
	f->width = split + 2;
	if(!( f->width & 1 ))
	{
		f->width++;
		frontAprox = 1;
	}
	else frontAprox = 0;

	if( f->width > MAX_PATCH_SIZE )
		Sys_Break( "MAX_PATCH_SIZE after split\n" );

	f->height = in->height;
	f->verts = BSP_Malloc( sizeof(f->verts[0]) * f->width * f->height );

	b = BSP_Malloc( sizeof( *b ));
	b->width = in->width - split;
	if(!( b->width & 1 ))
	{
		b->width++;
		backAprox = 1;
	}
	else backAprox = 0;

	if( b->width > MAX_PATCH_SIZE )
		Sys_Break( "MAX_PATCH_SIZE after split\n" );

	b->height = in->height;
	b->verts = BSP_Malloc( sizeof(b->verts[0]) * b->width * b->height );

	if( d[0][0] > 0 )
	{
		*front = f;
		*back = b;
	}
	else
	{
		*front = b;
		*back = f;
	}

	// distribute the points
	for( w = 0; w < in->width; w++ )
	{
		for( h = 0; h < in->height; h++ )
		{
			if( w <= split )
			{
				f->verts[h * f->width + w] = in->verts[h * in->width + w];
			}
			else
			{
				b->verts[h * b->width + w - split + backAprox] = in->verts[h * in->width + w];
			}
		}
	}

	// clip the crossing line
	for ( h = 0; h < in->height; h++ )
	{
		dv = &f->verts[ h * f->width + split + 1 ];
		v1 = &in->verts[ h * in->width + split ];
		v2 = &in->verts[ h * in->width + split + 1 ];

		frac = d[h][split] / ( d[h][split] - d[h][split+1] );
		
		LerpDrawVertAmount( v1, v2, frac, dv );
		
		if( frontAprox ) f->verts[ h * f->width + split + 2 ] = *dv;
		b->verts[ h * b->width ] = *dv;
		if( backAprox ) b->verts[ h * b->width + 1 ] = *dv;
	}
	FreeMesh( in );
}

/*
=============
ChopPatchSurfaceByBrush

chops a patch up by a fog brush
=============
*/
bool ChopPatchSurfaceByBrush( bsp_entity_t *e, drawsurf_t *ds, bspbrush_t *b )
{
	int			i, j;
	side_t		*s;
	plane_t		*plane;
	bsp_mesh_t		*outside[MAX_BUILD_SIDES];
	int			numOutside;
	bsp_mesh_t		*m, *front, *back;
	drawsurf_t	*newds;

	m = DrawSurfToMesh( ds );
	numOutside = 0;
	
	// only split by the top and bottom planes to avoid
	// some messy patch clipping issues
	
	for( i = 4; i <= 5; i++ )
	{
		s = &b->sides[ i ];
		plane = &mapplanes[ s->planenum ];

		SplitMeshByPlane( m, plane->normal, plane->dist, &front, &back );

		if( !back )
		{
			// nothing actually contained inside
			for( j = 0; j < numOutside; j++ )
			{
				FreeMesh( outside[j] );
			}
			return false;
		}
		m = back;

		if ( front )
		{
			if( numOutside == MAX_BUILD_SIDES )
			{
				Sys_Break( "MAX_BUILD_SIDES" );
			}
			outside[ numOutside ] = front;
			numOutside++;
		}
	}

	// all of outside fragments become seperate drawsurfs
	numFogPatchFragments += numOutside;
	for( i = 0; i < numOutside; i++ )
	{
		// transpose and invert the chopped patch (fixes potential crash. FIXME: why?)
		outside[i] = TransposeMesh( outside[i] );
		InvertMesh( outside[i] );
		
		// do this the hacky right way
		newds = AllocDrawSurf( SURFACE_PATCH );
		memcpy( newds, ds, sizeof( *ds ));
		newds->patchWidth = outside[i]->width;
		newds->patchHeight = outside[i]->height;
		newds->numVerts = outside[i]->width * outside[i]->height;
		newds->verts = BSP_Malloc( newds->numVerts * sizeof( *newds->verts ));
		Mem_Copy( newds->verts, outside[i]->verts, newds->numVerts * sizeof( *newds->verts ));
		
		// free the source mesh
		FreeMesh( outside[i] );
	}
	
	// only rejigger this patch if it was chopped
	if( numOutside > 0 )
	{
		// transpose and invert the chopped patch (fixes potential crash. FIXME: why?)
		m = TransposeMesh( m );
		InvertMesh( m );
		
		// replace ds with m
		ds->patchWidth = m->width;
		ds->patchHeight = m->height;
		ds->numVerts = m->width * m->height;
		BSP_Free( ds->verts );
		ds->verts = BSP_Malloc( ds->numVerts * sizeof( *ds->verts ));
		Mem_Copy( ds->verts, m->verts, ds->numVerts * sizeof( *ds->verts ));
	}
	
	// free the source mesh and return
	FreeMesh( m );
	return true;
}

/*
=============
ChopFaceSurfaceByBrush

chops up a face drawsurface by a fog brush, with a potential fragment left inside
=============
*/
bool ChopFaceSurfaceByBrush( bsp_entity_t *e, drawsurf_t *ds, bspbrush_t *b )
{
	int		i, j;
	side_t		*s;
	plane_t		*plane;
	winding_t		*w;
	winding_t		*front, *back;
	winding_t		*outside[MAX_BUILD_SIDES];
	int		numOutside;
	drawsurf_t	*newds;
	
	if( ds->sideRef == NULL || ds->sideRef->side == NULL )
		return false;
	
	w = WindingFromDrawSurf( ds );
	numOutside = 0;
	
	// chop by each brush side
	for( i = 0; i < b->numsides; i++ )
	{
		s = &b->sides[i];
		plane = &mapplanes[ s->planenum ];
		
		// handle coplanar outfacing (don't fog)
		if( ds->sideRef->side->planenum == s->planenum )
			return false;
		
		// handle coplanar infacing (keep inside)
		if( (ds->sideRef->side->planenum ^ 1) == s->planenum )
			continue;
		
		// general case
		ClipWindingEpsilon( w, plane->normal, plane->dist, ON_EPSILON, &front, &back );
		FreeWinding( w );
		
		if( back == NULL )
		{
			// nothing actually contained inside
			for( j = 0; j < numOutside; j++ )
				FreeWinding( outside[ j ] );
			return false;
		}
		
		if( front != NULL )
		{
			if( numOutside == MAX_BUILD_SIDES )
				Sys_Break( "MAX_BRUSH_SIDES limit exceeded\n" );
			outside[numOutside] = front;
			numOutside++;
		}
		w = back;
	}
	
	// all of outside fragments become seperate drawsurfs
	numFogFragments += numOutside;
	s = ds->sideRef->side;
	for( i = 0; i < numOutside; i++ )
	{
		newds = DrawSurfaceForSide( e, ds->mapBrush, s, outside[i] );
		newds->fogNum = ds->fogNum;
		FreeWinding( outside[i] );
	}
	
	// the old code neglected to snap to 0.125 for the fragment
	// inside the fog brush, leading to sparklies. this new code does
	// the right thing and uses the original surface's brush side
	
	// build a drawsurf for it
	newds = DrawSurfaceForSide( e, ds->mapBrush, s, w );
	if( newds == NULL ) return false;
	
	ClearSurface( ds );
	Mem_Copy( ds, newds, sizeof( drawsurf_t ));
	
	// didn't really add a new drawsurface...
	numdrawsurfs--;
	
	return true;
}



/*
=============
FogDrawSurfaces

call after the surface list has been pruned, before tjunction fixing
=============
*/
void FogDrawSurfaces( bsp_entity_t *e )
{
	int		i, j, k, fogNum;
	fog_t		*fog;
	drawsurf_t	*ds;
	vec3_t		mins, maxs;
	int		fogged, numFogged;
	int		numBaseDrawSurfs;
	
	MsgDev( D_NOTE, "----- FogDrawSurfs -----\n" );
	
	numFogged = 0;
	numFogFragments = 0;
	
	for( fogNum = 0; fogNum < numMapFogs; fogNum++ )
	{
		fog = &mapFogs[ fogNum ];
		
		// clip each surface into this, but don't clip any of the resulting fragments to the same brush
		numBaseDrawSurfs = numdrawsurfs;
		for( i = 0; i < numBaseDrawSurfs; i++ )
		{
			ds = &drawsurfs[i];
			
			if( ds->shader->noFog ) continue;
			
			// global fog doesn't have a brush
			if( fog->brush == NULL )
			{
				// don't re-fog already fogged surfaces
				if( ds->fogNum >= 0 ) continue;
				fogged = 1;
			}
			else
			{
				// find drawsurface bounds
				ClearBounds( mins, maxs );
				for( j = 0; j < ds->numVerts; j++ )
					AddPointToBounds( ds->verts[j].point, mins, maxs );

				// check against the fog brush
				for( k = 0; k < 3; k++ )
				{
					if( mins[k] > fog->brush->maxs[k] )
						break;
					if( maxs[k] < fog->brush->mins[k] )
						break;
				}
				if( k < 3 ) continue;
				
				// handle the various types of surfaces
				switch( ds->type )
				{
				case SURFACE_FACE:
					fogged = ChopFaceSurfaceByBrush( e, ds, fog->brush );
					break;
				case SURFACE_PATCH:
					fogged = ChopPatchSurfaceByBrush( e, ds, fog->brush );
					break;
				// FIXME: split triangle surfaces
				case SURFACE_TRIANGLES:
				case SURFACE_FORCED_META:
				case SURFACE_META:
					fogged = 1;
					break;
				default:
					fogged = 0;
					break;
				}
			}
			if( fogged )
			{
				numFogged += fogged;
				ds->fogNum = fogNum;
			}
		}
	}
	
	MsgDev( D_INFO, "%9d fog polygon fragments\n", numFogFragments );
	MsgDev( D_INFO, "%9d fog patch fragments\n", numFogPatchFragments );
	MsgDev( D_INFO, "%9d fogged drawsurfs\n", numFogged );
}



/*
=============
FogForPoint

gets the fog number for a point in space
=============
*/
int FogForPoint( vec3_t point, float epsilon )
{
	int		fogNum, i, j;
	float		dot;
	bool		inside;
	bspbrush_t	*brush;
	plane_t		*plane;
	
	// start with bogus fog num
	fogNum = defaultFogNum;
	
	for( i = 0; i < numMapFogs; i++ )
	{
		// global fog doesn't reference a brush
		if( mapFogs[i].brush == NULL )
		{
			fogNum = i;
			continue;
		}
		
		brush = mapFogs[i].brush;
		
		// check point against all planes
		inside = true;
		for( j = 0; j < brush->numsides && inside; j++ )
		{
			plane = &mapplanes[brush->sides[j].planenum]; // note usage of map planes here
			dot = DotProduct( point, plane->normal );
			dot -= plane->dist;
			if( dot > epsilon ) inside = false;
		}
		
		// if inside, return the fog num
		if( inside ) return i;
	}
	
	// if the point made it this far, it's not inside any fog volumes (or inside global fog)
	return fogNum;
}



/*
=============
FogForBounds

gets the fog number for a bounding box
=============
*/
int FogForBounds( vec3_t mins, vec3_t maxs, float epsilon )
{
	int		fogNum, i, j;
	float		highMin, lowMax, volume, bestVolume;
	vec3_t		fogMins, fogMaxs, overlap;
	bspbrush_t	*brush;
	
	
	fogNum = defaultFogNum;
	bestVolume = 0.0f;
	
	for( i = 0; i < numMapFogs; i++ )
	{
		// global fog doesn't reference a brush
		if( mapFogs[i].brush == NULL )
		{
			fogNum = i;
			continue;
		}
		
		brush = mapFogs[i].brush;
		
		fogMins[0] = brush->mins[0] - epsilon;
		fogMins[1] = brush->mins[1] - epsilon;
		fogMins[2] = brush->mins[2] - epsilon;
		fogMaxs[0] = brush->maxs[0] + epsilon;
		fogMaxs[1] = brush->maxs[1] + epsilon;
		fogMaxs[2] = brush->maxs[2] + epsilon;
		
		for( j = 0; j < 3; j++ )
		{
			if( mins[j] > fogMaxs[j] || maxs[j] < fogMins[j] )
				break;
			highMin = mins[j] > fogMins[j] ? mins[j] : fogMins[j];
			lowMax = maxs[j] < fogMaxs[j] ? maxs[j] : fogMaxs[j];
			overlap[j] = lowMax - highMin;
			if( overlap[j] < 1.0f ) overlap[j] = 1.0f;
		}
		
		if( j < 3 ) continue;
		
		volume = overlap[0] * overlap[1] * overlap[2];
		if( volume > bestVolume )
		{
			bestVolume = volume;
			fogNum = i;
		}
	}
	
	// if the point made it this far, it's not inside any fog volumes (or inside global fog)
	return fogNum;
}



/*
=============
CreateMapFogs

generates a list of map fogs
=============
*/
void CreateMapFogs( void )
{
	int		i;
	bsp_entity_t	*entity;
	bspbrush_t	*brush;
	fog_t		*fog;
	vec3_t		invFogDir;
	const char	*globalFog;
	
	MsgDev( D_INFO, "--- CreateMapFogs ---\n" );
	
	for( i = 0; i < num_entities; i++ )
	{
		entity = &entities[i];
		
		for( brush = entity->brushes; brush != NULL; brush = brush->next )
		{
			if( brush->shader->fogParms == false )
				continue;
			
			if( numMapFogs >= MAX_MAP_FOGS )
				Sys_Break( "MAX_MAP_FOGS limit exceeded\n" );
			
			fog = &mapFogs[numMapFogs++];
			fog->si = brush->shader;
			fog->brush = brush;
			fog->visibleSide = -1;
			
			// if shader specifies an explicit direction,
			// then find a matching brush side with an opposed normal
			if( VectorLength( fog->si->fogDir ) )
			{
				VectorScale( fog->si->fogDir, -1.0f, invFogDir );
				
				for( i = 0; i < brush->numsides; i++ )
				{
					if( VectorCompare( invFogDir, mapplanes[brush->sides[i].planenum].normal ))
					{
						fog->visibleSide = i;
						break;
					}
				}
			}
		}
	}
	
	// global fog
	globalFog = ValueForKey( &entities[0], "_fog" );
	if( globalFog[0] == '\0' ) globalFog = ValueForKey( &entities[0], "fog" );

	if( globalFog[0] != '\0' )
	{
		if( numMapFogs >= MAX_MAP_FOGS )
			Sys_Break( "MAX_MAP_FOGS limit exceeded(%d) trying to add global fog\n", MAX_MAP_FOGS );
		
		MsgDev( D_INFO, "Map has global fog shader %s\n", globalFog );
		
		fog = &mapFogs[numMapFogs++];
		fog->si = FindShader( globalFog );
		if( fog->si == NULL )
			Sys_Break( "Invalid shader \"%s\" referenced trying to add global fog\n", globalFog );
		fog->brush = NULL;
		fog->visibleSide = -1;
		defaultFogNum = numMapFogs - 1;
		
		// mark all worldspawn brushes as fogged
		for( brush = entities[0].brushes; brush != NULL; brush = brush->next )
			ApplySurfaceParm( "fog", &brush->contents, NULL );
	}
	
	MsgDev( D_INFO, "%6i fogs\n", numMapFogs );
}