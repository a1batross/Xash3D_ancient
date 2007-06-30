//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_texture.c - load & convert textures
//=======================================================================

#include "imglib.h"
#include "baseutils.h"

#pragma comment(lib, "formats/jpg")
#pragma comment(lib, "formats/png")

int image_width, image_height;
int image_bpp;
uint image_compression;
byte image_alpha;
byte *image_palette;
byte *image_rgba;

// note: reneder has four transparent modes	
// 0 - no alpha.
// 1 - 8-bit alpha unpaletted
// 2 - two modes for last palette color (0 = 255, 1 = 128 )
// 3 - palette last color is transparent (0 = 255 )
// 4 - get color percent from internal table

typedef struct imageformat_s
{
	char *formatstring;
	bool (*loadfunc)(char *name, char *buffer, int filesize);
}
imageformat_t;

imageformat_t image_formats[] =
{
	{"textures/%s.tga", LoadTGA},
	{"textures/%s.bmp", LoadBMP},
	{"textures/%s.pcx", LoadPCX},
	{"textures/%s.dds", LoadDDS},
	{"textures/%s.jpg", LoadJPG},
	{"textures/%s.png", LoadPNG},
	{"%s.tga", LoadTGA},
	{"%s.bmp", LoadBMP},
	{"%s.pcx", LoadPCX},
	{"%s.dds", LoadDDS},
	{"%s.jpg", LoadJPG},
	{"%s.png", LoadPNG},
	{"%s", LoadPCX}, //special case for loading skins
	{NULL, NULL}
};

