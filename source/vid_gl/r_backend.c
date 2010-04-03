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

#define FTABLE_SIZE_POW		10
#define FTABLE_SIZE			( 1<<FTABLE_SIZE_POW )
#define FTABLE_CLAMP( x )		(((uint)( ( x )*FTABLE_SIZE ) & ( FTABLE_SIZE-1 )))
#define FTABLE_EVALUATE( table, x )	(( table )[FTABLE_CLAMP( x )] )

static float			r_sintable[FTABLE_SIZE];
static float			r_sintableByte[256];
static float			r_triangletable[FTABLE_SIZE];
static float			r_squaretable[FTABLE_SIZE];
static float			r_sawtoothtable[FTABLE_SIZE];
static float			r_inversesawtoothtable[FTABLE_SIZE];
static float			r_warpsintable[256] =
{
#include "warpsin.h"
};

#define NOISE_SIZE			256
#define NOISE_VAL( a )	  	r_noiseperm[( a ) & ( NOISE_SIZE - 1 )]
#define NOISE_INDEX( x, y, z, t )	NOISE_VAL( x + NOISE_VAL( y + NOISE_VAL( z + NOISE_VAL( t ) ) ) )
#define NOISE_LERP( a, b, w )		( a * ( 1.0f - w ) + b * w )

static float r_noisetable[NOISE_SIZE];
static int r_noiseperm[NOISE_SIZE];

ALIGN( 16 ) vec4_t inVertsArray[MAX_ARRAY_VERTS];
ALIGN( 16 ) vec4_t inNormalsArray[MAX_ARRAY_VERTS];
vec4_t inSVectorsArray[MAX_ARRAY_VERTS];
elem_t inElemsArray[MAX_ARRAY_ELEMENTS];
vec2_t inCoordsArray[MAX_ARRAY_VERTS];
vec2_t inLightmapCoordsArray[LM_STYLES][MAX_ARRAY_VERTS];
rgba_t inColorsArray[LM_STYLES][MAX_ARRAY_VERTS];
vec2_t tUnitCoordsArray[MAX_TEXTURE_UNITS][MAX_ARRAY_VERTS];

elem_t *elemsArray;
vec4_t *vertsArray;
vec4_t *normalsArray;
vec4_t *sVectorsArray;
vec2_t *coordsArray;
vec2_t *lightmapCoordsArray[LM_STYLES];
rgba_t colorArray[MAX_ARRAY_VERTS];

ref_globals_t	tr;
ref_backacc_t	r_backacc;

bool		r_triangleOutlines;

static vec4_t	colorWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
static vec4_t	colorRed	=  { 1.0f, 0.0f, 0.0f, 1.0f };
static vec4_t	colorGreen = { 0.0f, 1.0f, 0.0f, 1.0f };
static vec4_t	colorBlue	=  { 0.0f, 0.0f, 1.0f, 1.0f };
static bool	r_arraysLocked;
static bool	r_normalsEnabled;

static int r_lightmapStyleNum[MAX_TEXTURE_UNITS];
static superLightStyle_t *r_superLightStyle;

static const meshbuffer_t *r_currentMeshBuffer;
static uint r_currentDlightBits;
static uint r_currentShadowBits;
static const ref_shader_t *r_currentShader;
static double r_currentShaderTime;
static int r_currentShaderState;
static int r_currentShaderPassMask;
static const shadowGroup_t *r_currentCastGroup;
static const mfog_t *r_texFog, *r_colorFog;

static ref_stage_t r_dlightsPass, r_fogPass;
static float r_lightmapPassesArgs[MAX_TEXTURE_UNITS+1][3];
static ref_stage_t r_lightmapPasses[MAX_TEXTURE_UNITS+1];
static ref_stage_t r_GLSLpasses[4];       // dlights and base
static ref_stage_t r_GLSLpassOutline;

static ref_stage_t *r_accumPasses[MAX_TEXTURE_UNITS];
static int r_numAccumPasses;

static int r_identityLighting;

int r_features;

static void R_DrawTriangles( void );
static void R_DrawNormals( void );
static void R_CleanUpTextureUnits( int last );
static void R_AccumulatePass( ref_stage_t *pass );

/*
==============
R_BackendInit
==============
*/
void R_BackendInit( void )
{
	int	i;
	float	t;

	r_numAccumPasses = 0;

	r_arraysLocked = false;
	r_triangleOutlines = false;
	tr.iRenderMode = kRenderNormal;

	R_ClearArrays();
	R_InitVertexBuffers();
	R_BackendResetPassMask();

	pglEnableClientState( GL_VERTEX_ARRAY );

	if( !r_ignorehwgamma->integer )
		r_identityLighting = (int)( 255.0f / pow( 2, max( 0, floor( r_overbrightbits->value ))));
	else r_identityLighting = 255;

	// build lookup tables
	for( i = 0; i < FTABLE_SIZE; i++ )
	{
		t = (float)i / (float)FTABLE_SIZE;

		r_sintable[i] = sin( t * M_PI2 );

		if( t < 0.25f ) r_triangletable[i] = t * 4.0f;
		else if( t < 0.75f ) r_triangletable[i] = 2 - 4.0f * t;
		else r_triangletable[i] = ( t - 0.75f ) * 4.0f - 1.0f;

		if( t < 0.5f ) r_squaretable[i] = 1.0f;
		else r_squaretable[i] = -1.0f;

		r_sawtoothtable[i] = t;
		r_inversesawtoothtable[i] = 1.0f - t;
	}

	for( i = 0; i < 256; i++ )
		r_sintableByte[i] = sin((float)i / 255.0f * M_PI2 );

	// init the noise table
	for( i = 0; i < NOISE_SIZE; i++ )
	{
		r_noisetable[i] = Com_RandomFloat( -1.0f, 1.0f );
		r_noiseperm[i] = Com_RandomLong( 0, 255 );
	}

	// init dynamic lights pass
	Mem_Set( &r_dlightsPass, 0, sizeof( ref_stage_t ) );
	r_dlightsPass.flags = SHADERSTAGE_DLIGHT;
	r_dlightsPass.glState = GLSTATE_DEPTHFUNC_EQ|GLSTATE_SRCBLEND_DST_COLOR|GLSTATE_DSTBLEND_ONE;

	// init fog pass
	Mem_Set( &r_fogPass, 0, sizeof( ref_stage_t ));
	r_fogPass.tcgen = TCGEN_FOG;
	r_fogPass.rgbGen.type = RGBGEN_FOG;
	r_fogPass.alphaGen.type = ALPHAGEN_IDENTITY;
	r_fogPass.flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_DECAL;
	r_fogPass.glState = GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;

	// the very first lightmap pass is reserved for GL_REPLACE or GL_MODULATE
	Mem_Set( r_lightmapPasses, 0, sizeof( r_lightmapPasses ));
	r_lightmapPasses[0].rgbGen.args = r_lightmapPassesArgs[0];

	// the rest are GL_ADD
	for( i = 1; i < MAX_TEXTURE_UNITS+1; i++ )
	{
		r_lightmapPasses[i].flags = SHADERSTAGE_LIGHTMAP|SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_ADD;
		r_lightmapPasses[i].glState = GLSTATE_DEPTHFUNC_EQ|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
		r_lightmapPasses[i].tcgen = TCGEN_LIGHTMAP;
		r_lightmapPasses[i].alphaGen.type = ALPHAGEN_IDENTITY;
		r_lightmapPasses[i].rgbGen.args = r_lightmapPassesArgs[i];
	}

	// init optional GLSL program passes
	Mem_Set( r_GLSLpasses, 0, sizeof( r_GLSLpasses ) );
	r_GLSLpasses[0].flags = SHADERSTAGE_DLIGHT|SHADERSTAGE_BLEND_ADD;
	r_GLSLpasses[0].glState = GLSTATE_DEPTHFUNC_EQ|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;

	r_GLSLpasses[1].flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_MODULATE;
	r_GLSLpasses[1].glState = GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_SRC_COLOR;
	r_GLSLpasses[1].tcgen = TCGEN_BASE;
	r_GLSLpasses[1].rgbGen.type = RGBGEN_IDENTITY;
	r_GLSLpasses[1].alphaGen.type = ALPHAGEN_IDENTITY;
	Mem_Copy( &r_GLSLpasses[2], &r_GLSLpasses[1], sizeof( ref_stage_t ) );

	r_GLSLpasses[3].flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_MODULATE;
	r_GLSLpasses[3].glState = GLSTATE_DEPTHFUNC_EQ /*|GLSTATE_OFFSET_FILL*/|GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_SRC_COLOR;
	r_GLSLpasses[3].tcgen = TCGEN_PROJECTION_SHADOW;
	r_GLSLpasses[3].rgbGen.type = RGBGEN_IDENTITY;
	r_GLSLpasses[3].alphaGen.type = ALPHAGEN_IDENTITY;
	r_GLSLpasses[3].program = DEFAULT_GLSL_SHADOWMAP_PROGRAM;
	r_GLSLpasses[3].program_type = PROGRAM_TYPE_SHADOWMAP;

	Mem_Set( &r_GLSLpassOutline, 0, sizeof( r_GLSLpassOutline ) );
	r_GLSLpassOutline.flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_MODULATE;
	r_GLSLpassOutline.glState = GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ZERO|GLSTATE_DEPTHWRITE;
	r_GLSLpassOutline.rgbGen.type = RGBGEN_OUTLINE;
	r_GLSLpassOutline.alphaGen.type = ALPHAGEN_OUTLINE;
	r_GLSLpassOutline.tcgen = TCGEN_NONE;
	r_GLSLpassOutline.program = DEFAULT_GLSL_OUTLINE_PROGRAM;
	r_GLSLpassOutline.program_type = PROGRAM_TYPE_OUTLINE;
}

/*
==============
R_BackendShutdown
==============
*/
void R_BackendShutdown( void )
{
	R_ShutdownVertexBuffers ();
}

/*
==============
R_FastSin
==============
*/
float R_FastSin( float t )
{
	return FTABLE_EVALUATE( r_sintable, t );
}

/*
=============
NormToLatLong
=============
*/
void NormToLatLong( const vec3_t normal, byte latlong[2] )
{
	// can't do atan2 (normal[1], normal[0])
	if( normal[0] == 0 && normal[1] == 0 )
	{
		if( normal[2] > 0 )
		{
			latlong[0] = 0;	// acos ( 1 )
			latlong[1] = 0;
		}
		else
		{
			latlong[0] = 128;	// acos ( -1 )
			latlong[1] = 0;
		}
	}
	else
	{
		int	angle;

		angle = (int)(com.acos( normal[2]) * 255.0 / M_PI2 ) & 255;
		latlong[0] = angle;
		angle = (int)(com.atan2( normal[1], normal[0] ) * 255.0 / M_PI2 ) & 255;
		latlong[1] = angle;
	}
}

/*
=============
R_LatLongToNorm
=============
*/
void R_LatLongToNorm( const byte latlong[2], vec3_t out )
{
	float	sin_a, sin_b, cos_a, cos_b;

	cos_a = r_sintableByte[(latlong[0] + 64) & 255];
	sin_a = r_sintableByte[latlong[0]];
	cos_b = r_sintableByte[(latlong[1] + 64) & 255];
	sin_b = r_sintableByte[latlong[1]];

	VectorSet( out, cos_b * sin_a, sin_b * sin_a, cos_a );
}

/*
==============
R_TableForFunc
==============
*/
static float *R_TableForFunc( uint func )
{
	switch( func )
	{
	case WAVEFORM_SIN:
		return r_sintable;
	case WAVEFORM_TRIANGLE:
		return r_triangletable;
	case WAVEFORM_SQUARE:
		return r_squaretable;
	case WAVEFORM_SAWTOOTH:
		return r_sawtoothtable;
	case WAVEFORM_INVERSESAWTOOTH:
		return r_inversesawtoothtable;
	case WAVEFORM_NOISE:
		return r_sintable;  // default to sintable
	default:
		return NULL;
	}
}

static float R_TableEvaluate( waveFunc_t func, float index )
{
	float *table = R_TableForFunc( func.type );

	if( table == NULL )
	{
		if( func.type == WAVEFORM_TABLE )
			return R_LookupTable( func.tableIndex, index );
		return 1.0f; // assume error
	}
	return FTABLE_EVALUATE( table, index );
} 

/*
==============
R_BackendGetNoiseValue
==============
*/
float R_BackendGetNoiseValue( float x, float y, float z, float t )
{
	int	i;
	int	ix, iy, iz, it;
	float	fx, fy, fz, ft;
	float	front[4], back[4];
	float	fvalue, bvalue, value[2], finalvalue;

	ix = ( int )floor( x );
	fx = x - ix;
	iy = ( int )floor( y );
	fy = y - iy;
	iz = ( int )floor( z );
	fz = z - iz;
	it = ( int )floor( t );
	ft = t - it;

	for( i = 0; i < 2; i++ )
	{
		front[0] = r_noisetable[NOISE_INDEX( ix, iy, iz, it + i )];
		front[1] = r_noisetable[NOISE_INDEX( ix+1, iy, iz, it + i )];
		front[2] = r_noisetable[NOISE_INDEX( ix, iy+1, iz, it + i )];
		front[3] = r_noisetable[NOISE_INDEX( ix+1, iy+1, iz, it + i )];

		back[0] = r_noisetable[NOISE_INDEX( ix, iy, iz + 1, it + i )];
		back[1] = r_noisetable[NOISE_INDEX( ix+1, iy, iz + 1, it + i )];
		back[2] = r_noisetable[NOISE_INDEX( ix, iy+1, iz + 1, it + i )];
		back[3] = r_noisetable[NOISE_INDEX( ix+1, iy+1, iz + 1, it + i )];

		fvalue = NOISE_LERP( NOISE_LERP( front[0], front[1], fx ), NOISE_LERP( front[2], front[3], fx ), fy );
		bvalue = NOISE_LERP( NOISE_LERP( back[0], back[1], fx ), NOISE_LERP( back[2], back[3], fx ), fy );
		value[i] = NOISE_LERP( fvalue, bvalue, fz );
	}
	finalvalue = NOISE_LERP( value[0], value[1], ft );

	return finalvalue;
}

/*
==============
R_BackendResetCounters
==============
*/
void R_BackendResetCounters( void )
{
	Mem_Set( &r_backacc, 0, sizeof( r_backacc ));
}

/*
==============
R_BackendStartFrame
==============
*/
void R_BackendStartFrame( void )
{
	r_speeds_msg[0] = '\0';
	R_BackendResetCounters();
}

