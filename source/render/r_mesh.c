//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     r_mesh.c - transformation and sorting
//=======================================================================

#include "r_local.h"
#include "r_meshbuffer.h"
#include "byteorder.h"
#include "mathlib.h"
#include "matrixlib.h"
#include "const.h"

#define QSORT_MAX_STACKDEPTH		2048

static byte	*r_meshpool;
meshlist_t	r_worldlist, r_shadowlist;
static meshlist_t	r_portallist, r_skyportallist;
static meshbuffer_t	**r_portalSurfMbuffers;
static meshbuffer_t	**r_skyPortalSurfMbuffers;

static void R_QSortMeshBuffers( meshbuffer_t *meshes, int Li, int Ri );
static void R_ISortMeshBuffers( meshbuffer_t *meshes, int num_meshes );
static bool R_AddPortalSurface( const meshbuffer_t *mb );
static bool R_DrawPortalSurface( void );

#define R_MBCopy( in, out ) \
	( \
	( out ).sortKey = ( in ).sortKey, \
	( out ).infoKey = ( in ).infoKey, \
	( out ).dlightbits = ( in ).dlightbits, \
	( out ).shaderKey = ( in ).shaderKey, \
	( out ).shadowbits = ( in ).shadowbits \
	)

#define R_MBCmp( mb1, mb2 ) \
	( \
	( mb1 ).shaderKey > ( mb2 ).shaderKey ? true : \
	( mb1 ).shaderKey < ( mb2 ).shaderKey ? false : \
	( mb1 ).sortKey > ( mb2 ).sortKey ? true : \
	( mb1 ).sortKey < ( mb2 ).sortKey ? false : \
	( mb1 ).dlightbits > ( mb2 ).dlightbits ? true : \
	( mb1 ).dlightbits < ( mb2 ).dlightbits ? false : \
	( mb1 ).shadowbits > ( mb2 ).shadowbits \
	)

/*
================
R_QSortMeshBuffers

Quicksort
================
*/
static void R_QSortMeshBuffers( meshbuffer_t *meshes, int Li, int Ri )
{
	int		li, ri, stackdepth = 0, total = Ri + 1;
	int		lstack[QSORT_MAX_STACKDEPTH];
	int		rstack[QSORT_MAX_STACKDEPTH];
	meshbuffer_t	median, tempbuf;

mark0:
	if( Ri - Li > 8 )
	{
		li = Li;
		ri = Ri;

		R_MBCopy( meshes[( Li+Ri )>>1], median );

		if( R_MBCmp( meshes[Li], median ) )
		{
			if( R_MBCmp( meshes[Ri], meshes[Li] ) )
				R_MBCopy( meshes[Li], median );
		}
		else if( R_MBCmp( median, meshes[Ri] ) )
		{
			R_MBCopy( meshes[Ri], median );
		}

		do
		{
			while( R_MBCmp( median, meshes[li] )) li++;
			while( R_MBCmp( meshes[ri], median )) ri--;

			if( li <= ri )
			{
				R_MBCopy( meshes[ri], tempbuf );
				R_MBCopy( meshes[li], meshes[ri] );
				R_MBCopy( tempbuf, meshes[li] );

				li++;
				ri--;
			}
		} while( li < ri );

		if(( Li < ri ) && ( stackdepth < QSORT_MAX_STACKDEPTH ))
		{
			lstack[stackdepth] = li;
			rstack[stackdepth] = Ri;
			stackdepth++;
			li = Li;
			Ri = ri;
			goto mark0;
		}

		if( li < Ri )
		{
			Li = li;
			goto mark0;
		}
	}
	if( stackdepth )
	{
		--stackdepth;
		Ri = ri = rstack[stackdepth];
		Li = li = lstack[stackdepth];
		goto mark0;
	}

	for( li = 1; li < total; li++ )
	{
		R_MBCopy( meshes[li], tempbuf );
		ri = li - 1;

		while(( ri >= 0 ) && ( R_MBCmp( meshes[ri], tempbuf )))
		{
			R_MBCopy( meshes[ri], meshes[ri+1] );
			ri--;
		}
		if( li != ri+1 ) R_MBCopy( tempbuf, meshes[ri+1] );
	}
}

/*
================
R_ISortMeshes

Insertion sort
================
*/
static void R_ISortMeshBuffers( meshbuffer_t *meshes, int num_meshes )
{
	int		i, j;
	meshbuffer_t	tempbuf;

	for( i = 1; i < num_meshes; i++ )
	{
		R_MBCopy( meshes[i], tempbuf );
		j = i - 1;

		while( ( j >= 0 ) && ( R_MBCmp( meshes[j], tempbuf )))
		{
			R_MBCopy( meshes[j], meshes[j+1] );
			j--;
		}
		if( i != j+1 ) R_MBCopy( tempbuf, meshes[j+1] );
	}
}

