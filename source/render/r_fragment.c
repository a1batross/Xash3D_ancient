//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        r_fragment.c - fragment clipping
//=======================================================================

#include "r_local.h"
#include "mathlib.h"

#define MAX_FRAGMENT_VERTS		128
#define MAX_DECAL_FRAGMENTS		128
#define MAX_DECAL_VERTS		384

static int		r_numFragmentVerts;
static int		r_maxFragmentVerts;
static vec3_t		*r_fragmentVerts;
static int		r_numFragments;
static int		r_maxFragments;
static decalFragment_t	*r_fragments;
static cplane_t		r_fragmentPlanes[6];
static int		r_fragmentCount;

/*
=================
R_ClipFragment_r
=================
*/
static void R_ClipFragment_r( int numVerts, vec3_t verts, int stage, decalFragment_t *df )
{
	int	i, f;
	float	*v;
	bool	frontSide;
	vec3_t	front[MAX_FRAGMENT_VERTS];
	float	dists[MAX_FRAGMENT_VERTS];
	int	sides[MAX_FRAGMENT_VERTS];
	cplane_t	*plane;
	float	dist;

	if( numVerts > MAX_FRAGMENT_VERTS - 2 )
		Host_Error( "R_ClipFragment: MAX_FRAGMENT_VERTS limit exceeded\n" );

	if( stage == 6 )
	{
		// fully clipped
		if( numVerts > 2 )
		{
			if( r_numFragmentVerts + numVerts > r_maxFragmentVerts )
				return;

			df->firstVert = r_numFragmentVerts;
			df->numVerts = numVerts;

			for( i = 0, v = verts; i < numVerts; i++, v += 3 )
				VectorCopy( v, r_fragmentVerts[r_numFragmentVerts+i] );
			r_numFragmentVerts += numVerts;
		}
		return;
	}

	frontSide = false;
	plane = &r_fragmentPlanes[stage];
	for( i = 0, v = verts; i < numVerts; i++, v += 3 )
	{
		if( plane->type < 3 )
			dists[i] = dist = v[plane->type] - plane->dist;
		else dists[i] = dist = DotProduct( v, plane->normal ) - plane->dist;

		if( dist > ON_EPSILON )
		{
			frontSide = true;
			sides[i] = SIDE_FRONT;
		}
		else if( dist < -ON_EPSILON )
			sides[i] = SIDE_BACK;
		else sides[i] = SIDE_ON;
	}

	if( !frontSide ) return; // not clipped

	// clip it
	dists[i] = dists[0];
	sides[i] = sides[0];
	VectorCopy( verts, (verts + (i * 3)));

	for( i = f = 0, v = verts; i < numVerts; i++, v += 3 )
	{
		switch( sides[i] )
		{
		case SIDE_FRONT:
			VectorCopy( v, front[f] );
			f++;
			break;
		case SIDE_BACK:
			break;
		case SIDE_ON:
			VectorCopy(v, front[f]);
			f++;
			break;
		}

		if( sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i] )
			continue;

		dist = dists[i] / (dists[i] - dists[i+1]);
		front[f][0] = v[0] + (v[3] - v[0]) * dist;
		front[f][1] = v[1] + (v[4] - v[1]) * dist;
		front[f][2] = v[2] + (v[5] - v[2]) * dist;
		f++;
	}

	// continue
	R_ClipFragment_r( f, front[0], stage+1, df );
}

/*
=================
R_ClipFragmentToSurface
=================
*/
static void R_ClipFragmentToSurface( surface_t *surf )
{
	decalFragment_t	*df;
	surfPoly_t	*p;
	vec3_t		verts[MAX_FRAGMENT_VERTS];
	int		i;

	// copy vertex data and clip to each triangle
	for( p = surf->poly; p; p = p->next )
	{
		for( i = 0; i < p->numIndices; i += 3 )
		{
			df = &r_fragments[r_numFragments];
			df->firstVert = df->numVerts = 0;

			VectorCopy( p->vertices[p->indices[i+0]].xyz, verts[0] );
			VectorCopy( p->vertices[p->indices[i+1]].xyz, verts[1] );
			VectorCopy( p->vertices[p->indices[i+2]].xyz, verts[2] );

			R_ClipFragment_r( 3, verts[0], 0, df );

			if( df->numVerts )
			{
				r_numFragments++;
				if( r_numFragmentVerts == r_maxFragmentVerts || r_numFragments == r_maxFragments )
					return; // already reached the limit
			}
		}
	}
}

