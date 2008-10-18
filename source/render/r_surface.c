//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    r_surface.c - surface-related refresh code
//=======================================================================

#include "r_local.h"
#include "r_meshbuffer.h"
#include "mathlib.h"
#include "matrixlib.h"
#include "const.h"

rmodel_t		*r_worldModel;
ref_entity_t	*r_worldEntity;
vec3_t		r_worldMins, r_worldMaxs;
int		r_frameCount;
int		r_visFrameCount;
int		r_areabitsChanged;
int		r_viewCluster;
int		r_oldViewCluster;
int		numRadarEnts = 0;
radar_ent_t	RadarEnts[MAX_RADAR_ENTS];
static vec3_t	modelorg;       // relative to viewpoint
static vec3_t	modelmins;
static vec3_t	modelmaxs;


/*
=============================================================

BRUSH MODELS

=============================================================
*/

/*
=================
R_SurfPotentiallyVisible
=================
*/
bool R_SurfPotentiallyVisible( msurface_t *surf )
{
	if( surf->faceType == MST_FLARE )
		return true;
	if( surf->flags & SURF_NODRAW )
		return false;
	if( !surf->mesh || R_InvalidMesh( surf->mesh ) )
		return false;
	return true;
}

/*
=================
R_CullSurface
=================
*/
bool R_CullSurface( msurface_t *surf, uint clipflags )
{
	ref_shader_t *shader = surf->shader;

	if(( shader->flags & SHADER_SKYPARMS ) && r_fastsky->integer )
		return true;
	if( r_nocull->integer )
		return false;
	if( shader->flags & SHADER_AUTOSPRITE )
		return false;

	// flare
	if( surf->faceType == MST_FLARE )
	{
		if( r_flares->integer && r_flarefade->value )
		{
			vec3_t origin;

			if( Ref.m_pCurrentModel != r_worldModel )
			{
				Matrix3x3_Transform( Ref.m_pCurrentEntity->matrix, surf->origin, origin );
				VectorAdd( origin, Ref.m_pCurrentEntity->origin, origin );
			}
			else
			{
				VectorCopy( surf->origin, origin );
			}

			// cull it because we don't want to sort unneeded things
			if((origin[0] - Ref.vieworg[0]) * Ref.forward[0] + (origin[1] - Ref.vieworg[1])
			* Ref.forward[1] + (origin[2] - Ref.vieworg[2]) * Ref.forward[2] < 0 )
				return true;
			return ( clipflags && R_CullSphere( origin, 1, clipflags ));
		}
		return true;
	}

	if( surf->faceType == MST_PLANAR && !r_nocull->integer )
	{
		cplane_t	*plane;
		float	dist;

		// NOTE: bsplib have support for SURF_PLANEBACK
		// this code don't working correctly with another bsp compilers			

		// find which side of the node we are on
		plane = surf->plane;
		if( plane->type < 3 ) dist = modelorg[plane->type] - plane->dist;
		else dist = DotProduct( modelorg, plane->normal ) - plane->dist;

		if(!(surf->flags & SURF_PLANEBACK) || ( Ref.params & RP_MIRRORVIEW ))
		{
			if( dist <= BACKFACE_EPSILON )
				return true; // wrong side
		}
		else
		{
			if( dist >= -BACKFACE_EPSILON )
				return true; // wrong side
		}
	}
	return ( clipflags && R_CullBox( surf->mins, surf->maxs, clipflags ));
}

