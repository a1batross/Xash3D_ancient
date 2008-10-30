//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_load.c - load various image formats
//=======================================================================

#include "imagelib.h"
#include "mathlib.h"

// global image variables
imglib_t	image;

typedef struct suffix_s
{
	const char	*suf;
	uint		flags;
	image_hint_t	hint;
} suffix_t;

static const suffix_t skybox_3ds[6] =
{
{ "ft", IMAGE_FLIP_X, CB_HINT_POSX },
{ "bk", IMAGE_FLIP_Y, CB_HINT_NEGX },
{ "rt", IMAGE_ROT_90, CB_HINT_POSY },
{ "lf", IMAGE_ROT270, CB_HINT_NEGY },
{ "up", IMAGE_ROT_90, CB_HINT_POSZ },
{ "dn", IMAGE_ROT_90, CB_HINT_NEGZ },
};

static const suffix_t cubemap_v1[6] =
{
{ "posx", 0, CB_HINT_POSX },
{ "negx", 0, CB_HINT_NEGX },
{ "posy", 0, CB_HINT_POSY },
{ "negy", 0, CB_HINT_NEGY },
{ "posz", 0, CB_HINT_POSZ },
{ "negz", 0, CB_HINT_NEGZ },
};

static const suffix_t cubemap_v2[6] =
{
{ "px", 0, CB_HINT_POSX },
{ "nx", 0, CB_HINT_NEGX },
{ "py", 0, CB_HINT_POSY },
{ "ny", 0, CB_HINT_NEGY },
{ "pz", 0, CB_HINT_POSZ },
{ "nz", 0, CB_HINT_NEGZ },
};

typedef struct cubepack_s
{
	const char	*name;	// just for debug
	const suffix_t	*type;
} cubepack_t;

static const cubepack_t load_cubemap[] =
{
{ "3Ds Sky ", skybox_3ds },
{ "3Ds Cube", cubemap_v2 },
{ "Tenebrae", cubemap_v1 },		// FIXME: remove this ?
{ NULL, NULL },
};

// soul of ImageLib - table of image format constants 
const bpc_desc_t PFDesc[] =
{
{PF_UNKNOWN,	"raw",	0x1908,	0x1401, 0,  0,  0 },
{PF_INDEXED_24,	"pal 24",	0x1908,	0x1401, 1,  1,  0 }, // expand data to RGBA buffer
{PF_INDEXED_32,	"pal 32",	0x1908,	0x1401, 1,  1,  0 },
{PF_RGBA_32,	"RGBA 32",0x1908,	0x1401, 4,  1, -4 },
{PF_BGRA_32,	"BGRA 32",0x80E1,	0x1401, 4,  1, -4 },
{PF_ARGB_32,	"ARGB 32",0x1908,	0x1401, 4,  1, -4 },
{PF_ABGR_64,	"ABGR 64",0x80E1,	0x1401, 4,  2, -8 },
{PF_RGB_24,	"RGB 24",	0x1908,	0x1401, 3,  1, -3 },
{PF_BGR_24,	"BGR 24",	0x80E0,	0x1401, 3,  1, -3 },
{PF_RGB_16,	"RGB 16",	0x8054,	0x8364, 3,  2, 16 }, // FIXME: do revision
{PF_DXT1,		"DXT1",	0x1908,	0x1401, 4,  1,  8 },
{PF_DXT2,		"DXT2",	0x1908,	0x1401, 4,  1, 16 },
{PF_DXT3,		"DXT3",	0x1908,	0x1401, 4,  1, 16 },
{PF_DXT4,		"DXT4",	0x1908,	0x1401, 4,  1, 16 },
{PF_DXT5,		"DXT5",	0x1908,	0x1401, 4,  1, 16 },
{PF_RXGB,		"RXGB",	0x1908,	0x1401, 3,  1, 16 },
{PF_ATI1N,	"ATI1N",	0x1908,	0x1401, 1,  1,  8 },
{PF_ATI2N,	"3DC",	0x1908,	0x1401, 3,  1, 16 },
{PF_LUMINANCE,	"LUM 8",	0x1909,	0x1401, 1,  1, -1 },
{PF_LUMINANCE_16,	"LUM 16", 0x1909,	0x1401, 2,  2, -2 },
{PF_LUMINANCE_ALPHA,"LUM A",	0x190A,	0x1401, 2,  1, -2 },
{PF_UV_16,	"UV 16",	0x190A,	0x1401, 2,  1, -2 },
{PF_UV_16,	"UV 16",	0x190A,	0x1401, 2,  1, -4 },
{PF_R_16F,	"R 16f",	0x8884,	0x1406, 1,  4, -2 }, // FIXME: these NV extension, reinstall for ATI
{PF_R_32F,	"R 32f",	0x8885,	0x1406, 1,  4, -4 },
{PF_GR_32F,	"GR 32f",	0x8886,	0x1406, 2,  4, -4 },
{PF_GR_64F,	"GR 64f",	0x8887,	0x1406, 2,  4, -8 },
{PF_ABGR_64F,	"ABGR64f",0x888A,	0x1406, 4,  2, -8 },
{PF_ABGR_128F,	"ABGR128",0x888B,	0x1406, 4,  4, -16},
{PF_RGBA_GN,	"system",	0x1908,	0x1401, 4,  1, -4 },
};

