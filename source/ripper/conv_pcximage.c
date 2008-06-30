//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       conv_pcximage.c - convert pcximages
//=======================================================================

#include "ripper.h"
#include "pal_utils.h"

bool PCX_ConvertImage( const char *name, char *buffer, int filesize )
{
	pcx_t	pcx;
	uint	*trans;
	int	s, i, p, x, y, x2, dataByte;
	byte	*pix, *pbuf, *palette, *fin, *enddata;
	rgbdata_t	pic;

	fin = buffer;
	Mem_Copy(&pcx, fin, sizeof(pcx));
	fin += sizeof(pcx);

	// probably it's not pcx file
	if (pcx.manufacturer != 0x0a || pcx.version != 5 || pcx.encoding != 1 ) return false;
	if (filesize < (int)sizeof(pcx) + 768)
	{
		MsgDev( D_ERROR, "ConvPCX: file (%s) have invalid size\n", name );
		return false;
	}
	memset( &pic, 0, sizeof(pic));

	pcx.xmax = LittleShort (pcx.xmax);
	pcx.xmin = LittleShort (pcx.xmin);
	pcx.ymax = LittleShort (pcx.ymax);
	pcx.ymin = LittleShort (pcx.ymin);
	pcx.hres = LittleShort (pcx.hres);
	pcx.vres = LittleShort (pcx.vres);
	pcx.bytes_per_line = LittleShort (pcx.bytes_per_line);
	pcx.palette_type = LittleShort (pcx.palette_type);

	pic.width = pcx.xmax + 1 - pcx.xmin;
	pic.height = pcx.ymax + 1 - pcx.ymin;

	if( pcx.bits_per_pixel != 8 || pcx.manufacturer != 0x0a || pcx.version != 5 || pcx.encoding != 1)
	{
		MsgDev( D_ERROR, "ConvPCX: (%s) have illegal pixel size '%d'\n", name, pcx.bits_per_pixel );
		return false;
	}
	if(pic.width > 640 || pic.height > 640 || pic.width <= 0 || pic.height <= 0)
	{
		MsgDev( D_ERROR, "ConvPCX: (%s) dimensions out of range [%dx%d]\n", name, pic.width, pic.height );
		return false;
	}
	palette = buffer + filesize - 768;

	pic.numLayers = 1;
	pic.numMips = 1;
	pic.type = PF_RGBA_32;

	s = pic.width * pic.height;
	pic.size = s * 4;
	pbuf = (byte *)Mem_Alloc( zonepool, s );
	pic.buffer = (byte *)Mem_Alloc( zonepool, pic.size );
	trans = (uint *)pic.buffer;
	enddata = palette;

	for (y = 0; y < pic.height && fin < enddata; y++)
	{
		pix = pbuf + y * pic.width;
		for (x = 0; x < pic.width && fin < enddata;)
		{
			dataByte = *fin++;
			if(dataByte >= 0xC0)
			{
				if(fin >= enddata) break;
				x2 = x + (dataByte & 0x3F);
				dataByte = *fin++;
				if (x2 > pic.width) x2 = pic.width; // technically an error
				while(x < x2) pix[x++] = dataByte;
			}
			else pix[x++] = dataByte;
		}
		// the number of bytes per line is always forced to an even number
		fin += pcx.bytes_per_line - pic.width;
		while(x < pic.width) pix[x++] = 0;
	}

	Conv_GetPalettePCX( palette );

	// convert to rgba
	for (i = 0; i < s; i++)
	{
		p = pbuf[i];
		if (p == 255)
		{
			pic.flags |= IMAGE_HAS_ALPHA; // found alpha channel
			((byte *)&trans[i])[0] = ((byte *)&d_currentpal[0])[0];
			((byte *)&trans[i])[1] = ((byte *)&d_currentpal[0])[1];
			((byte *)&trans[i])[2] = ((byte *)&d_currentpal[0])[2];
			((byte *)&trans[i])[3] = ((byte *)&d_currentpal[p])[3];
		}
		else trans[i] = d_currentpal[p];
	}

	Mem_Free( pbuf ); // free compressed image
	FS_StripExtension( (char *)name );
	Image->SaveImage( va("%s/%s.tga", gs_gamedir, name ), &pic ); // save converted image
	Mem_Free( pic.buffer ); // release buffer

	return true;
}

/*
============
ConvPCX
============
*/
bool ConvPCX( const char *name, char *buffer, int filesize )
{
	string picname, path;

	FS_FileBase( name, picname );
	if(!com.strnicmp("num", picname, 3 ))
		com.snprintf( path, MAX_STRING, "gfx/fonts/%s", picname );
	else if(!com.strnicmp("anum", picname, 4 ))
		com.snprintf( path, MAX_STRING, "gfx/fonts/%s", picname );
	else if(!com.strnicmp("conchars", picname, 8 ))
		com.snprintf( path, MAX_STRING, "gfx/fonts/%s", picname );
	else if(!com.strnicmp("a_", picname, 2 ))
		com.snprintf( path, MAX_STRING, "gfx/hud/%s", picname );
	else if(!com.strnicmp("p_", picname, 2 ))
		com.snprintf( path, MAX_STRING, "gfx/hud/%s", picname );
	else if(!com.strnicmp("k_", picname, 2 ))
		com.snprintf( path, MAX_STRING, "gfx/hud/%s", picname );
	else if(!com.strnicmp("i_", picname, 2 ))
		com.snprintf( path, MAX_STRING, "gfx/hud/%s", picname );
	else if(!com.strnicmp("w_", picname, 2 ))
		com.snprintf( path, MAX_STRING, "gfx/hud/%s", picname );
	else if(!com.strnicmp("m_", picname, 2 ))
		com.snprintf( path, MAX_STRING, "gfx/menu/%s", picname );
	else if(!com.strnicmp("m_", picname, 2 ))
		com.snprintf( path, MAX_STRING, "gfx/menu/%s", picname );
	else com.snprintf( path, MAX_STRING, "gfx/common/%s", picname );		

	if(PCX_ConvertImage( path, buffer, filesize ))
	{
		Msg("%s\n", name ); // echo to console about current image
		return true;
	}
	return false;
}