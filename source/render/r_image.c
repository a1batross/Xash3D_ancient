//=======================================================================
//			Copyright XashXT Group 2008 ©
//			 r_image.c - texture manager
//=======================================================================

#include "r_local.h"
#include "byteorder.h"
#include "vector_lib.h"

static rgbdata_t *R_LoadImage( script_t *script, const char *name, const byte *buf, size_t size, int *samples, texFlags_t *flags );
static int r_textureMinFilter = GL_LINEAR_MIPMAP_LINEAR;
static int r_textureMagFilter = GL_LINEAR;

// internal tables
static vec3_t r_luminanceTable[256];	// RGB to LUMA
static byte r_intensityTable[256];	// scale intensity
static byte r_gammaTable[256];	// adjust screenshot gamma

static texture_t	*r_texturesHashTable[TEXTURES_HASH_SIZE];
static texture_t	*r_textures[MAX_TEXTURES];
static int	r_numTextures;
static byte	*r_imagepool;	// immediate buffers
static byte	*r_texpool;	// texture_t permanent chain

texture_t	*r_defaultTexture;
texture_t	*r_whiteTexture;
texture_t	*r_blackTexture;
texture_t	*r_flatTexture;
texture_t	*r_cinematicTexture;
texture_t	*r_dlightTexture;
texture_t	*r_normalizeTexture;
texture_t	*r_attenuationTexture;
texture_t	*r_falloffTexture;
texture_t	*r_rawTexture;
texture_t	*r_fogTexture;
texture_t	*r_fogEnterTexture;
texture_t	*r_scratchTexture;
texture_t	*r_accumTexture;
texture_t	*r_mirrorRenderTexture;
texture_t	*r_portalRenderTexture;
texture_t	*r_currentRenderTexture;
texture_t	*r_lightmapTextures[MAX_LIGHTMAPS];

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
	int		numLayers;
	int		numSides;
	int		MipCount;
	int		BitsCount;
	GLuint		glFormat;
	GLuint		glType;
	GLuint		glTarget;
	GLuint		glSamples;
	GLuint		texTarget;

	uint		tflags;	// TF_ flags	
	uint		flags;	// IMAGE_ flags
	byte		*pal;
	byte		*source;
	byte		*scaled;
} image_desc;

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

	if( block == 0 ) BlockSize = width * height * bpp;
	else if( block > 0 ) BlockSize = ((width + 3)/4) * ((height + 3)/4) * depth * block;
	else if( block < 0 && rgbcount > 0 ) BlockSize = width * height * depth * rgbcount;
	else BlockSize = width * height * abs( block );

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

void R_RoundImageDimensions( int *width, int *height )
{
	// find nearest power of two, rounding down if desired
	*width = NearestPOW( *width, r_round_down->integer );
	*height = NearestPOW( *height, r_round_down->integer );

	// sample down if desired
	if( image_desc.tflags & TF_NORMALMAP )
	{
		if( r_round_down->integer && !(image_desc.tflags & TF_NOPICMIP))
		{
			while( *width > r_max_normal_texsize->integer || *height > r_max_normal_texsize->integer )
			{
				*width >>= 1;
				*height >>= 1;
			}
		}
	}
	else
	{
		if( r_round_down->integer && !(image_desc.tflags & TF_NOPICMIP))
		{
			while( *width > r_max_texsize->integer || *height > r_max_texsize->integer )
			{
				*width >>= 1;
				*height >>= 1;
			}
		}
	}

	// clamp to hardware limits
	if( image_desc.tflags & TF_CUBEMAP )
	{
		while( *width > gl_config.max_cubemap_texture_size || *height > gl_config.max_cubemap_texture_size )
		{
			*width >>= 1;
			*height >>= 1;
		}
	}
	else
	{
		while( *width > gl_config.max_2d_texture_size || *height > gl_config.max_2d_texture_size )
		{
			*width >>= 1;
			*height >>= 1;
		}
	}

	if( *width < 1 ) *width = 1;
	if( *height < 1 ) *height = 1;
}

