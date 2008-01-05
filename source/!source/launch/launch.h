//=======================================================================
//			Copyright XashXT Group 2007 ©
//			launch.h - launch.dll main header
//=======================================================================
#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <limits.h>
#include <fcntl.h>
#include <direct.h>
#include <sys/stat.h>
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <time.h>
#include <winreg.h>

#define LAUNCH_DLL		// skip alias names
#include "basetypes.h"

#define XASH_VERSION		0.48f // current version will be shared over gameinfo struct

typedef struct system_s
{
	char			progname[MAX_QPATH];
	int			app_name;

	bool			debug;
	bool			developer;
	bool			log_active;
	char			log_path[MAX_SYSPATH];
	bool			hooked_out;
	bool			stuffcmdsrun;

	HINSTANCE			hInstance;
	LPTOP_LEVEL_EXCEPTION_FILTER	oldFilter;		
	dll_info_t		*linked_dll;

	char			caption[MAX_QPATH];
	bool			con_readonly;
	bool			con_showalways;
	bool			con_showcredits;
	bool			con_silentmode;
	bool			error;
	bool			crash;
	byte			*basepool;
	byte			*zonepool;
	byte			*imagepool;
	byte			*stringpool;

	// simply profiling
	double			start, end;

	void (*Con_Print)( const char *msg );
	void ( *Init ) ( uint funcname, int argc, char **argv );
	void ( *Main ) ( void ); // host frame
	void ( *Free ) ( void ); // close host
	void ( *Cmd  ) ( void ); // cmd forward to server
	void (*CPrint)( const char *msg ); // console print
} system_t;

typedef struct cvar_s
{
	char	*name;
	char	*string;		// normal string
	float	value;		// atof( string )
	int	integer;		// atoi( string )
	bool	modified;		// set each time the cvar is changed

	char	*reset_string;	// cvar_restart will reset to this value
	char	*latched_string;	// for CVAR_LATCH vars
	char	*description;	// variable descrition info
	uint	flags;		// state flags
	uint	modificationCount;	// incremented each time the cvar is changed

	struct cvar_s *next;
	struct cvar_s *hash;
};

extern system_t Sys;
extern gameinfo_t GI;
extern stdlib_api_t	com;

//
// console.c
//
void Con_ShowConsole( bool show );
void Con_PrintA(const char *pMsg);
void Con_PrintW(const char *pMsg);
void Con_CreateConsole( void );
void Con_DestroyConsole( void );
char *Sys_Input( void );
void Con_RegisterHotkeys( void );

//
// system.c
//
void Sys_InitCPU( void );
gameinfo_t Sys_GameInfo( void );
uint Sys_SendKeyEvents( void );
void Sys_ParseCommandLine (LPSTR lpCmdLine);
void Sys_LookupInstance( void );
double Sys_DoubleTime( void );
char *Sys_GetClipboardData( void );
void Sys_Sleep( int msec );
void Sys_Init( void );
void Sys_Exit( void );
bool Sys_LoadLibrary ( dll_info_t *dll );
void* Sys_GetProcAddress ( dll_info_t *dll, const char* name );
bool Sys_FreeLibrary ( dll_info_t *dll );
void Sys_WaitForQuit( void );
void Sys_InitLog( void );
void Sys_CloseLog( void );
void Sys_Error(const char *error, ...);
void Sys_Break(const char *error, ...);
void Sys_PrintLog( const char *pMsg );
void Sys_Print(const char *pMsg);
void Sys_Msg( const char *pMsg, ... );
void Sys_MsgWarn( const char *pMsg, ... );
void Sys_MsgDev( int level, const char *pMsg, ... );
int Sys_GetThreadWork( void );
void Sys_ThreadWorkerFunction (int threadnum);
void Sys_ThreadSetDefault (void);
void Sys_ThreadLock( void );
void Sys_ThreadUnlock( void );
int Sys_GetNumThreads( void );
void Sys_RunThreadsOnIndividual(int workcnt, bool showpacifier, void(*func)(int));
void Sys_RunThreadsOn (int workcnt, bool showpacifier, void(*func)(int));