/*
=================
R_ReAllocMeshList
=================
*/
int R_ReAllocMeshList( meshbuffer_t **mb, int minMeshes, int maxMeshes )
{
	int		oldSize, newSize;
	meshbuffer_t	*newMB;

	oldSize = maxMeshes;
	newSize = max( minMeshes, oldSize * 2 );

	newMB = Mem_Alloc( r_meshpool, newSize * sizeof( meshbuffer_t ));
	if( *mb )
	{
		Mem_Copy( newMB, *mb, oldSize * sizeof( meshbuffer_t ));
		Mem_Free( *mb );
	}
	*mb = newMB;

	// NULL all pointers to old membuffers so we don't crash
	if( r_worldModel && !( Ref.refdef.rdflags & RDF_NOWORLDMODEL ) )
		Mem_Set( Ref.surfmbuffers, 0, r_worldBrushModel->numsurfaces * sizeof( meshbuffer_t * ));

	return newSize;
}

/*
=================
R_AddMeshToList

Calculate sortkey and store info used for batching and sorting.
All 3D-geometry passes this function.
=================
*/
meshbuffer_t *R_AddMeshToList( int type, mfog_t *fog, ref_shader_t *shader, int infokey )
{
	meshlist_t	*list;
	meshbuffer_t	*meshbuf;

	if( !shader ) return NULL;

	list = Ref.meshlist;
	if( shader->sort > SORT_OPAQUE )
	{
		if( list->num_translucent_meshes >= list->max_translucent_meshes ) // reallocate if needed
			list->max_translucent_meshes = R_ReAllocMeshList( &list->meshbuffer_translucent, MIN_RENDER_MESHES / 2, list->max_translucent_meshes );

		if( shader->flags & SHADER_PORTAL )
		{
			if( Ref.params & ( RP_MIRRORVIEW|RP_PORTALVIEW|RP_SKYPORTALVIEW ))
				return NULL;
			Ref.meshlist->num_portal_translucent_meshes++;
		}
		meshbuf = &list->meshbuffer_translucent[list->num_translucent_meshes++];
	}
	else
	{
		if( list->num_opaque_meshes >= list->max_opaque_meshes )	// reallocate if needed
			list->max_opaque_meshes = R_ReAllocMeshList( &list->meshbuffer_opaque, MIN_RENDER_MESHES, list->max_opaque_meshes );

		if( shader->flags & SHADER_PORTAL )
		{
			if( Ref.params & ( RP_MIRRORVIEW|RP_PORTALVIEW|RP_SKYPORTALVIEW ) )
				return NULL;
			Ref.meshlist->num_portal_opaque_meshes++;
		}

		meshbuf = &list->meshbuffer_opaque[list->num_opaque_meshes++];
	}

	// FIXME: implement
	// if( shader->flags & SHADER_VIDEOMAP ) R_UploadCinematicShader( shader );

	meshbuf->sortKey = R_EDICTNUM( Ref.m_pCurrentEntity )|R_FOGNUM( fog )|type;
	meshbuf->shaderKey = shader->sortKey;
	meshbuf->infoKey = infokey;
	meshbuf->dlightbits = 0;
	meshbuf->shadowbits = r_entShadowBits[Ref.m_pCurrentEntity - r_entities];

	return meshbuf;
}

/*
=================
R_AddMeshToList
=================
*/
void R_AddModelMeshToList( mfog_t *fog, ref_shader_t *shader, int meshnum )
{
	meshbuffer_t *mb;
	
	mb = R_AddMeshToList( MESH_MODEL, fog, shader, -( meshnum+1 ) );
}

/*
================
R_BatchMeshBuffer

Draw the mesh or batch it.
================
*/
static void R_BatchMeshBuffer( const meshbuffer_t *mb, const meshbuffer_t *nextmb )
{
	int		type, features;
	bool		nonMergable;
	ref_entity_t	*ent;
	ref_shader_t		*shader;
	msurface_t	*surf, *nextSurf;

	R_EDICT_FOR_KEY( mb->sortKey, ent );

	if( Ref.m_pCurrentEntity != ent )
	{
		Ref.m_pPrevEntity = Ref.m_pCurrentEntity;
		Ref.m_pCurrentEntity = ent;
		Ref.m_pCurrentModel = ent->model;
	}

	type = mb->sortKey & 3;

	switch( type )
	{
	case MESH_MODEL:
		switch( ent->model->type )
		{
		case mod_world:
		case mod_brush:
			Shader_ForKey( mb->shaderKey, shader );

			if( shader->flags & SHADER_SKYPARMS )
			{	
				// draw sky
				if( !( Ref.params & RP_NOSKY ))
					R_DrawSky( shader );
				return;
			}

			surf = &r_worldBrushModel->surfaces[mb->infoKey-1];
			nextSurf = NULL;

			features = shader->features;
			if( r_shownormals->integer ) features |= MF_NORMALS;
			features |= r_superLightStyles[surf->superLightStyle].features;

			if( features & MF_NONBATCHED )
			{
				nonMergable = true;
			}
			else
			{	// check if we need to render batched geometry this frame
				if( nextmb && ( nextmb->shaderKey == mb->shaderKey ) && ( nextmb->sortKey == mb->sortKey )
					&& ( nextmb->dlightbits == mb->dlightbits ) && ( nextmb->shadowbits == mb->shadowbits ))
				{
					if( nextmb->infoKey > 0 )
						nextSurf = &r_worldBrushModel->surfaces[nextmb->infoKey-1];
				}

				nonMergable = nextSurf ? R_MeshOverflow2( surf->mesh, nextSurf->mesh ) : true;
				if( nonMergable && !r_stats.numVertices ) features |= MF_NONBATCHED;
			}

			R_PushMesh( surf->mesh, features );

			if( nonMergable )
			{
				if( Ref.m_pPrevEntity != Ref.m_pCurrentEntity )
					R_RotateForEntity( Ref.m_pCurrentEntity );
				R_RenderMeshBuffer( mb );
			}
			break;
		case mod_studio:
			R_DrawStudioModel( mb );
			break;
		case mod_sprite:
			R_PushSpriteModel( mb );

			// no rotation for sprites
			// g-cont: why ?
			R_TranslateForEntity( Ref.m_pCurrentEntity );
			R_RenderMeshBuffer( mb );
			break;
		default: break;
		}
		break;
	case MESH_SPRITE:
	case MESH_CORONA:
		nonMergable = R_PushSpritePoly( mb );
		if( nonMergable || !nextmb || (( nextmb->shaderKey & 0xFC000FFF ) != ( mb->shaderKey & 0xFC000FFF ))
			|| (( nextmb->sortKey & 0xFFFFF ) != ( mb->sortKey & 0xFFFFF )) || R_SpriteOverflow())
		{
			if( !nonMergable )
			{
				Ref.m_pCurrentEntity = r_worldEntity;
				Ref.m_pCurrentModel = r_worldModel;
			}

			// no rotation for sprites
			if( Ref.m_pPrevEntity != Ref.m_pCurrentEntity )
				R_TranslateForEntity( Ref.m_pCurrentEntity );
			R_RenderMeshBuffer( mb );
		}
		break;
	case MESH_POLY:
		// polys are already batched at this point
		R_PushPoly( mb );

		if( Ref.m_pPrevEntity != Ref.m_pCurrentEntity )
			R_LoadIdentity();
		R_RenderMeshBuffer( mb );
		break;
	default:
		Host_Error( "R_BatchMeshBuffer: unsupported mesh type (probably not implemented)\n" ); 
	}
}

