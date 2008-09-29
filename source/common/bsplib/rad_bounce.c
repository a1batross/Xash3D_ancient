//=======================================================================
//			Copyright XashXT Group 2008 ©
//		      rad_bounce.c - radiocity bounce code
//=======================================================================

#include "bsplib.h"
#include "const.h"

#define MAX_SAMPLES			150
#define SAMPLE_GRANULARITY		6
#define RADIOSITY_MAX_GRADIENT	0.75f
#define RADIOSITY_VALUE		500.0f
#define RADIOSITY_MIN		0.0001f
#define RADIOSITY_CLIP_EPSILON	0.125f
#define PLANAR_EPSILON		0.1f

float	diffuseSubdivide = 256.0f;
float	minDiffuseSubdivide = 64.0f;
int	numDiffuseSurfaces = 0;
int	numDiffuseLights;
int	numBrushDiffuseLights;
int	numTriangleDiffuseLights;
int	numPatchDiffuseLights;

bool	noStyles = false;
float	falloffTolerance = 1.0f;
float	formFactorValueScale = 3.0f;
float	linearScale = (1.0f/8000.0f);
float	pointScale = 7500.0f;
float	areaScale = 0.25f;
float	skyScale = 1.0f;
float	bounceScale = 0.25f;

/*
=============
RadFreeLights

deletes any existing lights, freeing up memory for the next bounce
=============
*/
void RadFreeLights( void )
{
	light_t	*light, *next;
	
	for( light = lights; light; light = next )
	{
		next = light->next;
		if( light->w != NULL )
			FreeWinding( light->w );
		Mem_Free( light );
	}
	numLights = 0;
	lights = NULL;
}

/*
=============
RadClipWindingEpsilon

clips a rad winding by a plane
based off the regular clip winding code
=============
*/
static void RadClipWindingEpsilon( radWinding_t *in, vec3_t normal, vec_t dist, vec_t epsilon, radWinding_t *front, radWinding_t *back, clipWork_t *cw )
{
	vec_t		*dists;
	int		*sides;
	int		counts[3];
	vec_t		dot;
	int		i, j, k;
	radVert_t		*v1, *v2, mid;
	int		maxPoints;
	
	dists = cw->dists;
	sides = cw->sides;
	
	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for( i = 0; i < in->numVerts; i++ )
	{
		dot = DotProduct( in->verts[i].point, normal );
		dot -= dist;
		dists[i] = dot;
		if( dot > epsilon )
			sides[i] = SIDE_FRONT;
		else if( dot < -epsilon )
			sides[i] = SIDE_BACK;
		else
			sides[i] = SIDE_ON;
		counts[ sides[i] ]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];
	
	front->numVerts = back->numVerts = 0;
	
	if( counts[0] == 0 )
	{
		Mem_Copy( back, in, sizeof( radWinding_t ));
		return;
	}
	if( counts[1] == 0 )
	{
		Mem_Copy( front, in, sizeof( radWinding_t ));
		return;
	}
	
	maxPoints = in->numVerts + 4;
	
	for( i = 0; i < in->numVerts; i++ )
	{
		v1 = &in->verts[i];
		
		if( sides[i] == SIDE_ON )
		{
			Mem_Copy( &front->verts[front->numVerts++], v1, sizeof( radVert_t ));
			Mem_Copy( &back->verts[back->numVerts++], v1, sizeof( radVert_t ));
			continue;
		}
	
		if( sides[i] == SIDE_FRONT ) Mem_Copy( &front->verts[front->numVerts++], v1, sizeof( radVert_t ));
		if( sides[i] == SIDE_BACK ) Mem_Copy( &back->verts[back->numVerts++], v1, sizeof( radVert_t ));
		if( sides[i+1] == SIDE_ON || sides[i+1] == sides[i] )
			continue;
			
		v2 = &in->verts[(i + 1) % in->numVerts];
		dot = dists[i] / (dists[i] - dists[i+1]);

		for( j = 0; j < 4; j++ )
		{
			if( j < 4 )
			{
				for( k = 0; k < LM_STYLES; k++ )
					mid.color[k][j] = v1->color[k][j] + dot * (v2->color[k][j] - v1->color[k][j]);
			}
			
			// xyz, normal
			if( j < 3 )
			{
				mid.point[j] = v1->point[j] + dot * (v2->point[j] - v1->point[j]);
				mid.normal[j] = v1->normal[j] + dot * (v2->normal[j] - v1->normal[j]);
			}
			
			// st, lightmap
			if( j < 2 )
			{
				mid.st[j] = v1->st[j] + dot * (v2->st[j] - v1->st[j]);
				for( k = 0; k < LM_STYLES; k++ )
					mid.lm[k][j] = v1->lm[k][j] + dot * (v2->lm[k][j] - v1->lm[k][j]);
			}
		}
		
		// normalize the averaged normal
		VectorNormalize( mid.normal );

		// copy the midpoint to both windings
		Mem_Copy( &front->verts[front->numVerts++], &mid, sizeof( radVert_t ));
		Mem_Copy( &back->verts[back->numVerts++], &mid, sizeof( radVert_t ));
	}
	
	if( front->numVerts > maxPoints || front->numVerts > maxPoints )
		Sys_Error( "RadClipWindingEpsilon: points exceeded estimate\n" );
	if( front->numVerts > MAX_POINTS_ON_WINDING || front->numVerts > MAX_POINTS_ON_WINDING )
		Sys_Error( "RadClipWindingEpsilon: MAX_POINTS_ON_WINDING limit exceeded\n" );
}


