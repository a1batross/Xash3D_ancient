//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     r_backend.c - render backend utilites
//=======================================================================

#include "r_local.h"
#include "r_meshbuffer.h"
#include "byteorder.h"
#include "mathlib.h"
#include "matrixlib.h" 
#include "const.h"

#define TABLE_SIZE		1024
#define TABLE_MASK		TABLE_SIZE - 1
#define TABLE_CLAMP(x)	(((uint)(( x ) * TABLE_SIZE ) & ( TABLE_MASK )))
#define TABLE_EVALUATE(t,x)	((t)[TABLE_CLAMP( x )])

#define NOISE_SIZE		256
#define NOISE_MASK		NOISE_SIZE - 1
#define NOISE_VAL(a)	rb_noisePerm[( a ) & ( NOISE_MASK )]
#define NOISE_INDEX(x,y,z,t)	NOISE_VAL( x + NOISE_VAL( y + NOISE_VAL( z + NOISE_VAL( t ))))
#define NOISE_LERP(a,b,w)	( a * ( 1.0f - w ) + b * w )

static char	r_speeds_msg[MAX_SYSPATH];
static float	rb_identityLighting;
static float	rb_sinTable[TABLE_SIZE];
static float	rb_triangleTable[TABLE_SIZE];
static float	rb_squareTable[TABLE_SIZE];
static float	rb_sawtoothTable[TABLE_SIZE];
static float	rb_inverseSawtoothTable[TABLE_SIZE];
static float	rb_noiseTable[NOISE_SIZE];
static int	rb_noisePerm[NOISE_SIZE];	// permutation table

static float	rb_sinTableByte[256];
static float	rb_warpSinTable[256] =
{
#include "warpsin.h"
};

static uint	rb_vertexBuffers[MAX_VERTEX_BUFFERS];
static int	rb_numVertexBuffers;
static int	rb_staticBytes;
static int	rb_staticCount;
static int	rb_streamBytes;
static int	rb_streamCount;


bool		r_triangleOutlines;
static bool	r_arraysLocked;
static bool	r_normalsEnabled;
static const meshbuffer_t *m_pRenderMeshBuffer;
static uint	r_currentDlightBits;
static uint	r_currentShadowBits;

vbo_t			rb_vbo;
int			m_iInfoKey;
float			m_fShaderTime;
static int		r_currentShaderState;
static int		r_currentShaderPassMask;
static const mfog_t		*r_texFog, *r_colorFog;
static const shadowGroup_t	*r_currentCastGroup;
static int		r_lightmapStyleNum[MAX_TEXTURE_UNITS];
static superLightStyle_t	*r_superLightStyle;

static shaderStage_t	rb_dlightsStage, r_fogStage;
static shaderStage_t	rb_lightmapStages[MAX_TEXTURE_UNITS+1];
static shaderStage_t	rb_GLSLstages[4];       // dlights and base
static shaderStage_t	*rb_accumStages[MAX_TEXTURE_UNITS];
static int		rb_numAccumStages;

ALIGN vec4_t		inVertsArray[MAX_ARRAY_VERTS];
ALIGN vec4_t		inNormalArray[MAX_ARRAY_VERTS];
vec4_t			inSVectorsArray[MAX_ARRAY_VERTS];
uint			inElemsArray[MAX_ARRAY_ELEMENTS];
vec2_t			inTexCoordArray[MAX_ARRAY_VERTS];
vec2_t			inLMCoordsArray[LM_STYLES][MAX_ARRAY_VERTS];
vec4_t			inColorArray[LM_STYLES][MAX_ARRAY_VERTS];
vec2_t			tUnitCoordsArray[MAX_TEXTURE_UNITS][MAX_ARRAY_VERTS];

uint		*indexArray;
vec4_t		*vertexArray;
vec4_t		*normalArray;
vec4_t		*sVectorArray;
vec2_t		*texCoordArray;
vec2_t		*lmCoordArray[LM_STYLES];
vec4_t		colorArray[MAX_ARRAY_VERTS];

static GLenum	rb_drawMode;
static GLboolean	rb_CheckFlush;
static GLint	rb_vertexState;

static void RB_SetVertex( float x, float y, float z )
{
	GLuint	oldIndex = r_stats.numIndices;

	switch( rb_drawMode )
	{
	case GL_LINES:
		inElemsArray[r_stats.numIndices++] = r_stats.numVertices;
		if( rb_vertexState++ == 1 )
		{
			RB_SetVertex( x + 1, y + 1, z + 1 );
			rb_vertexState = 0;
			rb_CheckFlush = true; // Flush for long sequences of quads.
		}
		break;
	case GL_TRIANGLES:
		inElemsArray[r_stats.numIndices++] = r_stats.numVertices;
		if( rb_vertexState++ == 2 )
		{
			rb_vertexState = 0;
			rb_CheckFlush = true; // Flush for long sequences of triangles.
		}
		break;
	case GL_QUADS:
		if( rb_vertexState++ < 3 )
		{
			inElemsArray[r_stats.numIndices++] = r_stats.numVertices;
		}
		else
		{
			// we've already done triangle (0, 1, 2), now draw (2, 3, 0)
			inElemsArray[r_stats.numIndices++] = r_stats.numVertices - 1;
			inElemsArray[r_stats.numIndices++] = r_stats.numVertices;
			inElemsArray[r_stats.numIndices++] = r_stats.numVertices - 3;
			rb_vertexState = 0;
			rb_CheckFlush = true; // flush for long sequences of quads.
		}
		break;
	case GL_TRIANGLE_STRIP:
		if( r_stats.numVertices + rb_vertexState > MAX_VERTICES )
		{
			// This is a strip that's too big for us to buffer.
			// (We can't just flush the buffer because we have to keep
			// track of the last two vertices.
			Host_Error( "RB_SetVertex: overflow: %i > MAX_VERTICES\n", r_stats.numVertices + rb_vertexState );
		}
		if( rb_vertexState++ < 3 )
		{
			inElemsArray[r_stats.numIndices++] = r_stats.numVertices;
		}
		else
		{
			// flip triangles between clockwise and counter clockwise
			if( rb_vertexState & 1 )
			{
				// draw triangle [n-2 n-1 n]
				inElemsArray[r_stats.numIndices++] = r_stats.numVertices - 2;
				inElemsArray[r_stats.numIndices++] = r_stats.numVertices - 1;
				inElemsArray[r_stats.numIndices++] = r_stats.numVertices;
			}
			else
			{
				// draw triangle [n-1 n-2 n]
				inElemsArray[r_stats.numIndices++] = r_stats.numVertices - 1;
				inElemsArray[r_stats.numIndices++] = r_stats.numVertices - 2;
				inElemsArray[r_stats.numIndices++] = r_stats.numVertices;
			}
		}
		break;
	case GL_POLYGON:
	case GL_TRIANGLE_FAN:	// same as polygon
		if( r_stats.numVertices + rb_vertexState > MAX_VERTICES )
		{
			// This is a polygon or fan that's too big for us to buffer.
			// (We can't just flush the buffer because we have to keep
			// track of the starting vertex.
			Host_Error( "RB_SetVertex: overflow: %i > MAX_VERTICES\n", r_stats.numVertices + rb_vertexState );
		}
		if( rb_vertexState++ < 3 )
		{
			inElemsArray[r_stats.numIndices++] = r_stats.numVertices;
		}
		else
		{
			// draw triangle [0 n-1 n]
			inElemsArray[r_stats.numIndices++] = r_stats.numVertices - ( rb_vertexState - 1 );
			inElemsArray[r_stats.numIndices++] = r_stats.numVertices - 1;
			inElemsArray[r_stats.numIndices++] = r_stats.numVertices;
		}
		break;
	default:
		Host_Error( "RB_SetVertex: unsupported mode: %i\n", rb_drawMode );
		break;
	}

	// copy current vertex
	vertexArray[r_stats.numVertices][0] = x;
	vertexArray[r_stats.numVertices][1] = y;
	vertexArray[r_stats.numVertices][2] = z;
	r_stats.numVertices++;

	// flush buffer if needed
	if( rb_CheckFlush ) Host_Error( "rb_CheckMeshOverflow called!\n" );
	// RB_CheckMeshOverflow( r_stats.numIndices - oldIndex, rb_vertexState );
}

static void RB_SetTexCoord( GLfloat s, GLfloat t )
{
	inTexCoordArray[r_stats.numVertices][0] = s;
	inTexCoordArray[r_stats.numVertices][1] = t;
}

static void RB_SetLmCoord( GLfloat s, GLfloat t )
{
	// FIXME: these set coords only for fisrt lightmap
	inLMCoordsArray[0][r_stats.numVertices][0] = s;
	inLMCoordsArray[0][r_stats.numVertices][1] = t;
}

static void RB_SetColor( GLfloat r, GLfloat g, GLfloat b, GLfloat a )
{
	inColorArray[0][r_stats.numVertices][0] = r;
	inColorArray[0][r_stats.numVertices][1] = g;
	inColorArray[0][r_stats.numVertices][2] = b;
	inColorArray[0][r_stats.numVertices][3] = b;
}

static void RB_SetNormal( GLfloat x, GLfloat y, GLfloat z )
{
	inNormalArray[r_stats.numVertices][0] = x;
	inNormalArray[r_stats.numVertices][1] = y;
	inNormalArray[r_stats.numVertices][2] = z;
}

/*
==============
RB_DisableTexGen
==============
*/
static void RB_DisableTexGen( void )
{
	GL_EnableTexGen( GL_S, GL_FALSE );
	GL_EnableTexGen( GL_T, GL_FALSE );
	GL_EnableTexGen( GL_R, GL_FALSE );
	GL_EnableTexGen( GL_Q, GL_FALSE );
}

/*
=================
GL subsystem

arrays backend that emulate basic opengl funcs
=================
*/
void GL_Begin( GLuint drawMode )
{
	rb_drawMode = drawMode;
	rb_vertexState = 0;
	rb_CheckFlush = false;
}

void GL_End( void )
{
	if( r_stats.numIndices == 0 ) return;
	// RB_CheckMeshOverflow( 0, 0 );
}

void GL_Vertex2f( GLfloat x, GLfloat y )
{
	RB_SetVertex( x, y, 0 );
}

void GL_Vertex3f( GLfloat x, GLfloat y, GLfloat z )
{
	RB_SetVertex( x, y, z );
}

void GL_Vertex3fv( const GLfloat *v )
{
	RB_SetVertex( v[0], v[1], v[2] );
}

void GL_Normal3f( GLfloat x, GLfloat y, GLfloat z )
{
	RB_SetNormal( x, y, z );
}

void GL_Normal3fv( const GLfloat *v )
{
	RB_SetNormal( v[0], v[1], v[2] );
}

