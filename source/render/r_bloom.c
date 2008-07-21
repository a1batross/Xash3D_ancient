//=======================================================================
//			Copyright XashXT Group 2007 ©
//		     r_bloom.c - lighting post process effect
//=======================================================================

#include <assert.h>
#include "gl_local.h"

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


image_t *r_bloomscreentexture;
image_t *r_bloomeffecttexture;
image_t *r_bloombackuptexture;
image_t *r_bloomdownsamplingtexture;

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

//texture coordinates of adjusted textures
static float sampleText_tcw;
static float sampleText_tch;

//this macro is in sample size workspace coordinates
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
	byte	*data;
	rgbdata_t	r_bloom;
	
	data = Z_Malloc( width * height * 4 );

	memset(&r_bloom, 0, sizeof(rgbdata_t));

	r_bloom.width = width;
	r_bloom.height = height;
	r_bloom.type = PF_RGBA_GN;
	r_bloom.size = width * height * 4;
	r_bloom.flags = 0;
	r_bloom.numMips = 1;
	r_bloom.palette = NULL;
	r_bloom.buffer = data;

	r_screenbackuptexture_size = width;
	r_bloombackuptexture = R_LoadImage( "***r_bloombackuptexture***", &r_bloom, it_pic );
	Z_Free ( data );
}

/*
=================
R_Bloom_InitEffectTexture
=================
*/
void R_Bloom_InitEffectTexture( void )
{
	byte	*data;
	float	bloomsizecheck;
	rgbdata_t	r_bloomfx;

	memset(&r_bloomfx, 0, sizeof(rgbdata_t));
	
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

	//make sure bloom size doesn't have stupid values
	if( BLOOM_SIZE > screen_texture_width ||
		BLOOM_SIZE > screen_texture_height )
		BLOOM_SIZE = min( screen_texture_width, screen_texture_height );

	if( BLOOM_SIZE != (int)r_bloom_sample_size->value )
		Cvar_SetValue ("r_bloom_sample_size", BLOOM_SIZE);

	data = Z_Malloc( BLOOM_SIZE * BLOOM_SIZE * 4 );

	r_bloomfx.width = BLOOM_SIZE;
	r_bloomfx.height = BLOOM_SIZE;
	r_bloomfx.size = BLOOM_SIZE * BLOOM_SIZE * 4;
	r_bloomfx.type = PF_RGBA_GN;
	r_bloomfx.flags = 0;
	r_bloomfx.numMips = 1;
	r_bloomfx.palette = NULL;
	r_bloomfx.buffer = data;
	r_bloomeffecttexture = R_LoadImage( "***r_bloomeffecttexture***", &r_bloomfx, it_pic );
	
	Z_Free ( data );
}

/*
=================
R_Bloom_InitTextures
=================
*/
void R_Bloom_InitTextures( void )
{
	byte	*data;
	int	size;
	rgbdata_t	r_bloomscr, r_downsample;

	memset(&r_bloomscr, 0, sizeof(rgbdata_t));
	memset(&r_downsample, 0, sizeof(rgbdata_t));

	//find closer power of 2 to screen size 
	for (screen_texture_width = 1; screen_texture_width < r_width->integer; screen_texture_width *= 2);
	for (screen_texture_height = 1; screen_texture_height < r_height->integer; screen_texture_height *= 2);

	//init the screen texture
	size = screen_texture_width * screen_texture_height * 4;
	data = Z_Malloc( size );
	memset( data, 255, size );

	r_bloomscr.width = screen_texture_width;
	r_bloomscr.height = screen_texture_height;
	r_bloomscr.type = PF_RGBA_GN;
	r_bloomscr.flags = 0;
	r_bloomscr.palette = NULL;
	r_bloomscr.buffer = (byte *)data;
	r_bloomscr.numMips = 1;
	r_bloomscr.size = screen_texture_width * screen_texture_height * 4;
	r_bloomscreentexture = R_LoadImage( "***r_bloomscreentexture***", &r_bloomscr, it_pic );
	Z_Free ( data );

	//validate bloom size and init the bloom effect texture
	R_Bloom_InitEffectTexture ();

	//if screensize is more than 2x the bloom effect texture, set up for stepped downsampling
	r_bloomdownsamplingtexture = NULL;
	r_screendownsamplingtexture_size = 0;
	if( r_width->integer > (BLOOM_SIZE * 2) && !r_bloom_fast_sample->value )
	{
		r_screendownsamplingtexture_size = (int)(BLOOM_SIZE * 2);
		data = Z_Malloc( r_screendownsamplingtexture_size * r_screendownsamplingtexture_size * 4 );
		r_downsample.width = r_screendownsamplingtexture_size;
		r_downsample.height = r_screendownsamplingtexture_size;
		r_downsample.type = PF_RGBA_GN;
		r_downsample.size = r_screendownsamplingtexture_size * r_screendownsamplingtexture_size * 4;
		r_downsample.flags = 0;
		r_downsample.palette = NULL;
		r_downsample.buffer = (byte *)data;
		r_downsample.numMips = 1;
		r_bloomdownsamplingtexture = R_LoadImage( "***r_bloomdownsampetexture***", &r_downsample, it_pic );
		Z_Free ( data );
	}

	//Init the screen backup texture
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
	GL_Bind(r_bloomeffecttexture->texnum[0]);  
	pglEnable(GL_BLEND);
	pglBlendFunc(GL_ONE, GL_ONE);
	pglColor4f(r_bloom_alpha->value, r_bloom_alpha->value, r_bloom_alpha->value, 1.0f);
	GL_TexEnv(GL_MODULATE);
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
	
	pglDisable(GL_BLEND);
}



