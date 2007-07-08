//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_texture.c - load & convert textures
//=======================================================================

#include "platform.h"
#include "baseutils.h"

//global image variables

int image_width, image_height;
byte image_num_layers = 1;	// num layers in
byte image_num_mips = 4;	// build mipmaps
uint image_type;		// main type switcher
uint image_flags;		// additional image flags

byte *image_palette;	// palette pointer
byte *image_rgba;		// image pointer (see image_type for details)


/*
==============
LoadBMP
==============
*/
bool LoadBMP( char *name, char *buffer, int filesize )
{
	int	columns, rows, numPixels;
	byte	*buf_p = buffer;
	int	row, column;
	byte	*pixbuf;
	bmp_t	bmpHeader;
	byte	*bmpRGBA;
							//move pointer
	Mem_Copy( bmpHeader.id, buf_p, 2 );			buf_p += 2;
	bmpHeader.fileSize = BuffLittleLong( buf_p );		buf_p += 4;
	bmpHeader.reserved0 = BuffLittleLong( buf_p );		buf_p += 4;
	bmpHeader.bitmapDataOffset = BuffLittleLong( buf_p );	buf_p += 4;
	bmpHeader.bitmapHeaderSize = BuffLittleLong( buf_p );	buf_p += 4;
	bmpHeader.width = BuffLittleLong( buf_p );		buf_p += 4;
	bmpHeader.height = BuffLittleLong( buf_p );		buf_p += 4;
	bmpHeader.planes = BuffLittleShort( buf_p );		buf_p += 2;
	bmpHeader.bitsPerPixel = BuffLittleShort( buf_p );	buf_p += 2;
	bmpHeader.compression = BuffLittleLong( buf_p );		buf_p += 4;
	bmpHeader.bitmapDataSize = BuffLittleLong( buf_p );	buf_p += 4;
	bmpHeader.hRes = BuffLittleLong( buf_p );		buf_p += 4;
	bmpHeader.vRes = BuffLittleLong( buf_p );		buf_p += 4;
	bmpHeader.colors = BuffLittleLong( buf_p );		buf_p += 4;
	bmpHeader.importantColors = BuffLittleLong( buf_p );	buf_p += 4;

	memcpy( bmpHeader.palette, buf_p, sizeof( bmpHeader.palette ));
	if ( bmpHeader.bitsPerPixel == 8 ) buf_p += 1024;

	if ( bmpHeader.id[0] != 'B' && bmpHeader.id[1] != 'M' )
	{ 
		MsgDev( "LoadBMP: only Windows-style BMP files supported (%s)\n", name );
		return false;
	}
	if ( bmpHeader.fileSize != filesize )
	{
		MsgDev( "LoadBMP: header size does not match file size (%d vs. %d) (%s)\n", bmpHeader.fileSize, filesize, name );
		return false;
	}
	if ( bmpHeader.compression != 0 )
	{
		MsgDev( "LoadBMP: only uncompressed BMP files supported (%s)\n", name );
		return false;
	}
	if ( bmpHeader.bitsPerPixel < 8 )
	{
		MsgDev( "LoadBMP: monochrome and 4-bit BMP files not supported (%s)\n", name );
		return false;
	}

	columns = bmpHeader.width;
	rows = bmpHeader.height;
	if ( rows < 0 ) rows = -rows;
	numPixels = columns * rows;

	image_width = columns; 
	image_height = rows;
	image_num_layers = 1;
	image_type = PF_RGBA_32;

	if(bmpHeader.bitsPerPixel == 32) image_flags |= IMAGE_HAS_ALPHA;

	bmpRGBA = Malloc( numPixels * 4 );
	image_rgba = bmpRGBA;

	for ( row = rows-1; row >= 0; row-- )
	{
		pixbuf = bmpRGBA + row * columns * 4;

		for ( column = 0; column < columns; column++ )
		{
			byte red, green, blue, alpha;
			int palIndex, x;
			word shortPixel;

			switch( bmpHeader.bitsPerPixel )
			{
			case 8:
				x = column;
				palIndex = *buf_p++;
				*pixbuf++ = bmpHeader.palette[palIndex][2];
				*pixbuf++ = bmpHeader.palette[palIndex][1];
				*pixbuf++ = bmpHeader.palette[palIndex][0];
				*pixbuf++ = 0xff;
				break;
			case 16:
				shortPixel = * ( unsigned short *)pixbuf;
				pixbuf += 2;
				*pixbuf++ = ( shortPixel & ( 31 << 10 ) ) >> 7;
				*pixbuf++ = ( shortPixel & ( 31 << 5 ) ) >> 2;
				*pixbuf++ = ( shortPixel & ( 31 ) ) << 3;
				*pixbuf++ = 0xff;
				break;

			case 24:
				blue = *buf_p++;
				green = *buf_p++;
				red = *buf_p++;
				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = 255;
				break;
			case 32:
				blue = *buf_p++;
				green = *buf_p++;
				red = *buf_p++;
				alpha = *buf_p++;
				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = alpha;
				break;
			default:
				MsgDev("LoadBMP: illegal pixel_size '%d' in file '%s'\n", bmpHeader.bitsPerPixel, name );
				return false;
			}
		}
	}
	return true;
}

