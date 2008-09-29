//=======================================================================
//			Copyright XashXT Group 2007 ©
//		     r_backend.c - render backend utilites
//=======================================================================

#include "r_local.h"
#include "byteorder.h"
#include "mathlib.h"
#include "matrixlib.h" 

#define TABLE_SIZE		1024
#define TABLE_MASK		1023

static float	rb_sinTable[TABLE_SIZE];
static float	rb_triangleTable[TABLE_SIZE];
static float	rb_squareTable[TABLE_SIZE];
static float	rb_sawtoothTable[TABLE_SIZE];
static float	rb_inverseSawtoothTable[TABLE_SIZE];
static float	rb_noiseTable[TABLE_SIZE];
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

vbo_t		rb_vbo;
int		m_iInfoKey;
float		m_fShaderTime;
mesh_t		*m_pRenderMesh;
shader_t		*m_pCurrentShader;
ref_entity_t	*m_pCurrentEntity;

vec4_t		colorArray[MAX_VERTICES];
vec3_t		texCoordArray[MAX_TEXTURE_UNITS][MAX_VERTICES];

// input arrays
uint		indexArray[MAX_INDICES * 4];
vec3_t		vertexArray[MAX_VERTICES * 2];
vec3_t		tangentArray[MAX_VERTICES];
vec3_t		binormalArray[MAX_VERTICES];
vec3_t		normalArray[MAX_VERTICES];
vec4_t		inColorArray[MAX_VERTICES];
vec4_t		inTexCoordArray[MAX_VERTICES];

int		numIndex;
int		numVertex;
static GLenum	rb_drawMode;
static GLboolean	rb_CheckFlush;
static GLint	rb_vertexState;

static void RB_SetVertex( float x, float y, float z )
{
	GLuint	oldIndex = numIndex;

	switch( rb_drawMode )
	{
	case GL_LINES:
		indexArray[numIndex++] = numVertex;
		if( rb_vertexState++ == 1 )
		{
			RB_SetVertex( x + 1, y + 1, z + 1 );
			rb_vertexState = 0;
			rb_CheckFlush = true; // Flush for long sequences of quads.
		}
		break;
	case GL_TRIANGLES:
		indexArray[numIndex++] = numVertex;
		if( rb_vertexState++ == 2 )
		{
			rb_vertexState = 0;
			rb_CheckFlush = true; // Flush for long sequences of triangles.
		}
		break;
	case GL_QUADS:
		if( rb_vertexState++ < 3 )
		{
			indexArray[numIndex++] = numVertex;
		}
		else
		{
			// we've already done triangle (0, 1, 2), now draw (2, 3, 0)
			indexArray[numIndex++] = numVertex - 1;
			indexArray[numIndex++] = numVertex;
			indexArray[numIndex++] = numVertex - 3;
			rb_vertexState = 0;
			rb_CheckFlush = true; // flush for long sequences of quads.
		}
		break;
	case GL_TRIANGLE_STRIP:
		if( numVertex + rb_vertexState > MAX_VERTICES )
		{
			// This is a strip that's too big for us to buffer.
			// (We can't just flush the buffer because we have to keep
			// track of the last two vertices.
			Host_Error( "RB_SetVertex: overflow: %i > MAX_VERTICES\n", numVertex + rb_vertexState );
		}
		if( rb_vertexState++ < 3 )
		{
			indexArray[numIndex++] = numVertex;
		}
		else
		{
			// flip triangles between clockwise and counter clockwise
			if( rb_vertexState & 1 )
			{
				// draw triangle [n-2 n-1 n]
				indexArray[numIndex++] = numVertex - 2;
				indexArray[numIndex++] = numVertex - 1;
				indexArray[numIndex++] = numVertex;
			}
			else
			{
				// draw triangle [n-1 n-2 n]
				indexArray[numIndex++] = numVertex - 1;
				indexArray[numIndex++] = numVertex - 2;
				indexArray[numIndex++] = numVertex;
			}
		}
		break;
	case GL_POLYGON:
	case GL_TRIANGLE_FAN:	// same as polygon
		if( numVertex + rb_vertexState > MAX_VERTICES )
		{
			// This is a polygon or fan that's too big for us to buffer.
			// (We can't just flush the buffer because we have to keep
			// track of the starting vertex.
			Host_Error( "RB_SetVertex: overflow: %i > MAX_VERTICES\n", numVertex + rb_vertexState );
		}
		if( rb_vertexState++ < 3 )
		{
			indexArray[numIndex++] = numVertex;
		}
		else
		{
			// draw triangle [0 n-1 n]
			indexArray[numIndex++] = numVertex - ( rb_vertexState - 1 );
			indexArray[numIndex++] = numVertex - 1;
			indexArray[numIndex++] = numVertex;
		}
		break;
	default:
		Host_Error( "RB_SetVertex: unsupported mode: %i\n", rb_drawMode );
		break;
	}

	// copy current vertex
	vertexArray[numVertex][0] = x;
	vertexArray[numVertex][1] = y;
	vertexArray[numVertex][2] = z;
	numVertex++;

	// flush buffer if needed
	if( rb_CheckFlush ) RB_CheckMeshOverflow( numIndex - oldIndex, rb_vertexState );
}

static void RB_SetTexCoord( GLfloat s, GLfloat t, GLfloat ls, GLfloat lt )
{
	inTexCoordArray[numVertex][0] = s;
	inTexCoordArray[numVertex][1] = t;
	inTexCoordArray[numVertex][2] = ls;
	inTexCoordArray[numVertex][3] = lt;
}

static void RB_SetColor( GLfloat r, GLfloat g, GLfloat b, GLfloat a )
{
	inColorArray[numVertex][0] = r;
	inColorArray[numVertex][1] = g;
	inColorArray[numVertex][2] = b;
	inColorArray[numVertex][3] = b;
}

static void RB_SetNormal( GLfloat x, GLfloat y, GLfloat z )
{
	normalArray[numVertex][0] = x;
	normalArray[numVertex][1] = y;
	normalArray[numVertex][2] = z;
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
	if( numIndex == 0 ) return;
	RB_CheckMeshOverflow( 0, 0 );
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
	RB_SetTexCoord( s, t, 0.0f, 0.0f );
}

void GL_TexCoord4f( GLfloat s, GLfloat t, GLfloat ls, GLfloat lt )
{
	RB_SetTexCoord( s, t, ls, lt );
}

void GL_TexCoord4fv( const GLfloat *v )
{
	RB_SetTexCoord( v[0], v[1], v[2], v[3] );
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
		rb_noiseTable[i] = Com_RandomFloat( -1, 1 );
	}
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
	Host_Error( "RB_TableForFunc: unknown waveform type %i in shader '%s'\n", func->type, m_pCurrentShader->name );
	return NULL;
}

