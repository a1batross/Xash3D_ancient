/*
Copyright (C) Forest Hale
Copyright (C) 2006-2007 German Garcia

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

// r_bloom.c: 2D lighting post process effect

#include "r_local.h"

/*
==============================================================================

LIGHT BLOOMS

==============================================================================
*/

static float Diamond8x[8][8] =
{
	{ 0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f, },
	{ 0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f, },
	{ 0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f, },
	{ 0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f, },
	{ 0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f, },
	{ 0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f, },
	{ 0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f, },
	{ 0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f }
};

static float Diamond6x[6][6] =
{
	{ 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, },
	{ 0.0f, 0.3f, 0.5f, 0.5f, 0.3f, 0.0f, },
	{ 0.1f, 0.5f, 0.9f, 0.9f, 0.5f, 0.1f, },
	{ 0.1f, 0.5f, 0.9f, 0.9f, 0.5f, 0.1f, },
	{ 0.0f, 0.3f, 0.5f, 0.5f, 0.3f, 0.0f, },
	{ 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f }
};

static float Diamond4x[4][4] =
{
	{ 0.3f, 0.4f, 0.4f, 0.3f, },
	{ 0.4f, 0.9f, 0.9f, 0.4f, },
	{ 0.4f, 0.9f, 0.9f, 0.4f, },
	{ 0.3f, 0.4f, 0.4f, 0.3f }
};

static int BLOOM_SIZE;

static texture_t *r_bloomscreentexture;
static texture_t *r_bloomeffecttexture;
static texture_t *r_bloombackuptexture;
static texture_t *r_bloomdownsamplingtexture;

static int r_screendownsamplingtexture_size;
static int screen_texture_width, screen_texture_height;
static int r_screenbackuptexture_width, r_screenbackuptexture_height;

// current refdef size:
static int curView_x;
static int curView_y;
static int curView_width;
static int curView_height;

// texture coordinates of screen data inside screentexture
static float screenTex_tcw;
static float screenTex_tch;

static int sample_width;
static int sample_height;

// texture coordinates of adjusted textures
static float sampleText_tcw;
static float sampleText_tch;

/*
=================
R_Bloom_InitBackUpTexture
=================
*/
static void R_Bloom_InitBackUpTexture( int width, int height )
{
	rgbdata_t	r_bloom;

	Mem_Set( &r_bloom, 0, sizeof( rgbdata_t ));
	r_screenbackuptexture_width = width;
	r_screenbackuptexture_height = height;

	r_bloom.width = width;
	r_bloom.height = height;
	r_bloom.type = PF_RGBA_GN;
	r_bloom.size = width * height * 4;
	r_bloom.depth = r_bloom.numMips = 1;
	r_bloom.flags = 0;
	r_bloom.palette = NULL;
	r_bloom.buffer = Mem_Alloc( r_temppool, width * height * 4 );

	r_bloombackuptexture = R_LoadTexture( "***r_bloombackuptexture***", &r_bloom, 3, TF_STATIC|TF_UNCOMPRESSED|TF_NOPICMIP|TF_CLAMP|TF_NOMIPMAP );
	Mem_Free( r_bloom.buffer );
}

