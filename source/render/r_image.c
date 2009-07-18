/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "r_local.h"
#include "byteorder.h"
#include "mathlib.h"

#define MAX_GLIMAGES		4096
#define IMAGES_HASH_SIZE		64

static image_t images[MAX_GLIMAGES];
image_t	*r_lightmapTextures[MAX_GLIMAGES];
static image_t *images_hash[IMAGES_HASH_SIZE];
static unsigned int image_cur_hash;
static int r_numImages;

static byte *r_texturesPool;
static char *r_imagePathBuf, *r_imagePathBuf2;
static size_t r_sizeof_imagePathBuf, r_sizeof_imagePathBuf2;

const vec3_t r_cubeMapAngles[6] =
{
{   0, 180,  90},
{   0,   0, 270},
{   0,  90, 180},
{   0, 270,   0},
{ -90, 270,   0},
{  90,  90,   0}
};

const vec3_t r_skyBoxAngles[6] =
{
{   0,   0,   0},
{   0, 180,   0},
{   0,  90,   0},
{   0, 270,   0},
{ -90,   0,   0},
{  90,   0,   0}
};

#undef ENSUREBUFSIZE
#define ENSUREBUFSIZE(buf,need) \
	if( r_sizeof_ ##buf < need ) \
	{ \
		if( r_ ##buf ) \
			Mem_Free( r_ ##buf ); \
		r_sizeof_ ##buf += (((need) & (MAX_SHADERPATH-1))+1) * MAX_SHADERPATH; \
		r_ ##buf = Mem_Alloc( r_texturesPool, r_sizeof_ ##buf ); \
	}

int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int gl_filter_max = GL_LINEAR;

int gl_filter_depth = GL_LINEAR;

void GL_SelectTexture( int tmu )
{
	if( !GL_Support( R_ARB_MULTITEXTURE ))
		return;
	if( tmu == glState.currentTMU )
		return;

	glState.currentTMU = tmu;

	if( pglActiveTextureARB )
	{
		pglActiveTextureARB( tmu + GL_TEXTURE0_ARB );
		pglClientActiveTextureARB( tmu + GL_TEXTURE0_ARB );
	}
	else if( pglSelectTextureSGIS )
	{
		pglSelectTextureSGIS( tmu + GL_TEXTURE0_SGIS );
	}
}

void GL_TexEnv( GLenum mode )
{
	if( mode != ( GLenum )glState.currentEnvModes[glState.currentTMU] )
	{
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode );
		glState.currentEnvModes[glState.currentTMU] = ( int )mode;
	}
}

void GL_Bind( int tmu, image_t *tex )
{
	GL_SelectTexture( tmu );

	if( r_nobind->integer )  // performance evaluation option
		tex = r_notexture;
	if( glState.currentTextures[tmu] == tex->texnum )
		return;

	glState.currentTextures[tmu] = tex->texnum;
	if( tex->flags & IT_CUBEMAP )
		pglBindTexture( GL_TEXTURE_CUBE_MAP_ARB, tex->texnum );
	else if( tex->depth != 1 )
		pglBindTexture( GL_TEXTURE_3D, tex->texnum );
	else
		pglBindTexture( GL_TEXTURE_2D, tex->texnum );
}

void GL_LoadTexMatrix( const mat4x4_t m )
{
	pglMatrixMode( GL_TEXTURE );
	pglLoadMatrixf( m );
	glState.texIdentityMatrix[glState.currentTMU] = false;
}

void GL_LoadIdentityTexMatrix( void )
{
	if( !glState.texIdentityMatrix[glState.currentTMU] )
	{
		pglMatrixMode( GL_TEXTURE );
		pglLoadIdentity();
		glState.texIdentityMatrix[glState.currentTMU] = true;
	}
}

void GL_EnableTexGen( int coord, int mode )
{
	int tmu = glState.currentTMU;
	int bit, gen;

	switch( coord )
	{
	case GL_S:
		bit = 1;
		gen = GL_TEXTURE_GEN_S;
		break;
	case GL_T:
		bit = 2;
		gen = GL_TEXTURE_GEN_T;
		break;
	case GL_R:
		bit = 4;
		gen = GL_TEXTURE_GEN_R;
		break;
	case GL_Q:
		bit = 8;
		gen = GL_TEXTURE_GEN_Q;
		break;
	default:
		return;
	}

	if( mode )
	{
		if( !( glState.genSTEnabled[tmu] & bit ) )
		{
			pglEnable( gen );
			glState.genSTEnabled[tmu] |= bit;
		}
		pglTexGeni( coord, GL_TEXTURE_GEN_MODE, mode );
	}
	else
	{
		if( glState.genSTEnabled[tmu] & bit )
		{
			pglDisable( gen );
			glState.genSTEnabled[tmu] &= ~bit;
		}
	}
}

void GL_SetTexCoordArrayMode( int mode )
{
	int tmu = glState.currentTMU;
	int bit, cmode = glState.texCoordArrayMode[tmu];

	if( mode == GL_TEXTURE_COORD_ARRAY )
		bit = 1;
	else if( mode == GL_TEXTURE_CUBE_MAP_ARB )
		bit = 2;
	else
		bit = 0;

	if( cmode != bit )
	{
		if( cmode == 1 )
			pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		else if( cmode == 2 )
			pglDisable( GL_TEXTURE_CUBE_MAP_ARB );

		if( bit == 1 )
			pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		else if( bit == 2 )
			pglEnable( GL_TEXTURE_CUBE_MAP_ARB );

		glState.texCoordArrayMode[tmu] = bit;
	}
}

typedef struct
{
	char *name;
	int minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
};

#define NUM_GL_MODES ( sizeof( modes ) / sizeof( glmode_t ) )

/*
===============
R_TextureMode
===============
*/
void R_TextureMode( char *string )
{
	int i;
	image_t	*glt;

	for( i = 0; i < NUM_GL_MODES; i++ )
	{
		if( !com.stricmp( modes[i].name, string ) )
			break;
	}

	if( i == NUM_GL_MODES )
	{
		Msg( "R_TextureMode: bad filter name\n" );
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for( i = 1, glt = images; i < r_numImages; i++, glt++ )
	{
		GL_Bind( 0, glt );

		if( glt->flags & IT_DEPTH )
		{
			pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_depth );
			pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_depth );
		}
		else if( !( glt->flags & IT_NOMIPMAP ) )
		{
			pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
			pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
		}
		else
		{
			pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max );
			pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
		}
	}
}

