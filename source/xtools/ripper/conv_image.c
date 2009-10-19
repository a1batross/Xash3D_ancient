//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     conv_image.c - convert various image type
//=======================================================================

#include "ripper.h"

/*
========================================================================

.WAL image format	(Wally textures)

========================================================================
*/
typedef struct wal_s
{
	char	name[32];
	uint	width, height;
	uint	offsets[4];	// four mip maps stored
	char	animname[32];	// next frame in animation chain
	int	flags;
	int	contents;
	int	value;
} wal_t;

/*
============
ConvWAL
============
*/
bool ConvWAL( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t	*pic = FS_LoadImage( va( "#%s.wal", name ), buffer, filesize );

	if( pic )
	{
		wal_t *wal = (wal_t *)buffer;
		FS_SaveImage( va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		Conv_CreateShader( name, pic, ext, wal->animname, wal->flags, wal->contents );
		Msg("%s.wal\n", name ); // echo to console
		FS_FreeImage( pic );
		return true;
	}
	return false;
}

/*
=============
ConvJPG
=============
*/
bool ConvJPG( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t	*pic = FS_LoadImage( va( "#%s.jpg", name ), buffer, filesize );

	if( pic )
	{
		FS_SaveImage( va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		Conv_CreateShader( name, pic, ext, NULL, 0, 0 );
		Msg( "%s.jpg\n", name, ext ); // echo to console
		FS_FreeImage( pic );
		return true;
	}
	return false;
}

/*
=============
ConvBMP
=============
*/
bool ConvBMP( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t	*pic = FS_LoadImage( va( "#%s.bmp", name ), buffer, filesize );

	if( pic )
	{
		FS_SaveImage( va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		Conv_CreateShader( name, pic, ext, NULL, 0, 0 );
		Msg( "%s.bmp\n", name, ext ); // echo to console
		FS_FreeImage( pic );
		return true;
	}
	return false;
}

/*
=============
ConvPCX

this also uses by SP2_ConvertFrame
=============
*/
bool ConvPCX( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t	*pic = FS_LoadImage( va( "#%s.pcx", name ), buffer, filesize );

	if( pic )
	{
		FS_SaveImage( va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		// pcx images not required shader because it hud pics or sprite frames
		Msg( "%s.pcx\n", name ); // echo to console
		FS_FreeImage( pic );
		return true;
	}
	return false;
}

/*
=============
ConvVTF
=============
*/
bool ConvVTF( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t	*pic = FS_LoadImage( va( "#%s.vtf", name ), buffer, filesize );

	if( pic )
	{
		FS_SaveImage( va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		Conv_CreateShader( name, pic, ext, NULL, 0, 0 );
		Msg( "%s.vtf\n", name ); // echo to console
		FS_FreeImage( pic );
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
	rgbdata_t *pic = FS_LoadImage( va( "#%s.mip", name ), buffer, filesize );
	
	if( pic )
	{
		FS_SaveImage( va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		Conv_CreateShader( name, pic, ext, NULL, 0, 0 );
		Msg( "%s.mip\n", name ); // echo to console
		FS_FreeImage( pic );
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
		FS_SaveImage(va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		Msg("%s.lmp\n", name ); // echo to console
		FS_FreeImage( pic );
		return true;
	}
	return false;
}

/*
============
ConvFNT
============
*/
bool ConvFNT( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t *pic = FS_LoadImage( va( "#%s.fnt", name ), buffer, filesize );

	if( pic )
	{
		FS_SaveImage(va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		Msg("%s.fnt\n", name ); // echo to console
		FS_FreeImage( pic );
		return true;
	}
	return false;
}

/*
============
ConvRAW

write to disk without conversions
============
*/
bool ConvRAW( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	if( FS_WriteFile( va("%s/%s.%s", gs_gamedir, name, ext ), buffer, filesize ))
	{
		Msg( "%s.%s\n", name, ext ); // echo to console
		return true;
	}
	return false;
}