//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_texture.c - load & convert textures
//=======================================================================

#include "gl_local.h"
#include "byteorder.h"

/*
=============================================================

  TEXTURES UPLOAD

=============================================================
*/
dll_info_t imglib_dll = { "imglib.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(imglib_exp_t) };
image_t gltextures[MAX_GLTEXTURES];
int numgltextures;
byte intensitytable[256];
byte lumatable[256];
cvar_t *gl_maxsize;
byte *r_imagepool;
bool use_gl_extension = false;
cvar_t *intensity;
uint d_8to24table[256];
imglib_exp_t *Image;

#define STAGE_NORMAL	0
#define STAGE_LUMA		1

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
	uint		glMask;
	uint		glType;
	imagetype_t	type;
	int		stage;

	int		flags;
	byte		*pal;
	byte		*source;
	byte		*scaled;
} pixformat_desc_t;

static pixformat_desc_t image_desc;

static byte palette_int[] =
{
#include "palette.h"
};

/*
===============
R_ImageList_f
===============
*/
void R_ImageList_f (void)
{
	int	i;
	image_t	*image;
	const char *palstrings[2] = {"RGB","PAL"};

	Msg( "------------------\n");
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (image->texnum[0] <= 0) continue;
		switch (image->type)
		{
		case it_pic: Msg( "Pic "); break;
		case it_sky: Msg( "Sky "); break;
		case it_wall: Msg( "Wall"); break;
		case it_skin: Msg( "Skin"); break;
		case it_sprite: Msg( "Spr "); break;
		case it_cubemap: Msg( "Cubemap "); break;
		default: Msg( "Sys "); break;
		}
		Msg( " %3i %3i %s: %s\n", image->width, image->height, palstrings[image->paletted], image->name);
	}
	Msg( "Total images count (without mipmaps): %i\n", numgltextures);
}

bool R_ImageHasMips( void )
{
	// will be generated later
	if(image_desc.flags & IMAGE_GEN_MIPS)
		return true;
	if( image_desc.MipCount > 1)
		return true;
	return false;
}

/*
===============
R_GetImageSize

calculate buffer size for current miplevel
===============
*/
uint R_GetImageSize( int block, int width, int height, int depth, int bpp, int rgbcount )
{
	uint BlockSize = 0;

	if(block == 0) BlockSize = width * height * bpp;
	else if(block > 0) BlockSize = ((width + 3)/4) * ((height + 3)/4) * depth * block;
	else if(block < 0 && rgbcount > 0) BlockSize = width * height * depth * rgbcount;
	else BlockSize = width * height * abs(block);

	return BlockSize;
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
bool R_GetPixelFormat( rgbdata_t *pic, imagetype_t type )
{
	int	w, h, d, i, s, BlockSize;
	size_t	mipsize, totalsize = 0;

	if(!pic || !pic->buffer) return false;

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
	if(i != PF_TOTALCOUNT) // make sure what match found
	{
		image_desc.numLayers = d = pic->numLayers;
		image_desc.width = w = pic->width;
		image_desc.height = h = pic->height;
		image_desc.flags = pic->flags;
		image_desc.type = type;

		image_desc.bps = image_desc.width * image_desc.bpp * image_desc.bpc;
		image_desc.SizeOfPlane = image_desc.bps * image_desc.height;
		image_desc.SizeOfData = image_desc.SizeOfPlane * image_desc.numLayers;
		image_desc.BitsCount = pic->bitsCount;

		// now correct buffer size
		for( i = 0; i < pic->numMips; i++, totalsize += mipsize )
		{
			mipsize = R_GetImageSize( BlockSize, w, h, d, image_desc.bpp, image_desc.BitsCount / 8 );
			w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1;
		}

		if(type == it_pic || type == it_sky)
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
		}
		else
		{
			// so it normal texture without mips
			image_desc.flags |= IMAGE_GEN_MIPS;
			image_desc.MipCount = pic->numMips;
		}
		
		if(image_desc.MipCount < 1) image_desc.MipCount = 1;
		image_desc.pal = pic->palette;
	}	

	// restore temp dimensions
	w = image_desc.width;
	h = image_desc.height;
	s = w * h;

	// can use gl extension ?
	R_RoundImageDimensions(&w, &h);

	if(w == image_desc.width && h == image_desc.height) 
		use_gl_extension = true;
	else use_gl_extension = false;

	image_desc.source = Mem_Alloc( r_imagepool, s * 4 );	// source buffer
	image_desc.scaled = Mem_Alloc( r_imagepool, w * h * 4 );	// scaled buffer

	if(image_desc.flags & IMAGE_CUBEMAP)
		totalsize *= 6;

	if(totalsize != pic->size) // sanity check
	{
		MsgDev(D_WARN, "R_GetPixelFormat: invalid image size (%i should be %i)\n", pic->size, totalsize );
		return false;
	}
	if(s&3) // will be resample, not error
	{
		MsgDev(D_WARN, "R_GetPixelFormat: s&3 [%d x %d]\n", image_desc.width, image_desc.height );
		return false;
	}	
	return true;
}

