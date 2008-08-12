//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_png.c - png format load & save
//=======================================================================

#include <setjmp.h>
#include "launch.h"
#include "byteorder.h"
#include "filesystem.h"

#define PNG_LIBPNG_VER_STRING		"1.2.4"
#define PNG_COLOR_MASK_PALETTE	1
#define PNG_COLOR_MASK_COLOR		2
#define PNG_COLOR_MASK_ALPHA		4
#define PNG_COLOR_TYPE_GRAY		0
#define PNG_COLOR_TYPE_PALETTE	(PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_PALETTE)
#define PNG_COLOR_TYPE_RGB		(PNG_COLOR_MASK_COLOR)
#define PNG_COLOR_TYPE_RGB_ALPHA	(PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_ALPHA)
#define PNG_COLOR_TYPE_GRAY_ALPHA	(PNG_COLOR_MASK_ALPHA)
#define PNG_COLOR_TYPE_RGBA		PNG_COLOR_TYPE_RGB_ALPHA
#define PNG_COLOR_TYPE_GA		PNG_COLOR_TYPE_GRAY_ALPHA
#define PNG_INFO_tRNS 0x0010

extern void png_set_sig_bytes (void*, int);
extern int png_sig_cmp (const byte*, size_t, size_t);
extern void* png_create_read_struct (const char*, void*, void*, void*);
extern void* png_create_info_struct (void*);
extern void png_read_info (void*, void*);
extern void png_set_expand (void*);
extern void png_set_gray_1_2_4_to_8 (void*);
extern void png_set_palette_to_rgb (void*);
extern void png_set_tRNS_to_alpha (void*);
extern void png_set_gray_to_rgb	(void*);
extern void png_set_filler (void*, uint, int);
extern void png_read_update_info (void*, void*);
extern void png_read_image (void*, byte**);
extern void png_read_end (void*, void*);
extern void png_destroy_read_struct (void**, void**, void**);
extern void png_set_read_fn (void*, void*, void*);
extern uint png_get_valid (void*, void*, uint);
extern uint png_get_rowbytes (void*, void*);
extern byte png_get_channels (void*, void*);
extern byte png_get_bit_depth (void*, void*);
extern uint png_get_IHDR (void*, void*, uint*, uint*, int *, int *, int *, int *, int *);
extern char* png_get_libpng_ver (void*);
extern void png_set_strip_16 (void*);
extern void* png_create_write_struct (const char*, void*, void*, void*);
extern void png_set_IHDR (void*, void*, uint, uint, int, int, int, int, int);
extern void png_set_write_fn (void*, void*, void*, void* );
extern void png_write_image (void*, byte**);
extern void png_write_info (void*, void*);
extern void png_set_compression_level (void*, int);
extern void png_destroy_write_struct (void *, void*);
extern void png_init_io (void *, void* );
extern void png_write_end (void*, void* );

// this struct is only used for status information during loading
static struct
{
	const byte	*tmpBuf;
	file_t		*file;
	byte		ioBuffer[MAX_MSGLEN];
	int		tmpBuflength;
	int		tmpi;
	uint		FRowBytes;
	byte		**FRowPtrs;
	byte		*Data;
	int		BitDepth;
	int		bpp;
	int		color;
	uint		height;
	uint		width;
	int		il;		// interlace
	int		cmp;
	int		flt;
} png;

void png_fread( void *unused, byte *data, size_t length )
{
	size_t	l = png.tmpBuflength - png.tmpi;

	if( l < length )
	{
		MsgDev( D_WARN, "png_fread: overrun by %i bytes\n", length - l );
		// a read going past the end of the file, fill in the remaining bytes
		// with 0 just to be consistent
		memset( data + l, 0, length - l );
		length = l;
	}
	Mem_Copy( data, png.tmpBuf + png.tmpi, length );
	png.tmpi += (int)length;
}

void png_fwrite( void *file, byte *data, size_t length )
{
	FS_Write( png.file, data, length );
}

void png_error_fn( void *unused, const char *message )
{
	MsgDev( D_ERROR, "Image_LoadPNG: %s\n", message );
}

void png_warning_fn( void *unused, const char *message )
{
	MsgDev( D_WARN, "Image_LoadPNG: %s\n", message );
}

