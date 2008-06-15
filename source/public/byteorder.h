//=======================================================================
//			Copyright XashXT Group 2007 ©
//			byteorder.h - byte order functions
//=======================================================================
#ifndef BYTEORDER_H
#define BYTEORDER_H

#include "basetypes.h"

// byte order swap functions
_inline word WordSwap( word swap )
{
	word *s = &swap;
	
	__asm {
		mov ebx, s
		mov al, [ebx+1]
		mov ah, [ebx  ]
		mov [ebx], ax
	}
	return *s;
}

#define ShortSwap(x) WordSwap((short)x)

_inline uint UintSwap( uint swap )
{
	uint *i = &swap;

	__asm {
		mov ebx, i
		mov eax, [ebx]
		bswap eax
		mov [ebx], eax
	}
	return *i;
}

#define LongSwap(x) UintSwap((uint)x)

#define FloatSwap(x) UintSwap((uint)x)

_inline double DoubleSwap( double swap )
{
	#define dswap(x, y) t=b[x];b[x]=b[y];b[y]=b[x];

	byte t, *b = ((byte *)&swap);
	dswap(0,7);
	dswap(1,6);
	dswap(2,5);
	dswap(3,4);

	#undef dswap

	return swap;
}

//============================================================================
//			Endianess handling
//============================================================================
// using BSD-style defines: BYTE_ORDER is defined to either BIG_ENDIAN or LITTLE_ENDIAN

// Initializations
#if !defined(BYTE_ORDER) || !defined(LITTLE_ENDIAN) || !defined(BIG_ENDIAN) || (BYTE_ORDER != LITTLE_ENDIAN && BYTE_ORDER != BIG_ENDIAN)
#undef BYTE_ORDER
#undef LITTLE_ENDIAN
#undef BIG_ENDIAN
#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN 4321
#endif

// If we still don't know the CPU endianess at this point, we try to guess
#ifndef BYTE_ORDER
#if defined(WIN32)
#define BYTE_ORDER LITTLE_ENDIAN
#else
#if defined(SUNOS)
#if defined(__i386) || defined(__amd64)
#define BYTE_ORDER LITTLE_ENDIAN
#else
#define BYTE_ORDER BIG_ENDIAN
#endif
#else
#warning "Unable to determine the CPU endianess. Defaulting to little endian"
#define BYTE_ORDER LITTLE_ENDIAN
#endif
#endif
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
// little endian
#define BigShort(l) ShortSwap(l)
#define LittleShort(l) (l)
#define BigLong(l) LongSwap(l)
#define LittleLong(l) (l)
#define BigFloat(l) FloatSwap(l)
#define LittleFloat(l) (l)
#define BigDouble(l) DoubleSwap(l)
#define LittleDouble(l) (l)
static bool big_endian = false;
#else
// big endian
#define BigShort(l) (l)
#define LittleShort(l) ShortSwap(l)
#define BigLong(l) (l)
#define LittleLong(l) LongSwap(l)
#define BigFloat(l) (l)
#define LittleFloat(l) FloatSwap(l)
#define BigDouble(l) (l)
#define LittleDouble(l) DoubleSwap(l)
static bool big_endian = true;
#endif

//extract from buffer
_inline dword BuffBigLong (const byte *buffer)
{
	return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

_inline word BuffBigShort (const byte *buffer)
{
	return (buffer[0] << 8) | buffer[1];
}

_inline float BuffBigFloat (const byte *buffer)
{
	return BuffBigLong( buffer );//same as integer
} 

_inline double BuffBigDouble (const byte *buffer)
{
	return (buffer[0] << 64) | (buffer[1] << 56) | (buffer[2] << 40) | (buffer[3] << 32)
	| (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];
}

_inline dword BuffLittleLong (const byte *buffer)
{
	return (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
}

_inline word BuffLittleShort (const byte *buffer)
{
	return (buffer[1] << 8) | buffer[0];
}

_inline float BuffLittleFloat (const byte *buffer)
{
	return BuffLittleLong( buffer );
}

_inline double BuffLittleDouble (const byte *buffer)
{
	return (buffer[7] << 64) | (buffer[6] << 56) | (buffer[5] << 40) | (buffer[4] << 32)
	| (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
}

/*
=============
SwapBlock

generic lump swaping
=============
*/
_inline void SwapBlock( int *block, int sizeOfBlock )
{
	int	i;

	sizeOfBlock >>= 2;

	for( i = 0; i < sizeOfBlock; i++ )
		block[i] = LittleLong( block[i] );
}


#endif//BYTEORDER_H