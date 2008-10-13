//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       surface_extra.c - meta surfaces etc
//=======================================================================

#include "bsplib.h"
#include "const.h"
#include "matrixlib.h"

#define LIGHTMAP_EXCEEDED		-1
#define S_EXCEEDED			-2
#define T_EXCEEDED			-3
#define ST_EXCEEDED			-4
#define UNSUITABLE_TRIANGLE		-10
#define VERTS_EXCEEDED		-1000
#define INDEXES_EXCEEDED		-2000
#define MIN_OUTSIDE_EPSILON		-0.01f
#define MAX_OUTSIDE_EPSILON		1.01f
#define TJ_PLANE_EPSILON		(1.0f / 8.0f)
#define TJ_EDGE_EPSILON		(1.0f / 8.0f)
#define TJ_POINT_EPSILON		(1.0f / 8.0f)
#define GROW_META_VERTS		1024
#define GROW_META_TRIANGLES		1024
#define MAX_SAMPLES			256
#define THETA_EPSILON		0.000001
#define EQUAL_NORMAL_EPSILON		0.01
#define AXIS_SCORE			100000
#define AXIS_MIN			100000
#define VERT_SCORE			10000
#define SURFACE_SCORE		1000
#define ST_SCORE			50
#define ST_SCORE2			(2 * (ST_SCORE))
#define ADEQUATE_SCORE		((AXIS_MIN) + 1 * (VERT_SCORE))
#define GOOD_SCORE			((AXIS_MIN) + 2 * (VERT_SCORE) + 4 * (ST_SCORE))
#define PERFECT_SCORE		((AXIS_MIN) + + 3 * (VERT_SCORE) + (SURFACE_SCORE) + 4 * (ST_SCORE))
#define MAX_FOLIAGE_INSTANCES		8192

static int		numMetaSurfaces, numPatchMetaSurfaces;
static int		maxMetaVerts = 0;
static int		numMetaVerts = 0;
static int		firstSearchMetaVert = 0;
static int		numMergedVerts = 0;
static int		numMergedSurfaces = 0;
static dvertex_t		*metaVerts = NULL;

static int		maxMetaTriangles = 0;
static int		numMetaTriangles = 0;
static metaTriangle_t	*metaTriangles = NULL;
float			npDegrees = 0.0f;
static int		numFoliageInstances;
static foliageInstance_t	foliageInstances[MAX_FOLIAGE_INSTANCES];

/*
=================
ClearMetaVertexes

called before staring a new entity to clear out the triangle list
=================
*/
void ClearMetaTriangles( void )
{
	numMetaVerts = 0;
	numMetaTriangles = 0;
}

/*
=================
FindMetaVertex

finds a matching metavertex in the global list, returning its index
=================
*/
static int FindMetaVertex( dvertex_t *src )
{
	int	i;
	dvertex_t	*v, *temp;
	
	
	for( i = firstSearchMetaVert, v = &metaVerts[i]; i < numMetaVerts; i++, v++ )
	{
		if( memcmp( src, v, sizeof( dvertex_t )) == 0 )
			return i;
	}
	
	if( numMetaVerts >= maxMetaVerts )
	{
		maxMetaVerts += GROW_META_VERTS;
		temp = BSP_Malloc( maxMetaVerts * sizeof( dvertex_t ));
		if( metaVerts != NULL )
		{
			Mem_Copy( temp, metaVerts, numMetaVerts * sizeof( dvertex_t ));
			Mem_Free( metaVerts );
		}
		metaVerts = temp;
	}
	
	Mem_Copy( &metaVerts[numMetaVerts], src, sizeof( dvertex_t ));
	numMetaVerts++;
	
	return (numMetaVerts - 1);
}


/*
=================
AddMetaTriangle

adds a new meta triangle, allocating more memory if necessary
=================
*/
static int AddMetaTriangle( void )
{
	metaTriangle_t	*temp;
	
	if( numMetaTriangles >= maxMetaTriangles )
	{
		maxMetaTriangles += GROW_META_TRIANGLES;
		temp = BSP_Malloc( maxMetaTriangles * sizeof( metaTriangle_t ) );
		if( metaTriangles != NULL )
		{
			Mem_Copy( temp, metaTriangles, numMetaTriangles * sizeof( metaTriangle_t ) );
			Mem_Free( metaTriangles );
		}
		metaTriangles = temp;
	}
	
	numMetaTriangles++;
	return numMetaTriangles - 1;
}


/*
=================
FindMetaTriangle

finds a matching metatriangle in the global list,
otherwise adds it and returns the index to the metatriangle
=================
*/
int FindMetaTriangle( metaTriangle_t *src, dvertex_t *a, dvertex_t *b, dvertex_t *c, int planeNum )
{
	int	triIndex;
	vec3_t	dir;
	
	// detect degenerate triangles FIXME: do something proper here
	VectorSubtract( a->point, b->point, dir );
	if( VectorLength( dir ) < 0.125f )
		return -1;
	VectorSubtract( b->point, c->point, dir );
	if( VectorLength( dir ) < 0.125f )
		return -1;
	VectorSubtract( c->point, a->point, dir );
	if( VectorLength( dir ) < 0.125f )
		return -1;
	
	if( planeNum >= 0 )
	{
		// because of precision issues with small triangles, try to use the specified plane
		src->planeNum = planeNum;
		VectorCopy( mapplanes[planeNum].normal, src->plane );
		src->plane[3] = mapplanes[planeNum].dist;
	}
	else
	{
		// calculate a plane from the triangle's points (and bail if a plane can't be constructed)
		src->planeNum = -1;
		if( BspPlaneFromPoints( src->plane, a->point, b->point, c->point ) == false )
			return -1;
	}
	
	// ydnar 2002-10-03: repair any bogus normals (busted ase import kludge)
	if( VectorLength( a->normal ) <= 0.0f )
		VectorCopy( src->plane, a->normal );
	if( VectorLength( b->normal ) <= 0.0f )
		VectorCopy( src->plane, b->normal );
	if( VectorLength( c->normal ) <= 0.0f )
		VectorCopy( src->plane, c->normal );
	
	// ydnar 2002-10-04: set lightmap axis if not already set
	if( !(src->si->surfaceFlags & SURF_VERTEXLIT) && VectorIsNull( src->lightmapAxis ))
	{
		// the shader can specify an explicit lightmap axis
		if( src->si->lightmapAxis[0] || src->si->lightmapAxis[1] || src->si->lightmapAxis[2] )
			VectorCopy( src->si->lightmapAxis, src->lightmapAxis );
		else CalcLightmapAxis( src->plane, src->lightmapAxis );	// new axis-finding code
	}
	
	// fill out the src triangle
	src->indexes[0] = FindMetaVertex( a );
	src->indexes[1] = FindMetaVertex( b );
	src->indexes[2] = FindMetaVertex( c );
	
	// get a new triangle
	triIndex = AddMetaTriangle();
	
	// add the triangle
	Mem_Copy( &metaTriangles[triIndex], src, sizeof( metaTriangle_t ));
	
	// return the triangle index
	return triIndex;
}


