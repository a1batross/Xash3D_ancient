//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	surface.c - emit bsp surfaces
//=======================================================================

#include "bsplib.h"
#include "mathlib.h"
#include "const.h"

#define SNAP_FLOAT_TO_INT		32
#define SNAP_INT_TO_FLOAT		(1.0 / SNAP_FLOAT_TO_INT)
#define COLINEAR_AREA		10
#define MAX_INDICES			1024

drawsurf_t	drawsurfs[MAX_MAP_SURFACES];
int		numdrawsurfs;
int		c_stripSurfaces, c_fanSurfaces;

void ComputeAxisBase( vec3_t normal, vec3_t texX, vec3_t texY )
{
	vec_t	RotY, RotZ;

	// do some cleaning
	if( fabs( normal[0] ) < 1E-6 ) normal[0] = 0.0f;
	if( fabs( normal[1] ) < 1E-6 ) normal[1] = 0.0f;
	if( fabs( normal[2] ) < 1E-6 ) normal[2] = 0.0f;

	// compute the two rotations around Y and Z to rotate X to normal
	RotY = -atan2( normal[2], sqrt( normal[1] * normal[1] + normal[0] * normal[0] ));
	RotZ = atan2( normal[1], normal[0] );

	// rotate (0, 1, 0) and (0, 0, 1) to compute texX and texY
	texX[0] = -sin( RotZ );
	texX[1] = cos( RotZ );
	texX[2] = 0;

	// the texY vector is along -Z ( T texture coorinates axis )
	texY[0] = -sin( RotY ) * cos( RotZ );
	texY[1] = -sin( RotY ) * sin( RotZ );
	texY[2] = -cos( RotY );
}

/*
==================
IsTriangleDegenerate

Returns qtrue if all three points are collinear or backwards
===================
*/

static bool IsTriangleDegenerate( dvertex_t *points, int a, int b, int c )
{
	vec3_t	v1, v2, v3;
	float	d;

	VectorSubtract( points[b].point, points[a].point, v1 );
	VectorSubtract( points[c].point, points[a].point, v2 );
	CrossProduct( v1, v2, v3 );
	d = VectorLength( v3 );

	// assume all very small or backwards triangles will cause problems
	if( d < COLINEAR_AREA )
		return true;
	return false;
}

/*
===============
SurfaceAsTriFan

The surface can't be represented as a single tristrip without
leaving a degenerate triangle (and therefore a crack), so add
a point in the middle and create (points-1) triangles in fan order
===============
*/
static void SurfaceAsTriFan( dsurface_t *ds )
{
	int		i;
	vec4_t		colorSum;
	const float	*colorIn;
	dvertex_t		*mid, *v;

	// create a new point in the center of the face
	if( numvertexes == MAX_MAP_VERTEXES ) Sys_Break( "MAX_MAP_VERTEXES limit exceeded\n" );

	mid = &dvertexes[numvertexes];
	numvertexes++;

	colorSum[0] = colorSum[1] = colorSum[2] = colorSum[3] = 0;

	v = dvertexes + ds->firstvertex;
	for( i = 0; i < ds->numvertices; i++, v++ )
	{
		VectorAdd( mid->point, v->point, mid->point );
		mid->st[0] += v->st[0];
		mid->st[1] += v->st[1];
		mid->lm[0] += v->lm[0];
		mid->lm[1] += v->lm[1];
		colorIn = UnpackRGBA( v->color );

		colorSum[0] += colorIn[0];
		colorSum[1] += colorIn[1];
		colorSum[2] += colorIn[2];
		colorSum[3] += colorIn[3];
	}

	mid->point[0] /= ds->numvertices;
	mid->point[1] /= ds->numvertices;
	mid->point[2] /= ds->numvertices;

	mid->st[0] /= ds->numvertices;
	mid->st[1] /= ds->numvertices;
	mid->lm[0] /= ds->numvertices;
	mid->lm[1] /= ds->numvertices;

	colorSum[0] /= ds->numvertices;
	colorSum[1] /= ds->numvertices;
	colorSum[2] /= ds->numvertices;
	colorSum[3] /= ds->numvertices;

	mid->color = PackRGBA( colorSum[0], colorSum[1], colorSum[2], colorSum[3] );
	VectorCopy( (dvertexes + ds->firstvertex)->normal, mid->normal );

	// fill in indices in trifan order
	if( numindexes + ds->numvertices * 3 > MAX_MAP_INDEXES )
		Sys_Break( "MAX_MAP_INDEXES limit excceded\n" );

	ds->firstindex = numindexes;
	ds->numindices = ds->numvertices * 3;

	for( i = 0 ; i < ds->numvertices; i++ )
	{
		dindexes[numindexes++] = ds->numvertices;
		dindexes[numindexes++] = i;
		dindexes[numindexes++] = (i+1) % ds->numvertices;
	}
	ds->numvertices++;
}


