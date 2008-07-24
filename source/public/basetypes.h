//=======================================================================
//			Copyright XashXT Group 2007 ©
//			basetypes.h - base engine types
//=======================================================================
#ifndef BASETYPES_H
#define BASETYPES_H

#pragma warning(disable : 4244)	// MIPS
#pragma warning(disable : 4018)	// signed/unsigned mismatch
#pragma warning(disable : 4305)	// truncation from const double to float (probably not needed)

#define MAX_QPATH		64	// get rid of this
#define MAX_STRING		256
#define MAX_SYSPATH		1024
#define MAX_MSGLEN		32768	// max length of network message

typedef enum{ false, true }	bool;
typedef unsigned char 	byte;
typedef unsigned short	word;
typedef unsigned long	dword;
typedef unsigned int	uint;
typedef signed __int64	int64;
typedef float		vec_t;
typedef vec_t		vec2_t[2];
typedef vec_t		vec3_t[3];
typedef vec_t		vec4_t[4];
typedef vec_t		matrix3x3[3][3];
typedef vec_t		matrix3x4[3][4];
typedef vec_t		matrix4x3[4][3];
typedef vec_t		matrix4x4[4][4];
typedef char		string[MAX_STRING];

#endif//BASETYPES_H