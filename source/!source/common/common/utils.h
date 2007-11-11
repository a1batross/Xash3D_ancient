//=======================================================================
//			Copyright XashXT Group 2007 ©
//			utils.h - shared engine utility
//=======================================================================
#ifndef UTILS_H
#define UTILS_H

#include <time.h>

#define ALIGN( a ) a = (byte *)((int)((byte *)a + 3) & ~ 3)

extern unsigned __int64 __g_ProfilerStart;
extern unsigned __int64 __g_ProfilerEnd;
extern unsigned __int64 __g_ProfilerEnd2;
extern unsigned __int64 __g_ProfilerSpare;
extern unsigned __int64 __g_ProfilerTotalTicks;
extern double __g_ProfilerTotalMsec;
extern int com_argc;
extern char **com_argv;

#define Profile_Start()\
{\
if (std.GameInfo->rdtsc) \
{ \
__asm pushad \
__asm rdtsc \
__asm mov DWORD PTR[__g_ProfilerStart+4], edx \
__asm mov DWORD PTR[__g_ProfilerStart], eax \
__asm popad \
} \
}

#define Profile_End()\
{\
if (GI.rdtsc) \
{ \
__asm pushad \
__asm rdtsc \
__asm mov DWORD PTR[__g_ProfilerEnd+4], edx \
__asm mov DWORD PTR[__g_ProfilerEnd], eax \
__asm popad \
__asm pushad \
__asm rdtsc \
__asm mov DWORD PTR[__g_ProfilerEnd2+4], edx \
__asm mov DWORD PTR[__g_ProfilerEnd2], eax \
__asm popad \
} \
}

void Profile_RatioResults( void );
void _Profile_Results( const char *function );
void Profile_Store( void );
void Profile_Time( void );	// total profile time
#define Profile_Results( name )  _Profile_Results( #name )

#define numthreads std.get_numthreads()
#define ThreadLock std.thread_lock
#define ThreadUnlock std.thread_unlock
#define RunThreadsOnIndividual std.create_thread

//=====================================
//	memory manager funcs
//=====================================
#define Mem_Alloc(pool, size) std.malloc(pool, size, __FILE__, __LINE__)
#define Mem_Realloc(pool, ptr, size) std.realloc(pool, ptr, size, __FILE__, __LINE__)
#define Mem_Move(pool, ptr, data, size) std.move(pool, ptr, data, size, __FILE__, __LINE__)
#define Mem_Free(mem) std.free(mem, __FILE__, __LINE__)
#define Mem_AllocPool(name) std.mallocpool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) std.freepool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) std.clearpool(pool, __FILE__, __LINE__)
#define Mem_Copy(dest, src, size ) std.memcpy(dest, src, size, __FILE__, __LINE__)

//=====================================
//	parsing manager funcs
//=====================================
#define SC_ParseToken std.Script.ParseToken
#define SC_ParseWord std.Script.ParseWord
#define SC_Token() std.Script.Token
#define SC_Filter std.Script.FilterToken
#define FS_LoadScript std.Script.Load
#define FS_AddScript std.Script.Include
#define FS_ResetScript std.Script.Reset
#define SC_GetToken std.Script.GetToken
#define SC_TryToken std.Script.TryToken
#define SC_FreeToken std.Script.FreeToken
#define SC_SkipToken std.Script.SkipToken
#define SC_MatchToken std.Script.MatchToken
#define g_TXcommand std.Script.g_TXcommand

/*
===========================================
filesystem manager
===========================================
*/
#define FS_AddGameHierarchy std.AddGameHierarchy
#define FS_LoadGameInfo std.LoadGameInfo
#define FS_InitRootDir std.InitRootDir
#define FS_LoadFile(name, size) std.Fs.LoadFile(name, size)
#define FS_Search std.Fs.Search
#define FS_WriteFile(name, data, size) std.Fs.WriteFile(name, data, size )
#define FS_Open( path, mode ) std.Fs.Open( path, mode )
#define FS_Read( file, buffer, size ) std.Fs.Read( file, buffer, size )
#define FS_Write( file, buffer, size ) std.Fs.Write( file, buffer, size )
#define FS_StripExtension( path ) std.Fs.StripExtension( path )
#define FS_DefaultExtension( path, ext ) std.Fs.DefaultExtension( path, ext )
#define FS_FileExtension( ext ) std.Fs.FileExtension( ext )
#define FS_FileExists( file ) std.Fs.FileExists( file )
#define FS_Close( file ) std.Fs.Close( file )
#define FS_FileBase( x, y ) std.Fs.FileBase( x, y )
#define FS_Find( x ) std.Fs.Search( x, false )
#define FS_Printf std.Fs.Printf
#define FS_Print std.Fs.Print
#define FS_Seek std.Fs.Seek
#define FS_Tell std.Fs.Tell
#define FS_Gets std.Fs.Gets
#define FS_Gamedir std.GameInfo->gamedir
#define FS_ClearSearchPath std.Fs.ClearSearchPath
int FS_CheckParm (const char *parm);
bool FS_GetParmFromCmdLine( char *parm, char *out );
rgbdata_t *FS_LoadImage(const char *filename, char *buffer, int buffsize );
void FS_SaveImage(const char *filename, rgbdata_t *pix );
void FS_FreeImage( rgbdata_t *pack );


// virtual files managment
#define VFS_Open	std.VFs.Open
#define VFS_Write	std.VFs.Write
#define VFS_Read	std.VFs.Read
#define VFS_Seek	std.VFs.Seek
#define VFS_Tell	std.VFs.Tell
#define VFS_Close	std.VFs.Close

// crc stuff
#define CRC_Init		std.crc_init
#define CRC_Block		std.crc_block
#define CRC_ProcessByte	std.crc_process

#define Sys_DoubleTime	std.gettime

extern stdlib_api_t std;

#define Msg std.printf
#define MsgDev std.dprintf
#define MsgWarn std.wprintf
#define Sys_Error std.error
#define Sys_LoadLibrary std.LoadLibrary
#define Sys_FreeLibrary std.FreeLibrary

#define Malloc(size)	Mem_Alloc(basepool, size)  
#define Z_Malloc(size)	Mem_Alloc(zonepool, size)  
#define Free(mem)	 	Mem_Free(mem) 

extern char gs_mapname[ 64 ];
extern char gs_basedir[ MAX_SYSPATH ];
extern bool host_debug;

extern byte *qccpool;
extern byte *studiopool;

// misc common functions
#define copystring		std.stralloc
#define strlower		std.strlwr
#define va		std.va
#define stristr		std.stristr

byte *ReadBMP (char *filename, byte **palette, int *width, int *height);

// misc
bool CompileStudioModel ( byte *mempool, const char *name, byte parms );
bool CompileSpriteModel ( byte *mempool, const char *name, byte parms );
bool ConvertImagePixels ( byte *mempool, const char *name, byte parms );
bool PrepareBSPModel ( const char *dir, const char *name, byte params );
bool CompileBSPModel ( void );
bool PrepareROQVideo ( const char *dir, const char *name, byte params );
bool MakeROQ ( void );
#endif//UTILS_H