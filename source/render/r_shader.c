/*
Copyright (C) 1999 Stephen C. Taylor
Copyright (C) 2002-2007 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// r_shader.c

#include "r_local.h"
#include "mathlib.h"

typedef struct ref_script_s
{
	char		*name;
	int		type;
	uint		surfaceParm;
	char		*source;
	int		line;
	char		*buffer;
	size_t		size;
	struct ref_script_s	*nextHash;
} ref_script_t;

static byte		*r_shaderpool;
static table_t		*r_tablesHashTable[TABLES_HASH_SIZE];
static ref_script_t		*r_shaderCacheHash[SHADERS_HASH_SIZE];
static ref_shader_t		*r_shadersHash[SHADERS_HASH_SIZE];
static table_t		*r_tables[MAX_TABLES];
static int		r_numTables;
ref_shader_t		r_shaders[MAX_SHADERS];
static int		r_numShaders = 0;

typedef struct
{
	char *keyword;
	void ( *func )( ref_shader_t *shader, ref_stage_t *pass, const char **ptr );
} shaderkey_t;

typedef struct shadercache_s
{
	char *name;
	char *buffer;
	const char *filename;
	size_t offset;
	struct shadercache_s *hash_next;
} shadercache_t;

skydome_t *r_skydomes[MAX_SHADERS];

static char *shaderPaths;
static ref_shader_t	*shaders_hash[SHADERS_HASH_SIZE];
static shadercache_t *shadercache_hash[SHADERS_HASH_SIZE];

static deform_t	r_currentDeforms[MAX_SHADER_DEFORMS];
static ref_stage_t	r_currentPasses[MAX_SHADER_STAGES];
static float	r_currentRGBgenArgs[MAX_SHADER_STAGES][3], r_currentAlphagenArgs[MAX_SHADER_STAGES][2];
static waveFunc_t	r_currentRGBgenFuncs[MAX_SHADER_STAGES], r_currentAlphagenFuncs[MAX_SHADER_STAGES];
static tcMod_t	r_currentTcmods[MAX_SHADER_STAGES][MAX_SHADER_TCMODS];
static vec4_t	r_currentTcGen[MAX_SHADER_STAGES][2];
const char	*r_skyBoxSuffix[6] = { "rt", "bk", "lf", "ft", "up", "dn" }; // FIXME: get rid of this

static bool	r_shaderNoMipMaps;
static bool	r_shaderNoPicMip;
static bool	r_shaderNoCompress;
static bool	r_shaderHasDlightPass;

byte *r_shadersmempool;

static bool Shader_Parsetok( ref_shader_t *shader, ref_stage_t *pass, const shaderkey_t *keys, const char *token, const char **ptr );
static void Shader_MakeCache( const char *filename );
static unsigned int Shader_GetCache( const char *name, shadercache_t **cache );
#define Shader_FreePassCinematics(pass) if( (pass)->cinHandle ) { R_FreeCinematics( (pass)->cinHandle ); (pass)->cinHandle = 0; }

//===========================================================================
// COM_Compress and Com_ParseExt it's temporary stuff
#define MAX_TOKEN_CHARS	1024
/*
==============
COM_Compress

Parse a token out of a string
==============
*/
int COM_Compress( char *data_p )
{
	char	*in, *out;
	int	c;
	bool	newline = false, whitespace = false;

	in = out = data_p;
	if( in )
	{
		while( ( c = *in ) != 0 )
		{
			// skip double slash comments
			if( c == '/' && in[1] == '/' )
			{
				while( *in && *in != '\n' )
				{
					in++;
				}
				// skip /* */ comments
			}
			else if( c == '/' && in[1] == '*' )
			{
				while( *in && ( *in != '*' || in[1] != '/' ) )
					in++;
				if( *in )
					in += 2;
				// record when we hit a newline
			}
			else if( c == '\n' || c == '\r' )
			{
				newline = true;
				in++;
				// record when we hit whitespace
			}
			else if( c == ' ' || c == '\t' )
			{
				whitespace = true;
				in++;
				// an actual token
			}
			else
			{
				// if we have a pending newline, emit it (and it counts as whitespace)
				if( newline )
				{
					*out++ = '\n';
					newline = false;
					whitespace = false;
				}
				if( whitespace )
				{
					*out++ = ' ';
					whitespace = false;
				}

				// copy quoted strings unmolested
				if( c == '"' )
				{
					*out++ = c;
					in++;
					while( 1 )
					{
						c = *in;
						if( c && c != '"' )
						{
							*out++ = c;
							in++;
						}
						else
						{
							break;
						}
					}
					if( c == '"' )
					{
						*out++ = c;
						in++;
					}
				}
				else
				{
					*out = c;
					out++;
					in++;
				}
			}
		}
	}
	*out = 0;
	return out - data_p;
}

char	com_token[MAX_TOKEN_CHARS];

/*
==============
COM_ParseExt

Parse a token out of a string
==============
*/
char *COM_ParseExt( const char **data_p, bool nl )
{
	int		c;
	int		len;
	const char	*data;
	bool 		newlines = false;

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	if (!data)
	{
		*data_p = NULL;
		return "";
	}

// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
		{
			*data_p = NULL;
			return "";
		}
		if (c == '\n')
			newlines = true;
		data++;
	}

	if ( newlines && !nl )
	{
		*data_p = data;
		return com_token;
	}

	// skip // comments
	if (c == '/' && data[1] == '/')
	{
		data += 2;

		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// skip /* */ comments
	if (c == '/' && data[1] == '*')
	{
		data += 2;

		while (1)
		{
			if (!*data)
				break;
			if (*data != '*' || *(data+1) != '/')
				data++;
			else
			{
				data += 2;
				break;
			}
		}
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				if (len == MAX_TOKEN_CHARS)
					len = 0;
				com_token[len] = 0;
				*data_p = data;
				return com_token;
			}
			if (len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c>32);

	if (len == MAX_TOKEN_CHARS)
		len = 0;
	com_token[len] = 0;

	*data_p = data;
	return com_token;
}
//===========================================================================

static char *Shader_ParseString( const char **ptr )
{
	char *token;

	if( !ptr || !( *ptr ) )
		return "";
	if( !**ptr || **ptr == '}' )
		return "";

	token = COM_ParseExt( ptr, false );
	com.strlwr( token, token );

	return token;
}

static float Shader_ParseFloat( const char **ptr )
{
	if( !ptr || !( *ptr ) )
		return 0;
	if( !**ptr || **ptr == '}' )
		return 0;

	return atof( COM_ParseExt( ptr, false ) );
}

static void Shader_ParseVector( const char **ptr, float *v, unsigned int size )
{
	unsigned int i;
	char *token;
	bool bracket;

	if( !size )
		return;
	if( size == 1 )
	{
		Shader_ParseFloat( ptr );
		return;
	}

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "(" ) )
	{
		bracket = true;
		token = Shader_ParseString( ptr );
	}
	else if( token[0] == '(' )
	{
		bracket = true;
		token = &token[1];
	}
	else
	{
		bracket = false;
	}

	v[0] = atof( token );
	for( i = 1; i < size-1; i++ )
		v[i] = Shader_ParseFloat( ptr );

	token = Shader_ParseString( ptr );
	if( !token[0] )
	{
		v[i] = 0;
	}
	else if( token[strlen( token )-1] == ')' )
	{
		token[strlen( token )-1] = 0;
		v[i] = atof( token );
	}
	else
	{
		v[i] = atof( token );
		if( bracket )
			Shader_ParseString( ptr );
	}
}

static void Shader_SkipLine( const char **ptr )
{
	while( ptr )
	{
		const char *token = COM_ParseExt( ptr, false );
		if( !token[0] )
			return;
	}
}

static void Shader_SkipBlock( const char **ptr )
{
	const char *tok;
	int brace_count;

	// Opening brace
	tok = COM_ParseExt( ptr, true );
	if( tok[0] != '{' )
		return;

	for( brace_count = 1; brace_count > 0; )
	{
		tok = COM_ParseExt( ptr, true );
		if( !tok[0] )
			return;
		if( tok[0] == '{' )
			brace_count++;
		else if( tok[0] == '}' )
			brace_count--;
	}
}

#define MAX_CONDITIONS		8
typedef enum { COP_LS, COP_LE, COP_EQ, COP_GR, COP_GE, COP_NE } conOp_t;
typedef enum { COP2_AND, COP2_OR } conOp2_t;
typedef struct { int operand; conOp_t op; bool negative; int val; conOp2_t logic; } shaderCon_t;

char *conOpStrings[] = { "<", "<=", "==", ">", ">=", "!=", NULL };
char *conOpStrings2[] = { "&&", "||", NULL };

static bool Shader_ParseConditions( const char **ptr, ref_shader_t *shader )
{
	int i;
	char *tok;
	int numConditions;
	shaderCon_t conditions[MAX_CONDITIONS];
	bool result = false, val = false, skip, expectingOperator;
	static const int falseCondition = 0;

	numConditions = 0;
	memset( conditions, 0, sizeof( conditions ) );

	skip = false;
	expectingOperator = false;
	while( 1 )
	{
		tok = Shader_ParseString( ptr );
		if( !tok[0] )
		{
			if( expectingOperator )
				numConditions++;
			break;
		}
		if( skip )
			continue;

		for( i = 0; conOpStrings[i]; i++ )
		{
			if( !strcmp( tok, conOpStrings[i] ) )
				break;
		}

		if( conOpStrings[i] )
		{
			if( !expectingOperator )
			{
				MsgDev( D_WARN, "Bad syntax in condition (shader %s)\n", shader->name );
				skip = true;
			}
			else
			{
				conditions[numConditions].op = i;
				expectingOperator = false;
			}
			continue;
		}

		for( i = 0; conOpStrings2[i]; i++ )
		{
			if( !strcmp( tok, conOpStrings2[i] ) )
				break;
		}

		if( conOpStrings2[i] )
		{
			if( !expectingOperator )
			{
				MsgDev( D_WARN, "Bad syntax in condition (shader %s)\n", shader->name );
				skip = true;
			}
			else
			{
				conditions[numConditions++].logic = i;
				if( numConditions == MAX_CONDITIONS )
					skip = true;
				else
					expectingOperator = false;
			}
			continue;
		}

		if( expectingOperator )
		{
			MsgDev( D_WARN, "Bad syntax in condition (shader %s)\n", shader->name );
			skip = true;
			continue;
		}

		if( !strcmp( tok, "!" ) )
		{
			conditions[numConditions].negative = !conditions[numConditions].negative;
			continue;
		}

		if( !conditions[numConditions].operand )
		{
			if( !com.stricmp( tok, "maxTextureSize" ) )
				conditions[numConditions].operand = ( int  )glConfig.max_2d_texture_size;
			else if( !com.stricmp( tok, "maxTextureCubemapSize" ) )
				conditions[numConditions].operand = ( int )glConfig.max_cubemap_texture_size;
			else if( !com.stricmp( tok, "maxTextureUnits" ) )
				conditions[numConditions].operand = ( int )glConfig.max_texture_units;
			else if( !com.stricmp( tok, "textureCubeMap" ) )
				conditions[numConditions].operand = ( int )GL_Support( R_TEXTURECUBEMAP_EXT );
			else if( !com.stricmp( tok, "textureEnvCombine" ) )
				conditions[numConditions].operand = ( int )GL_Support( R_COMBINE_EXT );
			else if( !com.stricmp( tok, "textureEnvDot3" ) )
				conditions[numConditions].operand = ( int )GL_Support( R_SHADER_GLSL100_EXT );
			else if( !com.stricmp( tok, "GLSL" ) )
				conditions[numConditions].operand = ( int )GL_Support( R_SHADER_GLSL100_EXT );
			else if( !com.stricmp( tok, "deluxeMaps" ) || !com.stricmp( tok, "deluxe" ) )
				conditions[numConditions].operand = ( int )mapConfig.deluxeMappingEnabled;
			else if( !com.stricmp( tok, "portalMaps" ) )
				conditions[numConditions].operand = ( int )r_portalmaps->integer;
			else
			{
				MsgDev( D_WARN, "Unknown expression '%s' in shader %s\n", tok, shader->name );
//				skip = true;
				conditions[numConditions].operand = ( int )falseCondition;
			}

			conditions[numConditions].operand++;
			if( conditions[numConditions].operand < 0 )
				conditions[numConditions].operand = 0;

			if( !skip )
			{
				conditions[numConditions].op = COP_NE;
				expectingOperator = true;
			}
			continue;
		}

		if( !strcmp( tok, "false" ) )
			conditions[numConditions].val = 0;
		else if( !strcmp( tok, "true" ) )
			conditions[numConditions].val = 1;
		else
			conditions[numConditions].val = atoi( tok );
		expectingOperator = true;
	}

	if( skip )
		return false;

	if( !conditions[0].operand )
	{
		MsgDev( D_WARN, "Empty 'if' statement in shader %s\n", shader->name );
		return false;
	}


	for( i = 0; i < numConditions; i++ )
	{
		conditions[i].operand--;

		switch( conditions[i].op )
		{
		case COP_LS:
			val = ( conditions[i].operand < conditions[i].val );
			break;
		case COP_LE:
			val = ( conditions[i].operand <= conditions[i].val );
			break;
		case COP_EQ:
			val = ( conditions[i].operand == conditions[i].val );
			break;
		case COP_GR:
			val = ( conditions[i].operand > conditions[i].val );
			break;
		case COP_GE:
			val = ( conditions[i].operand >= conditions[i].val );
			break;
		case COP_NE:
			val = ( conditions[i].operand != conditions[i].val );
			break;
		default:
			break;
		}

		if( conditions[i].negative )
			val = !val;
		if( i )
		{
			switch( conditions[i-1].logic )
			{
			case COP2_AND:
				result = result && val;
				break;
			case COP2_OR:
				result = result || val;
				break;
			}
		}
		else
		{
			result = val;
		}
	}

	return result;
}

