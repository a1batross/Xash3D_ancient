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
#include <ref_system.h>
#include <ref_launcher.h>

//=====================================
//	platform export
//=====================================
void InitPlatform ( void );
void ClosePlatform ( void );
void Plat_InitCPU( void );
double Plat_DoubleTime( void );

filesystem_api_t FS_GetAPI( void );
memsystem_api_t Mem_GetAPI( void );
vfilesystem_api_t VFS_GetAPI( void );
scriptsystem_api_t Sc_GetAPI( void );
compilers_api_t Comp_GetAPI( void );

//=====================================
//	filesystem funcs
//=====================================
byte *FS_LoadFile (const char *path, fs_offset_t *filesizeptr );
rgbdata_t *FS_LoadImage(const char *filename, char *data, int size );
bool FS_WriteFile (const char *filename, void *data, fs_offset_t len);
search_t *FS_Search(const char *pattern, int caseinsensitive );
search_t *FS_SearchDirs(const char *pattern, int caseinsensitive );
void FS_FreeImage( rgbdata_t *pack );

//=====================================
//	memory manager funcs
//=====================================
void *_Mem_Alloc(byte *pool, size_t size, const char *filename, int fileline);
void *_Mem_Realloc(byte *poolptr, void *memptr, size_t size, const char *filename, int fileline);
void _Mem_Free(void *data, const char *filename, int fileline);
byte *_Mem_AllocPool(const char *name, const char *filename, int fileline);
void _Mem_FreePool(byte **pool, const char *filename, int fileline);
void _Mem_EmptyPool(byte *pool, const char *filename, int fileline);
void _Mem_Move (void *dest, void *src, size_t size, const char *filename, int fileline);
void _Mem_Copy (void *dest, void *src, size_t size, const char *filename, int fileline);

//=====================================
//	parsing manager funcs
//=====================================
bool FS_LoadScript( const char *name, char *buf, int size );
bool FS_AddScript( const char *name, char *buf, int size );
bool SC_MatchToken( const char *match );//match token
char *SC_GetToken( bool newline );	//unsafe way
bool SC_TryToken ( void );		//safe way
void SC_SkipToken( void );
void SC_FreeToken( void );
char *SC_ParseToken(const char **data_p);

#endif//BASEPLATFORM_H