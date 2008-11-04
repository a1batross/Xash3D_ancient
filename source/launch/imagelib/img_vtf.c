//=======================================================================
//			Copyright XashXT Group 2008 ©
//			img_vtf.c - vtf format load & save
//=======================================================================

#include "imagelib.h"

// VTF->DXT supply conversion structures
typedef struct
{
	long	ofs;		// buffer + ofs
	size_t	size;		// map size
} vlayer_t;

typedef struct
{
	vlayer_t	layers[512];	// max frames or layers
	int	numlayers;
} vmip_t;

typedef struct
{
	vmip_t	mips[512];	// max frames or layers	
	int	nummips;		// or cubemap counts
} vside_t;

typedef struct
{
	vside_t	sides[CB_FACECOUNT];// 6 cubemap sides and one envmap
	int	numSides;		// must be equal 1 or 7
} vtex_t;

// NOTE: not-in list VTF formats never used in games based on Source Engine
pixformat_t Image_VTFFormat( vtf_format_t srcFormat )
{
	switch( srcFormat )
	{
	case VTF_DXT1:
	case VTF_DXT1_ONEBITALPHA:
		return PF_DXT1;
	case VTF_DXT3:
		return PF_DXT3;
	case VTF_DXT5:
		return PF_DXT5;
	case VTF_RGBA16161616F:
		return PF_ABGR_64F;
	case VTF_BGR888:
		return PF_BGR_24;
	case VTF_UVWQ8888:
		return PF_UV_32;
	case VTF_BGRA8888:
		return PF_BGRA_32;
	case VTF_UV88:
		return PF_UV_16;
	case VTF_RGBA8888:
		return PF_RGBA_32;
	case VTF_RGB888:
		return PF_RGB_24;
	case VTF_UNKNOWN:
		return VTF_UNKNOWN;
	default: return PF_UNKNOWN;
	}
} 

/*
================
Image_VTFCalcLowResSize

lowres image hasn't mip-levels, frames or cubemap sides
typically params: 16x16 DXT1 but can be missing
================
*/
size_t Image_VTFCalcLowResSize( vtf_t *hdr )
{
	size_t	buffsize = 0;
	int	w, h, format;

	format = Image_VTFFormat( LittleLong( hdr->lowResImageFormat ));

	// missing lowRes image for -1 value
	if( format != VTF_UNKNOWN )
	{
		w = LittleShort( hdr->lowResImageWidth );
		h = LittleShort( hdr->lowResImageHeight );
		buffsize = Image_DXTGetLinearSize( format, w, h, 1, 0 );
	}
	return buffsize;
}

/*
================
Image_VTFCalcMipmapSize

stupid idea - last miplevel (1x1) put at begin of buffer
or DX8 format requries it, i'm don't know...
================
*/
size_t Image_VTFCalcMipmapSize( vtf_t *hdr, int mipNum ) 
{
	size_t	buffsize = 0;
	int	w, h, mipsize;

	w = max( 1, LittleShort(hdr->width)>>mipNum );
	h = max( 1, LittleShort(hdr->height)>>mipNum );
	mipsize = Image_DXTGetLinearSize( image.type, w, h, 1, 0 );
	return mipsize;
}


/*
================
Image_VTFCalcImageSize

main image size not included header or lowres
================
*/
size_t Image_VTFCalcImageSize( vtf_t *hdr, bool oldformat ) 
{
	size_t	buffsize = 0;
	int	i, numSides = 1;

	if( image.flags & IMAGE_CUBEMAP ) numSides = (oldformat) ? 6 : CB_FACECOUNT;
	for( i = 0; i < image.num_mips; i++ )
		buffsize += Image_VTFCalcMipmapSize( hdr, i ) * image.num_layers * numSides;
	return buffsize;
}

