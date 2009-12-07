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
#include "render_api.h"
#include "physic_api.h"
#include "vsound_api.h"
#include "safeproc.h"
#include "net_msg.h"

#define MAX_ENTNUMBER	99999		// for server and client parsing
#define MAX_HEARTBEAT	-99999		// connection time

/*
==============================================================

		SCREEN GLOBAL INFO

==============================================================
*/
#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480
extern cvar_t		*scr_width;
extern cvar_t		*scr_height;
extern cvar_t		*allow_download;

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
	byte		*mempool;		// static mempool for misc allocations
	string		finalmsg;		// server shutdown final message

	int		frametime;	// time between engine frames
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
void Host_SetServerState( int state );
int Host_ServerState( void );
int Host_CompareFileTime( long ft1, long ft2 );
void Host_EndGame( const char *message, ... );
void Host_AbortCurrentFrame( void );
void Host_WriteDefaultConfig( void );
void Host_WriteConfig( void );
void Host_ShutdownServer( void );
void Host_CheckChanges( void );
void Host_CheckRestart( void );
int Host_Milliseconds( void );
void Host_Print( const char *txt );
void Host_Error( const char *error, ... );
void Host_Credits( void );

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
cvar_t *pfnCVarRegister( const char *szName, const char *szValue, int flags, const char *szDesc );
char *pfnMemFgets( byte *pMemFile, int fileSize, int *filePos, char *pBuffer, int bufferSize );
byte* pfnLoadFile( const char *filename, int *pLength );
char *pfnParseToken( const char **data_p );
void pfnCVarSetString( const char *szName, const char *szValue );
void pfnCVarSetValue( const char *szName, float flValue );
float pfnCVarGetValue( const char *szName );
char* pfnCVarGetString( const char *szName );
void pfnFreeFile( void *buffer );
int pfnFileExists( const char *filename );
void *pfnLoadLibrary( const char *name );
void *pfnGetProcAddress( void *hInstance, const char *name );
void pfnFreeLibrary( void *hInstance );
long pfnRandomLong( long lLow, long lHigh );
float pfnRandomFloat( float flLow, float flHigh );
void pfnAlertMessage( ALERT_TYPE level, char *szFmt, ... );
void *pfnFOpen( const char* path, const char* mode );
long pfnFWrite( void *file, const void* data, size_t datasize );
long pfnFRead( void *file, void* buffer, size_t buffersize );
int pfnFGets( void *file, byte *string, size_t bufsize );
void pfnEngineFprintf( void *pfile, char *szFmt, ... );
int pfnFSeek( void *file, long offset, int whence );
int pfnFClose( void *file );
long pfnFTell( void *file );
void pfnRemoveFile( const char *szFilename );
const char *pfnCmd_Args( void );
const char *pfnCmd_Argv( int argc );
int pfnCmd_Argc( void );
float pfnTime( void );

/*
==============================================================

	MISC COMMON FUNCTIONS

==============================================================
*/
#define MAX_INFO_STRING	512

#define Z_Malloc(size)		Mem_Alloc( host.mempool, size )
#define Z_Realloc( ptr, size )	Mem_Realloc( host.mempool, ptr, size )
#define Z_Free( ptr )		if( ptr ) Mem_Free( ptr )

//
// keys.c
//
bool Key_IsDown( int keynum );
char *Key_IsBind( int keynum );
void Key_Event( int key, bool down, int time );
void Key_Init( void );
void Key_WriteBindings( file_t *f );
void Key_SetBinding( int keynum, char *binding );
void Key_ClearStates( void );
char *Key_KeynumToString( int keynum );
int Key_StringToKeynum( char *str );
int Key_GetKey( const char *binding );
void Key_EnumCmds_f( void );
void Key_SetKeyDest( int key_dest );

//
// cinematic.c
//
void CIN_Init( void );
void CIN_ReadChunk( cinematics_t *cin );
byte *CIN_ReadNextFrame( cinematics_t *cin, bool silent );

int CL_GetServerTime( void );
float CL_GetLerpFrac( void );
void CL_CharEvent( int key );
int CL_PointContents( const vec3_t point );
void CL_StudioFxTransform( edict_t *ent, float transform[4][4] );
void CL_GetEntitySoundSpatialization( int ent, vec3_t origin, vec3_t velocity );
bool CL_GetAttachment( int entityIndex, int number, vec3_t origin, vec3_t angles );
bool CL_SetAttachment( int entityIndex, int number, vec3_t origin, vec3_t angles );
void CL_StudioEvent( dstudioevent_t *event, edict_t *ent );
studioframe_t *CL_GetStudioFrame( int entityIndex );
edict_t *CL_GetEdictByIndex( int index );
edict_t *CL_GetLocalPlayer( void );
int CL_GetMaxClients( void );
byte CL_GetMouthOpen( int entityIndex );
bool SV_GetComment( char *comment, int savenum );
void SV_ForceMod( void );
void CL_MouseEvent( int mx, int my );
void CL_AddLoopingSounds( void );
void CL_Disconnect( void );
bool CL_NextDemo( void );
void CL_Drop( void );
void CL_ForceVid( void );
void CL_ForceSnd( void );
void SCR_Init( void );
void SCR_UpdateScreen( void );
void SCR_Shutdown( void );
void Con_Print( const char *txt );
char *Info_ValueForKey( const char *s, const char *key );
void Info_RemovePrefixedKeys( char *start, char prefix );
bool Info_RemoveKey( char *s, const char *key );
bool Info_SetValueForKey( char *s, const char *key, const char *value );
bool Info_Validate( const char *s );
void Info_Print( const char *s );
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