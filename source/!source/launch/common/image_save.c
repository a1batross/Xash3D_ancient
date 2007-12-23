//=======================================================================
//			Copyright XashXT Group 2007 ©
//			image_save.c - saving textures
//=======================================================================

#include "launch.h"
#include "image.h"
#include "mathlib.h"

#define Sum(c) ((c)->r + (c)->g + (c)->b)

bool dds_write_header( vfile_t *f, rgbdata_t *pix, uint cubemap_flags )
{
	uint	dwFourCC, dwFlags1 = 0, dwFlags2 = 0, dwCaps1 = 0;
	uint	dwLinearSize, dwBlockSize, dwCaps2 = 0;
	uint	dwIdent = DDSHEADER, dwSize = 124, dwSize2 = 32;

	if(!pix || pix->buffer )
		return false;

	// setup flags
	dwFlags1 |= DDS_LINEARSIZE | DDS_MIPMAPCOUNT | DDS_WIDTH | DDS_HEIGHT | DDS_CAPS | DDS_PIXELFORMAT;
	dwFlags2 |= DDS_FOURCC;
	if( pix->numLayers > 1) dwFlags1 |= DDS_DEPTH;

	switch( pix->type )
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

	VFS_Write(f, &dwIdent, sizeof(uint));
	VFS_Write(f, &dwSize, sizeof(uint));
	VFS_Write(f, &dwFlags1, sizeof(uint));
	VFS_Write(f, &pix->height, sizeof(uint));
	VFS_Write(f, &pix->width, sizeof(uint));

	dwBlockSize = PFDesc[pix->type].block;
	dwLinearSize = (((pix->width + 3)/4) * ((pix->height + 3)/4)) * dwBlockSize * pix->numLayers;
	VFS_Write(f, &dwLinearSize, sizeof(uint)); // TODO: use dds_get_linear_size

	if (pix->numLayers > 1)
	{
		VFS_Write(f, &pix->numLayers, sizeof(uint));
		dwCaps2 |= DDS_VOLUME;
	}
	else VFS_Write(f, 0, sizeof(uint));

	VFS_Write(f, &pix->numMips, sizeof(uint));
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

bool Get3DcBlock( byte *Block, byte *Data, rgbdata_t *pix, uint XPos, uint YPos, int channel )
{
	uint x, y, i = 0, Offset = 2*(YPos * pix->width + XPos) + channel;

	for (y = 0; y < 4; y++)
	{
		for (x = 0; x < 4; x++)
		{
			if(x < pix->width && y < pix->height)
  				Block[i++] = Data[Offset + 2*x];
			else Block[i++] = Data[Offset];
		}
		Offset += 2 * pix->width;
	}
	return true;
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
		for (i = 0, j = 0; i < pix->size; i += 4, j++)
		{
			Data[j]  = (pix->buffer[i+0] >> 3) << 11;
			Data[j] |= (pix->buffer[i+1] >> 2) << 5;
			Data[j] |=  pix->buffer[i+2] >> 3;
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

	if(!pix || pix->buffer )
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
	else if( saveformat == IL_ATI1N )
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

			Alpha = ilGetAlpha(IL_UNSIGNED_BYTE);
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
			case IL_DXT1:

				for (z = 0; z < pix->numLayers; z++) {
					for (y = 0; y < Image->Height; y += 4) {
						for (x = 0; x < Image->Width; x += 4) {
							GetAlphaBlock(AlphaBlock, Runner8, Image, x, y);
							HasAlpha = IL_FALSE;
							for (i = 0 ; i < 16; i++) {
								if (AlphaBlock[i] < 128) {
									HasAlpha = IL_TRUE;
									break;
								}
							}

							GetBlock(Block, Runner16, Image, x, y);
							ChooseEndpoints(Block, &ex0, &ex1);
							CorrectEndDXT1(&ex0, &ex1, HasAlpha);
							SaveLittleUShort(ex0);
							SaveLittleUShort(ex1);
							if (HasAlpha)
								BitMask = GenBitMask(ex0, ex1, 3, Block, AlphaBlock, NULL);
							else
								BitMask = GenBitMask(ex0, ex1, 4, Block, NULL, NULL);
							SaveLittleUInt(BitMask);
							Count += 8;
						}
					}



					Runner16 += Image->Width * Image->Height;

					Runner8 += Image->Width * Image->Height;

				}
				break;

			/*case IL_DXT2:
				for (y = 0; y < Image->Height; y += 4) {
					for (x = 0; x < Image->Width; x += 4) {
						GetAlphaBlock(AlphaBlock, Alpha, Image, x, y);
						for (i = 0; i < 16; i += 2) {
							iputc((ILubyte)(((AlphaBlock[i] >> 4) << 4) | (AlphaBlock[i+1] >> 4)));
						}

						GetBlock(Block, Data, Image, x, y);
						PreMult(Block, AlphaBlock);
						ChooseEndpoints(Block, &ex0, &ex1);
						SaveLittleUShort(ex0);
						SaveLittleUShort(ex1);
						BitMask = GenBitMask(ex0, ex1, 4, Block, NULL, NULL);
						SaveLittleUInt(BitMask);
					}		
				}
				break;*/

			case IL_DXT3:

				for (z = 0; z < Image->Depth; z++) {
					for (y = 0; y < Image->Height; y += 4) {
						for (x = 0; x < Image->Width; x += 4) {
							GetAlphaBlock(AlphaBlock, Runner8, Image, x, y);
							for (i = 0; i < 16; i += 2) {
								iputc((ILubyte)(((AlphaBlock[i+1] >> 4) << 4) | (AlphaBlock[i] >> 4)));
							}

							GetBlock(Block, Runner16, Image, x, y);
							ChooseEndpoints(Block, &t0, &t1);
							ex0 = IL_MAX(t0, t1);
							ex1 = IL_MIN(t0, t1);
							CorrectEndDXT1(&ex0, &ex1, 0);
							SaveLittleUShort(ex0);
							SaveLittleUShort(ex1);
							BitMask = GenBitMask(ex0, ex1, 4, Block, NULL, NULL);
							SaveLittleUInt(BitMask);
							Count += 16;
						}
					}



					Runner16 += Image->Width * Image->Height;

					Runner8 += Image->Width * Image->Height;

				}
				break;

			case IL_RXGB:
			case IL_DXT5:

				for (z = 0; z < Image->Depth; z++) {
					for (y = 0; y < Image->Height; y += 4) {
						for (x = 0; x < Image->Width; x += 4) {
							GetAlphaBlock(AlphaBlock, Runner8, Image, x, y);
							ChooseAlphaEndpoints(AlphaBlock, &a0, &a1);
							GenAlphaBitMask(a0, a1, AlphaBlock, AlphaBitMask, NULL/*AlphaOut*/);
							/*Rms2 = RMSAlpha(AlphaBlock, AlphaOut);
							GenAlphaBitMask(a0, a1, 8, AlphaBlock, AlphaBitMask, AlphaOut);
							Rms1 = RMSAlpha(AlphaBlock, AlphaOut);
							if (Rms2 <= Rms1) {  // Yeah, we have to regenerate...
								GenAlphaBitMask(a0, a1, 6, AlphaBlock, AlphaBitMask, AlphaOut);
								Rms2 = a1;  // Just reuse Rms2 as a temporary variable...
								a1 = a0;
								a0 = Rms2;
							}*/
							iputc(a0);
							iputc(a1);
							iwrite(AlphaBitMask, 1, 6);

							GetBlock(Block, Runner16, Image, x, y);
							ChooseEndpoints(Block, &t0, &t1);
							ex0 = IL_MAX(t0, t1);
							ex1 = IL_MIN(t0, t1);
							CorrectEndDXT1(&ex0, &ex1, 0);
							SaveLittleUShort(ex0);
							SaveLittleUShort(ex1);
							BitMask = GenBitMask(ex0, ex1, 4, Block, NULL, NULL);
							SaveLittleUInt(BitMask);
							Count += 16;
						}
					}



					Runner16 += Image->Width * Image->Height;

					Runner8 += Image->Width * Image->Height;

				}
				break;
		}

		ifree(Data);
		ifree(Alpha);
	} //else no 3dc

	return Count;
}