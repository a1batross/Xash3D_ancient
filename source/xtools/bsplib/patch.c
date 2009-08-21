/* -------------------------------------------------------------------------------

Copyright (C) 1999-2007 id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

----------------------------------------------------------------------------------

This code has been altered significantly from its original form, to support
several games based on the Quake III Arena engine, in the form of "Q3Map2."

------------------------------------------------------------------------------- */

#define PATCH_C

#include "q3map2.h"

/*
=================
ExpandLongestCurve

finds length of quadratic curve specified and determines if length is longer than the supplied max
=================
*/

#define APPROX_SUBDIVISION	8

static void ExpandLongestCurve( float *longestCurve, vec3_t a, vec3_t b, vec3_t c )
{
	int		i;
	float		t, len;
	vec3_t		ab, bc, ac, pt, last, delta;
	
	VectorSubtract( b, a, ab );
	if( VectorNormalizeLength( ab ) < 0.125f )
		return;
	VectorSubtract( c, b, bc );
	if( VectorNormalizeLength( bc ) < 0.125f )
		return;
	VectorSubtract( c, a, ac );
	if( VectorNormalizeLength( ac ) < 0.125f )
		return;
	
	// if all 3 vectors are the same direction, then this edge is linear, so we ignore it
	if( DotProduct( ab, bc ) > 0.99f && DotProduct( ab, ac ) > 0.99f )
		return;
	
	VectorSubtract( b, a, ab );
	VectorSubtract( c, b, bc );
	
	VectorCopy( a, last );
	for( i = 0, len = 0.0f, t = 0.0f; i < APPROX_SUBDIVISION; i++, t += (1.0f / APPROX_SUBDIVISION) )
	{
		delta[0] = ((1.0f - t) * ab[0]) + (t * bc[0]);
		delta[1] = ((1.0f - t) * ab[1]) + (t * bc[1]);
		delta[2] = ((1.0f - t) * ab[2]) + (t * bc[2]);
		
		// add to first point and calculate pt-pt delta
		VectorAdd( a, delta, pt );
		VectorSubtract( pt, last, delta );
		
		// add it to length and store last point
		len += VectorLength( delta );
		VectorCopy( pt, last );
	}
	
	if( len > *longestCurve ) *longestCurve = len;
}



