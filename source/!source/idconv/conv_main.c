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
string gs_searchmask;

#define	MAX_SEARCHMASK	128
string	searchmask[MAX_SEARCHMASK];
int	num_searchmask = 0;

typedef struct convformat_s
{
	char *formatstring;
	char *ext;
	bool (*convfunc)( const char *name, char *buffer, int filesize );
} convformat_t;

convformat_t convert_formats[] =
{
	{"%s.%s", "spr", ConvSPR},	// quake1/half-life sprite
	{"%s.%s","spr32",ConvSPR},	// spr32 sprite
	{"%s.%s", "sp2", ConvSP2},	// quake2 sprite
	{"%s.%s", "pcx", ConvPCX},	// quake2 pics
	{"%s.%s", "flt", ConvFLT},	// doom1 textures
	{"%s.%s", "mip", ConvMIP},	// Quake1/Half-Life textures
	{"%s.%s", "lmp", ConvLMP},	// Quake1/Half-Life graphics
	{"%s.%s", "fnt", ConvFNT},	// Half-Life fonts
	{"%s.%s", "wal", ConvWAL},	// Quake2 textures
	{"%s.%s", "skn", ConvSKN},	// doom1 sprite models
	{"%s.%s", "bsp", ConvBSP},	// Quake1\Half-Life map textures
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

void AddMask( const char *mask )
{
	if( num_searchmask >= MAX_SEARCHMASK )
	{
		MsgDev(D_WARN, "AddMask: searchlist is full\n");
		return;
	}
	com.strncpy( searchmask[num_searchmask], mask, MAX_STRING );
	num_searchmask++;
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
	string	errorstring;
	int	i, j, numConvertedRes = 0;

	Mem_Set( searchmask, 0,  MAX_STRING * MAX_SEARCHMASK ); 
	Mem_Set( errorstring, 0, MAX_STRING ); 

	switch(app_name)
	{
	case HOST_CONVERTOR:
		Msg("Converting ...\n\n");
		break;		
	case HOST_OFFLINE:
	default: return;
	}

	if(!FS_GetParmFromCmdLine("-mask", gs_searchmask ))
	{
		AddMask( "sprites/*.spr" );
		AddMask( "sprites/*.sp2" );
		AddMask( "sprites/*.spr32");
		AddMask( "textures/*.wal" );
		AddMask( "progs/*.spr32" );
		AddMask( "progs/*.spr" );
		AddMask( "pics/*.pcx" );
		AddMask( "maps/*.bsp" );
		AddMask( "gfx/*.lmp" );
		AddMask( "*.spr" );
		AddMask( "*.sp2" );
		AddMask( "*.pcx" );
		AddMask( "*.flt" );
		AddMask( "*.mip" );
		AddMask( "*.lmp" );
		AddMask( "*.fnt" );
	}
	else AddMask( gs_searchmask ); // custom mask
	
	// find subdirectories
	search = FS_Search("textures/*", true );
	if( search )
	{
		for(i = 0; i < search->numfilenames; i++)
			AddMask(va("%s/*.wal", search->filenames[i]));
		Mem_Free( search );
	}

	// directory to extract
	com.strncpy(gs_gamedir, "~tmpXash", sizeof(gs_gamedir));

	// search by mask		
	for( i = 0; i < num_searchmask; i++)
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
		Mem_Free( search );
	}
	if(numConvertedRes == 0) 
	{
		for(j = 0; j < 16; j++) 
		{
			if(!com.strlen(searchmask[j])) continue;
			com.strncat(errorstring, va("%s ", searchmask[j]), MAX_STRING );
		}
		Sys_Break("no %sfound in this folder!\n", errorstring );
	}

	end = Sys_DoubleTime();
	Msg ("%5.3f seconds elapsed\n", end - start);
	if(numConvertedRes > 1) Msg("total %d files compiled\n", numConvertedRes );
}

void CloseConvertor( void )
{
	// finalize qc-script
	Skin_FinalizeScript();
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