#define Msg Sys_Msg
#define MsgDev Sys_MsgDev
#define MsgWarn Sys_MsgWarn

// registry common tools
bool REG_GetValue( HKEY hKey, const char *SubKey, const char *Value, char *pBuffer);
bool REG_SetValue( HKEY hKey, const char *SubKey, const char *Value, char *pBuffer);

//
// stdlib.c
//
void com_strnupr(const char *in, char *out, size_t size_out);
void com_strupr(const char *in, char *out);
void com_strnlwr(const char *in, char *out, size_t size_out);
void com_strlwr(const char *in, char *out);
int com_strlen( const char *string );
int com_cstrlen( const char *string );
char com_toupper(const char in );
char com_tolower(const char in );
size_t com_strncat(char *dst, const char *src, size_t siz);
size_t com_strcat(char *dst, const char *src );
size_t com_strncpy(char *dst, const char *src, size_t siz);
size_t com_strcpy(char *dst, const char *src );
char *com_stralloc(const char *s, const char *filename, int fileline);
int com_atoi(const char *str);
float com_atof(const char *str);
void com_atov( float *vec, const char *str, size_t siz );
char *com_strchr(const char *s, char c);
char *com_strrchr(const char *s, char c);
int com_strnicmp(const char *s1, const char *s2, int n);
int com_stricmp(const char *s1, const char *s2);
int com_strncmp (const char *s1, const char *s2, int n);
int com_strcmp (const char *s1, const char *s2);
const char* com_timestamp( int format );
char *com_stristr( const char *string, const char *string2 );
size_t com_strpack( byte *buffer, size_t pos, char *string, int n );
size_t com_strunpack( byte *buffer, size_t pos, char *string );
int com_vsnprintf(char *buffer, size_t buffersize, const char *format, va_list args);
int com_vsprintf(char *buffer, const char *format, va_list args);
int com_snprintf(char *buffer, size_t buffersize, const char *format, ...);
int com_sprintf(char *buffer, const char *format, ...);
char *com_pretifymem( float value, int digitsafterdecimal );
char *va(const char *format, ...);
#define copystring(str)	com_stralloc(str, __FILE__, __LINE__)

//
// memlib.c
//
void Memory_Init( void );
void Memory_Shutdown( void );
void Memory_Init_Commands( void );
void _mem_move(byte *poolptr, void **dest, void *src, size_t size, const char *filename, int fileline);
void *_mem_realloc(byte *poolptr, void *memptr, size_t size, const char *filename, int fileline);
void _mem_copy(void *dest, const void *src, size_t size, const char *filename, int fileline);
void _mem_set(void *dest, int set, size_t size, const char *filename, int fileline);
void *_mem_alloc(byte *poolptr, size_t size, const char *filename, int fileline);
byte *_mem_allocpool(const char *name, const char *filename, int fileline);
void _mem_freepool(byte **poolptr, const char *filename, int fileline);
void _mem_emptypool(byte *poolptr, const char *filename, int fileline);
void _mem_free(void *data, const char *filename, int fileline);
byte *_mem_alloc_array( byte *poolptr, size_t recordsize, int count, const char *filename, int fileline );
void _mem_free_array( byte *arrayptr, const char *filename, int fileline );
void *_mem_alloc_array_element( byte *arrayptr, const char *filename, int fileline );
void _mem_free_array_element( byte *arrayptr, void *element, const char *filename, int fileline );
void *_mem_get_array_element( byte *arrayptr, size_t index );
size_t _mem_array_size( byte *arrayptr );
void _mem_check(const char *filename, int fileline);
bool _is_allocated( byte *poolptr, void *data );

