//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_mip.c - hl1 q1 image mips
//=======================================================================

#include "imagelib.h"
#include "qfiles_ref.h"

/*
============
Image_LoadPAL
============
*/
bool Image_LoadPAL( const char *name, const byte *buffer, size_t filesize )
{
	int	rendermode = LUMP_NORMAL; 

	if( filesize != 768 )
	{
		MsgDev( D_ERROR, "Image_LoadPAL: (%s) have invalid size (%d should be %d)\n", name, filesize, 768 );
		return false;
	}

	if( name[0] == '#' )
	{
		// using palette name as rendermode
		if( com.stristr( name, "normal" ))
			rendermode = LUMP_NORMAL;
		else if( com.stristr( name, "transparent" ))
			rendermode = LUMP_TRANSPARENT;
		else if( com.stristr( name, "decal" ))
			rendermode = LUMP_DECAL;
		else if( com.stristr( name, "qfont" ))
			rendermode = LUMP_QFONT;
	}

	// NOTE: image.d_currentpal not cleared with Image_Reset()
	// and stay valid any time before new call of Image_SetPalette
	Image_GetPaletteLMP( buffer, rendermode );
	Image_CopyPalette32bit();

	image.rgba = NULL;	// only palette, not real image
	image.size = 1024;	// expanded palette
	image.num_mips = image.depth = 0;
	image.width = image.height = 0;
	
	return true;
}

/*
============
Image_LoadMDL
============
*/
bool Image_LoadMDL( const char *name, const byte *buffer, size_t filesize )
{
	byte		*fin;
	size_t		pixels;
	dstudiotexture_t	*pin;
	int		flags;

	// check palette first
	if( image.hint != IL_HINT_HL ) return false; // unknown mode rejected

	if( !image.d_currentpal )
	{
		MsgDev( D_ERROR, "Image_LoadMDL: (%s) palette not installed\n", name );
		return false;		
	}
	
	pin = (dstudiotexture_t *)buffer;
	flags = LittleLong( pin->flags );

	// Valve never used endian functions for studiomodels...
	image.width = LittleLong( pin->width );
	image.height = LittleLong( pin->height );
	pixels = image.width * image.height;

	if( filesize < pixels + sizeof( dstudiotexture_t ) + 768 )		
	{
		MsgDev( D_ERROR, "Image_LoadMDL: file (%s) have invalid size\n", pin->name );
		return false;
	}

	if( flags & STUDIO_NF_TRANSPARENT )
	{
		if( image.d_rendermode != LUMP_TRANSPARENT )
			MsgDev( D_WARN, "Image_LoadMDL: (%s) using normal palette for alpha-skin\n", pin->name );
		image.flags |= IMAGE_HAS_ALPHA;
	}
	fin = (byte *)pin->index;	// setup buffer

	if(!Image_LumpValidSize( name )) return false;
	image.depth = 1;
	image.type = PF_INDEXED_32;	// 32-bit palete

	return FS_AddMipmapToPack( fin, image.width, image.height );
}

