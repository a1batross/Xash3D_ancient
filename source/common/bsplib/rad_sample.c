//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	rad_sample.c - radiocity samples
//=======================================================================

#include "bsplib.h"
#include "const.h"

#define MAX_SAMPLES			256
#define THETA_EPSILON		0.000001
#define EQUAL_NORMAL_EPSILON		0.01
#define NUDGE			0.5f
#define BOGUS_NUDGE			-99999.0f
#define VERTEX_NUDGE		4.0f
#define QUAD_PLANAR_EPSILON		0.5f
#define DIRT_CONE_ANGLE		88	// degrees
#define DIRT_NUM_ANGLE_STEPS		16
#define DIRT_NUM_ELEVATION_STEPS	3
#define DIRT_NUM_VECTORS		(DIRT_NUM_ANGLE_STEPS * DIRT_NUM_ELEVATION_STEPS)
#define STACK_LL_SIZE		(SUPER_LUXEL_SIZE * 64 * 64)
#define LIGHT_LUXEL( x, y )		(lightLuxels + ((((y) * lm->sw) + (x)) * SUPER_LUXEL_SIZE))
#define LIGHT_EPSILON		0.125f
#define LIGHT_NUDGE			2.0f

float	lightmapGamma = 1.0f;
float	lightmapCompensate = 1.0f;
float	shadeAngleDegrees = 0.0f;
bool	trisoup = false;
bool	dark = false;
bool	dirty = false;
bool	dirtDebug = false;
int	dirtMode = 0;
float	dirtDepth = 128.0f;
float	dirtScale = 1.0f;
float	dirtGain = 1.0f;
int	lightSamples = 1;
bool	debug = false;
bool	debugAxis = false;
bool	debugCluster = false;
bool	debugOrigin = false;
bool	lightmapBorder = false;
bool	normalmap = false;
bool	filter;
bool	cpmaHack = false;

static byte	radPvs[MAX_MAP_LEAFS/8];
static vec3_t	dirtVectors[DIRT_NUM_VECTORS];
static int	numDirtVectors = 0;
static int	numOpaqueBrushes, maxOpaqueBrush;
static byte	*opaqueBrushes;

/*
=============
ColorToBytes

ydnar: moved to here 2001-02-04
=============
*/
void ColorToBytes( const float *color, byte *colorBytes, float scale )
{
	int	i;
	float	max, gamma;
	vec3_t	sample;
	
	
	// scaling necessary for simulating r_overbrightBits on external lightmaps
	if( scale <= 0.0f ) scale = 1.0f;
	
	VectorScale( color, scale, sample );
	
	gamma = 1.0f / lightmapGamma;
	for( i = 0; i < 3; i++ )
	{
		if( sample[i] < 0.0f )
		{
			sample[i] = 0.0f;
			continue;
		}
		sample[i] = pow( sample[i] / 255.0f, gamma ) * 255.0f;
	}
	
	// clamp with color normalization
	max = sample[0];
	if( sample[1] > max ) max = sample[1];
	if( sample[2] > max ) max = sample[2];
	if( max > 255.0f ) VectorScale( sample, (255.0f / max), sample );
	
	// compensate for ingame overbrighting/bitshifting
	VectorScale( sample, (1.0f / lightmapCompensate), sample );
	
	colorBytes[0] = sample[0];
	colorBytes[1] = sample[1];
	colorBytes[2] = sample[2];
}


/*
===============================================================================

this section deals with phong shading (normal interpolation across brush faces)

===============================================================================
*/
/*
=============
SmoothNormals

smooths together coincident vertex normals across the bsp
=============
*/
void SmoothNormals( void )
{
	int		i, j, k, f, cs, numVerts, numVotes, fOld, start;
	float		shadeAngle, defaultShadeAngle, maxShadeAngle, dot, testAngle;
	dsurface_t	*ds;
	bsp_shader_t	*si;
	float		*shadeAngles;
	byte		*smoothed;
	vec3_t		average, diff;
	int		indexes[MAX_SAMPLES];
	vec3_t		votes[MAX_SAMPLES];
	
	// allocate shade angle table
	shadeAngles = BSP_Malloc( numvertexes * sizeof( float ));
	
	// allocate smoothed table
	cs = (numvertexes / 8) + 1;
	smoothed = BSP_Malloc( cs );
	
	// set default shade angle
	defaultShadeAngle = DEG2RAD( shadeAngleDegrees );
	maxShadeAngle = 0;
	
	// run through every surface and flag verts belonging to non-lightmapped surfaces
	// and set per-vertex smoothing angle
	for( i = 0; i < numsurfaces; i++ )
	{
		ds = &dsurfaces[i];
		
		si = surfaceInfos[i].si;
		if( si->shadeAngleDegrees )
			shadeAngle = DEG2RAD( si->shadeAngleDegrees );
		else shadeAngle = defaultShadeAngle;

		if( shadeAngle > maxShadeAngle ) maxShadeAngle = shadeAngle;
		
		for( j = 0; j < ds->numvertices; j++ )
		{
			f = ds->firstvertex + j;
			shadeAngles[f] = shadeAngle;
			if( ds->surfaceType == MST_TRIANGLE_SOUP )
				smoothed[f>>3] |= (1<<(f&7));
		}
		
		// optional force-to-trisoup
		if( trisoup && ds->surfaceType == MST_PLANAR )
		{
			ds->surfaceType = MST_TRIANGLE_SOUP;
			ds->lmapNum[0] = -3;
		}
	}
	
	// bail if no surfaces have a shade angle
	if( maxShadeAngle == 0 )
	{
		Mem_Free( shadeAngles );
		Mem_Free( smoothed );
		return;
	}
	
	fOld = -1;
	start = Sys_DoubleTime();
	
	for( i = 0; i < numvertexes; i++ )
	{
		f = 10 * i / numvertexes;
		if( f != fOld )
		{
			fOld = f;
			Msg( "%i...", f );
		}
		
		// already smoothed?
		if( smoothed[i>>3] & (1<<(i&7)))
			continue;
		
		VectorClear( average );
		numVerts = 0;
		numVotes = 0;
		
		// build a table of coincident vertexes
		for( j = i; j < numvertexes && numVerts < MAX_SAMPLES; j++ )
		{
			if( smoothed[j>>3] & (1<<(j&7)))
				continue;
			
			if( VectorCompare( yDrawVerts[i].point, yDrawVerts[j].point ) == false )
				continue;
			
			// use smallest shade angle
			shadeAngle = (shadeAngles[i] < shadeAngles[j] ? shadeAngles[i] : shadeAngles[j]);
			
			// check shade angle
			dot = DotProduct( dvertexes[i].normal, dvertexes[j].normal );
			if( dot > 1.0 ) dot = 1.0;
			else if( dot < -1.0 ) dot = -1.0;
			testAngle = acos( dot ) + THETA_EPSILON;
			if( testAngle >= shadeAngle ) continue;
			
			indexes[numVerts++] = j;
			
			// flag vertex
			smoothed[j>>3] |= (1<<(j&7));
			
			// see if this normal has already been voted
			for( k = 0; k < numVotes; k++ )
			{
				VectorSubtract( dvertexes[j].normal, votes[k], diff );
				if( fabs( diff[0] ) < EQUAL_NORMAL_EPSILON && fabs( diff[1] ) < EQUAL_NORMAL_EPSILON && fabs( diff[2] ) < EQUAL_NORMAL_EPSILON )
					break;
			}
			
			// add a new vote?
			if( k == numVotes && numVotes < MAX_SAMPLES )
			{
				VectorAdd( average, dvertexes[j].normal, average );
				VectorCopy( dvertexes[j].normal, votes[numVotes] );
				numVotes++;
			}
		}
		
		// don't average for less than 2 verts
		if( numVerts < 2 ) continue;
		
		// average normal
		if( VectorNormalizeLength( average ) > 0 )
		{
			for( j = 0; j < numVerts; j++ )
				VectorCopy( average, yDrawVerts[indexes[j]].normal );
		}
	}
	
	Mem_Free( shadeAngles );
	Mem_Free( smoothed );
	
	Msg( " (%i)\n", (int) (Sys_DoubleTime() - start) );
}



/*
===============================================================================

this section deals with phong shaded lightmap tracing

===============================================================================
*/
/*
=============
CalcTangentVectors

calculates the st tangent vectors for normalmapping
=============
*/
static bool CalcTangentVectors( int numVerts, dvertex_t **dv, vec3_t *stv, vec3_t *ttv )
{
	int		i;
	float		bb, s, t;
	vec3_t		bary;
	
	// calculate barycentric basis for the triangle
	bb = (dv[1]->st[0] - dv[0]->st[0]) * (dv[2]->st[1] - dv[0]->st[1]) - (dv[2]->st[0] - dv[0]->st[0]) * (dv[1]->st[1] - dv[0]->st[1]);
	if( fabs( bb ) < 0.00000001f )
		return false;
	
	for( i = 0; i < numVerts; i++ )
	{
		// calculate s tangent vector
		s = dv[i]->st[0] + 10.0f;
		t = dv[i]->st[1];
		bary[0] = ((dv[1]->st[0] - s) * (dv[2]->st[1] - t) - (dv[2]->st[0] - s) * (dv[1]->st[1] - t)) / bb;
		bary[1] = ((dv[2]->st[0] - s) * (dv[0]->st[1] - t) - (dv[0]->st[0] - s) * (dv[2]->st[1] - t)) / bb;
		bary[2] = ((dv[0]->st[0] - s) * (dv[1]->st[1] - t) - (dv[1]->st[0] - s) * (dv[0]->st[1] - t)) / bb;
		
		stv[i][0] = bary[0] * dv[0]->point[0] + bary[1] * dv[1]->point[0] + bary[2] * dv[2]->point[0];
		stv[i][1] = bary[0] * dv[0]->point[1] + bary[1] * dv[1]->point[1] + bary[2] * dv[2]->point[1];
		stv[i][2] = bary[0] * dv[0]->point[2] + bary[1] * dv[1]->point[2] + bary[2] * dv[2]->point[2];
		
		VectorSubtract( stv[i], dv[i]->point, stv[i] );
		VectorNormalize( stv[i] );
		
		// calculate t tangent vector
		s = dv[i]->st[0];
		t = dv[i]->st[1] + 10.0f;
		bary[0] = ((dv[1]->st[0] - s) * (dv[2]->st[1] - t) - (dv[2]->st[0] - s) * (dv[1]->st[1] - t)) / bb;
		bary[1] = ((dv[2]->st[0] - s) * (dv[0]->st[1] - t) - (dv[0]->st[0] - s) * (dv[2]->st[1] - t)) / bb;
		bary[2] = ((dv[0]->st[0] - s) * (dv[1]->st[1] - t) - (dv[1]->st[0] - s) * (dv[0]->st[1] - t)) / bb;
		
		ttv[i][0] = bary[0] * dv[0]->point[0] + bary[1] * dv[1]->point[0] + bary[2] * dv[2]->point[0];
		ttv[i][1] = bary[0] * dv[0]->point[1] + bary[1] * dv[1]->point[1] + bary[2] * dv[2]->point[1];
		ttv[i][2] = bary[0] * dv[0]->point[2] + bary[1] * dv[1]->point[2] + bary[2] * dv[2]->point[2];
		
		VectorSubtract( ttv[i], dv[i]->point, ttv[i] );
		VectorNormalize( ttv[i] );
	}
	return true;
}

/*
=============
PerturbNormal

perterbs the normal by the shader's normalmap in tangent space
=============
*/
static void PerturbNormal( dvertex_t *dv, bsp_shader_t *si, vec3_t pNormal, vec3_t stv[3], vec3_t ttv[3] )
{
	int		i;
	vec4_t		bump;

	VectorCopy( dv->normal, pNormal );
	
	// sample normalmap
	if( !RadSampleImage( si->normalImage->buffer, si->normalImage->width, si->normalImage->height, dv->st, bump ))
		return;
	
	// remap sampled normal from [0,255] to [-1,-1]
	for( i = 0; i < 3; i++ ) bump[i] = (bump[i] - 127.0f) * (1.0f / 127.5f);
	
	// scale tangent vectors and add to original normal
	VectorMA( dv->normal, bump[0], stv[0], pNormal );
	VectorMA( pNormal, bump[1], ttv[0], pNormal );
	VectorMA( pNormal, bump[2], dv->normal, pNormal );
	VectorNormalize( pNormal );
}


