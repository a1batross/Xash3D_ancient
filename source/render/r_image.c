//=======================================================================
//			Copyright XashXT Group 2008 ©
//			 r_image.c - texture manager
//=======================================================================

#include "r_local.h"
#include "byteorder.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "const.h"

#define TEXTURES_HASH_SIZE		64

static rgbdata_t *R_LoadImage( script_t *script, const char *name, const byte *buf, size_t size, int *samples, texFlags_t *flags );
static int r_textureMinFilter = GL_LINEAR_MIPMAP_LINEAR;
static int r_textureMagFilter = GL_LINEAR;
static int r_textureDepthFilter = GL_LINEAR;

// internal tables
static vec3_t	r_luminanceTable[256];	// RGB to luminance
static byte	r_glowTable[256][3];	// auto LUMA table
static byte	r_gammaTable[256];		// adjust screenshot gamma

static texture_t	r_textures[MAX_TEXTURES];
static texture_t	*r_texturesHashTable[TEXTURES_HASH_SIZE];
static int	r_numTextures;
static byte	*r_imagepool;		// immediate buffers
static byte	*r_texpool;		// texture_t permanent chain
static byte	data2D[256*256*4];
static rgbdata_t	r_image;			// generic pixelbuffer used for internal textures

typedef struct envmap_s
{
	vec3_t	angles;
	int	flags;
} envmap_t;

const envmap_t r_skyBoxInfo[6] =
{
{{   0, 270, 180}, IMAGE_FLIP_X },
{{   0,  90, 180}, IMAGE_FLIP_X },
{{ -90,   0, 180}, IMAGE_FLIP_X },
{{  90,   0, 180}, IMAGE_FLIP_X },
{{   0,   0, 180}, IMAGE_FLIP_X },
{{   0, 180, 180}, IMAGE_FLIP_X },
};

const envmap_t r_envMapInfo[6] =
{
{{  0,   0,  90}, 0 },
{{  0, 180, -90}, 0 },
{{  0,  90,   0}, 0 },
{{  0, 270, 180}, 0 },
{{-90, 180, -90}, 0 },
{{ 90, 180,  90}, 0 }
};

static struct
{
	string		name;
	pixformat_t	format;
	int		width;
	int		height;
	int		bpp;
	int		bpc;
	int		bps;
	int		SizeOfPlane;
	int		SizeOfData;
	int		SizeOfFile;
	int		depth;
	int		numSides;
	int		MipCount;
	int		BitsCount;
	GLuint		glFormat;
	GLuint		glType;
	GLuint		glTarget;
	GLuint		glSamples;
	GLuint		texTarget;
	texType_t		texType;

	uint		tflags;	// TF_ flags	
	uint		flags;	// IMAGE_ flags
	byte		*pal;
	byte		*source;
	byte		*scaled;
} image_desc;

void GL_SelectTexture( GLenum texture )
{
	if( !GL_Support( R_ARB_MULTITEXTURE ))
		return;
	if( glState.activeTMU == texture )
		return;

	glState.activeTMU = texture;

	if( pglActiveTextureARB )
	{
		pglActiveTextureARB( texture + GL_TEXTURE0_ARB );
		pglClientActiveTextureARB( texture + GL_TEXTURE0_ARB );
	}
	else if( pglSelectTextureSGIS )
	{
		pglSelectTextureSGIS( texture + GL_TEXTURE0_SGIS );
	}
}

void GL_TexEnv( GLenum mode )
{
	if( glState.currentEnvModes[glState.activeTMU] == mode )
		return;

	glState.currentEnvModes[glState.activeTMU] = mode;
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode );
}

void GL_Bind( GLenum tmu, texture_t *texture )
{
	GL_SelectTexture( tmu );

	if( r_nobind->integer ) texture = tr.defaultTexture;	// performance evaluation option
	if( glState.currentTextures[tmu] == texture->texnum )
		return;

	glState.currentTextures[tmu] = texture->texnum;
	pglBindTexture( texture->target, texture->texnum );
}

void GL_LoadTexMatrix( const matrix4x4 m )
{
	pglMatrixMode( GL_TEXTURE );
	GL_LoadMatrix( m );
	glState.texIdentityMatrix[glState.activeTMU] = false;
}

void GL_LoadMatrix( const matrix4x4 source )
{
	GLfloat	dest[16];

	Matrix4x4_ToArrayFloatGL( source, dest );
	pglLoadMatrixf( dest );
}

void GL_LoadIdentityTexMatrix( void )
{
	if( glState.texIdentityMatrix[glState.activeTMU] )
		return;

	pglMatrixMode( GL_TEXTURE );
	pglLoadIdentity();
	glState.texIdentityMatrix[glState.activeTMU] = true;
}

void GL_EnableTexGen( int coord, int mode )
{
	int	tmu = glState.activeTMU;
	int	bit, gen;

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
	default: return;
	}

	if( mode )
	{
		if(!( glState.genSTEnabled[tmu] & bit ))
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
	int	tmu = glState.activeTMU;
	int	bit, cmode = glState.texCoordArrayMode[tmu];

	if( mode == GL_TEXTURE_COORD_ARRAY )
		bit = 1;
	else if( mode == GL_TEXTURE_CUBE_MAP_ARB )
		bit = 2;
	else bit = 0;

	if( cmode != bit )
	{
		if( cmode == 1 ) pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		else if( cmode == 2 ) pglDisable( GL_TEXTURE_CUBE_MAP_ARB );

		if( bit == 1 ) pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		else if( bit == 2 ) pglEnable( GL_TEXTURE_CUBE_MAP_ARB );

		glState.texCoordArrayMode[tmu] = bit;
	}
}

/*
=================
R_SetTextureParameters
=================
*/
void R_SetTextureParameters( void )
{
	texture_t		*texture;
	int		i;

	if( !com.stricmp( gl_texturemode->string, "GL_NEAREST" ))
	{
		r_textureMinFilter = GL_NEAREST;
		r_textureMagFilter = GL_NEAREST;
	}
	else if( !com.stricmp( gl_texturemode->string, "GL_LINEAR" ))
	{
		r_textureMinFilter = GL_LINEAR;
		r_textureMagFilter = GL_LINEAR;
	}
	else if( !com.stricmp( gl_texturemode->string, "GL_NEAREST_MIPMAP_NEAREST" ))
	{
		r_textureMinFilter = GL_NEAREST_MIPMAP_NEAREST;
		r_textureMagFilter = GL_NEAREST;
	}
	else if( !com.stricmp( gl_texturemode->string, "GL_LINEAR_MIPMAP_NEAREST" ))
	{
		r_textureMinFilter = GL_LINEAR_MIPMAP_NEAREST;
		r_textureMagFilter = GL_LINEAR;
	}
	else if( !com.stricmp( gl_texturemode->string, "GL_NEAREST_MIPMAP_LINEAR" ))
	{
		r_textureMinFilter = GL_NEAREST_MIPMAP_LINEAR;
		r_textureMagFilter = GL_NEAREST;
	}
	else if( !com.stricmp( gl_texturemode->string, "GL_LINEAR_MIPMAP_LINEAR" ))
	{
		r_textureMinFilter = GL_LINEAR_MIPMAP_LINEAR;
		r_textureMagFilter = GL_LINEAR;
	}
	else
	{
		MsgDev( D_ERROR, "gl_texturemode invalid mode %s, defaulting to GL_LINEAR_MIPMAP_LINEAR\n", gl_texturemode->string );
		Cvar_Set( "gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR" );
		r_textureMinFilter = GL_LINEAR_MIPMAP_LINEAR;
		r_textureMagFilter = GL_LINEAR;
	}

	gl_texturemode->modified = false;

	if( GL_Support( R_ANISOTROPY_EXT ))
	{
		if( gl_texture_anisotropy->value > glConfig.max_texture_anisotropy )
			Cvar_SetValue( "r_anisotropy", glConfig.max_texture_anisotropy );
		else if( gl_texture_anisotropy->value < 1.0f )
			Cvar_SetValue( "r_anisotropy", 1.0f );
	}
	gl_texture_anisotropy->modified = false;

	if( GL_Support( R_TEXTURE_LODBIAS ))
	{
		if( gl_texture_lodbias->value > glConfig.max_texture_lodbias )
			Cvar_SetValue( "r_texture_lodbias", glConfig.max_texture_lodbias );
		else if( gl_texture_lodbias->value < -glConfig.max_texture_lodbias )
			Cvar_SetValue( "r_texture_lodbias", -glConfig.max_texture_lodbias );
	}
	gl_texture_lodbias->modified = false;

	// change all the existing mipmapped texture objects
	for( i = 0, texture = r_textures; i < r_numTextures; i++, texture++ )
	{
		if( !texture->texnum ) continue;	// free slot
		GL_Bind( GL_TEXTURE0, texture );

		if( texture->flags & TF_DEPTHMAP )
		{
			// set texture filter
			pglTexParameteri( texture->target, GL_TEXTURE_MIN_FILTER, r_textureDepthFilter );
			pglTexParameteri( texture->target, GL_TEXTURE_MAG_FILTER, r_textureDepthFilter );

			// set texture anisotropy if available
			if( GL_Support( R_ANISOTROPY_EXT ))
				pglTexParameterf( texture->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f );
		}
		else if( texture->flags & TF_NOMIPMAP )
		{
			// set texture filter
			pglTexParameteri( texture->target, GL_TEXTURE_MIN_FILTER, r_textureMagFilter );
			pglTexParameteri( texture->target, GL_TEXTURE_MAG_FILTER, r_textureMagFilter );
		}
		else
		{
			// set texture filter
			pglTexParameteri( texture->target, GL_TEXTURE_MIN_FILTER, r_textureMinFilter );
			pglTexParameteri( texture->target, GL_TEXTURE_MAG_FILTER, r_textureMagFilter );

			// set texture anisotropy if available
			if( GL_Support( R_ANISOTROPY_EXT ))
				pglTexParameterf( texture->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_anisotropy->value );

			// set texture LOD bias if available
			if( GL_Support( R_TEXTURE_LODBIAS ))
				pglTexParameterf( texture->target, GL_TEXTURE_LOD_BIAS_EXT, gl_texture_lodbias->value );
		}
	}
}

/*
===============
R_TextureList_f
===============
*/
void R_TextureList_f( void )
{
	texture_t	*image;
	int	i, texCount, bytes = 0;

	Msg( "\n" );
	Msg("      -w-- -h-- -size- -fmt- type -filter -wrap-- -name--------\n" );

	for( i = texCount = 0, image = r_textures; i < r_numTextures; i++, image++ )
	{
		if( !image->texnum ) continue;

		bytes += image->size;
		texCount++;

		Msg( "%4i: ", i );
		Msg( "%4i %4i ", image->width, image->height );
		Msg( "%5ik ", image->size >> 10 );

		switch( image->format )
		{
		case GL_COMPRESSED_RGBA_ARB:
			Msg( "CRGBA " );
			break;
		case GL_COMPRESSED_RGB_ARB:
			Msg( "CRGB  " );
			break;
		case GL_COMPRESSED_LUMINANCE_ALPHA_ARB:
			Msg( "CLA   " );
			break;
		case GL_COMPRESSED_LUMINANCE_ARB:
			Msg( "CL    " );
			break;
		case GL_COMPRESSED_ALPHA_ARB:
			Msg( "CA    " );
			break;
		case GL_COMPRESSED_INTENSITY_ARB:
			Msg( "CI    " );
			break;
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			Msg( "DXT1  " );
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
			Msg( "DXT3  " );
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			Msg( "DXT5  " );
			break;
		case GL_RGBA:
			Msg( "RGBA  " );
			break;
		case GL_RGBA8:
			Msg( "RGBA8 " );
			break;
		case GL_RGBA4:
			Msg( "RGBA4 " );
			break;
		case GL_RGB:
			Msg( "RGB   " );
			break;
		case GL_RGB8:
			Msg( "RGB8  " );
			break;
		case GL_RGB5:
			Msg( "RGB5  " );
			break;
		case GL_LUMINANCE8_ALPHA8:
			Msg( "L8A8  " );
			break;
		case GL_LUMINANCE8:
			Msg( "L8    " );
			break;
		case GL_ALPHA8:
			Msg( "A8    " );
			break;
		case GL_INTENSITY8:
			Msg( "I8    " );
			break;
		default:
			Msg( "????? " );
			break;
		}

		switch( image->target )
		{
		case GL_TEXTURE_2D:
			Msg( " 2D  " );
			break;
		case GL_TEXTURE_3D:
			Msg( " 3D  " );
			break;
		case GL_TEXTURE_CUBE_MAP_ARB:
			Msg( "CUBE " );
			break;
		default:
			Msg( "???? " );
			break;
		}

		if( image->flags & TF_NOMIPMAP )
			Msg( "linear " );
		else Msg( "default" );

		if( image->flags & TF_CLAMP )
			Msg( " clamp  " );
		else Msg( " repeat " );
		Msg( "  %s\n", image->name );
	}

	Msg( "---------------------------------------------------------\n" );
	Msg( "%i total textures\n", texCount );
	Msg( "%.2f total megabytes of textures\n", bytes/1048576.0 );
	Msg( "\n" );
}

/*
=======================================================================

TEXTURE INITIALIZATION AND LOADING

=======================================================================
*/

/*
===============
R_GetImageSize

calculate buffer size for current miplevel
===============
*/
uint R_GetImageSize( int block, int width, int height, int depth, int bpp, int rgbcount )
{
	uint BlockSize = 0;

	if( block == 0 ) BlockSize = width * height * depth * bpp;
	else if( block > 0 ) BlockSize = ((width + 3)/4) * ((height + 3)/4) * depth * block;
	else if( block < 0 && rgbcount > 0 ) BlockSize = width * height * depth * rgbcount;
	else BlockSize = width * height * depth * abs( block );

	return BlockSize;
}