/*
===============
R_ImageList_f
===============
*/
void R_ImageList_f( void )
{
	int i, num_depth = 0;
	image_t	*image;
	double texels = 0, add;
	double depth_texels = 0;

	Msg( "------------------\n" );

	for( i = 0, image = images; i < r_numImages; i++, image++ )
	{
		if( !image->upload_width || !image->upload_height || !image->upload_depth )
			continue;

		if( image->flags & IT_NOMIPMAP )
			add = image->upload_width * image->upload_height * image->upload_depth;
		else
			add = (unsigned)ceil( (double)image->upload_width * image->upload_height * image->upload_depth / 0.75 );

		if( image->flags & IT_CUBEMAP )
			add *= 6;

		if( image->flags & IT_DEPTH )
		{
			num_depth++;
			depth_texels += image->upload_width * image->upload_height * image->upload_depth;
			Msg( " %4i %4i %4i: %s\n", image->upload_width, image->upload_height, image->upload_depth, 
				image->name );
		}
		else
		{
			texels += add;
			Msg( " %4i %4i %4i: %s%s%s%s %.1f KB\n", image->upload_width, image->upload_height, image->upload_depth, 
				(image->flags & IT_NORGB ? "*A" : (image->flags & IT_NOALPHA ? "*C" : "")),
				image->name, image->extension, ((image->flags & IT_NOMIPMAP) ? "" : " (M)"), add / 1024.0 );
		}
	}

	Msg( "Total texels count (counting mipmaps, approx): %.0f\n", texels );
	Msg( "%i RGBA images, totalling %.3f megabytes\n", r_numImages - 1, texels / (1048576.0 / 4.0) );
	if( num_depth )
		Msg( "%i depth images, totalling %.0f texels\n", num_depth, depth_texels );
}

/*
=================================================================

TEMPORARY IMAGE BUFFERS

=================================================================
*/

enum
{
	TEXTURE_LOADING_BUF0,TEXTURE_LOADING_BUF1,TEXTURE_LOADING_BUF2,TEXTURE_LOADING_BUF3,TEXTURE_LOADING_BUF4,TEXTURE_LOADING_BUF5,
	TEXTURE_RESAMPLING_BUF,
	TEXTURE_LINE_BUF,
	TEXTURE_FLIPPING_BUF0,TEXTURE_FLIPPING_BUF1,TEXTURE_FLIPPING_BUF2,TEXTURE_FLIPPING_BUF3,TEXTURE_FLIPPING_BUF4,TEXTURE_FLIPPING_BUF5,

	NUM_IMAGE_BUFFERS
};

static byte *r_aviBuffer;

static byte *r_imageBuffers[NUM_IMAGE_BUFFERS];
static size_t r_imageBufSize[NUM_IMAGE_BUFFERS];

#define R_PrepareImageBuffer(buffer,size) _R_PrepareImageBuffer(buffer,size,__FILE__,__LINE__)

/*
==============
R_PrepareImageBuffer
==============
*/
static byte *_R_PrepareImageBuffer( int buffer, size_t size, const char *filename, int fileline )
{
	if( r_imageBufSize[buffer] < size )
	{
		r_imageBufSize[buffer] = size;
		if( r_imageBuffers[buffer] )
			Mem_Free( r_imageBuffers[buffer] );
		r_imageBuffers[buffer] = Mem_Alloc( r_texturesPool, size );
	}

	memset( r_imageBuffers[buffer], 255, size );

	return r_imageBuffers[buffer];
}

/*
==============
R_FreeImageBuffers
==============
*/
void R_FreeImageBuffers( void )
{
	int i;

	for( i = 0; i < NUM_IMAGE_BUFFERS; i++ )
	{
		if( r_imageBuffers[i] )
		{
			Mem_Free( r_imageBuffers[i] );
			r_imageBuffers[i] = NULL;
		}
		r_imageBufSize[i] = 0;
	}
}

//=======================================================
int R_GetSamples( int flags )
{
	if( flags & IMAGE_HAS_COLOR )
		return (flags & IMAGE_HAS_ALPHA) ? 4 : 3;
	return (flags & IMAGE_HAS_ALPHA) ? 2 : 1;
}

/*
===============
R_LoadImageFromDisk
===============
*/
static int R_LoadImageFromDisk( char *pathname, size_t pathname_size, byte **pic, int *width, int *height, int side )
{
	rgbdata_t	*image;
	int	samples = 0;

	*pic = NULL;
	*width = *height = 0;

	image = FS_LoadImage( pathname, NULL, 0 );
	if( image )
	{
		samples = R_GetSamples( image->flags );
		*pic = image->buffer;
		*width = image->width;
		*height = image->height;
	}
	// FIXME: need to release rgbdata


	return samples;
}

/*
================
R_FlipTexture
================
*/
static void R_FlipTexture( const byte *in, byte *out, int width, int height, int samples, bool flipx, bool flipy, bool flipdiagonal )
{
	int i, x, y;
	const byte *p, *line;
	int row_inc = ( flipy ? -samples : samples ) * width, col_inc = ( flipx ? -samples : samples );
	int row_ofs = ( flipy ? ( height - 1 ) * width * samples : 0 ), col_ofs = ( flipx ? ( width - 1 ) * samples : 0 );

	if( flipdiagonal )
	{
		for( x = 0, line = in + col_ofs; x < width; x++, line += col_inc )
			for( y = 0, p = line + row_ofs; y < height; y++, p += row_inc, out += samples )
				for( i = 0; i < samples; i++ )
					out[i] = p[i];
	}
	else
	{
		for( y = 0, line = in + row_ofs; y < height; y++, line += row_inc )
			for( x = 0, p = line + col_ofs; x < width; x++, p += col_inc, out += samples )
				for( i = 0; i < samples; i++ )
					out[i] = p[i];
	}
}