/*
=============
MapSingleLuxel

maps a luxel for triangle bv at
=============
*/
static int MapSingleLuxel( rawLightmap_t *lm, surfaceInfo_t *info, dvertex_t *dv, vec4_t plane, float pass, vec3_t stv[3], vec3_t ttv[3] )
{
	int		i, x, y, numClusters, *clusters, pointCluster, *cluster;
	float		*luxel, *origin, *normal, d, lightmapSampleOffset;
	bsp_shader_t	*si;
	vec3_t		pNormal;
	vec3_t		vecs[3];
	vec3_t		nudged;
	float		*nudge;
	static float	nudges[][2] = 
	{
	//{ 0, 0 },		// try center first
	{ -NUDGE, 0 },		// left
	{ NUDGE, 0 },		// right
	{ 0, NUDGE },		// up
	{ 0, -NUDGE },		// down
	{ -NUDGE, NUDGE },		// left / up
	{ NUDGE, -NUDGE },		// right / down
	{ NUDGE, NUDGE },		// right / up
	{ -NUDGE, -NUDGE },		// left / down
	{ BOGUS_NUDGE, BOGUS_NUDGE }
	};
	
	// find luxel xy coords (FIXME: subtract 0.5?)
	x = dv->lm[0][0];
	y = dv->lm[0][1];
	if( x < 0 ) x = 0;
	else if( x >= lm->sw )
		x = lm->sw - 1;
	if( y < 0 ) y = 0;
	else if( y >= lm->sh )
		y = lm->sh - 1;
	
	if( info != NULL )
	{
		si = info->si;
		numClusters = info->numSurfaceClusters;
		clusters = &surfaceClusters[info->firstSurfaceCluster];
	}
	else
	{
		si = NULL;
		numClusters = 0;
		clusters = NULL;
	}
	
	// get luxel, origin, cluster, and normal
	luxel = SUPER_LUXEL( 0, x, y );
	origin = SUPER_ORIGIN( x, y );
	normal = SUPER_NORMAL( x, y );
	cluster = SUPER_CLUSTER( x, y );
	
	// don't attempt to remap occluded luxels for planar surfaces
	if( (*cluster) == CLUSTER_OCCLUDED && lm->plane != NULL )
		return (*cluster);
	else if( (*cluster) >= 0 )	// only average the normal for premapped luxels
	{
		// do bumpmap calculations
		if( stv != NULL ) PerturbNormal( dv, si, pNormal, stv, ttv );
		else VectorCopy( dv->normal, pNormal );
		
		// add the additional normal data
		VectorAdd( normal, pNormal, normal );
		luxel[3] += 1.0f;
		return (*cluster);
	}
	
	// otherwise, unmapped luxels (*cluster == CLUSTER_UNMAPPED) will have their full attributes calculated
	// get origin

	// axial lightmap projection
	if( lm->vecs != NULL )
	{
		// calculate an origin for the sample from the lightmap vectors
		VectorCopy( lm->origin, origin );
		for( i = 0; i < 3; i++ )
		{
			// add unless it's the axis, which is taken care of later
			if( i == lm->axisNum ) continue;
			origin[i] += (x * lm->vecs[0][i]) + (y * lm->vecs[1][i]);
		}
		
		// project the origin onto the plane
		d = DotProduct( origin, plane ) - plane[3];
		d /= plane[lm->axisNum];
		origin[lm->axisNum] -= d;
	}
	else VectorCopy( dv->point, origin );	// non axial lightmap projection (explicit xyz)
	
	// planar surfaces have precalculated lightmap vectors for nudging
	if( lm->plane != NULL )
	{
		VectorCopy( lm->vecs[0], vecs[0] );
		VectorCopy( lm->vecs[1], vecs[1] );
		VectorCopy( lm->plane, vecs[2] );
	}
	else
	{
		// non-planar surfaces must calculate them
		if( plane != NULL ) VectorCopy( plane, vecs[2] );
		else VectorCopy( dv->normal, vecs[2] );
		VectorVectors( vecs[2], vecs[0], vecs[1] );
	}
	
	// push the origin off the surface a bit
	if( si != NULL ) lightmapSampleOffset = si->lightmapSampleOffset;
	else lightmapSampleOffset = DEFAULT_LIGHTMAP_SAMPLE_OFFSET;

	if( lm->axisNum < 0 )
		VectorMA( origin, lightmapSampleOffset, vecs[2], origin );
	else if( vecs[2][lm->axisNum] < 0.0f )
		origin[lm->axisNum] -= lightmapSampleOffset;
	else origin[lm->axisNum] += lightmapSampleOffset;
	
	// get cluster
	pointCluster = ClusterForPointExtFilter( origin, LUXEL_EPSILON, numClusters, clusters );
	
	// another retarded hack, storing nudge count in luxel[1]
	luxel[1] = 0.0f;	
	
	// point in solid? (except in dark mode)
	if( pointCluster < 0 && dark == false )
	{
		// nudge the the location around
		nudge = nudges[0];
		while( nudge[0] > BOGUS_NUDGE && pointCluster < 0 )
		{
			// nudge the vector around a bit
			for( i = 0; i < 3; i++ )
			{
				// set nudged point
				nudged[i] = origin[i] + (nudge[0] * vecs[0][i]) + (nudge[1] * vecs[1][i]);
			}
			nudge += 2;
			
			// get pvs cluster
			pointCluster = ClusterForPointExtFilter( nudged, LUXEL_EPSILON, numClusters, clusters );
			if( pointCluster >= 0 ) VectorCopy( nudged, origin );
			luxel[1] += 1.0f;
		}
	}
	
	// as a last resort, if still in solid, try drawvert origin offset by normal (except in dark mode)
	if( pointCluster < 0 && si != NULL && dark == false )
	{
		VectorMA( dv->point, lightmapSampleOffset, dv->normal, nudged );
		pointCluster = ClusterForPointExtFilter( nudged, LUXEL_EPSILON, numClusters, clusters );
		if( pointCluster >= 0 ) VectorCopy( nudged, origin );
		luxel[1] += 1.0f;
	}
	
	if( pointCluster < 0 )
	{
		(*cluster) = CLUSTER_OCCLUDED;
		VectorClear( origin );
		VectorClear( normal );
		numLuxelsOccluded++;
		return (*cluster);
	}
	
	// do bumpmap calculations
	if( stv ) PerturbNormal( dv, si, pNormal, stv, ttv );
	else VectorCopy( dv->normal, pNormal );
	
	// store the cluster and normal
	(*cluster) = pointCluster;
	VectorCopy( pNormal, normal );
	
	// store explicit mapping pass and implicit mapping pass
	luxel[0] = pass;
	luxel[3] = 1.0f;
	
	numLuxelsMapped++;
	
	/* return ok */
	return (*cluster);
}


/*
=============
MapTriangle_r

recursively subdivides a triangle until its edges are shorter
than the distance between two luxels (thanks jc :)
=============
*/
static void MapTriangle_r( rawLightmap_t *lm, surfaceInfo_t *info, dvertex_t *dv[3], vec4_t plane, vec3_t stv[3], vec3_t ttv[3] )
{
	dvertex_t	mid, *dv2[3];
	int	i, max;
	float	*a, *b, dx, dy, dist, maxDist;	
	
	// find the longest edge and split it
	max = -1;
	maxDist = 0;

	for( i = 0; i < 3; i++ )
	{
		a = dv[i]->lm[0];
		b = dv[ (i + 1) % 3 ]->lm[0];
			
		dx = a[0] - b[0];
		dy = a[1] - b[1];
		dist = (dx * dx) + (dy * dy);
			
		if( dist > maxDist )
		{
			maxDist = dist;
			max = i;
		}
	}
		
	if( max < 0 || maxDist <= DEFAULT_SUBDIVIDE_THRESHOLD )
		return;
	
	// split the longest edge and map it
	LerpDrawVert( dv[max], dv[(max + 1)%3], &mid );
	MapSingleLuxel( lm, info, &mid, plane, 1, stv, ttv );
	
	VectorCopy( dv, dv2 );
	dv2[max] = &mid;
	MapTriangle_r( lm, info, dv2, plane, stv, ttv );
	
	VectorCopy( dv, dv2 );
	dv2[(max + 1)%3] = &mid;
	MapTriangle_r( lm, info, dv2, plane, stv, ttv );
}



/*
=============
MapTriangle

seed function for MapTriangle_r
requires a cw ordered triangle
=============
*/
static bool MapTriangle( rawLightmap_t *lm, surfaceInfo_t *info, dvertex_t *dv[3], bool mapNonAxial )
{
	int	i;
	vec4_t	plane;
	vec3_t	*stv, *ttv, stvStatic[3], ttvStatic[3];
	
	if( lm->plane != NULL )
	{
		VectorCopy( lm->plane, plane );
		plane[3] = lm->plane[3];
	}
	else if( !PlaneFromPoints( plane, dv[0]->point, dv[1]->point, dv[2]->point ))
		return false;
	
	// check to see if we need to calculate texture->world tangent vectors
	if( info->si->normalImage != NULL && CalcTangentVectors( 3, dv, stvStatic, ttvStatic ))
	{
		stv = stvStatic;
		ttv = ttvStatic;
	}
	else
	{
		stv = NULL;
		ttv = NULL;
	}
	
	MapSingleLuxel( lm, info, dv[0], plane, 1, stv, ttv );
	MapSingleLuxel( lm, info, dv[1], plane, 1, stv, ttv );
	MapSingleLuxel( lm, info, dv[2], plane, 1, stv, ttv );
	
	// prefer axial triangle edges
	if( mapNonAxial )
	{
		MapTriangle_r( lm, info, dv, plane, stv, ttv );
		return true;
	}
	
	for( i = 0; i < 3; i++ )
	{
		float	*a, *b;
		dvertex_t	*dv2[3];
		
		a = dv[i]->lm[0];
		b = dv[ (i + 1) % 3 ]->lm[0];
		
		// make degenerate triangles for mapping edges
		if( fabs( a[0] - b[0] ) < 0.01f || fabs( a[1] - b[1] ) < 0.01f )
		{
			dv2[0] = dv[i];
			dv2[1] = dv[(i+1)%3];
			dv2[2] = dv[(i+1)%3];
			MapTriangle_r( lm, info, dv2, plane, stv, ttv );
		}
	}
	return true;
}

/*
=============
MapQuad_r

recursively subdivides a quad until its edges are shorter
than the distance between two luxels
=============
*/
static void MapQuad_r( rawLightmap_t *lm, surfaceInfo_t *info, dvertex_t *dv[4], vec4_t plane, vec3_t stv[4], vec3_t ttv[4] )
{
	dvertex_t	mid[2], *dv2[4];
	int	i, max;
	float	*a, *b, dx, dy, dist, maxDist;
		
		
	// find the longest edge and split it
	max = -1;
	maxDist = 0;

	for( i = 0; i < 4; i++ )
	{
		a = dv[i]->lm[0];
		b = dv[(i + 1)%4]->lm[0];
			
		dx = a[0] - b[0];
		dy = a[1] - b[1];
		dist = (dx * dx) + (dy * dy);
		
		if( dist > maxDist )
		{
			maxDist = dist;
			max = i;
		}
	}
		
	if( max < 0 || maxDist <= DEFAULT_SUBDIVIDE_THRESHOLD )
		return;
	
	// we only care about even/odd edges
	max &= 1;
	
	LerpDrawVert( dv[max], dv[(max+1)%4], &mid[0] );
	LerpDrawVert( dv[max+2], dv[(max+3)%4], &mid[1] );
	
	MapSingleLuxel( lm, info, &mid[0], plane, 1, stv, ttv );
	MapSingleLuxel( lm, info, &mid[1], plane, 1, stv, ttv );
	
	if( max == 0 )
	{
		dv2[0] = dv[0];
		dv2[1] = &mid[0];
		dv2[2] = &mid[1];
		dv2[3] = dv[3];
		MapQuad_r( lm, info, dv2, plane, stv, ttv );
		
		dv2[0] = &mid[0];
		dv2[1] = dv[1];
		dv2[2] = dv[2];
		dv2[3] = &mid[1];
		MapQuad_r( lm, info, dv2, plane, stv, ttv );
	}
	else
	{
		dv2[0] = dv[0];
		dv2[1] = dv[1];
		dv2[2] = &mid[0];
		dv2[3] = &mid[1];
		MapQuad_r( lm, info, dv2, plane, stv, ttv );
		
		dv2[0] = &mid[1];
		dv2[1] = &mid[0];
		dv2[2] = dv[2];
		dv2[3] = dv[3];
		MapQuad_r( lm, info, dv2, plane, stv, ttv );
	}
}



