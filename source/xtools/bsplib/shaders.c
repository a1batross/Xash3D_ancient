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

-------------------------------------------------------------------------------
*/

#define SHADERS_C

#include "q3map2.h"
#include "stdio.h"	// sscanf


/*
=================
ColorMod

routines for dealing with vertex color/alpha modification
=================
*/
void ColorMod( colorMod_t *cm, int numVerts, bspDrawVert_t *drawVerts )
{
	int		i, j, k;
	float		c;
	vec4_t		mult, add;
	bspDrawVert_t	*dv;
	colorMod_t	*cm2;
	
	if( cm == NULL || numVerts < 1 || drawVerts == NULL )
		return;
	
	for( i = 0; i < numVerts; i++ )
	{
		dv = &drawVerts[i];
		
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
				add[3] = cm2->data[0] * 255.0f;
				break;
			case CM_COLOR_SCALE:
				VectorCopy( cm2->data, mult );
				break;
			case CM_ALPHA_SCALE:
				mult[3] = cm2->data[0];
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
			default:	break;
			}
			
			// apply mod
			for( j = 0; j < MAX_LIGHTMAPS; j++ )
			{
				for( k = 0; k < 4; k++ )
				{
					c = (mult[k] * dv->color[j][k]) + add[k];
					dv->color[ j ][ k ] = bound( 0, c, 255 );
				}
			}
		}
	}
}

/*
=================
TCMod

routines for dealing with a 3x3 texture mod matrix
=================
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

	radians = euler / 180 * M_PI;
	sinv = sin( radians );
	cosv = cos( radians );

	temp[0][0] = cosv;
	temp[0][1] = -sinv;
	temp[1][0] = sinv;
	temp[1][1] = cosv;
	
	TCModMultiply( old, temp, mod );
}

/*
=================
ApplySurfaceParm

applies a named surfaceparm to the supplied flags
=================
*/
bool ApplySurfaceParm( char *name, int *contentFlags, int *surfaceFlags, int *compileFlags )
{
	int		i, fake;
	surfaceParm_t	*sp;
	
	if( name == NULL ) name = "";
	if( contentFlags == NULL ) contentFlags = &fake;
	if( surfaceFlags == NULL ) surfaceFlags = &fake;
	if( compileFlags == NULL ) compileFlags = &fake;
	
	// walk the current game's surfaceparms
	sp = game->surfaceParms;
	while( sp->name != NULL )
	{
		if( !com.stricmp( name, sp->name ))
		{
			*contentFlags &= ~(sp->contentFlagsClear);
			*contentFlags |= sp->contentFlags;
			*surfaceFlags &= ~(sp->surfaceFlagsClear);
			*surfaceFlags |= sp->surfaceFlags;
			*compileFlags &= ~(sp->compileFlagsClear);
			*compileFlags |= sp->compileFlags;
		
			return true;
		}
		sp++;
	}
	
	for( i = 0; i < numCustSurfaceParms; i++ )
	{
		sp = &custSurfaceParms[i];
		
		if( !com.stricmp( name, sp->name ))
		{
			*contentFlags &= ~(sp->contentFlagsClear);
			*contentFlags |= sp->contentFlags;
			*surfaceFlags &= ~(sp->surfaceFlagsClear);
			*surfaceFlags |= sp->surfaceFlags;
			*compileFlags &= ~(sp->compileFlagsClear);
			*compileFlags |= sp->compileFlags;
			
			return true;
		}
	}
	return false;
}

/*
=================
BeginMapShaderFile

erases and starts a new map shader script
=================
*/
void BeginMapShaderFile( const char *mapFile )
{
	mapName[0] = '\0';
	mapShaderFile[0] = '\0';

	if( mapFile == NULL || mapFile[0] == '\0' )
		return;
	
	FS_FileBase( mapFile, mapName );
	
	// append ../scripts/q3map2_<mapname>.shader
	com.sprintf( mapShaderFile, "scripts/q3map2_%s.shader", mapName );
	MsgDev( D_INFO, "Map has shader script %s\n", mapShaderFile );
	
	FS_Delete( mapShaderFile );
	
	// stop making warnings about missing images
	warnImage = false;
}

/*
=================
WriteMapShaderFile

writes a shader to the map shader script
=================
*/
void WriteMapShaderFile( void )
{
	file_t		*file;
	shaderInfo_t	*si;
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
	
	MsgDev( D_INFO, "--- WriteMapShaderFile ---\n");
	Msg( "Writing %s", mapShaderFile );
	
	file = FS_Open( mapShaderFile, "w" );
	if( file == NULL )
	{
		MsgDev( D_WARN, "Unable to open map shader file %s for writing\n", mapShaderFile );
		return;
	}
	
	FS_Printf( file, "// Custom shader file for %s.bsp\n" "// Generated by Xash BspLib\n" "// Do not edit! This file is overwritten on recompiles.\n\n", mapName );
	
	for( i = 0, num = 0; i < numShaderInfo; i++ )
	{
		si = &shaderInfo[i];
		if( si->custom == false || si->shaderText == NULL || si->shaderText[0] == '\0' )
			continue;
		FS_Printf( file, "%s%s\n", si->shader, si->shaderText );
		num++;
	}

	FS_Close( file );
	MsgDev( D_INFO, "\n%9d custom shaders emitted\n", num );
}



