//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_texture.c - load & convert textures
//=======================================================================

#include "gl_local.h"

/*
=============================================================

  TEXTURES UPLOAD

=============================================================
*/
image_t gltextures[MAX_GLTEXTURES];
int numgltextures;
byte intensitytable[256];
byte gammatable[256];
extern cvar_t *gl_picmip;
cvar_t *gl_maxsize;
byte *r_imagepool;
byte *imagebuffer;
int imagebufsize;
byte *uploadbuffer;
int uploadbufsize;
bool use_gl_extension = false;
cvar_t *intensity;
uint d_8to24table[256];

#define FILTER_SIZE 5 
#define BLUR_FILTER 0 
#define LIGHT_BLUR   1 
#define EDGE_FILTER 1 
#define EMBOSS_FILTER 3

float FilterMatrix[][FILTER_SIZE][FILTER_SIZE] = 
{ 
	{ // regular blur 
	{0, 0, 0, 0, 0}, 
	{0, 1, 1, 1, 0}, 
	{0, 1, 1, 1, 0}, 
	{0, 1, 1, 1, 0}, 
	{0, 0, 0, 0, 0}, 
	}, 
	{ // light blur 
	{0, 0, 0, 0, 0}, 
	{0, 1, 1, 1, 0}, 
	{0, 1, 4, 1, 0}, 
	{0, 1, 1, 1, 0}, 
	{0, 0, 0, 0, 0}, 
	}, 
	{ // find edges 
	{0,  0,  0,  0, 0}, 
	{0, -1, -1, -1, 0}, 
	{0, -1,  8, -1, 0}, 
	{0, -1, -1, -1, 0}, 
	{0,  0,  0,  0, 0}, 
	}, 
	{ // emboss  
	{-0.7, -0.7, -0.7, -0.7, 0   }, 
	{-0.7, -0.7, -0.7,  0,   0.7 }, 
	{-0.7, -0.7,  0,    0.7, 0.7 }, 
	{-0.7,  0,    0.7,  0.7, 0.7 }, 
	{ 0,    0.7,  0.7,  0.7, 0.7 }, 
	}
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
	uint		glMask;
	uint		glType;
	imagetype_t	type;

	int		flags;
	byte		*pal;
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
	Msg( "Total images count (not counting mipmaps): %i\n", numgltextures);
}

void R_SetPixelFormat( int width, int height, int depth )
{
	size_t	file_size;
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

	if(BlockSize == 0) image_desc.SizeOfFile = image_desc.width * image_desc.height * image_desc.bpp;
	else if(BlockSize > 0)
	{
		file_size = ((image_desc.width + 3)/4) * ((image_desc.height + 3)/4) * image_desc.numLayers;
		image_desc.SizeOfFile = file_size * BlockSize;
	}
	else if(BlockSize < 0 && image_desc.BitsCount > 0)
	{		
		file_size = image_desc.width * image_desc.height * image_desc.numLayers * (image_desc.BitsCount / 8);
		image_desc.SizeOfFile = file_size; 
	}
	else
	{
		file_size = image_desc.width * image_desc.height * abs(BlockSize);
		image_desc.SizeOfFile = file_size; 
	}

	if (image_desc.width * image_desc.height > imagebufsize / 4) // warning
		MsgWarn("R_SetPixelFormat: image too big [%i*%i]\n", image_desc.width, image_desc.height);
}

/*
===============
R_GetPixelFormat

filled additional info
===============
*/
bool R_GetPixelFormat( rgbdata_t *pic, imagetype_t type )
{
	int	w, h, i, BlockSize;
	size_t	file_size, r_size;

	if(!pic || !pic->buffer) return false;
		
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
	if(i != PF_TOTALCOUNT) //make sure what match found
	{
		image_desc.numLayers = pic->numLayers;
		image_desc.width = w = pic->width;
		image_desc.height = h = pic->height;
		image_desc.flags = pic->flags;
		image_desc.type = type;

		image_desc.bps = image_desc.width * image_desc.bpp * image_desc.bpc;
		image_desc.SizeOfPlane = image_desc.bps * image_desc.height;
		image_desc.SizeOfData = image_desc.SizeOfPlane * image_desc.numLayers;
		image_desc.BitsCount = pic->bitsCount;

		if(BlockSize == 0) image_desc.SizeOfFile = image_desc.width * image_desc.height * image_desc.bpp;
		else if(BlockSize > 0)
		{
			file_size = ((image_desc.width + 3)/4) * ((image_desc.height + 3)/4) * image_desc.numLayers;
			image_desc.SizeOfFile = file_size * BlockSize;
		}
		else if(BlockSize < 0 && image_desc.BitsCount > 0)
		{		
			file_size = image_desc.width * image_desc.height * image_desc.numLayers * (image_desc.BitsCount / 8);
			image_desc.SizeOfFile = file_size; 
		}
		else
		{
			file_size = image_desc.width * image_desc.height * abs(BlockSize);
			image_desc.SizeOfFile = file_size; 
		}

		// don't build mips for sky and hud pics
		if(type == it_pic || type == it_sky) image_desc.flags &= ~IMAGE_GEN_MIPS;
		else image_desc.flags |= IMAGE_GEN_MIPS;

		image_desc.MipCount = pic->numMips;
		if(image_desc.MipCount < 1) image_desc.MipCount = 1;
		image_desc.pal = pic->palette;
	}	

	// can use gl extension ?
	R_RoundImageDimensions(&w, &h, (image_desc.flags & IMAGE_GEN_MIPS));

	if(w == image_desc.width && h == image_desc.height) use_gl_extension = true;
	else use_gl_extension = false;

	if(image_desc.flags & IMAGE_CUBEMAP) r_size = image_desc.SizeOfFile * 6;
	else r_size = image_desc.SizeOfFile;

	if(r_size != pic->size) // sanity check
	{
		MsgWarn("R_GetPixelFormat: invalid image size\n");
		return false;
	}	
	return true;
}

