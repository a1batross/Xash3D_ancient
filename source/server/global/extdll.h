//=======================================================================
//			Copyright (C) Shambler Team 2005
//		    	basetypes.h - global defines for dll's
//						    
//=======================================================================

#ifndef BASETYPES_H
#define BASETYPES_H

// Silence certain warnings
#pragma warning(disable : 4305)		// int or float data truncation
#pragma warning(disable : 4201)		// nameless struct/union
#pragma warning(disable : 4514)		// unreferenced inline function removed
#pragma warning(disable : 4100)		// unreferenced formal parameter

// Prevent tons of unused windows definitions
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOWINRES
#define NOSERVICE
#define NOMCX
#define NOIME
#include "windows.h"
#else // _WIN32
#define FALSE 0
#define TRUE (!FALSE)
typedef unsigned long ULONG;
typedef unsigned char BYTE;
typedef int BOOL;
#define MAX_PATH PATH_MAX
#include <limits.h>
#include <stdarg.h>
#ifndef min
#define min(a,b)  (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)  (((a) > (b)) ? (a) : (b))
#define _vsnprintf(a,b,c,d) vsnprintf(a,b,c,d)
#endif
#endif //_WIN32

#ifndef BIT
#define BIT( n )		(1<<( n ))
#endif

// Misc C-runtime library headers
#include "stdio.h"
#include "stdlib.h"
#include "math.h"

// Header file containing definition of globalvars_t and entvars_t
typedef unsigned char 	byte;
typedef unsigned short	word;
typedef unsigned int	uint;
typedef unsigned long	dword;
typedef int		string_t;		// from engine's pr_comp.h;
typedef int		shader_t;
typedef float		vec_t;		// needed before including progdefs.h

inline unsigned long& FloatBits( vec_t& f ) { return *reinterpret_cast<unsigned long*>(&f); }
inline unsigned long const& FloatBits( vec_t const& f ) { return *reinterpret_cast<unsigned long const*>(&f); }
inline vec_t BitsToFloat( unsigned long i ) { return *reinterpret_cast<vec_t*>(&i); }
inline bool IsFinite( vec_t f ) { return ((FloatBits(f) & 0x7F800000) != 0x7F800000); }
inline unsigned long FloatAbsBits( vec_t f ) { return FloatBits(f) & 0x7FFFFFFF; }
inline float FloatMakeNegative( vec_t f ) { return BitsToFloat( FloatBits(f) | 0x80000000 ); }
inline float FloatMakePositive( vec_t f ) { return BitsToFloat( FloatBits(f) & 0x7FFFFFFF ); }

// Shared engine/DLL constants
#include "const.h"

// Vector class
#include "vector.h"

 // Shared header describing protocol between engine and DLLs
#include "entity_def.h"
#include "svgame_api.h"

extern Vector vec3_origin;
extern Vector vec3_angles;

#endif // BASETYPES_H