/*
 =================
 RB_DeformVertexes
 =================
*/
static void RB_DeformVertexes( void )
{
	deformVerts_t	*deformVertexes = m_pCurrentShader->deformVertexes;
	uint		deformVertexesNum = m_pCurrentShader->deformVertexesNum;
	int		i, j;
	float		*table;
	float		now, f, t;

	for( i = 0; i < deformVertexesNum; i++, deformVertexes++ )
	{
		switch( deformVertexes->type )
		{
		case DEFORMVERTEXES_WAVE:
			table = RB_TableForFunc(&deformVertexes->func);
			now = deformVertexes->func.params[2] + deformVertexes->func.params[3] * m_fShaderTime;

			for( j = 0; j < numVertex; j++ )
			{
				t = (vertexArray[j][0] + vertexArray[j][1] + vertexArray[j][2]) * deformVertexes->params[0] + now;
				f = table[((int)(t * TABLE_SIZE)) & TABLE_MASK] * deformVertexes->func.params[1] + deformVertexes->func.params[0];
				VectorMA(vertexArray[j], f, normalArray[j], vertexArray[j]);
			}
			break;
		case DEFORMVERTEXES_MOVE:
			table = RB_TableForFunc(&deformVertexes->func);
			now = deformVertexes->func.params[2] + deformVertexes->func.params[3] * m_fShaderTime;
			f = table[((int)(now * TABLE_SIZE)) & TABLE_MASK] * deformVertexes->func.params[1] + deformVertexes->func.params[0];

			for (j = 0; j < numVertex; j++)
			{
				VectorMA(vertexArray[j], f, deformVertexes->params, vertexArray[j]);
			}
			break;
		case DEFORMVERTEXES_NORMAL:
			now = deformVertexes->params[1] * m_fShaderTime;

			for (j = 0; j < numVertex; j++)
			{
				f = normalArray[j][2] * now;
				normalArray[j][0] *= (deformVertexes->params[0] * sin(f));
				normalArray[j][1] *= (deformVertexes->params[0] * cos(f));
				VectorNormalizeFast(normalArray[j]);
			}
			break;
		default:
			Host_Error( "RB_DeformVertexes: unknown deformVertexes type %i in shader '%s'\n", deformVertexes->type, m_pCurrentShader->name);
		}
	}
}

/*
=================
RB_CalcVertexColors
=================
*/
static void RB_CalcVertexColors( shaderStage_t *stage )
{

	rgbGen_t		*rgbGen = &stage->rgbGen;
	alphaGen_t	*alphaGen = &stage->alphaGen;
	vec3_t		vec, dir;
	float		*table;
	float		now, f;
	byte		r, g, b, a;
	int		i;

	switch( rgbGen->type )
	{
	case RGBGEN_IDENTITY:
		for( i = 0; i < numVertex; i++ )
		{
			colorArray[i][0] = 1.0f;
			colorArray[i][1] = 1.0f;
			colorArray[i][2] = 1.0f;
		}
		break;
	case RGBGEN_IDENTITYLIGHTING:
		if( gl_config.deviceSupportsGamma )
			r = g = b = 1>>r_overbrightbits->integer;
		else r = g = b = 1.0f;

		for( i = 0; i < numVertex; i++ )
		{
			colorArray[i][0] = r;
			colorArray[i][1] = g;
			colorArray[i][2] = b;
		}
		break;
	case RGBGEN_WAVE:
		table = RB_TableForFunc(&rgbGen->func);
		now = rgbGen->func.params[2] + rgbGen->func.params[3] * m_fShaderTime;
		f = table[((int)(now * TABLE_SIZE)) & TABLE_MASK] * rgbGen->func.params[1] + rgbGen->func.params[0];

		f = bound( 0.0, f, 1.0 );

		r = 1.0f * f;
		g = 1.0f * f;
		b = 1.0f * f;

		for( i = 0; i < numVertex; i++ )
		{
			colorArray[i][0] = r;
			colorArray[i][1] = g;
			colorArray[i][2] = b;
		}
		break;
	case RGBGEN_COLORWAVE:
		table = RB_TableForFunc(&rgbGen->func);
		now = rgbGen->func.params[2] + rgbGen->func.params[3] * m_fShaderTime;
		f = table[((int)(now * TABLE_SIZE)) & TABLE_MASK] * rgbGen->func.params[1] + rgbGen->func.params[0];

		f = bound(0.0, f, 1.0);
			
		r = 1.0f * (rgbGen->params[0] * f);
		g = 1.0f * (rgbGen->params[1] * f);
		b = 1.0f * (rgbGen->params[2] * f);

		for( i = 0; i < numVertex; i++ )
		{
			colorArray[i][0] = r;
			colorArray[i][1] = g;
			colorArray[i][2] = b;
		}
		break;
	case RGBGEN_VERTEX:
		for( i = 0; i < numVertex; i++ )
		{
			colorArray[i][0] = inColorArray[i][0];
			colorArray[i][1] = inColorArray[i][1];
			colorArray[i][2] = inColorArray[i][2];
		}
		break;
	case RGBGEN_ONEMINUSVERTEX:
		for( i = 0; i < numVertex; i++ )
		{
			colorArray[i][0] = 1.0f - inColorArray[i][0];
			colorArray[i][1] = 1.0f - inColorArray[i][1];
			colorArray[i][2] = 1.0f - inColorArray[i][2];
		}
		break;
	case RGBGEN_ENTITY:
		for( i = 0; i < numVertex; i++ )
		{
			colorArray[i][0] = m_pCurrentEntity->rendercolor[0];
			colorArray[i][1] = m_pCurrentEntity->rendercolor[1];
			colorArray[i][2] = m_pCurrentEntity->rendercolor[2];
		}
		break;
	case RGBGEN_ONEMINUSENTITY:
		for( i = 0; i < numVertex; i++ )
		{
			colorArray[i][0] = 1.0f - m_pCurrentEntity->rendercolor[0];
			colorArray[i][1] = 1.0f - m_pCurrentEntity->rendercolor[1];
			colorArray[i][2] = 1.0f - m_pCurrentEntity->rendercolor[2];
		}
		break;
	case RGBGEN_LIGHTINGAMBIENT:
		R_LightingAmbient();
		break;
	case RGBGEN_LIGHTINGDIFFUSE:
		R_LightingDiffuse();
		break;
	case RGBGEN_CONST:
		r = 1.0f * rgbGen->params[0];
		g = 1.0f * rgbGen->params[1];
		b = 1.0f * rgbGen->params[2];

		for( i = 0; i < numVertex; i++ )
		{
			colorArray[i][0] = r;
			colorArray[i][1] = g;
			colorArray[i][2] = b;
		}
		break;
	default:
		Host_Error( "RB_CalcVertexColors: unknown rgbGen type %i in shader '%s'\n", rgbGen->type, m_pCurrentShader->name);
	}

	switch( alphaGen->type )
	{
	case ALPHAGEN_IDENTITY:
		for( i = 0; i < numVertex; i++ )
			colorArray[i][3] = 1.0f;
		break;
	case ALPHAGEN_WAVE:
		table = RB_TableForFunc(&alphaGen->func);
		now = alphaGen->func.params[2] + alphaGen->func.params[3] * m_fShaderTime;
		f = table[((int)(now * TABLE_SIZE)) & TABLE_MASK] * alphaGen->func.params[1] + alphaGen->func.params[0];
		f = bound( 0.0, f, 1.0 );
		a = 1.0f * f;

		for( i = 0; i < numVertex; i++ )
			colorArray[i][3] = a;
		break;
	case ALPHAGEN_ALPHAWAVE:
		table = RB_TableForFunc(&alphaGen->func);
		now = alphaGen->func.params[2] + alphaGen->func.params[3] * m_fShaderTime;
		f = table[((int)(now * TABLE_SIZE)) & TABLE_MASK] * alphaGen->func.params[1] + alphaGen->func.params[0];
		f = bound( 0.0, f, 1.0 );
		a = 1.0f * (alphaGen->params[0] * f);

		for( i = 0; i < numVertex; i++ )
			colorArray[i][3] = a;
		break;
	case ALPHAGEN_VERTEX:
		for( i = 0; i < numVertex; i++ )
			colorArray[i][3] = inColorArray[i][3];
		break;
	case ALPHAGEN_ONEMINUSVERTEX:
		for( i = 0; i < numVertex; i++ )
			colorArray[i][3] = 1.0f - inColorArray[i][3];
		break;
	case ALPHAGEN_ENTITY:
		for( i = 0; i < numVertex; i++ )
			colorArray[i][3] = m_pCurrentEntity->renderamt;
		break;
	case ALPHAGEN_ONEMINUSENTITY:
		for( i = 0; i < numVertex; i++ )
			colorArray[i][3] = 1.0f - m_pCurrentEntity->renderamt;
		break;
	case ALPHAGEN_DOT:
		if( !AxisCompare( m_pCurrentEntity->axis, axisDefault ))
			VectorRotate( r_forward, m_pCurrentEntity->axis, vec );
		else VectorCopy( r_forward, vec );

		for( i = 0; i < numVertex; i++ )
		{
			f = DotProduct(vec, normalArray[i]);
			if( f < 0 ) f = -f;
			colorArray[i][3] = 1.0f * bound( alphaGen->params[0], f, alphaGen->params[1] );
		}
		break;
	case ALPHAGEN_ONEMINUSDOT:
		if( !AxisCompare( m_pCurrentEntity->axis, axisDefault ))
			VectorRotate( r_forward, m_pCurrentEntity->axis, vec );
		else VectorCopy( r_forward, vec );

		for( i = 0; i < numVertex; i++ )
		{
			f = DotProduct(vec, normalArray[i]);
			if( f < 0 ) f = -f;
			colorArray[i][3] = 1.0f * bound( alphaGen->params[0], 1.0 - f, alphaGen->params[1]);
		}
		break;
	case ALPHAGEN_FADE:
		for( i = 0; i < numVertex; i++ )
		{
			VectorAdd( vertexArray[i], m_pCurrentEntity->origin, vec );
			f = VectorDistance( vec, r_refdef.vieworg );

			f = bound( alphaGen->params[0], f, alphaGen->params[1] ) - alphaGen->params[0];
			f = f * alphaGen->params[2];
			colorArray[i][3] = 1.0f * bound( 0.0, f, 1.0 );
		}
		break;
	case ALPHAGEN_ONEMINUSFADE:
		for( i = 0; i < numVertex; i++ )
		{
			VectorAdd( vertexArray[i], m_pCurrentEntity->origin, vec );
			f = VectorDistance( vec, r_refdef.vieworg );

			f = bound( alphaGen->params[0], f, alphaGen->params[1] ) - alphaGen->params[0];
			f = f * alphaGen->params[2];
			colorArray[i][3] = 1.0f * bound( 0.0, 1.0 - f, 1.0 );
		}
		break;
	case ALPHAGEN_LIGHTINGSPECULAR:
		if( !AxisCompare( m_pCurrentEntity->axis, axisDefault ))
		{
			VectorSubtract( r_origin, m_pCurrentEntity->origin, dir );
			VectorRotate( dir, m_pCurrentEntity->axis, vec );
		}
		else VectorSubtract( r_origin, m_pCurrentEntity->origin, vec );

		for( i = 0; i < numVertex; i++ )
		{
			VectorSubtract( vec, vertexArray[i], dir );
			VectorNormalizeFast( dir );

			f = DotProduct( dir, normalArray[i] );
			f = pow( f, alphaGen->params[0] );
			colorArray[i][3] = 1.0f * bound( 0.0, f, 1.0 );
		}
		break;
	case ALPHAGEN_CONST:
		a = 1.0f * alphaGen->params[0];

		for( i = 0; i < numVertex; i++ )
			colorArray[i][3] = a;
		break;
	default: Host_Error( "RB_CalcVertexColors: unknown alphaGen type %i in shader '%s'\n", alphaGen->type, m_pCurrentShader->name);
	}
}

