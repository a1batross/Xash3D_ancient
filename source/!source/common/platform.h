//=======================================================================
//			Copyright XashXT Group 2007 ©
//			platform.h - game platform dll
//=======================================================================
#ifndef BASEPLATFORM_H
#define BASEPLATFORM_H

#include <stdio.h>
#include <setjmp.h>
#include <windows.h>
#include <basetypes.h>
#include "image.h"
#include <ref_system.h>

extern byte *basepool;
extern byte *zonepool;

//=====================================
//	platform export
//=====================================
bool InitPlatform ( int argc, char **argv );
void ClosePlatform ( void );

compilers_api_t Comp_GetAPI( void );
infostring_api_t Info_GetAPI( void );

//=====================================
//	filesystem funcs
//=====================================
rgbdata_t *FS_LoadImage(const char *filename, char *data, int size );
void FS_SaveImage(const char *filename, rgbdata_t *buffer );
void FS_FreeImage( rgbdata_t *pack );

#endif//BASEPLATFORM_H