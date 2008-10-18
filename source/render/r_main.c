//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_main.c - opengl render core
//=======================================================================

#include "r_local.h"
#include "r_meshbuffer.h"
#include "mathlib.h"
#include "matrixlib.h"
#include "const.h"

render_imp_t	ri;
stdlib_api_t	com;
mapconfig_t	mapConfig;
ref_state_t	Ref, oldRef;
refdef_t		r_lastRefdef;

byte		*r_temppool;
rmodel_t		*cl_models[MAX_MODELS];		// client replacement modeltable
lightstyle_t	r_lightStyles[MAX_LIGHTSTYLES];
ref_entity_t	r_entities[MAX_ENTITIES];
int		r_numEntities;
dlight_t		r_dlights[MAX_DLIGHTS];
int		r_numDLights;
particle_t	r_particles[MAX_PARTICLES];
int		r_numParticles;
poly_t		r_polys[MAX_POLYS];
int		r_numPolys;
refdef_t		r_refdef;
refstats_t	r_stats;
byte		*r_framebuffer;			// pause frame buffer
float		r_pause_alpha;
glconfig_t	gl_config;
glstate_t		gl_state;
static int	r_numNullEntities;
static int	r_numBmodelEntities;
static ref_entity_t	*r_bmodelEntities[MAX_ENTITIES];
static ref_entity_t	*r_nullEntities[MAX_ENTITIES];
static byte	r_entVisBits[MAX_ENTITIES/8];
int		r_entShadowBits[MAX_ENTITIES];
float		r_farclip_min, r_farclip_bias;
float		gldepthmin, gldepthmax;
int		r_features;

// view matrix
cvar_t	*r_check_errors;
cvar_t	*r_hwgamma;
cvar_t	*r_himodels;
cvar_t	*r_norefresh;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_nobind;
cvar_t	*r_drawworld;
cvar_t	*r_drawentities;
cvar_t	*r_drawparticles;
cvar_t	*r_drawpolys;
cvar_t	*r_fullbright;
cvar_t	*r_lightmap;
cvar_t	*r_lockpvs;
cvar_t	*r_frontbuffer;
cvar_t	*r_showcluster;
cvar_t	*r_showtris;
cvar_t	*r_shownormals;
cvar_t	*r_showtangentspace;
cvar_t	*r_showmodelbounds;
cvar_t	*r_showshadowvolumes;
cvar_t	*r_showlightmaps;
cvar_t	*r_showtextures;
cvar_t	*r_offsetfactor;
cvar_t	*r_offsetunits;
cvar_t	*r_debugsort;
cvar_t	*r_speeds;
cvar_t	*r_singleshader;
cvar_t	*r_skipbackend;
cvar_t	*r_skipfrontend;
cvar_t	*r_swapInterval;
cvar_t	*r_mode;
cvar_t	*r_testmode;
cvar_t	*r_fullscreen;
cvar_t	*r_minimap;
cvar_t	*r_minimap_size;
cvar_t	*r_minimap_zoom;
cvar_t	*r_minimap_style;
cvar_t	*r_pause;
cvar_t	*r_width;
cvar_t	*r_height;
cvar_t	*r_refreshrate;
cvar_t	*r_bitdepth;
cvar_t	*r_overbrightbits;
cvar_t	*r_mapoverbrightbits;
cvar_t	*r_shadows;
cvar_t	*r_caustics;
cvar_t	*r_occlusion_queries;
cvar_t	*r_occlusion_queries_finish;
cvar_t	*r_dynamiclights;
cvar_t	*r_modulate;
cvar_t	*r_ambientscale;
cvar_t	*r_directedscale;
cvar_t	*r_fastsky;
cvar_t	*r_polyblend;
cvar_t	*r_flares;
cvar_t	*r_flarefade;
cvar_t	*r_flaresize;
cvar_t	*r_coronascale;
cvar_t	*r_intensity;
cvar_t	*r_texturebits;
cvar_t	*r_texturefilter;
cvar_t	*r_texturefilteranisotropy;
cvar_t	*r_lighting_glossintensity;
cvar_t	*r_lighting_glossexponent;
cvar_t	*r_detailtextures;
cvar_t	*r_lmblocksize;
cvar_t	*r_portalmaps;
cvar_t	*r_lefthand;
cvar_t	*r_bloom;
cvar_t	*r_bloom_alpha;
cvar_t	*r_bloom_diamond_size;
cvar_t	*r_bloom_intensity;
cvar_t	*r_bloom_darken;
cvar_t	*r_bloom_sample_size;
cvar_t	*r_bloom_fast_sample;
cvar_t	*r_motionblur_intens;
cvar_t	*r_motionblur;
cvar_t	*r_mirroralpha;
cvar_t	*r_interpolate;
cvar_t	*r_physbdebug;
cvar_t	*r_pause_bw;
cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level

cvar_t	*gl_finish;
cvar_t	*gl_clear;
cvar_t	*vid_gamma;

/*
=============
R_TransformEntityBBox
=============
*/
void R_TransformEntityBBox( ref_entity_t *e, vec3_t mins, vec3_t maxs, vec3_t bbox[8], bool local )
{
	int		i;
	vec3_t		tmp;
	matrix3x3		matrix;

	if( e == r_worldEntity ) local = false;
	if( local ) Matrix3x3_Transpose( e->matrix, matrix ); // switch row-column order

	// rotate local bounding box and compute the full bounding box
	for( i = 0; i < 8; i++ )
	{
		vec_t	*corner = bbox[i];

		corner[0] = (( i & 1 ) ? mins[0] : maxs[0] );
		corner[1] = (( i & 2 ) ? mins[1] : maxs[1] );
		corner[2] = (( i & 4 ) ? mins[2] : maxs[2] );

		if( local )
		{
			Matrix3x3_Transform( matrix, corner, tmp );
			VectorAdd( tmp, e->origin, corner );
		}
	}
}


/*
=============
R_TraceLine
=============
*/
msurface_t *R_TraceLine( trace_t *tr, const vec3_t start, const vec3_t end, int surfumask )
{
	int		i;
	msurface_t	*surf;

	// trace against world
	surf = R_TransformedTraceLine( tr, start, end, r_worldEntity, surfumask );

	// trace against bmodels
	for( i = 0; i < r_numBmodelEntities; i++ )
	{
		trace_t		t2;
		msurface_t	*s2;

		s2 = R_TransformedTraceLine( &t2, start, end, r_bmodelEntities[i], surfumask );
		if( t2.fraction < tr->fraction )
		{
			*tr = t2;	// closer impact point
			surf = s2;
		}
	}
	return surf;
}

/*
=============
R_LoadIdentity
=============
*/
void R_LoadIdentity( void )
{
	Matrix4x4_LoadIdentity( Ref.entityMatrix );
	Matrix4x4_Copy( Ref.modelViewMatrix, Ref.worldMatrix );
	GL_LoadMatrix( Ref.modelViewMatrix );
}

/*
=================
R_RotateForEntity
=================
*/
void R_RotateForEntity( ref_entity_t *e )
{
	if( e == r_worldEntity )
	{
		R_LoadIdentity();
		return;
	}
#if 0
	// classic slow version (used for debug)
	Matrix4x4_LoadIdentity( Ref.entityMatrix );
	Matrix4x4_ConcatTranslate( Ref.entityMatrix, e->origin[0],  e->origin[1],  e->origin[2] );
	Matrix4x4_ConcatRotate( Ref.entityMatrix,  e->angles[1],  0, 0, 1 );
	Matrix4x4_ConcatRotate( Ref.entityMatrix, -e->angles[0],  0, 1, 0 );
	Matrix4x4_ConcatRotate( Ref.entityMatrix, -e->angles[2],  1, 0, 0 );
	Matrix4x4_Concat( Ref.modelViewMatrix, Ref.worldMatrix, Ref.entityMatrix );
#else
	Matrix4x4_FromVectors( Ref.entityMatrix, e->matrix[0], e->matrix[1], e->matrix[2], vec3_origin );
	Matrix4x4_SetOrigin( Ref.entityMatrix, e->origin[0], e->origin[1], e->origin[2] );
	Matrix4x4_Concat( Ref.modelViewMatrix, Ref.worldMatrix, Ref.entityMatrix );
#endif
	GL_LoadMatrix( Ref.modelViewMatrix );
}

