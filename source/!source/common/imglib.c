//=======================================================================
//			Copyright XashXT Group 2007 ©
//			imagelib.c - convert textures
//=======================================================================

#include "platform.h"
#include "utils.h"

bool ConvertImagePixels ( byte *mempool, const char *name, byte parms )
{
	rgbdata_t *image = FS_LoadImage( name, NULL, 0 );
	char	savename[MAX_QPATH];
	int	w, h;

	if(!image || !image->buffer) return false;

	com.strncpy( savename, name, sizeof(savename)-1);
	FS_StripExtension( savename ); // remove extension if needed
	FS_DefaultExtension( savename, ".tga" );// set new extension
	w = image->width, h = image->height; 

	if(FS_CheckParm("-resample"))
		Image_Processing( name, &image );

	Msg("%s\n", name );
	FS_SaveImage( savename, image );// save as TGA
	FS_FreeImage( image );

	return true;
}