/*
================
R_ResampleTexture
================
*/
static void R_ResampleTexture( const unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight )
{
	int i, j;
	const unsigned *inrow, *inrow2;
	unsigned frac, fracstep;
	unsigned *p1, *p2;
	byte *pix1, *pix2, *pix3, *pix4;

	if( inwidth == outwidth && inheight == outheight )
	{
		memcpy( out, in, inwidth * inheight * 4 );
		return;
	}

	p1 = ( unsigned * )R_PrepareImageBuffer( TEXTURE_LINE_BUF, outwidth * 1 * sizeof( unsigned ) * 2 );
	p2 = p1 + outwidth;

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;
	for( i = 0; i < outwidth; i++ )
	{
		p1[i] = 4 * ( frac >> 16 );
		frac += fracstep;
	}

	frac = 3 * ( fracstep >> 2 );
	for( i = 0; i < outwidth; i++ )
	{
		p2[i] = 4 * ( frac >> 16 );
		frac += fracstep;
	}

	for( i = 0; i < outheight; i++, out += outwidth )
	{
		inrow = in + inwidth * (int)( ( i + 0.25 ) * inheight / outheight );
		inrow2 = in + inwidth * (int)( ( i + 0.75 ) * inheight / outheight );

		for( j = 0; j < outwidth; j++ )
		{
			pix1 = (byte *)inrow + p1[j];
			pix2 = (byte *)inrow + p2[j];
			pix3 = (byte *)inrow2 + p1[j];
			pix4 = (byte *)inrow2 + p2[j];
			( ( byte * )( out + j ) )[0] = ( pix1[0] + pix2[0] + pix3[0] + pix4[0] ) >> 2;
			( ( byte * )( out + j ) )[1] = ( pix1[1] + pix2[1] + pix3[1] + pix4[1] ) >> 2;
			( ( byte * )( out + j ) )[2] = ( pix1[2] + pix2[2] + pix3[2] + pix4[2] ) >> 2;
			( ( byte * )( out + j ) )[3] = ( pix1[3] + pix2[3] + pix3[3] + pix4[3] ) >> 2;
		}
	}
}

/*
================
R_HeightmapToNormalmap
================
*/
static int R_HeightmapToNormalmap( const byte *in, byte *out, int width, int height, float bumpScale )
{
	int x, y;
	vec3_t n;
	float ibumpScale;
	const byte *p0, *p1, *p2;

	if( !bumpScale )
		bumpScale = 1.0f;
	bumpScale *= max( 0, r_lighting_bumpscale->value );
	ibumpScale = ( 255.0 * 3.0 ) / bumpScale;

	memset( out, 255, width * height * 4 );
	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++, out += 4 )
		{
			p0 = in + ( y * width + x ) * 4;
			p1 = ( x == width - 1 ) ? p0 - x * 4 : p0 + 4;
			p2 = ( y == height - 1 ) ? in + x * 4 : p0 + width * 4;

			n[0] = ( p0[0] + p0[1] + p0[2] ) - ( p1[0] + p1[1] + p1[2] );
			n[1] = ( p2[0] + p2[1] + p2[2] ) - ( p0[0] + p0[1] + p0[2] );
			n[2] = ibumpScale;
			VectorNormalize( n );

			out[0] = ( n[0] + 1 ) * 127.5f;
			out[1] = ( n[1] + 1 ) * 127.5f;
			out[2] = ( n[2] + 1 ) * 127.5f;
			out[3] = ( p0[0] + p0[1] + p0[2] ) / 3;
		}
	}

	return 4;
}

/*
================
R_MergeNormalmapDepthmap
================
*/
static int R_MergeNormalmapDepthmap( const char *pathname, byte *in, int iwidth, int iheight )
{
	const char *p;
	int width, height, samples;
	byte *pic, *pic2;
	char *depthName;
	size_t depthNameSize;

	ENSUREBUFSIZE( imagePathBuf2, strlen( pathname ) + (strlen( "depth" ) + 1) + 5 );
	depthName = r_imagePathBuf2;
	depthNameSize = r_sizeof_imagePathBuf2;

	com.strncpy( depthName, pathname, depthNameSize );

	p = com.strstr( pathname, "_norm" );
	if( !p )
		p = pathname + strlen( pathname );
	com.strncpy( depthName + (p - pathname), "_depth", depthNameSize - (p - pathname) );

	pic = NULL;
	samples = R_LoadImageFromDisk( depthName, depthNameSize, &pic, &width, &height, 1 );

	if( pic )
	{
		if( (width == iwidth) && (height == iheight) )
		{
			int i;
			for( i = iwidth*iheight - 1, pic2 = pic; i > 0; i--, in += 4, pic2 += 4 )
				in[3] = ((int)pic2[0] + (int)pic2[1] + (int)pic2[2]) / 3;
			return 4;
		}
		else
		{
			MsgDev( D_WARN, "different depth map dimensions differ from parent (%s)\n", depthName );
		}
	}

	return 3;
}

/*
================
R_MipMap

Operates in place, quartering the size of the texture
note: if given odd width/height this discards the last row/column of
pixels, rather than doing a proper box-filter scale down (LordHavoc)
================
*/
static void R_MipMap( byte *in, int width, int height )
{
	int i, j;
	byte *out;

	width <<= 2;
	height >>= 1;

	out = in;
	for( i = 0; i < height; i++, in += width )
	{
		for( j = 0; j < width; j += 8, out += 4, in += 8 )
		{
			out[0] = ( in[0] + in[4] + in[width+0] + in[width+4] )>>2;
			out[1] = ( in[1] + in[5] + in[width+1] + in[width+5] )>>2;
			out[2] = ( in[2] + in[6] + in[width+2] + in[width+6] )>>2;
			out[3] = ( in[3] + in[7] + in[width+3] + in[width+7] )>>2;
		}
	}
}

/*
===============
R_TextureFormat
===============
*/
static int R_TextureFormat( int samples, bool noCompress )
{
	int bits = r_texturebits->integer;

	if( GL_Support( R_TEXTURE_COMPRESSION_EXT ) && !noCompress )
	{
		if( samples == 3 )
			return GL_COMPRESSED_RGB_ARB;
		return GL_COMPRESSED_RGBA_ARB;
	}

	if( samples == 3 )
	{
		if( bits == 16 )
			return GL_RGB5;
		else if( bits == 32 )
			return GL_RGB8;
		return GL_RGB;
	}

	if( bits == 16 )
		return GL_RGBA4;
	else if( bits == 32 )
		return GL_RGBA8;
	return GL_RGBA;
}