void GL_TexCoord2f( GLfloat s, GLfloat t )
{
	RB_SetTexCoord( s, t );
}

void GL_TexCoord4f( GLfloat s, GLfloat t, GLfloat ls, GLfloat lt )
{
	RB_SetTexCoord( s, t );
	RB_SetLmCoord( ls, lt );
}

void GL_TexCoord4fv( const GLfloat *v )
{
	RB_SetTexCoord( v[0], v[1] );
	RB_SetLmCoord( v[2], v[3] );
}

void GL_Color3f( GLfloat r, GLfloat g, GLfloat b )
{
	RB_SetColor( r, g, b, 1.0f );
}

void GL_Color3fv( const GLfloat *v )
{
	RB_SetColor( v[0], v[1], v[2], 1.0f );
}

void GL_Color4f( GLfloat r, GLfloat g, GLfloat b, GLfloat a )
{
	RB_SetColor( r, g, b, a );
}

void GL_Color4fv( const GLfloat *v )
{
	RB_SetColor( v[0], v[1], v[2], v[3] );
}

void GL_Color4ub( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha )
{
	GL_Color4fv(UnpackRGBA(MakeRGBA( red, green, blue, alpha )));
}

void GL_Color4ubv( const GLubyte *v )
{
	GL_Color4fv(UnpackRGBA(BuffLittleLong( v )));
}

/*
=================
RB_BuildTables
=================
*/
static void RB_BuildTables( void )
{
	int	i;
	float	f;

	if( gl_config.deviceSupportsGamma && r_hwgamma->integer )
		rb_identityLighting = ( 1.0f / pow( 2.0f, max( 0, floor( r_overbrightbits->value ))));
	else rb_identityLighting = 1.0f;

	for( i = 0; i < TABLE_SIZE; i++ )
	{
		f = (float)i / (float)TABLE_SIZE;
		rb_sinTable[i] = sin(f * M_PI2);
		if( f < 0.25 ) rb_triangleTable[i] = 4.0 * f;
		else if( f < 0.75 ) rb_triangleTable[i] = 2.0 - 4.0 * f;
		else rb_triangleTable[i] = (f - 0.75) * 4.0 - 1.0;
		if( f < 0.5 ) rb_squareTable[i] = 1.0;
		else rb_squareTable[i] = -1.0;
		rb_sawtoothTable[i] = f;
		rb_inverseSawtoothTable[i] = 1.0 - f;
	}

	// init the noise table
	for( i = 0; i < NOISE_SIZE; i++ )
	{
		rb_noiseTable[i] = Com_RandomFloat( -1.0f, 1.0f );
		// premutation in range (0 - NOISE_SIZE)
		rb_noisePerm[i] = Com_RandomLong( 0, NOISE_MASK );
	}

	for( i = 0; i < 256; i++ )
		rb_sinTableByte[i] = sin((float)i / 255.0 * M_PI2 );
}

/*
==============
R_FastSin

get sin values from table
==============
*/
static float R_FastSin( float t )
{
	return TABLE_EVALUATE( rb_sinTable, t );
}

/*
=============
R_LatLongToNorm
=============
*/
void R_LatLongToNorm( const byte latlong[2], vec3_t out )
{
	float	sin_a, sin_b, cos_a, cos_b;

	cos_a = rb_sinTableByte[(latlong[0] + 64 ) & 255];
	sin_a = rb_sinTableByte[latlong[0]];
	cos_b = rb_sinTableByte[(latlong[1] + 64 ) & 255];
	sin_b = rb_sinTableByte[latlong[1]];

	VectorSet( out, cos_b * sin_a, sin_b * sin_a, cos_a );
}

/*
=================
RB_TableForFunc
=================
*/
static float *RB_TableForFunc( const waveFunc_t *func )
{
	switch( func->type )
	{
	case WAVEFORM_SIN:
		return rb_sinTable;
	case WAVEFORM_TRIANGLE:
		return rb_triangleTable;
	case WAVEFORM_SQUARE:
		return rb_squareTable;
	case WAVEFORM_SAWTOOTH:
		return rb_sawtoothTable;
	case WAVEFORM_INVERSESAWTOOTH:
		return rb_inverseSawtoothTable;
	case WAVEFORM_NOISE:
		return rb_noiseTable;
	}
	Host_Error( "RB_TableForFunc: unknown waveform type %i in shader '%s'\n", func->type, Ref.m_pCurrentShader->name );
	return NULL;
}

/*
==============
R_BackendGetNoiseValue
==============
*/
static float RB_GetNoiseValue( float x, float y, float z, float t )
{
	int	ix, iy, iz, it;
	float	fx, fy, fz, ft;
	float	front[4], back[4];
	float	fvalue, bvalue, value[2];
	int	i;

	ix = (int)floor( x );
	fx = x - ix;
	iy = (int)floor( y );
	fy = y - iy;
	iz = (int)floor( z );
	fz = z - iz;
	it = (int)floor( t );
	ft = t - it;

	for( i = 0; i < 2; i++ )
	{
		front[0] = rb_noiseTable[NOISE_INDEX( ix, iy, iz, it + i )];
		front[1] = rb_noiseTable[NOISE_INDEX( ix+1, iy, iz, it + i )];
		front[2] = rb_noiseTable[NOISE_INDEX( ix, iy+1, iz, it + i )];
		front[3] = rb_noiseTable[NOISE_INDEX( ix+1, iy+1, iz, it + i )];

		back[0] = rb_noiseTable[NOISE_INDEX( ix, iy, iz + 1, it + i )];
		back[1] = rb_noiseTable[NOISE_INDEX( ix+1, iy, iz + 1, it + i )];
		back[2] = rb_noiseTable[NOISE_INDEX( ix, iy+1, iz + 1, it + i )];
		back[3] = rb_noiseTable[NOISE_INDEX( ix+1, iy+1, iz + 1, it + i )];

		fvalue = NOISE_LERP( NOISE_LERP( front[0], front[1], fx ), NOISE_LERP( front[2], front[3], fx ), fy );
		bvalue = NOISE_LERP( NOISE_LERP( back[0], back[1], fx ), NOISE_LERP( back[2], back[3], fx ), fy );
		value[i] = NOISE_LERP( fvalue, bvalue, fz );
	}

	return NOISE_LERP( value[0], value[1], ft );
}