/*
================
R_SortMeshList

Use quicksort for opaque meshes and insertion sort for translucent meshes.
================
*/
void R_SortMeshes( void )
{
	if( Ref.meshlist->num_opaque_meshes )
		R_QSortMeshBuffers( Ref.meshlist->meshbuffer_opaque, 0, Ref.meshlist->num_opaque_meshes - 1 );
	if( Ref.meshlist->num_translucent_meshes )
		R_ISortMeshBuffers( Ref.meshlist->meshbuffer_translucent, Ref.meshlist->num_translucent_meshes );
}

/*
================
R_DrawPortals

Render portal views. For regular portals we stop after rendering the
first valid portal view.
Skyportal views are rendered afterwards.
g-cont: rewrote for recursive portal support ?
================
*/
void R_DrawPortals( void )
{
	int		i, trynum, num_meshes, total_meshes;
	meshbuffer_t	*mb;
	ref_shader_t		*shader;

	if( r_viewCluster == -1 )
		return;

	if(!( Ref.params & ( RP_MIRRORVIEW|RP_PORTALVIEW|RP_SHADOWMAPVIEW )))
	{
		if( Ref.meshlist->num_portal_opaque_meshes || Ref.meshlist->num_portal_translucent_meshes )
		{
			trynum = 0;
			R_AddPortalSurface( NULL );

			do
			{
				switch( trynum )
				{
				case 0:
					mb = Ref.meshlist->meshbuffer_opaque;
					total_meshes = Ref.meshlist->num_opaque_meshes;
					num_meshes = Ref.meshlist->num_portal_opaque_meshes;
					break;
				case 1:
					mb = Ref.meshlist->meshbuffer_translucent;
					total_meshes = Ref.meshlist->num_translucent_meshes;
					num_meshes = Ref.meshlist->num_portal_translucent_meshes;
					break;
				default:
					mb = NULL;
					total_meshes = num_meshes = 0;
					break;
				}

				for( i = 0; i < total_meshes && num_meshes; i++, mb++ )
				{
					Shader_ForKey( mb->shaderKey, shader );

					if( shader->flags & SHADER_PORTAL )
					{
						num_meshes--;

						if( r_fastsky->integer && !( shader->flags & (SHADER_REFLECTION|SHADER_REFRACTION)))
							continue;

						if( !R_AddPortalSurface( mb ))
						{
							if(R_DrawPortalSurface())
							{
								trynum = 2;
								break;
							}
						}
					}
				}
			} while( ++trynum < 2 );

			R_DrawPortalSurface();
		}
	}

	if(( Ref.refdef.rdflags & RDF_SKYPORTALINVIEW ) && !( Ref.params & RP_NOSKY ) && !r_fastsky->integer )
	{
		for( i = 0, mb = Ref.meshlist->meshbuffer_opaque; i < Ref.meshlist->num_opaque_meshes; i++, mb++ )
		{
			Shader_ForKey( mb->shaderKey, shader );

			if( shader->flags & SHADER_SKYPARMS )
			{
				R_DrawSky( shader );
				Ref.params |= RP_NOSKY;
				return;
			}
		}
	}
}