/*
===============
R_Upload32
===============
*/
void R_Upload32( byte **data, int width, int height, int flags, int *upload_width, int *upload_height, int *samples, bool subImage )
{
	int i, c, comp, format;
	int target, target2;
	int numTextures;
	unsigned *scaled = NULL;
	int scaledWidth, scaledHeight;

	Com_Assert( samples == NULL );

	if( GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
	{
		scaledWidth = width;
		scaledHeight = height;
	}
	else
	{
		for( scaledWidth = 1; scaledWidth < width; scaledWidth <<= 1 );
		for( scaledHeight = 1; scaledHeight < height; scaledHeight <<= 1 );
	}

	if( flags & IT_SKY )
	{
		// let people sample down the sky textures for speed
		scaledWidth >>= r_skymip->integer;
		scaledHeight >>= r_skymip->integer;
	}
	else if( !( flags & IT_NOPICMIP ) )
	{
		// let people sample down the world textures for speed
		scaledWidth >>= r_picmip->integer;
		scaledHeight >>= r_picmip->integer;
	}

	// don't ever bother with > maxSize textures
	if( flags & IT_CUBEMAP )
	{
		numTextures = 6;
		target = GL_TEXTURE_CUBE_MAP_ARB;
		target2 = GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB;
		scaledWidth = bound( 1, scaledWidth, glConfig.max_cubemap_texture_size );
		scaledHeight = bound( 1, scaledHeight, glConfig.max_cubemap_texture_size );
	}
	else
	{
		numTextures = 1;
		target = GL_TEXTURE_2D;
		target2 = GL_TEXTURE_2D;
		scaledWidth = bound( 1, scaledWidth, glConfig.max_2d_texture_size );
		scaledHeight = bound( 1, scaledHeight, glConfig.max_2d_texture_size );
	}

	if( upload_width )
		*upload_width = scaledWidth;
	if( upload_height )
		*upload_height = scaledHeight;

	// scan the texture for any non-255 alpha
	if( flags & ( IT_NORGB|IT_NOALPHA ) )
	{
		byte *scan;

		if( flags & IT_NORGB )
		{
			for( i = 0; i < numTextures && data[i]; i++ )
			{
				scan = ( byte * )data[i];
				for( c = width * height; c > 0; c--, scan += 4 )
					scan[0] = scan[1] = scan[2] = 255;
			}
		}
		else if( *samples == 4 )
		{
			for( i = 0; i < numTextures && data[i]; i++ )
			{
				scan = ( byte * )data[i] + 3;
				for( c = width * height; c > 0; c--, scan += 4 )
					*scan = 255;
			}
			*samples = 3;
		}
	}

	if( flags & IT_DEPTH )
	{
		comp = GL_DEPTH_COMPONENT;
		format = GL_DEPTH_COMPONENT;
	}
	else
	{
		comp = R_TextureFormat( *samples, flags & IT_NOCOMPRESS );
		format = GL_RGBA;
	}

	if( flags & IT_DEPTH )
	{
		pglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_depth );
		pglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_depth );

		if( GL_Support( R_ANISOTROPY_EXT ))
			pglTexParameteri( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1 );
	}
	else if( !( flags & IT_NOMIPMAP ) )
	{
		pglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_min );
		pglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( GL_Support( R_ANISOTROPY_EXT ))
			pglTexParameterf( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, glConfig.cur_texture_anisotropy );
	}
	else
	{
		pglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_max );
		pglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( GL_Support( R_ANISOTROPY_EXT ))
			pglTexParameterf( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f );
	}

	// clamp if required
	if( !( flags & IT_CLAMP ) )
	{
		pglTexParameteri( target, GL_TEXTURE_WRAP_S, GL_REPEAT );
		pglTexParameteri( target, GL_TEXTURE_WRAP_T, GL_REPEAT );
	}
	else if( GL_Support( R_CLAMPTOEDGE_EXT ))
	{
		pglTexParameteri( target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		pglTexParameteri( target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	}
	else
	{
		pglTexParameteri( target, GL_TEXTURE_WRAP_S, GL_CLAMP );
		pglTexParameteri( target, GL_TEXTURE_WRAP_T, GL_CLAMP );
	}

	if( ( scaledWidth == width ) && ( scaledHeight == height ) && ( flags & IT_NOMIPMAP ) )
	{
		if( subImage )
		{
			for( i = 0; i < numTextures; i++, target2++ )
				pglTexSubImage2D( target2, 0, 0, 0, scaledWidth, scaledHeight, format, GL_UNSIGNED_BYTE, data[i] );
		}
		else
		{
			for( i = 0; i < numTextures; i++, target2++ )
				pglTexImage2D( target2, 0, comp, scaledWidth, scaledHeight, 0, format, GL_UNSIGNED_BYTE, data[i] );
		}
	}
	else
	{
		bool driverMipmap = GL_Support( R_SGIS_MIPMAPS_EXT ) && !(flags & IT_CUBEMAP);

		for( i = 0; i < numTextures; i++, target2++ )
		{
			unsigned int *mip;

			if( scaledWidth == width && scaledHeight == height && driverMipmap )
			{
				mip = (unsigned *)(data[i]);
			}
			else
			{
				if( !scaled )
					scaled = ( unsigned * )R_PrepareImageBuffer( TEXTURE_RESAMPLING_BUF, scaledWidth * scaledHeight * 4 );

				// resample the texture
				mip = scaled;
				if( data[i] )
					R_ResampleTexture( (unsigned int *)( data[i] ), width, height, mip, scaledWidth, scaledHeight );
				else
					mip = (unsigned *)NULL;
			}

			// automatic mipmaps generation
			if( !( flags & IT_NOMIPMAP ) && mip && driverMipmap )
				pglTexParameteri( target2, GL_GENERATE_MIPMAP_SGIS, GL_TRUE );

			if( subImage )
				pglTexSubImage2D( target2, 0, 0, 0, scaledWidth, scaledHeight, format, GL_UNSIGNED_BYTE, mip );
			else
				pglTexImage2D( target2, 0, comp, scaledWidth, scaledHeight, 0, format, GL_UNSIGNED_BYTE, mip );

			// mipmaps generation
			if( !( flags & IT_NOMIPMAP ) && mip && !driverMipmap )
			{
				int w, h;
				int miplevel = 0;

				w = scaledWidth;
				h = scaledHeight;
				while( w > 1 || h > 1 )
				{
					R_MipMap( (byte *)mip, w, h );

					w >>= 1;
					h >>= 1;
					if( w < 1 )
						w = 1;
					if( h < 1 )
						h = 1;
					miplevel++;

					if( subImage )
						pglTexSubImage2D( target2, miplevel, 0, 0, w, h, format, GL_UNSIGNED_BYTE, mip );
					else
						pglTexImage2D( target2, miplevel, comp, w, h, 0, format, GL_UNSIGNED_BYTE, mip );
				}
			}
		}
	}
}

/*
===============
R_Upload32_3D

No resampling, scaling, mipmapping. Just to make 3D attenuation work ;)
===============
*/
void R_Upload32_3D_Fast( byte **data, int width, int height, int depth, int flags, int *upload_width, int *upload_height, int *upload_depth, int *samples, bool subImage )
{
	int comp;
	int scaledWidth, scaledHeight, scaledDepth;

	Com_Assert( samples == NULL );
	Com_Assert( ( flags & (IT_NOMIPMAP|IT_NOPICMIP) ) == 0 );

	for( scaledWidth = 1; scaledWidth < width; scaledWidth <<= 1 ) ;
	for( scaledHeight = 1; scaledHeight < height; scaledHeight <<= 1 ) ;
	for( scaledDepth = 1; scaledDepth < depth; scaledDepth <<= 1 ) ;

	if( width != scaledWidth || height != scaledHeight || depth != scaledDepth )
		Host_Error( "R_Upload32_3D: bad texture dimensions (not a power of 2)\n" );
	if( scaledWidth > glConfig.max_3d_texture_size || scaledHeight > glConfig.max_3d_texture_size || scaledDepth > glConfig.max_3d_texture_size )
		Host_Error( "R_Upload32_3D: texture is too large (resizing is not supported)\n" );

	if( upload_width )
		*upload_width = scaledWidth;
	if( upload_height )
		*upload_height = scaledHeight;
	if( upload_depth )
		*upload_depth = scaledDepth;

	comp = R_TextureFormat( *samples, flags & IT_NOCOMPRESS );

	if( !( flags & IT_NOMIPMAP ) )
	{
		pglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
		pglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( GL_Support( R_ANISOTROPY_EXT ))
			pglTexParameterf( GL_TEXTURE_3D, GL_TEXTURE_MAX_ANISOTROPY_EXT, glConfig.cur_texture_anisotropy );
	}
	else
	{
		pglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, gl_filter_max );
		pglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( GL_Support( R_ANISOTROPY_EXT ))
			pglTexParameterf( GL_TEXTURE_3D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f );
	}

	// clamp if required
	if( !( flags & IT_CLAMP ) )
	{
		pglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		pglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		pglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT );
	}
	else if( GL_Support( R_CLAMPTOEDGE_EXT ))
	{
		pglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		pglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		pglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
	}
	else
	{
		pglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		pglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP );
		pglTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP );
	}

	if( subImage )
		pglTexSubImage3D( GL_TEXTURE_3D, 0, 0, 0, 0, scaledWidth, scaledHeight, scaledDepth, GL_RGBA, GL_UNSIGNED_BYTE, data[0] );
	else
		pglTexImage3D( GL_TEXTURE_3D, 0, comp, scaledWidth, scaledHeight, scaledDepth, 0, GL_RGBA, GL_UNSIGNED_BYTE, data[0] );
}