/*
============
LoadPCX
============
*/
bool LoadPCX( char *name, char *buffer, int filesize )
{
	pcx_t pcx;
	byte *pbuf, *pix;
	byte *palette, *fin, *enddata;
	int x, y, dataByte, runLength;

	if (filesize < (int)sizeof(pcx) + 768)
	{
		MsgDev("LoadPCX: Bad file %s\n", name );
		return false;
	}
	fin = buffer;
	
	memcpy(&pcx, fin, sizeof(pcx));
	fin += sizeof(pcx);

	pcx.xmax = LittleShort (pcx.xmax);
	pcx.xmin = LittleShort (pcx.xmin);
	pcx.ymax = LittleShort (pcx.ymax);
	pcx.ymin = LittleShort (pcx.ymin);
	pcx.hres = LittleShort (pcx.hres);
	pcx.vres = LittleShort (pcx.vres);
	pcx.bytes_per_line = LittleShort (pcx.bytes_per_line);
	pcx.palette_type = LittleShort (pcx.palette_type);

	image_width = pcx.xmax + 1 - pcx.xmin;
	image_height = pcx.ymax + 1 - pcx.ymin;
	
	if (pcx.manufacturer != 0x0a || pcx.version != 5 || pcx.encoding != 1 || pcx.bits_per_pixel != 8 || image_width > 4096 || image_height > 4096 || image_width <= 0 || image_height <= 0)
	{
		MsgDev("LoadPCX: Bad file %s\n", name );
		return false;
	}

	palette = buffer + filesize - 768;

	image_num_layers = 1;
	image_type = PF_INDEXED_8;
	image_flags |= IMAGE_HAS_ALPHA;

	if(palette)
	{
		image_palette = Malloc( 768 );
		Mem_Copy( image_palette, palette, 768 );
	}	

	pix = image_rgba = (byte *)Malloc(image_width * image_height * 4);
	if (!image_rgba) return false;
	pbuf = image_rgba + image_width * image_height * 3;
	enddata = palette;

	for (y = 0; y <= pcx.ymax && fin < enddata; y++, pix += pcx.xmax + 1)
	{
		for (x = 0; x <= pcx.xmax && fin < enddata; )
		{
			dataByte = *fin++;
			if((dataByte & 0xC0) == 0xC0)
			{
				if (fin >= enddata) break;
				runLength = dataByte & 0x3F;
				dataByte = *fin++;
			}
			else runLength = 1;
			while(runLength-- > 0) pix[x++] = dataByte;
		}

	}
	return true;
}

