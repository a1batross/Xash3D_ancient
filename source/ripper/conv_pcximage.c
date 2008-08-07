//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       conv_pcximage.c - convert pcximages
//=======================================================================

#include "ripper.h"
#include "qc_gen.h"

bool PCX_ConvertImage( const char *name, char *buffer, int filesize )
{
	rgbdata_t	*pic = FS_LoadImage( "#internal.pcx", buffer, filesize );

	if( pic )
	{
		FS_StripExtension((char *)name );
		FS_SaveImage( va("%s/%s.bmp", gs_gamedir, name ), pic ); // save converted image
		FS_FreeImage( pic ); // release buffer
		Msg("%s.pcx\n", name ); // echo to console about current pic
		return true;
	}
	return false;
}

/*
============
ConvPCX
============
*/
bool ConvPCX( const char *name, char *buffer, int filesize )
{
	string picname, path;

	FS_FileBase( name, picname );
	if(!com.strnicmp("num", picname, 3 ))
		com.snprintf( path, MAX_STRING, "gfx/fonts/%s", picname );
	else if(!com.strnicmp("anum", picname, 4 ))
		com.snprintf( path, MAX_STRING, "gfx/fonts/%s", picname );
	else if(!com.strnicmp("conchars", picname, 8 ))
		com.snprintf( path, MAX_STRING, "gfx/fonts/%s", picname );
	else if(!com.strnicmp("a_", picname, 2 ))
		com.snprintf( path, MAX_STRING, "gfx/hud/%s", picname );
	else if(!com.strnicmp("p_", picname, 2 ))
		com.snprintf( path, MAX_STRING, "gfx/hud/%s", picname );
	else if(!com.strnicmp("k_", picname, 2 ))
		com.snprintf( path, MAX_STRING, "gfx/hud/%s", picname );
	else if(!com.strnicmp("i_", picname, 2 ))
		com.snprintf( path, MAX_STRING, "gfx/hud/%s", picname );
	else if(!com.strnicmp("w_", picname, 2 ))
		com.snprintf( path, MAX_STRING, "gfx/hud/%s", picname );
	else if(!com.strnicmp("m_", picname, 2 ))
		com.snprintf( path, MAX_STRING, "gfx/menu/%s", picname );
	else if(!com.strnicmp("m_", picname, 2 ))
		com.snprintf( path, MAX_STRING, "gfx/menu/%s", picname );
	else com.snprintf( path, MAX_STRING, "gfx/common/%s", picname );		

	if(PCX_ConvertImage( path, buffer, filesize ))
	{
		Msg("%s\n", name ); // echo to console about current image
		return true;
	}
	return false;
}