//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       conv_wadlumps.c - convert wad lumps
//=======================================================================

#include "idconv.h"

/*
============
ConvPAL
============
*/
bool ConvPAL( char *name, char *buffer, int filesize )
{
	if( filesize != 768 )
	{
		MsgWarn("LoadPAL: file (%s) have invalid size\n", name );
		return false;
	}

	// not a real image, just palette lump
	Conv_GetPaletteLMP( buffer, LUMP_NORMAL );

	// write palette out ?
	return true;
}

/*
============
ConvFLT
============
*/
bool ConvFLT( const char *name, char *buffer, int filesize )
{
	flat_t	flat;
	vfile_t	*f;
	word	column_loop, row_loop;
	int	i, column_offset, pointer_position, first_pos, pixels;
	byte	*Data, post, topdelta, length;
	rgbdata_t	pic;

	if(filesize < (int)sizeof(flat))
	{
		MsgWarn("LoadFLAT: file (%s) have invalid size\n", name );
		return false;
	}
	memset( &pic, 0, sizeof(pic));

	// stupid copypaste from DevIL, but it works
	f = VFS_Create( buffer, filesize );
	first_pos = VFS_Tell( f );
	VFS_Read(f, &flat, sizeof(flat));

	pic.width  = LittleShort( flat.width );
	pic.height = LittleShort( flat.height );
	flat.desc[0] = LittleShort( flat.desc[0] );
	flat.desc[1] = LittleShort( flat.desc[1] );
	if(!Lump_ValidSize( (char *)name, &pic, 256, 256 )) return false;
	Data = (byte *)Mem_Alloc( zonepool, pic.width * pic.height );
	memset( Data, 247, pic.width * pic.height ); // default mask
	pic.numLayers = 1;
	pic.type = PF_RGBA_32;

	for( column_loop = 0; column_loop < pic.width; column_loop++ )
	{
		VFS_Read(f, &column_offset, sizeof(int));
		pointer_position = VFS_Tell( f );
		VFS_Seek( f, first_pos + column_offset, SEEK_SET );

		while( 1 )
		{
			if(VFS_Read(f, &topdelta, 1) != 1) return false;
			if(topdelta == 255) break;
			if(VFS_Read(f, &length, 1) != 1) return false;
			if(VFS_Read(f, &post, 1) != 1) return false;

			for (row_loop = 0; row_loop < length; row_loop++)
			{
				if(VFS_Read(f, &post, 1) != 1) return false;
				if(row_loop + topdelta < pic.height)
					Data[(row_loop + topdelta) * pic.width + column_loop] = post;
			}
			VFS_Read(f, &post, 1);
		}
		VFS_Seek(f, pointer_position, SEEK_SET );
	}
	VFS_Close( f );

	// scan for transparency
	for (i = 0; i < pic.width * pic.height; i++)
	{
		if( Data[i] == 247 )
		{
			pic.flags |= IMAGE_HAS_ALPHA;
			break;
		}
	}

	pixels = pic.width * pic.height;
	pic.size = pixels * 4;
	pic.buffer = (byte *)Mem_Alloc( zonepool, pic.size );

	Conv_GetPaletteD1();
	Conv_Copy8bitRGBA( Data, pic.buffer, pixels );
	Mem_Free( Data );

	FS_StripExtension( (char *)name );
	FS_SaveImage(va("%s/textures/%s.tga", gs_gamedir, name ), &pic ); // save converted image
	Conv_CreateShader( name, &pic );
	Mem_Free( pic.buffer ); // release buffer
	Msg("%s.flat\n", name ); // echo to console about current texture

	return true;
}