/*
=============
R_TranslateForEntity
=============
*/
void R_TranslateForEntity( ref_entity_t *e )
{
	if( e == r_worldEntity )
	{
		R_LoadIdentity();
		return;
	}

	Matrix4x4_LoadIdentity( Ref.entityMatrix );
	Matrix4x4_SetOrigin( Ref.entityMatrix, e->origin[0], e->origin[1], e->origin[2] );
	Matrix4x4_Concat( Ref.modelViewMatrix, Ref.worldMatrix, Ref.entityMatrix );

	GL_LoadMatrix( Ref.modelViewMatrix );
}

/*
=============
R_FogForSphere
=============
*/
mfog_t *R_FogForSphere( const vec3_t centre, const float radius )
{
	int	i, j;
	mfog_t	*fog;
	cplane_t	*plane;

	if( !r_worldModel || ( Ref.refdef.rdflags & RDF_NOWORLDMODEL ) || !r_worldBrushModel->numFogs )
		return NULL;
	if( Ref.params & RP_SHADOWMAPVIEW )
		return NULL;
	if( r_worldBrushModel->globalfog )
		return r_worldBrushModel->globalfog;

	fog = r_worldBrushModel->fogs;
	for( i = 0; i < r_worldBrushModel->numFogs; i++, fog++ )
	{
		if( !fog->shader ) continue;

		plane = fog->planes;
		for( j = 0; j < fog->numplanes; j++, plane++ )
		{
			// if completely in front of face, no intersection
			if( PlaneDiff( centre, plane ) > radius )
				break;
		}

		if( j == fog->numplanes )
			return fog;
	}
	return NULL;
}

/*
=============
R_CompletelyFogged
=============
*/
bool R_CompletelyFogged( mfog_t *fog, vec3_t origin, float radius )
{
	// note that fog->distanceToEye < 0 is always true if
	// globalfog is not NULL and we're inside the world boundaries
	if( fog && fog->shader && Ref.fog_dist_to_eye[fog - r_worldBrushModel->fogs] < 0 )
	{
		float vpnDist = (( Ref.vieworg[0] - origin[0] ) * Ref.forward[0] + ( Ref.vieworg[1] - origin[1] ) * Ref.forward[1] + ( Ref.vieworg[2] - origin[2] ) * Ref.forward[2] );
		return (( vpnDist + radius ) / fog->shader->fogDist ) < -1;
	}
	return false;
}

// =====================================================================

/*
=============================================================

SPRITE MODELS AND FLARES

=============================================================
*/

static vec4_t spr_points[4] =
{
{ 0, 0, 0, 1 },
{ 0, 0, 0, 1 },
{ 0, 0, 0, 1 },
{ 0, 0, 0, 1 }
};

static vec2_t spr_st[4] =
{
{ 0, 1 },
{ 0, 0 },
{ 1, 0 },
{ 1, 1 }
};

static vec4_t spr_color[4];
static rb_mesh_t spr_mesh = { 4, spr_points, spr_points, NULL, spr_st, { 0, 0, 0, 0 }, { spr_color, spr_color, spr_color, spr_color }, 6, NULL };

/*
=================
R_PushSprite
=================
*/
static bool R_PushSprite( const meshbuffer_t *mb, float rotation, float right, float left, float up, float down )
{
	int		i, features;
	vec3_t		point;
	vec3_t		v_right, v_up;
	ref_entity_t	*e = Ref.m_pCurrentEntity;
	ref_shader_t		*shader;

	if( rotation )
	{
		RotatePointAroundVector( v_right, Ref.forward, Ref.right, rotation );
		CrossProduct( Ref.forward, v_right, v_up );
	}
	else
	{
		VectorCopy( Ref.right, v_right );
		VectorCopy( Ref.up, v_up );
	}

	VectorScale( v_up, down, point );
	VectorMA( point, -left, v_right, spr_points[0] );
	VectorMA( point, -right, v_right, spr_points[3] );

	VectorScale( v_up, up, point );
	VectorMA( point, -left,  v_right, spr_points[1] );
	VectorMA( point, -right, v_right, spr_points[2] );

	if( e->scale != 1.0f )
	{
		for( i = 0; i < 4; i++ )
			VectorScale( spr_points[i], e->scale, spr_points[i] );
	}

	Shader_ForKey( mb->shaderKey, shader );

	// the code below is disgusting, but some q3a shaders use 'rgbgen vertex'
	// and 'alphagen vertex' for effects instead of 'rgbgen entity' and 'alphagen entity'
	if( shader->features & MF_COLORS )
	{
		for( i = 0; i < 4; i++ )
			Vector4Copy( e->rendercolor, spr_color[i] );
	}

	features = MF_NOCULL|MF_TRIFAN|shader->features;
	if( r_shownormals->integer ) features |= MF_NORMALS;

	if( shader->flags & SHADER_ENTITYMERGABLE )
	{
		for( i = 0; i < 4; i++ )
			VectorAdd( spr_points[i], e->origin, spr_points[i] );
		R_PushMesh( &spr_mesh, features );
		return false;
	}

	R_PushMesh( &spr_mesh, MF_NONBATCHED|features );
	return true;
}

/*
=================
R_PushFlareSurf
=================
*/
static void R_PushFlareSurf( const meshbuffer_t *mb )
{
	int		i;
	vec4_t		color;
	vec3_t		origin, point, v;
	float		radius = r_flaresize->value, colorscale, depth;
	float		up = radius, down = -radius, left = -radius, right = radius;
	mbrushmodel_t	*bmodel = ( mbrushmodel_t * )Ref.m_pCurrentModel->extradata;
	msurface_t	*surf = &bmodel->surfaces[mb->infoKey - 1];
	ref_shader_t		*shader;

	if( Ref.m_pCurrentModel != r_worldModel )
	{
		Matrix3x3_Transform( Ref.m_pCurrentEntity->matrix, surf->origin, origin );
		VectorAdd( origin, Ref.m_pCurrentEntity->origin, origin );
	}
	else VectorCopy( surf->origin, origin );

	R_TransformWorldToScreen( origin, v );

	if( v[0] < Ref.refdef.rect.x || v[0] > Ref.refdef.rect.x + Ref.refdef.rect.width )
		return;
	if( v[1] < Ref.refdef.rect.y || v[1] > Ref.refdef.rect.y + Ref.refdef.rect.height )
		return;

	// read one pixel
	pglReadPixels((int)(v[0]), (int)(v[1]), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth );
	if( depth + 1e-4 < v[2] ) return; // occluded

	VectorCopy( origin, origin );

	VectorMA( origin, down, Ref.up, point );
	VectorMA( point, -left, Ref.right, spr_points[0] );
	VectorMA( point, -right, Ref.right, spr_points[3] );

	VectorMA( origin, up, Ref.up, point );
	VectorMA( point, -left, Ref.right, spr_points[1] );
	VectorMA( point, -right, Ref.right, spr_points[2] );

	colorscale = r_flarefade->value;
	Vector4Set( color, surf->color[0] * colorscale, surf->color[1] * colorscale, surf->color[2] * colorscale, 1.0f );
	for( i = 0; i < 4; i++ )
		color[i] = bound( 0, color[i], 1.0f );

	for( i = 0; i < 4; i++ )
		Vector4Copy( color, spr_color[i] );

	Shader_ForKey( mb->shaderKey, shader );

	R_PushMesh( &spr_mesh, MF_NOCULL|MF_TRIFAN|shader->features );
}

/*
=================
R_PushCorona
=================
*/
static void R_PushCorona( const meshbuffer_t *mb )
{
	int	i;
	vec4_t	color;
	vec3_t	origin, point;
	dlight_t	*light = r_dlights + ( -mb->infoKey - 1 );
	float	radius = light->intensity, colorscale;
	float	up = radius;
	float	down = -radius;
	float	left = -radius;
	float	right = radius;
	ref_shader_t	*shader;

	VectorCopy( light->origin, origin );

	VectorMA( origin, down, Ref.up, point );
	VectorMA( point, -left, Ref.right, spr_points[0] );
	VectorMA( point, -right, Ref.right, spr_points[3] );

	VectorMA( origin, up, Ref.up, point );
	VectorMA( point, -left, Ref.right, spr_points[1] );
	VectorMA( point, -right, Ref.right, spr_points[2] );

	colorscale = 1.0f * bound( 0, r_coronascale->value, 1.0 );
	Vector4Set( color, light->color[0] * colorscale, light->color[1] * colorscale, light->color[2] * colorscale, 1.0f );
	for( i = 0; i < 4; i++ )
		color[i] = bound( 0.0f, color[i], 1.0f );

	for( i = 0; i < 4; i++ )
		Vector4Copy( color, spr_color[i] );

	Shader_ForKey( mb->shaderKey, shader );
	R_PushMesh( &spr_mesh, MF_NOCULL|MF_TRIFAN|shader->features );
}

