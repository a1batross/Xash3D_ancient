#include "bsplib.h"
#include "const.h"

#define MAX_SHADER_FILES		64
#define MAX_SURFACE_INFO		4096
int numShaderInfo = 0;

bsp_shader_t shaderInfo[MAX_SURFACE_INFO];
string mapShaderFile;

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
	{"window",	SURF_BLEND,	CONTENTS_TRANSLUCENT,	0}, // blend surface preset
	{"mirror",	SURF_PORTAL,	0,			0}, // mirror surface
	{"portal",	SURF_PORTAL,	0,			0}, // portal surface
	{"light",		0,		0,			0}, // legacy
};

/*
===============
ColorMod

routines for dealing with vertex color/alpha modification
===============
*/
void ColorMod( cMod_t *cm, int numVerts, dvertex_t *drawVerts )
{
	int		i, j, k;
	vec4_t		mult, add;
	dvertex_t		*dv;
	cMod_t		*cm2;
	float		c;	
	
	if( cm == NULL || numVerts < 1 || drawVerts == NULL )
		return;

	for( i = 0; i < numVerts; i++ )
	{
		dv = &dvertexes[i];
		
		for( cm2 = cm; cm2 != NULL; cm2 = cm2->next )
		{
			VectorSet( mult, 1.0f, 1.0f, 1.0f );
			mult[3] = 1.0f;
			VectorSet( add, 0.0f, 0.0f, 0.0f );
			mult[3] = 0.0f;
			
			switch( cm2->type )
			{
			case CM_COLOR_SET:
				VectorClear( mult );
				VectorScale( cm2->data, 255.0f, add );
				break;
			case CM_ALPHA_SET:
				mult[3] = 0.0f;
				add[3] = cm2->data[ 0 ] * 255.0f;
				break;
			case CM_COLOR_SCALE:
				VectorCopy( cm2->data, mult );
				break;
			case CM_ALPHA_SCALE:
				mult[3] = cm2->data[ 0 ];
				break;
			case CM_COLOR_DOT_PRODUCT:
				c = DotProduct( dv->normal, cm2->data );
				VectorSet( mult, c, c, c );
				break;
			case CM_ALPHA_DOT_PRODUCT:
				mult[3] = DotProduct( dv->normal, cm2->data );
				break;
			case CM_COLOR_DOT_PRODUCT_2:
				c = DotProduct( dv->normal, cm2->data );
				c *= c;
				VectorSet( mult, c, c, c );
				break;
			case CM_ALPHA_DOT_PRODUCT_2:
				mult[3] = DotProduct( dv->normal, cm2->data );
				mult[3] *= mult[3];
				break;
			default: break;
			}
			
			// apply mod
			for( j = 0; j < LM_STYLES; j++ )
			{
				for( k = 0; k < 4; k++ )
				{
					c = (mult[k] * dv->color[j][k]) + add[k];
					dv->color[j][k] = bound( 0, c, 255 );
				}
			}
		}
	}
}

/*
===============
TCMod

routines for dealing with a 3x3 texture mod matrix
FIXME: replace tcMod_t with normal matrix4x4
===============
*/
void TCMod( tcMod_t mod, float st[2] )
{
	float	old[2];
	
	
	old[0] = st[0];
	old[1] = st[1];
	st[0] = (mod[0][0] * old[0]) + (mod[0][1] * old[1]) + mod[0][2];
	st[1] = (mod[1][0] * old[0]) + (mod[1][1] * old[1]) + mod[1][2];
}

void TCModIdentity( tcMod_t mod )
{
	mod[0][0] = 1.0f;
	mod[0][1] = 0.0f;
	mod[0][2] = 0.0f;
	mod[1][0] = 0.0f;
	mod[1][1] = 1.0f;
	mod[1][2] = 0.0f;
	mod[2][0] = 0.0f;
	mod[2][1] = 0.0f;
	mod[2][2] = 1.0f;	// this row is only used for multiples, not transformation
}

void TCModMultiply( tcMod_t a, tcMod_t b, tcMod_t out )
{
	int	i;
	
	for( i = 0; i < 3; i++ )
	{
		out[i][0] = (a[i][0] * b[0][0]) + (a[i][1] * b[1][0]) + (a[i][2] * b[2][0]);
		out[i][1] = (a[i][0] * b[0][1]) + (a[i][1] * b[1][1]) + (a[i][2] * b[2][1]);
		out[i][2] = (a[i][0] * b[0][2]) + (a[i][1] * b[1][2]) + (a[i][2] * b[2][2]);
	}
}

void TCModTranslate( tcMod_t mod, float s, float t )
{
	mod[0][2] += s;
	mod[1][2] += t;
}

void TCModScale( tcMod_t mod, float s, float t )
{
	mod[0][0] *= s;
	mod[1][1] *= t;
}

void TCModRotate( tcMod_t mod, float euler )
{
	tcMod_t	old, temp;
	float	radians, sinv, cosv;
	
	Mem_Copy( old, mod, sizeof( tcMod_t ));
	TCModIdentity( temp );

	radians = DEG2RAD( euler );
	sinv = sin( radians );
	cosv = cos( radians );

	temp[0][0] = cosv;
	temp[0][1] = -sinv;
	temp[1][0] = sinv;
	temp[1][1] = cosv;
	
	TCModMultiply( old, temp, mod );
}

/*
===============
ApplySurfaceParm

applies a named surfaceparm to the supplied flags
===============
*/
bool ApplySurfaceParm( const char *name, int *contentFlags, int *surfaceFlags )
{
	int		i, fake;
	int		numInfoParms = sizeof(infoParms) / sizeof(infoParms[0]);

	if( !name ) name = "";
	if( !contentFlags ) contentFlags = &fake;
	if( !surfaceFlags ) surfaceFlags = &fake;

	for ( i = 0; i < numInfoParms; i++ )
	{
		if(!com.stricmp( name, infoParms[i].name ))
		{
			*surfaceFlags |= infoParms[i].surfaceFlags;
			*contentFlags |= infoParms[i].contents;
			if( infoParms[i].clearSolid )
				*contentFlags &= ~CONTENTS_SOLID;
			return true;
		}
	}

	// no matching surfaceparm found
	return false;
}

/*
===============
BeginMapShaderFile

erases and starts a new map shader script
===============
*/
void BeginMapShaderFile( void )
{
	com.sprintf( mapShaderFile, "scripts/shaders/bsp_%s.txt", gs_filename );
	FS_Delete( mapShaderFile );
}



