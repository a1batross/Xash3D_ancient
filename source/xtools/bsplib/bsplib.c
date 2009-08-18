/* -------------------------------------------------------------------------------

Copyright (C) 1999-2007 id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

-------------------------------------------------------------------------------

This code has been altered significantly from its original form, to support
several games based on the Quake III Arena engine, in the form of "Q3Map2."

------------------------------------------------------------------------------- */



/* marker */
#define MAIN_C



/* dependencies */
#include "q3map2.h"
#include "byteorder.h"


/*
Random()
returns a pseudorandom number between 0 and 1
*/

vec_t Random( void )
{
	return (vec_t) rand() / RAND_MAX;
}

/*
ExitQ3Map()
cleanup routine
*/

static void ExitQ3Map( void )
{
	BSPFilesCleanup();
	if( mapDrawSurfs != NULL )
		Mem_Free( mapDrawSurfs );
}

/*
FixClipMap()
resets an clipmap checksum to match the given BSP
*/
int FixClipMap( int argc, char **argv )
{
	int	length, checksum;
	void	*buffer;
	file_t	*file;
	char	clipmap[MAX_SYSPATH];
	
	/* arg checking */
	if( argc < 2 )
	{
		Msg( "Usage: q3map -fixaas [-v] <mapname>\n" );
		return 0;
	}
	
	/* do some path mangling */
	strcpy( source, argv[argc-1] );
	FS_StripExtension( source );
	FS_DefaultExtension( source, ".bsp" );
	
	Msg( "--- FixClipMap ---\n" );
	
	Msg( "Loading %s\n", source );
	buffer = FS_LoadFile( source, &length );
	if( !buffer ) Sys_Break( "can't load %s\n", source );
	
	/* create bsp checksum */
	Msg( "Creating checksum...\n" );
	checksum = LittleLong( Com_BlockChecksum( buffer, length ));
	Mem_Free( buffer );
	
	/* mangle name */
	FS_FileBase( source, source );
	com.sprintf( clipmap, "maps/clipmaps/%s.bin", source );
	
	/* fix it */
	file = FS_Open( clipmap, "r+b" );
	if( !file ) return 1;

	FS_Seek( file, 0, SEEK_SET );
	if( FS_Write( file, &checksum, sizeof( int )) != sizeof( int ))
		Sys_Break( "Error writing checksum to %s\n", clipmap );
	FS_Close( file );
	
	return 0;
}



/*
AnalyzeBSP() - ydnar
analyzes a Quake engine BSP file
*/

typedef struct abspHeader_s
{
	char		ident[4];
	int		version;
	
	bspLump_t		lumps[1];	// unknown size
} abspHeader_t;

typedef struct abspLumpTest_s
{
	int		radix, minCount;
	char		*name;
} abspLumpTest_t;

static abspLumpTest_t lumpTests[] = 
{
{ sizeof( bspPlane_t ),	6,	"IBSP LUMP_PLANES" },
{ sizeof( bspBrush_t ),	1,	"IBSP LUMP_BRUSHES" },
{ 8,			6,	"IBSP LUMP_BRUSHSIDES" },
{ sizeof( bspBrushSide_t ),	6,	"RBSP LUMP_BRUSHSIDES" },
{ sizeof( bspModel_t ),	1,	"IBSP LUMP_MODELS" },
{ sizeof( bspNode_t ),	2,	"IBSP LUMP_NODES" },
{ sizeof( bspLeaf_t ),	1,	"IBSP LUMP_LEAFS" },
{ 104,			3,	"IBSP LUMP_DRAWSURFS" },
{ 44,			3,	"IBSP LUMP_DRAWVERTS" },
{ 4,			6,	"IBSP LUMP_DRAWINDEXES" },
{ 128 * 128 * 3,		1,	"IBSP LUMP_LIGHTMAPS" },
{ 256 * 256 * 3,		1,	"IBSP LUMP_LIGHTMAPS (256 x 256)" },
{ 512 * 512 * 3,		1,	"IBSP LUMP_LIGHTMAPS (512 x 512)" },
{ 0, 0, NULL }
};
	