/*
=================
R_PushSpriteModel
=================
*/
bool R_PushSpriteModel( const meshbuffer_t *mb )
{
	mspriteframe_t	*frame;
	ref_entity_t	*e = Ref.m_pCurrentEntity;

	frame = R_GetSpriteFrame( e );
	if( !frame ) return false;

	return R_PushSprite( mb, e->rotation, frame->right, frame->left, frame->up, frame->down );
}

/*
=================
R_PushSpritePoly
=================
*/
bool R_PushSpritePoly( const meshbuffer_t *mb )
{
	ref_entity_t	*e = Ref.m_pCurrentEntity;

	if(( mb->sortKey & 3 ) == MESH_CORONA )
	{
		R_PushCorona( mb );
		return false;
	}
	if( mb->infoKey > 0 )
	{
		R_PushFlareSurf( mb );
		return false;
	}
	return R_PushSprite( mb, e->rotation, -e->radius, e->radius, e->radius, -e->radius );
}

/*
=================
R_AddSpriteModelToList
=================
*/
static void R_AddSpriteModelToList( ref_entity_t *e )
{
	mspriteframe_t	*frame;
	msprite_t		*psprite;
	rmodel_t		*model = e->model;
	float		dist;
	meshbuffer_t	*mb;

	if(!( psprite = ((msprite_t *)model->extradata )))
		return;

	dist = (e->origin[0]-Ref.refdef.vieworg[0]) * Ref.forward[0] + (e->origin[1]-Ref.refdef.vieworg[1]) * Ref.forward[1] + (e->origin[2]-Ref.refdef.vieworg[2]) * Ref.forward[2];
	if( dist < 0 ) return; // cull it because we don't want to sort unneeded things

	frame = R_GetSpriteFrame( e );

	if( Ref.refdef.rdflags & (RDF_PORTALINVIEW|RDF_SKYPORTALINVIEW) || (Ref.params & RP_SKYPORTALVIEW))
	{
		if( R_VisCullSphere( e->origin, frame->radius ))
			return;
	}

	mb = R_AddMeshToList( MESH_MODEL, R_FogForSphere( e->origin, frame->radius ), frame->shader, -1 );
	if( mb ) mb->shaderKey |= (bound( 1, 0x4000 - (uint)dist, 0x4000 - 1 )<<12 );
}

/*
=================
R_AddSpritePolyToList
=================
*/
static void R_AddSpritePolyToList( ref_entity_t *e )
{
	float		dist;
	meshbuffer_t	*mb;

	dist = (e->origin[0] - Ref.refdef.vieworg[0]) * Ref.forward[0] + (e->origin[1] - Ref.refdef.vieworg[1]) * Ref.forward[1] + (e->origin[2] - Ref.refdef.vieworg[2]) * Ref.forward[2];
	if( dist < 0 ) return; // cull it because we don't want to sort unneeded things

	if( Ref.refdef.rdflags & (RDF_PORTALINVIEW|RDF_SKYPORTALINVIEW) || (Ref.params & RP_SKYPORTALVIEW))
	{
		if( R_VisCullSphere( e->origin, e->radius ))
			return;
	}

	mb = R_AddMeshToList( MESH_SPRITE, R_FogForSphere( e->origin, e->radius ), e->shader, -1 );
	if( mb ) mb->shaderKey |= ( bound( 1, 0x4000 - (uint)dist, 0x4000 - 1 )<<12 );
}

/*
=================
R_SpriteOverflow
=================
*/
bool R_SpriteOverflow( void )
{
	return R_MeshOverflow( &spr_mesh );
}

//==================================================================================