#define Mem_Alloc(pool, size) _mem_alloc(pool, size, __FILE__, __LINE__)
#define Mem_Realloc(pool, ptr, size) _mem_realloc(pool, ptr, size, __FILE__, __LINE__)
#define Mem_Free(mem) _mem_free(mem, __FILE__, __LINE__)
#define Mem_AllocPool(name) _mem_allocpool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) _mem_freepool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) _mem_emptypool(pool, __FILE__, __LINE__)
#define Mem_Move(pool, dest, src, size ) _mem_move(pool, dest, src, size, __FILE__, __LINE__)
#define Mem_Copy(dest, src, size ) _mem_copy(dest, src, size, __FILE__, __LINE__)
#define Mem_Set(dest, src, size ) _mem_set(dest, src, size, __FILE__, __LINE__)
#define Mem_CreateArray( p, s, n ) _mem_alloc_array( p, s, n, __FILE__, __LINE__)
#define Mem_RemoveArray( array ) _mem_free_array( array, __FILE__, __LINE__)
#define Mem_AllocElement( array ) _mem_alloc_array_element( array, __FILE__, __LINE__)
#define Mem_FreeElement( array, el ) _mem_free_array_element( array, el, __FILE__, __LINE__ )
#define Mem_GetElement( array, idx ) _mem_get_array_element( array, idx )
#define Mem_ArraySize( array ) _mem_array_size( array )
#define Mem_IsAllocated( mem ) _is_allocated( NULL, mem )
#define Mem_Check() _mem_check(__FILE__, __LINE__)
#define Mem_Pretify( x ) com_pretifymem(x, 3)
#define Malloc( size ) Mem_Alloc( Sys.basepool, size )

//
// filesystem.c
//
void FS_Init( void );
void FS_Path( void );
void FS_Shutdown( void );
void FS_InitEditor( void );
void FS_InitRootDir( char *path );
void FS_ClearSearchPath (void);
void FS_AddGameHierarchy (const char *dir);
int FS_CheckParm (const char *parm);
void FS_LoadGameInfo( const char *filename );
void FS_FileBase( const char *in, char *out);
const char *FS_FileExtension (const char *in);
void FS_DefaultExtension (char *path, const char *extension );
bool FS_GetParmFromCmdLine( char *parm, char *out );
void FS_ExtractFilePath(const char* const path, char* dest);
void FS_UpdateEnvironmentVariables( void );
const char *FS_FileWithoutPath (const char *in);
extern char sys_rootdir[];
extern char *fs_argv[];
extern int fs_argc;

// simply files managment interface
byte *FS_LoadFile (const char *path, fs_offset_t *filesizeptr );
bool FS_WriteFile (const char *filename, void *data, fs_offset_t len);
rgbdata_t *FS_LoadImage(const char *filename, char *data, int size );
void FS_SaveImage(const char *filename, rgbdata_t *buffer );
void FS_FreeImage( rgbdata_t *pack );
bool Image_Processing( const char *name, rgbdata_t **pix, int width, int height );
search_t *FS_Search(const char *pattern, int caseinsensitive );
search_t *FS_SearchDirs(const char *pattern, int caseinsensitive );

// files managment (like fopen, fread etc)
file_t *FS_Open (const char* filepath, const char* mode );
file_t* _FS_Open (const char* filepath, const char* mode, bool quiet, bool nonblocking);
fs_offset_t FS_Write (file_t* file, const void* data, size_t datasize);
fs_offset_t FS_Read (file_t* file, void* buffer, size_t buffersize);
int FS_VPrintf(file_t* file, const char* format, va_list ap);
int FS_Seek (file_t* file, fs_offset_t offset, int whence);
int FS_Gets (file_t* file, byte *string, size_t bufsize );
int FS_Printf(file_t* file, const char* format, ...);
fs_offset_t FS_FileSize (const char *filename);
int FS_Print(file_t* file, const char *msg);
bool FS_FileExists (const char *filename);
int FS_UnGetc (file_t* file, byte c);
void FS_StripExtension (char *path);
fs_offset_t FS_Tell (file_t* file);
void FS_Purge (file_t* file);
int FS_Close (file_t* file);
int FS_Getc (file_t* file);
bool FS_Eof( file_t* file);