/*
=================
RB_CalcTextureCoords
=================
*/
static void RB_CalcTextureCoords( stageBundle_t *bundle, uint unit )
{
	tcGen_t		*tcGen = &bundle->tcGen;
	tcMod_t		*tcMod = bundle->tcMod;
	uint		tcModNum = bundle->tcModNum;
	int		i, j;
	vec3_t		vec, dir;
	vec3_t		lightVector, eyeVector, halfAngle;
	float		*table;
	float		now, f, t;
	float		rad, s, c;
	vec2_t		st;

	switch( tcGen->type )
	{
	case TCGEN_BASE:
		for( i = 0; i < numVertex; i++ )
		{
			texCoordArray[unit][i][0] = inTexCoordArray[i][0];
			texCoordArray[unit][i][1] = inTexCoordArray[i][1];
		}
		break;
	case TCGEN_LIGHTMAP:
		for( i = 0; i < numVertex; i++ )
		{
			texCoordArray[unit][i][0] = inTexCoordArray[i][2];
			texCoordArray[unit][i][1] = inTexCoordArray[i][3];
		}
		break;
	case TCGEN_ENVIRONMENT:
		if (!AxisCompare( m_pCurrentEntity->axis, axisDefault ))
		{
			VectorSubtract( r_origin, m_pCurrentEntity->origin, dir );
			VectorRotate( dir, m_pCurrentEntity->axis, vec );
		}
		else VectorSubtract( r_origin, m_pCurrentEntity->origin, vec );

		for( i = 0; i < numVertex; i++ )
		{
			VectorSubtract( vec, vertexArray[i], dir );
			VectorNormalizeFast( dir );

			f = 2.0 * DotProduct( dir, normalArray[i] );
			texCoordArray[unit][i][0] = dir[0] - normalArray[i][0] * f;
			texCoordArray[unit][i][1] = dir[1] - normalArray[i][1] * f;
		}
		break;
	case TCGEN_VECTOR:
		for( i = 0; i < numVertex; i++ )
		{
			texCoordArray[unit][i][0] = DotProduct(vertexArray[i], &tcGen->params[0]);
			texCoordArray[unit][i][1] = DotProduct(vertexArray[i], &tcGen->params[3]);
		}
		break;
	case TCGEN_WARP:
		for( i = 0; i < numVertex; i++ )
		{
			texCoordArray[unit][i][0] = inTexCoordArray[i][0] + rb_warpSinTable[((int)((inTexCoordArray[i][1] * 8.0 + m_fShaderTime) * (256.0/M_PI2))) & 255] * (1.0/64);
			texCoordArray[unit][i][1] = inTexCoordArray[i][1] + rb_warpSinTable[((int)((inTexCoordArray[i][0] * 8.0 + m_fShaderTime) * (256.0/M_PI2))) & 255] * (1.0/64);
		}
		break;
	case TCGEN_LIGHTVECTOR:
		if( m_pCurrentEntity == r_worldEntity )
		{
			for( i = 0; i < numVertex; i++ )
			{
				R_LightDir( vertexArray[i], lightVector );

				texCoordArray[unit][i][0] = DotProduct( lightVector, tangentArray[i] );
				texCoordArray[unit][i][1] = DotProduct( lightVector, binormalArray[i] );
				texCoordArray[unit][i][2] = DotProduct( lightVector, normalArray[i] );
			}
		}
		else
		{
			R_LightDir( m_pCurrentEntity->origin, dir );
			if( !AxisCompare( m_pCurrentEntity->axis, axisDefault ))
				VectorRotate( dir, m_pCurrentEntity->axis, lightVector );
			else VectorCopy( dir, lightVector );

			for( i = 0; i < numVertex; i++ )
			{
				texCoordArray[unit][i][0] = DotProduct(lightVector, tangentArray[i] );
				texCoordArray[unit][i][1] = DotProduct(lightVector, binormalArray[i] );
				texCoordArray[unit][i][2] = DotProduct(lightVector, normalArray[i] );
			}
		}
		break;
	case TCGEN_HALFANGLE:
		if( m_pCurrentEntity == r_worldEntity )
		{
			for( i = 0; i < numVertex; i++ )
			{
				R_LightDir( vertexArray[i], lightVector );

				VectorSubtract( r_refdef.vieworg, vertexArray[i], eyeVector );
				VectorNormalizeFast( lightVector );
				VectorNormalizeFast( eyeVector );
				VectorAdd( lightVector, eyeVector, halfAngle );

				texCoordArray[unit][i][0] = DotProduct( halfAngle, tangentArray[i] );
				texCoordArray[unit][i][1] = DotProduct( halfAngle, binormalArray[i] );
				texCoordArray[unit][i][2] = DotProduct( halfAngle, normalArray[i] );
			}
		}
		else
		{
			R_LightDir( m_pCurrentEntity->origin, dir );

			if( !AxisCompare( m_pCurrentEntity->axis, axisDefault ))
			{
				VectorRotate( dir, m_pCurrentEntity->axis, lightVector );

				VectorSubtract( r_origin, m_pCurrentEntity->origin, dir );
				VectorRotate( dir, m_pCurrentEntity->axis, eyeVector );
			}
			else
			{
				VectorCopy( dir, lightVector );
				VectorSubtract( r_refdef.vieworg, m_pCurrentEntity->origin, eyeVector );
			}

			VectorNormalizeFast( lightVector );
			VectorNormalizeFast( eyeVector );

			VectorAdd( lightVector, eyeVector, halfAngle );

			for( i = 0; i < numVertex; i++ )
			{
				texCoordArray[unit][i][0] = DotProduct(halfAngle, tangentArray[i]);
				texCoordArray[unit][i][1] = DotProduct(halfAngle, binormalArray[i]);
				texCoordArray[unit][i][2] = DotProduct(halfAngle, normalArray[i]);
			}
		}
		break;
	case TCGEN_REFLECTION:
		pglTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB);
		pglTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB);
		pglTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB);

		break;
	case TCGEN_NORMAL:
		pglTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_NORMAL_MAP_ARB);
		pglTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_NORMAL_MAP_ARB);
		pglTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_NORMAL_MAP_ARB);

		break;
	default: Host_Error( "RB_CalcTextureCoords: unknown tcGen type %i in shader '%s'\n", tcGen->type, m_pCurrentShader->name);
	}

	for( i = 0; i < tcModNum; i++, tcMod++ )
	{
		switch( tcMod->type )
		{
		case TCMOD_TRANSLATE:
			for( j = 0; j < numVertex; j++ )
			{
				texCoordArray[unit][j][0] += tcMod->params[0];
				texCoordArray[unit][j][1] += tcMod->params[1];
			}
			break;
		case TCMOD_SCALE:
			for( j = 0; j < numVertex; j++ )
			{
				texCoordArray[unit][j][0] *= tcMod->params[0];
				texCoordArray[unit][j][1] *= tcMod->params[1];
			}
			break;
		case TCMOD_SCROLL:
			st[0] = tcMod->params[0] * m_fShaderTime;
			st[0] -= floor(st[0]);
			st[1] = tcMod->params[1] * m_fShaderTime;
			st[1] -= floor(st[1]);

			for( j = 0; j < numVertex; j++ )
			{
				texCoordArray[unit][j][0] += st[0];
				texCoordArray[unit][j][1] += st[1];
			}
			break;
		case TCMOD_ROTATE:
			rad = -DEG2RAD( tcMod->params[0] * m_fShaderTime );
			s = sin( rad );
			c = cos( rad );

			for( j = 0; j < numVertex; j++ )
			{
				st[0] = texCoordArray[unit][j][0];
				st[1] = texCoordArray[unit][j][1];
				texCoordArray[unit][j][0] = c * (st[0] - 0.5) - s * (st[1] - 0.5) + 0.5;
				texCoordArray[unit][j][1] = c * (st[1] - 0.5) + s * (st[0] - 0.5) + 0.5;
			}
			break;
		case TCMOD_STRETCH:
			table = RB_TableForFunc(&tcMod->func);
			now = tcMod->func.params[2] + tcMod->func.params[3] * m_fShaderTime;
			f = table[((int)(now * TABLE_SIZE)) & TABLE_MASK] * tcMod->func.params[1] + tcMod->func.params[0];
			
			f = (f) ? 1.0 / f : 1.0;
			t = 0.5 - 0.5 * f;

			for( j = 0; j < numVertex; j++ )
			{
				texCoordArray[unit][j][0] = texCoordArray[unit][j][0] * f + t;
				texCoordArray[unit][j][1] = texCoordArray[unit][j][1] * f + t;
			}
			break;
		case TCMOD_TURB:
			table = RB_TableForFunc(&tcMod->func);
			now = tcMod->func.params[2] + tcMod->func.params[3] * m_fShaderTime;

			for( j = 0; j < numVertex; j++ )
			{
				texCoordArray[unit][j][0] += (table[((int)(((vertexArray[j][0] + vertexArray[j][2]) * 1.0/128 * 0.125 + now) * TABLE_SIZE)) & TABLE_MASK] * tcMod->func.params[1] + tcMod->func.params[0]);
				texCoordArray[unit][j][1] += (table[((int)(((vertexArray[j][1]) * 1.0/128 * 0.125 + now) * TABLE_SIZE)) & TABLE_MASK] * tcMod->func.params[1] + tcMod->func.params[0]);
			}
			break;
		case TCMOD_TRANSFORM:
			for( j = 0; j < numVertex; j++ )
			{
				st[0] = texCoordArray[unit][j][0];
				st[1] = texCoordArray[unit][j][1];
				texCoordArray[unit][j][0] = st[0] * tcMod->params[0] + st[1] * tcMod->params[2] + tcMod->params[4];
				texCoordArray[unit][j][1] = st[1] * tcMod->params[1] + st[0] * tcMod->params[3] + tcMod->params[5];
			}
			break;
		default: Host_Error( "RB_CalcTextureCoords: unknown tcMod type %i in shader '%s'\n", tcMod->type, m_pCurrentShader->name);
		}
	}
}