/*
================
SurfaceAsTristrip

Try to create indices that make (points-2) triangles in tristrip order
================
*/
static void SurfaceAsTristrip( dsurface_t *ds )
{
	int	i;
	int	rotate;
	int	numIndices;
	int	ni;
	int	a, b, c;
	int	indices[MAX_INDICES];

	// determine the triangle strip order
	numIndices = ( ds->numvertices - 2 ) * 3;
	if( numIndices > MAX_INDICES ) Sys_Error( "MAX_INDICES limit exceeded for surface\n" );

	// try all possible orderings of the points looking
	// for a strip order that isn't degenerate
	for( rotate = 0; rotate < ds->numvertices; rotate++ )
	{
		for( ni = 0, i = 0; i < ds->numvertices - 2 - i; i++ )
		{
			a = ( ds->numvertices - 1 - i + rotate ) % ds->numvertices;
			b = ( i + rotate ) % ds->numvertices;
			c = ( ds->numvertices - 2 - i + rotate ) % ds->numvertices;

			if( IsTriangleDegenerate( dvertexes + ds->firstvertex, a, b, c ))
				break;

			indices[ni++] = a;
			indices[ni++] = b;
			indices[ni++] = c;

			if( i + 1 != ds->numvertices - 1 - i )
			{
				a = ( ds->numvertices - 2 - i + rotate ) % ds->numvertices;
				b = ( i + rotate ) % ds->numvertices;
				c = ( i + 1 + rotate ) % ds->numvertices;

				if( IsTriangleDegenerate( dvertexes + ds->firstvertex, a, b, c ))
					break;
				indices[ni++] = a;
				indices[ni++] = b;
				indices[ni++] = c;
			}
		}
		if( ni == numIndices ) break;	// got it done without degenerate triangles
	}

	// if any triangle in the strip is degenerate,
	// render from a centered fan point instead
	if( ni < numIndices )
	{
		c_fanSurfaces++;
		SurfaceAsTriFan( ds );
		return;
	}

	// a normal tristrip
	c_stripSurfaces++;

	if( numindexes + ni > MAX_MAP_INDEXES )
		Sys_Break( "MAX_MAP_NDEXES limit exceeded\n" );
	ds->firstindex = numindexes;
	ds->numindices = ni;

	Mem_Copy( dindexes + numindexes, indices, ni * sizeof( int ));
	numindexes += ni;
}

/*
===============
EmitPlanarSurf
===============
*/
void EmitPlanarSurf( drawsurf_t *ds )
{
	int		j;
	dsurface_t	*out;
	dvertex_t		*outv;

	if( numsurfaces == MAX_MAP_SURFACES ) Sys_Break( "MAX_MAP_SURFACES limit exceeded\n" );
	out = &dsurfaces[numsurfaces];
	numsurfaces++;

	out->shadernum = EmitShader( ds->shader->name );
	out->planenum = ds->planenum;
	out->lightmapnum = ds->lightmapNum;
	out->firstvertex = numvertexes;
	out->numvertices = ds->numverts;

	out->lm_base[0] = ds->lightmapX;
	out->lm_base[1] = ds->lightmapY;
	out->lm_size[0] = ds->lightmapWidth;
	out->lm_size[1] = ds->lightmapHeight;
	out->lm_side = ds->planenum & 1;

	VectorCopy( ds->lightmapOrigin, out->origin );
	VectorCopy( ds->lightmapVecs[0], out->vecs[0] );
	VectorCopy( ds->lightmapVecs[1], out->vecs[1] );
	VectorCopy( ds->lightmapVecs[2], out->normal );

	for( j = 0; j < ds->numverts; j++ )
	{
		if( numvertexes == MAX_MAP_VERTEXES )
			Sys_Break( "MAX_MAP_VERTEXES limit exceeded\n" );
		outv = &dvertexes[numvertexes];
		numvertexes++;
		Mem_Copy( outv, &ds->verts[j], sizeof( *outv ));
		outv->color = PackRGBA( 1.0f, 1.0f, 1.0f, 1.0f );
	}

	// create the indexes
	SurfaceAsTristrip( out );
}