static bool Shader_SkipConditionBlock( const char **ptr )
{
	const char *tok;
	int condition_count;

	for( condition_count = 1; condition_count > 0; )
	{
		tok = COM_ParseExt( ptr, true );
		if( !tok[0] )
			return false;
		if( !com.stricmp( tok, "if" ) )
			condition_count++;
		else if( !com.stricmp( tok, "endif" ) )
			condition_count--;
// Vic: commented out for now
//		else if( !com.stricmp( tok, "else" ) && (condition_count == 1) )
//			return true;
	}

	return true;
}

//===========================================================================

static void Shader_ParseSkySides( const char **ptr, ref_shader_t **shaders, bool farbox )
{
	int	i;
	char	*token;
	texture_t	*image;
	string	name;
	bool	noskybox = false;

	token = Shader_ParseString( ptr );
	if( token[0] == '-' || !com.stricmp( token, "full" ))
	{
		noskybox = true;
	}
	else
	{
		Mem_Set( shaders, 0, sizeof( ref_shader_t * ) * 6 );

		for( i = 0; i < 6; i++ )
		{
			com.snprintf( name, sizeof( name ), "%s_%s", token, r_skyBoxSuffix[i] );
			image = R_FindTexture( name, NULL, 0, TF_CLAMP|TF_NOMIPMAP|TF_SKYSIDE );
			if( !image ) break;
			shaders[i] = R_LoadShader( image->name, ( farbox ? SHADER_FARBOX : SHADER_NEARBOX ), true, image->flags, SHADER_INVALID );
		}
		if( i != 6 ) noskybox = true;
	}

	if( noskybox )
		memset( shaders, 0, sizeof( ref_shader_t * ) * 6 );
}

static void Shader_ParseFunc( const char **ptr, waveFunc_t *func )
{
	char *token;

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "sin" ) )
		func->type = WAVEFORM_SIN;
	else if( !strcmp( token, "triangle" ) )
		func->type = WAVEFORM_TRIANGLE;
	else if( !strcmp( token, "square" ) )
		func->type = WAVEFORM_SQUARE;
	else if( !strcmp( token, "sawtooth" ) )
		func->type = WAVEFORM_SAWTOOTH;
	else if( !strcmp( token, "inversesawtooth" ) )
		func->type = WAVEFORM_INVERSESAWTOOTH;
	else if( !strcmp( token, "noise" ) )
		func->type = WAVEFORM_NOISE;

	func->args[0] = Shader_ParseFloat( ptr );
	func->args[1] = Shader_ParseFloat( ptr );
	func->args[2] = Shader_ParseFloat( ptr );
	func->args[3] = Shader_ParseFloat( ptr );
}

//===========================================================================

static int Shader_SetImageFlags( ref_shader_t *shader )
{
	int flags = 0;

	if( shader->flags & SHADER_SKYPARMS )
		flags |= TF_SKYSIDE;
	if( r_shaderNoMipMaps )
		flags |= TF_NOMIPMAP;
	if( r_shaderNoPicMip )
		flags |= TF_NOPICMIP;
	if( r_shaderNoCompress )
		flags |= TF_UNCOMPRESSED;

	return flags;
}

static texture_t *Shader_FindImage( ref_shader_t *shader, char *name, int flags )
{
	texture_t		*image;

	if( !com.stricmp( name, "$whiteimage" ) || !com.stricmp( name, "*white" ) )
		return tr.whiteTexture;
	if( !com.stricmp( name, "$blackimage" ) || !com.stricmp( name, "*black" ) )
		return tr.blackTexture;
	if( !com.stricmp( name, "$blankbumpimage" ) || !com.stricmp( name, "*blankbump" ) )
		return tr.blankbumpTexture;
	if( !com.stricmp( name, "$particle" ) || !com.stricmp( name, "*particle" ) )
		return tr.particleTexture;
	if( !com.stricmp( name, "$corona" ) || !com.stricmp( name, "*corona" ) )
		return tr.coronaTexture;
	if( !com.strnicmp( name, "*lm", 3 ) )
	{
		MsgDev( D_WARN, "shader %s has a stage with explicit lightmap image.\n", shader->name );
		return tr.whiteTexture;
	}

	FS_StripExtension( name ); // FIXME: stupid quake3 bug!!!

	image = R_FindTexture( va( "\"%s\"", name ), NULL, 0, flags );
	if( !image )
	{
		MsgDev( D_WARN, "shader %s has a stage with no image: %s\n", shader->name, name );
		return tr.defaultTexture;
	}

	return image;
}

/****************** shader keyword functions ************************/

static void Shader_Cull( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	char *token;

	shader->flags &= ~( SHADER_CULL_FRONT|SHADER_CULL_BACK );

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "disable" ) || !strcmp( token, "none" ) || !strcmp( token, "twosided" ) )
		;
	else if( !strcmp( token, "back" ) || !strcmp( token, "backside" ) || !strcmp( token, "backsided" ) )
		shader->flags |= SHADER_CULL_BACK;
	else
		shader->flags |= SHADER_CULL_FRONT;
}

static void Shader_shaderNoMipMaps( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	r_shaderNoMipMaps = r_shaderNoPicMip = true;
}

static void Shader_shaderNoPicMip( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	r_shaderNoPicMip = true;
}

static void Shader_shaderNoCompress( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	r_shaderNoCompress = true;
}

static void Shader_DeformVertexes( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	char	*token;
	deform_t	*deformv;

	if( shader->numDeforms == MAX_SHADER_DEFORMS )
	{
		MsgDev( D_WARN, "shader %s has too many deforms\n", shader->name );
		Shader_SkipLine( ptr );
		return;
	}

	deformv = &r_currentDeforms[shader->numDeforms];

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "wave" ) )
	{
		deformv->type = DEFORM_WAVE;
		deformv->args[0] = Shader_ParseFloat( ptr );
		if( deformv->args[0] )
			deformv->args[0] = 1.0f / deformv->args[0];
		else
			deformv->args[0] = 100.0f;
		Shader_ParseFunc( ptr, &deformv->func );
	}
	else if( !strcmp( token, "normal" ) )
	{
		shader->flags |= SHADER_DEFORM_NORMAL;
		deformv->type = DEFORM_NORMAL;
		deformv->args[0] = Shader_ParseFloat( ptr );
		deformv->args[1] = Shader_ParseFloat( ptr );
	}
	else if( !strcmp( token, "bulge" ) )
	{
		deformv->type = DEFORM_BULGE;
		Shader_ParseVector( ptr, deformv->args, 3 );
	}
	else if( !strcmp( token, "move" ) )
	{
		deformv->type = DEFORM_MOVE;
		Shader_ParseVector( ptr, deformv->args, 3 );
		Shader_ParseFunc( ptr, &deformv->func );
	}
	else if( !strcmp( token, "autosprite" ) )
	{
		deformv->type = DEFORM_AUTOSPRITE;
		shader->flags |= SHADER_AUTOSPRITE;
	}
	else if( !strcmp( token, "autosprite2" ) )
	{
		deformv->type = DEFORM_AUTOSPRITE2;
		shader->flags |= SHADER_AUTOSPRITE;
	}
	else if( !strcmp( token, "projectionShadow" ) )
		deformv->type = DEFORM_PROJECTION_SHADOW;
	else if( !strcmp( token, "autoparticle" ) )
		deformv->type = DEFORM_AUTOPARTICLE;

	else if( !strcmp( token, "outline" ) )
		deformv->type = DEFORM_OUTLINE;
	else
	{
		Shader_SkipLine( ptr );
		return;
	}

	shader->numDeforms++;
}

static void Shader_SkyParms( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	int shaderNum;
	float skyheight;
	ref_shader_t *farboxShaders[6];
	ref_shader_t *nearboxShaders[6];

	shaderNum = shader - r_shaders;
	if( r_skydomes[shaderNum] )
		R_FreeSkydome( r_skydomes[shaderNum] );

	Shader_ParseSkySides( ptr, farboxShaders, true );

	skyheight = Shader_ParseFloat( ptr );
	if( !skyheight )
		skyheight = 512.0f;

//	if( skyheight*sqrt(3) > r_farclip_min )
//		r_farclip_min = skyheight*sqrt(3);
	if( skyheight*2 > r_farclip_min )
		r_farclip_min = skyheight*2;

	Shader_ParseSkySides( ptr, nearboxShaders, false );

	r_skydomes[shaderNum] = R_CreateSkydome( skyheight, farboxShaders, nearboxShaders );
	shader->flags |= SHADER_SKYPARMS;
	shader->sort = SORT_SKY;
}

static void Shader_FogParms( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	float div;
	vec3_t color, fcolor;

	if( !r_ignorehwgamma->integer )
		div = 1.0f / pow( 2, max( 0, floor( r_overbrightbits->value ) ) );
	else
		div = 1.0f;

	Shader_ParseVector( ptr, color, 3 );
	ColorNormalize( color, fcolor );
	VectorScale( fcolor, div, fcolor );

	shader->fog_color[0] = R_FloatToByte( fcolor[0] );
	shader->fog_color[1] = R_FloatToByte( fcolor[1] );
	shader->fog_color[2] = R_FloatToByte( fcolor[2] );
	shader->fog_color[3] = 255;
	shader->fog_dist = Shader_ParseFloat( ptr );
	if( shader->fog_dist <= 0.1 )
		shader->fog_dist = 128.0;

	shader->fog_clearDist = Shader_ParseFloat( ptr );
	if( shader->fog_clearDist > shader->fog_dist - 128 )
		shader->fog_clearDist = shader->fog_dist - 128;
	if( shader->fog_clearDist <= 0.0 )
		shader->fog_clearDist = 0;
}

static void Shader_Sort( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "portal" ) )
		shader->sort = SORT_PORTAL;
	else if( !strcmp( token, "sky" ) )
		shader->sort = SORT_SKY;
	else if( !strcmp( token, "opaque" ) )
		shader->sort = SORT_OPAQUE;
	else if( !strcmp( token, "banner" ) )
		shader->sort = SORT_BANNER;
	else if( !strcmp( token, "underwater" ) )
		shader->sort = SORT_UNDERWATER;
	else if( !strcmp( token, "additive" ) )
		shader->sort = SORT_ADDITIVE;
	else if( !strcmp( token, "nearest" ) )
		shader->sort = SORT_NEAREST;
	else
	{
		shader->sort = atoi( token );
		if( shader->sort > SORT_NEAREST )
			shader->sort = SORT_NEAREST;
	}
}