/*
=================
CustomShader

sets up a custom map shader
=================
*/
shaderInfo_t *CustomShader( shaderInfo_t *si, char *find, char *replace )
{
	shaderInfo_t	*csi;
	char		shader[ MAX_SHADERPATH ];
	char		*s;
	int		loc;
	MD5_CTX		md5_Hash;
	byte		digest[16];
	char		*srcShaderText;
	static char 	temp[16384], shaderText[16384];
	
	if( si == NULL ) return ShaderInfoForShader( "default" );
	
	srcShaderText = si->shaderText;
	
	if( si->implicitMap == IM_OPAQUE )
	{
		srcShaderText = temp;
		com.sprintf( temp, "\n"
			"{\t\t// defaulted (implicitMap)\n"
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
		com.sprintf( temp, "\n"
			"{\t\t// defaulted (implicitMask)\n"
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
		com.sprintf( temp, "\n"
			"{\t\t// defaulted (implicitBlend)\n"
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
		com.sprintf( temp, "\n"
			"{\t\t// defaulted\n"
			"\t{\n"
			"\t\tmap $lightmap\n"
			"\t\trgbGen identity\n"
			"\t}\n"
			"\tq3map_styleMarker\n"
			"\t{\n"
			"\t\tmap %s.tga\n"
			"\t\tblendFunc GL_DST_COLOR GL_ZERO\n"
			"\t\trgbGen identity\n"
			"\t}\n"
			"}\n",
			si->shader );
	}
	
	if(( com.strlen( mapName ) + 1 + 32) > MAX_SHADERPATH )
		Sys_Break( "Custom shader name length (%d) exceeded. Shorten your map name.\n", MAX_SHADERPATH );
	
	s = com.strstr( srcShaderText, find );
	if( s == NULL ) return si; // testing just using the existing shader if this fails
	else
	{
		loc = s - srcShaderText;
		com.strncpy( shaderText, srcShaderText, sizeof( shaderText ));
		shaderText[loc] = '\0';
		com.strcat( shaderText, replace );
		com.strcat( shaderText, &srcShaderText[loc + com.strlen( find )] );
	}
	
	// make md5 hash of the shader text
	MD5_Init( &md5_Hash );
	MD5_Update( &md5_Hash, shaderText, com.strlen( shaderText ));
	MD5_Final( &md5_Hash, digest );
	
	// mangle hash into a shader name
	com.sprintf( shader, "%s/%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", mapName,
		digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7], 
		digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15] );
	
	csi = ShaderInfoForShader( shader );
	
	if( csi->custom ) return csi;
	
	// clone the existing shader and rename
	Mem_Copy( csi, si, sizeof( shaderInfo_t ));
	com.strcpy( csi->shader, shader );
	csi->custom = true;
	csi->shaderText = copystring( shaderText );
	
	return csi;
}

/*
=================
EmitVertexRemapShader

adds a vertexremapshader key/value pair to worldspawn
=================
*/
void EmitVertexRemapShader( char *from, char *to )
{
	MD5_CTX		md5_Hash;
	byte		digest[16];
	char		key[64], value[256];
	
	if( from == NULL || from[0] == '\0' || to == NULL || to[0] == '\0' )
		return;
	
	com.sprintf( value, "%s;%s", from, to );
	
	MD5_Init( &md5_Hash );
	MD5_Update( &md5_Hash, value, com.strlen( value ));
	MD5_Final( &md5_Hash, digest );

	// make key (this is annoying, as vertexremapshader is precisely 17 characters,
	// which is one too long, so we leave off the last byte of the md5 digest)
	com.sprintf( key, "vertexremapshader%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
		digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7], 
		digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14] ); // no: digest[15]
	
	// add key/value pair to worldspawn
	SetKeyValue( &entities[0], key, value );
}



/*
=================
AllocShaderInfo

allocates and initializes a new shader
=================
*/
static shaderInfo_t	*AllocShaderInfo( void )
{
	shaderInfo_t	*si;
	
	if( shaderInfo == NULL )
	{
		shaderInfo = Malloc( sizeof( shaderInfo_t ) * MAX_SHADER_INFO );
		numShaderInfo = 0;
	}
	
	if( numShaderInfo == MAX_SHADER_INFO )
		Sys_Break( "MAX_SHADER_INFO exceeded\n Remove some PK3 files or shader scripts from shaderlist.txt and try again\n" );
	si = &shaderInfo[ numShaderInfo ];
	numShaderInfo++;
	
	Mem_Set( si, 0, sizeof( shaderInfo_t ));
	
	ApplySurfaceParm( "default", &si->contentFlags, &si->surfaceFlags, &si->compileFlags );
	
	si->backsplashFraction = DEF_BACKSPLASH_FRACTION;
	si->backsplashDistance = DEF_BACKSPLASH_DISTANCE;
	si->bounceScale = DEF_RADIOSITY_BOUNCE;
	si->lightStyle = LS_NORMAL;
	si->polygonOffset = false;
	si->shadeAngleDegrees = 0.0f;
	si->lightmapSampleSize = 0;
	si->lightmapSampleOffset = DEFAULT_LIGHTMAP_SAMPLE_OFFSET;
	si->patchShadows = false;
	si->vertexShadows = true;
	si->forceSunlight = false;
	si->vertexScale = 1.0;
	si->notjunc = false;
	
	// set texture coordinate transform matrix to identity
	TCModIdentity( si->mod );
	
	// lightmaps can now be > 128x128 in certain games or an externally generated tga
	si->lmCustomWidth = lmCustomSize;
	si->lmCustomHeight = lmCustomSize;
	
	return si;
}

/*
=================
FinishShader

sets a shader's width and height among other things
=================
*/
void FinishShader( shaderInfo_t *si )
{
	int	x, y;
	float	st[2], o[2], dist, bestDist;
	vec4_t	color, bestColor, delta;
	
	if( si->finished )
		return;
	
	// if they're explicitly set, copy from image size
	if( si->shaderWidth == 0 && si->shaderHeight == 0 )
	{
		si->shaderWidth = si->shaderImage->pic->width;
		si->shaderHeight = si->shaderImage->pic->height;
	}
	
	// legacy terrain has explicit image-sized texture projection
	if( si->legacyTerrain && si->tcGen == false )
	{
		// set xy texture projection
		si->tcGen = true;
		VectorSet( si->vecs[0], (1.0f / (si->shaderWidth * 0.5f )), 0.0f, 0.0f );
		VectorSet( si->vecs[1], 0.0f, (1.0f / (si->shaderHeight * 0.5f )), 0.0f );
	}
	
	// find pixel coordinates best matching the average color of the image
	bestDist = 99999999;
	o[0] = 1.0f / si->shaderImage->pic->width;
	o[1] = 1.0f / si->shaderImage->pic->height;
	for( y = 0, st[1] = 0.0f; y < si->shaderImage->pic->height; y++, st[1] += o[1] )
	{
		for( x = 0, st[0] = 0.0f; x < si->shaderImage->pic->width; x++, st[0] += o[0] )
		{
			RadSampleImage( si->shaderImage->pic, st, color );
			
			VectorSubtract( color, si->averageColor, delta );
			delta[3] = color[3] - si->averageColor[3];
			dist = DotProduct( delta, delta );
			if( dist < bestDist )
			{
				VectorCopy( color, bestColor );
				bestColor[3] = color[3];
				si->stFlat[0] = st[0];
				si->stFlat[1] = st[1];
			}
		}
	}
	si->finished = true;
}