/*
=============
RadSampleImage

samples a texture image for a given color
returns false if pixels are bad
=============
*/
bool RadSampleImage( byte *pixels, int width, int height, float st[2], float color[4] )
{
	float	sto[2];
	int		x, y;

	color[0] = color[1] = color[2] = color[3] = 255;
	
	if( pixels == NULL || width < 1 || height < 1 )
		return false;
	
	sto[0] = st[0];
	while( sto[0] < 0.0f ) sto[0] += 1.0f;
	sto[1] = st[1];
	while( sto[1] < 0.0f ) sto[1] += 1.0f;

	// get offsets
	x = ((float) width * sto[0]) + 0.5f;
	x %= width;
	y = ((float) height * sto[1])  + 0.5f;
	y %= height;
	
	pixels += (y * width * 4) + (x * 4);
	VectorCopy( pixels, color );
	color[3] = pixels[3];
	return true;
}


/*
=============
RadSample

samples a fragment's lightmap or vertex color and returns an
average color and a color gradient for the sample
=============
*/
static void RadSample( int lightmapNum, dsurface_t *ds, rawLightmap_t *lm, bsp_shader_t *si, radWinding_t *rw, vec3_t average, vec3_t gradient, int *style )
{
	int	i, j, k, l, v, x, y, samples;
	vec3_t	color, mins, maxs;
	vec4_t	textureColor;
	float	alpha, alphaI, bf;
	vec3_t	blend;
	float	st[2], lightmap[2], *radLuxel;
	radVert_t	*rv[3];

	ClearBounds( mins, maxs );
	VectorClear( average );
	VectorClear( gradient );
	alpha = 0;
	
	if( rw == NULL || rw->numVerts < 3 )
		return;
	
	samples = 0;
	
	// sample vertex colors if no lightmap or this is the initial pass
	if( lm == NULL || lm->radLuxels[lightmapNum] == NULL || bouncing == false )
	{
		for( samples = 0; samples < rw->numVerts; samples++ )
		{
			if( !RadSampleImage( si->lightImage->buffer, si->lightImage->width, si->lightImage->height, rw->verts[samples].st, textureColor ))
			{
				VectorCopy( si->averageColor, textureColor );
				textureColor[4] = 255.0f;
			}
			for( i = 0; i < 3; i++ )
				color[i] = (textureColor[i] / 255) * (rw->verts[samples].color[lightmapNum][i] / 255.0f);
			
			AddPointToBounds( color, mins, maxs );
			VectorAdd( average, color, average );
			
			alpha += (textureColor[3] / 255.0f) * (rw->verts[samples].color[lightmapNum][3] / 255.0f);
		}
		
		*style = ds->vStyles[lightmapNum];
	}
	else
	{
		// fracture the winding into a fan (including degenerate tris)
		for( v = 1; v < (rw->numVerts - 1) && samples < MAX_SAMPLES; v++ )
		{
			rv[0] = &rw->verts[0];
			rv[1] = &rw->verts[v];
			rv[2] = &rw->verts[v+1];
			
			// this code is embarassing (really should just rasterize the triangle)
			for( i = 1; i < SAMPLE_GRANULARITY && samples < MAX_SAMPLES; i++ )
			{
				for( j = 1; j < SAMPLE_GRANULARITY && samples < MAX_SAMPLES; j++ )
				{
					for( k = 1; k < SAMPLE_GRANULARITY && samples < MAX_SAMPLES; k++ )
					{
						// create a blend vector (barycentric coordinates)
						blend[0] = i;
						blend[1] = j;
						blend[2] = k;
						bf = (1.0 / (blend[0] + blend[1] + blend[2]));
						VectorScale( blend, bf, blend );
						
						// create a blended sample
						st[0] = st[1] = 0.0f;
						lightmap[0] = lightmap[1] = 0.0f;
						alphaI = 0.0f;
						for( l = 0; l < 3; l++ )
						{
							st[0] += (rv[l]->st[0] * blend[l]);
							st[1] += (rv[l]->st[1] * blend[l]);
							lightmap[0] += (rv[l]->lm[lightmapNum][0] * blend[l]);
							lightmap[1] += (rv[l]->lm[lightmapNum][1] * blend[l]);
							alphaI += (rv[l]->color[lightmapNum][3] * blend[l]);
						}
						
						// get lightmap xy coords
						x = lightmap[0] / (float) superSample;
						y = lightmap[1] / (float) superSample;
						if( x < 0 ) x = 0;
						else if ( x >= lm->w )
							x = lm->w - 1;
						if( y < 0 ) y = 0;
						else if ( y >= lm->h )
							y = lm->h - 1;
						
						// get radiosity luxel
						radLuxel = RAD_LUXEL( lightmapNum, x, y );
						
						// ignore unlit/unused luxels
						if( radLuxel[0] < 0.0f ) continue;
						
						samples++;
						
						// multiply by texture color
						if( !RadSampleImage( si->lightImage->buffer, si->lightImage->width, si->lightImage->height, st, textureColor ))
						{
							VectorCopy( si->averageColor, textureColor );
							textureColor[4] = 255;
						}
						for( i = 0; i < 3; i++ )
							color[i] = (textureColor[i] / 255) * (radLuxel[i] / 255);
						
						AddPointToBounds( color, mins, maxs );
						VectorAdd( average, color, average );
						alpha += (textureColor[3] / 255) * (alphaI / 255);
					}
				}
			}
		}
		
		*style = ds->lStyles[lightmapNum];
	}
	
	if( samples <= 0 ) return;
	
	// average the color
	VectorScale( average, (1.0 / samples), average );
	for( i = 0; i < 3; i++ ) gradient[i] = (maxs[i] - mins[i]) * maxs[i];
}