/*
=============
LoadTGA
=============
*/
bool LoadTGA( char *name, char *buffer, int filesize )
{
	int x, y, pix_inc, row_inc;
	int red, green, blue, alpha;
	int runlen, alphabits;

	byte *pixbuf, *p;
	const byte *fin, *enddata;
	tga_t targa_header;
	byte palette[256*4];

	if (filesize < 19) return false;

	fin = buffer;
	enddata = fin + filesize;

	targa_header.id_length = *fin++;
	targa_header.colormap_type = *fin++;
	targa_header.image_type = *fin++;

	targa_header.colormap_index = BuffLittleShort( fin );	fin += 2;
	targa_header.colormap_length = BuffLittleShort( fin );	fin += 2;
	targa_header.colormap_size = *fin++;
	targa_header.x_origin = BuffLittleShort( fin );		fin += 2;
	targa_header.y_origin = BuffLittleShort( fin );		fin += 2;
	targa_header.width = image_width = BuffLittleShort( fin );	fin += 2;
	targa_header.height = image_height = BuffLittleShort( fin );fin += 2;
	
	if (image_width > 4096 || image_height > 4096 || image_width <= 0 || image_height <= 0)
	{
		Msg("LoadTGA: invalid size\n");
		return false;
	}

	image_num_layers = 1;
	image_type = PF_RGBA_32;//always exctracted to 32-bit buffer

	targa_header.pixel_size = *fin++;
	targa_header.attributes = *fin++;
	// end of header
 
	// skip TARGA image comment (usually 0 bytes)
	fin += targa_header.id_length;

	// read/skip the colormap if present (note: according to the TARGA spec it
	// can be present even on truecolor or greyscale images, just not used by
	// the image data)
	if (targa_header.colormap_type)
	{
		if (targa_header.colormap_length > 256)
		{
			MsgDev("LoadTGA: only up to 256 colormap_length supported\n");
			return false;
		}
		if (targa_header.colormap_index)
		{
			MsgDev("LoadTGA: colormap_index not supported\n");
			return false;
		}
		image_palette = Malloc(256*4);

		if (targa_header.colormap_size == 24)
		{
			for (x = 0;x < targa_header.colormap_length;x++)
			{
				palette[x*4+2] = *fin++;
				palette[x*4+1] = *fin++;
				palette[x*4+0] = *fin++;
				palette[x*4+3] = 0xff;
			}
		}
		else if (targa_header.colormap_size == 32)
		{

			for (x = 0;x < targa_header.colormap_length;x++)
			{
				palette[x*4+2] = *fin++;
				palette[x*4+1] = *fin++;
				palette[x*4+0] = *fin++;
				palette[x*4+3] = *fin++;
			}
		}
		else
		{
			MsgDev("LoadTGA: Only 32 and 24 bit colormap_size supported\n");
			return false;
		}
		Mem_Copy(image_palette, palette, 256*4 );//copy palette
	}

	// check our pixel_size restrictions according to image_type
	switch (targa_header.image_type & ~8)
	{
	case 2:
		if (targa_header.pixel_size != 24 && targa_header.pixel_size != 32)
		{
			MsgDev("LoadTGA: only 24bit and 32bit pixel sizes supported for type 2 and type 10 images\n");
			return false;
		}
		break;
	case 3:
		// set up a palette to make the loader easier
		for (x = 0; x < 256; x++)
		{
			palette[x*4+2] = x;
			palette[x*4+1] = x;
			palette[x*4+0] = x;
			palette[x*4+3] = 255;
		}
		// fall through to colormap case
	case 1:
		if (targa_header.pixel_size != 8)
		{
			MsgDev("LoadTGA: only 8bit pixel size for type 1, 3, 9, and 11 images supported\n");
			return false;
		}
		break;
	default:
		MsgDev("LoadTGA: Only type 1, 2, 3, 9, 10, and 11 targa RGB images supported, image_type = %i\n", targa_header.image_type);
		return false;
	}

	if (targa_header.attributes & 0x10)
	{
		MsgDev("LoadTGA: origin must be in top left or bottom left, top right and bottom right are not supported\n");
		return false;
	}

	// number of attribute bits per pixel, we only support 0 or 8
	alphabits = targa_header.attributes & 0x0F;
	if (alphabits != 8 && alphabits != 0)
	{
		MsgDev("LoadTGA: only 0 or 8 attribute (alpha) bits supported\n");
		return false;
	}

	image_flags |= alphabits ? IMAGE_HAS_ALPHA : 0;
	image_rgba = Malloc(image_width * image_height * 4);

	if (!image_rgba)
	{
		MsgDev("LoadTGA: not enough memory for %i by %i image\n", image_width, image_height);
		return false;
	}

	// If bit 5 of attributes isn't set, the image has been stored from bottom to top
	if ((targa_header.attributes & 0x20) == 0)
	{
		pixbuf = image_rgba + (image_height - 1)*image_width*4;
		row_inc = -image_width*4*2;
	}
	else
	{
		pixbuf = image_rgba;
		row_inc = 0;
	}

	x = y = 0;
	red = green = blue = alpha = 255;
	pix_inc = 1;
	if ((targa_header.image_type & ~8) == 2) pix_inc = targa_header.pixel_size / 8;

	switch (targa_header.image_type)
	{
	case 1: // colormapped, uncompressed
	case 3: // greyscale, uncompressed
		if (fin + image_width * image_height * pix_inc > enddata)
			break;
		for (y = 0;y < image_height;y++, pixbuf += row_inc)
		{
			for (x = 0;x < image_width;x++)
			{
				p = palette + *fin++ * 4;
				*pixbuf++ = p[0];
				*pixbuf++ = p[1];
				*pixbuf++ = p[2];
				*pixbuf++ = p[3];
			}
		}
		break;
	case 2:
		// BGR or BGRA, uncompressed
		if (fin + image_width * image_height * pix_inc > enddata)
			break;
		if (targa_header.pixel_size == 32 && alphabits)
		{
			for (y = 0;y < image_height;y++, pixbuf += row_inc)
			{
				for (x = 0;x < image_width;x++, fin += pix_inc)
				{
					*pixbuf++ = fin[2];
					*pixbuf++ = fin[1];
					*pixbuf++ = fin[0];
					*pixbuf++ = fin[3];
				}
			}
		}
		else //24 bits
		{
			for (y = 0;y < image_height; y++, pixbuf += row_inc)
			{
				for (x = 0;x < image_width; x++, fin += pix_inc)
				{
					*pixbuf++ = fin[2];
					*pixbuf++ = fin[1];
					*pixbuf++ = fin[0];
					*pixbuf++ = 255;
				}
			}
		}
		break;
	case 9:  // colormapped, RLE
	case 11: // greyscale, RLE
		for (y = 0; y < image_height; y++, pixbuf += row_inc)
		{
			for (x = 0;x < image_width;)
			{
				if (fin >= enddata)
					break; // error - truncated file
				runlen = *fin++;
				if (runlen & 0x80)
				{
					// RLE - all pixels the same color
					runlen += 1 - 0x80;
					if (fin + pix_inc > enddata) break; // error - truncated file
					if (x + runlen > image_width) break; // error - line exceeds width
					p = palette + *fin++ * 4;
					red = p[0];
					green = p[1];
					blue = p[2];
					alpha = p[3];
					for (;runlen--; x++)
					{
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alpha;
					}
				}
				else
				{
					// uncompressed - all pixels different color
					runlen++;
					if (fin + pix_inc * runlen > enddata) break; // error - truncated file
					if (x + runlen > image_width) break; // error - line exceeds width
					for (;runlen--; x++)
					{
						p = palette + *fin++ * 4;
						*pixbuf++ = p[0];
						*pixbuf++ = p[1];
						*pixbuf++ = p[2];
						*pixbuf++ = p[3];
					}
				}
			}
		}
		break;
	case 10:
		// BGR or BGRA, RLE
		if (targa_header.pixel_size == 32 && alphabits)
		{
			for (y = 0;y < image_height;y++, pixbuf += row_inc)
			{
				for (x = 0;x < image_width;)
				{
					if (fin >= enddata)
						break; // error - truncated file
					runlen = *fin++;
					if (runlen & 0x80)
					{
						// RLE - all pixels the same color
						runlen += 1 - 0x80;
						if (fin + pix_inc > enddata)
							break; // error - truncated file
						if (x + runlen > image_width)
							break; // error - line exceeds width
						red = fin[2];
						green = fin[1];
						blue = fin[0];
						alpha = fin[3];
						fin += pix_inc;
						for (;runlen--;x++)
						{
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alpha;
						}
					}
					else
					{
						// uncompressed - all pixels different color
						runlen++;
						if (fin + pix_inc * runlen > enddata)
							break; // error - truncated file
						if (x + runlen > image_width)
							break; // error - line exceeds width
						for (;runlen--;x++, fin += pix_inc)
						{
							*pixbuf++ = fin[2];
							*pixbuf++ = fin[1];
							*pixbuf++ = fin[0];
							*pixbuf++ = fin[3];
						}
					}
				}
			}
		}
		else
		{
			for (y = 0;y < image_height;y++, pixbuf += row_inc)
			{
				for (x = 0;x < image_width;)
				{
					if (fin >= enddata)
						break; // error - truncated file
					runlen = *fin++;
					if (runlen & 0x80)
					{
						// RLE - all pixels the same color
						runlen += 1 - 0x80;
						if (fin + pix_inc > enddata)
							break; // error - truncated file
						if (x + runlen > image_width)
							break; // error - line exceeds width
						red = fin[2];
						green = fin[1];
						blue = fin[0];
						alpha = 255;
						fin += pix_inc;
						for (;runlen--;x++)
						{
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alpha;
						}
					}
					else
					{
						// uncompressed - all pixels different color
						runlen++;
						if (fin + pix_inc * runlen > enddata)
							break; // error - truncated file
						if (x + runlen > image_width)
							break; // error - line exceeds width
						for (;runlen--;x++, fin += pix_inc)
						{
							*pixbuf++ = fin[2];
							*pixbuf++ = fin[1];
							*pixbuf++ = fin[0];
							*pixbuf++ = 255;
						}
					}
				}
			}
		}
		break;
	default:
		// unknown image_type
		break;
	}
	return true;
}