/*
=================
SurfaceToMetaTriangles

converts a classified surface to metatriangles
=================
*/
static void SurfaceToMetaTriangles( drawsurf_t *ds )
{
	int		i;
	metaTriangle_t	src;
	dvertex_t		a, b, c;
	
	
	// only handle certain types of surfaces
	if( ds->type != SURFACE_FACE && ds->type != SURFACE_META && ds->type != SURFACE_FORCED_META && ds->type != SURFACE_DECAL )
		return;
	
	// speed at the expense of memory
	firstSearchMetaVert = numMetaVerts;
	
	// only handle valid surfaces
	if( ds->type != SURFACE_BAD && ds->numVerts >= 3 && ds->numIndexes >= 3 )
	{
		for( i = 0; i < ds->numIndexes; i += 3 )
		{
			if( ds->indexes[i] == ds->indexes[i+1] || ds->indexes[i] == ds->indexes[i+2] || ds->indexes[i+1] == ds->indexes[i+2] )
				continue;
			
			// build a metatriangle
			src.si = ds->shader;
			src.side = (ds->sideRef != NULL ? ds->sideRef->side : NULL);
			src.entityNum = ds->entitynum;
			src.surfaceNum = ds->surfacenum;
			src.planeNum = ds->planeNum;
			src.castShadows = ds->castShadows;
			src.recvShadows = ds->recvShadows;
			src.fogNum = ds->fogNum;
			src.sampleSize = ds->sampleSize;
			VectorCopy( ds->lightmapAxis, src.lightmapAxis );
			
			/* copy drawverts */
			Mem_Copy( &a, &ds->verts[ds->indexes[i+0]], sizeof( a ));
			Mem_Copy( &b, &ds->verts[ds->indexes[i+1]], sizeof( b ));
			Mem_Copy( &c, &ds->verts[ds->indexes[i+2]], sizeof( c ));
			FindMetaTriangle( &src, &a, &b, &c, ds->planeNum );
		}
		numMetaSurfaces++;
	}
	
	// clear the surface (free verts and indexes, sets it to SURFACE_BAD)
	ClearSurface( ds );
}


/*
=================
TriangulatePatchSurface

creates triangles from a patch
=================
*/
void TriangulatePatchSurface( drawsurf_t *ds )
{
	int		iterations, x, y, pw[5], r;
	drawsurf_t	*dsNew;
	bsp_mesh_t	src, *subdivided, *mesh;
	
	if( ds->numVerts == 0 || ds->type != SURFACE_PATCH || patchMeta == false )
		return;
	
	// make a mesh from the drawsurf 
	src.width = ds->patchWidth;
	src.height = ds->patchHeight;
	src.verts = ds->verts;
	iterations = IterationsForCurve( ds->longestCurve, patchSubdivisions );
	subdivided = SubdivideMesh2( src, iterations );
	
	// fit it to the curve and remove colinear verts on rows/columns
	PutMeshOnCurve( *subdivided );
	mesh = RemoveLinearMeshColumnsRows( subdivided );
	FreeMesh( subdivided );
	
	// make a copy of the drawsurface
	dsNew = AllocDrawSurf( SURFACE_META );
	Mem_Copy( dsNew, ds, sizeof( *ds ));
	
	// if the patch is nonsolid, then discard it */
	if(!(ds->shader->contents & CONTENTS_SOLID))
		ClearSurface( ds );
	
	ds = dsNew;
	
	// basic transmogrification
	ds->type = SURFACE_META;
	ds->numIndexes = 0;
	ds->indexes = BSP_Malloc( mesh->width * mesh->height * 6 * sizeof( int ));
	
	ds->numVerts = (mesh->width * mesh->height);
	ds->verts = mesh->verts;
	
	for( y = 0; y < (mesh->height - 1); y++ )
	{
		for( x = 0; x < (mesh->width - 1); x++ )
		{
			pw[0] = x + (y * mesh->width);
			pw[1] = x + ((y + 1) * mesh->width);
			pw[2] = x + 1 + ((y + 1) * mesh->width);
			pw[3] = x + 1 + (y * mesh->width);
			pw[4] = x + (y * mesh->width);	// same as pw[0]
			
			r = (x + y) & 1;
			
			ds->indexes[ds->numIndexes++] = pw[r+0];
			ds->indexes[ds->numIndexes++] = pw[r+1];
			ds->indexes[ds->numIndexes++] = pw[r+2];
			
			ds->indexes[ds->numIndexes++] = pw[r+0];
			ds->indexes[ds->numIndexes++] = pw[r+2];
			ds->indexes[ds->numIndexes++] = pw[r+3];
		}
	}
	
	// free the mesh, but not the verts
	Mem_Free( mesh );
	
	numPatchMetaSurfaces++;
	ClassifySurfaces( 1, ds );
}