/*
=============
RadSubdivideDiffuseLight

subdivides a radiosity winding until it is smaller than subdivide, then generates an area light
=============
*/
static void RadSubdivideDiffuseLight( int lightmapNum, dsurface_t *ds, rawLightmap_t *lm, bsp_shader_t *si, float scale, float subdivide, bool original, radWinding_t *rw, clipWork_t *cw )
{
	int		i, style;
	float		dist, area, value;
	vec3_t		mins, maxs, normal, d1, d2, cross, color, gradient;
	light_t		*light, *splash;
	winding_t		*w;
	
	if( rw == NULL || rw->numVerts < 3 )
		return;
	
	ClearBounds( mins, maxs );
	for( i = 0; i < rw->numVerts; i++ ) AddPointToBounds( rw->verts[i].point, mins, maxs );
	
	for( i = 0; i < 3; i++ )
	{
		if( maxs[i] - mins[i] > subdivide )
		{
			radWinding_t	front, back;

			VectorClear( normal );
			normal[i] = 1;
			dist = (maxs[i] + mins[i]) * 0.5f;
			
			RadClipWindingEpsilon( rw, normal, dist, RADIOSITY_CLIP_EPSILON, &front, &back, cw );
			
			RadSubdivideDiffuseLight( lightmapNum, ds, lm, si, scale, subdivide, false, &front, cw );
			RadSubdivideDiffuseLight( lightmapNum, ds, lm, si, scale, subdivide, false, &back, cw );
			return;
		}
	}
	
	area = 0.0f;
	for( i = 2; i < rw->numVerts; i++ )
	{
		VectorSubtract( rw->verts[ i - 1 ].point, rw->verts[0].point, d1 );
		VectorSubtract( rw->verts[i].point, rw->verts[0].point, d2 );
		CrossProduct( d1, d2, cross );
		area += 0.5f * VectorLength( cross );
	}
	if( area < 1.0f || area > 20000000.0f )
		return;
	
	if( bouncing )
	{
		// get color sample for the surface fragment
		RadSample( lightmapNum, ds, lm, si, rw, color, gradient, &style );
		
		/* if color gradient is too high, subdivide again */
		if( subdivide > minDiffuseSubdivide && (gradient[0] > RADIOSITY_MAX_GRADIENT || gradient[1] > RADIOSITY_MAX_GRADIENT || gradient[2] > RADIOSITY_MAX_GRADIENT))
		{
			RadSubdivideDiffuseLight( lightmapNum, ds, lm, si, scale, (subdivide / 2.0f), false, rw, cw );
			return;
		}
	}
	
	// create a regular winding and an average normal
	w = AllocWinding( rw->numVerts );
	w->numpoints = rw->numVerts;
	VectorClear( normal );
	for( i = 0; i < rw->numVerts; i++ )
	{
		VectorCopy( rw->verts[i].point, w->p[i] );
		VectorAdd( normal, rw->verts[i].normal, normal );
	}

	VectorScale( normal, (1.0f / rw->numVerts), normal );
	if( VectorNormalizeLength( normal ) == 0.0f )
		return;
	
	if( bouncing && VectorLength( color ) < RADIOSITY_MIN )
		return;

	numDiffuseLights++;
	switch( ds->surfaceType )
	{
	case MST_PLANAR:
		numBrushDiffuseLights++;
		break;
	case MST_TRIANGLE_SOUP:
		numTriangleDiffuseLights++;
		break;
	case MST_PATCH:
		numPatchDiffuseLights++;
		break;
	}
	
	light = BSP_Malloc( sizeof( *light ));
	
	ThreadLock();
	light->next = lights;
	lights = light;
	ThreadUnlock();
	
	light->flags = LIGHT_AREA_DEFAULT;
	light->type = emit_area;
	light->si = si;
	light->fade = 1.0f;
	light->w = w;
	
	light->falloffTolerance = falloffTolerance;
	
	// bouncing light?
	if( bouncing == false )
	{
		value = si->value;
		light->photons = value * area * areaScale;
		light->add = value * formFactorValueScale * areaScale;
		VectorCopy( si->color, light->color );
		VectorScale( light->color, light->add, light->emitColor );
		light->style = noStyles ? LS_NORMAL : si->lightStyle;
		if( light->style < LS_NORMAL || light->style >= LS_NONE )
			light->style = LS_NORMAL;
		
		VectorAdd( mins, maxs, light->origin );
		VectorScale( light->origin, 0.5f, light->origin );
		
		// nudge it off the plane a bit
		VectorCopy( normal, light->normal );
		VectorMA( light->origin, 1.0f, light->normal, light->origin );
		light->dist = DotProduct( light->origin, normal );
		
		// optionally create a point splashsplash light for first pass
		if( original && si->backsplashFraction > 0 )
		{
			splash = BSP_Malloc( sizeof( *splash ));
			splash->next = lights;
			lights = splash;
			
			splash->flags = LIGHT_Q3A_DEFAULT;
			splash->type = emit_point;
			splash->photons = light->photons * si->backsplashFraction;
			splash->fade = 1.0f;
			splash->si = si;
			VectorMA( light->origin, si->backsplashDistance, normal, splash->origin );
			VectorCopy( si->color, splash->color );
			splash->falloffTolerance = falloffTolerance;
			splash->style = noStyles ? LS_NORMAL : light->style;
			
			numPointLights++;
		}
	}
	else
	{
		// handle bounced light (radiosity) a little differently
		value = RADIOSITY_VALUE * si->bounceScale * 0.375f;
		light->photons = value * area * bounceScale;
		light->add = value * formFactorValueScale * bounceScale;
		VectorCopy( color, light->color );
		VectorScale( light->color, light->add, light->emitColor );
		light->style = noStyles ? LS_NORMAL : style;
		if( light->style < LS_NORMAL || light->style >= LS_NONE )
			light->style = LS_NORMAL;
		
		WindingCenter( w, light->origin );
		
		// nudge it off the plane a bit
		VectorCopy( normal, light->normal );
		VectorMA( light->origin, 1.0f, light->normal, light->origin );
		light->dist = DotProduct( light->origin, normal );
	}
	
	// emit light from both sides?
	if( si->contents & CONTENTS_FOG || si->twoSided )
		light->flags |= LIGHT_TWOSIDED;
}