/*
=================
R_ForceTextureBorder
=================
*/
static void R_ForceTextureBorder( byte *in, int width, int height, bool hasAlpha )
{
	byte	*out = in;
	uint	borderColor;
	int	i;

	if( hasAlpha ) borderColor = 0x00000000;
	else borderColor = 0xFF000000;

	// top
	for( i = 0, out = in; i < width; i++, out += 4 )
		*(uint *)out = borderColor;
	// bottom
	for( i = 0, out = in + (4 * ((height - 1) * width)); i < width; i++, out += 4 )
		*(uint *)out = borderColor;
	// left
	for( i = 0, out = in; i < height; i++, out += 4 * width )
		*(uint *)out = borderColor;

	// right
	for( i = 0, out = in + (4 * (width - 1)); i < height; i++, out += 4 * width )
		*(uint *)out = borderColor;
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
static void R_ResampleTexture( byte *source, int inWidth, int inHeight, int outWidth, int outHeight, bool isNormalMap )
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

	if( !pic || !pic->buffer ) return false;
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
	image_desc.numLayers = d = pic->numLayers;
	image_desc.width = w = pic->width;
	image_desc.height = h = pic->height;
	image_desc.flags = pic->flags;
	image_desc.tflags = tex_flags;

	image_desc.bps = image_desc.width * image_desc.bpp * image_desc.bpc;
	image_desc.SizeOfPlane = image_desc.bps * image_desc.height;
	image_desc.SizeOfData = image_desc.SizeOfPlane * image_desc.numLayers;
	image_desc.glSamples = R_GetSamples( image_desc.flags );
	image_desc.BitsCount = pic->bitsCount;

	// now correct buffer size
	for( i = 0; i < pic->numMips; i++, totalsize += mipsize )
	{
		mipsize = R_GetImageSize( BlockSize, w, h, d, image_desc.bpp, image_desc.BitsCount / 8 );
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1;
	}

	if( image_desc.tflags & TF_CUBEMAP )
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
				image_desc.tflags &= ~TF_SKYBOX;
			}
		}
		else
		{
			MsgDev( D_WARN, "R_GetPixelFormat: cubemaps isn't supported, %s ignored\n", name );
			image_desc.tflags &= ~TF_CUBEMAP;
			image_desc.tflags &= ~TF_SKYBOX;
		}
	}

	if( image_desc.tflags & TF_NOPICMIP|TF_SKYBOX )
	{
		// don't build mips for sky and hud pics
		image_desc.tflags &= ~TF_GEN_MIPS;
		image_desc.MipCount = 1; // and ignore it to load
	} 
	else if( pic->numMips > 1 )
	{
		// .dds, .vtf, .wal or .mip image
		image_desc.tflags &= ~TF_GEN_MIPS;
		image_desc.MipCount = pic->numMips;
	}
	else
	{
		// so it normal texture without mips
		image_desc.tflags |= TF_GEN_MIPS;
		image_desc.MipCount = pic->numMips;
	}
		
	if( image_desc.MipCount < 1 ) image_desc.MipCount = 1;
	image_desc.pal = pic->palette;

	// check for permanent images
	if( image_desc.format == PF_RGBA_GN ) image_desc.tflags |= TF_STATIC;
	if( image_desc.tflags & TF_NOPICMIP ) image_desc.tflags |= TF_STATIC;

	// restore temp dimensions
	w = image_desc.width;
	h = image_desc.height;
	s = w * h;

	// calc immediate buffers
	R_RoundImageDimensions( &w, &h );

	image_desc.source = Mem_Alloc( r_imagepool, s * 4 );	// source buffer
	image_desc.scaled = Mem_Alloc( r_imagepool, w * h * 4 );	// scaled buffer
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
	image_desc.glFormat = PFDesc( image_desc.format )->glFormat;
	image_desc.glType = PFDesc( image_desc.format )->glType;

	image_desc.numLayers = depth;
	image_desc.width = width;
	image_desc.height = height;

	image_desc.bps = image_desc.width * image_desc.bpp * image_desc.bpc;
	image_desc.SizeOfPlane = image_desc.bps * image_desc.height;
	image_desc.SizeOfData = image_desc.SizeOfPlane * image_desc.numLayers;

	// NOTE: size of current miplevel or cubemap side, not total (filesize - sizeof(header))
	image_desc.SizeOfFile = R_GetImageSize( BlockSize, width, height, depth, image_desc.bpp, image_desc.BitsCount / 8);
}

/*
 =================
 R_SetTextureParameters
 =================
*/
void R_SetTextureParameters( void )
{
	texture_t	*texture;
	int	i;

	if( !com.stricmp( r_texturefilter->string, "GL_NEAREST" ))
	{
		r_textureMinFilter = GL_NEAREST;
		r_textureMagFilter = GL_NEAREST;
	}
	else if( !com.stricmp( r_texturefilter->string, "GL_LINEAR" ))
	{
		r_textureMinFilter = GL_LINEAR;
		r_textureMagFilter = GL_LINEAR;
	}
	else if( !com.stricmp( r_texturefilter->string, "GL_NEAREST_MIPMAP_NEAREST" ))
	{
		r_textureMinFilter = GL_NEAREST_MIPMAP_NEAREST;
		r_textureMagFilter = GL_NEAREST;
	}
	else if( !com.stricmp( r_texturefilter->string, "GL_LINEAR_MIPMAP_NEAREST" ))
	{
		r_textureMinFilter = GL_LINEAR_MIPMAP_NEAREST;
		r_textureMagFilter = GL_LINEAR;
	}
	else if( !com.stricmp( r_texturefilter->string, "GL_NEAREST_MIPMAP_LINEAR" ))
	{
		r_textureMinFilter = GL_NEAREST_MIPMAP_LINEAR;
		r_textureMagFilter = GL_NEAREST;
	}
	else if( !com.stricmp( r_texturefilter->string, "GL_LINEAR_MIPMAP_LINEAR" ))
	{
		r_textureMinFilter = GL_LINEAR_MIPMAP_LINEAR;
		r_textureMagFilter = GL_LINEAR;
	}
	else
	{
		Cvar_Set( "gl_texturefilter", "GL_LINEAR_MIPMAP_LINEAR" );
		r_textureMinFilter = GL_LINEAR_MIPMAP_LINEAR;
		r_textureMagFilter = GL_LINEAR;
	}

	r_texturefilter->modified = false;

	if( GL_Support( R_ANISOTROPY_EXT ))
	{
		if( r_texturefilteranisotropy->value > gl_config.max_anisotropy )
			Cvar_SetValue( "r_anisotropy", gl_config.max_anisotropy );
		else if( r_texturefilteranisotropy->value < 1.0 )
			Cvar_SetValue( "r_anisotropy", 1.0f );
	}
	r_texturefilteranisotropy->modified = false;

	if( GL_Support( R_TEXTURE_LODBIAS ))
	{
		if( r_texturelodbias->value > gl_config.max_lodbias )
			Cvar_SetValue( "r_texture_lodbias", gl_config.max_lodbias );
		else if( r_texturelodbias->value < -gl_config.max_lodbias )
			Cvar_SetValue( "r_texture_lodbias", -gl_config.max_lodbias );
	}
	r_texturelodbias->modified = false;

	// change all the existing mipmapped texture objects
	for( i = 0; i < r_numTextures; i++ )
	{
		texture = r_textures[i];
		if( !texture ) continue;	// free slot

		if( texture->filter != TF_DEFAULT )
			continue;

		GL_BindTexture( texture );

		// set texture filter
		pglTexParameteri( texture->target, GL_TEXTURE_MIN_FILTER, r_textureMinFilter );
		pglTexParameteri( texture->target, GL_TEXTURE_MAG_FILTER, r_textureMagFilter );

		// set texture anisotropy if available
		if( GL_Support( R_ANISOTROPY_EXT ))
			pglTexParameterf( texture->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_texturefilteranisotropy->value );

		// set texture LOD bias if available
		if( GL_Support( R_TEXTURE_LODBIAS ))
			pglTexParameterf( texture->target, GL_TEXTURE_LOD_BIAS_EXT, r_texturelodbias->value );
	}
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
static rgbdata_t *R_HeightMap( rgbdata_t *in, float scale )
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
	out = image_desc.source;

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

			normal[0] = (c - cx) * scale;
			normal[1] = (c - cy) * scale;
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

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &token ))
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
 R_ParseHeightMap
 =================
