//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	qrad3.c - calcaulate static light
//=======================================================================

#include "bsplib.h"
#include "const.h"

#define EXTRASCALE		2
#define MAX_FILTERS		1024
#define MAX_FACE_POINTS	128
#define MAX_CONTRIBUTIONS	1024

typedef struct
{
	vec3_t		dir;
	vec3_t		color;
} contribution_t;

typedef struct
{
	float		plane[4];
	vec3_t		origin;
	vec3_t		vectors[2];
	bsp_shader_t	*si;
} filter_t;

filter_t	filters[MAX_FILTERS];
int	numFilters;

bool	notrace;
bool	patchshadows;
bool	extra;
bool	extraWide;
bool	lightmapBorder;
bool	noSurfaces;

int	samplesize = 16;		// sample size in units
int	novertexlighting = 0;
int	nogridlighting = 0;

// for run time tweaking of all area sources in the level
float	areaScale = 0.25;

// for run time tweaking of all point sources in the level
float	pointScale = 7500;
bool	exactPointToPolygon = true;
float	formFactorValueScale = 3;
float	linearScale = 1.0 / 8000;
light_t	*lights;
int	numPointLights;
int	numAreaLights;
int	c_visible, c_occluded;
int	defaultLightSubdivide = 999;		// vary by surface size?
vec3_t	ambientColor;
vec3_t	surfaceOrigin[MAX_MAP_SURFACES];
int	entitySurface[MAX_MAP_SURFACES];

// these are usually overrided by shader values
vec3_t	sunDirection = { 0.45, 0.3, 0.9 };
vec3_t	sunLight = { 100, 100, 50 };

// g-cont. moved it here to avoid stack overflow problems
byte	occluded[LIGHTMAP_WIDTH*EXTRASCALE][LIGHTMAP_HEIGHT*EXTRASCALE];
vec3_t	color[LIGHTMAP_WIDTH*EXTRASCALE][LIGHTMAP_HEIGHT*EXTRASCALE];

typedef struct
{
	dbrush_t	*b;
	vec3_t	bounds[2];
} skyBrush_t;

int		numSkyBrushes;
skyBrush_t	skyBrushes[MAX_MAP_BRUSHES];

/*
===============================================================
NormalToLatLong

We use two byte encoded normals in some space critical applications.
Lat = 0 at (1,0,0) to 360 (-1,0,0), encoded in 8-bit sine table format
Lng = 0 at (0,0,1) to 180 (0,0,-1), encoded in 8-bit sine table format
===============================================================
*/
void NormalToLatLong( const vec3_t normal, byte bytes[2] )
{
	// check for singularities
	if( normal[0] == 0 && normal[1] == 0 )
	{
		if( normal[2] > 0 )
		{
			bytes[0] = 0;
			bytes[1] = 0;	// lat = 0, long = 0
		}
		else
		{
			bytes[0] = 128;
			bytes[1] = 0;	// lat = 0, long = 128
		}
	}
	else
	{
		int	a, b;

		a = RAD2DEG( atan2( normal[1], normal[0] ) ) * (255.0f / 360.0f );
		a &= 0xff;
		b = RAD2DEG( acos( normal[2] ) ) * ( 255.0f / 360.0f );
		b &= 0xff;
		bytes[0] = b;	// longitude
		bytes[1] = a;	// lattitude
	}
}


/*
===============================================================
the corners of a patch mesh will always be exactly at lightmap samples.
The dimensions of the lightmap will be equal to the average length of the control
mesh in each dimension divided by 2.
The lightmap sample points should correspond to the chosen subdivision points.
===============================================================
*/

/*
===============================================================

SURFACE LOADING

===============================================================
*/
/*
===============
SubdivideAreaLight

Subdivide area lights that are very large
A light that is subdivided will never backsplash, avoiding weird pools of light near edges
===============
*/
void SubdivideAreaLight( bsp_shader_t *ls, winding_t *w, vec3_t normal, float areaSubdivide, bool backsplash )
{
	float		area, value, intensity;
	light_t		*dl, *dl2;
	vec3_t		mins, maxs;
	int		axis;
	winding_t		*front, *back;
	vec3_t		planeNormal;
	float		planeDist;

	if( !w ) return;

	WindingBounds( w, mins, maxs );

	// check for subdivision
	for( axis = 0; axis < 3; axis++ )
	{
		if( maxs[axis] - mins[axis] > areaSubdivide )
		{
			VectorClear( planeNormal );
			planeNormal[axis] = 1;
			planeDist = ( maxs[axis] + mins[axis] ) * 0.5;
			ClipWindingEpsilon ( w, planeNormal, planeDist, ON_EPSILON, &front, &back );
			SubdivideAreaLight( ls, front, normal, areaSubdivide, false );
			SubdivideAreaLight( ls, back, normal, areaSubdivide, false );
			FreeWinding( w );
			return;
		}
	}

	// create a light from this
	area = WindingArea( w );
	if ( area <= 0 || area > 20000000 )
		return;

	numAreaLights++;
	dl = BSP_Malloc(sizeof(*dl));

	dl->next = lights;
	lights = dl;
	dl->type = emit_area;

	WindingCenter( w, dl->origin );
	dl->w = w;
	VectorCopy( normal, dl->normal);
	dl->dist = DotProduct( dl->origin, normal );

	value = ls->value;
	intensity = value * area * areaScale;
	VectorAdd( dl->origin, dl->normal, dl->origin );
	VectorCopy( ls->color, dl->color );

	dl->photons = intensity;

	// emitColor is irrespective of the area
	VectorScale( ls->color, value*formFactorValueScale*areaScale, dl->emitColor );

	dl->si = ls;

	if( ls->contents & CONTENTS_FOG )
	{
		dl->twosided = true;
	}

	// optionally create a point backsplash light
	if( backsplash && ls->backsplashFraction > 0 )
	{
		dl2 = BSP_Malloc(sizeof(*dl));

		dl2->next = lights;
		lights = dl2;
		dl2->type = emit_point;

		VectorMA( dl->origin, ls->backsplashDistance, normal, dl2->origin );

		VectorCopy( ls->color, dl2->color );

		dl2->photons = dl->photons * ls->backsplashFraction;
		dl2->si = ls;
	}
}


