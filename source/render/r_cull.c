//=======================================================================
//			Copyright XashXT Group 2008 ©
//			r_cull.c - frustum and pvs culling
//=======================================================================

#include <assert.h>
#include "r_local.h"
#include "r_meshbuffer.h"
#include "mathlib.h"
#include "const.h"

/*
=============================================================

FRUSTUM AND PVS CULLING

=============================================================
*/
/*
=================
R_CullBox

Returns true if the box is completely outside the frustum
=================
*/
bool R_CullBox( const vec3_t mins, const vec3_t maxs, const uint clipflags )
{
	uint		i, bit;
	const cplane_t	*plane;

	if( r_nocull->integer ) return false;

	i = sizeof( Ref.frustum ) / sizeof( Ref.frustum[0] );
	for( bit = 1, plane = Ref.frustum; i > 0; i--, bit<<=1, plane++ )
	{
		if(!( clipflags & bit )) continue;
		if( BoxOnPlaneSide( mins, maxs, plane ) == SIDE_ON )
			return true;
	}
	return false;
}

/*
=================
R_CullSphere

Returns true if the sphere is completely outside the frustum
=================
*/
bool R_CullSphere( const vec3_t origin, const float radius, const uint clipflags )
{
	uint		i, bit;
	const cplane_t	*plane;

	if( r_nocull->integer ) return false;

	i = sizeof( Ref.frustum ) / sizeof( Ref.frustum[0] ); 
	for( bit = 1, plane = Ref.frustum; i > 0; i--, bit<<=1, plane++ )
	{
		if(!( clipflags & bit )) continue;
		if( DotProduct( origin, plane->normal ) - plane->dist <= -radius )
			return true;
	}
	return false;
}

/*
===================
R_VisCullBox
===================
*/
bool R_VisCullBox( const vec3_t mins, const vec3_t maxs )
{
	int	s, stackdepth = 0;
	vec3_t	extmins, extmaxs;
	mnode_t	*node, *localstack[2048];

	if( !r_worldModel || ( Ref.refdef.rdflags & RDF_NOWORLDMODEL ))
		return false;
	if( r_novis->integer ) return false;

	for( s = 0; s < 3; s++ )
	{
		extmins[s] = mins[s] - 4;
		extmaxs[s] = maxs[s] + 4;
	}

	for( node = r_worldBrushModel->nodes;; )
	{
		if( node->visFrame != r_visFrameCount )
		{
			if( !stackdepth ) return true;
			node = localstack[--stackdepth];
			continue;
		}

		if( !node->plane ) return false;

		s = BoxOnPlaneSide( extmins, extmaxs, node->plane ) - 1;
		if( s < 2 )
		{
			node = node->children[s];
			continue;
		}

		// go down both sides
		if( stackdepth < sizeof( localstack )/sizeof( mnode_t * ))
			localstack[stackdepth++] = node->children[0];
		node = node->children[1];
	}
	return true;
}

/*
===================
R_VisCullSphere
===================
*/
bool R_VisCullSphere( const vec3_t origin, float radius )
{
	float	dist;
	int	stackdepth = 0;
	mnode_t	*node, *localstack[2048];

	if( !r_worldModel || ( Ref.refdef.rdflags & RDF_NOWORLDMODEL ))
		return false;
	if( r_novis->integer ) return false;

	radius += 4;
	for( node = r_worldBrushModel->nodes;; )
	{
		if( node->visFrame != r_visFrameCount )
		{
			if( !stackdepth ) return true;
			node = localstack[--stackdepth];
			continue;
		}

		if( !node->plane ) return false;

		dist = PlaneDiff( origin, node->plane );
		if( dist > radius )
		{
			node = node->children[0];
			continue;
		}
		else if( dist < -radius )
		{
			node = node->children[1];
			continue;
		}

		// go down both sides
		if( stackdepth < sizeof( localstack ) / sizeof( mnode_t * ))
			localstack[stackdepth++] = node->children[0];
		node = node->children[1];
	}
	return true;
}

/*
=============
R_CullModel
=============
*/
int R_CullModel( ref_entity_t *e, vec3_t mins, vec3_t maxs, float radius )
{
	if( e->renderfx & RF_VIEWMODEL )
	{
		if( Ref.params & RP_NONVIEWERREF )
			return SIDE_BACK;
		return SIDE_FRONT;
	}

	if( e->renderfx & RF_PLAYERMODEL )
	{
		if(!( Ref.params & ( RP_MIRRORVIEW|RP_SHADOWMAPVIEW )))
			return SIDE_BACK;
	}

	if( R_CullSphere( e->origin, radius, Ref.clipFlags ))
		return SIDE_BACK;

	if( Ref.refdef.rdflags & ( RDF_PORTALINVIEW|RDF_SKYPORTALINVIEW ) || ( Ref.params & RP_SKYPORTALVIEW ))
	{
		if( R_VisCullSphere( e->origin, radius ))
			return SIDE_ON;
	}
	return SIDE_FRONT;
}