/*
=================
LoadShaderImages

loads a shader's images
=================
*/
static void LoadShaderImages( shaderInfo_t *si )
{
	int	i, count;
	float	color[4];
	
	// nodraw shaders don't need images
	if( si->compileFlags & C_NODRAW )
	{
		si->shaderImage = ImageLoad( DEFAULT_IMAGE );
	}
	else
	{
		// try to load editor image first
		si->shaderImage = ImageLoad( si->editorImagePath );
		
		// then try shadername
		if( si->shaderImage == NULL ) si->shaderImage = ImageLoad( si->shader );
		
		// then try implicit image path (note: new behavior!)
		if( si->shaderImage == NULL ) si->shaderImage = ImageLoad( si->implicitImagePath );
		
		// then try lightimage (note: new behavior!)
		if( si->shaderImage == NULL ) si->shaderImage = ImageLoad( si->lightImagePath );
		
		// otherwise, use default image
		if( si->shaderImage == NULL )
		{
			si->shaderImage = ImageLoad( DEFAULT_IMAGE );
			if( warnImage && com.strcmp( si->shader, MAP_DEFAULT_SHADER ))
				MsgDev( D_WARN, "Couldn't find image for shader %s\n", si->shader );
		}
		
		si->lightImage = ImageLoad( si->lightImagePath );
		
		// load normalmap image (ok if this is NULL)
		si->normalImage = ImageLoad( si->normalImagePath );
		if( si->normalImage != NULL )
		{
			MsgDev( D_INFO, "Shader %s has\n" "    NM %s\n", si->shader, si->normalImagePath );
		}
	}
	
	// if no light image, use shader image
	if( si->lightImage == NULL ) si->lightImage = ImageLoad( si->shaderImage->name );
	
	// create default and average colors
	count = si->lightImage->pic->width * si->lightImage->pic->height;
	VectorClear( color );
	color[3] = 0.0f;

	Image_Process( &si->lightImage->pic, 0, 0, IMAGE_FORCE_RGBA ); // paranoid mode

	for( i = 0; i < count; i++ )
	{
		color[0] += si->lightImage->pic->buffer[i * 4 + 0];
		color[1] += si->lightImage->pic->buffer[i * 4 + 1];
		color[2] += si->lightImage->pic->buffer[i * 4 + 2];
		color[3] += si->lightImage->pic->buffer[i * 4 + 3];
	}
	
	if( VectorLength( si->color ) <= 0.0f )
		ColorNormalize( color, si->color );
	VectorScale( color, (1.0f / count), si->averageColor );
}

/*
=================
ShaderInfoForShader

finds a shaderinfo for a named shader
=================
*/
shaderInfo_t *ShaderInfoForShader( const char *shaderName )
{
	int		i;
	shaderInfo_t	*si;
	char		shader[MAX_SHADERPATH];
	
	if( shaderName == NULL || shaderName[0] == '\0' )
	{
		MsgDev( D_WARN, "Null or empty shader name\n" );
		shaderName = "missing";
	}
	
	com.strcpy( shader, shaderName );
	FS_StripExtension( shader );
	
	for( i = 0; i < numShaderInfo; i++ )
	{
		si = &shaderInfo[i];
		if( !com.stricmp( shader, si->shader ) )
		{
			if( si->finished == false )
			{
				LoadShaderImages( si );
				FinishShader( si );
			}
			return si;
		}
	}
	
	// allocate a default shader
	si = AllocShaderInfo();
	com.strcpy( si->shader, shader );
	LoadShaderImages( si );
	FinishShader( si );
	
	return si;
}

/*
=================
GetTokenAppend

gets a token and appends its text to the specified buffer
=================
*/

static int oldScriptLine = 0;
static int tabDepth = 0;

bool GetTokenAppend( script_t *script, token_t *tok, char *buffer, bool crossline )
{
	int	i, r;
	
	r = Com_ReadToken( script, crossline|SC_PARSE_GENERIC, tok );
	if( r == false || buffer == NULL || tok->string[0] == '\0' )
		return r;
	
	// pre-tabstops
	if( !com.strcmp( tok->string, "}" ))
		tabDepth--;
	
	// append?
	if( oldScriptLine != tok->line )
	{
		com.strcat( buffer, "\n" );
		for( i = 0; i < tabDepth; i++ )
			com.strcat( buffer, "\t" );
	}
	else com.strcat( buffer, " " );

	oldScriptLine = tok->line;
	com.strcat( buffer, tok->string );
	
	// post-tabstops
	if(!com.strcmp( tok->string, "{" )) tabDepth++;
	
	return r;
}

void Parse1DMatrixAppend( script_t *script, char *buffer, int x, vec_t *m )
{
	int	i;
	token_t	token;
	
	if(!GetTokenAppend( script, &token, buffer, true ) || com.strcmp( token.string, "(" ))
		Sys_Break( "Parse1DMatrixAppend(): line %d: ( not found!", token.line );
	for( i = 0; i < x; i++ )
	{
		if( !GetTokenAppend( script, &token, buffer, false ))
			Sys_Break( "Parse1DMatrixAppend(): line %d: Number not found!", token.line );
		m[i] = com.atof( token.string );
	}
	if( !GetTokenAppend( script, &token, buffer, true ) || com.strcmp( token.string, ")" ))
		Sys_Break( "Parse1DMatrixAppend(): line %d: ) not found!", token.line );
}

