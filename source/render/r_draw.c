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
R_DrawStretchPic
===============
*/
void R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, shader_t shadernum )
{
	int		bcolor;
	ref_shader_t	*shader = &r_shaders[shadernum];	// FIXME: check bounds

	if( !shader ) return;

	// lower-left
	Vector2Set( pic_xyz[0], x, y );
	Vector2Set( pic_st[0], s1, t1 );
	Vector4Set( pic_colors[0], R_FloatToByte( glState.draw_color[0] ), R_FloatToByte( glState.draw_color[1] ), 
		R_FloatToByte( glState.draw_color[2] ), R_FloatToByte( glState.draw_color[3] ));
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

	pic_mbuffer.infokey -= 4;
	pic_mbuffer.shaderkey = shader->sortkey;

	// upload video right before rendering
	if( shader->flags & SHADER_VIDEOMAP )
		R_UploadCinematicShader( shader );

	R_PushMesh( &pic_mesh, MF_TRIFAN | shader->features | ( r_shownormals->integer ? MF_NORMALS : 0 ) );
}

/*
=============
R_DrawStretchRaw
=============
*/
void R_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, byte *data, bool redraw )
{
	int samples = 3;

	GL_Bind( 0, r_cintexture );

	R_Upload32( &data, cols, rows, TF_CINEMATIC, NULL, NULL, &samples, ( cols == r_cintexture->srcWidth && rows == r_cintexture->srcHeight ) );

	r_cintexture->srcWidth = cols;
	r_cintexture->srcHeight = rows;

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

void R_DrawSetParms( shader_t handle, kRenderMode_t rendermode, int frame )
{
}

void R_DrawGetParms( int *w, int *h, int *f, int frame, shader_t handle )
{
	ref_shader_t *shader;
	int cur = 0;

	if( !w && !h && !f ) return;

	// assume error
	if( w ) *w = 0;
	if( h ) *h = 0;
	if( f ) *f = 1;

	if( handle < 0 || handle > MAX_SHADERS || !(shader = &r_shaders[handle]))
		return;

	if( !shader->numpasses || !shader->passes[0].anim_frames[0] )
		return;

	if( shader->passes[0].anim_frames[0] && shader->passes[0].anim_numframes && frame > 0 )
		cur = bound( 0, frame, shader->passes[0].anim_numframes );

	if( w ) *w = (int)shader->passes[0].anim_frames[cur]->srcWidth;
	if( h ) *h = (int)shader->passes[0].anim_frames[cur]->srcHeight;
	if( f ) *f = (int)shader->passes[0].anim_numframes;
}

void R_DrawFill( float x, float y, float w, float h )
{
	pglDisable( GL_TEXTURE_2D );
	pglColor4fv( glState.draw_color );

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