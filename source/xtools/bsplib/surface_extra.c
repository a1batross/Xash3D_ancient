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

#define SURFACE_EXTRA_C

#include "q3map2.h"

/*
-------------------------------------------------------------------------------

srf file module

-------------------------------------------------------------------------------
*/
#define GROW_SURFACE_EXTRAS		1024

typedef struct surfaceExtra_s
{
	mapDrawSurface_t		*mds;
	shaderInfo_t		*si;
	int			parentSurfaceNum;
	int			entityNum;
	int			castShadows, recvShadows;
	int			sampleSize;
	float			longestCurve;
	vec3_t			lightmapAxis;
} surfaceExtra_t;

int		numSurfaceExtras = 0;
int		maxSurfaceExtras = 0;
surfaceExtra_t	*surfaceExtras;
surfaceExtra_t	seDefault = { NULL, NULL, -1, 0, WORLDSPAWN_CAST_SHADOWS, WORLDSPAWN_RECV_SHADOWS, 0, 0, { 0, 0, 0 }};

/*
=================
AllocSurfaceExtra

allocates a new extra storage
=================
*/
static surfaceExtra_t *AllocSurfaceExtra( void )
{
	surfaceExtra_t	*se;
	
	if( numSurfaceExtras >= maxSurfaceExtras )
	{
		maxSurfaceExtras += GROW_SURFACE_EXTRAS;
		se = Malloc( maxSurfaceExtras * sizeof( surfaceExtra_t ));
		if( surfaceExtras != NULL )
		{
			Mem_Copy( se, surfaceExtras, numSurfaceExtras * sizeof( surfaceExtra_t ));
			Mem_Free( surfaceExtras );
		}
		surfaceExtras = se;
	}
	
	se = &surfaceExtras[numSurfaceExtras];
	numSurfaceExtras++;
	Mem_Copy( se, &seDefault, sizeof( surfaceExtra_t ));
	
	return se;
}

/*
=================
SetDefaultSampleSize

sets the default lightmap sample size
=================
*/
void SetDefaultSampleSize( int sampleSize )
{
	seDefault.sampleSize = sampleSize;
}

/*
=================
SetSurfaceExtra

stores extra (q3map2) data for the specific numbered drawsurface
=================
*/
void SetSurfaceExtra( mapDrawSurface_t *ds, int num )
{
	surfaceExtra_t	*se;

	if( ds == NULL || num < 0 )
		return;
	
	se = AllocSurfaceExtra();
	se->mds = ds;
	se->si = ds->shaderInfo;
	se->parentSurfaceNum = ds->parent != NULL ? ds->parent->outputNum : -1;
	se->entityNum = ds->entityNum;
	se->castShadows = ds->castShadows;
	se->recvShadows = ds->recvShadows;
	se->sampleSize = ds->sampleSize;
	se->longestCurve = ds->longestCurve;
	VectorCopy( ds->lightmapAxis, se->lightmapAxis );
}

/*
=================
GetSurfaceExtra

getter functions for extra surface data
=================
*/
static surfaceExtra_t *GetSurfaceExtra( int num )
{
	if( num < 0 || num >= numSurfaceExtras )
		return &seDefault;
	return &surfaceExtras[num];
}

shaderInfo_t *GetSurfaceExtraShaderInfo( int num )
{
	return GetSurfaceExtra( num )->si;
}


int GetSurfaceExtraParentSurfaceNum( int num )
{
	return GetSurfaceExtra( num )->parentSurfaceNum;
}

int GetSurfaceExtraEntityNum( int num )
{
	return GetSurfaceExtra( num )->entityNum;
}

int GetSurfaceExtraCastShadows( int num )
{
	return GetSurfaceExtra( num )->castShadows;
}

int GetSurfaceExtraRecvShadows( int num )
{
	return GetSurfaceExtra( num )->recvShadows;
}

int GetSurfaceExtraSampleSize( int num )
{
	return GetSurfaceExtra( num )->sampleSize;
}

float GetSurfaceExtraLongestCurve( int num )
{
	return GetSurfaceExtra( num )->longestCurve;
}

void GetSurfaceExtraLightmapAxis( int num, vec3_t lightmapAxis )
{
	VectorCopy( GetSurfaceExtra( num )->lightmapAxis, lightmapAxis );
}