/*
==============
R_LockArrays
==============
*/
void R_LockArrays( int numverts )
{
	if( r_arraysLocked ) return;

	if( !GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
	{
		pglVertexPointer( 3, GL_FLOAT, 16, vertexArray );

		if( r_features & MF_ENABLENORMALS )
		{
			r_normalsEnabled = true;
			pglEnableClientState( GL_NORMAL_ARRAY );
			pglNormalPointer( GL_FLOAT, 16, normalArray );
		}
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
	if( !r_arraysLocked ) return;

	if( GL_Support( R_CUSTOM_VERTEX_ARRAY_EXT ))
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

	r_stats.numVertices = 0;
	r_stats.numIndices = 0;
	r_stats.numColors = 0;

	vertexArray = inVertsArray;
	indexArray = inElemsArray;
	normalArray = inNormalArray;
	sVectorArray = inSVectorsArray;
	texCoordArray = inTexCoordArray;
	for( i = 0; i < LM_STYLES; i++ )
		lmCoordArray[i] = inLMCoordsArray[i];
}

/*
==============
R_FlushArrays
==============
*/
void R_FlushArrays( void )
{
	if( !r_stats.numVertices || !r_stats.numIndices )
		return;

	if( r_stats.numColors == 1 )
	{
		pglColor4fv( colorArray[0] );
	}
	else if( r_stats.numColors > 1 )
	{
		pglEnableClientState( GL_COLOR_ARRAY );
		if( !GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
			pglColorPointer( 4, GL_FLOAT, 0, colorArray );
	}

	if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
		pglDrawRangeElementsEXT( GL_TRIANGLES, 0, r_stats.numVertices, r_stats.numIndices, GL_UNSIGNED_INT, indexArray );
	else pglDrawElements( GL_TRIANGLES, r_stats.numIndices, GL_UNSIGNED_INT, indexArray );

	if( r_stats.numColors > 1 ) pglDisableClientState( GL_COLOR_ARRAY );

	r_stats.totalTris += r_stats.numIndices / 3;
	r_stats.totalFlushes++;
}

/*
==============
RB_CleanUpTextureUnits

FIXME: rename
not a RB_CleanUpTextureUnit!
==============
*/
void RB_CleanUpTextureUnits( int last )
{
	int	i;

	for( i = gl_state.activeTMU; i > last - 1; i-- )
	{
		RB_DisableTexGen();
		GL_TexCoordMode( GL_FALSE );

		pglDisable( GL_TEXTURE_2D );
		GL_SelectTexture( i - 1 );
	}
}

/*
================
R_CleanUpTextureUnits

disable custom texcoords and texgen
================
*/
void R_CleanUpTextureUnits( void )
{
	RB_CleanUpTextureUnits( GL_TEXTURE1 );

	GL_LoadIdentityTexMatrix();
	pglMatrixMode( GL_MODELVIEW );

	RB_DisableTexGen();
	GL_TexCoordMode( GL_NONE );
}

/*
==============
RB_ResetCounters
==============
*/
void RB_ResetCounters( void )
{
	Mem_Set( &r_stats, 0, sizeof( r_stats ));
}

/*
================
RB_SetPassMask
================
*/
void RB_SetPassMask( int mask )
{
	r_currentShaderPassMask = mask;
}

/*
================
RB_ResetPassMask
================
*/
void RB_ResetPassMask( void )
{
	r_currentShaderPassMask = GLSTATE_MASK;
}

/*
================
RB_BeginTriangleOutlines
================
*/
void RB_BeginTriangleOutlines( void )
{
	r_triangleOutlines = true;
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	GL_CullFace( GL_NONE );
	GL_SetState( GLSTATE_NO_DEPTH_TEST );
	pglDisable( GL_TEXTURE_2D );
	pglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
}

/*
================
RB_EndTriangleOutlines
================
*/
void RB_EndTriangleOutlines( void )
{
	r_triangleOutlines = false;
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	GL_SetState( 0 );
	pglEnable( GL_TEXTURE_2D );
	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
}

/*
================
RB_SetColorForOutlines
================
*/
static void RB_SetColorForOutlines( void )
{
	int	type = m_pRenderMeshBuffer->sortKey & 3;

	switch( type )
	{
	case MESH_MODEL:
		if( m_pRenderMeshBuffer->infoKey < 0 )
			pglColor4f( 1.0f, 0.0f, 0.0f, 1.0f );
		else pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		break;
	case MESH_SPRITE:
		pglColor4f( 0.0f, 0.0f, 1.0f, 1.0f );
		break;
	case MESH_POLY:
		pglColor4f( 0.0f, 1.0f, 0.0f, 1.0f );
		break;
	}
}

/*
================
RB_DrawTriangles
================
*/
static void RB_DrawTriangles( void )
{
	if( r_showtris->integer == 2 )
		RB_SetColorForOutlines();

	if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
		pglDrawRangeElementsEXT( GL_TRIANGLES, 0, r_stats.numVertices, r_stats.numIndices, GL_UNSIGNED_INT, indexArray );
	else pglDrawElements( GL_TRIANGLES, r_stats.numIndices, GL_UNSIGNED_INT, indexArray );
}


/*
=================
RB_DrawNormals
=================
*/
static void RB_DrawNormals( void )
{
	int	i;
	vec3_t	v;

	if( r_shownormals->integer == 2 )
		RB_SetColorForOutlines();

	pglBegin( GL_LINES );
	for( i = 0; i < r_stats.numVertices; i++ )
	{
		VectorAdd( vertexArray[i], normalArray[i], v );
		pglVertex3fv( vertexArray[i] );
		pglVertex3fv( v );
	}
	pglEnd();
}

/*
=================
RB_DrawTangentSpace
=================
*/
static void RB_DrawTangentSpace( void )
{
#if 0	// FIXME: get to work

	int	i;
	vec3_t	v;

	// tangent
	pglColor4f( 1.0f, 0.0f, 0.0f, 1.0f );
	pglBegin( GL_LINES );
	for( i = 0; i < r_stats.numVertices; i++ )
	{
		VectorAdd( vertexArray[i], tangentArray[i], v );
		pglVertex3fv( vertexArray[i] );
		pglVertex3fv( v );
	}
	pglEnd();

	// binormal
	pglColor4f( 0.0f, 1.0f, 0.0f, 1.0f );
	pglBegin( GL_LINES );
	for( i = 0; i < r_stats.numVertices; i++ )
	{
		VectorAdd( vertexArray[i], binormalArray[i], v );
		pglVertex3fv( vertexArray[i] );
		pglVertex3fv( v );
	}
	pglEnd();

	// normal
	pglColor4f( 0.0f, 0.0f, 1.0f, 1.0f );
	pglBegin( GL_LINES );
	for( i = 0; i < r_stats.numVertices; i++ )
	{
		VectorAdd( vertexArray[i], normalArray[i], v );
		pglVertex3fv( vertexArray[i] );
		pglVertex3fv( v );
	}
	pglEnd();
#endif
}

/*
=================
RB_DrawModelBounds
=================
*/
static void RB_DrawModelBounds( void )
{
#if 0
	rmodel_t		*model;
	vec3_t		bbox[8];
	int		i;

	if( m_pCurrentEntity == r_worldEntity )
		return;

	if( m_pRenderMesh->meshType == MESH_SURFACE )
	{
		model = m_pCurrentEntity->model;

		// compute a full bounding box
		for( i = 0; i < 8; i++ )
		{
			bbox[i][0] = (i & 1) ? model->mins[0] : model->maxs[0];
			bbox[i][1] = (i & 2) ? model->mins[1] : model->maxs[1];
			bbox[i][2] = (i & 4) ? model->mins[2] : model->maxs[2];
		}
	}
	else if( m_pRenderMesh->meshType == MESH_STUDIO )
	{
		R_StudioComputeBBox( bbox );
	}
	else return;

	// draw it
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	pglBegin( GL_LINES );
	for( i = 0; i < 2; i += 1 )
	{
		pglVertex3fv(bbox[i+0]);
		pglVertex3fv(bbox[i+2]);
		pglVertex3fv(bbox[i+4]);
		pglVertex3fv(bbox[i+6]);
		pglVertex3fv(bbox[i+0]);
		pglVertex3fv(bbox[i+4]);
		pglVertex3fv(bbox[i+2]);
		pglVertex3fv(bbox[i+6]);
		pglVertex3fv(bbox[i*2+0]);
		pglVertex3fv(bbox[i*2+1]);
		pglVertex3fv(bbox[i*2+4]);
		pglVertex3fv(bbox[i*2+5]);
	}
	pglEnd();
#endif
}

/*
==============
RB_StartFrame
==============
*/
void RB_StartFrame( void )
{
	r_speeds_msg[0] = '\0';
	RB_ResetCounters();
}

/*
==============
RB_EndFrame
==============
*/
void RB_EndFrame( void )
{
	// unlock arrays if any
	R_UnlockArrays();

	// clean up texture units
	RB_CleanUpTextureUnits( GL_TEXTURE1 );

	if( r_speeds->integer && !( Ref.refdef.rdflags & RDF_NOWORLDMODEL ))
	{
		com.snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%4i wpoly %4i leafs %4i verts %4i tris %4i flushes %3i locks",
		r_stats.brushPolys, r_stats.worldLeafs, r_stats.totalVerts, r_stats.totalTris, r_stats.totalFlushes, r_stats.totalKeptLocks );

	}
}

/*
=================
RB_DeformVertexes
=================
*/
static void RB_DeformVertexes( void )
{
	deformVerts_t	*deformVertexes = Ref.m_pCurrentShader->deformVertexes;
	uint		deformVertexesNum = Ref.m_pCurrentShader->deformVertexesNum;
	float		deflect, *quad[4];
	double		temp, params[4];
	vec3_t		tv, rot_centre;
	int		i, j, k;
	const float	*table;

	for( i = 0; i < deformVertexesNum; i++, deformVertexes++ )
	{
		switch( deformVertexes->type )
		{
		case DEFORMVERTEXES_NONE:
			break;
		case DEFORMVERTEXES_WAVE:
			table = RB_TableForFunc( &deformVertexes->func );
			// deflect vertex along its normal by wave amount
			if( deformVertexes->func.params[3] == 0 )
			{
				temp = deformVertexes->func.params[2];
				deflect = TABLE_EVALUATE( table, temp ) * deformVertexes->func.params[1] + deformVertexes->func.params[0];

				for( j = 0; j < r_stats.numVertices; j++ )
					VectorMA( inVertsArray[j], deflect, inNormalArray[j], inVertsArray[j] );
			}
			else
			{
				params[0] = deformVertexes->func.params[0];
				params[1] = deformVertexes->func.params[1];
				params[2] = deformVertexes->func.params[2] + deformVertexes->func.params[3] * m_fShaderTime;
				params[3] = deformVertexes->func.params[0];

				for( j = 0; j < r_stats.numVertices; j++ )
				{
					temp = params[2] + params[3] * ( inVertsArray[j][0] + inVertsArray[j][1] + inVertsArray[j][2] );
					deflect = TABLE_EVALUATE( table, temp ) * params[1] + params[0];
					VectorMA( inVertsArray[j], deflect, inNormalArray[j], inVertsArray[j] );
				}
			}
			break;
		case DEFORMVERTEXES_NORMAL:
			// without this * 0.1f deformation looks wrong, although q3a doesn't have it
			params[0] = deformVertexes->func.params[3] * m_fShaderTime * 0.1f;
			params[1] = deformVertexes->func.params[1];

			for( j = 0; j < r_stats.numVertices; j++ )
			{
				VectorScale( inVertsArray[j], 0.98f, tv );
				inNormalArray[j][0] += params[1] * RB_GetNoiseValue( tv[0], tv[1], tv[2], params[0] );
				inNormalArray[j][1] += params[1] * RB_GetNoiseValue( tv[0] + 100, tv[1], tv[2], params[0] );
				inNormalArray[j][2] += params[1] * RB_GetNoiseValue( tv[0] + 200, tv[1], tv[2], params[0] );
				VectorNormalizeFast( inNormalArray[j] );
			}
			break;
		case DEFORMVERTEXES_MOVE:
			table = RB_TableForFunc( &deformVertexes->func );
			temp = deformVertexes->func.params[2] + m_fShaderTime * deformVertexes->func.params[3];
			deflect = TABLE_EVALUATE( table, temp ) * deformVertexes->func.params[1] + deformVertexes->func.params[0];

			for( j = 0; j < r_stats.numVertices; j++ )
				VectorMA( inVertsArray[j], deflect, deformVertexes->func.params, inVertsArray[j] );
			break;
		case DEFORMVERTEXES_BULGE:
			params[0] = deformVertexes->func.params[0];
			params[1] = deformVertexes->func.params[1];
			params[2] = m_fShaderTime * deformVertexes->func.params[2];

			for( j = 0; j < r_stats.numVertices; j++ )
			{
				temp = ( texCoordArray[j][0] * params[0] + params[2] ) / M_PI2;
				deflect = R_FastSin( temp ) * params[1];
				VectorMA( inVertsArray[j], deflect, inNormalArray[j], inVertsArray[j] );
			}
			break;
		case DEFORMVERTEXES_AUTOSPRITE:
			{
				vec4_t	*v;
				vec2_t	*st;
				uint	*elem;
				float	radius;
				vec3_t	point, v_centre, v_right, v_up;

				if( r_stats.numVertices % 4 || r_stats.numIndices % 6 )
					break;

				if( Ref.m_pCurrentEntity && (Ref.m_pCurrentModel != r_worldModel ))
				{
					Matrix3x3_Transform( Ref.m_pCurrentEntity->matrix, Ref.right, v_right );
					Matrix3x3_Transform( Ref.m_pCurrentEntity->matrix, Ref.up, v_up );
				}
				else
				{
					VectorCopy( Ref.right, v_right );
					VectorCopy( Ref.up, v_up );
				}

				radius = Ref.m_pCurrentEntity->scale;
				if( radius && radius != 1.0f )
				{
					radius = 1.0f / radius;
					VectorScale( v_right, radius, v_right );
					VectorScale( v_up, radius, v_up );
				}

				for( k = 0, v = inVertsArray, st = texCoordArray, elem = indexArray; k < r_stats.numVertices; k += 4, v += 4, st += 4, elem += 6 )
				{
					for( j = 0; j < 3; j++ )
						v_centre[j] = (v[0][j] + v[1][j] + v[2][j] + v[3][j]) * 0.25f;

					VectorSubtract( v[0], v_centre, point );
					radius = VectorLength( point ) * 0.707106f;	// 1.0f / sqrt(2)

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
		case DEFORMVERTEXES_AUTOSPRITE2:
			if( r_stats.numIndices % 6 )
				break;

			for( k = 0; k < r_stats.numIndices; k += 6 )
			{
				int	long_axis = 0, short_axis = 0;
				vec3_t	axis, tmp;
				float	len[3];
				matrix3x3	m0, m1, m2, result;

				quad[0] = (float *)(inVertsArray + indexArray[k+0]);
				quad[1] = (float *)(inVertsArray + indexArray[k+1]);
				quad[2] = (float *)(inVertsArray + indexArray[k+2]);

				for( j = 2; j >= 0; j-- )
				{
					quad[3] = (float *)(inVertsArray + indexArray[k+3+j]);

					if( !VectorCompare( quad[3], quad[0] ) && !VectorCompare( quad[3], quad[1] ) && !VectorCompare( quad[3], quad[2] ))
						break;
				}

				// build a matrix were the longest axis of the billboard is the Y-Axis
				VectorSubtract( quad[1], quad[0], m0[0] );
				VectorSubtract( quad[2], quad[0], m0[1] );
				VectorSubtract( quad[2], quad[1], m0[2] );
				len[0] = DotProduct( m0[0], m0[0] );
				len[1] = DotProduct( m0[1], m0[1] );
				len[2] = DotProduct( m0[2], m0[2] );

				if( ( len[2] > len[1] ) && ( len[2] > len[0] ) )
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
				else if( ( len[1] > len[2] ) && ( len[1] > len[0] ) )
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
				else if( ( len[0] > len[1] ) && ( len[0] > len[2] ) )
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

				if( Ref.m_pCurrentEntity && ( Ref.m_pCurrentModel != r_worldModel ))
				{
					VectorAdd( Ref.m_pCurrentEntity->origin, rot_centre, tv );
					VectorSubtract( Ref.vieworg, tv, tmp );
					Matrix3x3_Transform( Ref.m_pCurrentEntity->matrix, tmp, tv );
				}
				else
				{
					VectorCopy( rot_centre, tv );
					VectorSubtract( Ref.vieworg, tv, tv );
				}

				// filter any longest-axis-parts off the camera-direction
				deflect = -DotProduct( tv, axis );

				VectorMA( tv, deflect, axis, m1[2] );
				VectorNormalizeFast( m1[2] );
				VectorCopy( axis, m1[1] );
				CrossProduct( m1[1], m1[2], m1[0] );

				Matrix3x3_Transpose( m1, m2 );
				Matrix3x3_Concat( result, m2, m0 );

				for( j = 0; j < 4; j++ )
				{
					VectorSubtract( quad[j], rot_centre, tv );
					Matrix3x3_Transform( result, tv, quad[j] );
					VectorAdd( rot_centre, quad[j], quad[j] );
				}
			}
			break;
		case DEFORMVERTEXES_PROJECTION_SHADOW:
			// R_DeformVPlanarShadow( r_stats.numVertices, inVertsArray[0] );
			break;
		case DEFORMVERTEXES_AUTOPARTICLE:
			{
				float	scale;
				matrix3x3	m0, m1, m2, result;

				if( r_stats.numIndices % 6 )
					break;

				if( Ref.m_pCurrentEntity && ( Ref.m_pCurrentModel != r_worldModel ))
					Matrix4x4_ToMatrix3x3( m1, Ref.modelViewMatrix );
				else Matrix4x4_ToMatrix3x3( m1, Ref.worldMatrix );

				Matrix3x3_Transpose( m1, m2 );

				for( k = 0; k < r_stats.numIndices; k += 6 )
				{
					quad[0] = (float *)(inVertsArray + indexArray[k+0]);
					quad[1] = (float *)(inVertsArray + indexArray[k+1]);
					quad[2] = (float *)(inVertsArray + indexArray[k+2]);

					for( j = 2; j >= 0; j-- )
					{
						quad[3] = (float *)(inVertsArray + indexArray[k+3+j]);

						if(!VectorCompare( quad[3], quad[0] ) && !VectorCompare( quad[3], quad[1] ) && !VectorCompare( quad[3], quad[2] ))
							break;
					}

					Matrix3x3_FromPoints( quad[0], quad[1], quad[2], m0 );
					Matrix3x3_Concat( result, m2, m0 );

					// HACK a scale up to keep particles from disappearing
					scale = ( quad[0][0] - Ref.vieworg[0] ) * Ref.forward[0] + ( quad[0][1] - Ref.vieworg[1] ) * Ref.forward[1] + ( quad[0][2] - Ref.vieworg[2] ) * Ref.forward[2];
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
		default:
			Host_Error( "RB_DeformVertexes: unknown deformVertexes type %i in shader '%s'\n", deformVertexes->type, Ref.m_pCurrentShader->name );
		}
	}
}

/*
=================
RB_CalcVertexColors
=================
*/
static void RB_CalcVertexColors( const shaderStage_t *stage )
{

	const rgbGen_t	*rgbGen = &stage->rgbGen;
	const alphaGen_t	*alphaGen = &stage->alphaGen;
	bool		noArray, identityAlpha, entityAlpha;
	float		rgba[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float		*fArray, *inArray, a; 
	vec3_t		v, t, style;
	int		i, c, bits = 1;	// to avoid division by zero
	const float	*table;
	double		temp;

	fArray = colorArray[0];
	inArray = inColorArray[0][0];
	noArray = ( stage->flags & SHADERSTAGE_NOCOLORARRAY ) && !r_colorFog;
	r_stats.numColors = noArray ? 1 : r_stats.numVertices;
	if( r_overbrightbits->integer > 0 && r_hwgamma->integer )
		bits = (1<<r_overbrightbits->integer);

	if( rgbGen->type != RGBGEN_IDENTITYLIGHTING && rgbGen->type != RGBGEN_EXACTVERTEX )
	{
		entityAlpha = false;
		identityAlpha = true;
		Mem_Set( fArray, 1.0f, sizeof( vec4_t ) * r_stats.numColors );
	}

	switch( rgbGen->type )
	{
	case RGBGEN_IDENTITY:
		break;
	case RGBGEN_IDENTITYLIGHTING:
		entityAlpha = identityAlpha = false;
		Mem_Set( fArray, rb_identityLighting, sizeof( vec4_t ) * r_stats.numColors );
		break;
	case RGBGEN_EXACTVERTEX:
		entityAlpha = identityAlpha = false;
		Mem_Copy( fArray, inArray, sizeof( vec4_t ) * r_stats.numColors );
		break;
	case RGBGEN_CONST:
		rgba[0] = rgbGen->params[0];
		rgba[1] = rgbGen->params[1];
		rgba[2] = rgbGen->params[2];

		for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
			Vector4Copy( rgba, fArray );
		break;
	case RGBGEN_WAVE:
		if( rgbGen->func.type == WAVEFORM_NOISE )
		{
			temp = RB_GetNoiseValue( 0, 0, 0, ( m_fShaderTime + rgbGen->func.params[2] ) * rgbGen->func.params[3] );
		}
		else
		{
			table = RB_TableForFunc( &rgbGen->func );
			temp = m_fShaderTime * rgbGen->func.params[3] + rgbGen->func.params[2];
			temp = TABLE_EVALUATE( table, temp ) * rgbGen->func.params[1] + rgbGen->func.params[0];
		}

		temp = temp * rgbGen->func.params[1] + rgbGen->func.params[0];
		rgba[0] = bound( 0.0f, rgbGen->func.params[0] * temp, 1.0f );
		rgba[1] = bound( 0.0f, rgbGen->func.params[1] * temp, 1.0f );
		rgba[2] = bound( 0.0f, rgbGen->func.params[2] * temp, 1.0f );

		for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
			Vector4Copy( rgba, fArray );
		break;
	case RGBGEN_ENTITY:
		entityAlpha = true;
		identityAlpha = ( Ref.m_pCurrentEntity->rendercolor[3] == 1.0f );
		for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
			Vector4Copy( Ref.m_pCurrentEntity->rendercolor, fArray );
		break;
	case RGBGEN_ONEMINUSENTITY:
		rgba[0] = 1.0f - Ref.m_pCurrentEntity->rendercolor[0];
		rgba[1] = 1.0f - Ref.m_pCurrentEntity->rendercolor[1];
		rgba[2] = 1.0f - Ref.m_pCurrentEntity->rendercolor[2];

		for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
			Vector4Copy( rgba, fArray );
		break;
	case RGBGEN_VERTEX:
		VectorSet( style, -1, -1, -1 );
		if( !r_superLightStyle || r_superLightStyle->vStyles[1] == 255 )
		{
			VectorSet( style, 1, 1, 1 );
			if( r_superLightStyle && r_superLightStyle->vStyles[0] != 255 )
				VectorCopy( r_lightStyles[r_superLightStyle->vStyles[0]].rgb, style );
		}

		if( style[0] == style[1] && style[1] == style[2] && style[2] == 1 )
		{
			for( i = 0; i < r_stats.numColors; i++, fArray += 4, inArray += 4 )
				VectorDivide( inArray, bits, fArray );
		}
		else
		{
			int	j;
			float	*tc;
			vec3_t	temp[MAX_ARRAY_VERTS];

			Mem_Set( temp, 0, sizeof( vec3_t ) * r_stats.numColors );

			for( j = 0; j < LM_STYLES && r_superLightStyle->vStyles[j] != 255; j++ )
			{
				VectorCopy( r_lightStyles[r_superLightStyle->vStyles[j]].rgb, style );
				if( VectorIsNull( style )) continue;

				inArray = inColorArray[j][0];
				for( i = 0, tc = temp[0]; i < r_stats.numColors; i++, tc += 3, inArray += 4 )
				{
					tc[0] += ( inArray[0] / bits ) * style[0];
					tc[1] += ( inArray[1] / bits ) * style[1];
					tc[2] += ( inArray[2] / bits ) * style[2];
				}
			}
			for( i = 0, tc = temp[0]; i < r_stats.numColors; i++, tc += 3, fArray += 4 )
			{
				fArray[0] = bound( 0.0f, tc[0], 1.0f );
				fArray[1] = bound( 0.0f, tc[1], 1.0f );
				fArray[2] = bound( 0.0f, tc[2], 1.0f );
			}
		}
		break;
	case RGBGEN_ONEMINUSVERTEX:
		for( i = 0; i < r_stats.numColors; i++, fArray += 4, inArray += 4 )
		{
			fArray[0] = 1.0f - ( inArray[0] / bits );
			fArray[1] = 1.0f - ( inArray[1] / bits );
			fArray[2] = 1.0f - ( inArray[2] / bits );
		}
		break;
	case RGBGEN_LIGHTINGDIFFUSE:
		if( Ref.m_pCurrentEntity )
			R_LightForEntity( Ref.m_pCurrentEntity, fArray );
		break;
	case RGBGEN_LIGHTINGDIFFUSE_ONLY:
		if( Ref.m_pCurrentEntity && !( Ref.params & RP_SHADOWMAPVIEW ) )
		{
			if( Ref.m_pCurrentEntity->renderfx & RF_FULLBRIGHT )
				VectorSet( rgba, 1.0f, 1.0f, 1.0f );
			else R_LightForPoint( Ref.m_pCurrentEntity->lightOrg, t, NULL, rgba, Ref.m_pCurrentModel->radius * Ref.m_pCurrentEntity->scale );

			for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
				Vector4Copy( rgba, fArray );
		}
		break;
	case RGBGEN_LIGHTINGAMBIENT_ONLY:
		if( Ref.m_pCurrentEntity && !( Ref.params & RP_SHADOWMAPVIEW ))
		{
			if( Ref.m_pCurrentEntity->renderfx & RF_FULLBRIGHT )
				VectorSet( rgba, 1.0f, 1.0f, 1.0f );
			else R_LightForPoint( Ref.m_pCurrentEntity->lightOrg, t, rgba, NULL, Ref.m_pCurrentEntity->radius * Ref.m_pCurrentEntity->scale );

			for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
				Vector4Copy( rgba, fArray );
		}
		break;
	case RGBGEN_FOG:
		for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
			Vector4Copy( r_texFog->shader->fogColor, fArray );
		break;
	case RGBGEN_ENVIRONMENT:
		// disabled
		break;
	default:	break;
	}

	fArray = colorArray[0];
	inArray = inColorArray[0][0];

	switch( alphaGen->type )
	{
	case ALPHAGEN_IDENTITY:
		if( identityAlpha ) break;
		for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
			fArray[3] = 1.0f;
		break;
	case ALPHAGEN_CONST:
		for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
			fArray[3] = alphaGen->params[0];
		break;
	case ALPHAGEN_WAVE:
		if( alphaGen->func.type == WAVEFORM_NOISE )
		{
			a = RB_GetNoiseValue( 0.0f, 0.0f, 0.0f, ( m_fShaderTime + alphaGen->func.params[2] ) * alphaGen->func.params[3] );
		}
		else
		{
			table = RB_TableForFunc( &alphaGen->func );
			a = alphaGen->func.params[2] + m_fShaderTime * alphaGen->func.params[3];
			a = TABLE_EVALUATE( table, a );
		}

		a = bound( 0.0f, a * alphaGen->func.params[1] + alphaGen->func.params[0], 1.0f );

		for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
			fArray[3] = a;
		break;
	case ALPHAGEN_FADE:
	case ALPHAGEN_PORTAL:
		VectorAdd( vertexArray[0], Ref.m_pCurrentEntity->origin, v );
		VectorSubtract( Ref.vieworg, v, t );
		a = VectorLength( t ) * alphaGen->func.params[0];
		a = bound( 0.0f, a, 1.0f );

		for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
			fArray[3] = a;
		break;
	case ALPHAGEN_VERTEX:
		for( i = 0; i < r_stats.numColors; i++, fArray += 4, inArray += 4 )
			fArray[3] = inArray[3];
		break;
	case ALPHAGEN_ONEMINUSVERTEX:
		for( i = 0; i < r_stats.numColors; i++, fArray += 4, inArray += 4 )
			fArray[3] = 1.0f - inArray[3];
		break;
	case ALPHAGEN_ENTITY:
		if( entityAlpha ) break;
		for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
			fArray[3] = Ref.m_pCurrentEntity->renderamt;
		break;
	case ALPHAGEN_ONEMINUSENTITY:
		if( entityAlpha ) break;
		for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
			fArray[3] = 1.0f - Ref.m_pCurrentEntity->renderamt;
		break;
	case ALPHAGEN_SPECULAR:
		VectorSubtract( Ref.vieworg, Ref.m_pCurrentEntity->origin, t );
		if( !Matrix3x3_Compare( Ref.m_pCurrentEntity->matrix, matrix3x3_identity ))
			Matrix3x3_Transform( Ref.m_pCurrentEntity->matrix, t, v );
		else VectorCopy( t, v );

		for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
		{
			VectorSubtract( v, vertexArray[i], t );
			c = VectorLength( t );
			a = DotProduct( t, normalArray[i] ) / max( 0.1f, c );
			a = pow( a, alphaGen->func.params[0] );
			fArray[3] = bound( 0.0f, a, 1.0f );
		}
		break;
	case ALPHAGEN_DOT:
		if( !Matrix3x3_Compare( Ref.m_pCurrentEntity->matrix, matrix3x3_identity ))
			Matrix3x3_Transform( Ref.m_pCurrentEntity->matrix, Ref.forward, v );
		else VectorCopy( Ref.forward, v );

		for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
		{
			a = DotProduct( v, inNormalArray[i] );
			if( a < 0 ) a = -a;
			fArray[3] = bound( alphaGen->func.params[0], a, alphaGen->func.params[1] );
		}
		break;
	case ALPHAGEN_ONEMINUSDOT:
		if( !Matrix3x3_Compare( Ref.m_pCurrentEntity->matrix, matrix3x3_identity ))
			Matrix3x3_Transform( Ref.m_pCurrentEntity->matrix, Ref.forward, v );
		else VectorCopy( Ref.forward, v );

		for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
		{
			a = DotProduct( v, inNormalArray[i] );
			if( a < 0 ) a = -a;
			a = 1.0f - a;
			fArray[3] = bound( alphaGen->func.params[0], a, alphaGen->func.params[1] );
		}
		break;
	default:	break;
	}

	if( r_colorFog )
	{
		float	dist, vdist;
		cplane_t	*fogPlane;
		vec3_t	viewtofog;
		float	fogNormal[3], vpnNormal[3];
		float	fogDist, vpnDist, fogShaderDistScale;
		int	blendsrc, blenddst;
		int	fogptype;
		bool	alphaFog;

		blendsrc = stage->flags & GLSTATE_SRCBLEND_MASK;
		blenddst = stage->flags & GLSTATE_DSTBLEND_MASK;
		if(( blendsrc != GLSTATE_SRCBLEND_SRC_ALPHA && blenddst != GLSTATE_DSTBLEND_SRC_ALPHA ) && ( blendsrc != GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA && blenddst != GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA ))
			alphaFog = false;
		else alphaFog = true;

		fogPlane = r_colorFog->visible;
		fogShaderDistScale = 1.0 / (r_colorFog->shader->fog_dist - r_colorFog->shader->fogClearDist);
		dist = Ref.fog_dist_to_eye[r_colorFog - r_worldBrushModel->fogs];

		if( Ref.m_pCurrentShader->flags & SHADER_SKY )
		{
			if( dist > 0 ) VectorScale( fogPlane->normal, -dist, viewtofog );
			else VectorClear( viewtofog );
		}
		else VectorCopy( Ref.m_pCurrentEntity->origin, viewtofog );

		vpnNormal[0] = DotProduct( Ref.m_pCurrentEntity->matrix[0], Ref.forward ) * fogShaderDistScale * Ref.m_pCurrentEntity->scale;
		vpnNormal[1] = DotProduct( Ref.m_pCurrentEntity->matrix[1], Ref.forward ) * fogShaderDistScale * Ref.m_pCurrentEntity->scale;
		vpnNormal[2] = DotProduct( Ref.m_pCurrentEntity->matrix[2], Ref.forward ) * fogShaderDistScale * Ref.m_pCurrentEntity->scale;
		vpnDist = (((Ref.vieworg[0]-viewtofog[0])*Ref.forward[0]+(Ref.vieworg[1]-viewtofog[1])*Ref.forward[1]+(Ref.vieworg[2]-viewtofog[2])*Ref.forward[2])+r_colorFog->shader->fogClearDist) * fogShaderDistScale;

		fArray = colorArray[0];

		if( dist < 0 )
		{	
			// camera is inside the fog
			for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
			{
				temp = DotProduct( vertexArray[i], vpnNormal ) - vpnDist;
				c = ( 1.0f - bound( 0.0f, temp, 1.0f )) * 0xFFFF;

				if( alphaFog )
				{
					fArray[3] = (int)(fArray[3] * c)>>16;
				}
				else
				{
					fArray[0] = (int)(fArray[0] * c)>>16;
					fArray[1] = (int)(fArray[1] * c)>>16;
					fArray[2] = (int)(fArray[2] * c)>>16;
				}
			}
		}
		else
		{
			fogNormal[0] = DotProduct( Ref.m_pCurrentEntity->matrix[0], fogPlane->normal ) * Ref.m_pCurrentEntity->scale;
			fogNormal[1] = DotProduct( Ref.m_pCurrentEntity->matrix[1], fogPlane->normal ) * Ref.m_pCurrentEntity->scale;
			fogNormal[2] = DotProduct( Ref.m_pCurrentEntity->matrix[2], fogPlane->normal ) * Ref.m_pCurrentEntity->scale;
			fogptype = ( fogNormal[0] == 1.0 ? PLANE_X : ( fogNormal[1] == 1.0 ? PLANE_Y : ( fogNormal[2] == 1.0 ? PLANE_Z : 3 )));
			fogDist = fogPlane->dist - DotProduct( viewtofog, fogPlane->normal );

			for( i = 0; i < r_stats.numColors; i++, fArray += 4 )
			{
				if( fogptype < 3 ) vdist = vertexArray[i][fogptype] - fogDist;
				else vdist = DotProduct( vertexArray[i], fogNormal ) - fogDist;

				if( vdist < 0 )
				{
					temp = ( DotProduct( vertexArray[i], vpnNormal ) - vpnDist ) * vdist / ( vdist - dist );
					c = ( 1.0f - bound( 0, temp, 1.0f )) * 0xFFFF;

					if( alphaFog )
					{
						fArray[3] = (int)(fArray[3] * c)>>16;
					}
					else
					{
						fArray[0] = (int)(fArray[0] * c)>>16;
						fArray[1] = (int)(fArray[1] * c)>>16;
						fArray[2] = (int)(fArray[2] * c)>>16;
					}
				}
			}
		}
	}
}

/*
=================
RB_CalcTextureCoords
=================
*/
static bool RB_CalcTextureCoords( const stageBundle_t *bundle, uint unit, matrix4x4 matrix )
{
	const tcGen_t	*tcGen = &bundle->tcGen;
	bool		identityMatrix = false;
	vec3_t		projection, transform;
	float		*outCoords;
	GLfloat		genVector[4][4];
	float		depth, *n;
	matrix4x4		m1, m2;
	int		i;

	Matrix4x4_LoadIdentity( matrix );

	switch( tcGen->type )
	{
	case TCGEN_BASE:
		RB_DisableTexGen();

		if( !GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
		{
			pglTexCoordPointer( 2, GL_FLOAT, 0, texCoordArray );
			return true;
		}
		break;
	case TCGEN_LIGHTMAP:
		RB_DisableTexGen();

		if( !GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
		{
			pglTexCoordPointer( 2, GL_FLOAT, 0, lmCoordArray[r_lightmapStyleNum[unit]] );
			return true;
		}
		break;
	case TCGEN_ENVIRONMENT:
		if( gl_state.orthogonal ) return true;

		if(!( Ref.params & RP_SHADOWMAPVIEW ))
		{
			VectorSubtract( Ref.vieworg, Ref.m_pCurrentEntity->origin, projection );
			Matrix3x3_Transform( Ref.m_pCurrentEntity->matrix, projection, transform );

			outCoords = tUnitCoordsArray[unit][0];
			for( i = 0, n = normalArray[0]; i < r_stats.numVertices; i++, outCoords += 2, n += 4 )
			{
				VectorSubtract( transform, vertexArray[i], projection );
				VectorNormalizeFast( projection );

				depth = DotProduct( n, projection );
				depth += depth;
				outCoords[0] = 0.5 + ( n[1] * depth - projection[1] ) * 0.5;
				outCoords[1] = 0.5 - ( n[2] * depth - projection[2] ) * 0.5;
			}
		}

		RB_DisableTexGen();

		if(!GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
		{
			pglTexCoordPointer( 2, GL_FLOAT, 0, tUnitCoordsArray[unit] );
			return true;
		}
		break;
	case TCGEN_VECTOR:
		for( i = 0; i < 3; i++ )
		{
			genVector[0][i] = tcGen->params[i];
			genVector[1][i] = tcGen->params[i+4];
		}
		genVector[0][3] = genVector[1][3] = 0;

// FIXME: HACK
#ifdef OPENGL_STYLE
		matrix[3][0] = tcGen->params[3];
		matrix[3][1] = tcGen->params[7];
#else
		matrix[0][3] = tcGen->params[3];
		matrix[1][3] = tcGen->params[7];
#endif
		GL_TexCoordMode( GL_FALSE );
		GL_EnableTexGen( GL_S, GL_OBJECT_LINEAR );
		GL_EnableTexGen( GL_T, GL_OBJECT_LINEAR );
		GL_EnableTexGen( GL_R, 0 );
		GL_EnableTexGen( GL_Q, 0 );
		pglTexGenfv( GL_S, GL_OBJECT_PLANE, genVector[0] );
		pglTexGenfv( GL_T, GL_OBJECT_PLANE, genVector[1] );
		return false;
	case TCGEN_WARP:
		outCoords = tUnitCoordsArray[unit][0];
		for( i = 0; i < r_stats.numVertices; i++ )
		{
			outCoords[0] = texCoordArray[i][0] + rb_warpSinTable[((int)((texCoordArray[i][1] * 8.0 + m_fShaderTime) * (256.0/M_PI2))) & 255] * (1.0/64);
			outCoords[1] = texCoordArray[i][1] + rb_warpSinTable[((int)((texCoordArray[i][0] * 8.0 + m_fShaderTime) * (256.0/M_PI2))) & 255] * (1.0/64);
		}

		RB_DisableTexGen();
		if( !GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
		{
			pglTexCoordPointer( 2, GL_FLOAT, 0, texCoordArray );
			return true;
		}
		break;
	case TCGEN_PROJECTION:
		GL_TexCoordMode( GL_FALSE );

		Matrix4x4_Copy( matrix, Ref.worldProjectionMatrix );
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
	case TCGEN_REFLECTION:
		GL_EnableTexGen( GL_S, GL_REFLECTION_MAP_ARB );
		GL_EnableTexGen( GL_T, GL_REFLECTION_MAP_ARB );
		GL_EnableTexGen( GL_R, GL_REFLECTION_MAP_ARB );
		GL_EnableTexGen( GL_Q, 0 );
		return true;
	case TCGEN_NORMAL:
		GL_EnableTexGen( GL_S, GL_NORMAL_MAP_ARB);
		GL_EnableTexGen( GL_T, GL_NORMAL_MAP_ARB);
		GL_EnableTexGen( GL_R, GL_NORMAL_MAP_ARB);
		GL_EnableTexGen( GL_Q, 0 );
		return true;
	case TCGEN_FOG:
	{
		int	fogPtype;
		cplane_t	*fogPlane;
		shader_t	*fogShader;
		vec3_t	viewtofog;
		float	fogNormal[3], vpnNormal[3];
		float	dist, vdist, fogDist, vpnDist;

		fogPlane = r_texFog->visible;
		fogShader = r_texFog->shader;

		matrix[0][0] = matrix[1][1] = 1.0f / ( fogShader->fog_dist - fogShader->fogClearDist );
// FIXME: HACK
#ifdef OPENGL_STYLE
		matrix[3][1] = 1.5f / (float)FOG_TEXTURE_HEIGHT;
#else
		matrix[1][3] = 1.5f / (float)FOG_TEXTURE_HEIGHT;
#endif
		// distance to fog
		dist = Ref.fog_dist_to_eye[r_texFog - r_worldBrushModel->fogs];

		if( Ref.m_pCurrentShader->flags & SHADER_SKY )
		{
			if( dist > 0 ) VectorMA( Ref.vieworg, -dist, fogPlane->normal, viewtofog );
			else VectorCopy( Ref.vieworg, viewtofog );
		}
		else VectorCopy( Ref.m_pCurrentEntity->origin, viewtofog );

		// some math tricks to take entity's rotation matrix into account
		// for fog texture coordinates calculations:
		// M is rotation matrix, v is vertex, t is transform vector
		// n is plane's normal, d is plane's dist, r is view origin
		// (M*v + t)*n - d = (M*n)*v - ((d - t*n))
		// (M*v + t - r)*n = (M*n)*v - ((r - t)*n)
		fogNormal[0] = DotProduct( Ref.m_pCurrentEntity->matrix[0], fogPlane->normal ) * Ref.m_pCurrentEntity->scale;
		fogNormal[1] = DotProduct( Ref.m_pCurrentEntity->matrix[1], fogPlane->normal ) * Ref.m_pCurrentEntity->scale;
		fogNormal[2] = DotProduct( Ref.m_pCurrentEntity->matrix[2], fogPlane->normal ) * Ref.m_pCurrentEntity->scale;
		fogPtype = ( fogNormal[0] == 1.0 ? PLANE_X : ( fogNormal[1] == 1.0 ? PLANE_Y : ( fogNormal[2] == 1.0 ? PLANE_Z : 3 )));
		fogDist = ( fogPlane->dist - DotProduct( viewtofog, fogPlane->normal ) );

		vpnNormal[0] = DotProduct(Ref.m_pCurrentEntity->matrix[0], Ref.forward) * Ref.m_pCurrentEntity->scale;
		vpnNormal[1] = DotProduct(Ref.m_pCurrentEntity->matrix[1], Ref.forward) * Ref.m_pCurrentEntity->scale;
		vpnNormal[2] = DotProduct(Ref.m_pCurrentEntity->matrix[2], Ref.forward) * Ref.m_pCurrentEntity->scale;
		vpnDist = (( Ref.vieworg[0] - viewtofog[0] ) * Ref.forward[0] + ( Ref.vieworg[1] - viewtofog[1] ) * Ref.forward[1] + ( Ref.vieworg[2] - viewtofog[2] ) * Ref.forward[2] ) + fogShader->fogClearDist;

		outCoords = tUnitCoordsArray[unit][0];
		if( dist < 0 )
		{
			// camera is inside the fog brush
			for( i = 0; i < r_stats.numVertices; i++, outCoords += 2 )
			{
				outCoords[0] = DotProduct( vertexArray[i], vpnNormal ) - vpnDist;
				if( fogPtype < 3 ) outCoords[1] = -( vertexArray[i][fogPtype] - fogDist );
				else outCoords[1] = -( DotProduct( vertexArray[i], fogNormal ) - fogDist );
			}
		}
		else
		{
			for( i = 0; i < r_stats.numVertices; i++, outCoords += 2 )
			{
				if( fogPtype < 3 ) vdist = vertexArray[i][fogPtype] - fogDist;
				else vdist = DotProduct( vertexArray[i], fogNormal ) - fogDist;
				outCoords[0] = (( vdist < 0 ) ? ( DotProduct( vertexArray[i], vpnNormal ) - vpnDist ) * vdist / ( vdist - dist ) : 0.0f );
				outCoords[1] = -vdist;
			}
		}

		RB_DisableTexGen();

		if( !GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
		{
			pglTexCoordPointer( 2, GL_FLOAT, 0, tUnitCoordsArray[unit] );
			return false;
		}
		break;
	}
	case TCGEN_SVECTORS:
		RB_DisableTexGen();

		if( !GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
		{
			pglTexCoordPointer( 4, GL_FLOAT, 0, sVectorArray );
			return true;
		}
		break;
	case TCGEN_PROJECTION_SHADOW:
		GL_TexCoordMode( GL_FALSE );
		RB_DisableTexGen();
		Matrix4x4_Concat( matrix, r_currentCastGroup->worldProjectionMatrix, Ref.entityMatrix );
		break;
	default:	break;
	}

	return identityMatrix;
}

/*
================
RB_SetupTCMods
================
*/
static void RB_SetupTCMods( const shaderStage_t *stage, const stageBundle_t *bundle, matrix4x4 result )
{
	int		i;
	const float	*table;
	double		t1, t2, sint, cost;
	matrix4x4		m1, m2;
	const tcMod_t	*tcMod;

	for( i = 0, tcMod = bundle->tcMod; i < bundle->tcModNum; i++, tcMod++ )
	{
		switch( tcMod->type )
		{
		case TCMOD_TRANSLATE:
			Matrix4x4_Translate2D( result, tcMod->params[0], tcMod->params[1] );
			break;
		case TCMOD_SCALE:
			Matrix4x4_Scale2D( result, tcMod->params[0], tcMod->params[1] );
			break;
		case TCMOD_ROTATE:
			cost = tcMod->params[0] * m_fShaderTime;
			sint = R_FastSin( cost );
			cost = R_FastSin( cost + 0.25 );
			m2[0][0] = m2[1][1] = cost;
// FIXME: HACK
#ifdef OPENGL_STYLE
			m2[0][1] = sint;
			m2[3][0] = 0.5f * ( sint - cost + 1 );
			m2[1][0] = -sint;
			m2[3][1] = -0.5f * ( sint + cost - 1 );
#else
			m2[1][0] = sint;
			m2[0][3] = 0.5f * ( sint - cost + 1 );
			m2[0][1] = -sint;
			m2[1][3] = -0.5f * ( sint + cost - 1 );
#endif
			Matrix4x4_Copy2D( m1, result );
			Matrix4x4_Concat2D( result, m2, m1 );
			break;
		case TCMOD_TURB:
			t1 = ( 1.0f / 4.0f );
			t2 = tcMod->params[2] + m_fShaderTime * tcMod->params[3];
			Matrix4x4_Scale2D( result, 1.0f+(tcMod->params[1]*R_FastSin(t2)+tcMod->params[0])*t1, 1+(tcMod->params[1]*R_FastSin(t2+0.25f)+tcMod->params[0])*t1 );
			break;
		case TCMOD_STRETCH:
			table = RB_TableForFunc( &tcMod->func );
			t2 = tcMod->params[3] + m_fShaderTime * tcMod->params[4];
			t1 = TABLE_EVALUATE( table, t2 ) * tcMod->params[2] + tcMod->params[1];
			t1 = t1 ? 1.0f / t1 : 1.0f;
			t2 = 0.5f - 0.5f * t1;
			Matrix4x4_Stretch2D( result, t1, t2 );
			break;
		case TCMOD_SCROLL:
			t1 = tcMod->params[0] * m_fShaderTime;
			t2 = tcMod->params[1] * m_fShaderTime;
			if( stage->progType != PROGRAM_DISTORTION )
			{	
				// HACKHACK
				t1 = t1 - floor( t1 );
				t2 = t2 - floor( t2 );
			}
			Matrix4x4_Translate2D( result, t1, t2 );
			break;
		case TCMOD_TRANSFORM:
			m2[0][0] = tcMod->params[0];
			m2[1][1] = tcMod->params[1];
// FIXME: HACK
#ifdef OPENGL_STYLE
			m2[0][1] = tcMod->params[2];
			m2[3][0] = tcMod->params[4];
			m2[1][0] = tcMod->params[3];
			m2[3][1] = tcMod->params[5];
#else
			m2[1][0] = tcMod->params[2];
			m2[0][3] = tcMod->params[4];
			m2[0][1] = tcMod->params[3];
			m2[1][3] = tcMod->params[5];
#endif
			Matrix4x4_Copy2D( m1, result );
			Matrix4x4_Concat2D( m2, m1, result );
			break;
		default:	break;
		}
	}
}

/*
=================
RB_SetupTextureCombiners
=================
*/
static void RB_SetupTextureCombiners( const stageBundle_t *bundle )
{
	const texEnvCombine_t	*texEnvCombine = &bundle->texEnvCombine;

	pglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, texEnvCombine->rgbCombine);
	pglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, texEnvCombine->rgbSource[0]);
	pglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, texEnvCombine->rgbSource[1]);
	pglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, texEnvCombine->rgbSource[2]);
	pglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, texEnvCombine->rgbOperand[0]);
	pglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, texEnvCombine->rgbOperand[1]);
	pglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, texEnvCombine->rgbOperand[2]);
	pglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, texEnvCombine->rgbScale);

	pglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, texEnvCombine->alphaCombine);
	pglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, texEnvCombine->alphaSource[0]);
	pglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, texEnvCombine->alphaSource[1]);
	pglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_ARB, texEnvCombine->alphaSource[2]);
	pglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, texEnvCombine->alphaOperand[0]);
	pglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, texEnvCombine->alphaOperand[1]);
	pglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA_ARB, texEnvCombine->alphaOperand[2]);
	pglTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, texEnvCombine->alphaScale);

	pglTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, texEnvCombine->constColor);
}