/*
=================
RB_SetupVertexProgram
=================
*/
static void RB_SetupVertexProgram( shaderStage_t *stage )
{
	program_t	*program = stage->vertexProgram;

	pglBindProgramARB( GL_VERTEX_PROGRAM_ARB, program->progNum );
	pglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 0, r_origin[0], r_origin[1], r_origin[2], 0);
	pglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 1, r_forward[0], r_forward[1], r_forward[2], 0 );
	pglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 2, r_right[0], r_right[1], r_right[2], 0 );
	pglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 3, r_up[0], r_up[1], r_up[2], 0 );
	pglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 4, m_pCurrentEntity->origin[0], m_pCurrentEntity->origin[1], m_pCurrentEntity->origin[2], 0 );
	pglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 5, m_pCurrentEntity->axis[0][0], m_pCurrentEntity->axis[0][1], m_pCurrentEntity->axis[0][2], 0 );
	pglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 6, m_pCurrentEntity->axis[1][0], m_pCurrentEntity->axis[1][1], m_pCurrentEntity->axis[1][2], 0 );
	pglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 7, m_pCurrentEntity->axis[2][0], m_pCurrentEntity->axis[2][1], m_pCurrentEntity->axis[2][2], 0 );
	pglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 8, m_fShaderTime, 0, 0, 0 );
}

