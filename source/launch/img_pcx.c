//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_pcx.c - pcx format load & save
//=======================================================================

#include "launch.h"
#include "byteorder.h"
#include "filesystem.h"

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

	image_width = pcx.xmax + 1 - pcx.xmin;
	image_height = pcx.ymax + 1 - pcx.ymin;

	if( pcx.bits_per_pixel != 8 || pcx.manufacturer != 0x0a || pcx.version != 5 || pcx.encoding != 1)
	{
		MsgDev( D_ERROR, "Image_LoadPCX: (%s) have illegal pixel size '%d'\n", name, pcx.bits_per_pixel );
		return false;
	}
	if(!Image_ValidSize( name )) return false;
	palette = (byte *)buffer + filesize - 768;

	image_num_layers = 1;
	image_num_mips = 1;

	s = image_width * image_height;
	pbuf = (byte *)Mem_Alloc( Sys.imagepool, s );
	enddata = palette;

	for( y = 0; y < image_height && fin < enddata; y++ )
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

	// check for transparency
	for (i = 0; i < s; i++)
	{
		if( pbuf[i] == 255 )
		{
			image_flags |= IMAGE_HAS_ALPHA; // found alpha channel
			break;
		}
	}
	image_type = PF_INDEXED_32; // scaled up to 32 bit
          result = FS_AddMipmapToPack( pbuf, image_width, image_height, false );
	Mem_Free( pbuf ); // free compressed image

	return result;
}