bpc_desc_t *Image_GetPixelFormat( pixformat_t type )
{
	type = bound( PF_UNKNOWN, type, PF_TOTALCOUNT - 1 );
	return (bpc_desc_t *)&PFDesc[type];
}

void Image_Reset( void )
{
	// reset global variables
	image.width = image.height = 0;
	image.source_width = image.source_height = 0;
	image.bits_count = image.flags = 0;
	image.num_sides = 0;
	image.num_layers = 1;
	image.source_type = 0;
	image.num_mips = 0;
	image.filter = CB_HINT_NO;
	image.type = PF_UNKNOWN;

	// pointers will be saved with prevoius picture struct
	// don't care about it
	image.palette = NULL;
	image.cubemap = NULL;
	image.rgba = NULL;
	image.ptr = 0;
	image.size = 0;
}

rgbdata_t *ImagePack( void )
{
	rgbdata_t *pack = Mem_Alloc( Sys.imagepool, sizeof(rgbdata_t));

	if( image.cubemap && image.num_sides != 6 )
	{
		// this neved be happens, just in case
		MsgDev( D_NOTE, "ImagePack: inconsistent cubemap pack %d\n", image.num_sides );
		FS_FreeImage( pack );
		return NULL;
	}

	if( image.cubemap ) 
	{
		image.flags |= IMAGE_CUBEMAP;
		pack->buffer = image.cubemap;
		pack->width = image.source_width;
		pack->height = image.source_height;
		pack->type = image.source_type;
		pack->size = image.size * image.num_sides;
	}
	else 
	{
		pack->buffer = image.rgba;
		pack->width = image.width;
		pack->height = image.height;
		pack->type = image.type;
		pack->size = image.size;
	}

	pack->numLayers = image.num_layers;
	pack->numMips = image.num_mips;
	pack->bitsCount = image.bits_count;
	pack->flags = image.flags;
	pack->palette = image.palette;
	return pack;
}

/*
================
FS_AddSideToPack

FIXME: rewrite with VirtualFS using ?
================
*/
bool FS_AddSideToPack( const char *name, int adjust_flags )
{
	byte	*resampled, *flipped;
	
	// first side set average size for all cubemap sides!
	if( !image.cubemap )
	{
		image.source_width = image.width;
		image.source_height = image.height;
		image.source_type = image.type;
	}
	image.size = image.source_width * image.source_height * 4; // keep constant size, render.dll expecting it
          
	// mixing dds format with any existing ?
	if( image.type != image.source_type )
		return false;

	// flip image if needed
	flipped = Image_FlipInternal( image.rgba, &image.width, &image.height, image.source_type, adjust_flags );
	if( !flipped ) return false; // try to reasmple dxt?
	if( flipped != image.rgba ) image.rgba = Image_Copy( image.size );

	// resampling image if needed
	resampled = Image_ResampleInternal((uint *)image.rgba, image.width, image.height, image.source_width, image.source_height, image.source_type );
	if( !resampled ) return false; // try to reasmple dxt?
	if( resampled != image.rgba ) image.rgba = Image_Copy( image.size );

	image.cubemap = Mem_Realloc( Sys.imagepool, image.cubemap, image.ptr + image.size );
	Mem_Copy( image.cubemap + image.ptr, image.rgba, image.size ); // add new side

	Mem_Free( image.rgba );	// release source buffer
	image.ptr += image.size; 	// move to next
	image.num_sides++;		// bump sides count

	return true;
}