/*
=================
R_Bloom_InitEffectTexture
=================
*/
static void R_Bloom_InitEffectTexture( void )
{
	int	limit;
	rgbdata_t	r_bloomfx;

	Mem_Set( &r_bloomfx, 0, sizeof( rgbdata_t ));
	
	if( r_bloom_sample_size->integer < 32 )
		Cvar_Set( "r_bloom_sample_size", "32" );

	// make sure bloom size doesn't have stupid values
	limit = min( r_bloom_sample_size->integer, min( screen_texture_width, screen_texture_height ) );

	if( GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
		BLOOM_SIZE = limit;
	else	// make sure bloom size is a power of 2
		for( BLOOM_SIZE = 32; (BLOOM_SIZE<<1) <= limit; BLOOM_SIZE <<= 1 );

	if( BLOOM_SIZE != r_bloom_sample_size->integer )
		Cvar_Set( "r_bloom_sample_size", va( "%i", BLOOM_SIZE ) );

	r_bloomfx.width = BLOOM_SIZE;
	r_bloomfx.height = BLOOM_SIZE;
	r_bloomfx.size = BLOOM_SIZE * BLOOM_SIZE * 4;
	r_bloomfx.type = PF_RGBA_GN;
	r_bloomfx.flags = 0;
	r_bloomfx.numMips = r_bloomfx.depth = 1;
	r_bloomfx.palette = NULL;
	r_bloomfx.buffer = Mem_Alloc( r_temppool, BLOOM_SIZE * BLOOM_SIZE * 4 );

	r_bloomeffecttexture = R_LoadTexture( "***r_bloomeffecttexture***", &r_bloomfx, 3, TF_STATIC|TF_UNCOMPRESSED|TF_NOPICMIP|TF_CLAMP|TF_NOMIPMAP );
	Mem_Free( r_bloomfx.buffer );
}

/*
=================
R_Bloom_InitTextures
=================
*/
static void R_Bloom_InitTextures( void )
{
	int	size;
	rgbdata_t	r_bloomscr, r_downsample;

	Mem_Set( &r_bloomscr, 0, sizeof( rgbdata_t ));
	Mem_Set( &r_downsample, 0, sizeof( rgbdata_t ));

	if( GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
	{
		screen_texture_width = glState.width;
		screen_texture_height = glState.height;
	}
	else
	{
		// find closer power of 2 to screen size 
		for (screen_texture_width = 1;screen_texture_width < glState.width;screen_texture_width <<= 1);
		for (screen_texture_height = 1;screen_texture_height < glState.height;screen_texture_height <<= 1);
	}

	// disable blooms if we can't handle a texture of that size
	if( screen_texture_width > glConfig.max_2d_texture_size || screen_texture_height > glConfig.max_2d_texture_size )
	{
		screen_texture_width = screen_texture_height = 0;
		Cvar_Set( "r_bloom", "0" );
		MsgDev( D_WARN, "'R_InitBloomScreenTexture' too high resolution for light bloom, effect disabled\n" );
		return;
	}

	// init the screen texture
	size = screen_texture_width * screen_texture_height * 4;

	r_bloomscr.width = screen_texture_width;
	r_bloomscr.height = screen_texture_height;
	r_bloomscr.type = PF_RGBA_GN;
	r_bloomscr.flags = 0;
	r_bloomscr.palette = NULL;
	r_bloomscr.numMips = r_bloomscr.depth = 1;
	r_bloomscr.size = size;
	r_bloomscr.buffer = Mem_Alloc( r_temppool, size );
	Mem_Set( r_bloomscr.buffer, 255, size );

	r_bloomscreentexture = R_LoadTexture( "***r_bloomscreentexture***", &r_bloomscr, 3, TF_STATIC|TF_UNCOMPRESSED|TF_NOPICMIP|TF_CLAMP|TF_NOMIPMAP );
	Mem_Free( r_bloomscr.buffer );

	// validate bloom size and init the bloom effect texture
	R_Bloom_InitEffectTexture();

	// if screensize is more than 2x the bloom effect texture, set up for stepped downsampling
	r_bloomdownsamplingtexture = NULL;
	r_screendownsamplingtexture_size = 0;

	if(( glState.width > (BLOOM_SIZE * 2) || glState.height > (BLOOM_SIZE * 2)) && !r_bloom_fast_sample->integer )
	{
		r_screendownsamplingtexture_size = (int)( BLOOM_SIZE * 2 );
		r_downsample.width = r_screendownsamplingtexture_size;
		r_downsample.height = r_screendownsamplingtexture_size;
		r_downsample.type = PF_RGBA_GN;
		r_downsample.size = r_screendownsamplingtexture_size * r_screendownsamplingtexture_size * 4;
		r_downsample.flags = 0;
		r_downsample.palette = NULL;
		r_downsample.buffer = Mem_Alloc( r_temppool, r_downsample.size );
		r_downsample.numMips = r_downsample.depth = 1;

		r_bloomdownsamplingtexture = R_LoadTexture( "***r_bloomdownsamplingtexture***", &r_downsample, 3, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED|TF_CLAMP|TF_NOMIPMAP );
		Mem_Free( r_downsample.buffer );
	}

	// init the screen backup texture
	if( r_screendownsamplingtexture_size )
		R_Bloom_InitBackUpTexture( r_screendownsamplingtexture_size, r_screendownsamplingtexture_size );
	else R_Bloom_InitBackUpTexture( BLOOM_SIZE, BLOOM_SIZE );
}

/*
=================
R_InitBloomTextures
=================
*/
void R_InitBloomTextures( void )
{
	BLOOM_SIZE = 0;
	if( !r_bloom->integer )
		return;
	R_Bloom_InitTextures();
}

/*
=================
R_Bloom_SamplePass
=================
*/
static _inline void R_Bloom_SamplePass( int xpos, int ypos )
{
	pglBegin( GL_QUADS );
	pglTexCoord2f( 0, sampleText_tch );
	pglVertex2f( xpos, ypos );
	pglTexCoord2f( 0, 0 );
	pglVertex2f( xpos, ypos+sample_height );
	pglTexCoord2f( sampleText_tcw, 0 );
	pglVertex2f( xpos+sample_width, ypos+sample_height );
	pglTexCoord2f( sampleText_tcw, sampleText_tch );
	pglVertex2f( xpos+sample_width, ypos );
	pglEnd();
}

/*
=================
R_Bloom_Quad
=================
*/
static _inline void R_Bloom_Quad( int x, int y, int w, int h, float texwidth, float texheight )
{
	pglBegin( GL_QUADS );
	pglTexCoord2f( 0, texheight );
	pglVertex2f( x, glState.height-h-y );
	pglTexCoord2f( 0, 0 );
	pglVertex2f( x, glState.height-y );
	pglTexCoord2f( texwidth, 0 );
	pglVertex2f( x+w, glState.height-y );
	pglTexCoord2f( texwidth, texheight );
	pglVertex2f( x+w, glState.height-h );
	pglEnd();
}

/*
=================
R_Bloom_DrawEffect
=================
*/
static void R_Bloom_DrawEffect( void )
{
	GL_Bind( 0, r_bloomeffecttexture );
	GL_TexEnv( GL_MODULATE );
	GL_SetState( GLSTATE_NO_DEPTH_TEST|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE );
	pglColor4f( r_bloom_alpha->value, r_bloom_alpha->value, r_bloom_alpha->value, 1.0f );

	pglBegin( GL_QUADS );
	pglTexCoord2f( 0, sampleText_tch );
	pglVertex2f( curView_x, curView_y );
	pglTexCoord2f( 0, 0 );
	pglVertex2f( curView_x, curView_y+curView_height );
	pglTexCoord2f( sampleText_tcw, 0 );
	pglVertex2f( curView_x+curView_width, curView_y+curView_height );
	pglTexCoord2f( sampleText_tcw, sampleText_tch );
	pglVertex2f( curView_x+curView_width, curView_y );
	pglEnd();
}

/*
=================
R_Bloom_GeneratexDiamonds
=================
*/
static void R_Bloom_GeneratexDiamonds( void )
{
	int i, j, k;
	float intensity, scale, *diamond;

	// set up sample size workspace
	pglScissor( 0, 0, sample_width, sample_height );
	pglViewport( 0, 0, sample_width, sample_height );
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();
	pglOrtho( 0, sample_width, sample_height, 0, -10, 100 );
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	// copy small scene into r_bloomeffecttexture
	GL_Bind( 0, r_bloomeffecttexture );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height );

	// start modifying the small scene corner
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	// darkening passes
	if( r_bloom_darken->integer )
	{
		GL_TexEnv( GL_MODULATE );
		GL_SetState( GLSTATE_NO_DEPTH_TEST|GLSTATE_SRCBLEND_DST_COLOR|GLSTATE_DSTBLEND_ZERO );

		for( i = 0; i < r_bloom_darken->integer; i++ )
			R_Bloom_SamplePass( 0, 0 );
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height );
	}

	// bluring passes
	GL_SetState( GLSTATE_NO_DEPTH_TEST|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR );

	if( r_bloom_diamond_size->integer > 7 || r_bloom_diamond_size->integer <= 3 )
	{
		if( r_bloom_diamond_size->integer != 8 )
			Cvar_Set( "r_bloom_diamond_size", "8" );
	}
	else if( r_bloom_diamond_size->integer > 5 )
	{
		if( r_bloom_diamond_size->integer != 6 )
			Cvar_Set( "r_bloom_diamond_size", "6" );
	}
	else if( r_bloom_diamond_size->integer > 3 )
	{
		if( r_bloom_diamond_size->integer != 4 )
			Cvar_Set( "r_bloom_diamond_size", "4" );
	}

	switch( r_bloom_diamond_size->integer )
	{
	case 4:
		k = 2;
		diamond = &Diamond4x[0][0];
		scale = r_bloom_intensity->value * 0.8f;
		break;
	case 6:
		k = 3;
		diamond = &Diamond6x[0][0];
		scale = r_bloom_intensity->value * 0.5f;
		break;
	default:
//	case 8:
		k = 4;
		diamond = &Diamond8x[0][0];
		scale = r_bloom_intensity->value * 0.3f;
		break;
	}

	for( i = 0; i < r_bloom_diamond_size->integer; i++ )
	{
		for( j = 0; j < r_bloom_diamond_size->integer; j++, diamond++ )
		{
			intensity =  *diamond * scale;
			if( intensity < 0.01f )
				continue;

			pglColor4f( intensity, intensity, intensity, 1.0 );
			R_Bloom_SamplePass( i - k, j - k );
		}
	}

	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height );

	// restore full screen workspace
	pglScissor( 0, 0, glState.width, glState.height );
	pglViewport( 0, 0, glState.width, glState.height );
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();
	pglOrtho( 0, glState.width, glState.height, 0, -10, 100 );
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();
}

