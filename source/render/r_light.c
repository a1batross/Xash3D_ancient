//=======================================================================
//			Copyright XashXT Group 2007 ©
//			  r_light.c - scene lighting
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "matrixlib.h"
#include "const.h"

/*
=======================================================================

 DYNAMIC LIGHTS

=======================================================================
*/
/*
=======================================================================

AMBIENT & DIFFUSE LIGHTING

=======================================================================
*/

static vec3_t	r_pointColor;
static vec3_t	r_lightColors[MAX_VERTICES];

/*
=================
R_RecursiveLightPoint
=================
*/
static bool R_RecursiveLightPoint( node_t *node, const vec3_t start, const vec3_t end )
{
#if 0
	float		front, back, frac;
	int		i, map, size, s, t;
	vec3_t		mid;
	int		side;
	cplane_t		*plane;
	surface_t		*surf;
	mipTex_t		*tex;
	byte		*lm;
	vec3_t		scale;

	if( node->contents != -1 ) return false; // didn't hit anything

	// Calculate mid point
	plane = node->plane;
	if( plane->type < 3 )
	{
		front = start[plane->type] - plane->dist;
		back = end[plane->type] - plane->dist;
	}
	else
	{
		front = DotProduct( start, plane->normal ) - plane->dist;
		back = DotProduct( end, plane->normal ) - plane->dist;
	}

	side = front < 0;
	if((back < 0) == side ) return R_RecursiveLightPoint( node->children[side], start, end );

	frac = front / (front - back);

	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

	// Go down front side	
	if( R_RecursiveLightPoint( node->children[side], start, mid ))
		return true; // hit something

	if((back < 0) == side ) return false; // didn't hit anything

	// check for impact on this node
	surf = r_worldModel->surfaces + node->firstSurface;
	for( i = 0; i < node->numSurfaces; i++, surf++ )
	{
		tex = surf->texInfo;

		if( tex->flags & (SURF_SKY|SURF_WARP|SURF_NODRAW|SURF_NOLIGHTMAP))
			continue;	// no lightmaps

		s = DotProduct(mid, surf->lmVecs[0]) + surf->lmVecs[0][3] - surf->textureMins[0];
		t = DotProduct(mid, surf->lmVecs[1]) + surf->lmVecs[1][3] - surf->textureMins[1];

		if((s < 0 || s > surf->extents[0]) || (t < 0 || t > surf->extents[1]))
			continue;

		s >>= 4;
		t >>= 4;

		if( !surf->lmSamples )
			return true;

		VectorClear( r_pointColor );

		lm = surf->lmSamples + 3 * (t * surf->lmWidth + s);
		size = surf->lmWidth * surf->lmHeight * 3;

		for( map = 0; map < surf->numStyles; map++ )
		{
			VectorScale( r_lightStyles[surf->styles[map]].rgb, r_modulate->value, scale );

			r_pointColor[0] += lm[0] * scale[0];
			r_pointColor[1] += lm[1] * scale[1];
			r_pointColor[2] += lm[2] * scale[2];

			lm += size; // skip to next lightmap
		}
		return true;
	}

	// go down back side
	return R_RecursiveLightPoint( node->children[!side], mid, end );
#endif
	return true;
}

/*
=================
R_LightForPoint
=================
*/
void R_LightForPoint( const vec3_t point, vec3_t ambientLight )
{
	dlight_t		*dl;
	vec3_t		end, dir;
	float		dist, add;
	int		l;

	// Set to full bright if no light data
	if( !r_worldModel || !r_worldModel->lightMaps )
	{
		VectorSet( ambientLight, 1, 1, 1 );
		return;
	}

	// Get lighting at this point
	VectorSet( end, point[0], point[1], point[2] - MAX_WORLD_COORD );
	VectorSet( r_pointColor, 1, 1, 1 );

	R_RecursiveLightPoint( r_worldModel->nodes, point, end );

	VectorCopy( r_pointColor, ambientLight );

	// add dynamic lights
	if( r_dynamiclights->integer )
	{
		for( l = 0, dl = r_dlights; l < r_numDLights; l++, dl++ )
		{
			VectorSubtract(dl->origin, point, dir);
			dist = VectorLength(dir);
			if( !dist || dist > dl->intensity )
				continue;

			add = (dl->intensity - dist);
			VectorMA( ambientLight, add, dl->color, ambientLight );
		}
	}
}

