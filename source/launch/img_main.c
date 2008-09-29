//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_load.c - load various image formats
//=======================================================================

#include "launch.h"
#include "byteorder.h"
#include "filesystem.h"
#include "mathlib.h"

// global image variables
int image_width, image_height;
byte image_num_layers = 1;	// num layers in
byte image_num_mips = 0;	// build mipmaps
uint image_type;		// main type switcher
uint image_flags;		// additional image flags
byte image_bits_count;	// bits per RGBA
size_t image_size;		// image rgba size
uint image_ptr;
byte *image_palette;	// palette pointer
byte *image_rgba;		// image pointer (see image_type for details)

// cubemap variables
int cubemap_width, cubemap_height;
int cubemap_num_sides;	// how mach sides is loaded 
byte *image_cubemap;	// cubemap pack
uint base_image_type;	// shared image type for all mipmaps or cubemap sides
const char *suf[6] = { "ft", "bk", "rt", "lf", "up", "dn" };

typedef struct loadformat_s
{
	char *formatstring;
	char *ext;
	bool (*loadfunc)( const char *name, const byte *buffer, size_t filesize );
} loadformat_t;

static loadformat_t load_formats0[] =
{
	{"textures/%s%s.%s", "dds", Image_LoadDDS},	// cubemaps, textures
	{"textures/%s%s.%s", "png", Image_LoadPNG},	// levelshot save as .png
	{"textures/%s%s.%s", "tga", Image_LoadTGA},	// all other
	{"%s%s.%s", "dds", Image_LoadDDS},
	{"%s%s.%s", "png", Image_LoadPNG},
	{"%s%s.%s", "tga", Image_LoadTGA},
	{NULL, NULL, NULL}
};

static loadformat_t load_formats1[] =
{
	{"textures/%s%s.%s", "dds", Image_LoadDDS},
	{"textures/%s%s.%s", "tga", Image_LoadTGA},
	{"textures/%s%s.%s", "jpg", Image_LoadJPG},
	{"textures/%s%s.%s", "png", Image_LoadPNG},
	{"textures/%s%s.%s", "mip", Image_LoadMIP},
	{"%s%s.%s", "dds", Image_LoadDDS},
	{"%s%s.%s", "tga", Image_LoadTGA},
	{"%s%s.%s", "jpg", Image_LoadJPG},
	{"%s%s.%s", "png", Image_LoadPNG},
	{"%s%s.%s", "mip", Image_LoadMIP},
	{NULL, NULL, NULL}
};

static loadformat_t load_formats2[] =
{
	{"textures/%s%s.%s", "dds", Image_LoadDDS},
	{"textures/%s%s.%s", "tga", Image_LoadTGA},
	{"textures/%s%s.%s", "jpg", Image_LoadJPG},
	{"textures/%s%s.%s", "png", Image_LoadPNG},
	{"textures/%s%s.%s", "mip", Image_LoadMIP},
	{"textures/%s%s.%s", "bmp", Image_LoadBMP},
	{"textures/%s%s.%s", "wal", Image_LoadWAL},
	{"textures/%s%s.%s", "pcx", Image_LoadPCX},
	{"textures/%s%s.%s", "lmp", Image_LoadLMP},
	{"%s%s.%s", "dds", Image_LoadDDS},
	{"%s%s.%s", "tga", Image_LoadTGA},
	{"%s%s.%s", "jpg", Image_LoadJPG},
	{"%s%s.%s", "png", Image_LoadPNG},
	{"%s%s.%s", "mip", Image_LoadMIP},
	{"%s%s.%s", "bmp", Image_LoadBMP},
	{"%s%s.%s", "wal", Image_LoadWAL},
	{"%s%s.%s", "pcx", Image_LoadPCX},
	{"%s%s.%s", "lmp", Image_LoadLMP},
	{NULL, NULL, NULL}
};

