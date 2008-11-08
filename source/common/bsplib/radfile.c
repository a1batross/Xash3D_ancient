//=======================================================================
//			Copyright XashXT Group 2007 �
//		        radfile.c - radiocity helper file
//=======================================================================

#include "bsplib.h"
#include "const.h"

/*
==============================================================================

Radiocity file generation

Save out name.rad for qrad to read
==============================================================================
*/
typedef struct surfaceExtra_s
{
	drawsurf_t	*mds;
	bsp_shader_t	*si;
	int		parentSurfaceNum;
	int		entityNum;
	int		castShadows, recvShadows;
	int		sampleSize;
	float		longestCurve;
	vec3_t		lightmapAxis;
} surfaceExtra_t;

#define GROW_SURFACE_EXTRAS	1024

int		numSurfaceExtras = 0;
int		maxSurfaceExtras = 0;
surfaceExtra_t	*surfaceExtras;
surfaceExtra_t	seDefault = { NULL, NULL, -1, 0, WORLDSPAWN_CAST_SHADOWS, WORLDSPAWN_RECV_SHADOWS, 0, 0, { 0, 0, 0 } };



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
		se = BSP_Malloc( maxSurfaceExtras * sizeof( surfaceExtra_t ) );
		if( surfaceExtras != NULL )
		{
			Mem_Copy( se, surfaceExtras, numSurfaceExtras * sizeof( surfaceExtra_t ));
			BSP_Free( surfaceExtras );
		}
		surfaceExtras = se;
	}
	
	se = &surfaceExtras[ numSurfaceExtras ];
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

stores extradata for the specific numbered drawsurface
=================
*/
void SetSurfaceExtra( drawsurf_t *ds, int num )
{
	surfaceExtra_t	*se;
	
	if( ds == NULL || num < 0 )
		return;
	
	se = AllocSurfaceExtra();
	
	se->mds = ds;
	se->si = ds->shader;
	se->parentSurfaceNum = ds->parent != NULL ? ds->parent->outputnum : -1;
	se->entityNum = ds->entitynum;
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

bsp_shader_t *GetSurfaceExtraShader( int num )
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
	surfaceExtra_t  *se = GetSurfaceExtra( num );
	VectorCopy( se->lightmapAxis, lightmapAxis );
}

/*
=================
WriteSurfaceExtraFile

writes out a surface info file (<map>.rad)
=================
*/
void WriteSurfaceExtraFile( void )
{
	file_t		*sf;
	surfaceExtra_t	*se;
	int		i;
	
	sf = FS_Open( va( "maps/%s.rad", gs_filename ), "w" );
	if( sf == NULL ) Sys_Error( "Error opening maps/%s.rad for writing\n", gs_filename );
	
	for( i = -1; i < numSurfaceExtras; i++ )
	{
		se = GetSurfaceExtra( i );
		
		if( i < 0 ) FS_Printf( sf, "default" );
		else FS_Printf( sf, "%d", i );
		
		if( se->mds == NULL ) FS_Printf( sf, "\n" );
		else
		{
			FS_Printf( sf, " // %s V: %d I: %d %s\n", surfaceTypes[se->mds->type], se->mds->numVerts, se->mds->numIndexes, (se->mds->planar ? "planar" : ""));
		}
		
		FS_Printf( sf, "{\n" );
		
		if( se->si != NULL ) FS_Printf( sf, "\tshader %s\n", se->si->name );
			
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
		
		// close braces
		FS_Printf( sf, "}\n\n" );
	}
	FS_Close( sf );
}



/*
=================
LoadSurfaceExtraFile

reads a surface info file (<map>.rad)
=================
*/
void LoadSurfaceExtraFile( void )
{
	surfaceExtra_t	*se;
	int		surfaceNum;
	script_t		*radfile = Com_OpenScript( va("maps/%s.rad", gs_filename ), NULL, 0 );	
	token_t		token;

	if( !radfile )
	{
		MsgDev( D_WARN, "Unable to find surface file maps/%s.rad, using defaults.\n", gs_filename );
		return;
	}
	
	while( 1 )
	{
		if( !Com_ReadToken( radfile, SC_ALLOW_NEWLINES, &token ))
			break;
		
		if( !com.strcmp( token.string, "default" )) se = &seDefault;
		else
		{
			surfaceNum = com.atoi( token.string );
			if( surfaceNum < 0 || surfaceNum > MAX_MAP_SURFACES )
				Sys_Error( "ReadSurfaceExtraFile(): line %d: bogus surface num %d", token.line, surfaceNum );
			while( surfaceNum >= numSurfaceExtras )
				se = AllocSurfaceExtra();
			se = &surfaceExtras[surfaceNum];
		}
		
		// handle { } section
		if( !Com_ReadToken( radfile, SC_ALLOW_NEWLINES, &token ) || com.strcmp( token.string, "{" ))
			Sys_Error( "ReadSurfaceExtraFile(): line %d: { not found", token.line );

		while( 1 )
		{
			if( !Com_ReadToken( radfile, SC_ALLOW_NEWLINES, &token ))
				break;
			if( !com.strcmp( token.string, "}" ))
				break;
			
			if( !com.strcmp( token.string, "shader" ))
			{
				Com_ReadToken( radfile, SC_PARSE_GENERIC, &token );
				se->si = FindShader( token.string );
			}
			else if(!com.strcmp( token.string, "parent" ))
				Com_ReadLong( radfile, false, &se->parentSurfaceNum );
			else if(!com.strcmp( token.string, "entity" ))
				Com_ReadLong( radfile, false, &se->entityNum );
			else if(!com.strcmp( token.string, "castShadows" ))
				Com_ReadLong( radfile, false, &se->castShadows );
			else if(!com.strcmp( token.string, "receiveShadows" ))
				Com_ReadLong( radfile, false, &se->recvShadows );
			else if(!com.strcmp( token.string, "sampleSize" ))
				Com_ReadLong( radfile, false, &se->sampleSize );
			else if(!com.strcmp( token.string, "longestCurve" ))
				Com_ReadFloat( radfile, false, &se->longestCurve );
			else if(!com.strcmp( token.string, "lightmapAxis" ))
				Com_Parse1DMatrix( radfile, 3, se->lightmapAxis );
			
			// ignore all other tokens on the line
			Com_SkipRestOfLine( radfile );
		}
	}
}