/*
=============
MapQuad

seed function for MapQuad_r
requires a cw ordered triangle quad
=============
*/
static bool MapQuad( rawLightmap_t *lm, surfaceInfo_t *info, dvertex_t *dv[4] )
{
	float	dist;
	vec4_t	plane;
	vec3_t	*stv, *ttv, stvStatic[4], ttvStatic[4];
	
	
	// get plane if possible
	if( lm->plane != NULL )
	{
		VectorCopy( lm->plane, plane );
		plane[3] = lm->plane[3];
	}
	else if( !PlaneFromPoints( plane, dv[0]->point, dv[1]->point, dv[2]->point ))
		return false;
	
	// 4th point must fall on the plane
	dist = DotProduct( plane, dv[3]->point ) - plane[3];
	if( fabs( dist ) > QUAD_PLANAR_EPSILON )
		return false;
	
	// check to see if we need to calculate texture->world tangent vectors
	if( info->si->normalImage != NULL && CalcTangentVectors( 4, dv, stvStatic, ttvStatic ))
	{
		stv = stvStatic;
		ttv = ttvStatic;
	}
	else
	{
		stv = NULL;
		ttv = NULL;
	}
	
	MapSingleLuxel( lm, info, dv[0], plane, 1, stv, ttv );
	MapSingleLuxel( lm, info, dv[1], plane, 1, stv, ttv );
	MapSingleLuxel( lm, info, dv[2], plane, 1, stv, ttv );
	MapSingleLuxel( lm, info, dv[3], plane, 1, stv, ttv );
	
	MapQuad_r( lm, info, dv, plane, stv, ttv );
	return true;
}


/*
=============
MapRawLightmap

maps the locations, normals, and pvs clusters for a raw lightmap
=============
*/
void MapRawLightmap( int rawLightmapNum )
{
	int		n, num, i, x, y, sx, sy, pw[5], r, *cluster, mapNonAxial;
	float		*luxel, *origin, *normal, samples, radius, pass;
	rawLightmap_t	*lm;
	dsurface_t	*ds;
	surfaceInfo_t	*info;
	bsp_mesh_t	src, *subdivided, *mesh;
	dvertex_t		*verts, *dv[4], fake;
	
	
	/* bail if this number exceeds the number of raw lightmaps */
	if( rawLightmapNum >= numRawLightmaps )
		return;
	
	lm = &rawLightmaps[rawLightmapNum];
	
	/*
	-----------------------------------------------------------------
	map referenced surfaces onto the raw lightmap
	-----------------------------------------------------------------
	*/
	
	for( n = 0; n < lm->numLightSurfaces; n++ )
	{
		// with > 1 surface per raw lightmap, clear occluded
		if( n > 0 )
		{
			for( y = 0; y < lm->sh; y++ )
			{
				for( x = 0; x < lm->sw; x++ )
				{
					cluster = SUPER_CLUSTER( x, y );
					if( *cluster < 0 ) *cluster = CLUSTER_UNMAPPED;
				}
			}
		}
		
		num = lightSurfaces[lm->firstLightSurface+n];
		ds = &dsurfaces[num];
		info = &surfaceInfos[num];
		
		// bail if no lightmap to calculate
		if( info->lm != lm )
		{
			MsgDev( D_INFO, "!" );
			continue;
		}
		
		// map the surface onto the lightmap origin/cluster/normal buffers
		switch( ds->surfaceType )
		{
		case MST_PLANAR:
			verts = yDrawVerts + ds->firstvertex;
			for( mapNonAxial = 0; mapNonAxial < 2; mapNonAxial++ )
			{
				for( i = 0; i < ds->numindices; i += 3 )
				{
					dv[0] = &verts[ dindexes[ds->firstindex+i+0]];
					dv[1] = &verts[ dindexes[ds->firstindex+i+1]];
					dv[2] = &verts[ dindexes[ds->firstindex+i+2]];
					MapTriangle( lm, info, dv, mapNonAxial );
				}
			}
			break;
		case MST_PATCH:
			src.width = ds->patch.width;
			src.height = ds->patch.height;
			src.verts = &yDrawVerts[ds->firstvertex];
			subdivided = SubdivideMesh2( src, info->patchIterations );
				
			// fit it to the curve and remove colinear verts on rows/columns
			PutMeshOnCurve( *subdivided );
			mesh = RemoveLinearMeshColumnsRows( subdivided );
			FreeMesh( subdivided );
			verts = mesh->verts;
				
			for( y = 0; y < (mesh->height - 1); y++ )
			{
				for( x = 0; x < (mesh->width - 1); x++ )
				{
					pw[0] = x + (y * mesh->width);
					pw[1] = x + ((y + 1) * mesh->width);
					pw[2] = x + 1 + ((y + 1) * mesh->width);
					pw[3] = x + 1 + (y * mesh->width);
					pw[4] = pw[0];
						
					r = (x + y) & 1;
						
					dv[0] = &verts[pw[r+0]];
					dv[1] = &verts[pw[r+1]];
					dv[2] = &verts[pw[r+2]];
					dv[3] = &verts[pw[r+3]];
					if( MapQuad( lm, info, dv ) )
						continue;
						
					MapTriangle( lm, info, dv, mapNonAxial );
						
					dv[1] = &verts[pw[r+2]];
					dv[2] = &verts[pw[r+3]];
					MapTriangle( lm, info, dv, mapNonAxial );
				}
			}
			FreeMesh( mesh );
			break;
		default: break;
		}
	}
	
	/*
	-----------------------------------------------------------------
	   average and clean up luxel normals
	-----------------------------------------------------------------
	*/
	
	for( y = 0; y < lm->sh; y++ )
	{
		for( x = 0; x < lm->sw; x++ )
		{
			luxel = SUPER_LUXEL( 0, x, y );
			normal = SUPER_NORMAL( x, y );
			cluster = SUPER_CLUSTER( x, y );

			if( *cluster < 0 ) continue;
			
			// the normal data could be the sum of multiple samples
			if( luxel[3] > 1.0f ) VectorNormalize( normal );
			
			// mark this luxel as having only one normal
			luxel[3] = 1.0f;
		}
	}
	
	// non-planar surfaces stop here
	if( lm->plane == NULL ) return;
	
	/*
	-----------------------------------------------------------------
	   map occluded or unuxed luxels
	-----------------------------------------------------------------
	*/
	
	radius = floor( superSample / 2 );
	radius = radius > 0 ? radius : 1.0f;
	radius += 1.0f;

	for( pass = 2.0f; pass <= radius; pass += 1.0f )
	{
		for( y = 0; y < lm->sh; y++ )
		{
			for( x = 0; x < lm->sw; x++ )
			{
				luxel = SUPER_LUXEL( 0, x, y );
				normal = SUPER_NORMAL( x, y );
				cluster = SUPER_CLUSTER( x, y );
				
				if( *cluster != CLUSTER_UNMAPPED )
					continue;
				
				// divine a normal and origin from neighboring luxels
				VectorClear( fake.point );
				VectorClear( fake.normal );
				fake.lm[0][0] = x;
				fake.lm[0][1] = y;
				samples = 0.0f;

				for( sy = (y - 1); sy <= (y + 1); sy++ )
				{
					if( sy < 0 || sy >= lm->sh )
						continue;
					
					for( sx = (x - 1); sx <= (x + 1); sx++ )
					{
						if( sx < 0 || sx >= lm->sw || (sx == x && sy == y) )
							continue;
						
						// get neighboring luxel
						luxel = SUPER_LUXEL( 0, sx, sy );
						origin = SUPER_ORIGIN( sx, sy );
						normal = SUPER_NORMAL( sx, sy );
						cluster = SUPER_CLUSTER( sx, sy );
						
						// only consider luxels mapped in previous passes
						if( *cluster < 0 || luxel[0] >= pass )
							continue;
						
						// add its distinctiveness to our own
						VectorAdd( fake.point, origin, fake.point );
						VectorAdd( fake.normal, normal, fake.normal );
						samples += luxel[3];
					}
				}
				
				if( samples == 0.0f ) continue;
				
				VectorDivide( fake.point, samples, fake.point );
				if( VectorNormalizeLength( fake.normal ) == 0.0f )
					continue;
				
				// map the fake vert
				MapSingleLuxel( lm, NULL, &fake, lm->plane, pass, NULL, NULL );
			}
		}
	}
	
	/*
	-----------------------------------------------------------------
	   average and clean up luxel normals
	-----------------------------------------------------------------
	*/
	
	for( y = 0; y < lm->sh; y++ )
	{
		for( x = 0; x < lm->sw; x++ )
		{
			luxel = SUPER_LUXEL( 0, x, y );
			normal = SUPER_NORMAL( x, y );
			cluster = SUPER_CLUSTER( x, y );
			
			if( *cluster < 0 ) continue;
			
			// the normal data could be the sum of multiple samples
			if( luxel[3] > 1.0f ) VectorNormalize( normal );
			
			// mark this luxel as having only one normal
			luxel[3] = 1.0f;
		}
	}
}



/*
=============
SetupDirt

sets up dirtmap (ambient occlusion)
=============
*/
void SetupDirt( void )
{
	int	i, j;
	float	angle, elevation, angleStep, elevationStep;

	MsgDev( D_NOTE, "--- SetupDirt ---\n" );
	
	angleStep = DEG2RAD( 360.0f / DIRT_NUM_ANGLE_STEPS );
	elevationStep = DEG2RAD( DIRT_CONE_ANGLE / DIRT_NUM_ELEVATION_STEPS );
	
	angle = 0.0f;
	for( i = 0, angle = 0.0f; i < DIRT_NUM_ANGLE_STEPS; i++, angle += angleStep )
	{
		for( j = 0, elevation = elevationStep * 0.5f; j < DIRT_NUM_ELEVATION_STEPS; j++, elevation += elevationStep )
		{
			dirtVectors[numDirtVectors][0] = sin( elevation ) * cos( angle );
			dirtVectors[numDirtVectors][1] = sin( elevation ) * sin( angle );
			dirtVectors[numDirtVectors][2] = cos( elevation );
			numDirtVectors++;
		}
	}
	
	MsgDev( D_NOTE, "%6i dirtmap vectors\n", numDirtVectors );
}


