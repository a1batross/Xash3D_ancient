//=======================================================================
//			Copyright XashXT Group 2007 ©
//		     r_bloom.c - lighting post process effect
//=======================================================================

#include "r_local.h"
#include "mathlib.h"

static float Diamond8x[8][8] =
{ 
		0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f, 
		0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f, 
		0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f, 
		0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f, 
		0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f, 
		0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f, 
		0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f, 
		0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f
};

static float Diamond6x[6][6] =
{ 
		0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 
		0.0f, 0.3f, 0.5f, 0.5f, 0.3f, 0.0f,  
		0.1f, 0.5f, 0.9f, 0.9f, 0.5f, 0.1f, 
		0.1f, 0.5f, 0.9f, 0.9f, 0.5f, 0.1f, 
		0.0f, 0.3f, 0.5f, 0.5f, 0.3f, 0.0f, 
		0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f
};

static float Diamond4x[4][4] =
{  
		0.3f, 0.4f, 0.4f, 0.3f,  
		0.4f, 0.9f, 0.9f, 0.4f, 
		0.4f, 0.9f, 0.9f, 0.4f, 
		0.3f, 0.4f, 0.4f, 0.3f
};


static int BLOOM_SIZE;


texture_t *r_bloomscreentexture;
texture_t *r_bloomeffecttexture;
texture_t *r_bloombackuptexture;
texture_t *r_bloomdownsamplingtexture;

static int r_screendownsamplingtexture_size;
static int screen_texture_width, screen_texture_height;
static int r_screenbackuptexture_size;

//current refdef size:
static int curView_x;
static int curView_y;
static int curView_width;
static int curView_height;

//texture coordinates of screen data inside screentexture
static float screenText_tcw;
static float screenText_tch;

static int sample_width;
static int sample_height;

// texture coordinates of adjusted textures
static float sampleText_tcw;
static float sampleText_tch;

// this macro is in sample size workspace coordinates
#define R_Bloom_SamplePass( xpos, ypos )\
	pglBegin(GL_QUADS);\
	pglTexCoord2f( 0, sampleText_tch);\
	pglVertex2f( xpos, ypos);\
	pglTexCoord2f( 0, 0);\
	pglVertex2f( xpos, ypos+sample_height);\
	pglTexCoord2f( sampleText_tcw, 0); \
	pglVertex2f( xpos+sample_width, ypos+sample_height);\
	pglTexCoord2f(sampleText_tcw, sampleText_tch);\
	pglVertex2f(xpos+sample_width, ypos);\
	pglEnd();
	
#define R_Bloom_Quad( x, y, width, height, textwidth, textheight )\
	pglBegin(GL_QUADS);\
	pglTexCoord2f( 0, textheight);\
	pglVertex2f( x, y);\
	pglTexCoord2f( 0, 0);\
	pglVertex2f( x, y+height);\
	pglTexCoord2f( textwidth, 0);\
	pglVertex2f( x+width, y+height);\
	pglTexCoord2f( textwidth, textheight);\
	pglVertex2f( x+width, y);\
	pglEnd();



/*
=================
R_Bloom_InitBackUpTexture
=================
*/
void R_Bloom_InitBackUpTexture( int width, int height )
{
	rgbdata_t	r_bloom;

	Mem_Set( &r_bloom, 0, sizeof( rgbdata_t ));
	r_bloom.width = width;
	r_bloom.height = height;
	r_bloom.type = PF_RGBA_GN;
	r_bloom.size = width * height * 4;
	r_bloom.flags = 0;
	r_bloom.numMips = 1;
	r_bloom.palette = NULL;
	r_bloom.buffer = r_framebuffer;

	r_screenbackuptexture_size = width;
	r_bloombackuptexture = R_LoadTexture( "*r_bloombackuptexture", &r_bloom, 3, TF_STATIC|TF_NOPICMIP, TF_LINEAR, TW_CLAMP );
}

