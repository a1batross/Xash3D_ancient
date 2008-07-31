//=======================================================================
//			Copyright XashXT Group 2007 ©
//			conv_main.c - resource converter
//=======================================================================

#include "ripper.h"
#include "pal_utils.h"

dll_info_t imglib_dll = { "imglib.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(imglib_exp_t) };
dll_info_t vprogs_dll = { "vprogs.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(vprogs_exp_t) };
imglib_exp_t *Image;
vprogs_exp_t *PRVM;
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
	{"%s.%s", "jpg", ConvJPG},	// quake3 textures
	{"%s.%s", "pcx", ConvPCX},	// quake2 pics
	{"%s.%s", "pal", ConvPAL},	// q1\q2\hl\d1 palette
	{"%s.%s", "flt", ConvFLT},	// doom1 textures
	{"%s.%s", "flp", ConvFLP},	// doom1 menu pics
	{"%s.%s", "mip", ConvMIP},	// Quake1/Half-Life textures
	{"%s.%s", "lmp", ConvLMP},	// Quake1/Half-Life gfx
	{"%s.%s", "fnt", ConvFNT},	// Half-Life fonts
	{"%s.%s", "wal", ConvWAL},	// Quake2 textures
	{"%s.%s", "skn", ConvSKN},	// doom1 sprite models
	{"%s.%s", "bsp", ConvBSP},	// Quake1\Half-Life map textures
	{"%s.%s", "mus", ConvMID},	// Quake1\Half-Life map textures
	{"%s.%s", "snd", ConvSND},	// Quake1\Half-Life map textures
	{"%s.%s", "dat", ConvDAT},	// Decompile Quake1 qc-progs
	{"%s.%s", "txt", ConvRAW},	// (hidden) Xash-extract scripts
	{"%s.%s", "dat", ConvRAW},	// (hidden) Xash-extract progs
	{NULL, NULL }		// list terminator
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
			com.sprintf( path, format->formatstring, convname, format->ext );
			buffer = FS_LoadFile( path, &filesize );
			FS_FileBase( path, basename );
			if( buffer && filesize > 0 )
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

void ClrMask( void )
{
	num_searchmask = 0;
	memset( searchmask, 0,  MAX_STRING * MAX_SEARCHMASK ); 
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
void InitConvertor ( int argc, char **argv )
{
	launch_t	CreateImglib, CreateVprogs;
	string	gamedir;
	
	// init pools
	basepool = Mem_AllocPool( "Temp" );
	zonepool = Mem_AllocPool( "Zone" );
          app_name = g_Instance;

	Sys_LoadLibrary( &imglib_dll ); // load imagelib
	CreateImglib = (void *)imglib_dll.main;
	Image = CreateImglib( &com, NULL ); // second interface not allowed

	switch( app_name )
	{
	case RIPP_QCCDEC:
		Sys_LoadLibrary( &vprogs_dll ); // load qcclib
		CreateVprogs = (void *)vprogs_dll.main;
		PRVM = CreateVprogs( &com, NULL ); // second interface not allowed

		PRVM->Init( argc, argv );

		if(!FS_GetParmFromCmdLine("-dir", gamedir ))
			com.strncpy( gamedir, ".", sizeof(gamedir));
		start = Sys_DoubleTime();
		PRVM->PrepareDAT( gamedir, "progs.dat" ); // generic name
		break;
	default:
		FS_InitRootDir(".");
		Image->Init(); // initialize image support
		break;	
	}

	start = Sys_DoubleTime();
	Msg("Converting ...\n\n");
}

void RunConvertor( void )
{
	search_t	*search;
	string	errorstring;
	int	i, j, numConvertedRes = 0;

	memset( errorstring, 0, MAX_STRING ); 
	ClrMask();

	switch(app_name)
	{
	case RIPP_MIPDEC:
		search = FS_Search("textures/*", true );
		if( search )
		{
			// find subdirectories
			for(i = 0; i < search->numfilenames; i++)
			{
				AddMask(va("%s/*.wal", search->filenames[i]));
				AddMask(va("%s/*.jpg", search->filenames[i]));
			}
			Mem_Free( search );
		}
		AddMask( "maps/*.bsp" );	// textures from bsp
		AddMask( "*.flt" );		// Doom1 textures
		AddMask( "*.mip" );		// Quake1\Hl1 textures
		AddMask( "*.jpg" );		// Quake3 textures
		AddMask( "*.wal" );		// Quake2 textures
		break;
	case RIPP_SPRDEC:
		AddMask( "sprites/*.spr32");	// QW 32bit sprites
		AddMask( "sprites/*.sp2" );	// Quake2 sprites
		AddMask( "sprites/*.spr" );	// Quake1 and Half-Life sprites
		AddMask( "progs/*.spr32" );
		AddMask( "progs/*.spr" );
		AddMask( "*.spr" );
		AddMask( "*.sp2" );
		AddMask( "*.sp32");
		break;
	case RIPP_MDLDEC:
		AddMask( "*.skn" );		// Doom1 sprite models
		break;
	case RIPP_LMPDEC:
		AddMask( "pics/*.pcx");	// Quake2 pics
		AddMask( "gfx/*.lmp" );	// Quake1 pics
		AddMask( "*.fnt" );		// Half-Life fonts 
		AddMask( "*.flp" );		// Doom1 pics
		AddMask( "*.pcx" );
		AddMask( "*.lmp" );
		break;
	case RIPP_SNDDEC:
		AddMask( "*.snd" );
		AddMask( "*.mus" );
		break;
	case RIPP_QCCDEC:
		AddMask( "*.dat" );
		break;
	case RIPP_BSPDEC:
		Sys_Break(" not implemented\n");
		break;
	case HOST_OFFLINE:
	default: return;
	}

	if(FS_GetParmFromCmdLine("-file", gs_searchmask ))
	{
		ClrMask(); // clear all previous masks
		AddMask( gs_searchmask ); // custom mask
	}

	// directory to extract
	com.strncpy( gs_gamedir, "tmpQuArK", sizeof(gs_gamedir));

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
	if( numConvertedRes == 0 ) 
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

	Image->Free();
	Sys_FreeLibrary( &imglib_dll ); // free imagelib

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