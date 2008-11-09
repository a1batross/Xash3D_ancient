//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	lightmaps.c - lightmap manager
//=======================================================================

#include "bsplib.h"
#include "const.h"

#define MAX_STITCH_CANDIDATES		32
#define MAX_STITCH_LUXELS		64
#define SOLID_EPSILON		0.0625
#define LUXEL_TOLERANCE		0.0025
#define LUXEL_COLOR_FRAC		0.001302083	// 1 / 3 / 256

int		numLightSurfaces = 0;
int		*lightSurfaces = NULL;
int		numSurfaceClusters, maxSurfaceClusters;
int		*surfaceClusters;
int		*sortSurfaces = NULL;
int		superSample = 0;
int		numRawSuperLuxels = 0;
int		numRawLightmaps = 0;
rawLightmap_t	*rawLightmaps = NULL;
int		*sortLightmaps = NULL;
float		*vertexLuxels[LM_STYLES];
float		*radVertexLuxels[LM_STYLES];
int		numLightmapShaders = 0;
int		numSolidLightmaps = 0;
int		numOutLightmaps = 0;
int		numBSPLightmaps = 0;
int		numExtLightmaps = 0;
outLightmap_t	*outLightmaps = NULL;
int		numSurfsVertexLit = 0;
int		numSurfsVertexForced = 0;
int		numSurfsVertexApproximated = 0;
int		numSurfsLightmapped = 0;
int		numPlanarsLightmapped = 0;
int		numNonPlanarsLightmapped = 0;
int		numPatchesLightmapped = 0;
int		numPlanarPatchesLightmapped = 0;
int		numLuxels = 0;
int		numLuxelsMapped = 0;
int		numLuxelsOccluded = 0;
int		numLuxelsIlluminated = 0;
int		numVertsIlluminated = 0;
float		max_map_distance = 0.0f;
dvertex_t		*yDrawVerts;
vec3_t		minLight;
vec3_t		minVertexLight;
vec3_t		minGridLight;
vec3_t		ambientColor = { 0, 0, 0 };
int		num_lightmaps = 0;

// user light settings
int		bounce = 0;
bool		bounceOnly = false;
bool		bouncing = false;
bool		bouncegrid = false;
bool		deluxemap = false;
bool		debugDeluxemap = false;
bool		fast = false;
bool		faster = false;
bool		fastgrid = false;
bool		fastbounce = false;
bool		cheap = false;
bool		cheapgrid = false;
bool		debug;
bool		noCollapse = false;
int		approximateTolerance = 0;

/* 
===============================================================================

this file contains code that doe lightmap allocation and projection that
runs in the -light phase.

this is handled here rather than in the bsp phase for a few reasons--
surfaces are no longer necessarily convex polygons, patches may or may not be
planar or have lightmaps projected directly onto control points.

also, this allows lightmaps to be calculated before being allocated and stored
in the bsp. lightmaps that have little high-frequency information are candidates
for having their resolutions scaled down.

===============================================================================
*/

size_t WriteLightmap( int number, int width, int height, byte *buffer )
{
	string		filename;
	rgbdata_t 	*r_lightmap;
	size_t		lightmap_size;

	// set output format
	r_lightmap = BSP_Malloc( sizeof( rgbdata_t ));
	r_lightmap->width = width;
	r_lightmap->height = height;
	r_lightmap->type = PF_RGB_24;
	r_lightmap->size = r_lightmap->width * r_lightmap->height * 3;
	r_lightmap->palette = NULL;
	r_lightmap->numLayers = 1;
	r_lightmap->numMips = 1;
	r_lightmap->buffer = buffer;
	lightmap_size = r_lightmap->size;

	com.sprintf( filename, "gfx/lightmaps/%s_%04d.dds", gs_filename, number );
	FS_SaveImage( filename, r_lightmap );
	BSP_Free( r_lightmap );

	return lightmap_size; 
}

/*
=================
WriteLightmaps

exports the lightmaps as a list of numbered tga images
=================
*/
void WriteLightmaps( void )
{
	int		i;
	byte		*lightdata;

	
	MsgDev( D_NOTE, "--- ExportLightmaps ---\n");
	
	// sanity check
	if( lightbytes == NULL )
	{
		MsgDev( D_WARN, "No BSP lightmap data\n" );
		return;
	}
	
	// iterate through the lightmaps
	for( i = 0, lightdata = lightbytes; lightdata < (lightbytes + num_lightbytes); i++ )
	{
		lightdata += WriteLightmap( i, bsp_lightmap_size->integer, bsp_lightmap_size->integer, lightdata );
	}
}

/*
===============================================================================

this section deals with projecting a lightmap onto a raw drawsurface

===============================================================================
*/
/*
=================
CompareLightSurface

compare function for qsort
=================
*/
static int CompareLightSurface( const void *a, const void *b )
{
	bsp_shader_t	*asi, *bsi;
	
	asi = surfaceInfos[*((int*)a)].si;
	bsi = surfaceInfos[*((int*)b)].si;
	
	if( asi == NULL ) return -1;
	if( bsi == NULL ) return 1;
	
	return com.strcmp( asi->name, bsi->name );
}

/*
=================
FinishRawLightmap

allocates a raw lightmap's necessary buffers
=================
*/
void FinishRawLightmap( rawLightmap_t *lm )
{
	int		i, j, c, size, *sc;
	float		is;
	surfaceInfo_t	*info;
	
	
	// sort light surfaces by shader name
	qsort( &lightSurfaces[lm->firstLightSurface], lm->numLightSurfaces, sizeof( int ), CompareLightSurface );
	
	lm->numLightClusters = 0;
	for( i = 0; i < lm->numLightSurfaces; i++ )
	{
		info = &surfaceInfos[lightSurfaces[lm->firstLightSurface+i]];
		lm->numLightClusters += info->numSurfaceClusters;
	}
	
	lm->lightClusters = BSP_Malloc( lm->numLightClusters * sizeof( *lm->lightClusters ) );
	c = 0;
	for( i = 0; i < lm->numLightSurfaces; i++ )
	{
		info = &surfaceInfos[lightSurfaces[lm->firstLightSurface+i]];
		
		for( j = 0; j < info->numSurfaceClusters; j++ )
			lm->lightClusters[c++] = surfaceClusters[info->firstSurfaceCluster+j];
	}
	
	lm->styles[0] = LS_NORMAL;
	for( i = 1; i < LM_STYLES; i++ )
		lm->styles[i] = LS_NONE;
	
	// set supersampling size
	lm->sw = lm->w * superSample;
	lm->sh = lm->h * superSample;
	
	// add to super luxel count
	numRawSuperLuxels += (lm->sw * lm->sh);
	
	// manipulate origin/vecs for supersampling
	if( superSample > 1 && lm->vecs != NULL )
	{
		is = 1.0f / superSample;
		
		// scale the vectors and shift the origin
#if 1
		// new code that works for arbitrary supersampling values
		VectorMA( lm->origin, -0.5, lm->vecs[0], lm->origin );
		VectorMA( lm->origin, -0.5, lm->vecs[1], lm->origin );
		VectorScale( lm->vecs[0], is, lm->vecs[0] );
		VectorScale( lm->vecs[1], is, lm->vecs[1] );
		VectorMA( lm->origin, is, lm->vecs[0], lm->origin );
		VectorMA( lm->origin, is, lm->vecs[1], lm->origin );
#else
		// old code that only worked with a value of 2
		VectorScale( lm->vecs[0], is, lm->vecs[0] );
		VectorScale( lm->vecs[1], is, lm->vecs[1] );
		VectorMA( lm->origin, -is, lm->vecs[0], lm->origin );
		VectorMA( lm->origin, -is, lm->vecs[1], lm->origin );
#endif
	}
	
	// allocate bsp lightmap storage
	size = lm->w * lm->h * BSP_LUXEL_SIZE * sizeof( float );
	if( lm->bspLuxels[0] == NULL )
		lm->bspLuxels[0] = BSP_Malloc( size );
	else memset( lm->bspLuxels[0], 0, size );
	
	// allocate radiosity lightmap storage
	if( bounce )
	{
		size = lm->w * lm->h * RAD_LUXEL_SIZE * sizeof( float );
		if( lm->radLuxels[0] == NULL )
			lm->radLuxels[0] = BSP_Malloc( size );
		else memset( lm->radLuxels[0], 0, size );
	}
	
	// allocate sampling lightmap storage
	size = lm->sw * lm->sh * SUPER_LUXEL_SIZE * sizeof( float );
	if( lm->superLuxels[0] == NULL )
		lm->superLuxels[0] = BSP_Malloc( size );
	else memset( lm->superLuxels[0], 0, size );
	
	// allocate origin map storage
	size = lm->sw * lm->sh * SUPER_ORIGIN_SIZE * sizeof( float );
	if( lm->superOrigins == NULL )
		lm->superOrigins = BSP_Malloc( size );
	else memset( lm->superOrigins, 0, size );
	
	// allocate normal map storage
	size = lm->sw * lm->sh * SUPER_NORMAL_SIZE * sizeof( float );
	if( lm->superNormals == NULL )
		lm->superNormals = BSP_Malloc( size );
	else memset( lm->superNormals, 0, size );
	
	// allocate cluster map storage
	size = lm->sw * lm->sh * sizeof( int );
	if( lm->superClusters == NULL )
		lm->superClusters = BSP_Malloc( size );
	size = lm->sw * lm->sh;
	sc = lm->superClusters;
	for( i = 0; i < size; i++ )
		(*sc++) = CLUSTER_UNMAPPED;
	
	// deluxemap allocation
	if( deluxemap )
	{
		// allocate sampling deluxel storage
		size = lm->sw * lm->sh * SUPER_DELUXEL_SIZE * sizeof( float );
		if( lm->superDeluxels == NULL )
			lm->superDeluxels = BSP_Malloc( size );
		else memset( lm->superDeluxels, 0, size );
		
		// allocate bsp deluxel storage
		size = lm->w * lm->h * BSP_DELUXEL_SIZE * sizeof( float );
		if( lm->bspDeluxels == NULL )
			lm->bspDeluxels = BSP_Malloc( size );
		else memset( lm->bspDeluxels, 0, size );
	}
	numLuxels += (lm->sw * lm->sh);
}