/*
===============
R_GetPalette
===============
*/
void R_GetPalette( void )
{
	uint	v;
	int	i, r, g, b;
	byte	*pal = palette_int;

	// used by particle system once only
	for (i = 0; i < 256; i++)
	{
		r = pal[i*3+0];
		g = pal[i*3+1];
		b = pal[i*3+2];
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		d_8to24table[i] = LittleLong(v);
	}
	d_8to24table[255] &= LittleLong(0xffffff); // 255 is transparent
}

/*
===============
R_ShutdownTextures
===============
*/
void R_ShutdownTextures (void)
{
	int	i, k;
	image_t	*image;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		// free image_t slot
		if (!image->registration_sequence) continue;

		// free it
		if(image->type == it_sky || image->type == it_cubemap)
			for(k = 0; k < 6; k++) pglDeleteTextures (1, &image->texnum[k] );
		else pglDeleteTextures (1, &image->texnum[0] );
		memset (image, 0, sizeof(*image));
	}

	Image->Free();
	Sys_FreeLibrary( &imglib_dll ); // free imagelib
}

/*
===============
R_InitTextures
===============
*/
void R_InitTextures( void )
{
	int	texsize, i, j;
	launch_t	CreateImglib;
	float	f;

	Sys_LoadLibrary( &imglib_dll ); // load imagelib
	CreateImglib = (void *)imglib_dll.main;
	Image = CreateImglib( &com, NULL ); // second interface not allowed

	Image->Init();
	r_imagepool = Mem_AllocPool("Texture Pool");
          gl_maxsize = Cvar_Get( "gl_maxsize", "4096", CVAR_ARCHIVE, "texture dimension max size" );
	
	pglGetIntegerv(GL_MAX_TEXTURE_SIZE, &texsize); // merge value
	if( gl_maxsize->integer != texsize ) Cvar_SetValue( "gl_maxsize", texsize );

	registration_sequence = 1;

	// init intensity conversions
	intensity = Cvar_Get ("intensity", "2", 0, "gamma intensity value" );
	if( intensity->value <= 1 ) Cvar_SetValue( "intensity", 1 );
	gl_state.inverse_intensity = 1 / intensity->value;

	for (i = 0; i < 256; i++)
	{
		j = i * intensity->value;
		intensitytable[i] = bound( 0, j, 255 );
	}

	// make a luma table by squaring the intensity twice
	for( i = 0; i < 256; i++ )
	{
		f = ( float )i/255.0f;

		f *= f;
		f *= 2;
		f *= f;
		f *= 2;
		lumatable[i] = ( byte )(bound(0,f,1) * 255.0f);
	}
	R_GetPalette();
}

void R_RoundImageDimensions( int *scaled_width, int *scaled_height )
{
	int width, height;

	for( width = 1; width < *scaled_width; width <<= 1 );
	for( height = 1; height < *scaled_height; height <<= 1 );

	*scaled_width = bound( 1, width, gl_maxsize->integer );
	*scaled_height = bound( 1, height, gl_maxsize->integer );
}

