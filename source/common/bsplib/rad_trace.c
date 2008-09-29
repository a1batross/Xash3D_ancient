//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	rad_trace.c - ray casting
//=======================================================================

#include "bsplib.h"
#include "const.h"
#include "matrixlib.h"

#define Vector2Copy( a, b )		((b)[ 0 ] = (a)[ 0 ], (b)[ 1 ] = (a)[ 1 ])

#define MAX_NODE_ITEMS		5
#define MAX_NODE_TRIANGLES		5
#define MAX_TRACE_DEPTH		32
#define MIN_NODE_SIZE		32.0f

#define GROW_TRACE_INFOS		32768
#define GROW_TRACE_WINDINGS		65536
#define GROW_TRACE_TRIANGLES		131072
#define GROW_TRACE_NODES		16384
#define GROW_NODE_ITEMS		16

#define MAX_TW_VERTS		12
#define TW_ON_EPSILON		0.25f
#define TRACE_ON_EPSILON		0.1f
#define TRACE_LEAF			-1
#define TRACE_LEAF_SOLID		-2

typedef struct traceVert_s
{
	vec3_t		point;
	float		st[2];
} traceVert_t;

typedef struct traceInfo_s
{
	bsp_shader_t	*si;
	int		surfaceNum, castShadows;
} traceInfo_t;

typedef struct traceWinding_s
{
	vec4_t		plane;
	int		infoNum, numVerts;
	traceVert_t	v[MAX_TW_VERTS];
} traceWinding_t;

typedef struct traceTriangle_s
{
	vec3_t		edge1, edge2;
	int		infoNum;
	traceVert_t	v[3];
} traceTriangle_t;

typedef struct traceNode_s
{
	int		type;
	vec4_t		plane;
	vec3_t		mins, maxs;
	int		children[2];
	int		numItems, maxItems;
	int		*items;
} traceNode_t;

int		noDrawContentFlags, noDrawSurfaceFlags, noDrawCompileFlags;
int		numTraceInfos = 0, maxTraceInfos = 0, firstTraceInfo = 0;
traceInfo_t	*traceInfos = NULL;
int		numTraceWindings = 0, maxTraceWindings = 0, deadWinding = -1;
traceWinding_t	*traceWindings = NULL;
int		numTraceTriangles = 0, maxTraceTriangles = 0, deadTriangle = -1;
traceTriangle_t	*traceTriangles = NULL;
int		headNodeNum = 0, skyboxNodeNum = 0, maxTraceDepth = 0, numTraceLeafNodes = 0;
int		numTraceNodes = 0, maxTraceNodes = 0;
traceNode_t	*traceNodes = NULL;
surfaceInfo_t	*surfaceInfos = NULL;
bool		patchshadows = false;
/* 
===============================================================================

	allocation and list management

===============================================================================
*/
/*
=================
AddTraceInfo

adds a trace info structure to the pool
=================
*/
static int AddTraceInfo( traceInfo_t *ti )
{
	int		num;
	void	*temp;

	// find an existing info
	for( num = firstTraceInfo; num < numTraceInfos; num++ )
	{
		if( traceInfos[num].si == ti->si && traceInfos[num].surfaceNum == ti->surfaceNum && traceInfos[num].castShadows == ti->castShadows )
			return num;
	}
	
	// enough space?
	if( numTraceInfos >= maxTraceInfos )
	{
		// allocate more room
		maxTraceInfos += GROW_TRACE_INFOS;
		temp = BSP_Malloc( maxTraceInfos * sizeof( *traceInfos ) );
		if( traceInfos != NULL )
		{
			Mem_Copy( temp, traceInfos, numTraceInfos * sizeof( *traceInfos ) );
			Mem_Free( traceInfos );
		}
		traceInfos = (traceInfo_t*)temp;
	}
	
	Mem_Copy( &traceInfos[num], ti, sizeof( *traceInfos ));
	if( num == numTraceInfos ) numTraceInfos++;
	
	return num;
}

/*
=================
AllocTraceNode

allocates a new trace node
=================
*/
static int AllocTraceNode( void )
{
	traceNode_t	*temp;

	// enough space?
	if( numTraceNodes >= maxTraceNodes )
	{
		// reallocate more room
		maxTraceNodes += GROW_TRACE_NODES;
		temp = BSP_Malloc( maxTraceNodes * sizeof( traceNode_t ));
		if( traceNodes != NULL )
		{
			Mem_Copy( temp, traceNodes, numTraceNodes * sizeof( traceNode_t ));
			Mem_Free( traceNodes );
		}
		traceNodes = temp;
	}
	
	// add the node
	memset( &traceNodes[numTraceNodes], 0, sizeof( traceNode_t ));
	traceNodes[numTraceNodes].type = TRACE_LEAF;
	ClearBounds( traceNodes[numTraceNodes].mins, traceNodes[numTraceNodes].maxs );
	numTraceNodes++;
	
	return (numTraceNodes - 1);
}

/*
=================
AddTraceWinding

adds a winding to the raytracing pool
=================
*/
static int AddTraceWinding( traceWinding_t *tw )
{
	int	num;
	void	*temp;
	
	
	// check for a dead winding
	if( deadWinding >= 0 && deadWinding < numTraceWindings )
	{
		num = deadWinding;
	}
	else
	{
		num = numTraceWindings;
		
		if( numTraceWindings >= maxTraceWindings )
		{
			maxTraceWindings += GROW_TRACE_WINDINGS;
			temp = BSP_Malloc( maxTraceWindings * sizeof( *traceWindings ));
			if( traceWindings != NULL )
			{
				Mem_Copy( temp, traceWindings, numTraceWindings * sizeof( *traceWindings ));
				Mem_Free( traceWindings );
			}
			traceWindings = (traceWinding_t*) temp;
		}
	}
	
	Mem_Copy( &traceWindings[num], tw, sizeof( *traceWindings ) );
	if( num == numTraceWindings ) numTraceWindings++;
	deadWinding = -1;
	
	return num;
}