/*
=============
DirtForSample

calculates dirt value for a given sample
=============
*/
float DirtForSample( lighttrace_t *trace )
{
	int	i;
	float	gatherDirt, outDirt, angle, elevation, ooDepth;
	vec3_t	normal, worldUp, myUp, myRt, temp, direction, displacement;
	
	if( !dirty ) return 1.0f;
	if( trace == NULL || trace->cluster < 0 )
		return 0.0f;
	
	gatherDirt = 0.0f;
	ooDepth = 1.0f / dirtDepth;
	VectorCopy( trace->normal, normal );
	
	// check if the normal is aligned to the world-up
	if( normal[0] == 0.0f && normal[1] == 0.0f )
	{
		if( normal[2] == 1.0f )		
		{
			VectorSet( myRt, 1.0f, 0.0f, 0.0f );
			VectorSet( myUp, 0.0f, 1.0f, 0.0f );
		}
		else if( normal[2] == -1.0f )
		{
			VectorSet( myRt, -1.0f, 0.0f, 0.0f );
			VectorSet( myUp,  0.0f, 1.0f, 0.0f );
		}
	}
	else
	{
		VectorSet( worldUp, 0.0f, 0.0f, 1.0f );
		CrossProduct( normal, worldUp, myRt );
		VectorNormalize( myRt );
		CrossProduct( myRt, normal, myUp );
		VectorNormalize( myUp );
	}
	
	// 1 = random mode, 0 (well everything else) = non-random mode
	if( dirtMode == 1 )
	{
		for( i = 0; i < numDirtVectors; i++ )
		{
			angle = RANDOM_FLOAT( 0, 1 ) * DEG2RAD( 360.0f );
			elevation = RANDOM_FLOAT( 0, 1 ) * DEG2RAD( DIRT_CONE_ANGLE );
			temp[0] = cos( angle ) * sin( elevation );
			temp[1] = sin( angle ) * sin( elevation );
			temp[2] = cos( elevation );
			
			direction[0] = myRt[0] * temp[0] + myUp[0] * temp[1] + normal[0] * temp[2];
			direction[1] = myRt[1] * temp[0] + myUp[1] * temp[1] + normal[1] * temp[2];
			direction[2] = myRt[2] * temp[0] + myUp[2] * temp[1] + normal[2] * temp[2];
			
			VectorMA( trace->origin, dirtDepth, direction, trace->end );
			SetupTrace( trace );
			
			TraceLine( trace );
			if( trace->opaque )
			{
				VectorSubtract( trace->hit, trace->origin, displacement );
				gatherDirt += 1.0f - ooDepth * VectorLength( displacement );
			}
		}
	}
	else
	{
		for( i = 0; i < numDirtVectors; i++ )
		{
			direction[0] = myRt[0] * dirtVectors[i][0] + myUp[0] * dirtVectors[i][1] + normal[0] * dirtVectors[i][2];
			direction[1] = myRt[1] * dirtVectors[i][0] + myUp[1] * dirtVectors[i][1] + normal[1] * dirtVectors[i][2];
			direction[2] = myRt[2] * dirtVectors[i][0] + myUp[2] * dirtVectors[i][1] + normal[2] * dirtVectors[i][2];
			
			VectorMA( trace->origin, dirtDepth, direction, trace->end );
			SetupTrace( trace );
			
			TraceLine( trace );
			if( trace->opaque )
			{
				VectorSubtract( trace->hit, trace->origin, displacement );
				gatherDirt += 1.0f - ooDepth * VectorLength( displacement );
			}
		}
	}
	
	// direct ray
	VectorMA( trace->origin, dirtDepth, normal, trace->end );
	SetupTrace( trace );
	
	TraceLine( trace );
	if( trace->opaque )
	{
		VectorSubtract( trace->hit, trace->origin, displacement );
		gatherDirt += 1.0f - ooDepth * VectorLength( displacement );
	}
	
	if( gatherDirt <= 0.0f ) return 1.0f;
	
	// apply gain (does this even do much? heh)
	outDirt = pow( gatherDirt / (numDirtVectors + 1), dirtGain );
	if( outDirt > 1.0f ) outDirt = 1.0f;
	
	outDirt *= dirtScale;
	if( outDirt > 1.0f )
		outDirt = 1.0f;
	
	return 1.0f - outDirt;
}


/*
=============
DirtyRawLightmap

calculates dirty fraction for each luxel
=============
*/
void DirtyRawLightmap( int rawLightmapNum )
{
	int		i, x, y, sx, sy, *cluster;
	float		*origin, *normal, *dirt, *dirt2, average, samples;
	rawLightmap_t	*lm;
	surfaceInfo_t	*info;
	lighttrace_t	trace;
	
	
	// bail if this number exceeds the number of raw lightmaps
	if( rawLightmapNum >= numRawLightmaps )
		return;
	
	lm = &rawLightmaps[rawLightmapNum];
	
	/* setup trace */
	trace.testOcclusion = true;
	trace.forceSunlight = false;
	trace.recvShadows = lm->recvShadows;
	trace.numSurfaces = lm->numLightSurfaces;
	trace.surfaces = &lightSurfaces[ lm->firstLightSurface ];
	trace.inhibitRadius = DEFAULT_INHIBIT_RADIUS;
	trace.testAll = false;
	
	// twosided lighting (may or may not be a good idea for lightmapped stuff)
	trace.twoSided = false;
	for( i = 0; i < trace.numSurfaces; i++ )
	{
		info = &surfaceInfos[ trace.surfaces[i] ];
		
		if( info->si->twoSided )
		{
			trace.twoSided = true;
			break;
		}
	}
	
	for( y = 0; y < lm->sh; y++ )
	{
		for( x = 0; x < lm->sw; x++ )
		{
			cluster = SUPER_CLUSTER( x, y );
			origin = SUPER_ORIGIN( x, y );
			normal = SUPER_NORMAL( x, y );
			dirt = SUPER_DIRT( x, y );
			
			*dirt = 0.0f;
			
			if( *cluster < 0 ) continue;
			
			trace.cluster = *cluster;
			VectorCopy( origin, trace.origin );
			VectorCopy( normal, trace.normal );
			*dirt = DirtForSample( &trace );
		}
	}
	
	// filter dirt
	for( y = 0; y < lm->sh; y++ )
	{
		for( x = 0; x < lm->sw; x++ )
		{
			cluster = SUPER_CLUSTER( x, y );
			dirt = SUPER_DIRT( x, y );
			
			// filter dirt by adjacency to unmapped luxels
			average = *dirt;
			samples = 1.0f;
			for( sy = (y - 1); sy <= (y + 1); sy++ )
			{
				if( sy < 0 || sy >= lm->sh )
					continue;
				
				for( sx = (x - 1); sx <= (x + 1); sx++ )
				{
					if( sx < 0 || sx >= lm->sw || (sx == x && sy == y) )
						continue;
					
					// get neighboring luxel
					cluster = SUPER_CLUSTER( sx, sy );
					dirt2 = SUPER_DIRT( sx, sy );
					if( *cluster < 0 || *dirt2 <= 0.0f )
						continue;
					
					average += *dirt2;
					samples += 1.0f;
				}
				if( samples <= 0.0f ) break;
			}

			if( samples <= 0.0f ) continue;
			*dirt = average / samples;
		}
	}
}



/*
=============
SubmapRawLuxel

calculates the pvs cluster, origin, normal of a sub-luxel
=============
*/
static bool SubmapRawLuxel( rawLightmap_t *lm, int x, int y, float bx, float by, int *sampleCluster, vec3_t sampleOrigin, vec3_t sampleNormal )
{
	int	i, *cluster, *cluster2;
	float	*origin, *origin2, *normal;
	vec3_t	originVecs[2];
	
	if((x < (lm->sw - 1) && bx >= 0.0f) || (x == 0 && bx <= 0.0f))
	{
		cluster = SUPER_CLUSTER( x, y );
		origin = SUPER_ORIGIN( x, y );
		cluster2 = SUPER_CLUSTER( x + 1, y );
		origin2 = *cluster2 < 0 ? SUPER_ORIGIN( x, y ) : SUPER_ORIGIN( x + 1, y );
	}
	else if((x > 0 && bx <= 0.0f) || (x == (lm->sw - 1) && bx >= 0.0f))
	{
		cluster = SUPER_CLUSTER( x - 1, y );
		origin = *cluster < 0 ? SUPER_ORIGIN( x, y ) : SUPER_ORIGIN( x - 1, y );
		cluster2 = SUPER_CLUSTER( x, y );
		origin2 = SUPER_ORIGIN( x, y );
	}
	else MsgDev( D_WARN, "Spurious lightmap S vector\n" );
	
	VectorSubtract( origin2, origin, originVecs[0] );
	
	if((y < (lm->sh - 1) && bx >= 0.0f) || (y == 0 && bx <= 0.0f))
	{
		cluster = SUPER_CLUSTER( x, y );
		origin = SUPER_ORIGIN( x, y );
		cluster2 = SUPER_CLUSTER( x, y + 1 );
		origin2 = *cluster2 < 0 ? SUPER_ORIGIN( x, y ) : SUPER_ORIGIN( x, y + 1 );
	}
	else if((y > 0 && bx <= 0.0f) || (y == (lm->sh - 1) && bx >= 0.0f))
	{
		cluster = SUPER_CLUSTER( x, y - 1 );
		origin = *cluster < 0 ? SUPER_ORIGIN( x, y ) : SUPER_ORIGIN( x, y - 1 );
		cluster2 = SUPER_CLUSTER( x, y );
		origin2 = SUPER_ORIGIN( x, y );
	}
	else MsgDev( D_WARN, "Spurious lightmap T vector\n" );
	
	VectorSubtract( origin2, origin, originVecs[1] );
	
	for( i = 0; i < 3; i++ )
		sampleOrigin[i] = sampleOrigin[i] + (bx * originVecs[0][i]) + (by * originVecs[1][i]);
	
	// get cluster
	*sampleCluster = ClusterForPointExtFilter( sampleOrigin, (LUXEL_EPSILON * 2), lm->numLightClusters, lm->lightClusters );
	if( *sampleCluster < 0 ) return false;
	
	// calculate new normal
	normal = SUPER_NORMAL( x, y );
	VectorCopy( normal, sampleNormal );
	
	return true;
}


/*
=============
SubsampleRawLuxel_r

recursively subsamples a luxel until its color gradient is low enough or subsampling limit is reached
=============
*/
static void SubsampleRawLuxel_r( rawLightmap_t *lm, lighttrace_t *trace, vec3_t sampleOrigin, int x, int y, float bias, float *lightLuxel )
{
	int	b, samples, mapped, lighted;
	int	cluster[4];
	vec4_t	luxel[4];
	vec3_t	origin[4], normal[4];
	float	biasDirs[4][2] = {{ -1.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 1.0f }, { 1.0f, 1.0f }};
	vec3_t	color, total;
	
	if( lightLuxel[3] >= lightSamples )
		return;
	
	VectorClear( total );
	mapped = 0;
	lighted = 0;
	
	// make 2x2 subsample stamp
	for( b = 0; b < 4; b++ )
	{
		VectorCopy( sampleOrigin, origin[b] );
		
		if( !SubmapRawLuxel( lm, x, y, (bias * biasDirs[b][0]), (bias * biasDirs[b][1]), &cluster[b], origin[b], normal[b] ))
		{
			cluster[b] = -1;
			continue;
		}
		mapped++;
		
		luxel[b][3] = lightLuxel[3] + 1.0f;
		
		trace->cluster = *cluster;
		VectorCopy( origin[b], trace->origin );
		VectorCopy( normal[b], trace->normal );
		
		LightContributionToSample( trace );
		
		// add to totals (FIXME: make contrast function)
		VectorCopy( trace->color, luxel[b] );
		VectorAdd( total, trace->color, total );
		if( (luxel[b][0] + luxel[b][1] + luxel[b][2]) > 0.0f )
			lighted++;
	}
	
	// subsample further?
	if( (lightLuxel[3] + 1.0f) < lightSamples && (total[0] > 4.0f || total[1] > 4.0f || total[2] > 4.0f) && lighted != 0 && lighted != mapped )
	{
		for( b = 0; b < 4; b++ )
		{
			if( cluster[b] < 0 ) continue;
			SubsampleRawLuxel_r( lm, trace, origin[b], x, y, (bias * 0.25f), luxel[b] );
		}
	}
	
	// average
	VectorCopy( lightLuxel, color );
	samples = 1;
	for( b = 0; b < 4; b++ )
	{
		if( cluster[b] < 0 )
			continue;
		VectorAdd( color, luxel[b], color );
		samples++;
	}
	
	// add to luxel
	if( samples > 0 )
	{
		color[0] /= samples;
		color[1] /= samples;
		color[2] /= samples;
		
		VectorCopy( color, lightLuxel );
		lightLuxel[3] += 1.0f;
	}
}