/*
=================
AddPatchToRawLightmap

projects a lightmap for a patch surface
since lightmap calculation for surfaces is now handled in a general way (light.c),
it is no longer necessary for patch verts to fall exactly on a lightmap sample
based on AllocateLightmapForPatch()
=================
*/
bool AddPatchToRawLightmap( int num, rawLightmap_t *lm )
{
	dsurface_t	*ds;
	surfaceInfo_t	*info;
	int		x, y;
	dvertex_t		*verts, *a, *b;
	vec3_t		delta;
	bsp_mesh_t	src, *subdivided, *mesh;
	float		sBasis, tBasis, s, t;
	float		length, widthTable[MAX_EXPANDED_AXIS], heightTable[MAX_EXPANDED_AXIS];
	
	// patches finish a raw lightmap
	lm->finished = true;
	
	ds = &dsurfaces[num];
	info = &surfaceInfos[num];
	
	// make a temporary mesh from the drawsurf 
	src.width = ds->patch.width;
	src.height = ds->patch.height;
	src.verts = &yDrawVerts[ds->firstvertex];
	subdivided = SubdivideMesh2( src, info->patchIterations );
	
	// fit it to the curve and remove colinear verts on rows/columns
	PutMeshOnCurve( *subdivided );
	mesh = RemoveLinearMeshColumnsRows( subdivided );
	FreeMesh( subdivided );
	
	// find the longest distance on each row/column
	verts = mesh->verts;
	memset( widthTable, 0, sizeof( widthTable ));
	memset( heightTable, 0, sizeof( heightTable ));

	for( y = 0; y < mesh->height; y++ )
	{
		for( x = 0; x < mesh->width; x++ )
		{
			if( x + 1 < mesh->width )
			{
				a = &verts[(y * mesh->width)+x+0];
				b = &verts[(y * mesh->width)+x+1];
				VectorSubtract( a->point, b->point, delta );
				length = VectorLength( delta );
				if( length > widthTable[x] )
					widthTable[x] = length;
			}
			
			if( y + 1 < mesh->height )
			{
				a = &verts[(y * mesh->width)+x];
				b = &verts[((y + 1) * mesh->width)+x];
				VectorSubtract( a->point, b->point, delta );
				length = VectorLength( delta );
				if( length > heightTable[y] )
					heightTable[y] = length;
			}
		}
	}
	
	// determine lightmap width
	length = 0;
	for( x = 0; x < (mesh->width - 1); x++ )
		length += widthTable[x];
	lm->w = ceil( length / lm->sampleSize ) + 1;
	if( lm->w < ds->patch.width )
		lm->w = ds->patch.width;
	if( lm->w > lm->customWidth )
		lm->w = lm->customWidth;
	sBasis = (float) (lm->w - 1) / (float) (ds->patch.width - 1);
	
	// determine lightmap height
	length = 0;
	for( y = 0; y < (mesh->height - 1); y++ )
		length += heightTable[ y ];
	lm->h = ceil( length / lm->sampleSize ) + 1;
	if( lm->h < ds->patch.height )
		lm->h = ds->patch.height;
	if( lm->h > lm->customHeight )
		lm->h = lm->customHeight;
	tBasis = (float) (lm->h - 1) / (float) (ds->patch.height - 1);
	
	// free the temporary mesh
	FreeMesh( mesh );
	
	// set the lightmap texture coordinates in yDrawVerts
	lm->wrap[0] = true;
	lm->wrap[1] = true;
	verts = &yDrawVerts[ds->firstvertex];
	for( y = 0; y < ds->patch.height; y++ )
	{
		t = (tBasis * y) + 0.5f;
		for( x = 0; x < ds->patch.width; x++ )
		{
			s = (sBasis * x) + 0.5f;
			verts[(y * ds->patch.width)+x].lm[0][0] = s * superSample;
			verts[(y * ds->patch.width)+x].lm[0][1] = t * superSample;
			
			if( y == 0 && !VectorCompare( verts[x].point, verts[((ds->patch.height - 1) * ds->patch.width) + x].point ))
				lm->wrap[1] = false;
		}
		
		if( !VectorCompare( verts[(y * ds->patch.width)].point, verts[(y * ds->patch.width) + (ds->patch.width - 1)].point ))
			lm->wrap[0] = false;
	}
	numPatchesLightmapped++;
	
	return true;
}



/*
=================
AddSurfaceToRawLightmap

projects a lightmap for a surface
based on AllocateLightmapForSurface
=================
*/
bool AddSurfaceToRawLightmap( int num, rawLightmap_t *lm )
{
	dsurface_t	*ds, *ds2;
	surfaceInfo_t	*info, *info2;
	int		num2, n, i, axisNum;
	float		s, t, d, len, sampleSize;
	vec3_t		mins, maxs, origin, faxis, size, exactSize, delta, normalized, vecs[2];
	vec4_t		plane;
	dvertex_t		*verts;
	
	ds = &dsurfaces[num];
	info = &surfaceInfos[num];
	
	lightSurfaces[numLightSurfaces++] = num;
	lm->numLightSurfaces++;
	
	// does this raw lightmap already have any surfaces?
	if( lm->numLightSurfaces > 1 )
	{
		// surface and raw lightmap must have the same lightmap projection axis
		if( !VectorCompare( info->axis, lm->axis ))
			return false;
		
		if( info->sampleSize != lm->sampleSize || info->entityNum != lm->entityNum || info->recvShadows != lm->recvShadows ||
		info->si->lmCustomWidth != lm->customWidth || info->si->lmCustomHeight != lm->customHeight ||
		info->si->lmBrightness != lm->brightness || info->si->lmFilterRadius != lm->filterRadius || info->si->splotchFix != lm->splotchFix )
			return false;
		
		// surface bounds must intersect with raw lightmap bounds
		for( i = 0; i < 3; i++ )
		{
			if( info->mins[i] > lm->maxs[i] ) return false;
			if( info->maxs[i] < lm->mins[i] ) return false;
		}
		
		// plane check (FIXME: allow merging of nonplanars)
		if( info->si->lmMergable == false )
		{
			if( info->plane == NULL || lm->plane == NULL )
				return false;
			
			for( i = 0; i < 4; i++ )
				if( fabs( info->plane[i] - lm->plane[i] ) > EQUAL_EPSILON )
					return false;
		}
	}
	
	// set plane
	if( info->plane == NULL ) lm->plane = NULL;
	
	// add surface to lightmap bounds
	AddPointToBounds( info->mins, lm->mins, lm->maxs );
	AddPointToBounds( info->maxs, lm->mins, lm->maxs );
	
	// check to see if this is a non-planar patch
	if( ds->surfaceType == MST_PATCH && lm->axis[0] == 0.0f && lm->axis[1] == 0.0f && lm->axis[2] == 0.0f )
		return AddPatchToRawLightmap( num, lm );
	
	// start with initially requested sample size
	sampleSize = lm->sampleSize;
	
	// round to the lightmap resolution
	for( i = 0; i < 3; i++ )
	{
		exactSize[i] = lm->maxs[i] - lm->mins[i];
 		mins[i] = sampleSize * floor( lm->mins[i] / sampleSize );
 		maxs[i] = sampleSize * ceil( lm->maxs[i] / sampleSize );
 		size[i] = (maxs[i] - mins[i]) / sampleSize + 1.0f;
		
		// hack (god this sucks)
		if( size[i] > lm->customWidth || size[i] > lm->customHeight )
		{
			i = -1;
			sampleSize += 1.0f;
		}
	}
	
	// set actual sample size
	lm->actualSampleSize = sampleSize;
	
	// FIXME: copy rounded mins/maxes to lightmap record?
	if( lm->plane == NULL )
	{
		VectorCopy( mins, lm->mins );
		VectorCopy( maxs, lm->maxs );
		VectorCopy( mins, origin );
	}
	
	VectorCopy( lm->mins, origin );
	
	faxis[0] = fabs( lm->axis[0] );
	faxis[1] = fabs( lm->axis[1] );
	faxis[2] = fabs( lm->axis[2] );
	Mem_Set( vecs, 0, sizeof( vecs ));
	
	// classify the plane (x y or z major) (ydnar: biased to z axis projection)
	if( faxis[2] >= faxis[0] && faxis[2] >= faxis[1] )
	{
		axisNum = 2;
		lm->w = size[0];
		lm->h = size[1];
		vecs[0][0] = 1.0f / sampleSize;
		vecs[1][1] = 1.0f / sampleSize;
	}
	else if( faxis[0] >= faxis[1] && faxis[0] >= faxis[2] )
	{
		axisNum = 0;
		lm->w = size[1];
		lm->h = size[2];
		vecs[0][1] = 1.0f / sampleSize;
		vecs[1][2] = 1.0f / sampleSize;
	}
	else
	{
		axisNum = 1;
		lm->w = size[0];
		lm->h = size[2];
		vecs[0][0] = 1.0f / sampleSize;
		vecs[1][2] = 1.0f / sampleSize;
	}
	
	if( faxis[axisNum] == 0.0f )
	{
		MsgDev( D_WARN, "ProjectSurfaceLightmap: Choose a 0 valued axis\n" );
		lm->w = lm->h = 0;
		return false;
	}
	
	lm->axisNum = axisNum;
	
	for( n = 0; n < lm->numLightSurfaces; n++ )
	{
		num2 = lightSurfaces[lm->firstLightSurface+n];
		ds2 = &dsurfaces[num2];
		info2 = &surfaceInfos[num2];
		verts = &yDrawVerts[ds2->firstvertex];
		
		// set the lightmap texture coordinates in yDrawVerts in [0, superSample * lm->customWidth] space
		for( i = 0; i < ds2->numvertices; i++ )
		{
			VectorSubtract( verts[i].point, origin, delta );
			s = DotProduct( delta, vecs[0] ) + 0.5f;
			t = DotProduct( delta, vecs[1] ) + 0.5f;
			verts[i].lm[0][0] = s * superSample;
			verts[i].lm[0][1] = t * superSample;
			
			if( s > (float) lm->w || t > (float) lm->h )
				MsgDev( D_WARN, "Lightmap texture coords out of range: S %1.4f > %3d || T %1.4f > %3d\n", s, lm->w, t, lm->h );
		}
	}
	
	// get first drawsurface
	num2 = lightSurfaces[lm->firstLightSurface];
	ds2 = &dsurfaces[num2];
	info2 = &surfaceInfos[num2];
	verts = &yDrawVerts[ds2->firstvertex];
	
	// calculate lightmap origin
	if( VectorLength( ds2->flat.normal ))
		VectorCopy( ds2->flat.normal, plane );
	else VectorCopy( lm->axis, plane );
	plane[3] = DotProduct( verts[0].point, plane );
	
	VectorCopy( origin, lm->origin );
	d = DotProduct( lm->origin, plane ) - plane[3];
	d /= plane[axisNum];
	lm->origin[axisNum] -= d;
	
	// legacy support
	VectorCopy( lm->origin, ds->flat.lmapOrigin );
	
	// for planar surfaces, create lightmap vectors for st->point conversion
	if( VectorLength( ds->flat.normal ) || 1 )
	{
		// ydnar: can't remember what exactly i was thinking here...
		// g-cont. You bunny wrote!!! what a hell ???

		lm->vecs = BSP_Malloc( 3 * sizeof( vec3_t ));
		VectorCopy( ds->flat.normal, lm->vecs[2] );
		
		// project stepped lightmap blocks and subtract to get planevecs
		for( i = 0; i < 2; i++ )
		{
			VectorCopy( vecs[i], normalized );
			len = VectorNormalizeLength( normalized );
			VectorScale( normalized, (1.0 / len), lm->vecs[i] );
			d = DotProduct( lm->vecs[i], plane );
			d /= plane[axisNum];
			lm->vecs[i][axisNum] -= d;
		}
	}
	else lm->vecs = NULL;
	
	if( ds->surfaceType == MST_PATCH )
	{
		numPatchesLightmapped++;
		if( lm->plane != NULL )
			numPlanarPatchesLightmapped++;
	}
	else
	{
		if( lm->plane != NULL )
			numPlanarsLightmapped++;
		else numNonPlanarsLightmapped++;
	}
	return true;
}