/*
===============
WriteMapShaderFile

writes a shader to the map shader script
===============
*/
void WriteMapShaderFile( void )
{
	file_t		*file;
	bsp_shader_t	*si;
	int		i, num;

	if( mapShaderFile[0] == '\0' )
		return;
	
	for( i = 0, num = 0; i < numShaderInfo; i++ )
	{
		if( shaderInfo[i].custom ) 
			break;
	}
	if( i == numShaderInfo )
		return;
	
	MsgDev( D_NOTE, "--- WriteMapShaderFile ---\n");
	MsgDev( D_NOTE, "Writing %s", mapShaderFile );
	
	/* open shader file */
	file = FS_Open( mapShaderFile, "w" );
	if( file == NULL )
	{
		MsgDev( D_WARN, "unable to open map shader file %s for writing\n", mapShaderFile );
		return;
	}
	
	// print header
	FS_Printf (file, "//=======================================================================\n");
	FS_Printf (file, "//\t\t\tCopyright XashXT Group 2008 ©\n");
	FS_Printf (file, "//\t\t      custom shader for %s.bsp\n", gs_filename );
	FS_Printf (file, "//=======================================================================\n");
	
	// walk the shader list
	for( i = 0, num = 0; i < numShaderInfo; i++ )
	{
		si = &shaderInfo[i];
		if( si->custom == false || si->shaderText == NULL || si->shaderText[0] == '\0' )
			continue;
		num++;
		FS_Printf( file, "%s%s\n", si->name, si->shaderText );
	}
	FS_Close( file );
}



/*
===============
CustomShader

sets up a custom map shader
===============
*/
bsp_shader_t *CustomShader( bsp_shader_t *si, const char *find, const char *replace )
{
	bsp_shader_t	*csi;
	string		shader;
	char		*s;
	int		loc;
	char		*srcShaderText, temp[8192], shaderText[8192];
	
	if( si == NULL ) return FindShader( "default" );
	
	// default shader text source
	srcShaderText = si->shaderText;
	
	if( si->implicitMap == IM_OPAQUE )
	{
		srcShaderText = temp;
		com.snprintf( temp, 8192, "\n"
			"{ // bsplib defaulted (implicitMap)\n"
			"\t{\n"
			"\t\tmap $lightmap\n"
			"\t\trgbGen identity\n"
			"\t}\n"
			"\tq3map_styleMarker\n"
			"\t{\n"
			"\t\tmap %s\n"
			"\t\tblendFunc GL_DST_COLOR GL_ZERO\n"
			"\t\trgbGen identity\n"
			"\t}\n"
			"}\n",
			si->implicitImagePath );
	}
	else if( si->implicitMap == IM_MASKED )
	{
		srcShaderText = temp;
		com.snprintf( temp, 8192, "\n"
			"{ // bsplib defaulted (implicitMask)\n"
			"\tcull none\n"
			"\t{\n"
			"\t\tmap %s\n"
			"\t\talphaFunc GE128\n"
			"\t\tdepthWrite\n"
			"\t}\n"
			"\t{\n"
			"\t\tmap $lightmap\n"
			"\t\trgbGen identity\n"
			"\t\tdepthFunc equal\n"
			"\t}\n"
			"\tq3map_styleMarker\n"
			"\t{\n"
			"\t\tmap %s\n"
			"\t\tblendFunc GL_DST_COLOR GL_ZERO\n"
			"\t\tdepthFunc equal\n"
			"\t\trgbGen identity\n"
			"\t}\n"
			"}\n",
			si->implicitImagePath,
			si->implicitImagePath );
	}
	else if( si->implicitMap == IM_BLEND )
	{
		srcShaderText = temp;
		com.snprintf( temp, 8192, "\n"
			"{ // bsplib defaulted (implicitBlend)\n"
			"\tcull none\n"
			"\t{\n"
			"\t\tmap %s\n"
			"\t\tblendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA\n"
			"\t}\n"
			"\t{\n"
			"\t\tmap $lightmap\n"
			"\t\trgbGen identity\n"
			"\t\tblendFunc GL_DST_COLOR GL_ZERO\n"
			"\t}\n"
			"\tq3map_styleMarker\n"
			"}\n",
			si->implicitImagePath );
	}
	else if( srcShaderText == NULL )
	{
		srcShaderText = temp;
		com.snprintf( temp, 8192, "\n"
			"{ // Q3Map2 defaulted\n"
			"\t{\n"
			"\t\tmap $lightmap\n"
			"\t\trgbGen identity\n"
			"\t}\n"
			"\tq3map_styleMarker\n"
			"\t{\n"
			"\t\tmap %s\n"
			"\t\tblendFunc GL_DST_COLOR GL_ZERO\n"
			"\t\trgbGen identity\n"
			"\t}\n"
			"}\n",
			si->name );
	}
	
	if((com.strlen(gs_filename) + 1 ) > MAX_SHADERPATH )
		Sys_Break( "Custom shader name length (%d) exceeded. Shorten your map name.\n", MAX_SHADERPATH );
	
	// do some bad find-replace
	s = com.strstr( srcShaderText, find );
	if( s == NULL ) return si;	// testing just using the existing shader if this fails
	else
	{
		// substitute 'find' with 'replace'
		loc = s - srcShaderText;
		com.strcpy( shaderText, srcShaderText );
		shaderText[loc] = '\0';
		com.strcat( shaderText, replace );
		com.strcat( shaderText, &srcShaderText[loc + com.strlen( find )] );
	}

	// create unique shader name
	com.sprintf( shader, "%s/%i", gs_filename, Com_BlockChecksum( shaderText, com.strlen( shaderText )));

	csi = FindShader( shader );
	if( csi->custom ) return csi;
	
	Mem_Copy( csi, si, sizeof( bsp_shader_t ) );
	com.strcpy( csi->name, shader );
	csi->custom = true;
	csi->shaderText = BSP_Malloc( com.strlen( shaderText ) + 1 );
	com.strcpy( csi->shaderText, shaderText );
	
	return csi;
}

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
	Mem_Set( si, 0, sizeof( *si ));
	si->contents = CONTENTS_SOLID;
	si->backsplashFraction = DEF_BACKSPLASH_FRACTION;
	si->backsplashDistance = DEF_BACKSPLASH_DISTANCE;
	si->bounceScale = DEF_RADIOSITY_BOUNCE;
	si->lightStyle = LS_NORMAL;
	si->polygonOffset = false;
	si->shadeAngleDegrees = 0.0f;
	si->lightmapSampleSize = 0;
	si->lightmapSampleOffset = DEFAULT_LIGHTMAP_SAMPLE_OFFSET;
	si->patchShadows = false;
	si->vertexShadows = true;	// ydnar: changed default behavior
	si->forceSunlight = false;
	si->vertexScale = 1.0;
	si->notjunc = false;
	TCModIdentity( si->mod );
	si->lmCustomWidth = bsp_lightmap_size->integer;
	si->lmCustomHeight = bsp_lightmap_size->integer;

	return si;
}

