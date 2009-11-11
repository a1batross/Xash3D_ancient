//=======================================================================
//			Copyright (C) Shambler Team 2005
//		    	extdll.h - global defines for dll's
//						    
//=======================================================================

#ifndef EXTDLL_H
#define EXTDLL_H

// Silence certain warnings
#pragma warning(disable : 4305)		// int or float data truncation
#pragma warning(disable : 4201)		// nameless struct/union
#pragma warning(disable : 4514)		// unreferenced inline function removed
#pragma warning(disable : 4100)		// unreferenced formal parameter

#include "windows.h"
#include "basetypes.h"

#define FALSE	0
#define TRUE 	1

typedef unsigned long ULONG;

#include <limits.h>
#include <stdarg.h>

#ifndef min
#define min(a,b)  (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b)  (((a) > (b)) ? (a) : (b))
#endif

#define _vsnprintf(a,b,c,d) vsnprintf(a,b,c,d)

// misc C-runtime library headers
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

inline unsigned long& FloatBits( vec_t& f ) { return *reinterpret_cast<unsigned long*>(&f); }
inline unsigned long const& FloatBits( vec_t const& f ) { return *reinterpret_cast<unsigned long const*>(&f); }
inline vec_t BitsToFloat( unsigned long i ) { return *reinterpret_cast<vec_t*>(&i); }
inline bool IsFinite( vec_t f ) { return ((FloatBits(f) & 0x7F800000) != 0x7F800000); }
inline unsigned long FloatAbsBits( vec_t f ) { return FloatBits(f) & 0x7FFFFFFF; }
inline float FloatMakeNegative( vec_t f ) { return BitsToFloat( FloatBits(f) | 0x80000000 ); }
inline float FloatMakePositive( vec_t f ) { return BitsToFloat( FloatBits(f) & 0x7FFFFFFF ); }

// Shared engine/DLL constants
#include "const.h"

#include "game_shared.h"

// Vector class
#include "vector.h"

 // Shared header describing protocol between engine and DLLs
#include "entity_def.h"
#include "svgame_api.h"

extern Vector vec3_origin;
extern Vector vec3_angles;

#endif // EXTDLL_H