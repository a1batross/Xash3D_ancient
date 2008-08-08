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

	if( pic )
	{
		string	skinpath;
		FS_FileBase( name, skinpath );
		FS_SaveImage(va("%s/sprites/%s.bmp", gs_gamedir, skinpath ), pic );
		Skin_CreateScript( skinpath, pic );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.skin\n", skinpath ); // echo to console about current skin
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

	if( pic )
	{
		FS_StripExtension( (char *)name );
		FS_SaveImage(va("%s/textures/%s.bmp", gs_gamedir, name ), pic );
		Conv_CreateShader( name, pic, "flt", NULL, 0, 0 );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.flat\n", name ); // echo to console about current texture
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
	string	savedname;

	if( pic )
	{
		FS_FileBase( name, savedname );
		FS_SaveImage(va("%s/gfx/%s.bmp", gs_gamedir, savedname ), pic );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.flmp\n", savedname ); // echo to console about current pic
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
	rgbdata_t *pic = FS_LoadImage( "#internal.mip", buffer, filesize );
	string	savedname, path;
	int	k, l;

	if( pic )
	{
		FS_FileBase( name, savedname );
		k = com.strlen( com.strchr( savedname, '*' ));
		l = com.strlen( savedname );
		if( k ) savedname[l-k] = '!'; // quake1 issues

		if(!com.stricmp( savedname, "gfx/conchars" ))
			com.snprintf( path, MAX_STRING, "%s/gfx/%s.bmp", gs_gamedir, savedname );
		else com.snprintf( path, MAX_STRING, "%s/textures/%s.bmp", gs_gamedir, savedname );
		FS_SaveImage( path, pic );
		Conv_CreateShader( name, pic, "mip", NULL, 0, 0 );
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
bool ConvLMP( const char *name, char *buffer, int filesize )
{
	rgbdata_t *pic = FS_LoadImage( "#internal.lmp", buffer, filesize );
	string	savedname;

	if( pic )
	{
		FS_FileBase( name, savedname );
		FS_SaveImage(va("%s/gfx/%s.bmp", gs_gamedir, savedname ), pic );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.lmp\n", savedname ); // echo to console about current pic
		return true;
	}
	return false;
}