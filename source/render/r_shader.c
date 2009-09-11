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
	char		*source;
	int		line;
	char		*buffer;
	size_t		size;
	struct ref_script_s	*nextHash;
} ref_script_t;

typedef struct
{
	const char *name;
	bool (*func)( ref_shader_t *shader, ref_stage_t *pass, script_t *script );
} ref_parsekey_t;

byte		*r_shaderpool;
static table_t	*r_tablesHashTable[TABLES_HASH_SIZE];
static ref_script_t	*r_shaderScriptsHash[SHADERS_HASH_SIZE];
static ref_shader_t	*r_shadersHash[SHADERS_HASH_SIZE];
static table_t	*r_tables[MAX_TABLES];
static int	r_numTables;
ref_shader_t	r_shaders[MAX_SHADERS];
static int	r_numShaders = 0;
static deform_t	r_currentDeforms[MAX_SHADER_DEFORMS];
static ref_stage_t	r_currentPasses[MAX_SHADER_STAGES];
static float	r_currentRGBgenArgs[MAX_SHADER_STAGES][3], r_currentAlphagenArgs[MAX_SHADER_STAGES][2];
static waveFunc_t	r_currentRGBgenFuncs[MAX_SHADER_STAGES], r_currentAlphagenFuncs[MAX_SHADER_STAGES];
static tcMod_t	r_currentTcmods[MAX_SHADER_STAGES][MAX_SHADER_TCMODS];
static vec4_t	r_currentTcGen[MAX_SHADER_STAGES][2];
const char	*r_skyBoxSuffix[6] = { "rt", "bk", "lf", "ft", "up", "dn" }; // FIXME: get rid of this
static texture_t	*r_stageTexture[MAX_STAGE_TEXTURES];		// MAX_FRAMES in spritegen.c
static kRenderMode_t	r_shaderRenderMode;			// sprite, alias or studiomodel rendermode
static int		r_numStageTextures;			// num textures in group
static float		r_stageAnimFrequency;		// for auto-animate groups
static bool		r_shaderTwoSided;
static bool		r_shaderNoMipMaps;
static bool		r_shaderNoPicMip;
static bool		r_shaderNoCompress;
static bool		r_shaderHasDlightPass;

#define Shader_FreePassCinematics( s )	if((s)->cinHandle ) { R_FreeCinematics((s)->cinHandle ); (s)->cinHandle = 0; }
#define Shader_CopyString( str )	com.stralloc( r_shaderpool, str, __FILE__, __LINE__ )
#define Shader_Malloc( size )		Mem_Alloc( r_shaderpool, size )
#define Shader_Free( data )		Mem_Free( data )

/*
=======================================================================

 TABLE PARSING

=======================================================================
*/
/*
=================
R_LoadTable
=================
*/
static void R_LoadTable( const char *name, tableFlags_t flags, size_t size, float *values )
{
	table_t	*table;
	uint	hash;

	if( r_numTables == MAX_TABLES )
		Host_Error( "R_LoadTable: MAX_TABLES limit exceeds\n" );

	// fill it in
	table = r_tables[r_numTables++] = Mem_Alloc( r_shaderpool, sizeof( table_t ));
	table->name = Shader_CopyString( name );
	table->index = r_numTables - 1;
	table->flags = flags;
	table->size = size;

	table->values = Mem_Alloc( r_shaderpool, size * sizeof( float ));
	Mem_Copy( table->values, values, size * sizeof( float ));

	// add to hash table
	hash = Com_HashKey( table->name, TABLES_HASH_SIZE );
	table->nextHash = r_tablesHashTable[hash];
	r_tablesHashTable[hash] = table;
}

/*
=================
R_FindTable
=================
*/
static table_t *R_FindTable( const char *name )
{
	table_t	*table;
	uint	hash;

	if( !name || !name[0] ) return NULL;
	if( com.strlen( name ) >= MAX_STRING )
		Host_Error( "R_FindTable: table name exceeds %i symbols\n", MAX_STRING );

	// see if already loaded
	hash = Com_HashKey( name, TABLES_HASH_SIZE );

	for( table = r_tablesHashTable[hash]; table; table = table->nextHash )
	{
		if( !com.stricmp( table->name, name ))
			return table;
	}
	return NULL;
}

/*
=================
R_ParseTable
=================
*/
static bool R_ParseTable( script_t *script, tableFlags_t flags )
{
	token_t	token;
	string	name;
	size_t	size = 0, bufsize = 0;
	bool	variable = false;
	float	*values = NULL;

	if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &token ))
	{
		MsgDev( D_WARN, "missing table name\n" );
		return false;
	}

	com.strncpy( name, token.string, sizeof( name ));

	Com_ReadToken( script, false, &token );
	if( com.stricmp( token.string, "[" ))
	{
		MsgDev( D_WARN, "expected '[', found '%s' instead in table '%s'\n", token.string, name );
		return false;
	}

	Com_ReadToken( script, false, &token );
	if( com.stricmp( token.string, "]" ))
	{
		bufsize = com.atoi( token.string );
		if( bufsize <= 0 )
		{
			MsgDev( D_WARN, "'%s' have invalid size\n", name );
			return false;
		}

		// reserve one slot to avoid corrupt memory
		values = Mem_Alloc( r_shaderpool, sizeof( float ) * (bufsize + 1)); 
		Com_ReadToken( script, false, &token );
		if( com.stricmp( token.string, "]" ))
		{
			MsgDev( D_WARN, "expected ']', found '%s' instead in table '%s'\n", token.string, name );
			return false;
		}
	}
	else variable = true; // variable sized

	Com_ReadToken( script, false, &token );
	if( com.stricmp( token.string, "=" ))
	{
		MsgDev( D_WARN, "expected '=', found '%s' instead in table '%s'\n", token.string, name );
		return false;
	}

	// parse values now
	while( 1 )
	{
		if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &token ))
		{
			MsgDev( D_WARN, "missing parameters for table '%s'\n", name );
			return false;
		}

		if( com.stricmp( token.string, "{" ))
		{
			MsgDev( D_WARN, "expected '{', found '%s' instead in table '%s'\n", token.string, name );
			return false;
		}

		while( 1 )
		{
			if( size >= bufsize )
			{
				if( variable )
				{
					bufsize = size + 8;
					values = Mem_Realloc( r_shaderpool, values, sizeof(float) * bufsize );
				}
				else if( size > bufsize )
				{
					MsgDev( D_WARN, "'%s' too many initializers\n", name );
					if( values ) Mem_Free( values );
					return false;
				}
			}

			if( size != 0 )
			{
				Com_ReadToken( script, SC_ALLOW_NEWLINES, &token );
				if( !com.stricmp( token.string, "}" )) break; // end
				else if( !com.stricmp( token.string, ";" ))
				{
					// save token, to let grab semicolon properly
					Com_SaveToken( script, &token );
					break;
				}
				else if( com.stricmp( token.string, "," ))
				{
					MsgDev( D_WARN, "expected ',', found '%s' instead in table '%s'\n", token.string, name );
					if( values ) Mem_Free( values );
					return false;
				}
			}

			if( !Com_ReadFloat( script, SC_ALLOW_NEWLINES, &values[size] ))
			{
				if( size != 0 ) continue; // probably end of the table
				else
				{
					MsgDev( D_WARN, "'%s' is empty table\n", name );
					if( values ) Mem_Free( values );
					return false; // empty table ?
				}
			}
			size++;
		}
		break;
	}

	// check sizes
	if( !variable && size < bufsize )
		MsgDev( D_WARN, "'%s' have explicit size %i, but real size is %i\n", name, bufsize, size );

	Com_ReadToken( script, SC_ALLOW_NEWLINES, &token );
	if( com.stricmp( token.string, ";" ))
	{
		MsgDev( D_WARN, "'%s' missing seimcolon at end of table definition\n", name );
		Com_SaveToken( script, &token );
	}

	// register new table
	R_LoadTable( name, flags, size, values );
	return true;
}

/*
=================
R_LookupTable
=================
*/
float R_LookupTable( int tableIndex, float index )
{
	table_t	*table;
	float	frac, value;
	uint	curIndex, oldIndex;

	if( tableIndex < 0 || tableIndex >= r_numTables )
		Host_Error( "R_LookupTable: out of range\n" );

	table = r_tables[tableIndex];

	index *= table->size;
	frac = index - floor(index);

	curIndex = (uint)index + 1;
	oldIndex = (uint)index;

	if( table->flags & TABLE_CLAMP )
	{
		curIndex = bound( 0, curIndex, table->size - 1 );
		oldIndex = bound( 0, oldIndex, table->size - 1 );
	}
	else
	{
		curIndex %= table->size;
		oldIndex %= table->size;
	}

	if( table->flags & TABLE_SNAP ) value = table->values[oldIndex];
	else value = table->values[oldIndex] + (table->values[curIndex] - table->values[oldIndex]) * frac;

	return value;
}

/*
=================
R_GetTableByHandle
=================
*/
float *R_GetTableByHandle( int tableIndex )
{
	table_t	*table;

	if( tableIndex < 0 || tableIndex >= r_numTables )
	{
		MsgDev( D_ERROR, "R_GetTableByHandle: out of range\n" );
		return NULL;
	}
	table = r_tables[tableIndex];

	if( !table ) return NULL;
	return table->values;
}


/*
=======================================================================

 SHADER PARSING

=======================================================================
*/
static bool Shader_ParseVector( script_t *script, float *v, size_t size )
{
	uint	i;
	token_t	token;
	bool	bracket = false;

	if( v == NULL || size == 0 )
		return false;

	Mem_Set( v, 0, sizeof( *v ) * size );

	if( size == 1 )
		return Com_ReadFloat( script, 0, v );

	if( !Com_ReadToken( script, false, &token ))
		return false;
	if( token.type == TT_PUNCTUATION && !com.stricmp( token.string, "(" ))
		bracket = true;
	else Com_SaveToken( script, &token ); // save token to right get it again

	for( i = 0; i < size; i++ )
	{
		if( !Com_ReadFloat( script, false, &v[i] ))
			v[i] = 0;	// because Com_ReadFloat may return 0 if parsing expression it's not a number
	}

	if( !bracket ) return true;	// done

	if( !Com_ReadToken( script, false, &token ))
		return false;

	if( token.type == TT_PUNCTUATION && !com.stricmp( token.string, ")" ))
		return true;
	return false;
}

static void Shader_SkipLine( script_t *script )
{
	Com_SkipRestOfLine( script );
}

static void Shader_SkipBlock( script_t *script )
{
	Com_SkipBracedSection( script, 1 );
}

#define MAX_CONDITIONS	8
typedef enum
{
	COP_LS,
	COP_LE,
	COP_EQ,
	COP_GR,
	COP_GE,
	COP_NE
} conOp_t;

typedef enum
{
	COP2_AND,
	COP2_OR
} conOp2_t;

typedef struct
{
	int	operand;
	conOp_t	op;
	bool	negative;
	int	val;
	conOp2_t	logic;
} shaderCon_t;

char *conOpStrings[] = { "<", "<=", "==", ">", ">=", "!=", NULL };
char *conOpStrings2[] = { "&&", "||", NULL };

