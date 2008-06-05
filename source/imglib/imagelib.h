//=======================================================================
//			Copyright XashXT Group 2007 ©
//		     imagelib.h - image processing library
//=======================================================================
#ifndef IMAGELIB_H
#define IMAGELIB_H

#include <windows.h>
#include "basetypes.h"
#include "stdapi.h"
#include "stdref.h"
#include "basefiles.h"
#include "dllapi.h"

#define IMAGE_MAXWIDTH	4096
#define IMAGE_MAXHEIGHT	4096

extern stdlib_api_t com;
extern byte *zonepool;
extern int app_name;
#define Sys_Error com.error
extern cvar_t *img_resample_lerp;

extern int image_width, image_height;
extern byte image_num_layers;	// num layers in
extern byte image_num_mips;	// build mipmaps
extern uint image_type;	// main type switcher
extern uint image_flags;	// additional image flags
extern byte image_bits_count;	// bits per RGBA
extern size_t image_size;	// image rgba size
extern byte *image_rgba;	// image pointer (see image_type for details)
extern byte *image_palette;	// palette pointer

bool Image_AddMipmapToPack( const byte *in, int width, int height );
void Image_RoundDimensions( int *scaled_width, int *scaled_height );
byte *Image_ResampleInternal( const void *indata, int inwidth, int inheight, int outwidth, int outheight, int intype );
bool Image_Resample( const char *name, rgbdata_t **image, int width, int height, bool free_baseimage );
void Image_FreeImage( rgbdata_t *pack );
void Image_Save( const char *filename, rgbdata_t *pix );
rgbdata_t *Image_Load(const char *filename, char *buffer, int buffsize );

//
// formats
//
bool Image_LoadTGA( const char *name, byte *buffer, size_t filesize );
bool Image_SaveTGA( const char *name, rgbdata_t *pix, int saveformat);
bool Image_LoadDDS( const char *name, byte *buffer, size_t filesize );
bool Image_SaveDDS( const char *name, rgbdata_t *pix, int saveformat);

//
// img_utils.c
//
bool Image_ValidSize( const char *name );
bool Image_DecompressDXTC( rgbdata_t **image );	// compilers version of decompressor
bool Image_DecompressARGB( rgbdata_t **image );

#endif//IMAGELIB_H