int AnalyzeBSP( int argc, char **argv )
{
	abspHeader_t	*header;
	int		size, i, version, offset, length, lumpInt, count;
	char		ident[5];
	void		*lump;
	float		lumpFloat;
	char		lumpString[MAX_SYSPATH], source[MAX_SYSPATH];
	bool		lumpSwap = false;
	abspLumpTest_t	*lumpTest;
	
	/* arg checking */
	if( argc < 1 )
	{
		Msg( "Usage: q3map -analyze [-lumpswap] [-v] <mapname>\n" );
		return 0;
	}
	
	/* process arguments */
	for( i = 1; i < (argc - 1); i++ )
	{
		/* -format map|ase|... */
		if( !strcmp( argv[i],  "-lumpswap" ) )
		{
			Msg( "Swapped lump structs enabled\n" );
 			lumpSwap = true;
 		}
	}
	
	/* clean up map name */
	com.strcpy( source, argv[i] );
	Msg( "Loading %s\n", source );
	
	/* load the file */
	header = (void *)FS_LoadFile( source, &size );
	if( size == 0 || header == NULL )
	{
		Msg( "Unable to load %s.\n", source );
		return -1;
	}
	
	/* analyze ident/version */
	Mem_Copy( ident, header->ident, 4 );
	ident[4] = '\0';
	version = LittleLong( header->version );
	
	Msg( "Identity:      %s\n", ident );
	Msg( "Version:       %d\n", version );
	Msg( "---------------------------------------\n" );
	
	// analyze each lump
	for( i = 0; i < 100; i++ )
	{
		/* call of duty swapped lump pairs */
		if( lumpSwap )
		{
			offset = LittleLong( header->lumps[ i ].length );
			length = LittleLong( header->lumps[ i ].offset );
		}
		
		/* standard lump pairs */
		else
		{
			offset = LittleLong( header->lumps[ i ].offset );
			length = LittleLong( header->lumps[ i ].length );
		}
		
		/* extract data */
		lump = (byte*) header + offset;
		lumpInt = LittleLong( (int) *((int*) lump) );
		lumpFloat = LittleFloat( (float) *((float*) lump) );
		memcpy( lumpString, (char*) lump, (length < 1024 ? length : 1024) );
		lumpString[ 1024 ] = '\0';
		
		/* print basic lump info */
		Msg( "Lump:          %d\n", i );
		Msg( "Offset:        %d bytes\n", offset );
		Msg( "Length:        %d bytes\n", length );
		
		/* only operate on valid lumps */
		if( length > 0 )
		{
			/* print data in 4 formats */
			Msg( "As hex:        %08X\n", lumpInt );
			Msg( "As int:        %d\n", lumpInt );
			Msg( "As float:      %f\n", lumpFloat );
			Msg( "As string:     %s\n", lumpString );
			
			/* guess lump type */
			if( lumpString[ 0 ] == '{' && lumpString[ 2 ] == '"' )
				Msg( "Type guess:    IBSP LUMP_ENTITIES\n" );
			else if( strstr( lumpString, "textures/" ) )
				Msg( "Type guess:    IBSP LUMP_SHADERS\n" );
			else
			{
				/* guess based on size/count */
				for( lumpTest = lumpTests; lumpTest->radix > 0; lumpTest++ )
				{
					if( (length % lumpTest->radix) != 0 )
						continue;
					count = length / lumpTest->radix;
					if( count < lumpTest->minCount )
						continue;
					Msg( "Type guess:    %s (%d x %d)\n", lumpTest->name, count, lumpTest->radix );
				}
			}
		}
		
		Msg( "---------------------------------------\n" );
		
		/* end of file */
		if( offset + length >= size )
			break;
	}
	
	/* last stats */
	Msg( "Lump count:    %d\n", i + 1 );
	Msg( "File size:     %d bytes\n", size );
	
	/* return to caller */
	return 0;
}