static loadformat_t load_formats3[] =
{
	{"textures/%s%s.%s", "dds", Image_LoadDDS},
	{"textures/%s%s.%s", "tga", Image_LoadTGA},
	{"textures/%s%s.%s", "jpg", Image_LoadJPG},
	{"textures/%s%s.%s", "png", Image_LoadPNG},
	{"textures/%s%s.%s", "mip", Image_LoadMIP},
	{"textures/%s%s.%s", "bmp", Image_LoadBMP},
	{"textures/%s%s.%s", "wal", Image_LoadWAL},
	{"textures/%s%s.%s", "pcx", Image_LoadPCX},
	{"textures/%s%s.%s", "lmp", Image_LoadLMP},
	{"textures/%s%s.%s", "flt", Image_LoadFLT},
	{"textures/%s%s.%s", "pal", Image_LoadPAL},
	{"%s%s.%s", "dds", Image_LoadDDS},
	{"%s%s.%s", "tga", Image_LoadTGA},
	{"%s%s.%s", "jpg", Image_LoadJPG},
	{"%s%s.%s", "png", Image_LoadPNG},
	{"%s%s.%s", "mip", Image_LoadMIP},
	{"%s%s.%s", "bmp", Image_LoadBMP},
	{"%s%s.%s", "wal", Image_LoadWAL},
	{"%s%s.%s", "pcx", Image_LoadPCX},
	{"%s%s.%s", "lmp", Image_LoadLMP},
	{"%s%s.%s", "flt", Image_LoadFLT},
	{"%s%s.%s", "pal", Image_LoadPAL},
	{NULL, NULL, NULL}
};

static loadformat_t load_formats4[] =
{
	{"%s%s.%s", "bmp", Image_LoadBMP},
	{"%s%s.%s", "pcx", Image_LoadPCX},
	{"%s%s.%s", "mip", Image_LoadMIP},
	{"%s%s.%s", "wal", Image_LoadWAL},
	{"%s%s.%s", "lmp", Image_LoadLMP},
	{"%s%s.%s", "flt", Image_LoadFLT},
	{NULL, NULL, NULL}
};

void Image_Reset( void )
{
	// reset global variables
	image_width = image_height = 0;
	cubemap_width = cubemap_height = 0;
	image_bits_count = image_flags = 0;
	cubemap_num_sides = 0;
	image_num_layers = 1;
	base_image_type = 0;
	image_num_mips = 0;
	image_type = PF_UNKNOWN;

	// pointers will be saved with prevoius picture struct
	// don't care about it
	image_palette = NULL;
	image_rgba = NULL;
	image_cubemap = NULL;
	image_ptr = 0;
	image_size = 0;
}

rgbdata_t *ImagePack( void )
{
	rgbdata_t *pack = Mem_Alloc( Sys.imagepool, sizeof(rgbdata_t));

	if( image_cubemap && cubemap_num_sides != 6 )
	{
		// this neved be happens, just in case
		MsgDev( D_NOTE, "ImagePack: inconsistent cubemap pack %d\n", cubemap_num_sides );
		FS_FreeImage( pack );
		return NULL;
	}

	if( image_cubemap ) 
	{
		image_flags |= IMAGE_CUBEMAP;
		pack->buffer = image_cubemap;
		pack->width = cubemap_width;
		pack->height = cubemap_height;
		pack->type = base_image_type;
		pack->size = image_size * cubemap_num_sides;
	}
	else 
	{
		pack->buffer = image_rgba;
		pack->width = image_width;
		pack->height = image_height;
		pack->type = image_type;
		pack->size = image_size;
	}

	pack->numLayers = image_num_layers;
	pack->numMips = image_num_mips;
	pack->bitsCount = image_bits_count;
	pack->flags = image_flags;
	pack->palette = image_palette;
	return pack;
}