static bool Shader_ParseConditions( script_t *script, ref_shader_t *shader )
{
	int		i;
	token_t		tok;
	int		numConditions;
	shaderCon_t	conditions[MAX_CONDITIONS];
	bool		result = false, val = false, skip, expectingOperator;
	static const int	falseCondition = 0;

	numConditions = 0;
	Mem_Set( conditions, 0, sizeof( conditions ));

	skip = false;
	expectingOperator = false;
	while( 1 )
	{
		if( !Com_ReadToken( script, false, &tok ))
		{
			if( expectingOperator )
				numConditions++;
			break;
		}
		if( skip ) continue;

		for( i = 0; conOpStrings[i]; i++ )
		{
			if( !com.stricmp( tok.string, conOpStrings[i] ))
				break;
		}

		if( conOpStrings[i] )
		{
			if( !expectingOperator )
			{
				MsgDev( D_ERROR, "bad syntax condition in shader '%s'\n", shader->name );
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
			if( !com.stricmp( tok.string, conOpStrings2[i] ))
				break;
		}

		if( conOpStrings2[i] )
		{
			if( !expectingOperator )
			{
				MsgDev( D_ERROR, "bad syntax condition in shader '%s'\n", shader->name );
				skip = true;
			}
			else
			{
				conditions[numConditions++].logic = i;
				if( numConditions == MAX_CONDITIONS )
					skip = true;
				else expectingOperator = false;
			}
			continue;
		}

		if( expectingOperator )
		{
			MsgDev( D_ERROR, "bad syntax condition in shader '%s'\n", shader->name );
			skip = true;
			continue;
		}

		if( !com.stricmp( tok.string, "!" ))
		{
			conditions[numConditions].negative = !conditions[numConditions].negative;
			continue;
		}

		if( !conditions[numConditions].operand )
		{
			if( !com.stricmp( tok.string, "maxTextureSize" ))
				conditions[numConditions].operand = glConfig.max_2d_texture_size;
			else if( !com.stricmp( tok.string, "maxTextureCubemapSize" ))
				conditions[numConditions].operand = glConfig.max_cubemap_texture_size;
			else if( !com.stricmp( tok.string, "maxTextureUnits" ))
				conditions[numConditions].operand = glConfig.max_texture_units;
			else if( !com.stricmp( tok.string, "textureCubeMap" ))
				conditions[numConditions].operand = GL_Support( R_TEXTURECUBEMAP_EXT );
			else if( !com.stricmp( tok.string, "textureEnvCombine" ))
				conditions[numConditions].operand = GL_Support( R_ENV_COMBINE_EXT );
			else if( !com.stricmp( tok.string, "textureEnvDot3" ))
				conditions[numConditions].operand = GL_Support( R_SHADER_GLSL100_EXT );
			else if( !com.stricmp( tok.string, "GLSL" ))
				conditions[numConditions].operand = GL_Support( R_SHADER_GLSL100_EXT );
			else if( !com.stricmp( tok.string, "deluxeMaps" ) || !com.stricmp( tok.string, "deluxe" ))
				conditions[numConditions].operand = mapConfig.deluxeMappingEnabled;
			else if( !com.stricmp( tok.string, "portalMaps" ))
				conditions[numConditions].operand = r_portalmaps->integer;
			else
			{
				MsgDev( D_WARN, "unknown expression '%s' in shader '%s'\n", tok, shader->name );
				conditions[numConditions].operand = falseCondition;
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

		if( !com.stricmp( tok.string, "false" ))
			conditions[numConditions].val = 0;
		else if( !com.stricmp( tok.string, "true" ))
			conditions[numConditions].val = 1;
		else conditions[numConditions].val = com.atoi( tok.string );
		expectingOperator = true;
	}

	if( skip ) return false;

	if( !conditions[0].operand )
	{
		MsgDev( D_WARN, "empty 'if' statement in shader '%s'\n", shader->name );
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
		else result = val;
	}
	return result;
}

static bool Shader_SkipConditionBlock( script_t *script )
{
	token_t	tok;
	int	condition_count = 1;

	while( condition_count > 0 )
	{
		if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &tok ))
			return false;
		if( !com.stricmp( tok.string, "if" ))
			condition_count++;
		else if( !com.stricmp( tok.string, "endif" ))
			condition_count--;
	}
	return true;
}

//===========================================================================
static bool Shader_CheckSkybox( const char *name )
{
	const char	*skybox_ext[4] = { "tga", "jpg", "png", "dds" };
	int		i, j, num_checked_sides;
	const char	*sidename;
	string		loadname;

	com.strncpy( loadname, name, sizeof( loadname ));
	FS_StripExtension( loadname );                                     
	if( loadname[com.strlen( loadname ) - 1] == '_' )
		loadname[com.strlen( loadname ) - 1] = '\0';

	if( FS_FileExists( va( "%s.dds", loadname )))
		return true;

	if( FS_FileExists( va( "%s_.dds", loadname )))
		return true;

	// complex cubemap pack not found, search for skybox images				
	for( i = 0; i < 4; i++ )
	{	
		num_checked_sides = 0;
		for( j = 0; j < 6; j++ )
		{         
			// build side name
			sidename = va( "%s_%s.%s", loadname, r_skyBoxSuffix[j], skybox_ext[i] );
			if( FS_FileExists( sidename )) num_checked_sides++;

		}
		if( num_checked_sides == 6 )
			return true; // image exists
		for( j = 0; j < 6; j++ )
		{         
			// build side name
			sidename = va( "%s%s.%s", loadname, r_skyBoxSuffix[j], skybox_ext[i] );
			if( FS_FileExists( sidename )) num_checked_sides++;
		}
		if( num_checked_sides == 6 )
			return true; // images exists
	}
	return false;
}

static bool Shader_ParseSkySides( script_t *script, ref_shader_t *shader, ref_shader_t **shaders, bool farbox )
{
	int		i, shaderType;
	texture_t		*image;
	string		name;
	token_t		tok;

	Mem_Set( shaders, 0, sizeof( ref_shader_t* ) * 6 );

	switch( shader->type )
	{
	case SHADER_VERTEX:
	case SHADER_TEXTURE:
	case SHADER_SKY: break;
	default:
		MsgDev( D_ERROR, "'skyParms' not allowed in shader '%s'[%i]\n", shader->name, shader->type );
		return false;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'skyParms' in shader '%s'\n", shader->name );
		return false;
	}

	if( com.stricmp( tok.string, "-" ) && com.stricmp( tok.string, "full" ))
	{
		shaderType = ( farbox ? SHADER_FARBOX : SHADER_NEARBOX );
		if( tok.string[com.strlen( tok.string ) - 1] == '_' )
			tok.string[com.strlen( tok.string ) - 1] = '\0';

		for( i = 0; i < 6; i++ )
		{
			com.snprintf( name, sizeof( name ), "%s_%s", tok.string, r_skyBoxSuffix[i] );
			image = R_FindTexture( name, NULL, 0, TF_CLAMP|TF_NOMIPMAP|TF_SKYSIDE );
			if( !image ) break;
			shaders[i] = R_LoadShader( image->name, shaderType, true, image->flags, SHADER_INVALID );
		}
		if( i == 6 ) return true;

		for( i = 0; i < 6; i++ )
		{
			com.snprintf( name, sizeof( name ), "%s%s", tok.string, r_skyBoxSuffix[i] );
			image = R_FindTexture( name, NULL, 0, TF_CLAMP|TF_NOMIPMAP|TF_SKYSIDE );
			if( !image ) break;
			shaders[i] = R_LoadShader( image->name, shaderType, true, image->flags, SHADER_INVALID );
		}
		if( i == 6 ) return true;

		// create default skybox
		for( i = 0; i < 6; i++ )
		{
			image = tr.skyTexture;
			shaders[i] = R_LoadShader( image->name, shaderType, true, image->flags, SHADER_INVALID );
		}
		return true;
	}
	return true;
}

static bool Shader_ParseFunc( script_t *script, waveFunc_t *func, ref_shader_t *shader )
{
	token_t	tok;
	table_t	*tb;

	if( !Com_ReadToken( script, false, &tok ))
		return false;

	func->tableIndex = -1;

	if( !com.stricmp( tok.string, "0" )) func->type = WAVEFORM_SIN;
	else if( !com.stricmp( tok.string, "sin" )) func->type = WAVEFORM_SIN;
	else if( !com.stricmp( tok.string, "triangle" )) func->type = WAVEFORM_TRIANGLE;
	else if( !com.stricmp( tok.string, "square" )) func->type = WAVEFORM_SQUARE;
	else if( !com.stricmp( tok.string, "sawtooth" )) func->type = WAVEFORM_SAWTOOTH;
	else if( !com.stricmp( tok.string, "inverseSawtooth" )) func->type = WAVEFORM_INVERSESAWTOOTH;
	else if( !com.stricmp( tok.string, "noise" )) func->type = WAVEFORM_NOISE;
	else
	{	// check for custom table
		tb = R_FindTable( tok.string );
		if( tb )
		{
			func->type = WAVEFORM_TABLE;
			func->tableIndex = tb->index;
		}
		else
		{
			MsgDev( D_WARN, "unknown waveform '%s' in shader '%s', defaulting to sin\n", tok.string, shader->name );
			func->type = WAVEFORM_SIN;
		}
	}

	if( !Shader_ParseVector( script, func->args, 4 ))
	{
		MsgDev( D_ERROR, "misson waveform parms in shader '%s'\n", shader->name );
		return false;
	}
	return true;
}

//===========================================================================

static int Shader_SetImageFlags( ref_shader_t *shader )
{
	int	flags = 0;

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

static texture_t *Shader_FindImage( ref_shader_t *shader, const char *name, int flags )
{
	texture_t		*image;
	string		srcpath;

	if( !com.stricmp( name, "$whiteimage" ) || !com.stricmp( name, "*white" ))
		return tr.whiteTexture;
	if( !com.stricmp( name, "$blackimage" ) || !com.stricmp( name, "*black" ))
		return tr.blackTexture;
	if( !com.stricmp( name, "$blankbumpimage" ) || !com.stricmp( name, "*blankbump" ))
		return tr.blankbumpTexture;
	if( !com.stricmp( name, "$particle" ) || !com.stricmp( name, "*particle" ))
		return tr.particleTexture;
	if( !com.stricmp( name, "$corona" ) || !com.stricmp( name, "*corona" ))
		return tr.coronaTexture;
	if( !com.strnicmp( name, "*lm", 3 ))
	{
		MsgDev( D_WARN, "shader %s has a stage with explicit lightmap image.\n", shader->name );
		return tr.whiteTexture;
	}

	com.strncpy( srcpath, name, sizeof( srcpath ));
	if( shader->type != SHADER_STUDIO ) FS_StripExtension( srcpath );

	image = R_FindTexture( srcpath, NULL, 0, flags );
	if( !image )
	{
		MsgDev( D_WARN, "couldn't find texture '%s' in shader '%s'\n", srcpath, shader->name );
		return tr.defaultTexture;
	}
	return image;
}

/****************** shader keyword functions ************************/

static bool Shader_Cull( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	token_t	tok;

	shader->flags &= ~(SHADER_CULL_FRONT|SHADER_CULL_BACK);

	if( !Com_ReadToken( script, false, &tok ))
	{
		shader->flags |= SHADER_CULL_FRONT;
	}
	else
	{
		if( !com.stricmp( tok.string, "front" )) 
			shader->flags |= SHADER_CULL_FRONT;
		else if( !com.stricmp( tok.string, "back" ) || !com.stricmp( tok.string, "backSide" ) || !com.stricmp( tok.string, "backSided" ))
			shader->flags |= SHADER_CULL_BACK;
		else if( !com.stricmp( tok.string, "disable" ) || !com.stricmp( tok.string, "none" ) || !com.stricmp( tok.string, "twoSided" ))
			shader->flags &= ~(SHADER_CULL_FRONT|SHADER_CULL_BACK);
		else
		{
			MsgDev( D_ERROR, "unknown 'cull' parameter '%s' in shader '%s'\n", tok.string, shader->name );
			return false;
		}
	}
	return true;
}

static bool Shader_shaderNoMipMaps( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	r_shaderNoMipMaps = r_shaderNoPicMip = true;
	return true;
}

static bool Shader_shaderNoPicMip( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	r_shaderNoPicMip = true;
	return true;
}

static bool Shader_shaderNoCompress( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	r_shaderNoCompress = true;
	return true;
}

static bool Shader_DeformVertexes( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	token_t	tok;
	deform_t	*deformv;

	if( shader->numDeforms == MAX_SHADER_DEFORMS )
	{
		MsgDev( D_ERROR, "MAX_SHADER_DEFORMS hit in shader '%s'\n", shader->name );
		return false;
	}

	deformv = &r_currentDeforms[shader->numDeforms];

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'deformVertexes' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "wave" ))
	{
		if( !Com_ReadToken( script, false, &tok ))
		{
			MsgDev( D_ERROR, "missing parameters for 'deformVertexes wave' in shader '%s'\n", shader->name );
			return false;
		}

		deformv->args[0] = com.atof( tok.string );
		if( deformv->args[0] == 0.0f )
		{
			MsgDev( D_ERROR, "illegal div value of 0 for 'deformVertexes wave' in shader '%s', defaulting to 100\n", shader->name );
			deformv->args[0] = 100.0f;
		}
		deformv->args[0] = 1.0 / deformv->args[0];

		if( !Shader_ParseFunc( script, &deformv->func, shader ))
		{
			MsgDev( D_ERROR, "missing waveform parameters for 'deformVertexes wave' in shader '%s'\n", shader->name );
			return false;
		}
		deformv->type = DEFORM_WAVE;
	}
	else if( !com.stricmp( tok.string, "normal" ) )
	{
		if( !Shader_ParseVector( script, deformv->args, 2 ))
		{
			MsgDev( D_ERROR, "missing parameters for 'deformVertexes move' in shader '%s'\n", shader->name );
			return false;
		}
		shader->flags |= SHADER_DEFORM_NORMAL;
		deformv->type = DEFORM_NORMAL;
	}
	else if( !com.stricmp( tok.string, "bulge" ))
	{
		if( !Shader_ParseVector( script, deformv->args, 3 ))
		{
			MsgDev( D_ERROR, "missing parameters for 'deformVertexes bulge' in shader '%s'\n", shader->name );
			return false;
		}
		deformv->type = DEFORM_BULGE;
	}
	else if( !com.stricmp( tok.string, "move" ))
	{
		if( !Shader_ParseVector( script, deformv->args, 3 ))
		{
			MsgDev( D_ERROR, "missing parameters for 'deformVertexes move' in shader '%s'\n", shader->name );
			return false;
		}

		if(!Shader_ParseFunc( script, &deformv->func, shader ))
		{
			MsgDev( D_ERROR, "missing waveform parameters for 'deformVertexes move' in shader '%s'\n", shader->name );
			return false;
		}
		deformv->type = DEFORM_MOVE;
	}
	else if( !com.stricmp( tok.string, "autosprite" ))
	{
		deformv->type = DEFORM_AUTOSPRITE;
		shader->flags |= SHADER_AUTOSPRITE;
	}
	else if( !com.stricmp( tok.string, "autosprite2" ))
	{
		deformv->type = DEFORM_AUTOSPRITE2;
		shader->flags |= SHADER_AUTOSPRITE;
	}
	else if( !com.stricmp( tok.string, "projectionShadow" ))
	{
		deformv->type = DEFORM_PROJECTION_SHADOW;
	}
	else if( !com.stricmp( tok.string, "autoparticle" ))
	{
		deformv->type = DEFORM_AUTOPARTICLE;
          }
	else if( !com.stricmp( tok.string, "outline" ))
	{
		deformv->type = DEFORM_OUTLINE;
	}
	else
	{
		MsgDev( D_WARN, "unknown 'deformVertexes' parameter '%s' in shader '%s'\n", tok.string, shader->name );
		Shader_SkipLine( script );
		return true;
	}

	shader->numDeforms++;
	return true;
}

static bool Shader_SkyParms( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	float		cloudHeight;
	ref_shader_t	*farboxShaders[6];
	ref_shader_t	*nearboxShaders[6];
	token_t		tok;

	if( shader->skyParms )
	{
		R_FreeSkydome( shader->skyParms );
		shader->skyParms = NULL;
	}
	if( !Shader_ParseSkySides( script, shader, farboxShaders, true ))
	{
		MsgDev( D_ERROR, "missing far skybox parameters for 'skyParms' in shader '%s'\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'skyParms' in shader '%s'\n", shader->name );
		return false;
	}

	if( com.stricmp( tok.string, "-" ))
	{
		cloudHeight = com.atof( tok.string );
		if( cloudHeight < 8.0f || cloudHeight > 1024.0f )
		{
			MsgDev( D_WARN, "out of range cloudHeight value of %f for 'skyParms' in shader '%s', defaulting to 512\n", cloudHeight, shader->name );
			cloudHeight = 512.0f;
		}
	}
	else cloudHeight = 512.0f;

	// merge farclip
	if( cloudHeight * 2 > r_farclip_min ) r_farclip_min = cloudHeight * 2;

	if( !Shader_ParseSkySides( script, shader, nearboxShaders, false ))
	{
		MsgDev( D_ERROR, "missing near skybox parameters for 'skyParms' in shader '%s'\n", shader->name );
		return false;
	}

	shader->skyParms = R_CreateSkydome( r_shaderpool, cloudHeight, farboxShaders, nearboxShaders );
	shader->flags |= SHADER_SKYPARMS;
	shader->sort = SORT_SKY;

	return true;
}

static bool Shader_FogParms( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	float	div;
	vec3_t	color, fcolor;

	if( !r_ignorehwgamma->integer )
		div = 1.0f / pow( 2, max( 0, floor( r_overbrightbits->value )));
	else div = 1.0f;
	if( IS_NAN( div )) div = 1.0f; // FIXME: strange bug

	Vector4Set( shader->fog_color, 0, 0, 0, 0 );
	shader->fog_dist = shader->fog_clearDist = 0;

	if( !Shader_ParseVector( script, color, 3 ))
	{
		MsgDev( D_ERROR, "missing fog color for 'fogParms' in shader '%s'\n", shader->name );
		return false;
	}

	ColorNormalize( color, fcolor );
	VectorScale( fcolor, div, fcolor );

	shader->fog_color[0] = R_FloatToByte( fcolor[0] );
	shader->fog_color[1] = R_FloatToByte( fcolor[1] );
	shader->fog_color[2] = R_FloatToByte( fcolor[2] );
	shader->fog_color[3] = 255;

	if( !Com_ReadFloat( script, false, &shader->fog_dist ))
	{
		MsgDev( D_ERROR, "missing fog distance for 'fogParms' in shader '%s'\n", shader->name );
		return false;
	}

	if( shader->fog_dist <= 0.1f ) shader->fog_dist = 128.0f;

	// clear dist is optionally parm
	Com_ReadFloat( script, false, &shader->fog_clearDist );

	if( shader->fog_clearDist > shader->fog_dist - 128.0f )
		shader->fog_clearDist = shader->fog_dist - 128.0f;
	if( shader->fog_clearDist <= 0.0f ) shader->fog_clearDist = 0.0f;

	return true;
}

static bool Shader_SkyRotate( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	VectorSet( shader->skyAxis, 0.0f, 0.0f, 1.0f );
	shader->skySpeed = 0.0f;

	// clear dist is optionally parm
	if( !Com_ReadFloat( script, false, &shader->skySpeed ))
	{
		MsgDev( D_ERROR, "missing sky speed for 'skyRotate' in shader '%s'\n", shader->name );
		return false;
	}

	if( !Shader_ParseVector( script, shader->skyAxis, 3 ))	// skyAxis is optionally
	{
		VectorSet( shader->skyAxis, 0.0f, 0.0f, 1.0f );
		return true;
	}

	if( VectorIsNull( shader->skyAxis ))
		VectorSet( shader->skyAxis, 0.0f, 0.0f, 1.0f );	
	VectorNormalize( shader->skyAxis );

	return true;
}

static bool Shader_Sort( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	token_t	tok;

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'sort' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "portal" )) shader->sort = SORT_PORTAL;
	else if( !com.stricmp( tok.string, "sky" )) shader->sort = SORT_SKY;
	else if( !com.stricmp( tok.string, "decal" )) shader->sort = SORT_DECAL;
	else if( !com.stricmp( tok.string, "opaque" )) shader->sort = SORT_OPAQUE;
	else if( !com.stricmp( tok.string, "banner" )) shader->sort = SORT_BANNER;
	else if( !com.stricmp( tok.string, "alphaTest" )) shader->sort = SORT_ALPHATEST;
	else if( !com.stricmp( tok.string, "seeThrough" )) shader->sort = SORT_ALPHATEST;
	else if( !com.stricmp( tok.string, "underWater" )) shader->sort = SORT_UNDERWATER;
	else if( !com.stricmp( tok.string, "additive" )) shader->sort = SORT_ADDITIVE;
	else if( !com.stricmp( tok.string, "nearest" )) shader->sort = SORT_NEAREST;
	else if( !com.stricmp( tok.string, "water" )) shader->sort = SORT_WATER;
	else
	{
		shader->sort = com.atoi( tok.string );
		if( shader->sort < 1 || shader->sort > 16 )
		{
			MsgDev( D_WARN, "unknown 'sort' parameter '%s' in shader '%s', defaulting to 'opaque'\n", tok.string, shader->name );
			shader->sort = SORT_OPAQUE;
		}
	}
	return true;
}

static bool Shader_Portal( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	shader->flags |= SHADER_PORTAL;
	shader->sort = SORT_PORTAL;

	return true;
}

static bool Shader_PolygonOffset( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	shader->flags |= SHADER_POLYGONOFFSET;
	return true;
}

static bool Shader_EntityMergable( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	shader->flags |= SHADER_ENTITY_MERGABLE;
	return true;
}

static bool Shader_If( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	if( !Shader_ParseConditions( script, shader ))
	{
		if( !Shader_SkipConditionBlock( script ))
		{
			MsgDev( D_ERROR, "mismatched if/endif pair in shader '%s'\n", shader->name );
			return false;
		}
	}
	return true;
}

