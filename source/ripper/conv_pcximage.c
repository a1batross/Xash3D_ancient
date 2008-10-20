//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       conv_pcximage.c - convert pcximages
//=======================================================================

#include "ripper.h"
#include "qc_gen.h"

// this also uses by SP2_ConvertFrame
bool PCX_ConvertImage( const char *name, char *buffer, int filesize )
{
	rgbdata_t	*pic = FS_LoadImage( "#internal.pcx", buffer, filesize );

	if( pic )
	{
		FS_StripExtension((char *)name );
		FS_SaveImage( va("%s/%s.bmp", gs_gamedir, name ), PF_INDEXED_32, pic ); // save converted image
		FS_FreeImage( pic ); // release buffer
		Msg( "%s.pcx\n", name ); // echo to console about current pic
		return true;
	}
	return false;
}

/*
============
ConvPCX
============
*/
bool ConvPCX( const char *name, char *buffer, int filesize )
{
	return PCX_ConvertImage( name, buffer, filesize );
}