/*
BSPInfo()
emits statistics about the bsp file
*/

int BSPInfo( int count, char **fileNames )
{
	int		i;
	char		source[MAX_SYSPATH];
	const char	*ext;
	int		size;
	file_t		*f;
	
	
	/* dummy check */
	if( count < 1 )
	{
		Msg( "No files to dump info for.\n");
		return -1;
	}
	
	/* enable info mode */
	infoMode = true;
	
	/* walk file list */
	for( i = 0; i < count; i++ )
	{
		Msg( "---------------------------------\n" );
		
		/* mangle filename and get size */
		strcpy( source, fileNames[ i ] );
		ext = FS_FileExtension( source );
		if( !com.stricmp( ext, "map" ))
			FS_StripExtension( source );
		FS_DefaultExtension( source, ".bsp" );
		f = FS_Open( source, "rb" );
		if( f )
		{
			FS_Seek( f, 0, SEEK_END );
			size = FS_Tell( f );
			FS_Close( f );
		}
		else size = 0;
		
		/* load the bsp file and print lump sizes */
		Msg( "%s\n", source );
		LoadBSPFile( source );		
		PrintBSPFileSizes();
		
		/* print sizes */
		Msg( "\n" );
		Msg( "          total         %9d\n", size );
		Msg( "                        %9d KB\n", size / 1024 );
		Msg( "                        %9d MB\n", size / (1024 * 1024) );
		
		Msg( "---------------------------------\n" );
	}
	
	/* return count */
	return i;
}



/*
ScaleBSPMain()
amaze and confuse your enemies with wierd scaled maps!
*/

int ScaleBSPMain( int argc, char **argv )
{
	int		i;
	float		f, scale;
	vec3_t		vec;
	char		str[MAX_SYSPATH];
	
	
	/* arg checking */
	if( argc < 2 )
	{
		Msg( "Usage: q3map -scale <value> [-v] <mapname>\n" );
		return 0;
	}
	
	/* get scale */
	scale = com.atof( argv[argc - 2] );
	if( scale == 0.0f )
	{
		Msg( "Usage: q3map -scale <value> [-v] <mapname>\n" );
		Msg( "Non-zero scale value required.\n" );
		return 0;
	}
	
	/* do some path mangling */
	strcpy( source, argv[ argc - 1 ] );
	FS_StripExtension( source );
	FS_DefaultExtension( source, ".bsp" );
	
	/* load the bsp */
	Msg( "Loading %s\n", source );
	LoadBSPFile( source );
	ParseEntities();
	
	/* note it */
	Msg( "--- ScaleBSP ---\n" );
	MsgDev( D_NOTE, "%9d entities\n", numEntities );
	
	/* scale entity keys */
	for( i = 0; i < numBSPEntities && i < numEntities; i++ )
	{
		// scale origin
		GetVectorForKey( &entities[ i ], "origin", vec );
		if( (vec[0] + vec[1] + vec[2]) )
		{
			VectorScale( vec, scale, vec );
			com.sprintf( str, "%f %f %f", vec[0], vec[1], vec[2] );
			SetKeyValue( &entities[ i ], "origin", str );
		}
		
		// scale door lip
		f = FloatForKey( &entities[ i ], "lip" );
		if( f )
		{
			f *= scale;
			com.sprintf( str, "%f", f );
			SetKeyValue( &entities[ i ], "lip", str );
		}
	}
	
	/* scale models */
	for( i = 0; i < numBSPModels; i++ )
	{
		VectorScale( bspModels[ i ].mins, scale, bspModels[ i ].mins );
		VectorScale( bspModels[ i ].maxs, scale, bspModels[ i ].maxs );
	}
	
	/* scale nodes */
	for( i = 0; i < numBSPNodes; i++ )
	{
		VectorScale( bspNodes[ i ].mins, scale, bspNodes[ i ].mins );
		VectorScale( bspNodes[ i ].maxs, scale, bspNodes[ i ].maxs );
	}
	
	/* scale leafs */
	for( i = 0; i < numBSPLeafs; i++ )
	{
		VectorScale( bspLeafs[ i ].mins, scale, bspLeafs[ i ].mins );
		VectorScale( bspLeafs[ i ].maxs, scale, bspLeafs[ i ].maxs );
	}
	
	/* scale drawverts */
	for( i = 0; i < numBSPDrawVerts; i++ )
		VectorScale( bspDrawVerts[ i ].xyz, scale, bspDrawVerts[ i ].xyz );
	
	/* scale planes */
	for( i = 0; i < numBSPPlanes; i++ )
		bspPlanes[ i ].dist *= scale;
	
	/* scale gridsize */
	GetVectorForKey( &entities[ 0 ], "gridsize", vec );
	if((vec[0] + vec[1] + vec[2]) == 0.0f ) VectorCopy( gridSize, vec );
	VectorScale( vec, scale, vec );
	com.sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
	SetKeyValue( &entities[ 0 ], "gridsize", str );
	
	/* write the bsp */
	UnparseEntities();
	FS_StripExtension( source );
	FS_DefaultExtension( source, "_s.bsp" );
	Msg( "Writing %s\n", source );
	WriteBSPFile( source );
	
	return 0;
}



