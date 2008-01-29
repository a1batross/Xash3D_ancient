//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_dds.c - dds format load & save
//=======================================================================

#include "imagelib.h"
#include "img_formats.h"

#define Sum(c) ((c)->r + (c)->g + (c)->b)

/*
====================
Image_DXTReadColors

read colors from dxt image
====================
*/
void Image_DXTReadColors( const byte* data, color32* out )
{
	byte r0, g0, b0, r1, g1, b1;

	b0 = data[0] & 0x1F;
	g0 = ((data[0] & 0xE0) >> 5) | ((data[1] & 0x7) << 3);
	r0 = (data[1] & 0xF8) >> 3;

	b1 = data[2] & 0x1F;
	g1 = ((data[2] & 0xE0) >> 5) | ((data[3] & 0x7) << 3);
	r1 = (data[3] & 0xF8) >> 3;

	out[0].r = r0<<3;
	out[0].g = g0<<2;
	out[0].b = b0<<3;

	out[1].r = r1<<3;
	out[1].g = g1<<2;
	out[1].b = b1<<3;
}

/*
====================
Image_DXTReadColor

read one color from dxt image
====================
*/
void Image_DXTReadColor( word data, color32* out )
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
Image_GetBitsFromMask
====================
*/
void Image_GetBitsFromMask( uint Mask, uint *ShiftLeft, uint *ShiftRight )
{
	uint Temp, i;

	if( Mask == 0 )
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
		if(!(Temp & 1)) break;
	}
	*ShiftLeft = 8 - i;
}

void Image_ShortToColor565( word Pixel, color16 *Colour )
{
	Colour->r = (Pixel & 0xF800) >> 11;
	Colour->g = (Pixel & 0x07E0) >> 5;
	Colour->b = (Pixel & 0x001F);
}


void Image_ShortToColor888( word Pixel, color24 *Colour )
{
	Colour->r = ((Pixel & 0xF800) >> 11) << 3;
	Colour->g = ((Pixel & 0x07E0) >> 5)  << 2;
	Colour->b = ((Pixel & 0x001F))       << 3;
}

word Image_Color565ToShort( color16 *Colour )
{
	return (Colour->r << 11) | (Colour->g << 5) | (Colour->b);
}

word Image_Color888ToShort( color24 *Colour )
{
	return ((Colour->r >> 3) << 11) | ((Colour->g >> 2) << 5) | (Colour->b >> 3);
}

void Image_ChooseEndpoints( word *Block, word *ex0, word *ex1 )
{
	uint	i;
	color24	Colours[16];
	int	Lowest = 0, Highest = 0;

	for (i = 0; i < 16; i++)
	{
		Image_ShortToColor888(Block[i], &Colours[i]);
		if(Sum(&Colours[i]) < Sum(&Colours[Lowest])) Lowest = i;
		if(Sum(&Colours[i]) > Sum(&Colours[Highest])) Highest = i;
	}
	*ex0 = Block[Highest];
	*ex1 = Block[Lowest];
}

void Image_CorrectEndDXT1( word *ex0, word *ex1, bool HasAlpha )
{
	word	Temp;

	if( HasAlpha )
	{
		if(*ex0 > *ex1)
		{
			Temp = *ex0;
			*ex0 = *ex1;
			*ex1 = Temp;
		}
	}
	else
	{
		if (*ex0 < *ex1)
		{
			Temp = *ex0;
			*ex0 = *ex1;
			*ex1 = Temp;
		}
	}
}

void Image_ChooseAlphaEndpoints( byte *Block, byte *a0, byte *a1 )
{
	uint	i, Lowest = 0xFF, Highest = 0;

	for (i = 0; i < 16; i++)
	{
		if( Block[i] < Lowest ) Lowest = Block[i];
		if( Block[i] > Highest) Highest = Block[i];
	}
	*a0 = Lowest;
	*a1 = Highest;
}

// Assumed to be 16-bit (5:6:5).
bool Image_GetBlock( word *Block, word *Data, rgbdata_t *pix, uint XPos, uint YPos )
{
	uint x, y, i = 0, Offset = YPos * pix->width + XPos;

	for( y = 0; y < 4; y++)
	{
		for (x = 0; x < 4; x++)
		{
			if(x < pix->width && y < pix->height)
				Block[i++] = Data[Offset + x];
			else Block[i++] = Data[Offset];
		}
		Offset += pix->width;
	}
	return true;
}

bool Image_GetAlphaBlock( byte *Block, byte *Data, rgbdata_t *pix, uint XPos, uint YPos )
{
	uint x, y, i = 0, Offset = YPos * pix->width + XPos;

	for (y = 0; y < 4; y++)
	{
		for (x = 0; x < 4; x++)
		{
			if (x < pix->width && y < pix->height)
				Block[i++] = Data[Offset + x];
			else Block[i++] = Data[Offset];
		}
		Offset += pix->width;
	}
	return true;
}

