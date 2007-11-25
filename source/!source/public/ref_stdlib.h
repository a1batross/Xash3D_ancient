//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    ref_stdlib.h - generic shared interfaces
//=======================================================================
#ifndef REF_STDLIB_H
#define REF_STDLIB_H

/*
==============================================================================

STDLIB GENERIC ALIAS NAMES
==============================================================================
*/
/*
==========================================
	memory manager funcs
==========================================
*/
#define Mem_Alloc(pool, size) std.malloc(pool, size, __FILE__, __LINE__)
#define Mem_Realloc(pool, ptr, size) std.realloc(pool, ptr, size, __FILE__, __LINE__)
#define Mem_Move(pool, ptr, data, size) std.move(pool, ptr, data, size, __FILE__, __LINE__)
#define Mem_Free(mem) std.free(mem, __FILE__, __LINE__)
#define Mem_AllocPool(name) std.mallocpool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) std.freepool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) std.clearpool(pool, __FILE__, __LINE__)
#define Mem_Copy(dest, src, size ) std.memcpy(dest, src, size, __FILE__, __LINE__)
#define Mem_Set(dest, src, size ) std.memset(dest, src, size, __FILE__, __LINE__)
#define Mem_Check() std.memcheck(__FILE__, __LINE__)

/*
==========================================
	parsing manager funcs
==========================================
*/
#define Com_ParseToken std.Com_ParseToken
#define Com_ParseWord std.Com_ParseWord
#define Com_Filter std.Com_Filter
#define Com_LoadScript std.Com_LoadScript
#define Com_IncludeScript std.Com_AddScript
#define Com_ResetScript std.Com_ResetScript
#define Com_GetToken std.Com_ReadToken
#define Com_TryToken std.Com_TryToken
#define Com_FreeToken std.Com_FreeToken
#define Com_SkipToken std.Com_SkipToken
#define Com_MatchToken std.Com_MatchToken
#define com_token std.com_token
#define g_TXcommand std.com_TXcommand		// get rid of this

/*
===========================================
filesystem manager
===========================================
*/
#define FS_AddGameHierarchy std.Com_AddGameHierarchy
#define FS_LoadGameInfo std.Com_LoadGameInfo
#define FS_InitRootDir std.Com_InitRootDir
#define FS_LoadFile(name, size) std.Com_LoadFile(name, size)
#define FS_Search std.Com_Search
#define FS_WriteFile(name, data, size) std.Com_WriteFile(name, data, size )
#define FS_Open( path, mode ) std.fopen( path, mode )
#define FS_Read( file, buffer, size ) std.fread( file, buffer, size )
#define FS_Write( file, buffer, size ) std.fwrite( file, buffer, size )
#define FS_StripExtension( path ) std.Com_StripExtension( path )
#define FS_DefaultExtension( path, ext ) std.Com_DefaultExtension( path, ext )
#define FS_FileExtension( ext ) std.Com_FileExtension( ext )
#define FS_FileExists( file ) std.Com_FileExists( file )
#define FS_Close( file ) std.fclose( file )
#define FS_FileBase( x, y ) std.Com_FileBase( x, y )
#define FS_Printf std.fprintf
#define FS_Print std.fprint
#define FS_Seek std.fseek
#define FS_Tell std.ftell
#define FS_Gets std.fgets
#define FS_Gamedir std.GameInfo->gamedir
#define FS_Title std.GameInfo->title
#define FS_ClearSearchPath std.Com_ClearSearchPath
#define FS_CheckParm std.Com_CheckParm
#define FS_GetParmFromCmdLine std.Com_GetParm
#define FS_LoadImage std.Com_LoadImage
#define FS_SaveImage std.Com_SaveImage
#define FS_FreeImage std.Com_FreeImage

/*
===========================================
virtual filesystem manager
===========================================
*/
#define VFS_Open	std.vfopen
#define VFS_Write	std.vfwrite
#define VFS_Read	std.vfread
#define VFS_Print	std.vfprint
#define VFS_Printf	std.vfprintf
#define VFS_Gets	std.vfgets
#define VFS_Seek	std.vfseek
#define VFS_Tell	std.vftell
#define VFS_Close	std.vfclose
#define VFS_Unpack	std.vfunpack

/*
===========================================
crclib manager
===========================================
*/
#define CRC_Init		std.crc_init
#define CRC_Block		std.crc_block
#define CRC_ProcessByte	std.crc_process
#define CRC_Sequence	std.crc_sequence
#define Com_BlockChecksum	std.crc_blockchecksum
#define Com_BlockChecksumKey	std.crc_blockchecksumkey

/*
===========================================
imagelib utils
===========================================
*/
#define Image_Processing std.Com_ProcessImage

/*
===========================================
misc utils
===========================================
*/
#define GI		std.GameInfo
#define Sys_LoadLibrary	std.Com_LoadLibrary
#define Sys_FreeLibrary	std.Com_FreeLibrary
#define Sys_GetProcAddress	std.Com_GetProcAddress
#define Sys_Sleep		std.sleep
#define Sys_Print		std.print
#define Sys_ConsoleInput	std.input
#define Sys_GetKeyEvents	std.keyevents
#define Sys_GetClipboardData	std.clipboard
#define Sys_Quit		std.exit
#define Sys_ConsoleInput	std.input
#define GetNumThreads	std.Com_NumThreads
#define ThreadLock		std.Com_ThreadLock
#define ThreadUnlock	std.Com_ThreadUnlock
#define RunThreadsOnIndividual std.Com_CreateThread

/*
===========================================
misc utils
===========================================
*/
#define timestamp		std.timestamp
#define copystring(str)	std.stralloc(str, __FILE__, __LINE__)
#define strcasecmp		std.stricmp
#define strncasecmp		std.strnicmp
#define va		std.va

#endif//REF_STDLIB_H