//=======================================================================
//			Copyright XashXT Group 2007 ©
//		common.h - definitions common between client and server
//=======================================================================
#ifndef COMMON_H
#define COMMON_H

#include <setjmp.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>

#include "basetypes.h"
#include "stdapi.h"
#include "stdref.h"
#include "basefiles.h"
#include "dllapi.h"
#include "net_msg.h"
#include "screen.h"

// linked interfaces
extern stdlib_api_t		com;
extern physic_exp_t		*pe;
extern vprogs_exp_t		*vm;
extern vsound_exp_t		*se;

/*
==============================================================

INPUT

==============================================================
*/

typedef enum e_keycodes
{
	// keyboard
	K_TAB = 9,
	K_ENTER = 13,
	K_ESCAPE = 27,
	K_SPACE = 32,
	K_BACKSPACE = 127,
	K_COMMAND = 128,
	K_CAPSLOCK,
	K_POWER,
	K_PAUSE,
	K_UPARROW,
	K_DOWNARROW,
	K_LEFTARROW,
	K_RIGHTARROW,
	K_ALT,
	K_CTRL,
	K_SHIFT,
	K_INS,
	K_DEL,
	K_PGDN,
	K_PGUP,
	K_HOME,
	K_END,
	K_F1,
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,
	K_F13,
	K_F14,
	K_F15,
	K_KP_HOME,
	K_KP_UPARROW,
	K_KP_PGUP,
	K_KP_LEFTARROW,
	K_KP_5,
	K_KP_RIGHTARROW,
	K_KP_END,
	K_KP_DOWNARROW,
	K_KP_PGDN,
	K_KP_ENTER,
	K_KP_INS,
	K_KP_DEL,
	K_KP_SLASH,
	K_KP_MINUS,
	K_KP_PLUS,
	K_KP_NUMLOCK,
	K_KP_STAR,
	K_KP_EQUALS,

	// mouse
	K_MOUSE1,
	K_MOUSE2,
	K_MOUSE3,
	K_MOUSE4,
	K_MOUSE5,
	K_MWHEELDOWN,
	K_MWHEELUP,

	K_LAST_KEY // this had better be < 256!
} keyNum_t;

