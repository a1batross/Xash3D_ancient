//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	qrad3.c - calcaulate static light
//=======================================================================

#include "bsplib.h"
#include "const.h"

#define APPROX_BOUNCE	1.0f
#define ONE_OVER_2PI	0.159154942f	// (1.0f / (2.0f * 3.141592657f))
#define MAX_CONTRIBUTIONS	1024

typedef struct
{
	vec3_t		dir;
	vec3_t		color;
	int		style;
} contribution_t;

bool	notrace;
bool	patchshadows;
bool	extra;
bool	extraWide;
bool	noSurfaces;

bool	noGridLighting = false;
bool	exactPointToPolygon = true;
vec3_t	ambientColor;
bool	wolfLight = false;	// g-cont. compare with q3a style
bool	shade = false;

light_t	*lights;
int	numLights;
int	numSunLights;
int	numSpotLights;
int	numCulledLights;
int	lightsPlaneCulled;
int	lightsBoundsCulled;
int	lightsEnvelopeCulled;
int	numPointLights;
int	lightsClusterCulled;	
int	gridBoundsCulled;
int	gridEnvelopeCulled;
int		numRawGridPoints = 0;
rawGridPoint_t	*rawGridPoints = NULL;
vec3_t		gridMins;
int		gridBounds[3];
vec3_t		gridSize = { 64, 64, 128 };

/*
=============
NormalToLatLong

We use two byte encoded normals in some space critical applications.
Lat = 0 at (1,0,0) to 360 (-1,0,0), encoded in 8-bit sine table format
Lng = 0 at (0,0,1) to 180 (0,0,-1), encoded in 8-bit sine table format
=============
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

		a = (int)( RAD2DEG(atan2( normal[1], normal[0] )) * (255.0f/360.0f ));
		a &= 0xff;

		b = (int)( RAD2DEG(acos( normal[2] )) * ( 255.0f/360.0f ));
		b &= 0xff;

		bytes[0] = b;	// longitude
		bytes[1] = a;	// lattitude
	}
}

/*
=============
CreateSunLight

this creates a sun light
=============
*/
static void CreateSunLight( sun_t *sun )
{
	int	i;
	float	photons, d, angle, elevation, da, de;
	vec3_t	direction;
	light_t	*light;
	
	if( sun == NULL ) return;
	
	if( sun->numSamples < 1 )
		sun->numSamples = 1;
	
	photons = sun->photons / sun->numSamples;
	
	for( i = 0; i < sun->numSamples; i++ )
	{
		if( i == 0 ) VectorCopy( sun->direction, direction );
		else
		{
			d = sqrt( sun->direction[0] * sun->direction[0] + sun->direction[1] * sun->direction[1] );
			angle = atan2( sun->direction[1], sun->direction[0] );
			elevation = atan2( sun->direction[2], d );
			
			/* jitter the angles (loop to keep random sample within sun->deviance steridians) */
			do
			{
				da = (RANDOM_FLOAT( 0, 1.0f ) * 2.0f - 1.0f) * sun->deviance;
				de = (RANDOM_FLOAT( 0, 1.0f ) * 2.0f - 1.0f) * sun->deviance;
			}
			while( (da * da + de * de) > (sun->deviance * sun->deviance));
			angle += da;
			elevation += de;
			
			direction[0] = cos( angle ) * cos( elevation );
			direction[1] = sin( angle ) * cos( elevation );
			direction[2] = sin( elevation );
		}
		
		numSunLights++;
		light = BSP_Malloc( sizeof( *light ));
		memset( light, 0, sizeof( *light ));
		light->next = lights;
		lights = light;
		
		light->flags = LIGHT_SUN_DEFAULT;
		light->type = emit_sun;
		light->fade = 1.0f;
		light->falloffTolerance = falloffTolerance;
		light->filterRadius = sun->filterRadius / sun->numSamples;
		light->style = noStyles ? LS_NORMAL : sun->style;
		
		// set the light's position out to infinity
		VectorMA( vec3_origin, (MAX_WORLD_COORD * 8.0f), direction, light->origin );
		
		// set the facing to be the inverse of the sun direction
		VectorScale( direction, -1.0, light->normal );
		light->dist = DotProduct( light->origin, light->normal );
		
		VectorCopy( sun->color, light->color );
		light->photons = photons * skyScale;
	}

	// another sun?
	if( sun->next != NULL ) CreateSunLight( sun->next );
}


/*
=============
CreateSkyLights

simulates sky light with multiple suns
=============
*/
static void CreateSkyLights( vec3_t color, float value, int iterations, float filterRadius, int style )
{
	int	i, j, numSuns;
	int	angleSteps, elevationSteps;
	float	angle, elevation;
	float	angleStep, elevationStep;
	float	step, start;
	sun_t	sun;
	
	if( value <= 0.0f || iterations < 2 )
		return;
	
	step = 2.0f / (iterations - 1);
	start = -1.0f;
	
	VectorCopy( color, sun.color );
	sun.deviance = 0.0f;
	sun.filterRadius = filterRadius;
	sun.numSamples = 1;
	sun.style = noStyles ? LS_NORMAL : style;
	sun.next = NULL;
	
	elevationSteps = iterations - 1;
	angleSteps = elevationSteps * 4;
	angle = 0.0f;
	elevationStep = DEG2RAD( 90.0f / iterations );	/* skip elevation 0 */
	angleStep = DEG2RAD( 360.0f / angleSteps );
	
	numSuns = angleSteps * elevationSteps + 1;
	sun.photons = value / numSuns;
	
	elevation = elevationStep * 0.5f;
	angle = 0.0f;
	for( i = 0, elevation = elevationStep * 0.5f; i < elevationSteps; i++ )
	{
		for( j = 0; j < angleSteps; j++ )
		{
			sun.direction[0] = cos( angle ) * cos( elevation );
			sun.direction[1] = sin( angle ) * cos( elevation );
			sun.direction[2] = sin( elevation );
			CreateSunLight( &sun );
			angle += angleStep;
		}
			
		elevation += elevationStep;
		angle += angleStep / elevationSteps;
	}
	
	VectorSet( sun.direction, 0.0f, 0.0f, 1.0f );
	CreateSunLight( &sun );
	
	// short circuit
	return;
}