/*
=================
MakeEntityMetaTriangles

builds meta triangles from brush faces (tristrips and fans)
=================
*/
void MakeEntityMetaTriangles( bsp_entity_t *e )
{
	int		i;
	drawsurf_t	*ds;

	MsgDev( D_NOTE, "--- MakeEntityMetaTriangles ---\n" );
	
	// walk the list of surfaces in the entity
	for( i = e->firstsurf; i < numdrawsurfs; i++ )
	{
		ds = &drawsurfs[i];
		if( ds->numVerts <= 0 )
			continue;
		
		// ignore autosprite surfaces
		if( ds->shader->autosprite )
			continue;
		
		// meta this surface?
		if( ds->shader->forceMeta == false )
			continue;
		
		switch( ds->type )
		{
		case SURFACE_FACE:
		case SURFACE_DECAL:
			StripFaceSurface( ds );
			SurfaceToMetaTriangles( ds );
			break;
		case SURFACE_PATCH:
			TriangulatePatchSurface( ds );
			break;
		case SURFACE_TRIANGLES:
			break;
		case SURFACE_FORCED_META:
		case SURFACE_META:
			SurfaceToMetaTriangles( ds );
			break;
		default:	break;
		}
	}
	
	MsgDev( D_INFO, "%6i total meta surfaces\n", numMetaSurfaces );
	MsgDev( D_INFO, "%6i stripped surfaces\n", c_stripSurfaces );
	MsgDev( D_INFO, "%6i fanned surfaces\n", c_fanSurfaces );
	MsgDev( D_INFO, "%6i patch meta surfaces\n", numPatchMetaSurfaces );
	MsgDev( D_INFO, "%6i meta verts\n", numMetaVerts );
	MsgDev( D_INFO, "%6i meta triangles\n", numMetaTriangles );
	
	TidyEntitySurfaces( e );
}


/*
=================
PointTriangleIntersect

assuming that all points lie in plane, determine if pt
is inside the triangle abc
code originally (c) 2001 softSurfer (www.softsurfer.com)
=================
*/
static bool PointTriangleIntersect( vec3_t pt, vec4_t plane, vec3_t a, vec3_t b, vec3_t c, vec3_t bary )
{
	vec3_t	u, v, w;
	float	uu, uv, vv, wu, wv, d;
	
	VectorSubtract( b, a, u );
	VectorSubtract( c, a, v );
	VectorSubtract( pt, a, w );
	
	uu = DotProduct( u, u );
	uv = DotProduct( u, v );
	vv = DotProduct( v, v );
	wu = DotProduct( w, u );
	wv = DotProduct( w, v );
	d = uv * uv - uu * vv;
	
	bary[1] = (uv * wv - vv * wu) / d;
	if( bary[1] < MIN_OUTSIDE_EPSILON || bary[1] > MAX_OUTSIDE_EPSILON )
		return false;
	bary[2] = (uv * wv - uu * wv) / d;
	if( bary[2] < MIN_OUTSIDE_EPSILON || bary[2] > MAX_OUTSIDE_EPSILON )
		return false;
	bary[0] = 1.0f - (bary[1] + bary[2]);
	
	return true;
}


/*
=================
CreateEdge

sets up an edge structure from a plane and 2 points that the edge ab falls lies in
=================
*/

typedef struct edge_s
{
	vec3_t	origin, edge;
	vec_t	length, kingpinLength;
	int	kingpin;
	vec4_t	plane;
} edge_t;

void CreateEdge( vec4_t plane, vec3_t a, vec3_t b, edge_t *edge )
{
	VectorCopy( a, edge->origin );
	
	// create vector aligned with winding direction of edge
	VectorSubtract( b, a, edge->edge );
	
	if( fabs( edge->edge[0] ) > fabs( edge->edge[1] ) && fabs( edge->edge[0] ) > fabs( edge->edge[2] ))
		edge->kingpin = 0;
	else if( fabs( edge->edge[1] ) > fabs( edge->edge[0] ) && fabs( edge->edge[1] ) > fabs( edge->edge[2] ))
		edge->kingpin = 1;
	else edge->kingpin = 2;
	edge->kingpinLength = edge->edge[edge->kingpin];
	
	VectorNormalize( edge->edge );
	edge->edge[3] = DotProduct( a, edge->edge );
	edge->length = DotProduct( b, edge->edge ) - edge->edge[3];
	
	// create perpendicular plane that edge lies in
	CrossProduct( plane, edge->edge, edge->plane );
	edge->plane[3] = DotProduct( a, edge->plane );
}

