//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_draw.c - draw 2d pictures
//=======================================================================

#include "r_local.h"

/*
=============
Draw_FindPic

oldstuff
=============
*/
texture_t *Draw_FindPic( const char *name )
{
	string		fullname;
	const byte	*buffer = NULL;
	size_t		bufsize = 0;		

	// don't let user set invalid font
	if(com.stristr( name, "fonts/" ))
		buffer = FS_LoadInternal( "default.dds", &bufsize );

	com.snprintf( fullname, MAX_STRING, "gfx/%s", name );
	return R_FindTexture( fullname, buffer, bufsize, TF_IMAGE2D, 0 );
}

/*
=============
Draw_GetPicSize

oldstuff
=============
*/
void Draw_GetPicSize( int *w, int *h, const char *pic )
{
	texture_t *tex;

	tex = Draw_FindPic( pic );
	if( !tex )
	{
		*w = *h = -1;
		return;
	}
	*w = tex->width;
	*h = tex->height;
}


/*
=================
R_GetPicSize

this is needed by some client drawing functions
=================
*/
void R_GetPicSize( int *w, int *h, const char *pic )
{
	shader_t	*shader;

	shader = R_RegisterShaderNoMip( pic );

	*w = (int)shader->stages[0]->bundles[0]->textures[0]->width;
	*h = (int)shader->stages[0]->bundles[0]->textures[0]->height;
}


/*
=================
R_DrawStretchPic

Batches the pic in the vertex arrays.
Call RB_RenderMesh to flush.
=================
*/
void R_DrawStretchPic( float x, float y, float w, float h, float sl, float tl, float sh, float th, const char *name )
{
	shader_t *shader = R_RegisterShaderNoMip( name );
	RB_DrawStretchPic( x, y, w, h, sl, tl, sh, th, shader );
}

/*
=================
R_DrawStretchRaw

Cinematic streaming
=================
*/
void R_DrawStretchRaw( int x, int y, int w, int h, int rawWidth, int rawHeight, const byte *raw, bool dirty )
{
	int	width = 1, height = 1;

	// Make sure everything is flushed if needed
	if( !dirty ) RB_RenderMesh();

	// Check the dimensions
	while( width < rawWidth ) width <<= 1;
	while( height < rawHeight ) height <<= 1;

	if( rawWidth != width || rawHeight != height )
		Host_Error( "R_DrawStretchRaw: size is not a power of two (%i x %i)\n", rawWidth, rawHeight );

	if( rawWidth > gl_config.max_2d_texture_size )
		Host_Error( "R_DrawStretchRaw: size exceeds hardware limits (%i > %i)\n", rawWidth, gl_config.max_2d_texture_size ); 
	if( rawHeight > gl_config.max_2d_texture_size )
		Host_Error( "R_DrawStretchRaw: size exceeds hardware limits (%i > %i)\n", rawHeight, gl_config.max_2d_texture_size); 

	// update the texture as appropriate
	GL_BindTexture( r_rawTexture );

	if( rawWidth == r_rawTexture->width && rawHeight == r_rawTexture->height )
		pglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, rawWidth, rawHeight, GL_RGBA, GL_UNSIGNED_BYTE, raw );
	else
	{
		r_rawTexture->width = rawWidth;
		r_rawTexture->height = rawHeight;
		pglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, rawWidth, rawHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, raw );
	}

	if( dirty ) return;

	// Set the state
	GL_TexEnv( GL_REPLACE );

	GL_Disable( GL_CULL_FACE );
	GL_Disable( GL_POLYGON_OFFSET_FILL );
	GL_Disable( GL_VERTEX_PROGRAM_ARB );
	GL_Disable( GL_FRAGMENT_PROGRAM_ARB );
	GL_Disable( GL_ALPHA_TEST );
	GL_Disable( GL_BLEND );
	GL_Disable( GL_DEPTH_TEST );
	GL_DepthMask( GL_FALSE );

	pglEnable( GL_TEXTURE_2D );

	// draw it
	pglColor4fv( gl_state.draw_color );

	pglBegin( GL_QUADS );
	pglTexCoord2f( 0, 0 );
	pglVertex2f( x, y );
	pglTexCoord2f( 1, 0 );
	pglVertex2f( x+w, y );
	pglTexCoord2f( 1, 1 );
	pglVertex2f( x+w, y+h );
	pglTexCoord2f( 0, 1 );
	pglVertex2f( x, y+h );
	pglEnd();

	pglDisable( GL_TEXTURE_2D );
}

/*
=================
R_CopyScreenRect

Screen blur
=================
*/
void R_CopyScreenRect( float x, float y, float w, float h )
{
	if( !gl_config.texRectangle || !GL_Support( R_CLAMPTOEDGE_EXT ))
		return;

	if( w > gl_config.max_2d_rectangle_size || h > gl_config.max_2d_rectangle_size )
		return;

	pglBindTexture( gl_config.texRectangle, gl_state.screenTexture );
	pglCopyTexImage2D( gl_config.texRectangle, 0, GL_RGB, x, r_height->integer - h - y, w, h, 0 );

	pglTexParameterf( gl_config.texRectangle, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	pglTexParameterf( gl_config.texRectangle, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	pglTexParameterf( gl_config.texRectangle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	pglTexParameterf( gl_config.texRectangle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
}

/*
 =================
 R_DrawScreenRect

 Screen blur
 =================
*/
void R_DrawScreenRect( float x, float y, float w, float h )
{
	if( !gl_config.texRectangle || !GL_Support( R_CLAMPTOEDGE_EXT ))
		return;

	if( w > gl_config.max_2d_rectangle_size || h > gl_config.max_2d_rectangle_size )
		return;

	// set the state
	GL_TexEnv( GL_MODULATE );
	GL_Disable( GL_CULL_FACE );
	GL_Disable( GL_POLYGON_OFFSET_FILL );
	GL_Disable( GL_VERTEX_PROGRAM_ARB );
	GL_Disable( GL_FRAGMENT_PROGRAM_ARB );
	GL_Disable( GL_ALPHA_TEST );
	GL_Enable( GL_BLEND );
	GL_Disable( GL_DEPTH_TEST );

	GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	GL_DepthMask( GL_FALSE );

	pglEnable( gl_config.texRectangle );

	// draw it
	pglBindTexture( gl_config.texRectangle, gl_state.screenTexture );
	pglColor4fv( gl_state.draw_color );

	pglBegin( GL_QUADS );
	pglTexCoord2f( 0, h );
	pglVertex2f( x, y );
	pglTexCoord2f( w, h );
	pglVertex2f( x+w, y );
	pglTexCoord2f( w, 0 );
	pglVertex2f(x+w, y+h);
	pglTexCoord2f( 0, 0 );
	pglVertex2f( x, y+h );
	pglEnd();

	pglDisable( gl_config.texRectangle );
}


/*
=============
R_DrawFill

fills a box of pixels with a single color
=============
*/
void R_DrawFill( float x, float y, float w, float h )
{
	pglDisable( GL_TEXTURE_2D );
	pglColor4fv( gl_state.draw_color );
	GL_Enable( GL_BLEND );
	if(gl_state.draw_color[3] != 1.0f )
		GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	else GL_BlendFunc( GL_ONE, GL_ZERO );

	pglBegin( GL_QUADS );
	pglVertex2f( x, y );
	pglVertex2f( x + w, y );
	pglVertex2f( x + w, y + h );
	pglVertex2f( x, y + h );
	pglEnd();

	GL_Disable( GL_BLEND );
	pglEnable( GL_TEXTURE_2D );
}