*/
static rgbdata_t *R_ParseHeightMap( script_t *script, int *samples, texFlags_t *flags )
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

	pic = R_LoadImage( script, token.string, NULL, 0, samples, flags );
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
	else if( !com.stricmp( name, "heightMap" ))
		return R_ParseHeightMap( script, samples, flags );
	else if( !com.stricmp( name, "addNormals" ))
		return R_ParseAddNormals( script, samples, flags );
	else if( !com.stricmp( name, "smoothNormals" ))
		return R_ParseSmoothNormals( script, samples, flags );
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
void GL_GenerateMipmaps( byte *buffer, texture_t *tex, int side, bool border )
{
	int	mipLevel;
	int	mipWidth, mipHeight;

	// not needs
	//if(!(tex->flags & TF_GEN_MIPS)) return;
	if( tex->filter != TF_DEFAULT ) return;

	if( GL_Support( R_SGIS_MIPMAPS_EXT ))
	{
		pglHint( GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST );
		pglTexParameteri( image_desc.glTarget, GL_GENERATE_MIPMAP_SGIS, GL_TRUE );
		if(pglGetError()) MsgDev(D_WARN, "GL_GenerateMipmaps: can't create mip levels\n");
		else return; // falltrough to software mipmap generating
	}

	if( image_desc.format != PF_RGBA_32 && image_desc.format != PF_RGBA_GN )
	{
		// g-cont. because i'm don't know how to generate miplevels for GL_FLOAT or GL_SHORT_REV_1_bla_bla
		// ok, please show me videocard which don't supported GL_GENERATE_MIPMAP_SGIS ...
		MsgDev( D_ERROR, "GL_GenerateMipmaps: software mip generator failed on %s\n", PFDesc( image_desc.format )->name );
		return;
	}
	
	mipLevel = 0;
	mipWidth = tex->width;
	mipHeight = tex->height;

	// software mipmap generator
	while( mipWidth > 1 || mipHeight > 1 )
	{
		// Build the mipmap
		R_BuildMipMap( buffer, mipWidth, mipHeight, (tex->flags & TF_NORMALMAP));

		mipWidth = (mipWidth+1)>>1;
		mipHeight = (mipHeight+1)>>1;
		mipLevel++;

		// make sure it has a border if needed
		if( border ) R_ForceTextureBorder( buffer, mipWidth, mipHeight, (tex->wrap == TW_CLAMP_TO_ZERO_ALPHA));
		pglTexImage2D( image_desc.glTarget + side, mipLevel, tex->format, mipWidth, mipHeight, 0, image_desc.glFormat, image_desc.glType, buffer );
	}
}