/*
=================
R_AddSurfaceToList
=================
*/
static meshbuffer_t *R_AddSurfaceToList( msurface_t *surf, uint clipflags )
{
	ref_shader_t		*shader;
	meshbuffer_t	*mb;

	if( R_CullSurface( surf, clipflags ))
		return NULL;

	shader = ((r_drawworld->integer == 2) ? R_OcclusionShader() : surf->shader);
	if( shader->flags & SHADER_SKYPARMS )
	{
		bool vis = R_AddSkySurface( surf );
		if( ( Ref.params & RP_NOSKY ) && vis )
		{
			R_AddMeshToList( MESH_MODEL, surf->fog, shader, surf - r_worldBrushModel->surfaces + 1 );
			Ref.params &= ~RP_NOSKY;
		}
		return NULL;
	}

	if( OCCLUSION_QUERIES_ENABLED( Ref ))
	{
		if( shader->flags & SHADER_PORTAL )
			R_SurfOcclusionQueryKey( Ref.m_pCurrentEntity, surf );
		if( OCCLUSION_OPAQUE_SHADER( shader ))
			R_AddOccludingSurface( surf, shader );
	}

	mb = R_AddMeshToList( surf->faceType == MST_FLARE ? MESH_SPRITE : MESH_MODEL, surf->fog, shader, surf - r_worldBrushModel->surfaces + 1 );
	Ref.surfmbuffers[surf - r_worldBrushModel->surfaces] = mb;
	return mb;
}

/*
=================
R_CullBrushModel
=================
*/
bool R_CullBrushModel( ref_entity_t *e )
{
	int		i;
	bool		rotated;
	rmodel_t		*model = e->model;
	mbrushmodel_t	*bmodel = (mbrushmodel_t *)model->extradata;

	if( bmodel->numModelSurfaces == 0 )
		return true;

	if( !Matrix3x3_Compare( e->matrix, matrix3x3_identity ))
	{
		rotated = true;
		for( i = 0; i < 3; i++ )
		{
			modelmins[i] = e->origin[i] - model->radius * e->scale;
			modelmaxs[i] = e->origin[i] + model->radius * e->scale;
		}
		if( R_CullSphere( e->origin, model->radius * e->scale, Ref.clipFlags ))
			return true;
	}
	else
	{
		rotated = false;
		VectorMA( e->origin, e->scale, model->mins, modelmins );
		VectorMA( e->origin, e->scale, model->maxs, modelmaxs );
		if( R_CullBox( modelmins, modelmaxs, Ref.clipFlags ))
			return true;
	}

	if( Ref.refdef.rdflags & ( RDF_PORTALINVIEW|RDF_SKYPORTALINVIEW ) || ( Ref.params & RP_SKYPORTALVIEW ))
	{
		if( rotated )
		{
			if( R_VisCullSphere( e->origin, model->radius * e->scale ) )
				return true;
		}
		else
		{
			if( R_VisCullBox( modelmins, modelmaxs ) )
				return true;
		}
	}
	return false;
}

/*
=================
R_AddBrushModelToList
=================
*/
void R_AddBrushModelToList( ref_entity_t *e )
{
	uint		i;
	bool		rotated;
	rmodel_t		*model = e->model;
	mbrushmodel_t	*bmodel = ( mbrushmodel_t * )model->extradata;
	msurface_t	*psurf;
	uint		dlightbits;
	meshbuffer_t	*mb;

	rotated = !Matrix3x3_Compare( e->matrix, matrix3x3_identity );
	VectorSubtract( Ref.refdef.vieworg, e->origin, modelorg );
	if( rotated )
	{
		vec3_t temp;

		VectorCopy( modelorg, temp );
		Matrix3x3_Transform( e->matrix, temp, modelorg );
	}

	dlightbits = 0;
	if(( r_dynamiclights->integer == 1 ) && !r_fullbright->integer && !( Ref.params & RP_SHADOWMAPVIEW ))
	{
		for( i = 0; i < r_numDLights; i++ )
		{
			if( BoundsIntersect( modelmins, modelmaxs, r_dlights[i].mins, r_dlights[i].maxs ))
				dlightbits |= ( 1<<i );
		}
	}

	for( i = 0, psurf = bmodel->firstModelSurface; i < (unsigned)bmodel->numModelSurfaces; i++, psurf++ )
	{
		if( !R_SurfPotentiallyVisible( psurf ))
			continue;

		if( Ref.params & RP_SHADOWMAPVIEW )
		{
			if( psurf->visFrame != r_frameCount )
				continue;
			if(( psurf->shader->sort >= SORT_OPAQUE ) && ( psurf->shader->sort <= SORT_BANNER ))
			{
				if( oldRef.surfmbuffers[psurf - r_worldBrushModel->surfaces] )
				{
					if( !R_CullSurface( psurf, 0 ) )
					{
						Ref.params |= RP_WORLDSURFVISIBLE;
						oldRef.surfmbuffers[psurf - r_worldBrushModel->surfaces]->shadowbits |= Ref.shadowGroup->bit;
					}
				}
			}
			continue;
		}

		psurf->visFrame = r_frameCount;
		mb = R_AddSurfaceToList( psurf, 0 );
		if( mb )
		{
			mb->sortKey |= (( psurf->superLightStyle+1 )<<10 );
			if( R_SurfPotentiallyLit( psurf ))
				mb->dlightbits = dlightbits;
		}
	}
}