/*
===============
FinishShader

sets a shader's width and height among other things
===============
*/
void FinishShader( bsp_shader_t *si )
{
	int	x, y;
	float	st[2], o[2], dist, bestDist;
	vec4_t	color, bestColor, delta;

	// don't double-dip
	if( si->finished ) return;
	
	// if they're explicitly set, copy from image size
	if( si->width == 0 && si->height == 0 )
	{
		si->width = si->shaderImage->width;
		si->height = si->shaderImage->height;
	}
	
	// legacy terrain has explicit image-sized texture projection
	if( si->legacyTerrain && si->tcGen == false )
	{
		// set xy texture projection
		si->tcGen = true;
		VectorSet( si->vecs[0], (1.0f / (si->width * 0.5f)), 0, 0 );
		VectorSet( si->vecs[1], 0, (1.0f / (si->height * 0.5f)), 0 );
	}
	
	// find pixel coordinates best matching the average color of the image
	bestDist = 99999999;
	o[0] = 1.0f / si->shaderImage->width;
	o[1] = 1.0f / si->shaderImage->height;
	for( y = 0, st[1] = 0.0f; y < si->shaderImage->height; y++, st[1] += o[1] )
	{
		for( x = 0, st[0] = 0.0f; x < si->shaderImage->width; x++, st[0] += o[0] )
		{
			// sample the shader image
			RadSampleImage( si->shaderImage->buffer, si->shaderImage->width, si->shaderImage->height, st, color );
			
			// determine error squared
			VectorSubtract( color, si->averageColor, delta );
			delta[3] = color[3] - si->averageColor[3];
			dist = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2] + delta[3] * delta[3];
			if( dist < bestDist )
			{
				VectorCopy( color, bestColor );
				bestColor[ 3 ] = color[ 3 ];
				si->stFlat[ 0 ] = st[ 0 ];
				si->stFlat[ 1 ] = st[ 1 ];
			}
		}
	}
	
	// set to finished
	si->finished = true;
}

/*
===============
LoadShaderImage
===============
*/
static void LoadShaderImages( bsp_shader_t *si )
{
	int	count;
	vec4_t	color;
	int	i;

	if( !si ) return;

	// force to load blank whiteimage
	if( si->surfaceFlags & SURF_NODRAW || !com.strcmp( si->name, "noshader" ))
	{
		si->shaderImage = FS_LoadImage( DEFAULT_IMAGE, checkermate_dds, checkermate_dds_size );
	}
	else
	{
		// try to load editor image first
		if( si->editorImagePath[0] != '\0' )
			si->shaderImage = FS_LoadImage( si->editorImagePath, NULL, 0 );
		
		// then try shadername
		if( si->shaderImage == NULL )
			si->shaderImage = FS_LoadImage( si->name, NULL, 0 );
		
		// then try implicit image path (NOTE: new behavior!)
		if( si->shaderImage == NULL && si->implicitImagePath[0] != '\0' )
			si->shaderImage = FS_LoadImage( si->implicitImagePath, NULL, 0 );
		
		// then try lightimage (note: new behavior!)
		if( si->shaderImage == NULL && si->lightImagePath[0] != '\0' )
			si->shaderImage = FS_LoadImage( si->lightImagePath, NULL, 0 );
		
		// otherwise, use default image
		if( si->shaderImage == NULL )
		{
			si->shaderImage = FS_LoadImage( DEFAULT_IMAGE, checkermate_dds, checkermate_dds_size );
			if( com.strcmp( si->name, "noshader" ) )
				MsgDev( D_WARN, "Couldn't find image for shader %s\n", si->name );
		}
		
		// load light image
		if( si->lightImagePath[0] != '\0' )
		{
			si->lightImage = FS_LoadImage( si->lightImagePath, NULL, 0 );
		}		
                    
		if( si->normalImagePath[0] != '\0' )
		{		
			// load normalmap image (ok if this is NULL)
			si->normalImage = FS_LoadImage( si->normalImagePath, NULL, 0 );
		}
	}

	// if no light image, use shader image
	if( si->lightImage == NULL ) si->lightImage = si->shaderImage;

	// create default and average colors
	count = si->lightImage->width * si->lightImage->height;
	VectorClear( color );
	color[3] = 0.0f;
	for( i = 0; i < count; i++ )
	{
		color[0] += si->lightImage->buffer[i*4+0];
		color[1] += si->lightImage->buffer[i*4+1];
		color[2] += si->lightImage->buffer[i*4+2];
		color[3] += si->lightImage->buffer[i*4+3];
	}

	// suctom color can be already set form shader
	if( VectorLength( si->color ) <= 0.0f )
		ColorNormalize( color, si->color );
	VectorScale( color, 1.0 / count, si->averageColor );
}