static void Shader_Portal( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	shader->flags |= SHADER_PORTAL;
	shader->sort = SORT_PORTAL;
}

static void Shader_PolygonOffset( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	shader->flags |= SHADER_POLYGONOFFSET;
}

static void Shader_EntityMergable( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	shader->flags |= SHADER_ENTITY_MERGABLE;
}

static void Shader_If( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	if( !Shader_ParseConditions( ptr, shader ) )
	{
		if( !Shader_SkipConditionBlock( ptr ) )
			MsgDev( D_WARN, "Mismatched if/endif pair in shader %s\n", shader->name );
	}
}

static void Shader_Endif( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
}

static void Shader_NoModulativeDlights( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	shader->flags |= SHADER_NO_MODULATIVE_DLIGHTS;
}

static void Shader_OffsetMappingScale( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	shader->offsetmapping_scale = Shader_ParseFloat( ptr );
	if( shader->offsetmapping_scale < 0 )
		shader->offsetmapping_scale = 0;
}

static const shaderkey_t shaderkeys[] =
{
	{ "cull", Shader_Cull },
	{ "skyparms", Shader_SkyParms },
	{ "fogparms", Shader_FogParms },
	{ "fogvars", Shader_FogParms },	// RTCW fog params
	{ "nomipmaps", Shader_shaderNoMipMaps },
	{ "nopicmip", Shader_shaderNoPicMip },
	{ "polygonoffset", Shader_PolygonOffset },
	{ "sort", Shader_Sort },
	{ "deformvertexes", Shader_DeformVertexes },
	{ "portal", Shader_Portal },
	{ "entitymergable", Shader_EntityMergable },
	{ "if",	Shader_If },
	{ "endif", Shader_Endif },
	{ "nomodulativedlights", Shader_NoModulativeDlights },
	{ "nocompress",	Shader_shaderNoCompress },
	{ "offsetmappingscale", Shader_OffsetMappingScale },
	{ NULL,	NULL }
};

// ===============================================================

static bool Shaderpass_LoadMaterial( texture_t **normalmap, texture_t **glossmap, texture_t **decalmap, const char *name, int addFlags, float bumpScale )
{
	texture_t		*images[3];
	
	// set defaults
	images[0] = images[1] = images[2] = NULL;

	// load normalmap image
	images[0] = R_FindTexture( va( "heightMap( \"%s_bump\", %g );", name, bumpScale ), NULL, 0, addFlags );
	if( !images[0] )
	{
		images[0] = R_FindTexture( va( "mergeDepthmap( \"%s_norm\", \"%s_depth\" );", name, name ), NULL, 0, (addFlags|TF_NORMALMAP));

		if( !images[0] )
		{
			if( !r_lighting_diffuse2heightmap->integer )
				return false;
			images[0] = R_FindTexture( va( "heightMap( \"%s\", 2.0f );", name ), NULL, 0, addFlags );
			if( !images[0] ) return false;
		}
	}

	// load glossmap image
	if( r_lighting_specular->integer )
		images[1] = R_FindTexture( va( "%s_gloss", name ), NULL, 0, addFlags );

	images[2] = R_FindTexture( va( "%s_decal", name ), NULL, 0, addFlags );

	*normalmap = images[0];
	*glossmap = images[1];
	*decalmap = images[2];

	return true;
}

static void Shaderpass_MapExt( ref_shader_t *shader, ref_stage_t *pass, int addFlags, const char **ptr )
{
	int flags;
	char *token;

	Shader_FreePassCinematics( pass );

	token = Shader_ParseString( ptr );

	if( token[0] == '$' )
	{
		token++;
		if( !strcmp( token, "lightmap" ) )
		{
			pass->tcgen = TCGEN_LIGHTMAP;
			pass->flags = ( pass->flags & ~( SHADERSTAGE_PORTALMAP|SHADERSTAGE_DLIGHT ) ) | SHADERSTAGE_LIGHTMAP;
			pass->animFrequency = 0;
			pass->textures[0] = NULL;
			return;
		}
		else if( !strcmp( token, "dlight" ) )
		{
			pass->tcgen = TCGEN_BASE;
			pass->flags = ( pass->flags & ~( SHADERSTAGE_LIGHTMAP|SHADERSTAGE_PORTALMAP ) ) | SHADERSTAGE_DLIGHT;
			pass->animFrequency = 0;
			pass->textures[0] = NULL;
			r_shaderHasDlightPass = true;
			return;
		}
		else if( !strcmp( token, "portalmap" ) || !strcmp( token, "mirrormap" ) )
		{
			pass->tcgen = TCGEN_PROJECTION;
			pass->flags = ( pass->flags & ~( SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT ) ) | SHADERSTAGE_PORTALMAP;
			pass->animFrequency = 0;
			pass->textures[0] = NULL;
			if( ( shader->flags & SHADER_PORTAL ) && ( shader->sort == SORT_PORTAL ) )
				shader->sort = 0; // reset sorting so we can figure it out later. FIXME?
			shader->flags |= SHADER_PORTAL|( r_portalmaps->integer ? SHADER_PORTAL_CAPTURE1 : 0 );
			return;
		}
		else if( !strcmp( token, "rgb" ) )
		{
			addFlags |= TF_NOALPHA;
			token = Shader_ParseString( ptr );
		}
		else if( !strcmp( token, "alpha" ) )
		{
			addFlags |= TF_NORGB;
			token = Shader_ParseString( ptr );
		}
		else
		{
			token--;
		}
	}

	flags = Shader_SetImageFlags( shader ) | addFlags;
	pass->tcgen = TCGEN_BASE;
	pass->flags &= ~( SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT|SHADERSTAGE_PORTALMAP );
	pass->animFrequency = 0;
	pass->textures[0] = Shader_FindImage( shader, token, flags );
	if( !pass->textures[0] )
		MsgDev( D_WARN, "Shader %s has a stage with no image: %s.\n", shader->name, token );
}

static void Shaderpass_AnimMapExt( ref_shader_t *shader, ref_stage_t *pass, int addFlags, const char **ptr )
{
	int flags;
	char *token;

	Shader_FreePassCinematics( pass );

	flags = Shader_SetImageFlags( shader ) | addFlags;

	pass->tcgen = TCGEN_BASE;
	pass->flags &= ~( SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT|SHADERSTAGE_PORTALMAP );
	pass->animFrequency = Shader_ParseFloat( ptr );
	pass->num_textures = 0;

	for(;; )
	{
		token = Shader_ParseString( ptr );
		if( !token[0] )
			break;
		if( pass->num_textures < MAX_STAGE_TEXTURES )
			pass->textures[pass->num_textures++] = Shader_FindImage( shader, token, flags );
	}

	if( pass->num_textures == 0 )
		pass->animFrequency = 0;
}

static void Shaderpass_CubeMapExt( ref_shader_t *shader, ref_stage_t *pass, int addFlags, int tcgen, const char **ptr )
{
	int		flags;
	char		*token;
	
	Shader_FreePassCinematics( pass );

	token = Shader_ParseString( ptr );
	flags = Shader_SetImageFlags( shader ) | addFlags;
	pass->animFrequency = 0;
	pass->flags &= ~( SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT|SHADERSTAGE_PORTALMAP );

	if( !GL_Support( R_TEXTURECUBEMAP_EXT ))
	{
		MsgDev( D_WARN, "Shader %s has an unsupported cubemap stage: %s.\n", shader->name );
		pass->textures[0] = tr.defaultTexture;
		pass->tcgen = TCGEN_BASE;
		return;
	}

	pass->textures[0] = R_FindTexture( token, NULL, 0, flags|TF_CUBEMAP );
	if( pass->textures[0] )
	{
		pass->tcgen = tcgen;
	}
	else
	{
		MsgDev( D_WARN, "Shader %s has a stage with no image: %s.\n", shader->name, token );
		pass->textures[0] = tr.defaultTexture;
		pass->tcgen = TCGEN_BASE;
	}
}

static void Shaderpass_Map( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	Shaderpass_MapExt( shader, pass, 0, ptr );
}

static void Shaderpass_ClampMap( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	Shaderpass_MapExt( shader, pass, TF_CLAMP, ptr );
}

static void Shaderpass_AnimMap( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	Shaderpass_AnimMapExt( shader, pass, 0, ptr );
}

static void Shaderpass_AnimClampMap( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	Shaderpass_AnimMapExt( shader, pass, TF_CLAMP, ptr );
}

static void Shaderpass_CubeMap( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	Shaderpass_CubeMapExt( shader, pass, TF_CLAMP, TCGEN_REFLECTION, ptr );
}

static void Shaderpass_ShadeCubeMap( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	Shaderpass_CubeMapExt( shader, pass, TF_CLAMP, TCGEN_REFLECTION_CELLSHADE, ptr );
}

static void Shaderpass_VideoMap( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	char *token;

	Shader_FreePassCinematics( pass );

	token = Shader_ParseString( ptr );

	pass->cinHandle = R_StartCinematics( token );
	pass->tcgen = TCGEN_BASE;
	pass->animFrequency = 0;
	pass->flags &= ~(SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT|SHADERSTAGE_PORTALMAP);
}

static void Shaderpass_NormalMap( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	int flags;
	char *token;
	float bumpScale = 0;

	if( !GL_Support( R_SHADER_GLSL100_EXT ))
	{
		MsgDev( D_WARN, "shader %s has a normalmap stage, while GLSL is not supported\n", shader->name );
		Shader_SkipLine( ptr );
		return;
	}

	Shader_FreePassCinematics( pass );

	flags = Shader_SetImageFlags( shader );
	token = Shader_ParseString( ptr );

	if( !strcmp( token, "$heightmap" ) )
	{
		bumpScale = Shader_ParseFloat( ptr );
		token = Shader_ParseString( ptr );
		token = va( "heightMap( \"%s\", %g );", token, bumpScale );
	}

	pass->tcgen = TCGEN_BASE;
	pass->flags &= ~( SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT|SHADERSTAGE_PORTALMAP );
	pass->textures[1] = Shader_FindImage( shader, token, flags );
	if( pass->textures[1] )
	{
		pass->program = DEFAULT_GLSL_PROGRAM;
		pass->program_type = PROGRAM_TYPE_MATERIAL;
	}

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "$noimage" ) )
		pass->textures[0] = tr.whiteTexture;
	else
		pass->textures[0] = Shader_FindImage( shader, token, Shader_SetImageFlags( shader ));
}

static void Shaderpass_Material( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	int flags;
	char *token;
	float bumpScale = 0;

	if( !GL_Support( R_SHADER_GLSL100_EXT ))
	{
		MsgDev( D_WARN, "shader %s has a normalmap stage, while GLSL is not supported\n", shader->name );
		Shader_SkipLine( ptr );
		return;
	}

	Shader_FreePassCinematics( pass );

	flags = Shader_SetImageFlags( shader );
	token = Shader_ParseString( ptr );

	if( token[0] == '$' )
	{
		token++;
		if( !strcmp( token, "rgb" ) )
		{
			flags |= TF_NOALPHA;
			token = Shader_ParseString( ptr );
		}
		else if( !strcmp( token, "alpha" ) )
		{
			flags |= TF_NORGB;
			token = Shader_ParseString( ptr );
		}
		else
		{
			token--;
		}
	}

	pass->textures[0] = Shader_FindImage( shader, token, flags );
	if( !pass->textures[0] )
	{
		MsgDev( D_WARN, "failed to load base/diffuse image for material %s in shader %s.\n", token, shader->name );
		return;
	}

	pass->textures[1] = pass->textures[2] = pass->textures[3] = NULL;

	pass->tcgen = TCGEN_BASE;
	pass->flags &= ~( SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT|SHADERSTAGE_PORTALMAP );
	flags &= ~(TF_NOALPHA|TF_NORGB);

	while( 1 )
	{
		token = Shader_ParseString( ptr );
		if( !*token )
			break;

		if( com.is_digit( token ) )
		{
			bumpScale = com.atoi( token );
		}
		else if( !pass->textures[1] )
		{
			if( bumpScale ) token = va( "heightMap( \"%s\", %g );", token, bumpScale );
			pass->textures[1] = Shader_FindImage( shader, token, flags );
			if( !pass->textures[1] )
			{
				MsgDev( D_WARN, "missing normalmap image %s in shader %s.\n", token, shader->name );
				pass->textures[1] = tr.blankbumpTexture;
			}
			else
			{
				pass->program = DEFAULT_GLSL_PROGRAM;
				pass->program_type = PROGRAM_TYPE_MATERIAL;
			}
		}
		else if( !pass->textures[2] )
		{
			if( strcmp( token, "-" ) && r_lighting_specular->integer )
			{
				pass->textures[2] = Shader_FindImage( shader, token, flags );
				if( !pass->textures[2] )
					MsgDev( D_WARN, "missing glossmap image %s in shader %s.\n", token, shader->name );
			}
			
			// set gloss to r_blacktexture so we know we have already parsed the gloss image
			if( pass->textures[2] == NULL )
				pass->textures[2] = tr.blackTexture;
		}
		else
		{
			pass->textures[3] = Shader_FindImage( shader, token, flags );
			if( !pass->textures[3] )
				MsgDev( D_WARN, "missing decal image %s in shader %s.\n", token, shader->name );
		}
	}

	// black texture => no gloss, so don't waste time in the GLSL program
	if( pass->textures[2] == tr.blackTexture )
		pass->textures[2] = NULL;

	if( pass->textures[1] )
		return;

	// try loading default images
	if( Shaderpass_LoadMaterial( &pass->textures[1], &pass->textures[2], &pass->textures[3], pass->textures[0]->name, flags, bumpScale ) )
	{
		pass->program = DEFAULT_GLSL_PROGRAM;
		pass->program_type = PROGRAM_TYPE_MATERIAL;
	}
	else
	{
		MsgDev( D_WARN, "failed to load default images for material %s in shader %s.\n", pass->textures[0]->name, shader->name );
	}
}