/*
=================
R_ReadLightGrid
=================
*/
static void R_ReadLightGrid( const vec3_t origin, vec3_t lightDir )
{
	vec3_t		vf1, vf2;
	float		scale[8];
	int		vi[3], index[4];
	int		i;

	if( !r_worldModel->lightGrid )
	{
		VectorSet( lightDir, 1, 0, -1 );
		return;
	}

	for( i = 0; i < 3; i++ )
	{
		vf1[i] = (origin[i] - r_worldModel->gridMins[i]) / r_worldModel->gridSize[i];
		vi[i] = (int)vf1[i];
		vf1[i] = vf1[i] - floor(vf1[i]);
		vf2[i] = 1.0 - vf1[i];
	}

	index[0] = vi[2] * r_worldModel->gridBounds[3] + vi[1] * r_worldModel->gridBounds[0] + vi[0];
	index[1] = index[0] + r_worldModel->gridBounds[0];
	index[2] = index[0] + r_worldModel->gridBounds[3];
	index[3] = index[2] + r_worldModel->gridBounds[0];

	for( i = 0; i < 4; i++ )
	{
		if( index[i] < 0 || index[i] >= r_worldModel->gridPoints -1 )
		{
			VectorSet( lightDir, 1, 0, -1 );
			return;
		}
	}

	scale[0] = vf2[0] * vf2[1] * vf2[2];
	scale[1] = vf1[0] * vf2[1] * vf2[2];
	scale[2] = vf2[0] * vf1[1] * vf2[2];
	scale[3] = vf1[0] * vf1[1] * vf2[2];
	scale[4] = vf2[0] * vf2[1] * vf1[2];
	scale[5] = vf1[0] * vf2[1] * vf1[2];
	scale[6] = vf2[0] * vf1[1] * vf1[2];
	scale[7] = vf1[0] * vf1[1] * vf1[2];

	VectorClear(lightDir);

	for( i = 0; i < 4; i++ )
	{
		VectorMA( lightDir, scale[i*2+0], r_worldModel->lightGrid[index[i]+0].lightDir, lightDir );
		VectorMA( lightDir, scale[i*2+1], r_worldModel->lightGrid[index[i]+1].lightDir, lightDir );
	}
}

/*
=================
R_LightDir
=================
*/
void R_LightDir( const vec3_t origin, vec3_t lightDir )
{
	dlight_t		*dl;
	vec3_t		dir;
	float		dist;
	int		l;

	// Get light direction from light grid
	R_ReadLightGrid( origin, lightDir );

	// Add dynamic lights
	if( r_dynamiclights->integer )
	{
		for( l = 0, dl = r_dlights; l < r_numDLights; l++, dl++ )
		{
			VectorSubtract( dl->origin, origin, dir );
			dist = VectorLength( dir );
			if( !dist || dist > dl->intensity )
				continue;

			VectorAdd( lightDir, dir, lightDir );
		}
	}
}

