//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_draw.c - draw 2d pictures
//=======================================================================

#include "r_local.h"
#include "mathlib.h"

static vec4_t pic_xyz[4] = { {0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {0,0,0,1} };
static vec2_t pic_st[4];
static rgba_t pic_colors[4];
static mesh_t pic_mesh = { 4, pic_xyz, pic_xyz, NULL, pic_st, { 0, 0, 0, 0 }, { pic_colors, pic_colors, pic_colors, pic_colors }, 6, NULL };
meshbuffer_t  pic_mbuffer;

/*
===============
R_DrawSetColor
===============
*/
void R_DrawSetColor( const rgba_t color )
{
	if( color ) Vector4Copy( color, glState.draw_color );
	else Vector4Set( glState.draw_color, 255, 255, 255, 255 );
}

/*
===============
R_DrawStretchPic
===============
*/
void R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, shader_t handle )
{
	int		bcolor;
	ref_shader_t	*shader;
	static int	oldframe;

	if( handle < 0 || handle > MAX_SHADERS || !(shader = &r_shaders[handle]))
		return;

	// lower-left
	Vector2Set( pic_xyz[0], x, y );
	Vector2Set( pic_st[0], s1, t1 );
	Vector4Copy( glState.draw_color, pic_colors[0] );
	bcolor = *(int *)pic_colors[0];

	// lower-right
	Vector2Set( pic_xyz[1], x+w, y );
	Vector2Set( pic_st[1], s2, t1 );
	*(int *)pic_colors[1] = bcolor;

	// upper-right
	Vector2Set( pic_xyz[2], x+w, y+h );
	Vector2Set( pic_st[2], s2, t2 );
	*(int *)pic_colors[2] = bcolor;

	// upper-left
	Vector2Set( pic_xyz[3], x, y+h );
	Vector2Set( pic_st[3], s1, t2 );
	*(int *)pic_colors[3] = bcolor;

	if( pic_mbuffer.shaderkey != (int)shader->sortkey || -pic_mbuffer.infokey-1+4 > MAX_ARRAY_VERTS )
	{
		if( pic_mbuffer.shaderkey )
		{
			pic_mbuffer.infokey = -1;
			R_RenderMeshBuffer( &pic_mbuffer );
		}
	}

	tr.iRenderMode = glState.draw_rendermode;
	pic_mbuffer.shaderkey = shader->sortkey;
	pic_mbuffer.infokey -= 4;

	// upload video right before rendering
	if( shader->flags & SHADER_VIDEOMAP ) R_UploadCinematicShader( shader );
	R_PushMesh( &pic_mesh, MF_TRIFAN|shader->features | ( r_shownormals->integer ? MF_NORMALS : 0 ));

	if( oldframe != glState.draw_frame )
	{
		if( pic_mbuffer.shaderkey != (int)shader->sortkey )
		{
			// will be rendering on next call
			oldframe = glState.draw_frame;
			return;
		}
		if( pic_mbuffer.shaderkey )
		{
			pic_mbuffer.infokey = -1;
			R_RenderMeshBuffer( &pic_mbuffer );
		}
		oldframe = glState.draw_frame;
	}
}

/*
=============
R_DrawStretchRaw
=============
*/
void R_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, const byte *data, bool redraw )
{
	if( !GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
	{
		int	width = 1, height = 1;
	
		// check the dimensions
		while( width < cols ) width <<= 1;
		while( height < rows ) height <<= 1;

		if( cols != width || rows != height )
			Host_Error( "R_DrawStretchRaw: size is not a power of two (%i x %i)\n", cols, rows );
	}

	if( cols > glConfig.max_2d_texture_size )
		Host_Error( "R_DrawStretchRaw: size exceeds hardware limits (%i > %i)\n", cols, glConfig.max_2d_texture_size ); 
	if( rows > glConfig.max_2d_texture_size )
		Host_Error( "R_DrawStretchRaw: size exceeds hardware limits (%i > %i)\n", rows, glConfig.max_2d_texture_size );

	GL_Bind( 0, tr.cinTexture );

	if( cols == tr.cinTexture->width && rows == tr.cinTexture->height )
	{
		pglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
	}
	else
	{
		tr.cinTexture->width = cols;
		tr.cinTexture->height = rows;
		pglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
	}
	R_CheckForErrors();
	if( redraw ) return;

	pglBegin( GL_QUADS );
	pglTexCoord2f( 0, 0 );
	pglVertex2f( x, y );
	pglTexCoord2f( 1, 0 );
	pglVertex2f( x + w, y );
	pglTexCoord2f( 1, 1 );
	pglVertex2f( x + w, y + h );
	pglTexCoord2f( 0, 1 );
	pglVertex2f( x, y + h );
	pglEnd();
}

void R_DrawGetParms( int *w, int *h, int *f, int frame, shader_t handle )
{
	ref_shader_t	*shader;
	int		cur = 0;

	if( !w && !h && !f ) return;

	// assume error
	if( w ) *w = 0;
	if( h ) *h = 0;
	if( f ) *f = 1;

	if( handle < 0 || handle > MAX_SHADERS || !(shader = &r_shaders[handle]))
		return;

	if( !shader->num_stages || !shader->stages[0].textures[0] )
		return;

	if( shader->stages[0].textures[0] && shader->stages[0].num_textures && frame > 0 )
		cur = bound( 0, frame, shader->stages[0].num_textures );

	if( w ) *w = (int)shader->stages[0].textures[cur]->srcWidth;
	if( h ) *h = (int)shader->stages[0].textures[cur]->srcHeight;
	if( f ) *f = (int)shader->stages[0].num_textures;
}

void R_DrawSetParms( shader_t handle, kRenderMode_t rendermode, int frame )
{
	ref_shader_t	*shader;

	if( handle < 0 || handle > MAX_SHADERS || !(shader = &r_shaders[handle]) || !shader->num_stages )
		return;

	glState.draw_rendermode = rendermode;

	if( !shader->stages[0].num_textures )
		return;

	// change frame if need
	if( shader->stages[0].flags & SHADERSTAGE_FRAMES )
	{
		// make sure what frame inbound
		glState.draw_frame = bound( 0, frame, shader->stages[0].num_textures - 1 );
	}
}

void R_DrawFill( float x, float y, float w, float h )
{
	pglDisable( GL_TEXTURE_2D );
	pglColor4ubv( glState.draw_color );

	if( glState.draw_color[3] != 255 )
		GL_SetState( GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	else GL_SetState( GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ZERO );

	pglBegin( GL_QUADS );
	pglVertex2f( x, y );
	pglVertex2f( x + w, y );
	pglVertex2f( x + w, y + h );
	pglVertex2f( x, y + h );
	pglEnd();

	pglEnable( GL_TEXTURE_2D );
}