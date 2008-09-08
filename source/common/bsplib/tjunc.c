//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	tjunc.c - merge tjunctions
//=======================================================================

#include "bsplib.h"

// these should be whatever epsilon we actually expect, plus SNAP_INT_TO_FLOAT 
#define LINE_POSITION_EPSILON		0.125
#define POINT_ON_LINE_EPSILON		0.125
#define MAX_ORIGINAL_EDGES		0x10000
#define MAX_EDGE_LINES		0x10000
#define MAX_SURFACE_VERTS		512

typedef struct edgePoint_s
{
	float		intercept;
	vec3_t		point;
	struct edgePoint_s	*prev, *next;
} edgePoint_t;

typedef struct edgeLine_s
{
	vec3_t		normal1;
	float		dist1;
	
	vec3_t		normal2;
	float		dist2;
	
	vec3_t		origin;
	vec3_t		dir;

	edgePoint_t	chain;		// unused element of doubly linked list
} edgeLine_t;

typedef struct
{
	float		length;
	dvertex_t		*dv[2];
} originalEdge_t;

originalEdge_t	originalEdges[MAX_ORIGINAL_EDGES];
int		numOriginalEdges;
edgeLine_t	edgeLines[MAX_EDGE_LINES];
int		numEdgeLines;

int		c_degenerateEdges;
int		c_addedVerts;
int		c_totalVerts;
int		c_natural, c_rotate, c_cant;

/*
====================
InsertPointOnEdge
====================
*/
void InsertPointOnEdge( vec3_t v, edgeLine_t *e )
{
	vec3_t		delta;
	float		d;
	edgePoint_t	*p, *scan;

	VectorSubtract( v, e->origin, delta );
	d = DotProduct( delta, e->dir );

	p = BSP_Malloc( sizeof( edgePoint_t ));
	p->intercept = d;
	VectorCopy( v, p->point );

	if( e->chain.next == &e->chain )
	{
		e->chain.next = e->chain.prev = p;
		p->next = p->prev = &e->chain;
		return;
	}

	scan = e->chain.next;
	for( ; scan != &e->chain; scan = scan->next )
	{
		d = p->intercept - scan->intercept;
		if( d > -LINE_POSITION_EPSILON && d < LINE_POSITION_EPSILON )
		{
			Mem_Free( p );
			return;	// the point is already set
		}

		if( p->intercept < scan->intercept )
		{
			// insert here
			p->prev = scan->prev;
			p->next = scan;
			scan->prev->next = p;
			scan->prev = p;
			return;
		}
	}

	// add at the end
	p->prev = scan->prev;
	p->next = scan;
	scan->prev->next = p;
	scan->prev = p;
}


/*
====================
AddEdge
====================
*/
int AddEdge( vec3_t v1, vec3_t v2, bool createNonAxial )
{
	int		i;
	edgeLine_t	*e;
	float		d;
	vec3_t		dir;

	VectorSubtract( v2, v1, dir );
	d = VectorNormalizeLength( dir );
	if( d < 0.1f )
	{
		// if we added a 0 length vector, it would make degenerate planes
		c_degenerateEdges++;
		return -1;
	}

	if( !createNonAxial )
	{
		if( fabs( dir[0] + dir[1] + dir[2] ) != 1.0f )
		{
			if( numOriginalEdges == MAX_ORIGINAL_EDGES )
				Sys_Break( "MAX_ORIGINAL_EDGES limit exceeded\n" );
			originalEdges[numOriginalEdges].dv[0] = (dvertex_t *)v1;
			originalEdges[numOriginalEdges].dv[1] = (dvertex_t *)v2;
			originalEdges[numOriginalEdges].length = d;
			numOriginalEdges++;
			return -1;
		}
	}

	for( i = 0; i < numEdgeLines; i++ )
	{
		e = &edgeLines[i];

		d = DotProduct( v1, e->normal1 ) - e->dist1;
		if( d < -POINT_ON_LINE_EPSILON || d > POINT_ON_LINE_EPSILON )
			continue;
		d = DotProduct( v1, e->normal2 ) - e->dist2;
		if( d < -POINT_ON_LINE_EPSILON || d > POINT_ON_LINE_EPSILON )
			continue;
		d = DotProduct( v2, e->normal1 ) - e->dist1;
		if( d < -POINT_ON_LINE_EPSILON || d > POINT_ON_LINE_EPSILON )
			continue;
		d = DotProduct( v2, e->normal2 ) - e->dist2;
		if( d < -POINT_ON_LINE_EPSILON || d > POINT_ON_LINE_EPSILON )
			continue;

		// this is the edge
		InsertPointOnEdge( v1, e );
		InsertPointOnEdge( v2, e );
		return i;
	}

	// create a new edge
	if( numEdgeLines >= MAX_EDGE_LINES ) Sys_Break( "MAX_EDGE_LINES limit exceeded\n" );

	e = &edgeLines[numEdgeLines];
	numEdgeLines++;

	e->chain.next = e->chain.prev = &e->chain;

	VectorCopy( v1, e->origin );
	VectorCopy( dir, e->dir );

	VectorVectors( e->dir, e->normal1, e->normal2 );
	e->dist1 = DotProduct( e->origin, e->normal1 );
	e->dist2 = DotProduct( e->origin, e->normal2 );

	InsertPointOnEdge( v1, e );
	InsertPointOnEdge( v2, e );

	return numEdgeLines - 1;
}

