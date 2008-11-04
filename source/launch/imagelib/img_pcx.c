//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_pcx.c - pcx format load & save
//=======================================================================

#include "imagelib.h"

/*
=============
Image_LoadPCX
=============
*/
bool Image_LoadPCX( const char *name, const byte *buffer, size_t filesize )
{
	pcx_t	pcx;
	bool	result = false;
	int	s, i, x, y, x2, dataByte;
	byte	*pix, *pbuf, *palette, *fin, *enddata;

	fin = (byte *)buffer;
	Mem_Copy(&pcx, fin, sizeof(pcx));
	fin += sizeof(pcx);

	// probably it's not pcx file
	if( pcx.manufacturer != 0x0a || pcx.version != 5 || pcx.encoding != 1 ) return false;
	if( filesize < (int)sizeof(pcx) + 768)
	{
		MsgDev( D_ERROR, "Image_LoadPCX: file (%s) have invalid size\n", name );
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

	image.width = pcx.xmax + 1 - pcx.xmin;
	image.height = pcx.ymax + 1 - pcx.ymin;

	if( pcx.bits_per_pixel != 8 || pcx.manufacturer != 0x0a || pcx.version != 5 || pcx.encoding != 1)
	{
		MsgDev( D_ERROR, "Image_LoadPCX: (%s) have unknown version '%d'\n", name, pcx.version );
		return false;
	}
	if(!Image_ValidSize( name )) return false;
	palette = (byte *)buffer + filesize - 768;

	image.num_layers = 1;
	image.num_mips = 1;

	s = image.width * image.height;
	pbuf = (byte *)Mem_Alloc( Sys.imagepool, s );
	enddata = palette;

	for( y = 0; y < image.height && fin < enddata; y++ )
	{
		pix = pbuf + y * image.width;
		for (x = 0; x < image.width && fin < enddata;)
		{
			dataByte = *fin++;
			if(dataByte >= 0xC0)
			{
				if (fin >= enddata) break;
				x2 = x + (dataByte & 0x3F);
				dataByte = *fin++;
				if (x2 > image.width) x2 = image.width; // technically an error
				while(x < x2) pix[x++] = dataByte;
			}
			else pix[x++] = dataByte;
		}
		// the number of bytes per line is always forced to an even number
		fin += pcx.bytes_per_line - image.width;
		while(x < image.width) pix[x++] = 0;
	}

	// NOTE: IL_HINT_Q2 does wrong result for sprite frames. use with caution
	if( image.hint == IL_HINT_Q2 ) Image_GetPaletteQ2();
	else Image_GetPalettePCX( palette );

	// check for transparency
	for (i = 0; i < s; i++)
	{
		if( pbuf[i] == 255 )
		{
			image.flags |= IMAGE_HAS_ALPHA; // found alpha channel
			break;
		}
	}
	image.type = PF_INDEXED_32; // 32 bit palette
          result = FS_AddMipmapToPack( pbuf, image.width, image.height );
	Mem_Free( pbuf ); // free compressed image

	return result;
}

/* 
============== 
Image_SavePCX
============== 
*/ 
bool Image_SavePCX( const char *name, rgbdata_t *pix )
{
	byte	*data, *out, *pack;
	byte	*palette;
	file_t	*file;
	int	i, j;
	pcx_t	pcx;

	if(FS_FileExists( name ) && !(image.cmd_flags & IL_ALLOW_OVERWRITE ))
		return false; // already existed

	// bogus parameter check
	if( !pix->palette || !pix->buffer )
		return false;

	switch( pix->type )
	{
	case PF_INDEXED_24:
		break;
	case PF_INDEXED_32:
		Image_ConvertPalTo24bit( pix );
		break;
	default:
		MsgDev( D_WARN, "Image_SavePCX: unsupported image type %s\n", PFDesc[pix->type].name );
		return false;
	}
	  
	out = Mem_Alloc( Sys.imagepool, ( pix->size * 2 ) + 768 ); 
	Mem_Set( &pcx, 0, sizeof( pcx ));
	
	pcx.manufacturer = 0x0a;		// PCX id
	pcx.version = 5;			// 256 color
 	pcx.encoding = 1;			// uncompressed
	pcx.bits_per_pixel = 8;		// 256 color
	pcx.xmin = 0;
	pcx.ymin = 0;
	pcx.xmax = LittleShort((short)(pix->width - 1));
	pcx.ymax = LittleShort((short)(pix->height - 1));
	pcx.hres = LittleShort((short)pix->width);
	pcx.vres = LittleShort((short)pix->height);
	pcx.color_planes = 1;		// chunky image
	pcx.bytes_per_line = LittleShort((short)pix->width);
	pcx.palette_type = LittleShort( 1 );	// not a grey scale

	// pack the image
	palette = pix->palette;
	data = pix->buffer;
	pack = out;

	// simple runlength encoding	
	for( i = 0; i < pix->height; i++ )
	{
		for( j = 0; j < pix->width; j++ )
		{
			if((*data & 0xc0) != 0xc0 )
			{
				*pack++ = *data++;
			}
			else
			{
				*pack++ = 0xc1;
				*pack++ = *data++;
			}
		}
	}
			
	// write the palette
	*pack++ = 0x0c;	// palette ID byte
	for( i = 0; i < 768; i++ )
		*pack++ = *palette++;

	file = FS_Open( name, "wb" );
	if( !file ) return false;

	// write header
	FS_Write( file, &pcx, sizeof( pcx_t ));

	// write image and palette
	FS_Write( file, out, pack - out ); 
	Mem_Free( out );

	FS_Close( file );
	return true;
}