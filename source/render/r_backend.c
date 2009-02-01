//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     r_backend.c - render backend interface
//=======================================================================

#include "r_local.h"

ref_backend_t	ref;
ref_globals_t	tr;

/*
=======================================================================

VERTEX BUFFERS

=======================================================================
*/
static ref_buffer_t	rb_vertexBuffers[MAX_VERTEX_BUFFERS];
static int	rb_numVertexBuffers;

/*
=================
RB_UpdateVertexBuffer
=================
*/
void RB_UpdateVertexBuffer( ref_buffer_t *vertexBuffer, const void *data, size_t size )
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
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, size, data, vertexBuffer->usage );
}

/*
=================
RB_AllocVertexBuffer
=================
*/
ref_buffer_t *RB_AllocVertexBuffer( size_t size, GLuint usage )
{
	ref_buffer_t	*vertexBuffer;

	if( rb_numVertexBuffers == MAX_VERTEX_BUFFERS )
		Host_Error( "RB_AllocVertexBuffer: MAX_VERTEX_BUFFERS limit exceeds\n" );

	vertexBuffer = &rb_vertexBuffers[rb_numVertexBuffers++];

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
RB_DrawElements
=================
*/
void RB_DrawElements( void )
{
	elem_t	*indices;

	indices = ref.indexArray;

	if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
		pglDrawRangeElementsEXT( GL_TRIANGLES, 0, ref.numVertex, ref.numIndex, GL_UNSIGNED_INT, indices );
	else pglDrawElements( GL_TRIANGLES, ref.numIndex, GL_UNSIGNED_INT, indices );
}

/*
=================
RB_InitVertexBuffers
=================
*/
void RB_InitVertexBuffers( void )
{
	int	i;

	ref.vertexBuffer = RB_AllocVertexBuffer( MAX_VERTEXES * sizeof(vec3_t), GL_STREAM_DRAW_ARB );
	ref.colorBuffer = RB_AllocVertexBuffer( MAX_VERTEXES * sizeof(rgba_t), GL_STREAM_DRAW_ARB );
	ref.normalBuffer = RB_AllocVertexBuffer( MAX_VERTEXES * sizeof(vec3_t), GL_STREAM_DRAW_ARB );
	for( i = 0; i < MAX_TEXTURE_UNITS; i++ )
		ref.texCoordBuffer[i] = RB_AllocVertexBuffer( MAX_VERTEXES * sizeof(vec3_t), GL_STREAM_DRAW_ARB );	
}

/*
=================
RB_ShutdownVertexBuffers
=================
*/
void RB_ShutdownVertexBuffers( void )
{
	ref_buffer_t	*vertexBuffer;
	int		i;

	if( !GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
	{
		Mem_Set( rb_vertexBuffers, 0, sizeof( rb_vertexBuffers ));
		rb_numVertexBuffers = 0;
		return;
	}

	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	for( i = 0, vertexBuffer = rb_vertexBuffers; i < rb_numVertexBuffers; i++, vertexBuffer++ )
		pglDeleteBuffersARB( 1, &vertexBuffer->bufNum );

	Mem_Set( rb_vertexBuffers, 0, sizeof( rb_vertexBuffers ));
	rb_numVertexBuffers = 0;
} 