/*
==============
R_BackendEndFrame
==============
*/
void R_BackendEndFrame( void )
{
	// unlock arrays if any
	R_UnlockArrays();

	// clean up texture units
	R_CleanUpTextureUnits( 1 );

	if( r_speeds->integer && !( RI.refdef.flags & RDF_NOWORLDMODEL ) )
	{
		switch( r_speeds->integer )
		{
		case 1:
		default:
			com.snprintf( r_speeds_msg, sizeof( r_speeds_msg ),
				"%4i wpoly %4i leafs %4i verts\n%4i tris %4i flushes %4i locks",
				c_brush_polys,
				c_world_leafs,
				r_backacc.c_totalVerts,
				r_backacc.c_totalTris,
				r_backacc.c_totalFlushes,
				r_backacc.c_totalKeptLocks
			);
			break;
		case 2:
			com.snprintf( r_speeds_msg, sizeof( r_speeds_msg ),
				"lvs: %5i  node: %5i\nfarclip: %6.f",
				r_mark_leaves,
				r_world_node,
				RI.farClip
			);
			break;
		case 3:
			com.snprintf( r_speeds_msg, sizeof( r_speeds_msg ),
				"polys\\ents: %5i\\%5i\nsort\\draw: %5i\\%i",
				r_add_polys, r_add_entities,
				r_sort_meshes, r_draw_meshes
			);
			break;
		case 4:
			if( r_debug_surface )
			{
				com.snprintf( r_speeds_msg, sizeof( r_speeds_msg ),
					"%s", r_debug_surface->shader->name );

				if( r_debug_surface->fog && r_debug_surface->fog->shader
					&& r_debug_surface->fog->shader != r_debug_surface->shader )
				{
					com.strncat( r_speeds_msg, "\n", sizeof( r_speeds_msg ) );
					com.strncat( r_speeds_msg, r_debug_surface->fog->shader->name, sizeof( r_speeds_msg ) );
				}
			}
			break;
		case 5:
			com.snprintf( r_speeds_msg, sizeof( r_speeds_msg ),
				"%.1f %.1f %.1f\n(%.1f,%.1f,%.1f)",
				RI.refdef.vieworg[0], RI.refdef.vieworg[1], RI.refdef.vieworg[2],
				RI.refdef.viewangles[0], RI.refdef.viewangles[1], RI.refdef.viewangles[2]
			);
			break;
		case 6:
			com.snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "cl_numents %i", r_numEntities );
			break;
		}
	}
}

/*
==============
R_LockArrays
==============
*/
void R_LockArrays( int numverts )
{
	if( r_arraysLocked ) return;

	R_UpdateVertexBuffer( tr.vertexBuffer, vertsArray, numverts * sizeof( vec4_t ));
	pglVertexPointer( 3, GL_FLOAT, 16, tr.vertexBuffer->pointer );

	if( r_features & MF_ENABLENORMALS )
	{
		r_normalsEnabled = true;
		R_UpdateVertexBuffer( tr.normalBuffer, normalsArray, numverts * sizeof( vec4_t ));
		pglEnableClientState( GL_NORMAL_ARRAY );
		pglNormalPointer( GL_FLOAT, 16, tr.normalBuffer->pointer );
	}

	if( GL_Support( R_CUSTOM_VERTEX_ARRAY_EXT ))
		pglLockArraysEXT( 0, numverts );

	r_arraysLocked = true;
}

/*
==============
R_UnlockArrays
==============
*/
void R_UnlockArrays( void )
{
	if( !r_arraysLocked )
		return;

	if(GL_Support( R_CUSTOM_VERTEX_ARRAY_EXT ))
		pglUnlockArraysEXT();

	if( r_normalsEnabled )
	{
		r_normalsEnabled = false;
		pglDisableClientState( GL_NORMAL_ARRAY );
	}
	r_arraysLocked = false;
}

/*
==============
R_ClearArrays
==============
*/
void R_ClearArrays( void )
{
	int i;

	r_backacc.numVerts = 0;
	r_backacc.numElems = 0;
	r_backacc.numColors = 0;

	vertsArray = inVertsArray;
	elemsArray = inElemsArray;
	normalsArray = inNormalsArray;
	sVectorsArray = inSVectorsArray;
	coordsArray = inCoordsArray;
	for( i = 0; i < LM_STYLES; i++ )
		lightmapCoordsArray[i] = inLightmapCoordsArray[i];
}

/*
==============
R_FlushArrays
==============
*/
void R_FlushArrays( void )
{
	if( !r_backacc.numVerts || !r_backacc.numElems )
		return;

	if( r_backacc.numColors == 1 )
	{
		pglColor4ubv( colorArray[0] );
	}
	else if( r_backacc.numColors > 1 )
	{
		pglEnableClientState( GL_COLOR_ARRAY );
		R_UpdateVertexBuffer( tr.colorsBuffer, colorArray, r_backacc.numVerts * sizeof( rgba_t ));
		pglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tr.colorsBuffer->pointer );
	}

	if( r_drawelements->integer || glState.in2DMode || RI.refdef.flags & RDF_NOWORLDMODEL )
	{
		if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
			pglDrawRangeElementsEXT( GL_TRIANGLES, 0, r_backacc.numVerts, r_backacc.numElems, GL_UNSIGNED_INT, elemsArray );
		else pglDrawElements( GL_TRIANGLES, r_backacc.numElems, GL_UNSIGNED_INT, elemsArray );
	}

	if( r_backacc.numColors > 1 )
		pglDisableClientState( GL_COLOR_ARRAY );

	r_backacc.c_totalTris += r_backacc.numElems / 3;
	r_backacc.c_totalFlushes++;
}

/*
==============
GL_DisableAllTexGens
==============
*/
static _inline void GL_DisableAllTexGens( void )
{
	GL_EnableTexGen( GL_S, 0 );
	GL_EnableTexGen( GL_T, 0 );
	GL_EnableTexGen( GL_R, 0 );
	GL_EnableTexGen( GL_Q, 0 );
}

/*
==============
R_CleanUpTextureUnits
==============
*/
static void R_CleanUpTextureUnits( int last )
{
	int	i;

	for( i = glState.activeTMU; i > last - 1; i-- )
	{
		GL_DisableAllTexGens();
		GL_SetTexCoordArrayMode( 0 );

		pglDisable( GL_TEXTURE_2D );
		GL_SelectTexture( i - 1 );
	}
}

/*
================
R_DeformVertices
================
*/
void R_DeformVertices( void )
{
	uint		i, j, k;
	double		args[4], temp;
	float		deflect, *quad[4];
	const deform_t	*deformv;
	vec3_t		tv, rot_centre;

	deformv = &r_currentShader->deforms[0];
	for( i = 0; i < r_currentShader->numDeforms; i++, deformv++ )
	{
		switch( deformv->type )
		{
		case DEFORM_NONE:
			break;
		case DEFORM_WAVE:
			// Deflect vertex along its normal by wave amount
			if( deformv->func.args[3] == 0 )
			{
				temp = deformv->func.args[2];
				deflect = R_TableEvaluate( deformv->func, temp ) * deformv->func.args[1] + deformv->func.args[0];

				for( j = 0; j < r_backacc.numVerts; j++ )
					VectorMA( inVertsArray[j], deflect, inNormalsArray[j], inVertsArray[j] );
			}
			else
			{
				args[0] = deformv->func.args[0];
				args[1] = deformv->func.args[1];
				args[2] = deformv->func.args[2] + deformv->func.args[3] * r_currentShaderTime;
				args[3] = deformv->args[0];

				for( j = 0; j < r_backacc.numVerts; j++ )
				{
					temp = args[2] + args[3] * ( inVertsArray[j][0] + inVertsArray[j][1] + inVertsArray[j][2] );
					deflect = R_TableEvaluate( deformv->func, temp ) * args[1] + args[0];
					VectorMA( inVertsArray[j], deflect, inNormalsArray[j], inVertsArray[j] );
				}
			}
			break;
		case DEFORM_NORMAL:
			// without this * 0.1f deformation looks wrong, although q3a doesn't have it
			args[0] = deformv->func.args[3] * r_currentShaderTime * 0.1f;
			args[1] = deformv->func.args[1];

			for( j = 0; j < r_backacc.numVerts; j++ )
			{
				VectorScale( inVertsArray[j], 0.98f, tv );
				inNormalsArray[j][0] += args[1] *R_BackendGetNoiseValue( tv[0], tv[1], tv[2], args[0] );
				inNormalsArray[j][1] += args[1] *R_BackendGetNoiseValue( tv[0] + 100, tv[1], tv[2], args[0] );
				inNormalsArray[j][2] += args[1] *R_BackendGetNoiseValue( tv[0] + 200, tv[1], tv[2], args[0] );
				VectorNormalizeFast( inNormalsArray[j] );
			}
			break;
		case DEFORM_MOVE:
			temp = deformv->func.args[2] + r_currentShaderTime * deformv->func.args[3];
			deflect = R_TableEvaluate( deformv->func, temp ) * deformv->func.args[1] + deformv->func.args[0];

			for( j = 0; j < r_backacc.numVerts; j++ )
				VectorMA( inVertsArray[j], deflect, deformv->args, inVertsArray[j] );
			break;
		case DEFORM_BULGE:
			args[0] = deformv->args[0];
			args[1] = deformv->args[1];
			args[2] = r_currentShaderTime * deformv->args[2];

			for( j = 0; j < r_backacc.numVerts; j++ )
			{
				temp = ( coordsArray[j][0] * args[0] + args[2] ) / M_PI2;
				deflect = R_FastSin( temp ) * args[1];
				VectorMA( inVertsArray[j], deflect, inNormalsArray[j], inVertsArray[j] );
			}
			break;
		case DEFORM_AUTOSPRITE:
			{
				vec4_t *v;
				vec2_t *st;
				elem_t *elem;
				float radius;
				vec3_t point, v_centre, v_right, v_up;

				if( r_backacc.numVerts % 4 || r_backacc.numElems % 6 )
					break;

				if( RI.currententity && (RI.currentmodel != r_worldmodel) )
				{
					Matrix3x3_Transform( RI.currententity->axis, RI.vright, v_right );
					Matrix3x3_Transform( RI.currententity->axis, RI.vup, v_up );
				}
				else
				{
					VectorCopy( RI.vright, v_right );
					VectorCopy( RI.vup, v_up );
				}

				radius = RI.currententity->scale;
				if( radius && radius != 1.0f )
				{
					radius = 1.0f / radius;
					VectorScale( v_right, radius, v_right );
					VectorScale( v_up, radius, v_up );
				}

				for( k = 0, v = inVertsArray, st = coordsArray, elem = elemsArray; k < r_backacc.numVerts; k += 4, v += 4, st += 4, elem += 6 )
				{
					for( j = 0; j < 3; j++ )
						v_centre[j] = (v[0][j] + v[1][j] + v[2][j] + v[3][j]) * 0.25;

					VectorSubtract( v[0], v_centre, point );
					radius = VectorLength( point ) * 0.707106f;		// 1.0f / sqrt(2)

					// very similar to R_PushSprite
					VectorMA( v_centre, -radius, v_up, point );
					VectorMA( point, -radius, v_right, v[0] );
					VectorMA( point,  radius, v_right, v[3] );

					VectorMA( v_centre, radius, v_up, point );
					VectorMA( point, -radius, v_right, v[1] );
					VectorMA( point,  radius, v_right, v[2] );

					// reset texcoords
					Vector2Set( st[0], 0, 1 );
					Vector2Set( st[1], 0, 0 );
					Vector2Set( st[2], 1, 0 );
					Vector2Set( st[3], 1, 1 );

					// trifan elems
					elem[0] = k;
					elem[1] = k + 2 - 1;
					elem[2] = k + 2;

					elem[3] = k;
					elem[4] = k + 3 - 1;
					elem[5] = k + 3;
				}
			}
			break;
		case DEFORM_AUTOSPRITE2:
			if( r_backacc.numElems % 6 )
				break;

			for( k = 0; k < r_backacc.numElems; k += 6 )
			{
				int	long_axis = 0, short_axis = 0;
				vec3_t	axis, tmp;
				float	len[3];
				vec3_t	m0[3], m1[3], m2[3], result[3];

				quad[0] = (float *)(inVertsArray + elemsArray[k+0]);
				quad[1] = (float *)(inVertsArray + elemsArray[k+1]);
				quad[2] = (float *)(inVertsArray + elemsArray[k+2]);

				for( j = 2; j >= 0; j-- )
				{
					quad[3] = (float *)(inVertsArray + elemsArray[k+3+j]);

					if( !VectorCompare( quad[3], quad[0] ) && !VectorCompare( quad[3], quad[1] ) && !VectorCompare( quad[3], quad[2] ))
					{
						break;
					}
				}

				// build a matrix were the longest axis of the billboard is the Y-Axis
				VectorSubtract( quad[1], quad[0], m0[0] );
				VectorSubtract( quad[2], quad[0], m0[1] );
				VectorSubtract( quad[2], quad[1], m0[2] );
				len[0] = DotProduct( m0[0], m0[0] );
				len[1] = DotProduct( m0[1], m0[1] );
				len[2] = DotProduct( m0[2], m0[2] );

				if(( len[2] > len[1] ) && ( len[2] > len[0] ))
				{
					if( len[1] > len[0] )
					{
						long_axis = 1;
						short_axis = 0;
					}
					else
					{
						long_axis = 0;
						short_axis = 1;
					}
				}
				else if(( len[1] > len[2] ) && ( len[1] > len[0] ))
				{
					if( len[2] > len[0] )
					{
						long_axis = 2;
						short_axis = 0;
					}
					else
					{
						long_axis = 0;
						short_axis = 2;
					}
				}
				else if(( len[0] > len[1] ) && ( len[0] > len[2] ))
				{
					if( len[2] > len[1] )
					{
						long_axis = 2;
						short_axis = 1;
					}
					else
					{
						long_axis = 1;
						short_axis = 2;
					}
				}

				if( !len[long_axis] ) break;
				len[long_axis] = rsqrt( len[long_axis] );
				VectorScale( m0[long_axis], len[long_axis], axis );

				if( DotProduct( m0[long_axis], m0[short_axis] ) )
				{
					VectorCopy( axis, m0[1] );
					if( axis[0] || axis[1] )
						VectorVectors( m0[1], m0[0], m0[2] );
					else VectorVectors( m0[1], m0[2], m0[0] );
				}
				else
				{
					if( !len[short_axis] ) break;
					len[short_axis] = rsqrt( len[short_axis] );
					VectorScale( m0[short_axis], len[short_axis], m0[0] );
					VectorCopy( axis, m0[1] );
					CrossProduct( m0[0], m0[1], m0[2] );
				}

				for( j = 0; j < 3; j++ )
					rot_centre[j] = ( quad[0][j] + quad[1][j] + quad[2][j] + quad[3][j] ) * 0.25;

				if( RI.currententity && ( RI.currentmodel != r_worldmodel ))
				{
					VectorAdd( RI.currententity->origin, rot_centre, tv );
					VectorSubtract( RI.viewOrigin, tv, tmp );
					Matrix3x3_Transform( RI.currententity->axis, tmp, tv );
				}
				else
				{
					VectorCopy( rot_centre, tv );
					VectorSubtract( RI.viewOrigin, tv, tv );
				}

				// filter any longest-axis-parts off the camera-direction
				deflect = -DotProduct( tv, axis );

				VectorMA( tv, deflect, axis, m1[2] );
				VectorNormalizeFast( m1[2] );
				VectorCopy( axis, m1[1] );
				CrossProduct( m1[1], m1[2], m1[0] );

				Matrix3x3_Transpose( m2, m1 );
				Matrix3x3_Concat( result, m2, m0 );

				for( j = 0; j < 4; j++ )
				{
					VectorSubtract( quad[j], rot_centre, tv );
					Matrix3x3_Transform( result, tv, quad[j] );
					VectorAdd( rot_centre, quad[j], quad[j] );
				}
			}
			break;
		case DEFORM_PROJECTION_SHADOW:
			R_DeformVPlanarShadow( r_backacc.numVerts, inVertsArray[0] );
			break;
		case DEFORM_AUTOPARTICLE:
			{
				float	scale;
				vec3_t	m0[3], m1[3], m2[3], result[3];

				if( r_backacc.numElems % 6 )
					break;

				if( RI.currententity && ( RI.currentmodel != r_worldmodel ))
					Matrix3x3_FromMatrix4x4( m1, RI.modelviewMatrix );
				else Matrix3x3_FromMatrix4x4( m1, RI.worldviewMatrix );

				Matrix3x3_Transpose( m2, m1 );

				for( k = 0; k < r_backacc.numElems; k += 6 )
				{
					quad[0] = ( float * )( inVertsArray + elemsArray[k+0] );
					quad[1] = ( float * )( inVertsArray + elemsArray[k+1] );
					quad[2] = ( float * )( inVertsArray + elemsArray[k+2] );

					for( j = 2; j >= 0; j-- )
					{
						quad[3] = ( float * )( inVertsArray + elemsArray[k+3+j] );

						if( !VectorCompare( quad[3], quad[0] ) && !VectorCompare( quad[3], quad[1] ) && !VectorCompare( quad[3], quad[2] ))
						{
							break;
						}
					}

					Matrix3x3_FromPoints( quad[0], quad[1], quad[2], m0 );
					Matrix3x3_Concat( result, m2, m0 );

					// hack a scale up to keep particles from disappearing
					scale = ( quad[0][0] - RI.viewOrigin[0] ) * RI.vpn[0] + ( quad[0][1] - RI.viewOrigin[1] ) * RI.vpn[1] + ( quad[0][2] - RI.viewOrigin[2] ) * RI.vpn[2];
					if( scale < 20 ) scale = 1.5;
					else scale = 1.5 + scale * 0.006f;

					for( j = 0; j < 3; j++ )
						rot_centre[j] = ( quad[0][j] + quad[1][j] + quad[2][j] + quad[3][j] ) * 0.25;

					for( j = 0; j < 4; j++ )
					{
						VectorSubtract( quad[j], rot_centre, tv );
						Matrix3x3_Transform( result, tv, quad[j] );
						VectorMA( rot_centre, scale, quad[j], quad[j] );
					}
				}
			}
			break;
		case DEFORM_OUTLINE:
			// deflect vertex along its normal by outline amount
			deflect = RI.currententity->outlineHeight * r_outlines_scale->value;
			for( j = 0; j < r_backacc.numVerts; j++ )
				VectorMA( inVertsArray[j], deflect, inNormalsArray[j], inVertsArray[j] );
			break;
		default:	break;
		}
	}
}