/*
====================
AddSurfaceEdges
====================
*/
void AddSurfaceEdges( drawsurf_t *ds )
{
	int		i;

	for( i = 0; i < ds->numverts; i++ )
	{
		// save the edge number in the lightmap field
		// so we don't need to look it up again
		ds->verts[i].lm[0] = AddEdge( ds->verts[i].point, ds->verts[(i+1) % ds->numverts].point, false );
	}
}

/*
================
ColinearEdge
================
*/
bool ColinearEdge( vec3_t v1, vec3_t v2, vec3_t v3 )
{
	vec3_t	midpoint, dir, offset, on;
	float	d;

	VectorSubtract( v2, v1, midpoint );
	VectorSubtract( v3, v1, dir );
	d = VectorNormalizeLength( dir );
	if( d == 0 ) return false; // degenerate

	d = DotProduct( midpoint, dir );
	VectorScale( dir, d, on );
	VectorSubtract( midpoint, on, offset );
	d = VectorLength( offset );

	if( d < 0.1 )
		return true;
	return false;
}

/*
====================
FixSurfaceJunctions
====================
*/
void FixSurfaceJunctions( drawsurf_t *ds )
{
	edgeLine_t	*e;
	edgePoint_t	*p;
	int		originalVerts;
	int		counts[MAX_SURFACE_VERTS];
	int		originals[MAX_SURFACE_VERTS];
	int		firstVert[MAX_SURFACE_VERTS];
	dvertex_t		verts[MAX_SURFACE_VERTS], *v1, *v2;
	float		start, end, frac;
	int		numVerts = 0;
	int		i, j, k;
	vec3_t		delta;

	originalVerts = ds->numverts;

	for( i = 0; i < ds->numverts; i++ )
	{
		counts[i] = 0;
		firstVert[i] = numVerts;

		// copy first vert
		if( numVerts == MAX_SURFACE_VERTS ) Sys_Break( "MAX_SURFACE_VERTS limit exceeded\n" );
		verts[numVerts] = ds->verts[i];
		originals[numVerts] = i;
		numVerts++;

		// check to see if there are any t junctions before the next vert
		v1 = &ds->verts[i];
		v2 = &ds->verts[(i+1) % ds->numverts];

		j = (int)ds->verts[i].lm[0];
		if( j == -1 ) continue; // degenerate edge
		e = &edgeLines[j];
		
		VectorSubtract( v1->point, e->origin, delta );
		start = DotProduct( delta, e->dir );

		VectorSubtract( v2->point, e->origin, delta );
		end = DotProduct( delta, e->dir );

		if( start < end )
			p = e->chain.next;
		else p = e->chain.prev;

		for( ; p != &e->chain; )
		{
			if( start < end )
			{
				if( p->intercept > end - ON_EPSILON )
					break;
			}
			else
			{
				if( p->intercept < end + ON_EPSILON )
					break;
			}

			if((start < end && p->intercept > start + ON_EPSILON) || (start > end && p->intercept < start - ON_EPSILON))
			{
				// insert this point
				if( numVerts == MAX_SURFACE_VERTS ) Sys_Break( "MAX_SURFACE_VERTS limit exceeded\n" );

				// take the exact intercept point
				VectorCopy( p->point, verts[numVerts].point );

				// copy the normal
				VectorCopy( v1->normal, verts[numVerts].normal );

				// interpolate the texture coordinates
				frac = ( p->intercept - start ) / ( end - start );
				for( j = 0; j < 2; j++ )
				{
					verts[numVerts].st[j] = v1->st[j] + frac * ( v2->st[j] - v1->st[j] );
				}
				originals[numVerts] = i;
				numVerts++;
				counts[i]++;
			}

			if( start < end ) p = p->next;
			else p = p->prev;
		}
	}

	c_addedVerts += numVerts - ds->numverts;
	c_totalVerts += numVerts;


	// FIXME: check to see if the entire surface degenerated after snapping

	// rotate the points so that the initial vertex is between
	// two non-subdivided edges
	for( i = 0; i < numVerts; i++ )
	{
		if( originals[(i+1) % numVerts] == originals[i] )
			continue;
		j = (i + numVerts - 1 ) % numVerts;
		k = (i + numVerts - 2 ) % numVerts;
		if( originals[ j ] == originals[ k ] )
			continue;
		break;
	}

	if( i == 0 )
	{
		// fine the way it is
		c_natural++;
		ds->numverts = numVerts;
		ds->verts = BSP_Malloc( numVerts * sizeof( *ds->verts ));
		Mem_Copy( ds->verts, verts, numVerts * sizeof( *ds->verts ));
		return;
	}

	if( i == numVerts )
	{
		// create a vertex in the middle to start the fan
		c_cant++;
#if 0
		memset( &verts[numVerts], 0, sizeof( verts[numVerts] ));
		for( i = 0; i < numVerts; i++ )
			for( j = 0; j < 10; j++ )
				verts[numVerts].point[j] += verts[i].point[j];
		for( j = 0 ; j < 10 ; j++ )
			verts[numVerts].point[j] /= numVerts;
		i = numVerts;
		numVerts++;
#endif
	}
	else c_rotate++; // just rotate the vertexes

	ds->numverts = numVerts;
	ds->verts = BSP_Malloc( numVerts * sizeof( *ds->verts ));

	for( j = 0; j < ds->numverts; j++ )
		ds->verts[j] = verts[( j + i ) % ds->numverts];
}