/*
=============
CreateEntityLights

creates lights from light entities
=============
*/
void CreateEntityLights( void )
{
	int		i, j;
	light_t		*light, *light2;
	bsp_entity_t		*e, *e2;
	const char	*name;
	const char	*target;
	vec3_t		dest;
	const char	*_color;
	float		intensity, scale, deviance, filterRadius;
	int		spawnflags, flags, numSamples;
	bool		junior;

	for( i = 0; i < num_entities; i++ )
	{
		e = &entities[ i ];
		name = ValueForKey( e, "classname" );
		
		// check for lightJunior */
		if( !com.strnicmp( name, "lightJunior", 11 ))
			junior = true;
		else if( !com.strnicmp( name, "light", 5 ))
			junior = false;
		else continue;
		
		// lights with target names (and therefore styles) are only parsed from BSP
		target = ValueForKey( e, "targetname" );
		if( target[0] != '\0' && i >= num_entities )
			continue;
		
		numPointLights++;
		light = BSP_Malloc( sizeof( *light ));
		light->next = lights;
		lights = light;
		
		spawnflags = IntForKey( e, "spawnflags" );
		
		if( wolfLight == false )
		{
			flags = LIGHT_Q3A_DEFAULT;
			
			if( spawnflags & 1 )
			{
				flags |= LIGHT_ATTEN_LINEAR;
				flags &= ~LIGHT_ATTEN_ANGLE;
			}
			
			if( spawnflags & 2 ) flags &= ~LIGHT_ATTEN_ANGLE;
		}
		else
		{
			flags = LIGHT_WOLF_DEFAULT;
			
			// inverse distance squared attenuation?
			if( spawnflags & 1 )
			{
				flags &= ~LIGHT_ATTEN_LINEAR;
				flags |= LIGHT_ATTEN_ANGLE;
			}
			
			// angle attenuate?
			if( spawnflags & 2 ) flags |= LIGHT_ATTEN_ANGLE;
		}
		
		// other flags (borrowed from wolf)
		
		// wolf dark light
		if( (spawnflags & 4) || (spawnflags & 8))
			flags |= LIGHT_DARK;
		
		// nogrid?
		if( spawnflags & 16 ) flags &= ~LIGHT_GRID;
		
		// junior?
		if( junior )
		{
			flags |= LIGHT_GRID;
			flags &= ~LIGHT_SURFACES;
		}
		
		light->flags = flags;
		
		// set fade key (from wolf)
		light->fade = 1.0f;
		if( light->flags & LIGHT_ATTEN_LINEAR )
		{
			light->fade = FloatForKey( e, "fade" );
			if( light->fade == 0.0f ) light->fade = 1.0f;
		}
		
		// set angle scaling (from vlight)
		light->angleScale = FloatForKey( e, "_anglescale" );
		if( light->angleScale != 0.0f ) light->flags |= LIGHT_ATTEN_ANGLE;
		
		GetVectorForKey( e, "origin", light->origin);
		light->style = IntForKey( e, "_style" );
		if( light->style == LS_NORMAL ) light->style = IntForKey( e, "style" );
		if( light->style < LS_NORMAL || light->style >= LS_NONE )
			Sys_Break( "Invalid lightstyle (%d) on entity %d", light->style, i );
		
		if( noStyles ) light->style = LS_NORMAL;
		
		intensity = FloatForKey( e, "_light" );
		if( intensity == 0.0f ) intensity = FloatForKey( e, "light" );
		if( intensity == 0.0f) intensity = 300.0f;
		
		scale = FloatForKey( e, "scale" );
		if( scale == 0.0f ) scale = 1.0f;
		intensity *= scale;
		
		// get deviance and samples
		deviance = FloatForKey( e, "_deviance" );
		if( deviance == 0.0f ) deviance = FloatForKey( e, "_deviation" );
		if( deviance == 0.0f ) deviance = FloatForKey( e, "_jitter" );
		numSamples = IntForKey( e, "_samples" );
		if( deviance < 0.0f || numSamples < 1 )
		{
			deviance = 0.0f;
			numSamples = 1;
		}
		intensity /= numSamples;
		
		// get filter radius
		filterRadius = FloatForKey( e, "_filterradius" );
		if( filterRadius == 0.0f ) filterRadius = FloatForKey( e, "_filteradius" );
		if( filterRadius == 0.0f ) filterRadius = FloatForKey( e, "_filter" );
		if( filterRadius < 0.0f ) filterRadius = 0.0f;
		light->filterRadius = filterRadius;
		
		// set light color
		// FIXME: handle 220 map format
		_color = ValueForKey( e, "_color" );
		if( _color && _color[0] )
		{
			sscanf( _color, "%f %f %f", &light->color[0], &light->color[1], &light->color[2] );
			ColorNormalize( light->color, light->color );
		}
		else light->color[0] = light->color[1] = light->color[2] = 1.0f;
		
		intensity = intensity * pointScale;
		light->photons = intensity;
		
		light->type = emit_point;
		
		// set falloff threshold
		light->falloffTolerance = falloffTolerance / numSamples;
		
		// lights with a target will be spotlights
		target = ValueForKey( e, "target" );
		if( target[0] )
		{
			float		radius;
			float		dist;
			sun_t		sun;
			const char	*_sun;
			
			e2 = FindTargetEntity( target );
			if( e2 == NULL )
			{
				MsgDev( D_WARN, "light at (%i %i %i) has missing target\n",
					(int)light->origin[0], (int)light->origin[1], (int)light->origin[2] );
			}
			else
			{
				numPointLights--;
				numSpotLights++;
				
				GetVectorForKey( e2, "origin", dest );
				VectorSubtract( dest, light->origin, light->normal );
				dist = VectorNormalizeLength( light->normal );
				radius = FloatForKey( e, "radius" );
				if( !radius ) radius = 64;
				if( !dist ) dist = 64;
				light->radiusByDist = (radius + 16) / dist;
				light->type = emit_spotlight;
				
				// wolf mods: spotlights always use nonlinear + angle attenuation
				light->flags &= ~LIGHT_ATTEN_LINEAR;
				light->flags |= LIGHT_ATTEN_ANGLE;
				light->fade = 1.0f;
				
				_sun = ValueForKey( e, "_sun" );
				if( _sun[0] == '1' )
				{
					numSpotLights--;
					
					lights = light->next;
					
					VectorScale( light->normal, -1.0f, sun.direction );
					VectorCopy( light->color, sun.color );
					sun.photons = (intensity / pointScale);
					sun.deviance = deviance / 180.0f * M_PI;
					sun.numSamples = numSamples;
					sun.style = noStyles ? LS_NORMAL : light->style;
					sun.next = NULL;
					
					CreateSunLight( &sun );
					
					BSP_Free( light );
					light = NULL;
					
					// skip the rest of this love story
					continue;
				}
			}
		}
		
		// jitter the light
		for( j = 1; j < numSamples; j++ )
		{
			light2 = BSP_Malloc( sizeof( *light ));
			Mem_Copy( light2, light, sizeof( *light ));
			light2->next = lights;
			lights = light2;
			
			if( light->type == emit_spotlight )
				numSpotLights++;
			else numPointLights++;
			
			// jitter it
			light2->origin[0] = light->origin[0] + (RANDOM_FLOAT( 0, 1.0f ) * 2.0f - 1.0f) * deviance;
			light2->origin[1] = light->origin[1] + (RANDOM_FLOAT( 0, 1.0f ) * 2.0f - 1.0f) * deviance;
			light2->origin[2] = light->origin[2] + (RANDOM_FLOAT( 0, 1.0f ) * 2.0f - 1.0f) * deviance;
		}
	}
}