/*
================
R_DrawMeshes
================
*/
void R_DrawMeshes( void )
{
	int		i;
	meshbuffer_t	*meshbuf;

	Ref.m_pPrevEntity = NULL;
	if( Ref.meshlist->num_opaque_meshes )
	{
		meshbuf = Ref.meshlist->meshbuffer_opaque;
		for( i = 0; i < Ref.meshlist->num_opaque_meshes - 1; i++, meshbuf++ )
			R_BatchMeshBuffer( meshbuf, meshbuf+1 );
		R_BatchMeshBuffer( meshbuf, NULL );
	}

	if( Ref.meshlist->num_translucent_meshes )
	{
		meshbuf = Ref.meshlist->meshbuffer_translucent;
		for( i = 0; i < Ref.meshlist->num_translucent_meshes - 1; i++, meshbuf++ )
			R_BatchMeshBuffer( meshbuf, meshbuf + 1 );
		R_BatchMeshBuffer( meshbuf, NULL );
	}

	R_LoadIdentity();
}

/*
===============
R_InitMeshLists
===============
*/
void R_InitMeshLists( void )
{
	if( !r_meshpool ) r_meshpool = Mem_AllocPool( "MeshList" );
}

/*
===============
R_AllocMeshbufPointers
===============
*/
void R_AllocMeshbufPointers( ref_state_t *ref )
{
	if( !ref->surfmbuffers )
		ref->surfmbuffers = Mem_Alloc( r_meshpool, r_worldBrushModel->numsurfaces * sizeof(meshbuffer_t *));
}

/*
===============
R_FreeMeshLists
===============
*/
void R_FreeMeshLists( void )
{
	if( !r_meshpool ) return;

	r_portalSurfMbuffers = NULL;
	r_skyPortalSurfMbuffers = NULL;

	Mem_FreePool( &r_meshpool );

	Mem_Set( &r_worldlist, 0, sizeof( meshlist_t ));
	Mem_Set( &r_shadowlist, 0, sizeof( meshlist_t ));
	Mem_Set( &r_portallist, 0, sizeof( meshlist_t ));
	Mem_Set( &r_skyportallist, 0, sizeof( r_skyportallist ));
}

/*
===============
R_ClearMeshList
===============
*/
void R_ClearMeshList( meshlist_t *meshlist )
{
	// clear counters
	meshlist->num_opaque_meshes = 0;
	meshlist->num_translucent_meshes = 0;

	meshlist->num_portal_opaque_meshes = 0;
	meshlist->num_portal_translucent_meshes = 0;
}

/*
===============
RB_DrawTriangleOutlines
===============
*/
void RB_DrawTriangleOutlines( bool showTris, bool showNormals )
{
	if( !showTris && !showNormals )
		return;

	Ref.params |= (showTris ? RP_TRISOUTLINES : 0)|(showNormals ? RP_SHOWNORMALS : 0);
	RB_BeginTriangleOutlines();
	R_DrawMeshes();
	RB_EndTriangleOutlines();
	Ref.params &= ~(RP_TRISOUTLINES|RP_SHOWNORMALS);
}

/*
===============
R_ScissorForPortal

Returns the on-screen scissor box for given bounding box in 3D-space.
===============
*/
bool R_ScissorForPortal( ref_entity_t *ent, vec3_t mins, vec3_t maxs, int *x, int *y, int *w, int *h )
{
	int	i;
	int	ix1, iy1, ix2, iy2;
	float	x1, y1, x2, y2;
	vec3_t	v, bbox[8];

	R_TransformEntityBBox( ent, mins, maxs, bbox, true );

	x1 = y1 = 999999;
	x2 = y2 = -999999;

	for( i = 0; i < 8; i++ )
	{
		// compute and rotate a full bounding box
		vec_t *corner = bbox[i];
		R_TransformWorldToScreen( corner, v );

		if( v[2] < 0 || v[2] > 1 )
		{                    
			// the test point is behind the nearclip plane
			if( PlaneDiff( corner, &Ref.frustum[0] ) < PlaneDiff( corner, &Ref.frustum[1] ))
				v[0] = 0;
			else v[0] = Ref.refdef.rect.width;
			if( PlaneDiff( corner, &Ref.frustum[2] ) < PlaneDiff( corner, &Ref.frustum[3] ))
				v[1] = 0;
			else v[1] = Ref.refdef.rect.height;
		}

		x1 = min( x1, v[0] ); y1 = min( y1, v[1] );
		x2 = max( x2, v[0] ); y2 = max( y2, v[1] );
	}

	ix1 = max( x1 - 1.0f, 0 ); ix2 = min( x2 + 1.0f, Ref.refdef.rect.width );
	if( ix1 >= ix2 ) return false; // FIXME

	iy1 = max( y1 - 1.0f, 0 ); iy2 = min( y2 + 1.0f, Ref.refdef.rect.height );
	if( iy1 >= iy2 ) return false; // FIXME

	*x = ix1;
	*y = iy1;
	*w = ix2 - ix1;
	*h = iy2 - iy1;

	return true;
}

/*
===============
R_AddPortalSurface
===============
*/
static ref_entity_t *r_portal_ent;
static cplane_t r_portal_plane, r_original_portal_plane;
static ref_shader_t *r_portal_shader;
static vec3_t r_portal_mins, r_portal_maxs, r_portal_centre;