/*
=================
FixMetaTJunctions

fixes t-junctions on meta triangles
=================
*/
void FixMetaTJunctions( void )
{
	int		i, j, k, vertIndex, triIndex, numTJuncs;
	metaTriangle_t	*tri, *newTri;
	bsp_shader_t	*si;
	dvertex_t		*a, *b, *c, junc;
	float		dist, amount;
	vec3_t		pt;
	vec4_t		plane;
	edge_t		edges[3];
	
	
	// this code is crap; revisit later
	return;
	
	/* note it */
	MsgDev( D_NOTE, "--- FixMetaTJunctions ---\n" );
	
	numTJuncs = 0;
	for( i = 0; i < numMetaTriangles; i++ )
	{
		tri = &metaTriangles[ i ];
		
		// attempt to early out
		si = tri->si;
		if( (si->surfaceFlags & SURF_NODRAW) || si->autosprite || si->notjunc )
			continue;
		
		VectorCopy( tri->plane, plane );
		plane[3] = tri->plane[3];
		CreateEdge( plane, metaVerts[tri->indexes[0]].point, metaVerts[tri->indexes[1]].point, &edges[0] );
		CreateEdge( plane, metaVerts[tri->indexes[1]].point, metaVerts[tri->indexes[2]].point, &edges[1] );
		CreateEdge( plane, metaVerts[tri->indexes[2]].point, metaVerts[tri->indexes[0]].point, &edges[2] );
		
		for( j = 0; j < numMetaVerts; j++ )
		{
			VectorCopy( metaVerts[ j ].point, pt );

			if( i == 0 ) VectorSet( metaVerts[j].color[0], 8, 8, 8 );
			
			dist = DotProduct( pt, plane ) - plane[3];
			if( fabs( dist ) > TJ_PLANE_EPSILON )
				continue;
			
			// skip this point if it already exists in the triangle
			for( k = 0; k < 3; k++ )
			{
				if( fabs( pt[0] - metaVerts[tri->indexes[k]].point[0] ) <= TJ_POINT_EPSILON &&
					fabs( pt[1] - metaVerts[tri->indexes[k]].point[1] ) <= TJ_POINT_EPSILON &&
					fabs( pt[2] - metaVerts[tri->indexes[k]].point[2] ) <= TJ_POINT_EPSILON )
					break;
			}
			if( k < 3 ) continue;
			
			for( k = 0; k < 3; k++ )
			{
				if( fabs( edges[k].kingpinLength ) < TJ_EDGE_EPSILON )
					continue;
				
				dist = DotProduct( pt, edges[k].plane ) - edges[k].plane[ 3 ];
				if( fabs( dist ) > TJ_EDGE_EPSILON )
					continue;
				
				// determine how far along the edge the point lies
				amount = (pt[edges[k].kingpin] - edges[k].origin[edges[k].kingpin]) / edges[k].kingpinLength;
				if( amount <= 0.0f || amount >= 1.0f )
					continue;
				
				VectorSet( metaVerts[tri->indexes[k]].color[0], 255, 204, 0 );
				VectorSet( metaVerts[tri->indexes[(k+1)%3]].color[0], 255, 204, 0 );
				

				// the edge opposite the zero-weighted vertex was hit, so use that as an amount
				a = &metaVerts[tri->indexes[(k+0)%3]];
				b = &metaVerts[tri->indexes[(k+1)%3]];
				c = &metaVerts[tri->indexes[(k+2)%3]];
				
				LerpDrawVertAmount( a, b, amount, &junc );
				VectorCopy( pt, junc.point );
				
				if( VectorCompare( junc.point, a->point ) || VectorCompare( junc.point, b->point ) || VectorCompare( junc.point, c->point ))
					continue;
				
				// see if we can just re-use the existing vert
				if( !memcmp( &metaVerts[j], &junc, sizeof( junc ))) vertIndex = j;
				else
				{
					// find new vertex (NOTE: a and b are invalid pointers after this)
					firstSearchMetaVert = numMetaVerts;
					vertIndex = FindMetaVertex( &junc );
					if( vertIndex < 0 ) continue;
				}
						
				triIndex = AddMetaTriangle();
				if( triIndex < 0 )
					continue;
				
				tri = &metaTriangles[i];
				newTri = &metaTriangles[ triIndex ];
				Mem_Copy( newTri, tri, sizeof( *tri ) );
				
				tri->indexes[(k+1)%3] = vertIndex;
				newTri->indexes[k] = vertIndex;
				
				CreateEdge( plane, metaVerts[tri->indexes[0]].point, metaVerts[tri->indexes[1]].point, &edges[0] );
				CreateEdge( plane, metaVerts[tri->indexes[1]].point, metaVerts[tri->indexes[2]].point, &edges[1] );
				CreateEdge( plane, metaVerts[tri->indexes[2]].point, metaVerts[tri->indexes[0]].point, &edges[2] );
				
				metaVerts[vertIndex].color[0][0] = 255;
				metaVerts[vertIndex].color[0][1] = 204;
				metaVerts[vertIndex].color[0][2] = 0;
				
				// add to counter and end processing of this vert
				numTJuncs++;
				break;
			}
		}
	}

	MsgDev( D_INFO, "%6i T-junctions added\n", numTJuncs );
}


/*
=================
SmoothMetaTriangles

averages coincident vertex normals in the meta triangles
=================
*/
void SmoothMetaTriangles( void )
{
	int		i, j, k, cs, numVerts, numVotes, numSmoothed;
	float		shadeAngle, defaultShadeAngle, maxShadeAngle, dot, testAngle;
	metaTriangle_t	*tri;
	float		*shadeAngles;
	byte		*smoothed;
	vec3_t		average, diff;
	int		indexes[MAX_SAMPLES];
	vec3_t		votes[MAX_SAMPLES];
	
	MsgDev( D_INFO, "--- SmoothMetaTriangles ---\n" );
	
	// allocate shade angle table
	shadeAngles = BSP_Malloc( numMetaVerts * sizeof( float ));
	
	// allocate smoothed table
	cs = (numMetaVerts / 8) + 1;
	smoothed = BSP_Malloc( cs );
	
	// set default shade angle
	defaultShadeAngle = DEG2RAD( npDegrees );
	maxShadeAngle = 0.0f;
	
	// run through every surface and flag verts belonging to non-lightmapped surfaces
	// and set per-vertex smoothing angle
	for( i = 0, tri = &metaTriangles[i]; i < numMetaTriangles; i++, tri++ )
	{
		if( tri->si->shadeAngleDegrees > 0.0f )
			shadeAngle = DEG2RAD( tri->si->shadeAngleDegrees );
		else shadeAngle = defaultShadeAngle;
		if( shadeAngle > maxShadeAngle )
			maxShadeAngle = shadeAngle;
		
		// flag its verts
		for( j = 0; j < 3; j++ )
		{
			shadeAngles[tri->indexes[j]] = shadeAngle;
			if( shadeAngle <= 0 )
				smoothed[tri->indexes[j]>>3] |= (1<<(tri->indexes[j]&7));
		}
	}
	
	// bail if no surfaces have a shade angle
	if( maxShadeAngle <= 0 )
	{
		MsgDev( D_INFO, "No smoothing angles specified, aborting\n" );
		if( shadeAngles ) Mem_Free( shadeAngles );
		if( smoothed ) Mem_Free( smoothed );
		return;
	}
	
	numSmoothed = 0;
	for( i = 0; i < numMetaVerts; i++ )
	{
		if( smoothed[i>>3] & (1<<(i&7)))
			continue;
		
		VectorClear( average );
		numVerts = 0;
		numVotes = 0;
		
		// build a table of coincident vertexes
		for( j = i; j < numMetaVerts && numVerts < MAX_SAMPLES; j++ )
		{
			if( smoothed[j>>3] & (1<<(j&7)))
				continue;

			if( VectorCompare( metaVerts[i].point, metaVerts[j].point ) == false )
				continue;
			
			// use smallest shade angle
			shadeAngle = (shadeAngles[i] < shadeAngles[j] ? shadeAngles[i] : shadeAngles[j]);
			
			dot = DotProduct( metaVerts[i].normal, metaVerts[j].normal );
			if( dot > 1.0 ) dot = 1.0;
			else if( dot < -1.0 ) dot = -1.0;
			testAngle = acos( dot ) + THETA_EPSILON;
			if( testAngle >= shadeAngle )
				continue;
			
			indexes[numVerts++] = j;
			smoothed[j>>3] |= (1<<(j&7));
			
			// see if this normal has already been voted
			for( k = 0; k < numVotes; k++ )
			{
				VectorSubtract( metaVerts[j].normal, votes[k], diff );
				if( fabs( diff[0] ) < EQUAL_NORMAL_EPSILON &&
				fabs( diff[1] ) < EQUAL_NORMAL_EPSILON && fabs( diff[2] ) < EQUAL_NORMAL_EPSILON )
					break;
			}
			
			// add a new vote?
			if( k == numVotes && numVotes < MAX_SAMPLES )
			{
				VectorAdd( average, metaVerts[j].normal, average );
				VectorCopy( metaVerts[j].normal, votes[ numVotes ] );
				numVotes++;
			}
		}
		
		// don't average for less than 2 verts
		if( numVerts < 2 ) continue;
		
		// average normal
		if( VectorNormalizeLength( average ) > 0 )
		{
			for( j = 0; j < numVerts; j++ )
				VectorCopy( average, metaVerts[indexes[j]].normal );
			numSmoothed++;
		}
	}
	
	Mem_Free( shadeAngles );
	Mem_Free( smoothed );
	MsgDev( D_INFO, "%6i smoothed vertexes\n", numSmoothed );
}

