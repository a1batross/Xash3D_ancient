//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    r_surface.c - surface-related refresh code
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "matrixlib.h"
#include "const.h"

#define BACKFACE_EPSILON	0.01

rmodel_t		*r_worldModel;
ref_entity_t	*r_worldEntity;
vec3_t		r_worldMins, r_worldMaxs;
int		r_frameCount;
int		r_visFrameCount;
int		r_areabitsChanged;
int		r_viewCluster;
int		numRadarEnts = 0;
radar_ent_t	RadarEnts[MAX_RADAR_ENTS];

/*
=================
R_DrawSurface
=================
*/
void R_DrawSurface( void )
{
	surface_t		*surf = m_pRenderMesh->mesh;
	surfPoly_t	*p;
	surfPolyVert_t	*v;
	int		i;

	for( p = surf->poly; p; p = p->next )
	{
		RB_CheckMeshOverflow( p->numIndices, p->numVertices );

		for( i = 0; i < p->numIndices; i += 3 )
		{
			indexArray[numIndex++] = numVertex + p->indices[i+0];
			indexArray[numIndex++] = numVertex + p->indices[i+1];
			indexArray[numIndex++] = numVertex + p->indices[i+2];
		}

		for( i = 0, v = p->vertices; i < p->numVertices; i++, v++ )
		{
			vertexArray[numVertex][0] = v->xyz[0];
			vertexArray[numVertex][1] = v->xyz[1];
			vertexArray[numVertex][2] = v->xyz[2];
			tangentArray[numVertex][0] = surf->tangent[0];
			tangentArray[numVertex][1] = surf->tangent[1];
			tangentArray[numVertex][2] = surf->tangent[2];
			binormalArray[numVertex][0] = surf->binormal[0];
			binormalArray[numVertex][1] = surf->binormal[1];
			binormalArray[numVertex][2] = surf->binormal[2];
			normalArray[numVertex][0] = surf->normal[0];
			normalArray[numVertex][1] = surf->normal[1];
			normalArray[numVertex][2] = surf->normal[2];
			inTexCoordArray[numVertex][0] = v->st[0];
			inTexCoordArray[numVertex][1] = v->st[1];
			inTexCoordArray[numVertex][2] = v->lightmap[0];
			inTexCoordArray[numVertex][3] = v->lightmap[1];
			Vector4Copy(v->color, inColorArray[numVertex]);
			numVertex++;
		}
	}
}

/*
=================
R_CullSurface
=================
*/
static bool R_CullSurface( surface_t *surf, const vec3_t origin, int clipFlags )
{
	cplane_t	*plane;
	float	dist;

	if( r_nocull->integer ) return false;

	// find which side of the node we are on
	plane = surf->plane;
	if( plane->type < 3 ) dist = origin[plane->type] - plane->dist;
	else dist = DotProduct( origin, plane->normal ) - plane->dist;

	if(!(surf->flags & SURF_PLANEBACK))
	{
		if( dist <= BACKFACE_EPSILON )
			return true; // wrong side
	}
	else
	{
		if( dist >= -BACKFACE_EPSILON )
			return true; // wrong side
	}

	// cull
	if( clipFlags )
	{
		if( R_CullBox( surf->mins, surf->maxs, clipFlags ))
			return true;
	}
	return false;
}

/*
=================
R_AddSurfaceToList
=================
*/
static void R_AddSurfaceToList( surface_t *surf, ref_entity_t *entity )
{
	mipTex_t		*tex = surf->texInfo;
	ref_shader_t	*shader = tex->shader;
	int		map, lmNum;

	if( tex->flags & SURF_NODRAW )
		return;

	// select lightmap
	lmNum = surf->lmNum;

	// check for lightmap modification
	if( r_dynamiclights->integer && (shader->flags & SHADER_HASLIGHTMAP))
	{
		if( surf->dlightFrame == r_frameCount ) lmNum = 255;
		else
		{
			for( map = 0; map < surf->numStyles; map++ )
			{
				if( surf->cachedLight[map] != r_lightStyles[surf->styles[map]].white )
				{
					lmNum = 255;
					break;
				}
			}
		}
	}

	// add it
	R_AddMeshToList( MESH_SURFACE, surf, shader, entity, lmNum );

	// Also add caustics
	if( r_caustics->integer )
	{
		if( surf->flags & SURF_WATERCAUSTICS )
			R_AddMeshToList( MESH_SURFACE, surf, r_waterCausticsShader, entity, 0 );
		if( surf->flags & SURF_SLIMECAUSTICS )
			R_AddMeshToList( MESH_SURFACE, surf, r_slimeCausticsShader, entity, 0 );
		if( surf->flags & SURF_LAVACAUSTICS )
			R_AddMeshToList( MESH_SURFACE, surf, r_lavaCausticsShader, entity, 0 );
	}
}