int R_GetSamples( int flags )
{
	if( flags & IMAGE_HAS_COLOR )
		return (flags & IMAGE_HAS_ALPHA) ? 4 : 3;
	return (flags & IMAGE_HAS_ALPHA) ? 2 : 1;
}

int R_SetSamples( int s1, int s2 )
{
	int	samples;

	if( s1 == 1 ) samples = s2;
	else if( s1 == 2 )
	{
		if( s2 == 3 || s2 == 4 )
			samples = 4;
		else samples = 2;
	}
	else if( s1 == 3 )
	{
		if( s2 == 2 || s2 == 4 )
			samples = 4;
		else samples = 3;
	}
	else samples = s1;

	return samples;
}

/*
===============
R_TextureFormat
===============
*/
static void R_TextureFormat( texture_t *tex, bool compress )
{
	// set texture format
	if( tex->flags & TF_DEPTHMAP )
	{
		tex->format = GL_DEPTH_COMPONENT;
		tex->flags &= ~TF_INTENSITY;
		tex->flags &= ~TF_ALPHA;
	}
	else if( compress )
	{
		switch( tex->samples )
		{
		case 1: tex->format = GL_COMPRESSED_LUMINANCE_ARB; break;
		case 2: tex->format = GL_COMPRESSED_LUMINANCE_ALPHA_ARB; break;
		case 3: tex->format = GL_COMPRESSED_RGB_ARB; break;
		case 4: tex->format = GL_COMPRESSED_RGBA_ARB; break;
		}

		if( tex->flags & TF_INTENSITY )
			tex->format = GL_COMPRESSED_INTENSITY_ARB;
		if( tex->flags & TF_ALPHA )
			tex->format = GL_COMPRESSED_ALPHA_ARB;
		tex->flags &= ~TF_INTENSITY;
		tex->flags &= ~TF_ALPHA;
	}
	else
	{
		int	bits = r_texturebits->integer;

		switch( tex->samples )
		{
		case 1: tex->format = GL_LUMINANCE8; break;
		case 2: tex->format = GL_LUMINANCE8_ALPHA8; break;
		case 3:
			switch( bits )
			{
			case 16: tex->format = GL_RGB5; break;
			case 32: tex->format = GL_RGB8; break;
			default: tex->format = GL_RGB; break;
			}
			break;		
		case 4:
			switch( bits )
			{
			case 16: tex->format = GL_RGBA4; break;
			case 32: tex->format = GL_RGBA8; break;
			default: tex->format = GL_RGBA; break;
			}
			break;
		}

		if( tex->flags & TF_INTENSITY )
			tex->format = GL_INTENSITY8;
		if( tex->flags & TF_ALPHA )
			tex->format = GL_ALPHA8;
		tex->flags &= ~TF_INTENSITY;
		tex->flags &= ~TF_ALPHA;
	}
}

void R_RoundImageDimensions( int *width, int *height, int *depth, bool force )
{
	int	scaledWidth, scaledHeight, scaledDepth;

	if( *depth > 1 && !GL_Support( R_TEXTURE_3D_EXT ))
		return; // nothing to resample

	scaledWidth = *width;
	scaledHeight = *height;
	scaledDepth = *depth;

	if( force || !GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
	{
		// find nearest power of two, rounding down if desired
		scaledWidth = NearestPOW( scaledWidth, gl_round_down->integer );
		scaledHeight = NearestPOW( scaledHeight, gl_round_down->integer );
		scaledDepth = NearestPOW( scaledDepth, gl_round_down->integer );
	}

	if( image_desc.tflags & TF_SKYSIDE )
	{
		// let people sample down the sky textures for speed
		scaledWidth >>= r_skymip->integer;
		scaledHeight >>= r_skymip->integer;
		scaledDepth >>= r_skymip->integer;
	}
	else if(!( image_desc.tflags & TF_NOPICMIP ))
	{
		// let people sample down the world textures for speed
		scaledWidth >>= r_picmip->integer;
		scaledHeight >>= r_picmip->integer;
		scaledDepth >>= r_picmip->integer;
	}

	// clamp to hardware limits
	if( *depth > 1 )
	{
		while( scaledWidth > glConfig.max_3d_texture_size || scaledHeight > glConfig.max_3d_texture_size || scaledDepth > glConfig.max_3d_texture_size )
		{
			scaledWidth >>= 1;
			scaledHeight >>= 1;
			scaledDepth >>= 1;
		}

		// FIXME: probably Xash supported 3d-reasmpling. we needs to testing it
		if( *width != scaledWidth || *height != scaledHeight || *depth != scaledDepth )
			Host_Error( "R_RoundImageDimensions: bad texture_3D dimensions (not a power of 2)\n" );
		if( scaledWidth > glConfig.max_3d_texture_size || scaledHeight > glConfig.max_3d_texture_size || scaledDepth > glConfig.max_3d_texture_size )
			Host_Error( "R_RoundImageDimensions: texture_3D is too large (resizing is not supported)\n" );
	}
	else if( image_desc.tflags & TF_CUBEMAP )
	{
		while( scaledWidth > glConfig.max_cubemap_texture_size || scaledHeight > glConfig.max_cubemap_texture_size )
		{
			scaledWidth >>= 1;
			scaledHeight >>= 1;
		}
	}
	else
	{
		while( scaledWidth > glConfig.max_2d_texture_size || scaledHeight > glConfig.max_2d_texture_size )
		{
			scaledWidth >>= 1;
			scaledHeight >>= 1;
		}
	}

	if( scaledWidth < 1 ) scaledWidth = 1;
	if( scaledHeight < 1 ) scaledHeight = 1;
	if( scaledDepth < 1 ) scaledDepth = 1;

	*width = scaledWidth;
	*height = scaledHeight;
	*depth = scaledDepth;
}

/*
=================
R_BuildMipMap

Operates in place, quartering the size of the texture
=================
*/
static void R_BuildMipMap( byte *in, int width, int height, bool isNormalMap )
{
	byte	*out = in;
	vec3_t	normal;
	int	x, y;

	width <<= 2;
	height >>= 1;

	if( isNormalMap )
	{
		for( y = 0; y < height; y++, in += width )
		{
			for( x = 0; x < width; x += 8, in += 8, out += 4 )
			{
				normal[0] = (in[0] * (1.0/127) - 1.0) + (in[4] * (1.0/127) - 1.0) + (in[width+0] * (1.0/127) - 1.0) + (in[width+4] * (1.0/127) - 1.0);
				normal[1] = (in[1] * (1.0/127) - 1.0) + (in[5] * (1.0/127) - 1.0) + (in[width+1] * (1.0/127) - 1.0) + (in[width+5] * (1.0/127) - 1.0);
				normal[2] = (in[2] * (1.0/127) - 1.0) + (in[6] * (1.0/127) - 1.0) + (in[width+2] * (1.0/127) - 1.0) + (in[width+6] * (1.0/127) - 1.0);

				if( !VectorNormalizeLength( normal )) VectorSet( normal, 0.0, 0.0, 1.0 );

				out[0] = (byte)(128 + 127 * normal[0]);
				out[1] = (byte)(128 + 127 * normal[1]);
				out[2] = (byte)(128 + 127 * normal[2]);
				out[3] = 255;
			}
		}
	}
	else
	{
		for( y = 0; y < height; y++, in += width )
		{
			for( x = 0; x < width; x += 8, in += 8, out += 4 )
			{
				out[0] = (in[0] + in[4] + in[width+0] + in[width+4]) >> 2;
				out[1] = (in[1] + in[5] + in[width+1] + in[width+5]) >> 2;
				out[2] = (in[2] + in[6] + in[width+2] + in[width+6]) >> 2;
				out[3] = (in[3] + in[7] + in[width+3] + in[width+7]) >> 2;
			}
		}
	}
}

/*
=================
R_ResampleTexture
=================
*/
static void R_ResampleTexture( const byte *source, int inWidth, int inHeight, int outWidth, int outHeight, bool isNormalMap )
{
	uint	frac, fracStep;
	uint	*in = (uint *)source;
	uint	p1[0x1000], p2[0x1000];
	byte	*pix1, *pix2, *pix3, *pix4;
	uint	*out = (uint *)image_desc.scaled;
	uint	*inRow1, *inRow2;
	vec3_t	normal;
	int	i, x, y;

	fracStep = inWidth * 0x10000 / outWidth;

	frac = fracStep >> 2;
	for( i = 0; i < outWidth; i++ )
	{
		p1[i] = 4 * (frac >> 16);
		frac += fracStep;
	}

	frac = (fracStep >> 2) * 3;
	for( i = 0; i < outWidth; i++ )
	{
		p2[i] = 4 * (frac >> 16);
		frac += fracStep;
	}

	if( isNormalMap )
	{
		for( y = 0; y < outHeight; y++, out += outWidth )
		{
			inRow1 = in + inWidth * (int)(((float)y + 0.25) * inHeight/outHeight);
			inRow2 = in + inWidth * (int)(((float)y + 0.75) * inHeight/outHeight);

			for( x = 0; x < outWidth; x++ )
			{
				pix1 = (byte *)inRow1 + p1[x];
				pix2 = (byte *)inRow1 + p2[x];
				pix3 = (byte *)inRow2 + p1[x];
				pix4 = (byte *)inRow2 + p2[x];

				normal[0] = (pix1[0] * (1.0/127) - 1.0) + (pix2[0] * (1.0/127) - 1.0) + (pix3[0] * (1.0/127) - 1.0) + (pix4[0] * (1.0/127) - 1.0);
				normal[1] = (pix1[1] * (1.0/127) - 1.0) + (pix2[1] * (1.0/127) - 1.0) + (pix3[1] * (1.0/127) - 1.0) + (pix4[1] * (1.0/127) - 1.0);
				normal[2] = (pix1[2] * (1.0/127) - 1.0) + (pix2[2] * (1.0/127) - 1.0) + (pix3[2] * (1.0/127) - 1.0) + (pix4[2] * (1.0/127) - 1.0);

				if( !VectorNormalizeLength( normal )) VectorSet( normal, 0.0, 0.0, 1.0 );

				((byte *)(out+x))[0] = (byte)(128 + 127 * normal[0]);
				((byte *)(out+x))[1] = (byte)(128 + 127 * normal[1]);
				((byte *)(out+x))[2] = (byte)(128 + 127 * normal[2]);
				((byte *)(out+x))[3] = 255;
			}
		}
	}
	else
	{
		for( y = 0; y < outHeight; y++, out += outWidth )
		{
			inRow1 = in + inWidth * (int)(((float)y + 0.25) * inHeight/outHeight);
			inRow2 = in + inWidth * (int)(((float)y + 0.75) * inHeight/outHeight);

			for( x = 0; x < outWidth; x++ )
			{
				pix1 = (byte *)inRow1 + p1[x];
				pix2 = (byte *)inRow1 + p2[x];
				pix3 = (byte *)inRow2 + p1[x];
				pix4 = (byte *)inRow2 + p2[x];

				((byte *)(out+x))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
				((byte *)(out+x))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
				((byte *)(out+x))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
				((byte *)(out+x))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
			}
		}
	}
}

/*
===============
R_GetPixelFormat

filled additional info
===============
*/
bool R_GetPixelFormat( const char *name, rgbdata_t *pic, uint tex_flags )
{
	int	w, h, d, i, s, BlockSize;
	size_t	mipsize, totalsize = 0;

	if( !pic ) return false; // pass images with NULL buffer (e.g. shadowmaps, portalmaps )
	Mem_EmptyPool( r_imagepool ); // flush buffers		
	Mem_Set( &image_desc, 0, sizeof( image_desc ));

	BlockSize = PFDesc( pic->type )->block;
	image_desc.bpp = PFDesc( pic->type )->bpp;
	image_desc.bpc = PFDesc( pic->type )->bpc;
	image_desc.glFormat = PFDesc( pic->type )->glFormat;
	image_desc.glType = PFDesc( pic->type )->glType;
	image_desc.format = pic->type;
	image_desc.numSides = 1;

	image_desc.texTarget = image_desc.glTarget = GL_TEXTURE_2D;
	image_desc.depth = d = pic->depth;
	image_desc.width = w = pic->width;
	image_desc.height = h = pic->height;
	image_desc.flags = pic->flags;
	image_desc.tflags = tex_flags;

	image_desc.bps = image_desc.width * image_desc.bpp * image_desc.bpc;
	image_desc.SizeOfPlane = image_desc.bps * image_desc.height;
	image_desc.SizeOfData = image_desc.SizeOfPlane * image_desc.depth;
	image_desc.glSamples = R_GetSamples( image_desc.flags );
	image_desc.BitsCount = pic->bitsCount;

	// now correct buffer size
	for( i = 0; i < pic->numMips; i++, totalsize += mipsize )
	{
		mipsize = R_GetImageSize( BlockSize, w, h, d, image_desc.bpp, image_desc.BitsCount / 8 );
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1;
	}

	if( image_desc.tflags & TF_DEPTHMAP )
	{
		image_desc.glFormat = GL_DEPTH_COMPONENT;
	}
	else if( image_desc.depth > 1 )
	{
		if( GL_Support( R_TEXTURE_3D_EXT ))
			image_desc.texTarget = image_desc.glTarget = GL_TEXTURE_3D;
		else MsgDev( D_ERROR, "R_GetPixelFormat: GL_TEXTURE_3D isn't supported\n" );
	}
	else if( image_desc.tflags & TF_CUBEMAP )
	{
		if( GL_Support( R_TEXTURECUBEMAP_EXT ))
		{		
			if( pic->flags & IMAGE_CUBEMAP )
			{
				image_desc.numSides = 6;
				image_desc.glTarget = GL_TEXTURE_CUBE_MAP_ARB;
				image_desc.texTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB;
			}
			else
			{
				MsgDev( D_WARN, "R_GetPixelFormat: %s it's not a cubemap image\n", name );
				image_desc.tflags &= ~TF_CUBEMAP;
			}
		}
		else
		{
			MsgDev( D_WARN, "R_GetPixelFormat: cubemaps isn't supported, %s ignored\n", name );
			image_desc.tflags &= ~TF_CUBEMAP;
		}
	}

	if( image_desc.tflags & TF_NOMIPMAP )
	{
		// don't build mips for sky and hud pics
		image_desc.MipCount = 1; // and ignore it to load
	} 
	else image_desc.MipCount = pic->numMips;
		
	if( image_desc.MipCount < 1 ) image_desc.MipCount = 1;
	image_desc.pal = pic->palette;

	// check for permanent images
	if( image_desc.format == PF_RGBA_GN ) image_desc.tflags |= TF_STATIC;
	if( image_desc.tflags & TF_NOPICMIP ) image_desc.tflags |= TF_STATIC;

	// restore temp dimensions
	w = image_desc.width;
	h = image_desc.height;
	d = image_desc.depth;
	s = w * h * d;

	// apply texture type (R_ShowTextures uses it)
	if( image_desc.format == PF_RGBA_GN )
		image_desc.texType = TEX_SYSTEM;
	else if( image_desc.tflags & (TF_NOPICMIP|TF_NOMIPMAP))
		image_desc.texType = TEX_NOMIP;
	else if( image_desc.tflags & TF_SKYSIDE )
		image_desc.texType = TEX_SKYBOX;
	else image_desc.texType = TEX_GENERIC;

	// calc immediate buffers
	R_RoundImageDimensions( &w, &h, &d, false );

	image_desc.source = Mem_Alloc( r_imagepool, s * 4 );		// source buffer
	image_desc.scaled = Mem_Alloc( r_imagepool, w * h * d * 4 );	// scaled buffer
	totalsize *= image_desc.numSides;

	if( totalsize != pic->size ) // sanity check
	{
		MsgDev( D_ERROR, "R_GetPixelFormat: %s has invalid size (%i should be %i)\n", name, pic->size, totalsize );
		return false;
	}
	if( s&3 )
	{
		// will be resample, just tell me for debug targets
		MsgDev( D_NOTE, "R_GetPixelFormat: %s s&3 [%d x %d]\n", name, image_desc.width, image_desc.height );
	}	
	return true;
}

/*
===============
R_SetPixelFormat

prepare image to upload in video memory
===============
*/
void R_SetPixelFormat( int width, int height, int depth )
{
	int	BlockSize;
	
	BlockSize = PFDesc( image_desc.format )->block;
	image_desc.bpp = PFDesc( image_desc.format )->bpp;
	image_desc.bpc = PFDesc( image_desc.format )->bpc;
	image_desc.glType = PFDesc( image_desc.format )->glType;

	if( image_desc.tflags & TF_DEPTHMAP )
		image_desc.glFormat = GL_DEPTH_COMPONENT;
	else image_desc.glFormat = PFDesc( image_desc.format )->glFormat;
	
	image_desc.depth = depth;
	image_desc.width = width;
	image_desc.height = height;

	image_desc.bps = image_desc.width * image_desc.bpp * image_desc.bpc;
	image_desc.SizeOfPlane = image_desc.bps * image_desc.height;
	image_desc.SizeOfData = image_desc.SizeOfPlane * image_desc.depth;

	// NOTE: size of current miplevel or cubemap side, not total (filesize - sizeof(header))
	image_desc.SizeOfFile = R_GetImageSize( BlockSize, width, height, depth, image_desc.bpp, image_desc.BitsCount / 8);
}

rgbdata_t *R_ForceImageToRGBA( rgbdata_t *pic )
{
	// no need additional check - image lib make it self
	Image_Process( &pic, 0, 0, IMAGE_FORCE_RGBA );
	return pic;
}

/*
=======================================================================

 IMAGE PROGRAM FUNCTIONS

=======================================================================
*/
/*
=================
R_AddImages

Adds the given images together
=================
*/
static rgbdata_t *R_AddImages( rgbdata_t *in1, rgbdata_t *in2 )
{
	rgbdata_t	*out;
	int	width, height;
	int	r, g, b, a;
	int	x, y;

	// make sure what we processing RGBA images
	in1 = R_ForceImageToRGBA( in1 );
	in2 = R_ForceImageToRGBA( in2 );
	width = in1->width, height = in1->height;
	out = in1;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = in1->buffer[4*(y*width+x)+0] + in2->buffer[4*(y*width+x)+0];
			g = in1->buffer[4*(y*width+x)+1] + in2->buffer[4*(y*width+x)+1];
			b = in1->buffer[4*(y*width+x)+2] + in2->buffer[4*(y*width+x)+2];
			a = in1->buffer[4*(y*width+x)+3] + in2->buffer[4*(y*width+x)+3];
			out->buffer[4*(y*width+x)+0] = bound( 0, r, 255 );
			out->buffer[4*(y*width+x)+1] = bound( 0, g, 255 );
			out->buffer[4*(y*width+x)+2] = bound( 0, b, 255 );
			out->buffer[4*(y*width+x)+3] = bound( 0, a, 255 );
		}
	}
	FS_FreeImage( in2 );

	return out;
}