static void Shaderpass_Distortion( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	int flags;
	char *token;
	float bumpScale = 0;

	if( !GL_Support( R_SHADER_GLSL100_EXT ) || !r_portalmaps->integer )
	{
		MsgDev( D_WARN, "shader %s has a distortion stage, while GLSL is not supported\n", shader->name );
		Shader_SkipLine( ptr );
		return;
	}

	Shader_FreePassCinematics( pass );

	flags = Shader_SetImageFlags( shader );
	pass->flags &= ~( SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT|SHADERSTAGE_PORTALMAP );
	pass->textures[0] = pass->textures[1] = NULL;

	while( 1 )
	{
		token = Shader_ParseString( ptr );
		if( !*token )
			break;

		if( com.is_digit( token ) )
		{
			bumpScale = com.atoi( token );
		}
		else if( !pass->textures[0] )
		{
			pass->textures[0] = Shader_FindImage( shader, token, flags );
			if( !pass->textures[0] )
			{
				MsgDev( D_WARN, "missing dudvmap image %s in shader %s.\n", token, shader->name );
				pass->textures[0] = tr.blackTexture;
			}

			pass->program = DEFAULT_GLSL_DISTORTION_PROGRAM;
			pass->program_type = PROGRAM_TYPE_DISTORTION;
		}
		else
		{
			if( bumpScale ) token = va( "heightMap( \"%s\", %g );", token, bumpScale );
			pass->textures[1] = Shader_FindImage( shader, token, flags );
			if( !pass->textures[1] )
				MsgDev( D_WARN, "missing normalmap image %s in shader.\n", token, shader->name );
		}
	}

	if( pass->rgbGen.type == RGBGEN_UNKNOWN )
	{
		pass->rgbGen.type = RGBGEN_CONST;
		VectorClear( pass->rgbGen.args );
	}

	shader->flags |= SHADER_PORTAL|SHADER_PORTAL_CAPTURE;
}

static void Shaderpass_RGBGen( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "identitylighting" ) )
		pass->rgbGen.type = RGBGEN_IDENTITY_LIGHTING;
	else if( !strcmp( token, "identity" ) )
		pass->rgbGen.type = RGBGEN_IDENTITY;
	else if( !strcmp( token, "wave" ) )
	{
		pass->rgbGen.type = RGBGEN_WAVE;
		pass->rgbGen.args[0] = 1.0f;
		pass->rgbGen.args[1] = 1.0f;
		pass->rgbGen.args[2] = 1.0f;
		Shader_ParseFunc( ptr, pass->rgbGen.func );
	}
	else if( !strcmp( token, "colorwave" ) )
	{
		pass->rgbGen.type = RGBGEN_WAVE;
		Shader_ParseVector( ptr, pass->rgbGen.args, 3 );
		Shader_ParseFunc( ptr, pass->rgbGen.func );
	}
	else if( !strcmp( token, "entity" ) )
		pass->rgbGen.type = RGBGEN_ENTITY;
	else if( !strcmp( token, "oneminusentity" ) )
		pass->rgbGen.type = RGBGEN_ONE_MINUS_ENTITY;
	else if( !strcmp( token, "vertex" ) )
		pass->rgbGen.type = RGBGEN_VERTEX;
	else if( !strcmp( token, "oneminusvertex" ) )
		pass->rgbGen.type = RGBGEN_ONE_MINUS_VERTEX;
	else if( !strcmp( token, "lightingdiffuse" ) )
		pass->rgbGen.type = RGBGEN_LIGHTING_DIFFUSE;
	else if( !strcmp( token, "lightingdiffuseonly" ) )
		pass->rgbGen.type = RGBGEN_LIGHTING_DIFFUSE_ONLY;
	else if( !strcmp( token, "lightingambientonly" ) )
		pass->rgbGen.type = RGBGEN_LIGHTING_AMBIENT_ONLY;
	else if( !strcmp( token, "exactvertex" ) )
		pass->rgbGen.type = RGBGEN_EXACT_VERTEX;
	else if( !strcmp( token, "const" ) || !strcmp( token, "constant" ) )
	{
		float div;
		vec3_t color;

		if( !r_ignorehwgamma->integer )
			div = 1.0f / pow( 2, max( 0, floor( r_overbrightbits->value ) ) );
		else
			div = 1.0f;

		pass->rgbGen.type = RGBGEN_CONST;
		Shader_ParseVector( ptr, color, 3 );
		ColorNormalize( color, pass->rgbGen.args );
		VectorScale( pass->rgbGen.args, div, pass->rgbGen.args );
	}
	else if( !strcmp( token, "custom" ) || !strcmp( token, "teamcolor" ) )
	{
		// the "teamcolor" thing comes from warsow
		pass->rgbGen.type = RGBGEN_CUSTOM;
		pass->rgbGen.args[0] = (int)Shader_ParseFloat( ptr );
		if( pass->rgbGen.args[0] < 0 || pass->rgbGen.args[0] >= NUM_CUSTOMCOLORS )
			pass->rgbGen.args[0] = 0;
	}
}

static void Shaderpass_AlphaGen( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "portal" ) )
	{
		pass->alphaGen.type = ALPHAGEN_PORTAL;
		pass->alphaGen.args[0] = fabs( Shader_ParseFloat( ptr ) );
		if( !pass->alphaGen.args[0] )
			pass->alphaGen.args[0] = 256;
		pass->alphaGen.args[0] = 1.0f / pass->alphaGen.args[0];
	}
	else if( !strcmp( token, "vertex" ) )
		pass->alphaGen.type = ALPHAGEN_VERTEX;
	else if( !strcmp( token, "oneminusvertex" ) )
		pass->alphaGen.type = ALPHAGEN_ONE_MINUS_VERTEX;
	else if( !strcmp( token, "entity" ) )
		pass->alphaGen.type = ALPHAGEN_ENTITY;
	else if( !strcmp( token, "wave" ) )
	{
		pass->alphaGen.type = ALPHAGEN_WAVE;
		Shader_ParseFunc( ptr, pass->alphaGen.func );
	}
	else if( !strcmp( token, "lightingspecular" ) )
	{
		pass->alphaGen.type = ALPHAGEN_SPECULAR;
		pass->alphaGen.args[0] = fabs( Shader_ParseFloat( ptr ) );
		if( !pass->alphaGen.args[0] )
			pass->alphaGen.args[0] = 5.0f;
	}
	else if( !strcmp( token, "const" ) || !strcmp( token, "constant" ) )
	{
		pass->alphaGen.type = ALPHAGEN_CONST;
		pass->alphaGen.args[0] = fabs( Shader_ParseFloat( ptr ) );
	}
	else if( !strcmp( token, "dot" ) )
	{
		pass->alphaGen.type = ALPHAGEN_DOT;
		pass->alphaGen.args[0] = fabs( Shader_ParseFloat( ptr ) );
		pass->alphaGen.args[1] = fabs( Shader_ParseFloat( ptr ) );
		if( !pass->alphaGen.args[1] )
			pass->alphaGen.args[1] = 1.0f;
	}
	else if( !strcmp( token, "oneminusdot" ) )
	{
		pass->alphaGen.type = ALPHAGEN_ONE_MINUS_DOT;
		pass->alphaGen.args[0] = fabs( Shader_ParseFloat( ptr ) );
		pass->alphaGen.args[1] = fabs( Shader_ParseFloat( ptr ) );
		if( !pass->alphaGen.args[1] )
			pass->alphaGen.args[1] = 1.0f;
	}
}

static _inline int Shaderpass_SrcBlendBits( char *token )
{
	if( !strcmp( token, "gl_zero" ) )
		return GLSTATE_SRCBLEND_ZERO;
	if( !strcmp( token, "gl_one" ) )
		return GLSTATE_SRCBLEND_ONE;
	if( !strcmp( token, "gl_dst_color" ) )
		return GLSTATE_SRCBLEND_DST_COLOR;
	if( !strcmp( token, "gl_one_minus_dst_color" ) )
		return GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR;
	if( !strcmp( token, "gl_src_alpha" ) )
		return GLSTATE_SRCBLEND_SRC_ALPHA;
	if( !strcmp( token, "gl_one_minus_src_alpha" ) )
		return GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	if( !strcmp( token, "gl_dst_alpha" ) )
		return GLSTATE_SRCBLEND_DST_ALPHA;
	if( !strcmp( token, "gl_one_minus_dst_alpha" ) )
		return GLSTATE_SRCBLEND_ONE_MINUS_DST_ALPHA;
	return GLSTATE_SRCBLEND_ONE;
}

static _inline int Shaderpass_DstBlendBits( char *token )
{
	if( !strcmp( token, "gl_zero" ) )
		return GLSTATE_DSTBLEND_ZERO;
	if( !strcmp( token, "gl_one" ) )
		return GLSTATE_DSTBLEND_ONE;
	if( !strcmp( token, "gl_src_color" ) )
		return GLSTATE_DSTBLEND_SRC_COLOR;
	if( !strcmp( token, "gl_one_minus_src_color" ) )
		return GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR;
	if( !strcmp( token, "gl_src_alpha" ) )
		return GLSTATE_DSTBLEND_SRC_ALPHA;
	if( !strcmp( token, "gl_one_minus_src_alpha" ) )
		return GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	if( !strcmp( token, "gl_dst_alpha" ) )
		return GLSTATE_DSTBLEND_DST_ALPHA;
	if( !strcmp( token, "gl_one_minus_dst_alpha" ) )
		return GLSTATE_DSTBLEND_ONE_MINUS_DST_ALPHA;
	return GLSTATE_DSTBLEND_ONE;
}

static void Shaderpass_BlendFunc( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );

	pass->glState &= ~(GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK);
	if( !strcmp( token, "blend" ))
		pass->glState |= GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	else if( !strcmp( token, "filter" ) )
		pass->glState |= GLSTATE_SRCBLEND_DST_COLOR|GLSTATE_DSTBLEND_ZERO;
	else if( !strcmp( token, "add" ) )
		pass->glState |= GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
	else
	{
		pass->glState |= Shaderpass_SrcBlendBits( token );
		pass->glState |= Shaderpass_DstBlendBits( Shader_ParseString( ptr ));
	}
}

static void Shaderpass_AlphaFunc( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );

	pass->glState &= ~(GLSTATE_ALPHAFUNC);
	if( !strcmp( token, "gt0" ) )
		pass->glState |= GLSTATE_AFUNC_GT0;
	else if( !strcmp( token, "lt128" ) )
		pass->glState |= GLSTATE_AFUNC_LT128;
	else if( !strcmp( token, "ge128" ) )
		pass->glState |= GLSTATE_AFUNC_GE128;
}