/*
============
Image_LoadSPR
============
*/
bool Image_LoadSPR( const char *name, const byte *buffer, size_t filesize )
{
	dspriteframe_t	*pin;	// indetical for q1\hl sprites

	if( image.hint == IL_HINT_HL )
	{
		if( !image.d_currentpal )
		{
			MsgDev( D_ERROR, "Image_LoadSPR: (%s) palette not installed\n", name );
			return false;		
		}
	}
	else if( image.hint == IL_HINT_Q1 )
	{
		Image_GetPaletteQ1();
	}
	else return false; // unknown mode rejected

	pin = (dspriteframe_t *)buffer;
	image.width = LittleLong( pin->width );
	image.height = LittleLong( pin->height );

	if( filesize < image.width * image.height )
	{
		MsgDev( D_ERROR, "Image_LoadSPR: file (%s) have invalid size\n", name );
		return false;
	}

	// sorry, can't validate palette rendermode
	if(!Image_LumpValidSize( name )) return false;
	image.depth = 1;
	image.type = PF_INDEXED_32;	// 32-bit palete

	// detect alpha-channel by palette type
	if( image.d_rendermode == LUMP_DECAL || image.d_rendermode == LUMP_TRANSPARENT )
		image.flags |= IMAGE_HAS_ALPHA;

	return FS_AddMipmapToPack( (byte *)(pin + 1), image.width, image.height );
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
	const byte *fin;

	if( filesize < (int)sizeof( wal ))
	{
		MsgDev( D_ERROR, "Image_LoadWAL: file (%s) have invalid size\n", name );
		return false;
	}
	Mem_Copy( &wal, buffer, sizeof( wal ));

	flags = LittleLong(wal.flags);
	value = LittleLong(wal.value);
	contents = LittleLong(wal.contents);
	image.width = LittleLong(wal.width);
	image.height = LittleLong(wal.height);
	for(i = 0; i < 4; i++) ofs[i] = LittleLong(wal.offsets[i]);
	if(!Image_LumpValidSize( name )) return false;

	pixels = image.width * image.height;
	mipsize = (int)sizeof(wal) + ofs[0] + pixels;
	if( pixels > 256 && filesize < mipsize )
	{
		// NOTE: wal's with dimensions < 32 have invalid sizes.
		MsgDev( D_ERROR, "Image_LoadWAL: file (%s) have invalid size\n", name );
		return false;
	}

	image.depth = 1;
	image.type = PF_INDEXED_32;	// 32-bit palete
	fin = buffer + ofs[0];

	// check for luma pixels
	for( i = 0; i < image.width * image.height; i++ )
	{
		if( fin[i] > 208 && fin[i] < 240 )
		{
			image.flags |= IMAGE_HAS_LUMA_Q2;
			break;
		}
	}

	Image_GetPaletteQ2(); // hardcoded
	return FS_AddMipmapToPack( fin, image.width, image.height );
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
	bool	result = false;
	int	trans_pixels = 0;
	word	column_loop, row_loop;
	int	i, column_offset, pointer_position, first_pos;
	byte	*Data, post, topdelta, length;

	// wadsupport disabled, so nothing to load
	if( Sys.app_name == HOST_NORMAL && !fs_wadsupport->integer )
		return false;

	if(filesize < (int)sizeof( flat ))
	{
		MsgDev( D_ERROR, "Image_LoadFLAT: file (%s) have invalid size\n", name );
		return false;
	}

	// stupid copypaste from DevIL, but it works
	f = VFS_Create( buffer, filesize );
	first_pos = VFS_Tell( f );
	VFS_Read(f, &flat, sizeof(flat));

	image.width  = LittleShort( flat.width );
	image.height = LittleShort( flat.height );
	flat.desc[0] = LittleShort( flat.desc[0] );
	flat.desc[1] = LittleShort( flat.desc[1] );
	if(!Image_LumpValidSize( name )) return false;
	Data = (byte *)Mem_Alloc( Sys.imagepool, image.width * image.height );
	Mem_Set( Data, 247, image.width * image.height ); // set default transparency
	image.depth = 1;

	for( column_loop = 0; column_loop < image.width; column_loop++ )
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

			for( row_loop = 0; row_loop < length; row_loop++ )
			{
				if(VFS_Read(f, &post, 1) != 1) goto img_trunc;
				if(row_loop + topdelta < image.height)
					Data[(row_loop + topdelta) * image.width + column_loop] = post;
			}
			VFS_Read( f, &post, 1 );
		}
		VFS_Seek( f, pointer_position, SEEK_SET );
	}
	VFS_Close( f );

	// swap colors in image, and check for transparency
	for( i = 0; i < image.width * image.height; i++ )
	{
		if( Data[i] == 247 )
		{
			Data[i] = 255;
			trans_pixels++;
		}
		else if( Data[i] == 255 ) Data[i] = 247;
	}

	// yes it's really transparent texture
	// otherwise transparency it's a product of lazy designers (or painters ?)
	if( trans_pixels > TRANS_THRESHOLD ) image.flags |= IMAGE_HAS_ALPHA;

	image.type = PF_INDEXED_32;	// 32-bit palete
	Image_GetPaletteD1();

	result = FS_AddMipmapToPack( Data, image.width, image.height );
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
	int	rendermode;
	int	pixels;

	// wadsupport disabled, so nothing to load
	if( Sys.app_name == HOST_NORMAL && !fs_wadsupport->integer )
		return false;

	if( filesize < sizeof( lmp ))
	{
		MsgDev( D_ERROR, "Image_LoadLMP: file (%s) have invalid size\n", name );
		return false;
	}

	// greatest hack from id software
	if( image.hint != IL_HINT_HL && com.stristr( name, "conchars" ))
	{
		image.width = image.height = 128;
		image.flags |= IMAGE_HAS_ALPHA;
		rendermode = LUMP_QFONT;
		filesize += sizeof(lmp);
		fin = (byte *)buffer;
	}
	else
	{
		fin = (byte *)buffer;
		Mem_Copy( &lmp, fin, sizeof( lmp ));
		image.width = LittleLong( lmp.width );
		image.height = LittleLong( lmp.height );
		rendermode = LUMP_NORMAL;
		fin += sizeof(lmp);
	}
	pixels = image.width * image.height;

	if( filesize < sizeof( lmp ) + pixels )
	{
		MsgDev( D_ERROR, "Image_LoadLMP: file (%s) have invalid size %d\n", name, filesize );
		return false;
	}

	if(!Image_ValidSize( name )) return false;         
	image.depth = 1;

	if( image.hint != IL_HINT_Q1 && filesize > (int)sizeof(lmp) + pixels )
	{
		int	numcolors;

		pal = fin + pixels;
		numcolors = BuffLittleShort( pal );
		if( numcolors != 256 ) pal = NULL; // corrupted lump ?
		else pal += sizeof( short );
	}
	else if( image.hint != IL_HINT_HL ) pal = NULL;
	else return false; // unknown mode rejected
	if( fin[0] == 255 ) image.flags |= IMAGE_HAS_ALPHA;

	Image_GetPaletteLMP( pal, rendermode );
	image.type = PF_INDEXED_32;	// 32-bit palete
	return FS_AddMipmapToPack( fin, image.width, image.height );
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

	Mem_Copy( &mip, buffer, sizeof( mip ));
	image.width = LittleLong( mip.width );
	image.height = LittleLong( mip.height );
	if(!Image_ValidSize( name )) return false;
	for( i = 0; i < 4; i++ ) ofs[i] = LittleLong( mip.offsets[i] );
	pixels = image.width * image.height;
	image.depth = 1;

	if( image.hint != IL_HINT_Q1 && filesize >= (int)sizeof(mip) + ((pixels * 85)>>6) + sizeof(short) + 768)
	{
		// half-life 1.0.0.1 mip version with palette
		fin = (byte *)buffer + mip.offsets[0];
		pal = (byte *)buffer + mip.offsets[0] + (((image.width * image.height) * 85)>>6);
		numcolors = BuffLittleShort( pal );
		if( numcolors != 256 ) pal = NULL; // corrupted mip ?
		else  pal += sizeof(short); // skip colorsize 
		// detect rendermode
		if( com.strchr( name, '{' ))
		{
			rendermode = LUMP_TRANSPARENT;

			// qlumpy used this color for transparent textures, otherwise it's decals
 			if( pal[255*3+0] == 0 && pal[255*3+1] == 0 && pal[255*3+2] == 255 );
			else if(!( image.cmd_flags & IL_KEEP_8BIT ))
			{
				// apply decal palette immediately
				image.flags |= IMAGE_COLORINDEX;
				rendermode = LUMP_DECAL;
			}
			image.flags |= IMAGE_HAS_ALPHA;
		}
		else rendermode = LUMP_NORMAL;
	}
	else if( image.hint != IL_HINT_HL && filesize >= (int)sizeof(mip) + ((pixels * 85)>>6))
	{
		// quake1 1.01 mip version without palette
		fin = (byte *)buffer + mip.offsets[0];
		pal = NULL; // clear palette
		rendermode = LUMP_NORMAL;

		// check for luma pixels
		for( i = 0; i < image.width * image.height; i++ )
		{
			if( fin[i] > 224 )
			{
				image.flags |= IMAGE_HAS_LUMA_Q1;
				break;
			}
		}
	}
	else
	{
		if( image.hint == IL_HINT_NO )
			MsgDev( D_ERROR, "Image_LoadMIP: lump (%s) is corrupted\n", name );
		return false; // unknown or unsupported mode rejected
	} 

	Image_GetPaletteLMP( pal, rendermode );
	image.type = PF_INDEXED_32;	// 32-bit palete
	return FS_AddMipmapToPack( fin, image.width, image.height );
}