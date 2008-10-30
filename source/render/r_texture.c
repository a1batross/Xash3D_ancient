//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_texture.c - load & convert textures
//=======================================================================

#include "r_local.h"
#include "byteorder.h"
#include "mathlib.h"

#define NUM_TEXTURE_FILTERS	(sizeof( r_textureFilters ) / sizeof( textureFilter_t ))

typedef struct
{
	const char	*name;
	int		min;
	int		mag;
} textureFilter_t;

static texture_t r_textures[MAX_TEXTURES];
static int r_numTextures;
static double totalTime = 0;

static textureFilter_t r_textureFilters[] =
{
{"GL_NEAREST",		GL_NEAREST,		GL_NEAREST},
{"GL_LINEAR",		GL_LINEAR,		GL_LINEAR	},
{"GL_NEAREST_MIPMAP_NEAREST",	GL_NEAREST_MIPMAP_NEAREST,	GL_NEAREST},
{"GL_LINEAR_MIPMAP_NEAREST",	GL_LINEAR_MIPMAP_NEAREST,	GL_LINEAR	},
{"GL_NEAREST_MIPMAP_LINEAR",	GL_NEAREST_MIPMAP_LINEAR,	GL_NEAREST},
{"GL_LINEAR_MIPMAP_LINEAR",	GL_LINEAR_MIPMAP_LINEAR,	GL_LINEAR	},
};

typedef struct
{
	string		name;
	int		format;
	int		width;
	int		height;
	int		bpp;
	int		bpc;
	int		bps;
	int		SizeOfPlane;
	int		SizeOfData;
	int		SizeOfFile;
	int		numLayers;
	int		MipCount;
	int		BitsCount;
	float		bumpScale;
	GLuint		glMask;
	GLuint		glType;
	GLuint		glTarget;
	GLuint		glSamples;

	// image loader
	bool		(*texImage)( GLuint, GLint, GLint, GLuint, GLuint, GLint, GLuint, const GLvoid* );

	uint		tflags;	// TF_ flags	
	uint		flags;	// IMAGE_ flags
	byte		*pal;
	byte		*source;
	byte		*scaled;
} PixFormatDesc;

static PixFormatDesc image_desc;
static int r_textureFilterMin = GL_LINEAR_MIPMAP_LINEAR;
static int r_textureFilterMag = GL_LINEAR;
static vec3_t r_luminanceTable[256];
static byte r_intensityTable[256];
static byte r_gammaTable[256];

const char *r_skyBoxSuffix[6] = {"rt", "lf", "bk", "ft", "up", "dn"};
vec3_t r_skyBoxAngles[6] =
{
{   0,   0,   0},
{   0, 180,   0},
{   0,  90,   0},
{   0, 270,   0},
{ -90,   0,   0},
{  90,   0,   0}
};

const char *r_cubeMapSuffix[6] = {"px", "nx", "py", "ny", "pz", "nz"};
vec3_t r_cubeMapAngles[6] =
{
{   0, 180,  90},
{   0,   0, 270},
{   0,  90, 180},
{   0, 270,   0},
{ -90, 270,   0},
{  90,  90,   0}
};

int skyorder_q2[6] = { 2, 3, 1, 0, 4, 5, }; // Quake, Half-Life skybox ordering
int skyorder_ms[6] = { 4, 5, 1, 0, 2, 3  }; // Microsoft DDS ordering (reverse)

texture_t	*r_defaultTexture;
texture_t	*r_whiteTexture;
texture_t	*r_blackTexture;
texture_t	*r_rawTexture;
texture_t	*r_dlightTexture;
texture_t	*r_lightmapTextures[MAX_LIGHTMAPS];
texture_t	*r_normalizeTexture;
texture_t *r_radarMap;
texture_t *r_aroundMap;

/*
=================
R_AddImages

Adds the given images together
=================
*/
static byte *R_AddImages( byte *in1, byte *in2, int width, int height )
{
	byte	*out = in1;
	int	r, g, b, a;
	int	x, y;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = in1[4*(y*width+x)+0] + in2[4*(y*width+x)+0];
			g = in1[4*(y*width+x)+1] + in2[4*(y*width+x)+1];
			b = in1[4*(y*width+x)+2] + in2[4*(y*width+x)+2];
			a = in1[4*(y*width+x)+3] + in2[4*(y*width+x)+3];
			out[4*(y*width+x)+0] = bound( 0, r, 255 );
			out[4*(y*width+x)+1] = bound( 0, g, 255 );
			out[4*(y*width+x)+2] = bound( 0, b, 255 );
			out[4*(y*width+x)+3] = bound( 0, a, 255 );
		}
	}
	Mem_Free( in2 );

	return out;
}

/*
=================
R_MultiplyImages

Multiplies the given images
=================
*/
static byte *R_MultiplyImages( byte *in1, byte *in2, int width, int height )
{
	byte	*out = in1;
	int	r, g, b, a;
	int	x, y;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = in1[4*(y*width+x)+0] * (in2[4*(y*width+x)+0] * (1.0/255));
			g = in1[4*(y*width+x)+1] * (in2[4*(y*width+x)+1] * (1.0/255));
			b = in1[4*(y*width+x)+2] * (in2[4*(y*width+x)+2] * (1.0/255));
			a = in1[4*(y*width+x)+3] * (in2[4*(y*width+x)+3] * (1.0/255));
			out[4*(y*width+x)+0] = bound( 0, r, 255 );
			out[4*(y*width+x)+1] = bound( 0, g, 255 );
			out[4*(y*width+x)+2] = bound( 0, b, 255 );
			out[4*(y*width+x)+3] = bound( 0, a, 255 );
		}
	}
	Mem_Free( in2 );

	return out;
}