bool FS_AddImageToPack( const char *name )
{
	byte	*resampled;
	
	// first image have suffix "ft" and set average size for all cubemap sides!
	if(!image_cubemap)
	{
		cubemap_width = image_width;
		cubemap_height = image_height;
		base_image_type = image_type;
	}
	image_size = cubemap_width * cubemap_height * 4; // keep constant size, render.dll expecting it
          
	// mixing dds format with any existing ?
	if(image_type != base_image_type) return false;

	// resampling image if needed
	resampled = Image_ResampleInternal((uint *)image_rgba, image_width, image_height, cubemap_width, cubemap_height, base_image_type );
	if(!resampled) return false; // try to reasmple dxt?
	if( resampled != image_rgba ) Mem_Move( Sys.imagepool, &image_rgba, resampled, image_size ); // update buffer

	image_cubemap = Mem_Realloc( Sys.imagepool, image_cubemap, image_ptr + image_size );
	Mem_Copy(image_cubemap + image_ptr, image_rgba, image_size );

	Mem_Free( image_rgba );	// memmove aren't help us
	image_ptr += image_size; 	// move to next
	cubemap_num_sides++;	// sides counter

	return true;
}

bool FS_AddMipmapToPack( const byte *in, int width, int height, bool expand )
{
	int mipsize = width * height;
	int outsize = width * height;

	// check for inconsistency
	if( !base_image_type ) base_image_type = image_type;
	// trying to add 8 bit mimpap into 32-bit mippack or somewhat...
	if( base_image_type != image_type ) return false;
	if( expand ) outsize *= 4;
	else Image_CopyPalette32bit(); 

	// reallocate image buffer
	image_rgba = Mem_Realloc( Sys.imagepool, image_rgba, image_size + outsize );	
	if( !expand ) Mem_Copy( image_rgba + image_ptr, in, outsize );
	else if( !Image_Copy8bitRGBA( in, image_rgba + image_ptr, mipsize ))
		return false; // probably pallette not installed

	image_size += outsize;
	image_ptr += outsize;
	image_num_mips++;

	return true;
}

void Image_ConvertToRGBA( rgbdata_t *pic )
{
	int	j, texels;
	byte	*in, *out, *buffer;
	bool	result = false;

	if( !pic )
	{
		MsgDev( D_ERROR, "Image_ConvertToRGBA: image not loaded\n" );
		return;
	}
	if(!pic->palette && (pic->type == PF_INDEXED_24 || pic->type == PF_INDEXED_32))
	{
		MsgDev( D_ERROR, "Image_ConvertToRGBA: indexed image doesn't have palette\n" );
		return;
	}
	if( pic->type == PF_RGBA_32 || pic->type == PF_ABGR_64 )
		return; // nothing to process

	texels = pic->width * pic->height;
	buffer = Mem_Alloc( Sys.imagepool, texels * 4 );
	in = pic->buffer;
	out = buffer;

	switch( pic->type )
	{
	case PF_DXT1:
	case PF_DXT3:
	case PF_DXT5:
		result = Image_DecompressDXTC( &pic );
		break;
	case PF_RGB_24:
		//
		for( j = 0; j < texels; j++, in += 3, out += 4 )
		{
			out[0] = in[0];
			out[1] = in[1];
			out[2] = in[2];
			out[3] = 255;
		}
		result = true;
		break;			
	case PF_INDEXED_24:
		// FIXME: make flag IMAGE_HAS_INDEXALPHA for decals ?
		if( pic->flags & IMAGE_HAS_ALPHA )
			Image_GetPaletteLMP( pic->palette, LUMP_TRANSPARENT ); 
		else Image_GetPaletteLMP( pic->palette, LUMP_NORMAL );
		// intentional falltrough
	case PF_INDEXED_32:
		d_currentpal = ( uint *)pic->palette;
		result = Image_Copy8bitRGBA( pic->buffer, buffer, texels );
		break;
	default: break; // unsupported format
	}

	if( result )
	{
		MsgDev( D_NOTE, "Image_ConvertToRGBA: from %s to RGBA 32\n", PFDesc[pic->type].name ); 
		pic->type = PF_RGBA_32; // sucessfully converted
		Mem_Free( pic->buffer );
		pic->buffer = buffer;
	}
	else
	{
		MsgDev( D_WARN, "Image_ConvertToRGBA: can't convert from %s to RGBA 32\n", PFDesc[pic->type].name );
		Mem_Free( buffer );
	}
}