static byte scan_to_key[128] = 
{ 
	0,27,'1','2','3','4','5','6','7','8','9','0','-','=',K_BACKSPACE,9,
	'q','w','e','r','t','y','u','i','o','p','[',']', 13 , K_CTRL,
	'a','s','d','f','g','h','j','k','l',';','\'','`',
	K_SHIFT,'\\','z','x','c','v','b','n','m',',','.','/',K_SHIFT,
	'*',K_ALT,' ',K_CAPSLOCK,
	K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,K_F9,K_F10,
	K_PAUSE,0,K_HOME,K_UPARROW,K_PGUP,K_KP_MINUS,K_LEFTARROW,K_KP_5,
	K_RIGHTARROW,K_KP_PLUS,K_END,K_DOWNARROW,K_PGDN,K_INS,K_DEL,
	0,0,0,K_F11,K_F12,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

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
extern cvar_t *crosshair;
extern cvar_t *scr_loading;
extern cvar_t *scr_width;
extern cvar_t *scr_height;

/*
==============================================================

HOST INTERFACE

==============================================================
*/
typedef enum
{
	HOST_INIT = 0,	// initalize operations
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

	int		developer;	// show all developer's message
	bool		paused;		// freeze server
	bool		stuffcmdsrun;	// sturtup script
} host_parm_t;

extern host_parm_t host;
long Host_WndProc( void *hWnd, uint uMsg, uint wParam, long lParam );
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
void Host_SndRestart_f( void );

// host cmds
void Host_Error_f( void );

/*
==============================================================

CLIENT / SERVER SYSTEMS

==============================================================
*/
void CL_Init( void );
void CL_Shutdown( void );
void CL_Frame( float time );

void SV_Init( void );
void SV_Shutdown( bool reconnect );
void SV_Frame( float time );

/*
==============================================================

PRVM INTERACTIONS

==============================================================
*/
#define prog	vm->prog	// global callback to vprogs.dll

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

#define PRVM_ERROR if( prog ) prog->error_cmd
#define PRVM_NUM_FOR_EDICT(e) ((int)((edict_t *)(e) - prog->edicts))
#define PRVM_NEXT_EDICT(e) ((e) + 1)
#define PRVM_EDICT_TO_PROG(e) (PRVM_NUM_FOR_EDICT(e))
#define PRVM_PROG_TO_EDICT(n) (PRVM_EDICT_NUM(n))
#define PRVM_PUSH_GLOBALS prog->pev_save = prog->globals.sv->pev, prog->other_save = prog->globals.sv->other
#define PRVM_POP_GLOBALS prog->globals.sv->pev = prog->pev_save, prog->globals.sv->other = prog->other_save

#define PRVM_G_FLOAT(o) (prog->globals.gp[o])
#define PRVM_G_INT(o) (*(int *)&prog->globals.gp[o])
#define PRVM_G_EDICT(o) (PRVM_PROG_TO_EDICT(*(int *)&prog->globals.gp[o]))
#define PRVM_G_EDICTNUM(o) PRVM_NUM_FOR_EDICT(PRVM_G_EDICT(o))
#define PRVM_G_VECTOR(o) (&prog->globals.gp[o])
#define PRVM_G_STRING(o) (PRVM_GetString(*(string_t *)&prog->globals.gp[o]))
#define PRVM_G_FUNCTION(o) (*(func_t *)&prog->globals.gp[o])
#define VM_RETURN_EDICT(e)	(((int *)prog->globals.gp)[OFS_RETURN] = PRVM_EDICT_TO_PROG(e))
#define e10 NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL

// helper common functions
const char *VM_VarArgs( int start_arg );
bool VM_ValidateArgs( const char *builtin, int num_argc );
void VM_ValidateString( const char *s );
void VM_Cmd_Init( void );
void VM_Cmd_Reset( void );

#define PRVM_GetString	vm->GetString
#define PRVM_SetEngineString	vm->SetEngineString
#define PRVM_SetTempString	vm->SetTempString
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
void VM_ConPrintf( void );
void VM_ConDPrintf( void );
void VM_HostError( void );
void VM_EdictError( void );
void VM_SysExit( void );
void VM_CmdArgv( void );
void VM_CmdArgc( void );
void VM_ComTrace( void );
void VM_ComFileExists( void );
void VM_ComFileSize( void );
void VM_ComFileTime( void );
void VM_ComLoadScript( void );
void VM_ComResetScript( void );
void VM_ComReadToken( void );
void VM_ComFilterToken( void );
void VM_ComSearchFiles( void );
void VM_ComSearchNames( void );
void VM_RandomLong( void );
void VM_RandomFloat( void );
void VM_RandomVector( void );
void VM_CvarRegister( void );
void VM_CvarSetValue( void );
void VM_CvarGetValue( void );
void VM_CvarSetString( void );
void VM_CvarGetString( void );
void VM_ComVA( void );
void VM_ComStrlen( void );
void VM_TimeStamp( void );
void VM_SubString( void );
void VM_LocalCmd( void );
void VM_localsound( void );
void VM_SpawnEdict( void );
void VM_RemoveEdict( void );
void VM_NextEdict( void );
void VM_CopyEdict( void );
void VM_FindEdict( void );
void VM_FindField( void );
void VM_FS_Open( void );
void VM_FS_Close( void );
void VM_FS_Gets( void );
void VM_FS_Puts( void );
void VM_precache_pic( void );
void VM_drawcharacter( void );
void VM_drawstring( void );
void VM_drawpic( void );
void VM_drawfill( void );
void VM_drawmodel( void );
void VM_getimagesize( void );
void VM_min( void );
void VM_max( void );
void VM_bound( void );
void VM_mod( void );
void VM_pow( void );
void VM_sin( void );
void VM_cos( void );
void VM_tan( void );
void VM_asin( void );
void VM_acos( void );
void VM_atan( void );
void VM_sqrt( void );
void VM_rint( void );
void VM_floor( void );
void VM_ceil( void );
void VM_fabs( void );
void VM_VectorNormalize( void );
void VM_VectorLength( void );		

/*
==============================================================

MISC COMMON FUNCTIONS

==============================================================
*/
// client printf level
enum e_clprint
{
	PRINT_CONSOLE = 0,	// normal messages
	PRINT_CENTER,	// centerprint
	PRINT_CHAT,	// chat messages (with sound)
};
extern byte *zonepool;

#define Z_Malloc(size) Mem_Alloc( zonepool, size )
void CL_GetEntitySoundSpatialization( int ent, vec3_t origin, vec3_t velocity );
void SV_Transform( sv_edict_t *ed, matrix4x3 transform );
bool SV_ReadComment( char *comment, int savenum );
int CL_PMpointcontents( vec3_t point );
void CL_AddLoopingSounds( void );
void CL_RegisterSounds( void );
void CL_Drop( void );
char *Info_ValueForKey( char *s, char *key );
void Info_RemoveKey( char *s, char *key );
void Info_SetValueForKey( char *s, char *key, char *value );
bool Info_Validate( char *s );
void Info_Print( char *s );
void Cmd_ForwardToServer( void ); // client callback
char *Cvar_Userinfo( void );
char *Cvar_Serverinfo( void );
void Cmd_WriteVariables( file_t *f );
bool Cmd_GetMapList(const char *s, char *completedname, int length );
bool Cmd_GetFontList(const char *s, char *completedname, int length );
bool Cmd_GetDemoList(const char *s, char *completedname, int length );
bool Cmd_GetMovieList(const char *s, char *completedname, int length );
bool Cmd_GetMusicList(const char *s, char *completedname, int length );
bool Cmd_GetSoundList(const char *s, char *completedname, int length );
bool Cmd_CheckMapsList( void );
void Sys_Error( const char *msg, ... );
void Sys_SendKeyEvents( void );

#define MAX_ENTNUMBER	99999		// for server and client parsing
#define MAX_HEARTBEAT	-99999.0f		// connection time


// get rid of this
float frand(void);	// 0 to 1
float crand(void);	// -1 to 1



#endif//COMMON_H