//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       conv_raw.c - extract hidden lumps
//=======================================================================

#include "ripper.h"

/*
============
ConvRAW
============
*/
bool ConvRAW( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	FS_WriteFile(va("%s/other/%s.ext", gs_gamedir, name, ext ), buffer, filesize );
	Msg("%s.%s\n", name, ext ); // echo to console aboutfile
	return true;
}