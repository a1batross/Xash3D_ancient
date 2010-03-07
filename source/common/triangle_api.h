//=======================================================================
//			Copyright XashXT Group 2008 ©
//		  triangle_api.h - custom triangles rendering
//=======================================================================
#ifndef TRIANGLE_API_H
#define TRIANGLE_API_H

typedef enum 
{
	TRI_FRONT = 0,
	TRI_NONE,
} TRI_CULL;

typedef enum
{
	TRI_TRIANGLES = 1,
	TRI_TRIANGLE_FAN,
	TRI_TRIANGLE_STRIP,
	TRI_POLYGON,
	TRI_QUADS,
	TRI_LINES,
} TRI_DRAW;

typedef enum
{
	TRI_SHADER = 1,
	TRI_MAXCAPS
} TRI_CAPS;

typedef struct triapi_s
{
	size_t	api_size;			// must match with sizeof( triapi_t );

	shader_t	(*LoadShader)( const char *szShaderName, int fShaderNoMip );
	shader_t	(*GetSpriteTexture)( int spriteIndex, int spriteFrame );
	void	(*RenderMode)( kRenderMode_t mode );
	void	(*Begin)( TRI_DRAW mode );
	void	(*End)( void );

	void	(*Enable)( int cap );
	void	(*Disable)( int cap );
	void	(*Vertex3f)( float x, float y, float z );
	void	(*Vertex3fv)( const float *v );
	void	(*Normal3f)( float x, float y, float z );
	void	(*Normal3fv)( const float *v );
	void	(*Color4f)( float r, float g, float b, float a );
	void	(*Color4ub)( byte r, byte g, byte b, byte a );
	void	(*TexCoord2f)( float u, float v );
	void	(*Bind)( shader_t shader, int frame );	// use handle that return pfnLoadShader
	void	(*CullFace)( TRI_CULL mode );
	void	(*ScreenToWorld)( float *screen, float *world  ); 
	int	(*WorldToScreen)( float *world, float *screen );  // returns 1 if it's z clipped
	void	(*Fog)( float flFogColor[3], float flStart, float flEnd, int bOn );
} triapi_t;

#endif//TRIANGLE_API_H