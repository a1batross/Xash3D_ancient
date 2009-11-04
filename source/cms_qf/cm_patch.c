//=======================================================================
//			Copyright XashXT Group 2009 ©
//			cm_patch.c - curves collision
//=======================================================================

#include "cm_local.h"
#include "mathlib.h"

/*
===============================================================================

					PATCH LOADING

===============================================================================
*/
#define CM_SUBDIV_LEVEL		(16)

/*
=================
CM_CreateFacetFromPoints
=================
*/
static int CM_CreateFacetFromPoints( cbrush_t *facet, vec3_t *verts, int numverts, dshader_t *shader )
{
	int		i, j, k;
	int		axis, dir;
	cbrushside_t	*s;
	cplane_t		*planes;
	vec3_t		normal, mins, maxs;
	float		d, dist;
	cplane_t		mainplane;
	vec3_t		vec, vec2;
	int		numbrushplanes;
	cplane_t		brushplanes[32];

	// set default values for brush
	facet->numsides = 0;
	facet->brushsides = NULL;
	facet->contents = shader->contentFlags;

	// calculate plane for this triangle
	CM_PlaneFromPoints( verts, &mainplane );
	if( CM_ComparePlanes( mainplane.normal, mainplane.dist, vec3_origin, 0.0f ))
		return 0;

	// test a quad case
	if( numverts > 3 )
	{
		d = DotProduct( verts[3], mainplane.normal ) - mainplane.dist;
		if( d < -0.1f || d > 0.1f )
			return 0;

		if( 0 )
		{
			vec3_t	v[3];
			cplane_t	plane;

			// try different combinations of planes
			for( i = 1; i < 4; i++ )
			{
				VectorCopy( verts[(i+0)],   v[0] );
				VectorCopy( verts[(i+1)%4], v[1] );
				VectorCopy( verts[(i+2)%4], v[2] );
				CM_PlaneFromPoints( v, &plane );

				if( fabs( DotProduct( mainplane.normal, plane.normal )) < 0.9f )
					return 0;
			}
		}
	}

	numbrushplanes = 0;

	// add front plane
	CM_SnapPlane( mainplane.normal, &mainplane.dist );
	VectorCopy( mainplane.normal, brushplanes[numbrushplanes].normal );
	brushplanes[numbrushplanes].dist = mainplane.dist; numbrushplanes++;

	// calculate mins & maxs
	ClearBounds( mins, maxs );
	for( i = 0; i < numverts; i++ )
		AddPointToBounds( verts[i], mins, maxs );

	// add the axial planes
	for( axis = 0; axis < 3; axis++ )
	{
		for( dir = -1; dir <= 1; dir += 2 )
		{
			for( i = 0; i < numbrushplanes; i++ )
			{
				if( brushplanes[i].normal[axis] == dir )
					break;
			}

			if( i == numbrushplanes )
			{
				VectorClear( normal );
				normal[axis] = dir;
				if( dir == 1 ) dist = maxs[axis];
				else dist = -mins[axis];

				VectorCopy( normal, brushplanes[numbrushplanes].normal );
				brushplanes[numbrushplanes].dist = dist; numbrushplanes++;
			}
		}
	}

	// add the edge bevels
	for( i = 0; i < numverts; i++ )
	{
		j = (i + 1) % numverts;
		k = (i + 2) % numverts;

		VectorSubtract( verts[i], verts[j], vec );
		if( VectorNormalizeLength( vec ) < 0.5f )
			continue;

		CM_SnapVector( vec );
		for( j = 0; j < 3; j++ )
		{
			if( vec[j] == 1 || vec[j] == -1 )
				break; // axial
		}
		if( j != 3 ) continue; // only test non-axial edges

		// try the six possible slanted axials from this edge
		for( axis = 0; axis < 3; axis++ )
		{
			for( dir = -1; dir <= 1; dir += 2 )
			{
				// construct a plane
				VectorClear( vec2 );
				vec2[axis] = dir;
				CrossProduct( vec, vec2, normal );
				if( VectorNormalizeLength( normal ) < 0.5f )
					continue;
				dist = DotProduct( verts[i], normal );

				for( j = 0; j < numbrushplanes; j++ )
				{
					// if this plane has already been used, skip it
					if( CM_ComparePlanes( brushplanes[j].normal, brushplanes[j].dist, normal, dist ))
						break;
				}
				if( j != numbrushplanes ) continue;

				// if all other points are behind this plane, it is a proper edge bevel
				for( j = 0; j < numverts; j++ )
				{
					if( j != i )
					{
						d = DotProduct( verts[j], normal ) - dist;
						// point in front: this plane isn't part of the outer hull
						if( d > 0.1f ) break;
					}
				}
				if( j != numverts ) continue;

				// add this plane
				VectorCopy( normal, brushplanes[numbrushplanes].normal );
				brushplanes[numbrushplanes].dist = dist; numbrushplanes++;
			}
		}
	}

	if( !numbrushplanes ) return 0;

	// add brushsides
	s = facet->brushsides = Mem_Alloc( cms.mempool, numbrushplanes * sizeof( *s ));
	planes = Mem_Alloc( cms.mempool, numbrushplanes * sizeof( cplane_t ));

	for( i = 0; i < numbrushplanes; i++, s++ )
	{
		planes[i] = brushplanes[i];
		CM_SnapPlane( planes[i].normal, &planes[i].dist );
		CM_CategorizePlane( &planes[i] );

		s->plane = &planes[i];
		s->shadernum = shader - cm.shaders;
	}

	return( facet->numsides = numbrushplanes );
}