void GL_TexFilter( texture_t *tex )
{
	vec4_t	zeroBorder = { 0.0f, 0.0f, 0.0f, 1.0f };
	vec4_t	zeroAlphaBorder = { 0.0f, 0.0f, 0.0f, 0.0f };

	// set texture filter
	switch( tex->filter )
	{
	case TF_DEFAULT:
		pglTexParameteri( tex->target, GL_TEXTURE_MIN_FILTER, r_textureMinFilter );
		pglTexParameteri( tex->target, GL_TEXTURE_MAG_FILTER, r_textureMagFilter );

		// set texture anisotropy if available
		if( GL_Support( R_ANISOTROPY_EXT ))
			pglTexParameterf( tex->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_texturefilteranisotropy->value );

		// set texture LOD bias if available
		if( GL_Support( R_TEXTURE_LODBIAS ))
			pglTexParameterf( tex->target, GL_TEXTURE_LOD_BIAS_EXT, r_texturelodbias->value );
		break;
	case TF_NEAREST:
		pglTexParameteri( tex->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		pglTexParameteri( tex->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		break;
	case TF_LINEAR:
		pglTexParameteri( tex->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		pglTexParameteri( tex->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		break;
	default:
		Host_Error( "GL_TexFilter: bad texture filter (%i)\n", tex->filter );
	}

	// set texture wrap
	switch( tex->wrap )
	{
	case TW_REPEAT:
		pglTexParameteri( tex->target, GL_TEXTURE_WRAP_S, GL_REPEAT );
		pglTexParameteri( tex->target, GL_TEXTURE_WRAP_T, GL_REPEAT );
		break;
	case TW_CLAMP:
		if(GL_Support( R_CLAMPTOEDGE_EXT ))
		{
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		}
		else
		{
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_S, GL_CLAMP );
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_T, GL_CLAMP );
		}
		break;
	case TW_CLAMP_TO_ZERO:
		if( GL_Support( R_CLAMP_TEXBORDER_EXT ))
		{
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER_ARB );
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER_ARB );
		}
		else
		{
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_S, GL_CLAMP );
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_T, GL_CLAMP );
		}
		pglTexParameterfv( tex->target, GL_TEXTURE_BORDER_COLOR, zeroBorder );
		break;
	case TW_CLAMP_TO_ZERO_ALPHA:
		if( GL_Support( R_CLAMP_TEXBORDER_EXT ))
		{
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER_ARB );
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER_ARB );
		}
		else
		{
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_S, GL_CLAMP );
			pglTexParameteri( tex->target, GL_TEXTURE_WRAP_T, GL_CLAMP );
		}
		pglTexParameterfv( tex->target, GL_TEXTURE_BORDER_COLOR, zeroAlphaBorder );
		break;
	default:
		Host_Error( "GL_TexFilter: bad texture wrap (%i)", tex->wrap );
	}
}

static void R_UploadTexture( rgbdata_t *pic, texture_t *tex )
{
	uint	mipsize = 0, offset = 0;
	bool	dxtformat = true;
	bool	compress, border;
	int	i, j, w, h, d;
	byte	*buf, *data;

	tex->width = tex->srcWidth;
	tex->height = tex->srcHeight;
	R_RoundImageDimensions( &tex->width, &tex->height );

	// check if it should be compressed
	if( tex->flags & TF_NORMALMAP )
	{
		if( !r_compress_normal_textures->integer || (tex->flags & TF_UNCOMPRESSED))
			compress = false;
		else compress = GL_Support( R_TEXTURE_COMPRESSION_EXT );
	}
	else
	{
		if( !r_compress_textures->integer || (tex->flags & TF_UNCOMPRESSED))
			compress = false;
		else compress = GL_Support( R_TEXTURE_COMPRESSION_EXT );
	}

	// check if it needs a border
	if( tex->wrap == TW_CLAMP_TO_ZERO || tex->wrap == TW_CLAMP_TO_ZERO_ALPHA )
		border = !GL_Support( R_CLAMP_TEXBORDER_EXT );
	else border = false;

	// set texture format
	if( compress )
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
		switch( tex->samples )
		{
		case 1: tex->format = GL_LUMINANCE8; break;
		case 2: tex->format = GL_LUMINANCE8_ALPHA8; break;
		case 3: tex->format = GL_RGB8; break;
		case 4: tex->format = GL_RGBA8; break;
		}

		if( tex->flags & TF_INTENSITY )
			tex->format = GL_INTENSITY8;
		if( tex->flags & TF_ALPHA )
			tex->format = GL_ALPHA8;
		tex->flags &= ~TF_INTENSITY;
		tex->flags &= ~TF_ALPHA;
	}

	// also check for compressed textures
	switch( pic->type )
	{
	case PF_DXT1: tex->format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
	case PF_DXT3: tex->format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
	case PF_DXT5: tex->format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
	default: dxtformat = false; break;
	}

	pglGenTextures( 1, &tex->texnum );
	GL_BindTexture( tex );
		
	// uploading texture into video memory
	for( i = 0, buf = pic->buffer; i < image_desc.numSides; i++, buf += offset )
	{
		R_SetPixelFormat( image_desc.width, image_desc.height, image_desc.numLayers );
		w = image_desc.width, h = image_desc.height, d = image_desc.numLayers;
		offset = image_desc.SizeOfFile; // member side offset

		for( j = 0; j < image_desc.MipCount; j++, buf += mipsize )
		{
			R_SetPixelFormat( w, h, d );
			mipsize = image_desc.SizeOfFile; // member mipsize offset
			tex->size += mipsize;

			// copy or resample the texture
			if( tex->width == tex->srcWidth && tex->height == tex->srcHeight ) data = buf;
			else
			{
				R_ResampleTexture( buf, tex->srcWidth, tex->srcHeight, tex->width, tex->height, (tex->flags & TF_NORMALMAP));
				data = image_desc.scaled;
			}
			if( border ) R_ForceTextureBorder( data, tex->width, tex->height, (tex->wrap == TW_CLAMP_TO_ZERO_ALPHA));
			if( j == 0 ) GL_GenerateMipmaps( data, tex, i, border );
			if( dxtformat ) pglCompressedTexImage2DARB( image_desc.texTarget + i, j, tex->format, w, h, 0, mipsize, data );
			else pglTexImage2D( image_desc.texTarget + i, j, tex->format, tex->width, tex->height, 0, image_desc.glFormat, image_desc.glType, data );

			w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1; // calc size of next mip
			if( r_check_errors->integer ) R_CheckForErrors();
		}
	}
}