static void Shaderpass_DepthFunc( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );

	pass->glState &= ~GLSTATE_DEPTHFUNC_EQ;
	if( !strcmp( token, "equal" ))
		pass->glState |= GLSTATE_DEPTHFUNC_EQ;
}

static void Shaderpass_DepthWrite( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	shader->flags |= SHADER_DEPTHWRITE;
	pass->glState |= GLSTATE_DEPTHWRITE;
}

static void Shaderpass_TcMod( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	int	i;
	tcMod_t	*tcmod;
	char	*token;

	if( pass->numtcMods == MAX_SHADER_TCMODS )
	{
		MsgDev( D_WARN, "shader %s has too many tcmods\n", shader->name );
		Shader_SkipLine( ptr );
		return;
	}

	tcmod = &pass->tcMods[pass->numtcMods];

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "rotate" ) )
	{
		tcmod->args[0] = -Shader_ParseFloat( ptr ) / 360.0f;
		if( !tcmod->args[0] )
			return;
		tcmod->type = TCMOD_ROTATE;
	}
	else if( !strcmp( token, "scale" ) )
	{
		Shader_ParseVector( ptr, tcmod->args, 2 );
		tcmod->type = TCMOD_SCALE;
	}
	else if( !strcmp( token, "scroll" ) )
	{
		Shader_ParseVector( ptr, tcmod->args, 2 );
		tcmod->type = TCMOD_SCROLL;
	}
	else if( !strcmp( token, "stretch" ) )
	{
		waveFunc_t func;

		Shader_ParseFunc( ptr, &func );

		tcmod->args[0] = func.type;
		for( i = 1; i < 5; i++ )
			tcmod->args[i] = func.args[i-1];
		tcmod->type = TCMOD_STRETCH;
	}
	else if( !strcmp( token, "transform" ) )
	{
		Shader_ParseVector( ptr, tcmod->args, 6 );
		tcmod->args[4] = tcmod->args[4] - floor( tcmod->args[4] );
		tcmod->args[5] = tcmod->args[5] - floor( tcmod->args[5] );
		tcmod->type = TCMOD_TRANSFORM;
	}
	else if( !strcmp( token, "turb" ) )
	{
		Shader_ParseVector( ptr, tcmod->args, 4 );
		tcmod->type = TCMOD_TURB;
	}
	else
	{
		Shader_SkipLine( ptr );
		return;
	}

	r_currentPasses[shader->num_stages].numtcMods++;
}

static void Shaderpass_TcGen( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	char *token;

	token = Shader_ParseString( ptr );
	if( !strcmp( token, "base" ) )
		pass->tcgen = TCGEN_BASE;
	else if( !strcmp( token, "lightmap" ) )
		pass->tcgen = TCGEN_LIGHTMAP;
	else if( !strcmp( token, "environment" ) )
		pass->tcgen = TCGEN_ENVIRONMENT;
	else if( !strcmp( token, "vector" ) )
	{
		pass->tcgen = TCGEN_VECTOR;
		Shader_ParseVector( ptr, &pass->tcgenVec[0], 4 );
		Shader_ParseVector( ptr, &pass->tcgenVec[4], 4 );
	}
	else if( !strcmp( token, "reflection" ) )
		pass->tcgen = TCGEN_REFLECTION;
	else if( !strcmp( token, "cellshade" ) )
		pass->tcgen = TCGEN_REFLECTION_CELLSHADE;
}

static void Shaderpass_Detail( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	pass->flags |= SHADERSTAGE_DETAIL;
}

static void Shaderpass_RenderMode( ref_shader_t *shader, ref_stage_t *pass, const char **ptr )
{
	// does nothing
}

static const shaderkey_t shaderpasskeys[] =
{
	{ "rgbgen", Shaderpass_RGBGen },
	{ "blendfunc", Shaderpass_BlendFunc },
	{ "depthfunc", Shaderpass_DepthFunc },
	{ "depthwrite",	Shaderpass_DepthWrite },
	{ "alphafunc", Shaderpass_AlphaFunc },
	{ "tcmod", Shaderpass_TcMod },
	{ "map", Shaderpass_Map },
	{ "animmap", Shaderpass_AnimMap },
	{ "cubemap", Shaderpass_CubeMap },
	{ "shadecubemap", Shaderpass_ShadeCubeMap },
	{ "videomap", Shaderpass_VideoMap },
	{ "clampmap", Shaderpass_ClampMap },
	{ "animclampmap", Shaderpass_AnimClampMap },
	{ "normalmap", Shaderpass_NormalMap },
	{ "material", Shaderpass_Material },
	{ "distortion",	Shaderpass_Distortion },
	{ "tcgen", Shaderpass_TcGen },
	{ "alphagen", Shaderpass_AlphaGen },
	{ "detail", Shaderpass_Detail },
	{ "renderMode", Shaderpass_RenderMode },
	{ NULL,	NULL }
};

// ===============================================================

/*
===============
R_ShaderList_f
===============
*/
void R_ShaderList_f( void )
{
	int i;
	ref_shader_t *shader;

	Msg( "------------------\n" );
	for( i = 0, shader = r_shaders; i < r_numShaders; i++, shader++ )
		Msg( " %2i %2i: %s\n", shader->num_stages, shader->sort, shader->name );
	Msg( "%i shaders total\n", r_numShaders );
}

/*
===============
R_ShaderDump_f
===============
*/
void R_ShaderDump_f( void )
{
	char backup, *start;
	const char *name, *ptr;
	shadercache_t *cache;
	
	if( (Cmd_Argc() < 2) && !r_debug_surface )
	{
		Msg( "Usage: %s [name]\n", Cmd_Argv(0) );
		return;
	}

	if( Cmd_Argc() < 2 )
		name = r_debug_surface->shader->name;
	else
		name = Cmd_Argv( 1 );

	Shader_GetCache( name, &cache );
	if( !cache )
	{
		Msg( "Could not find shader %s in cache.\n", name );
		return;
	}

	start = cache->buffer + cache->offset;

	// temporarily hack in the zero-char
	ptr = start;
	Shader_SkipBlock( &ptr );
	backup = cache->buffer[ptr - cache->buffer];
	cache->buffer[ptr - cache->buffer] = '\0';

	Msg( "Found in %s:\n\n", cache->filename );
	Msg( "^2%s%s\n", name, start );

	cache->buffer[ptr - cache->buffer] = backup;
}

void R_InitShaders( void )
{
	search_t	*t;
	int	i;

	r_shadersmempool = Mem_AllocPool( "Shaders" );

	t = FS_Search( "scripts/*.shader", true );
	if( !t )
	{
		Mem_FreePool( &r_shadersmempool );
		Host_Error( "Could not find any shaders!\n" );
	}

	Mem_Set( shadercache_hash, 0, sizeof( shadercache_t * )*SHADERS_HASH_SIZE );

	for( i = 0; i < t->numfilenames; i++ )
		Shader_MakeCache( t->filenames[i] );

	if( t ) Mem_Free( t );
}

static void Shader_MakeCache( const char *filename )
{
	int		size;
	uint		key;
	char		*buf, *temp = NULL;
	const char	*token, *ptr;
	shadercache_t	*cache;
	byte		*cacheMemBuf;
	size_t		cacheMemSize;

	temp = FS_LoadFile( filename, &size );
	if( !temp || size <= 0 ) goto done;

	size = COM_Compress( temp );
	if( !size ) goto done;

	buf = Mem_Alloc( r_shadersmempool, size + 1 );
	Mem_Copy( buf, temp, size );
	Mem_Free( temp );
	temp = NULL;

	// calculate buffer size to allocate our cache objects all at once (we may leak
	// insignificantly here because of duplicate entries)
	for( ptr = buf, cacheMemSize = 0; ptr; )
	{
		token = COM_ParseExt( &ptr, true );
		if( !token[0] )
			break;

		cacheMemSize += sizeof( shadercache_t ) + com.strlen( token ) + 1;
		Shader_SkipBlock( &ptr );
	}

	if( !cacheMemSize )
	{
		Shader_Free( buf );
		goto done;
	}

	cacheMemBuf = Shader_Malloc( cacheMemSize );

	for( ptr = buf; ptr; )
	{
		token = COM_ParseExt( &ptr, true );
		if( !token[0] )
			break;

		key = Shader_GetCache( token, &cache );
		if( cache )
			goto set_path_and_offset;

		cache = ( shadercache_t * )cacheMemBuf; cacheMemBuf += sizeof( shadercache_t ) + strlen( token ) + 1;
		cache->hash_next = shadercache_hash[key];
		cache->name = ( char * )( (byte *)cache + sizeof( shadercache_t ) );
		strcpy( cache->name, token );
		shadercache_hash[key] = cache;

set_path_and_offset:
		cache->filename = com.stralloc( r_shadersmempool, filename, __FILE__, __LINE__ );
		cache->buffer = buf;
		cache->offset = ptr - buf;

		Shader_SkipBlock( &ptr );
	}

done:
	if( temp ) Mem_Free( temp );
}

static unsigned int Shader_GetCache( const char *name, shadercache_t **cache )
{
	unsigned int key;
	shadercache_t *c;

	*cache = NULL;

	key = Com_HashKey( name, SHADERS_HASH_SIZE );
	for( c = shadercache_hash[key]; c; c = c->hash_next )
	{
		if( !com.stricmp( c->name, name ) )
		{
			*cache = c;
			return key;
		}
	}

	return key;
}

void Shader_FreeShader( ref_shader_t *shader )
{
	int i;
	int shaderNum;
	ref_stage_t *pass;

	shaderNum = shader - r_shaders;
	if( ( shader->flags & SHADER_SKYPARMS ) && r_skydomes[shaderNum] )
	{
		R_FreeSkydome( r_skydomes[shaderNum] );
		r_skydomes[shaderNum] = NULL;
	}

	if( shader->flags & SHADER_VIDEOMAP )
	{
		for( i = 0, pass = shader->stages; i < shader->num_stages; i++, pass++ )
			Shader_FreePassCinematics( pass );
	}
	Shader_Free( shader->name );
}

void R_ShutdownShaders( void )
{
	int i;
	ref_shader_t *shader;

	if( !r_shadersmempool )
		return;

	for( i = 0, shader = r_shaders; i < r_numShaders; i++, shader++ )
		Shader_FreeShader( shader );

	Mem_FreePool( &r_shadersmempool );

	r_numShaders = 0;

	shaderPaths = NULL;
	memset( r_shaders, 0, sizeof( r_shaders ) );
	memset( shaders_hash, 0, sizeof( shaders_hash ) );
	memset( shadercache_hash, 0, sizeof( shadercache_hash ) );
}

void Shader_SetBlendmode( ref_stage_t *pass )
{
	int blendsrc, blenddst;

	if( pass->flags & SHADERSTAGE_BLENDMODE )
		return;
	if( !pass->textures[0] && !( pass->flags & ( SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT )))
		return;

	if(!( pass->glState & (GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK)))
	{
		if(( pass->rgbGen.type == RGBGEN_IDENTITY ) && ( pass->alphaGen.type == ALPHAGEN_IDENTITY ))
			pass->flags |= SHADERSTAGE_BLEND_REPLACE;
		else pass->flags |= SHADERSTAGE_BLEND_MODULATE;
		return;
	}

	blendsrc = pass->glState & GLSTATE_SRCBLEND_MASK;
	blenddst = pass->glState & GLSTATE_DSTBLEND_MASK;

	if( blendsrc == GLSTATE_SRCBLEND_ONE && blenddst == GLSTATE_DSTBLEND_ZERO )
		pass->flags |= SHADERSTAGE_BLEND_MODULATE;
	else if(( blendsrc == GLSTATE_SRCBLEND_ZERO && blenddst == GLSTATE_DSTBLEND_SRC_COLOR ) || ( blendsrc == GLSTATE_SRCBLEND_DST_COLOR && blenddst == GLSTATE_DSTBLEND_ZERO ))
		pass->flags |= SHADERSTAGE_BLEND_MODULATE;
	else if( blendsrc == GLSTATE_SRCBLEND_ONE && blenddst == GLSTATE_DSTBLEND_ONE )
		pass->flags |= SHADERSTAGE_BLEND_ADD;
	else if( blendsrc == GLSTATE_SRCBLEND_SRC_ALPHA && blenddst == GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA )
		pass->flags |= SHADERSTAGE_BLEND_DECAL;
}

