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
#include "ref_dllapi.h"
#include "net_msg.h"

// linked interfaces
extern stdlib_api_t		com;
extern physic_exp_t		*pe;
extern vprogs_exp_t		*vm;
extern vsound_exp_t		*se;

#define MAX_ENTNUMBER	99999		// for server and client parsing
#define MAX_HEARTBEAT	-99999		// connection time
#define MAX_EVENTS		1024		// system events

// cvars
extern cvar_t *scr_loading;
extern cvar_t *scr_download;
extern cvar_t *scr_width;
extern cvar_t *scr_height;
extern cvar_t *sv_newprotocol;

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

typedef enum
{
	RD_NONE = 0,
	RD_CLIENT,
	RD_PACKET,

} rdtype_t;

typedef struct host_redirect_s
{
	rdtype_t	target;
	char	*buffer;
	int	buffersize;
	netadr_t	address;
	void	(*flush)( netadr_t adr, rdtype_t target, char *buffer );
} host_redirect_t;

typedef struct host_parm_s
{
	host_state	state;		// global host state
	uint		type;		// running at
	host_redirect_t	rd;		// remote console
	jmp_buf		abortframe;	// abort current frame
	dword		errorframe;	// to avoid each-frame host error
	string		finalmsg;		// server shutdown final message

	dword		framecount;	// global framecount
	dword		frametime[2];	// second value is old frametime
	int		events_head;
	int		events_tail;
	sys_event_t	events[MAX_EVENTS];

	HWND		hWnd;		// main window
	int		developer;	// show all developer's message
	word		max_edicts;
} host_parm_t;

extern host_parm_t host;

//
// host.c
//

void Host_Init ( int argc, char **argv );
void Host_Main ( void );
void Host_Free ( void );
void Host_SetServerState( int state );
int Host_ServerState( void );
int Host_FrameTime( void );
int Host_MaxClients( void );
void Host_AbortCurrentFrame( void );
dword Host_EventLoop( void );
void Host_Print(const char *txt);
void Host_Error( const char *error, ... );
void Sys_Error( const char *msg, ... );

/*
==============================================================

CLIENT / SERVER SYSTEMS

==============================================================
*/
#define FL_CLIENT			(1<<0)	// this is client
#define FL_MONSTER			(1<<1)	// this is npc
#define FL_DEADMONSTER		(1<<2)	// dead npc or dead player
#define FL_WORLDBRUSH		(1<<3)	// Not moveable/removeable brush entity
#define FL_DORMANT			(1<<4)	// Entity is dormant, no updates to client
#define FL_FRAMETHINK		(1<<5)	// entity will be thinking every frame
#define FL_GRAPHED			(1<<6)	// worldgraph has this ent listed as something that blocks a conection
#define FL_FLOAT			(1<<7)	// this entity can be floating. FIXME: remove this ?
#define FL_TRACKTRAIN		(1<<8)	// old stuff...

void CL_Init( void );
void CL_Shutdown( void );
void CL_Frame( dword time );
void CL_PacketEvent( netadr_t from, sizebuf_t *msg );

void SV_Init( void );
void SV_Shutdown( bool reconnect );
void SV_Frame( dword time );
void SV_PacketEvent( netadr_t from, sizebuf_t *msg );

/*
==============================================================

PRVM INTERACTIONS

==============================================================
*/
#define prog	vm->prog	// global callback to vprogs.dll
#define PRVM_EDICT_NUM( num )	_PRVM_EDICT_NUM( num, __FILE__, __LINE__ )

_inline edict_t *_PRVM_EDICT_NUM( int n, const char * file, const int line )
{
	if(!prog) Host_Error(" prog unset at (%s:%d)\n", file, line );
	if((n >= 0) && (n < prog->max_edicts))
		return prog->edicts + n;
	prog->error_cmd( "PRVM_EDICT_NUM: %s: bad number %i (called at %s:%i)\n", prog->name, n, file, line );
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

#define PRVM_E_FLOAT(e,o) (((float*)e->progs.vp)[o])
#define PRVM_E_INT(e,o) (((int*)e->progs.vp)[o])
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
void VM_SetTraceGlobals( const trace_t *trace );
void VM_ValidateString( const char *s );
void VM_Cmd_Init( void );
void VM_Cmd_Reset( void );

#define PRVM_GetString	vm->GetString
#define PRVM_SetEngineString	vm->SetEngineString
#define PRVM_SetTempString	vm->SetTempString

#define VM_Frame		vm->Update
#define PRVM_SetProg	vm->SetProg
#define PRVM_InitProg	vm->InitProg
#define PRVM_ResetProg	vm->ResetProg
#define PRVM_LoadProgs	vm->LoadProgs
#define PRVM_ProgLoaded	vm->ProgLoaded
#define PRVM_ED_LoadFromFile	vm->LoadFromFile
#define PRVM_ED_ReadGlobals	vm->ReadGlobals
#define PRVM_ED_WriteGlobals	vm->WriteGlobals
#define PRVM_ED_Print	vm->PrintEdict
#define PRVM_ED_Write	vm->WriteEdict
#define PRVM_ED_Read	vm->ReadEdict
#define PRVM_ED_Alloc	vm->AllocEdict
#define PRVM_ED_Free	vm->FreeEdict
#define PRVM_ExecuteProgram( func, name )	vm->ExecuteProgram( func, name, __FILE__, __LINE__ )

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
void VM_AddCommand( void );
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
#define MAX_INFO_KEY	64
#define MAX_INFO_VALUE	64
#define MAX_INFO_STRING	512

enum e_trace
{
	MOVE_NORMAL = 0,
	MOVE_NOMONSTERS,
	MOVE_MISSILE,
	MOVE_WORLDONLY,
	MOVE_HITMODEL,
};

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
void CL_MouseEvent( int mx, int my, int time );
void CL_AddLoopingSounds( void );
void CL_RegisterSounds( void );
void CL_Drop( void );
char *Info_ValueForKey( char *s, char *key );
void Info_RemoveKey( char *s, char *key );
void Info_SetValueForKey( char *s, char *key, char *value );
bool Info_Validate( char *s );
void Info_Print( char *s );
char *Cvar_Userinfo( void );
char *Cvar_Serverinfo( void );
void Cmd_WriteVariables( file_t *f );
bool Cmd_CheckMapsList( void );
bool Cmd_CheckName( const char *name );

typedef struct autocomplete_list_s
{
	const char *name;
	bool (*func)( const char *s, char *name, int length );
} autocomplete_list_t;

extern autocomplete_list_t cmd_list[];

#endif//COMMON_H