/*
=================
R_LoadTexture
=================
*/
texture_t *R_LoadTexture( const char *name, rgbdata_t *pic, int samples, texFlags_t flags, texFilter_t filter, texWrap_t wrap )
{
	texture_t	*texture;
	uint	i, hash;

	if( r_numTextures == MAX_TEXTURES )
		Host_Error( "R_LoadTexture: MAX_TEXTURES limit exceeds\n" );

	// find a free texture_t slot
	for( i = 0; i < r_numTextures; i++ )
		if( !r_textures[i] ) break;

	if( i == r_numTextures )
		r_textures[r_numTextures++] = texture = Mem_Alloc( r_texpool, sizeof( texture_t ));
	else r_textures[i] = texture = Mem_Alloc( r_texpool, sizeof( texture_t ));

	// fill it in
	com.strncpy( texture->name, name, sizeof( texture->name ));
	texture->filter = filter;
	texture->wrap = wrap;
	texture->srcWidth = pic->width;
	texture->srcHeight = pic->height;
	texture->numLayers = pic->numLayers;
	texture->touchFrame = registration_sequence;
	if( samples <= 0 ) texture->samples = R_GetSamples( pic->flags );
	else texture->samples = samples;

	// setup image_desc
	R_GetPixelFormat( name, pic, flags );
	texture->flags = image_desc.tflags;
	texture->target = image_desc.glTarget;
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
texture_t *R_FindTexture( const char *name, const byte *buf, size_t size, texFlags_t flags, texFilter_t filter, texWrap_t wrap )
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
		if( texture->flags & TF_CUBEMAP|TF_SKYBOX )
			continue;

		if( !com.stricmp( texture->name, name ))
		{
			if( texture->flags & TF_STATIC )
				return texture;

			if( texture->flags != flags )
				MsgDev( D_WARN, "reused texture '%s' with mixed flags parameter\n", name );
			if( texture->filter != filter )
				MsgDev( D_WARN, "reused texture '%s' with mixed filter parameter\n", name );
			if( texture->wrap != wrap )
				MsgDev( D_WARN, "reused texture '%s' with mixed wrap parameter\n", name );

			// prolonge registration
			texture->touchFrame = registration_sequence;
			return texture;
		}
	}

	// NOTE: texname may contains some commands over textures
	script = Com_OpenScript( name, name, com.strlen( name ));
	if( !script ) return NULL;

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES|(name[0] == '#') ? SC_PARSE_GENERIC : 0, &token ))
	{
		Com_CloseScript( script );
		return NULL;
	}

	image = R_LoadImage( script, token.string, buf, size, &samples, &flags );
	Com_CloseScript( script );

	// load the texture
	if( image )
	{
		texture = R_LoadTexture( name, image, samples, flags, filter, wrap );
		FS_FreeImage( image );
		return texture;
	}

	// not found or invalid
	return NULL;
}

texture_t *R_FindCubeMapTexture( const char *name, const byte *buf, size_t size, texFlags_t flags, texFilter_t filter, texWrap_t wrap, bool skybox )
{
	if( skybox ) flags |= TF_SKYBOX;
	return R_FindTexture( name, buf, size, flags|TF_CUBEMAP, filter, wrap );
}

