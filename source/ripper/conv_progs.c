//=======================================================================
//			Copyright XashXT Group 2007 ©
//		  conv_progs.c - convert quakec dat into source
//=======================================================================

#include "ripper.h"

bool ConvDAT( const char *name, char *buffer, int filesize )
{
	// let wadlib just extract progs.dat from progs.wad 
	if( app_name != RIPP_QCCDEC )
		return false;
	return PRVM->DecompileDAT( name );
}