static void Shader_Readpass( ref_shader_t *shader, const char **ptr )
{
	int n = shader->num_stages;
	const char *token;
	ref_stage_t *pass;

	if( n == MAX_SHADER_STAGES )
	{
		MsgDev( D_WARN, "shader %s has too many passes\n", shader->name );

		while( ptr )
		{	// skip
			token = COM_ParseExt( ptr, true );
			if( !token[0] || token[0] == '}' )
				break;
		}
		return;
	}

	// Set defaults
	pass = &r_currentPasses[n];
	memset( pass, 0, sizeof( ref_stage_t ) );
	pass->rgbGen.type = RGBGEN_UNKNOWN;
	pass->rgbGen.args = r_currentRGBgenArgs[n];
	pass->rgbGen.func = &r_currentRGBgenFuncs[n];
	pass->alphaGen.type = ALPHAGEN_UNKNOWN;
	pass->alphaGen.args = r_currentAlphagenArgs[n];
	pass->alphaGen.func = &r_currentAlphagenFuncs[n];
	pass->tcgenVec = r_currentTcGen[n][0];
	pass->tcgen = TCGEN_BASE;
	pass->tcMods = r_currentTcmods[n];

	while( ptr )
	{
		token = COM_ParseExt( ptr, true );

		if( !token[0] )
			break;
		else if( token[0] == '}' )
			break;
		else if( Shader_Parsetok( shader, pass, shaderpasskeys, token, ptr ) )
			break;
	}

	if((( pass->glState & GLSTATE_SRCBLEND_MASK ) == GLSTATE_SRCBLEND_ONE ) && (( pass->glState & GLSTATE_DSTBLEND_MASK ) == GLSTATE_DSTBLEND_ZERO ))
	{
		pass->glState &= ~(GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK);
		pass->flags |= SHADERSTAGE_BLEND_MODULATE;
	}

	if(!( pass->glState & (GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK)))
		pass->glState |= GLSTATE_DEPTHWRITE;
	if( pass->glState & GLSTATE_DEPTHWRITE )
		shader->flags |= SHADER_DEPTHWRITE;

	switch( pass->rgbGen.type )
	{
	case RGBGEN_IDENTITY_LIGHTING:
	case RGBGEN_IDENTITY:
	case RGBGEN_CONST:
	case RGBGEN_WAVE:
	case RGBGEN_ENTITY:
	case RGBGEN_ONE_MINUS_ENTITY:
	case RGBGEN_LIGHTING_DIFFUSE_ONLY:
	case RGBGEN_LIGHTING_AMBIENT_ONLY:
	case RGBGEN_CUSTOM:
	case RGBGEN_OUTLINE:
	case RGBGEN_UNKNOWN:   // assume RGBGEN_IDENTITY or RGBGEN_IDENTITY_LIGHTING
		switch( pass->alphaGen.type )
		{
		case ALPHAGEN_UNKNOWN:
		case ALPHAGEN_IDENTITY:
		case ALPHAGEN_CONST:
		case ALPHAGEN_WAVE:
		case ALPHAGEN_ENTITY:
		case ALPHAGEN_OUTLINE:
			pass->flags |= SHADERSTAGE_NOCOLORARRAY;
			break;
		default:
			break;
		}

		break;
	default:
		break;
	}

	if(( shader->flags & SHADER_SKYPARMS ) && ( shader->flags & SHADER_DEPTHWRITE ))
	{
		if( pass->glState & GLSTATE_DEPTHWRITE )
			pass->glState &= ~GLSTATE_DEPTHWRITE;
	}
	shader->num_stages++;
}

static bool Shader_Parsetok( ref_shader_t *shader, ref_stage_t *pass, const shaderkey_t *keys, const char *token, const char **ptr )
{
	const shaderkey_t *key;

	for( key = keys; key->keyword != NULL; key++ )
	{
		if( !com.stricmp( token, key->keyword ) )
		{
			if( key->func )
				key->func( shader, pass, ptr );
			if( *ptr && **ptr == '}' )
			{
				*ptr = *ptr + 1;
				return true;
			}
			return false;
		}
	}

	Shader_SkipLine( ptr );

	return false;
}

void Shader_SetFeatures( ref_shader_t *s )
{
	int i;
	ref_stage_t *pass;

	if( s->numDeforms )
		s->features |= MF_DEFORMVS;
	if( s->flags & SHADER_AUTOSPRITE )
		s->features |= MF_NOCULL;

	for( i = 0; i < s->numDeforms; i++ )
	{
		switch( s->deforms[i].type )
		{
		case DEFORM_BULGE:
			s->features |= MF_STCOORDS;
		case DEFORM_WAVE:
		case DEFORM_NORMAL:
			s->features |= MF_NORMALS;
			break;
		case DEFORM_MOVE:
			break;
		default:
			break;
		}
	}

	for( i = 0, pass = s->stages; i < s->num_stages; i++, pass++ )
	{
		if( pass->program && ( pass->program_type == PROGRAM_TYPE_MATERIAL || pass->program_type == PROGRAM_TYPE_DISTORTION ) )
			s->features |= MF_NORMALS|MF_SVECTORS|MF_LMCOORDS|MF_ENABLENORMALS;

		switch( pass->rgbGen.type )
		{
		case RGBGEN_LIGHTING_DIFFUSE:
			s->features |= MF_NORMALS;
			break;
		case RGBGEN_VERTEX:
		case RGBGEN_ONE_MINUS_VERTEX:
		case RGBGEN_EXACT_VERTEX:
			s->features |= MF_COLORS;
			break;
		}

		switch( pass->alphaGen.type )
		{
		case ALPHAGEN_SPECULAR:
		case ALPHAGEN_DOT:
		case ALPHAGEN_ONE_MINUS_DOT:
			s->features |= MF_NORMALS;
			break;
		case ALPHAGEN_VERTEX:
		case ALPHAGEN_ONE_MINUS_VERTEX:
			s->features |= MF_COLORS;
			break;
		}

		switch( pass->tcgen )
		{
		case TCGEN_LIGHTMAP:
			s->features |= MF_LMCOORDS;
			break;
		case TCGEN_ENVIRONMENT:
			s->features |= MF_NORMALS;
			break;
		case TCGEN_REFLECTION:
		case TCGEN_REFLECTION_CELLSHADE:
			s->features |= MF_NORMALS|MF_ENABLENORMALS;
			break;
		default:
			s->features |= MF_STCOORDS;
			break;
		}
	}
}

void Shader_Finish( ref_shader_t *s )
{
	int		i, j;
	const char	*oldname = s->name;
	size_t		size = com.strlen( oldname ) + 1;
	ref_stage_t	*pass;
	byte		*buffer;

	// if the portal capture texture hasn't been initialized yet, do that
	if(( s->flags & SHADER_PORTAL_CAPTURE1 ) && !tr.portaltexture1 )
		R_InitPortalTexture( &tr.portaltexture1, 1, glState.width, glState.height );
	if(( s->flags & SHADER_PORTAL_CAPTURE2 ) && !tr.portaltexture2 )
		R_InitPortalTexture( &tr.portaltexture2, 2, glState.width, glState.height );

	if( !s->num_stages && !s->sort )
	{
		if( s->numDeforms )
		{
			s->deforms = Shader_Malloc( s->numDeforms * sizeof( deform_t ));
			memcpy( s->deforms, r_currentDeforms, s->numDeforms * sizeof( deform_t ));
		}
		if( s->flags & SHADER_PORTAL )
			s->sort = SORT_PORTAL;
		else s->sort = SORT_ADDITIVE;
	}

	if( ( s->flags & SHADER_POLYGONOFFSET ) && !s->sort )
		s->sort = SORT_DECAL;

	size += s->numDeforms * sizeof( deform_t ) + s->num_stages * sizeof( ref_stage_t );
	for( i = 0, pass = r_currentPasses; i < s->num_stages; i++, pass++ )
	{
		// rgbgen args
		if( pass->rgbGen.type == RGBGEN_WAVE ||
			pass->rgbGen.type == RGBGEN_CONST ||
			pass->rgbGen.type == RGBGEN_CUSTOM )
			size += sizeof( float ) * 3;

		// alphagen args
		if( pass->alphaGen.type == ALPHAGEN_PORTAL ||
			pass->alphaGen.type == ALPHAGEN_SPECULAR ||
			pass->alphaGen.type == ALPHAGEN_CONST ||
			pass->alphaGen.type == ALPHAGEN_DOT || pass->alphaGen.type == ALPHAGEN_ONE_MINUS_DOT )
			size += sizeof( float ) * 2;

		if( pass->rgbGen.type == RGBGEN_WAVE )
			size += sizeof( waveFunc_t );
		if( pass->alphaGen.type == ALPHAGEN_WAVE )
			size += sizeof( waveFunc_t );
		size += pass->numtcMods * sizeof( tcMod_t );
		if( pass->tcgen == TCGEN_VECTOR )
			size += sizeof( vec4_t ) * 2;
	}

	buffer = Shader_Malloc( size );

	s->name = ( char * )buffer; buffer += strlen( oldname ) + 1;
	s->stages = ( ref_stage_t * )buffer; buffer += s->num_stages * sizeof( ref_stage_t );

	strcpy( s->name, oldname );
	memcpy( s->stages, r_currentPasses, s->num_stages * sizeof( ref_stage_t ) );

	for( i = 0, pass = s->stages; i < s->num_stages; i++, pass++ )
	{
		if( pass->rgbGen.type == RGBGEN_WAVE ||
			pass->rgbGen.type == RGBGEN_CONST ||
			pass->rgbGen.type == RGBGEN_CUSTOM )
		{
			pass->rgbGen.args = ( float * )buffer; buffer += sizeof( float ) * 3;
			memcpy( pass->rgbGen.args, r_currentPasses[i].rgbGen.args, sizeof( float ) * 3 );
		}

		if( pass->alphaGen.type == ALPHAGEN_PORTAL ||
			pass->alphaGen.type == ALPHAGEN_SPECULAR ||
			pass->alphaGen.type == ALPHAGEN_CONST ||
			pass->alphaGen.type == ALPHAGEN_DOT || pass->alphaGen.type == ALPHAGEN_ONE_MINUS_DOT )
		{
			pass->alphaGen.args = ( float * )buffer; buffer += sizeof( float ) * 2;
			memcpy( pass->alphaGen.args, r_currentPasses[i].alphaGen.args, sizeof( float ) * 2 );
		}

		if( pass->rgbGen.type == RGBGEN_WAVE )
		{
			pass->rgbGen.func = ( waveFunc_t * )buffer; buffer += sizeof( waveFunc_t );
			memcpy( pass->rgbGen.func, r_currentPasses[i].rgbGen.func, sizeof( waveFunc_t ) );
		}
		else
		{
			pass->rgbGen.func = NULL;
		}

		if( pass->alphaGen.type == ALPHAGEN_WAVE )
		{
			pass->alphaGen.func = ( waveFunc_t * )buffer; buffer += sizeof( waveFunc_t );
			memcpy( pass->alphaGen.func, r_currentPasses[i].alphaGen.func, sizeof( waveFunc_t ) );
		}
		else
		{
			pass->alphaGen.func = NULL;
		}

		if( pass->numtcMods )
		{
			pass->tcMods = ( tcMod_t * )buffer; buffer += r_currentPasses[i].numtcMods * sizeof( tcMod_t );
			pass->numtcMods = r_currentPasses[i].numtcMods;
			Mem_Copy( pass->tcMods, r_currentPasses[i].tcMods, r_currentPasses[i].numtcMods * sizeof( tcMod_t ) );
		}

		if( pass->tcgen == TCGEN_VECTOR )
		{
			pass->tcgenVec = ( vec_t * )buffer; buffer += sizeof( vec4_t ) * 2;
			Vector4Copy( &r_currentPasses[i].tcgenVec[0], &pass->tcgenVec[0] );
			Vector4Copy( &r_currentPasses[i].tcgenVec[4], &pass->tcgenVec[4] );
		}
	}

	if( s->numDeforms )
	{
		s->deforms = ( deform_t * )buffer;
		Mem_Copy( s->deforms, r_currentDeforms, s->numDeforms * sizeof( deform_t ));
	}

	if( s->flags & SHADER_AUTOSPRITE )
		s->flags &= ~( SHADER_CULL_FRONT|SHADER_CULL_BACK );
	if( r_shaderHasDlightPass )
		s->flags |= SHADER_NO_MODULATIVE_DLIGHTS;

	for( i = 0, pass = s->stages; i < s->num_stages; i++, pass++ )
	{
		if( pass->cinHandle )
			s->flags |= SHADER_VIDEOMAP;
		if( pass->flags & SHADERSTAGE_LIGHTMAP )
			s->flags |= SHADER_HASLIGHTMAP;
		if( pass->program )
		{
			s->flags |= SHADER_NO_MODULATIVE_DLIGHTS;
			if( pass->program_type == PROGRAM_TYPE_MATERIAL )
				s->flags |= SHADER_MATERIAL;
			if( r_shaderHasDlightPass )
				pass->textures[5] = ( (texture_t *)1); // HACKHACK no dlights
		}
		Shader_SetBlendmode( pass );
	}

	for( i = 0, pass = s->stages; i < s->num_stages; i++, pass++ )
	{
		if( !( pass->glState & ( GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK )))
			break;
	}

	// all passes have blendfuncs
	if( i == s->num_stages )
	{
		int opaque;

		opaque = -1;
		for( i = 0, pass = s->stages; i < s->num_stages; i++, pass++ )
		{
			if((( pass->glState & GLSTATE_SRCBLEND_MASK ) == GLSTATE_SRCBLEND_ONE ) && (( pass->glState & GLSTATE_DSTBLEND_MASK ) == GLSTATE_DSTBLEND_ZERO ))
				opaque = i;

			if( pass->rgbGen.type == RGBGEN_UNKNOWN )
			{
				if( !s->fog_dist && !( pass->flags & SHADERSTAGE_LIGHTMAP ))
					pass->rgbGen.type = RGBGEN_IDENTITY_LIGHTING;
				else pass->rgbGen.type = RGBGEN_IDENTITY;
			}

			if( pass->alphaGen.type == ALPHAGEN_UNKNOWN )
			{
				if( pass->rgbGen.type == RGBGEN_VERTEX /* || pass->rgbGen.type == RGBGEN_EXACT_VERTEX*/ )
					pass->alphaGen.type = ALPHAGEN_VERTEX;
				else
					pass->alphaGen.type = ALPHAGEN_IDENTITY;
			}
		}

		if(!( s->flags & SHADER_SKYPARMS ) && !s->sort )
		{
			if( s->flags & SHADER_DEPTHWRITE || ( opaque != -1 && s->stages[opaque].glState & GLSTATE_ALPHAFUNC ))
				s->sort = SORT_ALPHATEST;
			else if( opaque == -1 )
				s->sort = SORT_ADDITIVE;
			else s->sort = SORT_OPAQUE;
		}
	}
	else
	{
		ref_stage_t *sp;

		for( j = 0, sp = s->stages; j < s->num_stages; j++, sp++ )
		{
			if( sp->rgbGen.type == RGBGEN_UNKNOWN )
			{
				if( sp->glState & GLSTATE_ALPHAFUNC && !( j && s->stages[j-1].flags & SHADERSTAGE_LIGHTMAP ))  // FIXME!
					sp->rgbGen.type = RGBGEN_IDENTITY_LIGHTING;
				else sp->rgbGen.type = RGBGEN_IDENTITY;
			}

			if( sp->alphaGen.type == ALPHAGEN_UNKNOWN )
			{
				if( sp->rgbGen.type == RGBGEN_VERTEX /* || sp->rgbGen.type == RGBGEN_EXACT_VERTEX*/ )
					sp->alphaGen.type = ALPHAGEN_VERTEX;
				else
					sp->alphaGen.type = ALPHAGEN_IDENTITY;
			}
		}

		if( !s->sort )
		{
			if( pass->glState & GLSTATE_ALPHAFUNC )
				s->sort = SORT_ALPHATEST;
		}

		if( !( pass->glState & GLSTATE_DEPTHWRITE ) && !( s->flags & SHADER_SKYPARMS ))
		{
			pass->glState |= GLSTATE_DEPTHWRITE;
			s->flags |= SHADER_DEPTHWRITE;
		}
	}

	if( !s->sort ) s->sort = SORT_OPAQUE;

	if(( s->flags & SHADER_SKYPARMS ) && ( s->flags & SHADER_DEPTHWRITE ))
		s->flags &= ~SHADER_DEPTHWRITE;

	Shader_SetFeatures( s );
}

