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
#include <ref_launcher.h>
#include <ref_platform.h>

//import variables
char *(*Sys_Input ) ( void );	
void ( *Msg )( char *msg, ... );
void ( *Sys_Print )( char *msg );
void ( *Sys_InitConsole )( void );
void ( *Sys_FreeConsole )( void );
void ( *MsgDev )( char *msg, ... );
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
extern int com_argc;
extern char *com_argv[MAX_NUM_ARGVS];
extern bool console_read_only;
extern bool show_always;
char *va(const char *format, ...);
static int sys_error = false;

int CheckParm (const char *parm);
void ParseCommandLine (LPSTR lpCmdLine);
void UpdateEnvironmentVariables( void );

//win32 console
void Sys_PrintW(const char *pMsg);
void Sys_MsgW( const char *pMsg, ... );
void Sys_MsgDevW( const char *pMsg, ... );
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

#endif//LAUNCHER_H