/*
=================
RB_SetShaderState
=================
*/
static void RB_SetShaderState( void )
{
	if( Ref.m_pCurrentShader->flags & SHADER_CULL )
	{
		GL_Enable( GL_CULL_FACE );
		GL_CullFace( Ref.m_pCurrentShader->cull.mode );
	}
	else GL_Disable( GL_CULL_FACE );

	if( Ref.m_pCurrentShader->flags & SHADER_POLYGONOFFSET )
	{
		GL_Enable( GL_POLYGON_OFFSET_FILL );
		GL_PolygonOffset( r_offsetfactor->value, r_offsetunits->value );
	}
	else GL_Disable( GL_POLYGON_OFFSET_FILL );

	if( Ref.m_pCurrentShader->flags & SHADER_FLARE )
		GL_Disable( GL_DEPTH_TEST );
}

/*
=================
RB_SetShaderStageState
=================
*/
static void RB_SetShaderStageState( const shaderStage_t *stage )
{
	if( stage->flags & SHADERSTAGE_ALPHAFUNC )
	{
		GL_Enable( GL_ALPHA_TEST );
		GL_AlphaFunc( stage->alphaFunc.func, stage->alphaFunc.ref );
	}
	else GL_Disable( GL_ALPHA_TEST );

	if( stage->flags & SHADERSTAGE_BLENDFUNC )
	{
		GL_Enable( GL_BLEND );
		GL_BlendFunc( stage->blendFunc.src, stage->blendFunc.dst );
	}
	else GL_Disable( GL_BLEND );

	if( stage->flags & SHADERSTAGE_DEPTHFUNC )
	{
		GL_Enable( GL_DEPTH_TEST );
		GL_DepthFunc( stage->depthFunc.func );
	}
	else GL_Disable( GL_DEPTH_TEST );

	if( stage->flags & SHADERSTAGE_DEPTHWRITE )
		GL_DepthMask( GL_TRUE );
	else GL_DepthMask( GL_FALSE );
}