/*
=================
ParseShaderFile

parses a shader file into discrete shaderInfo_t
=================
*/
static void ParseShaderFile( const char *filename )
{
	int		i, val;
	shaderInfo_t	*si = NULL;
	char		*suffix, temp[MAX_SYSPATH];
	static char	shaderText[16384];
	script_t		*script;
	token_t		token;
	
	shaderText[0] = '\0';
	
	script = Com_OpenScript( filename, NULL, 0 );
	if( !script ) return;
	
	while( 1 )
	{
		// copy shader text to the shaderinfo
		if( si != NULL && shaderText[0] != '\0' )
		{
			com.strcat( shaderText, "\n" );
			si->shaderText = copystring( shaderText );
		}
		
		shaderText[0] = '\0';	// this will be done only once for each shader
		
		// test for end of file
		if(!Com_ReadToken( script, SC_ALLOW_NEWLINES|SC_ALLOW_PATHNAMES2, &token ))
			break;
		
		// shader name is initial token
		si = AllocShaderInfo();
		com.strcpy( si->shader, token.string );
		
		// ignore ":q3map" suffix
		suffix = com.strstr( si->shader, ":q3map" );
		if( suffix != NULL ) *suffix = '\0';
		
		// handle { } section
		if( !GetTokenAppend( script, &token, shaderText, true ))
			break;
		if( com.strcmp( token.string, "{" ) )
		{
			if( si != NULL ) Sys_Break( "ParseShaderFile(): %s, line %d: { not found!\n" 
			"found instead: %s\nLast known shader: %s", filename, token.line, token.string, si->shader );
			else Sys_Break( "ParseShaderFile(): %s, line %d: { not found!\nFound instead: %s", filename, token.line, token.string );
		}
		
		while( 1 )
		{
			// get the next token
			if( !GetTokenAppend( script, &token, shaderText, true ))
				break;
			if( !com.strcmp( token.string, "}" ))
				break;
			
			
			/*
			-----------------------------------------------------------------
			   shader stages (passes)
			-----------------------------------------------------------------
			*/
			
			// parse stage directives
			if( !strcmp( token.string, "{" ))
			{
				si->hasPasses = true;
				while( 1 )
				{
					if( !GetTokenAppend( script, &token, shaderText, true ))
						break;
					if( !com.strcmp( token.string, "}" ))
						break;
					
					// only care about images if we don't have a editor/light image
					if( si->editorImagePath[0] == '\0' && si->lightImagePath[0] == '\0' && si->implicitImagePath[0] == '\0' )
					{
						// digest any images
						if( !com.stricmp( token.string, "map" ) || !com.stricmp( token.string, "clampMap" ) || !com.stricmp( token.string, "animMap" )
						|| !com.stricmp( token.string, "clampAnimMap" ) || !com.stricmp( token.string, "clampMap" )
						|| !com.stricmp( token.string, "mapComp" ) || !com.stricmp( token.string, "mapNoComp" ))
						{
							// skip one token for animated stages
							if( !com.stricmp( token.string, "animMap" ) || !com.stricmp( token.string, "clampAnimMap" ) )
								GetTokenAppend( script, &token, shaderText, false );
							
							// get an image
							GetTokenAppend( script, &token, shaderText, false );
							if( token.string[0] != '*' && token.string[0] != '$' )
							{
								com.strcpy( si->lightImagePath, token.string );
								FS_DefaultExtension( si->lightImagePath, ".tga" );
							}
						}
					}
				}
			}
			
			/*
			-----------------------------------------------------------------
			surfaceparm * directives
			-----------------------------------------------------------------
			*/
			
			// match surfaceparm
			else if( !com.stricmp( token.string, "surfaceparm" ))
			{
				GetTokenAppend( script, &token, shaderText, false );
				if( !ApplySurfaceParm( token.string, &si->contentFlags, &si->surfaceFlags, &si->compileFlags ))
					MsgDev( D_WARN, "unknown surfaceparm: \"%s\"\n", token.string );
			}
			
			
			/*
			-----------------------------------------------------------------
			game-related shader directives
			-----------------------------------------------------------------
			*/
			
			// fogparms (for determining fog volumes)
			else if( !com.stricmp( token.string, "fogparms" ))
				si->fogParms = true;
			
			// polygonoffset (for no culling)
			else if( !com.stricmp( token.string, "polygonoffset" ))
				si->polygonOffset = true;
			
			// tesssize is used to force liquid surfaces to subdivide
			else if( !com.stricmp( token.string, "tessSize" ) || !com.stricmp( token.string, "q3map_tessSize" ))
			{
				GetTokenAppend( script, &token, shaderText, false );
				si->subdivisions = com.atof( token.string );
			}
			
			// cull none will set twoSided
			else if( !com.stricmp( token.string, "cull" ))
			{
				GetTokenAppend( script, &token, shaderText, false );
				if( !com.stricmp( token.string, "none" ) || !com.stricmp( token.string, "disable" ) || !com.stricmp( token.string, "twosided" ))
					si->twoSided = true;
			}
			
			// deformVertexes autosprite[2] we catch this so autosprited surfaces become point
			// lights instead of area lights
			else if( !com.stricmp( token.string, "deformVertexes" ))
			{
				GetTokenAppend( script, &token, shaderText, false );
				
				// deformVertexes autosprite(2)
				if( !com.strnicmp( token.string, "autosprite", 10 ))
				{
					// set it as autosprite and detail
					si->autosprite = true;
					ApplySurfaceParm( "detail", &si->contentFlags, &si->surfaceFlags, &si->compileFlags );
					
					// added these useful things
					si->noClip = true;
					si->notjunc = true;
				}
				
				// deformVertexes move <x> <y> <z> <func> <base> <amplitude> <phase> <freq> (for particle studio support)
				if( !com.stricmp( token.string, "move" ))
				{
					vec3_t	amt, mins, maxs;
					float	base, amp;
					
					GetTokenAppend( script, &token, shaderText, false );
					amt[0] = com.atof( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					amt[1] = com.atof( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					amt[2] = com.atof( token.string );
					
					GetTokenAppend( script, &token, shaderText, false );
					
					GetTokenAppend( script, &token, shaderText, false );
					base = com.atof( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					amp = com.atof( token.string );
					
					VectorScale( amt, base, mins );
					VectorMA( mins, amp, amt, maxs );
					VectorAdd( si->mins, mins, si->mins );
					VectorAdd( si->maxs, maxs, si->maxs );
				} 
			}
			
			// light <value> (old-style flare specification)
			else if( !com.stricmp( token.string, "light" ))
			{
				GetTokenAppend( script, &token, shaderText, false );
				si->flareShader = game->flareShader;
			}
			
			// damageShader <shader> <health> (sof2 mods)
			else if( !com.stricmp( token.string, "damageShader" ))
			{
				GetTokenAppend( script, &token, shaderText, false );
				if( token.string[0] != '\0' )
				{
					si->damageShader = Malloc( strlen( token.string ) + 1 );
					strcpy( si->damageShader, token.string );
				}
				GetTokenAppend( script, &token, shaderText, false ); // don't do anything with health
			}
			
			// enemy territory implicit shaders
			else if( !com.stricmp( token.string, "implicitMap" ))
			{
				si->implicitMap = IM_OPAQUE;
				GetTokenAppend( script, &token, shaderText, false );
				if( token.string[0] == '-' && token.string[1] == '\0' )
					com.sprintf( si->implicitImagePath, "%s.tga", si->shader );
				else com.strcpy( si->implicitImagePath, token.string );
			}
			else if( !com.stricmp( token.string, "implicitMask" ))
			{
				si->implicitMap = IM_MASKED;
				GetTokenAppend( script, &token, shaderText, false );
				if( token.string[0] == '-' && token.string[1] == '\0' )
					com.sprintf( si->implicitImagePath, "%s.tga", si->shader );
				else com.strcpy( si->implicitImagePath, token.string );
			}
			else if( !com.stricmp( token.string, "implicitBlend" ))
			{
				si->implicitMap = IM_MASKED;
				GetTokenAppend( script, &token, shaderText, false );
				if( token.string[0] == '-' && token.string[1] == '\0' )
					com.sprintf( si->implicitImagePath, "%s.tga", si->shader );
				else com.strcpy( si->implicitImagePath, token.string );
			}
			
			/*
			-----------------------------------------------------------------
			image directives
			-----------------------------------------------------------------
			*/
			
			// qer_editorimage <image>
			else if( !com.stricmp( token.string, "qer_editorImage" ))
			{
				GetTokenAppend( script, &token, shaderText, false );
				strcpy( si->editorImagePath, token.string );
				FS_DefaultExtension( si->editorImagePath, ".tga" );
			}
			
			// q3map_normalimage <image> (bumpmapping normal map)
			else if( !com.stricmp( token.string, "q3map_normalImage" ))
			{
				GetTokenAppend( script, &token, shaderText, false );
				strcpy( si->normalImagePath, token.string );
				FS_DefaultExtension( si->normalImagePath, ".tga" );
			}
			
			// q3map_lightimage <image>
			else if( !com.stricmp( token.string, "q3map_lightImage" ))
			{
				GetTokenAppend( script, &token, shaderText, false );
				strcpy( si->lightImagePath, token.string );
				FS_DefaultExtension( si->lightImagePath, ".tga" );
			}
			
			// skyparms <outer image> <cloud height> <inner image>
			else if( !com.stricmp( token.string, "skyParms" ) )
			{
				GetTokenAppend( script, &token, shaderText, false );
				
				if( com.stricmp( token.string, "-" ) && com.stricmp( token.string, "full" ))
				{
					com.strcpy( si->skyParmsImageBase, token.string );
					
					// use top image as sky light image
					if( si->lightImagePath[0] == '\0' )
						com.sprintf( si->lightImagePath, "%sup.tga", si->skyParmsImageBase );
				}
				
				// skip rest of line
				GetTokenAppend( script, &token, shaderText, false );
				GetTokenAppend( script, &token, shaderText, false );
			}
			
			/*
			-----------------------------------------------------------------
			q3map_* directives
			-----------------------------------------------------------------
			*/
			
			// q3map_sun <red> <green> <blue> <intensity> <degrees> <elevation>
			// color will be normalized, so it doesn't matter what range you use
			// intensity falls off with angle but not distance 100 is a fairly bright sun
			// degree of 0 = from the east, 90 = north, etc.  altitude of 0 = sunrise/set, 90 = noon
			else if( !com.stricmp( token.string, "q3map_sun" ) || !com.stricmp( token.string, "q3map_sunExt" ))
			{
				float	a, b;
				sun_t	*sun;
				bool	ext;
				
				// extended sun directive?
				if( !com.stricmp( token.string, "q3map_sunext" ))
					ext = true;
				
				sun = Malloc( sizeof( *sun ) );
				sun->style = si->lightStyle;
				
				GetTokenAppend( script, &token, shaderText, false );
				sun->color[0] = com.atof( token.string );
				GetTokenAppend( script, &token, shaderText, false );
				sun->color[1] = com.atof( token.string );
				GetTokenAppend( script, &token, shaderText, false );
				sun->color[2] = com.atof( token.string );
				
				VectorNormalize( sun->color );
				
				GetTokenAppend( script, &token, shaderText, false );
				sun->photons = com.atof( token.string );
				
				// get sun angle/elevation
				GetTokenAppend( script, &token, shaderText, false );
				a = com.atof( token.string );
				a = a / 180.0f * M_PI;
				
				GetTokenAppend( script, &token, shaderText, false );
				b = com.atof( token.string );
				b = b / 180.0f * M_PI;
				
				sun->direction[0] = com.cos( a ) * com.cos( b );
				sun->direction[1] = com.sin( a ) * com.cos( b );
				sun->direction[2] = com.sin( b );
				
				// get filter radius from shader
				sun->filterRadius = si->lightFilterRadius;
				
				// get sun angular deviance/samples
				if( ext && GetTokenAppend( script, &token, shaderText, false ))
				{
					sun->deviance = com.atof( token.string );
					sun->deviance = sun->deviance / 180.0f * M_PI;
					
					GetTokenAppend( script, &token, shaderText, false );
					sun->numSamples = com.atoi( token.string );
				}
				
				sun->next = si->sun;
				si->sun = sun;
				
				// apply sky surfaceparm
				ApplySurfaceParm( "sky", &si->contentFlags, &si->surfaceFlags, &si->compileFlags );
				
				// don't process any more tokens on this line
				continue;
			}

			// match q3map_
			else if( !com.strnicmp( token.string, "q3map_", 6 ))
			{
				// q3map_baseShader <shader> (inherit this shader's parameters)
				if( !com.stricmp( token.string, "q3map_baseShader" ))
				{
					shaderInfo_t	*si2;
					bool		oldWarnImage;
					
					GetTokenAppend( script, &token, shaderText, false );
					oldWarnImage = warnImage;
					warnImage = false;
					si2 = ShaderInfoForShader( token.string );
					warnImage = oldWarnImage;
					
					// subclass it
					if( si2 != NULL )
					{
						com.strcpy( temp, si->shader );
						Mem_Copy( si, si2, sizeof( *si ));
						
						// restore name and set to unfinished
						com.strcpy( si->shader, temp );
						si->shaderWidth = 0;
						si->shaderHeight = 0;
						si->finished = false;
					}
				}
				
				// q3map_surfacemodel <path to model> <density> <min scale> <max scale> <min angle> <max angle> <oriented (0 or 1)>
				else if( !com.stricmp( token.string, "q3map_surfacemodel" ))
				{
					surfaceModel_t	*model;

					// allocate new model and attach it
					model = Malloc( sizeof( *model ));
					model->next = si->surfaceModel;
					si->surfaceModel = model;
						
					GetTokenAppend( script, &token, shaderText, false );
					com.strcpy( model->model, token.string );
					
					GetTokenAppend( script, &token, shaderText, false );
					model->density = com.atof( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					model->odds = com.atof( token.string );
					
					GetTokenAppend( script, &token, shaderText, false );
					model->minScale = com.atof( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					model->maxScale = com.atof( token.string );
					
					GetTokenAppend( script, &token, shaderText, false );
					model->minAngle = com.atof( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					model->maxAngle = com.atof( token.string );
					
					GetTokenAppend( script, &token, shaderText, false );
					model->oriented = (token.string[0] == '1' ? true : false);
				}
				
				// q3map_foliage <path to model> <scale> <density> <odds> <invert alpha (1 or 0)>
				else if( !com.stricmp( token.string, "q3map_foliage" ))
				{
					foliage_t	*foliage;
					
					// allocate new foliage struct and attach it
					foliage = Malloc( sizeof( *foliage ) );
					foliage->next = si->foliage;
					si->foliage = foliage;
					
					// get parameters
					GetTokenAppend( script, &token, shaderText, false );
					com.strcpy( foliage->model, token.string );
					
					GetTokenAppend( script, &token, shaderText, false );
					foliage->scale = com.atof( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					foliage->density = com.atof( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					foliage->odds = com.atof( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					foliage->inverseAlpha = com.atoi( token.string );
				}
				
				// q3map_bounce <value> (fraction of light to re-emit during radiosity passes)
				else if( !com.stricmp( token.string, "q3map_bounce" ) || !com.stricmp( token.string, "q3map_bounceScale" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					si->bounceScale = com.atof( token.string );
				}

				// q3map_skyLight <value> <iterations>
				else if( !com.stricmp( token.string, "q3map_skyLight" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					si->skyLightValue = com.atof( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					si->skyLightIterations = com.atoi( token.string );
					
					/* clamp */
					if( si->skyLightValue < 0.0f ) si->skyLightValue = 0.0f;
					if( si->skyLightIterations < 2 ) si->skyLightIterations = 2;
				}
				
				// q3map_surfacelight <value>
				else if( !com.stricmp( token.string, "q3map_surfacelight" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					si->value = com.atof( token.string );
				}
				
				// q3map_lightStyle (sof2/jk2 lightstyle)
				else if( !com.stricmp( token.string, "q3map_lightStyle" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					val = com.atoi( token.string );
					si->lightStyle = bound( 0, val, LS_NONE );
				}
				
				// wolf: q3map_lightRGB <red> <green> <blue>
				else if( !com.stricmp( token.string, "q3map_lightRGB" ))
				{
					VectorClear( si->color );
					GetTokenAppend( script, &token, shaderText, false );
					si->color[0] = com.atof( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					si->color[1] = com.atof( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					si->color[2] = com.atof( token.string );
					ColorNormalize( si->color, si->color );
				}
				
				// q3map_lightSubdivide <value>
				else if( !com.stricmp( token.string, "q3map_lightSubdivide" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					si->lightSubdivide = atoi( token.string );
				}
				
				// q3map_backsplash <percent> <distance>
				else if( !com.stricmp( token.string, "q3map_backsplash" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					si->backsplashFraction = com.atof( token.string ) * 0.01f;
					GetTokenAppend( script, &token, shaderText, false );
					si->backsplashDistance = com.atof( token.string );
				}
				
				// q3map_lightmapSampleSize <value>
				else if( !com.stricmp( token.string, "q3map_lightmapSampleSize" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					si->lightmapSampleSize = com.atoi( token.string );
				}
				
				// q3map_lightmapSampleSffset <value>
				else if( !com.stricmp( token.string, "q3map_lightmapSampleOffset" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					si->lightmapSampleOffset = com.atof( token.string );
				}
				
				// q3map_lightmapFilterRadius <self> <other>
				else if( !com.stricmp( token.string, "q3map_lightmapFilterRadius" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					si->lmFilterRadius = com.atof( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					si->lightFilterRadius = com.atof( token.string );
				}
				
				// q3map_lightmapAxis [xyz]
				else if( !com.stricmp( token.string, "q3map_lightmapAxis" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					if( !com.stricmp( token.string, "x" )) VectorSet( si->lightmapAxis, 1, 0, 0 );
					else if( !com.stricmp( token.string, "y" )) VectorSet( si->lightmapAxis, 0, 1, 0 );
					else if( !com.stricmp( token.string, "z" )) VectorSet( si->lightmapAxis, 0, 0, 1 );
					else
					{
						MsgDev( D_WARN, "Unknown value for lightmap axis: %s\n", token.string );
						VectorClear( si->lightmapAxis );
					}
				}
				
				// q3map_lightmapSize <width> <height> (for autogenerated shaders + external tga lightmaps)
				else if( !com.stricmp( token.string, "q3map_lightmapSize" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					si->lmCustomWidth = com.atoi( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					si->lmCustomHeight = com.atoi( token.string );
					
					// must be a power of 2
					if(((si->lmCustomWidth - 1) & si->lmCustomWidth) || ((si->lmCustomHeight - 1) & si->lmCustomHeight))
					{
						MsgDev( D_WARN, "Non power-of-two lightmap size specified (%d, %d)\n", si->lmCustomWidth, si->lmCustomHeight );
						si->lmCustomWidth = lmCustomSize;
						si->lmCustomHeight = lmCustomSize;
					}
				}

				// q3map_lightmapBrightness N (for autogenerated shaders + external tga lightmaps)
				else if( !com.stricmp( token.string, "q3map_lightmapBrightness" ) || !com.stricmp( token.string, "q3map_lightmapGamma" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					si->lmBrightness = com.atof( token.string );
					if( si->lmBrightness < 0 ) si->lmBrightness = 1.0f;
				}
				
				// q3map_vertexScale (scale vertex lighting by this fraction)
				else if( !com.stricmp( token.string, "q3map_vertexScale" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					si->vertexScale = com.atof( token.string );
				}
				
				// q3map_noVertexLight
				else if( !com.stricmp( token.string, "q3map_noVertexLight" ))
				{
					si->noVertexLight = true;
				}
				
				// q3map_flare[Shader] <shader>
				else if( !com.stricmp( token.string, "q3map_flare" ) || !com.stricmp( token.string, "q3map_flareShader" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					if( token.string[0] != '\0' ) si->flareShader = copystring( token.string );
				}
				
				// q3map_backShader <shader>
				else if( !com.stricmp( token.string, "q3map_backShader" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					if( token.string[0] != '\0' ) si->backShader = copystring( token.string );
				}
				
				// q3map_cloneShader <shader>
				else if ( !com.stricmp( token.string, "q3map_cloneShader" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					if( token.string[0] != '\0' ) si->cloneShader = copystring( token.string );
				}
				
				// q3map_remapShader <shader>
				else if( !com.stricmp( token.string, "q3map_remapShader" ) )
				{
					GetTokenAppend( script, &token, shaderText, false );
					if( token.string[0] != '\0' ) si->remapShader = copystring( token.string );
				}
				
				// q3map_offset <value>
				else if( !com.stricmp( token.string, "q3map_offset" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					si->offset = com.atof( token.string );
				}
				
				// q3map_fur <numLayers> <offset>
				else if( !com.stricmp( token.string, "q3map_fur" ) )
				{
					GetTokenAppend( script, &token, shaderText, false );
					si->furNumLayers = com.atoi( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					si->furOffset = com.atof( token.string );
					GetTokenAppend( script, &token, shaderText, false );
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
				
				// picomodel: q3map_forceMeta (forces brush faces and/or triangle models to go through the metasurface pipeline)
				else if( !com.stricmp( token.string, "q3map_forceMeta" ))
				{
					si->forceMeta = true;
				}
				
				// q3map_shadeAngle <degrees>
				else if( !com.stricmp( token.string, "q3map_shadeAngle" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					si->shadeAngleDegrees = com.atof( token.string );
				}
				
				// q3map_textureSize <width> <height> (substitute for q3map_lightimage derivation for terrain)
				else if( !com.stricmp( token.string, "q3map_textureSize" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					si->shaderWidth = com.atoi( token.string );
					GetTokenAppend( script, &token, shaderText, false );
					si->shaderHeight = com.atoi( token.string );
				}
				
				// q3map_tcGen <style> <parameters>
				else if( !com.stricmp( token.string, "q3map_tcGen" ))
				{
					si->tcGen = true;
					GetTokenAppend( script, &token, shaderText, false );
					
					// q3map_tcGen vector <s vector> <t vector>
					if( !com.stricmp( token.string, "vector" ) )
					{
						Parse1DMatrixAppend( script, shaderText, 3, si->vecs[0] );
						Parse1DMatrixAppend( script, shaderText, 3, si->vecs[1] );
					}
					
					// q3map_tcGen ivector <1.0/s vector> <1.0/t vector> (inverse vector, easier for mappers to understand)
					else if( !com.stricmp( token.string, "ivector" ) )
					{
						Parse1DMatrixAppend( script, shaderText, 3, si->vecs[0] );
						Parse1DMatrixAppend( script, shaderText, 3, si->vecs[1] );
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
					colorMod_t	*cm, *cm2;
					int		alpha;
					
					
					// alphamods are colormod + 1
					alpha = (!com.stricmp( token.string, "q3map_alphaGen" ) || !com.stricmp( token.string, "q3map_alphaMod" )) ? 1 : 0;
					
					cm = Malloc( sizeof( *cm ) );
					
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
					
					GetTokenAppend( script, &token, shaderText, false );
					
					// alpha set|const A
					if( alpha && (!com.stricmp( token.string, "set" ) || !com.stricmp( token.string, "const" )))
					{
						cm->type = CM_ALPHA_SET;
						GetTokenAppend( script, &token, shaderText, false );
						cm->data[0] = atof( token.string );
					}
					
					// color|rgb set|const ( X Y Z )
					else if( !com.stricmp( token.string, "set" ) || !com.stricmp( token.string, "const" ) )
					{
						cm->type = CM_COLOR_SET;
						Parse1DMatrixAppend( script, shaderText, 3, cm->data );
					}
					
					// alpha scale A
					else if( alpha && !com.stricmp( token.string, "scale" ))
					{
						cm->type = CM_ALPHA_SCALE;
						GetTokenAppend( script, &token, shaderText, false );
						cm->data[0] = atof( token.string );
					}
					
					// color|rgb scale ( X Y Z )
					else if( !com.stricmp( token.string, "scale" ))
					{
						cm->type = CM_COLOR_SCALE;
						Parse1DMatrixAppend( script, shaderText, 3, cm->data );
					}
					
					// dotProduct ( X Y Z )
					else if( !com.stricmp( token.string, "dotProduct" ))
					{
						cm->type = CM_COLOR_DOT_PRODUCT + alpha;
						Parse1DMatrixAppend( script, shaderText, 3, cm->data );
					}
					
					// dotProduct2 ( X Y Z )
					else if( !com.stricmp( token.string, "dotProduct2" ))
					{
						cm->type = CM_COLOR_DOT_PRODUCT_2 + alpha;
						Parse1DMatrixAppend( script, shaderText, 3, cm->data );
					}
					
					// volume
					else if( !com.stricmp( token.string, "volume" ))
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
					
					GetTokenAppend( script, &token, shaderText, false );
					
					// q3map_tcMod [translate | shift | offset] <s> <t>
					if( !com.stricmp( token.string, "translate" ) || !com.stricmp( token.string, "shift" ) || !com.stricmp( token.string, "offset" ))
					{
						GetTokenAppend( script, &token, shaderText, false );
						a = com.atof( token.string );
						GetTokenAppend( script, &token, shaderText, false );
						b = com.atof( token.string );
						
						TCModTranslate( si->mod, a, b );
					}

					// q3map_tcMod scale <s> <t>
					else if( !com.stricmp( token.string, "scale" ))
					{
						GetTokenAppend( script, &token, shaderText, false );
						a = com.atof( token.string );
						GetTokenAppend( script, &token, shaderText, false );
						b = com.atof( token.string );
						
						TCModScale( si->mod, a, b );
					}
					
					// q3map_tcMod rotate <s> <t> (FIXME: make this communitive)
					else if( !com.stricmp( token.string, "rotate" ) )
					{
						GetTokenAppend( script, &token, shaderText, false );
						a = com.atof( token.string );

						TCModRotate( si->mod, a );
					}
					else MsgDev( D_WARN, "unknown q3map_tcMod method: %s\n", token.string );
				}
				
				// q3map_fogDir (direction a fog shader fades from transparent to opaque)
				else if( !com.stricmp( token.string, "q3map_fogDir" ))
				{
					Parse1DMatrixAppend( script, shaderText, 3, si->fogDir );
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
				
				// q3map_indexed (for explicit terrain-style indexed mapping)
				else if( !com.stricmp( token.string, "q3map_indexed" ))
					si->indexed = true;
				
				// q3map_invert (inverts a drawsurface's facing)
				else if( !com.stricmp( token.string, "q3map_invert" ))
					si->invert = true;
				
				// q3map_lightmapMergable (ok to merge non-planar)
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
				
				// q3map_onlyvertexlighting (sof2)
				else if( !com.stricmp( token.string, "q3map_onlyVertexLighting" ))
					ApplySurfaceParm( "pointlight", &si->contentFlags, &si->surfaceFlags, &si->compileFlags );
				
				// q3map_material (sof2)
				else if( !com.stricmp( token.string, "q3map_material" ))
				{
					GetTokenAppend( script, &token, shaderText, false );
					com.sprintf( temp, "*mat_%s", token.string );
					if( !ApplySurfaceParm( temp, &si->contentFlags, &si->surfaceFlags, &si->compileFlags ))
						MsgDev( D_WARN, "Unknown material \"%s\"\n", token.string );
				}
				
				// q3map_clipmodel (autogenerate clip brushes for model triangles using this shader)
				else if( !com.stricmp( token.string, "q3map_clipmodel" ))
					si->clipModel = true;
				
				// q3map_styleMarker[2]
				else if( !com.stricmp( token.string, "q3map_styleMarker" ))
					si->styleMarker = 1;
				else if( !com.stricmp( token.string, "q3map_styleMarker2" )) // uses depthFunc equal
					si->styleMarker = 2;
				
				// default to searching for q3map_<surfaceparm>
				else ApplySurfaceParm( &token.string[6], &si->contentFlags, &si->surfaceFlags, &si->compileFlags );
			}
			
			
			/*
			-----------------------------------------------------------------
			skip
			-----------------------------------------------------------------
			*/
			
			// ignore all other tokens on the line
			Com_SkipRestOfLine( script );
		}			
	}
	Com_CloseScript( script );
}



/*
=================
ParseCustomInfoParms

loads custom info parms file for mods
=================
*/
static void ParseCustomInfoParms( void )
{
	bool	parsedContent, parsedSurface;
	script_t	*script;
	token_t	token;
	
	script = Com_OpenScript( "scripts/custinfoparms.txt", NULL, 0 );
	if( !script ) return;
	
	Mem_Set( custSurfaceParms, 0, sizeof( custSurfaceParms ));
	numCustSurfaceParms = 0;
	parsedContent = parsedSurface = false;
	
	Com_CheckToken( script, "{" );
	while ( 1 )
	{
		if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &token ))
			break;

		if( !com.strcmp( token.string, "}" ))
		{
			parsedContent = true;
			break;
		}

		custSurfaceParms[numCustSurfaceParms].name = copystring( token.string );
		Com_ReadToken( script, false, &token );
		sscanf( token.string, "%x", &custSurfaceParms[numCustSurfaceParms].contentFlags );
		numCustSurfaceParms++;
	}
	
	// any content?
	if( !parsedContent )
	{
		MsgDev( D_WARN, "Couldn't find valid custom contentsflag section\n" );
		return;
	}
	
	// parse custom surfaceflags
	Com_CheckToken( script, "{" );
	while( 1 )
	{
		if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &token ))
			break;

		if( !com.strcmp( token.string, "}" ))
		{
			parsedSurface = true;
			break;
		}

		custSurfaceParms[numCustSurfaceParms].name = copystring( token.string );
		Com_ReadToken( script, false, &token );
		sscanf( token.string, "%x", &custSurfaceParms[numCustSurfaceParms].surfaceFlags );
		numCustSurfaceParms++;
	}
	
	// any content?
	if( !parsedContent ) MsgDev( D_WARN, "Couldn't find valid custom surfaceflag section\n" );
}

	

/*
=================
LoadShaderInfo
=================
*/
void LoadShaderInfo( void )
{
	search_t	*search;
	int	i, numShaderFiles;
	
	// parse custom infoparms first
	if( useCustomInfoParms ) ParseCustomInfoParms();

	numShaderFiles = 0;
	search = FS_Search( "scripts/*.shader", true );
	if( !search ) return;
	
	for( i = 0; i < search->numfilenames; i++ )
	{
		ParseShaderFile( search->filenames[i] );
		numShaderFiles++;
	}
	Mem_Free( search );

	MsgDev( D_INFO, "%9d shaderInfo\n", numShaderInfo );
}