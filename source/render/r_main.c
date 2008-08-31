//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_main.c - opengl render core
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "matrixlib.h"
#include "const.h"

render_imp_t	ri;
stdlib_api_t	com;

byte		*r_temppool;
matrix4x4		r_worldMatrix;
matrix4x4		r_entityMatrix;

gl_matrix		gl_projectionMatrix;
gl_matrix		gl_worldMatrix;
gl_matrix		gl_entityMatrix;
gl_matrix		gl_textureMatrix;
cplane_t		r_frustum[4];
float		r_frameTime;
mesh_t		r_solidMeshes[MAX_MESHES];
int		r_numSolidMeshes;
mesh_t		r_transMeshes[MAX_MESHES];
int		r_numTransMeshes;
ref_entity_t	*r_nullModels[MAX_ENTITIES];
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
polyVert_t	r_polyVerts[MAX_POLY_VERTS];
int		r_numPolyVerts;
int		r_numNullModels;
refdef_t		r_refdef;
refstats_t	r_stats;
byte		*r_framebuffer;			// pause frame buffer
float		r_pause_alpha;
glconfig_t	gl_config;
glstate_t		gl_state;

// view matrix
vec3_t		r_forward;
vec3_t		r_right;
vec3_t		r_up;
vec3_t		r_origin;

cvar_t	*r_check_errors;
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
cvar_t	*r_shadows;
cvar_t	*r_caustics;
cvar_t	*r_dynamiclights;
cvar_t	*r_modulate;
cvar_t	*r_ambientscale;
cvar_t	*r_directedscale;
cvar_t	*r_intensity;
cvar_t	*r_texturebits;
cvar_t	*r_texturefilter;
cvar_t	*r_texturefilteranisotropy;
cvar_t	*r_detailtextures;
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
=================
R_CullBox