bool R_ResampleTexture( uint *in, int inwidth, int inheight, uint *out, int outwidth, int outheight )
{
	int	i, j;
	uint	frac, fracstep;
	uint	*inrow, *inrow2;
	uint	p1[4096], p2[4096];
	byte	*pix1, *pix2, *pix3, *pix4;

	// check for buffers
	if(!in || !out || in == out) return false;
	if(outheight == 0 || outwidth == 0) return false;
	
	if( image_desc.stage == STAGE_LUMA )
	{
		// apply the double-squared luminescent version
		for( i = 0, pix1 = (byte *)in; i < inwidth * inheight; i++, pix1 += 4 ) 
		{
			pix1[0] = lumatable[pix1[0]];
			pix1[1] = lumatable[pix1[1]];
			pix1[2] = lumatable[pix1[2]];
		}
	}

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

void R_ImageCorrectPreMult( uint *data, int datasize )
{
	int i;

	for (i = 0; i < datasize; i += 4)
	{
		if (data[i+3] != 0) // Cannot divide by 0.
		{	
			data[i+0] = (byte)(((uint)data[i+0]<<8) / data[i+3]);
			data[i+1] = (byte)(((uint)data[i+1]<<8) / data[i+3]);
			data[i+2] = (byte)(((uint)data[i+2]<<8) / data[i+3]);
		}
	}
}

/*
===============
pglGenerateMipmaps

sgis generate mipmap
===============
*/
void GL_GenerateMipmaps( void )
{
	if( image_desc.flags & IMAGE_GEN_MIPS )
	{
		pglHint( GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST );
		pglTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE );
		if(pglGetError()) MsgDev(D_WARN, "GL_GenerateMipmaps: can't create mip levels\n");
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
	int 	samples;

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
	samples = (has_alpha) ? gl_tex_alpha_format : gl_tex_solid_format;
	R_ResampleTexture ((uint *)fout, w, h, scaled, scaled_width, scaled_height);
	if( !level ) GL_GenerateMipmaps(); // generate mips if needed
	pglTexImage2D ( target, level, samples, scaled_width, scaled_height, border, image_desc.glMask, image_desc.glType, (byte *)scaled );

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
		if( !level ) GL_GenerateMipmaps(); // generate mips if needed
		pglCompressedTexImage2DARB(target, level, dxtformat, width, height, border, imageSize, data );
		if(!pglGetError()) return true;
		// otherwise try loading with software unpacker
	}
	return qrsCompressedTexImage2D(target, level, pixformat, width, height, border, imageSize, data );
}