/*
==============
R_VertexTCBase
==============
*/
static bool R_VertexTCBase( const ref_stage_t *pass, int unit, matrix4x4 matrix )
{
	uint	i;
	float	*outCoords;
	bool	identityMatrix = false;

	Matrix4x4_LoadIdentity( matrix );

	switch( pass->tcgen )
	{
	case TCGEN_BASE:
		GL_DisableAllTexGens();

		R_UpdateVertexBuffer( tr.tcoordBuffer[unit], coordsArray, r_backacc.numVerts * sizeof( vec2_t ));
		pglTexCoordPointer( 2, GL_FLOAT, 0, tr.tcoordBuffer[unit]->pointer );
		return true;
	case TCGEN_LIGHTMAP:
		GL_DisableAllTexGens();

		R_UpdateVertexBuffer( tr.tcoordBuffer[unit], lightmapCoordsArray[r_lightmapStyleNum[unit]], r_backacc.numVerts * sizeof( vec2_t ));
		pglTexCoordPointer( 2, GL_FLOAT, 0, tr.tcoordBuffer[unit]->pointer );
		return true;
	case TCGEN_ENVIRONMENT:
		{
			float	depth, *n;
			vec3_t	projection, transform;

			if( glState.in2DMode )
				return true;

			if( !( RI.params & RP_SHADOWMAPVIEW ))
			{
				VectorSubtract( RI.viewOrigin, RI.currententity->origin, projection );
				Matrix3x3_Transform( RI.currententity->axis, projection, transform );

				outCoords = tUnitCoordsArray[unit][0];
				for( i = 0, n = normalsArray[0]; i < r_backacc.numVerts; i++, outCoords += 2, n += 4 )
				{
					VectorSubtract( transform, vertsArray[i], projection );
					VectorNormalizeFast( projection );

					depth = DotProduct( n, projection ); depth += depth;
					outCoords[0] = 0.5 + ( n[1] * depth - projection[1] ) * 0.5f;
					outCoords[1] = 0.5 - ( n[2] * depth - projection[2] ) * 0.5f;
				}
			}

			GL_DisableAllTexGens();

			R_UpdateVertexBuffer( tr.tcoordBuffer[unit], tUnitCoordsArray[unit], r_backacc.numVerts * sizeof( vec2_t ));
			pglTexCoordPointer( 2, GL_FLOAT, 0, tr.tcoordBuffer[unit]->pointer );
			return true;
		}
	case TCGEN_VECTOR:
		{
			GLfloat genVector[2][4];

			for( i = 0; i < 3; i++ )
			{
				genVector[0][i] = pass->tcgenVec[i+0];
				genVector[1][i] = pass->tcgenVec[i+4];
			}
			genVector[0][3] = genVector[1][3] = 0;

			Matrix4x4_SetOrigin2D( matrix, pass->tcgenVec[3], pass->tcgenVec[7] );

			GL_SetTexCoordArrayMode( 0 );
			GL_EnableTexGen( GL_S, GL_OBJECT_LINEAR );
			GL_EnableTexGen( GL_T, GL_OBJECT_LINEAR );
			GL_EnableTexGen( GL_R, 0 );
			GL_EnableTexGen( GL_Q, 0 );
			pglTexGenfv( GL_S, GL_OBJECT_PLANE, genVector[0] );
			pglTexGenfv( GL_T, GL_OBJECT_PLANE, genVector[1] );
			return false;
		}
	case TCGEN_PROJECTION:
		{
			matrix4x4	m1, m2;
			GLfloat	genVector[4][4];

			GL_SetTexCoordArrayMode( 0 );

			// nonmoving objects using worldviewmatrix
			if( VectorIsNull( RI.currententity->origin ) && Matrix3x3_Compare( RI.currententity->axis, matrix3x3_identity ))
				Matrix4x4_Copy( matrix, RI.worldviewProjectionMatrix );
			else Matrix4x4_Concat( matrix, RI.projectionMatrix, RI.modelviewMatrix );

			Matrix4x4_LoadIdentity( m1 );
			Matrix4x4_ConcatScale( m1, 0.5f );
			Matrix4x4_Concat( m2, m1, matrix );

			Matrix4x4_LoadIdentity( m1 );
			Matrix4x4_ConcatTranslate( m1, 0.5f, 0.5f, 0.5f );
			Matrix4x4_Concat( matrix, m1, m2 );

			for( i = 0; i < 4; i++ )
			{
				genVector[0][i] = i == 0 ? 1 : 0;
				genVector[1][i] = i == 1 ? 1 : 0;
				genVector[2][i] = i == 2 ? 1 : 0;
				genVector[3][i] = i == 3 ? 1 : 0;
			}

			GL_EnableTexGen( GL_S, GL_OBJECT_LINEAR );
			GL_EnableTexGen( GL_T, GL_OBJECT_LINEAR );
			GL_EnableTexGen( GL_R, GL_OBJECT_LINEAR );
			GL_EnableTexGen( GL_Q, GL_OBJECT_LINEAR );

			pglTexGenfv( GL_S, GL_OBJECT_PLANE, genVector[0] );
			pglTexGenfv( GL_T, GL_OBJECT_PLANE, genVector[1] );
			pglTexGenfv( GL_R, GL_OBJECT_PLANE, genVector[2] );
			pglTexGenfv( GL_Q, GL_OBJECT_PLANE, genVector[3] );
			return false;
		}
	case TCGEN_WARP:
		for( i = 0; r_currentShader->tessSize != 0.0f && i < r_backacc.numVerts; i++ )
		{
			coordsArray[i][0] += r_warpsintable[((int)((inCoordsArray[i][1] * 8.0 + r_currentShaderTime) * (256.0/M_PI2))) & 255] * (1.0 / r_currentShader->tessSize );
			coordsArray[i][1] += r_warpsintable[((int)((inCoordsArray[i][0] * 8.0 + r_currentShaderTime) * (256.0/M_PI2))) & 255] * (1.0 / r_currentShader->tessSize );
		}
		R_UpdateVertexBuffer( tr.tcoordBuffer[unit], coordsArray, r_backacc.numVerts * sizeof( vec2_t ));
		pglTexCoordPointer( 2, GL_FLOAT, 0, tr.tcoordBuffer[unit]->pointer );
		return true;
	case TCGEN_REFLECTION_CELLSHADE:
		if( RI.currententity && !( RI.params & RP_SHADOWMAPVIEW ) )
		{
			vec3_t		dir, vpn, vright, vup;
			matrix4x4		m;

			R_LightForOrigin( RI.currententity->lightingOrigin, dir, NULL, NULL, RI.currentmodel->radius * RI.currententity->scale );

			// rotate direction
			Matrix3x3_Transform( RI.currententity->axis, dir, vpn );
			VectorNormalizeLength( vpn );
			VectorVectors( vpn, vright, vup );

			Matrix4x4_FromVectors( m, vpn, vright, vup, vec3_origin );
			Matrix4x4_Transpose( matrix, m );
		}
	case TCGEN_REFLECTION:
		GL_EnableTexGen( GL_S, GL_REFLECTION_MAP_ARB );
		GL_EnableTexGen( GL_T, GL_REFLECTION_MAP_ARB );
		GL_EnableTexGen( GL_R, GL_REFLECTION_MAP_ARB );
		GL_EnableTexGen( GL_Q, 0 );
		return true;
	case TCGEN_NORMAL:
		GL_EnableTexGen( GL_S, GL_NORMAL_MAP_ARB );
		GL_EnableTexGen( GL_T, GL_NORMAL_MAP_ARB );
		GL_EnableTexGen( GL_R, GL_NORMAL_MAP_ARB );
		GL_EnableTexGen( GL_Q, 0 );
		return true;
	case TCGEN_FOG:
		{
			int		fogPtype;
			cplane_t		*fogPlane;
			ref_shader_t	*fogShader;
			vec3_t		viewtofog;
			float		fogNormal[3], vpnNormal[3];
			float		dist, vdist, fogDist, vpnDist;

			fogPlane = r_texFog->visibleplane;
			fogShader = r_texFog->shader;

			matrix[0][0] = matrix[1][1] = 1.0 / (fogShader->fog_dist - fogShader->fog_clearDist);
			Matrix4x4_SetOrigin2D( matrix, 0, 1.5f / (float)FOG_TEXTURE_HEIGHT );

			// distance to fog
			dist = RI.fog_dist_to_eye[r_texFog-r_worldbrushmodel->fogs];

			if( r_currentShader->flags & SHADER_SKYPARMS )
			{
				if( dist > 0 ) VectorMA( RI.viewOrigin, -dist, fogPlane->normal, viewtofog );
				else VectorCopy( RI.viewOrigin, viewtofog );
			}
			else VectorCopy( RI.currententity->origin, viewtofog );

			// some math tricks to take entity's rotation matrix into account
			// for fog texture coordinates calculations:
			// M is rotation matrix, v is vertex, t is transform vector
			// n is plane's normal, d is plane's dist, r is view origin
			// (M*v + t)*n - d = (M*n)*v - ((d - t*n))
			// (M*v + t - r)*n = (M*n)*v - ((r - t)*n)
			fogNormal[0] = DotProduct( RI.currententity->axis[0], fogPlane->normal ) * RI.currententity->scale;
			fogNormal[1] = DotProduct( RI.currententity->axis[1], fogPlane->normal ) * RI.currententity->scale;
			fogNormal[2] = DotProduct( RI.currententity->axis[2], fogPlane->normal ) * RI.currententity->scale;
			fogPtype = ( fogNormal[0] == 1.0 ? PLANE_X : ( fogNormal[1] == 1.0 ? PLANE_Y : ( fogNormal[2] == 1.0 ? PLANE_Z : PLANE_NONAXIAL ) ) );
			fogDist = ( fogPlane->dist - DotProduct( viewtofog, fogPlane->normal ) );

			vpnNormal[0] = DotProduct( RI.currententity->axis[0], RI.vpn ) * RI.currententity->scale;
			vpnNormal[1] = DotProduct( RI.currententity->axis[1], RI.vpn ) * RI.currententity->scale;
			vpnNormal[2] = DotProduct( RI.currententity->axis[2], RI.vpn ) * RI.currententity->scale;
			vpnDist = ( ( RI.viewOrigin[0] - viewtofog[0] ) * RI.vpn[0] + ( RI.viewOrigin[1] - viewtofog[1] ) * RI.vpn[1] + ( RI.viewOrigin[2] - viewtofog[2] ) * RI.vpn[2] ) + fogShader->fog_clearDist;

			outCoords = tUnitCoordsArray[unit][0];
			if( dist < 0 )
			{	
				// camera is inside the fog brush
				for( i = 0; i < r_backacc.numVerts; i++, outCoords += 2 )
				{
					outCoords[0] = DotProduct( vertsArray[i], vpnNormal ) - vpnDist;
					if( fogPtype < 3 ) outCoords[1] = -( vertsArray[i][fogPtype] - fogDist );
					else outCoords[1] = -( DotProduct( vertsArray[i], fogNormal ) - fogDist );
				}
			}
			else
			{
				for( i = 0; i < r_backacc.numVerts; i++, outCoords += 2 )
				{
					if( fogPtype < 3 ) vdist = vertsArray[i][fogPtype] - fogDist;
					else vdist = DotProduct( vertsArray[i], fogNormal ) - fogDist;
					outCoords[0] = ( ( vdist < 0 ) ? ( DotProduct( vertsArray[i], vpnNormal ) - vpnDist ) * vdist / ( vdist - dist ) : 0.0f );
					outCoords[1] = -vdist;
				}
			}

			GL_DisableAllTexGens();

			R_UpdateVertexBuffer( tr.tcoordBuffer[unit], tUnitCoordsArray[unit], r_backacc.numVerts * sizeof( vec2_t ));
			pglTexCoordPointer( 2, GL_FLOAT, 0, tr.tcoordBuffer[unit]->pointer );
			return false;
		}
	case TCGEN_SVECTORS:
		GL_DisableAllTexGens();
		R_UpdateVertexBuffer( tr.tcoordBuffer[unit], sVectorsArray, r_backacc.numVerts * sizeof( vec4_t ));
		pglTexCoordPointer( 4, GL_FLOAT, 0, tr.tcoordBuffer[unit]->pointer );
		return true;
	case TCGEN_PROJECTION_SHADOW:
		GL_SetTexCoordArrayMode( 0 );
		GL_DisableAllTexGens();
		Matrix4x4_Concat( matrix, r_currentCastGroup->worldviewProjectionMatrix, RI.objectMatrix );
		break;
	default:	break;
	}
	return identityMatrix;
}

