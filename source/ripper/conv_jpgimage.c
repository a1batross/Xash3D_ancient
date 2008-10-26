//=======================================================================
//			Copyright XashXT Group 2007 ©
//		conv_shader.c - analyze and write texture shader
//=======================================================================

#include "ripper.h"
#include "qc_gen.h"

/*
=============
ConvJPG
=============
*/
bool ConvJPG( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t	*pic = FS_LoadImage( "#internal.jpg", buffer, filesize );

	if( pic )
	{
		FS_StripExtension((char *)name );
		FS_SaveImage( va("%s/%s.%s", gs_gamedir, name, ext ), pic ); // save converted image
		Conv_CreateShader( name, pic, "jpg", NULL, 0, 0 );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.jpg\n", name, ext ); // echo to console about current pic
		return true;
	}
	return false;
}