/*
===============
R_LoadImageDXT
===============
*/
bool R_LoadImageDXT( byte *data )
{
	int i, size = 0;
	int w = image_desc.width;
	int h = image_desc.height;
	int d = image_desc.numLayers;
	
	for( i = 0; i < image_desc.MipCount; i++, data += size )
	{
		R_SetPixelFormat( w, h, d );
		size = image_desc.SizeOfFile;

		if(!CompressedTexImage2D(GL_TEXTURE_2D, i, image_desc.format, w, h, 0, size, data ))
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
	int	i, p, samples; 
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
		if (image_desc.flags & IMAGE_HAS_ALPHA)
		{
			// studio model indexed texture probably with alphachannel
			for (i = 0; i < width * height; i++)
			{
				p = fin[i];
				if (p == 255) noalpha = false;
				fout[(i<<2)+0] = image_desc.pal[p*3+0];
				fout[(i<<2)+1] = image_desc.pal[p*3+1];
				fout[(i<<2)+2] = image_desc.pal[p*3+2];
				fout[(i<<2)+3] = (p == 255) ? 0 : 255;
			}
		}
		else
		{
			// studio model indexed texture without alphachannel
			for (i = 0; i < width * height; i++)
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
		for (i = 0; i < width*height; i++ )
		{
			fout[(i<<2)+0] = image_desc.pal[fin[i]*4+0];
			fout[(i<<2)+1] = image_desc.pal[fin[i]*4+1];
			fout[(i<<2)+2] = image_desc.pal[fin[i]*4+2];
			fout[(i<<2)+3] = image_desc.pal[fin[i]*4+3];
		}
		break;
	case PF_RGB_24:
	case PF_RGB_24_FLIP:
		// 24-bit image, that will not expanded to RGBA in imagelib.c for some reasons
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
	samples = (image_desc.flags & IMAGE_HAS_ALPHA) ? gl_tex_alpha_format : gl_tex_solid_format;
	R_ResampleTexture((uint *)fout, width, height, scaled, scaled_width, scaled_height);
	if( !level ) GL_GenerateMipmaps(); // generate mips if needed
	pglTexImage2D( target, level, samples, scaled_width, scaled_height, border, image_desc.glMask, image_desc.glType, (byte *)scaled );

	if(pglGetError()) return false;
	return true;
}

/*
===============
R_LoadImageRGBA
===============
*/
bool R_LoadImageRGBA( byte *data )
{
	int i, size = 0;
	int w = image_desc.width;
	int h = image_desc.height;
	int d = image_desc.numLayers; // ABGR_64 may using some layers

	for( i = 0; i < image_desc.MipCount; i++, data += size )
	{
		R_SetPixelFormat( w, h, d );
		size = image_desc.SizeOfFile;

		if(!qrsDecompressedTexImage2D(GL_TEXTURE_2D, i, image_desc.format, w, h, 0, size, data ))
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
	int	i, w, h, samples;

	if (!data) return false;
	if(image_desc.pal)
	{
		byte *pal = image_desc.pal;//copy ptr
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

	for (i = 0; i < image_desc.SizeOfData; i += image_desc.bpp)
	{
		// TODO: This is SLOOOW...
		// but the old version crashed in release build under
		// winxp (and xp is right to stop this code - I always
		// wondered that it worked the old way at all)
		if (image_desc.SizeOfData - i < 4)
		{
			// less than 4 byte to write?
			if (TempBpp == 1) ReadI = *((byte*)fin);
			else if (TempBpp == 2) ReadI = BuffLittleShort( fin );
			else if (TempBpp == 3) ReadI = BuffLittleLong( fin );
		}
		else ReadI = BuffLittleLong( fin );
		fin += TempBpp;
		fout[i] = ((ReadI & r_bitmask)>> RedR) << RedL;

		if(image_desc.bpp >= 3)
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
	R_RoundImageDimensions(&scaled_width, &scaled_height );

	// upload base image or miplevel
	samples = (image_desc.flags & IMAGE_HAS_ALPHA) ? gl_tex_alpha_format : gl_tex_solid_format;
	R_ResampleTexture ((uint *)fout, w, h, scaled, scaled_width, scaled_height);
	pglTexImage2D ( target, level, samples, scaled_width, scaled_height, border, image_desc.glMask, image_desc.glType, (byte *)scaled );

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

	if(use_gl_extension)
	{
		if( !level ) GL_GenerateMipmaps(); // generate mips if needed
		pglTexImage2D( target, level, argbformat, width, height, border, image_desc.glMask, datatype, data );
		if(!pglGetError()) return true;
		// otherwise try loading with software unpacker
	}
	return qrsDecompressImageARGB(target, level, pixformat, width, height, border, imageSize, data );
}

bool R_LoadImageARGB( byte *data )
{
	int i, size = 0;
	int w = image_desc.width;
	int h = image_desc.height;
	int d = image_desc.numLayers;
	
	for( i = 0; i < image_desc.MipCount; i++, data += size )
	{
		R_SetPixelFormat( w, h, d );
		size = image_desc.SizeOfFile;

		if(!DecompressImageARGB(GL_TEXTURE_2D, i, image_desc.format, w, h, 0, size, data ))
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

	if(use_gl_extension)
	{
		if( !level ) GL_GenerateMipmaps(); // generate mips if needed
		pglTexImage2D( target, level, floatformat, width, height, border, image_desc.glMask, datatype, data );
		if(!pglGetError()) return true;
		// otherwise try loading with software unpacker
	}
	return qrsDecompressImageFloat( target, level, pixformat, width, height, border, imageSize, data );
}

bool R_LoadImageFloat( byte *data )
{
	int i, size = 0;
	int w = image_desc.width;
	int h = image_desc.height;
	int d = image_desc.numLayers;

	for( i = 0; i < image_desc.MipCount; i++, data += size )
	{
		R_SetPixelFormat( w, h, d );
		size = image_desc.SizeOfFile;

		if(!DecompressImageFloat(GL_TEXTURE_2D, i, image_desc.format, w, h, 0, size, data ))
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
image_t *R_FindImage( char *name, const byte *buffer, size_t size, imagetype_t type )
{
	image_t	*image;
	rgbdata_t	*pic = NULL;
	int	i;
          
	if (!name ) return NULL;
          
	// look for it
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!strcmp(name, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	pic = Image->LoadImage(name, buffer, size ); //loading form disk or buffer
	image = R_LoadImage(name, pic, type ); //upload into video buffer
	Image->FreeImage( pic ); //free image

	return image;
}

bool R_UploadTexture( byte *buffer, int type )
{
	bool	iResult;

	switch( type )
	{
	case PF_RGB_24:
	case PF_ABGR_64:
	case PF_RGBA_32:
	case PF_RGBA_GN:
	case PF_INDEXED_24:
	case PF_INDEXED_32: iResult = R_LoadImageRGBA( buffer ); break;
	case PF_LUMINANCE:
	case PF_LUMINANCE_16:
	case PF_LUMINANCE_ALPHA:
	case PF_ARGB_32: iResult = R_LoadImageARGB( buffer ); break;
	case PF_DXT1:
	case PF_DXT3:
	case PF_DXT5: iResult = R_LoadImageDXT( buffer ); break;
	case PF_ABGR_128F: iResult = R_LoadImageFloat( buffer ); break;
	case PF_UNKNOWN: iResult = false; break;
	}

	return iResult;
}

/*
================
R_LoadImage

This is also used as an entry point for the generated r_notexture
================
*/
image_t *R_LoadImage( char *name, rgbdata_t *pic, imagetype_t type )
{
	image_t	*image;
          bool	iResult = true;
	int	i, numsides = 1, width, height;
	uint	offset = 0;
	int	skyorder_q2[6] = { 2, 3, 1, 0, 4, 5, }; // Quake, Half-Life skybox ordering
	int	skyorder_ms[6] = { 4, 5, 1, 0, 2, 3  }; // Microsoft DDS ordering (reverse)
	byte	*buf;

	// find a free image_t
	for( i = 0, image = gltextures; i < numgltextures; i++, image++ )
	{
		if (!image->texnum[0]) break;
	}
	if( i == numgltextures )
	{
		if( numgltextures == MAX_GLTEXTURES )
		{
			MsgDev(D_ERROR, "R_LoadImage: gl_textures limit is out\n");
			return NULL;
		}
		numgltextures++;
	}
	image = &gltextures[i];

	if( com.strlen(name) >= sizeof(image->name)) MsgDev( D_WARN, "R_LoadImage: \"%s\" is too long", name);

	// nothing to load
	if( !pic || !pic->buffer )
	{
		// create notexture with another name
		Mem_Copy( image, r_notexture, sizeof(image_t));
		com.strncpy( image->name, name, sizeof(image->name));
		image->registration_sequence = registration_sequence;
		return image;
	}

	com.strncpy (image->name, name, sizeof(image->name));
	image->registration_sequence = registration_sequence;

	if(pic->flags & IMAGE_CUBEMAP) 
	{
		numsides = 6;
		if(pic->flags & IMAGE_CUBEMAP_FLIP)
			Mem_Copy(image->texorder, skyorder_ms, sizeof(int) * 6 );
		else Mem_Copy(image->texorder, skyorder_q2, sizeof(int) * 6 );
	}
	else memset(image->texorder, 0, sizeof(int) * 6 );

	image->width = width = pic->width;
	image->height = height = pic->height;
	image->type = type;
          image->paletted = pic->palette ? true : false;
          buf = pic->buffer;

	// fill image_desc
	R_GetPixelFormat( pic, type );

	for(i = 0; i < numsides; i++, buf += offset )
	{
		image->texnum[i] = TEXNUM_IMAGES + numgltextures++;
		GL_Bind( image->texnum[i] );
		
		R_SetPixelFormat( image_desc.width, image_desc.height, image_desc.numLayers );
		offset = image_desc.SizeOfFile;// move pointer

		MsgDev(D_LOAD, "%s [%s] \n", name, PFDesc[image_desc.format].name );
		image_desc.stage = STAGE_NORMAL;
		R_UploadTexture( buf, pic->type );
                    
		image->lumatex[i] = TEXNUM_LUMAS + (image - gltextures);
		GL_Bind( image->lumatex[i] );
		image_desc.stage = STAGE_LUMA;
		R_UploadTexture( buf, pic->type );
	}          
	// check for errors
	if(!iResult)
	{
		MsgDev( D_ERROR, "R_LoadImage: can't loading %s with bpp %d\n", name, image_desc.bpp ); 
		return r_notexture;
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
void R_ImageFreeUnused(void)
{
	int	i, k;
	image_t	*image;

	// never free r_notexture or particle texture
	r_notexture->registration_sequence = registration_sequence;
	r_particletexture->registration_sequence = registration_sequence;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		// used this sequence
		if (image->registration_sequence == registration_sequence) continue;
		if (!image->registration_sequence) continue; // free image_t slot
		if (image->type == it_pic) continue; // don't free pics
		if (image->type == it_sky || image->type == it_cubemap)
			for(k = 0; k < 6; k++) pglDeleteTextures (1, &image->texnum[k] );
		else pglDeleteTextures (1, &image->texnum[0] );
		memset(image, 0, sizeof(*image));
	}
}