/*
================
R_ApplyTCMods
================
*/
static void R_ApplyTCMods( const ref_stage_t *pass, matrix4x4 result )
{
	int		i;
	double		f, t1, t2, sint, cost;
	matrix4x4		m1, m2;
	const tcMod_t	*tcmod;
	waveFunc_t	func;

	for( i = 0, tcmod = pass->tcMods; i < pass->numtcMods; i++, tcmod++ )
	{
		switch( tcmod->type )
		{
		case TCMOD_TRANSLATE:
			Matrix4x4_Translate2D( result, tcmod->args[0], tcmod->args[1] );
			break;
		case TCMOD_ROTATE:
			cost = tcmod->args[0] * r_currentShaderTime;
			sint = R_FastSin( cost );
			cost = R_FastSin( cost + 0.25 );
			Matrix4x4_Setup2D( m2, cost, sint, -sint, cost, 0.5f*(sint-cost+1), -0.5f*(sint+cost-1));
			Matrix4x4_Copy2D( m1, result );
			Matrix4x4_Concat2D( result, m2, m1 );
			break;
		case TCMOD_SCALE:
			Matrix4x4_Scale2D( result, tcmod->args[0], tcmod->args[1] );
			break;
		case TCMOD_TURB:
			t1 = ( 1.0f / 4.0f );
			t2 = tcmod->args[2] + r_currentShaderTime * tcmod->args[3];
			Matrix4x4_Scale2D( result, 1 + ( tcmod->args[1] * R_FastSin( t2 ) + tcmod->args[0] ) * t1, 1 + ( tcmod->args[1] * R_FastSin( t2 + 0.25 ) + tcmod->args[0] ) * t1 );
			break;
		case TCMOD_STRETCH:
			func.type = (uint)tcmod->args[0];
			func.tableIndex = (uint)tcmod->args[5];
			t2 = tcmod->args[3] + r_currentShaderTime * tcmod->args[4];
			t1 = R_TableEvaluate( func, t2 ) * tcmod->args[2] + tcmod->args[1];
			t1 = t1 ? 1.0f / t1 : 1.0f;
			t2 = 0.5f - 0.5f * t1;
			Matrix4x4_Stretch2D( result, t1, t2 );
			break;
		case TCMOD_SCROLL:
			t1 = tcmod->args[0] * r_currentShaderTime;
			t2 = tcmod->args[1] * r_currentShaderTime;
			if( pass->program_type != PROGRAM_TYPE_DISTORTION )
			{	
				// HACKHACK
				t1 = t1 - floor( t1 );
				t2 = t2 - floor( t2 );
			}
			Matrix4x4_Translate2D( result, t1, t2 );
			break;
		case TCMOD_TRANSFORM:
			Matrix4x4_Setup2D( m2, tcmod->args[0], tcmod->args[2], tcmod->args[3], tcmod->args[1], tcmod->args[4], tcmod->args[5] );
			Matrix4x4_Copy2D( m1, result );
			Matrix4x4_Concat2D( result, m1, m2 );
			break;
		case TCMOD_CONVEYOR:
			if( RI.currententity->framerate == 0.0f ) return;
			f = (RI.currententity->framerate * r_currentShaderTime) * 0.0039; // magic number :-)
			t1 = RI.currententity->movedir[0];
			t2 = RI.currententity->movedir[1];

			t1 = f * t1;
			t1 -= floor( t1 );
			t2 = f * t2;
			t2 -= floor( t2 );

			Matrix4x4_Translate2D( result, -t1, t2 );
			break;
		default: break;
		}
	}
}

/*
==============
R_ShaderpassTex
==============
*/
static _inline texture_t *R_ShaderpassTex( const ref_stage_t *pass, int unit )
{
	if( pass->flags & SHADERSTAGE_ANGLEDMAP )
	{
		float	yaw = RI.currententity->angles[1] - 45;	// angled bias
		int	angleframe = (int)((RI.refdef.viewangles[1] - yaw) / 360 * 8 + 0.5 - 4) & 7;
		if( !RI.currententity ) return pass->textures[0];	// assume error
		return pass->textures[angleframe];
	}
	if( pass->flags & SHADERSTAGE_FRAMES && !(pass->flags & SHADERSTAGE_ANIMFREQUENCY))
	{
		if( glState.in2DMode || triState.fActive )
			return pass->textures[bound( 0, glState.draw_frame, pass->num_textures - 1)];
		else if( RI.currententity && RI.currententity->model )
		{
			switch( RI.currententity->model->type )
			{
			case mod_brush:
			case mod_world:
				return pass->textures[bound( 0, (int)RI.currententity->frame, pass->num_textures - 1)];
			case mod_studio:
				return pass->textures[bound( 0, (int)RI.currententity->skin, pass->num_textures - 1)];
			}
		}
	}
	if( pass->flags & SHADERSTAGE_ANIMFREQUENCY && pass->animFrequency[0] && pass->num_textures )
	{
		int	frame, numframes;

		if( RI.currententity )
		{
			if( RI.currententity->frame && pass->animFrequency[1] != 0.0f )
			{
				numframes = bound( 1, pass->num_textures - pass->anim_offset, MAX_STAGE_TEXTURES - 1 );
				frame = (int)( pass->animFrequency[1] * r_currentShaderTime ) % numframes;
				frame = bound( 0, frame + pass->anim_offset, pass->num_textures ); // bias
			}
			else
			{
				numframes = bound( 1, pass->anim_offset, MAX_STAGE_TEXTURES - 1 );
				frame = (int)( pass->animFrequency[0] * r_currentShaderTime ) % numframes;
			}
		}
		else frame = (int)( pass->animFrequency[0] * r_currentShaderTime ) % pass->num_textures;
		return pass->textures[frame];
	}
	if( pass->flags & SHADERSTAGE_LIGHTMAP )
		return tr.lightmapTextures[r_superLightStyle->lightmapNum[r_lightmapStyleNum[unit]]];
	if( pass->flags & SHADERSTAGE_PORTALMAP )
		return tr.portaltexture1;
	return ( pass->textures[0] ? pass->textures[0] : tr.defaultTexture );
}

/*
=================
RB_SetShaderRenderMode

UNDONE: not all cases are filled
=================
*/
static void R_ShaderpassRenderMode( ref_stage_t *pass )
{
	int	mod_type = mod_bad; // mod_bad interpretate as orthogonal shader

	if(!( pass->flags & SHADERSTAGE_RENDERMODE ))
		return;

	if( RI.currentmodel && !glState.in2DMode && !triState.fActive )
		mod_type = RI.currentmodel->type;

	switch( tr.iRenderMode )
	{
	case kRenderNormal:
		// restore real state
		pass->glState = pass->prev.glState;
		pass->flags = pass->prev.flags;
		pass->rgbGen = pass->prev.rgbGen;
		pass->alphaGen = pass->prev.alphaGen;
		break;		
	case kRenderTransColor:
		switch( mod_type )
		{
		case mod_bad:
			pass->glState = (GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_SRC_COLOR);
			pass->flags = SHADERSTAGE_BLEND_DECAL;
			pass->rgbGen.type = RGBGEN_VERTEX;
			pass->alphaGen.type = ALPHAGEN_VERTEX;
			break;
		case mod_world:
		case mod_brush:
		case mod_studio:
		case mod_sprite:
			break;
		}
		break;
	case kRenderTransTexture:
		switch( mod_type )
		{
		case mod_bad:
			pass->glState = (GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA);
			pass->flags = SHADERSTAGE_BLEND_MODULATE;
			pass->rgbGen.type = RGBGEN_VERTEX;
			pass->alphaGen.type = ALPHAGEN_VERTEX;
			break;
		case mod_world:
			pass->glState = (GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA);
			pass->flags = SHADERSTAGE_BLEND_MODULATE;
			pass->rgbGen.type = RGBGEN_VERTEX;
			pass->alphaGen.type = ALPHAGEN_ENTITY;
			break;
		case mod_brush:
			pass->glState = (GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA);
			pass->flags = SHADERSTAGE_BLEND_MODULATE;
			pass->rgbGen.type = RGBGEN_VERTEX;
			pass->alphaGen.type = ALPHAGEN_ENTITY;
			break;
		case mod_studio:
			pass->glState = (GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA);
			pass->flags = SHADERSTAGE_BLEND_MODULATE;
			pass->rgbGen.type = RGBGEN_LIGHTING_DIFFUSE;
			pass->alphaGen.type = ALPHAGEN_ENTITY;
			break;
		case mod_sprite:
			pass->glState = (GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA);
			pass->flags = SHADERSTAGE_BLEND_MODULATE;
			pass->rgbGen.type = RGBGEN_VERTEX;
			pass->alphaGen.type = ALPHAGEN_VERTEX;
			break;
		}
		break;
	case kRenderGlow:
		switch( mod_type )
		{
		case mod_bad:
			pass->glState = (GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE);
			pass->flags = SHADERSTAGE_BLEND_ADD;
			pass->rgbGen.type = RGBGEN_VERTEX;
			pass->alphaGen.type = ALPHAGEN_VERTEX;
			break;
		case mod_world:
		case mod_brush:
		case mod_studio:
			break;
		case mod_sprite:
			pass->glState = (GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE|GLSTATE_NO_DEPTH_TEST);
			pass->rgbGen.type = RGBGEN_VERTEX;
			pass->alphaGen.type = ALPHAGEN_VERTEX;
			break;
		}
		break;
	case kRenderTransAlpha:
		switch( mod_type )
		{
		case mod_bad:
			pass->glState = GLSTATE_AFUNC_GE128;
			pass->rgbGen.type = RGBGEN_VERTEX;
			pass->alphaGen.type = ALPHAGEN_VERTEX;
			break;
		case mod_world:
		case mod_brush:
		case mod_studio:
			break;
		case mod_sprite:
			pass->flags = SHADERSTAGE_BLEND_MODULATE;
			pass->glState = GLSTATE_AFUNC_GE128;
			pass->rgbGen.type = RGBGEN_VERTEX;
			pass->alphaGen.type = ALPHAGEN_VERTEX;
			break;
		}
		break;
	case kRenderTransAdd:
		switch( mod_type )
		{
		case mod_bad:
			pass->flags = SHADERSTAGE_BLEND_ADD;
			pass->glState = (GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE);
			pass->rgbGen.type = RGBGEN_VERTEX;
			pass->alphaGen.type = ALPHAGEN_VERTEX;
			break;
		case mod_world:
		case mod_brush:
			pass->flags = SHADERSTAGE_BLEND_ADD;
			pass->glState = (GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE);
			pass->rgbGen.type = RGBGEN_IDENTITY_LIGHTING;
			pass->alphaGen.type = ALPHAGEN_ENTITY;
			break;
		case mod_studio:
			pass->flags = SHADERSTAGE_BLEND_ADD;
			pass->glState = (GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE);
			pass->rgbGen.type = RGBGEN_IDENTITY_LIGHTING;	// models ignore color in 'add' mode
			pass->alphaGen.type = ALPHAGEN_ENTITY;
			break;
		case mod_sprite:
			pass->flags = SHADERSTAGE_BLEND_ADD;
			pass->glState = (GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE);
			pass->rgbGen.type = RGBGEN_VERTEX;
			pass->alphaGen.type = ALPHAGEN_VERTEX;
			break;
		}
		break;
	}

	// restore some flags
	pass->flags |= SHADERSTAGE_RENDERMODE;
	if( pass->prev.flags & SHADERSTAGE_ANIMFREQUENCY )
		pass->flags |= SHADERSTAGE_ANIMFREQUENCY;
	if( pass->prev.flags & SHADERSTAGE_FRAMES )
		pass->flags |= SHADERSTAGE_FRAMES;
	if( pass->prev.flags & SHADERSTAGE_ANGLEDMAP )
		pass->flags |= SHADERSTAGE_ANGLEDMAP;
}

/*
================
R_BindShaderpass
================
*/
static void R_BindShaderpass( const ref_stage_t *pass, texture_t *tex, int unit )
{
	matrix4x4	m1, m2, result;
	bool	identityMatrix;

	if( !tex ) tex = R_ShaderpassTex( pass, unit );

	GL_Bind( unit, tex );
	if( unit && !pass->program ) pglEnable( GL_TEXTURE_2D );
	GL_SetTexCoordArrayMode(( tex->flags & TF_CUBEMAP ? GL_TEXTURE_CUBE_MAP_ARB : GL_TEXTURE_COORD_ARRAY ));

	identityMatrix = R_VertexTCBase( pass, unit, result );

	if( pass->numtcMods )
	{
		identityMatrix = false;
		R_ApplyTCMods( pass, result );
	}

	if( pass->tcgen == TCGEN_REFLECTION || pass->tcgen == TCGEN_REFLECTION_CELLSHADE )
	{
		Matrix4x4_Transpose( m1, RI.modelviewMatrix );
		Matrix4x4_Copy( m2, result );
		Matrix4x4_Concat( result, m2, m1 );
		GL_LoadTexMatrix( result );
		return;
	}

	if( identityMatrix )
		GL_LoadIdentityTexMatrix();
	else GL_LoadTexMatrix( result );
}