static bool Shader_Endif( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	return true;
}

static bool Shader_SurfaceParm( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	Com_SkipRestOfLine( script );
	return true;
}

static bool Shader_TessSize( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	Com_ReadFloat( script, false, &shader->tessSize );
	return true;
}

static bool Shader_Light( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	Com_SkipRestOfLine( script );
	return true;
}

static bool Shader_NoModulativeDlights( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	shader->flags |= SHADER_NO_MODULATIVE_DLIGHTS;
	return true;
}

static bool Shader_OffsetMappingScale( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	if( !Com_ReadFloat( script, false, &shader->offsetmapping_scale ))
	{
		MsgDev( D_ERROR, "missing value 'offsetMappingScale' in shader '%s'\n", shader->name );
		return false;
	}

	if( shader->offsetmapping_scale < 0.0f ) shader->offsetmapping_scale = 0.0f;
	return true;
}

static const ref_parsekey_t shaderkeys[] =
{
{ "if",			Shader_If			},
{ "sort",			Shader_Sort		},
{ "cull",			Shader_Cull		},
{ "light",		Shader_Light		},
{ "endif",		Shader_Endif		},
{ "portal",		Shader_Portal		},
{ "fogvars",		Shader_FogParms		}, // RTCW fog params
{ "skyParms",		Shader_SkyParms		},
{ "skyRotate",		Shader_SkyRotate		},
{ "fogparms",		Shader_FogParms		},
{ "tessSize",		Shader_TessSize		},
{ "nopicmip",		Shader_shaderNoPicMip	},
{ "nomipmap",		Shader_shaderNoMipMaps	},
{ "nomipmaps",		Shader_shaderNoMipMaps	},
{ "nocompress",		Shader_shaderNoCompress	},
{ "surfaceParm",		Shader_SurfaceParm,		},
{ "polygonoffset",		Shader_PolygonOffset	},
{ "deformvertexes",		Shader_DeformVertexes	},
{ "entitymergable",		Shader_EntityMergable	},
{ "offsetmappingscale",	Shader_OffsetMappingScale	},
{ "nomodulativedlights",	Shader_NoModulativeDlights	},
{ NULL,			NULL			}
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

static bool Shaderpass_AnimFrequency( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	float	anim_fps;

	if( !Com_ReadFloat( script, false, &anim_fps ))
	{
		MsgDev( D_ERROR, "missing parameters for 'animFrequency' in shader '%s\n", shader->name );
		return false;
	}

	pass->flags |= SHADERSTAGE_ANIMFREQUENCY;
	if( pass->num_textures )
	{
		if( pass->anim_offset )
		{
			// someone tired specify third anim sequence
			MsgDev( D_ERROR, "too many 'animFrequency' declared in shader '%s\n", shader->name );
			return false;
		}
		pass->animFrequency[1] = anim_fps;
		pass->anim_offset = pass->num_textures;
	}
	else
	{
		pass->animFrequency[0] = anim_fps;
		pass->anim_offset = 0;
	}
	return true;
}

static bool Shaderpass_MapExt( ref_shader_t *shader, ref_stage_t *pass, int addFlags, script_t *script )
{
	int	flags;
	string	name;
	token_t	tok;

	Shader_FreePassCinematics( pass );

	if( pass->num_textures )
	{
		if( pass->num_textures == MAX_STAGE_TEXTURES )
		{
			MsgDev( D_ERROR, "MAX_STAGE_TEXTURES hit in shader '%s'\n", shader->name );
			return false;
		}
		if(!( pass->flags & SHADERSTAGE_ANIMFREQUENCY ))
			pass->flags |= SHADERSTAGE_FRAMES;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'map' in shader '%s'\n", shader->name );
		return false;
	}

	com.strncpy( name, tok.string, sizeof( name ));

	if( !com.stricmp( tok.string, "$lightmap" ))
	{
		pass->tcgen = TCGEN_LIGHTMAP;
		pass->flags = ( pass->flags & ~(SHADERSTAGE_PORTALMAP|SHADERSTAGE_DLIGHT)) | SHADERSTAGE_LIGHTMAP;
		pass->animFrequency[0] = pass->animFrequency[1] = 0.0f;
		pass->anim_offset = 0;
		pass->textures[0] = NULL;
		return true;
	}
	else if( !com.stricmp( tok.string, "$dlight" ))
	{
		pass->tcgen = TCGEN_BASE;
		pass->flags = ( pass->flags & ~(SHADERSTAGE_LIGHTMAP|SHADERSTAGE_PORTALMAP)) | SHADERSTAGE_DLIGHT;
		pass->animFrequency[0] = pass->animFrequency[1] = 0.0f;
		pass->anim_offset = 0;
		pass->textures[0] = NULL;
		r_shaderHasDlightPass = true;
		return true;
	}
	else if( !com.stricmp( tok.string, "$portalmap" ) || !com.stricmp( tok.string, "$mirrormap" ))
	{
		pass->tcgen = TCGEN_PROJECTION;
		pass->flags = ( pass->flags & ~(SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT)) | SHADERSTAGE_PORTALMAP;
		pass->animFrequency[0] = pass->animFrequency[1] = 0.0f;
		pass->anim_offset = 0;
		pass->textures[0] = NULL;
		if(( shader->flags & SHADER_PORTAL ) && ( shader->sort == SORT_PORTAL ))
			shader->sort = 0; // reset sorting so we can figure it out later. FIXME?
		shader->flags |= SHADER_PORTAL|( r_portalmaps->integer ? SHADER_PORTAL_CAPTURE1 : 0 );
		return true;
	}
	else if( !com.stricmp( tok.string, "$whiteImage" ))
	{
		pass->textures[0] = tr.whiteTexture;
	}
	else if( !com.stricmp( tok.string, "$blackImage" ))
	{
		pass->textures[0] = tr.blackTexture;
	}
	else if( !com.stricmp( tok.string, "$particle" ))
	{
		pass->textures[0] = tr.particleTexture;
	}
	else
	{
		flags = Shader_SetImageFlags( shader ) | addFlags;

		while( 1 )
		{
			if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
				break;

			com.strncat( name, " ", sizeof( name ));
			com.strncat( name, tok.string, sizeof( name ));
		}

		pass->textures[pass->num_textures] = Shader_FindImage( shader, name, flags );
		pass->tcgen = TCGEN_BASE;
		pass->flags &= ~(SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT|SHADERSTAGE_PORTALMAP);
	}
	pass->num_textures++;

	return true;
}

static bool Shaderpass_AnimMapExt( ref_shader_t *shader, ref_stage_t *pass, int addFlags, script_t *script )
{
	int	flags;
	float	anim_fps;
	token_t	tok;

	Shader_FreePassCinematics( pass );
	flags = Shader_SetImageFlags( shader ) | addFlags;

	pass->tcgen = TCGEN_BASE;
	pass->flags &= ~( SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT|SHADERSTAGE_PORTALMAP );

	if( !Com_ReadFloat( script, false, &anim_fps ))
	{
		MsgDev( D_ERROR, "missing 'AnimFrequency' parameter for 'animMap' in shader '%s\n", shader->name );
		return false;
	}

	if( pass->num_textures )
	{
		if( pass->anim_offset )
		{
			// someone tired specify third anim sequence
			MsgDev( D_ERROR, "too many 'animFrequency' declared in shader '%s\n", shader->name );
			return false;
		}
		pass->animFrequency[1] = anim_fps;
		pass->anim_offset = pass->num_textures;
	}
	else
	{
		pass->animFrequency[0] = anim_fps;
		pass->anim_offset = 0;
	}
	pass->flags |= (SHADERSTAGE_ANIMFREQUENCY|SHADERSTAGE_FRAMES);

	while( 1 )
	{
		if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
			break;
		if( pass->num_textures < MAX_STAGE_TEXTURES )
			pass->textures[pass->num_textures++] = Shader_FindImage( shader, tok.string, flags );
	}

	if( pass->num_textures == 0 )
	{
		MsgDev( D_WARN, "missing animation frames for 'animMap' in shader '%s'\n", shader->name );
		pass->flags &= ~(SHADERSTAGE_ANIMFREQUENCY|SHADERSTAGE_FRAMES);
		pass->animFrequency[0] = pass->animFrequency[1] = 0.0f;
		pass->anim_offset = 0;
	}
	return true;
}

static bool Shaderpass_CubeMapExt( ref_shader_t *shader, ref_stage_t *pass, int addFlags, int tcgen, script_t *script )
{
	int	flags;
	string	name;
	token_t	tok;
	
	Shader_FreePassCinematics( pass );

	if( pass->num_textures )
	{
		if( pass->num_textures == MAX_STAGE_TEXTURES )
		{
			MsgDev( D_ERROR, "MAX_STAGE_TEXTURES hit in shader '%s'\n", shader->name );
			return false;
		}
		if(!( pass->flags & SHADERSTAGE_ANIMFREQUENCY ))
			pass->flags |= SHADERSTAGE_FRAMES;
	}

	pass->flags &= ~(SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT|SHADERSTAGE_PORTALMAP);

	if( !GL_Support( R_TEXTURECUBEMAP_EXT ))
	{
		MsgDev( D_ERROR, "Shader %s has an unsupported cubemap stage: %s.\n", shader->name );
		pass->textures[0] = tr.defaultTexture;
		pass->tcgen = TCGEN_BASE;
		return false;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'cubeMap' in shader '%s'\n", shader->name );
		return false;
	}

	com.strncpy( name, tok.string, sizeof( name ));
	flags = Shader_SetImageFlags( shader ) | addFlags | TF_CUBEMAP;
	
	while( 1 )
	{
		if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
			break;

		com.strncat( name, " ", sizeof( name ));
		com.strncat( name, tok.string, sizeof( name ));
	}

	pass->tcgen = TCGEN_BASE;
	pass->textures[pass->num_textures] = Shader_FindImage( shader, name, flags );
	pass->flags &= ~(SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT|SHADERSTAGE_PORTALMAP);
	if( pass->textures[pass->num_textures] != tr.defaultTexture ) pass->tcgen = tcgen;
	pass->num_textures++;

	return true;
}

static bool Shaderpass_Map( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	return Shaderpass_MapExt( shader, pass, 0, script );
}

static bool Shaderpass_ClampMap( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	return Shaderpass_MapExt( shader, pass, TF_CLAMP, script );
}

static bool Shaderpass_AnimMap( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	return Shaderpass_AnimMapExt( shader, pass, 0, script );
}

static bool Shaderpass_AnimClampMap( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	return Shaderpass_AnimMapExt( shader, pass, TF_CLAMP, script );
}

static bool Shaderpass_CubeMap( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	return Shaderpass_CubeMapExt( shader, pass, TF_CLAMP, TCGEN_REFLECTION, script );
}

static bool Shaderpass_ShadeCubeMap( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	return Shaderpass_CubeMapExt( shader, pass, TF_CLAMP, TCGEN_REFLECTION_CELLSHADE, script );
}

static bool Shaderpass_VideoMap( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	token_t	tok;

	Shader_FreePassCinematics( pass );

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'videoMap' in shader '%s'\n", shader->name );
		return false;
	}

	pass->tcgen = TCGEN_BASE;
	pass->cinHandle = R_StartCinematics( tok.string );
	pass->flags &= ~(SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT|SHADERSTAGE_PORTALMAP|SHADERSTAGE_ANIMFREQUENCY|SHADERSTAGE_FRAMES);
	pass->animFrequency[0] = pass->animFrequency[1] = 0.0f;
	pass->anim_offset = 0;

	return true;
}

static bool Shaderpass_NormalMap( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	int		flags;
	const char	*name;
	float		bumpScale = 0;
	token_t		tok;

	if( !GL_Support( R_SHADER_GLSL100_EXT ))
	{
		MsgDev( D_ERROR, "shader %s has a normalmap stage, while GLSL is not supported\n", shader->name );
		return false;
	}

	Shader_FreePassCinematics( pass );

	flags = Shader_SetImageFlags( shader );
	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'normalMap' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "$heightmap" ) )
	{
		if( !Com_ReadFloat( script, false, &bumpScale ))
		{
			MsgDev( D_WARN, "missing bumpScale parameter for 'normalMap $heightmap' in shader '%s', defaulting to 2.0f\n", shader->name );
			bumpScale = 2.0f;
		}

		if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
		{
			MsgDev( D_ERROR, "missing parameters for 'normalMap $heightmap' in shader '%s'\n", shader->name );
			return false;
		}
		name = va( "heightMap( \"%s\", %g );", tok.string, bumpScale );
	}
	else name = tok.string;

	pass->tcgen = TCGEN_BASE;
	pass->flags &= ~(SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT|SHADERSTAGE_PORTALMAP);
	pass->textures[1] = Shader_FindImage( shader, name, flags );
	pass->num_textures++;
	if( pass->textures[1] != tr.defaultTexture )
	{
		pass->program = DEFAULT_GLSL_PROGRAM;
		pass->program_type = PROGRAM_TYPE_MATERIAL;
	}

	// basemap parameter is optionally
	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
	{
		pass->textures[0] = tr.whiteTexture;
		return true;
	}

	if( !com.stricmp( tok.string, "$noimage" ))
		pass->textures[0] = tr.whiteTexture;
	else pass->textures[0] = Shader_FindImage( shader, tok.string, Shader_SetImageFlags( shader ));
	pass->num_textures++;

	return true;
}

static bool Shaderpass_Material( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	int		flags;
	float		bumpScale = 0;
	const char	*name;
	token_t		tok;

	if( !GL_Support( R_SHADER_GLSL100_EXT ))
	{
		MsgDev( D_ERROR, "shader %s has a normalmap stage, while GLSL is not supported\n", shader->name );
		return false;
	}

	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'material' in shader '%s'\n", shader->name );
		return false;
	}

	Shader_FreePassCinematics( pass );

	if( pass->num_textures )
	{
		pass->num_textures = 0;
		pass->flags &= ~(SHADERSTAGE_ANIMFREQUENCY|SHADERSTAGE_FRAMES);
	}

	flags = Shader_SetImageFlags( shader );
	
	if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ));
		return false;

	if( !com.stricmp( tok.string, "$rgb" ))
	{
		if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ));
			return false;
		name = va( "clearPixels( \"%s\", alpha );", tok.string );
	}
	else if( !com.stricmp( tok.string, "$alpha" ))
	{
		if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ));
			return false;
		name = va( "clearPixels( \"%s\", color );", tok.string );
	}
	else name = tok.string;

	pass->textures[0] = Shader_FindImage( shader, name, flags );
	if( pass->textures[0] == tr.defaultTexture )
	{
		MsgDev( D_ERROR, "failed to load base/diffuse image for material %s in shader %s.\n", tok.string, shader->name );
		return false;
	}
	pass->num_textures++;
		
	pass->textures[1] = pass->textures[2] = pass->textures[3] = NULL;

	pass->tcgen = TCGEN_BASE;
	pass->flags &= ~(SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT|SHADERSTAGE_PORTALMAP);

	while( 1 )
	{
		if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
			break;

		if( com.is_digit( tok.string ))
		{
			bumpScale = com.atoi( tok.string );
		}
		else if( !pass->textures[1] )
		{
			if( bumpScale )
				pass->textures[1] = Shader_FindImage( shader, va( "heightMap( \"%s\", %g );", tok.string, bumpScale ), flags );
			else pass->textures[1] = Shader_FindImage( shader, tok.string, flags );
	         		pass->num_textures++;
			if( pass->textures[1] == tr.defaultTexture )
			{
				MsgDev( D_WARN, "missing normalmap image '%s' in shader '%s'\n", tok.string, shader->name );
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
			if( com.stricmp( tok.string, "-" ) && r_lighting_specular->integer )
			{
				pass->textures[2] = Shader_FindImage( shader, tok.string, flags );
				if( pass->textures[2]  == tr.defaultTexture )
					MsgDev( D_WARN, "missing glossmap image '%s' in shader '%s'\n", tok.string, shader->name );
			}
			
			// set gloss to r_blacktexture so we know we have already parsed the gloss image
			if( pass->textures[2] == tr.defaultTexture ) pass->textures[2] = tr.blackTexture;
			pass->num_textures++;
		}
		else
		{
			pass->textures[3] = Shader_FindImage( shader, tok.string, flags );
			if( pass->textures[3] == tr.defaultTexture )
			{
				MsgDev( D_WARN, "missing decal image '%s' in shader '%s'\n", tok.string, shader->name );
				pass->textures[3] = NULL;
			}
			else pass->num_textures++;
		}
	}

	// black texture => no gloss, so don't waste time in the GLSL program
	if( pass->textures[2] == tr.blackTexture )
	{
		pass->num_textures--;
		pass->textures[2] = NULL;
          }
	if( pass->textures[1] )
		return true;

	// try loading default images
	if( Shaderpass_LoadMaterial( &pass->textures[1], &pass->textures[2], &pass->textures[3], pass->textures[0]->name, flags, bumpScale ))
	{
		pass->program = DEFAULT_GLSL_PROGRAM;
		pass->program_type = PROGRAM_TYPE_MATERIAL;
	}
	else MsgDev( D_WARN, "failed to load default images for material '%s' in shader '%s'\n", pass->textures[0]->name, shader->name );

	return true;
}

