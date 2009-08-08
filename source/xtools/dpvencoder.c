//=======================================================================
//			Copyright XashXT Group 2008 ©
//			dpvencoder.c - DP video encoder
//=======================================================================

#include "xtools.h"
#include "utils.h"

byte *dpvpool;

bool CompileDPVideo( byte *mempool, const char *name, byte parms )
{
	if( mempool ) dpvpool = mempool;
	else
	{
		Msg( "DPV Encoder: can't allocate memory pool.\nAbort compilation\n" );
		return false;
	}

	// FIXME: implement
	return false;	
}