/*
=================
R_MultiplyImages

Multiplies the given images
=================
*/
static rgbdata_t *R_MultiplyImages( rgbdata_t *in1, rgbdata_t *in2 )
{
	rgbdata_t	*out;
	int	width, height;
	int	r, g, b, a;
	int	x, y;

	// make sure what we processing RGBA images
	in1 = R_ForceImageToRGBA( in1 );
	in2 = R_ForceImageToRGBA( in2 );
	width = in1->width, height = in1->height;
	out = in1;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = in1->buffer[4*(y*width+x)+0] * (in2->buffer[4*(y*width+x)+0] * (1.0/255));
			g = in1->buffer[4*(y*width+x)+1] * (in2->buffer[4*(y*width+x)+1] * (1.0/255));
			b = in1->buffer[4*(y*width+x)+2] * (in2->buffer[4*(y*width+x)+2] * (1.0/255));
			a = in1->buffer[4*(y*width+x)+3] * (in2->buffer[4*(y*width+x)+3] * (1.0/255));
			out->buffer[4*(y*width+x)+0] = bound( 0, r, 255 );
			out->buffer[4*(y*width+x)+1] = bound( 0, g, 255 );
			out->buffer[4*(y*width+x)+2] = bound( 0, b, 255 );
			out->buffer[4*(y*width+x)+3] = bound( 0, a, 255 );
		}
	}
	FS_FreeImage( in2 );

	return out;
}

/*
=================
R_BiasImage

Biases the given image
=================
*/
static rgbdata_t *R_BiasImage( rgbdata_t *in, const vec4_t bias )
{
	rgbdata_t	*out;
	int	width, height;
	int	r, g, b, a;
	int	x, y;

	// make sure what we processing RGBA image
	in = R_ForceImageToRGBA( in );
	width = in->width, height = in->height;
	out = in;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = in->buffer[4*(y*width+x)+0] + (255 * bias[0]);
			g = in->buffer[4*(y*width+x)+1] + (255 * bias[1]);
			b = in->buffer[4*(y*width+x)+2] + (255 * bias[2]);
			a = in->buffer[4*(y*width+x)+3] + (255 * bias[3]);
			out->buffer[4*(y*width+x)+0] = bound( 0, r, 255 );
			out->buffer[4*(y*width+x)+1] = bound( 0, g, 255 );
			out->buffer[4*(y*width+x)+2] = bound( 0, b, 255 );
			out->buffer[4*(y*width+x)+3] = bound( 0, a, 255 );
		}
	}
	return out;
}

/*
=================
R_ScaleImage

Scales the given image
=================
*/
static rgbdata_t *R_ScaleImage( rgbdata_t *in, const vec4_t scale )
{
	rgbdata_t	*out;
	int	width, height;
	int	r, g, b, a;
	int	x, y;

	// make sure what we processing RGBA image
	in = R_ForceImageToRGBA( in );
	width = in->width, height = in->height;
	out = in;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = in->buffer[4*(y*width+x)+0] * scale[0];
			g = in->buffer[4*(y*width+x)+1] * scale[1];
			b = in->buffer[4*(y*width+x)+2] * scale[2];
			a = in->buffer[4*(y*width+x)+3] * scale[3];
			out->buffer[4*(y*width+x)+0] = bound( 0, r, 255 );
			out->buffer[4*(y*width+x)+1] = bound( 0, g, 255 );
			out->buffer[4*(y*width+x)+2] = bound( 0, b, 255 );
			out->buffer[4*(y*width+x)+3] = bound( 0, a, 255 );
		}
	}
	return out;
}

/*
=================
R_InvertColor

Inverts the color channels of the given image
=================
*/
static rgbdata_t *R_InvertColor( rgbdata_t *in )
{
	rgbdata_t	*out;
	int	width, height;
	int	x, y;

	// make sure what we processing RGBA image
	in = R_ForceImageToRGBA( in );
	width = in->width, height = in->height;
	out = in;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			out->buffer[4*(y*width+x)+0] = 255 - in->buffer[4*(y*width+x)+0];
			out->buffer[4*(y*width+x)+1] = 255 - in->buffer[4*(y*width+x)+1];
			out->buffer[4*(y*width+x)+2] = 255 - in->buffer[4*(y*width+x)+2];
		}
	}
	return out;
}

/*
=================
R_InvertAlpha

Inverts the alpha channel of the given image
=================
*/
static rgbdata_t *R_InvertAlpha( rgbdata_t *in )
{
	rgbdata_t	*out;
	int	width, height;
	int	x, y;

	// make sure what we processing RGBA image
	in = R_ForceImageToRGBA( in );
	width = in->width, height = in->height;
	out = in;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
			out->buffer[4*(y*width+x)+3] = 255 - in->buffer[4*(y*width+x)+3];
	}
	return out;
}

/*
=================
R_MakeIntensity

Converts the given image to intensity
=================
*/
static rgbdata_t *R_MakeIntensity( rgbdata_t *in )
{
	rgbdata_t	*out;
	int	width, height;
	byte	intensity;
	float	r, g, b;
	int	x, y;

	// make sure what we processing RGBA image
	in = R_ForceImageToRGBA( in );
	width = in->width, height = in->height;
	out = in;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = r_luminanceTable[in->buffer[4*(y*width+x)+0]][0];
			g = r_luminanceTable[in->buffer[4*(y*width+x)+1]][1];
			b = r_luminanceTable[in->buffer[4*(y*width+x)+2]][2];

			intensity = (byte)(r + g + b);

			out->buffer[4*(y*width+x)+0] = intensity;
			out->buffer[4*(y*width+x)+1] = intensity;
			out->buffer[4*(y*width+x)+2] = intensity;
			out->buffer[4*(y*width+x)+3] = intensity;
		}
	}
	return out;
}

/*
=================
R_MakeLuminance

Converts the given image to luminance
=================
*/
static rgbdata_t *R_MakeLuminance( rgbdata_t *in )
{
	rgbdata_t	*out;
	int	width, height;
	byte	luminance;
	float	r, g, b;
	int	x, y;

	// make sure what we processing RGBA image
	in = R_ForceImageToRGBA( in );
	width = in->width, height = in->height;
	out = in;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = r_luminanceTable[in->buffer[4*(y*width+x)+0]][0];
			g = r_luminanceTable[in->buffer[4*(y*width+x)+1]][1];
			b = r_luminanceTable[in->buffer[4*(y*width+x)+2]][2];

			luminance = (byte)(r + g + b);

			out->buffer[4*(y*width+x)+0] = luminance;
			out->buffer[4*(y*width+x)+1] = luminance;
			out->buffer[4*(y*width+x)+2] = luminance;
			out->buffer[4*(y*width+x)+3] = 255;
		}
	}
	return out;
}

/*
=================
R_MakeGlow

Converts the given image to glow (LUMA)
=================
*/
static rgbdata_t *R_MakeGlow( rgbdata_t *in )
{
	rgbdata_t	*out;
	int	width, height;
	byte	r, g, b;
	int	x, y;

	// make sure what we processing RGBA image
	in = R_ForceImageToRGBA( in );
	width = in->width, height = in->height;
	out = in;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = r_glowTable[in->buffer[4*(y*width+x)+0]][0];
			g = r_glowTable[in->buffer[4*(y*width+x)+1]][1];
			b = r_glowTable[in->buffer[4*(y*width+x)+2]][2];

			out->buffer[4*(y*width+x)+0] = r;
			out->buffer[4*(y*width+x)+1] = g;
			out->buffer[4*(y*width+x)+2] = b;
			out->buffer[4*(y*width+x)+3] = 255; // kill alpha if any
		}
	}
	return out;
}

static rgbdata_t *R_MakeImageBlock( rgbdata_t *in , int block[4] )
{
	byte	*fin, *out;
	int	i, x, y, xl, yl, xh, yh, w, h;
	int	linedelta;

	// make sure what we processing RGBA image
	in = R_ForceImageToRGBA( in );

	xl = block[0];
	yl = block[1];
	w = block[2];
	h = block[3];
	xh = xl + w;
	yh = yl + h;

	image_desc.source = Mem_Realloc( r_temppool, image_desc.source, w * h * 4 );
	out = image_desc.source;
          
	fin = in->buffer + (yl * in->width + xl) * 4;
	linedelta = (in->width - w) * 4;

	// cut block from source
	for( y = yl; y < yh; y++ )
	{
		for( x = xl; x < xh; x++ )
			for( i = 0; i < 4; i++ )
				*out++ = *fin++;
		fin += linedelta;
	}

	// copy result back
	in->buffer = Mem_Realloc( r_temppool, in->buffer, w * h * 4 );
	Mem_Copy( in->buffer, image_desc.source, w * h * 4 );
	in->size = w * h * 4;
	in->height = h;
	in->width = w;

	return in;
}