/*
=================
R_Bloom_InitEffectTexture
=================
*/
void R_Bloom_InitEffectTexture( void )
{
	float	bloomsizecheck;
	rgbdata_t	r_bloomfx;

	Mem_Set(&r_bloomfx, 0, sizeof(rgbdata_t));
	
	if( (int)r_bloom_sample_size->value < 32 )
		Cvar_SetValue ("r_bloom_sample_size", 32);

	//make sure bloom size is a power of 2
	BLOOM_SIZE = (int)r_bloom_sample_size->value;
	bloomsizecheck = (float)BLOOM_SIZE;
	while(bloomsizecheck > 1.0f) bloomsizecheck /= 2.0f;
	if( bloomsizecheck != 1.0f )
	{
		BLOOM_SIZE = 32;
		while( BLOOM_SIZE < (int)r_bloom_sample_size->value )
			BLOOM_SIZE *= 2;
	}

	// make sure bloom size doesn't have stupid values
	if( BLOOM_SIZE > screen_texture_width || BLOOM_SIZE > screen_texture_height )
		BLOOM_SIZE = min( screen_texture_width, screen_texture_height );

	if( BLOOM_SIZE != (int)r_bloom_sample_size->value )
		Cvar_SetValue ("r_bloom_sample_size", BLOOM_SIZE);

	r_bloomfx.width = BLOOM_SIZE;
	r_bloomfx.height = BLOOM_SIZE;
	r_bloomfx.size = BLOOM_SIZE * BLOOM_SIZE * 4;
	r_bloomfx.type = PF_RGBA_GN;
	r_bloomfx.flags = 0;
	r_bloomfx.numMips = 1;
	r_bloomfx.palette = NULL;
	r_bloomfx.buffer = r_framebuffer;
	r_bloomeffecttexture = R_LoadTexture( "*r_bloomeffecttexture", &r_bloomfx, 3, TF_STATIC|TF_NOPICMIP, TF_LINEAR, TW_REPEAT );
}

/*
=================
R_Bloom_InitTextures
=================
*/
void R_Bloom_InitTextures( void )
{
	int	size;
	rgbdata_t	r_bloomscr, r_downsample;

	Mem_Set( &r_bloomscr, 0, sizeof( rgbdata_t ));
	Mem_Set( &r_downsample, 0, sizeof( rgbdata_t ));

	// find closer power of 2 to screen size 
	screen_texture_width = NearestPOW( r_width->integer, true );
	screen_texture_height = NearestPOW( r_height->integer, true );

	// init the screen texture
	size = screen_texture_width * screen_texture_height * 4;
	Mem_Set( r_framebuffer, 255, size );

	r_bloomscr.width = screen_texture_width;
	r_bloomscr.height = screen_texture_height;
	r_bloomscr.type = PF_RGBA_GN;
	r_bloomscr.flags = 0;
	r_bloomscr.palette = NULL;
	r_bloomscr.buffer = r_framebuffer;
	r_bloomscr.numMips = 1;
	r_bloomscr.size = screen_texture_width * screen_texture_height * 4;
	r_bloomscreentexture = R_LoadTexture( "*r_bloomscreentexture", &r_bloomscr, 3, TF_STATIC|TF_NOPICMIP, TF_LINEAR, TW_CLAMP );

	// validate bloom size and init the bloom effect texture
	R_Bloom_InitEffectTexture ();

	// if screensize is more than 2x the bloom effect texture, set up for stepped downsampling
	r_bloomdownsamplingtexture = NULL;
	r_screendownsamplingtexture_size = 0;
	if( r_width->integer > (BLOOM_SIZE * 2) && !r_bloom_fast_sample->value )
	{
		r_screendownsamplingtexture_size = (int)(BLOOM_SIZE * 2);
		r_downsample.width = r_screendownsamplingtexture_size;
		r_downsample.height = r_screendownsamplingtexture_size;
		r_downsample.type = PF_RGBA_GN;
		r_downsample.size = r_screendownsamplingtexture_size * r_screendownsamplingtexture_size * 4;
		r_downsample.flags = 0;
		r_downsample.palette = NULL;
		r_downsample.buffer = r_framebuffer;
		r_downsample.numMips = 1;
		r_bloomdownsamplingtexture = R_LoadTexture( "*r_bloomdownsampetexture", &r_downsample, 3, TF_STATIC|TF_NOPICMIP, TF_LINEAR, TW_CLAMP );
	}

	// Init the screen backup texture
	if( r_screendownsamplingtexture_size )
		R_Bloom_InitBackUpTexture( r_screendownsamplingtexture_size, r_screendownsamplingtexture_size );
	else R_Bloom_InitBackUpTexture( BLOOM_SIZE, BLOOM_SIZE );
	
}