/*
=======================================================================

 BRUSH MODELS

=======================================================================
*/
/*
=================
R_AddBrushModelToList
=================
*/
void R_AddBrushModelToList( ref_entity_t *entity )
{
	rmodel_t		*model = entity->model;
	surface_t		*surf;
	dlight_t		*dl;
	vec3_t		origin, tmp;
	vec3_t		mins, maxs;
	int		i, l;

	if( !model->numModelSurfaces )
		return;

	// cull
	if( !AxisCompare( entity->axis, axisDefault ))
	{
		for( i = 0; i < 3; i++ )
		{
			mins[i] = entity->origin[i] - model->radius;
			maxs[i] = entity->origin[i] + model->radius;
		}

		if( R_CullSphere( entity->origin, model->radius, MAX_CLIPFLAGS ))
			return;

		VectorSubtract( r_origin, entity->origin, tmp );
		VectorRotate( tmp, entity->axis, origin );
	}
	else
	{
		VectorAdd( entity->origin, model->mins, mins );
		VectorAdd( entity->origin, model->maxs, maxs );

		if( R_CullBox( mins, maxs, MAX_CLIPFLAGS ))
			return;

		VectorSubtract( r_refdef.vieworg, entity->origin, origin );
	}

	// Calculate dynamic lighting
	if( r_dynamiclights->integer )
	{
		for( l = 0, dl = r_dlights; l < r_numDLights; l++, dl++ )
		{
			if( !BoundsAndSphereIntersect( mins, maxs, dl->origin, dl->intensity ))
				continue;

			surf = model->surfaces + model->firstModelSurface;
			for( i = 0; i < model->numModelSurfaces; i++, surf++ )
			{
				if( surf->dlightFrame != r_frameCount )
				{
					surf->dlightFrame = r_frameCount;
					surf->dlightBits = (1<<l);
				}
				else surf->dlightBits |= (1<<l);
			}
		}
	}

	// add all the surfaces
	surf = model->surfaces + model->firstModelSurface;
	for( i = 0; i < model->numModelSurfaces; i++, surf++ )
	{
		// cull
		if( R_CullSurface( surf, origin, 0 ))
			continue;

		// add the surface
		R_AddSurfaceToList( surf, entity );
	}
}


/*
=======================================================================

WORLD MODEL

=======================================================================
*/


/*
=================
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current cluster
=================
*/
static void R_MarkLeaves( void )
{
	byte		*vis;
	int		i, cluster;
	node_t		*leaf, *parent;

	// lockpvs lets designers walk around to determine the
	// extent of the current pvs
	if( r_lockpvs->integer ) return;

	// Current view cluster
	leaf = R_PointInLeaf( r_refdef.vieworg );
	cluster = leaf->cluster;

	// if the cluster is the same and the area visibility matrix
	// hasn't changed, we don't need to mark everything again

	// if r_showcluster was just turned on, remark everything 
	if( r_viewCluster == cluster && !r_areabitsChanged && !r_showcluster->modified )
		return;

	if( r_showcluster->modified || r_showcluster->integer )
	{
		r_showcluster->modified = false;
		if( r_showcluster->integer )
			Msg( "cluster:%i  area:%i\n", cluster, leaf->area );
	}

	r_visFrameCount++;
	r_viewCluster = cluster;
	
	if( r_novis->integer || r_viewCluster == -1 )
	{
		for( i = 0; i < r_worldModel->numNodes; i++ )
		{
			if( r_worldModel->nodes[i].contents != CONTENTS_SOLID )
				r_worldModel->nodes[i].visFrame = r_visFrameCount;
		}
		return;
	}

	vis = R_ClusterPVS( r_viewCluster );

	for( i = 0, leaf = r_worldModel->nodes; i < r_worldModel->numNodes; i++, leaf++ )
	{
		cluster = leaf->cluster;
		if( cluster < 0 || cluster >= r_worldModel->numClusters )
			continue;

		// check general pvs
		if(!(vis[cluster>>3] & (1<<(cluster&7))))
			continue;

		// check for door connection
		if((r_refdef.areabits[leaf->area>>3] & (1<<(leaf->area & 7 ))))
			continue;	// not visible

		parent = (node_t *)leaf;
		do {
			if( parent->visFrame == r_visFrameCount )
				break;
			parent->visFrame = r_visFrameCount;
			parent = parent->parent;
		} while( parent );
	}
}

