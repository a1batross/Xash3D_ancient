//=======================================================================
//			Copyright XashXT Group 2007 ©
//			conv_main.c - resource converter
//=======================================================================

#include "ripper.h"

bool write_qscsript;
int game_family;

typedef struct convformat_s
{
	const char *path;
	const char *ext;
	bool (*convfunc)( const char *name, byte *buffer, size_t filesize, const char *ext );
	const char *ext2;	// save format
} convformat_t;

convformat_t convert_formats[] =
{
	{"%s.%s", "spr", ConvSPR, "bmp" },	// quake1/half-life sprite
	{"%s.%s","spr32",ConvSPR, "tga" },	// spr32 sprite
	{"%s.%s", "sp2", ConvSPR, "bmp" },	// quake2 sprite
	{"%s.%s", "jpg", ConvJPG, "tga" },	// quake3 textures
	{"%s.%s", "pcx", ConvPCX, "bmp" },	// quake2 pics
	{"%s.%s", "flt", ConvFLT, "bmp" },	// doom1 textures
	{"%s.%s", "flp", ConvFLP, "bmp" },	// doom1 menu pics
	{"%s.%s", "mip", ConvMIP, "bmp" },	// Quake1/Half-Life textures
	{"%s.%s", "lmp", ConvLMP, "bmp" },	// Quake1/Half-Life gfx
	{"%s.%s", "wal", ConvWAL, "bmp" },	// Quake2 textures
	{"%s.%s", "vtf", ConvVTF, "dds" },	// Half-Life 2 materials
	{"%s.%s", "skn", ConvSKN, "bmp" },	// doom1 sprite models
	{"%s.%s", "bsp", ConvBSP, "bmp" },	// Quake1\Half-Life map textures
	{"%s.%s", "mus", ConvMID, "mid" },	// doom1\2 music files
	{"%s.%s", "txt", ConvRAW, "txt" },	// (hidden) Xash-extract scripts
	{"%s.%s", "dat", ConvRAW, "dat" },	// (hidden) Xash-extract progs
	{NULL, NULL, NULL, NULL }		// list terminator
};

convformat_t convert_formats32[] =
{
	{"%s.%s", "spr", ConvSPR, "png" },	// quake1/half-life sprite
	{"%s.%s","spr32",ConvSPR, "dds" },	// spr32 sprite
	{"%s.%s", "sp2", ConvSPR, "dds" },	// quake2 sprite
	{"%s.%s", "jpg", ConvJPG, "jpg" },	// quake3 textures
	{"%s.%s", "bmp", ConvBMP, "dds" },	// 8-bit maps with alpha-cnahnel	
	{"%s.%s", "pcx", ConvPCX, "png" },	// quake2 pics
	{"%s.%s", "flt", ConvFLT, "png" },	// doom1 textures
	{"%s.%s", "flp", ConvFLP, "png" },	// doom1 menu pics
	{"%s.%s", "mip", ConvMIP, "png" },	// Quake1/Half-Life textures
	{"%s.%s", "lmp", ConvLMP, "png" },	// Quake1/Half-Life gfx
	{"%s.%s", "wal", ConvWAL, "png" },	// Quake2 textures
	{"%s.%s", "vtf", ConvVTF, "dds" },	// Half-Life 2 materials
	{"%s.%s", "skn", ConvSKN, "png" },	// doom1 sprite models
	{"%s.%s", "bsp", ConvBSP, "png" },	// Quake1\Half-Life map textures
	{"%s.%s", "mus", ConvMID, "mid" },	// doom1\2 music files
	{"%s.%s", "txt", ConvRAW, "txt" },	// (hidden) Xash-extract scripts
	{"%s.%s", "dat", ConvRAW, "dat" },	// (hidden) Xash-extract progs
	{NULL, NULL, NULL, NULL }		// list terminator
};

bool CheckForExist( const char *path, const char *ext )
{
	if( game_family == GAME_DOOM1 )
	{
		string	name, wad;

		// HACK: additional check for { symbol
		FS_ExtractFilePath( path, wad );
		FS_FileBase( path, name );
		if(FS_FileExists( va( "%s/%s/{%s.%s", gs_gamedir, wad, name, ext )))
			return true;
		// fallback to normal checking
	}
	return FS_FileExists( va( "%s/%s.%s", gs_gamedir, path, ext ));
}

