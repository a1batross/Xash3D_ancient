/*
Copyright (C) 2002-2007 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// r_mesh.c: transformation and sorting

#include "r_local.h"
#include "mathlib.h"
#include "quatlib.h"

#define	    QSORT_MAX_STACKDEPTH    2048

static byte *r_meshlistmempool;

meshlist_t r_worldlist, r_shadowlist;
static meshlist_t r_portallist, r_skyportallist;

static meshbuffer_t **r_portalSurfMbuffers;
static meshbuffer_t **r_skyPortalSurfMbuffers;

static void R_QSortMeshBuffers( meshbuffer_t *meshes, int Li, int Ri );
static void R_ISortMeshBuffers( meshbuffer_t *meshes, int num_meshes );

static bool R_AddPortalSurface( const meshbuffer_t *mb );
static bool R_DrawPortalSurface( void );

#define R_MBCopy( in, out ) \
	( \
	( out ).sortkey = ( in ).sortkey, \
	( out ).infokey = ( in ).infokey, \
	( out ).dlightbits = ( in ).dlightbits, \
	( out ).shaderkey = ( in ).shaderkey, \
	( out ).shadowbits = ( in ).shadowbits \
	)

#define R_MBCmp( mb1, mb2 ) \
	( \
	( mb1 ).shaderkey > ( mb2 ).shaderkey ? true : \
	( mb1 ).shaderkey < ( mb2 ).shaderkey ? false : \
	( mb1 ).sortkey > ( mb2 ).sortkey ? true : \
	( mb1 ).sortkey < ( mb2 ).sortkey ? false : \
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
	int li, ri, stackdepth = 0, total = Ri + 1;
	meshbuffer_t median, tempbuf;
	int lstack[QSORT_MAX_STACKDEPTH], rstack[QSORT_MAX_STACKDEPTH];

mark0:
	if( Ri - Li > 8 )
	{
		li = Li;
		ri = Ri;

		R_MBCopy( meshes[( Li+Ri ) >> 1], median );

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
			while( R_MBCmp( median, meshes[li] ) ) li++;
			while( R_MBCmp( meshes[ri], median ) ) ri--;

			if( li <= ri )
			{
				R_MBCopy( meshes[ri], tempbuf );
				R_MBCopy( meshes[li], meshes[ri] );
				R_MBCopy( tempbuf, meshes[li] );

				li++;
				ri--;
			}
		}
		while( li < ri );

		if( ( Li < ri ) && ( stackdepth < QSORT_MAX_STACKDEPTH ) )
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

		while( ( ri >= 0 ) && ( R_MBCmp( meshes[ri], tempbuf ) ) )
		{
			R_MBCopy( meshes[ri], meshes[ri+1] );
			ri--;
		}
		if( li != ri+1 )
			R_MBCopy( tempbuf, meshes[ri+1] );
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
	int i, j;
	meshbuffer_t tempbuf;

	for( i = 1; i < num_meshes; i++ )
	{
		R_MBCopy( meshes[i], tempbuf );
		j = i - 1;

		while( ( j >= 0 ) && ( R_MBCmp( meshes[j], tempbuf ) ) )
		{
			R_MBCopy( meshes[j], meshes[j+1] );
			j--;
		}
		if( i != j+1 )
			R_MBCopy( tempbuf, meshes[j+1] );
	}
}

/*
=======================================================================

VERTEX BUFFERS

=======================================================================
*/
/*
=================
R_UpdateVertexBuffer
=================
*/
void R_UpdateVertexBuffer( ref_buffer_t *vertexBuffer, const void *data, size_t size )
{
	if( !GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
	{
		vertexBuffer->pointer = (char *)data;
		return;
	}

	if( !r_vertexbuffers->integer )
	{
		vertexBuffer->pointer = (char *)data;
		pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
		return;
	}

	vertexBuffer->pointer = NULL;
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, vertexBuffer->bufNum );
	pglBufferSubDataARB( GL_ARRAY_BUFFER_ARB, 0, size, data );
}

/*
=================
R_AllocVertexBuffer
=================
*/
ref_buffer_t *R_AllocVertexBuffer( size_t size, GLuint usage )
{
	ref_buffer_t	*vertexBuffer;

	if( tr.numVertexBufferObjects == MAX_VERTEX_BUFFER_OBJECTS )
		Host_Error( "RB_AllocVertexBuffer: MAX_VERTEX_BUFFER_OBJECTS limit exceeds\n" );

	vertexBuffer = &tr.vertexBufferObjects[tr.numVertexBufferObjects++];

	vertexBuffer->pointer = NULL;
	vertexBuffer->size = size;
	vertexBuffer->usage = usage;

	if(!GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
		return vertexBuffer;

	pglGenBuffersARB( 1, &vertexBuffer->bufNum );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, vertexBuffer->bufNum );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, vertexBuffer->size, NULL, vertexBuffer->usage );

	return vertexBuffer;
}