/*
===============
CountLightmaps
===============
*/
void CountLightmaps( void )
{
	dsurface_t	*ds;
	int		i, count = 0;

	MsgDev( D_INFO, "--- CountLightmaps ---\n" );

	for( i = 0; i < numsurfaces; i++ )
	{
		// see if this surface is light emiting
		ds = &dsurfaces[i];
		if( ds->lightmapnum > count )
			count = ds->lightmapnum;
	}

	count++;
	lightdatasize = count * LM_SIZE;
	if( lightdatasize > MAX_MAP_LIGHTDATA )
		Sys_Break( "MAX_MAP_LIGHTDATA limit exceeded\n" );

	MsgDev( D_INFO, "%5i dsurfaces\n", numsurfaces );
	MsgDev( D_INFO, "%5i lightmaps\n", count );
}

/*
===============
CreateSurfaceLights

This creates area lights
===============
*/
void CreateSurfaceLights( void )
{
	int		i, j, side;
	dsurface_t	*ds;
	bsp_shader_t	*ls;
	winding_t		*w;
	cFacet_t		*f;
	light_t		*dl;
	vec3_t		origin;
	dvertex_t		*dv;
	int		c_lightSurfaces;
	float		lightSubdivide;
	vec3_t		normal;

	MsgDev( D_INFO, "--- CreateSurfaceLights ---\n" );
	c_lightSurfaces = 0;

	for( i = 0; i < numsurfaces; i++ )
	{
		// see if this surface is light emiting
		ds = &dsurfaces[i];

		ls = FindShader( dshaders[ds->shadernum].name );
		if( ls->value == 0 ) continue;

		// determine how much we need to chop up the surface
		if( ls->lightSubdivide )
		{
			lightSubdivide = ls->lightSubdivide;
		}
		else
		{
			lightSubdivide = defaultLightSubdivide;
		}

		c_lightSurfaces++;

		// an autosprite shader will become
		// a point light instead of an area light
		if ( ls->autosprite )
		{
			// autosprite geometry should only have four vertexes
			if( surfaceTest[i] )
			{
				// curve or misc_model
				f = surfaceTest[i]->facets;
				if( surfaceTest[i]->numFacets != 1 || f->numBoundaries != 4 )
				{
					MsgDev( D_WARN, "surface at (%i %i %i) has autosprite shader but isn't a quad\n",
						(int)f->points[0], (int)f->points[1], (int)f->points[2] );
				}
				VectorAdd( f->points[0], f->points[1], origin );
				VectorAdd( f->points[2], origin, origin );
				VectorAdd( f->points[3], origin, origin );
				VectorScale( origin, 0.25, origin );
			}
			else
			{
				// normal polygon
				dv = &dvertexes[ds->firstvertex];
				if( ds->numvertices != 4 )
				{
					MsgDev( D_WARN, "surface at (%i %i %i) has autosprite shader but %i verts\n",
						(int)dv->point[0], (int)dv->point[1], (int)dv->point[2] );
					continue;
				}

				VectorAdd( dv[0].point, dv[1].point, origin );
				VectorAdd( dv[2].point, origin, origin );
				VectorAdd( dv[3].point, origin, origin );
				VectorScale( origin, 0.25, origin );
			}

			numPointLights++;
			dl = BSP_Malloc(sizeof(*dl));
			dl->next = lights;
			lights = dl;

			VectorCopy( origin, dl->origin );
			VectorCopy( ls->color, dl->color );
			dl->photons = ls->value * pointScale;
			dl->type = emit_point;
			continue;
		}

		// possibly create for both sides of the polygon
		for( side = 0; side <= ls->twoSided; side++ )
		{
			// create area lights
			if( surfaceTest[i] )
			{
				// curve or misc_model
				for( j = 0; j < surfaceTest[i]->numFacets; j++ )
				{
					f = surfaceTest[i]->facets + j;
					w = AllocWinding( f->numBoundaries );
					w->numpoints = f->numBoundaries;
					Mem_Copy( w->p, f->points, f->numBoundaries * 12 );

					VectorCopy( f->surface, normal );
					if( side )
					{
						winding_t	*t;

						t = w;
						w = ReverseWinding( t );
						FreeWinding( t );
						VectorSubtract( vec3_origin, normal, normal );
					}
					SubdivideAreaLight( ls, w, normal, lightSubdivide, true );
				}
			}
			else
			{
				// normal polygon
				w = AllocWinding( ds->numvertices );
				w->numpoints = ds->numvertices;
				for( j = 0 ; j < ds->numvertices ; j++ )
				{
					VectorCopy( dvertexes[ds->firstvertex+j].point, w->p[j] );
				}
				VectorCopy( ds->normal, normal );
				if ( side )
				{
					winding_t	*t;

					t = w;
					w = ReverseWinding( t );
					FreeWinding( t );
					VectorSubtract( vec3_origin, normal, normal );
				}
				SubdivideAreaLight( ls, w, normal, lightSubdivide, true );
			}
		}
	}
	MsgDev( D_INFO, "%5i light emitting surfaces\n", c_lightSurfaces );
}