/*
===============
ParseShaderFile
===============
*/
static void ParseShaderFile( const char *filename )
{
	bsp_shader_t	*si;
	token_t		token;
	int		i, val;
	string		name, temp;
	char		shaderText[MAX_MSGLEN];
	script_t		*script = Com_OpenScript( filename, NULL, 0 );

	if( script )
	{
		si = NULL;
		shaderText[0] = '\0';
		FS_FileBase( filename, name );
		MsgDev(D_INFO, "Adding shader: %s.txt\n", name );
	}
          
	while( !Com_EndOfScript( script ))
	{
		// copy shader text to the shaderinfo
		if( si != NULL && shaderText[0] != '\0' )
		{
			com.strcat( shaderText, "\n" );
			si->shaderText = copystring( shaderText );
		}
		
		shaderText[0] = '\0';
		if( !Com_ReadToken( script, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
			break;

		si = AllocShaderInfo();
		com.strncpy( si->name, token.string, sizeof( si->name ));
		Com_ReadToken( script, SC_ALLOW_NEWLINES, &token );
		
		if( com.stricmp( token.string, "{" ))
		{
			Msg("ParseShaderFile: shader %s without opening brace!\n", si->name );
			continue;
                    }
                    
		while ( 1 )
		{
			if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &token ))
				break;
			if( !com.strcmp( token.string, "}" ))
				break;

			// skip internal braced sections
			if( !com.strcmp( token.string, "{" ) )
			{
				si->hasStages = true;
				while ( 1 )
				{
					if( !Com_ReadToken( script, SC_ALLOW_NEWLINES|SC_ALLOW_PATHNAMES, &token ))
						break;
					if( !com.strcmp( token.string, "}" ))
						break;

					// only care about images if we don't have a editor / light image
					if( si->editorImagePath[0] == '\0' && si->lightImagePath[0] == '\0' && si->implicitImagePath[0] == '\0' )
					{
						if( !com.stricmp( token.string, "map" ) || !com.stricmp( token.string, "animMap" ))
						{
							// skip one token for animated stages
							if( !com.stricmp( token.string, "animMap" ))
								Com_GetTokenAppend( script, shaderText, false, &token );
							
							// get an image
							Com_GetTokenAppend( script, shaderText, false, &token );
							if( token.string[0] != '*' && token.string[0] != '$' )
								com.strcpy( si->lightImagePath, token.string );
						}
					}
				}
				continue;
			}

			// match surfaceparm
			else if( !com.stricmp( token.string, "surfaceparm" ))
			{
				Com_GetTokenAppend( script, shaderText, false, &token );
				if(!ApplySurfaceParm( token.string, &si->contents, &si->surfaceFlags ))
					MsgDev( D_WARN, "unknown surfaceparm: \"%s\"\n", token.string );
			}

			// fogparms (for determining fog volumes)
			else if( !com.stricmp( token.string, "fogparms" ))
			{
				si->fogParms = true;
			}

			// polygonoffset (for no culling)
			else if( !com.stricmp( token.string, "polygonoffset" ))
			{
				si->polygonOffset = true;
			}			
			// tesssize is used to force liquid surfaces to subdivide
			else if( !com.stricmp( token.string, "tessSize" ) || !com.stricmp( token.string, "q3map_tessSize" ))
			{
				Com_GetTokenAppend( script, shaderText, false, &token );
				si->subdivisions = com.atof( token.string );
			}
			
			// cull none will set twoSided
			else if( !com.stricmp( token.string, "cull" ))
			{
				Com_GetTokenAppend( script, shaderText, false, &token );
				if( !com.stricmp( token.string, "none" ) || !com.stricmp( token.string, "disable" ) || !com.stricmp( token.string, "twosided" ))
					si->twoSided = true;
			}
			
			// deformVertexes autosprite[2] we catch this so autosprited surfaces become point lights instead of area lights
			else if( !com.stricmp( token.string, "deformVertexes" ))
			{
				Com_GetTokenAppend( script, shaderText, false, &token );
				
				// deformVertexes autosprite(2)
				if( !com.strnicmp( token.string, "autosprite", 10 ))
				{
					// set it as autosprite and detail
					si->autosprite = true;
					ApplySurfaceParm( "detail", &si->contents, &si->surfaceFlags );
					
					// added these useful things
					si->noClip = true;
					si->notjunc = true;
				}
				
				// deformVertexes move <x> <y> <z> <func> <base> <amplitude> <phase> <freq> (particle studio support)
				if( !com.stricmp( token.string, "move" ))
				{
					vec3_t	amt, mins, maxs;
					float	base, amp;
					
					// get move amount
					Com_GetTokenAppend( script, shaderText, false, &token );
					amt[0] = com.atof( token.string );
					Com_GetTokenAppend( script, shaderText, false, &token );
					amt[1] = com.atof( token.string );
					Com_GetTokenAppend( script, shaderText, false, &token );
					amt[2] = com.atof( token.string );
					
					// skip func
					Com_GetTokenAppend( script, shaderText, false, &token );
					
					// get base and amplitude
					Com_GetTokenAppend( script, shaderText, false, &token );
					base = com.atof( token.string );
					Com_GetTokenAppend( script, shaderText, false, &token );
					amp = com.atof( token.string );
					
					// calculate
					VectorScale( amt, base, mins );
					VectorMA( mins, amp, amt, maxs );
					VectorAdd( si->mins, mins, si->mins );
					VectorAdd( si->maxs, maxs, si->maxs );
				} 
			}
			
			// light <value> (old-style flare specification)
			else if( !com.stricmp( token.string, "light" ))
			{
				Com_GetTokenAppend( script, shaderText, false, &token );
				com.strcpy( si->flareShader, "flareshader" );
			}
	
			// enemy territory implicit shaders
			else if( !com.stricmp( token.string, "implicitMap" ))
			{
				si->implicitMap = IM_OPAQUE;
				Com_GetTokenAppend( script, shaderText, false, &token );
				if( token.string[0] == '-' && token.string[1] == '\0' )
					com.sprintf( si->implicitImagePath, "%s", si->name );
				else com.strcpy( si->implicitImagePath, token.string );
			}
			else if( !com.stricmp( token.string, "implicitMask" ))
			{
				si->implicitMap = IM_MASKED;
				Com_GetTokenAppend( script, shaderText, false, &token );
				if( token.string[0] == '-' && token.string[1] == '\0' )
					com.sprintf( si->implicitImagePath, "%s", si->name );
				else com.strcpy( si->implicitImagePath, token.string );
			}

			else if( !com.stricmp( token.string, "implicitBlend" ) )
			{
				si->implicitMap = IM_MASKED;
				Com_GetTokenAppend( script, shaderText, false, &token );
				if( token.string[0] == '-' && token.string[1] == '\0' )
					com.sprintf( si->implicitImagePath, "%s", si->name );
				else com.strcpy( si->implicitImagePath, token.string );
			}
			// qer_editorimage <image>
			else if( !com.stricmp( token.string, "qer_editorImage" ))
			{
				Com_GetTokenAppend( script, shaderText, false, &token );
				com.strcpy( si->editorImagePath, token.string );
			}
			
			// q3map_normalimage <image> (bumpmapping normal map)
			else if( !com.stricmp( token.string, "q3map_normalImage" ) )
			{
				Com_GetTokenAppend( script, shaderText, false, &token );
				com.strcpy( si->normalImagePath, token.string );
			}
			
			// q3map_lightimage <image>
			else if( !com.stricmp( token.string, "q3map_lightImage" ) )
			{
				Com_GetTokenAppend( script, shaderText, false, &token );
				com.strcpy( si->lightImagePath, token.string );
			}
			
			// skyparms <outer image> <cloud height> <inner image> */
			else if( !com.stricmp( token.string, "skyParms" ) )
			{
				Com_GetTokenAppend( script, shaderText, false, &token );
				
				// ignore bogus paths
				if( com.stricmp( token.string, "-" ) && com.stricmp( token.string, "full" ))
				{
					com.strcpy( si->skyParmsImageBase, token.string );
					
					// use top image as sky light image
					if( si->lightImagePath[0] == '\0' )
						com.sprintf( si->lightImagePath, "%s_up", si->skyParmsImageBase );
				}
				
				// skip rest of line
				Com_GetTokenAppend( script, shaderText, false, &token );
				Com_GetTokenAppend( script, shaderText, false, &token );
			}

			// q3map_sun <red> <green> <blue> <intensity> <degrees> <elevation>
			// color will be normalized, so it doesn't matter what range you use
			// intensity falls off with angle but not distance 100 is a fairly bright sun
			// degree of 0 = from the east, 90 = north, etc.  altitude of 0 = sunrise/set, 90 = noon
			else if( !com.stricmp( token.string, "q3map_sun" ) || !com.stricmp( token.string, "q3map_sunExt" ))
			{
				float	a, b;
				sun_t	*sun;
				bool	ext;
				
				// ydnar: extended sun directive?
				if( !com.stricmp( token.string, "q3map_sunext" ))
					ext = true;
				
				sun = BSP_Malloc( sizeof( *sun ));
				
				sun->style = si->lightStyle;
				Com_GetTokenAppend( script, shaderText, false, &token );
				sun->color[0] = com.atof( token.string );
				Com_GetTokenAppend( script, shaderText, false, &token );
				sun->color[1] = com.atof( token.string );
				Com_GetTokenAppend( script, shaderText, false, &token );
				sun->color[2] = com.atof( token.string );
				
				VectorNormalize( sun->color );
				
				// scale color by brightness
				Com_GetTokenAppend( script, shaderText, false, &token );
				sun->photons = com.atof( token.string );
				
				// get sun angle/elevation
				Com_GetTokenAppend( script, shaderText, false, &token );
				a = DEG2RAD( com.atof( token.string ));
				
				Com_GetTokenAppend( script, shaderText, false, &token );
				b = DEG2RAD( com.atof( token.string ));
				
				sun->direction[0] = cos( a ) * cos( b );
				sun->direction[1] = sin( a ) * cos( b );
				sun->direction[2] = sin( b );
				
				// get filter radius from shader
				sun->filterRadius = si->lightFilterRadius;
				
				// get sun angular deviance / samples
				if( ext && Com_ReadToken( script, false, &token ))
				{
					Com_SaveToken( script, &token );
					Com_GetTokenAppend( script, shaderText, false, &token );
					sun->deviance = DEG2RAD( com.atof( token.string ));
					
					Com_GetTokenAppend( script, shaderText, false, &token );
					sun->numSamples = com.atoi( token.string );
				}
				
				// store sun
				sun->next = si->sun;
				si->sun = sun;
				
				ApplySurfaceParm( "sky", &si->contents, &si->surfaceFlags );
				
				// don't process any more tokens on this line
				continue;
			}

			/* match q3map_ */
			else if( !strnicmp( token.string, "q3map_", 6 ))
			{
				// q3map_baseShader <shader> (inherit this shader's parameters)
				if( !com.stricmp( token.string, "q3map_baseShader" ) )
				{
					bsp_shader_t	*si2;
					
					// get shader
					Com_GetTokenAppend( script, shaderText, false, &token );
					si2 = FindShader( token.string );
				
					// subclass it
					if( si2 != NULL )
					{
						com.strcpy( temp, si->name );
						Mem_Copy( si, si2, sizeof( *si ));
						
						com.strcpy( si->name, temp );
						si->width = 0;
						si->height = 0;
						si->finished = false;
					}
				}
				
				// q3map_surfacemodel <path to model> <density> <min scale> <max scale> <min angle> <max angle> <oriented (0 or 1)>
				else if( !com.stricmp( token.string, "q3map_surfacemodel" ) )
				{
					surfmod_t	*model;
					
					model = BSP_Malloc( sizeof( *model ));
					model->next = si->surfaceModel;
					si->surfaceModel = model;
						
					Com_GetTokenAppend( script, shaderText, false, &token );
					com.strcpy( model->model, token.string );
					
					Com_GetTokenAppend( script, shaderText, false, &token );
					model->density = com.atof( token.string );
					Com_GetTokenAppend( script, shaderText, false, &token );
					model->odds = atof( token.string );
					
					Com_GetTokenAppend( script, shaderText, false, &token );
					model->minScale = com.atof( token.string );
					Com_GetTokenAppend( script, shaderText, false, &token );
					model->maxScale = com.atof( token.string );
					
					Com_GetTokenAppend( script, shaderText, false, &token );
					model->minAngle = com.atof( token.string );
					Com_GetTokenAppend( script, shaderText, false, &token );
					model->maxAngle = com.atof( token.string );
					
					Com_GetTokenAppend( script, shaderText, false, &token );
					model->oriented = (token.string[0] == '1' ? true : false);
				}
				
				// q3map_foliage <path to model> <scale> <density> <odds> <invert alpha (1 or 0)>
				else if( !com.stricmp( token.string, "q3map_foliage" ) )
				{
					foliage_t	*foliage;

					foliage = BSP_Malloc( sizeof( *foliage ));
					foliage->next = si->foliage;
					si->foliage = foliage;
					
					Com_GetTokenAppend( script, shaderText, false, &token );
					com.strcpy( foliage->model, token.string );
					
					Com_GetTokenAppend( script, shaderText, false, &token );
					foliage->scale = com.atof( token.string );
					Com_GetTokenAppend( script, shaderText, false, &token );
					foliage->density = com.atof( token.string );
					Com_GetTokenAppend( script, shaderText, false, &token );
					foliage->odds = com.atof( token.string );
					Com_GetTokenAppend( script, shaderText, false, &token );
					foliage->inverseAlpha = com.atoi( token.string );
				}
				
				// q3map_bounce <value> (fraction of light to re-emit during radiosity passes)
				else if( !com.stricmp( token.string, "q3map_bounce" ) || !com.stricmp( token.string, "q3map_bounceScale" ))
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->bounceScale = com.atof( token.string );
				}

				// q3map_skyLight <value> <iterations>
				else if( !com.stricmp( token.string, "q3map_skyLight" ))
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->skyLightValue = com.atof( token.string );
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->skyLightIterations = com.atoi( token.string );
					
					if( si->skyLightValue < 0.0f )
						si->skyLightValue = 0.0f;
					if( si->skyLightIterations < 2 )
						si->skyLightIterations = 2;
				}
				
				// q3map_surfacelight <value>
				else if( !com.stricmp( token.string, "q3map_surfacelight" ))
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->value = com.atof( token.string );
				}
				
				// q3map_lightStyle (sof2/jk2 lightstyle)
				else if( !com.stricmp( token.string, "q3map_lightStyle" ) )
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					val = com.atoi( token.string );
					if( val < 0 ) val = 0;
					else if( val > LS_NONE )
						val = LS_NONE;
					si->lightStyle = val;
				}
				
				// wolf/Xash3D: q3map_lightRGB <red> <green> <blue>
				else if( !com.stricmp( token.string, "q3map_lightRGB" ) || !com.stricmp( token.string, "q3map_radiosity" ))
				{
					float divider = 1.0f;

					// Xash3D using unsigned byte values 
					if(!com.stricmp( token.string, "q3map_radiosity" ))
						divider = 255.0f;      
										
					VectorClear( si->color );
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->color[0] = com.atof( token.string ) / divider;
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->color[1] = com.atof( token.string ) / divider;
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->color[2] = com.atof( token.string ) / divider;
					ColorNormalize( si->color, si->color );
				}
				
				// q3map_lightSubdivide <value>
				else if( !com.stricmp( token.string, "q3map_lightSubdivide" )  )
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->lightSubdivide = com.atoi( token.string );
				}
				
				// q3map_backsplash <percent> <distance>
				else if( !com.stricmp( token.string, "q3map_backsplash" ) )
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->backsplashFraction = com.atof( token.string ) * 0.01f;
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->backsplashDistance = com.atof( token.string );
				}
				
				// q3map_lightmapSampleSize <value>
				else if( !com.stricmp( token.string, "q3map_lightmapSampleSize" ) )
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->lightmapSampleSize = com.atoi( token.string );
				}
				
				// q3map_lightmapSampleSffset <value>
				else if( !com.stricmp( token.string, "q3map_lightmapSampleOffset" ) )
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->lightmapSampleOffset = com.atof( token.string );
				}
				
				// q3map_lightmapFilterRadius <self> <other>
				else if( !com.stricmp( token.string, "q3map_lightmapFilterRadius" ) )
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->lmFilterRadius = com.atof( token.string );
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->lightFilterRadius = com.atof( token.string );
				}
				
				// q3map_lightmapAxis [xyz]
				else if( !com.stricmp( token.string, "q3map_lightmapAxis" ))
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					if( !com.stricmp( token.string, "x" ) )
						VectorSet( si->lightmapAxis, 1, 0, 0 );
					else if( !com.stricmp( token.string, "y" ) )
						VectorSet( si->lightmapAxis, 0, 1, 0 );
					else if( !com.stricmp( token.string, "z" ) )
						VectorSet( si->lightmapAxis, 0, 0, 1 );
					else
					{
						MsgDev( D_WARN, "unknown value for lightmap axis: %s\n", token.string );
						VectorClear( si->lightmapAxis );
					}
				}
				
				// q3map_lightmapSize <width> <height> (for autogenerated shaders + external tga lightmaps)
				else if( !com.stricmp( token.string, "q3map_lightmapSize" ) )
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->lmCustomWidth = com.atoi( token.string );
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->lmCustomHeight = com.atoi( token.string );
					
					// must be a power of 2
					if(((si->lmCustomWidth - 1) & si->lmCustomWidth) || ((si->lmCustomHeight - 1) & si->lmCustomHeight) )
					{
						MsgDev( D_WARN, "Non power-of-two lightmap size specified (%dx%d)\n", si->lmCustomWidth, si->lmCustomHeight );
						si->lmCustomWidth = bsp_lightmap_size->integer;
						si->lmCustomHeight = bsp_lightmap_size->integer;
					}
				}

				// q3map_lightmapBrightness N (for autogenerated shaders + external tga lightmaps)
				else if( !com.stricmp( token.string, "q3map_lightmapBrightness" ) || !com.stricmp( token.string, "q3map_lightmapGamma" ))
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->lmBrightness = com.atof( token.string );
					if( si->lmBrightness < 0 ) si->lmBrightness = 1.0;
				}
				
				// q3map_vertexScale (scale vertex lighting by this fraction)
				else if( !com.stricmp( token.string, "q3map_vertexScale" ) )
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->vertexScale = com.atof( token.string );
				}
				
				// q3map_noVertexLight
				else if( !com.stricmp( token.string, "q3map_noVertexLight" )  )
				{
					si->noVertexLight = true;
				}
				
				// q3map_flare[Shader] <shader>
				else if( !com.stricmp( token.string, "q3map_flare" ) || !com.stricmp( token.string, "q3map_flareShader" ))
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					if( token.string[0] != '\0' ) com.strcpy( si->flareShader, token.string );
				}
				
				// q3map_backShader <shader>
				else if( !com.stricmp( token.string, "q3map_backShader" ))
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					if( token.string[0] != '\0' ) com.strcpy( si->backShader, token.string );
				}
				
				// q3map_cloneShader <shader>
				else if( !com.stricmp( token.string, "q3map_cloneShader" ) )
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					if( token.string[0] != '\0' ) com.strcpy( si->cloneShader, token.string );
				}
				
				// q3map_remapShader <shader>
				else if( !com.stricmp( token.string, "q3map_remapShader" ) )
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					if( token.string[0] != '\0' ) com.strcpy( si->remapShader, token.string );
				}
				
				// q3map_offset <value>
				else if( !com.stricmp( token.string, "q3map_offset" ) )
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->offset = com.atof( token.string );
				}
				
				// q3map_textureSize <width> <height> (substitute for q3map_lightimage derivation for terrain)
				else if( !com.stricmp( token.string, "q3map_fur" ) )
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->furNumLayers = com.atoi( token.string );
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->furOffset = com.atof( token.string );
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->furFade = com.atof( token.string );
				}
				
				// legacy support for terrain/terrain2 shaders
				else if( !com.stricmp( token.string, "q3map_terrain" ))
				{
					// team arena terrain is assumed to be nonplanar, with full normal averaging,
					// passed through the metatriangle surface pipeline, with a lightmap axis on z

					si->legacyTerrain = true;
					si->noClip = true;
					si->notjunc = true;
					si->indexed = true;
					si->nonplanar = true;
					si->forceMeta = true;
					si->shadeAngleDegrees = 179.0f;
				}
				
				// q3map_forceMeta (forces brush faces and/or triangle models to go through the metasurface pipeline)
				else if( !com.stricmp( token.string, "q3map_forceMeta" ))
				{
					si->forceMeta = true;
				}
				
				// q3map_shadeAngle <degrees>
				else if( !com.stricmp( token.string, "q3map_shadeAngle" ))
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->shadeAngleDegrees = com.atof( token.string );
				}
				
				// q3map_textureSize <width> <height> (substitute for q3map_lightimage derivation for terrain)
				else if( !com.stricmp( token.string, "q3map_textureSize" ) )
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->width = com.atoi( token.string );
					Com_GetTokenAppend( script, shaderText, false, &token );
					si->height = com.atoi( token.string );
				}
				
				// q3map_tcGen <style> <parameters>
				else if( !com.stricmp( token.string, "q3map_tcGen" ))
				{
					si->tcGen = true;
					Com_GetTokenAppend( script, shaderText, false, &token );
					
					// q3map_tcGen vector <s vector> <t vector>
					if( !com.stricmp( token.string, "vector" ) )
					{
						Com_Parse1DMatrixAppend( script, shaderText, 3, si->vecs[0] );
						Com_Parse1DMatrixAppend( script, shaderText, 3, si->vecs[1] );
					}
					
					// q3map_tcGen ivector <1.0/s vector> <1.0/t vector> (inverse vector, easier for mappers to understand)
					else if( !com.stricmp( token.string, "ivector" ) )
					{
						Com_Parse1DMatrixAppend( script, shaderText, 3, si->vecs[0] );
						Com_Parse1DMatrixAppend( script, shaderText, 3, si->vecs[1] );
						for( i = 0; i < 3; i++ )
						{
							si->vecs[0][i] = si->vecs[0][i] ? 1.0 / si->vecs[0][i] : 0;
							si->vecs[1][i] = si->vecs[1][i] ? 1.0 / si->vecs[1][i] : 0;
						}
					}
					else
					{
						MsgDev( D_WARN, "unknown q3map_tcGen method: %s\n", token.string );
						VectorClear( si->vecs[0] );
						VectorClear( si->vecs[1] );
					}
				}
				
				// q3map_[color|rgb|alpha][Gen|Mod] <style> <parameters>
				else if( !com.stricmp( token.string, "q3map_colorGen" ) || !com.stricmp( token.string, "q3map_colorMod" ) ||
					!com.stricmp( token.string, "q3map_rgbGen" ) || !com.stricmp( token.string, "q3map_rgbMod" ) ||
					!com.stricmp( token.string, "q3map_alphaGen" ) || !com.stricmp( token.string, "q3map_alphaMod" ))
				{
					cMod_t	*cm, *cm2;
					int	alpha;
					
					
					// alphamods are colormod + 1
					alpha = (!com.stricmp( token.string, "q3map_alphaGen" ) || !com.stricmp( token.string, "q3map_alphaMod" )) ? 1 : 0;
					
					cm = BSP_Malloc( sizeof( *cm ));
					
					if( si->colorMod == NULL ) si->colorMod = cm;
					else
					{
						for( cm2 = si->colorMod; cm2 != NULL; cm2 = cm2->next )
						{
							if( cm2->next == NULL )
							{
								cm2->next = cm;
								break;
							}
						}
					}
					
					Com_GetTokenAppend( script, shaderText, false, &token );
					
					// alpha set|const A
					if( alpha && (!com.stricmp( token.string, "set" ) || !com.stricmp( token.string, "const" )))
					{
						cm->type = CM_ALPHA_SET;
						Com_GetTokenAppend( script, shaderText, false, &token );
						cm->data[ 0 ] = com.atof( token.string );
					}
					
					// color|rgb set|const ( X Y Z )
					else if( !com.stricmp( token.string, "set" ) || !com.stricmp( token.string, "const" ))
					{
						cm->type = CM_COLOR_SET;
						Com_Parse1DMatrixAppend( script, shaderText, 3, cm->data );
					}
					
					// alpha scale A
					else if( alpha && !com.stricmp( token.string, "scale" ))
					{
						cm->type = CM_ALPHA_SCALE;
						Com_GetTokenAppend( script, shaderText, false, &token );
						cm->data[ 0 ] = atof( token.string );
					}
					
					// color|rgb scale ( X Y Z )
					else if( !com.stricmp( token.string, "scale" ))
					{
						cm->type = CM_COLOR_SCALE;
						Com_Parse1DMatrixAppend( script, shaderText, 3, cm->data );
					}
					
					// dotProduct ( X Y Z )
					else if( !com.stricmp( token.string, "dotProduct" ))
					{
						cm->type = CM_COLOR_DOT_PRODUCT + alpha;
						Com_Parse1DMatrixAppend( script, shaderText, 3, cm->data );
					}
					
					// dotProduct2 ( X Y Z )
					else if( !com.stricmp( token.string, "dotProduct2" ))
					{
						cm->type = CM_COLOR_DOT_PRODUCT_2 + alpha;
						Com_Parse1DMatrixAppend( script, shaderText, 3, cm->data );
					}
					
					// volume
					else if( !com.stricmp( token.string, "volume" ) )
					{
						// special stub mode for flagging volume brushes
						cm->type = CM_VOLUME;
					}
					else MsgDev( D_WARN, "unknown colorMod method: %s\n", token.string );
				}
				
				// q3map_tcMod <style> <parameters>
				else if( !com.stricmp( token.string, "q3map_tcMod" ))
				{
					float	a, b;
					
					Com_GetTokenAppend( script, shaderText, false, &token );
					
					// q3map_tcMod [translate | shift | offset] <s> <t>
					if( !com.stricmp( token.string, "translate" ) || !com.stricmp( token.string, "shift" ) || !com.stricmp( token.string, "offset" ))
					{
						Com_GetTokenAppend( script, shaderText, false, &token );
						a = com.atof( token.string );
						Com_GetTokenAppend( script, shaderText, false, &token );
						b = com.atof( token.string );
						TCModTranslate( si->mod, a, b );
					}

					// q3map_tcMod scale <s> <t>
					else if( !com.stricmp( token.string, "scale" ))
					{
						Com_GetTokenAppend( script, shaderText, false, &token );
						a = com.atof( token.string );
						Com_GetTokenAppend( script, shaderText, false, &token );
						b = com.atof( token.string );
						TCModScale( si->mod, a, b );
					}
					
					// q3map_tcMod rotate <s> <t> (fixme: make this communitive)
					else if( !com.stricmp( token.string, "rotate" ) )
					{
						Com_GetTokenAppend( script, shaderText, false, &token );
						a = com.atof( token.string );
						TCModRotate( si->mod, a );
					}
					else MsgDev( D_WARN, "unknown q3map_tcMod method: %s\n", token.string );
				}
				
				// q3map_fogDir (direction a fog shader fades from transparent to opaque)
				else if( !com.stricmp( token.string, "q3map_fogDir" ) )
				{
					Com_Parse1DMatrixAppend( script, shaderText, 3, si->fogDir );
					VectorNormalize( si->fogDir );
				}
				
				// q3map_globaltexture
				else if( !com.stricmp( token.string, "q3map_globaltexture" ))
					si->globalTexture = true;
				
				// q3map_nonplanar (make it a nonplanar merge candidate for meta surfaces)
				else if( !com.stricmp( token.string, "q3map_nonplanar" ))
					si->nonplanar = true;
				
				// q3map_noclip (preserve original face winding, don't clip by bsp tree)
				else if( !com.stricmp( token.string, "q3map_noclip" ))
					si->noClip = true;
				
				// q3map_notjunc
				else if( !com.stricmp( token.string, "q3map_notjunc" ))
					si->notjunc = true;
				
				// q3map_nofog
				else if( !com.stricmp( token.string, "q3map_nofog" ))
					si->noFog = true;
				
				// ydnar: gs mods: q3map_indexed (for explicit terrain-style indexed mapping)
				else if( !com.stricmp( token.string, "q3map_indexed" ) )
					si->indexed = true;
				
				// q3map_invert (inverts a drawsurface's facing)
				else if( !com.stricmp( token.string, "q3map_invert" ))
					si->invert = true;
				
				// q3map_lightmapMergable (ok to merge non-planar )
				else if( !com.stricmp( token.string, "q3map_lightmapMergable" ))
					si->lmMergable = true;
				
				// q3map_nofast
				else if( !com.stricmp( token.string, "q3map_noFast" ))
					si->noFast = true;
				
				// q3map_patchshadows
				else if( !com.stricmp( token.string, "q3map_patchShadows" ))
					si->patchShadows = true;
				
				// q3map_vertexshadows
				else if( !com.stricmp( token.string, "q3map_vertexShadows" ))
					si->vertexShadows = true;
				
				// q3map_novertexshadows
				else if( !com.stricmp( token.string, "q3map_noVertexShadows" ))
					si->vertexShadows = false;
				
				// q3map_splotchfix (filter dark lightmap luxels on lightmapped models)
				else if( !com.stricmp( token.string, "q3map_splotchfix" ))
					si->splotchFix = true;
				
				// q3map_forcesunlight
				else if( !com.stricmp( token.string, "q3map_forceSunlight" ))
					si->forceSunlight = true;
				
				// q3map_onlyvertexlighting
				else if( !com.stricmp( token.string, "q3map_onlyVertexLighting" ))
					ApplySurfaceParm( "pointlight", &si->contents, &si->surfaceFlags );
				
				// q3map_material
				else if( !com.stricmp( token.string, "q3map_material" ))
				{
					Com_GetTokenAppend( script, shaderText, false, &token );
					sprintf( temp, "*mat_%s", token.string );
					if(!ApplySurfaceParm( temp, &si->contents, &si->surfaceFlags ))
						MsgDev( D_WARN, "unknown material \"%s\"\n", token.string );
				}
				
				// q3map_clipmodel (autogenerate clip brushes for model triangles using this shader)
				else if( !com.stricmp( token.string, "q3map_clipmodel" ))
					si->clipModel = true;
				
				// q3map_styleMarker[2]
				else if( !com.stricmp( token.string, "q3map_styleMarker" ))
					si->styleMarker = 1;
				else if( !com.stricmp( token.string, "q3map_styleMarker2" ))// uses depthFunc equal
					si->styleMarker = 2;
				// default to searching for q3map_<surfaceparm>
				else ApplySurfaceParm( &token.string[6], &si->contents, &si->surfaceFlags );
			}

			// ignore all other token.strings on the line
			while( Com_ReadToken( script, SC_PARSE_GENERIC, &token ))
			{
				Com_SaveToken( script, &token );
				if( !Com_GetTokenAppend( script, shaderText, false, &token ))
					break;
			}
		}			
	}

	// release script when done
	if( script ) Com_CloseScript( script );
}

