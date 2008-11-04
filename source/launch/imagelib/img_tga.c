//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_tga.c - tga format load & save
//=======================================================================

#include "imagelib.h"

/*
=============
Image_LoadTGA
=============
*/
bool Image_LoadTGA( const char *name, const byte *buffer, size_t filesize )
{
	int	x, y, pix_inc, row_inc;
	int	red, green, blue, alpha;
	int	runlen, alphabits;
	byte	*p, *pixbuf;
	const byte *fin, *enddata;
	byte	palette[256*4];
	tga_t	targa_header;

	if( filesize < sizeof( tga_t ))
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
	targa_header.width = image.width = BuffLittleShort( fin ); fin += 2;
	targa_header.height = image.height = BuffLittleShort( fin );fin += 2;

	// check for tga file
	if(!Image_ValidSize( name )) return false;

	image.num_layers = 1;
	image.num_mips = 1;
	image.type = PF_RGBA_32; // always exctracted to 32-bit buffer

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
		if( targa_header.colormap_length > 256 )
		{
			MsgDev( D_ERROR, "Image_LoadTGA: only up to 256 colormap_length supported\n" );
			return false;
		}
		if( targa_header.colormap_index )
		{
			MsgDev( D_ERROR, "Image_LoadTGA: colormap_index not supported\n" );
			return false;
		}
		if( targa_header.colormap_size == 24 )
		{
			for( x = 0; x < targa_header.colormap_length; x++ )
			{
				palette[x*4+2] = *fin++;
				palette[x*4+1] = *fin++;
				palette[x*4+0] = *fin++;
				palette[x*4+3] = 255;
			}
		}
		else if( targa_header.colormap_size == 32 )
		{
			for( x = 0; x < targa_header.colormap_length; x++ )
			{
				palette[x*4+2] = *fin++;
				palette[x*4+1] = *fin++;
				palette[x*4+0] = *fin++;
				palette[x*4+3] = *fin++;
			}
		}
		else
		{
			Msg("Image_LoadTGA: Only 32 and 24 bit colormap_size supported\n");
			return false;
		}
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
	case 3:
		// set up a palette to make the loader easier
		for( x = 0; x < 256; x++ )
		{
			palette[x*4+2] = x;
			palette[x*4+1] = x;
			palette[x*4+0] = x;
			palette[x*4+3] = 255;
		}
		// fall through to colormap case
	case 1:
		if( targa_header.pixel_size != 8 )
		{
			MsgDev( D_ERROR, "Image_LoadTGA: only 8bit pixel size for type 1, 3, 9, and 11 images supported\n" );
			return false;
		}
		break;
	default:
		MsgDev(D_ERROR, "Image_LoadTGA: (%s) is unsupported image type '%i'\n", name, targa_header.image_type );
		return false;
	}

	if( targa_header.attributes & 0x10 )
	{
		MsgDev( D_WARN, "Image_LoadTGA: (%s): top right and bottom right origin are not supported\n", name );
		return false;
	}

	// number of attribute bits per pixel, we only support 0 or 8
	alphabits = targa_header.attributes & 0x0F;
	if( alphabits != 8 && alphabits != 0 )
	{
		MsgDev( D_WARN, "LoadTGA: (%s) have invalid attributes '%i'\n", name, alphabits );
		return false;
	}

	image.flags |= alphabits ? IMAGE_HAS_ALPHA : 0;
	image.size = image.width * image.height * 4;
	image.rgba = Mem_Alloc( Sys.imagepool, image.size );

	// If bit 5 of attributes isn't set, the image has been stored from bottom to top
	if(!(targa_header.attributes & 0x20))
	{
		pixbuf = image.rgba + (image.height - 1) * image.width * 4;
		row_inc = -image.width * 4 * 2;
	}
	else
	{
		pixbuf = image.rgba;
		row_inc = 0;
	}

	x = y = 0;
	red = green = blue = alpha = 255;
	pix_inc = 1;
	if((targa_header.image_type & ~8) == 2) pix_inc = targa_header.pixel_size / 8;

	switch( targa_header.image_type )
	{
	case 1: // colormapped, uncompressed
	case 3: // greyscale, uncompressed
		if( fin + image.width * image.height * pix_inc > enddata )
			break;
		for( y = 0; y < image.height; y++, pixbuf += row_inc )
		{
			for( x = 0; x < image.width; x++ )
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
		if( fin + image.width * image.height * pix_inc > enddata )
			break;
		if( targa_header.pixel_size == 32 && alphabits )
		{
			for( y = 0;y < image.height;y++, pixbuf += row_inc )
			{
				for( x = 0;x < image.width;x++, fin += pix_inc )
				{
					*pixbuf++ = fin[2];
					*pixbuf++ = fin[1];
					*pixbuf++ = fin[0];
					*pixbuf++ = fin[3];

					if( fin[2] != fin[1] || fin[1] != fin[0] )
						image.flags |= IMAGE_HAS_COLOR;
				}
			}
		}
		else // 24 bits
		{
			for( y = 0; y < image.height; y++, pixbuf += row_inc )
			{
				for( x = 0;x < image.width; x++, fin += pix_inc )
				{
					*pixbuf++ = fin[2];
					*pixbuf++ = fin[1];
					*pixbuf++ = fin[0];
					*pixbuf++ = 255;

					if( fin[2] != fin[1] || fin[1] != fin[0] )
						image.flags |= IMAGE_HAS_COLOR;
				}
			}
		}
		break;
	case 9: // colormapped, RLE
	case 11: // greyscale, RLE
		for( y = 0; y < image.height; y++, pixbuf += row_inc )
		{
			for( x = 0; x < image.width; )
			{
				if( fin >= enddata ) break; // error - truncated file
				runlen = *fin++;
				if( runlen & 0x80 )
				{
					// RLE - all pixels the same color
					runlen += 1 - 0x80;
					if( fin + pix_inc > enddata )
						break; // error - truncated file
					if( x + runlen > image.width )
						break; // error - line exceeds width
					p = palette + *fin++ * 4;
					red = p[0];
					green = p[1];
					blue = p[2];
					alpha = p[3];
					for( ; runlen--; x++ )
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
					if( fin + pix_inc * runlen > enddata )
						break; // error - truncated file
					if( x + runlen > image.width )
						break; // error - line exceeds width
					for( ; runlen--; x++ )
					{
						p = palette + *fin++ * 4;
						*pixbuf++ = p[0];
						*pixbuf++ = p[1];
						*pixbuf++ = p[2];
						*pixbuf++ = p[3];

						if( p[0] != p[1] || p[1] != p[2] )
							image.flags |= IMAGE_HAS_COLOR;
					}
				}
			}
		}
		break;
	case 10:
		// BGR or BGRA, RLE
		if( targa_header.pixel_size == 32 && alphabits )
		{
			for( y = 0; y < image.height; y++, pixbuf += row_inc )
			{
				for (x = 0; x < image.width; )
				{                           
					if( fin >= enddata ) break; // error - truncated file
					runlen = *fin++;
					if( runlen & 0x80 )
					{
						// RLE - all pixels the same color
						runlen += 1 - 0x80;
						if( fin + pix_inc > enddata ) break; // error - truncated file
						if( x + runlen > image.width) break; // error - line exceeds width
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
						if( x + runlen > image.width ) break; // error - line exceeds width
						for( ;runlen--; x++, fin += pix_inc )
						{
							*pixbuf++ = fin[2];
							*pixbuf++ = fin[1];
							*pixbuf++ = fin[0];
							*pixbuf++ = fin[3];

							if( fin[2] != fin[1] || fin[1] != fin[0] )
								image.flags |= IMAGE_HAS_COLOR;
						}
					}
				}
			}
		}
		else
		{
			for( y = 0; y < image.height; y++, pixbuf += row_inc )
			{
				for (x = 0; x < image.width; )
				{
					if( fin >= enddata ) break; // error - truncated file
					runlen = *fin++;
					if( runlen & 0x80 )
					{
						// RLE - all pixels the same color
						runlen += 1 - 0x80;
						if( fin + pix_inc > enddata ) break; // error - truncated file
						if( x + runlen > image.width )break; // error - line exceeds width
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
						if( x + runlen > image.width ) break; // error - line exceeds width
						for ( ; runlen--; x++, fin += pix_inc)
						{
							*pixbuf++ = fin[2];
							*pixbuf++ = fin[1];
							*pixbuf++ = fin[0];
							*pixbuf++ = 255;

							if( fin[2] != fin[1] || fin[1] != fin[0] )
								image.flags |= IMAGE_HAS_COLOR;
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
Image_SaveTGA
=============
*/
bool Image_SaveTGA( const char *name, rgbdata_t *pix )
{
	int		y, outsize, pixel_size;
	const byte	*bufend, *in;
	byte		*buffer, *out;
	const char	*comment = "Generated by Xash ImageLib\0";

	if(FS_FileExists( name ) && !(image.cmd_flags & IL_ALLOW_OVERWRITE ))
		return false; // already existed

	if( pix->flags & IMAGE_HAS_ALPHA )
		outsize = pix->width * pix->height * 4 + 18 + com_strlen( comment );
	else outsize = pix->width * pix->height * 3 + 18 + com_strlen( comment );

	buffer = (byte *)Mem_Alloc( Sys.imagepool, outsize );
	Mem_Set( buffer, 0, 18 );

	// prepare header
	buffer[0] = com_strlen( comment ); // tga comment length
	buffer[2] = 2; // uncompressed type
	buffer[12] = (pix->width >> 0) & 0xFF;
	buffer[13] = (pix->width >> 8) & 0xFF;
	buffer[14] = (pix->height >> 0) & 0xFF;
	buffer[15] = (pix->height >> 8) & 0xFF;
	buffer[16] = ( pix->flags & IMAGE_HAS_ALPHA ) ? 32 : 24;
	buffer[17] = ( pix->flags & IMAGE_HAS_ALPHA ) ? 8 : 0; // 8 bits of alpha
	com_strncpy( buffer + 18, comment, com_strlen( comment )); 
	out = buffer + 18 + com_strlen( comment );

	// get image description
	switch( pix->type )
	{
	case PF_RGB_24:
	case PF_BGR_24: pixel_size = 3; break;
	case PF_RGBA_32:
	case PF_BGRA_32: pixel_size = 4; break;	
	default:
		MsgDev( D_ERROR, "Image_SaveTGA: unsupported image type %s\n", PFDesc[pix->type].name );
		return false;
	}

	switch( pix->type )
	{
	case PF_RGB_24:
	case PF_RGBA_32:
		// swap rgba to bgra and flip upside down
		for( y = pix->height - 1; y >= 0; y-- )
		{
			in = pix->buffer + y * pix->width * pixel_size;
			bufend = in + pix->width * pixel_size;
			for( ; in < bufend; in += pixel_size )
			{
				*out++ = in[2];
				*out++ = in[1];
				*out++ = in[0];
				if( pix->type == PF_RGBA_32 )
					*out++ = in[3];
			}
		}
		break;
	case PF_BGR_24:
	case PF_BGRA_32:
		// flip upside down
		for( y = pix->height - 1; y >= 0; y-- )
		{
			in = pix->buffer + y * pix->width * pixel_size;
			bufend = in + pix->width * pixel_size;
			for( ; in < bufend; in += pixel_size )
			{
				*out++ = in[0];
				*out++ = in[1];
				*out++ = in[2];
				if( pix->flags & IMAGE_HAS_ALPHA )
					*out++ = in[3];
			}
		}
		break;
	}	
	FS_WriteFile( name, buffer, outsize );

	Mem_Free( buffer );
	return true;
}