/*
================
R_InitPic
================
*/
static _inline image_t *R_InitPic( const char *name, byte **pic, int width, int height, int depth, int flags, int samples )
{
	image_t	*image;

	if( r_numImages == MAX_GLIMAGES )
		Host_Error( "R_LoadPic: r_numImages == MAX_GLIMAGES" );

	image = images + r_numImages;
	image->name = Mem_Alloc( r_texturesPool, strlen( name ) + 1 );
	strcpy( image->name, name );
	image->width = width;
	image->height = height;
	image->depth = image->upload_depth = depth;
	image->flags = flags;
	image->samples = samples;
	image->texnum = ++r_numImages;

	// add to hash table
	if( image_cur_hash >= IMAGES_HASH_SIZE )
		image_cur_hash = Com_HashKey( name, IMAGES_HASH_SIZE );
	image->hash_next = images_hash[image_cur_hash];
	images_hash[image_cur_hash] = image;

	pglDeleteTextures( 1, &image->texnum );
	GL_Bind( 0, image );
	
	if( depth == 1 )
		R_Upload32( pic, width, height, flags, &image->upload_width, &image->upload_height, &image->samples, false );
	else
		R_Upload32_3D_Fast( pic, width, height, depth, flags, &image->upload_width, &image->upload_height, &image->upload_depth, &image->samples, false );

	image_cur_hash = IMAGES_HASH_SIZE+1;
	return image;
}

/*
================
R_LoadPic
================
*/
image_t *R_LoadPic( const char *name, byte **pic, int width, int height, int flags, int samples )
{
	return R_InitPic( name, pic, width, height, 1, flags, samples );
}

/*
================
R_Load3DPic
================
*/
image_t *R_Load3DPic( const char *name, byte **pic, int width, int height, int depth, int flags, int samples )
{
	return R_InitPic( name, pic, width, height, depth, flags, samples );
}