/*
=================
CompareSurfaceInfo

compare function for qsort
=================
*/
static int CompareSurfaceInfo( const void *a, const void *b )
{
	surfaceInfo_t	*aInfo, *bInfo;
	int		i;

	aInfo = &surfaceInfos[*((int*)a)];
	bInfo = &surfaceInfos[*((int*)b)];
	
	if( aInfo->model < bInfo->model ) return 1;
	else if( aInfo->model > bInfo->model ) return -1;
	
	if( aInfo->hasLightmap < bInfo->hasLightmap ) return 1;
	else if( aInfo->hasLightmap > bInfo->hasLightmap ) return -1;
	
	if( aInfo->sampleSize < bInfo->sampleSize ) return 1;
	else if( aInfo->sampleSize > bInfo->sampleSize ) return -1;
	
	for( i = 0; i < 3; i++ )
	{
		if( aInfo->axis[i] < bInfo->axis[i] ) return 1;
		else if( aInfo->axis[i] > bInfo->axis[i] ) return -1;
	}
	
	if( aInfo->plane == NULL && bInfo->plane != NULL ) return 1;
	else if( aInfo->plane != NULL && bInfo->plane == NULL ) return -1;
	else if( aInfo->plane != NULL && bInfo->plane != NULL )
	{
		for( i = 0; i < 4; i++ )
		{
			if( aInfo->plane[i] < bInfo->plane[i] ) return 1;
			else if( aInfo->plane[i] > bInfo->plane[i] ) return -1;
		}
	}
	
	for( i = 0; i < 3; i++ )
	{
		if( aInfo->mins[i] < bInfo->mins[i] ) return 1;
		else if( aInfo->mins[i] > bInfo->mins[i] ) return -1;
	}
	
	// these are functionally identical (this should almost never happen)
	return 0;
}



/*
=================
SetupSurfaceLightmaps

allocates lightmaps for every surface in the bsp that needs one
this depends on yDrawVerts being allocated
=================
*/
void SetupSurfaceLightmaps( void )
{
	int		i, j, k, s,num, num2;
	dmodel_t		*model;
	dleaf_t		*leaf;
	dsurface_t	*ds, *ds2;
	surfaceInfo_t	*info, *info2;
	rawLightmap_t	*lm;
	bool		added;
	vec3_t		entityOrigin;
	
	MsgDev( D_NOTE, "--- SetupSurfaceLightmaps ---\n");
	
	if( superSample < 1 ) superSample = 1;
	else if( superSample > 8 )
	{
		MsgDev( D_WARN, "Insane supersampling amount (%d) detected.\n", superSample );
		superSample = 8;
	}
	
	ClearBounds( map_mins, map_maxs );
	
	numSurfaceClusters = 0;
	maxSurfaceClusters = numleaffaces;
	surfaceClusters = BSP_Malloc( maxSurfaceClusters * sizeof( *surfaceClusters ));
	
	// allocate a list for per-surface info
	surfaceInfos = BSP_Malloc( numsurfaces * sizeof( *surfaceInfos ));
	for( i = 0; i < numsurfaces; i++ )
		surfaceInfos[i].childSurfaceNum = -1;
	
	// allocate a list of surface indexes to be sorted
	sortSurfaces = BSP_Malloc( numsurfaces * sizeof( int ));
	
	for( i = 0; i < nummodels; i++ )
	{
		model = &dmodels[i];
		
		for( j = 0; j < model->numfaces; j++ )
		{
			num = model->firstface + j;
			
			sortSurfaces[num] = num;
			ds = &dsurfaces[num];
			info = &surfaceInfos[num];
			
			if( ds->numvertices > 0 )
				VectorSubtract( yDrawVerts[ds->firstvertex].point, dvertexes[ds->firstvertex].point, entityOrigin );
			else VectorClear( entityOrigin );
			
			info->model = model;
			info->lm = NULL;
			info->plane = NULL;
			info->firstSurfaceCluster = numSurfaceClusters;
			
			info->si = GetSurfaceExtraShader( num );
			if( info->si == NULL ) info->si = FindShader( dshaders[ds->shadernum].name );
			info->parentSurfaceNum = GetSurfaceExtraParentSurfaceNum( num );
			info->entityNum = GetSurfaceExtraEntityNum( num );
			info->castShadows = GetSurfaceExtraCastShadows( num );
			info->recvShadows = GetSurfaceExtraRecvShadows( num );
			info->sampleSize = GetSurfaceExtraSampleSize( num );
			info->longestCurve = GetSurfaceExtraLongestCurve( num );
			info->patchIterations = IterationsForCurve( info->longestCurve, patchSubdivisions );
			GetSurfaceExtraLightmapAxis( num, info->axis );
			
			if( info->parentSurfaceNum >= 0 )
				surfaceInfos[info->parentSurfaceNum].childSurfaceNum = j;
			
			ClearBounds( info->mins, info->maxs );
			for( k = 0; k < ds->numvertices; k++ )
			{
				AddPointToBounds( yDrawVerts[ds->firstvertex+k].point, map_mins, map_maxs );
				AddPointToBounds( yDrawVerts[ds->firstvertex+k].point, info->mins, info->maxs );
			}
			
			// find all the bsp clusters the surface falls into
			for( k = 0; k < numleafs; k++ )
			{
				leaf = &dleafs[ k ];
				
				if( leaf->mins[0] > info->maxs[0] || leaf->maxs[0] < info->mins[0] ||
					leaf->mins[1] > info->maxs[1] || leaf->maxs[1] < info->mins[1] ||
					leaf->mins[2] > info->maxs[2] || leaf->maxs[2] < info->mins[2] )
					continue;
				
				for( s = 0; s < leaf->numleafsurfaces; s++ )
				{
					if( dleaffaces[leaf->firstleafsurface+s] == num )
					{
						if( numSurfaceClusters >= maxSurfaceClusters )
							Sys_Error( "maxSurfaceClusters exceeded\n" );
						surfaceClusters[ numSurfaceClusters ] = leaf->cluster;
						numSurfaceClusters++;
						info->numSurfaceClusters++;
					}
				}
			}
			
			if( VectorLength( ds->flat.normal ) > 0.0f )
			{
				info->plane = BSP_Malloc( 4 * sizeof( float ) );
				VectorCopy( ds->flat.normal, info->plane );
				info->plane[3] = DotProduct( yDrawVerts[ds->firstvertex].point, info->plane );
			}
			
			// determine if surface requires a lightmap
			if( ds->surfaceType == MST_TRIANGLE_SOUP || ds->surfaceType == MST_FOLIAGE || (info->si->surfaceFlags & SURF_VERTEXLIT))
				numSurfsVertexLit++;
			else
			{
				numSurfsLightmapped++;
				info->hasLightmap = true;
			}
		}
	}
	
	VectorSubtract( map_maxs, map_mins, map_size );
	max_map_distance = VectorLength( map_size );
	
	qsort( sortSurfaces, numsurfaces, sizeof( int ), CompareSurfaceInfo );
	
	numLightSurfaces = 0;
	lightSurfaces = BSP_Malloc( numSurfsLightmapped * sizeof( int ));
	
	numRawSuperLuxels = 0;
	numRawLightmaps = 0;
	rawLightmaps = BSP_Malloc( numSurfsLightmapped * sizeof( *rawLightmaps ));
	
	for( i = 0; i < numsurfaces; i++ )
	{
		num = sortSurfaces[i];
		ds = &dsurfaces[num];
		info = &surfaceInfos[num];
		if( info->hasLightmap == false || info->lm != NULL || info->parentSurfaceNum >= 0 )
			continue;
		
		lm = &rawLightmaps[numRawLightmaps];
		numRawLightmaps++;
		
		lm->splotchFix = info->si->splotchFix;
		lm->firstLightSurface = numLightSurfaces;
		lm->numLightSurfaces = 0;
		lm->sampleSize = info->sampleSize;
		lm->actualSampleSize = info->sampleSize;
		lm->entityNum = info->entityNum;
		lm->recvShadows = info->recvShadows;
		lm->brightness = info->si->lmBrightness;
		lm->filterRadius = info->si->lmFilterRadius;
		VectorCopy( info->axis, lm->axis );
		lm->plane = info->plane;	
		VectorCopy( info->mins, lm->mins );
		VectorCopy( info->maxs, lm->maxs );
		
		lm->customWidth = info->si->lmCustomWidth;
		lm->customHeight = info->si->lmCustomHeight;
		
		AddSurfaceToRawLightmap( num, lm );
		info->lm = lm;
		
		added = true;
		while( added )
		{
			/* walk the list of surfaces again */
			added = false;
			for( j = i + 1; j < numsurfaces && lm->finished == false; j++ )
			{
				num2 = sortSurfaces[j];
				ds2 = &dsurfaces[num2];
				info2 = &surfaceInfos[num2];
				if( info2->hasLightmap == false || info2->lm != NULL )
					continue;
				
				if( AddSurfaceToRawLightmap( num2, lm ))
				{
					info2->lm = lm;
					added = true;
				}
				else
				{
					lm->numLightSurfaces--;
					numLightSurfaces--;
				}
			}
		}
		
		// finish the lightmap and allocate the various buffers
		FinishRawLightmap( lm );
	}
	
	// allocate vertex luxel storage
	for( k = 0; k < LM_STYLES; k++ )
	{
		vertexLuxels[k] = BSP_Malloc( numvertexes * VERTEX_LUXEL_SIZE * sizeof( float )); 
		radVertexLuxels[k] = BSP_Malloc( numvertexes * VERTEX_LUXEL_SIZE * sizeof( float ));
	}
	
	// emit some stats
	MsgDev( D_NOTE, "%6i surfaces\n", numsurfaces );
	MsgDev( D_NOTE, "%6i raw lightmaps\n", numRawLightmaps );
	MsgDev( D_NOTE, "%6i surfaces vertex lit\n", numSurfsVertexLit );
	MsgDev( D_NOTE, "%6i surfaces lightmapped\n", numSurfsLightmapped );
	MsgDev( D_NOTE, "%6i planar surfaces lightmapped\n", numPlanarsLightmapped );
	MsgDev( D_NOTE, "%6i non-planar surfaces lightmapped\n", numNonPlanarsLightmapped );
	MsgDev( D_NOTE, "%6i patches lightmapped\n", numPatchesLightmapped );
	MsgDev( D_NOTE, "%6i planar patches lightmapped\n", numPlanarPatchesLightmapped );
}



