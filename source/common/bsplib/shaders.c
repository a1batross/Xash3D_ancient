#include "bsplib.h"

#define MAX_SHADER_FILES		64
#define MAX_SURFACE_INFO		4096
int numShaderInfo = 0;

shader_t shaderInfo[MAX_SURFACE_INFO];

typedef struct
{
	char	*name;
	int	surfaceFlags;
	int	contents;
	bool	notsolid;
} infoParm_t;

infoParm_t infoParms[] =
{
	// server relevant contents
	{"window",	SURF_BLEND,	CONTENTS_WINDOW,		0},
	{"aux",		SURF_NONE,	CONTENTS_AUX,		0},		
	{"lava",		SURF_WARP,	CONTENTS_LAVA,		1}, // very damaging
	{"slime",		SURF_WARP,	CONTENTS_SLIME,		1}, // mildly damaging
	{"water",		SURF_WARP,	CONTENTS_WATER,		1},
          
	// utility relevant attributes
	{"fog",		SURF_NONE,	CONTENTS_FOG,		0}, // carves surfaces entering
	{"areaportal",	SURF_NONE,	CONTENTS_AREAPORTAL,	1},
	{"playerclip",	SURF_NONE,	CONTENTS_PLAYERCLIP,	1},
	{"monsterclip",	SURF_NONE,	CONTENTS_MONSTERCLIP,	1},
	{"clip",		SURF_NODRAW,	CONTENTS_CLIP,		1},
	{"current0",	SURF_NONE,	CONTENTS_CURRENT_0,		0},
	{"current90",	SURF_NONE,	CONTENTS_CURRENT_90,	0},
	{"current180",	SURF_NONE,	CONTENTS_CURRENT_180,	0},
	{"current270",	SURF_NONE,	CONTENTS_CURRENT_270,	0},
	{"currentup",	SURF_NONE,	CONTENTS_CURRENT_UP,	0},	
	{"currentdown",	SURF_NONE,	CONTENTS_CURRENT_DOWN,	0},
	{"origin",	SURF_NONE,	CONTENTS_ORIGIN,		1}, // center of rotating brushes
	{"trans",		SURF_NONE,	CONTENTS_TRANSLUCENT,	0}, // don't eat contained surfaces
	{"detail",	SURF_NONE,	CONTENTS_DETAIL,		0}, // don't include in structural bsp
	{"sky",		SURF_SKY,		CONTENTS_NONE,		0}, // emit light from environment map
	{"hint",		SURF_HINT,	CONTENTS_NONE,		0}, // use as a primary splitter
	{"skip",		SURF_SKIP,	CONTENTS_NONE,		0}, // use as a secondary splitter
	{"null",		SURF_NODRAW,	CONTENTS_SOLID,		0},
	{"mirror",	SURF_MIRROR,	CONTENTS_SOLID,		0},

	// server attributes
	{"slick",		SURF_SLICK,	CONTENTS_NONE,		0},
	{"light",		SURF_LIGHT,	CONTENTS_NONE,		0},
	{"ladder",	SURF_NONE,	CONTENTS_LADDER,		0},

	// drawsurf attributes
	{"nodraw",	SURF_NODRAW,	CONTENTS_NONE,		0,}, // don't generate a drawsurface
};

/*
===============
FindShader
===============
*/
shader_t *FindShader( const char *texture )
{
	shader_t *texshader;
	char	shader[128];
	char	texname[128];
	int i;

	// convert to lower case
	strlower(texture, texname );
          
	// build full path
	com.sprintf (shader, "textures/%s", texname );
	
	// look for it
	for (i = 0, texshader = shaderInfo; i < numShaderInfo; i++, texshader++)
	{
		if(!com.strcmp(shader, texshader->name))
			return texshader;
	}
	return NULL; //no shaders for this texture
}

/*
===============
AllocShaderInfo
===============
*/
static shader_t *AllocShaderInfo( void )
{
	shader_t		*si;

	if ( numShaderInfo == MAX_SURFACE_INFO ) Sys_Error( "MAX_SURFACE_INFO" );

	si = &shaderInfo[ numShaderInfo ];
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
static void ParseShaderFile( char *filename )
{
	int		i, numInfoParms = sizeof(infoParms) / sizeof(infoParms[0]);
	char		name[128];
	shader_t		*si;

	bool load = Com_LoadScript( filename, NULL, 0 );

	if( load )
	{
		FS_FileBase( filename, name );
		MsgDev(D_INFO, "Adding shader: %s.txt\n", name );
          }
          
	while ( load )
	{
		if ( !Com_GetToken( true )) break;

		si = AllocShaderInfo();
		strcpy( si->name, com_token );
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
				si->hasPasses = true;
				while ( 1 )
				{
					if ( !Com_GetToken( true )) break;
					if ( !strcmp( com_token, "}" )) break;
				}
				continue;
			}

			if ( !stricmp( com_token, "nextframe" ))
			{                              
				Com_GetToken( false );
				strcpy(si->nextframe, com_token );
			}
			if ( !stricmp( com_token, "surfaceparm" ))
			{
				Com_GetToken( false );
				for ( i = 0 ; i < numInfoParms ; i++ )
				{
					if ( !stricmp( com_token, infoParms[i].name ))
					{
						si->surfaceFlags |= infoParms[i].surfaceFlags;
						si->contents |= infoParms[i].contents;
						if ( infoParms[i].notsolid )
							si->contents &= ~CONTENTS_SOLID;
						break;
					}
				}
				continue;
			}
			// light color <value> <value> <value>
			if ( !stricmp( com_token, "radiocity" )  )
			{
				Com_GetToken( false );
				si->color[0] = atof( com_token );
				Com_GetToken( false );
				si->color[1] = atof( com_token );
				Com_GetToken( false );
				si->color[2] = atof( com_token );
				continue;
			}

			// light intensity <value>
			if ( !stricmp( com_token, "intensity" ))
			{
				Com_GetToken( false );
				si->intensity = atoi( com_token );
				continue;
			}

			// ignore all other com_tokens on the line
			while (Com_TryToken());
		}			
	}
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