/*
=============
CreateSurfaceLights

this hijacks the radiosity code to generate surface lights for first pass
=============
*/
void CreateSurfaceLights( void )
{
	int		i;
	dsurface_t	*ds;
	surfaceInfo_t	*info;
	bsp_shader_t	*si;
	light_t		*light;
	float		subdivide;
	vec3_t		origin;
	clipWork_t	cw;
	const char	*nss;
	
	// get sun shader supressor
	nss = ValueForKey( &entities[0], "_noshadersun" );
	
	for( i = 0; i < numsurfaces; i++ )
	{
		ds = &dsurfaces[i];
		info = &surfaceInfos[i];
		si = info->si;
		
		if( si->sun != NULL && nss[0] != '1' )
		{
			MsgDev( D_NOTE, "Sun: %s\n", si->name );
			CreateSunLight( si->sun );
			si->sun = NULL;
		}
		
		if( si->skyLightValue > 0.0f )
		{
			MsgDev( D_NOTE, "Sky: %s\n", si->name );
			CreateSkyLights( si->color, si->skyLightValue, si->skyLightIterations, si->lightFilterRadius, si->lightStyle );
			si->skyLightValue = 0.0f; // FIXME: hack!
		}
		
		if( si->value <= 0 ) continue;
		
		// autosprite shaders become point lights
		if( si->autosprite )
		{
			VectorAdd( info->mins, info->maxs, origin );
			VectorScale( origin, 0.5f, origin );
			
			light = BSP_Malloc( sizeof( *light ) );
			light->next = lights;
			lights = light;
			
			light->flags = LIGHT_Q3A_DEFAULT;
			light->type = emit_point;
			light->photons = si->value * pointScale;
			light->fade = 1.0f;
			light->si = si;
			VectorCopy( origin, light->origin );
			VectorCopy( si->color, light->color );
			light->falloffTolerance = falloffTolerance;
			light->style = si->lightStyle;
			
			// add to point light count and continue
			numPointLights++;
			continue;
		}
		
		if( si->lightSubdivide > 0 ) subdivide = si->lightSubdivide;
		else subdivide = 999; // more values that crashes q3a render
		
		switch( ds->surfaceType )
		{
		case MST_PLANAR:
		case MST_TRIANGLE_SOUP:
			RadLightForTriangles( i, 0, info->lm, si, APPROX_BOUNCE, subdivide, &cw );
			break;
		case MST_PATCH:
			RadLightForPatch( i, 0, info->lm, si, APPROX_BOUNCE, subdivide, &cw );
			break;
		default:	break;
		}
	}
}

/*
=============
SetEntityOrigins

find the offset values for inline models
=============
*/
void SetEntityOrigins( void )
{
	int		i, j, k, f;
	bsp_entity_t	*e;
	vec3_t	 	origin;
	const char 	*key;
	int	 	modelnum;
	dmodel_t		*dm;
	dsurface_t	*ds;

	// copy drawverts into private storage for nefarious purposes
	yDrawVerts = BSP_Malloc( numvertexes * sizeof( dvertex_t ));
	Mem_Copy( yDrawVerts, dvertexes, numvertexes * sizeof( dvertex_t ));
	
	for( i = 0; i < num_entities; i++ )
	{
		e = &entities[i];
		key = ValueForKey( e, "model" );
		if( key[0] != '*' ) continue;
		modelnum = com.atoi( key + 1 );
		dm = &dmodels[ modelnum ];
		
		key = ValueForKey( e, "origin" );
		if( key[0] == '\0' ) continue;
		GetVectorForKey( e, "origin", origin );
		
		// set origin for all surfaces for this model
		for( j = 0; j < dm->numfaces; j++ )
		{
			ds = &dsurfaces[dm->firstface+j];
			for( k = 0; k < ds->numvertices; k++ )
			{
				f = ds->firstvertex + k;
				VectorAdd( origin, dvertexes[f].point, yDrawVerts[f].point );
			}
		}
	}
}

/*
=============
PointToPolygonFormFactor

calculates the area over a point/normal hemisphere a winding covers
FIXME: there has to be a faster way to calculate this
without the expensive per-vert sqrts and transcendental functions
ydnar 2002-09-30: added -faster switch because only 19% deviance > 10%
between this and the approximation
=============
*/
float PointToPolygonFormFactor( const vec3_t point, const vec3_t normal, const winding_t *w )
{
	vec3_t		triVector, triNormal;
	int		i, j;
	vec3_t		dirs[ MAX_POINTS_ON_WINDING ];
	float		total;
	float		dot, angle, facing;
	
	// this is expensive 
	for( i = 0; i < w->numpoints; i++ )
	{
		VectorSubtract( w->p[i], point, dirs[i] );
		VectorNormalize( dirs[i] );
	}
	
	// duplicate first vertex to avoid mod operation
	VectorCopy( dirs[0], dirs[i] );
	
	// calculcate relative area
	total = 0.0f;
	for( i = 0; i < w->numpoints; i++ )
	{
		j = i + 1;
		dot = DotProduct( dirs[i], dirs[j] );
		
		// roundoff can cause slight creep, which gives an IND from acos
		if( dot > 1.0f ) dot = 1.0f;
		else if( dot < -1.0f ) dot = -1.0f;
		
		angle = com.acos( dot );
		
		CrossProduct( dirs[i], dirs[j], triVector );
		VectorCopy( triVector, triNormal );
		if( VectorNormalizeLength( triNormal ) < 0.0001f )
			continue;
		
		facing = DotProduct( normal, triNormal );
		total += facing * angle;
		
		// this was throwing too many errors with radiosity + crappy maps. ignoring it
		if( total > 6.3f || total < -6.3f ) return 0.0f;
	}
	
	// now in the range of 0 to 1 over the entire incoming hemisphere
	total *= ONE_OVER_2PI;
	return total;
}