/*
=================
StitchSurfaceLightmaps

stitches lightmap edges
2002-11-20 update: use this func only for stitching nonplanar patch lightmap seams
=================
*/
void StitchSurfaceLightmaps( void )
{
	int		i, j, x, y, x2, y2, *cluster, *cluster2;
	int		numStitched, numCandidates, numLuxels;
	rawLightmap_t	*lm, *a, *b, *c[MAX_STITCH_CANDIDATES];
	float		*luxel, *luxel2, *origin, *origin2, *normal, *normal2; 
	float		sampleSize, average[3], totalColor, ootc, *luxels[MAX_STITCH_LUXELS];
	
	
	// disabled for now
	// g-cont. why ???
	return;
	
	MsgDev( D_NOTE, "--- StitchSurfaceLightmaps ---\n");

	numStitched = 0;
	for( i = 0; i < numRawLightmaps; i++ )
	{
		a = &rawLightmaps[i];
		
		numCandidates = 0;
		for( j = i + 1; j < numRawLightmaps && numCandidates < MAX_STITCH_CANDIDATES; j++ )
		{
			b = &rawLightmaps[j];
			
			if( a->mins[0] > b->maxs[0] || a->maxs[0] < b->mins[0] ||
			a->mins[1] > b->maxs[1] || a->maxs[1] < b->mins[1] ||
			a->mins[2] > b->maxs[2] || a->maxs[2] < b->mins[2] )
				continue;
			
			c[numCandidates++] = b;
		}
		
		for( y = 0; y < a->sh; y++ )
		{
			for( x = 0; x < a->sw; x++ )
			{
				lm = a;
				cluster = SUPER_CLUSTER( x, y );
				if( *cluster == CLUSTER_UNMAPPED )
					continue;
				luxel = SUPER_LUXEL( 0, x, y );
				if( luxel[3] <= 0.0f ) continue;
				
				origin = SUPER_ORIGIN( x, y );
				normal = SUPER_NORMAL( x, y );
				
				for( j = 0; j < numCandidates; j++ )
				{
					b = c[j];
					lm = b;
					
					// set samplesize to the smaller of the pair
					sampleSize = 0.5f * (a->actualSampleSize < b->actualSampleSize ? a->actualSampleSize : b->actualSampleSize);
					
					if( origin[0] < (b->mins[0] - sampleSize) || (origin[0] > b->maxs[0] + sampleSize) ||
					origin[1] < (b->mins[1] - sampleSize) || (origin[1] > b->maxs[1] + sampleSize) ||
					origin[2] < (b->mins[2] - sampleSize) || (origin[2] > b->maxs[2] + sampleSize) )
						continue;
					
					VectorClear( average );
					numLuxels = 0;
					totalColor = 0.0f;
					for( y2 = 0; y2 < b->sh && numLuxels < MAX_STITCH_LUXELS; y2++ )
					{
						for( x2 = 0; x2 < b->sw && numLuxels < MAX_STITCH_LUXELS; x2++ )
						{
							if( a == b && abs( x - x2 ) <= 1 && abs( y - y2 ) <= 1 )
								continue;
							
							cluster2 = SUPER_CLUSTER( x2, y2 );
							if( *cluster2 == CLUSTER_UNMAPPED )
								continue;
							luxel2 = SUPER_LUXEL( 0, x2, y2 );
							if( luxel2[3] <= 0.0f )
								continue;
							
							origin2 = SUPER_ORIGIN( x2, y2 );
							normal2 = SUPER_NORMAL( x2, y2 );
							
							if( DotProduct( normal, normal2 ) < 0.5f )
								continue;
							
							if( fabs( origin[0] - origin2[0] ) > sampleSize || fabs( origin[1] - origin2[1] ) > sampleSize || fabs( origin[2] - origin2[2] ) > sampleSize )
								continue;
							
							luxels[numLuxels++] = luxel2;
							VectorAdd( average, luxel2, average );
							totalColor += luxel2[3];
						}
					}
					
					if( numLuxels == 0 ) continue;
					
					ootc = 1.0f / totalColor;
					VectorScale( average, ootc, luxel );
					luxel[3] = 1.0f;
					numStitched++;
				}
			}
		}
	}
	MsgDev( D_NOTE, "%6i luxels stitched\n", numStitched );
}



/*
=================
CompareBSPLuxels

compares two surface lightmaps' bsp luxels, ignoring occluded luxels
=================
*/
static bool CompareBSPLuxels( rawLightmap_t *a, int aNum, rawLightmap_t *b, int bNum )
{
	rawLightmap_t	*lm;
	int		x, y;
	double		delta, total, rd, gd, bd;
	float		*aLuxel, *bLuxel;
	
	// styled lightmaps will never be collapsed to non-styled lightmaps when there is _minlight
	if( (minLight[0] || minLight[1] || minLight[2]) && ((aNum == 0 && bNum != 0) || (aNum != 0 && bNum == 0)))
		return false;
	
	if( a->customWidth != b->customWidth || a->customHeight != b->customHeight ||
	a->brightness != b->brightness || a->solid[aNum] != b->solid[bNum] ||
	a->bspLuxels[aNum] == NULL || b->bspLuxels[bNum] == NULL )
		return false;
	
	if( a->solid[aNum] && b->solid[bNum] )
	{
		rd = fabs( a->solidColor[aNum][0] - b->solidColor[bNum][0] );
		gd = fabs( a->solidColor[aNum][1] - b->solidColor[bNum][1] );
		bd = fabs( a->solidColor[aNum][2] - b->solidColor[bNum][2] );
		
		if( rd > SOLID_EPSILON || gd > SOLID_EPSILON|| bd > SOLID_EPSILON )
			return false;
		return true;
	}
	
	if( a->w != b->w || a->h != b->h )
		return false;
	
	delta = 0.0;
	total = 0.0;
	for( y = 0; y < a->h; y++ )
	{
		for( x = 0; x < a->w; x++ )
		{
			total += 1.0;
			
			lm = a;	aLuxel = BSP_LUXEL( aNum, x, y );
			lm = b;	bLuxel = BSP_LUXEL( bNum, x, y );
			
			if( aLuxel[0] < 0 || bLuxel[0] < 0 )
				continue;
			
			rd = fabs( aLuxel[0] - bLuxel[0] );
			gd = fabs( aLuxel[1] - bLuxel[1] );
			bd = fabs( aLuxel[2] - bLuxel[2] );
			
			// compare individual luxels
			if( rd > 3.0 || gd > 3.0 || bd > 3.0 )
				return false;
			
			/* compare (fixme: take into account perceptual differences) */
			delta += rd * LUXEL_COLOR_FRAC;
			delta += gd * LUXEL_COLOR_FRAC;
			delta += bd * LUXEL_COLOR_FRAC;
			
			// is the change too high?
			if( total > 0.0 && ((delta / total) > LUXEL_TOLERANCE) )
				return false;
		}
	}
	
	// made it this far, they must be identical (or close enough)
	return true;
}



/*
=================
MergeBSPLuxels

merges two surface lightmaps' bsp luxels, overwriting occluded luxels
=================
*/
static bool MergeBSPLuxels( rawLightmap_t *a, int aNum, rawLightmap_t *b, int bNum )
{
	rawLightmap_t	*lm;
	int		x, y;
	float		luxel[3], *aLuxel, *bLuxel;
	
	if( a->customWidth != b->customWidth || a->customHeight != b->customHeight ||
	a->brightness != b->brightness || a->solid[aNum] != b->solid[bNum] ||
	a->bspLuxels[aNum] == NULL || b->bspLuxels[bNum] == NULL )
		return false;
	
	if( a->solid[aNum] && b->solid[bNum] )
	{
		VectorAdd( a->solidColor[aNum], b->solidColor[bNum], luxel );
		VectorScale( luxel, 0.5f, luxel );
		
		VectorCopy( luxel, a->solidColor[aNum] );
		VectorCopy( luxel, b->solidColor[bNum] );
		return true;
	}
	
	if( a->w != b->w || a->h != b->h )
		return false;
	
	for( y = 0; y < a->h; y++ )
	{
		for( x = 0; x < a->w; x++ )
		{
			lm = a;
			aLuxel = BSP_LUXEL( aNum, x, y );
			lm = b;
			bLuxel = BSP_LUXEL( bNum, x, y );
			
			if( aLuxel[0] < 0.0f ) VectorCopy( bLuxel, aLuxel );
			else if( bLuxel[0] < 0.0f ) VectorCopy( aLuxel, bLuxel );
			else
			{
				VectorAdd( aLuxel, bLuxel, luxel );
				VectorScale( luxel, 0.5f, luxel );
				VectorCopy( luxel, aLuxel );
				VectorCopy( luxel, bLuxel );
			}
		}
	}
	return true;
}