/*
=============
LoadDDS
=============
*/
uint dds_get_linear_size( int width, int height, int depth, int bpp )
{
	uint i, block, BlockSize = 0;

	//right calcualte blocksize
	for(i = 0; i < PF_TOTALCOUNT; i++)
	{
		if(image_type == PixelFormatDescription[i].format)
		{
			block = PixelFormatDescription[i].block;
			break;
		} 
	}

	if(i != PF_TOTALCOUNT) //make sure what match found
	{
		if(block == 0) BlockSize = width * height * bpp;
		else if(block > 0) BlockSize = ((width + 3)/4) * ((height + 3)/4) * depth * block;
		else if(block < 0) BlockSize = width * height * depth * bpp * abs(block);
	}
	return BlockSize;
}

uint dds_get_pixelformat( dds_t *hdr )
{
	uint bits;

	// All volume textures I've seem so far didn't have the DDS_COMPLEX flag set,
	// even though this is normally required. But because noone does set it,
	// also read images without it (TODO: check file size for 3d texture?)
	if (!(hdr->dsCaps.dwCaps2 & DDS_VOLUME)) hdr->dwDepth = 1;

	if (hdr->dsPixelFormat.dwFlags & DDS_FOURCC)
	{
		switch (hdr->dsPixelFormat.dwFourCC)
		{
		case TYPE_DXT1: image_type = PF_DXT1; break;
		case TYPE_DXT2: image_type = PF_DXT2; break;
		case TYPE_DXT3: image_type = PF_DXT3; break;
		case TYPE_DXT4: image_type = PF_DXT4; break;
		case TYPE_DXT5: image_type = PF_DXT5; break;
		case TYPE_ATI1: image_type = PF_ATI1N; break;
		case TYPE_ATI2: image_type = PF_3DC; break;
		case TYPE_RXGB: image_type = PF_RXGB; break;
		case TYPE_$: image_type = PF_ABGR_64; break;
		case TYPE_o: image_type = PF_R_16F; break;
		case TYPE_p: image_type = PF_GR_32F; break;
		case TYPE_q: image_type = PF_ABGR_64F; break;
		case TYPE_r: image_type = PF_R_32F; break;
		case TYPE_s: image_type = PF_GR_64F; break;
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
			if (hdr->dsPixelFormat.dwFlags & DDS_ALPHAPIXELS) image_type = PF_ARGB_32;
			else image_type = PF_RGB_24;
		}
	}

	// setup additional flags
	if( hdr->dwFlags & DDS_COMPLEX )
	{
		image_flags |= (hdr->dsPixelFormat.dwFlags & DDS_CUBEMAP) ? IMAGE_CUBEMAP : 0;
		Msg("Cubemap\n");
	}

	if(image_type == TYPE_DXT2 || image_type == TYPE_DXT4) 
		image_flags |= IMAGE_PREMULT;

	bits = hdr->dsPixelFormat.dwRGBBitCount / 8;

	return dds_get_linear_size( image_width, image_height, image_num_layers, bits );
}

void dds_addjust_volume_texture( dds_t *hdr )
{
	uint bits;
	
	if (hdr->dwDepth <= 1) return;
	bits = hdr->dsPixelFormat.dwRGBBitCount / 8;
	hdr->dwFlags |= DDS_LINEARSIZE;
	hdr->dwLinearSize = dds_get_linear_size( hdr->dwWidth, hdr->dwHeight, hdr->dwDepth, bits );
}