/*
=============
LightContributionTosample

determines the amount of light reaching a sample (luxel or vertex) from a given light
=============
*/
int LightContributionToSample( lighttrace_t *trace )
{
	light_t			*light;
	float			angle;
	float			add;
	float			dist;

	light = trace->light;
	VectorClear( trace->color );
	
	if(!(light->flags & LIGHT_SURFACES) || light->envelope <= 0.0f )
		return 0;
	
	// do some culling checks
	if( light->type != emit_sun )
	{
		// MrE: if the light is behind the surface
		if( !trace->twoSided )
			if( DotProduct( light->origin, trace->normal ) - DotProduct( trace->origin, trace->normal ) < 0.0f )
				return 0;
		
		if( !ClusterVisible( trace->cluster, light->cluster ) )
			return 0;
	}
	
	// exact point to polygon form factor
	if( light->type == emit_area )
	{
		float	d, factor;
		vec3_t	pushedOrigin;
		
		// project sample point into light plane
		d = DotProduct( trace->origin, light->normal ) - light->dist;
		if( d < 3.0f )
		{
			// sample point behind plane?
			if(!(light->flags & LIGHT_TWOSIDED) && d < -1.0f )
				return 0;
			
			// sample plane coincident?
			if( d > -3.0f && DotProduct( trace->normal, light->normal ) > 0.9f )
				return 0;
		}
		
		// nudge the point so that it is clearly forward of the light
		// so that surfaces meeting a light emiter don't get black edges
		if( d > -8.0f && d < 8.0f )
			VectorMA( trace->origin, (8.0f - d), light->normal, pushedOrigin );				
		else VectorCopy( trace->origin, pushedOrigin );
		
		VectorCopy( light->origin, trace->end );
		dist = SetupTrace( trace );
		if( dist >= light->envelope )
			return 0;
		
		// ptpff approximation
		if( faster )
		{
			angle = DotProduct( trace->normal, trace->direction );
			
			if( trace->twoSided ) angle = fabs( angle );
			
			angle *= -DotProduct( light->normal, trace->direction );
			if( angle == 0.0f ) return 0;
			else if( angle < 0.0f && (trace->twoSided || (light->flags & LIGHT_TWOSIDED)))
				angle = -angle;
			add = light->photons / (dist * dist) * angle;
		}
		else
		{
			// calculate the contribution
			factor = PointToPolygonFormFactor( pushedOrigin, trace->normal, light->w );
			if( factor == 0.0f ) return 0;
			else if( factor < 0.0f )
			{
				if( trace->twoSided || (light->flags & LIGHT_TWOSIDED))
				{
					factor = -factor;

					// push light origin to other side of the plane
					VectorMA( light->origin, -2.0f, light->normal, trace->end );
					dist = SetupTrace( trace );
					if( dist >= light->envelope )
						return 0;
				}
				else return 0;
			}
			add = factor * light->add;
		}
	}
	else if( light->type == emit_point || light->type == emit_spotlight )
	{
		VectorCopy( light->origin, trace->end );
		dist = SetupTrace( trace );
		if( dist >= light->envelope )
			return 0;
		
		// clamp the distance to prevent super hot spots
		if( dist < 16.0f ) dist = 16.0f;
		
		angle = (light->flags & LIGHT_ATTEN_ANGLE) ? DotProduct( trace->normal, trace->direction ) : 1.0f;
		if( light->angleScale != 0.0f )
		{
			angle /= light->angleScale;
			if( angle > 1.0f ) angle = 1.0f;
		}
		
		if( trace->twoSided ) angle = fabs( angle );
		
		if( light->flags & LIGHT_ATTEN_LINEAR )
		{
			add = angle * light->photons * linearScale - (dist * light->fade);
			if( add < 0.0f ) add = 0.0f;
		}
		else add = light->photons / (dist * dist) * angle;
		
		if( light->type == emit_spotlight )
		{
			float	distByNormal, radiusAtDist, sampleRadius;
			vec3_t	pointAtDist, distToSample;
			
			distByNormal = -DotProduct( trace->displacement, light->normal );
			if( distByNormal < 0.0f ) return 0;
			VectorMA( light->origin, distByNormal, light->normal, pointAtDist );
			radiusAtDist = light->radiusByDist * distByNormal;
			VectorSubtract( trace->origin, pointAtDist, distToSample );
			sampleRadius = VectorLength( distToSample );
			
			if( sampleRadius >= radiusAtDist )
				return 0;
			
			if( sampleRadius > (radiusAtDist - 32.0f) )
				add *= ((radiusAtDist - sampleRadius) / 32.0f);
		}
	}
	else if( light->type == emit_sun )
	{
		VectorAdd( trace->origin, light->origin, trace->end );
		dist = SetupTrace( trace );
		
		angle = (light->flags & LIGHT_ATTEN_ANGLE) ? DotProduct( trace->normal, trace->direction ) : 1.0f;
		
		if( trace->twoSided ) angle = fabs( angle );
		
		add = light->photons * angle;
		if( add <= 0.0f ) return 0;
		
		trace->testAll = true;
		VectorScale( light->color, add, trace->color );
		
		if( trace->testOcclusion && !trace->forceSunlight )
		{
			TraceLine( trace );
			if(!(trace->surfaceFlags & SURF_SKY) || trace->opaque )
			{
				VectorClear( trace->color );
				return -1;
			}
		}
		return 1;
	}
	
	if( add <= 0.0f || (add <= light->falloffTolerance && (light->flags & LIGHT_FAST_ACTUAL)) )
		return 0;
	
	trace->testAll = false;
	VectorScale( light->color, add, trace->color );
	
	TraceLine( trace );
	if( trace->passSolid || trace->opaque )
	{
		VectorClear( trace->color );
		return -1;
	}
	return 1;
}



/*
=============
LightingAtSample

determines the amount of light reaching a sample (luxel or vertex)
=============
*/
void LightingAtSample( lighttrace_t *trace, byte styles[LM_STYLES], vec3_t colors[LM_STYLES] )
{
	int	i, lightmapNum;
	
	for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
		VectorClear( colors[lightmapNum] );
	
	if( normalmap )
	{
		colors[0][0] = (trace->normal[0] + 1.0f) * 127.5f;
		colors[0][1] = (trace->normal[1] + 1.0f) * 127.5f;
		colors[0][2] = (trace->normal[2] + 1.0f) * 127.5f;
		return;
	}
	
	if( !bouncing ) VectorCopy( ambientColor, colors[0] );
	
	// trace to all the list of lights pre-stored in tw
	for( i = 0; i < trace->numLights && trace->lights[i] != NULL; i++ )
	{
		trace->light = trace->lights[i];
		
		for( lightmapNum = 0; lightmapNum < LM_STYLES; lightmapNum++ )
		{
			if( styles[lightmapNum] == trace->light->style || styles[lightmapNum] == LS_NONE )
				break;
		}
		
		// max of LM_STYLES (4) styles allowed to hit a sample
		if( lightmapNum >= LM_STYLES ) continue;
		
		// sample light
		LightContributionToSample( trace );
		if( trace->color[0] == 0.0f && trace->color[1] == 0.0f && trace->color[2] == 0.0f )
			continue;
		
		if( trace->light->flags & LIGHT_NEGATIVE ) VectorScale( trace->color, -1.0f, trace->color );
		styles[lightmapNum] = trace->light->style;
		VectorAdd( colors[lightmapNum], trace->color, colors[lightmapNum] );
	}
}