/*
=================
ApproximateLuxel

determines if a single luxel is can be approximated with the interpolated vertex rgba
=================
*/
static bool ApproximateLuxel( rawLightmap_t *lm, dvertex_t *dv )
{
	int	i, x, y, d, lightmapNum;
	float	*luxel;
	vec3_t	color, vertexColor;
	byte	cb[4], vcb[4];
	
	
	// find luxel xy coords
	x = dv->lm[0][0] / superSample;
	y = dv->lm[0][1] / superSample;
	if( x < 0 ) x = 0;
	else if( x >= lm->w )
		x = lm->w - 1;
	if( y < 0 ) y = 0;
	else if( y >= lm->h )
		y = lm->h - 1;
	
	for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
	{
		if( lm->styles[lightmapNum] == LS_NONE )
			continue;
		
		luxel = BSP_LUXEL( lightmapNum, x, y );
		if( luxel[0] < 0.0f || luxel[1] < 0.0f || luxel[2] < 0.0f )
			return true;
		
		VectorCopy( luxel, color );
		VectorCopy( dv->color[0], vertexColor );

		if( lightmapNum == 0 )
		{
			for( i = 0; i < 3; i++ )
			{
				if( color[i] < minLight[i] ) color[i] = minLight[i];
				if( vertexColor[i] < minLight[i] ) vertexColor[i] = minLight[i];
			}
		}
		
		ColorToBytes( color, cb, 1.0f );
		ColorToBytes( vertexColor, vcb, 1.0f );
		
		for( i = 0; i < 3; i++ )
		{
			d = cb[i] - vcb[i];
			if( d < 0 ) d *= -1;
			if( d > approximateTolerance )
				return false;
		}
	}
	return true;
}

/*
=================
ApproximateTriangle

determines if a single triangle can be approximated with vertex rgba
=================
*/
static bool ApproximateTriangle_r( rawLightmap_t *lm, dvertex_t *dv[3] )
{
	dvertex_t		mid, *dv2[3];
	int		max;
	
	if( ApproximateLuxel( lm, dv[0] ) == false )
		return false;
	if( ApproximateLuxel( lm, dv[1] ) == false )
		return false;
	if( ApproximateLuxel( lm, dv[2] ) == false )
		return false;
	
	// subdivide calc
	{
		int	i;
		float	dx, dy, dist, maxDist;
		
		
		// find the longest edge and split it
		max = -1;
		maxDist = 0;
		for( i = 0; i < 3; i++ )
		{
			dx = dv[i]->lm[0][0] - dv[(i + 1)%3]->lm[0][0];
			dy = dv[i]->lm[0][1] - dv[(i + 1)%3]->lm[0][1];
			dist = sqrt((dx * dx) + (dy * dy));
			if( dist > maxDist )
			{
				maxDist = dist;
				max = i;
			}
		}
		if( i < 0 || maxDist < 1.0f )
			return true;
	}

	// split the longest edge and map it
	LerpDrawVert( dv[max], dv[(max + 1)%3], &mid );
	if( ApproximateLuxel( lm, &mid ) == false )
		return false;
	
	VectorCopy( dv, dv2 );
	dv2[max] = &mid;
	if( ApproximateTriangle_r( lm, dv2 ) == false )
		return false;
	
	VectorCopy( dv, dv2 );
	dv2[(max + 1)%3] = &mid;
	return ApproximateTriangle_r( lm, dv2 );
}

/*
=================
ApproximateLightmap

determines if a raw lightmap can be approximated sufficiently with vertex colors
=================
*/
static bool ApproximateLightmap( rawLightmap_t *lm )
{
	int		n, num, i, x, y, pw[ 5 ], r;
	dsurface_t	*ds;
	surfaceInfo_t	*info;
	bsp_mesh_t	src, *subdivided, *mesh;
	dvertex_t		*verts, *dv[3];
	bool		approximated;
	
	if( approximateTolerance <= 0 )
		return false;
	
	// test for jmonroe
	// g-cont. Vova who is all this peoples ???
#if 0
	// don't approx lightmaps with styled twins
	if( lm->numStyledTwins > 0 ) return false;
		
	// don't approx lightmaps with styles
	for( i = 1; i < LM_STYLES; i++ )
	{
		if( lm->styles[i] != LS_NONE )
			return false;
	}
#endif
	
	// assume reduced until shadow detail is found
	approximated = true;
	
	for( n = 0; n < lm->numLightSurfaces; n++ )
	{
		num = lightSurfaces[lm->firstLightSurface+n];
		ds = &dsurfaces[num];
		info = &surfaceInfos[num];
		
		// assume not-reduced initially
		info->approximated = false;
		
		// bail if lightmap doesn't match up
		if( info->lm != lm ) continue;
		
		// bail if not vertex lit
		if( info->si->noVertexLight )
			continue;
		
		// assume that surfaces whose bounding boxes is smaller than 2x samplesize will be forced to vertex */
		if( (info->maxs[0] - info->mins[0]) <= (2.0f * info->sampleSize) && (info->maxs[1] - info->mins[1]) <= (2.0f * info->sampleSize) && (info->maxs[2] - info->mins[2]) <= (2.0f * info->sampleSize))
		{
			info->approximated = true;
			numSurfsVertexForced++;
			continue;
		}
		
		switch( ds->surfaceType )
		{
		case MST_PLANAR:
			verts = yDrawVerts + ds->firstvertex;
			info->approximated = true;
			for( i = 0; i < ds->numindices && info->approximated; i += 3 )
			{
				dv[0] = &verts[dindexes[ds->firstindex+i+0]];
				dv[1] = &verts[dindexes[ds->firstindex+i+1]];
				dv[2] = &verts[dindexes[ds->firstindex+i+2]];
				info->approximated = ApproximateTriangle_r( lm, dv );
			}
			break;
		case MST_PATCH:
			// make a mesh from the drawsurf 
			src.width = ds->patch.width;
			src.height = ds->patch.height;
			src.verts = &yDrawVerts[ds->firstvertex];
			subdivided = SubdivideMesh2( src, info->patchIterations );

			/* fit it to the curve and remove colinear verts on rows/columns */
			PutMeshOnCurve( *subdivided );
			mesh = RemoveLinearMeshColumnsRows( subdivided );
			FreeMesh( subdivided );
				
			verts = mesh->verts;
			info->approximated = true;
			for( y = 0; y < (mesh->height - 1) && info->approximated; y++ )
			{
				for( x = 0; x < (mesh->width - 1) && info->approximated; x++ )
				{
					pw[0] = x + (y * mesh->width);
					pw[1] = x + ((y + 1) * mesh->width);
					pw[2] = x + 1 + ((y + 1) * mesh->width);
					pw[3] = x + 1 + (y * mesh->width);
					pw[4] = x + (y * mesh->width); // same as pw[0]
						
					r = (x + y) & 1;
					dv[0] = &verts[pw[r+0]];
					dv[1] = &verts[pw[r+1]];
					dv[2] = &verts[pw[r+2]];
					info->approximated = ApproximateTriangle_r( lm, dv );
						
					/* get drawverts and map second triangle */
					dv[0] = &verts[pw[r+0]];
					dv[1] = &verts[pw[r+2]];
					dv[2] = &verts[pw[r+3]];
					if( info->approximated )
						info->approximated = ApproximateTriangle_r( lm, dv );
				}
			}
			FreeMesh( mesh );
			break;
		default: break;
		}
		
		if( info->approximated == false )
			approximated = false;
		else numSurfsVertexApproximated++;
	}
	return approximated;
}



/*
=================
TestOutLightmapStamp

tests a stamp on a given lightmap for validity
=================
*/
static bool TestOutLightmapStamp( rawLightmap_t *lm, int lightmapNum, outLightmap_t *olm, int x, int y )
{
	int	sx, sy, ox, oy, offset;
	float	*luxel;
	
	if( x < 0 || y < 0 || (x + lm->w) > olm->customWidth || (y + lm->h) > olm->customHeight )
		return false;
	
	// solid lightmaps test a 1x1 stamp
	if( lm->solid[lightmapNum] )
	{
		offset = (y * olm->customWidth) + x;
		if( olm->lightBits[offset>>3] & (1<<(offset & 7)))
			return false;
		return true;
	}
	
	for( sy = 0; sy < lm->h; sy++ )
	{
		for( sx = 0; sx < lm->w; sx++ )
		{
			luxel = BSP_LUXEL( lightmapNum, sx, sy );
			if( luxel[0] < 0.0f ) continue;
			
			// get bsp lightmap coords and test
			ox = x + sx;
			oy = y + sy;
			offset = (oy * olm->customWidth) + ox;
			if( olm->lightBits[offset>>3] & (1<<(offset & 7)))
				return false;
		}
	}
	return true;
}



/*
=================
SetupOutLightmap

sets up an output lightmap
=================
*/
static void SetupOutLightmap( rawLightmap_t *lm, outLightmap_t *olm )
{
	if( lm == NULL || olm == NULL )
		return;
	
	// is this a "normal" bsp-stored lightmap?
	if( 0 )
	{
		olm->lightmapNum = num_lightmaps;
		num_lightmaps++;
		
		// lightmaps are interleaved with light direction maps
		if( deluxemap ) num_lightmaps++;
	}
	else olm->lightmapNum = -3;
	
	// set external lightmap number
	olm->extLightmapNum = -1;
	
	olm->numLightmaps = 0;
	olm->customWidth = lm->customWidth;
	olm->customHeight = lm->customHeight;
	olm->freeLuxels = olm->customWidth * olm->customHeight;
	olm->numShaders = 0;
	
	// allocate buffers
	olm->lightBits = BSP_Malloc((olm->customWidth * olm->customHeight / 8) + 8 );
	olm->bspLightBytes = BSP_Malloc( olm->customWidth * olm->customHeight * 3 );
	if( deluxemap ) olm->bspDirBytes = BSP_Malloc( olm->customWidth * olm->customHeight * 3 );
}



