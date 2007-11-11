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
#include <ref_system.h>
#include "vprogs.h"
#include "const.h"
#include "common.h"
#include "cvar.h"
#include "client.h"

extern stdlib_api_t		std;
extern common_exp_t		*Com;
extern physic_exp_t		*Phys;
extern byte		*zonepool;

typedef enum
{
	HOST_INIT,	// initalize operations
	HOST_FRAME,	// host running
	HOST_SHUTDOWN,	// shutdown operations	
	HOST_ERROR,	// host stopped by error
	HOST_SLEEP,	// sleeped by different reason, e.g. minimize window
	HOST_NOFOCUS,	// same as HOST_FRAME, but disable mouse

} host_state;

typedef enum
{
	HOST_OFFLINE,	// host not running
	HOST_NORMAL,	// normal mode
	HOST_DEDICATED,	// dedicated mode
} host_mode;

typedef struct host_redirect_s
{
	int	target;
	char	*buffer;
	int	buffersize;
	void	(*flush)(int target, char *buffer);

} host_redirect_t;

typedef struct host_parm_s
{
	host_state	state;		// global host state
	host_mode		type;		// running at

	bool		debug;		// show all warnings mode
	int		developer;	// show all developer's message

	bool		paused;		// freeze server
	bool		stuffcmdsrun;	// sturtup script

	jmp_buf		abortframe;	// abort current frame

	dword		framecount;	// global framecount
	double		realtime;		// host realtime
	float		frametime;	// frametime (default 0.1)
	uint		sv_timer;		// SV_Input msg time
	uint		cl_timer;		// CL_Input msg time

	uint		maxclients;	// host max clients

	host_redirect_t	rd;

} host_parm_t;

bool _GetParmFromCmdLine( char *parm, char *out, size_t size );
#define GetParmFromCmdLine( parm, out ) _GetParmFromCmdLine( parm, out, sizeof(out)) 

stdlib_api_t Host_GetStdio( bool crash_on_error );

/*
===========================================
memory manager
===========================================
*/
//z_malloc-free
#define Z_Malloc(size) Mem_Alloc(zonepool, size)
#define Z_Free(data) Mem_Free(data)

//malloc-free
#define Mem_Alloc(pool,size) std.malloc(pool, size, __FILE__, __LINE__)
#define Mem_Realloc(pool, mem, size) std.realloc(pool, mem, size, __FILE__, __LINE__)
#define Mem_Free(mem) std.free(mem, __FILE__, __LINE__)

//Hunk_AllocName
#define Mem_AllocPool(name) std.mallocpool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) std.freepool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) std.clearpool(pool, __FILE__, __LINE__)

#define Mem_Copy(dest, src, size) std.memcpy(dest, src, size, __FILE__, __LINE__) 

/*
===========================================
filesystem manager
===========================================
*/
#define FS_AddGameHierarchy std.AddGameHierarchy
#define FS_LoadGameInfo std.LoadGameInfo
#define FS_InitRootDir std.InitRootDir
#define FS_LoadFile(name, size) std.Fs.LoadFile(name, size)
#define FS_LoadImage(name, data, size) std.Fs.LoadImage(name, data, size)
#define FS_Search(path) std.Fs.Search( path, true )
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
char *FS_Gamedir( void );

/*
===========================================
scriptsystem manager
===========================================
*/
#define COM_Parse(data) std.Script.ParseToken(data)
#define COM_Token std.Script.Token
#define COM_Filter std.Script.FilterToken

#define COM_LoadScript std.Script.Load
#define COM_IncludeScript std.Script.Include
#define COM_ResetScript std.Script.Reset
#define COM_GetToken std.Script.GetToken
#define COM_TryToken std.Script.TryToken
#define COM_FreeToken std.Script.FreeToken
#define COM_SkipToken std.Script.SkipToken
#define COM_MatchToken std.Script.MatchToken

#define CRC_Init		std.crc_init
#define CRC_Block		std.crc_block
#define CRC_ProcessByte	std.crc_process

/*
===========================================
infostring manager
===========================================
*/
#define Info_Print(x) Com->Info.Print
#define Info_Validate(x) Com->Info.Validate(x)
#define Info_RemoveKey(x, y) Com->Info.RemoveKey(x,y)
#define Info_ValueForKey(x,y) Com->Info.ValueForKey(x,y)
#define Info_SetValueForKey(x,y,z) Com->Info.SetValueForKey(x,y,z)

/*
===========================================
System Timer
===========================================
*/
double Sys_DoubleTime( void );
#define GI Com->GameInfo()

/*
===========================================
System Events
===========================================
*/

#define Msg Com_Printf
#define MsgDev Com_DPrintf
#define MsgWarn Com_DWarnf
#define Sys_LoadLibrary std.LoadLibrary
#define Sys_FreeLibrary std.FreeLibrary
#define Sys_Sleep std.sleep
#define Sys_Print std.print
#define Sys_Quit std.exit
#define Sys_ConsoleInput std.input
#define copystring		std.stralloc
#define va		std.va

/*
===========================================
Host Interface
===========================================
*/
extern host_parm_t host;
void Host_Init ( char *funcname, int argc, char **argv );
void Host_Main ( void );
void Host_Free ( void );
void Host_Error( const char *error, ... );
void Host_AbortCurrentFrame( void );


// host cmds
void Host_Error_f( void );

/*
===========================================
System utilites
===========================================
*/
void Sys_Error( const char *msg, ... );

//
// in_win.c
//
extern int mouse_x, mouse_y, old_mouse_x, old_mouse_y, mx_accum, my_accum;

// vm_exec.c
void PRVM_Init (void);

#endif//ENGINE_H