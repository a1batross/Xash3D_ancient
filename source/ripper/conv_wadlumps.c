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
bool ConvSKN( const char *name, char *buffer, int filesize )
{
	rgbdata_t *pic = FS_LoadImage( "#internal.flt", buffer, filesize );
	string	savedname, skinname, wad;

	if( pic )
	{
		com.strncpy( savedname, name, MAX_STRING );
		FS_ExtractFilePath( name, wad );
		FS_StripExtension( savedname );
		FS_FileBase( savedname, skinname );

		FS_SaveImage( va("%s/sprites/%s.bmp", gs_gamedir, savedname ), pic );
		Skin_CreateScript( wad, skinname, pic );
		FS_FreeImage( pic ); // release buffer
		Msg("%s/%s.bmp\n", wad, skinname ); // echo to console about current skin
		return true;
	}
	return false;
}

/*
============
ConvFLT
============
*/
bool ConvFLT( const char *name, char *buffer, int filesize )
{
	rgbdata_t *pic = FS_LoadImage( "#internal.flt", buffer, filesize );
	string	savedname, tempname, path;

	if( pic )
	{
		com.strncpy( savedname, name, MAX_STRING );
		if( pic->flags & IMAGE_HAS_ALPHA )
		{
			FS_ExtractFilePath( savedname, path );
			FS_FileBase( savedname, tempname );
			com.snprintf( savedname, MAX_STRING, "%s/{%s", path, tempname );
		}
		else FS_StripExtension( savedname );

		FS_SaveImage(va("%s/textures/%s.bmp", gs_gamedir, savedname ), pic );
		Conv_CreateShader( savedname, pic, "flt", NULL, 0, 0 );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.bmp\n", savedname ); // echo to console about current texture
		return true;
	}
	return false;
}

/*
============
ConvFLP
============
*/
bool ConvFLP( const char *name, char *buffer, int filesize )
{
	rgbdata_t *pic = FS_LoadImage( "#internal.flt", buffer, filesize );

	if( pic )
	{
		FS_StripExtension( (char *)name );
		FS_SaveImage(va("%s/gfx/%s.bmp", gs_gamedir, name ), pic );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.bmp\n", name ); // echo to console about current pic
		return true;
	}
	return false;
}

/*
============
ConvMIP
============
*/
bool ConvMIP( const char *name, char *buffer, int filesize )
{
	rgbdata_t *pic;
	string	savedname, path;

	if(com.stristr( name, "gfx/conchars" )) // AGRHHHHHHH!!!!!!!!!!!!!!!!111
		pic = FS_LoadImage( "conchars", buffer, filesize );
	else if(com.stristr( name, "colormap" ))
		return true; // colormap not needs anywhere		 
	else if(com.stristr( name, "palette" ))
		return ConvPAL( name, buffer, filesize );
	else pic = FS_LoadImage( "#internal.mip", buffer, filesize );
	
	if( pic )
	{
		com.strncpy( savedname, name, MAX_STRING );
		FS_StripExtension( savedname );

		if(com.stristr( name, "gfx/conchars" ))
			com.snprintf( path, MAX_STRING, "%s/%s.bmp", gs_gamedir, savedname );
		else
		{
			com.snprintf( path, MAX_STRING, "%s/textures/%s.bmp", gs_gamedir, savedname );
			Conv_CreateShader( savedname, pic, "mip", NULL, 0, 0 ); // replace * with ! in shader too
		}
		FS_SaveImage( path, pic );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.bmp\n", savedname ); // echo to console about current pic
		return true;
	}
	return false;
}

/*
============
ConvLMP
============
*/
bool ConvLMP( const char *name, char *buffer, int filesize )
{
	rgbdata_t *pic;

	if(com.stristr( name, "colormap" ))
		return true;  // colormap not needs anywhere		 
	else if(com.stristr( name, "palette" ))
		return ConvPAL( name, buffer, filesize );
	else pic = FS_LoadImage( "#internal.lmp", buffer, filesize );

	if( pic )
	{
		FS_StripExtension( (char *)name );
		FS_SaveImage(va("%s/%s.bmp", gs_gamedir, name ), pic );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.bmp\n", name ); // echo to console about current pic
		return true;
	}
	return false;
}