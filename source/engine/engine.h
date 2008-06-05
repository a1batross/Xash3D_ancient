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
#include "stdapi.h"
#include "stdref.h"
#include "basefiles.h"
#include "dllapi.h"
#include "net_msg.h"
#include "screen.h"
#include "keycodes.h"

extern stdlib_api_t		com;
extern physic_exp_t		*pe;
extern vprogs_exp_t		*vm;
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

PRVM INTERACTIONS

==============================================================
*/
#define	prog vm->prog	// global callback to vprogs.dll

_inline edict_t *PRVM_EDICT_NUM( int n )
{
	if((n >= 0) && (n < prog->max_edicts))
		return prog->edicts + n;
	prog->error_cmd( "PRVM_EDICT_NUM: %s: bad number %i (called at %s:%i)\n", prog->name, n, __FILE__, __LINE__ );
	return NULL;	
}

#define PRVM_Begin
#define PRVM_End	prog = 0
#define PRVM_NAME	(prog->name ? prog->name : "unnamed.dat")

#define VM_SAFEPARMCOUNT(p,f)	if(prog->argc != p) PRVM_ERROR(#f " wrong parameter count (" #p " expected ) !")
#define PRVM_ERROR if( prog ) prog->error_cmd
#define PRVM_NUM_FOR_EDICT(e) ((int)((edict_t *)(e) - prog->edicts))
#define PRVM_NEXT_EDICT(e) ((e) + 1)
#define PRVM_EDICT_TO_PROG(e) (PRVM_NUM_FOR_EDICT(e))
#define PRVM_PROG_TO_EDICT(n) (PRVM_EDICT_NUM(n))
#define PRVM_PUSH_GLOBALS prog->pev_save = prog->globals.sv->pev, prog->other_save = prog->globals.sv->other
#define PRVM_POP_GLOBALS prog->globals.sv->pev = prog->pev_save, prog->globals.sv->other = prog->other_save
#define VM_SAFEPARMCOUNT(p,f)	if(prog->argc != p) PRVM_ERROR(#f " wrong parameter count (" #p " expected ) !")

#define	PRVM_G_FLOAT(o) (prog->globals.gp[o])
#define	PRVM_G_INT(o) (*(int *)&prog->globals.gp[o])
#define	PRVM_G_EDICT(o) (PRVM_PROG_TO_EDICT(*(int *)&prog->globals.gp[o]))
#define	PRVM_G_EDICTNUM(o) PRVM_NUM_FOR_EDICT(PRVM_G_EDICT(o))
#define	PRVM_G_VECTOR(o) (&prog->globals.gp[o])
#define	PRVM_G_STRING(o) (PRVM_GetString(*(string_t *)&prog->globals.gp[o]))
#define	PRVM_G_FUNCTION(o) (*(func_t *)&prog->globals.gp[o])
#define	VM_RETURN_EDICT(e)	(((int *)prog->globals.gp)[OFS_RETURN] = PRVM_EDICT_TO_PROG(e))

// FIXME: make these go away?
#define	PRVM_E_FLOAT(e,o) (((float*)e->progs.vp)[o])
#define	PRVM_E_INT(e,o) (((int*)e->progs.vp)[o])
#define	PRVM_E_VECTOR(e,o) (&((float*)e->progs.vp)[o])
#define	PRVM_E_STRING(e,o) (PRVM_GetString(*(string_t *)&((float*)e->progs.vp)[o]))

#define	e10	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL
#define	e100	e10,e10,e10,e10,e10,e10,e10,e10,e10,e10
#define	e1000	e100,e100,e100,e100,e100,e100,e100,e100,e100,e100

#define	VM_STRINGTEMP_BUFFERS 16
#define	VM_STRINGTEMP_LENGTH MAX_INPUTLINE

#define PRVM_GetString	vm->GetString
#define PRVM_SetEngineString	vm->SetEngineString
#define PRVM_AllocString	vm->AllocString
#define PRVM_FreeString	vm->FreeString

#define PRVM_SetProg	vm->SetProg
#define PRVM_InitProg	vm->InitProg
#define PRVM_ResetProg	vm->ResetProg
#define PRVM_LoadProgs	vm->LoadProgs
#define PRVM_ExecuteProgram	vm->ExecuteProgram
#define PRVM_ED_LoadFromFile	vm->LoadFromFile
#define PRVM_ED_ParseGlobals	vm->ParseGlobals
#define PRVM_ED_WriteGlobals	vm->WriteGlobals
#define PRVM_ED_Print	vm->PrintEdict
#define PRVM_ED_Write	vm->WriteEdict
#define PRVM_ED_ParseEdict	vm->ParseEdict
#define PRVM_ED_Alloc	vm->AllocEdict
#define PRVM_ED_Free	vm->FreeEdict
#define PRVM_MEM_IncreaseEdicts vm->IncreaseEdicts