void R_UploadCinematicShader( const ref_shader_t *shader )
{
	int j;
	ref_stage_t *pass;

	// upload cinematics
	for( j = 0, pass = shader->stages; j < shader->num_stages; j++, pass++ )
	{
		if( pass->cinHandle )
			pass->textures[0] = R_UploadCinematics( pass->cinHandle );
	}
}

void R_DeformvBBoxForShader( const ref_shader_t *shader, vec3_t ebbox )
{
	int dv;

	if( !shader )
		return;
	for( dv = 0; dv < shader->numDeforms; dv++ )
	{
		switch( shader->deforms[dv].type )
		{
		case DEFORM_WAVE:
			ebbox[0] = max( ebbox[0], fabs( shader->deforms[dv].func.args[1] ) + shader->deforms[dv].func.args[0] );
			ebbox[1] = ebbox[0];
			ebbox[2] = ebbox[0];
			break;
		default:
			break;
		}
	}
}

ref_shader_t *R_LoadShader( const char *name, int type, bool forceDefault, int addFlags, int ignoreType )
{
	int		i;
	uint		key, length;
	string		shortname;
	ref_shader_t	*s;
	shadercache_t	*cache;
	ref_stage_t	*pass;
	texture_t		*materialImages[MAX_STAGE_TEXTURES];

	if( !name || !name[0] )
		return NULL;

	if( r_numShaders == MAX_SHADERS )
		Host_Error( "R_LoadShader: Shader limit exceeded\n" );

	for( i = ( name[0] == '/' || name[0] == '\\' ), length = 0; name[i] && ( length < sizeof( shortname )-1 ); i++ )
	{
		if( name[i] == '\\' )
			shortname[length++] = '/';
		else
			shortname[length++] = com.tolower( name[i] );
	}

	if( !length ) return NULL;
	shortname[length] = 0;

	// test if already loaded
	key = Com_HashKey( shortname, SHADERS_HASH_SIZE );
	for( s = shaders_hash[key]; s; s = s->nextHash )
	{
		if( !strcmp( s->name, shortname ) && ( s->type != ignoreType ))
			return s;
	}

	s = &r_shaders[r_numShaders++];
	memset( s, 0, sizeof( ref_shader_t ) );
	s->name = shortname;
	s->offsetmapping_scale = 1;

	if( ignoreType == SHADER_UNKNOWN )
		forceDefault = true;

	r_shaderNoMipMaps =	false;
	r_shaderNoPicMip = false;
	r_shaderNoCompress = false;
	r_shaderHasDlightPass = false;

	cache = NULL;
	if( !forceDefault )
		Shader_GetCache( shortname, &cache );

	// the shader is in the shader scripts
	if( cache )
	{
		const char *ptr, *token;

		MsgDev( D_NOTE, "Loading shader %s from cache...\n", name );

		// set defaults
		s->type = SHADER_UNKNOWN;
		s->flags = SHADER_CULL_FRONT;
		s->features = MF_NONE;

		ptr = cache->buffer + cache->offset;
		token = COM_ParseExt( &ptr, true );

		if( !ptr || token[0] != '{' )
			goto create_default;

		while( ptr )
		{
			token = COM_ParseExt( &ptr, true );

			if( !token[0] )
				break;
			else if( token[0] == '}' )
				break;
			else if( token[0] == '{' )
				Shader_Readpass( s, &ptr );
			else if( Shader_Parsetok( s, NULL, shaderkeys, token, &ptr ) )
				break;
		}

		Shader_Finish( s );
	}
	else
	{          
		// make a default shader
		switch( type )
		{
		case SHADER_VERTEX:
			s->type = SHADER_VERTEX;
			s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT|SHADER_NO_MODULATIVE_DLIGHTS;
			s->features = MF_STCOORDS|MF_COLORS;
			s->sort = SORT_OPAQUE;
			s->num_stages = 3;
			s->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * s->num_stages );
			strcpy( s->name, shortname );
			s->stages = ( ref_stage_t * )( ( byte * )s->name + length + 1 );
			pass = &s->stages[0];
			pass->textures[0] = tr.whiteTexture;
			pass->flags = SHADERSTAGE_BLEND_MODULATE;
			pass->glState = GLSTATE_DEPTHWRITE/*|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ZERO*/;
			pass->tcgen = TCGEN_BASE;
			pass->rgbGen.type = RGBGEN_VERTEX;
			pass->alphaGen.type = ALPHAGEN_IDENTITY;
			pass = &s->stages[1];
			pass->flags = SHADERSTAGE_DLIGHT|SHADERSTAGE_BLEND_ADD;
			pass->glState = GLSTATE_DEPTHFUNC_EQ|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
			pass->tcgen = TCGEN_BASE;
			pass = &s->stages[2];
			pass->flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_MODULATE;
			pass->glState = GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_SRC_COLOR;
			pass->tcgen = TCGEN_BASE;
			pass->textures[0] = Shader_FindImage( s, shortname, addFlags );
			pass->rgbGen.type = RGBGEN_IDENTITY;
			pass->alphaGen.type = ALPHAGEN_IDENTITY;
			break;
		case SHADER_FLARE:
			s->type = SHADER_FLARE;
			s->features = MF_STCOORDS|MF_COLORS;
			s->sort = SORT_ADDITIVE;
			s->num_stages = 1;
			s->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * s->num_stages );
			strcpy( s->name, shortname );
			s->stages = ( ref_stage_t * )( ( byte * )s->name + length + 1 );
			pass = &s->stages[0];
			pass->flags = SHADERSTAGE_BLEND_ADD;
			pass->glState = GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
			pass->textures[0] = Shader_FindImage( s, shortname, addFlags );
			pass->rgbGen.type = RGBGEN_VERTEX;
			pass->alphaGen.type = ALPHAGEN_IDENTITY;
			pass->tcgen = TCGEN_BASE;
			break;
		case SHADER_MD3:
			s->type = SHADER_MD3;
			s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT;
			s->features = MF_STCOORDS|MF_NORMALS;
			s->sort = SORT_OPAQUE;
			s->num_stages = 1;
			s->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * s->num_stages );
			strcpy( s->name, shortname );
			s->stages = ( ref_stage_t * )( ( byte * )s->name + length + 1 );
			pass = &s->stages[0];
			pass->flags = SHADERSTAGE_BLEND_MODULATE;
			pass->glState = GLSTATE_DEPTHWRITE;
			pass->rgbGen.type = RGBGEN_LIGHTING_DIFFUSE;
			pass->alphaGen.type = ALPHAGEN_IDENTITY;
			pass->tcgen = TCGEN_BASE;
			pass->textures[0] = Shader_FindImage( s, shortname, addFlags );

			// load default GLSL program if there's a bumpmap was found
			if( ( r_lighting_models_followdeluxe->integer ? mapConfig.deluxeMappingEnabled : GL_Support( R_SHADER_GLSL100_EXT ))
				&& Shaderpass_LoadMaterial( &materialImages[0], &materialImages[1], &materialImages[2], shortname, addFlags, 1 ) )
			{
				pass->rgbGen.type = RGBGEN_IDENTITY;
				pass->program = DEFAULT_GLSL_PROGRAM;
				pass->program_type = PROGRAM_TYPE_MATERIAL;
				pass->textures[1] = materialImages[0]; // normalmap
				pass->textures[2] = materialImages[1]; // glossmap
				pass->textures[3] = materialImages[2]; // decalmap
				s->features |= MF_SVECTORS|MF_ENABLENORMALS;
				s->flags |= SHADER_MATERIAL;
			}
			break;
		case SHADER_FONT:
			s->type = SHADER_FONT;
			s->features = MF_STCOORDS|MF_COLORS;
			s->sort = SORT_ADDITIVE;
			s->num_stages = 1;
			s->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * s->num_stages );
			strcpy( s->name, shortname );
			s->stages = ( ref_stage_t * )(( byte * )s->name + length + 1 );
			pass = &s->stages[0];
			pass->flags = SHADERSTAGE_BLEND_MODULATE;
			pass->glState = GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			pass->textures[0] = R_FindTexture( shortname, NULL, 0, addFlags|TF_CLAMP|TF_NOMIPMAP|TF_NOPICMIP );
			if( !pass->textures[0] )
			{
				// don't let user set invalid font
				MsgDev( D_WARN, "couldn't find texture for shader '%s', using default...\n", s->name );
				pass->textures[0] = tr.defaultConchars;
			}
			pass->rgbGen.type = RGBGEN_VERTEX;
			pass->alphaGen.type = ALPHAGEN_VERTEX;
			pass->tcgen = TCGEN_BASE;
			break;
		case SHADER_NOMIP:
			s->type = SHADER_NOMIP;
			s->features = MF_STCOORDS|MF_COLORS;
			s->sort = SORT_ADDITIVE;
			s->num_stages = 1;
			s->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * s->num_stages );
			strcpy( s->name, shortname );
			s->stages = ( ref_stage_t * )( ( byte * )s->name + length + 1 );
			pass = &s->stages[0];
			pass->flags = SHADERSTAGE_BLEND_MODULATE/*|SHADERSTAGE_NOCOLORARRAY*/;
			pass->glState = GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA; 
			if( s->name[0] == '#' )
			{
				// search for internal resource
				size_t	bufsize = 0;
				byte	*buffer = FS_LoadInternal( s->name + 1, &bufsize );
				pass->textures[0] = R_FindTexture( s->name, buffer, bufsize, addFlags|TF_NOPICMIP|TF_CLAMP|TF_NOMIPMAP );
			}
			else pass->textures[0] = Shader_FindImage( s, shortname, addFlags|TF_NOPICMIP|TF_CLAMP|TF_NOMIPMAP );
			if( !pass->textures[0] )
			{
				MsgDev( D_WARN, "couldn't find texture for shader '%s', using default...\n", s->name );
				pass->textures[0] = tr.defaultTexture;
			}
			pass->rgbGen.type = RGBGEN_VERTEX;
			pass->alphaGen.type = ALPHAGEN_VERTEX;
			pass->tcgen = TCGEN_BASE;
			break;
		case SHADER_FARBOX:
			s->type = SHADER_FARBOX;
			s->features = MF_STCOORDS;
			s->sort = SORT_SKY;
			s->flags = SHADER_SKYPARMS;
			s->num_stages = 1;
			s->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * s->num_stages );
			strcpy( s->name, shortname );
			s->stages = ( ref_stage_t * )( ( byte * )s->name + length + 1 );
			pass = &s->stages[0];
			pass->flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_MODULATE; 