/*
 =================
 RB_SetupFragmentProgram
 =================
*/
static void RB_SetupFragmentProgram( shaderStage_t *stage )
{
	program_t	*program = stage->fragmentProgram;

	pglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, program->progNum );
	pglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, r_origin[0], r_origin[1], r_origin[2], 0);
	pglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 1, r_forward[0], r_forward[1], r_forward[2], 0 );
	pglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 2, r_right[0], r_right[1], r_right[2], 0 );
	pglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 3, r_up[0], r_up[1], r_up[2], 0 );
	pglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 4, m_pCurrentEntity->origin[0], m_pCurrentEntity->origin[1], m_pCurrentEntity->origin[2], 0 );
	pglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 5, m_pCurrentEntity->axis[0][0], m_pCurrentEntity->axis[0][1], m_pCurrentEntity->axis[0][2], 0 );
	pglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 6, m_pCurrentEntity->axis[1][0], m_pCurrentEntity->axis[1][1], m_pCurrentEntity->axis[1][2], 0 );
	pglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 7, m_pCurrentEntity->axis[2][0], m_pCurrentEntity->axis[2][1], m_pCurrentEntity->axis[2][2], 0 );
	pglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 8, m_fShaderTime, 0, 0, 0 );
}

/*
=================
RB_SetupTextureCombiners
=================
*/
static void RB_SetupTextureCombiners (stageBundle_t *bundle)
{
	texEnvCombine_t	*texEnvCombine = &bundle->texEnvCombine;

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
	if( m_pCurrentShader->flags & SHADER_CULL )
	{
		GL_Enable( GL_CULL_FACE );
		GL_CullFace( m_pCurrentShader->cull.mode );
	}
	else GL_Disable( GL_CULL_FACE );

	if( m_pCurrentShader->flags & SHADER_POLYGONOFFSET )
	{
		GL_Enable( GL_POLYGON_OFFSET_FILL );
		GL_PolygonOffset( r_offsetfactor->value, r_offsetunits->value );
	}
	else GL_Disable( GL_POLYGON_OFFSET_FILL );
}