/*
=================
WriteSurfaceExtraFile

writes out a surface info file (<map>.srf)
=================
*/
void WriteSurfaceExtraFile( const char *path )
{
	char		srfPath[MAX_SYSPATH];
	file_t		*sf;
	surfaceExtra_t	*se;
	int		i;

	if( path == NULL || path[0] == '\0' )
		return;
	
	MsgDev( D_NOTE, "--- WriteSurfaceExtraFile ---\n" );
	
	com.snprintf( srfPath, sizeof( srfPath ), "maps/%s", path );
	FS_StripExtension( srfPath );
	FS_DefaultExtension( srfPath, ".srf" );
	Msg( "Writing %s\n", srfPath );
	sf = FS_Open( srfPath, "w" );
	if( !sf ) Sys_Error( "Error opening %s for writing", srfPath );
	
	for( i = -1; i < numSurfaceExtras; i++ )
	{
		se = GetSurfaceExtra( i );
		
		if( i < 0 ) FS_Printf( sf, "default" );
		else FS_Printf( sf, "%d", i );
		
		if( se->mds == NULL ) FS_Printf( sf, "\n" );
		else
		{
			FS_Printf( sf, " // %s V: %d I: %d %s\n", surfaceTypes[se->mds->type], se->mds->numVerts, se->mds->numIndexes,
			(se->mds->planar ? "planar" : ""));
		}
		
		FS_Printf( sf, "{\n" );
		
		if( se->si != NULL ) FS_Printf( sf, "\tshader %s\n", se->si->shader );
			
		if( se->parentSurfaceNum != seDefault.parentSurfaceNum )
			FS_Printf( sf, "\tparent %d\n", se->parentSurfaceNum );
			
		if( se->entityNum != seDefault.entityNum )
			FS_Printf( sf, "\tentity %d\n", se->entityNum );
			
		if( se->castShadows != seDefault.castShadows || se == &seDefault )
			FS_Printf( sf, "\tcastShadows %d\n", se->castShadows );
			
		if( se->recvShadows != seDefault.recvShadows || se == &seDefault )
			FS_Printf( sf, "\treceiveShadows %d\n", se->recvShadows );
			
		if( se->sampleSize != seDefault.sampleSize || se == &seDefault )
			FS_Printf( sf, "\tsampleSize %d\n", se->sampleSize );
			
		if( se->longestCurve != seDefault.longestCurve || se == &seDefault )
			FS_Printf( sf, "\tlongestCurve %f\n", se->longestCurve );
			
		if( VectorCompare( se->lightmapAxis, seDefault.lightmapAxis ) == false )
			FS_Printf( sf, "\tlightmapAxis ( %f %f %f )\n", se->lightmapAxis[0], se->lightmapAxis[1], se->lightmapAxis[2] );
		FS_Printf( sf, "}\n\n" );
	}
	FS_Close( sf );
}

/*
=================
LoadSurfaceExtraFile

reads a surface info file (<map>.srf)
=================
*/
void LoadSurfaceExtraFile( const char *path )
{
	char		srfPath[MAX_SYSPATH];
	surfaceExtra_t	*se;
	int		surfaceNum;
	script_t		*script;
	token_t		token;
	
	if( path == NULL || path[0] == '\0' )
		return;
	
	com.strcpy( srfPath, path );
	FS_StripExtension( srfPath );
	FS_DefaultExtension( srfPath, ".srf" );
	Msg( "Loading %s\n", srfPath );

	script = (void *)Com_OpenScript( srfPath, NULL, 0 );
	if( !script ) Sys_Break( "unable to load %s\n", srfPath ); // q3map is always crashed if this missed
	
	while( 1 )
	{
		if( !Com_ReadToken( script, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
			break;
		
		if( !com.stricmp( token.string, "default" )) se = &seDefault;
		else
		{
			surfaceNum = com.atoi( token.string );
			if( surfaceNum < 0 || surfaceNum > MAX_MAP_DRAW_SURFS )
				Sys_Error( "ReadSurfaceExtraFile(): %s, line %d: bogus surface num %d", srfPath, token.line, surfaceNum );
			while( surfaceNum >= numSurfaceExtras )
				se = AllocSurfaceExtra();
			se = &surfaceExtras[surfaceNum];
		}
		
		// handle { } section
		if( !Com_ReadToken( script, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ) || com.strcmp( token.string, "{" ))
			Sys_Error( "ReadSurfaceExtraFile(): %s, line %d: { not found\n", srfPath, token.line );
		while( 1 )
		{
			if( !Com_ReadToken( script, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
				break;
			if( !com.strcmp( token.string, "}" ))
				break;
			
			if( !com.stricmp( token.string, "shader" ))
			{
				Com_ReadToken( script, SC_ALLOW_PATHNAMES|SC_PARSE_GENERIC, &token );
				se->si = ShaderInfoForShader( token.string );
			}
			else if( !com.stricmp( token.string, "parent" ))
			{
				Com_ReadLong( script, false, &se->parentSurfaceNum );
			}
			else if( !com.stricmp( token.string, "entity" ))
			{
				Com_ReadLong( script, false, &se->entityNum );
			}
			else if( !com.stricmp( token.string, "castShadows" ))
			{
				Com_ReadLong( script, false, &se->castShadows );
			}
			else if( !com.stricmp( token.string, "receiveShadows" ))
			{
				Com_ReadLong( script, false, &se->recvShadows );
			}
			else if( !com.stricmp( token.string, "sampleSize" ))
			{
				Com_ReadLong( script, false, &se->sampleSize );
			}
			else if( !com.stricmp( token.string, "longestCurve" ))
			{
				Com_ReadFloat( script, false, &se->longestCurve );
			}
			else if( !com.stricmp( token.string, "lightmapAxis" ))
				Com_Parse1DMatrix( script, 3, se->lightmapAxis );
			
			// ignore all other tokens on the line
			Com_SkipRestOfLine( script );
		}
	}
	Com_CloseScript( script );
}