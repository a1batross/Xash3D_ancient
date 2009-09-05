//=======================================================================
//			Copyright XashXT Group 2009 ©
//		         ximage.c - Image FX & Processing
//=======================================================================

#include "xtools.h"
#include "utils.h"

extern string gs_gamedir;
bool unknown_rotate = false;
bool unknown_flip = false;

bool ConvertImages( byte *mempool, const char *name, byte parms )
{
	rgbdata_t		*pic = FS_LoadImage( name, NULL, 0 );
	int		flags = 0, width = 0, height = 0;
	string		value, outputname;
	bool		result;
	const char	*ext;

	if( !pic ) return false;

	if( FS_GetParmFromCmdLine( "-rotate", value ))
	{
		if( !com.stricmp( value, "90" ))
			flags |= IMAGE_ROT_90;
		else if( !com.stricmp( value, "180" ))
			flags |= IMAGE_ROT180;
		else if( !com.stricmp( value, "270" ))
			flags |= IMAGE_ROT270;
		else if( !com.stricmp( value, "360" ))
			Sys_Break( "Stupid User!!! What you doing?\n" );
		else if( !unknown_rotate )
		{
			Msg( "Usage: -rotate <90|180|270>\n" );
			unknown_rotate = true;
		}
	}

	if( FS_GetParmFromCmdLine( "-flip", value ))
	{
		if( !com.stricmp( value, "vertical" ))
			flags |= IMAGE_FLIP_Y;
		else if( !com.stricmp( value, "horizontal" ))
			flags |= IMAGE_FLIP_X;
		else if( !com.stricmp( value, "diagonal" ))
			flags |= IMAGE_ROT_90;
		else if( !unknown_flip )
		{
			Msg( "Usage: -flip <vertical|horizontal|diagonal>\n" );
			unknown_flip = true;
		}
	}

	// resample modes
	if( FS_CheckParm( "-pow2" )) flags |= IMAGE_ROUND;
	else if( FS_CheckParm( "-pow2fill" )) flags |= IMAGE_ROUNDFILLER;
	else if( FS_CheckParm( "-resample" ))
	{
		int	argc = FS_CheckParm( "-resample" );

		if( !argc ) Sys_Break( "Usage: -resample [width height] or -resample [dimension]\n" );

		if( com.is_digit( com_argv[argc+1] )) width = com.atoi( com_argv[argc+1] );		
		if( com.is_digit( com_argv[argc+2] )) height = com.atoi( com_argv[argc+2] );
		else height = width;
		flags |= IMAGE_RESAMPLE;
	}

	if( FS_CheckParm( "-makeLuma" ) && pic->flags && IMAGE_HAS_LUMA )
		flags |= IMAGE_MAKE_LUMA;

	if( !Image_Process( &pic, width, height, flags ))
	{
		Msg( "Error: can't processing image %s\n", name );
		FS_FreeImage( pic );
		return false;
	}

	if( !gs_filename[0] ) ext = FS_FileExtension( name );
	else ext = gs_filename;

	if( com.stricmp( ext, "tga" ) && com.stricmp( ext, "jpg" ) && com.stricmp( ext, "png" ) &&
	    com.stricmp( ext, "dds" ) && com.stricmp( ext, "pcx" ) && com.stricmp( ext, "bmp" ))
		Sys_Break( "unsupported output format %s\n", ext );

	com.strncpy( outputname, name, sizeof( outputname ));
	FS_StripExtension( outputname );
	result = FS_SaveImage( va( "%s/%s.%s", gs_gamedir, outputname, ext ), pic );
	if( result ) Msg( "%s.%s\n", outputname, ext ); // echo to console
	else Msg( "can't save %s\n", outputname, ext );
	FS_FreeImage( pic );

	return result;	
}