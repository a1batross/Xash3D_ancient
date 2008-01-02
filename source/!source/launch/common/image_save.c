//=======================================================================
//			Copyright XashXT Group 2007 ©
//			image_save.c - saving textures
//=======================================================================

#include "launch.h"
#include "image.h"
#include "mathlib.h"

#define Sum(c) ((c)->r + (c)->g + (c)->b)

/*
===============
GetImageSize

calculate buffer size for current miplevel
===============
*/
uint GetImageSize( int block, int width, int height, int depth, int bpp, int rgbcount )
{
	uint BlockSize = 0;

	if(block == 0) BlockSize = width * height * bpp;
	else if(block > 0) BlockSize = ((width + 3)/4) * ((height + 3)/4) * depth * block;
	else if(block < 0 && rgbcount > 0) BlockSize = width * height * depth * rgbcount;
	else BlockSize = width * height * abs(block);

	return BlockSize;
}

bool dds_write_header( vfile_t *f, rgbdata_t *pix, uint cubemap_flags, uint savetype )
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
	case PF_DXT2:
		dwFourCC = TYPE_DXT2;
		break;
	case PF_DXT3:
		dwFourCC = TYPE_DXT3;
		break;
	case PF_DXT4:
		dwFourCC = TYPE_DXT4;
		break;
	case PF_DXT5:
		dwFourCC = TYPE_DXT5;
		break;
	case PF_ATI1N:
		dwFourCC = TYPE_ATI1;
		break;
	case PF_ATI2N:
		dwFourCC = TYPE_ATI2;
		break;
	case PF_RXGB:
		dwFourCC = TYPE_RXGB;
		break;
	default:
		MsgDev( D_ERROR, "dds_write_header: unsupported type %s\n", PFDesc[pix->type].name );	
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
	dwLinearSize = GetImageSize(PFDesc[savetype].block, pix->width, pix->height, pix->numLayers, PFDesc[savetype].bpp, pix->bitsCount / 8 );
	//dwLinearSize = (((pix->width + 3)/4) * ((pix->height + 3)/4)) * dwBlockSize * pix->numLayers;
	VFS_Write(f, &dwLinearSize, sizeof(uint)); // TODO: use dds_get_linear_size

	if (pix->numLayers > 1)
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

void ShortToColor565(word Pixel, color16 *Colour)
{
	Colour->r = (Pixel & 0xF800) >> 11;
	Colour->g = (Pixel & 0x07E0) >> 5;
	Colour->b = (Pixel & 0x001F);
	return;
}


void ShortToColor888(word Pixel, color24 *Colour)
{
	Colour->r = ((Pixel & 0xF800) >> 11) << 3;
	Colour->g = ((Pixel & 0x07E0) >> 5)  << 2;
	Colour->b = ((Pixel & 0x001F))       << 3;
	return;
}

word Color565ToShort(color16 *Colour)
{
	return (Colour->r << 11) | (Colour->g << 5) | (Colour->b);
}

word Color888ToShort(color24 *Colour)
{
	return ((Colour->r >> 3) << 11) | ((Colour->g >> 2) << 5) | (Colour->b >> 3);
}

void ChooseEndpoints(word *Block, word *ex0, word *ex1)
{
	uint	i;
	color24	Colours[16];
	int	Lowest=0, Highest=0;

	for (i = 0; i < 16; i++)
	{
		ShortToColor888(Block[i], &Colours[i]);
		if(Sum(&Colours[i]) < Sum(&Colours[Lowest])) Lowest = i;
		if(Sum(&Colours[i]) > Sum(&Colours[Highest])) Highest = i;
	}
	*ex0 = Block[Highest];
	*ex1 = Block[Lowest];
}

void CorrectEndDXT1( word *ex0, word *ex1, bool HasAlpha )
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
	return;
}