/*
================
FS_LoadImage

loading and unpack to rgba any known image
================
*/
rgbdata_t *FS_LoadImage( const char *filename, const byte *buffer, size_t buffsize )
{
          const char	*ext = FS_FileExtension( filename );
	char		path[128], loadname[128], texname[128];
	loadformat_t	*format, *desired_formats = load_formats0;
	bool		anyformat = !com_stricmp(ext, "") ? true : false;
	int		i, filesize = 0;
	byte		*f;

#if 0     // don't try to be very clever
	if( !buffer || !buffsize ) buffer = (char *)florr1_2_jpg, buffsize = sizeof(florr1_2_jpg);
#endif
	switch( Sys.app_name )
	{
	case HOST_NORMAL:
	case HOST_BSPLIB:
	case HOST_VIEWER:
		switch( img_oldformats->integer )
		{
		case 0: desired_formats = load_formats0; break;	// tga, dds
		case 1: desired_formats = load_formats1; break;	// tga, dds, jpg, png, mip
		case 2: desired_formats = load_formats2; break;	// tga, dds, jpg, png, mip, bmp, pcx, wal, lmp
		case 3: desired_formats = load_formats3; break;	// tga, dds, jpg, png, mip, bmp, pcx, wal, lmp, flat, pal
		default: desired_formats = load_formats0; break;	// tga, dds
		}
		break;
	case HOST_SPRITE:
	case HOST_STUDIO:
	case HOST_WADLIB:
		desired_formats = load_formats4;		// bmp, pcx, mip, wal, lmp, flat
		break;
	case HOST_RIPPER:
		// everything else
		desired_formats = load_formats3;		// tga, dds, jpg, png, mip, bmp, pcx, wal, lmp, flat, pal
		break;
	default:
		// other instances not needs image support
		desired_formats = NULL;
		break;	
	}

	Image_Reset(); // clear old image

	// skip any checks, load file from buffer
	if(com_stristr( filename, "#internal" ) && buffer && buffsize )
		goto load_internal;

	com_strncpy( loadname, filename, sizeof(loadname)-1);
	FS_StripExtension( loadname ); //remove extension if needed

	// developer warning
	if(!anyformat) MsgDev(D_NOTE, "Note: %s will be loading only with ext .%s\n", loadname, ext );
	
	// now try all the formats in the selected list
	for( format = desired_formats; format && format->formatstring; format++)
	{
		if( anyformat || !com_stricmp(ext, format->ext ))
		{
			com_sprintf( path, format->formatstring, loadname, "", format->ext );
			f = FS_LoadFile( path, &filesize );
			if(f && filesize > 0)
			{
				// this name will be used only for tell user about problems 
				FS_FileBase( path, texname );
				if( format->loadfunc(texname, f, filesize ))
				{
					Mem_Free(f); // release buffer
					return ImagePack(); // loaded
				}
			}
		}
	}

	// maybe it skybox or cubemap ?
	for( i = 0; i < 6; i++ )
	{
		for( format = desired_formats; format && format->formatstring; format++ )
		{
			if( anyformat || !com_stricmp(ext, format->ext ))
			{
				com_sprintf( path, format->formatstring, loadname, suf[i], format->ext );
				f = FS_LoadFile( path, &filesize );
				if(f && filesize > 0)
				{
					// this name will be used only for tell user about problems 
					FS_FileBase( path, texname );
					if( format->loadfunc(texname, f, filesize ))
					{
						if(FS_AddImageToPack(va("%s%s.%s", loadname, suf[i], format->ext)))
							break; // loaded
					}
					Mem_Free(f);
				}
			}
		}
		if( cubemap_num_sides != i + 1 ) // check side
		{
			// first side not found, probably it's not cubemap
			// it contain info about image_type and dimensions, don't generate black cubemaps 
			if(!image_cubemap) break;
			MsgDev(D_ERROR, "FS_LoadImage: couldn't load (%s%s.%s), create black image\n",loadname,suf[i],ext );

			// Mem_Alloc already filled memblock with 0x00, no need to do it again
			image_cubemap = Mem_Realloc( Sys.imagepool, image_cubemap, image_ptr + image_size );
			image_ptr += image_size; // move to next
			cubemap_num_sides++; // merge counter
		}
	}

	if( image_cubemap ) return ImagePack(); // now it's cubemap pack 

load_internal:
	// try to load image from const buffer (e.g. const byte blank_frame )
	com_strncpy( texname, filename, sizeof(texname) - 1);

	for( format = desired_formats; format && format->formatstring; format++ )
	{
		if( anyformat || !com_stricmp( ext, format->ext ))
		{
			if( buffer && buffsize > 0  )
			{
				// this name will be used only for tell user about problems 
				FS_FileBase( texname, texname );
				if( format->loadfunc( texname, buffer, buffsize ))
					return ImagePack(); // loaded
			}
		}
	}

	if( !desired_formats ) MsgDev( D_NOTE, "FS_LoadImage: imagelib offline\n" );
	else if(!com_stristr( filename, "#internal" ))
		MsgDev(D_WARN, "FS_LoadImage: couldn't load \"%s\"\n", texname );

	return NULL;
}