/*
================
EdgeCompare
================
*/
int EdgeCompare( const void *elem1, const void *elem2 )
{
	float	d1, d2;

	d1 = ((originalEdge_t *)elem1)->length;
	d2 = ((originalEdge_t *)elem2)->length;

	if( d1 < d2 ) return -1;
	if( d2 > d1 ) return 1;
	return 0;
}


/*
================
FixTJunctions

Call after the surface list has been pruned, but before lightmap allocation
================
*/
void FixTJunctions( bsp_entity_t *ent )
{
	int		i, axialEdgeLines;
	drawsurf_t	*ds;
	originalEdge_t	*e;

	MsgDev( D_INFO, "----- FixTJuncs -----\n" );

	numEdgeLines = 0;
	numOriginalEdges = 0;

	// add all the edges
	// this actually creates axial edges, but it
	// only creates originalEdge_t structures
	// for non-axial edges
	for( i = ent->firstsurf; i < numdrawsurfs; i++ )
	{
		ds = &drawsurfs[i];
		AddSurfaceEdges( ds );
	}

	axialEdgeLines = numEdgeLines;

	// sort the non-axial edges by length
	qsort( originalEdges, numOriginalEdges, sizeof(originalEdges[0]), EdgeCompare );

	// add the non-axial edges, longest first
	// this gives the most accurate edge description
	for( i = 0; i < numOriginalEdges; i++ )
	{
		e = &originalEdges[i];
		e->dv[0]->lm[0] = AddEdge( e->dv[0]->point, e->dv[1]->point, true );
	}

	MsgDev( D_INFO, "%6i axial edge lines\n", axialEdgeLines );
	MsgDev( D_INFO, "%6i non-axial edge lines\n", numEdgeLines - axialEdgeLines );
	MsgDev( D_INFO, "%6i degenerate edges\n", c_degenerateEdges );

	// insert any needed vertexes
	for( i = ent->firstsurf; i < numdrawsurfs; i++ )
	{
		ds = &drawsurfs[i];
		FixSurfaceJunctions( ds );
	}

	MsgDev( D_INFO, "%6i verts added for tjunctions\n", c_addedVerts );
	MsgDev( D_INFO, "%6i total verts\n", c_totalVerts );
	MsgDev( D_INFO, "%6i naturally ordered\n", c_natural );
	MsgDev( D_INFO, "%6i rotated orders\n", c_rotate );
	MsgDev( D_INFO, "%6i can't order\n", c_cant );
}