/*
=================
AddTraceTriangle

adds a triangle to the raytracing pool
=================
*/
static int AddTraceTriangle( traceTriangle_t *tt )
{
	int	num;
	void	*temp;
	
	if( deadTriangle >= 0 && deadTriangle < numTraceTriangles )
	{
		num = deadTriangle;
	}
	else
	{
		num = numTraceTriangles;
		
		if( numTraceTriangles >= maxTraceTriangles )
		{
			maxTraceTriangles += GROW_TRACE_TRIANGLES;
			temp = BSP_Malloc( maxTraceTriangles * sizeof( *traceTriangles ) );
			if( traceTriangles != NULL )
			{
				Mem_Copy( temp, traceTriangles, numTraceTriangles * sizeof( *traceTriangles ));
				Mem_Free( traceTriangles );
			}
			traceTriangles = (traceTriangle_t*) temp;
		}
	}
	
	// find vectors for two edges sharing the first vert
	VectorSubtract( tt->v[1].point, tt->v[0].point, tt->edge1 );
	VectorSubtract( tt->v[2].point, tt->v[0].point, tt->edge2 );
	
	// add the triangle
	Mem_Copy( &traceTriangles[num], tt, sizeof( *traceTriangles ));
	if( num == numTraceTriangles ) numTraceTriangles++;
	deadTriangle = -1;
	
	return num;
}

/*
=================
AddItemToTraceNode

adds an item reference (winding or triangle) to a trace node
=================
*/
static int AddItemToTraceNode( traceNode_t *node, int num )
{
	void	*temp;
	
	if( num < 0 ) return -1;
	
	if( node->numItems >= node->maxItems )
	{
		if( node == traceNodes ) node->maxItems *= 2;
		else node->maxItems += GROW_NODE_ITEMS;

		if( node->maxItems <= 0 )
			node->maxItems = GROW_NODE_ITEMS;
		temp = BSP_Malloc( node->maxItems * sizeof( *node->items ));
		if( node->items != NULL )
		{
			Mem_Copy( temp, node->items, node->numItems * sizeof( *node->items ));
			Mem_Free( node->items );
		}
		node->items = (int*) temp;
	}
	
	node->items[ node->numItems ] = num;
	node->numItems++;
	
	return (node->numItems - 1);
}


/*
===============================================================================

trace node setup

===============================================================================
*/
/*
=================
SetupTraceNodes_r

recursively create the initial trace node structure from the bsp tree
=================
*/
static int SetupTraceNodes_r( int bspNodeNum )
{
	int		i, nodeNum, bspLeafNum;
	dplane_t		*plane;
	dnode_t 		*bspNode;
	
	bspNode = &dnodes[bspNodeNum];
	plane = &dplanes[bspNode->planenum];
	
	nodeNum = AllocTraceNode();
	
	traceNodes[nodeNum].type = PlaneTypeForNormal( plane->normal );
	VectorCopy( plane->normal, traceNodes[nodeNum].plane );
	traceNodes[nodeNum].plane[3] = plane->dist;
	
	for( i = 0; i < 2; i++ )
	{
		if( bspNode->children[i] < 0 )
		{
			bspLeafNum = -bspNode->children[i] - 1;
			
			traceNodes[nodeNum].children[i] = AllocTraceNode();
			if( dleafs[bspLeafNum].cluster == -1 )
				traceNodes[traceNodes[nodeNum].children[i]].type = TRACE_LEAF_SOLID;
		}
		else traceNodes[nodeNum].children[i] = SetupTraceNodes_r( bspNode->children[i] );
	}
	return nodeNum;
}