bool ConvertResource( byte *mempool, const char *filename, byte parms )
{
	convformat_t	*format;
          const char	*ext = FS_FileExtension( filename );
	bool		anyformat = !com.stricmp(ext, "") ? true : false;
	string		path, convname, basename;
	int		filesize = 0;
	byte		*buffer = NULL;

	com.strncpy( convname, filename, sizeof( convname ));
	FS_StripExtension( convname ); // remove extension if needed

	if( FS_CheckParm( "-force32" ))
		format = convert_formats32;
	else format = convert_formats;

	// now try all the formats in the selected list
	for( ; format->convfunc; format++ )
	{
		if( anyformat || !com.stricmp( ext, format->ext ))
		{
			if( CheckForExist( convname, format->ext2 )) return true;	// already exist
			com.sprintf( path, format->path, convname, format->ext );
			buffer = FS_LoadFile( path, &filesize );
			if( buffer && filesize > 0 )
			{
				// this path may contains wadname: wadfile/lumpname
				if( format->convfunc( convname, buffer, filesize, format->ext2 ))
				{
					Mem_Free( buffer );	// release buffer
					return true;	// converted ok
				}
			}
			if( buffer ) Mem_Free( buffer );	// release buffer
		}
	}
	FS_FileBase( convname, basename );
	MsgDev( D_WARN, "ConvertResource: couldn't load \"%s\"\n", basename );
	return false;
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
	ClrMask();
}

