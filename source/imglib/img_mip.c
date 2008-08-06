//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_mip.c - hl1 q1 image mips
//=======================================================================

#include "imagelib.h"
#include "byteorder.h"
#include "img_formats.h"

/*
=============
Image_LoadMIP
=============
*/
bool Image_LoadMIP( const char *name, const byte *buffer, size_t filesize )
{
	dmip_t	mip;
	byte	*fin, *pal;
	int	ofs[4], rendermode;
	int	i, k, l, pixels, numcolors;

	if( filesize < (int)sizeof(mip))
	{
		MsgDev( D_ERROR, "Image_LoadMIP: file (%s) have invalid size\n", name );
		return false;
	}

	memset( &pic, 0, sizeof(pic));

	Mem_Copy(&mip, buffer, sizeof(mip));
	pic.width = LittleLong(mip.width);
	pic.height = LittleLong(mip.height);
	for(i = 0; i < 4; i++) ofs[i] = LittleLong(mip.offsets[i]);
	pixels = pic.width * pic.height;
	pic.numLayers = 1;
	pic.type = PF_RGBA_32;

	if(!com.stricmp( savedname, "gfx/conchars" ))
	{
		// greatest hack from id software
		pic.width = pic.height = 128;
		pic.flags |= IMAGE_HAS_ALPHA;
		rendermode = LUMP_QFONT;
		pixels = pic.width * pic.height;
		pal = NULL; // clear palette
		fin = buffer;
		com.snprintf( filepath, MAX_STRING, "%s/gfx/%s.tga", gs_gamedir, savedname );
	}
	else if(filesize >= (int)sizeof(mip) + ((pixels * 85)>>6) + sizeof(short) + 768)
	{
		// half-life 1.0.0.1 mip version with palette
		fin = buffer + mip.offsets[0];
		pal = buffer + mip.offsets[0] + (((pic.width * pic.height) * 85)>>6);
		numcolors = BuffLittleShort( pal );
		if(numcolors != 256) pal = NULL; // corrupted mip ?
		else  pal += sizeof(short); // skip colorsize 
		// detect rendermode
		if( com.strchr( savedname, '{' ))
		{
			// qlumpy used this color for transparent textures, otherwise it's decals
 			if(pal[255*3+0] == 0 && pal[255*3+1] == 0 && pal[255*3+2] == 255)
				rendermode = LUMP_TRANSPARENT;
			else rendermode = LUMP_DECAL;
			pic.flags |= IMAGE_HAS_ALPHA;
		}
		else rendermode = LUMP_NORMAL;
		com.snprintf( filepath, MAX_STRING, "%s/textures/%s.tga", gs_gamedir, savedname );
	}
	else if(filesize >= (int)sizeof(mip) + ((pixels * 85)>>6))
	{
		// quake1 1.01 mip version without palette
		pal = NULL; // clear palette
		rendermode = LUMP_NORMAL;
		fin = buffer + mip.offsets[0];
		com.snprintf( filepath, MAX_STRING, "%s/textures/%s.tga", gs_gamedir, savedname );
	}
	else
	{
		MsgDev( D_ERROR, "LoadMIP: lump (%s) is corrupted\n", savedname );
		return false;
	} 

	if(!Lump_ValidSize( savedname, &pic, 640, 640 )) return false;
	pic.size = pixels * 4;
	pic.buffer = (byte *)Mem_Alloc(zonepool, pic.size );
	Conv_GetPaletteLMP( pal, rendermode );
	Conv_Copy8bitRGBA( fin, pic.buffer, pixels );

	Image->SaveImage( filepath, &pic ); // save converted image
	Conv_CreateShader( name, &pic, "mip", NULL, 0, 0 );
	Mem_Free( pic.buffer ); // release buffer
	Msg("%s.mip\n", savedname ); // echo to console about current texture

	return true;
}

bool Image_SaveMIP( const char *name, rgbdata_t *pix, int saveformat )
{
	return false;
}