/*
=================
ExpandMaxIterations

determines how many iterations a quadratic curve needs to be subdivided with to fit the specified error
=================
*/
static void ExpandMaxIterations( int *maxIterations, int maxError, vec3_t a, vec3_t b, vec3_t c )
{
	int		i, j;
	vec3_t		prev, next, mid, delta, delta2;
	float		len, len2;
	int		numPoints, iterations;
	vec3_t		points[MAX_EXPANDED_AXIS];
	
	
	numPoints = 3;
	VectorCopy( a, points[0] );
	VectorCopy( b, points[1] );
	VectorCopy( c, points[2] );

	for( i = 0; i + 2 < numPoints; i += 2 )
	{
		if( numPoints + 2 >= MAX_EXPANDED_AXIS )
			break;
		
		for( j = 0; j < 3; j++ )
		{
			prev[j] = points[ i + 1 ][j] - points[i][j]; 
			next[j] = points[ i + 2 ][j] - points[ i + 1 ][j]; 
			mid[j] = (points[i][j] + points[ i + 1 ][j] * 2.0f + points[ i + 2 ][j]) * 0.25f;
		}
		
		// see if this midpoint is off far enough to subdivide
		VectorSubtract( points[i + 1], mid, delta );
		len = VectorLength( delta );
		if( len < maxError ) continue;
		
		numPoints += 2;
		
		for( j = 0; j < 3; j++ )
		{
			prev[j] = 0.5f * (points[i][j] + points[ i + 1 ][j]);
			next[j] = 0.5f * (points[ i + 1 ][j] + points[ i + 2 ][j]);
			mid[j] = 0.5f * (prev[j] + next[j]);
		}
		
		for( j = numPoints - 1; j > i + 3; j-- )
			VectorCopy( points[ j - 2 ], points[j] );
		
		VectorCopy( prev, points[i + 1] );
		VectorCopy( mid,  points[i + 2] );
		VectorCopy( next, points[i + 3] );

		// back up and recheck this set again, it may need more subdivision
		i -= 2;
	}
	
	for( i = 1; i < numPoints; i += 2 )
	{
		for( j = 0; j < 3; j++ )
		{
			prev[j] = 0.5f * (points[i][j] + points[ i + 1 ][j] );
			next[j] = 0.5f * (points[i][j] + points[ i - 1 ][j] );
			points[i][j] = 0.5f * (prev[j] + next[j]);
		}
	}
	
	// eliminate linear sections
	for( i = 0; i + 2 < numPoints; i++ )
	{
		VectorSubtract( points[i + 1], points[i], delta );
		len = VectorNormalizeLength( delta );
		VectorSubtract( points[i + 2], points[i + 1], delta2 );
		len2 = VectorNormalizeLength( delta2 );
		
		// if either edge is degenerate, then eliminate it
		if( len < 0.0625f || len2 < 0.0625f || DotProduct( delta, delta2 ) >= 1.0f )
		{
			for( j = i + 1; j + 1 < numPoints; j++ )
				VectorCopy( points[j + 1], points[j] );
			numPoints--;
			continue;
		}
	}
	
	// the number of iterations is 2^(points - 1) - 1
	numPoints >>= 1;
	iterations = 0;
	while( numPoints > 1 )
	{
		numPoints >>= 1;
		iterations++;
	}
	
	if( iterations > *maxIterations ) *maxIterations = iterations;
}