/*
=================
R_MakeAlpha

Converts the given image to alpha
=================
*/
static rgbdata_t *R_MakeAlpha( rgbdata_t *in )
{
	rgbdata_t	*out;
	int	width, height;
	byte	alpha;
	float	r, g, b;
	int	x, y;

	// make sure what we processing RGBA image
	in = R_ForceImageToRGBA( in );
	width = in->width, height = in->height;
	out = in;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = r_luminanceTable[in->buffer[4*(y*width+x)+0]][0];
			g = r_luminanceTable[in->buffer[4*(y*width+x)+1]][1];
			b = r_luminanceTable[in->buffer[4*(y*width+x)+2]][2];

			alpha = (byte)(r + g + b);

			out->buffer[4*(y*width+x)+0] = 255;
			out->buffer[4*(y*width+x)+1] = 255;
			out->buffer[4*(y*width+x)+2] = 255;
			out->buffer[4*(y*width+x)+3] = alpha;
		}
	}
	return out;
}

/*
=================
R_HeightMap

Converts the given height map to a normal map
=================
*/
static rgbdata_t *R_HeightMap( rgbdata_t *in, float bumpScale )
{
	byte	*out;
	int	width, height;
	vec3_t	normal;
	float	r, g, b;
	float	c, cx, cy;
	int	x, y;

	// make sure what we processing RGBA image
	in = R_ForceImageToRGBA( in );
	width = in->width, height = in->height;
	out = image_desc.source = Mem_Realloc( r_temppool, image_desc.source, width * height * 4 );

	if( !bumpScale ) bumpScale = 1.0f;
	bumpScale *= max( 0, r_lighting_bumpscale->value );

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = r_luminanceTable[in->buffer[4*(y*width+x)+0]][0];
			g = r_luminanceTable[in->buffer[4*(y*width+x)+1]][1];
			b = r_luminanceTable[in->buffer[4*(y*width+x)+2]][2];

			c = (r + g + b) * (1.0/255);

			r = r_luminanceTable[in->buffer[4*(y*width+((x+1)%width))+0]][0];
			g = r_luminanceTable[in->buffer[4*(y*width+((x+1)%width))+1]][1];
			b = r_luminanceTable[in->buffer[4*(y*width+((x+1)%width))+2]][2];

			cx = (r + g + b) * (1.0/255);

			r = r_luminanceTable[in->buffer[4*(((y+1)%height)*width+x)+0]][0];
			g = r_luminanceTable[in->buffer[4*(((y+1)%height)*width+x)+1]][1];
			b = r_luminanceTable[in->buffer[4*(((y+1)%height)*width+x)+2]][2];

			cy = (r + g + b) * (1.0/255);

			normal[0] = (c - cx) * bumpScale;
			normal[1] = (c - cy) * bumpScale;
			normal[2] = 1.0;

			if(!VectorNormalizeLength( normal )) VectorSet( normal, 0.0f, 0.0f, 1.0f );
			out[4*(y*width+x)+0] = (byte)(128 + 127 * normal[0]);
			out[4*(y*width+x)+1] = (byte)(128 + 127 * normal[1]);
			out[4*(y*width+x)+2] = (byte)(128 + 127 * normal[2]);
			out[4*(y*width+x)+3] = 255;
		}
	}

	// copy result back
	Mem_Copy( in->buffer, out, width * height * 4 );

	return in;
}

/*
=================
R_AddNormals

Adds the given normal maps together
=================
*/
static rgbdata_t *R_AddNormals( rgbdata_t *in1, rgbdata_t *in2 )
{
	byte	*out;
	int	width, height;
	vec3_t	normal;
	int	x, y;

	// make sure what we processing RGBA images
	in1 = R_ForceImageToRGBA( in1 );
	in2 = R_ForceImageToRGBA( in2 );
	width = in1->width, height = in1->height;
	out = image_desc.source;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			normal[0] = (in1->buffer[4*(y*width+x)+0] * (1.0/127) - 1.0) + (in2->buffer[4*(y*width+x)+0] * (1.0/127) - 1.0);
			normal[1] = (in1->buffer[4*(y*width+x)+1] * (1.0/127) - 1.0) + (in2->buffer[4*(y*width+x)+1] * (1.0/127) - 1.0);
			normal[2] = (in1->buffer[4*(y*width+x)+2] * (1.0/127) - 1.0) + (in2->buffer[4*(y*width+x)+2] * (1.0/127) - 1.0);

			if(!VectorNormalizeLength( normal )) VectorSet( normal, 0.0f, 0.0f, 1.0f );

			out[4*(y*width+x)+0] = (byte)(128 + 127 * normal[0]);
			out[4*(y*width+x)+1] = (byte)(128 + 127 * normal[1]);
			out[4*(y*width+x)+2] = (byte)(128 + 127 * normal[2]);
			out[4*(y*width+x)+3] = 255;
		}
	}

	// copy result back
	Mem_Copy( in1->buffer, out, width * height * 4 );
	FS_FreeImage( in2 );

	return in1;
}

/*
=================
R_SmoothNormals

Smoothes the given normal map
=================
*/
static rgbdata_t *R_SmoothNormals( rgbdata_t *in )
{
	byte	*out;
	int	width, height;
	uint	frac, fracStep;
	uint	p1[0x1000], p2[0x1000];
	byte	*pix1, *pix2, *pix3, *pix4;
	uint	*inRow1, *inRow2;
	vec3_t	normal;
	int	i, x, y;

	// make sure what we processing RGBA image
	in = R_ForceImageToRGBA( in );
	width = in->width, height = in->height;
	out = image_desc.source;

	fracStep = 0x10000;
	frac = fracStep>>2;
	for( i = 0; i < width; i++ )
	{
		p1[i] = 4 * (frac>>16);
		frac += fracStep;
	}

	frac = (fracStep>>2) * 3;
	for( i = 0; i < width; i++ )
	{
		p2[i] = 4 * (frac>>16);
		frac += fracStep;
	}

	for( y = 0; y < height; y++ )
	{
		inRow1 = (uint *)in->buffer + width * (int)((float)y + 0.25);
		inRow2 = (uint *)in->buffer + width * (int)((float)y + 0.75);

		for( x = 0; x < width; x++ )
		{
			pix1 = (byte *)inRow1 + p1[x];
			pix2 = (byte *)inRow1 + p2[x];
			pix3 = (byte *)inRow2 + p1[x];
			pix4 = (byte *)inRow2 + p2[x];

			normal[0] = (pix1[0] * (1.0/127) - 1.0) + (pix2[0] * (1.0/127) - 1.0) + (pix3[0] * (1.0/127) - 1.0) + (pix4[0] * (1.0/127) - 1.0);
			normal[1] = (pix1[1] * (1.0/127) - 1.0) + (pix2[1] * (1.0/127) - 1.0) + (pix3[1] * (1.0/127) - 1.0) + (pix4[1] * (1.0/127) - 1.0);
			normal[2] = (pix1[2] * (1.0/127) - 1.0) + (pix2[2] * (1.0/127) - 1.0) + (pix3[2] * (1.0/127) - 1.0) + (pix4[2] * (1.0/127) - 1.0);

			if( !VectorNormalizeLength( normal )) VectorSet( normal, 0.0f, 0.0f, 1.0f );
			out[4*(y*width+x)+0] = (byte)(128 + 127 * normal[0]);
			out[4*(y*width+x)+1] = (byte)(128 + 127 * normal[1]);
			out[4*(y*width+x)+2] = (byte)(128 + 127 * normal[2]);
			out[4*(y*width+x)+3] = 255;
		}
	}

	// copy result back
	Mem_Copy( in->buffer, out, width * height * 4 );

	return in;
}

/*
================
R_IncludeDepthmap

Write depthmap into alpha-channel the given normal map
================
*/
static rgbdata_t *R_IncludeDepthmap( rgbdata_t *in1, rgbdata_t *in2 )
{
	int	i;
	byte	*pic1, *pic2;

	// make sure what we processing RGBA images
	in1 = R_ForceImageToRGBA( in1 );
	in2 = R_ForceImageToRGBA( in2 );

	pic1 = in1->buffer;
	pic2 = in2->buffer;

	for( i = (in1->width * in1->height) - 1; i > 0; i--, pic1 += 4, pic2 += 4 )
	{
		if( in2->flags & IMAGE_HAS_COLOR )
			pic1[3] = ((int)pic2[0] + (int)pic2[1] + (int)pic2[2]) / 3;
		else if( in2->flags & IMAGE_HAS_ALPHA )
			pic1[3] = pic2[3];
		else pic1[3] = pic2[0];
	}

	FS_FreeImage( in2 );
	in1->flags |= (IMAGE_HAS_COLOR|IMAGE_HAS_ALPHA);

	return in1;
}

/*
================
R_ClearPixels

clear specified area: color or alpha
================
*/
static rgbdata_t *R_ClearPixels( rgbdata_t *in, bool clearAlpha )
{
	int	i;
	byte	*pic;

	// make sure what we processing RGBA images
	in = R_ForceImageToRGBA( in );
	pic = in->buffer;

	if( clearAlpha )
	{
		for( i = 0; i < in->width * in->height && in->flags & IMAGE_HAS_ALPHA; i++ )
			pic[(i<<2)+3] = 0xFF;
	}
	else
	{
		// clear color or greyscale image otherwise
		for( i = 0; i < in->width * in->height; i++ )
			pic[(i<<2)+0] = pic[(i<<2)+1] = pic[(i<<2)+2] = 0xFF;
	}
	return in;
}


/*
=================
R_ParseAdd
=================
*/
static rgbdata_t *R_ParseAdd( script_t *script, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t	*pic1, *pic2;
	int	samples1, samples2;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp(token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'add'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'add'\n" );
		return NULL;
	}

	pic1 = R_LoadImage( script, token.string, NULL, 0, &samples1, flags );
	if( !pic1 ) return NULL;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "," ))
	{
		MsgDev( D_WARN, "expected ',', found '%s' instead for 'add'\n", token.string );
		FS_FreeImage( pic1 );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'add'\n" );
		FS_FreeImage( pic1 );
		return NULL;
	}

	pic2 = R_LoadImage( script, token.string, NULL, 0, &samples2, flags );
	if( !pic2 )
	{
		FS_FreeImage( pic1 );
		return NULL;
	}

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'add'\n", token.string );
		FS_FreeImage( pic1 );
		FS_FreeImage( pic2 );
		return NULL;
	}

	if( pic1->width != pic2->width || pic1->height != pic2->height )
	{
		MsgDev( D_WARN, "images for 'add' have mismatched dimensions [%ix%i] != [%ix%i]\n",
			pic1->width, pic1->height, pic2->width, pic2->height );

		FS_FreeImage( pic1 );
		FS_FreeImage( pic2 );
		return NULL;
	}

	*samples = R_SetSamples( samples1, samples2 );
	if( *samples != 1 )
	{
		*flags &= ~TF_INTENSITY;
		*flags &= ~TF_ALPHA;
	}
	return R_AddImages( pic1, pic2 );
}

/*
=================
R_ParseMultiply
=================
*/
static rgbdata_t *R_ParseMultiply( script_t *script, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t	*pic1, *pic2;
	int	samples1, samples2;

	Com_ReadToken( script, 0, &token );

	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'multiply'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'multiply'\n" );
		return NULL;
	}

	pic1 = R_LoadImage( script, token.string, NULL, 0, &samples1, flags );
	if( !pic1 ) return NULL;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "," ))
	{
		MsgDev( D_WARN, "expected ',', found '%s' instead for 'multiply'\n", token.string );
		FS_FreeImage( pic1 );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'multiply'\n" );
		FS_FreeImage( pic1 );
		return NULL;
	}

	pic2 = R_LoadImage( script, token.string, NULL, 0, &samples2, flags );
	if( !pic2 )
	{
		FS_FreeImage( pic1 );
		return NULL;
	}

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'multiply'\n", token.string );

		FS_FreeImage( pic1 );
		FS_FreeImage( pic2 );
		return NULL;
	}

	if( pic1->width != pic2->width || pic1->height != pic2->height )
	{
		MsgDev( D_WARN, "images for 'multiply' have mismatched dimensions [%ix%i] != [%ix%i]\n",
			pic1->width, pic1->height, pic2->width, pic2->height );

		FS_FreeImage( pic1 );
		FS_FreeImage( pic2 );
		return NULL;
	}

	*samples = R_SetSamples( samples1, samples2 );

	if( *samples != 1 )
	{
		*flags &= ~TF_INTENSITY;
		*flags &= ~TF_ALPHA;
	}
	return R_MultiplyImages( pic1, pic2 );
}

/*
=================
R_ParseBias
=================
*/
static rgbdata_t *R_ParseBias( script_t *script, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t	*pic;
	vec4_t	bias;
	int	i;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'bias'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'bias'\n" );
		return NULL;
	}

	pic = R_LoadImage( script, token.string, NULL, 0, samples, flags );
	if( !pic ) return NULL;

	for( i = 0; i < 4; i++ )
	{
		Com_ReadToken( script, 0, &token );
		if( com.stricmp( token.string, "," ))
		{
			MsgDev( D_WARN, "expected ',', found '%s' instead for 'bias'\n", token.string );
			FS_FreeImage( pic );
			return NULL;
		}

		if( !Com_ReadFloat( script, 0, &bias[i] ))
		{
			MsgDev( D_WARN, "missing parameters for 'bias'\n" );
			FS_FreeImage( pic );
			return NULL;
		}
	}

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'bias'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}

	if( *samples < 3 ) *samples += 2;
	*flags &= ~TF_INTENSITY;
	*flags &= ~TF_ALPHA;

	return R_BiasImage( pic, bias );
}

/*
=================
R_ParseScale
=================
*/
static rgbdata_t *R_ParseScale( script_t *script, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t	*pic;
	vec4_t	scale;
	int	i;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'scale'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'scale'\n" );
		return NULL;
	}

	pic = R_LoadImage( script, token.string, NULL, 0, samples, flags );
	if( !pic ) return NULL;

	for( i = 0; i < 4; i++ )
	{
		Com_ReadToken( script, 0, &token );
		if( com.stricmp( token.string, "," ))
		{
			MsgDev( D_WARN, "expected ',', found '%s' instead for 'scale'\n", token.string );
			FS_FreeImage( pic );
			return NULL;
		}

		if( !Com_ReadFloat( script, 0, &scale[i] ))
		{
			MsgDev( D_WARN, "missing parameters for 'scale'\n" );
			FS_FreeImage( pic );
			return NULL;
		}
	}

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'scale'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}

	if( *samples < 3 ) *samples += 2;
	*flags &= ~TF_INTENSITY;
	*flags &= ~TF_ALPHA;

	return R_ScaleImage( pic, scale );
}