/*
=================
R_RecursiveWorldNode
=================
*/
static void R_RecursiveWorldNode( node_t *node, int planeBits, int dlightBits )
{
	cplane_t		*plane;
	surface_t		*surf, **mark;
	int		i, clipped;

	while( 1 )
	{
		int	newDlights[2];

		if( node->contents == CONTENTS_SOLID )
			return;	// solid

		if( node->visFrame != r_visFrameCount )
			return;

		// if the bounding volume is outside the frustum, nothing
		// inside can be visible OPTIMIZE: don't do this all the way to leafs?
		// g-cont. because we throw node->firstface and node->numfaces !!!!
		if( planeBits )
		{
			for( i = 0, plane = r_frustum; i < 4; i++, plane++ )
			{
				if(!(planeBits & (1<<i))) continue;

				clipped = BoxOnPlaneSide( node->mins, node->maxs, plane );
				if( clipped == 2 ) return;
				if( clipped == 1 ) planeBits &= ~(1<<i);
			}
		}

		if( node->contents != CONTENTS_NODE ) break;

		// node is just a decision point, so go down both sides
		// since we don't care about sort orders, just go positive to negative

		// determine which dlights are needed
		newDlights[0] = 0;
		newDlights[1] = 0;
		if( dlightBits )
		{
			for( i = 0; i < r_numDLights; i++ )
			{
				dlight_t	*dl;
				float	dist;

				if( dlightBits & (1<<i ))
				{
					dl = &r_dlights[i];
					dist = DotProduct( dl->origin, node->plane->normal ) - node->plane->dist;
					
					if( dist > -dl->intensity ) newDlights[0] |= (1<<i);
					if( dist < dl->intensity ) newDlights[1] |= (1<<i);
				}
			}
		}

		// recurse down the children, front side first
		R_RecursiveWorldNode( node->children[0], planeBits, newDlights[0] );

		// tail recurse
		node = node->children[1];
		dlightBits = newDlights[1];
	}

	// add to world mins/maxs
	AddPointToBounds( node->mins, r_worldMins, r_worldMaxs );
	AddPointToBounds( node->maxs, r_worldMins, r_worldMaxs );

	r_stats.numLeafs++;

	// add the individual surfaces
	for( i = 0, mark = node->firstMarkSurface; i < node->numMarkSurfaces; i++, mark++ )
	{
		surf = *mark;

		if( surf->visFrame == r_frameCount )
			continue;	// already added this surface from another leaf
		surf->visFrame = r_frameCount;
		if( surf->dlightFrame != r_frameCount )
		{
			surf->dlightFrame = r_frameCount;
			surf->dlightBits = dlightBits;
		}
		else surf->dlightBits |= dlightBits;

		// cull
		if( R_CullSurface( surf, r_refdef.vieworg, planeBits ))
			continue;

		// clip sky surfaces
		if( surf->texInfo->flags & SURF_SKY )
		{
			R_ClipSkySurface( surf );
			continue;
		}

		// add the surface
		R_AddSurfaceToList( surf, r_worldEntity );
	}
}

/*
=================
R_AddWorldToList
=================
*/
void R_AddWorldToList( void )
{
	int	planeBits;
	int	dlightBits;

	if( r_refdef.rdflags & RDF_NOWORLDMODEL )
		return;

	if( !r_drawworld->integer )
		return;

	if( r_nocull->integer ) planeBits = 0;
	else planeBits = MAX_CLIPFLAGS;

	if( !r_dynamiclights->integer ) dlightBits = 0;
	else dlightBits = (1<<r_numDLights ) - 1;

	// bump frame count
	r_frameCount++;

	// auto cycle the world frame for texture animation
	r_worldEntity->frame = (int)(r_refdef.time * 2);
	r_stats.numDLights += r_numDLights;

	// clear world mins/maxs
	ClearBounds( r_worldMins, r_worldMaxs );

	R_MarkLeaves();
	R_ClearSky();	// S.T.A.L.K.E.R ClearSky

	R_RecursiveWorldNode( r_worldModel->nodes, planeBits, dlightBits );
	R_AddSkyToList();
}
