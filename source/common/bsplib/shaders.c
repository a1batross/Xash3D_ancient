#include "bsplib.h"
#include "const.h"

#define MAX_SHADER_FILES		64
#define MAX_SURFACE_INFO		4096
int numShaderInfo = 0;

bsp_shader_t shaderInfo[MAX_SURFACE_INFO];

typedef struct
{
	const char	*name;
	int		surfaceFlags;
	int		contents;
	bool		clearSolid;
} infoParm_t;

infoParm_t infoParms[] =
{
	// server relevant contents
	{"water",		0,		CONTENTS_WATER,		1},
	{"slime",		0,		CONTENTS_SLIME,		1}, // mildly damaging
	{"lava",		0,		CONTENTS_LAVA,		1}, // very damaging
	{"playerclip",	0,		CONTENTS_PLAYERCLIP,	1},
	{"monsterclip",	0,		CONTENTS_MONSTERCLIP,	1},
	{"clip",		0,		CONTENTS_CLIP,		1},
	{"nonsolid",	0,		0,			1}, // just clears the solid flag

	// utility relevant attributes
	{"origin",	0,		CONTENTS_ORIGIN,		1}, // center of rotating brushes
	{"trans",		0,		CONTENTS_TRANSLUCENT,	0}, // don't eat contained surfaces
	{"detail",	0,		CONTENTS_DETAIL,		0}, // carves surfaces entering
	{"world",		0,		CONTENTS_STRUCTURAL,	0}, // force into world even if trans
	{"areaportal",	0,		CONTENTS_AREAPORTAL,	1},
	{"fog",		0,		CONTENTS_FOG,		1}, // carves surfaces entering
	{"sky",		SURF_SKY,		0,			0}, // emit light from environment map
	{"skyroom",	SURF_SKYROOM,	0,			0}, // env_sky surface
	{"lightfilter",	SURF_LIGHTFILTER,	0,			0}, // filter light going through it
	{"alphashadow",	SURF_ALPHASHADOW,	0,			0}, // test light on a per-pixel basis
	{"hint",		SURF_HINT,	0,			0}, // use as a primary splitter
	{"skip",		SURF_NODRAW,	0,			0}, // use as a secondary splitter
	{"null",		SURF_NODRAW,	0,			0}, // don't generate a drawsurface
	{"nodraw",	SURF_NODRAW,	0,			0}, // don't generate a drawsurface

	// server attributes
	{"slick",		0,		SURF_SLICK,		0},
	{"noimpact",	0,		SURF_NOIMPACT,		0}, // no impact explosions or marks
	{"nomarks",	0,		SURF_NOMARKS,		0}, // no impact marks, but explodes
	{"ladder",	0,		CONTENTS_LADDER,		0},
	{"nodamage",	SURF_NODAMAGE,	0,			0},
	{"nosteps",	SURF_NOSTEPS,	0,			0},

	// drawsurf attributes
	{"nolightmap",	SURF_NOLIGHTMAP,	0,			0}, // don't generate a lightmap
	{"nodlight",	SURF_NODLIGHT,	0,			0}, // don't ever add dynamic lights
	{"alpha",		SURF_ALPHA,	CONTENTS_TRANSLUCENT,	0}, // alpha surface preset
	{"additive",	SURF_ADDITIVE,	CONTENTS_TRANSLUCENT,	0}, // additive surface preset
	{"blend",		SURF_BLEND,	CONTENTS_TRANSLUCENT,	0}, // blend surface preset
	{"mirror",	SURF_PORTAL,	0,			0}, // mirror surface
	{"portal",	SURF_PORTAL,	0,			0}, // portal surface
};

/*
===============
AllocShaderInfo
===============
*/
static bsp_shader_t *AllocShaderInfo( void )
{
	bsp_shader_t	*si;

	if ( numShaderInfo == MAX_SURFACE_INFO ) Sys_Error( "MAX_SURFACE_INFO" );

	si = &shaderInfo[numShaderInfo];
	numShaderInfo++;

	// set defaults
	memset( si, 0, sizeof( *si ));
	si->contents = CONTENTS_SOLID;

	return si;
}
/*
===============
LoadShaderImage
===============
*/
static void LoadShaderImage( bsp_shader_t *si )
{
	if( !si ) return;
	if( !com.stricmp( si->name, "default" )) 
		return;

	if( !si->pixels )
	{
		int	i, count;
		vec3_t	color;
		rgbdata_t	*pic;
	
		if( si->lightimage[0] )
			pic = FS_LoadImage( si->lightimage, NULL, 0 );
		else if( si->editorimage[0] ) 
			pic = FS_LoadImage( si->editorimage, NULL, 0 );
		else pic = FS_LoadImage( si->name, NULL, 0 ); // last chance
		if( !pic )
		{
			si->width = 64;
			si->height = 64;
			return; // not found 
		}
		
		count = pic->width * pic->height;
		si->width = pic->width;
		si->height = pic->height;
		VectorClear( color );

		if( pic->type == PF_RGBA_32 )
		{
			for( i = 0; i < count; i++ )
			{
				color[0] += pic->buffer[i*4+0];
				color[1] += pic->buffer[i*4+1];
				color[2] += pic->buffer[i*4+2];
			}
	
			ColorNormalize( color, si->color );
			VectorScale( color, 1.0 / count, si->averageColor );
			si->pixels = BSP_Malloc( pic->size );
			Mem_Copy( si->pixels, pic->buffer, pic->size );
			FS_FreeImage( pic ); // release pic
		}
		else  MsgDev( D_WARN, "%s have type %s\n", si->name, PFDesc( pic->type )->name );
	}
}

