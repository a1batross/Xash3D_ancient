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
#include <windows.h>
#include <time.h>
#include <io.h>

//register new types
#include "basetypes.h"
#include "basemath.h"
#include <ref_system.h>
#include <ref_stdlib.h>
#include "vprogs.h"
#include "common.h"
#include "client.h"

extern stdlib_api_t		std;
extern physic_exp_t		*pe;
extern byte		*zonepool;

#define EXEC_NOW		0 // don't return until completed
#define EXEC_INSERT		1 // insert at current position, but don't run yet
#define EXEC_APPEND		2 // add to end of the command buffer

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
	host_redirect_t	rd;		// remote console
	jmp_buf		abortframe;	// abort current frame

	char		finalmsg[MAX_STRING];// server shutdown final message

	dword		framecount;	// global framecount
	double		realtime;		// host realtime
	float		frametime;	// frametime (default 0.1)
	uint		sv_timer;		// SV_Input msg time
	uint		cl_timer;		// CL_Input msg time

	HWND		hWnd;		// main window

	bool		debug;		// show all warnings mode
	int		developer;	// show all developer's message

	bool		paused;		// freeze server
	bool		stuffcmdsrun;	// sturtup script



	uint		maxclients;	// host max clients

} host_parm_t;

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

#define Msg Host_Printf
#define MsgDev Host_DPrintf
#define MsgWarn Host_DWarnf

/*
===========================================
Host Interface
===========================================
*/
extern host_parm_t host;
long _stdcall Host_WndProc( HWND hWnd, uint uMsg, WPARAM wParam, LPARAM lParam);
void Host_Init ( uint funcname, int argc, char **argv );
void Host_Main ( void );
void Host_Free ( void );
void Host_SetServerState( int state );
int Host_ServerState( void );
void Host_AbortCurrentFrame( void );

// message functions
void Host_Print(const char *txt);
void Host_Printf(const char *fmt, ...);
void Host_DPrintf(int level, const char *fmt, ...);
void Host_DWarnf( const char *fmt, ...);
void Host_Error( const char *error, ... );

// host dlls managment
void Host_FreeRender( void );

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

// mouse support
void M_Activate( void );
void M_Event( int mstate );
#define WM_MOUSEWHEEL (WM_MOUSELAST + 1) // message that will be supported by the OS 
extern int mouse_x, mouse_y, old_mouse_x, old_mouse_y, mx_accum, my_accum;

// vm_exec.c
void PRVM_Init (void);

// cvars
extern cvar_t *dedicated;
extern cvar_t *host_serverstate;
extern cvar_t *host_frametime;

/*
==============================================================

CLIENT / SERVER SYSTEMS

==============================================================
*/
void CL_Init (void);
void CL_Drop (void);
void CL_Shutdown (void);
void CL_Frame (float time);

void SV_Init( void );
void SV_Shutdown( bool reconnect );
void SV_Frame( float time );
void SV_Transform( sv_edict_t *ed, vec3_t origin, vec3_t angles );

/*
==============================================================

CONSOLE VARIABLES\COMMANDS

==============================================================
*/
// console variables
cvar_t *Cvar_FindVar (const char *var_name);
cvar_t *_Cvar_Get (const char *var_name, const char *value, int flags, const char *description);
void Cvar_Set( const char *var_name, const char *value);
cvar_t *Cvar_Set2 (const char *var_name, const char *value, bool force);
void Cvar_CommandCompletion( void(*callback)(const char *s, const char *m));
void Cvar_FullSet (char *var_name, char *value, int flags);
void Cvar_SetLatched( const char *var_name, const char *value);
void Cvar_SetValue( const char *var_name, float value);
float Cvar_VariableValue (const char *var_name);
char *Cvar_VariableString (const char *var_name);
bool Cvar_Command (void);
void Cvar_WriteVariables( file_t *f );
void Cvar_Init (void);
char *Cvar_Userinfo (void);
char *Cvar_Serverinfo (void);
extern bool userinfo_modified;
extern cvar_t *cvar_vars;

// key / value info strings
char *Info_ValueForKey( char *s, char *key );
void Info_RemoveKey( char *s, char *key );
void Info_SetValueForKey( char *s, char *key, char *value );
bool Info_Validate( char *s );
void Info_Print( char *s );

// command buffer
void Cbuf_Init( int argc, char **argv );
void Cbuf_AddText (const char *text);
void Cbuf_InsertText (const char *text);
void Cbuf_ExecuteText (int exec_when, const char *text);
void Cbuf_Execute (void);

// console commands
int Cmd_Argc( void );
char *Cmd_Args( void );
char *Cmd_Argv( int arg );
void Cmd_Init( int argc, char **argv );
void Cmd_AddCommand(const char *cmd_name, xcommand_t function, const char *cmd_desc);
void Cmd_RemoveCommand(const char *cmd_name);
bool Cmd_Exists (const char *cmd_name);
void Cmd_CommandCompletion( void(*callback)(const char *s, const char *m));
bool Cmd_GetMapList( const char *s, char *completedname, int length );
bool Cmd_GetDemoList( const char *s, char *completedname, int length );
bool Cmd_GetMovieList (const char *s, char *completedname, int length );
void Cmd_TokenizeString (const char *text);
void Cmd_ExecuteString (const char *text);
void Cmd_ForwardToServer (void);

// get rid of this
#define Cvar_Get(name, value, flags) _Cvar_Get( name, value, flags, "no description" )

#endif//ENGINE_H