/*
================
FindSkyBrushes
================
*/
void FindSkyBrushes( void )
{
	int		i, j;
	dbrush_t		*b;
	skyBrush_t	*sb;
	bsp_shader_t	*si;
	dbrushside_t	*s;

	// find the brushes
	for( i = 0; i < numbrushes; i++ )
	{
		b = &dbrushes[i];
		for( j = 0; j < b->numsides; j++ )
		{
			s = &dbrushsides[b->firstside + j];
			if( dshaders[s->shadernum].flags & SURF_SKY )
			{
				sb = &skyBrushes[ numSkyBrushes ];
				sb->b = b;
				sb->bounds[0][0] = -dplanes[dbrushsides[b->firstside + 0].planenum].dist - 1;
				sb->bounds[1][0] = dplanes[dbrushsides[ b->firstside + 1].planenum].dist + 1;
				sb->bounds[0][1] = -dplanes[dbrushsides[b->firstside + 2].planenum].dist - 1;
				sb->bounds[1][1] = dplanes[dbrushsides[ b->firstside + 3].planenum].dist + 1;
				sb->bounds[0][2] = -dplanes[dbrushsides[b->firstside + 4].planenum].dist - 1;
				sb->bounds[1][2] = dplanes[dbrushsides[ b->firstside + 5].planenum].dist + 1;
				numSkyBrushes++;
				break;
			}
		}
	}

	// default
	VectorNormalize( sunDirection );

	// find the sky shader
	for( i = 0; i < numsurfaces; i++ )
	{
		si = FindShader( dshaders[dsurfaces[i].shadernum].name );
		if( si->surfaceFlags & SURF_SKY )
		{
			VectorCopy( si->sunLight, sunLight );
			VectorCopy( si->sunDirection, sunDirection );
			break;
		}
	}
}

/*
=================================================================

  LIGHT SETUP

=================================================================
*/
/*
==================
FindTargetEntity
==================
*/
bsp_entity_t *FindTargetEntity( const char *target )
{
	int		i;
	const char	*n;

	for( i = 0; i < num_entities; i++ )
	{
		n = ValueForKey( &entities[i], "targetname" );
		if( !com.strcmp( n, target ))
			return &entities[i];
	}
	return NULL;
}



/*
=============
CreateEntityLights
=============
*/
void CreateEntityLights( void )
{
	int		i, j;
	light_t		*dl;
	bsp_entity_t	*e, *e2;
	const char	*name;
	const char	*target;
	vec3_t		dest;
	const char	*_color;
	float		intensity;
	int		spawnflags;
	bool		valve_format = false;

	// entities
	for( i = 0; i < num_entities; i++ )
	{
		e = &entities[i];
		j = FloatForKey( e, "mapversion" );
		if( j == VALVE_FORMAT ) valve_format = true;

		name = ValueForKey( e, "classname" );
		if( com.strncmp( name, "light", 5 ))
			continue;

		numPointLights++;
		dl = BSP_Malloc( sizeof( *dl ));
		dl->next = lights;
		lights = dl;

		if( valve_format )
		{
			_color = ValueForKey( e, "_light" );
			if( _color && _color[0] )
			{
				sscanf (_color, "%f %f %f %f", &dl->color[0], &dl->color[1], &dl->color[2], &intensity );

				// convert to default OpenGl scale
				dl->color[0] /= 255.0f;
				dl->color[1] /= 255.0f;
				dl->color[2] /= 255.0f;
				ColorNormalize( dl->color, dl->color );
			}
			else dl->color[0] = dl->color[1] = dl->color[2] = 1.0;
		}
		else
		{
			spawnflags = FloatForKey( e, "spawnflags" );
			if( spawnflags & 1 ) dl->linearLight = true;
			intensity = FloatForKey( e, "light" );
			if( !intensity ) intensity = FloatForKey( e, "_light" );
			if( !intensity ) intensity = 300;
			_color = ValueForKey( e, "_color" );
			if( _color && _color[0] )
			{
				sscanf (_color, "%f %f %f", &dl->color[0],&dl->color[1],&dl->color[2]);
				ColorNormalize (dl->color, dl->color);
			}
			else dl->color[0] = dl->color[1] = dl->color[2] = 1.0;
		}

		GetVectorForKey( e, "origin", dl->origin );
		dl->style = FloatForKey( e, "_style" );
		if( !dl->style ) dl->style = FloatForKey( e, "style" );
		if( dl->style < 0 ) dl->style = 0;

		intensity = intensity * pointScale;
		dl->photons = intensity;

		dl->type = emit_point;

		// lights with a target will be spotlights
		target = ValueForKey( e, "target" );

		if( target[0] )
		{
			float	radius;
			float	dist;

			e2 = FindTargetEntity (target);
			if( !e2 )
			{
				MsgDev( D_WARN, "light at (%i %i %i) has missing target\n",
					(int)dl->origin[0], (int)dl->origin[1], (int)dl->origin[2]);
			}
			else
			{
				GetVectorForKey( e2, "origin", dest );
				VectorSubtract( dest, dl->origin, dl->normal );
				dist = VectorNormalizeLength( dl->normal );
				radius = FloatForKey( e, "radius" );
				if( !radius ) radius = 64;
				if( !dist ) dist = 64;
				dl->radiusByDist = (radius + 16) / dist;
				dl->type = emit_spotlight;
			}
		}
	}
}

//=================================================================

/*
================
SetEntityOrigins

Find the offset values for inline models
================
*/
void SetEntityOrigins( void )
{
	int		i, j;
	bsp_entity_t	*e;
	vec3_t		origin;
	const char	*key;
	int		modelnum;
	dmodel_t		*dm;

	for( i = 0; i < num_entities; i++ )
	{
		e = &entities[i];
		key = ValueForKey( e, "model" );
		if( key[0] != '*' ) continue;

		modelnum = com.atoi( key + 1 );
		dm = &dmodels[modelnum];

		// set entity surface to true for all surfaces for this model
		for( j = 0; j < dm->numfaces; j++ )
			entitySurface[dm->firstface + j] = true;

		key = ValueForKey( e, "origin" );
		if( !key[0] ) continue;
		GetVectorForKey( e, "origin", origin );

		// set origin for all surfaces for this model
		for( j = 0 ; j < dm->numfaces; j++ )
		{
			VectorCopy( origin, surfaceOrigin[dm->firstface + j] );
		}
	}
}