/*
===============
R_GetPalette
===============
*/
void R_GetPalette (void)
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
			for(k = 0; k < 6; k++) qglDeleteTextures (1, &image->texnum[k] );
		else qglDeleteTextures (1, &image->texnum[0] );
		memset (image, 0, sizeof(*image));
	}
}

/*
===============
R_InitTextures
===============
*/
void R_InitTextures( void )
{
	int texsize, i, j;
          float g = vid_gamma->value;
	
	r_imagepool = Mem_AllocPool("Texture Pool");
          gl_maxsize = ri.Cvar_Get ("gl_maxsize", "0", 0);
	
	qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &texsize);
	if (gl_maxsize->value > texsize)
		ri.Cvar_SetValue ("gl_maxsize", texsize);

	if(texsize < 2048) imagebufsize = 2048*2048*4;
	else imagebufsize = texsize * texsize * 4;
	uploadbufsize = texsize * texsize * 4;
	
	//create intermediate & upload image buffer
	imagebuffer = Mem_Alloc( r_imagepool, imagebufsize );
          uploadbuffer = Mem_Alloc( r_imagepool, uploadbufsize );

	registration_sequence = 1;

	// init intensity conversions
	intensity = ri.Cvar_Get ("intensity", "2", 0);
	if ( intensity->value <= 1 ) ri.Cvar_Set( "intensity", "1" );

	gl_state.inverse_intensity = 1 / intensity->value;
	R_GetPalette();

	for ( i = 0; i < 256; i++ )
	{
		if ( g == 1 ) gammatable[i] = i;
		else
		{
			float inf;

			inf = 255 * pow ( (i+0.5)/255.5 , g ) + 0.5;
			if (inf < 0) inf = 0;
			if (inf > 255) inf = 255;
			gammatable[i] = inf;
		}
	}
	for (i = 0; i < 256; i++)
	{
		j = i * intensity->value;
		if (j > 255) j = 255;
		intensitytable[i] = j;
	}
}

/* 
================== 
R_FilterTexture 

Applies a 5 x 5 filtering matrix to the texture, then runs it through a simulated OpenGL texture environment 
blend with the original data to derive a new texture.  Freaky, funky, and *f--king* *fantastic*.  You can do 
reasonable enough "fake bumpmapping" with this baby... 

Filtering algorithm from http://www.student.kuleuven.ac.be/~m0216922/CG/filtering.html 
All credit due 
================== 
*/
 