bool LoadDDS( char *name, char *buffer, int filesize )
{
	dds_t	header;
	byte	*fin;
	uint	i, BlockSize, buffsize = 0;

	fin = buffer;

	//swap header
	header.dwIdent = BuffLittleLong(fin);		fin += 4;
	header.dwSize = BuffLittleLong(fin);		fin += 4;
	header.dwFlags = BuffLittleLong(fin);		fin += 4;
	header.dwHeight = BuffLittleLong(fin);		fin += 4;
	header.dwWidth = BuffLittleLong(fin);		fin += 4;
	header.dwLinearSize = BuffLittleLong(fin);	fin += 4;
	header.dwDepth = BuffLittleLong(fin);		fin += 4;
	header.dwMipMapCount = BuffLittleLong(fin);	fin += 4;
	header.dwAlphaBitDepth = BuffLittleLong(fin);	fin += 4;

	for (i = 0; i < 10; i++) 
	{
		header.dwReserved1[i] = BuffLittleLong(fin);
		fin += 4;
	}

	//pixel format
	header.dsPixelFormat.dwSize = BuffLittleLong(fin);	fin += 4;
	header.dsPixelFormat.dwFlags = BuffLittleLong(fin);	fin += 4;
	header.dsPixelFormat.dwFourCC = BuffLittleLong(fin);	fin += 4;
	header.dsPixelFormat.dwRGBBitCount = BuffLittleLong(fin);	fin += 4;
	header.dsPixelFormat.dwRBitMask = BuffLittleLong(fin);	fin += 4;
	header.dsPixelFormat.dwGBitMask = BuffLittleLong(fin);	fin += 4;
	header.dsPixelFormat.dwBBitMask = BuffLittleLong(fin);	fin += 4;
	header.dsPixelFormat.dwAlphaBitMask = BuffLittleLong(fin);	fin += 4;

	//caps
	header.dsCaps.dwCaps1 = BuffLittleLong(fin);	fin += 4;
	header.dsCaps.dwCaps2 = BuffLittleLong(fin);	fin += 4;
	header.dsCaps.dwCaps3 = BuffLittleLong(fin);	fin += 4;
	header.dsCaps.dwCaps4 = BuffLittleLong(fin);	fin += 4;
	header.dwTextureStage = BuffLittleLong(fin);	fin += 4;

	if(header.dwIdent != DDSHEADER) return false; // it's not a dds file, just skip it
	if(header.dwSize != sizeof(dds_t) - 4 ) // size of the structure (minus MagicNum)
	{
		Msg("LoadDDS: %s has corrupt dds header\n", name );
		return false;
	}
	if(header.dsPixelFormat.dwSize != sizeof(dds_pixf_t)) // size of the structure
	{
		Msg("LoadDDS: %s has corrupt pixelformat header\n", name );
		return false;
	}

	image_width = header.dwWidth;
	image_height = header.dwHeight;
	if(header.dwFlags & DDS_DEPTH) image_num_layers = header.dwDepth;
	
	if (image_width > 4096 || image_height > 4096 || image_width <= 0 || image_height <= 0)
	{
		Msg("LoadDDS: %s has invalid size\n", name );
		return false;
	}

	BlockSize = dds_get_pixelformat( &header );// and image type too
	dds_addjust_volume_texture( &header );

	if (image_type == PF_UNKNOWN) 
	{
		Msg("LoadDDS: %s has unknown compression type\n", name );
		return false; //unknown type
	}

	// Microsoft bug, they're not following their own documentation.
	if (!(header.dwFlags & (DDS_LINEARSIZE | DDS_PITCH)) || header.dwLinearSize == 0)
	{
		header.dwFlags |= DDS_LINEARSIZE;
		header.dwLinearSize = BlockSize;
		if (header.dwLinearSize < 8) header.dwLinearSize = 8;
	}

	Msg("Loading dds %s, type: %s\n", name, PixelFormatDescription[image_type-1].name );

	if(header.dwFlags & DDS_MIPMAPCOUNT) 
	{
		int w = image_width;
		int h = image_height;
		int d = image_num_layers;
		int i, mipsize = 0;
		int bits = header.dsPixelFormat.dwRGBBitCount / 8;
		image_num_mips = header.dwMipMapCount;

		//now correct buffer size
		for( i = 0; i < image_num_mips; i++, buffsize += mipsize )
		{
			mipsize = dds_get_linear_size( w, h, d, bits );
			w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1;
		}
		if((filesize - 128) != buffsize)
		{
			Msg("LoadDDS: %s have corrupt mipmaps\n", name );
			return false;
		}
	}
	else 
	{
		image_num_mips = 1;//FIXME
		buffsize = header.dwLinearSize;
	}

	// dds files will be uncompressed on a render. requires minimal of info for set this
	image_rgba = Malloc( buffsize ); 
	Mem_Copy( image_rgba, fin, buffsize );

	return true;
}

/*
=============
LoadJPG
=============
*/
int jpeg_read_byte( void )
{
	// read byte
	jpg_file.curbyte = *jpg_file.buffer++;
	jpg_file.curbit = 0;
	return jpg_file.curbyte;
}

int jpeg_read_word( void )
{
	// read word
	word	i = BuffLittleShort( jpg_file.buffer);
	i = ((i << 8) & 0xFF00) + ((i >> 8) & 0x00FF);
	jpg_file.buffer += 2;
	
	return i;
}

int jpeg_read_bit( void )
{
	// read bit
	register int i;
	if(jpg_file.curbit == 0)
	{
		jpeg_read_byte();
		if(jpg_file.curbyte == 0xFF)
		{
			while(jpg_file.curbyte == 0xFF) jpeg_read_byte();
			if(jpg_file.curbyte >= 0xD0 && jpg_file.curbyte <= 0xD7)
				memset(jpg_file.dc,0,sizeof(int) * 3);
			if(jpg_file.curbyte == 0) jpg_file.curbyte = 0xFF;
			else jpeg_read_byte();
		}
	}

	i = (jpg_file.curbyte >> (7 - jpg_file.curbit++)) & 0x01;
	if(jpg_file.curbit == 8) jpg_file.curbit = 0;

	return i;
}

int jpeg_read_bits( int num )
{
	// read num bit
	register int i, j;

	for(i = 0, j = 0; i < num; i++)
	{
		j <<= 1;
		j |= jpeg_read_bit();
	}
	return j;
}

int jpeg_bit2int( int bit, int i )
{
	// convert bit code to int
	if((i & (1 << (bit - 1))) > 0) return i;
	return -(i ^ ((1 << bit) - 1));
}

int jpeg_huffmancode( huffman_table_t *table )
{
	// get Huffman code
	register int i,size,code;
	for(size = 1, code = 0, i = 0; size < 17; size++)
	{
		code <<= 1;
		code |= jpeg_read_bit();
		while(table->size[i] <= size)
		{
			if(table->code[i] == code)
				return table->hval[i];
			i++;
		}
	}
	return code;
}