/*
=================
AddMetaVertToSurface

adds a drawvert to a surface unless an existing vert matching already exists
returns the index of that vert (or < 0 on failure)
=================
*/
int AddMetaVertToSurface( drawsurf_t *ds, dvertex_t *dv1, int *coincident )
{
	int		i;
	dvertex_t		*dv2;
	
	// go through the verts and find a suitable candidate
	for( i = 0; i < ds->numVerts; i++ )
	{
		dv2 = &ds->verts[i];
		
		if( VectorCompare( dv1->point, dv2->point ) == false )
			continue;
		if( VectorCompare( dv1->normal, dv2->normal ) == false )
			continue;
		
		// good enough at this point
		(*coincident)++;
		
		if( dv1->st[0] != dv2->st[0] || dv1->st[1] != dv2->st[1] )
			continue;
		if( dv1->color[0][3] != dv2->color[0][3] )
			continue;
		
		numMergedVerts++;
		return i;
	}

	if( ds->numVerts >= ((ds->shader->surfaceFlags & SURF_VERTEXLIT) ? 999 : 64 ))
		return VERTS_EXCEEDED;
	
	// made it this far, add the vert and return
	dv2 = &ds->verts[ds->numVerts++];
	*dv2 = *dv1;
	return (ds->numVerts - 1);
}


/*
=================
AddMetaTriangleToSurface

attempts to add a metatriangle to a surface
returns the score of the triangle added
=================
*/
static int AddMetaTriangleToSurface( drawsurf_t *ds, metaTriangle_t *tri, bool testAdd )
{
	int		i, score, coincident, ai, bi, ci, oldTexRange[ 2 ];
	float		lmMax;
	vec3_t		mins, maxs;
	bool		inTexRange, es, et;
	drawsurf_t	old;
	
	if( ds->numIndexes >= 6000 )
		return 0;
	
	if( ds->entitynum != tri->entityNum )
		return 0;
	if( ds->castShadows != tri->castShadows || ds->recvShadows != tri->recvShadows )
		return 0;
	if( ds->shader != tri->si || ds->fogNum != tri->fogNum || ds->sampleSize != tri->sampleSize )
		return 0;
	
	// planar surfaces will only merge with triangles in the same plane
	if( npDegrees == 0.0f && ds->shader->nonplanar == false && ds->planeNum >= 0 )
	{
		if( VectorCompare( mapplanes[ds->planeNum].normal, tri->plane ) == false || mapplanes[ds->planeNum].dist != tri->plane[3] )
			return 0;
		if( tri->planeNum >= 0 && tri->planeNum != ds->planeNum )
			return 0;
	}
	
	// set initial score
	score = tri->surfaceNum == ds->surfacenum ? SURFACE_SCORE : 0;
	
	/* score the the dot product of lightmap axis to plane */
	if( (ds->shader->surfaceFlags & SURF_VERTEXLIT) || VectorCompare( ds->lightmapAxis, tri->lightmapAxis ) )
		score += AXIS_SCORE;
	else score += AXIS_SCORE * DotProduct( ds->lightmapAxis, tri->plane );
	
	// preserve old drawsurface if this fails
	Mem_Copy( &old, ds, sizeof( *ds ));
	
	// attempt to add the verts
	coincident = 0;
	ai = AddMetaVertToSurface( ds, &metaVerts[tri->indexes[0]], &coincident );
	bi = AddMetaVertToSurface( ds, &metaVerts[tri->indexes[1]], &coincident );
	ci = AddMetaVertToSurface( ds, &metaVerts[tri->indexes[2]], &coincident );
	
	// check vertex underflow
	if( ai < 0 || bi < 0 || ci < 0 )
	{
		Mem_Copy( ds, &old, sizeof( *ds ) );
		return 0;
	}
	
	// score coincident vertex count (2003-02-14: changed so this only matters on planar surfaces)
	score += (coincident * VERT_SCORE);
	
	// add new vertex bounds to mins/maxs
	VectorCopy( ds->mins, mins );
	VectorCopy( ds->maxs, maxs );
	AddPointToBounds( metaVerts[ tri->indexes[ 0 ] ].point, mins, maxs );
	AddPointToBounds( metaVerts[ tri->indexes[ 1 ] ].point, mins, maxs );
	AddPointToBounds( metaVerts[ tri->indexes[ 2 ] ].point, mins, maxs );
	
	// check lightmap bounds overflow (after at least 1 triangle has been added)
	if( !(ds->shader->surfaceFlags & SURF_VERTEXLIT) && ds->numIndexes > 0 && VectorLength( ds->lightmapAxis ) > 0.0f &&
	(VectorCompare( ds->mins, mins ) == false || VectorCompare( ds->maxs, maxs ) == false))
	{
		// set maximum size before lightmap scaling (normally 2032 units) */
		// 2004-02-24: scale lightmap test size by 2 to catch larger brush faces */
		// 2004-04-11: reverting to actual lightmap size */
		lmMax = (ds->sampleSize * (ds->shader->lmCustomWidth - 1));
		for( i = 0; i < 3; i++ )
		{
			if( (maxs[i] - mins[i]) > lmMax )
			{
				Mem_Copy( ds, &old, sizeof( *ds ));
				return 0;
			}
		}
	}
	
	// check texture range overflow
	oldTexRange[0] = ds->texRange[0];
	oldTexRange[1] = ds->texRange[1];
	inTexRange = CalcSurfaceTextureRange( ds );
	
	es = (ds->texRange[0] > oldTexRange[0]) ? true : false;
	et = (ds->texRange[1] > oldTexRange[1]) ? true : false;
	
	if( inTexRange == false && ds->numIndexes > 0 )
	{
		Mem_Copy( ds, &old, sizeof( *ds ) );
		return UNSUITABLE_TRIANGLE;
	}
	
	// score texture range
	if( ds->texRange[0] <= oldTexRange[0] )
		score += ST_SCORE2;
	else if( ds->texRange[0] > oldTexRange[0] && oldTexRange[1] > oldTexRange[0] )
		score += ST_SCORE;
	
	if( ds->texRange[1] <= oldTexRange[1] )
		score += ST_SCORE2;
	else if( ds->texRange[1] > oldTexRange[1] && oldTexRange[0] > oldTexRange[1] )
		score += ST_SCORE;
	
	
	// go through the indexes and try to find an existing triangle that matches abc
	for( i = 0; i < ds->numIndexes; i += 3 )
	{
		// 2002-03-11 (birthday!): rotate the triangle 3x to find an existing triangle
		if( (ai == ds->indexes[i] && bi == ds->indexes[ i + 1 ] && ci == ds->indexes[ i + 2 ]) ||
		(bi == ds->indexes[i] && ci == ds->indexes[ i + 1 ] && ai == ds->indexes[ i + 2 ]) ||
		(ci == ds->indexes[i] && ai == ds->indexes[ i + 1 ] && bi == ds->indexes[ i + 2 ]) )
		{
			Mem_Copy( ds, &old, sizeof( *ds ) );
			tri->si = NULL;
			return 0;
		}
		
		// rotate the triangle 3x to find an inverse triangle (error case)
		if( (ai == ds->indexes[i] && bi == ds->indexes[ i + 2 ] && ci == ds->indexes[ i + 1 ]) ||
		(bi == ds->indexes[i] && ci == ds->indexes[ i + 2 ] && ai == ds->indexes[ i + 1 ]) ||
		(ci == ds->indexes[i] && ai == ds->indexes[ i + 2 ] && bi == ds->indexes[ i + 1 ]) )
		{
			MsgDev( D_WARN, "Flipped triangle: (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f)\n",
				ds->verts[ ai ].point[ 0 ], ds->verts[ ai ].point[ 1 ], ds->verts[ ai ].point[ 2 ],
				ds->verts[ bi ].point[ 0 ], ds->verts[ bi ].point[ 1 ], ds->verts[ bi ].point[ 2 ],
				ds->verts[ ci ].point[ 0 ], ds->verts[ ci ].point[ 1 ], ds->verts[ ci ].point[ 2 ] );
			
			/* reverse triangle already present */
			Mem_Copy( ds, &old, sizeof( *ds ) );
			tri->si = NULL;
			return 0;
		}
	}
	
	// add the triangle indexes
	if( ds->numIndexes < 6000 )
		ds->indexes[ds->numIndexes++] = ai;
	if( ds->numIndexes < 6000 )
		ds->indexes[ds->numIndexes++] = bi;
	if( ds->numIndexes < 6000 )
		ds->indexes[ds->numIndexes++] = ci;
	
	if( ds->numIndexes >= 6000 )
	{
		Mem_Copy( ds, &old, sizeof( *ds ) );
		return 0;
	}
	
	if( ds->numIndexes >= 3 &&
		(ds->indexes[ds->numIndexes - 3] == ds->indexes[ds->numIndexes - 2] ||
		ds->indexes[ds->numIndexes - 3] == ds->indexes[ds->numIndexes - 1] ||
		ds->indexes[ds->numIndexes - 2] == ds->indexes[ds->numIndexes - 1]))
		MsgDev( D_INFO, "DEG:%d! ", ds->numVerts );
	
	// testing only?
	if( testAdd ) Mem_Copy( ds, &old, sizeof( *ds ));
	else
	{
		VectorCopy( mins, ds->mins );
		VectorCopy( maxs, ds->maxs );
		tri->si = NULL;
	}
	
	ds->sideRef = AllocSideRef( tri->side, ds->sideRef );
	
	return score;
}