/*
ConvertBSPMain()
main argument processing function for bsp conversion
*/

int ConvertBSPMain( int argc, char **argv )
{
	int	i;
	int	(*convertFunc)( char * );
	game_t	*convertGame;
	
	
	convertFunc = ConvertBSPToASE;
	convertGame = NULL;
	
	/* arg checking */
	if( argc < 1 )
	{
		Msg( "Usage: q3map -scale <value> [-v] <mapname>\n" );
		return 0;
	}
	
	/* process arguments */
	for( i = 1; i < (argc - 1); i++ )
	{
		/* -format map|ase|... */
		if( !strcmp( argv[ i ],  "-format" ) )
 		{
			i++;
			if( !com.stricmp( argv[ i ], "ase" ) )
				convertFunc = ConvertBSPToASE;
			else if( !com.stricmp( argv[ i ], "map" ) )
				convertFunc = ConvertBSPToMap;
			else
			{
				convertGame = GetGame( argv[ i ] );
				if( convertGame == NULL )
					Msg( "Unknown conversion format \"%s\". Defaulting to ASE.\n", argv[ i ] );
			}
 		}
	}
	
	/* clean up map name */
	strcpy( source, argv[ i ] );
	FS_StripExtension( source );
	FS_DefaultExtension( source, ".bsp" );
	
	LoadShaderInfo();
	
	Msg( "Loading %s\n", source );
	
	/* ydnar: load surface file */
	//%	LoadSurfaceExtraFile( source );
	
	LoadBSPFile( source );
	
	/* parse bsp entities */
	ParseEntities();
	
	/* bsp format convert? */
	if( convertGame != NULL )
	{
		/* set global game */
		game = convertGame;
		
		/* write bsp */
		FS_StripExtension( source );
		FS_DefaultExtension( source, "_c.bsp" );
		Msg( "Writing %s\n", source );
		WriteBSPFile( source );
		
		/* return to sender */
		return 0;
	}
	return convertFunc( source );
}



/*
main()
q3map mojo...
*/

