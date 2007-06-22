//=======================================================================
//			Copyright XashXT Group 2007 ©
//			basetypes.h - base engine types
//=======================================================================
#ifndef BASETYPES_H
#define BASETYPES_H

#pragma warning(disable : 4244)	// MIPS
#pragma warning(disable : 4018)	// signed/unsigned mismatch
#pragma warning(disable : 4305)	// truncation from const double to float

#define DLLEXPORT	__declspec(dllexport)
#define DLLIMPORT	__declspec(dllimport)

#define MIN_PATH		64
#define MAX_QPATH		64
#define MAX_OSPATH		128
#define MAX_NUM_ARGVS	128
#define MAX_STRING		256
#define MAX_SYSPATH		1024
#define MAX_INPUTLINE	16384
#define STRING_COLOR_TAG	'^'
#define IsColorString(p)	( p && *(p) == STRING_COLOR_TAG && *((p)+1) && *((p)+1) != STRING_COLOR_TAG )

#define PITCH		0
#define YAW		1
#define ROLL		2

typedef unsigned char 	byte;
typedef enum {false, true}	bool;
typedef unsigned short	word;
typedef unsigned long	dword;
typedef unsigned int	uint;
typedef signed __int64	int64;
typedef struct file_s	file_t;
typedef struct edict_s	edict_t;
typedef struct gclient_s	gclient_t;
typedef struct progfuncs_s	progfuncs_t;
typedef int		progsnum_t;
typedef int		string_t;
typedef int		func_t;
typedef float		vec_t;
typedef byte		jboolean;
typedef vec_t		vec3_t[3];
typedef vec_t		vec4_t[4];
typedef long		fs_offset_t;
typedef vec_t		matrix3x4[3][4];



#define BigShort(l) ShortSwap(l)
#define BigLong(l) LongSwap(l)
#define LittleShort(l) (l)
#define LittleLong(l) (l)
#define LittleFloat(l) (l)

#ifndef NULL
#define NULL	((void *)0)
#endif


#ifndef O_NONBLOCK
#define O_NONBLOCK	0
#endif

_inline short ShortSwap (short l) { byte b1,b2; b1 = l&255; b2 = (l>>8)&255; return (b1<<8) + b2; }
_inline int LongSwap (int l)
{
	byte b1,b2,b3,b4; b1 = l&255; b2 = (l>>8)&255; b3 = (l>>16)&255; b4 = (l>>24)&255;
	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

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