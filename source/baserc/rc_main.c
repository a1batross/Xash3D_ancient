//=======================================================================
//			Copyright XashXT Group 2008 ©
//			rc_main.c - resource library
//=======================================================================

#include <windows.h>
#include "launch_api.h"
#include "baserc_api.h"

// resources
#include "images.h"
#include "conback.h"

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
	{"checkerboard.dds", q1mip_dds, sizeof(q1mip_dds)},
	{"default.dds", deffont_dds, sizeof(deffont_dds)},
	{"conback.dds", conback_dds, sizeof(conback_dds)},
	{"net.png", net_png, sizeof(net_png)},
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

// main DLL entry point
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	return TRUE;
}

baserc_exp_t DLLEXPORT *CreateAPI( stdlib_api_t *input, void *unused )
{
	static baserc_exp_t		Com;

	com = *input;

	// generic functions
	Com.api_size = sizeof( baserc_exp_t );
	Com.com_size = sizeof( stdlib_api_t );
	Com.LoadFile = RC_FindFile;

	return &Com;
}