/*
================
R_ModifyColor
================
*/
void R_ModifyColor( const ref_stage_t *pass )
{
	uint		i;
	float		a;
	int		c, bits;
	double		temp;
	vec3_t		t, v, style;
	byte		*bArray, *inArray, rgba[4] = { 255, 255, 255, 255 };
	bool		noArray, identityAlpha, entityAlpha;
	const waveFunc_t	*rgbgenfunc, *alphagenfunc;

	noArray = ( pass->flags & SHADERSTAGE_NOCOLORARRAY ) && !r_colorFog;
	r_backacc.numColors = noArray ? 1 : r_backacc.numVerts;
	bits = ( r_overbrightbits->integer > 0 ) && !( r_ignorehwgamma->integer ) ? r_overbrightbits->integer : 0;

	bArray = colorArray[0];
	inArray = inColorsArray[0][0];

	if( pass->rgbGen.type == RGBGEN_IDENTITY_LIGHTING )
	{
		entityAlpha = identityAlpha = false;
		Mem_Set( bArray, r_identityLighting, sizeof( rgba_t ) * r_backacc.numColors );
	}
	else if( pass->rgbGen.type == RGBGEN_EXACT_VERTEX )
	{
		entityAlpha = identityAlpha = false;
		Mem_Copy( bArray, inArray, sizeof( rgba_t ) * r_backacc.numColors );
	}
	else
	{
		entityAlpha = false;
		identityAlpha = true;
		Mem_Set( bArray, 255, sizeof( rgba_t ) * r_backacc.numColors );

		switch( pass->rgbGen.type )
		{
		case RGBGEN_IDENTITY:
			break;
		case RGBGEN_CONST:
			rgba[0] = R_FloatToByte( pass->rgbGen.args[0] );
			rgba[1] = R_FloatToByte( pass->rgbGen.args[1] );
			rgba[2] = R_FloatToByte( pass->rgbGen.args[2] );

			for( i = 0, c = *(int *)rgba; i < r_backacc.numColors; i++, bArray += 4 )
				*(int *)bArray = c;
			break;
		case RGBGEN_WAVE:
		case RGBGEN_COLORWAVE:
			rgbgenfunc = pass->rgbGen.func;
			if( rgbgenfunc->type == WAVEFORM_NOISE )
			{
				temp = R_BackendGetNoiseValue( 0, 0, 0, ( r_currentShaderTime + rgbgenfunc->args[2] ) * rgbgenfunc->args[3] );
			}
			else
			{
				temp = r_currentShaderTime * rgbgenfunc->args[3] + rgbgenfunc->args[2];
				temp = R_TableEvaluate( *rgbgenfunc, temp ) * rgbgenfunc->args[1] + rgbgenfunc->args[0];
			}

			temp = bound( 0.0f, temp, 1.0f );
			a = bound( 0.0f, pass->rgbGen.args[0] * temp, 1.0f );
			rgba[0] = R_FloatToByte( a );
			a = bound( 0.0f, pass->rgbGen.args[1] * temp, 1.0f );
			rgba[1] = R_FloatToByte( a );
			a = bound( 0.0f, pass->rgbGen.args[2] * temp, 1.0f );
			rgba[2] = R_FloatToByte( a );

			for( i = 0, c = *(int *)rgba; i < r_backacc.numColors; i++, bArray += 4 )
				*(int *)bArray = c;
			break;
		case RGBGEN_ENTITY:
			rgba[0] = RI.currententity->rendercolor[0];
			rgba[1] = RI.currententity->rendercolor[1];
			rgba[2] = RI.currententity->rendercolor[2];
			entityAlpha = true;
			identityAlpha = ( RI.currententity->renderamt == 255 );

			for( i = 0, c = *(int *)rgba; i < r_backacc.numColors; i++, bArray += 4 )
				*(int *)bArray = c;
			break;
		case RGBGEN_OUTLINE:
			identityAlpha = ( RI.currententity->outlineColor[3] == 255 );

			for( i = 0, c = *(int *)RI.currententity->outlineColor; i < r_backacc.numColors; i++, bArray += 4 )
				*(int *)bArray = c;
			break;
		case RGBGEN_ONE_MINUS_ENTITY:
			rgba[0] = 255 - RI.currententity->rendercolor[0];
			rgba[1] = 255 - RI.currententity->rendercolor[1];
			rgba[2] = 255 - RI.currententity->rendercolor[2];

			for( i = 0, c = *(int *)rgba; i < r_backacc.numColors; i++, bArray += 4 )
				*(int *)bArray = c;
			break;
		case RGBGEN_VERTEX:
			VectorSet( style, -1, -1, -1 );

			if( !r_superLightStyle || r_superLightStyle->vertexStyles[1] == 255 )
			{
				VectorSet( style, 1, 1, 1 );
				if( r_superLightStyle && r_superLightStyle->vertexStyles[0] != 255 )
					VectorCopy( r_lightStyles[r_superLightStyle->vertexStyles[0]].rgb, style );
			}

			if( style[0] == style[1] && style[1] == style[2] && style[2] == 1 )
			{
				for( i = 0; i < r_backacc.numColors; i++, bArray += 4, inArray += 4 )
				{
					bArray[0] = inArray[0] >> bits;
					bArray[1] = inArray[1] >> bits;
					bArray[2] = inArray[2] >> bits;
				}
			}
			else
			{
				int j;
				float *tc;
				vec3_t temp[MAX_ARRAY_VERTS];

				Mem_Set( temp, 0, sizeof( vec3_t ) * r_backacc.numColors );

				for( j = 0; j < LM_STYLES && r_superLightStyle->vertexStyles[j] != 255; j++ )
				{
					VectorCopy( r_lightStyles[r_superLightStyle->vertexStyles[j]].rgb, style );
					if( VectorCompare( style, vec3_origin ) )
						continue;

					inArray = inColorsArray[j][0];
					for( i = 0, tc = temp[0]; i < r_backacc.numColors; i++, tc += 3, inArray += 4 )
					{
						tc[0] += ( inArray[0] >> bits ) * style[0];
						tc[1] += ( inArray[1] >> bits ) * style[1];
						tc[2] += ( inArray[2] >> bits ) * style[2];
					}
				}

				for( i = 0, tc = temp[0]; i < r_backacc.numColors; i++, tc += 3, bArray += 4 )
				{
					bArray[0] = bound( 0, tc[0], 255 );
					bArray[1] = bound( 0, tc[1], 255 );
					bArray[2] = bound( 0, tc[2], 255 );
				}
			}
			break;
		case RGBGEN_ONE_MINUS_VERTEX:
			for( i = 0; i < r_backacc.numColors; i++, bArray += 4, inArray += 4 )
			{
				bArray[0] = 255 - ( inArray[0] >> bits );
				bArray[1] = 255 - ( inArray[1] >> bits );
				bArray[2] = 255 - ( inArray[2] >> bits );
			}
			break;
		case RGBGEN_LIGHTING_DIFFUSE:
			if( RI.currententity ) R_LightForEntity( RI.currententity, bArray );
			break;
		case RGBGEN_LIGHTING_DIFFUSE_ONLY:
			if( RI.currententity && !( RI.params & RP_SHADOWMAPVIEW ) )
			{
				vec4_t diffuse;
				vec3_t lightingOrigin;

				if( RI.currententity )
					VectorCopy( RI.currententity->lightingOrigin, lightingOrigin );
                                        else if( triState.fActive )
					VectorCopy( triState.lightingOrigin, lightingOrigin );
				else VectorClear( lightingOrigin );	// FIXME: MB_POLY

				if( RI.currententity->flags & EF_FULLBRIGHT || !r_worldbrushmodel->lightgrid )
					VectorSet( diffuse, 1, 1, 1 );
				else R_LightForOrigin( lightingOrigin, t, NULL, diffuse, RI.currentmodel->radius * RI.currententity->scale );

				rgba[0] = R_FloatToByte( diffuse[0] );
				rgba[1] = R_FloatToByte( diffuse[1] );
				rgba[2] = R_FloatToByte( diffuse[2] );

				for( i = 0, c = *(int *)rgba; i < r_backacc.numColors; i++, bArray += 4 )
					*(int *)bArray = c;
			}
			break;
		case RGBGEN_LIGHTING_AMBIENT_ONLY:
			if( RI.currententity && !( RI.params & RP_SHADOWMAPVIEW ))
			{
				vec4_t ambient;
				vec3_t lightingOrigin;

				if( RI.currententity )
					VectorCopy( RI.currententity->lightingOrigin, lightingOrigin );
                                        else if( triState.fActive )
					VectorCopy( triState.lightingOrigin, lightingOrigin );
				else VectorClear( lightingOrigin );	// FIXME: MB_POLY

				if( RI.currententity->flags & EF_FULLBRIGHT || !r_worldbrushmodel->lightgrid )
					VectorSet( ambient, 1.0f, 1.0f, 1.0f );
				else R_LightForOrigin( lightingOrigin, t, ambient, NULL, RI.currentmodel->radius * RI.currententity->scale );

				rgba[0] = R_FloatToByte( ambient[0] );
				rgba[1] = R_FloatToByte( ambient[1] );
				rgba[2] = R_FloatToByte( ambient[2] );

				for( i = 0, c = *(int *)rgba; i < r_backacc.numColors; i++, bArray += 4 )
					*(int *)bArray = c;
			}
			break;
		case RGBGEN_FOG:
			for( i = 0, c = *(int *)r_texFog->shader->fog_color; i < r_backacc.numColors; i++, bArray += 4 )
				*(int *)bArray = c;
			break;
		case RGBGEN_CUSTOM:
			c = (int)pass->rgbGen.args[0];
			for( i = 0, c = R_GetCustomColor( c ); i < r_backacc.numColors; i++, bArray += 4 )
				*(int *)bArray = c;
			break;
		case RGBGEN_ENVIRONMENT:
			for( i = 0, c = *(int *)mapConfig.environmentColor; i < r_backacc.numColors; i++, bArray += 4 )
				*(int *)bArray = c;
			break;
		default:
			break;
		}
	}

	bArray = colorArray[0];
	inArray = inColorsArray[0][0];

	switch( pass->alphaGen.type )
	{
	case ALPHAGEN_IDENTITY:
		if( identityAlpha ) break;
		for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
			bArray[3] = 255;
		break;
	case ALPHAGEN_CONST:
		c = R_FloatToByte( pass->alphaGen.args[0] );
		for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
			bArray[3] = c;
		break;
	case ALPHAGEN_WAVE:
	case ALPHAGEN_ALPHAWAVE:
		alphagenfunc = pass->alphaGen.func;
		if( alphagenfunc->type == WAVEFORM_NOISE )
		{
			a = R_BackendGetNoiseValue( 0, 0, 0, ( r_currentShaderTime + alphagenfunc->args[2] ) * alphagenfunc->args[3] );
		}
		else
		{
			a = alphagenfunc->args[2] + r_currentShaderTime * alphagenfunc->args[3];
			a = R_TableEvaluate( *alphagenfunc, a );
		}

		a = a * alphagenfunc->args[1] + alphagenfunc->args[0];
		c = a <= 0 ? 0 : R_FloatToByte( a );

		for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
			bArray[3] = c;
		break;
	case ALPHAGEN_PORTAL:
		VectorAdd( vertsArray[0], RI.currententity->origin, v );
		VectorSubtract( RI.viewOrigin, v, t );
		a = VectorLength( t ) * pass->alphaGen.args[0];
		a = bound( 0.0f, a, 1.0f );
		c = R_FloatToByte( a );

		for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
			bArray[3] = c;
		break;
	case ALPHAGEN_VERTEX:
		for( i = 0; i < r_backacc.numColors; i++, bArray += 4, inArray += 4 )
			bArray[3] = inArray[3];
		break;
	case ALPHAGEN_ONE_MINUS_VERTEX:
		for( i = 0; i < r_backacc.numColors; i++, bArray += 4, inArray += 4 )
			bArray[3] = 255 - inArray[3];
		break;
	case ALPHAGEN_ENTITY:
		if( entityAlpha ) break;
		for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
			bArray[3] = RI.currententity->renderamt;
		break;
	case  ALPHAGEN_ONE_MINUS_ENTITY:
		rgba[3] = 255 - RI.currententity->renderamt;
		for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
			bArray[3] = rgba[3];
		break;
	case ALPHAGEN_OUTLINE:
		for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
			bArray[3] = RI.currententity->outlineColor[3];
		break;
	case ALPHAGEN_SPECULAR:
		VectorSubtract( RI.viewOrigin, RI.currententity->origin, t );
		if( !Matrix3x3_Compare( RI.currententity->axis, matrix3x3_identity ))
			Matrix3x3_Transform( RI.currententity->axis, t, v );
		else VectorCopy( t, v );

		for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
		{
			VectorSubtract( v, vertsArray[i], t );
			c = VectorLength( t );
			a = DotProduct( t, normalsArray[i] ) / max( 0.1, c );
			a = pow( a, pass->alphaGen.args[0] );
			bArray[3] = a <= 0 ? 0 : R_FloatToByte( a );
		}
		break;
	case ALPHAGEN_DOT:
		if( !Matrix3x3_Compare( RI.currententity->axis, matrix3x3_identity ))
			Matrix3x3_Transform( RI.currententity->axis, RI.vpn, v );
		else VectorCopy( RI.vpn, v );

		for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
		{
			a = DotProduct( v, inNormalsArray[i] ); if( a < 0 ) a = -a;
			bArray[3] = R_FloatToByte( bound( pass->alphaGen.args[0], a, pass->alphaGen.args[1] ));
		}
		break;
	case ALPHAGEN_ONE_MINUS_DOT:
		if( !Matrix3x3_Compare( RI.currententity->axis, matrix3x3_identity ))
			Matrix3x3_Transform( RI.currententity->axis, RI.vpn, v );
		else VectorCopy( RI.vpn, v );

		for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
		{
			a = DotProduct( v, inNormalsArray[i] ); if( a < 0 ) a = -a; a = 1.0f - a;
			bArray[3] = R_FloatToByte( bound( pass->alphaGen.args[0], a, pass->alphaGen.args[1] ));
		}
		break;
	case ALPHAGEN_FADE:
		for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
		{
			VectorAdd( vertsArray[i], RI.currententity->origin, v );
			a = VectorDistance( v, RI.viewOrigin );

			a = bound( pass->alphaGen.args[0], a, pass->alphaGen.args[1] ) - pass->alphaGen.args[0];
			a = a * pass->alphaGen.args[2];
			bArray[3] = R_FloatToByte( bound( 0.0, a, 1.0 ));
		}
		break;
	case ALPHAGEN_ONE_MINUS_FADE:
		for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
		{
			VectorAdd( vertsArray[i], RI.currententity->origin, v );
			a = VectorDistance( v, RI.viewOrigin );

			a = bound( pass->alphaGen.args[0], a, pass->alphaGen.args[1] ) - pass->alphaGen.args[0];
			a = a * pass->alphaGen.args[2];
			bArray[3] = R_FloatToByte( bound( 0.0, 1.0 - a, 1.0 ));
		}
		break;
	default:
		break;
	}

	if( r_colorFog )
	{
		float dist, vdist;
		cplane_t *fogPlane;
		vec3_t viewtofog;
		float fogNormal[3], vpnNormal[3];
		float fogDist, vpnDist, fogShaderDistScale;
		int fogptype;
		bool alphaFog;
		int blendsrc, blenddst;

		blendsrc = pass->glState & GLSTATE_SRCBLEND_MASK;
		blenddst = pass->glState & GLSTATE_DSTBLEND_MASK;
		if(( blendsrc != GLSTATE_SRCBLEND_SRC_ALPHA && blenddst != GLSTATE_DSTBLEND_SRC_ALPHA ) && ( blendsrc != GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA && blenddst != GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA ))
			alphaFog = false;
		else alphaFog = true;

		fogPlane = r_colorFog->visibleplane;
		fogShaderDistScale = 1.0 / (r_colorFog->shader->fog_dist - r_colorFog->shader->fog_clearDist);
		dist = RI.fog_dist_to_eye[r_colorFog-r_worldbrushmodel->fogs];

		if( r_currentShader->flags & SHADER_SKYPARMS )
		{
			if( dist > 0 )
				VectorScale( fogPlane->normal, -dist, viewtofog );
			else
				VectorClear( viewtofog );
		}
		else
		{
			VectorCopy( RI.currententity->origin, viewtofog );
		}

		vpnNormal[0] = DotProduct( RI.currententity->axis[0], RI.vpn ) * fogShaderDistScale * RI.currententity->scale;
		vpnNormal[1] = DotProduct( RI.currententity->axis[1], RI.vpn ) * fogShaderDistScale * RI.currententity->scale;
		vpnNormal[2] = DotProduct( RI.currententity->axis[2], RI.vpn ) * fogShaderDistScale * RI.currententity->scale;
		vpnDist = (( ( RI.viewOrigin[0] - viewtofog[0] ) * RI.vpn[0] + ( RI.viewOrigin[1] - viewtofog[1] ) * RI.vpn[1] + ( RI.viewOrigin[2] - viewtofog[2] ) * RI.vpn[2] )
			+ r_colorFog->shader->fog_clearDist) * fogShaderDistScale;

		bArray = colorArray[0];
		if( dist < 0 )
		{	// camera is inside the fog
			for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
			{
				temp = DotProduct( vertsArray[i], vpnNormal ) - vpnDist;
				c = ( 1.0f - bound( 0, temp, 1.0f ) ) * 0xFFFF;

				if( alphaFog )
				{
					bArray[3] = ( bArray[3] * c ) >> 16;
				}
				else
				{
					bArray[0] = ( bArray[0] * c ) >> 16;
					bArray[1] = ( bArray[1] * c ) >> 16;
					bArray[2] = ( bArray[2] * c ) >> 16;
				}
			}
		}
		else
		{
			fogNormal[0] = DotProduct( RI.currententity->axis[0], fogPlane->normal ) * RI.currententity->scale;
			fogNormal[1] = DotProduct( RI.currententity->axis[1], fogPlane->normal ) * RI.currententity->scale;
			fogNormal[2] = DotProduct( RI.currententity->axis[2], fogPlane->normal ) * RI.currententity->scale;
			fogptype = ( fogNormal[0] == 1.0 ? PLANE_X : ( fogNormal[1] == 1.0 ? PLANE_Y : ( fogNormal[2] == 1.0 ? PLANE_Z : PLANE_NONAXIAL ) ) );
			fogDist = fogPlane->dist - DotProduct( viewtofog, fogPlane->normal );

			for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
			{
				if( fogptype < 3 )
					vdist = vertsArray[i][fogptype] - fogDist;
				else
					vdist = DotProduct( vertsArray[i], fogNormal ) - fogDist;

				if( vdist < 0 )
				{
					temp = ( DotProduct( vertsArray[i], vpnNormal ) - vpnDist ) * vdist / ( vdist - dist );
					c = ( 1.0f - bound( 0, temp, 1.0f ) ) * 0xFFFF;

					if( alphaFog )
					{
						bArray[3] = ( bArray[3] * c ) >> 16;
					}
					else
					{
						bArray[0] = ( bArray[0] * c ) >> 16;
						bArray[1] = ( bArray[1] * c ) >> 16;
						bArray[2] = ( bArray[2] * c ) >> 16;
					}
				}
			}
		}
	}
}