/*
=============
IlluminateRawLightmap

illuminates the luxels
=============
*/
void IlluminateRawLightmap( int rawLightmapNum )
{
	int		i, t, x, y, sx, sy, size, llSize, luxelFilterRadius, lightmapNum;
	int		*cluster, *cluster2, mapped, lighted, totalLighted;
	rawLightmap_t	*lm;
	surfaceInfo_t	*info;
	bool		filterColor, filterDir;
	float		brightness;
	float		*origin, *normal, *dirt, *luxel, *luxel2, *deluxel, *deluxel2;
	float		*lightLuxels, *lightLuxel, samples, filterRadius, weight;
	vec3_t		color, averageColor, averageDir, total, temp, temp2;
	float		tests[4][2] = { { 0.0f, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
	lighttrace_t	trace;
	float		stackLightLuxels[STACK_LL_SIZE];
	
	
	// bail if this number exceeds the number of raw lightmaps
	if( rawLightmapNum >= numRawLightmaps )
		return;
	
	lm = &rawLightmaps[rawLightmapNum];
	
	trace.testOcclusion = !notrace;
	trace.forceSunlight = false;
	trace.recvShadows = lm->recvShadows;
	trace.numSurfaces = lm->numLightSurfaces;
	trace.surfaces = &lightSurfaces[lm->firstLightSurface];
	trace.inhibitRadius = DEFAULT_INHIBIT_RADIUS;
	
	// twosided lighting (may or may not be a good idea for lightmapped stuff)
	trace.twoSided = false;
	for( i = 0; i < trace.numSurfaces; i++ )
	{
		info = &surfaceInfos[ trace.surfaces[i] ];
		
		if( info->si->twoSided )
		{
			trace.twoSided = true;
			break;
		}
	}
	
	// create a culled light list for this raw lightmap
	CreateTraceLightsForBounds( lm->mins, lm->maxs, lm->plane, lm->numLightClusters, lm->lightClusters, LIGHT_SURFACES, &trace );
	
	/*
	-----------------------------------------------------------------
	   fill pass
	-----------------------------------------------------------------
	*/
	
	numLuxelsIlluminated += (lm->sw * lm->sh);
	
	if( debugSurfaces || debugAxis || debugCluster || debugOrigin || dirtDebug || normalmap )
	{
		for( y = 0; y < lm->sh; y++ )
		{
			for( x = 0; x < lm->sw; x++ )
			{
				cluster = SUPER_CLUSTER( x, y );

				if( *cluster < 0 ) continue;
				
				luxel = SUPER_LUXEL( 0, x, y );
				origin = SUPER_ORIGIN( x, y );
				normal = SUPER_NORMAL( x, y );
				
				// color the luxel with raw lightmap num?
				if( debugSurfaces ) VectorCopy( debugColors[rawLightmapNum%12], luxel );
				
				// color the luxel with lightmap axis?
				else if( debugAxis )
				{
					luxel[0] = (lm->axis[0] + 1.0f) * 127.5f;
					luxel[1] = (lm->axis[1] + 1.0f) * 127.5f;
					luxel[2] = (lm->axis[2] + 1.0f) * 127.5f;
				}
				else if( debugCluster )
					VectorCopy( debugColors[ *cluster % 12 ], luxel );
				else if( debugOrigin )
				{
					VectorSubtract( lm->maxs, lm->mins, temp );
					VectorScale( temp, (1.0f / 255.0f), temp );
					VectorSubtract( origin, lm->mins, temp2 );
					luxel[0] = lm->mins[0] + (temp[0] * temp2[0]);
					luxel[1] = lm->mins[1] + (temp[1] * temp2[1]);
					luxel[2] = lm->mins[2] + (temp[2] * temp2[2]);
				}
				else if( normalmap )
				{
					luxel[0] = (normal[0] + 1.0f) * 127.5f;
					luxel[1] = (normal[1] + 1.0f) * 127.5f;
					luxel[2] = (normal[2] + 1.0f) * 127.5f;
				}
				else VectorClear( luxel );
				luxel[3] = 1.0f;
			}
		}
	}
	else
	{
		// allocate temporary per-light luxel storage
		llSize = lm->sw * lm->sh * SUPER_LUXEL_SIZE * sizeof( float );
		if( llSize <= (STACK_LL_SIZE * sizeof( float )) )
			lightLuxels = stackLightLuxels;
		else lightLuxels = BSP_Malloc( llSize );
	
		for( y = 0; y < lm->sh; y++ )
		{
			for( x = 0; x < lm->sw; x++ )
			{
				cluster = SUPER_CLUSTER( x, y );
				luxel = SUPER_LUXEL( 0, x, y );
				normal = SUPER_NORMAL( x, y );
				deluxel = SUPER_DELUXEL( x, y );
				
				if( *cluster < 0 ) VectorClear( luxel );
				else
				{
					VectorCopy( ambientColor, luxel );
					if( deluxemap ) VectorScale( normal, 0.00390625f, deluxel );
					luxel[3] = 1.0f;
				}
			}
		}
		
		size = lm->sw * lm->sh * SUPER_LUXEL_SIZE * sizeof( float );
		for( lightmapNum = 1; lightmapNum < LM_STYLES; lightmapNum++ )
		{
			if( lm->superLuxels[ lightmapNum ] != NULL )
				memset( lm->superLuxels[ lightmapNum ], 0, size );
		}

		// walk light list
		for( i = 0; i < trace.numLights; i++ )
		{
			trace.light = trace.lights[i];
			
			for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
			{
				if( lm->styles[lightmapNum] == trace.light->style || lm->styles[lightmapNum] == LS_NONE )
					break;
			}
			
			// max of LM_STYLES (4) styles allowed to hit a surface/lightmap
			if( lightmapNum >= LM_STYLES )
			{
				MsgDev( D_WARN, "Hit per-surface style limit (%d)\n", LM_STYLES );
				continue;
			}
			
			memset( lightLuxels, 0, llSize );
			totalLighted = 0;
			
			for( y = 0; y < lm->sh; y++ )
			{
				for( x = 0; x < lm->sw; x++ )
				{
					cluster = SUPER_CLUSTER( x, y );
					if( *cluster < 0 ) continue;
					
					lightLuxel = LIGHT_LUXEL( x, y );
					deluxel = SUPER_DELUXEL( x, y );
					origin = SUPER_ORIGIN( x, y );
					normal = SUPER_NORMAL( x, y );
					
					lightLuxel[3] = 1.0f;
					
					trace.cluster = *cluster;
					VectorCopy( origin, trace.origin );
					VectorCopy( normal, trace.normal );
					
					LightContributionToSample( &trace );
					VectorCopy( trace.color, lightLuxel );
					
					if( trace.color[0] || trace.color[1] || trace.color[2] )
						totalLighted++;
					
					// add to light direction map (FIXME: use luxel normal as starting point for deluxel?)
					if( deluxemap )
					{
						// color to grayscale (photoshop rgb weighting)
						brightness = trace.color[0] * 0.3f + trace.color[1] * 0.59f + trace.color[2] * 0.11f;
						brightness *= (1.0 / 255.0);
						VectorScale( trace.direction, brightness, trace.direction );
						VectorAdd( deluxel, trace.direction, deluxel );
					}
				}
			}
			
			// don't even bother with everything else if nothing was lit
			if( totalLighted == 0 )
				continue;
			
			// determine filter radius
			filterRadius = lm->filterRadius > trace.light->filterRadius ? lm->filterRadius : trace.light->filterRadius;
			if( filterRadius < 0.0f ) filterRadius = 0.0f;
			
			// set luxel filter radius
			luxelFilterRadius = superSample * filterRadius / lm->sampleSize;
			if( luxelFilterRadius == 0 && (filterRadius > 0.0f || filter ))
				luxelFilterRadius = 1;
			
			// secondary pass, adaptive supersampling (FIXME: use a contrast function to determine if subsampling is necessary)
			// 2003-09-27: changed it so filtering disamples supersampling, as it would waste time
			if( lightSamples > 1 && luxelFilterRadius == 0 )
			{
				for( y = 0; y < (lm->sh - 1); y++ )
				{
					for( x = 0; x < (lm->sw - 1); x++ )
					{
						mapped = 0;
						lighted = 0;
						VectorClear( total );
						
						// test 2x2 stamp
						for( t = 0; t < 4; t++ )
						{
							sx = x + tests[ t ][0];
							sy = y + tests[ t ][1];
							
							cluster = SUPER_CLUSTER( sx, sy );
							if( *cluster < 0 ) continue;
							mapped++;
							
							lightLuxel = LIGHT_LUXEL( sx, sy );
							VectorAdd( total, lightLuxel, total );
							if((lightLuxel[0] + lightLuxel[1] + lightLuxel[2]) > 0.0f )
								lighted++;
						}
						
						// if total color is under a certain amount, then don't bother subsampling
						if( total[0] <= 4.0f && total[1] <= 4.0f && total[2] <= 4.0f )
							continue;
						
						// if all 4 pixels are either in shadow or light, then don't subsample
						if( lighted != 0 && lighted != mapped )
						{
							for( t = 0; t < 4; t++ )
							{
								// set sample coords
								sx = x + tests[t][0];
								sy = y + tests[t][1];
								
								// get luxel
								cluster = SUPER_CLUSTER( sx, sy );
								if( *cluster < 0 ) continue;
								lightLuxel = LIGHT_LUXEL( sx, sy );
								origin = SUPER_ORIGIN( sx, sy );
								
								SubsampleRawLuxel_r( lm, &trace, origin, sx, sy, 0.25f, lightLuxel );
							}
						}
					}
				}
			}
			
			// tertiary pass, apply dirt map (ambient occlusion) */
			if( 0 && dirty )
			{
				for( y = 0; y < lm->sh; y++ )
				{
					for( x = 0; x < lm->sw; x++ )
					{
						cluster = SUPER_CLUSTER( x, y );
						if( *cluster < 0 ) continue;
						
						lightLuxel = LIGHT_LUXEL( x, y );
						dirt = SUPER_DIRT( x, y );
						
						VectorScale( lightLuxel, *dirt, lightLuxel );
					}
				}
			}
			
			// allocate sampling lightmap storage
			if( lm->superLuxels[lightmapNum] == NULL )
			{
				// allocate sampling lightmap storage
				size = lm->sw * lm->sh * SUPER_LUXEL_SIZE * sizeof( float );
				lm->superLuxels[lightmapNum] = BSP_Malloc( size );
			}
			
			// set style
			if( lightmapNum > 0 )
			{
				lm->styles[lightmapNum] = trace.light->style;
			}
			
			// copy to permanent luxels
			for( y = 0; y < lm->sh; y++ )
			{
				for( x = 0; x < lm->sw; x++ )
				{
					cluster = SUPER_CLUSTER( x, y );
					if( *cluster < 0 ) continue;
					origin = SUPER_ORIGIN( x, y );
					
					if( luxelFilterRadius )
					{
						VectorClear( averageColor );
						samples = 0.0f;
						
						// cheaper distance-based filtering
						for( sy = (y - luxelFilterRadius); sy <= (y + luxelFilterRadius); sy++ )
						{
							if( sy < 0 || sy >= lm->sh ) continue;
							
							for( sx = (x - luxelFilterRadius); sx <= (x + luxelFilterRadius); sx++ )
							{
								if( sx < 0 || sx >= lm->sw )
									continue;
								
								cluster = SUPER_CLUSTER( sx, sy );
								if( *cluster < 0 ) continue;
								lightLuxel = LIGHT_LUXEL( sx, sy );
								
								weight = (abs( sx - x ) == luxelFilterRadius ? 0.5f : 1.0f);
								weight *= (abs( sy - y ) == luxelFilterRadius ? 0.5f : 1.0f);
								
								VectorScale( lightLuxel, weight, color );
								VectorAdd( averageColor, color, averageColor );
								samples += weight;
							}
						}
						
						if( samples <= 0.0f	) continue;
						
						luxel = SUPER_LUXEL( lightmapNum, x, y );
						luxel[3] = 1.0f;
						
						if( trace.light->flags & LIGHT_NEGATIVE )
						{ 
							luxel[0] -= averageColor[0] / samples;
							luxel[1] -= averageColor[1] / samples;
							luxel[2] -= averageColor[2] / samples;
						}
						else
						{ 
							luxel[0] += averageColor[0] / samples;
							luxel[1] += averageColor[1] / samples;
							luxel[2] += averageColor[2] / samples;
						}
					}
					else
					{
						lightLuxel = LIGHT_LUXEL( x, y );
						luxel = SUPER_LUXEL( lightmapNum, x, y );
						
						if( trace.light->flags & LIGHT_NEGATIVE )
							VectorScale( averageColor, -1.0f, averageColor );

						luxel[3] = 1.0f;
						
						if( trace.light->flags & LIGHT_NEGATIVE )
							VectorSubtract( luxel, lightLuxel, luxel );
						else VectorAdd( luxel, lightLuxel, luxel );
					}
				}
			}
		}
		
		if( lightLuxels != stackLightLuxels )
			Mem_Free( lightLuxels );
	}
	
	FreeTraceLights( &trace );
	
	/*
	-----------------------------------------------------------------
	dirt pass
	-----------------------------------------------------------------
	*/
	
	if( dirty )
	{
		for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
		{
			if( lm->superLuxels[ lightmapNum ] == NULL )
				continue;
			
			for( y = 0; y < lm->sh; y++ )
			{
				for( x = 0; x < lm->sw; x++ )
				{
					cluster	= SUPER_CLUSTER( x, y );
					luxel = SUPER_LUXEL( lightmapNum, x, y );
					dirt = SUPER_DIRT( x, y );
					
					VectorScale( luxel, *dirt, luxel );
					
					if( dirtDebug ) VectorSet( luxel, *dirt * 255.0f, *dirt * 255.0f, *dirt * 255.0f );
				}
			}
		}
	}
	
	/*
	-----------------------------------------------------------------
	filter pass
	-----------------------------------------------------------------
	*/
	
	for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
	{
		if( lm->superLuxels[ lightmapNum ] == NULL )
			continue;
		
		// average occluded luxels from neighbors
		for( y = 0; y < lm->sh; y++ )
		{
			for( x = 0; x < lm->sw; x++ )
			{
				cluster = SUPER_CLUSTER( x, y );
				luxel = SUPER_LUXEL( lightmapNum, x, y );
				deluxel = SUPER_DELUXEL( x, y );
				normal = SUPER_NORMAL( x, y );
				
				filterColor = false;
				filterDir = false;
				if( *cluster < 0 || (lm->splotchFix && (luxel[0] <= ambientColor[0] || luxel[1] <= ambientColor[1] || luxel[2] <= ambientColor[2])) )
					filterColor = true;
				if( deluxemap && lightmapNum == 0 && (*cluster < 0 || filter) )
					filterDir = true;
				
				if( !filterColor && !filterDir )
					continue;
				
				// choose seed amount
				VectorClear( averageColor );
				VectorClear( averageDir );
				samples = 0.0f;
				
				// walk 3x3 matrix
				for( sy = (y - 1); sy <= (y + 1); sy++ )
				{
					if( sy < 0 || sy >= lm->sh )
						continue;
					
					for( sx = (x - 1); sx <= (x + 1); sx++ )
					{
						if( sx < 0 || sx >= lm->sw || (sx == x && sy == y) )
							continue;
						
						// get neighbor's particulars
						cluster2 = SUPER_CLUSTER( sx, sy );
						luxel2 = SUPER_LUXEL( lightmapNum, sx, sy );
						deluxel2 = SUPER_DELUXEL( sx, sy );
						
						// ignore unmapped/unlit luxels
						if( *cluster2 < 0 || luxel2[3] == 0.0f || (lm->splotchFix && VectorCompare( luxel2, ambientColor )))
							continue;
						
						// add its distinctiveness to our own
						VectorAdd( averageColor, luxel2, averageColor );
						samples += luxel2[3];
						if( filterDir ) VectorAdd( averageDir, deluxel2, averageDir );
					}
				}
				
				// fall through
				if( samples <= 0.0f ) continue;
				
				// dark lightmap seams
				if( dark )
				{
					if( lightmapNum == 0 )
						VectorMA( averageColor, 2.0f, ambientColor, averageColor );
					samples += 2.0f;
				}
				
				// average it
				if( filterColor )
				{
					VectorDivide( averageColor, samples, luxel );
					luxel[3] = 1.0f;
				}
				if( filterDir ) VectorDivide( averageDir, samples, deluxel );
				
				// set cluster to -3
				if( *cluster < 0 ) *cluster = CLUSTER_FLOODED;
			}
		}
	}
}



/*
=============
IlluminateVertexes

light the surface vertexes
=============
*/
void IlluminateVertexes( int num )
{
	int		i, x, y, z, x1, y1, z1, sx, sy, radius, maxRadius, *cluster;
	int		lightmapNum, numAvg;
	float		samples, *vertLuxel, *radVertLuxel, *luxel, dirt;
	vec3_t		origin, temp, temp2, colors[ LM_STYLES ], avgColors[ LM_STYLES ];
	dsurface_t	*ds;
	surfaceInfo_t	*info;
	rawLightmap_t	*lm;
	dvertex_t		*verts;
	lighttrace_t	trace;
	
	
	// get surface, info, and raw lightmap
	ds = &dsurfaces[num];
	info = &surfaceInfos[num];
	lm = info->lm;
	
	/*
	-----------------------------------------------------------------
	illuminate the vertexes
	-----------------------------------------------------------------
	*/
	
	// calculate vertex lighting for surfaces without lightmaps
	if( lm == NULL || cpmaHack )
	{
		/* setup trace */
		trace.testOcclusion = (cpmaHack && lm != NULL) ? false : !notrace;
		trace.forceSunlight = info->si->forceSunlight;
		trace.recvShadows = info->recvShadows;
		trace.numSurfaces = 1;
		trace.surfaces = &num;
		trace.inhibitRadius = DEFAULT_INHIBIT_RADIUS;
		
		trace.twoSided = info->si->twoSided;
		
		// make light list for this surface
		CreateTraceLightsForSurface( num, &trace );
		
		verts = yDrawVerts + ds->firstvertex;
		numAvg = 0;
		memset( avgColors, 0, sizeof( avgColors ));
		
		for( i = 0; i < ds->numvertices; i++ )
		{
			radVertLuxel = RAD_VERTEX_LUXEL( 0, ds->firstvertex + i );
			
			if( debugSurfaces ) VectorCopy( debugColors[num%12], radVertLuxel );
			else if( debugOrigin )
			{
				// color the luxel with luxel origin?
				VectorSubtract( info->maxs, info->mins, temp );
				VectorScale( temp, (1.0f / 255.0f), temp );
				VectorSubtract( origin, lm->mins, temp2 );
				radVertLuxel[0] = info->mins[0] + (temp[0] * temp2[0]);
				radVertLuxel[1] = info->mins[1] + (temp[1] * temp2[1]);
				radVertLuxel[2] = info->mins[2] + (temp[2] * temp2[2]);
			}
			else if( normalmap )
			{
				// color the luxel with the normal
				radVertLuxel[0] = (verts[i].normal[0] + 1.0f) * 127.5f;
				radVertLuxel[1] = (verts[i].normal[1] + 1.0f) * 127.5f;
				radVertLuxel[2] = (verts[i].normal[2] + 1.0f) * 127.5f;
			}
			else
			{
				// illuminate the vertex
				VectorSet( radVertLuxel, -1.0f, -1.0f, -1.0f );
				
				trace.cluster = ClusterForPointExtFilter( verts[i].point, VERTEX_EPSILON, info->numSurfaceClusters, &surfaceClusters[ info->firstSurfaceCluster ] );
				if( trace.cluster >= 0 )
				{
					VectorCopy( verts[i].point, trace.origin );
					VectorCopy( verts[i].normal, trace.normal );
					
					if( dirty ) dirt = DirtForSample( &trace );
					else dirt = 1.0f;

					LightingAtSample( &trace, ds->vStyles, colors );
					
					for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
					{
						VectorScale( colors[lightmapNum], dirt, colors[lightmapNum] );
						
						radVertLuxel = RAD_VERTEX_LUXEL( lightmapNum, ds->firstvertex + i );
						VectorCopy( colors[lightmapNum], radVertLuxel );
						VectorAdd( avgColors[lightmapNum], colors[lightmapNum], colors[lightmapNum] );
					}
				}
				
				// is this sample bright enough?
				radVertLuxel = RAD_VERTEX_LUXEL( 0, ds->firstvertex + i );
				if( radVertLuxel[0] <= ambientColor[0] && radVertLuxel[1] <= ambientColor[1] && radVertLuxel[2] <= ambientColor[2] )
				{
					// nudge the sample point around a bit
					for( x = 0; x < 4; x++ )
					{
						// two's complement 0, 1, -1, 2, -2, etc
						x1 = ((x >> 1) ^ (x & 1 ? -1 : 0)) + (x & 1);
						
						for( y = 0; y < 4; y++ )
						{
							y1 = ((y >> 1) ^ (y & 1 ? -1 : 0)) + (y & 1);
							
							for( z = 0; z < 4; z++ )
							{
								z1 = ((z >> 1) ^ (z & 1 ? -1 : 0)) + (z & 1);
								
								trace.origin[0] = verts[i].point[0] + (VERTEX_NUDGE * x1);
								trace.origin[1] = verts[i].point[1] + (VERTEX_NUDGE * y1);
								trace.origin[2] = verts[i].point[2] + (VERTEX_NUDGE * z1);
								
								trace.cluster = ClusterForPointExtFilter( origin, VERTEX_EPSILON, info->numSurfaceClusters, &surfaceClusters[ info->firstSurfaceCluster ] );
								if( trace.cluster < 0 )
									continue;
															
								LightingAtSample( &trace, ds->vStyles, colors );
								
								for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
								{
									VectorScale( colors[lightmapNum], dirt, colors[ lightmapNum ] );
									
									radVertLuxel = RAD_VERTEX_LUXEL( lightmapNum, ds->firstvertex + i );
									VectorCopy( colors[lightmapNum], radVertLuxel );
								}
								
								radVertLuxel = RAD_VERTEX_LUXEL( 0, ds->firstvertex + i );
								if( radVertLuxel[0] > ambientColor[0] || radVertLuxel[1] > ambientColor[1] || radVertLuxel[2] > ambientColor[2] )
									x = y = z = 1000;
							}
						}
					}
				}
				
				radVertLuxel = RAD_VERTEX_LUXEL( 0, ds->firstvertex + i );
				if( radVertLuxel[0] > ambientColor[0] || radVertLuxel[1] > ambientColor[1] || radVertLuxel[2] > ambientColor[2] )
				{
					numAvg++;
					for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
					{
						radVertLuxel = RAD_VERTEX_LUXEL( lightmapNum, ds->firstvertex + i );
						VectorAdd( avgColors[lightmapNum], radVertLuxel, avgColors[lightmapNum] );
					}
				}
			}
			numVertsIlluminated++;
		}
		
		if( numAvg > 0 )
		{
			for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
				VectorScale( avgColors[ lightmapNum ], (1.0f / numAvg), avgColors[ lightmapNum ] );
		}
		else
		{
			VectorCopy( ambientColor, avgColors[0] );
		}
		
		for( i = 0; i < ds->numvertices; i++ )
		{
			radVertLuxel = RAD_VERTEX_LUXEL( 0, ds->firstvertex + i );
			
			// store average in occluded vertexes
			if( radVertLuxel[0] < 0.0f )
			{
				for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
				{
					radVertLuxel = RAD_VERTEX_LUXEL( lightmapNum, ds->firstvertex + i );
					VectorCopy( avgColors[ lightmapNum ], radVertLuxel );
				}
			}
			
			for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
			{
				vertLuxel = VERTEX_LUXEL( lightmapNum, ds->firstvertex + i );
				radVertLuxel = RAD_VERTEX_LUXEL( lightmapNum, ds->firstvertex + i );
				
				if( bouncing || bounce == 0 || !bounceOnly )
					VectorAdd( vertLuxel, radVertLuxel, vertLuxel );
				if( !info->si->noVertexLight )
					ColorToBytes( vertLuxel, verts[i].color[ lightmapNum ], info->si->vertexScale );
			}
		}
		
		FreeTraceLights( &trace );
		return;
	}
	
	/*
	-----------------------------------------------------------------
	reconstitute vertex lighting from the luxels
	-----------------------------------------------------------------
	*/
	
	// set styles from lightmap
	for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
		ds->vStyles[lightmapNum] = lm->styles[lightmapNum];
	
	maxRadius = lm->sw;
	maxRadius = maxRadius > lm->sh ? maxRadius : lm->sh;
	
	verts = yDrawVerts + ds->firstvertex;
	for( i = 0; i < ds->numvertices; i++ )
	{
		for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
		{
			if( lm->superLuxels[lightmapNum] == NULL )
				continue;
			
			x = verts[i].lm[lightmapNum][0];
			y = verts[i].lm[lightmapNum][1];
			if( x < 0 ) x = 0;
			else if( x >= lm->sw )
				x = lm->sw - 1;
			if( y < 0 ) y = 0;
			else if( y >= lm->sh )
				y = lm->sh - 1;
			
			vertLuxel = VERTEX_LUXEL( lightmapNum, ds->firstvertex + i );
			radVertLuxel = RAD_VERTEX_LUXEL( lightmapNum, ds->firstvertex + i );
			
			/* color the luxel with the normal? */
			if( normalmap )
			{
				radVertLuxel[0] = (verts[i].normal[0] + 1.0f) * 127.5f;
				radVertLuxel[1] = (verts[i].normal[1] + 1.0f) * 127.5f;
				radVertLuxel[2] = (verts[i].normal[2] + 1.0f) * 127.5f;
			}
			else if( debugSurfaces )
				VectorCopy( debugColors[ num % 12 ], radVertLuxel );
			else
			{
				VectorClear( radVertLuxel );
				samples = 0.0f;

				for( radius = 0; radius < maxRadius && samples <= 0.0f; radius++ )
				{
					for( sy = (y - radius); sy <= (y + radius); sy++ )
					{
						if( sy < 0 || sy >= lm->sh )
							continue;
						
						for( sx = (x - radius); sx <= (x + radius); sx++ )
						{
							if( sx < 0 || sx >= lm->sw )
								continue;
							
							luxel = SUPER_LUXEL( lightmapNum, sx, sy );
							cluster = SUPER_CLUSTER( sx, sy );
							if( *cluster < 0 ) continue;
							
							// add its distinctiveness to our own
							VectorAdd( radVertLuxel, luxel, radVertLuxel );
							samples += luxel[3];
						}
					}
				}
				
				if( samples > 0.0f )
					VectorDivide( radVertLuxel, samples, radVertLuxel );
				else VectorCopy( ambientColor, radVertLuxel );
			}
			
			VectorAdd( vertLuxel, radVertLuxel, vertLuxel );
			numVertsIlluminated++;
			
			if( !info->si->noVertexLight )
				ColorToBytes( vertLuxel, verts[i].color[lightmapNum], 1.0f );
		}
	}
}



/* 
===============================================================================

light optimization (-fast)

creates a list of lights that will affect a surface and stores it in tw
this is to optimize surface lighting by culling out as many of the
lights in the world as possible from further calculation

===============================================================================
*/
/*
=============
SetupBrushes

determines opaque brushes in the world and find sky shaders for sunlight calculations
=============
*/
void SetupBrushes( void )
{
	int		i, j, b, contents;
	bool		inside;
	dbrush_t		*brush;
	dbrushside_t	*side;
	dshader_t		*shader;
	bsp_shader_t	*si;
	
	MsgDev( D_NOTE, "--- SetupBrushes ---\n" );
	
	if( opaqueBrushes == NULL ) opaqueBrushes = BSP_Malloc( numbrushes / 8 + 1 );
	numOpaqueBrushes = 0;
	
	for( i = 0; i < dmodels[0].numbrushes; i++ )
	{
		b = dmodels[0].firstbrush + i;
		brush = &dbrushes[b];
		
		inside = true;
		contents = 0;
		for( j = 0; j < brush->numsides && inside; j++ )
		{
			side = &dbrushsides[brush->firstside+j];
			shader = &dshaders[side->shadernum];
			
			si = FindShader( shader->name );
			if( si == NULL ) continue;
			contents |= si->contents;
		}
		
		// determine if this brush is opaque to light
		if(!(contents & CONTENTS_TRANSLUCENT))
		{
			opaqueBrushes[b>>3] |= (1<<(b&7));
			numOpaqueBrushes++;
			maxOpaqueBrush = i;
		}
	}
	MsgDev( D_NOTE, "%9d opaque brushes\n", numOpaqueBrushes );
}



/*
=============
ClusterVisible

determines if two clusters are visible to each other using the PVS
=============
*/
bool ClusterVisible( int a, int b )
{
	if( a < 0 || b < 0 ) return false;
	if( a == b ) return true;
	if( !visdatasize ) return true;

	DecompressVis( dvisdata + dvis->bitofs[a][DVIS_PVS], radPvs );
	
	if((radPvs[b>>3] & (1<<(b&7))))
		return true;
	return false;
}


/*
=============
PointInLeafNum_r

borrowed from vlight.c
=============
*/
int PointInLeafNum_r( vec3_t point, int nodenum )
{
	int		leafnum;
	vec_t		dist;
	dnode_t		*node;
	dplane_t		*plane;
	
	
	while( nodenum >= 0 )
	{
		node = &dnodes[nodenum];
		plane = &dplanes[node->planenum];
		dist = DotProduct( point, plane->normal ) - plane->dist;
		if( dist > 0.1 ) nodenum = node->children[0];
		else if( dist < -0.1 ) nodenum = node->children[1];
		else
		{
			leafnum = PointInLeafNum_r( point, node->children[0] );
			if( dleafs[leafnum].cluster != -1 )
				return leafnum;
			nodenum = node->children[1];
		}
	}
	leafnum = -nodenum - 1;
	return leafnum;
}


/*
=============
PointInLeafnum

borrowed from vlight.c
=============
*/
int PointInLeafNum( vec3_t point )
{
	return PointInLeafNum_r( point, 0 );
}


/*
=============
ClusterForPoint

returns the pvs cluster for point
=============
*/
int ClusterForPoint( vec3_t point )
{
	int	leafNum;

	leafNum = PointInLeafNum( point );
	if( leafNum < 0 ) return -1;
	
	return dleafs[leafNum].cluster;
}
 

/*
=============
ClusterVisibleToPoint

returns true if point can "see" cluster
=============
*/
bool ClusterVisibleToPoint( vec3_t point, int cluster )
{
	int	pointCluster;

	// get leafnum for point
	pointCluster = ClusterForPoint( point );
	if( pointCluster < 0 ) return false;
	
	// check pvs
	return ClusterVisible( pointCluster, cluster );
}

/*
=============
ClusterForPointExt

also takes brushes into account for occlusion testing
=============
*/
int ClusterForPointExt( vec3_t point, float epsilon )
{
	int		i, j, b, leafNum, cluster;
	float		dot;
	bool		inside;
	int		*brushes, numBSPBrushes;
	dleaf_t		*leaf;
	dbrush_t		*brush;
	dplane_t		*plane;
	
	leafNum = PointInLeafNum( point );
	if( leafNum < 0 ) return -1;
	leaf = &dleafs[leafNum];
	
	cluster = leaf->cluster;
	if( cluster < 0 ) return -1;
	
	// transparent leaf, so check point against all brushes in the leaf

	brushes = &dleafbrushes[leaf->firstleafbrush];
	numBSPBrushes = leaf->numleafbrushes;

	for( i = 0; i < numBSPBrushes; i++ )
	{
		b = brushes[i];
		if( b > maxOpaqueBrush ) continue;
		brush = &dbrushes[b];
		if( !(opaqueBrushes[b>>3] & (1<<(b&7))))
			continue;
		
		inside = true;
		for( j = 0; j < brush->numsides && inside; j++ )
		{
			plane = &dplanes[dbrushsides[brush->firstside+j].planenum];
			dot = DotProduct( point, plane->normal );
			dot -= plane->dist;
			if( dot > epsilon ) inside = false;
		}
		
		// if inside, return bogus cluster
		if( inside ) return -1 - b;
	}
	
	// if the point made it this far, it's not inside any opaque brushes
	return cluster;
}



/*
=============
ClusterForPointExtFilter

adds cluster checking against a list of known valid clusters
=============
*/
int ClusterForPointExtFilter( vec3_t point, float epsilon, int numClusters, int *clusters )
{
	int	i, cluster;
	
	cluster = ClusterForPointExt( point, epsilon );
	
	if( cluster < 0 || numClusters <= 0 || clusters == NULL )
		return cluster;
	
	for( i = 0; i < numClusters; i++ )
	{
		if( cluster == clusters[i] || ClusterVisible( cluster, clusters[i] ))
			return cluster;
	}
	return -1;
}


/*
=============
ShaderForPointInLeaf

checks a point against all brushes in a leaf, returning the shader of the brush
also sets the cumulative surface and content flags for the brush hit
=============
*/
int ShaderForPointInLeaf( vec3_t point, int leafNum, float epsilon, int wantContentFlags, int wantSurfaceFlags, int *contentFlags, int *surfaceFlags )
{
	int		i, j;
	float		dot;
	bool		inside;
	int		*brushes, numBSPBrushes;
	dleaf_t		*leaf;
	dbrush_t		*brush;
	dbrushside_t	*side;
	dplane_t		*plane;
	dshader_t		*shader;
	int		allSurfaceFlags, allContentFlags;
	
	*surfaceFlags = 0;
	*contentFlags = 0;
	
	if( leafNum < 0 ) return -1;
	leaf = &dleafs[leafNum];
	
	// transparent leaf, so check point against all brushes in the leaf
	brushes = &dleafbrushes[leaf->firstleafbrush];
	numBSPBrushes = leaf->numleafbrushes;

	for( i = 0; i < numBSPBrushes; i++ )
	{
		brush = &dbrushes[brushes[i]];
		
		inside = true;
		allSurfaceFlags = 0;
		allContentFlags = 0;
		for( j = 0; j < brush->numsides && inside; j++ )
		{
			side = &dbrushsides[brush->firstside+j];
			plane = &dplanes[side->planenum];
			dot = DotProduct( point, plane->normal );
			dot -= plane->dist;
			if( dot > epsilon ) inside = false;
			else
			{
				shader = &dshaders[side->shadernum];
				allSurfaceFlags |= shader->surfaceFlags;
				allContentFlags |= shader->contents;
			}
		}

		if( inside )
		{
			// if there are desired flags, check for same and continue if they aren't matched
			if( wantContentFlags && !(wantContentFlags & allContentFlags) )
				continue;
			if( wantSurfaceFlags && !(wantSurfaceFlags & allSurfaceFlags) )
				continue;
			
			// store the cumulative flags and return the brush shader (which is mostly useless)
			*surfaceFlags = allSurfaceFlags;
			*contentFlags = allContentFlags;
			return brush->shadernum;
		}
	}
	
	// if the point made it this far, it's not inside any brushes
	return -1;
}


/*
=============
ChopBounds

chops a bounding box by the plane defined by origin and normal
returns false if the bounds is entirely clipped away
this is not exactly the fastest way to do this...
=============
*/
bool ChopBounds( vec3_t mins, vec3_t maxs, vec3_t origin, vec3_t normal )
{
	// FIXME: rewrite this so it doesn't use bloody brushes
	return true;
}



/*
=============
SetupEnvelopes

calculates each light's effective envelope,
taking into account brightness, type, and pvs.
=============
*/
void SetupEnvelopes( bool forGrid, bool fastFlag )
{
	int		i, x, y, z, x1, y1, z1;
	light_t		*light, *light2, **owner;
	dleaf_t		*leaf;
	vec3_t		origin, dir, mins, maxs, nullVector = { 0, 0, 0 };
	float		radius, intensity;
	light_t		*buckets[256];
	
	if( lights == NULL ) return;
	MsgDev( D_NOTE, "--- SetupEnvelopes%s ---\n", fastFlag ? " (fast)" : "" );
	
	numLights = 0;
	numCulledLights = 0;
	owner = &lights;

	while( *owner != NULL )
	{
		light = *owner;
		
		if( light->photons < 0.0f || light->add < 0.0f )
		{
			light->photons *= -1.0f;
			light->add *= -1.0f;
			light->flags |= LIGHT_NEGATIVE;
		}
		
		if( light->type == emit_sun )
		{
			light->cluster = 0;
			light->envelope = MAX_WORLD_COORD * 8.0f;
			VectorSet( light->mins, MIN_WORLD_COORD * 8.0f, MIN_WORLD_COORD * 8.0f, MIN_WORLD_COORD * 8.0f );
			VectorSet( light->maxs, MAX_WORLD_COORD * 8.0f, MAX_WORLD_COORD * 8.0f, MAX_WORLD_COORD * 8.0f );
		}
		else
		{
			light->cluster = ClusterForPointExt( light->origin, LIGHT_EPSILON );
			
			if( light->cluster < 0 )
			{
				for( x = 0; x < 4; x++ )
				{
					// two's complement 0, 1, -1, 2, -2, etc
					x1 = ((x >> 1) ^ (x & 1 ? -1 : 0)) + (x & 1);
					
					for( y = 0; y < 4; y++ )
					{
						y1 = ((y >> 1) ^ (y & 1 ? -1 : 0)) + (y & 1);
						
						for( z = 0; z < 4; z++ )
						{
							z1 = ((z >> 1) ^ (z & 1 ? -1 : 0)) + (z & 1);
							
							origin[0] = light->origin[0] + (LIGHT_NUDGE * x1);
							origin[1] = light->origin[1] + (LIGHT_NUDGE * y1);
							origin[2] = light->origin[2] + (LIGHT_NUDGE * z1);
							
							light->cluster = ClusterForPointExt( origin, LIGHT_EPSILON );
							if( light->cluster < 0 ) continue;
							VectorCopy( origin, light->origin );
						}
					}
				}
			}
			
			// only calculate for lights in pvs and outside of opaque brushes
			if( light->cluster >= 0 )
			{
				// set light fast flag
				if( fastFlag ) light->flags |= LIGHT_FAST_TEMP;
				else light->flags &= ~LIGHT_FAST_TEMP;
				if( light->si && light->si->noFast )
					light->flags &= ~(LIGHT_FAST|LIGHT_FAST_TEMP);
				
				light->envelope = 0;
				
				if( exactPointToPolygon && light->type == emit_area && light->w != NULL )
				{
					// ugly hack to calculate extent for area lights, but only done once
					VectorScale( light->normal, -1.0f, dir );
					for( radius = 100.0f; radius < 130000.0f && light->envelope == 0; radius += 10.0f )
					{
						float	factor;
						
						VectorMA( light->origin, radius, light->normal, origin );
						factor = PointToPolygonFormFactor( origin, dir, light->w );
						if( factor < 0.0f ) factor *= -1.0f;
						if( (factor * light->add) <= light->falloffTolerance )
							light->envelope = radius;
					}
					if( !(light->flags & LIGHT_FAST) && !(light->flags & LIGHT_FAST_TEMP))
						light->envelope = MAX_WORLD_COORD * 8.0f;
				}
				else
				{
					radius = 0.0f;
					intensity = light->photons;
				}
				
				if( light->envelope <= 0.0f )
				{
					if(!(light->flags & LIGHT_ATTEN_DISTANCE) )
						light->envelope = MAX_WORLD_COORD * 8.0f;
					else if( (light->flags & LIGHT_ATTEN_LINEAR ))
						light->envelope = ((intensity * linearScale) - light->falloffTolerance) / light->fade;

					/*
					add = angle * light->photons * linearScale - (dist * light->fade);
					T = (light->photons * linearScale) - (dist * light->fade);
					T + (dist * light->fade) = (light->photons * linearScale);
					dist * light->fade = (light->photons * linearScale) - T;
					dist = ((light->photons * linearScale) - T) / light->fade;
					*/
					
					// solve for inverse square falloff
					else light->envelope = sqrt( intensity / light->falloffTolerance ) + radius;
						
					/*
					add = light->photons / (dist * dist);
					T = light->photons / (dist * dist);
					T * (dist * dist) = light->photons;
					dist = sqrt( light->photons / T );
					*/
				}
				
				// chop radius against pvs
				ClearBounds( mins, maxs );
					
				for( i = 0; i < numleafs; i++ )
				{
					leaf = &dleafs[i];
						
					if( leaf->cluster < 0 ) continue;
					if(!ClusterVisible( light->cluster, leaf->cluster ))
						continue;
						
					// add this leafs bbox to the bounds
					VectorCopy( leaf->mins, origin );
					AddPointToBounds( origin, mins, maxs );
					VectorCopy( leaf->maxs, origin );
					AddPointToBounds( origin, mins, maxs );
				}
					
				// test to see if bounds encompass light
				for( i = 0; i < 3; i++ )
				{
					if( mins[i] > light->origin[i] || maxs[i] < light->origin[i] )
						AddPointToBounds( light->origin, mins, maxs );
				}
					
				// chop the bounds by a plane for area lights and spotlights
				if( light->type == emit_area || light->type == emit_spotlight )
					ChopBounds( mins, maxs, light->origin, light->normal );
					
				VectorCopy( mins, light->mins );
				VectorCopy( maxs, light->maxs );
					
				// reflect bounds around light origin
				VectorScale( light->origin, 2, origin );
				VectorSubtract( origin, maxs, origin );
				AddPointToBounds( origin, mins, maxs );
				VectorScale( light->origin, 2, origin );
				VectorSubtract( origin, mins, origin );
				AddPointToBounds( origin, mins, maxs );
					 
				// calculate spherical bounds
				VectorSubtract( maxs, light->origin, dir );
				radius = (float)VectorLength( dir );
					
				// if this radius is smaller than the envelope, then set the envelope to it
				if( radius < light->envelope )
				{
					light->envelope = radius;
				}
				
				// add grid/surface only check
				if( forGrid )
				{
					if( !(light->flags & LIGHT_GRID))
						light->envelope = 0.0f;
				}
				else
				{
					if( !(light->flags & LIGHT_SURFACES))
						light->envelope = 0.0f;
				}
			}
			
			// culled?
			if( light->cluster < 0 || light->envelope <= 0.0f )
			{
				numCulledLights++;
				*owner = light->next;
				if( light->w != NULL )
					Mem_Free( light->w );
				Mem_Free( light );
				continue;
			}
		}
		
		light->envelope2 = (light->envelope * light->envelope);
		numLights++;
		owner = &((**owner).next);
	}
	
	// bucket sort lights by style
	memset( buckets, 0, sizeof( buckets ));
	light2 = NULL;

	for( light = lights; light != NULL; light = light2 )
	{
		light2 = light->next;
		
		light->next = buckets[light->style];
		buckets[light->style] = light;
		
		// if any styled light is present, automatically set nocollapse
		if( light->style != LS_NORMAL ) noCollapse = true;
	}
	
	lights = NULL;
	for( i = 255; i >= 0; i-- )
	{
		light2 = NULL;
		for( light = buckets[i]; light != NULL; light = light2 )
		{
			light2 = light->next;
			light->next = lights;
			lights = light;
		}
	}
	
	MsgDev( D_INFO, "%6i total lights\n", numLights );
	MsgDev( D_INFO, "%6i culled lights\n", numCulledLights );
}


/*
=============
CreateTraceLightsForBounds

creates a list of lights that affect the given bounding box and pvs clusters (bsp leaves)
=============
*/
void CreateTraceLightsForBounds( vec3_t mins, vec3_t maxs, vec3_t normal, int numClusters, int *clusters, int flags, lighttrace_t *trace )
{
	int 	i;
	light_t	*light;
	vec3_t	origin, dir, nullVector = { 0.0f, 0.0f, 0.0f };
	float	radius, dist, length;

	if( numLights == 0 ) SetupEnvelopes( false, fast );

	// allocate the light list
	trace->lights = BSP_Malloc( sizeof( light_t* ) * (numLights + 1));
	trace->numLights = 0;
	
	VectorAdd( mins, maxs, origin );
	VectorScale( origin, 0.5f, origin );
	VectorSubtract( maxs, origin, dir );
	radius = (float) VectorLength( dir );
	
	// get length of normal vector
	if( normal != NULL ) length = VectorLength( normal );
	else
	{
		normal = nullVector;
		length = 0;
	}
	
	// test each light and see if it reaches the sphere
	// NOTE: the attenuation code MUST match LightingAtSample
	for( light = lights; light; light = light->next )
	{
		if( light->envelope <= 0 )
		{
			lightsEnvelopeCulled++;
			continue;
		}
		
		if(!(light->flags & flags))
			continue;
		
		// sunlight skips all this nonsense
		if( light->type != emit_sun )
		{
			if( numClusters > 0 && clusters != NULL )
			{
				for( i = 0; i < numClusters; i++ )
				{
					if( ClusterVisible( light->cluster, clusters[i] ))
						break;
				}
				
				// FIXME!
				if( i == numClusters )
				{
					lightsClusterCulled++;
					continue;
				}
			}
			
			// if the light's bounding sphere intersects with the bounding sphere then this light needs to be tested
			VectorSubtract( light->origin, origin, dir );
			dist = VectorLength( dir );
			dist -= light->envelope;
			dist -= radius;
			if( dist > 0 )
			{
				lightsEnvelopeCulled++;
				continue;
			}
		}
		
		// planar surfaces (except twosided surfaces) have a couple more checks
		if( length > 0.0f && trace->twoSided == false )
		{
			// lights coplanar with a surface won't light it
			if( !(light->flags & LIGHT_TWOSIDED) && DotProduct( light->normal, normal ) > 0.999f )
			{
				lightsPlaneCulled++;
				continue;
			}
			
			// check to see if light is behind the plane
			if( DotProduct( light->origin, normal ) - DotProduct( origin, normal ) < -1.0f )
			{
				lightsPlaneCulled++;
				continue;
			}
		}
		
		trace->lights[trace->numLights++] = light;
	}
	
	// make last light null
	trace->lights[trace->numLights] = NULL;
}

void FreeTraceLights( lighttrace_t *trace )
{
	if( trace->lights != NULL )
		Mem_Free( trace->lights );
}

/*
=============
CreateTraceLightsForSurface

creates a list of lights that can potentially affect a drawsurface
=============
*/
void CreateTraceLightsForSurface( int num, lighttrace_t *trace )
{
	int		i;
	vec3_t		mins, maxs, normal;
	dvertex_t		*dv;
	dsurface_t	*ds;
	surfaceInfo_t	*info;
	
	if( num < 0 ) return;
	
	ds = &dsurfaces[num];
	info = &surfaceInfos[num];
	
	// get the mins/maxs for the dsurf
	ClearBounds( mins, maxs );
	VectorCopy( dvertexes[ds->firstvertex].normal, normal );
	for( i = 0; i < ds->numvertices; i++ )
	{
		dv = &yDrawVerts[ds->firstvertex+i];
		AddPointToBounds( dv->point, mins, maxs );
		if( !VectorCompare( dv->normal, normal ))
			VectorClear( normal );
	}
	
	// create the lights for the bounding box
	CreateTraceLightsForBounds( mins, maxs, normal, info->numSurfaceClusters, &surfaceClusters[info->firstSurfaceCluster ], LIGHT_SURFACES, trace );
}