void jpeg_idct( float *data )
{
	// aa&n algorithm inverse DCT
	float   t0, t1, t2, t3, t4, t5, t6, t7;
	float   t10, t11, t12, t13;
	float   z5, z10, z11, z12, z13;
	float   *dataptr;
	int i;

	dataptr = data;
    
	for(i = 0; i < 8; i++)
	{
		t0 = dataptr[8 * 0];
		t1 = dataptr[8 * 2];
		t2 = dataptr[8 * 4];
		t3 = dataptr[8 * 6];

		t10 = t0 + t2;
		t11 = t0 - t2;
		t13 = t1 + t3;
		t12 = - t13 + (t1 - t3) * 1.414213562;//??
        
		t0 = t10 + t13;
		t3 = t10 - t13;
		t1 = t11 + t12;
		t2 = t11 - t12;
		t4 = dataptr[8 * 1];
		t5 = dataptr[8 * 3];
		t6 = dataptr[8 * 5];
		t7 = dataptr[8 * 7];
        
		z13 = t6 + t5;
		z10 = t6 - t5;
		z11 = t4 + t7;
		z12 = t4 - t7;
        
		t7 = z11 + z13;
		t11 = (z11 - z13) * 1.414213562;
		z5 = (z10 + z12) * 1.847759065;
		t10 = - z5 + z12 * 1.082392200;
		t12 = z5 - z10 * 2.613125930;
		t6 = t12 - t7;
		t5 = t11 - t6;
		t4 = t10 + t5;

		dataptr[8 * 0] = t0 + t7;
		dataptr[8 * 7] = t0 - t7;
		dataptr[8 * 1] = t1 + t6;
		dataptr[8 * 6] = t1 - t6;
		dataptr[8 * 2] = t2 + t5;
		dataptr[8 * 5] = t2 - t5;
		dataptr[8 * 4] = t3 + t4;
		dataptr[8 * 3] = t3 - t4;
		dataptr++;
	}

	dataptr = data;

	for(i = 0; i < 8; i++)
	{
		t10 = dataptr[0] + dataptr[4];
		t11 = dataptr[0] - dataptr[4];
		t13 = dataptr[2] + dataptr[6];
		t12 = - t13 + (dataptr[2] - dataptr[6]) * 1.414213562;//??

		t0 = t10 + t13;
		t3 = t10 - t13;
		t1 = t11 + t12;
		t2 = t11 - t12;
        
		z13 = dataptr[5] + dataptr[3];
		z10 = dataptr[5] - dataptr[3];
		z11 = dataptr[1] + dataptr[7];
		z12 = dataptr[1] - dataptr[7];

		t7 = z11 + z13;
		t11 = (z11 - z13) * 1.414213562;
		z5 = (z10 + z12) * 1.847759065;
		t10 = - z5 + z12 * 1.082392200;
		t12 = z5 - z10 * 2.613125930;
        
		t6 = t12 - t7;
		t5 = t11 - t6;
		t4 = t10 + t5;

		dataptr[0] = t0 + t7;
		dataptr[7] = t0 - t7;
		dataptr[1] = t1 + t6;
		dataptr[6] = t1 - t6;
		dataptr[2] = t2 + t5;
		dataptr[5] = t2 - t5;
		dataptr[4] = t3 + t4;
		dataptr[3] = t3 - t4;
		dataptr += 8;//move ptr
	}
}

int jpeg_readmarkers( void )
{
	// read jpeg markers
	int marker, length, i, j, k, l, m;
	huffman_table_t *hptr;

	while( 1 )
	{
		marker = jpeg_read_byte();
		if(marker != 0xFF) return 0;

		marker = jpeg_read_byte();
		if(marker != 0xD8)
		{
			length = jpeg_read_word();
			length -= 2;

			switch(marker)
			{
			case 0xC0:  // Baseline
				jpg_file.data_precision = jpeg_read_byte();
				jpg_file.height = jpeg_read_word();
				jpg_file.width = jpeg_read_word();
				jpg_file.num_components = jpeg_read_byte();
				if(length - 6 != jpg_file.num_components * 3) return 0;
				
				for(i = 0; i < jpg_file.num_components; i++)
				{
					jpg_file.component_info[i].id = jpeg_read_byte();
					j = jpeg_read_byte();
					jpg_file.component_info[i].h = (j >> 4) & 0x0F;
					jpg_file.component_info[i].v = j & 0x0F;
					jpg_file.component_info[i].t = jpeg_read_byte();
				}
				break;
			case 0xC1:  // Extended sequetial, Huffman
			case 0xC2:  // Progressive, Huffman            
				jpg_file.data_precision = jpeg_read_byte();
				jpg_file.height = jpeg_read_word();
				jpg_file.width = jpeg_read_word();
				jpg_file.num_components = 2 + 3 * jpeg_read_byte();
				if(length - 4 != jpg_file.num_components)
				{
					Msg("%d == %d\n", length, jpg_file.num_components );
					return 0;
				}
				for(i = 0; i < jpg_file.num_components; i++)
				{
					jpg_file.component_info[i].id = jpeg_read_byte();
					j = jpeg_read_byte();
					jpg_file.component_info[i].h = (j >> 4) & 0x0F;
					jpg_file.component_info[i].v = j & 0x0F;
					jpg_file.component_info[i].t = jpeg_read_byte();
				}
				break;
			case 0xC3:  // Lossless, Huffman
			case 0xC5:  // Differential sequential, Huffman
			case 0xC6:  // Differential progressive, Huffman
			case 0xC7:  // Differential lossless, Huffman
			case 0xC8:  // Reserved for JPEG extensions
			case 0xC9:  // Extended sequential, arithmetic
			case 0xCA:  // Progressive, arithmetic
			case 0xCB:  // Lossless, arithmetic
			case 0xCD:  // Differential sequential, arithmetic
			case 0xCE:  // Differential progressive, arithmetic
			case 0xCF:  // Differential lossless, arithmetic
				return 0;
			case 0xC4:  // Huffman table
				while(length > 0)
				{
					k = jpeg_read_byte();
					if(k & 0x10) hptr = &jpg_file.hac[k & 0x0F];
					else hptr = &jpg_file.hdc[k & 0x0F];
					for(i = 0, j = 0; i < 16; i++)
					{
						hptr->bits[i] = jpeg_read_byte();
						j += hptr->bits[i];
					}
					length -= 17;
					for(i = 0; i < j; i++) hptr->hval[i] = jpeg_read_byte();
					length -= j;

					for(i = 0, k = 0, l = 0; i < 16; i++)
					{
						for(j = 0; j < hptr->bits[i]; j++, k++)
						{
							hptr->size[k] = i + 1;
							hptr->code[k] = l++;
						}
						l <<= 1;
					}
				}
				break;
			case 0xDB:  // Quantization table
				while(length > 0)
				{
					j = jpeg_read_byte();
					k = (j >> 4) & 0x0F;
					for(i = 0; i < 64; i++)
					{
						if( k )jpg_file.qtable[j][i] = jpeg_read_word();
						else jpg_file.qtable[j][i] = jpeg_read_byte();
					}
					length -= 65;
					if( k )length -= 64;
				}
				break;
			case 0xD9:  // End of image (EOI)
				return 0;
			case 0xDA:  // Start of scan (SOS)
				j = jpeg_read_byte();
				for(i = 0; i < j; i++)
				{
					k = jpeg_read_byte();
					m = jpeg_read_byte();
					for(l = 0; l < jpg_file.num_components; l++)
					{
						if(jpg_file.component_info[l].id == k)
						{
							jpg_file.component_info[l].td = (m >> 4) & 0x0F;
							jpg_file.component_info[l].ta = m & 0x0F;
						}
					}
				}

				jpg_file.scan.ss = jpeg_read_byte();
				jpg_file.scan.se = jpeg_read_byte();
				k = jpeg_read_byte();
				jpg_file.scan.ah = (k >> 4) & 0x0F;
				jpg_file.scan.al = k & 0x0F;
				return 1;
			case 0xDD:  // Restart interval
				jpg_file.restart_interval = jpeg_read_word();
				break;
			default:
				jpg_file.buffer += length; //move ptr
				break;
			}
		}
	}
}


