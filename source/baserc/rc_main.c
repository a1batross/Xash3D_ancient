//=======================================================================
//			Copyright XashXT Group 2008 ©
//			rc_main.c - resource library
//=======================================================================

#include <windows.h>
#include "launch_api.h"
#include "baserc_api.h"

// resources
#include "images.h"
#include "server.h"
#include "client.h"
#include "uimenu.h"

stdlib_api_t com;

typedef struct loadres_s
{
	const char	*intname;
	const byte	*buffer;
	const size_t	filesize;
} loadres_t;

loadres_t load_resources[] =
{
	// add new resource description here
	{"error.tga", error_tga, sizeof(error_tga)},
	{"blank.bmp", blank_bmp, sizeof(blank_bmp)},
	{"default.dds", deffont_dds, sizeof(deffont_dds)},
	{"server.dat", server_dat, sizeof(server_dat)},
	{"client.dat", client_dat, sizeof(client_dat)},
	{"uimenu.dat", uimenu_dat, sizeof(uimenu_dat)},
	{NULL, NULL, 0 }
};

byte *RC_FindFile( const char *filename, fs_offset_t *size )
{
	loadres_t	*res;

	// now try all the formats in the selected list
	for( res = load_resources; res->intname; res++ )
	{
		if(!com.stricmp( filename, res->intname ))
		{
			MsgDev( D_LOAD, "RC_FindFile: %s\n", res->intname );	
			if( size ) *size = res->filesize;
			return ( byte *)res->buffer;
		}
	}

	// no matching found
	if( size ) *size = 0;
	return NULL;
}

baserc_exp_t DLLEXPORT *CreateAPI( stdlib_api_t *input, void *unused )
{
	static baserc_exp_t		Com;

	com = *input;

	// generic functions
	Com.api_size = sizeof(baserc_exp_t);
	Com.LoadFile = RC_FindFile;

	return &Com;
}