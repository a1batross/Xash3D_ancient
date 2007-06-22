//=======================================================================
//			Copyright XashXT Group 2007 ©
//			image.h - tga, pcx image headers
//=======================================================================

#ifndef IMGLIB_H
#define IMGLIB_H

#include "platform.h"

byte *LoadBMP( char *name, char *buffer, int filesize, int *width, int *height );
byte *LoadPCX( char *name, char *buffer, int filesize, int *width, int *height );
byte *LoadTGA( char *name, char *buffer, int filesize, int *width, int *height );
byte *LoadPNG( char *name, char *buffer, int filesize, int *width, int *height );
byte *LoadJPG( char *name, char *buffer, int filesize, int *width, int *height );

#endif//IMGLIB_H