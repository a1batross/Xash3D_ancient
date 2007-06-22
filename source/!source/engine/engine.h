//=======================================================================
//			Copyright XashXT Group 2007 ©
//			engine.h - engine.dll main header
//=======================================================================
#ifndef ENGINE_H
#define ENGINE_H

#include <assert.h>
#include <setjmp.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <direct.h>
#include <time.h>
#include <io.h>

//register new types
#include "basetypes.h"
#include "basemath.h"
#include "qfiles.h"
#include "cvar.h"
#include <ref_platform.h>
#include <ref_launcher.h>
#include <ref_renderer.h>
#include "bspmodel.h"
#include "const.h"
#include "common.h"

extern system_api_t gSysFuncs;
extern platform_api_t    pi;
extern byte *zonepool;
extern jmp_buf abortframe;

extern int host_debug;

int Sys_Milliseconds (void);

/*
===========================================
memory manager
===========================================
*/
//z_malloc-free
#define Z_Malloc(size) Mem_Alloc(zonepool, size)
#define Z_Free(data) Mem_Free(data)

//malloc-free
#define Mem_Alloc(pool,size) pi.MS_Alloc(pool, size, __FILE__, __LINE__)
#define Mem_Free(mem) pi.MS_Free(mem, __FILE__, __LINE__)

//Hunk_AllocName
#define Mem_AllocPool(name) pi.MS_AllocPool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) pi.MS_FreePool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) pi.MS_EmptyPool(pool, __FILE__, __LINE__)

/*
===========================================
filesystem manager
===========================================
*/
#define FS_LoadFile(name, size) pi.FS_LoadFile(name, size)
#define FS_LoadImage(name, width, height) pi.FS_LoadFile(name, width, height)
#define FS_Search(path) pi.FS_Search( path, true, false )
#define FS_WriteFile(name, data, size) pi.FS_WriteFile(name, data, size )
#define FS_Open( path, mode ) pi.FS_Open( path, mode, true, false )
#define FS_Read( file, buffer, size ) pi.FS_Read( file, buffer, size )
#define FS_Write( file, buffer, size ) pi.FS_Write( file, buffer, size )
#define FS_StripExtension( path ) pi.FS_StripExtension( path )
#define FS_DefaultExtension( path, ext ) pi.FS_DefaultExtension( path, ext )
#define FS_FileExists( file ) pi.FS_FileExists( file )
#define FS_Close( file ) pi.FS_Close( file )
#define FS_FileBase( x, y ) pi.FS_FileBase( x, y )
#define FS_Printf pi.FS_Printf
#define FS_Seek pi.FS_Seek
#define FS_Tell pi.FS_Tell
#define FS_Getc pi.FS_Getc
#define FS_UnGetc pi.FS_UnGetc
char *FS_Gamedir( void );

/*
===========================================
System Timer
===========================================
*/
#define Sys_DoubleTime pi.DoubleTime
#define GI pi.GameInfo()

/*
===========================================
System Events
===========================================
*/

#define Msg Com_Printf
#define MsgDev Com_DPrintf
#define WinError gSysFuncs.sys_err

/*
===========================================
Host Interface
===========================================
*/
void Host_Init ( char *funcname, int argc, char **argv );
void Host_Main ( void );
void Host_Free ( void );

#endif//ENGINE_H