/*
=================
R_Bloom_DrawEffect
=================
*/
void R_Bloom_DrawEffect( void )
{
	GL_BindTexture( r_bloomeffecttexture );  
	GL_Enable( GL_BLEND );
	GL_BlendFunc( GL_ONE, GL_ONE );
	pglColor4f(r_bloom_alpha->value, r_bloom_alpha->value, r_bloom_alpha->value, 1.0f);
	GL_TexEnv( GL_MODULATE );
	pglBegin(GL_QUADS);							
	pglTexCoord2f(	0,			sampleText_tch	);	
	pglVertex2f(	curView_x,		curView_y		);				
	pglTexCoord2f(	0,			0		);				
	pglVertex2f(	curView_x,		curView_y + curView_height	);	
	pglTexCoord2f(	sampleText_tcw,		0		);				
	pglVertex2f(	curView_x + curView_width,	curView_y + curView_height	);	
	pglTexCoord2f(	sampleText_tcw,		sampleText_tch	);	
	pglVertex2f(	curView_x + curView_width,	curView_y		);				
	pglEnd();
	
	GL_Disable( GL_BLEND );
}

/*
=================
R_Bloom_GeneratexDiamonds
=================
*/
void R_Bloom_GeneratexDiamonds( void )
{
	int		i, j;
	static float	intensity;

	//set up sample size workspace
	pglViewport( 0, 0, sample_width, sample_height );
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity ();
	pglOrtho( 0, sample_width, sample_height, 0, -10, 100 );
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity ();

	// copy small scene into r_bloomeffecttexture
	GL_BindTexture( r_bloomeffecttexture );
	pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

	// start modifying the small scene corner
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	GL_Enable( GL_BLEND );

	// darkening passes
	if( r_bloom_darken->value )
	{
		GL_BlendFunc( GL_DST_COLOR, GL_ZERO );
		GL_TexEnv( GL_MODULATE );
		
		for( i = 0; i < r_bloom_darken->value; i++ )
		{
			R_Bloom_SamplePass( 0, 0 );
		}
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height );
	}

	// bluring passes
	GL_BlendFunc( GL_ONE, GL_ONE );
	
	if( r_bloom_diamond_size->value > 7 || r_bloom_diamond_size->value <= 3)
	{
		if( r_bloom_diamond_size->integer != 8 ) Cvar_SetValue( "r_bloom_diamond_size", 8 );

		for( i = 0; i < r_bloom_diamond_size->value; i++ )
		{
			for( j = 0; j < r_bloom_diamond_size->value; j++ )
			{
				intensity = r_bloom_intensity->value * 0.3 * Diamond8x[i][j];
				if( intensity < 0.01f ) continue;
				pglColor4f( intensity, intensity, intensity, 1.0);
				R_Bloom_SamplePass( i-4, j-4 );
			}
		}
	}
	else if( r_bloom_diamond_size->value > 5 )
	{
		if( r_bloom_diamond_size->value != 6 ) Cvar_SetValue( "r_bloom_diamond_size", 6 );

		for( i = 0; i < r_bloom_diamond_size->value; i++ )
		{
			for( j = 0; j < r_bloom_diamond_size->value; j++ )
			{
				intensity = r_bloom_intensity->value * 0.5 * Diamond6x[i][j];
				if( intensity < 0.01f ) continue;
				pglColor4f( intensity, intensity, intensity, 1.0 );
				R_Bloom_SamplePass( i-3, j-3 );
			}
		}
	}
	else if( r_bloom_diamond_size->value > 3 )
	{
		if( (int)r_bloom_diamond_size->value != 4 ) Cvar_SetValue( "r_bloom_diamond_size", 4 );

		for( i = 0; i < r_bloom_diamond_size->value; i++ )
		{
			for( j = 0; j < r_bloom_diamond_size->value; j++ )
			{
				intensity = r_bloom_intensity->value * 0.8f * Diamond4x[i][j];
				if( intensity < 0.01f ) continue;
				pglColor4f( intensity, intensity, intensity, 1.0 );
				R_Bloom_SamplePass( i-2, j-2 );
			}
		}
	}
	
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height );

	// restore full screen workspace
	pglViewport( 0, 0, r_width->integer, r_height->integer );
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();
	pglOrtho( 0, r_width->integer, r_height->integer, 0, -10, 100 );
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();
}											