static bool Shaderpass_Distortion( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	int	flags;
	float	bumpScale = 0;
	token_t	tok;

	if( !GL_Support( R_SHADER_GLSL100_EXT ) || !r_portalmaps->integer )
	{
		if( !r_portalmaps->integer )
			MsgDev( D_ERROR, "shader %s has a distortion stage, while portalmaps is disabled\n", shader->name );
		else MsgDev( D_ERROR, "shader %s has a distortion stage, while GLSL is not supported\n", shader->name );
		return false;
	}

	Shader_FreePassCinematics( pass );

	if( pass->num_textures )
	{
		pass->num_textures = 0;
		pass->flags &= ~(SHADERSTAGE_ANIMFREQUENCY|SHADERSTAGE_FRAMES);
	}

	flags = Shader_SetImageFlags( shader );
	pass->flags &= ~(SHADERSTAGE_LIGHTMAP|SHADERSTAGE_DLIGHT|SHADERSTAGE_PORTALMAP);
	pass->textures[0] = pass->textures[1] = NULL;

	while( 1 )
	{
		if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
			break;

		if( com.is_digit( tok.string ))
		{
			bumpScale = com.atoi( tok.string );
		}
		else if( !pass->textures[0] )
		{
			pass->textures[0] = Shader_FindImage( shader, tok.string, flags );
			if( pass->textures[0] == tr.defaultTexture )
			{
				MsgDev( D_WARN, "missing dudvmap image '%s' in shader '%s'\n", tok.string, shader->name );
				pass->textures[0] = tr.blackTexture;
			}
			pass->program = DEFAULT_GLSL_DISTORTION_PROGRAM;
			pass->program_type = PROGRAM_TYPE_DISTORTION;
			pass->num_textures++;
		}
		else
		{
			if( bumpScale )
				pass->textures[1] = Shader_FindImage( shader, va( "heightMap( \"%s\", %g );", tok.string, bumpScale ), flags );
			else pass->textures[1] = Shader_FindImage( shader, tok.string, flags );
			if( pass->textures[1] == tr.defaultTexture ) pass->textures[1] = NULL;
			pass->num_textures++;
		}
	}

	if( pass->rgbGen.type == RGBGEN_UNKNOWN )
	{
		pass->rgbGen.type = RGBGEN_CONST;
		VectorClear( pass->rgbGen.args );
	}

	shader->flags |= (SHADER_PORTAL|SHADER_PORTAL_CAPTURE);
	return true;
}

static bool Shaderpass_RGBGen( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	token_t	tok;

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'rgbGen' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "identityLighting" )) pass->rgbGen.type = RGBGEN_IDENTITY_LIGHTING;
	else if( !com.stricmp( tok.string, "identity" )) pass->rgbGen.type = RGBGEN_IDENTITY;
	else if( !com.stricmp( tok.string, "wave" ))
	{
		if( !Shader_ParseFunc( script, pass->rgbGen.func, shader ))
		{
			MsgDev( D_ERROR, "missing waveform parameters for 'rgbGen wave' in shader '%s'\n", shader->name );
			return false;
		}
		pass->rgbGen.type = RGBGEN_WAVE;
		VectorSet( pass->rgbGen.args, 1.0f, 1.0f, 1.0f );
	}
	else if( !com.stricmp( tok.string, "colorWave" ))
	{
		if( !Shader_ParseVector( script, pass->rgbGen.args, 3 ))
		{
			MsgDev( D_ERROR, "missing parameters for 'rgbGen colorWave' in shader '%s'\n", shader->name );
			return false;
		}
		if( !Shader_ParseFunc( script, pass->rgbGen.func, shader ))
		{
			MsgDev( D_ERROR, "missing waveform parameters for 'rgbGen colorWave' in shader '%s'\n", shader->name );
			return false;
		}
		pass->rgbGen.type = RGBGEN_COLORWAVE;
	}
	else if( !com.stricmp( tok.string, "entity" )) pass->rgbGen.type = RGBGEN_ENTITY;
	else if( !com.stricmp( tok.string, "oneMinusEntity" )) pass->rgbGen.type = RGBGEN_ONE_MINUS_ENTITY;
	else if( !com.stricmp( tok.string, "vertex" )) pass->rgbGen.type = RGBGEN_VERTEX;
	else if( !com.stricmp( tok.string, "oneMinusVertex" )) pass->rgbGen.type = RGBGEN_ONE_MINUS_VERTEX;
	else if( !com.stricmp( tok.string, "lightingDiffuseOnly" )) pass->rgbGen.type = RGBGEN_LIGHTING_DIFFUSE_ONLY;
	else if( !com.stricmp( tok.string, "lightingDiffuse" )) pass->rgbGen.type = RGBGEN_LIGHTING_DIFFUSE;
	else if( !com.stricmp( tok.string, "exactVertex" )) pass->rgbGen.type = RGBGEN_EXACT_VERTEX;
	else if( !com.stricmp( tok.string, "lightingAmbientOnly" )) 
	{
		// optional undocs parm 'invLight'
		if( Com_ReadToken( script, false, &tok ))
			if( !com.stricmp( tok.string, "invLight" ))
				pass->rgbGen.args[0] = true;
		pass->rgbGen.type = RGBGEN_LIGHTING_AMBIENT_ONLY;
	}
	else if( !com.stricmp( tok.string, "const" ) || !com.stricmp( tok.string, "constant" ))
	{
		float	div;
		vec3_t	color;

		if( !Shader_ParseVector( script, color, 3 ))
		{
			MsgDev( D_ERROR, "missing parameters for 'rgbGen const' in shader '%s'\n", shader->name );
			return false;
		}

		if( !r_ignorehwgamma->integer )
			div = 1.0f / pow( 2, max( 0, floor( r_overbrightbits->value ) ) );
		else div = 1.0f;

		pass->rgbGen.type = RGBGEN_CONST;
		ColorNormalize( color, pass->rgbGen.args );
		VectorScale( pass->rgbGen.args, div, pass->rgbGen.args );
	}
	else if( !com.stricmp( tok.string, "custom" ) || !com.stricmp( tok.string, "teamcolor" ))
	{
		// the "teamcolor" thing comes from warsow
		pass->rgbGen.type = RGBGEN_CUSTOM;

		if( !Com_ReadFloat( script, false, &pass->rgbGen.args[0] ))
			MsgDev( D_WARN, "missing parameters for 'rgbGen teamcolor' in shader '%s'\n", shader->name );
		if( pass->rgbGen.args[0] < 0 || pass->rgbGen.args[0] >= NUM_CUSTOMCOLORS )
			pass->rgbGen.args[0] = 0;
	}
	else
	{
		MsgDev( D_WARN, "unknown 'rgbGen' parameter '%s' in shader '%s'\n", tok.string, shader->name );
		pass->rgbGen.type = RGBGEN_IDENTITY;
		Shader_SkipLine( script );
		return true;
	}
	return true;
}

static bool Shaderpass_AlphaGen( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	token_t	tok;

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'alphaGen' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "portal" ))
	{
		pass->alphaGen.type = ALPHAGEN_PORTAL;
		Com_ReadFloat( script, false, &pass->alphaGen.args[0] );
		pass->alphaGen.args[0] = fabs( pass->alphaGen.args[0] );

		if( !pass->alphaGen.args[0] ) pass->alphaGen.args[0] = 256.0f;
		pass->alphaGen.args[0] = 1.0f / pass->alphaGen.args[0];
	}
	else if( !com.stricmp( tok.string, "identity" )) pass->alphaGen.type = ALPHAGEN_IDENTITY;
	else if( !com.stricmp( tok.string, "vertex" )) pass->alphaGen.type = ALPHAGEN_VERTEX;
	else if( !com.stricmp( tok.string, "oneMinusVertex" )) pass->alphaGen.type = ALPHAGEN_ONE_MINUS_VERTEX;
	else if( !com.stricmp( tok.string, "entity" )) pass->alphaGen.type = ALPHAGEN_ENTITY;
	else if( !com.stricmp( tok.string, "wave" ))
	{
		if( !Shader_ParseFunc( script, pass->alphaGen.func, shader ))
		{
			MsgDev( D_ERROR, "missing waveform parameters for 'alphaGen wave' in shader '%s'\n", shader->name );
			return false;
		}
		pass->alphaGen.type = ALPHAGEN_WAVE;
	}
	else if( !com.stricmp( tok.string, "alphaWave" ))
	{
		if( !Com_ReadToken( script, false, &tok ))
		{
			MsgDev( D_ERROR, "missing parameters for 'alphaGen alphaWave' in shader '%s'\n", shader->name );
			return false;
		}
		pass->alphaGen.args[0] = com.atof( tok.string );

		if( !Shader_ParseFunc( script, pass->alphaGen.func, shader ))
		{
			MsgDev( D_ERROR, "missing waveform parameters for 'alphaGen alphaWave' in shader '%s'\n", shader->name );
			return false;
		}
		pass->alphaGen.type = ALPHAGEN_ALPHAWAVE;
	}
	else if( !com.stricmp( tok.string, "lightingSpecular" ))
	{
		pass->alphaGen.type = ALPHAGEN_SPECULAR;
		Com_ReadFloat( script, false, &pass->alphaGen.args[0] );
		pass->alphaGen.args[0] = fabs( pass->alphaGen.args[0] );
		if( !pass->alphaGen.args[0] ) pass->alphaGen.args[0] = 5.0f;
	}
	else if( !com.stricmp( tok.string, "const" ) || !com.stricmp( tok.string, "constant" ))
	{
		pass->alphaGen.type = ALPHAGEN_CONST;
		Com_ReadFloat( script, false, &pass->alphaGen.args[0] );
		pass->alphaGen.args[0] = fabs( pass->alphaGen.args[0] );
	}
	else if( !com.stricmp( tok.string, "dot" ))
	{
		Com_ReadFloat( script, false, &pass->alphaGen.args[0] );
		pass->alphaGen.args[0] = fabs( pass->alphaGen.args[0] );
		Com_ReadFloat( script, false, &pass->alphaGen.args[1] );
		pass->alphaGen.args[1] = fabs( pass->alphaGen.args[1] );
		if( !pass->alphaGen.args[1] ) pass->alphaGen.args[1] = 1.0f;
		pass->alphaGen.type = ALPHAGEN_DOT;
	}
	else if( !com.stricmp( tok.string, "oneMinusDot" ) )
	{
		Com_ReadFloat( script, false, &pass->alphaGen.args[0] );
		pass->alphaGen.args[0] = fabs( pass->alphaGen.args[0] );
		Com_ReadFloat( script, false, &pass->alphaGen.args[1] );
		pass->alphaGen.args[1] = fabs( pass->alphaGen.args[1] );
		if( !pass->alphaGen.args[1] ) pass->alphaGen.args[1] = 1.0f;
		pass->alphaGen.type = ALPHAGEN_ONE_MINUS_DOT;
	}
	else if( !com.stricmp( tok.string, "fade" ))
	{
		if( !Com_ReadToken( script, false, &tok ))
		{
			pass->alphaGen.args[0] = 0.0f;
			pass->alphaGen.args[1] = 256.0f;
			pass->alphaGen.args[2] = 1.0f / 256.0f;
		}
		else
		{
			pass->alphaGen.args[0] = com.atof( tok.string );
			if( !Com_ReadToken( script, false, &tok ))
			{
				MsgDev( D_ERROR, "missing parameters for 'alphaGen fade' in shader '%s'\n", shader->name );
				return false;
			}

			pass->alphaGen.args[1] = com.atof( tok.string );
			pass->alphaGen.args[2] = pass->alphaGen.args[1] - pass->alphaGen.args[0];
			if( pass->alphaGen.args[2] ) pass->alphaGen.args[2] = 1.0f / pass->alphaGen.args[2];
		}
		pass->alphaGen.type = ALPHAGEN_FADE;
	}
	else if( !com.stricmp( tok.string, "oneMinusFade" ))
	{
		if( !Com_ReadToken( script, false, &tok ))
		{
			pass->alphaGen.args[0] = 0.0f;
			pass->alphaGen.args[1] = 256.0f;
			pass->alphaGen.args[2] = 1.0f / 256.0f;
		}
		else
		{
			pass->alphaGen.args[0] = com.atof( tok.string );

			if( !Com_ReadToken( script, false, &tok ))
			{
				MsgDev( D_ERROR, "missing parameters for 'alphaGen oneMinusFade' in shader '%s'\n", shader->name );
				return false;
			}

			pass->alphaGen.args[1] = com.atof( tok.string );
			pass->alphaGen.args[2] = pass->alphaGen.args[1] - pass->alphaGen.args[0];
			if( pass->alphaGen.args[2] ) pass->alphaGen.args[2] = 1.0f / pass->alphaGen.args[2];
		}
		pass->alphaGen.type = ALPHAGEN_ONE_MINUS_FADE;
	}
	else
	{
		MsgDev( D_WARN, "unknown 'alphaGen' parameter '%s' in shader '%s'\n", tok.string, shader->name );
		pass->alphaGen.type = ALPHAGEN_IDENTITY;
		Shader_SkipLine( script );
		return true;
	}
	return true;
}

static _inline int Shaderpass_SrcBlendBits( const char *token )
{
	if( !com.stricmp( token, "gl_zero" ))
		return GLSTATE_SRCBLEND_ZERO;
	if( !com.stricmp( token, "gl_one" ))
		return GLSTATE_SRCBLEND_ONE;
	if( !com.stricmp( token, "gl_dst_color" ))
		return GLSTATE_SRCBLEND_DST_COLOR;
	if( !com.stricmp( token, "gl_one_minus_dst_color" ))
		return GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR;
	if( !com.stricmp( token, "gl_src_alpha" ))
		return GLSTATE_SRCBLEND_SRC_ALPHA;
	if( !com.stricmp( token, "gl_one_minus_src_alpha" ))
		return GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	if( !com.stricmp( token, "gl_dst_alpha" ))
		return GLSTATE_SRCBLEND_DST_ALPHA;
	if( !com.stricmp( token, "gl_one_minus_dst_alpha" ))
		return GLSTATE_SRCBLEND_ONE_MINUS_DST_ALPHA;
	return GLSTATE_SRCBLEND_ONE;
}

static _inline int Shaderpass_DstBlendBits( const char *token )
{
	if( !com.stricmp( token, "gl_zero" ))
		return GLSTATE_DSTBLEND_ZERO;
	if( !com.stricmp( token, "gl_one" ))
		return GLSTATE_DSTBLEND_ONE;
	if( !com.stricmp( token, "gl_src_color" ))
		return GLSTATE_DSTBLEND_SRC_COLOR;
	if( !com.stricmp( token, "gl_one_minus_src_color" ))
		return GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR;
	if( !com.stricmp( token, "gl_src_alpha" ))
		return GLSTATE_DSTBLEND_SRC_ALPHA;
	if( !com.stricmp( token, "gl_one_minus_src_alpha" ))
		return GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	if( !com.stricmp( token, "gl_dst_alpha" ))
		return GLSTATE_DSTBLEND_DST_ALPHA;
	if( !com.stricmp( token, "gl_one_minus_dst_alpha" ))
		return GLSTATE_DSTBLEND_ONE_MINUS_DST_ALPHA;
	return GLSTATE_DSTBLEND_ONE;
}

static bool Shaderpass_BlendFunc( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	token_t	tok;

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'blendFunc' in shader '%s'\n", shader->name );
		return false;
	}

	pass->glState &= ~(GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK);
	if( !com.stricmp( tok.string, "blend" ))
	{
		pass->glState |= GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if( !com.stricmp( tok.string, "filter" ))
	{
		pass->glState |= GLSTATE_SRCBLEND_DST_COLOR|GLSTATE_DSTBLEND_ZERO;
	}
	else if( !com.stricmp( tok.string, "add" ))
	{
		pass->glState |= GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
	}
	else
	{
		pass->glState |= Shaderpass_SrcBlendBits( tok.string );

		if( !Com_ReadToken( script, false, &tok ))
		{
			MsgDev( D_ERROR, "missing parameters for 'blendFunc' in shader '%s'\n", shader->name );
			return false;
		}
		pass->glState |= Shaderpass_DstBlendBits( tok.string );
	}
	return true;
}

