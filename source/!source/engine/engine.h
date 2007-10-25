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
#include "console.h"

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
	HOST_NOFOCUS,	// same as HOST_FRAME, but disable mouse and joy

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
#define Mem_Alloc(pool,size) Com->Mem.Alloc(pool, size, __FILE__, __LINE__)
#define Mem_Free(mem) Com->Mem.Free(mem, __FILE__, __LINE__)

//Hunk_AllocName
#define Mem_AllocPool(name) Com->Mem.AllocPool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) Com->Mem.FreePool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) Com->Mem.EmptyPool(pool, __FILE__, __LINE__)

#define Mem_Copy(dest, src, size) Com->Mem.Copy(dest, src, size, __FILE__, __LINE__) 

/*
===========================================
filesystem manager
===========================================
*/
#define FS_LoadFile(name, size) Com->Fs.LoadFile(name, size)
#define FS_LoadImage(name, data, size) Com->Fs.LoadImage(name, data, size)
#define FS_Search(path) Com->Fs.Search( path, true )
#define FS_WriteFile(name, data, size) Com->Fs.WriteFile(name, data, size )
#define FS_Open( path, mode ) Com->Fs.Open( path, mode )
#define FS_Read( file, buffer, size ) Com->Fs.Read( file, buffer, size )
#define FS_Write( file, buffer, size ) Com->Fs.Write( file, buffer, size )
#define FS_StripExtension( path ) Com->Fs.StripExtension( path )
#define FS_DefaultExtension( path, ext ) Com->Fs.DefaultExtension( path, ext )
#define FS_FileExtension( ext ) Com->Fs.FileExtension( ext )
#define CRC_Block( crc, size ) Com->Fs.CRC_Block( crc, size )
#define FS_FileExists( file ) Com->Fs.FileExists( file )
#define FS_Close( file ) Com->Fs.Close( file )
#define FS_FileBase( x, y ) Com->Fs.FileBase( x, y )
#define FS_Find( x ) Com->Fs.Search( x, false )
#define FS_Printf Com->Fs.Printf
#define FS_Print Com->Fs.Print
#define FS_Seek Com->Fs.Seek
#define FS_Tell Com->Fs.Tell
#define FS_Gets Com->Fs.Gets
char *FS_Gamedir( void );

/*
===========================================
scriptsystem manager
===========================================
*/
#define COM_Parse(data) Com->Script.ParseToken(data)
#define COM_Token Com->Script.Token

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

#define Msg Con_Printf
#define MsgDev Con_DPrintf
#define MsgWarn Con_DWarnf
#define Sys_LoadLibrary std.LoadLibrary
#define Sys_FreeLibrary std.FreeLibrary
#define Sys_Sleep std.sleep
#define Sys_Print std.print
#define Sys_Quit std.exit
#define Sys_ConsoleInput std.input


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