Returns true if the box is completely outside the frustum
=================
*/
bool R_CullBox( const vec3_t mins, const vec3_t maxs, int clipFlags )
{
	cplane_t	*plane;
	int	i;
	
	if( r_nocull->integer )
		return false;

	for( i = 0, plane = r_frustum; i < 4; i++, plane++ )
	{
		if(!(clipFlags & (1<<i))) continue;
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
bool R_CullSphere( const vec3_t origin, float radius, int clipFlags )
{
	cplane_t	*plane;
	int	i;
	
	if( r_nocull->integer )
		return false;

	for( i = 0, plane = r_frustum; i < 4; i++, plane++ )
	{
		if(!(clipFlags & (1<<i)))
			continue;
		if( DotProduct( origin, plane->normal ) - plane->dist <= -radius )
			return true;
	}
	return false;
}

/*
=================
R_RotateForEntity
=================
*/
void R_RotateForEntity( ref_entity_t *entity )
{
	matrix4x4		entMatrix;
	gl_matrix		rotateMatrix;
#if 0
	Matrix4x4_LoadIdentity( entMatrix );
	Matrix4x4_ConcatTranslate( entMatrix, entity->origin[0],  entity->origin[1],  entity->origin[2] );
	Matrix4x4_ConcatRotate( entMatrix,  entity->angles[1],  0, 0, 1 );
	Matrix4x4_ConcatRotate( entMatrix, -entity->angles[0],  0, 1, 0 );
	Matrix4x4_ConcatRotate( entMatrix, -entity->angles[2],  1, 0, 0 );
#if 1
	Matrix4x4_ToArrayFloatGL( entMatrix, rotateMatrix );
	MatrixGL_MultiplyFast( gl_worldMatrix, rotateMatrix, gl_entityMatrix );
	pglLoadMatrixf( gl_entityMatrix );
#else
	Matrix4x4_Concat( r_entityMatrix, r_worldMatrix, entMatrix );
	GL_LoadMatrix( r_entityMatrix );
#endif
	return;
#endif
	rotateMatrix[ 0] = entity->axis[0][0];
	rotateMatrix[ 1] = entity->axis[0][1];
	rotateMatrix[ 2] = entity->axis[0][2];
	rotateMatrix[ 3] = 0.0;
	rotateMatrix[ 4] = entity->axis[1][0];
	rotateMatrix[ 5] = entity->axis[1][1];
	rotateMatrix[ 6] = entity->axis[1][2];
	rotateMatrix[ 7] = 0.0;
	rotateMatrix[ 8] = entity->axis[2][0];
	rotateMatrix[ 9] = entity->axis[2][1];
	rotateMatrix[10] = entity->axis[2][2];
	rotateMatrix[11] = 0.0;
	rotateMatrix[12] = entity->origin[0];
	rotateMatrix[13] = entity->origin[1];
	rotateMatrix[14] = entity->origin[2];
	rotateMatrix[15] = 1.0;

	MatrixGL_MultiplyFast(gl_worldMatrix, rotateMatrix, gl_entityMatrix);

	pglLoadMatrixf(gl_entityMatrix);
}


// =====================================================================

static void R_AddStudioModelToList( ref_entity_t *entity )
{
}

/*
=================
R_DrawBeam
=================
*/
void R_DrawBeam( void )
{
	vec3_t	axis[3];
	float	length;
	int		i;

	// find orientation vectors
	VectorSubtract( r_refdef.vieworg, m_pCurrentEntity->origin, axis[0] );
	VectorSubtract( m_pCurrentEntity->prev.origin, m_pCurrentEntity->origin, axis[1] );// FIXME

	CrossProduct( axis[0], axis[1], axis[2] );
	VectorNormalizeFast( axis[2] );

	// find normal
	CrossProduct( axis[1], axis[2], axis[0] );
	VectorNormalizeFast( axis[0] );

	// scale by radius
	VectorScale( axis[2], m_pCurrentEntity->frame / 2, axis[2] );

	// find segment length
	length = VectorLength( axis[1] ) / m_pCurrentEntity->prev.frame;

	// draw it
	RB_CheckMeshOverflow( 6, 4 );
	
	for( i = 2; i < 4; i++ )
	{
		indexArray[numIndex++] = numVertex + 0;
		indexArray[numIndex++] = numVertex + i-1;
		indexArray[numIndex++] = numVertex + i;
	}

	vertexArray[numVertex+0][0] = m_pCurrentEntity->origin[0] + axis[2][0];
	vertexArray[numVertex+0][1] = m_pCurrentEntity->origin[1] + axis[2][1];
	vertexArray[numVertex+0][2] = m_pCurrentEntity->origin[2] + axis[2][2];
	vertexArray[numVertex+1][0] = m_pCurrentEntity->prev.origin[0] + axis[2][0];
	vertexArray[numVertex+1][1] = m_pCurrentEntity->prev.origin[1] + axis[2][1];
	vertexArray[numVertex+1][2] = m_pCurrentEntity->prev.origin[2] + axis[2][2];
	vertexArray[numVertex+2][0] = m_pCurrentEntity->prev.origin[0] - axis[2][0];
	vertexArray[numVertex+2][1] = m_pCurrentEntity->prev.origin[1] - axis[2][1];
	vertexArray[numVertex+2][2] = m_pCurrentEntity->prev.origin[2] - axis[2][2];
	vertexArray[numVertex+3][0] = m_pCurrentEntity->origin[0] - axis[2][0];
	vertexArray[numVertex+3][1] = m_pCurrentEntity->origin[1] - axis[2][1];
	vertexArray[numVertex+3][2] = m_pCurrentEntity->origin[2] - axis[2][2];

	inTexCoordArray[numVertex+0][0] = 0;
	inTexCoordArray[numVertex+0][1] = 0;
	inTexCoordArray[numVertex+1][0] = length;
	inTexCoordArray[numVertex+1][1] = 0;
	inTexCoordArray[numVertex+2][0] = length;
	inTexCoordArray[numVertex+2][1] = 1;
	inTexCoordArray[numVertex+3][0] = 0;
	inTexCoordArray[numVertex+3][1] = 1;

	for( i = 0; i < 4; i++ )
	{
		normalArray[numVertex][0] = axis[0][0];
		normalArray[numVertex][1] = axis[0][1];
		normalArray[numVertex][2] = axis[0][2];
		inColorArray[numVertex][0] = m_pCurrentEntity->rendercolor[0];
		inColorArray[numVertex][1] = m_pCurrentEntity->rendercolor[1];
		inColorArray[numVertex][2] = m_pCurrentEntity->rendercolor[2];
		inColorArray[numVertex][3] = m_pCurrentEntity->renderamt;
		numVertex++;
	}
}

/*
=================
R_AddBeamToList
=================
*/
static void R_AddBeamToList( ref_entity_t *entity )
{
	R_AddMeshToList( MESH_BEAM, NULL, entity->shader, entity, 0 );
}

/*
=================
R_AddEntitiesToList
=================
*/
static void R_AddEntitiesToList( void )
{
	ref_entity_t	*entity;
	rmodel_t		*model;
	int		i;

	if( !r_drawentities->integer || r_numEntities == 1 )
		return;

	r_stats.numEntities += (r_numEntities - 1);

	for( i = 1, entity = &r_entities[1]; i < r_numEntities; i++, entity++ )
	{
		switch( entity->ent_type )
		{
		case ED_NORMAL:
		case ED_CLIENT:
		case ED_VIEWMODEL:
			model = m_pRenderModel = entity->model;
			if( !model || model->type == mod_bad )
			{
				r_nullModels[r_numNullModels++] = entity;
				break;
			}

			switch( model->type )
			{
			case mod_world:
			case mod_brush:
				R_AddBrushModelToList( entity );
				break;
			case mod_studio:
				R_AddStudioModelToList( entity );
				break;
			case mod_sprite:
				R_AddSpriteModelToList( entity );
				break;
			default:
				Host_Error( "R_AddEntitiesToList: bad model type (%i)\n", model->type );
			}
			break;
		case ED_BEAM:
			R_AddBeamToList( entity );
			break;
		default:
			Host_Error( "R_AddEntitiesToList: bad entity type (%i)\n", entity->ent_type );
		}
	}
}

/*
=================
R_DrawNullModels
=================
*/
static void R_DrawNullModels( void )
{
	ref_entity_t	*entity;
	vec3_t		points[3];
	int		i;

	if (!r_numNullModels)
		return;

	pglLoadMatrixf( gl_worldMatrix );

	// Set the state
	GL_Enable(GL_CULL_FACE);
	GL_Disable(GL_POLYGON_OFFSET_FILL);
	GL_Disable(GL_VERTEX_PROGRAM_ARB);
	GL_Disable(GL_FRAGMENT_PROGRAM_ARB);
	GL_Disable(GL_ALPHA_TEST);
	GL_Enable(GL_BLEND);
	GL_Enable(GL_DEPTH_TEST);

	GL_CullFace(GL_FRONT);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthFunc(GL_LEQUAL);
	GL_DepthMask(GL_FALSE);

	// Draw them
	for( i = 0; i < r_numNullModels; i++ )
	{
		entity = r_nullModels[i];

		VectorMA( entity->origin, 15, entity->axis[0], points[0] );
		VectorMA( entity->origin, -15, entity->axis[1], points[1] );
		VectorMA( entity->origin, 15, entity->axis[2], points[2] );

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
	r_numNullModels = 0;
}

/*
=================
R_DrawParticle
=================
*/
void R_DrawParticle( void )
{
	particle_t	*particle = m_pRenderMesh->mesh;
	vec3_t		axis[3];
	int		i;

	// Draw it
	RB_CheckMeshOverflow( 6, 4 );
	
	for( i = 2; i < 4; i++ )
	{
		indexArray[numIndex++] = numVertex + 0;
		indexArray[numIndex++] = numVertex + i-1;
		indexArray[numIndex++] = numVertex + i;
	}

	if( particle->length != 1 )
	{
		// find orientation vectors
		VectorSubtract( r_refdef.vieworg, particle->origin, axis[0] );
		VectorSubtract( particle->old_origin, particle->origin, axis[1] );
		CrossProduct( axis[0], axis[1], axis[2] );

		VectorNormalizeFast( axis[1] );
		VectorNormalizeFast( axis[2] );

		// find normal
		CrossProduct( axis[1], axis[2], axis[0] );
		VectorNormalizeFast( axis[0] );

		VectorMA( particle->origin, -particle->length, axis[1], particle->old_origin );
		VectorScale( axis[2], particle->radius, axis[2] );

		vertexArray[numVertex+0][0] = particle->old_origin[0] + axis[2][0];
		vertexArray[numVertex+0][1] = particle->old_origin[1] + axis[2][1];
		vertexArray[numVertex+0][2] = particle->old_origin[2] + axis[2][2];
		vertexArray[numVertex+1][0] = particle->origin[0] + axis[2][0];
		vertexArray[numVertex+1][1] = particle->origin[1] + axis[2][1];
		vertexArray[numVertex+1][2] = particle->origin[2] + axis[2][2];
		vertexArray[numVertex+2][0] = particle->origin[0] - axis[2][0];
		vertexArray[numVertex+2][1] = particle->origin[1] - axis[2][1];
		vertexArray[numVertex+2][2] = particle->origin[2] - axis[2][2];
		vertexArray[numVertex+3][0] = particle->old_origin[0] - axis[2][0];
		vertexArray[numVertex+3][1] = particle->old_origin[1] - axis[2][1];
		vertexArray[numVertex+3][2] = particle->old_origin[2] - axis[2][2];
	}
	else
	{
		if( particle->rotation )
		{
			// Rotate it around its normal
			RotatePointAroundVector( axis[1], r_forward, r_right, particle->rotation );
			CrossProduct( r_forward, axis[1], axis[2] );

			// The normal should point at the viewer
			VectorNegate( r_forward, axis[0] );

			// Scale the axes by radius
			VectorScale( axis[1], particle->radius, axis[1] );
			VectorScale( axis[2], particle->radius, axis[2] );
		}
		else
		{
			// the normal should point at the viewer
			VectorNegate( r_forward, axis[0] );

			// scale the axes by radius
			VectorScale( r_right, particle->radius, axis[1] );
			VectorScale( r_up, particle->radius, axis[2] );
		}

		vertexArray[numVertex+0][0] = particle->origin[0] + axis[1][0] + axis[2][0];
		vertexArray[numVertex+0][1] = particle->origin[1] + axis[1][1] + axis[2][1];
		vertexArray[numVertex+0][2] = particle->origin[2] + axis[1][2] + axis[2][2];
		vertexArray[numVertex+1][0] = particle->origin[0] - axis[1][0] + axis[2][0];
		vertexArray[numVertex+1][1] = particle->origin[1] - axis[1][1] + axis[2][1];
		vertexArray[numVertex+1][2] = particle->origin[2] - axis[1][2] + axis[2][2];
		vertexArray[numVertex+2][0] = particle->origin[0] - axis[1][0] - axis[2][0];
		vertexArray[numVertex+2][1] = particle->origin[1] - axis[1][1] - axis[2][1];
		vertexArray[numVertex+2][2] = particle->origin[2] - axis[1][2] - axis[2][2];
		vertexArray[numVertex+3][0] = particle->origin[0] + axis[1][0] - axis[2][0];
		vertexArray[numVertex+3][1] = particle->origin[1] + axis[1][1] - axis[2][1];
		vertexArray[numVertex+3][2] = particle->origin[2] + axis[1][2] - axis[2][2];
	}

	inTexCoordArray[numVertex+0][0] = 0;
	inTexCoordArray[numVertex+0][1] = 0;
	inTexCoordArray[numVertex+1][0] = 1;
	inTexCoordArray[numVertex+1][1] = 0;
	inTexCoordArray[numVertex+2][0] = 1;
	inTexCoordArray[numVertex+2][1] = 1;
	inTexCoordArray[numVertex+3][0] = 0;
	inTexCoordArray[numVertex+3][1] = 1;

	for( i = 0; i < 4; i++ )
	{
		normalArray[numVertex][0] = axis[0][0];
		normalArray[numVertex][1] = axis[0][1];
		normalArray[numVertex][2] = axis[0][2];
		Vector4Copy(particle->modulate, inColorArray[numVertex] ); 
		numVertex++;
	}
}

/*
=================
R_AddParticlesToList
=================
*/
static void R_AddParticlesToList( void )
{
	particle_t	*particle;
	vec3_t		vec;
	int			i;

	if( !r_drawparticles->integer || !r_numParticles )
		return;

	r_stats.numParticles += r_numParticles;

	for (i = 0, particle = r_particles; i < r_numParticles; i++, particle++ )
	{
		// cull
		if( !r_nocull->integer )
		{
			VectorSubtract( particle->origin, r_refdef.vieworg, vec );
			VectorNormalizeFast( vec );

			if( DotProduct( vec, r_forward ) < 0 )
				continue;
		}

		// add it
		R_AddMeshToList( MESH_PARTICLE, particle, particle->shader, r_worldEntity, 0 );
	}
}

// =====================================================================


/*
=================
R_QSortMeshes
=================
*/
static void R_QSortMeshes( mesh_t *meshes, int numMeshes )
{
	static mesh_t	tmp;
	static int	stack[4096];
	int		depth = 0;
	int		L, R, l, r, median;
	uint		pivot;

	if( !numMeshes ) return;

	L = 0;
	R = numMeshes - 1;

start:
	l = L;
	r = R;

	median = (L + R) >> 1;

	if( meshes[L].sortKey > meshes[median].sortKey )
	{
		if( meshes[L].sortKey < meshes[R].sortKey ) 
			median = L;
	} 
	else if( meshes[R].sortKey < meshes[median].sortKey )
		median = R;

	pivot = meshes[median].sortKey;

	while( l < r )
	{
		while( meshes[l].sortKey < pivot )
			l++;
		while( meshes[r].sortKey > pivot )
			r--;

		if( l <= r )
		{
			tmp = meshes[r];
			meshes[r] = meshes[l];
			meshes[l] = tmp;

			l++;
			r--;
		}
	}

	if((L < r) && (depth < 4096))
	{
		stack[depth++] = l;
		stack[depth++] = R;
		R = r;
		goto start;
	}

	if( l < R )
	{
		L = l;
		goto start;
	}

	if( depth )
	{
		R = stack[--depth];
		L = stack[--depth];
		goto start;
	}
}

/*
=================
R_ISortMeshes
=================
*/
static void R_ISortMeshes( mesh_t *meshes, int numMeshes )
{
	static mesh_t	tmp;
	int		i, j;

	if( !numMeshes ) return;

	for( i = 1; i < numMeshes; i++ )
	{
		tmp = meshes[i];
		j = i - 1;

		while((j >= 0) && (meshes[j].sortKey > tmp.sortKey))
		{
			meshes[j+1] = meshes[j];
			j--;
		}

		if( i != j+1 ) meshes[j+1] = tmp;
	}
}

/*
=================
R_AddMeshToList

Calculates sort key and stores info used for sorting and batching.
All 3D geometry passes this function.
=================
*/
void R_AddMeshToList( meshType_t meshType, void *mesh, shader_t *shader, ref_entity_t *entity, int infoKey )
{
	mesh_t	*m;

	if( shader->sort <= SORT_DECAL )
	{
		if( r_numSolidMeshes == MAX_MESHES )
			Host_Error( "R_AddMeshToList: MAX_MESHES hit\n" );
		m = &r_solidMeshes[r_numSolidMeshes++];
	}
	else
	{
		if( r_numTransMeshes == MAX_MESHES )
			Host_Error( "R_AddMeshToList: MAX_MESHES hit\n" );
		m = &r_transMeshes[r_numTransMeshes++];
	}

	m->sortKey = (shader->sort<<28) | (shader->shaderNum<<18) | ((entity - r_entities)<<8) | (infoKey);
	m->meshType = meshType;
	m->mesh = mesh;
}


// =====================================================================


/*
=================
R_SetFrustum
=================
*/
static void R_SetFrustum( void )
{
	int	i;

	// build the transformation matrix for the given view angles
	VectorCopy( r_refdef.vieworg, r_origin );
	AnglesToAxis( r_refdef.viewangles );

	RotatePointAroundVector( r_frustum[0].normal, r_up, r_forward, -(90 - r_refdef.fov_x / 2));
	RotatePointAroundVector( r_frustum[1].normal, r_up, r_forward, 90 - r_refdef.fov_x / 2);
	RotatePointAroundVector( r_frustum[2].normal, r_right, r_forward, 90 - r_refdef.fov_y / 2);
	RotatePointAroundVector( r_frustum[3].normal, r_right, r_forward, -(90 - r_refdef.fov_y / 2));

	for( i = 0; i < 4; i++ )
	{
		r_frustum[i].dist = DotProduct( r_refdef.vieworg, r_frustum[i].normal );
		PlaneClassify( &r_frustum[i] );
	}
}

/*
=================
R_SetFarClip
=================
*/
static float R_SetFarClip( void )
{
	float	farDist, dirDist, worldDist = 0;
	int	i;

	if( r_refdef.rdflags & RDF_NOWORLDMODEL)
		return 4096.0;

	dirDist = DotProduct( r_refdef.vieworg, r_forward );
	farDist = dirDist + 256.0;

	for( i = 0; i < 3; i++ )
	{
		if( r_forward[i] < 0 )
			worldDist += (r_worldMins[i] * r_forward[i]);
		else worldDist += (r_worldMaxs[i] * r_forward[i]);
	}

	if( farDist < worldDist ) farDist = worldDist;
	return farDist - dirDist + 256.0;
}

/*
=================
R_SetMatrices
=================
*/
static void R_SetMatrices( void )
{
	float	xMax, xMin, yMax, yMin;
	float	xDiv, yDiv, zDiv;
	float	zNear, zFar;

	zNear = 4.0;
	zFar = R_SetFarClip();

	xMax = zNear * tan(r_refdef.fov_x * M_PI / 360.0);
	xMin = -xMax;

	yMax = zNear * tan(r_refdef.fov_y * M_PI / 360.0);
	yMin = -yMax;

	xDiv = 1.0 / (xMax - xMin);
	yDiv = 1.0 / (yMax - yMin);
	zDiv = 1.0 / (zFar - zNear);

	gl_projectionMatrix[ 0] = (2.0 * zNear) * xDiv;
	gl_projectionMatrix[ 1] = 0.0;
	gl_projectionMatrix[ 2] = 0.0;
	gl_projectionMatrix[ 3] = 0.0;
	gl_projectionMatrix[ 4] = 0.0;
	gl_projectionMatrix[ 5] = (2.0 * zNear) * yDiv;
	gl_projectionMatrix[ 6] = 0.0;
	gl_projectionMatrix[ 7] = 0.0;
	gl_projectionMatrix[ 8] = (xMax + xMin) * xDiv;
	gl_projectionMatrix[ 9] = (yMax + yMin) * yDiv;
	gl_projectionMatrix[10] = -(zNear + zFar) * zDiv;
	gl_projectionMatrix[11] = -1.0;
	gl_projectionMatrix[12] = 0.0;
	gl_projectionMatrix[13] = 0.0;
	gl_projectionMatrix[14] = -(2.0 * zNear * zFar) * zDiv;
	gl_projectionMatrix[15] = 0.0;
#if 1
	Matrix4x4_LoadIdentity( r_worldMatrix );
	Matrix4x4_ConcatRotate( r_worldMatrix, -90,  1, 0, 0 );	    // put Z going up
	Matrix4x4_ConcatRotate( r_worldMatrix,	90,  0, 0, 1 );	    // put Z going up
	Matrix4x4_ConcatRotate( r_worldMatrix, -r_refdef.viewangles[2],  1, 0, 0 );
	Matrix4x4_ConcatRotate( r_worldMatrix, -r_refdef.viewangles[0],  0, 1, 0 );
	Matrix4x4_ConcatRotate( r_worldMatrix, -r_refdef.viewangles[1],  0, 0, 1 );
	Matrix4x4_ConcatTranslate( r_worldMatrix, -r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2] );
#else
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity ();
	pglRotatef( -90,  1, 0, 0 );	    // put Z going up
	pglRotatef( 90,  0, 0, 1 );	    // put Z going up
	pglRotatef( -r_refdef.viewangles[2],  1, 0, 0 );
	pglRotatef( -r_refdef.viewangles[0],  0, 1, 0 );
	pglRotatef( -r_refdef.viewangles[1],  0, 0, 1 );
	pglTranslatef( -r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2] );
	GL_SaveMatrix( GL_MODELVIEW_MATRIX, r_worldMatrix );
#endif
	gl_worldMatrix[ 0] = -r_right[0];
	gl_worldMatrix[ 1] = r_up[0];
	gl_worldMatrix[ 2] = -r_forward[0];
	gl_worldMatrix[ 3] = 0.0;
	gl_worldMatrix[ 4] = -r_right[1];
	gl_worldMatrix[ 5] = r_up[1];
	gl_worldMatrix[ 6] = -r_forward[1];
	gl_worldMatrix[ 7] = 0.0;
	gl_worldMatrix[ 8] = -r_right[2];
	gl_worldMatrix[ 9] = r_up[2];
	gl_worldMatrix[10] = -r_forward[2];
	gl_worldMatrix[11] = 0.0;
	gl_worldMatrix[12] = DotProduct( r_origin, r_right );
	gl_worldMatrix[13] = -DotProduct( r_origin, r_up );
	gl_worldMatrix[14] = DotProduct( r_origin, r_forward );
	gl_worldMatrix[15] = 1.0;

	gl_textureMatrix[ 0] = r_right[0];
	gl_textureMatrix[ 1] = -r_right[1];
	gl_textureMatrix[ 2] = -r_right[2];
	gl_textureMatrix[ 3] = 0.0;
	gl_textureMatrix[ 4] = -r_up[0];
	gl_textureMatrix[ 5] = r_up[1];
	gl_textureMatrix[ 6] = r_up[2];
	gl_textureMatrix[ 7] = 0.0;
	gl_textureMatrix[ 8] = r_forward[0];
	gl_textureMatrix[ 9] = -r_forward[1];
	gl_textureMatrix[10] = -r_forward[2];
	gl_textureMatrix[11] = 0.0;
	gl_textureMatrix[12] = 0.0;
	gl_textureMatrix[13] = 0.0;
	gl_textureMatrix[14] = 0.0;
	gl_textureMatrix[15] = 1.0;
}

/*
=================
R_RenderView
=================
*/
void R_RenderView( const refdef_t *fd )
{
	if( r_skipfrontend->integer )
		return;

	r_refdef = *fd;
	r_numSolidMeshes = 0;
	r_numTransMeshes = 0;

	// set up frustum
	R_SetFrustum();

	// build mesh lists
	R_AddWorldToList();
	R_AddEntitiesToList();
	R_AddParticlesToList();

	// sort mesh lists
	R_QSortMeshes( r_solidMeshes, r_numSolidMeshes );
	R_ISortMeshes( r_transMeshes, r_numTransMeshes );

	// set up matrices
	R_SetMatrices();

	// go into 3D mode
	GL_Setup3D();

	// render everything
	RB_RenderMeshes( r_solidMeshes, r_numSolidMeshes );

	// R_RenderShadows();

	RB_RenderMeshes( r_transMeshes, r_numTransMeshes );

	// finish up
	R_DrawNullModels();

	RB_DebugGraphics();
	R_BloomBlend( fd );
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
	r_numPolyVerts = 0;
}

/*
====================
R_SetLightLevel

HACKHACK
====================
*/
void R_SetLightLevel( void )
{
	vec3_t	shadelight;

	if( r_refdef.rdflags & RDF_NOWORLDMODEL )
		return;

	// save off light value for server to look at (BIG HACK!)

	R_LightForPoint( r_refdef.vieworg, shadelight );

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

	AnglesToAxisPrivate( refent->angles, refent->axis );

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
	if( !refent->ent_type ) refent->ent_type = ED_NORMAL;

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

	dl = &r_dlights[r_numDLights];
	VectorCopy( org, dl->origin );
	VectorCopy( color, dl->color );
	dl->intensity = intensity;
	r_numDLights++;

	return true;
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
R_RenderFrame
=================
*/
void R_RenderFrame( refdef_t *rd )
{
	if( r_norefresh->integer )
		return;

	r_refdef = *rd;

	if(!(r_refdef.rdflags & RDF_NOWORLDMODEL ))
	{
		if( !r_worldModel ) Host_Error( "R_RenderScene: NULL worldmodel\n" );
	}

	// Make sure all 2D stuff is flushed
	RB_RenderMesh();

	// render view
	R_RenderView( rd );

	// go into 2D mode
	GL_Setup2D();
}

/*
=================
R_BeginFrame
=================
*/
void R_BeginFrame( void )
{
	// clear r_speeds statistics
	memset(&r_stats, 0, sizeof(refstats_t));

	if( vid_gamma->modified )
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
	if( gl_clear->integer )
	{
		GL_DepthMask( GL_TRUE );

		pglClearColor( 1.0, 0.0, 0.5, 0.5 );
		pglClear( GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT );
	}

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
	RB_RenderMesh();

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

bool R_UploadImage( const char *unused, int index )
{
	texture_t	*texture;

	// nothing to load
	if( !r_worldModel ) return false;
	m_pLoadModel = r_worldModel;

	texture = R_FindTexture( m_pLoadModel->textures[index].name, NULL, 0, 0, 0 );
	m_pLoadModel->textures[index].image = texture; // now all pointers are valid
	return true;
}

/*
=================
R_PrecachePic

prefetching 2d graphics
=================
*/
bool R_PrecachePic( const char *name )
{
	texture_t *pic = R_FindTexture(va( "gfx/%s", name ), NULL, 0, TF_IMAGE2D, 0 );

	if( !pic || pic == r_defaultTexture )
		return false;
	return true;	
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
	re.RegisterImage = R_UploadImage;
	re.PrecacheImage = R_PrecachePic;
	re.SetSky = R_SetupSky;
	re.EndRegistration = R_EndRegistration;

	re.AddLightStyle = R_AddLightStyle;
	re.AddRefEntity = R_AddEntityToScene;
	re.AddDynLight = R_AddDynamicLight;
	re.AddParticle = R_AddParticleToScene;
	re.ClearScene = R_ClearScene;

	re.BeginFrame = R_BeginFrame;
	re.RenderFrame = R_RenderFrame;
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