//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    r_surface.c - surface-related refresh code
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "const.h"

#define BACKFACE_EPSILON	0.01

rmodel_t		*r_worldModel;
ref_entity_t	*r_worldEntity;
vec3_t		r_worldMins, r_worldMaxs;
int		r_frameCount;
int		r_visFrameCount;
int		r_viewCluster, r_viewCluster2;
int		r_oldViewCluster, r_oldViewCluster2;
int		numRadarEnts = 0;
radar_ent_t	RadarEnts[MAX_RADAR_ENTS];

void R_SurfaceRenderMode( void )
{
	if( m_pCurrentShader->stages[0]->renderMode == m_pCurrentEntity->rendermode )
		return;

	m_pCurrentShader->flags |= SHADER_SORT;

	switch( m_pCurrentEntity->rendermode )
	{
	case kRenderTransColor:
		m_pCurrentShader->stages[0]->flags |= SHADERSTAGE_BLENDFUNC;
		m_pCurrentShader->stages[0]->blendFunc.src = GL_SRC_COLOR;
		m_pCurrentShader->stages[0]->blendFunc.dst = GL_ZERO;
		m_pCurrentShader->sort = SORT_OPAQUE;
		break;
	case kRenderTransTexture:
		m_pCurrentShader->stages[0]->flags |= SHADERSTAGE_BLENDFUNC;
		m_pCurrentShader->stages[0]->blendFunc.src = GL_SRC_COLOR;
		m_pCurrentShader->stages[0]->blendFunc.dst = GL_SRC_ALPHA;
		m_pCurrentShader->sort = SORT_BLEND;
		break;
	case kRenderTransAlpha:
		m_pCurrentShader->stages[0]->flags |= SHADERSTAGE_ALPHAFUNC;
		m_pCurrentShader->stages[0]->alphaFunc.func = GL_GREATER;
		m_pCurrentShader->stages[0]->alphaFunc.ref = 0.666;
		m_pCurrentShader->sort = SORT_SEETHROUGH;
		break;
	case kRenderTransAdd:
		m_pCurrentShader->stages[0]->flags |= SHADERSTAGE_BLENDFUNC;
		m_pCurrentShader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
		m_pCurrentShader->stages[0]->blendFunc.dst = GL_ONE;
		m_pCurrentShader->sort = SORT_ADDITIVE;
		break;
	case kRenderNormal:
	default:
		m_pCurrentShader->sort = SORT_OPAQUE;
		m_pCurrentShader->stages[0]->flags &= ~(SHADERSTAGE_BLENDFUNC|SHADERSTAGE_ALPHAFUNC);
		break;
	}
	m_pCurrentShader->stages[0]->renderMode = m_pCurrentEntity->rendermode;
}

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

	// HACKHACK: manually set rendermode for surfaces
	if(!(m_pCurrentShader->flags & SHADER_EXTERNAL) && m_pCurrentEntity != r_worldEntity )
		R_SurfaceRenderMode();

	for( p = surf->poly; p; p = p->next )
	{
#if 0
		GL_Begin( GL_POLYGON );

		for( i = 0, v = p->vertices; i < p->numVertices; i++, v++ )
		{
                              GL_TexCoord4f( v->st[0], v->st[1], v->lm[0], v->lm[1] );
			GL_Binormal3fv( surf->binormal );
			GL_Tangent3fv( surf->tangent );
			GL_Normal3fv( surf->normal );
			GL_Color4fv( v->color );
			GL_Vertex3fv( v->xyz );
		}
		GL_End();
#else
		RB_CheckMeshOverflow( p->numIndices, p->numVertices );

		for( i = 0; i < p->numIndices; i += 3 )
		{
			ref.indexArray[ref.numIndex++] = ref.numVertex + p->indices[i+0];
			ref.indexArray[ref.numIndex++] = ref.numVertex + p->indices[i+1];
			ref.indexArray[ref.numIndex++] = ref.numVertex + p->indices[i+2];
		}

		for( i = 0, v = p->vertices; i < p->numVertices; i++, v++ )
		{
			ref.vertexArray[ref.numVertex][0] = v->xyz[0];
			ref.vertexArray[ref.numVertex][1] = v->xyz[1];
			ref.vertexArray[ref.numVertex][2] = v->xyz[2];
			ref.tangentArray[ref.numVertex][0] = surf->tangent[0];
			ref.tangentArray[ref.numVertex][1] = surf->tangent[1];
			ref.tangentArray[ref.numVertex][2] = surf->tangent[2];
			ref.binormalArray[ref.numVertex][0] = surf->binormal[0];
			ref.binormalArray[ref.numVertex][1] = surf->binormal[1];
			ref.binormalArray[ref.numVertex][2] = surf->binormal[2];
			ref.normalArray[ref.numVertex][0] = surf->normal[0];
			ref.normalArray[ref.numVertex][1] = surf->normal[1];
			ref.normalArray[ref.numVertex][2] = surf->normal[2];
			ref.inTexCoordArray[ref.numVertex][0] = v->st[0];
			ref.inTexCoordArray[ref.numVertex][1] = v->st[1];
			ref.inTexCoordArray[ref.numVertex][2] = v->lm[0];
			ref.inTexCoordArray[ref.numVertex][3] = v->lm[1];
			Vector4Copy( v->color, ref.colorArray[ref.numVertex] );
			ref.numVertex++;
		}
#endif
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
	int		map, lmNum;
	ref_shader_t	*shader;

	if( surf->texInfo->shader->surfaceParm & SURF_NODRAW )
		return;

	shader = surf->texInfo->shader;

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

	// also add caustics
	if( r_caustics->integer )
	{
		if( surf->flags & SURF_WATERCAUSTICS )
			R_AddMeshToList( MESH_SURFACE, surf, tr.waterCausticsShader, entity, 0 );
		if( surf->flags & SURF_SLIMECAUSTICS )
			R_AddMeshToList( MESH_SURFACE, surf, tr.slimeCausticsShader, entity, 0 );
		if( surf->flags & SURF_LAVACAUSTICS )
			R_AddMeshToList( MESH_SURFACE, surf, tr.lavaCausticsShader, entity, 0 );
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
	if( !Matrix3x3_Compare( m_pCurrentEntity->matrix, matrix3x3_identity ))
	{
		for( i = 0; i < 3; i++ )
		{
			mins[i] = entity->origin[i] - model->radius;
			maxs[i] = entity->origin[i] + model->radius;
		}

		if( R_CullSphere( entity->origin, model->radius, MAX_CLIPFLAGS ))
			return;

		VectorSubtract( r_origin, entity->origin, tmp );
		Matrix3x3_Transform( m_pCurrentEntity->matrix, tmp, origin );
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
	byte		*vis, fatVis[MAX_MAP_LEAFS/8];
	node_t		*node;
	leaf_t		*leaf;
	vec3_t		tmp;
	int		i, c;

	// Current view cluster
	r_oldViewCluster = r_viewCluster;
	r_oldViewCluster2 = r_viewCluster2;

	leaf = R_PointInLeaf( r_refdef.vieworg );

	if( r_showcluster->integer )
		Msg( "Cluster: %i, Area: %i\n", leaf->cluster, leaf->area );

	r_viewCluster = r_viewCluster2 = leaf->cluster;

	// check above and below so crossing solid water doesn't draw wrong
	if( !leaf->contents )
	{
		// look down a bit
		VectorCopy( r_refdef.vieworg, tmp );
		tmp[2] -= 16;
		leaf = R_PointInLeaf( tmp );
		if(!(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != r_viewCluster2))
			r_viewCluster2 = leaf->cluster;
	}
	else
	{
		// Look up a bit
		VectorCopy( r_refdef.vieworg, tmp );
		tmp[2] += 16;
		leaf = R_PointInLeaf( tmp );
		if(!(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != r_viewCluster2))
			r_viewCluster2 = leaf->cluster;
	}

	if( r_viewCluster == r_oldViewCluster && r_viewCluster2 == r_oldViewCluster2 && !r_novis->integer && r_viewCluster != -1 )
		return;

	// development aid to let you run around and see exactly where the PVS ends
	if( r_lockpvs->integer )
		return;

	r_visFrameCount++;
	r_oldViewCluster = r_viewCluster;
	r_oldViewCluster2 = r_viewCluster2;

	if( r_novis->integer || r_viewCluster == -1 || !r_worldModel->vis )
	{
		// mark everything
		for( i = 0, leaf = r_worldModel->leafs; i < r_worldModel->numLeafs; i++, leaf++ )
			leaf->visFrame = r_visFrameCount;
		for( i = 0, node = r_worldModel->nodes; i < r_worldModel->numNodes; i++, node++ )
			node->visFrame = r_visFrameCount;
		return;
	}

	// may have to combine two clusters because of solid water boundaries
	vis = R_ClusterPVS(r_viewCluster);
	if( r_viewCluster != r_viewCluster2 )
	{
		Mem_Copy( fatVis, vis, (r_worldModel->numLeafs+7)/8);
		vis = R_ClusterPVS( r_viewCluster2 );
		c = (r_worldModel->numLeafs+31)/32;
		for( i = 0; i < c; i++ )
			((int *)fatVis)[i] |= ((int *)vis)[i];
		vis = fatVis;
	}
	
	for( i = 0, leaf = r_worldModel->leafs; i < r_worldModel->numLeafs; i++, leaf++ )
	{
		if( leaf->cluster == -1 )
			continue;
		if(!(vis[leaf->cluster>>3] & (1<<(leaf->cluster&7))))
			continue;

		node = (node_t *)leaf;
		do
		{
			if( node->visFrame == r_visFrameCount )
				break;
			node->visFrame = r_visFrameCount;
			node = node->parent;
		} while( node );
	}
}

/*
=================
R_RecursiveWorldNode
=================
*/
static void R_RecursiveWorldNode( node_t *node, int clipFlags )
{
	leaf_t		*leaf;
	surface_t		*surf, **mark;
	cplane_t		*plane;
	int		i, clipped;

	if( node->contents == CONTENTS_SOLID )
		return;	// solid

	if( node->visFrame != r_visFrameCount )
		return;

	// cull
	if( clipFlags )
	{
		for( i = 0, plane = r_frustum; i < 4; i++, plane++ )
		{
			if(!(clipFlags & (1<<i)))
				continue;

			clipped = BoxOnPlaneSide( node->mins, node->maxs, plane );
			if( clipped == 2 ) return;
			if( clipped == 1 ) clipFlags &= ~(1<<i);
		}
	}

	// recurse down the children
	if( node->contents == -1 )
	{
		R_RecursiveWorldNode( node->children[0], clipFlags );
		R_RecursiveWorldNode( node->children[1], clipFlags );
		return;
	}

	// if a leaf node, draw stuff
	leaf = (leaf_t *)node;

	if( !leaf->numMarkSurfaces ) return;

	// check for door connected areas
	if( !r_refdef.nextView && r_refdef.areabits )	// HACKHACK for pre-alpha release
	{
		if(!(r_refdef.areabits[leaf->area>>3] & (1<<(leaf->area&7))))
			return; // not visible
	}

	// add to world mins/maxs
	AddPointToBounds( leaf->mins, r_worldMins, r_worldMaxs );
	AddPointToBounds( leaf->maxs, r_worldMins, r_worldMaxs );

	r_stats.numLeafs++;

	// add all the surfaces
	for( i = 0, mark = leaf->firstMarkSurface; i < leaf->numMarkSurfaces; i++, mark++ )
	{
		surf = *mark;

		if( surf->visFrame == r_frameCount )
			continue;	// already added this surface from another leaf
		surf->visFrame = r_frameCount;

		// cull
		if( R_CullSurface( surf, r_refdef.vieworg, clipFlags ))
			continue;

		// clip sky surfaces
		if( surf->texInfo->surfaceFlags & SURF_SKY )
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
	if( r_refdef.onlyClientDraw )
		return;

	if( !r_drawworld->integer )
		return;

	// bump frame count
	r_frameCount++;

	// auto cycle the world frame for texture animation
	r_worldEntity->frame = (int)(r_refdef.time * 2);

	// clear world mins/maxs
	ClearBounds( r_worldMins, r_worldMaxs );

	R_MarkLeaves();
	R_MarkLights();

	R_ClearSky();

	if( r_nocull->integer ) R_RecursiveWorldNode( r_worldModel->nodes, 0 );
	else R_RecursiveWorldNode( r_worldModel->nodes, 15 );
	R_AddSkyToList();
}