/*
=================
RB_SetupTextureUnit
=================
*/
static void RB_SetupTextureUnit( const shaderStage_t *stage, const stageBundle_t *bundle, uint unit )
{
	texture_t	*image;
	matrix4x4	m1, m2, result;
	bool	identityMatrix;
	
	GL_SelectTexture( unit );

	switch( bundle->texType )
	{
	case TEX_GENERIC:
		if( bundle->numTextures == 1 ) image = bundle->textures[0];
		else image = bundle->textures[(int)(bundle->animFrequency * m_fShaderTime) % bundle->numTextures];
		break;
	case TEX_LIGHTMAP:
		image = r_lightmapTextures[r_superLightStyle->lmapNum[r_lightmapStyleNum[unit]]];
		break;
	case TEX_CINEMATIC:
		// FIXME: implement
		image = r_defaultTexture;
		//CIN_RunCinematic( bundle->cinematicHandle );
		//CIN_DrawCinematic( bundle->cinematicHandle );
		break;
	case TEX_PORTAL:
		image = r_portaltexture;
		break;
	case TEX_DLIGHT:
		image = r_dlightTexture;
		break;
	default: Host_Error( "RB_SetupTextureUnit: unknown texture type %i in shader '%s'\n", bundle->texType, Ref.m_pCurrentShader->name );
	}

	GL_BindTexture( image );

	if( unit < gl_config.textureunits )
	{
		if( unit && !stage->program )
			pglEnable( GL_TEXTURE_2D );
		if( bundle->flags & STAGEBUNDLE_CUBEMAP )
			GL_TexCoordMode( GL_TEXTURE_CUBE_MAP_ARB );
		else GL_TexCoordMode( GL_TEXTURE_COORD_ARRAY );

		GL_TexEnv( bundle->texEnv );

		if( bundle->flags & STAGEBUNDLE_TEXENVCOMBINE )
			RB_SetupTextureCombiners( bundle );
	}

	identityMatrix = RB_CalcTextureCoords( bundle, unit, result );

	if( bundle->tcModNum )
	{
		identityMatrix = false;
		RB_SetupTCMods( stage, bundle, result );
	}

	if( bundle->tcGen.type == TCGEN_REFLECTION || bundle->tcGen.type == TCGEN_NORMAL )
	{
		Matrix4x4_Transpose( Ref.modelViewMatrix, m1 );
		Matrix4x4_Copy( result, m2 );
		Matrix4x4_Concat( result, m2, m1 );
		GL_LoadTexMatrix( result );
		return;
	}

	if( !identityMatrix )
		GL_LoadTexMatrix( result );
	else GL_LoadIdentityTexMatrix();
}