/*
=================
ParsePatch

creates a mapDrawSurface_t from the patch text
=================
*/
void ParsePatch( bool onlyLights )
{
	float			info[5];
	int			i, j, k;
	parseMesh_t		*pm;
	char			texture[MAX_SHADERPATH];
	char			shader[MAX_SHADERPATH];
	mesh_t			m;
	token_t			token;
	bspDrawVert_t		*verts;
	epair_t			*ep;
	vec4_t			delta, delta2, delta3;
	bool			degenerate;
	float			longestCurve;
	int			maxIterations;
	
	
	Com_CheckToken( mapfile, "{" );
	
	Com_ReadToken( mapfile, SC_ALLOW_PATHNAMES|SC_PARSE_GENERIC, &token );
	com.strncpy( texture, token.string, sizeof( texture ));
	
	Com_Parse1DMatrix( mapfile, 5, info );
	m.width = info[0];
	m.height = info[1];
	m.verts = verts = Malloc( m.width * m.height * sizeof( m.verts[0] ));
	
	if( m.width < 0 || m.width > MAX_PATCH_SIZE || m.height < 0 || m.height > MAX_PATCH_SIZE )
		Sys_Break( "ParsePatch: bad size\n" );
	
	Com_CheckToken( mapfile, "(" );
	for( j = 0; j < m.width ; j++ )
	{
		Com_CheckToken( mapfile, "(" );
		for( i = 0; i < m.height ; i++ )
		{
			Com_Parse1DMatrix( mapfile, 5, verts[i * m.width + j].xyz );
			
			for( k = 0; k < MAX_LIGHTMAPS; k++ )
			{
				verts[i * m.width + j].color[k][0] = 0xFF;
				verts[i * m.width + j].color[k][1] = 0xFF;
				verts[i * m.width + j].color[k][2] = 0xFF;
				verts[i * m.width + j].color[k][3] = 0xFF;
			}
		}
		Com_CheckToken( mapfile, ")" );
	}
	Com_CheckToken( mapfile, ")" );

	// if brush primitives format, we may have some epairs to ignore here
	Com_ReadToken( mapfile, SC_ALLOW_PATHNAMES|SC_PARSE_GENERIC, &token );
	if( g_bBrushPrimit == BRUSH_RADIANT && com.strcmp( token.string, "}" ))
		ep = ParseEpair( mapfile, &token );
	else Com_SaveToken( mapfile, &token );

	Com_CheckToken( mapfile, "}" );
	Com_CheckToken( mapfile, "}" );
	
	if( noCurveBrushes || onlyLights ) return;

	// delete and warn about degenerate patches
	j = (m.width * m.height);
	VectorClear( delta );
	delta[3] = 0;
	degenerate = true;
	
	// find first valid vector
	for( i = 1; i < j && delta[3] == 0; i++ )
	{
		VectorSubtract( m.verts[0].xyz, m.verts[i].xyz, delta );
		delta[3] = VectorNormalizeLength( delta );
	}
	
	// secondary degenerate test
	if( delta[3] == 0 ) degenerate = true;
	else
	{
		// if all vectors match this or are zero, then this is a degenerate patch
		for( i = 1; i < j && degenerate == true; i++ )
		{
			VectorSubtract( m.verts[0].xyz, m.verts[i].xyz, delta2 );
			delta2[3] = VectorNormalizeLength( delta2 );
			if( delta2[3] != 0 )
			{
				VectorCopy( delta2, delta3 );
				delta3[3] = delta2[3];
				VectorNegate( delta3, delta3 );
				
				if( !VectorCompare( delta, delta2 ) && !VectorCompare( delta, delta3 ))
					degenerate = false;
			}
		}
	}
	
	if( degenerate )
	{
		MsgDev( D_ERROR, "Entity %i, Brush %i degenerate patch\n", mapEnt->mapEntityNum, entitySourceBrushes );
		Mem_Free( m.verts );
		return;
	}
	
	// find longest curve on the mesh
	longestCurve = 0.0f;
	maxIterations = 0;
	for( j = 0; j + 2 < m.width; j += 2 )
	{
		for( i = 0; i + 2 < m.height; i += 2 )
		{
			ExpandLongestCurve( &longestCurve, verts[i * m.width + j].xyz, verts[i * m.width + (j + 1)].xyz, verts[i * m.width + (j + 2)].xyz ); // row
			ExpandLongestCurve( &longestCurve, verts[i * m.width + j].xyz, verts[(i + 1) * m.width + j].xyz, verts[(i + 2) * m.width + j].xyz ); // col
			ExpandMaxIterations( &maxIterations, patch_subdivide->integer, verts[i * m.width + j].xyz, verts[i * m.width + (j + 1)].xyz, verts[i * m.width + (j + 2)].xyz ); // row
			ExpandMaxIterations( &maxIterations, patch_subdivide->integer, verts[i * m.width + j].xyz, verts[(i + 1) * m.width + j].xyz, verts[(i + 2) * m.width + j].xyz ); // col
		}
	}
	
	pm = Malloc( sizeof( *pm ));
	
	pm->entityNum = mapEnt->mapEntityNum;
	pm->brushNum = entitySourceBrushes;
	
	com.snprintf( shader, sizeof( shader ), "textures/%s", texture );
	pm->shaderInfo = ShaderInfoForShader( shader );
	pm->mesh = m;
	
	pm->longestCurve = longestCurve;
	pm->maxIterations = maxIterations;
	
	pm->next = mapEnt->patches;
	mapEnt->patches = pm;
}

/*
=================
GrowGroup_r

recursively adds patches to a lod group
=================
*/
static void GrowGroup_r( parseMesh_t *pm, int patchNum, int patchCount, parseMesh_t **meshes, byte *bordering, byte *group )
{
	int		i;
	const byte	*row;
	
	if( group[patchNum] ) return;
	
	group[patchNum] = 1;
	row = bordering + patchNum * patchCount;
	
	if( meshes[patchNum]->longestCurve > pm->longestCurve )
		pm->longestCurve = meshes[patchNum]->longestCurve;
	if( meshes[patchNum]->maxIterations > pm->maxIterations )
		pm->maxIterations = meshes[patchNum]->maxIterations;
	
	for( i = 0; i < patchCount; i++ )
		if( row[i] ) GrowGroup_r( pm, i, patchCount, meshes, bordering, group );
}