bool FS_AddMipmapToPack( const byte *in, int width, int height )
{
	int mipsize = width * height;
	int outsize = width * height;

	// check for inconsistency
	if( !image.source_type ) image.source_type = image.type;
	// trying to add 8 bit mimpap into 32-bit mippack or somewhat...
	if( image.source_type != image.type ) return false;
	if(!( image.cmd_flags & IL_KEEP_8BIT )) outsize *= 4;
	else Image_CopyPalette32bit(); 

	// reallocate image buffer
	image.rgba = Mem_Realloc( Sys.imagepool, image.rgba, image.size + outsize );	
	if( image.cmd_flags & IL_KEEP_8BIT ) Mem_Copy( image.rgba + image.ptr, in, outsize );
	else if( !Image_Copy8bitRGBA( in, image.rgba + image.ptr, mipsize ))
		return false; // probably pallette not installed

	image.size += outsize;
	image.ptr += outsize;
	image.num_mips++;

	return true;
}

/*
================
FS_LoadImage

loading and unpack to rgba any known image
================
*/
rgbdata_t *FS_LoadImage( const char *filename, const byte *buffer, size_t size )
{
          const char	*ext = FS_FileExtension( filename );
	string		path, loadname, sidename;
	bool		anyformat = !com_stricmp( ext, "" ) ? true : false;
	bool		dds_installed = false; // current loadformats list supported dds
	int		i, filesize = 0;
	const loadformat_t	*format;
	const cubepack_t	*cmap;
	byte		*f;

#if 0     // don't try to be very clever
	if( !buffer || !buffsize ) buffer = (char *)florr1_2_jpg, buffsize = sizeof(florr1_2_jpg);
#endif
	Image_Reset(); // clear old image
	com.strncpy( loadname, filename, sizeof( loadname ));
	FS_StripExtension( loadname ); // remove extension if needed

	// HACKHACK: skip any checks, load file from buffer
	if( filename[0] == '#' && buffer && size ) goto load_internal;

	// developer warning
	if( !anyformat ) MsgDev( D_NOTE, "Note: %s will be loading only with ext .%s\n", loadname, ext );
	
	// now try all the formats in the selected list
	for( format = image.loadformats; format && format->formatstring; format++)
	{
		if( !com_stricmp( format->ext, "dds" )) dds_installed = true;
		if( anyformat || !com_stricmp( ext, format->ext ))
		{
			com_sprintf( path, format->formatstring, loadname, "", format->ext );
			image.hint = format->hint;
			f = FS_LoadFile( path, &filesize );
			if( f && filesize > 0 )
			{
				if( format->loadfunc( path, f, filesize ))
				{
					Mem_Free(f); // release buffer
					return ImagePack(); // loaded
				}
			}
		}
	}

	// special case for extract cubemap side from dds image
	if( dds_installed && (anyformat || !com_stricmp( ext, "dds" )))
	{
		// first, check for cubemap suffixes
		for( cmap = load_cubemap; cmap && cmap->type; cmap++ )
		{
			for( i = 0; i < 6; i++ )
			{
				int	suflen = com.strlen( cmap->type[i].suf );
				char	*suffix = &loadname[com.strlen(loadname)-suflen];

				// suffixes may have difference length, need fine check
				if( !com.strnicmp( suffix, cmap->type[i].suf, suflen ))
				{
					com.strncpy( path, loadname, com.strlen( loadname ) - suflen + 1 );
					FS_DefaultExtension( path, ".dds" );
					image.filter = cmap->type[i].hint;	// install side hint
					f = FS_LoadFile( path, &filesize );
					if( f && filesize > 0 )
					{
						// this name will be used only for tell user about problems 
						if( Image_LoadDDS( path, f, filesize ))
						{         
							Mem_Free(f);	// release buffer
							return ImagePack();	// loaded
						}
						else
						{
							Mem_Free(f);	// release buffer
							goto load_internal;	// cubemap test failed
						}
					}
				}
			}
		}
	} 

	// check all cubemap sides with package suffix
	for( cmap = load_cubemap; cmap && cmap->type; cmap++ )
	{
		for( i = 0; i < 6; i++ )
		{
			// for support mixed cubemaps e.g. sky_ft.jpg, sky_rt.tga, sky_bk.png
			// NOTE: all loaders must keep sides in one format for all
			for( format = image.loadformats; format && format->formatstring; format++ )
			{
				if( anyformat || !com_stricmp( ext, format->ext ))
				{
					com_sprintf( path, format->formatstring, loadname, cmap->type[i].suf, format->ext );
					image.hint = cmap->type[i].hint; // side hint
					f = FS_LoadFile( path, &filesize );
					if( f && filesize > 0 )
					{
						// this name will be used only for tell user about problems 
						if( format->loadfunc( path, f, filesize ))
						{         
							com_snprintf( sidename, MAX_STRING, "%s%s.%s", loadname, cmap->type[i].suf, format->ext );
							if( FS_AddSideToPack( sidename, cmap->type[i].flags )) // process flags to flip some sides
								break; // loaded
						}
						Mem_Free( f );
					}
				}
			}

			if( image.num_sides != i + 1 ) // check side
			{
				// first side not found, probably it's not cubemap
				// it contain info about image_type and dimensions, don't generate black cubemaps 
				if( !image.cubemap ) break;
				MsgDev( D_ERROR, "FS_LoadImage: couldn't load (%s%s.%s), create black image\n", loadname, cmap->type[i].suf );

				// Mem_Alloc already filled memblock with 0x00, no need to do it again
				image.cubemap = Mem_Realloc( Sys.imagepool, image.cubemap, image.ptr + image.size );
				image.ptr += image.size; // move to next
				image.num_sides++; // merge counter
			}
		}

		// make sure what all sides is loaded
		if( image.num_sides != 6 )
		{
			// unexpected errors ?
			if( image.cubemap ) Mem_Free( image.cubemap );
			Image_Reset();
		}
		else break; // all done
	}

load_internal:
	for( format = image.loadformats; format && format->formatstring; format++ )
	{
		if( anyformat || !com_stricmp( ext, format->ext ))
		{
			image.hint = format->hint;
			if( buffer && size > 0  )
			{
				if( format->loadfunc( loadname, buffer, size ))
					return ImagePack(); // loaded
			}
		}
	}

	if( !image.loadformats || image.loadformats->ext == NULL )
		MsgDev( D_NOTE, "FS_LoadImage: imagelib offline\n" );
	else if( filename[0] != '#' )
		MsgDev( D_WARN, "FS_LoadImage: couldn't load \"%s\"\n", loadname );

	return NULL;
}

/*
================
Image_Save

writes image as any known format
================
*/
bool FS_SaveImage( const char *filename, rgbdata_t *pix )
{
          const char	*ext = FS_FileExtension( filename );
	bool		anyformat = !com_stricmp(ext, "") ? true : false;
	string		path, savename;
	const saveformat_t	*format;

	if( !pix || !pix->buffer || anyformat ) return false;
	com.strncpy( savename, filename, sizeof( savename ));
	FS_StripExtension( savename ); // remove extension if needed

	// now try all the formats in the selected list
	for( format = image.saveformats; format && format->formatstring; format++ )
	{
		if( !com.stricmp( ext, format->ext ))
		{
			com.sprintf( path, format->formatstring, savename, "", format->ext );
			if( format->savefunc( path, pix )) return true; // saved
		}
	}
	return false;	
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
	else MsgDev( D_WARN, "FS_FreeImage: trying to free NULL image\n" );
}