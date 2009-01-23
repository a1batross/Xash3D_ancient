//=======================================================================
//			Copyright XashXT Group 2008 ©
//		  triangle_api.h - custom triangles rendering
//=======================================================================
#ifndef TRIANGLE_API_H
#define TRIANGLE_API_H

typedef enum 
{
	TRI_FRONT = 0,
	TRI_BACK,
	TRI_NONE,
} TRI_CULL;

typedef enum
{
	TRI_TRIANGLES = 0,
	TRI_TRIANGLE_FAN,
	TRI_TRIANGLE_STRIP,
	TRI_POLYGON,
	TRI_QUADS,
	TRI_LINES,
} TRI_DRAW;

typedef enum
{
	TRI_SHADER = 0,
	TRI_CLIP_PLANE,
} TRI_CAPS;

typedef struct triapi_s
{
	size_t	api_size;			// must match with sizeof( triapi_t );

	void	(*RenderMode)( kRenderMode_t mode );
	void	(*Bind)( HSPRITE shader );	// use handle that return pfnLoadShader
	void	(*Begin)( TRI_DRAW mode );
	void	(*End)( void );

	void	(*Enable)( int cap );
	void	(*Disable)( int cap );
	void	(*Vertex2f)( float x, float y );
	void	(*Vertex3f)( float x, float y, float z );
	void	(*Vertex2fv)( const float *v );
	void	(*Vertex3fv)( const float *v );
	void	(*Color3f)( float r, float g, float b );
	void	(*Color4f)( float r, float g, float b, float a );
	void	(*Color4ub)( byte r, byte g, byte b, byte a );
	void	(*TexCoord2f)( float u, float v );
	void	(*TexCoord2fv)( const float *v );
	void	(*CullFace)( TRI_CULL mode );
	void	(*ScreenToWorld)( float *screen, float *world  ); 
	int	(*WorldToScreen)( float *world, float *screen );  // returns 1 if it's z clipped
	void	(*Fog)( float flFogColor[3], float flStart, float flEnd, int bOn );
} triapi_t;

#endif//TRIANGLE_API_H