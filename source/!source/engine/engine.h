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
#include <ref_system.h>
#include "vprogs.h"
#include "bspmodel.h"
#include "const.h"
#include "common.h"
#include "cvar.h"

extern stdinout_api_t	std;
extern platform_exp_t	*pi;
extern byte		*zonepool;
extern jmp_buf		abortframe;

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
#define Mem_Alloc(pool,size) pi->Mem.Alloc(pool, size, __FILE__, __LINE__)
#define Mem_Free(mem) pi->Mem.Free(mem, __FILE__, __LINE__)

//Hunk_AllocName
#define Mem_AllocPool(name) pi->Mem.AllocPool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) pi->Mem.FreePool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) pi->Mem.EmptyPool(pool, __FILE__, __LINE__)

#define Mem_Copy(dest, src, size) pi->Mem.Copy(dest, src, size, __FILE__, __LINE__) 

/*
===========================================
filesystem manager
===========================================
*/
#define FS_LoadFile(name, size) pi->Fs.LoadFile(name, size)
#define FS_LoadImage(name, data, size) pi->Fs.LoadImage(name, data, size)
#define FS_Search(path) pi->Fs.Search( path, true )
#define FS_WriteFile(name, data, size) pi->Fs.WriteFile(name, data, size )
#define FS_Open( path, mode ) pi->Fs.Open( path, mode )
#define FS_Read( file, buffer, size ) pi->Fs.Read( file, buffer, size )
#define FS_Write( file, buffer, size ) pi->Fs.Write( file, buffer, size )
#define FS_StripExtension( path ) pi->Fs.StripExtension( path )
#define FS_DefaultExtension( path, ext ) pi->Fs.DefaultExtension( path, ext )
#define FS_FileExtension( ext ) pi->Fs.FileExtension( ext )
#define FS_FileExists( file ) pi->Fs.FileExists( file )
#define FS_Close( file ) pi->Fs.Close( file )
#define FS_FileBase( x, y ) pi->Fs.FileBase( x, y )
#define FS_Find( x ) pi->Fs.Search( x, false )
#define FS_Printf pi->Fs.Printf
#define FS_Print pi->Fs.Print
#define FS_Seek pi->Fs.Seek
#define FS_Tell pi->Fs.Tell
#define FS_Gets pi->Fs.Gets
char *FS_Gamedir( void );

/*
===========================================
scriptsystem manager
===========================================
*/
#define COM_Parse(data) pi->Script.ParseToken(data)
#define COM_Token pi->Script.Token

/*
===========================================
infostring manager
===========================================
*/
#define Info_Print(x) pi->Info.Print
#define Info_Validate(x) pi->Info.Validate(x)
#define Info_RemoveKey(x, y) pi->Info.RemoveKey(x,y)
#define Info_ValueForKey(x,y) pi->Info.ValueForKey(x,y)
#define Info_SetValueForKey(x,y,z) pi->Info.SetValueForKey(x,y,z)

/*
===========================================
System Timer
===========================================
*/
#define Sys_DoubleTime pi->DoubleTime
#define GI pi->GameInfo()

/*
===========================================
System Events
===========================================
*/

#define Msg Com_Printf
#define MsgDev Com_DPrintf
#define MsgWarn Com_DWarnf
void Sys_Error( char *msg, ... );

/*
===========================================
Host Interface
===========================================
*/
void Host_Init ( char *funcname, int argc, char **argv );
void Host_Main ( void );
void Host_Free ( void );

//
// in_win.c
//
extern int mouse_x, mouse_y, old_mouse_x, old_mouse_y, mx_accum, my_accum;


#endif//ENGINE_H