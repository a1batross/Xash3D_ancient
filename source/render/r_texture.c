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

static texture_t *r_textures[MAX_TEXTURES];
static int r_numTextures;

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

	uint		tflags;	// TF_ flags	
	uint		flags;	// IMAGE_ flags
	byte		*pal;
	byte		*source;
	byte		*scaled;
} PixFormatDesc;

static PixFormatDesc image_desc;
static int r_textureFilterMin = GL_LINEAR_MIPMAP_LINEAR;
static int r_textureFilterMag = GL_LINEAR;
static byte r_intensityTable[256];
static byte r_lumaTable[256];
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
		texture = r_textures[i];

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
	if( image_desc.flags & IMAGE_GEN_MIPS )
		return true;
	if( image_desc.MipCount > 1)
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
		texture = r_textures[i];

		if( texture->target == GL_TEXTURE_2D )
			texels += (texture->width * texture->height);
		else texels += (texture->width * texture->height) * 6;

		Msg( "%4i: ", i );

		Msg( "%4i %4i ", texture->width, texture->height );
		Msg("%3s", PFDesc[texture->type].name );

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
	
	BlockSize = PFDesc[image_desc.format].block;
	image_desc.bpp = PFDesc[image_desc.format].bpp;
	image_desc.bpc = PFDesc[image_desc.format].bpc;

	image_desc.glMask = PFDesc[image_desc.format].glmask;
	image_desc.glType = PFDesc[image_desc.format].gltype;

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
	memset( &image_desc, 0, sizeof(image_desc));
	for(i = 0; i < PF_TOTALCOUNT; i++)
	{
		if(pic->type == PFDesc[i].format)
		{
			BlockSize = PFDesc[i].block;
			image_desc.bpp = PFDesc[i].bpp;
			image_desc.bpc = PFDesc[i].bpc;
			image_desc.glMask = PFDesc[i].glmask;
			image_desc.glType = PFDesc[i].gltype;
			image_desc.format = pic->type;
			break;
		} 
	} 		

	if( i != PF_TOTALCOUNT ) // make sure what match found
	{
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

		if( tex_flags & ( TF_IMAGE2D|TF_SKYSIDE|TF_SKYSIDE_FLIP ))
		{
			// don't build mips for sky and hud pics
			image_desc.flags &= ~IMAGE_GEN_MIPS;
			image_desc.MipCount = 1; // and ignore it to load
		} 
		else if( pic->numMips > 1 )
		{
			// .dds, .wal or .mip image
			image_desc.flags &= ~IMAGE_GEN_MIPS;
			image_desc.MipCount = pic->numMips;
			image_desc.tflags |= TF_MIPMAPS;
		}
		else
		{
			// so it normal texture without mips
			image_desc.tflags |= TF_MIPMAPS;
			image_desc.flags |= IMAGE_GEN_MIPS;
			image_desc.MipCount = pic->numMips;
		}
		
		if( image_desc.MipCount < 1 ) image_desc.MipCount = 1;
		image_desc.pal = pic->palette;
	}	

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
		texture = r_textures[i];
		pglDeleteTextures( 1, &texture->texnum );
	}
	memset( r_textures, 0, sizeof( r_textures ));
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
	byte	*data3D, *dataCM;
	rgbdata_t r_generic;
	vec3_t	normal;
	int	i, x, y;
	float	s, t;

	// FIXME: too many hardcoded values in this function

	// default texture
	memset( &r_generic, 0, sizeof( r_generic ));
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
	r_generic.flags |= IMAGE_GEN_MIPS;
	r_defaultTexture = R_LoadTexture( "*default", &r_generic, 0, 0 );

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
	memset( data2D, 255, 256*256*4 );
	r_generic.width = 256;
	r_generic.height = 256;
	r_generic.size = r_generic.width * r_generic.height * 4;
	r_rawTexture = R_LoadTexture( "*raw", &r_generic, 0, 0 );

	// dynamic light texture
	memset( data2D, 255, 128*128*4 );
	r_generic.width = 128;
	r_generic.height = 128;
	r_generic.size = r_generic.width * r_generic.height * 4;
	r_dlightTexture = R_LoadTexture( "*dlight", &r_generic, TF_CLAMP, 0 );

	if( GL_Support( R_TEXTURECUBEMAP_EXT ))
	{
		data3D = dataCM = Mem_Alloc( r_imagepool, (128*128*4) * 6 );

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
		Mem_Free( data3D );
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

	registration_sequence = 1;

	// init intensity conversions
	r_intensity = Cvar_Get( "r_intensity", "2", 0, "gamma intensity value" );
	if( r_intensity->value <= 1 ) Cvar_SetValue( "r_intensity", 1 );

	for (i = 0; i < 256; i++)
	{
		j = i * r_intensity->value;
		r_intensityTable[i] = bound( 0, j, 255 );
	}

	// make a luma table by squaring the intensity twice
	for( i = 0; i < 256; i++ )
	{
		f = ( float )i/255.0f;

		f *= f;
		f *= 2;
		f *= f;
		f *= 2;
		r_lumaTable[i] = ( byte )(bound(0,f,1) * 255.0f);
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
			pix1[0] = r_lumaTable[pix1[0]];
			pix1[1] = r_lumaTable[pix1[1]];
			pix1[2] = r_lumaTable[pix1[2]];
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

	if(!( image_desc.flags & IMAGE_GEN_MIPS )) return;

	if( GL_Support( R_SGIS_MIPMAPS_EXT ))
	{
		pglHint( GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST );
		pglTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE );
		if(pglGetError()) MsgDev(D_WARN, "GL_GenerateMipmaps: can't create mip levels\n");
		else return; // falltrough to software mipmap generating
	}

	if( use_gl_extension )
	{
		// g-cont. because i'm don't know how to generate miplevels for GL_FLOAT or GL_SHORT_REV_1_bla_bla
		// ok, please show me videocard which don't supported GL_GENERATE_MIPMAP_SGIS ...
		MsgDev( D_ERROR, "GL_GenerateMipmaps: software mip generator failed on %s\n", PFDesc[image_desc.format].name );
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

/*
===============
qrsCompressedTexImage2D

cpu version of decompress dds
===============
*/
bool qrsCompressedTexImage2D( uint target, int level, int internalformat, uint width, uint height, int border, uint imageSize, const void* data )
{
	color32	colours[4], *col;
	uint	bits, bitmask, Offset; 
	int	scaled_width, scaled_height;
	word	sAlpha, sColor0, sColor1;
	byte	*fin, *fout = image_desc.source;
	byte	alphas[8], *alpha, *alphamask; 
	int	w, h, x, y, z, i, j, k, Select; 
	uint	*scaled = (uint *)image_desc.scaled;
	bool	has_alpha = false;

	if (!data) return false;
	fin = (byte *)data;
	w = width;
	h = height;
	
	switch( internalformat )
	{
	case PF_DXT1:
		colours[0].a = 0xFF;
		colours[1].a = 0xFF;
		colours[2].a = 0xFF;

		for (z = 0; z < image_desc.numLayers; z++)
		{
			for (y = 0; y < h; y += 4)
			{
				for (x = 0; x < w; x += 4)
				{
					sColor0 = *((word*)fin);
					sColor0 = LittleShort(sColor0);
					sColor1 = *((word*)(fin + 2));
					sColor1 = LittleShort(sColor1);

					R_DXTReadColor(sColor0, colours);
					R_DXTReadColor(sColor1, colours + 1);

					bitmask = ((uint*)fin)[1];
					bitmask = LittleLong( bitmask );
					fin += 8;

					if (sColor0 > sColor1)
					{
						// Four-color block: derive the other two colors.
						// 00 = color_0, 01 = color_1, 10 = color_2, 11 = color_3
						// These 2-bit codes correspond to the 2-bit fields 
						// stored in the 64-bit block.
						colours[2].b = (2 * colours[0].b + colours[1].b + 1) / 3;
						colours[2].g = (2 * colours[0].g + colours[1].g + 1) / 3;
						colours[2].r = (2 * colours[0].r + colours[1].r + 1) / 3;
						colours[3].b = (colours[0].b + 2 * colours[1].b + 1) / 3;
						colours[3].g = (colours[0].g + 2 * colours[1].g + 1) / 3;
						colours[3].r = (colours[0].r + 2 * colours[1].r + 1) / 3;
						colours[3].a = 0xFF;
					}
					else
					{ 
						// Three-color block: derive the other color.
						// 00 = color_0,  01 = color_1,  10 = color_2,
						// 11 = transparent.
						// These 2-bit codes correspond to the 2-bit fields 
						// stored in the 64-bit block. 
						colours[2].b = (colours[0].b + colours[1].b) / 2;
						colours[2].g = (colours[0].g + colours[1].g) / 2;
						colours[2].r = (colours[0].r + colours[1].r) / 2;
						colours[3].b = (colours[0].b + 2 * colours[1].b + 1) / 3;
						colours[3].g = (colours[0].g + 2 * colours[1].g + 1) / 3;
						colours[3].r = (colours[0].r + 2 * colours[1].r + 1) / 3;
						colours[3].a = 0x00;
					}
					for (j = 0, k = 0; j < 4; j++)
					{
						for (i = 0; i < 4; i++, k++)
						{
							Select = (bitmask & (0x03 << k*2)) >> k*2;
							col = &colours[Select];

							if (((x + i) < w) && ((y + j) < h))
							{
								uint ofs = z * image_desc.SizeOfPlane + (y + j) * image_desc.bps + (x + i) * image_desc.bpp;
								fout[ofs + 0] = col->r;
								fout[ofs + 1] = col->g;
								fout[ofs + 2] = col->b;
								fout[ofs + 3] = col->a;
								if(col->a == 0) has_alpha = true;
							}
						}
					}
				}
			}
		}
		break;
	case PF_DXT3:
		for (z = 0; z < image_desc.numLayers; z++)
		{
			for (y = 0; y < h; y += 4)
			{
				for (x = 0; x < w; x += 4)
				{
					alpha = fin;
					fin += 8;
					R_DXTReadColors(fin, colours);
					bitmask = ((uint*)fin)[1];
					bitmask = LittleLong(bitmask);
					fin += 8;

					// Four-color block: derive the other two colors.    
					// 00 = color_0, 01 = color_1, 10 = color_2, 11 = color_3
					// These 2-bit codes correspond to the 2-bit fields 
					// stored in the 64-bit block.
					colours[2].b = (2 * colours[0].b + colours[1].b + 1) / 3;
					colours[2].g = (2 * colours[0].g + colours[1].g + 1) / 3;
					colours[2].r = (2 * colours[0].r + colours[1].r + 1) / 3;
					colours[3].b = (colours[0].b + 2 * colours[1].b + 1) / 3;
					colours[3].g = (colours[0].g + 2 * colours[1].g + 1) / 3;
					colours[3].r = (colours[0].r + 2 * colours[1].r + 1) / 3;

					k = 0;
					for (j = 0; j < 4; j++)
					{
						for (i = 0; i < 4; i++, k++)
						{
							Select = (bitmask & (0x03 << k*2)) >> k*2;
							col = &colours[Select];
							if (((x + i) < w) && ((y + j) < h))
							{
								Offset = z * image_desc.SizeOfPlane + (y + j) * image_desc.bps + (x + i) * image_desc.bpp;
								fout[Offset + 0] = col->r;
								fout[Offset + 1] = col->g;
								fout[Offset + 2] = col->b;
							}
						}
					}
					for (j = 0; j < 4; j++)
					{
						sAlpha = alpha[2*j] + 256*alpha[2*j+1];
						for (i = 0; i < 4; i++)
						{
							if (((x + i) < w) && ((y + j) < h))
							{
								Offset = z * image_desc.SizeOfPlane + (y + j) * image_desc.bps + (x + i) * image_desc.bpp + 3;
								fout[Offset] = sAlpha & 0x0F;
								fout[Offset] = fout[Offset] | (fout[Offset]<<4);
								if(sAlpha == 0) has_alpha = true;
							}
							sAlpha >>= 4;
						}
					}
				}
			}
		}
		break;
	case PF_DXT5:
		for (z = 0; z < image_desc.numLayers; z++)
		{
			for (y = 0; y < h; y += 4)
			{
				for (x = 0; x < w; x += 4)
				{
					if (y >= h || x >= w) break;
					alphas[0] = fin[0];
					alphas[1] = fin[1];
					alphamask = fin + 2;
					fin += 8;

					R_DXTReadColors(fin, colours);
					bitmask = ((uint*)fin)[1];
					bitmask = LittleLong(bitmask);
					fin += 8;

					// Four-color block: derive the other two colors.    
					// 00 = color_0, 01 = color_1, 10 = color_2, 11 = color_3
					// These 2-bit codes correspond to the 2-bit fields 
					// stored in the 64-bit block.
					colours[2].b = (2 * colours[0].b + colours[1].b + 1) / 3;
					colours[2].g = (2 * colours[0].g + colours[1].g + 1) / 3;
					colours[2].r = (2 * colours[0].r + colours[1].r + 1) / 3;
					colours[3].b = (colours[0].b + 2 * colours[1].b + 1) / 3;
					colours[3].g = (colours[0].g + 2 * colours[1].g + 1) / 3;
					colours[3].r = (colours[0].r + 2 * colours[1].r + 1) / 3;

					k = 0;
					for (j = 0; j < 4; j++)
					{
						for (i = 0; i < 4; i++, k++)
						{
							Select = (bitmask & (0x03 << k*2)) >> k*2;
							col = &colours[Select];
							// only put pixels out < width or height
							if (((x + i) < w) && ((y + j) < h))
							{
								Offset = z * image_desc.SizeOfPlane + (y + j) * image_desc.bps + (x + i) * image_desc.bpp;
								fout[Offset + 0] = col->r;
								fout[Offset + 1] = col->g;
								fout[Offset + 2] = col->b;
							}
						}
					}
					// 8-alpha or 6-alpha block?    
					if (alphas[0] > alphas[1])
					{    
						// 8-alpha block:  derive the other six alphas.    
						// Bit code 000 = alpha_0, 001 = alpha_1, others are interpolated.
						alphas[2] = (6 * alphas[0] + 1 * alphas[1] + 3) / 7; // bit code 010
						alphas[3] = (5 * alphas[0] + 2 * alphas[1] + 3) / 7; // bit code 011
						alphas[4] = (4 * alphas[0] + 3 * alphas[1] + 3) / 7; // bit code 100
						alphas[5] = (3 * alphas[0] + 4 * alphas[1] + 3) / 7; // bit code 101
						alphas[6] = (2 * alphas[0] + 5 * alphas[1] + 3) / 7; // bit code 110
						alphas[7] = (1 * alphas[0] + 6 * alphas[1] + 3) / 7; // bit code 111
					}
					else
					{
						// 6-alpha block.
						// Bit code 000 = alpha_0, 001 = alpha_1, others are interpolated.
						alphas[2] = (4 * alphas[0] + 1 * alphas[1] + 2) / 5; // Bit code 010
						alphas[3] = (3 * alphas[0] + 2 * alphas[1] + 2) / 5; // Bit code 011
						alphas[4] = (2 * alphas[0] + 3 * alphas[1] + 2) / 5; // Bit code 100
						alphas[5] = (1 * alphas[0] + 4 * alphas[1] + 2) / 5; // Bit code 101
						alphas[6] = 0x00;				// Bit code 110
						alphas[7] = 0xFF;				// Bit code 111
					}
					// Note: Have to separate the next two loops,
					// it operates on a 6-byte system.

					// First three bytes
					bits = (alphamask[0]) | (alphamask[1] << 8) | (alphamask[2] << 16);
					for (j = 0; j < 2; j++)
					{
						for (i = 0; i < 4; i++)
						{
							// only put pixels out < width or height
							if (((x + i) < w) && ((y + j) < h))
							{
								Offset = z * image_desc.SizeOfPlane + (y + j) * image_desc.bps + (x + i) * image_desc.bpp + 3;
								fout[Offset] = alphas[bits & 0x07];
							}
							bits >>= 3;
						}
					}
					// Last three bytes
					bits = (alphamask[3]) | (alphamask[4] << 8) | (alphamask[5] << 16);
					for (j = 2; j < 4; j++)
					{
						for (i = 0; i < 4; i++)
						{
							// only put pixels out < width or height
							if (((x + i) < w) && ((y + j) < h))
							{
								Offset = z * image_desc.SizeOfPlane + (y + j) * image_desc.bps + (x + i) * image_desc.bpp + 3;
								fout[Offset] = alphas[bits & 0x07];
								if(bits & 0x07) has_alpha = true; 
							}
							bits >>= 3;
						}
					}
				}
			}
		}
		break;
	default:
		MsgDev(D_WARN, "qrsCompressedTexImage2D: invalid compression type: %s\n", PFDesc[internalformat].name );
		return false;
	}

	scaled_width = w;
	scaled_height = h;
	R_RoundImageDimensions(&scaled_width, &scaled_height );

	// upload base image or miplevel
	if( image_desc.tflags & TF_COMPRESS )
		image_desc.glSamples = (has_alpha) ? GL_COMPRESSED_RGBA_ARB : GL_COMPRESSED_RGB_ARB;
	else image_desc.glSamples = (has_alpha) ? GL_RGBA : GL_RGB;
	R_ResampleTexture ((uint *)fout, w, h, scaled, scaled_width, scaled_height);
	if( !level ) GL_GenerateMipmaps( scaled_width, scaled_height ); // generate mips if needed
	pglTexImage2D ( target, level, image_desc.glSamples, scaled_width, scaled_height, border, image_desc.glMask, image_desc.glType, (byte *)scaled );

	if(pglGetError()) return false;
	return true;
}

bool CompressedTexImage2D( uint target, int level, int intformat, uint width, uint height, int border, uint imageSize, const void* data )
{
	uint dxtformat = 0;
	uint pixformat = PFDesc[intformat].format;

	if(GL_Support( R_TEXTURE_COMPRESSION_EXT ))
	{
		switch( pixformat )
		{
		case PF_DXT1: dxtformat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
		case PF_DXT3: dxtformat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
		case PF_DXT5: dxtformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
		default: use_gl_extension = false; break;
		}
	}
	else use_gl_extension = false;

	if( use_gl_extension )
	{
		if( !level ) GL_GenerateMipmaps( width, height ); // generate mips if needed
		pglCompressedTexImage2DARB( target, level, dxtformat, width, height, border, imageSize, data );
		if(!pglGetError()) return true;
		// otherwise try loading with software unpacker
	}
	return qrsCompressedTexImage2D( target, level, pixformat, width, height, border, imageSize, data );
}

/*
===============
R_LoadImageDXT
===============
*/
bool R_LoadImageDXT( byte *data, GLuint target )
{
	int i, size = 0;
	int w = image_desc.width;
	int h = image_desc.height;
	int d = image_desc.numLayers;
	
	for( i = 0; i < image_desc.MipCount; i++, data += size )
	{
		R_SetPixelFormat( w, h, d );
		size = image_desc.SizeOfFile;

		if(!CompressedTexImage2D( target, i, image_desc.format, w, h, 0, size, data ))
			break; // there were errors
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1; //calc size of next mip
	}

	GL_TexFilter();
	return true;
}

/*
===============
qrsDecompressedTexImage2D

cpu version of loading non paletted rgba buffer
===============
*/
bool qrsDecompressedTexImage2D( uint target, int level, int internalformat, uint width, uint height, int border, uint imageSize, const void* data )
{
	byte	*fin;
	int	i, p; 
	int	scaled_width, scaled_height;
	uint	*scaled = (uint *)image_desc.scaled;
	byte	*fout = image_desc.source;
	bool	noalpha = true;

	if (!data) return false;
	fin = (byte *)data;
	scaled_width = width;
	scaled_height = height;

	switch( PFDesc[internalformat].format )
	{
	case PF_INDEXED_24:
		if( image_desc.flags & IMAGE_HAS_ALPHA )
		{
			// studio model indexed texture probably with alphachannel
			for (i = 0; i < width * height; i++)
			{
				p = fin[i];
				if( p == 255 ) noalpha = false;
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
		if( noalpha ) image_desc.flags &= ~IMAGE_HAS_ALPHA;
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
		MsgDev(D_WARN, "qrsDecompressedTexImage2D: invalid compression type: %s\n", PFDesc[internalformat].name );
		return false;
	}

	R_RoundImageDimensions( &scaled_width, &scaled_height );
	if( image_desc.tflags & TF_COMPRESS )
		image_desc.glSamples = (!noalpha) ? GL_COMPRESSED_RGBA_ARB : GL_COMPRESSED_RGB_ARB;
	else image_desc.glSamples = (!noalpha) ? GL_RGBA : GL_RGB;
	R_ResampleTexture((uint *)fout, width, height, scaled, scaled_width, scaled_height);
	if( !level ) GL_GenerateMipmaps( scaled_width, scaled_height ); // generate mips if needed
	pglTexImage2D( target, level, image_desc.glSamples, scaled_width, scaled_height, border, image_desc.glMask, image_desc.glType, (byte *)scaled );

	if(pglGetError()) return false;
	return true;
}

/*
===============
R_LoadImageRGBA
===============
*/
bool R_LoadImageRGBA( byte *data, GLuint target )
{
	int i, size = 0;
	int w = image_desc.width;
	int h = image_desc.height;
	int d = image_desc.numLayers; // ABGR_64 may using some layers

	for( i = 0; i < image_desc.MipCount; i++, data += size )
	{
		R_SetPixelFormat( w, h, d );
		size = image_desc.SizeOfFile;

		if(!qrsDecompressedTexImage2D( target, i, image_desc.format, w, h, 0, size, data ))
			break; // there were errors
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1; // calc size of next mip
	}

	GL_TexFilter();
	return true;
}

bool qrsDecompressImageARGB( uint target, int level, int internalformat, uint width, uint height, int border, uint imageSize, const void* data )
{
	uint	ReadI = 0, TempBpp;
	uint	RedL, RedR, GreenL, GreenR, BlueL, BlueR, AlphaL, AlphaR;
	uint	r_bitmask, g_bitmask, b_bitmask, a_bitmask;
	uint	*scaled = (unsigned *)image_desc.scaled;
	byte	*fin, *fout = image_desc.source;
	bool	has_alpha = false;
	int	scaled_width, scaled_height;
	int	i, w, h;

	if (!data) return false;
	if(image_desc.pal)
	{
		byte *pal = image_desc.pal; // copy ptr
		r_bitmask	= BuffLittleLong( pal ); pal += 4;
		g_bitmask = BuffLittleLong( pal ); pal += 4;
		b_bitmask = BuffLittleLong( pal ); pal += 4;
		a_bitmask = BuffLittleLong( pal ); pal += 4;
	}
	else
	{
		MsgDev(D_ERROR, "R_StoreImageARGB: can't get RGBA bitmask\n" );		
		return false;
	}

	R_GetBitsFromMask(r_bitmask, &RedL, &RedR);
	R_GetBitsFromMask(g_bitmask, &GreenL, &GreenR);
	R_GetBitsFromMask(b_bitmask, &BlueL, &BlueR);
	R_GetBitsFromMask(a_bitmask, &AlphaL, &AlphaR);
       
	fin = (byte *)data;
	w = width;
	h = height;
	TempBpp = image_desc.BitsCount / 8;

	for( i = 0; i < image_desc.SizeOfData; i += image_desc.bpp )
	{
		// TODO: This is SLOOOW...
		// but the old version crashed in release build under
		// winxp (and xp is right to stop this code - I always
		// wondered that it worked the old way at all)
		if( image_desc.SizeOfData - i < 4 )
		{
			// less than 4 byte to write?
			if( TempBpp == 1 ) ReadI = *((byte*)fin);
			else if( TempBpp == 2 ) ReadI = BuffLittleShort( fin );
			else if( TempBpp == 3 ) ReadI = BuffLittleLong( fin );
		}
		else ReadI = BuffLittleLong( fin );
		fin += TempBpp;
		fout[i] = ((ReadI & r_bitmask)>> RedR) << RedL;

		if( image_desc.bpp >= 3 )
		{
			fout[i+1] = ((ReadI & g_bitmask) >> GreenR) << GreenL;
			fout[i+2] = ((ReadI & b_bitmask) >> BlueR) << BlueL;
			if (image_desc.bpp == 4)
			{
				fout[i+3] = ((ReadI & a_bitmask) >> AlphaR) << AlphaL;
				if (AlphaL >= 7) fout[i+3] = fout[i+3] ? 0xFF : 0x00;
				else if (AlphaL >= 4) fout[i+3] = fout[i+3] | (fout[i+3] >> 4);
			}
		}
		else if (image_desc.bpp == 2)
		{
			fout[i+1] = ((ReadI & a_bitmask) >> AlphaR) << AlphaL;
			if (AlphaL >= 7) fout[i+1] = fout[i+1] ? 0xFF : 0x00;
			else if (AlphaL >= 4) fout[i+1] = fout[i+1] | (fout[i+3] >> 4);
		}
	}

	scaled_width = w;
	scaled_height = h;
	R_RoundImageDimensions( &scaled_width, &scaled_height );

	// upload base image or miplevel
	if( image_desc.tflags & TF_COMPRESS )
		image_desc.glSamples = (has_alpha) ? GL_COMPRESSED_RGBA_ARB : GL_COMPRESSED_RGB_ARB;
	else image_desc.glSamples = (has_alpha) ? GL_RGBA : GL_RGB;
	R_ResampleTexture ((uint *)fout, w, h, scaled, scaled_width, scaled_height);
	if( !level ) GL_GenerateMipmaps( scaled_width, scaled_height ); // generate mips if needed
	pglTexImage2D ( target, level, image_desc.glSamples, scaled_width, scaled_height, border, image_desc.glMask, image_desc.glType, (byte *)scaled );

	if(pglGetError()) return false;
	return true;
}

bool DecompressImageARGB( uint target, int level, int intformat, uint width, uint height, int border, uint imageSize, const void* data )
{
	uint argbformat = 0;
	uint datatype = 0;
	uint pixformat = PFDesc[intformat].format;

	switch( pixformat )
	{
	case PF_ARGB_32:
	case PF_LUMINANCE:
	case PF_LUMINANCE_16:
	case PF_LUMINANCE_ALPHA:
		argbformat = GL_RGB5_A1;
		datatype = GL_UNSIGNED_SHORT_1_5_5_5_REV;
		break;
	default: use_gl_extension = false; break;
	}

	if( use_gl_extension )
	{
		if( !level ) GL_GenerateMipmaps( width, height ); // generate mips if needed
		pglTexImage2D( target, level, argbformat, width, height, border, image_desc.glMask, datatype, data );
		if(!pglGetError()) return true;
		// otherwise try loading with software unpacker
	}
	return qrsDecompressImageARGB(target, level, pixformat, width, height, border, imageSize, data );
}

bool R_LoadImageARGB( byte *data, GLuint target )
{
	int i, size = 0;
	int w = image_desc.width;
	int h = image_desc.height;
	int d = image_desc.numLayers;
	
	for( i = 0; i < image_desc.MipCount; i++, data += size )
	{
		R_SetPixelFormat( w, h, d );
		size = image_desc.SizeOfFile;

		if(!DecompressImageARGB( target, i, image_desc.format, w, h, 0, size, data ))
			break; //there were errors
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1; // calc size of next mip
	}

	GL_TexFilter();
	return true;
}

bool qrsDecompressImageFloat( uint target, int level, int internalformat, uint width, uint height, int border, uint imageSize, const void* data )
{
	// not implemented
	return false;
}

bool DecompressImageFloat( uint target, int level, int intformat, uint width, uint height, int border, uint imageSize, const void* data )
{
	uint floatformat = 0;
	uint datatype = 0;
	uint pixformat = PFDesc[intformat].format;

	switch( pixformat )
	{
	case PF_ABGR_128F:
		floatformat = GL_FLOAT;
		datatype = GL_UNSIGNED_SHORT_1_5_5_5_REV;
		break;
	default: use_gl_extension = false; break;
	}

	if( use_gl_extension )
	{
		if( !level ) GL_GenerateMipmaps( width, height ); // generate mips if needed // generate mips if needed
		pglTexImage2D( target, level, floatformat, width, height, border, image_desc.glMask, datatype, data );
		if(!pglGetError()) return true;
		// otherwise try loading with software unpacker
	}
	return qrsDecompressImageFloat( target, level, pixformat, width, height, border, imageSize, data );
}

bool R_LoadImageFloat( byte *data, GLuint target )
{
	int i, size = 0;
	int w = image_desc.width;
	int h = image_desc.height;
	int d = image_desc.numLayers;

	for( i = 0; i < image_desc.MipCount; i++, data += size )
	{
		R_SetPixelFormat( w, h, d );
		size = image_desc.SizeOfFile;

		if(!DecompressImageFloat( target, i, image_desc.format, w, h, 0, size, data ))
			break; // there were errors 
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1; // calc size of next mip
	}

	GL_TexFilter();
	return true;
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
		image = r_textures[i];
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
	bool	iResult;

	switch( type )
	{
	case PF_RGB_24:
	case PF_ABGR_64:
	case PF_RGBA_32:
	case PF_RGBA_GN:
	case PF_INDEXED_24:
	case PF_INDEXED_32: iResult = R_LoadImageRGBA( buffer, target ); break;
	case PF_LUMINANCE:
	case PF_LUMINANCE_16:
	case PF_LUMINANCE_ALPHA:
	case PF_ARGB_32: iResult = R_LoadImageARGB( buffer, target ); break;
	case PF_DXT1:
	case PF_DXT3:
	case PF_DXT5: iResult = R_LoadImageDXT( buffer, target ); break;
	case PF_ABGR_128F: iResult = R_LoadImageFloat( buffer, target ); break;
	case PF_UNKNOWN: iResult = false; break;
	}

	return iResult;
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
		image = r_textures[i];
		if( !image->texnum ) break;
	}
	if( i == r_numTextures )
	{
		if( r_numTextures == MAX_TEXTURES )
		{
			MsgDev(D_ERROR, "R_LoadTexture: r_textures limit is out\n");
			return r_defaultTexture;
		}
		r_numTextures++;
	}

	image = r_textures[i] = Mem_Alloc( r_imagepool, sizeof( texture_t ));
	if( com.strlen( name ) >= sizeof(image->name)) MsgDev( D_WARN, "R_LoadImage: \"%s\" is too long", name);

	// nothing to load
	if( !pic || !pic->buffer )
	{
		// create notexture with another name
		Mem_Copy( image, r_defaultTexture, sizeof( texture_t ));
		com.strncpy( image->name, name, sizeof( image->name ));
		image->registration_sequence = registration_sequence;
		return image;
	}

	com.strncpy( image->name, name, sizeof( image->name ));
	image->registration_sequence = registration_sequence;

	if( flags & TF_CUBEMAP )
	{
		if( pic->flags & IMAGE_CUBEMAP )
		{
			numsides = 6;
			if( pic->flags & IMAGE_CUBEMAP_FLIP )
			{
				// change draworder for not packed cubemaps
				flags &= ~TF_CUBEMAP;
				flags |= TF_CUBEMAP_FLIP;
			}
			target = GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB;
		}
		else
		{
			MsgDev( D_WARN, "texture %s it's not a cubemap image\n", name );
			flags &= ~TF_CUBEMAP;
		}
	}
	else if( flags & TF_SKYBOX )
	{
		//FIXME: get to work
		if( pic->flags & IMAGE_CUBEMAP )
		{
			numsides = 6;
			if( pic->flags & IMAGE_CUBEMAP_FLIP )
			{
				// change draworder for skies
				flags |= TF_SKYSIDE_FLIP;
			}
			else flags |= TF_SKYSIDE;
		}
		else
		{
			MsgDev( D_WARN, "texture %s it's not a skybox set\n", name );
			flags &= ~TF_SKYBOX;
		}
		Host_Error("TF_SKYBOX not implemeneted\n");
	}

	image->width = width = pic->width;
	image->height = height = pic->height;
	image->bumpScale = bumpScale;
	image->flags = flags;
          buf = pic->buffer;

	// fill image_desc
	R_GetPixelFormat( pic, flags, bumpScale );
	pglGenTextures( 1, &image->texnum );

	for(i = 0; i < numsides; i++, buf += offset )
	{
		GL_BindTexture( image );
		
		R_SetPixelFormat( image_desc.width, image_desc.height, image_desc.numLayers );
		offset = image_desc.SizeOfFile; // move pointer

		MsgDev(D_LOAD, "%s [%s] \n", name, PFDesc[image_desc.format].name );
		R_UploadTexture( buf, pic->type, target + i );
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
	
	for( i = 0; i < r_numTextures; i++ )
	{
		image = r_textures[i];
		// used this sequence
		if( image->registration_sequence == registration_sequence ) continue;
		if( !image->name[0] || image->name[0] == '*' ) continue; // free texture slot or system texture
		pglDeleteTextures( 1, &image->texnum );
		Mem_Free( image );
		image = NULL;
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
	rgbdata_t 	*r_shot;

	// shared framebuffer not init
	if( !r_framebuffer ) return false;

	// get screen frame
	pglReadPixels( 0, 0, r_width->integer, r_height->integer, GL_RGB, GL_UNSIGNED_BYTE, r_framebuffer );

	r_shot = Mem_Alloc( r_imagepool, sizeof( rgbdata_t ));
	r_shot->width = r_width->integer;
	r_shot->height = r_height->integer;
	r_shot->type = PF_RGB_24;
	r_shot->hint = PF_DXT5; // save format
	r_shot->size = r_shot->width * r_shot->height * 3;
	r_shot->palette = NULL;
	r_shot->numLayers = 1;
	r_shot->numMips = 1;
	r_shot->buffer = r_framebuffer;

	if( levelshot ) Image_Resample( &r_shot, 512, 384, false ); // resample to 512x384
	else VID_ImageAdjustGamma( r_shot->buffer, r_shot->width, r_shot->height ); // adjust brightness
	Image_Process( &r_shot, IMAGE_FLIP_Y, false );

	// write image
	FS_SaveImage( filename, r_shot );
	Mem_Free( r_shot ); // don't touch framebuffer!
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