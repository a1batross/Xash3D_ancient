//=======================================================================
//			Copyright XashXT Group 2007 ©
//			engine.h - engine.dll main header
//=======================================================================
#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <winreg.h>
#include <fcntl.h>
#include <basetypes.h>
typedef int bool;

#include <ref_system.h>

enum
{
	ERR_INVALID_ROOT,
	ERR_CONSOLE_FAIL,
	ERR_OSINFO_FAIL,
	ERR_INVALID_VER,
	ERR_WINDOWS_32S,
};

// import variables
char *(*Sys_Input ) ( void );	
void ( *Msg )( char *msg, ... );
void ( *Sys_Print )( const char *msg );
void ( *Sys_InitConsole )( void );
void ( *Sys_FreeConsole )( void );
void ( *MsgDev )( int level, char *msg, ... );
void ( *MsgWarn )( char *msg, ... );
void ( *Sys_Error )( char *msg, ... );
void ( *Sys_ShowConsole )( bool show );
void Sys_Init( void ); // static
void Sys_Exit( void ); // static
void Sys_Sleep( int msec);
bool Sys_LoadLibrary ( dll_info_t *dll ); // load library 
bool Sys_FreeLibrary ( dll_info_t *dll ); // free library
void* Sys_GetProcAddress ( dll_info_t *dll, const char* name );
void Sys_WaitForQuit( void ); // waiting for 'ESC' or close command
long WINAPI Sys_ExecptionFilter( PEXCEPTION_POINTERS pExceptionInfo );

// export variables
void ( *Host_Init ) ( char *funcname, int argc, char **argv ); // init host
void ( *Host_Main ) ( void );	// host frame
void ( *Host_Free ) ( void );	// close host

extern HINSTANCE	base_hInstance;
extern dll_info_t	*linked_dll;
extern bool debug_mode;
extern bool log_active;
extern bool hooked_out;
extern int dev_mode;
extern int com_argc;
extern LPTOP_LEVEL_EXCEPTION_FILTER oldFilter;
extern char *com_argv[MAX_NUM_ARGVS];
extern char sys_rootdir[ MAX_SYSPATH ];
extern char log_path[256];
extern bool console_read_only;
extern bool show_always;
extern bool about_mode;
extern bool silent_mode;
extern bool sys_error;
char *va(const char *format, ...);

//
// utils.c
//
const char* Log_Timestamp( void );
int CheckParm (const char *parm);
void ParseCommandLine (LPSTR lpCmdLine);
void UpdateEnvironmentVariables( void );
bool _GetParmFromCmdLine( char *parm, char *out, size_t size );
#define GetParmFromCmdLine( parm, out ) _GetParmFromCmdLine( parm, out, sizeof(out)) 
float CalcEngineVersion( void );
float CalcEditorVersion( void );

//
// console.c
//
void Sys_PrintA(const char *pMsg);
void Sys_PrintW(const char *pMsg);
void Sys_MsgW( const char *pMsg, ... );
void Sys_MsgDevW( int level, const char *pMsg, ... );
void Sys_MsgWarnW( const char *pMsg, ... );
void Sys_CreateConsoleW( void );
void Sys_DestroyConsoleW( void );
void Sys_ShowConsoleW( bool show );
char *Sys_InputW( void );
void Sys_ErrorW(char *error, ...);
void Sys_ErrorA(char *error, ...);
void Sys_InitLog( void );
void Sys_CloseLog( void );
void _Sys_ErrorFatal( int type, const char *filename, int fileline );

//generic stub
__inline void NullVoid( void ) {}
__inline void NullVoidWithArg( bool parm ) {}
__inline void NullVarArgs( char *parm, ... ) {}
__inline void NullVarArgs2( int level, char *parm, ... ) {}
__inline char *NullChar( void ) { return NULL; }
__inline void NullVoidWithName( const char *caption ) {}
__inline void NullInit ( char *funcname, int argc, char **argv ) {}

//memory manager
#define Mem_Alloc(pool, size) Com->Mem.Alloc(pool, size, __FILE__, __LINE__)
#define Mem_Realloc(pool, ptr, size) Com->Mem.Realloc(pool, ptr, size, __FILE__, __LINE__)
#define Mem_Free(mem) Com->Mem.Free(mem, __FILE__, __LINE__)
#define Mem_AllocPool(name) Com->Mem.AllocPool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) Com->Mem.FreePool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) Com->Mem.EmptyPool(pool, __FILE__, __LINE__)
#define Mem_Move(dest, src, size ) Com->Mem.Move (dest, src, size, __FILE__, __LINE__)
#define Mem_Copy(dest, src, size ) Com->Mem.Copy (dest, src, size, __FILE__, __LINE__)
#define Mem_Check() Com->Mem.CheckSentinelsGlobal(__FILE__, __LINE__)
#define Sys_ErrorFatal( type ) _Sys_ErrorFatal( type, __FILE__, __LINE__ )
#define Sys_Stop() Sys_ErrorFatal( -1 )

#endif//LAUNCHER_H