void PreMult(word *Data, byte *Alpha)
{
	color24		Colour;
	uint		i;

	for (i = 0; i < 16; i++)
	{
		ShortToColor888(Data[i], &Colour);
		Colour.r = (byte)(((uint)Colour.r * Alpha[i]) >> 8);
		Colour.g = (byte)(((uint)Colour.g * Alpha[i]) >> 8);
		Colour.b = (byte)(((uint)Colour.b * Alpha[i]) >> 8);
		Data[i] = Color888ToShort(&Colour);
		ShortToColor888(Data[i], &Colour);
	}

	return;
}

void ChooseAlphaEndpoints( byte *Block, byte *a0, byte *a1)
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
bool GetBlock( word *Block, word *Data, rgbdata_t *pix, uint XPos, uint YPos)
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

bool GetAlphaBlock( byte *Block, byte *Data, rgbdata_t *pix, uint XPos, uint YPos)
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

bool Get3DcBlock( byte *Block, byte *Data, rgbdata_t *pix, uint XPos, uint YPos, int channel )
{
	uint x, y, i = 0, Offset = 2*(YPos * pix->width + XPos) + channel;

	for (y = 0; y < 4; y++)
	{
		for (x = 0; x < 4; x++)
		{
			if(x < pix->width && y < pix->height)
  				Block[i++] = Data[Offset + 2 * x];
			else Block[i++] = Data[Offset];
		}
		Offset += 2 * pix->width;
	}
	return true;
}

byte* GetAlpha( rgbdata_t *pix )
{
	byte		*Alpha;
	uint		i, j, Bpc, Size, AlphaOff;

	Bpc = PFDesc[pix->type].bpc;
	if( Bpc == 0 ) return NULL;

	Size = pix->width * pix->height * pix->numLayers * PFDesc[pix->type].bpp;
	Alpha = (byte*)Mem_Alloc( Sys.imagepool, Size / PFDesc[pix->type].bpp * Bpc);

	if( pix->type == PF_LUMINANCE_ALPHA )
		AlphaOff = 2;
	else AlphaOff = 4;

	for (i = AlphaOff - 1, j = 0; i < Size; i += AlphaOff, j++ )
		Alpha[j] = pix->buffer[i];

	return Alpha;
}

uint RMSAlpha( byte *Orig, byte *Test )
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

uint Distance(color24 *c1, color24 *c2)
{
	return  (c1->r - c2->r) * (c1->r - c2->r) + (c1->g - c2->g) * (c1->g - c2->g) + (c1->b - c2->b) * (c1->b - c2->b);
}

