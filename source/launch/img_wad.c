//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_mip.c - hl1 q1 image mips
//=======================================================================

#include "launch.h"
#include "byteorder.h"
#include "filesystem.h"

/*
============
Image_LoadPAL
============
*/
bool Image_LoadPAL( const char *name, const byte *buffer, size_t filesize )
{
	if( filesize != 768 )
	{
		MsgDev( D_ERROR, "Image_LoadPAL: (%s) have invalid size (%d should be %d)\n", name, filesize, 768 );
		return false;
	}

	Image_GetPaletteLMP( buffer, LUMP_NORMAL );
	Image_CopyPalette32bit();

	image_rgba = NULL; // only palette, not real image
	image_num_mips = image_num_layers = 0;
	image_flags = IMAGE_ONLY_PALETTE;
	image_width = image_height = 0;
	image_size = 1024;
	
	return true;
}

/*
==============
Image_LoadWAL
==============
*/
bool Image_LoadWAL( const char *name, const byte *buffer, size_t filesize )
{
	wal_t 	wal;
	int	pixels, ofs[4], mipsize;
	int	i, flags, value, contents; // wal additional parms

	if( filesize < (int)sizeof(wal))
	{
		MsgDev( D_ERROR, "Image_LoadWAL: file (%s) have invalid size\n", name );
		return false;
	}
	Mem_Copy( &wal, buffer, sizeof(wal));

	flags = LittleLong(wal.flags);
	value = LittleLong(wal.value);
	contents = LittleLong(wal.contents);
	image_width = LittleLong(wal.width);
	image_height = LittleLong(wal.height);
	for(i = 0; i < 4; i++) ofs[i] = LittleLong(wal.offsets[i]);
	if(!Image_ValidSize( name )) return false;

	pixels = image_width * image_height;
	mipsize = (int)sizeof(wal) + ofs[0] + pixels;
	if( pixels > 256 && filesize < mipsize )
	{
		// NOTE: wal's with dimensions < 32 have invalid sizes.
		MsgDev( D_ERROR, "Image_LoadWAL: file (%s) have invalid size\n", name );
		return false;
	}

	image_num_layers = 1;
	image_type = PF_INDEXED_32;	// scaled up to 32-bit

	Image_GetPaletteQ2(); // hardcoded
	return FS_AddMipmapToPack( buffer + ofs[0], image_width, image_height, false );
}

/*
============
Image_LoadFLT
============
*/
bool Image_LoadFLT( const char *name, const byte *buffer, size_t filesize )
{
	flat_t	flat;
	vfile_t	*f;
	byte	temp[4];
	bool	result = false;
	int	trans_threshold = 0;
	word	column_loop, row_loop;
	int	i, column_offset, pointer_position, first_pos;
	byte	*Data, post, topdelta, length;

	// wadsupport disabled, so nothing to load
	if( Sys.app_name == HOST_NORMAL && !fs_wadsupport->integer )
		return false;

	if(filesize < (int)sizeof(flat))
	{
		MsgDev( D_ERROR, "Image_LoadFLAT: file (%s) have invalid size\n", name );
		return false;
	}

	// stupid copypaste from DevIL, but it works
	f = VFS_Create( buffer, filesize );
	first_pos = VFS_Tell( f );
	VFS_Read(f, &flat, sizeof(flat));

	image_width  = LittleShort( flat.width );
	image_height = LittleShort( flat.height );
	flat.desc[0] = LittleShort( flat.desc[0] );
	flat.desc[1] = LittleShort( flat.desc[1] );
	if(!Image_LumpValidSize( name )) return false;
	Data = (byte *)Mem_Alloc( Sys.imagepool, image_width * image_height );
	memset( Data, 247, image_width * image_height ); // set default transparency
	image_num_layers = 1;

	for( column_loop = 0; column_loop < image_width; column_loop++ )
	{
		VFS_Read(f, &column_offset, sizeof(int));
		pointer_position = VFS_Tell( f );
		VFS_Seek( f, first_pos + column_offset, SEEK_SET );

		while( 1 )
		{
			if(VFS_Read(f, &topdelta, 1) != 1) goto img_trunc;
			if( topdelta == 255 ) break;
			if(VFS_Read(f, &length, 1) != 1) goto img_trunc;
			if(VFS_Read(f, &post, 1) != 1) goto img_trunc;

			for (row_loop = 0; row_loop < length; row_loop++)
			{
				if(VFS_Read(f, &post, 1) != 1) goto img_trunc;
				if(row_loop + topdelta < image_height)
					Data[(row_loop + topdelta) * image_width + column_loop] = post;
			}
			VFS_Read( f, &post, 1 );
		}
		VFS_Seek(f, pointer_position, SEEK_SET );
	}
	VFS_Close( f );

	// swap colors in image, and check for transparency
	for( i = 0; i < image_width * image_height; i++ )
	{
		if( Data[i] == 247 )
		{
			Data[i] = 255;
			trans_threshold++;
		}
		else if( Data[i] == 255 ) Data[i] = 247;
	}

	// yes it's really transparent texture
	// otherwise transparency it's product of lazy designers (or painters ?)
	if( trans_threshold > 10 ) image_flags |= IMAGE_HAS_ALPHA;

	image_type = PF_INDEXED_32; // scaled up to 32-bit
	Image_GetPaletteD1();

	// move 247 color to 255 position
	if( d_currentpal )
	{
		Mem_Copy( temp, &d_currentpal[247], 4 );
		Mem_Copy( &d_currentpal[247], &d_currentpal[255], 4 );		
		Mem_Copy( &d_currentpal[247], temp, 4 );
	}

	result = FS_AddMipmapToPack( Data, image_width, image_height, false );
	if( Data ) Mem_Free( Data );
	return result;
img_trunc:
	VFS_Close( f );
	Mem_Free( Data );
	MsgDev( D_NOTE, "Image_LoadFLAT: probably it's not a .flat image)\n" );	
	return false;
}