static bool R_AddPortalSurface( const meshbuffer_t *mb )
{
	int		i;
	float		dist;
	ref_entity_t	*ent;
	ref_shader_t		*shader;
	msurface_t	*surf;
	cplane_t		plane, oplane;
	rb_mesh_t		*mesh;
	vec3_t		v[3], mins, maxs, centre;
	matrix3x3		entity_rotation;

	if( !mb )
	{
		r_portal_ent = r_worldEntity;
		r_portal_shader = NULL;
		VectorClear( r_portal_plane.normal );
		ClearBounds( r_portal_mins, r_portal_maxs );
		return false;
	}

	R_EDICT_FOR_KEY( mb->sortKey, ent );
	if( !ent->model ) return false;

	surf = mb->infoKey > 0 ? &r_worldBrushModel->surfaces[mb->infoKey-1] : NULL;
	if( !surf || !( mesh = surf->mesh ) || !mesh->points )
		return false;

	Shader_ForKey( mb->shaderKey, shader );

	VectorCopy( mesh->points[mesh->indexes[0]], v[0] );
	VectorCopy( mesh->points[mesh->indexes[1]], v[1] );
	VectorCopy( mesh->points[mesh->indexes[2]], v[2] );
	PlaneFromPoints( v, &oplane );
	oplane.dist += DotProduct( ent->origin, oplane.normal );
	PlaneClassify( &oplane );

	// rotating portal surfaces
	if( !Matrix3x3_Compare( ent->matrix, matrix3x3_identity ))
	{
		Matrix3x3_Transpose( ent->matrix, entity_rotation );
		Matrix3x3_Transform( entity_rotation, mesh->points[mesh->indexes[0]], v[0] );
		VectorMA( ent->origin, ent->scale, v[0], v[0] );
		Matrix3x3_Transform( entity_rotation, mesh->points[mesh->indexes[1]], v[1] );
		VectorMA( ent->origin, ent->scale, v[1], v[1] );
		Matrix3x3_Transform( entity_rotation, mesh->points[mesh->indexes[2]], v[2] );
		VectorMA( ent->origin, ent->scale, v[2], v[2] );
		PlaneFromPoints( v, &plane );
		PlaneClassify( &plane );
	}
	else plane = oplane;

	if(( dist = PlaneDiff( Ref.vieworg, &plane )) <= BACKFACE_EPSILON )
	{
		if(!( shader->flags & SHADER_REFRACTION ))
			return true;
	}

	// check if we are too far away and the portal view is obscured
	// by an alphagen portal stage
	for( i = 0; i < shader->numStages; i++ )
	{
		if( shader->stages[i]->alphaGen.type == ALPHAGEN_ONEMINUSFADE )
		{
			if( dist > ( 1.0f / shader->stages[i]->alphaGen.params[0] ))
				return true; // completely alpha'ed out
		}
	}

	if( OCCLUSION_QUERIES_ENABLED( Ref ))
	{
		if( !R_GetOcclusionQueryResultBool( OQ_CUSTOM, R_SurfOcclusionQueryKey( ent, surf ), true ))
			return true;
	}

	VectorAdd( ent->origin, surf->mins, mins );
	VectorAdd( ent->origin, surf->maxs, maxs );
	VectorAverage( mins, maxs, centre );

	if( r_portal_shader && ( shader != r_portal_shader ))
	{
		if( VectorDistance2( Ref.vieworg, centre ) > VectorDistance2( Ref.vieworg, r_portal_centre ))
			return true;
		VectorClear( r_portal_plane.normal );
		ClearBounds( r_portal_mins, r_portal_maxs );
	}
	r_portal_shader = shader;

	if( !Matrix3x3_Compare( ent->matrix, matrix3x3_identity ))
	{
		r_portal_ent = ent;
		r_portal_plane = plane;
		r_original_portal_plane = oplane;
		VectorCopy( surf->mins, r_portal_mins );
		VectorCopy( surf->maxs, r_portal_maxs );
		return false;
	}

	if( !VectorCompare( r_portal_plane.normal, vec3_origin ) && !( VectorCompare( plane.normal, r_portal_plane.normal )
		&& plane.dist == r_portal_plane.dist ))
	{
		if( VectorDistance2( Ref.vieworg, centre ) > VectorDistance2( Ref.vieworg, r_portal_centre ))
			return true;
		VectorClear( r_portal_plane.normal );
		ClearBounds( r_portal_mins, r_portal_maxs );
	}

	if( VectorCompare( r_portal_plane.normal, vec3_origin ) )
	{
		r_portal_plane = plane;
		r_original_portal_plane = oplane;
	}

	AddPointToBounds( mins, r_portal_mins, r_portal_maxs );
	AddPointToBounds( maxs, r_portal_mins, r_portal_maxs );
	VectorAverage( r_portal_mins, r_portal_maxs, r_portal_centre );

	return true;
}