/*
=================
R_ParseInvertColor
=================
*/
static rgbdata_t *R_ParseInvertColor( script_t *script, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t	*pic;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'invertColor'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'invertColor'\n" );
		return NULL;
	}

	pic = R_LoadImage( script, token.string, NULL, 0, samples, flags );
	if( !pic ) return NULL;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'invertColor'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}
	return R_InvertColor( pic );
}

/*
=================
R_ParseInvertAlpha
=================
*/
static rgbdata_t *R_ParseInvertAlpha( script_t *script, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t	*pic;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'invertAlpha'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'invertAlpha'\n" );
		return NULL;
	}

	pic = R_LoadImage( script, token.string, NULL, 0, samples, flags );
	if( !pic ) return NULL;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'invertAlpha'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}
	return R_InvertAlpha( pic );
}

/*
=================
R_ParseMakeIntensity
=================
*/
static rgbdata_t *R_ParseMakeIntensity( script_t *script, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t	*pic;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'makeIntensity'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'makeIntensity'\n" );
		return NULL;
	}

	pic = R_LoadImage( script, token.string, NULL, 0, samples, flags );
	if( !pic ) return NULL;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'makeIntensity'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}

	*samples = 1;
	*flags |= TF_INTENSITY;
	*flags &= ~TF_ALPHA;
	*flags &= ~TF_NORMALMAP;

	return R_MakeIntensity( pic );
}

/*
=================
R_ParseMakeLuminance
=================
*/
static rgbdata_t *R_ParseMakeLuminance( script_t *script, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t	*pic;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'makeLuminance'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'makeLuminance'\n" );
		return NULL;
	}

	pic = R_LoadImage( script, token.string, NULL, 0, samples, flags );
	if( !pic ) return NULL;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'makeLuminance'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}

	*samples = 1;
	*flags &= ~TF_INTENSITY;
	*flags &= ~TF_ALPHA;
	*flags &= ~TF_NORMALMAP;

	return R_MakeIntensity( pic );
}

/*
=================
R_ParseMakeAlpha
=================
*/
static rgbdata_t *R_ParseMakeAlpha( script_t *script, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t *pic;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'makeAlpha'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'makeAlpha'\n" );
		return NULL;
	}

	pic = R_LoadImage( script, token.string, NULL, 0, samples, flags );
	if( !pic ) return NULL;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'makeAlpha'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}

	*samples = 1;
	*flags &= ~TF_INTENSITY;
	*flags |= TF_ALPHA;
	*flags &= ~TF_NORMALMAP;

	return R_MakeAlpha( pic );
}

/*
=================
R_ParseMakeGlow
=================
*/
static rgbdata_t *R_ParseMakeGlow( script_t *script, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t *pic;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'makeGlow'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'makeGlow'\n" );
		return NULL;
	}

	pic = R_LoadImage( script, token.string, NULL, 0, samples, flags );
	if( !pic ) return NULL;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'makeGlow'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}

	*samples = 3;

	return R_MakeGlow( pic );
}

static rgbdata_t *R_ParseStudioSkin( script_t *script, const byte *buf, size_t size, int *samples, texFlags_t *flags )
{
	token_t		token;
	rgbdata_t 	*pic;
	string		model_path;
	string		modelT_path;
	string		skinname;
	dstudiohdr_t	hdr;
	file_t		*f;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'Studio'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'Studio'\n" );
		return NULL;
	}

	// NOTE: studio skin show as 'models/props/flame1.mdl/flame2a.bmp'
	FS_ExtractFilePath( token.string, model_path );
	FS_StripExtension( model_path );
	com.snprintf( modelT_path, MAX_STRING, "%sT.mdl", model_path );
	FS_DefaultExtension( model_path, ".mdl" );
	FS_FileBase( token.string, skinname );

	if( buf && size )
	{
		// load it in
		pic = R_LoadImage( script, va( "#%s.mdl", skinname ), buf, size, samples, flags );
		if( !pic ) return NULL;
		goto studio_done;
          }
	FS_DefaultExtension( skinname, ".bmp" );
	f = FS_Open( model_path, "rb" );
	if( !f )
	{
		MsgDev( D_WARN, "'Studio' can't find studiomodel %s\n", model_path );
		return NULL;
	}
	if( FS_Read( f, &hdr, sizeof( hdr )) != sizeof( hdr ))
	{
		MsgDev( D_WARN, "'Studio' %s probably corrupted\n", model_path );
		FS_Close( f );
		return NULL;		
	}
	SwapBlock( (int *)&hdr, sizeof( hdr ));

	if( hdr.numtextures == 0 )
	{
		// textures are keep seperate
		FS_Close( f );
		f = FS_Open( modelT_path, "rb" );
		if( !f )
		{
			MsgDev( D_WARN, "'Studio' can't find studiotextures %s\n", modelT_path );
			return NULL;
		}
		if( FS_Read( f, &hdr, sizeof( hdr )) != sizeof( hdr ))
		{
			MsgDev( D_WARN, "'Studio' %s probably corrupted\n", modelT_path );
			FS_Close( f );
			return NULL;		
		}
		SwapBlock( (int *)&hdr, sizeof( hdr ));
	}

	if( hdr.textureindex > 0 && hdr.numtextures <= MAXSTUDIOSKINS )
	{
		// all ok, can load model into memory
		dstudiotexture_t	*ptexture, *tex;
		size_t		mdl_size, tex_size;
		byte		*pin;
		int		i;

		FS_Seek( f, 0, SEEK_END );
		mdl_size = FS_Tell( f );
		FS_Seek( f, 0, SEEK_SET );

		pin = Mem_Alloc( r_imagepool, mdl_size );
		if( FS_Read( f, pin, mdl_size ) != mdl_size )
		{
			MsgDev( D_WARN, "'Studio' %s probably corrupted\n", model_path );
			Mem_Free( pin );
			FS_Close( f );
			return NULL;
		}
		ptexture = (dstudiotexture_t *)(pin + hdr.textureindex);

		// find specified texture
		for( i = 0; i < hdr.numtextures; i++ )
		{
			if( !com.stricmp( ptexture[i].name, skinname ))
				break; // found
		}
		if( i == hdr.numtextures )
		{
			MsgDev( D_WARN, "'Studio' %s doesn't have skin %s\n", model_path, skinname );
			Mem_Free( pin );
			FS_Close( f );
			return NULL;
		}
		tex = ptexture + i;

		// NOTE: replace index with pointer to start of imagebuffer, ImageLib expected it
		tex->index = (int)pin + tex->index;
		tex_size = sizeof( dstudiotexture_t ) + tex->width * tex->height + 768;

		// load studio texture and bind it
		FS_FileBase( skinname, skinname );

		// load it in
		pic = R_LoadImage( script, va( "#%s.mdl", tex->name ), (byte *)tex, tex_size, samples, flags );

		// shutdown operations
		Mem_Free( pin );
		FS_Close( f );

		if( !pic ) return NULL;
	}
	else
	{
		MsgDev( D_WARN, "'Studio' %s has invalid skin count\n", model_path );
		FS_Close( f );
		return NULL;		
	}

studio_done:
	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'Studio'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}
	return pic;
}

static rgbdata_t *R_ParseSpriteFrame( script_t *script, const byte *buf, size_t size, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t *pic;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'Sprite'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'Sprite'\n" );
		return NULL;
	}

	pic = R_LoadImage( script, va( "#%s.spr", token.string ), buf, size, samples, flags );
	if( !pic ) return NULL;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'Sprite'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}
	return pic;
}

static rgbdata_t *R_ParseAliasSkin( script_t *script, const byte *buf, size_t size, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t *pic;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'Alias'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'Alias'\n" );
		return NULL;
	}

	pic = R_LoadImage( script, va( "#%s.mdl", token.string ), buf, size, samples, flags );
	if( !pic ) return NULL;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'Alias'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}
	return pic;
}

/*
=================
R_ParseScrapBlock
=================
*/
static rgbdata_t *R_ParseScrapBlock( script_t *script, int *samples, texFlags_t *flags )
{
	int	i, block[4];
	token_t	token;
	rgbdata_t *pic;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'scrapBlock'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'scrapBlock'\n" );
		return NULL;
	}

	pic = R_LoadImage( script, token.string, NULL, 0, samples, flags );
	if( !pic ) return NULL;

	for( i = 0; i < 4; i++ )
	{
		Com_ReadToken( script, 0, &token );
		if( com.stricmp( token.string, "," ))
		{
			MsgDev( D_WARN, "expected ',', found '%s' instead for 'rect'\n", token.string );
			FS_FreeImage( pic );
			return NULL;
		}

		if( !Com_ReadLong( script, 0, &block[i] ))
		{
			MsgDev( D_WARN, "missing parameters for 'block'\n" );
			FS_FreeImage( pic );
			return NULL;
		}
		if( block[i] < 0 )
		{
			MsgDev( D_WARN, "invalid argument %i for 'block'\n", i+1 );
			FS_FreeImage( pic );
			return NULL;
		}
		if((i+1) & 1 && block[i] > pic->width ) 
		{
			MsgDev( D_WARN, "invalid argument %i for 'block'\n", i+1 );
			FS_FreeImage( pic );
			return NULL;
		}
		if((i+1) & 2 && block[i] > pic->height ) 
		{
			MsgDev( D_WARN, "invalid argument %i for 'block'\n", i+1 );
			FS_FreeImage( pic );
			return NULL;
		}
	}

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'bias'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}
	if((block[0] + block[2] > pic->width) || (block[1] + block[3] > pic->height))
	{
		MsgDev( D_WARN, "'ScrapBlock' image size out of bounds\n" );
		FS_FreeImage( pic );
		return NULL;
	}

	return R_MakeImageBlock( pic, block );
}

/*
=================
R_ParseHeightMap
=================
*/
static rgbdata_t *R_ParseHeightMap( script_t *script, const byte *buf, size_t size, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t *pic;
	float	scale;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'heightMap'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'heightMap'\n" );
		return NULL;
	}

	pic = R_LoadImage( script, token.string, buf, size, samples, flags );
	if( !pic ) return NULL;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "," ))
	{
		MsgDev( D_WARN, "expected ',', found '%s' instead for 'heightMap'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}

	if( !Com_ReadFloat( script, 0, &scale ))
	{
		MsgDev( D_WARN, "missing parameters for 'heightMap'\n" );
		FS_FreeImage( pic );
		return NULL;
	}

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'heightMap'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}

	*samples = 3;
	*flags &= ~TF_INTENSITY;
	*flags &= ~TF_ALPHA;
	*flags |= TF_NORMALMAP;

	return R_HeightMap( pic, scale );
}

/*
=================
R_ParseAddNormals
=================
*/
static rgbdata_t *R_ParseAddNormals( script_t *script, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t *pic1, *pic2;
	int	samples1, samples2;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'addNormals'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'addNormals'\n" );
		return NULL;
	}

	pic1 = R_LoadImage( script, token.string, NULL, 0, &samples1, flags );
	if( !pic1 ) return NULL;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "," ))
	{
		MsgDev( D_WARN, "expected ',', found '%s' instead for 'addNormals'\n", token.string );
		FS_FreeImage( pic1 );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'addNormals'\n" );
		FS_FreeImage( pic1 );
		return NULL;
	}

	pic2 = R_LoadImage( script, token.string, NULL, 0, &samples2, flags );
	if( !pic2 )
	{
		FS_FreeImage( pic1 );
		return NULL;
	}

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'addNormals'\n", token.string );
		FS_FreeImage( pic1 );
		FS_FreeImage( pic2 );
		return NULL;
	}

	if( pic1->width != pic2->width || pic1->height != pic2->height )
	{
		MsgDev( D_WARN, "images for 'addNormals' have mismatched dimensions [%ix%i] != [%ix%i]\n",
			pic1->width, pic1->height, pic2->width, pic2->height );
		FS_FreeImage( pic1 );
		FS_FreeImage( pic2 );
		return NULL;
	}

	*samples = 3;
	*flags &= ~TF_INTENSITY;
	*flags &= ~TF_ALPHA;
	*flags |= TF_NORMALMAP;

	return R_AddNormals( pic1, pic2 );
}

/*
=================
R_ParseSmoothNormals
=================
*/
static rgbdata_t *R_ParseSmoothNormals( script_t *script, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t *pic;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'smoothNormals'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'smoothNormals'\n" );
		return NULL;
	}

	pic = R_LoadImage( script, token.string, NULL, 0, samples, flags );
	if( !pic ) return NULL;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'smoothNormals'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}

	*samples = 3;
	*flags &= ~TF_INTENSITY;
	*flags &= ~TF_ALPHA;
	*flags |= TF_NORMALMAP;

	return R_SmoothNormals( pic );
}

/*
=================
R_ParseDepthmap
=================
*/
static rgbdata_t *R_ParseDepthmap( script_t *script, const byte *buf, size_t size, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t	*pic1, *pic2;
	int	samples1, samples2;

	Com_ReadToken( script, 0, &token );

	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'mergeDepthmap'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'mergeDepthmap'\n" );
		return NULL;
	}

	pic1 = R_LoadImage( script, token.string, buf, size, &samples1, flags );
	if( !pic1 ) return NULL;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "," ))
	{
		MsgDev( D_WARN, "expected ',', found '%s' instead for 'mergeDepthmap'\n", token.string );
		FS_FreeImage( pic1 );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'mergeDepthmap'\n" );
		FS_FreeImage( pic1 );
		return NULL;
	}

	*samples = 3;
	pic2 = R_LoadImage( script, token.string, buf, size, &samples2, flags );
	if( !pic2 ) return pic1; // don't free normalmap

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'mergeDepthmap'\n", token.string );

		FS_FreeImage( pic1 );
		FS_FreeImage( pic2 );
		return NULL;
	}

	if( pic1->width != pic2->width || pic1->height != pic2->height )
	{
		MsgDev( D_WARN, "images for 'mergeDepthmap' have mismatched dimensions [%ix%i] != [%ix%i]\n",
			pic1->width, pic1->height, pic2->width, pic2->height );

		FS_FreeImage( pic2 );
		return pic1; // don't free normalmap
	}

	*samples = 4;
	*flags &= ~TF_INTENSITY;

	return R_IncludeDepthmap( pic1, pic2 );
}

