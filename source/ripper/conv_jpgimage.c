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
bool ConvJPG(const char *name, char *buffer, int filesize )
{
	rgbdata_t	*pic = FS_LoadImage( "#internal.jpg", buffer, filesize );

	if( pic )
	{
		FS_StripExtension((char *)name );
		FS_SaveImage( va("%s/%s.tga", gs_gamedir, name ), pic ); // save converted image
		Conv_CreateShader( name, pic, "jpg", NULL, 0, 0 );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.pcx\n", name ); // echo to console about current pic
		return true;
	}
	return false;
}