/*
============
Image_LoadLMP
============
*/
bool Image_LoadLMP( const char *name, const byte *buffer, size_t filesize )
{
	lmp_t	lmp;
	byte	*fin, *pal;
	int	pixels;

	// wadsupport disabled, so nothing to load
	if( Sys.app_name == HOST_NORMAL && !fs_wadsupport->integer )
		return false;

	if( filesize < (int)sizeof(lmp))
	{
		MsgDev( D_ERROR, "Image_LoadLMP: file (%s) have invalid size\n", name );
		return false;
	}
	fin = (byte *)buffer;
	Mem_Copy(&lmp, fin, sizeof(lmp));
	image_width = LittleLong(lmp.width);
	image_height = LittleLong(lmp.height);
	fin += sizeof(lmp);
	pixels = image_width * image_height;

	if( filesize < (int)sizeof(lmp) + pixels )
	{
		MsgDev( D_ERROR, "Image_LoadLMP: file (%s) have invalid size %d\n", name, filesize );
		return false;
	}

	if(!Image_ValidSize( name )) return false;         
	image_num_mips = 1;
	image_num_layers = 1;

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
	if( fin[0] == 255 ) image_flags |= IMAGE_HAS_ALPHA;

	Image_GetPaletteLMP( pal, LUMP_NORMAL );
	image_type = PF_INDEXED_32; // scaled up to 32 bit
	return FS_AddMipmapToPack( fin, image_width, image_height, false );
}

/*
=============
Image_LoadMIP
=============
*/
bool Image_LoadMIP( const char *name, const byte *buffer, size_t filesize )
{
	mip_t	mip;
	byte	*fin, *pal;
	int	ofs[4], rendermode;
	int	i, pixels, numcolors;

	// wadsupport disabled, so nothing to load
	if( Sys.app_name == HOST_NORMAL && !fs_wadsupport->integer )
		return false;

	if( filesize < (int)sizeof(mip))
	{
		MsgDev( D_ERROR, "Image_LoadMIP: file (%s) have invalid size\n", name );
		return false;
	}

	Mem_Copy(&mip, buffer, sizeof(mip));
	image_width = LittleLong(mip.width);
	image_height = LittleLong(mip.height);
	for(i = 0; i < 4; i++) ofs[i] = LittleLong(mip.offsets[i]);
	pixels = image_width * image_height;
	image_num_layers = 1;

	if(!com_stricmp( name, "conchars" ))
	{
		// greatest hack from id software
		image_width = image_height = 128;
		image_flags |= IMAGE_HAS_ALPHA;
		rendermode = LUMP_QFONT;
		pal = NULL; // clear palette
		fin = (byte *)buffer;
	}
	else if(filesize >= (int)sizeof(mip) + ((pixels * 85)>>6) + sizeof(short) + 768)
	{
		// half-life 1.0.0.1 mip version with palette
		fin = (byte *)buffer + mip.offsets[0];
		pal = (byte *)buffer + mip.offsets[0] + (((image_width * image_height) * 85)>>6);
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
			image_flags |= IMAGE_HAS_ALPHA;
		}
		else rendermode = LUMP_NORMAL;
	}
	else if(filesize >= (int)sizeof(mip) + ((pixels * 85)>>6))
	{
		// quake1 1.01 mip version without palette
		fin = (byte *)buffer + mip.offsets[0];
		pal = NULL; // clear palette
		rendermode = LUMP_NORMAL;
	}
	else
	{
		MsgDev( D_ERROR, "Image_LoadMIP: lump (%s) is corrupted\n", name );
		return false;
	} 

	if(!Image_ValidSize( name )) return false;
	Image_GetPaletteLMP( pal, rendermode );
	image_type = PF_INDEXED_32; // scaled up to 32 bit
	return FS_AddMipmapToPack( fin, image_width, image_height, false );
}