/*
================
R_ShaderpassBlendmode
================
*/
static int R_ShaderpassBlendmode( int passFlags )
{
	if( passFlags & SHADERSTAGE_BLEND_REPLACE )
		return GL_REPLACE;
	if( passFlags & SHADERSTAGE_BLEND_MODULATE )
		return GL_MODULATE;
	if( passFlags & SHADERSTAGE_BLEND_ADD )
		return GL_ADD;
	if( passFlags & SHADERSTAGE_BLEND_DECAL )
		return GL_DECAL;
	return 0;
}

/*
================
R_SetShaderState
================
*/
static void R_SetShaderState( void )
{
	int	state;

	// Face culling
	if( !gl_cull->integer || ( r_features & MF_NOCULL ))
		GL_Cull( 0 );
	else if( r_currentShader->flags & SHADER_CULL_FRONT )
		GL_Cull( GL_FRONT );
	else if( r_currentShader->flags & SHADER_CULL_BACK )
		GL_Cull( GL_BACK );
	else GL_Cull( 0 );

	state = 0;
	if( r_currentShader->flags & SHADER_POLYGONOFFSET || RI.params & RP_SHADOWMAPVIEW )
		state |= GLSTATE_OFFSET_FILL;
	if( r_currentShader->type == SHADER_FLARE )
		state |= GLSTATE_NO_DEPTH_TEST;
	r_currentShaderState = state;
}

/*
================
R_RenderMeshGeneric
================
*/
void R_RenderMeshGeneric( void )
{
	const ref_stage_t *pass = r_accumPasses[0];

	R_BindShaderpass( pass, NULL, 0 );
	R_ModifyColor( pass );

	if( pass->flags & SHADERSTAGE_BLEND_REPLACE )
		GL_TexEnv( GL_REPLACE );
	else
		GL_TexEnv( GL_MODULATE );
	GL_SetState( r_currentShaderState | ( pass->glState & r_currentShaderPassMask ));

	R_FlushArrays();
}

/*
================
R_RenderMeshMultitextured
================
*/
void R_RenderMeshMultitextured( void )
{
	int		i;
	const ref_stage_t	*pass = r_accumPasses[0];

	R_BindShaderpass( pass, NULL, 0 );
	R_ModifyColor( pass );

	GL_TexEnv( GL_MODULATE );
	GL_SetState( r_currentShaderState | ( pass->glState & r_currentShaderPassMask ) | GLSTATE_BLEND_MTEX );

	for( i = 1; i < r_numAccumPasses; i++ )
	{
		pass = r_accumPasses[i];
		R_BindShaderpass( pass, NULL, i );
		GL_TexEnv( R_ShaderpassBlendmode( pass->flags ) );
	}

	R_FlushArrays();
}

/*
================
R_RenderMeshCombined
================
*/
void R_RenderMeshCombined( void )
{
	int i;
	const ref_stage_t *pass = r_accumPasses[0];

	R_BindShaderpass( pass, NULL, 0 );
	R_ModifyColor( pass );

	GL_TexEnv( GL_MODULATE );
	GL_SetState( r_currentShaderState | ( pass->glState & r_currentShaderPassMask ) | GLSTATE_BLEND_MTEX );

	for( i = 1; i < r_numAccumPasses; i++ )
	{
		pass = r_accumPasses[i];
		R_BindShaderpass( pass, NULL, i );

		if( pass->flags & ( SHADERSTAGE_BLEND_REPLACE|SHADERSTAGE_BLEND_MODULATE ))
		{
			GL_TexEnv( GL_MODULATE );
		}
		else if( pass->flags & SHADERSTAGE_BLEND_ADD )
		{
			// these modes are best set with TexEnv, Combine4 would need much more setup
			GL_TexEnv( GL_ADD );
		}
		else if( pass->flags & SHADERSTAGE_BLEND_DECAL )
		{
			// mimics Alpha-Blending in upper texture stage, but instead of multiplying the alpha-channel, they're added
			// this way it can be possible to use GL_DECAL in both texture-units, while still looking good
			// normal mutlitexturing would multiply the alpha-channel which looks ugly
			GL_TexEnv( GL_COMBINE_ARB );
			pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_INTERPOLATE_ARB );
			pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_ADD );

			pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE );
			pglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR );
			pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE );
			pglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA );

			pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB );
			pglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR );
			pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB );
			pglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA );

			pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_TEXTURE );
			pglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_ALPHA );
		}
	}

	R_FlushArrays();
}

/*
================
R_RenderMeshGLSL_Material
================
*/
static void R_RenderMeshGLSL_Material( void )
{
	int		i, tcgen;
	int		state;
	bool		breakIntoPasses = false;
	int		program, object;
	int		programFeatures = 0;
	texture_t		*base, *normalmap, *glossmap, *decalmap;
	matrix4x4		unused;
	vec3_t		lightDir = { 0.0f, 0.0f, 0.0f };
	vec4_t		ambient = { 0.0f, 0.0f, 0.0f, 0.0f }, diffuse = { 0.0f, 0.0f, 0.0f, 0.0f };
	float		offsetmappingScale;
	superLightStyle_t	*lightStyle;
	ref_stage_t	*pass = r_accumPasses[0];

	// handy pointers
	base = pass->textures[0];
	normalmap = pass->textures[1];
	glossmap = pass->textures[2];
	decalmap = pass->textures[3];

	Com_Assert( normalmap == NULL );

	if( normalmap->samples == 4 )
		offsetmappingScale = r_offsetmapping_scale->value * r_currentShader->offsetmapping_scale;
	else	// no alpha in normalmap, don't bother with offset mapping
		offsetmappingScale = 0;

	if( GL_Support( R_GLSL_BRANCHING ))
		programFeatures |= PROGRAM_APPLY_BRANCHING;
	if( GL_Support( R_GLSL_NO_HALF_TYPES ))
		programFeatures |= PROGRAM_APPLY_NO_HALF_TYPES;
	if( RI.params & RP_CLIPPLANE )
		programFeatures |= PROGRAM_APPLY_CLIPPING;

	if( r_currentMeshBuffer->infokey > 0 )
	{	
		// world surface
		int	srcAlpha = (pass->flags & SHADERSTAGE_BLEND_DECAL);

		// CHECKTHIS: this is right ?
		srcAlpha |= (pass->glState & (GLSTATE_ALPHAFUNC|GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA|GLSTATE_DSTBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA));

		if( !( r_offsetmapping->integer & 1 ) )
			offsetmappingScale = 0;

		if( r_lightmap->integer || ( r_currentDlightBits && !pass->textures[5] ) )
		{
			if( !srcAlpha )
				base = tr.whiteTexture;		// white
			else
				programFeatures |= PROGRAM_APPLY_BASETEX_ALPHA_ONLY;
		}

		// we use multipass for dynamic lights, so bind the white texture
		// instead of base in GLSL program and add another modulative pass (diffusemap)
		if( !r_lightmap->integer && ( r_currentDlightBits && !pass->textures[5] ) )
		{
			breakIntoPasses = true;
			r_GLSLpasses[1] = *pass;
			r_GLSLpasses[1].flags = ( pass->flags & SHADERSTAGE_NOCOLORARRAY )|SHADERSTAGE_BLEND_MODULATE;
			r_GLSLpasses[1].glState = GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_SRC_COLOR|((pass->glState & GLSTATE_ALPHAFUNC) ? GLSTATE_DEPTHFUNC_EQ : 0);

			// decal
			if( decalmap )
			{
				r_GLSLpasses[1].rgbGen.type = RGBGEN_IDENTITY;
				r_GLSLpasses[1].alphaGen.type = ALPHAGEN_IDENTITY;

				r_GLSLpasses[2] = *pass;
				r_GLSLpasses[2].flags = ( pass->flags & SHADERSTAGE_NOCOLORARRAY )|SHADERSTAGE_BLEND_DECAL;
				r_GLSLpasses[2].glState = GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA|((pass->glState & GLSTATE_ALPHAFUNC) ? GLSTATE_DEPTHFUNC_EQ : 0);
				r_GLSLpasses[2].textures[0] = decalmap;
			}

			if( offsetmappingScale <= 0 )
			{
				r_GLSLpasses[1].program = r_GLSLpasses[2].program = NULL;
				r_GLSLpasses[1].program_type = r_GLSLpasses[2].program_type = PROGRAM_TYPE_NONE;
			}
			else
			{
				r_GLSLpasses[1].textures[2] = r_GLSLpasses[2].textures[2] = NULL; // no specular
				r_GLSLpasses[1].textures[3] = r_GLSLpasses[2].textures[3] = NULL; // no decal
				r_GLSLpasses[1].textures[6] = r_GLSLpasses[6].textures[2] = ((texture_t *)1); // HACKHACK no ambient
			}
		}
	}
	else if( ( r_currentMeshBuffer->sortkey & 3 ) == MB_POLY )
	{
		// polys
		if( !( r_offsetmapping->integer & 2 ))
			offsetmappingScale = 0;

		R_BuildTangentVectors( r_backacc.numVerts, vertsArray, normalsArray, coordsArray, r_backacc.numElems/3, elemsArray, inSVectorsArray );
	}
	else
	{	// models
		if( !( r_offsetmapping->integer & 4 ) )
			offsetmappingScale = 0;
	}

	tcgen = pass->tcgen;                // store the original tcgen

	pass->tcgen = TCGEN_BASE;
	R_BindShaderpass( pass, base, 0 );
	if( !breakIntoPasses )
	{	
		// calculate the fragment color
		R_ModifyColor( pass );
	}
	else
	{	// rgbgen identity (255,255,255,255)
		r_backacc.numColors = 1;
		colorArray[0][0] = colorArray[0][1] = colorArray[0][2] = colorArray[0][3] = 255;
	}

	// set shaderpass state (blending, depthwrite, etc)
	state = r_currentShaderState | ( pass->glState & r_currentShaderPassMask ) | GLSTATE_BLEND_MTEX;
	GL_SetState( state );

	// don't waste time on processing GLSL programs with zero colormask
	if( RI.params & RP_SHADOWMAPVIEW )
	{
		pass->tcgen = tcgen; // restore original tcgen
		R_FlushArrays();
		return;
	}

	// we only send S-vectors to GPU and recalc T-vectors as cross product
	// in vertex shader
	pass->tcgen = TCGEN_SVECTORS;
	GL_Bind( 1, normalmap );         // normalmap
	GL_SetTexCoordArrayMode( GL_TEXTURE_COORD_ARRAY );
	R_VertexTCBase( pass, 1, unused );

	if( glossmap && r_lighting_glossintensity->value )
	{
		programFeatures |= PROGRAM_APPLY_SPECULAR;
		GL_Bind( 2, glossmap ); // gloss
		GL_SetTexCoordArrayMode( 0 );
	}

	if( decalmap && !breakIntoPasses )
	{
		programFeatures |= PROGRAM_APPLY_DECAL;
		GL_Bind( 3, decalmap ); // decal
		GL_SetTexCoordArrayMode( 0 );
	}

	if( offsetmappingScale > 0 )
		programFeatures |= r_offsetmapping_reliefmapping->integer ? PROGRAM_APPLY_RELIEFMAPPING : PROGRAM_APPLY_OFFSETMAPPING;

	if( r_currentMeshBuffer->infokey > 0 )
	{                                           // world surface
		lightStyle = r_superLightStyle;

		// bind lightmap textures and set program's features for lightstyles
		if( r_superLightStyle && r_superLightStyle->lightmapNum[0] >= 0 )
		{
			pass->tcgen = TCGEN_LIGHTMAP;

			for( i = 0; i < LM_STYLES && r_superLightStyle->lightmapStyles[i] != 255; i++ )
			{
				programFeatures |= ( PROGRAM_APPLY_LIGHTSTYLE0 << i );

				r_lightmapStyleNum[i+4] = i;
				GL_Bind( i+4, tr.lightmapTextures[r_superLightStyle->lightmapNum[i]] ); // lightmap
				GL_SetTexCoordArrayMode( GL_TEXTURE_COORD_ARRAY );
				R_VertexTCBase( pass, i+4, unused );
			}

			if( i == 1 )
			{
				vec_t *rgb = r_lightStyles[r_superLightStyle->lightmapStyles[0]].rgb;

				// PROGRAM_APPLY_FB_LIGHTMAP indicates that there's no need to renormalize
				// the lighting vector for specular (saves 3 adds, 3 muls and 1 normalize per pixel)
				if( rgb[0] == 1 && rgb[1] == 1 && rgb[2] == 1 )
					programFeatures |= PROGRAM_APPLY_FB_LIGHTMAP;
			} 
		}

		if( !pass->textures[6] && !VectorCompare( mapConfig.ambient, vec3_origin ) )
		{
			VectorCopy( mapConfig.ambient, ambient );
			programFeatures |= PROGRAM_APPLY_AMBIENT_COMPENSATION;
		}
	}
	else
	{
		vec3_t temp;

		lightStyle = NULL;
		programFeatures |= PROGRAM_APPLY_DIRECTIONAL_LIGHT;

		if( ( r_currentMeshBuffer->sortkey & 3 ) == MB_POLY )
		{
			VectorCopy( r_polys[-r_currentMeshBuffer->infokey-1].normal, lightDir );
			Vector4Set( ambient, 0, 0, 0, 0 );
			Vector4Set( diffuse, 1, 1, 1, 1 );
		}
		else if( RI.currententity )
		{
			if( RI.currententity->flags & EF_FULLBRIGHT )
			{
				Vector4Set( ambient, 1, 1, 1, 1 );
				Vector4Set( diffuse, 1, 1, 1, 1 );
			}
			else
			{
				// get weighted incoming direction of world and dynamic lights
				R_LightForOrigin( RI.currententity->lightingOrigin, temp, ambient, diffuse,
					RI.currententity->model ? RI.currententity->model->radius * RI.currententity->scale : 0 );

				if( RI.currententity->flags & EF_MINLIGHT )
				{
					if( ambient[0] <= 0.1f || ambient[1] <= 0.1f || ambient[2] <= 0.1f )
						VectorSet( ambient, 0.1f, 0.1f, 0.1f );
				}

				// rotate direction
				Matrix3x3_Transform( RI.currententity->axis, temp, lightDir );
			}
		}
	}

	pass->tcgen = tcgen;    // restore original tcgen

	program = R_RegisterGLSLProgram( pass->program, NULL, programFeatures );
	object = R_GetProgramObject( program );
	if( object )
	{
		pglUseProgramObjectARB( object );

		// update uniforms
		R_UpdateProgramUniforms( program, RI.viewOrigin, vec3_origin, lightDir, ambient, diffuse, lightStyle, 
			true, 0, 0, 0, offsetmappingScale );

		R_FlushArrays();

		pglUseProgramObjectARB( 0 );
	}

	if( breakIntoPasses )
	{
		unsigned int oDB = r_currentDlightBits;		// HACK HACK HACK
		superLightStyle_t *oSL = r_superLightStyle;

		R_AccumulatePass( &r_GLSLpasses[0] );		// dynamic lighting pass

		if( offsetmappingScale )
		{
			r_superLightStyle = NULL;
			r_currentDlightBits = 0;
		}

		R_AccumulatePass( &r_GLSLpasses[1] );		// modulate (diffusemap)

		if( decalmap )
			R_AccumulatePass( &r_GLSLpasses[2] );	// alpha-blended decal texture

		if( offsetmappingScale )
		{
			r_superLightStyle = oSL;
			r_currentDlightBits = oDB;
		}
	}
}