size_t Image_DXTGetLinearSize( int image_type, int width, int height, int depth, int rgbcount )
{
	size_t BlockSize = 0;
	int block, bpp;

	// right calcualte blocksize
	block = PFDesc[image_type].block;
	bpp = PFDesc[image_type].bpp;

	if( block == 0 ) BlockSize = width * height * bpp;
	else if(block > 0) BlockSize = ((width + 3)/4) * ((height + 3)/4) * depth * block;
	else if(block < 0 && rgbcount > 0) BlockSize = width * height * depth * rgbcount;
	else BlockSize = width * height * abs(block);

	return BlockSize;
}

bool Image_DXTWriteHeader( vfile_t *f, rgbdata_t *pix, uint cubemap_flags, uint savetype )
{
	uint	dwFourCC, dwFlags1 = 0, dwFlags2 = 0, dwCaps1 = 0;
	uint	dwLinearSize, dwBlockSize, dwCaps2 = 0;
	uint	dwIdent = DDSHEADER, dwSize = 124, dwSize2 = 32;
	uint	dwWidth, dwHeight, dwDepth, dwMipCount = 1;

	if(!pix || !pix->buffer )
		return false;

	// setup flags
	dwFlags1 |= DDS_LINEARSIZE | DDS_WIDTH | DDS_HEIGHT | DDS_CAPS | DDS_PIXELFORMAT;
	dwFlags2 |= DDS_FOURCC;
	if( pix->numLayers > 1) dwFlags1 |= DDS_DEPTH;

	switch( savetype )
	{
	case PF_DXT1:
		dwFourCC = TYPE_DXT1;
		break;
	case PF_DXT3:
		dwFourCC = TYPE_DXT3;
		break;
	case PF_DXT5:
		dwFourCC = TYPE_DXT5;
		break;
	default:
		MsgDev( D_ERROR, "Image_DXTWriteHeader: unsupported type %s\n", PFDesc[pix->type].name );	
		return false;
	}
	dwWidth = pix->width;
	dwHeight = pix->height;

	VFS_Write(f, &dwIdent, sizeof(uint));
	VFS_Write(f, &dwSize, sizeof(uint));
	VFS_Write(f, &dwFlags1, sizeof(uint));
	VFS_Write(f, &dwHeight, sizeof(uint));
	VFS_Write(f, &dwWidth, sizeof(uint));

	dwBlockSize = PFDesc[savetype].block;
	dwLinearSize = Image_DXTGetLinearSize( savetype, pix->width, pix->height, pix->numLayers, pix->bitsCount / 8 );
	VFS_Write(f, &dwLinearSize, sizeof(uint)); // TODO: use dds_get_linear_size

	if( pix->numLayers > 1 )
	{
		dwDepth = pix->numLayers;
		VFS_Write(f, &dwDepth, sizeof(uint));
		dwCaps2 |= DDS_VOLUME;
	}
	else VFS_Write(f, 0, sizeof(uint));

	VFS_Write(f, &dwMipCount, sizeof(uint));
	VFS_Write(f, 0, sizeof(uint));
	VFS_Write(f, pix->color, sizeof(vec3_t));
	VFS_Write(f, &pix->bump_scale, sizeof(float));
	VFS_Write(f, 0, sizeof(uint) * 6 ); // reserved fields

	VFS_Write(f, &dwSize2, sizeof(uint));
	VFS_Write(f, &dwFlags2, sizeof(uint));
	VFS_Write(f, &dwFourCC, sizeof(uint));
	VFS_Write(f, 0, sizeof(uint) * 5 ); // bit masks

	dwCaps1 |= DDS_TEXTURE;
	if( pix->numMips > 1 ) dwCaps1 |= DDS_MIPMAP | DDS_COMPLEX;
	if( cubemap_flags )
	{
		dwCaps1 |= DDS_COMPLEX;
		dwCaps2 |= cubemap_flags;
	}

	VFS_Write(f, &dwCaps1, sizeof(uint));
	VFS_Write(f, &dwCaps2, sizeof(uint));
	VFS_Write(f, 0, sizeof(uint) * 3 ); // other caps and TextureStage

	return true;
}

byte* Image_GetAlpha( rgbdata_t *pix )
{
	byte	*Alpha;
	uint	i, j, Bpc, Size, AlphaOff;

	Bpc = PFDesc[pix->type].bpc;
	if( Bpc == 0 ) return NULL;

	Size = pix->width * pix->height * pix->numLayers * PFDesc[pix->type].bpp;
	Alpha = (byte*)Mem_Alloc( zonepool, Size / PFDesc[pix->type].bpp * Bpc);

	switch( pix->type )
	{
		case PF_RGB_24:
		case PF_RGB_24_FLIP:
		case PF_LUMINANCE:
			memset( Alpha, 0xFF, Size / PFDesc[pix->type].bpp * Bpc );
			return Alpha;
	}

	if( pix->type == PF_LUMINANCE_ALPHA )
		AlphaOff = 2;
	else AlphaOff = 4;

	for (i = AlphaOff - 1, j = 0; i < Size; i += AlphaOff, j++ )
		Alpha[j] = pix->buffer[i];

	return Alpha;
}

uint Image_RMSAlpha( byte *Orig, byte *Test )
{
	uint	RMS = 0, i;
	int	d;

	for (i = 0; i < 16; i++)
	{
		d = Orig[i] - Test[i];
		RMS += d*d;
	}
	return RMS;
}