static bool Shaderpass_AlphaFunc( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	token_t	tok;

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'alphaFunc' in shader '%s'\n", shader->name );
		return false;
	}

	pass->glState &= ~(GLSTATE_ALPHAFUNC);
	if( !com.stricmp( tok.string, "GT0" ))
		pass->glState |= GLSTATE_AFUNC_GT0;
	else if( !com.stricmp( tok.string, "LT128" ))
		pass->glState |= GLSTATE_AFUNC_LT128;
	else if( !com.stricmp( tok.string, "GE128" ))
		pass->glState |= GLSTATE_AFUNC_GE128;
	else
	{
		MsgDev( D_ERROR, "unknown 'alphaFunc' parameter '%s' in shader '%s'\n", tok.string, shader->name );
		return false;
	}
	return true;
}

static bool Shaderpass_DepthFunc( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	token_t	tok;

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'depthFunc' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "equal" ))
		pass->glState |= GLSTATE_DEPTHFUNC_EQ;
	else if( !com.stricmp( tok.string, "lequal" ))
		pass->glState &= ~GLSTATE_DEPTHFUNC_EQ;
	else
	{
		MsgDev( D_ERROR, "unknown 'depthFunc' parameter '%s' in shader '%s'\n", tok.string, shader->name );
		return false;
	}
	return true;
}

static bool Shaderpass_DepthWrite( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	shader->flags |= SHADER_DEPTHWRITE;
	pass->glState |= GLSTATE_DEPTHWRITE;

	return true;
}

static bool Shaderpass_TcMod( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	int	i;
	tcMod_t	*tcMod;
	token_t	tok;

	if( pass->numtcMods == MAX_SHADER_TCMODS )
	{
		MsgDev( D_ERROR, "shader %s has too many tcmods\n", shader->name );
		return false;
	}

	tcMod = &pass->tcMods[pass->numtcMods];

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'tcMod' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "rotate" ))
	{
		if( !Com_ReadFloat( script, false, &tcMod->args[0] ))
		{
			MsgDev( D_ERROR, "missing parameters for 'tcMod rotate' in shader '%s'\n", shader->name );
			return false;
		}
		tcMod->args[0] = -tcMod->args[0] / 360.0f;
		Com_ReadFloat( script, false, NULL );	// skip second parm if present
		if( !tcMod->args[0] ) return true;
		tcMod->type = TCMOD_ROTATE;
	}
	else if( !com.stricmp( tok.string, "scale" ))
	{
		if( !Shader_ParseVector( script, tcMod->args, 2 ))
		{
			MsgDev( D_ERROR, "missing parameters for 'tcMod scale' in shader '%s'\n", shader->name );
			return false;
		}
		tcMod->type = TCMOD_SCALE;
	}
	else if( !com.stricmp( tok.string, "scroll" ))
	{
		if( !Shader_ParseVector( script, tcMod->args, 2 ))
		{
			MsgDev( D_ERROR, "missing parameters for 'tcMod scroll' in shader '%s'\n", shader->name );
			return false;
		}
		tcMod->type = TCMOD_SCROLL;
	}
	else if( !com.stricmp( tok.string, "translate" ))
	{
		if( !Shader_ParseVector( script, tcMod->args, 2 ))
		{
			MsgDev( D_ERROR, "missing parameters for 'tcMod translate' in shader '%s'\n", shader->name );
			return false;
		}
		tcMod->type = TCMOD_TRANSLATE;
	}
	else if( !com.stricmp( tok.string, "stretch" ))
	{
		waveFunc_t	func;

		if( !Shader_ParseFunc( script, &func, shader ))
		{
			MsgDev( D_ERROR, "missing waveform parameters for 'tcMod stretch' in shader '%s'\n", shader->name );
			return false;
		}
		tcMod->args[0] = func.type;
		tcMod->args[5] = func.tableIndex;
		for( i = 1; i < 5; i++ )
			tcMod->args[i] = func.args[i-1];
		tcMod->type = TCMOD_STRETCH;
	}
	else if( !com.stricmp( tok.string, "transform" ))
	{
		if( !Shader_ParseVector( script, tcMod->args, 6 ))
		{
			MsgDev( D_ERROR, "missing parameters for 'tcMod transform' in shader '%s'\n", shader->name );
			return false;
		}
		tcMod->args[4] = tcMod->args[4] - floor( tcMod->args[4] );
		tcMod->args[5] = tcMod->args[5] - floor( tcMod->args[5] );
		tcMod->type = TCMOD_TRANSFORM;
	}
	else if( !com.stricmp( tok.string, "turb" ))
	{
		if( !Shader_ParseVector( script, tcMod->args, 4 ))
		{
			MsgDev( D_ERROR, "missing parameters for 'tcMod turb' in shader '%s'\n", shader->name );
			return false;
		}
		Com_ReadFloat( script, false, NULL );	// skip five parm if present
		tcMod->type = TCMOD_TURB;
	}
	else if( !com.stricmp( tok.string, "conveyor" ))
	{
		tcMod->type = TCMOD_CONVEYOR;
	}
	else
	{
		MsgDev( D_WARN, "unknown 'tcMod' parameter '%s' in shader '%s'\n", tok.string, shader->name );
		Shader_SkipLine( script );
		return true;
	}

	r_currentPasses[shader->num_stages].numtcMods++;
	return true;
}

static bool Shaderpass_TcGen( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	token_t	tok;

	if( !Com_ReadToken( script, false, &tok ))
	{
		MsgDev( D_ERROR, "missing parameters for 'tcGen' in shader '%s'\n", shader->name );
		return false;
	}

	if( !com.stricmp( tok.string, "base" ) || !com.stricmp( tok.string, "texture" ))
		pass->tcgen = TCGEN_BASE;
	else if( !com.stricmp( tok.string, "lightmap" ))
		pass->tcgen = TCGEN_LIGHTMAP;
	else if( !com.stricmp( tok.string, "environment" ))
		pass->tcgen = TCGEN_ENVIRONMENT;
	else if( !com.stricmp( tok.string, "vector" ))
	{
		if(!Shader_ParseVector( script, &pass->tcgenVec[0], 4 ))
		{
			MsgDev( D_ERROR, "missing parameters for 'tcGen vector' in shader '%s'\n", shader->name );
			return false;
		}
		if(!Shader_ParseVector( script, &pass->tcgenVec[4], 4 ))
		{
			MsgDev( D_ERROR, "missing parameters for 'tcGen vector' in shader '%s'\n", shader->name );
			return false;
		}
		pass->tcgen = TCGEN_VECTOR;
	}
	else if( !com.stricmp( tok.string, "warp" ))
		pass->tcgen = TCGEN_WARP;
	else if( !com.stricmp( tok.string, "reflection" ))
		pass->tcgen = TCGEN_REFLECTION;
	else if( !com.stricmp( tok.string, "normal"))
		pass->tcgen = TCGEN_NORMAL;
	else if( !com.stricmp( tok.string, "cellShade" ))
		pass->tcgen = TCGEN_REFLECTION_CELLSHADE;
	else
	{
		MsgDev( D_WARN, "unknown 'tcGen' parameter '%s' in shader '%s'\n", tok.string, shader->name );
		Shader_SkipLine( script );
		return true;
	}
	return true;
}

static bool Shaderpass_Detail( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	pass->flags |= SHADERSTAGE_DETAIL;
	return true;
}

static bool Shaderpass_RenderMode( ref_shader_t *shader, ref_stage_t *pass, script_t *script )
{
	shader->flags |= SHADER_RENDERMODE;
	pass->flags |= SHADERSTAGE_RENDERMODE;
	return true;
}

static const ref_parsekey_t shaderpasskeys[] =
{
{ "rgbGen",	Shaderpass_RGBGen		},
{ "blendFunc",	Shaderpass_BlendFunc	},
{ "depthFunc",	Shaderpass_DepthFunc	},
{ "depthWrite",	Shaderpass_DepthWrite	},
{ "alphaFunc",	Shaderpass_AlphaFunc	},
{ "tcMod",	Shaderpass_TcMod		},
{ "AnimFrequency",	Shaderpass_AnimFrequency	},
{ "map",		Shaderpass_Map		},
{ "animMap",	Shaderpass_AnimMap		},
{ "cubeMap",	Shaderpass_CubeMap		},
{ "shadeCubeMap",	Shaderpass_ShadeCubeMap	},
{ "videoMap",	Shaderpass_VideoMap		},
{ "clampMap",	Shaderpass_ClampMap		},
{ "animClampMap",	Shaderpass_AnimClampMap	},
{ "normalMap",	Shaderpass_NormalMap	},
{ "material",	Shaderpass_Material		},
{ "distortion",	Shaderpass_Distortion	},
{ "tcGen",	Shaderpass_TcGen		},
{ "alphaGen",	Shaderpass_AlphaGen		},
{ "detail",	Shaderpass_Detail		},
{ "renderMode",	Shaderpass_RenderMode	},
{ NULL,		NULL			}
};

// ===============================================================
static void Shader_ParseFile( script_t *script, const char *name )
{
	ref_script_t	*shaderScript;
	tableFlags_t	table_flags = 0;
	char		*buffer, *end;
	int		tableStatus = 0;
	uint		hashKey;
	token_t		tok;
	size_t		size;

	while( 1 )
	{
		// parse the name
		if( !Com_ReadToken( script, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &tok ))
			break; // end of data

		// check for table parms
		while( 1 )
		{
			if( !com.stricmp( tok.string, "clamp" ))
				table_flags |= TABLE_CLAMP;
			else if( !com.stricmp( tok.string, "snap" ))
				table_flags |= TABLE_SNAP;
			else if( !com.stricmp( tok.string, "table" ))
			{
				if( !R_ParseTable( script, table_flags ))
					tableStatus = -1; // parsing failed
				else tableStatus = 1;
				break;
			} 
			else
			{
				tableStatus = table_flags = 0;  // not a table
				break;
			}
			Com_ReadToken( script, false, &tok );
		}

		if( tableStatus != 0 )
		{
			if( tableStatus == -1 )
				Com_SkipRestOfLine( script );
			continue; // table status will be reset on a next loop
		}

		// parse the script
		buffer = script->text;
		Com_SkipBracedSection( script, 0 );
		end = script->text;

		if( !buffer ) buffer = script->buffer; // missing body ?
		if( !end ) end = script->buffer + script->size;	// EOF ?
		size = end - buffer;

		// store the script
		shaderScript = Mem_Alloc( r_shaderpool, sizeof( ref_script_t ));
		shaderScript->name = Shader_CopyString( tok.string );
		shaderScript->type = SHADER_INVALID;

		shaderScript->source =Shader_CopyString( name );
		shaderScript->line = tok.line;
		shaderScript->buffer = Mem_Alloc( r_shaderpool, size + 1 );
		Mem_Copy( shaderScript->buffer, buffer, size );
		shaderScript->buffer[size] = 0; // terminator
		shaderScript->size = size;

		// add to hash table
		hashKey = Com_HashKey( shaderScript->name, SHADERS_HASH_SIZE );
		shaderScript->nextHash = r_shaderScriptsHash[hashKey];
		r_shaderScriptsHash[hashKey] = shaderScript;
	}
}

static ref_script_t *Shader_GetCache( const char *name, int type, uint hashKey )
{
	ref_script_t	*cache = NULL;

	// see if there's a script for this shader
	for( cache = r_shaderScriptsHash[hashKey]; cache; cache = cache->nextHash )
	{
		if( !com.stricmp( cache->name, name ))
		{
			if( cache->type == SHADER_INVALID ) break; // not initialized
			if( cache->type == type ) break;
		}
	}
	return cache;
}

/*
===============
R_ShaderList_f
===============
*/
void R_ShaderList_f( void )
{
	ref_shader_t	*shader;
	int		i, shaderCount;

	Msg( "\n" );
	Msg( "-----------------------------------\n" );

	for( i = shaderCount = 0, shader = r_shaders; i < r_numShaders; i++, shader++ )
	{
		if( !shader->shadernum ) continue;

		Msg( "%i ", shader->num_stages );

		switch( shader->type )
		{
		case SHADER_SKY:
			Msg( "sky  " );
			break;
		case SHADER_FARBOX:
			Msg( "far  " );
			break;
		case SHADER_NEARBOX:
			Msg( "near " );
			break;
		case SHADER_TEXTURE:
			Msg( "bsp  " );
			break;
		case SHADER_VERTEX:
			Msg( "vert " );
			break;
		case SHADER_STUDIO:
			Msg( "mdl  " );
			break;
		case SHADER_ALIAS:
			Msg( "alias" );
			break;
		case SHADER_FONT:
			Msg( "font " );
			break;
		case SHADER_SPRITE:
			Msg( "spr  " );
			break;
		case SHADER_FLARE:
			Msg( "flare" );
			break;
		case SHADER_PLANAR_SHADOW:
			Msg( "shdw " );
			break;
		case SHADER_OPAQUE_OCCLUDER:
			Msg( "occl " );
			break;
		case SHADER_OUTLINE:
			Msg( "outl " );
			break;
		case SHADER_NOMIP:
			Msg( "pic  " );
			break;
		case SHADER_GENERIC:
			Msg( "gen  " );
			break;
		default:
			Msg( "?? %i", shader->type );
			break;
		}
		Msg( " %s", shader->name );
		if( shader->flags & SHADER_DEFAULTED )
			Msg( "  DEFAULTED\n" );
		else Msg( "\n" );
		shaderCount++;
	}

	Msg( "-----------------------------------\n" );
	Msg( "%i total shaders\n", shaderCount );
	Msg( "\n" );
}

/*
===============
R_ShaderDump_f
===============
*/
void R_ShaderDump_f( void )
{
	const char	*name;
	ref_shader_t	*shader;
	ref_script_t	*cache;
	uint		hashKey;
	
	if(( Cmd_Argc() < 2 ) && !r_debug_surface )
	{
		Msg( "Usage: shaderdump <name>\n" );
		return;
	}

	if( Cmd_Argc() < 2 )
		name = r_debug_surface->shader->name;
	else name = Cmd_Argv( 1 );

	hashKey = Com_HashKey( name, SHADERS_HASH_SIZE );
	for( shader = r_shadersHash[hashKey]; shader; shader = shader->nextHash )
	{
		if( !com.stricmp( shader->name, name ))
			break;
	}
	if( !shader )
	{
		Msg( "could not find shader %s\n", name );
		return;
	}
	cache = Shader_GetCache( name, shader->type, hashKey );
	if( !cache )
	{
		Msg( "could not find shader %s in cache\n", name );
		return;
	}

	Msg( "found in %s:\n\n", cache->source );
	Msg( "^2%s%s\n", name, cache->buffer );
}

bool R_ShaderCheckCache( const char *name )
{
	return (Shader_GetCache( name, SHADER_INVALID, Com_HashKey( name, SHADERS_HASH_SIZE ))) ? true : false;
}

void R_RegisterBuiltinShaders( void )
{
	tr.defaultShader = R_LoadShader( MAP_DEFAULT_SHADER, SHADER_NOMIP, true, (TF_NOMIPMAP|TF_NOPICMIP), SHADER_UNKNOWN );
}

void R_InitShaders( void )
{
	script_t	*script;
	search_t	*t;
	int	i;

	MsgDev( D_NOTE, "R_InitShaders()\n" );
	r_shaderpool = Mem_AllocPool( "Shader Zone" );

	t = FS_Search( "scripts/*.shader", true );
	if( !t ) MsgDev( D_ERROR, "couldn't find any shaders!\n");

	// load them
	for( i = 0; t && i < t->numfilenames; i++ )
	{
		script = Com_OpenScript( t->filenames[i], NULL, 0 );
		if( !script )
		{
			MsgDev( D_ERROR, "Couldn't load '%s'\n", t->filenames[i] );
			continue;
		}

		// parse this file
		MsgDev( D_LOAD, "%s\n", t->filenames[i] );
		Shader_ParseFile( script, t->filenames[i] );
		Com_CloseScript( script );
	}
	if( t ) Mem_Free( t ); // free search

	// init sprite frames
	for( i = 0; i < MAX_STAGE_TEXTURES; i++ )
		r_stageTexture[i] = tr.defaultTexture;

	r_shaderTwoSided = 0;
	r_stageAnimFrequency = 0.0f;
	r_numStageTextures = 0;
	r_shaderRenderMode = kRenderNormal;

	R_RegisterBuiltinShaders ();
}