/*
=================================================================


=================================================================
*/
/*
================
PointToPolygonFormFactor
================
*/
float PointToPolygonFormFactor( const vec3_t point, const vec3_t normal, const winding_t *w )
{
	vec3_t		triVector, triNormal;
	int		i, j;
	vec3_t		dirs[MAX_POINTS_ON_WINDING];
	float		total;
	float		dot, angle, facing;

	for( i = 0; i < w->numpoints; i++ )
	{
		VectorSubtract( w->p[i], point, dirs[i] );
		VectorNormalize( dirs[i] );
	}

	// duplicate first vertex to avoid mod operation
	VectorCopy( dirs[0], dirs[i] );

	total = 0;
	for( i = 0; i < w->numpoints; i++ )
	{
		j = i + 1;
		dot = DotProduct( dirs[i], dirs[j] );

		// roundoff can cause slight creep, which gives an IND from acos
		if( dot > 1.0 ) dot = 1.0;
		else if( dot < -1.0 ) dot = -1.0;
		
		angle = acos( dot );
		CrossProduct( dirs[i], dirs[j], triVector );
		VectorCopy( triVector, triNormal );
		if( VectorNormalizeLength( triNormal ) < 0.0001 )
			continue;
		facing = DotProduct( normal, triNormal );
		total += facing * angle;

		if( total > 6.3 || total < -6.3 )
		{
			static bool printed;

			if( !printed )
			{
				printed = true;
				MsgDev( D_WARN, "bad PointToPolygonFormFactor: %f at %1.1f %1.1f %1.1f from %1.1f %1.1f %1.1f\n",
					total, w->p[i][0], w->p[i][1], w->p[i][2], point[0], point[1], point[2] );
			}
			return 0;
		}

	}
	total /= 2*3.141592657;	// now in the range of 0 to 1 over the entire incoming hemisphere

	return total;
}


/*
================
FilterTrace

Returns 0 to 1.0 filter fractions for the given trace
================
*/
void FilterTrace( const vec3_t start, const vec3_t end, vec3_t filter )
{
	float		d1, d2;
	filter_t		*f;
	int		filterNum;
	vec3_t		point;
	float		frac;
	int			i;
	float		s, t;
	int			u, v;
	int			x, y;
	byte		*pixel;
	float		radius;
	float		len;
	vec3_t		total;

	filter[0] = 1.0;
	filter[1] = 1.0;
	filter[2] = 1.0;

	for ( filterNum = 0 ; filterNum < numFilters ; filterNum++ ) {
		f = &filters[ filterNum ];

		// see if the plane is crossed
		d1 = DotProduct( start, f->plane ) - f->plane[3];
		d2 = DotProduct( end, f->plane ) - f->plane[3];

		if(( d1 < 0 ) == ( d2 < 0 ))
		{
			continue;
		}

		// calculate the crossing point
		frac = d1 / ( d1 - d2 );

		for( i = 0 ; i < 3 ; i++ )
		{
			point[i] = start[i] + frac * ( end[i] - start[i] );
		}

		VectorSubtract( point, f->origin, point );

		s = DotProduct( point, f->vectors[0] );
		t = 1.0 - DotProduct( point, f->vectors[1] );
		if( s < 0 || s >= 1.0 || t < 0 || t >= 1.0 )
		{
			continue;
		}

		// decide the filter size
		radius = 10 * frac;
		len = VectorLength( f->vectors[0] );
		if( !len ) continue;
		radius = radius * len * f->si->width;

		// look up the filter, taking multiple samples
		VectorClear( total );
		for( u = -1; u <= 1; u++ )
		{
			for( v = -1; v <= 1; v++ )
			{
				x = s * f->si->width + u * radius;
				if( x < 0 )
				{
					x = 0;
				}
				if( x >= f->si->width )
				{
					x = f->si->width - 1;
				}
				y = t * f->si->height + v * radius;
				if( y < 0 )
				{
					y = 0;
				}
				if( y >= f->si->height )
				{
					y = f->si->height - 1;
				}

				pixel = f->si->pixels + ( y * f->si->width + x ) * 4;
				total[0] += pixel[0];
				total[1] += pixel[1];
				total[2] += pixel[2];
			}
		}

		filter[0] *= total[0] / (255.0 * 9);
		filter[1] *= total[1] / (255.0 * 9);
		filter[2] *= total[2] / (255.0 * 9);
	}

}

/*
================
SunToPoint

Returns an amount of light to add at the point
================
*/
int c_sunHit, c_sunMiss;
void SunToPoint( const vec3_t origin, traceWork_t *tw, vec3_t addLight )
{
	int		i;
	lighttrace_t	trace;
	skyBrush_t	*b;
	vec3_t		end;

	if( !numSkyBrushes )
	{
		VectorClear( addLight );
		return;
	}

	VectorMA( origin, MAX_WORLD_COORD * 2, sunDirection, end );
	TraceLine( origin, end, &trace, true, tw );

	// see if trace.hit is inside a sky brush
	for( i = 0; i < numSkyBrushes; i++ )
	{
		b = &skyBrushes[ i ];

		// this assumes that sky brushes are axial...
		if( trace.hit[0] < b->bounds[0][0]  || trace.hit[0] > b->bounds[1][0] || trace.hit[1] < b->bounds[0][1]
			|| trace.hit[1] > b->bounds[1][1] || trace.hit[2] < b->bounds[0][2] || trace.hit[2] > b->bounds[1][2] )
		{
			continue;
		}


		// trace again to get intermediate filters
		TraceLine( origin, trace.hit, &trace, true, tw );

		// we hit the sky, so add sunlight
		if( GetNumThreads() == 1 ) c_sunHit++;
		addLight[0] = trace.filter[0] * sunLight[0];
		addLight[1] = trace.filter[1] * sunLight[1];
		addLight[2] = trace.filter[2] * sunLight[2];

		return;
	}

	if( GetNumThreads() == 1 ) c_sunMiss++;
	VectorClear( addLight );
}

/*
================
SunToPlane
================
*/
void SunToPlane( const vec3_t origin, const vec3_t normal, vec3_t color, traceWork_t *tw )
{
	float	angle;
	vec3_t	sunColor;

	if( !numSkyBrushes ) return;

	angle = DotProduct( normal, sunDirection );
	if( angle <= 0 ) return; // facing away

	SunToPoint( origin, tw, sunColor );
	VectorMA( color, angle, sunColor, color );
}