/*
=================
R_Bloom_GeneratexDiamonds
=================
*/
void R_Bloom_GeneratexDiamonds( void )
{
	int			i, j;
	static float intensity;

	//set up sample size workspace
	pglViewport( 0, 0, sample_width, sample_height );
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity ();
	pglOrtho(0, sample_width, sample_height, 0, -10, 100);
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity ();

	//copy small scene into r_bloomeffecttexture
	GL_Bind(r_bloomeffecttexture->texnum[0]);
	pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

	//start modifying the small scene corner
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglEnable(GL_BLEND);

	//darkening passes
	if( r_bloom_darken->value )
	{
		pglBlendFunc(GL_DST_COLOR, GL_ZERO);
		GL_TexEnv(GL_MODULATE);
		
		for(i=0; i<r_bloom_darken->value ;i++) {
			R_Bloom_SamplePass( 0, 0 );
		}
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);
	}

	//bluring passes
	//pglBlendFunc(GL_ONE, GL_ONE);
	pglBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
	
	if( r_bloom_diamond_size->value > 7 || r_bloom_diamond_size->value <= 3)
	{
		if( (int)r_bloom_diamond_size->value != 8 ) Cvar_SetValue( "r_bloom_diamond_size", 8 );

		for(i=0; i<r_bloom_diamond_size->value; i++) {
			for(j=0; j<r_bloom_diamond_size->value; j++) {
				intensity = r_bloom_intensity->value * 0.3 * Diamond8x[i][j];
				if( intensity < 0.01f ) continue;
				pglColor4f( intensity, intensity, intensity, 1.0);
				R_Bloom_SamplePass( i-4, j-4 );
			}
		}
	} else if( r_bloom_diamond_size->value > 5 ) {
		
		if( r_bloom_diamond_size->value != 6 ) Cvar_SetValue( "r_bloom_diamond_size", 6 );

		for(i=0; i<r_bloom_diamond_size->value; i++) {
			for(j=0; j<r_bloom_diamond_size->value; j++) {
				intensity = r_bloom_intensity->value * 0.5 * Diamond6x[i][j];
				if( intensity < 0.01f ) continue;
				pglColor4f( intensity, intensity, intensity, 1.0);
				R_Bloom_SamplePass( i-3, j-3 );
			}
		}
	} else if( r_bloom_diamond_size->value > 3 ) {

		if( (int)r_bloom_diamond_size->value != 4 ) Cvar_SetValue( "r_bloom_diamond_size", 4 );

		for(i=0; i<r_bloom_diamond_size->value; i++) {
			for(j=0; j<r_bloom_diamond_size->value; j++) {
				intensity = r_bloom_intensity->value * 0.8f * Diamond4x[i][j];
				if( intensity < 0.01f ) continue;
				pglColor4f( intensity, intensity, intensity, 1.0);
				R_Bloom_SamplePass( i-2, j-2 );
			}
		}
	}
	
	pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

	//restore full screen workspace
	pglViewport( 0, 0, r_width->integer, r_height->integer );
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity ();
	pglOrtho(0, r_width->integer, r_height->integer, 0, -10, 100);
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity ();
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

	//stepped downsample
	if( r_screendownsamplingtexture_size )
	{
		int		midsample_width = r_screendownsamplingtexture_size * sampleText_tcw;
		int		midsample_height = r_screendownsamplingtexture_size * sampleText_tch;
		
		//copy the screen and draw resized
		GL_Bind(r_bloomscreentexture->texnum[0]);
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, curView_x, r_height->integer - (curView_y + curView_height), curView_width, curView_height);
		R_Bloom_Quad( 0,  r_height->integer-midsample_height, midsample_width, midsample_height, screenText_tcw, screenText_tch  );
		
		//now copy into Downsampling (mid-sized) texture
		GL_Bind(r_bloomdownsamplingtexture->texnum[0]);
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, midsample_width, midsample_height);

		//now draw again in bloom size
		pglColor4f( 0.5f, 0.5f, 0.5f, 1.0f );
		R_Bloom_Quad( 0,  r_height->integer-sample_height, sample_width, sample_height, sampleText_tcw, sampleText_tch );
		
		//now blend the big screen texture into the bloom generation space (hoping it adds some blur)
		pglEnable( GL_BLEND );
		pglBlendFunc(GL_ONE, GL_ONE);
		pglColor4f( 0.5f, 0.5f, 0.5f, 1.0f );
		GL_Bind(r_bloomscreentexture->texnum[0]);
		R_Bloom_Quad( 0,  r_height->integer-sample_height, sample_width, sample_height, screenText_tcw, screenText_tch );
		pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		pglDisable( GL_BLEND );

	} else {	//downsample simple

		GL_Bind(r_bloomscreentexture->texnum[0]);
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, curView_x, r_height->integer - (curView_y + curView_height), curView_width, curView_height);
		R_Bloom_Quad( 0, r_height->integer-sample_height, sample_width, sample_height, screenText_tcw, screenText_tch );
	}
}

