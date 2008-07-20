//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       conv_wadlumps.c - convert wad lumps
//=======================================================================

#include "ripper.h"

/*
============
ConvRAW
============
*/
bool ConvRAW( const char *name, char *buffer, int filesize )
{
	if( app_name == RIPP_QCCDEC )
		return false;

	FS_WriteFile(va("%s/other/%s", gs_gamedir, name ), buffer, filesize );
	Msg("%s\n", name ); // echo to console aboutfile
	return true;
}