void jpeg_decompress( void )
{
	// decompress jpeg file (Baseline algorithm)
	register int x, y, i, j, k, l, c;
	int X, Y, H, V, plane, scaleh[3], scalev[3];
    	static float vector[64], dct[64];
    
	static const int jpeg_zigzag[64] = {
	0,  1,  5,  6,  14, 15, 27, 28,
	2,  4,  7,  13, 16, 26, 29, 42,
	3,  8,  12, 17, 25, 30, 41, 43,
	9,  11, 18, 24, 31, 40, 44, 53,
	10, 19, 23, 32, 39, 45, 52, 54,
	20, 22, 33, 38, 46, 51, 55, 60,
	21, 34, 37, 47, 50, 56, 59, 61,
	35, 36, 48, 49, 57, 58, 62, 63
	};

	// 1.0, k = 0; cos(k * PI / 16) * sqrt(2), k = 1...7
	static const float aanscale[8] = {
	1.0, 1.387039845, 1.306562965, 1.175875602,
	1.0, 0.785694958, 0.541196100, 0.275899379
	};

	scaleh[0] = 1;
	scalev[0] = 1;
    
	if(jpg_file.num_components == 3)
	{
		scaleh[1] = jpg_file.component_info[0].h / jpg_file.component_info[1].h;
		scalev[1] = jpg_file.component_info[0].v / jpg_file.component_info[1].v;
		scaleh[2] = jpg_file.component_info[0].h / jpg_file.component_info[2].h;
		scalev[2] = jpg_file.component_info[0].v / jpg_file.component_info[2].v;
	}
	memset(jpg_file.dc,0,sizeof(int) * 3);

	for(Y = 0; Y < jpg_file.height; Y += jpg_file.component_info[0].v << 3)
	{
		if(jpg_file.restart_interval > 0) jpg_file.curbit = 0;
		for(X = 0; X < jpg_file.width; X += jpg_file.component_info[0].h << 3)
		{
			for(plane = 0; plane < jpg_file.num_components; plane++)
			{
                			for(V = 0; V < jpg_file.component_info[plane].v; V++)
                			{
					for(H = 0; H < jpg_file.component_info[plane].h; H++)
					{
						i = jpeg_huffmancode(&jpg_file.hdc[jpg_file.component_info[plane].td]);
						i &= 0x0F;
						vector[0] = jpg_file.dc[plane] + jpeg_bit2int(i,jpeg_read_bits(i));
						jpg_file.dc[plane] = vector[0];
						i = 1;

						while(i < 64)
						{
							j = jpeg_huffmancode(&jpg_file.hac[jpg_file.component_info[plane].ta]);
							if(j == 0) while(i < 64) vector[i++] = 0;
							else
							{
								k = i + ((j >> 4) & 0x0F);
								while(i < k) vector[i++] = 0;
								j &= 0x0F;
								vector[i++] = jpeg_bit2int(j,jpeg_read_bits(j));
							}
						}

						k = jpg_file.component_info[plane].t;
						for(y = 0, i = 0; y < 8; y++)
						{
							for(x = 0; x < 8; x++, i++)
							{
								j = jpeg_zigzag[i];
								dct[i] = vector[j] * jpg_file.qtable[k][j] * aanscale[x] * aanscale[y];
							}
						}

						jpeg_idct(dct);
						for(y = 0; y < 8; y++)
						{
							for(x = 0; x < 8; x++)
							{
								c = ((int)dct[(y << 3) + x] >> 3) + 128;
								if(c < 0) c = 0;
								else if(c > 255) c = 255;

								if(scaleh[plane] == 1 && scalev[plane] == 1)
								{
									i = X + x + (H << 3);
									j = Y + y + (V << 3);
									if(i < jpg_file.width && j < jpg_file.height)
										jpg_file.data[((j * jpg_file.width + i) << 2) + plane] = c;
								}
								else for(l = 0; l < scalev[plane]; l++)//else for, heh...
								{
									for(k = 0; k < scaleh[plane]; k++)
									{
										i = X + (x + (H << 3)) * scaleh[plane] + k;
										j = Y + (y + (V << 3)) * scalev[plane] + l;
										if(i < jpg_file.width && j < jpg_file.height)
											jpg_file.data[((j * jpg_file.width + i) << 2) + plane] = c;
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void jpeg_ycbcr2rgba( void )
{
	int i, Y, Cb, Cr, R, G, B;

	// convert YCbCr image to RGBA
	for(i = 0; i < jpg_file.width * jpg_file.height << 2; i += 4)
	{
		Y = jpg_file.data[i];
		Cb = jpg_file.data[i + 1] - 128;
		Cr = jpg_file.data[i + 2] - 128;

		R = Y + 1.40200 * Cr;
		G = Y - 0.34414 * Cb - 0.71414 * Cr;
		B = Y + 1.77200 * Cb;

		// bound colors
		if(R < 0) R = 0;
		else if(R > 255) R = 255;
		if(G < 0) G = 0;
		else if(G > 255) G = 255;
		if(B < 0) B = 0;
		else if(B > 255) B = 255;

		jpg_file.data[i + 0] = R;
		jpg_file.data[i + 1] = G;
		jpg_file.data[i + 2] = B;
		jpg_file.data[i + 3] = 0xff;//alpha channel
	}
}

void jpeg_gray2rgba( void )
{
	int i, j;

	// grayscale image to RGBA
	for(i = 0; i < jpg_file.width * jpg_file.height << 2; i += 4)
	{
		j = jpg_file.data[i];
		jpg_file.data[i + 0] = j;
		jpg_file.data[i + 1] = j;
		jpg_file.data[i + 2] = j;
		jpg_file.data[i + 3] = 0xff;
	}
}

bool LoadJPG(char *name, char *buffer, int filesize )
{
	memset(&jpg_file, 0, sizeof(jpg_file));
	jpg_file.buffer = buffer;

	if(!jpeg_readmarkers())//read header
	{
		Msg("LoadJPG: readmarkers did not find jpg\n");
		return false;
	}
	image_width = jpg_file.width;
	image_height = jpg_file.height;
	image_type = PF_RGBA_32;

	if (image_width > 4096 || image_height > 4096 || image_width <= 0 || image_height <= 0)
	{
		Msg("LoadJPG: invalid size\n");
		return false;
	}	

	jpg_file.data = Malloc(jpg_file.width * jpg_file.height * 4);
	if(!jpg_file.data) return false;

	jpeg_decompress();
	if(jpg_file.num_components == 1) jpeg_gray2rgba();        
	if(jpg_file.num_components == 3) jpeg_ycbcr2rgba();

	image_num_layers = 1;
	image_rgba = jpg_file.data;

	return true;
}

typedef struct imageformat_s
{
	char *formatstring;
	bool (*loadfunc)(char *name, char *buffer, int filesize);
}
imageformat_t;

imageformat_t image_formats[] =
{
	{"textures/%s.dds", LoadDDS},
	{"textures/%s.tga", LoadTGA},
	{"textures/%s.bmp", LoadBMP},
	{"textures/%s.jpg", LoadJPG},
	{"textures/%s.pcx", LoadPCX},
	{"%s.dds", LoadDDS},
	{"%s.tga", LoadTGA},
	{"%s.bmp", LoadBMP},
	{"%s.jpg", LoadJPG},
	{"%s.pcx", LoadPCX},
	{NULL, NULL}
};

rgbdata_t *ImagePack( void )
{
	rgbdata_t *pack = Malloc(sizeof(rgbdata_t));

	pack->width = image_width;
	pack->height = image_height;
	pack->numLayers = image_num_layers;
	pack->numMips = image_num_mips;
	pack->type = image_type;
	pack->flags = image_flags;
	pack->palette = image_palette;
	pack->buffer = image_rgba;

	return pack;
}

/*
================
FS_LoadImage

loading and unpack to rgba any known image
================
*/

rgbdata_t *FS_LoadImage(const char *filename, char *buffer, int buffsize )
{
	imageformat_t	*format;
	char path[128], loadname[128], texname[128];
	int filesize = 0;
	byte *f;

	memcpy( loadname, filename, sizeof(loadname));
	FS_StripExtension( loadname );//remove extension if needed

	// now try all the formats in the selected list
	for (format = image_formats;format->formatstring; format++)
	{
		sprintf (path, format->formatstring, loadname);
		f = FS_LoadFile( path, &filesize );
		if(f && filesize > 0)
		{
			//this name will be used only for
			//tell user about a imageload problems 
			FS_FileBase( path, texname );
			if( format->loadfunc(texname, f, filesize ))
				return ImagePack(); //loaded
		}
	}

	// try to load image from const buffer (e.g. const byte blank_frame )
	memcpy( texname, filename, sizeof(texname));

	for (format = image_formats; format->formatstring; format++)
	{
		if(buffer && buffsize > 0)
		{
			//this name will be used only for
			//tell user about a imageload problems 
			FS_FileBase( loadname, texname );
			if( format->loadfunc(texname, buffer, buffsize ))
				return ImagePack(); //loaded
		}
	}

	MsgDev("couldn't load %s\n", texname );
	return NULL;
}

void FS_FreeImage( rgbdata_t *pack )
{
	//reset global variables
	image_width = image_height = image_flags = 0;
	image_num_layers = 1;
	image_num_mips = 4;
	image_type = PF_UNKNOWN;
	image_palette = NULL;
	image_rgba = NULL;
	
	if( !pack ) return;
	
	if( pack->buffer ) Free( pack->buffer );
	if( pack->palette ) Free( pack->palette );
	Free( pack );
}