/*
=================
R_Bloom_DownsampleView
=================
*/
void R_Bloom_DownsampleView( void )
{
	pglDisable( GL_BLEND );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	// stepped downsample
	if( r_screendownsamplingtexture_size )
	{
		int	midsample_width = r_screendownsamplingtexture_size * sampleText_tcw;
		int	midsample_height = r_screendownsamplingtexture_size * sampleText_tch;
		
		//copy the screen and draw resized
		GL_BindTexture( r_bloomscreentexture );
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, curView_x, r_height->integer - (curView_y + curView_height), curView_width, curView_height);
		R_Bloom_Quad( 0,  r_height->integer-midsample_height, midsample_width, midsample_height, screenText_tcw, screenText_tch  );
		
		// now copy into Downsampling (mid-sized) texture
		GL_BindTexture( r_bloomdownsamplingtexture );
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, midsample_width, midsample_height );

		// now draw again in bloom size
		pglColor4f( 0.5f, 0.5f, 0.5f, 1.0f );
		R_Bloom_Quad( 0,  r_height->integer-sample_height, sample_width, sample_height, sampleText_tcw, sampleText_tch );
		
		// now blend the big screen texture into the bloom generation space (hoping it adds some blur)
		GL_Enable( GL_BLEND );
		GL_BlendFunc( GL_ONE, GL_ONE );
		pglColor4f( 0.5f, 0.5f, 0.5f, 1.0f );
		GL_BindTexture( r_bloomscreentexture );
		R_Bloom_Quad( 0,  r_height->integer-sample_height, sample_width, sample_height, screenText_tcw, screenText_tch );
		pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		GL_Disable( GL_BLEND );

	}
	else
	{
		// downsample simple
		GL_BindTexture( r_bloomscreentexture );
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, curView_x, r_height->integer - (curView_y + curView_height), curView_width, curView_height);
		R_Bloom_Quad( 0, r_height->integer-sample_height, sample_width, sample_height, screenText_tcw, screenText_tch );
	}
}

/*
=================
R_BloomBlend
=================
*/
void R_BloomBlend( const ref_params_t *fd )
{
	if( !r_bloom->value ) return;
	if( screen_texture_width < BLOOM_SIZE || screen_texture_height < BLOOM_SIZE )
		return;
	
	// set up full screen workspace
	pglViewport( 0, 0, r_width->integer, r_height->integer );
	GL_Disable( GL_DEPTH_TEST );
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity ();
	pglOrtho(0, r_width->integer, r_height->integer, 0, -10, 100);
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity ();
	GL_Disable( GL_CULL_FACE );
	GL_DepthMask( GL_FALSE );
	GL_Disable( GL_BLEND );
	pglEnable( GL_TEXTURE_2D );

	pglColor4f( 1, 1, 1, 1 );

	// set up current sizes
	curView_x = fd->viewport[0];
	curView_y = fd->viewport[1];
	curView_width = fd->viewport[2];
	curView_height = fd->viewport[3];
	screenText_tcw = ((float)fd->viewport[2] / (float)screen_texture_width);
	screenText_tch = ((float)fd->viewport[3] / (float)screen_texture_height);
	if( fd->viewport[3] > fd->viewport[2] )
	{
		sampleText_tcw = ((float)fd->viewport[2] / (float)fd->viewport[3]);
		sampleText_tch = 1.0f;
	}
	else
	{
		sampleText_tcw = 1.0f;
		sampleText_tch = ((float)fd->viewport[3] / (float)fd->viewport[2]);
	}
	sample_width = BLOOM_SIZE * sampleText_tcw;
	sample_height = BLOOM_SIZE * sampleText_tch;
	
	//copy the screen space we'll use to work into the backup texture
	GL_BindTexture( r_bloombackuptexture );
	pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, r_screenbackuptexture_size * sampleText_tcw, r_screenbackuptexture_size * sampleText_tch);

	// create the bloom image
	R_Bloom_DownsampleView();
	R_Bloom_GeneratexDiamonds();

	// restore the screen-backup to the screen
	GL_Disable( GL_BLEND );
	GL_BindTexture( r_bloombackuptexture );
	pglColor4f( 1, 1, 1, 1 );
	R_Bloom_Quad( 0, 
		r_height->integer - (r_screenbackuptexture_size * sampleText_tch),
		r_screenbackuptexture_size * sampleText_tcw,
		r_screenbackuptexture_size * sampleText_tch,
		sampleText_tcw, sampleText_tch );

	R_Bloom_DrawEffect();
	
	GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	pglEnable( GL_TEXTURE_2D );
	GL_Enable( GL_DEPTH_TEST );
	pglColor4f(1,1,1,1);
	GL_DepthMask( GL_TRUE );
}