/*
================
LightingAtSample
================
*/
void LightingAtSample( vec3_t origin, vec3_t normal, vec3_t color, bool testOcclusion, bool forceSunLight, traceWork_t *tw )
{
	light_t		*light;
	lighttrace_t	trace;
	float		angle;
	float		add;
	float		dist;
	vec3_t		dir;

	VectorCopy( ambientColor, color );

	// trace to all the lights
	for( light = lights; light; light = light->next )
	{

		// if the light is behind the surface
		if( DotProduct( light->origin, normal ) - DotProduct( normal, origin ) < 0 )
			continue;
		// testing exact PTPFF
		if( exactPointToPolygon && light->type == emit_area )
		{
			float		factor;
			float		d;
			vec3_t		pushedOrigin;

			// see if the point is behind the light
			d = DotProduct( origin, light->normal ) - light->dist;
			if( !light->twosided )
			{
				if( d < -1 ) continue; // point is behind light
			}

			// test occlusion and find light filters
			// clip the line, tracing from the surface towards the light
			if( !notrace && testOcclusion )
			{
				TraceLine( origin, light->origin, &trace, false, tw );

				// other light rays must not hit anything
				if( trace.passSolid ) continue;
			}
			else
			{
				trace.filter[0] = 1.0;
				trace.filter[1] = 1.0;
				trace.filter[2] = 1.0;
			}

			// nudge the point so that it is clearly forward of the light
			// so that surfaces meeting a light emiter don't get black edges
			if( d > -8 && d < 8 )
			{
				VectorMA( origin, (8-d), light->normal, pushedOrigin );	
			}
			else
			{
				VectorCopy( origin, pushedOrigin );
			}

			// calculate the contribution
			factor = PointToPolygonFormFactor( pushedOrigin, normal, light->w );
			if( factor <= 0 )
			{
				if( light->twosided )
				{
					factor = -factor;
				}
				else continue;
			}
			color[0] += factor * light->emitColor[0] * trace.filter[0];
			color[1] += factor * light->emitColor[1] * trace.filter[1];
			color[2] += factor * light->emitColor[2] * trace.filter[2];
			continue;
		}

		// calculate the amount of light at this sample
		if( light->type == emit_point )
		{
			VectorSubtract( light->origin, origin, dir );
			dist = VectorNormalizeLength( dir );
			// clamp the distance to prevent super hot spots
			if( dist < 16 )
			{
				dist = 16;
			}
			angle = DotProduct( normal, dir );
			if( light->linearLight )
			{
				add = angle * light->photons * linearScale - dist;
				if( add < 0 ) add = 0;
			}
			else
			{
				add = light->photons / ( dist * dist ) * angle;
			}
		}
		else if( light->type == emit_spotlight )
		{
			float	distByNormal;
			vec3_t	pointAtDist;
			float	radiusAtDist;
			float	sampleRadius;
			vec3_t	distToSample;
			float	coneScale;

			VectorSubtract( light->origin, origin, dir );

			distByNormal = -DotProduct( dir, light->normal );
			if( distByNormal < 0 )
			{
				continue;
			}
			VectorMA( light->origin, distByNormal, light->normal, pointAtDist );
			radiusAtDist = light->radiusByDist * distByNormal;

			VectorSubtract( origin, pointAtDist, distToSample );
			sampleRadius = VectorLength( distToSample );

			if( sampleRadius >= radiusAtDist )
			{
				continue;		// outside the cone
			}
			if( sampleRadius <= radiusAtDist - 32 )
			{
				coneScale = 1.0;	// fully inside
			}
			else
			{
				coneScale = ( radiusAtDist - sampleRadius ) / 32.0;
			}
			
			dist = VectorNormalizeLength( dir );
			// clamp the distance to prevent super hot spots
			if( dist < 16 ) dist = 16;
			angle = DotProduct( normal, dir );
			add = light->photons / ( dist * dist ) * angle * coneScale;

		}
		else if( light->type == emit_area )
		{
			VectorSubtract( light->origin, origin, dir );
			dist = VectorNormalizeLength( dir );

			// clamp the distance to prevent super hot spots
			if( dist < 16 )
			{
				dist = 16;
			}
			angle = DotProduct( normal, dir );
			if( angle <= 0 )
			{
				continue;
			}
			angle *= -DotProduct( light->normal, dir );
			if( angle <= 0 )
			{
				continue;
			}

			if( light->linearLight )
			{
				add = angle * light->photons * linearScale - dist;
				if( add < 0 ) add = 0;
			}
			else
			{
				add = light->photons / ( dist * dist ) * angle;
			}
		}

		if( add <= 1.0 ) continue;

		// clip the line, tracing from the surface towards the light
		if( !notrace && testOcclusion )
		{
			TraceLine( origin, light->origin, &trace, false, tw );

			// other light rays must not hit anything
			if( trace.passSolid ) continue;
		}
		else
		{
			trace.filter[0] = 1;
			trace.filter[1] = 1;
			trace.filter[2] = 1;
		}
		
		// add the result
		color[0] += add * light->color[0] * trace.filter[0];
		color[1] += add * light->color[1] * trace.filter[1];
		color[2] += add * light->color[2] * trace.filter[2];
	}

	// trace directly to the sun
	if( testOcclusion || forceSunLight )
	{
		SunToPlane( origin, normal, color, tw );
	}
}

/*
==============
ColorToBytes
==============
*/
void ColorToBytes( const float *color, byte *colorBytes )
{
	float	max;
	vec3_t	sample;

	VectorCopy( color, sample );

	// clamp with color normalization
	max = sample[0];
	if( sample[1] > max )
	{
		max = sample[1];
	}
	if( sample[2] > max )
	{
		max = sample[2];
	}
	if( max > 255 )
	{
		VectorScale( sample, 255 / max, sample );
	}

	colorBytes[0] = sample[0];
	colorBytes[1] = sample[1];
	colorBytes[2] = sample[2];
}