/*
============
R_PolyBlend
============
*/
static void R_PolyBlend( void )
{
	if( !r_polyblend->integer ) return;
	if( Ref.refdef.blend[3] < 0.01f ) return;

	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();
	pglOrtho( 0, 1, 1, 0, -99999, 99999 );

	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	GL_CullFace( GL_NONE );
	GL_SetState( GLSTATE_NO_DEPTH_TEST|GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	pglDisable( GL_TEXTURE_2D );
	pglColor4fv( Ref.refdef.blend );

	pglBegin( GL_TRIANGLES );
	pglVertex2f( -5, -5 );
	pglVertex2f( 10, -5 );
	pglVertex2f( -5, 10 );
	pglEnd();

	pglEnable( GL_TEXTURE_2D );

	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
}

/*
===============
R_ApplySoftwareGamma
===============
*/
static void R_ApplySoftwareGamma( void )
{
	double	f, div;

	// apply software gamma
	if( r_hwgamma->integer ) return;

	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();
	pglOrtho( 0, 1, 1, 0, -99999, 99999 );

	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	GL_CullFace( GL_NONE );
	GL_SetState( GLSTATE_NO_DEPTH_TEST | GLSTATE_SRCBLEND_DST_COLOR | GLSTATE_DSTBLEND_ONE );

	pglDisable( GL_TEXTURE_2D );

	if( r_overbrightbits->integer > 0 )
		div = 0.5 * (double)( 1 << r_overbrightbits->integer );
	else div = 0.5;

	f = div + vid_gamma->value;
	f = bound( 0.1, f, 5.0 );

	pglBegin( GL_TRIANGLES );

	while( f >= 1.01f )
	{
		if( f >= 2 ) pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		else pglColor4f( f - 1.0f, f - 1.0f, f - 1.0f, 1.0f );

		pglVertex2f( -5, -5 );
		pglVertex2f( 10, -5 );
		pglVertex2f( -5, 10 );
		f *= 0.5;
	}

	pglEnd();
	pglEnable( GL_TEXTURE_2D );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
}

/*
=================
R_DrawBeam
=================
*/
void R_DrawBeam( void )
{
#if 0
	matrix3x3		axis;
	float		length;
	int		i;

	// find orientation vectors
	VectorSubtract( Ref.refdef.vieworg, Ref.m_pCurrentEntity->origin, axis[0] );
	VectorSubtract( Ref.m_pCurrentEntity->prev.origin, Ref.m_pCurrentEntity->origin, axis[1] );// FIXME

	CrossProduct( axis[0], axis[1], axis[2] );
	VectorNormalizeFast( axis[2] );

	// find normal
	CrossProduct( axis[1], axis[2], axis[0] );
	VectorNormalizeFast( axis[0] );

	// scale by radius
	VectorScale( axis[2], Ref.m_pCurrentEntity->frame / 2, axis[2] );

	// find segment length
	length = VectorLength( axis[1] ) / Ref.m_pCurrentEntity->prev.frame;

	// draw it
	RB_CheckMeshOverflow( 6, 4 );
	
	for( i = 2; i < 4; i++ )
	{
		indexArray[r_stats.numIndices++] = r_stats.numVertices + 0;
		indexArray[r_stats.numIndices++] = r_stats.numVertices + i-1;
		indexArray[r_stats.numIndices++] = r_stats.numVertices + i;
	}

	vertexArray[r_stats.numVertices+0][0] = Ref.m_pCurrentEntity->origin[0] + axis[2][0];
	vertexArray[r_stats.numVertices+0][1] = Ref.m_pCurrentEntity->origin[1] + axis[2][1];
	vertexArray[r_stats.numVertices+0][2] = Ref.m_pCurrentEntity->origin[2] + axis[2][2];
	vertexArray[r_stats.numVertices+1][0] = Ref.m_pCurrentEntity->prev.origin[0] + axis[2][0];
	vertexArray[r_stats.numVertices+1][1] = Ref.m_pCurrentEntity->prev.origin[1] + axis[2][1];
	vertexArray[r_stats.numVertices+1][2] = Ref.m_pCurrentEntity->prev.origin[2] + axis[2][2];
	vertexArray[r_stats.numVertices+2][0] = Ref.m_pCurrentEntity->prev.origin[0] - axis[2][0];
	vertexArray[r_stats.numVertices+2][1] = Ref.m_pCurrentEntity->prev.origin[1] - axis[2][1];
	vertexArray[r_stats.numVertices+2][2] = Ref.m_pCurrentEntity->prev.origin[2] - axis[2][2];
	vertexArray[r_stats.numVertices+3][0] = Ref.m_pCurrentEntity->origin[0] - axis[2][0];
	vertexArray[r_stats.numVertices+3][1] = Ref.m_pCurrentEntity->origin[1] - axis[2][1];
	vertexArray[r_stats.numVertices+3][2] = Ref.m_pCurrentEntity->origin[2] - axis[2][2];

	inTexCoordArray[r_stats.numVertices+0][0] = 0;
	inTexCoordArray[r_stats.numVertices+0][1] = 0;
	inTexCoordArray[r_stats.numVertices+1][0] = length;
	inTexCoordArray[r_stats.numVertices+1][1] = 0;
	inTexCoordArray[r_stats.numVertices+2][0] = length;
	inTexCoordArray[r_stats.numVertices+2][1] = 1;
	inTexCoordArray[r_stats.numVertices+3][0] = 0;
	inTexCoordArray[r_stats.numVertices+3][1] = 1;

	for( i = 0; i < 4; i++ )
	{
		normalArray[r_stats.numVertices][0] = axis[0][0];
		normalArray[r_stats.numVertices][1] = axis[0][1];
		normalArray[r_stats.numVertices][2] = axis[0][2];
		inColorArray[0][r_stats.numVertices][0] = Ref.m_pCurrentEntity->rendercolor[0];
		inColorArray[0][r_stats.numVertices][1] = Ref.m_pCurrentEntity->rendercolor[1];
		inColorArray[0][r_stats.numVertices][2] = Ref.m_pCurrentEntity->rendercolor[2];
		inColorArray[0][r_stats.numVertices][3] = Ref.m_pCurrentEntity->renderamt;
		r_stats.numVertices++;
	}
#endif
}
// =====================================================================

/*
=================
R_SetupFrustum
=================
*/
static void R_SetupFrustum( void )
{
	int	i;
	vec3_t	farPoint;

	// 0 - left
	// 1 - right
	// 2 - down
	// 3 - up
	// 4 - farclip

	RotatePointAroundVector( Ref.frustum[0].normal, Ref.up, Ref.forward, -(90 - Ref.refdef.fov_x / 2 ));
	RotatePointAroundVector( Ref.frustum[1].normal, Ref.up, Ref.forward, 90 - Ref.refdef.fov_x / 2 );
	RotatePointAroundVector( Ref.frustum[2].normal, Ref.right, Ref.forward, 90 - Ref.refdef.fov_y / 2 );
	RotatePointAroundVector( Ref.frustum[3].normal, Ref.right, Ref.forward, -(90 - Ref.refdef.fov_y / 2 ));
	VectorNegate( Ref.forward, Ref.frustum[4].normal );

	for( i = 0; i < 4; i++ )
	{
		Ref.frustum[i].dist = DotProduct( Ref.vieworg, Ref.frustum[i].normal );
		PlaneClassify( &Ref.frustum[i] );
	}

	VectorMA( Ref.vieworg, Ref.farClip, Ref.forward, farPoint );
	Ref.frustum[i].dist = DotProduct( farPoint, Ref.frustum[i].normal );
	PlaneClassify( &Ref.frustum[i] );
}

/*
=================
R_SetFarClip
=================
*/
static float R_SetFarClip( void )
{
	float	farDist;

	if( r_worldModel && !( Ref.refdef.rdflags & RDF_NOWORLDMODEL ))
	{
		int	i;
		float	dist;
		vec3_t	tmp;

		farDist = 0;
		for( i = 0; i < 8; i++ )
		{
			tmp[0] = (( i & 1 ) ? Ref.visMins[0] : Ref.visMaxs[0] );
			tmp[1] = (( i & 2 ) ? Ref.visMins[1] : Ref.visMaxs[1] );
			tmp[2] = (( i & 4 ) ? Ref.visMins[2] : Ref.visMaxs[2] );

			dist = VectorDistance2( tmp, Ref.vieworg );
			farDist = max( farDist, dist );
		}

		farDist = sqrt( farDist );

		if( r_worldBrushModel->globalfog )
		{
			float	fogdist = r_worldBrushModel->globalfog->shader->fogDist;
			if( farDist > fogdist ) farDist = fogdist;
			else Ref.clipFlags &= ~16;
		}
	}
	else farDist = 2048.0f;	// const

	return max( r_farclip_min, farDist ) + r_farclip_bias;
}

/*
=============
R_SetupProjectionMatrix
=============
*/
void R_SetupProjectionMatrix( const refdef_t *rd, matrix4x4 out )
{
	GLdouble	xMin, xMax, yMin, yMax, zNear, zFar;

	if( rd->rdflags & RDF_NOWORLDMODEL )
		Ref.farClip = 2048.0;
	else Ref.farClip = R_SetFarClip();

	zNear = Z_NEAR;
	zFar = Ref.farClip;

	yMax = zNear * tan( rd->fov_y * M_PI / 360.0 );
	yMin = -yMax;

	xMax = zNear * tan( rd->fov_x * M_PI / 360.0 );
	xMin = -xMax;

// FIXME: hack
#ifdef OPENGL_STYLE
	out[0][0] = ( 2.0 * zNear ) / ( xMax - xMin );
	out[0][1] = 0.0f;
	out[0][2] = 0.0f;
	out[0][3] = 0.0f;
	out[1][0] = 0.0f;
	out[1][1] = ( 2.0 * zNear ) / ( yMax - yMin );
	out[1][2] = 0.0f;
	out[1][3] = 0.0f;
	out[2][0] = ( xMax + xMin ) / ( xMax - xMin );
	out[2][1] = ( yMax + yMin ) / ( yMax - yMin );
	out[2][2] = -( zFar + zNear ) / ( zFar - zNear );
	out[2][3] = -1.0f;
	out[3][0] = 0.0f;
	out[3][1] = 0.0f;
	out[3][2] = -( 2.0 * zFar * zNear ) / ( zFar - zNear );
	out[3][3] = 0.0f;
#else
	out[0][0] = ( 2.0 * zNear ) / ( xMax - xMin );
	out[1][0] = 0.0f;
	out[2][0] = 0.0f;
	out[3][0] = 0.0f;
	out[0][1] = 0.0f;
	out[1][1] = ( 2.0 * zNear ) / ( yMax - yMin );
	out[2][1] = 0.0f;
	out[3][1] = 0.0f;
	out[0][2] = ( xMax + xMin ) / ( xMax - xMin );
	out[1][2] = ( yMax + yMin ) / ( yMax - yMin );
	out[2][2] = -( zFar + zNear ) / ( zFar - zNear );
	out[3][2] = -1.0f;
	out[0][3] = 0.0f;
	out[1][3] = 0.0f;
	out[2][3] = -( 2.0 * zFar * zNear ) / ( zFar - zNear );
	out[3][3] = 0.0f;
#endif
}

/*
=============
R_SetupModelviewMatrix
=============
*/
void R_SetupModelViewMatrix( const refdef_t *rd, matrix4x4 out )
{
#if 1	
	// g-cont. Debug version enabled!!!
	// classic slow version (used for debug)
	Matrix4x4_LoadIdentity( out );
	Matrix4x4_ConcatRotate( out, -90, 1, 0, 0 );	    // put Z going up
	Matrix4x4_ConcatRotate( out,	90, 0, 0, 1 );	    // put Z going up
	Matrix4x4_ConcatRotate( out, -rd->viewangles[2],  1, 0, 0 );
	Matrix4x4_ConcatRotate( out, -rd->viewangles[0],  0, 1, 0 );
	Matrix4x4_ConcatRotate( out, -rd->viewangles[1],  0, 0, 1 );
	Matrix4x4_ConcatTranslate( out, -rd->vieworg[0],  -rd->vieworg[1],  -rd->vieworg[2] );
#else
	// FIXME: get values from rd, not Ref! these values will be valid only for first pass 
	Matrix4x4_CreateModelview_FromAxis( out, Ref.forward, Ref.right, Ref.up, Ref.origin );
#endif
}

/*
===============
R_SetupFrame
===============
*/
static void R_SetupFrame( void )
{
	mleaf_t	*leaf;

	// build the transformation matrix for the given view angles
	VectorCopy( Ref.refdef.vieworg, Ref.vieworg );
	AngleVectors( Ref.refdef.viewangles, Ref.forward, Ref.right, Ref.up );

	if( Ref.params & RP_SHADOWMAPVIEW ) 
		return;

	// go into 3D mode
	// FIXME: find right place
	// GL_Setup3D();
	
	r_frameCount++;

	// current viewcluster
	if(!( Ref.refdef.rdflags & RDF_NOWORLDMODEL ))
	{
		VectorCopy( r_worldModel->mins, Ref.visMins );
		VectorCopy( r_worldModel->maxs, Ref.visMaxs );

		if(!( Ref.params & RP_OLDVIEWCLUSTER ))
		{
			r_oldViewCluster = r_viewCluster;
			leaf = R_PointInLeaf( Ref.pvsOrigin );
			r_viewCluster = leaf->cluster;
		}
	}
}

/*
===============
R_SetupViewMatrices
===============
*/
static void R_SetupViewMatrices( void )
{
	R_SetupModelViewMatrix( &Ref.refdef, Ref.worldMatrix );

	if( Ref.params & RP_SHADOWMAPVIEW )
	{
		int		i;
		float		x1, x2, y1, y2;
		int		ix1, ix2, iy1, iy2;
		int		sizex = Ref.refdef.rect.width;
		int		sizey = Ref.refdef.rect.height;
		int		diffx, diffy;
		shadowGroup_t	*group = Ref.shadowGroup;

		R_SetupProjectionMatrix( &Ref.refdef, Ref.projectionMatrix );
		Matrix4x4_Concat( Ref.worldProjectionMatrix, Ref.projectionMatrix, Ref.worldMatrix );

		// compute optimal fov to increase depth precision (so that shadow group objects are
		// as close to the nearplane as possible)
		// note that it's suboptimal to use bbox calculated in worldspace (FIXME)
		x1 = y1 = 999999;
		x2 = y2 = -999999;

		for( i = 0; i < 8; i++ )
		{
			vec3_t v, tmp;

			// compute and rotate a full bounding box
			tmp[0] = (( i & 1 ) ? group->mins[0] : group->maxs[0] );
			tmp[1] = (( i & 2 ) ? group->mins[1] : group->maxs[1] );
			tmp[2] = (( i & 4 ) ? group->mins[2] : group->maxs[2] );

			// transform to screen
			R_TransformWorldToScreen( tmp, v );
			x1 = min( x1, v[0] ); y1 = min( y1, v[1] );
			x2 = max( x2, v[0] ); y2 = max( y2, v[1] );
		}

		// give it 1 pixel gap on both sides
		ix1 = x1 - 1.0f;
		ix2 = x2 + 1.0f;
		iy1 = y1 - 1.0f;
		iy2 = y2 + 1.0f;

		diffx = sizex - min( ix1, sizex - ix2 ) * 2;
		diffy = sizey - min( iy1, sizey - iy2 ) * 2;

		// adjust fov
		Ref.refdef.fov_x = 2 * RAD2DEG( atan((float)diffx / (float)sizex ));
		Ref.refdef.fov_y = 2 * RAD2DEG( atan((float)diffy / (float)sizey ));
	}

	R_SetupProjectionMatrix( &Ref.refdef, Ref.projectionMatrix );
	if( Ref.params & RP_MIRRORVIEW ) Ref.projectionMatrix[0][0] = -Ref.projectionMatrix[0][0];
	Matrix4x4_Concat( Ref.worldProjectionMatrix, Ref.projectionMatrix, Ref.worldMatrix );
}

/*
=============
R_Clear
=============
*/
static void R_Clear( int bitMask )
{
	int	bits;

	bits = GL_DEPTH_BUFFER_BIT;

	if(!( Ref.refdef.rdflags & RDF_NOWORLDMODEL ) && r_fastsky->integer )
		bits |= GL_COLOR_BUFFER_BIT;
	if( r_shadows->integer >= SHADOW_PLANAR )
		bits |= GL_STENCIL_BUFFER_BIT;

	bits &= bitMask;

	if( bits & GL_STENCIL_BUFFER_BIT )
		pglClearStencil( 128 );

	if( bits & GL_COLOR_BUFFER_BIT )
	{
		vec3_t	color;
		
		if( r_worldModel && !( Ref.refdef.rdflags & RDF_NOWORLDMODEL ) && r_worldBrushModel->globalfog )
			VectorCopy( r_worldBrushModel->globalfog->shader->fogColor, color );
		else VectorSet( color, 1.0f, 0.0f, 0.5f );

		pglClearColor( color[0], color[1], color[2], 1.0f );
	}

	pglClear( bits );

	gldepthmin = 0;
	gldepthmax = 1;
	pglDepthRange( gldepthmin, gldepthmax );
}

/*
=============
R_SetupGL
=============
*/
static void R_SetupGL( void )
{
	pglScissor( Ref.scissor[0], Ref.scissor[1], Ref.scissor[2], Ref.scissor[3] );
	pglViewport( Ref.viewport[0], Ref.viewport[1], Ref.viewport[2], Ref.viewport[3] );

	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( Ref.projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( Ref.worldMatrix );

	if( Ref.params & RP_CLIPPLANE )
	{
		GLdouble	clip[4];
		cplane_t	*p = &Ref.clipPlane;

		clip[0] = p->normal[0];
		clip[1] = p->normal[1];
		clip[2] = p->normal[2];
		clip[3] = -p->dist;

		pglClipPlane( GL_CLIP_PLANE0, clip );
		pglEnable( GL_CLIP_PLANE0 );
	}

	if( Ref.params & RP_FLIPFRONTFACE )
		GL_FrontFace( !gl_state.frontFace );

	if( Ref.params & RP_SHADOWMAPVIEW )
	{
		pglShadeModel( GL_FLAT );
		pglColorMask( 0, 0, 0, 0 );
		pglPolygonOffset( 1, 4 );
		if( oldRef.params & RP_CLIPPLANE )
			pglDisable( GL_CLIP_PLANE0 );
	}

	GL_CullFace( GL_FRONT );
	GL_SetState( GLSTATE_DEPTHWRITE );
}

/*
=============
R_EndGL
=============
*/
static void R_EndGL( void )
{
	if( Ref.params & RP_SHADOWMAPVIEW )
	{
		pglPolygonOffset( -1, -2 );
		pglColorMask( 1, 1, 1, 1 );
		pglShadeModel( GL_SMOOTH );
		if( oldRef.params & RP_CLIPPLANE )
			pglEnable( GL_CLIP_PLANE0 );
	}

	if( Ref.params & RP_FLIPFRONTFACE )
		GL_FrontFace( !gl_state.frontFace );

	if( Ref.params & RP_CLIPPLANE )
		pglDisable( GL_CLIP_PLANE0 );
}

/*
=============
R_CategorizeEntities
=============
*/
static void R_CategorizeEntities( void )
{
	uint	i;

	r_numNullEntities = 0;
	r_numBmodelEntities = 0;

	if( !r_drawentities->integer )
		return;

	for( i = 1; i < r_numEntities; i++ )
	{
		Ref.m_pPrevEntity = Ref.m_pCurrentEntity;
		Ref.m_pCurrentEntity = &r_entities[i];

		switch( Ref.m_pCurrentEntity->ent_type )
		{
		case ED_NORMAL:
		case ED_CLIENT:
		case ED_BSPBRUSH:
		case ED_VIEWMODEL:
			break;
		default:
			continue;
		}

		Ref.m_pCurrentModel = Ref.m_pCurrentEntity->model;
		if( !Ref.m_pCurrentModel )
		{
			r_nullEntities[r_numNullEntities++] = Ref.m_pCurrentEntity;
			continue;
		}

		switch( Ref.m_pCurrentModel->type )
		{
		case mod_brush:
			r_bmodelEntities[r_numBmodelEntities++] = Ref.m_pCurrentEntity;
			break;
		case mod_studio:
			// FIXME: shadows not implemented
			// if(!( Ref.m_pCurrentEntity->renderfx & ( RF_NOSHADOW|RF_PLANARSHADOW )))
			//	R_AddShadowCaster( Ref.m_pCurrentEntity ); // build groups and mark shadow casters
			break;
		case mod_sprite:
			break;
		default:
			Host_Error( "%s: bad modeltype", Ref.m_pCurrentModel->name );
			break;
		}
	}
}

/*
=============
R_CullEntities
=============
*/
static void R_CullEntities( void )
{
	uint		i;
	ref_entity_t	*e;
	bool		culled;

	memset( r_entVisBits, 0, sizeof( r_entVisBits ));
	if( !r_drawentities->integer ) return;

	for( i = 1; i < r_numEntities; i++ )
	{
		Ref.m_pPrevEntity = Ref.m_pCurrentEntity;
		Ref.m_pCurrentEntity = e = &r_entities[i];
		culled = true;

		switch( e->ent_type )
		{
		case ED_NORMAL:
		case ED_CLIENT:
		case ED_BSPBRUSH:
		case ED_VIEWMODEL:
			if( !e->model ) break;
			switch( e->model->type )
			{
			case mod_studio:
				culled = R_CullStudioModel( e );
				break;
			case mod_brush:
				culled = R_CullBrushModel( e );
				break;
			case mod_sprite:
				culled = false;
				break;
			default:	break;
			}
			break;
		case ED_TEMPENTITY:
			culled = ( e->radius <= 0 );
			break;
		default:	break;
		}

		if( !culled ) r_entVisBits[i>>3] |= ( 1<<(i & 7));
	}
}

/*
=============
R_DrawNullModel
=============
*/
static void R_DrawNullModel( void )
{
	vec3_t		points[3];
	ref_entity_t	*entity;

	entity = Ref.m_pCurrentEntity;

	if( !entity ) return;
	VectorMA( entity->origin, 15, entity->matrix[0], points[0] );
	VectorMA( entity->origin, -15, entity->matrix[1], points[1] );
	VectorMA( entity->origin, 15, entity->matrix[2], points[2] );

	pglBegin( GL_LINES );

	pglColor4f( 1.0f, 0.0f, 0.0f, 0.5f );
	pglVertex3fv( entity->origin );
	pglVertex3fv( points[0] );

	pglColor4f( 0, 1.0f, 0, 0.5f );
	pglVertex3fv( entity->origin );
	pglVertex3fv( points[1] );

	pglColor4f( 0, 0, 1.0f, 0.5f );
	pglVertex3fv( entity->origin );
	pglVertex3fv( points[2] );

	pglEnd();
}

/*
=============
R_DrawBmodelEntities
=============
*/
static void R_DrawBmodelEntities( void )
{
	int	i, j;

	for( i = 0; i < r_numBmodelEntities; i++ )
	{
		Ref.m_pPrevEntity = Ref.m_pCurrentEntity;
		Ref.m_pCurrentEntity = r_bmodelEntities[i];
		j = Ref.m_pCurrentEntity - r_entities;

		if( r_entVisBits[j>>3] & (1<<( j&7 )))
			R_AddBrushModelToList( Ref.m_pCurrentEntity );
	}
}

/*
=============
R_DrawRegularEntities
=============
*/
static void R_DrawRegularEntities( void )
{
	uint		i;
	ref_entity_t	*e;
	bool		shadowmap = (( Ref.params & RP_SHADOWMAPVIEW ) != 0 );

	for( i = 1; i < r_numEntities; i++ )
	{
		Ref.m_pPrevEntity = Ref.m_pCurrentEntity;
		Ref.m_pCurrentEntity = e = &r_entities[i];

		if( shadowmap )
		{
			if( e->renderfx & RF_NOSHADOW ) continue;
			if( r_entShadowBits[i] & Ref.shadowGroup->bit )
				goto add; // shadow caster
		}
		if(!( r_entVisBits[i>>3] & (1<<( i&7 ))))
			continue;

add:
		switch( e->ent_type )
		{
		case ED_NORMAL:
		case ED_CLIENT:
		case ED_BSPBRUSH:
		case ED_VIEWMODEL:
			Ref.m_pCurrentModel = e->model;
			switch( Ref.m_pCurrentModel->type )
			{
			case mod_studio:
				R_AddStudioModelToList( e );
				break;
			case mod_sprite:
				if( !shadowmap )
					R_AddSpriteModelToList( e );
				break;
			default:	break;
			}
			break;
		case ED_TEMPENTITY:
			if( !shadowmap )
				R_AddSpritePolyToList( e );
			break;
		default:	break;
		}
	}
}

/*
=============
R_DrawNullEntities
=============
*/
static void R_DrawNullEntities( void )
{
	int	i;

	if( !r_numNullEntities )
		return;

	pglDisable( GL_TEXTURE_2D );
	GL_SetState( GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	// draw non-transparent first
	for( i = 0; i < r_numNullEntities; i++ )
	{
		Ref.m_pPrevEntity = Ref.m_pCurrentEntity;
		Ref.m_pCurrentEntity = r_nullEntities[i];

		if( Ref.params & RP_MIRRORVIEW )
		{
			if( Ref.m_pCurrentEntity->renderfx & RF_VIEWMODEL )
				continue;
		}
		else
		{
			if( Ref.m_pCurrentEntity->renderfx & RF_PLAYERMODEL )
				continue;
		}
		R_DrawNullModel();
	}
	pglEnable( GL_TEXTURE_2D );
}

/*
=============
R_DrawEntities
=============
*/
static void R_DrawEntities( void )
{
	bool	shadowmap = (( Ref.params & RP_SHADOWMAPVIEW ) != 0 );

	if( !r_drawentities->integer ) return;
	if( !shadowmap )
	{
		R_CullEntities();		// mark visible entities in r_entVisBits
		// FIXME: shadows not implemented
		// R_CullShadowmapGroups();
	}

	// we don't mark bmodel entities in RP_SHADOWMAPVIEW, only individual surfaces
	R_DrawBmodelEntities();

	if( OCCLUSION_QUERIES_ENABLED( Ref ))
	{
		R_EndOcclusionPass();
	}

	// because we want render scene only with static models
	if( Ref.params & RP_ENVVIEW ) return;

	// FIXME: shadows not implemented
	/*if( !shadowmap ) R_DrawShadowmaps(); // render to depth textures, mark shadowed entities and surfaces
	else*/ if(!( Ref.params & RP_WORLDSURFVISIBLE ) || ( oldRef.shadowBits & Ref.shadowGroup->bit ))
		return; // we're supposed to cast shadows but there are no visible surfaces for this light, so stop
	// or we've already drawn and captured textures for this group

	R_DrawRegularEntities();
}

/*
=================
R_RenderView
=================
*/
void R_RenderView( const refdef_t *fd )
{
	bool	shadowMap = Ref.params & RP_SHADOWMAPVIEW ? true : false;

	Ref.refdef = *fd;

	R_ClearMeshList( Ref.meshlist );
	R_SetupFrame();

	// we know the farclip so adjust fov before setting up the frustum
	if( shadowMap )
	{
		R_SetupViewMatrices();
	}
	else if( OCCLUSION_QUERIES_ENABLED( Ref ))
	{
		R_SetupViewMatrices();
		R_SetupGL();
		R_Clear( ~( GL_STENCIL_BUFFER_BIT|GL_COLOR_BUFFER_BIT ));
		R_BeginOcclusionPass();
	}

	R_SetupFrustum();

	R_MarkLeaves();
	R_DrawWorld();

	// we know the the farclip at this point after determining visible world leafs
	if( !shadowMap )
	{
		R_SetupViewMatrices();
		R_DrawCoronas();

		R_AddPolysToList();
	}

	R_DrawEntities();

	if( shadowMap )
	{
		if( !( Ref.params & RP_WORLDSURFVISIBLE ) )
			return; // we didn't cast shadows on anything, so stop
		if( oldRef.shadowBits & Ref.shadowGroup->bit )
			return; // already drawn
	}

	R_SortMeshes();
	R_DrawPortals();

	R_SetupGL();
	R_Clear( shadowMap ? ~(GL_STENCIL_BUFFER_BIT|GL_COLOR_BUFFER_BIT) : ~0 );

	R_DrawMeshes();

	R_CleanUpTextureUnits();
	RB_DrawTriangleOutlines( r_showtris->integer ? true : false, r_shownormals->integer ? true : false );
	R_DrawNullEntities();
	RB_DebugGraphics();

	R_EndGL();
}

void R_DrawPauseScreen( void )
{
	// don't apply post effects for custom window
	if(r_refdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if( !r_pause_bw->integer )
		return;

	if( r_pause->modified )
	{
		// reset saturation value
		if( !r_pause->value )
			r_pause_alpha = 0.0f;
		r_pause->modified = false;
	}
	if( !r_pause->value ) return;          
	if( r_pause_alpha < 1.0f ) r_pause_alpha += 0.03f;

	if( r_pause_alpha <= 1.0f || r_lefthand->modified )
	{
		int	k = r_pause_alpha * 255.0f;
		int	i, s, r, g, b;

		pglFlush();
		pglReadPixels(0, 0, r_width->integer, r_height->integer, GL_RGB, GL_UNSIGNED_BYTE, r_framebuffer);
		for (i = 0; i < r_width->integer * r_height->integer * 3; i+=3)
		{
			r = r_framebuffer[i+0];
			g = r_framebuffer[i+1];
			b = r_framebuffer[i+2];
			s = (r + 2 * g + b) * k>>2; // simply bw recomputing
			r_framebuffer[i+0] = (r*(255-k)+s)>>8;
			r_framebuffer[i+1] = (g*(255-k)+s)>>8;
			r_framebuffer[i+2] = (b*(255-k)+s)>>8;
		}
		r_lefthand->modified = false;
	}
	// set custom orthogonal mode
	pglMatrixMode(GL_PROJECTION);
	pglLoadIdentity ();
	pglOrtho(0, r_width->integer, 0, r_height->integer, 0, 1.0f);
	pglMatrixMode(GL_MODELVIEW);
	pglLoadIdentity ();

	pglDisable(GL_TEXTURE_2D);
	pglRasterPos2f(0, 0);
	pglDrawPixels(r_width->integer, r_height->integer, GL_RGB, GL_UNSIGNED_BYTE, r_framebuffer);
	pglFlush();
	pglEnable(GL_TEXTURE_2D);
}

/*
=================
R_ClearScene
=================
*/
void R_ClearScene( void )
{
	r_numEntities = 1;
	r_numDLights = 0;
	r_numParticles = 0;
	r_numPolys = 0;

	Ref.m_pPrevEntity = NULL;
	Ref.m_pCurrentEntity = r_worldEntity;
	Ref.m_pCurrentModel = r_worldModel;
}

/*
====================
R_SetLightLevel

HACKHACK
====================
*/
void R_SetLightLevel( void )
{
	vec3_t	temp, shadelight;

	if( r_refdef.rdflags & RDF_NOWORLDMODEL )
		return;

	// save off light value for server to look at (BIG HACK!)
	R_LightForPoint( Ref.vieworg, temp, shadelight, NULL, 0 );

	// pick the greatest component, which should be the same
	// as the mono value returned by software
	if( shadelight[0] > shadelight[1] )
	{
		if( shadelight[0] > shadelight[2] )
			r_lightlevel->value = 150 * shadelight[0];
		else r_lightlevel->value = 150 * shadelight[2];
	}
	else
	{
		if( shadelight[1] > shadelight[2] )
			r_lightlevel->value = 150 * shadelight[1];
		else r_lightlevel->value = 150 * shadelight[2];
	}

}

/*
=================
R_AddEntityToScene
=================
*/
static bool R_AddEntityToScene( entity_state_t *s1, entity_state_t *s2, float lerpfrac )
{
	ref_entity_t	*refent;
	int		i;

	if( !s1 || !s1->model.index ) return false; // if set to invisible, skip
	if( r_numEntities >= MAX_ENTITIES ) return false;

	refent = &r_entities[r_numEntities];
	if( !s2 ) s2 = s1; // no lerping state

	// copy state to render
	refent->frame = s1->model.frame;
	refent->index = s1->number;
	refent->ent_type = s1->ed_type;
	refent->backlerp = 1.0f - lerpfrac;
	refent->renderamt = s1->renderamt;
	refent->body = s1->model.body;
	refent->sequence = s1->model.sequence;		
	refent->movetype = s1->movetype;
	refent->scale = s1->model.scale ? s1->model.scale : 1.0f;
	refent->colormap = s1->model.colormap;
	refent->framerate = s1->model.framerate;
	refent->effects = s1->effects;
	refent->animtime = s1->model.animtime;
	VectorCopy( s1->rendercolor, refent->rendercolor );

	// setup latchedvars
	refent->prev.frame = s2->model.frame;
	refent->prev.animtime = s2->model.animtime;
	VectorCopy( s2->origin, refent->prev.origin );
	VectorCopy( s2->angles, refent->prev.angles );
	refent->prev.sequence = s2->model.sequence;
		
	// interpolate origin
	for( i = 0; i < 3; i++ )
		refent->origin[i] = LerpPoint( s2->origin[i], s1->origin[i], lerpfrac );

	// set skin
	refent->skin = s1->model.skin;
	refent->model = cl_models[s1->model.index];
	refent->weaponmodel = cl_models[s1->pmodel.index];
	refent->renderfx = s1->renderfx;
	refent->prev.sequencetime = s1->model.animtime - s2->model.animtime;

	// calculate angles
	if( refent->effects & EF_ROTATE )
	{	
		// some bonus items auto-rotate
		VectorSet( refent->angles, 0, anglemod( r_refdef.time / 10), 0 );
	}
	else
	{	
		// interpolate angles
		for( i = 0; i < 3; i++ )
			refent->angles[i] = LerpAngle( s2->angles[i], s1->angles[i], lerpfrac );
	}

	Matrix3x3_FromAngles( refent->angles, refent->matrix );

	// copy controllers
	for( i = 0; i < MAXSTUDIOCONTROLLERS; i++ )
	{
		refent->controller[i] = s1->model.controller[i];
		refent->prev.controller[i] = s2->model.controller[i];
	}

	// copy blends
	for( i = 0; i < MAXSTUDIOBLENDS; i++ )
	{
		refent->blending[i] = s1->model.blending[i];
		refent->prev.blending[i] = s2->model.blending[i];
	}

	if( refent->ent_type == ED_CLIENT )
	{
		// only draw from mirrors
		refent->renderfx |= RF_PLAYERMODEL;
		refent->gaitsequence = s1->model.gaitsequence;
	}

	// because entity without models never added to scene
	if( !refent->ent_type )
	{
		switch( refent->model->type )
		{
		case mod_brush:
			refent->ent_type = ED_BSPBRUSH;
			break;
		case mod_studio:
		case mod_sprite:		
			refent->ent_type = ED_NORMAL;
          		break;
		// and ignore all other unset ents
		}
	}

	if( !refent->shader ) refent->shader = r_defaultShader;

	// add entity
	r_numEntities++;

	return true;
}

/*
=================
R_AddLightToScene
=================
*/
static bool R_AddDynamicLight( vec3_t org, vec3_t color, float intensity )
{
	dlight_t	*dl;

	if( r_numDLights >= MAX_DLIGHTS )
		return false;

	dl = &r_dlights[r_numDLights++];
	VectorCopy( org, dl->origin );
	VectorCopy( color, dl->color );
	dl->intensity = intensity * DLIGHT_SCALE;

	// FIXME
	dl->shader = r_defaultShader;
	R_LightBounds( org, dl->intensity, dl->mins, dl->maxs );

	return true;
}

/*
=====================
R_AddPolyToScene
=====================
*/
static bool R_AddPolyToScene( const poly_t *poly )
{
	poly_t	*dp; 

	if( r_numPolys >= MAX_POLYS )
		return false;

	dp = &r_polys[r_numPolys++];
	*dp = *poly;

	if( dp->numverts > MAX_POLY_VERTS )
		dp->numverts = MAX_POLY_VERTS;
}

/*
=================
R_AddParticleToScene
=================
*/
bool R_AddParticleToScene( const vec3_t origin, float alpha, int color )
{
	particle_t	*p;

	if( r_numParticles >= MAX_PARTICLES )
		return false;

	p = &r_particles[r_numParticles];

	p->shader = r_defaultShader;
	VectorCopy( origin, p->origin );
	VectorCopy( origin, p->old_origin );
	p->radius = 5;
	p->length = alpha;
	p->rotation = 0;
	Vector4Set( p->modulate, 1.0f, 1.0f, 1.0f, 1.0f );
	r_numParticles++;

	return true;
}

static bool R_AddLightStyle( int style, vec3_t color )
{
	lightstyle_t	*ls;

	if( style < 0 || style > MAX_LIGHTSTYLES )
		return false; // invalid lightstyle

	ls = &r_lightStyles[style];
	ls->white = color[0] + color[1] + color[2];
	VectorCopy( color, ls->rgb );

	return true;
}

/*
=================
R_RenderScene
=================
*/
void R_RenderScene( refdef_t *fd )
{
	if( r_norefresh->integer )
		return;

	RB_StartFrame();

	if(!( fd->rdflags & RDF_NOWORLDMODEL ) )
	{
		if( !r_worldModel ) Host_Error( "R_RenderScene: NULL worldmodel\n" );
		r_lastRefdef = *fd;
	}

	Ref.params = RP_NONE;
	Ref.refdef = *fd;
	Ref.farClip = 0;
	Ref.clipFlags = MAX_CLIPFLAGS;

	if( r_worldModel && !( Ref.refdef.rdflags & RDF_NOWORLDMODEL ) && r_worldBrushModel->globalfog )
	{
		Ref.farClip = r_worldBrushModel->globalfog->shader->fogDist;
		Ref.farClip = max( r_farclip_min, Ref.farClip ) + r_farclip_bias;
		Ref.clipFlags |= 16;
	}
	Ref.meshlist = &r_worldlist;
	Ref.shadowBits = 0;
	Ref.shadowGroup = NULL;

	Vector4Set( Ref.scissor, fd->rect.x, r_height->integer - fd->rect.height - fd->rect.y, fd->rect.width, fd->rect.height );
	Vector4Set( Ref.viewport, fd->rect.x, r_height->integer - fd->rect.height - fd->rect.y, fd->rect.width, fd->rect.height );
	VectorCopy( fd->vieworg, Ref.pvsOrigin );

	if( gl_finish->integer && !( fd->rdflags & RDF_NOWORLDMODEL ))
		pglFinish();

	// FIXME: shadows not implemented
	// R_ClearShadowmaps();
	R_CategorizeEntities();
	R_RenderView( fd );
	R_BloomBlend( fd );
	R_PolyBlend();
	RB_EndFrame();

	// go into 2D mode
	GL_Setup2D();
}

/*
=================
R_BeginFrame
=================
*/
void R_BeginFrame( bool forceClear )
{
	if( r_hwgamma->integer && vid_gamma->modified )
	{
		vid_gamma->modified = false;
		GL_UpdateGammaRamp();
	}

	// update texture filtering
	if( r_texturefilter->modified || r_texturefilteranisotropy->modified )
	{
		R_TextureFilter();
		r_texturefilter->modified = false;
		r_texturefilteranisotropy->modified = false;
	}

	// Set draw buffer
	if( r_frontbuffer->integer )
		pglDrawBuffer( GL_FRONT );
	else pglDrawBuffer( GL_BACK );

	// clear screen if desired
	if( gl_clear->integer || forceClear )
	{
		GL_DepthMask( GL_TRUE );

		pglClearColor( 1.0, 0.0, 0.5, 0.5 );
		pglClear( GL_COLOR_BUFFER_BIT );
	}

	// run cinematic passes on shaders
	// FIXME: implement
	// RB_RunCinematics();

	// Go into 2D mode
	GL_Setup2D();

	// check for errors
	if( r_check_errors->integer ) R_CheckForErrors();
}

/*
=================
R_EndFrame
=================
*/
void R_EndFrame( void )
{
	// make sure all 2D stuff is flushed
	R_CleanUpTextureUnits();
	R_ApplySoftwareGamma();

	// Swap the buffers
	if( !r_frontbuffer->integer )
	{
		if( !pwglSwapBuffers( glw_state.hDC ) )
			Sys_Break("R_EndFrame() - SwapBuffers() failed!\n" );
	}

	// print r_speeds statistics
	if( r_speeds->integer )
	{
		switch( r_speeds->integer )
		{
		case 1:
			Msg("%i/%i shaders/stages %i meshes %i leafs %i verts %i/%i tris\n", r_stats.numShaders, r_stats.numStages, r_stats.numMeshes, r_stats.numLeafs, r_stats.numVertices, (r_stats.numIndices / 3), (r_stats.totalIndices / 3));
			break;
		case 2:
			Msg("%i entities %i dlights %i particles %i polys\n", r_stats.numEntities, r_stats.numDLights, r_stats.numParticles, r_stats.numPolys);
			break;
		}
	}

	// check for errors
	if( r_check_errors->integer ) R_CheckForErrors();
}

bool R_UploadModel( const char *name, int index )
{
	rmodel_t	*mod;

	// this array used by AddEntityToScene
	mod = R_RegisterModel( name );
	cl_models[index] = mod;

	return (mod != NULL);	
}

/*
=================
R_PrecachePic

prefetching 2d graphics
=================
*/
bool R_PrecachePic( const char *name )
{
	texture_t *pic = R_FindTexture(va( "gfx/%s", name ), NULL, 0, TF_NOMIPMAP, 0 );

	if( !pic || pic == r_defaultTexture )
		return false;
	return true;	
}

void R_SetupSky( const char *name, float rotate, const vec3_t axis )
{
	// FIXME: implement
}

/*
=================
R_Init
=================
*/
bool R_Init( void )
{
	GL_InitBackend();

	// create the window and set up the context
	if(!R_Init_OpenGL())
	{
		R_Free_OpenGL();
		return false;
	}

	GL_InitExtensions();
	RB_InitBackend();

	R_InitTextures();
	R_InitPrograms();
	R_InitShaders();
	R_InitModels();
	R_CheckForErrors();

	return true;
}

/*
=================
R_Shutdown
=================
*/
void R_Shutdown( void )
{
	R_ShutdownModels();
	R_ShutdownShaders();
	R_ShutdownPrograms();
	R_ShutdownTextures();

	RB_ShutdownBackend();
	GL_ShutdownBackend();

	// shut down OS specific OpenGL stuff like contexts, etc.
	R_Free_OpenGL();
}

/*
@@@@@@@@@@@@@@@@@@@@@
CreateAPI

@@@@@@@@@@@@@@@@@@@@@
*/
render_exp_t DLLEXPORT *CreateAPI(stdlib_api_t *input, render_imp_t *engfuncs )
{
	static render_exp_t re;

	com = *input;
	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but second argument will be 0
	// and always make exception, run simply check for avoid it
	if( engfuncs ) ri = *engfuncs;

	// generic functions
	re.api_size = sizeof(render_exp_t);

	re.Init = R_Init;
	re.Shutdown = R_Shutdown;
	
	re.BeginRegistration = R_BeginRegistration;
	re.RegisterModel = R_UploadModel;
	re.RegisterImage = Mod_RegisterShader;
	re.PrecacheImage = R_PrecachePic;
	re.SetSky = R_SetupSky;
	re.EndRegistration = R_EndRegistration;

	re.AddLightStyle = R_AddLightStyle;
	re.AddRefEntity = R_AddEntityToScene;
	re.AddDynLight = R_AddDynamicLight;
	re.AddParticle = R_AddParticleToScene;
	re.ClearScene = R_ClearScene;

	re.BeginFrame = R_BeginFrame;
	re.RenderFrame = R_RenderScene;
	re.EndFrame = R_EndFrame;

	re.SetColor = GL_SetColor;
	re.ScrShot = VID_ScreenShot;
	re.DrawFill = R_DrawFill;
	re.DrawStretchRaw = R_DrawStretchRaw;
	re.DrawStretchPic = R_DrawStretchPic;

	// get rid of this
	re.DrawGetPicSize = R_GetPicSize;

	return &re;
}