/*
=================
R_BiasImage

Biases the given image
=================
*/
static byte *R_BiasImage( byte *in, int width, int height, const vec4_t bias )
{
	int	x, y;
	byte	*out = in;
	int	r, g, b, a;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = in[4*(y*width+x)+0] + (255 * bias[0]);
			g = in[4*(y*width+x)+1] + (255 * bias[1]);
			b = in[4*(y*width+x)+2] + (255 * bias[2]);
			a = in[4*(y*width+x)+3] + (255 * bias[3]);
			out[4*(y*width+x)+0] = bound( 0, r, 255 );
			out[4*(y*width+x)+1] = bound( 0, g, 255 );
			out[4*(y*width+x)+2] = bound( 0, b, 255 );
			out[4*(y*width+x)+3] = bound( 0, a, 255 );
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
static byte *R_ScaleImage( byte *in, int width, int height, const vec4_t scale )
{
	byte	*out = in;
	int	r, g, b, a;
	int	x, y;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = in[4*(y*width+x)+0] * scale[0];
			g = in[4*(y*width+x)+1] * scale[1];
			b = in[4*(y*width+x)+2] * scale[2];
			a = in[4*(y*width+x)+3] * scale[3];
			out[4*(y*width+x)+0] = bound( 0, r, 255 );
			out[4*(y*width+x)+1] = bound( 0, g, 255 );
			out[4*(y*width+x)+2] = bound( 0, b, 255 );
			out[4*(y*width+x)+3] = bound( 0, a, 255 );
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
static byte *R_InvertColor( byte *in, int width, int height )
{
	byte	*out = in;
	int	x, y;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			out[4*(y*width+x)+0] = 255 - in[4*(y*width+x)+0];
			out[4*(y*width+x)+1] = 255 - in[4*(y*width+x)+1];
			out[4*(y*width+x)+2] = 255 - in[4*(y*width+x)+2];
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
static byte *R_InvertAlpha( byte *in, int width, int height )
{
	byte	*out = in;
	int	x, y;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
			out[4*(y*width+x)+3] = 255 - in[4*(y*width+x)+3];
	}
	return out;
}

/*
=================
R_MakeIntensity

Converts the given image to intensity
=================
*/
static byte *R_MakeIntensity( byte *in, int width, int height )
{
	byte	*out = in;
	byte	intensity;
	float	r, g, b;
	int	x, y;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = r_luminanceTable[in[4*(y*width+x)+0]][0];
			g = r_luminanceTable[in[4*(y*width+x)+1]][1];
			b = r_luminanceTable[in[4*(y*width+x)+2]][2];

			intensity = (byte)(r + g + b);

			out[4*(y*width+x)+0] = intensity;
			out[4*(y*width+x)+1] = intensity;
			out[4*(y*width+x)+2] = intensity;
			out[4*(y*width+x)+3] = intensity;
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
static byte *R_MakeLuminance( byte *in, int width, int height )
{
	byte	*out = in;
	byte	luminance;
	float	r, g, b;
	int	x, y;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = r_luminanceTable[in[4*(y*width+x)+0]][0];
			g = r_luminanceTable[in[4*(y*width+x)+1]][1];
			b = r_luminanceTable[in[4*(y*width+x)+2]][2];

			luminance = (byte)(r + g + b);

			out[4*(y*width+x)+0] = luminance;
			out[4*(y*width+x)+1] = luminance;
			out[4*(y*width+x)+2] = luminance;
			out[4*(y*width+x)+3] = 255;
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
static byte *R_MakeAlpha( byte *in, int width, int height )
{
	byte	*out = in;
	byte	alpha;
	float	r, g, b;
	int	x, y;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = r_luminanceTable[in[4*(y*width+x)+0]][0];
			g = r_luminanceTable[in[4*(y*width+x)+1]][1];
			b = r_luminanceTable[in[4*(y*width+x)+2]][2];

			alpha = (byte)(r + g + b);

			out[4*(y*width+x)+0] = 255;
			out[4*(y*width+x)+1] = 255;
			out[4*(y*width+x)+2] = 255;
			out[4*(y*width+x)+3] = alpha;
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
static byte *R_HeightMap( byte *in, int width, int height, float scale )
{
	byte	*out;
	vec3_t	normal;
	float	r, g, b;
	float	c, cx, cy;
	int	x, y;

	out = image_desc.source;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			r = r_luminanceTable[in[4*(y*width+x)+0]][0];
			g = r_luminanceTable[in[4*(y*width+x)+1]][1];
			b = r_luminanceTable[in[4*(y*width+x)+2]][2];

			c = (r + g + b) * (1.0/255);

			r = r_luminanceTable[in[4*(y*width+((x+1)%width))+0]][0];
			g = r_luminanceTable[in[4*(y*width+((x+1)%width))+1]][1];
			b = r_luminanceTable[in[4*(y*width+((x+1)%width))+2]][2];

			cx = (r + g + b) * (1.0/255);

			r = r_luminanceTable[in[4*(((y+1)%height)*width+x)+0]][0];
			g = r_luminanceTable[in[4*(((y+1)%height)*width+x)+1]][1];
			b = r_luminanceTable[in[4*(((y+1)%height)*width+x)+2]][2];

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
	return out;
}

/*
 =================
 R_AddNormals

 Adds the given normal maps together
 =================
*/
static byte *R_AddNormals (byte *in1, byte *in2, int width, int height){

	byte	*out;
	vec3_t	normal;
	int	x, y;

	out = image_desc.source;

	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++ )
		{
			normal[0] = (in1[4*(y*width+x)+0] * (1.0/127) - 1.0) + (in2[4*(y*width+x)+0] * (1.0/127) - 1.0);
			normal[1] = (in1[4*(y*width+x)+1] * (1.0/127) - 1.0) + (in2[4*(y*width+x)+1] * (1.0/127) - 1.0);
			normal[2] = (in1[4*(y*width+x)+2] * (1.0/127) - 1.0) + (in2[4*(y*width+x)+2] * (1.0/127) - 1.0);

			if(!VectorNormalizeLength( normal )) VectorSet( normal, 0.0f, 0.0f, 1.0f );

			out[4*(y*width+x)+0] = (byte)(128 + 127 * normal[0]);
			out[4*(y*width+x)+1] = (byte)(128 + 127 * normal[1]);
			out[4*(y*width+x)+2] = (byte)(128 + 127 * normal[2]);
			out[4*(y*width+x)+3] = 255;
		}
	}

	Mem_Free( in1 );
	Mem_Free( in2 );

	return out;
}

/*
 =================
 R_SmoothNormals

 Smoothes the given normal map
 =================
*/
static byte *R_SmoothNormals( byte *in, int width, int height )
{
	byte	*out;
	uint	frac, fracStep;
	uint	p1[0x1000], p2[0x1000];
	byte	*pix1, *pix2, *pix3, *pix4;
	uint	*inRow1, *inRow2;
	vec3_t	normal;
	int	i, x, y;

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
		inRow1 = (uint *)in + width * (int)((float)y + 0.25);
		inRow2 = (uint *)in + width * (int)((float)y + 0.75);

		for( x = 0; x < width; x++ )
		{
			pix1 = (byte *)inRow1 + p1[x];
			pix2 = (byte *)inRow1 + p2[x];
			pix3 = (byte *)inRow2 + p1[x];
			pix4 = (byte *)inRow2 + p2[x];

			normal[0] = (pix1[0] * (1.0/127) - 1.0) + (pix2[0] * (1.0/127) - 1.0) + (pix3[0] * (1.0/127) - 1.0) + (pix4[0] * (1.0/127) - 1.0);
			normal[1] = (pix1[1] * (1.0/127) - 1.0) + (pix2[1] * (1.0/127) - 1.0) + (pix3[1] * (1.0/127) - 1.0) + (pix4[1] * (1.0/127) - 1.0);
			normal[2] = (pix1[2] * (1.0/127) - 1.0) + (pix2[2] * (1.0/127) - 1.0) + (pix3[2] * (1.0/127) - 1.0) + (pix4[2] * (1.0/127) - 1.0);

			if (!VectorNormalizeLength( normal )) VectorSet( normal, 0.0f, 0.0f, 1.0f );
			out[4*(y*width+x)+0] = (byte)(128 + 127 * normal[0]);
			out[4*(y*width+x)+1] = (byte)(128 + 127 * normal[1]);
			out[4*(y*width+x)+2] = (byte)(128 + 127 * normal[2]);
			out[4*(y*width+x)+3] = 255;
		}
	}
	Mem_Free( in );

	return out;
}

/*
=================
R_TextureFilter
=================
*/
void R_TextureFilter( void )
{
	texture_t	*texture;
	int	i;

	for( i = 0; i < NUM_TEXTURE_FILTERS; i++ )
	{
		if(!com.stricmp( r_textureFilters[i].name, r_texturefilter->string ))
			break;
	}

	if( i == NUM_TEXTURE_FILTERS )
	{
		Msg( "bad texture filter name\n" );

		Cvar_Set( "r_texturefilter", "GL_LINEAR_MIPMAP_LINEAR" );
		r_textureFilterMin = GL_LINEAR_MIPMAP_LINEAR;
		r_textureFilterMag = GL_LINEAR;
	}
	else
	{
		r_textureFilterMin = r_textureFilters[i].min;
		r_textureFilterMag = r_textureFilters[i].mag;
	}

	if( GL_Support( R_ANISOTROPY_EXT ))
	{
		if( r_texturefilteranisotropy->value > gl_config.max_anisotropy )
			Cvar_SetValue( "r_texture_filter_anisotropy", gl_config.max_anisotropy );
		else if( r_texturefilteranisotropy->value < 1.0 )
			Cvar_SetValue( "r_texture_filter_anisotropy", 1.0 );
	}

	// change all the existing texture objects
	for( i = 0; i < r_numTextures; i++ )
	{
		texture = &r_textures[i];
		GL_BindTexture( texture );

		if( texture->flags & TF_MIPMAPS )
		{
			pglTexParameterf( texture->target, GL_TEXTURE_MIN_FILTER, r_textureFilterMin );
			pglTexParameterf( texture->target, GL_TEXTURE_MAG_FILTER, r_textureFilterMag );

			if( GL_Support( R_ANISOTROPY_EXT ))
				pglTexParameterf( texture->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_texturefilteranisotropy->value );
		}
		else
		{
			pglTexParameterf( texture->target, GL_TEXTURE_MIN_FILTER, r_textureFilterMag );
			pglTexParameterf( texture->target, GL_TEXTURE_MAG_FILTER, r_textureFilterMag );
		}
	}
}

bool R_ImageHasMips( void )
{
	// will be generated later
	if( image_desc.tflags & TF_GEN_MIPMAPS )
		return true;
	if( image_desc.MipCount > 1 )
		return true;
	return false;
}

void GL_TexFilter( void )
{
	// set texture filter
	if( R_ImageHasMips( ))
	{
		pglTexParameterf( image_desc.glTarget, GL_TEXTURE_MIN_FILTER, r_textureFilterMin );
		pglTexParameterf( image_desc.glTarget, GL_TEXTURE_MAG_FILTER, r_textureFilterMag );

		if( GL_Support( R_ANISOTROPY_EXT ))
			pglTexParameterf( image_desc.glTarget, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_texturefilteranisotropy->value );
	}
	else
	{
		pglTexParameterf( image_desc.glTarget, GL_TEXTURE_MIN_FILTER, r_textureFilterMag );
		pglTexParameterf( image_desc.glTarget, GL_TEXTURE_MAG_FILTER, r_textureFilterMag );
	}

	// set texture wrap mode
	if( image_desc.tflags & TF_CLAMP )
	{
		if(GL_Support( R_CLAMPTOEDGE_EXT ))
		{
			pglTexParameterf( image_desc.glTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			pglTexParameterf( image_desc.glTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		}
		else
		{
			pglTexParameterf( image_desc.glTarget, GL_TEXTURE_WRAP_S, GL_CLAMP );
			pglTexParameterf( image_desc.glTarget, GL_TEXTURE_WRAP_T, GL_CLAMP );
		}
	}
	else
	{
		pglTexParameterf( image_desc.glTarget, GL_TEXTURE_WRAP_S, GL_REPEAT );
		pglTexParameterf( image_desc.glTarget, GL_TEXTURE_WRAP_T, GL_REPEAT );
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
	int	i, texels = 0;

	Msg( "\n" );
	Msg("      -w-- -h-- -fmt- -t-- -mm- wrap -name--------\n" );

	for( i = 0; i < r_numTextures; i++ )
	{
		texture = &r_textures[i];

		if( texture->target == GL_TEXTURE_2D )
			texels += (texture->width * texture->height);
		else texels += (texture->width * texture->height) * 6;

		Msg( "%4i: ", i );

		Msg( "%4i %4i ", texture->width, texture->height );
		Msg("%3s", PFDesc( texture->type )->name );

		switch( texture->target )
		{
		case GL_TEXTURE_2D:
			Msg(" 2D  ");
			break;
		case GL_TEXTURE_3D:
			Msg(" 3D  ");
			break;
		case GL_TEXTURE_CUBE_MAP_ARB:
			Msg(" CM  ");
			break;
		default:
			Msg(" ??  ");
			break;
		}

		if( texture->flags & TF_MIPMAPS )
			Msg(" yes ");
		else Msg(" no  ");

		if( texture->flags & TF_CLAMP )
			Msg( "clmp " );
		else Msg( "rept " );
		Msg( "%s\n", texture->name );
	}

	Msg( "------------------------------------------------------\n" );
	Msg( "%i total texels (not including mipmaps)\n", texels );
	Msg( "%i total textures\n", r_numTextures );
	Msg( "\n" );
}


/*
=============================================================

  TEXTURES UPLOAD

=============================================================
*/
static byte *r_imagepool;
bool use_gl_extension = false;

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

void R_RoundImageDimensions( int *scaled_width, int *scaled_height )
{
	int width, height;

	for( width = 1; width < *scaled_width; width <<= 1 );
	for( height = 1; height < *scaled_height; height <<= 1 );

	if( image_desc.flags & IMAGE_CUBEMAP )
	{
		*scaled_width = bound( 1, width, gl_config.max_cubemap_texture_size );
		*scaled_height = bound( 1, height, gl_config.max_cubemap_texture_size );
	}
	else
	{
		*scaled_width = bound( 1, width, gl_config.max_2d_texture_size );
		*scaled_height = bound( 1, height, gl_config.max_2d_texture_size );
	}
}

/*
====================
Image Decompress

read colors from dxt image
====================
*/
void R_DXTReadColors( const byte* data, color32* out )
{
	byte r0, g0, b0, r1, g1, b1;

	b0 = data[0] & 0x1F;
	g0 = ((data[0] & 0xE0) >> 5) | ((data[1] & 0x7) << 3);
	r0 = (data[1] & 0xF8) >> 3;

	b1 = data[2] & 0x1F;
	g1 = ((data[2] & 0xE0) >> 5) | ((data[3] & 0x7) << 3);
	r1 = (data[3] & 0xF8) >> 3;

	out[0].r = r0 << 3;
	out[0].g = g0 << 2;
	out[0].b = b0 << 3;

	out[1].r = r1 << 3;
	out[1].g = g1 << 2;
	out[1].b = b1 << 3;
}

/*
====================
Image Decompress

read one color from dxt image
====================
*/
void R_DXTReadColor( word data, color32* out )
{
	byte r, g, b;

	b = data & 0x1f;
	g = (data & 0x7E0) >>5;
	r = (data & 0xF800)>>11;

	out->r = r << 3;
	out->g = g << 2;
	out->b = b << 3;
}

/*
====================
R_GetBitsFromMask

====================
*/
void R_GetBitsFromMask( uint Mask, uint *ShiftLeft, uint *ShiftRight )
{
	uint Temp, i;

	if (Mask == 0)
	{
		*ShiftLeft = *ShiftRight = 0;
		return;
	}

	Temp = Mask;
	for (i = 0; i < 32; i++, Temp >>= 1)
	{
		if (Temp & 1) break;
	}
	*ShiftRight = i;

	// Temp is preserved, so use it again:
	for (i = 0; i < 8; i++, Temp >>= 1)
	{
		if (!(Temp & 1)) break;
	}
	*ShiftLeft = 8 - i;
	return;
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

	image_desc.glMask = PFDesc( image_desc.format )->glmask;
	image_desc.glType = PFDesc( image_desc.format )->gltype;

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
===============
R_GetPixelFormat

filled additional info
===============
*/
bool R_GetPixelFormat( rgbdata_t *pic, uint tex_flags, float bumpScale )
{
	int	w, h, d, i, s, BlockSize;
	size_t	mipsize, totalsize = 0;

	if( !pic || !pic->buffer ) return false;
	Mem_EmptyPool( r_imagepool ); // flush buffers		
	Mem_Set( &image_desc + MAX_STRING, 0, sizeof(image_desc) - MAX_STRING );	// FIXME

	BlockSize = PFDesc( pic->type )->block;
	image_desc.bpp = PFDesc( pic->type )->bpp;
	image_desc.bpc = PFDesc( pic->type )->bpc;
	image_desc.glMask = PFDesc( pic->type )->glmask;
	image_desc.glType = PFDesc( pic->type )->gltype;
	image_desc.format = pic->type;

	image_desc.numLayers = d = pic->numLayers;
	image_desc.width = w = pic->width;
	image_desc.height = h = pic->height;
	image_desc.flags = pic->flags;
	image_desc.tflags = tex_flags;

	image_desc.bps = image_desc.width * image_desc.bpp * image_desc.bpc;
	image_desc.SizeOfPlane = image_desc.bps * image_desc.height;
	image_desc.SizeOfData = image_desc.SizeOfPlane * image_desc.numLayers;
	image_desc.BitsCount = pic->bitsCount;
	image_desc.bumpScale = bumpScale;

	// now correct buffer size
	for( i = 0; i < pic->numMips; i++, totalsize += mipsize )
	{
		mipsize = R_GetImageSize( BlockSize, w, h, d, image_desc.bpp, image_desc.BitsCount / 8 );
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1;
	}

	if( tex_flags & ( TF_IMAGE2D|TF_SKYSIDE ))
	{
		// don't build mips for sky and hud pics
		image_desc.tflags &= ~TF_GEN_MIPMAPS;
		image_desc.MipCount = 1; // and ignore it to load
	} 
	else if( pic->numMips > 1 )
	{
		// .dds, .wal or .mip image
		image_desc.tflags &= ~TF_GEN_MIPMAPS;
		image_desc.MipCount = pic->numMips;
		image_desc.tflags |= TF_MIPMAPS;
	}
	else
	{
		// so it normal texture without mips
		image_desc.tflags |= (TF_MIPMAPS|TF_GEN_MIPMAPS);
		image_desc.MipCount = pic->numMips;
	}
		
	if( image_desc.MipCount < 1 ) image_desc.MipCount = 1;
	image_desc.pal = pic->palette;

	// check for permanent images
	if( image_desc.format == PF_RGBA_GN ) image_desc.tflags |= TF_STATIC;
	if( tex_flags & TF_IMAGE2D ) image_desc.tflags |= TF_STATIC;

	// restore temp dimensions
	w = image_desc.width;
	h = image_desc.height;
	s = w * h;

	// can use gl extension ?
	R_RoundImageDimensions( &w, &h );

	if( w == image_desc.width && h == image_desc.height ) 
		use_gl_extension = true;
	else use_gl_extension = false;

	image_desc.source = Mem_Alloc( r_imagepool, s * 4 );	// source buffer
	image_desc.scaled = Mem_Alloc( r_imagepool, w * h * 4 );	// scaled buffer

	if( image_desc.flags & IMAGE_CUBEMAP )
	{
		totalsize *= 6;
		image_desc.glTarget = GL_TEXTURE_CUBE_MAP_ARB;
	}
	else image_desc.glTarget = GL_TEXTURE_2D;

	if( totalsize != pic->size ) // sanity check
	{
		MsgDev( D_ERROR, "R_GetPixelFormat: invalid image size (%i should be %i)\n", pic->size, totalsize );
		return false;
	}
	if( s&3 )
	{
		// will be resample, just tell me for debug targets
		MsgDev( D_NOTE, "R_GetPixelFormat: s&3 [%d x %d]\n", image_desc.width, image_desc.height );
	}	
	return true;
}

/*
=================
R_IntensityScaleTexture
=================
*/
static void R_IntensityScaleTexture( uint *in, int width, int height )
{
	int	i, c;
	byte	*out = (byte *)in;

	if( r_intensity->value == 1.0 ) return;
	c = width * height;

	for( i = 0; i < c; i++, in += 4, out += 4 )
	{
		out[0] = r_intensityTable[in[0]];
		out[1] = r_intensityTable[in[1]];
		out[2] = r_intensityTable[in[2]];
	}
}

/*
 =================
 R_HeightToNormal

 Assumes the input is a grayscale image converted to RGBA
 =================
*/
static uint *R_HeightToNormal( uint *src, int width, int height, float bumpScale )
{
	int	i, j;
	vec3_t	normal;
	float	invLength;
	float	c, cx, cy;
	byte	*out, *in = (byte *)src;
	
	out = Mem_Alloc( r_imagepool, width * height * 4 );

	for( i = 0; i < height; i++ )
	{
		for( j = 0; j < width; j++ )
		{
			c = in[4*(i*width+j)] * (1.0/255);
			cx = in[4*(i*width+(j+1)%width)] * (1.0/255);
			cy = in[4*(((i+1)%height)*width+j)] * (1.0/255);
			cx = (c - cx) * bumpScale;
			cy = (c - cy) * bumpScale;
			invLength = 1.0 / sqrt(cx*cx + cy*cy + 1.0);
			VectorSet( normal, cx * invLength, -cy * invLength, invLength );

			out[4*(i*width+j)+0] = (byte)(127.5 * (normal[0] + 1.0));
			out[4*(i*width+j)+1] = (byte)(127.5 * (normal[1] + 1.0));
			out[4*(i*width+j)+2] = (byte)(127.5 * (normal[2] + 1.0));
			out[4*(i*width+j)+3] = in[4*(i*width+j)+3];
		}
	}

	Mem_Free( in );
	return (uint *)out;
}

/*
===============
R_ShutdownTextures
===============
*/
void R_ShutdownTextures( void )
{
	texture_t	*texture;
	int	i;

	if( gl_config.texRectangle )
		pglDeleteTextures( 1, &gl_state.screenTexture );

	for( i = 0; i < r_numTextures; i++ )
	{
		texture = &r_textures[i];
		pglDeleteTextures( 1, &texture->texnum );
	}
	Mem_Set( r_textures, 0, sizeof( r_textures ));
	r_numTextures = 0;
}

/*
 =================
 R_CreateBuiltInTextures
 =================
*/
static void R_CreateBuiltInTextures( void )
{
	byte	data2D[256*256*4];
	rgbdata_t r_generic;
	vec3_t	normal;
	int	i, x, y;
	float	s, t;

	// FIXME: too many hardcoded values in this function

	// default texture
	Mem_Set( &r_generic, 0, sizeof( r_generic ));
	for( i = x = 0; x < 16; x++ )
	{
		for( y = 0; y < 16; y++ )
		{
			if( x == 0 || x == 15 || y == 0 || y == 15 )
				((uint *)&data2D)[i++] = LittleLong( 0xffffffff );
			else ((uint *)&data2D)[i++] = LittleLong( 0xff000000 );
		}
	}

	r_generic.width = 16;
	r_generic.height = 16;
	r_generic.type = PF_RGBA_GN; // generated
	r_generic.size = r_generic.width * r_generic.height * 4;
	r_generic.numMips = 1;
	r_generic.buffer = (byte *)data2D;
	r_defaultTexture = R_LoadTexture( "*default", &r_generic, TF_GEN_MIPMAPS, 0 );

	// white texture
	for( i = 0; i < 64; i++ ) ((uint *)&data2D)[i] = LittleLong(0xffffffff);
	r_generic.width = 8;
	r_generic.height = 8;
	r_generic.size = r_generic.width * r_generic.height * 4;
	r_generic.flags = 0;
	r_whiteTexture = R_LoadTexture( "*white", &r_generic, 0, 0 );

	// Black texture
	for( i = 0; i < 64; i++ ) ((uint *)&data2D)[i] = LittleLong(0xff000000);
	r_blackTexture = R_LoadTexture( "*black", &r_generic, 0, 0 );

	// raw texture
	Mem_Set( data2D, 255, 256*256*4 );
	r_generic.width = 256;
	r_generic.height = 256;
	r_generic.size = r_generic.width * r_generic.height * 4;
	r_rawTexture = R_LoadTexture( "*raw", &r_generic, 0, 0 );

	// dynamic light texture
	Mem_Set( data2D, 255, 128*128*4 );
	r_generic.width = 128;
	r_generic.height = 128;
	r_generic.size = r_generic.width * r_generic.height * 4;
	r_dlightTexture = R_LoadTexture( "*dlight", &r_generic, TF_CLAMP, 0 );

	if( GL_Support( R_TEXTURECUBEMAP_EXT ))
	{
		byte	data3D[128*128*4*6]; // full cubemap size
		byte	*dataCM = (byte *)data3D;

		// normalize texture
		for( i = 0; i < 6; i++ )
		{
			for( y = 0; y < 128; y++ )
			{
				for( x = 0; x < 128; x++ )
				{
					s = (((float)x + 0.5) / 128.0) * 2.0 - 1.0;
					t = (((float)y + 0.5) / 128.0) * 2.0 - 1.0;

					switch( i )
					{
					case 0:
						VectorSet( normal, 1.0, -t, -s );
						break;
					case 1:
						VectorSet( normal, -1.0, -t, s );
						break;
					case 2:
						VectorSet( normal, s, 1.0, t );
						break;
					case 3:
						VectorSet( normal, s, -1.0, -t );
						break;
					case 4:
						VectorSet( normal, s, -t, 1.0 );
						break;
					case 5:
						VectorSet( normal, -s, -t, -1.0 );
						break;
					}

					VectorNormalize( normal );
					dataCM[4*(y*128+x)+0] = (byte)(127.5 * (normal[0] + 1.0));
					dataCM[4*(y*128+x)+1] = (byte)(127.5 * (normal[1] + 1.0));
					dataCM[4*(y*128+x)+2] = (byte)(127.5 * (normal[2] + 1.0));
					dataCM[4*(y*128+x)+3] = 255;
				}
			}
			dataCM += (128*128*4); // move pointer
		}

		r_generic.width = 128;
		r_generic.height = 128;
		r_generic.size = (r_generic.width * r_generic.height * 4) * 6;
		r_generic.flags = IMAGE_CUBEMAP; // yes it's cubemap
		r_generic.buffer = (byte *)data3D;
		r_normalizeTexture = R_LoadTexture( "*normalize", &r_generic, TF_CLAMP|TF_CUBEMAP, 0 );
	}

	// screen rect texture (just reserve a slot)
	if( gl_config.texRectangle ) pglGenTextures( 1, &gl_state.screenTexture );
}

/*
===============
R_InitTextures
===============
*/
void R_InitTextures( void )
{
	int	i, j;
	float	f;

	r_imagepool = Mem_AllocPool( "Texture Pool" );	// for scaling and resampling
	pglGetIntegerv( GL_MAX_TEXTURE_SIZE, &gl_config.max_2d_texture_size );

	r_numTextures = 0;
	registration_sequence = 1;
	Mem_Set( &r_textures, 0, sizeof( r_textures ));

	// init intensity conversions
	r_intensity = Cvar_Get( "r_intensity", "2", 0, "gamma intensity value" );
	if( r_intensity->value <= 1 ) Cvar_SetValue( "r_intensity", 1 );

	for (i = 0; i < 256; i++)
	{
		j = i * r_intensity->value;
		r_intensityTable[i] = bound( 0, j, 255 );
	}

	// build luminance table
	for( i = 0; i < 256; i++ )
	{
		f = (float)i;
		r_luminanceTable[i][0] = f * 0.299;	// red weight
		r_luminanceTable[i][1] = f * 0.587;	// green weight
		r_luminanceTable[i][2] = f * 0.114;	// blue weight
	}

	// create built-in textures
	R_CreateBuiltInTextures();
}

bool R_ResampleTexture( uint *in, int inwidth, int inheight, uint *out, int outwidth, int outheight )
{
	int	i, j;
	uint	frac, fracstep;
	uint	*inrow, *inrow2;
	uint	p1[4096], p2[4096];
	byte	*pix1, *pix2, *pix3, *pix4;

	// check for buffers
	if( !in || !out || in == out ) return false;
	if( outheight == 0 || outwidth == 0 ) return false;

	// apply intensity if needed
	if((image_desc.tflags & TF_MIPMAPS) && !(image_desc.tflags & TF_NORMALMAP))
			R_IntensityScaleTexture( in, inwidth, inheight );
	
	if( image_desc.tflags & TF_LUMA )
	{
		// apply the double-squared luminescent version
		for( i = 0, pix1 = (byte *)in; i < inwidth * inheight; i++, pix1 += 4 ) 
		{
			pix1[0] = r_luminanceTable[pix1[0]][0];
			pix1[1] = r_luminanceTable[pix1[1]][1];
			pix1[2] = r_luminanceTable[pix1[2]][2];
		}
	}

	if( image_desc.tflags & TF_HEIGHTMAP )
		in = R_HeightToNormal( in, inwidth, inheight, image_desc.bumpScale );

	// nothing to resample ?
	if( inwidth == outwidth && inheight == outheight)
	{
		Mem_Copy( out, in, inheight * inwidth * 4 );
		return false;
	}

	fracstep = inwidth * 0x10000 / outwidth;
	frac = fracstep>>2;

	for( i = 0; i < outwidth; i++)
	{
		p1[i] = 4 * (frac>>16);
		frac += fracstep;
	}
	frac = 3 * (fracstep>>2);

	for( i = 0; i < outwidth; i++)
	{
		p2[i] = 4 * (frac>>16);
		frac += fracstep;
	}

	for (i = 0; i < outheight; i++, out += outwidth)
	{
		inrow = in + inwidth * (int)((i + 0.25) * inheight / outheight);
		inrow2 = in + inwidth * (int)((i + 0.75) * inheight / outheight);
		frac = fracstep>>1;

		for (j = 0; j < outwidth; j++)
		{
			pix1 = (byte *)inrow + p1[j];
			pix2 = (byte *)inrow + p2[j];
			pix3 = (byte *)inrow2 + p1[j];
			pix4 = (byte *)inrow2 + p2[j];
			((byte *)(out+j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			((byte *)(out+j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			((byte *)(out+j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			((byte *)(out+j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}
	return true;
}

void R_ImageMipmap( byte *in, int width, int height )
{
	int	i, j;
	byte	*out;

	width <<=  2;
	height >>= 1;
	out = in;
	for( i = 0; i < height; i++, in += width )
	{
		for( j = 0; j < width; j += 8, out += 4, in += 8 )
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}

/*
===============
pglGenerateMipmaps

sgis generate mipmap
===============
*/
void GL_GenerateMipmaps( int width, int height )
{
	int miplevel = 0;

	if(!( image_desc.tflags & TF_GEN_MIPMAPS )) return;

	if( GL_Support( R_SGIS_MIPMAPS_EXT ))
	{
		pglHint( GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST );
		pglTexParameteri( image_desc.glTarget, GL_GENERATE_MIPMAP_SGIS, GL_TRUE );
		if(pglGetError()) MsgDev(D_WARN, "GL_GenerateMipmaps: can't create mip levels\n");
		else return; // falltrough to software mipmap generating
	}

	if( use_gl_extension )
	{
		// g-cont. because i'm don't know how to generate miplevels for GL_FLOAT or GL_SHORT_REV_1_bla_bla
		// ok, please show me videocard which don't supported GL_GENERATE_MIPMAP_SGIS ...
		MsgDev( D_ERROR, "GL_GenerateMipmaps: software mip generator failed on %s\n", PFDesc( image_desc.format )->name );
		return;
	}

	// software mipmap generator
	while( width > 1 || height > 1 )
	{
		R_ImageMipmap( image_desc.scaled, width, height );
		width >>= 1;
		height >>= 1;
		if( width < 1) width = 1;
		if( height < 1) height = 1;
		miplevel++;
		pglTexImage2D( image_desc.glTarget, miplevel, image_desc.glSamples, width, height, 0, image_desc.glMask, image_desc.glType, image_desc.scaled );
	}
}

bool R_LoadImageDXT( GLuint target, GLint level, GLint intformat, GLuint width, GLuint height, GLint border, GLuint imageSize, const GLvoid* data )
{
	GLuint dxtformat;

	switch( intformat )
	{
	case PF_DXT1: dxtformat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
	case PF_DXT3: dxtformat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
	case PF_DXT5: dxtformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
	default: return false;
	}

	if( !level ) GL_GenerateMipmaps( width, height ); // generate mips if needed
	pglCompressedTexImage2DARB( target, level, dxtformat, width, height, border, imageSize, data );
	return !pglGetError();
}

/*
===============
qrsDecompressedTexImage2D

cpu version of loading non paletted rgba buffer
===============
*/
bool R_LoadImageRGBA( uint target, int level, int intformat, uint width, uint height, int border, uint imageSize, const void* data )
{
	byte	*fin;
	int	i, p; 
	int	scaled_width, scaled_height;
	uint	*scaled = (uint *)image_desc.scaled;
	byte	*fout = image_desc.source;
	bool	has_alpha = false;

	if (!data) return false;
	fin = (byte *)data;
	scaled_width = width;
	scaled_height = height;

	switch( intformat )
	{
	case PF_INDEXED_24:
		if( image_desc.flags & IMAGE_HAVE_ALPHA )
		{
			// studio model indexed texture probably with alphachannel
			for (i = 0; i < width * height; i++)
			{
				p = fin[i];
				if( p == 255 ) has_alpha = true;
				fout[(i<<2)+0] = image_desc.pal[p*3+0];
				fout[(i<<2)+1] = image_desc.pal[p*3+1];
				fout[(i<<2)+2] = image_desc.pal[p*3+2];
				fout[(i<<2)+3] = (p == 255) ? 0 : 255;
			}
		}
		else
		{
			// studio model indexed texture without alphachannel
			for( i = 0; i < width * height; i++ )
			{
				p = fin[i];
				fout[(i<<2)+0] = image_desc.pal[p*3+0];
				fout[(i<<2)+1] = image_desc.pal[p*3+1];
				fout[(i<<2)+2] = image_desc.pal[p*3+2];
				fout[(i<<2)+3] = 255;
			}
		}
		if( !has_alpha ) image_desc.flags &= ~IMAGE_HAVE_ALPHA;
		break;
	case PF_INDEXED_32:
		// sprite indexed frame with alphachannel
		for( i = 0; i < width*height; i++ )
		{
			fout[(i<<2)+0] = image_desc.pal[fin[i]*4+0];
			fout[(i<<2)+1] = image_desc.pal[fin[i]*4+1];
			fout[(i<<2)+2] = image_desc.pal[fin[i]*4+2];
			fout[(i<<2)+3] = image_desc.pal[fin[i]*4+3];
		}
		break;
	case PF_RGB_24:
		// 24-bit image, that will not expanded to RGBA in imglib.dll for some reasons
		for (i = 0; i < width * height; i++ )
		{
			fout[(i<<2)+0] = fin[i+0];
			fout[(i<<2)+1] = fin[i+1];
			fout[(i<<2)+2] = fin[i+2];
			fout[(i<<2)+3] = 255;
		}
		break;
	case PF_ABGR_64:
	case PF_RGBA_32:
	case PF_RGBA_GN:
		fout = fin; // nothing to process
		break;
	default:
		MsgDev(D_WARN, "qrsDecompressedTexImage2D: invalid compression type: %s\n", PFDesc( intformat )->name );
		return false;
	}

	R_RoundImageDimensions( &scaled_width, &scaled_height );
	if( image_desc.tflags & TF_COMPRESS )
		image_desc.glSamples = (image_desc.flags & IMAGE_HAVE_ALPHA) ? GL_COMPRESSED_RGBA_ARB : GL_COMPRESSED_RGB_ARB;
	else image_desc.glSamples = (image_desc.flags & IMAGE_HAVE_ALPHA) ? GL_RGBA : GL_RGB;
	R_ResampleTexture((uint *)fout, width, height, scaled, scaled_width, scaled_height);
	if( !level ) GL_GenerateMipmaps( scaled_width, scaled_height ); // generate mips if needed
	pglTexImage2D( target, level, image_desc.glSamples, scaled_width, scaled_height, border, image_desc.glMask, image_desc.glType, (byte *)scaled );

	return !pglGetError();
}

bool R_LoadImageARGB( uint target, int level, int intformat, uint width, uint height, int border, uint imageSize, const void* data )
{
	uint argbformat, datatype;

	switch( intformat )
	{
	case PF_ARGB_32:
	case PF_LUMINANCE:
	case PF_LUMINANCE_16:
	case PF_LUMINANCE_ALPHA:
		argbformat = GL_RGB5_A1;
		datatype = GL_UNSIGNED_SHORT_1_5_5_5_REV;
		break;
	default: return false;
	}

	if( !level ) GL_GenerateMipmaps( width, height ); // generate mips if needed
	pglTexImage2D( target, level, argbformat, width, height, border, image_desc.glMask, datatype, data );
	return !pglGetError();
}

bool R_LoadImageFloat( uint target, int level, int intformat, uint width, uint height, int border, uint imageSize, const void* data )
{
	uint floatformat, datatype;

	switch( intformat )
	{
	case PF_R_16F:
	case PF_R_32F:
	case PF_GR_32F:
	case PF_GR_64F:
	case PF_ABGR_64F:
	case PF_ABGR_128F:
		floatformat = GL_FLOAT;
		datatype = GL_UNSIGNED_SHORT_1_5_5_5_REV;
		break;
	default: return false;
	}

	if( !level ) GL_GenerateMipmaps( width, height ); // generate mips if needed // generate mips if needed
	pglTexImage2D( target, level, floatformat, width, height, border, image_desc.glMask, datatype, data );
	return !pglGetError();
}

/*
===============
R_FindImage

Finds or loads the given image
===============
*/
texture_t	*R_FindTexture( const char *name, const byte *buffer, size_t size, uint flags, float bumpScale )
{
	texture_t		*image;
	rgbdata_t		*pic = NULL;
	int		i;
          
	if( !name ) return r_defaultTexture;
          
	// look for it
	for( i = 0; i < r_numTextures; i++ )
	{
		image = &r_textures[i];
		if( !com.stricmp( name, image->name ))
		{
			// prolonge registration
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	pic = FS_LoadImage( name, buffer, size ); // loading form disk or buffer
	if( pic )
	{
		image = R_LoadTexture( name, pic, flags, bumpScale ); // upload into video buffer
		FS_FreeImage( pic ); //free image
	}
	else image = r_defaultTexture;

	return image;
}

texture_t *R_FindCubeMapTexture( const char *name, uint flags, float bumpScale )
{
	return R_FindTexture( name, NULL, 0, flags|TF_CUBEMAP, bumpScale );
}

bool R_UploadTexture( byte *buffer, int type, GLuint target )
{
	int	i, size = 0;
	int	w = image_desc.width;
	int	h = image_desc.height;
	int	d = image_desc.numLayers;

	switch( type )
	{
	case PF_RGB_24:
	case PF_RGBA_32: 
	case PF_ABGR_64:
	case PF_RGBA_GN: image_desc.texImage = R_LoadImageRGBA; break;
	case PF_LUMINANCE:
	case PF_LUMINANCE_16:
	case PF_LUMINANCE_ALPHA:
	case PF_ARGB_32: image_desc.texImage = R_LoadImageARGB; break;
	case PF_DXT1:
	case PF_DXT3:
	case PF_DXT5: image_desc.texImage = R_LoadImageDXT; break;
	case PF_R_16F:
	case PF_R_32F:
	case PF_GR_32F:
	case PF_GR_64F:
	case PF_ABGR_64F:
	case PF_ABGR_128F: image_desc.texImage = R_LoadImageFloat; break;
	default: return false;
	}

	for( i = 0; i < image_desc.MipCount; i++, buffer += size )
	{
		R_SetPixelFormat( w, h, d );
		size = image_desc.SizeOfFile;

		if( !image_desc.texImage( target, i, image_desc.format, w, h, 0, size, buffer ))
			return false; // there were errors 
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1; // calc size of next mip
	}

	GL_TexFilter();
	return true;
}

/*
================
R_LoadTexture

This is also used as an entry point for the generated r_notexture
================
*/
texture_t	*R_LoadTexture( const char *name, rgbdata_t *pic, uint flags, float bumpScale )
{
	texture_t	*image;
          bool	iResult = true;
	int	i, numsides = 1, width, height;
	uint	offset = 0, target = GL_TEXTURE_2D;
	byte	*buf;

	// find a free texture_t
	for( i = 0; i < r_numTextures; i++ )
	{
		image = &r_textures[i];
	}
	if( i == r_numTextures )
	{
		if( r_numTextures == MAX_TEXTURES )
		{
			MsgDev(D_ERROR, "R_LoadTexture: r_textures limit is out\n");
			return r_defaultTexture;
		}
	}

	image = &r_textures[r_numTextures++];
	if( com.strlen( name ) >= sizeof(image->name)) MsgDev( D_WARN, "R_LoadTexture: \"%s\" is too long", name);

	// nothing to load
	if( !pic || !pic->buffer )
	{
		// create notexture with another name
		MsgDev( D_WARN, "R_LoadTexture: can't loading %s\n", name ); 
		Mem_Copy( image, r_defaultTexture, sizeof( texture_t ));
		com.strncpy( image->name, name, sizeof( image->name ));
		image->registration_sequence = registration_sequence;
		return image;
	}

	com.strncpy( image->name, name, sizeof( image->name ));
	com.strncpy( image_desc.name, name, sizeof( image_desc.name ));
	image->registration_sequence = registration_sequence;

	if( flags & TF_CUBEMAP )
	{
		if( pic->flags & IMAGE_CUBEMAP )
		{
			numsides = 6;
			target = GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB;
		}
		else
		{
			MsgDev( D_WARN, "texture %s it's not a cubemap image\n", name );
			flags &= ~TF_CUBEMAP;
		}
	}

	image->width = width = pic->width;
	image->height = height = pic->height;
	image->bumpScale = bumpScale;
	buf = pic->buffer;

	// fill image_desc
	R_GetPixelFormat( pic, flags, bumpScale );

	pglGenTextures( 1, &image->texnum );
	image->target = image_desc.glTarget;
	image->flags = image_desc.tflags;	// merged by R_GetPixelFormat
	image->type = image_desc.format;

	for( i = 0; i < numsides; i++, buf += offset )
	{
		GL_BindTexture( image );

		R_SetPixelFormat( image_desc.width, image_desc.height, image_desc.numLayers );
		offset = image_desc.SizeOfFile; // move pointer

		if( numsides == 6 )
			MsgDev( D_LOAD, "%s[%i] [%s] \n", name, i, PFDesc( image_desc.format )->name );
		else MsgDev( D_LOAD, "%s [%s] \n", name, PFDesc( image_desc.format )->name );
		iResult = R_UploadTexture( buf, pic->type, target + i );
	}          
	// check for errors
	if( !iResult )
	{
		MsgDev( D_ERROR, "R_LoadTexture: can't loading %s with bpp %d\n", name, image_desc.bpp ); 
		return r_defaultTexture;
	} 
	return image;
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
	texture_t		*image;
	int		i;

	Msg( "Totaling DDS Time: %.8f\n", totalTime );

	for( i = 0, image = r_textures; i < r_numTextures; i++, image++ )
	{
		// used this sequence
		if( image->registration_sequence == registration_sequence ) continue;
		if( image->flags & TF_STATIC || !image->name[0] ) // static or already freed
			continue;
		Msg("release texture %s\n", image->name );
		pglDeleteTextures( 1, &image->texnum );
		Mem_Set( image, 0, sizeof( *image ));
	}
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
	int	i = 1;

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

	for( i = 0; i < 6; i++ )
	{
		string		name;
		rgbdata_t		*r_shot;
	
		if( skyshot )
		{
			com.snprintf( name, sizeof(name), "env/%s%s.tga", base, r_skyBoxSuffix[i] );
			VectorCopy( r_skyBoxAngles[i], r_refdef.viewangles );
		}
		else
		{
			com.snprintf(name, sizeof(name), "env/%s_%s.tga", base, r_cubeMapSuffix[i]);
			VectorCopy( r_cubeMapAngles[i], r_refdef.viewangles );
		}

		R_RenderView( &r_refdef );
		pglReadPixels( 0, r_height->integer - size, size, size, GL_RGB, GL_UNSIGNED_BYTE, r_framebuffer );

		r_shot = Mem_Alloc( r_imagepool, sizeof( rgbdata_t ));
		r_shot->width = size;
		r_shot->height = size;
		r_shot->type = PF_RGB_24;
		r_shot->size = r_shot->width * r_shot->height * 3;
		r_shot->palette = NULL;
		r_shot->numLayers = 1;
		r_shot->numMips = 1;
		r_shot->buffer = r_framebuffer;

		// write image
		FS_SaveImage( name, r_shot );
		Mem_Free( r_shot ); // don't touch framebuffer!
	}
	return true;
}