/*
=================
R_BloomBlend
=================
*/
void R_SetupGL (void);
void R_BloomBlend ( refdef_t *fd )
{
	if(!r_bloom->value ) return;

	if( screen_texture_width < BLOOM_SIZE || screen_texture_height < BLOOM_SIZE )
		return;
	
	//set up full screen workspace
	pglViewport( 0, 0, r_width->integer, r_height->integer );
	pglDisable( GL_DEPTH_TEST );
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity ();
	pglOrtho(0, r_width->integer, r_height->integer, 0, -10, 100);
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity ();
	pglDisable(GL_CULL_FACE);
	pglDepthMask(false);
	pglDisable( GL_BLEND );
	pglEnable( GL_TEXTURE_2D );

	pglColor4f( 1, 1, 1, 1 );

	//set up current sizes
	curView_x = fd->x;
	curView_y = fd->y;
	curView_width = fd->width;
	curView_height = fd->height;
	screenText_tcw = ((float)fd->width / (float)screen_texture_width);
	screenText_tch = ((float)fd->height / (float)screen_texture_height);
	if( fd->height > fd->width )
	{
		sampleText_tcw = ((float)fd->width / (float)fd->height);
		sampleText_tch = 1.0f;
	}
	else
	{
		sampleText_tcw = 1.0f;
		sampleText_tch = ((float)fd->height / (float)fd->width);
	}
	sample_width = BLOOM_SIZE * sampleText_tcw;
	sample_height = BLOOM_SIZE * sampleText_tch;
	
	//copy the screen space we'll use to work into the backup texture
	GL_Bind(r_bloombackuptexture->texnum[0]);
	pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, r_screenbackuptexture_size * sampleText_tcw, r_screenbackuptexture_size * sampleText_tch);

	//create the bloom image
	R_Bloom_DownsampleView();
	R_Bloom_GeneratexDiamonds();

	//restore the screen-backup to the screen
	pglDisable(GL_BLEND);
	GL_Bind(r_bloombackuptexture->texnum[0]);
	pglColor4f( 1, 1, 1, 1 );
	R_Bloom_Quad( 0, 
		r_height->integer - (r_screenbackuptexture_size * sampleText_tch),
		r_screenbackuptexture_size * sampleText_tcw,
		r_screenbackuptexture_size * sampleText_tch,
		sampleText_tcw, sampleText_tch );

	R_Bloom_DrawEffect();
	
	pglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	pglEnable (GL_TEXTURE_2D);
	pglEnable( GL_DEPTH_TEST );
	pglColor4f(1,1,1,1);
	pglDepthMask(true);
}