/*
=================
R_RecursiveFragmentNode
=================
*/
static void R_RecursiveFragmentNode( node_t *node, const vec3_t origin, const vec3_t normal, float radius )
{
	int	i;
	float	dist;
	cplane_t	*plane;
	surface_t	*surf;

	if( node->contents != CONTENTS_NODE )
		return;

	if( r_numFragmentVerts == r_maxFragmentVerts || r_numFragments == r_maxFragments )
		return; // already reached the limit somewhere else

	// find which side of the node we are on
	plane = node->plane;
	if( plane->type < 3 )
		dist = origin[plane->type] - plane->dist;
	else dist = DotProduct( origin, plane->normal ) - plane->dist;

	// go down the appropriate sides
	if( dist > radius )
	{
		R_RecursiveFragmentNode( node->children[0], origin, normal, radius );
		return;
	}
	if( dist < -radius )
	{
		R_RecursiveFragmentNode( node->children[1], origin, normal, radius );
		return;
	}

	// clip to each surface
	surf = r_worldModel->surfaces + node->firstSurface;
	for( i = 0; i < node->numSurfaces; i++, surf++ )
	{
		if( r_numFragmentVerts == r_maxFragmentVerts || r_numFragments == r_maxFragments )
			break;	// already reached the limit

		if( surf->fragmentFrame == r_fragmentCount )
			continue;	// already checked this surface in another node
		surf->fragmentFrame = r_fragmentCount;

		if( surf->texInfo->surfaceFlags & (SURF_SKY|SURF_3DSKY|SURF_NODRAW|SURF_WARP))
			continue;	// don't bother clipping

		if( surf->texInfo->shader->flags & SHADER_NOFRAGMENTS )
			continue;	// don't bother clipping

		if( !BoundsAndSphereIntersect( surf->mins, surf->maxs, origin, radius ))
			continue;	// no intersection

		if(!( surf->flags & SURF_PLANEBACK ))
		{
			if( DotProduct( normal, surf->plane->normal ) < 0.5f )
				continue;	// greater than 60 degrees
		}
		else
		{
			if( DotProduct( normal, surf->plane->normal ) > -0.5f)
				continue;	// greater than 60 degrees
		}

		// clip to the surface
		R_ClipFragmentToSurface( surf );
	}

	// recurse down the children
	R_RecursiveFragmentNode( node->children[0], origin, normal, radius );
	R_RecursiveFragmentNode( node->children[1], origin, normal, radius );
}

/*
=================
R_DecalFragments
=================
*/
int R_DecalFragments( const vec3_t origin, const matrix3x3 matrix, float radius, int maxVerts, vec3_t *verts, int maxFragments, decalFragment_t *fragments )
{
	int	i;
	float	dot;

	if( !r_worldModel ) return 0;	// map not loaded
	r_fragmentCount++;		// for multi-check avoidance

	// initialize fragments
	r_numFragmentVerts = 0;
	r_maxFragmentVerts = maxVerts;
	r_fragmentVerts = verts;

	r_numFragments = 0;
	r_maxFragments = maxFragments;
	r_fragments = fragments;

	// calculate clipping planes
	for( i = 0; i < 3; i++ )
	{
		dot = DotProduct( origin, matrix[i] );

		VectorCopy( matrix[i], r_fragmentPlanes[i*2+0].normal );
		r_fragmentPlanes[i*2+0].dist = dot - radius;
		PlaneClassify( &r_fragmentPlanes[i*2+0] );

		VectorNegate( matrix[i], r_fragmentPlanes[i*2+1].normal );
		r_fragmentPlanes[i*2+1].dist = -dot - radius;
		PlaneClassify( &r_fragmentPlanes[i*2+1] );
	}

	// clip against world geometry
	R_RecursiveFragmentNode( r_worldModel->nodes, origin, matrix[0], radius );
	return r_numFragments;
}

/*
=================
R_ImpactMark

temporary marks will be inmediately passed to the renderer
=================
*/
void R_ImpactMark( vec3_t org, vec3_t dir, float rot, float radius, vec4_t rgba, bool fade, shader_t shader, bool temp )
{
	int		i, j;
	vec3_t		delta;
	matrix3x3		matrix;
	int		numFragments;
	decalFragment_t	decalFragments[MAX_DECAL_FRAGMENTS], *df;
	vec3_t		decalVerts[MAX_DECAL_VERTS];
	polyVert_t	verts[MAX_VERTS_ON_POLY];

	// find orientation vectors
	VectorNormalize2( dir, matrix[0] );
	PerpendicularVector( matrix[1], matrix[0] );
	RotatePointAroundVector( matrix[2], matrix[0], matrix[1], rot );
	CrossProduct( matrix[0], matrix[2], matrix[1] );

	// get the clipped decal fragments
	numFragments = R_DecalFragments( org, matrix, radius, MAX_DECAL_VERTS, decalVerts, MAX_DECAL_FRAGMENTS, decalFragments );
	if( !numFragments ) return;

	VectorScale( matrix[1], 0.5 / radius, matrix[1] );
	VectorScale( matrix[2], 0.5 / radius, matrix[2] );

	for( i = 0, df = decalFragments; i < numFragments; i++, df++ )
	{
		if( !df->numVerts ) continue;
		if( df->numVerts > MAX_VERTS_ON_POLY )
			df->numVerts = MAX_VERTS_ON_POLY;

		// if it is a temporary decal, pass it to the renderer without storing
		if( temp )
		{
			for( j = 0; j < df->numVerts; j++ )
			{
				VectorCopy( decalVerts[df->firstVert + j], verts[j].point );
				VectorSubtract( verts[j].point, org, delta );
				verts[j].st[0] = 0.5 + DotProduct( delta, matrix[1] );
				verts[j].st[1] = 0.5 + DotProduct( delta, matrix[2] );
				Vector4Copy( rgba, verts[j].modulate );
			}
			R_AddPolyToScene( shader, df->numVerts, verts );
			continue;
		}
		ri.AddDecal( org, matrix, shader, rgba, fade, df, decalVerts );
	}
}