/*
=================
RB_CleanupTextureUnit
=================
*/
static void RB_CleanupTextureUnit( const stageBundle_t *bundle, uint unit )
{
	GL_SelectTexture( unit );

	if( bundle->tcGen.type == TCGEN_REFLECTION || bundle->tcGen.type == TCGEN_NORMAL )
	{
		RB_DisableTexGen();
		GL_TexCoordMode( GL_NONE );
	}

	if( unit < gl_config.textureunits )
	{
		if( bundle->flags & STAGEBUNDLE_CUBEMAP )
			pglDisable( GL_TEXTURE_CUBE_MAP_ARB );
		else pglDisable( GL_TEXTURE_2D );
	}
}

/*
================
RB_ShaderStageBlendmode

stupid code!!!!!
================
*/
static int RB_ShaderStageBlendmode( const shaderStage_t *stage )
{
	//if( stage->flags & SHADERSTAGE_BLENDFUNC )
	{
		return GL_MODULATE;
	}
	//return 0;
}

/*
================
RB_RenderMeshGeneric

TEST ONLY, another modes probably not needed
================
*/
static void RB_RenderMeshGeneric( void )
{
	const shaderStage_t *stage = rb_accumStages[0];
	const stageBundle_t	*bundle;
	int	i, j;

	RB_SetShaderStageState( stage );
	RB_SetupTextureUnit( stage, stage->bundles[0], GL_TEXTURE0 );
	RB_CalcVertexColors( stage );

	//if( stage->flags & SHADERSTAGE_BLENDFUNC )
	//	GL_TexEnv( stage->blendFunc.func );
	GL_TexEnv( GL_MODULATE );
	GL_SetState( r_currentShaderState|( stage->flags & r_currentShaderPassMask ));

	/*for( i = 0; i < rb_numAccumStages; i++ )
	{
		stage = Ref.m_pCurrentShader->stages[i];

		RB_SetShaderStageState( stage );
		RB_CalcVertexColors( stage );

		for( j = 0; j < stage->numBundles; j++ )
		{
			bundle = stage->bundles[j];
			RB_SetupTextureUnit( stage, bundle, j );
		}
		for( j = stage->numBundles - 1; j >= 0; j-- )
		{
			bundle = stage->bundles[j];
			RB_CleanupTextureUnit( bundle, j );
		}
	}*/
	R_FlushArrays();
}

