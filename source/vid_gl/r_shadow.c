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

#include "r_local.h"
#include "mathlib.h"
#include "matrix_lib.h"

/*
=============================================================

PLANAR STENCIL SHADOWS

=============================================================
*/

static ref_shader_t *r_planarShadowShader;

/*
===============
R_InitPlanarShadows
===============
*/
static void R_InitPlanarShadows( void )
{
	r_planarShadowShader = R_LoadShader( "***r_planarShadow***", SHADER_PLANAR_SHADOW, true, 0, 0 );
}

/*
===============
R_PlanarShadowShader
===============
*/
ref_shader_t *R_PlanarShadowShader( void )
{
	return r_planarShadowShader;
}

/*
===============
R_GetShadowImpactAndDir
===============
*/
static void R_GetShadowImpactAndDir( ref_entity_t *e, trace_t *tr, vec3_t lightdir )
{
	vec3_t	point;

	R_LightForOrigin( e->lightingOrigin, lightdir, NULL, NULL, e->model->radius * e->scale );

	VectorSet( lightdir, -lightdir[0], -lightdir[1], -1 );
	VectorNormalizeFast( lightdir );
	VectorMA( e->origin, /*(e->model->radius*e->scale*4 + r_shadows_projection_distance->value)*/ 1024.0f, lightdir, point );

	R_TraceLine( tr, e->origin, point, SURF_NONSOLID );
}

/*
===============
R_CullPlanarShadow
===============
*/
bool R_CullPlanarShadow( ref_entity_t *e, vec3_t mins, vec3_t maxs, bool occlusion_query )
{
	float	planedist, dist;
	vec3_t	lightdir, point;
	vec3_t	bbox[8], newmins, newmaxs;
	trace_t	tr;
	int	i;

	if((e->flags & EF_NOSHADOW) || (e->ent_type == ED_VIEWMODEL))
		return true;
	if( RP_LOCALCLIENT( e ))
		return false;

	R_GetShadowImpactAndDir( e, &tr, lightdir );
	if( tr.flFraction == 1.0f )
		return true;

	R_TransformEntityBBox( e, mins, maxs, bbox, true );

	VectorSubtract( tr.vecEndPos, e->origin, point );
	planedist = DotProduct( point, tr.vecPlaneNormal ) + 1;
	dist = -1.0f / DotProduct( lightdir, tr.vecPlaneNormal );
	VectorScale( lightdir, dist, lightdir );

	ClearBounds( newmins, newmaxs );
	for( i = 0; i < 8; i++ )
	{
		VectorSubtract( bbox[i], e->origin, bbox[i] );
		dist = DotProduct( bbox[i], tr.vecPlaneNormal ) - planedist;
		if( dist > 0 ) VectorMA( bbox[i], dist, lightdir, bbox[i] );
		AddPointToBounds( bbox[i], newmins, newmaxs );
	}

	VectorAdd( newmins, e->origin, newmins );
	VectorAdd( newmaxs, e->origin, newmaxs );

	if( R_CullBox( newmins, newmaxs, RI.clipFlags ))
		return true;

	// mins/maxs are pretransfomed so use r_worldent here
	if( occlusion_query && OCCLUSION_QUERIES_ENABLED( RI ) )
		R_IssueOcclusionQuery( R_GetOcclusionQueryNum( OQ_PLANARSHADOW, e - r_entities ), r_worldent, newmins, newmaxs );

	return false;
}

/*
===============
R_DeformVPlanarShadow
===============
*/
void R_DeformVPlanarShadow( int numV, float *v )
{
	float		planedist, dist;
	ref_entity_t	*e = RI.currententity;
	vec3_t		planenormal, lightdir, lightdir2, point;
	trace_t		tr;

	R_GetShadowImpactAndDir( e, &tr, lightdir );

	Matrix3x3_Transform( e->axis, lightdir, lightdir2 );
	Matrix3x3_Transform( e->axis, tr.vecPlaneNormal, planenormal );
	VectorScale( planenormal, e->scale, planenormal );

	VectorSubtract( tr.vecEndPos, e->origin, point );
	planedist = DotProduct( point, tr.vecPlaneNormal ) + 1;
	dist = -1.0f / DotProduct( lightdir2, planenormal );
	VectorScale( lightdir2, dist, lightdir2 );

	for( ; numV > 0; numV--, v += 4 )
	{
		dist = DotProduct( v, planenormal ) - planedist;
		if( dist > 0 ) VectorMA( v, dist, lightdir2, v );
	}
}