/*
=============================================================

WORLD MODEL

=============================================================
*/
/*
================
R_MarkLeafSurfaces
================
*/
static void R_MarkLeafSurfaces( msurface_t **mark, uint clipflags, uint dlightbits )
{
	uint		newDlightbits;
	msurface_t	*surf;
	meshbuffer_t	*mb;

	do
	{
		surf = *mark++;

		// NOTE: that R_AddSurfaceToList may set meshBuffer to NULL
		// for world ALL surfaces to prevent referencing to freed memory region
		if( surf->visFrame != r_frameCount )
		{
			surf->visFrame = r_frameCount;
			mb = R_AddSurfaceToList( surf, clipflags );
			if( mb ) mb->sortKey |= (( surf->superLightStyle+1 )<<10 );
		}
		else mb = Ref.surfmbuffers[surf - r_worldBrushModel->surfaces];

		newDlightbits = mb ? dlightbits & ~mb->dlightbits : 0;
		if( newDlightbits && R_SurfPotentiallyLit( surf ))
			mb->dlightbits |= R_AddSurfDlighbits( surf, newDlightbits );
	} while( *mark );
}

/*
================
R_RecursiveWorldNode
================
*/
static void R_RecursiveWorldNode( mnode_t *node, uint clipflags, uint dlightbits )
{
	uint		i, bit, newDlightbits;
	const cplane_t	*clipplane;
	mleaf_t		*pleaf;

	while( 1 )
	{
		if( node->visFrame != r_visFrameCount )
			return;

		if( clipflags )
		{
			i = sizeof(Ref.frustum) / sizeof( Ref.frustum[0]);
			for( bit = 1, clipplane = Ref.frustum; i > 0; i--, bit<<=1, clipplane++ )
			{
				if( clipflags & bit )
				{
					int clipped = BoxOnPlaneSide( node->mins, node->maxs, clipplane );
					if( clipped == 2 ) return;
					if( clipped == 1 ) clipflags &= ~bit; // node is entirely on screen
				}
			}
		}

		if( !node->plane ) break;

		newDlightbits = 0;
		if( dlightbits )
		{
			float dist;

			for( i = 0, bit = 1; i < r_numDLights; i++, bit<<=1 )
			{
				if(!( dlightbits & bit ))
					continue;

				dist = PlaneDiff( r_dlights[i].origin, node->plane );
				if( dist < -r_dlights[i].intensity )
					dlightbits &= ~bit;
				if( dist < r_dlights[i].intensity )
					newDlightbits |= bit;
			}
		}

		R_RecursiveWorldNode( node->children[0], clipflags, dlightbits );

		node = node->children[1];
		dlightbits = newDlightbits;
	}

	// if a leaf node, draw stuff
	pleaf = ( mleaf_t * )node;
	pleaf->visFrame = r_frameCount;

	// add leaf bounds to view bounds
	for( i = 0; i < 3; i++ )
	{
		Ref.visMins[i] = min( Ref.visMins[i], pleaf->mins[i] );
		Ref.visMaxs[i] = max( Ref.visMaxs[i], pleaf->maxs[i] );
	}

	R_MarkLeafSurfaces( pleaf->firstVisSurface, clipflags, dlightbits );
}

