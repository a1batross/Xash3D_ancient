//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_utils.c - misc common utils
//=======================================================================

#include "imagelib.h"

//=======================================================================
//			IMGLIB COMMON TOOLS
//=======================================================================
bool Image_ValidSize( const char *name )
{
	if( image_width > IMAGE_MAXWIDTH || image_height > IMAGE_MAXHEIGHT || image_width <= 0 || image_height <= 0 )
	{
		MsgDev( D_WARN, "Image_ValidSize: (%s) dimensions out of range [%dx%d]\n",name, image_width, image_height );
		return false;
	}
	return true;
}
