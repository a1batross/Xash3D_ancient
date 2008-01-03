//=======================================================================
//			Copyright XashXT Group 2007 ©
//			conv_main.c - resource converter
//=======================================================================

#include "idconv.h"

bool host_debug = false;
stdlib_api_t com;
byte *basepool;
byte *zonepool;
static double start, end;
uint app_name = 0;
string gs_gamedir;

typedef struct convformat_s
{
	char *formatstring;
	char *ext;
	bool (*convfunc)( const char *name, char *buffer, int filesize );
} convformat_t;

convformat_t convert_formats[] =
{
	{"%s.%s", "spr", ConvSPR},	// half-life sprite
	{"%s.%s", "sp2", ConvSP2},	// quake2 sprite
	{"%s.%s", "pcx", ConvPCX},	// quake2 pics
	{"%s.%s", "flt", ConvFLT},	// doom1 textures
	{"%s.%s", "mip", ConvMIP},	// Quake1/Half-Life textures
	{"%s.%s", "lmp", ConvLMP},	// Quake1/Half-Life graphics
	{"%s.%s", "fnt", ConvFNT},	// Half-Life fonts
	{NULL, NULL } // list terminator
};

bool ConvertResource( const char *filename )
{
	convformat_t	*format;
          const char	*ext = FS_FileExtension( filename );
	char		path[128], convname[128], basename[128];
	bool		anyformat = !com.stricmp(ext, "") ? true : false;
	int		filesize = 0;
	byte		*buffer = NULL;

	com.strncpy( convname, filename, sizeof(convname)-1);
	FS_StripExtension( convname ); //remove extension if needed

	// now try all the formats in the selected list
	for (format = convert_formats; format->formatstring; format++)
	{
		if( anyformat || !com.stricmp( ext, format->ext ))
		{
			com.sprintf (path, format->formatstring, convname, format->ext );
			buffer = FS_LoadFile( path, &filesize );
			FS_FileBase( path, basename );
			if(buffer && filesize > 0)
			{
				// this path may contains wadname: wadfile/lumpname
				if( format->convfunc( path, buffer, filesize ))
				{
					Mem_Free( buffer );	// release buffer
					return true;	// converted ok
				}
			}
		}
	}
	MsgDev(D_WARN, "ConvertResource: couldn't load \"%s\"\n", basename );
	return false;
}

/*
==================
CommonInit

idconv.dll needs for some setup operations
so do it manually
==================
*/
void InitConvertor ( uint funcname, int argc, char **argv )
{
	// init pools
	basepool = Mem_AllocPool( "Temp" );
	zonepool = Mem_AllocPool( "Zone" );
          app_name = funcname;
	
	switch( funcname )
	{
	case HOST_CONVERTOR:
		FS_InitRootDir(".");
		start = Sys_DoubleTime();
		break;
	case HOST_OFFLINE:
		break;
	}
}

void RunConvertor ( void )
{
	search_t	*search;
	char	filename[MAX_QPATH], typemod[16], errorstring[256];
	string	searchmask[16];
	int	i, j, numConvertedRes = 0;

	Mem_Set( searchmask, 0,  MAX_STRING * 16 ); 
	Mem_Set( errorstring, 0, MAX_STRING ); 

	switch(app_name)
	{
	case HOST_CONVERTOR:
		strcpy(typemod, "resources" );
		strcpy(searchmask[0], "sprites/*.spr" );	// half-life sprites
		strcpy(searchmask[1], "sprites/*.sp2" );	// quake2 sprites
		strcpy(searchmask[2], "pics/*.pcx" );		// quake2 menu images
		strcpy(searchmask[3], "gfx/*.lmp" );		// quake1 menu images
		strcpy(searchmask[4], "*.spr" );		// half-life sprites
		strcpy(searchmask[5], "*.sp2" );		// half-life sprites
		strcpy(searchmask[6], "*.pcx" );		// quake2 menu images
		strcpy(searchmask[7], "*.flt" );		// doom1 textures
		strcpy(searchmask[8], "*.mip" );		// quake1\half-life textures
		strcpy(searchmask[9], "*.lmp" );		// quake1\half-life pictures
		strcpy(searchmask[10],"*.fnt" );		// half-life fonts
		Msg("Converting ...\n\n");
		break;		
	case HOST_OFFLINE:
		break;
	}

	if(!FS_GetParmFromCmdLine("-game", gs_gamedir ))
		com.strncpy(gs_gamedir, "xash", sizeof(gs_gamedir));

	if(!FS_GetParmFromCmdLine("-file", filename ))
	{
		// search by mask		
		for( i = 0; i < 16; i++)
		{
			// skip blank mask
			if(!com.strlen(searchmask[i])) continue;
			search = FS_Search( searchmask[i], true );
			if(!search) continue; // try next mask

			for( j = 0; j < search->numfilenames; j++ )
			{
				if(ConvertResource( search->filenames[j] ))
					numConvertedRes++;
			}
		}
		if(numConvertedRes == 0) 
		{
			for(j = 0; j < 16; j++) 
			{
				if(!com.strlen(searchmask[j])) continue;
				com.strcat(errorstring, va("%s ", searchmask[j]));
			}
			Sys_Break("no %sfound in this folder!\n", errorstring );
		}
	}
	else ConvertResource( filename );

	end = Sys_DoubleTime();
	Msg ("%5.3f seconds elapsed\n", end - start);
	if(numConvertedRes > 1) Msg("total %d %s compiled\n", numConvertedRes, typemod );
}

void CloseConvertor( void )
{
	Mem_Check(); // check for leaks
	Mem_FreePool( &basepool );
	Mem_FreePool( &zonepool );
}

launch_exp_t DLLEXPORT *CreateAPI( stdlib_api_t *input, void *unused )
{
	static launch_exp_t		Com;

	com = *input;

	// generic functions
	Com.api_size = sizeof(launch_exp_t);

	Com.Init = InitConvertor;
	Com.Main = RunConvertor;
	Com.Free = CloseConvertor;

	return &Com;
}