/*
================
R_MarkShadowLeafSurfaces
================
*/
static void R_MarkShadowLeafSurfaces( msurface_t **mark, uint clipflags )
{
	msurface_t	*surf;
	meshbuffer_t	*mb;
	const uint	bit = Ref.shadowGroup->bit;

	do
	{
		surf = *mark++;
		if( surf->flags & ( SURF_NOIMPACT|SURF_NODRAW ))
			continue;

		mb = oldRef.surfmbuffers[surf - r_worldBrushModel->surfaces];
		if( !mb || (mb->shadowbits & bit))
			continue;

		// this surface is visible in previous RI, not marked as shadowed...
		if(( surf->shader->sort >= SORT_OPAQUE ) && ( surf->shader->sort <= SORT_ALPHATEST ))
		{	
			// ...is opaque
			if( !R_CullSurface( surf, clipflags ))
			{	
				// and is visible to the light source too
				Ref.params |= RP_WORLDSURFVISIBLE;
				mb->shadowbits |= bit;
			}
		}
	} while( *mark );
}

/*
================
R_LinearShadowLeafs
================
*/
static void R_LinearShadowLeafs( void )
{
	uint		i, j;
	uint		cpf, bit;
	const cplane_t	*clipplane;
	mleaf_t		*pleaf;

	for( j = r_worldBrushModel->numleafs, pleaf = r_worldBrushModel->leafs; j > 0; j--, pleaf++ )
	{
		if( pleaf->visFrame != r_frameCount )
			continue;
		if( !( Ref.shadowGroup->vis[pleaf->cluster>>3] & ( 1<<( pleaf->cluster&7 ) ) ) )
			continue;

		cpf = Ref.clipFlags;
		i = sizeof( Ref.frustum ) / sizeof( Ref.frustum[0] );
		for( bit = 1, clipplane = Ref.frustum; i > 0; i--, bit<<=1, clipplane++ )
		{
			int clipped = BoxOnPlaneSide( pleaf->mins, pleaf->maxs, clipplane );
			if( clipped == 2 ) break;
			if( clipped == 1 ) cpf &= ~bit; // leaf is entirely on screen
		}

		if( !i ) R_MarkShadowLeafSurfaces( pleaf->firstVisSurface, cpf );
	}
}

//==================================================================================

int r_surfQueryKeys[MAX_SURF_QUERIES];

/*
===============
R_ClearSurfOcclusionQueryKeys
===============
*/
void R_ClearSurfOcclusionQueryKeys( void )
{
	Mem_Set( r_surfQueryKeys, -1, sizeof( r_surfQueryKeys ));
}

/*
===============
R_SurfOcclusionQueryKey
===============
*/
int R_SurfOcclusionQueryKey( ref_entity_t *e, msurface_t *surf )
{
	int	i;
	int	*keys = r_surfQueryKeys;
	int	key = surf - r_worldBrushModel->surfaces;

	if( e != r_worldEntity ) return -1;

	for( i = 0; i < MAX_SURF_QUERIES; i++ )
	{
		if( keys[i] >= 0 )
		{
			if( keys[i] == key )
				return i;
		}
		else
		{
			keys[i] = key;
			return i;
		}
	}
	return -1;
}

/*
===============
R_SurfIssueOcclusionQueries
===============
*/
void R_SurfIssueOcclusionQueries( void )
{
	int i, *keys = r_surfQueryKeys;
	msurface_t *surf;

	for( i = 0; keys[i] >= 0; i++ )
	{
		surf = &r_worldBrushModel->surfaces[keys[i]];
		R_IssueOcclusionQuery( R_GetOcclusionQueryNum( OQ_CUSTOM, i ), r_worldEntity, surf->mins, surf->maxs );
	}
}