uint Image_DXTDistance(color24 *c1, color24 *c2)
{
	return (c1->r-c2->r)*(c1->r-c2->r)+(c1->g-c2->g)*(c1->g-c2->g)+(c1->b-c2->b)*(c1->b-c2->b);
}

uint Image_GenBitMask( word ex0, word ex1, uint NumCols, word *In, byte *Alpha, color24 *OutCol )
{
	uint		i, j, Closest, Dist, BitMask = 0;
	byte		Mask[16];
	color24		c, Colours[4];

	Image_ShortToColor888(ex0, &Colours[0]);
	Image_ShortToColor888(ex1, &Colours[1]);
	if (NumCols == 3)
	{
		Colours[2].r = (Colours[0].r + Colours[1].r) / 2;
		Colours[2].g = (Colours[0].g + Colours[1].g) / 2;
		Colours[2].b = (Colours[0].b + Colours[1].b) / 2;
		Colours[3].r = (Colours[0].r + Colours[1].r) / 2;
		Colours[3].g = (Colours[0].g + Colours[1].g) / 2;
		Colours[3].b = (Colours[0].b + Colours[1].b) / 2;
	}
	else
	{	// NumCols == 4
		Colours[2].r = (2 * Colours[0].r + Colours[1].r + 1) / 3;
		Colours[2].g = (2 * Colours[0].g + Colours[1].g + 1) / 3;
		Colours[2].b = (2 * Colours[0].b + Colours[1].b + 1) / 3;
		Colours[3].r = (Colours[0].r + 2 * Colours[1].r + 1) / 3;
		Colours[3].g = (Colours[0].g + 2 * Colours[1].g + 1) / 3;
		Colours[3].b = (Colours[0].b + 2 * Colours[1].b + 1) / 3;
	}

	for (i = 0; i < 16; i++)
	{
		if (Alpha)
		{
			// Test to see if we have 1-bit transparency
			if (Alpha[i] < 128)
			{
				Mask[i] = 3;  // Transparent
				if (OutCol)
				{
					OutCol[i].r = Colours[3].r;
					OutCol[i].g = Colours[3].g;
					OutCol[i].b = Colours[3].b;
				}
				continue;
			}
		}

		// if no transparency, try to find which colour is the closest.
		Closest = UINT_MAX;
		Image_ShortToColor888(In[i], &c);
		for (j = 0; j < NumCols; j++)
		{
			Dist = Image_DXTDistance(&c, &Colours[j]);
			if( Dist < Closest )
			{
				Closest = Dist;
				Mask[i] = j;
				if( OutCol )
				{
					OutCol[i].r = Colours[j].r;
					OutCol[i].g = Colours[j].g;
					OutCol[i].b = Colours[j].b;
				}
			}
		}
	}

	for( i = 0; i < 16; i++ )
	{
		BitMask |= (Mask[i] << (i*2));
	}
	return BitMask;
}


void Image_GenAlphaBitMask( byte a0, byte a1, byte *In, byte *Mask, byte *Out )
{
	byte 	Alphas[8], M[16];
	uint	i, j, Closest, Dist;

	Alphas[0] = a0;
	Alphas[1] = a1;

	// 8-alpha or 6-alpha block?
	if (a0 > a1)
	{
		// 8-alpha block:  derive the other six alphas.
		// Bit code 000 = alpha_0, 001 = alpha_1, others are interpolated.
		Alphas[2] = (6 * Alphas[0] + 1 * Alphas[1] + 3) / 7;	// bit code 010
		Alphas[3] = (5 * Alphas[0] + 2 * Alphas[1] + 3) / 7;	// bit code 011
		Alphas[4] = (4 * Alphas[0] + 3 * Alphas[1] + 3) / 7;	// bit code 100
		Alphas[5] = (3 * Alphas[0] + 4 * Alphas[1] + 3) / 7;	// bit code 101
		Alphas[6] = (2 * Alphas[0] + 5 * Alphas[1] + 3) / 7;	// bit code 110
		Alphas[7] = (1 * Alphas[0] + 6 * Alphas[1] + 3) / 7;	// bit code 111
	}
	else
	{
		// 6-alpha block.
		// Bit code 000 = alpha_0, 001 = alpha_1, others are interpolated.
		Alphas[2] = (4 * Alphas[0] + 1 * Alphas[1] + 2) / 5;	// Bit code 010
		Alphas[3] = (3 * Alphas[0] + 2 * Alphas[1] + 2) / 5;	// Bit code 011
		Alphas[4] = (2 * Alphas[0] + 3 * Alphas[1] + 2) / 5;	// Bit code 100
		Alphas[5] = (1 * Alphas[0] + 4 * Alphas[1] + 2) / 5;	// Bit code 101
		Alphas[6] = 0x00;										// Bit code 110
		Alphas[7] = 0xFF;										// Bit code 111
	}

	for (i = 0; i < 16; i++)
	{
		Closest = UINT_MAX;
		for (j = 0; j < 8; j++)
		{
			Dist = abs((int)In[i] - (int)Alphas[j]);
			if (Dist < Closest)
			{
				Closest = Dist;
				M[i] = j;
			}
		}
	}

	if( Out )
	{
		for (i = 0; i < 16; i++)
		{
			Out[i] = Alphas[M[i]];
		}
	}

	// First three bytes.
	Mask[0] = (M[0]) | (M[1] << 3) | ((M[2] & 0x03) << 6);
	Mask[1] = ((M[2] & 0x04) >> 2) | (M[3] << 1) | (M[4] << 4) | ((M[5] & 0x01) << 7);
	Mask[2] = ((M[5] & 0x06) >> 1) | (M[6] << 2) | (M[7] << 5);

	// Second three bytes.
	Mask[3] = (M[8]) | (M[9] << 3) | ((M[10] & 0x03) << 6);
	Mask[4] = ((M[10] & 0x04) >> 2) | (M[11] << 1) | (M[12] << 4) | ((M[13] & 0x01) << 7);
	Mask[5] = ((M[13] & 0x06) >> 1) | (M[14] << 2) | (M[15] << 5);
}