/*
=================
PatchMapDrawSurfs

any patches that share an edge need to choose their
level of detail as a unit, otherwise the edges would
pull apart.
=================
*/
void PatchMapDrawSurfs( entity_t *e )
{
	int		i, j, k, l, c1, c2;
	parseMesh_t	*pm;
	parseMesh_t	*check, *scan;
	mapDrawSurface_t	*ds;
	int		patchCount, groupCount;
	bspDrawVert_t	*v1, *v2;
	vec3_t		bounds[2];
	byte		*bordering;
	parseMesh_t	*meshes[MAX_MAP_DRAW_SURFS];
	qb_t		grouped[MAX_MAP_DRAW_SURFS];
	byte		group[MAX_MAP_DRAW_SURFS];
	
	MsgDev( D_NOTE, "--- PatchMapDrawSurfs ---\n" );

	patchCount = 0;
	for( pm = e->patches; pm; pm = pm->next )
	{
		meshes[patchCount] = pm;
		patchCount++;
	}

	if( !patchCount ) return;
	bordering = Malloc( patchCount * patchCount );

	// build the bordering matrix
	for( k = 0; k < patchCount; k++ )
	{
		bordering[k*patchCount+k] = 1;

		for( l = k+1; l < patchCount; l++ )
		{
			check = meshes[k];
			scan = meshes[l];
			c1 = scan->mesh.width * scan->mesh.height;
			v1 = scan->mesh.verts;

			for( i = 0; i < c1; i++, v1++ )
			{
				c2 = check->mesh.width * check->mesh.height;
				v2 = check->mesh.verts;
				for( j = 0; j < c2; j++, v2++ )
				{
					if( fabs( v1->xyz[0] - v2->xyz[0] ) < 1.0f && fabs( v1->xyz[1] - v2->xyz[1] ) < 1.0f && fabs( v1->xyz[2] - v2->xyz[2] ) < 1.0f )
						break;
				}
				if( j != c2 ) break;
			}
			if( i != c1 )
			{
				// we have a connection
				bordering[k*patchCount+l] = bordering[l*patchCount+k] = 1;
			}
			else
			{
				// no connection
				bordering[k*patchCount+l] = bordering[l*patchCount+k] = 0;
			}

		}
	}

	// build groups
	Mem_Set( grouped, 0, patchCount );
	groupCount = 0;
	for( i = 0; i < patchCount; i++ )
	{
		scan = meshes[i];
		
		if( !grouped[i] ) groupCount++;
		
		// recursively find all patches that belong in the same group
		Mem_Set( group, 0, patchCount );
		GrowGroup_r( scan, i, patchCount, meshes, bordering, group );
		
		ClearBounds( bounds[0], bounds[1] );
		for( j = 0; j < patchCount; j++ )
		{
			if( group[j] )
			{
				grouped[j] = true;
				check = meshes[j];
				c1 = check->mesh.width * check->mesh.height;
				v1 = check->mesh.verts;
				for( k = 0; k < c1; k++, v1++ )
					AddPointToBounds( v1->xyz, bounds[0], bounds[1] );
			}
		}

		// create drawsurf
		scan->grouped = true;
		ds = DrawSurfaceForMesh( e, scan, NULL );
		VectorCopy( bounds[0], ds->bounds[0] );
		VectorCopy( bounds[1], ds->bounds[1] );
	}
	
	MsgDev( D_INFO, "%9d patches\n", patchCount );
	MsgDev( D_INFO, "%9d patch LOD groups\n", groupCount );
}