/*
===============
R_DrawPortalSurface

Renders the portal view and captures the results from framebuffer if
we need to do a $portal stage. Note that for $portalmaps we must
use a different viewport.
Return true upon success so that we can stop rendering portals.
===============
*/
static bool R_DrawPortalSurface( void )
{
	uint		i;
	int		x, y, w, h;
	float		dist, d;
	ref_state_t	saveRef;
	vec3_t		origin, angles;
	ref_entity_t	*ent;
	cplane_t		*portal_plane = &r_portal_plane, *original_plane = &r_original_portal_plane;
	ref_shader_t		*shader = r_portal_shader;
	bool		mirror, refraction = false;
	texture_t		**captureTexture;
	int		captureTextureID;
	bool		doReflection, doRefraction;

	if( !r_portal_shader ) return false;

	doReflection = doRefraction = true;
	if( shader->flags & SHADER_REFLECTION )
	{
		shaderStage_t *stage;

		captureTexture = &r_portaltexture;
		captureTextureID = 1;

		for( i = 0; i < shader->numStages; i++ )
		{
			stage = shader->stages[i];
			if( stage->program && stage->progType == PROGRAM_DISTORTION )
			{
				if(( stage->alphaGen.type == ALPHAGEN_CONST && stage->alphaGen.params[0] == 1 ))
					doRefraction = false;
				else if( ( stage->alphaGen.type == ALPHAGEN_CONST && stage->alphaGen.params[0] == 0 ))
					doReflection = false;
				break;
			}
		}
	}
	else
	{
		captureTexture = NULL;
		captureTextureID = 0;
	}

	// copy portal plane here because we may be flipped later for refractions
	Ref.portalPlane = *portal_plane;

	if(( dist = PlaneDiff( Ref.vieworg, portal_plane )) <= BACKFACE_EPSILON || !doReflection )
	{
		if(!( shader->flags & SHADER_REFRACTION ) || !doRefraction )
			return false;

		// even if we're behind the portal, we still need to capture
		// the second portal image for refraction
		refraction = true;
		captureTexture = &r_portaltexture2;
		captureTextureID = 2;
		if( dist < 0 )
		{
			VectorNegate( portal_plane->normal, portal_plane->normal );
			portal_plane->dist = -portal_plane->dist;
		}
	}

	if( !R_ScissorForPortal( r_portal_ent, r_portal_mins, r_portal_maxs, &x, &y, &w, &h ))
		return false;

	mirror = true; // default to mirror view
	// it is stupid IMHO that mirrors require a RT_PORTALSURFACE entity

	ent = r_portal_ent;
	for( i = 1; i < r_numEntities; i++ )
	{
		ent = &r_entities[i];

		if( ent->ent_type == ED_PORTAL )
		{
			d = PlaneDiff( ent->origin, original_plane );
			if( ( d >= -64 ) && ( d <= 64 ) )
			{
				if( !VectorCompare( ent->origin, ent->infotarget )) // portal
					mirror = false;
				ent->ent_type = ED_INVALID; // will be restoring on next frame
				break;
			}
		}
	}

	if( ( i == r_numEntities ) && !captureTexture )
		return false;

setup_and_render:
	Ref.m_pPrevEntity = NULL;
	Mem_Copy( &saveRef, &oldRef, sizeof( ref_state_t ));
	Mem_Copy( &oldRef, &Ref, sizeof( ref_state_t ));

	if( refraction )
	{
		VectorNegate( portal_plane->normal, portal_plane->normal );
		portal_plane->dist = -portal_plane->dist - 1;
		PlaneClassify( portal_plane );
		VectorCopy( Ref.vieworg, origin );
		VectorCopy( Ref.refdef.viewangles, angles );

		Ref.params = RP_PORTALVIEW;
		if( r_viewCluster != -1 ) Ref.params |= RP_OLDVIEWCLUSTER;
	}
	else if( mirror )
	{
		matrix3x3	M;

		d = -2 * ( DotProduct( Ref.vieworg, portal_plane->normal ) - portal_plane->dist );
		VectorMA( Ref.vieworg, d, portal_plane->normal, origin );

		d = -2 * DotProduct( Ref.forward, portal_plane->normal );
		VectorMA( Ref.forward, d, portal_plane->normal, M[0] );
		VectorNormalize( M[0] );

		d = -2 * DotProduct( Ref.right, portal_plane->normal );
		VectorMA( Ref.right, d, portal_plane->normal, M[1] );
		VectorNormalize( M[1] );

		d = -2 * DotProduct( Ref.up, portal_plane->normal );
		VectorMA( Ref.up, d, portal_plane->normal, M[2] );
		VectorNormalize( M[2] );

		Matrix3x3_ToAngles( M, angles );
		angles[ROLL] = -angles[ROLL];

		Ref.params = RP_MIRRORVIEW|RP_FLIPFRONTFACE;
		if( r_viewCluster != -1 )
			Ref.params |= RP_OLDVIEWCLUSTER;
	}
	else
	{
		vec3_t tvec;
		matrix3x3	A, B, C, rot;

		// build world-to-portal rotation matrix
		VectorNegate( portal_plane->normal, A[0] );
		Matrix3x3_FromNormal( A[0], A );

		// build portal_dest-to-world rotation matrix
		AngleVectors( ent->infoangles, portal_plane->normal, NULL, NULL );
		Matrix3x3_FromNormal( portal_plane->normal, B );
		Matrix3x3_Transpose( B, C );

		// multiply to get world-to-world rotation matrix
		Matrix3x3_Concat( rot, C, A );

		// translate view origin
		VectorSubtract( Ref.vieworg, ent->origin, tvec );
		Matrix3x3_Transform( rot, tvec, origin );
		VectorAdd( origin, ent->infotarget, origin );

		Matrix3x3_Transform( A, Ref.forward, rot[0] );
		Matrix3x3_Transform( A, Ref.right, rot[1] );
		Matrix3x3_Transform( A, Ref.up, rot[2] );

		Matrix3x3_Concat( B, ent->matrix, rot );

		Matrix3x3_Transform( C, B[0], A[0] );
		Matrix3x3_Transform( C, B[1], A[1] );
		Matrix3x3_Transform( C, B[2], A[2] );

		// set up portal_plane
//		VectorCopy( A[0], portal_plane->normal );
		portal_plane->dist = DotProduct( ent->infotarget, portal_plane->normal );
		PlaneClassify( portal_plane );

		// calculate Euler angles for our rotation matrix
		Matrix3x3_ToAngles( A, angles );

		// for portals, vis data is taken from portal origin, not
		// view origin, because the view point moves around and
		// might fly into (or behind) a wall
		Ref.params = RP_PORTALVIEW;
		VectorCopy( ent->infotarget, Ref.pvsOrigin );
	}

	Ref.shadowGroup = NULL;
	Ref.meshlist = &r_portallist;
	Ref.surfmbuffers = r_portalSurfMbuffers;

	Ref.params |= RP_CLIPPLANE;
	Ref.clipPlane = *portal_plane;

	Ref.clipFlags |= ( 1<<5 );
	VectorNegate( portal_plane->normal, Ref.frustum[5].normal );
	Ref.frustum[5].dist = -portal_plane->dist;
	Ref.frustum[5] = *portal_plane;
	PlaneClassify( &Ref.frustum[5] );

	if( captureTexture )
	{
		// R_InitPortalTexture( captureTexture, captureTextureID, r_lastRefdef.rect.width, r_lastRefdef.rect.height );

		x = y = 0;
		w = (*captureTexture)->width;
		h = (*captureTexture)->height;
		Ref.refdef.rect.width = w;
		Ref.refdef.rect.height = h;
		Vector4Set( Ref.viewport, Ref.refdef.rect.x, Ref.refdef.rect.y, w, h );
	}

	Vector4Set( Ref.scissor, Ref.refdef.rect.x + x, Ref.refdef.rect.y + y, w, h );
	VectorCopy( origin, Ref.refdef.vieworg );
	for( i = 0; i < 3; i++ ) Ref.refdef.viewangles[i] = anglemod( angles[i] );

	// capture view from portal
	R_RenderView( &Ref.refdef );

	if(!( Ref.params & RP_OLDVIEWCLUSTER ))
		r_oldViewCluster = r_viewCluster = -1; // force markleafs next frame

	r_portalSurfMbuffers = Ref.surfmbuffers;

	Mem_Copy( &Ref, &oldRef, sizeof( ref_state_t ));
	Mem_Copy( &oldRef, &saveRef, sizeof( ref_state_t ));

	if( captureTexture )
	{
		GL_CullFace( GL_NONE );
		GL_SetState( GLSTATE_NO_DEPTH_TEST );
		pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

		// grab the results from framebuffer
		GL_SelectTexture( GL_TEXTURE0 );
		GL_BindTexture( *captureTexture );
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, Ref.refdef.rect.x, Ref.refdef.rect.y, (*captureTexture)->width, (*captureTexture)->height );
		Ref.params |= ( refraction ? RP_REFRACTED : RP_REFLECTED );
	}

	if( doRefraction && !refraction && ( shader->flags & RP_REFRACTED ))
	{
		refraction = true;
		captureTexture = &r_portaltexture2;
		captureTextureID = 2;
		goto setup_and_render;
	}

	R_AddPortalSurface( NULL );

	return true;
}