/*
===============
Image_DecompressDXTC

Warning: this version will be kill all mipmaps or cubemap sides
Not for game rendering
===============
*/
bool Image_DecompressDXTC( rgbdata_t **image )
{
	color32	colours[4], *col;
	uint	bits, bitmask, Offset; 
	word	sAlpha, sColor0, sColor1;
	byte	alphas[8], *alpha, *alphamask; 
	int	w, h, x, y, z, i, j, k, Select; 
	bool	has_alpha = false;
	byte	*fin, *fout;
	int	SizeOfPlane, Bpp, Bps; 
	rgbdata_t	*pix = *image;

	if( !pix || !pix->buffer )
		return false;

	fin = (byte *)pix->buffer;
	w = pix->width;
	h = pix->height;
	Bpp = PFDesc[pix->type].bpp;
	Bps = pix->width * Bpp * PFDesc[pix->type].bpc;
	SizeOfPlane = Bps * pix->height;
	fout = Mem_Alloc( zonepool, pix->width * pix->height * 4 );
	
	switch( pix->type )
	{
	case PF_DXT1:
		colours[0].a = 0xFF;
		colours[1].a = 0xFF;
		colours[2].a = 0xFF;

		for (z = 0; z < pix->numLayers; z++)
		{
			for (y = 0; y < h; y += 4)
			{
				for (x = 0; x < w; x += 4)
				{
					sColor0 = *((word*)fin);
					sColor0 = LittleShort(sColor0);
					sColor1 = *((word*)(fin + 2));
					sColor1 = LittleShort(sColor1);

					Image_DXTReadColor(sColor0, colours);
					Image_DXTReadColor(sColor1, colours + 1);

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
								uint ofs = z * SizeOfPlane + (y + j) * Bps + (x + i) * Bpp;
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
		for (z = 0; z < pix->numLayers; z++)
		{
			for (y = 0; y < h; y += 4)
			{
				for (x = 0; x < w; x += 4)
				{
					alpha = fin;
					fin += 8;
					Image_DXTReadColors(fin, colours);
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
								Offset = z * SizeOfPlane + (y + j) * Bps + (x + i) * Bpp;
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
								Offset = z * SizeOfPlane + (y + j) * Bps + (x + i) * Bpp + 3;
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
		for (z = 0; z < pix->numLayers; z++)
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

					Image_DXTReadColors(fin, colours);
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
								Offset = z * SizeOfPlane + (y + j) * Bps + (x + i) * Bpp;
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
								Offset = z * SizeOfPlane + (y + j) * Bps + (x + i) * Bpp + 3;
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
								Offset = z * SizeOfPlane + (y + j) * Bps + (x + i) * Bpp + 3;
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
		MsgDev(D_WARN, "qrsCompressedTexImage2D: invalid compression type: %s\n", PFDesc[pix->type].name );
		return false;
	}

	// update image parms
	pix->size = pix->width * pix->height * 4;
	Mem_Free( pix->buffer );
	pix->buffer = fout;
	pix->type = PF_RGBA_32;

	*image = pix;
	return true;
}

/*
===============
Image_DecompressARGB

Warning: this version will be kill all mipmaps or cubemap sides
Not for game rendering
===============
*/
bool Image_DecompressARGB( rgbdata_t **image )
{
	uint	ReadI = 0, TempBpp;
	uint	RedL, RedR, GreenL, GreenR, BlueL, BlueR, AlphaL, AlphaR;
	uint	r_bitmask, g_bitmask, b_bitmask, a_bitmask;
	bool	has_alpha = false;
	byte	*fin, *fout;
	int	SizeOfPlane, Bpp, Bps; 
	rgbdata_t	*pix = *image;
	int	i, w, h;

	if( !pix || !pix->buffer )
		return false;

	fin = (byte *)pix->buffer;
	w = pix->width;
	h = pix->height;
	Bpp = PFDesc[pix->type].bpp;
	Bps = pix->width * Bpp * PFDesc[pix->type].bpc;
	SizeOfPlane = Bps * pix->height;

	if( pix->palette )
	{
		byte *pal = pix->palette; // copy ptr
		r_bitmask	= BuffLittleLong( pal ); pal += 4;
		g_bitmask = BuffLittleLong( pal ); pal += 4;
		b_bitmask = BuffLittleLong( pal ); pal += 4;
		a_bitmask = BuffLittleLong( pal ); pal += 4;
	}
	else
	{
		MsgDev(D_ERROR, "Image_DecompressARGB: can't get RGBA bitmask\n" );		
		return false;
	}

	fout = Mem_Alloc( zonepool, pix->width * pix->height * 4 );
	TempBpp = pix->bitsCount / 8;
	
	Image_GetBitsFromMask(r_bitmask, &RedL, &RedR);
	Image_GetBitsFromMask(g_bitmask, &GreenL, &GreenR);
	Image_GetBitsFromMask(b_bitmask, &BlueL, &BlueR);
	Image_GetBitsFromMask(a_bitmask, &AlphaL, &AlphaR);

	for( i = 0; i < SizeOfPlane * pix->numLayers; i += Bpp )
	{
		// TODO: This is SLOOOW...
		// but the old version crashed in release build under
		// winxp (and xp is right to stop this code - I always
		// wondered that it worked the old way at all)
		if( SizeOfPlane * pix->numLayers - i < 4 )
		{
			// less than 4 byte to write?
			if (TempBpp == 1) ReadI = *((byte*)fin);
			else if (TempBpp == 2) ReadI = BuffLittleShort( fin );
			else if (TempBpp == 3) ReadI = BuffLittleLong( fin );
		}
		else ReadI = BuffLittleLong( fin );
		fin += TempBpp;
		fout[i] = ((ReadI & r_bitmask)>> RedR) << RedL;

		if( Bpp >= 3 )
		{
			fout[i+1] = ((ReadI & g_bitmask) >> GreenR) << GreenL;
			fout[i+2] = ((ReadI & b_bitmask) >> BlueR) << BlueL;
			if( Bpp == 4 )
			{
				fout[i+3] = ((ReadI & a_bitmask) >> AlphaR) << AlphaL;
				if (AlphaL >= 7) fout[i+3] = fout[i+3] ? 0xFF : 0x00;
				else if (AlphaL >= 4) fout[i+3] = fout[i+3] | (fout[i+3] >> 4);
			}
		}
		else if( Bpp == 2 )
		{
			fout[i+1] = ((ReadI & a_bitmask) >> AlphaR) << AlphaL;
			if (AlphaL >= 7) fout[i+1] = fout[i+1] ? 0xFF : 0x00;
			else if (AlphaL >= 4) fout[i+1] = fout[i+1] | (fout[i+3] >> 4);
		}
	}

	// update image parms
	pix->size = pix->width * pix->height * 4;
	Mem_Free( pix->buffer );
	pix->buffer = fout;
	pix->type = PF_RGBA_32;

	*image = pix;
	return true;
}

word *Image_Compress565( rgbdata_t *pix )
{
	word	*Data;
	uint	i, j;
	uint	Bps, SizeOfPlane, SizeOfData;

	Data = (word *)Mem_Alloc( zonepool, pix->width * pix->height * 2 * pix->numLayers );

	Bps = pix->width * PFDesc[pix->type].bpp * PFDesc[pix->type].bpc;
	SizeOfPlane = Bps * pix->height;
	SizeOfData = SizeOfPlane * pix->numLayers;

	switch ( pix->type )
	{
	case PF_RGB_24:
	case PF_RGB_24_FLIP:
		for (i = 0, j = 0; i < SizeOfData; i += 3, j++)
		{
			Data[j]  = (pix->buffer[i+0] >> 3) << 11;
			Data[j] |= (pix->buffer[i+1] >> 2) << 5;
			Data[j] |=  pix->buffer[i+2] >> 3;
		}
		break;
	case PF_RGBA_32:
		for( i = 0, j = 0; i < SizeOfData; i += 4, j++ )
		{
			Data[j] |= (pix->buffer[i+0]>>3)<<11;
			Data[j] |= (pix->buffer[i+1]>>2)<<5;
			Data[j] |= (pix->buffer[i+2]>>3);
		}
		break;
	case PF_LUMINANCE:
		for (i = 0, j = 0; i < SizeOfData; i++, j++)
		{
			Data[j]  = (pix->buffer[i] >> 3) << 11;
			Data[j] |= (pix->buffer[i] >> 2) << 5;
			Data[j] |=  pix->buffer[i] >> 3;
		}
		break;
	case PF_LUMINANCE_ALPHA:
		for (i = 0, j = 0; i < SizeOfData; i += 2, j++)
		{
			Data[j]  = (pix->buffer[i] >> 3) << 11;
			Data[j] |= (pix->buffer[i] >> 2) << 5;
			Data[j] |=  pix->buffer[i] >> 3;
		}
		break;
	}
	return Data;
}

byte *Image_Compress88( rgbdata_t *pix )
{
	byte	*Data;
	uint	i, j;

	Data = (byte *)Mem_Alloc(zonepool, pix->width * pix->height * 2 * pix->numLayers );

	switch( pix->type )
	{
	case PF_RGB_24:
		for (i = 0, j = 0; i < pix->size; i += 3, j += 2)
		{
			Data[j+0] = pix->buffer[i+1];
			Data[j+1] = pix->buffer[i+0];
		}
		break;
	case PF_RGBA_32:
		for (i = 0, j = 0; i < pix->size; i += 4, j += 2)
		{
			Data[j+0] = pix->buffer[i+1];
			Data[j+1] = pix->buffer[i+0];
		}
		break;
	case PF_RGB_24_FLIP:
		for (i = 0, j = 0; i < pix->size; i += 3, j += 2)
		{
			Data[j+0] = pix->buffer[i+1];
			Data[j+1] = pix->buffer[i+2];
		}
		break;
	case PF_LUMINANCE:
	case PF_LUMINANCE_ALPHA:
		for (i = 0, j = 0; i < pix->size; i++, j += 2)
		{
			Data[j] = Data[j+1] = 0; //??? Luminance is no normal map format...
		}
		break;
	}
	return Data;
}

size_t Image_CompressDXT( vfile_t *f, int saveformat, rgbdata_t *pix )
{
	word	*Data, Block[16], ex0, ex1, *Runner16, t0, t1;
	byte	*Alpha, AlphaBlock[16], AlphaBitMask[6], a0, a1;
	uint	x, y, z, i, BitMask;
	byte	*Runner8;
	bool	HasAlpha;
	size_t	Count = 0;

	if(!pix || !pix->buffer )
		return 0;

	Data = Image_Compress565( pix );
	if(!Data) return 0;
	Alpha = Image_GetAlpha( pix );
	if(!Alpha)
	{
		Mem_Free(Data);
		return 0;
	}

	Runner8 = Alpha;
	Runner16 = Data;

	switch( saveformat )
	{
	case PF_DXT1:
		for (z = 0; z < pix->numLayers; z++)
		{
			for (y = 0; y < pix->height; y += 4)
			{
				for (x = 0; x < pix->width; x += 4)
				{
					Image_GetAlphaBlock(AlphaBlock, Runner8, pix, x, y);
					HasAlpha = false;
					for (i = 0; i < 16; i++)
					{
						if(AlphaBlock[i] < 128)
						{
							HasAlpha = true;
							break;
						}
					}

					Image_GetBlock(Block, Runner16, pix, x, y);
					Image_ChooseEndpoints(Block, &ex0, &ex1);
					Image_CorrectEndDXT1(&ex0, &ex1, HasAlpha);
					VFS_Write(f, &ex0, sizeof(word));
					VFS_Write(f, &ex1, sizeof(word));
					if (HasAlpha) BitMask = Image_GenBitMask(ex0, ex1, 3, Block, AlphaBlock, NULL);
					else BitMask = Image_GenBitMask(ex0, ex1, 4, Block, NULL, NULL);
					VFS_Write(f, AlphaBitMask, sizeof(uint));
					Count += 8;
				}
			}
			Runner16 += pix->width * pix->height;
			Runner8 += pix->width * pix->height;
		}
		break;
	case PF_DXT3:
		for (z = 0; z < pix->numLayers; z++)
		{
			for (y = 0; y < pix->height; y += 4)
			{
				for (x = 0; x < pix->width; x += 4)
				{
					Image_GetAlphaBlock(AlphaBlock, Runner8, pix, x, y);
					for (i = 0; i < 16; i += 2)
					{
						byte tempBlock = ((AlphaBlock[i]>>4)<<4) | (AlphaBlock[i+1]>>4);
						VFS_Write(f, &tempBlock, 1 );
					}
					Image_GetBlock(Block, Runner16, pix, x, y);
					Image_ChooseEndpoints(Block, &t0, &t1);
					ex0 = max(t0, t1);
					ex1 = min(t0, t1);
					Image_CorrectEndDXT1(&ex0, &ex1, 0);
					VFS_Write(f, &ex0, sizeof(word));
					VFS_Write(f, &ex1, sizeof(word));
					BitMask = Image_GenBitMask(ex0, ex1, 4, Block, NULL, NULL);
					VFS_Write(f, AlphaBitMask, sizeof(uint));
					Count += 16;
				}
			}
			Runner16 += pix->width * pix->height;
			Runner8 += pix->width * pix->height;
		}
		break;
	case PF_DXT5:
		for (z = 0; z < pix->numLayers; z++)
		{
			for (y = 0; y < pix->height; y += 4)
			{
				for (x = 0; x < pix->width; x += 4)
				{
					Image_GetAlphaBlock(AlphaBlock, Runner8, pix, x, y);
					Image_ChooseAlphaEndpoints(AlphaBlock, &a0, &a1);
					Image_GenAlphaBitMask(a0, a1, AlphaBlock, AlphaBitMask, NULL/*AlphaOut*/);
					VFS_Write(f, &a0, sizeof(byte));
					VFS_Write(f, &a1, sizeof(byte));
                                                 	VFS_Write(f, &AlphaBitMask, sizeof(byte) * 6 );
                                                 	Image_GetBlock(Block, Runner16, pix, x, y);
					Image_ChooseEndpoints(Block, &t0, &t1);
					ex0 = max(t0, t1);
					ex1 = min(t0, t1);
					Image_CorrectEndDXT1(&ex0, &ex1, 0);
					VFS_Write(f, &ex0, sizeof(word));
					VFS_Write(f, &ex1, sizeof(word));
					BitMask = Image_GenBitMask(ex0, ex1, 4, Block, NULL, NULL);
					VFS_Write(f, &BitMask, sizeof(uint));
					Count += 16;
				}
			}
			Runner16 += pix->width * pix->height;
			Runner8 += pix->width * pix->height;
		}
		break;
	}

	Mem_Free( Data );
	Mem_Free( Alpha);

	return Count;
}

void Image_DXTGetPixelFormat( dds_t *hdr )
{
	uint bits = hdr->dsPixelFormat.dwRGBBitCount;

	// All volume textures I've seem so far didn't have the DDS_COMPLEX flag set,
	// even though this is normally required. But because noone does set it,
	// also read images without it (TODO: check file size for 3d texture?)
	if (!(hdr->dsCaps.dwCaps2 & DDS_VOLUME)) hdr->dwDepth = 1;

	if(hdr->dsPixelFormat.dwFlags & DDS_ALPHA)
		image_flags |= IMAGE_HAS_ALPHA;

	if (hdr->dsPixelFormat.dwFlags & DDS_FOURCC)
	{
		switch (hdr->dsPixelFormat.dwFourCC)
		{
		case TYPE_DXT1: image_type = PF_DXT1; break;
		case TYPE_DXT3: image_type = PF_DXT3; break;
		case TYPE_DXT5: image_type = PF_DXT5; break;
		case TYPE_$: image_type = PF_ABGR_64; break;
		case TYPE_t: image_type = PF_ABGR_128F; break;
		default: image_type = PF_UNKNOWN; break;
		}
	}
	else
	{
		// This dds texture isn't compressed so write out ARGB or luminance format
		if (hdr->dsPixelFormat.dwFlags & DDS_LUMINANCE)
		{
			if (hdr->dsPixelFormat.dwFlags & DDS_ALPHAPIXELS)
				image_type = PF_LUMINANCE_ALPHA;
			else if(hdr->dsPixelFormat.dwRGBBitCount == 16 && hdr->dsPixelFormat.dwRBitMask == 0xFFFF) 
				image_type = PF_LUMINANCE_16;
			else image_type = PF_LUMINANCE;
		}
		else 
		{
			if( bits == 32) image_type = PF_ABGR_64;
			else image_type = PF_ARGB_32;
		}
	}

	// setup additional flags
	if( hdr->dsCaps.dwCaps1 & DDS_COMPLEX && hdr->dsCaps.dwCaps2 & DDS_CUBEMAP)
	{
		image_flags |= IMAGE_CUBEMAP | IMAGE_CUBEMAP_FLIP;
	}

	if(hdr->dsPixelFormat.dwFlags & DDS_ALPHAPIXELS)
	{
		image_flags |= IMAGE_HAS_ALPHA;
	}

	if(hdr->dwFlags & DDS_MIPMAPCOUNT)
		image_num_mips = hdr->dwMipMapCount;
	else image_num_mips = 1;

	if(image_type == PF_ARGB_32 || image_type == PF_LUMINANCE || image_type == PF_LUMINANCE_16 || image_type == PF_LUMINANCE_ALPHA)
	{
		//store RGBA mask into one block, and get palette pointer
		byte *tmp = image_palette = Mem_Alloc( zonepool, sizeof(uint) * 4 );
		Mem_Copy( tmp, &hdr->dsPixelFormat.dwRBitMask, sizeof(uint)); tmp += 4;
		Mem_Copy( tmp, &hdr->dsPixelFormat.dwGBitMask, sizeof(uint)); tmp += 4;
		Mem_Copy( tmp, &hdr->dsPixelFormat.dwBBitMask, sizeof(uint)); tmp += 4;
		Mem_Copy( tmp, &hdr->dsPixelFormat.dwABitMask, sizeof(uint)); tmp += 4;
	}
}

void Image_DXTAdjustVolume( dds_t *hdr )
{
	uint bits;
	
	if (hdr->dwDepth <= 1) return;
	bits = hdr->dsPixelFormat.dwRGBBitCount / 8;
	hdr->dwFlags |= DDS_LINEARSIZE;
	hdr->dwLinearSize = Image_DXTGetLinearSize( image_type, hdr->dwWidth, hdr->dwHeight, hdr->dwDepth, bits );
}

uint Image_DXTCalcMipmapSize( dds_t *hdr ) 
{
	uint buffsize = 0;
	int w = hdr->dwWidth;
	int h = hdr->dwHeight;
	int d = hdr->dwDepth;
	int i, mipsize = 0;
	int bits = hdr->dsPixelFormat.dwRGBBitCount / 8;
		
	// now correct buffer size
	for( i = 0; i < image_num_mips; i++, buffsize += mipsize )
	{
		mipsize = Image_DXTGetLinearSize( image_type, w, h, d, bits );
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1;
	}
	return buffsize;
}

uint Image_DXTCalcSize( const char *name, dds_t *hdr, size_t filesize ) 
{
	size_t buffsize = 0;
	int w = image_width;
	int h = image_height;
	int d = image_num_layers;
	int bits = hdr->dsPixelFormat.dwRGBBitCount / 8;

	if(hdr->dsCaps.dwCaps2 & DDS_CUBEMAP) 
	{
		// cubemap w*h always match for all sides
		buffsize = Image_DXTCalcMipmapSize( hdr ) * 6;
	}
	else if(hdr->dwFlags & DDS_MIPMAPCOUNT)
	{
		// if mipcount > 1
		buffsize = Image_DXTCalcMipmapSize( hdr );
	}
	else if(hdr->dwFlags & (DDS_LINEARSIZE | DDS_PITCH))
	{
		// just in case (no need, really)
		buffsize = hdr->dwLinearSize;
	}
	else 
	{
		// pretty solution for microsoft bug
		buffsize = Image_DXTCalcMipmapSize( hdr );
	}

	if(filesize != buffsize) // main check
	{
		MsgWarn("LoadDDS: (%s) probably corrupted(%i should be %i)\n", name, buffsize, filesize );
		return false;
	}
	return buffsize;
}

/*
=============
Image_LoadDDS
=============
*/
bool Image_LoadDDS( const char *name, byte *buffer, size_t filesize )
{
	dds_t	header;
	byte	*fin;
	uint	i;

	fin = buffer;

	// swap header
	header.dwIdent = BuffLittleLong(fin); fin += 4;
	header.dwSize = BuffLittleLong(fin); fin += 4;
	header.dwFlags = BuffLittleLong(fin); fin += 4;
	header.dwHeight = BuffLittleLong(fin); fin += 4;
	header.dwWidth = BuffLittleLong(fin); fin += 4;
	header.dwLinearSize = BuffLittleLong(fin); fin += 4;
	header.dwDepth = BuffLittleLong(fin); fin += 4;
	header.dwMipMapCount = BuffLittleLong(fin); fin += 4;
	header.dwAlphaBitDepth = BuffLittleLong(fin); fin += 4;

	for(i = 0; i < 3; i++)
	{
		header.fReflectivity[i] = BuffLittleFloat(fin);
		fin += 4;
	}

	header.fBumpScale = BuffLittleFloat(fin); fin += 4;

	for (i = 0; i < 6; i++) 
	{
		// skip unused stuff
		header.dwReserved1[i] = BuffLittleLong(fin);
		fin += 4;
	}

	// pixel format
	header.dsPixelFormat.dwSize = BuffLittleLong(fin); fin += 4;
	header.dsPixelFormat.dwFlags = BuffLittleLong(fin); fin += 4;
	header.dsPixelFormat.dwFourCC = BuffLittleLong(fin); fin += 4;
	header.dsPixelFormat.dwRGBBitCount = BuffLittleLong(fin); fin += 4;
	header.dsPixelFormat.dwRBitMask = BuffLittleLong(fin); fin += 4;
	header.dsPixelFormat.dwGBitMask = BuffLittleLong(fin); fin += 4;
	header.dsPixelFormat.dwBBitMask = BuffLittleLong(fin); fin += 4;
	header.dsPixelFormat.dwABitMask = BuffLittleLong(fin); fin += 4;

	// caps
	header.dsCaps.dwCaps1 = BuffLittleLong(fin); fin += 4;
	header.dsCaps.dwCaps2 = BuffLittleLong(fin); fin += 4;
	header.dsCaps.dwCaps3 = BuffLittleLong(fin); fin += 4;
	header.dsCaps.dwCaps4 = BuffLittleLong(fin); fin += 4;
	header.dwTextureStage = BuffLittleLong(fin); fin += 4;

	if(header.dwIdent != DDSHEADER) return false; // it's not a dds file, just skip it
	if(header.dwSize != sizeof(dds_t) - 4 ) // size of the structure (minus MagicNum)
	{
		MsgWarn("LoadDDS: (%s) have corrupt header\n", name );
		return false;
	}
	if(header.dsPixelFormat.dwSize != sizeof(dds_pixf_t)) // size of the structure
	{
		MsgWarn("LoadDDS: (%s) have corrupt pixelformat header\n", name );
		return false;
	}

	image_width = header.dwWidth;
	image_height = header.dwHeight;
	image_bits_count = header.dsPixelFormat.dwRGBBitCount;
	if(header.dwFlags & DDS_DEPTH) image_num_layers = header.dwDepth;
	if(!Image_ValidSize( name )) return false;

	Image_DXTGetPixelFormat( &header );// and image type too :)
	Image_DXTAdjustVolume( &header );

	if (image_type == PF_UNKNOWN) 
	{
		MsgWarn("LoadDDS: (%s) have unsupported compression type\n", name );
		return false; //unknown type
	}

	image_size = Image_DXTCalcSize( name, &header, filesize - 128 ); 
	if(image_size == 0) return false; // just in case

	// dds files will be uncompressed on a render. requires minimal of info for set this
	image_rgba = Mem_Alloc( zonepool, image_size ); 
	Mem_Copy( image_rgba, fin, image_size );

	return true;
}

bool Image_SaveDDS( const char *name, rgbdata_t *pix, int saveformat )
{
	file_t	*file = FS_Open( name, "wb" );	// create real file
	vfile_t	*vhandle = VFS_Open( file, "w" );	// create virtual file

	Image_DXTWriteHeader( vhandle, pix, 0, saveformat );
	if(!Image_CompressDXT( vhandle, saveformat, pix ))
	{
		MsgDev(D_ERROR, "Image_SaveDDS: can't create dds file\n" );
		return false;
	}
	file = VFS_Close( vhandle );			// write buffer into hdd
	FS_Close( file );

	return true;
}