/*
=================
R_CreateBuiltInTextures
=================
*/
static void R_CreateBuiltInTextures( void )
{
	rgbdata_t	pic;
	byte	data2D[256*256*4];
	float	s, t, intensity;
	int	i, x, y;
	vec3_t	normal;

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

	Mem_Set( &pic, 0, sizeof( pic ));
	pic.width = pic.height = 16;
	pic.type = PF_RGBA_GN; // generated
	pic.size = pic.width * pic.height * 4;
	pic.numMips = 1;
	pic.buffer = (byte *)data2D;
	r_defaultTexture = R_LoadTexture( "*default", &pic, 1, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED, TF_DEFAULT, TW_REPEAT );

	// white texture
	for( i = 0; i < 64; i++ ) ((uint *)&data2D)[i] = LittleLong( 0xFFFFFFFF );
	pic.width = pic.height = 8;
	pic.size = pic.width * pic.height * 4;
	r_whiteTexture = R_LoadTexture("*white", &pic, 1, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED, TF_DEFAULT, TW_REPEAT );

	// black texture
	for( i = 0; i < 64; i++ ) ((uint *)&data2D)[i] = LittleLong( 0xFF000000 );
	r_blackTexture = R_LoadTexture("*black", &pic, 1, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED, TF_DEFAULT, TW_REPEAT );

	// flat texture
	for( i = 0; i < 64; i++ ) ((uint *)&data2D)[i] = LittleLong(0xFFFF8080);
	r_flatTexture = R_LoadTexture( "*flat", &pic, 3, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED|TF_NORMALMAP, TF_DEFAULT, TW_REPEAT );

	// attenuation texture
	for( y = 0; y < 128; y++ )
	{
		for( x = 0; x < 128; x++ )
		{
			s = ((((float)x + 0.5f) * (2.0f/128)) - 1.0f) * (1.0f/0.9375f);
			t = ((((float)y + 0.5f) * (2.0f/128)) - 1.0f) * (1.0f/0.9375f);

			intensity = 1.0 - com.sqrt( s*s + t*t );
			intensity = bound( 0.0f, 255.0f * intensity, 255.0f );

			data2D[4*(y*128+x)+0] = (byte)intensity;
			data2D[4*(y*128+x)+1] = (byte)intensity;
			data2D[4*(y*128+x)+2] = (byte)intensity;
			data2D[4*(y*128+x)+3] = 255;
		}
	}

	pic.width = pic.height = 128;
	pic.size = pic.width * pic.height * 4;
	r_attenuationTexture = R_LoadTexture( "*attenuation", &pic, 1, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED, TF_DEFAULT, TW_CLAMP_TO_ZERO );

	// falloff texture
	for( y = 0; y < 8; y++ )
	{
		for( x = 0; x < 64; x++ )
		{
			s = ((((float)x + 0.5f) * (2.0f/64)) - 1.0f) * (1.0f/0.9375f);

			intensity = 1.0f - com.sqrt( s*s );
			intensity = bound( 0.0f, 255.0 * intensity, 255.0f );

			data2D[4*(y*64+x)+0] = (byte)intensity;
			data2D[4*(y*64+x)+1] = (byte)intensity;
			data2D[4*(y*64+x)+2] = (byte)intensity;
			data2D[4*(y*64+x)+3] = (byte)intensity;
		}
	}

	pic.width = 64;
	pic.height = 8;
	pic.size = pic.width * pic.height * 4;
	r_falloffTexture = R_LoadTexture( "*falloff", &pic, 1, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED|TF_INTENSITY, TF_LINEAR, TW_CLAMP );

	// fog texture
	for( y = 0; y < 128; y++ )
	{
		for( x = 0; x < 128; x++)
		{
			s = ((((float)x + 0.5f) * (2.0f/128)) - 1.0f) * (1.0f/0.9375f);
			t = ((((float)y + 0.5f) * (2.0f/128)) - 1.0f) * (1.0f/0.9375f);

			intensity = pow( com.sqrt( s*s + t*t ), 0.5f );
			intensity = bound( 0.0f, 255.0f * intensity, 255.0f );

			data2D[4*(y*128+x)+0] = 255;
			data2D[4*(y*128+x)+1] = 255;
			data2D[4*(y*128+x)+2] = 255;
			data2D[4*(y*128+x)+3] = (byte)intensity;
		}
	}

	pic.width = pic.height = 128;
	pic.size = pic.width * pic.height * 4;
	r_fogTexture = R_LoadTexture( "*fog", &pic, 1, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED|TF_ALPHA, TF_LINEAR, TW_CLAMP );

	// fog-enter texture
	for( y = 0; y < 64; y++ )
	{
		for( x = 0; x < 64; x++ )
		{
			s = ((((float)x + 0.5) * (2.0/64)) - 1.0) * (1.0/0.3750);
			t = ((((float)y + 0.5) * (2.0/64)) - 1.0) * (1.0/0.3750);
			s = bound( -1.0f, s + (1.0f/16), 0.0f );
			t = bound( -1.0f, t + (1.0f/16), 0.0f );

			intensity = pow( com.sqrt( s*s + t*t ), 0.5f );
			intensity = bound( 0.0f, 255.0 * intensity, 255.0f );

			data2D[4*(y*64+x)+0] = 255;
			data2D[4*(y*64+x)+1] = 255;
			data2D[4*(y*64+x)+2] = 255;
			data2D[4*(y*64+x)+3] = (byte)intensity;
		}
	}

	pic.width = pic.height = 64;
	pic.size = pic.width * pic.height * 4;
	r_fogEnterTexture = R_LoadTexture( "*fogEnter", &pic, 1, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED|TF_ALPHA, TF_LINEAR, TW_CLAMP );

	// cinematic texture
	Mem_Set( data2D, 0xFF, 16*16*4 );
	pic.width = pic.height = 16;
	pic.size = pic.width * pic.height * 4;
	r_cinematicTexture = R_LoadTexture( "*cinematic", &pic, 4, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED, TF_LINEAR, TW_REPEAT );

	// scratch texture
	r_scratchTexture = R_LoadTexture( "*scratch", &pic, 4, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED, TF_LINEAR, TW_REPEAT );

	// accum texture
	r_accumTexture = R_LoadTexture( "*accum", &pic, 3, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED, TF_LINEAR, TW_CLAMP );

	// mirror render texture
	r_mirrorRenderTexture = R_LoadTexture( "*mirrorRender", &pic, 3, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED, TF_LINEAR, TW_CLAMP );

	// portal render texture
	r_portalRenderTexture = R_LoadTexture( "*remoteRender", &pic, 3, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED, TF_LINEAR, TW_REPEAT );

	// current render texture
	r_currentRenderTexture = R_LoadTexture( "*currentRender", &pic, 3, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED, TF_LINEAR, TW_CLAMP );

	// raw texture
	Mem_Set( data2D, 255, 256*256*4 );
	pic.width = pic.height = 256;
	pic.size = pic.width * pic.height * 4;
	r_rawTexture = R_LoadTexture( "*raw", &pic, 3, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED, TF_LINEAR, TW_CLAMP );

	if( GL_Support( R_TEXTURECUBEMAP_EXT ))
	{
		byte	data3D[32*32*4*6]; // full cubemap size
		byte	*dataCM = (byte *)data3D;

		// normal cube map texture
		for( i = 0; i < 6; i++ )
		{
			for( y = 0; y < 32; y++ )
			{
				for( x = 0; x < 32; x++ )
				{
					s = (((float)x + 0.5f) * (2.0f/32)) - 1.0f;
					t = (((float)y + 0.5f) * (2.0f/32)) - 1.0f;

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

					dataCM[4*(y*32+x)+0] = (byte)(128 + 127 * normal[0]);
					dataCM[4*(y*32+x)+1] = (byte)(128 + 127 * normal[1]);
					dataCM[4*(y*32+x)+2] = (byte)(128 + 127 * normal[2]);
					dataCM[4*(y*32+x)+3] = 255;
				}
			}
			dataCM += (32*32*4); // move pointer
		}
		pic.width = pic.height = 32;
		pic.size = pic.width * pic.height * 4 * 6;
		pic.flags |= IMAGE_CUBEMAP; // yes it's cubemap
		pic.buffer = (byte *)data3D;
		r_normalizeTexture = R_LoadTexture( "*normalCubeMap", &pic, 3, TF_STATIC|TF_NOPICMIP|TF_UNCOMPRESSED|TF_CUBEMAP, TF_LINEAR, TW_CLAMP );
	}

	// screen rect texture (just reserve a slot)
	if( gl_config.texRectangle ) pglGenTextures( 1, &gl_state.screenTexture );
}