/*
=================
MetaTrianglesToSurface

creates map drawsurface(s) from the list of possibles
=================
*/
static void MetaTrianglesToSurface( int numPossibles, metaTriangle_t *possibles, int *fOld, int *numAdded )
{
	int		i, j, f, best, score, bestScore;
	metaTriangle_t	*seed, *test;
	drawsurf_t	*ds;
	dvertex_t		*verts;
	int		*indexes;
	bool		added;
	
	verts = BSP_Malloc( sizeof( *verts ) * 999 );
	indexes = BSP_Malloc( sizeof( *indexes ) * 6000 );
	
	for( i = 0, seed = possibles; i < numPossibles; i++, seed++ )
	{
		// skip this triangle if it has already been merged
		if( seed->si == NULL ) continue;
		
		// start a new drawsurface
		ds = AllocDrawSurf( SURFACE_META );
		ds->entitynum = seed->entityNum;
		ds->surfacenum = seed->surfaceNum;
		ds->castShadows = seed->castShadows;
		ds->recvShadows = seed->recvShadows;
		
		ds->shader = seed->si;
		ds->planeNum = seed->planeNum;
		ds->fogNum = seed->fogNum;
		ds->sampleSize = seed->sampleSize;
		ds->verts = verts;
		ds->indexes = indexes;
		VectorCopy( seed->lightmapAxis, ds->lightmapAxis );
		ds->sideRef = AllocSideRef( seed->side, NULL );
		
		ClearBounds( ds->mins, ds->maxs );
		
		memset( verts, 0, sizeof( verts ));
		memset( indexes, 0, sizeof( indexes ));
		
		// add the first triangle
		if( AddMetaTriangleToSurface( ds, seed, false ))
			(*numAdded)++;
		
		// progressively walk the list until no more triangles can be added
		added = true;
		while( added )
		{
			f = 10 * *numAdded / numMetaTriangles;
			if( f > *fOld )
			{
				*fOld = f;
				MsgDev( D_INFO, "%d...", f );
			}
			
			// reset best score
			best = -1;
			bestScore = 0;
			added = false;
			
			for( j = i + 1, test = &possibles[j]; j < numPossibles; j++, test++ )
			{
				if( test->si == NULL ) continue;
				
				score = AddMetaTriangleToSurface( ds, test, true );
				if( score > bestScore )
				{
					best = j;
					bestScore = score;
					
					// if we have a score over a certain threshold, just use it
					if( bestScore >= GOOD_SCORE )
					{
						if( AddMetaTriangleToSurface( ds, &possibles[best], false ))
							(*numAdded)++;
						best = -1;
						bestScore = 0;
						added = true;
					}
				}
			}
			
			if( best >= 0 && bestScore > ADEQUATE_SCORE )
			{
				if( AddMetaTriangleToSurface( ds, &possibles[best], false ))
					(*numAdded)++;
				added = true;
			}
		}
		
		// copy the verts and indexes to the new surface
		ds->verts = BSP_Malloc( ds->numVerts * sizeof( dvertex_t ));
		Mem_Copy( ds->verts, verts, ds->numVerts * sizeof( dvertex_t ));
		ds->indexes = BSP_Malloc( ds->numIndexes * sizeof( int ));
		Mem_Copy( ds->indexes, indexes, ds->numIndexes * sizeof( int ));
		
		ClassifySurfaces( 1, ds );
	}
	
	Mem_Free( verts );
	Mem_Free( indexes );
}


