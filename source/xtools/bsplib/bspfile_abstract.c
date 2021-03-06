/* -------------------------------------------------------------------------------

Copyright (C) 1999-2007 id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is Mem_Free software; you can redistribute it and/or modify
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
#define BSPFILE_ABSTRACT_C



/* dependencies */
#include "q3map2.h"
#include "byteorder.h"
#include "stdio.h"		// rename support, FIXME, implement rename into launch.dll


/* -------------------------------------------------------------------------------

this file was copied out of the common directory in order to not break
compatibility with the q3map 1.x tree. it was moved out in order to support
the raven bsp format (RBSP) used in soldier of fortune 2 and jedi knight 2.

since each game has its own set of particular features, the data structures
below no longer directly correspond to the binary format of a particular game.

the translation will be done at bsp load/save time to keep any sort of
special-case code messiness out of the rest of the program.

------------------------------------------------------------------------------- */



/* FIXME: remove the functions below that handle memory management of bsp file chunks */

int numBSPDrawVertsBuffer = 0;
void IncDrawVerts( void )
{
	numBSPDrawVerts++;

	if( bspDrawVerts == 0 )
	{
		numBSPDrawVertsBuffer = MAX_MAP_DRAW_VERTS / 37;
		bspDrawVerts = Malloc( sizeof( bspDrawVert_t ) * numBSPDrawVertsBuffer );

	}
	else if( numBSPDrawVerts > numBSPDrawVertsBuffer )
	{
		numBSPDrawVertsBuffer *= 3; // multiply by 1.5
		numBSPDrawVertsBuffer /= 2;

		if(numBSPDrawVertsBuffer > MAX_MAP_DRAW_VERTS)
			numBSPDrawVertsBuffer = MAX_MAP_DRAW_VERTS;

		bspDrawVerts = Realloc( bspDrawVerts, sizeof( bspDrawVert_t ) * numBSPDrawVertsBuffer );
	}
}

void SetDrawVerts(int n)
{
	if(bspDrawVerts != 0)
		Mem_Free(bspDrawVerts);

	numBSPDrawVerts = n;
	numBSPDrawVertsBuffer = numBSPDrawVerts;

	bspDrawVerts = Malloc( sizeof( bspDrawVert_t ) * numBSPDrawVertsBuffer );
}

int numBSPDrawSurfacesBuffer = 0;
void SetDrawSurfacesBuffer()
{
	if(bspDrawSurfaces != 0)
		Mem_Free(bspDrawSurfaces);

	numBSPDrawSurfacesBuffer = MAX_MAP_DRAW_SURFS;

	bspDrawSurfaces = Malloc( sizeof( bspDrawSurface_t ) * numBSPDrawSurfacesBuffer );
}

void SetDrawSurfaces(int n)
{
	if( bspDrawSurfaces != 0 )
		Mem_Free( bspDrawSurfaces );

	numBSPDrawSurfaces = n;
	numBSPDrawSurfacesBuffer = numBSPDrawSurfaces;

	bspDrawSurfaces = Malloc( sizeof( bspDrawSurface_t ) * numBSPDrawSurfacesBuffer );
}

void BSPFilesCleanup()
{
	if(bspDrawVerts != 0)
		Mem_Free(bspDrawVerts);
	if(bspDrawSurfaces != 0)
		Mem_Free(bspDrawSurfaces);
	if(bspLightBytes != 0)
		Mem_Free(bspLightBytes);
	if(bspGridPoints != 0)
		Mem_Free(bspGridPoints);
}

/*
SwapBSPFile()
byte swaps all data in the abstract bsp
*/

