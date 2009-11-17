//=======================================================================
//			Copyright XashXT Group 2007 ©
//			utils.c - global system utils
//=======================================================================

#include "launch.h"
#include "qfiles_ref.h"
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

/*
=================
Com_HashKey

returns hash key for string
=================
*/
uint Com_HashKey( const char *string, uint hashSize )
{
	uint	hashKey = 0;
	int	i;

	for( i = 0; string[i]; i++ )
		hashKey = (hashKey + i) * 37 + com_tolower(string[i]);

	return (hashKey % hashSize);
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

#define ST_STATIC_ALLOCATE		// comment this to use realloc
// (all pointers to real strings will be invalid after another calling SetString, but it - memory economy mode )

typedef struct stringtable_s
{
	string	name;		// system name (for debug purposes)
	byte	*mempool;		// system mempool
	char	*data;		// buffer with strings
	size_t	datasize;		// current buffsize
	size_t	maxdatasize;	// dynamically resized

	int	*table;		// indexes at begining string
	size_t	numstrings;	// current strings count
	size_t	maxstrings;	// current system limit
} stringtable_t;

stringtable_t *dstring[MAX_STRING_TABLES];

bool StringTable_CheckHandle( int handle, bool silent )
{
	if( handle < 0 || handle > MAX_STRING_TABLES )
	{
		if( !silent )
			MsgDev( D_ERROR, "StringTable_CheckHandle: invalid system handle %d\n", handle );
		return false;
	}
	if( !dstring[handle] )
	{
		if( !silent )
			MsgDev( D_ERROR, "StringTable_CheckHandle: system with handle %d inactive\n", handle );
		return false;
	}
	return true;
}

bool StringTable_CheckString( int handle, string_t str )
{
	if(!StringTable_CheckHandle( handle, true ))
		return false;

	if( str < 0 || str >= dstring[handle]->numstrings )
	{
		MsgDev( D_ERROR, "StringTable( %s ): invalid string index %i\n", dstring[handle]->name, str );
		return false;
	}
	return true;
}

const char *StringTable_GetName( int handle )
{
	if( !StringTable_CheckHandle( handle, true ))
		return NULL;	
	return dstring[handle]->name;
}

int StringTable_CreateNewSystem( const char *name, size_t max_strings )
{
	int	i;

	// fisrt, find free stringtable system
	for( i = 0; i < MAX_STRING_TABLES; i++ )
	{
		if( !dstring[i] )
		{
			// found free slot
			dstring[i] = Mem_Alloc( Sys.basepool, sizeof( stringtable_t ));
			dstring[i]->mempool = Mem_AllocPool( va( "StringTable_%s", name ));
			com.strncpy( dstring[i]->name, name, MAX_STRING );
			dstring[i]->maxdatasize = max_strings * 8;
			dstring[i]->maxstrings = max_strings;
#ifdef ST_STATIC_ALLOCATE
			// create static arrays
			dstring[i]->data = (char *)Mem_Alloc( dstring[i]->mempool, dstring[i]->maxdatasize );
			dstring[i]->table = (int *)Mem_Alloc( dstring[i]->mempool, dstring[i]->maxstrings );
#endif

			StringTable_SetString( i, "" ); // make iNullString
			return i;
		}
	}

	MsgDev( D_ERROR, "StringTable_CreateNewSystem: no free string systems\n" );
	return -1;
}

void StringTable_DeleteSystem( int handle )
{
	if( !StringTable_CheckHandle( handle, false ))
		return;

	// now free stringtable
	Mem_FreePool( &dstring[handle]->mempool );
	Mem_Free( dstring[handle] ); // free himself
	dstring[handle] = NULL;
}

void StringTable_ClearSystem( int handle )
{
	if( !StringTable_CheckHandle( handle, false ))
		return;

	Mem_EmptyPool( dstring[handle]->mempool );
	dstring[handle]->datasize = dstring[handle]->numstrings = 0;
#ifdef ST_STATIC_ALLOCATE
	dstring[handle]->data = (char *)Mem_Alloc( dstring[handle]->mempool, dstring[handle]->maxdatasize );
	dstring[handle]->table = (int *)Mem_Alloc( dstring[handle]->mempool, dstring[handle]->maxstrings );
#else
	dstring[handle]->table = NULL;
	dstring[handle]->data = NULL;
#endif
	StringTable_SetString( handle, "" ); // make iNullString
}

const char *StringTable_GetString( int handle, string_t index )
{
	if(!StringTable_CheckString( handle, index )) return "";
	return &dstring[handle]->data[dstring[handle]->table[index]];
}

string_t StringTable_SetString( int handle, const char *string )
{
	int	i, len, table_size, data_size;

	if( !StringTable_CheckHandle( handle, false ))
		return -1;

	for( i = 0; i < dstring[handle]->numstrings; i++ )
	{
		if(!com.strcmp( string, StringTable_GetString( handle, i )))
			return i; // already existing
	}

	// register new string
	len = com.strlen( string );
	table_size = sizeof(string_t) * (dstring[handle]->numstrings + 1);
	data_size = dstring[handle]->datasize + len + 1;

	if( table_size >= dstring[handle]->maxstrings || data_size >= dstring[handle]->maxdatasize )
	{
		MsgDev( D_ERROR, "StringTable_SetString: string table %s limit exeeded\n", dstring[handle]->name );
		return -1;
	}
#ifndef ST_STATIC_ALLOCATE
	dstring[handle]->table = Mem_Realloc( dstring[handle]->mempool, dstring[handle]->table, table_size );
	dstring[handle]->data = Mem_Realloc( dstring[handle]->mempool, dstring[handle]->data, data_size );  
#endif
	com.strcpy( &dstring[handle]->data[dstring[handle]->datasize], string );
	dstring[handle]->table[dstring[handle]->numstrings] = dstring[handle]->datasize;
	dstring[handle]->datasize += len + 1; // null terminator
	dstring[handle]->numstrings++;

	return dstring[handle]->numstrings - 1; // current index
}

bool StringTable_SaveSystem( int h, wfile_t *wad )
{
	int table_size;

	if(!StringTable_CheckHandle( h, false ))
		return false;
	if(!W_SaveLump( wad, "stringdata", dstring[h]->data, dstring[h]->datasize, TYPE_STRDATA, CMP_ZLIB ))
		return false;
	table_size = dstring[h]->numstrings * sizeof( string_t );
	if( !W_SaveLump( wad, "stringtable", dstring[h]->table, table_size, TYPE_STRDATA, CMP_ZLIB ))
		return false;
	return true;
}

int StringTable_LoadSystem( wfile_t *wad, const char *name )
{
	int datasize, table_size;
	int h = StringTable_CreateNewSystem( name, 0x10000 ); // 65535 unique strings
	char *data = (char *)W_LoadLump( wad, "stringdata", &datasize, TYPE_STRDATA );
	int *table = (int *)W_LoadLump( wad, "stringtable", &table_size, TYPE_STRDATA );

	if(( datasize > dstring[h]->maxdatasize ) || ((table_size / sizeof( int )) > dstring[h]->maxstrings ))
		Sys_Error( "Too small StringTable for loading\n" );

#ifndef ST_STATIC_ALLOCATE
	dstring[h]->data = Mem_Realloc( dstring[handle]->mempool, dstring[handle]->data, datasize );	
	dstring[h]->table = Mem_Realloc( dstring[handle]->mempool, dstring[handle]->table, table_size );
#endif
	Mem_Copy( dstring[h]->data, data, datasize );
	Mem_Copy( dstring[h]->table, table, table_size ); 
	dstring[h]->datasize = datasize;
	dstring[h]->numstrings = table_size / sizeof( int );
	return h;
}

void StringTable_Info_f( void )
{
	int	i, j;

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: stringlist <name>\n" );
		return;
	}

	// print all strings in selected StringTable
	for( i = 0; i < MAX_STRING_TABLES; i++ )
	{
		if( !dstring[i] ) continue;

		if( !com.stricmp( dstring[i]->name, Cmd_Argv( 1 )))
		{
			Msg( "------------- %i strings -------------\n", dstring[i]->numstrings );
			for( j = 0; j < dstring[i]->numstrings; j++ )
				Msg( "%s ", StringTable_GetString( i, j ));
			Msg( "\n  ^3total %s used\n", com_pretifymem( dstring[i]->datasize, 3 ));
			break;
		}
	}
}