/*
=============
RadLightForTriangles

creates unbounced diffuse lights for triangle soup (misc_models, etc)
=============
*/
void RadLightForTriangles( int num, int lightmapNum, rawLightmap_t *lm, bsp_shader_t *si, float scale, float subdivide, clipWork_t *cw )
{
	int		i, j, k, v;
	dsurface_t	*ds;
	surfaceInfo_t	*info;
	float		*radVertexLuxel;
	radWinding_t	rw;
	
	ds = &dsurfaces[num];
	info = &surfaceInfos[num];
	
	// each triangle is a potential emitter
	rw.numVerts = 3;
	for( i = 0; i < ds->numindices; i += 3 )
	{
		for( j = 0; j < 3; j++ )
		{
			// get vertex index and rad vertex luxel
			v = ds->firstvertex + dindexes[ds->firstindex+i+j];
			
			// get most everything
			Mem_Copy( &rw.verts[j], &yDrawVerts[v], sizeof( dvertex_t ));
			
			// fix colors
			for( k = 0; k < LM_STYLES; k++ )
			{
				radVertexLuxel = RAD_VERTEX_LUXEL( k, ds->firstvertex + dindexes[ds->firstindex+i+j] );
				VectorCopy( radVertexLuxel, rw.verts[j].color[k] );
				rw.verts[j].color[k][3] = yDrawVerts[v].color[k][3];
			}
		}
		
		// subdivide into area lights
		RadSubdivideDiffuseLight( lightmapNum, ds, lm, si, scale, subdivide, true, &rw, cw );
	}
}


