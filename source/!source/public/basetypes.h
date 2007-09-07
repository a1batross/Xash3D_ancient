//=======================================================================
//			Copyright XashXT Group 2007 ©
//			basetypes.h - base engine types
//=======================================================================
#ifndef BASETYPES_H
#define BASETYPES_H

#pragma warning(disable : 4244)	// MIPS
#pragma warning(disable : 4018)	// signed/unsigned mismatch
#pragma warning(disable : 4305)	// truncation from const double to float
#pragma warning(disable : 4201)		// nameless struct/union
#pragma warning(disable : 4514)		// unreferenced inline function removed
#pragma warning(disable : 4100)		// unreferenced formal parameter

#define DLLEXPORT	__declspec(dllexport)
#define DLLIMPORT	__declspec(dllimport)

#define MIN_PATH		64
#define MAX_QPATH		64
#define MAX_OSPATH		128
#define MAX_NUM_ARGVS	128
#define MAX_STRING		256
#define MAX_SYSPATH		1024
#define MAX_INPUTLINE	16384
#define MAX_INFO_KEY	64
#define MAX_INFO_VALUE	64
#define MAX_INFO_STRING	512

#define STRING_COLOR_TAG	'^'
#define IsColorString(p)	( p && *(p) == STRING_COLOR_TAG && *((p)+1) && *((p)+1) != STRING_COLOR_TAG )

#define PITCH		0
#define YAW		1
#define ROLL		2

typedef enum{false, true}	bool;
typedef unsigned char 	byte;
typedef unsigned short	word;
typedef unsigned long	dword;
typedef unsigned int	uint;
typedef signed __int64	int64;
typedef struct file_s	file_t;
typedef struct vfile_s	vfile_t;
typedef struct image_s	image_t;
typedef struct model_s	model_t;
typedef int		func_t;
typedef struct edict_s	edict_t;
typedef struct gclient_s	gclient_t;
typedef int		string_t;
typedef int		progsnum_t;
typedef struct progfuncs_s	progfuncs_t;
typedef float		vec_t;
typedef vec_t		vec3_t[3];
typedef vec_t		vec4_t[4];
typedef long		fs_offset_t;
typedef vec_t		matrix3x4[3][4];
typedef struct prvm_edict_s	prvm_edict_t;
typedef struct { int fileofs; int filelen; }lump_t;
typedef struct { byte r; byte g; byte b; } color24;
typedef struct { uint b:5; uint g:6; uint r:5; } color16;
typedef struct { byte r; byte g; byte b; byte a; } color32;

#ifndef NULL
#define NULL	((void *)0)
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK	0
#endif

#include "byteorder.h"

#ifdef WIN32
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
_inline char *va(const char *format, ...)
{
	va_list argptr;
	static char string[8][1024], *s;
	static int stringindex = 0;

	s = string[stringindex];
	stringindex = (stringindex + 1) & 7;
	va_start (argptr, format);
	vsprintf (s, format, argptr);
	va_end (argptr);
	return s;
}

#endif//BASETYPES_H