/*
=================
FindOutLightmaps

for a given surface lightmap, find output lightmap pages and positions for it
=================
*/
static void FindOutLightmaps( rawLightmap_t *lm )
{
	int		i, j, lightmapNum, xMax, yMax, x, y, sx, sy, ox, oy, offset, temp;
	outLightmap_t	*olm;
	surfaceInfo_t	*info;
	float		*luxel, *deluxel;
	vec3_t		color, direction;
	byte		*pixel;
	bool		ok;
	
	
	// set default lightmap number (-3 = LIGHTMAP_BY_VERTEX)
	for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
		lm->outLightmapNums[lightmapNum] = -3;

	// can this lightmap be approximated with vertex color?
	if( ApproximateLightmap( lm )) return;

	// walk list
	for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
	{
		if( lm->styles[lightmapNum] == LS_NONE )
			continue;

		// don't store twinned lightmaps
		if( lm->twins[lightmapNum] != NULL )
			continue;
		
		// if this is a styled lightmap, try some normalized locations first
		ok = false;
		if( lightmapNum > 0 && outLightmaps != NULL )
		{
			for( j = 0; j < 2; j++ )
			{
				for( i = 0; i < numOutLightmaps; i++ )
				{
					olm = &outLightmaps[i];
					if( olm->freeLuxels < lm->used )
						continue;
					// don't store non-custom raw lightmaps on custom bsp lightmaps
					if( olm->customWidth != lm->customWidth || olm->customHeight != lm->customHeight )
						continue;
					
					if( j == 0 )
					{
						x = lm->lightmapX[0];
						y = lm->lightmapY[0];
						ok = TestOutLightmapStamp( lm, lightmapNum, olm, x, y );
					}
					else
					{
						for( sy = -1; sy <= 1; sy++ )
						{
							for( sx = -1; sx <= 1; sx++ )
							{
								x = lm->lightmapX[0] + sx * (olm->customWidth >> 1);	//%	lm->w;
								y = lm->lightmapY[0] + sy * (olm->customHeight >> 1);	//%	lm->h;
								ok = TestOutLightmapStamp( lm, lightmapNum, olm, x, y );

								if( ok ) break;
							}
							if( ok ) break;
						}
					}
					if( ok ) break;
				}
				if( ok ) break;
			}
		}
		
		// try normal placement algorithm
		if( ok == false )
		{
			x = 0;
			y = 0;
			
			for( i = 0; i < numOutLightmaps; i++ )
			{
				olm = &outLightmaps[i];
				
				if( olm->freeLuxels < lm->used )
					continue;
				
				// don't store non-custom raw lightmaps on custom bsp lightmaps
				if( olm->customWidth != lm->customWidth || olm->customHeight != lm->customHeight )
					continue;
				
				if( lm->solid[ lightmapNum ] )
				{
					xMax = olm->customWidth;
					yMax = olm->customHeight;
				}
				else
				{
					xMax = (olm->customWidth - lm->w) + 1;
					yMax = (olm->customHeight - lm->h) + 1;
				}
				
				for( y = 0; y < yMax; y++ )
				{
					for( x = 0; x < xMax; x++ )
					{
						// find a fine tract of lauhnd
						ok = TestOutLightmapStamp( lm, lightmapNum, olm, x, y );
						
						if( ok ) break;
					}
					if( ok ) break;
				}
				if( ok ) break;
				
				x = 0;
				y = 0;
			}
		}
		
		if( ok == false )
		{
			// allocate two new output lightmaps
			numOutLightmaps += 2;
			olm = BSP_Malloc( numOutLightmaps * sizeof( outLightmap_t ) );
			if( outLightmaps != NULL && numOutLightmaps > 2 )
			{
				Mem_Copy( olm, outLightmaps, (numOutLightmaps - 2) * sizeof( outLightmap_t ));
				BSP_Free( outLightmaps );
			}
			outLightmaps = olm;
			
			// initialize both out lightmaps
			SetupOutLightmap( lm, &outLightmaps[numOutLightmaps-2] );
			SetupOutLightmap( lm, &outLightmaps[numOutLightmaps-1] );
			
			// set out lightmap
			i = numOutLightmaps - 2;
			olm = &outLightmaps[i];
			
			// set stamp xy origin to the first surface lightmap
			if( lightmapNum > 0 )
			{
				x = lm->lightmapX[0];
				y = lm->lightmapY[0];
			}
		}
		
		// if this is a style-using lightmap, it must be exported
		if( lightmapNum > 0 ) olm->extLightmapNum = 0;
		
		// add the surface lightmap to the bsp lightmap
		lm->outLightmapNums[lightmapNum] = i;

		lm->lightmapX[lightmapNum] = x;
		lm->lightmapY[lightmapNum] = y;
		olm->numLightmaps++;
		
		for( i = 0; i < lm->numLightSurfaces; i++ )
		{
			info = &surfaceInfos[lightSurfaces[lm->firstLightSurface+i]];
			
			for( j = 0; j < olm->numShaders; j++ )
			{
				if( olm->shaders[j] == info->si )
					break;
			}
			
			// if it doesn't exist, add it
			if( j >= olm->numShaders && olm->numShaders < MAX_LIGHTMAP_SHADERS )
			{
				olm->shaders[olm->numShaders] = info->si;
				olm->numShaders++;
				numLightmapShaders++;
			}
		}
		
		if( lm->solid[lightmapNum] )
		{
			xMax = 1;
			yMax = 1;
		}
		else
		{
			xMax = lm->w;
			yMax = lm->h;
		}
		
		// mark the bits used
		for( y = 0; y < yMax; y++ )
		{
			for( x = 0; x < xMax; x++ )
			{
				luxel = BSP_LUXEL( lightmapNum, x, y );
				deluxel = BSP_DELUXEL( x, y );
				if( luxel[0] < 0.0f && !lm->solid[lightmapNum])
					continue;
				
				if( lm->solid[lightmapNum] )
				{
					if( debug ) VectorSet( color, 255.0f, 0.0f, 0.0f );
					else VectorCopy( lm->solidColor[lightmapNum], color );
				}
				else VectorCopy( luxel, color );
				
				// styles are not affected by minlight
				if( lightmapNum == 0 )
				{
					for( i = 0; i < 3; i++ )
					{
						if( color[i] < minLight[i] )
							color[i] = minLight[i];
					}
				}
				
				// get bsp lightmap coords
				ox = x + lm->lightmapX[lightmapNum];
				oy = y + lm->lightmapY[lightmapNum];
				offset = (oy * olm->customWidth) + ox;
				
				// flag pixel as used
				olm->lightBits[offset>>3] |= (1<<(offset & 7));
				olm->freeLuxels--;
				
				// store color
				pixel = olm->bspLightBytes + (((oy * olm->customWidth) + ox) * 3);
				ColorToBytes( color, pixel, lm->brightness );
				
				// store direction
				if( deluxemap )
				{
					// normalize average light direction
					VectorCopy( deluxel, direction );
					if( VectorNormalizeLength( direction ))
					{
						// encode [-1,1] in [0,255]
						pixel = olm->bspDirBytes + (((oy * olm->customWidth) + ox) * 3);
						for( i = 0; i < 3; i++ )
						{
							temp = (direction[i] + 1.0f) * 127.5f;
							if( temp < 0 ) pixel[i] = 0;
							else if( temp > 255 )
								pixel[i] = 255;
							else pixel[i] = temp;
						}
					}
				}
			}
		}
	}
}



/*
=================
CompareRawLightmap

compare function for qsort
=================
*/
static int CompareRawLightmap( const void *a, const void *b )
{
	rawLightmap_t	*alm, *blm;
	surfaceInfo_t	*aInfo, *bInfo;
	int		i, min, diff;
	
	alm = &rawLightmaps[*((int*)a)];
	blm = &rawLightmaps[*((int*)b)];
	
	// get min number of surfaces
	min = (alm->numLightSurfaces < blm->numLightSurfaces ? alm->numLightSurfaces : blm->numLightSurfaces);
	
	for( i = 0; i < min; i++ )
	{
		aInfo = &surfaceInfos[lightSurfaces[alm->firstLightSurface+i]];
		bInfo = &surfaceInfos[lightSurfaces[blm->firstLightSurface+i]];
		
		diff = com.strcmp( aInfo->si->name, bInfo->si->name );
		if( diff != 0 ) return diff;
	}

	diff = 0;
	for( i = 0; i < LM_STYLES; i++ )
		diff += blm->styles[i] - alm->styles[i];
	if( diff ) return diff;
	
	diff = (blm->w * blm->h) - (alm->w * alm->h);
	if( diff != 0 ) return diff;
	
	return 0;
}



