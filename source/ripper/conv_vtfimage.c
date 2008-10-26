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
bool ConvVTF( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t	*pic;

	FS_StripExtension((char *)name );
	pic = FS_LoadImage( va( "#%s.vtf", name ), buffer, filesize );

	if( pic )
	{
		// NOTE: save vtf textures into dds for speedup reson
		// also avoid unneeded convertations DXT->RGBA->DXT
		FS_SaveImage( va("%s/%s.%s", gs_gamedir, name, ext ), pic ); // save converted image
		Conv_CreateShader( name, pic, "vtf", NULL, 0, 0 );
		FS_FreeImage( pic ); // release buffer
		Msg( "%s.vtf\n", name ); // echo to console about current pic
		return true;
	}
	return false;
}