/*
=================
R_Bloom_DownsampleView
=================
*/
static void R_Bloom_DownsampleView( void )
{
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	if( r_screendownsamplingtexture_size )
	{
		// stepped downsample
		int midsample_width = ( r_screendownsamplingtexture_size * sampleText_tcw );
		int midsample_height = ( r_screendownsamplingtexture_size * sampleText_tch );

		// copy the screen and draw resized
		GL_Bind( 0, r_bloomscreentexture );
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, curView_x, glState.height - ( curView_y + curView_height ), curView_width, curView_height );
		R_Bloom_Quad( 0, 0, midsample_width, midsample_height, screenTex_tcw, screenTex_tch );

		// now copy into downsampling (mid-sized) texture
		GL_Bind( 0, r_bloomdownsamplingtexture );
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, midsample_width, midsample_height );

		// now draw again in bloom size
		pglColor4f( 0.5f, 0.5f, 0.5f, 1.0f );
		R_Bloom_Quad( 0, 0, sample_width, sample_height, sampleText_tcw, sampleText_tch );

		// now blend the big screen texture into the bloom generation space (hoping it adds some blur)
		GL_SetState( GLSTATE_NO_DEPTH_TEST|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE );

		GL_Bind( 0, r_bloomscreentexture );
		R_Bloom_Quad( 0, 0, sample_width, sample_height, screenTex_tcw, screenTex_tch );
		pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	}
	else
	{
		// downsample simple
		GL_Bind( 0, r_bloomscreentexture );
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, curView_x, glState.height - ( curView_y + curView_height ), curView_width, curView_height );
		R_Bloom_Quad( 0, 0, sample_width, sample_height, screenTex_tcw, screenTex_tch );
	}
}