/*
===============
R_PlanarShadowPass
===============
*/
void R_PlanarShadowPass( int state )
{
	GL_EnableTexGen( GL_S, 0 );
	GL_EnableTexGen( GL_T, 0 );
	GL_EnableTexGen( GL_R, 0 );
	GL_EnableTexGen( GL_Q, 0 );
	GL_SetTexCoordArrayMode( 0 );

	GL_SetState( state );
	pglColor4f( 0, 0, 0, bound( 0.0f, r_shadows_alpha->value, 1.0f ) );

	pglDisable( GL_TEXTURE_2D );
	if( glState.stencilEnabled )
		pglEnable( GL_STENCIL_TEST );

	R_FlushArrays();

	if( glState.stencilEnabled )
		pglDisable( GL_STENCIL_TEST );
	pglEnable( GL_TEXTURE_2D );
}

/*
=============================================================

STANDARD PROJECTIVE SHADOW MAPS (SSM)

=============================================================
*/

int r_numShadowGroups;
shadowGroup_t r_shadowGroups[MAX_SHADOWGROUPS];
int r_entShadowBits[MAX_ENTITIES];

//static bool r_shadowGroups_sorted;

#define SHADOWGROUPS_HASH_SIZE	8
static shadowGroup_t *r_shadowGroups_hash[SHADOWGROUPS_HASH_SIZE];
static byte r_shadowCullBits[MAX_SHADOWGROUPS/8];

/*
===============
R_InitShadowmaps
===============
*/
static void R_InitShadowmaps( void )
{
	// clear all possible values, should be called once per scene
	r_numShadowGroups = 0;
//	r_shadowGroups_sorted = false;

	Mem_Set( r_shadowGroups, 0, sizeof( r_shadowGroups ));
	Mem_Set( r_entShadowBits, 0, sizeof( r_entShadowBits ));
	Mem_Set( r_shadowGroups_hash, 0, sizeof( r_shadowGroups_hash ));
}

/*
===============
R_ClearShadowmaps
===============
*/
void R_ClearShadowmaps( void )
{
	r_numShadowGroups = 0;

	if( r_shadows->integer != SHADOW_MAPPING || RI.refdef.flags & RDF_NOWORLDMODEL )
		return;

	// clear all possible values, should be called once per scene
//	r_shadowGroups_sorted = false;
	memset( r_shadowGroups, 0, sizeof( r_shadowGroups ) );
	memset( r_entShadowBits, 0, sizeof( r_entShadowBits ) );
	memset( r_shadowGroups_hash, 0, sizeof( r_shadowGroups_hash ) );
}