/*
===============
R_FindImage

Finds or loads the given image
===============
*/
image_t	*R_FindImage( const char *name, const char *suffix, int flags, float bumpScale )
{
	int i, lastDot;
	unsigned int len, key;
	image_t	*image;
	int width = 1, height = 1, samples = 0;
	char *pathname;
	size_t pathsize;

	if( !name || !name[0] )
		return NULL; //	Com_Error (ERR_DROP, "R_FindImage: NULL name");

	ENSUREBUFSIZE( imagePathBuf, strlen( name ) + (suffix ? strlen( suffix ) + 1 : 0) + 5 );
	pathname = r_imagePathBuf;
	pathsize = r_sizeof_imagePathBuf;

	lastDot = -1;
	for( i = ( name[0] == '/' || name[0] == '\\' ), len = 0; name[i]; i++ )
	{
		if( name[i] == '.' )
			lastDot = len;
		if( name[i] == '\\' )
			pathname[len++] = '/';
		else
			pathname[len++] = tolower( name[i] );
	}

	if( len < 5 )
		return NULL;
	else if( lastDot != -1 )
		len = lastDot;

	pathname[len] = 0;
	if( suffix )
	{
		pathname[len++] = '_';
		for( i = 0; suffix[i]; i++ )
			pathname[len++] = tolower( suffix[i] );
	}

	// look for it
	key = image_cur_hash = Com_HashKey( pathname, IMAGES_HASH_SIZE );
	if( flags & IT_HEIGHTMAP )
	{
		for( image = images_hash[key]; image; image = image->hash_next )
		{
			if( ( image->flags == flags ) && ( image->bumpScale == bumpScale ) && !strcmp( image->name, pathname ) )
				return image;
		}
	}
	else
	{
		for( image = images_hash[key]; image; image = image->hash_next )
		{
			if( ( image->flags == flags ) && !strcmp( image->name, pathname ) )
				return image;
		}
	}

	pathname[len] = 0;
	image = NULL;

	//
	// load the pic from disk
	//
	if( flags & IT_CUBEMAP )
	{
		byte *pic[6];
		struct cubemapSufAndFlip
		{
			char *suf; int flags;
		} cubemapSides[2][6] = {
			{ 
				{ "px", 0 }, { "nx", 0 }, { "py", 0 },
				{ "ny", 0 }, { "pz", 0 }, { "nz", 0 } 
			},
			{
				{ "rt", IT_FLIPDIAGONAL }, { "lf", IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL }, { "bk", IT_FLIPY },
				{ "ft", IT_FLIPX }, { "up", IT_FLIPDIAGONAL }, { "dn", IT_FLIPDIAGONAL }
			}
		};
		int j, lastSize = 0;

		pathname[len] = '_';
		for( i = 0; i < 2; i++ )
		{
			for( j = 0; j < 6; j++ )
			{
				pathname[len+1] = cubemapSides[i][j].suf[0];
				pathname[len+2] = cubemapSides[i][j].suf[1];
				pathname[len+3] = 0;

				samples = R_LoadImageFromDisk( pathname, pathsize, &(pic[j]), &width, &height, j );
				if( pic[j] )
				{
					if( width != height )
					{
						Msg( "Not square cubemap image %s\n", pathname );
						break;
					}
					if( !j )
					{
						lastSize = width;
					}
					else if( lastSize != width )
					{
						Msg( "Different cubemap image size: %s\n", pathname );
						break;
					}
					if( cubemapSides[i][j].flags & ( IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL ) )
					{
						int flags = cubemapSides[i][j].flags;
						byte *temp = R_PrepareImageBuffer( TEXTURE_FLIPPING_BUF0+j, width * height * 4 );
						R_FlipTexture( pic[j], temp, width, height, 4, (flags & IT_FLIPX), (flags & IT_FLIPY), (flags & IT_FLIPDIAGONAL) );
						pic[j] = temp;
					}
					continue;
				}
				break;
			}
			if( j == 6 )
				break;
		}

		if( i != 2 )
		{
			pathname[len] = 0;
			image = R_LoadPic( pathname, pic, width, height, flags, samples );
			image->extension[0] = '.';
			com.strncpy( &image->extension[1], &pathname[len+4], sizeof( image->extension )-1 );
		}
	}
	else
	{
		byte *pic;

		samples = R_LoadImageFromDisk( pathname, pathsize, &pic, &width, &height, 0 );

		if( pic )
		{
			byte *temp;

			if( flags & IT_NORMALMAP )
			{
				if( (samples == 3) && GL_Support( R_SHADER_GLSL100_EXT ))
					samples = R_MergeNormalmapDepthmap( pathname, pic, width, height );
			}
			else if( flags & IT_HEIGHTMAP )
			{
				temp = R_PrepareImageBuffer( TEXTURE_FLIPPING_BUF0, width * height * 4 );
				samples = R_HeightmapToNormalmap( pic, temp, width, height, bumpScale );
				pic = temp;
			}

			if( flags & ( IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL ) )
			{
				temp = R_PrepareImageBuffer( TEXTURE_FLIPPING_BUF0, width * height * 4 );
				R_FlipTexture( pic, temp, width, height, 4, (flags & IT_FLIPX), (flags & IT_FLIPY), (flags & IT_FLIPDIAGONAL) );
				pic = temp;
			}

			image = R_LoadPic( pathname, &pic, width, height, flags, samples );
			image->extension[0] = '.';
			com.strncpy( &image->extension[1], &pathname[len+1], sizeof( image->extension )-1 );
		}
	}

	return image;
}

/*
==============================================================================

SCREEN SHOTS

==============================================================================
*/
bool VID_ScreenShot( const char *filename, bool levelshot )
{
	rgbdata_t *r_shot;
	uint	flags = IMAGE_FLIP_Y;
	bool	result;

	r_shot = Mem_Alloc( r_temppool, sizeof( rgbdata_t ));
	r_shot->width = glState.width;
	r_shot->height = glState.height;
	r_shot->type = PF_RGB_24;
	r_shot->size = r_shot->width * r_shot->height * PFDesc( r_shot->type )->bpp;
	r_shot->palette = NULL;
	r_shot->numLayers = 1;
	r_shot->numMips = 1;
	r_shot->buffer = Mem_Alloc( r_temppool, glState.width * glState.height * 3 );

	// get screen frame
	pglReadPixels( 0, 0, glState.width, glState.height, GL_RGB, GL_UNSIGNED_BYTE, r_shot->buffer );

	if( levelshot ) flags |= IMAGE_RESAMPLE;
//	else VID_ImageAdjustGamma( r_shot->buffer, r_shot->width, r_shot->height ); // adjust brightness
	Image_Process( &r_shot, 512, 384, flags );

	// write image
	result = FS_SaveImage( filename, r_shot );
	FS_FreeImage( r_shot );
	return result;
}

/*
=================
VID_CubemapShot
=================
*/
bool VID_CubemapShot( const char *base, uint size, bool skyshot )
{
	rgbdata_t		*r_shot;
	byte		*temp = NULL;
	byte		*buffer = NULL;
	string		basename;
	int		i = 1, result;

	if( RI.refdef.onlyClientDraw || !r_worldmodel )
		return false;

	// make sure the specified size is valid
	while( i < size ) i<<=1;

	if( i != size ) return false;
	if( size > glState.width || size > glState.height )
		return false;

	// setup refdef
	RI.params |= RP_ENVVIEW;		// do not render non-bmodel entities

	// alloc space
	temp = Mem_Alloc( r_temppool, size * size * 3 );
	buffer = Mem_Alloc( r_temppool, size * size * 3 * 6 );

	for( i = 0; i < 6; i++ )
	{
		if( skyshot ) R_DrawCubemapView( r_lastRefdef.vieworg, r_skyBoxAngles[i], size );
		else R_DrawCubemapView( r_lastRefdef.vieworg, r_cubeMapAngles[i], size );

		pglReadPixels( 0, glState.height - size, size, size, GL_RGB, GL_UNSIGNED_BYTE, temp );
		Mem_Copy( buffer + (size * size * 3 * i), temp, size * size * 3 );
	}

	RI.params &= ~RP_ENVVIEW;

	r_shot = Mem_Alloc( r_temppool, sizeof( rgbdata_t ));
	r_shot->flags |= IMAGE_CUBEMAP;
	r_shot->width = size;
	r_shot->height = size;
	r_shot->type = PF_RGB_24;
	r_shot->size = r_shot->width * r_shot->height * 3 * 6;
	r_shot->palette = NULL;
	r_shot->numLayers = 1;
	r_shot->numMips = 1;
	r_shot->buffer = buffer;

	// make sure what we have right extension
	com.strncpy( basename, base, MAX_STRING );
	FS_StripExtension( basename );
	FS_DefaultExtension( basename, ".dds" );
	
	// write image as dds packet
	result = FS_SaveImage( basename, r_shot );
	FS_FreeImage( r_shot );
	Mem_Free( temp );

	return result;
}
//=======================================================