/*
=============
Image_LoadPNG
=============
*/
bool Image_LoadPNG( const char *name, const byte *buffer, size_t filesize )
{
	uint	y;
	void	*fin, *pnginfo;

	// FIXME: register an error handler so that abort() won't be called on error

	if(png_sig_cmp((char*)buffer, 0, filesize ))
		return false;
	fin = (void *)png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, (void *)png_error_fn, (void *)png_warning_fn );
	if( !fin ) return false;

	if(setjmp((int*)fin))
	{
		png_destroy_read_struct( &fin, &pnginfo, 0 );
		return false;
	}
          
	pnginfo = png_create_info_struct( fin );
	if( !pnginfo )
	{
		png_destroy_read_struct( &fin, &pnginfo, 0 );
		return false;
	}
	png_set_sig_bytes( fin, 0);

	memset(&png, 0, sizeof( png ));
	png.tmpBuf = buffer;
	png.tmpBuflength = filesize;
	png.tmpi = 0;
	png.color = PNG_COLOR_TYPE_RGB;
	png_set_read_fn( fin, png.ioBuffer, (void *)png_fread );
	png_read_info( fin, pnginfo );
	png_get_IHDR( fin, pnginfo, &png.width, &png.height,&png.BitDepth, &png.color, &png.il, &png.cmp, &png.flt );
	image_width = png.width;
	image_height = png.height;

	if(!Image_ValidSize( name ))
	{
		png_destroy_read_struct( &fin, &pnginfo, 0 );
		return false;
	}

	if( png.color == PNG_COLOR_TYPE_PALETTE )
	{
		png_set_palette_to_rgb( fin );
		png_set_filler( fin, 255, 1 );			
	}

	if( png.color == PNG_COLOR_TYPE_GRAY && png.BitDepth < 8 ) 
		png_set_gray_1_2_4_to_8( fin );
	
	if( png_get_valid( fin, pnginfo, PNG_INFO_tRNS ))
		png_set_tRNS_to_alpha( fin ); 
          
          if( png.color & PNG_COLOR_MASK_ALPHA )
		image_flags |= IMAGE_HAS_ALPHA;
	
	if( png.BitDepth >= 8 && png.color == PNG_COLOR_TYPE_RGB )	
		png_set_filler( fin, 255, 1 );
	
	if( png.color == PNG_COLOR_TYPE_GRAY || png.color == PNG_COLOR_TYPE_GRAY_ALPHA )
	{
		png_set_gray_to_rgb( fin );
		png_set_filler( fin, 255, 1 );			
	}

	if( png.BitDepth < 8 ) png_set_expand( fin );
	else if( png.BitDepth == 16 ) png_set_strip_16( fin );

	png_read_update_info( fin, pnginfo );
	png.FRowBytes = png_get_rowbytes( fin, pnginfo );
	png.bpp = png_get_channels( fin, pnginfo );
	png.FRowPtrs = (byte **)Mem_Alloc( Sys.imagepool, png.height * sizeof(*png.FRowPtrs));

	if( png.FRowPtrs )
	{
		png.Data = (byte *)Mem_Alloc( Sys.imagepool, png.height * png.FRowBytes );
		if( png.Data )
		{
			for( y = 0; y < png.height; y++ )
				png.FRowPtrs[y] = png.Data + y * png.FRowBytes;
			png_read_image( fin, png.FRowPtrs );
		}
		Mem_Free( png.FRowPtrs );
	}

	png_read_end( fin, pnginfo );
	png_destroy_read_struct( &fin, &pnginfo, 0 );

	image_size = png.height * png.width * 4;
	image_type = PF_RGBA_32;	
	image_width = png.width;
	image_height = png.height;
	image_rgba = png.Data;
	image_num_layers = 1;
	image_num_mips = 1;

	return true;
}

/*
=============
Image_SavePNG
=============
*/
bool Image_SavePNG( const char *name, rgbdata_t *pix, int saveformat )
{
	void	*fin;
	void	*info;
	byte	**row;
	int	i, pixel_size;
	
	if(FS_FileExists( name )) return false; // already existed

	// get image description
	switch( pix->type )
	{
	case PF_RGB_24: pixel_size = 3; break;
	case PF_RGBA_32: pixel_size = 4; break;	
	default:
		MsgDev( D_ERROR, "Image_SavePNG: unsupported image type %s\n", PFDesc[pix->type].name );
		return false;
	}

	png.file = FS_Open( name, "wb" );
	if( !png.file ) return false;

	if(!(fin = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL )))
	{
		FS_Close( png.file );
		return false;
	}

	if(!(info = png_create_info_struct( fin )))
	{
		png_destroy_write_struct( &fin, NULL );
		FS_Close( png.file );
		return false;
	}

	if(setjmp((int*)fin))
	{
		png_destroy_write_struct( &fin, &info );
		FS_Close( png.file );
		return false;
	}

	png_init_io( fin, png.file );
	png_set_write_fn( fin, png.ioBuffer, (void *)png_fwrite, NULL );
	png_set_compression_level( fin, 9 ); // Z_BEST_COMPRESSION
	png_set_IHDR( fin, info, pix->width, pix->height, 8, PNG_COLOR_TYPE_RGB, 0, 0, 0 );
	png_write_info( fin, info );

	row = Mem_Alloc( Sys.imagepool, pix->height * sizeof(byte*));
	for( i = 0; i < pix->height; i++ )
		row[i] = pix->buffer + i * pix->width * pixel_size;
	png_write_image( fin, row );
	png_write_end( fin, info );
	Mem_Free( row );
	png_destroy_write_struct( &fin, &info );
	FS_Close( png.file );

	return true;
}