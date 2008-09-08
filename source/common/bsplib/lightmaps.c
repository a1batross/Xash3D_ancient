//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	lightmaps.c - lightmap manager
//=======================================================================

#include "bsplib.h"
#include "const.h"

int		numSortShaders;
drawsurf_t	*surfsOnShader[MAX_MAP_SHADERS];


int		allocated[LIGHTMAP_WIDTH];
int		numLightmaps = 1;
int		c_exactLightmap;


void PrepareNewLightmap( void )
{
	memset( allocated, 0, sizeof( allocated ) );
	numLightmaps++;
}

/*
===============
AllocLMBlock

returns a texture number and the position inside it
===============
*/
bool AllocLMBlock( int w, int h, int *x, int *y )
{
	int	i, j;
	int	best, best2;

	best = LIGHTMAP_HEIGHT;

	for( i = 0; i <= LIGHTMAP_WIDTH - w; i++ )
	{
		best2 = 0;

		for( j = 0; j < w; j++ )
		{
			if( allocated[i+j] >= best )
				break;
			if( allocated[i+j] > best2 )
				best2 = allocated[i+j];
		}
		if( j == w )
		{
			// this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if( best + h > LIGHTMAP_HEIGHT )
		return false;

	for( i = 0; i < w; i++ )
		allocated[*x+i] = best + h;
	return true;
}

/*
===================
AllocateLightmapForSurface
===================
*/
void AllocateLightmapForSurface( drawsurf_t *ds )
{
	dvertex_t		*verts;
	int		axis, i, w, h;
	int		x, y, ssize;
	vec3_t		vecs[2];
	float		d, s, t;
	vec3_t		origin;
	plane_t		*plane;
	vec3_t		planeNormal;
	int		entNum, brushNum;
	vec3_t		mins, maxs, size, exactSize, delta;

	ssize = LM_SAMPLE_SIZE;
	if( ds->shader->lightmap_size )
		ssize = ds->shader->lightmap_size;

	plane = &mapplanes[ds->side->planenum];

	// bound the surface
	ClearBounds( mins, maxs );
	verts = ds->verts;

	for( i = 0 ; i < ds->numverts; i++ )
		AddPointToBounds( verts[i].point, mins, maxs );

	// round to the lightmap resolution
	for( i = 0; i < 3; i++ )
	{
		exactSize[i] = maxs[i] - mins[i];
		mins[i] = ssize * floor( mins[i] / ssize );
		maxs[i] = ssize * ceil( maxs[i] / ssize );
		size[i] = (maxs[i] - mins[i]) / ssize + 1;
	}

	// the two largest axis will be the lightmap size
	memset( vecs, 0, sizeof( vecs ));

	planeNormal[0] = fabs( plane->normal[0] );
	planeNormal[1] = fabs( plane->normal[1] );
	planeNormal[2] = fabs( plane->normal[2] );

	if( planeNormal[0] >= planeNormal[1] && planeNormal[0] >= planeNormal[2] )
	{
		w = size[1];
		h = size[2];
		axis = 0;
		vecs[0][1] = 1.0 / ssize;
		vecs[1][2] = 1.0 / ssize;
	}
	else if( planeNormal[1] >= planeNormal[0] && planeNormal[1] >= planeNormal[2] )
	{
		w = size[0];
		h = size[2];
		axis = 1;
		vecs[0][0] = 1.0 / ssize;
		vecs[1][2] = 1.0 / ssize;
	}
	else
	{
		w = size[0];
		h = size[1];
		axis = 2;
		vecs[0][0] = 1.0 / ssize;
		vecs[1][1] = 1.0 / ssize;
	}

	if( !plane->normal[axis] ) Sys_Error( "Chose a 0 valued axis\n" );

	if( w > LIGHTMAP_WIDTH )
	{
		VectorScale( vecs[0], (float)LIGHTMAP_WIDTH/w, vecs[0] );
		w = LIGHTMAP_WIDTH;
	}
	
	if( h > LIGHTMAP_HEIGHT )
	{
		VectorScale( vecs[1], (float)LIGHTMAP_HEIGHT/h, vecs[1] );
		h = LIGHTMAP_HEIGHT;
	}
	
	c_exactLightmap += w * h;

	if( !AllocLMBlock( w, h, &x, &y ))
	{
		PrepareNewLightmap();
		if( !AllocLMBlock( w, h, &x, &y ))
		{
			entNum = ds->mapbrush->entitynum;
			brushNum = ds->mapbrush->brushnum;
			if( entNum == 0 ) Sys_Break( "Can't allocate lightmap for world brush %i\n", brushNum );
			else  Sys_Break( "Can't allocate lightmap for entity %i brush %i\n", entNum, brushNum );
		}
	}

	// set the lightmap texture coordinates in the drawVerts
	ds->lightmapNum = numLightmaps - 1;
	ds->lightmapWidth = w;
	ds->lightmapHeight = h;
	ds->lightmapX = x;
	ds->lightmapY = y;

	for( i = 0; i < ds->numverts; i++ )
	{
		VectorSubtract( verts[i].point, mins, delta );
		s = DotProduct( delta, vecs[0] ) + x + 0.5;
		t = DotProduct( delta, vecs[1] ) + y + 0.5;
		verts[i].lm[0] = s / LIGHTMAP_WIDTH;
		verts[i].lm[1] = t / LIGHTMAP_HEIGHT;
	}

	// calculate the world coordinates of the lightmap samples

	// project mins onto plane to get origin
	d = DotProduct( mins, plane->normal ) - plane->dist;
	d /= plane->normal[ axis ];
	VectorCopy( mins, origin );
	origin[axis] -= d;

	// project stepped lightmap blocks and subtract to get planevecs
	for( i = 0; i < 2; i++ )
	{
		vec3_t	normalized;
		float	len;

		VectorCopy( vecs[i], normalized );
		len = VectorNormalizeLength( normalized );
		VectorScale( normalized, ( 1.0 / len ), vecs[i] );
		d = DotProduct( vecs[i], plane->normal );
		d /= plane->normal[ axis ];
		vecs[i][axis] -= d;
	}

	VectorCopy( origin, ds->lightmapOrigin );
	VectorCopy( vecs[0], ds->lightmapVecs[0] );
	VectorCopy( vecs[1], ds->lightmapVecs[1] );
	VectorCopy( plane->normal, ds->lightmapVecs[2] );
}

/*
===================
AllocateLightmaps
===================
*/
void AllocateLightmaps( bsp_entity_t *e )
{
	int		i, j;
	drawsurf_t	*ds;
	shader_t		*si;

	MsgDev( D_INFO, "--- AllocateLightmaps ---\n" );

	// sort all surfaces by shader so common shaders will usually
	// be in the same lightmap
	numSortShaders = 0;

	for( i = e->firstsurf; i < numdrawsurfs; i++ )
	{
		ds = &drawsurfs[i];
		if( !ds->numverts ) continue;	// leftover from a surface subdivision

		// g-cont. hey this is extra information probably not needed
		VectorCopy( mapplanes[ds->side->planenum].normal, ds->lightmapVecs[2] );

		// search for this shader
		for( j = 0; j < numSortShaders; j++ )
		{
			if( ds->shader == surfsOnShader[j]->shader )
			{
				ds->next = surfsOnShader[j];
				surfsOnShader[j] = ds;
				break;
			}
		}
		if( j == numSortShaders )
		{
			if( numSortShaders >= MAX_MAP_SHADERS )
				Sys_Break( "MAX_MAP_SHADERS limit exceeded" );
			surfsOnShader[j] = ds;
			numSortShaders++;
		}
	}
	MsgDev( D_INFO, "%5i unique shaders\n", numSortShaders );

	// for each shader, allocate lightmaps for each surface

	for( i = 0; i < numSortShaders; i++ )
	{
		si = surfsOnShader[i]->shader;

		// g-cont. assert for me
		if( !si ) Sys_Error("drawsurf without any shader!\n" );

		for( ds = surfsOnShader[i]; ds; ds = ds->next )
		{
			// some surfaces don't need lightmaps allocated for them
			if( si->surfaceFlags & SURF_NOLIGHTMAP )
				ds->lightmapNum = -1;
			else AllocateLightmapForSurface( ds );
		}
	}

	MsgDev( D_INFO, "%7i exact lightmap texels\n", c_exactLightmap );
	MsgDev( D_INFO, "%7i block lightmap texels\n", numLightmaps * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT );
}