/*
=================
R_LightingAmbient
=================
*/
void R_LightingAmbient( void )
{
	dlight_t		*dl;
	vec3_t		end, dir;
	float		add, dist, radius;
	int		i, l;
	vec3_t		ambientLight;

	// Set to full bright if no light data
	if(( r_refdef.rdflags & RDF_NOWORLDMODEL) || !r_worldModel->lightMaps )
	{
		for( i = 0; i < ref.numVertex; i++ )
		{
			ref.vertsArray[i].color[0] = 1.0f;
			ref.vertsArray[i].color[1] = 1.0f;
			ref.vertsArray[i].color[2] = 1.0f;
			ref.vertsArray[i].color[3] = 1.0f;
		}
		return;
	}

	// Get lighting at this point
	VectorSet( end, m_pCurrentEntity->origin[0], m_pCurrentEntity->origin[1], m_pCurrentEntity->origin[2] - MAX_WORLD_COORD );
	VectorSet( r_pointColor, 1, 1, 1 );

	R_RecursiveLightPoint( r_worldModel->nodes, m_pCurrentEntity->origin, end );
	VectorScale( r_pointColor, r_ambientscale->value, ambientLight );

	// Always have some light
	if( m_pCurrentEntity->renderfx & RF_MINLIGHT )
	{
		for( i = 0; i < 3; i++ )
		{
			if( ambientLight[i] > 0.1 )
				break;
		}

		if( i == 3 ) VectorSet( ambientLight, 0.1, 0.1, 0.1 );
	}

	// add dynamic lights
	if( r_dynamiclights->integer )
	{
		if( m_pCurrentEntity->ent_type == ED_NORMAL )
			radius = m_pCurrentEntity->model->radius;
		else radius = m_pCurrentEntity->radius;

		for( l = 0, dl = r_dlights; l < r_numDLights; l++, dl++ )
		{
			VectorSubtract( dl->origin, m_pCurrentEntity->origin, dir );
			dist = VectorLength( dir );
			if( !dist || dist > dl->intensity + radius )
				continue;

			add = (dl->intensity - dist);
			VectorMA( ambientLight, add, dl->color, ambientLight );
		}
	}

	// normalize and convert to byte
	ColorNormalize( ambientLight, ambientLight );

	for( i = 0; i < ref.numVertex; i++ )
	{
		ref.vertsArray[i].color[0] = ambientLight[0];
		ref.vertsArray[i].color[1] = ambientLight[1];
		ref.vertsArray[i].color[2] = ambientLight[2];
		ref.vertsArray[i].color[3] = 1.0f;
	}
}

/*
=================
R_LightingDiffuse
=================
*/
void R_LightingDiffuse( void )
{
	dlight_t		*dl;
	int		i, l;
	vec3_t		end, dir;
	float		add, dot, dist, intensity, radius;
	vec3_t		ambientLight, directedLight, lightDir;

	// Set to full bright if no light data
	if((r_refdef.rdflags & RDF_NOWORLDMODEL) || !r_worldModel->lightMaps )
	{
		for( i = 0; i < ref.numVertex; i++ )
		{
			ref.vertsArray[i].color[0] = 1.0f;
			ref.vertsArray[i].color[1] = 1.0f;
			ref.vertsArray[i].color[2] = 1.0f;
			ref.vertsArray[i].color[3] = 1.0f;
		}
		return;
	}

	// Get lighting at this point
	VectorSet( end, m_pCurrentEntity->origin[0], m_pCurrentEntity->origin[1], m_pCurrentEntity->origin[2] - MAX_WORLD_COORD );
	VectorSet( r_pointColor, 1, 1, 1 );

	R_RecursiveLightPoint( r_worldModel->nodes, m_pCurrentEntity->origin, end );

	VectorScale( r_pointColor, r_ambientscale->value, ambientLight );
	VectorScale( r_pointColor, r_directedscale->value, directedLight );

	R_ReadLightGrid( m_pCurrentEntity->origin, lightDir );

	// Always have some light
	if( m_pCurrentEntity->renderfx & RF_MINLIGHT )
	{
		for( i = 0; i < 3; i++ )
		{
			if( ambientLight[i] > 0.1 )
				break;
		}

		if( i == 3 ) VectorSet( ambientLight, 0.1, 0.1, 0.1 );
	}

	// Compute lighting at each vertex
	VectorRotate( lightDir, m_pCurrentEntity->axis, dir );
	VectorNormalizeFast( dir );

	for( i = 0; i < ref.numVertex; i++ )
	{
		dot = DotProduct( ref.vertsArray[i].normal, dir );
		if( dot <= 0 )
		{
			VectorCopy( ambientLight, r_lightColors[i] );
			continue;
		}
		VectorMA( ambientLight, dot, directedLight, r_lightColors[i] );
	}

	// add dynamic lights
	if( r_dynamiclights->integer )
	{
		if( m_pCurrentEntity->ent_type == ED_NORMAL )
			radius = m_pCurrentEntity->model->radius;
		else radius = m_pCurrentEntity->radius;

		for( l = 0, dl = r_dlights; l < r_numDLights; l++, dl++ )
		{
			VectorSubtract( dl->origin, m_pCurrentEntity->origin, dir );
			dist = VectorLength( dir );
			if( !dist || dist > dl->intensity + radius )
				continue;

			VectorRotate( dir, m_pCurrentEntity->axis, lightDir );
			intensity = dl->intensity * 8;

			// compute lighting at each vertex
			for( i = 0; i < ref.numVertex; i++ )
			{
				VectorSubtract( lightDir, ref.vertsArray[i].point, dir );
				add = DotProduct( ref.vertsArray[i].normal, dir );
				if( add <= 0 ) continue;

				dot = DotProduct( dir, dir );
				add *= (intensity / dot) * rsqrt( dot );
				VectorMA( r_lightColors[i], add, dl->color, r_lightColors[i] );
			}
		}
	}

	// Normalize and convert to byte
	for (i = 0; i < ref.numVertex; i++)
	{
		ColorNormalize( r_lightColors[i], r_lightColors[i] );
		ref.vertsArray[i].color[0] = r_lightColors[i][0];
		ref.vertsArray[i].color[1] = r_lightColors[i][1];
		ref.vertsArray[i].color[2] = r_lightColors[i][2];
		ref.vertsArray[i].color[3] = 1.0f;
	}
}