/*
================
Image_VTFSwapBuffer

VTF: <format>
header:	vtf_t
lowResImage[lowResSize]
mipmaps
  layers (frames)
    sides

DDS: <format>
header:	dds_t
sides
  mipmaps
    layers 
================
*/
void Image_VTFSwapBuffer( vtf_t *hdr, const byte *input, size_t input_size, bool oldformat )
{
	int	numSides = (image.flags & IMAGE_CUBEMAP) ? oldformat ? 6 : CB_FACECOUNT : 1; 
	vtex_t	*texture = Mem_Alloc( Sys.imagepool, sizeof( vtex_t ));
	bool	ignore_mips = (image.cmd_flags & IL_IGNORE_MIPS); 
	uint	i, j, k, out_size = 0;
	byte	*src, *dst;

	// output size can't be more than input size
	image.tempbuffer = Mem_Realloc( Sys.imagepool, image.tempbuffer, input_size );
	src = (byte *)(input + input_size);
	texture->numSides = numSides;
	dst = image.tempbuffer;

	// build image representation table
	// NOTE: src = buffer + sizeof(vtf_t) + lowResSize;
	for( i = 0; i < image.num_mips; i++ )
	{
		for( j = 0; j < image.num_layers; j++ )
		{
			for( k = 0; k < numSides; k++ )
			{
				texture->sides[k].mips[i].layers[j].size = Image_VTFCalcMipmapSize( hdr, i );
				src -= texture->sides[k].mips[i].layers[j].size;
				texture->sides[k].mips[i].layers[j].ofs = src - input;
				texture->sides[k].mips[i].numlayers++;
				texture->sides[k].nummips++;
			}
		}		
	}

	// write to DXT buffer
	for( i = 0; i < texture->numSides; i++ )
	{
		// NOTE: we needs to swap sides order
		vside_t *side = &texture->sides[texture->numSides-i-1];
		if( !oldformat && texture->numSides > 1 && i == 6 )
			continue; // skip envmap if present
		for( j = 0; j < side->nummips; j++ )
		{
			vmip_t	*mip = &side->mips[j];
			if( ignore_mips && j > 0 ) continue;
			for( k = 0; k < mip->numlayers; k++ )
			{
				// NOTE: we needs to swap frames order
				vlayer_t	*layer = &mip->layers[mip->numlayers-k-1];
				Mem_Copy( dst, input + layer->ofs, layer->size );
				out_size += layer->size;
				dst += layer->size;
			}
		}
	}

	// copy swapped buffer back to the input file, because tempbuffer
	// too unreliable place for keep this data
	Mem_Copy((byte *)input, image.tempbuffer, out_size );
	if( ignore_mips ) image.num_mips = 1;
	if( texture ) Mem_Free( texture );
	image.size = out_size; // merge out size
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
	bool	oldformat = false;
	int	i, flags, vtfFormat;
	uint	hdrSize, biasSize, resSize, lowResSize;

	fin = (byte *)buffer;
	Mem_Copy( &vtf, fin, sizeof( vtf ));
	hdrSize = LittleLong( vtf.hdr_size );
	biasSize = 0;

	if( LittleLong( vtf.ident ) != VTFHEADER ) return false; // it's not a vtf file, just skip it
	FS_FileBase( name, shortname );

	// bounds check
	i = LittleLong( vtf.ver_major );
	if( i != VTF_VERSION )
	{
		MsgDev( D_ERROR, "Image_LoadVTF: %s has wrong ver (%i should be %i)\n", shortname, i, VTF_VERSION );
		return false;
	}

	i = LittleLong( vtf.ver_minor );
	if( i == VTF_SUBVERSION0 && vtf.hdr_size == 64 ) oldformat = true; // 7.0 hasn't envmap for cubemap images
	// all other subversions are valid

	image.width = LittleShort( vtf.width );
	image.height = LittleShort( vtf.height );
	if(!Image_ValidSize( name )) return false;

	// translate VF_flags into IMAGE_flags
	flags = LittleLong( vtf.flags );
	if((flags & VF_ONEBITALPHA) || (flags & VF_EIGHTBITALPHA))
		image.flags |= IMAGE_HAS_ALPHA;
	if( flags & VF_ENVMAP ) image.flags |= IMAGE_CUBEMAP;

	vtfFormat = LittleLong( vtf.imageFormat );
	image.type = Image_VTFFormat( vtfFormat );
	image.num_layers = LittleLong( vtf.num_frames );
	image.num_mips = LittleLong( vtf.numMipLevels );

	if( image.type == PF_UNKNOWN )
	{
		MsgDev( D_ERROR, "Image_LoadVTF: file (%s) has unknown format %i\n", shortname, vtfFormat );
		return false;		
	}

	lowResSize = Image_VTFCalcLowResSize( &vtf);          
	resSize = Image_VTFCalcImageSize( &vtf, (i == 0));
	i = hdrSize + lowResSize + resSize;

	if( filesize > i )
	{
		// Valve changed format after subversion 3 and higher...
		MsgDev( D_WARN, "Image_LoadVTF: %s have invalid size\n", name );
		biasSize = filesize - i;
	}
	else if( filesize < i ) return false; // corrupted texture or somewhat

	fin += hdrSize + biasSize + lowResSize; // go to main image
	image.size = resSize; // base size can be merged after swapping

	// convert VTF to DXT
	Image_VTFSwapBuffer( &vtf, fin, resSize, oldformat );

	// FIXME: set IMAGE_HAS_ALPHA and IMAGE_HAS_COLOR properly
	image.flags |= IMAGE_HAS_COLOR;

	if( Image_ForceDecompress())
	{
		int	offset, numsides = 1;
		uint	target = 1;
		byte	*buf = fin;

		// if hardware loader is absent or image not power of two
		// or user want load current side from cubemap we run software decompressing
		if( image.flags & IMAGE_CUBEMAP ) numsides = 6;
		Image_SetPixelFormat( image.width, image.height, image.num_layers ); // setup
		image.size = image.ptr = 0;
		image.cur_mips = image.num_mips;
		image.num_mips = 0; // clear mipcount

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
		// vtf files will be uncompressed on a render. requires minimal of info for set this
		image.rgba = Mem_Alloc( Sys.imagepool, image.size );
		Mem_Copy( image.rgba, fin, image.size );
	}
	return true;
}