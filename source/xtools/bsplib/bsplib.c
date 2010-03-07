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

bool	(*BspFunc)( int argc, char **argv );
int	com_argc;
char	**com_argv;

/*
Random()
returns a pseudorandom number between 0 and 1
*/

vec_t Random( void )
{
	return (vec_t) rand() / RAND_MAX;
}

void Bsp_PrintLog( const char *pMsg )
{
	if( !enable_log ) return;
	if( !bsplog ) bsplog = FS_Open( va( "maps/%s.log", gs_filename ), "ab" );
	FS_Print( bsplog, pMsg );
}

/*
GetGame() - ydnar
gets the game_t based on a -game argument
returns NULL if no match found
*/
game_t *GetGame( char *arg )
{
	if( !com.stricmp( arg, "q3a" ))
		return &games[0];
	if( !com.stricmp( arg, "xash" ))
		return &games[1];
	return NULL;
}


void Bsp_Shutdown( void )
{
	BSPFilesCleanup();
	if( mapDrawSurfs != NULL )
		Mem_Free( mapDrawSurfs );
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

void InitGameType( void )
{

}

/*
main()
q3map mojo...
*/

bool PrepareBSPModel( int argc, char **argv )
{
	int	i;

	srand( 0 );

	com_argc = argc;
	com_argv = argv;

	// check for general parms
	if( FS_CheckParm( "-force" )) force = true;
	patch_subdivide = Cvar_Get( "bsp_patch_subdivide", "8", CVAR_SYSTEMINFO, "bsplib patch subdivisions" );
	Cvar_SetValue( "bsp_patch_subdivide", bound( 1, patch_subdivide->integer, 128 )); 	
	
	// init model library
	PicoInit();
	PicoSetMallocFunc( PicoMalloc );
	PicoSetFreeFunc( PicoFree );
	PicoSetPrintFunc( PicoPrintFunc );
	PicoSetLoadFileFunc( PicoLoadFileFunc );
	PicoSetFreeFileFunc( PicoFree );
		
	// generate sinusoidal jitter table
	for( i = 0; i < MAX_JITTERS; i++ )
		jitters[ i ] = sin( i * 139.54152147 );
	
	game = &games[1];	// defaulting to Xash
	FS_LoadGameInfo( NULL );
	BspFunc = NULL;

	if( FS_CheckParm( "-analyze" )) BspFunc = AnalyzeBSP;
	else if( FS_CheckParm( "-info" )) BspFunc = BSPInfo;
	else if( FS_CheckParm( "-vis" )) BspFunc = VisMain;
	else if( FS_CheckParm( "-light" )) BspFunc = LightMain;
	else if( FS_CheckParm( "-export" )) BspFunc = ExportLightmapsMain;
	else if( FS_CheckParm( "-import" )) BspFunc = ImportLightmapsMain;
	else if( FS_CheckParm( "-scale" )) BspFunc = ScaleBSPMain;
	else if( FS_CheckParm( "-convert" )) BspFunc = ConvertBSPMain;
	else BspFunc = BSPMain;

	return true;
}

bool CompileBSPModel ( void )
{
	if( !BspFunc ) return false;

	return BspFunc( com_argc, com_argv );
}