/*
=============
TraceLtm
=============
*/
void TraceLtm( int num )
{
	dsurface_t	*ds;
	int		i, j, k;
	int		x, y;
	int		position, numPositions;
	vec3_t		base, origin, normal;
	traceWork_t	tw;
	vec3_t		average;
	int		count;
	bsp_shader_t	*si;
	static float	nudge[2][9] =
	{
	{ 0, -1, 0, 1, -1, 1, -1, 0, 1 },
	{ 0, -1, -1, -1, 0, 0, 1, 1, 1 }
	};
	int		sampleWidth, sampleHeight, ssize;
	vec3_t		lightmapOrigin, lightmapVecs[2];

	memset( occluded, 0, LIGHTMAP_WIDTH*EXTRASCALE*LIGHTMAP_HEIGHT*EXTRASCALE );
	memset( color, 0, LIGHTMAP_WIDTH*EXTRASCALE*LIGHTMAP_HEIGHT*EXTRASCALE );

	ds = &dsurfaces[num];
	si = FindShader( dshaders[ds->shadernum].name );
	
	if( ds->lightmapnum == -1 ) return; // doesn't need lighting at all

	si = FindShader( dshaders[ds->shadernum].name );
	ssize = samplesize;
	if( si->lightmapSampleSize )
		ssize = si->lightmapSampleSize;

	VectorCopy( ds->normal, normal );

	if( !extra )
	{
		VectorCopy( ds->origin, lightmapOrigin );
		VectorCopy( ds->vecs[0], lightmapVecs[0] );
		VectorCopy( ds->vecs[1], lightmapVecs[1] );
	}
	else
	{
		// sample at a closer spacing for antialiasing
		VectorCopy( ds->origin, lightmapOrigin );
		VectorScale( ds->vecs[0], 0.5, lightmapVecs[0] );
		VectorScale( ds->vecs[1], 0.5, lightmapVecs[1] );
		VectorMA( lightmapOrigin, -0.5, lightmapVecs[0], lightmapOrigin );
		VectorMA( lightmapOrigin, -0.5, lightmapVecs[1], lightmapOrigin );
	}

	if( extra )
	{
		sampleWidth = ds->lm_size[0] * 2;
		sampleHeight = ds->lm_size[1] * 2;
	}
	else
	{
		sampleWidth = ds->lm_size[0];
		sampleHeight = ds->lm_size[1];
	}

	memset ( color, 0, sizeof( color ) );

	// determine which samples are occluded
	memset( occluded, 0, sizeof( occluded ));
	for( i = 0; i < sampleWidth; i++ )
	{
		for( j = 0; j < sampleHeight; j++ )
		{

			numPositions = 9;
			for( k = 0; k < 3; k++ )
			{
				base[k] = lightmapOrigin[k] + normal[k] + i * lightmapVecs[0][k] + j * lightmapVecs[1][k];
			}
			VectorAdd( base, surfaceOrigin[ num ], base );

			// we may need to slightly nudge the sample point
			// if directly on a wall
			for( position = 0; position < numPositions; position++ )
			{
				// calculate lightmap sample position
				for( k = 0; k < 3; k++ )
				{
					origin[k] = base[k] + ( nudge[0][position]/16 ) * lightmapVecs[0][k] + ( nudge[1][position]/16 ) * lightmapVecs[1][k];
				}

				if( notrace ) break;
				if( !PointInSolid( origin ))
				{
					break;
				}
			}

			// if none of the nudges worked, this sample is occluded
			if( position == numPositions )
			{
				occluded[i][j] = true;
				if( GetNumThreads() == 1 )
				{
					c_occluded++;
				}
				continue;
			}
			
			if( GetNumThreads() == 1 )
			{
				c_visible++;
			}
			occluded[i][j] = false;
			LightingAtSample( origin, normal, color[i][j], true, false, &tw );
		}
	}

	// calculate average values for occluded samples
	for( i = 0; i < sampleWidth; i++ )
	{
		for( j = 0; j < sampleHeight; j++ )
		{
			if( !occluded[i][j] )
			{
				continue;
			}
			// scan all surrounding samples
			count = 0;
			VectorClear( average );
			for( x = -1; x <= 1; x++ )
			{
				for( y = -1; y <= 1; y++ )
				{
					if( i + x < 0 || i + x >= sampleWidth )
					{
						continue;
					}
					if( j + y < 0 || j + y >= sampleHeight )
					{
						continue;
					}
					if( occluded[i+x][j+y] )
					{
						continue;
					}
					count++;
					VectorAdd( color[i+x][j+y], average, average );
				}
			}
			if( count )
			{
				VectorScale( average, 1.0 / count, color[i][j] );
			}
		}
	}

	// average together the values if we are extra sampling
	if( ds->lm_size[0] != sampleWidth )
	{
		for( i = 0; i < ds->lm_size[0]; i++ )
		{
			for( j = 0; j < ds->lm_size[1]; j++ )
			{
				for( k = 0; k < 3; k++ )
				{
					float	value, coverage;

					value = color[i*2][j*2][k] + color[i*2][j*2+1][k] + color[i*2+1][j*2][k] + color[i*2+1][j*2+1][k];
					coverage = 4;
					if( extraWide )
					{
						// wider than box filter
						if( i > 0 )
						{
							value += color[i*2-1][j*2][k] + color[i*2-1][j*2+1][k];
							value += color[i*2-2][j*2][k] + color[i*2-2][j*2+1][k];
							coverage += 4;
						}
						if( i < ds->lm_size[0] - 1 )
						{
							value += color[i*2+2][j*2][k] + color[i*2+2][j*2+1][k];
							value += color[i*2+3][j*2][k] + color[i*2+3][j*2+1][k];
							coverage += 4;
						}
						if( j > 0 )
						{
							value += color[i*2][j*2-1][k] + color[i*2+1][j*2-1][k];
							value += color[i*2][j*2-2][k] + color[i*2+1][j*2-2][k];
							coverage += 4;
						}
						if( j < ds->lm_size[1] - 1 )
						{
							value += color[i*2][j*2+2][k] + color[i*2+1][j*2+2][k];
							value += color[i*2][j*2+3][k] + color[i*2+1][j*2+3][k];
							coverage += 2;
						}
					}
					color[i][j][k] = value / coverage;
				}
			}
		}
	}

	// optionally create a debugging border around the lightmap
	if( lightmapBorder )
	{
		for( i = 0; i < ds->lm_size[0]; i++ )
		{
			color[i][0][0] = 255;
			color[i][0][1] = 0;
			color[i][0][2] = 0;

			color[i][ds->lm_size[1]-1][0] = 255;
			color[i][ds->lm_size[1]-1][1] = 0;
			color[i][ds->lm_size[1]-1][2] = 0;
		}
		for( i = 0; i < ds->lm_size[1]; i++ )
		{
			color[0][i][0] = 255;
			color[0][i][1] = 0;
			color[0][i][2] = 0;

			color[ds->lm_size[0]-1][i][0] = 255;
			color[ds->lm_size[0]-1][i][1] = 0;
			color[ds->lm_size[0]-1][i][2] = 0;
		}
	}

	// clamp the colors to bytes and store off
	for( i = 0; i < ds->lm_size[0]; i++ )
	{
		for( j = 0; j < ds->lm_size[1]; j++ )
		{
			k = ( ds->lightmapnum * LIGHTMAP_HEIGHT + ds->lm_base[1] + j) * LIGHTMAP_WIDTH + ds->lm_base[0] + i;
			ColorToBytes( color[i][j], dlightdata + k * 3 );
		}
	}
}