/*
===============
R_DrawSkyPortal
===============
*/
void R_DrawSkyPortal( skyportal_t *skyportal, vec3_t mins, vec3_t maxs )
{
	int		x, y, w, h;
	ref_state_t	saveRef;

	if( !R_ScissorForPortal( r_worldEntity, mins, maxs, &x, &y, &w, &h ))
		return;

	Ref.m_pPrevEntity = NULL;
	Mem_Copy( &saveRef, &oldRef, sizeof( ref_state_t ));
	Mem_Copy( &oldRef, &Ref, sizeof( ref_state_t ));

	Ref.params = ( Ref.params | RP_SKYPORTALVIEW ) & ~(RP_OLDVIEWCLUSTER|RP_REFLECTED|RP_REFRACTED);
	VectorCopy( skyportal->vieworg, Ref.pvsOrigin );

	Ref.clipFlags = 15;
	Ref.shadowGroup = NULL;
	Ref.meshlist = &r_skyportallist;
	Ref.surfmbuffers = r_skyPortalSurfMbuffers;
	Vector4Set( Ref.scissor, Ref.refdef.rect.x + x, Ref.refdef.rect.y + y, w, h );

	if( skyportal->scale )
	{
		vec3_t centre, diff;

		VectorAverage( r_worldModel->mins, r_worldModel->maxs, centre );
		VectorSubtract( centre, Ref.vieworg, diff );
		VectorMA( skyportal->vieworg, -skyportal->scale, diff, Ref.refdef.vieworg );
	}
	else
	{
		VectorCopy( skyportal->vieworg, Ref.refdef.vieworg );
	}

	VectorAdd( Ref.refdef.viewangles, skyportal->viewofs, Ref.refdef.viewangles );

	Ref.refdef.rdflags &= ~( RDF_UNDERWATER|RDF_SKYPORTALINVIEW );
	if( skyportal->fov )
	{
		Ref.refdef.fov_x = skyportal->fov;
		Ref.refdef.fov_y = ri.CalcFov( Ref.refdef.fov_x, Ref.refdef.rect.width, Ref.refdef.rect.height );
	}

	// probably is incorrect, check Xash 0.45 source
	R_RenderView( &Ref.refdef );

	r_skyPortalSurfMbuffers = Ref.surfmbuffers;
	r_oldViewCluster = r_viewCluster = -1;		// force markleafs next frame

	Mem_Copy( &Ref, &oldRef, sizeof( ref_state_t ));
	Mem_Copy( &oldRef, &saveRef, sizeof( ref_state_t ));
}

