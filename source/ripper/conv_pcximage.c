//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       conv_pcximage.c - convert pcximages
//=======================================================================

#include "ripper.h"
#include "qc_gen.h"

// this also uses by SP2_ConvertFrame
bool ConvPCX( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t	*pic = FS_LoadImage( "#internal.pcx", buffer, filesize );

	if( pic )
	{
		FS_StripExtension((char *)name );
		FS_SaveImage( va("%s/%s.%s", gs_gamedir, name, ext ), pic ); // save converted image
		FS_FreeImage( pic ); // release buffer
		Msg( "%s.pcx\n", name ); // echo to console about current pic
		return true;
	}
	return false;
}