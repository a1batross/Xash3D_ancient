//=======================================================================
//			Copyright XashXT Group 2007 ©
//		     r_backend.c - render backend utilites
//=======================================================================

#include "r_local.h"
#include "byteorder.h"
#include "mathlib.h"
#include "matrix_lib.h" 
#include "const.h"

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

int		m_iInfoKey;
float		m_fShaderTime;
mesh_t		*m_pRenderMesh;
ref_shader_t	*m_pCurrentShader;
ref_entity_t	*m_pCurrentEntity;

static GLenum	rb_drawMode;
static GLboolean	rb_CheckFlush;
static GLint	rb_vertexState;

static void RB_SetVertex( float x, float y, float z )
{
	GLuint	oldIndex = ref.numIndex;

	switch( rb_drawMode )
	{
	case GL_LINES:
		ref.indexArray[ref.numIndex++] = ref.numVertex;
		if( rb_vertexState++ == 1 )
		{
			RB_SetVertex( x + 1, y + 1, z + 1 );
			rb_vertexState = 0;
			rb_CheckFlush = true; // Flush for long sequences of quads.
		}
		break;
	case GL_TRIANGLES:
		ref.indexArray[ref.numIndex++] = ref.numVertex;
		if( rb_vertexState++ == 2 )
		{
			rb_vertexState = 0;
			rb_CheckFlush = true; // Flush for long sequences of triangles.
		}
		break;
	case GL_QUADS:
		if( rb_vertexState++ < 3 )
		{
			ref.indexArray[ref.numIndex++] = ref.numVertex;
		}
		else
		{
			// we've already done triangle (0, 1, 2), now draw (2, 3, 0)
			ref.indexArray[ref.numIndex++] = ref.numVertex - 1;
			ref.indexArray[ref.numIndex++] = ref.numVertex;
			ref.indexArray[ref.numIndex++] = ref.numVertex - 3;
			rb_vertexState = 0;
			rb_CheckFlush = true; // flush for long sequences of quads.
		}
		break;
	case GL_TRIANGLE_STRIP:
		if( ref.numVertex + rb_vertexState > MAX_VERTEXES )
		{
			// This is a strip that's too big for us to buffer.
			// (We can't just flush the buffer because we have to keep
			// track of the last two vertices.
			Host_Error( "RB_SetVertex: overflow: %i > MAX_VERTEXES\n", ref.numVertex + rb_vertexState );
		}
		if( rb_vertexState++ < 3 )
		{
			ref.indexArray[ref.numIndex++] = ref.numVertex;
		}
		else
		{
			// flip triangles between clockwise and counter clockwise
			if( rb_vertexState & 1 )
			{
				// draw triangle [n-2 n-1 n]
				ref.indexArray[ref.numIndex++] = ref.numVertex - 2;
				ref.indexArray[ref.numIndex++] = ref.numVertex - 1;
				ref.indexArray[ref.numIndex++] = ref.numVertex;
			}
			else
			{
				// draw triangle [n-1 n-2 n]
				ref.indexArray[ref.numIndex++] = ref.numVertex - 1;
				ref.indexArray[ref.numIndex++] = ref.numVertex - 2;
				ref.indexArray[ref.numIndex++] = ref.numVertex;
			}
		}
		break;
	case GL_POLYGON:
	case GL_TRIANGLE_FAN:	// same as polygon
		if( ref.numVertex + rb_vertexState > MAX_VERTEXES )
		{
			// This is a polygon or fan that's too big for us to buffer.
			// (We can't just flush the buffer because we have to keep
			// track of the starting vertex.
			Host_Error( "RB_SetVertex: overflow: %i > MAX_VERTEXES\n", ref.numVertex + rb_vertexState );
		}
		if( rb_vertexState++ < 3 )
		{
			ref.indexArray[ref.numIndex++] = ref.numVertex;
		}
		else
		{
			// draw triangle [0 n-1 n]
			ref.indexArray[ref.numIndex++] = ref.numVertex - ( rb_vertexState - 1 );
			ref.indexArray[ref.numIndex++] = ref.numVertex - 1;
			ref.indexArray[ref.numIndex++] = ref.numVertex;
		}
		break;
	default:
		Host_Error( "RB_SetVertex: unsupported mode: %i\n", rb_drawMode );
		break;
	}

	// copy current vertex
	ref.vertexArray[ref.numVertex][0] = x;
	ref.vertexArray[ref.numVertex][1] = y;
	ref.vertexArray[ref.numVertex][2] = z;
	ref.numVertex++;

	// flush buffer if needed
	if( rb_CheckFlush ) RB_CheckMeshOverflow( ref.numIndex - oldIndex, rb_vertexState );
}

static void RB_SetTexCoord( GLfloat s, GLfloat t, GLfloat ls, GLfloat lt )
{
	ref.inTexCoordArray[ref.numVertex][0] = s;
	ref.inTexCoordArray[ref.numVertex][1] = t;
	ref.inTexCoordArray[ref.numVertex][2] = ls;
	ref.inTexCoordArray[ref.numVertex][3] = lt;
}

static void RB_SetColor( GLfloat r, GLfloat g, GLfloat b, GLfloat a )
{
	ref.colorArray[ref.numVertex][0] = 255 * r;
	ref.colorArray[ref.numVertex][1] = 255 * g;
	ref.colorArray[ref.numVertex][2] = 255 * b;
	ref.colorArray[ref.numVertex][3] = 255 * a;
}

static void RB_SetNormal( GLfloat x, GLfloat y, GLfloat z )
{
	ref.normalArray[ref.numVertex][0] = x;
	ref.normalArray[ref.numVertex][1] = y;
	ref.normalArray[ref.numVertex][2] = z;
}

static void RB_SetTangent( GLfloat x, GLfloat y, GLfloat z )
{
	ref.tangentArray[ref.numVertex][0] = x;
	ref.tangentArray[ref.numVertex][1] = y;
	ref.tangentArray[ref.numVertex][2] = z;
}

static void RB_SetBinormal( GLfloat x, GLfloat y, GLfloat z )
{
	ref.binormalArray[ref.numVertex][0] = x;
	ref.binormalArray[ref.numVertex][1] = y;
	ref.binormalArray[ref.numVertex][2] = z;
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
	if( ref.numIndex == 0 ) return;
	RB_CheckMeshOverflow( 0, 0 );
}