//=============================================================================

vec3_t	gridMins;
vec3_t	gridSize = { 64, 64, 128 };
int	gridBounds[3];


/*
========================
LightContributionToPoint
========================
*/
bool LightContributionToPoint( const light_t *light, const vec3_t origin, vec3_t color, traceWork_t *tw )
{
	lighttrace_t	trace;
	float		add = 0;

	VectorClear( color );

	// testing exact PTPFF
	if( exactPointToPolygon && light->type == emit_area )
	{
		float		factor;
		float		d;
		vec3_t		normal;

		// see if the point is behind the light
		d = DotProduct( origin, light->normal ) - light->dist;
		if( !light->twosided )
		{
			if( d < 1 )
			{
				return false;		// point is behind light
			}
		}

		// test occlusion
		// clip the line, tracing from the surface towards the light
		TraceLine( origin, light->origin, &trace, false, tw );
		if( trace.passSolid )
		{
			return false;
		}

		// calculate the contribution
		VectorSubtract( light->origin, origin, normal );
		if( VectorNormalizeLength( normal ) == 0 )
		{
			return false;
		}
		factor = PointToPolygonFormFactor( origin, normal, light->w );
		if( factor <= 0 )
		{
			if( light->twosided )
			{
				factor = -factor;
			}
			else
			{
				return false;
			}
		}
		VectorScale( light->emitColor, factor, color );
		return true;
	}

	// calculate the amount of light at this sample
	if( light->type == emit_point || light->type == emit_spotlight )
	{
		vec3_t		dir;
		float		dist;

		VectorSubtract( light->origin, origin, dir );
		dist = VectorLength( dir );
		// clamp the distance to prevent super hot spots
		if( dist < 16 )
		{
			dist = 16;
		}
		if( light->linearLight )
		{
			add = light->photons * linearScale - dist;
			if( add < 0 )
			{
				add = 0;
			}
		}
		else
		{
			add = light->photons / ( dist * dist );
		}
	}
	else
	{
		return false;
	}

	if( add <= 1.0 )
	{
		return false;
	}

	// clip the line, tracing from the surface towards the light
	TraceLine( origin, light->origin, &trace, false, tw );

	// other light rays must not hit anything
	if( trace.passSolid ) return false;

	// add the result
	color[0] = add * light->color[0];
	color[1] = add * light->color[1];
	color[2] = add * light->color[2];

	return true;
}

/*
=============
TraceGrid

Grid samples are for quickly determining the lighting
of dynamically placed entities in the world
=============
*/
void TraceGrid( int num )
{
	int		x, y, z;
	vec3_t		origin;
	light_t		*light;
	vec3_t		color;
	int		mod;
	vec3_t		directedColor;
	vec3_t		summedDir;
	contribution_t	contributions[MAX_CONTRIBUTIONS];
	int		numCon;
	int		i;
	traceWork_t	tw;
	float		addSize;

	mod = num;
	z = mod / ( gridBounds[0] * gridBounds[1] );
	mod -= z * ( gridBounds[0] * gridBounds[1] );

	y = mod / gridBounds[0];
	mod -= y * gridBounds[0];

	x = mod;

	origin[0] = gridMins[0] + x * gridSize[0];
	origin[1] = gridMins[1] + y * gridSize[1];
	origin[2] = gridMins[2] + z * gridSize[2];

	if( PointInSolid( origin ) )
	{
		vec3_t	baseOrigin;
		int	step;

		VectorCopy( origin, baseOrigin );

		// try to nudge the origin around to find a valid point
		for( step = 9; step <= 18; step += 9 )
		{
			for( i = 0; i < 8; i++ )
			{
				VectorCopy( baseOrigin, origin );
				if( i & 1 )
				{
					origin[0] += step;
				}
				else
				{
					origin[0] -= step;
				}

				if( i & 2 )
				{
					origin[1] += step;
				}
				else
				{
					origin[1] -= step;
				}

				if( i & 4 )
				{
					origin[2] += step;
				}
				else
				{
					origin[2] -= step;
				}

				if( !PointInSolid( origin ))
				{
					break;
				}
			}

			if( i != 8 ) break;
		}
		if( step > 18 )
		{
			// can't find a valid point at all
			for( i = 0; i < 8; i++ )
				dlightgrid[num * 8 + i] = 0;
			return;
		}
	}

	VectorClear( summedDir );

	// trace to all the lights

	// find the major light direction, and divide the
	// total light between that along the direction and
	// the remaining in the ambient 
	numCon = 0;
	for( light = lights; light; light = light->next )
	{
		vec3_t		add;
		vec3_t		dir;
		float		addSize;

		if( !LightContributionToPoint( light, origin, add, &tw ))
		{
			continue;
		}

		VectorSubtract( light->origin, origin, dir );
		VectorNormalize( dir );

		VectorCopy( add, contributions[numCon].color );
		VectorCopy( dir, contributions[numCon].dir );
		numCon++;

		addSize = VectorLength( add );
		VectorMA( summedDir, addSize, dir, summedDir );

		if( numCon == MAX_CONTRIBUTIONS - 1 )
			break;
	}

	//
	// trace directly to the sun
	//
	SunToPoint( origin, &tw, color );
	addSize = VectorLength( color );
	if( addSize > 0 )
	{
		VectorCopy( color, contributions[numCon].color );
		VectorCopy( sunDirection, contributions[numCon].dir );
		VectorMA( summedDir, addSize, sunDirection, summedDir );
		numCon++;
	}


	// now that we have identified the primary light direction,
	// go back and seperate all the light into directed and ambient
	VectorNormalize( summedDir );
	VectorCopy( ambientColor, color );
	VectorClear( directedColor );

	for( i = 0; i < numCon; i++ )
	{
		float	d;

		d = DotProduct( contributions[i].dir, summedDir );
		if( d < 0 ) d = 0;

		VectorMA( directedColor, d, contributions[i].color, directedColor );

		// the ambient light will be at 1/4 the value of directed light
		d = 0.25 * ( 1.0 - d );
		VectorMA( color, d, contributions[i].color, color );
	}

	// now do some fudging to keep the ambient from being too low
	VectorMA( color, 0.25, directedColor, color );

	// save the resulting value out
	ColorToBytes( color, dlightgrid + num * 8 );
	ColorToBytes( directedColor, dlightgrid + num * 8 + 3 );

	VectorNormalize( summedDir );
	NormalToLatLong( summedDir, dlightgrid + num * 8 + 6);
}