//==================================================================================

/*
=============
R_CalcDistancesToFogVolumes
=============
*/
static void R_CalcDistancesToFogVolumes( void )
{
	int	i;
	mfog_t	*fog;

	for( i = 0, fog = r_worldBrushModel->fogs; i < r_worldBrushModel->numFogs; i++, fog++ )
		Ref.fog_dist_to_eye[fog - r_worldBrushModel->fogs] = PlaneDiff( Ref.vieworg, fog->visible );
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld( void )
{
	int	clipflags, msec = 0;
	uint	dlightbits;

	if( !r_drawworld->integer ) return;
	if( !r_worldModel ) return;
	if( Ref.refdef.rdflags & RDF_NOWORLDMODEL )
		return;

	VectorCopy( Ref.refdef.vieworg, modelorg );

	Ref.m_pPrevEntity = NULL;
	Ref.m_pCurrentEntity = r_worldEntity;
	Ref.m_pCurrentModel = Ref.m_pCurrentEntity->model;

	if( !( Ref.params & RP_SHADOWMAPVIEW ) )
	{
		R_AllocMeshbufPointers( &Ref );
		memset( Ref.surfmbuffers, 0, r_worldBrushModel->numsurfaces * sizeof( meshbuffer_t * ));

		R_CalcDistancesToFogVolumes();
	}

	ClearBounds( Ref.visMins, Ref.visMaxs );
	R_ClearSky();

	if( r_nocull->integer ) clipflags = 0;
	else clipflags = Ref.clipFlags;

	if( r_dynamiclights->integer != 1 || r_fullbright->integer )
		dlightbits = 0;
	else dlightbits = r_numDLights < 32 ? ( 1 << r_numDLights ) - 1 : -1;

	if( Ref.params & RP_SHADOWMAPVIEW ) R_LinearShadowLeafs ();
	else R_RecursiveWorldNode( r_worldBrushModel->nodes, clipflags, dlightbits );
}

/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current cluster
===============
*/
void R_MarkLeaves( void )
{
	byte	*vis;
	int	i;
	mleaf_t	*leaf, **pleaf;
	mnode_t	*node;
	int	cluster;

	if( Ref.refdef.rdflags & RDF_NOWORLDMODEL )
		return;
	if( r_oldViewCluster == r_viewCluster && ( Ref.refdef.rdflags & RDF_OLDAREABITS ) && !r_novis->integer && r_viewCluster != -1 )
		return;
	if( Ref.params & RP_SHADOWMAPVIEW )
		return;

	// development aid to let you run around and see exactly where the pvs ends
	if( r_lockpvs->integer ) return;

	r_visFrameCount++;
	r_oldViewCluster = r_viewCluster;

	if( r_novis->integer || r_viewCluster == -1 || !r_worldBrushModel->vis )
	{
		// mark everything
		for( pleaf = r_worldBrushModel->visleafs, leaf = *pleaf; leaf; leaf = *pleaf++ )
			leaf->visFrame = r_visFrameCount;
		for( i = 0, node = r_worldBrushModel->nodes; i < r_worldBrushModel->numnodes; i++, node++ )
			node->visFrame = r_visFrameCount;
		return;
	}

	vis = R_ClusterPVS( r_viewCluster );
	for( pleaf = r_worldBrushModel->visleafs, leaf = *pleaf; leaf; leaf = *pleaf++ )
	{
		cluster = leaf->cluster;

		// check for door connected areas
		if( Ref.refdef.areabits )
		{
			if(!( Ref.refdef.areabits[leaf->area>>3] & (1<<( leaf->area&7 ))))
				continue; // not visible
		}

		if( vis[cluster>>3] & (1<<( cluster&7 )))
		{
			node = (mnode_t *)leaf;
			do
			{
				if( node->visFrame == r_visFrameCount )
					break;
				node->visFrame = r_visFrameCount;
				node = node->parent;
			}
			while( node );
		}
	}
}