/*
=============
LightContributionToPoint

for a given light, how much light/color reaches a given point in space (with no facing)
NOTE: this is similar to LightContributionToSample but optimized for omnidirectional sampling
=============
*/
int LightContributionToPoint( lighttrace_t *trace )
{
	light_t		*light;
	float		add, dist;
	
	light = trace->light;
	VectorClear( trace->color );
	if(!(light->flags & LIGHT_GRID) || light->envelope <= 0.0f )
		return false;
	
	if( light->type != emit_sun )
	{
		if(!ClusterVisible( trace->cluster, light->cluster ))
			return false;
	}
	
	// check origin against light's pvs envelope
	if( trace->origin[0] > light->maxs[0] || trace->origin[0] < light->mins[0] ||
	trace->origin[1] > light->maxs[1] || trace->origin[1] < light->mins[1] ||
	trace->origin[2] > light->maxs[2] || trace->origin[2] < light->mins[2] )
	{
		gridBoundsCulled++;
		return false;
	}
	
	if( light->type == emit_sun )
		VectorAdd( trace->origin, light->origin, trace->end );
	else VectorCopy( light->origin, trace->end );
	
	dist = SetupTrace( trace );
	
	if( dist > light->envelope )
	{
		gridEnvelopeCulled++;
		return false;
	}
	
	// ptpff approximation
	if( light->type == emit_area && faster )
	{
		// clamp the distance to prevent super hot spots
		if( dist < 16.0f ) dist = 16.0f;
		add = light->photons / (dist * dist);
	}
	
	// exact point to polygon form factor
	else if( light->type == emit_area )
	{
		float	factor, d;
		vec3_t	pushedOrigin;
		
		
		// see if the point is behind the light
		d = DotProduct( trace->origin, light->normal ) - light->dist;
		if( !(light->flags & LIGHT_TWOSIDED) && d < -1.0f )
			return false;
		
		// nudge the point so that it is clearly forward of the light
		// so that surfaces meeting a light emiter don't get black edges
		if( d > -8.0f && d < 8.0f )
			VectorMA( trace->origin, (8.0f - d), light->normal, pushedOrigin );				
		else VectorCopy( trace->origin, pushedOrigin );
		
		// calculate the contribution (ydnar 2002-10-21: [bug 642] bad normal calc)
		factor = PointToPolygonFormFactor( pushedOrigin, trace->direction, light->w );
		if( factor == 0.0f ) return false;
		else if( factor < 0.0f )
		{
			if( light->flags & LIGHT_TWOSIDED )
				factor = -factor;
			else return false;
		}
		add = factor * light->add;
	}
	else if( light->type == emit_point || light->type == emit_spotlight )
	{
		// clamp the distance to prevent super hot spots
		if( dist < 16.0f ) dist = 16.0f;
		
		if( light->flags & LIGHT_ATTEN_LINEAR )
		{
			add = light->photons * linearScale - (dist * light->fade);
			if( add < 0.0f ) add = 0.0f;
		}
		else add = light->photons / (dist * dist);
		
		if( light->type == emit_spotlight )
		{
			float	distByNormal, radiusAtDist, sampleRadius;
			vec3_t	pointAtDist, distToSample;

			distByNormal = -DotProduct( trace->displacement, light->normal );
			if( distByNormal < 0.0f ) return false;
			VectorMA( light->origin, distByNormal, light->normal, pointAtDist );
			radiusAtDist = light->radiusByDist * distByNormal;
			VectorSubtract( trace->origin, pointAtDist, distToSample );
			sampleRadius = VectorLength( distToSample );
			
			if( sampleRadius >= radiusAtDist )
				return false;
			
			if( sampleRadius > (radiusAtDist - 32.0f) )
				add *= ((radiusAtDist - sampleRadius) / 32.0f);
		}
	}
	else if( light->type == emit_sun )
	{
		add = light->photons;
		if( add <= 0.0f )
			return false;
		
		trace->testAll = true;
		VectorScale( light->color, add, trace->color );
		
		if( trace->testOcclusion && !trace->forceSunlight )
		{
			TraceLine( trace );
			if(!(trace->surfaceFlags & SURF_SKY) || trace->opaque )
			{
				VectorClear( trace->color );
				return -1;
			}
		}
		return true;
	}
	else return false;
	
	if( add <= 0.0f || (add <= light->falloffTolerance && (light->flags & LIGHT_FAST_ACTUAL)) )
		return false;
	
	trace->testAll = false;
	VectorScale( light->color, add, trace->color );
	
	TraceLine( trace );
	if( trace->passSolid )
	{
		VectorClear( trace->color );
		return false;
	}
	return true;
}