/*
==================
R_InitNoTexture
==================
*/
static byte *R_InitNoTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	int x, y;
	byte *data;
	byte dottexture[8][8] =
	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 1, 1, 0, 0, 0, 0 },
		{ 0, 1, 1, 1, 1, 0, 0, 0 },
		{ 0, 1, 1, 1, 1, 0, 0, 0 },
		{ 0, 0, 1, 1, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
	};

	//
	// also use this for bad textures, but without alpha
	//
	*w = *h = 8;
	*depth = 1;
	*flags = 0;
	*samples = 3;

	data = R_PrepareImageBuffer( TEXTURE_LOADING_BUF0, 8 * 8 * 4 );
	for( x = 0; x < 8; x++ )
	{
		for( y = 0; y < 8; y++ )
		{
			data[( y*8 + x )*4+0] = dottexture[x&3][y&3]*127;
			data[( y*8 + x )*4+1] = dottexture[x&3][y&3]*127;
			data[( y*8 + x )*4+2] = dottexture[x&3][y&3]*127;
		}
	}

	return data;
}

/*
==================
R_InitDynamicLightTexture
==================
*/
static byte *R_InitDynamicLightTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	vec3_t v = { 0, 0, 0 };
	float intensity;
	int x, y, z, d, size;
	byte *data;

	//
	// dynamic light texture
	//
	if( GL_Support( R_TEXTURE_3D_EXT ))
	{
		size = 32;
		*depth = size;
	}
	else
	{
		size = 64;
		*depth = 1;
	}

	*w = *h = size;
	*flags = IT_NOPICMIP|IT_NOMIPMAP|IT_CLAMP|IT_NOCOMPRESS;
	*samples = 3;

	data = R_PrepareImageBuffer( TEXTURE_LOADING_BUF0, size * size * *depth * 4 );
	for( x = 0; x < size; x++ )
	{
		for( y = 0; y < size; y++ )
		{
			for( z = 0; z < *depth; z++ )
			{
				v[0] = ( ( x + 0.5f ) * ( 2.0f / (float)size ) - 1.0f );
				v[1] = ( ( y + 0.5f ) * ( 2.0f / (float)size ) - 1.0f );
				if( *depth > 1 )
					v[2] = ( ( z + 0.5f ) * ( 2.0f / (float)size ) - 1.0f );

				intensity = 1.0f - sqrt( DotProduct( v, v ) );
				if( intensity > 0 )
					intensity = intensity * intensity * 215.5f;
				d = bound( 0, intensity, 255 );

				data[( ( z*size+y )*size + x ) * 4 + 0] = d;
				data[( ( z*size+y )*size + x ) * 4 + 1] = d;
				data[( ( z*size+y )*size + x ) * 4 + 2] = d;
			}
		}
	}
	return data;
}

/*
==================
R_InitSolidColorTexture
==================
*/
static byte *R_InitSolidColorTexture( int *w, int *h, int *depth, int *flags, int *samples, int color )
{
	byte *data;

	//
	// solid color texture
	//
	*w = *h = 1;
	*depth = 1;
	*flags = IT_NOPICMIP|IT_NOCOMPRESS;
	*samples = 3;

	data = R_PrepareImageBuffer( TEXTURE_LOADING_BUF0, 1 * 1 * 4 );
	data[0] = data[1] = data[2] = color;
	return data;
}

/*
==================
R_InitParticleTexture
==================
*/
static byte *R_InitParticleTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	int x, y;
	int dx2, dy, d;
	byte *data;

	//
	// particle texture
	//
	*w = *h = 16;
	*depth = 1;
	*flags = IT_NOPICMIP|IT_NOMIPMAP;
	*samples = 4;

	data = R_PrepareImageBuffer( TEXTURE_LOADING_BUF0, 16 * 16 * 4 );
	for( x = 0; x < 16; x++ )
	{
		dx2 = x - 8;
		dx2 = dx2 * dx2;

		for( y = 0; y < 16; y++ )
		{
			dy = y - 8;
			d = 255 - 35 *sqrt( dx2 + dy *dy );
			data[( y*16 + x ) * 4 + 3] = bound( 0, d, 255 );
		}
	}
	return data;
}

/*
==================
R_InitWhiteTexture
==================
*/
static byte *R_InitWhiteTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	return R_InitSolidColorTexture( w, h, depth, flags, samples, 255 );
}

/*
==================
R_InitBlackTexture
==================
*/
static byte *R_InitBlackTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	return R_InitSolidColorTexture( w, h, depth, flags, samples, 0 );
}

/*
==================
R_InitBlankBumpTexture
==================
*/
static byte *R_InitBlankBumpTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	byte *data = R_InitSolidColorTexture( w, h, depth, flags, samples, 128 );

/*
	data[2] = 128;	// normal X
	data[1] = 128;	// normal Y
*/
	data[0] = 255;	// normal Z
	data[3] = 128;	// height

	return data;
}

/*
==================
R_InitFogTexture
==================
*/
static byte *R_InitFogTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	byte *data;
	int x, y;
	double tw = 1.0f / ( (float)FOG_TEXTURE_WIDTH - 1.0f );
	double th = 1.0f / ( (float)FOG_TEXTURE_HEIGHT - 1.0f );
	double tx, ty, t;

	//
	// fog texture
	//
	*w = FOG_TEXTURE_WIDTH;
	*h = FOG_TEXTURE_HEIGHT;
	*depth = 1;
	*flags = IT_NOMIPMAP|IT_CLAMP;
	*samples = 4;

	data = R_PrepareImageBuffer( TEXTURE_LOADING_BUF0, FOG_TEXTURE_WIDTH*FOG_TEXTURE_HEIGHT*4 );
	for( y = 0, ty = 0.0f; y < FOG_TEXTURE_HEIGHT; y++, ty += th )
	{
		for( x = 0, tx = 0.0f; x < FOG_TEXTURE_WIDTH; x++, tx += tw )
		{
			t = sqrt( tx ) * 255.0;
			data[( x+y*FOG_TEXTURE_WIDTH )*4+3] = (byte)( min( t, 255.0 ) );
		}
		data[y*4+3] = 0;
	}
	return data;
}

