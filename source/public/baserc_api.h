//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       baserc_api.h - xash resource library
//=======================================================================
#ifndef BASERC_API_H
#define BASERC_API_H

/*
==============================================================================

BASERC.DLL INTERFACE

just a resource package library
==============================================================================
*/
typedef struct baserc_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(baserc_exp_t)
	size_t	com_size;		// must matched with sizeof(stdlib_api_t)

	byte *(*LoadFile)( const char *filename, fs_offset_t *size );
} baserc_exp_t;

#endif//BASERC_API_H