/*
=================
CompareMetaTriangles

compare function for qsort
=================
*/
static int CompareMetaTriangles( const void *a, const void *b )
{
	int	i, j, av, bv;
	vec3_t	aMins, bMins;
	
	// shader first
	if(((metaTriangle_t*) a)->si < ((metaTriangle_t*) b)->si )
		return 1;
	else if(((metaTriangle_t*) a)->si > ((metaTriangle_t*) b)->si )
		return -1;
	
	// then fog
	else if(((metaTriangle_t*) a)->fogNum < ((metaTriangle_t*) b)->fogNum )
		return 1;
	else if(((metaTriangle_t*) a)->fogNum > ((metaTriangle_t*) b)->fogNum )
		return -1;
	
	// find mins
	VectorSet( aMins, 999999, 999999, 999999 );
	VectorSet( bMins, 999999, 999999, 999999 );

	for( i = 0; i < 3; i++ )
	{
		av = ((metaTriangle_t*) a)->indexes[i];
		bv = ((metaTriangle_t*) b)->indexes[i];
		for( j = 0; j < 3; j++ )
		{
			if( metaVerts[av].point[j] < aMins[j] )
				aMins[j] = metaVerts[av].point[j];
			if( metaVerts[bv].point[j] < bMins[j] )
				bMins[j] = metaVerts[bv].point[j];
		}
	}
	
	for( i = 0; i < 3; i++ )
	{
		if( aMins[i] < bMins[i] )
			return 1;
		else if( aMins[i] > bMins[i] )
			return -1;
	}
	return 0;
}


/*
=================
MergeMetaTriangles

merges meta triangles into drawsurfaces
=================
*/
void MergeMetaTriangles( void )
{
	int		i, j, fOld, start, numAdded;
	metaTriangle_t	*head, *end;
	
	if( numMetaTriangles <= 0 )
		return;
	
	MsgDev( D_INFO, "--- MergeMetaTriangles ---\n" );
	
	// sort the triangles by shader major, fognum minor */
	qsort( metaTriangles, numMetaTriangles, sizeof( metaTriangle_t ), CompareMetaTriangles );

	fOld = -1;
	start = Sys_Milliseconds();
	numAdded = 0;
	
	for( i = 0, j = 0; i < numMetaTriangles; i = j )
	{
		head = &metaTriangles[i];
		
		if( head->si == NULL ) continue;
		
		if( j <= i )
		{
			for( j = i + 1; j < numMetaTriangles; j++ )
			{
				end = &metaTriangles[j];
				if( head->si != end->si || head->fogNum != end->fogNum )
					break;
			}
		}
		MetaTrianglesToSurface( (j - i), head, &fOld, &numAdded );
	}
	
	ClearMetaTriangles();
	
	if( i ) MsgDev( D_INFO, " (%d)\n", (int) (Sys_Milliseconds() - start));
	
	MsgDev( D_INFO, "%6i surfaces merged\n", numMergedSurfaces );
	MsgDev( D_INFO, "%6i vertexes merged\n", numMergedVerts );
}

/*
=================
SubdivideFoliageTriangle_r

recursively subdivides a triangle until the triangle is smaller than
the desired density, then pseudo-randomly sets a point
=================
*/
static void SubdivideFoliageTriangle_r( drawsurf_t *ds, foliage_t *foliage, dvertex_t **tri )
{
	float		*a, *b, dx, dy, dz, dist, maxDist;
	dvertex_t		mid, *tri2[3];
	int		i, max;
	vec4_t		plane;
	foliageInstance_t	*fi;
	
	if( numFoliageInstances >= MAX_FOLIAGE_INSTANCES )
		return;
	
	if( !BspPlaneFromPoints( plane, tri[0]->point, tri[1]->point, tri[2]->point ))
		return;
	if( plane[2] < 0.5f ) return;
	
	fi = &foliageInstances[ numFoliageInstances ];
	max = -1;
	maxDist = 0.0f;

	VectorClear( fi->point );
	VectorClear( fi->normal );

	for( i = 0; i < 3; i++ )
	{
		a = tri[i]->point;
		b = tri[(i+1)%3]->point;
			
		dx = a[0] - b[0];
		dy = a[1] - b[1];
		dz = a[2] - b[2];
		dist = (dx * dx) + (dy * dy) + (dz * dz);
			
		if( dist > maxDist )
		{
			maxDist = dist;
			max = i;
		}
			
		VectorAdd( fi->point, tri[i]->point, fi->point );
		VectorAdd( fi->normal, tri[i]->normal, fi->normal );
	}
		
	if( maxDist <= (foliage->density * foliage->density) )
	{
		float	alpha, odds, r;
			
		if( foliage->inverseAlpha == 2 ) alpha = 1.0f;
		else
		{
			alpha = ((float) tri[0]->color[0][3] + (float) tri[1]->color[0][3] + (float) tri[2]->color[0][3]) / 765.0f;
			if( foliage->inverseAlpha == 1 ) alpha = 1.0f - alpha;
			if( alpha < 0.75f ) return;
		}
			
		// roll the dice
		odds = foliage->odds * alpha;
		r = RANDOM_FLOAT( 0, 1.0f );
		if( r > odds ) return;
			
		// scale centroid
		VectorScale( fi->point, 0.33333333f, fi->point );
		if( VectorNormalizeLength( fi->normal ) == 0.0f )
			return;
			
		numFoliageInstances++;
		return;
	}
	
	// split the longest edge and map it
	LerpDrawVert( tri[max], tri[(max+1)%3], &mid );
	
	// recurse to first triangle
	VectorCopy( tri, tri2 );
	tri2[max] = &mid;
	SubdivideFoliageTriangle_r( ds, foliage, tri2 );
	
	// recurse to second triangle
	VectorCopy( tri, tri2 );
	tri2[(max+1)%3] = &mid;
	SubdivideFoliageTriangle_r( ds, foliage, tri2 );
}

