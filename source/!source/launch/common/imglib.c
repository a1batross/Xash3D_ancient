//=======================================================================
//			Copyright XashXT Group 2007 ©
//			image.c - loading textures
//=======================================================================

#include "launch.h"
#include "image.h"
#include "mathlib.h"

// global image variables
int image_width, image_height;
byte image_num_layers = 1;	// num layers in
byte image_num_mips = 0;	// build mipmaps
uint image_type;		// main type switcher
uint image_flags;		// additional image flags
byte image_bits_count;	// bits per RGBA
size_t image_size;		// image rgba size
uint image_ptr;

byte *image_palette;	// palette pointer
byte *image_rgba;		// image pointer (see image_type for details)
jpg_t jpg_file;		// jpeg read struct

// cubemap variables
int cubemap_width, cubemap_height;
int cubemap_num_sides;	// how mach sides is loaded 
byte *image_cubemap;	// cubemap pack
uint cubemap_image_type;	// shared image type
char *suf[6] = {"ft", "bk", "rt", "lf", "up", "dn"};

uint d_8toD1table[256];
uint d_8toQ1table[256];
uint d_8toQ2table[256];
uint d_8to24table[256];
uint *d_currentpal;
bool d1palette_init = false;
bool q1palette_init = false;
bool q2palette_init = false;

//=======================================================================
//			IMGLIB COMMON TOOLS
//=======================================================================

bool Image_ValidSize( char *name )
{
	if(image_width > 4096 || image_height > 4096 || image_width <= 0 || image_height <= 0)
	{
		MsgWarn( "Image_ValidSize: (%s) image size out of range [%dx%d]\n", name, image_width, image_height );
		return false;
	}
	return true;
}

void Image_GetPalette( byte *pal, uint *d_table )
{
	int	i;
	byte	rgba[4];

	// setup palette
	for (i = 0; i < 256; i++)
	{
		rgba[0] = 0xFF;
		rgba[3] = pal[i*3+0];
		rgba[2] = pal[i*3+1];
		rgba[1] = pal[i*3+2];
		d_table[i] = BuffBigLong( rgba );
	}
}

void Image_GetD1Palette( void )
{
	if(!d1palette_init)
	{
		Image_GetPalette( palette_d1, d_8toD1table );
		d_8toD1table[255] = 247; // 247 is transparent
		d1palette_init = true;
	}
	d_currentpal = d_8toD1table;
}

void Image_GetQ1Palette( void )
{
	if(!q1palette_init)
	{
		Image_GetPalette( palette_q1, d_8toQ1table );
		d_8toQ1table[255] = 0; // 255 is transparent
		q1palette_init = true;
	}
	d_currentpal = d_8toQ1table;
}

void Image_GetQ2Palette( void )
{
	if(!q2palette_init)
	{
		Image_GetPalette( palette_q2, d_8toQ2table );
		d_8toQ2table[255] &= LittleLong(0xffffff);
		q2palette_init = true;
	}
	d_currentpal = d_8toQ2table;
}

void Image_GetPalettePCX( byte *pal )
{
	if(pal)
	{
		Image_GetPalette( pal, d_8to24table );
		d_8to24table[255] &= LittleLong(0xffffff);
		d_currentpal = d_8to24table;
	}
	else Image_GetQ2Palette();          
}

/*
============
Image_Copy8bitRGBA

NOTE: must call Image_GetQ2Palette or Image_GetQ1Palette
before used
============
*/
bool Image_Copy8bitRGBA(const byte *in, byte *out, int pixels)
{
	int *iout = (int *)out;

	if(!d_currentpal)
	{
		MsgDev(D_ERROR,"Image_Copy8bitRGBA: no palette set\n");
		return false;
	}

	while (pixels >= 8)
	{
		iout[0] = d_currentpal[in[0]];
		iout[1] = d_currentpal[in[1]];
		iout[2] = d_currentpal[in[2]];
		iout[3] = d_currentpal[in[3]];
		iout[4] = d_currentpal[in[4]];
		iout[5] = d_currentpal[in[5]];
		iout[6] = d_currentpal[in[6]];
		iout[7] = d_currentpal[in[7]];
		in += 8;
		iout += 8;
		pixels -= 8;
	}
	if (pixels & 4)
	{
		iout[0] = d_currentpal[in[0]];
		iout[1] = d_currentpal[in[1]];
		iout[2] = d_currentpal[in[2]];
		iout[3] = d_currentpal[in[3]];
		in += 4;
		iout += 4;
	}
	if (pixels & 2)
	{
		iout[0] = d_currentpal[in[0]];
		iout[1] = d_currentpal[in[1]];
		in += 2;
		iout += 2;
	}
	if (pixels & 1)
		iout[0] = d_currentpal[in[0]];

	return true;
}