/*
================
RB_RenderAccumulatedStages
================
*/
static void RB_RenderAccumulatedStages( void )
{
	const shaderStage_t	*stage = rb_accumStages[0];

	RB_CleanUpTextureUnits( rb_numAccumStages );

	if( stage->program )
	{
		rb_numAccumStages = 0;
		return;
	}
	if( stage->flags & SHADERSTAGE_DLIGHT )
	{
		rb_numAccumStages = 0;
		R_AddDynamicLights( r_currentDlightBits, r_currentShaderState|(stage->flags & r_currentShaderPassMask));
		return;
	}
	if( stage->flags & SHADERSTAGE_STENCILSHADOW )
	{
		rb_numAccumStages = 0;
		// R_PlanarShadowPass( r_currentShaderState|( stage->flags & r_currentShaderPassMask ));
		return;
	}

	RB_RenderMeshGeneric();
	rb_numAccumStages = 0;
}

/*
================
RB_AccumulateStage
================
*/
static void RB_AccumulateStage( shaderStage_t *stage )
{
	bool accumulate, renderNow;
	const shaderStage_t	*prevStage;

	// for depth texture we render light's view to, ignore passes that do not write into depth buffer
	if(( Ref.params & RP_SHADOWMAPVIEW ) && !( stage->flags & SHADERSTAGE_DEPTHWRITE ))
		return;

	// see if there are any free texture units
	renderNow = ( stage->flags & (SHADERSTAGE_DLIGHT|SHADERSTAGE_STENCILSHADOW )) || stage->program;
	accumulate = ( rb_numAccumStages < gl_config.textureunits ) && !renderNow;

	if( accumulate )
	{
		if( !rb_numAccumStages )
		{
			rb_accumStages[rb_numAccumStages++] = stage;
			return;
		}

		// ok, we've got several passes, diff against the previous
		prevStage = rb_accumStages[rb_numAccumStages-1];

		// see if depthfuncs and colors are good
		if(
			((( prevStage->flags ^ stage->flags ) & SHADERSTAGE_DEPTHFUNC) && ( stage->depthFunc.func == GL_EQUAL )) ||
			(( prevStage->flags & SHADERSTAGE_ALPHAFUNC ) && !((stage->flags & SHADERSTAGE_DEPTHFUNC) && ( stage->depthFunc.func == GL_EQUAL ))) ||
			( stage->flags & SHADERSTAGE_ALPHAFUNC ) ||
			( stage->rgbGen.type != RGBGEN_IDENTITY ) ||
			( stage->alphaGen.type != ALPHAGEN_IDENTITY )
			)
			accumulate = false;

		// see if blendmodes are good
		if( accumulate )
		{
			int mode, prevMode;

			mode = RB_ShaderStageBlendmode( stage );
			if( mode )
			{
				prevMode = RB_ShaderStageBlendmode( prevStage );

				if( GL_Support( R_COMBINE_EXT ))
				{
					if( prevMode == GL_REPLACE )
						accumulate = (mode == GL_ADD) ? GL_Support( R_TEXTURE_ENV_ADD_EXT ) : true;
					else if( prevMode == GL_ADD )
						accumulate = (mode == GL_ADD) && GL_Support( R_TEXTURE_ENV_ADD_EXT );
					else if( prevMode == GL_MODULATE )
						accumulate = (mode == GL_MODULATE || mode == GL_REPLACE);
					else accumulate = false;
				}
				else // if( GL_Support( R_ARB_MULTITEXTURE ))
				{
					if( prevMode == GL_REPLACE )
						accumulate = (mode == GL_ADD) ? GL_Support( R_TEXTURE_ENV_ADD_EXT ) : (mode != GL_DECAL);
					else if( prevMode == GL_ADD )
						accumulate = (mode == GL_ADD) && GL_Support( R_TEXTURE_ENV_ADD_EXT );
					else if( prevMode == GL_MODULATE )
						accumulate = (mode == GL_MODULATE || mode == GL_REPLACE);
					else accumulate = false;
				}
			}
			else accumulate = false;
		}
	}

	// no, failed to accumulate
	if( !accumulate )
	{
		if( rb_numAccumStages )
			RB_RenderAccumulatedStages();
	}

	rb_accumStages[rb_numAccumStages++] = stage;
	if( renderNow ) RB_RenderAccumulatedStages();
}

