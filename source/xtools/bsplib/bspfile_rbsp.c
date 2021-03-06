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

----------------------------------------------------------------------------------

This code has been altered significantly from its original form, to support
several games based on the Quake III Arena engine, in the form of "Q3Map2."

------------------------------------------------------------------------------- */



/* marker */
#define BSPFILE_RBSP_C



/* dependencies */
#include "q3map2.h"
#include "byteorder.h"



/* -------------------------------------------------------------------------------

this file handles translating the bsp file format used by quake 3, rtcw, and ef
into the abstracted bsp file used by q3map2.

------------------------------------------------------------------------------- */

/* constants */
#define	LUMP_ENTITIES		0
#define	LUMP_SHADERS		1
#define	LUMP_PLANES		2
#define	LUMP_NODES		3
#define	LUMP_LEAFS		4
#define	LUMP_LEAFSURFACES		5
#define	LUMP_LEAFBRUSHES		6
#define	LUMP_MODELS		7
#define	LUMP_BRUSHES		8
#define	LUMP_BRUSHSIDES		9
#define	LUMP_DRAWVERTS		10
#define	LUMP_DRAWINDEXES		11
#define	LUMP_FOGS			12
#define	LUMP_SURFACES		13
#define	LUMP_LIGHTMAPS		14
#define	LUMP_LIGHTGRID		15
#define	LUMP_VISIBILITY		16
#define	LUMP_LIGHTARRAY		17
#define	HEADER_LUMPS		18


/* types */
typedef struct
{
	char		ident[4];
	int		version;
	
	bspLump_t		lumps[HEADER_LUMPS];
} rbspHeader_t;



/* light grid */
#define MAX_MAP_GRID	0xffff
#define MAX_MAP_GRIDARRAY	0x100000
#define LG_EPSILON		4


static void CopyLightGridLumps( rbspHeader_t *header )
{
	int				i;
	unsigned short	*inArray;
	bspGridPoint_t	*in, *out;
	
	
	/* get count */
	numBSPGridPoints = GetLumpElements( (bspHeader_t*) header, LUMP_LIGHTARRAY, sizeof( *inArray ) );
	
	/* allocate buffer */
	bspGridPoints = Malloc( numBSPGridPoints * sizeof( *bspGridPoints ));
	
	/* copy */
	inArray = GetLump( (bspHeader_t*) header, LUMP_LIGHTARRAY );
	in = GetLump( (bspHeader_t*) header, LUMP_LIGHTGRID );
	out = bspGridPoints;
	for( i = 0; i < numBSPGridPoints; i++ )
	{
		Mem_Copy( out, &in[ *inArray ], sizeof( *in ) );
		inArray++;
		out++;
	}
}


static void AddLightGridLumps( file_t *file, rbspHeader_t *header )
{
	int		i, j, k, c, d;
	int		numGridPoints, maxGridPoints;
	bspGridPoint_t	*gridPoints, *in, *out;
	int		numGridArray;
	unsigned short	*gridArray;
	bool		bad;
	
	
	/* allocate temporary buffers */
	maxGridPoints = (numBSPGridPoints < MAX_MAP_GRID) ? numBSPGridPoints : MAX_MAP_GRID;
	gridPoints = Malloc( maxGridPoints * sizeof( *gridPoints ) );
	gridArray = Malloc( numBSPGridPoints * sizeof( *gridArray ) );
	
	/* zero out */
	numGridPoints = 0;
	numGridArray = numBSPGridPoints;
	
	/* for each bsp grid point, find an approximate twin */
	MsgDev( D_NOTE, "Storing lightgrid: %d points\n", numBSPGridPoints );
	for( i = 0; i < numGridArray; i++ )
	{
		/* get points */
		in = &bspGridPoints[ i ];
		
		/* walk existing list */
		for( j = 0; j < numGridPoints; j++ )
		{
			/* get point */
			out = &gridPoints[ j ];
			
			/* compare styles */
			if( *((unsigned int*) in->styles) != *((unsigned int*) out->styles) )
				continue;
			
			/* compare direction */
			d = abs( in->latLong[ 0 ] - out->latLong[ 0 ] );
			if( d < (255 - LG_EPSILON) && d > LG_EPSILON )
				continue;
			d = abs( in->latLong[ 1 ] - out->latLong[ 1 ] );
			if( d < 255 - LG_EPSILON && d > LG_EPSILON )
				continue;
			
			/* compare light */
			bad = false;
			for( k = 0; (k < MAX_LIGHTMAPS && bad == false); k++ )
			{
				for( c = 0; c < 3; c++ )
				{
					if( abs( (int) in->ambient[ k ][ c ] - (int) out->ambient[ k ][ c ]) > LG_EPSILON ||
						abs( (int) in->directed[ k ][ c ] - (int) out->directed[ k ][ c ]) > LG_EPSILON )
					{
						bad = true;
						break;
					}
				}
			}
			
			/* failure */
			if( bad )
				continue;
			
			/* this sample is ok */
			break;
		}
		
		/* set sample index */
		gridArray[ i ] = (unsigned short) j;
		
		/* if no sample found, add a new one */
		if( j >= numGridPoints && numGridPoints < maxGridPoints )
		{
			out = &gridPoints[ numGridPoints++ ];
			Mem_Copy( out, in, sizeof( *in ) );
		}
	}
	
	/* swap array */
	for( i = 0; i < numGridArray; i++ )
		gridArray[ i ] = LittleShort( gridArray[ i ] );
	
	/* write lumps */
	AddLump( file, (bspHeader_t*) header, LUMP_LIGHTGRID, gridPoints, (numGridPoints * sizeof( *gridPoints )) );
	AddLump( file, (bspHeader_t*) header, LUMP_LIGHTARRAY, gridArray, (numGridArray * sizeof( *gridArray )) );
	
	/* free buffers */
	if( gridPoints ) Mem_Free( gridPoints );
	if( gridArray ) Mem_Free( gridArray );
}