/*
===============
FindShader
===============
*/
bsp_shader_t *FindShader( const char *shaderName )
{
	string		shader;
	bsp_shader_t	*si;
	int		i;

	if( shaderName == NULL || shaderName[0] == '\0' )
	{
		MsgDev( D_WARN, "null or empty shader name\n" );
		shaderName = "missing";
	}

	// strip off extension
	com.strncpy( shader, shaderName, MAX_STRING );
	FS_StripExtension( shader );	// cut off extension
	com.strlwr( shader, shader );	// convert to lower case

	// look for it
	for( i = 0, si = shaderInfo; i < numShaderInfo; i++, si++ )
	{
		if( !com.stricmp( va("textures/%s", shader ), si->name ) || !com.stricmp( shader, si->name ))
		{
			// load image if necessary
			if( !si->finished )
			{
				LoadShaderImages( si );
				FinishShader( si );
			}
			return si;			
		}
	}

	// create a new one
	si = AllocShaderInfo();
	com.strncpy( si->name, shader, MAX_STRING );
	LoadShaderImages( si );
	FinishShader( si );

	return si;
}


/*
===============
LoadShaderInfo
===============
*/
int LoadShaderInfo( void )
{
	search_t	*search = FS_Search( "scripts/shaders/*.txt", true );
	int	i, numShaderFiles = 0;

	if( !search ) return 0;
	
	for( i = 0; i < search->numfilenames; i++ )
	{
		ParseShaderFile( search->filenames[i] );
		numShaderFiles++;
	}
	Mem_Free( search );

	return numShaderInfo;
}