/*
=================
RB_SetShaderStageState
=================
*/
static void RB_SetShaderStageState( shaderStage_t *stage )
{
	if( stage->flags & SHADERSTAGE_VERTEXPROGRAM )
	{
		GL_Enable( GL_VERTEX_PROGRAM_ARB );
		RB_SetupVertexProgram( stage );
	}
	else GL_Disable( GL_VERTEX_PROGRAM_ARB );

	if( stage->flags & SHADERSTAGE_FRAGMENTPROGRAM )
	{
		GL_Enable( GL_FRAGMENT_PROGRAM_ARB );
		RB_SetupFragmentProgram( stage );
	}
	else GL_Disable( GL_FRAGMENT_PROGRAM_ARB );

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
static void RB_SetupTextureUnit( stageBundle_t *bundle, uint unit )
{
	GL_SelectTexture( unit );

	switch( bundle->texType )
	{
	case TEX_GENERIC:
		if( bundle->numTextures == 1 )
			GL_BindTexture( bundle->textures[0] );
		else GL_BindTexture( bundle->textures[(int)(bundle->animFrequency * m_fShaderTime) % bundle->numTextures] );
		break;
	case TEX_LIGHTMAP:
		if( m_iInfoKey != 255 )
		{
			GL_BindTexture( r_worldModel->lightMaps[m_iInfoKey] );
			break;
		}
		R_UpdateSurfaceLightmap( m_pRenderMesh->mesh );
		break;
	case TEX_CINEMATIC:
		// not implemented
		//CIN_RunCinematic( bundle->cinematicHandle );
		//CIN_DrawCinematic( bundle->cinematicHandle );
		break;
	default: Host_Error( "RB_SetupTextureUnit: unknown texture type %i in shader '%s'\n", bundle->texType, m_pCurrentShader->name );
	}

	if( unit < gl_config.textureunits )
	{
		if( bundle->flags & STAGEBUNDLE_CUBEMAP )
			pglEnable( GL_TEXTURE_CUBE_MAP_ARB );
		else pglEnable( GL_TEXTURE_2D );

		GL_TexEnv( bundle->texEnv );

		if( bundle->flags & STAGEBUNDLE_TEXENVCOMBINE )
			RB_SetupTextureCombiners(bundle);
	}

	if( bundle->tcGen.type == TCGEN_REFLECTION || bundle->tcGen.type == TCGEN_NORMAL )
	{
		pglMatrixMode( GL_TEXTURE );
		pglLoadMatrixf( gl_textureMatrix );
		pglMatrixMode( GL_MODELVIEW );

		pglEnable( GL_TEXTURE_GEN_S );
		pglEnable( GL_TEXTURE_GEN_T );
		pglEnable( GL_TEXTURE_GEN_R );
	}
}

/*
=================
RB_CleanupTextureUnit
=================
*/
static void RB_CleanupTextureUnit( stageBundle_t *bundle, uint unit )
{
	GL_SelectTexture( unit );

	if( bundle->tcGen.type == TCGEN_REFLECTION || bundle->tcGen.type == TCGEN_NORMAL )
	{
		pglDisable( GL_TEXTURE_GEN_S );
		pglDisable( GL_TEXTURE_GEN_T );
		pglDisable( GL_TEXTURE_GEN_R );

		pglMatrixMode( GL_TEXTURE );
		pglLoadIdentity();
		pglMatrixMode( GL_MODELVIEW );
	}

	if( unit < gl_config.textureunits )
	{
		if( bundle->flags & STAGEBUNDLE_CUBEMAP )
			pglDisable( GL_TEXTURE_CUBE_MAP_ARB );
		else pglDisable( GL_TEXTURE_2D );
	}
}

/*
=================
RB_RenderShaderARB
=================
*/
static void RB_RenderShaderARB( void )
{
	shaderStage_t	*stage;
	stageBundle_t	*bundle;
	int		i, j;

	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, rb_vbo.indexBuffer );
	pglBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, numIndex * sizeof(uint), indexArray, GL_STREAM_DRAW_ARB );

	RB_SetShaderState();
	RB_DeformVertexes();

	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, rb_vbo.vertexBuffer );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVertex * sizeof(vec3_t), vertexArray, GL_STREAM_DRAW_ARB );
	pglEnableClientState( GL_VERTEX_ARRAY );
	pglVertexPointer( 3, GL_FLOAT, 0, VBO_OFFSET(0));

	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, rb_vbo.normalBuffer );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVertex * sizeof(vec3_t), normalArray, GL_STREAM_DRAW_ARB );
	pglEnableClientState( GL_NORMAL_ARRAY );
	pglNormalPointer( GL_FLOAT, 0, VBO_OFFSET(0));

	for( i = 0; i < m_pCurrentShader->numStages; i++ )
	{
		stage = m_pCurrentShader->stages[i];

		RB_SetShaderStageState( stage );
		RB_CalcVertexColors( stage );

		pglBindBufferARB( GL_ARRAY_BUFFER_ARB, rb_vbo.colorBuffer );
		pglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVertex * sizeof(vec4_t), colorArray, GL_STREAM_DRAW_ARB );
		pglEnableClientState( GL_COLOR_ARRAY );
		pglColorPointer( 4, GL_FLOAT, 0, VBO_OFFSET(0));

		for( j = 0; j < stage->numBundles; j++ )
		{
			bundle = stage->bundles[j];

			RB_SetupTextureUnit( bundle, j );
			RB_CalcTextureCoords( bundle, j );

			pglBindBufferARB( GL_ARRAY_BUFFER_ARB, rb_vbo.texCoordBuffer[j] );
			pglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVertex * sizeof(vec3_t), texCoordArray[j], GL_STREAM_DRAW_ARB );
			pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			pglTexCoordPointer( 3, GL_FLOAT, 0, VBO_OFFSET(0));
		}

		if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
			pglDrawRangeElementsEXT( GL_TRIANGLES, 0, numVertex, numIndex, GL_UNSIGNED_INT, VBO_OFFSET(0));
		else pglDrawElements( GL_TRIANGLES, numIndex, GL_UNSIGNED_INT, VBO_OFFSET(0));

		for( j = stage->numBundles - 1; j >= 0; j-- )
		{
			bundle = stage->bundles[j];

			RB_CleanupTextureUnit( bundle, j );
			pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		}
	}
}

