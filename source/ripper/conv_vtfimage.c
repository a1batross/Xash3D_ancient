//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       conv_vtfimage.c - convert vtf materials
//=======================================================================

#include "ripper.h"
#include "qc_gen.h"

/*
=============
ConvVTF
=============
*/
bool ConvVTF( const char *name, char *buffer, int filesize )
{
	rgbdata_t	*pic;

	FS_StripExtension((char *)name );
	pic = FS_LoadImage( va( "#%s.vtf", name ), buffer, filesize );

	if( pic )
	{
		FS_SaveImage( va("%s/%s.tga", gs_gamedir, name ), PF_RGBA_32, pic ); // save converted image
		Conv_CreateShader( name, pic, "vtf", NULL, 0, 0 );
		FS_FreeImage( pic ); // release buffer
		Msg( "%s.vtf\n", name ); // echo to console about current pic
		return true;
	}
	return false;
}