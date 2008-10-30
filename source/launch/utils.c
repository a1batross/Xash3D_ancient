//=======================================================================
//			Copyright XashXT Group 2007 ©
//			utils.c - global system utils
//=======================================================================

#include "launch.h"
#include "mathlib.h"
#include "const.h"

#pragma warning( disable:4730 )	// "mixing _m64 and floating point expressions may result in incorrect code"
#define SIN_TABLE_SIZE	256

static long idum = 0;
static uint iFastSqrtTable[0x10000];
static float fSinCosTable[SIN_TABLE_SIZE];

#define MAX_RANDOM_RANGE	0x7FFFFFFFUL
#define IA		16807
#define IM		2147483647
#define IQ		127773
#define IR		2836
#define NTAB		32
#define NDIV		(1+(IM-1)/NTAB)
#define AM		(1.0/IM)
#define EPS		1.2e-7
#define RNMX		(1.0 - EPS)
#define FP_BITS( fp )	(*(dword *) &(fp))
#define FTOIBIAS		12582912.f

void SeedRandomNumberGenerator( long lSeed )
{
	if( lSeed ) idum = lSeed;
	else idum = -time( NULL );

	if( 1000 < idum ) idum = -idum;
	else if( -1000 < idum ) idum -= 22261048;
}

long lran1( void )
{
	int	j;
	long	k;

	static long iy = 0;
	static long iv[NTAB];
	
	if( idum <= 0 || !iy )
	{
		if(-(idum) < 1) idum=1;
		else idum = -(idum);

		for( j = NTAB + 7; j >= 0; j-- )
		{
			k = (idum) / IQ;
			idum = IA * (idum - k * IQ) - IR * k;
			if( idum < 0 ) idum += IM;
			if( j < NTAB ) iv[j] = idum;
		}
		iy = iv[0];
	}
	k = (idum)/IQ;
	idum = IA * (idum - k * IQ) - IR * k;
	if( idum < 0 ) idum += IM;
	j = iy / NDIV;
	iy = iv[j];
	iv[j] = idum;

	return iy;
}

// fran1 -- return a random floating-point number on the interval [0,1)
float fran1( void )
{
	float temp = (float)AM * lran1();
	if( temp > RNMX ) return (float)RNMX;
	else return temp;
}

float Com_RandomFloat( float flLow, float flHigh )
{
	float	fl;

	if( idum == 0 ) SeedRandomNumberGenerator(0);

	fl = fran1(); // float in [0, 1)
	return (fl * (flHigh - flLow)) + flLow; // float in [low, high)
}

long Com_RandomLong( long lLow, long lHigh )
{
	dword	maxAcceptable;
	dword	n, x = lHigh-lLow + 1; 	

	if( idum == 0 ) SeedRandomNumberGenerator(0);

	if( x <= 0 || MAX_RANDOM_RANGE < x-1 )
		return lLow;

	// The following maps a uniform distribution on the interval [0, MAX_RANDOM_RANGE]
	// to a smaller, client-specified range of [0,x-1] in a way that doesn't bias
	// the uniform distribution unfavorably. Even for a worst case x, the loop is
	// guaranteed to be taken no more than half the time, so for that worst case x,
	// the average number of times through the loop is 2. For cases where x is
	// much smaller than MAX_RANDOM_RANGE, the average number of times through the
	// loop is very close to 1.
	maxAcceptable = MAX_RANDOM_RANGE - ((MAX_RANDOM_RANGE+1) % x );
	do
	{
		n = lran1();
	} while( n > maxAcceptable );

	return lLow + (n % x);
}

// build the square root table
void Com_BuildSqrtTable( void )
{
	union { long l; float f; } dat;
	uint i;

	// build the fast square root table
	for( i = 0; i <= 0x7FFF; i++ )
	{
		// build a float with the bit pattern i as mantissa
		// and an exponent of 0, stored as 127
		dat.l = (i<<8) | (0x7F<<23);
		dat.f = (float) sqrt(dat.f);
    
		// take the square root then strip the first 7 bits of
		// the mantissa into the table
		iFastSqrtTable[i + 0x8000] = (dat.l & 0x7FFFFF);
    
		// repeat the process, this time with an exponent of 1, 
		// stored as 128
		dat.l = (i<<8) | (0x80<<23);
		dat.f = (float) sqrt(dat.f);
    
		iFastSqrtTable[i] = (dat.l & 0x7FFFFF);
	}
}

void Com_BuildSinCosTable( void )
{
	uint	i;
	for( i = 0; i < SIN_TABLE_SIZE; i++ )
		fSinCosTable[i] = sin( i * M_PI2 / SIN_TABLE_SIZE );
}

void SinCos( float radians, float *sine, float *cosine )
{
	_asm
	{
		fld	dword ptr [radians]
		fsincos

		mov edx, dword ptr [cosine]
		mov eax, dword ptr [sine]

		fstp dword ptr [edx]
		fstp dword ptr [eax]
	}
}

float com_sqrt( float x )
{
	// check for square root of 0
	if( FP_BITS( x ) == 0 ) return 0.0f;
	FP_BITS(x) = iFastSqrtTable[(FP_BITS(x)>>8)&0xFFFF]|((((FP_BITS(x)-0x3F800000)>>1)+0x3F800000)&0x7F800000);
	return x;
}

