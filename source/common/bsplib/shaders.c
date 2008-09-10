#include "bsplib.h"
#include "const.h"

#define MAX_SHADER_FILES		64
#define MAX_SURFACE_INFO		4096
int numShaderInfo = 0;

shader_t shaderInfo[MAX_SURFACE_INFO];

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
	{"window",	SURF_BLEND,	0,			0}, // normal window
	{"lava",		SURF_WARP,	CONTENTS_LAVA,		1}, // very damaging
	{"slime",		SURF_WARP,	CONTENTS_SLIME,		1}, // mildly damaging
	{"water",		SURF_WARP,	CONTENTS_WATER,		1},
          
	// utility relevant attributes
	{"fog",		0,		CONTENTS_FOG,		1}, // carves surfaces entering
	{"detail",	0,		CONTENTS_DETAIL,		0}, // carves surfaces entering
	{"world",		0,		CONTENTS_STRUCTURAL,	0}, // force into world even if trans
	{"areaportal",	0,		CONTENTS_AREAPORTAL,	1},
	{"playerclip",	0,		CONTENTS_PLAYERCLIP,	1},
	{"monsterclip",	0,		CONTENTS_MONSTERCLIP,	1},
	{"clip",		SURF_NODRAW,	CONTENTS_CLIP,		1},
	{"origin",	0,		CONTENTS_ORIGIN,		1}, // center of rotating brushes
	{"alpha",		SURF_ALPHA,	CONTENTS_TRANSLUCENT,	0}, // don't eat contained surfaces
	{"additive",	SURF_ADDITIVE,	CONTENTS_TRANSLUCENT,	0}, // don't eat contained surfaces
	{"chrome",	SURF_CHROME,	0,			0},
	{"sky",		SURF_SKY,		0,			0}, // emit light from environment map
	{"hint",		SURF_HINT,	0,			0}, // use as a primary splitter
	{"skip",		SURF_NODRAW,	0,			0}, // use as a secondary splitter
	{"mirror",	SURF_MIRROR,	CONTENTS_SOLID,		0},
	{"portal",	SURF_PORTAL,	CONTENTS_TRIGGER,		0},
	{"blend",		SURF_BLEND,	0,			0}, // normal window

	// drawsurf attributes
	{"null",		SURF_NODRAW,	0,			0}, // don't generate a drawsurface
	{"nolightmap",	SURF_NOLIGHTMAP,	0,			0}, // don't generate a lightmap
	{"nodlight",	SURF_NODLIGHT,	0,			0}, // don't ever add dynamic lights

	// server attributes
	{"light",		SURF_LIGHT,	0,			0},
	{"lightmap",	SURF_LIGHT,	0,			0},
	{"ladder",	0,		CONTENTS_LADDER,		0},
};

/*
===============
AllocShaderInfo
===============
*/
static shader_t *AllocShaderInfo( void )
{
	shader_t		*si;

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
static void LoadShaderImage( shader_t *si )
{
	rgbdata_t		*pic;

	if( !si ) return;
	if( !com.stricmp( si->name, "default" )) 
		return;
	pic = FS_LoadImage( si->name, NULL, 0 );
	if( !pic ) return;

	si->width = pic->width;
	si->height = pic->height;

	// get radiosity color too
	if(VectorIsNull( si->color ))
	{
		// if lightmap specified as different image
		if( com.stricmp( si->name, si->lightmap ))
		{
			FS_FreeImage( pic );
			pic = FS_LoadImage( si->name, NULL, 0 );
		}
		Image_GetColor( pic );
		if( pic ) VectorCopy( pic->color, si->color );
		if( !si->value && pic ) si->value = pic->bump_scale;
	}
	if( pic ) FS_FreeImage( pic ); // release image

	// to avoid multiple loading
	if(VectorIsNull( si->color )) VectorSet( si->color, 0.5f, 0.5f, 0.5f );
	if( !si->value ) si->value = 100;
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
		com.strcpy( si->name, com_token );
		com.strcpy( si->lightmap, com_token );	// can be overrided later
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

			// cull none will set twoSided
			if( Com_MatchToken( "cull" ))
			{
				Com_GetToken( false );
				if( Com_MatchToken( "none" ))
					si->twoSided = true;
				continue;
			}

			// q3map_surfacelight <value>
			if( Com_MatchToken( "surfacelight" ))
			{
				Com_GetToken( false );
				si->value = com.atoi( com_token );
				si->surfaceFlags |= SURF_LIGHT;
				continue;
			}

			// light color <value> <value> <value>
			// FIXME
			if( Com_MatchToken( "radiocity" )  )
			{
				Com_GetToken( false );
				si->color[0] = atof( com_token );
				Com_GetToken( false );
				si->color[1] = atof( com_token );
				Com_GetToken( false );
				si->color[2] = atof( com_token );
				continue;
			}

			// custom lightmap
			if( Com_MatchToken( "lmap" ))
			{
				Com_GetToken( false );
				com.strcpy( si->lightmap, com_token );
				si->notjunc = true;
				continue;
			}

			// q3map_notjunc
			if( Com_MatchToken( "notjunc" ))
			{
				si->notjunc = true;
				continue;
			}

			// q3map_globaltexture
			if( Com_MatchToken( "globaltexture" ))
			{
				si->globalTexture = true;
				continue;
			}

			// tesssize is used to force liquid surfaces to subdivide
			if( Com_MatchToken( "tesssize" ))
			{
				Com_GetToken( false );
				si->subdivisions = com.atof( com_token );
				continue;
			}

			if( !stricmp( com_token, "sort" ))
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
shader_t *FindShader( const char *textureName )
{
	string		shader;
	shader_t		*si;
	int		i;

	// strip off extension
	com.strncpy( shader, textureName, MAX_STRING );
	FS_StripExtension( shader );	// cut off extension
	com.strlwr( shader, shader );	// convert to lower case

	// look for it
	for( i = 0, si = shaderInfo; i < numShaderInfo; i++, si++ )
	{
		if( !com.stricmp( va("textures/%s", shader ), si->name ) || !com.stricmp( shader, si->name ))
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