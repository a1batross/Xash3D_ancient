//=======================================================================
//			Copyright XashXT Group 2007 ©
//			 utils.c - physics callbacks
//=======================================================================

#include "physic.h"

long _ftol( double );
long _ftol2( double dblSource ) 
{
	return _ftol( dblSource );
}

void* Palloc (int size )
{
	return Mem_Alloc( physpool, size );
}

void Pfree (void *ptr, int size )
{
	Mem_Free( ptr );
}