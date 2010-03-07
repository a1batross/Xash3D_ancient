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