/*
=================
StoreSurfaceLightmaps

stores the surface lightmaps into the bsp as byte rgb triplets
=================
*/
void StoreSurfaceLightmaps( void )
{
	int		i, j, k, x, y, lx, ly, sx, sy, *cluster, mappedSamples;
	int		size, lightmapNum, lightmapNum2;
	float		*normal, *luxel, *bspLuxel, *bspLuxel2, *radLuxel, samples, occludedSamples;
	vec3_t		sample, occludedSample, dirSample, colorMins, colorMaxs;
	float		*deluxel, *bspDeluxel, *bspDeluxel2;
	int		numUsed, numTwins, numTwinLuxels, numStored;
	float		lmx, lmy, efficiency;
	vec3_t		color;
	dsurface_t	*ds, *parent, dsTemp;
	surfaceInfo_t	*info;
	rawLightmap_t	*lm, *lm2;
	outLightmap_t	*olm;
	dvertex_t		*dv, *ydv, *dvParent;
	string		filename;
	char		*rgbGenValues[256];
	char		*alphaGenValues[256];
	
	
	MsgDev( D_NOTE, "--- StoreSurfaceLightmaps ---\n" );
	
	memset( rgbGenValues, 0, sizeof( rgbGenValues ));
	memset( alphaGenValues, 0, sizeof( alphaGenValues ));
	
	/*
	-----------------------------------------------------------------
	   average the sampled luxels into the bsp luxels
	-----------------------------------------------------------------
	*/
	
	MsgDev( D_NOTE, "Subsampling..." );
	
	numUsed = 0;
	numTwins = 0;
	numTwinLuxels = 0;
	numSolidLightmaps = 0;
	for( i = 0; i < numRawLightmaps; i++ )
	{
		lm = &rawLightmaps[i];
		
		for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
		{
			if( lm->superLuxels[lightmapNum] == NULL )
				continue;
			
			// allocate bsp luxel storage
			if( lm->bspLuxels[lightmapNum] == NULL )
			{
				size = lm->w * lm->h * BSP_LUXEL_SIZE * sizeof( float );
				lm->bspLuxels[lightmapNum] = BSP_Malloc( size );
			}

			// allocate radiosity lightmap storage
			if( bounce )
			{
				size = lm->w * lm->h * RAD_LUXEL_SIZE * sizeof( float );
				if( lm->radLuxels[lightmapNum] == NULL )
					lm->radLuxels[lightmapNum] = BSP_Malloc( size );
			}
			
			// average supersampled luxels
			for( y = 0; y < lm->h; y++ )
			{
				for( x = 0; x < lm->w; x++ )
				{
					// subsample
					samples = 0.0f;
					occludedSamples = 0.0f;
					mappedSamples = 0;
					VectorClear( sample );
					VectorClear( occludedSample );
					VectorClear( dirSample );
					for( ly = 0; ly < superSample; ly++ )
					{
						for( lx = 0; lx < superSample; lx++ )
						{
							// sample luxel
							sx = x * superSample + lx;
							sy = y * superSample + ly;
							luxel = SUPER_LUXEL( lightmapNum, sx, sy );
							deluxel = SUPER_DELUXEL( sx, sy );
							normal = SUPER_NORMAL( sx, sy );
							cluster = SUPER_CLUSTER( sx, sy );
							
							// sample deluxemap
							if( deluxemap && lightmapNum == 0 )
								VectorAdd( dirSample, deluxel, dirSample );
							
							// keep track of used/occluded samples
							if( *cluster != CLUSTER_UNMAPPED )
								mappedSamples++;
							
							// handle lightmap border?
							if( lightmapBorder && (sx == 0 || sx == (lm->sw - 1) || sy == 0 || sy == (lm->sh - 1) ) && luxel[3] > 0.0f )
							{
								VectorSet( sample, 255.0f, 0.0f, 0.0f );
								samples += 1.0f;
							}
							else if( debug && *cluster < 0 ) // handle debug
							{
								if( *cluster == CLUSTER_UNMAPPED )
									VectorSet( luxel, 255, 204, 0 );
								else if( *cluster == CLUSTER_OCCLUDED )
									VectorSet( luxel, 255, 0, 255 );
								else if( *cluster == CLUSTER_FLOODED )
									VectorSet( luxel, 0, 32, 255 );
								VectorAdd( occludedSample, luxel, occludedSample );
								occludedSamples += 1.0f;
							}
							else if( luxel[3] > 0.0f ) // normal luxel handling
							{
								// handle lit or flooded luxels
								if( *cluster > 0 || *cluster == CLUSTER_FLOODED )
								{
									VectorAdd( sample, luxel, sample );
									samples += luxel[3];
								}
								else // handle occluded or unmapped luxels
								{
									VectorAdd( occludedSample, luxel, occludedSample );
									occludedSamples += luxel[3];
								}
								
								// handle style debugging
								if( debug && lightmapNum > 0 && x < 2 && y < 2 )
								{
									VectorCopy( debugColors[0], sample );
									samples = 1;
								}
							}
						}
					}
					
					// only use occluded samples if necessary
					if( samples <= 0.0f )
					{
						VectorCopy( occludedSample, sample );
						samples = occludedSamples;
					}
					
					// get luxels
					luxel = SUPER_LUXEL( lightmapNum, x, y );
					deluxel = SUPER_DELUXEL( x, y );
					
					// store light direction
					if( deluxemap && lightmapNum == 0 )
						VectorCopy( dirSample, deluxel );
					
					// store the sample back in super luxels
					if( samples > 0.01f )
					{
						VectorScale( sample, (1.0f / samples), luxel );
						luxel[3] = 1.0f;
					}
					else if( mappedSamples > 0 )
					{
						// if any samples were mapped in any way, store ambient color
						if( lightmapNum == 0 )
							VectorCopy( ambientColor, luxel );
						else VectorClear( luxel );
						luxel[3] = 1.0f;
					}
					else
					{
						// store a bogus value to be fixed later
						VectorClear( luxel );
						luxel[3] = -1.0f;
					}
				}
			}
			
			lm->used = 0;
			ClearBounds( colorMins, colorMaxs );
			
			// clean up and store into bsp luxels
			for( y = 0; y < lm->h; y++ )
			{
				for( x = 0; x < lm->w; x++ )
				{
					// get luxels
					luxel = SUPER_LUXEL( lightmapNum, x, y );
					deluxel = SUPER_DELUXEL( x, y );
					
					// copy light direction
					if( deluxemap && lightmapNum == 0 )
						VectorCopy( deluxel, dirSample );
					
					// is this a valid sample?
					if( luxel[3] > 0.0f )
					{
						VectorCopy( luxel, sample );
						samples = luxel[3];
						numUsed++;
						lm->used++;
						
						// fix negative samples
						for( j = 0; j < 3; j++ )
						{
							if( sample[j] < 0.0f )
								sample[j] = 0.0f;
						}
					}
					else
					{
						// nick an average value from the neighbors
						VectorClear( sample );
						VectorClear( dirSample );
						samples = 0.0f;
						
						// FIXME: why is this disabled??
						// g-cont. yes! why is this disabled ?
						for( sy = (y - 1); sy <= (y + 1); sy++ )
						{
							if( sy < 0 || sy >= lm->h )
								continue;
							
							for( sx = (x - 1); sx <= (x + 1); sx++ )
							{
								if( sx < 0 || sx >= lm->w || (sx == x && sy == y))
									continue;
								
								// get neighbor's particulars
								luxel = SUPER_LUXEL( lightmapNum, sx, sy );
								if( luxel[3] < 0.0f )
									continue;
								VectorAdd( sample, luxel, sample );
								samples += luxel[3];
							}
						}
						
						// no samples?
						if( samples == 0.0f )
						{
							VectorSet( sample, -1.0f, -1.0f, -1.0f );
							samples = 1.0f;
						}
						else
						{
							numUsed++;
							lm->used++;
							
							// fix negative samples
							for( j = 0; j < 3; j++ )
							{
								if( sample[j] < 0.0f )
									sample[j] = 0.0f;
							}
						}
					}
					
					// scale the sample
					VectorScale( sample, (1.0f / samples), sample );
					
					// store the sample in the radiosity luxels
					if( bounce > 0 )
					{
						radLuxel = RAD_LUXEL( lightmapNum, x, y );
						VectorCopy( sample, radLuxel );
						
						// if only storing bounced light, early out here
						if( bounceOnly && !bouncing )
							continue;
					}
					
					/* store the sample in the bsp luxels */
					bspLuxel = BSP_LUXEL( lightmapNum, x, y );
					bspDeluxel = BSP_DELUXEL( x, y );
					
					VectorAdd( bspLuxel, sample, bspLuxel );
					if( deluxemap && lightmapNum == 0 )
						VectorAdd( bspDeluxel, dirSample, bspDeluxel );
					
					/* add color to bounds for solid checking */
					if( samples > 0.0f )
						AddPointToBounds( bspLuxel, colorMins, colorMaxs );
				}
			}
			
			// set solid color
			lm->solid[lightmapNum] = false;
			VectorAdd( colorMins, colorMaxs, lm->solidColor[lightmapNum] );
			VectorScale( lm->solidColor[lightmapNum], 0.5f, lm->solidColor[lightmapNum] );
			
			// nocollapse prevents solid lightmaps
			if( noCollapse == false )
			{
				// check solid color
				VectorSubtract( colorMaxs, colorMins, sample );

				// small lightmaps get forced to solid color
				if((sample[0] <= SOLID_EPSILON && sample[1] <= SOLID_EPSILON && sample[2] <= SOLID_EPSILON) || (lm->w <= 2 && lm->h <= 2))
				{
					// set to solid
					VectorCopy( colorMins, lm->solidColor[lightmapNum] );
					lm->solid[lightmapNum] = true;
					numSolidLightmaps++;
				}
				
				// if all lightmaps aren't solid, then none of them are solid
				if( lm->solid[lightmapNum] != lm->solid[0] )
				{
					for( y = 0; y < LM_STYLES; y++ )
					{
						if( lm->solid[ y ] )
							numSolidLightmaps--;
						lm->solid[ y ] = false;
					}
				}
			}
			
			// wrap bsp luxels if necessary
			if( lm->wrap[0] )
			{
				for( y = 0; y < lm->h; y++ )
				{
					bspLuxel = BSP_LUXEL( lightmapNum, 0, y );
					bspLuxel2 = BSP_LUXEL( lightmapNum, lm->w - 1, y );
					VectorAdd( bspLuxel, bspLuxel2, bspLuxel );
					VectorScale( bspLuxel, 0.5f, bspLuxel );
					VectorCopy( bspLuxel, bspLuxel2 );
					if( deluxemap && lightmapNum == 0 )
					{
						bspDeluxel = BSP_DELUXEL( 0, y );
						bspDeluxel2 = BSP_DELUXEL( lm->w - 1, y );
						VectorAdd( bspDeluxel, bspDeluxel2, bspDeluxel );
						VectorScale( bspDeluxel, 0.5f, bspDeluxel );
						VectorCopy( bspDeluxel, bspDeluxel2 );
					}
				}
			}
			if( lm->wrap[1] )
			{
				for( x = 0; x < lm->w; x++ )
				{
					bspLuxel = BSP_LUXEL( lightmapNum, x, 0 );
					bspLuxel2 = BSP_LUXEL( lightmapNum, x, lm->h - 1 );
					VectorAdd( bspLuxel, bspLuxel2, bspLuxel );
					VectorScale( bspLuxel, 0.5f, bspLuxel );
					VectorCopy( bspLuxel, bspLuxel2 );
					if( deluxemap && lightmapNum == 0 )
					{
						bspDeluxel = BSP_DELUXEL( x, 0 );
						bspDeluxel2 = BSP_DELUXEL( x, lm->h - 1 );
						VectorAdd( bspDeluxel, bspDeluxel2, bspDeluxel );
						VectorScale( bspDeluxel, 0.5f, bspDeluxel );
						VectorCopy( bspDeluxel, bspDeluxel2 );
					}
				}
			}
		}
	}
	
	/*
	-----------------------------------------------------------------
	   collapse non-unique lightmaps
	-----------------------------------------------------------------
	*/
	
	if( noCollapse == false && deluxemap == false )
	{
		// note it
		MsgDev( D_NOTE, "collapsing..." );
		
		// set all twin refs to null
		for( i = 0; i < numRawLightmaps; i++ )
		{
			for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
			{
				rawLightmaps[i].twins[lightmapNum] = NULL;
				rawLightmaps[i].twinNums[lightmapNum] = -1;
				rawLightmaps[i].numStyledTwins = 0;
			}
		}
		
		for( i = 0; i < numRawLightmaps; i++ )
		{
			lm = &rawLightmaps[i];
			
			for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
			{
				if( lm->bspLuxels[lightmapNum] == NULL || lm->twins[lightmapNum] != NULL )
					continue;
				
				// find all lightmaps that are virtually identical to this one
				for( j = i + 1; j < numRawLightmaps; j++ )
				{
					lm2 = &rawLightmaps[j];
					
					for( lightmapNum2 = 0; lightmapNum2 < LM_STYLES; lightmapNum2++ )
					{
						if( lm2->bspLuxels[lightmapNum2] == NULL || lm2->twins[lightmapNum2] != NULL )
							continue;
						
						// compare them
						if( CompareBSPLuxels( lm, lightmapNum, lm2, lightmapNum2 ) )
						{
							// merge and set twin
							if( MergeBSPLuxels( lm, lightmapNum, lm2, lightmapNum2 ) )
							{
								lm2->twins[lightmapNum2] = lm;
								lm2->twinNums[lightmapNum2] = lightmapNum;
								numTwins++;
								numTwinLuxels += (lm->w * lm->h);
								
								// count styled twins
								if( lightmapNum > 0 ) lm->numStyledTwins++;
							}
						}
					}
				}
			}
		}
	}
	
	/*
	-----------------------------------------------------------------
	   sort raw lightmaps by shader
	-----------------------------------------------------------------
	*/
	
	MsgDev( D_NOTE, "sorting..." );
	
	if( sortLightmaps == NULL ) sortLightmaps = BSP_Malloc( numRawLightmaps * sizeof( int ));
	
	// fill it out and sort it
	for( i = 0; i < numRawLightmaps; i++ ) sortLightmaps[i] = i;
	qsort( sortLightmaps, numRawLightmaps, sizeof( int ), CompareRawLightmap );
	
	/*
	-----------------------------------------------------------------
	   allocate output lightmaps
	-----------------------------------------------------------------
	*/
	
	MsgDev( D_NOTE, "allocating..." );
	
	// kill all existing output lightmaps
	if( outLightmaps != NULL )
	{
		for( i = 0; i < numOutLightmaps; i++ )
		{
			BSP_Free( outLightmaps[i].lightBits );
			BSP_Free( outLightmaps[i].bspLightBytes );
		}
		BSP_Free( outLightmaps );
		outLightmaps = NULL;
	}
	
	numLightmapShaders = 0;
	numOutLightmaps = 0;
	numBSPLightmaps = 0;
	numExtLightmaps = 0;
	
	// find output lightmap
	for( i = 0; i < numRawLightmaps; i++ )
	{
		lm = &rawLightmaps[sortLightmaps[i]];
		FindOutLightmaps( lm );
	}
	
	// set output numbers in twinned lightmaps
	for( i = 0; i < numRawLightmaps; i++ )
	{
		lm = &rawLightmaps[sortLightmaps[i]];

		for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
		{
			lm2 = lm->twins[lightmapNum];
			if( lm2 == NULL ) continue;
			lightmapNum2 = lm->twinNums[lightmapNum];
			
			lm->outLightmapNums[lightmapNum] = lm2->outLightmapNums[lightmapNum2];
			lm->lightmapX[lightmapNum] = lm2->lightmapX[lightmapNum2];
			lm->lightmapY[lightmapNum] = lm2->lightmapY[lightmapNum2];
		}
	}
	
	/*
	-----------------------------------------------------------------
	   store output lightmaps
	-----------------------------------------------------------------
	*/
	
	MsgDev( D_NOTE, "storing..." );
	
	if( lightbytes != NULL ) BSP_Free( lightbytes );
	num_lightbytes = 0;
	lightbytes = NULL;
	
	for( i = 0; i < numOutLightmaps; i++ )
	{
		olm = &outLightmaps[i];
		olm->extLightmapNum = numExtLightmaps;
		WriteLightmap( numExtLightmaps, olm->customWidth, olm->customHeight, olm->bspLightBytes );
		numExtLightmaps++;
			
		if( deluxemap )
		{
			WriteLightmap( numExtLightmaps, olm->customWidth, olm->customHeight, olm->bspDirBytes );
			numExtLightmaps++;
	
			if( debugDeluxemap ) olm->extLightmapNum++;
		}
	}
	
	if( numExtLightmaps > 0 ) MsgDev( D_NOTE, "\n" );
	
	// delete unused external lightmaps
	for( i = numExtLightmaps; i; i++ )
	{
		com.sprintf( filename, "gfx/lightmaps/%s_%04d.dds", gs_filename, i );
		if( !FS_FileExists( filename )) break;
		FS_Delete( filename );
	}
	
	/*
	-----------------------------------------------------------------
	   project the lightmaps onto the bsp surfaces
	-----------------------------------------------------------------
	*/
	
	MsgDev( D_NOTE, "projecting..." );
	
	for( i = 0; i < numsurfaces; i++ )
	{
		ds = &dsurfaces[i];
		info = &surfaceInfos[i];
		lm = info->lm;
		olm = NULL;
		
		if( info->parentSurfaceNum >= 0 )
		{
			parent = &dsurfaces[info->parentSurfaceNum];
			Mem_Copy( &dsTemp, ds, sizeof( *ds ));
			Mem_Copy( ds, parent, sizeof( *ds ) );
			
			ds->fognum = dsTemp.fognum;
			ds->firstvertex = dsTemp.firstvertex;
			ds->firstindex = dsTemp.firstindex;
			Mem_Copy( ds->flat.vecs, dsTemp.flat.vecs, sizeof( dsTemp.flat.vecs ));
			
			/* set vertex data */
			dv = &dvertexes[ds->firstvertex];
			dvParent = &dvertexes[parent->firstvertex];
			for( j = 0; j < ds->numvertices; j++ )
			{
				Mem_Copy( dv[j].lm, dvParent[j].lm, sizeof( dv[j].lm ));
				Mem_Copy( dv[j].color, dvParent[j].color, sizeof( dv[j].color ));
			}
			
			// skip the rest
			continue;
		}
		// handle vertex lit or approximated surfaces
		else if( lm == NULL || lm->outLightmapNums[0] < 0 )
		{
			for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
			{
				ds->lmapNum[lightmapNum] = -3;
				ds->lStyles[lightmapNum] = ds->vStyles[lightmapNum];
			}
		}
		else // handle lightmapped surfaces
		{
			for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
			{
				ds->lStyles[lightmapNum] = lm->styles[lightmapNum];
				
				if( lm->styles[lightmapNum] == LS_NONE || lm->outLightmapNums[lightmapNum] < 0 )
				{
					ds->lmapNum[lightmapNum] = -3;
					continue;
				}
				
				olm = &outLightmaps[lm->outLightmapNums[lightmapNum]];				
		
				// set bsp lightmap number
				// FIXME: test
				ds->lmapNum[lightmapNum] = lm->outLightmapNums[lightmapNum];//olm->lightmapNum;
				Msg("LightMap number: [%i] == %i\n", lightmapNum, lm->outLightmapNums[lightmapNum] );
				// deluxemap debugging makes the deluxemap visible
				if( deluxemap && debugDeluxemap && lightmapNum == 0 )
					ds->lmapNum[lightmapNum]++;
				
				// calc lightmap origin in texture space
				lmx = (float) lm->lightmapX[lightmapNum] / (float) olm->customWidth;
				lmy = (float) lm->lightmapY[lightmapNum] / (float) olm->customHeight;
				
				// calc lightmap st coords
				dv = &dvertexes[ds->firstvertex];
				ydv = &yDrawVerts[ds->firstvertex];
				for( j = 0; j < ds->numvertices; j++ )
				{
					if( lm->solid[lightmapNum] )
					{
						dv[j].lm[lightmapNum][0] = lmx + (0.5f / (float) olm->customWidth);
						dv[j].lm[lightmapNum][1] = lmy + (0.5f / (float) olm->customWidth);
					}
					else
					{
						dv[j].lm[lightmapNum][0] = lmx + (ydv[j].lm[0][0] / (superSample * olm->customWidth));
						dv[j].lm[lightmapNum][1] = lmy + (ydv[j].lm[0][1] / (superSample * olm->customHeight));
					}
				}
			}
		}
		
		// store vertex colors
		dv = &dvertexes[ds->firstvertex];
		for( j = 0; j < ds->numvertices; j++ )
		{
			for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
			{
				// handle unused style
				if( ds->vStyles[lightmapNum] == LS_NONE )
				{
					VectorClear( color );
				}
				else
				{
					luxel = VERTEX_LUXEL( lightmapNum, ds->firstvertex + j );
					VectorCopy( luxel, color );
					
					// set minimum light
					if( lightmapNum == 0 )
					{
						for( k = 0; k < 3; k++ )
							if( color[k] < minVertexLight[k] )
								color[k] = minVertexLight[k];
					}
				}
				
				if( !info->si->noVertexLight )
					ColorToBytes( color, dv[j].color[lightmapNum], info->si->vertexScale );
			}
		}
		ds->shadernum = EmitShader( info->si->name, &dshaders[ds->shadernum].contents, &dshaders[ds->shadernum].surfaceFlags );
	}
	
	MsgDev( D_NOTE, "done.\n" );
	
	numStored = num_lightbytes / 3;
	efficiency = (numStored <= 0) ? 0 : (float) numUsed / (float) numStored;
	
	MsgDev( D_INFO, "%6i luxels used\n", numUsed );
	MsgDev( D_INFO, "%6i luxels stored (%3.2f percent efficiency)\n", numStored, efficiency * 100.0f );
	MsgDev( D_INFO, "%6i solid surface lightmaps\n", numSolidLightmaps );
	MsgDev( D_INFO, "%6i identical surface lightmaps, using %d luxels\n", numTwins, numTwinLuxels );
	MsgDev( D_INFO, "%6i vertex forced surfaces\n", numSurfsVertexForced );
	MsgDev( D_INFO, "%6i vertex approximated surfaces\n", numSurfsVertexApproximated );
	MsgDev( D_INFO, "%6i BSP lightmaps\n", numBSPLightmaps );
	MsgDev( D_INFO, "%6i total lightmaps\n", numOutLightmaps );
	MsgDev( D_INFO, "%6i unique lightmap/shader combinations\n", numLightmapShaders );
	
	WriteMapShaderFile();
}