/*
=============
TraceGrid

grid samples are for quickly determining the lighting
of dynamically placed entities in the world
=============
*/
void TraceGrid( int num )
{
	int		i, j, x, y, z, mod, step, numCon, numStyles;
	float		d;
	vec3_t		baseOrigin, color;
	rawGridPoint_t	*gp;
	dlightgrid_t	*bgp;
	contribution_t	contributions[MAX_CONTRIBUTIONS];
	lighttrace_t	trace;

	gp = &rawGridPoints[num];
	bgp = &dlightgrid[num];
	
	mod = num;
	z = mod / (gridBounds[0] * gridBounds[1]);
	mod -= z * (gridBounds[0] * gridBounds[1]);
	y = mod / gridBounds[0];
	mod -= y * gridBounds[0];
	x = mod;
	
	trace.origin[0] = gridMins[0] + x * gridSize[0];
	trace.origin[1] = gridMins[1] + y * gridSize[1];
	trace.origin[2] = gridMins[2] + z * gridSize[2];
	
	// set inhibit sphere
	if( gridSize[0] > gridSize[1] && gridSize[0] > gridSize[2] )
		trace.inhibitRadius = gridSize[0] * 0.5f;
	else if( gridSize[1] > gridSize[0] && gridSize[1] > gridSize[2] )
		trace.inhibitRadius = gridSize[1] * 0.5f;
	else trace.inhibitRadius = gridSize[2] * 0.5f;
	
	// find point cluster
	trace.cluster = ClusterForPointExt( trace.origin, GRID_EPSILON );
	if( trace.cluster < 0 )
	{
		// try to nudge the origin around to find a valid point
		VectorCopy( trace.origin, baseOrigin );
		for( step = 9; step <= 18; step += 9 )
		{
			for( i = 0; i < 8; i++ )
			{
				VectorCopy( baseOrigin, trace.origin );
				if( i & 1 ) trace.origin[0] += step;
				else trace.origin[0] -= step;
				
				if( i & 2 ) trace.origin[1] += step;
				else trace.origin[1] -= step;
				
				if( i & 4 ) trace.origin[2] += step;
				else trace.origin[2] -= step;
				
				// changed to find cluster num
				trace.cluster = ClusterForPointExt( trace.origin, VERTEX_EPSILON );
				if( trace.cluster >= 0 ) break;
			}
			if( i != 8 ) break;
		}
		
		// can't find a valid point at all
		// FIXME: use sv_stepsize instead of hardcoded value
		if( step > 18 ) return;
	}
	
	trace.testOcclusion = !notrace;
	trace.forceSunlight = false;
	trace.recvShadows = WORLDSPAWN_RECV_SHADOWS;
	trace.numSurfaces = 0;
	trace.surfaces = NULL;
	trace.numLights = 0;
	trace.lights = NULL;
	
	numCon = 0;
	// trace to all the lights, find the major light direction, and divide the
	// total light between that along the direction and the remaining in the ambient

	for( trace.light = lights; trace.light != NULL; trace.light = trace.light->next )
	{
		float	addSize;
		
		if( !LightContributionToPoint( &trace ) )
			continue;
		
		if( trace.light->flags & LIGHT_NEGATIVE )
			VectorScale( trace.color, -1.0f, trace.color );
		
		// add a contribution
		VectorCopy( trace.color, contributions[numCon].color );
		VectorCopy( trace.direction, contributions[numCon].dir );
		contributions[numCon].style = trace.light->style;
		numCon++;
		
		// push average direction around
		addSize = VectorLength( trace.color );
		VectorMA( gp->dir, addSize, trace.direction, gp->dir );
		
		// stop after a while
		if( numCon >= (MAX_CONTRIBUTIONS - 1))
			break;
	}
	
	// normalize to get primary light direction
	VectorNormalize( gp->dir );
	
	// now that we have identified the primary light direction,
	// go back and separate all the light into directed and ambient
	
	numStyles = 1;
	for( i = 0; i < numCon; i++ )
	{
		// get relative directed strength
		d = DotProduct( contributions[i].dir, gp->dir );
		if( d < 0.0f ) d = 0.0f;
		
		// find appropriate style
		for( j = 0; j < numStyles; j++ )
		{
			if( gp->styles[j] == contributions[i].style )
				break;
		}
		
		if( j >= numStyles )
		{
			// add a new style
			if( numStyles < LM_STYLES )
			{
				gp->styles[ numStyles ] = contributions[i].style;
				bgp->styles[ numStyles ] = contributions[i].style;
				numStyles++;
			}
			else j = 0;
		}
		
		// add the directed color
		VectorMA( gp->directed[j], d, contributions[i].color, gp->directed[j] );
		
		// ambient light will be at 1/4 the value of directed light
		// (ydnar: nuke this in favor of more dramatic lighting?)
		d = 0.25f * (1.0f - d);
		VectorMA( gp->ambient[j], d, contributions[i].color, gp->ambient[j] );
	}

	for( i = 0; i < LM_STYLES; i++ )
	{
		// do some fudging to keep the ambient from being too low (2003-07-05: 0.25 -> 0.125)
		if( !bouncing ) VectorMA( gp->ambient[i], 0.125f, gp->directed[i], gp->ambient[i] );
		
		// set minimum light and copy off to bytes
		VectorCopy( gp->ambient[i], color );
		for( j = 0; j < 3; j++ )
			if( color[j] < minGridLight[j] )
				color[j] = minGridLight[j];
		ColorToBytes( color, bgp->ambient[i], 1.0f );
		ColorToBytes( gp->directed[i], bgp->direct[i], 1.0f );
	}

	// store direction
	if( !bouncing ) NormalToLatLong( gp->dir, bgp->latLong );
}


/*
=============
SetupGrid

calculates the size of the lightgrid and allocates memory
=============
*/
void SetupGrid( void )
{
	int		i, j;
	vec3_t		maxs, oldGridSize;
	const char	*value;
	char		temp[MAX_SHADERPATH];
	 
	if( noGridLighting ) return;
	
	value = ValueForKey( &entities[0], "gridsize" );
	if( value[0] != '\0' )
		sscanf( value, "%f %f %f", &gridSize[0], &gridSize[1], &gridSize[2] );
	
	// quantize
	VectorCopy( gridSize, oldGridSize );
	for( i = 0; i < 3; i++ )
		gridSize[i] = gridSize[i] >= 8.0f ? floor( gridSize[i] ) : 8.0f;
	
	// increase gridSize until grid count is smaller than max allowed
	numRawGridPoints = MAX_MAP_LIGHTGRID + 1;
	j = 0;
	while( numRawGridPoints > MAX_MAP_LIGHTGRID )
	{
		for( i = 0; i < 3; i++ )
		{
			gridMins[i] = gridSize[i] * ceil( dmodels[0].mins[i] / gridSize[i] );
			maxs[i] = gridSize[i] * floor( dmodels[0].maxs[i] / gridSize[i] );
			gridBounds[i] = (maxs[i] - gridMins[i]) / gridSize[i] + 1;
		}
	
		numRawGridPoints = gridBounds[0] * gridBounds[1] * gridBounds[2];
		
		if( numRawGridPoints > MAX_MAP_LIGHTGRID ) gridSize[j++%3] += 16.0f;
	}
	
	MsgDev( D_INFO, "Grid size = { %1.0f, %1.0f, %1.0f }\n", gridSize[0], gridSize[1], gridSize[2] );
	
	if( !VectorCompare( gridSize, oldGridSize ) )
	{
		com.sprintf( temp, "%.0f %.0f %.0f", gridSize[0], gridSize[1], gridSize[2] );
		SetKeyValue( &entities[0], "gridsize", (const char*) temp );
		MsgDev( D_NOTE, "Storing adjusted grid size\n" );
	}
	
	// 2nd variable. FIXME: is this silly?
	numgridpoints = numRawGridPoints;
	
	rawGridPoints = BSP_Malloc( numRawGridPoints * sizeof( *rawGridPoints ));

	for( i = 0; i < numRawGridPoints; i++ )
	{
		VectorCopy( ambientColor, rawGridPoints[i].ambient[j] );
		rawGridPoints[i].styles[0] = LS_NORMAL;
		dlightgrid[i].styles[0] = LS_NORMAL;
		for( j = 1; j < LM_STYLES; j++ )
		{
			rawGridPoints[i].styles[j] = LS_NONE;
			dlightgrid[i].styles[j] = LS_NONE;
		}
	}
	
	MsgDev( D_INFO, "%6i grid points\n", numRawGridPoints );
}