/*
=================
ClipTraceWinding

clips a trace winding against a plane into one or two parts
=================
*/
void ClipTraceWinding( traceWinding_t *tw, vec4_t plane, traceWinding_t *front, traceWinding_t *back )
{
	int		i, j, k;
	int		sides[MAX_TW_VERTS], counts[ 3 ] = { 0, 0, 0 };
	float		dists[MAX_TW_VERTS];
	float		frac;
	traceVert_t	*a, *b, mid;
	
	front->numVerts = 0;
	back->numVerts = 0;
	
	for( i = 0; i < tw->numVerts; i++ )
	{
		dists[i] = DotProduct( tw->v[i].point, plane ) - plane[3];
		if( dists[i] < -TW_ON_EPSILON )
			sides[i] = SIDE_BACK;
		else if( dists[i] > TW_ON_EPSILON )
			sides[i] = SIDE_FRONT;
		else sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	
	if( counts[SIDE_BACK] == 0 )
		Mem_Copy( front, tw, sizeof( *front ));
	else if( counts[SIDE_FRONT] == 0 )
		Mem_Copy( back, tw, sizeof( *back ));
	else
	{
		Mem_Copy( front, tw, sizeof( *front ));
		front->numVerts = 0;
		Mem_Copy( back, tw, sizeof( *back )); 
		back->numVerts = 0;
		
		for( i = 0; i < tw->numVerts; i++ )
		{
			j = (i + 1) % tw->numVerts;
			
			a = &tw->v[i];
			b = &tw->v[j];
			
			/* handle points on the splitting plane */
			switch( sides[i] )
			{
			case SIDE_FRONT:
				if( front->numVerts >= MAX_TW_VERTS )
					Sys_Error( "MAX_TW_VERTS (%d) limit exceeded\n", MAX_TW_VERTS );
				front->v[ front->numVerts++ ] = *a;
				break;
			case SIDE_BACK:
				if( back->numVerts >= MAX_TW_VERTS )
					Sys_Error( "MAX_TW_VERTS (%d) limit exceeded\n", MAX_TW_VERTS );
				back->v[ back->numVerts++ ] = *a;
				break;
			case SIDE_ON:
				if( front->numVerts >= MAX_TW_VERTS || back->numVerts >= MAX_TW_VERTS )
					Sys_Error( "MAX_TW_VERTS (%d) limit exceeded\n", MAX_TW_VERTS );
				front->v[ front->numVerts++ ] = *a;
				back->v[ back->numVerts++ ] = *a;
				continue;
			}
			
			// check next point to see if we need to split the edge
			if( sides[j] == SIDE_ON || sides[j] == sides[i] )
				continue;
			
			if( front->numVerts >= MAX_TW_VERTS || back->numVerts >= MAX_TW_VERTS )
				Sys_Error( "MAX_TW_VERTS (%d) limit exceeded\n", MAX_TW_VERTS );
			
			// generate a split point
			frac = dists[i] / (dists[i] - dists[j]);
			for( k = 0; k < 3; k++ )
			{
				// minimize fp precision errors
				if( plane[k] == 1.0f )
					mid.point[k] = plane[3];
				else if( plane[k] == -1.0f )
					mid.point[k] = -plane[3];
				else mid.point[k] = a->point[k] + frac * (b->point[k] - a->point[k]);
				
				// set texture coordinates
				if( k > 1 ) continue;
				mid.st[0] = a->st[0] + frac * (b->st[0] - a->st[0]);
				mid.st[1] = a->st[1] + frac * (b->st[1] - a->st[1]);
			}
			
			// copy midpoint to front and back polygons
			front->v[front->numVerts++] = mid;
			back->v[back->numVerts++] = mid;
		}
	}
}



/*
=================
FilterPointToTraceNodes_r

debugging tool
=================
*/
static int FilterPointToTraceNodes_r( vec3_t pt, int nodeNum )
{
	float		dot;
	traceNode_t	*node;
	
	
	if( nodeNum < 0 || nodeNum >= numTraceNodes )
		return -1;
	
	node = &traceNodes[ nodeNum ];
	
	if( node->type >= 0 )
	{
		dot = DotProduct( pt, node->plane ) - node->plane[ 3 ];
		if( dot > -0.001f ) FilterPointToTraceNodes_r( pt, node->children[0] );
		if( dot < 0.001f ) FilterPointToTraceNodes_r( pt, node->children[1] );
		return -1;
	}
	MsgDev( D_INFO, "%d ", nodeNum );
	
	return nodeNum;
}



/*
=================
FilterTraceWindingIntoNodes_r

filters a trace winding into the raytracing tree
=================
*/
static void FilterTraceWindingIntoNodes_r( traceWinding_t *tw, int nodeNum )
{
	int		num;
	vec4_t		plane1, plane2, reverse;
	traceNode_t	*node;
	traceWinding_t	front, back;
	
	// don't filter if passed a bogus node (solid, etc)
	if( nodeNum < 0 || nodeNum >= numTraceNodes )
		return;
	
	node = &traceNodes[nodeNum];
	
	// is this a decision node?
	if( node->type >= 0 )
	{	
		// create winding plane if necessary, filtering out bogus windings as well
		if( nodeNum == headNodeNum )
		{
			if( !PlaneFromPoints( tw->plane, tw->v[0].point, tw->v[1].point, tw->v[2].point ) )
				return;
		}
	
		if( node->children[0] == 0 || node->children[1] == 0 )
			Sys_Error( "Invalid tracenode: %d\n", nodeNum );
		
		Vector4Copy( node->plane, plane1 );
		Vector4Copy( tw->plane, plane2 );
		VectorSubtract( vec3_origin, plane2, reverse );
		reverse[3] = -plane2[3];
		
		if( DotProduct( plane1, plane2 ) > 0.999f && fabs( plane1[3] - plane2[3] ) < 0.001f )
		{
			FilterTraceWindingIntoNodes_r( tw, node->children[0] );
			return;
		}
		
		if( DotProduct( plane1, reverse ) > 0.999f && fabs( plane1[3] - reverse[3] ) < 0.001f )
		{
			FilterTraceWindingIntoNodes_r( tw, node->children[1] );
			return;
		}
		
		ClipTraceWinding( tw, plane1, &front, &back );
		
		if( front.numVerts >= 3 ) FilterTraceWindingIntoNodes_r( &front, node->children[0] );
		if( back.numVerts >= 3 ) FilterTraceWindingIntoNodes_r( &back, node->children[1] );
		return;
	}
	
	num = AddTraceWinding( tw );
	AddItemToTraceNode( node, num );
}



/*
=================
SubdivideTraceNode_r

recursively subdivides a tracing node until it meets certain size and complexity criteria
=================
*/
static void SubdivideTraceNode_r( int nodeNum, int depth )
{
	int		i, j, count, num, frontNum, backNum, type;
	vec3_t		size;
	float		dist;
	double		average[3];
	traceNode_t	*node, *frontNode, *backNode;
	traceWinding_t	*tw, front, back;
	
	if( nodeNum < 0 || nodeNum >= numTraceNodes )
		return;
	
	node = &traceNodes[ nodeNum ];
	
	// runaway recursion check
	if( depth >= MAX_TRACE_DEPTH )
	{
		numTraceLeafNodes++;
		return;
	}
	depth++;
	
	if( node->type >= 0 )
	{
		// subdivide children
		frontNum = node->children[0];
		backNum = node->children[1];
		SubdivideTraceNode_r( frontNum, depth );
		SubdivideTraceNode_r( backNum, depth );
		return;
	}
	
	ClearBounds( node->mins, node->maxs );
	VectorClear( average );
	count = 0;
	for( i = 0; i < node->numItems; i++ )
	{
		tw = &traceWindings[ node->items[i] ];
		
		for( j = 0; j < tw->numVerts; j++ )
		{
			AddPointToBounds( tw->v[j].point, node->mins, node->maxs );
			average[0] += tw->v[j].point[0];
			average[1] += tw->v[j].point[1];
			average[2] += tw->v[j].point[2];
			count++;
		}
	}
	
	if((count - (node->numItems * 2)) < MAX_NODE_TRIANGLES )
	{
		numTraceLeafNodes++;
		return;
	}
	
	// the largest dimension of the bounding box will be the split axis
	VectorSubtract( node->maxs, node->mins, size );
	if( size[0] >= size[1] && size[0] >= size[2] )
		type = PLANE_X;
	else if( size[1] >= size[0] && size[1] >= size[2] )
		type = PLANE_Y;
	else type = PLANE_Z;
	
	// don't split small nodes
	if( size[ type ] <= MIN_NODE_SIZE )
	{
		numTraceLeafNodes++;
		return;
	}
	
	if( depth > maxTraceDepth ) maxTraceDepth = depth;
	
	// snap the average
	dist = floor( average[type] / count );
	
	if( dist <= node->mins[type] || dist >= node->maxs[type] )
		dist = floor( 0.5f * (node->mins[type] + node->maxs[type]));
	
	frontNum = AllocTraceNode();
	backNum = AllocTraceNode();
	
	node = &traceNodes[ nodeNum ];
	frontNode = &traceNodes[ frontNum ];
	backNode = &traceNodes[ backNum ];
	
	node->type = type;
	node->plane[type] = 1.0f;
	node->plane[3] = dist;
	node->children[0] = frontNum;
	node->children[1] = backNum;
	
	frontNode->maxItems = (node->maxItems >> 1);
	frontNode->items = BSP_Malloc( frontNode->maxItems * sizeof( *frontNode->items ));
	backNode->maxItems = (node->maxItems >> 1);
	backNode->items = BSP_Malloc( backNode->maxItems * sizeof( *backNode->items ));
	
	for( i = 0; i < node->numItems; i++ )
	{
		tw = &traceWindings[ node->items[i] ];
		
		ClipTraceWinding( tw, node->plane, &front, &back );
		if( front.numVerts >= 3 || back.numVerts >= 3 )
			deadWinding = node->items[i];
		
		if( front.numVerts >= 3 )
		{
			num = AddTraceWinding( &front );
			AddItemToTraceNode( frontNode, num );
		}
		
		if( back.numVerts >= 3 )
		{
			num = AddTraceWinding( &back );
			AddItemToTraceNode( backNode, num );
		}
	}
	
	node->numItems = 0;
	node->maxItems = 0;
	Mem_Free( node->items );
	node->items = NULL;
	
	if( frontNode->numItems <= 0 )
	{
		frontNode->maxItems = 0;
		Mem_Free( frontNode->items );
		frontNode->items = NULL;
	}
	
	if( backNode->numItems <= 0 )
	{
		backNode->maxItems = 0;
		Mem_Free( backNode->items );
		backNode->items = NULL;
	}
	
	SubdivideTraceNode_r( frontNum, depth );
	SubdivideTraceNode_r( backNum, depth );
}



/*
=================
TriangulateTraceNode_r

optimizes the tracing data by changing trace windings into triangles
=================
*/
static int TriangulateTraceNode_r( int nodeNum )
{
	int		i, j, num, frontNum, backNum, numWindings, *windings;
	traceNode_t	*node;
	traceWinding_t	*tw;
	traceTriangle_t	tt;
	
	if( nodeNum < 0 || nodeNum >= numTraceNodes )
		return 0;
	
	node = &traceNodes[nodeNum];
	
	if( node->type >= 0 )
	{
		frontNum = node->children[0];
		backNum = node->children[1];
		node->numItems = TriangulateTraceNode_r( frontNum );
		node->numItems += TriangulateTraceNode_r( backNum );
		return node->numItems;
	}
	
	if( node->numItems == 0 )
	{
		node->maxItems = 0;
		if( node->items != NULL )
			Mem_Free( node->items );
		return node->numItems;
	}
	
	numWindings = node->numItems;
	windings = node->items;
	node->numItems = 0;
	node->maxItems = numWindings * 2;
	node->items = BSP_Malloc( node->maxItems * sizeof( tt ));
	
	for( i = 0; i < numWindings; i++ )
	{
		tw = &traceWindings[ windings[i] ];
		
		tt.infoNum = tw->infoNum;
		tt.v[0] = tw->v[0];
		
		for( j = 1; j + 1 < tw->numVerts; j++ )
		{
			tt.v[1] = tw->v[j];
			tt.v[2] = tw->v[ j + 1 ];
			
			// find vectors for two edges sharing the first vert
			VectorSubtract( tt.v[1].point, tt.v[0].point, tt.edge1 );
			VectorSubtract( tt.v[2].point, tt.v[0].point, tt.edge2 );
			
			num = AddTraceTriangle( &tt );
			AddItemToTraceNode( node, num );
		}
	}
	
	if( windings != NULL ) Mem_Free( windings );
	
	return node->numItems;
}



/*
===============================================================================

shadow casting item setup (triangles, patches, entities)

===============================================================================
*/
/*
=================
PopulateWithBSPModel

filters a bsp model's surfaces into the raytracing tree
=================
*/
static void PopulateWithBSPModel( dmodel_t *model, matrix4x4 transform )
{
	dsurface_t		*ds;
	surfaceInfo_t		*info;
	dvertex_t			*verts;
	int			*indexes;
	bsp_mesh_t		srcMesh, *mesh, *subdivided;
	int			i, j, x, y, pw[5], r, nodeNum;
	traceInfo_t		ti;
	traceWinding_t		tw;
	
	if( model == NULL || transform == NULL )
		return;
	
	for( i = 0; i < model->numfaces; i++ )
	{
		ds = &dsurfaces[ model->firstface + i ];
		info = &surfaceInfos[ model->firstface + i ];
		if( info->si == NULL ) continue;
		
		if( !info->castShadows ) continue;
		
		if( ds->surfaceType == MST_PATCH && patchshadows == false )
			continue;
		
		// some surfaces in the bsp might have been tagged as nodraw, with a bogus shader
		if(( dshaders[ds->shadernum].contents & noDrawContentFlags) || (dshaders[ds->shadernum].surfaceFlags & noDrawSurfaceFlags))
			continue;
		
		// translucent surfaces that are neither alphashadow or lightfilter don't cast shadows
		if((info->si->surfaceFlags & SURF_NODRAW))
			continue;
		if((info->si->contents & CONTENTS_TRANSLUCENT) && !(info->si->surfaceFlags & SURF_ALPHASHADOW) && !(info->si->surfaceFlags & SURF_LIGHTFILTER))
			continue;
		
		ti.si = info->si;
		ti.castShadows = info->castShadows;
		ti.surfaceNum = model->firstbrush + i;
		
		// choose which node (normal or skybox)
		if( info->parentSurfaceNum >= 0 )
		{
			nodeNum = skyboxNodeNum;
			
			// sky surfaces in portal skies are ignored
			if( info->si->surfaceFlags & SURF_SKY)
				continue;
		}
		else nodeNum = headNodeNum;
		
		memset( &tw, 0, sizeof( tw ));
		tw.infoNum = AddTraceInfo( &ti );
		tw.numVerts = 3;
		
		switch( ds->surfaceType )
		{
		case MST_PATCH:
			srcMesh.width = ds->patch.width;
			srcMesh.height = ds->patch.height;
			srcMesh.verts = &dvertexes[ds->firstvertex];
			subdivided = SubdivideMesh2( srcMesh, info->patchIterations );
				
			// fit it to the curve and remove colinear verts on rows/columns
			PutMeshOnCurve( *subdivided );
			mesh = RemoveLinearMeshColumnsRows( subdivided );
			FreeMesh( subdivided );
				
			verts = mesh->verts;
				
			// subdivide each quad to place the models
			for( y = 0; y < (mesh->height - 1); y++ )
			{
				for( x = 0; x < (mesh->width - 1); x++ )
				{
					pw[0] = x + (y * mesh->width);
					pw[1] = x + ((y + 1) * mesh->width);
					pw[2] = x + 1 + ((y + 1) * mesh->width);
					pw[3] = x + 1 + (y * mesh->width);
					pw[4] = x + (y * mesh->width);	/* same as pw[0] */
						
					r = (x + y) & 1;
						
					VectorCopy( verts[pw[r+0]].point, tw.v[0].point );
					Vector2Copy( verts[pw[r+0]].st, tw.v[0].st );
					VectorCopy( verts[pw[r+1 ]].point, tw.v[1].point );
					Vector2Copy( verts[pw[r+1]].st, tw.v[1].st );
					VectorCopy( verts[pw[r+2]].point, tw.v[2].point );
					Vector2Copy( verts[pw[r+2]].st, tw.v[2].st );
					Matrix4x4_TransformPoint( transform, tw.v[0].point );
					Matrix4x4_TransformPoint( transform, tw.v[1].point );
					Matrix4x4_TransformPoint( transform, tw.v[2].point );
					FilterTraceWindingIntoNodes_r( &tw, nodeNum );
						
					VectorCopy( verts[pw[r+0]].point, tw.v[0].point );
					Vector2Copy( verts[pw[r+0]].st, tw.v[0].st );
					VectorCopy( verts[pw[r+2]].point, tw.v[1].point );
					Vector2Copy( verts[pw[r+2]].st, tw.v[1].st );
					VectorCopy( verts[pw[r+3]].point, tw.v[2].point );
					Vector2Copy( verts[pw[r+3]].st, tw.v[2].st );
					Matrix4x4_TransformPoint( transform, tw.v[0].point );
					Matrix4x4_TransformPoint( transform, tw.v[1].point );
					Matrix4x4_TransformPoint( transform, tw.v[2].point );
					FilterTraceWindingIntoNodes_r( &tw, nodeNum );
				}
			}
			FreeMesh( mesh );
			break;
		case MST_TRIANGLE_SOUP:
		case MST_PLANAR:
			verts = &dvertexes[ds->firstvertex];
			indexes = &dindexes[ds->firstindex];
				
			for( j = 0; j < ds->numindices; j += 3 )
			{
				VectorCopy( verts[ indexes[j] ].point, tw.v[0].point );
				Vector2Copy( verts[ indexes[j] ].st, tw.v[0].st );
				VectorCopy( verts[ indexes[ j + 1 ] ].point, tw.v[1].point );
				Vector2Copy( verts[ indexes[ j + 1 ] ].st, tw.v[1].st );
				VectorCopy( verts[ indexes[ j + 2 ] ].point, tw.v[2].point );
				Vector2Copy( verts[ indexes[ j + 2 ] ].st, tw.v[2].st );
				Matrix4x4_TransformPoint( transform, tw.v[0].point );
				Matrix4x4_TransformPoint( transform, tw.v[1].point );
				Matrix4x4_TransformPoint( transform, tw.v[2].point );
				FilterTraceWindingIntoNodes_r( &tw, nodeNum );
			}
			break;
			default: break;
		}
	}
}

/*
=================
PopulateWithStudioModel

filters a picomodel's surfaces into the raytracing tree
=================
*/
static void PopulateWithStudioModel( int castShadows, cmodel_t *model, matrix4x4 transform )
{
#if 0	// FIXME: implement
	int			i, j, k, numSurfaces, numIndexes;
	picoSurface_t		*surface;
	picoShader_t		*shader;
	picoVec_t			*xyz, *st;
	picoIndex_t		*indexes;
	traceInfo_t		ti;
	traceWinding_t		tw;
	
	
	/* dummy check */
	if( model == NULL || transform == NULL )
		return;
	
	/* get info */
	numSurfaces = PicoGetModelNumSurfaces( model );
	
	/* walk the list of surfaces in this model and fill out the info structs */
	for( i = 0; i < numSurfaces; i++ )
	{
		/* get surface */
		surface = PicoGetModelSurface( model, i );
		if( surface == NULL )
			continue;
		
		/* only handle triangle surfaces initially (fixme: support patches) */
		if( PicoGetSurfaceType( surface ) != PICO_TRIANGLES )
			continue;
		
		/* get shader (fixme: support shader remapping) */
		shader = PicoGetSurfaceShader( surface );
		if( shader == NULL )
			continue;
		ti.si = ShaderInfoForShader( PicoGetShaderName( shader ) );
		if( ti.si == NULL )
			continue;
		
		/* translucent surfaces that are neither alphashadow or lightfilter don't cast shadows */
		if( (ti.si->compileFlags & C_NODRAW) )
			continue;
		if( (ti.si->compileFlags & C_TRANSLUCENT) &&
			!(ti.si->compileFlags & C_ALPHASHADOW) && 
			!(ti.si->compileFlags & C_LIGHTFILTER) )
			continue;
		
		/* setup trace info */
		ti.castShadows = castShadows;
		ti.surfaceNum = -1;
		
		/* setup trace winding */
		memset( &tw, 0, sizeof( tw ) );
		tw.infoNum = AddTraceInfo( &ti );
		tw.numVerts = 3;
		
		/* get info */
		numIndexes = PicoGetSurfaceNumIndexes( surface );
		indexes = PicoGetSurfaceIndexes( surface, 0 );
		
		/* walk the triangle list */
		for( j = 0; j < numIndexes; j += 3, indexes += 3 )
		{
			for( k = 0; k < 3; k++ )
			{
				xyz = PicoGetSurfaceXYZ( surface, indexes[ k ] );
				st = PicoGetSurfaceST( surface, 0, indexes[ k ] );
				VectorCopy( xyz, tw.v[ k ].xyz );
				Vector2Copy( st, tw.v[ k ].st );
				m4x4_transform_point( transform, tw.v[ k ].xyz );
			}
			FilterTraceWindingIntoNodes_r( &tw, headNodeNum );
		}
	}
#endif
}

/*
=================
PopulateTraceNodes

fills the raytracing tree with world and entity occluders
=================
*/
static void PopulateTraceNodes( void )
{
	float		temp;
	bsp_entity_t	*e;
	const char	*value;
	vec3_t		origin, scale, angles;
	cmodel_t		*model;
	int		i, m, castShadows;
	matrix4x4		transform;
	
	Matrix4x4_LoadIdentity( transform );
	PopulateWithBSPModel( &dmodels[0], transform );
	
	/* walk each entity list */
	for( i = 1; i < num_entities; i++ )
	{
		e = &entities[i];
		
		castShadows = ENTITY_CAST_SHADOWS;
		GetEntityShadowFlags( e, NULL, &castShadows, NULL );
		
		if( !castShadows ) continue;
		
		GetVectorForKey( e, "origin", origin );
		
		scale[0] = scale[1] = scale[2] = 1.0f;
		temp = FloatForKey( e, "scale" );
		if( temp != 0.0f ) scale[0] = scale[1] = scale[2] = temp;

		angles[0] = angles[1] = angles[2] = 0.0f;
		angles[2] = FloatForKey( e, "angle" );
		value = ValueForKey( e, "angles" );
		if( value[0] != '\0' ) sscanf( value, "%f %f %f", &angles[1], &angles[2], &angles[0] );
		
		Matrix4x4_LoadIdentity( transform );
		Matrix4x4_Pivot( transform, origin, angles, scale, vec3_origin );
		
		value = ValueForKey( e, "model" );
		
		switch( value[0] )
		{
		case '\0': break;
		case '*':
			m = com.atoi( &value[1] );
			if( m <= 0 || m >= nummodels )
				continue;
			PopulateWithBSPModel( &dmodels[m], transform );
			break;
		default:
			if( !pe ) continue;
			model = pe->RegisterModel( value );
			if( model == NULL ) continue;
			PopulateWithStudioModel( castShadows, model, transform );
			continue;
		}
	}
}



/*
===============================================================================

trace initialization

===============================================================================
*/
/*
=================
SetupTraceNodes

creates a balanced bsp with axis-aligned splits for efficient raytracing
=================
*/
void SetupTraceNodes( void )
{
	MsgDev( D_NOTE, "--- SetupTraceNodes ---\n" );
	
	noDrawContentFlags = noDrawSurfaceFlags = 0;
	ApplySurfaceParm( "nodraw", &noDrawContentFlags, &noDrawSurfaceFlags );
	
	// create the baseline raytracing tree from the bsp tree
	headNodeNum = SetupTraceNodes_r( 0 );
	
	// create outside node for skybox surfaces
	skyboxNodeNum = AllocTraceNode();
	
	// populate the tree with triangles from the world and shadow casting entities
	PopulateTraceNodes();
	
	// create the raytracing bsp
	SubdivideTraceNode_r( headNodeNum, 0 );
	SubdivideTraceNode_r( skyboxNodeNum, 0 );
	
	// create triangles from the trace windings
	TriangulateTraceNode_r( headNodeNum );
	TriangulateTraceNode_r( skyboxNodeNum );
	
	MsgDev( D_INFO, "%6i trace windings (%.2fMB)\n", numTraceWindings, (float) (numTraceWindings * sizeof( *traceWindings )) / (1024.0f * 1024.0f));
	MsgDev( D_INFO, "%6i trace triangles (%.2fMB)\n", numTraceTriangles, (float) (numTraceTriangles * sizeof( *traceTriangles )) / (1024.0f * 1024.0f));
	MsgDev( D_INFO, "%6i trace nodes (%.2fMB)\n", numTraceNodes, (float) (numTraceNodes * sizeof( *traceNodes )) / (1024.0f * 1024.0f));
	MsgDev( D_INFO, "%6i leaf nodes (%.2fMB)\n", numTraceLeafNodes, (float) (numTraceLeafNodes * sizeof( *traceNodes )) / (1024.0f * 1024.0f));
	MsgDev( D_INFO, "%6i average windings per leaf node\n", numTraceWindings / (numTraceLeafNodes + 1));
	MsgDev( D_INFO, "%6i max trace depth\n", maxTraceDepth );
	
	Mem_Free( traceWindings );
	numTraceWindings = 0;
	maxTraceWindings = 0;
	deadWinding = -1;
}



/*
===============================================================================

raytracer

===============================================================================
*/

/*
=================
TraceTriangle

based on code written by william 'spog' joseph
based on code originally written by tomas moller and ben trumbore,
journal of graphics tools, 2(1):21-28, 1997
=================
*/

#define BARY_EPSILON		0.01f
#define ASLF_EPSILON		0.0001f	// so to not get double shadows
#define COPLANAR_EPSILON		0.25f
#define NEAR_SHADOW_EPSILON		1.5f
#define SELF_SHADOW_EPSILON		0.5f

bool TraceTriangle( traceInfo_t *ti, traceTriangle_t *tt, lighttrace_t *trace )
{
	float		tvec[3], pvec[3], qvec[3];
	float		det, invDet, depth;
	float		u, v, w, s, t;
	int		i, is, it;
	byte		*pixel;
	float		shadow;
	bsp_shader_t	*si;
	
	
	// don't double-trace against sky
	si = ti->si;
	if( trace->surfaceFlags & si->surfaceFlags & SURF_SKY )
		return false;
	
	// receive shadows from worldspawn group only
	if( trace->recvShadows == 1 )
	{
		if( ti->castShadows != 1 )
			return false;
	}
	
	// receive shadows from same group and worldspawn group
	else if( trace->recvShadows > 1 )
	{
		if( ti->castShadows != 1 && abs( ti->castShadows ) != abs( trace->recvShadows ))
			return false;
	}
	
	// receive shadows from the same group only (< 0)
	else
	{
		if( abs( ti->castShadows ) != abs( trace->recvShadows ))
			return false;
	}
	
	// begin calculating determinant - also used to calculate u parameter
	CrossProduct( trace->direction, tt->edge2, pvec );
	
	// if determinant is near zero, trace lies in plane of triangle
	det = DotProduct( tt->edge1, pvec );
	
	// the non-culling branch
	if( fabs( det ) < COPLANAR_EPSILON )
		return false;
	invDet = 1.0f / det;
	
	/* calculate distance from first vertex to ray origin */
	VectorSubtract( trace->origin, tt->v[0].point, tvec );
	
	// calculate u parameter and test bounds
	u = DotProduct( tvec, pvec ) * invDet;
	if( u < -BARY_EPSILON || u > (1.0f + BARY_EPSILON) )
		return false;
	
	// prepare to test v parameter
	CrossProduct( tvec, tt->edge1, qvec );
	
	// calculate v parameter and test bounds
	v = DotProduct( trace->direction, qvec ) * invDet;
	if( v < -BARY_EPSILON || (u + v) > (1.0f + BARY_EPSILON) )
		return false;
	
	// calculate t (depth)
	depth = DotProduct( tt->edge2, qvec ) * invDet;
	if( depth <= trace->inhibitRadius || depth >= trace->distance )
		return false;
	
	// if hitpoint is really close to trace origin (sample point), then check for self-shadowing
	if( depth <= SELF_SHADOW_EPSILON )
	{
		// don't self-shadow
		for( i = 0; i < trace->numSurfaces; i++ )
		{
			if( ti->surfaceNum == trace->surfaces[i] )
				return false;
		}
	}
	
	trace->surfaceFlags |= si->surfaceFlags;
	
	// don't trace against sky
	if( si->surfaceFlags & SURF_SKY )
		return false;
	
	// most surfaces are completely opaque
	if( !(si->surfaceFlags & (SURF_ALPHASHADOW|SURF_LIGHTFILTER)) || si->lightImage == NULL || si->lightImage->buffer == NULL )
	{
		VectorMA( trace->origin, depth, trace->direction, trace->hit );
		VectorClear( trace->color );
		trace->opaque = true;
		return true;
	}
	
	// try to avoid double shadows near triangle seams
	if( u < -ASLF_EPSILON || u > (1.0f + ASLF_EPSILON) || v < -ASLF_EPSILON || (u + v) > (1.0f + ASLF_EPSILON))
		return false;
	
	w = 1.0f - (u + v);
	
	// calculate st from uvw (barycentric) coordinates
	s = w * tt->v[0].st[0] + u * tt->v[1].st[0] + v * tt->v[2].st[0];
	t = w * tt->v[0].st[1] + u * tt->v[1].st[1] + v * tt->v[2].st[1];
	s = s - floor( s );
	t = t - floor( t );
	is = s * si->lightImage->width;
	it = t * si->lightImage->height;
	
	// get pixel
	pixel = si->lightImage->buffer + 4 * (it * si->lightImage->width + is);
	
	// ydnar: color filter
	if( si->surfaceFlags & SURF_LIGHTFILTER )
	{
		trace->color[0] *= ((1.0f / 255.0f) * pixel[0]);
		trace->color[1] *= ((1.0f / 255.0f) * pixel[1]);
		trace->color[2] *= ((1.0f / 255.0f) * pixel[2]);
	}
	
	// alpha filter
	if( si->surfaceFlags & SURF_ALPHASHADOW )
	{
		// filter by inverse texture alpha
		shadow = (1.0f / 255.0f) * (255 - pixel[3]);
		trace->color[0] *= shadow;
		trace->color[1] *= shadow;
		trace->color[2] *= shadow;
	}
	
	// check filter for opaque
	if( trace->color[0] <= 0.001f && trace->color[1] <= 0.001f && trace->color[2] <= 0.001f )
	{
		VectorMA( trace->origin, depth, trace->direction, trace->hit );
		trace->opaque = true;
		return true;
	}
	
	// continue tracing
	return false;
}



/*
=================
TraceWinding

temporary hack
=================
*/
bool TraceWinding( traceWinding_t *tw, lighttrace_t *trace )
{
	int		i;
	traceTriangle_t	tt;
	
	tt.infoNum = tw->infoNum;
	tt.v[0] = tw->v[0];
	
	for( i = 1; i + 1 < tw->numVerts; i++ )
	{
		tt.v[1] = tw->v[i+0];
		tt.v[2] = tw->v[i+1];
		
		// find vectors for two edges sharing the first vert
		VectorSubtract( tt.v[1].point, tt.v[0].point, tt.edge1 );
		VectorSubtract( tt.v[2].point, tt.v[0].point, tt.edge2 );
		
		if( TraceTriangle( &traceInfos[tt.infoNum], &tt, trace ))
			return true;
	}
	return false;
}

/*
=================
TraceLine_r

returns true if something is hit and tracing can stop
=================
*/
static bool TraceLine_r( int nodeNum, vec3_t origin, vec3_t end, lighttrace_t *trace )
{
	traceNode_t	*node;
	int		side;
	float		front, back, frac;
	vec3_t		mid;
	bool		r;
	
	// bogus node number means solid, end tracing unless testing all
	if( nodeNum < 0 )
	{
		VectorCopy( origin, trace->hit );
		trace->passSolid = true;
		return true;
	}
	
	node = &traceNodes[nodeNum];
	
	if( node->type == TRACE_LEAF_SOLID )
	{
		VectorCopy( origin, trace->hit );
		trace->passSolid = true;
		return true;
	}
	
	if( node->type < 0 )
	{
		// note leaf and return	
		if( node->numItems > 0 && trace->numTestNodes < MAX_TRACE_TEST_NODES )
			trace->testNodes[ trace->numTestNodes++ ] = nodeNum;
		return false;
	}
	
	// don't test branches of the bsp with nothing in them when testall is enabled
	if( trace->testAll && node->numItems == 0 )
		return false;
	
	// classify beginning and end points
	switch( node->type )
	{
	case PLANE_X:
		front = origin[0] - node->plane[3];
		back = end[0] - node->plane[3];
		break;
	case PLANE_Y:
		front = origin[1] - node->plane[3];
		back = end[1] - node->plane[3];
		break;
	case PLANE_Z:
		front = origin[2] - node->plane[3];
		back = end[2] - node->plane[3];
		break;
	default:
		front = DotProduct( origin, node->plane ) - node->plane[3];
		back = DotProduct( end, node->plane ) - node->plane[3];
		break;
	}
	
	if( front >= -TRACE_ON_EPSILON && back >= -TRACE_ON_EPSILON )
		return TraceLine_r( node->children[0], origin, end, trace );
	
	if( front < TRACE_ON_EPSILON && back < TRACE_ON_EPSILON )
		return TraceLine_r( node->children[1], origin, end, trace );
	
	side = front < 0;
	
	frac = front / (front - back);
	mid[0] = origin[0] + (end[0] - origin[0]) * frac;
	mid[1] = origin[1] + (end[1] - origin[1]) * frac;
	mid[2] = origin[2] + (end[2] - origin[2]) * frac;
	
	// FIXME: check inhibit radius, then solid nodes and ignore
	
	// trace first side
	r = TraceLine_r( node->children[side], origin, mid, trace );
	if( r ) return r;
	
	// trace other side
	return TraceLine_r( node->children[!side], mid, end, trace );
}



/*
=================
TraceLine

rewrote this function a bit :)
=================
*/
void TraceLine( lighttrace_t *trace )
{
	int		i, j;
	traceNode_t	*node;
	traceTriangle_t	*tt;
	traceInfo_t	*ti;
	
	
	// setup output (note: this code assumes the input data is completely filled out)
	trace->passSolid = false;
	trace->opaque = false;
	trace->surfaceFlags = 0;
	trace->numTestNodes = 0;
	
	if( !trace->recvShadows || !trace->testOcclusion || trace->distance <= 0.00001f )
		return;
	
	TraceLine_r( headNodeNum, trace->origin, trace->end, trace );
	if( trace->passSolid && !trace->testAll )
	{
		trace->opaque = true;
		return;
	}
	
	if( noSurfaces ) return;
	
	// testall means trace through sky	
	if( trace->testAll && trace->numTestNodes < MAX_TRACE_TEST_NODES && trace->surfaceFlags & SURF_SKY &&
	(trace->numSurfaces == 0 || surfaceInfos[ trace->surfaces[0] ].childSurfaceNum < 0))
	{
		TraceLine_r( skyboxNodeNum, trace->origin, trace->end, trace );
	}
	
	for( i = 0; i < trace->numTestNodes; i++ )
	{
		node = &traceNodes[ trace->testNodes[i] ];
		
		for( j = 0; j < node->numItems; j++ )
		{
			tt = &traceTriangles[ node->items[j] ];
			ti = &traceInfos[ tt->infoNum ];
			if( TraceTriangle( ti, tt, trace ))
				return;
		}
	}
}



/*
=================
SetupTrace

sets up certain trace values
=================
*/
float SetupTrace( lighttrace_t *trace )
{
	VectorSubtract( trace->end, trace->origin, trace->displacement );
	VectorCopy( trace->displacement, trace->direction );
	trace->distance = VectorNormalizeLength( trace->direction );
	VectorCopy( trace->origin, trace->hit );
	return trace->distance;
}