/*
=============================================================

OCCLUSION QUERIES

=============================================================
*/

// occlusion queries for entities
#define BEGIN_OQ_ENTITIES		0
#define MAX_OQ_ENTITIES		MAX_ENTITIES
#define END_OQ_ENTITIES		( BEGIN_OQ_ENTITIES+MAX_OQ_ENTITIES )

// occlusion queries for planar shadows
#define BEGIN_OQ_PLANARSHADOWS	END_OQ_ENTITIES
#define MAX_OQ_PLANARSHADOWS		MAX_ENTITIES
#define END_OQ_PLANARSHADOWS		( BEGIN_OQ_PLANARSHADOWS+MAX_OQ_PLANARSHADOWS )

// occlusion queries for shadowgroups
#define BEGIN_OQ_SHADOWGROUPS		END_OQ_PLANARSHADOWS
#define MAX_OQ_SHADOWGROUPS		MAX_SHADOWGROUPS
#define END_OQ_SHADOWGROUPS		( BEGIN_OQ_SHADOWGROUPS+MAX_OQ_SHADOWGROUPS )

// custom occlusion queries (portals, mirrors, etc)
#define BEGIN_OQ_CUSTOM		END_OQ_SHADOWGROUPS
#define MAX_OQ_CUSTOM		MAX_SURF_QUERIES
#define END_OQ_CUSTOM		( BEGIN_OQ_CUSTOM+MAX_OQ_CUSTOM )

#define MAX_OQ_TOTAL		( MAX_OQ_ENTITIES+MAX_OQ_PLANARSHADOWS+MAX_OQ_SHADOWGROUPS+MAX_OQ_CUSTOM )

static byte	r_queriesBits[MAX_OQ_TOTAL/8];
static GLuint	r_occlusionQueries[MAX_OQ_TOTAL];

static meshbuffer_t r_occluderMB;
static ref_shader_t	*r_occlusionShader;
static bool	r_occludersQueued;
static ref_entity_t	*r_occlusionEntity;

static void R_RenderOccludingSurfaces( void );

/*
===============
R_InitOcclusionQueries
===============
*/
void R_InitOcclusionQueries( void )
{
	meshbuffer_t *meshbuf = &r_occluderMB;

	if( !r_occlusionShader )
		r_occlusionShader = R_FindShader( "***r_occlusion***", SHADER_GENERIC, 0 );

	if( !GL_Support( R_OCCLUSION_QUERY ))
		return;

	pglGenQueriesARB( MAX_OQ_TOTAL, r_occlusionQueries );

	meshbuf->sortKey = R_EDICTNUM( r_worldEntity )|MESH_MODEL;
	meshbuf->shaderKey = r_occlusionShader->sortKey;
}

/*
===============
R_BeginOcclusionPass
===============
*/
void R_BeginOcclusionPass( void )
{
	assert( OCCLUSION_QUERIES_ENABLED( Ref ));

	r_occludersQueued = false;
	r_occlusionEntity = r_worldEntity;
	Mem_Set( r_queriesBits, 0, sizeof( r_queriesBits ));

	R_LoadIdentity();
	RB_ResetPassMask();
	R_ClearSurfOcclusionQueryKeys();

	pglShadeModel( GL_FLAT );
	pglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
	pglDisable( GL_TEXTURE_2D );
}

/*
===============
R_EndOcclusionPass
===============
*/
void R_EndOcclusionPass( void )
{
	assert( OCCLUSION_QUERIES_ENABLED( Ref ));

	R_RenderOccludingSurfaces();
	R_SurfIssueOcclusionQueries();
	RB_ResetPassMask();
	RB_ResetCounters();

	if( r_occlusion_queries_finish->integer )
		pglFinish();
	else pglFlush();

	pglShadeModel( GL_SMOOTH );
	pglEnable( GL_TEXTURE_2D );
	pglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
}

/*
===============
R_RenderOccludingSurfaces
===============
*/
static void R_RenderOccludingSurfaces( void )
{
	if( !r_occludersQueued )
		return;

	r_occludersQueued = false;
	R_RotateForEntity( r_occlusionEntity );
	RB_ResetPassMask();
	R_RenderMeshBuffer( &r_occluderMB );
}

/*
===============
R_OcclusionShader
===============
*/
ref_shader_t *R_OcclusionShader( void )
{
	return r_occlusionShader;
}

/*
===============
R_AddOccludingSurface
===============
*/
void R_AddOccludingSurface( msurface_t *surf, ref_shader_t *shader )
{
	int diff = shader->flags ^ r_occlusionShader->flags;

	if( R_MeshOverflow( surf->mesh ) || ( diff & ( SHADER_CULL )) || r_occlusionEntity != Ref.m_pCurrentEntity )
	{
		R_RenderOccludingSurfaces();

		r_occlusionShader->flags ^= diff;
		r_occlusionEntity = Ref.m_pCurrentEntity;
	}

	r_occludersQueued = true;
	R_PushMesh( surf->mesh, 0 );
}