/*
=============================================================================

DRAWSURF CONSTRUCTION

=============================================================================
*/

/*
=================
AllocDrawSurf
=================
*/
drawsurf_t *AllocDrawSurf( void )
{
	drawsurf_t	*ds;

	if( numdrawsurfs >= MAX_MAP_SURFACES )
		Sys_Break( "MAX_MAP_SURFACES limit exceeded\n");
	ds = &drawsurfs[numdrawsurfs];
	numdrawsurfs++;

	return ds;
}

/*
=================
DrawSurfaceForSide
=================
*/
drawsurf_t *DrawSurfaceForSide( bspbrush_t *b, side_t *s, winding_t *w )
{
	drawsurf_t	*ds;
	int		i, j;
	shader_t		*si;
	dvertex_t		*dv;
	float		mins[2], maxs[2];
	vec3_t		texX,texY;
	vec_t		x,y;

	if( w->numpoints > MAX_POINTS_ON_WINDING )
		Sys_Error( "DrawSurfaceForSide: winding overflow\n" );

	si = s->shader;
	ds = AllocDrawSurf();
	ds->shader = si;
	ds->mapbrush = b;
	ds->side = s;
	ds->numverts = w->numpoints;
	ds->verts = BSP_Malloc( ds->numverts * sizeof( *ds->verts ));
	ds->planenum = s->planenum; // g-cont. probably unneeded

	mins[0] = mins[1] = 99999;
	maxs[0] = maxs[1] = -99999;

	// compute s/t coordinates from brush primitive texture matrix
	// compute axis base
	ComputeAxisBase( mapplanes[s->planenum].normal, texX, texY );

	for( j = 0; j < w->numpoints; j++ )
	{
		dv = ds->verts + j;

		// round the xyz to a given precision
		for( i = 0; i < 3; i++ )
			dv->point[i] = SNAP_INT_TO_FLOAT * floor( w->p[j][i] * SNAP_FLOAT_TO_INT + 0.5 );
	
		if( g_brushtype == BRUSH_RADIANT )
		{
			// calculate texture s/t from brush primitive texture matrix
			x = DotProduct( dv->point, texX );
			y = DotProduct( dv->point, texY );
			dv->st[0] = s->matrix[0][0] * x + s->matrix[0][1] * y + s->matrix[0][2];
			dv->st[1] = s->matrix[1][0] * x + s->matrix[1][1] * y + s->matrix[1][2];
		} 
		else
		{
			// calculate texture s/t
			dv->st[0] = s->vecs[0][3] + DotProduct( s->vecs[0], dv->point );
			dv->st[1] = s->vecs[1][3] + DotProduct( s->vecs[1], dv->point );
			dv->st[0] /= si->width;
			dv->st[1] /= si->height;
		}

		for( i = 0; i < 2; i++ )
		{
			if( dv->st[i] < mins[i] ) mins[i] = dv->st[i];
			if( dv->st[i] > maxs[i] ) maxs[i] = dv->st[i];
		}

		// copy normal
		// g-cont. hey we can restore normals on a map loading
		VectorCopy ( mapplanes[s->planenum].normal, dv->normal );
	}

	// adjust the texture coordinates to be as close to 0 as possible
	mins[0] = floor( mins[0] );
	mins[1] = floor( mins[1] );
	for( i = 0; i < w->numpoints; i++ )
	{
		dv = ds->verts + i;
		dv->st[0] -= mins[0];
		dv->st[1] -= mins[1];
	}

	return ds;
}