void SwapBSPFile( void )
{
	int		i, j;
	
	
	/* models */
	SwapBlock( (int*) bspModels, numBSPModels * sizeof( bspModels[ 0 ] ) );

	/* shaders (don't swap the name) */
	for( i = 0; i < numBSPShaders ; i++ )
	{
		bspShaders[ i ].contentFlags = LittleLong( bspShaders[ i ].contentFlags );
		bspShaders[ i ].surfaceFlags = LittleLong( bspShaders[ i ].surfaceFlags );
	}

	/* planes */
	SwapBlock( (int*) bspPlanes, numBSPPlanes * sizeof( bspPlanes[ 0 ] ) );
	
	/* nodes */
	SwapBlock( (int*) bspNodes, numBSPNodes * sizeof( bspNodes[ 0 ] ) );

	/* leafs */
	SwapBlock( (int*) bspLeafs, numBSPLeafs * sizeof( bspLeafs[ 0 ] ) );

	/* leaffaces */
	SwapBlock( (int*) bspLeafSurfaces, numBSPLeafSurfaces * sizeof( bspLeafSurfaces[ 0 ] ) );

	/* leafbrushes */
	SwapBlock( (int*) bspLeafBrushes, numBSPLeafBrushes * sizeof( bspLeafBrushes[ 0 ] ) );

	// brushes
	SwapBlock( (int*) bspBrushes, numBSPBrushes * sizeof( bspBrushes[ 0 ] ) );

	// brushsides
	SwapBlock( (int*) bspBrushSides, numBSPBrushSides * sizeof( bspBrushSides[ 0 ] ) );

	// vis
	((int*) &bspVisBytes)[ 0 ] = LittleLong( ((int*) &bspVisBytes)[ 0 ] );
	((int*) &bspVisBytes)[ 1 ] = LittleLong( ((int*) &bspVisBytes)[ 1 ] );

	/* drawverts (don't swap colors) */
	for( i = 0; i < numBSPDrawVerts; i++ )
	{
		bspDrawVerts[ i ].xyz[ 0 ] = LittleFloat( bspDrawVerts[ i ].xyz[ 0 ] );
		bspDrawVerts[ i ].xyz[ 1 ] = LittleFloat( bspDrawVerts[ i ].xyz[ 1 ] );
		bspDrawVerts[ i ].xyz[ 2 ] = LittleFloat( bspDrawVerts[ i ].xyz[ 2 ] );
		bspDrawVerts[ i ].normal[ 0 ] = LittleFloat( bspDrawVerts[ i ].normal[ 0 ] );
		bspDrawVerts[ i ].normal[ 1 ] = LittleFloat( bspDrawVerts[ i ].normal[ 1 ] );
		bspDrawVerts[ i ].normal[ 2 ] = LittleFloat( bspDrawVerts[ i ].normal[ 2 ] );
		bspDrawVerts[ i ].st[ 0 ] = LittleFloat( bspDrawVerts[ i ].st[ 0 ] );
		bspDrawVerts[ i ].st[ 1 ] = LittleFloat( bspDrawVerts[ i ].st[ 1 ] );
		for( j = 0; j < MAX_LIGHTMAPS; j++ )
		{
			bspDrawVerts[ i ].lightmap[ j ][ 0 ] = LittleFloat( bspDrawVerts[ i ].lightmap[ j ][ 0 ] );
			bspDrawVerts[ i ].lightmap[ j ][ 1 ] = LittleFloat( bspDrawVerts[ i ].lightmap[ j ][ 1 ] );
		}
	}
	
	/* drawindexes */
	SwapBlock( (int*) bspDrawIndexes, numBSPDrawIndexes * sizeof( bspDrawIndexes[0] ) );

	/* drawsurfs */
	/* note: rbsp files (and hence q3map2 abstract bsp) have byte lightstyles index arrays, this follows sof2map convention */
	SwapBlock( (int*) bspDrawSurfaces, numBSPDrawSurfaces * sizeof( bspDrawSurfaces[ 0 ] ) );

	/* fogs */
	for( i = 0; i < numBSPFogs; i++ )
	{
		bspFogs[ i ].brushNum = LittleLong( bspFogs[ i ].brushNum );
		bspFogs[ i ].visibleSide = LittleLong( bspFogs[ i ].visibleSide );
	}

	/* advertisements */
	for( i = 0; i < numBSPAds; i++ )
	{
		bspAds[ i ].cellId = LittleLong( bspAds[ i ].cellId );
		bspAds[ i ].normal[ 0 ] = LittleFloat( bspAds[ i ].normal[ 0 ] );
		bspAds[ i ].normal[ 1 ] = LittleFloat( bspAds[ i ].normal[ 1 ] );
		bspAds[ i ].normal[ 2 ] = LittleFloat( bspAds[ i ].normal[ 2 ] );

		for( j = 0; j < 4; j++ ) 
		{
			bspAds[ i ].rect[j][ 0 ] = LittleFloat( bspAds[ i ].rect[j][ 0 ] );
			bspAds[ i ].rect[j][ 1 ] = LittleFloat( bspAds[ i ].rect[j][ 1 ] );
			bspAds[ i ].rect[j][ 2 ] = LittleFloat( bspAds[ i ].rect[j][ 2 ] );
		}
	}
}