/*
=================
R_InitVertexBuffers
=================
*/
void R_InitVertexBuffers( void )
{
	int	i;

	tr.vertexBuffer = R_AllocVertexBuffer( MAX_ARRAY_VERTS * sizeof( vec4_t ), GL_STREAM_DRAW_ARB );
	tr.colorsBuffer = R_AllocVertexBuffer( MAX_ARRAY_VERTS * sizeof( rgba_t ), GL_STREAM_DRAW_ARB );
	tr.normalBuffer = R_AllocVertexBuffer( MAX_ARRAY_VERTS * sizeof( vec4_t ), GL_STREAM_DRAW_ARB );
	for( i = 0; i < MAX_TEXTURE_UNITS; i++ )
		tr.tcoordBuffer[i] = R_AllocVertexBuffer( MAX_ARRAY_VERTS * sizeof( vec4_t ), GL_STREAM_DRAW_ARB );	
}

/*
=================
R_ShutdownVertexBuffers
=================
*/
void R_ShutdownVertexBuffers( void )
{
	ref_buffer_t	*vertexBuffer;
	int		i;

	if( !GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
	{
		Mem_Set( tr.vertexBufferObjects, 0, sizeof( tr.vertexBufferObjects ));
		tr.numVertexBufferObjects = 0;
		return;
	}

	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	for( i = 0, vertexBuffer = tr.vertexBufferObjects; i < tr.numVertexBufferObjects; i++, vertexBuffer++ )
		pglDeleteBuffersARB( 1, &vertexBuffer->bufNum );

	Mem_Set( tr.vertexBufferObjects, 0, sizeof( tr.vertexBufferObjects ));
	tr.numVertexBufferObjects = 0;
} 

/*
=================
R_ReAllocMeshList
=================
*/
int R_ReAllocMeshList( meshbuffer_t **mb, int minMeshes, int maxMeshes )
{
	int oldSize, newSize;
	meshbuffer_t *newMB;

	oldSize = maxMeshes;
	newSize = max( minMeshes, oldSize * 2 );

	newMB = Mem_Alloc( r_meshlistmempool, newSize * sizeof( meshbuffer_t ));
	if( *mb )
	{
		Mem_Copy( newMB, *mb, oldSize * sizeof( meshbuffer_t ) );
		Mem_Free( *mb );
	}
	*mb = newMB;

	// NULL all pointers to old membuffers so we don't crash
	if( r_worldmodel && !( RI.refdef.rdflags & RDF_NOWORLDMODEL ) )
		memset( RI.surfmbuffers, 0, r_worldbrushmodel->numsurfaces * sizeof( meshbuffer_t * ) );

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
	meshlist_t *list;
	meshbuffer_t *meshbuf;

	if( !shader )
		return NULL;

	list = RI.meshlist;
	if( shader->sort > SORT_OPAQUE )
	{
		if( list->num_translucent_meshes >= list->max_translucent_meshes )	// reallocate if needed
			list->max_translucent_meshes = R_ReAllocMeshList( &list->meshbuffer_translucent, MIN_RENDER_MESHES/2, list->max_translucent_meshes );

		if( shader->flags & SHADER_PORTAL )
		{
			if( RI.params & ( RP_MIRRORVIEW|RP_PORTALVIEW|RP_SKYPORTALVIEW ) )
				return NULL;
			RI.meshlist->num_portal_translucent_meshes++;
		}

		meshbuf = &list->meshbuffer_translucent[list->num_translucent_meshes++];
	}
	else
	{
		if( list->num_opaque_meshes >= list->max_opaque_meshes )	// reallocate if needed
			list->max_opaque_meshes = R_ReAllocMeshList( &list->meshbuffer_opaque, MIN_RENDER_MESHES, list->max_opaque_meshes );

		if( shader->flags & SHADER_PORTAL )
		{
			if( RI.params & ( RP_MIRRORVIEW|RP_PORTALVIEW|RP_SKYPORTALVIEW ) )
				return NULL;
			RI.meshlist->num_portal_opaque_meshes++;
		}

		meshbuf = &list->meshbuffer_opaque[list->num_opaque_meshes++];
	}

	if( shader->flags & SHADER_VIDEOMAP )
		R_UploadCinematicShader( shader );

	meshbuf->sortkey = MB_ENTITY2NUM( RI.currententity ) | MB_FOG2NUM( fog ) | type;
	meshbuf->shaderkey = shader->sortkey;
	meshbuf->infokey = infokey;
	meshbuf->dlightbits = 0;
	meshbuf->shadowbits = r_entShadowBits[RI.currententity - r_entities];

	return meshbuf;
}

/*
=================
R_AddMeshToList
=================
*/
void R_AddModelMeshToList( unsigned int modhandle, mfog_t *fog, ref_shader_t *shader, int meshnum )
{
	meshbuffer_t *mb;
	
	mb = R_AddMeshToList( MB_MODEL, fog, shader, -( meshnum+1 ) );
	if( mb )
		mb->LODModelHandle = modhandle;

	if( !GL_Support( R_SHADER_GLSL100_EXT ) && RI.currententity->outlineHeight/* && !(RI.params & RP_SHADOWMAPVIEW)*/ )
	{
		if(( shader->sort == SORT_OPAQUE ) && ( shader->flags & SHADER_CULL_FRONT ))
			R_AddModelMeshOutline( modhandle, fog, meshnum );
	}
}

/*
================
R_BatchMeshBuffer

Draw the mesh or batch it.
================
*/
static void R_BatchMeshBuffer( const meshbuffer_t *mb, const meshbuffer_t *nextmb )
{
	int type, features;
	bool nonMergable;
	ref_entity_t *ent;
	ref_shader_t *shader;
	msurface_t *surf, *nextSurf;

	MB_NUM2ENTITY( mb->sortkey, ent );

	if( RI.currententity != ent )
	{
		RI.previousentity = RI.currententity;
		RI.currententity = ent;
		RI.currentmodel = ent->model;
	}

	type = mb->sortkey & 3;
	tr.iRenderMode = RI.currententity->rendermode;

	switch( type )
	{
	case MB_MODEL:
		switch( ent->model->type )
		{
		case mod_world:
		case mod_brush:
			MB_NUM2SHADER( mb->shaderkey, shader );

			if( shader->flags & SHADER_SKYPARMS )
			{	// draw sky
				if( !( RI.params & RP_NOSKY ) )
					R_DrawSky( shader );
				return;
			}

			surf = &r_worldbrushmodel->surfaces[mb->infokey-1];
			nextSurf = NULL;

			features = shader->features;
			if( r_shownormals->integer )
				features |= MF_NORMALS;
			if( ent->outlineHeight )
				features |= (MF_NORMALS|MF_ENABLENORMALS);
			features |= r_superLightStyles[surf->superLightStyle].features;

			if( features & MF_NONBATCHED )
			{
				nonMergable = true;
			}
			else
			{	// check if we need to render batched geometry this frame
				if( nextmb
					&& ( nextmb->shaderkey == mb->shaderkey )
					&& ( nextmb->sortkey == mb->sortkey )
					&& ( nextmb->dlightbits == mb->dlightbits )
					&& ( nextmb->shadowbits == mb->shadowbits ) )
				{
					if( nextmb->infokey > 0 )
						nextSurf = &r_worldbrushmodel->surfaces[nextmb->infokey-1];
				}

				nonMergable = nextSurf ? R_MeshOverflow2( surf->mesh, nextSurf->mesh ) : true;
				if( nonMergable && !r_backacc.numVerts )
					features |= MF_NONBATCHED;
			}

			R_PushMesh( surf->mesh, features );

			if( nonMergable )
			{
				if( RI.previousentity != RI.currententity )
					R_RotateForEntity( RI.currententity );
				R_RenderMeshBuffer( mb );
			}
			break;
		case mod_alias:
			R_DrawAliasModel( mb );
			break;
		case mod_sprite:
			R_PushSpriteModel( mb );

			// no rotation for sprites
			R_TranslateForEntity( RI.currententity );
			R_RenderMeshBuffer( mb );
			break;
		case mod_studio:
			break;
		default:
			Com_Assert( 1 );    // shut up compiler
			break;
		}
		break;
	case MB_SPRITE:
	case MB_CORONA:
		nonMergable = R_PushSpritePoly( mb );
		if( nonMergable || !nextmb || (( nextmb->shaderkey & 0xFC000FFF ) != ( mb->shaderkey & 0xFC000FFF ))
		|| (( nextmb->sortkey & 0xFFFFF ) != ( mb->sortkey & 0xFFFFF )) || R_SpriteOverflow() )
		{
			if( !nonMergable )
			{
				RI.currententity = r_worldent;
				RI.currentmodel = r_worldmodel;
			}

			// no rotation for sprites
			if( RI.previousentity != RI.currententity )
				R_TranslateForEntity( RI.currententity );
			R_RenderMeshBuffer( mb );
		}
		break;
	case MB_POLY:
		// polys are already batched at this point
		R_PushPoly( mb );

		if( RI.previousentity != RI.currententity )
			R_LoadIdentity();
		R_RenderMeshBuffer( mb );
		break;
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
	if( r_draworder->integer )
		return;

	if( RI.meshlist->num_opaque_meshes )
		R_QSortMeshBuffers( RI.meshlist->meshbuffer_opaque, 0, RI.meshlist->num_opaque_meshes - 1 );
	if( RI.meshlist->num_translucent_meshes )
		R_ISortMeshBuffers( RI.meshlist->meshbuffer_translucent, RI.meshlist->num_translucent_meshes );
}

/*
================
R_DrawPortals

Render portal views. For regular portals we stop after rendering the
first valid portal view.
Skyportal views are rendered afterwards.
================
*/
void R_DrawPortals( void )
{
	int i;
	int trynum, num_meshes, total_meshes;
	meshbuffer_t *mb;
	ref_shader_t *shader;

	if( r_viewcluster == -1 )
		return;

	if( !( RI.params & ( RP_MIRRORVIEW|RP_PORTALVIEW|RP_SHADOWMAPVIEW ) ) )
	{
		if( RI.meshlist->num_portal_opaque_meshes || RI.meshlist->num_portal_translucent_meshes )
		{
			trynum = 0;

			R_AddPortalSurface( NULL );

			do
			{
				switch( trynum )
				{
				case 0:
					mb = RI.meshlist->meshbuffer_opaque;
					total_meshes = RI.meshlist->num_opaque_meshes;
					num_meshes = RI.meshlist->num_portal_opaque_meshes;
					break;
				case 1:
					mb = RI.meshlist->meshbuffer_translucent;
					total_meshes = RI.meshlist->num_translucent_meshes;
					num_meshes = RI.meshlist->num_portal_translucent_meshes;
					break;
				default:
					mb = NULL;
					total_meshes = num_meshes = 0;
					Com_Assert( 1 );
					break;
				}

				for( i = 0; i < total_meshes && num_meshes; i++, mb++ )
				{
					MB_NUM2SHADER( mb->shaderkey, shader );

					if( shader->flags & SHADER_PORTAL )
					{
						num_meshes--;

						if( r_fastsky->integer && !( shader->flags & SHADER_PORTAL_CAPTURE ) )
							continue;

						if( !R_AddPortalSurface( mb ) )
						{
							if( R_DrawPortalSurface() )
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

	if( ( RI.refdef.rdflags & RDF_SKYPORTALINVIEW ) && !( RI.params & RP_NOSKY ) && !r_fastsky->integer )
	{
		for( i = 0, mb = RI.meshlist->meshbuffer_opaque; i < RI.meshlist->num_opaque_meshes; i++, mb++ )
		{
			MB_NUM2SHADER( mb->shaderkey, shader );

			if( shader->flags & SHADER_SKYPARMS )
			{
				R_DrawSky( shader );
				RI.params |= RP_NOSKY;
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
	int i;
	meshbuffer_t *meshbuf;

	RI.previousentity = NULL;
	if( RI.meshlist->num_opaque_meshes )
	{
		meshbuf = RI.meshlist->meshbuffer_opaque;
		for( i = 0; i < RI.meshlist->num_opaque_meshes - 1; i++, meshbuf++ )
			R_BatchMeshBuffer( meshbuf, meshbuf+1 );
		R_BatchMeshBuffer( meshbuf, NULL );
	}

	if( RI.meshlist->num_translucent_meshes )
	{
		meshbuf = RI.meshlist->meshbuffer_translucent;
		for( i = 0; i < RI.meshlist->num_translucent_meshes - 1; i++, meshbuf++ )
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
	if( !r_meshlistmempool )
		r_meshlistmempool = Mem_AllocPool( "MeshList" );
}

/*
===============
R_AllocMeshbufPointers
===============
*/
void R_AllocMeshbufPointers( refinst_t *RI )
{
	Com_Assert( r_worldmodel == NULL );
	if( !RI->surfmbuffers )
		RI->surfmbuffers = Mem_Alloc( r_worldmodel->mempool, r_worldbrushmodel->numsurfaces * sizeof( meshbuffer_t* ));
}

/*
===============
R_FreeMeshLists
===============
*/
void R_FreeMeshLists( void )
{
	if( !r_meshlistmempool )
		return;

	r_portalSurfMbuffers = NULL;
	r_skyPortalSurfMbuffers = NULL;

	Mem_FreePool( &r_meshlistmempool );

	memset( &r_worldlist, 0, sizeof( meshlist_t ) );
	memset( &r_shadowlist, 0, sizeof( meshlist_t ) );
	memset( &r_portallist, 0, sizeof( meshlist_t ) );
	memset( &r_skyportallist, 0, sizeof( r_skyportallist ) );
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
R_DrawTriangleOutlines
===============
*/
void R_DrawTriangleOutlines( bool showTris, bool showNormals )
{
	if( !showTris && !showNormals )
		return;

	RI.params |= (showTris ? RP_TRISOUTLINES : 0) | (showNormals ? RP_SHOWNORMALS : 0);
	R_BackendBeginTriangleOutlines();
	R_DrawMeshes();
	R_BackendEndTriangleOutlines();
	RI.params &= ~(RP_TRISOUTLINES|RP_SHOWNORMALS);
}

/*
===============
R_ScissorForPortal

Returns the on-screen scissor box for given bounding box in 3D-space.
===============
*/
bool R_ScissorForPortal( ref_entity_t *ent, vec3_t mins, vec3_t maxs, int *x, int *y, int *w, int *h )
{
	int i;
	int ix1, iy1, ix2, iy2;
	float x1, y1, x2, y2;
	vec3_t v, bbox[8];

	R_TransformEntityBBox( ent, mins, maxs, bbox, true );

	x1 = y1 = 999999;
	x2 = y2 = -999999;
	for( i = 0; i < 8; i++ )
	{                       // compute and rotate a full bounding box
		vec_t *corner = bbox[i];
		R_TransformToScreen_Vec3( corner, v );

		if( v[2] < 0 || v[2] > 1 )
		{                    // the test point is behind the nearclip plane
			if( PlaneDiff( corner, &RI.frustum[0] ) < PlaneDiff( corner, &RI.frustum[1] ) )
				v[0] = 0;
			else
				v[0] = RI.refdef.viewport[2];
			if( PlaneDiff( corner, &RI.frustum[2] ) < PlaneDiff( corner, &RI.frustum[3] ) )
				v[1] = 0;
			else
				v[1] = RI.refdef.viewport[3];
		}

		x1 = min( x1, v[0] ); y1 = min( y1, v[1] );
		x2 = max( x2, v[0] ); y2 = max( y2, v[1] );
	}

	ix1 = max( x1 - 1.0f, 0 ); ix2 = min( x2 + 1.0f, RI.refdef.viewport[2] );
	if( ix1 >= ix2 )
		return false; // FIXME

	iy1 = max( y1 - 1.0f, 0 ); iy2 = min( y2 + 1.0f, RI.refdef.viewport[3] );
	if( iy1 >= iy2 )
		return false; // FIXME

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
	int i;
	float dist;
	ref_entity_t *ent;
	ref_shader_t *shader;
	msurface_t *surf;
	cplane_t plane, oplane;
	mesh_t *mesh;
	vec3_t mins, maxs, centre;
	vec3_t v[3], entity_rotation[3];

	if( !mb )
	{
		r_portal_ent = r_worldent;
		r_portal_shader = NULL;
		VectorClear( r_portal_plane.normal );
		ClearBounds( r_portal_mins, r_portal_maxs );
		return false;
	}

	MB_NUM2ENTITY( mb->sortkey, ent );
	if( !ent->model )
		return false;

	surf = mb->infokey > 0 ? &r_worldbrushmodel->surfaces[mb->infokey-1] : NULL;
	if( !surf || !( mesh = surf->mesh ) || !mesh->xyzArray )
		return false;

	MB_NUM2SHADER( mb->shaderkey, shader );

	VectorCopy( mesh->xyzArray[mesh->elems[0]], v[0] );
	VectorCopy( mesh->xyzArray[mesh->elems[1]], v[1] );
	VectorCopy( mesh->xyzArray[mesh->elems[2]], v[2] );
	PlaneFromPoints( v, &oplane );
	oplane.dist += DotProduct( ent->origin, oplane.normal );
	CategorizePlane( &oplane );

	if( !Matrix_Compare( ent->axis, axis_identity ))
	{
		Matrix_Transpose( ent->axis, entity_rotation );
		Matrix_TransformVector( entity_rotation, mesh->xyzArray[mesh->elems[0]], v[0] );
		VectorMA( ent->origin, ent->scale, v[0], v[0] );
		Matrix_TransformVector( entity_rotation, mesh->xyzArray[mesh->elems[1]], v[1] );
		VectorMA( ent->origin, ent->scale, v[1], v[1] );
		Matrix_TransformVector( entity_rotation, mesh->xyzArray[mesh->elems[2]], v[2] );
		VectorMA( ent->origin, ent->scale, v[2], v[2] );
		PlaneFromPoints( v, &plane );
		CategorizePlane( &plane );
	}
	else
	{
		plane = oplane;
	}

	if( ( dist = PlaneDiff( RI.viewOrigin, &plane ) ) <= BACKFACE_EPSILON )
	{
		if( !( shader->flags & SHADER_PORTAL_CAPTURE2 ))
			return true;
	}

	// check if we are too far away and the portal view is obscured
	// by an alphagen portal stage
	for( i = 0; i < shader->num_stages; i++ )
	{
		if( shader->stages[i].alphaGen.type == ALPHAGEN_PORTAL )
		{
			if( dist > ( 1.0f / shader->stages[i].alphaGen.args[0] ) )
				return true; // completely alpha'ed out
		}
	}

	if( OCCLUSION_QUERIES_ENABLED( RI ) )
	{
		if( !R_GetOcclusionQueryResultBool( OQ_CUSTOM, R_SurfOcclusionQueryKey( ent, surf ), true ) )
			return true;
	}

	VectorAdd( ent->origin, surf->mins, mins );
	VectorAdd( ent->origin, surf->maxs, maxs );
	VectorAdd( mins, maxs, centre );
	VectorScale( centre, 0.5, centre );

	if( r_portal_shader && ( shader != r_portal_shader ))
	{
		if( VectorDistance2( RI.viewOrigin, centre ) > VectorDistance2( RI.viewOrigin, r_portal_centre ))
			return true;
		VectorClear( r_portal_plane.normal );
		ClearBounds( r_portal_mins, r_portal_maxs );
	}
	r_portal_shader = shader;

	if( !Matrix_Compare( ent->axis, axis_identity ) )
	{
		r_portal_ent = ent;
		r_portal_plane = plane;
		r_original_portal_plane = oplane;
		VectorCopy( surf->mins, r_portal_mins );
		VectorCopy( surf->maxs, r_portal_maxs );
		return false;
	}

	if( !VectorCompare( r_portal_plane.normal, vec3_origin ) && !( VectorCompare( plane.normal, r_portal_plane.normal ) && plane.dist == r_portal_plane.dist ) )
	{
		if( VectorDistance2( RI.viewOrigin, centre ) > VectorDistance2( RI.viewOrigin, r_portal_centre ) )
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
	VectorAdd( r_portal_mins, r_portal_maxs, r_portal_centre );
	VectorScale( r_portal_centre, 0.5, r_portal_centre );

	return true;
}

/*
===============
R_DrawPortalSurface

Renders the portal view and captures the results from framebuffer if
we need to do a $portalmap stage. Note that for $portalmaps we must
use a different viewport.
Return true upon success so that we can stop rendering portals.
===============
*/
static bool R_DrawPortalSurface( void )
{
	unsigned int i;
	int x, y, w, h;
	float dist, d;
	refinst_t oldRI;
	vec3_t origin, angles;
	ref_entity_t *ent;
	cplane_t *portal_plane = &r_portal_plane, *original_plane = &r_original_portal_plane;
	ref_shader_t *shader = r_portal_shader;
	bool mirror, refraction = false;
	texture_t **captureTexture;
	int captureTextureID;
	bool doReflection, doRefraction;

	if( !r_portal_shader )
		return false;

	doReflection = doRefraction = true;
	if( shader->flags & SHADER_PORTAL_CAPTURE1 )
	{
		ref_stage_t *pass;

		captureTexture = &tr.portaltexture1;
		captureTextureID = 1;

		for( i = 0, pass = shader->stages; i < shader->num_stages; i++, pass++ )
		{
			if( pass->program && pass->program_type == PROGRAM_TYPE_DISTORTION )
			{
				if( ( pass->alphaGen.type == ALPHAGEN_CONST && pass->alphaGen.args[0] == 1 ) )
					doRefraction = false;
				else if( ( pass->alphaGen.type == ALPHAGEN_CONST && pass->alphaGen.args[0] == 0 ) )
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
	RI.portalPlane = *portal_plane;

	if( ( dist = PlaneDiff( RI.viewOrigin, portal_plane ) ) <= BACKFACE_EPSILON || !doReflection )
	{
		if( !( shader->flags & SHADER_PORTAL_CAPTURE2 ) || !doRefraction )
			return false;

		// even if we're behind the portal, we still need to capture
		// the second portal image for refraction
		refraction = true;
		captureTexture = &tr.portaltexture2;
		captureTextureID = 2;
		if( dist < 0 )
		{
			VectorNegate( portal_plane->normal, portal_plane->normal );
			portal_plane->dist = -portal_plane->dist;
		}
	}

	if( !R_ScissorForPortal( r_portal_ent, r_portal_mins, r_portal_maxs, &x, &y, &w, &h ) )
		return false;

	mirror = true; // default to mirror view
	// it is stupid IMO that mirrors require a RT_PORTALSURFACE entity

	ent = r_portal_ent;
	for( i = 1; i < r_numEntities; i++ )
	{
		ent = &r_entities[i];

		if( ent->rtype == RT_PORTALSURFACE )
		{
			d = PlaneDiff( ent->origin, original_plane );
			if( ( d >= -64 ) && ( d <= 64 ) )
			{
				if( !VectorCompare( ent->origin, ent->origin2 ) )	// portal
					mirror = false;
				ent->rtype = -1;
				break;
			}
		}
	}

	if( ( i == r_numEntities ) && !captureTexture )
		return false;

setup_and_render:
	RI.previousentity = NULL;
	memcpy( &oldRI, &prevRI, sizeof( refinst_t ) );
	memcpy( &prevRI, &RI, sizeof( refinst_t ) );

	if( refraction )
	{
		VectorNegate( portal_plane->normal, portal_plane->normal );
		portal_plane->dist = -portal_plane->dist - 1;
		CategorizePlane( portal_plane );
		VectorCopy( RI.viewOrigin, origin );
		VectorCopy( RI.refdef.viewangles, angles );

		RI.params = RP_PORTALVIEW;
		if( r_viewcluster != -1 )
			RI.params |= RP_OLDVIEWCLUSTER;
	}
	else if( mirror )
	{
		vec3_t M[3];

		d = -2 * ( DotProduct( RI.viewOrigin, portal_plane->normal ) - portal_plane->dist );
		VectorMA( RI.viewOrigin, d, portal_plane->normal, origin );

		for( i = 0; i < 3; i++ )
		{
			d = -2 * DotProduct( RI.viewAxis[i], portal_plane->normal );
			VectorMA( RI.viewAxis[i], d, portal_plane->normal, M[i] );
			VectorNormalize( M[i] );
		}

		Matrix_EulerAngles( M, angles );
		angles[ROLL] = -angles[ROLL];

		RI.params = RP_MIRRORVIEW|RP_FLIPFRONTFACE;
		if( r_viewcluster != -1 )
			RI.params |= RP_OLDVIEWCLUSTER;
	}
	else
	{
		vec3_t tvec;
		vec3_t A[3], B[3], C[3], rot[3];

		// build world-to-portal rotation matrix
		VectorNegate( portal_plane->normal, A[0] );
		NormalVectorToAxis( A[0], A );

		// build portal_dest-to-world rotation matrix
		VectorCopy( ent->movedir, portal_plane->normal );
		NormalVectorToAxis( portal_plane->normal, B );
		Matrix_Transpose( B, C );

		// multiply to get world-to-world rotation matrix
		Matrix_Multiply( C, A, rot );

		// translate view origin
		VectorSubtract( RI.viewOrigin, ent->origin, tvec );
		Matrix_TransformVector( rot, tvec, origin );
		VectorAdd( origin, ent->origin2, origin );

		for( i = 0; i < 3; i++ )
			Matrix_TransformVector( A, RI.viewAxis[i], rot[i] );
		Matrix_Multiply( ent->axis, rot, B );
		for( i = 0; i < 3; i++ )
			Matrix_TransformVector( C, B[i], A[i] );

		// set up portal_plane
//		VectorCopy( A[0], portal_plane->normal );
		portal_plane->dist = DotProduct( ent->origin2, portal_plane->normal );
		CategorizePlane( portal_plane );

		// calculate Euler angles for our rotation matrix
		Matrix_EulerAngles( A, angles );

		// for portals, vis data is taken from portal origin, not
		// view origin, because the view point moves around and
		// might fly into (or behind) a wall
		RI.params = RP_PORTALVIEW;
		VectorCopy( ent->origin2, RI.pvsOrigin );
		VectorCopy( ent->origin2, RI.lodOrigin );
	}

	RI.shadowGroup = NULL;
	RI.meshlist = &r_portallist;
	RI.surfmbuffers = r_portalSurfMbuffers;

	RI.params |= RP_CLIPPLANE;
	RI.clipPlane = *portal_plane;

	RI.clipFlags |= ( 1<<5 );
	VectorNegate( portal_plane->normal, RI.frustum[5].normal );
	RI.frustum[5].dist = -portal_plane->dist;
	RI.frustum[5] = *portal_plane;
	CategorizePlane( &RI.frustum[5] );

	if( captureTexture )
	{
		R_InitPortalTexture( captureTexture, captureTextureID, r_lastRefdef.viewport[2], r_lastRefdef.viewport[3] );

		x = y = 0;
		w = ( *captureTexture )->width;
		h = ( *captureTexture )->height;
		RI.refdef.viewport[2] = w;
		RI.refdef.viewport[3] = h;
		Vector4Set( RI.viewport, RI.refdef.viewport[0], RI.refdef.viewport[1], w, h );
	}

	Vector4Set( RI.scissor, RI.refdef.viewport[0] + x, RI.refdef.viewport[1] + y, w, h );
	VectorCopy( origin, RI.refdef.vieworg );
	for( i = 0; i < 3; i++ )
		RI.refdef.viewangles[i] = anglemod( angles[i] );

	R_RenderView( &RI.refdef );

	if( !( RI.params & RP_OLDVIEWCLUSTER ) )
		r_oldviewcluster = r_viewcluster = -1; // force markleafs next frame

	r_portalSurfMbuffers = RI.surfmbuffers;

	memcpy( &RI, &prevRI, sizeof( refinst_t ) );
	memcpy( &prevRI, &oldRI, sizeof( refinst_t ) );

	if( captureTexture )
	{
		GL_Cull( 0 );
		GL_SetState( GLSTATE_NO_DEPTH_TEST );
		pglColor4f( 1, 1, 1, 1 );

		// grab the results from framebuffer
		GL_Bind( 0, *captureTexture );
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, RI.refdef.viewport[0], RI.refdef.viewport[1], ( *captureTexture )->width, ( *captureTexture )->height );
		RI.params |= ( refraction ? RP_PORTALCAPTURED2 : RP_PORTALCAPTURED );
	}

	if( doRefraction && !refraction && ( shader->flags & SHADER_PORTAL_CAPTURE2 ) )
	{
		refraction = true;
		captureTexture = &tr.portaltexture2;
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
	int x, y, w, h;
	refinst_t oldRI;

	if( !R_ScissorForPortal( r_worldent, mins, maxs, &x, &y, &w, &h ) )
		return;

	RI.previousentity = NULL;
	memcpy( &oldRI, &prevRI, sizeof( refinst_t ) );
	memcpy( &prevRI, &RI, sizeof( refinst_t ) );

	RI.params = ( RI.params|RP_SKYPORTALVIEW ) & ~( RP_OLDVIEWCLUSTER|RP_PORTALCAPTURED|RP_PORTALCAPTURED2 );
	VectorCopy( skyportal->vieworg, RI.pvsOrigin );

	RI.clipFlags = 15;
	RI.shadowGroup = NULL;
	RI.meshlist = &r_skyportallist;
	RI.surfmbuffers = r_skyPortalSurfMbuffers;
	Vector4Set( RI.scissor, RI.refdef.viewport[0] + x, RI.refdef.viewport[1] + y, w, h );
//	Vector4Set( RI.viewport, RI.refdef.viewport[0], glState.height - RI.refdef.viewport[3] - RI.refdef.viewport[1], RI.refdef.viewport[2], RI.refdef.viewport[3] );

	if( skyportal->scale )
	{
		vec3_t centre, diff;

		VectorAdd( r_worldmodel->mins, r_worldmodel->maxs, centre );
		VectorScale( centre, 0.5f, centre );
		VectorSubtract( centre, RI.viewOrigin, diff );
		VectorMA( skyportal->vieworg, -skyportal->scale, diff, RI.refdef.vieworg );
	}
	else
	{
		VectorCopy( skyportal->vieworg, RI.refdef.vieworg );
	}

	VectorAdd( RI.refdef.viewangles, skyportal->viewanglesOffset, RI.refdef.viewangles );

	RI.refdef.rdflags &= ~RDF_SKYPORTALINVIEW;
	if( skyportal->fov )
	{
		RI.refdef.fov_x = skyportal->fov;
		RI.refdef.fov_y = CalcFov( RI.refdef.fov_x, RI.refdef.viewport[2], RI.refdef.viewport[3] );
		if( glState.wideScreen && !( RI.refdef.rdflags & RDF_NOFOVADJUSTMENT ) )
			AdjustFov( &RI.refdef.fov_x, &RI.refdef.fov_y, glState.width, glState.height, false );
	}

	R_RenderView( &RI.refdef );

	r_skyPortalSurfMbuffers = RI.surfmbuffers;
	r_oldviewcluster = r_viewcluster = -1;		// force markleafs next frame

	memcpy( &RI, &prevRI, sizeof( refinst_t ) );
	memcpy( &prevRI, &oldRI, sizeof( refinst_t ) );
}

/*
===============
R_DrawCubemapView
===============
*/
void R_DrawCubemapView( const vec3_t origin, const vec3_t angles, int size )
{
	ref_params_t *fd;

	fd = &RI.refdef;
	*fd = r_lastRefdef;
	fd->time = 0;
	fd->viewport[0] = RI.refdef.viewport[1] = 0;
	fd->viewport[2] = size;
	fd->viewport[3] = size;
	fd->fov_x = 90;
	fd->fov_y = 90;
	VectorCopy( origin, fd->vieworg );
	VectorCopy( angles, fd->viewangles );

	r_numPolys = 0;
	r_numDlights = 0;

	R_RenderScene( fd );

	r_oldviewcluster = r_viewcluster = -1;		// force markleafs next frame
}

/*
===============
R_BuildTangentVectors
===============
*/
void R_BuildTangentVectors( int numVertexes, vec4_t *xyzArray, vec4_t *normalsArray, vec2_t *stArray, int numTris, elem_t *elems, vec4_t *sVectorsArray )
{
	int i, j;
	float d, *v[3], *tc[3];
	vec_t *s, *t, *n;
	vec3_t stvec[3], cross;
	vec3_t stackTVectorsArray[128];
	vec3_t *tVectorsArray;

	if( numVertexes > sizeof( stackTVectorsArray )/sizeof( stackTVectorsArray[0] ) )
		tVectorsArray = Mem_Alloc( r_temppool, sizeof( vec3_t ) * numVertexes );
	else
		tVectorsArray = stackTVectorsArray;

	// assuming arrays have already been allocated
	// this also does some nice precaching
	memset( sVectorsArray, 0, numVertexes * sizeof( *sVectorsArray ) );
	memset( tVectorsArray, 0, numVertexes * sizeof( *tVectorsArray ) );

	for( i = 0; i < numTris; i++, elems += 3 )
	{
		for( j = 0; j < 3; j++ )
		{
			v[j] = ( float * )( xyzArray + elems[j] );
			tc[j] = ( float * )( stArray + elems[j] );
		}

		// calculate two mostly perpendicular edge directions
		VectorSubtract( v[1], v[0], stvec[0] );
		VectorSubtract( v[2], v[0], stvec[1] );

		// we have two edge directions, we can calculate the normal then
		CrossProduct( stvec[1], stvec[0], cross );

		for( j = 0; j < 3; j++ )
		{
			stvec[0][j] = ( ( tc[1][1] - tc[0][1] ) * ( v[2][j] - v[0][j] ) - ( tc[2][1] - tc[0][1] ) * ( v[1][j] - v[0][j] ) );
			stvec[1][j] = ( ( tc[1][0] - tc[0][0] ) * ( v[2][j] - v[0][j] ) - ( tc[2][0] - tc[0][0] ) * ( v[1][j] - v[0][j] ) );
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
		else
			s[3] = 1;
	}

	if( tVectorsArray != stackTVectorsArray )
		Mem_Free( tVectorsArray );
}
