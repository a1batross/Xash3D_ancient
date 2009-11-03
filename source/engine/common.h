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

#include "launch_api.h"
#include "qfiles_ref.h"
#include "engine_api.h"
#include "entity_def.h"
#include "physic_api.h"
#include "vprogs_api.h"
#include "vsound_api.h"
#include "net_msg.h"

// linked interfaces
extern stdlib_api_t		com;
extern physic_exp_t		*pe;
extern vprogs_exp_t		*vm;
extern vsound_exp_t		*se;

#define MAX_ENTNUMBER	99999		// for server and client parsing
#define MAX_HEARTBEAT	-99999		// connection time
#define MAX_EVENTS		1024		// system events

// some engine shared constants
#define DEFAULT_MAXVELOCITY	"2000"
#define DEFAULT_GRAVITY	"800"
#define DEFAULT_ROLLSPEED	"200"
#define DEFAULT_ROLLANGLE	"2"
#define DEFAULT_STEPHEIGHT	"18"
#define DEFAULT_AIRACCEL	"0"
#define DEFAULT_MAXSPEED	"320"
#define DEFAULT_ACCEL	"10"
#define DEFAULT_FRICTION	"4"

/*
==============================================================

		SCREEN GLOBAL INFO

==============================================================
*/
#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480
extern cvar_t		*scr_width;
extern cvar_t		*scr_height;

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
	HOST_RESTART,	// during the changes video mode
} host_state;

typedef enum
{
	RD_NONE = 0,
	RD_CLIENT,
	RD_PACKET,

} rdtype_t;

typedef struct host_redirect_s
{
	rdtype_t		target;
	char		*buffer;
	int		buffersize;
	netadr_t		address;
	void		(*flush)( netadr_t adr, rdtype_t target, char *buffer );
} host_redirect_t;

typedef struct host_parm_s
{
	host_state	state;		// global host state
	uint		type;		// running at
	host_redirect_t	rd;		// remote console
	jmp_buf		abortframe;	// abort current frame
	dword		errorframe;	// to avoid each-frame host error
	string		finalmsg;		// server shutdown final message

	int		frametime[2];	// time between engine frames
	uint		framecount;	// global framecount

	int		events_head;
	int		events_tail;
	sys_event_t	events[MAX_EVENTS];

	HWND		hWnd;		// main window
	int		developer;	// show all developer's message
} host_parm_t;

extern host_parm_t	host;

//
// host.c
//
void Host_Init( const int argc, const char **argv );
void Host_Main( void );
void Host_Free( void );
void Host_SetServerState( int state );
int Host_ServerState( void );
int Host_CompareFileTime( long ft1, long ft2 );
void Host_EndGame( const char *message, ... );
void Host_AbortCurrentFrame( void );
void Host_WriteDefaultConfig( void );
void Host_WriteConfig( void );
void Host_ShutdownServer( void );
void Host_CheckChanges( void );
int Host_Milliseconds( void );
void Host_Print( const char *txt );
void Host_Error( const char *error, ... );

/*
==============================================================

CLIENT / SERVER SYSTEMS

==============================================================
*/
void CL_Init( void );
void CL_Shutdown( void );
void CL_Frame( int time );
bool CL_Active( void );

void SV_Init( void );
void SV_Shutdown( bool reconnect );
void SV_Frame( int time );
bool SV_Active( void );

/*
==============================================================

	SHARED ENGFUNCS

==============================================================
*/

//
// obsolete vm stuff
//
#define VM_Frame		vm->Update


void pfnMemCopy( void *dest, const void *src, size_t cb, const char *filename, const int fileline );
cvar_t *pfnCVarRegister( const char *szName, const char *szValue, int flags, const char *szDesc );
byte* pfnLoadFile( const char *filename, int *pLength );
void pfnFreeFile( void *buffer );
int pfnFileExists( const char *filename );
long pfnRandomLong( long lLow, long lHigh );
float pfnRandomFloat( float flLow, float flHigh );
void pfnAlertMessage( ALERT_TYPE level, char *szFmt, ... );
void *pfnFOpen( const char* path, const char* mode );
long pfnFWrite( void *file, const void* data, size_t datasize );
long pfnFRead( void *file, void* buffer, size_t buffersize );
int pfnFGets( void *file, byte *string, size_t bufsize );
int pfnFSeek( void *file, long offset, int whence );
int pfnFClose( void *file );
long pfnFTell( void *file );
void pfnGetGameDir( char *szGetGameDir );
float pfnTime( void );

/*
==============================================================

	MISC COMMON FUNCTIONS

==============================================================
*/
#define MAX_INFO_STRING	512

extern byte *zonepool;

#define Z_Malloc(size)		Mem_Alloc( zonepool, size )
#define Z_Realloc( ptr, size )	Mem_Realloc( zonepool, ptr, size )
#define Z_Free( ptr )		if( ptr ) Mem_Free( ptr )

//
// physic.dll exports
//
#define Mod_GetBounds	if( pe ) pe->Mod_GetBounds
#define Mod_GetFrames	if( pe ) pe->Mod_GetFrames
#define CM_RegisterModel	if( pe ) pe->RegisterModel

_inline int CM_LeafArea( int leafnum )
{
	if( !pe )	return -1;
	return pe->LeafArea( leafnum );
}		

_inline int CM_LeafCluster( int leafnum )
{
	if( !pe )	return -1;
	return pe->LeafCluster( leafnum );
}

_inline int CM_BoxLeafnums( vec3_t mins, vec3_t maxs, int *list, int listsize, int *lastleaf )
{
	if( !pe )
	{
		if( lastleaf ) *lastleaf = 0;
		return 0;
	}
	return pe->BoxLeafnums( mins, maxs, list, listsize, lastleaf );
}

_inline void *Mod_Extradata( model_t modelIndex )
{
	if( !pe )	return NULL;
	return pe->Mod_Extradata( modelIndex );
}

void CL_GetEntitySoundSpatialization( int ent, vec3_t origin, vec3_t velocity );
bool SV_GetComment( char *comment, int savenum );
void CL_MouseEvent( int mx, int my );
void CL_AddLoopingSounds( void );
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

typedef struct autocomplete_list_s
{
	const char *name;
	bool (*func)( const char *s, char *name, int length );
} autocomplete_list_t;

extern autocomplete_list_t cmd_list[];

#endif//COMMON_H