bool Q3MapMain( int argc, char **argv )
{
	int	i, r;
	double	start, end;
	
	
	/* we want consistent 'randomness' */
	srand( 0 );
	
	/* start timer */
	start = Sys_DoubleTime();

	/* this was changed to emit version number over the network */
	Msg( Q3MAP_VERSION "\n" );
	
	/* set exit call */
	atexit( ExitQ3Map );
	
	/* read general options first */
	for( i = 1; i < argc; i++ )
	{
		/* force */
		if( !strcmp( argv[ i ], "-force" ) )
		{
			force = true;
			argv[ i ] = NULL;
		}
		
		/* patch subdivisions */
		else if( !strcmp( argv[ i ], "-subdivisions" ) )
		{
			argv[ i ] = NULL;
			i++;
			patchSubdivisions = atoi( argv[ i ] );
			argv[ i ] = NULL;
			if( patchSubdivisions <= 0 )
				patchSubdivisions = 1;
		}
	}
	
	/* init model library */
	PicoInit();
	PicoSetMallocFunc( NULL );
	PicoSetFreeFunc( NULL );
	PicoSetPrintFunc( PicoPrintFunc );
	PicoSetLoadFileFunc( PicoLoadFileFunc );
	PicoSetFreeFileFunc( NULL );
	
	/* generate sinusoid jitter table */
	for( i = 0; i < MAX_JITTERS; i++ )
		jitters[ i ] = sin( i * 139.54152147 );
	
	/* we print out two versions, q3map's main version (since it evolves a bit out of GtkRadiant)
	   and we put the GtkRadiant version to make it easy to track with what version of Radiant it was built with */
	Msg( "Q3Map         - v1.0r (c) 1999 Id Software Inc.\n" );
	Msg( "Q3Map (ydnar) - v" Q3MAP_VERSION "\n" );
	Msg( "%s\n", Q3MAP_MOTD );
	
	/* ydnar: new path initialization */
	InitPaths( &argc, argv );
	
	/* check if we have enough options left to attempt something */
	if( argc < 2 ) Sys_Break( "Usage: %s [general options] [options] mapfile", argv[ 0 ] );
	
	/* fixaas */
	if( !strcmp( argv[ 1 ], "-fixclip" ) )
		r = FixClipMap( argc - 1, argv + 1 );
	
	/* analyze */
	else if( !strcmp( argv[ 1 ], "-analyze" ) )
		r = AnalyzeBSP( argc - 1, argv + 1 );
	
	/* info */
	else if( !strcmp( argv[ 1 ], "-info" ) )
		r = BSPInfo( argc - 2, argv + 2 );
	
	/* vis */
	else if( !strcmp( argv[ 1 ], "-vis" ) )
		r = VisMain( argc - 1, argv + 1 );
	
	/* light */
	else if( !strcmp( argv[ 1 ], "-light" ) )
		r = LightMain( argc - 1, argv + 1 );
	
	/* vlight */
	else if( !strcmp( argv[ 1 ], "-vlight" ) )
	{
		Msg( "WARNING: VLight is no longer supported, defaulting to -light -fast instead\n\n" );
		argv[ 1 ] = "-fast";	/* eek a hack */
		r = LightMain( argc, argv );
	}
	
	/* ydnar: lightmap export */
	else if( !strcmp( argv[ 1 ], "-export" ) )
		r = ExportLightmapsMain( argc - 1, argv + 1 );
	
	/* ydnar: lightmap import */
	else if( !strcmp( argv[ 1 ], "-import" ) )
		r = ImportLightmapsMain( argc - 1, argv + 1 );
	
	/* ydnar: bsp scaling */
	else if( !strcmp( argv[ 1 ], "-scale" ) )
		r = ScaleBSPMain( argc - 1, argv + 1 );
	
	/* ydnar: bsp conversion */
	else if( !strcmp( argv[ 1 ], "-convert" ) )
		r = ConvertBSPMain( argc - 1, argv + 1 );
	
	/* ydnar: otherwise create a bsp */
	else
		r = BSPMain( argc, argv );
	
	/* emit time */
	end = Sys_DoubleTime();
	Msg( "%9.0f seconds elapsed\n", end - start );
	
	return r;
}
