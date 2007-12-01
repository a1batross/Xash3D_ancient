//=======================================================================
//			Copyright XashXT Group 2007 ©
//			utils.h - shared engine utility
//=======================================================================
#ifndef UTILS_H
#define UTILS_H

#include <time.h>

// bsplib compile flags
#define BSP_ONLYENTS	0x01
#define BSP_ONLYVIS		0x02
#define BSP_ONLYRAD		0x04
#define BSP_FULLCOMPILE	0x08

// qcclib compile flags
#define QCC_PROGDEFS	0x01
#define QCC_OPT_LEVEL_0	0x02
#define QCC_OPT_LEVEL_1	0x04
#define QCC_OPT_LEVEL_2	0x08
#define QCC_OPT_LEVEL_3	0x10

#define ALIGN( a ) a = (byte *)((int)((byte *)a + 3) & ~ 3)

extern unsigned __int64 __g_ProfilerStart;
extern unsigned __int64 __g_ProfilerEnd;
extern unsigned __int64 __g_ProfilerEnd2;
extern unsigned __int64 __g_ProfilerSpare;
extern unsigned __int64 __g_ProfilerTotalTicks;
extern double __g_ProfilerTotalMsec;
extern int com_argc;
extern char **com_argv;
extern byte *basepool;
extern byte *zonepool;

#define Profile_Start()\
{\
if (com.GameInfo->rdtsc) \
{ \
__asm pushad \
__asm rdtsc \
__asm mov DWORD PTR[__g_ProfilerStart+4], edx \
__asm mov DWORD PTR[__g_ProfilerStart], eax \
__asm popad \
} \
}

#define Profile_End()\
{\
if (GI.rdtsc) \
{ \
__asm pushad \
__asm rdtsc \
__asm mov DWORD PTR[__g_ProfilerEnd+4], edx \
__asm mov DWORD PTR[__g_ProfilerEnd], eax \
__asm popad \
__asm pushad \
__asm rdtsc \
__asm mov DWORD PTR[__g_ProfilerEnd2+4], edx \
__asm mov DWORD PTR[__g_ProfilerEnd2], eax \
__asm popad \
} \
}

void Profile_RatioResults( void );
void _Profile_Results( const char *function );
void Profile_Store( void );
void Profile_Time( void );	// total profile time
#define Profile_Results( name )  _Profile_Results( #name )

extern stdlib_api_t com;


#define Sys_Error		com.error
#define Malloc(size)	Mem_Alloc(basepool, size)  
#define Z_Malloc(size)	Mem_Alloc(zonepool, size)  
#define Free(mem)	 	Mem_Free(mem) 

extern char gs_mapname[ 64 ];
extern char gs_basedir[ MAX_SYSPATH ];
extern bool host_debug;

extern byte *qccpool;
extern byte *studiopool;

// misc common functions
byte *ReadBMP (char *filename, byte **palette, int *width, int *height);

// misc
bool CompileStudioModel ( byte *mempool, const char *name, byte parms );
bool CompileSpriteModel ( byte *mempool, const char *name, byte parms );
bool ConvertImagePixels ( byte *mempool, const char *name, byte parms );
bool PrepareBSPModel ( const char *dir, const char *name, byte params );
bool CompileBSPModel ( void );
bool PrepareDATProgs ( const char *dir, const char *name, byte params );
bool CompileDATProgs ( void );
bool PrepareROQVideo ( const char *dir, const char *name, byte params );
bool MakeROQ ( void );
#endif//UTILS_H