void Image_RoundDimensions(int *scaled_width, int *scaled_height)
{
	int width, height;

	for( width = 1; width < *scaled_width; width <<= 1 );
	for( height = 1; height < *scaled_height; height <<= 1 );

	*scaled_width = bound(1, width, 4096 );
	*scaled_height = bound(1, height, 4096 );
}

byte *Image_Resample(uint *in, int inwidth, int inheight, int outwidth, int outheight, int in_type )
{
	int		i, j;
	uint		frac, fracstep;
	uint		*inrow1, *inrow2;
	byte		*pix1, *pix2, *pix3, *pix4;
	uint		*out, *buf, p1[4096], p2[4096];

	// check for buffers
	if(!in) return NULL;

	// nothing to resample ?
	if (inwidth == outwidth && inheight == outheight)
		return (byte *)in;

	// can't resample compressed formats
	if(in_type != PF_RGBA_32) return NULL;

	// malloc new buffer
	out = buf = (uint *)Mem_Alloc( Sys.imagepool, outwidth * outheight * 4 );

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

	for (i = 0; i < outheight; i++,buf += outwidth)
	{
		inrow1 = in + inwidth * (int)((i + 0.25) * inheight / outheight);
		inrow2 = in + inwidth * (int)((i + 0.75) * inheight / outheight);
		frac = fracstep>>1;

		for (j = 0; j < outwidth; j++)
		{
			pix1 = (byte *)inrow1 + p1[j];
			pix2 = (byte *)inrow1 + p2[j];
			pix3 = (byte *)inrow2 + p1[j];
			pix4 = (byte *)inrow2 + p2[j];
			((byte *)(buf+j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			((byte *)(buf+j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			((byte *)(buf+j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			((byte *)(buf+j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}
	return (byte *)out;
}

bool Image_Processing( const char *name, rgbdata_t **pix )
{
	int		w, h;
	rgbdata_t		*image = *pix;
	byte		*out;
	char		width[4], height[4];

	//check for buffers
	if(!image || !image->buffer) return false;

	w = image->width;
	h = image->height;

	if(FS_GetParmFromCmdLine("-w", width ) && FS_GetParmFromCmdLine("-h", height ))
	{
		// custom size
		w = bound(4, atoi(width), 1024 );	// maxwidth 1024
		h = bound(4, atoi(height), 1024);	// maxheight 1024
	}
	else Image_RoundDimensions( &w, &h ); // auto detect new size

	out = Image_Resample((uint *)image->buffer, image->width, image->height, w, h, image->type );
	if(out != image->buffer)
	{
		MsgDev(D_INFO,"Resampling %s from[%d x %d] to[%d x %d]\n",name, image->width, image->height, w, h );
		Mem_Move( Sys.imagepool, &image->buffer, out, w * h * 4 );// update image->buffer
		image->width = w,image->height = h;
		*pix = image;
		return true;
	}
	return false;
}

/*
==============
LoadWAL
==============
*/
bool LoadWAL( char *name, char *buffer, int filesize )
{
	wal_t 	wal;
	int	pixels, ofs[4], mipsize, nummips = 1;// FIXME, adding four mips ?
	int	w, h, i, flags, value, contents; // wal additional parms

	if (filesize < (int)sizeof(wal))
	{
		MsgWarn("LoadWAL: file (%s) have invalid size\n", name );
		return false;
	}
	Mem_Copy(&wal, buffer, sizeof(wal));

	flags = LittleLong(wal.flags);
	value = LittleLong(wal.value);
	contents = LittleLong(wal.contents);
	w = image_width = LittleLong(wal.width);
	h = image_height = LittleLong(wal.height);
	for(i = 0; i < 4; i++) ofs[i] = LittleLong(wal.offsets[i]);
	if(!Image_ValidSize( name )) return false;

	pixels = w * h;
	mipsize = (int)sizeof(wal) + ofs[0] + pixels;
	if( pixels > 256 && filesize < mipsize )
	{
		// wal's width dimensions < 32 have invalid sizes.
		MsgWarn("LoadWAL: file (%s) have invalid size\n", name );
		return false;
	}

	image_num_layers = 1;
	image_type = PF_RGBA_32;

	Image_GetQ2Palette(); // hardcoded
	for(i = 0; i < nummips; i++ )
	{
		if(!FS_AddMipmapToPack( buffer + ofs[i], w, h ))
			break; // there were errors
		w = (w+1)>>1, h = (h+1)>>1; // calc size of next mip
	}
	return true;
}

/*
============
LoadLMP
============
*/
bool LoadLMP( char *name, char *buffer, int filesize )
{
	lmp_t	lmp;
	byte	*fin;
	int	i, pixels;

	if (filesize < (int)sizeof(lmp))
	{
		MsgWarn("LoadLMP: file (%s) have invalid size\n", name );
		return false;
	}

	fin = buffer;
	Mem_Copy(&lmp, fin, sizeof(lmp));
	fin += sizeof(lmp);

	image_width = LittleLong(lmp.width);
	image_height = LittleLong(lmp.height);
	image_num_mips = 1;

	if(!Image_ValidSize( name )) return false;         
          image_size = image_width * image_height * 4;
	pixels = image_width * image_height;
	
	if (filesize < (int)sizeof(lmp) + pixels)
	{
		MsgWarn("LoadLMP: file (%s) have invalid size\n", name );
		return false;
	}

	image_rgba = (byte *)Mem_Alloc(Sys.imagepool, image_size );
	image_num_layers = 1;
	image_type = PF_RGBA_32;

	// try to find transparent pixels
	for(i = 0; i < pixels; i++)
	{
		if(fin[i] != 255) continue;
		image_flags |= IMAGE_HAS_ALPHA;
		break; // found			
	}

	Image_GetQ1Palette();// hardcoded
	Image_Copy8bitRGBA( fin, image_rgba, pixels );

	return true;
}

/*
============
LoadMIP
============
*/
bool LoadMIP( char *name, char *buffer, int filesize )
{
	mip_t	mip;
	int	i, w, h;
	int	ofs[4], mipsize, nummips = 1;// FIXME, adding four mips ?

	if (filesize < (int)sizeof(mip))
	{
		MsgWarn("LoadMIP: file (%s) have invalid size\n", name );
		return false;
	}

	Mem_Copy(&mip, buffer, sizeof(mip));
	w = image_width = LittleLong(mip.width);
	h = image_height = LittleLong(mip.height);
	for(i = 0; i < nummips; i++) ofs[i] = LittleLong(mip.offsets[i]);

	if(!Image_ValidSize( name )) return false;         
	mipsize = image_width * image_height;
	
	if (filesize < (int)sizeof(mip) + mipsize)
	{
		MsgWarn("LoadMIP: file (%s) have invalid size\n", name );
		return false;
	}

	image_num_layers = 1;
	image_type = PF_RGBA_32;

	Image_GetQ1Palette();// hardcoded
	for(i = 0; i < nummips; i++ )
	{
		if(!FS_AddMipmapToPack( buffer + ofs[i], w, h ))
			break; // there were errors
		w = (w+1)>>1, h = (h+1)>>1; // calc size of next mip
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
	pcx_t	pcx;
	uint	*trans;
	int	s, i, p, x, y, x2, dataByte;
	byte	*pix, *pbuf, *palette, *fin, *enddata;

	fin = buffer;
	Mem_Copy(&pcx, fin, sizeof(pcx));
	fin += sizeof(pcx);

	// probably it's not pcx file
	if (pcx.manufacturer != 0x0a || pcx.version != 5 || pcx.encoding != 1 ) return false;
	if (filesize < (int)sizeof(pcx) + 768)
	{
		MsgWarn("LoadPCX: file (%s) have invalid size\n", name );
		return false;
	}

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

	if( pcx.bits_per_pixel != 8 || pcx.manufacturer != 0x0a || pcx.version != 5 || pcx.encoding != 1)
	{
		MsgWarn("LoadPCX: (%s) have illegal pixel size '%d'\n", name, pcx.bits_per_pixel );
		return false;
	}
	if(!Image_ValidSize( name )) return false;
	palette = buffer + filesize - 768;

	image_num_layers = 1;
	image_num_mips = 1;
	image_type = PF_RGBA_32;

	if(palette)
	{
		image_palette = Mem_Alloc( Sys.imagepool, 768 );
		Mem_Copy( image_palette, palette, 768 );
	}	

	s = image_width * image_height;
	image_size = image_width * image_height * 4;
	pbuf = (byte *)Mem_Alloc( Sys.imagepool, s );
	image_rgba = (byte *)Mem_Alloc( Sys.imagepool, image_size );
	trans = (uint *)image_rgba;
	enddata = palette;

	for (y = 0; y < image_height && fin < enddata; y++)
	{
		pix = pbuf + y * image_width;
		for (x = 0; x < image_width && fin < enddata;)
		{
			dataByte = *fin++;
			if(dataByte >= 0xC0)
			{
				if (fin >= enddata) break;
				x2 = x + (dataByte & 0x3F);
				dataByte = *fin++;
				if (x2 > image_width) x2 = image_width; // technically an error
				while(x < x2) pix[x++] = dataByte;
			}
			else pix[x++] = dataByte;
		}
		// the number of bytes per line is always forced to an even number
		fin += pcx.bytes_per_line - image_width;
		while(x < image_width) pix[x++] = 0;
	}

	Image_GetPalettePCX( palette );

	// convert to rgba
	for (i = 0; i < s; i++)
	{
		p = pbuf[i];
		if (p == 255)
		{
			image_flags |= IMAGE_HAS_ALPHA; // found alpha channel
			((byte *)&trans[i])[0] = ((byte *)&d_currentpal[0])[0];
			((byte *)&trans[i])[1] = ((byte *)&d_currentpal[0])[1];
			((byte *)&trans[i])[2] = ((byte *)&d_currentpal[0])[2];
			((byte *)&trans[i])[3] = ((byte *)&d_currentpal[p])[3];
		}
		else trans[i] = d_currentpal[p];
	}

	Mem_Free( pbuf ); // free compressed image
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

	targa_header.colormap_index = BuffLittleShort( fin ); fin += 2;
	targa_header.colormap_length = BuffLittleShort( fin ); fin += 2;
	targa_header.colormap_size = *fin++;
	targa_header.x_origin = BuffLittleShort( fin ); fin += 2;
	targa_header.y_origin = BuffLittleShort( fin ); fin += 2;
	targa_header.width = image_width = BuffLittleShort( fin ); fin += 2;
	targa_header.height = image_height = BuffLittleShort( fin );fin += 2;

	if(!Image_ValidSize( name )) return false;

	image_num_layers = 1;
	image_num_mips = 1;
	image_type = PF_RGBA_32; //always exctracted to 32-bit buffer

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
			MsgWarn("LoadTGA: (%s) have unsupported colormap type ( more than 256 bytes)\n", name );
			return false;
		}
		if (targa_header.colormap_index)
		{
			MsgWarn("LoadTGA: (%s) have unspported indexed colormap\n", name );
			return false;
		}
		image_palette = Mem_Alloc(Sys.imagepool, 256*4 );

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
			MsgWarn("LoadTGA: (%s) have unsupported colormap size (valid is 32 or 24 bit)\n", name );
			return false;
		}
		Mem_Copy(image_palette, palette, 256 * 4 ); //copy palette
	}

	// check our pixel_size restrictions according to image_type
	switch (targa_header.image_type & ~8)
	{
	case 2:
		if (targa_header.pixel_size != 24 && targa_header.pixel_size != 32)
		{
			MsgWarn("LoadTGA: (%s) have unsupported pixel size '%d', for type '%d'\n", name, targa_header.pixel_size, targa_header.image_type );
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
			MsgWarn("LoadTGA: (%s) have unsupported pixel size '%d', for type '%d'\n", name, targa_header.pixel_size, targa_header.image_type );
			return false;
		}
		break;
	default:
		MsgWarn("LoadTGA: (%s) is unsupported image type '%i'\n", name, targa_header.image_type);
		return false;
	}

	if (targa_header.attributes & 0x10)
	{
		MsgWarn("LoadTGA: (%s): top right and bottom right origin are not supported\n", name );
		return false;
	}

	// number of attribute bits per pixel, we only support 0 or 8
	alphabits = targa_header.attributes & 0x0F;
	if (alphabits != 8 && alphabits != 0)
	{
		MsgWarn("LoadTGA: (%s) have invalid attributes '%i'\n", name, alphabits );
		return false;
	}

	image_flags |= alphabits ? IMAGE_HAS_ALPHA : 0;
	image_size = image_width * image_height * 4;
	image_rgba = Mem_Alloc( Sys.imagepool, image_size );

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
	default:  break; // unknown image_type
	}
	return true;
}

/*
=============
LoadDDS
=============
*/
uint dds_get_linear_size( int width, int height, int depth, int rgbcount )
{
	uint i, BlockSize = 0;
	int block, bpp;

	// right calcualte blocksize
	for(i = 0; i < PF_TOTALCOUNT; i++)
	{
		if(image_type == PFDesc[i].format)
		{
			block = PFDesc[i].block;
			bpp = PFDesc[i].bpp;
			break;
		} 
	}

	if(i != PF_TOTALCOUNT) //make sure what match found
	{                  
		if(block == 0) BlockSize = width * height * bpp;
		else if(block > 0) BlockSize = ((width + 3)/4) * ((height + 3)/4) * depth * block;
		else if(block < 0 && rgbcount > 0) BlockSize = width * height * depth * rgbcount;
		else BlockSize = width * height * abs(block);
	}
	return BlockSize;
}

void dds_get_pixelformat( dds_t *hdr )
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
		case TYPE_DXT2: image_type = PF_DXT2; break;
		case TYPE_DXT3: image_type = PF_DXT3; break;
		case TYPE_DXT4: image_type = PF_DXT4; break;
		case TYPE_DXT5: image_type = PF_DXT5; break;
		case TYPE_ATI1: image_type = PF_ATI1N; break;
		case TYPE_ATI2: image_type = PF_ATI2N; break;
		case TYPE_RXGB: image_type = PF_RXGB; break;
		case TYPE_$: image_type = PF_ABGR_64; break;
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

	if(image_type == TYPE_DXT2 || image_type == TYPE_DXT4) 
		image_flags |= IMAGE_PREMULT;

	if(hdr->dwFlags & DDS_MIPMAPCOUNT)
		image_num_mips = hdr->dwMipMapCount;
	else image_num_mips = 1;

	if(image_type == PF_ARGB_32 || image_type == PF_LUMINANCE || image_type == PF_LUMINANCE_16 || image_type == PF_LUMINANCE_ALPHA)
	{
		//store RGBA mask into one block, and get palette pointer
		byte *tmp = image_palette = Mem_Alloc( Sys.imagepool, sizeof(uint) * 4 );
		Mem_Copy( tmp, &hdr->dsPixelFormat.dwRBitMask, sizeof(uint)); tmp += 4;
		Mem_Copy( tmp, &hdr->dsPixelFormat.dwGBitMask, sizeof(uint)); tmp += 4;
		Mem_Copy( tmp, &hdr->dsPixelFormat.dwBBitMask, sizeof(uint)); tmp += 4;
		Mem_Copy( tmp, &hdr->dsPixelFormat.dwABitMask, sizeof(uint)); tmp += 4;
	}
}

void dds_addjust_volume_texture( dds_t *hdr )
{
	uint bits;
	
	if (hdr->dwDepth <= 1) return;
	bits = hdr->dsPixelFormat.dwRGBBitCount / 8;
	hdr->dwFlags |= DDS_LINEARSIZE;
	hdr->dwLinearSize = dds_get_linear_size( hdr->dwWidth, hdr->dwHeight, hdr->dwDepth, bits );
}

uint dds_calc_mipmap_size( dds_t *hdr ) 
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
		mipsize = dds_get_linear_size( w, h, d, bits );
		w = (w+1)>>1, h = (h+1)>>1, d = (d+1)>>1;
	}
	return buffsize;
}

uint dds_calc_size( char *name, dds_t *hdr, uint filesize ) 
{
	uint buffsize = 0;
	int w = image_width;
	int h = image_height;
	int d = image_num_layers;
	int bits = hdr->dsPixelFormat.dwRGBBitCount / 8;

	if(hdr->dsCaps.dwCaps2 & DDS_CUBEMAP) 
	{
		// cubemap w*h always match for all sides
		buffsize = dds_calc_mipmap_size( hdr ) * 6;
	}
	else if(hdr->dwFlags & DDS_MIPMAPCOUNT)
	{
		// if mipcount > 1
		buffsize = dds_calc_mipmap_size( hdr );
	}
	else if(hdr->dwFlags & (DDS_LINEARSIZE | DDS_PITCH))
	{
		// just in case (no need, really)
		buffsize = hdr->dwLinearSize;
	}
	else 
	{
		// pretty solution for microsoft bug
		buffsize = dds_calc_mipmap_size( hdr );
	}

	if(filesize != buffsize) // main check
	{
		MsgWarn("LoadDDS: (%s) probably corrupted(%i should be %i)\n", name, buffsize, filesize );
		return false;
	}
	return buffsize;
}

bool LoadDDS( char *name, char *buffer, int filesize )
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

	// skip unused stuff
	for (i = 0; i < 10; i++) 
	{
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

	dds_get_pixelformat( &header );// and image type too :)
	dds_addjust_volume_texture( &header );

	if (image_type == PF_UNKNOWN) 
	{
		MsgWarn("LoadDDS: (%s) have unsupported compression type\n", name );
		return false; //unknown type
	}

	image_size = dds_calc_size( name, &header, filesize - 128 ); 
	if(image_size == 0) return false; // just in case

	// dds files will be uncompressed on a render. requires minimal of info for set this
	image_rgba = Mem_Alloc( Sys.imagepool, image_size ); 
	Mem_Copy( image_rgba, fin, image_size );

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
				memset(jpg_file.dc, 0, sizeof(int) * 3);
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
    
	static const int jpeg_zigzag[64] = 
	{
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
	static const float aanscale[8] =
	{
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

	if(!jpeg_readmarkers()) 
		return false; // it's not a jpg file, just skip it

	image_width = jpg_file.width;
	image_height = jpg_file.height;
	image_type = PF_RGBA_32;
	if(!Image_ValidSize( name )) return false;

	image_size = jpg_file.width * jpg_file.height * 4;
	jpg_file.data = Mem_Alloc( Sys.imagepool, image_size );

	jpeg_decompress();
	if(jpg_file.num_components == 1) jpeg_gray2rgba();        
	if(jpg_file.num_components == 3) jpeg_ycbcr2rgba();

	image_num_layers = 1;
	image_num_mips = 1;
	image_rgba = jpg_file.data;

	return true;
}

typedef struct loadformat_s
{
	char *formatstring;
	char *ext;
	bool (*loadfunc)(char *name, char *buffer, int filesize);
} loadformat_t;

loadformat_t load_formats[] =
{
	{"textures/%s%s.%s", "dds", LoadDDS},
	{"textures/%s%s.%s", "tga", LoadTGA},
	{"textures/%s%s.%s", "jpg", LoadJPG},
	{"textures/%s%s.%s", "pcx", LoadPCX},
	{"textures/%s%s.%s", "wal", LoadWAL},
	{"textures/%s%s.%s", "lmp", LoadLMP},
	{"textures/%s%s.%s", "mip", LoadMIP},
	{"%s%s.%s", "dds", LoadDDS},
	{"%s%s.%s", "tga", LoadTGA},
	{"%s%s.%s", "jpg", LoadJPG},
	{"%s%s.%s", "pcx", LoadPCX},
	{"%s%s.%s", "wal", LoadWAL},
	{"%s%s.%s", "lmp", LoadLMP},
	{"%s%s.%s", "mip", LoadMIP},
	{NULL, NULL}
};

rgbdata_t *ImagePack( void )
{
	rgbdata_t *pack = Mem_Alloc( Sys.imagepool, sizeof(rgbdata_t));

	if(image_cubemap && cubemap_num_sides != 6)
	{
		// this neved be happens, just in case
		MsgWarn("ImagePack: inconsistent cubemap pack %d\n", cubemap_num_sides );
		FS_FreeImage( pack );
		return NULL;
	}

	if(image_cubemap) 
	{
		image_flags |= IMAGE_CUBEMAP;
		pack->buffer = image_cubemap;
		pack->width = cubemap_width;
		pack->height = cubemap_height;
		pack->type = cubemap_image_type;
		pack->size = image_size * cubemap_num_sides;
	}
	else 
	{
		pack->buffer = image_rgba;
		pack->width = image_width;
		pack->height = image_height;
		pack->type = image_type;
		pack->size = image_size;
	}

	pack->numLayers = image_num_layers;
	pack->numMips = image_num_mips;
	pack->bitsCount = image_bits_count;
	pack->flags = image_flags;
	pack->palette = image_palette;
	return pack;
}

bool FS_AddImageToPack( const char *name )
{
	byte	*resampled;
	
	// first image have suffix "ft" and set average size for all cubemap sides!
	if(!image_cubemap)
	{
		cubemap_width = image_width;
		cubemap_height = image_height;
		cubemap_image_type = image_type;
	}
	image_size = cubemap_width * cubemap_height * 4; // keep constant size, render.dll expecting it
          
	// mixing dds format with any existing ?
	if(image_type != cubemap_image_type) return false;

	// resampling image if needed
	resampled = Image_Resample((uint *)image_rgba, image_width, image_height, cubemap_width, cubemap_height, cubemap_image_type );
	if(!resampled) return false; // try to reasmple dxt?

	if(resampled != image_rgba) 
	{
		MsgDev(D_NOTE, "FS_AddImageToPack: resample %s from [%dx%d] to [%dx%d]\n", name, image_width, image_height, cubemap_width, cubemap_height );  
		Mem_Move( Sys.imagepool, &image_rgba, resampled, image_size );// update buffer
	}	

	image_cubemap = Mem_Realloc( Sys.imagepool, image_cubemap, image_ptr + image_size );
	Mem_Copy(image_cubemap + image_ptr, image_rgba, image_size );

	Mem_Free( image_rgba );	// memmove aren't help us
	image_ptr += image_size; 	// move to next
	cubemap_num_sides++;	// sides counter

	return true;
}

bool FS_AddMipmapToPack( const byte *in, int width, int height )
{
	int mipsize = width * height;
	int outsize = width * height * 4;

	// reallocate image buffer
	image_rgba = Mem_Realloc( Sys.imagepool, image_rgba, image_size + outsize );	
	if(!Image_Copy8bitRGBA( in, image_rgba + image_size, mipsize ))
		return false; // pallette not installed
	image_size += outsize;
	image_num_mips++;

	return true;
}


/*
================
FS_LoadImage

loading and unpack to rgba any known image
================
*/
rgbdata_t *FS_LoadImage(const char *filename, char *buffer, int buffsize )
{
	loadformat_t	*format;
          const char	*ext = FS_FileExtension( filename );
	char		path[128], loadname[128], texname[128];
	bool		anyformat = !stricmp(ext, "") ? true : false;
	int		i, filesize = 0;
	byte		*f;

#if 0     // don't try to be very clever
	if(!buffer || !buffsize) buffer = (char *)florr1_2_jpg, buffsize = sizeof(florr1_2_jpg);
#endif
	com_strncpy( loadname, filename, sizeof(loadname)-1);
	FS_StripExtension( loadname ); //remove extension if needed

	// developer warning
	if(!anyformat) MsgDev(D_NOTE, "Note: %s will be loading only with ext .%s\n", loadname, ext );
	
	// now try all the formats in the selected list
	for (format = load_formats; format->formatstring; format++)
	{
		if(anyformat || !stricmp(ext, format->ext ))
		{
			com_sprintf (path, format->formatstring, loadname, "", format->ext );
			f = FS_LoadFile( path, &filesize );
			if(f && filesize > 0)
			{
				// this name will be used only for tell user about problems 
				FS_FileBase( path, texname );
				if( format->loadfunc(texname, f, filesize ))
					return ImagePack(); //loaded
			}
		}
	}

	// maybe it skybox or cubemap ?
	for(i = 0; i < 6; i++)
	{
		for (format = load_formats; format->formatstring; format++)
		{
			if(anyformat || !stricmp(ext, format->ext ))
			{
				com_sprintf (path, format->formatstring, loadname, suf[i], format->ext );
				f = FS_LoadFile( path, &filesize );
				if(f && filesize > 0)
				{
					// this name will be used only for tell user about problems 
					FS_FileBase( path, texname );
					if( format->loadfunc(texname, f, filesize ))
					{
						if(FS_AddImageToPack(va("%s%s.%s", loadname, suf[i], format->ext)))
							break; // loaded
					}
				}
			}
		}
		if(cubemap_num_sides != i + 1) //check side
		{
			// first side not found, probably it's not cubemap
			// it contain info about image_type and dimensions, don't generate black cubemaps 
			if(!image_cubemap) break;
			MsgDev(D_ERROR, "FS_LoadImage: couldn't load (%s%s.%s), create balck image\n",loadname,suf[i],ext );

			// Mem_Alloc already filled memblock with 0x00, no need to do it again
			image_cubemap = Mem_Realloc( Sys.imagepool, image_cubemap, image_ptr + image_size );
			image_ptr += image_size; // move to next
			cubemap_num_sides++; // merge counter
		}
	}

	if(image_cubemap) return ImagePack(); // now it's cubemap pack 

	// try to load image from const buffer (e.g. const byte blank_frame )
	com_strncpy( texname, filename, sizeof(texname) - 1);

	for (format = load_formats; format->formatstring; format++)
	{
		if(anyformat || !stricmp(ext, format->ext ))
		{
			if(buffer && buffsize > 0)
			{
				// this name will be used only for tell user about problems 
				FS_FileBase( loadname, texname );
				if( format->loadfunc(texname, buffer, buffsize ))
					return ImagePack();// loaded
			}
		}
	}

	MsgDev(D_WARN, "couldn't load %s\n", texname );
	return NULL;
}

/*
================
FS_FreeImage

free RGBA buffer
================
*/
void FS_FreeImage( rgbdata_t *pack )
{
	if( pack )
	{
		if( pack->buffer ) Mem_Free( pack->buffer );
		if( pack->palette ) Mem_Free( pack->palette );
		Mem_Free( pack );
	}

	// reset global variables
	image_width = image_height = 0;
	cubemap_width = cubemap_height = 0;
	image_bits_count = image_flags = 0;
	cubemap_num_sides = 0;
	image_num_layers = 1;
	image_num_mips = 0;
	image_type = PF_UNKNOWN;
	image_palette = NULL;
	image_rgba = NULL;
	image_cubemap = NULL;
	image_ptr = 0;
	image_size = 0;
}

/*
=============
SaveTGA
=============
*/
bool SaveTGA( const char *filename, byte *data, int width, int height, bool alpha, int imagetype )
{
	int		y, outsize, pixel_size;
	const byte	*bufend, *in;
	byte		*buffer, *out;
	const char	*comment = "Generated by Xash ImageLib\0";

	if(alpha) outsize = width * height * 4 + 18 + com_strlen(comment);
	else outsize = width * height * 3 + 18 + com_strlen(comment);

	buffer = (byte *)Malloc( outsize );
	memset (buffer, 0, 18);

	// prepare header
	buffer[0] = com_strlen(comment); // tga comment length
	buffer[2] = 2; // uncompressed type
	buffer[12] = (width >> 0) & 0xFF;
	buffer[13] = (width >> 8) & 0xFF;
	buffer[14] = (height >> 0) & 0xFF;
	buffer[15] = (height >> 8) & 0xFF;
	buffer[16] = alpha ? 32 : 24;
	buffer[17] = alpha ? 8 : 0; // 8 bits of alpha
	com_strncpy(buffer + 18, comment, com_strlen(comment)); 
	out = buffer + 18 + com_strlen(comment);

	// get image description
	switch( imagetype )
	{
	case PF_RGB_24_FLIP:
	case PF_RGB_24: pixel_size = 3; break;
	case PF_RGBA_32: pixel_size = 4; break;	
	default:
		MsgWarn("SaveTGA: unsupported image type %s\n", PFDesc[image_type].name );
		return false;
	}

	// flip buffer
	switch( imagetype )
	{
	case PF_RGB_24_FLIP:
		// glReadPixels rotating image at 180 degrees, flip it
		for (in = data; in < data + width * height * pixel_size; in += pixel_size)
		{
			*out++ = in[2];
			*out++ = in[1];
			*out++ = in[0];
		}	
		break;
	case PF_RGB_24:
	case PF_RGBA_32:
		// swap rgba to bgra and flip upside down
		for (y = height - 1; y >= 0; y--)
		{
			in = data + y * width * pixel_size;
			bufend = in + width * pixel_size;
			for ( ;in < bufend; in += pixel_size)
			{
				*out++ = in[2];
				*out++ = in[1];
				*out++ = in[0];
				if(alpha) *out++ = in[3];
			}
		}
	}	

	MsgDev(D_NOTE, "Writing %s[%d]\n", filename, alpha ? 32 : 24 );
	FS_WriteFile (filename, buffer, outsize );

	Mem_Free( buffer );
	return true;
} 

typedef struct saveformat_s
{
	char *formatstring;
	char *ext;
	bool (*savefunc)(char *filename, byte *data, int width, int height, bool alpha, int imagetype );
} saveformat_t;

saveformat_t save_formats[] =
{
	{"%s%s.%s", "tga", SaveTGA},
	{NULL, NULL}
};

/*
================
FS_SaveImage

writes image as tga RGBA format
================
*/
void FS_SaveImage(const char *filename, rgbdata_t *pix )
{
	bool		has_alpha = false;
	int		i, numsides = 1;
	byte		*data;
	char		savename[256];

	if(!pix || !pix->buffer) return;

	data = pix->buffer;
	FS_StripExtension( (char *)filename );
	if(pix->flags & IMAGE_HAS_ALPHA) has_alpha = true;
	if(pix->flags & IMAGE_CUBEMAP) numsides = 6;

	for(i = 0; i < numsides; i++)
	{
		if(numsides > 1) com_sprintf(savename, "%s%s.tga", filename, suf[i] );
		else com_sprintf(savename, "%s.tga", filename );

		SaveTGA( savename, data, pix->width, pix->height, has_alpha, pix->type );
		data += pix->width * pix->height * PFDesc[pix->type].bpp;
	}
}