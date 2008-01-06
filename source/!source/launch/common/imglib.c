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

// cubemap variables
int cubemap_width, cubemap_height;
int cubemap_num_sides;	// how mach sides is loaded 
byte *image_cubemap;	// cubemap pack
uint cubemap_image_type;	// shared image type
char *suf[6] = {"ft", "bk", "rt", "lf", "up", "dn"};

//=======================================================================
//			IMGLIB COMMON TOOLS
//=======================================================================
void Image_Init( void )
{
	Sys.imagepool = Mem_AllocPool( "ImageLib Pool" );
}

void Image_Shutdown( void )
{
	Mem_FreePool( &Sys.imagepool );
}

bool Image_ValidSize( char *name )
{
	if(image_width > 4096 || image_height > 4096 || image_width <= 0 || image_height <= 0)
	{
		MsgWarn( "Image_ValidSize: (%s) dimensions out of range [%dx%d]\n", name, image_width, image_height );
		return false;
	}
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

bool Image_Processing( const char *name, rgbdata_t **pix, int width, int height )
{
	int		w, h;
	rgbdata_t		*image = *pix;
	byte		*out;

	// check for buffers
	if(!image || !image->buffer) return false;

	w = image->width;
	h = image->height;

	if(width && height)
	{
		// custom size
		w = bound(4, width, 1024 );	// maxwidth 1024
		h = bound(4, height, 1024);	// maxheight 1024
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
	{"%s%s.%s", "dds", LoadDDS},
	{"%s%s.%s", "tga", LoadTGA},
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
		if(anyformat || !com_stricmp(ext, format->ext ))
		{
			com_sprintf (path, format->formatstring, loadname, "", format->ext );
			f = FS_LoadFile( path, &filesize );
			if(f && filesize > 0)
			{
				// this name will be used only for tell user about problems 
				FS_FileBase( path, texname );
				if( format->loadfunc(texname, f, filesize ))
				{
					Mem_Free(f); // release buffer
					return ImagePack(); // loaded
				}
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
					Mem_Free(f);
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
		if(anyformat || !com_stricmp(ext, format->ext ))
		{
			if(buffer && buffsize > 0)
			{
				// this name will be used only for tell user about problems 
				FS_FileBase( loadname, texname );
				if( format->loadfunc(texname, buffer, buffsize ))
					return ImagePack(); // loaded
			}
		}
	}

	MsgDev(D_WARN, "FS_LoadImage: couldn't load \"%s\"\n", texname );
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
bool SaveTGA( const char *filename, rgbdata_t *pix )
{
	int		y, outsize, pixel_size;
	const byte	*bufend, *in;
	byte		*buffer, *out;
	const char	*comment = "Generated by Xash ImageLib\0";

	if(FS_FileExists(filename)) return false; // already existed
	if( pix->flags & IMAGE_HAS_ALPHA ) outsize = pix->width * pix->height * 4 + 18 + com_strlen(comment);
	else outsize = pix->width * pix->height * 3 + 18 + com_strlen(comment);

	buffer = (byte *)Malloc( outsize );
	memset( buffer, 0, 18 );

	// prepare header
	buffer[0] = com_strlen(comment); // tga comment length
	buffer[2] = 2; // uncompressed type
	buffer[12] = (pix->width >> 0) & 0xFF;
	buffer[13] = (pix->width >> 8) & 0xFF;
	buffer[14] = (pix->height >> 0) & 0xFF;
	buffer[15] = (pix->height >> 8) & 0xFF;
	buffer[16] = ( pix->flags & IMAGE_HAS_ALPHA ) ? 32 : 24;
	buffer[17] = ( pix->flags & IMAGE_HAS_ALPHA ) ? 8 : 0; // 8 bits of alpha
	com_strncpy( buffer + 18, comment, com_strlen(comment)); 
	out = buffer + 18 + com_strlen(comment);

	// get image description
	switch( pix->type )
	{
	case PF_RGB_24_FLIP:
	case PF_RGB_24: pixel_size = 3; break;
	case PF_RGBA_32: pixel_size = 4; break;	
	default:
		MsgWarn("SaveTGA: unsupported image type %s\n", PFDesc[pix->type].name );
		return false;
	}

	// flip buffer
	switch( pix->type )
	{
	case PF_RGB_24_FLIP:
		// glReadPixels rotating image at 180 degrees, flip it
		for (in = pix->buffer; in < pix->buffer + pix->width * pix->height * pixel_size; in += pixel_size)
		{
			*out++ = in[2];
			*out++ = in[1];
			*out++ = in[0];
		}	
		break;
	case PF_RGB_24:
	case PF_RGBA_32:
		// swap rgba to bgra and flip upside down
		for (y = pix->height - 1; y >= 0; y--)
		{
			in = pix->buffer + y * pix->width * pixel_size;
			bufend = in + pix->width * pixel_size;
			for ( ;in < bufend; in += pixel_size)
			{
				*out++ = in[2];
				*out++ = in[1];
				*out++ = in[0];
				if( pix->flags & IMAGE_HAS_ALPHA )
					*out++ = in[3];
			}
		}
	}	

	MsgDev(D_NOTE, "Writing %s[%d]\n", filename, (pix->flags & IMAGE_HAS_ALPHA) ? 32 : 24 );
	FS_WriteFile( filename, buffer, outsize );

	Mem_Free( buffer );
	return true;
} 

/*
=============
SaveDDS
=============
*/
bool SaveDDS( const char *filename, rgbdata_t *pix )
{
	return false; //FIXME
}

typedef struct saveformat_s
{
	char *formatstring;
	char *ext;
	bool (*savefunc)( char *filename, rgbdata_t *pix );
} saveformat_t;

saveformat_t save_formats[] =
{
	{"%s%s.%s", "tga", SaveTGA},
	{"%s%s.%s", "dds", SaveDDS},
	{NULL, NULL}
};

/*
================
FS_SaveImage

writes image as tga RGBA format
================
*/
void FS_SaveImage( const char *filename, rgbdata_t *pix )
{
	saveformat_t	*format;
          const char	*ext = FS_FileExtension( filename );
	char		path[128], savename[128];
	bool		anyformat = !stricmp(ext, "") ? true : false;
	int		filesize = 0;
	byte		*data;
	bool		has_alpha = false;

	if(!pix || !pix->buffer) return;
	if(pix->flags & IMAGE_HAS_ALPHA) has_alpha = true;
	data = pix->buffer;

	com_strncpy( savename, filename, sizeof(savename) - 1);
	FS_StripExtension( savename ); // remove extension if needed

	// developer warning
	if(!anyformat) MsgDev(D_NOTE, "Note: %s will be saving only with ext .%s\n", savename, ext );

	// now try all the formats in the selected list
	for (format = save_formats; format->formatstring; format++)
	{
		if( anyformat || !com_stricmp( ext, format->ext ))
		{
			com_sprintf( path, format->formatstring, savename, "", format->ext );
			if( format->savefunc( path, pix ))
				return; // saved
		}
	}

	/*for(i = 0; i < numsides; i++)
	{
		if(numsides > 1) com_sprintf(savename, "%s%s.tga", filename, suf[i] );
		else com_sprintf(savename, "%s.tga", filename );

		SaveTGA( savename, data, pix->width, pix->height, has_alpha, pix->type, pix->palette );
		data += pix->width * pix->height * PFDesc[pix->type].bpp;
	}*/
}