/*
===================
SubdivideDrawSurf
===================
*/
void SubdivideDrawSurf( drawsurf_t *ds, winding_t *w, float subdivisions )
{
	int		i;
	int		axis;
	vec3_t		bounds[2];
	const float	epsilon = 0.1;
	int		subFloor, subCeil;
	winding_t		*frontWinding, *backWinding;
	drawsurf_t	*newds;

	if( !w ) return;
	if( w->numpoints < 3 ) Sys_Break( "SubdivideDrawSurf: too few w->numpoints\n" );
	ClearBounds( bounds[0], bounds[1] );
	for( i = 0; i < w->numpoints; i++ )
	{
		AddPointToBounds( w->p[i], bounds[0], bounds[1] );
	}

	for( axis = 0; axis < 3; axis++ )
	{
		vec3_t	planePoint = { 0, 0, 0 };
		vec3_t	planeNormal = { 0, 0, 0 };
		float	d;

		subFloor = floor( bounds[0][axis]  / subdivisions ) * subdivisions;
		subCeil = ceil( bounds[1][axis] / subdivisions ) * subdivisions;

		planePoint[axis] = subFloor + subdivisions;
		planeNormal[axis] = -1;

		d = DotProduct( planePoint, planeNormal );

		// subdivide if necessary
		if( subCeil - subFloor > subdivisions )
		{
			// gotta clip polygon into two polygons
			ClipWindingEpsilon( w, planeNormal, d, epsilon, &frontWinding, &backWinding );

			// the clip may not produce two polygons if it was epsilon close
			if( !frontWinding ) w = backWinding;
			else if( !backWinding ) w = frontWinding;
			else
			{
				SubdivideDrawSurf( ds, frontWinding, subdivisions );
				SubdivideDrawSurf( ds, backWinding, subdivisions );
				return;
			}
		}
	}

	// emit this polygon
	newds = DrawSurfaceForSide( ds->mapbrush, ds->side, w );
}


/*
=====================
SubdivideDrawSurfs

Chop up surfaces that have subdivision attributes
=====================
*/
void SubdivideDrawSurfs( bsp_entity_t *e, tree_t *tree )
{
	int		i;
	drawsurf_t	*ds;
	winding_t		*w;
	float		subdivision;
	int		numbasedrawsurfs;
	shader_t		*si;

	// g-cont. Xash3d can subdivide polygons on-fly
	// probably this func unneeded
	MsgDev( D_INFO, "----- Subdivide Surfaces -----\n" );
	numbasedrawsurfs = numdrawsurfs;

	for( i = e->firstsurf; i < numbasedrawsurfs; i++ )
	{
		ds = &drawsurfs[i];

		// only subdivide brush sides, not patches or misc_models
		if ( !ds->side ) {
			continue;
		}

		// check subdivision for shader
		si = ds->side->shader;
		if( !si ) continue;

		subdivision = si->subdivisions;
		if( !subdivision ) continue;

		w = WindingFromDrawSurf( ds );
		ds->numverts = 0; // remove this reference
		SubdivideDrawSurf( ds, w, subdivision );
	}

}

//===================================================================================

/*
====================
ClipSideIntoTree_r

Adds non-opaque leaf fragments to the convex hull
====================
*/
void ClipSideIntoTree_r( winding_t *w, side_t *side, node_t *node )
{
	plane_t		*plane;
	winding_t		*front, *back;

	if( !w ) return;

	if( node->planenum != PLANENUM_LEAF )
	{
		if( side->planenum == node->planenum )
		{
			ClipSideIntoTree_r( w, side, node->children[0] );
			return;
		}
		if( side->planenum == ( node->planenum ^ 1))
		{
			ClipSideIntoTree_r( w, side, node->children[1] );
			return;
		}

		plane = &mapplanes[ node->planenum ];
		ClipWindingEpsilon( w, plane->normal, plane->dist, ON_EPSILON, &front, &back );
		FreeWinding( w );

		ClipSideIntoTree_r( front, side, node->children[0] );
		ClipSideIntoTree_r( back, side, node->children[1] );
		return;
	}

	// if opaque leaf, don't add
	if( !node->opaque )
	{
		AddWindingToConvexHull( w, &side->visibleHull, mapplanes[ side->planenum ].normal );
	}

	FreeWinding( w );
	return;
}