/*
===============
R_DrawCubemapView
===============
*/
void R_DrawCubemapView( vec3_t origin, vec3_t angles, int size )
{
	refdef_t	*fd;

	fd = &Ref.refdef;
	*fd = r_lastRefdef;
	fd->time = 0;
	fd->rect.x = Ref.refdef.rect.y = 0;
	fd->rect.width = size;
	fd->rect.height = size;
	fd->fov_x = 90;
	fd->fov_y = 90;
	VectorCopy( origin, fd->vieworg );
	VectorCopy( angles, fd->viewangles );

	r_numPolys = 0;
	r_numDLights = 0;

	R_RenderScene( fd );

	r_oldViewCluster = r_viewCluster = -1;		// force markleafs next frame
}

/*
===============
R_BuildTangentVectors
===============
*/
void R_BuildTangentVectors( int numVertexes, vec4_t *xyzArray, vec4_t *normalsArray, vec2_t *stArray, int numTris, uint *elems, vec4_t *sVectorsArray )
{
	int	i, j;
	float	d, *v[3], *tc[3];
	vec_t	*s, *t, *n;
	vec3_t	stvec[3], cross;
	vec3_t	stackTVectorsArray[128];
	vec3_t	*tVectorsArray;

	if( numVertexes > sizeof( stackTVectorsArray ) / sizeof( stackTVectorsArray[0] ))
		tVectorsArray = Mem_Alloc( r_temppool, sizeof( vec3_t ) * numVertexes );
	else tVectorsArray = stackTVectorsArray;

	// assuming arrays have already been allocated
	// this also does some nice precaching
	Mem_Set( sVectorsArray, 0, numVertexes * sizeof( *sVectorsArray ) );
	Mem_Set( tVectorsArray, 0, numVertexes * sizeof( *tVectorsArray ) );

	for( i = 0; i < numTris; i++, elems += 3 )
	{
		for( j = 0; j < 3; j++ )
		{
			v[j] = (float *)(xyzArray + elems[j]);
			tc[j] = (float *)(stArray + elems[j]);
		}

		// calculate two mostly perpendicular edge directions
		VectorSubtract( v[1], v[0], stvec[0] );
		VectorSubtract( v[2], v[0], stvec[1] );

		// we have two edge directions, we can calculate the normal then
		CrossProduct( stvec[1], stvec[0], cross );

		for( j = 0; j < 3; j++ )
		{
			stvec[0][j] = ((tc[1][1] - tc[0][1]) * (v[2][j] - v[0][j]) - (tc[2][1] - tc[0][1]) * (v[1][j] - v[0][j]));
			stvec[1][j] = ((tc[1][0] - tc[0][0]) * (v[2][j] - v[0][j]) - (tc[2][0] - tc[0][0]) * (v[1][j] - v[0][j]));
		}

		// inverse tangent vectors if their cross product goes in the opposite
		// direction to triangle normal
		CrossProduct( stvec[1], stvec[0], stvec[2] );
		if( DotProduct( stvec[2], cross ) < 0 )
		{
			VectorNegate( stvec[0], stvec[0] );
			VectorNegate( stvec[1], stvec[1] );
		}

		for( j = 0; j < 3; j++ )
		{
			VectorAdd( sVectorsArray[elems[j]], stvec[0], sVectorsArray[elems[j]] );
			VectorAdd( tVectorsArray[elems[j]], stvec[1], tVectorsArray[elems[j]] );
		}
	}

	// normalize
	for( i = 0, s = *sVectorsArray, t = *tVectorsArray, n = *normalsArray; i < numVertexes; i++, s += 4, t += 3, n += 4 )
	{
		// keep s\t vectors perpendicular
		d = -DotProduct( s, n );
		VectorMA( s, d, n, s );
		VectorNormalize( s );

		d = -DotProduct( t, n );
		VectorMA( t, d, n, t );

		// store polarity of t-vector in the 4-th coordinate of s-vector
		CrossProduct( n, s, cross );
		if( DotProduct( cross, t ) < 0 )
			s[3] = -1;
		else s[3] = 1;
	}

	if( tVectorsArray != stackTVectorsArray )
		Mem_Free( tVectorsArray );
}