/*
================
R_FreeImage
================
*/
static void R_FreeImage( texture_t *image )
{
	uint	hash;

	if( !image ) return;

	// add to hash table
	Msg( "release texture %s\n", image->name );
	hash = Com_HashKey( image->name, TEXTURES_HASH_SIZE );
	r_texturesHashTable[hash] = image->nextHash;

	pglDeleteTextures( 1, &image->texnum );
	Mem_Free( image );
}

/*
================
R_ImageFreeUnused

Any image that was not touched on this registration sequence
will be freed.
================
*/
void R_ImageFreeUnused( void )
{
	texture_t	*image;
	int	i;

	for( i = 0; i < r_numTextures; i++ )
	{
		image = r_textures[i];
		if( !image ) continue;
		
		// used this sequence
		if( image->touchFrame == registration_sequence ) continue;
		if( image->flags & TF_STATIC ) continue;

		R_FreeImage( image );
		r_textures[i] = NULL;
	}
}

/*
=================
R_TextureList_f
=================
*/
void R_TextureList_f( void )
{
	texture_t	*texture;
	int	i, bytes = 0;

	Msg( "\n" );
	Msg("      -w-- -h-- -size- -fmt- type -filter -wrap-- -name--------\n" );

	for( i = 0; i < r_numTextures; i++ )
	{
		texture = r_textures[i];
		if( !texture ) continue;

		bytes += texture->size;

		Msg( "%4i: ", i );
		Msg( "%4i %4i ", texture->width, texture->height );
		Msg( "%5ik ", texture->size >> 10 );

		switch( texture->format )
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
		case GL_RGBA8:
			Msg( "RGBA8 " );
			break;
		case GL_RGB8:
			Msg( "RGB8  " );
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

		switch( texture->target )
		{
		case GL_TEXTURE_2D:
			Msg( " 2D  " );
			break;
		case GL_TEXTURE_CUBE_MAP_ARB:
			Msg( "CUBE " );
			break;
		default:
			Msg( "???? " );
			break;
		}

		switch( texture->filter )
		{
		case TF_DEFAULT:
			Msg( "default" );
			break;
		case TF_NEAREST:
			Msg( "nearest" );
			break;
		case TF_LINEAR:
			Msg( "linear " );
			break;
		default:
			Msg( "????   " );
			break;
		}

		switch( texture->wrap )
		{
		case TW_REPEAT:
			Msg( "repeat " );
			break;
		case TW_CLAMP:
			Msg( "clamp  " );
			break;
		case TW_CLAMP_TO_ZERO:
			Msg( "clamp 0" );
			break;
		case TW_CLAMP_TO_ZERO_ALPHA:
			Msg( "clamp A" );
			break;
		default:
			Msg( "????   " );
			break;
		}
		Msg( "%s\n", texture->name );
	}

	Msg( "---------------------------------------------------------\n" );
	Msg( "%i total textures\n", r_numTextures );
	Msg( "%.2f total megabytes of textures\n", bytes/1048576.0 );
	Msg( "\n" );
}