/*
=======================================================================

LIGHT SAMPLING

=======================================================================
*/

static vec3_t	r_blockLights[128*128];


/*
=================
R_SetCacheState
=================
*/
static void R_SetCacheState( surface_t *surf )
{
	int	map;

	for( map = 0; map < surf->numStyles; map++ )
		surf->cachedLight[map] = r_lightStyles[surf->styles[map]].white;
}

/*
=================
R_AddDynamicLights
=================
*/
static void R_AddDynamicLights( surface_t *surf )
{
#if 0
	int		l;
	int		s, t, sd, td;
	float		sl, tl, sacc, tacc;
	float		dist, rad, scale;
	cplane_t		*plane;
	vec3_t		origin, tmp, impact;
	mipTex_t		*tex = surf->texInfo;
	dlight_t		*dl;
	float		*bl;

	for( l = 0, dl = r_dlights; l < r_numDLights; l++, dl++ )
	{
		if(!(surf->dlightBits & (1<<l)))
			continue;	// not lit by this light

		if( !AxisCompare( m_pCurrentEntity->axis, axisDefault ))
		{
			VectorSubtract( dl->origin, m_pCurrentEntity->origin, tmp );
			VectorRotate( tmp, m_pCurrentEntity->axis, origin );
		}
		else VectorSubtract( dl->origin, m_pCurrentEntity->origin, origin );

		plane = surf->plane;
		if( plane->type < 3 ) dist = origin[plane->type] - plane->dist;
		else dist = DotProduct( origin, plane->normal ) - plane->dist;

		// rad is now the highest intensity on the plane
		rad = dl->intensity - fabs(dist);
		if( rad < 0 ) continue;

		if( plane->type < 3 )
		{
			VectorCopy( origin, impact );
			impact[plane->type] -= dist;
		}
		else VectorMA( origin, -dist, plane->normal, impact );

		sl = DotProduct(impact, tex->vecs[0]) + tex->vecs[0][3] - surf->textureMins[0];
		tl = DotProduct(impact, tex->vecs[1]) + tex->vecs[1][3] - surf->textureMins[1];

		bl = (float *)r_blockLights;

		for( t = 0, tacc = 0; t < surf->lmHeight; t++, tacc += 16 )
		{
			td = tl - tacc;
			if( td < 0 ) td = -td;

			for( s = 0, sacc = 0; s < surf->lmWidth; s++, sacc += 16 )
			{
				sd = sl - sacc;
				if( sd < 0 ) sd = -sd;

				if( sd > td ) dist = sd + (td >> 1);
				else dist = td + (sd >> 1);

				if( dist < rad )
				{
					scale = rad - dist;
					bl[0] += dl->color[0] * scale;
					bl[1] += dl->color[1] * scale;
					bl[2] += dl->color[2] * scale;
				}
				bl += 3;
			}
		}
	}
#endif
}