rgbdata_t *ImagePack( void )
{
	rgbdata_t *pack = Malloc(sizeof(rgbdata_t));

	pack->width = image_width;
	pack->height = image_height;
	pack->bitsperpixel = image_bpp;
	pack->compression = image_compression;
	pack->alpha = image_alpha;
	pack->palette = image_palette;
	pack->buffer = image_rgba;

	return pack;
}

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
	FS_FileBase( loadname, texname );

	for (format = image_formats;format->formatstring; format++)
	{
		if(buffer && buffsize > 0)
		{
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
	image_width = image_height = 0;
	image_alpha = image_bpp = 0;
	image_compression = 0;
	image_palette = NULL;
	image_rgba = NULL;
	
	if( !pack ) return;
	
	if( pack->buffer ) Free( pack->buffer );
	if( pack->palette ) Free( pack->palette );
	Free( pack );
}

/*
==============
LoadBMP
==============
*/
bool LoadBMP( char *name, char *buffer, int filesize )
{
	int	columns, rows, numPixels;
	byte	*buf_p;
	int	row, column;
	byte	*pixbuf;
	bmp_t	bmpHeader;
	byte	*bmpRGBA;

	buf_p = buffer;

	bmpHeader.id[0] = *buf_p++;
	bmpHeader.id[1] = *buf_p++;					//move pointer
	bmpHeader.fileSize = LittleLong( * ( long * ) buf_p );		buf_p += 4;
	bmpHeader.reserved0 = LittleLong( * ( long * ) buf_p );		buf_p += 4;
	bmpHeader.bitmapDataOffset = LittleLong( * ( long * ) buf_p );	buf_p += 4;
	bmpHeader.bitmapHeaderSize = LittleLong( * ( long * ) buf_p );	buf_p += 4;
	bmpHeader.width = LittleLong( * ( long * ) buf_p );		buf_p += 4;
	bmpHeader.height = LittleLong( * ( long * ) buf_p );		buf_p += 4;
	bmpHeader.planes = LittleShort( * ( short * ) buf_p );		buf_p += 2;
	bmpHeader.bitsPerPixel = LittleShort( * ( short * ) buf_p );	buf_p += 2;
	bmpHeader.compression = LittleLong( * ( long * ) buf_p );		buf_p += 4;
	bmpHeader.bitmapDataSize = LittleLong( * ( long * ) buf_p );	buf_p += 4;
	bmpHeader.hRes = LittleLong( * ( long * ) buf_p );		buf_p += 4;
	bmpHeader.vRes = LittleLong( * ( long * ) buf_p );		buf_p += 4;
	bmpHeader.colors = LittleLong( * ( long * ) buf_p );		buf_p += 4;
	bmpHeader.importantColors = LittleLong( * ( long * ) buf_p );	buf_p += 4;

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
	image_bpp = 32;//bmpHeader.bitsPerPixel;

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

	image_bpp = pcx.bits_per_pixel;
	palette = buffer + filesize - 768;
	image_alpha = 1;//first alpha type always

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

	byte *pixbuf;
	const byte *fin, *enddata;
	byte *p, *f;
	tga_t targa_header;
	byte palette[256*4];

	if (filesize < 19) return false;

	f = buffer;
	enddata = f + filesize;

	targa_header.id_length = f[0];
	targa_header.colormap_type = f[1];
	targa_header.image_type = f[2];

	targa_header.colormap_index = f[3] + f[4] * 256;
	targa_header.colormap_length = f[5] + f[6] * 256;
	targa_header.colormap_size = f[7];
	targa_header.x_origin = f[8] + f[9] * 256;
	targa_header.y_origin = f[10] + f[11] * 256;
	targa_header.width = image_width = f[12] + f[13] * 256;
	targa_header.height = image_height = f[14] + f[15] * 256;
	
	if (image_width > 4096 || image_height > 4096 || image_width <= 0 || image_height <= 0)
	{
		Msg("LoadTGA: invalid size\n");
		return false;
	}

	targa_header.pixel_size = f[16];
	targa_header.attributes = f[17];

	// advance to end of header
	fin = f + 18;
 
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

	image_bpp = 32;//LoadTGA always return 32 bit image buffer;
	image_alpha = alphabits ? true : false;
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
LoadPNG
=============
*/
void PNG_fReadData(void *png, byte *data, size_t length)
{
	size_t l;
	l = png_buf.tmpBuflength - png_buf.tmpi;
	if (l < length)
	{
		Msg("LoadPNG: overrun by %i bytes\n", length - l);
		// a read going past the end of the file, fill in the remaining bytes
		// with 0 just to be consistent
		memset(data + l, 0, length - l);
		length = l;
	}
	memcpy(data, png_buf.tmpBuf + png_buf.tmpi, length);
	png_buf.tmpi += (int)length;
}

void png_errorfn(void *png, const char *message)
{
	Msg("LoadPNG: error: %s\n", message);
}

void png_warningfn(void *png, const char *message)
{
	Msg("LoadPNG: warning: %s\n", message);
}

bool LoadPNG( char *name, char *buffer, int filesize )
{
	unsigned int y;
	void *png, *pnginfo;
	byte ioBuffer[8192];

	if(png_sig_cmp((char*)buffer, 0, filesize)) return false;
	png = (void *)png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, (void *)png_errorfn, (void *)png_warningfn);
	if(!png) return false;

	if (setjmp((int*)png))
	{
		png_destroy_read_struct(&png, &pnginfo, 0);
		return false;
	}
          
	pnginfo = png_create_info_struct(png);
	if(!pnginfo)
	{
		png_destroy_read_struct(&png, &pnginfo, 0);
		return false;
	}
	png_set_sig_bytes(png, 0);

	memset(&png_buf, 0, sizeof(png_buf));
	png_buf.tmpBuf = buffer;
	png_buf.tmpBuflength = filesize;
	png_buf.tmpi = 0;
	png_buf.ColorType	= PNG_COLOR_TYPE_RGB;
	png_set_read_fn(png, ioBuffer, (void *)PNG_fReadData);
	png_read_info(png, pnginfo);
	png_get_IHDR(png, pnginfo, &png_buf.Width, &png_buf.Height,&png_buf.BitDepth, &png_buf.ColorType, &png_buf.Interlace, &png_buf.Compression, &png_buf.Filter);

	if (png_buf.ColorType == PNG_COLOR_TYPE_PALETTE)
	{
		png_set_palette_to_rgb(png);
		png_set_filler(png, 255, 1);			
	}

	if (png_buf.ColorType == PNG_COLOR_TYPE_GRAY && png_buf.BitDepth < 8) 
	{
		png_set_gray_1_2_4_to_8(png);
	}
	
	if (png_get_valid( png, pnginfo, PNG_INFO_tRNS ))		
	{
		png_set_tRNS_to_alpha(png); 
          }
          
	if (png_buf.BitDepth >= 8 && png_buf.ColorType == PNG_COLOR_TYPE_RGB)	
	{
		png_set_filler(png, 255, 1);
	}
	
	if (png_buf.ColorType == PNG_COLOR_TYPE_GRAY || png_buf.ColorType == PNG_COLOR_TYPE_GRAY_ALPHA)
	{
		png_set_gray_to_rgb( png );
		png_set_filler(png, 255, 1);			
	}

	if (png_buf.BitDepth < 8) png_set_expand(png);
	else if (png_buf.BitDepth == 16) png_set_strip_16(png);

	png_read_update_info(png, pnginfo);
	png_buf.FRowBytes = png_get_rowbytes(png, pnginfo);
	png_buf.BytesPerPixel = png_get_channels(png, pnginfo);

	png_buf.FRowPtrs = (byte **)Malloc( png_buf.Height * sizeof(*png_buf.FRowPtrs));

	if (png_buf.FRowPtrs)
	{
		png_buf.Data = (byte *)Malloc( png_buf.Height * png_buf.FRowBytes);
		if(png_buf.Data)
		{
			for(y = 0; y < png_buf.Height; y++)
				png_buf.FRowPtrs[y] = png_buf.Data + y * png_buf.FRowBytes;
			png_read_image(png, png_buf.FRowPtrs);
		}
		else MsgDev("PNG_LoadImage : not enough memory\n");
		Free(png_buf.FRowPtrs);
	}
	else MsgDev("PNG_LoadImage : not enough memory\n");

	png_read_end(png, pnginfo);
	png_destroy_read_struct(&png, &pnginfo, 0);

	//return image info
	image_width = png_buf.Width;
	image_height = png_buf.Height;
          
	image_bpp = 32;//png_buf.BitDepth;
	image_rgba = png_buf.Data;
	
	return true;
}

/*
=============
LoadDDS
=============
*/
bool LoadDDS( char *name, char *buffer, int filesize )
{
	dds_t	*header;
	byte	*fin;
	size_t	imgsize;

	fin = buffer;

	if(memcmp(fin, "DDS", 3 ))
	{
		Msg("LoadDDS: invalid dds file %s\n", name );
		return false;
	}
	fin += 4; //skip signature

	header = Malloc(sizeof(dds_t));
	Mem_Copy(header, fin, sizeof(dds_t));

	if(header->dwSize != sizeof(dds_t)) 
	{
		Msg("LoadDDS: corrupt or unsuppotred dds file %s \n", name );
		return false;
	}
	fin += header->dwSize;

	// dds files will be uncompressed on a render. requires minimal of info for set this
	imgsize = header->dwPitchOrLinearSize;
	image_width = header->dwWidth;
	image_height = header->dwHeight;
	image_bpp = header->dwDepth;
	image_compression = 0;

	//merge info
	if( imgsize < 8) imgsize = 8;
	if(!image_bpp) image_bpp = (imgsize / (image_width * image_height)) * 8;
	if(header->dwMipMapCount > 1) return false;// unsupported

	//packed DXT_FORMAT into image_alpha
	if (*(int*)&header->ddpfPixelFormat.dwFourCC == *(int*)"DXT1") 
	{
		image_compression = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
	}
	else if (*(int*)&header->ddpfPixelFormat.dwFourCC == *(int*)"DXT3") 
	{
		image_compression = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
	}
	else if (*(int*)&header->ddpfPixelFormat.dwFourCC == *(int*)"DXT5")
	{
		image_compression = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
	}
	else
	{
		Msg("LoadDDS: %s unsuppotred dxt type %d\n", name, header->ddpfPixelFormat.dwFourCC );
		return false;
	}
	
	//copy image data
	image_rgba = Malloc( imgsize ); 
	Mem_Copy( image_rgba, fin, imgsize );

	return true;
}

/*
=============
LoadJPG
=============
*/
byte jpeg_eoi_marker [2] = {0xFF, JPEG_EOI};
bool error_in_jpeg;
void JPEG_Noop(j_decompress_ptr cinfo) {}

jboolean JPEG_FillInputBuffer (j_decompress_ptr cinfo)
{
	// Insert a fake EOI marker
	cinfo->src->next_input_byte = jpeg_eoi_marker;
	cinfo->src->bytes_in_buffer = 2;
	return TRUE;
}

void JPEG_SkipInputData (j_decompress_ptr cinfo, long num_bytes)
{
	if (cinfo->src->bytes_in_buffer <= (unsigned long)num_bytes)
	{
		cinfo->src->bytes_in_buffer = 0;
		return;
	}

	cinfo->src->next_input_byte += num_bytes;
	cinfo->src->bytes_in_buffer -= num_bytes;
}

void JPEG_MemSrc (j_decompress_ptr cinfo, const unsigned char *buffer, size_t filesize)
{
	cinfo->src = (struct jpeg_source_mgr *)cinfo->mem->alloc_small ((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof (struct jpeg_source_mgr));
	cinfo->src->next_input_byte = buffer;
	cinfo->src->bytes_in_buffer = filesize;

	cinfo->src->init_source = JPEG_Noop;
	cinfo->src->fill_input_buffer = JPEG_FillInputBuffer;
	cinfo->src->skip_input_data = JPEG_SkipInputData;
	cinfo->src->resync_to_restart = jpeg_resync_to_restart; // use the default method
	cinfo->src->term_source = JPEG_Noop;
}

void JPEG_ErrorExit (j_common_ptr cinfo)
{
	((struct jpeg_decompress_struct*)cinfo)->err->output_message (cinfo);
	error_in_jpeg = true;
}

bool LoadJPG( char *name, char *buffer, int filesize )
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	byte *scanline;
	dword line;
		
	cinfo.err = jpeg_std_error (&jerr);
	jpeg_create_decompress (&cinfo);
	JPEG_MemSrc (&cinfo, buffer, filesize);
	jpeg_read_header (&cinfo, TRUE);
	jpeg_start_decompress (&cinfo);

	image_width = cinfo.image_width;
	image_height = cinfo.image_height;

	if (image_width > 4096 || image_height > 4096 || image_width <= 0 || image_height <= 0)
	{
		MsgDev("LoadJPG: invalid image size %ix%i\n", image_width, image_height);
		return false;
	}

	image_rgba = (unsigned char *)Malloc(image_width * image_height * 4);
	scanline = (unsigned char *)Malloc(image_width * cinfo.output_components);
	if (!image_rgba || !scanline)
	{
		if (image_rgba) Free (image_rgba);
		if (scanline) Free (scanline);

		MsgDev("LoadJPG: not enough memory for %i by %i image\n", image_width, image_height);
		jpeg_finish_decompress (&cinfo);
		jpeg_destroy_decompress (&cinfo);
		return false;
	}

	image_bpp = 32;//cinfo.output_components == 3 ? 24 : 8;

	// Decompress the image, line by line
	line = 0;
	while (cinfo.output_scanline < cinfo.output_height)
	{
		byte *buffer_ptr;
		int ind;

		jpeg_read_scanlines (&cinfo, &scanline, 1);

		// Convert the image to RGBA
		switch (cinfo.output_components)
		{
			// RGB images
			case 3:
				buffer_ptr = &image_rgba[image_width * line * 4];
				for (ind = 0; ind < image_width * 3; ind += 3, buffer_ptr += 4)
				{
					buffer_ptr[0] = scanline[ind];
					buffer_ptr[1] = scanline[ind + 1];
					buffer_ptr[2] = scanline[ind + 2];
					buffer_ptr[3] = 0xff;
				}
				break;

			// Greyscale images (default to it, just in case)
			case 1:
			default:
				buffer_ptr = &image_rgba[image_width * line * 4];
				for (ind = 0; ind < image_width; ind++, buffer_ptr += 4)
				{
					buffer_ptr[0] = scanline[ind];
					buffer_ptr[1] = scanline[ind];
					buffer_ptr[2] = scanline[ind];
					buffer_ptr[3] = 0xff;
				}
		}

		line++;
	}
	Free (scanline);

	jpeg_finish_decompress (&cinfo);
	jpeg_destroy_decompress (&cinfo);

	return true;
}