/*
=============
RadLightForPatch

creates unbounced diffuse lights for patches
=============
*/
void RadLightForPatch( int num, int lightmapNum, rawLightmap_t *lm, bsp_shader_t *si, float scale, float subdivide, clipWork_t *cw )
{
	int		i, x, y, v, t, pw[5], r;
	dsurface_t	*ds;
	surfaceInfo_t	*info;
	dvertex_t		*bogus;
	dvertex_t		*dv[4];
	bsp_mesh_t	src, *subdivided, *mesh;
	float		*radVertexLuxel;
	float		dist;
	vec4_t		plane;
	bool		planar;
	radWinding_t	rw;
	
	ds = &dsurfaces[num];
	info = &surfaceInfos[num];
	
	// construct a bogus vert list with color index stuffed into color[0]
	bogus = BSP_Malloc( ds->numvertices * sizeof( dvertex_t ));
	Mem_Copy( bogus, &yDrawVerts[ds->firstvertex], ds->numvertices * sizeof( dvertex_t ));
	for( i = 0; i < ds->numvertices; i++ ) bogus[i].color[0][0] = i;
	
	// build a subdivided mesh identical to shadow facets for this patch
	// this MUST MATCH FacetsForPatch() identically!
	src.width = ds->patch.width;
	src.height = ds->patch.height;
	src.verts = bogus;

	subdivided = SubdivideMesh2( src, info->patchIterations );
	PutMeshOnCurve( *subdivided );

	mesh = RemoveLinearMeshColumnsRows( subdivided );
	FreeMesh( subdivided );
	Mem_Free( bogus );
	
	// FIXME: build interpolation table into color[1]
	
	// fix up color indexes
	for( i = 0; i < (mesh->width * mesh->height); i++ )
	{
		dv[0] = &mesh->verts[i];
		if( dv[0]->color[0][0] >= ds->numvertices )
			dv[0]->color[0][0] = ds->numvertices - 1;
	}
	
	// iterate through the mesh quads
	for( y = 0; y < (mesh->height - 1); y++ )
	{
		for( x = 0; x < (mesh->width - 1); x++ )
		{
			pw[0] = x + (y * mesh->width);
			pw[1] = x + ((y + 1) * mesh->width);
			pw[2] = x + 1 + ((y + 1) * mesh->width);
			pw[3] = x + 1 + (y * mesh->width);
			pw[4] = x + (y * mesh->width); // same as pw[0]
			
			r = (x + y) & 1;
			
			dv[0] = &mesh->verts[pw[r+0]];
			dv[1] = &mesh->verts[pw[r+1]];
			dv[2] = &mesh->verts[pw[r+2]];
			dv[3] = &mesh->verts[pw[r+3]];
			
			planar = PlaneFromPoints( plane, dv[0]->point, dv[1]->point, dv[2]->point );
			if( planar )
			{
				dist = DotProduct( dv[1]->point, plane ) - plane[3];
				if( fabs( dist ) > PLANAR_EPSILON )
					planar = false;
			}
			
			// generate a quad
			if( planar )
			{
				rw.numVerts = 4;
				for( v = 0; v < 4; v++ )
				{
					Mem_Copy( &rw.verts[v], dv[v], sizeof( dvertex_t ));
					
					for( i = 0; i < LM_STYLES; i++ )
					{
						radVertexLuxel = RAD_VERTEX_LUXEL( i, ds->firstvertex + dv[v]->color[0][0] );
						VectorCopy( radVertexLuxel, rw.verts[v].color[i] );
						rw.verts[v].color[i][3] = dv[v]->color[i][3];
					}
				}
				
				// subdivide into area lights
				RadSubdivideDiffuseLight( lightmapNum, ds, lm, si, scale, subdivide, true, &rw, cw );
			}
			else	// generate 2 tris
			{
				rw.numVerts = 3;
				for( t = 0; t < 2; t++ )
				{
					for( v = 0; v < 3 + t; v++ )
					{
						// get "other" triangle (stupid hacky logic, but whatevah)
						if( v == 1 && t == 1 ) v++;

						Mem_Copy( &rw.verts[ v ], dv[ v ], sizeof( dvertex_t ));
						
						for( i = 0; i < LM_STYLES; i++ )
						{
							radVertexLuxel = RAD_VERTEX_LUXEL( i, ds->firstvertex + dv[v]->color[0][0] );
							VectorCopy( radVertexLuxel, rw.verts[v].color[i] );
							rw.verts[v].color[i][3] = dv[v]->color[i][3];
						}
					}
					
					// subdivide into area lights
					RadSubdivideDiffuseLight( lightmapNum, ds, lm, si, scale, subdivide, true, &rw, cw );
				}
			}
		}
	}
	FreeMesh( mesh );
}




