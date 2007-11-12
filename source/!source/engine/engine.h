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
#include <ref_stdlib.h>
#include "vprogs.h"
#include "const.h"
#include "common.h"
#include "cvar.h"
#include "client.h"

extern stdlib_api_t		std;
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
	uint		type;		// running at

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

stdlib_api_t Host_GetStdio( bool crash_on_error );

/*
===========================================
memory manager
===========================================
*/
// z_malloc-free
#define Z_Malloc(size) Mem_Alloc(zonepool, size)
#define Z_Free(data) Mem_Free(data)

/*
===========================================
System Events
===========================================
*/

#define Msg Com_Printf
#define MsgDev Com_DPrintf
#define MsgWarn Com_DWarnf

/*
===========================================
Host Interface
===========================================
*/
extern host_parm_t host;
void Host_Init ( uint funcname, int argc, char **argv );
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
double Sys_DoubleTime( void );
void Sys_Error( const char *msg, ... );
void Sys_SendKeyEvents( void );

//
// in_win.c
//
extern int mouse_x, mouse_y, old_mouse_x, old_mouse_y, mx_accum, my_accum;

// vm_exec.c
void PRVM_Init (void);

#endif//ENGINE_H