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

// NOTE: this table must match with same table in render\r_shader.c
infoParm_t infoParms[] =
{
	// server relevant contents
	{"window",	SURF_NONE,	CONTENTS_WINDOW,		0},
	{"aux",		SURF_NONE,	CONTENTS_AUX,		0},
	{"warp",		SURF_WARP,	CONTENTS_NONE,		0},
	{"water",		SURF_NONE,	CONTENTS_WATER,		1},
	{"slime",		SURF_NONE,	CONTENTS_SLIME,		1}, // mildly damaging
	{"lava",		SURF_NONE,	CONTENTS_LAVA,		1}, // very damaging
	{"playerclip",	SURF_NODRAW,	CONTENTS_PLAYERCLIP,	1},
	{"monsterclip",	SURF_NODRAW,	CONTENTS_MONSTERCLIP,	1},
	{"clip",		SURF_NODRAW,	CONTENTS_CLIP,		1},
	{"notsolid",	SURF_NONE,	CONTENTS_NONE,		1}, // just clear solid flag
	{"trigger",	SURF_NODRAW,	CONTENTS_TRIGGER,		1}, // trigger volume
	          
	// utility relevant attributes
	{"origin",	SURF_NODRAW,	CONTENTS_ORIGIN,		1}, // center of rotating brushes
	{"nolightmap",	SURF_NOLIGHTMAP,	CONTENTS_NONE,		0}, // don't generate a lightmap
	{"translucent",	SURF_NONE,	CONTENTS_TRANSLUCENT,	0}, // don't eat contained surfaces
	{"detail",	SURF_NONE,	CONTENTS_DETAIL,		0}, // don't include in structural bsp
	{"fog",		SURF_NOLIGHTMAP,	CONTENTS_FOG,		0}, // carves surfaces entering
	{"sky",		SURF_SKY,		CONTENTS_SKY,		0}, // emit light from environment map
	{"hint",		SURF_HINT,	CONTENTS_NONE,		0}, // use as a primary splitter
	{"skip",		SURF_SKIP,	CONTENTS_NONE,		0}, // use as a secondary splitter
	{"null",		SURF_NODRAW,	CONTENTS_NONE,		0}, // nodraw texture
	{"mirror",	SURF_MIRROR,	CONTENTS_NONE,		0},

	// server attributes
	{"slick",		SURF_SLICK,	CONTENTS_NONE,		0},
	{"light",		SURF_LIGHT,	CONTENTS_NONE,		0},
	{"ladder",	SURF_NONE,	CONTENTS_LADDER,		0},
};

/*
===============
FindShader
===============
*/
bsp_shader_t *FindShader( const char *texture )
{
	bsp_shader_t	*texshader;
	string		shader;
	int		i;

	// convert to lower case
	com.strlwr( texture, shader );

	// look for it
	for( i = 0, texshader = shaderInfo; i < numShaderInfo; i++, texshader++)
	{
		if(!com.stricmp( shader, texshader->name ))
			return texshader;
	}
	return NULL; //no shaders for this texture
}

/*
===============
AllocShaderInfo
===============
*/
static bsp_shader_t *AllocShaderInfo( void )
{
	bsp_shader_t	*si;

	if ( numShaderInfo == MAX_SURFACE_INFO )
		Sys_Error( "MAX_SURFACE_INFO limit exceeded\n" );

	si = &shaderInfo[numShaderInfo];
	numShaderInfo++;

	// set defaults
	si->contents = CONTENTS_SOLID;
          si->color[0] = 0;
          si->color[1] = 0;
          si->color[2] = 0;
	si->surfaceFlags = 0;
          si->intensity = 0;

	return si;
}

/*
===============
ParseShaderFile
===============
*/
static void ParseShaderFile( const char *filename )
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
		MsgDev( D_LOAD, "Adding: %s.shader\n", name );
          }
          
	while( shader )
	{
		if( !Com_ReadToken( shader, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
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
				si->hasPasses = true;
				Com_SkipBracedSection( shader, 1 );
				continue;
			}

			if( !com.stricmp( token.string, "surfaceParm" ))
			{
				Com_ReadToken( shader, 0, &token );
				for( i = 0; i < numInfoParms; i++ )
				{
					if( !com.stricmp( token.string, infoParms[i].name ))
					{
						si->surfaceFlags |= infoParms[i].surfaceFlags;
						si->contents |= infoParms[i].contents;
						if( infoParms[i].clearSolid )
							si->contents &= ~CONTENTS_SOLID;
						break;
					}
				}
				continue;
			}

			// light color <value> <value> <value>
			if( !com.stricmp( token.string, "rad_color" ))
			{
				Com_ReadFloat( shader, false, &si->color[0] );
				Com_ReadFloat( shader, false, &si->color[1] );
				Com_ReadFloat( shader, false, &si->color[2] );
				continue;
			}

			// light intensity <value>
			if( !com.stricmp( token.string, "rad_intensity" ))
			{
				Com_ReadLong( shader, false, &si->intensity );
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
LoadShaderInfo
===============
*/
int LoadShaderInfo( void )
{
	search_t	*search;
	int	i, numShaderFiles;

	numShaderFiles = 0;
	search = FS_Search( "scripts/*.shader", true );
	if( !search ) return 0;
	
	for( i = 0; i < search->numfilenames; i++ )
	{
		ParseShaderFile( search->filenames[i] );
		numShaderFiles++;
	}
	Mem_Free( search );

	return numShaderInfo;
}