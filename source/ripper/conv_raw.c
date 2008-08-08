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
	FS_WriteFile(va("%s/other/%s", gs_gamedir, name ), buffer, filesize );
	Msg("%s\n", name ); // echo to console aboutfile
	return true;
}