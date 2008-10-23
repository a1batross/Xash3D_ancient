//=======================================================================
//			Copyright XashXT Group 2008 ©
//			img_vtf.c - vtf format load & save
//=======================================================================

#include "imagelib.h"

pixformat_t Image_VTFFormat( vtf_format_t srcFormat )
{
	switch( srcFormat )
	{
	case VTF_UNKNOWN:
		return VTF_UNKNOWN;
	case VTF_RGBA8888:
		return PF_RGBA_32;
	case VTF_RGB888:
		return PF_RGB_24;
	case VTF_BGR888:
		return PF_BGR_24;
	case VTF_RGB565:
		return PF_RGB_16;
	case VTF_I8:
		return PF_LUMINANCE;
	case VTF_IA88:
		return PF_LUMINANCE_ALPHA;
	case VTF_P8:
		return PF_INDEXED_24;
	case VTF_ARGB8888:
		return PF_ARGB_32;
	case VTF_DXT1:
	case VTF_DXT1_ONEBITALPHA:
		return PF_DXT1;
	case VTF_DXT3:
		return PF_DXT3;
	case VTF_DXT5:
		return PF_DXT5;
	case VTF_BGRA8888:
		return PF_BGRA_32;
	case VTF_RGBA16161616F:
		return PF_ABGR_64F;
	default: return PF_UNKNOWN;
	}
} 

size_t Image_VTFCalcImageSize( vtf_t *hdr, int numFrames )
{
	size_t	mipsize;
	int	numSides = 1;

	if( image.flags & IMAGE_CUBEMAP ) numSides = 6;
	mipsize = Image_DXTGetLinearSize( image.type, hdr->width, hdr->height, 1, 0 );
	return mipsize * numFrames * numSides;
} 

/*
================
Image_VTFCalcMipmapSize

stupid idea - last miplevel (1x1) put at begin of buffer
or DX8 format requries it, i'm don't know...
================
*/
size_t Image_VTFCalcMipmapSize( vtf_t *hdr, int startMip, int numFrames ) 
{
	size_t	buffsize = 0;
	int	numSides = 1;
	int	w, h, i, mipsize = 0;

	if( image.flags & IMAGE_CUBEMAP ) numSides = 6;

	// now correct buffer size
	for( i = image.num_mips - 1; i > startMip; --i )
	{
		w = max( 1, (hdr->width)>>i);
		h = max( 1, (hdr->height)>>i);
		mipsize = Image_DXTGetLinearSize( image.type, w, h, 1, 0 );
		buffsize += mipsize * numFrames * numSides;
	}
	return buffsize;
}

size_t Image_VTFCalcLowResSize( vtf_t *hdr )
{
	size_t	buffsize = 0;
	int	w, h, format;

	format = Image_VTFFormat( LittleLong( hdr->lowResImageFormat ));

	// missing lowRes image for -1 value
	if( format != VTF_UNKNOWN )
	{
		w = hdr->lowResImageWidth;
		h = hdr->lowResImageHeight;
		buffsize = Image_DXTGetLinearSize( format, w, h, 1, 0 );
	}
	return buffsize;
}

/*
=============
Image_LoadVTF
=============
*/
bool Image_LoadVTF( const char *name, const byte *buffer, size_t filesize )
{            
	vtf_t	vtf;
	byte	*fin;
	string	shortname;
	int	i, flags, numFrames, vtfFormat;
	uint	hdrSize, resSize, mipSize, lowResSize;

	fin = (byte *)buffer;
	Mem_Copy( &vtf, fin, sizeof( vtf ));
	hdrSize = LittleLong( vtf.hdr_size );

	if( LittleLong( vtf.ident ) != VTFHEADER ) return false; // it's not a dds file, just skip it
	FS_FileBase( name, shortname );

	// bounds check
	i = LittleLong( vtf.ver_major );
	if( i != VTF_VERSION )
	{
		MsgDev( D_ERROR, "Image_LoadVTF: %s has wrong ver (%i should be %i)\n", shortname, i, VTF_VERSION );
		return false;
	}

	i = LittleLong( vtf.ver_minor );
	if( i == VTF_SUBVERSION1 && vtf.hdr_size == 64 )
		MsgDev( D_NOTE, "Image_LoadVTF: ver 7.1 without HDR\n" );
	else if( i == VTF_SUBVERSION2 && vtf.hdr_size == sizeof( vtf_t ))
		MsgDev( D_NOTE, "Image_LoadVTF: ver 7.2 with HDR\n" );
	else
	{
		MsgDev( D_ERROR, "Image_LoadVTF: file (%s) has wrong subversion %i\n", shortname, i );
		return false;
	}

	image.width = LittleShort( vtf.width );
	image.height = LittleShort( vtf.height );
	if(!Image_ValidSize( name )) return false;

	// translate VF_flags into IMAGE_flags
	flags = LittleLong( vtf.flags );
	if((flags & VF_ONEBITALPHA) || (flags & VF_EIGHTBITALPHA))
		image.flags |= IMAGE_HAVE_ALPHA;
	if( flags & VF_ENVMAP ) image.flags |= IMAGE_CUBEMAP;

	vtfFormat = LittleLong( vtf.imageFormat );
	numFrames = LittleLong( vtf.num_frames );
	image.type = Image_VTFFormat( vtfFormat );
	image.num_mips = LittleLong( vtf.numMipLevels );

	lowResSize = Image_VTFCalcLowResSize( &vtf );
	resSize = Image_VTFCalcImageSize( &vtf, numFrames );
	mipSize = Image_VTFCalcMipmapSize( &vtf, 0, numFrames );
	image.size = hdrSize + lowResSize + mipSize + resSize;
	fin += hdrSize + lowResSize + mipSize;
	image.num_layers = 1;

	if( image.type == PF_UNKNOWN )
	{
		MsgDev( D_ERROR, "Image_LoadVTF: file (%s) has unknown format %i\n", shortname, vtfFormat );
		return false;		
	}
	else if( image.size == filesize )
	{
		int	offset, numsides = 1;
		uint	target = 1;
		byte	*buf = fin;

		// if hardware loader is absent or image not power of two
		// or user want load current side from cubemap we run software decompressing
		if( image.flags & IMAGE_CUBEMAP ) numsides = 6;
		Image_SetPixelFormat( image.width, image.height, image.num_layers ); // setup
		image.size = image.ptr = 0;
		if( image.cmd_flags & IL_IGNORE_MIPS )
			image.cur_mips = 1;
		else image.cur_mips = image.num_mips;
		image.num_mips = 1; // defaulting to one mip

		for( i = 0, offset = 0; i < numsides; i++, buf += offset )
		{
			Image_SetPixelFormat( image.curwidth, image.curheight, image.curdepth );
			offset = image.SizeOfFile; // move pointer

			Image_DecompressDDS( buf, target + i );
		}
		// now we can change type to RGBA
		if( image.hint != IL_HINT_NO ) image.flags &= ~IMAGE_CUBEMAP; // side extracted
		image.type = PF_RGBA_32;
	}
	else
	{
		MsgDev( D_WARN, "wrong vtf size: %i, calc size %i\n", filesize, image.size );  
		return false;
	}
	return true;
}