/*
=============
RadLight

creates unbounced diffuse lights for a given surface
=============
*/
void RadLight( int num )
{
	int		lightmapNum;
	float		scale, subdivide;
	int		contentFlags, surfaceFlags;
	dsurface_t	*ds;
	surfaceInfo_t	*info;
	rawLightmap_t	*lm;
	bsp_shader_t	*si;
	clipWork_t	cw;
	
	
	ds = &dsurfaces[num];
	info = &surfaceInfos[num];
	lm = info->lm;
	si = info->si;
	scale = si->bounceScale;
	
	// find nodraw bit
	contentFlags = surfaceFlags = 0;
	ApplySurfaceParm( "nodraw", &contentFlags, &surfaceFlags );
	
	if( scale <= 0.0f || (si->surfaceFlags & SURF_SKY) || si->autosprite || (dshaders[ds->shadernum ].contents & contentFlags) || (dshaders[ds->shadernum].surfaceFlags & surfaceFlags))
		return;
	
	// determine how much we need to chop up the surface
	if( si->lightSubdivide )
		subdivide = si->lightSubdivide;
	else subdivide = diffuseSubdivide;
	
	numDiffuseSurfaces++;
	
	// iterate through styles (this could be more efficient, yes)
	for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
	{
		if( ds->lStyles[ lightmapNum ] != LS_NONE && ds->lStyles[lightmapNum] != LS_UNUSED )
		{
			switch( ds->surfaceType )
			{
			case MST_PLANAR:
			case MST_TRIANGLE_SOUP:
				RadLightForTriangles( num, lightmapNum, lm, si, scale, subdivide, &cw );
				break;
			case MST_PATCH:
				RadLightForPatch( num, lightmapNum, lm, si, scale, subdivide, &cw );
				break;
			default: break;
			}
		}
	}
}



/*
=============
RadCreateDiffuseLights

creates lights for unbounced light on surfaces in the bsp
=============
*/

int	iterations = 0;

void RadCreateDiffuseLights( void )
{
	MsgDev( D_NOTE, "--- RadCreateDiffuseLights ---\n" );

	numDiffuseSurfaces = 0;
	numDiffuseLights = 0;
	numBrushDiffuseLights = 0;
	numTriangleDiffuseLights = 0;
	numPatchDiffuseLights = 0;
	
	RunThreadsOnIndividual( numsurfaces, true, RadLight );
	iterations++;
	
	MsgDev( D_INFO, "%8d diffuse surfaces\n", numDiffuseSurfaces );
	MsgDev( D_INFO, "%8d total diffuse lights\n", numDiffuseLights );
	MsgDev( D_INFO, "%8d brush diffuse lights\n", numBrushDiffuseLights );
	MsgDev( D_INFO, "%8d patch diffuse lights\n", numPatchDiffuseLights );
	MsgDev( D_INFO, "%8d triangle diffuse lights\n", numTriangleDiffuseLights );
}