/*
=================
R_ParseClearPixels
=================
*/
static rgbdata_t *R_ParseClearPixels( script_t *script, int *samples, texFlags_t *flags )
{
	token_t	token;
	rgbdata_t *pic;
	bool	clearAlpha;

	Com_ReadToken( script, 0, &token );
	if( com.stricmp( token.string, "(" ))
	{
		MsgDev( D_WARN, "expected '(', found '%s' instead for 'clearPixels'\n", token.string );
		return NULL;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
	{
		MsgDev( D_WARN, "missing parameters for 'clearPixels'\n" );
		return NULL;
	}

	pic = R_LoadImage( script, token.string, NULL, 0, samples, flags );
	if( !pic ) return NULL;

	Com_ReadToken( script, 0, &token );
	if( !com.stricmp( token.string, "alpha" ))
	{
		Com_ReadToken( script, 0, &token );
		clearAlpha = true;
	}
	else if( !com.stricmp( token.string, "color" ))
	{
		Com_ReadToken( script, 0, &token );
		clearAlpha = false;
	}
	else if( !com.stricmp( token.string, ")" ))
	{
		Com_SaveToken( script, &token );
		clearAlpha = false;	// clear color as default
	}
	else Com_ReadToken( script, 0, &token ); // skip unknown token
	
	if( com.stricmp( token.string, ")" ))
	{
		MsgDev( D_WARN, "expected ')', found '%s' instead for 'clearPixels'\n", token.string );
		FS_FreeImage( pic );
		return NULL;
	}

	*samples = clearAlpha ? 3 : 1;
	if( clearAlpha ) *flags &= ~TF_ALPHA;
	*flags &= ~TF_INTENSITY;

	return R_ClearPixels( pic, clearAlpha );
}

/*
=================
R_LoadImage
=================
*/
static rgbdata_t *R_LoadImage( script_t *script, const char *name, const byte *buf, size_t size, int *samples, texFlags_t *flags )
{
	if( !com.stricmp( name, "add" ))
		return R_ParseAdd( script, samples, flags );
	else if( !com.stricmp( name, "multiply" ))
		return R_ParseMultiply( script, samples, flags );
	else if( !com.stricmp( name, "bias" ))
		return R_ParseBias( script, samples, flags );
	else if( !com.stricmp( name, "scale"))
		return R_ParseScale( script, samples, flags );
	else if( !com.stricmp( name, "invertColor" ))
		return R_ParseInvertColor( script, samples, flags );
	else if( !com.stricmp( name, "invertAlpha" ))
		return R_ParseInvertAlpha( script, samples, flags );
	else if( !com.stricmp( name, "makeIntensity" ))
		return R_ParseMakeIntensity( script, samples, flags );
	else if( !com.stricmp( name, "makeLuminance" ))
		return R_ParseMakeLuminance( script, samples, flags);
	else if( !com.stricmp( name, "makeAlpha" ))
		return R_ParseMakeAlpha( script, samples, flags );
	else if( !com.stricmp( name, "makeGlow" ))
		return R_ParseMakeGlow( script, samples, flags );
	else if( !com.stricmp( name, "heightMap" ))
		return R_ParseHeightMap( script, buf, size, samples, flags );
	else if( !com.stricmp( name, "ScrapBlock" ))
		return R_ParseScrapBlock( script, samples, flags );
	else if( !com.stricmp( name, "addNormals" ))
		return R_ParseAddNormals( script, samples, flags );
	else if( !com.stricmp( name, "smoothNormals" ))
		return R_ParseSmoothNormals( script, samples, flags );
	else if( !com.stricmp( name, "mergeDepthmap" ))
		return R_ParseDepthmap( script, buf, size, samples, flags );
	else if( !com.stricmp( name, "clearPixels" ))
		return R_ParseClearPixels( script, samples, flags );
	else if( !com.stricmp( name, "Studio" ))
		return R_ParseStudioSkin( script, buf, size, samples, flags );
	else if( !com.stricmp( name, "Alias" ))
		return R_ParseAliasSkin( script, buf, size, samples, flags );
	else if( !com.stricmp( name, "Sprite" ))
		return R_ParseSpriteFrame( script, buf, size, samples, flags );
	else
	{	
		// loading form disk
		rgbdata_t	*image = FS_LoadImage( name, buf, size );
		if( image ) *samples = R_GetSamples( image->flags );
		return image;
	}
	return NULL;
}

/*
===============
GL_GenerateMipmaps

sgis generate mipmap
===============
*/
void GL_GenerateMipmaps( byte *buffer, texture_t *tex, int side )
{
	int	mipLevel;
	int	mipWidth, mipHeight;

	// not needs
	if( tex->flags & TF_NOMIPMAP ) return;

	if( GL_Support( R_SGIS_MIPMAPS_EXT ))
	{
		pglHint( GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST );
		pglTexParameteri( image_desc.glTarget, GL_GENERATE_MIPMAP_SGIS, GL_TRUE );
		if( pglGetError()) MsgDev( D_WARN, "GL_GenerateMipmaps: %s can't create mip levels\n", tex->name );
		else return; // falltrough to software mipmap generating
	}

	if( image_desc.format != PF_RGBA_32 && image_desc.format != PF_RGBA_GN )
	{
		// g-cont. because i'm don't know how to generate miplevels for GL_FLOAT or GL_SHORT_REV_1_bla_bla
		// ok, please show me videocard which don't supported GL_GENERATE_MIPMAP_SGIS ...
		MsgDev( D_ERROR, "GL_GenerateMipmaps: failed on %s\n", PFDesc( image_desc.format )->name );
		return;
	}
	
	mipLevel = 0;
	mipWidth = tex->width;
	mipHeight = tex->height;

	// software mipmap generator
	while( mipWidth > 1 || mipHeight > 1 )
	{
		// build the mipmap
		R_BuildMipMap( buffer, mipWidth, mipHeight, (tex->flags & TF_NORMALMAP));

		mipWidth = (mipWidth+1)>>1;
		mipHeight = (mipHeight+1)>>1;
		mipLevel++;

		pglTexImage2D( image_desc.texTarget + side, mipLevel, tex->format, mipWidth, mipHeight, 0, image_desc.glFormat, image_desc.glType, buffer );
	}
}

void GL_TexFilter( texture_t *tex )
{
	// set texture filter
	if( tex->flags & TF_DEPTHMAP )
	{
		pglTexParameteri( tex->target, GL_TEXTURE_MIN_FILTER, r_textureDepthFilter );
		pglTexParameteri( tex->target, GL_TEXTURE_MAG_FILTER, r_textureDepthFilter );

		if( GL_Support( R_ANISOTROPY_EXT ))
			pglTexParameterf( tex->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f );
	}
	else if( tex->flags & TF_NOMIPMAP )
	{
		pglTexParameteri( tex->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		pglTexParameteri( tex->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}
	else
	{
		pglTexParameteri( tex->target, GL_TEXTURE_MIN_FILTER, r_textureMinFilter );
		pglTexParameteri( tex->target, GL_TEXTURE_MAG_FILTER, r_textureMagFilter );

		// set texture anisotropy if available
		if( GL_Support( R_ANISOTROPY_EXT ))
			pglTexParameterf( tex->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_anisotropy->value );

		// set texture LOD bias if available
		if( GL_Support( R_TEXTURE_LODBIAS ))
			pglTexParameterf( tex->target, GL_TEXTURE_LOD_BIAS_EXT, gl_texture_lodbias->value );
	}

	// set texture wrap
	if( tex->flags & TF_CLAMP )
	{
		if(GL_Support( R_CLAMPTOEDGE_EXT ))
		{
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			if( tex->target == GL_TEXTURE_3D )
				pglTexParameteri( tex->target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		}
		else
		{
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_S, GL_CLAMP );
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_T, GL_CLAMP );
			if( tex->target == GL_TEXTURE_3D )
				pglTexParameteri( tex->target, GL_TEXTURE_WRAP_R, GL_CLAMP );
		}
	}
	else
	{
		pglTexParameteri( tex->target, GL_TEXTURE_WRAP_S, GL_REPEAT );
		pglTexParameteri( tex->target, GL_TEXTURE_WRAP_T, GL_REPEAT );
		if( tex->target == GL_TEXTURE_3D )
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_R, GL_REPEAT );
	}
}

static void R_UploadTexture( rgbdata_t *pic, texture_t *tex )
{
	uint		mipsize = 0, offset = 0;
	bool		dxtformat = true;
	bool		compress;
	int		i, j, w, h, d;
	byte		*buf, *data;
	const byte	*bufend;

	tex->width = tex->srcWidth;
	tex->height = tex->srcHeight;
	R_RoundImageDimensions( &tex->width, &tex->height, &tex->depth, false );

	// check if it should be compressed
	if( !gl_compress_textures->integer || (tex->flags & TF_UNCOMPRESSED))
		compress = false;
	else compress = GL_Support( R_TEXTURE_COMPRESSION_EXT );

	R_TextureFormat( tex, compress );

	// also check for compressed textures
	switch( pic->type )
	{
	case PF_DXT1: tex->format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
	case PF_DXT3: tex->format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
	case PF_DXT5: tex->format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
	default: dxtformat = false; break;
	}
	pglGenTextures( 1, &tex->texnum );
	GL_Bind( GL_TEXTURE0, tex );

	buf = pic->buffer;
	bufend = pic->buffer + pic->size;

	// uploading texture into video memory
	for( offset = i = 0; i < image_desc.numSides; i++ )
	{
		R_SetPixelFormat( tex->width, tex->height, tex->depth );
		w = image_desc.width, h = image_desc.height, d = image_desc.depth;
		offset = image_desc.SizeOfFile; // member side offset

		if( buf >= bufend ) Host_Error( "R_UploadTexture: %s image buffer overflow\n", tex->name );

		for( mipsize = j = 0; j < image_desc.MipCount; j++ )
		{
			R_SetPixelFormat( w, h, d );
			mipsize = image_desc.SizeOfFile; // member mipsize offset
			tex->size += mipsize;

			// copy or resample the texture
			if( tex->width == tex->srcWidth && tex->height == tex->srcHeight )
			{
				data = buf;
			}
			else
			{
				R_ResampleTexture( buf, tex->srcWidth, tex->srcHeight, tex->width, tex->height, (tex->flags & TF_NORMALMAP));
				data = image_desc.scaled;
			}
			if( j == 0 && GL_Support( R_SGIS_MIPMAPS_EXT )) GL_GenerateMipmaps( data, tex, i );
			if( dxtformat ) pglCompressedTexImage2DARB( image_desc.texTarget + i, j, tex->format, w, h, 0, mipsize, data );
			else
			{
				if( image_desc.depth == 1 )
					pglTexImage2D( image_desc.texTarget + i, j, tex->format, tex->width, tex->height, 0, image_desc.glFormat, image_desc.glType, data );
				else pglTexImage3D( image_desc.texTarget, j, tex->format, tex->width, tex->height, tex->depth, 0, image_desc.glFormat, image_desc.glType, data );
				if( j == 0 && !GL_Support( R_SGIS_MIPMAPS_EXT )) GL_GenerateMipmaps( data, tex, i );
			}                              
			w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1; // calc size of next mip
			if( image_desc.MipCount > 1 ) buf += mipsize;

			R_CheckForErrors();
		}
		if( image_desc.numSides > 1 ) buf += offset;
	}
}

/*
=================
R_LoadTexture
=================
*/
texture_t *R_LoadTexture( const char *name, rgbdata_t *pic, int samples, texFlags_t flags )
{
	texture_t	*texture;
	uint	i, hash;

	if( r_numTextures == MAX_TEXTURES )
		Host_Error( "R_LoadTexture: MAX_TEXTURES limit exceeds\n" );

	// find a free texture_t slot
	for( i = 0, texture = r_textures; i < r_numTextures; i++, texture++ )
		if( !texture->texnum ) break;

	if( i == r_numTextures )
	{
		if( r_numTextures == MAX_TEXTURES )
		{
			MsgDev( D_ERROR, "R_LoadTexture: gl textures is out\n" );
			return tr.defaultTexture;
		}
		r_numTextures++;
	}

	texture = &r_textures[i];

	// fill it in
	com.strncpy( texture->name, name, sizeof( texture->name ));
	texture->srcWidth = pic->width;
	texture->srcHeight = pic->height;
	texture->depth = pic->depth;
	texture->touchFrame = tr.registration_sequence;
	if( samples <= 0 )
		texture->samples = R_GetSamples( pic->flags );
	else texture->samples = samples;

	// setup image_desc
	R_GetPixelFormat( name, pic, flags );
	texture->flags = image_desc.tflags;
	texture->target = image_desc.glTarget;
	texture->texType = image_desc.texType;
	texture->type = image_desc.format;

	R_UploadTexture( pic, texture );
	GL_TexFilter( texture ); // update texture filter, wrap etc
	MsgDev( D_LOAD, "%s [%s] \n", name, PFDesc( image_desc.format )->name );

	// add to hash table
	hash = Com_HashKey( texture->name, TEXTURES_HASH_SIZE );
	texture->nextHash = r_texturesHashTable[hash];
	r_texturesHashTable[hash] = texture;

	return texture;
}

/*
=================
R_FindTexture
=================
*/
texture_t *R_FindTexture( const char *name, const byte *buf, size_t size, texFlags_t flags )
{
	texture_t		*texture;
	script_t		*script;
	rgbdata_t		*image;
	token_t		token;
	int		samples;
	uint		hash;

	if( !name || !name[0] ) return NULL;
	if( com.strlen( name ) >= MAX_STRING )
		Host_Error( "R_FindTexture: texture name exceeds %i symbols\n", MAX_STRING );

	// see if already loaded
	hash = Com_HashKey( name, TEXTURES_HASH_SIZE );

	for( texture = r_texturesHashTable[hash]; texture; texture = texture->nextHash )
	{
		if( texture->flags & TF_CUBEMAP )
			continue;

		if( !com.stricmp( texture->name, name ))
		{
			if( texture->flags & TF_STATIC )
				return texture;

			if( texture->flags != flags )
				MsgDev( D_WARN, "reused texture '%s' with mixed flags parameter (%p should be %p)\n", name, texture->flags, flags );

			// prolonge registration
			texture->touchFrame = tr.registration_sequence;
			return texture;
		}
	}

	// NOTE: texname may contains some commands over textures
	script = Com_OpenScript( name, name, com.strlen( name ));
	if( !script ) return NULL;

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &token ))
	{
		Com_CloseScript( script );
		return NULL;
	}

	image = R_LoadImage( script, token.string, buf, size, &samples, &flags );
	Com_CloseScript( script );

	// load the texture
	if( image )
	{
		texture = R_LoadTexture( name, image, samples, flags );
		FS_FreeImage( image );
		return texture;
	}

	// not found or invalid
	return NULL;
}

