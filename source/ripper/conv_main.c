//=======================================================================
//			Copyright XashXT Group 2007 ©
//			conv_main.c - resource converter
//=======================================================================

#include "ripper.h"
#include "qc_gen.h"

stdlib_api_t com;
byte *basepool;
byte *zonepool;
static double start, end;
string gs_gamedir;
string gs_searchmask;

#define	MAX_SEARCHMASK	128
string	searchmask[MAX_SEARCHMASK];
int	num_searchmask = 0;
int	game_family;

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
	{"%s.%s", "wal", ConvWAL},	// Quake2 textures
	{"%s.%s", "skn", ConvSKN},	// doom1 sprite models
	{"%s.%s", "bsp", ConvBSP},	// Quake1\Half-Life map textures
	{"%s.%s", "mus", ConvMID},	// Quake1\Half-Life map textures
	{"%s.%s", "snd", ConvSND},	// Quake1\Half-Life map textures
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
	FS_StripExtension( convname ); // remove extension if needed

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

void Conv_DetectGameType( void )
{
	int	i;
	search_t	*search;

	game_family = GAME_GENERIC;

	// first, searching for map format
	AddMask( "maps/*.bsp" );

	// search by mask		
	search = FS_Search( searchmask[0], true );
	for( i = 0; search && i < search->numfilenames; i++ )
	{
		if(Conv_CheckMap( search->filenames[i] ))
			break;
	}
	if( search ) Mem_Free( search );
	ClrMask();

	// game family sucessfully determined
	if( game_family != GAME_GENERIC ) return;

	AddMask( "*.wad" );

	// search by mask		
	search = FS_Search( searchmask[0], true );
	for( i = 0; search && i < search->numfilenames; i++ )
	{
		if(Conv_CheckWad( search->filenames[i] ))
			break;
	}
	if( search ) Mem_Free( search );
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
	
	// init pools
	basepool = Mem_AllocPool( "Temp" );
	zonepool = Mem_AllocPool( "Zone" );
	FS_InitRootDir(".");

	start = Sys_DoubleTime();
	Msg("\n\n");
}

void RunConvertor( void )
{
	search_t	*search;
	string	errorstring;
	int	i, j, numConvertedRes = 0;
	cvar_t	*fs_defaultdir = Cvar_Get( "fs_defaultdir", "tmpQuArK", CVAR_SYSTEMINFO, NULL );

	memset( errorstring, 0, MAX_STRING ); 
	ClrMask();
	Conv_DetectGameType();
          
	if( game_family ) Msg("Game: %s family\n", game_names[game_family] );

	switch( game_family )
	{
	case GAME_DOOM1:
		AddMask( "*.skn" );		// Doom1 sprite models	
		AddMask( "*.flp" );		// Doom1 pics
		AddMask( "*.flt" );		// Doom1 textures
		AddMask( "*.snd" );		// Doom1 sounds
		AddMask( "*.mus" );		// Doom1 music
		break;
	case GAME_HEXEN2:
	case GAME_QUAKE1:
		AddMask( "maps/*.bsp" );	// Quake1 textures from bsp
		AddMask( "sprites/*.spr" );	// Quake1 sprites
		AddMask( "sprites/*.spr32" );	// QW 32bit sprites
		AddMask( "progs/*.spr32" );	// FTE, Darkplaces new sprites
		AddMask( "progs/*.spr32" );
		AddMask( "progs/*.spr" );
		AddMask( "env/it/*.lmp" );	// Nehahra issues
		AddMask( "gfx/*.mip" );	// Quake1 gfx/conchars
		AddMask( "gfx/*.lmp" );	// Quake1 pics
		AddMask( "*.sp32");
		AddMask( "*.spr" );
		AddMask( "*.mip" );
		break;
	case GAME_QUAKE2:
		search = FS_Search("textures/*", true );
		if( search )
		{
			// find subdirectories
			for( i = 0; i < search->numfilenames; i++ )
				AddMask(va("%s/*.wal", search->filenames[i]));
			Mem_Free( search );
		}
		AddMask( "*.wal" );		// Quake2 textures
		AddMask( "*.sp2" );		// Quake2 sprites
		AddMask( "*.pcx" );		// Quake2 sprites
		AddMask( "sprites/*.sp2" );	// Quake2 sprites
		AddMask( "pics/*.pcx");	// Quake2 pics
		AddMask( "env/*.pcx" );	// Quake2 skyboxes
		break;
	case GAME_RTCW:
	case GAME_QUAKE3:
	case GAME_QUAKE4:
	case GAME_HALFLIFE2:
	case GAME_HALFLIFE2_BETA:
		Sys_Break("Sorry, nothing to decompile (not implemeneted yet)\n" );
		break;
	case GAME_HALFLIFE:
		AddMask( "maps/*.bsp" );	// textures from bsp
		AddMask( "sprites/*.spr" );	// Half-Life sprites
		AddMask( "gfx/*.lmp" );	// some images
		AddMask( "*.mip" );		// all textures from wads
		AddMask( "*.spr" );
		AddMask( "*.lmp" );
		break;
	case GAME_XASH3D:
		Sys_Break("Sorry, but a can't decompile himself\n" );
		break;
	case HOST_OFFLINE:
	default:	Sys_Break("Sorry, game family not recognized\n" );
		break;
	}

	if(FS_GetParmFromCmdLine("-file", gs_searchmask ))
	{
		ClrMask(); // clear all previous masks
		AddMask( gs_searchmask ); // custom mask
	}

	// directory to extract
	com.strncpy( gs_gamedir, fs_defaultdir->string, sizeof(gs_gamedir));
	Msg( "Converting ...\n\n" );

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