/*
===============
ParseShaderFile
===============
*/
static void ParseShaderFile( char *filename )
{
	int		i, numInfoParms = sizeof(infoParms) / sizeof(infoParms[0]);
	string		name;
	bsp_shader_t	*si;

	bool load = Com_LoadScript( filename, NULL, 0 );

	if( load )
	{
		FS_FileBase( filename, name );
		MsgDev(D_INFO, "Adding shader: %s.txt\n", name );
          }
          
	while( load )
	{
		if ( !Com_GetToken( true )) break;

		si = AllocShaderInfo();
		com.strcpy( si->name, com_token );
		Com_GetToken( true );
		
		if(!Com_MatchToken( "{" ))
		{
			Msg("ParseShaderFile: shader %s without opening brace!\n", si->name );
			continue;
                    }
                    
		while ( 1 )
		{
			if ( !Com_GetToken( true ) )break;
			if ( !strcmp( com_token, "}" ) ) break;

			// skip internal braced sections
			if ( !strcmp( com_token, "{" ) )
			{
				si->hasStages = true;
				while ( 1 )
				{
					if ( !Com_GetToken( true )) break;
					if ( !strcmp( com_token, "}" )) break;
				}
				continue;
			}

			if( Com_MatchToken( "surfaceparm" ))
			{
				Com_GetToken( false );
				for ( i = 0; i < numInfoParms; i++ )
				{
					if( Com_MatchToken( infoParms[i].name ))
					{
						si->surfaceFlags |= infoParms[i].surfaceFlags;
						si->contents |= infoParms[i].contents;
						if ( infoParms[i].clearSolid )
							si->contents &= ~CONTENTS_SOLID;
						break;
					}
				}
				continue;
			}

			// qer_editorimage <image>
			if( Com_MatchToken( "qer_editorimage" ))
			{
				Com_GetToken( false );
				com.strncpy( si->editorimage, com_token, MAX_STRING );
				continue;
			}

			// q3map_lightimage <image>
			if( Com_MatchToken( "q3map_lightimage" ))
			{
				Com_GetToken( false );
				com.strncpy( si->lightimage, com_token, MAX_STRING );
				continue;
			}

			// q3map_surfacelight <value>
			if( Com_MatchToken( "q3map_surfacelight" ))
			{
				Com_GetToken( false );
				si->value = com.atoi( com_token );
				continue;
			}

			// q3map_lightsubdivide <value>
			if( Com_MatchToken( "q3map_lightsubdivide" ))
			{
				Com_GetToken( false );
				si->lightSubdivide = com.atoi( com_token );
				continue;
			}

			// q3map_lightmapsamplesize <value>
			if( Com_MatchToken( "q3map_lightmapsamplesize" ))
			{
				Com_GetToken( false );
				si->lightmapSampleSize = com.atoi( com_token );
				continue;
			}

			// q3map_tracelight
			if( Com_MatchToken( "q3map_tracelight" ))
			{
				si->forceTraceLight = true;
				continue;
			}

			// q3map_vlight
			if ( Com_MatchToken( "q3map_vlight" ))
			{
				si->forceVLight = true;
				continue;
			}

			// q3map_forcesunlight
			if( Com_MatchToken( "q3map_forcesunlight" ))
			{
				si->forceSunLight = true;
				continue;
			}

			// q3map_vertexscale
			if( Com_MatchToken( "q3map_vertexscale" ))
			{
				Com_GetToken( false );
				si->vertexScale = com.atof( com_token );
				continue;
			}

			// q3map_notjunc
			if( Com_MatchToken( "q3map_notjunc" ))
			{
				si->notjunc = true;
				continue;
			}

			// q3map_globaltexture
			if( Com_MatchToken( "q3map_globaltexture" ))
			{
				si->globalTexture = true;
				continue;
			}

			// q3map_backsplash <percent> <distance>
			if( Com_MatchToken( "q3map_backsplash" ))
			{
				Com_GetToken( false );
				si->backsplashFraction = com.atof( com_token ) * 0.01f;
				Com_GetToken( false );
				si->backsplashDistance = com.atof( com_token );
				continue;
			}

			// q3map_backshader <shader>
			if( Com_MatchToken( "q3map_backshader" ))
			{
				Com_GetToken( false );
				com.strncpy( si->backShader, com_token, MAX_STRING );
				continue;
			}

			// q3map_flare <shader>
			if( Com_MatchToken( "q3map_flare" ))
			{
				Com_GetToken( false );
				com.strncpy( si->flareShader, com_token, MAX_STRING );
				continue;
			}

			// light <value> 
			// old style flare specification
			if( Com_MatchToken( "light" ))
			{
				Com_GetToken( false );
				com.strncpy( si->flareShader, "flareshader", MAX_STRING );
				continue;
			}

			// q3map_sun <red> <green> <blue> <intensity> <degrees> <elivation>
			// color will be normalized, so it doesn't matter what range you use
			// intensity falls off with angle but not distance 100 is a fairly bright sun
			// degree of 0 = from the east, 90 = north, etc.  altitude of 0 = sunrise/set, 90 = noon
			if( Com_MatchToken( "q3map_sun" ))
			{
				float	a, b;

				Com_GetToken( false );
				si->sunLight[0] = com.atof( com_token );
				Com_GetToken( false );
				si->sunLight[1] = com.atof( com_token );
				Com_GetToken( false );
				si->sunLight[2] = com.atof( com_token );
				VectorNormalize( si->sunLight );

				Com_GetToken( false );
				a = com.atof( com_token );
				VectorScale( si->sunLight, a, si->sunLight );

				Com_GetToken( false );
				a = com.atof( com_token );
				a = a / 180 * M_PI;

				Com_GetToken( false );
				b = com.atof( com_token );
				b = b / 180 * M_PI;

				si->sunDirection[0] = cos( a ) * cos( b );
				si->sunDirection[1] = sin( a ) * cos( b );
				si->sunDirection[2] = sin( b );
				si->surfaceFlags |= SURF_SKY;
				continue;
			}

			// tesssize is used to force liquid surfaces to subdivide
			if( Com_MatchToken( "tesssize" ))
			{
				Com_GetToken( false );
				si->subdivisions = com.atof( com_token );
				continue;
			}

			// cull none will set twoSided
			if( Com_MatchToken( "cull" ))
			{
				Com_GetToken( false );
				if(Com_MatchToken( "twoSided" )) si->twoSided = true;
				if(Com_MatchToken( "disable" )) si->twoSided = true;
				if(Com_MatchToken( "none" )) si->twoSided = true;
				continue;
			}


			// deformVertexes autosprite[2]
			// we catch this so autosprited surfaces become point
			// lights instead of area lights
			if( Com_MatchToken( "deformVertexes" ))
			{
				Com_GetToken( false );
				if( !strnicmp( com_token, "autosprite", 10 ))
				{
					si->autosprite = true;
					si->contents = CONTENTS_DETAIL;
				}
				continue;
			}

			if( Com_MatchToken( "sort" ))
			{
				Com_GetToken( false );
				continue;
			}

			// ignore all other com_tokens on the line
			while( Com_TryToken());
		}			
	}
}

/*
===============
FindShader
===============
*/
bsp_shader_t *FindShader( const char *textureName )
{
	string		shader;
	bsp_shader_t	*si;
	int		i;

	// strip off extension
	com.strncpy( shader, textureName, MAX_STRING );
	FS_StripExtension( shader );	// cut off extension
	com.strlwr( shader, shader );	// convert to lower case

	// look for it
	for( i = 0, si = shaderInfo; i < numShaderInfo; i++, si++ )
	{
		if( !com.stricmp( shader, si->name ))
		{
			if( !si->width || !si->height )
				LoadShaderImage( si );
			return si;			
		}
	}

	// create a new one
	si = AllocShaderInfo();
	com.strncpy( si->name, shader, MAX_STRING );
	LoadShaderImage( si );

	return si;
}


/*
===============
LoadShaderInfo
===============
*/
int LoadShaderInfo( void )
{
	search_t	*search;
	int	i, numShaderFiles;

	numShaderFiles = 0;
	search = FS_Search("scripts/shaders/*.txt", true );
	if (!search) return 0;
	
	for( i = 0; i < search->numfilenames; i++ )
	{
		ParseShaderFile( search->filenames[i] );
		numShaderFiles++;
	}
	Mem_Free( search );

	return numShaderInfo;
}