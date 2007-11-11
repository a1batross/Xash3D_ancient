//=======================================================================
//			Copyright XashXT Group 2007 ©
//			launch.h - launch.dll main header
//=======================================================================
#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <winreg.h>
#include <fcntl.h>
#include <basetypes.h>

#include <ref_system.h>

enum
{
	ERR_INVALID_ROOT,
	ERR_CONSOLE_FAIL,
	ERR_OSINFO_FAIL,
	ERR_INVALID_VER,
	ERR_WINDOWS_32S,
};

enum 
{
	DEFAULT =	0,	// host_init( funcname *arg ) same much as:
	HOST_SHARED,	// "host_shared"
	HOST_DEDICATED,	// "host_dedicated"
	HOST_EDITOR,	// "host_editor"
	BSPLIB,		// "bsplib"
	IMGLIB,		// "imglib"
	QCCLIB,		// "qcclib"
	ROQLIB,		// "roqlib"
	SPRITE,		// "sprite"
	STUDIO,		// "studio"
	CREDITS,		// "splash"
	HOST_INSTALL,	// "install"
};

typedef struct system_s
{
	char			progname[MAX_QPATH];
	int			app_name;

	bool			debug;
	bool			developer;
	bool			log_active;
	char			log_path[MAX_SYSPATH];
	bool			hooked_out;

	HINSTANCE			hInstance;
	LPTOP_LEVEL_EXCEPTION_FILTER	oldFilter;		
	dll_info_t		*linked_dll;

	char			caption[MAX_QPATH];
	bool			con_readonly;
	bool			con_showalways;
	bool			con_showcredits;
	bool			con_silentmode;
	bool			error;
	byte			*basepool;
	byte			*zonepool;

	// simply profiling
	double			start, end;

	void (*Con_Print)( const char *msg );
	void ( *Init ) ( char *funcname, int argc, char **argv );
	void ( *Main ) ( void ); // host frame
	void ( *Free ) ( void ); // close host
} system_t;

extern system_t sys;
extern gameinfo_t GI;

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
void _Sys_ErrorFatal( int type, const char *filename, int fileline );
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
void Sys_Error(char *error, ...);
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

#define Sys_ErrorFatal( type ) _Sys_ErrorFatal( type, __FILE__, __LINE__ )
#define Sys_Stop() Sys_ErrorFatal( -1 )
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
char *com_stralloc(const char *s);
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
char *va(const char *format, ...);

//
// memlib.c
//
void Memory_Init( void );
void Memory_Shutdown( void );
void _mem_move(byte *poolptr, void **dest, void *src, size_t size, const char *filename, int fileline);
void *_mem_realloc(byte *poolptr, void *memptr, size_t size, const char *filename, int fileline);
void _mem_copy(void *dest, const void *src, size_t size, const char *filename, int fileline);
void _mem_set(void *dest, int set, size_t size, const char *filename, int fileline);
void *_mem_alloc(byte *poolptr, size_t size, const char *filename, int fileline);
byte *_mem_allocpool(const char *name, const char *filename, int fileline);
void _mem_freepool(byte **poolptr, const char *filename, int fileline);
void _mem_emptypool(byte *poolptr, const char *filename, int fileline);
void _mem_free(void *data, const char *filename, int fileline);
void _mem_check(const char *filename, int fileline);

#define Mem_Alloc(pool, size) _mem_alloc(pool, size, __FILE__, __LINE__)
#define Mem_Realloc(pool, ptr, size) _mem_realloc(pool, ptr, size, __FILE__, __LINE__)
#define Mem_Free(mem) _mem_free(mem, __FILE__, __LINE__)
#define Mem_AllocPool(name) _mem_allocpool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) _mem_freepool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) _mem_emptypool(pool, __FILE__, __LINE__)
#define Mem_Move(dest, src, size ) _mem_move (dest, src, size, __FILE__, __LINE__)
#define Mem_Copy(dest, src, size ) _mem_copy(dest, src, size, __FILE__, __LINE__)
#define Mem_Set(dest, src, size ) _mem_set(dest, src, size, __FILE__, __LINE__)
#define Mem_Check() _mem_check(__FILE__, __LINE__)

#define Malloc( size )	Mem_Alloc( sys.basepool, size )

//
// filesystem.c
//
filesystem_api_t FS_GetAPI( void );
void FS_Init( void );
void FS_Path( void );
void FS_Shutdown( void );
void FS_InitEditor( void );
void FS_InitRootDir( char *path );
void FS_ClearSearchPath (void);
void FS_AddGameHierarchy (const char *dir);
int FS_CheckParm (const char *parm);
void FS_LoadGameInfo( const char *filename );
void FS_FileBase (char *in, char *out);
const char *FS_FileExtension (const char *in);
void FS_DefaultExtension (char *path, const char *extension );
bool FS_GetParmFromCmdLine( char *parm, char *out );
void FS_UpdateEnvironmentVariables( void );
extern char sys_rootdir[];
extern char *fs_argv[];
extern int fs_argc;

// simply files managment interface
byte *FS_LoadFile (const char *path, fs_offset_t *filesizeptr );
bool FS_WriteFile (const char *filename, void *data, fs_offset_t len);
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

// virtual files managment
vfilesystem_api_t VFS_GetAPI( void );
vfile_t *VFS_Open(const char *filename, const char* mode);
fs_offset_t VFS_Write( vfile_t *file, const void *buf, size_t size );
fs_offset_t VFS_Read(vfile_t* file, void* buffer, size_t buffersize);
int VFS_Seek( vfile_t *file, fs_offset_t offset, int whence );
fs_offset_t VFS_Tell (vfile_t* file);
int VFS_Close( vfile_t *file );

//
// crclib.c
//
void CRC_Init(word *crcvalue);
word CRC_Block (byte *start, int count);
void CRC_ProcessByte(word *crcvalue, byte data);
byte CRC_BlockSequence(byte *base, int length, int sequence);

//
// parselib.c
//
scriptsystem_api_t Sc_GetAPI( void );
bool SC_LoadScript( const char *filename, char *buf, int size );
bool SC_FilterToken(char *filter, char *name, int casecmp);
char *SC_ParseToken(const char **data_p );
char *SC_ParseWord( const char **data_p );
bool SC_MatchToken( const char *match );
void SC_SkipToken( void );
void SC_FreeToken( void );
bool SC_TryToken( void );
char *SC_GetToken( bool newline );
char *SC_Token( void );

#endif//LAUNCHER_H