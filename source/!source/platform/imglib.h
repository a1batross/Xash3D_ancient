//=======================================================================
//			Copyright XashXT Group 2007 ©
//			image.h - tga, pcx image headers
//=======================================================================

#ifndef IMGLIB_H
#define IMGLIB_H

#include "platform.h"

bool LoadBMP( char *name, char *buffer, int filesize );
bool LoadPCX( char *name, char *buffer, int filesize );
bool LoadTGA( char *name, char *buffer, int filesize );
bool LoadDDS( char *name, char *buffer, int filesize );
bool LoadPNG( char *name, char *buffer, int filesize );
bool LoadJPG( char *name, char *buffer, int filesize );

#endif//IMGLIB_H