/*
===============
R_AddShadowCaster
===============
*/
bool R_AddShadowCaster( ref_entity_t *ent )
{
	int i;
	float radius;
	vec3_t origin;
	unsigned int hash_key;
	shadowGroup_t *group;
	mleaf_t *leaf;
	vec3_t mins, maxs, bbox[8];

	if( r_shadows->integer != SHADOW_MAPPING || RI.refdef.flags & RDF_NOWORLDMODEL )
		return false;
	if( !GL_Support( R_SHADER_GLSL100_EXT ) || !GL_Support( R_DEPTH_TEXTURE ) || !GL_Support( R_SHADOW_EXT ))
		return false;

	VectorCopy( ent->lightingOrigin, origin );
	if( VectorIsNull( origin ))
		return false;

	// find lighting group containing entities with same lightingOrigin as ours
	hash_key = (uint)( origin[0] * 7 + origin[1] * 5 + origin[2] * 3 );
	hash_key &= (SHADOWGROUPS_HASH_SIZE - 1);

	for( group = r_shadowGroups_hash[hash_key]; group; group = group->hashNext )
	{
		if( VectorCompare( group->origin, origin ))
			goto add; // found an existing one, add
	}

	if( r_numShadowGroups == MAX_SHADOWGROUPS )
		return false; // no free groups

	leaf = Mod_PointInLeaf( origin, r_worldmodel );

	// start a new group
	group = &r_shadowGroups[r_numShadowGroups];
	group->bit = ( 1<<r_numShadowGroups );
	//	group->cluster = leaf->cluster;
	group->vis = Mod_ClusterPVS( leaf->cluster, r_worldmodel );

	// clear group bounds
	VectorCopy( origin, group->origin );
	ClearBounds( group->mins, group->maxs );

	// add to hash table
	group->hashNext = r_shadowGroups_hash[hash_key];
	r_shadowGroups_hash[hash_key] = group;

	r_numShadowGroups++;
add:
	// get model bounds
	switch( ent->model->type )
	{
	case mod_studio:
		R_StudioModelBBox( ent, mins, maxs );
		break;
	default:
		VectorClear( mins );
		VectorClear( maxs );
		break;
	}

	for( i = 0; i < 3; i++ )
	{
		if( mins[i] >= maxs[i] )
			return false;
	}

	r_entShadowBits[ent - r_entities] |= group->bit;
	if( ent->ent_type == ED_VIEWMODEL )
		return true;

	// rotate local bounding box and compute the full bounding box for this group
	R_TransformEntityBBox( ent, mins, maxs, bbox, true );
	for( i = 0; i < 8; i++ )
		AddPointToBounds( bbox[i], group->mins, group->maxs );

	// increase projection distance if needed
	VectorSubtract( group->mins, origin, mins );
	VectorSubtract( group->maxs, origin, maxs );
	radius = RadiusFromBounds( mins, maxs );
	group->projDist = max( group->projDist, radius * ent->scale * 2 + min( r_shadows_projection_distance->value, 64 ));

	return true;
}

/*
===============
R_ShadowGroupSort

Make sure current view cluster comes first
===============
*/
/*
static int R_ShadowGroupSort (void const *a, void const *b)
{
	shadowGroup_t *agroup, *bgroup;

	agroup = (shadowGroup_t *)a;
	bgroup = (shadowGroup_t *)b;

	if( agroup->cluster == r_viewcluster )
		return -2;
	if( bgroup->cluster == r_viewcluster )
		return 2;
	if( agroup->cluster < bgroup->cluster )
		return -1;
	if( agroup->cluster > bgroup->cluster )
		return 1;
	return 0;
}
*/

/*
===============
R_CullShadowmapGroups
===============
*/
void R_CullShadowmapGroups( void )
{
	int		i, j;
	vec3_t		mins, maxs;
	shadowGroup_t	*group;

	if( RI.refdef.flags & RDF_NOWORLDMODEL )
		return;

	Mem_Set( r_shadowCullBits, 0, sizeof( r_shadowCullBits ));

	for( i = 0, group = r_shadowGroups; i < r_numShadowGroups; i++, group++ )
	{
		for( j = 0; j < 3; j++ )
		{
			mins[j] = group->origin[j] - group->projDist * 1.75 * 0.5 * 0.5;
			maxs[j] = group->origin[j] + group->projDist * 1.75 * 0.5 * 0.5;
		}

		// check if view point is inside the bounding box...
		for( j = 0; j < 3; j++ )
		{
			if( RI.viewOrigin[j] < mins[j] || RI.viewOrigin[j] > maxs[j] )
				break;
                    }
		
		if( j == 3 ) continue;			// ...it is, so trivially accept

		if( R_CullBox( mins, maxs, RI.clipFlags ) )
			r_shadowCullBits[i>>3] |= (1<<(i&7));	// trivially reject
		else if( OCCLUSION_QUERIES_ENABLED( RI ))
			R_IssueOcclusionQuery( R_GetOcclusionQueryNum( OQ_SHADOWGROUP, i ), r_worldent, mins, maxs );
	}
}

