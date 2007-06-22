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
#include "basemem.h"
#include <ref_platform.h>
#include <ref_launcher.h>

//=====================================
//	main platform funcs
//=====================================
void InitPlatform ( char *funcname, int argc, char **argv );
void PlatformFrame ( void );
void ClosePlatform ( void );
double Plat_DoubleTime( void );

//=====================================
//	filesystem funcs
//=====================================
byte *FS_LoadFile (const char *path, fs_offset_t *filesizeptr );
byte *FS_LoadImage (char *filename, int *width, int *height);
byte *FS_LoadImageData (char *filename, char *buffer, int size, int *width, int *height);
bool FS_WriteFile (const char *filename, void *data, fs_offset_t len);
search_t *FS_Search(const char *pattern, int caseinsensitive, int quiet);

//=====================================
//	memory manager funcs
//=====================================
void *_Mem_Alloc(byte *pool, size_t size, const char *filename, int fileline);
void *_Mem_Realloc(byte *poolptr, void *memptr, size_t size, const char *filename, int fileline);
void _Mem_Free(void *data, const char *filename, int fileline);
byte *_Mem_AllocPool(const char *name, const char *filename, int fileline);
void _Mem_FreePool(byte **pool, const char *filename, int fileline);
void _Mem_EmptyPool(byte *pool, const char *filename, int fileline);


//=====================================
//	parsing manager funcs
//=====================================
bool FS_LoadScript( char *filename );
bool MS_LoadScript( char *buf, int size );
bool FS_AddScript( char *filename );
bool SC_MatchToken( char *match );	//match token
char *SC_GetToken( bool newline );	//unsafe way
bool SC_TryToken ( void );		//safe way
void SC_SkipToken( void );
char *SC_ParseToken( char *data );
char *SC_ParseWord( char *data );

#endif//BASEPLATFORM_H