/*
=================
R_BuildLightmap

Combine and scale multiple lightmaps into the floating format in r_blockLights
=================
*/
static void R_BuildLightmap( surface_t *surf, byte *dest, int stride )
{
#if 0
	int	i, map, size, s, t;
	vec3_t	scale;
	float	*bl;
	byte	*lm;

	lm = surf->lmSamples;
	size = surf->lmWidth * surf->lmHeight;

	if( !lm )
	{
		// set to full bright if no light data
		for( i = 0, bl = (float *)r_blockLights; i < size; i++, bl += 3 )
		{
			bl[0] = 255;
			bl[1] = 255;
			bl[2] = 255;
		}
	}
	else
	{
		// add all the lightmaps
		VectorScale( r_lightStyles[surf->styles[0]].rgb, r_modulate->value, scale );

		for( i = 0, bl = (float *)r_blockLights; i < size; i++, bl += 3, lm += 3 )
		{
			bl[0] = lm[0] * scale[0];
			bl[1] = lm[1] * scale[1];
			bl[2] = lm[2] * scale[2];
		}

		if( surf->numStyles > 1 )
		{
			for( map = 1; map < surf->numStyles; map++ )
			{
				VectorScale( r_lightStyles[surf->styles[map]].rgb, r_modulate->value, scale );
				for( i = 0, bl = (float *)r_blockLights; i < size; i++, bl += 3, lm += 3 )
				{
					bl[0] += lm[0] * scale[0];
					bl[1] += lm[1] * scale[1];
					bl[2] += lm[2] * scale[2];
				}
			}
		}

		// add all the dynamic lights
		if( surf->dlightFrame == r_frameCount )
			R_AddDynamicLights( surf );
	}

	// put into texture format
	stride -= (surf->lmWidth<<2);
	bl = (float *)r_blockLights;

	for( t = 0; t < surf->lmHeight; t++ )
	{
		for( s = 0; s < surf->lmWidth; s++ )
		{
			ColorNormalize( bl, bl );
			Vector4Set( dest, bl[0], bl[1], bl[2], 255 );			
			bl += 3;
			dest += 4;
		}
		dest += stride;
	}
#endif
}


/*
=======================================================================

LIGHTMAP ALLOCATION

=======================================================================
*/

typedef struct
{
	int	currentNum;
	int	allocated[LIGHTMAP_WIDTH];
	byte	buffer[LIGHTMAP_WIDTH*LIGHTMAP_HEIGHT*4];
} lmState_t;

static lmState_t	r_lmState;

/*
=================
R_BeginBuildingLightmaps
=================
*/
void R_BeginBuildingLightmaps( void )
{
	int	i;
		
	// setup the base lightstyles so the lightmaps 
	// won't have to be regenerated the first time they're seen
	for( i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		r_lightStyles[i].white = 3;
		r_lightStyles[i].rgb[0] = 1;
		r_lightStyles[i].rgb[1] = 1;
		r_lightStyles[i].rgb[2] = 1;
	}
	
	r_lmState.currentNum = -1;
	Mem_Set( r_lmState.allocated, 0, sizeof( r_lmState.allocated ));
	Mem_Set( r_lmState.buffer, 255, sizeof( r_lmState.buffer ));
}

/*
=================
R_EndBuildingLightmaps
=================
*/
void R_EndBuildingLightmaps( void )
{
}

/*
 =================
 R_BuildSurfaceLightmap
 =================
*/
void R_BuildSurfaceLightmap( surface_t *surf )
{
	byte	*base = NULL;

	if(!(surf->texInfo->shader->flags & SHADER_HASLIGHTMAP))
		return;	// no lightmaps

	r_lmState.currentNum = surf->lmNum;

	R_SetCacheState( surf );
	R_BuildLightmap( surf, base, LIGHTMAP_WIDTH * 4 );
}

/*
=================
R_UpdateSurfaceLightmap
=================
*/
void R_UpdateSurfaceLightmap( surface_t *surf )
{
	if( surf->dlightFrame == r_frameCount )
		GL_BindTexture( r_dlightTexture );
	else
	{
		GL_BindTexture( r_worldModel->lightMaps[surf->lmNum] );
		R_SetCacheState( surf );
	}

	R_BuildLightmap( surf, r_lmState.buffer, surf->lmWidth * 4 );
	pglTexSubImage2D( GL_TEXTURE_2D, 0, surf->lmS, surf->lmT, surf->lmWidth, surf->lmHeight, GL_RGBA, GL_UNSIGNED_BYTE, r_lmState.buffer );
}