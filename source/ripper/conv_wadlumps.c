//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       conv_wadlumps.c - convert wad lumps
//=======================================================================

#include "ripper.h"
#include "mathlib.h"
#include "qc_gen.h"

/*
============
ConvSKN
============
*/
bool ConvSKN( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t *pic = FS_LoadImage( "#internal.flt", buffer, filesize );
	string	savedname, skinname, wad;

	if( pic )
	{
		com.strncpy( savedname, name, MAX_STRING );
		FS_ExtractFilePath( name, wad );
		FS_StripExtension( savedname );
		FS_FileBase( savedname, skinname );

		FS_SaveImage( va("%s/sprites/%s.%s", gs_gamedir, savedname, ext ), pic );
		Skin_CreateScript( wad, skinname, pic );
		FS_FreeImage( pic ); // release buffer
		Msg("%s/%s.flat\n", wad, skinname ); // echo to console about current skin
		return true;
	}
	return false;
}

/*
============
ConvFLT
============
*/
bool ConvFLT( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t *pic = FS_LoadImage( "#internal.flt", buffer, filesize );
	string	savedname, tempname, path;

	if( pic )
	{
		com.strncpy( savedname, name, MAX_STRING );
		if( pic->flags & IMAGE_HAVE_ALPHA )
		{
			FS_ExtractFilePath( savedname, path );
			FS_FileBase( savedname, tempname );
			com.snprintf( savedname, MAX_STRING, "%s/{%s", path, tempname );
		}
		else FS_StripExtension( savedname );

		FS_SaveImage(va("%s/textures/%s.%s", gs_gamedir, savedname, ext ), pic );
		Conv_CreateShader( savedname, pic, "flt", NULL, 0, 0 );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.flat\n", savedname ); // echo to console about current texture
		return true;
	}
	return false;
}

/*
============
ConvFLP
============
*/
bool ConvFLP( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t *pic = FS_LoadImage( "#internal.flt", buffer, filesize );

	if( pic )
	{
		FS_StripExtension( (char *)name );
		FS_SaveImage(va("%s/gfx/%s.%s", gs_gamedir, name, ext ), pic );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.flat\n", name ); // echo to console about current pic
		return true;
	}
	return false;
}

/*
============
ConvMIP
============
*/
bool ConvMIP( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t *pic = FS_LoadImage( "#internal.mip", buffer, filesize );
	string	savedname, path;
	
	if( pic )
	{
		com.strncpy( savedname, name, MAX_STRING );
		FS_StripExtension( savedname );
		com.snprintf( path, MAX_STRING, "%s/textures/%s.%s", gs_gamedir, savedname, ext );
		Conv_CreateShader( savedname, pic, "mip", NULL, 0, 0 ); // replace * with ! in shader too
		FS_SaveImage( path, pic );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.mip\n", savedname ); // echo to console about current pic
		return true;
	}
	return false;
}

/*
============
ConvLMP
============
*/
bool ConvLMP( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t *pic = FS_LoadImage( va( "#%s.lmp", name ), buffer, filesize );

	if( pic )
	{
		FS_StripExtension( (char *)name );
		FS_SaveImage(va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.lmp\n", name ); // echo to console about current pic
		return true;
	}
	return false;
}