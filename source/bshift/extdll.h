/***
*
*	Copyright (c) 1999, 2000 Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#ifndef EXTDLL_H
#define EXTDLL_H


//
// Global header file for extension DLLs
//

// Allow "DEBUG" in addition to default "_DEBUG"
#ifdef _DEBUG
#define DEBUG 1
#endif

// Silence certain warnings
#pragma warning(disable : 4244)		// int or float down-conversion
#pragma warning(disable : 4305)		// int or float data truncation
#pragma warning(disable : 4201)		// nameless struct/union
#pragma warning(disable : 4514)		// unreferenced inline function removed
#pragma warning(disable : 4100)		// unreferenced formal parameter

#include "windows.h"
#include "basetypes.h"

#define FALSE	0
#define TRUE	1

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

// Misc C-runtime library headers
#include "stdio.h"
#include "stdlib.h"
#include "math.h"

// Shared engine/DLL constants
#include "const.h"

// Vector class
#include "vector.h"

 // Shared header describing protocol between engine and DLLs
#include "entity_def.h"
#include "svgame_api.h"

// Shared header between the client DLL and the game DLLs
#include "cdll_dll.h"

#endif //EXTDLL_H