/*
GetLumpElements()
gets the number of elements in a bsp lump
*/

int GetLumpElements( bspHeader_t *header, int lump, int size )
{
	/* check for odd size */
	if( header->lumps[ lump ].length % size )
	{
		if( force )
		{
			MsgDev( D_WARN, "GetLumpElements: odd lump size (%d) in lump %d\n", header->lumps[ lump ].length, lump );
			return 0;
		}
		else Sys_Break( "GetLumpElements: odd lump size (%d) in lump %d", header->lumps[ lump ].length, lump );
	}
	
	/* return element count */
	return header->lumps[ lump ].length / size;
}



/*
GetLump()
returns a pointer to the specified lump
*/

void *GetLump( bspHeader_t *header, int lump )
{
	return (void*)( (byte*) header + header->lumps[ lump ].offset);
}



/*
CopyLump()
copies a bsp file lump into a destination buffer
*/

int CopyLump( bspHeader_t *header, int lump, void *dest, int size )
{
	int		length, offset;
	
	
	/* get lump length and offset */
	length = header->lumps[ lump ].length;
	offset = header->lumps[ lump ].offset;
	
	/* handle erroneous cases */
	if( length == 0 )
		return 0;
	if( length % size )
	{
		if( force )
		{
			MsgDev( D_WARN, "CopyLump: odd lump size (%d) in lump %d\n", length, lump );
			return 0;
		}
		else Sys_Break( "CopyLump: odd lump size (%d) in lump %d", length, lump );
	}
	
	/* copy block of memory and return */
	memcpy( dest, (byte*) header + offset, length );
	return length / size;
}



/*
AddLump()
adds a lump to an outgoing bsp file
*/

void AddLump( file_t *file, bspHeader_t *header, int lumpNum, const void *data, int length )
{
	bspLump_t	*lump;
	
	
	/* add lump to bsp file header */
	lump = &header->lumps[ lumpNum ];
	lump->offset = LittleLong( FS_Tell( file ) );
	lump->length = LittleLong( length );
	
	/* write lump to file */
	FS_Write( file, data, (length + 3) & ~3 );
}



/*
LoadBSPFile()
loads a bsp file into memory
*/

void LoadBSPFile( const char *filename )
{
	/* dummy check */
	if( game == NULL || game->load == NULL )
		Sys_Break( "LoadBSPFile: unsupported BSP file format" );
	
	/* load it, then byte swap the in-memory version */
	game->load( filename );
	SwapBSPFile();
}



/*
WriteBSPFile()
writes a bsp file
*/

void WriteBSPFile( const char *filename )
{
	if( game == NULL || game->write == NULL )
		Sys_Break( "WriteBSPFile: unsupported BSP file format" );

	// byteswap, write the bsp, then swap back so it can be manipulated further
	SwapBSPFile();
	game->write( filename );
	SwapBSPFile();
}