/*
================
R_FreeImage
================
*/
void R_FreeImage( texture_t *image )
{
	uint		hash;
	texture_t		*cur;
	texture_t		**prev;
	
	Com_Assert( image == NULL );

	// remove from hash table
	hash = Com_HashKey( image->name, TEXTURES_HASH_SIZE );
	prev = &r_texturesHashTable[hash];

	while( 1 )
	{
		cur = *prev;
		if( !cur ) break;

		if( cur == image )
		{
			*prev = cur->nextHash;
			break;
		}
		prev = &cur->nextHash;
	}

	pglDeleteTextures( 1, &image->texnum );
	Mem_Set( image, 0, sizeof( *image ));
}


/*
==============================================================================

SCREEN SHOTS

==============================================================================
*/
/*
================
VID_ImageAdjustGamma
================
*/
void VID_ImageAdjustGamma( byte *in, uint width, uint height )
{
	int	i, c = width * height;
	float	g = 1.0f / bound( 0.5f, r_gamma->value, 3.0f );
	byte	*p = in;

	// screenshots gamma	
	for( i = 0; i < 256; i++ )
	{
		if ( g == 1 ) r_gammaTable[i] = i;
		else r_gammaTable[i] = bound( 0, 255 * pow((i + 0.5)/255.5f, g ) + 0.5, 255);
	}
	for( i = 0; i < c; i++, p += 3 )
	{
		p[0] = r_gammaTable[p[0]];
		p[1] = r_gammaTable[p[1]];
		p[2] = r_gammaTable[p[2]];
	}
}

bool VID_ScreenShot( const char *filename, int shot_type )
{
	rgbdata_t *r_shot;
	uint	flags = IMAGE_FLIP_Y;
	int	width = 0, height = 0;
	bool	result;

	r_shot = Mem_Alloc( r_temppool, sizeof( rgbdata_t ));
	r_shot->width = glState.width;
	r_shot->height = glState.height;
	r_shot->flags = IMAGE_HAS_COLOR;
	r_shot->type = PF_RGB_24;
	r_shot->size = r_shot->width * r_shot->height * PFDesc( r_shot->type )->bpp;
	r_shot->palette = NULL;
	r_shot->depth = 1;
	r_shot->numMips = 1;
	r_shot->buffer = Mem_Alloc( r_temppool, glState.width * glState.height * 3 );

	// get screen frame
	pglReadPixels( 0, 0, glState.width, glState.height, GL_RGB, GL_UNSIGNED_BYTE, r_shot->buffer );

	switch( shot_type )
	{
	case VID_SCREENSHOT:
		VID_ImageAdjustGamma( r_shot->buffer, r_shot->width, r_shot->height ); // adjust brightness
		break;
	case VID_LEVELSHOT:
		flags |= IMAGE_RESAMPLE;
		height = 384;
		width = 512;
		break;
	case VID_SAVESHOT:
		flags |= IMAGE_RESAMPLE;
		height = 192;
		width = 256;
		break;
	}

	Image_Process( &r_shot, width, height, flags );

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
	rgbdata_t		*r_shot, *r_side;
	byte		*temp = NULL;
	byte		*buffer = NULL;
	string		basename;
	int		i = 1, flags, result;

	if( RI.refdef.onlyClientDraw || !r_worldmodel )
		return false;

	// make sure the specified size is valid
	while( i < size ) i<<=1;

	if( i != size ) return false;
	if( size > glState.width || size > glState.height )
		return false;

	// setup refdef
	RI.params |= RP_ENVVIEW;	// do not render non-bmodel entities

	// alloc space
	temp = Mem_Alloc( r_temppool, size * size * 3 );
	buffer = Mem_Alloc( r_temppool, size * size * 3 * 6 );
	r_shot = Mem_Alloc( r_temppool, sizeof( rgbdata_t ));
	r_side = Mem_Alloc( r_temppool, sizeof( rgbdata_t ));

	for( i = 0; i < 6; i++ )
	{
		if( skyshot )
		{
			R_DrawCubemapView( r_lastRefdef.vieworg, r_skyBoxInfo[i].angles, size );
			flags = r_skyBoxInfo[i].flags;
		}
		else
		{
			R_DrawCubemapView( r_lastRefdef.vieworg, r_envMapInfo[i].angles, size );
			flags = r_envMapInfo[i].flags;
                    }
		pglReadPixels( 0, glState.height - size, size, size, GL_RGB, GL_UNSIGNED_BYTE, temp );
		r_side->flags = IMAGE_HAS_COLOR;
		r_side->width = r_side->height = size;
		r_side->type = PF_RGB_24;
		r_side->size = r_side->width * r_side->height * 3;
		r_side->depth = r_side->numMips = 1;
		r_side->buffer = temp;

		if( flags ) Image_Process( &r_side, 0, 0, flags );
		Mem_Copy( buffer + (size * size * 3 * i), r_side->buffer, size * size * 3 );
	}

	RI.params &= ~RP_ENVVIEW;

	r_shot->flags |= IMAGE_CUBEMAP|IMAGE_HAS_COLOR;
	r_shot->width = size;
	r_shot->height = size;
	r_shot->type = PF_RGB_24;
	r_shot->size = r_shot->width * r_shot->height * 3 * 6;
	r_shot->palette = NULL;
	r_shot->depth = 1;
	r_shot->numMips = 1;
	r_shot->buffer = buffer;

	// make sure what we have right extension
	com.strncpy( basename, base, MAX_STRING );
	FS_StripExtension( basename );
	FS_DefaultExtension( basename, ".dds" );

	// write image as dds packet
	result = FS_SaveImage( basename, r_shot );
	FS_FreeImage( r_shot );
	FS_FreeImage( r_side );

	return result;
}
//=======================================================

/*
==================
R_InitNoTexture
==================
*/
static rgbdata_t *R_InitNoTexture( int *flags, int *samples )
{
	int	x, y;

	// also use this for bad textures, but without alpha
	r_image.width = r_image.height = 16;
	r_image.numMips = r_image.depth = 1;
	r_image.buffer = data2D;
	r_image.flags = IMAGE_HAS_COLOR;
	r_image.type = PF_RGBA_GN;
	r_image.size = r_image.width * r_image.height * 4;

	*flags = 0;
	*samples = 3;

	// default texture
	for( y = 0; y < 16; y++ )
	{
		for( x = 0; x < 16; x++ )
		{
			if( x == 0 || x == 15 || y == 0 || y == 15 )
				((uint *)&data2D)[y*16+x] = LittleLong( 0xFFFFFFFF );
			else ((uint *)&data2D)[y*16+x] = LittleLong( 0xFF000000 );
		}
	}
	return &r_image;
}

/*
==================
R_InitDynamicLightTexture
==================
*/
static rgbdata_t *R_InitDynamicLightTexture( int *flags, int *samples )
{
	vec3_t	v = { 0, 0, 0 };
	int	x, y, z, size, size2, halfsize;
	float	intensity;

	// dynamic light texture
	if( GL_Support( R_TEXTURE_3D_EXT ))
	{
		r_image.depth = size = 32;
	}
	else
	{
		size = 128;
		r_image.depth = 1;
	}

	r_image.width = r_image.height = size;
	r_image.numMips =  1;
	r_image.buffer = data2D;
	r_image.flags = IMAGE_HAS_COLOR;
	r_image.type = PF_RGBA_GN;
	r_image.size = r_image.width * r_image.height * r_image.depth * 4;

	halfsize = size / 2;
	intensity = halfsize * halfsize;
	size2 = size * size;

	*flags = TF_NOPICMIP|TF_NOMIPMAP|TF_CLAMP|TF_UNCOMPRESSED;
	*samples = 3;

	if( GL_Support( R_TEXTURE_3D_EXT ))
	{		
		for( x = 0; x < r_image.width; x++ )
		{
			for( y = 0; y < r_image.height; y++ )
			{
				for( z = 0; z < r_image.depth; z++ )
				{
					float	dist, att;

					v[0] = (float)x - halfsize;
					v[1] = (float)y - halfsize;
					v[2] = (float)z - halfsize;

					dist = VectorLength( v );
					if( dist > halfsize ) dist = halfsize;

					if( x == 0 || y == 0 || z == 0 || x == size - 1 || y == size - 1 || z == size - 1 )
						att = 0;
					else att = (((dist * dist) / intensity) -1 ) * -255;

					data2D[(x * size2 + y * size + z) * 4 + 0] = (byte)(att);
					data2D[(x * size2 + y * size + z) * 4 + 1] = (byte)(att);
					data2D[(x * size2 + y * size + z) * 4 + 2] = (byte)(att);
				}
			}
		}
	}
	else
	{
		for( x = 0; x < size; x++ )
		{
			for( y = 0; y < size; y++ )
			{
				float	result;

				if( x == size - 1 || x == 0 || y == size - 1 || y == 0 )
					result = 255;
				else
				{
					float	xf = ((float)x - 64 ) / 64.0f;
					float	yf = ((float)y - 64 ) / 64.0f;
					result = ((xf * xf) + (yf * yf)) * 255;
					if( result > 255 ) result = 255;
				}
				data2D[(x*size+y)*4+0] = (byte)255 - result;
				data2D[(x*size+y)*4+1] = (byte)255 - result;
				data2D[(x*size+y)*4+2] = (byte)255 - result;
			}
		}
	}
	return &r_image;
}

/*
==================
R_InitSpotLightTexture
==================
*/
static rgbdata_t *R_InitSpotLightTexture( int *flags, int *samples )
{
	*flags = TF_NOPICMIP|TF_NOMIPMAP|TF_CLAMP|TF_UNCOMPRESSED;
	*samples = 0;

	return FS_LoadImage( "textures/effects/flashlight.tga", NULL, 0 );	// UNDONE: test only 
}

/*
==================
R_InitNormalizeCubemap
==================
*/
static rgbdata_t *R_InitNormalizeCubemap( int *flags, int *samples )
{
	int	i, x, y, size = 32;
	byte	*dataCM = data2D;
	float	s, t;
	vec3_t	normal;

	if( !GL_Support( R_TEXTURECUBEMAP_EXT ))
		return NULL;

	// normal cube map texture
	for( i = 0; i < 6; i++ )
	{
		for( y = 0; y < size; y++ )
		{
			for( x = 0; x < size; x++ )
			{
				s = (((float)x + 0.5f) * (2.0f / size )) - 1.0f;
				t = (((float)y + 0.5f) * (2.0f / size )) - 1.0f;

				switch( i )
				{
				case 0: VectorSet( normal, 1.0f, -t, -s ); break;
				case 1: VectorSet( normal, -1.0f, -t, s ); break;
				case 2: VectorSet( normal, s,  1.0f,  t ); break;
				case 3: VectorSet( normal, s, -1.0f, -t ); break;
				case 4: VectorSet( normal, s, -t, 1.0f  ); break;
				case 5: VectorSet( normal, -s, -t, -1.0f); break;
				}

				VectorNormalize( normal );

				dataCM[4*(y*size+x)+0] = (byte)(128 + 127 * normal[0]);
				dataCM[4*(y*size+x)+1] = (byte)(128 + 127 * normal[1]);
				dataCM[4*(y*size+x)+2] = (byte)(128 + 127 * normal[2]);
				dataCM[4*(y*size+x)+3] = 255;
			}
		}
		dataCM += (size*size*4); // move pointer
	}

	*flags = (TF_STATIC|TF_NOPICMIP|TF_NOMIPMAP|TF_UNCOMPRESSED|TF_CUBEMAP|TF_CLAMP);
	*samples = 3;

	r_image.width = r_image.height = size;
	r_image.numMips = r_image.depth = 1;
	r_image.size = r_image.width * r_image.height * 4 * 6;
	r_image.flags |= (IMAGE_CUBEMAP|IMAGE_HAS_COLOR); // yes it's cubemap
	r_image.buffer = data2D;
	r_image.type = PF_RGBA_GN;

	return &r_image;
}

/*
==================
R_InitSolidColorTexture
==================
*/
static rgbdata_t *R_InitSolidColorTexture( int *flags, int *samples, int color )
{
	// solid color texture
	r_image.width = r_image.height = 1;
	r_image.numMips = r_image.depth = 1;
	r_image.buffer = data2D;
	r_image.flags = IMAGE_HAS_COLOR;
	r_image.type = PF_RGB_24;
	r_image.size = r_image.width * r_image.height * 3;

	*flags = TF_NOPICMIP|TF_UNCOMPRESSED;
	*samples = 3;

	data2D[0] = data2D[1] = data2D[2] = color;
	return &r_image;
}