typedef struct saveformat_s
{
	const char *formatstring;
	const char *ext;
	bool (*savefunc)( const char *name, rgbdata_t *pix, int saveformat );
} saveformat_t;

saveformat_t save_formats[] =
{
	{"%s%s.%s", "tga", Image_SaveTGA},
	{"%s%s.%s", "png", Image_SavePNG},
	{"%s%s.%s", "dds", Image_SaveDDS},
	{"%s%s.%s", "bmp", Image_SaveBMP},
	{"%s%s.%s", "pcx", Image_SavePCX},
	{NULL, NULL}
};

/*
================
Image_Save

writes image as any known format
================
*/
void FS_SaveImage( const char *filename, rgbdata_t *pix )
{
	saveformat_t	*format;
          const char	*ext = FS_FileExtension( filename );
	char		path[128], savename[128];
	bool		anyformat = !com_stricmp(ext, "") ? true : false;
	int		filesize = 0;
	byte		*data;
	bool		has_alpha = false;

	if(!pix || !pix->buffer) return;
	if(pix->flags & IMAGE_HAS_ALPHA) has_alpha = true;
	data = pix->buffer;

	com_strncpy( savename, filename, sizeof(savename) - 1);
	FS_StripExtension( savename ); // remove extension if needed

	// developer warning
	if(!anyformat) MsgDev(D_NOTE, "Note: %s will be saving only with ext .%s\n", savename, ext );

	// now try all the formats in the selected list
	for (format = save_formats; format->formatstring; format++)
	{
		if( anyformat || !com_stricmp( ext, format->ext ))
		{
			com_sprintf( path, format->formatstring, savename, "", format->ext );
			if( format->savefunc( path, pix, pix->hint ))
				break; // saved
		}
	}
}

/*
================
Image_FreeImage

free RGBA buffer
================
*/
void FS_FreeImage( rgbdata_t *pack )
{
	if( pack )
	{
		if( pack->buffer ) Mem_Free( pack->buffer );
		if( pack->palette ) Mem_Free( pack->palette );
		Mem_Free( pack );
	}
}