//=======================================================================
//			Copyright XashXT Group 2007 ©
//		     conv_image.c - convert various image type
//=======================================================================

#include "ripper.h"
#include "qc_gen.h"

/*
=============
ConvJPG

decompress jpeg to tga 32 bit
=============
*/
bool ConvJPG( const char *name, char *buffer, int filesize )
{
	rgbdata_t	*pic = FS_LoadImage( "#internal.jpg", buffer, filesize );

	if( pic )
	{
		FS_StripExtension((char *)name );
		FS_SaveImage( va("%s/%s.tga", gs_gamedir, name ), pic ); // save converted image
		Conv_CreateShader( name, pic, "jpg", NULL, 0, 0 );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.jpg\n", name ); // echo to console about current pic
		return true;
	}
	return false;
}

/*
=============
ConvPCX

decompress pcx to bmp 8 bit indexed (runlength decompress)
=============
*/
bool ConvPCX( const char *name, char *buffer, int filesize )
{
	rgbdata_t	*pic = FS_LoadImage( "#internal.pcx", buffer, filesize );

	if( pic )
	{
		FS_StripExtension((char *)name );
		FS_SaveImage( va("%s/%s.bmp", gs_gamedir, name ), pic ); // save converted image
		FS_FreeImage( pic ); // release buffer
		Msg("%s.pcx\n", name ); // echo to console about current pic
		return true;
	}
	return false;
}

/*
============
ConvPAL

get palette from wad (just in case, not really needed)
============
*/
bool ConvPAL( const char *name, char *buffer, int filesize )
{
	rgbdata_t	*pic = FS_LoadImage( "#internal.pal", buffer, filesize );

	if( pic && pic->palette )
	{
		FS_StripExtension((char *)name );
		FS_WriteFile( va("%s.pal", name ), pic->palette, 768 ); // expands to 32bit
		FS_FreeImage( pic ); // release buffer
		Msg("%s.pal\n", name ); // echo to console about current pic
		return true;
	}
	return false;
}

/*
============
ConvDAT

decompile progs.dat
============
*/
bool ConvDAT( const char *name, char *buffer, int filesize )
{
	return PRVM->DecompileDAT( name );
}