/*
=============
SetupGrid
=============
*/
void SetupGrid( void )
{
	int	i;
	vec3_t	maxs;

	for( i = 0; i < 3; i++ )
	{
		gridMins[i] = gridSize[i] * ceil( dmodels[0].mins[i] / gridSize[i] );
		maxs[i] = gridSize[i] * floor( dmodels[0].maxs[i] / gridSize[i] );
		gridBounds[i] = (maxs[i] - gridMins[i]) / gridSize[i] + 1;
	}

	numgridpoints = gridBounds[0] * gridBounds[1] * gridBounds[2];
	if( numgridpoints * sizeof(dlightgrid_t) >= MAX_MAP_LIGHTGRID )
		Sys_Break( "MAX_MAP_LIGHTGRID limit exceeded\n" );
	Msg( "%5i gridpoints\n", numgridpoints );
}

//=============================================================================

/*
=============
RemoveLightsInSolid
=============
*/
void RemoveLightsInSolid( void )
{
	light_t	*light, *prev;
	int	numsolid = 0;

	prev = NULL;
	for( light = lights; light;  )
	{
		if( PointInSolid( light->origin ))
		{
			if( prev ) prev->next = light->next;
			else lights = light->next;
			if( light->w ) FreeWinding( light->w );
			Mem_Free( light );
			numsolid++;
			if( prev ) light = prev->next;
			else light = lights;
		}
		else
		{
			prev = light;
			light = light->next;
		}
	}
	MsgDev( D_INFO, " %7i lights in solid\n", numsolid );
}

/*
=============
LightWorld
=============
*/
void LightWorld( void )
{
	float	f;

	// determine the number of grid points
	SetupGrid();

	// find the optional world ambient
	GetVectorForKey( &entities[0], "_color", ambientColor );
	f = FloatForKey( &entities[0], "ambient" );
	VectorScale( ambientColor, f, ambientColor );

	// create lights out of patches and lights
	MsgDev( D_INFO, "--- CreateLights ---\n" );
	CreateEntityLights();
	Msg("%i point lights\n", numPointLights);
	Msg("%i area lights\n", numAreaLights);

	if( !nogridlighting )
	{
		MsgDev( D_INFO, "--- TraceGrid ---\n" );
		RunThreadsOnIndividual( numgridpoints, true, TraceGrid );
		MsgDev( D_INFO, "%i x %i x %i = %i grid\n", gridBounds[0], gridBounds[1], gridBounds[2], numgridpoints );
	}

	MsgDev( D_INFO, "--- TraceLtm ---\n" );
	RunThreadsOnIndividual( numsurfaces, true, TraceLtm );
	Msg( "%5i visible samples\n", c_visible );
	Msg( "%5i occluded samples\n", c_occluded );
}

/*
========
WradMain

========
*/
void WradMain ( bool option )
{
	string		cmdparm;
	const char	*value;

	if(!LoadBSPFile())
	{
		// map not exist, create it
		WbspMain( false );
		LoadBSPFile();
	}

	extra = option;
	if( FS_GetParmFromCmdLine("-area", cmdparm ))
		areaScale *= com.atof( cmdparm );
	if( FS_GetParmFromCmdLine("-point", cmdparm ))
		pointScale *= com.atof( cmdparm );
	if( FS_CheckParm( "-notrace" )) notrace = true;
	if( FS_CheckParm( "-patchshadows" )) patchshadows = true;
	if( FS_CheckParm( "-extrawide" )) extra = extraWide = true;
	
	samplesize = bsp_lightmap_size->integer;
	if( samplesize < 1 ) samplesize = 1;

	Msg("---- Radiocity ---- [%s]\n", extra ? "extra" : "normal" );

	FindSkyBrushes();
	ParseEntities();

	value = ValueForKey( &entities[0], "gridsize" );
	if( com.strlen( value )) sscanf( value, "%f %f %f", &gridSize[0], &gridSize[1], &gridSize[2] );

	InitTrace();
	SetEntityOrigins();
	CountLightmaps();
	CreateSurfaceLights();

	LightWorld();
	WriteBSPFile();
}