/*
LoadRBSPFile()
loads a raven bsp file into memory
*/

void LoadRBSPFile( const char *filename )
{
	rbspHeader_t	*header;
	
	/* load the file header */
	header = (rbspHeader_t *)FS_LoadFile( filename, NULL );
	if( !header ) Sys_Break( "couldn't load %s\n", filename );
	
	/* swap the header (except the first 4 bytes) */
	SwapBlock( (int*) ((byte*) header + sizeof( int )), sizeof( *header ) - sizeof( int ) );
	
	/* make sure it matches the format we're trying to load */
	if( force == false && *((int*) header->ident) != *((int*) game->bspIdent) )
		Sys_Break( "%s is not a %s file", filename, game->bspIdent );
	if( force == false && header->version != game->bspVersion )
		Sys_Break( "%s is version %d, not %d", filename, header->version, game->bspVersion );
	
	/* load/convert lumps */
	numBSPShaders = CopyLump( (bspHeader_t*) header, LUMP_SHADERS, bspShaders, sizeof( bspShader_t ) );
	numBSPModels = CopyLump( (bspHeader_t*) header, LUMP_MODELS, bspModels, sizeof( bspModel_t ) );
	numBSPPlanes = CopyLump( (bspHeader_t*) header, LUMP_PLANES, bspPlanes, sizeof( bspPlane_t ) );
	numBSPLeafs = CopyLump( (bspHeader_t*) header, LUMP_LEAFS, bspLeafs, sizeof( bspLeaf_t ) );
	numBSPNodes = CopyLump( (bspHeader_t*) header, LUMP_NODES, bspNodes, sizeof( bspNode_t ) );
	numBSPLeafSurfaces = CopyLump( (bspHeader_t*) header, LUMP_LEAFSURFACES, bspLeafSurfaces, sizeof( bspLeafSurfaces[ 0 ] ) );
	numBSPLeafBrushes = CopyLump( (bspHeader_t*) header, LUMP_LEAFBRUSHES, bspLeafBrushes, sizeof( bspLeafBrushes[ 0 ] ) );
	numBSPBrushes = CopyLump( (bspHeader_t*) header, LUMP_BRUSHES, bspBrushes, sizeof( bspBrush_t ) );
	numBSPBrushSides = CopyLump( (bspHeader_t*) header, LUMP_BRUSHSIDES, bspBrushSides, sizeof( bspBrushSide_t ) );
	numBSPDrawVerts = GetLumpElements( (bspHeader_t*) header, LUMP_DRAWVERTS, sizeof( bspDrawVerts[ 0 ] ) );

	SetDrawVerts( numBSPDrawVerts );
	CopyLump( (bspHeader_t*) header, LUMP_DRAWVERTS, bspDrawVerts, sizeof( bspDrawVerts[ 0 ] ) );
	
	numBSPDrawSurfaces = GetLumpElements( (bspHeader_t*) header, LUMP_SURFACES, sizeof( bspDrawSurfaces[ 0 ] ) );
	SetDrawSurfaces( numBSPDrawSurfaces );
	CopyLump( (bspHeader_t*) header, LUMP_SURFACES, bspDrawSurfaces, sizeof( bspDrawSurfaces[ 0 ] ) );
	
	numBSPFogs = CopyLump( (bspHeader_t*) header, LUMP_FOGS, bspFogs, sizeof( bspFogs[ 0 ] ) );
	numBSPDrawIndexes = CopyLump( (bspHeader_t*) header, LUMP_DRAWINDEXES, bspDrawIndexes, sizeof( bspDrawIndexes[ 0 ] ) );
	numBSPVisBytes = CopyLump( (bspHeader_t*) header, LUMP_VISIBILITY, bspVisBytes, 1 );
	numBSPLightBytes = GetLumpElements( (bspHeader_t*) header, LUMP_LIGHTMAPS, 1 );

	bspLightBytes = Malloc( numBSPLightBytes );
	CopyLump( (bspHeader_t*) header, LUMP_LIGHTMAPS, bspLightBytes, 1 );
	
	bspEntDataSize = CopyLump( (bspHeader_t*) header, LUMP_ENTITIES, bspEntData, 1);
	CopyLightGridLumps( header );
	
	/* free the file buffer */
	Mem_Free( header );
}