/*
=====================
ClipSidesIntoTree

Creates side->visibleHull for all visible sides

The drawsurf for a side will consist of the convex hull of
all points in non-opaque clusters, which allows overlaps
to be trimmed off automatically.
=====================
*/
void ClipSidesIntoTree( bsp_entity_t *e, tree_t *tree )
{
	bspbrush_t	*b;
	int		i;
	winding_t		*w;
	shader_t		*si;
	side_t		*side, *newSide;

	MsgDev( D_INFO, "----- ClipSidesIntoTree -----\n" );

	for( b = e->brushes; b; b = b->next )
	{
		for( i = 0; i < b->numsides; i++ )
		{
			side = &b->sides[i];
			if( !side->winding ) continue;
			w = CopyWinding( side->winding );
			side->visibleHull = NULL;
			ClipSideIntoTree_r( w, side, tree->headnode );

			w = side->visibleHull;
			if( !w ) continue;

			si = side->shader;
			if( !si ) continue;
			// don't create faces for non-visible sides
			if( si->surfaceFlags & SURF_NODRAW ) continue;

			if( side->bevel ) Sys_Break( "Sys_MonkeyShouldBeSpanked!\n" );
			// save this winding as a visible surface
			DrawSurfaceForSide( b, side, w );

			// make a back side for it if needed
			if( !(si->contents & CONTENTS_FOG ))
				continue;

			// duplicate the up-facing side
			w = ReverseWinding( w );
		
			newSide = BSP_Malloc( sizeof( *side ));
			*newSide = *side;
			newSide->visibleHull = w;
			newSide->planenum ^= 1;

			// save this winding as a visible surface
			DrawSurfaceForSide( b, newSide, w );
		}
	}
}

/*
===================================================================================

  FILTER REFERENCES DOWN THE TREE

===================================================================================
*/
/*
====================
FilterSideIntoTree_r

Place a reference to the given drawsurf in every leaf it contacts
====================
*/
int FilterSideIntoTree_r( winding_t *w, side_t *side, drawsurf_t *ds, node_t *node )
{
	surfaceref_t	*dsr;
	plane_t		*plane;
	winding_t		*front, *back;
	int		total;

	if( !w ) return 0;

	if( node->planenum != PLANENUM_LEAF )
	{
		if( side->planenum == node->planenum )
			return FilterSideIntoTree_r( w, side, ds, node->children[0] );
		if( side->planenum == ( node->planenum ^ 1 ))
			return FilterSideIntoTree_r( w, side, ds, node->children[1] );

		plane = &mapplanes[node->planenum];
		ClipWindingEpsilon( w, plane->normal, plane->dist, ON_EPSILON, &front, &back );

		// g-cont. it's expression valid ?
		total = FilterSideIntoTree_r( front, side, ds, node->children[0] );
		total += FilterSideIntoTree_r( back, side, ds, node->children[1] );

		FreeWinding( w );
		return total;
	}

	// if opaque leaf, don't add
	if( node->opaque ) return 0;

	dsr = BSP_Malloc( sizeof( *dsr ));
	dsr->outputnum = numsurfaces;
	dsr->next = node->surfaces;
	node->surfaces = dsr;

	FreeWinding( w );
	return 1;
}


/*
=====================
FilterFaceIntoTree
=====================
*/
int FilterFaceIntoTree( drawsurf_t *ds, tree_t *tree )
{
	winding_t	*w;
	int	l;
	
	w = WindingFromDrawSurf( ds );
	l = FilterSideIntoTree_r( w, ds->side, ds, tree->headnode );

	return l;
}

/*
=====================
FilterDrawsurfsIntoTree

Upon completion, all drawsurfs that actually generate a reference
will have been emited to the bspfile arrays, and the references
will have valid final indexes
=====================
*/
void FilterDrawsurfsIntoTree( bsp_entity_t *e, tree_t *tree )
{
	int		i;
	drawsurf_t	*ds;
	int		refs;
	int		c_surfs = 0, c_refs = 0;

	MsgDev( D_INFO, "----- FilterDrawsurfsIntoTree -----\n" );
          
	for( i = e->firstsurf; i < numdrawsurfs; i++ )
	{
		ds = &drawsurfs[i];

		if( !ds->numverts ) continue;

		refs = FilterFaceIntoTree( ds, tree );
		EmitPlanarSurf( ds );		
		if( refs > 0 )
		{
			c_surfs++;
			c_refs += refs;
		}
	}
	MsgDev( D_INFO, "%5i emited drawsurfs\n", c_surfs );
	MsgDev( D_INFO, "%5i references\n", c_refs );
	MsgDev( D_INFO, "%5i stripfaces\n", c_stripSurfaces );
	MsgDev( D_INFO, "%5i fanfaces\n", c_fanSurfaces );
}


                                                                                                                                                              