/*
=================
R_BloomBlend
=================
*/
void R_BloomBlend( const ref_params_t *fd )
{
	if( !r_bloom->integer )
		return;

	if( !BLOOM_SIZE )
		R_Bloom_InitTextures();

	if( screen_texture_width < BLOOM_SIZE || screen_texture_height < BLOOM_SIZE )
		return;

	// set up full screen workspace
	pglScissor( 0, 0, glState.width, glState.height );
	pglViewport( 0, 0, glState.width, glState.height );
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();
	pglOrtho( 0, glState.width, glState.height, 0, -10, 100 );
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	GL_Cull( 0 );
	GL_SetState( GLSTATE_NO_DEPTH_TEST );

	pglColor4f( 1, 1, 1, 1 );

	// set up current sizes
	curView_x = fd->viewport[0];
	curView_y = fd->viewport[1];
	curView_width = fd->viewport[2];
	curView_height = fd->viewport[3];

	screenTex_tcw = ( (float)curView_width / (float)screen_texture_width );
	screenTex_tch = ( (float)curView_height / (float)screen_texture_height );
	if( curView_height > curView_width )
	{
		sampleText_tcw = ( (float)curView_width / (float)curView_height );
		sampleText_tch = 1.0f;
	}
	else
	{
		sampleText_tcw = 1.0f;
		sampleText_tch = ( (float)curView_height / (float)curView_width );
	}

	sample_width = ( BLOOM_SIZE * sampleText_tcw );
	sample_height = ( BLOOM_SIZE * sampleText_tch );

	// copy the screen space we'll use to work into the backup texture
	GL_Bind( 0, r_bloombackuptexture );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, r_screenbackuptexture_width, r_screenbackuptexture_height );

	// create the bloom image
	R_Bloom_DownsampleView();
	R_Bloom_GeneratexDiamonds();

	// restore the screen-backup to the screen
	GL_SetState( GLSTATE_NO_DEPTH_TEST );
	GL_Bind( 0, r_bloombackuptexture );

	pglColor4f( 1, 1, 1, 1 );

	R_Bloom_Quad( 0, 0, r_screenbackuptexture_width, r_screenbackuptexture_height, 1.0f, 1.0f );

	pglScissor( RI.scissor[0], RI.scissor[1], RI.scissor[2], RI.scissor[3] );

	R_Bloom_DrawEffect();

	pglViewport( RI.viewport[0], RI.viewport[1], RI.viewport[2], RI.viewport[3] );
}