/*
===============
R_GetOcclusionQueryNum
===============
*/
int R_GetOcclusionQueryNum( int type, int key )
{
	switch( type )
	{
	case OQ_ENTITY:
		return ( r_occlusion_queries->integer & 1 ) ? BEGIN_OQ_ENTITIES + ( key%MAX_OQ_ENTITIES ) : -1;
	case OQ_PLANARSHADOW:
		return ( r_occlusion_queries->integer & 1 ) ? BEGIN_OQ_PLANARSHADOWS + ( key%MAX_OQ_PLANARSHADOWS ) : -1;
	case OQ_SHADOWGROUP:
		return ( r_occlusion_queries->integer & 2 ) ? BEGIN_OQ_SHADOWGROUPS + ( key%MAX_OQ_SHADOWGROUPS ) : -1;
	case OQ_CUSTOM:
		return BEGIN_OQ_CUSTOM + ( key%MAX_OQ_CUSTOM );
	}
	return -1;
}

/*
===============
R_IssueOcclusionQuery
===============
*/
int R_IssueOcclusionQuery( int query, ref_entity_t *e, vec3_t mins, vec3_t maxs )
{
	static vec4_t	verts[8];
	static rb_mesh_t	mesh;
	static uint	indices[] =
	{
		0, 1, 2, 1, 2, 3, // top
		7, 5, 6, 5, 6, 4, // bottom
		4, 0, 6, 0, 6, 2, // front
		3, 7, 1, 7, 1, 5, // back
		0, 1, 4, 1, 4, 5, // left
		7, 6, 3, 6, 3, 2, // right
	};
	int	i;
	vec3_t	bbox[8];

	if( query < 0 ) return -1;
	if( r_queriesBits[query>>3] & (1<<( query&7 )))
		return -1;
	r_queriesBits[query>>3] |= (1<<( query&7 ));

	R_RenderOccludingSurfaces();

	mesh.numVerts = 8;
	mesh.points = verts;
	mesh.numIndexes = 36;
	mesh.indexes = indices;
	r_occlusionShader->flags &= ~SHADER_CULL;

	pglBeginQueryARB( GL_SAMPLES_PASSED_ARB, r_occlusionQueries[query] );

	R_TransformEntityBBox( e, mins, maxs, bbox, false );
	for( i = 0; i < 8; i++ )
		Vector4Set( verts[i], bbox[i][0], bbox[i][1], bbox[i][2], 1 );

	R_RotateForEntity( e );
	RB_SetPassMask( GLSTATE_MASK & ~GLSTATE_DEPTHWRITE );
	R_PushMesh( &mesh, MF_NONBATCHED|MF_NOCULL );
	R_RenderMeshBuffer( &r_occluderMB );

	pglEndQueryARB( GL_SAMPLES_PASSED_ARB );

	return query;
}

/*
===============
R_OcclusionQueryIssued
===============
*/
bool R_OcclusionQueryIssued( int query )
{
	assert( query >= 0 && query < MAX_OQ_TOTAL );
	return r_queriesBits[query>>3] & (1<<(query & 7));
}

/*
===============
R_GetOcclusionQueryResult
===============
*/
uint R_GetOcclusionQueryResult( int query, bool wait )
{
	GLint 	available;
	GLuint	sampleCount;

	if( query < 0 ) return 1;
	if( !R_OcclusionQueryIssued( query ))
		return 1;

	query = r_occlusionQueries[query];

	pglGetQueryObjectivARB( query, GL_QUERY_RESULT_AVAILABLE_ARB, &available );
	if( !available && wait )
	{
		int n = 0;
		do
		{
			n++;
			pglGetQueryObjectivARB( query, GL_QUERY_RESULT_AVAILABLE_ARB, &available );
		} while( !available && ( n < 10000 ));
	}

	if( !available ) return 0;
	pglGetQueryObjectuivARB( query, GL_QUERY_RESULT_ARB, &sampleCount );

	return sampleCount;
}

/*
===============
R_GetOcclusionQueryResultBool
===============
*/
bool R_GetOcclusionQueryResultBool( int type, int key, bool wait )
{
	int query = R_GetOcclusionQueryNum( type, key );

	if( query >= 0 && R_OcclusionQueryIssued( query ) )
	{
		if( R_GetOcclusionQueryResult( query, wait ) )
			return true;
		return false;
	}
	return true;
}

/*
===============
R_ShutdownOcclusionQueries
===============
*/
void R_ShutdownOcclusionQueries( void )
{
	r_occlusionShader = NULL;

	if(!GL_Support( R_OCCLUSION_QUERY ))
		return;

	pglDeleteQueriesARB( MAX_OQ_TOTAL, r_occlusionQueries );
	r_occludersQueued = false;
}