/*
==================
R_InitParticleTexture
==================
*/
static rgbdata_t *R_InitParticleTexture( int *flags, int *samples )
{
	int	x, y;
	int	dx2, dy, d;

	// particle texture
	r_image.width = r_image.height = 16;
	r_image.numMips = r_image.depth = 1;
	r_image.buffer = data2D;
	r_image.flags = (IMAGE_HAS_COLOR|IMAGE_HAS_ALPHA);
	r_image.type = PF_RGBA_GN;
	r_image.size = r_image.width * r_image.height * 4;

	*flags = TF_NOPICMIP|TF_NOMIPMAP;
	*samples = 4;

	for( x = 0; x < 16; x++ )
	{
		dx2 = x - 8;
		dx2 = dx2 * dx2;

		for( y = 0; y < 16; y++ )
		{
			dy = y - 8;
			d = 255 - 35 * com.sqrt( dx2 + dy * dy );
			data2D[( y*16 + x ) * 4 + 3] = bound( 0, d, 255 );
		}
	}
	return &r_image;
}

/*
==================
R_InitWhiteTexture
==================
*/
static rgbdata_t *R_InitWhiteTexture( int *flags, int *samples )
{
	return R_InitSolidColorTexture( flags, samples, 255 );
}

/*
==================
R_InitBlackTexture
==================
*/
static rgbdata_t *R_InitBlackTexture( int *flags, int *samples )
{
	return R_InitSolidColorTexture( flags, samples, 0 );
}

/*
==================
R_InitBlankBumpTexture
==================
*/
static rgbdata_t *R_InitBlankBumpTexture( int *flags, int *samples )
{
	rgbdata_t	*pic = R_InitSolidColorTexture( flags, samples, 128 );

/*
	pic->buffer[2] = 128;	// normal X
	pic->buffer[1] = 128;	// normal Y
*/
	pic->buffer[0] = 255;	// normal Z
	pic->buffer[3] = 128;	// height

	return pic;
}

/*
==================
R_InitSkyTexture
==================
*/
static rgbdata_t *R_InitSkyTexture( int *flags, int *samples )
{
	int	i;

	// skybox texture
	for( i = 0; i < 256; i++ )
		((uint *)&data2D)[i] = LittleLong( 0xFFFFDEB5 );

	*flags = TF_NOPICMIP|TF_UNCOMPRESSED;
	*samples = 3;

	r_image.numMips = r_image.depth = 1;
	r_image.buffer = data2D;
	r_image.width = r_image.height = 16;
	r_image.size = r_image.width * r_image.height * 4;
	r_image.flags = IMAGE_HAS_COLOR;
	r_image.type = PF_RGBA_GN;

	return &r_image;
}

/*
==================
R_InitFogTexture
==================
*/
static rgbdata_t *R_InitFogTexture( int *flags, int *samples )
{
	int	x, y;
	double	tw = 1.0f / ( (float)FOG_TEXTURE_WIDTH - 1.0f );
	double	th = 1.0f / ( (float)FOG_TEXTURE_HEIGHT - 1.0f );
	double	tx, ty, t;

	// fog texture
	r_image.width = FOG_TEXTURE_WIDTH;
	r_image.height = FOG_TEXTURE_HEIGHT;
	r_image.numMips = r_image.depth = 1;
	r_image.buffer = data2D;
	r_image.flags = IMAGE_HAS_COLOR;
	r_image.type = PF_RGBA_GN;
	r_image.size = r_image.width * r_image.height * 4;

	*flags = TF_NOMIPMAP|TF_CLAMP;
	*samples = 4;

	for( y = 0, ty = 0.0f; y < FOG_TEXTURE_HEIGHT; y++, ty += th )
	{
		for( x = 0, tx = 0.0f; x < FOG_TEXTURE_WIDTH; x++, tx += tw )
		{
			t = com.sqrt( tx ) * 255.0;
			data2D[( x+y*FOG_TEXTURE_WIDTH )*4+3] = (byte)( min( t, 255.0 ));
		}
		data2D[y*4+3] = 0;
	}
	return &r_image;
}

/*
==================
R_InitCoronaTexture
==================
*/
static rgbdata_t *R_InitCoronaTexture( int *flags, int *samples )
{
	int	x, y, a;
	float	dx, dy;

	// light corona texture
	r_image.width = r_image.height = 32;
	r_image.numMips = r_image.depth = 1;
	r_image.buffer = data2D;
	r_image.flags = IMAGE_HAS_COLOR;
	r_image.type = PF_RGBA_GN;
	r_image.size = r_image.width * r_image.height * 4;

	*flags = TF_NOMIPMAP|TF_NOPICMIP|TF_UNCOMPRESSED|TF_CLAMP;
	*samples = 4;

	for( y = 0; y < 32; y++ )
	{
		dy = ( y - 15.5f ) * ( 1.0f / 16.0f );
		for( x = 0; x < 32; x++ )
		{
			dx = ( x - 15.5f ) * ( 1.0f / 16.0f );
			a = (int)((( 1.0f / ( dx * dx + dy * dy + 0.2f )) - ( 1.0f / ( 1.0f + 0.2 ))) * 32.0f / ( 1.0f / ( 1.0f + 0.2 )));
			a = bound( 0, a, 255 );
			data2D[( y*32+x )*4+0] = data2D[( y*32+x )*4+1] = data2D[( y*32+x )*4+2] = a;
		}
	}
	return &r_image;
}

/*
==================
R_InitScreenTexture
==================
*/
static void R_InitScreenTexture( texture_t **ptr, const char *name, int id, int screenWidth, int screenHeight, int size, int flags, int samples )
{
	int	limit;
	int	width, height;
	rgbdata_t	r_screen;

	Mem_Set( &r_screen, 0, sizeof( rgbdata_t ));
	limit = glConfig.max_2d_texture_size;
	if( size ) limit = min( limit, size );

	limit = min( limit, min( screenWidth, screenHeight ));
	for( size = 2; size <= limit; size <<= 1 );
	width = height = size >> 1;

	if( !(*ptr) || (*ptr)->width != width || (*ptr)->height != height )
	{
		byte	*data = NULL;

		if( !*ptr )
		{
			string	uploadName;

			com.snprintf( uploadName, sizeof( uploadName ), "***%s%i***", name, id );

			r_screen.width = width;
			r_screen.height = height;
                              r_screen.type = (samples == 1) ? PF_LUMINANCE : PF_RGB_24;
			r_screen.buffer = data;
			r_screen.depth = r_screen.numMips = 1;
			r_screen.size = width * height * samples;
			*ptr = R_LoadTexture( uploadName, &r_screen, samples, flags );
			return;
		}

		GL_Bind( 0, *ptr );
		(*ptr)->width = width;
		(*ptr)->height = height;
		R_RoundImageDimensions(&((*ptr)->width), &((*ptr)->height), &((*ptr)->depth), true );
		pglTexImage2D( GL_TEXTURE_2D, 0, (*ptr)->format, (*ptr)->width, (*ptr)->height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL );
		GL_TexFilter( *ptr );
	}
}

/*
==================
R_InitPortalTexture
==================
*/
void R_InitPortalTexture( texture_t **texture, int id, int screenWidth, int screenHeight )
{
	R_InitScreenTexture( texture, "r_portaltexture", id, screenWidth, screenHeight, r_portalmaps_maxtexsize->integer, TF_PORTALMAP, 3 );
}

/*
==================
R_InitShadowmapTexture
==================
*/
void R_InitShadowmapTexture( texture_t **texture, int id, int screenWidth, int screenHeight )
{
	R_InitScreenTexture( texture, "r_shadowmap", id, screenWidth, screenHeight, r_shadows_maxtexsize->integer, TF_SHADOWMAP, 1 );
}

/*
==================
R_InitCinematicTexture
==================
*/
static rgbdata_t *R_InitCinematicTexture( int *flags, int *samples )
{
	// light corona texture
	r_image.width = r_image.height = 256;
	r_image.numMips = r_image.depth = 1;
	r_image.buffer = data2D;
	r_image.flags = IMAGE_HAS_COLOR;
	r_image.type = PF_RGBA_GN;
	r_image.size = r_image.width * r_image.height * 4;

	*flags = TF_STATIC|TF_NOMIPMAP|TF_NOPICMIP|TF_UNCOMPRESSED|TF_CLAMP;
	*samples = 4;

	return &r_image;
}

/*
==================
R_InitBuiltinTextures
==================
*/
static void R_InitBuiltinTextures( void )
{
	rgbdata_t		*pic;
	int		flags, samples;
	size_t		datasize;
	texture_t		*image;
	byte		*data;

	const struct
	{
		char *name;
		texture_t	**image;
		rgbdata_t *( *init )( int *flags, int *samples );
	}
	textures[] =
	{
		{ "*default", &tr.defaultTexture, R_InitNoTexture },
		{ "*fog", &tr.fogTexture, R_InitFogTexture },
		{ "*sky", &tr.skyTexture, R_InitSkyTexture },
		{ "*white", &tr.whiteTexture, R_InitWhiteTexture },
		{ "*black", &tr.blackTexture, R_InitBlackTexture },
		{ "*blankbump", &tr.blankbumpTexture, R_InitBlankBumpTexture },
		{ "*dlight", &tr.dlightTexture, R_InitDynamicLightTexture },
		{ "*dspotlight", &tr.dspotlightTexture, R_InitSpotLightTexture },
          	{ "*normalize", &tr.normalizeTexture, R_InitNormalizeCubemap },
		{ "*particle", &tr.particleTexture, R_InitParticleTexture },
		{ "*corona", &tr.coronaTexture, R_InitCoronaTexture },
		{ "*cintexture", &tr.cinTexture, R_InitCinematicTexture },
		{ NULL, NULL, NULL }
	};
	size_t i, num_builtin_textures = sizeof( textures ) / sizeof( textures[0] ) - 1;

	for( i = 0; i < num_builtin_textures; i++ )
	{
		Mem_Set( &r_image, 0, sizeof( rgbdata_t ));
		Mem_Set( data2D, 0xFF, sizeof( data2D ));

		pic = textures[i].init( &flags, &samples );
		Com_Assert( pic == NULL );
		image = R_LoadTexture( textures[i].name, pic, samples, flags );

		if( textures[i].image )
			*( textures[i].image ) = image;
	}
	tr.portaltexture1 = NULL;
	tr.portaltexture2 = NULL;

	data = FS_LoadInternal( "default.dds", &datasize );
	tr.defaultConchars = R_FindTexture( "#default.dds", data, datasize, TF_NOPICMIP|TF_STATIC|TF_NOMIPMAP );
}

/*
===============
R_ShowTextures

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.
===============
*/
void R_ShowTextures( void )
{
	texture_t		*image;
	float		x, y, w, h;
	int		i, j, base_w, base_h;

	if( !r_showtextures->integer )
		return;

	if( !glState.in2DMode )
	{
		R_Set2DMode( true );
	}

	pglClear( GL_COLOR_BUFFER_BIT );
	pglFinish();

	base_w = 16;
	base_h = 12;

	for( i = j = 0, image = r_textures; i < r_numTextures; i++, image++ )
	{
		if( !image->texnum ) continue;

		if( image->texType != r_showtextures->integer )
			continue;

		w = glState.width / base_w;
		h = glState.height / base_h;
		x = j % base_w * w;
		y = j / base_w * h;

		pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		GL_Bind( GL_TEXTURE0, image );
		R_CheckForErrors();
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
		j++;
	}
	pglFinish();

	R_CheckForErrors ();
}

//=======================================================

/*
===============
R_InitImages
===============
*/
void R_InitImages( void )
{
	int	i;
	float	f;

	r_texpool = Mem_AllocPool( "Texture Manager Pool" );
	r_imagepool = Mem_AllocPool( "Immediate TexturePool" );	// for scaling and resampling

	r_numTextures = 0;
	tr.registration_sequence = 1;
	Mem_Set( r_textures, 0, sizeof( r_textures ));
	Mem_Set( r_texturesHashTable, 0, sizeof( r_texturesHashTable ));

	// build the auto-luma table
	for( i = 0; i < 256; i++ )
	{
		// 224 is a Q1 luma threshold
		r_glowTable[i][0] = i > 224 ? i : 0;
		r_glowTable[i][1] = i > 224 ? i : 0;
		r_glowTable[i][2] = i > 224 ? i : 0;
	}

	// build luminance table
	for( i = 0; i < 256; i++ )
	{
		f = (float)i;
		r_luminanceTable[i][0] = f * 0.299;
		r_luminanceTable[i][1] = f * 0.587;
		r_luminanceTable[i][2] = f * 0.114;
	}

	// set texture parameters
	R_SetTextureParameters();

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
	int		i;
	texture_t		*image;

	if( !glw_state.initialized ) return;

	for( i = MAX_TEXTURE_UNITS - 1; i >= 0; i-- )
	{
		if( i >= glConfig.max_texture_units )
			continue;

		GL_SelectTexture( i );
		pglBindTexture( GL_TEXTURE_2D, 0 );
		pglBindTexture( GL_TEXTURE_CUBE_MAP_ARB, 0 );
	}

	for( i = 0, image = r_textures; i < r_numTextures; i++, image++ )
	{
		if( !image->texnum ) continue;
		R_FreeImage( image );
	}

	Mem_Set( tr.lightmapTextures, 0, sizeof( tr.lightmapTextures ));
	Mem_Set( tr.shadowmapTextures, 0, sizeof( tr.shadowmapTextures ));
	Mem_Set( r_texturesHashTable, 0, sizeof( r_texturesHashTable ));
	Mem_Set( r_textures, 0, sizeof( r_textures ));

	r_numTextures = 0;
	tr.portaltexture1 = NULL;
	tr.portaltexture2 = NULL;
}