void Shader_TouchImages( ref_shader_t *shader, bool free_unused )
{
	int		i, j;
	int		c_total = 0;
	ref_stage_t	*stage;
	texture_t		*texture;

	Com_Assert( shader == NULL );
 	if( !free_unused ) shader->touchFrame = tr.registration_sequence;

	for( i = 0; i < shader->num_stages; i++ )
	{
		stage = &shader->stages[i];

		if( free_unused && stage->cinHandle )
			Shader_FreePassCinematics( stage );

		for( j = 0; j < stage->num_textures; j++ )
		{
			// prolonge registration for all shader textures
			texture = stage->textures[j];

			if( !texture || !texture->texnum ) continue;
			if( texture->flags & TF_STATIC ) continue;
			if( free_unused && texture->touchFrame != tr.registration_sequence )
				R_FreeImage( texture );
			else texture->touchFrame = tr.registration_sequence;
			c_total++; // just for debug
		}
	}

	if( shader->flags & SHADER_SKYPARMS && shader->skyParms )
	{
		for( i = 0; i < 6; i++ )
		{
			if( shader->skyParms->farboxShaders[i] )
				Shader_TouchImages( shader->skyParms->farboxShaders[i], free_unused );
			if( shader->skyParms->nearboxShaders[i] )
				Shader_TouchImages( shader->skyParms->nearboxShaders[i], free_unused );
		}
	}
}

void Shader_FreeShader( ref_shader_t *shader )
{
	uint		i, hashKey;
	ref_shader_t	*cur, **prev;
	shader_t		handle;
	ref_stage_t	*pass;

	Com_Assert( shader == NULL );

	if( !shader->shadernum )
		return;	// already freed

	// free uinque shader images only
	Shader_TouchImages( shader, true );

	// remove from hash table
	hashKey = Com_HashKey( shader->name, SHADERS_HASH_SIZE );
	prev = &r_shadersHash[hashKey];

	while( 1 )
	{
		cur = *prev;
		if( !cur ) break;

		if( cur == shader )
		{
			*prev = cur->nextHash;
			break;
		}
		prev = &cur->nextHash;
	}

	handle = shader - r_shaders;
	if(( shader->flags & SHADER_SKYPARMS ) && shader->skyParms )
	{
		for( i = 0; i < 6; i++ )
		{
			if( shader->skyParms->farboxShaders[i] )
				Shader_FreeShader( shader->skyParms->farboxShaders[i] );
			if( shader->skyParms->nearboxShaders[i] )
				Shader_FreeShader( shader->skyParms->nearboxShaders[i] );
		}
		R_FreeSkydome( shader->skyParms );
		shader->skyParms = NULL;
	}

	if( shader->flags & SHADER_VIDEOMAP )
	{
		for( i = 0, pass = shader->stages; i < shader->num_stages; i++, pass++ )
			Shader_FreePassCinematics( pass );
	}

	// free all allocated memory by shader
	Shader_Free( shader->name );
	Mem_Set( shader, 0, sizeof( ref_shader_t ));
}

void R_ShutdownShaders( void )
{
	int		i;
	ref_shader_t	*shader;

	if( !r_shaderpool )
		return;

	for( i = 0, shader = r_shaders; i < r_numShaders; i++, shader++ )
	{
		if( !shader->shadernum ) continue;	// already freed
		Shader_FreeShader( shader );
	}

	Mem_FreePool( &r_shaderpool );
	Mem_Set( r_shaderScriptsHash, 0, sizeof( r_shaderScriptsHash ));
	Mem_Set( r_shadersHash, 0, sizeof( r_shadersHash ));
	Mem_Set( r_shaders, 0, sizeof( r_shaders ));
	r_numShaders = 0;
}

void Shader_SetBlendmode( ref_stage_t *pass )
{
	int	blendsrc, blenddst;

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
	else if( blendsrc == GLSTATE_DSTBLEND_SRC_COLOR && blenddst == GLSTATE_SRCBLEND_DST_COLOR )
		pass->flags |= SHADERSTAGE_BLEND_DECAL;	// for detail textures
}

static bool Shader_ParseCommand( ref_shader_t *shader, script_t *script, const char *command )
{
	const ref_parsekey_t	*cmd;

	for( cmd = shaderkeys; cmd->name != NULL; cmd++ )
	{
		if( !com.stricmp( cmd->name, command ))
			return cmd->func( shader, NULL, script );
	}

	// compiler or mapeditor commands ignored silently
	if( !com.strnicmp( "q3map_", command, 6 ) || !com.strnicmp( "qer_", command, 4 ))
	{
		Com_SkipRestOfLine( script );
		return true;
	}

	MsgDev( D_WARN, "unknown general command '%s' in shader '%s'\n", command, shader->name );
	Shader_SkipLine( script );
	return true;
}

static bool Shader_ParseStageCommand( ref_shader_t *shader, ref_stage_t *stage, script_t *script, const char *command )
{
	const ref_parsekey_t	*cmd;

	for( cmd = shaderpasskeys; cmd->name != NULL; cmd++ )
	{
		if(!com.stricmp( cmd->name, command ))
			return cmd->func( shader, stage, script );
	}

	MsgDev( D_WARN, "unknown stage command '%s' in shader '%s'\n", command, shader->name );
	Shader_SkipLine( script );
	return true;
}

static bool Shader_ParseShader( ref_shader_t *shader, script_t *script )
{
	ref_stage_t	*stage;
	token_t		tok;
	int		n;

	if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &tok ))
	{
		MsgDev( D_ERROR, "shader '%s' has an empty script\n", shader->name );
		return false;
	}

	// parse the shader
	if( !com.stricmp( tok.string, "{" ))
	{
		while( 1 )
		{
			if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &tok ))
			{
				MsgDev( D_ERROR, "no concluding '}' in shader '%s'\n", shader->name );
				return false; // end of data
			}

			if( !com.stricmp( tok.string, "}" )) break; // end of shader

			// parse a stage
			if( !com.stricmp( tok.string, "{" ))
			{
				// create a new stage
				if( shader->num_stages == MAX_SHADER_STAGES )
				{
					MsgDev( D_ERROR, "MAX_SHADER_STAGES hit in shader '%s'\n", shader->name );
					return false;
				}

				n = shader->num_stages;

				// set defaults
				stage = &r_currentPasses[n];
				Mem_Set( stage, 0, sizeof( ref_stage_t ));
				stage->rgbGen.type = RGBGEN_UNKNOWN;
				stage->rgbGen.args = r_currentRGBgenArgs[n];
				stage->rgbGen.func = &r_currentRGBgenFuncs[n];
				stage->alphaGen.type = ALPHAGEN_UNKNOWN;
				stage->alphaGen.args = r_currentAlphagenArgs[n];
				stage->alphaGen.func = &r_currentAlphagenFuncs[n];
				stage->tcgenVec = r_currentTcGen[n][0];
				stage->tcgen = TCGEN_BASE;
				stage->tcMods = r_currentTcmods[n];

				// parse it
				while( 1 )
				{
					if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &tok ))
					{
						MsgDev( D_ERROR, "no matching '}' in shader '%s'\n", shader->name );
						return false; // end of data
					}

					if( !com.stricmp( tok.string, "}" )) break; // end of stage

					// parse the command
					if( !Shader_ParseStageCommand( shader, stage, script, tok.string ))
						return false;
				}

				if((( stage->glState & GLSTATE_SRCBLEND_MASK ) == GLSTATE_SRCBLEND_ONE ) && (( stage->glState & GLSTATE_DSTBLEND_MASK ) == GLSTATE_DSTBLEND_ZERO ))
				{
					stage->glState &= ~(GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK);
					stage->flags |= SHADERSTAGE_BLEND_MODULATE;
				}

				if(!( stage->glState & (GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK)))
					stage->glState |= GLSTATE_DEPTHWRITE;
				if( stage->glState & GLSTATE_DEPTHWRITE )
					shader->flags |= SHADER_DEPTHWRITE;

				switch( stage->rgbGen.type )
				{
				case RGBGEN_IDENTITY_LIGHTING:
				case RGBGEN_IDENTITY:
				case RGBGEN_CONST:
				case RGBGEN_WAVE:
				case RGBGEN_COLORWAVE:
				case RGBGEN_ENTITY:
				case RGBGEN_ONE_MINUS_ENTITY:
				case RGBGEN_LIGHTING_DIFFUSE_ONLY:
				case RGBGEN_LIGHTING_AMBIENT_ONLY:
				case RGBGEN_CUSTOM:
				case RGBGEN_OUTLINE:
				case RGBGEN_UNKNOWN:   // assume RGBGEN_IDENTITY or RGBGEN_IDENTITY_LIGHTING
					switch( stage->alphaGen.type )
					{
					case ALPHAGEN_UNKNOWN:
					case ALPHAGEN_IDENTITY:
					case ALPHAGEN_CONST:
					case ALPHAGEN_WAVE:
					case ALPHAGEN_ALPHAWAVE:
					case ALPHAGEN_ENTITY:
					case ALPHAGEN_OUTLINE:
						stage->flags |= SHADERSTAGE_NOCOLORARRAY;
						break;
					default:	break;
					}
					break;
				default:	break;
				}

				if(( shader->flags & SHADER_SKYPARMS ) && ( shader->flags & SHADER_DEPTHWRITE ))
				{
					if( stage->glState & GLSTATE_DEPTHWRITE )
						stage->glState &= ~GLSTATE_DEPTHWRITE;
				}
				shader->num_stages++;
				continue;
			}

			// parse the command
			if( !Shader_ParseCommand( shader, script, tok.string ))
				return false;
		}
		return true;
	}
	else
	{
		MsgDev( D_WARN, "expected '{', found '%s' instead in shader '%s'\n", shader->name );
		return false;
	}
	return false;
}

static void Shader_SetRenderMode( ref_shader_t *s )
{
	int		i;
	ref_stage_t	*pass;

	for( i = 0, pass = s->stages; i < s->num_stages; i++, pass++ )
	{
		pass->prev.glState = pass->glState;
		pass->prev.flags = pass->flags;
		pass->prev.rgbGen = pass->rgbGen;
		pass->prev.alphaGen = pass->alphaGen;
	}
	s->realsort = s->sort;
}