/*
WriteRBSPFile()
writes a raven bsp file
*/

void WriteRBSPFile( const char *filename )
{		
	rbspHeader_t	outheader, *header;
	file_t		*file;
	char		marker[MAX_SYSPATH];
	int		size;
	
	
	/* set header */
	header = &outheader;
	memset( header, 0, sizeof( *header ) );
	
	//%	Swapfile();
	
	/* set up header */
	*((int*) (bspHeader_t*) header->ident) = *((int*) game->bspIdent);
	header->version = LittleLong( game->bspVersion );
	
	/* write initial header */
	file = FS_Open( filename, "wb" );
	FS_Write( file, (bspHeader_t *)header, sizeof( *header ));	/* overwritten later */
	
	// add marker lump
	com.sprintf( marker, "Written by Xash BspLib at %s)", timestamp( TIME_FILENAME ));
	AddLump( file, (bspHeader_t*) header, 0, marker, com.strlen( marker ) + 1 );
	
	/* add lumps */
	AddLump( file, (bspHeader_t*) header, LUMP_SHADERS, bspShaders, numBSPShaders * sizeof( bspShader_t ) );
	AddLump( file, (bspHeader_t*) header, LUMP_PLANES, bspPlanes, numBSPPlanes * sizeof( bspPlane_t ) );
	AddLump( file, (bspHeader_t*) header, LUMP_LEAFS, bspLeafs, numBSPLeafs * sizeof( bspLeaf_t ) );
	AddLump( file, (bspHeader_t*) header, LUMP_NODES, bspNodes, numBSPNodes * sizeof( bspNode_t ) );
	AddLump( file, (bspHeader_t*) header, LUMP_BRUSHES, bspBrushes, numBSPBrushes*sizeof( bspBrush_t ) );
	AddLump( file, (bspHeader_t*) header, LUMP_BRUSHSIDES, bspBrushSides, numBSPBrushSides * sizeof( bspBrushSides[ 0 ] ) );
	AddLump( file, (bspHeader_t*) header, LUMP_LEAFSURFACES, bspLeafSurfaces, numBSPLeafSurfaces * sizeof( bspLeafSurfaces[ 0 ] ) );
	AddLump( file, (bspHeader_t*) header, LUMP_LEAFBRUSHES, bspLeafBrushes, numBSPLeafBrushes * sizeof( bspLeafBrushes[ 0 ] ) );
	AddLump( file, (bspHeader_t*) header, LUMP_MODELS, bspModels, numBSPModels * sizeof( bspModel_t ) );
	AddLump( file, (bspHeader_t*) header, LUMP_DRAWVERTS, bspDrawVerts, numBSPDrawVerts * sizeof( bspDrawVerts[ 0 ] ) );
	AddLump( file, (bspHeader_t*) header, LUMP_SURFACES, bspDrawSurfaces, numBSPDrawSurfaces * sizeof( bspDrawSurfaces[ 0 ] ) );
	AddLump( file, (bspHeader_t*) header, LUMP_VISIBILITY, bspVisBytes, numBSPVisBytes );
	AddLump( file, (bspHeader_t*) header, LUMP_LIGHTMAPS, bspLightBytes, numBSPLightBytes );
	AddLightGridLumps( file, header );
	AddLump( file, (bspHeader_t*) header, LUMP_ENTITIES, bspEntData, bspEntDataSize );
	AddLump( file, (bspHeader_t*) header, LUMP_FOGS, bspFogs, numBSPFogs * sizeof( bspFog_t ) );
	AddLump( file, (bspHeader_t*) header, LUMP_DRAWINDEXES, bspDrawIndexes, numBSPDrawIndexes * sizeof( bspDrawIndexes[ 0 ] ) );
	
	/* emit bsp size */
	size = FS_Tell( file );
	Msg( "Writing %s ( %s )\n", filename, memprint( size ));
	
	/* write the completed header */
	FS_Seek( file, 0, SEEK_SET );
	FS_Write( file, header, sizeof( *header ));
	
	/* close the file */
	FS_Close( file );	
}