void R_FilterTexture (int filterindex, uint *data, int width, int height, float factor, float bias, bool greyscale, GLenum GLBlendOperator) 
{ 
	int i, x, y; 
	int filterX, filterY; 
	uint *temp; 

	// allocate a temp buffer 
	temp = Z_Malloc (width * height * 4); 

	for (x = 0; x < width; x++) 
	{ 
		for (y = 0; y < height; y++) 
		{ 
			float rgbFloat[3] = {0, 0, 0}; 

			for (filterX = 0; filterX < FILTER_SIZE; filterX++) 
			{ 
				for (filterY = 0; filterY < FILTER_SIZE; filterY++) 
				{ 
					int imageX = (x - (FILTER_SIZE / 2) + filterX + width) % width; 
					int imageY = (y - (FILTER_SIZE / 2) + filterY + height) % height; 

					// casting's a unary operation anyway, so the othermost set of brackets in the left part 
					// of the rvalue should not be necessary... but i'm paranoid when it comes to C... 
					rgbFloat[0] += ((float) ((byte *) &data[imageY * width + imageX])[0]) * FilterMatrix[filterindex][filterX][filterY]; 
					rgbFloat[1] += ((float) ((byte *) &data[imageY * width + imageX])[1]) * FilterMatrix[filterindex][filterX][filterY]; 
					rgbFloat[2] += ((float) ((byte *) &data[imageY * width + imageX])[2]) * FilterMatrix[filterindex][filterX][filterY]; 
				} 
			} 

			// multiply by factor, add bias, and clamp 
			for (i = 0; i < 3; i++) 
			{ 
				rgbFloat[i] *= factor; 
				rgbFloat[i] += bias; 

				if (rgbFloat[i] < 0) rgbFloat[i] = 0; 
				if (rgbFloat[i] > 255) rgbFloat[i] = 255; 
			} 

			if (greyscale) 
			{ 
				// NTSC greyscale conversion standard 
				float avg = (rgbFloat[0] * 30 + rgbFloat[1] * 59 + rgbFloat[2] * 11) / 100; 

				// divide by 255 so GL operations work as expected 
				rgbFloat[0] = avg / 255.0; 
				rgbFloat[1] = avg / 255.0; 
				rgbFloat[2] = avg / 255.0; 
			} 

			// write to temp - first, write data in (to get the alpha channel quickly and 
			// easily, which will be left well alone by this particular operation...!) 
			temp[y * width + x] = data[y * width + x]; 

			// now write in each element, applying the blend operator.  blend 
			// operators are based on standard OpenGL TexEnv modes, and the 
			// formulae are derived from the OpenGL specs (http://www.opengl.org). 
			for (i = 0; i < 3; i++) 
			{ 
				// divide by 255 so GL operations work as expected 
				float TempTarget; 
				float SrcData = ((float) ((byte *) &data[y * width + x])[i]) / 255.0; 

				switch (GLBlendOperator) 
				{ 
				case GL_ADD: 
					TempTarget = rgbFloat[i] + SrcData; 
					break; 
				case GL_BLEND: 
					// default is FUNC_ADD here 
					// CsS + CdD works out as Src * Dst * 2 
					TempTarget = rgbFloat[i] * SrcData * 2.0; 
					break; 
				case GL_DECAL: 
					// same as GL_REPLACE unless there's alpha, which we ignore for this 
				case GL_REPLACE: 
					TempTarget = rgbFloat[i]; 
					break; 
				case GL_ADD_SIGNED: 
					TempTarget = (rgbFloat[i] + SrcData) - 0.5; 
					break; 
				case GL_MODULATE: 
					// same as default 
				default: 
					TempTarget = rgbFloat[i] * SrcData; 
					break; 
				} 

				// multiply back by 255 to get the proper byte scale 
				TempTarget *= 255.0; 

				// bound the temp target again now, cos the operation may have thrown it out 
				if (TempTarget < 0) TempTarget = 0; 
				if (TempTarget > 255) TempTarget = 255; 
				// and copy it in 
				((byte *) &temp[y * width + x])[i] = (byte) TempTarget; 
			} 
		} 
	} 

	memcpy(data, temp, width * height * 4);
	Z_Free (temp); // release the temp buffer
}

/*
================
R_ImageLightScale
================
*/
void R_ImageLightScale (unsigned *in, int inwidth, int inheight )
{
	int	i, c = inwidth * inheight;
	byte	*p = (byte *)in;

	for (i = 0; i < c; i++, p += 4)
	{
		p[0] = gammatable[p[0]];
		p[1] = gammatable[p[1]];
		p[2] = gammatable[p[2]];
	}
}

void R_RoundImageDimensions(int *scaled_width, int *scaled_height, bool mipmap)
{
	int width = *scaled_width;
	int height = *scaled_height;

	*scaled_width = nearest_pow( *scaled_width );
	*scaled_height = nearest_pow( *scaled_height);

	if (mipmap)
	{
		*scaled_width >>= (int)gl_picmip->value;
		*scaled_height >>= (int)gl_picmip->value;
	}

	if (gl_maxsize->value)
	{
		if (*scaled_width > gl_maxsize->value) *scaled_width = gl_maxsize->value;
		if (*scaled_height > gl_maxsize->value) *scaled_height = gl_maxsize->value;
	}

	if (*scaled_width < 1) *scaled_width = 1;
	if (*scaled_height < 1) *scaled_height = 1;
}