/*
=================
R_InitTextures
=================
*/
void R_InitTextures( void )
{
	int	i, j;
	float	f;

	r_texpool = Mem_AllocPool( "Texture Manager Pool" );
	r_imagepool = Mem_AllocPool( "Immediate TexturePool" );	// for scaling and resampling
	pglGetIntegerv( GL_MAX_TEXTURE_SIZE, &gl_config.max_2d_texture_size );

	r_numTextures = 0;
	registration_sequence = 1;
	Mem_Set( r_textures, 0, sizeof( r_textures ));
	Mem_Set( r_texturesHashTable, 0, sizeof( r_texturesHashTable ));
	
	// init intensity conversions
	r_intensity = Cvar_Get( "r_intensity", "2", 0, "gamma intensity value" );
	if( r_intensity->value <= 1 ) Cvar_SetValue( "r_intensity", 1 );

	// build intensity table
	for( i = 0; i < 256; i++ )
	{
		j = i * r_intensity->value;
		r_intensityTable[i] = bound( 0, j, 255 );
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

	// Create built-in textures
	R_CreateBuiltInTextures();
}

/*
=================
R_ShutdownTextures
=================
*/
void R_ShutdownTextures( void )
{
	int	i;
	texture_t	*texture;

	for( i = MAX_TEXTURE_UNITS - 1; i >= 0; i-- )
	{
		if( GL_Support( R_FRAGMENT_PROGRAM_EXT ))
		{
			if( i >= gl_config.texturecoords || i >= gl_config.teximageunits )
				continue;
		}
		else
		{
			if( i >= gl_config.textureunits )
				continue;
		}

		GL_SelectTexture( i );
		pglBindTexture( GL_TEXTURE_2D, 0 );
		pglBindTexture( GL_TEXTURE_CUBE_MAP_ARB, 0 );
	}

	if( gl_config.texRectangle )
		pglDeleteTextures( 1, &gl_state.screenTexture );

	for( i = 0; i < r_numTextures; i++ )
	{
		texture = r_textures[i];
		if( !texture ) continue;

		pglDeleteTextures( 1, &texture->texnum );
		Mem_Free( texture );
		texture = NULL;
	}

	Mem_Set( r_texturesHashTable, 0, sizeof( r_texturesHashTable ));
	Mem_Set( r_textures, 0, sizeof( r_textures ));

	r_numTextures = 0;
}

/*
================
VID_ImageAdjustGamma
================
*/
void VID_ImageAdjustGamma( byte *in, uint width, uint height )
{
	int	i, c = width * height;
	float	g = vid_gamma->value;
	byte	*p = in;

	// screenshots gamma	
	for( i = 0; i < 256; i++ )
	{
		if ( g == 1 ) r_gammaTable[i] = i;
		else r_gammaTable[i] = bound(0, 255 * pow((i + 0.5)/255.5 , g ) + 0.5, 255);
	}
	for( i = 0; i < c; i++, p += 3 )
	{
		p[0] = r_gammaTable[p[0]];
		p[1] = r_gammaTable[p[1]];
		p[2] = r_gammaTable[p[2]];
	}
}

bool VID_ScreenShot( const char *filename, bool levelshot )
{
	rgbdata_t *r_shot;
	uint	flags = IMAGE_FLIP_Y;

	r_shot = Mem_Alloc( r_imagepool, sizeof( rgbdata_t ));
	r_shot->width = r_width->integer;
	r_shot->height = r_height->integer;
	r_shot->type = PF_RGB_24;
	r_shot->size = r_shot->width * r_shot->height * PFDesc( r_shot->type )->bpp;
	r_shot->palette = NULL;
	r_shot->numLayers = 1;
	r_shot->numMips = 1;
	r_shot->buffer = Mem_Alloc( r_temppool, r_width->integer * r_height->integer * 3 );

	// get screen frame
	pglReadPixels( 0, 0, r_width->integer, r_height->integer, GL_RGB, GL_UNSIGNED_BYTE, r_shot->buffer );

	if( levelshot ) flags |= IMAGE_RESAMPLE;
	else VID_ImageAdjustGamma( r_shot->buffer, r_shot->width, r_shot->height ); // adjust brightness
	Image_Process( &r_shot, 512, 384, flags );

	// write image
	FS_SaveImage( filename, r_shot );
	FS_FreeImage( r_shot );
	return true;
}

/*
=================
VID_CubemapShot
=================
*/
bool VID_CubemapShot( const char *base, uint size, bool skyshot )
{
	rgbdata_t		*r_shot;
	byte		*buffer = NULL;
	int		i = 1;

	if(( r_refdef.rdflags & RDF_NOWORLDMODEL) || !r_worldModel)
		return false;

	// shared framebuffer not init
	if( !r_framebuffer ) return false;

	// make sure the specified size is valid
	while( i < size ) i<<=1;

	if( i != size ) return false;
	if( size > r_width->integer || size > r_height->integer )
		return false;

	// setup refdef
	r_refdef.rect.x = 0;
	r_refdef.rect.y = 0;
	r_refdef.rect.width = size;
	r_refdef.rect.height = size;
	r_refdef.fov_x = 90;
	r_refdef.fov_y = 90;

	// alloc space
	buffer = Mem_Alloc( r_temppool, size * size * 3 * 6 );

	for( i = 0; i < 6; i++ )
	{
		if( skyshot ) VectorCopy( r_skyBoxAngles[i], r_refdef.viewangles );
		else VectorCopy( r_cubeMapAngles[i], r_refdef.viewangles );

		R_RenderView( &r_refdef );
		pglReadPixels( 0, r_height->integer - size, size, size, GL_RGB, GL_UNSIGNED_BYTE, r_framebuffer );
		Mem_Copy( buffer + (size * size * 3 * i), r_framebuffer, size * size * 3 );
	}

	r_shot = Mem_Alloc( r_imagepool, sizeof( rgbdata_t ));
	r_shot->width = size;
	r_shot->height = size;
	r_shot->type = PF_RGB_24;
	r_shot->size = r_shot->width * r_shot->height * 3;
	r_shot->palette = NULL;
	r_shot->numLayers = 1;
	r_shot->numMips = 1;
	r_shot->buffer = buffer;
	
	// write image as dds packet
	FS_SaveImage( va( "gfx/env/%s.dds", base ), r_shot );
	FS_FreeImage( r_shot ); // don't touch framebuffer!

	return true;
}