/*
=================
CM_CreatePatch
=================
*/
void CM_CreatePatch( cface_t *patch, dshader_t *shader, vec3_t *verts, int *patch_cp )
{
	int	step[2], size[2], flat[2], i, u, v;
	cbrush_t	*facets;
	vec3_t	*points;
	vec3_t	tverts[4];

	// find the degree of subdivision in the u and v directions
	Patch_GetFlatness( CM_SUBDIV_LEVEL, (vec_t *)verts[0], 3, patch_cp, flat );

	step[0] = 1 << flat[0];
	step[1] = 1 << flat[1];
	size[0] = ( patch_cp[0] >> 1 ) * step[0] + 1;
	size[1] = ( patch_cp[1] >> 1 ) * step[1] + 1;
	if( size[0] <= 0 || size[1] <= 0 )
		return;

	points = Mem_Alloc( cms.mempool, size[0] * size[1] * sizeof( vec3_t ));
	facets = Mem_Alloc( cms.mempool, (size[0]-1) * (size[1]-1) * 2 * sizeof( cbrush_t ));

	// fill in
	Patch_Evaluate( verts[0], patch_cp, step, points[0], 3 );

	patch->numfacets = 0;
	patch->facets = NULL;
	ClearBounds( patch->mins, patch->maxs );

	// create a set of facets
	for( v = 0; v < size[1]-1; v++ )
	{
		for( u = 0; u < size[0]-1; u++ )
		{
			i = v * size[0] + u;
			VectorCopy( points[i], tverts[0] );
			VectorCopy( points[i + size[0]], tverts[1] );
			VectorCopy( points[i + size[0] + 1], tverts[2] );
			VectorCopy( points[i + 1], tverts[3] );

			for( i = 0; i < 4; i++ )
				AddPointToBounds( tverts[i], patch->mins, patch->maxs );

			// try to create one facet from a quad
			if( CM_CreateFacetFromPoints( &facets[patch->numfacets], tverts, 4, shader ))
			{
				patch->numfacets++;
				continue;
			}

			VectorCopy( tverts[3], tverts[2] );

			// create two facets from triangles
			if( CM_CreateFacetFromPoints( &facets[patch->numfacets], tverts, 3, shader ))
				patch->numfacets++;

			VectorCopy( tverts[2], tverts[0] );
			VectorCopy( points[v * size[0] + u + size[0] + 1], tverts[2] );

			if( CM_CreateFacetFromPoints( &facets[patch->numfacets], tverts, 3, shader ))
				patch->numfacets++;
		}
	}

	if( !patch->numfacets )
	{
		ClearBounds( patch->mins, patch->maxs );
		Mem_Free( points );
		Mem_Free( facets );
		return;
	}

	patch->contents = shader->contentFlags;
	patch->facets = Mem_Alloc( cms.mempool, patch->numfacets * sizeof( cbrush_t ));
	Mem_Copy( patch->facets, facets, patch->numfacets * sizeof( cbrush_t ));

	Mem_Free( points );
	Mem_Free( facets );

	for( i = 0; i < 3; i++ )
	{
		// spread the mins / maxs by a pixel
		patch->mins[i] -= 1;
		patch->maxs[i] += 1;
	}
}