/*
PrintBSPFileSizes()
dumps info about current file
*/

void PrintBSPFileSizes( void )
{
	/* parse entities first */
	if( numEntities <= 0 )
		ParseEntities();
	
	/* note that this is abstracted */
	Msg( "Abstracted BSP file components (*actual sizes may differ)\n" );
	
	/* print various and sundry bits */
	Msg( "%9d models        %9d\n", numBSPModels, (int) (numBSPModels * sizeof( bspModel_t )) );
	Msg( "%9d shaders       %9d\n", numBSPShaders, (int) (numBSPShaders * sizeof( bspShader_t )) );
	Msg( "%9d brushes       %9d\n", numBSPBrushes, (int) (numBSPBrushes * sizeof( bspBrush_t )) );
	Msg( "%9d brushsides    %9d *\n", numBSPBrushSides, (int) (numBSPBrushSides * sizeof( bspBrushSide_t )) );
	Msg( "%9d fogs          %9d\n", numBSPFogs, (int) (numBSPFogs * sizeof( bspFog_t ) ) );
	Msg( "%9d planes        %9d\n", numBSPPlanes, (int) (numBSPPlanes * sizeof( bspPlane_t )) );
	Msg( "%9d entdata       %9d\n", numEntities, bspEntDataSize );
	Msg( "\n");
	
	Msg( "%9d nodes         %9d\n", numBSPNodes, (int) (numBSPNodes * sizeof( bspNode_t)) );
	Msg( "%9d leafs         %9d\n", numBSPLeafs, (int) (numBSPLeafs * sizeof( bspLeaf_t )) );
	Msg( "%9d leafsurfaces  %9d\n", numBSPLeafSurfaces, (int) (numBSPLeafSurfaces * sizeof( *bspLeafSurfaces )) );
	Msg( "%9d leafbrushes   %9d\n", numBSPLeafBrushes, (int) (numBSPLeafBrushes * sizeof( *bspLeafBrushes )) );
	Msg( "\n");
	
	Msg( "%9d drawsurfaces  %9d *\n", numBSPDrawSurfaces, (int) (numBSPDrawSurfaces * sizeof( *bspDrawSurfaces )) );
	Msg( "%9d drawverts     %9d *\n", numBSPDrawVerts, (int) (numBSPDrawVerts * sizeof( *bspDrawVerts )) );
	Msg( "%9d drawindexes   %9d\n", numBSPDrawIndexes, (int) (numBSPDrawIndexes * sizeof( *bspDrawIndexes )) );
	Msg( "\n");
	
	Msg( "%9d lightmaps     %9d\n", numBSPLightBytes / (game->lightmapSize * game->lightmapSize * 3), numBSPLightBytes );
	Msg( "%9d lightgrid     %9d *\n", numBSPGridPoints, (int) (numBSPGridPoints * sizeof( *bspGridPoints )) );
	Msg( "          visibility    %9d\n", numBSPVisBytes );
}



/* -------------------------------------------------------------------------------

entity data handling

------------------------------------------------------------------------------- */


/*
StripTrailing()
strips low byte chars off the end of a string
*/

void StripTrailing( char *e )
{
	char	*s;

	s = e + com.strlen( e ) - 1;
	while( s >= e && *s <= 32 )
	{
		*s = 0;
		s--;
	}
}

/*
=================
ParseEpair

parses a single quoted "key" "value" pair into an epair struct
=================
*/
epair_t *ParseEpair( script_t *script, token_t *token )
{
	epair_t	*e;

	e = Malloc( sizeof( epair_t ));
	
	if( com.strlen( token->string ) >= MAX_KEY - 1 )
		Sys_Break( "ParseEpair: key %s too long\n", token->string );
	e->key = copystring( token->string );
	Com_ReadToken( script, SC_PARSE_GENERIC, token );
	if( com.strlen( token->string ) >= MAX_VALUE - 1 )
		Sys_Break( "ParseEpair: value %s too long\n", token->string );
	e->value = copystring( token->string );

	// strip trailing spaces if needs
	StripTrailing( e->key );
	StripTrailing( e->value );

	return e;
}