bool R_ResampleTexture (uint *in, int inwidth, int inheight, uint *out, int outwidth, int outheight)
{
	int	i, j;
	uint	frac, fracstep;
	uint	*inrow, *inrow2;
	uint	p1[4096], p2[4096];
	byte	*pix1, *pix2, *pix3, *pix4;

	//check for buffers
	if(!in || !out || in == out) return false;
	if(outheight == 0 || outwidth == 0) return false;

	// nothing to resample ?
	if (inwidth == outwidth && inheight == outheight)
	{
		memcpy(out, in, inwidth * inheight * 4);
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
qglGenerateMipmaps

sgis generate mipmap
===============
*/
void GL_GenerateMipmaps( void )
{
	if( image_desc.flags & IMAGE_GEN_MIPS )
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
		if(qglGetError()) MsgDev(D_WARN, "R_LoadTexImage: can't create mip levels\n");
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
	color16	*color_0, *color_1;
	uint	bits, bitmask, Offset; 
	int	scaled_width, scaled_height;
	word	sAlpha, sColor0, sColor1;
	byte	*fin, *fout = imagebuffer;
	byte	alphas[8], *alpha, *alphamask; 
	int	w, h, x, y, z, i, j, k, Select; 
	uint	*scaled = (unsigned *)uploadbuffer;
	bool	has_alpha = false, mipmap = (image_desc.flags & IMAGE_GEN_MIPS) ? true : false;
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
	case PF_DXT2:
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

		// Can do color & alpha same as dxt3, but color is pre-multiplied 
		// so the result will be wrong unless corrected. 
		if(image_desc.flags & IMAGE_PREMULT) R_ImageCorrectPreMult( (uint *)fout, image_desc.SizeOfData );
		break;
	case PF_DXT4:
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
		// Can do color & alpha same as dxt5, but color is pre-multiplied 
		// so the result will be wrong unless corrected. 
		if(image_desc.flags & IMAGE_PREMULT) R_ImageCorrectPreMult( (uint *)fout, image_desc.SizeOfData );
		break;
	case PF_RXGB:
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

					color_0 = ((color16*)fin);
					color_1 = ((color16*)(fin+2));
					bitmask = ((uint*)fin)[1];
					fin += 8;

					colours[0].r = color_0->r << 3;
					colours[0].g = color_0->g << 2;
					colours[0].b = color_0->b << 3;
					colours[0].a = 0xFF;
					colours[1].r = color_1->r << 3;
					colours[1].g = color_1->g << 2;
					colours[1].b = color_1->b << 3;
					colours[1].a = 0xFF;
					// Four-color block: derive the other two colors.    
					// 00 = color_0, 01 = color_1, 10 = color_2, 11 = color_3
					// These 2-bit codes correspond to the 2-bit fields 
					// stored in the 64-bit block.
					colours[2].b = (2 * colours[0].b + colours[1].b + 1) / 3;
					colours[2].g = (2 * colours[0].g + colours[1].g + 1) / 3;
					colours[2].r = (2 * colours[0].r + colours[1].r + 1) / 3;
					colours[2].a = 0xFF;
					colours[3].b = (colours[0].b + 2 * colours[1].b + 1) / 3;
					colours[3].g = (colours[0].g + 2 * colours[1].g + 1) / 3;
					colours[3].r = (colours[0].r + 2 * colours[1].r + 1) / 3;
					colours[3].a = 0xFF;

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
						alphas[2] = (4 * alphas[0] + 1 * alphas[1] + 2) / 5;	// Bit code 010
						alphas[3] = (3 * alphas[0] + 2 * alphas[1] + 2) / 5;	// Bit code 011
						alphas[4] = (2 * alphas[0] + 3 * alphas[1] + 2) / 5;	// Bit code 100
						alphas[5] = (1 * alphas[0] + 4 * alphas[1] + 2) / 5;	// Bit code 101
						alphas[6] = 0x00;					// Bit code 110
						alphas[7] = 0xFF;					// Bit code 111
					}

					// Note: Have to separate the next two loops,
					// it operates on a 6-byte system.
					// First three bytes

					bits = *((int*)alphamask);
					for (j = 0; j < 2; j++)
					{
						for (i = 0; i < 4; i++)
						{
							// only put pixels out < width or height
							if (((x + i) < w) && ((y + j) < h))
							{
								Offset = z * image_desc.SizeOfPlane + (y + j) * image_desc.bps + (x + i) * image_desc.bpp + 0;
								fout[Offset] = alphas[bits & 0x07];
								if(bits & 0x07) has_alpha = true; 
							}
							bits >>= 3;
						}
					}

					// Last three bytes
					bits = *((int*)&alphamask[3]);

					for (j = 2; j < 4; j++)
					{
						for (i = 0; i < 4; i++)
						{
							// only put pixels out < width or height
							if (((x + i) < w) && ((y + j) < h))
							{
								Offset = z * image_desc.SizeOfPlane + (y + j) * image_desc.bps + (x + i) * image_desc.bpp + 0;
								fout[Offset] = alphas[bits & 0x07];
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
	R_RoundImageDimensions(&scaled_width, &scaled_height, mipmap );

	// upload base image or miplevel
	samples = (has_alpha) ? gl_tex_alpha_format : gl_tex_solid_format;
	R_ResampleTexture ((uint *)fout, w, h, scaled, scaled_width, scaled_height);

	// fake embossmaping
	if (image_desc.type == it_wall && mipmap && r_emboss_bump->value)
		R_FilterTexture (EMBOSS_FILTER, scaled, scaled_width, scaled_height, 1, 128, true, GL_MODULATE);

	R_ImageLightScale(scaled, scaled_width, scaled_height ); //check for round
	qglTexImage2D ( target, level, samples, scaled_width, scaled_height, border, image_desc.glMask, image_desc.glType, (byte *)scaled );

	if(qglGetError()) return false;
	return true;
}

bool CompressedTexImage2D( uint target, int level, int intformat, uint width, uint height, int border, uint imageSize, const void* data )
{
	uint dxtformat = 0;
	uint pixformat = PFDesc[intformat].format;

	if(gl_config.arb_compressed_teximage)
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

	if(use_gl_extension)
	{
		qglCompressedTexImage2D(target, level, dxtformat, width, height, border, imageSize, data );
		if(!qglGetError()) return true;
		// otherwise try loading with software unpacker
	}
	return qrsCompressedTexImage2D(target, level, pixformat, width, height, border, imageSize, data );
}

/*
===============
R_LoadTexImage
===============
*/
bool R_LoadTexImage( uint *data )
{
	int	samples, miplevel = 0;
	int	scaled_width, scaled_height;
	uint	*scaled = (uint *)uploadbuffer;
	bool	mipmap = (image_desc.flags & IMAGE_GEN_MIPS) ? true : false;

	scaled_width = image_desc.width;
	scaled_height = image_desc.height;
	R_RoundImageDimensions(&scaled_width, &scaled_height, mipmap);
        
	samples = (image_desc.flags & IMAGE_HAS_ALPHA) ? gl_tex_alpha_format : gl_tex_solid_format;

	// fake embossmaping
	if (image_desc.type == it_wall && mipmap && r_emboss_bump->value)
		R_FilterTexture (EMBOSS_FILTER, data, scaled_width, scaled_height, 1, 128, true, GL_MODULATE);

	R_ResampleTexture (data, image_desc.width, image_desc.height, scaled, scaled_width, scaled_height);
	R_ImageLightScale(scaled, scaled_width, scaled_height );

	GL_GenerateMipmaps(); //SGIS_ext
	qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, image_desc.glMask, image_desc.glType, scaled);
	GL_TexFilter( mipmap );

          if(qglGetError()) return false;
	return true;
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
	bool mipmap;
	
	for( i = 0; i < image_desc.MipCount; i++, data += size )
	{
		R_SetPixelFormat( w, h, d );
		size = image_desc.SizeOfFile;

		if(!CompressedTexImage2D(GL_TEXTURE_2D, i, image_desc.format, w, h, 0, size, data ))
			break; // there were errors
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1; //calc size of next mip
	}
	mipmap = (i > 1) ? true : false;

	GL_TexFilter( mipmap );
	return true;
}

bool qrsDecompressImageATI( uint target, int level, int internalformat, uint width, uint height, int border, uint imageSize, const void* data )
{
	int	x, y, z, w, h, i, j, k, t1, t2, samples;
	byte	*fin, *fin2, *fout = imagebuffer;
	byte	Colours[8], XColours[8], YColours[8];
	uint	bitmask, bitmask2, CurrOffset, Offset = 0;

	if (!data) return false;

	fin = (byte *)data;

	w = width;
	h = height;

	switch( PFDesc[internalformat].format )
	{
	case PF_ATI1N:
		for (z = 0; z < image_desc.numLayers; z++)
		{
			for (y = 0; y < h; y += 4)
			{
				for (x = 0; x < w; x += 4)
				{
					//Read palette
					t1 = Colours[0] = fin[0];
					t2 = Colours[1] = fin[1];
					fin += 2;

					if (t1 > t2)
					{
						for (i = 2; i < 8; ++i)
							Colours[i] = t1 + ((t2 - t1)*(i - 1))/7;
					}
					else
					{
						for (i = 2; i < 6; ++i)
							Colours[i] = t1 + ((t2 - t1)*(i - 1))/5;
						Colours[6] = 0;
						Colours[7] = 0xFF;
					}
					//decompress pixel data
					CurrOffset = Offset;
					for (k = 0; k < 4; k += 2)
					{
						// First three bytes
						bitmask = ((uint)(fin[0]) << 0) | ((uint)(fin[1]) << 8) | ((uint)(fin[2]) << 16);

						for (j = 0; j < 2; j++)
						{
							// only put pixels out < height
							if ((y + k + j) < h)
							{
								for (i = 0; i < 4; i++)
								{
									// only put pixels out < width
									if (((x + i) < w))
									{
										t1 = CurrOffset + (x + i);
										fout[t1] = Colours[bitmask & 0x07];
									}
									bitmask >>= 3;
								}
								CurrOffset += image_desc.bps;
							}
						}
						fin += 3;
					}
				}
				Offset += image_desc.bps * 4;
			}
		}
		break;
	case PF_ATI2N:
		for (z = 0; z < image_desc.numLayers; z++)
		{
			for (y = 0; y < h; y += 4)
			{
				for (x = 0; x < w; x += 4)
				{
					fin2 = fin + 8;

					//Read Y palette
					t1 = YColours[0] = fin[0];
					t2 = YColours[1] = fin[1];
					fin += 2;

					if (t1 > t2)
					{
						for (i = 2; i < 8; ++i)
							YColours[i] = t1 + ((t2 - t1)*(i - 1))/7;
					}
					else
					{
						for (i = 2; i < 6; ++i)
							YColours[i] = t1 + ((t2 - t1)*(i - 1))/5;
						YColours[6] = 0;
						YColours[7] = 0xFF;
					}

					// Read X palette
					t1 = XColours[0] = fin2[0];
					t2 = XColours[1] = fin2[1];
					fin2 += 2;

					if (t1 > t2)
					{
						for (i = 2; i < 8; ++i)
							XColours[i] = t1 + ((t2 - t1)*(i - 1))/7;
					}
					else
					{
						for (i = 2; i < 6; ++i)
							XColours[i] = t1 + ((t2 - t1)*(i - 1))/5;
						XColours[6] = 0;
						XColours[7] = 0xFF;
					}
					//decompress pixel data
					CurrOffset = Offset;

					for (k = 0; k < 4; k += 2)
					{
						// First three bytes
						bitmask = ((uint)(fin[0]) << 0) | ((uint)(fin[1]) << 8) | ((uint)(fin[2]) << 16);
						bitmask2 = ((uint)(fin2[0]) << 0) | ((uint)(fin2[1]) << 8) | ((uint)(fin2[2]) << 16);

						for (j = 0; j < 2; j++)
						{
							// only put pixels out < height
							if ((y + k + j) < h)
							{
								for (i = 0; i < 4; i++)
								{
									// only put pixels out < width
									if (((x + i) < w))
									{
										int t, tx, ty;

										t1 = CurrOffset + (x + i)*3;
										fout[t1 + 1] = ty = YColours[bitmask & 0x07];
										fout[t1 + 0] = tx = XColours[bitmask2 & 0x07];

										//calculate b (z) component ((r/255)^2 + (g/255)^2 + (b/255)^2 = 1
										t = 127*128 - (tx - 127)*(tx - 128) - (ty - 127)*(ty - 128);

										if (t > 0) fout[t1 + 2] = (byte)(sqrt(t) + 128);
										else fout[t1 + 2] = 0x7F;
									}
									bitmask >>= 3;
									bitmask2 >>= 3;
								}
								CurrOffset += image_desc.bps;
							}
						}
						fin += 3;
						fin2 += 3;
					}
					//skip bytes that were read via Temp2
					fin += 8;
				}
				Offset += image_desc.bps * 4;
			}
		}
		break;
	default:
		MsgDev(D_WARN, "qrsDecompressImageATI: invalid compression type: %s\n", PFDesc[internalformat].name );
		return false;
	}

	// ati images always have power of two sizes
	R_ImageLightScale((uint *)fout, width, height ); //check for round

	// upload base image or miplevel
	samples = (image_desc.flags & IMAGE_HAS_ALPHA) ? gl_tex_alpha_format : gl_tex_solid_format;
	qglTexImage2D ( target, level, samples, w, h, border, GL_RGBA, GL_UNSIGNED_BYTE, fout );

	if(qglGetError()) return false;
	return true;
}

/*
===============
R_LoadImageATI
===============
*/
bool R_LoadImageATI( byte *data )
{
	int i, size = 0;
	int w = image_desc.width;
	int h = image_desc.height;
	int d = image_desc.numLayers;
	bool mipmap;
	
	for( i = 0; i < image_desc.MipCount; i++, data += size )
	{
		R_SetPixelFormat( w, h, d );
		size = image_desc.SizeOfFile;

		if(!qrsDecompressImageATI(GL_TEXTURE_2D, i, image_desc.format, w, h, 0, size, data ))
			break; // there were errors
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1; //calc size of next mip
	}
	mipmap = (i > 1) ? true : false;

	GL_TexFilter( mipmap );
	return true;
}

bool R_StoreImageFloat( uint target, int level, uint width, uint height, uint imageSize, const void* data )
{
	float	*dest = (float *)imagebuffer;
	word	*src = (word *)data; 
	int	samples;

	if(!data) return false;

	memcpy( dest, src, image_desc.SizeOfData );
	samples = (image_desc.flags & IMAGE_HAS_ALPHA) ? gl_tex_alpha_format : gl_tex_solid_format;
	qglTexImage2D (target, level, samples, width, height, 0, GL_RGBA, GL_FLOAT, dest );

	if(qglGetError()) return false;
	return true;
}

bool R_DecompressImageFloat( byte *data )
{
	int i, size = 0;
	int w = image_desc.width;
	int h = image_desc.height;
	int d = image_desc.numLayers;
	bool mipmap;

	for( i = 0; i < image_desc.MipCount; i++, data += size )
	{
		R_SetPixelFormat( w, h, d );
		size = image_desc.SizeOfFile;
		if(!R_StoreImageFloat(GL_TEXTURE_2D, i, w, h, size, data )) break; 
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1; //calc size of next mip
	}
	mipmap = (i > 1) ? true : false;

	GL_TexFilter( mipmap );
	return true;
}

bool R_StoreImageARGB( uint target, int level, uint width, uint height, uint imageSize, const void* data )
{
	uint	ReadI = 0, TempBpp;
	uint	RedL, RedR, GreenL, GreenR, BlueL, BlueR, AlphaL, AlphaR;
	uint	r_bitmask, g_bitmask, b_bitmask, a_bitmask;
	uint	*scaled = (unsigned *)uploadbuffer;
	byte	*fin, *fout = imagebuffer;
	bool	has_alpha = false, mipmap = (image_desc.flags & IMAGE_GEN_MIPS) ? true : false;
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
			//less than 4 byte to write?
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
	R_RoundImageDimensions(&scaled_width, &scaled_height, mipmap );

	// upload base image or miplevel
	samples = (image_desc.flags & IMAGE_HAS_ALPHA) ? gl_tex_alpha_format : gl_tex_solid_format;
	R_ResampleTexture ((uint *)fout, w, h, scaled, scaled_width, scaled_height);

	// fake embossmaping
	if (image_desc.type == it_wall && mipmap && r_emboss_bump->value)
		R_FilterTexture (EMBOSS_FILTER, scaled, scaled_width, scaled_height, 1, 128, true, GL_MODULATE);

	R_ImageLightScale(scaled, scaled_width, scaled_height ); //check for round
	qglTexImage2D ( target, level, samples, scaled_width, scaled_height, 0, image_desc.glMask, image_desc.glType, scaled );

	if(qglGetError()) return false;
	return true;
}

bool R_LoadImageARGB( byte *data )
{
	int i, size = 0;
	int w = image_desc.width;
	int h = image_desc.height;
	int d = image_desc.numLayers;
	bool mipmap;
	
	for( i = 0; i < image_desc.MipCount; i++, data += size )
	{
		R_SetPixelFormat( w, h, d );
		size = image_desc.SizeOfFile;

		if(use_gl_extension)
		{
			qglTexImage2D( GL_TEXTURE_2D, i, GL_RGB5_A1, w, h, 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, data );
			if(qglGetError()) break; // there were errors
		}
		else if(!R_StoreImageARGB(GL_TEXTURE_2D, i, w, h, size, data )) break; 
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1; //calc size of next mip
	}
	mipmap = (i > 1) ? true : false;

	GL_TexFilter( mipmap );
	return true;
}

/*
===============
R_LoadImageABGR
===============
*/
bool R_LoadImageABGR( byte *data )
{
	byte	*trans = imagebuffer;
	int	i, s = image_desc.width * image_desc.height;

	if (s&3)
	{
		MsgDev(D_ERROR, "R_LoadImageABGR: s&3\n");
		return false;
	}

	// swap alpha and red
	for (i = 0; i < s; i++ )
	{
		trans[(i<<2)+0] = data[(i<<2)+3];
		trans[(i<<2)+1] = data[(i<<2)+2];
		trans[(i<<2)+2] = data[(i<<2)+1];
		trans[(i<<2)+3] = data[(i<<2)+0];
	}
	return R_LoadTexImage((uint*)trans );
}

/*
===============
R_LoadImageRGBA
===============
*/
bool R_LoadImageRGBA (byte *data )
{
	byte	*trans = imagebuffer;
	int	i, s = image_desc.width * image_desc.height;

	// nothing to process
	if(!image_desc.pal) return R_LoadTexImage((uint*)data );

	if (s&3)
	{
		MsgDev(D_ERROR, "R_LoadImageRGBA: s&3\n");
		return false;
	}
	for (i = 0; i < s; i++ )
	{
		trans[(i<<2)+0] = gammatable[image_desc.pal[data[i]*4+0]];
		trans[(i<<2)+1] = gammatable[image_desc.pal[data[i]*4+1]];
		trans[(i<<2)+2] = gammatable[image_desc.pal[data[i]*4+2]];
		trans[(i<<2)+3] = gammatable[image_desc.pal[data[i]*4+3]];
	}
	return R_LoadTexImage((uint*)trans );
}

/*
===============
R_LoadImageRGB
===============
*/
bool R_LoadImageRGB(byte *data )
{
	byte	*trans = imagebuffer;
	int	i, s = image_desc.width * image_desc.height;
	bool	noalpha;
	int	p;

	if (s&3) 
	{
		MsgDev(D_ERROR, "R_LoadImageRGB: s&3\n");
		return false;
	}

	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (image_desc.flags & IMAGE_HAS_ALPHA)
	{
		noalpha = true;
		if(image_desc.pal)
		{
			for (i=0 ; i<s ; i++)
			{
				p = data[i];
				if (p == 255) noalpha = false;
				trans[(i<<2)+0] = image_desc.pal[p*3+0];
				trans[(i<<2)+1] = image_desc.pal[p*3+1];
				trans[(i<<2)+2] = image_desc.pal[p*3+2];
				trans[(i<<2)+3] = (p==255) ? 0 : 255;
			}
			if (noalpha) image_desc.flags &= ~IMAGE_HAS_ALPHA;
		}
		else
		{
			for (i = 0; i < s; i++)
			{
				p = data[i];
				if (p == 0) noalpha = false;
				trans[(i<<2)+0] = data[i+0];
				trans[(i<<2)+1] = data[i+1];
				trans[(i<<2)+2] = data[i+2];
				trans[(i<<2)+3] = (p==0) ? 0 : 255;
			}
			if (noalpha) image_desc.flags &= ~IMAGE_HAS_ALPHA;
		}
	}
	else
	{
		if(image_desc.pal)
		{
			for (i = 0; i < s; i+=1)
			{
				trans[(i<<2)+0] = image_desc.pal[data[i]*3+0];
				trans[(i<<2)+1] = image_desc.pal[data[i]*3+1];
				trans[(i<<2)+2] = image_desc.pal[data[i]*3+2];
				trans[(i<<2)+3] = 255;
			}
		}
		else
		{
			for (i = 0; i < s; i+=1)
			{
				trans[(i<<2)+0] = data[i+0];
				trans[(i<<2)+1] = data[i+1];
				trans[(i<<2)+2] = data[i+2];
				trans[(i<<2)+3] = 255;
			}
		}
	}
	return R_LoadTexImage((uint*)trans );
}

/*
===============
R_FindImage

Finds or loads the given image
===============
*/
image_t *R_FindImage (char *name, char *buffer, int size, imagetype_t type)
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

	pic = FS_LoadImage(name, buffer, size ); //loading form disk or buffer
	image = R_LoadImage(name, pic, type ); //upload into video buffer
	FS_FreeImage(pic ); //free image

	return image;
}


/*
================
R_LoadImage

This is also used as an entry point for the generated r_notexture
================
*/
image_t *R_LoadImage(char *name, rgbdata_t *pic, imagetype_t type )
{
	image_t	*image;
          bool	iResult = true;
	int	i, numsides = 1, width, height;
	uint	offset = 0;
	int	skyorder_q2[6] = { 2, 3, 1, 0, 4, 5, }; // Quake, Half-Life skybox ordering
	int	skyorder_ms[6] = { 4, 5, 1, 0, 2, 3  }; // Microsoft DDS ordering (reverse)
	byte	*buf;
	
	//nothing to load
	if (!pic || !pic->buffer) return NULL;

	// find a free image_t
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->texnum[0]) break;
	}
	if (i == numgltextures)
	{
		if (numgltextures == MAX_GLTEXTURES)
		{
			MsgDev(D_ERROR, "R_LoadImage: gl_textures limit is out\n");
			return NULL;
		}
		numgltextures++;
	}
	image = &gltextures[i];

	if (strlen(name) >= sizeof(image->name)) MsgDev( D_WARN, "R_LoadImage: \"%s\" is too long", name);

	strncpy (image->name, name, sizeof(image->name));
	image->registration_sequence = registration_sequence;

	if(pic->flags & IMAGE_CUBEMAP) 
	{
		numsides = 6;
		if(pic->flags & IMAGE_CUBEMAP_FLIP)
			memcpy(image->texorder, skyorder_ms, sizeof(int) * 6 );
		else memcpy(image->texorder, skyorder_q2, sizeof(int) * 6 );
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
		GL_Bind(image->texnum[i]);
		
		R_SetPixelFormat( image_desc.width, image_desc.height, image_desc.numLayers );
		offset = image_desc.SizeOfFile;// move pointer
		
		MsgDev(D_LOAD, "loading %s [%s] \n", name, PFDesc[image_desc.format].name );

		switch(pic->type)
		{
		case PF_INDEXED_24:	iResult = R_LoadImageRGB( buf ); break;
		case PF_INDEXED_32: iResult = R_LoadImageRGBA( buf ); break;
		case PF_RGB_24: iResult = R_LoadImageRGB( buf ); break;
		case PF_ABGR_64:
		case PF_RGBA_32: 
		case PF_RGBA_GN: iResult = R_LoadTexImage((uint*)buf ); break;
		case PF_LUMINANCE:
		case PF_LUMINANCE_16:
		case PF_LUMINANCE_ALPHA:
		case PF_ARGB_32: iResult = R_LoadImageARGB( buf ); break;
		case PF_RXGB:
		case PF_DXT1:
		case PF_DXT2:
		case PF_DXT3:
		case PF_DXT4:
		case PF_DXT5: iResult = R_LoadImageDXT( buf ); break;
		case PF_ATI1N:
		case PF_ATI2N: iResult = R_LoadImageATI( buf ); break;
		case PF_UNKNOWN: image = r_notexture; break;
		}
	}          

	//check for errors
	if(!iResult)
	{
		MsgWarn("R_LoadImage: can't loading %s with bpp %d\n", name, image_desc.bpp ); 
		image->name[0] = '\0';
		image->registration_sequence = 0;
		return NULL;
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
			for(k = 0; k < 6; k++) qglDeleteTextures (1, &image->texnum[k] );
		else qglDeleteTextures (1, &image->texnum[0] );
		memset (image, 0, sizeof(*image));
	}
}