void Conv_RunSearch( void )
{
	search_t	*search;
	int	i, j, k, imageflags;

	Conv_DetectGameType();
          
	if( game_family ) Msg( "Game: %s family\n", game_names[game_family] );
	imageflags = IL_USE_LERPING;
	write_qscsript = false;

	switch( game_family )
	{
	case GAME_DOOM1:
		search = FS_Search("*.wad", true );
		if( search )
		{
			// make sure, that we stored all files from all wads
			for( i = 0; i < search->numfilenames; i++ )
			{
				AddMask(va("%s/*.flt", search->filenames[i]));
				AddMask(va("%s/*.flp", search->filenames[i]));
				AddMask(va("%s/*.skn", search->filenames[i]));
				AddMask(va("%s/*.mus", search->filenames[i]));
			}
			Mem_Free( search );
		}
		else
		{
			// just use global mask
			AddMask( "*.skn" );		// Doom1 sprite models	
			AddMask( "*.flp" );		// Doom1 pics
			AddMask( "*.flt" );		// Doom1 textures
			AddMask( "*.mus" );		// Doom1 music
		}
		if( !FS_CheckParm( "-force32" ))
		{ 
			imageflags |= IL_KEEP_8BIT;
			write_qscsript = true;
		}
		Image_Init( "Doom1", imageflags );
		break;
	case GAME_HEXEN2:
	case GAME_QUAKE1:
		search = FS_Search("*.wad", true );
		// make sure, that we stored all files from all wads
		for( i = 0; search && i < search->numfilenames; i++ )
			AddMask(va("%s/*.mip", search->filenames[i]));
		if( search ) Mem_Free( search );
		else AddMask( "*.mip" );
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
		if( !FS_CheckParm( "-force32" ))
		{
			imageflags |= IL_KEEP_8BIT;
			write_qscsript = true;
		}
		Image_Init( "Quake1", imageflags );
		break;
	case GAME_QUAKE2:
		search = FS_Search("textures/*", true );

		// find subdirectories
		for( i = 0; search && i < search->numfilenames; i++ )
		{
			if( com.strstr( search->filenames[i], "/." )) continue;
			if( com.strstr( search->filenames[i], "/.." )) continue;
			if( com.stricmp(FS_FileExtension( search->filenames[i] ), "" ))
				continue;
			AddMask( va("%s/*.wal", search->filenames[i]));
		}
		if( search ) Mem_Free( search );
		else AddMask( "*.wal" );	// Quake2 textures
		AddMask( "*.sp2" );		// Quake2 sprites
		AddMask( "*.pcx" );		// Quake2 sprites
		AddMask( "sprites/*.sp2" );	// Quake2 sprites
		AddMask( "pics/*.pcx");	// Quake2 pics
		AddMask( "env/*.pcx" );	// Quake2 skyboxes
		if( !FS_CheckParm( "-force32" ))
		{
			imageflags |= IL_KEEP_8BIT;
			write_qscsript = true;
		}
		Image_Init( "Quake2", imageflags );
		break;
	case GAME_RTCW:
	case GAME_QUAKE3:
		search = FS_Search("textures/*", true );
		// find subdirectories
		for( i = 0; search && i < search->numfilenames; i++ )
		{
			if( com.strstr( search->filenames[i], "/." )) continue;
			if( com.strstr( search->filenames[i], "/.." )) continue;
			if( com.stricmp(FS_FileExtension( search->filenames[i] ), "" ))
				continue;
			AddMask(va("%s/*.jpg", search->filenames[i]));
		}
		if( search ) Mem_Free( search );
		else AddMask( "*.jpg" );	// Quake3 textures
		search = FS_Search("gfx/*", true );
		if( search )
		{
			// find subdirectories
			for( i = 0; i < search->numfilenames; i++ )
				AddMask(va("%s/*.jpg", search->filenames[i]));
			Mem_Free( search );
		}
		else  AddMask( "gfx/*.jpg" );	// Quake3 hud pics
		AddMask( "sprites/*.jpg" );	// Quake3 sprite frames
		Image_Init( "Quake3", imageflags );
		break;
	case GAME_HALFLIFE2:
	case GAME_HALFLIFE2_BETA:
		search = FS_Search("materials/*", true );

		// hl2 like using multiple included sub-dirs
		for( i = 0; search && i < search->numfilenames; i++ )
		{
			search_t	*search2;
			if( com.strstr( search->filenames[i], "/." )) continue;
			if( com.strstr( search->filenames[i], "/.." )) continue;
			if( com.stricmp(FS_FileExtension( search->filenames[i] ), "" ))
				continue;
			search2 = FS_Search( va("%s/*", search->filenames[i] ), true );
			AddMask(va("%s/*.vtf", search->filenames[i]));
			for( j = 0; search2 && j < search2->numfilenames; j++ )
			{
				search_t	*search3;
				if( com.strstr( search2->filenames[j], "/." )) continue;
				if( com.strstr( search2->filenames[j], "/.." )) continue;
				if( com.stricmp(FS_FileExtension( search2->filenames[j] ), "" ))
					continue;
				search3 = FS_Search( va("%s/*", search2->filenames[j] ), true );
				AddMask(va("%s/*.vtf", search2->filenames[j]));
				for( k = 0; search3 && k < search3->numfilenames; k++ )
				{
					if( com.strstr( search3->filenames[k], "/." )) continue;
					if( com.strstr( search3->filenames[k], "/.." )) continue;
					if( com.stricmp(FS_FileExtension( search3->filenames[k] ), "" ))
						continue;
					AddMask(va("%s/*.vtf", search3->filenames[k]));
				}
				if( search3 ) Mem_Free( search3 );
			}
			if( search2 ) Mem_Free( search2 );
		}
		if( search ) Mem_Free( search );
		else AddMask( "*.jpg" );	// Quake3 textures
		imageflags |= IL_DDS_HARDWARE; // because we want save textures into original DXT format
		Image_Init( "Half-Life 2", imageflags );
		break;
	case GAME_QUAKE4:
		// i'm lazy today and i'm forget textures representation of IdTech4 :)
		Sys_Break( "Sorry, nothing to decompile (not implemeneted yet)\n" );
		Image_Init( "Quake4", imageflags );
		break;
	case GAME_HALFLIFE:
		search = FS_Search( "*.wad", true );
		if( search )
		{
			// find subdirectories
			for( i = 0; i < search->numfilenames; i++ )
			{
				AddMask(va("%s/*.mip", search->filenames[i]));
				AddMask(va("%s/*.lmp", search->filenames[i]));
			}
			Mem_Free( search );
		}
		else
		{
			// try to use generic mask
			AddMask( "*.mip" );
			AddMask( "*.lmp" );
		}
		AddMask( "maps/*.bsp" );	// textures from bsp
		AddMask( "sprites/*.spr" );	// Half-Life sprites
		if( !FS_CheckParm( "-force32" ))
		{
			imageflags |= IL_KEEP_8BIT;
			write_qscsript = true;
		}
		Image_Init( "Half-Life", imageflags );
		break;
	case GAME_XASH3D:
		Sys_Break("Sorry, but i'm can't decompile himself\n" );
		Image_Init( "Xash3D", imageflags );
		break;
	default:
		Image_Init( "Generic", imageflags );	// everything else
		break;
	}
}