/*
=================
GenFoliage

generates a foliage file for a bsp
=================
*/
void Foliage( drawsurf_t *src )
{
	int		i, j, k, x, y, pw[5], r, oldNumMapDrawSurfs;
	drawsurf_t	*ds;
	bsp_shader_t	*si;
	foliage_t		*foliage;
	bsp_mesh_t	srcMesh, *subdivided, *mesh;
	dvertex_t		*verts, *dv[3], *fi;
	matrix4x4		transform;
	
	si = src->shader;
	if( si == NULL || si->foliage == NULL )
		return;
	
	for( foliage = si->foliage; foliage != NULL; foliage = foliage->next )
	{
		numFoliageInstances = 0;
		
		// map the surface onto the lightmap origin/cluster/normal buffers
		switch( src->type )
		{
		case SURFACE_META:
		case SURFACE_FORCED_META:
		case SURFACE_TRIANGLES:
			verts = src->verts;
			for( i = 0; i < src->numIndexes; i += 3 )
			{
				dv[0] = &verts[src->indexes[i+0] ];
				dv[1] = &verts[src->indexes[i+1] ];
				dv[2] = &verts[src->indexes[i+2] ];
				SubdivideFoliageTriangle_r( src, foliage, dv );
			}
			break;
		case SURFACE_PATCH:
			srcMesh.width = src->patchWidth;
			srcMesh.height = src->patchHeight;
			srcMesh.verts = src->verts;
			subdivided = SubdivideMesh( srcMesh, 8, 512 );
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
					pw[4] = x + (y * mesh->width);
					r = (x + y) & 1;
					
					dv[0] = &verts[ pw[r+0]];
					dv[1] = &verts[ pw[r+1]];
					dv[2] = &verts[ pw[r+2]];
					SubdivideFoliageTriangle_r( src, foliage, dv );
						
					dv[0] = &verts[pw[r+0]];
					dv[1] = &verts[pw[r+2]];
					dv[2] = &verts[pw[r+3]];
					SubdivideFoliageTriangle_r( src, foliage, dv );
				}
			}
				
			FreeMesh( mesh );
			break;
		default:	break;
		}
		
		if( numFoliageInstances < 1 ) continue;
		
		oldNumMapDrawSurfs = numdrawsurfs;
		
		Matrix4x4_LoadIdentity( transform );
		Matrix4x4_ConcatScale( transform, foliage->scale );
		
		// add the model to the bsp
		InsertModel( foliage->model, 0, 0, 0.0f, transform, src, 0 );
		
		for( i = oldNumMapDrawSurfs; i < numdrawsurfs; i++ )
		{
			ds = &drawsurfs[i];
			
			ds->type = SURFACE_FOLIAGE;
			ds->numFoliageInstances = numFoliageInstances;
			
			// a wee hack
			ds->patchWidth = ds->numFoliageInstances;
			ds->patchHeight = ds->numVerts;
			
			// set fog to be same as source surface
			ds->fogNum = src->fogNum;
			
			verts = BSP_Malloc( (ds->numVerts + ds->numFoliageInstances) * sizeof( *verts ));
			Mem_Copy( verts, ds->verts, ds->numVerts * sizeof( *verts ));
			Mem_Free( ds->verts );
			ds->verts = verts;
			
			for( j = 0; j < ds->numFoliageInstances; j++ )
			{
				// get vert (foliage instance)
				fi = &ds->verts[ds->numVerts+j];

				VectorCopy( foliageInstances[j].point, fi->point );
				VectorCopy( foliageInstances[j].normal, fi->normal );

				for( k = 0; k < LM_STYLES; k++ )
				{
					fi->color[k][0] = 255;
					fi->color[k][1] = 255;
					fi->color[k][2] = 255;
					fi->color[k][3] = 255;
				}
			}
			ds->numVerts += ds->numFoliageInstances;
		}
	}
}


/*
=================
Fur

runs the fur processing algorithm on a map drawsurface
=================
*/
void Fur( drawsurf_t *ds )
{
	int		i, j, k, numLayers;
	float		offset, fade, a;
	drawsurf_t	*fur;
	dvertex_t		*dv;
	
	if( ds == NULL || ds->fur || ds->shader->furNumLayers < 1 )
		return;
	
	numLayers = ds->shader->furNumLayers;
	offset = ds->shader->furOffset;
	fade = ds->shader->furFade * 255.0f;
	
	for( j = 0; j < ds->numVerts; j++ )
	{
		dv = &ds->verts[j];
			
		// offset is scaled by original vertex alpha
		a = (float) dv->color[0][3] / 255.0;
		VectorMA( dv->point, (offset * a), dv->normal, dv->point );
	}
	
	// wash, rinse, repeat
	for( i = 1; i < numLayers; i++ )
	{
		fur = CloneSurface( ds, ds->shader );
		if( fur == NULL ) return;
		
		fur->fur = true;
		for( j = 0; j < fur->numVerts; j++ )
		{
			dv = &ds->verts[ j ];
				
			// offset is scaled by original vertex alpha
			a = (float)dv->color[0][3] / 255.0;
			dv = &fur->verts[j];
			VectorMA( dv->point, (offset * a * i), dv->normal, dv->point );
			
			for( k = 0; k < LM_STYLES; k++ )
			{
				a = (float) dv->color[k][3] - fade;
				if( a > 255.0f ) dv->color[k][3] = 255;
				else if( a < 0 ) dv->color[k][3] = 0;
				else dv->color[k][3] = a;
			}
		}
	}
}