void GL_Vertex2f( GLfloat x, GLfloat y )
{
	RB_SetVertex( x, y, 1.0f );
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

void GL_Tangent3fv( const GLfloat *v )
{
	RB_SetTangent( v[0], v[1], v[2] );
}

void GL_Binormal3fv( const GLfloat *v )
{
	RB_SetBinormal( v[0], v[1], v[2] );
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
	ref.colorArray[ref.numVertex][0] = red;
	ref.colorArray[ref.numVertex][1] = green;
	ref.colorArray[ref.numVertex][2] = blue;
	ref.colorArray[ref.numVertex][3] = alpha;
}

void GL_Color4ubv( const GLubyte *v )
{
	ref.colorArray[ref.numVertex][0] = v[0];
	ref.colorArray[ref.numVertex][1] = v[1];
	ref.colorArray[ref.numVertex][2] = v[2];
	ref.colorArray[ref.numVertex][3] = v[3];
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
	deform_t		*deformVertexes = m_pCurrentShader->deform;
	uint		deformVertexesNum = m_pCurrentShader->numDeforms;
	matrix3x3		mat1, mat2, mat3, matrix, invMatrix;
	vec3_t		vec, tmp, len, axis, rotCentre;
	int		index, longAxis, shortAxis;
	float		*table, *v;
	float		now, f, t;
	float		*quad[4];
	int		i, j;

	for( i = 0; i < deformVertexesNum; i++, deformVertexes++ )
	{
		switch( deformVertexes->type )
		{
		case DEFORM_WAVE:
			table = RB_TableForFunc(&deformVertexes->func);
			now = deformVertexes->func.params[2] + deformVertexes->func.params[3] * m_fShaderTime;

			for( j = 0; j < ref.numVertex; j++ )
			{
				v = ref.vertexArray[j];
				t = (v[0] + v[1] + v[2]) * deformVertexes->params[0] + now;
				f = table[((int)(t * TABLE_SIZE)) & TABLE_MASK] * deformVertexes->func.params[1] + deformVertexes->func.params[0];
				VectorMA( ref.vertexArray[j], f, ref.normalArray[j], ref.vertexArray[j] );
			}
			break;
		case DEFORM_MOVE:
			table = RB_TableForFunc(&deformVertexes->func);
			now = deformVertexes->func.params[2] + deformVertexes->func.params[3] * m_fShaderTime;
			f = table[((int)(now * TABLE_SIZE)) & TABLE_MASK] * deformVertexes->func.params[1] + deformVertexes->func.params[0];

			for( j = 0; j < ref.numVertex; j++ )
			{
				VectorMA(ref.vertexArray[j], f, deformVertexes->params, ref.vertexArray[j]);
			}
			break;
		case DEFORM_NORMAL:
			now = deformVertexes->params[1] * m_fShaderTime;

			for (j = 0; j < ref.numVertex; j++)
			{
				f = ref.normalArray[j][2] * now;
				ref.normalArray[j][0] *= (deformVertexes->params[0] * com.sin( f ));
				ref.normalArray[j][1] *= (deformVertexes->params[0] * com.cos( f ));
				VectorNormalizeFast( ref.normalArray[j] );
			}
			break;
		case DEFORM_AUTOSPRITE:
			if(( ref.numIndex % 6) || (ref.numVertex % 4 ))
			{
				MsgDev( D_WARN, "Shader '%s' has autoSprite but it's not a triangle quad\n", m_pCurrentShader->name );
				break;
			}

			if( m_pCurrentEntity == r_worldEntity || !m_pCurrentEntity->model )
				Matrix3x3_FromMatrix4x4( invMatrix, r_worldMatrix );
			else Matrix3x3_FromMatrix4x4( invMatrix, r_entityMatrix );

			for( index = 0; index < ref.numIndex; index += 6 )
			{
				quad[0] = (float *)(ref.vertexArray + ref.indexArray[index+0]);
				quad[1] = (float *)(ref.vertexArray + ref.indexArray[index+1]);
				quad[2] = (float *)(ref.vertexArray + ref.indexArray[index+2]);

				for( j = 2; j >= 0; j-- )
				{
					quad[3] = (float *)(ref.vertexArray + ref.indexArray[index+3+j]);
					if( !VectorCompare( quad[3], quad[0] ) && !VectorCompare( quad[3], quad[1] ) && !VectorCompare( quad[3], quad[2] ))
						break;
				}

				VectorSubtract( quad[0], quad[1], mat1[0] );
				VectorSubtract( quad[2], quad[1], mat1[1] );
				CrossProduct( mat1[0], mat1[1], mat1[2] );
				VectorNormalizeFast( mat1[2] );
				VectorVectors( mat1[2], mat1[1], mat1[0] );

				Matrix3x3_Concat( matrix, invMatrix, mat1 );

				rotCentre[0] = (quad[0][0] + quad[1][0] + quad[2][0] + quad[3][0]) * 0.25;
				rotCentre[1] = (quad[0][1] + quad[1][1] + quad[2][1] + quad[3][1]) * 0.25;
				rotCentre[2] = (quad[0][2] + quad[1][2] + quad[2][2] + quad[3][2]) * 0.25;

				for( j = 0; j < 4; j++ )
				{
					VectorSubtract(quad[j], rotCentre, vec);
					Matrix3x3_Transform( matrix, vec, quad[j] );
					VectorAdd( quad[j], rotCentre, quad[j] );
				}
			}
			break;
		case DEFORM_AUTOSPRITE2:
			if(( ref.numIndex % 6) || (ref.numVertex % 4 ))
			{
				MsgDev( D_WARN, "Shader '%s' has autoSprite2 but it's not a triangle quad\n", m_pCurrentShader->name );
				break;
			}

			for( index = 0; index < ref.numIndex; index += 6 )
			{
				quad[0] = (float *)(ref.vertexArray + ref.indexArray[index+0]);
				quad[1] = (float *)(ref.vertexArray + ref.indexArray[index+1]);
				quad[2] = (float *)(ref.vertexArray + ref.indexArray[index+2]);

				for( j = 2; j >= 0; j-- )
				{
					quad[3] = (float *)(ref.vertexArray + ref.indexArray[index+3+j]);
					if( !VectorCompare( quad[3], quad[0] ) && !VectorCompare( quad[3], quad[1] ) && !VectorCompare( quad[3], quad[2] ))
						break;
				}

				VectorSubtract( quad[1], quad[0], mat1[0] );
				VectorSubtract( quad[2], quad[0], mat1[1] );
				VectorSubtract( quad[2], quad[1], mat1[2] );

				len[0] = DotProduct( mat1[0], mat1[0] );
				len[1] = DotProduct( mat1[1], mat1[1] );
				len[2] = DotProduct( mat1[2], mat1[2] );

				if( len[2] > len[1] && len[2] > len[0] )
				{
					if( len[1] > len[0] )
					{
						longAxis = 1;
						shortAxis = 0;
					}
					else
					{
						longAxis = 0;
						shortAxis = 1;
					}
				}
				else if( len[1] > len[2] && len[1] > len[0] )
				{
					if( len[2] > len[0] )
					{
						longAxis = 2;
						shortAxis = 0;
					}
					else
					{
						longAxis = 0;
						shortAxis = 2;
					}
				}
				else if( len[0] > len[1] && len[0] > len[2] )
				{
					if( len[2] > len[1] )
					{
						longAxis = 2;
						shortAxis = 1;
					}
					else
					{
						longAxis = 1;
						shortAxis = 2;
					}
				}
				else
				{
					longAxis = 0;
					shortAxis = 0;
				}

				if( DotProduct( mat1[longAxis], mat1[shortAxis] ))
				{
					VectorNormalize2( mat1[longAxis], axis );
					VectorCopy(axis, mat1[1]);

					if( axis[0] || axis[1] )
						VectorVectors( mat1[1], mat1[0], mat1[2] );
					else VectorVectors( mat1[1], mat1[2], mat1[0] );
				}
				else
				{
					VectorNormalize2( mat1[longAxis], axis );
					VectorNormalize2( mat1[shortAxis], mat1[0] );
					VectorCopy( axis, mat1[1] );
					CrossProduct( mat1[0], mat1[1], mat1[2] );
				}

				rotCentre[0] = (quad[0][0] + quad[1][0] + quad[2][0] + quad[3][0]) * 0.25;
				rotCentre[1] = (quad[0][1] + quad[1][1] + quad[2][1] + quad[3][1]) * 0.25;
				rotCentre[2] = (quad[0][2] + quad[1][2] + quad[2][2] + quad[3][2]) * 0.25;

				if( !Matrix3x3_Compare( m_pCurrentEntity->matrix, matrix3x3_identity ))
				{
					VectorAdd( rotCentre, m_pCurrentEntity->origin, vec );
					VectorSubtract( r_refdef.vieworg, vec, tmp );
					Matrix3x3_Transform( m_pCurrentEntity->matrix, tmp, vec );
				}
				else
				{
					VectorAdd( rotCentre, m_pCurrentEntity->origin, vec );
					VectorSubtract( r_refdef.vieworg, vec, vec );
				}

				f = -DotProduct( vec, axis );

				VectorMA( vec, f, axis, mat2[2] );
				VectorNormalizeFast( mat2[2] );
				VectorCopy( axis, mat2[1] );
				CrossProduct( mat2[1], mat2[2], mat2[0] );

				VectorSet( mat3[0], mat2[0][0], mat2[1][0], mat2[2][0] );
				VectorSet( mat3[1], mat2[0][1], mat2[1][1], mat2[2][1] );
				VectorSet( mat3[2], mat2[0][2], mat2[1][2], mat2[2][2] );

				Matrix3x3_Concat( matrix, mat3, mat1 );

				for( j = 0; j < 4; j++ )
				{
					VectorSubtract( quad[j], rotCentre, vec );
					Matrix3x3_Transform( matrix, vec, quad[j] );
					VectorAdd( quad[j], rotCentre, quad[j] );
				}
			}
			break;
		default: Host_Error( "RB_DeformVertexes: unknown deformVertexes type %i in shader '%s'\n", deformVertexes->type, m_pCurrentShader->name);
		}
	}
}

static shaderStage_t *RB_SetShaderRenderMode( shaderStage_t *stage )
{
	static shaderStage_t currentStage;

	if( !m_pCurrentEntity || !(m_pCurrentShader->flags & SHADER_RENDERMODE))
		return stage;

	currentStage = *stage;

	switch( m_pCurrentEntity->rendermode )
	{
	case kRenderTransColor:
		currentStage.flags |= SHADERSTAGE_BLENDFUNC;
		currentStage.flags |= SHADERSTAGE_ALPHAGEN;
		currentStage.flags |= SHADERSTAGE_RGBGEN;
		currentStage.blendFunc.src = GL_SRC_COLOR;
		currentStage.blendFunc.dst = GL_ZERO;
		currentStage.alphaGen.type = ALPHAGEN_ENTITY;
		currentStage.rgbGen.type = RGBGEN_ENTITY;
		break;
	case kRenderTransTexture:
		currentStage.flags |= (SHADERSTAGE_BLENDFUNC|SHADERSTAGE_ALPHAGEN|SHADERSTAGE_RGBGEN);
		currentStage.alphaGen.type = ALPHAGEN_ENTITY;
		currentStage.blendFunc.src = GL_SRC_ALPHA;
		currentStage.blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
		currentStage.rgbGen.type = RGBGEN_VERTEX;
		break;
	case kRenderGlow:
		currentStage.flags |= SHADERSTAGE_BLENDFUNC;
		currentStage.flags |= SHADERSTAGE_ALPHAGEN;
		currentStage.flags |= SHADERSTAGE_RGBGEN;
		currentStage.blendFunc.src = GL_ONE_MINUS_SRC_ALPHA;
		currentStage.blendFunc.dst = GL_ONE;
		currentStage.flags &= ~SHADERSTAGE_DEPTHFUNC;
		currentStage.depthFunc.func = 0;
		break;
	case kRenderTransAlpha:
		currentStage.flags |= SHADERSTAGE_ALPHAFUNC;
		currentStage.flags |= SHADERSTAGE_ALPHAGEN;
		currentStage.flags |= SHADERSTAGE_RGBGEN;
		currentStage.alphaFunc.func = GL_GREATER;
		currentStage.alphaFunc.ref = 0.666;
		currentStage.alphaGen.type = ALPHAGEN_ENTITY;
		currentStage.rgbGen.type = RGBGEN_ENTITY;
		break;
	case kRenderTransAdd:
		currentStage.flags |= (SHADERSTAGE_BLENDFUNC|SHADERSTAGE_ALPHAGEN|SHADERSTAGE_RGBGEN);
		currentStage.alphaGen.type = ALPHAGEN_ENTITY;
		currentStage.rgbGen.type = RGBGEN_VERTEX;
		currentStage.blendFunc.src = GL_SRC_ALPHA;
		currentStage.blendFunc.dst = GL_ONE;
		break;
	case kRenderNormal:
	default: return stage;
	}
	return &currentStage;
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
	pglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 5, m_pCurrentEntity->matrix[0][0], m_pCurrentEntity->matrix[0][1], m_pCurrentEntity->matrix[0][2], 0 );
	pglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 6, m_pCurrentEntity->matrix[1][0], m_pCurrentEntity->matrix[1][1], m_pCurrentEntity->matrix[1][2], 0 );
	pglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 7, m_pCurrentEntity->matrix[2][0], m_pCurrentEntity->matrix[2][1], m_pCurrentEntity->matrix[2][2], 0 );
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
	pglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 5, m_pCurrentEntity->matrix[0][0], m_pCurrentEntity->matrix[0][1], m_pCurrentEntity->matrix[0][2], 0 );
	pglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 6, m_pCurrentEntity->matrix[1][0], m_pCurrentEntity->matrix[1][1], m_pCurrentEntity->matrix[1][2], 0 );
	pglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 7, m_pCurrentEntity->matrix[2][0], m_pCurrentEntity->matrix[2][1], m_pCurrentEntity->matrix[2][2], 0 );
	pglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 8, m_fShaderTime, 0, 0, 0 );
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
RB_CalcVertexColors
=================
*/
static void RB_CalcVertexColors( shaderStage_t *stage )
{
	rgbGen_t		*rgbGen;
	alphaGen_t	*alphaGen;
	shaderStage_t	*newStage;
	vec3_t		vec, dir;
	float		*table;
	float		now, f;
	byte		r, g, b, a;
	int		i;

	newStage = RB_SetShaderRenderMode( stage );
	RB_SetShaderStageState( newStage );

	rgbGen = &newStage->rgbGen;
	alphaGen = &newStage->alphaGen;

	switch( rgbGen->type )
	{
	case RGBGEN_IDENTITY:
		for( i = 0; i < ref.numVertex; i++ )
		{
			ref.colorArray[i][0] = 255;
			ref.colorArray[i][1] = 255;
			ref.colorArray[i][2] = 255;
		}
		break;
	case RGBGEN_IDENTITYLIGHTING:
		if( gl_config.deviceSupportsGamma )
			r = g = b = 255 >> r_overbrightbits->integer;
		else r = g = b = 255;

		for( i = 0; i < ref.numVertex; i++ )
		{
			ref.colorArray[i][0] = r;
			ref.colorArray[i][1] = g;
			ref.colorArray[i][2] = b;
		}
		break;
	case RGBGEN_WAVE:
		table = RB_TableForFunc( &rgbGen->func );
		now = rgbGen->func.params[2] + rgbGen->func.params[3] * m_fShaderTime;
		f = table[((int)(now * TABLE_SIZE)) & TABLE_MASK] * rgbGen->func.params[1] + rgbGen->func.params[0];

		f = bound( 0.0, f, 1.0 );

		r = 255 * f;
		g = 255 * f;
		b = 255 * f;

		for( i = 0; i < ref.numVertex; i++ )
		{
			ref.colorArray[i][0] = r;
			ref.colorArray[i][1] = g;
			ref.colorArray[i][2] = b;
		}
		break;
	case RGBGEN_COLORWAVE:
		table = RB_TableForFunc(&rgbGen->func);
		now = rgbGen->func.params[2] + rgbGen->func.params[3] * m_fShaderTime;
		f = table[((int)(now * TABLE_SIZE)) & TABLE_MASK] * rgbGen->func.params[1] + rgbGen->func.params[0];

		f = bound(0.0, f, 1.0);
			
		r = 255 * (rgbGen->params[0] * f);
		g = 255 * (rgbGen->params[1] * f);
		b = 255 * (rgbGen->params[2] * f);

		for( i = 0; i < ref.numVertex; i++ )
		{
			ref.colorArray[i][0] = r;
			ref.colorArray[i][1] = g;
			ref.colorArray[i][2] = b;
		}
		break;
	case RGBGEN_VERTEX:
		break;
	case RGBGEN_ONEMINUSVERTEX:
		for( i = 0; i < ref.numVertex; i++ )
		{
			ref.colorArray[i][0] = 255 - ref.colorArray[i][0];
			ref.colorArray[i][1] = 255 - ref.colorArray[i][1];
			ref.colorArray[i][2] = 255 - ref.colorArray[i][2];
		}
		break;
	case RGBGEN_ENTITY:
		switch( m_pCurrentEntity->rendermode )
		{
		case kRenderNormal:
		case kRenderTransAlpha:
		case kRenderTransTexture:
			break;
		case kRenderGlow:
		case kRenderTransAdd:
		case kRenderTransColor:
			for( i = 0; i < ref.numVertex; i++ )
			{
				ref.colorArray[i][0] = m_pCurrentEntity->rendercolor[0];
				ref.colorArray[i][1] = m_pCurrentEntity->rendercolor[1];
				ref.colorArray[i][2] = m_pCurrentEntity->rendercolor[2];
			}
			break;
		}
		break;
	case RGBGEN_ONEMINUSENTITY:
		for( i = 0; i < ref.numVertex; i++ )
		{
			ref.colorArray[i][0] = 255 - m_pCurrentEntity->rendercolor[0];
			ref.colorArray[i][1] = 255 - m_pCurrentEntity->rendercolor[1];
			ref.colorArray[i][2] = 255 - m_pCurrentEntity->rendercolor[2];
		}
		break;
	case RGBGEN_LIGHTINGAMBIENT:
		R_LightingAmbient();
		break;
	case RGBGEN_LIGHTINGDIFFUSE:
		R_LightingDiffuse();
		break;
	case RGBGEN_CONST:
		r = 255 * rgbGen->params[0];
		g = 255 * rgbGen->params[1];
		b = 255 * rgbGen->params[2];

		for( i = 0; i < ref.numVertex; i++ )
		{
			ref.colorArray[i][0] = r;
			ref.colorArray[i][1] = g;
			ref.colorArray[i][2] = b;
		}
		break;
	default:
		Host_Error( "RB_CalcVertexColors: unknown rgbGen type %i in shader '%s'\n", rgbGen->type, m_pCurrentShader->name);
	}

	switch( alphaGen->type )
	{
	case ALPHAGEN_IDENTITY:
		for( i = 0; i < ref.numVertex; i++ )
			ref.colorArray[i][3] = 255;
		break;
	case ALPHAGEN_WAVE:
		table = RB_TableForFunc(&alphaGen->func);
		now = alphaGen->func.params[2] + alphaGen->func.params[3] * m_fShaderTime;
		f = table[((int)(now * TABLE_SIZE)) & TABLE_MASK] * alphaGen->func.params[1] + alphaGen->func.params[0];
		f = bound( 0.0, f, 1.0 );
		a = 255 * f;

		for( i = 0; i < ref.numVertex; i++ )
			ref.colorArray[i][3] = a;
		break;
	case ALPHAGEN_ALPHAWAVE:
		table = RB_TableForFunc(&alphaGen->func);
		now = alphaGen->func.params[2] + alphaGen->func.params[3] * m_fShaderTime;
		f = table[((int)(now * TABLE_SIZE)) & TABLE_MASK] * alphaGen->func.params[1] + alphaGen->func.params[0];
		f = bound( 0.0, f, 1.0 );
		a = 255 * (alphaGen->params[0] * f);

		for( i = 0; i < ref.numVertex; i++ )
			ref.colorArray[i][3] = a;
		break;
	case ALPHAGEN_VERTEX:
		break;
	case ALPHAGEN_ONEMINUSVERTEX:
		for( i = 0; i < ref.numVertex; i++ )
			ref.colorArray[i][3] = 255 - ref.colorArray[i][3];
		break;
	case ALPHAGEN_ENTITY:
		switch( m_pCurrentEntity->rendermode )
		{
		case kRenderNormal:
		case kRenderTransColor:
			for( i = 0; i < ref.numVertex; i++ )
				ref.colorArray[i][3] = 1.0f;
			break;
		case kRenderGlow:
		case kRenderTransAdd:
		case kRenderTransAlpha:
		case kRenderTransTexture:
			for( i = 0; i < ref.numVertex; i++ )
				ref.colorArray[i][3] = m_pCurrentEntity->renderamt;
			break;
		}
		break;
	case ALPHAGEN_ONEMINUSENTITY:
		for( i = 0; i < ref.numVertex; i++ )
			ref.colorArray[i][3] = 255 - m_pCurrentEntity->renderamt;
		break;
	case ALPHAGEN_DOT:
		if( !Matrix3x3_Compare( m_pCurrentEntity->matrix, matrix3x3_identity ))
			Matrix3x3_Transform( m_pCurrentEntity->matrix, r_forward, vec );
		else VectorCopy( r_forward, vec );

		for( i = 0; i < ref.numVertex; i++ )
		{
			f = DotProduct(vec, ref.normalArray[i] );
			if( f < 0 ) f = -f;
			ref.colorArray[i][3] = 255 * bound( alphaGen->params[0], f, alphaGen->params[1] );
		}
		break;
	case ALPHAGEN_ONEMINUSDOT:
		if( !Matrix3x3_Compare( m_pCurrentEntity->matrix, matrix3x3_identity ))
			Matrix3x3_Transform( m_pCurrentEntity->matrix, r_forward, vec );
		else VectorCopy( r_forward, vec );

		for( i = 0; i < ref.numVertex; i++ )
		{
			f = DotProduct(vec, ref.normalArray[i] );
			if( f < 0 ) f = -f;
			ref.colorArray[i][3] = bound( alphaGen->params[0], 255 - f, alphaGen->params[1]);
		}
		break;
	case ALPHAGEN_FADE:
		for( i = 0; i < ref.numVertex; i++ )
		{
			VectorAdd( ref.vertexArray[i], m_pCurrentEntity->origin, vec );
			f = VectorDistance( vec, r_refdef.vieworg );

			f = bound( alphaGen->params[0], f, alphaGen->params[1] ) - alphaGen->params[0];
			f = f * alphaGen->params[2];
			ref.colorArray[i][3] = bound( 0.0, f, 1.0 );
		}
		break;
	case ALPHAGEN_ONEMINUSFADE:
		for( i = 0; i < ref.numVertex; i++ )
		{
			VectorAdd( ref.vertexArray[i], m_pCurrentEntity->origin, vec );
			f = VectorDistance( vec, r_refdef.vieworg );

			f = bound( alphaGen->params[0], f, alphaGen->params[1] ) - alphaGen->params[0];
			f = f * alphaGen->params[2];
			ref.colorArray[i][3] = 255 * bound( 0.0, 1.0 - f, 1.0 );
		}
		break;
	case ALPHAGEN_LIGHTINGSPECULAR:
		if( !Matrix3x3_Compare( m_pCurrentEntity->matrix, matrix3x3_identity ))
		{
			VectorSubtract( r_origin, m_pCurrentEntity->origin, dir );
			Matrix3x3_Transform( m_pCurrentEntity->matrix, dir, vec );
		}
		else VectorSubtract( r_origin, m_pCurrentEntity->origin, vec );

		for( i = 0; i < ref.numVertex; i++ )
		{
			VectorSubtract( vec, ref.vertexArray[i], dir );
			VectorNormalizeFast( dir );

			f = DotProduct( dir, ref.normalArray[i] );
			f = pow( f, alphaGen->params[0] );
			ref.colorArray[i][3] = 255 * bound( 0.0, f, 1.0 );
		}
		break;
	case ALPHAGEN_CONST:
		a = 255 * alphaGen->params[0];

		for( i = 0; i < ref.numVertex; i++ )
			ref.colorArray[i][3] = a;
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
	float		*table, *v;
	float		now, f, t;
	float		rad, s, c;
	vec2_t		st;

	switch( tcGen->type )
	{
	case TCGEN_BASE:
		for( i = 0; i < ref.numVertex; i++ )
		{
			ref.texCoordArray[unit][i][0] = ref.inTexCoordArray[i][0];
			ref.texCoordArray[unit][i][1] = ref.inTexCoordArray[i][1];
		}
		break;
	case TCGEN_LIGHTMAP:
		for( i = 0; i < ref.numVertex; i++ )
		{
			ref.texCoordArray[unit][i][0] = ref.inTexCoordArray[i][2];
			ref.texCoordArray[unit][i][1] = ref.inTexCoordArray[i][3];
		}
		break;
	case TCGEN_ENVIRONMENT:
		if( !Matrix3x3_Compare( m_pCurrentEntity->matrix, matrix3x3_identity ))
		{
			VectorSubtract( r_origin, m_pCurrentEntity->origin, dir );
			Matrix3x3_Transform( m_pCurrentEntity->matrix, dir, vec );
		}
		else VectorSubtract( r_origin, m_pCurrentEntity->origin, vec );

		for( i = 0; i < ref.numVertex; i++ )
		{
			VectorSubtract( vec, ref.vertexArray[i], dir );
			VectorNormalizeFast( dir );

			f = 2.0 * DotProduct( dir, ref.normalArray[i] );
			ref.texCoordArray[unit][i][0] = dir[0] - ref.normalArray[i][0] * f;
			ref.texCoordArray[unit][i][1] = dir[1] - ref.normalArray[i][1] * f;
		}
		break;
	case TCGEN_VECTOR:
		for( i = 0; i < ref.numVertex; i++ )
		{
			ref.texCoordArray[unit][i][0] = DotProduct( ref.vertexArray[i], &tcGen->params[0] );
			ref.texCoordArray[unit][i][1] = DotProduct( ref.vertexArray[i], &tcGen->params[3] );
		}
		break;
	case TCGEN_WARP:
		for( i = 0; i < ref.numVertex; i++ )
		{
			ref.texCoordArray[unit][i][0] = ref.inTexCoordArray[i][0] + rb_warpSinTable[((int)((ref.inTexCoordArray[i][1] * 8.0 + m_fShaderTime) * (256.0/M_PI2))) & 255] * (1.0/64);
			ref.texCoordArray[unit][i][1] = ref.inTexCoordArray[i][1] + rb_warpSinTable[((int)((ref.inTexCoordArray[i][0] * 8.0 + m_fShaderTime) * (256.0/M_PI2))) & 255] * (1.0/64);
		}
		break;
	case TCGEN_LIGHTVECTOR:
		if( m_pCurrentEntity == r_worldEntity )
		{
			for( i = 0; i < ref.numVertex; i++ )
			{
				R_LightDir( ref.vertexArray[i], lightVector );

				ref.texCoordArray[unit][i][0] = DotProduct( lightVector, ref.tangentArray[i] );
				ref.texCoordArray[unit][i][1] = DotProduct( lightVector, ref.binormalArray[i] );
				ref.texCoordArray[unit][i][2] = DotProduct( lightVector, ref.normalArray[i] );
			}
		}
		else
		{
			R_LightDir( m_pCurrentEntity->origin, dir );
			if( !Matrix3x3_Compare( m_pCurrentEntity->matrix, matrix3x3_identity ))
				Matrix3x3_Transform( m_pCurrentEntity->matrix, dir, lightVector );
			else VectorCopy( dir, lightVector );

			for( i = 0; i < ref.numVertex; i++ )
			{
				ref.texCoordArray[unit][i][0] = DotProduct(lightVector, ref.tangentArray[i] );
				ref.texCoordArray[unit][i][1] = DotProduct(lightVector, ref.binormalArray[i] );
				ref.texCoordArray[unit][i][2] = DotProduct(lightVector, ref.normalArray[i] );
			}
		}
		break;
	case TCGEN_HALFANGLE:
		if( m_pCurrentEntity == r_worldEntity )
		{
			for( i = 0; i < ref.numVertex; i++ )
			{
				R_LightDir( ref.vertexArray[i], lightVector );

				VectorSubtract( r_refdef.vieworg, ref.vertexArray[i], eyeVector );
				VectorNormalizeFast( lightVector );
				VectorNormalizeFast( eyeVector );
				VectorAdd( lightVector, eyeVector, halfAngle );

				ref.texCoordArray[unit][i][0] = DotProduct( halfAngle, ref.tangentArray[i] );
				ref.texCoordArray[unit][i][1] = DotProduct( halfAngle, ref.binormalArray[i] );
				ref.texCoordArray[unit][i][2] = DotProduct( halfAngle, ref.normalArray[i] );
			}
		}
		else
		{
			R_LightDir( m_pCurrentEntity->origin, dir );

			if( !Matrix3x3_Compare( m_pCurrentEntity->matrix, matrix3x3_identity ))
			{
				Matrix3x3_Transform( m_pCurrentEntity->matrix, dir, lightVector );

				VectorSubtract( r_origin, m_pCurrentEntity->origin, dir );
				Matrix3x3_Transform( m_pCurrentEntity->matrix, dir, eyeVector );
			}
			else
			{
				VectorCopy( dir, lightVector );
				VectorSubtract( r_refdef.vieworg, m_pCurrentEntity->origin, eyeVector );
			}

			VectorNormalizeFast( lightVector );
			VectorNormalizeFast( eyeVector );

			VectorAdd( lightVector, eyeVector, halfAngle );

			for( i = 0; i < ref.numVertex; i++ )
			{
				ref.texCoordArray[unit][i][0] = DotProduct(halfAngle, ref.tangentArray[i] );
				ref.texCoordArray[unit][i][1] = DotProduct(halfAngle, ref.binormalArray[i] );
				ref.texCoordArray[unit][i][2] = DotProduct(halfAngle, ref.normalArray[i] );
			}
		}
		break;
	case TCGEN_REFLECTION:
		pglTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB );
		pglTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB );
		pglTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB );

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
			for( j = 0; j < ref.numVertex; j++ )
			{
				ref.texCoordArray[unit][j][0] += tcMod->params[0];
				ref.texCoordArray[unit][j][1] += tcMod->params[1];
			}
			break;
		case TCMOD_SCALE:
			for( j = 0; j < ref.numVertex; j++ )
			{
				ref.texCoordArray[unit][j][0] *= tcMod->params[0];
				ref.texCoordArray[unit][j][1] *= tcMod->params[1];
			}
			break;
		case TCMOD_SCROLL:
			st[0] = tcMod->params[0] * m_fShaderTime;
			st[0] -= floor(st[0]);
			st[1] = tcMod->params[1] * m_fShaderTime;
			st[1] -= floor(st[1]);

			for( j = 0; j < ref.numVertex; j++ )
			{
				ref.texCoordArray[unit][j][0] += st[0];
				ref.texCoordArray[unit][j][1] += st[1];
			}
			break;
		case TCMOD_CONVEYOR:
			if( m_pCurrentEntity->framerate == 0.0f ) return;
			now = (m_pCurrentEntity->framerate * m_fShaderTime) * 0.0039; // magic number :-)
			s = m_pCurrentEntity->movedir[0];
			t = m_pCurrentEntity->movedir[1];

			st[0] = now * s;
			st[0] -= floor(st[0]);
			st[1] = now * t;
			st[1] -= floor(st[1]);

			for( j = 0; j < ref.numVertex; j++ )
			{
				ref.texCoordArray[unit][j][0] -= st[0];
				ref.texCoordArray[unit][j][1] += st[1];
			}
			break;
		case TCMOD_ROTATE:
			rad = -DEG2RAD( tcMod->params[0] * m_fShaderTime );
			s = com.sin( rad );
			c = com.cos( rad );

			for( j = 0; j < ref.numVertex; j++ )
			{
				st[0] = ref.texCoordArray[unit][j][0];
				st[1] = ref.texCoordArray[unit][j][1];
				ref.texCoordArray[unit][j][0] = c * (st[0] - 0.5) - s * (st[1] - 0.5) + 0.5;
				ref.texCoordArray[unit][j][1] = c * (st[1] - 0.5) + s * (st[0] - 0.5) + 0.5;
			}
			break;
		case TCMOD_STRETCH:
			table = RB_TableForFunc(&tcMod->func);
			now = tcMod->func.params[2] + tcMod->func.params[3] * m_fShaderTime;
			f = table[((int)(now * TABLE_SIZE)) & TABLE_MASK] * tcMod->func.params[1] + tcMod->func.params[0];
			
			f = (f) ? 1.0 / f : 1.0;
			t = 0.5 - 0.5 * f;

			for( j = 0; j < ref.numVertex; j++ )
			{
				ref.texCoordArray[unit][j][0] = ref.texCoordArray[unit][j][0] * f + t;
				ref.texCoordArray[unit][j][1] = ref.texCoordArray[unit][j][1] * f + t;
			}
			break;
		case TCMOD_TURB:
			table = RB_TableForFunc(&tcMod->func);
			now = tcMod->func.params[2] + tcMod->func.params[3] * m_fShaderTime;

			for( j = 0; j < ref.numVertex; j++ )
			{
				v = ref.vertexArray[j];
				ref.texCoordArray[unit][j][0] += (table[((int)(((v[0] + v[2]) * 1.0/128 * 0.125 + now) * TABLE_SIZE)) & TABLE_MASK] * tcMod->func.params[1] + tcMod->func.params[0]);
				ref.texCoordArray[unit][j][1] += (table[((int)(((v[1]) * 1.0/128 * 0.125 + now) * TABLE_SIZE)) & TABLE_MASK] * tcMod->func.params[1] + tcMod->func.params[0]);
			}
			break;
		case TCMOD_TRANSFORM:
			for( j = 0; j < ref.numVertex; j++ )
			{
				st[0] = ref.texCoordArray[unit][j][0];
				st[1] = ref.texCoordArray[unit][j][1];
				ref.texCoordArray[unit][j][0] = st[0] * tcMod->params[0] + st[1] * tcMod->params[2] + tcMod->params[4];
				ref.texCoordArray[unit][j][1] = st[1] * tcMod->params[1] + st[0] * tcMod->params[3] + tcMod->params[5];
			}
			break;
		default: Host_Error( "RB_CalcTextureCoords: unknown tcMod type %i in shader '%s'\n", tcMod->type, m_pCurrentShader->name);
		}
	}
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
RB_SetupTextureUnit
=================
*/
static void RB_SetupTextureUnit( stageBundle_t *bundle, uint unit )
{
	int	angleframe;

	GL_SelectTexture( unit );

	switch( bundle->texType )
	{
	case TEX_GENERIC:
		if( bundle->numTextures == 1 )
		{
			GL_BindTexture( bundle->textures[0] );
		}
		else if( bundle->flags & STAGEBUNDLE_FRAMES )
		{
			// manually changed
			bundle->currentFrame = bound( 0, bundle->currentFrame, bundle->numTextures - 1 );
			GL_BindTexture( bundle->textures[bundle->currentFrame] );
		}
		else if( bundle->flags & STAGEBUNDLE_ANIMFREQUENCY )
		{
			// animated image
			GL_BindTexture( bundle->textures[(int)(bundle->animFrequency * m_fShaderTime) % bundle->numTextures] );
		}
		break;
	case TEX_LIGHTMAP:
		if( m_iInfoKey != 255 )
		{
			GL_BindTexture( r_lightmapTextures[m_iInfoKey] );
			break;
		}
		R_UpdateSurfaceLightmap( m_pRenderMesh->mesh );
		break;
	case TEX_CINEMATIC:
		// not implemented
		//CIN_RunCinematic( bundle->cinematicHandle );
		//CIN_DrawCinematic( bundle->cinematicHandle );
		break;
	case TEX_ANGLEDMAP:
		angleframe = (int)((r_refdef.viewangles[1] - m_pCurrentEntity->angles[1])/360*8 + 0.5 - 4) & 7;
		GL_BindTexture( bundle->textures[angleframe] );
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

	RB_SetShaderState();
	RB_DeformVertexes();

	RB_UpdateVertexBuffer( ref.vertexBuffer, ref.vertexArray, ref.numVertex * sizeof( vec3_t ));
	pglEnableClientState( GL_VERTEX_ARRAY );
	pglVertexPointer( 3, GL_FLOAT, 0, ref.vertexBuffer->pointer );

	RB_UpdateVertexBuffer( ref.normalBuffer, ref.normalArray, ref.numVertex * sizeof( vec3_t ));
	pglEnableClientState( GL_NORMAL_ARRAY );
	pglNormalPointer( GL_FLOAT, 0, ref.normalBuffer->pointer );

	if( !GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ) && GL_Support( R_CUSTOM_VERTEX_ARRAY_EXT ))
	{
		if( m_pCurrentShader->numStages != 1 )
		{
			pglDisableClientState( GL_COLOR_ARRAY );
			pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
			pglLockArraysEXT( 0, ref.numVertex );
		}
	}

	for( i = 0; i < m_pCurrentShader->numStages; i++ )
	{
		stage = m_pCurrentShader->stages[i];

		RB_CalcVertexColors( stage );

		RB_UpdateVertexBuffer( ref.colorBuffer, ref.colorArray, ref.numVertex * sizeof( rgba_t ));
		pglEnableClientState( GL_COLOR_ARRAY );
		pglColorPointer( 4, GL_UNSIGNED_BYTE, 0, ref.colorBuffer->pointer );
	
		for( j = 0; j < stage->numBundles; j++ )
		{
			bundle = stage->bundles[j];

			RB_SetupTextureUnit( bundle, j );
			RB_CalcTextureCoords( bundle, j );

			RB_UpdateVertexBuffer( ref.texCoordBuffer[j], ref.texCoordArray[j], ref.numVertex * sizeof( vec3_t ));
			pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			pglTexCoordPointer( 3, GL_FLOAT, 0, ref.texCoordBuffer[j]->pointer );
		}

		if(!GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ) && GL_Support( R_CUSTOM_VERTEX_ARRAY_EXT ))
		{
			if( m_pCurrentShader->numStages == 1 )
				pglLockArraysEXT( 0, ref.numVertex );
		}

		RB_DrawElements();

		for( j = stage->numBundles - 1; j >= 0; j-- )
		{
			bundle = stage->bundles[j];

			RB_CleanupTextureUnit( bundle, j );
			pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		}
	}

	if(!GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ) && GL_Support( R_CUSTOM_VERTEX_ARRAY_EXT ))
		pglUnlockArraysEXT();
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
	int		i, j, k, l;

	pglDisableClientState( GL_VERTEX_ARRAY );
	pglDisableClientState( GL_NORMAL_ARRAY );
	pglDisableClientState( GL_COLOR_ARRAY );
	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	RB_SetShaderState();
	RB_DeformVertexes();

	for( i = 0; i < m_pCurrentShader->numStages; i++ )
	{
		stage = m_pCurrentShader->stages[i];

		RB_CalcVertexColors( stage );
	
		for( j = 0; j < stage->numBundles; j++ )
		{
			bundle = stage->bundles[j];

			RB_SetupTextureUnit( bundle, j );
			RB_CalcTextureCoords( bundle, j );
		}

		for( k = 0; k < ref.numIndex; k += 3 )
		{
			pglBegin( GL_TRIANGLES );

			l = ref.indexArray[k+0];
			pglTexCoord2f( ref.texCoordArray[0][l][0], ref.texCoordArray[0][l][1] );
			pglNormal3fv( ref.normalArray[l]);
			pglColor4ubv( ref.colorArray[l] );
			pglVertex3fv( ref.vertexArray[l] );

			l = ref.indexArray[k+1];
			pglTexCoord2f( ref.texCoordArray[0][l][0], ref.texCoordArray[0][l][1] );
			pglNormal3fv( ref.normalArray[l]);
			pglColor4ubv( ref.colorArray[l] );
			pglVertex3fv( ref.vertexArray[l] );

			l = ref.indexArray[k+2];
			pglTexCoord2f( ref.texCoordArray[0][l][0], ref.texCoordArray[0][l][1] );
			pglNormal3fv( ref.normalArray[l]);
			pglColor4ubv( ref.colorArray[l] );
			pglVertex3fv( ref.vertexArray[l] );

			pglEnd();
		}

		for( j = stage->numBundles - 1; j >= 0; j-- )
		{
			bundle = stage->bundles[j];
			RB_CleanupTextureUnit( bundle, j );
		}
	}
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
		RB_DrawElements();
	}
	else
	{
		if( GL_Support( R_CUSTOM_VERTEX_ARRAY_EXT ))
			pglLockArraysEXT( 0, ref.numVertex );

		RB_DrawElements();

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
	for( i = 0; i < ref.numVertex; i++ )
	{
		VectorAdd( ref.vertexArray[i], ref.normalArray[i], v );
		pglVertex3fv( ref.vertexArray[i] );
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
	for( i = 0; i < ref.numVertex; i++ )
	{
		VectorAdd( ref.vertexArray[i], ref.tangentArray[i], v );
		pglVertex3fv( ref.vertexArray[i] );
		pglVertex3fv( v );
	}
	pglEnd();

	pglColor4f( 0.0f, 1.0f, 0.0f, 1.0f );

	pglBegin( GL_LINES );
	for( i = 0; i < ref.numVertex; i++ )
	{
		VectorAdd( ref.vertexArray[i], ref.binormalArray[i], v );
		pglVertex3fv( ref.vertexArray[i] );
		pglVertex3fv( v );
	}
	pglEnd();

	pglColor4f( 0.0f, 0.0f, 1.0f, 1.0f );

	pglBegin( GL_LINES );
	for( i = 0; i < ref.numVertex; i++ )
	{
		VectorAdd( ref.vertexArray[i], ref.normalArray[i], v );
		pglVertex3fv( ref.vertexArray[i] );
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

static void RB_DrawLine( int color, int numpoints, const float *points, const int *elements )
{
	int	i = numpoints - 1;
	vec3_t	p0, p1;

	VectorSet( p0, points[i*3+0], points[i*3+1], points[i*3+2] );
	if( r_physbdebug->integer == 1 ) ConvertPositionToGame( p0 );

	for (i = 0; i < numpoints; i ++)
	{
		VectorSet( p1, points[i*3+0], points[i*3+1], points[i*3+2] );
		if( r_physbdebug->integer == 1 ) ConvertPositionToGame( p1 );
 
		pglColor4ubv( (byte *)color );
		pglVertex3fv( p0 );
		pglVertex3fv( p1 );
 
 		VectorCopy( p1, p0 );
 	}
}

void RB_DebugGraphics( void )
{
	if( r_refdef.onlyClientDraw )
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
	if( r_pause_bw->integer )
	{
		RB_DrawPauseScreen();
	}
}

/*
=================
RB_DrawDebugTools
=================
*/
static void RB_DrawDebugTools( void )
{
	if( gl_state.orthogonal || r_refdef.onlyClientDraw )
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
	if( numIndices > MAX_ELEMENTS || numVertices > MAX_VERTEXES )
		Host_Error( "RB_CheckMeshOverflow: %i > MAX_ELEMENTS or %i > MAX_VERTEXES\n", numIndices, numVertices );

	if( ref.numIndex + numIndices <= MAX_ELEMENTS && ref.numVertex + numVertices <= MAX_VERTEXES )
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
	if( !ref.numIndex || !ref.numVertex || !m_pCurrentShader )
		return;

	// update r_speeds statistics
	r_stats.numShaders++;
	r_stats.numStages += m_pCurrentShader->numStages;
	r_stats.numVertices += ref.numVertex;
	r_stats.numIndices += ref.numIndex;
	r_stats.totalIndices += ref.numIndex * m_pCurrentShader->numStages;

	// NOTE: too small models uses glBegin\glEnd for perfomance reason
//	if( ref.numVertex < 512 ) 
	if( glw_state.software )
		RB_RenderShader();
	else RB_RenderShaderARB();

	// draw debug tools
	if( r_showtris->integer || r_physbdebug->integer || r_shownormals->integer || r_showtangentspace->integer || r_showmodelbounds->integer )
		RB_DrawDebugTools();

	// check for errors
	if( r_check_errors->integer ) R_CheckForErrors();

	// clear arrays
	ref.numIndex = ref.numVertex = 0;
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
	ref_shader_t	*shader;
	ref_entity_t	*entity;
	int		infoKey;
	qword		sortKey = 0;

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
			shader = &r_shaders[(sortKey>>20) & (MAX_SHADERS - 1)];
			entity = &r_entities[(sortKey>>8) & MAX_ENTITIES-1];
			infoKey = sortKey & 255;

			Com_Assert( shader == NULL );

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
				if( entity->model )
				{
					switch( entity->model->type )
					{
					case mod_brush:
						R_RotateForEntity( entity );
						break;
					case mod_studio:
					case mod_sprite:
						GL_LoadMatrix( r_worldMatrix );
						break;
					default:	break;
					}
				}
				else GL_LoadMatrix( r_worldMatrix );
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
			R_DrawPoly();
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
void RB_DrawStretchPic( float x, float y, float w, float h, float sl, float tl, float sh, float th, ref_shader_t *shader )
{
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

	GL_Begin( GL_QUADS );
		GL_Color4ubv( gl_state.draw_color );
		GL_TexCoord2f( sl, tl );
		GL_Vertex2f( x, y );

		GL_Color4ubv( gl_state.draw_color );
		GL_TexCoord2f( sh, tl );
		GL_Vertex2f( x + w, y );

		GL_Color4ubv( gl_state.draw_color );
		GL_TexCoord2f( sh, th );
		GL_Vertex2f( x + w, y + h );

		GL_Color4ubv( gl_state.draw_color );
		GL_TexCoord2f( sl, th );
		GL_Vertex2f( x, y + h );
	GL_End();
}

/*
=================
RB_InitBackend
=================
*/
void RB_InitBackend( void )
{
	Mem_Set( &ref, 0, sizeof( ref ));

	// build waveform tables
	RB_BuildTables();

	// clear the state
	m_pRenderMesh = NULL;
	m_pCurrentShader = NULL;
	m_pRenderModel = NULL;
	m_fShaderTime = 0;
	m_pCurrentEntity = NULL;
	m_iInfoKey = -1;

	// Set default GL state
	GL_SetDefaultState();

	RB_InitVertexBuffers();
}

/*
=================
RB_ShutdownBackend
=================
*/
void RB_ShutdownBackend( void )
{
	int	i;

	if( !glw_state.initialized ) return;

	// disable arrays
	if( GL_Support( R_ARB_MULTITEXTURE ))
	{
		for( i = MAX_TEXTURE_UNITS - 1; i > 0; i-- )
		{
			if(GL_Support( R_FRAGMENT_PROGRAM_EXT ))
			{
				if( i >= gl_config.textureunits && (i >= gl_config.texturecoords || i >= gl_config.teximageunits))
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

	pglDisableClientState( GL_COLOR_ARRAY );
	pglDisableClientState( GL_NORMAL_ARRAY );
	pglDisableClientState( GL_VERTEX_ARRAY );

	// shutdown vertex buffers
	RB_ShutdownVertexBuffers();
}     