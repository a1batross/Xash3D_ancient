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
	script_t		*shader;
	token_t		token;
	string		name;
	bsp_shader_t	*si;

	shader = Com_OpenScript( filename, NULL, 0 );

	if( shader )
	{
		FS_FileBase( filename, name );
		MsgDev( D_INFO, "Adding shader: %s.txt\n", name );
          }
          
	while( shader )
	{
		if ( !Com_ReadToken( shader, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
			break;

		si = AllocShaderInfo();
		com.strncpy( si->name, token.string, sizeof( si->name ));
		Com_ReadToken( shader, SC_ALLOW_NEWLINES, &token );
		
		if( com.strcmp( token.string, "{" ))
		{
			Msg( "ParseShaderFile: shader %s without opening brace!\n", si->name );
			continue;
                    }
                    
		while ( 1 )
		{
			if ( !Com_ReadToken( shader, SC_ALLOW_NEWLINES, &token )) break;
			if ( !com.strcmp( token.string, "}" )) break;

			// skip internal braced sections
			if ( !com.strcmp( token.string, "{" ))
			{
				si->hasStages = true;
				Com_SkipBracedSection( shader, 1 );
				continue;
			}

			if( !com.stricmp( token.string, "surfaceparm" ))
			{
				Com_ReadToken( shader, 0, &token );
				for( i = 0; i < numInfoParms; i++ )
				{
					if( !com.stricmp( token.string, infoParms[i].name ))
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
			if( !com.stricmp( token.string, "qer_editorimage" ))
			{
				Com_ReadString( shader, false, si->editorimage );
				continue;
			}

			// q3map_lightimage <image>
			if( !com.stricmp( token.string, "q3map_lightimage" ))
			{
				Com_ReadString( shader, false, si->lightimage );
				continue;
			}

			// q3map_surfacelight <value>
			if( !com.stricmp( token.string, "q3map_surfacelight" ))
			{
				Com_ReadLong( shader, false, &si->value );
				continue;
			}

			// q3map_lightsubdivide <value>
			if( !com.stricmp( token.string, "q3map_lightsubdivide" ))
			{
				Com_ReadFloat( shader, false, &si->lightSubdivide );
				continue;
			}

			// q3map_lightmapsamplesize <value>
			if( !com.stricmp( token.string, "q3map_lightmapsamplesize" ))
			{
				Com_ReadLong( shader, false, &si->lightmapSampleSize );
				continue;
			}

			// q3map_tracelight
			if( !com.stricmp( token.string, "q3map_tracelight" ))
			{
				si->forceTraceLight = true;
				continue;
			}

			// q3map_vlight
			if ( !com.stricmp( token.string, "q3map_vlight" ))
			{
				si->forceVLight = true;
				continue;
			}

			// q3map_forcesunlight
			if( !com.stricmp( token.string, "q3map_forcesunlight" ))
			{
				si->forceSunLight = true;
				continue;
			}

			// q3map_vertexscale
			if( !com.stricmp( token.string, "q3map_vertexscale" ))
			{
				Com_ReadFloat( shader, false, &si->vertexScale );
				continue;
			}

			// q3map_notjunc
			if( !com.stricmp( token.string, "q3map_notjunc" ))
			{
				si->notjunc = true;
				continue;
			}

			// q3map_globaltexture
			if( !com.stricmp( token.string, "q3map_globaltexture" ))
			{
				si->globalTexture = true;
				continue;
			}

			// q3map_backsplash <percent> <distance>
			if( !com.stricmp( token.string, "q3map_backsplash" ))
			{
				Com_ReadFloat( shader, false, &si->backsplashFraction );
				si->backsplashFraction *= 0.01f;
				Com_ReadFloat( shader, false, &si->backsplashDistance );
				continue;
			}

			// q3map_backshader <shader>
			if( !com.stricmp( token.string, "q3map_backshader" ))
			{
				Com_ReadString( shader, false, si->backShader );
				continue;
			}

			// q3map_flare <shader>
			if( !com.stricmp( token.string, "q3map_flare" ))
			{
				Com_ReadString( shader, false, si->flareShader );
				continue;
			}

			// light <value> 
			// old style flare specification
			if( !com.stricmp( token.string, "light" ))
			{
				Com_ReadToken( shader, 0, NULL );	// old-style declared
				com.strncpy( si->flareShader, "flareshader", MAX_STRING );
				continue;
			}

			// q3map_sun <red> <green> <blue> <intensity> <degrees> <elivation>
			// color will be normalized, so it doesn't matter what range you use
			// intensity falls off with angle but not distance 100 is a fairly bright sun
			// degree of 0 = from the east, 90 = north, etc.  altitude of 0 = sunrise/set, 90 = noon
			if( !com.stricmp( token.string, "q3map_sun" ))
			{
				float	a, b;

				Com_ReadFloat( shader, false, &si->sunLight[0] );
				Com_ReadFloat( shader, false, &si->sunLight[1] );
				Com_ReadFloat( shader, false, &si->sunLight[2] );
				VectorNormalize( si->sunLight );

				Com_ReadFloat( shader, false, &a );
				VectorScale( si->sunLight, a, si->sunLight );

				Com_ReadFloat( shader, false, &a );
				a = a / 180 * M_PI;

				Com_ReadFloat( shader, false, &b );
				b = b / 180 * M_PI;

				si->sunDirection[0] = com.cos( a ) * com.cos( b );
				si->sunDirection[1] = com.sin( a ) * com.cos( b );
				si->sunDirection[2] = com.sin( b );
				si->surfaceFlags |= SURF_SKY;
				continue;
			}

			// tesssize is used to force liquid surfaces to subdivide
			if( !com.stricmp( token.string, "tessSize" ))
			{
				Com_ReadFloat( shader, false, &si->subdivisions );
				continue;
			}

			// cull none will set twoSided
			if( !com.stricmp( token.string, "cull" ))
			{
				Com_ReadToken( shader, 0, &token );
				if( !com.stricmp( token.string, "twoSided" )) si->twoSided = true;
				if( !com.stricmp( token.string, "disable" )) si->twoSided = true;
				if( !com.stricmp( token.string, "none" )) si->twoSided = true;
				continue;
			}


			// deformVertexes autosprite[2]
			// we catch this so autosprited surfaces become point
			// lights instead of area lights
			if( !com.stricmp( token.string, "deformVertexes" ))
			{
				Com_ReadToken( shader, 0, &token );
				if( !strnicmp( token.string, "autosprite", 10 ))
				{
					si->autosprite = true;
					si->contents = CONTENTS_DETAIL;
				}
				continue;
			}

			// ignore all other com_tokens on the line
			Com_SkipRestOfLine( shader );
		}			
	}
	Com_CloseScript( shader );
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
	search = FS_Search( "scripts/shaders/*.txt", true );
	if (!search) return 0;
	
	for( i = 0; i < search->numfilenames; i++ )
	{
		ParseShaderFile( search->filenames[i] );
		numShaderFiles++;
	}
	Mem_Free( search );

	return numShaderInfo;
}