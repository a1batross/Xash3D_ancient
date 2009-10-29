//=======================================================================
//			Copyright XashXT Group 2008 ©
//		         engine_api.h - xash engine api
//=======================================================================
#ifndef ENGINE_API_H
#define ENGINE_API_H

#include "const.h"

//
// engine constant limits, touching networking protocol modify with precaution
//
#define MAX_DLIGHTS			32	// dynamic lights (rendered per one frame)
#define MAX_LIGHTSTYLES		256	// can't be blindly increased
#define MAX_DECALS			256	// server decal indexes (different decalnames, not a render limit)
#define MAX_USER_MESSAGES		200	// another 56 messages reserved for engine routines
#define MAX_CLASSNAMES		512	// maxcount of various edicts classnames
#define MAX_SOUNDS			512	// openal software limit
#define MAX_MODELS			4096	// total count of brush & studio various models per one map
#define MAX_PARTICLES		32768	// per one frame
#define MAX_EDICTS			32768	// absolute limit that never be reached, (do not edit!)

/*
==============================================================================

ENGINE TRACE FORMAT
==============================================================================
*/
typedef struct cplane_s
{
	vec3_t		normal;
	float		dist;
	short		type;		// for fast side tests
	short		signbits;		// signx + (signy<<1) + (signz<<1)
} cplane_t;

/*
==============================================================================

Generic LAUNCH.DLL INTERFACE
==============================================================================
*/
typedef struct launch_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(launch_api_t)
	size_t	com_size;		// must matched with sizeof(stdlib_api_t)

	void (*Init)( const int argc, const char **argv );	// init host
	void (*Main)( void );				// host frame
	void (*Free)( void );				// close host
	void (*CPrint)( const char *msg );			// host print
} launch_exp_t;

#endif//ENGINE_API_H