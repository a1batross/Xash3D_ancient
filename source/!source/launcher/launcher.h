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
#include <ref_system.h>

//import variables
char *(*Sys_Input ) ( void );	
void ( *Msg )( char *msg, ... );
void ( *Sys_Print )( char *msg );
void ( *Sys_InitConsole )( void );
void ( *Sys_FreeConsole )( void );
void ( *MsgDev )( char *msg, ... );
void ( *MsgWarn )( char *msg, ... );
void ( *Sys_Error )( char *msg, ... );
void ( *Sys_ShowConsole )( bool show );
void Sys_Exit( void ); //static

//export variables
void ( *Host_Init ) ( char *funcname, int argc, char **argv ); //init host
void ( *Host_Main ) ( void );	//host frame
void ( *Host_Free ) ( void );	//close host

extern HINSTANCE base_hInstance;
extern HINSTANCE linked_dll;
extern bool debug_mode;
extern bool log_active;
extern int com_argc;
extern char *com_argv[MAX_NUM_ARGVS];
extern char sys_rootdir[ MAX_SYSPATH ];
extern char log_path[256];
extern bool console_read_only;
extern bool show_always;
char *va(const char *format, ...);
static int sys_error = false;

const char* Log_Timestamp( void );
int CheckParm (const char *parm);
void ParseCommandLine (LPSTR lpCmdLine);
void UpdateEnvironmentVariables( void );
bool GetParmFromCmdLine( char *parm, char *out );

//win32 console
void Sys_PrintA(const char *pMsg);
void Sys_PrintW(const char *pMsg);
void Sys_MsgW( const char *pMsg, ... );
void Sys_MsgDevW( const char *pMsg, ... );
void Sys_MsgWarnW( const char *pMsg, ... );
void Sys_CreateConsoleW( void );
void Sys_DestroyConsoleW( void );
void Sys_ShowConsoleW( bool show );
char *Sys_InputW( void );
void Sys_ErrorW(char *error, ...);

//generic stub
__inline void NullVoid( void ) {}
__inline void NullVoidWithArg( bool parm ) {}
__inline void NullVarArgs( char *parm, ... ) {}
__inline char *NullChar( void ) { return NULL; }
__inline void NullInit ( char *funcname, int argc, char **argv ) {}

//memory manager
#define Mem_Alloc(pool, size) pi->Mem.Alloc(pool, size, __FILE__, __LINE__)
#define Mem_Realloc(pool, ptr, size) pi->Mem.Realloc(pool, ptr, size, __FILE__, __LINE__)
#define Mem_Free(mem) pi->Mem.Free(mem, __FILE__, __LINE__)
#define Mem_AllocPool(name) pi->Mem.AllocPool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) pi->Mem.FreePool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) pi->Mem.EmptyPool(pool, __FILE__, __LINE__)
#define Mem_Move(dest, src, size ) pi->Mem.Move (dest, src, size, __FILE__, __LINE__)
#define Mem_Copy(dest, src, size ) pi->Mem.Copy (dest, src, size, __FILE__, __LINE__)
#define Mem_Check() pi->Mem.CheckSentinelsGlobal(__FILE__, __LINE__)

#endif//LAUNCHER_H