/*
=================
RB_RenderShader
=================
*/
static void RB_RenderShader( void )
{
	shaderStage_t	*stage;
	stageBundle_t	*bundle;
	int		i, j;

	RB_SetShaderState();
	RB_DeformVertexes();

	pglEnableClientState( GL_VERTEX_ARRAY );
	pglVertexPointer( 3, GL_FLOAT, 0, vertexArray );

	pglEnableClientState( GL_NORMAL_ARRAY );
	pglNormalPointer( GL_FLOAT, 0, normalArray );

	if( GL_Support( R_CUSTOM_VERTEX_ARRAY_EXT ))
	{
		if( m_pCurrentShader->numStages != 1 )
		{
			pglDisableClientState( GL_COLOR_ARRAY );
			pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
			pglLockArraysEXT( 0, numVertex );
		}
	}

	for( i = 0; i < m_pCurrentShader->numStages; i++ )
	{
		stage = m_pCurrentShader->stages[i];

		RB_SetShaderStageState( stage );
		RB_CalcVertexColors( stage );

		pglEnableClientState( GL_COLOR_ARRAY );
		pglColorPointer( 4, GL_FLOAT, 0, colorArray );

		for( j = 0; j < stage->numBundles; j++ )
		{
			bundle = stage->bundles[j];

			RB_SetupTextureUnit( bundle, j );
			RB_CalcTextureCoords( bundle, j );

			pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			pglTexCoordPointer( 3, GL_FLOAT, 0, texCoordArray[j] );
		}

		if(GL_Support( R_CUSTOM_VERTEX_ARRAY_EXT ))
		{
			if( m_pCurrentShader->numStages == 1 )
				pglLockArraysEXT( 0, numVertex );
		}

		if(GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
			pglDrawRangeElementsEXT( GL_TRIANGLES, 0, numVertex, numIndex, GL_UNSIGNED_INT, indexArray );
		else pglDrawElements( GL_TRIANGLES, numIndex, GL_UNSIGNED_INT, indexArray );

		for( j = stage->numBundles - 1; j >= 0; j-- )
		{
			bundle = stage->bundles[j];

			RB_CleanupTextureUnit( bundle, j );
			pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		}
	}

	if( GL_Support( R_CUSTOM_VERTEX_ARRAY_EXT )) pglUnlockArraysEXT();
}

/*
=================
RB_DrawTris
=================
*/
static void RB_DrawTris( void )
{
	pglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	pglDisableClientState( GL_NORMAL_ARRAY );
	pglDisableClientState( GL_COLOR_ARRAY );
	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	if(GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
	{
		if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
			pglDrawRangeElementsEXT( GL_TRIANGLES, 0, numVertex, numIndex, GL_UNSIGNED_INT, VBO_OFFSET(0));
		else pglDrawElements( GL_TRIANGLES, numIndex, GL_UNSIGNED_INT, VBO_OFFSET(0));
	}
	else {
		if( GL_Support( R_CUSTOM_VERTEX_ARRAY_EXT ))
			pglLockArraysEXT(0, numVertex);

		if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
			pglDrawRangeElementsEXT( GL_TRIANGLES, 0, numVertex, numIndex, GL_UNSIGNED_INT, indexArray );
		else pglDrawElements( GL_TRIANGLES, numIndex, GL_UNSIGNED_INT, indexArray );

		if( GL_Support( R_CUSTOM_VERTEX_ARRAY_EXT ))
			pglUnlockArraysEXT();
	}

	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
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

	if( m_pRenderMesh->meshType == MESH_POLY )
		return;

	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	pglBegin( GL_LINES );
	for( i = 0; i < numVertex; i++ )
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
	int	i;
	vec3_t	v;

	if( m_pRenderMesh->meshType != MESH_SURFACE && m_pRenderMesh->meshType != MESH_STUDIO )
		return;

	pglColor4f( 1.0f, 0.0f, 0.0f, 1.0f );

	pglBegin( GL_LINES );
	for( i = 0; i < numVertex; i++ )
	{
		VectorAdd( vertexArray[i], tangentArray[i], v );
		pglVertex3fv( vertexArray[i] );
		pglVertex3fv( v );
	}
	pglEnd();

	pglColor4f( 0.0f, 1.0f, 0.0f, 1.0f );

	pglBegin( GL_LINES );
	for( i = 0; i < numVertex; i++ )
	{
		VectorAdd( vertexArray[i], binormalArray[i], v );
		pglVertex3fv( vertexArray[i] );
		pglVertex3fv( v );
	}
	pglEnd();

	pglColor4f( 0.0f, 0.0f, 1.0f, 1.0f );

	pglBegin( GL_LINES );
	for( i = 0; i < numVertex; i++ )
	{
		VectorAdd( vertexArray[i], normalArray[i], v );
		pglVertex3fv( vertexArray[i] );
		pglVertex3fv( v );
	}
	pglEnd();
}

/*
=================
RB_DrawModelBounds
=================
*/
static void RB_DrawModelBounds( void )
{
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
		GL_LoadMatrix( r_worldMatrix );
		pglBegin( GL_LINES );
		ri.ShowCollision( RB_DrawLine );
		pglEnd();
	}
	if( r_showtextures->integer )
	{
		RB_ShowTextures();
	}
}

/*
=================
RB_DrawDebugTools
=================
*/
static void RB_DrawDebugTools( void )
{
	if( gl_state.orthogonal || r_refdef.rdflags & RDF_NOWORLDMODEL )
		return;

	GL_Disable( GL_VERTEX_PROGRAM_ARB );
	GL_Disable( GL_FRAGMENT_PROGRAM_ARB );
	GL_Disable( GL_ALPHA_TEST );
	GL_Disable( GL_BLEND );
	GL_DepthFunc( GL_LEQUAL );
	GL_DepthMask( GL_TRUE );

	pglDepthRange( 0, 0 );

	if( r_showtris->integer ) RB_DrawTris();
	if( r_shownormals->integer ) RB_DrawNormals();
	if( r_showtangentspace->integer ) RB_DrawTangentSpace();
	if( r_showmodelbounds->integer ) RB_DrawModelBounds();

	pglDepthRange( 0, 1 );
}

/*
=================
RB_CheckMeshOverflow
=================
*/
void RB_CheckMeshOverflow( int numIndices, int numVertices )
{
	if( numIndices > MAX_INDICES || numVertices > MAX_VERTICES )
		Host_Error( "RB_CheckMeshOverflow: %i > MAX_INDICES or %i > MAX_VERTICES\n", numIndices, numVertices );

	if( numIndex + numIndices <= MAX_INDICES && numVertex + numVertices <= MAX_VERTICES )
		return;
	RB_RenderMesh();
}

/*
=================
RB_RenderMesh
=================
*/
void RB_RenderMesh( void )
{
	if( !numIndex || !numVertex )
		return;

	// update r_speeds statistics
	r_stats.numShaders++;
	r_stats.numStages += m_pCurrentShader->numStages;
	r_stats.numVertices += numVertex;
	r_stats.numIndices += numIndex;
	r_stats.totalIndices += numIndex * m_pCurrentShader->numStages;

	// render the shader
	if( GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
		RB_RenderShaderARB();
	else RB_RenderShader();

	// draw debug tools
	if( r_showtris->integer || r_physbdebug->integer || r_shownormals->integer || r_showtangentspace->integer || r_showmodelbounds->integer )
		RB_DrawDebugTools();

	// check for errors
	if( r_check_errors->integer ) R_CheckForErrors();

	// clear arrays
	numIndex = numVertex = 0;
}

/*
=================
RB_RenderMeshes
=================
*/
void RB_RenderMeshes( mesh_t *meshes, int numMeshes )
{
	int		i;
	mesh_t		*mesh;
	shader_t		*shader;
	ref_entity_t	*entity;
	int		infoKey;
	uint		sortKey = 0;

	if( r_skipbackend->integer || !numMeshes )
		return;

	r_stats.numMeshes += numMeshes;

	// Clear the state
	m_pRenderMesh = NULL;
	m_pRenderModel = NULL;
	m_pCurrentShader = NULL;
	m_pCurrentEntity = NULL;
	m_fShaderTime = 0;
	m_iInfoKey = -1;

	// draw everything
	for( i = 0, mesh = meshes; i < numMeshes; i++, mesh++ )
	{
		// check for changes
		if( sortKey != mesh->sortKey || (mesh->sortKey & 255) == 255 )
		{
			sortKey = mesh->sortKey;

			// unpack sort key
			shader = r_shaders[(sortKey>>18) & (MAX_SHADERS - 1)];
			entity = &r_entities[(sortKey >> 8) & MAX_ENTITIES-1];
			infoKey = sortKey & 255;

			// development tool
			if( r_debugsort->integer )
			{
				if( r_debugsort->integer != shader->sort )
					continue;
			}

			// check if the rendering state changed
			if((m_pCurrentShader != shader) || (m_pCurrentEntity != entity && !(shader->flags & SHADER_ENTITYMERGABLE)) || (m_iInfoKey != infoKey || infoKey == 255))
			{
				RB_RenderMesh();

				m_pCurrentShader = shader;
				m_iInfoKey = infoKey;
			}

			// check if the entity changed
			if( m_pCurrentEntity != entity )
			{
				if( entity == r_worldEntity )
					GL_LoadMatrix( r_worldMatrix );
				else if( entity->ent_type == ED_BSPBRUSH )
					R_RotateForEntity( entity );
				// sprites and studio models make transformation locally

				m_pCurrentEntity = entity;
				m_fShaderTime = r_refdef.time - entity->shaderTime;
				m_pRenderModel = m_pCurrentEntity->model;
			}
		}

		// set the current mesh
		m_pRenderMesh = mesh;

		// feed arrays
		switch( m_pRenderMesh->meshType )
		{
		case MESH_SKY:
			R_DrawSky();
			break;
		case MESH_SURFACE:
			R_DrawSurface();
			break;
		case MESH_STUDIO:
			R_DrawStudioModel();
			break;
		case MESH_SPRITE:
			R_DrawSpriteModel();
			break;
		case MESH_BEAM:
			R_DrawBeam();
			break;
		case MESH_PARTICLE:
			R_DrawParticle();
			break;
		case MESH_POLY:
			// R_DrawPoly();
			break;
		default:
			Host_Error( "RB_RenderMeshes: bad meshType (%i)\n", m_pRenderMesh->meshType );
		}
	}

	// make sure everything is flushed
	RB_RenderMesh();
}

/*
=================
RB_DrawStretchPic
=================
*/
void RB_DrawStretchPic( float x, float y, float w, float h, float sl, float tl, float sh, float th, shader_t *shader )
{
	int	i;

	if( r_skipbackend->integer )
		return;

	// check if the rendering state changed
	if( m_pCurrentShader != shader )
	{
		RB_RenderMesh();

		m_pCurrentShader = shader;
		m_fShaderTime = r_frameTime;
	}

	// check if the arrays will overflow
	RB_CheckMeshOverflow( 6, 4 );

	// draw it
	for( i = 2; i < 4; i++ )
	{
		indexArray[numIndex++] = numVertex + 0;
		indexArray[numIndex++] = numVertex + i-1;
		indexArray[numIndex++] = numVertex + i;
	}

	vertexArray[numVertex+0][0] = x;
	vertexArray[numVertex+0][1] = y;
	vertexArray[numVertex+0][2] = 0;
	vertexArray[numVertex+1][0] = x + w;
	vertexArray[numVertex+1][1] = y;
	vertexArray[numVertex+1][2] = 0;
	vertexArray[numVertex+2][0] = x + w;
	vertexArray[numVertex+2][1] = y + h;
	vertexArray[numVertex+2][2] = 0;
	vertexArray[numVertex+3][0] = x;
	vertexArray[numVertex+3][1] = y + h;
	vertexArray[numVertex+3][2] = 0;

	inTexCoordArray[numVertex+0][0] = sl;
	inTexCoordArray[numVertex+0][1] = tl;
	inTexCoordArray[numVertex+1][0] = sh;
	inTexCoordArray[numVertex+1][1] = tl;
	inTexCoordArray[numVertex+2][0] = sh;
	inTexCoordArray[numVertex+2][1] = th;
	inTexCoordArray[numVertex+3][0] = sl;
	inTexCoordArray[numVertex+3][1] = th;

	for( i = 0; i < 4; i++ )
	{
		inColorArray[numVertex][0] = gl_state.draw_color[0];
		inColorArray[numVertex][1] = gl_state.draw_color[1];
		inColorArray[numVertex][2] = gl_state.draw_color[2];
		inColorArray[numVertex][3] = gl_state.draw_color[3];
		numVertex++;
	}
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
RB_AllocStaticBuffer
=================
*/
uint RB_AllocStaticBuffer( uint target, int size )
{
	uint	buffer;

	if( rb_numVertexBuffers == MAX_VERTEX_BUFFERS )
		Host_Error( "RB_AllocStaticBuffer: MAX_VERTEX_BUFFERS hit\n" );

	pglGenBuffersARB( 1, &buffer );
	pglBindBufferARB( target, buffer );
	pglBufferDataARB( target, size, NULL, GL_STATIC_DRAW_ARB );

	rb_vertexBuffers[rb_numVertexBuffers++] = buffer;

	rb_staticBytes += size;
	rb_staticCount++;

	return buffer;
}

/*
=================
RB_AllocStreamBuffer
=================
*/
uint RB_AllocStreamBuffer( uint target, int size )
{
	uint	buffer;

	if( rb_numVertexBuffers == MAX_VERTEX_BUFFERS )
		Host_Error( "RB_AllocStreamBuffer: MAX_VERTEX_BUFFERS hit\n" );

	pglGenBuffersARB( 1, &buffer );
	pglBindBufferARB( target, buffer );
	pglBufferDataARB( target, size, NULL, GL_STREAM_DRAW_ARB );

	rb_vertexBuffers[rb_numVertexBuffers++] = buffer;

	rb_streamBytes += size;
	rb_streamCount++;

	return buffer;
}

/*
=================
RB_InitBackend
=================
*/
void RB_InitBackend( void )
{
	int		i;

	// build waveform tables
	RB_BuildTables();

	// clear the state
	m_pRenderMesh = NULL;
	m_pCurrentShader = NULL;
	m_pRenderModel = NULL;
	m_fShaderTime = 0;
	m_pCurrentEntity = NULL;
	m_iInfoKey = -1;

	// clear arrays
	numIndex = numVertex = 0;

	// Set default GL state
	GL_SetDefaultState();

	// create vertex buffers
	if(GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
	{
		rb_vbo.indexBuffer = RB_AllocStreamBuffer( GL_ELEMENT_ARRAY_BUFFER_ARB, MAX_INDICES * 4 * sizeof( uint ));

		rb_vbo.vertexBuffer = RB_AllocStreamBuffer( GL_ARRAY_BUFFER_ARB, MAX_VERTICES * 2 * sizeof(vec3_t));
		rb_vbo.normalBuffer = RB_AllocStreamBuffer( GL_ARRAY_BUFFER_ARB, MAX_VERTICES * sizeof(vec3_t));
		rb_vbo.colorBuffer = RB_AllocStreamBuffer( GL_ARRAY_BUFFER_ARB, MAX_VERTICES * sizeof(vec4_t));
		rb_vbo.texCoordBuffer[0] = RB_AllocStreamBuffer( GL_ARRAY_BUFFER_ARB, MAX_VERTICES * sizeof(vec3_t));

		if( GL_Support( R_ARB_MULTITEXTURE ))
		{
			for( i = 1; i < MAX_TEXTURE_UNITS; i++ )
			{
				if( GL_Support( R_FRAGMENT_PROGRAM_EXT ))
				{
					if( i >= gl_config.textureunits && (i >= gl_config.texturecoords || i >= gl_config.imageunits))
						break;
				}
				else
				{
					if( i >= gl_config.textureunits )
						break;
				}

				rb_vbo.texCoordBuffer[i] = RB_AllocStreamBuffer( GL_ARRAY_BUFFER_ARB, MAX_VERTICES * sizeof( vec3_t ));
			}
		}
	}
}

/*
=================
RB_ShutdownBackend
=================
*/
void RB_ShutdownBackend( void )
{
	int	i;

	// disable arrays
	if( GL_Support( R_ARB_MULTITEXTURE ))
	{
		for( i = MAX_TEXTURE_UNITS - 1; i > 0; i-- )
		{
			if(GL_Support( R_FRAGMENT_PROGRAM_EXT ))
			{
				if( i >= gl_config.textureunits && (i >= gl_config.texturecoords || i >= gl_config.imageunits))
					continue;
			}
			else
			{
				if( i >= gl_config.textureunits )
					continue;
			}

			GL_SelectTexture( i );
			pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		}
		GL_SelectTexture( 0 );
	}

	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	pglDisableClientState( GL_COLOR_ARRAY );
	pglDisableClientState( GL_NORMAL_ARRAY );
	pglDisableClientState( GL_VERTEX_ARRAY );

	// delete vertex buffers
	if(GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
	{
		for( i = 0; i < rb_numVertexBuffers; i++ )
			pglDeleteBuffersARB( 1, &rb_vertexBuffers[i] );

		memset( rb_vertexBuffers, 0, sizeof( rb_vertexBuffers ));

		rb_numVertexBuffers = 0;
		rb_staticBytes = 0;
		rb_staticCount = 0;
		rb_streamBytes = 0;
		rb_streamCount = 0;
	}
}     