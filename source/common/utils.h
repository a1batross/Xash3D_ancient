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
#define ALIGN( a ) a = (byte *)((int)((byte *)a + 3) & ~ 3)

extern int com_argc;
extern char **com_argv;
extern byte *basepool;
extern byte *zonepool;

extern stdlib_api_t com;
extern vprogs_exp_t *PRVM;

#define Sys_Error			com.error
#define Malloc(size)		Mem_Alloc( basepool, size )  

extern string gs_filename;
extern char gs_basedir[ MAX_SYSPATH ];
extern byte *error_bmp;
extern size_t error_bmp_size;

extern byte *studiopool;

enum
{
	QC_SPRITEGEN = 1,
	QC_STUDIOMDL,
	QC_ROQLIB,
	QC_WADLIB
};

bool Com_ValidScript( const char *token, int scripttype );

// misc
bool CompileStudioModel ( byte *mempool, const char *name, byte parms );
bool CompileSpriteModel ( byte *mempool, const char *name, byte parms );
bool ConvertImagePixels ( byte *mempool, const char *name, byte parms );
bool CompileWad3Archive ( byte *mempool, const char *name, byte parms );
bool CompileROQVideo( byte *mempool, const char *name, byte parms );
bool PrepareBSPModel ( const char *dir, const char *name, byte params );
bool CompileBSPModel ( void );
#endif//UTILS_H