/*
==================
R_InitCoronaTexture
==================
*/
static byte *R_InitCoronaTexture( int *w, int *h, int *depth, int *flags, int *samples )
{
	int x, y, a;
	float dx, dy;
	byte *data;

	//
	// light corona texture
	//
	*w = *h = 32;
	*depth = 1;
	*flags = IT_NOMIPMAP|IT_NOPICMIP|IT_NOCOMPRESS|IT_CLAMP;
	*samples = 4;

	data = R_PrepareImageBuffer( TEXTURE_LOADING_BUF0, 32 * 32 * 4 );
	for( y = 0; y < 32; y++ )
	{
		dy = ( y - 15.5f ) * ( 1.0f / 16.0f );
		for( x = 0; x < 32; x++ )
		{
			dx = ( x - 15.5f ) * ( 1.0f / 16.0f );
			a = (int)( ( ( 1.0f / ( dx * dx + dy * dy + 0.2f ) ) - ( 1.0f / ( 1.0f + 0.2 ) ) ) * 32.0f / ( 1.0f / ( 1.0f + 0.2 ) ) );
			a = bound( 0, a, 255 );
			data[( y*32+x )*4+0] = data[( y*32+x )*4+1] = data[( y*32+x )*4+2] = a;
		}
	}
	return data;
}

/*
==================
R_InitScreenTexture
==================
*/
static void R_InitScreenTexture( image_t **texture, const char *name, int id, int screenWidth, int screenHeight, int size, int flags, int samples )
{
	int limit;
	int width, height;

	limit = glConfig.max_2d_texture_size;
	if( size )
		limit = min( limit, size );

	if( GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
	{
		width = min( screenWidth, limit );
		height = min( screenHeight, limit );
	}
	else
	{
		limit = min( limit, min( screenWidth, screenHeight ) );
		for( size = 2; size <= limit; size <<= 1 );
		width = height = size >> 1;
	}

	if( !( *texture ) || ( *texture )->width != width || ( *texture )->height != height )
	{
		byte *data = NULL;

		if( !*texture )
		{
			char uploadName[128];

			com.snprintf( uploadName, sizeof( uploadName ), "***%s%i***", name, id );
			*texture = R_LoadPic( uploadName, &data, width, height, flags, samples );
			return;
		}

		GL_Bind( 0, *texture );
		( *texture )->width = width;
		( *texture )->height = height;
		R_Upload32( &data, width, height, flags, &( ( *texture )->upload_width ), &( ( *texture )->upload_height ),
			&( ( *texture )->samples ), false );
	}
}

/*
==================
R_InitPortalTexture
==================
*/
void R_InitPortalTexture( image_t **texture, int id, int screenWidth, int screenHeight )
{
	R_InitScreenTexture( texture, "r_portaltexture", id, screenWidth, screenHeight, r_portalmaps_maxtexsize->integer, IT_PORTALMAP, 3 );
}

/*
==================
R_InitShadowmapTexture
==================
*/
void R_InitShadowmapTexture( image_t **texture, int id, int screenWidth, int screenHeight )
{
	R_InitScreenTexture( texture, "r_shadowmap", id, screenWidth, screenHeight, r_shadows_maxtexsize->integer, IT_SHADOWMAP, 1 );
}

/*
==================
R_InitCinematicTexture
==================
*/
static void R_InitCinematicTexture( void )
{
	// reserve a dummy texture slot
	r_cintexture = &images[r_numImages++];
	r_cintexture->texnum = 1;
	r_cintexture->depth = 1;
}

/*
==================
R_InitBuiltinTextures
==================
*/
static void R_InitBuiltinTextures( void )
{
	byte *data;
	int w, h, depth, flags, samples;
	image_t *image;
	const struct
	{
		char *name;
		image_t	**image;
		byte *( *init )( int *w, int *h, int *depth, int *flags, int *samples );
	}
	textures[] =
	{
		{ "***r_notexture***", &r_notexture, R_InitNoTexture },
		{ "***r_whitetexture***", &r_whitetexture, R_InitWhiteTexture },
		{ "***r_blacktexture***", &r_blacktexture, R_InitBlackTexture },
		{ "***r_blankbumptexture***", &r_blankbumptexture, R_InitBlankBumpTexture },
		{ "***r_dlighttexture***", &r_dlighttexture, R_InitDynamicLightTexture },
		{ "***r_particletexture***", &r_particletexture, R_InitParticleTexture },
		{ "***r_fogtexture***", &r_fogtexture, R_InitFogTexture },
		{ "***r_coronatexture***", &r_coronatexture, R_InitCoronaTexture },
		{ NULL, NULL, NULL }
	};
	size_t i, num_builtin_textures = sizeof( textures ) / sizeof( textures[0] ) - 1;

	for( i = 0; i < num_builtin_textures; i++ )
	{
		data = textures[i].init( &w, &h, &depth, &flags, &samples );
		Com_Assert( data == NULL );

		image = ( depth == 1 ?
			R_LoadPic( textures[i].name, &data, w, h, flags, samples ) :
		R_Load3DPic( textures[i].name, &data, w, h, depth, flags, samples )
			);

		if( textures[i].image )
			*( textures[i].image ) = image;
	}

	r_portaltexture = NULL;
	r_portaltexture2 = NULL;
}

//=======================================================

/*
===============
R_InitImages
===============
*/
void R_InitImages( void )
{
	r_texturesPool = Mem_AllocPool( "Textures" );
	image_cur_hash = IMAGES_HASH_SIZE+1;

	r_imagePathBuf = r_imagePathBuf2 = NULL;
	r_sizeof_imagePathBuf = r_sizeof_imagePathBuf2 = 0;

	R_InitCinematicTexture();
	R_InitBuiltinTextures();
	R_InitBloomTextures();
}

/*
===============
R_ShutdownImages
===============
*/
void R_ShutdownImages( void )
{
	int i;

	if( !r_texturesPool )
		return;

	R_FreeImageBuffers ();

	for( i = 0; i < r_numImages; i++ )
		if( images[i].name )
			Mem_Free( images[i].name );

	if( r_imagePathBuf )
		Mem_Free( r_imagePathBuf );
	if( r_imagePathBuf2 )
		Mem_Free( r_imagePathBuf2 );

	Mem_FreePool( &r_texturesPool );

	r_portaltexture = NULL;
	r_portaltexture2 = NULL;
	memset( r_shadowmapTextures, 0, sizeof( image_t * ) * MAX_SHADOWGROUPS );

	r_imagePathBuf = r_imagePathBuf2 = NULL;
	r_sizeof_imagePathBuf = r_sizeof_imagePathBuf2 = 0;

	r_numImages = 0;
	memset( images, 0, sizeof( images ) );
	memset( r_lightmapTextures, 0, sizeof( r_lightmapTextures ) );
	memset( images_hash, 0, sizeof( images_hash ) );
}