//
// cvar.c
//
cvar_t *Cvar_FindVar (const char *var_name);
cvar_t *Cvar_Get (const char *var_name, const char *value, int flags, const char *description);
void Cvar_Set( const char *var_name, const char *value);
cvar_t *Cvar_Set2 (const char *var_name, const char *value, bool force);
void Cvar_LookupVars( int checkbit, char *buffer, void *ptr, cvarcmd_t callback );
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
char *Info_ValueForKey( char *s, char *key );
void Info_RemoveKey( char *s, char *key );
void Info_SetValueForKey( char *s, char *key, char *value );
bool Info_Validate( char *s );
void Info_Print( char *s );
extern cvar_t *cvar_vars;

//
// cmd.c
//
void Cbuf_Init( void );
void Cbuf_AddText (const char *text);
void Cbuf_InsertText (const char *text);
void Cbuf_ExecuteText (int exec_when, const char *text);
void Cbuf_Execute (void);
uint Cmd_Argc( void );
char *Cmd_Args( void );
char *Cmd_Argv( uint arg );
void Cmd_Init( void );
void Cmd_AddCommand(const char *cmd_name, xcommand_t function, const char *cmd_desc);
void Cmd_RemoveCommand(const char *cmd_name);
bool Cmd_Exists (const char *cmd_name);
void Cmd_LookupCmds( char *buffer, void *ptr, cvarcmd_t callback );
bool Cmd_GetMapList( const char *s, char *completedname, int length );
bool Cmd_GetDemoList( const char *s, char *completedname, int length );
bool Cmd_GetMovieList (const char *s, char *completedname, int length );
void Cmd_TokenizeString (const char *text);
void Cmd_ExecuteString (const char *text);
void Cmd_ForwardToServer (void);

// virtual files managment
vfile_t *VFS_Create(byte *buffer, size_t buffsize);
vfile_t *VFS_Open(file_t *handle, const char* mode);
fs_offset_t VFS_Write( vfile_t *file, const void *buf, size_t size );
fs_offset_t VFS_Read(vfile_t* file, void* buffer, size_t buffersize);
int VFS_Print(vfile_t* file, const char *msg);
int VFS_Printf(vfile_t* file, const char* format, ...);
int VFS_Seek( vfile_t *file, fs_offset_t offset, int whence );
int VFS_Gets(vfile_t* file, byte *string, size_t bufsize );
bool VFS_Unpack( void* compbuf, size_t compsize, void **buf, size_t size );
fs_offset_t VFS_Tell (vfile_t* file);
file_t *VFS_Close( vfile_t *file );

//
// crclib.c
//
void CRC_Init(word *crcvalue);
word CRC_Block (byte *start, int count);
void CRC_ProcessByte(word *crcvalue, byte data);
byte CRC_BlockSequence(byte *base, int length, int sequence);
uint Com_BlockChecksum (void *buffer, int length);
uint Com_BlockChecksumKey(void *buffer, int length, int key);

//
// parselib.c
//
bool SC_LoadScript( const char *filename, char *buf, int size );
bool SC_AddScript( const char *filename, char *buf, int size );
bool SC_FilterToken(char *filter, char *name, int casecmp);
bool SC_ParseToken_Simple(const char **data_p);
char *SC_ParseToken(const char **data_p );
char *SC_ParseWord( const char **data_p );
bool SC_MatchToken( const char *match );
void SC_ResetScript( void );
void SC_SkipToken( void );
void SC_FreeToken( void );
bool SC_TryToken( void );
char *SC_GetToken( bool newline );
char *SC_Token( void );
extern char token[];

//
// imglib.c
//
void Image_Init( void );
void Image_Shutdown( void );

#endif//LAUNCHER_H