//			pass->glState = GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ZERO;
			pass->textures[0] = R_FindTexture( shortname, NULL, 0, addFlags|TF_CLAMP|TF_NOMIPMAP );
			pass->rgbGen.type = RGBGEN_IDENTITY_LIGHTING;
			pass->alphaGen.type = ALPHAGEN_IDENTITY;
			pass->tcgen = TCGEN_BASE;
			break;
		case SHADER_NEARBOX:
			s->type = SHADER_NEARBOX;
			s->features = MF_STCOORDS;
			s->sort = SORT_SKY;
			s->num_stages = 1;
			s->flags = SHADER_SKYPARMS;
			s->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * s->num_stages );
			strcpy( s->name, shortname );
			s->stages = ( ref_stage_t * )( ( byte * )s->name + length + 1 );
			pass = &s->stages[0];
			pass->flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_DECAL;
			pass->glState = GLSTATE_ALPHAFUNC|GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			pass->textures[0] = R_FindTexture( shortname, NULL, 0, addFlags|TF_CLAMP|TF_NOMIPMAP );
			pass->rgbGen.type = RGBGEN_IDENTITY_LIGHTING;
			pass->alphaGen.type = ALPHAGEN_IDENTITY;
			pass->tcgen = TCGEN_BASE;
			break;
		case SHADER_PLANAR_SHADOW:
			s->type = SHADER_PLANAR_SHADOW;
			s->features = MF_DEFORMVS;
			s->sort = SORT_DECAL;
			s->flags = 0;
			s->numDeforms = 1;
			s->num_stages = 1;
			s->name = Shader_Malloc( length + 1 + s->numDeforms * sizeof( deform_t ) + sizeof( ref_stage_t ) * s->num_stages );
			strcpy( s->name, shortname );
			s->deforms = ( deform_t * )( ( byte * )s->name + length + 1 );
			s->deforms[0].type = DEFORM_PROJECTION_SHADOW;
			s->stages = ( ref_stage_t * )( ( byte * )s->deforms + s->numDeforms * sizeof( deform_t ) );
			pass = &s->stages[0];
			pass->flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_STENCILSHADOW|SHADERSTAGE_BLEND_DECAL;
			pass->glState = GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			pass->rgbGen.type = RGBGEN_IDENTITY;
			pass->alphaGen.type = ALPHAGEN_IDENTITY;
			pass->tcgen = TCGEN_NONE;
			break;
		case SHADER_OPAQUE_OCCLUDER:
			s->type = SHADER_OPAQUE_OCCLUDER;
			s->sort = SORT_OPAQUE;
			s->flags = SHADER_CULL_FRONT|SHADER_DEPTHWRITE;
			s->num_stages = 1;
			s->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * s->num_stages + 3 * sizeof( float ) );
			strcpy( s->name, shortname );
			s->stages = ( ref_stage_t * )( ( byte * )s->name + length + 1 );
			pass = &s->stages[0];
			pass->textures[0] = tr.whiteTexture;
			pass->flags = SHADERSTAGE_NOCOLORARRAY;
			pass->glState = GLSTATE_DEPTHWRITE;
			pass->rgbGen.type = RGBGEN_ENVIRONMENT;
			pass->rgbGen.args = ( float * )( ( byte * )s->stages + sizeof( ref_stage_t ) * s->num_stages );
			VectorClear( pass->rgbGen.args );
			pass->alphaGen.type = ALPHAGEN_IDENTITY;
			pass->tcgen = TCGEN_NONE;
			break;
		case SHADER_OUTLINE:
			s->type = SHADER_OUTLINE;
			s->features = MF_NORMALS|MF_DEFORMVS;
			s->sort = SORT_OPAQUE;
			s->flags = SHADER_CULL_BACK|SHADER_DEPTHWRITE;
			s->numDeforms = 1;
			s->num_stages = 1;
			s->name = Shader_Malloc( length + 1 + s->numDeforms * sizeof( deform_t ) + sizeof( ref_stage_t ) * s->num_stages );
			strcpy( s->name, shortname );
			s->deforms = ( deform_t * )( ( byte * )s->name + length + 1 );
			s->deforms[0].type = DEFORM_OUTLINE;
			s->stages = ( ref_stage_t * )( ( byte * )s->deforms + s->numDeforms * sizeof( deform_t ) );
			pass = &s->stages[0];
			pass->textures[0] = tr.whiteTexture;
			pass->flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_MODULATE;
			pass->glState = GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ZERO|GLSTATE_DEPTHWRITE;
			pass->rgbGen.type = RGBGEN_OUTLINE;
			pass->alphaGen.type = ALPHAGEN_OUTLINE;
			pass->tcgen = TCGEN_NONE;
			break;
		case SHADER_TEXTURE:
			if( mapConfig.deluxeMappingEnabled && Shaderpass_LoadMaterial( &materialImages[0], &materialImages[1], &materialImages[2], shortname, addFlags, 1 ) )
			{
				s->type = SHADER_TEXTURE;
				s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT|SHADER_NO_MODULATIVE_DLIGHTS|SHADER_HASLIGHTMAP|SHADER_MATERIAL;
				s->features = MF_STCOORDS|MF_LMCOORDS|MF_NORMALS|MF_SVECTORS|MF_ENABLENORMALS;
				s->sort = SORT_OPAQUE;
				s->num_stages = 1;
				s->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * s->num_stages );
				strcpy( s->name, shortname );
				s->stages = (ref_stage_t *)(( byte * )s->name + length + 1 );
				pass = &s->stages[0];
				pass->flags = SHADERSTAGE_LIGHTMAP|SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_REPLACE;
				pass->glState = GLSTATE_DEPTHWRITE;
				pass->tcgen = TCGEN_BASE;
				pass->rgbGen.type = RGBGEN_IDENTITY;
				pass->alphaGen.type = ALPHAGEN_IDENTITY;
				pass->program = DEFAULT_GLSL_PROGRAM;
				pass->program_type = PROGRAM_TYPE_MATERIAL;
				pass->textures[0] = Shader_FindImage( s, shortname, addFlags );
				pass->textures[1] = materialImages[0];	// normalmap
				pass->textures[2] = materialImages[1];	// glossmap
				pass->textures[3] = materialImages[2];	// decalmap
			}
			else
			{
				s->type = SHADER_TEXTURE;
				s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT|SHADER_NO_MODULATIVE_DLIGHTS|SHADER_HASLIGHTMAP;
				s->features = MF_STCOORDS|MF_LMCOORDS;
				s->sort = SORT_OPAQUE;
				s->num_stages = 3;
				s->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * s->num_stages );
				strcpy( s->name, shortname );
				s->stages = ( ref_stage_t * )( ( byte * )s->name + length + 1 );
				pass = &s->stages[0];
				pass->flags = SHADERSTAGE_LIGHTMAP|SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_REPLACE;
				pass->glState = GLSTATE_DEPTHWRITE;
				pass->tcgen = TCGEN_LIGHTMAP;
				pass->rgbGen.type = RGBGEN_IDENTITY;
				pass->alphaGen.type = ALPHAGEN_IDENTITY;
				pass = &s->stages[1];
				pass->flags = SHADERSTAGE_DLIGHT|SHADERSTAGE_BLEND_ADD;
				pass->glState = GLSTATE_DEPTHFUNC_EQ|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
				pass->tcgen = TCGEN_BASE;
				pass = &s->stages[2];
				pass->flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_MODULATE;
				pass->glState = GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_SRC_COLOR;
				pass->tcgen = TCGEN_BASE;
				pass->textures[0] = Shader_FindImage( s, shortname, addFlags );
				pass->rgbGen.type = RGBGEN_IDENTITY;
				pass->alphaGen.type = ALPHAGEN_IDENTITY;
			}
			break;
create_default:
		case SHADER_GENERIC:
		default:
			s->type = SHADER_GENERIC;
			s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT|SHADER_NO_MODULATIVE_DLIGHTS;
			s->features = MF_STCOORDS;
			s->sort = SORT_OPAQUE;
			s->num_stages = 1;
			s->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * s->num_stages );
			strcpy( s->name, shortname );
			s->stages = ( ref_stage_t * )(( byte * )s->name + length + 1 );
			pass = &s->stages[0];
			pass->flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_MODULATE;
			pass->glState = GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_SRC_COLOR;
			pass->tcgen = TCGEN_BASE;
			pass->textures[0] = Shader_FindImage( s, shortname, addFlags );
			pass->rgbGen.type = RGBGEN_IDENTITY;
			pass->alphaGen.type = ALPHAGEN_IDENTITY;
			break;
		}
	}

	// calculate sortkey
	s->sortkey = Shader_Sortkey( s, s->sort );

	// add to hash table
	s->nextHash = shaders_hash[key];
	shaders_hash[key] = s;

	return s;
}

ref_shader_t *R_RegisterPic( const char *name )
{
	return R_LoadShader( name, SHADER_NOMIP, false, 0, SHADER_INVALID );
}

ref_shader_t *R_RegisterShader( const char *name )
{
	return R_LoadShader( name, SHADER_TEXTURE, false, 0, SHADER_INVALID );
}

ref_shader_t *R_RegisterSkin( const char *name )
{
	return R_LoadShader( name, SHADER_MD3, false, 0, SHADER_INVALID );
}