float amd_sqrt( float x )
{
	float	root = 0.f;
	_asm
	{
		_emit	0x0f
		_emit	0x0e
		movd	mm0, x
		_emit	0x0f
		_emit	0x0f
		_emit	((0xc1 & 0x3f)<<3)|0xc0
		_emit	0x97  
		punpckldq	mm0, mm0
		_emit	0x0f
		_emit	0x0f
		_emit	((0xc0 & 0x3f)<<3)|0xc1
		_emit	0xb4  
		movd	root, mm0
		_emit	0x0f
		_emit	0x0e
	}
	return root;
}

float sse_sqrt( float x )
{
	float	root = 0.f;
	_asm
	{
		sqrtss	xmm0, x
		movss	root, xmm0
	}
	return root;
}


#define MAX_STRINGTABLE_SYSTEMS	8	// separately stringsystems

typedef struct stringtable_s
{
	string	name;		// system name (for debug purposes)
	char	*data;		// buffer with strings
	size_t	datasize;		// current buffsize
	size_t	maxdatasize;	// dynamically resized

	int	*table;		// indexes at begining string
	size_t	numstrings;	// current strings count
	size_t	maxstrings;	// current system limit
} stringtable_t;

stringtable_t *dstring[MAX_STRINGTABLE_SYSTEMS];

bool StringTable_CheckHandle( int handle )
{
	if( handle < 0 || handle > MAX_STRINGTABLE_SYSTEMS )
	{
		MsgDev( D_ERROR, "StringTable_CheckHandle: invalid system handle %d\n", handle );
		return false;
	}
	if( !dstring[handle] )
	{
		MsgDev( D_ERROR, "StringTable_CheckHandle: system with handle %d inactive\n", handle );
		return false;
	}
	return true;
}

bool StringTable_CheckString( int handle, string_t str )
{
	if(!StringTable_CheckHandle( handle ))
		return false;

	if( str < 0 || str > dstring[handle]->numstrings )
	{
		MsgDev( D_ERROR, "StringTable_CheckString: invalid string index %i\n", str );
		return false;
	}
	return true;
}

int StringTable_CreateNewSystem( const char *name, size_t max_strings )
{
	int	i;

	// fisrt, find free stringtable system
	for( i = 0; i < MAX_STRINGTABLE_SYSTEMS; i++ )
	{
		if( !dstring[i] )
		{
			// found free slot
			dstring[i] = Mem_Alloc( Sys.stringpool, sizeof( stringtable_t ));
			com.strncpy( dstring[i]->name, name, MAX_STRING );
			dstring[i]->maxstrings = max_strings;
			return i;
		}
	}

	MsgDev( D_ERROR, "StringTable_CreateNewSystem: no free string systems\n" );
	return -1;
}

void StringTable_DeleteSystem( int handle )
{
	if(!StringTable_CheckHandle( handle ))
		return;

	// now free stringtable
	Mem_Free( dstring[handle]->data );	// free strings
	Mem_Free( dstring[handle]->table);	// free indices
	Mem_Free( dstring[handle] );		// free himself
	Mem_Set( &dstring[handle], 0, sizeof( stringtable_t )); 
}

const char *StringTable_GetString( int handle, string_t index )
{
	if(!StringTable_CheckString( handle, index )) return "";
	return &dstring[handle]->data[dstring[handle]->table[index]];
}

string_t StringTable_SetString( int handle, const char *string )
{
	int i, len, table_size, data_size;

	if(!StringTable_CheckHandle( handle ))
		return -1;

	for( i = 0; i < dstring[handle]->numstrings; i++ )
	{
		if(!com.stricmp( string, StringTable_GetString( handle, i )))
			return i; // already existing
	}

	// register new string
	len = com.strlen( string );
	table_size = sizeof(string_t) * (dstring[handle]->numstrings + 1);
	data_size = dstring[handle]->datasize + len + 1;

	if( table_size >= dstring[handle]->maxstrings )
	{
		MsgDev( D_ERROR, "StringTable_SetString: string table %s limit exeeded\n", dstring[handle]->name );
		return -1;
	}

	dstring[handle]->table = Mem_Realloc( Sys.stringpool, dstring[handle]->table, table_size );
	dstring[handle]->data = Mem_Realloc( Sys.stringpool, dstring[handle]->data, data_size );  

	com.strcpy( &dstring[handle]->data[dstring[handle]->datasize], string );
	dstring[handle]->table[dstring[handle]->numstrings] = dstring[handle]->datasize;
	dstring[handle]->datasize += len + 1; // null terminator
	dstring[handle]->numstrings++;

	return dstring[handle]->numstrings - 1; // current index
}

bool StringTable_SaveSystem( int h, wfile_t *wad )
{
	int table_size;

	if(!StringTable_CheckHandle( h ))
		return false;
	if(!W_SaveLump( wad, "stringdata", dstring[h]->data, dstring[h]->datasize, TYPE_STRDATA, CMP_ZLIB ))
		return false;
	table_size = dstring[h]->numstrings * sizeof( string_t );
	if(!W_SaveLump( wad, "stringtable", dstring[h]->table, table_size, TYPE_STRDATA, CMP_ZLIB ))
		return false;

	StringTable_DeleteSystem( h ); // strings dumped, now we can free it
	return true;
}

int StringTable_LoadSystem( wfile_t *wad, const char *name )
{
	int h = StringTable_CreateNewSystem( name, 0x10000 ); // 65535 unique strings
	int datasize, numstrings;

	dstring[h]->data = W_LoadLump( wad, "stringdata", &datasize, TYPE_STRDATA );
	dstring[h]->table = (int *)W_LoadLump( wad, "stringtable", &numstrings, TYPE_STRDATA );
	dstring[h]->datasize = datasize;
	dstring[h]->numstrings = numstrings / sizeof(int);
	return h;
}