/*
================
R_RenderMeshBuffer
================
*/
void R_RenderMeshBuffer( const meshbuffer_t *mb )
{
	int		i;
	msurface_t	*surf;
	shaderStage_t	*stage;
	mfog_t		*fog;

	if( !r_stats.numVertices || !r_stats.numIndices )
	{
		R_ClearArrays();
		return;
	}
          
	surf = mb->infoKey > 0 ? &r_worldBrushModel->surfaces[mb->infoKey - 1] : NULL;
	if( surf ) r_superLightStyle = &r_superLightStyles[surf->superLightStyle];
	else r_superLightStyle = NULL;
	m_pRenderMeshBuffer = mb;

	R_SHADER_FOR_KEY( mb->shaderKey, Ref.m_pCurrentShader );

	if( gl_state.orthogonal )
	{
		m_fShaderTime = Sys_DoubleTime();
	}
	else
	{
		m_fShaderTime = Ref.refdef.time;
		if( Ref.m_pCurrentEntity )
		{
			m_fShaderTime -= Ref.m_pCurrentEntity->shaderTime;
			if( m_fShaderTime < 0 ) m_fShaderTime = 0;
		}
	}

	if( !r_triangleOutlines ) RB_SetShaderState();

	if( Ref.m_pCurrentShader->deformVertexesNum )
		RB_DeformVertexes();

	if( r_features & MF_KEEPLOCK )
		r_stats.totalKeptLocks++;
	else R_UnlockArrays();

	if( r_triangleOutlines )
	{
		R_LockArrays( r_stats.numVertices );

		if( Ref.params & RP_TRISOUTLINES )
			RB_DrawTriangles();
		if( Ref.params & RP_SHOWNORMALS )
			RB_DrawNormals();
		if( Ref.params & RP_SHOWTANGENTS )
			RB_DrawTangentSpace();
		R_ClearArrays();
		return;
	}

	// extract the fog volume number from sortkey
	if( !r_worldModel ) fog = NULL;
	else R_FOG_FOR_KEY( mb->sortKey, fog );
	if( fog && !fog->shader ) fog = NULL;

	// can we fog the geometry with alpha texture?
	r_texFog = ( fog && (( Ref.m_pCurrentShader->sort <= SORT_ALPHATEST && ( Ref.m_pCurrentShader->flags & ( SHADER_DEPTHWRITE|SHADER_SKY ))) || Ref.m_pCurrentShader->fog_dist )) ? fog : NULL;

	// check if the fog volume is present but we can't use alpha texture
	r_colorFog = ( fog && !r_texFog ) ? fog : NULL;

	if( Ref.m_pCurrentShader->flags & SHADER_FLARE )
		r_currentDlightBits = 0;
	else r_currentDlightBits = surf ? mb->dlightbits : 0;

	r_currentShadowBits = mb->shadowbits & Ref.shadowBits;
	R_LockArrays( r_stats.numVertices );

	for( i = 0; i < Ref.m_pCurrentShader->numStages; i++ )
	{
		stage = Ref.m_pCurrentShader->stages[i];
		RB_AccumulateStage( stage );
          }

	// flush any remaining passes
	if( rb_numAccumStages )
		RB_RenderAccumulatedStages();

	R_ClearArrays();
	pglMatrixMode( GL_MODELVIEW );
}

static void RB_DrawLine( int color, int numpoints, const float *points )
{
	int	i = numpoints - 1;
	vec3_t	p0, p1;

	VectorSet( p0, points[i*3+0], points[i*3+1], points[i*3+2] );
	if( r_physbdebug->integer == 1 ) ConvertPositionToGame( p0 );

	for (i = 0; i < numpoints; i ++)
	{
		VectorSet( p1, points[i*3+0], points[i*3+1], points[i*3+2] );
		if( r_physbdebug->integer == 1 ) ConvertPositionToGame( p1 );
 
		pglColor4fv(UnpackRGBA( color ));
		pglVertex3fv( p0 );
		pglVertex3fv( p1 );
 
 		VectorCopy( p1, p0 );
 	}
}

void RB_DebugGraphics( void )
{
	if( r_refdef.rdflags & RDF_NOWORLDMODEL )
		return;

	if( r_physbdebug->integer )
	{
		// physic debug
		GL_LoadMatrix( Ref.worldMatrix );
		pglBegin( GL_LINES );
		ri.ShowCollision( RB_DrawLine );
		pglEnd();
	}
	if( r_showtextures->integer )
	{
		RB_ShowTextures();
	}
}


static vec4_t pic_points[4] =
{
{ 0, 0, 0, 1 },
{ 0, 0, 0, 1 },
{ 0, 0, 0, 1 },
{ 0, 0, 0, 1 }
};
static vec2_t pic_st[4];
static vec4_t pic_colors[4];
static rb_mesh_t pic_mesh = { 4, pic_points, pic_points, NULL, pic_st, { 0, 0, 0, 0 }, { pic_colors, pic_colors, pic_colors, pic_colors }, 6, NULL };
meshbuffer_t pic_mbuffer;

/*
===============
RB_DrawStretchPic
===============
*/
void RB_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, shader_t *shader )
{
	if( !shader ) return;

	// lower-left
	Vector2Set( pic_points[0], x, y );
	Vector2Set( pic_st[0], s1, t1 );
	Vector4Copy( gl_state.draw_color, pic_colors[0] ); 

	// lower-right
	Vector2Set( pic_points[1], x+w, y );
	Vector2Set( pic_st[1], s2, t1 );
	Vector4Copy( pic_colors[0], pic_colors[1] );

	// upper-right
	Vector2Set( pic_points[2], x+w, y+h );
	Vector2Set( pic_st[2], s2, t2 );
	Vector4Copy( pic_colors[0], pic_colors[2] );

	// upper-left
	Vector2Set( pic_points[3], x, y+h );
	Vector2Set( pic_st[3], s1, t2 );
	Vector4Copy( pic_colors[0], pic_colors[3] );

	if( pic_mbuffer.shaderKey != (int)shader->sortKey || -pic_mbuffer.infoKey-1+4 > MAX_ARRAY_VERTS )
	{
		if( pic_mbuffer.shaderKey )
		{
			pic_mbuffer.infoKey = -1;
			R_RenderMeshBuffer( &pic_mbuffer );
		}
	}

	pic_mbuffer.infoKey -= 4;
	pic_mbuffer.shaderKey = shader->sortKey;

	// upload video right before rendering
	// FIXME: implement
	// if( shader->flags & SHADER_VIDEOMAP ) R_UploadCinematicShader( shader );

	R_PushMesh( &pic_mesh, MF_TRIFAN|shader->features|( r_shownormals->integer ? MF_NORMALS : 0 ));
}

/*
=================
RB_VBOInfo_f
=================
*/
void RB_VBOInfo_f( void )
{
	if( !GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
	{
		Msg( "GL_ARB_vertex_buffer_object extension is disabled or not supported\n" );
		return;
	}

	Msg( "%i bytes in %i static buffers\n", rb_staticBytes, rb_staticCount );
	Msg( "%i bytes in %i stream buffers\n", rb_streamBytes, rb_streamCount );
}

/*
=================
RB_InitBackend
=================
*/
void RB_InitBackend( void )
{
	rb_numAccumStages = 0;
	r_arraysLocked = false;
	r_triangleOutlines = false;

	R_ClearArrays();

	RB_ResetPassMask();

	// build waveform tables
	RB_BuildTables();

	// Set default GL state
	GL_SetDefaultState();
}

/*
=================
RB_ShutdownBackend
=================
*/
void RB_ShutdownBackend( void )
{
}     