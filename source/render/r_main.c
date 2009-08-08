/*
Copyright (C) 1997-2001 Id Software, Inc.
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

// r_main.c

#include <stdio.h>		// sscanf
#include "r_local.h"
#include "mathlib.h"
#include "matrix_lib.h"

render_imp_t	ri;
stdlib_api_t	com;

ref_model_t	*r_worldmodel;
mbrushmodel_t	*r_worldbrushmodel;

float gldepthmin, gldepthmax;

mapconfig_t mapConfig;

refinst_t RI, prevRI;
ref_params_t r_lastRefdef;

static int r_numnullentities;
static ref_entity_t	*r_nullentities[MAX_EDICTS];
ref_model_t *cl_models[MAX_MODELS];		// client replacement modeltable
static int r_numbmodelentities;
static ref_entity_t	*r_bmodelentities[MAX_EDICTS];

static byte r_entVisBits[MAX_EDICTS/8];

int r_pvsframecount;    // bumped when going to a new PVS
int r_framecount;       // used for dlight push checking

int c_brush_polys, c_world_leafs;

int r_mark_leaves, r_world_node;
int r_add_polys, r_add_entities;
int r_sort_meshes, r_draw_meshes;

msurface_t *r_debug_surface;

char r_speeds_msg[MAX_RSPEEDSMSGSIZE];

//
// screen size info
//
unsigned int r_numEntities;
ref_entity_t r_entities[MAX_ENTITIES];
ref_entity_t *r_worldent = &r_entities[0];

unsigned int r_numDlights;
dlight_t r_dlights[MAX_DLIGHTS];

unsigned int r_numPolys;
poly_t r_polys[MAX_POLYS];

lightstyle_t r_lightStyles[MAX_LIGHTSTYLES];

int r_viewcluster, r_oldviewcluster;

float r_farclip_min, r_farclip_bias = 64.0f;

/*
=================
GL_Cull
=================
*/
void GL_Cull( int cull )
{
	if( glState.faceCull == cull )
		return;

	if( !cull )
	{
		pglDisable( GL_CULL_FACE );
		glState.faceCull = 0;
		return;
	}

	if( !glState.faceCull )
		pglEnable( GL_CULL_FACE );
	pglCullFace( cull );
	glState.faceCull = cull;
}

/*
=================
GL_SetState
=================
*/
void GL_SetState( int state )
{
	int diff;

	if( glState.in2DMode )
		state |= GLSTATE_NO_DEPTH_TEST;
	if( state & GLSTATE_NO_DEPTH_TEST )
		state &= ~( GLSTATE_DEPTHWRITE|GLSTATE_DEPTHFUNC_EQ );

	diff = glState.flags ^ state;
	if( !diff )
		return;

	if( diff & ( GLSTATE_BLEND_MTEX|GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK ) )
	{
		if( state & ( GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK ) )
		{
			int blendsrc, blenddst;

			switch( state & GLSTATE_SRCBLEND_MASK )
			{
			case GLSTATE_SRCBLEND_ZERO:
				blendsrc = GL_ZERO;
				break;
			case GLSTATE_SRCBLEND_DST_COLOR:
				blendsrc = GL_DST_COLOR;
				break;
			case GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR:
				blendsrc = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLSTATE_SRCBLEND_SRC_ALPHA:
				blendsrc = GL_SRC_ALPHA;
				break;
			case GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				blendsrc = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLSTATE_SRCBLEND_DST_ALPHA:
				blendsrc = GL_DST_ALPHA;
				break;
			case GLSTATE_SRCBLEND_ONE_MINUS_DST_ALPHA:
				blendsrc = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
			case GLSTATE_SRCBLEND_ONE:
				blendsrc = GL_ONE;
				break;
			}

			switch( state & GLSTATE_DSTBLEND_MASK )
			{
			case GLSTATE_DSTBLEND_ONE:
				blenddst = GL_ONE;
				break;
			case GLSTATE_DSTBLEND_SRC_COLOR:
				blenddst = GL_SRC_COLOR;
				break;
			case GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR:
				blenddst = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLSTATE_DSTBLEND_SRC_ALPHA:
				blenddst = GL_SRC_ALPHA;
				break;
			case GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				blenddst = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLSTATE_DSTBLEND_DST_ALPHA:
				blenddst = GL_DST_ALPHA;
				break;
			case GLSTATE_DSTBLEND_ONE_MINUS_DST_ALPHA:
				blenddst = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
			case GLSTATE_DSTBLEND_ZERO:
				blenddst = GL_ZERO;
				break;
			}

			if( state & GLSTATE_BLEND_MTEX )
			{
				if( glState.currentEnvModes[glState.activeTMU] != GL_REPLACE )
					pglEnable( GL_BLEND );
				else
					pglDisable( GL_BLEND );
			}
			else
			{
				pglEnable( GL_BLEND );
			}

			pglBlendFunc( blendsrc, blenddst );
		}
		else
		{
			pglDisable( GL_BLEND );
		}
	}

	if( diff & GLSTATE_ALPHAFUNC )
	{
		if( state & GLSTATE_ALPHAFUNC )
		{
			if( !( glState.flags & GLSTATE_ALPHAFUNC ) )
				pglEnable( GL_ALPHA_TEST );

			if( state & GLSTATE_AFUNC_GT0 )
				pglAlphaFunc( GL_GREATER, 0 );
			else if( state & GLSTATE_AFUNC_LT128 )
				pglAlphaFunc( GL_LESS, 0.5f );
			else
				pglAlphaFunc( GL_GEQUAL, 0.5f );
		}
		else
		{
			pglDisable( GL_ALPHA_TEST );
		}
	}

	if( diff & GLSTATE_DEPTHFUNC_EQ )
	{
		if( state & GLSTATE_DEPTHFUNC_EQ )
			pglDepthFunc( GL_EQUAL );
		else
			pglDepthFunc( GL_LEQUAL );
	}

	if( diff & GLSTATE_DEPTHWRITE )
	{
		if( state & GLSTATE_DEPTHWRITE )
			pglDepthMask( GL_TRUE );
		else
			pglDepthMask( GL_FALSE );
	}

	if( diff & GLSTATE_NO_DEPTH_TEST )
	{
		if( state & GLSTATE_NO_DEPTH_TEST )
			pglDisable( GL_DEPTH_TEST );
		else
			pglEnable( GL_DEPTH_TEST );
	}

	if( diff & GLSTATE_OFFSET_FILL )
	{
		if( state & GLSTATE_OFFSET_FILL )
			pglEnable( GL_POLYGON_OFFSET_FILL );
		else
			pglDisable( GL_POLYGON_OFFSET_FILL );
	}

	glState.flags = state;
}

/*
=================
GL_FrontFace
=================
*/
void GL_FrontFace( int front )
{
	pglFrontFace( front ? GL_CW : GL_CCW );
	glState.frontFace = front;
}

/*
=============
R_TransformEntityBBox
=============
*/
void R_TransformEntityBBox( ref_entity_t *e, vec3_t mins, vec3_t maxs, vec3_t bbox[8], bool local )
{
	int	i;
	vec3_t	axis[3], tmp;

	if( e == r_worldent ) local = false;
	if( local ) Matrix3x3_Transpose( axis, e->axis );	// switch row-column order

	// rotate local bounding box and compute the full bounding box
	for( i = 0; i < 8; i++ )
	{
		vec_t *corner = bbox[i];

		corner[0] = (( i & 1 ) ? mins[0] : maxs[0] );
		corner[1] = (( i & 2 ) ? mins[1] : maxs[1] );
		corner[2] = (( i & 4 ) ? mins[2] : maxs[2] );

		if( local )
		{
			Matrix3x3_Transform( axis, corner, tmp );
			VectorAdd( tmp, e->origin, corner );
		}
	}
}

/*
=============
R_LoadIdentity
=============
*/
void R_LoadIdentity( void )
{
	Matrix4_Identity( RI.objectMatrix );
	Matrix4_Copy( RI.worldviewMatrix, RI.modelviewMatrix );
	pglLoadMatrixf( RI.modelviewMatrix );
}

