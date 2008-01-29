//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_tga.c - tga format load & save
//=======================================================================

#include "imagelib.h"
#include "img_formats.h"

/*
=============
Image_LoadTGA
=============
*/
bool Image_LoadTGA( const char *name, byte *buffer, size_t filesize )
{
	int	x, y, pix_inc, row_inc;
	int	red, green, blue, alpha;
	int	runlen, alphabits;
	byte	*pixbuf;
	const byte *fin, *enddata;
	tga_t	targa_header;

	if(filesize < sizeof(tga_t))
		return false;

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
	image_type = PF_RGBA_32; // always exctracted to 32-bit buffer

	targa_header.pixel_size = *fin++;
	targa_header.attributes = *fin++;
	// end of header
 
	// skip TARGA image comment ( usually 0 bytes )
	fin += targa_header.id_length;

	// read/skip the colormap if present (note: according to the TARGA spec it
	// can be present even on truecolor or greyscale images, just not used by
	// the image data)
	if( targa_header.colormap_type )
	{
		MsgDev(D_ERROR, "LoadTGA: (%s) colormap images unsupported (valid is 32 or 24 bit)\n", name );
		return false;
	}

	// check our pixel_size restrictions according to image_type
	switch (targa_header.image_type & ~8)
	{
	case 2:
		if( targa_header.pixel_size != 24 && targa_header.pixel_size != 32 )
		{
			MsgDev(D_ERROR, "LoadTGA: (%s) have unsupported pixel size '%d', for type '%d'\n", name, targa_header.pixel_size, targa_header.image_type );
			return false;
		}
		break;
	default:
		MsgDev(D_ERROR, "LoadTGA: (%s) is unsupported image type '%i'\n", name, targa_header.image_type );
		return false;
	}

	if( targa_header.attributes & 0x10 )
	{
		MsgWarn("LoadTGA: (%s): top right and bottom right origin are not supported\n", name );
		return false;
	}

	// number of attribute bits per pixel, we only support 0 or 8
	alphabits = targa_header.attributes & 0x0F;
	if( alphabits != 8 && alphabits != 0 )
	{
		MsgWarn("LoadTGA: (%s) have invalid attributes '%i'\n", name, alphabits );
		return false;
	}

	image_flags |= alphabits ? IMAGE_HAS_ALPHA : 0;
	image_size = image_width * image_height * 4;
	image_rgba = Mem_Alloc( zonepool, image_size );

	// If bit 5 of attributes isn't set, the image has been stored from bottom to top
	if(!(targa_header.attributes & 0x20))
	{
		pixbuf = image_rgba + (image_height - 1) * image_width * 4;
		row_inc = -image_width * 4 * 2;
	}
	else
	{
		pixbuf = image_rgba;
		row_inc = 0;
	}

	x = y = 0;
	red = green = blue = alpha = 255;
	pix_inc = 1;
	if((targa_header.image_type & ~8) == 2) pix_inc = targa_header.pixel_size / 8;

	switch( targa_header.image_type )
	{
	case 2:
		// BGR or BGRA, uncompressed
		if(fin + image_width * image_height * pix_inc > enddata)
			break;
		if(targa_header.pixel_size == 32 && alphabits)
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
		else // 24 bits
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
	case 10:
		// BGR or BGRA, RLE
		if( targa_header.pixel_size == 32 && alphabits )
		{
			for( y = 0; y < image_height; y++, pixbuf += row_inc )
			{
				for (x = 0; x < image_width; )
				{                           
					if( fin >= enddata ) break; // error - truncated file
					runlen = *fin++;
					if( runlen & 0x80 )
					{
						// RLE - all pixels the same color
						runlen += 1 - 0x80;
						if( fin + pix_inc > enddata ) break; // error - truncated file
						if( x + runlen > image_width) break; // error - line exceeds width
						red = fin[2];
						green = fin[1];
						blue = fin[0];
						alpha = fin[3];
						fin += pix_inc;
						for( ; runlen--; x++)
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
						if( fin + pix_inc * runlen > enddata ) break; // error - truncated file
						if( x + runlen > image_width ) break; // error - line exceeds width
						for( ;runlen--; x++, fin += pix_inc )
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
			for( y = 0; y < image_height; y++, pixbuf += row_inc )
			{
				for (x = 0; x < image_width; )
				{
					if( fin >= enddata ) break; // error - truncated file
					runlen = *fin++;
					if( runlen & 0x80 )
					{
						// RLE - all pixels the same color
						runlen += 1 - 0x80;
						if( fin + pix_inc > enddata ) break; // error - truncated file
						if( x + runlen > image_width )break; // error - line exceeds width
						red = fin[2];
						green = fin[1];
						blue = fin[0];
						alpha = 255;
						fin += pix_inc;
						for( ;runlen--; x++ )
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
						if( fin + pix_inc * runlen > enddata ) break; // error - truncated file
						if( x + runlen > image_width ) break; // error - line exceeds width
						for ( ; runlen--; x++, fin += pix_inc)
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
	// unknown image_type
	default:  return false;
	}

	return true;
}

/*
=============
SaveTGA
=============
*/
bool Image_SaveTGA( const char *name, rgbdata_t *pix, int saveformat )
{
	int		y, outsize, pixel_size;
	const byte	*bufend, *in;
	byte		*buffer, *out;
	const char	*comment = "Generated by Xash ImageLib\0";

	if(FS_FileExists(name)) return false; // already existed
	if( pix->flags & IMAGE_HAS_ALPHA ) outsize = pix->width * pix->height * 4 + 18 + com.strlen(comment);
	else outsize = pix->width * pix->height * 3 + 18 + com.strlen(comment);

	buffer = (byte *)Mem_Alloc( zonepool, outsize );
	memset( buffer, 0, 18 );

	// prepare header
	buffer[0] = com.strlen(comment); // tga comment length
	buffer[2] = 2; // uncompressed type
	buffer[12] = (pix->width >> 0) & 0xFF;
	buffer[13] = (pix->width >> 8) & 0xFF;
	buffer[14] = (pix->height >> 0) & 0xFF;
	buffer[15] = (pix->height >> 8) & 0xFF;
	buffer[16] = ( pix->flags & IMAGE_HAS_ALPHA ) ? 32 : 24;
	buffer[17] = ( pix->flags & IMAGE_HAS_ALPHA ) ? 8 : 0; // 8 bits of alpha
	com.strncpy( buffer + 18, comment, com.strlen(comment)); 
	out = buffer + 18 + com.strlen(comment);

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

	MsgDev(D_NOTE, "Writing %s[%d]\n", name, (pix->flags & IMAGE_HAS_ALPHA) ? 32 : 24 );
	FS_WriteFile( name, buffer, outsize );

	Mem_Free( buffer );
	return true;
}