#define PRVM_StackTrace	vm->StackTrace
#define VM_Warning		vm->Warning
#define PRVM_Crash		vm->Crash
#define VM_Error		vm->Error

#define PRVM_ED_FindFieldOffset	vm->FindFieldOffset
#define PRVM_ED_FindGlobalOffset	vm->FindGlobalOffset
#define PRVM_ED_FindFunctionOffset	vm->FindFunctionOffset
#define PRVM_ED_FindField		vm->FindField
#define PRVM_ED_FindGlobal		vm->FindGlobal
#define PRVM_ED_FindFunction		vm->FindFunction

// builtins and other general functions

char *VM_GetTempString(void);
void VM_CheckEmptyString (const char *s);
void VM_VarString(int first, char *out, int outlength);

void VM_checkextension (void);
void VM_error (void);
void VM_objerror (void);
void VM_print (void);
void VM_bprint (void);
void VM_sprint (void);
void VM_centerprint(void);
void VM_normalize (void);
void VM_veclength (void);
void VM_vectoyaw (void);
void VM_vectoangles (void);
void VM_random_long (void);
void VM_random_float (void);
void VM_localsound(void);
void VM_break (void);
void VM_localcmd (void);
void VM_cvar (void);
void VM_cvar_string(void);
void VM_cvar_set (void);
void VM_wprint (void);
void VM_ftoa (void);
void VM_fabs (void);
void VM_vtoa (void);
void VM_atov (void);
void VM_etos (void);
void VM_atof(void);
void VM_itof(void);
void VM_ftoe(void);
void VM_create (void);
void VM_remove (void);
void VM_find (void);
void VM_findfloat (void);
void VM_findchain (void);
void VM_findchainfloat (void);
void VM_findflags (void);
void VM_findchainflags (void);
void VM_precache_file (void);
void VM_precache_error (void);
void VM_precache_sound (void);
void VM_coredump (void);

void VM_stackdump (void);
void VM_crash(void); // REMOVE IT
void VM_traceon (void);
void VM_traceoff (void);
void VM_eprint (void);
void VM_rint (void);
void VM_floor (void);
void VM_ceil (void);
void VM_nextent (void);

void VM_changelevel (void);
void VM_sin (void);
void VM_cos (void);
void VM_sqrt (void);
void VM_randomvec (void);
void VM_registercvar (void);
void VM_min (void);
void VM_max (void);
void VM_bound (void);
void VM_pow (void);
void VM_copyentity (void);

void VM_Files_Init(void);
void VM_Files_CloseAll(void);

void VM_fopen(void);
void VM_fclose(void);
void VM_fgets(void);
void VM_fputs(void);
vfile_t *VM_GetFileHandle( int index );

void VM_strlen(void);
void VM_strcat(void);
void VM_substring(void);
void VM_stov(void);
void VM_allocstring(void);
void VM_freestring(void);

void VM_servercmd (void);
void VM_clientcmd (void);

void VM_tokenize (void);
void VM_argv (void);

void VM_isserver(void);
void VM_clientcount(void);
void VM_clientstate(void);
void VM_getmousepos(void);
void VM_gettime(void);
void VM_loadfromdata(void);
void VM_parseentitydata(void);
void VM_loadfromfile(void);
void VM_modulo(void);

void VM_search_begin(void);
void VM_search_end(void);
void VM_search_getsize(void);
void VM_search_getfilename(void);
void VM_chr(void);
void VM_iscachedpic(void);
void VM_precache_pic(void);
void VM_drawcharacter(void);
void VM_drawstring(void);
void VM_drawpic(void);
void VM_drawfill(void);
void VM_drawsetcliparea(void);
void VM_drawresetcliparea(void);
void VM_getimagesize(void);

void VM_vectorvectors (void);
void VM_makevectors (void);
void VM_makevectors2 (void);

void VM_keynumtostring (void);
void VM_stringtokeynum (void);

void VM_cin_open( void );
void VM_cin_close( void );
void VM_cin_getstate( void );
void VM_cin_restart( void );

void VM_R_PolygonBegin (void);
void VM_R_PolygonVertex (void);
void VM_R_PolygonEnd (void);

void VM_bitshift (void);

void VM_altstr_count( void );
void VM_altstr_prepare( void );
void VM_altstr_get( void );
void VM_altstr_set( void );
void VM_altstr_ins(void);

void VM_buf_create(void);
void VM_buf_del (void);
void VM_buf_getsize (void);
void VM_buf_copy (void);
void VM_buf_sort (void);
void VM_buf_implode (void);
void VM_bufstr_get (void);
void VM_bufstr_set (void);
void VM_bufstr_add (void);
void VM_bufstr_free (void);

void VM_Cmd_Init(void);
void VM_Cmd_Reset(void);

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