/*
============
ConvMIP
============
*/
bool ConvMIP( const char *name, char *buffer, int filesize )
{
	mip_t	mip;
	rgbdata_t	pic;
	byte	*fin, *pal;
	int	ofs[4], rendermode;
	int	i, pixels, numcolors;

	if (filesize < (int)sizeof(mip))
	{
		MsgWarn("LoadMIP: file (%s) have invalid size\n", name );
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

	if(!com.stricmp( name, "gfx/conchars.mip" ))
	{
		// greatest hack from id software
		pic.width = pic.height = 128;
		pic.flags |= IMAGE_HAS_ALPHA;
		rendermode = LUMP_QFONT;
		pixels = pic.width * pic.height;
		pal = NULL; // clear palette
		fin = buffer;
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
		if( name[0] == '{' )
		{
			// qlumpy used this color for transparent textures, otherwise it's decals
 			if(pal[255*3+0] == 0 && pal[255*3+1] == 0 && pal[255*3+2] == 255)
				rendermode = LUMP_TRANSPARENT;
			else rendermode = LUMP_DECAL;
			pic.flags |= IMAGE_HAS_ALPHA;
		}
		else rendermode = LUMP_NORMAL;
	}
	else if(filesize >= (int)sizeof(mip) + ((pixels * 85)>>6))
	{
		// quake1 1.01 mip version without palette
		pal = NULL; // clear palette
		rendermode = LUMP_NORMAL;
		fin = buffer + mip.offsets[0];
	}
	else
	{
		MsgWarn("LoadMIP: lump (%s) is corrupted\n", name );
		return false;
	} 

	if(!Lump_ValidSize((char *)name, &pic, 640, 640 )) return false;
	pic.size = pixels * 4;
	pic.buffer = (byte *)Mem_Alloc(zonepool, pic.size );
	Conv_GetPaletteLMP( pal, rendermode );
	Conv_Copy8bitRGBA( fin, pic.buffer, pixels );

	FS_StripExtension( (char *)name );
	FS_SaveImage(va("%s/textures/%s.tga", gs_gamedir, name ), &pic ); // save converted image
	Conv_CreateShader( name, &pic );
	Mem_Free( pic.buffer ); // release buffer
	Msg("%s.mip\n", name ); // echo to console about current texture

	return true;
}

/*
============
ConvLMP
============
*/
bool ConvLMP( const char *name, char *buffer, int filesize )
{
	lmp_t	lmp;
	byte	*fin, *pal;
	int	pixels;
	rgbdata_t	pic;

	if (filesize < (int)sizeof(lmp))
	{
		MsgWarn("LoadLMP: file (%s) have invalid size\n", name );
		return false;
	}
	memset( &pic, 0, sizeof(pic));

	fin = buffer;
	Mem_Copy(&lmp, fin, sizeof(lmp));
	pic.width = LittleLong(lmp.width);
	pic.height = LittleLong(lmp.height);
	fin += sizeof(lmp);
	pixels = pic.width * pic.height;

	if(filesize < (int)sizeof(lmp) + pixels)
	{
		MsgWarn("LoadLMP: file (%s) have invalid size %d\n", name, filesize );
		return false;
	}

	if(!Lump_ValidSize((char *)name, &pic, 640, 480 )) return false;         
          pic.size = pixels * 4;
	pic.numMips = 1;

	pic.buffer = (byte *)Mem_Alloc(zonepool, pic.size );
	pic.numLayers = 1;
	pic.type = PF_RGBA_32;

	// half-life 1.0.0.1 lmp version with palette
	if( filesize > (int)sizeof(lmp) + pixels )
	{
		int numcolors;
		pal = fin + pixels;
		numcolors = BuffLittleShort( pal );
		if( numcolors != 256 ) pal = NULL; // corrupted lump ?
		else pal += sizeof(short);
	}
	else pal = NULL;
	if(fin[0] == 255) pic.flags |= IMAGE_HAS_ALPHA;

	pic.numLayers = 1;
	pic.type = PF_RGBA_32;

	FS_StripExtension((char *)name );
	Conv_GetPaletteLMP( pal, LUMP_NORMAL );
	Conv_Copy8bitRGBA( fin, pic.buffer, pixels );
	FS_SaveImage(va("%s/graphics/%s.tga", gs_gamedir, name ), &pic ); // save converted image
	Mem_Free( pic.buffer ); // release buffer
	Msg("%s.lmp\n", name ); // echo to console about current image

	return true;
}

/*
============
ConvFNT
============
*/
bool ConvFNT( const char *name, char *buffer, int filesize )
{
	qfont_t	font;
	byte	*pal, *fin;
	int	i, pixels, fullsize;
	int	numcolors;
	rgbdata_t	pic;

	if(filesize < (int)sizeof(font))
	{
		MsgWarn("LoadFNT: file (%s) have invalid size\n", name );
		return false;
	}
	Mem_Copy(&font, buffer, sizeof(font));

	// swap lumps
	font.width = LittleShort(font.width);
	font.height = LittleShort(font.height);
	font.rowcount = LittleShort(font.rowcount);
	font.rowheight = LittleShort(font.rowheight);
	for(i = 0; i < 256; i++)
	{
		font.fontinfo[i].startoffset = LittleShort( font.fontinfo[i].startoffset ); 
		font.fontinfo[i].charwidth = LittleShort( font.fontinfo[i].charwidth );
	}
	
	// last sixty four bytes - what the hell ????
	fullsize = sizeof(qfont_t)-4+(128 * font.width * QCHAR_WIDTH) + sizeof(short) + 768 + 64;

	if( fullsize != filesize )
	{
		// oldstyle font: "conchars" or "creditsfont"
		pic.width = 256; // hardcoded
		pic.height = font.height;
	}
	else
	{
		// Half-Life 1.1.0.0 font style (qfont_t)
		pic.width = font.width * QCHAR_WIDTH;
		pic.height = font.height;
	}

	pic.numLayers = 1;
	pic.type = PF_RGBA_32;
	if(!Lump_ValidSize((char *)name, &pic, 512, 512 )) return false;
	fin = buffer + sizeof(font) - 4;

	pixels = pic.width * pic.height;
	pal = fin + pixels;
	numcolors = BuffLittleShort( pal ), pal += sizeof(short);
	pic.flags |= IMAGE_HAS_ALPHA; // fonts always have transparency

	if( numcolors == 768 )
	{
		// newstyle font
		Conv_GetPaletteLMP( pal, LUMP_QFONT );
	}
	else if( numcolors == 256 )
	{
		// oldstyle font
		Conv_GetPaletteLMP( pal, LUMP_TRANSPARENT );
	}
	else 
	{
		MsgWarn("LoadFNT: file (%s) have invalid palette size %d\n", name, numcolors );
		return false;
	}

	pixels = pic.width * pic.height;
	pic.size = pixels * 4;
	pic.buffer = (byte *)Mem_Alloc(zonepool, pic.size );
	pic.numLayers = 1;
	pic.type = PF_RGBA_32;

	FS_StripExtension((char *)name );
	Conv_Copy8bitRGBA( fin, pic.buffer, pixels );
	FS_SaveImage(va("%s/graphics/%s.tga", gs_gamedir, name ), &pic ); // save converted image
	Mem_Free( pic.buffer ); // release buffer
	Msg("%s.qfont\n", name ); // echo to console about current font

	return true;
}