/*
================
ParseEntity

parses an entity's epairs
================
*/
bool ParseEntity( void )
{
	epair_t		*e;
	token_t		token;

	if( !Com_ReadToken( mapfile, SC_ALLOW_NEWLINES, &token ))
		return false;

	if( com.stricmp( token.string, "{" )) Sys_Break( "ParseEntity: '{' not found\n" );
	if( numEntities == MAX_MAP_ENTITIES ) Sys_Break( "MAX_MAP_ENTITIES limit exceeded\n" );

	mapEnt = &entities[numEntities];
	numEntities++;

	while( 1 )
	{
		if( !Com_ReadToken( mapfile, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
			Sys_Break( "ParseEntity: EOF without closing brace\n" );
		if( !com.stricmp( token.string, "}" )) break;
		e = ParseEpair( mapfile, &token );
		e->next = mapEnt->epairs;
		mapEnt->epairs = e;
	}
	return true;
}

/*
================
ParseEntities

parses the bsp entity data string into entities
================
*/
void ParseEntities( void )
{
	numEntities = 0;
	mapfile = Com_OpenScript( "entities", bspEntData, bspEntDataSize );
	if( mapfile )
	{
		while( ParseEntity( ));
		Com_CloseScript( mapfile );
	}

	// set number of bsp entities in case a map is loaded on top
	numBSPEntities = numEntities;
}

/*
================
UnparseEntities

generates the bsp entity data string from all the entities
this allows the utilities to add or remove key/value
pairs to the data created by the map editor
================
*/
void UnparseEntities( void )
{
	epair_t		*ep;
	char		*buf, *end;
	char		line[2048];
	char		key[MAX_KEY], value[MAX_VALUE];
	const char	*value2;
	int		i;
	
	buf = bspEntData;
	end = buf;
	*end = 0;
	
	// run through entity list
	for( i = 0; i < numBSPEntities && i < numEntities; i++ )
	{
		ep = entities[i].epairs;
		if( !ep ) continue;	// ent got removed
		
		// certain entities get stripped from bsp file
		value2 = ValueForKey( &entities[i], "classname" );
		if( !com.stricmp( value2, "misc_model" ) || !com.stricmp( value2, "_decal" ) || !com.stricmp( value2, "_skybox" ))
			continue;
		
		com.strcat( end, "{\n" );
		end += 2;
		
		for( ep = entities[ i ].epairs; ep != NULL; ep = ep->next )
		{
			com.strncpy( key, ep->key, MAX_KEY );
			StripTrailing( key );
			com.strncpy( value, ep->value, MAX_VALUE );
			StripTrailing( value );
			
			/* add to buffer */
			com.snprintf( line, sizeof( line ), "\"%s\" \"%s\"\n", key, value );
			com.strcat( end, line );
			end += com.strlen( line );
		}
		
		com.strcat( end,"}\n" );
		end += 2;
		
		if( end > buf + MAX_MAP_ENTSTRING )
			Sys_Break( "Entity text too long\n" );
	}
	bspEntDataSize = end - buf + 1;
}



/*
================
PrintEntity

prints an entity's epairs to the console
================
*/
void PrintEntity( const entity_t *ent )
{
	epair_t	*ep;
	

	Msg( "------- entity %p -------\n", ent );
	for( ep = ent->epairs; ep != NULL; ep = ep->next )
		Msg( "%s = %s\n", ep->key, ep->value );

}

/*
================
SetKeyValue

sets an epair in an entity
================
*/
void SetKeyValue( entity_t *ent, const char *key, const char *value )
{
	epair_t	*ep;
	
	for( ep = ent->epairs; ep != NULL; ep = ep->next )
	{
		if( !com.strcmp( ep->key, key ))
		{
			Mem_Free( ep->value );
			ep->value = copystring( value );
			return;
		}
	}
	
	ep = Malloc( sizeof( *ep ) );
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = copystring( key );
	ep->value = copystring( value );
}



/*
================
ValueForKey

gets the value for an entity key
================
*/
const char *ValueForKey( const entity_t *ent, const char *key )
{
	epair_t	*ep;

	if( !ent ) return "";
	
	for( ep = ent->epairs; ep != NULL; ep = ep->next )
	{
		if( !com.strcmp( ep->key, key ))
			return ep->value;
	}
	return "";
}



/*
================
IntForKey

gets the integer point value for an entity key
================
*/
int IntForKey( const entity_t *ent, const char *key )
{
	return com.atoi( ValueForKey( ent, key ));
}

/*
================
FloatForKey

gets the floating point value for an entity key
================
*/
float FloatForKey( const entity_t *ent, const char *key )
{
	return com.atof( ValueForKey( ent, key ));
}



/*
================
GetVectorForKey

gets a 3-element vector value for an entity key
================
*/
void GetVectorForKey( const entity_t *ent, const char *key, vec3_t vec )
{
	const char	*k;
	double		v1, v2, v3;

	k = ValueForKey( ent, key );
	
	// scanf into doubles, then assign, so it is vec_t size independent
	v1 = v2 = v3 = 0.0;
	sscanf( k, "%lf %lf %lf", &v1, &v2, &v3 );
	VectorSet( vec, v1, v2, v3 );
}



/*
================
FindTargetEntity

finds an entity target
================
*/
entity_t *FindTargetEntity( const char *target )
{
	int		i;
	const char	*n;
	
	for( i = 0; i < numEntities; i++ )
	{
		n = ValueForKey( &entities[i], "targetname" );
		if( !com.strcmp( n, target )) return &entities[i];
	}
	return NULL;
}

/*
================
GetEntityShadowFlags

gets an entity's shadow flags
note: does not set them to defaults if the keys are not found!
================
*/
void GetEntityShadowFlags( const entity_t *ent, const entity_t *ent2, int *castShadows, int *recvShadows )
{
	const char	*value;
	
	if( castShadows != NULL )
	{
		value = ValueForKey( ent, "_castShadows" );
		if( value[0] == '\0' ) value = ValueForKey( ent, "_cs" );
		if( value[0] == '\0' ) value = ValueForKey( ent2, "_castShadows" );
		if( value[0] == '\0' ) value = ValueForKey( ent2, "_cs" );
		if( value[0] != '\0' ) *castShadows = com.atoi( value );
	}
	
	if( recvShadows != NULL )
	{
		value = ValueForKey( ent, "_receiveShadows" );
		if( value[0] == '\0' ) value = ValueForKey( ent, "_rs" );
		if( value[0] == '\0' ) value = ValueForKey( ent2, "_receiveShadows" );
		if( value[0] == '\0' ) value = ValueForKey( ent2, "_rs" );
		if( value[0] != '\0' ) *recvShadows = com.atoi( value );
	}
}

/*
================
Com_CheckToken

================
*/
void Com_CheckToken( script_t *script, const char *match )
{
	token_t	token;
	
	Com_ReadToken( script, SC_ALLOW_NEWLINES, &token );

	if( com.stricmp( token.string, match ))
		Sys_Break( "Com_CheckToken: \"%s\" not found\n", match );
}

void Com_Parse1DMatrix( script_t *script, int x, float *m )
{
	int	i;

	Com_CheckToken( script, "(" );
	for( i = 0; i < x; i++ )
		Com_ReadFloat( script, false, &m[i] );
	Com_CheckToken( script, ")" );
}

void Com_Parse2DMatrix( script_t *script, int y, int x, float *m )
{
	int	i;

	Com_CheckToken( script, "(" );
	for( i = 0; i < y; i++ )
		Com_Parse1DMatrix( script, x, m+i*x );
	Com_CheckToken( script, ")" );
}