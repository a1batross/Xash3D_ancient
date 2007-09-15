//=======================================================================
//			Copyright XashXT Group 2007 ©
//			utils.h - shared engine utility
//=======================================================================
#ifndef UTILS_H
#define UTILS_H

#include <time.h>

#define ALIGN( a ) a = (byte *)((int)((byte *)a + 3) & ~ 3)
#define PATHSEPARATOR(c) ((c) == '\\' || (c) == '/')
#define SYSTEM_SLASH_CHAR  '\\'

// Processor Information:
typedef struct cpuinfo_s
{
	bool m_bRDTSC	: 1;	// Is RDTSC supported?
	bool m_bCMOV	: 1;	// Is CMOV supported?
	bool m_bFCMOV	: 1;	// Is FCMOV supported?
	bool m_bSSE	: 1;	// Is SSE supported?
	bool m_bSSE2	: 1;	// Is SSE2 Supported?
	bool m_b3DNow	: 1;	// Is 3DNow! Supported?
	bool m_bMMX	: 1;	// Is MMX supported?
	bool m_bHT	: 1;	// Is HyperThreading supported?

	byte m_usNumLogicCore;	// Number op logical processors.
	byte m_usNumPhysCore;	// Number of physical processors

	int64 m_speed;		// In cycles per second.
	int   m_size;		// structure size
	char* m_szCPUID;		// Processor vendor Identification.
} cpuinfo_t;
cpuinfo_t GetCPUInformation( void );

//internal filesystem functions
void FS_Path (void);
void FS_InitEditor( void );
void FS_InitRootDir( char *path );
void FS_ClearSearchPath (void);
void FS_AddGameHierarchy (const char *dir);
int FS_CheckParm (const char *parm);
void FS_LoadGameInfo( const char *filename );
void FS_FileBase (char *in, char *out);
void FS_InitCmdLine( int argc, char **argv );
const char *FS_FileExtension (const char *in);
void FS_DefaultExtension (char *path, const char *extension );
bool FS_GetParmFromCmdLine( char *parm, char *out );



//files managment (like fopen, fread etc)
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

//virtual files managment
vfile_t *VFS_Open(file_t* real_file, const char* mode);
fs_offset_t VFS_Write( vfile_t *file, const void *buf, size_t size );
fs_offset_t VFS_Read(vfile_t* file, void* buffer, size_t buffersize);
int VFS_Seek( vfile_t *file, fs_offset_t offset, int whence );
fs_offset_t VFS_Tell (vfile_t* file);
int VFS_Close( vfile_t *file );

void InitMemory (void); //must be init first at application
void FreeMemory( void );

void FS_Init( int argc, char **argv );
void FS_Shutdown (void);

#define Mem_Alloc(pool, size) _Mem_Alloc(pool, size, __FILE__, __LINE__)
#define Mem_Realloc(pool, ptr, size) _Mem_Realloc(pool, ptr, size, __FILE__, __LINE__)
#define Mem_Free(mem) _Mem_Free(mem, __FILE__, __LINE__)
#define Mem_AllocPool(name) _Mem_AllocPool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) _Mem_FreePool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) _Mem_EmptyPool(pool, __FILE__, __LINE__)
#define Mem_Move(dest, src, size ) _Mem_Move (dest, src, size, __FILE__, __LINE__)
#define Mem_Copy(dest, src, size ) _Mem_Copy (dest, src, size, __FILE__, __LINE__)

extern stdlib_api_t std;
extern gameinfo_t GI;

#define Msg std.printf
#define MsgDev std.dprintf
#define MsgWarn std.wprintf
#define Sys_Error std.error
#define Sys_LoadLibrary std.LoadLibrary
#define Sys_FreeLibrary std.FreeLibrary


#define Malloc(size)	Mem_Alloc(basepool, size)  
#define Z_Malloc(size)	Mem_Alloc(zonepool, size)  
#define Free(mem)	 	Mem_Free(mem) 

extern char fs_rootdir[ MAX_SYSPATH ];	//root directory of engine
extern char fs_basedir[ MAX_SYSPATH ];	//base directory of game
extern char fs_gamedir[ MAX_SYSPATH ];	//game current directory
extern char token[ MAX_INPUTLINE ];
extern char gs_mapname[ 64 ];
extern char gs_basedir[ MAX_SYSPATH ];
extern char g_TXcommand;
extern bool endofscript;
extern bool host_debug;

extern int fs_argc;
extern char **fs_argv;
extern byte *qccpool;
extern byte *studiopool;

//misc common functions
char *copystring(char *s);
char *strlower (char *start);
char *va(const char *format, ...);
char *stristr( const char *string, const char *string2 );
void ExtractFilePath(const char* const path, char* dest);
byte *ReadBMP (char *filename, byte **palette, int *width, int *height);

extern int numthreads;
void ThreadLock (void);
void ThreadUnlock (void);
void ThreadSetDefault (void);
void RunThreadsOn (int workcnt, bool showpacifier, void(*func)(int));
void RunThreadsOnIndividual (int workcnt, bool showpacifier, void(*func)(int));

_inline double I_FloatTime (void) { time_t t; time(&t); return t; }

//misc
bool CompileStudioModel ( byte *mempool, const char *name, byte parms );
bool CompileSpriteModel ( byte *mempool, const char *name, byte parms );
bool PrepareBSPModel ( const char *dir, const char *name, byte params );
bool CompileBSPModel ( void );

#endif//UTILS_H