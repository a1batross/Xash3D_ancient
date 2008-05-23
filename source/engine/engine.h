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
#include "basetypes.h"
#include "net_msg.h"
#include "screen.h"
#include "keycodes.h"
#include "pmove.h"

extern stdlib_api_t		com;
extern physic_exp_t		*pe;
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
	host_redirect_t	rd;		// remote console
	jmp_buf		abortframe;	// abort current frame

	string		finalmsg;		// server shutdown final message

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



	uint		maxclients;	// host max clients (unused)

} host_parm_t;

/*
===========================================
memory manager
===========================================
*/
// zone malloc
#define Z_Malloc(size) Mem_Alloc( zonepool, size )

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
extern cvar_t *cm_paused;

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
void SV_Transform( sv_edict_t *ed, matrix4x3 transform );

/*
==============================================================

MISC UTILS

==============================================================
*/
char *Info_ValueForKey( char *s, char *key );
void Info_RemoveKey( char *s, char *key );
void Info_SetValueForKey( char *s, char *key, char *value );
bool Info_Validate( char *s );
void Info_Print( char *s );
void Cmd_ForwardToServer( void ); // client callback
char *Cvar_Userinfo( void );
char *Cvar_Serverinfo( void );
void Cmd_WriteVariables( file_t *f );
bool Cmd_GetMapList (const char *s, char *completedname, int length );
bool Cmd_GetFontList (const char *s, char *completedname, int length );
bool Cmd_GetDemoList(const char *s, char *completedname, int length );
bool Cmd_GetMovieList(const char *s, char *completedname, int length);

// get rid of this
float frand(void);	// 0 to 1
float crand(void);	// -1 to 1

#endif//ENGINE_H