/*
=============
R_RotateForEntity
=============
*/
void R_RotateForEntity( ref_entity_t *e )
{
	if( e == r_worldent )
	{
		R_LoadIdentity();
		return;
	}

	if( e->scale != 1.0f )
	{
		RI.objectMatrix[0] = e->axis[0][0] * e->scale;
		RI.objectMatrix[1] = e->axis[0][1] * e->scale;
		RI.objectMatrix[2] = e->axis[0][2] * e->scale;
		RI.objectMatrix[4] = e->axis[1][0] * e->scale;
		RI.objectMatrix[5] = e->axis[1][1] * e->scale;
		RI.objectMatrix[6] = e->axis[1][2] * e->scale;
		RI.objectMatrix[8] = e->axis[2][0] * e->scale;
		RI.objectMatrix[9] = e->axis[2][1] * e->scale;
		RI.objectMatrix[10] = e->axis[2][2] * e->scale;
	}
	else
	{
		RI.objectMatrix[0] = e->axis[0][0];
		RI.objectMatrix[1] = e->axis[0][1];
		RI.objectMatrix[2] = e->axis[0][2];
		RI.objectMatrix[4] = e->axis[1][0];
		RI.objectMatrix[5] = e->axis[1][1];
		RI.objectMatrix[6] = e->axis[1][2];
		RI.objectMatrix[8] = e->axis[2][0];
		RI.objectMatrix[9] = e->axis[2][1];
		RI.objectMatrix[10] = e->axis[2][2];
	}

	RI.objectMatrix[3] = 0;
	RI.objectMatrix[7] = 0;
	RI.objectMatrix[11] = 0;
	RI.objectMatrix[12] = e->origin[0];
	RI.objectMatrix[13] = e->origin[1];
	RI.objectMatrix[14] = e->origin[2];
	RI.objectMatrix[15] = 1.0;

	Matrix4_MultiplyFast( RI.worldviewMatrix, RI.objectMatrix, RI.modelviewMatrix );
	pglLoadMatrixf( RI.modelviewMatrix );
}

/*
=============
R_TranslateForEntity
=============
*/
void R_TranslateForEntity( ref_entity_t *e )
{
	if( e == r_worldent )
	{
		R_LoadIdentity();
		return;
	}

	Matrix4_Identity( RI.objectMatrix );

	RI.objectMatrix[12] = e->origin[0];
	RI.objectMatrix[13] = e->origin[1];
	RI.objectMatrix[14] = e->origin[2];

	Matrix4_MultiplyFast( RI.worldviewMatrix, RI.objectMatrix, RI.modelviewMatrix );
	pglLoadMatrixf( RI.modelviewMatrix );
}

/*
=============
R_LerpTag
=============
*/
bool R_LerpTag( orientation_t *orient, const ref_model_t *mod, int oldframe, int frame, float lerpfrac, const char *name )
{
	if( !orient )
		return false;

	VectorClear( orient->origin );
	Matrix3x3_LoadIdentity( orient->axis );

	if( !name ) return false;

	if( mod->type == mod_alias )
		return R_AliasModelLerpTag( orient, mod->extradata, oldframe, frame, lerpfrac, name );

	return false;
}