static void Shader_SetFeatures( ref_shader_t *s )
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
		if( pass->program && ( pass->program_type == PROGRAM_TYPE_MATERIAL || pass->program_type == PROGRAM_TYPE_DISTORTION ))
			s->features |= MF_NORMALS|MF_SVECTORS|MF_LMCOORDS|MF_ENABLENORMALS;

		if( pass->flags & SHADERSTAGE_RENDERMODE )
			s->features |= MF_COLORS;

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
			Mem_Copy( s->deforms, r_currentDeforms, s->numDeforms * sizeof( deform_t ));
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
		if( pass->rgbGen.type == RGBGEN_WAVE || pass->rgbGen.type == RGBGEN_COLORWAVE ||
		    pass->rgbGen.type == RGBGEN_CONST || pass->rgbGen.type == RGBGEN_CUSTOM )
			size += sizeof( float ) * 3;

		// alphagen args
		if( pass->alphaGen.type == ALPHAGEN_PORTAL || pass->alphaGen.type == ALPHAGEN_SPECULAR ||
		    pass->alphaGen.type == ALPHAGEN_CONST || pass->alphaGen.type == ALPHAGEN_DOT ||
		    pass->alphaGen.type == ALPHAGEN_ONE_MINUS_DOT )
			size += sizeof( float ) * 2;

		if( pass->rgbGen.type == RGBGEN_WAVE || pass->rgbGen.type == RGBGEN_COLORWAVE )
			size += sizeof( waveFunc_t );
		if( pass->alphaGen.type == ALPHAGEN_WAVE || pass->alphaGen.type == ALPHAGEN_ALPHAWAVE )
			size += sizeof( waveFunc_t );
		size += pass->numtcMods * sizeof( tcMod_t );
		if( pass->tcgen == TCGEN_VECTOR )
			size += sizeof( vec4_t ) * 2;
	}

	buffer = Shader_Malloc( size );

	s->name = (char *)buffer; buffer += strlen( oldname ) + 1;
	s->stages = ( ref_stage_t * )buffer; buffer += s->num_stages * sizeof( ref_stage_t );

	com.strcpy( s->name, oldname );
	Mem_Copy( s->stages, r_currentPasses, s->num_stages * sizeof( ref_stage_t ));

	for( i = 0, pass = s->stages; i < s->num_stages; i++, pass++ )
	{
		if( pass->rgbGen.type == RGBGEN_WAVE || pass->rgbGen.type == RGBGEN_COLORWAVE ||
			pass->rgbGen.type == RGBGEN_CONST || pass->rgbGen.type == RGBGEN_CUSTOM )
		{
			pass->rgbGen.args = ( float * )buffer; buffer += sizeof( float ) * 3;
			Mem_Copy( pass->rgbGen.args, r_currentPasses[i].rgbGen.args, sizeof( float ) * 3 );
		}

		if( pass->alphaGen.type == ALPHAGEN_PORTAL || pass->alphaGen.type == ALPHAGEN_SPECULAR ||
		    pass->alphaGen.type == ALPHAGEN_CONST || pass->alphaGen.type == ALPHAGEN_DOT || pass->alphaGen.type == ALPHAGEN_ONE_MINUS_DOT )
		{
			pass->alphaGen.args = ( float * )buffer; buffer += sizeof( float ) * 2;
			Mem_Copy( pass->alphaGen.args, r_currentPasses[i].alphaGen.args, sizeof( float ) * 2 );
		}

		if( pass->rgbGen.type == RGBGEN_WAVE || pass->rgbGen.type == RGBGEN_COLORWAVE )
		{
			pass->rgbGen.func = ( waveFunc_t * )buffer; buffer += sizeof( waveFunc_t );
			Mem_Copy( pass->rgbGen.func, r_currentPasses[i].rgbGen.func, sizeof( waveFunc_t ));
		}
		else
		{
			pass->rgbGen.func = NULL;
		}

		if( pass->alphaGen.type == ALPHAGEN_WAVE  || pass->alphaGen.type == ALPHAGEN_ALPHAWAVE )
		{
			pass->alphaGen.func = ( waveFunc_t * )buffer; buffer += sizeof( waveFunc_t );
			Mem_Copy( pass->alphaGen.func, r_currentPasses[i].alphaGen.func, sizeof( waveFunc_t ));
		}
		else
		{
			pass->alphaGen.func = NULL;
		}

		if( pass->numtcMods )
		{
			pass->tcMods = ( tcMod_t * )buffer; buffer += r_currentPasses[i].numtcMods * sizeof( tcMod_t );
			pass->numtcMods = r_currentPasses[i].numtcMods;
			Mem_Copy( pass->tcMods, r_currentPasses[i].tcMods, r_currentPasses[i].numtcMods * sizeof( tcMod_t ));
		}

		if( pass->tcgen == TCGEN_VECTOR )
		{
			pass->tcgenVec = ( vec_t * )buffer; buffer += sizeof( vec4_t ) * 2;
			Vector4Copy( &r_currentPasses[i].tcgenVec[0], &pass->tcgenVec[0] );
			Vector4Copy( &r_currentPasses[i].tcgenVec[4], &pass->tcgenVec[4] );
		}

		if( pass->tcgen == TCGEN_WARP && s->tessSize == 0.0f )
		{
			MsgDev( D_WARN, "shader '%s' has pass with 'tcGen warp' without specified 'tessSize'\n", s->name );
			pass->tcgen = TCGEN_BASE;
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
		if( pass->flags & SHADERSTAGE_ANIMFREQUENCY && pass->anim_offset == 0 )
		{
			pass->anim_offset = pass->num_textures;	// alt-anim is missing
			pass->animFrequency[1] = 0.0f;
		}
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
		int	opaque = -1;

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
				else pass->alphaGen.type = ALPHAGEN_IDENTITY;
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
		ref_stage_t	*sp;

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
				else sp->alphaGen.type = ALPHAGEN_IDENTITY;
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

	Shader_SetRenderMode( s );
	Shader_SetFeatures( s );

 	// refresh registration sequence
 	Shader_TouchImages( s, false );
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
	int	dv;

	if( !shader ) return;
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

static ref_shader_t *Shader_CreateDefault( ref_shader_t *shader, int type, int addFlags, const char *shortname, int length )
{
	ref_stage_t	*pass;
	texture_t		*materialImages[MAX_STAGE_TEXTURES];
	script_t		*script;
	char		*skyParms;
	uint		i, hashKey;

	// make a default shader
	switch( type )
	{
	case SHADER_VERTEX:
		shader->type = SHADER_VERTEX;
		shader->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT|SHADER_NO_MODULATIVE_DLIGHTS;
		shader->features = MF_STCOORDS|MF_COLORS;
		shader->sort = SORT_OPAQUE;
		shader->num_stages = 3;
		shader->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * shader->num_stages );
		strcpy( shader->name, shortname );
		shader->stages = ( ref_stage_t * )( ( byte * )shader->name + length + 1 );
		pass = &shader->stages[0];
		pass->textures[0] = tr.whiteTexture;
		pass->flags = SHADERSTAGE_BLEND_MODULATE;
		pass->glState = GLSTATE_DEPTHWRITE/*|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ZERO*/;
		pass->tcgen = TCGEN_BASE;
		pass->rgbGen.type = RGBGEN_VERTEX;
		pass->alphaGen.type = ALPHAGEN_IDENTITY;
		pass->num_textures++;
		pass = &shader->stages[1];
		pass->flags = SHADERSTAGE_DLIGHT|SHADERSTAGE_BLEND_ADD;
		pass->glState = GLSTATE_DEPTHFUNC_EQ|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
		pass->tcgen = TCGEN_BASE;
		pass = &shader->stages[2];
		pass->flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_MODULATE;
		pass->glState = GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_SRC_COLOR;
		pass->tcgen = TCGEN_BASE;
		pass->textures[0] = Shader_FindImage( shader, shortname, addFlags );
		pass->rgbGen.type = RGBGEN_IDENTITY;
		pass->alphaGen.type = ALPHAGEN_IDENTITY;
		pass->num_textures++;
		break;
	case SHADER_FLARE:
		shader->type = SHADER_FLARE;
		shader->features = MF_STCOORDS|MF_COLORS;
		shader->sort = SORT_ADDITIVE;
		shader->num_stages = 1;
		shader->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * shader->num_stages );
		strcpy( shader->name, shortname );
		shader->stages = ( ref_stage_t * )(( byte * )shader->name + length + 1 );
		pass = &shader->stages[0];
		pass->flags = SHADERSTAGE_BLEND_ADD;
		pass->glState = GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
		pass->textures[0] = Shader_FindImage( shader, shortname, addFlags );
		pass->rgbGen.type = RGBGEN_VERTEX;
		pass->alphaGen.type = ALPHAGEN_IDENTITY;
		pass->tcgen = TCGEN_BASE;
		pass->num_textures++;
		break;
	case SHADER_ALIAS:
		shader->type = SHADER_ALIAS;
		shader->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT;
		shader->features = MF_STCOORDS|MF_NORMALS;
		shader->num_stages = 1;
		shader->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * shader->num_stages );
		strcpy( shader->name, shortname );
		shader->stages = ( ref_stage_t * )(( byte * )shader->name + length + 1 );
		pass = &shader->stages[0];
		pass->tcgen = TCGEN_BASE;

		if( r_numStageTextures > 1 )
		{
			// store group frames into one stage
			pass->flags |= SHADERSTAGE_FRAMES;
			if( r_stageAnimFrequency != 0.0f )
			{
				pass->flags |= SHADERSTAGE_ANIMFREQUENCY;
				pass->animFrequency[0] = r_stageAnimFrequency;
			}

			for( i = 0; i < r_numStageTextures; i++ )
			{
				if( !r_stageTexture[i] ) pass->textures[i] = tr.defaultTexture;
				else pass->textures[i] = r_stageTexture[i];
				pass->num_textures++;
			}
			
		}
		else
		{
			// single frame
			pass->textures[0] = r_stageTexture[0];
			pass->num_textures++;

			if( !pass->textures[0] )
			{
				MsgDev( D_WARN, "couldn't find alias skin for shader '%s', using default...\n", shader->name );
				pass->textures[0] = tr.defaultTexture;
			}
		}

		// NOTE: all alias models allow to change their rendermodes but using kRenderNormal as default  		
		pass->flags |= SHADERSTAGE_RENDERMODE;
		pass->glState = GLSTATE_DEPTHWRITE;
		pass->rgbGen.type = RGBGEN_LIGHTING_DIFFUSE;
		pass->alphaGen.type = ALPHAGEN_IDENTITY;
		shader->sort = SORT_OPAQUE;
		break;
	case SHADER_STUDIO:
		shader->type = SHADER_STUDIO;
		shader->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT|SHADER_RENDERMODE;
		shader->features = MF_STCOORDS|MF_NORMALS;
		shader->num_stages = 1;
		shader->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * shader->num_stages );
		strcpy( shader->name, shortname );
		shader->stages = ( ref_stage_t * )(( byte * )shader->name + length + 1 );
		pass = &shader->stages[0];
		pass->tcgen = TCGEN_BASE;

		switch( r_shaderRenderMode )
		{
		case kRenderTransTexture:
			// normal transparency
			pass->flags |= SHADERSTAGE_BLEND_MODULATE;
			pass->glState = GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA|GLSTATE_DEPTHWRITE;
			pass->rgbGen.type = RGBGEN_LIGHTING_AMBIENT_ONLY;
			pass->alphaGen.type = ALPHAGEN_ENTITY;
	         		shader->sort = SORT_ADDITIVE;
			break;
		case kRenderTransAdd:
			pass->flags |= SHADERSTAGE_BLEND_ADD;
			pass->glState = GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA|GLSTATE_DSTBLEND_ONE|GLSTATE_DEPTHWRITE;
			pass->alphaGen.type = ALPHAGEN_ENTITY;
			pass->rgbGen.type = RGBGEN_ENTITY;
			shader->sort = SORT_ADDITIVE;
			break;
		case kRenderTransAlpha:
			pass->flags |= SHADERSTAGE_BLEND_DECAL;
			pass->glState = GLSTATE_AFUNC_GE128|GLSTATE_DEPTHWRITE;
			pass->rgbGen.type = RGBGEN_LIGHTING_AMBIENT_ONLY;
			pass->alphaGen.type = ALPHAGEN_ENTITY;
			shader->sort = SORT_ALPHATEST;
			break;
		default:
			pass->flags |= SHADERSTAGE_RENDERMODE; // any studio model can overrided himself rendermode
			pass->glState = GLSTATE_DEPTHWRITE;
			pass->rgbGen.type = RGBGEN_LIGHTING_DIFFUSE;
			pass->alphaGen.type = ALPHAGEN_IDENTITY;
			shader->sort = SORT_OPAQUE;
			break;
		}

		if( MOD_ALLOWBUMP() && r_numStageTextures == 4 )
		{
			// material
			shader->flags &= ~SHADER_RENDERMODE;
			pass->flags &= ~SHADERSTAGE_RENDERMODE;
			pass->rgbGen.type = RGBGEN_IDENTITY;
			pass->program = DEFAULT_GLSL_PROGRAM;
			pass->program_type = PROGRAM_TYPE_MATERIAL;
			pass->textures[0] = r_stageTexture[0]; // diffusemap
			pass->num_textures++;
			pass->textures[1] = r_stageTexture[1]; // normalmap
			pass->num_textures++;
			pass->textures[2] = (r_lighting_specular->integer) ? r_stageTexture[2] : NULL; // glossmap
			pass->num_textures++;
			pass->textures[3] = r_stageTexture[3]; // decalmap
			pass->num_textures++;
			shader->features |= MF_SVECTORS|MF_ENABLENORMALS;
			shader->flags |= SHADER_MATERIAL;
		}
		else
		{
			pass->textures[0] = r_stageTexture[0];
			pass->num_textures++;

			if( !pass->textures[0] )
			{
				MsgDev( D_WARN, "couldn't find studio skin for shader '%s', using default...\n", shader->name );
				pass->textures[0] = tr.defaultTexture;
			}
		}
		break;
	case SHADER_SPRITE:
		shader->type = SHADER_SPRITE;
		shader->flags = SHADER_DEPTHWRITE|SHADER_RENDERMODE;
		shader->flags |= (r_shaderTwoSided) ? 0 : SHADER_CULL_FRONT;
		shader->features = MF_STCOORDS|MF_COLORS;
		shader->num_stages = 1;
		shader->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * shader->num_stages );
		strcpy( shader->name, shortname );
		shader->stages = (ref_stage_t *)( ( byte * )shader->name + length + 1 );
		pass = &shader->stages[0];
		pass->tcgen = TCGEN_BASE;
		if( r_stageAnimFrequency == 0.0f && r_numStageTextures == 8 )
		{
			// store angled map into one bundle
			pass->flags |= SHADERSTAGE_ANGLEDMAP;

			for( i = 0; i < 8; i++ )
			{
				if( !r_stageTexture[i] ) pass->textures[i] = tr.defaultTexture;
				else pass->textures[i] = r_stageTexture[i];
				pass->num_textures++;
			}
		}
		else if( r_numStageTextures > 1 )
		{
			// store group frames into one stage
			pass->flags |= SHADERSTAGE_FRAMES;
			if( r_stageAnimFrequency != 0.0f )
			{
				pass->flags |= SHADERSTAGE_ANIMFREQUENCY;
				pass->animFrequency[0] = r_stageAnimFrequency;
			}
			for( i = 0; i < r_numStageTextures; i++ )
			{
				if( !r_stageTexture[i] ) pass->textures[i] = tr.defaultTexture;
				else pass->textures[i] = r_stageTexture[i];
				pass->num_textures++;
			}
			
		}
		else
		{
			// single frame
			pass->textures[0] = r_stageTexture[0];
			pass->num_textures++;

			if( !pass->textures[0] )
			{
				MsgDev( D_WARN, "couldn't find spriteframe for shader '%s', using default...\n", shader->name );
				pass->textures[0] = tr.defaultTexture;
			}
		}

 		pass->flags |= SHADERSTAGE_RENDERMODE; // any sprite can overrided himself rendermode
                  
		switch( r_shaderRenderMode )
		{
		case kRenderTransTexture:
			// normal transparency
			pass->flags |= SHADERSTAGE_BLEND_MODULATE;
			pass->glState = GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA|GLSTATE_DEPTHWRITE;
			pass->alphaGen.type = ALPHAGEN_ENTITY;
			pass->rgbGen.type = RGBGEN_ENTITY;
	         		shader->sort = SORT_ADDITIVE;
			break;
		case kRenderTransAdd:
			pass->flags |= SHADERSTAGE_BLEND_ADD;
			pass->glState = GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA|GLSTATE_DSTBLEND_ONE|GLSTATE_DEPTHWRITE;
			pass->alphaGen.type = ALPHAGEN_ENTITY;
			pass->rgbGen.type = RGBGEN_ENTITY;
			shader->sort = SORT_ADDITIVE;
			break;
		case kRenderGlow:
			pass->flags |= SHADERSTAGE_BLEND_ADD;
			pass->glState = GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA|GLSTATE_DSTBLEND_ONE|GLSTATE_NO_DEPTH_TEST;
			pass->alphaGen.type = ALPHAGEN_ENTITY;
			pass->rgbGen.type = RGBGEN_ENTITY;
			shader->sort = SORT_ADDITIVE;
			break;
		case kRenderTransAlpha:
			pass->flags |= SHADERSTAGE_BLEND_DECAL;
			pass->glState = GLSTATE_AFUNC_GE128|GLSTATE_DEPTHWRITE;
			shader->sort = SORT_ALPHATEST;
			break;
		}
		break;
	case SHADER_FONT:
		shader->type = SHADER_FONT;
		shader->features = MF_STCOORDS|MF_COLORS;
		shader->flags = SHADER_STATIC;
		shader->sort = SORT_ADDITIVE;
		shader->num_stages = 1;
		shader->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * shader->num_stages );
		strcpy( shader->name, shortname );
		shader->stages = ( ref_stage_t * )(( byte * )shader->name + length + 1 );
		pass = &shader->stages[0];
		pass->flags = SHADERSTAGE_BLEND_MODULATE;
		pass->glState = GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		pass->textures[0] = R_FindTexture( shortname, NULL, 0, addFlags|TF_CLAMP|TF_NOMIPMAP|TF_NOPICMIP );
		if( !pass->textures[0] )
		{
			// don't let user set invalid font
			MsgDev( D_WARN, "couldn't find texture for shader '%s', using default...\n", shader->name );
			pass->textures[0] = tr.defaultConchars;
		}
		pass->rgbGen.type = RGBGEN_VERTEX;
		pass->alphaGen.type = ALPHAGEN_VERTEX;
		pass->tcgen = TCGEN_BASE;
		pass->num_textures++;
		break;
	case SHADER_NOMIP:
		shader->type = SHADER_NOMIP;
		shader->features = MF_STCOORDS|MF_COLORS;
		shader->flags = SHADER_STATIC;
		shader->sort = SORT_ADDITIVE;
		shader->num_stages = 1;
		shader->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * shader->num_stages );
		strcpy( shader->name, shortname );
		shader->stages = ( ref_stage_t * )( ( byte * )shader->name + length + 1 );
		pass = &shader->stages[0];
		pass->flags = SHADERSTAGE_BLEND_MODULATE/*|SHADERSTAGE_NOCOLORARRAY*/;
		pass->glState = GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA; 
		if( shader->name[0] == '#' )
		{
			// search for internal resource
			size_t	bufsize = 0;
			byte	*buffer = FS_LoadInternal( shader->name + 1, &bufsize );
			pass->textures[0] = R_FindTexture( shader->name, buffer, bufsize, addFlags|TF_NOPICMIP|TF_CLAMP|TF_NOMIPMAP );
		}
		else pass->textures[0] = Shader_FindImage( shader, shortname, addFlags|TF_NOPICMIP|TF_CLAMP|TF_NOMIPMAP );
		if( !pass->textures[0] )
		{
			MsgDev( D_WARN, "couldn't find texture for shader '%s', using default...\n", shader->name );
			pass->textures[0] = tr.defaultTexture;
		}
		pass->rgbGen.type = RGBGEN_IDENTITY;
		pass->alphaGen.type = ALPHAGEN_IDENTITY;
		pass->tcgen = TCGEN_BASE;
		pass->num_textures++;
		break;
	case SHADER_SKY:
		shader->type = SHADER_SKY;
		shader->name = Shader_Malloc( length + 1 );
		strcpy( shader->name, shortname );
		// create simple sky parms, to do Shader_SkyParms parsing it properly
		skyParms = va( "%s - -", shortname );
		script = Com_OpenScript( "skybox", skyParms, com.strlen( skyParms ));
		Shader_SkyParms( shader, NULL, script );
		Com_Assert( shader->skyParms == NULL );
		Com_CloseScript( script );
		break;
	case SHADER_FARBOX:
		shader->type = SHADER_FARBOX;
		shader->features = MF_STCOORDS;
		shader->sort = SORT_SKY;
		shader->flags = SHADER_SKYPARMS;
		shader->num_stages = 1;
		shader->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * shader->num_stages );
		strcpy( shader->name, shortname );
		shader->stages = ( ref_stage_t * )( ( byte * )shader->name + length + 1 );
		pass = &shader->stages[0];
		pass->flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_MODULATE; 
