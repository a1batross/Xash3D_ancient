//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_load.c - load various image formats
//=======================================================================

#include "imagelib.h"
#include "img_formats.h"

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
uint cubemap_image_type;	// shared image type
const char *suf[6] = { "ft", "bk", "rt", "lf", "up", "dn" };

typedef struct loadformat_s
{
	char *formatstring;
	char *ext;
	bool (*loadfunc)( const char *name, byte *buffer, size_t filesize );
} loadformat_t;

loadformat_t load_formats[] =
{
	{"textures/%s%s.%s", "dds", Image_LoadDDS},
	{"textures/%s%s.%s", "tga", Image_LoadTGA},
	{"%s%s.%s", "dds", Image_LoadDDS},
	{"%s%s.%s", "tga", Image_LoadTGA},
	{NULL, NULL}
};

rgbdata_t *ImagePack( void )
{
	rgbdata_t *pack = Mem_Alloc( zonepool, sizeof(rgbdata_t));

	if( image_cubemap && cubemap_num_sides != 6 )
	{
		// this neved be happens, just in case
		MsgDev( D_NOTE, "ImagePack: inconsistent cubemap pack %d\n", cubemap_num_sides );
		Image_FreeImage( pack );
		return NULL;
	}

	if( image_cubemap ) 
	{
		image_flags |= IMAGE_CUBEMAP;
		pack->buffer = image_cubemap;
		pack->width = cubemap_width;
		pack->height = cubemap_height;
		pack->type = cubemap_image_type;
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
		cubemap_image_type = image_type;
	}
	image_size = cubemap_width * cubemap_height * 4; // keep constant size, render.dll expecting it
          
	// mixing dds format with any existing ?
	if(image_type != cubemap_image_type) return false;

	// resampling image if needed
	resampled = Image_ResampleInternal((uint *)image_rgba, image_width, image_height, cubemap_width, cubemap_height, cubemap_image_type );
	if(!resampled) return false; // try to reasmple dxt?
	if( resampled != image_rgba ) Mem_Move( zonepool, &image_rgba, resampled, image_size ); // update buffer

	image_cubemap = Mem_Realloc( zonepool, image_cubemap, image_ptr + image_size );
	Mem_Copy(image_cubemap + image_ptr, image_rgba, image_size );

	Mem_Free( image_rgba );	// memmove aren't help us
	image_ptr += image_size; 	// move to next
	cubemap_num_sides++;	// sides counter

	return true;
}

/*
================
FS_LoadImage

loading and unpack to rgba any known image
================
*/
rgbdata_t *Image_Load(const char *filename, char *buffer, int buffsize )
{
	loadformat_t	*format;
          const char	*ext = FS_FileExtension( filename );
	char		path[128], loadname[128], texname[128];
	bool		anyformat = !com.stricmp(ext, "") ? true : false;
	int		i, filesize = 0;
	byte		*f;

#if 0     // don't try to be very clever
	if(!buffer || !buffsize) buffer = (char *)florr1_2_jpg, buffsize = sizeof(florr1_2_jpg);
#endif
	com.strncpy( loadname, filename, sizeof(loadname)-1);
	FS_StripExtension( loadname ); //remove extension if needed

	// developer warning
	if(!anyformat) MsgDev(D_NOTE, "Note: %s will be loading only with ext .%s\n", loadname, ext );
	
	// now try all the formats in the selected list
	for (format = load_formats; format->formatstring; format++)
	{
		if( anyformat || !com.stricmp(ext, format->ext ))
		{
			com.sprintf (path, format->formatstring, loadname, "", format->ext );
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
		for( format = load_formats; format->formatstring; format++ )
		{
			if( anyformat || !com.stricmp(ext, format->ext ))
			{
				com.sprintf (path, format->formatstring, loadname, suf[i], format->ext );
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
			image_cubemap = Mem_Realloc( zonepool, image_cubemap, image_ptr + image_size );
			image_ptr += image_size; // move to next
			cubemap_num_sides++; // merge counter
		}
	}

	if(image_cubemap) return ImagePack(); // now it's cubemap pack 

	// try to load image from const buffer (e.g. const byte blank_frame )
	com.strncpy( texname, filename, sizeof(texname) - 1);

	for (format = load_formats; format->formatstring; format++)
	{
		if(anyformat || !com.stricmp(ext, format->ext ))
		{
			if(buffer && buffsize > 0)
			{
				// this name will be used only for tell user about problems 
				FS_FileBase( loadname, texname );
				if( format->loadfunc(texname, buffer, buffsize ))
					return ImagePack(); // loaded
			}
		}
	}

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
	{"%s%s.%s", "dds", Image_SaveDDS},
	{NULL, NULL}
};

/*
================
Image_Save

writes image as any known format
================
*/
void Image_Save( const char *filename, rgbdata_t *pix )
{
	saveformat_t	*format;
          const char	*ext = FS_FileExtension( filename );
	char		path[128], savename[128];
	bool		anyformat = !com.stricmp(ext, "") ? true : false;
	int		filesize = 0;
	byte		*data;
	bool		has_alpha = false;

	if(!pix || !pix->buffer) return;
	if(pix->flags & IMAGE_HAS_ALPHA) has_alpha = true;
	data = pix->buffer;

	com.strncpy( savename, filename, sizeof(savename) - 1);
	FS_StripExtension( savename ); // remove extension if needed

	// developer warning
	if(!anyformat) MsgDev(D_NOTE, "Note: %s will be saving only with ext .%s\n", savename, ext );

	// now try all the formats in the selected list
	for (format = save_formats; format->formatstring; format++)
	{
		if( anyformat || !com.stricmp( ext, format->ext ))
		{
			com.sprintf( path, format->formatstring, savename, "", format->ext );
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
void Image_FreeImage( rgbdata_t *pack )
{
	if( pack )
	{
		if( pack->buffer ) Mem_Free( pack->buffer );
		if( pack->palette ) Mem_Free( pack->palette );
		Mem_Free( pack );
	}

	// reset global variables
	image_width = image_height = 0;
	cubemap_width = cubemap_height = 0;
	image_bits_count = image_flags = 0;
	cubemap_num_sides = 0;
	image_num_layers = 1;
	image_num_mips = 0;
	image_type = PF_UNKNOWN;
	image_palette = NULL;
	image_rgba = NULL;
	image_cubemap = NULL;
	image_ptr = 0;
	image_size = 0;
}