/*
=============
R_FogForSphere
=============
*/
mfog_t *R_FogForSphere( const vec3_t centre, const float radius )
{
	int i, j;
	mfog_t *fog;
	cplane_t *plane;

	if( !r_worldmodel || ( RI.refdef.rdflags & RDF_NOWORLDMODEL ) || !r_worldbrushmodel->numfogs )
		return NULL;
	if( RI.params & RP_SHADOWMAPVIEW )
		return NULL;
	if( r_worldbrushmodel->globalfog )
		return r_worldbrushmodel->globalfog;

	fog = r_worldbrushmodel->fogs;
	for( i = 0; i < r_worldbrushmodel->numfogs; i++, fog++ )
	{
		if( !fog->shader )
			continue;

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
	if( fog && fog->shader && RI.fog_dist_to_eye[fog-r_worldbrushmodel->fogs] < 0 )
	{
		float vpnDist = ( ( RI.viewOrigin[0] - origin[0] ) * RI.vpn[0] + ( RI.viewOrigin[1] - origin[1] ) * RI.vpn[1] + ( RI.viewOrigin[2] - origin[2] ) * RI.vpn[2] );
		return ( ( vpnDist + radius ) / fog->shader->fog_dist ) < -1;
	}

	return false;
}

/*
=============================================================

CUSTOM COLORS

=============================================================
*/

static rgba_t r_customColors[NUM_CUSTOMCOLORS];

/*
=================
R_InitCustomColors
=================
*/
void R_InitCustomColors( void )
{
	memset( r_customColors, 255, sizeof( r_customColors ) );
}

/*
=================
R_SetCustomColor
=================
*/
void R_SetCustomColor( int num, int r, int g, int b )
{
	if( num < 0 || num >= NUM_CUSTOMCOLORS )
		return;
	Vector4Set( r_customColors[num], (byte)r, (byte)g, (byte)b, 255 );
}
/*
=================
R_GetCustomColor
=================
*/
int R_GetCustomColor( int num )
{
	if( num < 0 || num >= NUM_CUSTOMCOLORS )
		return MakeRGBA( 255, 255, 255, 255 );
	return *(int *)r_customColors[num];
}

/*
=============================================================

SPRITE MODELS AND FLARES

=============================================================
*/

static vec4_t spr_xyz[4] = { {0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {0,0,0,1} };
static vec2_t spr_st[4] = { {0, 1}, {0, 0}, {1,0}, {1,1} };
static rgba_t spr_color[4];
static mesh_t spr_mesh = { 4, spr_xyz, spr_xyz, NULL, spr_st, { 0, 0, 0, 0 }, { spr_color, spr_color, spr_color, spr_color }, 6, NULL };

/*
=================
R_PushSprite
=================
*/
static bool R_PushSprite( const meshbuffer_t *mb, int type, float right, float left, float up, float down )
{
	int		i, features;
	vec3_t		v_forward, v_right, v_up;
	ref_entity_t	*e = RI.currententity;
	float		angle, sr, cr;
	ref_shader_t	*shader;
	vec3_t		point;

	switch( type )
	{
	case SPR_ORIENTED:
		VectorCopy( e->axis[0], v_forward );
		VectorCopy( e->axis[1], v_right );
		VectorCopy( e->axis[2], v_up );
		VectorScale( v_forward, 0.01f, v_forward ); // to avoid z-fighting
		VectorSubtract( e->origin, v_forward, e->origin );
		break;
	case SPR_FACING_UPRIGHT:
		VectorSet( v_right, e->origin[1] - RI.viewOrigin[1], -(e->origin[0] - RI.viewOrigin[0]), 0 );
		VectorSet( v_up, 0, 0, 1 );
		VectorNormalize( v_right );
		break;
	case SPR_FWD_PARALLEL_UPRIGHT:
		VectorSet( v_right, RI.vpn[1], -RI.vpn[0], 0 );
		VectorSet( v_up, 0, 0, 1 );
		break;
	case SPR_FWD_PARALLEL_ORIENTED:
		angle = e->angles[ROLL] * (M_PI * 2.0f/360.0f);
		sr = com.sin( angle );
		cr = com.cos( angle );
		for( i = 0; i < 3; i++ )
		{
			v_right[i] = (RI.vright[i] * cr + RI.vup[i] * sr);
			v_up[i] = RI.vright[i] * -sr + RI.vup[i] * cr;
		}
		break;
	default:
		if( e->spriteshader && e->angles[PITCH] )
		{
			RotatePointAroundVector( v_right, RI.vpn, RI.vright, e->angles[PITCH] );
			CrossProduct( RI.vpn, v_right, v_up );
			break;
		}
		// intentional fallthrough
	case SPR_FWD_PARALLEL: // normal sprite
		VectorCopy( RI.vright, v_right );
		VectorCopy( RI.vup, v_up );
		break;
	}

	VectorScale( v_up, down, point );
	VectorMA( point, -left, v_right, spr_xyz[0] );
	VectorMA( point, -right, v_right, spr_xyz[3] );

	VectorScale( v_up, up, point );
	VectorMA( point, -left, v_right, spr_xyz[1] );
	VectorMA( point, -right, v_right, spr_xyz[2] );

	if( e->scale != 1.0f )
	{
		for( i = 0; i < 4; i++ )
			VectorScale( spr_xyz[i], e->scale, spr_xyz[i] );
	}

	MB_NUM2SHADER( mb->shaderkey, shader );

	// the code below is disgusting, but some q3a shaders use 'rgbgen vertex'
	// and 'alphagen vertex' for effects instead of 'rgbgen entity' and 'alphagen entity'
	if( shader->features & MF_COLORS )
	{
		for( i = 0; i < 4; i++ )
			Vector4Set( spr_color[i], e->rendercolor[0], e->rendercolor[1], e->rendercolor[2], e->renderamt );
	}

	features = MF_NOCULL | MF_TRIFAN | shader->features;
	if( r_shownormals->integer )
		features |= MF_NORMALS;

	if( shader->flags & SHADER_ENTITY_MERGABLE )
	{
		for( i = 0; i < 4; i++ )
			VectorAdd( spr_xyz[i], e->origin, spr_xyz[i] );
		R_PushMesh( &spr_mesh, features );
		return false;
	}

	R_PushMesh( &spr_mesh, MF_NONBATCHED | features );
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
	mbrushmodel_t	*bmodel = ( mbrushmodel_t * )RI.currentmodel->extradata;
	msurface_t	*surf = &bmodel->surfaces[mb->infokey - 1];
	ref_shader_t	*shader;

	if( RI.currentmodel != r_worldmodel )
	{
		Matrix3x3_Transform( RI.currententity->axis, surf->origin, origin );
		VectorAdd( origin, RI.currententity->origin, origin );
	}
	else VectorCopy( surf->origin, origin );
	R_TransformToScreen_Vec3( origin, v );

	if( v[0] < RI.refdef.viewport[0] || v[0] > RI.refdef.viewport[0] + RI.refdef.viewport[2] )
		return;
	if( v[1] < RI.refdef.viewport[1] || v[1] > RI.refdef.viewport[1] + RI.refdef.viewport[3] )
		return;

	pglReadPixels((int)( v[0] ), (int)( v[1] ), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth );
	if( depth + 1e-4 < v[2] ) return; // occluded

	VectorCopy( origin, origin );

	VectorMA( origin, down, RI.vup, point );
	VectorMA( point, -left, RI.vright, spr_xyz[0] );
	VectorMA( point, -right, RI.vright, spr_xyz[3] );

	VectorMA( origin, up, RI.vup, point );
	VectorMA( point, -left, RI.vright, spr_xyz[1] );
	VectorMA( point, -right, RI.vright, spr_xyz[2] );

	colorscale = 255.0 / r_flarefade->value;
	Vector4Set( color,
		surf->color[0] * colorscale,
		surf->color[1] * colorscale,
		surf->color[2] * colorscale,
		255 );
	for( i = 0; i < 4; i++ )
		color[i] = bound( 0, color[i], 255 );

	for( i = 0; i < 4; i++ )
		Vector4Copy( color, spr_color[i] );

	MB_NUM2SHADER( mb->shaderkey, shader );

	R_PushMesh( &spr_mesh, MF_NOCULL | MF_TRIFAN | shader->features );
}

/*
=================
R_PushCorona
=================
*/
static void R_PushCorona( const meshbuffer_t *mb )
{
	int i;
	vec4_t color;
	vec3_t origin, point;
	dlight_t *light = r_dlights + ( -mb->infokey - 1 );
	float radius = light->intensity, colorscale;
	float up = radius, down = -radius, left = -radius, right = radius;
	ref_shader_t *shader;

	VectorCopy( light->origin, origin );

	VectorMA( origin, down, RI.vup, point );
	VectorMA( point, -left, RI.vright, spr_xyz[0] );
	VectorMA( point, -right, RI.vright, spr_xyz[3] );

	VectorMA( origin, up, RI.vup, point );
	VectorMA( point, -left, RI.vright, spr_xyz[1] );
	VectorMA( point, -right, RI.vright, spr_xyz[2] );

	colorscale = 255.0 * bound( 0, r_coronascale->value, 1.0 );
	Vector4Set( color,
		light->color[0] * colorscale,
		light->color[1] * colorscale,
		light->color[2] * colorscale,
		255 );
	for( i = 0; i < 4; i++ )
		color[i] = bound( 0, color[i], 255 );

	for( i = 0; i < 4; i++ )
		Vector4Copy( color, spr_color[i] );

	MB_NUM2SHADER( mb->shaderkey, shader );

	R_PushMesh( &spr_mesh, MF_NOCULL | MF_TRIFAN | shader->features );
}

/*
=================
R_PushSpriteModel
=================
*/
bool R_PushSpriteModel( const meshbuffer_t *mb )
{
	mspriteframe_t	*frame;
	msprite_t		*psprite;
	ref_entity_t	*e = RI.currententity;
	ref_model_t	*model = e->model;

	psprite = (msprite_t * )model->extradata;
	frame = R_GetSpriteFrame( e );

	return R_PushSprite( mb, psprite->type, frame->left, frame->right, frame->down, frame->up );
}

/*
=================
R_PushSpritePoly
=================
*/
bool R_PushSpritePoly( const meshbuffer_t *mb )
{
	ref_entity_t *e = RI.currententity;

	if( ( mb->sortkey & 3 ) == MB_CORONA )
	{
		R_PushCorona( mb );
		return false;
	}
	if( mb->infokey > 0 )
	{
		R_PushFlareSurf( mb );
		return false;
	}
	return R_PushSprite( mb, -1, -e->radius, e->radius, e->radius, -e->radius );
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
	ref_model_t	*model = e->model;
	ref_shader_t	*shader;
	float		dist;
	meshbuffer_t	*mb;

	if(!( psprite = (( msprite_t* )model->extradata )))
		return;

	dist = (e->origin[0] - RI.viewOrigin[0]) * RI.vpn[0] + (e->origin[1] - RI.viewOrigin[1]) * RI.vpn[1] + (e->origin[2] - RI.viewOrigin[2]) * RI.vpn[2];
	if( dist < 0 ) return; // cull it because we don't want to sort unneeded things

	frame = R_GetSpriteFrame( e );
	shader = &r_shaders[frame->shader];

	if( e->rendermode == kRenderGlow )
	{
		trace_t	tr;

		if( e->renderfx == kRenderFxNoDissipation );
		else if( R_TraceLine( &tr, e->origin, RI.viewOrigin, SURF_NONSOLID ) == NULL )
		{ 
			float dist = VectorDistance( e->origin, RI.viewOrigin );
			e->scale = bound( 1.0, dist * 0.005f, 10.0f );
			e->renderamt = 255 * bound( 0, dist / 1000, 1.0f );
			if( e->renderamt >= 255 ) return; // faded
		}
		else return; // occluded
	}
			
	if( RI.refdef.rdflags & (RDF_PORTALINVIEW|RDF_SKYPORTALINVIEW) || ( RI.params & RP_SKYPORTALVIEW ))
	{
		if( R_VisCullSphere( e->origin, frame->radius ))
			return;
	}

	mb = R_AddMeshToList( MB_MODEL, R_FogForSphere( e->origin, frame->radius ), shader, -1 );
	if( mb ) mb->shaderkey |= ( bound( 1, 0x4000 - (uint)dist, 0x4000 - 1 )<<12 );
}

/*
=================
R_AddSpritePolyToList
=================
*/
static void R_AddSpritePolyToList( ref_entity_t *e )
{
	float 		dist;
	meshbuffer_t	*mb;

	dist = (e->origin[0] - RI.refdef.vieworg[0]) * RI.vpn[0] + (e->origin[1] - RI.refdef.vieworg[1]) * RI.vpn[1] + (e->origin[2] - RI.refdef.vieworg[2]) * RI.vpn[2];
	if( dist < 0 ) return; // cull it because we don't want to sort unneeded things

	if( RI.refdef.rdflags & ( RDF_PORTALINVIEW|RDF_SKYPORTALINVIEW ) || ( RI.params & RP_SKYPORTALVIEW ) )
	{
		if( R_VisCullSphere( e->origin, e->radius ) )
			return;
	}

	mb = R_AddMeshToList( MB_SPRITE, R_FogForSphere( e->origin, e->radius ), e->spriteshader, -1 );
	if( mb ) mb->shaderkey |= ( bound( 1, 0x4000 - (unsigned int)dist, 0x4000 - 1 ) << 12 );
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
===============
R_Set2DMode
===============
*/
void R_Set2DMode( bool enable )
{
	if( enable )
	{
		if( glState.in2DMode )
			return;

		// set 2D virtual screen size
		pglScissor( 0, 0, glState.width, glState.height );
		pglViewport( 0, 0, glState.width, glState.height );
		pglMatrixMode( GL_PROJECTION );
		pglLoadIdentity();
		pglOrtho( 0, glState.width, glState.height, 0, -99999, 99999 );
		pglMatrixMode( GL_MODELVIEW );
		pglLoadIdentity();

		GL_Cull( 0 );
		GL_SetState( GLSTATE_NO_DEPTH_TEST );

		pglColor4f( 1, 1, 1, 1 );

		glState.in2DMode = true;
		RI.currententity = RI.previousentity = NULL;
		RI.currentmodel = NULL;

		pic_mbuffer.infokey = -1;
		pic_mbuffer.shaderkey = 0;
	}
	else
	{
		if( pic_mbuffer.infokey != -1 )
		{
			R_RenderMeshBuffer( &pic_mbuffer );
			pic_mbuffer.infokey = -1;
		}

		glState.in2DMode = false;
	}
}


/*
============
R_PolyBlend
============
*/
static void R_PolyBlend( void )
{
	if( !r_polyblend->integer )
		return;
	if( RI.refdef.blend[3] < 0.01f )
		return;

	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();
	pglOrtho( 0, 1, 1, 0, -99999, 99999 );

	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	GL_Cull( 0 );
	GL_SetState( GLSTATE_NO_DEPTH_TEST|GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	pglDisable( GL_TEXTURE_2D );

	pglColor4fv( RI.refdef.blend );

	pglBegin( GL_TRIANGLES );
	pglVertex2f( -5, -5 );
	pglVertex2f( 10, -5 );
	pglVertex2f( -5, 10 );
	pglEnd();

	pglEnable( GL_TEXTURE_2D );

	pglColor4f( 1, 1, 1, 1 );
}

/*
===============
R_ApplySoftwareGamma
===============
*/
static void R_ApplySoftwareGamma( void )
{
	double f, div;

	// apply software gamma
	if( !r_ignorehwgamma->integer )
		return;

	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();
	pglOrtho( 0, 1, 1, 0, -99999, 99999 );

	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	GL_Cull( 0 );
	GL_SetState( GLSTATE_NO_DEPTH_TEST | GLSTATE_SRCBLEND_DST_COLOR | GLSTATE_DSTBLEND_ONE );

	pglDisable( GL_TEXTURE_2D );

	if( r_overbrightbits->integer > 0 )
		div = 0.5 * (double)( 1 << r_overbrightbits->integer );
	else
		div = 0.5;
	f = div + r_gamma->value;
	f = bound( 0.1f, f, 5.0f );

	pglBegin( GL_TRIANGLES );

	while( f >= 1.01f )
	{
		if( f >= 2 )
			pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		else
			pglColor4f( f - 1.0f, f - 1.0f, f - 1.0f, 1.0f );

		pglVertex2f( -5, -5 );
		pglVertex2f( 10, -5 );
		pglVertex2f( -5, 10 );
		f *= 0.5;
	}

	pglEnd();

	pglEnable( GL_TEXTURE_2D );

	pglColor4f( 1, 1, 1, 1 );
}

//=============================================================================
static ref_shader_t *r_outlineShader;
/*
===============
R_InitOutlines
===============
*/
void R_InitOutlines( void )
{
	r_outlineShader = R_LoadShader( "celloutline/default", SHADER_OUTLINE, false, 0, SHADER_INVALID );
}

/*
===============
R_AddModelMeshOutline
===============
*/
void R_AddModelMeshOutline( unsigned int modhandle, mfog_t *fog, int meshnum )
{
	meshbuffer_t *mb = R_AddMeshToList( MB_MODEL, fog, r_outlineShader, -( meshnum+1 ) );
	if( mb ) mb->LODModelHandle = modhandle;
}
//=======================================================================

/*
===============
R_SetupFrustum
===============
*/
static void R_SetupFrustum( void )
{
	int i;
	vec3_t farPoint;

	// 0 - left
	// 1 - right
	// 2 - down
	// 3 - up
	// 4 - farclip

	// rotate RI.vpn right by FOV_X/2 degrees
	RotatePointAroundVector( RI.frustum[0].normal, RI.vup, RI.vpn, -( 90-RI.refdef.fov_x / 2 ) );
	// rotate RI.vpn left by FOV_X/2 degrees
	RotatePointAroundVector( RI.frustum[1].normal, RI.vup, RI.vpn, 90-RI.refdef.fov_x / 2 );
	// rotate RI.vpn up by FOV_X/2 degrees
	RotatePointAroundVector( RI.frustum[2].normal, RI.vright, RI.vpn, 90-RI.refdef.fov_y / 2 );
	// rotate RI.vpn down by FOV_X/2 degrees
	RotatePointAroundVector( RI.frustum[3].normal, RI.vright, RI.vpn, -( 90 - RI.refdef.fov_y / 2 ) );
	// negate forward vector
	VectorNegate( RI.vpn, RI.frustum[4].normal );

	for( i = 0; i < 4; i++ )
	{
		RI.frustum[i].type = PLANE_NONAXIAL;
		RI.frustum[i].dist = DotProduct( RI.viewOrigin, RI.frustum[i].normal );
		RI.frustum[i].signbits = SignbitsForPlane( &RI.frustum[i] );
	}

	VectorMA( RI.viewOrigin, RI.farClip, RI.vpn, farPoint );
	RI.frustum[i].type = PLANE_NONAXIAL;
	RI.frustum[i].dist = DotProduct( farPoint, RI.frustum[i].normal );
	RI.frustum[i].signbits = SignbitsForPlane( &RI.frustum[i] );
}

/*
===============
R_FarClip
===============
*/
static float R_FarClip( void )
{
	float farclip_dist;

	if( r_worldmodel && !( RI.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		int i;
		float dist;
		vec3_t tmp;

		farclip_dist = 0;
		for( i = 0; i < 8; i++ )
		{
			tmp[0] = ( ( i & 1 ) ? RI.visMins[0] : RI.visMaxs[0] );
			tmp[1] = ( ( i & 2 ) ? RI.visMins[1] : RI.visMaxs[1] );
			tmp[2] = ( ( i & 4 ) ? RI.visMins[2] : RI.visMaxs[2] );

			dist = VectorDistance2( tmp, RI.viewOrigin );
			farclip_dist = max( farclip_dist, dist );
		}

		farclip_dist = sqrt( farclip_dist );

		if( r_worldbrushmodel->globalfog )
		{
			float fogdist = r_worldbrushmodel->globalfog->shader->fog_dist;
			if( farclip_dist > fogdist )
				farclip_dist = fogdist;
			else
				RI.clipFlags &= ~16;
		}
	}
	else
	{
		farclip_dist = 2048;
	}

	return max( r_farclip_min, farclip_dist ) + r_farclip_bias;
}

/*
=============
R_SetupProjectionMatrix
=============
*/
static void R_SetupProjectionMatrix( const ref_params_t *rd, mat4x4_t m )
{
	GLdouble xMin, xMax, yMin, yMax, zNear, zFar;

	if( rd->rdflags & RDF_NOWORLDMODEL )
		RI.farClip = 2048;
	else
		RI.farClip = R_FarClip();

	zNear = Z_NEAR;
	zFar = RI.farClip;

	yMax = zNear *tan( rd->fov_y *M_PI / 360.0 );
	yMin = -yMax;

	xMax = zNear *tan( rd->fov_x *M_PI / 360.0 );
	xMin = -xMax;

	m[0] = ( 2.0 * zNear ) / ( xMax - xMin );
	m[1] = 0.0f;
	m[2] = 0.0f;
	m[3] = 0.0f;
	m[4] = 0.0f;
	m[5] = ( 2.0 * zNear ) / ( yMax - yMin );
	m[6] = 0.0f;
	m[7] = 0.0f;
	m[8] = ( xMax + xMin ) / ( xMax - xMin );
	m[9] = ( yMax + yMin ) / ( yMax - yMin );
	m[10] = -( zFar + zNear ) / ( zFar - zNear );
	m[11] = -1.0f;
	m[12] = 0.0f;
	m[13] = 0.0f;
	m[14] = -( 2.0 * zFar * zNear ) / ( zFar - zNear );
	m[15] = 0.0f;
}

/*
=============
R_SetupModelviewMatrix
=============
*/
static void R_SetupModelviewMatrix( const ref_params_t *rd, mat4x4_t m )
{
#if 0
	Matrix4_Identity( m );
	Matrix4_Rotate( m, -90, 1, 0, 0 );
	Matrix4_Rotate( m, 90, 0, 0, 1 );
#else
	Vector4Set( &m[0], 0, 0, -1, 0 );
	Vector4Set( &m[4], -1, 0, 0, 0 );
	Vector4Set( &m[8], 0, 1, 0, 0 );
	Vector4Set( &m[12], 0, 0, 0, 1 );
#endif
	Matrix4_Rotate( m, -rd->viewangles[2], 1, 0, 0 );
	Matrix4_Rotate( m, -rd->viewangles[0], 0, 1, 0 );
	Matrix4_Rotate( m, -rd->viewangles[1], 0, 0, 1 );
	Matrix4_Translate( m, -rd->vieworg[0], -rd->vieworg[1], -rd->vieworg[2] );
}

/*
===============
R_SetupFrame
===============
*/
static void R_SetupFrame( void )
{
	mleaf_t *leaf;

	// build the transformation matrix for the given view angles
	VectorCopy( RI.refdef.vieworg, RI.viewOrigin );
	AngleVectors( RI.refdef.viewangles, RI.viewAxis[0], RI.viewAxis[1], RI.viewAxis[2] );
	RI.vpn = RI.viewAxis[0];
	RI.vright = RI.viewAxis[1];
	RI.vup = RI.viewAxis[2];

	if( RI.params & RP_SHADOWMAPVIEW )
		return;

	r_framecount++;

	RI.lod_dist_scale_for_fov = tan( RI.refdef.fov_x * ( M_PI/180 ) * 0.5f );

	// current viewcluster
	if( !( RI.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		VectorCopy( r_worldmodel->mins, RI.visMins );
		VectorCopy( r_worldmodel->maxs, RI.visMaxs );

		if( !( RI.params & RP_OLDVIEWCLUSTER ) )
		{
			r_oldviewcluster = r_viewcluster;
			leaf = Mod_PointInLeaf( RI.pvsOrigin, r_worldmodel );
			r_viewcluster = leaf->cluster;
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
	R_SetupModelviewMatrix( &RI.refdef, RI.worldviewMatrix );
	if( RI.params & RP_SHADOWMAPVIEW )
	{
		int i;
		float x1, x2, y1, y2;
		int ix1, ix2, iy1, iy2;
		int sizex = RI.refdef.viewport[2], sizey = RI.refdef.viewport[3];
		int diffx, diffy;
		shadowGroup_t *group = RI.shadowGroup;

		R_SetupProjectionMatrix( &RI.refdef, RI.projectionMatrix );
		Matrix4_Multiply( RI.projectionMatrix, RI.worldviewMatrix, RI.worldviewProjectionMatrix );

		// compute optimal fov to increase depth precision (so that shadow group objects are
		// as close to the nearplane as possible)
		// note that it's suboptimal to use bbox calculated in worldspace (FIXME)
		x1 = y1 = 999999;
		x2 = y2 = -999999;
		for( i = 0; i < 8; i++ )
		{                   // compute and rotate a full bounding box
			vec3_t v, tmp;

			tmp[0] = ( ( i & 1 ) ? group->mins[0] : group->maxs[0] );
			tmp[1] = ( ( i & 2 ) ? group->mins[1] : group->maxs[1] );
			tmp[2] = ( ( i & 4 ) ? group->mins[2] : group->maxs[2] );

			// transform to screen
			R_TransformToScreen_Vec3( tmp, v );
			x1 = min( x1, v[0] ); y1 = min( y1, v[1] );
			x2 = max( x2, v[0] ); y2 = max( y2, v[1] );
		}

		// give it 1 pixel gap on both sides
		ix1 = x1 - 1.0f; ix2 = x2 + 1.0f;
		iy1 = y1 - 1.0f; iy2 = y2 + 1.0f;

		diffx = sizex - min( ix1, sizex - ix2 ) * 2;
		diffy = sizey - min( iy1, sizey - iy2 ) * 2;

		// adjust fov
		RI.refdef.fov_x = 2 * RAD2DEG( atan( (float)diffx / (float)sizex ) );
		RI.refdef.fov_y = 2 * RAD2DEG( atan( (float)diffy / (float)sizey ) );
	}
	R_SetupProjectionMatrix( &RI.refdef, RI.projectionMatrix );
	if( RI.params & RP_MIRRORVIEW )
		RI.projectionMatrix[0] = -RI.projectionMatrix[0];
	Matrix4_Multiply( RI.projectionMatrix, RI.worldviewMatrix, RI.worldviewProjectionMatrix );
}

/*
=============
R_Clear
=============
*/
static void R_Clear( int bitMask )
{
	int bits;

	bits = GL_DEPTH_BUFFER_BIT;

	if( !( RI.refdef.rdflags & RDF_NOWORLDMODEL ) && r_fastsky->integer )
		bits |= GL_COLOR_BUFFER_BIT;
	if( glState.stencilEnabled && ( r_shadows->integer >= SHADOW_PLANAR ) )
		bits |= GL_STENCIL_BUFFER_BIT;

	bits &= bitMask;

	if( bits & GL_STENCIL_BUFFER_BIT )
		pglClearStencil( 128 );

	if( bits & GL_COLOR_BUFFER_BIT )
	{
		byte *color = r_worldmodel && !( RI.refdef.rdflags & RDF_NOWORLDMODEL ) && r_worldbrushmodel->globalfog ?
			r_worldbrushmodel->globalfog->shader->fog_color : mapConfig.environmentColor;
		pglClearColor( (float)color[0]*( 1.0/255.0 ), (float)color[1]*( 1.0/255.0 ), (float)color[2]*( 1.0/255.0 ), 1 );
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
	pglScissor( RI.scissor[0], RI.scissor[1], RI.scissor[2], RI.scissor[3] );
	pglViewport( RI.viewport[0], RI.viewport[1], RI.viewport[2], RI.viewport[3] );

	pglMatrixMode( GL_PROJECTION );
	pglLoadMatrixf( RI.projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	pglLoadMatrixf( RI.worldviewMatrix );

	if( RI.params & RP_CLIPPLANE )
	{
		GLdouble clip[4];
		cplane_t *p = &RI.clipPlane;

		clip[0] = p->normal[0];
		clip[1] = p->normal[1];
		clip[2] = p->normal[2];
		clip[3] = -p->dist;

		pglClipPlane( GL_CLIP_PLANE0, clip );
		pglEnable( GL_CLIP_PLANE0 );
	}

	if( RI.params & RP_FLIPFRONTFACE )
		GL_FrontFace( !glState.frontFace );

	if( RI.params & RP_SHADOWMAPVIEW )
	{
		pglShadeModel( GL_FLAT );
		pglColorMask( 0, 0, 0, 0 );
		pglPolygonOffset( 1, 4 );
		if( prevRI.params & RP_CLIPPLANE )
			pglDisable( GL_CLIP_PLANE0 );
	}

	GL_Cull( GL_FRONT );
	GL_SetState( GLSTATE_DEPTHWRITE );
}

/*
=============
R_EndGL
=============
*/
static void R_EndGL( void )
{
	if( RI.params & RP_SHADOWMAPVIEW )
	{
		pglPolygonOffset( -1, -2 );
		pglColorMask( 1, 1, 1, 1 );
		pglShadeModel( GL_SMOOTH );
		if( prevRI.params & RP_CLIPPLANE )
			pglEnable( GL_CLIP_PLANE0 );
	}

	if( RI.params & RP_FLIPFRONTFACE )
		GL_FrontFace( !glState.frontFace );

	if( RI.params & RP_CLIPPLANE )
		pglDisable( GL_CLIP_PLANE0 );
}


/*
=============
R_CategorizeEntities
=============
*/
static void R_CategorizeEntities( void )
{
	unsigned int i;

	r_numnullentities = 0;
	r_numbmodelentities = 0;

	if( !r_drawentities->integer )
		return;

	for( i = 1; i < r_numEntities; i++ )
	{
		RI.previousentity = RI.currententity;
		RI.currententity = &r_entities[i];

		if( RI.currententity->rtype != RT_MODEL )
			continue;

		RI.currentmodel = RI.currententity->model;
		if( !RI.currentmodel )
		{
			r_nullentities[r_numnullentities++] = RI.currententity;
			continue;
		}

		switch( RI.currentmodel->type )
		{
		case mod_brush:
		case mod_world:
			r_bmodelentities[r_numbmodelentities++] = RI.currententity;
			break;
		case mod_alias:
		case mod_studio:
			if(!( RI.currententity->flags & (EF_NOSHADOW|EF_PLANARSHADOW)))
				R_AddShadowCaster( RI.currententity ); // build groups and mark shadow casters
			break;
		case mod_sprite:
			break;
		default:
			Host_Error( "%s: bad modeltype\n", RI.currentmodel->name );
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
	unsigned int i;
	ref_entity_t *e;
	bool culled;

	memset( r_entVisBits, 0, sizeof( r_entVisBits ) );
	if( !r_drawentities->integer )
		return;

	for( i = 1; i < r_numEntities; i++ )
	{
		RI.previousentity = RI.currententity;
		RI.currententity = e = &r_entities[i];
		culled = true;

		switch( e->rtype )
		{
		case RT_MODEL:
			if( !e->model )
				break;
			switch( e->model->type )
			{
			case mod_alias:
				culled = R_CullAliasModel( e );
				break;
			case mod_studio:
				culled = true;
				break;
			case mod_brush:
			case mod_world:
				culled = R_CullBrushModel( e );
				break;
			case mod_sprite:
				culled = false;
				break;
			default:	break;
			}
			break;
		case RT_SPRITE:
			culled = ( e->radius <= 0 ) || ( e->spriteshader == NULL );
			break;
		default:
			break;
		}

		if( !culled )
			r_entVisBits[i>>3] |= ( 1<<( i&7 ) );
	}
}

/*
=============
R_DrawNullModel
=============
*/
static void R_DrawNullModel( void )
{
	pglBegin( GL_LINES );

	pglColor4f( 1, 0, 0, 0.5 );
	pglVertex3fv( RI.currententity->origin );
	pglVertex3f( RI.currententity->origin[0] + RI.currententity->axis[0][0] * 15,
		RI.currententity->origin[1] + RI.currententity->axis[0][1] * 15,
		RI.currententity->origin[2] + RI.currententity->axis[0][2] * 15 );

	pglColor4f( 0, 1, 0, 0.5 );
	pglVertex3fv( RI.currententity->origin );
	pglVertex3f( RI.currententity->origin[0] - RI.currententity->axis[1][0] * 15,
		RI.currententity->origin[1] - RI.currententity->axis[1][1] * 15,
		RI.currententity->origin[2] - RI.currententity->axis[1][2] * 15 );

	pglColor4f( 0, 0, 1, 0.5 );
	pglVertex3fv( RI.currententity->origin );
	pglVertex3f( RI.currententity->origin[0] + RI.currententity->axis[2][0] * 15,
		RI.currententity->origin[1] + RI.currententity->axis[2][1] * 15,
		RI.currententity->origin[2] + RI.currententity->axis[2][2] * 15 );

	pglEnd();
}

/*
=============
R_DrawBmodelEntities
=============
*/
static void R_DrawBmodelEntities( void )
{
	int i, j;

	for( i = 0; i < r_numbmodelentities; i++ )
	{
		RI.previousentity = RI.currententity;
		RI.currententity = r_bmodelentities[i];
		j = RI.currententity - r_entities;
		if( r_entVisBits[j>>3] & ( 1<<( j&7 ) ) )
			R_AddBrushModelToList( RI.currententity );
	}
}

/*
=============
R_DrawRegularEntities
=============
*/
static void R_DrawRegularEntities( void )
{
	unsigned int i;
	ref_entity_t *e;
	bool shadowmap = ( ( RI.params & RP_SHADOWMAPVIEW ) != 0 );

	for( i = 1; i < r_numEntities; i++ )
	{
		RI.previousentity = RI.currententity;
		RI.currententity = e = &r_entities[i];

		if( shadowmap )
		{
			if( e->flags & EF_NOSHADOW )
				continue;
			if( r_entShadowBits[i] & RI.shadowGroup->bit )
				goto add; // shadow caster
		}
		if( !( r_entVisBits[i>>3] & ( 1<<( i&7 ) ) ) )
			continue;

add:
		switch( e->rtype )
		{
		case RT_MODEL:
			RI.currentmodel = e->model;
			switch( RI.currentmodel->type )
			{
			case mod_alias:
				R_AddAliasModelToList( e );
				break;
			case mod_studio:
				break;
			case mod_sprite:
				if( !shadowmap )
					R_AddSpriteModelToList( e );
				break;
			default:	break;
			}
			break;
		case RT_SPRITE:
			if( !shadowmap )
				R_AddSpritePolyToList( e );
			break;
		default:
			break;
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
	int i;

	if( !r_numnullentities )
		return;

	pglDisable( GL_TEXTURE_2D );
	GL_SetState( GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	// draw non-transparent first
	for( i = 0; i < r_numnullentities; i++ )
	{
		RI.previousentity = RI.currententity;
		RI.currententity = r_nullentities[i];

		if( RI.params & RP_MIRRORVIEW )
		{
			if( RI.currententity->ent_type == ED_VIEWMODEL )
				continue;
		}
		else
		{
			if( RP_LOCALCLIENT( RI.currententity ))
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
	bool shadowmap = ( ( RI.params & RP_SHADOWMAPVIEW ) != 0 );

	if( !r_drawentities->integer )
		return;

	if( !shadowmap )
	{
		R_CullEntities(); // mark visible entities in r_entVisBits

		R_CullShadowmapGroups();
	}

	// we don't mark bmodel entities in RP_SHADOWMAPVIEW, only individual surfaces
	R_DrawBmodelEntities();

	if( OCCLUSION_QUERIES_ENABLED( RI ) )
	{
		R_EndOcclusionPass();
	}

	if( RI.params & RP_ENVVIEW )
		return;

	if( !shadowmap )
		R_DrawShadowmaps(); // render to depth textures, mark shadowed entities and surfaces
	else if( !( RI.params & RP_WORLDSURFVISIBLE ) || ( prevRI.shadowBits & RI.shadowGroup->bit ) )
		return; // we're supposed to cast shadows but there are no visible surfaces for this light, so stop
	// or we've already drawn and captured textures for this group

	R_DrawRegularEntities();
}


/*
===============
R_RenderDebugSurface
===============
*/
void R_RenderDebugSurface( void )
{
	trace_t tr;
	vec3_t forward;
	vec3_t start, end;

	if( RI.params & RP_NONVIEWERREF || RI.refdef.rdflags & RDF_NOWORLDMODEL )
		return;

	r_debug_surface = NULL;
	if( r_speeds->integer != 4 )
		return;

	VectorCopy( RI.vpn, forward );
	VectorCopy( RI.viewOrigin, start );
	VectorMA( start, 4096, forward, end );

	r_debug_surface = R_TraceLine( &tr, start, end, 0 );
	if( r_debug_surface && r_debug_surface->mesh && !r_showtris->integer )
	{
		RI.previousentity = NULL;
		RI.currententity = (ref_entity_t *)tr.gp;

		R_ClearMeshList( RI.meshlist );
		R_AddMeshToList( MB_MODEL, NULL, r_debug_surface->shader, r_debug_surface - r_worldbrushmodel->surfaces + 1 );
		R_DrawTriangleOutlines( true, false );
	}
}

/*
================
R_RenderView

RI.refdef must be set before the first call
================
*/
void R_RenderView( const ref_params_t *fd )
{
	int msec = 0;
	bool shadowMap = RI.params & RP_SHADOWMAPVIEW ? true : false;

	RI.refdef = *fd;

	R_ClearMeshList( RI.meshlist );

	if( !r_worldmodel && !( RI.refdef.rdflags & RDF_NOWORLDMODEL ) )
		Host_Error( "R_RenderView: NULL worldmodel\n" );

	R_SetupFrame();

	// we know the farclip so adjust fov before setting up the frustum
	if( shadowMap )
	{
		R_SetupViewMatrices();
	}
	else if( OCCLUSION_QUERIES_ENABLED( RI ) )
	{
		R_SetupViewMatrices();

		R_SetupGL();

		R_Clear( ~( GL_STENCIL_BUFFER_BIT|GL_COLOR_BUFFER_BIT ) );

		R_BeginOcclusionPass();
	}

	R_SetupFrustum();

	if( r_speeds->integer )
		msec = Sys_Milliseconds();
	R_MarkLeaves();
	if( r_speeds->integer )
		r_mark_leaves += ( Sys_Milliseconds() - msec );

	R_DrawWorld();

	// we know the the farclip at this point after determining visible world leafs
	if( !shadowMap )
	{
		R_SetupViewMatrices();

		R_DrawCoronas();

		if( r_speeds->integer )
			msec = Sys_Milliseconds();
		R_AddPolysToList();
		if( r_speeds->integer )
			r_add_polys += ( Sys_Milliseconds() - msec );
	}

	if( r_speeds->integer )
		msec = Sys_Milliseconds();
	R_DrawEntities();
	if( r_speeds->integer )
		r_add_entities += ( Sys_Milliseconds() - msec );

	if( shadowMap )
	{
		if( !( RI.params & RP_WORLDSURFVISIBLE ) )
			return; // we didn't cast shadows on anything, so stop
		if( prevRI.shadowBits & RI.shadowGroup->bit )
			return; // already drawn
	}

	if( r_speeds->integer )
		msec = Sys_Milliseconds();
	R_SortMeshes();
	if( r_speeds->integer )
		r_sort_meshes += ( Sys_Milliseconds() - msec );

	R_DrawPortals();

	if( r_portalonly->integer && !( RI.params & ( RP_MIRRORVIEW|RP_PORTALVIEW ) ) )
		return;

	R_SetupGL();

	R_Clear( shadowMap ? ~( GL_STENCIL_BUFFER_BIT|GL_COLOR_BUFFER_BIT ) : ~0 );

	if( r_speeds->integer )
		msec = Sys_Milliseconds();
	R_DrawMeshes();
	if( r_speeds->integer )
		r_draw_meshes += ( Sys_Milliseconds() - msec );

	R_BackendCleanUpTextureUnits();

	R_DrawTriangleOutlines( r_showtris->integer ? true : false, r_shownormals->integer ? true : false );

	R_RenderDebugSurface ();

	R_DrawPhysDebug ();

	R_DrawNullEntities();

	R_EndGL();
}

//=======================================================================
/*
===============
R_BeginFrame
===============
*/
void R_BeginFrame( void )
{
	if( gl_finish->integer && gl_delayfinish->integer )
	{
		// flush any remaining 2D bits
//		R_Set2DMode( false );

		// apply software gamma
//		R_ApplySoftwareGamma ();

		pglFinish();

		R_CheckForErrors ();

		// Swap the buffers
		if( !r_frontbuffer->integer )
		{
			if( !pwglSwapBuffers( glw_state.hDC ))
				Sys_Break( "R_EndFrame() - SwapBuffers() failed!\n" );
		}
	}

	pglDrawBuffer( GL_BACK );

	if( r_colorbits->modified )
	{
		r_colorbits->modified = false;
	}

	if( r_environment_color->modified )
	{
		VectorClear( mapConfig.environmentColor );
		mapConfig.environmentColor[3] = 255;

		if( r_environment_color->string[0] )
		{
			int	r, g, b;

			if( sscanf( r_environment_color->string, "%i %i %i", &r, &g, &b ) == 3 )
			{
				mapConfig.environmentColor[0] = bound( 0, r, 255 );
				mapConfig.environmentColor[1] = bound( 0, g, 255 );
				mapConfig.environmentColor[2] = bound( 0, b, 255 );
			}
			else
			{
				Cvar_Set( "r_environment_color", "" );
			}
		}
		r_environment_color->modified = false;
	}

	if( r_clear->integer )
	{
		rgba_t color;
		
		Vector4Copy( mapConfig.environmentColor, color );
		pglClearColor( color[0]*( 1.0/255.0 ), color[1]*( 1.0/255.0 ), color[2]*( 1.0/255.0 ), 1 );
		pglClear( GL_COLOR_BUFFER_BIT );
	}
	// update gamma
	if( r_gamma->modified )
	{
		r_gamma->modified = false;
		GL_UpdateGammaRamp();
	}

	// run cinematic passes on shaders
	R_RunAllCinematics();

	// go into 2D mode
	R_Set2DMode( true );

	// draw buffer stuff
	if( r_frontbuffer->modified )
	{
		r_frontbuffer->modified = false;

		if( r_frontbuffer->integer )
			pglDrawBuffer( GL_FRONT );
		else pglDrawBuffer( GL_BACK );
	}

	// texturemode stuff
	// update texture parameters
	if( gl_texturemode->modified || gl_texture_anisotropy->modified || gl_texture_lodbias ->modified )
		R_SetTextureParameters();

	// swapinterval stuff
	GL_UpdateSwapInterval();
}


/*
====================
R_ClearScene
====================
*/
void R_ClearScene( void )
{
	r_numEntities = 1;
	r_numDlights = 0;
	r_numPolys = 0;
	RI.previousentity = NULL;
	RI.currententity = r_worldent;
	RI.currentmodel = r_worldmodel;
}

/*
=====================
R_AddPolyToScene
=====================
*/
bool R_AddPolyToScene( const poly_t *poly )
{
	if(( r_numPolys < MAX_POLYS ) && poly && poly->numverts )
	{
		poly_t *dp = &r_polys[r_numPolys++];

		*dp = *poly;
		if( dp->numverts > MAX_POLY_VERTS )
			dp->numverts = MAX_POLY_VERTS;
		dp->shader = &r_shaders[poly->shadernum];
		return true;
	}
	return false;
}

/*
=====================
R_AddLightStyleToScene
=====================
*/
void R_AddLightStyleToScene( int style, float r, float g, float b )
{
	lightstyle_t *ls;

	if( style < 0 || style > MAX_LIGHTSTYLES )
		Host_Error( "R_AddLightStyleToScene: bad light style %i\n", style );

	ls = &r_lightStyles[style];
	ls->rgb[0] = max( 0, r );
	ls->rgb[1] = max( 0, g );
	ls->rgb[2] = max( 0, b );
}

/*
===============
R_RenderScene
===============
*/
void R_RenderScene( const ref_params_t *fd )
{
	// flush any remaining 2D bits
	R_Set2DMode( false );

	if( r_norefresh->integer )
		return;

	R_BackendStartFrame();

	if( !( fd->rdflags & RDF_NOWORLDMODEL ) )
	{
		r_lastRefdef = *fd;
	}

	c_brush_polys = 0;
	c_world_leafs = 0;

	r_mark_leaves =
		r_add_polys =
		r_add_entities =
		r_sort_meshes =
		r_draw_meshes =
		r_world_node = 0;

	RI.params = RP_NONE;
	RI.refdef = *fd;
	RI.farClip = 0;
	RI.clipFlags = 15;
	if( r_worldmodel && !( RI.refdef.rdflags & RDF_NOWORLDMODEL ) && r_worldbrushmodel->globalfog )
	{
		RI.farClip = r_worldbrushmodel->globalfog->shader->fog_dist;
		RI.farClip = max( r_farclip_min, RI.farClip ) + r_farclip_bias;
		RI.clipFlags |= 16;
	}
	RI.meshlist = &r_worldlist;
	RI.shadowBits = 0;
	RI.shadowGroup = NULL;

	// adjust field of view for widescreen
	if( glState.wideScreen && !( fd->rdflags & RDF_NOFOVADJUSTMENT ) )
		AdjustFov( &RI.refdef.fov_x, &RI.refdef.fov_y, glState.width, glState.height, false );

	Vector4Set( RI.scissor, fd->viewport[0], glState.height - fd->viewport[3] - fd->viewport[1], fd->viewport[2], fd->viewport[3] );
	Vector4Set( RI.viewport, fd->viewport[0], glState.height - fd->viewport[3] - fd->viewport[1], fd->viewport[2], fd->viewport[3] );
	VectorCopy( fd->vieworg, RI.pvsOrigin );
	VectorCopy( fd->vieworg, RI.lodOrigin );

	if( gl_finish->integer && !gl_delayfinish->integer && !( fd->rdflags & RDF_NOWORLDMODEL ) )
		pglFinish();

	R_ClearShadowmaps();

	R_CategorizeEntities();

	R_RenderView( fd );

	R_BloomBlend( fd );

	R_PolyBlend();

	R_BackendEndFrame();

	R_Set2DMode( true );

	R_ShowTextures();
}

/*
===============
R_EndFrame
===============
*/
void R_EndFrame( void )
{
	// flush any remaining 2D bits
	R_Set2DMode( false );

	// cleanup texture units
	R_BackendCleanUpTextureUnits();

	// apply software gamma
	R_ApplySoftwareGamma();

	if( gl_finish->integer && gl_delayfinish->integer )
	{
		pglFlush();
		return;
	}

	R_CheckForErrors ();

	// Swap the buffers
	if( !r_frontbuffer->integer )
	{
		if( !pwglSwapBuffers( glw_state.hDC ))
			Sys_Break( "R_EndFrame() - SwapBuffers() failed!\n" );
	}
}

/*
===============
R_SpeedsMessage
===============
*/
const char *R_SpeedsMessage( char *out, size_t size )
{
	if( out )
		com.strncpy( out, r_speeds_msg, size );
	return out;
}

//==================================================================================

/*
=============
R_TransformToScreen_Vec3
=============
*/
void R_TransformToScreen_Vec3( vec3_t in, vec3_t out )
{
	vec4_t temp, temp2;

	temp[0] = in[0];
	temp[1] = in[1];
	temp[2] = in[2];
	temp[3] = 1.0f;
	Matrix4_Multiply_Vector( RI.worldviewProjectionMatrix, temp, temp2 );

	if( !temp2[3] )
		return;

	out[0] = ( temp2[0] / temp2[3] + 1.0f ) * 0.5f * RI.refdef.viewport[2];
	out[1] = ( temp2[1] / temp2[3] + 1.0f ) * 0.5f * RI.refdef.viewport[3];
	out[2] = ( temp2[2] / temp2[3] + 1.0f ) * 0.5f;
}

/*
=============
R_TransformVectorToScreen
=============
*/
void R_TransformVectorToScreen( const ref_params_t *rd, const vec3_t in, vec2_t out )
{
	mat4x4_t p, m;
	vec4_t temp, temp2;

	if( !rd || !in || !out )
		return;

	temp[0] = in[0];
	temp[1] = in[1];
	temp[2] = in[2];
	temp[3] = 1.0f;

	R_SetupProjectionMatrix( rd, p );
	R_SetupModelviewMatrix( rd, m );

	Matrix4_Multiply_Vector( m, temp, temp2 );
	Matrix4_Multiply_Vector( p, temp2, temp );

	if( !temp[3] )
		return;

	out[0] = rd->viewport[0] + ( temp[0] / temp[3] + 1.0f ) * rd->viewport[2] * 0.5f;
	out[1] = rd->viewport[1] + ( temp[1] / temp[3] + 1.0f ) * rd->viewport[3] * 0.5f;
}

//==================================================================================

/*
=============
R_TraceLine
=============
*/
msurface_t *R_TraceLine( trace_t *tr, const vec3_t start, const vec3_t end, int surfumask )
{
	int i;
	msurface_t *surf;

	// trace against world
	surf = R_TransformedTraceLine( tr, start, end, r_worldent, surfumask );

	// trace against bmodels
	for( i = 0; i < r_numbmodelentities; i++ )
	{
		trace_t t2;
		msurface_t *s2;

		s2 = R_TransformedTraceLine( &t2, start, end, r_bmodelentities[i], surfumask );
		if( t2.fraction < tr->fraction )
		{
			*tr = t2;	// closer impact point
			surf = s2;
		}
	}

	return surf;
}

bool R_UploadModel( const char *name, int index )
{
	ref_model_t	*mod;

	// this array used by AddEntityToScene
	mod = R_RegisterModel( name );
	cl_models[index] = mod;

	return (mod != NULL);	
}

shader_t Mod_RegisterShader( const char *name, int shaderType )
{
	ref_shader_t	*src;

	if( !glState.initializedMedia )
		return 0;

	switch( shaderType )
	{
	case SHADER_FONT:
	case SHADER_NOMIP:
		src = R_LoadShader( name, shaderType, false, TF_CLAMP|TF_NOMIPMAP|TF_NOPICMIP, SHADER_INVALID );
		break;
	case SHADER_GENERIC:
		src = R_LoadShader( name, shaderType, false, 0, SHADER_INVALID );
		break;
	case SHADER_SKY:
		src = R_SetupSky( name );
		break;
	default:
		MsgDev( D_WARN, "Mod_RegisterShader: invalid shader type (%i)\n", shaderType );	
		return 0;
	}
	return src - r_shaders;
}

bool R_AddLightStyle( int stylenum, vec3_t color )
{
	if( stylenum < 0 || stylenum > MAX_LIGHTSTYLES )
		return false; // invalid lightstyle

	R_AddLightStyleToScene( stylenum, color[0], color[1], color[2] );
	return true;
}

bool R_AddGenericEntity( edict_t *pRefEntity, ref_entity_t *refent, int ed_type, float lerpfrac )
{
	int	i;

	// check model
	if( !refent->model ) return false;

	switch( refent->model->type )
	{
	case mod_world:
	case mod_brush: break;
	case mod_studio:
		if( !refent->model->extradata )
			return false;
		refent->rtype = RT_MODEL;
		break;
	case mod_sprite:		
		if( !refent->model->extradata )
			return false;
		refent->rtype = RT_MODEL;
		break;
	case mod_bad: // let the render drawing null model
		break;
	}

	// setup latchedvars
	VectorCopy( pRefEntity->v.oldorigin, refent->prev.origin );
	VectorCopy( pRefEntity->v.oldangles, refent->prev.angles );
	VectorCopy( pRefEntity->v.origin, refent->lightingOrigin );

	// do animate
	if( refent->flags & EF_ANIMATE )
	{
		switch( refent->model->type )
		{
		case mod_studio:
			if( pRefEntity->v.frame == -1 )
			{
				pRefEntity->v.frame = refent->frame = 0;
				refent->sequence = pRefEntity->v.sequence;
//				R_StudioResetSequenceInfo( refent, refent->model->phdr );
			}
			else
			{
//				R_StudioFrameAdvance( refent, 0 );

				if( refent->m_fSequenceFinished )
				{
					if( refent->m_fSequenceLoops )
						pRefEntity->v.frame = -1;
					// hold at last frame
				}
				else
				{
					// copy current frame back to let user grab it on a client-side
					pRefEntity->v.frame = refent->frame;
				}
			}
			break;
		case mod_sprite:
		case mod_brush:
		case mod_world:
			break;
		}
          }
	else
	{
		refent->prev.frame = refent->frame;
		refent->frame = pRefEntity->v.frame;
		refent->prev.sequence = refent->sequence;
		refent->prev.animtime = refent->animtime;
		refent->animtime = pRefEntity->v.animtime;
		refent->sequence = pRefEntity->v.sequence;
	}

	refent->prev.sequencetime = refent->animtime - refent->prev.animtime;
	refent->weaponmodel = cl_models[pRefEntity->v.weaponmodel];

	if( refent->ent_type == ED_MOVER || refent->ent_type == ED_BSPBRUSH )
	{
		VectorNormalize2( pRefEntity->v.movedir, refent->movedir );
	}
	else VectorClear( refent->movedir );

	// calculate angles
	if( refent->flags & EF_ROTATE )
	{	
		// some bonus items auto-rotate
		VectorSet( refent->angles, 0, anglemod( RI.refdef.time / 10), 0 );
	}
	else
	{	
		// interpolate angles
		for( i = 0; i < 3; i++ )
			refent->angles[i] = LerpAngle( pRefEntity->v.oldangles[i], pRefEntity->v.angles[i], lerpfrac );
	}

	// interpolate origin
	for( i = 0; i < 3; i++ )
		refent->origin[i] = LerpPoint( pRefEntity->v.oldorigin[i], pRefEntity->v.origin[i], lerpfrac );
	Matrix3x3_FromAngles( refent->angles, refent->axis );
	VectorClear( refent->origin2 );

	if( refent->ent_type == ED_VIEWMODEL )
	{
		if( r_lefthand->integer == 1 )
			VectorNegate( refent->axis[1], refent->axis[1] ); 
	}

	// copy controllers
	for( i = 0; i < MAXSTUDIOCONTROLLERS; i++ )
	{
		refent->prev.controller[i] = refent->controller[i];
		refent->controller[i] = pRefEntity->v.controller[i];
	}

	// copy blends
	for( i = 0; i < MAXSTUDIOBLENDS; i++ )
	{
		refent->prev.blending[i] = refent->blending[i];
		refent->blending[i] = pRefEntity->v.blending[i];
	}

	if( refent->ent_type == ED_CLIENT )
	{
		refent->gaitsequence = pRefEntity->v.gaitsequence;
		refent->flags |= EF_OCCLUSIONTEST;
	}
	else refent->gaitsequence = 0;

	// because entity without models never added to scene
	if( !refent->ent_type )
	{
		switch( refent->model->type )
		{
		case mod_brush:
		case mod_world:
			refent->ent_type = ED_BSPBRUSH;
			break;
		case mod_studio:
		case mod_sprite:		
			refent->ent_type = ED_NORMAL;
          		break;
		// and ignore all other unset ents
		}
	}
	return true;
}

bool R_AddPortalEntity( edict_t *pRefEntity, ref_entity_t *refent, int ed_type, float lerpfrac )
{
	refent->rtype = RT_PORTALSURFACE;

	VectorClear( refent->angles );
	VectorCopy( pRefEntity->v.movedir, refent->movedir );
	VectorCopy( pRefEntity->v.origin, refent->origin );
	VectorCopy( pRefEntity->v.oldorigin, refent->origin2 );

	if( refent->flags & EF_ROTATE )
	{
		float	phase = pRefEntity->v.frame;
		float	speed = (pRefEntity->v.framerate ? pRefEntity->v.framerate : 50.0f);
		refent->angles[ROLL] = 5 * sin(( phase + RI.refdef.time * speed * 0.01f ) * M_PI2);
	}

	// calculate angles
	Matrix3x3_FromAngles( refent->angles, refent->axis );
	return true;
}

bool R_AddEntityToScene( edict_t *pRefEntity, int ed_type, float lerpfrac )
{
	ref_entity_t	*refent;
	bool		result = false;

	if( !pRefEntity || !pRefEntity->v.modelindex )
		return false; // if set to invisible, skip
	if( r_numEntities >= MAX_ENTITIES ) return false;

	refent = &r_entities[r_numEntities];

	if( pRefEntity->v.effects & EF_NODRAW )
		return true; // done

	// filter ents
	switch( ed_type )
	{
	case ED_MOVER:
	case ED_PORTAL:
	case ED_CLIENT:
	case ED_NORMAL:
	case ED_MONSTER:
	case ED_BSPBRUSH:
	case ED_RIGIDBODY:
	case ED_VIEWMODEL: break;
	default: return false;
	}

	// copy state to render
	refent->index = pRefEntity->serialnumber;
	refent->ent_type = ed_type;
	refent->backlerp = 1.0f - lerpfrac;
	refent->rendermode = pRefEntity->v.rendermode;
	refent->body = pRefEntity->v.body;
	refent->skin = pRefEntity->v.skin;
	refent->scale = pRefEntity->v.scale;
	refent->colormap = pRefEntity->v.colormap;
	refent->flags = pRefEntity->v.effects;
	refent->renderfx = pRefEntity->v.renderfx;
	VectorCopy( pRefEntity->v.rendercolor, refent->rendercolor );
	refent->renderamt = pRefEntity->v.renderamt;
	refent->model = cl_models[pRefEntity->v.modelindex];
	refent->movetype = pRefEntity->v.movetype;
	refent->framerate = pRefEntity->v.framerate;

	// setup rtype
	switch( ed_type )
	{
	case ED_PORTAL:
		result = R_AddPortalEntity( pRefEntity, refent, ed_type, lerpfrac );
		break;
	default:
		result = R_AddGenericEntity( pRefEntity, refent, ed_type, lerpfrac );
		break;
	}

	// add entity
	r_numEntities++;

	return result;
}

bool R_AddDynamicLight( vec3_t org, vec3_t color, float intensity, shader_t handle )
{
	dlight_t		*dl;
	ref_shader_t	*shader;

	if(( r_numDlights >= MAX_DLIGHTS ) || (intensity == 0) || ( VectorIsNull( color )))
		return false;
	if( handle < 0 || handle > MAX_SHADERS || !(shader = &r_shaders[handle])->name)
		shader = NULL;
	dl = &r_dlights[r_numDlights++];

	VectorCopy( org, dl->origin );
	VectorCopy( color, dl->color );
	dl->intensity = intensity * DLIGHT_SCALE;
	dl->shader = shader;

	R_LightBounds( org, dl->intensity, dl->mins, dl->maxs );

	return true;
}

void R_LightForPoint( const vec3_t point, vec3_t ambientLight )
{
	vec4_t	ambient;
	vec3_t	dir;

	R_LightForOrigin( point, dir, ambient, NULL, 64.0f );
	VectorCopy( ambient, ambientLight );
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
	re.RegisterShader = Mod_RegisterShader;
	re.EndRegistration = R_EndRegistration;

	re.AddLightStyle = R_AddLightStyle;
	re.AddRefEntity = R_AddEntityToScene;
	re.AddDynLight = R_AddDynamicLight;
	re.AddPolygon = R_AddPolyToScene;
	re.ClearScene = R_ClearScene;

	re.BeginFrame = R_BeginFrame;
	re.RenderFrame = R_RenderScene;
	re.EndFrame = R_EndFrame;

	re.SetColor = R_DrawSetColor;
	re.GetParms = R_DrawGetParms;
	re.SetParms = R_DrawSetParms;
	re.ScrShot = VID_ScreenShot;
	re.EnvShot = VID_CubemapShot;
	re.LightForPoint = R_LightForPoint;
	re.DrawFill = R_DrawFill;
	re.DrawStretchRaw = R_DrawStretchRaw;
	re.DrawStretchPic = R_DrawStretchPic;
	re.GetFragments = R_GetClippedFragments;

	return &re;
}