/*
===============
R_DrawShadowmaps
===============
*/
void R_DrawShadowmaps( void )
{
	int		i, j;
	int		width, height, textureWidth, textureHeight;
	vec3_t		angles;
	vec3_t		lightdir, M[3];
	refinst_t 	oldRI;
	shadowGroup_t	*group;

	if( !r_numShadowGroups )
		return;

	width = r_lastRefdef.viewport[2];
	height = r_lastRefdef.viewport[3];

	RI.previousentity = NULL;
	Mem_Copy( &oldRI, &prevRI, sizeof( refinst_t ) );
	Mem_Copy( &prevRI, &RI, sizeof( refinst_t ) );
	RI.refdef.flags &= ~RDF_SKYPORTALINVIEW;
/*
	// sort by clusternum (not really needed anymore, but oh well)
	if( !r_shadowGroups_sorted ) {		// note: this breaks hash pointers
		r_shadowGroups_sorted = true;
		qsort( r_shadowGroups, r_numShadowGroups, sizeof(shadowGroup_t), R_ShadowGroupSort );
	}
*/

	// find lighting group containing entities with same lightingOrigin as ours
	for( i = 0, group = r_shadowGroups; i < r_numShadowGroups; i++, group++ )
	{
		if( r_shadowCullBits[i>>3] & ( 1<<( i&7 ) ) )
			continue;

		if( OCCLUSION_QUERIES_ENABLED( prevRI ) )
		{
			if( !R_GetOcclusionQueryResultBool( OQ_SHADOWGROUP, i, true ) )
				continue;
		}

		RI.farClip = group->projDist;
		RI.clipFlags |= ( 1<<4 ); // clip by far plane too
		RI.shadowBits = 0;      // no shadowing yet
		RI.meshlist = &r_shadowlist;
		RI.shadowGroup = group;
		RI.params = RP_SHADOWMAPVIEW|RP_FLIPFRONTFACE|RP_OLDVIEWCLUSTER; // make sure RP_WORLDSURFVISIBLE isn't set

		// allocate/resize the texture if needed
		R_InitShadowmapTexture( &( tr.shadowmapTextures[i] ), i, width, height );

		group->depthTexture = tr.shadowmapTextures[i];
		textureWidth = group->depthTexture->width;
		textureHeight = group->depthTexture->height;

		// default to fov 90, R_SetupFrame will most likely alter the values to give depth more precision
		RI.refdef.viewport[2] = textureWidth;
		RI.refdef.viewport[3] = textureHeight;
		RI.refdef.fov_x = 90;
		RI.refdef.fov_y = CalcFov( RI.refdef.fov_x, RI.refdef.viewport[2], RI.refdef.viewport[3] );
		Vector4Set( RI.viewport, RI.refdef.viewport[0], RI.refdef.viewport[1], textureWidth, textureHeight );
		Vector4Set( RI.scissor, RI.refdef.viewport[0], RI.refdef.viewport[1], textureWidth, textureHeight );

		// set the view transformation matrix according to lightgrid
		R_LightForOrigin( group->origin, lightdir, NULL, NULL, group->projDist * 0.5 );
		VectorSet( lightdir, -lightdir[0], -lightdir[1], -lightdir[2] );
		VectorNormalizeFast( lightdir );

		NormalVectorToAxis( lightdir, M );
		Matrix3x3_ToAngles( M, angles, true );

		for( j = 0; j < 3; j++ )
			RI.refdef.viewangles[j] = anglemod( angles[j] );

		// position the light source in the opposite direction
		VectorMA( group->origin, -group->projDist * 0.5, lightdir, RI.refdef.vieworg );

		R_RenderView( &RI.refdef );

		if( !( RI.params & RP_WORLDSURFVISIBLE ) )
			continue; // we didn't cast any shadows on opaque meshes so discard this group

		if( !( prevRI.shadowBits & group->bit ) )
		{	// capture results from framebuffer into depth texture
			prevRI.shadowBits |= group->bit;
			GL_Bind( 0, group->depthTexture );
			pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, RI.refdef.viewport[0], RI.refdef.viewport[1], textureWidth, textureHeight );
		}

		Matrix4x4_Copy( group->worldviewProjectionMatrix, RI.worldviewProjectionMatrix );
	}

	oldRI.shadowBits |= prevRI.shadowBits;  // set shadowBits for all RI's so that we won't
	Mem_Copy( &RI, &prevRI, sizeof( refinst_t ));
	Mem_Copy( &prevRI, &oldRI, sizeof( refinst_t ));
}

//==================================================================================

/*
===============
R_InitShadows
===============
*/
void R_InitShadows( void )
{
	R_InitPlanarShadows();

	R_InitShadowmaps();
}

/*
===============
R_ShutdownShadows
===============
*/
void R_ShutdownShadows( void )
{
	r_planarShadowShader = NULL;
}