/*
================
R_RenderMeshGLSL_Distortion
================
*/
static void R_RenderMeshGLSL_Distortion( void )
{
	int		state, tcgen;
	int		program, object;
	int		programFeatures = 0;
	matrix4x4		unused;
	ref_stage_t	*pass = r_accumPasses[0];
	texture_t		*portaltexture1, *portaltexture2;
	bool		frontPlane;

	if( !( RI.params & ( RP_PORTALCAPTURED|RP_PORTALCAPTURED2 )))
		return;

	if( GL_Support( R_GLSL_BRANCHING ))
		programFeatures |= PROGRAM_APPLY_BRANCHING;
	if( GL_Support( R_GLSL_NO_HALF_TYPES ))
		programFeatures |= PROGRAM_APPLY_NO_HALF_TYPES;
	if( RI.params & RP_CLIPPLANE )
		programFeatures |= PROGRAM_APPLY_CLIPPING;

	portaltexture1 = ( RI.params & RP_PORTALCAPTURED ) ? tr.portaltexture1 : tr.blackTexture;
	portaltexture2 = ( RI.params & RP_PORTALCAPTURED2 ) ? tr.portaltexture2 : tr.blackTexture;

	frontPlane = (PlaneDiff( RI.viewOrigin, &RI.portalPlane ) > 0 ? true : false);

	tcgen = pass->tcgen;                // store the original tcgen

	R_BindShaderpass( pass, pass->textures[0], 0 );  // dudvmap

	// calculate the fragment color
	R_ModifyColor( pass );

	if( frontPlane )
	{
		if( pass->alphaGen.type != ALPHAGEN_IDENTITY )
			programFeatures |= PROGRAM_APPLY_DISTORTION_ALPHA;
	}

	// set shaderpass state (blending, depthwrite, etc)
	state = r_currentShaderState | ( pass->glState & r_currentShaderPassMask ) | GLSTATE_BLEND_MTEX;
	GL_SetState( state );

	if( pass->textures[1] /* && ( RI.params & RP_PORTALCAPTURED )*/ )
	{	
		// eyeDot
		programFeatures |= PROGRAM_APPLY_EYEDOT;

		pass->tcgen = TCGEN_SVECTORS;
		GL_Bind( 1, pass->textures[1] ); // normalmap
		GL_SetTexCoordArrayMode( GL_TEXTURE_COORD_ARRAY );
		R_VertexTCBase( pass, 1, unused );
	}

	GL_Bind( 2, portaltexture1 );           // reflection
	GL_Bind( 3, portaltexture2 );           // refraction

	pass->tcgen = tcgen;    // restore original tcgen

	// update uniforms
	program = R_RegisterGLSLProgram( pass->program, NULL, programFeatures );
	object = R_GetProgramObject( program );
	if( object )
	{
		pglUseProgramObjectARB( object );

		R_UpdateProgramUniforms( program, RI.viewOrigin, vec3_origin, vec3_origin, NULL, NULL, NULL,
			frontPlane, tr.portaltexture1->width, tr.portaltexture1->height, 0, 0 );

		R_FlushArrays();

		pglUseProgramObjectARB( 0 );
	}
}

/*
================
R_RenderMeshGLSL_Shadowmap
================
*/
static void R_RenderMeshGLSL_Shadowmap( void )
{
	int i;
	int state;
	int program, object;
	int programFeatures = GL_Support( R_GLSL_BRANCHING ) ? PROGRAM_APPLY_BRANCHING : 0;
	ref_stage_t *pass = r_accumPasses[0];

	if( r_shadows_pcf->integer == 2 )
		programFeatures |= PROGRAM_APPLY_PCF2x2;
	else if( r_shadows_pcf->integer == 3 )
		programFeatures |= PROGRAM_APPLY_PCF3x3;

	// update uniforms
	program = R_RegisterGLSLProgram( pass->program, NULL, programFeatures );
	object = R_GetProgramObject( program );
	if( !object )
		return;

	for( i = 0, r_currentCastGroup = r_shadowGroups; i < r_numShadowGroups; i++, r_currentCastGroup++ )
	{
		if( !( r_currentShadowBits & r_currentCastGroup->bit ) )
			continue;

		R_BindShaderpass( pass, r_currentCastGroup->depthTexture, 0 );

		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB );
		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL );

		// calculate the fragment color
		R_ModifyColor( pass );

		// set shaderpass state (blending, depthwrite, etc)
		state = r_currentShaderState | ( pass->glState & r_currentShaderPassMask ) | GLSTATE_BLEND_MTEX;
		GL_SetState( state );

		pglUseProgramObjectARB( object );

		R_UpdateProgramUniforms( program, RI.viewOrigin, vec3_origin, vec3_origin, NULL, NULL, NULL, true,
			r_currentCastGroup->depthTexture->width, r_currentCastGroup->depthTexture->height, 
			r_currentCastGroup->projDist, 0 );

		R_FlushArrays();

		pglUseProgramObjectARB( 0 );

		pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_NONE );
	}
}

/*
================
R_RenderMeshGLSL_Outline
================
*/
static void R_RenderMeshGLSL_Outline( void )
{
	int faceCull;
	int state;
	int program, object;
	int programFeatures = GL_Support( R_GLSL_BRANCHING ) ? PROGRAM_APPLY_BRANCHING : 0;
	ref_stage_t *pass = r_accumPasses[0];

	if( RI.params & RP_CLIPPLANE )
		programFeatures |= PROGRAM_APPLY_CLIPPING;

	// update uniforms
	program = R_RegisterGLSLProgram( pass->program, NULL, programFeatures );
	object = R_GetProgramObject( program );
	if( !object )
		return;

	faceCull = glState.faceCull;
	GL_Cull( GL_BACK );

	GL_SelectTexture( 0 );
	GL_SetTexCoordArrayMode( 0 );

	// calculate the fragment color
	R_ModifyColor( pass );

	// set shaderpass state (blending, depthwrite, etc)
	state = r_currentShaderState | ( pass->glState & r_currentShaderPassMask ) | GLSTATE_BLEND_MTEX;
	GL_SetState( state );

	pglUseProgramObjectARB( object );

	R_UpdateProgramUniforms( program, RI.viewOrigin, vec3_origin, vec3_origin, NULL, NULL, NULL, true,
		0, 0, RI.currententity->outlineHeight * r_outlines_scale->value, 0 );

	R_FlushArrays();

	pglUseProgramObjectARB( 0 );

	GL_Cull( faceCull );
}

/*
================
R_RenderMeshGLSLProgrammed
================
*/
static void R_RenderMeshGLSLProgrammed( void )
{
	const ref_stage_t *pass = ( ref_stage_t * )r_accumPasses[0];

	switch( pass->program_type )
	{
	case PROGRAM_TYPE_MATERIAL:
		R_RenderMeshGLSL_Material();
		break;
	case PROGRAM_TYPE_DISTORTION:
		R_RenderMeshGLSL_Distortion();
		break;
	case PROGRAM_TYPE_SHADOWMAP:
		R_RenderMeshGLSL_Shadowmap();
		break;
	case PROGRAM_TYPE_OUTLINE:
		R_RenderMeshGLSL_Outline ();
		break;
	default:
		MsgDev( D_WARN, "Unknown GLSL program type %i\n", pass->program_type );
		break;
	}
}

/*
================
R_RenderAccumulatedPasses
================
*/
static void R_RenderAccumulatedPasses( void )
{
	const ref_stage_t *pass = r_accumPasses[0];

	R_CleanUpTextureUnits( r_numAccumPasses );

	if( pass->program )
	{
		r_numAccumPasses = 0;
		R_RenderMeshGLSLProgrammed();
		return;
	}
	if( pass->flags & SHADERSTAGE_DLIGHT )
	{
		r_numAccumPasses = 0;
		R_AddDynamicLights( r_currentDlightBits, r_currentShaderState | ( pass->glState & r_currentShaderPassMask ));
		return;
	}
	if( pass->flags & SHADERSTAGE_STENCILSHADOW )
	{
		r_numAccumPasses = 0;
		R_PlanarShadowPass( r_currentShaderState | ( pass->glState & r_currentShaderPassMask ));
		return;
	}

	if( r_numAccumPasses == 1 )
		R_RenderMeshGeneric();
	else if( GL_Support( R_ENV_COMBINE_EXT ))
		R_RenderMeshCombined();
	else
		R_RenderMeshMultitextured();

	r_numAccumPasses = 0;
}