uint GenBitMask( word ex0, word ex1, uint NumCols, word *In, byte *Alpha, color24 *OutCol )
{
	uint		i, j, Closest, Dist, BitMask = 0;
	byte		Mask[16];
	color24		c, Colours[4];

	ShortToColor888(ex0, &Colours[0]);
	ShortToColor888(ex1, &Colours[1]);
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
		ShortToColor888(In[i], &c);
		for (j = 0; j < NumCols; j++)
		{
			Dist = Distance(&c, &Colours[j]);
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


void GenAlphaBitMask( byte a0, byte a1, byte *In, byte *Mask, byte *Out )
{
	byte Alphas[8], M[16];
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

	return;
}

word *dds_compress_565( rgbdata_t *pix )
{
	word		*Data;
	uint		i, j;

	Data = (word*)Mem_Alloc( Sys.imagepool, pix->width * pix->height * 2 * pix->numLayers );

	switch ( pix->type )
	{
	case PF_RGB_24:
		for (i = 0, j = 0; i < pix->size; i += 3, j++)
		{
			Data[j]  = (pix->buffer[i+0] >> 3) << 11;
			Data[j] |= (pix->buffer[i+1] >> 2) << 5;
			Data[j] |=  pix->buffer[i+2] >> 3;
		}
		break;
	case PF_RGBA_32:
		for( i = 0, j = 0; i < pix->size; i += 4, j++ )
		{
			Data[j] |= (pix->buffer[i+0]>>3)<<11;
			Data[j] |= (pix->buffer[i+1]>>2)<<5;
			Data[j] |= (pix->buffer[i+2]>>3);
		}
		break;
	case PF_RGB_24_FLIP:
		for (i = 0, j = 0; i < pix->size; i += 3, j++)
		{
			Data[j]  = (pix->buffer[i+2] >> 3) << 11;
			Data[j] |= (pix->buffer[i+1] >> 2) << 5;
			Data[j] |=  pix->buffer[i+0] >> 3;
		}
		break;
	case PF_LUMINANCE:
		for (i = 0, j = 0; i < pix->size; i++, j++)
		{
			Data[j]  = (pix->buffer[i] >> 3) << 11;
			Data[j] |= (pix->buffer[i] >> 2) << 5;
			Data[j] |=  pix->buffer[i] >> 3;
		}
		break;
	case PF_LUMINANCE_ALPHA:
		for (i = 0, j = 0; i < pix->size; i += 2, j++)
		{
			Data[j]  = (pix->buffer[i] >> 3) << 11;
			Data[j] |= (pix->buffer[i] >> 2) << 5;
			Data[j] |=  pix->buffer[i] >> 3;
		}
		break;
	}
	return Data;
}

byte *dds_compress_88( rgbdata_t *pix )
{
	byte	*Data;
	uint	i, j;

	Data = (byte*)Mem_Alloc(Sys.imagepool, pix->width * pix->height * 2 * pix->numLayers );

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
			Data[j  ] = pix->buffer[i+1];
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

void dds_compress_RXGB(rgbdata_t *pix, word **xgb, byte **r )
{
	uint		i, j;
	word		*Data;
	byte		*Alpha;

	*xgb = NULL;
	*r = NULL;

	*xgb = (word*)Mem_Alloc( Sys.imagepool, pix->width * pix->height * 2 * pix->numLayers );
	*r = Mem_Alloc( Sys.imagepool, pix->width * pix->height * pix->numLayers);

	//alias pointers to be able to use copy'n'pasted code :)
	Data = *xgb;
	Alpha = *r;

	switch( pix->type )
	{
	case PF_RGB_24:
		for (i = 0, j = 0; i < pix->size; i += 3, j++)
		{
			Alpha[j] = pix->buffer[i+0];
			Data[j] = (pix->buffer[i+1] >> 2) << 5;
			Data[j] |= pix->buffer[i+2] >> 3;
		}
		break;
	case PF_RGBA_32:
		for (i = 0, j = 0; i < pix->size; i += 4, j++)
		{
			Alpha[j] = pix->buffer[i+0];
			Data[j] = (pix->buffer[i+1] >> 2) << 5;
			Data[j] |= pix->buffer[i+2] >> 3;
		}
		break;
	case PF_RGB_24_FLIP:
		for (i = 0, j = 0; i < pix->size; i += 3, j++)
		{
			Alpha[j] = pix->buffer[i+2];
			Data[j] = (pix->buffer[i+1] >> 2) << 5;
			Data[j] |= pix->buffer[i+0] >> 3;
		}
		break;
	case PF_LUMINANCE:
		for (i = 0, j = 0; i < pix->size; i++, j++)
		{
			Alpha[j] = pix->buffer[i];
			Data[j] = (pix->buffer[i] >> 2) << 5;
			Data[j] |= pix->buffer[i] >> 3;
		}
		break;
	case PF_LUMINANCE_ALPHA:
		for (i = 0, j = 0; i < pix->size; i += 2, j++)
		{
			Alpha[j] = pix->buffer[i];
			Data[j] = (pix->buffer[i] >> 2) << 5;
			Data[j] |= pix->buffer[i] >> 3;
		}
		break;
	}
}

uint dds_compress_dxt( vfile_t *f, int saveformat, rgbdata_t *pix )
{
	word	*Data, Block[16], ex0, ex1, *Runner16, t0, t1;
	byte	*Alpha, AlphaBlock[16], AlphaBitMask[6], a0, a1;
	uint	x, y, z, i, BitMask;
	byte	*Data3Dc, *Runner8;
	bool	HasAlpha;
	uint	Count = 0;

	if(!pix || !pix->buffer )
		return 0;

	if( saveformat == PF_ATI2N)
	{
		Data3Dc = dds_compress_88( pix );
		if(!Data3Dc) return 0;
		Runner8 = Data3Dc;

		for (z = 0; z < pix->numLayers; z++)
		{
			for (y = 0; y < pix->height; y += 4)
			{
				for (x = 0; x < pix->width; x += 4)
				{
					Get3DcBlock( AlphaBlock, Runner8, pix, x, y, 0);
					ChooseAlphaEndpoints(AlphaBlock, &a0, &a1);
					GenAlphaBitMask(a0, a1, AlphaBlock, AlphaBitMask, NULL);
					VFS_Write(f, &a0, sizeof(byte));
					VFS_Write(f, &a1, sizeof(byte));
					VFS_Write(f, &AlphaBitMask, sizeof(byte) * 6 );
					Get3DcBlock(AlphaBlock, Runner8, pix, x, y, 1);
					ChooseAlphaEndpoints(AlphaBlock, &a0, &a1);
					GenAlphaBitMask(a0, a1, AlphaBlock, AlphaBitMask, NULL);
					VFS_Write(f, &a0, sizeof(byte));
					VFS_Write(f, &a1, sizeof(byte));
					VFS_Write(f, &AlphaBitMask, sizeof(byte) * 6 );
					Count += 16;
				}
			}
			Runner8 += pix->width * pix->height * 2;

		}
		Mem_Free( Data3Dc );
	}
	else if( saveformat == PF_ATI1N )
	{
		rgbdata_t	*lum = NULL;
		if (PFDesc[pix->type].bpp != 1)
		{
			//FIXME: lum = Image_Convert( pix, PF_LUMINANCE );
			if(!lum) return 0;
		}
		else lum = pix;

		Runner8 = lum->buffer;
		for (z = 0; z < pix->numLayers; z++)
		{
			for (y = 0; y < pix->height; y += 4)
			{
				for (x = 0; x < pix->width; x += 4)
				{
					GetAlphaBlock(AlphaBlock, Runner8, pix, x, y);
					ChooseAlphaEndpoints(AlphaBlock, &a0, &a1);
					GenAlphaBitMask(a0, a1, AlphaBlock, AlphaBitMask, NULL);
					VFS_Write(f, &a0, sizeof(byte));
					VFS_Write(f, &a1, sizeof(byte));
					VFS_Write(f, &AlphaBitMask, sizeof(byte) * 6 );
					Count += 8;
				}
			}
			Runner8 += pix->width * pix->height;
		}
		if(lum != pix) FS_FreeImage( lum );
	}
	else
	{
		if( saveformat != PF_RXGB )
		{
			Data = dds_compress_565( pix );
			if(!Data) return 0;

			Alpha = GetAlpha( pix );
			if(!Alpha)
			{
				Mem_Free(Data);
				return 0;
			}
		}
		else
		{
			dds_compress_RXGB(pix, &Data, &Alpha);
			if (!Data || !Alpha)
			{
				if( Data ) Mem_Free( Data );
				if( Alpha) Mem_Free( Alpha);
				return 0;
			}
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
						GetAlphaBlock(AlphaBlock, Runner8, pix, x, y);
						HasAlpha = false;
						for (i = 0; i < 16; i++)
						{
							if(AlphaBlock[i] < 128)
							{
								HasAlpha = true;
								break;
							}
						}

						GetBlock(Block, Runner16, pix, x, y);
						ChooseEndpoints(Block, &ex0, &ex1);
						CorrectEndDXT1(&ex0, &ex1, HasAlpha);
						VFS_Write(f, &ex0, sizeof(word));
						VFS_Write(f, &ex1, sizeof(word));
						if (HasAlpha) BitMask = GenBitMask(ex0, ex1, 3, Block, AlphaBlock, NULL);
						else BitMask = GenBitMask(ex0, ex1, 4, Block, NULL, NULL);
						VFS_Write(f, AlphaBitMask, sizeof(uint));
						Count += 8;
					}
				}
				Runner16 += pix->width * pix->height;
				Runner8 += pix->width * pix->height;
			}
			break;
		case PF_DXT2:
			for (y = 0; y < pix->height; y += 4)
			{
				for (x = 0; x < pix->width; x += 4)
				{
					GetAlphaBlock(AlphaBlock, Runner8, pix, x, y);
					for (i = 0; i < 16; i += 2)
					{
						byte tempBlock = ((AlphaBlock[i]>>4)<<4) | (AlphaBlock[i+1]>>4);
						VFS_Write(f, &tempBlock, 1 );
					}
					GetBlock(Block, Runner16, pix, x, y);
					PreMult(Block, AlphaBlock);
					ChooseEndpoints(Block, &ex0, &ex1);
					VFS_Write(f, &ex0, sizeof(word));
					VFS_Write(f, &ex1, sizeof(word));
					BitMask = GenBitMask(ex0, ex1, 4, Block, NULL, NULL);
					VFS_Write(f, &AlphaBitMask, sizeof(uint));
					Count += 16;
				}		
			}
			break;
		case PF_DXT3:
			for (z = 0; z < pix->numLayers; z++)
			{
				for (y = 0; y < pix->height; y += 4)
				{
					for (x = 0; x < pix->width; x += 4)
					{
						GetAlphaBlock(AlphaBlock, Runner8, pix, x, y);
						for (i = 0; i < 16; i += 2)
						{
							byte tempBlock = ((AlphaBlock[i]>>4)<<4) | (AlphaBlock[i+1]>>4);
							VFS_Write(f, &tempBlock, 1 );
						}
						GetBlock(Block, Runner16, pix, x, y);
						ChooseEndpoints(Block, &t0, &t1);
						ex0 = max(t0, t1);
						ex1 = min(t0, t1);
						CorrectEndDXT1(&ex0, &ex1, 0);
						VFS_Write(f, &ex0, sizeof(word));
						VFS_Write(f, &ex1, sizeof(word));
						BitMask = GenBitMask(ex0, ex1, 4, Block, NULL, NULL);
						VFS_Write(f, AlphaBitMask, sizeof(uint));
						Count += 16;
					}
				}
				Runner16 += pix->width * pix->height;
				Runner8 += pix->width * pix->height;
			}
			break;
		case PF_RXGB:
		case PF_DXT5:
			for (z = 0; z < pix->numLayers; z++)
			{
				for (y = 0; y < pix->height; y += 4)
				{
					for (x = 0; x < pix->width; x += 4)
					{
						GetAlphaBlock(AlphaBlock, Runner8, pix, x, y);
						ChooseAlphaEndpoints(AlphaBlock, &a0, &a1);
						GenAlphaBitMask(a0, a1, AlphaBlock, AlphaBitMask, NULL/*AlphaOut*/);
						VFS_Write(f, &a0, sizeof(byte));
						VFS_Write(f, &a1, sizeof(byte));
                                                  	VFS_Write(f, &AlphaBitMask, sizeof(byte) * 6 );
                                                  	GetBlock(Block, Runner16, pix, x, y);
						ChooseEndpoints(Block, &t0, &t1);
						ex0 = max(t0, t1);
						ex1 = min(t0, t1);
						CorrectEndDXT1(&ex0, &ex1, 0);
						VFS_Write(f, &ex0, sizeof(word));
						VFS_Write(f, &ex1, sizeof(word));
						BitMask = GenBitMask(ex0, ex1, 4, Block, NULL, NULL);
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
	}
	return Count;
}

bool dds_save_image( const char *name, rgbdata_t *pix, int saveformat )
{
	file_t	*file;
	vfile_t	*vhandle;

	file = FS_Open( name, "wb" ); // create real file
	vhandle = VFS_Open( file, "w" ); // create virtual file

	dds_write_header( vhandle, pix, 0, saveformat );
	if(!dds_compress_dxt( vhandle, saveformat, pix ))
	{
		Msg("dds_save_image: can't create dds file\n");
		return false;
	}
	file = VFS_Close( vhandle ); // write buffer into hdd
	FS_Close( file );

	return true;
}