/*
=============
RadWorld
=============
*/
void RadWorld( void )
{
	vec3_t		color;
	float		f;
	int		b, bt;
	bool		minVertex, minGrid;
	const char	*value;

	if( shade )
	{
		MsgDev( D_NOTE, "--- SmoothNormals ---\n" );
		SmoothNormals();
	}
	
	MsgDev( D_NOTE, "--- SetupGrid ---\n" );
	SetupGrid();
	
	GetVectorForKey( &entities[0], "_color", color );
	if( VectorLength( color ) == 0.0f ) VectorSet( color, 1.0, 1.0, 1.0 );
	
	f = FloatForKey( &entities[0], "_ambient" );
	if( f == 0.0f ) f = FloatForKey( &entities[0], "ambient" );
	VectorScale( color, f, ambientColor );
	
	minVertex = false;
	value = ValueForKey( &entities[0], "_minvertexlight" );
	if( value[0] != '\0' )
	{
		minVertex = true;
		f = com.atof( value );
		VectorScale( color, f, minVertexLight );
	}
	
	minGrid = false;
	value = ValueForKey( &entities[0], "_mingridlight" );
	if( value[0] != '\0' )
	{
		minGrid = true;
		f = com.atof( value );
		VectorScale( color, f, minGridLight );
	}
	
	value = ValueForKey( &entities[0], "_minlight" );
	if( value[0] != '\0' )
	{
		f = atof( value );
		VectorScale( color, f, minLight );
		if( !minVertex ) VectorScale( color, f, minVertexLight );
		if( !minGrid ) VectorScale( color, f, minGridLight );
	}
	
	// create world lights
	MsgDev( D_NOTE, "--- CreateLights ---\n" );
	CreateEntityLights();
	CreateSurfaceLights();
	MsgDev( D_INFO, "%6i point lights\n", numPointLights );
	MsgDev( D_INFO, "%6i spotlights\n", numSpotLights );
	MsgDev( D_INFO, "%6i diffuse (area) lights\n", numDiffuseLights );
	MsgDev( D_INFO, "%6i sun/sky lights\n", numSunLights );
	
	if( !noGridLighting )
	{
		SetupEnvelopes( true, fastgrid );
		
		MsgDev( D_INFO, "--- TraceGrid ---\n" );
		RunThreadsOnIndividual( numRawGridPoints, true, TraceGrid );

		MsgDev( D_INFO, "%d x %d x %d = %d grid\n", gridBounds[0], gridBounds[1], gridBounds[2], numgridpoints );
		MsgDev( D_NOTE, "%6i grid points envelope culled\n", gridEnvelopeCulled );
		MsgDev( D_NOTE, "%6i grid points bounds culled\n", gridBoundsCulled );
	}
	
	/* map the world luxels */
	MsgDev( D_INFO, "--- MapRawLightmap ---\n" );
	RunThreadsOnIndividual( numRawLightmaps, true, MapRawLightmap );
	MsgDev( D_INFO, "%6i luxels\n", numLuxels );
	MsgDev( D_INFO, "%6i luxels mapped\n", numLuxelsMapped );
	MsgDev( D_INFO, "%6i luxels occluded\n", numLuxelsOccluded );
	
	if( dirty )
	{
		MsgDev( D_INFO, "--- DirtyRawLightmap ---\n" );
		RunThreadsOnIndividual( numRawLightmaps, true, DirtyRawLightmap );
	}
	

	SetupEnvelopes( false, fast );
	lightsPlaneCulled = 0;
	lightsEnvelopeCulled = 0;
	lightsBoundsCulled = 0;
	lightsClusterCulled = 0;
	
	MsgDev( D_INFO, "--- IlluminateRawLightmap ---\n" );
	RunThreadsOnIndividual( numRawLightmaps, true, IlluminateRawLightmap );
	MsgDev( D_INFO, "%6i luxels illuminated\n", numLuxelsIlluminated );
	
	StitchSurfaceLightmaps();
	
	MsgDev( D_INFO, "--- IlluminateVertexes ---\n" );
	RunThreadsOnIndividual( numsurfaces, true, IlluminateVertexes );
	MsgDev( D_INFO, "%6i vertexes illuminated\n", numVertsIlluminated );
	
	MsgDev( D_NOTE, "%6i lights plane culled\n", lightsPlaneCulled );
	MsgDev( D_NOTE, "%6i lights envelope culled\n", lightsEnvelopeCulled );
	MsgDev( D_NOTE, "%6i lights bounds culled\n", lightsBoundsCulled );
	MsgDev( D_NOTE, "%6i lights cluster culled\n", lightsClusterCulled );
	
	// radiosity
	b = 1;
	bt = bounce;
	while( bounce > 0 )
	{
		// store off the bsp between bounces
		StoreSurfaceLightmaps();
		WriteBSPFile();
		
		MsgDev( D_INFO, "\n--- Radiosity (bounce %d of %d) ---\n", b, bt );
		
		bouncing = true;
		VectorClear( ambientColor );
		
		RadFreeLights();
		RadCreateDiffuseLights();
		
		SetupEnvelopes( false, fastbounce );
		if( numLights == 0 )
		{
			MsgDev( D_WARN, "No diffuse light to calculate, ending radiosity.\n" );
			break;
		}
		
		if( bouncegrid )
		{
			gridEnvelopeCulled = 0;
			gridBoundsCulled = 0;
			
			MsgDev( D_NOTE, "--- BounceGrid ---\n" );
			RunThreadsOnIndividual( numRawGridPoints, true, TraceGrid );
			MsgDev( D_NOTE, "%6i grid points envelope culled\n", gridEnvelopeCulled );
			MsgDev( D_NOTE, "%6i grid points bounds culled\n", gridBoundsCulled );
		}
		
		lightsPlaneCulled = 0;
		lightsEnvelopeCulled = 0;
		lightsBoundsCulled = 0;
		lightsClusterCulled = 0;
		
		MsgDev( D_NOTE, "--- IlluminateRawLightmap ---\n" );
		RunThreadsOnIndividual( numRawLightmaps, true, IlluminateRawLightmap );
		MsgDev( D_NOTE, "%6i luxels illuminated\n", numLuxelsIlluminated );
		MsgDev( D_NOTE, "%6i vertexes illuminated\n", numVertsIlluminated );
		
		StitchSurfaceLightmaps();
		
		MsgDev( D_NOTE, "--- IlluminateVertexes ---\n" );
		RunThreadsOnIndividual( numsurfaces, true, IlluminateVertexes );
		MsgDev( D_NOTE, "%6i vertexes illuminated\n", numVertsIlluminated );
		
		MsgDev( D_NOTE, "%6i lights plane culled\n", lightsPlaneCulled );
		MsgDev( D_NOTE, "%6i lights envelope culled\n", lightsEnvelopeCulled );
		MsgDev( D_NOTE, "%6i lights bounds culled\n", lightsBoundsCulled );
		MsgDev( D_NOTE, "%6i lights cluster culled\n", lightsClusterCulled );
		
		bounce--;
		b++;
	}
}