//		pass->glState = GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ZERO;
		pass->textures[0] = R_FindTexture( shortname, NULL, 0, addFlags|TF_CLAMP|TF_NOMIPMAP );
		pass->rgbGen.type = RGBGEN_IDENTITY_LIGHTING;
		pass->alphaGen.type = ALPHAGEN_IDENTITY;
		pass->tcgen = TCGEN_BASE;
		pass->num_textures++;
		break;
	case SHADER_NEARBOX:
		shader->type = SHADER_NEARBOX;
		shader->features = MF_STCOORDS;
		shader->sort = SORT_SKY;
		shader->num_stages = 1;
		shader->flags = SHADER_SKYPARMS;
		shader->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * shader->num_stages );
		strcpy( shader->name, shortname );
		shader->stages = ( ref_stage_t * )( ( byte * )shader->name + length + 1 );
		pass = &shader->stages[0];
		pass->flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_DECAL;
		pass->glState = GLSTATE_ALPHAFUNC|GLSTATE_SRCBLEND_DST_COLOR|GLSTATE_DSTBLEND_SRC_COLOR;
		pass->textures[0] = R_FindTexture( shortname, NULL, 0, addFlags|TF_CLAMP|TF_NOMIPMAP );
		pass->rgbGen.type = RGBGEN_IDENTITY_LIGHTING;
		pass->alphaGen.type = ALPHAGEN_IDENTITY;
		pass->tcgen = TCGEN_BASE;
		pass->num_textures++;
		break;
	case SHADER_PLANAR_SHADOW:
		shader->type = SHADER_PLANAR_SHADOW;
		shader->features = MF_DEFORMVS;
		shader->sort = SORT_DECAL;
		shader->flags = SHADER_STATIC;
		shader->numDeforms = 1;
		shader->num_stages = 1;
		shader->name = Shader_Malloc( length + 1 + shader->numDeforms * sizeof( deform_t ) + sizeof( ref_stage_t ) * shader->num_stages );
		strcpy( shader->name, shortname );
		shader->deforms = ( deform_t * )( ( byte * )shader->name + length + 1 );
		shader->deforms[0].type = DEFORM_PROJECTION_SHADOW;
		shader->stages = ( ref_stage_t * )((byte *)shader->deforms + shader->numDeforms * sizeof( deform_t ) );
		pass = &shader->stages[0];
		pass->flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_STENCILSHADOW|SHADERSTAGE_BLEND_DECAL;
		pass->glState = GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		pass->rgbGen.type = RGBGEN_IDENTITY;
		pass->alphaGen.type = ALPHAGEN_IDENTITY;
		pass->tcgen = TCGEN_NONE;
		break;
	case SHADER_OPAQUE_OCCLUDER:
		shader->type = SHADER_OPAQUE_OCCLUDER;
		shader->sort = SORT_OPAQUE;
		shader->flags = SHADER_CULL_FRONT|SHADER_DEPTHWRITE|SHADER_STATIC;
		shader->num_stages = 1;
		shader->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * shader->num_stages + 3 * sizeof( float ) );
		strcpy( shader->name, shortname );
		shader->stages = ( ref_stage_t * )( ( byte * )shader->name + length + 1 );
		pass = &shader->stages[0];
		pass->textures[0] = tr.whiteTexture;
		pass->flags = SHADERSTAGE_NOCOLORARRAY;
		pass->glState = GLSTATE_DEPTHWRITE;
		pass->rgbGen.type = RGBGEN_ENVIRONMENT;
		pass->rgbGen.args = ( float * )( ( byte * )shader->stages + sizeof( ref_stage_t ) * shader->num_stages );
		VectorClear( pass->rgbGen.args );
		pass->alphaGen.type = ALPHAGEN_IDENTITY;
		pass->tcgen = TCGEN_NONE;
		pass->num_textures++;
		break;
	case SHADER_OUTLINE:
		shader->type = SHADER_OUTLINE;
		shader->features = MF_NORMALS|MF_DEFORMVS;
		shader->sort = SORT_OPAQUE;
		shader->flags = SHADER_CULL_BACK|SHADER_DEPTHWRITE|SHADER_STATIC;
		shader->numDeforms = 1;
		shader->num_stages = 1;
		shader->name = Shader_Malloc( length + 1 + shader->numDeforms * sizeof( deform_t ) + sizeof( ref_stage_t ) * shader->num_stages );
		strcpy( shader->name, shortname );
		shader->deforms = ( deform_t * )( ( byte * )shader->name + length + 1 );
		shader->deforms[0].type = DEFORM_OUTLINE;
		shader->stages = ( ref_stage_t * )( ( byte * )shader->deforms + shader->numDeforms * sizeof( deform_t ) );
		pass = &shader->stages[0];
		pass->textures[0] = tr.whiteTexture;
		pass->flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_MODULATE;
		pass->glState = GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ZERO|GLSTATE_DEPTHWRITE;
		pass->rgbGen.type = RGBGEN_OUTLINE;
		pass->alphaGen.type = ALPHAGEN_OUTLINE;
		pass->tcgen = TCGEN_NONE;
		pass->num_textures++;
		break;
	case SHADER_TEXTURE:
		if( mapConfig.deluxeMappingEnabled && Shaderpass_LoadMaterial( &materialImages[0], &materialImages[1], &materialImages[2], shortname, addFlags, 1 ))
		{
			shader->type = SHADER_TEXTURE;
			shader->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT|SHADER_NO_MODULATIVE_DLIGHTS|SHADER_HASLIGHTMAP|SHADER_MATERIAL;
			shader->features = MF_STCOORDS|MF_LMCOORDS|MF_NORMALS|MF_SVECTORS|MF_ENABLENORMALS;
			shader->sort = SORT_OPAQUE;
			shader->num_stages = 1;
			shader->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * shader->num_stages );
			strcpy( shader->name, shortname );
			shader->stages = (ref_stage_t *)(( byte * )shader->name + length + 1 );
			pass = &shader->stages[0];
			pass->flags = SHADERSTAGE_LIGHTMAP|SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_REPLACE;
			pass->glState = GLSTATE_DEPTHWRITE;
			pass->tcgen = TCGEN_BASE;
			pass->rgbGen.type = RGBGEN_IDENTITY;
			pass->alphaGen.type = ALPHAGEN_IDENTITY;
			pass->program = DEFAULT_GLSL_PROGRAM;
			pass->program_type = PROGRAM_TYPE_MATERIAL;
			pass->textures[0] = Shader_FindImage( shader, shortname, addFlags );
			pass->num_textures++;
			pass->textures[1] = materialImages[0];	// normalmap
			pass->num_textures++;
			pass->textures[2] = materialImages[1];	// glossmap
			pass->num_textures++;
			pass->textures[3] = materialImages[2];	// decalmap
			pass->num_textures++;
		}
		else
		{
			shader->type = SHADER_TEXTURE;
			shader->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT|SHADER_NO_MODULATIVE_DLIGHTS|SHADER_HASLIGHTMAP;
			shader->features = MF_STCOORDS|MF_LMCOORDS;
			shader->sort = SORT_OPAQUE;
			shader->num_stages = 3;
			shader->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * shader->num_stages );
			strcpy( shader->name, shortname );
			shader->stages = ( ref_stage_t * )( ( byte * )shader->name + length + 1 );
			pass = &shader->stages[0];
			pass->flags = SHADERSTAGE_LIGHTMAP|SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_REPLACE;
			pass->glState = GLSTATE_DEPTHWRITE;
			pass->tcgen = TCGEN_LIGHTMAP;
			pass->rgbGen.type = RGBGEN_IDENTITY;
			pass->alphaGen.type = ALPHAGEN_IDENTITY;
			pass = &shader->stages[1];
			pass->flags = SHADERSTAGE_DLIGHT|SHADERSTAGE_BLEND_ADD;
			pass->glState = GLSTATE_DEPTHFUNC_EQ|GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE;
			pass->tcgen = TCGEN_BASE;
			pass = &shader->stages[2];
			pass->flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_MODULATE;
			pass->glState = GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_SRC_COLOR;
			pass->tcgen = TCGEN_BASE;
			pass->textures[0] = Shader_FindImage( shader, shortname, addFlags );
			pass->rgbGen.type = RGBGEN_IDENTITY;
			pass->alphaGen.type = ALPHAGEN_IDENTITY;
			pass->num_textures++;
		}
		break;
	case SHADER_GENERIC:
	default:	shader->type = SHADER_GENERIC;
		shader->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT|SHADER_NO_MODULATIVE_DLIGHTS;
		shader->features = MF_STCOORDS;
		shader->sort = SORT_OPAQUE;
		shader->num_stages = 1;
		shader->name = Shader_Malloc( length + 1 + sizeof( ref_stage_t ) * shader->num_stages );
		strcpy( shader->name, shortname );
		shader->stages = ( ref_stage_t * )(( byte * )shader->name + length + 1 );
		pass = &shader->stages[0];
		pass->flags = SHADERSTAGE_NOCOLORARRAY|SHADERSTAGE_BLEND_MODULATE;
		pass->glState = GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_SRC_COLOR;
		pass->tcgen = TCGEN_BASE;
		pass->textures[0] = Shader_FindImage( shader, shortname, addFlags );
		pass->rgbGen.type = RGBGEN_IDENTITY;
		pass->alphaGen.type = ALPHAGEN_IDENTITY;
		pass->num_textures++;
		break;
	}

	// reset parms
	r_shaderTwoSided = 0;
	r_numStageTextures = 0;
	r_stageAnimFrequency = 0.0f;
	r_shaderRenderMode = kRenderNormal;

	Shader_SetRenderMode( shader );

 	// refresh registration sequence
 	Shader_TouchImages( shader, false );

	// calculate sortkey
	shader->sortkey = Shader_Sortkey( shader, shader->sort );
	shader->flags |= SHADER_DEFAULTED;

	// add to hash table
	hashKey = Com_HashKey( shortname, SHADERS_HASH_SIZE );
	shader->nextHash = r_shadersHash[hashKey];
	r_shadersHash[hashKey] = shader;

	return shader;
}

ref_shader_t *R_LoadShader( const char *name, int type, bool forceDefault, int addFlags, int ignoreType )
{
	ref_shader_t	*shader;
	string		shortname;
	ref_script_t	*cache = NULL;
	uint		i, hashKey, length;

	if( !name || !name[0] ) return NULL;

	for( i = ( name[0] == '/' || name[0] == '\\' ), length = 0; name[i] && ( length < sizeof( shortname )-1 ); i++ )
	{
		if( name[i] == '\\' ) shortname[length++] = '/';
		else shortname[length++] = com.tolower( name[i] );
	}

	if( !length ) return NULL;
	shortname[length] = 0;

	// see if already loaded
	hashKey = Com_HashKey( shortname, SHADERS_HASH_SIZE );

	for( shader = r_shadersHash[hashKey]; shader; shader = shader->nextHash )
	{
		if( shader->type == ignoreType ) continue;

		if( !com.stricmp( shader->name, shortname ))
		{
			if( shader->type != type )
			{
				if( shader->flags & SHADER_SKYPARMS )
					MsgDev( D_WARN, "reused shader '%s' with mixed types (%i should be %i)\n", shortname, shader->type, type );
				else continue;
                              }
			// prolonge registration
			Shader_TouchImages( shader, false );
			return shader;
		}
	}

	// find a free shader_t slot
	for( i = 0, shader = r_shaders; i < r_numShaders; i++, shader++ )
		if( !shader->name ) break;

	if( i == r_numShaders )
	{
		if( r_numShaders == MAX_SHADERS )
			Host_Error( "R_LoadShader: MAX_SHADERS limit exceeded\n" );
		r_numShaders++;
	}

	shader = &r_shaders[i];
	Mem_Set( shader, 0, sizeof( ref_shader_t ));
	shader->offsetmapping_scale = 1.0f;
	shader->name = shortname;
	shader->shadernum = i;

	if( ignoreType == SHADER_UNKNOWN )
		forceDefault = true;

	r_shaderNoMipMaps =	false;
	r_shaderNoPicMip = false;
	r_shaderNoCompress = false;
	r_shaderHasDlightPass = false;

	if( !forceDefault )
		cache = Shader_GetCache( shortname, type, hashKey );

	// the shader is in the shader scripts
	if( cache )
	{
		script_t	*script;
	
		MsgDev( D_NOTE, "Loading shader %s from cache...\n", name );

		// set defaults
		shader->type = type;
		shader->flags = SHADER_CULL_FRONT;
		shader->features = MF_NONE;

		shader->cache = cache;

		// load the script text
		script = Com_OpenScript( cache->name, cache->buffer, cache->size );
		if( !script ) return Shader_CreateDefault( shader, type, addFlags, shortname, length );

		if( !Shader_ParseShader( shader, script ))
		{
			Mem_Set( shader, 0, sizeof( ref_shader_t ));
			shader->offsetmapping_scale = 1.0f;
			shader->name = shortname;
			shader->shadernum = i;

			Com_CloseScript( script );
			return Shader_CreateDefault( shader, type, addFlags, shortname, length );
		}
		Com_CloseScript( script );
		Shader_Finish( shader );
	}
	else return Shader_CreateDefault( shader, type, addFlags, shortname, length );

	// calculate sortkey
	shader->sortkey = Shader_Sortkey( shader, shader->sort );

	// add to hash table
	shader->nextHash = r_shadersHash[hashKey];
	r_shadersHash[hashKey] = shader;

	return shader;
}

void R_ShaderFreeUnused( void )
{
	ref_shader_t	*shader;
	int		i;

	for( i = 0, shader = r_shaders; i < r_numShaders; i++, shader++ )
	{
		if( !shader->shadernum ) continue;
		
		// used this sequence
		if( shader->touchFrame == tr.registration_sequence ) continue;
		if( shader->flags & SHADER_STATIC ) continue;
		Shader_FreeShader( shader );
	}
}

void R_ShaderAddStageTexture( texture_t *mipTex )
{
	if( r_numStageTextures >= MAX_STAGE_TEXTURES ) return;
	r_stageTexture[r_numStageTextures++] = mipTex;
}

void R_ShaderSetRenderMode( kRenderMode_t mode, bool twoSided )
{
	r_shaderRenderMode = mode;
	r_shaderTwoSided = twoSided;
}

void R_ShaderAddStageIntervals( float interval )
{
	r_stageAnimFrequency += interval;
}

/*
=================
R_SetupSky
=================
*/
ref_shader_t *R_SetupSky( const char *name )
{
	string		loadname;
	bool		shader_valid = false;
	bool		force_default = false;
	int		index;

	if( name && name[0] )
	{
		ref_script_t	*cache;
		ref_shader_t	*shader;
		uint		hashKey;

		com.strncpy( loadname, name, sizeof( loadname ));
	
		// make sure what new shader it's a skyShader and existing
		hashKey = Com_HashKey( loadname, SHADERS_HASH_SIZE );

		for( shader = r_shadersHash[hashKey]; shader; shader = shader->nextHash )
		{
			if( !com.stricmp( shader->name, loadname ))
				break;
		}
		if( shader )
		{
			// already loaded, check parms
			if( shader->flags & SHADER_SKYPARMS && shader->skyParms )
				shader_valid = true;
		}
		else
		{
			cache = Shader_GetCache( loadname, SHADER_INVALID, hashKey );
			if( cache )
			{
				script_t	*script = Com_OpenScript( cache->name, cache->buffer, cache->size );
				token_t	tok;

				while( Com_ReadToken( script, SC_ALLOW_NEWLINES, &tok ))
				{
					if( !com.stricmp( "skyParms", tok.string ))
					{
						// check only far skybox images for existing
						// because near skybox without far will be ignored by engine
						 if( Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
						 {
							if( com.stricmp( "-", tok.string ))
							{
								if( Shader_CheckSkybox( tok.string ))
								{
									shader_valid = true;
									break;
								}
							}
							else
							{
								shader_valid = true;
								break; // new shader just reset skybox
						 	}
						 }
					}
					else if( !com.stricmp( "surfaceParm", tok.string ))
					{
						// check only far skybox images for existing
						// because near skybox without far will be ignored by engine
						if( Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &tok ))
						{
							if( !com.stricmp( "sky", tok.string ))
							{
								shader_valid = true;
								break; // yes it's q3-style skyshader
							}
						}
					}
				}
				Com_CloseScript( script );          
			}
			else
			{
				if( !Shader_CheckSkybox( loadname ))
				{
					com.strncpy( loadname, va( "env/%s", name ), sizeof( loadname ));
					if( Shader_CheckSkybox( loadname ))
							shader_valid = true;
					else shader_valid = true;
				}
				else shader_valid = true;
				force_default = true;
			}
		}
	}
	else
	{
		// NULL names force to get current sky shader by user requesting
		return (tr.currentSkyShader) ? tr.currentSkyShader : tr.defaultShader;
	}

	if( !shader_valid )
	{
		MsgDev( D_ERROR, "R_SetupSky: 'couldn't find shader '%s'\n", name );  
		return (tr.currentSkyShader) ? tr.currentSkyShader : tr.defaultShader;				
	}

	if( tr.currentSkyShader == NULL )
	{
		if( !r_worldmodel ) MsgDev( D_ERROR, "R_SetupSky: map not loaded\n" );
		else MsgDev( D_ERROR, "R_SetupSky: map %s not contain sky surfaces\n", r_worldmodel->name );  
		return (tr.currentSkyShader) ? tr.currentSkyShader : tr.defaultShader;
	}

	index = tr.currentSkyShader->shadernum;
	Shader_FreeShader( tr.currentSkyShader ); // release old sky

	// new sky shader
	tr.currentSkyShader = R_LoadShader( loadname, SHADER_SKY, force_default, 0, SHADER_INVALID );
	if( index != tr.currentSkyShader->shadernum )
		MsgDev( D_ERROR, "R_SetupSky: mismatch shader indexes %i != %i\n", index, tr.currentSkyShader->shadernum );

	return tr.currentSkyShader;
}

void R_FreeSky( void )
{
	if( tr.currentSkyShader == NULL ) return;	// already freed

	Shader_FreeShader( tr.currentSkyShader );
	tr.currentSkyShader = NULL;
}