/*
================
R_AccumulatePass
================
*/
static void R_AccumulatePass( ref_stage_t *pass )
{
	bool accumulate, renderNow;
	const ref_stage_t *prevPass;

	// for depth texture we render light's view to, ignore passes that do not write into depth buffer
	if( ( RI.params & RP_SHADOWMAPVIEW ) && !( pass->glState & GLSTATE_DEPTHWRITE ))
		return;

	R_ShaderpassRenderMode( pass );

	// see if there are any free texture units
	renderNow = ( pass->flags & ( SHADERSTAGE_DLIGHT|SHADERSTAGE_STENCILSHADOW ) ) || pass->program;
	accumulate = ( r_numAccumPasses < glConfig.max_texture_units ) && !renderNow;

	if( accumulate )
	{
		if( !r_numAccumPasses )
		{
			r_accumPasses[r_numAccumPasses++] = pass;
			return;
		}

		// ok, we've got several passes, diff against the previous
		prevPass = r_accumPasses[r_numAccumPasses-1];

		// see if depthfuncs and colors are good
		if(
			(( prevPass->glState ^ pass->glState ) & GLSTATE_DEPTHFUNC_EQ ) ||
			( pass->glState & GLSTATE_ALPHAFUNC ) ||
			( pass->rgbGen.type != RGBGEN_IDENTITY ) ||
			( pass->alphaGen.type != ALPHAGEN_IDENTITY ) ||
			( ( prevPass->glState & GLSTATE_ALPHAFUNC ) && !( pass->glState & GLSTATE_DEPTHFUNC_EQ ))
			)
			accumulate = false;

		// see if blendmodes are good
		if( accumulate )
		{
			int mode, prevMode;

			mode = R_ShaderpassBlendmode( pass->flags );
			if( mode )
			{
				prevMode = R_ShaderpassBlendmode( prevPass->flags );

				if( GL_Support( R_ENV_COMBINE_EXT ))
				{
					if( prevMode == GL_REPLACE )
						accumulate = ( mode == GL_ADD ) ? GL_Support( R_TEXTURE_ENV_ADD_EXT ) : true;
					else if( prevMode == GL_ADD )
						accumulate = ( mode == GL_ADD ) && GL_Support( R_TEXTURE_ENV_ADD_EXT );
					else if( prevMode == GL_MODULATE )
						accumulate = ( mode == GL_MODULATE || mode == GL_REPLACE );
					else
						accumulate = false;
				}
				else /* if( GL_Support( R_ARB_MULTITEXTURE ))*/
				{
					if( prevMode == GL_REPLACE )
						accumulate = ( mode == GL_ADD ) ? GL_Support( R_TEXTURE_ENV_ADD_EXT ) : ( mode != GL_DECAL );
					else if( prevMode == GL_ADD )
						accumulate = ( mode == GL_ADD ) && GL_Support( R_TEXTURE_ENV_ADD_EXT );
					else if( prevMode == GL_MODULATE )
						accumulate = ( mode == GL_MODULATE || mode == GL_REPLACE );
					else
						accumulate = false;
				}
			}
			else
			{
				accumulate = false;
			}
		}
	}

	// no, failed to accumulate
	if( !accumulate )
	{
		if( r_numAccumPasses )
			R_RenderAccumulatedPasses();
	}

	r_accumPasses[r_numAccumPasses++] = pass;
	if( renderNow )
		R_RenderAccumulatedPasses();
}

/*
================
R_SetupLightmapMode
================
*/
void R_SetupLightmapMode( void )
{
	r_lightmapPasses[0].tcgen = TCGEN_LIGHTMAP;
	r_lightmapPasses[0].rgbGen.type = RGBGEN_IDENTITY;
	r_lightmapPasses[0].alphaGen.type = ALPHAGEN_IDENTITY;
	r_lightmapPasses[0].flags &= ~SHADERSTAGE_BLENDMODE;
	r_lightmapPasses[0].glState &= ~( GLSTATE_ALPHAFUNC|GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK|GLSTATE_DEPTHFUNC_EQ );
	r_lightmapPasses[0].flags |= SHADERSTAGE_LIGHTMAP|SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_MODULATE;
//	r_lightmapPasses[0].glState |= GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ZERO;
	if( r_lightmap->integer ) r_lightmapPasses[0].glState |= GLSTATE_DEPTHWRITE;
}

/*
================
R_RenderMeshBuffer
================
*/
void R_RenderMeshBuffer( const meshbuffer_t *mb )
{
	int i;
	msurface_t *surf;
	ref_stage_t *pass;
	mfog_t *fog;

	if( !r_backacc.numVerts || !r_backacc.numElems )
	{
		R_ClearArrays();
		return;
	}

	Com_Assert( mb == NULL );

	surf = mb->infokey > 0 ? &r_worldbrushmodel->surfaces[mb->infokey-1] : NULL;
	if( surf ) r_superLightStyle = &r_superLightStyles[surf->superLightStyle];
	else r_superLightStyle = NULL;
	r_currentMeshBuffer = mb;

	MB_NUM2SHADER( mb->shaderkey, r_currentShader );

	if( glState.in2DMode ) r_currentShaderTime = Sys_DoubleTime();
	else r_currentShaderTime = (double)RI.refdef.time;

	if( !r_triangleOutlines )
		R_SetShaderState();

	if( r_currentShader->numDeforms )
		R_DeformVertices();

	if( r_features & MF_KEEPLOCK )
		r_backacc.c_totalKeptLocks++;
	else
		R_UnlockArrays();

	if( r_triangleOutlines )
	{
		R_LockArrays( r_backacc.numVerts );

		if( RI.params & RP_TRISOUTLINES )
			R_DrawTriangles();
		if( RI.params & RP_SHOWNORMALS )
			R_DrawNormals();

		R_ClearArrays();
		return;
	}

	// extract the fog volume number from sortkey
	if( !r_worldmodel ) fog = NULL;
	else MB_NUM2FOG( mb->sortkey, fog );

	if( fog && !fog->shader ) fog = NULL;

	// can we fog the geometry with alpha texture?
	r_texFog = ( fog && ( ( r_currentShader->sort <= SORT_ALPHATEST &&
		( r_currentShader->flags & (SHADER_DEPTHWRITE|SHADER_SKYPARMS))) || r_currentShader->fog_dist ) ) ? fog : NULL;

	// check if the fog volume is present but we can't use alpha texture
	r_colorFog = ( fog && !r_texFog ) ? fog : NULL;

	if( r_currentShader->type == SHADER_FLARE )
		r_currentDlightBits = 0;
	else
		r_currentDlightBits = surf ? mb->dlightbits : 0;

	r_currentShadowBits = mb->shadowbits & RI.shadowBits;

	R_LockArrays( r_backacc.numVerts );

	// accumulate passes for dynamic merging
	for( i = 0, pass = r_currentShader->stages; i < r_currentShader->num_stages; i++, pass++ )
	{
		if( !pass->program )
		{
			if( pass->flags & SHADERSTAGE_LIGHTMAP )
			{
				int j, k, l, u;

				// no valid lightmaps, goodbye
				if( !r_superLightStyle || r_superLightStyle->lightmapNum[0] < 0 || r_superLightStyle->lightmapStyles[0] == 255 )
					continue;

				// try to apply lightstyles
				if(( !( pass->glState & (GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK)) || ( pass->flags & SHADERSTAGE_BLEND_MODULATE )) && ( pass->rgbGen.type == RGBGEN_IDENTITY ) && ( pass->alphaGen.type == ALPHAGEN_IDENTITY ))
				{
					vec3_t colorSum, color;

					// the first pass is always GL_MODULATE or GL_REPLACE
					// other passes are GL_ADD
					r_lightmapPasses[0] = *pass;

					for( j = 0, l = 0, u = 0; j < LM_STYLES && r_superLightStyle->lightmapStyles[j] != 255; j++ )
					{
						VectorCopy( r_lightStyles[r_superLightStyle->lightmapStyles[j]].rgb, colorSum );
						VectorClear( color );

						for( ; ; l++ )
						{
							for( k = 0; k < 3; k++ )
							{
								colorSum[k] -= color[k];
								color[k] = bound( 0, colorSum[k], 1 );
							}

							if( l )
							{
								if( !color[0] && !color[1] && !color[2] )
									break;
								if( l == MAX_TEXTURE_UNITS+1 )
									r_lightmapPasses[0] = r_lightmapPasses[1];
								u = l % ( MAX_TEXTURE_UNITS+1 );
							}

							if( VectorCompare( color, colorWhite ) )
							{
								r_lightmapPasses[u].rgbGen.type = RGBGEN_IDENTITY;
							}
							else
							{
								if( !l )
								{
									r_lightmapPasses[0].flags &= ~SHADERSTAGE_BLENDMODE;
									r_lightmapPasses[0].flags |= SHADERSTAGE_BLEND_MODULATE;
								}
								r_lightmapPasses[u].rgbGen.type = RGBGEN_CONST;
								VectorCopy( color, r_lightmapPasses[u].rgbGen.args );
							}

							if( r_lightmap->integer && !l )
								R_SetupLightmapMode();
							R_AccumulatePass( &r_lightmapPasses[u] );
							r_lightmapStyleNum[r_numAccumPasses - 1] = j;
						}
					}
				}
				else
				{
					if( r_lightmap->integer )
					{
						R_SetupLightmapMode();
						pass = r_lightmapPasses;
					}
					R_AccumulatePass( pass );
					r_lightmapStyleNum[r_numAccumPasses - 1] = 0;
				}
				continue;
			}
			else if( r_lightmap->integer && ( r_currentShader->flags & SHADER_HASLIGHTMAP ))
				continue;
			if(( pass->flags & SHADERSTAGE_PORTALMAP ) && !( RI.params & RP_PORTALCAPTURED ))
				continue;
			if(( pass->flags & SHADERSTAGE_DETAIL ) && !r_detailtextures->integer )
				continue;
			if(( pass->flags & SHADERSTAGE_DLIGHT ) && !r_currentDlightBits )
				continue;
		}

		R_AccumulatePass( pass );
	}

	// accumulate dynamic lights pass and fog pass if any
	if( r_currentDlightBits && !( r_currentShader->flags & SHADER_NO_MODULATIVE_DLIGHTS ))
	{
		if( !r_lightmap->integer || !( r_currentShader->flags & SHADER_HASLIGHTMAP ))
			R_AccumulatePass( &r_dlightsPass );
	}

	if( r_currentShadowBits && ( r_currentShader->sort >= SORT_OPAQUE ) && ( r_currentShader->sort <= SORT_ALPHATEST ))
		R_AccumulatePass( &r_GLSLpasses[3] );

	if( GL_Support( R_SHADER_GLSL100_EXT ) && RI.currententity && RI.currententity->outlineHeight && r_outlines_scale->value > 0
		&& ( r_currentShader->sort == SORT_OPAQUE ) && ( r_currentShader->flags & SHADER_CULL_FRONT )  )
		R_AccumulatePass( &r_GLSLpassOutline );

	if( r_texFog && r_texFog->shader )
	{
		r_fogPass.textures[0] = tr.fogTexture;
		if( !r_currentShader->num_stages || r_currentShader->fog_dist || ( r_currentShader->flags & SHADER_SKYPARMS ) )
			r_fogPass.glState &= ~GLSTATE_DEPTHFUNC_EQ;
		else r_fogPass.glState |= GLSTATE_DEPTHFUNC_EQ;
		R_AccumulatePass( &r_fogPass );
	}

	// flush any remaining passes
	if( r_numAccumPasses )
		R_RenderAccumulatedPasses();

	R_ClearArrays();

	pglMatrixMode( GL_MODELVIEW );
}

/*
================
R_BackendCleanUpTextureUnits
================
*/
void R_BackendCleanUpTextureUnits( void )
{
	R_CleanUpTextureUnits( 1 );

	GL_LoadIdentityTexMatrix();
	pglMatrixMode( GL_MODELVIEW );

	GL_DisableAllTexGens();
	GL_SetTexCoordArrayMode( 0 );
}

/*
================
R_BackendSetPassMask
================
*/
void R_BackendSetPassMask( int mask )
{
	r_currentShaderPassMask = mask;
}

/*
================
R_BackendResetPassMask
================
*/
void R_BackendResetPassMask( void )
{
	r_currentShaderPassMask = GLSTATE_MASK;
}

/*
================
R_BackendBeginTriangleOutlines
================
*/
void R_BackendBeginTriangleOutlines( void )
{
	r_triangleOutlines = true;
	pglColor4fv( colorWhite );

	GL_Cull( 0 );
	GL_SetState( GLSTATE_NO_DEPTH_TEST );
	pglDisable( GL_TEXTURE_2D );
	pglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
}

/*
================
R_BackendEndTriangleOutlines
================
*/
void R_BackendEndTriangleOutlines( void )
{
	r_triangleOutlines = false;
	pglColor4fv( colorWhite );
	GL_SetState( 0 );
	pglEnable( GL_TEXTURE_2D );
	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
}

/*
================
R_SetColorForOutlines
================
*/
static _inline void R_SetColorForOutlines( void )
{
	int	type = r_currentMeshBuffer->sortkey & 3;

	switch( type )
	{
	case MB_MODEL:
		if( triState.fActive )
			pglColor4fv( colorBlue );			
		else if( r_currentMeshBuffer->infokey < 0 )
			pglColor4fv( colorRed );
		else pglColor4fv( colorWhite );
		break;
	case MB_SPRITE:
		pglColor4fv( colorBlue );
		break;
	case MB_POLY:
		pglColor4fv( colorGreen );
		break;
	}
}

/*
================
R_DrawTriangles
================
*/
static void R_DrawTriangles( void )
{
	// ignore triapi triangles for 'debug_surface' mode
	if( triState.fActive && !gl_wireframe->integer && r_speeds->integer == 4 )
		return;

	if( gl_wireframe->integer == 2 )
		R_SetColorForOutlines();

	if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
		pglDrawRangeElementsEXT( GL_TRIANGLES, 0, r_backacc.numVerts, r_backacc.numElems, GL_UNSIGNED_INT, elemsArray );
	else pglDrawElements( GL_TRIANGLES, r_backacc.numElems, GL_UNSIGNED_INT, elemsArray );
}

/*
================
R_DrawNormals
================
*/
static void R_DrawNormals( void )
{
	uint	i;

	if( r_shownormals->integer == 2 )
		R_SetColorForOutlines();

	pglBegin( GL_LINES );
	for( i = 0; i < r_backacc.numVerts; i++ )
	{
		pglVertex3fv( vertsArray[i] );
		pglVertex3f( vertsArray[i][0] + normalsArray[i][0], vertsArray[i][1] + normalsArray[i][1], vertsArray[i][2] + normalsArray[i][2] );
	}
	pglEnd();
}

static void R_DrawLine( int color, int numpoints, const float *points )
{
	int	i = numpoints - 1;
	vec3_t	p0, p1;

	VectorSet( p0, points[i*3+0], points[i*3+1], points[i*3+2] );

	for( i = 0; i < numpoints; i++ )
	{
		VectorSet( p1, points[i*3+0], points[i*3+1], points[i*3+2] );
 
		switch( color )
		{
		case 1:
			pglColor4fv( colorRed );
			break;
		case 2:
			pglColor4fv( colorGreen );
			break;
		case 4:
			pglColor4fv( colorBlue );
			break;
		default:
			pglColor4fv( colorWhite );
			break;
		}

		pglVertex3fv( p0 );
		pglVertex3fv( p1 );
 
 		VectorCopy( p1, p0 );
 	}
}

/*
================
R_DrawPhysDebug
================
*/
void R_DrawPhysDebug( void )
{
	if( r_physbdebug->integer )
	{
		// physic debug
		pglDisable( GL_TEXTURE_2D );
		GL_SetState( GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA );

		GL_LoadMatrix( RI.worldviewMatrix );
		pglBegin( GL_LINES );
		ri.ShowCollision( R_DrawLine );

		pglEnd();
		pglEnable( GL_TEXTURE_2D );
	}
}