/*
========
WradMain

========
*/
void WradMain ( bool option )
{
	string		cmdparm;

	if(!LoadBSPFile())
	{
		// map not exist, create it
		WbspMain( false );
		LoadBSPFile();
	}
	LoadSurfaceExtraFile();

	if( FS_GetParmFromCmdLine("-point", cmdparm ))
		pointScale *= com.atof( cmdparm );
	if( FS_GetParmFromCmdLine("-area", cmdparm ))
		areaScale *= com.atof( cmdparm );		
	if( FS_GetParmFromCmdLine("-sky", cmdparm ))
		skyScale *= com.atof( cmdparm );		
	if( FS_GetParmFromCmdLine("-bouncescale", cmdparm ))
		bounceScale *= com.atof( cmdparm );
	if( FS_GetParmFromCmdLine("-scale", cmdparm ))
	{
		float f = com.atof( cmdparm );
		pointScale *= f;
		areaScale *= f;
		skyScale *= f;
		bounceScale *= f;
	}
	if( FS_GetParmFromCmdLine("-gamma", cmdparm ))
		lightmapGamma *= com.atof( cmdparm );		
	if( FS_GetParmFromCmdLine("-compensate", cmdparm ))
		lightmapCompensate *= com.atof( cmdparm );		
	if( FS_GetParmFromCmdLine("-bounce", cmdparm ))
		bounce = com.atoi( cmdparm );
	if( FS_GetParmFromCmdLine("-super", cmdparm ))
		superSample = bound( 1, com.atoi( cmdparm ), 10 );	
	if( FS_GetParmFromCmdLine("-samples", cmdparm ))
		lightSamples = com.atoi( cmdparm );
	if( FS_CheckParm( "-filter" )) filter = true;
	if( FS_CheckParm( "-dark" )) dark = true;		
	if( FS_GetParmFromCmdLine("-shadeangle", cmdparm ))
	{
		shadeAngleDegrees = bound( 0.0f, com.atoi( cmdparm ), 360.0f );
		if( shadeAngleDegrees ) shade = true;
	}
	if( FS_GetParmFromCmdLine("-approx", cmdparm ))
		approximateTolerance = bound( 0, com.atoi( cmdparm ), 10 );
	if( FS_CheckParm( "-deluxe" )) deluxemap = true;		
	if( FS_CheckParm( "-bounceonly" )) bounceOnly = true;		
	if( FS_CheckParm( "-nocollapse" )) noCollapse = true;		
	if( FS_CheckParm( "-shade" )) shade = true;		
	if( FS_CheckParm( "-bouncegrid" )) bouncegrid = true;		
	if( FS_CheckParm( "-smooth" )) lightSamples = EXTRA_SCALE;		
	if( FS_CheckParm( "-fast" ))
	{
		fast = true;
		fastgrid = true;
		fastbounce = true;
	}
	if( FS_CheckParm( "-faster" ))		
	{
		faster = true;
		fast = true;
		fastgrid = true;
		fastbounce = true;
	}
	if( FS_CheckParm( "-fastgrid" )) fastgrid = true;
	if( FS_CheckParm( "-fastbounce" )) fastbounce = true;
	if( FS_CheckParm( "-normalmap" )) normalmap = true;
	if( FS_CheckParm( "-trisoup" )) trisoup = true;
	if( FS_CheckParm( "-debug" ))	debug = true;
	if( FS_CheckParm( "-debugsurfaces" )) debugSurfaces = true;	
	if( FS_CheckParm( "-debugaxis" )) debugAxis = true;
	if( FS_CheckParm( "-debugcluster" )) debugCluster = true;
	if( FS_CheckParm( "-debugorigin" )) debugOrigin = true;		
	if( FS_CheckParm( "-debugdeluxe" ))
	{		
		deluxemap = true;
		debugDeluxemap = true;
	}
	if( FS_CheckParm( "-notrace" )) notrace = true;		
	if( FS_CheckParm( "-patchshadows" )) patchshadows = true;
	if( FS_CheckParm( "-extra" )) superSample = EXTRA_SCALE;
	if( FS_CheckParm( "-extrawide" ))
	{
		superSample = EXTRAWIDE_SCALE;
		filter = true;
	}
	if( FS_CheckParm( "-nogrid" )) noGridLighting = true;
	if( FS_CheckParm( "-border" )) lightmapBorder = true;
	if( FS_CheckParm( "-nosurf" )) noSurfaces = true;
	if( FS_CheckParm( "-nostyles" )) noStyles = true;
	if( FS_CheckParm( "-cpma" ))
	{
		cpmaHack = true;
		Msg( "Enabling Challenge Pro Mode Asstacular Vertex Lighting Mode (tm)\n" );
	}
	if( FS_CheckParm( "-dirty" ))	dirty = true;
	if( FS_CheckParm( "-dirtdebug" )) dirtDebug = true;
	if( FS_GetParmFromCmdLine("-dirtmode", cmdparm ))
		dirtMode = bound( 0, com.atoi( cmdparm ), 1 );
	if( FS_GetParmFromCmdLine("-dirtdepth", cmdparm ))
		dirtDepth = bound( 1, com.atof( cmdparm ), 128 );
	if( FS_GetParmFromCmdLine("-dirtscale", cmdparm ))
		dirtScale = (1.0f, com.atof( cmdparm ), 128.0f );
	if( FS_GetParmFromCmdLine("-dirtgain", cmdparm ))
		dirtGain = (1.0f, com.atof( cmdparm ), 128.0f );

	Msg("---- Radiocity ---- [%s]\n", extra ? "extra" : "normal" );

	ParseEntities();
	SetEntityOrigins();
	SetupBrushes();
	SetupDirt();
	SetupSurfaceLightmaps();
	SetupTraceNodes();
	
	RadWorld();
	
	StoreSurfaceLightmaps();
	
	UnparseEntities();
	WriteBSPFile();
	WriteLightmaps();
}